#include "nect/core.hpp"
#include "nect/io.hpp"
#include "nect/semantic_controls.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#include <process.h>
#endif

using namespace nect;
namespace {
int checks=0;
constexpr const char* amount_id="macro.offset.amount";
constexpr const char* copies_id="macro.copies";
constexpr const char* rotation_id="macro.rotation";
using Overrides=std::map<std::string,double>;
void check(bool ok,const std::string& reason) {
    if(!ok)throw std::runtime_error(reason);
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
    const auto bytes=encode(document);
    rejects(code,action);
    check(session.document()==document&&session.revision()==revision&&session.history()==history&&
        encode(session.document())==bytes,"Refusal preserves authored state, native bytes, revision and Undo history");
}
void atomic(Session& session,const char* code,std::vector<Command> commands) {
    unchanged(session,code,[&]{apply(session,std::move(commands));});
}
Document fixture() {
    auto document=empty_document("interface-doc","composition","artboard");
    Object path;path.id="path";path.name="Public interface path";
    Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t i=0;i<anchors.size();++i) {
        Point point;point.id="point-"+std::to_string(i+1);
        point.x.literal=anchors[i].x;point.y.literal=anchors[i].y;contour.points.push_back(point);
    }
    path.contours.push_back(contour);path.stack.emplace_back(default_operation("fill","nect.paint.fill"));
    document.objects.emplace(path.id,path);document.compositions.front().roots.push_back(path.id);return document;
}
MacroNode& node(MacroDefinitionRevision& revision,const Id& id) {
    const auto found=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& item){return item.operation.id==id;});
    if(found==revision.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
const MacroNode& node(const MacroDefinitionRevision& revision,const Id& id) {
    return node(const_cast<MacroDefinitionRevision&>(revision),id);
}
void connect(MacroDefinitionRevision& revision) {
    revision.edges.clear();MacroEndpoint cursor{"",revision.input.id};
    for(const auto& item:revision.nodes) {
        revision.edges.push_back({cursor,{item.operation.id,item.input_port}});
        cursor={item.operation.id,item.output_port};
    }
    revision.output_mapping=cursor;revision.edges.push_back({cursor,{"",revision.output.id}});
}
MacroDefinition definition(bool pair=false) {
    MacroDefinitionRevision initial;initial.graph_version=pair?1:2;
    initial.input={"input","local_paths_and_paint"};initial.output={"output","local_paths_and_paint"};
    const std::vector<Id> ids=pair?std::vector<Id>{"offset-b","repeater-a"}:
        std::vector<Id>{"offset-a","offset-b","repeater-a","repeater-b"};
    for(const auto& id:ids) {
        const bool offset=id.starts_with("offset");
        auto operation=default_operation(id,offset?"nect.shape.offset":"nect.shape.repeater");
        if(offset)operation.parameters.at("amount").literal=id=="offset-a"?2:3;
        else {
            operation.parameters.at("copies").literal=2;
            operation.parameters.at("position_x").literal=id=="repeater-a"?125:21;
            operation.parameters.at("position_y").literal=id=="repeater-a"?11:80;
            operation.parameters.at("rotation").literal=id=="repeater-a"?9:-13;
        }
        initial.nodes.push_back({operation,id+"-input",id+"-output"});
    }
    connect(initial);
    initial.public_parameters={{amount_id,"Amount","offset-b","amount","number","du","local_paths_and_paint"}};
    // Storage order is deliberately unrelated to the graph's execution order.
    std::reverse(initial.nodes.begin(),initial.nodes.end());std::reverse(initial.edges.begin(),initial.edges.end());
    auto published=initial;published.revision=2;published.interface_version=2;
    published.public_parameters.push_back({copies_id,"Copies","repeater-a","copies","number","scalar","local_paths_and_paint"});
    published.public_parameters.push_back({rotation_id,"Rotation",pair?"repeater-a":"repeater-b","rotation","number","degree","local_paths_and_paint"});
    MacroDefinition result;result.id="interface-definition";result.label="Mapped numeric controls";result.latest_revision=2;
    result.revisions={{1,std::move(initial)},{2,std::move(published)}};return result;
}
const MacroInstance& instance(const Session& session) {return *session.document().objects.at("path").stack.at(1).macro;}
void instantiate(Session& session,const MacroDefinition& source=definition(),std::uint64_t pin=2) {
    apply(session,{MacroCommand{CreateMacroDefinition{source}},MacroCommand{InstantiateMacro{"path",source.id,"instance",pin,1,std::nullopt,"",0,{}}}});
}
EvaluatedShape evaluated(const Document& document) {return evaluate_shape(document,"path",evaluate(document));}
Document ordinary(const MacroDefinitionRevision& revision,const Overrides& overrides={},bool pair=false,
    const Id& amount_node="offset-b",const Id& copies_node="repeater-a",const Id& rotation_node="repeater-b") {
    Session session(fixture());std::vector<Command> commands;std::size_t index=1;
    // Independent oracle: explicit fixture node identities and mappings, with
    // no use of production Macro traversal or public-parameter projection.
    const std::vector<Id> ids=pair?std::vector<Id>{"offset-b","repeater-a"}:
        std::vector<Id>{"offset-a","offset-b","repeater-a","repeater-b"};
    for(const auto& id:ids) {
        auto operation=node(revision,id).operation;
        if(id==amount_node&&overrides.contains(amount_id))operation.parameters.at("amount").literal=overrides.at(amount_id);
        if(id==copies_node&&overrides.contains(copies_id))operation.parameters.at("copies").literal=overrides.at(copies_id);
        if(id==(pair?"repeater-a":rotation_node)&&overrides.contains(rotation_id))operation.parameters.at("rotation").literal=overrides.at(rotation_id);
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
            for(std::size_t p=0;p<x.points.size();++p)
                if(!near(map_point(a[i].transform,x.points[p].anchor),map_point(b[i].transform,y.points[p].anchor))||
                    !near(map_point(a[i].transform,x.points[p].incoming),map_point(b[i].transform,y.points[p].incoming))||
                    !near(map_point(a[i].transform,x.points[p].outgoing),map_point(b[i].transform,y.points[p].outgoing)))return false;
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
void independent_overrides_resets_and_descriptors() {
    Session session(fixture());const auto original=encode(session.document());const auto source=definition();instantiate(session,source);
    check(source.revisions.at(1).interface_version==1&&source.revisions.at(2).interface_version==2,
        "Interface v2 is explicitly appended after the canonical legacy revision 1");
    same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(2))),"Published defaults match ordinary operations");
    session.undo(session.revision());check(encode(session.document())==original,"Creation and instantiation use one Undo");session.redo(session.revision());
    const std::array<std::pair<const char*,double>,3> values{{{amount_id,12},{copies_id,3},{rotation_id,-450.25}}};
    Overrides active;
    for(const auto& [id,value]:values) {
        const auto before=encode(session.document());const auto previous=evaluated(session.document());
        apply(session,{MacroCommand{SetMacroOverride{"path","instance",id,value}}});active[id]=value;
        check(instance(session).overrides==active&&macro_parameter_value(session.document(),"path","instance",id)==value,
            "Each stable PublicParamID independently reads back its local override");
        same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(2),active)),"Mapped control changes exactly its intended node parameter");
        check(!same_paths(previous.paths,evaluated(session.document()).paths),"Every published control independently changes geometry");
        const auto after=encode(session.document());session.undo(session.revision());
        check(encode(session.document())==before,"Each override is one exact Undo");session.redo(session.revision());
        check(encode(session.document())==after,"Each override has exact Redo");
    }
    check(session.document().macro_definitions.at(source.id)==source,"Local overrides leave every authored node default unchanged");
    const auto copies=macro_semantic_descriptor(session.document(),macro_parameter_ref("path","instance",copies_id));
    const auto rotation=macro_semantic_descriptor(session.document(),macro_parameter_ref("path","instance",rotation_id));
    const auto amount=macro_semantic_descriptor(session.document(),macro_parameter_ref("path","instance",amount_id));
    check(copies.key==copies_id&&copies.value_type=="number"&&copies.unit=="scalar"&&copies.minimum==0&&copies.maximum==1000&&copies.step==1&&copies.default_value==2,
        "Copies descriptor presents numeric integer-step bounds and the pinned default");
    check(rotation.key==rotation_id&&rotation.unit=="degree"&&rotation.default_value==-13&&rotation.minimum==-1e9&&rotation.maximum==1e9&&
        validate_semantic_descriptor(rotation).widget==SemanticWidget::angle,
        "Rotation descriptor keeps signed authored turns and uses the canonical angle control");
    check(amount.unit=="du"&&amount.default_value==3&&amount.minimum==-1e6&&amount.maximum==1e6,
        "Amount descriptor retains the mapped length limits and pinned default");
    for(const auto& [id,value]:values) {
        (void)value;const auto before=encode(session.document());
        apply(session,{MacroCommand{ResetMacroOverride{"path","instance",id}}});active.erase(id);
        check(instance(session).overrides==active,"Reset removes only the selected public override");
        same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(2),active)),"Independent reset restores only its mapped default");
        session.undo(session.revision());check(encode(session.document())==before,"One Undo restores a selected reset");session.redo(session.revision());
    }
    atomic(session,"NO_MACRO_OVERRIDE",{MacroCommand{ResetMacroOverride{"path","instance",copies_id}}});
}
void mapped_range_and_atomic_refusals() {
    Session session(fixture());instantiate(session);
    for(const double value:{-1.0,0.5,1000.5,1001.0})
        atomic(session,"OUT_OF_RANGE",{MacroCommand{SetMacroOverride{"path","instance",amount_id,10}},MacroCommand{SetMacroOverride{"path","instance",copies_id,value}}});
    for(const double value:{-1000000.1,1000000.1})atomic(session,"OUT_OF_RANGE",{MacroCommand{SetMacroOverride{"path","instance",amount_id,value}}});
    for(const double value:{-1000000000.1,1000000000.1})atomic(session,"OUT_OF_RANGE",{MacroCommand{SetMacroOverride{"path","instance",rotation_id,value}}});
    atomic(session,"NON_FINITE",{MacroCommand{SetMacroOverride{"path","instance",copies_id,std::numeric_limits<double>::quiet_NaN()}}});
    atomic(session,"NON_FINITE",{MacroCommand{SetMacroOverride{"path","instance",rotation_id,std::numeric_limits<double>::infinity()}}});
    atomic(session,"MISSING_MACRO_PARAMETER",{MacroCommand{SetMacroOverride{"path","instance","macro.missing",1}}});
    for(const auto& [id,value]:std::array<std::pair<const char*,double>,6>{{{copies_id,0},{copies_id,1000},{amount_id,-1e6},{amount_id,1e6},{rotation_id,-1e9},{rotation_id,1e9}}}) {
        apply(session,{MacroCommand{SetMacroOverride{"path","instance",id,value}}});
        check(macro_parameter_value(session.document(),"path","instance",id)==value,"Mapping-specific inclusive boundary is accepted without clamping");
    }
    Session zero(fixture());instantiate(zero);apply(zero,{MacroCommand{SetMacroOverride{"path","instance",copies_id,0}}});
    same_shape(evaluated(zero.document()),evaluated(ordinary(definition().revisions.at(2),{{copies_id,0}})),"Zero Copies follows canonical ordinary Repeater semantics");
    auto alias=definition();alias.revisions.at(2).public_parameters.front().id="macro.expansion";
    Session custom(fixture());instantiate(custom,alias);
    atomic(custom,"OUT_OF_RANGE",{MacroCommand{SetMacroOverride{"path","instance","macro.expansion",1e6+1}}});
    apply(custom,{MacroCommand{SetMacroOverride{"path","instance","macro.expansion",24}}});
    check(macro_parameter_value(custom.document(),"path","instance","macro.expansion")==24,"Custom Amount IDs use their mapped node limits and value");
    same_shape(evaluated(custom.document()),evaluated(ordinary(alias.revisions.at(2),{{amount_id,24}})),
        "Custom stable Amount ID evaluates its mapped Offset parameter");
}
void interface_validation_boundaries() {
    auto refuse=[](MacroDefinition source,const char* code) {
        Session session(fixture());atomic(session,code,{MacroCommand{CreateMacroDefinition{std::move(source)}}});
    };
    auto altered=definition();altered.revisions.at(1).interface_version=2;refuse(altered,"INVALID_MACRO_INTERFACE");
    altered=definition();altered.revisions.at(2).interface_version=4;refuse(altered,"UNSUPPORTED_MACRO_INTERFACE_VERSION");
    altered=definition();altered.revisions.at(2).interface_version=1;refuse(altered,"INVALID_MACRO_INTERFACE");
    altered=definition();altered.revisions.at(2).public_parameters[1].id=amount_id;refuse(altered,"DUPLICATE_MACRO_PARAMETER");
    altered=definition();altered.revisions.at(2).public_parameters.push_back({"macro.alias","Alias","repeater-a","copies","number","scalar","local_paths_and_paint"});
    refuse(altered,"INVALID_MACRO_MAPPING");
    for(const auto* id:{"copies","macro.","macro.bad space","macro.bad/field"}) {
        altered=definition();altered.revisions.at(2).public_parameters[1].id=id;refuse(altered,"INVALID_MACRO_INTERFACE");
    }
    altered=definition();altered.revisions.at(2).public_parameters[1].id="macro."+std::string(90,'x');
    validate_macro_definition(altered);check(true,"Stable PublicParamID accepts its 96-character boundary");
    altered.revisions.at(2).public_parameters[1].id+='x';refuse(altered,"INVALID_MACRO_INTERFACE");
    for(const auto* parameter:{"position_x","scale_x","amount"}) {
        altered=definition();altered.revisions.at(2).public_parameters[1].parameter=parameter;refuse(altered,"INVALID_MACRO_MAPPING");
    }
    altered=definition();altered.revisions.at(2).public_parameters[1].node="missing-node";refuse(altered,"INVALID_MACRO_MAPPING");
    altered=definition();altered.revisions.at(2).public_parameters[1].unit="degree";refuse(altered,"INVALID_MACRO_MAPPING");
    altered=definition();altered.revisions.at(2).public_parameters[2].value_type="integer";refuse(altered,"INVALID_MACRO_MAPPING");
    altered=definition();altered.revisions.at(2).public_parameters[2].domain="local_paths";refuse(altered,"INVALID_MACRO_MAPPING");
    altered=definition();altered.revisions.at(2).public_parameters[2].label.clear();refuse(altered,"INVALID_MACRO_INTERFACE");
    altered=definition();altered.revisions.at(2).public_parameters.front()={amount_id,"Misleading Amount","repeater-b","rotation","number","degree","local_paths_and_paint"};
    refuse(altered,"INVALID_MACRO_MAPPING");
    auto boundary=definition();auto& revision=boundary.revisions.at(2);revision.nodes.clear();revision.public_parameters.clear();
    for(std::size_t i=0;i<8;++i) {
        const auto id="repeater-"+std::to_string(i);auto operation=default_operation(id,"nect.shape.repeater");
        operation.parameters.at("copies").literal=1;
        revision.nodes.push_back({operation,id+"-in",id+"-out"});
        revision.public_parameters.push_back({"macro.copies-"+std::to_string(i),"Copies",id,"copies","number","scalar","local_paths_and_paint"});
        revision.public_parameters.push_back({"macro.rotation-"+std::to_string(i),"Rotation",id,"rotation","number","degree","local_paths_and_paint"});
    }
    connect(revision);Session maximum(fixture());instantiate(maximum,boundary);
    check(revision.public_parameters.size()==16&&instance(maximum).pinned_revision==2,"Interface v2 accepts 16 uniquely mapped numeric controls");
    revision.public_parameters.push_back({"macro.seventeenth","Extra","repeater-0","copies","number","scalar","local_paths_and_paint"});
    refuse(boundary,"INVALID_MACRO_INTERFACE");
    auto unpublished=definition();unpublished.revisions.at(2).public_parameters.clear();Session empty(fixture());instantiate(empty,unpublished);
    same_shape(evaluated(empty.document()),evaluated(ordinary(unpublished.revisions.at(2))),"Unpublished interface v2 evaluates only literal defaults");
}
void pins_migration_and_detach() {
    Session session(fixture());const auto source=definition();instantiate(session,source,1);
    atomic(session,"MISSING_MACRO_PARAMETER",{MacroCommand{SetMacroOverride{"path","instance",copies_id,3}}});
    auto next=source.revisions.at(2);next.revision=3;node(next,"offset-b").operation.parameters.at("amount").literal=8;
    node(next,"repeater-a").operation.parameters.at("copies").literal=3;node(next,"repeater-b").operation.parameters.at("rotation").literal=22;
    apply(session,{MacroCommand{UpdateMacroDefinition{source.id,next}}});
    check(instance(session).pinned_revision==1&&macro_parameter_value(session.document(),"path","instance",amount_id)==3,
        "Appending interface and defaults never changes an existing pin");
    same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(1))),"Legacy pin retains its original processing defaults");
    const auto before=encode(session.document());apply(session,{MacroCommand{UpdateMacroInstance{"path","instance",3}}});
    check(instance(session).pinned_revision==3&&macro_parameter_value(session.document(),"path","instance",copies_id)==3&&
        macro_parameter_value(session.document(),"path","instance",rotation_id)==22,"Explicit migration activates newly published controls and target defaults");
    same_shape(evaluated(session.document()),evaluated(ordinary(next)),"Explicit migration uses only its target revision defaults");
    session.undo(session.revision());check(encode(session.document())==before,"Pin migration uses one exact Undo");session.redo(session.revision());
    const Overrides overrides{{amount_id,12},{copies_id,4},{rotation_id,-450.25}};
    std::vector<Command> commands;for(const auto& [id,value]:overrides)commands.emplace_back(MacroCommand{SetMacroOverride{"path","instance",id,value}});
    apply(session,std::move(commands));
    auto remapped=next;remapped.revision=4;remapped.public_parameters[0].node="offset-a";
    remapped.public_parameters[1].node="repeater-b";remapped.public_parameters[2].node="repeater-a";
    apply(session,{MacroCommand{UpdateMacroDefinition{source.id,remapped}},MacroCommand{UpdateMacroInstance{"path","instance",4}}});
    check(instance(session).overrides==overrides,"Compatible node remapping retains every stable-ID override");
    same_shape(evaluated(session.document()),evaluated(ordinary(remapped,overrides,false,"offset-a","repeater-b","repeater-a")),"Compatible remapping applies each override at its new explicit internal Ref");
    auto changed_unit=remapped;changed_unit.revision=5;changed_unit.public_parameters[1]={copies_id,"Now Rotation","repeater-b","rotation","number","degree","local_paths_and_paint"};
    apply(session,{MacroCommand{UpdateMacroDefinition{source.id,changed_unit}}});
    atomic(session,"INCOMPATIBLE_MACRO_MIGRATION",{MacroCommand{UpdateMacroInstance{"path","instance",5}}});
    auto deleted=remapped;deleted.revision=6;deleted.public_parameters.erase(deleted.public_parameters.begin()+1);
    apply(session,{MacroCommand{UpdateMacroDefinition{source.id,deleted}}});
    atomic(session,"ORPHAN_MACRO_OVERRIDE",{MacroCommand{UpdateMacroInstance{"path","instance",6}}});
    atomic(session,"MACRO_IN_USE",{MacroCommand{DeleteMacroDefinition{source.id}}});
    const auto before_reset=encode(session.document());
    apply(session,{MacroCommand{ResetMacroOverride{"path","instance",copies_id}},MacroCommand{UpdateMacroInstance{"path","instance",6}}});
    check(instance(session).pinned_revision==6&&!instance(session).overrides.contains(copies_id),"Explicit reset permits migration past a removed control");
    session.undo(session.revision());check(encode(session.document())==before_reset,"Reset and migration are one atomic Undo");
    const auto before_detach=encode(session.document());const auto shape=evaluated(session.document());
    apply(session,{MacroCommand{DetachMacroInstance{"path","instance","materialized"}}});
    const auto& stack=session.document().objects.at("path").stack;
    check(stack.size()==5&&stack[1].parameters.at("amount").literal==12&&stack[3].parameters.at("rotation").literal==-450.25&&
        stack[2].parameters.at("amount").literal==8&&stack[4].parameters.at("copies").literal==4&&
        std::none_of(stack.begin(),stack.end(),[](const auto& entry){return entry.macro.has_value();}),
        "Detach materializes mapped Amount Copies Rotation selectively in execution order");
    same_shape(evaluated(session.document()),shape,"Generic detach preserves full cubic and paint semantics");
    session.undo(session.revision());check(encode(session.document())==before_detach,"Multi-control detach is one Undo");session.redo(session.revision());
    apply(session,{MacroCommand{DeleteMacroDefinition{source.id}}});check(session.document().macro_definitions.empty(),"Detached graphs no longer block definition deletion");
}
PresetDefinition preset(const Overrides& overrides) {
    PresetDefinition result;result.id="interface-preset";result.schema_version=2;result.label="Published defaults";result.category="Shape";
    PresetEntry entry;entry.kind="macro";entry.type=macro_entry_type;entry.macro_definition="interface-definition";
    entry.pinned_revision=2;entry.overrides=overrides;result.entries.push_back(std::move(entry));return result;
}
void preset_mapping_ranges() {
    Session session(fixture());instantiate(session);
    for(const auto& overrides:std::vector<Overrides>{{{copies_id,0.5}},{{copies_id,1001}},{{amount_id,1e6+1}},{{rotation_id,1e9+1}}}) {
        unchanged(session,"OUT_OF_RANGE",[&]{session.apply_preset_command(PresetCommand{CreatePreset{preset(overrides)}},session.revision());});
    }
    const Overrides values{{amount_id,12},{copies_id,3},{rotation_id,-450.25}};
    session.apply_preset_command(PresetCommand{CreatePreset{preset(values)}},session.revision());
    auto invalid=preset({{copies_id,-1}});
    unchanged(session,"OUT_OF_RANGE",[&]{session.apply_preset_command(PresetCommand{UpdatePreset{invalid}},session.revision());});
    auto source=fixture();source.macro_definitions.emplace("interface-definition",definition());source.preset_definitions.emplace("interface-preset",preset({{copies_id,0.5}}));
    Session retained(std::move(source));
    unchanged(retained,"OUT_OF_RANGE",[&]{retained.apply_preset_command(PresetCommand{ApplyPreset{"interface-preset","path","invalid"}},retained.revision());});
    auto valid=fixture();valid.macro_definitions.emplace("interface-definition",definition());valid.preset_definitions.emplace("interface-preset",preset(values));
    Session applied(std::move(valid));const auto before=encode(applied.document());
    applied.apply_preset_command(PresetCommand{ApplyPreset{"interface-preset","path","applied"}},applied.revision());
    check(applied.document().objects.at("path").stack.at(1).macro->overrides==values,"Preset application retains three distinct mapped numeric overrides");
    same_shape(evaluated(applied.document()),evaluated(ordinary(definition().revisions.at(2),values)),"Document-local preset uses the same mapped geometry");
    check(decode(encode(applied.document()))==applied.document(),"Native86 retains document-local preset mappings and overrides");
    applied.undo(applied.revision());check(encode(applied.document())==before,"Preset application is one atomic Undo");
}
std::string quoted(const std::string& value) {
#ifdef _WIN32
    return "\""+value+"\"";
#else
    std::string result="'";for(const auto ch:value)result+=ch=='\''?"'\\''":std::string(1,ch);return result+"'";
#endif
}
void cold_read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);check(file.good(),"Cold process can open saved native86");
    const std::string bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};Session reopened(decode(bytes));
    const auto& revision=reopened.document().macro_definitions.at("interface-definition").revisions.at(2);
    const Overrides values{{amount_id,12},{copies_id,3},{rotation_id,-450.25}};
    check(revision.interface_version==2&&instance(reopened).pinned_revision==2&&instance(reopened).overrides==values,
        "Cold reopen preserves explicit interface version pin and three exact signed numeric overrides");
    same_shape(evaluated(reopened.document()),evaluated(ordinary(revision,values)),"Fresh process recomputes the mapped cubic and painted geometry");
    check(encode(reopened.document())==bytes,"Cold reopen preserves exact canonical native authored bytes");
}
void native_portable_and_cold_reopen(const std::string& executable) {
    Session session(fixture());instantiate(session);apply(session,{MacroCommand{SetMacroOverride{"path","instance",amount_id,12}},
        MacroCommand{SetMacroOverride{"path","instance",copies_id,3}},MacroCommand{SetMacroOverride{"path","instance",rotation_id,-450.25}}});
    const auto bytes=encode(session.document());auto native=parsed(bytes);
    check(native.at("version").as_string()==native_version&&boost::json::value_to<unsigned>(serialized_revision(native,1).at("interface_version"))==2&&
        !serialized_revision(native,0).contains("interface_version"),"Native86 writes the required v2 marker and omits the legacy default marker");
    check(decode(bytes)==session.document(),"Native86 roundtrip preserves every retained definition revision and public mapping");
    for(const auto* version:{"0.85","0.83","0.82","0.65","0.1"}) {
        auto lying=native;lying["version"]=version;
        rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(boost::json::serialize(lying));});
    }
    auto missing=native;serialized_revision(missing,1).erase("interface_version");
    rejects("INVALID_MACRO_INTERFACE",[&]{(void)decode(boost::json::serialize(missing));});
    auto future=native;serialized_revision(future,1)["interface_version"]=4;
    rejects("UNSUPPORTED_MACRO_INTERFACE_VERSION",[&]{(void)decode(boost::json::serialize(future));});
    auto legacy=definition(true);legacy.revisions.erase(2);legacy.latest_revision=1;Session old(fixture());instantiate(old,legacy,1);
    auto old_json=parsed(encode(old.document()));
    check(!serialized_revision(old_json,0).contains("graph_version")&&!serialized_revision(old_json,0).contains("interface_version"),
        "Legacy pair omission keeps existing graph and Amount interface bytes canonical");
    for(const auto* version:{"0.82","0.83","0.84","0.85"}) {
        old_json["version"]=version;check(decode(boost::json::serialize(old_json))==old.document(),"Pre86 legacy Macro defaults remain exactly readable");
    }
    serialized_revision(old_json,0)["interface_version"]=1;
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(boost::json::serialize(old_json));});
    const auto portable=canonical_macro_payload(legacy);check(read_canonical_macro_payload(portable)==legacy,"Portable Macro v1 retains its legacy Amount contract");
    const auto pair=definition(true);rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(pair,1);});
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{validate_portable_macro_definition(pair);});
    Session paired(fixture());instantiate(paired,pair);
    const auto payload=boost::json::serialize(parsed(encode(paired.document())).at("macros").as_array().front());
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)read_canonical_macro_payload(payload,1);});
    const auto pair_payload=canonical_macro_payload(pair);
    check(pair_payload==canonical_macro_payload(pair,2)&&read_canonical_macro_payload(pair_payload)==pair&&
        read_canonical_macro_payload(pair_payload,2)==pair,"Auto/schema2 codec retains the graph1/interface2 mapping contract");
    Session imported(fixture());const auto before_import=encode(imported.document());
    apply(imported,{MacroCommand{InstantiateMacro{
        "path","imported-definition","imported-instance",2,1,pair,"portable-asset",41,{}}}});
    check(imported.document().macro_definitions.at("imported-definition").revisions==pair.revisions&&
        imported.document().objects.at("path").stack[1].macro->pinned_revision==2,
        "Typed schema2 import preserves interface2 mappings and explicit pin under a fresh Document identity");
    same_shape(evaluated(imported.document()),evaluated(ordinary(pair.revisions.at(2),{},true)),"Imported interface2 pair preserves mapped geometry");
    imported.undo(imported.revision());check(encode(imported.document())==before_import,"Typed interface2 import is one exact Undo");
    // All historical version strings still accept a valid feature-free document.
    for(unsigned minor=1;minor<=85;++minor) {
        boost::json::object historical{{"format","nect-native"},{"version","0."+std::to_string(minor)},{"id","historical-document"},
            {"units","du96"},{"color_space","srgb"},{"compositions",boost::json::array{}},{"objects",boost::json::array{}},{"collections",boost::json::array{}}};
        if(minor>=7)historical["named_colors"]=boost::json::array{};
        if(minor>=13)historical["raster_assets"]=boost::json::array{};
        if(minor>=63)historical["presets"]=boost::json::array{};
        if(minor>=64)historical["definitions"]=boost::json::array{};
        if(minor>=65)historical["macros"]=boost::json::array{};
        check(decode(boost::json::serialize(historical)).id=="historical-document","Native86 reader retains historical0."+std::to_string(minor)+" support");
    }
    const auto path=std::filesystem::temp_directory_path()/("nect-public-interface-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".nect");
    {std::ofstream file(path,std::ios::binary);file<<bytes;check(file.good(),"Three-control native86 saved for cold reopen");}
#ifdef _WIN32
    const auto result=_spawnl(_P_WAIT,executable.c_str(),quoted(executable).c_str(),"--cold-read",quoted(path.string()).c_str(),static_cast<const char*>(nullptr));
#else
    const auto result=std::system((quoted(executable)+" --cold-read "+quoted(path.string())).c_str());
#endif
    std::filesystem::remove(path);check(result==0,"Fresh process cold-reopens native86 and verifies all mapped semantics");
}
}
int main(int argc,char** argv) {
    const char* stage="cold reopen";
    try {
        if(argc==3&&std::string(argv[1])=="--cold-read") {cold_read(argv[2]);std::cout<<"Macro public interface cold checks: "<<checks<<"\n";return 0;}
        stage="independent controls";independent_overrides_resets_and_descriptors();
        stage="mapped bounds";mapped_range_and_atomic_refusals();
        stage="interface validation";interface_validation_boundaries();
        stage="pins and detach";pins_migration_and_detach();
        stage="presets";preset_mapping_ranges();
        stage="native and portable codec";native_portable_and_cold_reopen(std::filesystem::absolute(argv[0]).string());
        std::cout<<"Macro public interface checks: "<<checks<<"\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"Macro public interface ("<<stage<<", "<<checks<<" checks): "<<error.what()<<"\n";return 1;}
}
