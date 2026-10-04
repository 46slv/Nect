#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
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
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
auto snapshot(Session& s){return std::tuple{s.document(),encode(s.document()),s.revision(),s.history()};}
Document fixture(){
    auto d=empty_document("gradient-rail-document","gradient-rail-composition","gradient-rail-artboard");
    d.compositions.front().artboards.front().width=640;
    d.compositions.front().artboards.front().height=480;
    d.compositions.front().guides={{"gradient-guide","Vertical","x",280}};
    Session s(d);
    auto fill=default_operation("gradient-fill","nect.paint.fill");
    Gradient g;g.id="gradient-source";g.enabled=true;
    g.start_x.literal=-40;g.start_y.literal=-20;g.end_x.literal=40;g.end_y.literal=-20;
    GradientStop a;a.id="gradient-first";a.rgba[0].literal=1;
    GradientStop b;b.id="gradient-last";b.offset.literal=1;b.rgba[2].literal=1;g.stops={a,b};
    s.apply({CreatePrimitive{"gradient-rail-composition",{},"gradient-object","Gradient object",
        default_primitive("gradient-shape","nect.shape.rectangle")},
        AddOperation{"gradient-object",fill,0},SetGradient{"gradient-object",fill.id,g},
        Set{{"gradient-object",{},"transform.tx"},320},Set{{"gradient-object",{},"transform.ty"},240},
        CreatePrimitive{"gradient-rail-composition",{},"plain-object","Plain object",
            default_primitive("plain-shape","nect.shape.rectangle")}},s.revision());
    return s.document();
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");
        QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
        Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings));
        window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();
        auto& session=window.host.session;const auto original=fixture();
        session=Session(original);window.host.edited();window.canvas->fit_artboard();QApplication::processEvents();
        auto* gradient=window.findChild<QToolButton*>("tool-gradient");
        check(gradient&&gradient->isVisible()&&!gradient->isEnabled()&&gradient->toolTip().contains("Select one"),
            "Fixed Gradient slot is visible with an explicit no-selection reason");
        const auto click=[&](const char* name){
            auto* button=window.findChild<QToolButton*>(name);
            check(button&&button->isVisible()&&button->isEnabled(),"Requested Rail tool is visible and enabled");
            QTest::mouseClick(button,Qt::LeftButton);QApplication::processEvents();
        };
        window.canvas->set_selection("gradient-object");QApplication::processEvents();
        const auto before=snapshot(session);
        check(gradient->isEnabled()&&gradient->accessibleName().contains("Gradient")&&
            gradient->focusPolicy()==Qt::StrongFocus,"Selected eligible Object enables a named, focusable Gradient tool");
        window.canvas->set_guide_edit_mode(true);
        window.canvas->set_gradient_edit("gradient-object","gradient-fill");
        std::cout<<"Guide-to-Gradient observed guide="<<window.canvas->guide_edit_mode()
            <<" operation="<<window.canvas->gradient_operation()<<'\n';
        check(!window.canvas->guide_edit_mode()&&window.canvas->gradient_operation()=="gradient-fill",
            "Gradient activation must leave Guide mode exclusively");
        check(snapshot(session)==before,"Activation is full document/native/revision/history neutral");
        check(gradient->isChecked(),"Inspector/Canvas gradient activation synchronizes the Rail");
        QApplication::processEvents();
        QPushButton* inspector=nullptr;
        for(auto* button:window.findChildren<QPushButton*>("gradient-handles-gradient-fill"))
            if(button->isVisible())inspector=button;
        check(inspector&&inspector->text()=="Finish gradient handles","Gradient callback also preserves Inspector synchronization");
        if(app.arguments().contains("--mode-conflict-only"))return 0;
        QTest::keyClick(window.canvas,Qt::Key_Escape);QApplication::processEvents();
        check(window.canvas->gradient_operation().empty()&&!gradient->isChecked()&&
            window.findChild<QToolButton*>("tool-selection")->isChecked(),"Idle Escape returns gradient handles to Selection");
        click("tool-gradient");click("tool-gradient");
        check(gradient->isChecked()&&window.canvas->gradient_operation()=="gradient-fill"&&
            window.canvas->selected_object=="gradient-object","One click activates the exact sole target; ordinary reselect keeps it active");
        check(snapshot(session)==before,"Click, reselect and Escape remain authored/native/history neutral");
        const auto position=gradient->mapTo(&window,QPoint{});
        auto* guide=window.findChild<QToolButton*>("tool-guide");
        check(gradient->mapTo(&window,QPoint{}).y()>guide->mapTo(&window,QPoint{}).y(),"Gradient is appended after existing Guide slot");
        auto* structure=window.findChild<QDockWidget*>("structure");structure->hide();QApplication::processEvents();
        check(gradient->isVisible()&&gradient->mapTo(&window,QPoint{})==position,"Structure collapse preserves Gradient slot position");
        structure->show();QApplication::processEvents();
        for(const auto* name:{"tool-pen","tool-text","tool-anchor","tool-guide","tool-selection"}){
            click("tool-gradient");click(name);
            check(window.canvas->gradient_operation().empty()&&!gradient->isChecked(),"Other Rail tools exclusively leave Gradient mode");
            check(snapshot(session)==before,"Every mode switch preserves the full document/native/revision/history");
        }
        click("tool-gradient");window.canvas->set_selection("plain-object");QApplication::processEvents();
        check(!gradient->isEnabled()&&window.canvas->gradient_operation().empty()&&gradient->toolTip().contains("no enabled Gradient"),
            "Selection change clears the exact handle target and exposes the disabled reason without creating a Gradient");
        window.canvas->set_selections({{"gradient-object",{}},{"plain-object",{}}});QApplication::processEvents();
        check(!gradient->isEnabled()&&gradient->toolTip().contains("one whole Object"),"Multiple Object selection never silently chooses a target");
        check(snapshot(session)==before,"Selection changes never author or retarget a Gradient source");
        window.canvas->set_selection("gradient-object");click("tool-gradient");window.canvas->set_snap_enabled(false);
        window.canvas->fit_artboard();QApplication::processEvents();
        if(const auto capture=qEnvironmentVariable("NECT_GRADIENT_CAPTURE");!capture.isEmpty())window.grab().save(capture);
        const QPoint start(qRound(window.canvas->width()/2.0-40*window.canvas->zoom()),
            qRound(window.canvas->height()/2.0-20*window.canvas->zoom()));
        const QPoint finish=start+QPoint(24,16);
        QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(window.canvas,finish);QApplication::processEvents();
        QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,finish);QApplication::processEvents();
        auto expected=original;auto& expected_gradient=*expected.objects.at("gradient-object").stack.front().gradient;
        expected_gradient.start_x.literal+=24/window.canvas->zoom();expected_gradient.start_y.literal+=16/window.canvas->zoom();
        const auto& actual_gradient=*session.document().objects.at("gradient-object").stack.front().gradient;
        std::cout<<"Gradient drag expected="<<expected_gradient.start_x.literal<<','<<expected_gradient.start_y.literal
            <<" actual="<<actual_gradient.start_x.literal<<','<<actual_gradient.start_y.literal<<" revision="<<session.revision()<<'\n';
        check(std::abs(actual_gradient.start_x.literal-expected_gradient.start_x.literal)<1e-7&&
            std::abs(actual_gradient.start_y.literal-expected_gradient.start_y.literal)<1e-7&&session.revision()==1,
            "Real Canvas Gradient-start drag commits the two expected coordinates once");
        expected_gradient.start_x.literal=actual_gradient.start_x.literal;
        expected_gradient.start_y.literal=actual_gradient.start_y.literal;
        check(session.document()==expected,"Gradient drag preserves every other authored field and source identity exactly");
        session.undo(session.revision());window.host.edited();check(session.document()==original,"One Undo restores the complete original Document");
        session.redo(session.revision());window.host.edited();check(session.document()==expected,"One Redo restores the complete Gradient edit");
        const auto file=scratch.filePath("gradient.nect.json");window.host.save(file);
        Host reopened(scratch.filePath("cold"));reopened.open(file);
        check(reopened.session.document()==expected,"Native Host reopen preserves complete Gradient/source/operation/stop identity");
        QApplication::processEvents();
        if(const auto capture=qEnvironmentVariable("NECT_GRADIENT_CAPTURE");!capture.isEmpty())
            check(window.grab().save(capture),"Rendered editable Window capture saved");
        const auto old_session=window.host.session_id;window.host.open(file);QApplication::processEvents();
        check(window.host.session_id!=old_session&&window.canvas->gradient_operation().empty()&&!gradient->isChecked(),
            "Native load with identical Object/operation IDs never carries an old Session handle target");
        const auto loaded=snapshot(session);
        click("tool-gradient");QTest::keyClick(window.canvas,Qt::Key_Escape);QApplication::processEvents();
        check(snapshot(session)==loaded,"Reopened activation/Escape preserves native/document/revision/history");
        auto bypassed=*session.document().objects.at("gradient-object").stack.front().gradient;bypassed.enabled=false;
        session.apply({SetGradient{"gradient-object","gradient-fill",bypassed}},session.revision());window.host.edited();
        check(!gradient->isEnabled(),"Retained disabled Gradient is not silently enabled by the Rail");
        const auto enabled_ref=gradient_ref("gradient-object","gradient-fill","gradient-source","enabled");
        session.apply({SetGradientEnabledExpression{enabled_ref,{"true",1},false}},session.revision());window.host.edited();
        check(gradient->isEnabled(),"Eligibility follows the canonical evaluated enabled expression rather than the retained literal");
        const auto driven=snapshot(session);click("tool-gradient");
        check(snapshot(session)==driven,"Activation preserves the Gradient enabled source/expression");
        QTest::keyClick(window.canvas,Qt::Key_Escape);QApplication::processEvents();
        auto stroke=default_operation("gradient-stroke","nect.paint.stroke");
        auto second=bypassed;second.id="second-gradient";second.enabled=true;second.stops[0].id="second-first";second.stops[1].id="second-last";
        session.apply({AddOperation{"gradient-object",stroke,1},SetGradient{"gradient-object",stroke.id,second}},session.revision());window.host.edited();
        const auto multiple=snapshot(session);click("tool-gradient");
        auto* menu=window.findChild<QMenu*>("gradient-tool-targets");
        check(menu&&menu->isVisible()&&menu->actions().size()==2&&window.canvas->gradient_operation().empty(),
            "Multiple eligible paints expose a choice and activate no implicit first candidate");
        auto* target=window.findChild<QAction*>("gradient-tool-target-gradient-stroke");
        check(target&&target->text().contains("Stroke")&&target->text().contains("gradient-stroke"),"Choice labels identify exact paint operation");
        QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(target).center());QApplication::processEvents();
        check(gradient->isChecked()&&window.canvas->gradient_operation()=="gradient-stroke"&&
            snapshot(session)==multiple,"Actual menu click activates only the chosen operation without authoring");
        click("tool-gradient");target=window.findChild<QAction*>("gradient-tool-target-gradient-fill");
        window.canvas->set_selection("plain-object");const auto stale_selection=snapshot(session);
        target->trigger();menu->hide();QApplication::processEvents();
        check(window.canvas->gradient_operation().empty()&&window.canvas->selected_object=="plain-object"&&
            snapshot(session)==stale_selection&&window.statusBar()->currentMessage().contains("target changed"),
            "Stale menu choice refuses after selection changes and never retargets");
        window.canvas->set_selection("gradient-object");click("tool-gradient");
        target=window.findChild<QAction*>("gradient-tool-target-gradient-fill");
        session.apply({Set{{"plain-object",{},"transform.tx"},25}},session.revision());window.host.edited();const auto stale_revision=snapshot(session);
        target->trigger();menu->hide();QApplication::processEvents();
        check(window.canvas->gradient_operation().empty()&&snapshot(session)==stale_revision,"Stale menu choice refuses after an intervening revision");
        window.canvas->set_selection("gradient-object");click("tool-gradient");
        target=window.findChild<QAction*>("gradient-tool-target-gradient-fill");
        window.host.save(file);window.host.open(file);const auto stale_load=snapshot(session);
        target->trigger();menu->hide();QApplication::processEvents();
        check(window.canvas->gradient_operation().empty()&&snapshot(session)==stale_load,"Stale menu choice refuses after same-ID native Session reopen");
        std::cout<<"rail_gradient_tool_tests: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
