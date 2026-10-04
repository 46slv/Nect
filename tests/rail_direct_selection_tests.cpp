#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFocusEvent>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QWheelEvent>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
auto snapshot(Session& s){return std::tuple{s.document(),encode(s.document()),s.revision(),s.history()};}
void show(Window& w){w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1100,750);w.show();w.activateWindow();events();}
void click(Window& w,const char* name){
    auto* b=w.findChild<QToolButton*>(name);check(b&&b->isVisible()&&b->isEnabled(),"Real Rail tool reachable");
    QTest::mouseClick(b,Qt::LeftButton);events();
}
Document fixture(){
    auto d=empty_document("direct-document","composition","board");
    auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;
    Object path;path.id="path";path.name="Editable curve";
    Point a;a.id="stable-first";a.x.literal=120;a.y.literal=160;
    a.in_angle.literal=180;a.in_length.literal=50;a.out_length.literal=60;
    Point b;b.id="stable-second";b.x.literal=360;b.y.literal=240;b.in_angle.literal=180;b.in_length.literal=60;
    path.contours={{"stable-contour",false,{a,b}}};
    auto stroke=default_operation("curve-stroke","nect.paint.stroke");stroke.parameters.at("width").literal=3;
    Gradient g;g.id="gradient";g.enabled=true;g.end_x.literal=200;
    GradientStop first;first.id="first-stop";first.rgba[0].literal=1;first.rgba[1].literal=0;first.rgba[2].literal=0;
    GradientStop last=first;last.id="last-stop";last.offset.literal=1;g.stops={first,last};stroke.gradient=g;path.stack={stroke};
    Object other=path;other.id="other";other.name="Unrelated curve";other.stack.clear();
    other.contours.front().id="other-contour";
    other.contours.front().points.front().id="other-first";other.contours.front().points.back().id="other-second";
    other.contours.front().points.front().x.literal=450;other.contours.front().points.front().y.literal=100;
    other.contours.front().points.back().x.literal=550;other.contours.front().points.back().y.literal=120;
    d.objects.emplace(path.id,path);d.objects.emplace(other.id,other);d.compositions.front().roots={"path","other"};
    Session s(d);s.apply({CreatePrimitive{"composition",{},"circle","Retained Circle",default_primitive("circle-source","nect.shape.circle")},
        Set{{"circle",{},"generator.center_x"},480},Set{{"circle",{},"generator.center_y"},350},Set{{"circle",{},"generator.radius"},30}},s.revision());
    return s.document();
}
QPoint screen(Window& w,double x,double y){
    const auto b=evaluate_artboard(w.host.session.document().compositions.front(),"board");
    return {qRound(w.canvas->width()/2.0+(x-b.x-b.width/2)*w.canvas->zoom()),
        qRound(w.canvas->height()/2.0+(y-b.y-b.height/2)*w.canvas->zoom())};
}
void reset(Window& w,const Document& d){w.host.session=Session(d);w.host.edited();events();w.canvas->set_selection({});w.canvas->fit_artboard();w.canvas->set_snap_enabled(false);events();}
void drag(Window& w,QPoint from,QPoint to,Qt::KeyboardModifiers mods=Qt::NoModifier){
    QTest::mousePress(w.canvas,Qt::LeftButton,mods,from);events();QTest::mouseMove(w.canvas,to);events();
    QTest::mouseRelease(w.canvas,Qt::LeftButton,mods,to);events();
}
void undo_redo(Window& w,const Document& before,Document after){
    auto& s=w.host.session;s.undo(s.revision());w.host.edited();events();check(s.document()==before,"One Undo restores complete unrelated state and source");
    s.redo(s.revision());w.host.edited();events();check(s.document()==after,"One Redo restores complete authored edit");
}
void complete_edit(Session& actual,Session& expected,std::initializer_list<Ref> changed,const char* why){
    const auto observed=evaluate(actual.document()),wanted=evaluate(expected.document());std::vector<Command> roundoff;
    for(const auto& ref:changed){
        if(std::abs(observed.at(ref)-wanted.at(ref))>=1e-7)
            std::cerr<<"Coordinate mismatch: "<<ref.object<<'/'<<ref.point<<'/'<<ref.field<<" actual="<<observed.at(ref)<<" expected="<<wanted.at(ref)<<'\n';
        check(std::abs(observed.at(ref)-wanted.at(ref))<1e-7,"Pointer inverse mapping matches independent coordinate/handle oracle");
        // Qt's inverse mapping can differ by a final floating-point bit. Keep
        // the numeric oracle above, then compare every other authored field
        // exactly, with only these explicitly permitted Scalars normalized.
        roundoff.push_back(Set{ref,observed.at(ref)});
    }
    expected.apply(roundoff,expected.revision());check(actual.document()==expected.document(),why);
}
void exclusive(Window& w){
    int modes=w.canvas->direct_selection_mode()+w.canvas->zoom_mode()+w.canvas->hand_mode()+w.canvas->draw_mode()+w.canvas->text_mode()+
        w.canvas->anchor_edit()+w.canvas->guide_edit_mode()+!w.canvas->gradient_operation().empty()+w.canvas->circle_source_edit();
    int active=0;for(const auto* name:{"tool-selection","tool-pen","tool-text","tool-anchor","tool-guide","tool-gradient","tool-hand","tool-zoom","tool-direct-selection"})
        active+=w.findChild<QToolButton*>(name)->isChecked();
    check(modes==1&&active==1,"Exactly one Canvas Tool and one Rail slot active");
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try {
        QTemporaryDir scratch;check(scratch.isValid(),"Owned temporary directory");
        QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
        preferences.setValue("unrelated","preserved");preferences.setValue("workspace/tools/textCreationDirection","vertical");
        Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);show(w);
        const auto original=fixture();reset(w,original);auto& s=w.host.session;
        auto* direct=w.findChild<QToolButton*>("tool-direct-selection");
        check(direct&&direct->accessibleName().startsWith("Direct Selection")&&direct->focusPolicy()==Qt::StrongFocus&&
            direct->toolTip().contains("points"),"Named focusable visible Direct Selection");
        check(direct->mapTo(&w,QPoint{}).y()>w.findChild<QToolButton*>("tool-zoom")->mapTo(&w,QPoint{}).y(),"Append preserves existing Rail positions/order");
        const auto activation=snapshot(s);click(w,"tool-direct-selection");exclusive(w);
        check(w.canvas->direct_selection_mode()&&direct->isChecked()&&w.canvas->cursor().shape()==Qt::CrossCursor&&snapshot(s)==activation,
            "One Rail click activates visible persistent mode without authoring");
        click(w,"tool-direct-selection");check(snapshot(s)==activation&&w.canvas->direct_selection_mode(),"Reselect is neutral and persistent");
        const auto pos=direct->mapTo(&w,QPoint{});auto* structure=w.findChild<QDockWidget*>("structure");structure->hide();events();
        check(direct->isVisible()&&direct->mapTo(&w,QPoint{})==pos,"Structure collapse preserves slot");structure->show();events();w.canvas->fit_artboard();events();

        // The point starts unselected. The actual pointer must discover its stable ID.
        auto from=screen(w,120,160),to=from+QPoint(24,16);const double z=w.canvas->zoom();
        QTest::mousePress(w.canvas,Qt::LeftButton,Qt::NoModifier,from);events();
        check(w.canvas->selected_object=="path"&&w.canvas->selected_point=="stable-first"&&s.gesture_active(),"Unselected anchor reaches canonical Session gesture");
        QTest::mouseMove(w.canvas,to);events();check(s.document()==original&&s.revision()==0,"Preview keeps committed document and revision unchanged");
        QTest::mouseRelease(w.canvas,Qt::LeftButton,Qt::NoModifier,to);events();
        Session expected(original);expected.apply({Set{{"path","stable-first","x"},120+24/z},Set{{"path","stable-first","y"},160+16/z}},expected.revision());
        std::cout.precision(17);std::cout<<"Point actual="<<evaluate(s.document()).at({"path","stable-first","x"})<<','<<evaluate(s.document()).at({"path","stable-first","y"})
            <<" expected="<<120+24/z<<','<<160+16/z<<" zoom="<<z<<" revision="<<s.revision()<<" gesture="<<s.gesture_active()<<'\n';
        complete_edit(s,expected,{{"path","stable-first","x"},{"path","stable-first","y"}},"Actual point edit changes exactly stable-ID coordinates");
        check(s.revision()==1&&!s.gesture_active(),"One point drag commits once");
        undo_redo(w,original,expected.document());check(w.canvas->direct_selection_mode()&&direct->isChecked(),"Undo/Redo retains Tool");
        reset(w,original);QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,screen(w,120,160));QTest::keyClick(w.canvas,Qt::Key_Right);events();
        Session nudge(original);nudge.apply({Set{{"path","stable-first","x"},121}},nudge.revision());
        check(s.document()==nudge.document(),"Authored Arrow nudge remains exact and changes only the canonical point Scalar");undo_redo(w,original,nudge.document());
        reset(w,original);
        // Incoming/outgoing handles use the existing angle/length command contract.
        QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,screen(w,120,160));events();
        check(w.canvas->selected_point=="stable-first"&&s.document()==original,"Point click alone is authored-neutral");
        from=screen(w,180,160);to=from+QPoint(16,24);drag(w,from,to);
        const double dx=16/w.canvas->zoom(),dy=24/w.canvas->zoom();Session handles(original);
        handles.apply({Set{{"path","stable-first","out.angle"},std::atan2(dy,60+dx)*180/std::numbers::pi},
            Set{{"path","stable-first","out.length"},std::hypot(60+dx,dy)}},handles.revision());
        complete_edit(s,handles,{{"path","stable-first","out.angle"},{"path","stable-first","out.length"}},"Outgoing handle changes only its two canonical Scalars");undo_redo(w,original,handles.document());
        reset(w,original);w.canvas->set_selection("path","stable-first");events();from=screen(w,70,160);to=from+QPoint(-16,-24);drag(w,from,to);
        Session incoming(original);incoming.apply({Set{{"path","stable-first","in.angle"},std::atan2(-dy,-50-dx)*180/std::numbers::pi},
            Set{{"path","stable-first","in.length"},std::hypot(-50-dx,-dy)}},incoming.revision());
        complete_edit(s,incoming,{{"path","stable-first","in.angle"},{"path","stable-first","in.length"}},"Incoming handle remains a distinct stable-ID edit");undo_redo(w,original,incoming.document());
        reset(w,original);from=screen(w,120,160);to=from+QPoint(24,16);drag(w,from,to,Qt::AltModifier);
        QTest::keyRelease(w.canvas,Qt::Key_Alt);events();
        Session symmetric(original);const double symmetricAngle=std::atan2(16/z,24/z)*180/std::numbers::pi;
        symmetric.apply({Set{{"path","stable-first","out.angle"},symmetricAngle},Set{{"path","stable-first","out.length"},std::hypot(24/z,16/z)},
            Set{{"path","stable-first","in.angle"},symmetricAngle+180},Set{{"path","stable-first","in.length"},std::hypot(24/z,16/z)}},symmetric.revision());
        complete_edit(s,symmetric,{{"path","stable-first","out.angle"},{"path","stable-first","out.length"},{"path","stable-first","in.angle"},{"path","stable-first","in.length"}},"Alt changes only canonical symmetric handle Scalars");
        check(evaluate(s.document()).at({"path","stable-first","in.length"})==evaluate(s.document()).at({"path","stable-first","out.length"})&&
            s.document().objects.at("path").contours.front().points.front().x.literal==120,"Alt anchor drag preserves symmetric-handle semantics");
        undo_redo(w,original,s.document());

        reset(w,original);QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,screen(w,120,160));events();
        QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::ShiftModifier,screen(w,450,100));events();
        check(w.canvas->selections().size()==2&&s.document()==original,"Shift extends canonical point selection across same-scope Objects");
        from=screen(w,120,160);to=from+QPoint(20,10);drag(w,from,to);
        Session multi(original);multi.apply({Set{{"path","stable-first","x"},120+20/z},Set{{"path","stable-first","y"},160+10/z},
            Set{{"other","other-first","x"},450+20/z},Set{{"other","other-first","y"},100+10/z}},multi.revision());
        complete_edit(s,multi,{{"path","stable-first","x"},{"path","stable-first","y"},{"other","other-first","x"},{"other","other-first","y"}},"Multi-point drag resolves exact owner+point IDs without retargeting");undo_redo(w,original,multi.document());
        reset(w,original);drag(w,screen(w,100,140),screen(w,140,180));
        check(w.canvas->selections()==std::vector<Canvas::Selection>{{"path","stable-first"}}&&snapshot(s)==activation,"Direct marquee selects contained points, never Object bounds or authors state");
        drag(w,screen(w,430,80),screen(w,470,115),Qt::ShiftModifier);
        check(w.canvas->selections().size()==2&&snapshot(s)==activation,"Shift marquee extends points without authored mutation");

        // Preview cancellation and mode switches restore complete history/native state.
        for(int kind=0;kind<3;++kind){
            reset(w,original);click(w,"tool-direct-selection");from=screen(w,120,160);
            QTest::mousePress(w.canvas,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(w.canvas,from+QPoint(20,10));events();
            check(s.gesture_active(),"Cancellation fixture has actual active preview");
            if(kind==0)QTest::keyClick(w.canvas,Qt::Key_Escape);
            else if(kind==1)click(w,"tool-selection");
            else {QFocusEvent loss(QEvent::FocusOut,Qt::OtherFocusReason);QApplication::sendEvent(w.canvas,&loss);}
            events();QTest::mouseRelease(w.canvas,Qt::LeftButton,Qt::NoModifier,from+QPoint(20,10));events();
            check(snapshot(s)==activation&&!s.gesture_active(),"Escape/Tool/focus cancellation restores complete state");
        }
        click(w,"tool-direct-selection");w.canvas->set_selection("other");events();w.canvas->set_selection({});events();
        check(w.canvas->direct_selection_mode()&&direct->isChecked(),"Selection changes cannot silently switch Direct Selection");
        w.canvas->set_selection("path");events();for(const auto* name:{"tool-pen","tool-text","tool-anchor","tool-guide","tool-gradient","tool-hand","tool-zoom"}){
            click(w,name);exclusive(w);check(!w.canvas->direct_selection_mode(),"Other mode clears Direct Selection");click(w,"tool-direct-selection");exclusive(w);
            check(snapshot(s)==activation,"Bidirectional Tool transitions preserve full source/history");
        }
        w.canvas->set_selection("circle");w.canvas->set_circle_source_edit(true);events();exclusive(w);
        check(!w.canvas->direct_selection_mode(),"Inspector Circle source mode synchronizes exclusively");click(w,"tool-direct-selection");exclusive(w);
        const auto chosen=w.canvas->selections();QTest::keyClick(w.canvas,Qt::Key_Escape);events();
        check(!w.canvas->direct_selection_mode()&&w.findChild<QToolButton*>("tool-selection")->isChecked()&&w.canvas->selections()==chosen&&snapshot(s)==activation,"Idle Escape returns Selection and keeps selected targets");

        reset(w,original);click(w,"tool-direct-selection");
        for(const bool space:{false,true}){
            const auto before=snapshot(s);const auto selections=w.canvas->selections();
            if(space)QTest::keyPress(w.canvas,Qt::Key_Space);
            from=QPoint(w.canvas->width()/2,w.canvas->height()/2);const auto button=space?Qt::LeftButton:Qt::MiddleButton;
            const auto image=w.canvas->grab().toImage();
            QTest::mousePress(w.canvas,button,Qt::NoModifier,from);check(w.canvas->cursor().shape()==Qt::ClosedHandCursor,"Temporary pan active");
            QTest::mouseMove(w.canvas,from+QPoint(24,16));QTest::mouseRelease(w.canvas,button,Qt::NoModifier,from+QPoint(24,16));
            if(space)QTest::keyRelease(w.canvas,Qt::Key_Space);events();
            check(w.canvas->grab().toImage()!=image&&snapshot(s)==before&&w.canvas->selections()==selections&&w.canvas->direct_selection_mode()&&
                w.canvas->cursor().shape()==Qt::CrossCursor,"Actual temporary pan changes rendered view and returns to Direct Selection, full state neutral");
        }
        const auto zoom=w.canvas->zoom();const QPointF at(w.canvas->rect().center());
        QWheelEvent wheel(at,w.canvas->mapToGlobal(at.toPoint()),QPoint{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(w.canvas,&wheel);events();check(w.canvas->zoom()>zoom&&w.canvas->direct_selection_mode()&&snapshot(s)==activation,"Wheel zoom preserves persistent Direct Selection and authored state");
        QApplication::setActiveWindow(&w);w.canvas->setFocus();events();
        check(w.isActiveWindow()&&w.canvas->hasFocus(),"Shortcut fixture has actual active Window and Canvas focus");
        for(const auto key:{Qt::Key_P,Qt::Key_G}){QTest::keyClick(w.canvas,key);events();check(w.canvas->draw_mode()&&!w.canvas->direct_selection_mode(),"Existing P/G Pen shortcut remains unchanged");click(w,"tool-direct-selection");}

        reset(w,original);from=screen(w,240,200);to=from+QPoint(20,10);drag(w,from,to);
        check(w.canvas->selected_object=="path"&&w.canvas->selected_point.empty()&&snapshot(s)==activation,"Direct path-body click selects source without whole-Object translation");
        click(w,"tool-selection");drag(w,from,to);Session objectMove(original);
        objectMove.apply({TranslateObjects{{"path"},20/w.canvas->zoom(),10/w.canvas->zoom()}},objectMove.revision());
        complete_edit(s,objectMove,{{"path",{},"transform.tx"},{"path",{},"transform.ty"}},"Ordinary Selection still moves whole Object through canonical TranslateObjects");undo_redo(w,original,objectMove.document());
        auto hidden=original;hidden.objects.at("path").visible=false;reset(w,hidden);click(w,"tool-direct-selection");
        const auto hiddenState=snapshot(s);QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,screen(w,120,160));events();
        check(w.canvas->selected_object.empty()&&snapshot(s)==hiddenState,"Unselected hidden point remains unreachable and cannot author");

        // Generated point edits retain the source and use the existing PointEdit owner.
        reset(w,original);const auto generated=path_contours(s.document().objects.at("circle"),&w.canvas->evaluated_values());
        const auto east=std::find_if(generated.front().points.begin(),generated.front().points.end(),[&](const Point& p){return std::abs(evaluate(s.document()).at({"circle",p.id,"x"})-510)<1e-7;});
        check(east!=generated.front().points.end(),"Exact generated stable East point identified");
        from=screen(w,510,350);to=from+QPoint(20,10);drag(w,from,to);
        Session correction(original);correction.apply({Set{{"circle",east->id,"x"},510+20/w.canvas->zoom()},Set{{"circle",east->id,"y"},350+10/w.canvas->zoom()}},correction.revision());
        complete_edit(s,correction,{{"circle",east->id,"x"},{"circle",east->id,"y"}},"Generated correction changes only exact stable-ID coordinates");
        check(s.document().objects.at("circle").source==original.objects.at("circle").source&&
            s.document().objects.at("circle").point_edit.has_value(),"Pointer edit uses exact PointEdit correction while preserving retained generator");undo_redo(w,original,correction.document());
        const auto correctionBefore=s.document();QTest::keyClick(w.canvas,Qt::Key_Right);events();Session correctionNudge(correctionBefore);
        correctionNudge.apply({Set{{"circle",east->id,"x"},evaluate(correctionBefore).at({"circle",east->id,"x"})+1}},correctionNudge.revision());
        check(s.document()==correctionNudge.document(),"Generated point Arrow nudge still uses retained PointEdit authority");undo_redo(w,correctionBefore,correctionNudge.document());
        auto driven=original;driven.objects.at("path").contours.front().points.front().x.expression=Expression{"120",1};reset(w,driven);
        auto refused=snapshot(s);drag(w,screen(w,120,160),screen(w,150,180));check(snapshot(s)==refused&&!s.gesture_active(),"Driven coordinate refusal is atomic and retains source");

        auto grouped=original;Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"path"};
        grouped.objects.emplace("group",group);grouped.compositions.front().roots={"group","other","circle"};reset(w,grouped);
        QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,screen(w,120,160));events();
        check(w.canvas->selected_object=="group"&&w.canvas->selected_point.empty()&&s.document()==grouped,"Direct Selection respects current Group scope");
        QTest::mouseDClick(w.canvas,Qt::LeftButton,Qt::NoModifier,screen(w,120,160));events();
        check(w.canvas->drill_scope()=="group"&&w.canvas->direct_selection_mode(),"Existing Group drill retains Direct Selection");
        from=screen(w,120,160);to=from+QPoint(20,10);drag(w,from,to);
        Session groupEdit(grouped);groupEdit.apply({Set{{"path","stable-first","x"},120+20/w.canvas->zoom()},Set{{"path","stable-first","y"},160+10/w.canvas->zoom()}},groupEdit.revision());
        complete_edit(s,groupEdit,{{"path","stable-first","x"},{"path","stable-first","y"}},"Inside-scope child point uses complete canonical edit");undo_redo(w,grouped,groupEdit.document());w.canvas->leave_group();
        auto deformed=grouped;GroupPathFollow relation;relation.id="deform";relation.path="guide";relation.contour="guide-contour";
        relation.mode="deform";relation.normal_offset=20;relation.items={{"path",{0,0,true}}};deformed.objects.at("group").path_follow=relation;
        Object guide;guide.id="guide";guide.name="Guide source";guide.visible=false;Point ga;ga.id="ga";Point gb;gb.id="gb";gb.x.literal=640;
        guide.contours={{"guide-contour",false,{ga,gb}}};deformed.objects.emplace("guide",guide);deformed.compositions.front().roots.push_back("guide");reset(w,deformed);
        w.canvas->set_selection("path","stable-first");events();refused=snapshot(s);QString error;const auto oldError=w.canvas->error;w.canvas->error=[&](QString e){error=e;};
        drag(w,screen(w,120,180),screen(w,145,190));w.canvas->error=oldError;
        check(error.startsWith("DEFORM_SOURCE_EDIT_REQUIRED:")&&snapshot(s)==refused&&!s.gesture_active(),"Derived Deform point explicitly refuses inverse editing, exact source/history retained");
        error.clear();w.canvas->error=[&](QString e){error=e;};QTest::keyClick(w.canvas,Qt::Key_Right);events();w.canvas->error=oldError;
        std::cout<<"Deform nudge error="<<error.toStdString()<<" source_preserved="<<(snapshot(s)==refused)<<'\n';
        check(error.startsWith("DEFORM_SOURCE_EDIT_REQUIRED:")&&snapshot(s)==refused,"Arrow nudge also refuses derived Deform coordinates without source mutation");

        reset(w,original);click(w,"tool-direct-selection");from=screen(w,120,160);drag(w,from,from+QPoint(24,16));
        const auto native=encode(s.document());const auto file=scratch.filePath("direct.nect.json");w.host.save(file);events();w.host.open(file);events();
        check(encode(s.document())==native&&w.canvas->direct_selection_mode()&&direct->isChecked(),"Same Window native reopen preserves complete edited source and Tool");
        Window cold(scratch.filePath("cold"),std::make_unique<FolderLibrary>(preferences),&preferences);show(cold);cold.host.open(file);events();
        check(encode(cold.host.session.document())==native&&!cold.canvas->direct_selection_mode()&&cold.findChild<QToolButton*>("tool-selection")->isChecked(),"New Window native reopen defaults to Selection and preserves edited source");
        check(preferences.value("unrelated").toString()=="preserved"&&preferences.value("workspace/tools/textCreationDirection").toString()=="vertical","Existing Text variant and unrelated settings preserved");
        w.canvas->fit_artboard();w.canvas->set_selection("path","stable-first");events();
        if(const auto capture=qEnvironmentVariable("NECT_DIRECT_CAPTURE");!capture.isEmpty())check(w.grab().save(capture),"Editable direct-selection Window captured");
        std::cout<<"rail_direct_selection_tests: "<<checks<<" checks passed; DPR="<<w.devicePixelRatioF()<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"After "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
