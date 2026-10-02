#include "window.hpp"
#include "batch_angle_geometry.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
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
QByteArray ref_json(const Ref& ref) {
    return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
        {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}})
        .toJson(QJsonDocument::Compact);
}
QByteArray refs_json(const std::vector<Ref>& refs) {
    QJsonArray values;for(const auto& ref:refs)values.append(QJsonDocument::fromJson(ref_json(ref)).object());
    return QJsonDocument(values).toJson(QJsonDocument::Compact);
}
struct PrimitiveSpec {std::string object,type;double rotation=0;};
void load(Window& window,const std::vector<PrimitiveSpec>& specs,const std::vector<std::string>& selection={}) {
    auto document=empty_document("primitive-batch-document","primitive-batch-composition","primitive-batch-artboard");
    Session setup(document);std::vector<Command> commands;
    for(const auto& spec:specs) {
        auto source=default_primitive("source-"+spec.object,spec.type);
        const bool has_rotation=source.parameters.contains("rotation");
        commands.push_back(CreatePrimitive{"primitive-batch-composition","",spec.object,spec.object,source});
        if(has_rotation)commands.push_back(Set{{spec.object,"","generator.rotation"},spec.rotation});
    }
    setup.apply(commands,setup.revision());
    window.host.session=Session(setup.document());window.host.session_id+="-batch-primitive";window.host.edited();
    auto objects=selection;
    if(objects.empty())for(const auto& spec:specs)objects.push_back(spec.object);
    std::vector<Canvas::Selection> chosen;for(const auto& id:objects)chosen.push_back({id,{}});
    window.canvas->set_selections(chosen);events();
}
std::vector<Ref> selected_rotation_refs(Window& window) {
    std::vector<Ref> refs;for(const auto& selected:window.canvas->selections())refs.push_back({selected.object,"","generator.rotation"});return refs;
}
QWidget* visible_batch_dial(Window& window,const std::vector<Ref>& targets) {
    const auto data=refs_json(targets);
    for(auto* widget:window.findChildren<QWidget*>())
        if(widget->isVisible()&&widget->objectName()=="batch-primitive-angle-knob"&&
           widget->property("nect-targets").toByteArray()==data)return widget;
    throw std::runtime_error("visible whole-object Polygon/Star batch dial exists on the exact full target vector");
}
QString caption_delta(const QLabel* caption) {
    if(!caption)return {};
    const auto marker=QStringLiteral("Δ ");const auto start=caption->text().indexOf(marker);
    if(start<0)return {};
    const auto value_start=start+marker.size();const auto end=caption->text().indexOf(QStringLiteral("°"),value_start);
    return end<0?QString{}:caption->text().mid(value_start,end-value_start);
}
QString accessible_delta(const QWidget* dial) {
    if(!dial)return {};
    const auto marker=QStringLiteral("relative delta ");const auto start=dial->accessibleDescription().indexOf(marker);
    if(start<0)return {};
    const auto value_start=start+marker.size();const auto end=dial->accessibleDescription().indexOf(QStringLiteral(" degrees"),value_start);
    return end<0?QString{}:dial->accessibleDescription().mid(value_start,end-value_start);
}
bool has_visible_batch_dial(Window& window) {
    for(auto* widget:window.findChildren<QWidget*>())
        if(widget->isVisible()&&widget->objectName()=="batch-primitive-angle-knob")return true;
    return false;
}
QLineEdit* batch_numeric(Window& window,const std::vector<Ref>& targets) {
    const auto data=refs_json(targets);
    for(auto* input:window.findChildren<QLineEdit*>())
        if(input->isVisible()&&input->property("nect-targets").toByteArray()==data)return input;
    throw std::runtime_error("existing common source-parameter row is visible on the exact full target vector");
}
QPointF knob_point(double angle) {
    const auto radians=angle*std::acos(-1.0)/180.0;
    return {22+16*std::cos(radians),22+16*std::sin(radians)};
}
void mouse(QWidget* control,QEvent::Type type,double angle,Qt::MouseButton button,Qt::MouseButtons buttons) {
    const auto local=knob_point(angle);
    QMouseEvent event(type,local,QPointF(control->mapToGlobal(QPoint(0,0)))+local,button,buttons,Qt::NoModifier);
    QApplication::sendEvent(control,&event);events();
}
void press(QWidget* control) {mouse(control,QEvent::MouseButtonPress,0,Qt::LeftButton,Qt::LeftButton);}
void move(QWidget* control,double angle) {mouse(control,QEvent::MouseMove,angle,Qt::NoButton,Qt::LeftButton);}
void release(QWidget* control,double angle) {mouse(control,QEvent::MouseButtonRelease,angle,Qt::LeftButton,Qt::NoButton);}
void mouse_global(QWidget* control,QEvent::Type type,const QPointF& global,Qt::MouseButton button,Qt::MouseButtons buttons) {
    const QPointF local(control->mapFromGlobal(global.toPoint()));
    QMouseEvent event(type,local,global,button,buttons,Qt::NoModifier);
    QApplication::sendEvent(control,&event);events();
}
void edit(QLineEdit* input,const QString& text) {
    input->setText(text);input->setModified(true);input->editingFinished();events();
}
std::vector<double> evaluated(Window& window,const std::vector<Ref>& refs) {
    const auto values=evaluate(window.host.session.document());std::vector<double> result;
    for(const auto& ref:refs)result.push_back(values.at(ref));return result;
}
bool near_values(const std::vector<double>& actual,const std::vector<double>& expected,double tolerance=1e-8) {
    if(actual.size()!=expected.size())return false;
    for(std::size_t i=0;i<actual.size();++i)if(std::abs(actual[i]-expected[i])>tolerance)return false;
    return true;
}
void fixed_global_mixed_geometry(Window& window) {
    window.resize(1000,650);
    if(auto* scroll=window.findChild<QWidget*>("inspector-scroll"))scroll->setFixedWidth(300);
    if(auto* dock=window.findChild<QWidget*>("properties"))dock->setFixedWidth(320);
    events();
    load(window,{{"poly","nect.shape.polygon",45},{"star","nect.shape.star",45},{"poly-2","nect.shape.polygon",45}});
    QTest::qWait(80);window.grab();events();
    const auto common_targets=selected_rotation_refs(window);auto* common_dial=visible_batch_dial(window,common_targets);
    const QPoint common_center=common_dial->mapToGlobal(QPoint(common_dial->width()/2,common_dial->height()/2));
    const int common_row_height=common_dial->parentWidget()->height();
    const auto common_geometry=common_dial->geometry();const auto common_row_geometry=common_dial->parentWidget()->geometry();
    const batch_angle_test::Geometry common_stable(common_dial);
    mouse_global(common_dial,QEvent::MouseButtonPress,QPointF(common_center)+QPointF(16,0),Qt::LeftButton,Qt::LeftButton);
    mouse_global(common_dial,QEvent::MouseMove,QPointF(common_center)+QPointF(14,8),Qt::NoButton,Qt::LeftButton);
    const auto* common_caption=common_dial->parentWidget()->findChild<QLabel*>();
    const auto common_delta=caption_delta(common_caption);
    const auto exact_common_delta=accessible_delta(common_dial);bool exact_common_ok=false,visible_common_ok=false;
    const auto exact_common_value=exact_common_delta.toDouble(&exact_common_ok);
    const auto visible_common_value=common_delta.mid(1).toDouble(&visible_common_ok);
    check(common_stable.stable(common_dial)&&common_dial->geometry()==common_geometry&&common_dial->parentWidget()->geometry()==common_row_geometry&&
        common_dial->mapToGlobal(QPoint(common_dial->width()/2,common_dial->height()/2))==common_center&&
        common_caption&&common_caption->text().contains("+X zero")&&common_caption->text().contains("Δ ")&&
        !common_delta.isEmpty()&&common_delta.startsWith('+')&&common_delta.size()<=10&&
        exact_common_ok&&visible_common_ok&&exact_common_delta.size()>=17&&
        std::abs(exact_common_value-visible_common_value)<1e-5&&
        common_caption->accessibleName().contains("3 objects")&&
        common_caption->accessibleName().contains(QString("relative delta %1 degrees").arg(exact_common_delta))&&
        common_dial->accessibleDescription().contains("modulo 360")&&
        batch_angle_test::caption_fits(common_caption)&&
        common_caption&&common_caption->heightForWidth(common_caption->width())<=common_caption->height(),
        "Common caption keeps a stable center, shows a bounded signed delta and preserves the exact long delta accessibly");
    check(batch_angle_test::narrow_caption_samples_fit(common_caption),
        "shared primitive-batch caption samples fit 98×80px with the active UI font metrics");
    QTest::keyClick(common_dial,Qt::Key_Escape);events();
    check(!window.host.session.gesture_active()&&batch_numeric(window,common_targets)->text()=="45"&&
        common_dial->accessibleDescription().contains("current unwrapped angle 45 degrees"),
        "Common caption and accessibility restore the committed exact value after Escape");
    load(window,{{"poly","nect.shape.polygon",5},{"star","nect.shape.star",365},{"poly-2","nect.shape.polygon",725}});
    QTest::qWait(80);window.grab();events();
    const auto targets=selected_rotation_refs(window);auto* dial=visible_batch_dial(window,targets);
    const auto dial_geometry=dial->geometry();const auto row_geometry=dial->parentWidget()->geometry();
    const QPoint center_global=dial->mapToGlobal(QPoint(dial->width()/2,dial->height()/2));
    if(center_global!=common_center||dial->parentWidget()->height()!=common_row_height)
        std::cerr<<"GEOMETRY common row-height="<<common_row_height<<" center="<<common_center.x()<<","<<common_center.y()
            <<" mixed row-height="<<dial->parentWidget()->height()<<" center="<<center_global.x()<<","<<center_global.y()<<"\n";
    check(common_stable.stable(dial)&&center_global==common_center&&dial->parentWidget()->height()==common_row_height,
        "Common and Mixed batch caption content keeps the same fixed row height and dial center");
    check(dial_geometry.width()==44&&dial_geometry.height()==44,
        "whole-object batch dial keeps its fixed 44-pixel hit geometry");
    const batch_angle_test::Geometry mixed_stable(dial);
    mouse_global(dial,QEvent::MouseButtonPress,QPointF(center_global)+QPointF(16,0),Qt::LeftButton,Qt::LeftButton);
    for(const auto& vector:{QPointF(14,8),QPointF(8,14),QPointF(0,16)}) {
        mouse_global(dial,QEvent::MouseMove,QPointF(center_global)+vector,Qt::NoButton,Qt::LeftButton);
        const auto center_after=dial->mapToGlobal(QPoint(dial->width()/2,dial->height()/2));
        if(dial->geometry()!=dial_geometry||dial->parentWidget()->geometry()!=row_geometry||center_after!=center_global)
            std::cerr<<"GEOMETRY before="<<dial_geometry.x()<<","<<dial_geometry.y()<<" "<<dial_geometry.width()<<"x"<<dial_geometry.height()
                <<" row="<<row_geometry.x()<<","<<row_geometry.y()<<" "<<row_geometry.width()<<"x"<<row_geometry.height()
                <<" center="<<center_global.x()<<","<<center_global.y()<<" after="<<dial->geometry().x()<<","<<dial->geometry().y()
                <<" row="<<dial->parentWidget()->geometry().x()<<","<<dial->parentWidget()->geometry().y()<<" "
                <<dial->parentWidget()->geometry().width()<<"x"<<dial->parentWidget()->geometry().height()
                <<" center="<<center_after.x()<<","<<center_after.y()<<"\n";
        check(mixed_stable.stable(dial)&&dial->geometry()==dial_geometry&&dial->parentWidget()->geometry()==row_geometry&&
            center_after==center_global,
            "live Mixed caption updates leave the dial, row and global center fixed in a narrow Inspector");
        const auto* live_caption=dial->parentWidget()->findChild<QLabel*>();
        const auto visible_delta=caption_delta(live_caption);
        const auto exact_delta=accessible_delta(dial);bool exact_ok=false,visible_ok=false;
        const auto exact_value=exact_delta.toDouble(&exact_ok);
        const auto visible_value=visible_delta.mid(1).toDouble(&visible_ok);
        const auto content_height=live_caption?live_caption->heightForWidth(live_caption->width()):0;
        if(content_height>(live_caption?live_caption->height():0))
            std::cerr<<"CAPTION content-height="<<content_height<<" allocated="<<live_caption->height()<<" width="<<live_caption->width()<<"\n";
        const bool non_cardinal=vector.x()!=0&&vector.y()!=0;
        check(live_caption&&live_caption->text().contains("+X zero")&&
            !visible_delta.isEmpty()&&visible_delta.startsWith('+')&&
            visible_delta.size()<=(non_cardinal?10:3)&&exact_ok&&visible_ok&&
            (!non_cardinal||exact_delta.size()>=17)&&std::abs(exact_value-visible_value)<1e-5&&
            batch_angle_test::caption_fits(live_caption)&&content_height<=live_caption->height(),
            "fixed Mixed caption shows its bounded live delta without clipping and retains the exact value accessibly");
    }
    const auto preview_values=evaluate(window.host.session.preview_document());
    std::vector<double> preview;for(const auto& ref:targets)preview.push_back(preview_values.at(ref));
    const auto* caption=dial->parentWidget()->findChild<QLabel*>();
    check(near_values(preview,{95,455,815})&&
        caption&&caption->text().contains("Δ +90°")&&
        dial->accessibleDescription().contains("relative delta +90 degrees"),
        "fixed global non-cardinal quarter-turn remains exactly +90° while its signed live caption and accessibility update");
    mouse_global(dial,QEvent::MouseButtonRelease,QPointF(center_global)+QPointF(0,16),Qt::LeftButton,Qt::NoButton);
    check(near_values(evaluated(window,targets),{95,455,815}),
        "fixed global mixed-angle gesture commits the expected common quarter-turn");
    if(auto* scroll=window.findChild<QWidget*>("inspector-scroll")) {
        scroll->setMinimumWidth(300);scroll->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    if(auto* dock=window.findChild<QWidget*>("properties")) {
        dock->setMinimumWidth(0);dock->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    window.resize(1400,900);events();
}
void exact_dial_discovery(Window& window) {
    load(window,{{"poly-a","nect.shape.polygon",5},{"poly-b","nect.shape.polygon",5}});
    auto targets=selected_rotation_refs(window);auto* dial=visible_batch_dial(window,targets);auto* numeric=batch_numeric(window,targets);
    check(targets.size()==2&&targets[0].field=="generator.rotation"&&targets[1].field=="generator.rotation",
        "same-type Polygon object selection uses the existing exact rotation field only");
    check(dial->property("nect-targets").toByteArray()==refs_json(targets)&&
        dial->property("nect-reference").toByteArray()==ref_json(targets.front())&&
        numeric->property("nect-targets").toByteArray()==refs_json(targets),
        "dial and pre-existing numeric row share the complete ordered Ref vector and exact first Ref");
    check(numeric->text()=="5"&&!numeric->property("nect-mixed").toBool(),
        "common Polygon rotations retain the ordinary numeric row with exact value");
    load(window,{{"poly","nect.shape.polygon",5},{"star","nect.shape.star",365}});
    targets=selected_rotation_refs(window);dial=visible_batch_dial(window,targets);numeric=batch_numeric(window,targets);
    check(targets.size()==2&&dial->accessibleName().contains("2 objects")&&
        dial->accessibleName().contains("mixed",Qt::CaseInsensitive)&&numeric->text().isEmpty()&&
        numeric->placeholderText()=="Mixed"&&numeric->property("nect-mixed").toBool(),
        "mixed Polygon and Star rotations expose the same-row dial and remain exactly Mixed, including 5° versus 365°");
    load(window,{{"poly","nect.shape.polygon",725.1234567890123},{"star","nect.shape.star",725.1234567890123}});
    targets=selected_rotation_refs(window);dial=visible_batch_dial(window,targets);numeric=batch_numeric(window,targets);
    const auto precision=QString::number(725.1234567890123,'g',17);
    check(numeric->text()==precision&&dial->accessibleDescription().contains("current unwrapped angle "+precision+" degrees"),
        "common batch rotation displays and exposes its exact 17-digit unwrapped value");
    load(window,{{"poly","nect.shape.polygon",0},{"ellipse","nect.shape.ellipse",0}});
    check(!has_visible_batch_dial(window),
        "source types outside Polygon/Star do not get a Polygon/Star batch dial");
    load(window,{{"poly","nect.shape.polygon",0},{"star","nect.shape.star",0},{"circle","nect.shape.circle",0}},
        {"poly","star"});
    targets=selected_rotation_refs(window);check(targets.size()==2&&has_visible_batch_dial(window),
        "only the two selected whole objects are captured when another primitive is not selected");
    auto document=window.host.session.document();const auto values=evaluate(document);
    const auto poly_contours=path_contours(document.objects.at("poly"),&values);
    const auto star_contours=path_contours(document.objects.at("star"),&values);
    window.canvas->set_selections({{"poly",poly_contours.front().points.front().id},{"star",star_contours.front().points.front().id}});events();
    check(!has_visible_batch_dial(window),"selected points do not surface the whole-object Polygon/Star dial");
}
void shared_delta_history_and_numeric(Window& window,const QString& directory) {
    load(window,{{"poly","nect.shape.polygon",-355},{"star","nect.shape.star",5},{"poly-2","nect.shape.polygon",725}});
    auto targets=selected_rotation_refs(window);auto* dial=visible_batch_dial(window,targets);auto* numeric=batch_numeric(window,targets);
    if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")
        check(dial->devicePixelRatioF()>=1.9,"actual primitive-batch knob uses high-DPI widget device scale");
    const auto before=window.host.session.document();const auto bytes=encode(before);const auto revision=window.host.session.revision();
    press(dial);move(dial,8);auto preview=evaluate(window.host.session.preview_document());
    check(preview.at(targets[0])==-347&&preview.at(targets[1])==13&&preview.at(targets[2])==733,
        "first shared-delta preview applies from every exact starting source rotation");
    move(dial,12);preview=evaluate(window.host.session.preview_document());
    check(preview.at(targets[0])==-343&&preview.at(targets[1])==17&&preview.at(targets[2])==737,
        "later preview recomputes from the frozen starting snapshot rather than accumulating");
    move(dial,10);preview=evaluate(window.host.session.preview_document());
    check(preview.at(targets[0])==-345&&preview.at(targets[1])==15&&preview.at(targets[2])==735,
        "mixed signed and multi-turn Polygon/Star values preserve their individual differences");
    check(numeric->text().isEmpty()&&numeric->placeholderText()=="Mixed"&&
        dial->accessibleDescription().contains("relative delta +10 degrees")&&window.host.session.revision()==revision,
        "mixed preview reports a signed live delta accessibly without replacing the numeric Mixed state");
    release(dial,10);check(window.host.session.revision()==revision+1,
        "completed multi-object dial drag is exactly one Session revision");
    check(evaluated(window,targets)==std::vector<double>({-345,15,735}),
        "one batch gesture commits all requested source rotations");
    window.host.session.undo(window.host.session.revision());window.host.edited();events();
    check(encode(window.host.session.document())==bytes&&window.host.session.revision()==revision+2,
        "one Undo restores exact source bytes and all original angles");
    window.host.session.redo(window.host.session.revision());window.host.edited();events();
    check(evaluated(window,targets)==std::vector<double>({-345,15,735}),"one Redo restores the whole batch");
    const auto native_path=directory+"/primitive-batch-roundtrip.nect";
    window.host.save(native_path);const auto saved=window.host.session.document();window.host.open(native_path);events();
    check(window.host.session.document()==saved&&window.host.session.document().objects.at("poly").source->id=="source-poly"&&
        window.host.session.document().objects.at("star").source->id=="source-star",
        "native save/reopen preserves authored rotations and both source identities");

    load(window,{{"poly","nect.shape.polygon",2},{"star","nect.shape.star",3}});targets=selected_rotation_refs(window);
    edit(batch_numeric(window,targets),"45");
    check(evaluated(window,targets)==std::vector<double>({45,45}),
        "existing numeric absolute-entry semantics still set one shared value");
    targets=selected_rotation_refs(window);edit(batch_numeric(window,targets),"+=10");
    check(evaluated(window,targets)==std::vector<double>({55,55}),
        "existing numeric += semantics remain a one-time relative edit");
    load(window,{{"poly","nect.shape.polygon",-355},{"star","nect.shape.star",725}});targets=selected_rotation_refs(window);
    edit(batch_numeric(window,targets),"-=10");
    check(evaluated(window,targets)==std::vector<double>({-365,715}),
        "existing numeric -= semantics independently preserve each source's rotation difference");

    load(window,{{"poly","nect.shape.polygon",725},{"star","nect.shape.star",725}});targets=selected_rotation_refs(window);
    auto* common=visible_batch_dial(window,targets);const auto start=window.host.session.revision();
    press(common);move(common,10);
    check(batch_numeric(window,targets)->text()=="735"&&
        common->accessibleDescription().contains("current unwrapped angle 735 degrees")&&
        common->accessibleDescription().contains("relative delta +10 degrees"),
        "common numeric text and unwrapped accessibility track the live shared delta");
    QTest::keyClick(common,Qt::Key_Escape);events();
    check(window.host.session.revision()==start&&evaluated(window,targets)==std::vector<double>({725,725})&&
        batch_numeric(window,targets)->text()=="725"&&
        common->accessibleDescription().contains("current unwrapped angle 725 degrees"),
        "Escape restores the exact common starting value and accessible state without a revision");
}
void no_motion_full_turn_and_atomic_refusal(Window& window) {
    load(window,{{"poly","nect.shape.polygon",10},{"star","nect.shape.star",20}});
    auto targets=selected_rotation_refs(window);auto* dial=visible_batch_dial(window,targets);
    const auto bytes=encode(window.host.session.document());const auto base=window.host.session.revision();
    press(dial);release(dial,0);
    check(window.host.session.revision()==base&&!window.host.session.gesture_active()&&encode(window.host.session.document())==bytes,
        "a no-motion click cancels cleanly without committing an empty EditProperties command");
    dial=visible_batch_dial(window,targets);press(dial);move(dial,12);QTest::keyClick(dial,Qt::Key_Escape);events();
    check(window.host.session.revision()==base&&!window.host.session.gesture_active()&&encode(window.host.session.document())==bytes,
        "Escape cancels the entire two-source preview without a commit");
    dial=visible_batch_dial(window,targets);press(dial);move(dial,8);move(dial,0);release(dial,0);
    check(window.host.session.revision()==base&&!window.host.session.gesture_active()&&encode(window.host.session.document())==bytes,
        "return-to-start motion creates no revision or residue");
    load(window,{{"poly","nect.shape.polygon",725},{"star","nect.shape.star",725}});targets=selected_rotation_refs(window);
    dial=visible_batch_dial(window,targets);const auto turn_revision=window.host.session.revision();press(dial);
    for(int angle=45;angle<=360;angle+=45)move(dial,angle);
    release(dial,360);
    check(window.host.session.revision()==turn_revision+1&&evaluated(window,targets)==std::vector<double>({1085,1085}),
        "a genuine 360-degree common drag remains an authored turn and commits once");

    load(window,{{"poly","nect.shape.polygon",0},{"star","nect.shape.star",999999900}});targets=selected_rotation_refs(window);
    dial=visible_batch_dial(window,targets);const auto range_bytes=encode(window.host.session.document());const auto range_revision=window.host.session.revision();
    press(dial);move(dial,8);
    const auto valid_preview=evaluate(window.host.session.preview_document());
    check(window.host.session.gesture_active()&&valid_preview.at(targets[0])>0&&valid_preview.at(targets[0])<50&&
        std::abs((valid_preview.at(targets[1])-valid_preview.at(targets[0]))-999999900)<1e-6,
        "batch first accepts an in-range shared-delta preview for every target before a later range failure");
    move(dial,180);release(dial,180);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==range_revision&&
        encode(window.host.session.document())==range_bytes&&evaluated(window,targets)==std::vector<double>({0,999999900}),
        "a later target range failure cancels the complete batch without leaking an earlier preview");

    load(window,{{"poly","nect.shape.polygon",0},{"star","nect.shape.star",20},{"driver","nect.shape.polygon",90}});
    targets=selected_rotation_refs(window);const Ref driven{targets[1].object,"","generator.rotation"};
    const Ref source{"driver","","generator.rotation"};
    window.host.session.apply({Link{driven,Binding{source,1,0,"copy_local_value"}}},window.host.session.revision());window.host.edited();events();
    targets=selected_rotation_refs(window);dial=visible_batch_dial(window,targets);
    check(!dial->isEnabled(),"one driven source rotation disables the whole batch dial");
    const auto driven_bytes=encode(window.host.session.document());const auto driven_revision=window.host.session.revision();
    press(dial);release(dial,0);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==driven_revision&&
        encode(window.host.session.document())==driven_bytes,
        "a driven later target is refused atomically and cannot create a gesture");

    load(window,{{"poly","nect.shape.polygon",0},{"star","nect.shape.star",20}});
    targets=selected_rotation_refs(window);
    window.host.session.apply({SetExpression{{targets.back()},Expression{"30",1}}},window.host.session.revision());
    window.host.edited();events();targets=selected_rotation_refs(window);dial=visible_batch_dial(window,targets);
    check(!dial->isEnabled()&&evaluate(window.host.session.document()).at(targets.back())==30,
        "one expression-driven Polygon/Star source rotation disables the complete batch dial");
}
void stale_identity_and_lifecycle(Window& window) {
    load(window,{{"poly","nect.shape.polygon",12},{"star","nect.shape.star",34}});
    auto targets=selected_rotation_refs(window);QPointer<QWidget> dial=visible_batch_dial(window,targets);
    auto replacement=window.host.session.document();replacement.objects.at("star").source->id="replaced-source";
    window.host.session=Session(replacement); // same document and revision; the frozen source identity is the deciding guard
    press(dial);release(dial,0);
    check(!window.host.session.gesture_active()&&window.host.session.document().objects.at("star").source->id=="replaced-source"&&
        window.host.session.revision()==0,
        "same-revision later source identity replacement refuses the entire batch");

    load(window,{{"poly","nect.shape.polygon",12},{"star","nect.shape.star",34}});targets=selected_rotation_refs(window);
    dial=visible_batch_dial(window,targets);replacement=window.host.session.document();
    replacement.objects.at("star").source=default_primitive("source-star","nect.shape.polygon");
    replacement.objects.at("star").source->parameters.at("rotation").literal=34;
    window.host.session=Session(replacement);press(dial);release(dial,0);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==0&&
        window.host.session.document().objects.at("star").source->type=="nect.shape.polygon",
        "valid same-revision replacement from Star to Polygon source type cannot be retargeted by an old dial");

    load(window,{{"poly","nect.shape.polygon",12},{"star","nect.shape.star",34}});targets=selected_rotation_refs(window);
    dial=visible_batch_dial(window,targets);const auto stale=encode(window.host.session.document());
    window.host.session.apply({Set{targets.back(),35}},window.host.session.revision());const auto revision=window.host.session.revision();
    press(dial);release(dial,0);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==revision&&
        evaluate(window.host.session.document()).at(targets.front())==12&&evaluate(window.host.session.document()).at(targets.back())==35&&
        encode(window.host.session.document())!=stale,
        "stale captured revision is refused before changing any selected source");

    load(window,{{"poly","nect.shape.polygon",12},{"star","nect.shape.star",34}});targets=selected_rotation_refs(window);
    dial=visible_batch_dial(window,targets);const auto session_before=encode(window.host.session.document());
    window.host.session_id+="-replacement";press(dial);release(dial,0);
    check(!window.host.session.gesture_active()&&encode(window.host.session.document())==session_before,
        "replacement Host Session identity cannot retarget the frozen batch dial");

    load(window,{{"poly","nect.shape.polygon",12},{"star","nect.shape.star",34}});targets=selected_rotation_refs(window);
    dial=visible_batch_dial(window,targets);auto* dirty=batch_numeric(window,targets);const auto dirty_revision=window.host.session.revision();
    dirty->setText("45");dirty->setModified(true);press(dial);release(dial,0);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==dirty_revision&&dirty->isModified()&&
        dirty->text()=="45"&&window.statusBar()->currentMessage().startsWith("UNCOMMITTED_INPUT"),
        "a dirty numeric batch draft refuses the dial while preserving the visible uncommitted input");

    load(window,{{"poly","nect.shape.polygon",12},{"star","nect.shape.star",34}});targets=selected_rotation_refs(window);
    dial=visible_batch_dial(window,targets);const auto gesture_revision=window.host.session.revision();
    press(dial);move(dial,8);check(window.host.session.gesture_active(),"old dial initially owns gesture A");
    window.host.session.cancel_gesture();window.host.session.begin_gesture(gesture_revision);
    window.host.session.update_gesture({EditProperties{{targets.front()},321,false}});
    const auto replacement_preview=window.host.session.preview_document();
    move(dial,15);release(dial,15);QTest::keyClick(dial,Qt::Key_Escape);events();
    check(window.host.session.gesture_active()&&window.host.session.preview_document()==replacement_preview,
        "stale batch callbacks cannot update, commit, cancel or steal a same-revision replacement gesture B");
    window.host.session.cancel_gesture();

    load(window,{{"poly","nect.shape.polygon",12},{"star","nect.shape.star",34}});targets=selected_rotation_refs(window);
    QPointer<QWidget> old=visible_batch_dial(window,targets);const auto original=encode(window.host.session.document());
    press(old);move(old,10);check(window.host.session.gesture_active(),"lifecycle case begins a real provisional batch gesture");
    window.canvas->set_selections({{"poly",""}});events();
    check(!window.host.session.gesture_active()&&encode(window.host.session.document())==original&&old&&
        !old->isVisible(),"Inspector rebuild cancels and hides the old widget before deferred destruction");
    press(old);move(old,15);release(old,15);
    check(!window.host.session.gesture_active()&&encode(window.host.session.document())==original,
        "a disposed but not yet destroyed batch widget cannot start or mutate another gesture");
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);events();
    check(old.isNull(),"deferred Inspector teardown destroys the old batch dial");
    window.canvas->set_selections({{"poly",""},{"star",""}});events();
    targets=selected_rotation_refs(window);dial=visible_batch_dial(window,targets);press(dial);
    check(window.host.session.gesture_active(),"rebuilt primitive batch control is reusable");QTest::keyClick(dial,Qt::Key_Escape);events();

    load(window,{{"poly","nect.shape.polygon",12},{"star","nect.shape.star",34}});targets=selected_rotation_refs(window);
    dial=visible_batch_dial(window,targets);const auto close_bytes=encode(window.host.session.document());press(dial);move(dial,10);
    window.close();events();
    check(!window.host.session.gesture_active()&&encode(window.host.session.document())==close_bytes,
        "Window close synchronously cancels an active whole-object batch gesture");
    window.show();events();targets=selected_rotation_refs(window);dial=visible_batch_dial(window,targets);press(dial);
    check(window.host.session.gesture_active(),"surviving Inspector remains usable after Window close and reopen");
    QTest::keyClick(dial,Qt::Key_Escape);events();
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"temporary directory is available");
        QSettings settings(directory.filePath("settings.ini"),QSettings::IniFormat);
        Window window(directory.path(),std::make_unique<FolderLibrary>(settings));window.show();events();
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")
            check(window.devicePixelRatioF()>=1.9,"high-DPI run has actual scaled widget DPR");
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="1.25")
            check(window.devicePixelRatioF()>=1.2,"125% run has actual fractional widget DPR");
        if(qEnvironmentVariableIsSet("NECT_GEOMETRY_ONLY")) {
            fixed_global_mixed_geometry(window);
            std::cout<<"PASS fixed-global batch-angle geometry\n";return 0;
        }
        exact_dial_discovery(window);
        fixed_global_mixed_geometry(window);
        shared_delta_history_and_numeric(window,directory.path());
        no_motion_full_turn_and_atomic_refusal(window);
        stale_identity_and_lifecycle(window);
        std::cout<<"PASS "<<checks<<" primitive batch angle checks\n";return 0;
    } catch(const Error& error) {std::cerr<<"FAIL "<<error.code<<": "<<error.what()<<'\n';return 1;}
      catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
