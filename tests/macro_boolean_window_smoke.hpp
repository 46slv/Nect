#pragma once
#include "nect/core.hpp"
#include "nect/io.hpp"
#include "nect/semantic_controls.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <functional>
#include <stdexcept>
#include <string>

// Focused native087 public-Macro boolean Window/API smoke. The fixture follows
// macro_boolean_interface_tests.cpp, retaining a legacy revision before interface3.
// NECT_MACRO_BOOLEAN_API_ONLY permits the same fixture/API checks without Qt.
// Qt events qualify production wiring, not physical OS input or full regression.
namespace macro_boolean_window_smoke {
using namespace nect;
namespace json=boost::json;
inline int checks=0;
inline constexpr const char* amount_id="macro.offset.amount";
inline constexpr const char* copies_id="macro.copies";
inline constexpr const char* offset_enabled_id="macro.offset.enabled";
inline constexpr const char* repeater_enabled_id="macro.repeater.enabled";
inline constexpr double exact_amount=12.34567890123456;
inline void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
struct Snapshot {
    Document document,preview;std::uint64_t revision,generation;HistoryInfo history;std::string native;bool gesture;
    explicit Snapshot(const Session& session):document(session.document()),preview(session.preview_document()),
        revision(session.revision()),generation(session.gesture_generation()),history(session.history()),
        native(encode(session.document())),gesture(session.gesture_active()){}
    bool unchanged(const Session& session)const{return document==session.document()&&preview==session.preview_document()&&
        revision==session.revision()&&generation==session.gesture_generation()&&history==session.history()&&
        native==encode(session.document())&&gesture==session.gesture_active();}
};
inline const MacroInstance& instance(const Document& document,const Id& object="path") {
    const auto& stack=document.objects.at(object).stack;
    const auto found=std::find_if(stack.begin(),stack.end(),[](const auto& entry){return entry.macro.has_value();});
    if(found==stack.end())throw std::runtime_error("Missing fixture Macro instance");
    return *found->macro;
}
inline Id instance_id(const Id& object){return object=="path"?"instance":object=="other"?"independent-instance":"legacy-instance";}
inline Ref target(const char* parameter,const Id& object="path"){return macro_parameter_ref(object,instance_id(object),parameter);}
inline Document fixture(){
    auto document=empty_document("boolean-interface-doc","composition","artboard");
    for(const auto* id:{"path","other","legacy"}) {
        Object path;path.id=id;path.name="Boolean interface target";
        Contour contour;contour.id=path.id+"-contour";contour.closed=true;
        const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
        for(std::size_t i=0;i<anchors.size();++i){Point point;point.id=path.id+"-point-"+std::to_string(i+1);
            point.x.literal=anchors[i].x;point.y.literal=anchors[i].y;contour.points.push_back(point);}
        path.contours.push_back(contour);path.stack.emplace_back(default_operation(path.id+"-fill","nect.paint.fill"));
        document.objects.emplace(path.id,path);document.compositions.front().roots.push_back(path.id);
    }
    MacroDefinitionRevision initial;initial.input={"input","local_paths_and_paint"};initial.output={"output","local_paths_and_paint"};
    MacroEndpoint cursor{"",initial.input.id};
    for(const auto* id:{"offset-target","repeater-target"}) {
        const bool offset=std::string(id).starts_with("offset");
        auto operation=default_operation(id,offset?"nect.shape.offset":"nect.shape.repeater");
        if(offset)operation.parameters.at("amount").literal=5;
        else {operation.parameters.at("copies").literal=2;operation.parameters.at("position_x").literal=125;
            operation.parameters.at("position_y").literal=11;operation.parameters.at("rotation").literal=9;}
        initial.nodes.push_back({operation,std::string(id)+"-input",std::string(id)+"-output"});
        initial.edges.push_back({cursor,{id,std::string(id)+"-input"}});cursor={id,std::string(id)+"-output"};
    }
    initial.output_mapping=cursor;initial.edges.push_back({cursor,{"",initial.output.id}});
    initial.public_parameters={{amount_id,"Amount","offset-target","amount","number","du","local_paths_and_paint"}};
    std::reverse(initial.nodes.begin(),initial.nodes.end());std::reverse(initial.edges.begin(),initial.edges.end());
    auto published=initial;published.revision=2;published.interface_version=3;
    for(auto& node:published.nodes)if(node.operation.id=="repeater-target")node.operation.enabled=false;
    published.public_parameters.push_back({copies_id,"Copies","repeater-target","copies","number","scalar","local_paths_and_paint"});
    published.public_parameters.push_back({offset_enabled_id,"Use Offset","offset-target","enabled","boolean","boolean","local_paths_and_paint"});
    published.public_parameters.push_back({repeater_enabled_id,"Use Repeater","repeater-target","enabled","boolean","boolean","local_paths_and_paint"});
    MacroDefinition definition;definition.id="boolean-definition";definition.label="Enabled controls";definition.latest_revision=2;
    definition.revisions={{1,initial},{2,published}};
    Session session(document);session.apply({MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"path",definition.id,"instance",2,1,std::nullopt,"",0,{},{}}},
        MacroCommand{InstantiateMacro{"other",definition.id,"independent-instance",2,1,std::nullopt,"",0,{},{}}},
        MacroCommand{InstantiateMacro{"legacy",definition.id,"legacy-instance",1,1,std::nullopt,"",0,{},{}}},
        MacroCommand{SetMacroOverride{"path","instance",amount_id,exact_amount}},
        MacroCommand{SetMacroOverride{"path","instance",copies_id,3}}},0);
    return session.document();
}
inline json::object ref_json(const Ref& ref){return {{"object",ref.object},{"point",ref.point},{"field",ref.field}};}
using Endpoint=std::function<std::string(const json::object&)>;
inline Endpoint core_endpoint(Session& session){return [&session](const json::object& input){return request(session,json::serialize(input));};}
inline json::object response(const Endpoint& endpoint,const json::object& input){return json::parse(endpoint(input)).as_object();}
inline json::value success(const Endpoint& endpoint,const json::object& input){const auto result=response(endpoint,input);
    require(result.at("ok").as_bool(),"Canonical Macro API request succeeds");return result.at("result");}
inline json::object set_request(const Session& session,const char* parameter,json::value value){return {{"op","apply"},
    {"expected_revision",session.revision()},{"commands",json::array{json::object{{"type","set_macro_override"},
        {"object","path"},{"instance","instance"},{"public_parameter",parameter},{"value",std::move(value)}}}}};}
inline void api_readback(Session& session,const Endpoint& endpoint,const Id& object="path"){
    const Snapshot before(session);const auto list=success(endpoint,{{"op","properties"}}).as_array();
    for(const auto* parameter:{offset_enabled_id,repeater_enabled_id}) {
        const auto ref=target(parameter,object);const auto effective=macro_parameter_boolean_value(session.document(),ref.object,ref.point,ref.field);
        const auto got=success(endpoint,{{"op","get"},{"ref",ref_json(ref)}}).as_object();
        require(got.at("ref")==ref_json(ref)&&got.at("origin")=="macro_public_parameter"&&
            got.at("authored").is_bool()&&got.at("evaluated").is_bool()&&got.at("authored").as_bool()==effective&&
            got.at("evaluated").as_bool()==effective,"Macro get returns exact JSON booleans and the stable instance/PublicParamID Ref");
        int matches=0;for(const auto& item:list){const auto& property=item.as_object();if(property.at("ref")!=ref_json(ref))continue;++matches;
            require(property.at("type")=="boolean"&&property.at("unit")=="boolean"&&property.at("space")=="local"&&
                property.at("authored").is_bool()&&property.at("authored").as_bool()==effective&&
                property.at("evaluated").is_bool()&&property.at("evaluated").as_bool()==effective&&
                property.at("origin")== (instance(session.document(),object).boolean_overrides.contains(parameter)?"macro_override":"macro_default"),
                "Properties enumerates boolean values with override/default presence independent of true/false");}
        require(matches==1,"Properties enumerates each published boolean exactly once");
        bool refused=false;try{(void)macro_parameter_value(session.document(),ref.object,ref.point,ref.field);}
        catch(const Error& error){refused=error.code=="INVALID_MACRO_OVERRIDE";}
        require(refused,"Existing numeric C++ getter refuses boolean publication rather than coercing it");
    }
    for(const auto* parameter:{amount_id,copies_id}) {
        const auto ref=target(parameter,object);const auto value=macro_parameter_value(session.document(),ref.object,ref.point,ref.field);
        const auto got=success(endpoint,{{"op","get"},{"ref",ref_json(ref)}}).as_object();
        require(got.at("authored").is_number()&&got.at("evaluated").is_number()&&json::value_to<double>(got.at("authored"))==value&&
            json::value_to<double>(got.at("evaluated"))==value,"Existing numeric Macro getter/API retains the exact double alongside booleans");
        int matches=0;for(const auto& item:list){const auto& property=item.as_object();if(property.at("ref")!=ref_json(ref))continue;++matches;
            require(property.at("type")=="number"&&property.at("authored").is_number()&&property.at("evaluated").is_number()&&
                json::value_to<double>(property.at("evaluated"))==value,"Properties keeps numeric publication typed and exact");}
        require(matches==1,"Properties enumerates each published numeric control exactly once");
    }
    require(before.unchanged(session),"get/properties and numeric getter checks are authored/preview/history/byte-exact read-only operations");
}
inline void api_mutations(Session& session,const Endpoint& endpoint){
    api_readback(session,endpoint);const auto initial=session.document();const Snapshot before(session);
    (void)success(endpoint,set_request(session,offset_enabled_id,false));
    Session expected(before.document);expected.apply({MacroCommand{SetMacroBooleanOverride{"path","instance",offset_enabled_id,false}}},0);
    require(session.document()==expected.document()&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,"Generic set_macro_override routes JSON false to one exact boolean command");
    api_readback(session,endpoint);
    for(const auto& invalid:std::array<json::object,3>{set_request(session,offset_enabled_id,0),set_request(session,offset_enabled_id,"false"),
        set_request(session,amount_id,true)}) {
        const Snapshot refused(session);const auto result=response(endpoint,invalid);
        require(!result.at("ok").as_bool()&&refused.unchanged(session),"API refuses boolean/number/string type substitution atomically");
    }
    const auto override_state=session.document();(void)success(endpoint,{{"op","undo"},{"expected_revision",session.revision()}});
    require(session.document()==initial,"One API Undo restores false override absence exactly");
    (void)success(endpoint,{{"op","redo"},{"expected_revision",session.revision()}});
    require(session.document()==override_state,"One API Redo restores explicit false without numerical coercion");
    (void)success(endpoint,set_request(session,repeater_enabled_id,true));api_readback(session,endpoint);
    require(instance(session.document(),"other")==instance(initial,"other")&&instance(session.document(),"legacy")==instance(initial,"legacy")&&
        session.document().macro_definitions==initial.macro_definitions,"API boolean edits preserve independent revision pins and every authored node default");
    const Snapshot numeric_before(session);(void)success(endpoint,set_request(session,amount_id,-exact_amount));
    Session numeric_expected(numeric_before.document);numeric_expected.apply({MacroCommand{SetMacroOverride{"path","instance",amount_id,-exact_amount}}},0);
    require(session.document()==numeric_expected.document()&&session.revision()==numeric_before.revision+1&&
        session.history().states.size()==numeric_before.history.states.size()+1&&
        instance(session.document()).boolean_overrides==instance(numeric_before.document).boolean_overrides,
        "Generic set_macro_override retains exact numeric behavior without changing either boolean pin");
    api_readback(session,endpoint);api_readback(session,endpoint,"other");
}
inline int run_api(){checks=0;Session session(fixture());api_mutations(session,core_endpoint(session));
    Session cold(decode(encode(session.document())));require(cold.document()==session.document(),"Native087 roundtrip retains both typed maps and independent pins");
    api_readback(cold,core_endpoint(cold));return checks;}
}

#ifndef NECT_MACRO_BOOLEAN_API_ONLY
#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

namespace macro_boolean_window_smoke {
using namespace nect::desktop;
inline void events(){
    QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    // Queued Macro callbacks rebuild the Inspector during the first event pass.
    // Settle layout/show events for the new controls before checking visibility.
    QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
}
inline QByteArray reference(const Ref& ref){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
    {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);}
template<class T>inline T* visible(Window& window,const QString& name){
    for(auto* widget:window.findChildren<T*>(name))if(widget->isVisible())return widget;
    return nullptr;
}
inline QCheckBox* toggle(Window& window,const char* parameter,const Id& object="path"){
    auto* control=visible<QCheckBox>(window,QString::fromStdString("macro-boolean-"+instance_id(object)+"-"+parameter));
    require(control!=nullptr,"Stable named production Macro boolean checkbox exists");return control;}
inline QPushButton* reset(Window& window,const char* parameter,const Id& object="path"){
    return visible<QPushButton>(window,QString::fromStdString("macro-reset-boolean-"+instance_id(object)+"-"+parameter));}
inline QLineEdit* numeric(Window& window,const char* parameter,const Id& object="path"){
    for(auto* control:window.findChildren<QLineEdit*>())if(control->isVisible()&&control->property("nect-reference").toByteArray()==reference(target(parameter,object)))return control;
    throw std::runtime_error("Production Macro numeric control missing");}
inline Endpoint host_endpoint(Window& window){return [&window](const json::object& input){
    const auto outer=json::serialize(json::object{{"op","core"},{"session_id",window.host.session_id.toStdString()},
        {"document_id",window.host.session.document().id},{"request",input}});
    return window.host.dispatch(QByteArray::fromStdString(outer)).toStdString();};}
inline void load(Window& window){window.host.session=Session(fixture());window.host.session_id+="-macro-boolean";window.host.edited();
    window.canvas->set_active_artboard("composition","artboard",false);window.canvas->set_selection("path");events();}
inline void reveal(Window& window,QWidget* control){auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    require(scroll!=nullptr,"Production Inspector scroll exists");scroll->ensureWidgetVisible(control);events();}
inline void click(Window& window,QAbstractButton* control){require(control!=nullptr,"Production click target exists");reveal(window,control);
    QTest::mouseClick(control,Qt::LeftButton,Qt::NoModifier,QPoint(8,control->height()/2));events();}
inline void space(Window& window,QCheckBox* control){reveal(window,control);control->setFocus(Qt::OtherFocusReason);events();
    QTest::keyClick(control,Qt::Key_Space);events();}
inline void history_action(Window& window,const char* label){for(auto* action:window.findChildren<QAction*>())
    if(action->text()==QString::fromLatin1(label)){require(action->isEnabled(),"Production history action is enabled");action->trigger();events();return;}
    throw std::runtime_error("Production Undo/Redo action missing");}
inline void presentation(Window& window,const Id& object="path"){
    const auto& document=window.host.session.document();const auto& retained=instance(document,object);
    for(const auto* parameter:{offset_enabled_id,repeater_enabled_id}) {
        auto* control=toggle(window,parameter,object);const auto ref=target(parameter,object);const auto descriptor=macro_semantic_descriptor(document,ref);
        require(descriptor.key==parameter&&descriptor.value_type=="boolean"&&descriptor.unit=="boolean"&&descriptor.widget_hint=="toggle"&&
            descriptor.boolean_default==std::optional<bool>{parameter==offset_enabled_id}&&!descriptor.minimum&&!descriptor.maximum&&!descriptor.step&&
            validate_semantic_descriptor(descriptor).widget==SemanticWidget::toggle,"Pinned publication projects a typed boolean descriptor with true/false node defaults");
        require(control->property("nect-reference").toByteArray()==reference(ref)&&control->property("nect-semantic-key")==QString::fromLatin1(parameter)&&
            control->property("nect-widget-kind")=="toggle"&&control->property("nect-widget-hint")=="toggle"&&control->property("nect-value-type")=="boolean"&&
            control->property("nect-unit")=="boolean"&&control->property("nect-domain")=="local_paths_and_paint"&&
            control->property("nect-control-status")=="SUPPORTED"&&!control->property("nect-control-fallback").toBool()&&
            control->property("nect-exact-value").toBool()&&!control->isTristate()&&control->isEnabled()&&
            control->isChecked()==macro_parameter_boolean_value(document,object,instance_id(object),parameter),
            "Production two-state Macro checkbox carries exact stable Ref/typed metadata and effective value");
        require((reset(window,parameter,object)!=nullptr)==retained.boolean_overrides.contains(parameter),
            "Reset presence follows override-map membership, including an explicit false equal to the pinned default");
    }
    for(const auto* parameter:{amount_id,copies_id}){auto* control=numeric(window,parameter,object);
        require(control->property("nect-value-type")=="number"&&control->text().toDouble()==macro_parameter_value(document,object,instance_id(object),parameter),
            "Existing production numeric fields coexist at exact values beside boolean controls");}
    require(window.canvas->selected_object==object&&window.canvas->evaluated_values()==evaluate(document),
        "Production Canvas selection and evaluated projection agree with authored Macro state");
    require(evaluate_shape(document,object,evaluate(document)).paths.size()==
        (macro_parameter_boolean_value(document,object,instance_id(object),repeater_enabled_id)?
            static_cast<std::size_t>(macro_parameter_value(document,object,instance_id(object),copies_id)):1u),
        "Published Repeater boolean independently enables the pinned exact Copies geometry or bypasses it");
}
inline void cold_readback(const Document& document){QTemporaryDir files;require(files.isValid(),"Owned cold native directory exists");
    Host writer(files.path()+"/writer");writer.session=Session(document);const auto path=files.filePath("macro-boolean.nect");writer.save(path);writer.flush();
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window cold(files.path()+"/cold",std::make_unique<FolderLibrary>(settings));cold.setAttribute(Qt::WA_DontShowOnScreen);cold.resize(1400,900);cold.show();cold.host.open(path);
    cold.canvas->set_active_artboard("composition","artboard",false);cold.canvas->set_selection("path");events();
    require(cold.host.session.document()==document&&encode(cold.host.session.document())==encode(document),
        "Cold production Window reopen preserves native087, typed maps, stable IDs, node defaults and revision pins byte-exactly");
    presentation(cold);api_readback(cold.host.session,host_endpoint(cold));cold.host.changed={};cold.hide();}
inline void literal(Window& window,const char* parameter,bool keyboard){load(window);auto& session=window.host.session;
    const Snapshot before(session);presentation(window);const auto initial=macro_parameter_boolean_value(before.document,"path","instance",parameter);
    auto* control=toggle(window,parameter);control->setChecked(!initial);events();
    require(before.unchanged(session),"Programmatic checkbox setChecked does not create a Macro override or history");
    window.refresh();events();control=toggle(window,parameter);require(control->isChecked()==initial,"Inspector refresh restores the canonical boolean");
    reveal(window,control);control->setFocus(Qt::OtherFocusReason);events();control->clearFocus();events();window.refresh();events();
    require(before.unchanged(session),"Focus/blur and repeated Inspector refresh are authored/preview/history/byte-exact no-ops");
    if(keyboard)space(window,toggle(window,parameter));else click(window,toggle(window,parameter));
    Session expected(before.document);expected.apply({MacroCommand{SetMacroBooleanOverride{"path","instance",parameter,!initial}}},0);
    require(session.document()==expected.document()&&session.revision()==before.revision+1&&
        session.history()==expected.history()&&!session.gesture_active(),"One real mouse/Space activation commits exactly one canonical boolean override and Undo boundary");
    const auto committed=session.document();presentation(window);api_readback(session,host_endpoint(window));
    history_action(window,"Undo");require(session.document()==before.document&&!session.can_undo(),"One production Undo restores exact override absence and independent pins");presentation(window);
    history_action(window,"Redo");require(session.document()==committed&&!session.can_redo(),"One production Redo restores the entire exact typed result");presentation(window);
    window.canvas->set_selection("other");events();presentation(window,"other");
    require(instance(session.document(),"other")==instance(before.document,"other"),"Other interface3 instance keeps an independent override map");
    window.canvas->set_selection("legacy");events();
    require(!visible<QCheckBox>(window,QString::fromStdString("macro-boolean-legacy-instance-"+std::string(parameter)))&&
        instance(session.document(),"legacy").pinned_revision==1&&numeric(window,amount_id,"legacy")->text().toDouble()==5,
        "Legacy pinned revision keeps its numeric control and gains no controls from the latest interface3 revision");
    cold_readback(committed);
}
inline void reset_contract(Window& window,const char* parameter,bool value){load(window);auto& session=window.host.session;
    (void)success(host_endpoint(window),set_request(session,parameter,value));events();presentation(window);const Snapshot before(session);
    const auto initial=macro_parameter_boolean_value(session.document(),"path","instance",parameter);auto* button=reset(window,parameter);
    require(button!=nullptr,"Typed override, including false, exposes a production Reset");click(window,button);
    Session expected(before.document);expected.apply({MacroCommand{ResetMacroOverride{"path","instance",parameter}}},0);
    require(session.document()==expected.document()&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1&&!instance(session.document()).boolean_overrides.contains(parameter),
        "Production Reset removes exactly the typed selected override in one transaction without changing numeric overrides");
    require(macro_parameter_boolean_value(session.document(),"path","instance",parameter)==(parameter==offset_enabled_id)&&!reset(window,parameter),
        "Reset restores the pinned true or false default and removes Reset presentation");
    const auto reset_state=session.document();presentation(window);api_readback(session,host_endpoint(window));
    history_action(window,"Undo");require(session.document()==before.document&&macro_parameter_boolean_value(session.document(),"path","instance",parameter)==initial&&reset(window,parameter),
        "One Reset Undo restores typed override presence even when its false value equals the default");
    history_action(window,"Redo");require(session.document()==reset_state&&!reset(window,parameter),"One Reset Redo restores exact override absence");
    cold_readback(before.document);cold_readback(reset_state);
}
inline void independent_instance(Window& window,bool keyboard){load(window);auto& session=window.host.session;
    window.canvas->set_selection("other");events();presentation(window,"other");const Snapshot before(session);
    auto* control=toggle(window,repeater_enabled_id,"other");if(keyboard)space(window,control);else click(window,control);
    Session expected(before.document);expected.apply({MacroCommand{SetMacroBooleanOverride{"other","independent-instance",repeater_enabled_id,true}}},0);
    require(session.document()==expected.document()&&session.revision()==before.revision+1&&session.history()==expected.history()&&
        instance(session.document(),"path")==instance(before.document,"path")&&instance(session.document(),"legacy")==instance(before.document,"legacy"),
        "A real second-instance mouse/Space activation targets its own stable Ref and leaves both other revision pins exact");
    presentation(window,"other");api_readback(session,host_endpoint(window),"other");const auto committed=session.document();
    history_action(window,"Undo");require(session.document()==before.document,"Second-instance edit has one exact production Undo");
    history_action(window,"Redo");require(session.document()==committed,"Second-instance edit has one exact production Redo");cold_readback(committed);
}
inline void refusal(Window& window,const char* code,bool resetting){load(window);auto& session=window.host.session;
    if(resetting){(void)success(host_endpoint(window),set_request(session,repeater_enabled_id,false));events();}
    auto* control=toggle(window,repeater_enabled_id);QAbstractButton* button=resetting?static_cast<QAbstractButton*>(reset(window,repeater_enabled_id)):control;
    require(button!=nullptr,"Retained refusal test control exists");reveal(window,button);const auto frozen_session=window.host.session_id;
    if(std::string(code)=="REVISION_CONFLICT")session.apply({Rename{"path","Changed without Inspector refresh"}},session.revision());
    else if(std::string(code)=="SESSION_CONFLICT")window.host.session_id+="-different-document";
    else {session.begin_gesture(session.revision());session.update_gesture({Rename{"path","Uncommitted preview"}});}
    const Snapshot before(session);QTest::mouseClick(button,Qt::LeftButton,Qt::NoModifier,QPoint(8,button->height()/2));events();
    require(before.unchanged(session)&&window.statusBar()->currentMessage().contains(QString::fromLatin1(code)),
        "Retained click/Reset refuses stale revision, changed Session identity or active gesture without authored/preview/history mutation");
    require(control->isChecked()==false,"Rejected activation restores its captured false presentation");
    if(session.gesture_active())session.cancel_gesture();
    window.host.session_id=frozen_session;window.host.edited();events();
}
inline int run(){checks=0;QTemporaryDir files;require(files.isValid(),"Owned Window test directory exists");
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1400,900);window.show();events();
    for(const auto* parameter:{offset_enabled_id,repeater_enabled_id})for(const bool keyboard:{false,true})literal(window,parameter,keyboard);
    for(const bool keyboard:{false,true})independent_instance(window,keyboard);
    for(const auto* parameter:{offset_enabled_id,repeater_enabled_id})for(const bool value:{false,true})reset_contract(window,parameter,value);
    for(const auto* code:{"REVISION_CONFLICT","SESSION_CONFLICT","GESTURE_ACTIVE"})for(const bool resetting:{false,true})refusal(window,code,resetting);
    load(window);api_mutations(window.host.session,host_endpoint(window));events();presentation(window);cold_readback(window.host.session.document());
    window.host.changed={};window.hide();return checks;}
}
#endif
