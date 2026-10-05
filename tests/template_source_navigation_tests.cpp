#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QDockWidget>
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
Document fixture(){
    auto d=empty_document("template-navigation","composition","source");auto& c=d.compositions.front();
    c.artboards.front().name="Shared frame";c.artboards.front().width=1200;c.artboards.front().height=800;
    c.artboards.front().layout=ArtboardLayout{Margin{10,20,30,40},Grid{"source-grid",{30,20,100,80},2,2,5,5}};
    c.artboards.push_back({"decoy","Shared frame",1500,0,1200,800});
    c.artboards.push_back({"target","Target",3000,0,640,480});
    Object root;root.id="root";root.name="Source content";root.kind=Kind::group;root.children={"text"};root.visible=false;
    Object text;text.id="text";text.name="Retained text";text.kind=Kind::text;text.text=default_text("text-source","Retained text");
    d.objects.emplace(root.id,root);d.objects.emplace(text.id,text);c.roots={root.id};
    Session s(d);s.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",Id{"content"}}},
        ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","frame.width",640.0}},
        ArtboardGuideCommand{AddArtboardGuide{"composition","source",{"guide","Source guide","x",20,true}}},
        ArtboardGuideCommand{SetArtboardGuideOverride{"composition","target","guide","position",45.0}}},0);
    return s.document();
}
const Artboard& board(const Document& d,const Id& id){for(const auto& b:d.compositions.front().artboards)if(b.id==id)return b;throw std::runtime_error("Missing board");}
struct Snapshot {
    Document document,preview;std::string native;std::uint64_t revision,generation;HistoryInfo history;bool gesture;
    explicit Snapshot(const Session& s):document(s.document()),preview(s.preview_document()),native(encode(document)),revision(s.revision()),generation(s.gesture_generation()),history(s.history()),gesture(s.gesture_active()){}
    bool unchanged(const Session& s)const{return document==s.document()&&preview==s.preview_document()&&native==encode(s.document())&&revision==s.revision()&&generation==s.gesture_generation()&&history==s.history()&&gesture==s.gesture_active();}
};
template<class T>T* visible(Window& w,const char* name){for(auto* p:w.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(std::string("Missing visible ")+name);}
void reveal(Window& w,QWidget* input){for(auto* area:w.findChildren<QScrollArea*>())if(area->widget()&&area->widget()->isAncestorOf(input))area->ensureWidgetVisible(input);events();}
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
QPushButton* entry(Window& w){
    auto* button=visible<QPushButton>(w,"artboard-template-source-go");reveal(w,button);
    check(button->visibleRegion().contains(button->rect()),"Template source action fully inside Properties viewport");return button;
}
void reached(Window& w,const Snapshot& before,double zoom){
    check(before.unchanged(w.host.session),"Navigation preserves complete authored Document/native/preview/revision/history/generation");
    check(w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="source"&&w.canvas->selections().empty(),"Exact source frame selected despite duplicate names and dimensions");
    check(w.canvas->zoom()==zoom,"Source navigation retains zoom");
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");
    std::cout<<"Source Properties horizontal="<<area->horizontalScrollBar()->value()<<std::endl;
    check(area->horizontalScrollBar()->value()==0,"New source frame starts at the Properties left edge");
    const auto viewport_evidence=qEnvironmentVariable("NECT_TEMPLATE_VIEWPORT_EVIDENCE");
    if(!viewport_evidence.isEmpty())check(w.grab().save(viewport_evidence),"Source frame left-edge viewport captured");
    auto* list=w.findChild<QListWidget*>("artboards");check(list->currentItem()&&list->currentItem()->data(Qt::UserRole+1).toString()=="source","Artboards selection agrees with source stable ID");
    check(list->viewport()->rect().contains(list->visualItemRect(list->currentItem())),"Exact source row revealed");
    auto* height=visible<QLineEdit>(w,"artboard-height");reveal(w,height);
    check(height->text().toDouble()==board(before.document,"source").height,"Destination edits source frame height");
    check(visible<QLineEdit>(w,"margin-left")->text().toDouble()==10,"Destination exposes canonical source layout");
}
void history(Window& w,const QString& label){for(auto* a:w.findChildren<QAction*>())if(a->text()==label){a->trigger();events();return;}throw std::runtime_error("History action absent");}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir scratch;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("TemplateSourceNavigation");
    try{Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="template-navigation-session";
        w.host.edited();w.resize(1280,900);w.show();w.activateWindow();events();select_frame(w,"target");
        auto& s=w.host.session;s.apply({Rename{"text","Existing history"}},s.revision());w.host.edited();events();
        auto* area=w.findChild<QScrollArea*>("inspector-scroll");auto* horizontal=area->horizontalScrollBar();
        check(horizontal->isVisible()&&horizontal->maximum()>0,"Fixture exposes actual Properties horizontal overflow");
        horizontal->setFocus();QTest::keyClick(horizontal,Qt::Key_End);events();const auto manual_horizontal=horizontal->value();
        check(manual_horizontal>0,"Actual keyboard scroll moves the Properties viewport right");
        const Snapshot before_refresh(s);w.host.edited();events();
        check(before_refresh.unchanged(s)&&horizontal->value()==manual_horizontal,"Same-context refresh preserves manual horizontal scroll and complete Session");
        const Snapshot original(s);check(original.history.states.size()>1,"Fixture retains existing Undo history");
        const auto zoom=w.canvas->zoom();auto* button=entry(w);check(button->isEnabled(),"Assigned source entry enabled");
        const auto evidence=qEnvironmentVariable("NECT_TEMPLATE_SOURCE_EVIDENCE");if(!evidence.isEmpty())check(w.grab().save(evidence),"Actual Template source entry captured");
        QTest::mouseClick(button,Qt::LeftButton);events();reached(w,original,zoom);
        select_frame(w,"target");const Snapshot keyboard(s);const auto key_zoom=w.canvas->zoom();button=entry(w);button->setFocus();events();
        check(button->hasFocus(),"Source entry accepts keyboard focus");QTest::keyClick(button,Qt::Key_Space);events();reached(w,keyboard,key_zoom);
        auto source=board(s.document(),"source");source.height=850;Session expected(s.document());expected.apply({UpdateArtboard{"composition",source}},0);
        const Snapshot before_edit(s);auto* height=visible<QLineEdit>(w,"artboard-height");reveal(w,height);
        QTest::mouseClick(height,Qt::LeftButton);QTest::keyClick(height,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(height,"850");QTest::keyClick(height,Qt::Key_Return);events();
        check(s.document()==expected.document()&&s.revision()==before_edit.revision+1&&s.history().states.size()==before_edit.history.states.size()+1,"Destination edit equals one full canonical source transaction");
        const auto effective=evaluate_artboard(s.document().compositions.front(),"target");check(effective.height==850&&effective.width==640,"Source edits propagate while target width override remains");
        history(w,"Undo");check(s.document()==before_edit.document&&encode(s.document())==before_edit.native,"Undo restores full Template/content/layout/guide/other-board state");
        history(w,"Redo");check(s.document()==expected.document(),"Redo restores exact canonical source edit");
        select_frame(w,"target");button=entry(w);QTest::mousePress(button,Qt::LeftButton);
        s.apply({Rename{"text","External edit"}},s.revision());const Snapshot external(s);QTest::mouseRelease(button,Qt::LeftButton);events();
        check(external.unchanged(s)&&w.canvas->active_artboard()=="target","Pressed old entry refuses external revision");
        w.host.edited();events();button=entry(w);QTest::mousePress(button,Qt::LeftButton);
        s.begin_gesture(s.revision());auto preview_source=board(s.document(),"source");preview_source.height=900;s.update_gesture({UpdateArtboard{"composition",preview_source}});
        const Snapshot preview(s);QTest::mouseRelease(button,Qt::LeftButton);events();
        check(preview.unchanged(s)&&w.canvas->active_artboard()=="target","Old enabled entry never cancels external preview");
        w.host.edited();events();button=entry(w);check(!button->isEnabled(),"Current preview disables source action");
        QTest::mouseClick(button,Qt::LeftButton);events();check(preview.unchanged(s),"Disabled input retains complete external preview");
        s.cancel_gesture();w.host.edited();events();button=entry(w);QTest::mousePress(button,Qt::LeftButton);
        const auto replacement=s.document();w.host.session=Session(replacement);w.host.session_id="replacement-session";const Snapshot replaced(s);
        QTest::mouseRelease(button,Qt::LeftButton);events();check(replaced.unchanged(s)&&w.canvas->active_artboard()=="target","Old same-ID frame entry refuses replacement Session");w.host.edited();events();
        const auto file=scratch.filePath("template-source.nect");const auto saved=s.document();w.host.save(file);w.host.open(file);events();
        check(s.document()==saved,"Native reopen retains full source and assignments");select_frame(w,"target");const Snapshot reloaded(s);const auto reload_zoom=w.canvas->zoom();
        QTest::mouseClick(entry(w),Qt::LeftButton);events();reached(w,reloaded,reload_zoom);
        Window cold(scratch.filePath("cold"));cold.host.open(file);cold.resize(1280,900);cold.show();cold.activateWindow();events();
        check(cold.host.session.document()==saved,"Fresh Window native reopen retains exact source IDs");select_frame(cold,"target");const Snapshot cold_state(cold.host.session);const auto cold_zoom=cold.canvas->zoom();
        button=entry(cold);button->setFocus();events();QTest::keyClick(button,Qt::Key_Space);events();reached(cold,cold_state,cold_zoom);
        auto* cold_horizontal=cold.findChild<QScrollArea*>("inspector-scroll")->horizontalScrollBar();
        cold_horizontal->setFocus();QTest::keyClick(cold_horizontal,Qt::Key_End);events();
        const Snapshot queued(cold.host.session);
        cold.canvas->set_active_artboard("composition","target",false);cold.host.edited();events();
        check(queued.unchanged(cold.host.session)&&cold_horizontal->value()==0,"Back-to-back new-frame refresh retains queued left-edge reset without Session mutation");
        select_frame(cold,"decoy");check(!cold.findChild<QPushButton*>("artboard-template-source-go"),"Unassigned frame excludes Template-only action");
        w.host.changed={};cold.host.changed={};w.hide();cold.hide();std::cout<<"PASS "<<checks<<" Template source navigation checks; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
