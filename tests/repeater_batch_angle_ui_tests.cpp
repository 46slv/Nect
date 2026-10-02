#include "window.hpp"
#include "batch_angle_geometry.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QTest>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);++checks;}
void events() {QApplication::processEvents();}
QByteArray ref_json(const Ref& ref) {
    return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
        {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}})
        .toJson(QJsonDocument::Compact);
}
QByteArray refs_json(const std::vector<Ref>& refs) {
    QJsonArray values;for(const auto& ref:refs)values.append(QJsonDocument::fromJson(ref_json(ref)).object());
    return QJsonDocument(values).toJson(QJsonDocument::Compact);
}
struct RepeaterSpec {std::size_t slot=1;std::string id;double rotation=0;bool enabled=true;};
struct ObjectSpec {
    std::string id;Kind kind=Kind::path;double source_rotation=0;std::vector<RepeaterSpec> repeaters;
    bool add_fill_prefix=false;
    std::optional<std::pair<std::size_t,std::string>> extra_operation;
};
void load(Window& window,const std::vector<ObjectSpec>& specs,const std::vector<std::string>& selected={}) {
    auto document=empty_document("repeater-batch-document","repeater-batch-composition","repeater-batch-artboard");
    Session setup(document);std::vector<Command> create;
    for(const auto& spec:specs) {
        if(spec.kind==Kind::text)create.push_back(CreateText{"repeater-batch-composition","",spec.id,spec.id,
            default_text("text-source-"+spec.id,"Text "+spec.id)});
        else {
            const auto source=default_primitive("primitive-source-"+spec.id,"nect.shape.polygon");
            create.push_back(CreatePrimitive{"repeater-batch-composition","",spec.id,spec.id,source});
            create.push_back(Set{{spec.id,"","generator.rotation"},spec.source_rotation});
        }
    }
    setup.apply(create,setup.revision());
    for(const auto& spec:specs)if(spec.add_fill_prefix) {
        auto fill=default_operation("unrelated-fill-"+spec.id,"nect.paint.fill");
        setup.apply({AddOperation{spec.id,std::move(fill),0}},setup.revision());
    }
    for(const auto& spec:specs)if(spec.extra_operation) {
        auto other=default_operation(spec.extra_operation->second,"nect.shape.offset");
        setup.apply({AddOperation{spec.id,std::move(other),spec.extra_operation->first}},setup.revision());
    }
    for(const auto& spec:specs)for(const auto& row:spec.repeaters) {
        auto operation=default_operation(row.id,"nect.shape.repeater");
        operation.parameters.at("rotation").literal=row.rotation;operation.enabled=row.enabled;
        setup.apply({AddOperation{spec.id,std::move(operation),row.slot}},setup.revision());
    }
    window.canvas->set_selections({});window.host.session=Session(setup.document());
    window.host.session_id+="-repeater-batch";window.host.edited();
    auto ids=selected;if(ids.empty())for(const auto& spec:specs)ids.push_back(spec.id);
    std::vector<Canvas::Selection> selection;for(const auto& id:ids)selection.push_back({id,{}});
    window.canvas->set_selections(selection);events();
}
std::vector<Ref> rotation_refs(Window& window,std::size_t slot) {
    std::vector<Ref> result;
    for(const auto& selected:window.canvas->selections()) {
        const auto& object=window.host.session.document().objects.at(selected.object);
        if(slot>=object.stack.size())throw std::runtime_error("selected object has the requested Repeater slot");
        result.push_back(operation_ref(selected.object,object.stack[slot].id,"rotation"));
    }
    return result;
}
QWidget* batch_dial(Window& window,const std::vector<Ref>& targets) {
    const auto data=refs_json(targets);
    for(auto* widget:window.findChildren<QWidget*>())
        if(widget->isVisible()&&widget->objectName()=="batch-repeater-angle-knob"&&
           widget->property("nect-targets").toByteArray()==data)return widget;
    throw std::runtime_error("visible Repeater batch dial uses the exact complete ordered Ref vector");
}
QLineEdit* batch_numeric(Window& window,const std::vector<Ref>& targets) {
    const auto data=refs_json(targets);
    for(auto* input:window.findChildren<QLineEdit*>())
        if(input->isVisible()&&input->property("nect-targets").toByteArray()==data)return input;
    throw std::runtime_error("existing stack numeric row uses the exact complete ordered Ref vector");
}
QLabel* caption(QWidget* dial) {return dial?dial->parentWidget()->findChild<QLabel*>():nullptr;}
QString caption_delta(const QLabel* note) {
    if(!note)return {};
    const auto marker=QStringLiteral("Δ ");const auto start=note->text().indexOf(marker);
    if(start<0)return {};
    const auto value_start=start+marker.size();const auto end=note->text().indexOf(QStringLiteral("°"),value_start);
    return end<0?QString{}:note->text().mid(value_start,end-value_start);
}
QString accessible_delta(const QWidget* dial) {
    if(!dial)return {};
    const auto marker=QStringLiteral("relative delta ");const auto start=dial->accessibleDescription().indexOf(marker);
    if(start<0)return {};
    const auto value_start=start+marker.size();const auto end=dial->accessibleDescription().indexOf(QStringLiteral(" degrees"),value_start);
    return end<0?QString{}:dial->accessibleDescription().mid(value_start,end-value_start);
}
QPointF knob_point(double angle) {
    const auto radians=angle*std::acos(-1.0)/180.0;return {22+16*std::sin(radians),22-16*std::cos(radians)};
}
void mouse(QWidget* control,QEvent::Type type,double angle,Qt::MouseButton button,Qt::MouseButtons buttons) {
    const auto local=knob_point(angle);
    QMouseEvent event(type,local,QPointF(control->mapToGlobal(QPoint(0,0)))+local,button,buttons,Qt::NoModifier);
    QApplication::sendEvent(control,&event);events();
}
void press(QWidget* control,double angle=0) {mouse(control,QEvent::MouseButtonPress,angle,Qt::LeftButton,Qt::LeftButton);}
void move(QWidget* control,double angle) {mouse(control,QEvent::MouseMove,angle,Qt::NoButton,Qt::LeftButton);}
void release(QWidget* control,double angle=0) {mouse(control,QEvent::MouseButtonRelease,angle,Qt::LeftButton,Qt::NoButton);}
void mouse_global(QWidget* control,QEvent::Type type,const QPointF& global,Qt::MouseButton button,Qt::MouseButtons buttons) {
    const QPointF local(control->mapFromGlobal(global.toPoint()));
    QMouseEvent event(type,local,global,button,buttons,Qt::NoModifier);QApplication::sendEvent(control,&event);events();
}
void edit(QLineEdit* input,const QString& text) {input->setText(text);input->setModified(true);input->editingFinished();events();}
std::vector<double> values(Window& window,const std::vector<Ref>& refs) {
    const auto evaluated=evaluate(window.host.session.document());std::vector<double> result;
    for(const auto& ref:refs)result.push_back(evaluated.at(ref));return result;
}
bool near(const std::vector<double>& a,const std::vector<double>& b,double tolerance=1e-8) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(std::abs(a[i]-b[i])>tolerance)return false;
    return true;
}
std::vector<double> values_from_document(const Document& document,const std::vector<Ref>& refs);
QPointF rendered_indicator(Window& window,QWidget* dial) {
    events();QTest::qWait(80);const auto whole=window.grab().toImage();const auto dpr=whole.devicePixelRatio();
    const auto origin=dial->mapTo(&window,QPoint(0,0));
    const auto image=whole.copy(QRect(qRound(origin.x()*dpr),qRound(origin.y()*dpr),
        qRound(dial->width()*dpr),qRound(dial->height()*dpr)));
    QPointF sum;int count=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x) {
        const auto color=image.pixelColor(x,y);const QPointF delta((x+0.5)/dpr-22,(y+0.5)/dpr-22);
        const auto radius=std::hypot(delta.x(),delta.y());
        if(radius>4&&radius<14&&color.blue()-color.red()>50&&color.green()>100){sum+=delta;++count;}
    }
    check(count>5,"rendered batch Repeater dial exposes a visible indicator stroke");return sum/count;
}
bool has_repeater_dial(Window& window) {
    for(auto* widget:window.findChildren<QWidget*>())if(widget->isVisible()&&widget->objectName()=="batch-repeater-angle-knob")return true;
    return false;
}
void discovery_and_exact_refs(Window& window) {
    std::vector<Ref> refs;QWidget* dial=nullptr;QLineEdit* numeric=nullptr;Document replacement;
#ifdef _WIN32
    load(window,{{"path-a",Kind::path,0,{{1,"path-a-repeater",5}}},
        {"text-b",Kind::text,0,{{1,"text-b-repeater",5}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);numeric=batch_numeric(window,refs);
    const std::vector<Ref> expected{operation_ref("path-a","path-a-repeater","rotation"),
        operation_ref("text-b","text-b-repeater","rotation")};
    check(refs==expected&&dial->property("nect-targets").toByteArray()==refs_json(expected)&&
        numeric->property("nect-targets").toByteArray()==refs_json(expected)&&
        dial->property("nect-reference").toByteArray()==ref_json(expected.front()),
        "Path and Text Repeater batch share their existing numeric row's exact ordered per-object operation Refs");
    check(numeric->text()=="5"&&!numeric->property("nect-mixed").toBool()&&
        dial->accessibleName().contains("2 objects"),
        "different operation IDs and mixed Path/Text source kinds keep the common authored rotation");
    const auto before_text_drag=window.host.session.document();const auto text_revision=window.host.session.revision();
    press(dial);move(dial,8);const auto text_preview=evaluate(window.host.session.preview_document());
    check(near({text_preview.at(expected[0]),text_preview.at(expected[1])},{13,13})&&
        window.host.session.revision()==text_revision,
        "mixed Path/Text Repeater preview applies the same relative rotation through each own operation Ref");
    release(dial,8);auto expected_text_after=before_text_drag;
    const auto text_after=window.host.session.document();const auto text_after_values=evaluate(text_after);
    for(const auto& ref:expected) {
        const auto operation_id=ref.field.substr(3,ref.field.size()-3-std::string(".rotation").size());
        auto& entries=expected_text_after.objects.at(ref.object).stack;
        for(auto& entry:entries)if(entry.id==operation_id)entry.parameters.at("rotation").literal=text_after_values.at(ref);
    }
    check(window.host.session.revision()==text_revision+1&&window.host.session.document()==expected_text_after&&
        near(values(window,expected),{13,13})&&
        window.host.session.document().objects.at("text-b").text->id=="text-source-text-b",
        "mixed Path/Text Repeater drag commits one exact expected document while preserving Text source identity");
#endif

    load(window,{{"five",Kind::path,0,{{1,"five-repeat",5}}},
        {"three-sixty-five",Kind::path,0,{{1,"three-sixty-five-repeat",365}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);numeric=batch_numeric(window,refs);
    check(numeric->text().isEmpty()&&numeric->placeholderText()=="Mixed"&&numeric->property("nect-mixed").toBool()&&
        dial->accessibleName().contains("Mixed",Qt::CaseInsensitive)&&caption(dial)&&
        caption(dial)->text().contains("Mixed")&&caption(dial)->text().contains("top zero"),
        "Repeater batch uses exact equality for Common and keeps 5° versus 365° Mixed with a top-zero caption");

    load(window,{{"first",Kind::path,17,{{0,"first-repeat-one",5},{2,"first-repeat-two",25}}},
        {"second",Kind::path,-3,{{0,"second-repeat-one",5},{2,"second-repeat-two",25}}}});
    auto slot0=rotation_refs(window,0);auto slot2=rotation_refs(window,2);
    auto* dial0=batch_dial(window,slot0);auto* numeric0=batch_numeric(window,slot0);
    auto* dial2=batch_dial(window,slot2);auto* numeric2=batch_numeric(window,slot2);
    check(dial0->property("nect-targets").toByteArray()==numeric0->property("nect-targets").toByteArray()&&
        dial2->property("nect-targets").toByteArray()==numeric2->property("nect-targets").toByteArray()&&
        slot0[0].field!=slot0[1].field&&slot2[0].field!=slot2[1].field&&
        slot0[0].field!=slot2[0].field,
        "multiple same-type stack slots retain each object's own operation IDs without cross-slot retargeting");

    load(window,{{"fill-first",Kind::path,0,{{1,"fill-first-repeat",15}},true},
        {"stroke-second",Kind::path,0,{{1,"stroke-second-repeat",15}}}});
    auto different_stacks=rotation_refs(window,1);auto* different_dial=batch_dial(window,different_stacks);
    check(window.host.session.document().objects.at("fill-first").stack[0].type!="nect.paint.stroke"&&
        window.host.session.document().objects.at("stroke-second").stack[0].type=="nect.paint.stroke"&&
        different_dial->isEnabled(),
        "per-object full stack signatures may differ in unrelated entries while matching the same ordinary Repeater slot");
    const auto different_revision=window.host.session.revision();press(different_dial);move(different_dial,5);release(different_dial,5);
    check(window.host.session.revision()==different_revision+1&&near(values(window,different_stacks),{20,20}),
        "different unrelated stack entries do not block editing the exact common Repeater row");

#ifdef _WIN32
    load(window,{{"path",Kind::path,0,{{1,"path-repeater",0}}},
        {"text",Kind::text,0,{{1,"text-repeater",0}}}}, {"path","text"});
    auto mixed_domain=rotation_refs(window,1);auto* domain_dial=batch_dial(window,mixed_domain);
    replacement=window.host.session.document();replacement.objects.at("text").text->id="replacement-text-source";
    window.host.session=Session(replacement);press(domain_dial);release(domain_dial);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==0&&
        window.host.session.document().objects.at("text").text->id=="replacement-text-source",
        "same-revision replacement of the later Text source identity refuses the captured batch dial");
#endif

    load(window,{{"path-a",Kind::path,0,{{1,"path-a-repeater",5}}},
        {"path-b",Kind::path,0,{{1,"path-b-repeater",5}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);replacement=window.host.session.document();
    auto& stack=replacement.objects.at("path-b").stack;
    auto operation=std::find_if(stack.begin(),stack.end(),[](const auto& item){return item.id=="path-b-repeater";});
    check(operation!=stack.end(),"replacement fixture locates the later ordinary Repeater operation");
    operation->id="path-b-repeater-replacement";window.host.session=Session(replacement);
    press(dial);release(dial);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==0&&
        window.host.session.document().objects.at("path-b").stack[1].id=="path-b-repeater-replacement",
        "same-revision replacement of the later operation ID refuses the old complete Ref vector");

    load(window,{{"path-a",Kind::path,0,{{1,"path-a-repeater",5}}},
        {"path-b",Kind::path,0,{{1,"path-b-repeater",5}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);replacement=window.host.session.document();
    replacement.objects.at("path-b").source->id="replacement-path-source";window.host.session=Session(replacement);
    press(dial);release(dial);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==0&&
        window.host.session.document().objects.at("path-b").source->id=="replacement-path-source",
        "same-revision replacement of the later Path primitive source identity refuses the captured batch");

    load(window,{{"path-a",Kind::path,0,{{1,"path-a-repeater",5}}},
        {"path-b",Kind::path,0,{{1,"path-b-repeater",5}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);replacement=window.host.session.document();
    auto& target_scalar=replacement.objects.at("path-b").stack[1].parameters.at("rotation");
    target_scalar.binding=Binding{refs.front(),1,0,"copy_local_value"};
    window.host.session=Session(replacement);
    check(evaluate(window.host.session.document()).at(refs.back())==5,
        "same-revision Scalar replacement fixture preserves the committed evaluated rotation");
    press(dial);release(dial);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==0&&
        window.host.session.document().objects.at("path-b").stack[1].parameters.at("rotation").binding.has_value(),
        "same-revision Repeater rotation Scalar driver replacement is refused even when its value is unchanged");

    load(window,{{"path-a",Kind::path,0,{{0,"path-a-repeater",5},{2,"path-a-repeater-2",25}}},
        {"path-b",Kind::path,0,{{0,"path-b-repeater",5},{2,"path-b-repeater-2",25}}}});
    refs=rotation_refs(window,0);dial=batch_dial(window,refs);replacement=window.host.session.document();
    auto& ordered=replacement.objects.at("path-b").stack;std::swap(ordered[0],ordered[1]);
    window.host.session=Session(replacement);press(dial);release(dial);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==0,
        "same-revision change to the later object's full ordered stack signature refuses stale Repeater callbacks");

    load(window,{{"has-repeater",Kind::path,0,{{1,"has-repeat",5}}},
        {"missing-repeater",Kind::path,0,{}}});
    check(!has_repeater_dial(window),"a missing later Repeater suppresses the whole-object batch dial");
    load(window,{{"slot-one",Kind::path,0,{{1,"slot-one-repeat",5}}},
        {"slot-two",Kind::path,0,{{2,"slot-two-repeat",5}},true}});
    check(!has_repeater_dial(window),"different Repeater slots do not form a batch target row");
    load(window,{{"repeater",Kind::path,0,{{1,"one-repeat",5}}},
        {"other-type",Kind::path,0,{},false,std::pair<std::size_t,std::string>{1,"other-offset"}}});
    check(!has_repeater_dial(window),"a different operation type at the matching slot suppresses the batch dial");
    load(window,{{"point-a",Kind::path,0,{{1,"point-a-repeat",5}}},
        {"point-b",Kind::path,0,{{1,"point-b-repeat",5}}}});
    const auto& point_document=window.host.session.document();const auto point_values=evaluate(point_document);
    const auto a_contours=path_contours(point_document.objects.at("point-a"),&point_values);
    const auto b_contours=path_contours(point_document.objects.at("point-b"),&point_values);
    window.canvas->set_selections({{"point-a",a_contours.front().points.front().id},
        {"point-b",b_contours.front().points.front().id}});events();
    check(!has_repeater_dial(window),"point selections do not surface the whole-object Repeater dial");
}
void numeric_and_relative_batch(Window& window,const QString& directory) {
    load(window,{{"first",Kind::path,11,{{1,"first-repeat",-355}}},
        {"second",Kind::path,22,{{1,"second-repeat",5}}},
        {"third",Kind::path,33,{{1,"third-repeat",725}}},
        {"unselected",Kind::path,44,{{1,"unselected-repeat",222}}}}, {"first","second","third"});
    auto refs=rotation_refs(window,1);auto* numeric=batch_numeric(window,refs);
    const auto precise=QString::number(-355.1234567890123,'g',17);
    edit(numeric,precise);refs=rotation_refs(window,1);numeric=batch_numeric(window,refs);
    check(numeric->text()==precise&&near(values(window,refs),{-355.1234567890123,-355.1234567890123,-355.1234567890123}),
        "existing batch numeric absolute edit keeps exact round-trip precision across all Repeater Refs");
    refs=rotation_refs(window,1);edit(batch_numeric(window,refs),"+=10");refs=rotation_refs(window,1);
    check(near(values(window,refs),{-345.1234567890123,-345.1234567890123,-345.1234567890123}),
        "existing numeric += semantics remain an absolute one-time edit after a prior absolute value");

    load(window,{{"first",Kind::path,11,{{1,"first-repeat",-355}}},
        {"second",Kind::path,22,{{1,"second-repeat",5}}},
        {"third",Kind::path,33,{{1,"third-repeat",725}}},
        {"unselected",Kind::path,44,{{1,"unselected-repeat",222}}}}, {"first","second","third"});
    refs=rotation_refs(window,1);auto* dial=batch_dial(window,refs);const auto before=window.host.session.document();
    const auto native_before=encode(before);const auto revision=window.host.session.revision();
    press(dial);move(dial,8);auto preview=evaluate(window.host.session.preview_document());
    check(near({preview.at(refs[0]),preview.at(refs[1]),preview.at(refs[2])},{-347,13,733}),
        "first shared delta applies to each frozen Repeater starting value");
    move(dial,12);preview=evaluate(window.host.session.preview_document());
    check(near({preview.at(refs[0]),preview.at(refs[1]),preview.at(refs[2])},{-343,17,737}),
        "later Repeater preview recomputes from the committed snapshot instead of accumulating");
    move(dial,10);preview=evaluate(window.host.session.preview_document());
    check(near({preview.at(refs[0]),preview.at(refs[1]),preview.at(refs[2])},{-345,15,735})&&
        window.host.session.revision()==revision,
        "the exact −355/5/725 starting angles with +8/+12/+10 samples resolve noncumulatively to −345/15/735");
    release(dial,10);const auto after=window.host.session.document();const auto native_after=encode(after);
    auto expected_after=before;const auto committed_values=evaluate(after);
    for(const auto& ref:refs) {
        const auto operation_id=ref.field.substr(3,ref.field.size()-3-std::string(".rotation").size());
        auto& entries=expected_after.objects.at(ref.object).stack;
        for(auto& entry:entries)if(entry.id==operation_id)entry.parameters.at("rotation").literal=committed_values.at(ref);
    }
    check(window.host.session.revision()==revision+1&&near(values(window,refs),{-345,15,735})&&
        after==expected_after&&after.objects.at("unselected")==before.objects.at("unselected"),
        "one Repeater batch drag changes only its exact three rotation Refs and preserves every other authored property");
    window.host.session.undo(window.host.session.revision());window.host.edited();events();
    check(encode(window.host.session.document())==native_before&&window.host.session.revision()==revision+2,
        "one Undo restores the exact pre-gesture native byte representation");
    window.host.session.redo(window.host.session.revision());window.host.edited();events();
    check(encode(window.host.session.document())==native_after&&near(values(window,refs),{-345,15,735}),
        "one Redo restores every Repeater byte and rotation from the batch gesture");
    const auto path=directory+"/repeater-batch-roundtrip.nect";window.host.save(path);
    const auto saved=encode(window.host.session.document());window.host.open(path);events();
    check(encode(window.host.session.document())==saved&&
        window.host.session.document().objects.at("unselected")==before.objects.at("unselected"),
        "native save/reopen preserves exact batch Repeater state and unselected properties");

    load(window,{{"a",Kind::path,9,{{1,"a-repeat",-355}}},{"b",Kind::path,10,{{1,"b-repeat",5}}},
        {"c",Kind::path,11,{{1,"c-repeat",725}}}});
    refs=rotation_refs(window,1);edit(batch_numeric(window,refs),"+=10");refs=rotation_refs(window,1);
    check(near(values(window,refs),{-345,15,735}),
        "numeric += still adjusts each selected Repeater independently and preserves its authored difference");
}
void top_zero_geometry_and_indicator(Window& window) {
    window.resize(1000,2200);
    if(auto* scroll=window.findChild<QWidget*>("inspector-scroll"))scroll->setFixedWidth(300);
    if(auto* dock=window.findChild<QWidget*>("properties"))dock->setFixedWidth(320);
    load(window,{{"first",Kind::path,0,{{1,"first-repeat",0}}},
        {"second",Kind::path,0,{{1,"second-repeat",0}}},
        {"third",Kind::path,0,{{1,"third-repeat",0}}}});
    auto refs=rotation_refs(window,1);auto* common=batch_dial(window,refs);
    const auto zero=rendered_indicator(window,common);
    check(zero.y()<-2&&std::abs(zero.x())<2,
        "zero-degree Repeater batch indicator is visibly at the top rather than local +X");
    check(caption(common)&&caption(common)->text().contains("top zero")&&!caption(common)->text().contains("+X zero")&&
        common->accessibleName().contains("top zero")&&common->toolTip().contains("top")&&
        common->accessibleDescription().contains("Zero points up at the top")&&
        common->accessibleDescription().contains("clockwise")&&common->accessibleDescription().contains("modulo 360")&&
        !common->accessibleDescription().contains("Zero points right along local +X"),
        "caption, tooltip and accessibility consistently identify top-zero clockwise modulo-only Repeater orientation");
    const QPoint common_center=common->mapToGlobal(QPoint(common->width()/2,common->height()/2));
    const auto common_dial_geometry=common->geometry();const auto common_row_geometry=common->parentWidget()->geometry();
    check(batch_angle_test::caption_has_reserved_width(caption(common)),
        "Repeater-batch caption reserves a content-independent minimum before interaction");
    const batch_angle_test::Geometry common_stable(common);
    mouse_global(common,QEvent::MouseButtonPress,QPointF(common_center)+QPointF(0,-16),Qt::LeftButton,Qt::LeftButton);
    mouse_global(common,QEvent::MouseMove,QPointF(common_center)+QPointF(8,-14),Qt::NoButton,Qt::LeftButton);
    const auto* common_note=caption(common);const auto common_delta=caption_delta(common_note);
    const auto common_exact_delta=accessible_delta(common);bool common_exact_ok=false;
    const auto common_exact_value=common_exact_delta.toDouble(&common_exact_ok);
    check(common_stable.stable(common)&&common->geometry()==common_dial_geometry&&common->parentWidget()->geometry()==common_row_geometry&&
        common->mapToGlobal(QPoint(common->width()/2,common->height()/2))==common_center&&
        common_note&&common_note->text().contains("Common · 3")&&common_note->text().contains("top zero")&&
        common_note->text().contains("Δ "+common_delta+"°")&&common_note->accessibleName().contains("3 objects")&&
        common_note->accessibleName().contains(QString("relative delta %1 degrees").arg(common_exact_delta))&&
        common_exact_ok&&common_exact_delta.size()>=17&&common_exact_value>29.7&&common_exact_value<29.8&&
        batch_angle_test::caption_fits(common_note)&&
        common_note->heightForWidth(common_note->width())<=common_note->height(),
        "Common top-zero live caption keeps a fixed center, bounded delta, exact accessible delta and fits its reserved row");
    check(batch_angle_test::narrow_caption_samples_fit(common_note),
        "Measured three-line Common/Mixed captions fit an explicit 98×80px label width with the active UI font");
    QTest::keyClick(common,Qt::Key_Escape);events();
    check(!window.host.session.gesture_active()&&near(values(window,refs),{0,0,0}),
        "Escape restores the Common top-zero caption baseline after the live-fit sample");

    load(window,{{"first",Kind::path,0,{{1,"first-repeat",-355}}},
        {"second",Kind::path,0,{{1,"second-repeat",5}}},
        {"third",Kind::path,0,{{1,"third-repeat",725}}}});
    refs=rotation_refs(window,1);auto* dial=batch_dial(window,refs);
    const QPoint center=dial->mapToGlobal(QPoint(dial->width()/2,dial->height()/2));
    const QRect dial_geometry=dial->geometry();const QRect row_geometry=dial->parentWidget()->geometry();
    const int row_height=dial->parentWidget()->height();
    check(common_stable.stable(dial),
        "Common and Mixed Repeater captions keep identical global row and dial allocations");
    const batch_angle_test::Geometry mixed_stable(dial);
    mouse_global(dial,QEvent::MouseButtonPress,QPointF(center)+QPointF(0,-16),Qt::LeftButton,Qt::LeftButton);
    for(const auto& vector:{QPointF(8,-14),QPointF(14,-8),QPointF(16,0)}) {
        mouse_global(dial,QEvent::MouseMove,QPointF(center)+vector,Qt::NoButton,Qt::LeftButton);
        const auto center_after=dial->mapToGlobal(QPoint(dial->width()/2,dial->height()/2));
        check(mixed_stable.stable(dial)&&dial->geometry()==dial_geometry&&dial->parentWidget()->geometry()==row_geometry&&center_after==center,
            "fixed-global top-to-right arc keeps the dial, row, and hit target center stable");
        const auto* note=caption(dial);const auto delta=caption_delta(note);const auto content_height=note?note->heightForWidth(note->width()):0;
        check(note&&note->text().contains("top zero")&&!note->text().contains("+X zero")&&
            !delta.isEmpty()&&delta.startsWith('+')&&delta.size()<=10&&
            batch_angle_test::caption_fits(note)&&content_height<=note->height(),
            "top-zero narrow row displays its bounded signed delta without clipping");
    }
    const auto preview=evaluate(window.host.session.preview_document());const auto delta=caption_delta(caption(dial));
    const auto full_delta=accessible_delta(dial);bool delta_ok=false;const auto delta_value=full_delta.toDouble(&delta_ok);
    check(near({preview.at(refs[0]),preview.at(refs[1]),preview.at(refs[2])},{-265,95,815})&&
        delta=="+90"&&delta_ok&&delta_value==90&&dial->accessibleDescription().contains("relative delta +90 degrees")&&
        dial->accessibleDescription().contains("modulo 360"),
        "fixed-global top-to-right quarter turn is exact +90 and text/accessibility preserve the modulo-only indicator rule");
    mouse_global(dial,QEvent::MouseButtonRelease,QPointF(center)+QPointF(16,0),Qt::LeftButton,Qt::NoButton);
    check(near(values(window,refs),{-265,95,815}),
        "top-to-right Repeater batch gesture commits the unwrapped +90-degree result after the fixed-height live arc");
    check(batch_angle_test::wider_inspector_caption_expands(batch_dial(window,refs)),
        "caption expands in a wider Inspector without changing its reserved minimum or compact height");
    if(auto* scroll=window.findChild<QWidget*>("inspector-scroll")) {scroll->setMinimumWidth(300);scroll->setMaximumWidth(QWIDGETSIZE_MAX);}
    if(auto* dock=window.findChild<QWidget*>("properties")) {dock->setMinimumWidth(0);dock->setMaximumWidth(QWIDGETSIZE_MAX);}
    window.resize(1400,900);events();
}
void exact_boundary_values(Window& window) {
    for(const auto initial:{-1e9,1e9}) {
        load(window,{{"a",Kind::path,0,{{1,"a-repeat",initial}}},
            {"b",Kind::path,0,{{1,"b-repeat",initial}}}});
        auto refs=rotation_refs(window,1);auto* dial=batch_dial(window,refs);auto* numeric=batch_numeric(window,refs);
        const auto exact=QString::number(initial,'g',17);double modulo=std::fmod(initial,360.0);
        if(modulo<0)modulo+=360.0;
        const auto indicator=QString::number(modulo,'g',15);
        check(numeric->text()==exact&&dial->accessibleDescription().contains("current unwrapped angle "+exact+" degrees")&&
            dial->accessibleDescription().contains("Dial indicator "+indicator+" degrees")&&
            dial->accessibleDescription().contains("indicator is modulo 360")&&
            values(window,refs)==std::vector<double>({initial,initial}),
            "Repeater batch retains exact authored ±1e9 values while its accessible indicator is only modulo 360");
        const auto revision=window.host.session.revision();press(dial);release(dial);
        check(window.host.session.revision()==revision&&values(window,refs)==std::vector<double>({initial,initial}),
            "no-motion at either authored Repeater angle boundary creates no revision or normalization");
    }
}
void no_net_full_turn_and_range(Window& window,const QString& directory) {
    load(window,{{"a",Kind::path,0,{{1,"a-repeat",-355}}},{"b",Kind::path,0,{{1,"b-repeat",5}}},
        {"c",Kind::path,0,{{1,"c-repeat",725}}}});
    auto refs=rotation_refs(window,1);auto* dial=batch_dial(window,refs);const auto bytes=encode(window.host.session.document());
    const auto revision=window.host.session.revision();press(dial);release(dial,0);
    check(window.host.session.revision()==revision&&!window.host.session.gesture_active()&&
        encode(window.host.session.document())==bytes,"no-motion Repeater batch click cancels without an empty history command");
    dial=batch_dial(window,refs);press(dial);move(dial,8);move(dial,0);release(dial,0);
    check(window.host.session.revision()==revision&&!window.host.session.gesture_active()&&
        encode(window.host.session.document())==bytes,"Repeater batch out-and-back motion leaves no preview or ghost revision");
    dial=batch_dial(window,refs);press(dial);move(dial,25);QTest::keyClick(dial,Qt::Key_Escape);events();
    check(window.host.session.revision()==revision&&!window.host.session.gesture_active()&&
        encode(window.host.session.document())==bytes,"Escape atomically cancels a Repeater batch preview");

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",-355}}},{"b",Kind::path,0,{{1,"b-repeat",5}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);const auto turn_revision=window.host.session.revision();press(dial);
    for(int degrees=45;degrees<=360;degrees+=45)move(dial,degrees);release(dial,360);
    check(window.host.session.revision()==turn_revision+1&&near(values(window,refs),{5,365}),
        "a real +360 Repeater batch turn commits without normalization");
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);const auto reverse_revision=window.host.session.revision();press(dial);
    for(int degrees=-45;degrees>=-360;degrees-=45)move(dial,degrees);release(dial,-360);
    check(window.host.session.revision()==reverse_revision+1&&near(values(window,refs),{-355,5}),
        "a real −360 Repeater batch turn commits without normalization");

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",0}}},{"b",Kind::path,0,{{1,"b-repeat",999999900}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);const auto range_bytes=encode(window.host.session.document());
    const auto range_revision=window.host.session.revision();press(dial);move(dial,8);
    check(window.host.session.gesture_active()&&near(values_from_document(window.host.session.preview_document(),refs),{8,999999908}),
        "a later-target range fixture admits a shared-delta preview while all targets remain valid");
    move(dial,180);release(dial,180);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==range_revision&&
        encode(window.host.session.document())==range_bytes&&values(window,refs)==std::vector<double>({0,999999900}),
        "one later-target Repeater range failure cancels the entire batch without leaking an earlier preview");
    const auto saved_path=directory+"/repeater-batch-failed-preview.nect";window.host.save(saved_path);
    const auto saved_bytes=encode(window.host.session.document());window.host.open(saved_path);events();
    check(encode(window.host.session.document())==saved_bytes&&saved_bytes==range_bytes,
        "save after a failed Repeater batch preview contains committed bytes only");
}
std::vector<double> values_from_document(const Document& document,const std::vector<Ref>& refs) {
    const auto evaluated=evaluate(document);std::vector<double> result;
    for(const auto& ref:refs)result.push_back(evaluated.at(ref));return result;
}
void driven_state_and_numeric_draft(Window& window) {
    load(window,{{"a",Kind::path,0,{{1,"a-repeat",-20,false}}},
        {"b",Kind::path,0,{{1,"b-repeat",40,false}}}});
    auto refs=rotation_refs(window,1);auto* dial=batch_dial(window,refs);
    check(dial->isEnabled(),"disabled ordinary Repeater operations remain editable through their rotation dial");
    const auto revision=window.host.session.revision();press(dial);move(dial,10);release(dial,10);
    check(window.host.session.revision()==revision+1&&near(values(window,refs),{-10,50}),
        "editing a disabled ordinary Repeater batch changes only its rotation parameters");

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",-20}}},
        {"b",Kind::path,0,{{1,"b-repeat",40}}},
        {"driver",Kind::path,0,{{1,"driver-repeat",80}}}}, {"a","b"});
    refs=rotation_refs(window,1);const auto source=operation_ref("driver","driver-repeat","enabled");
    const auto enabled=operation_ref("b","b-repeat","enabled");
    window.host.session.apply({LinkOperationEnabled{enabled,source,false}},window.host.session.revision());
    window.host.edited();events();refs=rotation_refs(window,1);dial=batch_dial(window,refs);
    check(dial->isEnabled(),"an enabled-state driver alone does not make the ordinary rotation Scalar driven");
    const auto enabled_revision=window.host.session.revision();press(dial);move(dial,10);release(dial,10);
    check(window.host.session.revision()==enabled_revision+1&&near(values(window,refs),{-10,50}),
        "enabled-state-driven Repeater remains editable with its rotation Refs unaffected by that driver");

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",-20}}},
        {"b",Kind::path,0,{{1,"b-repeat",40}}}});
    refs=rotation_refs(window,1);const Ref driver{refs.front().object,"","op.a-repeat.rotation"};
    window.host.session.apply({Link{refs.back(),Binding{driver,1,0,"copy_local_value"}}},window.host.session.revision());
    window.host.edited();events();refs=rotation_refs(window,1);dial=batch_dial(window,refs);
    check(!dial->isEnabled(),"a binding on the later selected Repeater rotation disables the whole batch dial");
    const auto bound_bytes=encode(window.host.session.document());const auto bound_revision=window.host.session.revision();press(dial);release(dial);
    check(!window.host.session.gesture_active()&&encode(window.host.session.document())==bound_bytes&&
        window.host.session.revision()==bound_revision,"later-target driven binding refuses before opening any gesture");

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",-20}}},
        {"b",Kind::path,0,{{1,"b-repeat",40}}}});
    refs=rotation_refs(window,1);window.host.session.apply({SetExpression{{refs.back()},Expression{"30",1}},
        },window.host.session.revision());window.host.edited();events();refs=rotation_refs(window,1);dial=batch_dial(window,refs);
    check(!dial->isEnabled()&&evaluate(window.host.session.document()).at(refs.back())==30,
        "a later-target Repeater rotation expression disables the complete batch dial");

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",-20}}},{"b",Kind::path,0,{{1,"b-repeat",40}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);auto* numeric=batch_numeric(window,refs);
    const auto before=encode(window.host.session.document());const auto draft_revision=window.host.session.revision();
    numeric->setText("+=15");numeric->setModified(true);press(dial);release(dial);
    check(!window.host.session.gesture_active()&&numeric->isModified()&&numeric->text()=="+=15"&&
        encode(window.host.session.document())==before&&window.host.session.revision()==draft_revision&&
        window.statusBar()->currentMessage().startsWith("UNCOMMITTED_INPUT"),
        "dirty numeric relative draft refuses the batch dial without being lost or committed");
    int finished=0;QObject signal_scope;QObject::connect(numeric,&QLineEdit::editingFinished,&signal_scope,[&]{++finished;});
    numeric->setFocus();events();const auto focus_revision=window.host.session.revision();
    window.canvas->set_selections({{"a",{}}});events();
    check(finished==1&&window.host.session.revision()==focus_revision+1&&near(values(window,refs),{-5,55})&&
        !window.host.session.gesture_active(),
        "selection change commits a focused dirty batch numeric edit exactly once through its ordinary Session path");
    window.canvas->set_selections({{"a",{}},{"b",{}}});events();
}
void busy_aba_rebuild_and_close(Window& window) {
    load(window,{{"a",Kind::path,0,{{1,"a-repeat",12}}},{"b",Kind::path,0,{{1,"b-repeat",34}}}});
    auto refs=rotation_refs(window,1);auto* dial=batch_dial(window,refs);const auto revision=window.host.session.revision();
    window.host.session.begin_gesture(revision);window.host.session.update_gesture({EditProperties{{refs.front()},222,false}});
    const auto foreign=window.host.session.preview_document();press(dial);release(dial);
    check(window.host.session.gesture_active()&&window.host.session.preview_document()==foreign,
        "busy foreign gesture cannot be stolen, committed, or canceled by the Repeater batch dial");
    window.host.session.cancel_gesture();

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",12}}},{"b",Kind::path,0,{{1,"b-repeat",34}}}});
    refs=rotation_refs(window,1);QPointer<QWidget> old=batch_dial(window,refs);const auto stale_revision=window.host.session.revision();
    press(old);move(old,8);check(window.host.session.gesture_active(),"old Repeater batch owns gesture A before same-revision replacement");
    window.host.session.cancel_gesture();window.host.session.begin_gesture(stale_revision);
    window.host.session.update_gesture({EditProperties{{refs.front()},321,false}});
    const auto replacement_preview=window.host.session.preview_document();move(old,15);release(old,15);QTest::keyClick(old,Qt::Key_Escape);events();
    check(window.host.session.gesture_active()&&window.host.session.preview_document()==replacement_preview,
        "stale Repeater preview, release, and Escape callbacks cannot update or cancel replacement gesture B");
    window.host.session.cancel_gesture();

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",12}}},{"b",Kind::path,0,{{1,"b-repeat",34}}}});
    refs=rotation_refs(window,1);old=batch_dial(window,refs);const auto before=encode(window.host.session.document());
    press(old);move(old,10);window.canvas->set_selections({{"a",{}}});events();
    check(!window.host.session.gesture_active()&&old&&!old->isVisible()&&encode(window.host.session.document())==before,
        "Inspector rebuild disposes and cancels an active Repeater batch before the selection changes");
    press(old);release(old,10);check(!window.host.session.gesture_active()&&encode(window.host.session.document())==before,
        "disposed Repeater batch control cannot begin or mutate a later gesture");
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);events();check(old.isNull(),"deferred Inspector teardown deletes the old Repeater batch dial");
    window.canvas->set_selections({{"a",{}},{"b",{}}});events();refs=rotation_refs(window,1);dial=batch_dial(window,refs);
    press(dial);check(window.host.session.gesture_active(),"rebuilt Repeater batch control is reusable");QTest::keyClick(dial,Qt::Key_Escape);events();

    load(window,{{"a",Kind::path,0,{{1,"a-repeat",12}}},{"b",Kind::path,0,{{1,"b-repeat",34}}}});
    refs=rotation_refs(window,1);dial=batch_dial(window,refs);const auto close_bytes=encode(window.host.session.document());
    press(dial);move(dial,10);window.close();events();
    check(!window.host.session.gesture_active()&&encode(window.host.session.document())==close_bytes,
        "Window close synchronously cancels the active Repeater batch before flushing the Host");
    window.show();events();refs=rotation_refs(window,1);dial=batch_dial(window,refs);const auto after_close=window.host.session.revision();
    press(dial);move(dial,5);release(dial,5);
    check(window.host.session.revision()==after_close+1,"surviving Inspector can edit through a fresh Repeater batch after close/reopen");
}
}
int main(int argc,char** argv) {
    batch_angle_test::configure_test_qpa();QApplication app(argc,argv);batch_angle_test::report_test_qpa();
    try {
        QTemporaryDir directory;check(directory.isValid(),"temporary directory is available");
        QSettings settings(directory.filePath("settings.ini"),QSettings::IniFormat);
        Window window(directory.path(),std::make_unique<FolderLibrary>(settings));window.show();events();
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")
            check(window.devicePixelRatioF()>=1.9,"high-DPI Repeater batch run uses an actual scaled widget DPR");
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="1.25")
            check(window.devicePixelRatioF()>=1.2,"fractional-DPI Repeater batch run uses an actual scaled widget DPR");
        if(qEnvironmentVariableIsSet("NECT_GEOMETRY_ONLY")) {
            top_zero_geometry_and_indicator(window);std::cout<<"PASS fixed-global Repeater batch angle geometry\n";return 0;
        }
        discovery_and_exact_refs(window);
        numeric_and_relative_batch(window,directory.path());
        top_zero_geometry_and_indicator(window);
        exact_boundary_values(window);
        no_net_full_turn_and_range(window,directory.path());
        driven_state_and_numeric_draft(window);
        busy_aba_rebuild_and_close(window);
        std::cout<<"PASS "<<checks<<" Repeater batch angle checks\n";return 0;
    } catch(const Error& error) {std::cerr<<"FAIL "<<error.code<<": "<<error.what()<<'\n';return 1;}
      catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
