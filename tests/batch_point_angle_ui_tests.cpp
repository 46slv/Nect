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
QWidget* batch_dial(Window& window,const std::string& field,const std::vector<Ref>& targets) {
    const auto name="batch-point-angle-knob-"+field;
    for(auto* item:window.findChildren<QWidget*>())
        if(item->isVisible()&&item->objectName()==QString::fromStdString(name)&&
           item->property("nect-targets").toByteArray()==refs_json(targets))return item;
    throw std::runtime_error("multi-point Inspector has a visible batch angle dial on the exact full target vector");
}
std::vector<Ref> generated_refs(const Document& document,const std::string& object,const std::string& field,std::size_t count);
struct MixedSources {
    Document document;
    std::vector<Ref> targets;
    std::vector<Canvas::Selection> selection;
};
MixedSources mixed_sources(bool enabled_literal,bool flag_enabled) {
    auto document=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");
    Point authored;authored.id="authored-point";authored.x.literal=10;authored.y.literal=20;
    authored.in_angle.literal=-355;authored.out_angle.literal=17;authored.in_length.literal=0;authored.out_length.literal=0;
    Session setup(document);
    setup.apply({CreatePath{"batch-point-composition","","authored","Authored source",{{"authored-contour",false,{authored}}}},
        CreatePrimitive{"batch-point-composition","","circle","Circle",default_primitive("circle-source","nect.shape.circle")},
        CreatePrimitive{"batch-point-composition","","ellipse","Ellipse",default_primitive("ellipse-source","nect.shape.ellipse")},
        CreatePrimitive{"batch-point-composition","","flag","Point Edit flag",default_primitive("flag-source","nect.shape.ellipse")}},setup.revision());
    document=setup.document();const auto circle=generated_refs(document,"circle","in.angle",1).front();
    const auto ellipse=generated_refs(document,"ellipse","in.angle",1).front();
    document.objects.at("flag").point_edit=PointEdit{"flag-source-point-edit",1,flag_enabled,{}};
    PointEdit target_edit{"ellipse-source-point-edit",1,enabled_literal,{}};
    target_edit.enabled_driver=point_edit_enabled_ref("flag","flag-source-point-edit");
    document.objects.at("ellipse").point_edit=target_edit;
    return {std::move(document),{{"authored","authored-point","in.angle"},circle,ellipse},
        {{"authored","authored-point"},{"circle",circle.point},{"ellipse",ellipse.point}}};
}
QLineEdit* batch_numeric(Window& window,const std::vector<Ref>& refs) {
    const auto encoded=refs_json(refs);
    for(auto* input:window.findChildren<QLineEdit*>())
        if(input->isVisible()&&input->property("nect-targets").toByteArray()==encoded)return input;
    throw std::runtime_error("multi-point batch retains its exact numeric target vector");
}
std::vector<Ref> refs_for(const std::string& field) {
    return {{"authored","a",field},{"authored","b",field},{"authored","c",field}};
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
void edit_batch(QLineEdit* input,const QString& text) {
    input->setText(text);input->setModified(true);input->editingFinished();events();
}
void authored_fixture(Window& window,const std::vector<double>& incoming={-355,5,725}) {
    auto document=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");
    Point a;a.id="a";a.x.literal=10;a.y.literal=20;a.in_angle.literal=incoming.at(0);a.out_angle.literal=11;a.in_length.literal=0;a.out_length.literal=0;
    Point b;b.id="b";b.x.literal=30;b.y.literal=20;b.in_angle.literal=incoming.at(1);b.out_angle.literal=22;b.in_length.literal=0;b.out_length.literal=0;
    Point c;c.id="c";c.x.literal=50;c.y.literal=20;c.in_angle.literal=incoming.at(2);c.out_angle.literal=33;c.in_length.literal=0;c.out_length.literal=0;
    Point d;d.id="d";d.x.literal=70;d.y.literal=20;d.in_angle.literal=40;d.out_angle.literal=44;d.in_length.literal=0;d.out_length.literal=0;
    Session setup(document);
    setup.apply({CreatePath{"batch-point-composition","","authored","Batch authored path",
        {{"contour-a",false,{a,b}},{"contour-b",false,{c,d}}}}},setup.revision());
    window.host.session=Session(setup.document());window.host.session_id+="-batch-authored";window.host.edited();
    window.canvas->set_selections({{"authored","a"},{"authored","b"},{"authored","c"}});events();
}
void shared_caption_geometry(Window& window) {
    window.resize(1000,650);
    if(auto* scroll=window.findChild<QWidget*>("inspector-scroll"))scroll->setFixedWidth(300);
    if(auto* dock=window.findChild<QWidget*>("properties"))dock->setFixedWidth(320);
    events();
    authored_fixture(window,{45,45,45});QTest::qWait(80);window.grab();events();
    const auto targets=refs_for("in.angle");auto* common=batch_dial(window,"in.angle",targets);
    const QPoint center=common->mapToGlobal(QPoint(common->width()/2,common->height()/2));
    const auto common_geometry=common->geometry(),common_row=common->parentWidget()->geometry();
    const auto* common_caption=common->parentWidget()->findChild<QLabel*>();
    check(batch_angle_test::caption_has_reserved_width(common_caption),
        "point-batch caption reserves a content-independent minimum before interaction");
    const batch_angle_test::Geometry common_stable(common);
    mouse_global(common,QEvent::MouseButtonPress,QPointF(center)+QPointF(16,0),Qt::LeftButton,Qt::LeftButton);
    mouse_global(common,QEvent::MouseMove,QPointF(center)+QPointF(14,8),Qt::NoButton,Qt::LeftButton);
    check(common_stable.stable(common)&&common->geometry()==common_geometry&&common->parentWidget()->geometry()==common_row&&
        common->mapToGlobal(QPoint(common->width()/2,common->height()/2))==center&&common_caption&&
        common_caption->text().contains("+X zero")&&common_caption->text().contains("Δ +")&&
        batch_angle_test::caption_fits(common_caption)&&
        common_caption->heightForWidth(common_caption->width())<=common_caption->height(),
        "shared point-batch Common caption keeps fixed geometry and fully shows its fractional delta");
    check(batch_angle_test::narrow_caption_samples_fit(common_caption),
        "shared point-batch caption samples fit 98×80px with the active UI font metrics");
    QTest::keyClick(common,Qt::Key_Escape);events();
    check(!window.host.session.gesture_active()&&batch_numeric(window,targets)->text()=="45",
        "shared point-batch Common value restores after Escape");
    const int common_height=common->parentWidget()->height();
    const int common_dial_height=common->height();

    authored_fixture(window,{-355,5,725});QTest::qWait(80);window.grab();events();
    auto* mixed=batch_dial(window,"in.angle",targets);
    const auto mixed_geometry=mixed->geometry();const auto mixed_row=mixed->parentWidget()->geometry();
    const QPoint mixed_center=mixed->mapToGlobal(QPoint(mixed->width()/2,mixed->height()/2));
    check(common_stable.stable(mixed)&&common_height==mixed->parentWidget()->height()&&common_dial_height==mixed->height(),
        "shared point-batch Common and Mixed captions retain identical fixed row and dial sizes");
    const batch_angle_test::Geometry mixed_stable(mixed);
    mouse_global(mixed,QEvent::MouseButtonPress,QPointF(mixed_center)+QPointF(16,0),Qt::LeftButton,Qt::LeftButton);
    for(const auto& offset:{QPointF(14,8),QPointF(8,14),QPointF(0,16)}) {
        mouse_global(mixed,QEvent::MouseMove,QPointF(mixed_center)+offset,Qt::NoButton,Qt::LeftButton);
        check(mixed_stable.stable(mixed)&&mixed->geometry()==mixed_geometry&&mixed->parentWidget()->geometry()==mixed_row&&
            mixed->mapToGlobal(QPoint(mixed->width()/2,mixed->height()/2))==mixed_center,
            "shared point-batch fixed-global arc does not move its dial while caption text wraps");
        const auto* caption=mixed->parentWidget()->findChild<QLabel*>();
        check(caption&&caption->text().contains("+X zero")&&caption->text().contains("Δ +")&&
            batch_angle_test::caption_fits(caption)&&caption->heightForWidth(caption->width())<=caption->height(),
            "shared point-batch live signed delta remains fully visible in the fixed row");
    }
    const auto values=evaluate(window.host.session.preview_document());
    check(std::abs(values.at(targets[0])-(-265))<1e-8&&std::abs(values.at(targets[1])-95)<1e-8&&
        std::abs(values.at(targets[2])-815)<1e-8&&mixed->accessibleDescription().contains("relative delta +90 degrees"),
        "shared point-batch non-cardinal fixed-global quarter turn remains exactly +90 degrees");
    QTest::keyClick(mixed,Qt::Key_Escape);events();
    check(batch_angle_test::wider_inspector_caption_expands(batch_dial(window,"in.angle",targets)),
        "caption expands in a wider Inspector without changing its reserved minimum or compact height");
    if(auto* scroll=window.findChild<QWidget*>("inspector-scroll")) {
        scroll->setMinimumWidth(300);scroll->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    if(auto* dock=window.findChild<QWidget*>("properties")) {
        dock->setMinimumWidth(0);dock->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    window.resize(1400,900);events();
}
std::vector<Ref> generated_refs(const Document& document,const std::string& object,const std::string& field,std::size_t count=2) {
    const auto values=evaluate(document);const auto contours=path_contours(document.objects.at(object),&values);
    std::vector<Ref> refs;
    for(const auto& point:contours.front().points) {refs.push_back({object,point.id,field});if(refs.size()==count)break;}
    return refs;
}
void batch_vector_and_history(Window& window,const QString& directory) {
    authored_fixture(window);const auto targets=refs_for("in.angle");
    auto* dial=batch_dial(window,"in.angle",targets);auto* numeric=batch_numeric(window,targets);
    if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")
        check(dial->devicePixelRatioF()>=1.9,"actual batch-angle widget uses the high-DPI device scale");
    check(dial->property("nect-targets").toByteArray()==refs_json(targets)&&
        dial->property("nect-reference").toByteArray()==ref_json(targets.front())&&
        numeric->property("nect-targets").toByteArray()==refs_json(targets),
        "batch dial and numeric row freeze the same exact ordered target Refs, not only the first point");
    check(numeric->text().isEmpty()&&numeric->placeholderText()=="Mixed"&&numeric->property("nect-mixed").toBool(),
        "different exact source angles remain visibly Mixed in the numeric editor");
    check(dial->accessibleName().contains("mixed",Qt::CaseInsensitive)&&
        dial->accessibleName().contains("3")&&dial->accessibleName().contains("relative",Qt::CaseInsensitive),
        "mixed angle dial accessibility names Mixed, target count, and relative behavior");
    const auto before=window.host.session.document();const auto bytes=encode(before);const auto revision=window.host.session.revision();
    press(dial);move(dial,8);
    auto preview=evaluate(window.host.session.preview_document());
    check(preview.at(targets[0])==-347&&preview.at(targets[1])==13&&preview.at(targets[2])==733,
        "first preview is recomputed from the exact committed mixed starting snapshot");
    move(dial,12);preview=evaluate(window.host.session.preview_document());
    check(preview.at(targets[0])==-343&&preview.at(targets[1])==17&&preview.at(targets[2])==737,
        "second preview applies the absolute gesture delta, not a cumulative delta from preview one");
    move(dial,10);preview=evaluate(window.host.session.preview_document());
    check(preview.at(targets[0])==-345&&preview.at(targets[1])==15&&preview.at(targets[2])==735,
        "one shared relative delta preserves exact mixed angles including negative and multi-turn values");
    check(numeric->text().isEmpty()&&numeric->placeholderText()=="Mixed"&&window.host.session.revision()==revision,
        "mixed live preview never presents a first or averaged value and remains uncommitted");
    release(dial,10);check(window.host.session.revision()==revision+1,"batch radial gesture commits exactly one Session revision");
    check(evaluate(window.host.session.document()).at(targets[0])==-345&&
        evaluate(window.host.session.document()).at(targets[1])==15&&evaluate(window.host.session.document()).at(targets[2])==735,
        "batch angle gesture commits all values from the frozen starting snapshot");
    window.host.session.undo(window.host.session.revision());window.host.edited();events();
    check(encode(window.host.session.document())==bytes&&window.host.session.revision()==revision+2,
        "one Undo restores all exact starting angle bytes");
    window.host.session.redo(window.host.session.revision());window.host.edited();events();
    check(evaluate(window.host.session.document()).at(targets[0])==-345&&
        evaluate(window.host.session.document()).at(targets[1])==15&&evaluate(window.host.session.document()).at(targets[2])==735,
        "one Redo restores the complete relative batch");
    const auto native_path=directory+"/batch-point-roundtrip.nect";
    window.host.save(native_path);const auto expected=window.host.session.document();window.host.open(native_path);events();
    check(window.host.session.document()==expected&&window.host.session.document().objects.at("authored").contours.size()==2,
        "native save and reopen preserve batch values and authored contour identities");
    authored_fixture(window);auto* fresh=batch_numeric(window,targets);
    const auto fresh_revision=window.host.session.revision();edit_batch(fresh,"45");
    auto values=evaluate(window.host.session.document());
    check(values.at(targets[0])==45&&values.at(targets[1])==45&&values.at(targets[2])==45&&
        window.host.session.revision()==fresh_revision+1,
        "absolute mixed numeric entry assigns the exact value to every target in one revision");
    fresh=batch_numeric(window,targets);const auto relative_revision=window.host.session.revision();edit_batch(fresh,"+=10");
    values=evaluate(window.host.session.document());
    check(values.at(targets[0])==55&&values.at(targets[1])==55&&values.at(targets[2])==55&&
        window.host.session.revision()==relative_revision+1,
        "numeric += remains a one-time relative batch edit");
    authored_fixture(window);fresh=batch_numeric(window,targets);const auto mixed_relative_revision=window.host.session.revision();
    edit_batch(fresh,"+=10");values=evaluate(window.host.session.document());
    check(values.at(targets[0])==-345&&values.at(targets[1])==15&&values.at(targets[2])==735&&
        window.host.session.revision()==mixed_relative_revision+1,
        "numeric += from a mixed draft also preserves each exact starting difference");
    fresh=batch_numeric(window,targets);edit_batch(fresh,"45");dial=batch_dial(window,"in.angle",targets);
    check(dial->accessibleDescription().contains("current unwrapped angle 45 degrees")&&
        dial->accessibleDescription().contains("indicator 45 degrees")&&
        dial->accessibleName().contains("common",Qt::CaseInsensitive),
        "uniform exact values show their common modulo direction rather than a mixed marker");
    press(dial);move(dial,10);
    check(dial->accessibleDescription().contains("current unwrapped angle 55 degrees")&&
        dial->accessibleDescription().contains("relative delta +10 degrees")&&
        batch_numeric(window,targets)->text()=="55",
        "common batch accessibility and numeric value follow the live unwrapped angle preview");
    QTest::keyClick(dial,Qt::Key_Escape);events();
    check(dial->accessibleDescription().contains("current unwrapped angle 45 degrees")&&
        batch_numeric(window,targets)->text()=="45",
        "Escape restores the common exact value and its accessible preview description");
    constexpr double precise=725.1234567890123;authored_fixture(window,{precise,precise,precise});
    fresh=batch_numeric(window,targets);edit_batch(fresh,QString::number(precise,'g',17));
    dial=batch_dial(window,"in.angle",targets);values=evaluate(window.host.session.document());
    check(values.at(targets[0])==precise&&values.at(targets[1])==precise&&values.at(targets[2])==precise&&
        batch_numeric(window,targets)->text()==QString::number(precise,'g',17)&&
        dial->accessibleDescription().contains(QString("current unwrapped angle %1 degrees").arg(QString::number(precise,'g',17))),
        "common batch numeric value retains a nontrivial 17-digit signed angle exactly");
    authored_fixture(window,{5,365,725});dial=batch_dial(window,"in.angle",targets);
    const auto labels=dial->parentWidget()->findChildren<QLabel*>();
    const bool mixed_caption=std::any_of(labels.begin(),labels.end(),[](const QLabel* label) {
        return label->isVisible()&&label->text().contains("Mixed · 3")&&label->text().contains("+X zero")&&
            label->text().contains("Δ ");
    });
    const auto* modulo_mixed_numeric=batch_numeric(window,targets);
    check(modulo_mixed_numeric->text().isEmpty()&&modulo_mixed_numeric->placeholderText()=="Mixed"&&
        dial->accessibleName().contains("mixed",Qt::CaseInsensitive)&&mixed_caption,
        "5° and 365° remain Mixed even though their modulo directions match");
}
void field_isolation_and_generated_sources(Window& window) {
    authored_fixture(window);const auto in=refs_for("in.angle"),out=refs_for("out.angle");
    auto* out_dial=batch_dial(window,"out.angle",out);const auto start=evaluate(window.host.session.document());const auto revision=window.host.session.revision();
    press(out_dial);move(out_dial,12);release(out_dial,12);
    const auto result=evaluate(window.host.session.document());
    check(result.at(out[0])==23&&result.at(out[1])==34&&result.at(out[2])==45&&
        result.at(in[0])==-355&&result.at(in[1])==5&&result.at(in[2])==725&&window.host.session.revision()==revision+1,
        "outgoing batch angle edits only each requested angle and preserves incoming values across contours");
    check(start.at({"authored","d","out.angle"})==44&&result.at({"authored","d","out.angle"})==44&&
        result.at({"authored","d","in.angle"})==40,
        "unselected points and their independent handle angles remain unchanged");
    for(const auto* type:{"nect.shape.circle","nect.shape.ellipse","nect.shape.polygon","nect.shape.star"}) {
        auto document=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");Session setup(document);
        setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Generated",
            default_primitive("source-id",type)}},setup.revision());
        document=setup.document();const auto targets=generated_refs(document,"primitive","in.angle");
        check(targets.size()==2,"generated source exposes stable points for a multi-selection");
        const auto originals=evaluate(document);window.host.session=Session(document);window.host.session_id+="-generated";window.host.edited();
        window.canvas->set_selections({{"primitive",targets[0].point},{"primitive",targets[1].point}});events();
        auto* dial=batch_dial(window,"in.angle",targets);const auto base=window.host.session.revision();
        const auto untouched=encode(document);
        press(dial);release(dial,0);
        check(!window.host.session.document().objects.at("primitive").point_edit&&
            window.host.session.revision()==base&&encode(window.host.session.document())==untouched,
            "no-motion generated batch click creates no canonical Point Edit or history");
        dial=batch_dial(window,"in.angle",targets);press(dial);move(dial,12);
        const auto escape_preview=window.host.session.preview_document();
        check(escape_preview.objects.at("primitive").point_edit&&
            escape_preview.objects.at("primitive").point_edit->overrides.size()==2&&
            !window.host.session.document().objects.at("primitive").point_edit,
            "Escape case has a real provisional two-point Point Edit preview before cancellation");
        QTest::keyClick(dial,Qt::Key_Escape);events();
        check(!window.host.session.document().objects.at("primitive").point_edit&&
            window.host.session.revision()==base&&encode(window.host.session.document())==untouched,
            "Escape removes a provisional generated batch preview completely");
        dial=batch_dial(window,"in.angle",targets);press(dial);move(dial,12);
        check(window.host.session.preview_document().objects.at("primitive").point_edit&&
            !window.host.session.document().objects.at("primitive").point_edit,
            "return-to-start case begins with a real provisional Point Edit preview");
        move(dial,0);release(dial,0);
        check(!window.host.session.document().objects.at("primitive").point_edit&&
            window.host.session.revision()==base&&encode(window.host.session.document())==untouched,
            "no-net generated batch motion leaves no ghost Point Edit override");
        dial=batch_dial(window,"in.angle",targets);
        press(dial);move(dial,8);release(dial,8);
        const auto& edited=window.host.session.document();const auto values=evaluate(edited);
        check(edited.objects.at("primitive").source&&edited.objects.at("primitive").source->id=="source-id"&&
            edited.objects.at("primitive").point_edit&&edited.objects.at("primitive").point_edit->id=="source-id-point-edit"&&
            edited.objects.at("primitive").point_edit->version==1&&window.host.session.revision()==base+1,
            "generated point-angle batch preserves Circle/Ellipse/Polygon/Star source identity and creates canonical Point Edit");
        const auto& overrides=edited.objects.at("primitive").point_edit->overrides;
        check(overrides.size()==2&&overrides.at(targets[0].point).size()==1&&overrides.at(targets[1].point).size()==1&&
            overrides.at(targets[0].point).contains("in.angle")&&overrides.at(targets[1].point).contains("in.angle")&&
            std::abs(values.at(targets[0])-originals.at(targets[0])-8)<1e-9&&
            std::abs(values.at(targets[1])-originals.at(targets[1])-8)<1e-9,
            "generated batch changes only the requested incoming angles for selected stable point IDs");
    }
}
void cross_object_sources_and_atomic_rejection(Window& window) {
    auto mixed=mixed_sources(true,true);const auto initial=evaluate(mixed.document);
    window.host.session=Session(mixed.document);window.host.session_id+="-cross-object-batch";window.host.edited();
    window.canvas->set_selections(mixed.selection);events();auto* dial=batch_dial(window,"in.angle",mixed.targets);
    check(dial->isEnabled()&&batch_numeric(window,mixed.targets)->placeholderText()=="Mixed",
        "full batch control accepts an ordered authored+Circle+Ellipse target vector and marks distinct starts Mixed");
    const auto revision=window.host.session.revision();press(dial);move(dial,10);release(dial,10);
    const auto values=evaluate(window.host.session.document());const auto& objects=window.host.session.document().objects;
    check(window.host.session.revision()==revision+1&&
        std::abs(values.at(mixed.targets[0])-initial.at(mixed.targets[0])-10)<1e-9&&
        std::abs(values.at(mixed.targets[1])-initial.at(mixed.targets[1])-10)<1e-9&&
        std::abs(values.at(mixed.targets[2])-initial.at(mixed.targets[2])-10)<1e-9,
        "one exact mixed-source command applies the same relative delta across authored and two independent generators");
    check(objects.at("circle").source->id=="circle-source"&&objects.at("circle").point_edit&&
        objects.at("circle").point_edit->id=="circle-source-point-edit"&&
        objects.at("ellipse").source->id=="ellipse-source"&&objects.at("ellipse").point_edit&&
        objects.at("ellipse").point_edit->id=="ellipse-source-point-edit"&&objects.at("ellipse").point_edit->enabled_driver&&
        objects.at("ellipse").point_edit->enabled,
        "each generated object retains its own canonical Point Edit identity and typed enabled-driver state");
    check(objects.at("circle").point_edit->overrides.size()==1&&
        objects.at("circle").point_edit->overrides.at(mixed.targets[1].point).size()==1&&
        objects.at("circle").point_edit->overrides.at(mixed.targets[1].point).contains("in.angle")&&
        objects.at("ellipse").point_edit->overrides.size()==1&&
        objects.at("ellipse").point_edit->overrides.at(mixed.targets[2].point).size()==1&&
        objects.at("ellipse").point_edit->overrides.at(mixed.targets[2].point).contains("in.angle"),
        "each multi-source generated target receives only its requested in.angle override");

    auto rejected=mixed_sources(false,false);const auto before=encode(rejected.document);
    window.host.session=Session(rejected.document);window.host.session_id+="-late-target-rejection";window.host.edited();
    window.canvas->set_selections(rejected.selection);events();dial=batch_dial(window,"in.angle",rejected.targets);
    check(dial->isEnabled(),"typed enabled-driver target is left to the core authored-literal guard");
    const auto before_revision=window.host.session.revision();press(dial);move(dial,10);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==before_revision&&
        encode(window.host.session.document())==before&&window.statusBar()->currentMessage().contains("DRIVEN_PROPERTY")&&
        !window.host.session.document().objects.at("circle").point_edit&&
        window.host.session.document().objects.at("authored").contours.front().points.front().in_angle.literal==-355,
        "a later typed-disabled target atomically rejects earlier authored and generated edits with no partial Point Edit");
}
void no_net_dirty_and_range(Window& window) {
    authored_fixture(window);const auto targets=refs_for("in.angle");auto* dial=batch_dial(window,"in.angle",targets);
    const auto bytes=encode(window.host.session.document());const auto revision=window.host.session.revision();
    press(dial);release(dial,0);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==revision&&
        encode(window.host.session.document())==bytes,"click-only batch gesture creates no authored bytes or history");
    dial=batch_dial(window,"in.angle",targets);press(dial);move(dial,15);QTest::keyClick(dial,Qt::Key_Escape);events();
    check(!window.host.session.gesture_active()&&window.host.session.revision()==revision&&encode(window.host.session.document())==bytes,
        "Escape cancels all previews without Point Edit or authored residue");
    dial=batch_dial(window,"in.angle",targets);press(dial);move(dial,12);move(dial,0);release(dial,0);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==revision&&encode(window.host.session.document())==bytes,
        "return-to-start clears the entire preview instead of issuing zero relative edits");
    dial=batch_dial(window,"in.angle",targets);press(dial);move(dial,90);move(dial,180);move(dial,270);move(dial,0);release(dial,0);
    const auto full_turn=evaluate(window.host.session.document());
    check(window.host.session.revision()==revision+1&&std::abs(full_turn.at(targets[0])-5)<1e-8&&
        std::abs(full_turn.at(targets[1])-365)<1e-8&&std::abs(full_turn.at(targets[2])-1085)<1e-8,
        "a genuine complete 360-degree batch rotation commits even though the indicator returns to its start");
    authored_fixture(window,{-355,5,999999995});dial=batch_dial(window,"in.angle",targets);
    const auto range_bytes=encode(window.host.session.document());const auto range_revision=window.host.session.revision();
    press(dial);move(dial,10);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==range_revision&&
        encode(window.host.session.document())==range_bytes&&window.statusBar()->currentMessage().contains("OUT_OF_RANGE"),
        "a later authored target just beyond the legal range rejects the entire relative preview");
    auto generated=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");
    Session setup(generated);setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Generated range",
        default_primitive("range-source","nect.shape.ellipse")}},setup.revision());
    generated=setup.document();const auto generated_targets=generated_refs(generated,"primitive","in.angle",2);
    setup.apply({EditProperties{{generated_targets[1]},999999995,false}},setup.revision());
    window.host.session=Session(setup.document());window.host.session_id+="-generated-range";window.host.edited();
    window.canvas->set_selections({{"primitive",generated_targets[0].point},{"primitive",generated_targets[1].point}});events();
    const auto generated_bytes=encode(window.host.session.document());const auto generated_revision=window.host.session.revision();
    auto* generated_dial=batch_dial(window,"in.angle",generated_targets);
    check(window.host.session.document().objects.at("primitive").point_edit->overrides.size()==1&&
        window.host.session.document().objects.at("primitive").point_edit->overrides.contains(generated_targets[1].point),
        "generated range fixture has an earlier untouched target and a later existing correction near the limit");
    press(generated_dial);move(generated_dial,10);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==generated_revision&&
        encode(window.host.session.document())==generated_bytes&&
        window.statusBar()->currentMessage().contains("OUT_OF_RANGE")&&
        !window.host.session.document().objects.at("primitive").point_edit->overrides.contains(generated_targets[0].point),
        "later generated out-of-range target cancels without leaking an earlier provisional Point Edit");
    (void)targets;
}
void driven_batch_rejection(Window& window) {
    authored_fixture(window);const auto targets=refs_for("in.angle");
    window.host.session.apply({SetExpression{{targets[1]},Expression{"5",1}}},window.host.session.revision());
    window.host.edited();window.canvas->set_selections({{"authored","a"},{"authored","b"},{"authored","c"}});events();
    auto* dial=batch_dial(window,"in.angle",targets);check(!dial->isEnabled(),"any driven authored point-angle target disables the complete batch dial");
    const auto document=window.host.session.document();const auto revision=window.host.session.revision();press(dial);release(dial,0);
    check(window.host.session.revision()==revision&&window.host.session.document()==document,
        "disabled driven-target batch cannot partially author the other selected points");
    auto generated=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");Session setup(generated);
    setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Driven generated",
        default_primitive("source-id","nect.shape.ellipse")}},setup.revision());
    generated=setup.document();const auto gen=generated_refs(generated,"primitive","in.angle");
    setup=Session(generated);setup.apply({SetExpression{{gen[1]},Expression{"45",1}}},setup.revision());
    window.host.session=Session(setup.document());window.host.session_id+="-generated-driven";window.host.edited();
    window.canvas->set_selections({{"primitive",gen[0].point},{"primitive",gen[1].point}});events();
    dial=batch_dial(window,"in.angle",gen);check(!dial->isEnabled(),"a generated Point Edit expression on one target disables the whole batch dial");
    window.host.session.apply({EnablePointEdit{"primitive",false}},window.host.session.revision());window.host.edited();
    window.canvas->set_selections({{"primitive",gen[0].point},{"primitive",gen[1].point}});events();
    check(!batch_dial(window,"in.angle",gen)->isEnabled(),"bypassed stored Point Edit expression still blocks all batch targets");

    auto typed=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");Session typed_setup(typed);
    typed_setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Typed enabled source",
            default_primitive("typed-target-source","nect.shape.ellipse")},
        CreatePrimitive{"batch-point-composition","","flag","Enabled flag",
            default_primitive("typed-flag-source","nect.shape.ellipse")}},typed_setup.revision());
    typed=typed_setup.document();typed.objects.at("flag").point_edit=PointEdit{"typed-flag-source-point-edit",1,false,{}};
    PointEdit disabled{"typed-target-source-point-edit",1,false,{}};
    disabled.enabled_driver=point_edit_enabled_ref("flag","typed-flag-source-point-edit");
    typed.objects.at("primitive").point_edit=disabled;
    window.host.session=Session(typed);window.host.session_id+="-typed-batch";window.host.edited();
    const auto typed_targets=generated_refs(typed,"primitive","in.angle");
    window.canvas->set_selections({{"primitive",typed_targets[0].point},{"primitive",typed_targets[1].point}});events();
    dial=batch_dial(window,"in.angle",typed_targets);check(dial->isEnabled(),
        "typed Point Edit enabled driver continues to follow the core authored-literal policy");
    const auto typed_bytes=encode(window.host.session.document());const auto typed_revision=window.host.session.revision();
    press(dial);move(dial,10);
    check(!window.host.session.gesture_active()&&window.host.session.revision()==typed_revision&&
        encode(window.host.session.document())==typed_bytes&&window.statusBar()->currentMessage().contains("DRIVEN_PROPERTY"),
        "core typed enabled-driver rejection cancels the complete batch without partial overrides");

    auto bypass=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");Session bypass_setup(bypass);
    bypass_setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Existing correction",
        default_primitive("bypass-source","nect.shape.ellipse")}},bypass_setup.revision());
    bypass=bypass_setup.document();const auto bypass_targets=generated_refs(bypass,"primitive","in.angle");
    bypass.objects.at("primitive").point_edit=PointEdit{"bypass-source-point-edit",1,true,
        {{bypass_targets[0].point,{{"in.angle",Scalar{123,{}}}}}}};
    window.host.session=Session(bypass);window.host.session.apply({EnablePointEdit{"primitive",false}},window.host.session.revision());
    window.host.session_id+="-bypassed-literal";window.host.edited();
    window.canvas->set_selections({{"primitive",bypass_targets[0].point},{"primitive",bypass_targets[1].point}});events();
    const auto bypass_bytes=encode(window.host.session.document());const auto bypass_revision=window.host.session.revision();
    dial=batch_dial(window,"in.angle",bypass_targets);check(dial->isEnabled(),"stored bypassed literal corrections remain editable when no angle source is driven");
    press(dial);release(dial,0);
    dial=batch_dial(window,"in.angle",bypass_targets);press(dial);move(dial,8);QTest::keyClick(dial,Qt::Key_Escape);events();
    dial=batch_dial(window,"in.angle",bypass_targets);press(dial);move(dial,8);move(dial,0);release(dial,0);
    check(window.host.session.revision()==bypass_revision&&encode(window.host.session.document())==bypass_bytes&&
        !window.host.session.document().objects.at("primitive").point_edit->enabled&&
        window.host.session.document().objects.at("primitive").point_edit->overrides.at(bypass_targets[0].point).at("in.angle").literal==123,
        "click, Escape, and no-net batch gestures leave a bypassed existing correction and its bytes untouched");
}
void stale_and_arbitration(Window& window) {
    authored_fixture(window);const auto targets=refs_for("in.angle"),out_targets=refs_for("out.angle");
    auto* in=batch_dial(window,"in.angle",targets);auto* out=batch_dial(window,"out.angle",out_targets);
    press(in);move(in,8);const auto preview=window.host.session.preview_document();press(out);release(out,0);
    check(window.host.session.gesture_active()&&window.host.session.preview_document()==preview,
        "second batch angle dial cannot steal the active batch Session gesture");
    release(in,8);events();
    authored_fixture(window);in=batch_dial(window,"in.angle",targets);auto* numeric=batch_numeric(window,targets);
    numeric->setText("123");numeric->setModified(true);press(in);release(in,0);
    check(!window.host.session.gesture_active()&&numeric->isModified()&&numeric->text()=="123"&&
        window.statusBar()->currentMessage().startsWith("UNCOMMITTED_INPUT"),
        "batch dial refuses a dirty numeric draft without discarding it");
    authored_fixture(window);in=batch_dial(window,"in.angle",targets);
    window.host.session.apply({Set{targets[0],-350}},window.host.session.revision());const auto revision=window.host.session.revision();
    press(in);release(in,0);
    check(window.host.session.revision()==revision&&window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT"),
        "stale source revision refuses a batch angle dial before any target changes");
    authored_fixture(window);in=batch_dial(window,"in.angle",targets);auto replaced=window.host.session.document();replaced.id="replacement-document";
    window.host.session=Session(replaced);const auto replacement=encode(replaced);press(in);release(in,0);
    check(encode(window.host.session.document())==replacement&&window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT"),
        "stale document identity rejects batch angle control without partial state");
    authored_fixture(window);in=batch_dial(window,"in.angle",targets);replaced=window.host.session.document();
    replaced.objects.at("authored").contours.front().id="replacement-contour";window.host.session=Session(replaced);
    const auto changed_contour=encode(replaced);press(in);release(in,0);
    check(encode(window.host.session.document())==changed_contour&&window.statusBar()->currentMessage().startsWith("MISSING_PROPERTY"),
        "same-revision replacement contour identity is rejected atomically");
    authored_fixture(window);in=batch_dial(window,"in.angle",targets);const auto old_session_id=window.host.session_id;
    const auto host_identity_bytes=encode(window.host.session.document());window.host.session_id+="-replacement";
    press(in);release(in,0);
    check(window.host.session_id!=old_session_id&&encode(window.host.session.document())==host_identity_bytes&&
        window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT"),
        "batch angle dial rejects a same-revision replacement Host Session identity");
    authored_fixture(window);in=batch_dial(window,"in.angle",targets);press(in);move(in,8);
    auto& session=window.host.session;const auto old_preview=session.preview_document();session.cancel_gesture();
    session.begin_gesture(session.revision());session.update_gesture({Set{targets[0],321}});const auto replacement_preview=session.preview_document();
    move(in,15);release(in,15);
    check(session.gesture_active()&&session.preview_document()==replacement_preview&&session.preview_document()!=old_preview,
        "stale batch callback cannot update, commit, or cancel a same-revision replacement gesture");
    session.cancel_gesture();
}
void teardown_and_generated_staleness(Window& window,QTemporaryDir& directory) {
    const auto targets=refs_for("in.angle");authored_fixture(window);auto* dial=batch_dial(window,"in.angle",targets);const auto revision=window.host.session.revision();
    press(dial);move(dial,8);window.refresh(false);
    check(!window.host.session.gesture_active(),"Inspector rebuild cancels the batch adapter's own preview");
    QPointer<QWidget> retired(dial);
    check(!retired.isNull(),"rebuild teardown fixture retains the hidden widget until deferred deletion");
    if(retired) {
        const auto bytes=encode(window.host.session.document());const auto rev=window.host.session.revision();
        const auto local=knob_point(0);QMouseEvent press_event(QEvent::MouseButtonPress,local,
            QPointF(retired->mapToGlobal(QPoint(0,0)))+local,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(retired,&press_event);
        check(!window.host.session.gesture_active()&&window.host.session.revision()==rev&&
            encode(window.host.session.document())==bytes&&window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT"),
            "retired batch widget cannot restart a gesture during deferred Inspector deletion");
    }
    events();
    authored_fixture(window);dial=batch_dial(window,"in.angle",targets);press(dial);move(dial,8);window.close();events();
    check(!window.host.session.gesture_active(),"Window close cancels batch preview before Host teardown");
    window.show();events();authored_fixture(window);dial=batch_dial(window,"in.angle",targets);
    press(dial);move(dial,6);release(dial,6);check(window.host.session.revision()>revision,"batch angle dial remains reusable after close");
    for(const auto* type:{"nect.shape.circle","nect.shape.ellipse","nect.shape.polygon","nect.shape.star"}) {
        auto document=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");Session setup(document);
        setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Generated",default_primitive("source-id",type)}},setup.revision());
        document=setup.document();const auto selected=generated_refs(document,"primitive","in.angle");
        window.host.session=Session(document);window.host.session_id+="-stale-generated";window.host.edited();
        window.canvas->set_selections({{"primitive",selected[0].point},{"primitive",selected[1].point}});events();dial=batch_dial(window,"in.angle",selected);
        const auto before=encode(window.host.session.document());const auto rev=window.host.session.revision();
        auto changed=window.host.session.document();changed.objects.at("primitive").source->id="different-source";
        window.host.session=Session(changed);const auto changed_bytes=encode(changed);press(dial);release(dial,0);
        check(window.host.session.revision()==0&&encode(window.host.session.document())==changed_bytes&&before!=changed_bytes&&
            window.statusBar()->currentMessage().startsWith("MISSING_PROPERTY"),
            "replacement generated source ID cannot retarget a captured multi-point batch");
        (void)rev;
    }
    auto ellipse=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");Session setup(ellipse);
    setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Generated",
        default_primitive("source-id","nect.shape.circle")}},setup.revision());
    ellipse=setup.document();const auto circle_targets=generated_refs(ellipse,"primitive","in.angle");
    window.host.session=Session(ellipse);window.host.session_id+="-stale-source-type";window.host.edited();
    window.canvas->set_selections({{"primitive",circle_targets[0].point},{"primitive",circle_targets[1].point}});events();
    dial=batch_dial(window,"in.angle",circle_targets);auto changed=window.host.session.document();auto& source=*changed.objects.at("primitive").source;
    source.type="nect.shape.ellipse";source.parameters.erase("radius");source.parameters.emplace("width",Scalar{220,{}});
    source.parameters.emplace("height",Scalar{140,{}});window.host.session=Session(changed);const auto type_bytes=encode(changed);
    press(dial);release(dial,0);
    check(encode(window.host.session.document())==type_bytes&&window.statusBar()->currentMessage().startsWith("MISSING_PROPERTY"),
        "valid replacement primitive source type is rejected without retargeting the stale batch");

    ellipse=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");setup=Session(ellipse);
    setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Generated",
        default_primitive("point-edit-source","nect.shape.ellipse")}},setup.revision());
    ellipse=setup.document();const auto pedit_targets=generated_refs(ellipse,"primitive","in.angle");
    window.host.session=Session(ellipse);window.host.session_id+="-stale-point-edit";window.host.edited();
    window.canvas->set_selections({{"primitive",pedit_targets[0].point},{"primitive",pedit_targets[1].point}});events();
    dial=batch_dial(window,"in.angle",pedit_targets);changed=window.host.session.document();
    changed.objects.at("primitive").point_edit=PointEdit{"point-edit-source-point-edit",1,true,{}};
    window.host.session=Session(changed);const auto pedit_bytes=encode(changed);press(dial);release(dial,0);
    check(encode(window.host.session.document())==pedit_bytes&&window.statusBar()->currentMessage().startsWith("MISSING_PROPERTY"),
        "new canonical Point Edit destination invalidates a batch frozen without one");

    auto polygon=empty_document("batch-point-document","batch-point-composition","batch-point-artboard");setup=Session(polygon);
    setup.apply({CreatePrimitive{"batch-point-composition","","primitive","Polygon",
        default_primitive("polygon-source","nect.shape.polygon")}},setup.revision());
    polygon=setup.document();const auto polygon_values=evaluate(polygon);
    const auto polygon_contour=path_contours(polygon.objects.at("primitive"),&polygon_values).front();
    const auto first=polygon_contour.points.front().id,last=polygon_contour.points.back().id;
    window.host.session=Session(polygon);window.host.session_id+="-stale-topology";window.host.edited();
    const std::vector<Ref> polygon_targets{{"primitive",first,"in.angle"},{"primitive",last,"in.angle"}};
    window.canvas->set_selections({{"primitive",first},{"primitive",last}});events();dial=batch_dial(window,"in.angle",polygon_targets);
    changed=window.host.session.document();changed.objects.at("primitive").source->parameters.at("points").literal=3;
    window.host.session=Session(changed);const auto topology_bytes=encode(changed);press(dial);release(dial,0);
    check(encode(window.host.session.document())==topology_bytes&&window.statusBar()->currentMessage().startsWith("MISSING_PROPERTY"),
        "topology removal of one selected stable point rejects the complete frozen batch");
    const auto close_path=directory.path()+"/batch-point-reopen.nect";window.host.save(close_path);window.host.open(close_path);
}
}

int main(int argc,char** argv) {
    batch_angle_test::configure_test_qpa();QApplication app(argc,argv);batch_angle_test::report_test_qpa();
    try {
        QTemporaryDir directory;check(directory.isValid(),"batch point-angle test scratch exists");
        QSettings settings(directory.filePath("settings.ini"),QSettings::IniFormat);
        Window window(directory.path(),std::make_unique<FolderLibrary>(settings));window.show();events();
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")
            check(window.devicePixelRatioF()>=1.9,"high-DPI batch-angle run uses an actual scaled widget DPR");
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="1.25")
            check(window.devicePixelRatioF()>=1.2,"125% batch-angle geometry run uses an actual fractional widget DPR");
        if(qEnvironmentVariableIsSet("NECT_GEOMETRY_ONLY")) {
            shared_caption_geometry(window);
            std::cout<<"PASS fixed-global point-batch geometry\n";return 0;
        }
        shared_caption_geometry(window);
        batch_vector_and_history(window,directory.path());field_isolation_and_generated_sources(window);
        cross_object_sources_and_atomic_rejection(window);no_net_dirty_and_range(window);
        driven_batch_rejection(window);stale_and_arbitration(window);
        teardown_and_generated_staleness(window,directory);
        std::cout<<"PASS "<<checks<<" batch point-angle checks\n";return 0;
    } catch(const std::exception& exception) {
        std::cerr<<"FAIL "<<exception.what()<<'\n';return 1;
    }
}
