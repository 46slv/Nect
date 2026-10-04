#include "nect/io.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace nect;
namespace j=boost::json;
namespace {
int checks=0;
void check(bool ok,const std::string& why) {if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action) {
    try {action();}catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", received "+error.code+": "+error.what());return;
    }
    throw std::runtime_error("Expected "+std::string(code));
}
Document fixture() {
    auto document=empty_document("closure-document","composition","artboard");
    Object path;path.id="path";path.name="Target";path.kind=Kind::path;
    Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t i=0;i<anchors.size();++i) {
        Point point;point.id="point-"+std::to_string(i);point.x.literal=anchors[i].x;point.y.literal=anchors[i].y;
        contour.points.push_back(point);
    }
    path.contours.push_back(contour);
    document.objects.emplace(path.id,path);document.compositions.front().roots.push_back(path.id);
    return document;
}
void connect(MacroDefinitionRevision& revision) {
    MacroEndpoint cursor{"",revision.input.id};revision.edges.clear();
    for(const auto& node:revision.nodes) {
        revision.edges.push_back({cursor,{node.operation.id,node.input_port}});
        cursor={node.operation.id,node.output_port};
    }
    revision.output_mapping=cursor;revision.edges.push_back({cursor,{"",revision.output.id}});
    std::reverse(revision.nodes.begin(),revision.nodes.end());
    std::reverse(revision.edges.begin(),revision.edges.end());
}
MacroDefinition macro() {
    MacroDefinition result;result.id="source-macro";result.label="Retained reusable graph";result.latest_revision=3;
    MacroDefinitionRevision first;first.input={"input","local_paths_and_paint"};first.output={"output","local_paths_and_paint"};
    auto offset=default_operation("offset-node","nect.shape.offset");offset.parameters.at("amount").literal=4;
    auto repeater=default_operation("repeat-node","nect.shape.repeater");
    repeater.parameters.at("copies").literal=2;repeater.parameters.at("position_x").literal=120;
    first.nodes={{offset,"offset-in","offset-out"},{repeater,"repeat-in","repeat-out"}};
    first.public_parameters={{"macro.offset.amount","Amount","offset-node","amount","number","du","local_paths_and_paint"}};
    connect(first);result.revisions.emplace(1,first);
    auto second=first;second.revision=2;second.graph_version=2;second.interface_version=2;
    // Storage order differs from execution order and must survive unchanged.
    std::reverse(second.nodes.begin(),second.nodes.end());
    auto last=default_operation("last-offset","nect.shape.offset");last.parameters.at("amount").literal=2;
    second.nodes.push_back({last,"last-in","last-out"});
    second.public_parameters.push_back({"macro.copies","Copies","repeat-node","copies","number","scalar","local_paths_and_paint"});
    connect(second);result.revisions.emplace(2,second);
    auto third=second;third.revision=3;third.interface_version=3;
    third.public_parameters.push_back({"macro.repeat.enabled","Repeat enabled","repeat-node","enabled","boolean","boolean","local_paths_and_paint"});
    result.revisions.emplace(3,third);return result;
}
PresetEntry builtin(const ShapeOperation& operation) {
    PresetEntry entry;entry.type=operation.type;entry.version=operation.version;entry.enabled=operation.enabled;
    for(const auto& [key,value]:operation.parameters)entry.parameters.emplace(key,value.literal);
    entry.composite=operation.composite;entry.fill_rule=operation.fill_rule;
    entry.line_join=operation.line_join;entry.line_cap=operation.line_cap;return entry;
}
PortablePresetClosure closure() {
    PortablePresetClosure result;result.definition.id="source-preset";result.definition.label="Mixed pinned recipe";
    result.definition.schema_version=2;result.definition.category="Shape";result.definition.tags={"reusable"};
    PresetEntry early;early.kind="macro";early.type=macro_entry_type;early.macro_definition="source-macro";
    early.pinned_revision=1;early.overrides={{"macro.offset.amount",7}};early.enabled=false;
    auto late=early;late.pinned_revision=3;late.overrides={{"macro.offset.amount",11},{"macro.copies",3}};
    late.boolean_overrides={{"macro.repeat.enabled",false}};late.enabled=true;
    result.definition.entries={builtin(default_operation("template-offset","nect.shape.offset")),early,late,
        builtin(default_operation("template-fill","nect.paint.fill"))};
    result.macro_definitions.emplace("source-macro",macro());return result;
}
ImportAndApplyPresetClosure import(const PortablePresetClosure& source) {
    return {source,"imported-preset","path","applied","workspace-asset",41,{{"source-macro","imported-macro"}}};
}
void apply(Session& session,const ImportAndApplyPresetClosure& command) {
    apply_serializable_preset(session,PresetCommand{command},session.revision());
}
void expect_atomic(Session& session,const char* code,const ImportAndApplyPresetClosure& command) {
    const auto before=session.document();const auto bytes=encode(before);const auto revision=session.revision();const auto history=session.history();
    rejects(code,[&]{apply(session,command);});
    check(session.document()==before&&encode(session.document())==bytes&&session.revision()==revision&&session.history()==history,
        "Refusal preserves Document, native bytes, revision and complete Undo/Redo history");
}
bool near(Vec2 a,Vec2 b) {return std::abs(a.x-b.x)<1e-7&&std::abs(a.y-b.y)<1e-7;}
bool same_paths(const std::vector<PathInstance>& left,const std::vector<PathInstance>& right) {
    if(left.size()!=right.size())return false;
    for(std::size_t i=0;i<left.size();++i) {
        const auto& a=*left[i].contours;const auto& b=*right[i].contours;
        if(a.size()!=b.size())return false;
        for(std::size_t c=0;c<a.size();++c) {
            if(a[c].closed!=b[c].closed||a[c].points.size()!=b[c].points.size())return false;
            for(std::size_t p=0;p<a[c].points.size();++p) {
                const auto& x=a[c].points[p];const auto& y=b[c].points[p];
                if(!near(map_point(left[i].transform,x.anchor),map_point(right[i].transform,y.anchor))||
                    !near(map_point(left[i].transform,x.incoming),map_point(right[i].transform,y.incoming))||
                    !near(map_point(left[i].transform,x.outgoing),map_point(right[i].transform,y.outgoing)))return false;
            }
        }
    }
    return true;
}
void same_shape(const Document& a,const Document& b) {
    const auto left=evaluate_shape(a,"path",evaluate(a));const auto right=evaluate_shape(b,"path",evaluate(b));
    check(same_paths(left.paths,right.paths),"Closure import preserves complete evaluated cubic geometry");
    check(left.paints.size()==right.paints.size(),"Closure import preserves paint count");
    for(std::size_t i=0;i<left.paints.size();++i) {
        const auto& x=left.paints[i];const auto& y=right.paints[i];
        check(x.type==y.type&&x.rgba==y.rgba&&x.width==y.width&&x.fill_rule==y.fill_rule&&same_paths(x.paths,y.paths),
            "Closure import preserves complete painted cubic geometry");
    }
}
void roundtrip_and_atomic_import() {
    auto portable=closure();auto source=fixture();source.macro_definitions=portable.macro_definitions;
    source.preset_definitions.emplace(portable.definition.id,portable.definition);
    const auto before_source=encode(source);
    check(capture_portable_preset_closure(source,"source-preset")==portable,
        "Pure named Preset capture copies each repeated dependency once with all retained revisions");
    const auto payload=canonical_preset_closure_payload(portable);
    check(portable_preset_closure_schema(portable)==3&&read_canonical_preset_closure_payload(payload,3)==portable&&
        read_canonical_preset_closure_payload(payload)==portable,"Schema3 canonical and inferred decode preserve exact closure");
    rejects("UNKNOWN_FIELD",[&]{(void)read_canonical_preset_payload(payload);});
    Session baseline(source);baseline.apply_preset_command(PresetCommand{ApplyPreset{"source-preset","path","baseline"}},0);
    Session target(fixture());const auto empty=target.document();const auto history=target.history();apply(target,import(portable));
    const auto imported=target.document();
    check(target.revision()==1&&target.history().states.size()==history.states.size()+1,
        "Dependencies, Preset and applications enter one Session revision and one Undo state");
    auto expected_macro=portable.macro_definitions.at("source-macro");expected_macro.id="imported-macro";
    check(imported.macro_definitions.size()==1&&imported.macro_definitions.at("imported-macro")==expected_macro,
        "Only outer Macro identity changes; latest, all revisions, graph-local IDs and storage order remain exact");
    auto expected_preset=portable.definition;expected_preset.id="imported-preset";
    for(auto& entry:expected_preset.entries)if(entry.kind=="macro")entry.macro_definition="imported-macro";
    check(imported.preset_definitions.at("imported-preset")==expected_preset,
        "Only Preset identity and outer Macro references change, preserving pins and typed overrides");
    const auto& stack=imported.objects.at("path").stack;
    check(stack.size()==4&&stack[1].macro->pinned_revision==1&&stack[2].macro->pinned_revision==3&&
        !stack[1].enabled&&stack[2].enabled&&!stack[2].macro->boolean_overrides.at("macro.repeat.enabled"),
        "Repeated dependency applications retain different pins, enabled literals and boolean overrides");
    check(macro_parameter_value(imported,"path","applied-op-2","macro.offset.amount")==7&&
        macro_parameter_value(imported,"path","applied-op-3","macro.copies")==3&&
        !macro_parameter_boolean_value(imported,"path","applied-op-3","macro.repeat.enabled"),"Mapped numeric/boolean public values survive import");
    same_shape(baseline.document(),imported);
    check(decode(encode(imported))==imported,"Native0.87 roundtrip preserves imported closure state");
    check(encode(source)==before_source&&canonical_preset_closure_payload(portable)==payload,"Capture/import leaves source Document and canonical bytes unchanged");
    target.undo(target.revision());check(target.document()==empty,"One Undo removes every imported definition and application");
    target.redo(target.revision());check(target.document()==imported,"One Redo restores all exact definition IDs, application IDs and pins");
}
void legacy_bytes_and_strict_reader() {
    PresetDefinition legacy;legacy.id="legacy";legacy.label="Literal pair";
    legacy.entries={builtin(default_operation("offset","nect.shape.offset")),builtin(default_operation("repeat","nect.shape.repeater"))};
    for(unsigned schema:{1u,2u}) {
        legacy.schema_version=schema;
        const auto bytes=canonical_preset_payload(legacy);
        const auto decoded=read_canonical_preset_closure_payload(bytes,schema);
        check(decoded.definition==legacy&&decoded.macro_definitions.empty()&&portable_preset_closure_schema(decoded)==schema&&
            canonical_preset_closure_payload(decoded)==bytes&&read_canonical_preset_payload(bytes)==legacy,
            "Legacy schema1/2 remain byte-identical through new closure reader and old strict literal reader");
        rejects("PRESET_SCHEMA_MISMATCH",[&]{(void)read_canonical_preset_closure_payload(bytes,schema==1?2:1);});
    }
    rejects("PRESET_NONPORTABLE_SOURCE",[&]{(void)canonical_preset_payload(closure().definition);});
}
void dependency_and_import_refusals() {
    const auto good=closure();Session session(fixture());
    auto bad=import(good);bad.closure.macro_definitions.clear();expect_atomic(session,"MISSING_MACRO_DEFINITION",bad);
    bad=import(good);bad.closure.definition.entries[1].pinned_revision=99;expect_atomic(session,"MISSING_MACRO_REVISION",bad);
    bad=import(good);auto unused=macro();unused.id="unused";bad.closure.macro_definitions.emplace(unused.id,unused);
    expect_atomic(session,"UNUSED_PRESET_DEPENDENCY",bad);
    bad=import(good);bad.closure.macro_definitions.begin()->second.id="wrong-key";expect_atomic(session,"ID_MISMATCH",bad);
    bad=import(good);bad.closure.definition.entries[2].overrides["macro.repeat.enabled"]=1;
    bad.closure.definition.entries[2].boolean_overrides.clear();expect_atomic(session,"INCOMPATIBLE_MACRO_PUBLIC_PARAMETER",bad);
    bad=import(good);bad.closure.macro_definitions.begin()->second.revisions.at(3).graph_version=99;
    expect_atomic(session,"UNSUPPORTED_MACRO_GRAPH_VERSION",bad);
    bad=import(good);bad.macro_definition_ids.clear();expect_atomic(session,"INVALID_PRESET_DEPENDENCY_MAPPING",bad);
    bad=import(good);bad.macro_definition_ids={{"unknown","fresh"}};expect_atomic(session,"INVALID_PRESET_DEPENDENCY_MAPPING",bad);
    bad=import(good);bad.macro_definition_ids.begin()->second="imported-preset";expect_atomic(session,"DUPLICATE_ID",bad);
    bad=import(good);auto other=macro();other.id="other-macro";
    bad.closure.macro_definitions.emplace(other.id,other);bad.closure.definition.entries[1].macro_definition=other.id;
    bad.macro_definition_ids={{"source-macro","same-import"},{"other-macro","same-import"}};
    expect_atomic(session,"DUPLICATE_ID",bad);
    for(const auto* id:{"source-macro","source-preset","workspace-asset","path","point-0","artboard"}) {
        bad=import(good);bad.macro_definition_ids.begin()->second=id;expect_atomic(session,"DUPLICATE_ID",bad);
    }
    bad=import(good);bad.document_definition_id="path";expect_atomic(session,"DUPLICATE_ID",bad);
    bad=import(good);bad.macro_definition_ids.begin()->second="applied-op-1";expect_atomic(session,"DUPLICATE_ID",bad);
    bad=import(good);bad.object="missing";expect_atomic(session,"MISSING_OBJECT",bad);
    auto full=fixture();for(unsigned i=0;i<127;++i)full.objects.at("path").stack.push_back(default_operation("old-"+std::to_string(i),"nect.shape.offset"));
    Session full_session(full);expect_atomic(full_session,"LIMIT",import(good));
    auto payload=j::parse(canonical_preset_closure_payload(good)).as_object();
    payload.at("macro_definitions").as_array().push_back(payload.at("macro_definitions").as_array().front());
    rejects("DUPLICATE_PRESET_DEPENDENCY",[&]{(void)read_canonical_preset_closure_payload(j::serialize(payload),3);});
    rejects("UNSUPPORTED_PRESET_SCHEMA",[&]{(void)read_canonical_preset_closure_payload("{}",4);});
    rejects("NONCANONICAL_PRESET_PAYLOAD",[&]{(void)read_canonical_preset_closure_payload(canonical_preset_closure_payload(good)+" ",3);});
    auto empty_wrapper=j::parse(canonical_preset_closure_payload(good)).as_object();
    auto literal=good.definition;literal.entries={good.definition.entries.front()};
    empty_wrapper["definition"]=j::parse(canonical_preset_payload(literal));empty_wrapper["macro_definitions"]=j::array{};
    rejects("PRESET_SCHEMA_MISMATCH",[&]{(void)read_canonical_preset_closure_payload(j::serialize(empty_wrapper),3);});
}
void total_closure_limit() {
    auto portable=closure();portable.definition.entries.resize(2);
    auto large=macro();large.revisions.clear();large.latest_revision=20;
    for(unsigned number=1;number<=20;++number) {
        MacroDefinitionRevision revision;revision.revision=number;revision.graph_version=2;
        revision.input={"input","local_paths_and_paint"};revision.output={"output","local_paths_and_paint"};
        for(unsigned i=0;i<16;++i)revision.nodes.push_back({default_operation("node-"+std::to_string(i),"nect.shape.offset"),
            "in-"+std::to_string(i),"out-"+std::to_string(i)});
        revision.public_parameters={{"macro.offset.amount","Amount","node-0","amount","number","du","local_paths_and_paint"}};
        connect(revision);large.revisions.emplace(number,revision);
    }
    portable.macro_definitions={{large.id,large}};
    auto second=large;second.id="second-macro";portable.macro_definitions.emplace(second.id,second);
    auto second_entry=portable.definition.entries[1];second_entry.macro_definition=second.id;portable.definition.entries.push_back(second_entry);
    check(canonical_macro_payload(large).size()<portable_macro_payload_limit&&canonical_macro_payload(second).size()<portable_macro_payload_limit,
        "Both individual Macro dependencies fit their own asset budget");
    rejects("PRESET_PAYLOAD_LIMIT",[&]{(void)canonical_preset_closure_payload(portable);});
    auto command=import(portable);command.macro_definition_ids.emplace(second.id,"second-imported");
    Session session(fixture());expect_atomic(session,"PRESET_PAYLOAD_LIMIT",command);
    rejects("PRESET_PAYLOAD_LIMIT",[&]{(void)read_canonical_preset_closure_payload(std::string(portable_preset_payload_limit+1,'x'));});
}
void json_command_receipts() {
    auto command=import(closure());
    j::object wire{{"type","import_apply_preset_closure"},{"closure",j::parse(canonical_preset_closure_payload(command.closure))},
        {"definition_id",command.document_definition_id},{"object",command.object},{"operation_id_prefix",command.operation_id_prefix},
        {"asset_id",command.asset_id},{"accepted_revision",command.accepted_revision},
        {"macro_definition_ids",j::object{{"source-macro","imported-macro"}}}};
    Session session(fixture());
    auto send=[&](const j::object& item) {return j::parse(request(session,j::serialize(j::object{{"op","apply"},
        {"expected_revision",session.revision()},{"commands",j::array{item}}}))).as_object();};
    auto unknown=wire;unknown["source_path"]="forbidden";
    const auto rejected=send(unknown);check(!rejected.at("ok").as_bool()&&session.revision()==0,"Closure wire command rejects unknown fields before mutation");
    const auto response=send(wire);check(response.at("ok").as_bool(),"Dedicated Preset JSON family accepts closure import");
    const auto& receipt=response.at("result").at("applied_library_presets").as_array().front();
    check(receipt.at("definition_id").as_string()=="imported-preset"&&j::value_to<std::uint64_t>(receipt.at("accepted_revision"))==41&&
        receipt.at("imported_macro_definitions").as_array().front().at("definition_id").as_string()=="imported-macro"&&
        j::value_to<std::uint64_t>(receipt.at("macro_pins").as_array()[0].at("pinned_revision"))==1&&
        j::value_to<std::uint64_t>(receipt.at("macro_pins").as_array()[1].at("pinned_revision"))==3,
        "Receipt reports installed outer IDs and actual per-entry Macro pins with separate accepted asset revision");
}
}
int main() {
    try {
        roundtrip_and_atomic_import();legacy_bytes_and_strict_reader();dependency_and_import_refusals();
        total_closure_limit();json_command_receipts();
        std::cout<<"portable_preset_closure_tests: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
