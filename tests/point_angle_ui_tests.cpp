#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QTest>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;

namespace {
int checks=0;
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);++checks;}
void events() {QApplication::processEvents();}
QByteArray reference(const Ref& ref) {
    return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
        {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}})
        .toJson(QJsonDocument::Compact);
}
QWidget* dial(Window& window,const Ref& ref) {
    const auto encoded=reference(ref);
    for(auto* control:window.findChildren<QWidget*>())
        if(control->isVisible()&&control->objectName().startsWith("point-angle-knob")&&
           control->property("nect-reference").toByteArray()==encoded)return control;
    throw std::runtime_error("selected point angle has a visible radial control with the same exact Ref");
}
QWidget* control(Window& window,const QString& name,const Ref& ref) {
    const auto encoded=reference(ref);
    for(auto* item:window.findChildren<QWidget*>())
        if(item->isVisible()&&item->objectName()==name&&item->property("nect-reference").toByteArray()==encoded)
            return item;
    throw std::runtime_error("current visible control has the expected name and exact Ref");
}
QLineEdit* numeric(Window& window,const Ref& ref) {
    const auto encoded=reference(ref);
    for(auto* input:window.findChildren<QLineEdit*>())
        if(input->isVisible()&&input->property("nect-reference").toByteArray()==encoded)return input;
    throw std::runtime_error("selected point angle keeps its numeric editor on the exact Ref");
}
QPointF point(double angle) {
    const auto radians=angle*std::acos(-1.0)/180.0;
    return {22+16*std::cos(radians),22+16*std::sin(radians)};
}
void mouse(QWidget* control,QEvent::Type type,double angle,Qt::MouseButton button,Qt::MouseButtons buttons) {
    const auto local=point(angle);
    QMouseEvent event(type,local,QPointF(control->mapToGlobal(QPoint(0,0)))+local,button,buttons,Qt::NoModifier);
    QApplication::sendEvent(control,&event);events();
}
void press(QWidget* control) {mouse(control,QEvent::MouseButtonPress,0,Qt::LeftButton,Qt::LeftButton);}
void move(QWidget* control,double angle) {mouse(control,QEvent::MouseMove,angle,Qt::NoButton,Qt::LeftButton);}
void release(QWidget* control,double angle) {mouse(control,QEvent::MouseButtonRelease,angle,Qt::LeftButton,Qt::NoButton);}
void edit(Window& window,const Ref& ref,const QString& value) {
    auto* input=numeric(window,ref);input->setText(value);input->setModified(true);input->editingFinished();events();
}
QPointF rendered_indicator(QWidget* control) {
    events();QTest::qWait(15);const auto image=control->grab().toImage();const auto dpr=image.devicePixelRatio();
    QPointF sum;int count=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x) {
        const auto color=image.pixelColor(x,y);const QPointF delta((x+0.5)/dpr-control->width()/2.0,
            (y+0.5)/dpr-control->height()/2.0);const auto radius=std::hypot(delta.x(),delta.y());
        if(radius>4&&radius<14&&color.blue()-color.red()>50&&color.green()>100){sum+=delta;++count;}
    }
    check(count>5,"point-angle dial renders a visible direction indicator");return sum/count;
}
void load_path(Window& window) {
    auto document=empty_document("point-angle-document","point-angle-composition","point-angle-artboard");
    Point point;point.id="anchor-id";point.x.literal=80;point.y.literal=70;
    point.in_angle.literal=725.1234567890123;point.in_length.literal=0;
    point.out_angle.literal=-725.25;point.out_length.literal=0;
    Session session(document);
    session.apply({CreatePath{"point-angle-composition","","authored-path","Authored path",
        {{"path-contour-id",false,{point}}}}},session.revision());
    window.host.session=Session(session.document());window.host.session_id+="-path";window.host.edited();
    window.canvas->set_selection("authored-path","anchor-id");events();
}
Id first_generated_point(const Document& document) {
    const auto values=evaluate(document);
    return path_contours(document.objects.at("primitive"),&values).front().points.front().id;
}
void load_primitive(Window& window,const char* type,bool with_other_angles=false) {
    auto document=empty_document("point-angle-document","point-angle-composition","point-angle-artboard");
    Session session(document);
    auto source=default_primitive("source-stable-id",type);
    std::vector<Command> commands{CreatePrimitive{"point-angle-composition","","primitive","Primitive",source}};
    if(with_other_angles)commands.push_back(AddOperation{"primitive",default_operation("point-repeater","nect.shape.repeater"),1});
    session.apply(commands,session.revision());
    window.host.session=Session(session.document());window.host.session_id+="-primitive";window.host.edited();
    window.canvas->set_selection("primitive",first_generated_point(window.host.session.document()));events();
}
void shared_ref_and_history(Window& window) {
    load_path(window);
    for(const auto& ref:{Ref{"authored-path","anchor-id","in.angle"},Ref{"authored-path","anchor-id","out.angle"}}) {
        auto* control=dial(window,ref);auto* input=numeric(window,ref);
        check(control->property("nect-reference").toByteArray()==input->property("nect-reference").toByteArray(),
            "each point angle dial and numeric editor share the exact authored Ref");
        const auto initial=evaluate(window.host.session.document()).at(ref);
        check(input->text()==QString::number(initial,'g',17),"angle numeric entry round-trips the exact authored double");
        check(control->accessibleName().contains("angle",Qt::CaseInsensitive)&&
            control->accessibleDescription().contains(QString::number(initial,'g',17)),
            "point dial exposes a meaningful accessible name and exact unwrapped value");
        const auto modulo=std::fmod(initial,360.0)<0?std::fmod(initial,360.0)+360.0:std::fmod(initial,360.0);
        check(control->accessibleDescription().contains(QString("indicator %1 degrees").arg(QString::number(modulo,'g',15))),
            "point dial reports a modulo indicator while preserving signed unwrapped authored degrees");
        if(ref.field=="in.angle") {
            edit(window,ref,"0");auto* zero=dial(window,ref);auto direction=rendered_indicator(zero);
            check(direction.x()>2&&std::abs(direction.y())<2,"point-angle zero indicator points along local +X");
            edit(window,ref,"90");direction=rendered_indicator(dial(window,ref));
            check(direction.y()>2&&std::abs(direction.x())<2,"point-angle positive quarter-turn points clockwise in Y-down coordinates");
            edit(window,ref,QString::number(initial,'g',17));control=dial(window,ref);input=numeric(window,ref);
        }
        const auto old_document=window.host.session.document();const auto old_revision=window.host.session.revision();
        press(control);move(control,10);
        check(window.host.session.gesture_active()&&window.host.session.revision()==old_revision,
            "point dial previews within one active Session gesture without early revision");
        const auto preview=evaluate(window.host.session.preview_document()).at(ref);
        check(std::abs(preview-(initial+10))<1e-9&&input->text()==QString::number(preview,'g',17),
            "live point-angle preview preserves signed turns and refreshes the exact numeric field");
        check(window.host.session.document()==old_document,"live point-angle preview leaves committed point topology unchanged");
        release(control,10);
        check(window.host.session.revision()==old_revision+1&&
            std::abs(evaluate(window.host.session.document()).at(ref)-(initial+10))<1e-9,
            "one point-angle drag commits one signed authored-value revision");
        window.host.session.undo(window.host.session.revision());
        check(window.host.session.document()==old_document,"one Undo restores the exact original point angles and geometry");
        window.host.edited();events();
        check(numeric(window,ref)->text()==QString::number(initial,'g',17),"Undo refreshes the point-angle numeric editor");
        window.host.session.redo(window.host.session.revision());
        window.host.edited();events();
        check(std::abs(evaluate(window.host.session.document()).at(ref)-(initial+10))<1e-9&&
            numeric(window,ref)->text()==QString::number(initial+10,'g',17),"Redo restores the dial and numeric values together");
        load_path(window);
    }
}
void authored_cancel_and_zero_length(Window& window) {
    load_path(window);
    const Ref ref{"authored-path","anchor-id","in.angle"};auto* control=dial(window,ref);
    check(control->isEnabled(),"a zero-length authored handle remains angle-editable");
    const auto before=encode(window.host.session.document());const auto revision=window.host.session.revision();
    press(control);release(control,0);
    check(window.host.session.revision()==revision&&encode(window.host.session.document())==before,
        "a no-motion point dial click creates no history or authored bytes");
    control=dial(window,ref);press(control);move(control,15);QTest::keyClick(control,Qt::Key_Escape);events();
    check(!window.host.session.gesture_active()&&window.host.session.revision()==revision&&
        encode(window.host.session.document())==before,"Escape cancels point-angle preview without a commit");
    control=dial(window,ref);press(control);move(control,10);move(control,0);release(control,0);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==revision&&
        encode(window.host.session.document())==before,"returning a point dial to its start cancels instead of adding microscopic history");
}
void generated_fallback_and_identity(Window& window) {
    for(const auto* type:{"nect.shape.circle","nect.shape.ellipse","nect.shape.polygon","nect.shape.star"}) {
        load_primitive(window,type);
        const auto document=window.host.session.document();const auto source=document.objects.at("primitive").source->id;
        const auto point_id=window.canvas->selected_point;const Ref ref{"primitive",point_id,"out.angle"};
        const auto initial=evaluate(document).at(ref);auto* control=dial(window,ref);auto* input=numeric(window,ref);
        check(property_origin(document,ref)=="generated"&&input->text()==QString::number(initial,'g',17),
            "generated Circle/Ellipse/Polygon/Star fallback uses Inspector values without a display-only Scalar");
        const auto original=encode(document);const auto revision=window.host.session.revision();
        press(control);release(control,0);
        check(!window.host.session.document().objects.at("primitive").point_edit&&
            window.host.session.revision()==revision&&encode(window.host.session.document())==original,
            "no-motion generated-point click does not manufacture or activate Point Edit");
        control=dial(window,ref);press(control);move(control,10);move(control,0);release(control,0);
        check(!window.host.session.document().objects.at("primitive").point_edit&&
            window.host.session.revision()==revision&&encode(window.host.session.document())==original,
            "no-net generated-point gesture removes its provisional Point Edit and has no history");
        control=dial(window,ref);press(control);move(control,10);release(control,10);
        const auto& edited=window.host.session.document().objects.at("primitive");
        check(edited.source&&edited.source->id==source&&window.canvas->selected_point==point_id,
            "generated-point angle commit preserves source and stable point identity");
        check(edited.point_edit&&edited.point_edit->overrides.size()==1&&
            edited.point_edit->overrides.at(point_id).size()==1&&
            edited.point_edit->id=="source-stable-id-point-edit"&&edited.point_edit->version==1&&
            edited.point_edit->overrides.at(point_id).contains("out.angle")&&
            std::abs(evaluate(window.host.session.document()).at(ref)-(initial+10))<1e-9,
            "successful generated-angle edit writes only the exact canonical Point Edit destination and field");
    }
}
void driven_sources_and_drafts(Window& window) {
    load_path(window);const Ref target{"authored-path","anchor-id","in.angle"};
    window.host.session.apply({SetExpression{{target},Expression{"725",1}}},window.host.session.revision());
    window.host.edited();window.canvas->set_selection("authored-path","anchor-id");events();
    check(!dial(window,target)->isEnabled(),"an expression-driven authored handle disables its radial editor");
    load_primitive(window,"nect.shape.ellipse");const auto point_id=window.canvas->selected_point;
    const Ref generated{"primitive",point_id,"in.angle"};
    window.host.session.apply({SetExpression{{generated},Expression{"45",1}}},window.host.session.revision());
    window.host.edited();window.canvas->set_selection("primitive",point_id);events();
    check(!dial(window,generated)->isEnabled(),"a generated Point Edit expression source disables its radial editor");
    const auto edit_id=window.host.session.document().objects.at("primitive").point_edit->id;
    window.host.session.apply({EnablePointEdit{"primitive",false}},window.host.session.revision());
    window.host.edited();window.canvas->set_selection("primitive",point_id);events();
    check(property_origin(window.host.session.document(),generated)=="bypassed_point_edit"&&
        !dial(window,generated)->isEnabled(),
        "bypassed stored expression remains guarded even when the evaluated fallback is numeric");
    check(window.host.session.document().objects.at("primitive").point_edit->id==edit_id,
        "source guard keeps canonical Point Edit identity unchanged");
    load_primitive(window,"nect.shape.ellipse");const auto bound_point=window.canvas->selected_point;
    const Ref bound_target{"primitive",bound_point,"in.angle"};
    const Ref bound_source{"primitive",bound_point,"out.angle"};
    window.host.session.apply({LinkProperties{{bound_target},bound_source,false}},window.host.session.revision());
    window.host.edited();window.canvas->set_selection("primitive",bound_point);events();
    check(!dial(window,bound_target)->isEnabled(),"a binding-driven generated handle disables its radial editor");
    window.host.session.apply({EnablePointEdit{"primitive",false}},window.host.session.revision());
    window.host.edited();window.canvas->set_selection("primitive",bound_point);events();
    check(property_origin(window.host.session.document(),bound_target)=="bypassed_point_edit"&&
        !dial(window,bound_target)->isEnabled(),
        "bypassed stored binding remains guarded even when the evaluated fallback is numeric");
    load_primitive(window,"nect.shape.ellipse");const auto literal_point=window.canvas->selected_point;
    const Ref literal_target{"primitive",literal_point,"in.angle"};
    window.host.session.apply({EditProperties{{literal_target},123,false}},window.host.session.revision());
    window.host.session.apply({EnablePointEdit{"primitive",false}},window.host.session.revision());
    window.host.edited();window.canvas->set_selection("primitive",literal_point);events();
    check(property_origin(window.host.session.document(),literal_target)=="bypassed_point_edit"&&
        dial(window,literal_target)->isEnabled(),"an undriven bypassed literal follows the ordinary numeric editability policy");
    const auto bypassed_bytes=encode(window.host.session.document());const auto bypassed_revision=window.host.session.revision();
    auto* literal_dial=dial(window,literal_target);press(literal_dial);move(literal_dial,10);move(literal_dial,0);release(literal_dial,0);
    check(encode(window.host.session.document())==bypassed_bytes&&window.host.session.revision()==bypassed_revision&&
        !window.host.session.document().objects.at("primitive").point_edit->enabled,
        "no-net dial gesture preserves an existing bypassed literal Point Edit and its disabled state");
    load_path(window);const Ref ref{"authored-path","anchor-id","out.angle"};
    auto* input=numeric(window,ref);auto* control=dial(window,ref);const auto bytes=encode(window.host.session.document());
    const auto revision=window.host.session.revision();input->setText("123");input->setModified(true);
    press(control);release(control,0);
    check(!window.host.session.gesture_active()&&input->isModified()&&input->text()=="123"&&
        window.host.session.revision()==revision&&encode(window.host.session.document())==bytes&&
        window.statusBar()->currentMessage().startsWith("UNCOMMITTED_INPUT"),
        "radial control refuses to overwrite a dirty numeric draft");
}
void point_edit_enabled_driver_guard(Window& window) {
    auto document=empty_document("point-angle-driver-document","point-angle-composition","point-angle-artboard");
    Session setup(document);
    setup.apply({CreatePrimitive{"point-angle-composition","","primitive","Target",
            default_primitive("target-source-id","nect.shape.ellipse")},
        CreatePrimitive{"point-angle-composition","","flag-source","Flag source",
            default_primitive("flag-source-id","nect.shape.ellipse")}},setup.revision());
    document=setup.document();
    document.objects.at("flag-source").point_edit=PointEdit{"flag-source-id-point-edit",1,false,{}};
    PointEdit target_edit{"target-source-id-point-edit",1,false,{}};
    target_edit.enabled_driver=point_edit_enabled_ref("flag-source","flag-source-id-point-edit");
    document.objects.at("primitive").point_edit=target_edit;
    auto malformed=document;malformed.objects.at("flag-source").point_edit->id="flag-point-edit";
    bool invalid_id_rejected=false;
    try {Session invalid(malformed);} catch(const Error& error) {invalid_id_rejected=error.code=="INVALID_POINT_EDIT";}
    check(invalid_id_rejected,"noncanonical Point Edit identity is rejected by core validation");
    window.host.session=Session(document);window.host.session_id+="-enabled-driver";window.host.edited();
    const auto point_id="target-source-id-east";const Ref target{"primitive",point_id,"in.angle"};
    window.canvas->set_selection("primitive",point_id);events();
    auto* control=dial(window,target);
    check(control->isEnabled(),"Point Edit enabled driver does not invent a dial-disable policy for an otherwise undriven angle");
    const auto before=encode(window.host.session.document());const auto revision=window.host.session.revision();
    press(control);move(control,10);release(control,10);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==revision&&
        encode(window.host.session.document())==before&&window.statusBar()->currentMessage().startsWith("DRIVEN_PROPERTY"),
        "angle edit follows the existing core Point Edit enabled-driver guard without partial commit");
}
void point_edit_enabled_driver_allowed(Window& window) {
    auto document=empty_document("point-angle-enabled-driver-document","point-angle-composition","point-angle-artboard");
    Session setup(document);
    setup.apply({CreatePrimitive{"point-angle-composition","","primitive","Target",
            default_primitive("target-source-id","nect.shape.ellipse")},
        CreatePrimitive{"point-angle-composition","","flag-source","Flag source",
            default_primitive("flag-source-id","nect.shape.ellipse")}},setup.revision());
    document=setup.document();
    document.objects.at("flag-source").point_edit=PointEdit{"flag-source-id-point-edit",1,false,{}};
    PointEdit target_edit{"target-source-id-point-edit",1,true,{}};
    target_edit.enabled_driver=point_edit_enabled_ref("flag-source","flag-source-id-point-edit");
    document.objects.at("primitive").point_edit=target_edit;
    window.host.session=Session(document);window.host.session_id+="-enabled-driver-literal-true";window.host.edited();
    const auto point_id="target-source-id-east";const Ref target{"primitive",point_id,"in.angle"};
    window.canvas->set_selection("primitive",point_id);events();auto* control=dial(window,target);
    check(control->isEnabled(),"evaluated-false enabled driver does not override the authored-literal core guard in the UI");
    const auto revision=window.host.session.revision();press(control);move(control,10);release(control,10);
    const auto& edited=window.host.session.document().objects.at("primitive");
    check(window.host.session.revision()==revision+1&&edited.point_edit&&
        edited.point_edit->enabled&&edited.point_edit->enabled_driver&&
        edited.point_edit->overrides.at(point_id).contains("in.angle")&&
        property_origin(window.host.session.document(),target)=="bypassed_point_edit",
        "authored-enabled Point Edit accepts the exact angle edit while its typed enabled source still evaluates false");
}
void stale_range_and_aba(Window& window) {
    load_path(window);const Ref ref{"authored-path","anchor-id","in.angle"};auto* control=dial(window,ref);
    auto& session=window.host.session;session.apply({Set{ref,999999990}},session.revision());
    const auto range_bytes=encode(session.document());const auto range_revision=session.revision();window.host.edited();events();
    control=dial(window,ref);press(control);move(control,20);events();
    check(!session.gesture_active()&&session.revision()==range_revision&&encode(session.document())==range_bytes&&
        numeric(window,ref)->text()=="999999990",
        "out-of-range angle sample cancels without partial authored state or history");
    load_path(window);control=dial(window,ref);
    session.apply({Set{ref,30}},session.revision());press(control);release(control,0);
    check(!session.gesture_active()&&evaluate(session.document()).at(ref)==30&&
        window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT"),
        "a stale selected-point revision refuses a new radial gesture");
    load_path(window);control=dial(window,ref);auto changed_authored=session.document();
    changed_authored.objects.at("authored-path").contours.front().id="replacement-contour-id";
    session=Session(changed_authored);const auto changed_authored_bytes=encode(session.document());
    press(control);release(control,0);
    check(!session.gesture_active()&&encode(session.document())==changed_authored_bytes&&
        window.statusBar()->currentMessage().startsWith("MISSING_PROPERTY"),
        "same-revision authored point control rejects replacement contour identity");
    load_path(window);control=dial(window,ref);auto changed_document=session.document();changed_document.id="replacement-document-id";
    session=Session(changed_document);const auto changed_document_bytes=encode(session.document());
    press(control);release(control,0);
    check(!session.gesture_active()&&encode(session.document())==changed_document_bytes&&
        window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT"),
        "same-revision point control rejects a replacement document identity");
    load_path(window);control=dial(window,ref);const auto original_session_id=window.host.session_id;
    window.host.session_id+="-replacement";const auto session_bytes=encode(session.document());press(control);release(control,0);
    check(!session.gesture_active()&&window.host.session_id!=original_session_id&&
        encode(window.host.session.document())==session_bytes&&window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT"),
        "point control refuses a replacement Host Session identity at the same revision");
    load_primitive(window,"nect.shape.ellipse");const auto generated_point=window.canvas->selected_point;
    const Ref generated_ref{"primitive",generated_point,"in.angle"};control=dial(window,generated_ref);
    auto point_edit_appeared=session.document();
    point_edit_appeared.objects.at("primitive").point_edit=PointEdit{"source-stable-id-point-edit",1,true,{}};
    session=Session(point_edit_appeared);const auto point_edit_bytes=encode(session.document());press(control);release(control,0);
    check(!session.gesture_active()&&encode(session.document())==point_edit_bytes&&
        window.statusBar()->currentMessage().startsWith("MISSING_PROPERTY"),
        "same-revision generated control refuses an unexpected canonical Point Edit destination");
    load_path(window);control=dial(window,ref);press(control);move(control,5);
    check(session.gesture_active(),"point dial owns preview A before same-revision replacement");
    session.cancel_gesture();session.begin_gesture(session.revision());session.update_gesture({EditProperties{{ref},321,false}});
    const auto replacement=session.preview_document();move(control,15);release(control,15);
    check(session.gesture_active()&&session.preview_document()==replacement,
        "old point dial callback cannot update, commit or cancel same-revision replacement gesture B");
    session.cancel_gesture();
    load_primitive(window,"nect.shape.polygon");control=dial(window,Ref{"primitive",window.canvas->selected_point,"in.angle"});
    auto changed=session.document();changed.objects.at("primitive").source->id="different-source-id";session=Session(changed);
    const auto changed_bytes=encode(session.document());press(control);release(control,0);
    check(!session.gesture_active()&&encode(session.document())==changed_bytes,
        "same-revision source identity replacement refuses the stale generated point-angle control");
    load_primitive(window,"nect.shape.polygon");
    const auto generated_values=evaluate(session.document());
    const auto generated_contours=path_contours(session.document().objects.at("primitive"),&generated_values);
    const auto removed_point=generated_contours.front().points.back().id;
    window.canvas->set_selection("primitive",removed_point);events();
    const Ref removed_ref{"primitive",removed_point,"in.angle"};control=dial(window,removed_ref);
    auto changed_topology=session.document();changed_topology.objects.at("primitive").source->parameters.at("points").literal=3;
    session=Session(changed_topology);const auto changed_topology_bytes=encode(session.document());press(control);release(control,0);
    check(!session.gesture_active()&&encode(session.document())==changed_topology_bytes&&
        window.statusBar()->currentMessage().startsWith("MISSING_PROPERTY"),
        "same-revision generated angle Ref refuses a point ID removed by changed topology");
}
void coexisting_and_teardown(Window& window) {
    load_primitive(window,"nect.shape.polygon",true);
    const auto selected_point=window.canvas->selected_point;
    const auto point_values=evaluate(window.host.session.document());
    const auto contours=path_contours(window.host.session.document().objects.at("primitive"),&point_values);
    const auto second_point=contours.front().points.back().id;
    window.canvas->set_selections({{"primitive",selected_point},{"primitive",second_point}});events();
    const auto all_controls=window.findChildren<QWidget*>();
    const auto visible_point_dials=std::count_if(all_controls.begin(),all_controls.end(),
        [](const auto* item){return item->isVisible()&&item->objectName().startsWith("point-angle-knob");});
    const auto visible_batch_dials=std::count_if(all_controls.begin(),all_controls.end(),
        [](const auto* item){return item->isVisible()&&item->objectName().startsWith("batch-point-angle-knob-");});
    check(visible_point_dials==0&&visible_batch_dials==2,
        "multi-point selection adds the two batch radial controls without adding single-point controls");
    window.canvas->set_selection("primitive",selected_point);events();
    const Ref point_ref{"primitive",selected_point,"in.angle"};auto* point_dial=dial(window,point_ref);
    const Ref other_point_ref{"primitive",selected_point,"out.angle"};auto* other_point_dial=dial(window,other_point_ref);
    const Ref primitive_rotation{"primitive","","generator.rotation"};
    const Ref repeater_rotation{"primitive","","op.point-repeater.rotation"};
    auto* primitive_dial=control(window,"primitive-angle-knob",primitive_rotation);
    auto* repeater_dial=control(window,"repeater-angle-knob-point-repeater",repeater_rotation);
    check(primitive_dial->isVisible()&&primitive_dial->isEnabled()&&
        repeater_dial->isVisible()&&repeater_dial->isEnabled()&&
        point_dial->property("nect-reference").toByteArray()==reference(point_ref)&&
        other_point_dial->property("nect-reference").toByteArray()==reference(other_point_ref),
        "current Inspector contains visible enabled primitive, Repeater, incoming and outgoing dials on exact Refs");
    press(point_dial);move(point_dial,8);const auto preview=window.host.session.preview_document();
    press(primitive_dial);release(primitive_dial,0);
    check(window.host.session.gesture_active()&&window.host.session.preview_document()==preview,
        "busy primitive adapter cannot steal a point-angle Session gesture");
    press(repeater_dial);release(repeater_dial,0);
    check(window.host.session.gesture_active()&&window.host.session.preview_document()==preview,
        "busy Repeater adapter cannot steal a point-angle Session gesture");
    press(other_point_dial);release(other_point_dial,0);
    check(window.host.session.gesture_active()&&window.host.session.preview_document()==preview,
        "busy outgoing point-angle adapter cannot steal the incoming point-angle preview");
    release(point_dial,8);events();
    for(const auto& field:{std::string("in.angle"),std::string("out.angle")}) {
        const Ref point_ref_now{"primitive",window.canvas->selected_point,field};point_dial=dial(window,point_ref_now);
        press(point_dial);move(point_dial,8);window.refresh(false);events();
        check(!window.host.session.gesture_active(),"Inspector rebuild synchronously cancels either point-angle adapter");
    }

    QTemporaryDir close_directory;check(close_directory.isValid(),"isolated close fixture scratch exists");
    QSettings close_settings(close_directory.filePath("settings.ini"),QSettings::IniFormat);
    Window close_window(close_directory.path(),std::make_unique<FolderLibrary>(close_settings));close_window.show();events();
    auto& close_session=close_window.host.session;
    const auto close_composition=close_session.document().compositions.front().id;
    close_session.apply({CreatePrimitive{close_composition,"","close-object","Close test",
        default_primitive("close-source-id","nect.shape.ellipse")}},close_session.revision());
    close_window.host.edited();close_window.canvas->set_selection("close-object","close-source-id-east");events();
    const Ref close_ref{"close-object","close-source-id-east","out.angle"};
    auto* close_dial=dial(close_window,close_ref);press(close_dial);move(close_dial,8);close_window.close();events();
    check(!close_session.gesture_active(),"Window close cancels point-angle preview before Host flush");
    close_window.show();events();close_dial=dial(close_window,close_ref);
    const auto revision=close_session.revision();press(close_dial);move(close_dial,5);release(close_dial,5);
    check(close_session.revision()==revision+1,"surviving point-angle control remains reusable after close");
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"point-angle test scratch exists");
        QSettings settings(directory.filePath("settings.ini"),QSettings::IniFormat);
        Window window(directory.path(),std::make_unique<FolderLibrary>(settings));window.show();events();
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")
            check(window.devicePixelRatioF()>=1.9,"high-DPI point-angle run uses an actual scaled widget DPR");
        shared_ref_and_history(window);authored_cancel_and_zero_length(window);
        generated_fallback_and_identity(window);driven_sources_and_drafts(window);
        point_edit_enabled_driver_guard(window);point_edit_enabled_driver_allowed(window);
        stale_range_and_aba(window);coexisting_and_teardown(window);
        std::cout<<"PASS "<<checks<<" point angle checks\n";return 0;
    } catch(const std::exception& exception) {
        std::cerr<<"FAIL "<<exception.what()<<'\n';return 1;
    }
}
