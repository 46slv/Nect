#pragma once
#include "window.hpp"
#include "nect/semantic_controls.hpp"
#include "semantic_point_control.hpp"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <array>
#include <memory>
#include <stdexcept>

// Bounded local Gradient Start/End pair checkpoint: production Window Qt/API
// events and cold native readback. Physical OS input, general point controls,
// published Macro controls and full regression are separate qualifications.
namespace semantic_point_window_smoke {
using namespace nect;
using namespace nect::desktop;
using Pair=std::array<double,2>;
inline int checks=0;
inline void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
inline void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
inline const Pair linear_start{-12.34567890123456,23.45678901234567};
inline const Pair linear_end{134.5678901234567,-45.67890123456789};
inline const Pair radial_start{34.56789012345678,-56.7890123456789};
inline const Pair radial_end{156.7890123456789,67.89012345678901};
struct Endpoint {
    Id operation,gradient;std::string endpoint;
    Ref x()const{return gradient_ref("target",operation,gradient,endpoint+"_x");}
    Ref y()const{return gradient_ref("target",operation,gradient,endpoint+"_y");}
};
inline const std::array<Endpoint,4> endpoints{{
    {"target-fill","linear-gradient","start"},{"target-fill","linear-gradient","end"},
    {"target-outline","radial-gradient","start"},{"target-outline","radial-gradient","end"}}};
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{return document==session.document()&&revision==session.revision()&&
        history==session.history()&&native==encode(session.document());}
};
inline Gradient gradient(const Id& id,const std::string& type,const Pair& start,const Pair& end){
    Gradient result;result.id=id;result.type=type;
    result.start_x.literal=start[0];result.start_y.literal=start[1];
    result.end_x.literal=end[0];result.end_y.literal=end[1];
    GradientStop first;first.id=id+"-first";first.offset.literal=0;
    first.rgba[0].literal=0.1234567890123456;first.rgba[1].literal=0.2345678901234567;
    first.rgba[2].literal=0.3456789012345678;first.rgba[3].literal=0.4567890123456789;
    GradientStop last;last.id=id+"-last";last.offset.literal=1;
    last.rgba[0].literal=0.567890123456789;last.rgba[1].literal=0.6789012345678901;
    last.rgba[2].literal=0.7890123456789012;last.rgba[3].literal=0.8901234567890123;
    result.stops={first,last};return result;
}
inline Document fixture(){
    Session session(empty_document("point-document","composition","artboard"));
    auto primitive=default_primitive("target-rectangle","nect.shape.rectangle");
    primitive.parameters.at("width").literal=234.5678901234567;
    auto fill=default_operation("target-fill","nect.paint.fill");
    fill.gradient=gradient("linear-gradient","linear",linear_start,linear_end);
    fill.parameters.at("a").literal=0.6789012345678901;fill.fill_rule="evenodd";
    auto outline=default_operation("target-outline","nect.paint.stroke");
    outline.gradient=gradient("radial-gradient","radial",radial_start,radial_end);
    outline.parameters.at("width").literal=3.456789012345678;
    auto source=default_primitive("source-rectangle","nect.shape.rectangle");
    source.parameters.at("width").literal=78.90123456789012;
    source.parameters.at("height").literal=89.01234567890123;
    session.apply({CreatePrimitive{"composition","","target","Target",primitive},AddOperation{"target",fill,1},
        AddOperation{"target",outline,2},CreatePrimitive{"composition","","source","Source",source}},0);
    return session.document();
}
inline Pair value(const Document& document,const Endpoint& endpoint){const auto values=evaluate(document);
    return {values.at(endpoint.x()),values.at(endpoint.y())};}
inline QString number(double value){return QString::number(value,'g',17);}
inline QByteArray reference(const Ref& ref){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
    {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);}
inline void load(Window& window){
    window.host.session=Session(fixture());window.host.session_id+="-point";window.host.edited();
    window.canvas->set_active_artboard("composition","artboard",false);window.canvas->set_selection("target");events();
}
inline QLineEdit* scalar(Window& window,const Ref& ref){
    for(auto* field:window.findChildren<QLineEdit*>())
        if(field->isVisible()&&field->property("nect-reference").toByteArray()==reference(ref))return field;
    throw std::runtime_error("Visible canonical gradient scalar source field is missing");
}
inline QPushButton* button(Window& window,const Endpoint& endpoint){
    const auto name=QString::fromStdString("gradient-point-edit-"+endpoint.operation+"-"+endpoint.endpoint);
    for(auto* control:window.findChildren<QPushButton*>(name))if(control->isVisible())return control;
    throw std::runtime_error("Visible production gradient endpoint pair button is missing");
}
inline void reveal(Window& window,QWidget* control){
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");require(scroll!=nullptr,"Production Inspector scroll exists");
    scroll->ensureWidgetVisible(control);events();
}
inline SemanticPointInput* editor(QDialog& dialog){
    auto* widget=dialog.findChild<QWidget*>("semantic-gradient-point-input");
    auto* result=dynamic_cast<SemanticPointInput*>(widget);
    require(result!=nullptr,"Production endpoint dialog uses the shared SemanticPointInput without requiring Q_OBJECT");return result;
}
inline QPointer<QDialog> open(Window& window,const Endpoint& endpoint){
    auto* control=button(window,endpoint);require(control->isEnabled(),"Literal endpoint pair button is enabled");
    reveal(window,control);QTest::mouseClick(control,Qt::LeftButton);events();
    for(auto* dialog:window.findChildren<QDialog*>("semantic-gradient-point-dialog"))if(dialog->isVisible()){
        editor(*dialog);return dialog;}
    throw std::runtime_error("Real endpoint button click did not open its production dialog");
}
inline void draft(QLineEdit* field,const QString& text){
    field->setFocus(Qt::OtherFocusReason);field->selectAll();QTest::keyClick(field,Qt::Key_Backspace);QTest::keyClicks(field,text);
}
inline void draft(SemanticPointInput* input,const Pair& next){draft(input->x_input(),number(next[0]));draft(input->y_input(),number(next[1]));}
inline void apply(SemanticPointInput* input){QTest::mouseClick(input->apply_button(),Qt::LeftButton);events();}
inline void cancel(QPointer<QDialog>& dialog){
    require(dialog!=nullptr,"Unsubmitted endpoint dialog still exists");
    QTest::mouseClick(editor(*dialog)->cancel_button(),Qt::LeftButton);events();
    require(!dialog,"Cancel closes and deletes the production endpoint draft dialog");
}
inline void history_action(Window& window,const char* label){
    for(auto* action:window.findChildren<QAction*>())if(action->text()==QString::fromLatin1(label)){
        require(action->isEnabled(),"Production history action is enabled");action->trigger();events();return;}
    throw std::runtime_error("Production history action is missing");
}
inline void source_fields(Window& window){
    const Snapshot before(window.host.session);
    for(const auto& endpoint:endpoints)for(const auto& ref:{endpoint.x(),endpoint.y()}){
        auto* field=scalar(window,ref);auto* row=field->parentWidget();
        auto* source=row->findChild<QPushButton*>("property-source-pick");
        auto* expression=row->findChild<QPushButton*>("property-expression");
        require(source&&expression&&source->isVisible()&&expression->isVisible()&&
            source->property("nect-reference").toByteArray()==reference(ref)&&field->contextMenuPolicy()==Qt::CustomContextMenu,
            "Each canonical endpoint scalar retains its source picker, expression button and property context menu");
    }
    require(before.unchanged(window.host.session),"Inspecting the retained scalar source controls is byte-exact and history-free");
}
inline void presentation(QDialog& dialog,const Endpoint& endpoint,const Pair& expected){
    auto* input=editor(dialog);const auto descriptor=gradient_endpoint_semantic_descriptor(endpoint.endpoint);
    const Pair default_value=endpoint.endpoint=="start"?Pair{0,0}:Pair{100,0};
    require(descriptor.key==endpoint.endpoint&&descriptor.value_type=="point"&&descriptor.unit=="du"&&
        descriptor.domain=="local_gradient"&&descriptor.coordinate_space=="local"&&descriptor.point_default==default_value&&
        descriptor.widget_hint=="point"&&!descriptor.minimum&&!descriptor.maximum&&!descriptor.step&&
        validate_semantic_descriptor(descriptor).widget==SemanticWidget::point,
        "Gradient Start/End descriptors have typed canonical local point defaults without scalar constraints");
    require(input->property("nect-semantic-key")==QString::fromStdString(endpoint.endpoint)&&
        input->property("nect-value-type")=="point"&&input->property("nect-unit")=="du"&&
        input->property("nect-domain")=="local_gradient"&&input->property("nect-widget-kind")=="point"&&
        input->property("nect-widget-hint")=="point"&&input->property("nect-control-status")=="SUPPORTED"&&
        !input->property("nect-control-fallback").toBool()&&input->property("nect-exact-value").toBool(),
        "Production point editor carries shared supported point metadata");
    require(input->value()==expected&&input->x_input()->text()==number(expected[0])&&input->y_input()->text()==number(expected[1])&&
        !input->x_input()->isModified()&&!input->y_input()->isModified()&&input->isEnabled()&&
        !input->x_input()->accessibleName().isEmpty()&&!input->y_input()->accessibleName().isEmpty(),
        "Production editor displays both exact baseline doubles with accessible unmodified literal drafts");
}
inline void cold_readback(const Document& document){
    QTemporaryDir files;require(files.isValid(),"Owned cold native directory exists");
    Host writer(files.path()+"/writer");writer.session=Session(document);
    const auto path=files.path()+"/point.nect";writer.save(path);writer.flush();
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window cold(files.path()+"/cold",std::make_unique<FolderLibrary>(settings));
    cold.setAttribute(Qt::WA_DontShowOnScreen);cold.resize(1400,900);cold.show();cold.host.open(path);
    cold.canvas->set_active_artboard("composition","artboard",false);cold.canvas->set_selection("target");events();
    require(cold.host.session.document()==document&&encode(cold.host.session.document())==encode(document),
        "Cold production Window reopen preserves every exact endpoint, stop, stable ID, source and unrelated authored field");
    source_fields(cold);
    for(const auto& endpoint:endpoints){const auto expected=value(document,endpoint);
        const auto x=property(document,endpoint.x()),y=property(document,endpoint.y());
        const bool driven=x.binding||x.expression||y.binding||y.expression;
        require(button(cold,endpoint)->isEnabled()==!driven,"Cold pair button derives disablement from either coordinate's canonical source");
        for(const auto& ref:{endpoint.x(),endpoint.y()}){
            const auto response=QJsonDocument::fromJson(QByteArray::fromStdString(request(cold.host.session,
                "{\"op\":\"get\",\"ref\":"+reference(ref).toStdString()+"}"))).object();
            require(response.value("ok").toBool(),"Cold canonical endpoint scalar API succeeds");
            const auto result=response.value("result").toObject(),authored=result.value("authored").toObject();
            const auto retained=property(document,ref);const auto coordinate=ref==endpoint.x()?expected[0]:expected[1];
            require(QJsonDocument(result.value("ref").toObject()).toJson(QJsonDocument::Compact)==reference(ref)&&
                result.value("evaluated").toDouble()==coordinate&&authored.value("literal").toDouble()==retained.literal,
                "Cold scalar API reports exact authored and evaluated doubles at the same canonical gradient coordinate Ref");
            if(retained.binding){const auto binding=authored.value("binding").toObject();
                require(QJsonDocument(binding.value("source").toObject()).toJson(QJsonDocument::Compact)==reference(retained.binding->source)&&
                    binding.value("scale").toDouble()==retained.binding->scale&&binding.value("offset").toDouble()==retained.binding->offset&&
                    binding.value("mode")==QString::fromStdString(retained.binding->mode),"Cold scalar API retains the exact coordinate link");}
            if(retained.expression){const auto expression=authored.value("expression").toObject();
                require(expression.value("source")==QString::fromStdString(retained.expression->source)&&
                    expression.value("version").toInt()==static_cast<int>(retained.expression->version),
                    "Cold scalar API retains exact coordinate expression text and version");}
        }
        if(!driven){auto dialog=open(cold,endpoint);presentation(*dialog,endpoint,expected);cancel(dialog);}
    }
    cold.host.changed={};cold.hide();
}
inline void literal(Window& window,const Endpoint& endpoint){
    load(window);auto& session=window.host.session;const Snapshot before(session);const auto exact=value(before.document,endpoint);
    source_fields(window);auto dialog=open(window,endpoint);presentation(*dialog,endpoint,exact);
    auto* input=editor(*dialog);input->x_input()->setFocus(Qt::OtherFocusReason);events();input->x_input()->clearFocus();events();
    require(before.unchanged(session),"Opening and focusing an endpoint draft preserves exact authored values and history");
    draft(input,exact);apply(input);
    require(dialog&&before.unchanged(session),"Applying the displayed exact unchanged pair is history-free");
    presentation(*dialog,endpoint,exact);
    const Pair next{-98.76543210987654,210.9876543210987};draft(editor(*dialog),next);
    require(before.unchanged(session),"Changing both endpoint drafts does not preview or author either scalar");
    apply(editor(*dialog));require(!dialog,"Successful pair Apply closes and deletes the production dialog");
    Session expected(before.document);expected.apply({Set{endpoint.x(),next[0]},Set{endpoint.y(),next[1]}},0);
    const auto committed=session.document();
    require(committed==expected.document()&&session.revision()==before.revision+1&&session.history()==expected.history()&&
        session.history().states.size()==before.history.states.size()+1&&!session.gesture_active(),
        "One real Apply commits exactly two canonical Set commands in one atomic transaction and Undo state");
    require(value(committed,endpoint)==next&&property(committed,endpoint.x()).literal==next[0]&&property(committed,endpoint.y()).literal==next[1],
        "Both coordinates retain exact entered doubles after the atomic production commit");
    source_fields(window);history_action(window,"Undo");
    require(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
        "One production Undo restores both coordinates, gradient stops and every unrelated authored field exactly");
    history_action(window,"Redo");require(session.document()==committed&&!session.can_redo(),
        "One production Redo restores the complete exact endpoint pair");
    cold_readback(committed);
}
inline void invalid_and_cancel(Window& window){
    load(window);auto& session=window.host.session;const Snapshot before(session);const auto& endpoint=endpoints[0];
    auto dialog=open(window,endpoint);const auto exact=value(before.document,endpoint);auto* input=editor(*dialog);
    // A valid X plus invalid Y must never commit the first coordinate alone.
    draft(input->x_input(),"999.1234567890123");draft(input->y_input(),"not-a-number");apply(input);
    require(dialog&&before.unchanged(session)&&!editor(*dialog)->error_text().isEmpty()&&editor(*dialog)->value()==exact,
        "One invalid coordinate leaves both authored coordinates and history untouched with a visible error");
    input=editor(*dialog);draft(input->x_input(),"nan");draft(input->y_input(),"888.2345678901234");apply(input);
    require(before.unchanged(session)&&!editor(*dialog)->error_text().isEmpty(),"Nonfinite X cannot commit a valid Y alone");
    input=editor(*dialog);QTest::keyClick(input->y_input(),Qt::Key_Escape);events();presentation(*dialog,endpoint,exact);
    require(before.unchanged(session),"Escape resets an invalid point draft without creating history");
    draft(editor(*dialog),Pair{111.1234567890123,-222.2345678901234});cancel(dialog);
    require(before.unchanged(session),"Cancel discards both changed coordinate drafts without native or history mutation");
    dialog=open(window,endpoint);presentation(*dialog,endpoint,exact);cancel(dialog);
    require(before.unchanged(session),"Reopening after Cancel displays the original exact pair");
}
inline void driven(Window& window,const Endpoint& endpoint,bool y_coordinate,bool expression){
    load(window);auto& session=window.host.session;const auto target=y_coordinate?endpoint.y():endpoint.x();
    const Ref source{"source","",std::string("generator.")+(y_coordinate?"height":"width")};
    const Expression formula{" ref ( \"source\" , \"\" , \"generator."+
        std::string(y_coordinate?"height":"width")+"\" ) ",1};
    const auto literal=property(session.document(),target).literal;
    if(expression)session.apply({SetExpression{{target},formula,false}},session.revision());
    else session.apply({LinkProperties{{target},source,false}},session.revision());
    window.host.edited();events();const Snapshot before(session);const auto retained=property(before.document,target);
    require(retained.literal==literal&&evaluate(before.document).at(target)!=literal&&
        (expression?(retained.expression==formula&&!retained.binding):(retained.binding&&retained.binding->source==source&&!retained.expression)),
        "Driven fixture separates its exact endpoint literal from its authored source and evaluated value");
    source_fields(window);auto* control=button(window,endpoint);require(!control->isEnabled(),"Either coordinate source disables atomic literal pair editing");
    reveal(window,control);QTest::mouseClick(control,Qt::LeftButton);events();
    require(before.unchanged(session)&&!window.findChild<QDialog*>("semantic-gradient-point-dialog"),
        "Clicking a driven pair button cannot open a replacement draft, unlink either coordinate or create history");
    for(const auto& other:endpoints)if(other.operation==endpoint.operation&&other.endpoint!=endpoint.endpoint)
        require(button(window,other)->isEnabled(),"Driving one endpoint leaves the independent endpoint pair available");
    cold_readback(before.document);
}
inline void refusal(Window& window,const char* conflict){
    load(window);auto& session=window.host.session;const auto& endpoint=endpoints[0];
    auto dialog=open(window,endpoint);const auto exact=value(session.document(),endpoint);
    if(std::string(conflict)=="REVISION_CONFLICT")session.apply({Rename{"target","Changed without Inspector refresh"}},session.revision());
    else if(std::string(conflict)=="SESSION_CONFLICT")window.host.session_id+="-replacement";
    else {session.begin_gesture(session.revision());session.update_gesture({Set{operation_ref("target","target-outline","width"),8.123456789012345}});}
    const Snapshot before(session);const auto preview=session.preview_document();const auto generation=session.gesture_generation();
    draft(editor(*dialog),Pair{777.1234567890123,-888.2345678901234});apply(editor(*dialog));
    require(dialog&&before.unchanged(session)&&window.statusBar()->currentMessage().contains(QString::fromLatin1(conflict)),
        "Stale revision, replacement session or active gesture refuses an atomic pair Apply without authored or history mutation");
    presentation(*dialog,endpoint,exact);
    require(session.preview_document()==preview&&session.gesture_generation()==generation&&
        session.gesture_active()==(std::string(conflict)=="GESTURE_ACTIVE"),
        "Failed pair Apply resets its draft and preserves unrelated gesture identity and exact preview");
    cancel(dialog);if(session.gesture_active())session.cancel_gesture();window.host.edited();events();
}
inline void driver_guard(Window& window,bool expression){
    load(window);auto& session=window.host.session;const auto& endpoint=endpoints[0];
    auto dialog=open(window,endpoint);const auto exact=value(session.document(),endpoint);
    Session driven_session(session.document());const Ref source{"source","","generator.height"};
    if(expression)driven_session.apply({SetExpression{{endpoint.y()},{"ref(\"source\",\"\",\"generator.height\")",1},false}},0);
    else driven_session.apply({LinkProperties{{endpoint.y()},source,false}},0);
    // Defensive host-replacement seam: retain the frozen identity and revision
    // while replacing authored sources. This isolates the final driver guard
    // from the earlier revision-conflict check; no production mutation bypass.
    session=Session(driven_session.document());const Snapshot before(session);
    draft(editor(*dialog),Pair{333.1234567890123,444.2345678901234});apply(editor(*dialog));
    require(dialog&&before.unchanged(session)&&window.statusBar()->currentMessage().contains("DRIVEN_PROPERTY"),
        "Final production driver recheck refuses a new link or expression even when frozen identity and revision match");
    presentation(*dialog,endpoint,exact);cancel(dialog);window.host.edited();events();
    require(!button(window,endpoint)->isEnabled(),"Refreshing a refused source replacement disables its pair button");
}
inline int run(){
    checks=0;QTemporaryDir files;require(files.isValid(),"Owned Window point test directory exists");
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1400,900);window.show();events();
    for(const auto& endpoint:endpoints)literal(window,endpoint);
    invalid_and_cancel(window);
    for(const auto& endpoint:endpoints)for(const bool y:{false,true})for(const bool expression:{false,true})driven(window,endpoint,y,expression);
    refusal(window,"REVISION_CONFLICT");refusal(window,"SESSION_CONFLICT");refusal(window,"GESTURE_ACTIVE");
    driver_guard(window,false);driver_guard(window,true);
    window.host.changed={};window.hide();return checks;
}
}
