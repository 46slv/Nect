#include "window.hpp"
#include "folder_library.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void events(){QApplication::processEvents();}
auto snapshot(Session& session){return std::tuple{session.document(),encode(session.document()),session.revision(),session.history()};}
void click(Window& window,const char* name){
    auto* button=window.findChild<QToolButton*>(name);
    check(button&&button->isVisible()&&button->isEnabled(),"Rail tool is visible and enabled");
    QTest::mouseClick(button,Qt::LeftButton);events();
}
void pointer_isolation(bool keys_only=false){
    QTemporaryDir scratch;check(scratch.isValid(),"Guide pointer regression owns its files and settings");
    QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
    auto document=empty_document("guide-pointer-document","guide-pointer-composition","guide-pointer-artboard");
    auto& composition=document.compositions.front();
    composition.artboards.front().width=640;composition.artboards.front().height=480;
    composition.guides={{"guide-pointer-x","Vertical","x",120}};
    Session setup(document);
    setup.apply({CreatePrimitive{composition.id,{},"guide-pointer-object","Rectangle",
        default_primitive("guide-pointer-shape","nect.shape.rectangle")},
        AddOperation{"guide-pointer-object",default_operation("guide-pointer-fill","nect.paint.fill"),0},
        Set{{"guide-pointer-object",{},"transform.tx"},320},
        Set{{"guide-pointer-object",{},"transform.ty"},240}},setup.revision());
    auto& session=window.host.session;session=Session(setup.document());window.host.edited();events();
    window.canvas->set_snap_enabled(false);window.canvas->fit_artboard();events();
    const auto screen=[&](double x,double y){return QPoint(qRound(window.canvas->width()/2.0+(x-320)*window.canvas->zoom()),
        qRound(window.canvas->height()/2.0+(y-240)*window.canvas->zoom()));};
    auto* tree=window.findChild<QTreeWidget*>();check(tree&&tree->isVisible(),"Actual Structure tree is reachable");
    QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator i(tree);*i;++i)
        if((*i)->data(0,Qt::UserRole).toString()=="guide-pointer-object"&&
            (*i)->data(0,Qt::UserRole+1).toString().isEmpty()){row=*i;break;}
    check(row!=nullptr,"Exact whole-Object Structure row exists");tree->scrollToItem(row);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();
    check(window.canvas->selected_object=="guide-pointer-object"&&window.canvas->selected_point.empty(),
        "Actual Structure pointer selects the whole Rectangle");
    const auto initial=snapshot(session);click(window,"tool-guide");
    auto* tool=window.findChild<QToolButton*>("tool-guide");
    check(window.canvas->guide_edit_mode()&&tool->isChecked()&&snapshot(session)==initial,
        "Actual Guide activation is source/native/revision/history neutral");
    if(keys_only){
        const auto selection=window.canvas->selections();
        const auto probe=[&](Qt::Key key,Qt::KeyboardModifiers modifiers){
            const auto before=snapshot(session);QTest::keyClick(window.canvas,key,modifiers);events();
            std::cout<<"Idle Guide arrow: authored_equal="<<(snapshot(session)==before)
                <<" active_guide="<<window.canvas->guide_edit_mode()<<'\n';
            check(snapshot(session)==before,"Idle Guide arrow cannot move unrelated artwork");
            check(window.canvas->guide_edit_mode()&&tool->isChecked()&&window.canvas->selections()==selection,
                "Idle Guide arrow preserves Tool and selection");
        };
        probe(Qt::Key_Right,Qt::NoModifier);probe(Qt::Key_Down,Qt::ShiftModifier);
        window.canvas->set_show_guides(false);events();probe(Qt::Key_Left,Qt::NoModifier);
        click(window,"tool-selection");
        const auto before=session.document();const auto revision=session.revision();const auto history=session.history().states.size();
        Session oracle(before);oracle.apply({TranslateObjects{{"guide-pointer-object"},1,0}},oracle.revision());
        window.canvas->setFocus();QTest::keyClick(window.canvas,Qt::Key_Right);events();
        check(session.document()==oracle.document()&&session.revision()==revision+1&&session.history().states.size()==history+1,
            "Explicit Selection Arrow still performs one canonical artwork translation");
        const auto moved=session.document();session.undo(session.revision());window.host.edited();events();
        check(session.document()==before,"Selection Arrow Undo restores complete source");
        session.redo(session.revision());window.host.edited();events();
        check(session.document()==moved,"Selection Arrow Redo restores complete source");
        std::cout<<"guide_key_isolation: "<<checks<<" checks passed\n";return;
    }
    const auto probe=[&](QPoint start,const char* label){
        check(window.canvas->rect().contains(start),"Guide regression pointer lies in Canvas");
        const auto before=snapshot(session);const auto selection=window.canvas->selections();
        const auto finish=start+QPoint(24,18);
        QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,start);events();
        QTest::mouseMove(window.canvas,finish);events();const bool preview=session.gesture_active();
        QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,finish);events();
        std::cout<<label<<": preview="<<preview<<" authored_equal="<<(snapshot(session)==before)
            <<" active_guide="<<window.canvas->guide_edit_mode()<<" selected_point="<<window.canvas->selected_point<<'\n';
        check(!preview&&!session.gesture_active()&&snapshot(session)==before,
            "Guide Tool cannot author ordinary point geometry or Object translation");
        check(window.canvas->guide_edit_mode()&&tool->isChecked()&&window.canvas->selections()==selection,
            "Guide Tool away from guides retains Tool and whole-Object selection");
    };
    const auto& source=*setup.document().objects.at("guide-pointer-object").source;
    probe(screen(320-source.parameters.at("width").literal/2,240-source.parameters.at("height").literal/2),
        "Guide Tool rectangle anchor drag");
    probe(screen(320,240),"Guide Tool rectangle body drag");
    window.canvas->set_show_guides(false);events();
    probe(screen(320,240),"Hidden-guide Tool rectangle body drag");
    window.canvas->set_show_guides(true);events();
    const auto before=session.document();const auto revision=session.revision();const auto history=session.history().states.size();
    const auto start=screen(120,100),finish=start+QPoint(24,0);
    auto expected_guide=before.compositions.front().guides.front();expected_guide.position+=24/window.canvas->zoom();
    Session oracle(before);oracle.apply({UpdateGuide{composition.id,expected_guide}},oracle.revision());
    QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,start);events();
    QTest::mouseMove(window.canvas,finish);events();
    check(session.document()==before&&session.gesture_active(),"Actual Guide drag previews without committing source");
    QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,finish);events();
    check(session.document()==oracle.document()&&session.revision()==revision+1&&session.history().states.size()==history+1,
        "Real Guide endpoint delta equals canonical UpdateGuide in one transaction");
    const auto moved=session.document();session.undo(session.revision());window.host.edited();events();
    check(session.document()==before,"One Undo restores full source after Guide move");
    session.redo(session.revision());window.host.edited();events();
    check(session.document()==moved,"One Redo restores complete Guide source");
    check(window.canvas->guide_edit_mode()&&tool->isChecked(),"Guide remains active after its canonical move/Undo/Redo");
    const auto file=scratch.filePath("guide-pointer.nect.json");window.host.save(file);window.host.open(file);events();
    check(session.document()==moved,"Same Window native reopen preserves full authored state");
    QSettings cold_preferences(scratch.filePath("cold-settings.ini"),QSettings::IniFormat);
    Window cold(scratch.filePath("cold-recovery"),std::make_unique<FolderLibrary>(cold_preferences),&cold_preferences);
    cold.setAttribute(Qt::WA_DontShowOnScreen);cold.show();cold.host.open(file);events();
    check(cold.host.session.document()==moved,"New Window native reopen preserves full authored state");
    std::cout<<"guide_pointer_isolation: "<<checks<<" checks passed\n";
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try {
        if(app.arguments().contains("--pointer-isolation-only")){pointer_isolation();return 0;}
        if(app.arguments().contains("--key-isolation-only")){pointer_isolation(true);return 0;}
        QTemporaryDir scratch;check(scratch.isValid(),"Owned test scratch exists");
        QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
        Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences));
        window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
        auto& session=window.host.session;
        auto document=empty_document("guide-rail-document","guide-rail-composition","guide-rail-artboard");
        auto& composition=document.compositions.front();
        composition.artboards.front().width=640;composition.artboards.front().height=480;
        composition.guides={{"guide-rail-x","Vertical","x",120}};
        session=Session(document);window.host.edited();window.canvas->fit_artboard();events();
        const auto before=snapshot(session);
        // The old setters let Guide intercept a newly selected Pen/Anchor.
        window.canvas->set_guide_edit_mode(true);click(window,"tool-pen");
        check(window.canvas->draw_mode()&&!window.canvas->guide_edit_mode(),"Pen activation leaves Guide mode exclusively");
        check(snapshot(session)==before,"Mode transition preserves full document/native/revision/history");
        click(window,"tool-guide");
        auto* guide=window.findChild<QToolButton*>("tool-guide");
        check(guide->isChecked()&&window.canvas->guide_edit_mode()&&!window.canvas->draw_mode(),"Guide Rail slot activates real exclusive Guide Edit");
        check(guide->accessibleName().contains("Guide")&&guide->toolTip().contains("Guide"),"Guide tool has a named keyboard-focus and hover path");
        const auto rail_position=guide->mapTo(&window,QPoint{});
        auto* structure=window.findChild<QDockWidget*>("structure");structure->hide();events();
        check(guide->isVisible()&&guide->mapTo(&window,QPoint{})==rail_position,"Structure collapse preserves Guide slot placement");
        structure->show();events();
        for(const auto* tool:{"tool-anchor","tool-text","tool-selection","tool-pen"}){
            click(window,"tool-guide");click(window,tool);
            check(!window.canvas->guide_edit_mode()&&!guide->isChecked(),"Another Rail tool leaves Guide mode and its checked state");
            check(snapshot(session)==before,"Every tool transition is authored/history neutral");
        }
        click(window,"tool-guide");QTest::keyClick(window.canvas,Qt::Key_Escape);events();
        check(!window.canvas->guide_edit_mode()&&window.findChild<QToolButton*>("tool-selection")->isChecked(),"Escape leaves idle Guide Edit for Selection");
        check(snapshot(session)==before,"Escape from Guide Edit does not author a change");
        click(window,"tool-guide");window.canvas->set_snap_enabled(false);events();
        const QPoint start(qRound(window.canvas->width()/2.0+(120-320)*window.canvas->zoom()),
            qRound(window.canvas->height()/2.0+(100-240)*window.canvas->zoom()));
        const QPoint finish=start+QPoint(24,0);
        const auto expected=120+24/window.canvas->zoom();
        QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(window.canvas,finish);events();
        QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,finish);events();
        check(std::abs(session.document().compositions.front().guides.front().position-expected)<1e-7&&session.revision()==1,
            "Real Guide-tool Canvas drag commits one canonical Guide move");
        const auto moved=session.document();session.undo(session.revision());window.host.edited();
        check(session.document()==document,"One Undo restores the entire original document");
        session.redo(session.revision());window.host.edited();check(session.document()==moved,"One Redo restores the exact Guide edit");
        const auto file=scratch.filePath("guide.nect.json");window.host.save(file);
        Host reopened(scratch.filePath("cold"));reopened.open(file);
        check(reopened.session.document()==moved,"Native Host reopen preserves the Guide edit and identity");
        if(const auto path=qEnvironmentVariable("NECT_GUIDE_CAPTURE");!path.isEmpty())window.grab().save(path);
        std::cout<<"rail_guide_tool_tests: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
