#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QFile>
#include <QDockWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);QApplication::processEvents();}
auto snapshot(Session& s){return std::tuple{s.document(),encode(s.document()),s.revision(),s.history()};}
QRect region(QWidget* widget,Window& w){return {widget->mapTo(&w,QPoint{}),widget->size()};}
bool reachable(QWidget* widget){return widget&&widget->isVisible()&&widget->visibleRegion().contains(widget->rect());}
Document fixture(){
    const bool stacked=QCoreApplication::arguments().contains("--stacked-effects")||QCoreApplication::arguments().contains("--effect-edit-probe")||QCoreApplication::arguments().contains("--effect-draft-context");
    auto d=empty_document("workspace-document","composition","board");
    auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;
    Object path;path.id="curve";path.name="Editable workspace curve";
    Point a;a.id="first";a.x.literal=120;a.y.literal=160;a.out_length.literal=60;
    Point b;b.id="second";b.x.literal=360;b.y.literal=240;b.in_angle.literal=180;b.in_length.literal=60;
    path.contours={{"contour",false,{a,b}}};
    if(stacked){
        Point c;c.id="third";c.x.literal=120;c.y.literal=300;
        path.contours.front().closed=true;path.contours.front().points.push_back(c);
    }
    auto stroke=default_operation("stroke","nect.paint.stroke");stroke.parameters.at("width").literal=3;path.stack={stroke};
    d.objects.emplace(path.id,path);d.compositions.front().roots={path.id};
    Session s(d);s.apply({CreateText{"composition",{},"text","Editable Text",default_text("text-source","Workspace ABC 日本語")},
        Set{{"text",{},"transform.tx"},200},Set{{"text",{},"transform.ty"},300}},s.revision());
    if(stacked)
        for(int i=0;i<3;++i)s.apply({AddOperation{"curve",default_operation("offset-"+std::to_string(i),"nect.shape.offset"),static_cast<std::size_t>(i)}},s.revision());
    if(QCoreApplication::arguments().contains("--effect-draft-context")){
        s.apply({CreatePrimitive{"composition",{},"retained","Untouched Rectangle",default_primitive("retained-source","nect.shape.rectangle")}},s.revision());
        auto document=s.document();document.compositions.front().artboards.push_back({"other-board","Untouched Artboard",700,0,320,240});
        return document;
    }
    return s.document();
}
QTabBar* dock_tabs(Window& w){
    for(auto* bar:w.findChildren<QTabBar*>())
        for(int i=0;i<bar->count();++i)if(bar->tabText(i)=="Properties")return bar;
    throw std::runtime_error("Real Properties/Effects dock tabs missing");
}
void tab(Window& w,const QString& label){
    auto* bar=dock_tabs(w);int target=-1;
    for(int i=0;i<bar->count();++i)if(bar->tabText(i)==label)target=i;
    check(target>=0&&reachable(bar),"Real dock tab is fully reachable");
    QTest::mouseClick(bar,Qt::LeftButton,Qt::NoModifier,bar->tabRect(target).center());events();
    check(bar->currentIndex()==target,"Pointer switches the real dock tab");
}
void save(Window& w,const QString& name){
    const auto dir=qEnvironmentVariable("NECT_WORKSPACE_EVIDENCE");
    if(!dir.isEmpty())check(w.grab().save(dir+"/"+name+".png"),"Source-rendered Window evidence saved");
}
void effect_draft_context(Window& w,const QString& directory){
    QApplication::setActiveWindow(&w);events();
    auto& s=w.host.session;
    auto complete_session=[](const Session& session){return std::tuple{session.document(),encode(session.document()),session.preview_document(),session.revision(),session.history(),session.gesture_generation(),session.gesture_active()};};
    auto complete=[&]{return complete_session(s);};
    const Ref amount_ref{"curve",{},"op.offset-2.amount"};
    auto* inspector=w.findChild<QScrollArea*>("inspector-scroll");
    auto field=[&](const Ref& wanted){
        for(auto* field:inspector->findChildren<QLineEdit*>()){
            const auto ref=QJsonDocument::fromJson(field->property("nect-reference").toByteArray()).object();
            if(field->isVisible()&&ref.value("object").toString()==QString::fromStdString(wanted.object)&&
               ref.value("point").toString()==QString::fromStdString(wanted.point)&&ref.value("field").toString()==QString::fromStdString(wanted.field)){
                inspector->ensureWidgetVisible(field);events();
                check(field->isEnabled()&&reachable(field),"Exact effect Amount is visible, enabled and fully reachable");return field;
            }
        }
        throw std::runtime_error("Missing exact effect Amount field");
    };
    auto amount=[&]{return field(amount_ref);};
    auto draft=[&](const Ref& ref,const QString& text){
        auto* input=field(ref);QTest::mouseClick(input,Qt::LeftButton);events();
        QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);events();
        check(input->hasFocus()&&input->text()==text&&input->isModified(),"Real visible property input holds unfinished focused keyboard draft");
        return QPointer<QLineEdit>(input);
    };
    tab(w,"Effects");
    auto* scroll=w.findChild<QScrollArea*>("effects-scroll");
    auto* last=w.findChild<QPushButton*>("effects-edit-properties-offset-2");
    check(scroll&&last,"Exact retained effect entry exists");scroll->ensureWidgetVisible(last);events();
    check(last->isEnabled()&&reachable(last),"Exact effect Properties entry is reachable");
    const auto before_navigation=complete();QTest::mouseClick(last,Qt::LeftButton);events();
    check(complete()==before_navigation,"Effect entry navigation preserves complete canonical context");
    s.begin_gesture(s.revision());s.update_gesture({Set{{"curve",{},"transform.tx"},44}});w.host.edited();events();
    const auto active_preview=complete();QPointer<QLineEdit> input=amount();
    QTest::mouseClick(input,Qt::LeftButton);events();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"13");events();
    std::cout<<"effect draft focus="<<input->hasFocus()<<" text="<<input->text().toStdString()<<" modified="<<input->isModified()<<'\n';
    check(input->hasFocus()&&input->text()=="13"&&input->isModified(),"Preview-born effect Amount holds unfinished real keyboard draft");
    check(complete()==active_preview,"Unfinished effect draft preserves external preview and complete source");
    s.cancel_gesture();const auto cancelled=complete();
    check(input&&input->hasFocus(),"External cancellation precedes focused Amount refresh");
    w.host.edited();events();
    std::cout<<"effect Amount cancel/refresh value="<<property(s.document(),{"curve",{},"op.offset-2.amount"}).literal
        <<" revision="<<s.revision()<<" expected="<<std::get<3>(cancelled)<<'\n';
    check(complete()==cancelled,"Cancelling external preview then refreshing cannot commit its unfinished effect Amount");

    const auto history_action=[&](const char* name){
        for(auto* action:w.findChildren<QAction*>())if(action->text()==QString::fromLatin1(name)){
            check(action->isEnabled(),"Existing Window history action is enabled");action->trigger();events();return;
        }
        throw std::runtime_error("Missing Window history action");
    };
    const auto commit=[&](const Ref& ref,const QString& text,const Command& command){
        const auto before=complete();Session expected=s;expected.apply({command},expected.revision());
        auto input=draft(ref,text);check(complete()==before,"Typing a fresh property draft does not author source");
        QTest::keyClick(input,Qt::Key_Return);events();
        check(complete()==complete_session(expected),"Fresh Return equals one complete canonical source/native/history transaction");
        expected.undo(expected.revision());history_action("Undo");check(complete()==complete_session(expected),"One Window Undo restores complete canonical context");
        expected.redo(expected.revision());history_action("Redo");check(complete()==complete_session(expected),"One Window Redo restores complete canonical context");
    };
    commit(amount_ref,"13",EditProperties{{amount_ref},13,false});
    commit(amount_ref,"+=2",EditProperties{{amount_ref},2,true});

    input=draft(amount_ref,"31");
    s.apply({EditProperties{{amount_ref},22,false}},s.revision());const auto external=complete();w.host.edited();events();
    check(complete()==external,"External canonical Amount edit survives unfinished draft refresh with full retained source");
    check(amount()->text()=="22","Fresh Amount shows the incoming canonical edit");

    input=draft(amount_ref,"37");s.begin_gesture(s.revision());s.update_gesture({Set{{"curve",{},"transform.tx"},55}});
    const auto external_preview=complete();QTest::keyClick(input,Qt::Key_Return);events();
    check(complete()==external_preview,"Old property Return preserves another client's complete active preview");
    s.cancel_gesture();const auto old_cancelled=complete();w.host.edited();events();
    check(complete()==old_cancelled,"Old property draft remains invalid after intervening preview cancellation");

    s.begin_gesture(s.revision());s.update_gesture({Set{{"curve",{},"transform.tx"},66}});w.host.edited();events();
    input=draft(amount_ref,"41");const auto born_preview=complete();QTest::keyClick(input,Qt::Key_Return);events();
    check(complete()==born_preview,"Preview-born Amount Return preserves external preview ownership and source");
    s.cancel_gesture();const auto born_cancelled=complete();w.host.edited();events();
    check(complete()==born_cancelled,"Refused preview-born input stays nonmutating after cancel/refresh");

    input=draft(amount_ref,"47");s.begin_gesture(s.revision());s.update_gesture({Set{{"curve",{},"transform.tx"},77}});
    s.commit_gesture();const auto committed_preview=complete();w.host.edited();events();
    check(complete()==committed_preview,"External preview commit survives unfinished property draft refresh");

    input=draft(amount_ref,"not-a-number");const auto invalid=complete();QTest::keyClick(input,Qt::Key_Return);events();
    check(complete()==invalid,"Invalid numeric input is completely atomic");
    commit(amount_ref,"=7+2",SetExpression{{amount_ref},{"7+2",1},false});
    const auto driven=complete();auto* readonly=amount();check(readonly->isReadOnly(),"Expression source stays explicitly read-only");
    QTest::keyClick(readonly,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(readonly,"99");QTest::keyClick(readonly,Qt::Key_Return);events();
    check(complete()==driven,"Literal input cannot silently unlink expression source");

    w.canvas->set_selection("curve","first");events();
    const Ref point_ref{"curve","first","x"};check(!field(point_ref)->property("nect-semantic-key").isValid(),"Authored point uses the ordinary shared scalar field");
    commit(point_ref,"128",EditProperties{{point_ref},128,false});
    const auto native=directory+"/effect-context.nect";const auto saved=s.document();w.host.save(native);events();
    QFile file(native);check(file.open(QIODevice::ReadOnly)&&file.readAll().toStdString()==encode(saved),"Same Window native save writes every authored field exactly");file.close();
    input=draft(point_ref,"188");const auto old_session=w.host.session_id;w.host.open(native);events();
    const Session reopened(saved);
    check(w.host.session_id!=old_session&&complete()==complete_session(reopened),"Same-ID native reload discards focused scalar draft and preserves exact source/new Session context");
    w.canvas->set_selection("curve","first");events();commit(point_ref,"132",EditProperties{{point_ref},132,false});
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try {
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");
        QSettings preferences(scratch.filePath("preferences.ini"),QSettings::IniFormat);
        preferences.setValue("unrelated","preserved");preferences.setValue("workspace/tools/textCreationDirection","vertical");
        Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
        w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1100,750);w.show();w.activateWindow();events();
        auto& s=w.host.session;s=Session(fixture());s.apply({Set{{"curve",{},"transform.tx"},12}},s.revision());w.host.edited();events();
        w.canvas->set_selection("curve");events();w.canvas->fit_artboard();
        auto* direct=w.findChild<QToolButton*>("tool-direct-selection");QTest::mouseClick(direct,Qt::LeftButton);events();
        auto* left=w.findChild<QDockWidget*>("structure");auto* properties=w.findChild<QDockWidget*>("properties");
        auto* effects=w.findChild<QDockWidget*>("effects");auto* inspector=w.findChild<QScrollArea*>("inspector-scroll");
        check(left&&properties&&effects&&inspector,"Actual primary Window regions exist");
        if(QCoreApplication::arguments().contains("--effect-draft-context")){
            effect_draft_context(w,scratch.path());std::cout<<"PASS "<<checks<<" effect draft context checks (physical OS input NOT_RUN)\n";return 0;
        }
        if(QCoreApplication::arguments().contains("--effect-edit-probe")){
            w.resize(1000,650);events();tab(w,"Effects");
            auto* scroll=w.findChild<QScrollArea*>("effects-scroll");check(scroll,"Actual Effects scroll path exists");
            auto* bar=scroll->verticalScrollBar();bar->setFocus();QTest::keyClick(bar,Qt::Key_End);events();
            auto* last=w.findChild<QPushButton*>("effects-edit-properties-offset-2");check(reachable(last),"Last exact retained instance entry reachable after scrolling");
            const auto entry=snapshot(s);const auto selection=w.canvas->selections();save(w,"effect-entry-before");
            QTest::mouseClick(last,Qt::LeftButton);events();
            check(snapshot(s)==entry&&w.canvas->selections()==selection&&w.canvas->direct_selection_mode(),
                "Actual Effects-to-Properties navigation is authored/history/selection/Tool neutral");
            check(!properties->visibleRegion().isEmpty(),"Last-instance callback reveals real Properties");
            QLineEdit* input=nullptr;
            for(auto* field:inspector->findChildren<QLineEdit*>()){
                const auto ref=QJsonDocument::fromJson(field->property("nect-reference").toByteArray()).object();
                if(field->isVisible()&&ref.value("object")=="curve"&&ref.value("field")=="op.offset-2.amount"){input=field;break;}
            }
            check(input,"Exact last operation scalar control is present");inspector->ensureWidgetVisible(input);events();
            check(reachable(input),"Exact last-instance scalar is fully reachable");save(w,"effect-parameter-before");
            const auto initial=s.document();const auto revision=s.revision();Session expected(initial);
            expected.apply({EditProperties{{{"curve",{},"op.offset-2.amount"}},13,false}},expected.revision());
            input->setFocus();input->selectAll();QTest::keyClicks(input,"13");QTest::keyClick(input,Qt::Key_Return);events();
            check(s.document()==expected.document()&&s.revision()==revision+1,"Real last-instance numeric input performs exactly canonical source edit");
            save(w,"effect-parameter-after");
            auto history_action=[&](const QString& label){
                for(auto* action:w.findChildren<QAction*>())if(action->text()==label){action->trigger();events();return;}
                throw std::runtime_error("Existing history action missing");
            };
            history_action("Undo");check(s.document()==initial&&encode(s.document())==std::get<1>(entry),"Existing Undo restores every authored field and native source");
            history_action("Redo");check(s.document()==expected.document(),"Existing Redo restores exact last-instance edit");
            const auto native=scratch.filePath("effect-edited.nect");w.host.save(native);events();w.host.open(native);events();
            check(s.document()==expected.document(),"Same Window native reopen preserves all source after last-instance edit");
            Window reopened(scratch.filePath("effect-reopened"),std::make_unique<FolderLibrary>(preferences),&preferences);reopened.host.open(native);
            check(reopened.host.session.document()==expected.document(),"New Window native reopen preserves exact edited effect instance");
            std::cout<<"DPR="<<w.devicePixelRatioF()<<" PASS "<<checks<<" exact effect entry/edit checks\n";return 0;
        }
        QLineEdit* numeric=nullptr;
        for(auto* input:w.findChildren<QLineEdit*>()){
            const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
            if(input->isVisible()&&ref.value("object")=="curve"&&ref.value("field")=="transform.anchor_x"){numeric=input;break;}
        }
        check(numeric,"Exact source property reachable through the integrated Inspector");
        auto* fx=numeric->parentWidget()->findChild<QPushButton*>("property-expression");
        inspector->ensureWidgetVisible(fx);events();
        std::cout<<"fx="<<region(fx,w).x()<<','<<region(fx,w).y()<<','<<fx->width()<<','<<fx->height()
            <<" visible="<<fx->isVisible()<<" viewport="<<inspector->viewport()->width()<<','<<inspector->viewport()->height()
            <<" scroll="<<inspector->horizontalScrollBar()->value()<<','<<inspector->verticalScrollBar()->value()<<std::endl;
        save(w,"workspace-expression-entry");check(reachable(fx),"Actual expression entry can be revealed");
        QTest::mouseClick(fx,Qt::LeftButton);events();
        auto* draft=inspector->findChild<QPlainTextEdit*>();check(draft,"Explicit uncommitted expression editor opened");
        draft->setPlainText("12 + 7");events();
        const auto before=snapshot(s);const auto selection=w.canvas->selections();
        auto current_draft=[&]{
            for(auto* editor:inspector->findChildren<QPlainTextEdit*>()){
                const auto ref=QJsonDocument::fromJson(editor->property("nect-reference").toByteArray()).object();
                if(ref.value("object")=="curve"&&ref.value("field")=="transform.anchor_x")return editor;
            }
            throw std::runtime_error("Exact uncommitted property draft missing");
        };
        auto neutral=[&]{
            check(snapshot(s)==before&&w.canvas->selections()==selection&&!s.gesture_active(),"Layout transitions preserve complete source/native/revision/history/selection");
            check(w.canvas->direct_selection_mode()&&direct->isChecked(),"Current Tool survives workspace transitions");
            check(current_draft()->toPlainText()=="12 + 7","Explicit property draft survives without applying");
        };
        for(const auto size:{QSize(1100,750),QSize(1000,650),QSize(1440,900)}){
            w.resize(size);events();
            const auto canvas=region(w.canvas,w),l=region(left,w),r=region(properties,w);
            std::cout<<"layout requested="<<size.width()<<'x'<<size.height()<<" actual="<<w.width()<<'x'<<w.height()
                <<" DPR="<<w.devicePixelRatioF()<<" left="<<l.x()<<','<<l.y()<<','<<l.width()<<','<<l.height()
                <<" right="<<r.x()<<','<<r.y()<<','<<r.width()<<','<<r.height()
                <<" canvas="<<canvas.x()<<','<<canvas.y()<<','<<canvas.width()<<','<<canvas.height()<<std::endl;
            save(w,"workspace-properties-"+QString::number(size.width()));
            check(w.size()==size,"Requested ordinary/narrow/wide logical Window size is respected");
            check(l.right()<canvas.left()&&canvas.right()<r.left()&&canvas.width()>=350&&canvas.height()>=400,
                "Fixed left/right regions retain a usable primary Canvas");
            for(const auto* name:{"tool-selection","tool-pen","tool-text","tool-anchor","tool-guide","tool-gradient","tool-hand","tool-zoom","tool-direct-selection"})
                check(reachable(w.findChild<QToolButton*>(name)),"Every existing Rail slot remains fully reachable");
            neutral();tab(w,"Effects");
            save(w,"workspace-effects-"+QString::number(size.width()));
            for(const auto* name:{"effects-search","effects-catalog","effects-apply","effects-favorite"}){
                auto* control=w.findChild<QWidget*>(name);
                if(auto* scroll=w.findChild<QScrollArea*>("effects-scroll")){scroll->ensureWidgetVisible(control);events();}
                std::cout<<name<<" visible="<<(control&&control->isVisible())<<" full="<<reachable(control)<<std::endl;
                check(reachable(control),"Major Effects entries fit or can be reached in actual Window");
            }
            if(QCoreApplication::arguments().contains("--stacked-effects")){
                auto* last=w.findChild<QPushButton*>("effects-edit-properties-offset-2");
                if(auto* scroll=w.findChild<QScrollArea*>("effects-scroll")){
                    auto* bar=scroll->verticalScrollBar();check(bar->maximum()>0,"Stacked instances expose an actual scrollbar");
                    bar->setFocus();QTest::keyClick(bar,Qt::Key_End);events();
                    check(bar->value()==bar->maximum(),"Real keyboard navigation reaches the bottom of Effects");
                }
                save(w,"workspace-last-effect-"+QString::number(size.width()));
                check(reachable(last),"Every retained effect instance is reachable, including the last stacked instance");
            }
            auto* inner=w.findChild<QTabWidget*>("effects-tabs");
            QTest::mouseClick(inner->tabBar(),Qt::LeftButton,Qt::NoModifier,inner->tabBar()->tabRect(1).center());events();
            check(inner->currentIndex()==1&&reachable(inner->tabBar()),"Actual Presets tab stays reachable in the fixed Effects region");
            for(const auto* name:{"presets-search","presets-catalog","preset-save","preset-apply","preset-delete"}){
                auto* entry=w.findChild<QWidget*>(name);check(entry,"Major Presets entry exists");
                if(auto* scroll=w.findChild<QScrollArea*>("presets-scroll")){scroll->ensureWidgetVisible(entry);events();}
                check(reachable(entry),"Presets entries remain reachable in narrow/wide Window");
            }
            QTest::mouseClick(inner->tabBar(),Qt::LeftButton,Qt::NoModifier,inner->tabBar()->tabRect(0).center());events();
            neutral();tab(w,"Properties");neutral();
            auto* utility=w.findChild<QScrollArea*>("canvas-utility-scroll");
            for(const auto* name:{"canvas-create-circle","canvas-create-rectangle","canvas-create-text","canvas-create-path",
                "canvas-zoom-percent","utility-setup"}){
                auto* entry=w.findChild<QWidget*>(name);check(entry,"Major Create/Utility entry exists");
                utility->ensureWidgetVisible(entry);events();check(reachable(entry),"Horizontal overflow retains full Create/Utility reachability");
            }
            utility->horizontalScrollBar()->setValue(0);events();neutral();
            for(auto* dock:{left,properties,effects}){
                dock->toggleViewAction()->trigger();events();check(dock->isHidden(),"View action closes the exact panel");neutral();
                dock->toggleViewAction()->trigger();events();check(!dock->isHidden()&&!dock->isFloating(),"View action reopens exact panel in its fixed region");neutral();
            }
            properties->raise();events();
        }
        w.resize(1100,750);events();
        // Exercise QMainWindow's real splitter handling, rather than resizing
        // helper/shell docks or setting a panel's width directly.
        for(const bool resize_left:{true,false}){
            auto* dock=resize_left?left:properties;
            const auto box=region(dock,w);const auto old_width=dock->width();
            const QPoint start(resize_left?box.right()+3:box.left()-3,box.center().y());
            const QPoint end=start+QPoint(resize_left?35:-25,0);
            QTest::mouseMove(&w,start);events();
            std::cout<<"splitter start="<<start.x()<<','<<start.y()<<" cursor="<<w.cursor().shape()<<std::endl;
            QTest::mousePress(&w,Qt::LeftButton,Qt::NoModifier,start);
            QMouseEvent move(QEvent::MouseMove,end,w.mapToGlobal(end),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(&w,&move);events();
            QTest::mouseRelease(&w,Qt::LeftButton,Qt::NoModifier,end);events();
            std::cout<<"splitter "<<(resize_left?"left":"right")<<" width="<<old_width<<" -> "<<dock->width()<<std::endl;
            check(dock->width()>old_width&&w.canvas->width()>=350,"Real pointer divider drag resizes fixed panel and retains Canvas");neutral();
        }
        save(w,"workspace-resized");
        // Saving while the explicit draft is open persists only authored state.
        const auto native=scratch.filePath("workspace.nect");w.host.save(native);events();neutral();
        Window reopened(scratch.filePath("reopened"),std::make_unique<FolderLibrary>(preferences),&preferences);
        reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.host.open(native);
        check(reopened.host.session.document()==s.document()&&encode(reopened.host.session.document())==encode(s.document()),
            "New real Window reopens complete authored native source without property draft");
        // Commit the surviving draft through its existing Ctrl+Enter callback.
        draft=current_draft();inspector->ensureWidgetVisible(draft);events();check(reachable(draft),"Surviving draft remains editable after all workspace transitions");
        const auto pre_apply=s.document();Session expected(pre_apply);
        expected.apply({SetExpression{{{"curve",{},"transform.anchor_x"}},{"12 + 7",1},false}},expected.revision());
        QTest::keyClick(draft,Qt::Key_Return,Qt::ControlModifier);events();
        check(s.document()==expected.document()&&s.revision()==std::get<2>(before)+1,"Draft Apply performs exactly the canonical source command");
        const auto applied=s.document();s.undo(s.revision());w.host.edited();events();
        check(s.document()==pre_apply&&encode(s.document())==std::get<1>(before),"One Undo restores full source after post-layout property authoring");
        s.redo(s.revision());w.host.edited();events();check(s.document()==applied,"One Redo restores exact property source");
        w.host.save(native);events();w.host.open(native);events();
        check(s.document()==applied,"Same Window native reopen preserves exact committed property source");
        // One-shot creation is still on the separate Utility surface. It uses
        // the retained Text preference without altering existing Path/Text.
        auto* utility=w.findChild<QScrollArea*>("canvas-utility-scroll");
        auto* create=w.findChild<QToolButton*>("canvas-create-text");utility->ensureWidgetVisible(create);events();
        check(reachable(create)&&create->isEnabled(),"Actual Text creation is reachable after workspace transitions");
        const auto pre_create=s.document();const auto create_revision=s.revision();
        QTest::mouseClick(create,Qt::LeftButton);events();
        const auto created=s.document();check(created.objects.size()==pre_create.objects.size()+1&&s.revision()==create_revision+1,
            "Visible Create Text authors one actual source through the existing Session command");
        for(const auto& [id,object]:pre_create.objects)check(created.objects.at(id)==object,"Creation preserves every existing authored object");
        for(const auto& [id,object]:created.objects)if(!pre_create.objects.contains(id))
            check(object.text&&object.text->direction=="vertical","One-shot Text creation retains last-used workspace variant");
        s.undo(s.revision());w.host.edited();events();check(s.document()==pre_create,"One Undo removes only the created Text");
        s.redo(s.revision());w.host.edited();events();check(s.document()==created,"One Redo restores complete created Text source");
        save(w,"workspace-created-vertical-text");
        const auto variant_snapshot=snapshot(s);
        w.findChild<QAction*>("tool-text-horizontal")->trigger();events();check(snapshot(s)==variant_snapshot,"Horizontal variant selection remains authored/history neutral");
        create=w.findChild<QToolButton*>("canvas-create-text");utility->ensureWidgetVisible(create);events();
        const auto pre_horizontal=s.document();QTest::mouseClick(create,Qt::LeftButton);events();
        const auto horizontal=s.document();
        for(const auto& [id,object]:pre_horizontal.objects)check(horizontal.objects.at(id)==object,"Horizontal one-shot creation preserves every existing object");
        for(const auto& [id,object]:horizontal.objects)if(!pre_horizontal.objects.contains(id))check(object.text&&object.text->direction=="horizontal","Horizontal one-shot creation uses the actual current variant");
        s.undo(s.revision());w.host.edited();events();check(s.document()==pre_horizontal,"One Undo restores the full source before Horizontal creation");
        w.findChild<QAction*>("tool-text-vertical")->trigger();events();
        check(preferences.value("unrelated")=="preserved"&&preferences.value("workspace/tools/textCreationDirection")=="vertical","Workspace retains Text preference and unrelated settings");
        std::cout<<"PASS "<<checks<<" actual Window workspace checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<" after "<<checks<<" checks\n";return 1;}
}
