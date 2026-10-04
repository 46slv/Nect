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
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try {
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
