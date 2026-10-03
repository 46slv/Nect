#pragma once
#include "window.hpp"
#include "nect/semantic_controls.hpp"
#include "semantic_color_control.hpp"
#include <QAction>
#include <QApplication>
#include <QColorDialog>
#include <QEvent>
#include <QJsonArray>
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

// Bounded built-in Fill/Stroke solid-color checkpoint. Production Window,
// deterministic Qt picker acceptance and canonical API/native readback only;
// physical OS input, published Macro color controls and full regression are separate.
namespace semantic_color_window_smoke {
using namespace nect;
using namespace nect::desktop;
using Rgba=std::array<double,4>;
inline int checks=0;
inline void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
inline void events(){
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    // Queued commits can rebuild the Inspector in the first event pass.
    // Drain the resulting layout/show events before testing the new rows.
    QApplication::processEvents();
}
inline const Rgba exact_fill{0.1234567890123456,0.3456789012345678,0.567890123456789,0.7890123456789012};
inline const Rgba exact_stroke{0.2345678901234567,0.4567890123456789,0.6789012345678901,0.8901234567890123};
inline const Ref fill=operation_ref("target","target-fill","color");
inline const Ref stroke=operation_ref("target","target-outline","color");
inline const char* channels[]={"r","g","b","a"};
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{return document==session.document()&&revision==session.revision()&&
        history==session.history()&&native==encode(session.document());}
};
inline std::string operation_id(const Ref& ref){return ref==fill?"target-fill":"target-outline";}
inline Ref channel_ref(const Ref& ref,std::size_t index){return operation_ref(ref.object,operation_id(ref),channels[index]);}
inline ColorValue color(const Rgba& rgba){ColorValue value;value.rgba=rgba;return value;}
inline Rgba value(const Document& document,const Ref& ref){return color_value(document,ref,evaluate(document)).rgba;}
inline QColor displayed(const Rgba& rgba){return QColor::fromRgbF(rgba[0],rgba[1],rgba[2],rgba[3]);}
inline QString hex(const Rgba& rgba){QString result="#";
    for(const auto channel:rgba)result+=QString("%1").arg(qRound(channel*255),2,16,QChar('0'));
    return result.toUpper();}
inline Document fixture(){
    Session session(empty_document("color-document","composition","artboard"));
    auto primitive=default_primitive("target-rectangle","nect.shape.rectangle");
    primitive.parameters.at("width").literal=123.4567890123456;
    auto paint=default_operation("target-fill","nect.paint.fill");
    auto outline=default_operation("target-outline","nect.paint.stroke");
    outline.parameters.at("width").literal=3.456789012345678;
    for(std::size_t i=0;i<4;++i){paint.parameters.at(channels[i]).literal=exact_fill[i];outline.parameters.at(channels[i]).literal=exact_stroke[i];}
    NamedColor source;source.id="source-color";source.name="Exact source";
    for(std::size_t i=0;i<4;++i)source.rgba[i].literal=0.1+static_cast<double>(i)*0.2;
    session.apply({CreatePrimitive{"composition","","target","Target",primitive},AddOperation{"target",paint,1},
        AddOperation{"target",outline,2},CreateNamedColor{source}},0);
    return session.document();
}
inline QByteArray reference(const Ref& ref){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
    {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);}
inline SemanticColorInput* input(Window& window,const Ref& ref){
    const auto name=QString::fromStdString("semantic-color-"+operation_id(ref));
    for(auto* widget:window.findChildren<QWidget*>(name))if(widget->isVisible()){
        auto* result=dynamic_cast<SemanticColorInput*>(widget);require(result!=nullptr,"Production color row uses the shared SemanticColorInput");return result;}
    throw std::runtime_error("Visible production semantic color row is missing");
}
inline void load(Window& window){
    window.host.session=Session(fixture());window.host.session_id+="-color";window.host.edited();
    window.canvas->set_active_artboard("composition","artboard",false);window.canvas->set_selection("target");events();
}
inline void reveal(Window& window,QWidget* widget){
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");require(scroll!=nullptr,"Production Inspector scroll exists");
    scroll->ensureWidgetVisible(widget);events();
}
inline void edit(Window& window,const Ref& ref,const QString& text){auto* field=input(window,ref)->hex_input();reveal(window,field);
    field->setFocus(Qt::OtherFocusReason);field->selectAll();QTest::keyClick(field,Qt::Key_Backspace);
    QTest::keyClicks(field,text);QTest::keyClick(field,Qt::Key_Return);events();}
inline void history_action(Window& window,const char* label){
    for(auto* action:window.findChildren<QAction*>())if(action->text()==QString::fromLatin1(label)){
        require(action->isEnabled(),"Production history action is enabled");action->trigger();events();return;}
    throw std::runtime_error("Production history action is missing");
}
inline void presentation(Window& window,const Ref& ref,const Rgba& expected,bool driven=false){
    auto* control=input(window,ref);auto* swatch=control->swatch_button();auto* field=control->hex_input();
    const auto descriptor=property_semantic_descriptor(window.host.session.document(),ref);
    require(descriptor&&descriptor->key=="color"&&descriptor->value_type=="color"&&descriptor->color_default&&
        descriptor->unit=="srgb"&&descriptor->widget_hint=="color"&&!descriptor->minimum&&!descriptor->maximum&&!descriptor->step&&
        validate_semantic_descriptor(*descriptor).widget==SemanticWidget::color,
        "Stable built-in paint aggregate selects the typed sRGB color descriptor");
    for(auto* widget:std::array<QWidget*,3>{control,swatch,field})require(
        widget->property("nect-semantic-key")=="color"&&widget->property("nect-value-type")=="color"&&
        widget->property("nect-unit")=="srgb"&&widget->property("nect-domain")==QString::fromStdString(descriptor->domain)&&
        widget->property("nect-widget-hint")=="color"&&widget->property("nect-widget-kind")=="color"&&
        widget->property("nect-control-status")=="SUPPORTED"&&!widget->property("nect-control-fallback").toBool()&&
        widget->property("nect-exact-value").toBool(),"Production container, swatch and HEX carry shared supported color annotations");
    require(control->value()==expected&&field->text()==hex(expected)&&!field->isModified()&&
        swatch->objectName()==QString::fromStdString("operation-color-"+operation_id(ref))&&
        field->objectName()==QString::fromStdString("operation-hex-"+operation_id(ref))&&!swatch->icon().isNull()&&
        control->isEnabled()==!driven&&swatch->isEnabled()==!driven&&field->isEnabled()==!driven,
        "Exact baseline and rounded presentation retain compatible names, swatch and entire-row driven disablement");
}
inline void cold_readback(const Document& document,const Ref& ref){
    QTemporaryDir files;require(files.isValid(),"Owned cold native directory exists");
    Host writer(files.path()+"/writer");writer.session=Session(document);
    const auto path=files.path()+"/color.nect";writer.save(path);writer.flush();
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window cold(files.path()+"/cold",std::make_unique<FolderLibrary>(settings));
    cold.setAttribute(Qt::WA_DontShowOnScreen);cold.resize(1400,900);cold.show();cold.host.open(path);
    cold.canvas->set_active_artboard("composition","artboard",false);cold.canvas->set_selection("target");events();
    require(cold.host.session.document()==document&&encode(cold.host.session.document())==encode(document),
        "Cold production Window reopen preserves every exact authored value, stable ID, link and expression");
    bool driven=false;for(const auto& channel:color_channels(document,ref)){
        const auto scalar=property(document,channel);driven=driven||scalar.binding.has_value()||scalar.expression.has_value();}
    const auto expected=value(document,ref);presentation(cold,ref,expected,driven);
    const auto response=QJsonDocument::fromJson(QByteArray::fromStdString(request(cold.host.session,
        "{\"op\":\"get\",\"ref\":"+reference(ref).toStdString()+"}"))).object();
    require(response.value("ok").toBool(),"Cold canonical color aggregate API succeeds");
    const auto result=response.value("result").toObject();const auto evaluated=result.value("evaluated").toObject();
    const auto rgba=evaluated.value("rgba").toArray();const auto refs=result.value("channels").toArray();
    const auto authored=result.value("authored").toArray();
    require(QJsonDocument(result.value("ref").toObject()).toJson(QJsonDocument::Compact)==reference(ref)&&
        result.value("type")=="color"&&evaluated.value("space")=="srgb"&&evaluated.value("profile")=="srgb"&&
        evaluated.value("alpha")=="straight"&&rgba.size()==4&&refs.size()==4&&authored.size()==4,
        "Cold aggregate API retains typed sRGB/straight alpha and all four stable channel records");
    for(std::size_t i=0;i<4;++i){const auto index=static_cast<qsizetype>(i);const auto channel=channel_ref(ref,i);
        require(rgba.at(index).toDouble()==expected[i]&&
            QJsonDocument(refs.at(index).toObject()).toJson(QJsonDocument::Compact)==reference(channel)&&
            authored.at(index).toObject().value("literal").toDouble()==property(document,channel).literal,
            "Cold color API reports exact evaluated doubles and authored channel literals at the same stable references");}
    cold.host.changed={};cold.hide();
}
inline void committed(Window& window,const Ref& ref,const Snapshot& before,const Rgba& next){
    auto& session=window.host.session;Session expected(before.document);expected.apply({SetColor{ref,color(next)}},0);
    require(session.document()==expected.document()&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1&&
        session.history().states.back().label==expected.history().states.back().label&&!session.gesture_active(),
        "One color edit commits one canonical atomic SetColor transaction without changing any unrelated authored field");
    const auto result=session.document();presentation(window,ref,next);history_action(window,"Undo");
    require(session.document()==before.document&&encode(session.document())==before.native,"One production Undo restores all exact color doubles and other fields");
    history_action(window,"Redo");require(session.document()==result&&!session.can_redo(),"One production Redo restores the complete canonical color result");
    cold_readback(result,ref);
}
inline void literal(Window& window,const Ref& ref){
    load(window);auto& session=window.host.session;const Snapshot before(session);const auto exact=value(before.document,ref);
    presentation(window,ref,exact);cold_readback(before.document,ref);
    auto* control=input(window,ref);auto* field=control->hex_input();reveal(window,field);
    field->setFocus(Qt::OtherFocusReason);events();field->clearFocus();events();
    require(before.unchanged(session),"Color focus and blur preserve native high-precision values and history");
    control=input(window,ref);require(!control->accept_picker_color(displayed(exact)),"Unchanged picker result requests no color edit");events();
    require(before.unchanged(session)&&input(window,ref)->value()==exact,"Accepting Qt's unchanged quantized color preserves all authored doubles exactly");
    edit(window,ref,hex(exact));require(before.unchanged(session),"Retyping the displayed eight-digit HEX is byte-exact and history-free");
    control=input(window,ref);require(!control->accept_picker_color(QColor{}),"Invalid picker result represents cancellation");events();
    require(before.unchanged(session),"Picker cancellation creates no native or history mutation");
    const auto chosen=QColor::fromRgbF(0.8,0.2,0.4,0.6);
    const Rgba picked{chosen.redF(),chosen.greenF(),chosen.blueF(),chosen.alphaF()};
    require(input(window,ref)->accept_picker_color(chosen),"Changed deterministic picker acceptance requests the production commit");events();
    committed(window,ref,before,picked);
    // Change one byte only. The unchanged green/blue/alpha bytes must retain
    // their native precision, including values that cannot round-trip QColor.
    load(window);const Snapshot hex_before(session);auto next=exact;next[0]=0xee/255.0;
    auto text=hex(exact);text.replace(1,2,"EE");edit(window,ref,text);committed(window,ref,hex_before,next);
    const Snapshot invalid(session);edit(window,ref,"#bad");
    require(invalid.unchanged(session)&&!input(window,ref)->error_text().isEmpty(),"Invalid HEX remains a visible non-mutating draft");
    field=input(window,ref)->hex_input();QTest::keyClick(field,Qt::Key_Escape);events();
    require(invalid.unchanged(session)&&input(window,ref)->value()==next&&field->text()==hex(next)&&!field->isModified(),
        "Escape cancels an invalid HEX draft and restores exact baseline without authoring");
    field->selectAll();QTest::keyClicks(field,"#01020304");QTest::keyClick(field,Qt::Key_Escape);events();
    require(invalid.unchanged(session)&&field->text()==hex(next),"Escape also cancels a valid unsubmitted color draft");
}
inline void driven(Window& window,const Ref& ref,std::size_t channel,bool expression){
    load(window);auto& session=window.host.session;const auto target=channel_ref(ref,channel);
    const Ref source{"source-color","",std::string("color.")+channels[channel]};
    const Expression authored_expression{" ref ( \"source-color\" , \"\" , \"color."+std::string(channels[channel])+"\" ) ",1};
    if(expression)session.apply({SetExpression{{target},authored_expression,false}},session.revision());
    else session.apply({LinkProperties{{target},source,false}},session.revision());
    window.host.edited();events();const Snapshot linked(session);const auto exact=value(linked.document,ref);
    const auto scalar=property(linked.document,target);
    require(scalar.literal==(ref==fill?exact_fill:exact_stroke)[channel]&&
        (expression?(scalar.expression==authored_expression&&!scalar.binding):(scalar.binding&&scalar.binding->source==source&&!scalar.expression)),
        "Single-channel driver retains its separate exact literal and exact authored link or expression");
    presentation(window,ref,exact,true);auto* control=input(window,ref);reveal(window,control);
    QTest::mouseClick(control->swatch_button(),Qt::LeftButton);events();
    require(!control->accept_picker_color(QColor::fromRgbF(1,0,0,1)),"Disabled picker seam cannot replace even one driven channel");
    edit(window,ref,"#01020304");require(linked.unchanged(session),"Disabled swatch/HEX cannot replace a driver, edit independent channels or add history");
    auto* tools=window.findChild<QObject*>("color-tools");require(tools!=nullptr,"Existing explicit Color tools remain available for driven colors");
    cold_readback(linked.document,ref);
}
inline void interrupted(Window& window){
    const auto chosen=QColor::fromRgbF(0.8,0.2,0.4,0.6);
    load(window);auto& session=window.host.session;auto* control=input(window,fill);
    session.apply({Rename{"target","Changed without Inspector refresh"}},session.revision());const Snapshot stale(session);
    control->accept_picker_color(chosen);events();
    require(stale.unchanged(session)&&window.statusBar()->currentMessage().contains("REVISION_CONFLICT"),
        "Revision-stale production color commit refuses atomically");
    load(window);control=input(window,fill);window.host.session_id+="-replacement";const Snapshot replaced(session);
    control->accept_picker_color(chosen);events();
    require(replaced.unchanged(session)&&window.statusBar()->currentMessage().contains("SESSION_CONFLICT"),
        "Retained color input cannot commit into a replacement document session");
    load(window);control=input(window,fill);session.begin_gesture(session.revision());
    session.update_gesture({Set{operation_ref("target","target-outline","width"),8}});
    const Snapshot active(session);const auto preview=session.preview_document();control->accept_picker_color(chosen);events();
    require(active.unchanged(session)&&session.gesture_active()&&session.preview_document()==preview&&
        window.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Color refusal preserves an unrelated active gesture and its exact preview");
    session.cancel_gesture();window.host.edited();events();
    load(window);const Snapshot canceled(session);control=input(window,fill);reveal(window,control);
    QTest::mouseClick(control->swatch_button(),Qt::LeftButton);events();
    QPointer<QColorDialog> picker;
    for(auto* dialog:window.findChildren<QColorDialog*>("semantic-color-picker"))if(dialog->isVisible()){picker=dialog;break;}
    require(picker&&picker->currentColor().isValid(),"Real production swatch opens a valid QColorDialog draft");
    picker->setCurrentColor(chosen);picker->reject();events();
    require(canceled.unchanged(session),"Rejecting a changed real QColorDialog leaves high-precision authored values and history untouched");
    control=input(window,fill);QTest::mouseClick(control->swatch_button(),Qt::LeftButton);events();picker.clear();
    for(auto* dialog:window.findChildren<QColorDialog*>("semantic-color-picker"))if(dialog->isVisible()){picker=dialog;break;}
    require(picker!=nullptr,"A fresh production picker opens after cancellation");picker->setCurrentColor(chosen);
    QPointer<SemanticColorInput> retained(control);window.canvas->set_selection({});
    QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);events();
    require(!retained&&!picker&&canceled.unchanged(session),"Inspector replacement destroys the unfinished picker without committing its color draft");
    window.canvas->set_selection("target");events();presentation(window,fill,exact_fill);
}
inline int run(){
    checks=0;QTemporaryDir files;require(files.isValid(),"Owned Window color test directory exists");
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1400,900);window.show();events();
    literal(window,fill);literal(window,stroke);
    for(const auto& ref:{fill,stroke})for(std::size_t i=0;i<4;++i){driven(window,ref,i,false);driven(window,ref,i,true);}
    interrupted(window);window.host.changed={};window.hide();return checks;
}
}
