#pragma once
#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <stdexcept>

// Bounded built-in operation enabled family: production Window Qt/API events
// and cold native readback. Physical OS input and full regression are separate.
// This does not introduce or qualify published custom-Macro boolean controls.
namespace semantic_toggle_window_smoke {
using namespace nect;
using namespace nect::desktop;
inline int checks=0;
inline void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
inline void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
inline const Ref target=operation_ref("target","target-repeater","enabled");
inline const Ref source=operation_ref("source","source-offset","enabled");
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{return document==session.document()&&revision==session.revision()&&
        history==session.history()&&native==encode(session.document());}
};
inline Document fixture(bool literal=true,bool source_enabled=false){
    Session session(empty_document("toggle-document","composition","artboard"));
    auto target_primitive=default_primitive("target-rectangle","nect.shape.rectangle");
    auto source_primitive=default_primitive("source-rectangle","nect.shape.rectangle");
    auto repeater=default_operation("target-repeater","nect.shape.repeater");
    repeater.enabled=literal;repeater.parameters.at("copies").literal=4;
    repeater.parameters.at("rotation").literal=-725.1234567890123;
    auto offset=default_operation("source-offset","nect.shape.offset");
    offset.enabled=source_enabled;offset.parameters.at("amount").literal=12.34567890123456;
    session.apply({CreatePrimitive{"composition","","target","Target",target_primitive},AddOperation{"target",repeater,1},
        CreatePrimitive{"composition","","source","Source",source_primitive},AddOperation{"source",offset,1}},0);
    return session.document();
}
inline QByteArray reference(const Ref& ref){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
    {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);}
inline QCheckBox* toggle(Window& window,const Ref& ref=target){
    const auto name=QString::fromStdString(std::string("operation-enabled-")+(ref==target?"target-repeater":"source-offset"));
    for(auto* widget:window.findChildren<QCheckBox*>(name))if(widget->isVisible())return widget;
    throw std::runtime_error("Visible production operation enabled toggle with stable object name is missing");
}
inline void load(Window& window,bool literal=true,bool source_enabled=false){
    window.host.session=Session(fixture(literal,source_enabled));window.host.session_id+="-toggle";window.host.edited();
    window.canvas->set_active_artboard("composition","artboard",false);window.canvas->set_selection("target");events();
}
inline void reveal(Window& window,QWidget* control){
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");require(scroll!=nullptr,"Production Inspector scroll exists");
    scroll->ensureWidgetVisible(control);events();
}
inline void click(Window& window,QCheckBox* control){
    reveal(window,control);QTest::mouseClick(control,Qt::LeftButton,Qt::NoModifier,QPoint(8,control->height()/2));events();
}
inline void history_action(Window& window,const char* label){
    for(auto* action:window.findChildren<QAction*>())if(action->text()==QString::fromLatin1(label)){
        require(action->isEnabled(),"Production history action is enabled");action->trigger();events();return;}
    throw std::runtime_error("Production history action is missing");
}
inline void presentation(Window& window,bool expected,bool driven){
    auto* control=toggle(window);const auto descriptor=property_semantic_descriptor(window.host.session.document(),target);
    require(descriptor&&descriptor->key=="enabled"&&descriptor->value_type=="boolean"&&descriptor->boolean_default==true&&
        descriptor->unit=="boolean"&&descriptor->widget_hint=="toggle"&&!descriptor->minimum&&!descriptor->maximum&&!descriptor->step&&
        validate_semantic_descriptor(*descriptor).widget==SemanticWidget::toggle,
        "Stable built-in enabled Ref projects a typed boolean descriptor without numeric constraints");
    require(control->property("nect-semantic-key")=="enabled"&&control->property("nect-widget-kind")=="toggle"&&
        control->property("nect-widget-hint")=="toggle"&&control->property("nect-value-type")=="boolean"&&
        control->property("nect-unit")=="boolean"&&control->property("nect-domain")==QString::fromStdString(descriptor->domain)&&
        control->property("nect-control-status")=="SUPPORTED"&&!control->property("nect-control-fallback").toBool()&&
        control->property("nect-exact-value").toBool()&&!control->isTristate(),
        "Production checkbox has the shared native two-state semantic toggle annotations");
    require(control->isChecked()==expected&&control->isEnabled()==!driven,
        "Production toggle displays evaluated boolean and prevents editing a link or expression");
    require(window.canvas->selected_object=="target"&&
        evaluate_shape(window.host.session.document(),"target",evaluate(window.host.session.document())).paths.size()==(expected?4u:1u),
        "Selected target and evaluated repeated geometry agree with enabled presentation");
}
inline void cold_readback(const Document& document,bool expected){
    QTemporaryDir files;require(files.isValid(),"Owned cold native directory exists");
    Host writer(files.path()+"/writer");writer.session=Session(document);
    const auto path=files.path()+"/toggle.nect";writer.save(path);writer.flush();
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window cold(files.path()+"/cold",std::make_unique<FolderLibrary>(settings));
    cold.setAttribute(Qt::WA_DontShowOnScreen);cold.resize(1400,900);cold.show();cold.host.open(path);
    cold.canvas->set_active_artboard("composition","artboard",false);cold.canvas->set_selection("target");events();
    require(cold.host.session.document()==document&&encode(cold.host.session.document())==encode(document),
        "Cold production Window reopen preserves every authored field, exact values, stable IDs and dependencies");
    const auto authored=operation_enabled_state(document,target);
    const auto reopened=operation_enabled_state(cold.host.session.document(),target);
    require(reopened.literal==authored.literal&&reopened.driver==authored.driver&&reopened.expression==authored.expression&&
        reopened.evaluated==expected,"Cold reopen retains authored boolean separately from its exact link or expression and evaluated value");
    presentation(cold,expected,authored.driver.has_value()||authored.expression.has_value());
    const auto response=QJsonDocument::fromJson(QByteArray::fromStdString(request(cold.host.session,
        "{\"op\":\"get\",\"ref\":"+reference(target).toStdString()+"}"))).object();
    require(response.value("ok").toBool(),"Cold canonical property API succeeds for the same stable enabled Ref");
    const auto property=response.value("result").toObject();const auto retained=property.value("authored").toObject();
    require(QJsonDocument(property.value("ref").toObject()).toJson(QJsonDocument::Compact)==reference(target)&&
        property.value("type")=="bool"&&property.value("evaluated").isBool()&&property.value("evaluated").toBool()==expected&&
        retained.value("literal").isBool()&&retained.value("literal").toBool()==authored.literal,
        "Cold API reports boolean authored and evaluated values without numerical coercion");
    if(authored.driver)require(QJsonDocument(retained.value("driver").toObject().value("link").toObject()).toJson(QJsonDocument::Compact)==reference(*authored.driver),
        "Cold API preserves the exact authored source Ref");
    if(authored.expression){const auto expression=retained.value("expression").toObject();
        require(retained.value("source_kind")=="expression"&&expression.value("source")==QString::fromStdString(authored.expression->source)&&
            expression.value("version").toInt()==static_cast<int>(authored.expression->version),"Cold API preserves exact expression text and version");}
    cold.host.changed={};cold.hide();
}
inline void literal_click(Window& window,bool initial){
    load(window,initial);auto& session=window.host.session;const Snapshot before(session);presentation(window,initial,false);
    auto* control=toggle(window);reveal(window,control);control->setFocus(Qt::OtherFocusReason);events();control->clearFocus();events();
    require(before.unchanged(session),"Literal toggle focus and blur are byte-exact and history-free");
    click(window,toggle(window));Session expected(before.document);expected.apply({EnableOperation{"target","target-repeater",!initial}},0);
    const auto committed=session.document();
    require(committed==expected.document()&&session.revision()==before.revision+1&&
        session.history()==expected.history()&&session.history().states.size()==before.history.states.size()+1&&!session.gesture_active(),
        "One real checkbox click commits exactly one canonical EnableOperation transaction and Undo state");
    const auto state=operation_enabled_state(committed,target);
    require(state.literal==!initial&&state.evaluated==!initial&&!state.driver&&!state.expression,
        "Literal click authors a real boolean without inventing a dependency");
    presentation(window,!initial,false);history_action(window,"Undo");
    require(session.document()==before.document&&!session.can_undo(),"One production Undo restores the entire exact literal fixture");
    presentation(window,initial,false);history_action(window,"Redo");
    require(session.document()==committed&&!session.can_redo(),"One production Redo restores the complete exact toggle result");
    presentation(window,!initial,false);cold_readback(committed,!initial);
}
inline void driven(Window& window,bool literal,bool expression){
    const bool source_initial=expression?literal:!literal;load(window,literal,source_initial);auto& session=window.host.session;
    const Expression authored_expression{" ! ref ( \"source\" , \"\" , \"op.source-offset.enabled\" ) ",1};
    if(expression)session.apply({LinkOperationEnabled{target,authored_expression}},session.revision());
    else session.apply({LinkOperationEnabled{target,source}},session.revision());
    window.host.edited();events();const Snapshot linked(session);const auto state=operation_enabled_state(session.document(),target);
    require(state.literal==literal&&state.evaluated==!literal&&
        (expression?(state.expression==authored_expression&&!state.driver):(state.driver==source&&!state.expression)),
        "Driven fixture intentionally separates authored literal from the evaluated linked or expression value");
    presentation(window,!literal,true);click(window,toggle(window));
    require(linked.unchanged(session),"A real click on the disabled driven toggle cannot replace a dependency or create history");
    const auto target_before=session.document().objects.at("target");
    session.apply({EnableOperation{"source","source-offset",!source_initial}},session.revision());window.host.edited();events();
    require(session.document().objects.at("target")==target_before&&operation_enabled_state(session.document(),target).evaluated==literal,
        "Source edit changes evaluated target without rewriting its literal, parameters, exact link or expression");
    presentation(window,literal,true);const auto changed=session.document();history_action(window,"Undo");
    require(session.document()==linked.document,"One production Undo restores source and the exact authored dependency");
    presentation(window,!literal,true);history_action(window,"Redo");
    require(session.document()==changed,"One production Redo restores source evaluation without retargeting the dependency");
    presentation(window,literal,true);cold_readback(changed,literal);
    // Also reopen when evaluated and literal disagree: this detects stale-literal
    // presentation both immediately after linking and after native persistence.
    cold_readback(linked.document,!literal);
}
inline void enum_annotations(Window& window){
    load(window);auto& session=window.host.session;auto fill=default_operation("target-fill","nect.paint.fill");
    fill.fill_rule="evenodd";session.apply({AddOperation{"target",fill,2}},session.revision());window.host.edited();events();
    const Snapshot before(session);
    for(const auto& id:{Id{"target"},Id{"source"}}){
        window.canvas->set_selection(id);events();const auto operation=id=="target"?"target-fill":"source-offset";
        QComboBox* rule=nullptr;
        for(auto* control:window.findChildren<QComboBox*>(QString::fromStdString("operation-fill-rule-"+std::string(operation))))
            if(control->isVisible()){rule=control;break;}
        require(rule&&rule->property("nect-widget-kind")=="dropdown"&&rule->property("nect-value-type")=="enum"&&
            rule->property("nect-semantic-key")=="fill_rule"&&rule->count()==2&&rule->itemData(0)=="nonzero"&&
            rule->itemData(1)=="evenodd"&&rule->currentData()==(id=="target"?"evenodd":"nonzero"),
            "Existing production Fill and Offset rule widgets carry typed dropdown metadata and exact canonical choice IDs");
    }
    require(before.unchanged(session),"Inspecting typed Fill and Offset choices does not mutate authored state or history");
}
inline int run(){
    checks=0;QTemporaryDir files;require(files.isValid(),"Owned Window test directory exists");
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1400,900);window.show();events();
    literal_click(window,true);literal_click(window,false);
    driven(window,true,false);driven(window,false,false);driven(window,true,true);driven(window,false,true);
    enum_annotations(window);
    window.host.changed={};window.hide();return checks;
}
}
