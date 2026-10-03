#include "nect/core.hpp"
#include "nect/io.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#include <process.h>
#endif

using namespace nect;
namespace {
int checks=0;
constexpr const char* amount_id="macro.offset.amount";
constexpr const char* copies_id="macro.copies";
constexpr const char* offset_enabled_id="macro.offset.enabled";
constexpr const char* repeater_enabled_id="macro.repeater.enabled";
using Numbers=std::map<std::string,double>;
using Booleans=std::map<std::string,bool>;
void check(bool condition,const std::string& reason) {
    if(!condition)throw std::runtime_error(reason);
    ++checks;
}
template<class F>void rejects(const char* code,F action) {
    try {action();}
    catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        return;
    }
    throw std::runtime_error("Expected refusal: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands) {session.apply(commands,session.revision());}
template<class F>void unchanged(Session& session,const char* code,F action) {
    const auto document=session.document();const auto revision=session.revision();const auto history=session.history();
    const auto bytes=encode(document);rejects(code,action);
    check(session.document()==document&&session.revision()==revision&&session.history()==history&&encode(session.document())==bytes,
        "Refusal preserves authored state, native bytes, Session revision and complete Undo history");
}
void atomic(Session& session,const char* code,std::vector<Command> commands) {
    unchanged(session,code,[&]{apply(session,std::move(commands));});
}
Document fixture(bool empty_other=false) {
    auto document=empty_document("boolean-interface-doc","composition","artboard");
    for(const auto* id:{"path","other"}) {
        Object path;path.id=id;path.name="Boolean interface target";
        Contour contour;contour.id=path.id+"-contour";contour.closed=true;
        const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
        for(std::size_t i=0;i<anchors.size();++i) {
            Point point;point.id=path.id+"-point-"+std::to_string(i+1);
            point.x.literal=anchors[i].x;point.y.literal=anchors[i].y;contour.points.push_back(point);
        }
        path.contours.push_back(contour);
        if(!(empty_other&&path.id=="other"))path.stack.emplace_back(default_operation(path.id+"-fill","nect.paint.fill"));
        document.objects.emplace(path.id,path);document.compositions.front().roots.push_back(path.id);
    }
    return document;
}
MacroNode& node(MacroDefinitionRevision& revision,const Id& id) {
    const auto found=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& item){return item.operation.id==id;});
    if(found==revision.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
const MacroNode& node(const MacroDefinitionRevision& revision,const Id& id) {
    const auto found=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& item){return item.operation.id==id;});
    if(found==revision.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
MacroDefinition definition(bool chain=false) {
    MacroDefinitionRevision initial;initial.graph_version=chain?2:1;
    initial.input={"input","local_paths_and_paint"};initial.output={"output","local_paths_and_paint"};
    const std::vector<Id> ids=chain?std::vector<Id>{"offset-first","offset-target","repeater-target","repeater-last"}:
        std::vector<Id>{"offset-target","repeater-target"};
    MacroEndpoint cursor{"",initial.input.id};
    for(const auto& id:ids) {
        const bool offset=id.starts_with("offset");
        auto operation=default_operation(id,offset?"nect.shape.offset":"nect.shape.repeater");
        if(offset)operation.parameters.at("amount").literal=id=="offset-first"?2:5;
        else {
            operation.parameters.at("copies").literal=2;
            operation.parameters.at("position_x").literal=id=="repeater-target"?125:21;
            operation.parameters.at("position_y").literal=id=="repeater-target"?11:80;
            operation.parameters.at("rotation").literal=id=="repeater-target"?9:-13;
        }
        initial.nodes.push_back({operation,id+"-input",id+"-output"});
        initial.edges.push_back({cursor,{id,id+"-input"}});cursor={id,id+"-output"};
    }
    initial.output_mapping=cursor;initial.edges.push_back({cursor,{"",initial.output.id}});
    initial.public_parameters={{amount_id,"Amount","offset-target","amount","number","du","local_paths_and_paint"}};
    // The oracle below names the chain explicitly; neither side can rely on storage order.
    std::reverse(initial.nodes.begin(),initial.nodes.end());std::reverse(initial.edges.begin(),initial.edges.end());
    auto published=initial;published.revision=2;published.interface_version=3;
    node(published,"repeater-target").operation.enabled=false;
    published.public_parameters.push_back({copies_id,"Copies","repeater-target","copies","number","scalar","local_paths_and_paint"});
    published.public_parameters.push_back({offset_enabled_id,"Use Offset","offset-target","enabled","boolean","boolean","local_paths_and_paint"});
    published.public_parameters.push_back({repeater_enabled_id,"Use Repeater","repeater-target","enabled","boolean","boolean","local_paths_and_paint"});
    MacroDefinition result;result.id="boolean-definition";result.label="Enabled controls";result.latest_revision=2;
    result.revisions={{1,std::move(initial)},{2,std::move(published)}};return result;
}
void instantiate(Session& session,const MacroDefinition& source=definition(),std::uint64_t pin=2,bool independent=false) {
    std::vector<Command> commands{MacroCommand{CreateMacroDefinition{source}},
        MacroCommand{InstantiateMacro{"path",source.id,"instance",pin,1}}};
    if(independent)commands.emplace_back(MacroCommand{InstantiateMacro{"other",source.id,"independent-instance",pin,1}});
    apply(session,std::move(commands));
}
const MacroInstance& instance(const Session& session,const Id& object="path",const Id& id="instance") {
    const auto& stack=session.document().objects.at(object).stack;
    const auto found=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==id;});
    if(found==stack.end()||!found->macro)throw std::runtime_error("Missing instance: "+id);
    return *found->macro;
}
EvaluatedShape evaluated(const Document& document,const Id& object="path") {return evaluate_shape(document,object,evaluate(document));}
Document ordinary(const MacroDefinitionRevision& revision,const Numbers& numbers={},const Booleans& booleans={},bool outer_enabled=true,
    const Id& offset_node="offset-target",const Id& repeater_node="repeater-target") {
    Session session(fixture());std::vector<Command> commands;std::size_t index=1;
    const std::vector<Id> ids=revision.graph_version==2?std::vector<Id>{"offset-first","offset-target","repeater-target","repeater-last"}:
        std::vector<Id>{"offset-target","repeater-target"};
    for(const auto& id:ids) {
        auto operation=node(revision,id).operation;
        if(id=="offset-target"&&numbers.contains(amount_id))operation.parameters.at("amount").literal=numbers.at(amount_id);
        if(id=="repeater-target"&&numbers.contains(copies_id))operation.parameters.at("copies").literal=numbers.at(copies_id);
        if(id==offset_node&&booleans.contains(offset_enabled_id))operation.enabled=booleans.at(offset_enabled_id);
        if(id==repeater_node&&booleans.contains(repeater_enabled_id))operation.enabled=booleans.at(repeater_enabled_id);
        operation.enabled=operation.enabled&&outer_enabled;
        commands.emplace_back(AddOperation{"path",operation,index++});
    }
    apply(session,std::move(commands));return session.document();
}
bool near(Vec2 a,Vec2 b) {return std::abs(a.x-b.x)<1e-7&&std::abs(a.y-b.y)<1e-7;}
bool same_paths(const std::vector<PathInstance>& a,const std::vector<PathInstance>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i) {
        if(a[i].contours->size()!=b[i].contours->size())return false;
        for(std::size_t c=0;c<a[i].contours->size();++c) {
            const auto& x=(*a[i].contours)[c];const auto& y=(*b[i].contours)[c];
            if(x.closed!=y.closed||x.points.size()!=y.points.size())return false;
            for(std::size_t p=0;p<x.points.size();++p) {
                const auto& xp=x.points[p];const auto& yp=y.points[p];
                if(!near(map_point(a[i].transform,xp.anchor),map_point(b[i].transform,yp.anchor))||
                    !near(map_point(a[i].transform,xp.incoming),map_point(b[i].transform,yp.incoming))||
                    !near(map_point(a[i].transform,xp.outgoing),map_point(b[i].transform,yp.outgoing)))return false;
            }
        }
    }
    return true;
}
void same_shape(const EvaluatedShape& actual,const EvaluatedShape& expected,const std::string& reason) {
    check(same_paths(actual.paths,expected.paths),reason+": complete cubic geometry");
    check(actual.paints.size()==expected.paints.size(),reason+": paint count");
    for(std::size_t i=0;i<actual.paints.size();++i) {
        const auto& a=actual.paints[i];const auto& b=expected.paints[i];
        check(a.type==b.type&&a.rgba==b.rgba&&a.fill_rule==b.fill_rule&&a.transform==b.transform&&same_paths(a.paths,b.paths),
            reason+": painted cubic geometry");
    }
}
boost::json::object parsed(const std::string& text) {return boost::json::parse(text).as_object();}
boost::json::object& serialized_revision(boost::json::object& document,std::size_t index) {
    return document.at("macros").as_array().front().as_object().at("revisions").as_array().at(index).as_object();
}
boost::json::object& serialized_instance(boost::json::object& document,const Id& object="path") {
    for(auto& value:document.at("objects").as_array()) {
        auto& item=value.as_object();if(item.at("id").as_string()==object)
            return item.at("stack").as_array().at(1).as_object();
    }
    throw std::runtime_error("Missing serialized instance");
}
void independent_defaults_overrides_resets() {
    for(const bool chain:{false,true}) {
        Session session(fixture());const auto source=definition(chain);instantiate(session,source,2,true);
        const auto independent=instance(session,"other","independent-instance");
        const auto independent_shape=evaluated(session.document(),"other");
        check(macro_parameter_boolean_value(session.document(),"path","instance",offset_enabled_id)&&
            !macro_parameter_boolean_value(session.document(),"path","instance",repeater_enabled_id)&&
            instance(session).boolean_overrides.empty(),"Pinned boolean defaults come directly from each internal node's enabled literal");
        same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(2))),"Mixed true/false boolean defaults match ordinary operations");
        const Numbers numbers{{amount_id,12},{copies_id,3}};
        apply(session,{MacroCommand{SetMacroOverride{"path","instance",amount_id,12}},MacroCommand{SetMacroOverride{"path","instance",copies_id,3}}});
        Booleans active;
        for(const auto& [id,value]:std::array<std::pair<const char*,bool>,4>{{{offset_enabled_id,false},{repeater_enabled_id,true},
            {offset_enabled_id,true},{repeater_enabled_id,false}}}) {
            const auto before=encode(session.document());const auto before_revision=session.revision();const auto states=session.history().states.size();
            apply(session,{MacroCommand{SetMacroBooleanOverride{"path","instance",id,value}}});active[id]=value;
            check(instance(session).boolean_overrides==active&&instance(session).overrides==numbers&&
                macro_parameter_boolean_value(session.document(),"path","instance",id)==value,
                "Boolean command sets an exact typed stable-ID override alongside unchanged numeric overrides");
            check(session.revision()==before_revision+1&&session.history().states.size()==states+1,"Each boolean change is one Session history entry");
            same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(2),numbers,active)),"Only the mapped internal node is enabled or bypassed");
            check(instance(session,"other","independent-instance")==independent,"Other instance retains its independent defaults and empty override maps");
            same_shape(evaluated(session.document(),"other"),independent_shape,"Other instance geometry and paint remain independent");
            const auto after=encode(session.document());session.undo(session.revision());check(encode(session.document())==before,"Boolean override has exact one-step Undo");
            session.redo(session.revision());check(encode(session.document())==after,"Boolean override has exact Redo");
        }
        check(session.document().macro_definitions.at(source.id)==source,"Local booleans never mutate definition revisions or node defaults");
        for(const auto* id:{offset_enabled_id,repeater_enabled_id}) {
            const auto before=encode(session.document());apply(session,{MacroCommand{ResetMacroOverride{"path","instance",id}}});active.erase(id);
            check(instance(session).boolean_overrides==active&&instance(session).overrides==numbers,"Shared Reset removes only its selected typed boolean override");
            same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(2),numbers,active)),"Reset restores the pinned node's enabled default");
            session.undo(session.revision());check(encode(session.document())==before,"Boolean reset has exact Undo");session.redo(session.revision());
        }
        apply(session,{MacroCommand{ResetMacroOverride{"path","instance",amount_id}}});
        check(instance(session).overrides==Numbers{{copies_id,3}}&&instance(session).boolean_overrides.empty(),"Numeric Reset still targets the numeric map");
        atomic(session,"NO_MACRO_OVERRIDE",{MacroCommand{ResetMacroOverride{"path","instance",offset_enabled_id}}});
    }
}
void typed_validation_and_interface_boundaries() {
    Session session(fixture());instantiate(session);
    atomic(session,"INVALID_MACRO_OVERRIDE",{MacroCommand{SetMacroOverride{"path","instance",amount_id,17}},
        MacroCommand{SetMacroOverride{"path","instance",offset_enabled_id,0}}});
    atomic(session,"INVALID_MACRO_OVERRIDE",{MacroCommand{SetMacroBooleanOverride{"path","instance",offset_enabled_id,false}},
        MacroCommand{SetMacroBooleanOverride{"path","instance",copies_id,true}}});
    rejects("INVALID_MACRO_OVERRIDE",[&]{(void)macro_parameter_value(session.document(),"path","instance",offset_enabled_id);});
    rejects("INVALID_MACRO_OVERRIDE",[&]{(void)macro_parameter_boolean_value(session.document(),"path","instance",amount_id);});
    atomic(session,"MISSING_MACRO_PARAMETER",{MacroCommand{SetMacroBooleanOverride{"path","instance","macro.missing",true}}});
    atomic(session,"MISSING_MACRO_INSTANCE",{MacroCommand{SetMacroBooleanOverride{"path","missing",offset_enabled_id,true}}});
    atomic(session,"MISSING_OBJECT",{MacroCommand{SetMacroBooleanOverride{"missing","instance",offset_enabled_id,true}}});
    rejects("MISSING_MACRO_PARAMETER",[&]{(void)macro_parameter_boolean_value(session.document(),"path","instance","macro.missing");});
    auto refuse=[](MacroDefinition source,const char* code) {
        Session empty(fixture());atomic(empty,code,{MacroCommand{CreateMacroDefinition{std::move(source)}}});
    };
    auto invalid=definition();invalid.revisions.at(1).interface_version=3;refuse(invalid,"INVALID_MACRO_INTERFACE");
    invalid=definition();invalid.revisions.at(2).interface_version=2;
    node(invalid.revisions.at(2),"repeater-target").operation.enabled=true;refuse(invalid,"INVALID_MACRO_MAPPING");
    invalid=definition();invalid.revisions.at(2).interface_version=4;
    node(invalid.revisions.at(2),"repeater-target").operation.enabled=true;refuse(invalid,"UNSUPPORTED_MACRO_INTERFACE_VERSION");
    invalid=definition();invalid.revisions.at(2).graph_version=3;refuse(invalid,"UNSUPPORTED_MACRO_GRAPH_VERSION");
    for(const auto* field:{"unit","domain","value_type","parameter","node"}) {
        invalid=definition();auto& parameter=invalid.revisions.at(2).public_parameters.at(2);
        if(std::string(field)=="unit")parameter.unit="scalar";
        else if(std::string(field)=="domain")parameter.domain="local_paths";
        else if(std::string(field)=="value_type")parameter.value_type="number";
        else if(std::string(field)=="parameter")parameter.parameter="amount";
        else parameter.node="missing-node";
        refuse(invalid,"INVALID_MACRO_MAPPING");
    }
    invalid=definition();invalid.revisions.at(2).public_parameters.push_back(
        {"macro.alias","Alias","offset-target","enabled","boolean","boolean","local_paths_and_paint"});
    refuse(invalid,"INVALID_MACRO_MAPPING");
    invalid=definition();invalid.revisions.at(2).public_parameters.at(3).id=offset_enabled_id;
    refuse(invalid,"DUPLICATE_MACRO_PARAMETER");
    auto malformed=session.document();auto& retained=*malformed.objects.at("path").stack.at(1).macro;
    retained.overrides[offset_enabled_id]=0;retained.boolean_overrides[offset_enabled_id]=false;
    rejects("INVALID_MACRO_OVERRIDE",[&]{validate(malformed);});
    malformed=session.document();malformed.objects.at("path").stack.at(1).macro->boolean_overrides[copies_id]=true;
    rejects("INVALID_MACRO_OVERRIDE",[&]{validate(malformed);});
    malformed=session.document();malformed.objects.at("path").stack.at(1).macro->boolean_overrides["macro.missing"]=false;
    rejects("ORPHAN_MACRO_OVERRIDE",[&]{validate(malformed);});
    auto unpublished=definition();unpublished.revisions.at(2).public_parameters.resize(2);Session no_public_boolean(fixture());
    instantiate(no_public_boolean,unpublished);
    same_shape(evaluated(no_public_boolean.document()),evaluated(ordinary(unpublished.revisions.at(2))),
        "Unpublished interface3 node enabled literals still execute without a boolean override");
    auto legacy=definition();legacy.revisions.erase(2);legacy.latest_revision=1;Session old(fixture());instantiate(old,legacy,1);
    atomic(old,"MISSING_MACRO_PARAMETER",{MacroCommand{SetMacroBooleanOverride{"path","instance",offset_enabled_id,false}}});
    auto numeric=definition();auto& numeric_revision=numeric.revisions.at(2);numeric_revision.interface_version=2;
    numeric_revision.public_parameters.resize(2);node(numeric_revision,"repeater-target").operation.enabled=true;
    Session second(fixture());instantiate(second,numeric);
    atomic(second,"MISSING_MACRO_PARAMETER",{MacroCommand{SetMacroBooleanOverride{"path","instance",repeater_enabled_id,true}}});
    same_shape(evaluated(old.document()),evaluated(ordinary(legacy.revisions.at(1))),"Legacy graph and enabled-literal behavior stay unchanged");
}
void pins_migration_and_detach() {
    Session session(fixture());const auto source=definition();instantiate(session,source,1);
    auto next=source.revisions.at(2);next.revision=3;node(next,"offset-target").operation.enabled=false;
    node(next,"repeater-target").operation.enabled=true;
    apply(session,{MacroCommand{UpdateMacroDefinition{source.id,next}}});
    check(instance(session).pinned_revision==1,"Publishing enabled controls and new defaults does not migrate an existing legacy pin");
    const auto before=encode(session.document());apply(session,{MacroCommand{UpdateMacroInstance{"path","instance",3}}});
    check(!macro_parameter_boolean_value(session.document(),"path","instance",offset_enabled_id)&&
        macro_parameter_boolean_value(session.document(),"path","instance",repeater_enabled_id),"Explicit migration activates the target node enabled defaults");
    session.undo(session.revision());check(encode(session.document())==before,"Migration from a legacy pin has exact Undo");session.redo(session.revision());
    const Numbers numbers{{amount_id,12},{copies_id,3}};const Booleans booleans{{offset_enabled_id,true},{repeater_enabled_id,false}};
    apply(session,{MacroCommand{SetMacroOverride{"path","instance",amount_id,12}},MacroCommand{SetMacroOverride{"path","instance",copies_id,3}},
        MacroCommand{SetMacroBooleanOverride{"path","instance",offset_enabled_id,true}},MacroCommand{SetMacroBooleanOverride{"path","instance",repeater_enabled_id,false}}});
    auto remapped=next;remapped.revision=4;remapped.public_parameters.at(2).node="repeater-target";
    remapped.public_parameters.at(3).node="offset-target";
    apply(session,{MacroCommand{UpdateMacroDefinition{source.id,remapped}},MacroCommand{UpdateMacroInstance{"path","instance",4}}});
    check(instance(session).boolean_overrides==booleans&&instance(session).overrides==numbers,"Compatible enabled node remapping retains both typed override maps");
    same_shape(evaluated(session.document()),evaluated(ordinary(remapped,numbers,booleans,true,"repeater-target","offset-target")),
        "Stable boolean IDs follow their new explicit internal enabled mappings");
    auto changed_type=remapped;changed_type.revision=5;
    changed_type.public_parameters.at(2)={offset_enabled_id,"Rotation now","repeater-target","rotation","number","degree","local_paths_and_paint"};
    apply(session,{MacroCommand{UpdateMacroDefinition{source.id,changed_type}}});
    atomic(session,"INCOMPATIBLE_MACRO_MIGRATION",{MacroCommand{SetMacroOverride{"path","instance",amount_id,19}},
        MacroCommand{UpdateMacroInstance{"path","instance",5}}});
    auto deleted=remapped;deleted.revision=6;deleted.public_parameters.erase(deleted.public_parameters.begin()+2);
    apply(session,{MacroCommand{UpdateMacroDefinition{source.id,deleted}}});
    atomic(session,"ORPHAN_MACRO_OVERRIDE",{MacroCommand{SetMacroBooleanOverride{"path","instance",repeater_enabled_id,true}},
        MacroCommand{UpdateMacroInstance{"path","instance",6}}});
    const auto before_reset=encode(session.document());apply(session,{MacroCommand{ResetMacroOverride{"path","instance",offset_enabled_id}},
        MacroCommand{UpdateMacroInstance{"path","instance",6}}});
    check(instance(session).pinned_revision==6&&!instance(session).boolean_overrides.contains(offset_enabled_id),"Explicit typed reset permits migration beyond a removed enabled control");
    session.undo(session.revision());check(encode(session.document())==before_reset,"Reset and migration remain one atomic Undo");
    for(const bool outer_enabled:{true,false}) {
        apply(session,{EnableOperation{"path","instance",outer_enabled}});
        const auto macro_shape=evaluated(session.document());const auto before_detach=encode(session.document());
        same_shape(macro_shape,evaluated(ordinary(remapped,numbers,booleans,outer_enabled,"repeater-target","offset-target")),
            "Outer disabled instance overrides every internal enabled publication");
        apply(session,{MacroCommand{DetachMacroInstance{"path","instance","materialized"}}});
        const auto& stack=session.document().objects.at("path").stack;
        check(stack.size()==3&&!stack.at(1).macro&&!stack.at(2).macro&&
            stack.at(1).enabled==false&&stack.at(2).enabled==outer_enabled&&
            stack.at(1).parameters.at("amount").literal==12&&stack.at(2).parameters.at("copies").literal==3,
            "Detach resolves internal booleans first, then applies outer enabled while retaining numeric values");
        same_shape(evaluated(session.document()),macro_shape,"Detached enabled literals preserve complete geometry and paint");
        session.undo(session.revision());check(encode(session.document())==before_detach,"Typed enabled detach has exact one-step Undo");
    }
    Session numeric(fixture());instantiate(numeric);
    apply(numeric,{MacroCommand{SetMacroOverride{"path","instance",copies_id,3}}});
    auto numeric_to_bool=source.revisions.at(2);numeric_to_bool.revision=3;
    numeric_to_bool.public_parameters.at(1)={copies_id,"Enabled now","offset-target","enabled","boolean","boolean","local_paths_and_paint"};
    numeric_to_bool.public_parameters.erase(numeric_to_bool.public_parameters.begin()+2);
    apply(numeric,{MacroCommand{UpdateMacroDefinition{source.id,numeric_to_bool}}});
    atomic(numeric,"INCOMPATIBLE_MACRO_MIGRATION",{MacroCommand{UpdateMacroInstance{"path","instance",3}}});
}
void presets_preserve_boolean_maps() {
    Session session(fixture(true));instantiate(session);
    apply(session,{MacroCommand{SetMacroOverride{"path","instance",amount_id,12}},MacroCommand{SetMacroOverride{"path","instance",copies_id,3}},
        MacroCommand{SetMacroBooleanOverride{"path","instance",offset_enabled_id,false}},MacroCommand{SetMacroBooleanOverride{"path","instance",repeater_enabled_id,true}},
        EnableOperation{"path","instance",false}});
    PresetDefinition metadata;metadata.id="captured-boolean-preset";metadata.label="Typed Macro controls";metadata.schema_version=2;
    const auto captured=capture_preset_definition(session.document(),metadata,"path");
    check(captured.entries.size()==2&&captured.entries.at(1).kind=="macro"&&captured.entries.at(1).pinned_revision==2&&
        captured.entries.at(1).overrides==instance(session).overrides&&captured.entries.at(1).boolean_overrides==instance(session).boolean_overrides&&
        !captured.entries.at(1).enabled,"Pure Preset capture retains the pin, both exact typed maps and outer enabled literal");
    const auto before_capture=encode(session.document());
    session.apply_preset_command(PresetCommand{CreatePresetFromStack{metadata,"path"}},session.revision());
    check(session.document().preset_definitions.at(metadata.id)==captured,"Command capture uses the same typed Preset snapshot");
    session.undo(session.revision());check(encode(session.document())==before_capture,"Typed Preset capture has exact Undo");session.redo(session.revision());
    const auto before_apply=encode(session.document());
    session.apply_preset_command(PresetCommand{ApplyPreset{metadata.id,"other","applied"}},session.revision());
    const auto& applied=session.document().objects.at("other").stack;
    check(applied.size()==2&&applied.at(1).macro&&applied.at(1).macro->boolean_overrides==instance(session).boolean_overrides&&
        applied.at(1).macro->overrides==instance(session).overrides&&!applied.at(1).enabled,
        "Preset application preserves exact bool values, numeric values and the disabled outer instance");
    same_shape(evaluated(session.document(),"other"),evaluated(session.document()),"Applied Preset preserves the captured outer-disabled shape and paint");
    const auto after_apply=encode(session.document());session.undo(session.revision());check(encode(session.document())==before_apply,"Typed Preset application has exact Undo");
    session.redo(session.revision());check(encode(session.document())==after_apply,"Typed Preset application has exact Redo");
    check(decode(after_apply)==session.document(),"Native roundtrip preserves boolean Preset and applied instance maps");
    const auto native=parsed(after_apply);const auto& wire=native.at("presets").as_array().front().as_object().at("entries").as_array().at(1).as_object();
    const auto& wire_overrides=wire.at("overrides").as_object();
    check(wire_overrides.at(offset_enabled_id).is_bool()&&!wire_overrides.at(offset_enabled_id).as_bool()&&
        wire_overrides.at(repeater_enabled_id).is_bool()&&wire_overrides.at(repeater_enabled_id).as_bool()&&
        wire_overrides.at(amount_id).is_number()&&!wire.contains("boolean_overrides"),
        "Preset JSON uses the same single typed overrides object with exact false/true values");
    auto malformed=captured;malformed.entries.at(1).boolean_overrides.erase(offset_enabled_id);malformed.entries.at(1).overrides[offset_enabled_id]=0;
    unchanged(session,"INCOMPATIBLE_MACRO_PUBLIC_PARAMETER",[&]{session.apply_preset_command(PresetCommand{UpdatePreset{malformed}},session.revision());});
    malformed=captured;malformed.entries.at(1).boolean_overrides[copies_id]=true;
    unchanged(session,"INVALID_MACRO_OVERRIDE",[&]{session.apply_preset_command(PresetCommand{UpdatePreset{malformed}},session.revision());});
    auto changed=captured;changed.entries.at(1).boolean_overrides[offset_enabled_id]=true;
    session.apply_preset_command(PresetCommand{UpdatePreset{changed}},session.revision());
    check(session.document().objects.at("other").stack.at(1).macro->boolean_overrides.at(offset_enabled_id)==false,
        "Editing a captured boolean Preset does not retroactively alter existing applications");
}
std::string quoted(const std::string& value) {
#ifdef _WIN32
    return "\""+value+"\"";
#else
    std::string result="'";for(const auto ch:value)result+=ch=='\''?"'\\''":std::string(1,ch);return result+"'";
#endif
}
void cold_read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);check(file.good(),"Cold process opens saved native087");
    const std::string bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};Session reopened(decode(bytes));
    const auto& revision=reopened.document().macro_definitions.at("boolean-definition").revisions.at(2);
    const Numbers numbers{{amount_id,12},{copies_id,3}};const Booleans booleans{{offset_enabled_id,false},{repeater_enabled_id,true}};
    check(revision.interface_version==3&&instance(reopened).pinned_revision==2&&instance(reopened).overrides==numbers&&
        instance(reopened).boolean_overrides==booleans,"Cold reopen retains explicit interface3, pin and separate bool/numeric maps");
    same_shape(evaluated(reopened.document()),evaluated(ordinary(revision,numbers,booleans)),"Fresh process recomputes boolean-enabled geometry and paint");
    check(encode(reopened.document())==bytes,"Cold reopen preserves exact canonical native authored bytes");
}
void native_portable_and_cold_reopen(const std::string& executable) {
    Session session(fixture());const auto source=definition(true);instantiate(session,source);
    apply(session,{MacroCommand{SetMacroOverride{"path","instance",amount_id,12}},MacroCommand{SetMacroOverride{"path","instance",copies_id,3}},
        MacroCommand{SetMacroBooleanOverride{"path","instance",offset_enabled_id,false}},MacroCommand{SetMacroBooleanOverride{"path","instance",repeater_enabled_id,true}}});
    const auto bytes=encode(session.document());auto native=parsed(bytes);
    const auto& wire=serialized_instance(native);const auto& overrides=wire.at("overrides").as_object();
    check(native.at("version").as_string()=="0.87"&&serialized_revision(native,1).at("interface_version")==3&&
        !serialized_revision(native,0).contains("interface_version"),"Native087 writes only the nonlegacy interface3 marker");
    check(overrides.at(offset_enabled_id).is_bool()&&!overrides.at(offset_enabled_id).as_bool()&&
        overrides.at(repeater_enabled_id).is_bool()&&overrides.at(repeater_enabled_id).as_bool()&&
        overrides.at(amount_id).is_number()&&overrides.at(copies_id).is_number()&&!wire.contains("boolean_overrides"),
        "One JSON overrides object preserves native false/true and unchanged numeric values without 0/1 coercion");
    const auto& public_parameters=serialized_revision(native,1).at("public_parameters").as_array();
    check(public_parameters.at(2).as_object().size()==7&&!public_parameters.at(2).as_object().contains("default")&&
        !public_parameters.at(2).as_object().contains("default_value"),"Enabled publication adds no independent default field");
    check(decode(bytes)==session.document(),"Native087 roundtrip retains every graph, interface, public mapping and typed override");
    for(const auto* version:{"0.86","0.85","0.83","0.82"}) {
        auto lying=native;lying["version"]=version;rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(boost::json::serialize(lying));});
    }
    for(const boost::json::value value:{boost::json::value(0),boost::json::value(1)}) {
        auto mistyped=native;serialized_instance(mistyped).at("overrides").as_object()[offset_enabled_id]=value;
        rejects("INVALID_MACRO_OVERRIDE",[&]{(void)decode(boost::json::serialize(mistyped));});
    }
    auto mistyped=native;serialized_instance(mistyped).at("overrides").as_object()[amount_id]=false;
    rejects("INVALID_MACRO_OVERRIDE",[&]{(void)decode(boost::json::serialize(mistyped));});
    auto missing=native;serialized_revision(missing,1).erase("interface_version");
    for(auto& value:serialized_revision(missing,1).at("nodes").as_array())value.as_object().at("operation").as_object()["enabled"]=true;
    rejects("INVALID_MACRO_INTERFACE",[&]{(void)decode(boost::json::serialize(missing));});
    auto future=native;serialized_revision(future,1)["interface_version"]=4;
    for(auto& value:serialized_revision(future,1).at("nodes").as_array())value.as_object().at("operation").as_object()["enabled"]=true;
    rejects("UNSUPPORTED_MACRO_INTERFACE_VERSION",[&]{(void)decode(boost::json::serialize(future));});
    auto old_source=definition();old_source.revisions.erase(2);old_source.latest_revision=1;Session old(fixture());instantiate(old,old_source,1);
    apply(old,{MacroCommand{SetMacroOverride{"path","instance",amount_id,12}}});auto old_wire=parsed(encode(old.document()));old_wire["version"]="0.86";
    check(decode(boost::json::serialize(old_wire))==old.document(),"Native086 remains readable with its unchanged numeric-only override shape");
    const auto portable=canonical_macro_payload(source);
    check(portable_macro_payload_schema(source)==3&&portable==canonical_macro_payload(source,3)&&
        read_canonical_macro_payload(portable)==source&&read_canonical_macro_payload(portable,3)==source,
        "Auto/schema3 portable codec retains all graph2/interface3 revisions and exact enabled defaults");
    validate_portable_macro_definition(source,3);
    for(const unsigned schema:{1U,2U}) {
        rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(source,schema);});
        rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{validate_portable_macro_definition(source,schema);});
        rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)read_canonical_macro_payload(portable,schema);});
    }
    const auto legacy_bytes=canonical_macro_payload(old_source,1);
    check(portable_macro_payload_schema(old_source)==1&&canonical_macro_payload(old_source,3)==legacy_bytes&&
        read_canonical_macro_payload(legacy_bytes,3)==old_source,"Schema3 support leaves legacy canonical payload bytes unchanged");
    auto retained=source;auto last=old_source.revisions.at(1);last.revision=3;retained.revisions.emplace(3,last);retained.latest_revision=3;
    check(portable_macro_payload_schema(retained)==3&&read_canonical_macro_payload(canonical_macro_payload(retained),3)==retained,
        "A retained boolean interface requires schema3 even when the newest revision is legacy");
    Session imported(fixture());InstantiateMacro import{"path","imported-definition","imported-instance",2,1};
    import.imported_definition=source;import.asset_id="portable-asset";import.accepted_asset_revision=7;
    import.overrides=instance(session).overrides;import.boolean_overrides=instance(session).boolean_overrides;
    const auto before_import=encode(imported.document());apply(imported,{MacroCommand{import}});
    check(instance(imported,"path","imported-instance").overrides==import.overrides&&
        instance(imported,"path","imported-instance").boolean_overrides==import.boolean_overrides&&
        imported.document().macro_definitions.at(import.definition).revisions==source.revisions,
        "Typed portable import preserves both override maps and retained interface3 graphs under fresh document identity");
    same_shape(evaluated(imported.document()),evaluated(session.document()),"Portable import evaluates exact boolean and numeric values");
    imported.undo(imported.revision());check(encode(imported.document())==before_import,"Typed portable import is one exact Undo");
    import.boolean_overrides[copies_id]=true;atomic(imported,"INVALID_MACRO_OVERRIDE",{MacroCommand{import}});
    const auto path=std::filesystem::temp_directory_path()/("nect-boolean-interface-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".nect");
    {std::ofstream file(path,std::ios::binary);file<<bytes;check(file.good(),"Typed native087 saved for cold reopen");}
#ifdef _WIN32
    const auto result=_spawnl(_P_WAIT,executable.c_str(),quoted(executable).c_str(),"--cold-read",quoted(path.string()).c_str(),static_cast<const char*>(nullptr));
#else
    const auto result=std::system((quoted(executable)+" --cold-read "+quoted(path.string())).c_str());
#endif
    std::filesystem::remove(path);check(result==0,"Fresh process cold-reopens native087 and verifies typed enabled semantics");
}
}
int main(int argc,char** argv) {
    const char* stage="cold reopen";
    try {
        if(argc==3&&std::string(argv[1])=="--cold-read") {cold_read(argv[2]);std::cout<<"Macro boolean cold checks: "<<checks<<"\n";return 0;}
        stage="independent defaults and overrides";independent_defaults_overrides_resets();
        stage="typed validation and interface";typed_validation_and_interface_boundaries();
        stage="pins, migration and detach";pins_migration_and_detach();
        stage="Preset capture and apply";presets_preserve_boolean_maps();
        stage="native and portable codecs";native_portable_and_cold_reopen(std::filesystem::absolute(argv[0]).string());
        std::cout<<"Macro boolean interface checks: "<<checks<<"\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"Macro boolean interface ("<<stage<<", "<<checks<<" checks): "<<error.what()<<"\n";return 1;}
}
