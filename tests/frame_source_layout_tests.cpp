#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QDockWidget>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
const std::string raw_text="  Source\r\n日本語 🐈\xe2\x80\xa8line\xe2\x80\xa9tail\xc2\xa0\n";
Document fixture(bool assigned_text=false){
    auto d=empty_document("template-navigation","composition","source");auto& c=d.compositions.front();
    c.artboards.front().name="Shared frame";c.artboards.front().width=1200;c.artboards.front().height=800;
    c.artboards.front().layout=ArtboardLayout{Margin{10,20,30,40},Grid{"source-grid",{30,20,100,80},2,2,5,5}};
    c.artboards.push_back({"decoy","Shared frame",1500,0,1200,800});
    c.artboards.push_back({"target","Target",3000,0,640,480});
    Object root;root.id="root";root.name="Source content";root.kind=Kind::group;root.children={"text"};root.visible=false;
    Object text;text.id="text";text.name="Retained text";text.kind=Kind::text;text.text=default_text("text-source","Retained text");
    d.objects.emplace(root.id,root);d.objects.emplace(text.id,text);c.roots={root.id};
    if(assigned_text){
        auto driver=text;driver.id="driver";driver.text=default_text("driver-source",raw_text);driver.visible=false;
        auto sibling=text;sibling.id="sibling";sibling.text=default_text("sibling-source","Sibling retained");
        d.objects.at("text").text->content_driver=TextContentDriver{{"driver","","text.content"}};
        d.objects.at("root").children={"text","driver","sibling"};
        d.objects.emplace(driver.id,driver);d.objects.emplace(sibling.id,sibling);
    }
    Session s(d);s.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",Id{"content"}}},
        ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","frame.width",640.0}},
        ArtboardGuideCommand{AddArtboardGuide{"composition","source",{"guide","Source guide","x",20,true}}},
        ArtboardGuideCommand{SetArtboardGuideOverride{"composition","target","guide","position",45.0}}},0);
    if(assigned_text)s.apply({DefinitionCommand{CreateInstance{"composition","","other-content","definition","Other occurrence"}},
        DefinitionCommand{SetInstanceTextContentOverride{"content","sibling","Local sibling"}}},s.revision());
    return s.document();
}
const Artboard& board(const Document& d,const Id& id){for(const auto& b:d.compositions.front().artboards)if(b.id==id)return b;throw std::runtime_error("Missing board");}
struct Snapshot {
    Document document,preview;std::string native;std::uint64_t revision,generation;HistoryInfo history;bool gesture;
    explicit Snapshot(const Session& s):document(s.document()),preview(s.preview_document()),native(encode(document)),revision(s.revision()),generation(s.gesture_generation()),history(s.history()),gesture(s.gesture_active()){}
    bool unchanged(const Session& s)const{return document==s.document()&&preview==s.preview_document()&&native==encode(s.document())&&revision==s.revision()&&generation==s.gesture_generation()&&history==s.history()&&gesture==s.gesture_active();}
};
template<class T>T* visible(Window& w,const char* name){for(auto* p:w.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(std::string("Missing visible ")+name);}
void reveal(Window& w,QWidget* input){
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");
    area->verticalScrollBar()->setValue(input->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    check(area->horizontalScrollBar()->value()==0,"Reachability never uses horizontal scroll");
    check(area->viewport()->rect().contains(QRect(input->mapTo(area->viewport(),QPoint()),input->size())),"Control rectangle fits the actual viewport");
    check(input->visibleRegion().contains(input->rect()),"Existing control is fully visible at the default pane width");
}
void history(Window& w,const QString& label){for(auto* a:w.findChildren<QAction*>())if(a->text()==label){a->trigger();events();return;}throw std::runtime_error("Missing history action");}
void canonical(Window& w,const Snapshot& before,const std::vector<Command>& commands){
    Session expected(before.document);expected.apply(commands,0);auto& s=w.host.session;
    check(s.document()==expected.document()&&encode(s.document())==encode(expected.document()),"Actual action equals complete canonical Document and native source");
    check(s.revision()==before.revision+1&&s.history().states.size()==before.history.states.size()+1&&!s.gesture_active(),"Exactly one retained transaction, no preview");
    history(w,"Undo");check(s.document()==before.document&&encode(s.document())==before.native,"Undo restores all source, layout, Template, content and other boards");
    history(w,"Redo");check(s.document()==expected.document(),"Redo restores full canonical source transaction");
}
void text_canonical(Window& w,const Session& before,const Command& command){
    Session expected=before;expected.apply({command},expected.revision());auto& s=w.host.session;
    const auto matches=[&]{return s.document()==expected.document()&&s.preview_document()==expected.preview_document()&&
        encode(s.document())==encode(expected.document())&&s.revision()==expected.revision()&&s.history()==expected.history()&&
        s.gesture_generation()==expected.gesture_generation()&&s.gesture_active()==expected.gesture_active();};
    check(matches(),"Actual Text action equals full canonical Document/native/history/revision/preview/generation");
    history(w,"Undo");expected.undo(expected.revision());check(matches(),"Text Undo equals complete canonical state and existing history");
    history(w,"Redo");expected.redo(expected.revision());check(matches(),"Text Redo equals complete canonical state and existing history");
}
void select_frame(Window& w,const Id& id){
    auto* dock=w.findChild<QDockWidget*>("structure");dock->show();dock->raise();events();
    auto* list=w.findChild<QListWidget*>("artboards");QListWidgetItem* row=nullptr;
    for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()==QString::fromStdString(id))row=list->item(i);
    check(row,"Stable Artboard row exists");list->scrollToItem(row);events();
    QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(row).center());events();
    auto* edit=visible<QPushButton>(w,"artboard-edit");QTest::mouseClick(edit,Qt::LeftButton);events();
    check(w.canvas->active_artboard()==id,"Actual Artboard click chooses captured frame");
    auto* properties=w.findChild<QDockWidget*>("properties");properties->show();properties->raise();events();
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir scratch;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("FrameSourceLayout");
    try{const bool assigned_diagnostic=app.arguments().contains("--assigned-frame");
        Window w(scratch.filePath("recovery"));w.host.session=Session(fixture(assigned_diagnostic));w.host.session_id="frame-layout-session";
        w.host.edited();w.show();w.activateWindow();events();select_frame(w,"source");
        if(assigned_diagnostic)select_frame(w,"target");
        auto* area=w.findChild<QScrollArea*>("inspector-scroll");auto* viewport=area->viewport();
        std::cout<<"Window="<<w.width()<<" viewport="<<viewport->width()<<" content="<<area->widget()->width()<<" minimum="<<area->widget()->minimumSizeHint().width()<<" horizontal max="<<area->horizontalScrollBar()->maximum()<<std::endl;
        for(auto* child:area->widget()->findChildren<QWidget*>())if(child->minimumSizeHint().width()>viewport->width()-40)
            std::cout<<child->metaObject()->className()<<" "<<child->objectName().toStdString()<<" width="<<child->width()<<" minHint="<<child->minimumSizeHint().width()<<" hint="<<child->sizeHint().width()<<std::endl;
        auto* cancel=visible<QPushButton>(w,"artboard-width-cancel");
        area->verticalScrollBar()->setValue(cancel->mapTo(area->widget(),QPoint()).y()-viewport->height()/2);events();
        const auto evidence=qEnvironmentVariable("NECT_FRAME_LAYOUT_EVIDENCE");if(!evidence.isEmpty())w.grab().save(evidence);
        check(cancel->isEnabled(),"Cancel draft is enabled");
        check(area->horizontalScrollBar()->value()==0,"Default frame starts at left edge");
        check(cancel->visibleRegion().contains(cancel->rect()),"Enabled Cancel draft fits default Properties viewport without horizontal scrolling");
        if(assigned_diagnostic){
            auto& s=w.host.session;s.apply({Rename{"sibling","Existing history"}},s.revision());w.host.edited();events();
            const auto canvas_width=w.canvas->width(),pane_width=viewport->width();
            auto* picker=visible<QComboBox>(w,"instance-text-content-source");
            check(picker->currentData().toString()=="text"&&picker->count()==3,"Exact descendant target retained despite duplicate labels");
            check(evaluate_instance_text_content(s.document(),"content","text")==raw_text,"Typed source retains original CRLF/Unicode/separator bytes");
            Session before=s;
            auto* local=visible<QPushButton>(w,"instance-text-content-apply");
            reveal(w,local);check(local->isEnabled(),"Inherited content can freeze locally");
            QTest::mouseClick(local,Qt::LeftButton);events();
            text_canonical(w,before,DefinitionCommand{SetInstanceTextContentOverride{"content","text",raw_text}});
            auto* reset=visible<QPushButton>(w,"instance-text-content-reset");
            area->verticalScrollBar()->setValue(reset->mapTo(area->widget(),QPoint()).y()-viewport->height()/2);events();
            std::cout<<"Use Source enabled="<<reset->isEnabled()<<" x="<<reset->mapTo(viewport,QPoint()).x()<<" width="<<reset->width()<<" viewport="<<viewport->width()<<std::endl;
            if(!evidence.isEmpty())w.grab().save(evidence+".text.png");
            check(reset->isEnabled(),"Actual Apply local enables Use Source");
            check(area->horizontalScrollBar()->value()==0&&viewport->rect().contains(QRect(reset->mapTo(viewport,QPoint()),reset->size()))&&reset->visibleRegion().contains(reset->rect()),"Enabled Use Source fits standard Properties viewport without horizontal scrolling");
            check(area->horizontalScrollBar()->maximum()==0&&w.canvas->width()==canvas_width&&viewport->width()==pane_width,"Text actions fit without widening pane or Canvas resize");
            const Snapshot resized(s);auto* dock=w.findChild<QDockWidget*>("properties");const auto dock_width=dock->width();
            w.resizeDocks({dock},{600},Qt::Horizontal);events();
            for(const auto* name:{"instance-text-content-apply","instance-text-content-cancel","instance-text-content-clear","instance-text-content-reset"})reveal(w,visible<QPushButton>(w,name));
            w.resizeDocks({dock},{dock_width},Qt::Horizontal);events();
            check(resized.unchanged(s)&&viewport->width()==pane_width,"User pane resize and return preserve full Text/source/history/preview");
            before=s;reset=visible<QPushButton>(w,"instance-text-content-reset");reveal(w,reset);check(reset->isEnabled(),"Use Source remains enabled after pane resize");
            reset->setFocus();QTest::keyClick(reset,Qt::Key_Space);events();
            text_canonical(w,before,DefinitionCommand{ResetInstanceTextContentOverride{"content","text"}});
            check(!s.document().objects.at("content").instance->text_content_overrides.contains("text")&&
                evaluate_instance_text_content(s.document(),"other-content","text")==raw_text,"Use Source restores inheritance and preserves other occurrence");
            const Snapshot inherited(s);auto* editor=visible<QPlainTextEdit>(w,"instance-text-content-editor");reveal(w,editor);
            const auto display=editor->toPlainText();QTest::mouseClick(editor->viewport(),Qt::LeftButton);QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"temporary draft");events();
            auto* cancel_text=visible<QPushButton>(w,"instance-text-content-cancel");reveal(w,cancel_text);check(cancel_text->isEnabled(),"Changed Text draft enables Cancel");
            QTest::mouseClick(cancel_text,Qt::LeftButton);events();
            check(inherited.unchanged(s)&&editor->toPlainText()==display,"Pointer Cancel restores exact raw baseline without authored/history/preview changes");
            auto* clear=visible<QPushButton>(w,"instance-text-content-clear");reveal(w,clear);check(clear->isEnabled(),"Inherited Text enables Clear draft");
            clear->setFocus();QTest::keyClick(clear,Qt::Key_Space);events();
            check(inherited.unchanged(s)&&editor->toPlainText().isEmpty(),"Keyboard Clear makes only an empty draft");
            cancel_text=visible<QPushButton>(w,"instance-text-content-cancel");reveal(w,cancel_text);check(cancel_text->isEnabled(),"Clear draft enables Cancel");
            cancel_text->setFocus();QTest::keyClick(cancel_text,Qt::Key_Space);events();
            check(inherited.unchanged(s)&&editor->toPlainText()==display,"Keyboard Cancel after Clear restores raw inherited baseline");
            clear=visible<QPushButton>(w,"instance-text-content-clear");reveal(w,clear);QTest::mouseClick(clear,Qt::LeftButton);events();
            before=s;local=visible<QPushButton>(w,"instance-text-content-apply");reveal(w,local);check(local->isEnabled(),"Explicit empty draft enables Apply local");
            local->setFocus();QTest::keyClick(local,Qt::Key_Space);events();
            text_canonical(w,before,DefinitionCommand{SetInstanceTextContentOverride{"content","text",""}});
            check(s.document().objects.at("content").instance->text_content_overrides.at("text").empty()&&evaluate_instance_text_content(s.document(),"content","text").empty(),"Empty local content differs from inheritance");
            before=s;reset=visible<QPushButton>(w,"instance-text-content-reset");reveal(w,reset);check(reset->isEnabled(),"Explicit empty local content enables Use Source");
            QTest::mouseClick(reset,Qt::LeftButton);events();text_canonical(w,before,DefinitionCommand{ResetInstanceTextContentOverride{"content","text"}});
            check(evaluate_instance_text_content(s.document(),"content","text")==raw_text,"Pointer Use Source restores typed raw content after explicit empty override");
            before=s;local=visible<QPushButton>(w,"instance-text-content-apply");reveal(w,local);QTest::mouseClick(local,Qt::LeftButton);events();
            text_canonical(w,before,DefinitionCommand{SetInstanceTextContentOverride{"content","text",raw_text}});
            const auto saved=s.document();const auto file=scratch.filePath("text-actions.nect");w.host.save(file);w.host.open(file);events();select_frame(w,"target");
            check(s.document()==saved&&encode(s.document())==encode(saved),"Native reopen retains full raw local/source Text and unrelated authored state");
            Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();cold.activateWindow();events();select_frame(cold,"target");
            check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Fresh Window reopen retains complete authored/native state");
            before=cold.host.session;reset=visible<QPushButton>(cold,"instance-text-content-reset");reveal(cold,reset);check(reset->isEnabled(),"Reopened Use Source remains reachable and enabled");
            reset->setFocus();QTest::keyClick(reset,Qt::Key_Space);events();text_canonical(cold,before,DefinitionCommand{ResetInstanceTextContentOverride{"content","text"}});
            if(!evidence.isEmpty()){reset=visible<QPushButton>(cold,"instance-text-content-reset");reveal(cold,reset);check(cold.grab().save(evidence+".reopen.png"),"Reopened Text layout screenshot saved");}
            cold.host.changed={};cold.hide();w.host.changed={};w.hide();
            std::cout<<"PASS "<<checks<<" Instance Text action layout checks; physical input NOT_RUN\n";return 0;
        }
        auto& s=w.host.session;s.apply({Rename{"text","Existing history"}},s.revision());w.host.edited();events();
        const Snapshot initial(s);const auto canvas_width=w.canvas->width();const auto pane_width=viewport->width();
        check(area->horizontalScrollBar()->maximum()==0,"Default pane has no source-control overflow");
        for(auto* button:area->widget()->findChildren<QPushButton*>())reveal(w,button);
        check(initial.unchanged(s)&&w.canvas->width()==canvas_width,"Every frame/layout/Template/Guide action fits without authoring or Canvas resize");
        auto* link=visible<QPushButton>(w,"artboard-width-link");cancel=visible<QPushButton>(w,"artboard-width-cancel");
        check(cancel->y()>link->y(),"Narrow source actions wrap");
        auto* dock=w.findChild<QDockWidget*>("properties");const auto dock_width=dock->width();w.resizeDocks({dock},{600},Qt::Horizontal);events();
        check(cancel->y()==link->y(),"Wider user-resized pane reuses one action row");
        w.resizeDocks({dock},{dock_width},Qt::Horizontal);events();
        check(viewport->width()==pane_width&&cancel->y()>link->y()&&initial.unchanged(s),"Returning to standard pane restores wrapping without Session mutation");
        auto* expression=visible<QPlainTextEdit>(w,"artboard-width-expression");reveal(w,expression);
        QTest::mouseClick(expression->viewport(),Qt::LeftButton);QTest::keyClicks(expression,"900");events();
        check(initial.unchanged(s),"Typing a source expression is a local draft");
        cancel=visible<QPushButton>(w,"artboard-width-cancel");reveal(w,cancel);QTest::mouseClick(cancel,Qt::LeftButton);events();
        check(initial.unchanged(s)&&visible<QPlainTextEdit>(w,"artboard-width-expression")->toPlainText().isEmpty(),"Visible pointer Cancel discards draft and preserves full source/history/preview");
        auto* search=visible<QLineEdit>(w,"artboard-width-link-source-search");reveal(w,search);
        QTest::mouseClick(search,Qt::LeftButton);QTest::keyClicks(search,"decoy artboard.height");events();
        auto* chooser=visible<QComboBox>(w,"artboard-width-link-source");reveal(w,chooser);chooser->setFocus();QTest::keyClick(chooser,Qt::Key_Down);events();
        check(chooser->count()==1&&chooser->currentData(Qt::ToolTipRole).toString()=="decoy/artboard.height"&&chooser->currentText().contains("Shared frame"),"Compact picker preserves full duplicate-name label and exact source field");
        check(initial.unchanged(s),"Search and source choice retain full authored state");
        link=visible<QPushButton>(w,"artboard-width-link");reveal(w,link);check(link->isEnabled(),"Visible Link enabled after exact source selection");
        QTest::mouseClick(link,Qt::LeftButton);events();canonical(w,initial,{LinkArtboardSize{{"source","","artboard.width"},{"decoy","","artboard.height"},false}});
        check(evaluate_artboard(s.document().compositions.front(),"source").width==800&&evaluate_artboard(s.document().compositions.front(),"target").width==640,"Link evaluates source while target override stays local");
        const Snapshot linked(s);auto* unlink=visible<QPushButton>(w,"artboard-width-unlink");reveal(w,unlink);check(unlink->isEnabled(),"Visible Unlink enabled for retained source");
        unlink->setFocus();QTest::keyClick(unlink,Qt::Key_Space);events();canonical(w,linked,{UnlinkArtboardSize{{"source","","artboard.width"}}});
        check(board(s.document(),"source").width==800&&!board(s.document(),"source").width_driver,"Keyboard Unlink retains evaluated size");
        const Snapshot literal(s);expression=visible<QPlainTextEdit>(w,"artboard-height-expression");reveal(w,expression);
        QTest::mouseClick(expression->viewport(),Qt::LeftButton);QTest::keyClicks(expression,"900");events();check(literal.unchanged(s),"Height expression draft is source-neutral");
        auto* apply=visible<QPushButton>(w,"artboard-height-apply-expression");reveal(w,apply);check(apply->isEnabled(),"Visible Apply expression enabled");
        QTest::mouseClick(apply,Qt::LeftButton);events();canonical(w,literal,{SetArtboardSizeExpression{{"source","","artboard.height"},{"900",1},false}});
        check(evaluate_artboard(s.document().compositions.front(),"target").height==900,"Source height expression propagates through Template");
        const auto saved=s.document();const auto file=scratch.filePath("responsive-source.nect");w.host.save(file);w.host.open(file);events();select_frame(w,"source");
        check(s.document()==saved,"Native reopen preserves complete size source and Template layout");
        Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();cold.activateWindow();events();select_frame(cold,"source");
        check(cold.host.session.document()==saved,"Fresh Window native reopen preserves exact authored document");
        cancel=visible<QPushButton>(cold,"artboard-height-cancel");reveal(cold,cancel);const Snapshot reopened(cold.host.session);
        cancel->setFocus();QTest::keyClick(cancel,Qt::Key_Space);events();check(reopened.unchanged(cold.host.session),"Reopened visible keyboard Cancel keeps canonical source/history/preview");
        cold.host.changed={};cold.hide();
        w.host.changed={};w.hide();std::cout<<"PASS "<<checks<<" frame source layout checks; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
