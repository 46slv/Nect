#include "nect/core.hpp"
#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& message) {
    if(!ok)throw std::runtime_error(message);
    ++checks;
}
template<class F>void rejects(const char* code,F action) {
    try { action(); }
    catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        return;
    }
    throw std::runtime_error("Expected "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands) {
    session.apply(commands,session.revision());
}
void atomic(Session& session,const char* code,std::vector<Command> commands) {
    const auto before=session.document();const auto revision=session.revision();const auto history=session.history();
    const auto native=encode(session.document());
    rejects(code,[&]{apply(session,std::move(commands));});
    check(session.document()==before&&session.revision()==revision&&session.history()==history&&
        encode(session.document())==native,
        "Rejected Macro command preserves document, revision, history and native bytes");
}
Contour rectangle() {
    Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> points{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t i=0;i<points.size();++i) {
        Point point;point.id="point-"+std::to_string(i+1);
        point.x.literal=points[i].x;point.y.literal=points[i].y;contour.points.push_back(point);
    }
    return contour;
}
Document fixture() {
    auto document=empty_document("doc","composition","artboard");
    Object object;object.id="path";object.name="Macro path";object.contours={rectangle()};
    object.stack.emplace_back(default_operation("fill","nect.paint.fill"));
    document.objects.emplace(object.id,object);document.compositions.front().roots.push_back(object.id);
    return document;
}
MacroDefinition macro_definition(const Id& offset_node="node-offset",double amount=12) {
    auto offset=default_operation(offset_node,"nect.shape.offset");
    offset.parameters.at("amount").literal=amount;
    auto repeater=default_operation("node-repeater","nect.shape.repeater");
    repeater.parameters.at("copies").literal=2;
    repeater.parameters.at("position_x").literal=125;
    MacroDefinitionRevision revision;revision.revision=1;
    revision.input={"input-port","local_paths_and_paint"};
    revision.output={"output-port","local_paths_and_paint"};
    revision.nodes={{offset,"offset-input","offset-output"},{repeater,"repeater-input","repeater-output"}};
    // Deliberately store edges in a different order from execution order.
    revision.edges={
        {{"node-repeater","repeater-output"},{"","output-port"}},
        {{"","input-port"},{offset_node,"offset-input"}},
        {{offset_node,"offset-output"},{"node-repeater","repeater-input"}}};
    revision.output_mapping={"node-repeater","repeater-output"};
    revision.public_parameters.push_back({"macro.offset.amount","Amount",offset_node,"amount",
        "number","du","local_paths_and_paint"});
    MacroDefinition definition;definition.id="macro-definition";definition.label="Offset Repeat";
    definition.latest_revision=1;definition.revisions.emplace(1,std::move(revision));
    return definition;
}
EvaluatedShape evaluated(const Document& document) {
    return evaluate_shape(document,"path",evaluate(document));
}
Bounds bounds(const EvaluatedShape& shape) {
    Bounds result{INFINITY,INFINITY,-INFINITY,-INFINITY};
    for(const auto& instance:shape.paths)for(const auto& contour:*instance.contours)for(const auto& point:contour.points) {
        const auto p=map_point(instance.transform,point.anchor);
        result.left=std::min(result.left,p.x);result.top=std::min(result.top,p.y);
        result.right=std::max(result.right,p.x);result.bottom=std::max(result.bottom,p.y);
    }
    return result;
}
void same_bounds(const Bounds& a,const Bounds& b,const char* reason) {
    check(std::abs(a.left-b.left)<1e-7&&std::abs(a.top-b.top)<1e-7&&
        std::abs(a.right-b.right)<1e-7&&std::abs(a.bottom-b.bottom)<1e-7,reason);
}
boost::json::object parsed(const std::string& raw) { return boost::json::parse(raw).as_object(); }
boost::json::object api(Session& session,boost::json::object request_json) {
    return parsed(request(session,boost::json::serialize(request_json)));
}
bool matches_ref(const boost::json::value& value,const Ref& ref) {
    const auto& object=value.as_object();
    return boost::json::value_to<std::string>(object.at("object"))==ref.object&&
        boost::json::value_to<std::string>(object.at("point"))==ref.point&&
        boost::json::value_to<std::string>(object.at("field"))==ref.field;
}
const boost::json::object& find_property(const boost::json::array& properties,const Ref& ref) {
    for(const auto& property:properties)
        if(matches_ref(property.as_object().at("ref"),ref))return property.as_object();
    throw std::runtime_error("Macro public property is absent from properties");
}
void mixed_stack_and_property_contract() {
    auto document=fixture();Session session(document);
    auto definition=macro_definition();
    std::reverse(definition.revisions.at(1).nodes.begin(),definition.revisions.at(1).nodes.end());
    apply(session,{MacroCommand{CreateMacroDefinition{definition}}});
    apply(session,{MacroCommand{InstantiateMacro{"path",definition.id,"macro-instance",1,1}}});
    check(session.document().objects.at("path").stack.size()==2&&
        session.document().objects.at("path").stack[0].type=="nect.paint.fill"&&
        session.document().objects.at("path").stack[1].macro.has_value(),
        "Macro instance remains in the ordered stack beside an ordinary Fill");
    const Ref amount_ref=macro_parameter_ref("path","macro-instance","macro.offset.amount");
    check(amount_ref==Ref{"path","macro-instance","macro.offset.amount"},
        "Public parameter uses the canonical stable Ref macro.offset.amount without a duplicated prefix");
    auto property_list=api(session,{{"op","properties"}}).at("result").as_array();
    const auto& listed=find_property(property_list,amount_ref);
    check(listed.at("unit").as_string()=="du"&&listed.at("origin").as_string()=="macro_default"&&
        boost::json::value_to<double>(listed.at("evaluated"))==12,
        "Properties publishes Macro Amount with its canonical Ref, unit and pinned default");
    const auto got=api(session,{{"op","get"},{"ref",boost::json::object{
        {"object","path"},{"point","macro-instance"},{"field","macro.offset.amount"}}}});
    check(got.at("ok").as_bool()&&matches_ref(got.at("result").as_object().at("ref"),amount_ref)&&
        boost::json::value_to<double>(got.at("result").as_object().at("evaluated"))==12,
        "get exposes the same canonical Macro Amount Ref and value");

    Session ordinary(document);const auto& revision=definition.revisions.at(1);
    const auto offset=std::find_if(revision.nodes.begin(),revision.nodes.end(),[](const auto& node){return node.operation.type=="nect.shape.offset";});
    const auto repeater=std::find_if(revision.nodes.begin(),revision.nodes.end(),[](const auto& node){return node.operation.type=="nect.shape.repeater";});
    apply(ordinary,{AddOperation{"path",offset->operation,1},AddOperation{"path",repeater->operation,2}});
    same_bounds(bounds(evaluated(session.document())),bounds(evaluated(ordinary.document())),
        "Macro evaluates Offset then Repeater by graph identity despite reversed node storage order");

    const auto before_override=session.revision();
    auto set=api(session,{{"op","apply"},{"expected_revision",before_override},{"commands",boost::json::array{
        boost::json::object{{"type","set_macro_override"},{"object","path"},{"instance","macro-instance"},
            {"public_parameter","macro.offset.amount"},{"value",34}}}}});
    check(set.at("ok").as_bool()&&session.revision()==before_override+1,
        "JSON-lines API applies the public control through one Macro Session command");
    check(macro_parameter_value(session.document(),"path","macro-instance","macro.offset.amount")==34,
        "Macro override readback uses the same stable public ID");
    property_list=api(session,{{"op","properties"}}).at("result").as_array();
    check(find_property(property_list,amount_ref).at("origin").as_string()=="macro_override"&&
        boost::json::value_to<double>(find_property(property_list,amount_ref).at("authored"))==34,
        "Properties readback marks the authored Macro override");
    const auto override_shape=evaluated(session.document());
    check(bounds(override_shape).left<bounds(evaluated(ordinary.document())).left,
        "Published Offset amount changes the evaluated processing sequence");
    atomic(session,"OUT_OF_RANGE",{MacroCommand{SetMacroOverride{"path","macro-instance","macro.offset.amount",1e6+1}}});
    atomic(session,"MISSING_MACRO_PARAMETER",{MacroCommand{SetMacroOverride{"path","macro-instance","offset.amount",10}}});
    session.undo(session.revision());
    check(macro_parameter_value(session.document(),"path","macro-instance","macro.offset.amount")==12,
        "One Undo removes the atomic public parameter override");
    session.redo(session.revision());
    check(macro_parameter_value(session.document(),"path","macro-instance","macro.offset.amount")==34,
        "Redo restores the same Macro override");

    const auto active_bounds=bounds(evaluated(session.document()));
    apply(session,{EnableOperation{"path","macro-instance",false}});
    check(session.document().objects.at("path").stack[1].id=="macro-instance"&&
        session.document().objects.at("path").stack[1].macro->pinned_revision==1,
        "Bypass retains Macro identity and pinned revision");
    check(bounds(evaluated(session.document())).right<active_bounds.right,
        "Disabled Macro is a pass-through in the mixed stack");
    apply(session,{EnableOperation{"path","macro-instance",true}});
    apply(session,{ReorderOperations{"path",{"macro-instance","fill"}}});
    check(session.document().objects.at("path").stack[0].id=="macro-instance"&&
        session.document().objects.at("path").stack[1].id=="fill",
        "Reorder moves the Macro and ordinary operation in one ordered stack");
    apply(session,{ReorderOperations{"path",{"fill","macro-instance"}}});

    PresetDefinition metadata;metadata.id="must-not-flatten";metadata.label="Mixed";
    metadata.category="Shape";
    const auto before=session.document();const auto before_revision=session.revision();const auto before_history=session.history();
    rejects("PRESET_NONPORTABLE_SOURCE",[&]{session.apply_preset_command(
        PresetCommand{CreatePresetFromStack{metadata,"path"}},session.revision());});
    check(session.document()==before&&session.revision()==before_revision&&session.history()==before_history,
        "Preset v1 refuses Macro stacks atomically without flattening the Macro source");
}
void revisions_detach_and_native_contract() {
    auto document=fixture();Session session(document);auto definition=macro_definition();
    apply(session,{MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"path",definition.id,"macro-instance",1,1}},
        MacroCommand{SetMacroOverride{"path","macro-instance","macro.offset.amount",24}}});
    const auto before_update=evaluated(session.document());
    auto next=definition.revisions.at(1);next.revision=2;
    next.nodes.front().operation.parameters.at("amount").literal=18;
    next.public_parameters.clear();std::reverse(next.nodes.begin(),next.nodes.end());
    apply(session,{MacroCommand{UpdateMacroDefinition{definition.id,next}}});
    check(session.document().macro_definitions.at(definition.id).latest_revision==2&&
        session.document().objects.at("path").stack[1].macro->pinned_revision==1,
        "Definition update appends revision 2 while existing instance remains pinned to revision 1");
    same_bounds(bounds(before_update),bounds(evaluated(session.document())),
        "Appending a Macro revision does not change existing pinned instances");
    atomic(session,"ORPHAN_MACRO_OVERRIDE",{MacroCommand{UpdateMacroInstance{"path","macro-instance",2}}});
    atomic(session,"MACRO_IN_USE",{MacroCommand{DeleteMacroDefinition{definition.id}}});
    apply(session,{MacroCommand{ResetMacroOverride{"path","macro-instance","macro.offset.amount"}},
        MacroCommand{UpdateMacroInstance{"path","macro-instance",2}}});
    check(session.document().objects.at("path").stack[1].macro->pinned_revision==2,
        "Explicit migration succeeds after orphaned override is reset");
    check(macro_parameter_ref("path","macro-instance","macro.offset.amount").field=="macro.offset.amount",
        "Public ID remains canonical after migration removes the parameter from revision 2");

    const auto macro_native=encode(session.document());
    check(macro_native.find(test_support::current_native_version_marker())!=std::string::npos&&
        decode(macro_native)==session.document(),"Native 0.67 roundtrips Macro definitions and tagged mixed stacks");
    auto lied_native=macro_native;
    test_support::remove_empty_native_076_templates_for_legacy_fixture(lied_native);
    auto lied=boost::json::parse(lied_native).as_object();lied["version"]="0.64";
    rejects("UNKNOWN_FIELD",[&]{(void)decode(boost::json::serialize(lied));});

    auto legacy=boost::json::parse(test_support::untag_ordinary_processing_entries_for_legacy_fixture(encode(document))).as_object();
    legacy["version"]="0.64";legacy.erase("macros");
    const auto old_native=boost::json::serialize(legacy);
    check(decode(old_native)==document,"Native 0.64 untagged ordinary stack entries remain readable as ordinary operations");

    auto collision_definition=macro_definition("collision-detached-1");
    Session collision(fixture());
    apply(collision,{MacroCommand{CreateMacroDefinition{collision_definition}},
        MacroCommand{InstantiateMacro{"path",collision_definition.id,"collision-instance",1,1}}});
    atomic(collision,"DUPLICATE_ID",{MacroCommand{DetachMacroInstance{"path","collision-instance","collision"}}});

    apply(session,{MacroCommand{DetachMacroInstance{"path","macro-instance","fresh-op"}}});
    const auto& stack=session.document().objects.at("path").stack;
    check(stack.size()==3&&stack[0].id=="fill"&&stack[1].id=="fresh-op-detached-1"&&
        stack[2].id=="fresh-op-detached-2"&&stack[1].type=="nect.shape.offset"&&
        stack[2].type=="nect.shape.repeater"&&!stack[1].macro&&!stack[2].macro,
        "Detach replaces the Macro at the same stack position with fresh ordered ordinary operations");
    check(stack[1].id!="collision-detached-1"&&stack[1].id!="macro-definition"&&
        stack[1].id!="macro.offset.amount"&&stack[1].id!="node-repeater",
        "Detached operation IDs do not reuse Macro definition, graph-node, or public parameter IDs");
    apply(session,{MacroCommand{DeleteMacroDefinition{definition.id}}});
    check(!session.document().macro_definitions.contains(definition.id),
        "Macro definition can be deleted after its final live instance is detached");
}
void compatible_public_mapping_migration_contract() {
    Session session(fixture());
    const auto definition=macro_definition();
    apply(session,{MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"path",definition.id,"macro-instance",1,1}},
        MacroCommand{SetMacroOverride{"path","macro-instance","macro.offset.amount",27}}});
    const auto old_output=bounds(evaluated(session.document()));
    auto replacement=definition.revisions.at(1);
    replacement.revision=2;
    replacement.nodes.front().operation.id="replacement-offset";
    replacement.nodes.front().operation.parameters.at("amount").literal=19;
    replacement.public_parameters.front().node="replacement-offset";
    for(auto& edge:replacement.edges) {
        if(edge.from.node=="node-offset")edge.from.node="replacement-offset";
        if(edge.to.node=="node-offset")edge.to.node="replacement-offset";
    }
    apply(session,{MacroCommand{UpdateMacroDefinition{definition.id,replacement}}});
    check(session.document().objects.at("path").stack[1].macro->pinned_revision==1,
        "Appending a compatible replacement graph does not silently upgrade the instance");
    apply(session,{MacroCommand{UpdateMacroInstance{"path","macro-instance",2}}});
    check(session.document().objects.at("path").stack[1].macro->pinned_revision==2&&
        session.document().macro_definitions.at(definition.id).revisions.at(2).public_parameters.front().node=="replacement-offset"&&
        macro_parameter_value(session.document(),"path","macro-instance","macro.offset.amount")==27,
        "Explicit migration preserves the PublicParamID override through a replacement internal node Ref");
    same_bounds(old_output,bounds(evaluated(session.document())),
        "Compatible revision migration preserves evaluated output with the local override");
    session.undo(session.revision());
    check(session.document().objects.at("path").stack[1].macro->pinned_revision==1&&
        macro_parameter_value(session.document(),"path","macro-instance","macro.offset.amount")==27,
        "Undo restores the old pinned revision and exact public override");
    session.redo(session.revision());
    check(session.document().objects.at("path").stack[1].macro->pinned_revision==2&&
        macro_parameter_value(session.document(),"path","macro-instance","macro.offset.amount")==27,
        "Redo restores the replacement mapping and the same public override");
}
void invalid_graphs_and_stale_commands_are_atomic() {
    auto reject_definition=[](MacroDefinition definition,const char* code) {
        Session session(fixture());
        atomic(session,code,{MacroCommand{CreateMacroDefinition{std::move(definition)}}});
    };
    auto duplicate_definition=macro_definition();duplicate_definition.id="path";
    reject_definition(duplicate_definition,"DUPLICATE_ID");
    auto duplicate_node=macro_definition();
    duplicate_node.revisions.at(1).nodes[1].operation.id=duplicate_node.revisions.at(1).nodes[0].operation.id;
    reject_definition(duplicate_node,"DUPLICATE_ID");
    auto duplicate_port=macro_definition();
    duplicate_port.revisions.at(1).nodes[1].input_port=duplicate_port.revisions.at(1).nodes[0].input_port;
    reject_definition(duplicate_port,"DUPLICATE_ID");
    auto duplicate_public=macro_definition();
    duplicate_public.revisions.at(1).public_parameters.push_back(duplicate_public.revisions.at(1).public_parameters.front());
    reject_definition(duplicate_public,"DUPLICATE_MACRO_PARAMETER");
    auto missing_type=macro_definition();missing_type.revisions.at(1).nodes[0].operation.type="nect.shape.unknown";
    reject_definition(missing_type,"INVALID_MACRO_ORDER");
    auto unsupported_version=macro_definition();unsupported_version.revisions.at(1).nodes[0].operation.version=2;
    reject_definition(unsupported_version,"UNSUPPORTED_MACRO_NODE_VERSION");
    auto driven_node=macro_definition();driven_node.revisions.at(1).nodes[0].operation.enabled_expression=Expression{"false",1};
    reject_definition(driven_node,"INVALID_MACRO_NODE");
    auto incompatible_domain=macro_definition();incompatible_domain.revisions.at(1).input.domain="local_paths";
    reject_definition(incompatible_domain,"INVALID_MACRO_DOMAIN");
    auto cycle=macro_definition();cycle.revisions.at(1).edges[2]={
        {"node-repeater","repeater-output"},{"node-offset","offset-input"}};
    reject_definition(cycle,"INVALID_MACRO_GRAPH");
    auto wrong_output=macro_definition();wrong_output.revisions.at(1).output_mapping={"node-offset","offset-output"};
    reject_definition(wrong_output,"INVALID_MACRO_GRAPH");

    Session session(fixture());auto definition=macro_definition();
    Session invalid_payload(fixture());
    MacroCommand missing_payload{CreateMacroDefinition{definition}};missing_payload.mutation.reset();
    atomic(invalid_payload,"INVALID_MACRO_COMMAND",{missing_payload});
    const auto stale_revision=session.revision();
    apply(session,{MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"path",definition.id,"macro-instance",1,1}}});
    const Ref macro_enabled=operation_ref("path","macro-instance","enabled");
    const Ref builtin_enabled=operation_ref("path","fill","enabled");
    atomic(session,"INVALID_DOMAIN",{SetOperationEnabledExpression{macro_enabled,Expression{"false",1}}});
    atomic(session,"INVALID_DOMAIN",{SetOperationEnabledExpression{builtin_enabled,
        Expression{"ref(\"path\",\"\",\"op.macro-instance.enabled\")",1}}});
    const auto before=session.document();const auto revision=session.revision();const auto history=session.history();
    const auto native=encode(session.document());
    rejects("REVISION_CONFLICT",[&]{session.apply({MacroCommand{SetMacroOverride{
        "path","macro-instance","macro.offset.amount",20}}},stale_revision);});
    check(session.document()==before&&session.revision()==revision&&session.history()==history&&
        encode(session.document())==native,"Stale Session revision preserves Macro state and native bytes");

    auto incompatible_revision=definition.revisions.at(1);incompatible_revision.revision=2;
    incompatible_revision.public_parameters.front().unit="scalar";
    atomic(session,"INVALID_MACRO_MAPPING",{MacroCommand{UpdateMacroDefinition{definition.id,incompatible_revision}}});
}
boost::json::object imported_macro_command(const MacroDefinition& definition,const std::string& definition_id,
    const std::string& instance,std::uint64_t pin,std::size_t index,
    std::map<std::string,double> overrides={}) {
    boost::json::object override_json;
    for(const auto& [public_id,value]:overrides)override_json[public_id]=value;
    auto command=boost::json::object{{"type","import_apply_macro"},
        {"definition",boost::json::parse(canonical_macro_payload(definition))},
        {"definition_id",definition_id},{"object","path"},{"instance",instance},
        {"pinned_revision",pin},{"index",index},{"asset_id","workspace-asset"},{"accepted_revision",41}};
    if(!overrides.empty())command["overrides"]=std::move(override_json);
    return command;
}
void portable_import_contract() {
    auto source=macro_definition();
    auto revision2=source.revisions.at(1);revision2.revision=2;
    revision2.nodes.front().operation.parameters.at("amount").literal=18;
    source.revisions.emplace(2,revision2);
    auto revision3=revision2;revision3.revision=3;
    revision3.nodes.front().operation.parameters.at("amount").literal=24;
    revision3.public_parameters.clear();
    source.revisions.emplace(3,revision3);source.latest_revision=3;
    const auto canonical=canonical_macro_payload(source);
    check(read_canonical_macro_payload(canonical)==source&&source.revisions.size()==3&&
        source.revisions.at(1).nodes.front().operation.id==source.revisions.at(3).nodes.front().operation.id,
        "Canonical standalone Macro payload keeps every retained graph revision and graph-local identity");
    auto malformed=canonical;const auto marker=malformed.find("\"latest_revision\":3");
    check(marker!=std::string::npos,"Canonical Macro fixture has a latest revision field");
    malformed.insert(marker,"\"latest_revision\":3,");
    rejects("UNAVAILABLE_MACRO_ASSET",[&]{(void)read_canonical_macro_payload(malformed);});

    Session session(fixture());
    auto first=api(session,{{"op","apply"},{"expected_revision",session.revision()},
        {"commands",boost::json::array{imported_macro_command(source,"imported-definition-1","imported-instance-1",1,1)}}});
    check(first.at("ok").as_bool()&&session.revision()==1,
        "Import and instantiate commits as one ordinary Session apply command and one revision");
    const auto& imported=session.document().macro_definitions.at("imported-definition-1");
    check(imported.id=="imported-definition-1"&&imported.id!=source.id&&imported.revisions==source.revisions&&
        imported.latest_revision==3&&macro_parameter_value(session.document(),"path","imported-instance-1","macro.offset.amount")==12,
        "Fresh Document DefinitionID copies all source graphs and explicit pin 1 evaluates its authored Amount 12");
    const auto& receipt=first.at("result").as_object().at("applied_library_macros").as_array().front().as_object();
    check(receipt.at("source_definition_id").as_string()==source.id&&
        boost::json::value_to<std::uint64_t>(receipt.at("accepted_revision"))==41&&
        boost::json::value_to<std::uint64_t>(receipt.at("pinned_revision"))==1&&receipt.at("definition_present_after_batch").as_bool()&&
        receipt.at("instance_present_after_batch").as_bool(),
        "Import receipt distinguishes caller asset accepted revision from the explicit retained graph pin");
    const Ref first_amount=macro_parameter_ref("path","imported-instance-1","macro.offset.amount");
    check(find_property(api(session,{{"op","properties"}}).at("result").as_array(),first_amount)
        .at("evaluated").as_double()==12,"Imported public Amount uses its stable PublicParamID");
    const auto imported_shape=bounds(evaluated(session.document()));
    same_bounds(imported_shape,Bounds{-12,-12,237,72},
        "Pinned revision 1 has fixed Offset 12, Repeater copies 2 and x=125 bounds");
    Session ordinary(fixture());
    apply(ordinary,{AddOperation{"path",source.revisions.at(1).nodes[0].operation,1},
        AddOperation{"path",source.revisions.at(1).nodes[1].operation,2}});
    same_bounds(imported_shape,bounds(evaluated(ordinary.document())),
        "Imported revision 1 retains the Offset 12, Repeater copies 2, and x=125 geometry oracle");
    session.undo(session.revision());
    check(!session.document().macro_definitions.contains("imported-definition-1")&&
        session.document().objects.at("path").stack.size()==1,
        "One Undo removes both the imported definition and processing instance");
    session.redo(session.revision());
    check(session.document().macro_definitions.contains("imported-definition-1")&&
        session.document().objects.at("path").stack[1].id=="imported-instance-1",
        "Redo restores both imported identities together");

    auto second=api(session,{{"op","apply"},{"expected_revision",session.revision()},
        {"commands",boost::json::array{imported_macro_command(source,"imported-definition-2","imported-instance-2",2,2)}}});
    check(second.at("ok").as_bool()&&
        macro_parameter_value(session.document(),"path","imported-instance-2","macro.offset.amount")==18&&
        session.document().objects.at("path").stack.back().id=="imported-instance-2",
        "A second independent import can pin retained revision 2 at Amount 18");
    Session pin2_only(fixture());
    auto pin2_import=api(pin2_only,{{"op","apply"},{"expected_revision",pin2_only.revision()},
        {"commands",boost::json::array{imported_macro_command(source,"pin2-definition","pin2-instance",2,1)}}});
    check(pin2_import.at("ok").as_bool(),"Retained revision 2 imports into a fresh target");
    same_bounds(bounds(evaluated(pin2_only.document())),Bounds{-18,-18,243,78},
        "Pinned revision 2 has fixed Offset 18, Repeater copies 2 and x=125 bounds");
    auto third=api(session,{{"op","apply"},{"expected_revision",session.revision()},
        {"commands",boost::json::array{imported_macro_command(source,"imported-definition-3","imported-instance-3",3,3)}}});
    check(third.at("ok").as_bool()&&session.document().objects.at("path").stack[3].macro->pinned_revision==3&&
        session.document().objects.at("path").stack[3].id=="imported-instance-3",
        "A third import explicitly pins retained latest revision 3 after older pinned instances");
    Session latest_only(fixture());
    auto latest_import=api(latest_only,{{"op","apply"},{"expected_revision",latest_only.revision()},
        {"commands",boost::json::array{imported_macro_command(source,"latest-definition","latest-instance",3,1)}}});
    check(latest_import.at("ok").as_bool(),"Latest retained Macro pin imports successfully");
    same_bounds(bounds(evaluated(latest_only.document())),Bounds{-24,-24,249,84},
        "Latest retained pin has fixed Offset 24, Repeater copies 2 and x=125 bounds");
    check(session.document().macro_definitions.at("imported-definition-1").id!=
        session.document().macro_definitions.at("imported-definition-2").id&&
        session.document().objects.at("path").stack[1].id!=session.document().objects.at("path").stack[2].id,
        "Independent imports retain distinct fresh Document DefinitionIDs and instance IDs");

    auto overridden=api(session,{{"op","apply"},{"expected_revision",session.revision()},
        {"commands",boost::json::array{imported_macro_command(source,"imported-definition-override","imported-instance-override",1,4,
            {{"macro.offset.amount",22}})}}});
    check(overridden.at("ok").as_bool()&&
        macro_parameter_value(session.document(),"path","imported-instance-override","macro.offset.amount")==22,
        "Optional stable PublicParamID overrides are applied atomically with import");
    atomic(session,"ORPHAN_MACRO_OVERRIDE",{MacroCommand{UpdateMacroInstance{"path","imported-instance-override",3}}});
    apply(session,{MacroCommand{ResetMacroOverride{"path","imported-instance-override","macro.offset.amount"}},
        MacroCommand{UpdateMacroInstance{"path","imported-instance-override",3}}});
    check(session.document().objects.at("path").stack.back().macro->pinned_revision==3,
        "Explicit migration succeeds after an incompatible pin override is reset");

    const auto state_before_rejections=session.document();const auto revision_before_rejections=session.revision();
    auto bad=[&](boost::json::object command) {
        auto result=api(session,{{"op","apply"},{"expected_revision",session.revision()},
            {"commands",boost::json::array{std::move(command)}}});
        check(!result.at("ok").as_bool()&&session.document()==state_before_rejections&&
            session.revision()==revision_before_rejections,"Rejected imported Macro transport leaves the Session atomic");
        return result;
    };
    auto missing_pin=imported_macro_command(source,"bad-pin-definition","bad-pin-instance",5,1);
    check(bad(std::move(missing_pin)).at("error").as_object().at("code").as_string()=="MISSING_MACRO_REVISION",
        "Import refuses a pin not retained in the copied Macro");
    auto wrong_public=imported_macro_command(source,"bad-public-definition","bad-public-instance",1,1,
        {{"amount",10}});
    check(bad(std::move(wrong_public)).at("error").as_object().at("code").as_string()=="MISSING_MACRO_PARAMETER",
        "Import overrides must name the exact stable PublicParamID");
    auto definition_collision=imported_macro_command(source,source.id,"bad-collision-instance",1,1);
    check(bad(std::move(definition_collision)).at("error").as_object().at("code").as_string()=="MACRO_ASSET_ID_MISMATCH",
        "Imported Document DefinitionID must be fresh and distinct from the source ID");
    auto asset_collision=imported_macro_command(source,"fresh-collision-definition","bad-asset-collision-instance",1,1);
    asset_collision["definition_id"]="workspace-asset";
    check(bad(std::move(asset_collision)).at("error").as_object().at("code").as_string()=="MACRO_ASSET_ID_MISMATCH",
        "Imported Document DefinitionID must be distinct from the workspace AssetID");
    auto nonobject_overrides=imported_macro_command(source,"bad-overrides-definition","bad-overrides-instance",1,1);
    nonobject_overrides["overrides"]=boost::json::array{12};
    check(bad(std::move(nonobject_overrides)).at("error").as_object().at("code").as_string()=="INVALID_MACRO_OVERRIDES",
        "Present overrides must be an object of numeric stable PublicParamID values");
    auto fractional_pin=imported_macro_command(source,"bad-fraction-definition","bad-fraction-instance",1,1);
    fractional_pin["pinned_revision"]=1.5;
    check(bad(std::move(fractional_pin)).at("error").as_object().at("code").as_string()=="INVALID_MACRO_COMMAND",
        "Imported retained revision requires an exact JSON integer");
    auto negative_index=imported_macro_command(source,"bad-index-definition","bad-index-instance",1,1);
    negative_index["index"]=-1;
    check(bad(std::move(negative_index)).at("error").as_object().at("code").as_string()=="INVALID_MACRO_COMMAND",
        "Imported insertion index requires a nonnegative exact JSON integer");
    auto fractional_accepted=imported_macro_command(source,"bad-accepted-definition","bad-accepted-instance",1,1);
    fractional_accepted["accepted_revision"]=1.25;
    check(bad(std::move(fractional_accepted)).at("error").as_object().at("code").as_string()=="INVALID_MACRO_COMMAND",
        "Caller-supplied accepted asset revision requires an exact JSON integer");

    const auto before_late_failure=session.document();const auto revision_late_failure=session.revision();
    auto late_failure=api(session,{{"op","apply"},{"expected_revision",session.revision()},
        {"commands",boost::json::array{imported_macro_command(source,"batch-rollback-definition","batch-rollback-instance",1,5),
            boost::json::object{{"type","delete_macro_definition"},{"definition","missing-definition"}}}}});
    check(!late_failure.at("ok").as_bool()&&session.document()==before_late_failure&&session.revision()==revision_late_failure,
        "A later invalid batch command rolls back the import definition and instance together");

    auto completed_then_removed=api(session,{{"op","apply"},{"expected_revision",session.revision()},
        {"commands",boost::json::array{imported_macro_command(source,"gone-definition","gone-instance",1,5),
            boost::json::object{{"type","detach_macro_instance"},{"object","path"},{"instance","gone-instance"},{"operation_id_prefix","gone"}},
            boost::json::object{{"type","delete_macro_definition"},{"definition","gone-definition"}}}}});
    check(completed_then_removed.at("ok").as_bool()&&completed_then_removed.at("result").as_object()
        .at("applied_library_macros").as_array().front().as_object()
        .at("definition_present_after_batch").as_bool()==false&&
        completed_then_removed.at("result").as_object().at("applied_library_macros").as_array().front().as_object()
        .at("instance_present_after_batch").as_bool()==false,
        "A successful import/detach/delete batch builds its receipt without assuming imported state survives");

    auto bad_raw_command=boost::json::serialize(imported_macro_command(source,"duplicate-key-definition","duplicate-key-instance",1,1));
    const auto latest_marker=bad_raw_command.find("\"latest_revision\":3");
    check(latest_marker!=std::string::npos,"Serialized nested Macro definition has latest_revision");
    bad_raw_command.insert(latest_marker+std::string("\"latest_revision\":3").size(),",\"latest_revision\":3");
    auto duplicate_overrides=boost::json::serialize(imported_macro_command(source,"duplicate-json-definition","duplicate-json-instance",1,99,
        {{"macro.offset.amount",7}}));
    const auto amount_entry=duplicate_overrides.find("\"macro.offset.amount\":7");
    check(amount_entry!=std::string::npos,"Serialized override fixture has a unique value");
    duplicate_overrides.insert(amount_entry+std::string("\"macro.offset.amount\":7").size(),",\"macro.offset.amount\":8");
    const auto raw_before=session.document();const auto raw_revision=session.revision();
    const auto duplicate_reply=parsed(request(session,"{\"op\":\"apply\",\"expected_revision\":"+
        std::to_string(session.revision())+",\"commands\":["+duplicate_overrides+"]}"));
    check(!duplicate_reply.at("ok").as_bool()&&session.document()==raw_before&&session.revision()==raw_revision,
        "Duplicate PublicParamID JSON keys are refused before any Session mutation");
    const auto duplicate_payload_reply=parsed(request(session,"{\"op\":\"apply\",\"expected_revision\":"+
        std::to_string(session.revision())+",\"commands\":["+bad_raw_command+"]}"));
    check(!duplicate_payload_reply.at("ok").as_bool()&&session.document()==raw_before&&session.revision()==raw_revision,
        "Malformed transport never partially applies a Macro import");
}
}

int main() {
    try {
        mixed_stack_and_property_contract();
        revisions_detach_and_native_contract();
        compatible_public_mapping_migration_contract();
        invalid_graphs_and_stale_commands_are_atomic();
        portable_import_contract();
        std::cout<<"Macro contract checks: "<<checks<<"\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<"\n";
        return 1;
    }
}
