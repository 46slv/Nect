#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCursor>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QWheelEvent>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
auto snapshot(Session& s){return std::tuple{s.document(),encode(s.document()),s.revision(),s.history()};}
void show(Window& w){w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1100,750);w.show();events();}
void click(Window& w,const char* name){
    auto* button=w.findChild<QToolButton*>(name);
    check(button&&button->isVisible()&&button->isEnabled(),"Real requested Rail tool is visible and enabled");
    QTest::mouseClick(button,Qt::LeftButton);events();
}
Document fixture(){
    auto d=empty_document("zoom-document","zoom-composition","zoom-artboard");
    d.compositions.front().artboards.front().width=640;
    d.compositions.front().artboards.front().height=480;
    d.compositions.front().guides={{"zoom-guide","Vertical","x",120}};
    Session s(d);auto fill=default_operation("zoom-fill","nect.paint.fill");
    Gradient g;g.id="zoom-gradient";g.enabled=true;g.end_x.literal=40;
    GradientStop a;a.id="zoom-first";a.rgba[0].literal=1;a.rgba[1].literal=0;a.rgba[2].literal=0;
    GradientStop b=a;b.id="zoom-last";b.offset.literal=1;g.stops={a,b};
    s.apply({CreatePrimitive{"zoom-composition",{},"zoom-object","Zoom fixture",default_primitive("zoom-shape","nect.shape.rectangle")},
        AddOperation{"zoom-object",fill,0},SetGradient{"zoom-object",fill.id,g},
        Set{{"zoom-object",{},"transform.tx"},320},Set{{"zoom-object",{},"transform.ty"},240},
        CreatePrimitive{"zoom-composition",{},"zoom-circle","Circle",default_primitive("zoom-circle-source","nect.shape.circle")}},s.revision());
    return s.document();
}
// Recover screen coordinates from actual editable artwork, not a test-only view API.
struct Ink {QPointF center;double width,height;};
struct AltProbe : QObject {
    int presses=0,overrides=0,focus_out=0;
    bool eventFilter(QObject*,QEvent* event) override {
        if(event->type()==QEvent::FocusOut)++focus_out;
        if(event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Alt)++presses;
        if(event->type()==QEvent::ShortcutOverride&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Alt)++overrides;
        return false;
    }
};
Ink ink(Canvas& canvas){
    const auto image=canvas.grab().toImage();double xsum=0,ysum=0;int count=0;
    int left=image.width(),top=image.height(),right=0,bottom=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
        const auto c=image.pixelColor(x,y);
        if(c.red()>180&&c.green()<80&&c.blue()<80){
            xsum+=x;ysum+=y;++count;left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
        }
    }
    check(count>100,"Rendered fixture has observable red artwork");const auto dpr=image.devicePixelRatio();
    return {QPointF(xsum/count,ysum/count)/dpr,(right-left+1)/dpr,(bottom-top+1)/dpr};
}
void anchored(Window& w,QPoint pointer,double factor,const std::function<void()>& input){
    const auto before=ink(*w.canvas);const auto zoom=w.canvas->zoom();
    const auto authored=snapshot(w.host.session);const auto selection=w.canvas->selections();
    input();events();const auto after=ink(*w.canvas);const auto actual=w.canvas->zoom();
    const auto expected=QPointF(pointer)+(before.center-QPointF(pointer))*factor;
    const auto world_before=(QPointF(pointer)-before.center)/zoom;
    const auto world_after=(QPointF(pointer)-after.center)/actual;
    std::cout<<"zoom="<<zoom<<" -> "<<actual<<" pointer="<<pointer.x()<<','<<pointer.y()
        <<" rendered center="<<after.center.x()<<','<<after.center.y()<<" expected="<<expected.x()<<','<<expected.y()
        <<" world error="<<QLineF(world_before,world_after).length()<<'\n';
    check(std::abs(actual-zoom*factor)<1e-10,"Input changes actual viewport zoom by the expected factor");
    check(QLineF(after.center,expected).length()<0.85&&QLineF(world_before,world_after).length()<1.0,
        "Rendered artwork proves the world point under the pointer stays fixed");
    check(std::abs(after.width-before.width*factor)<2&&std::abs(after.height-before.height*factor)<2,
        "Rendered artwork extent scales with actual viewport zoom");
    check(snapshot(w.host.session)==authored&&w.canvas->selections()==selection&&!w.host.session.gesture_active(),
        "Zoom preserves complete Document/native/revision/history/selection and opens no authored gesture");
    auto* readout=w.findChild<QDoubleSpinBox*>("canvas-zoom-percent");
    check(readout&&std::abs(readout->value()-std::round(actual*100))<0.01,"Existing Utility zoom readout follows pointer zoom");
}
void exclusive(Window& w){
    const auto* c=w.canvas;
    const int modes=c->zoom_mode()+c->hand_mode()+c->draw_mode()+c->text_mode()+c->anchor_edit()+
        c->guide_edit_mode()+c->circle_source_edit()+!c->gradient_operation().empty();
    int checked=0;
    for(const auto* name:{"tool-selection","tool-pen","tool-text","tool-anchor","tool-guide","tool-gradient","tool-hand","tool-zoom"})
        checked+=w.findChild<QToolButton*>(name)->isChecked();
    check(modes==1&&checked==1,"Exactly one Canvas mode and one Rail slot are active");
}
void pan(Window& w,Qt::MouseButton button,QPoint delta){
    const auto before=ink(*w.canvas);const auto zoom=w.canvas->zoom();const auto authored=snapshot(w.host.session);
    const auto selection=w.canvas->selections();const QPoint start(w.canvas->width()/2,w.canvas->height()/2);
    QTest::mousePress(w.canvas,button,Qt::NoModifier,start);events();
    check(w.canvas->cursor().shape()==Qt::ClosedHandCursor,"Temporary pan press uses closed-hand cursor");
    QTest::mouseMove(w.canvas,start+delta);QTest::mouseRelease(w.canvas,button,Qt::NoModifier,start+delta);events();
    check(QLineF(ink(*w.canvas).center,before.center+delta).length()<0.85,"Temporary pan translates actual rendered artwork");
    check(w.canvas->zoom()==zoom&&snapshot(w.host.session)==authored&&w.canvas->selections()==selection,
        "Temporary pan preserves zoom and complete authored/selection state");
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try {
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");
        QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
        preferences.setValue("unrelated","preserve");preferences.setValue("workspace/tools/textCreationDirection","vertical");
        Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);show(w);
        auto& s=w.host.session;const auto original=fixture();s=Session(original);w.host.edited();events();
        s.apply({Set{{"zoom-object",{},"transform.tx"},328}},s.revision());w.host.edited();events();
        w.canvas->set_selection("zoom-object");w.canvas->fit_artboard();events();
        const auto before=snapshot(s);const auto selection=w.canvas->selections();
        auto* zoom=w.findChild<QToolButton*>("tool-zoom");auto* hand=w.findChild<QToolButton*>("tool-hand");
        check(zoom&&zoom->isVisible()&&zoom->isEnabled()&&zoom->accessibleName().startsWith("Zoom")&&
            zoom->focusPolicy()==Qt::StrongFocus&&zoom->toolTip().contains("Alt-click"),"Zoom is a visible named focusable Tool");
        check(zoom->mapTo(&w,QPoint{}).y()>hand->mapTo(&w,QPoint{}).y(),"Zoom appends after Hand without reordering slots");
        const auto position=zoom->mapTo(&w,QPoint{});
        click(w,"tool-gradient");click(w,"tool-zoom");exclusive(w);
        check(w.canvas->zoom_mode()&&zoom->isChecked()&&w.canvas->cursor().shape()==Qt::BitmapCursor,
            "One Rail click activates persistent Zoom and magnifier cursor");
        check(snapshot(s)==before&&w.canvas->selections()==selection,"Tool activation preserves full authored/history/selection state");
        click(w,"tool-zoom");check(w.canvas->zoom_mode(),"Reselect keeps Zoom active");
        auto* structure=w.findChild<QDockWidget*>("structure");structure->hide();events();
        check(zoom->isVisible()&&zoom->mapTo(&w,QPoint{})==position,"Panel collapse preserves Zoom spatial position");
        structure->show();events();w.canvas->fit_artboard();events();
        const auto pointer=(ink(*w.canvas).center+QPointF(23,-17)).toPoint();
        anchored(w,pointer,1.25,[&]{QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,pointer);});
        anchored(w,pointer,1.0/1.25,[&]{QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::AltModifier,pointer);});
        anchored(w,pointer,std::pow(1.0015,120),[&]{
            QWheelEvent wheel(pointer,w.canvas->mapToGlobal(pointer),{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
            QApplication::sendEvent(w.canvas,&wheel);
        });
        anchored(w,pointer,std::pow(1.0015,-120),[&]{
            QWheelEvent wheel(pointer,w.canvas->mapToGlobal(pointer),QPoint(0,-120),{},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
            QApplication::sendEvent(w.canvas,&wheel);
        });
        // The Utility setter retains its existing center-anchored behavior.
        const QPoint center(w.canvas->width()/2,w.canvas->height()/2);
        anchored(w,center,1.1,[&]{w.canvas->set_zoom(w.canvas->zoom()*1.1);});
        QApplication::setActiveWindow(&w);w.canvas->setFocus();events();
        check(w.isActiveWindow()&&w.canvas->hasFocus(),"Alt fixture has actual active Window and Canvas focus");
        QTest::mouseMove(w.canvas,pointer);events();const auto cursor_before_release=w.canvas->cursor().pixmap().toImage();
        QTest::keyRelease(w.canvas,Qt::Key_Alt);events();
        const auto cursor_in=w.canvas->cursor().pixmap().toImage();
        AltProbe alt_probe;w.canvas->installEventFilter(&alt_probe);
        QTest::keyPress(w.canvas,Qt::Key_Alt);events();const auto cursor_out=w.canvas->cursor().pixmap().toImage();
        std::cout<<"Alt cursor in-null="<<cursor_in.isNull()<<" out-null="<<cursor_out.isNull()
            <<" equal="<<(cursor_in==cursor_out)<<" before-release-equals-in="<<(cursor_before_release==cursor_in)
            <<" before-release-equals-out="<<(cursor_before_release==cursor_out)<<" active="<<w.isActiveWindow()<<" focus="<<w.canvas->hasFocus()
            <<" press="<<alt_probe.presses<<" override="<<alt_probe.overrides<<" focusout="<<alt_probe.focus_out<<'\n';
        check(!cursor_in.isNull()&&cursor_in!=cursor_out,"Alt visibly changes plus magnifier to minus");
        QTest::keyRelease(w.canvas,Qt::Key_Alt);events();
        check(w.canvas->cursor().pixmap().toImage()==cursor_in,"Alt release restores plus magnifier");
        const auto dbl_zoom=w.canvas->zoom();QTest::mouseDClick(w.canvas,Qt::LeftButton,Qt::NoModifier,pointer);events();
        check(w.canvas->zoom()>dbl_zoom&&snapshot(s)==before&&w.canvas->selections()==selection,
            "Zoom double-click zooms without selecting or drilling into artwork");
        const auto right_zoom=w.canvas->zoom();QTest::mouseClick(w.canvas,Qt::RightButton,Qt::NoModifier,pointer);events();
        check(w.canvas->zoom()==right_zoom&&snapshot(s)==before,"Right-click does not zoom or author");
        w.canvas->set_zoom(63);QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,pointer);events();
        check(w.canvas->zoom()==64,"Pointer zoom respects upper 6400 percent bound");
        QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,pointer);events();check(w.canvas->zoom()==64,"Repeated upper-bound zoom stays clamped");
        w.canvas->set_zoom(0.021);QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::AltModifier,pointer);events();
        check(w.canvas->zoom()==0.02,"Pointer zoom respects lower 2 percent bound");
        QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::AltModifier,pointer);events();check(w.canvas->zoom()==0.02,"Repeated lower-bound zoom stays clamped");
        w.canvas->fit_artboard();events();
        w.canvas->set_selection("zoom-circle");w.canvas->set_selection({});events();
        check(w.canvas->zoom_mode()&&zoom->isChecked(),"Selection changes and clearing never silently switch Zoom");
        w.canvas->set_selection("zoom-object");events();
        for(const auto* tool:{"tool-pen","tool-text","tool-anchor","tool-guide","tool-gradient","tool-hand"}){
            click(w,"tool-zoom");click(w,tool);exclusive(w);
            check(!w.canvas->zoom_mode()&&!zoom->isChecked(),"Other Tool leaves Zoom exclusively");
            click(w,"tool-zoom");exclusive(w);check(snapshot(s)==before,"Both transition directions preserve full authored/history state");
        }
        click(w,"tool-selection");check(!w.canvas->zoom_mode()&&!zoom->isChecked(),"Selection leaves Zoom");
        click(w,"tool-zoom");w.canvas->set_selection("zoom-circle");w.canvas->set_circle_source_edit(true);events();
        check(!w.canvas->zoom_mode()&&w.canvas->circle_source_edit(),"Existing Circle Inspector mode leaves Zoom");
        click(w,"tool-zoom");exclusive(w);w.canvas->set_selection("zoom-object");events();
        QTest::keyClick(w.canvas,Qt::Key_Escape);events();
        check(!w.canvas->zoom_mode()&&w.findChild<QToolButton*>("tool-selection")->isChecked()&&w.canvas->selected_object=="zoom-object",
            "Idle Escape exits Zoom without clearing selection");
        click(w,"tool-zoom");w.canvas->fit_artboard();events();
        QTest::keyPress(w.canvas,Qt::Key_Space);events();check(w.canvas->cursor().shape()==Qt::OpenHandCursor,"Space temporarily overrides Zoom cursor");
        pan(w,Qt::LeftButton,QPoint(24,16));QTest::keyRelease(w.canvas,Qt::Key_Space);events();
        check(w.canvas->zoom_mode()&&zoom->isChecked()&&w.canvas->cursor().shape()==Qt::BitmapCursor,"Space release returns to persistent Zoom");
        pan(w,Qt::MiddleButton,QPoint(-24,-16));
        check(w.canvas->zoom_mode()&&w.canvas->cursor().shape()==Qt::BitmapCursor,"Middle-button release returns to Zoom cursor");
        QTest::mousePress(w.canvas,Qt::MiddleButton,Qt::NoModifier,center);QTest::mouseMove(w.canvas,center+QPoint(8,8));events();
        QTest::keyClick(w.canvas,Qt::Key_Escape);QTest::mouseRelease(w.canvas,Qt::MiddleButton,Qt::NoModifier,center);events();
        check(w.canvas->zoom_mode()&&!s.gesture_active(),"Escape during temporary pan retains Zoom");
        QFocusEvent lost(QEvent::FocusOut);QApplication::sendEvent(w.canvas,&lost);events();
        check(w.canvas->zoom_mode()&&snapshot(s)==before,"Focus loss preserves Zoom and authored/history state");
        QApplication::setActiveWindow(&w);w.canvas->setFocus();events();
        check(w.isActiveWindow()&&w.canvas->hasFocus(),"Shortcut fixture has actual active Window and Canvas focus");
        QTest::keyClick(w.canvas,Qt::Key_P);events();check(w.canvas->draw_mode()&&!w.canvas->zoom_mode(),"Existing P shortcut leaves Zoom for Pen");
        click(w,"tool-zoom");QTest::keyClick(w.canvas,Qt::Key_G);events();check(w.canvas->draw_mode()&&!w.canvas->zoom_mode(),"Existing G shortcut leaves Zoom for Pen");
        click(w,"tool-zoom");check(w.statusBar()->currentMessage().isEmpty(),"Zoom activation clears the previous Pen hint");
        check(preferences.value("workspace/tools/textCreationDirection")=="vertical"&&preferences.value("unrelated")=="preserve"&&
            w.findChild<QToolButton*>("tool-text")->accessibleName().startsWith("Vertical"),"Zoom preserves Text preference and unrelated settings");
        const auto authored=s.document();s.undo(s.revision());w.host.edited();events();check(s.document()==original&&w.canvas->zoom_mode(),"Undo restores complete authored Document while retaining Zoom");
        s.redo(s.revision());w.host.edited();events();check(s.document()==authored,"Redo restores complete authored Document");
        const auto file=scratch.filePath("zoom.nect.json");w.host.save(file);events();const auto saved=encode(s.document());
        w.host.open(file);events();check(encode(s.document())==saved&&w.canvas->zoom_mode()&&zoom->isChecked(),"Same-Window native reopen preserves source and persistent Zoom");
        const auto loaded=snapshot(s);w.canvas->fit_artboard();events();const auto loaded_pointer=(ink(*w.canvas).center+QPointF(-19,15)).toPoint();
        anchored(w,loaded_pointer,1.25,[&]{QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,loaded_pointer);});
        check(snapshot(s)==loaded,"Reopened Session pointer zoom is fully authored/history neutral");
        Window reopened(scratch.filePath("cold"),std::make_unique<FolderLibrary>(preferences),&preferences);show(reopened);
        reopened.host.open(file);events();check(encode(reopened.host.session.document())==saved&&!reopened.canvas->zoom_mode()&&
            reopened.findChild<QToolButton*>("tool-selection")->isChecked(),"New Window native reopen preserves source and starts Selection");
        if(const auto capture=qEnvironmentVariable("NECT_ZOOM_CAPTURE");!capture.isEmpty()) {
            check(w.grab().save(capture),"Editable source rendered Window capture saved");
            check(cursor_in.save(capture+"-cursor-in.png")&&cursor_out.save(capture+"-cursor-out.png"),"Actual plus/minus cursor images saved");
        }
        std::cout<<"rail_zoom_tool_tests: "<<checks<<" checks passed; DPR="<<w.devicePixelRatioF()<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
