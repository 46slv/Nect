#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QAction>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
Document fixture(){
    auto d=empty_document("instance-navigation","composition","source-board");
    d.compositions.front().artboards.push_back({"other-board","Other",700,0,640,480});
    Object folder;folder.id="folder";folder.name="Folder";folder.kind=Kind::group;folder.children={"root","decoy"};
    Object root;root.id="root";root.name="Shared source";root.kind=Kind::group;root.children={"rectangle","text"};
    Object rectangle;rectangle.id="rectangle";rectangle.name="Rectangle";
    rectangle.source=default_primitive("rectangle-source","nect.shape.rectangle");
    rectangle.stack={default_operation("fill","nect.paint.fill")};
    Object text;text.id="text";text.name="Text";text.kind=Kind::text;text.text=default_text("text-source","Retained text");
    text.text->parameters.at("font_size").expression=Expression{"24 + 2",1};
    Object decoy;decoy.id="decoy";decoy.name=root.name;decoy.kind=Kind::group;
    for(const auto& o:{folder,root,rectangle,text,decoy})d.objects.emplace(o.id,o);
    d.compositions.front().roots={folder.id};
    Session s(d);s.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        DefinitionCommand{CreateDefinition{{"decoy-definition","Shared",decoy.id}}},
        DefinitionCommand{CreateInstance{"composition",{},"first","definition","First"}},
        DefinitionCommand{CreateInstance{"composition",{},"second","definition","Second"}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","other-board","template",Id{"content"}}}},0);
    return s.document();
}
struct Snapshot {
    Document document,preview;std::string native;std::uint64_t revision,generation;HistoryInfo history;bool gesture;
    explicit Snapshot(const Session& s):document(s.document()),preview(s.preview_document()),native(encode(document)),revision(s.revision()),generation(s.gesture_generation()),history(s.history()),gesture(s.gesture_active()){}
    bool unchanged(const Session& s)const{return document==s.document()&&preview==s.preview_document()&&native==encode(s.document())&&revision==s.revision()&&generation==s.gesture_generation()&&history==s.history()&&gesture==s.gesture_active();}
};
void select(Window& w,const Id& id){w.canvas->set_selection(id);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();}
QPushButton* entry(Window& w){
    auto* b=w.findChild<QPushButton*>("instance-source-go");check(b,"Selected Instance has contextual shared-source navigation");
    for(auto* scroll:w.findChildren<QScrollArea*>())if(scroll->widget()&&scroll->widget()->isAncestorOf(b))scroll->ensureWidgetVisible(b);
    events();check(b->isVisible()&&b->visibleRegion().contains(b->rect()),"Source action fully reachable in actual Properties viewport");return b;
}
void history(Window& w,const QString& label){for(auto* a:w.findChildren<QAction*>())if(a->text()==label){a->trigger();events();return;}throw std::runtime_error("History action absent");}
void navigated(Window& w,const Snapshot& before,const Id& artboard,double zoom){
    check(w.canvas->selected_object=="root"&&w.canvas->selected_point.empty()&&w.canvas->selections()==std::vector<Canvas::Selection>{{"root",{}}},"Navigation selects only exact stable root despite duplicate names");
    check(before.unchanged(w.host.session),"Navigation preserves complete Document/native/revision/history/generation/preview");
    check(w.canvas->active_artboard()==artboard&&w.canvas->zoom()==zoom,"Navigation retains active Artboard and zoom");
    auto* dock=w.findChild<QDockWidget*>("structure");auto* tree=dock?dock->findChild<QTreeWidget*>():nullptr;
    check(dock&&dock->isVisible()&&tree&&tree->currentItem(),"Source Structure is open with a current item");
    auto* item=tree->currentItem();check(item->data(0,Qt::UserRole).toString()=="root"&&item->data(0,Qt::UserRole+1).toString().isEmpty()&&item->isSelected(),"Structure selection agrees with whole source root ID");
    check(item->parent()&&item->parent()->isExpanded()&&tree->viewport()->rect().contains(tree->visualItemRect(item)),"Collapsed source parent expands and source row is viewport-reachable");
    check(w.findChild<QLineEdit*>("object-name")->text()==QString::fromStdString(before.document.objects.at("root").name),"Properties now edits the actual source root");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir scratch;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("InstanceSourceNavigation");
    try{Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="navigation-session";w.host.edited();
        w.resize(1280,900);w.show();w.activateWindow();events();select(w,"first");
        auto& s=w.host.session;
        s.apply({Rename{"first","Retained occurrence"}},s.revision());w.host.edited();events();
        const Snapshot original(s);auto* b=entry(w);check(b->isEnabled(),"Source action enabled");
        const auto evidence=qEnvironmentVariable("NECT_INSTANCE_SOURCE_EVIDENCE");
        if(!evidence.isEmpty())check(w.grab().save(evidence),"Actual Properties source entry captured");
        auto* structure=w.findChild<QDockWidget*>("structure");auto* tree=structure->findChild<QTreeWidget*>();tree->collapseAll();structure->hide();events();
        const auto board=w.canvas->active_artboard();const auto zoom=w.canvas->zoom();
        QTest::mouseClick(b,Qt::LeftButton);events();
        navigated(w,original,board,zoom);
        select(w,"second");const Snapshot keyboard(s);b=entry(w);b->setFocus();events();
        check(b->hasFocus(),"Source entry accepts keyboard focus");QTest::keyClick(b,Qt::Key_Space);events();navigated(w,keyboard,board,zoom);
        // The destination remains the canonical source editor, not a copy.
        const auto before_edit=s.document();Session expected(before_edit);expected.apply({Rename{"root","Edited shared source"}},0);
        const auto before_revision=s.revision();auto* name=w.findChild<QLineEdit*>("object-name");
        QTest::mouseClick(name,Qt::LeftButton);QTest::keyClick(name,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(name,"Edited shared source");QTest::keyClick(name,Qt::Key_Return);events();
        check(s.document()==expected.document()&&s.revision()==before_revision+1&&s.history().states.size()==keyboard.history.states.size()+1,"Reached source edit equals one canonical Rename with every Instance/Template/child retained");
        history(w,"Undo");check(s.document()==before_edit,"One source-edit Undo restores full document");history(w,"Redo");check(s.document()==expected.document(),"One Redo restores exact source edit");
        // A user press may outlive an external change before release.
        select(w,"first");b=entry(w);QTest::mousePress(b,Qt::LeftButton);
        s.apply({Rename{"root","Externally renamed source"}},s.revision());const Snapshot external(s);
        QTest::mouseRelease(b,Qt::LeftButton);events();check(external.unchanged(s)&&w.canvas->selected_object=="first","Pressed stale entry cannot navigate after external revision change");
        w.host.edited();events();b=entry(w);QTest::mousePress(b,Qt::LeftButton);
        s.begin_gesture(s.revision());s.update_gesture({SetVisibility{"root",false}});const Snapshot preview(s);
        QTest::mouseRelease(b,Qt::LeftButton);events();check(preview.unchanged(s)&&w.canvas->selected_object=="first","Old enabled entry cannot cancel or change an externally started preview");
        w.host.edited();events();b=entry(w);check(!b->isEnabled(),"Current preview disables source entry");
        QTest::mouseClick(b,Qt::LeftButton);events();check(preview.unchanged(s),"Disabled source input preserves active preview");
        s.cancel_gesture();w.host.edited();events();b=entry(w);QTest::mousePress(b,Qt::LeftButton);
        const auto replacement=s.document();w.host.session=Session(replacement);w.host.session_id="replacement-session";const Snapshot replaced(s);
        QTest::mouseRelease(b,Qt::LeftButton);events();check(replaced.unchanged(s)&&w.canvas->selected_object=="first","Old same-ID entry cannot navigate into replacement Session");w.host.edited();events();
        select(w,"first");const Snapshot fresh(s);b=entry(w);QTest::mouseClick(b,Qt::LeftButton);events();navigated(w,fresh,board,zoom);
        w.canvas->set_active_artboard("composition","other-board",false);select(w,"content");const Snapshot content(s);
        const auto content_zoom=w.canvas->zoom();b=entry(w);QTest::mouseClick(b,Qt::LeftButton);events();navigated(w,content,"other-board",content_zoom);
        const auto file=scratch.filePath("navigation.nect");const auto saved=s.document();w.host.save(file);w.host.open(file);events();
        check(s.document()==saved,"Native reopen retains full shared source and occurrence state");select(w,"first");const Snapshot reloaded(s);
        const auto reopened_board=w.canvas->active_artboard();const auto reopened_zoom=w.canvas->zoom();b=entry(w);b->setFocus();QTest::keyClick(b,Qt::Key_Space);events();navigated(w,reloaded,reopened_board,reopened_zoom);
        Window cold(scratch.filePath("cold"));cold.host.open(file);cold.resize(1280,900);cold.show();cold.activateWindow();events();
        check(cold.host.session.document()==saved,"Fresh Window native reopen retains all source IDs");select(cold,"second");const Snapshot cold_state(cold.host.session);
        const auto cold_board=cold.canvas->active_artboard();const auto cold_zoom=cold.canvas->zoom();QTest::mouseClick(entry(cold),Qt::LeftButton);events();navigated(cold,cold_state,cold_board,cold_zoom);
        w.canvas->set_selections({{"first",{}},{"second",{}}});events();check(!w.findChild<QPushButton*>("instance-source-go"),"Multi-selection has no one-Instance source entry");
        select(w,"rectangle");check(!w.findChild<QPushButton*>("instance-source-go"),"Plain authored source has no Instance-only entry");
        w.host.changed={};cold.host.changed={};w.hide();cold.hide();std::cout<<"PASS "<<checks<<" Instance source navigation checks; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
