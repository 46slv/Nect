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
    check(session.document()==document&&session.revision()==revision&&session.history()==history&&encode(session.document())==bytes,
        "Rejected portable payload/import preserves Document, native bytes, Session revision and Undo history");
}
Document fixture(bool collisions=false) {
    auto document=empty_document("portable-document","composition","artboard");
    Object path;path.id="path";path.name="Portable Macro geometry";
    Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t i=0;i<anchors.size();++i) {
        Point point;point.id="point-"+std::to_string(i+1);
        point.x.literal=anchors[i].x;point.y.literal=anchors[i].y;contour.points.push_back(point);
    }
    path.contours.push_back(contour);path.stack.emplace_back(default_operation("fill","nect.paint.fill"));
    document.objects.emplace(path.id,path);document.compositions.front().roots.push_back(path.id);
    if(collisions) {
        Object other;other.id="collision-path";other.name="Unrelated target identities";
        for(const auto* id:{"offset-first","offset-target","repeater-first","repeater-last"})
            other.stack.emplace_back(default_operation(id,"nect.shape.offset"));
        document.objects.emplace(other.id,other);document.compositions.front().roots.push_back(other.id);
    }
    return document;
}
void connect(MacroDefinitionRevision& revision) {
    MacroEndpoint cursor{"",revision.input.id};revision.edges.clear();
    for(const auto& node:revision.nodes) {
        revision.edges.push_back({cursor,{node.operation.id,node.input_port}});
        cursor={node.operation.id,node.output_port};
    }
    revision.output_mapping=cursor;revision.edges.push_back({cursor,{"",revision.output.id}});
    // Deliberately retain nonexecution node/edge storage order in the payload.
    std::reverse(revision.nodes.begin(),revision.nodes.end());std::reverse(revision.edges.begin(),revision.edges.end());
}
MacroDefinitionRevision revision(bool chain) {
    MacroDefinitionRevision result;result.graph_version=chain?2:1;
    result.input={"input","local_paths_and_paint"};result.output={"output","local_paths_and_paint"};
    const std::vector<Id> ids=chain?std::vector<Id>{"offset-first","offset-target","repeater-first","repeater-last"}:
        std::vector<Id>{"offset-first","repeater-first"};
    for(const auto& id:ids) {
        const bool offset=id.starts_with("offset");
        auto operation=default_operation(id,offset?"nect.shape.offset":"nect.shape.repeater");
        if(offset)operation.parameters.at("amount").literal=id=="offset-first"?2:3;
        else {
            operation.parameters.at("copies").literal=2;
            operation.parameters.at("position_x").literal=id=="repeater-first"?125:21;
            operation.parameters.at("position_y").literal=id=="repeater-first"?11:80;
            operation.parameters.at("rotation").literal=id=="repeater-first"?9:-13;
        }
        result.nodes.push_back({operation,id+"-in",id+"-out"});
    }
    result.public_parameters={{amount_id,"Amount",chain?"offset-target":"offset-first","amount","number","du","local_paths_and_paint"}};
    connect(result);return result;
}
MacroDefinition legacy_definition() {
    MacroDefinition result;result.id="source-definition";result.label="Portable legacy pair";
    result.revisions.emplace(1,revision(false));return result;
}
MacroDefinition advanced_definition(bool chain=true) {
    auto result=legacy_definition();result.label="Portable retained mapped chain";
    auto second=revision(chain);second.revision=2;result.revisions.emplace(2,second);
    auto third=second;third.revision=3;third.interface_version=2;
    third.public_parameters.push_back({copies_id,"Copies","repeater-first","copies","number","scalar","local_paths_and_paint"});
    third.public_parameters.push_back({rotation_id,"Rotation",chain?"repeater-last":"repeater-first","rotation","number","degree","local_paths_and_paint"});
    result.revisions.emplace(3,third);result.latest_revision=3;return result;
}
MacroNode& node(MacroDefinitionRevision& revision,const Id& id) {
    const auto found=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& item){return item.operation.id==id;});
    if(found==revision.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
const MacroNode& node(const MacroDefinitionRevision& revision,const Id& id) {
    return node(const_cast<MacroDefinitionRevision&>(revision),id);
}
EvaluatedShape evaluated(const Document& document) {return evaluate_shape(document,"path",evaluate(document));}
Document ordinary(const MacroDefinitionRevision& source,const Overrides& values={}) {
    Session session(fixture());std::vector<Command> commands;std::size_t index=1;
    // Independent oracle uses explicit fixture IDs, never Macro graph traversal.
    const std::vector<Id> ids=source.graph_version==2?std::vector<Id>{"offset-first","offset-target","repeater-first","repeater-last"}:
        std::vector<Id>{"offset-first","repeater-first"};
    for(const auto& id:ids) {
        auto operation=node(source,id).operation;
        if(values.contains(amount_id)&&id==(source.graph_version==2?"offset-target":"offset-first"))
            operation.parameters.at("amount").literal=values.at(amount_id);
        if(values.contains(copies_id)&&id=="repeater-first")operation.parameters.at("copies").literal=values.at(copies_id);
        if(values.contains(rotation_id)&&id==(source.graph_version==2?"repeater-last":"repeater-first"))
            operation.parameters.at("rotation").literal=values.at(rotation_id);
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
void same_shape(const EvaluatedShape& a,const EvaluatedShape& b,const std::string& reason) {
    check(same_paths(a.paths,b.paths),reason+": complete cubic geometry");
    check(a.paints.size()==b.paints.size(),reason+": paint count");
    for(std::size_t i=0;i<a.paints.size();++i) {
        const auto& x=a.paints[i];const auto& y=b.paints[i];
        check(x.type==y.type&&x.rgba==y.rgba&&x.fill_rule==y.fill_rule&&x.transform==y.transform&&same_paths(x.paths,y.paths),
            reason+": painted cubic geometry");
    }
}
Command import(const MacroDefinition& source,std::uint64_t pin=3,Overrides values={},
    const Id& definition_id="imported-definition",const Id& instance_id="imported-instance") {
    return MacroCommand{InstantiateMacro{"path",definition_id,instance_id,pin,1,source,"portable-asset",41,std::move(values)}};
}
boost::json::object parsed(const std::string& text) {return boost::json::parse(text).as_object();}
void codec_schema_contract() {
    const auto legacy=legacy_definition();const auto legacy_bytes=canonical_macro_payload(legacy,1);
    // Lock legacy numeric spelling, default-field omission and authored array order.
    constexpr const char* legacy_golden=
        R"json({"id":"source-definition","label":"Portable legacy pair","latest_revision":1,"revisions":[{"edges":[{"from":{"node":"repeater-first",)json"
        R"json("port":"repeater-first-out"},"to":{"node":"","port":"output"}},{"from":{"node":"offset-first","port":"offset-first-out"},)json"
        R"json("to":{"node":"repeater-first","port":"repeater-first-in"}},{"from":{"node":"","port":"input"},"to":{"node":"offset-first",)json"
        R"json("port":"offset-first-in"}}],"input":{"domain":"local_paths_and_paint","id":"input"},"nodes":[{"input_port":"repeater-first-in",)json"
        R"json("operation":{"composite":"below","enabled":true,"fill_rule":"nonzero","id":"repeater-first","parameters":{"anchor_x":{"literal":0E0},)json"
        R"json("anchor_y":{"literal":0E0},"copies":{"literal":2E0},"end_opacity":{"literal":1E0},"offset":{"literal":0E0},"position_x":{"literal":1.25E2},)json"
        R"json("position_y":{"literal":1.1E1},"rotation":{"literal":9E0},"scale_x":{"literal":1E0},"scale_y":{"literal":1E0},"start_opacity":{"literal":1E0}},)json"
        R"json("type":"nect.shape.repeater","version":1},"output_port":"repeater-first-out"},{"input_port":"offset-first-in","operation":{"composite":"below",)json"
        R"json("enabled":true,"fill_rule":"nonzero","id":"offset-first","line_join":"miter","parameters":{"amount":{"literal":2E0},)json"
        R"json("miter_limit":{"literal":4E0}},"type":"nect.shape.offset","version":1},"output_port":"offset-first-out"}],)json"
        R"json("output":{"domain":"local_paths_and_paint","id":"output"},"output_mapping":{"node":"repeater-first","port":"repeater-first-out"},)json"
        R"json("public_parameters":[{"domain":"local_paths_and_paint","id":"macro.offset.amount","label":"Amount","node":"offset-first","parameter":"amount",)json"
        R"json("unit":"du","value_type":"number"}],"revision":1}]})json";
    check(legacy_bytes==legacy_golden,"Schema1 retains the exact legacy canonical byte snapshot");
    check(portable_macro_payload_schema(legacy)==1&&canonical_macro_payload(legacy)==legacy_bytes&&
        canonical_macro_payload(legacy,2)==legacy_bytes,"Legacy payload keeps identical bytes under auto, explicit schema1 and schema2");
    validate_portable_macro_definition(legacy);validate_portable_macro_definition(legacy,2);
    check(read_canonical_macro_payload(legacy_bytes)==legacy&&read_canonical_macro_payload(legacy_bytes,1)==legacy&&
        read_canonical_macro_payload(legacy_bytes,2)==legacy,"Legacy schema1 payload retains every authored value and graph-local identity");
    const auto legacy_json=parsed(legacy_bytes);const auto& old=legacy_json.at("revisions").as_array().front().as_object();
    check(!old.contains("graph_version")&&!old.contains("interface_version")&&!legacy_json.contains("payload_schema"),
        "Schema1 omission contract and standalone payload shape remain unchanged");
    for(const bool chain:{false,true}) {
        const auto source=advanced_definition(chain);const auto bytes=canonical_macro_payload(source);
        check(portable_macro_payload_schema(source)==2&&bytes==canonical_macro_payload(source,2)&&bytes==canonical_macro_payload(source,0),
            "Auto chooses schema2 for graph2 or interface2 across all retained revisions");
        validate_portable_macro_definition(source,2);
        check(read_canonical_macro_payload(bytes)==source&&read_canonical_macro_payload(bytes,2)==source&&
            canonical_macro_payload(read_canonical_macro_payload(bytes,2),2)==bytes,
            "Schema2 roundtrip has stable canonical bytes, retained revisions, reversed storage and public mappings");
        rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{validate_portable_macro_definition(source);});
        rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{validate_portable_macro_definition(source,1);});
        rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(source,1);});
        rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)read_canonical_macro_payload(bytes,1);});
        auto graph_only=source;graph_only.revisions.erase(3);graph_only.latest_revision=2;
        check(portable_macro_payload_schema(graph_only)==(chain?2U:1U),"Graph2 requires schema2 without a published interface2");
        auto retained=source;auto latest=legacy.revisions.at(1);latest.revision=4;
        retained.revisions.emplace(4,latest);retained.latest_revision=4;
        check(portable_macro_payload_schema(retained)==2&&read_canonical_macro_payload(canonical_macro_payload(retained),2)==retained,
            "An advanced retained revision still requires schema2 after the latest revision returns to legacy semantics");
        rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(retained,1);});
    }
    const auto source=advanced_definition();const auto bytes=canonical_macro_payload(source);
    for(const unsigned future:{4U,std::numeric_limits<unsigned>::max()}) {
        rejects("UNSUPPORTED_MACRO_SCHEMA",[&]{validate_portable_macro_definition(source,future);});
        rejects("UNSUPPORTED_MACRO_SCHEMA",[&]{(void)canonical_macro_payload(source,future);});
        rejects("UNSUPPORTED_MACRO_SCHEMA",[&]{(void)read_canonical_macro_payload(bytes,future);});
    }
    rejects("UNSUPPORTED_MACRO_SCHEMA",[&]{validate_portable_macro_definition(source,0);});
    const auto json=parsed(bytes);const auto& second=json.at("revisions").as_array()[1].as_object();
    const auto& third=json.at("revisions").as_array()[2].as_object();
    check(boost::json::value_to<unsigned>(second.at("graph_version"))==2&&!second.contains("interface_version")&&
        boost::json::value_to<unsigned>(third.at("graph_version"))==2&&boost::json::value_to<unsigned>(third.at("interface_version"))==2,
        "Schema2 writes only required native086 graph/interface markers with no native-version change");
}
void malformed_and_atomic_contract() {
    const auto source=advanced_definition();const auto bytes=canonical_macro_payload(source);Session session(fixture());
    unchanged(session,"NONCANONICAL_MACRO_PAYLOAD",[&]{(void)read_canonical_macro_payload(bytes+"\n",2);});
    unchanged(session,"UNAVAILABLE_MACRO_ASSET",[&]{(void)read_canonical_macro_payload("{",2);});
    auto duplicate=bytes;const auto marker=duplicate.find("\"latest_revision\":3");
    check(marker!=std::string::npos,"Canonical duplicate-key fixture contains latest_revision");
    duplicate.insert(marker,"\"latest_revision\":3,");
    unchanged(session,"UNAVAILABLE_MACRO_ASSET",[&]{(void)read_canonical_macro_payload(duplicate,2);});
    unchanged(session,"MACRO_PAYLOAD_LIMIT",[&]{(void)read_canonical_macro_payload(std::string(portable_macro_payload_limit+1,' '),2);});
    auto oversized=source;
    for(std::uint64_t number=4;number<=128;++number) {
        auto retained=source.revisions.at(3);retained.revision=number;oversized.revisions.emplace(number,std::move(retained));
    }
    oversized.latest_revision=128;
    unchanged(session,"MACRO_PAYLOAD_LIMIT",[&]{(void)canonical_macro_payload(oversized,2);});
    const auto refuse=[&](MacroDefinition invalid,const char* code) {
        unchanged(session,code,[&]{apply(session,{import(invalid)});});
    };
    auto invalid=source;invalid.revisions.at(2).graph_version=3;refuse(invalid,"UNSUPPORTED_MACRO_GRAPH_VERSION");
    invalid=source;invalid.revisions.at(3).interface_version=4;refuse(invalid,"UNSUPPORTED_MACRO_INTERFACE_VERSION");
    invalid=source;node(invalid.revisions.at(3),"offset-first").operation.type="nect.shape.future";refuse(invalid,"UNSUPPORTED_MACRO_NODE");
    invalid=source;node(invalid.revisions.at(3),"offset-first").operation.version=2;refuse(invalid,"UNSUPPORTED_MACRO_NODE_VERSION");
    invalid=source;node(invalid.revisions.at(2),"offset-first").operation.parameters.at("amount").expression=Expression{"2",1};
    refuse(invalid,"INVALID_MACRO_NODE");
    invalid=source;invalid.revisions.at(3).public_parameters[1].node="missing-node";refuse(invalid,"INVALID_MACRO_MAPPING");
    invalid=source;invalid.revisions.at(3).public_parameters[1].unit="du";refuse(invalid,"INVALID_MACRO_MAPPING");
    invalid=source;invalid.revisions.at(3).output_mapping.node="offset-first";refuse(invalid,"INVALID_MACRO_GRAPH");
    invalid=source;invalid.revisions.at(2).edges.push_back(invalid.revisions.at(2).edges.back());refuse(invalid,"INVALID_MACRO_GRAPH");
    invalid=source;invalid.revisions.at(2).input.domain="local_paths";refuse(invalid,"INVALID_MACRO_DOMAIN");
    invalid=source;invalid.latest_revision=7;refuse(invalid,"MISSING_MACRO_REVISION");
    // A malformed retained revision is refused even when a valid older revision is pinned.
    invalid=source;invalid.revisions.at(3).public_parameters[1].node="missing-node";
    unchanged(session,"INVALID_MACRO_MAPPING",[&]{apply(session,{import(invalid,1)});});
    auto payload=parsed(bytes);payload.at("revisions").as_array()[2].as_object()["interface_version"]=4;
    unchanged(session,"UNAVAILABLE_MACRO_ASSET",[&]{(void)read_canonical_macro_payload(boost::json::serialize(payload),2);});
    payload=parsed(bytes);payload.at("revisions").as_array()[1].as_object()["graph_version"]=3;
    unchanged(session,"UNAVAILABLE_MACRO_ASSET",[&]{(void)read_canonical_macro_payload(boost::json::serialize(payload),2);});
    unchanged(session,"MISSING_MACRO_DEFINITION",[&]{apply(session,{import(source),MacroCommand{DeleteMacroDefinition{"missing-definition"}}});});
    unchanged(session,"MISSING_MACRO_REVISION",[&]{apply(session,{import(source,99)});});
    unchanged(session,"MISSING_MACRO_PARAMETER",[&]{apply(session,{import(source,3,{{"macro.missing",1}})});});
    unchanged(session,"MACRO_ASSET_ID_MISMATCH",[&]{apply(session,{import(source,3,{},source.id)});});
    unchanged(session,"MACRO_INSTANCE_ID_MISMATCH",[&]{apply(session,{import(source,3,{},"fresh-definition",source.id)});});
}
void mapped_import_ranges() {
    const auto source=advanced_definition();
    for(const auto& [id,value]:std::array<std::pair<const char*,double>,6>{{{amount_id,-1e6},{amount_id,1e6},
        {copies_id,0},{copies_id,1000},{rotation_id,-1e9},{rotation_id,1e9}}}) {
        Session session(fixture());apply(session,{import(source,3,{{id,value}})});
        check(macro_parameter_value(session.document(),"path","imported-instance",id)==value,
            "Typed schema2 import uses mapping-specific inclusive Amount, Copies and Rotation limits");
    }
    Session session(fixture());
    for(const auto& [id,value]:std::array<std::pair<const char*,double>,8>{{{amount_id,-1e6-1},{amount_id,1e6+1},
        {copies_id,-1},{copies_id,0.5},{copies_id,1001},{copies_id,1000.5},{rotation_id,-1e9-1},{rotation_id,1e9+1}}})
        unchanged(session,"OUT_OF_RANGE",[&]{apply(session,{import(source,3,{{id,value}})});});
    unchanged(session,"NON_FINITE",[&]{apply(session,{import(source,3,{{rotation_id,std::numeric_limits<double>::infinity()}})});});
    unchanged(session,"NON_FINITE",[&]{apply(session,{import(source,3,{{copies_id,std::numeric_limits<double>::quiet_NaN()}})});});
}
const Overrides imported_values{{amount_id,12},{copies_id,3},{rotation_id,-450.25}};
void check_imported(const Session& session) {
    const auto source=advanced_definition();auto expected=source;expected.id="imported-definition";
    check(session.document().macro_definitions.at(expected.id)==expected,
        "Import remaps only Document DefinitionID and preserves every retained graph-local ID, edge, order and mapping");
    check(session.document().macro_definitions.at(source.id)==legacy_definition(),"Existing target definition at the source identity remains untouched");
    const auto& entry=session.document().objects.at("path").stack.at(1);const auto& instance=*entry.macro;
    check(entry.id=="imported-instance"&&instance.definition==expected.id&&instance.pinned_revision==3&&instance.overrides==imported_values,
        "Fresh processing identity, explicit retained pin and exact stable PublicParamID overrides survive import");
    for(const auto& [id,value]:imported_values)
        check(macro_parameter_value(session.document(),"path",entry.id,id)==value,"Stable public control evaluates its intended imported mapping");
    check(session.document().objects.at("collision-path")==fixture(true).objects.at("collision-path"),
        "Source graph-local IDs may collide with target operation IDs without touching unrelated authored state");
    same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(3),imported_values)),
        "Imported graph2/interface2 follows explicit ordinary operation and selective override oracle");
}
std::string quoted(const std::string& value) {
#ifdef _WIN32
    return "\""+value+"\"";
#else
    std::string result="'";for(const auto ch:value)result+=ch=='\''?"'\\''":std::string(1,ch);return result+"'";
#endif
}
void cold_read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);check(file.good(),"Fresh process can read saved native086");
    const std::string bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};Session reopened(decode(bytes));
    check_imported(reopened);check(encode(reopened.document())==bytes,"Cold native reopen preserves exact authored bytes");
}
void json_lines_import_contract() {
    const auto source=advanced_definition();Session session(fixture());
    const auto before=encode(session.document());boost::json::object values;
    for(const auto& [id,value]:imported_values)values[id]=value;
    auto command=boost::json::object{{"type","import_apply_macro"},{"definition",parsed(canonical_macro_payload(source,2))},
        {"definition_id","api-definition"},{"object","path"},{"instance","api-instance"},{"pinned_revision",3},
        {"index",1},{"asset_id","portable-asset"},{"accepted_revision",41},{"overrides",values}};
    auto reply=parsed(request(session,boost::json::serialize(boost::json::object{{"op","apply"},
        {"expected_revision",session.revision()},{"commands",boost::json::array{command}}})));
    check(reply.at("ok").as_bool(),"Shared JSON-lines import accepts graph2/interface2 and three stable numeric overrides");
    const auto& receipt=reply.at("result").as_object().at("applied_library_macros").as_array().front().as_object();
    check(receipt.at("source_definition_id").as_string()==source.id&&
        boost::json::value_to<std::uint64_t>(receipt.at("accepted_revision"))==41&&
        boost::json::value_to<std::uint64_t>(receipt.at("pinned_revision"))==3&&
        receipt.at("definition_present_after_batch").as_bool()&&receipt.at("instance_present_after_batch").as_bool(),
        "Schema2 import receipt keeps workspace accepted revision separate from the retained graph pin");
    same_shape(evaluated(session.document()),evaluated(ordinary(source.revisions.at(3),imported_values)),
        "JSON-lines import shares the mapped typed geometry semantics");
    session.undo(session.revision());check(encode(session.document())==before,"JSON-lines mapped import is one exact Undo");
    const auto state=session.document();const auto revision=session.revision();const auto history=session.history();
    command.at("definition").as_object().at("revisions").as_array()[2].as_object()["interface_version"]=4;
    reply=parsed(request(session,boost::json::serialize(boost::json::object{{"op","apply"},
        {"expected_revision",session.revision()},{"commands",boost::json::array{command}}})));
    check(!reply.at("ok").as_bool()&&session.document()==state&&session.revision()==revision&&
        session.history()==history&&encode(session.document())==before,
        "Unsupported retained interface transport refuses before any shared Session mutation");
}
void typed_import_undo_and_cold_reopen(const std::string& executable) {
    const auto source=read_canonical_macro_payload(canonical_macro_payload(advanced_definition(),2),2);
    // Each retained explicit pin imports independently into a fresh Document.
    for(const auto pin:{1U,2U,3U}) {
        Session fresh(fixture());const Overrides values=pin==3?imported_values:Overrides{{amount_id,12}};
        apply(fresh,{import(source,pin,values)});
        check(fresh.document().objects.at("path").stack[1].macro->pinned_revision==pin&&
            fresh.document().macro_definitions.at("imported-definition").revisions==source.revisions,
            "Fresh-document import retains all revisions while selecting the explicit graph/interface pin");
        same_shape(evaluated(fresh.document()),evaluated(ordinary(source.revisions.at(pin),values)),"Each explicit imported pin matches ordinary geometry");
    }
    Session session(fixture(true));apply(session,{MacroCommand{CreateMacroDefinition{legacy_definition()}}});
    const auto before=session.document();const auto before_bytes=encode(before);const auto before_revision=session.revision();
    const auto before_history=session.history();apply(session,{import(source,3,imported_values)});
    check(session.revision()==before_revision+1&&session.history().states.size()==before_history.states.size()+1,
        "Definition import, instance and three public overrides commit as one Session revision and one Undo entry");
    check_imported(session);const auto imported_bytes=encode(session.document());
    session.undo(session.revision());check(session.document()==before&&encode(session.document())==before_bytes,
        "One Undo removes imported definition, instance and overrides and restores the complete colliding target");
    session.redo(session.revision());check(encode(session.document())==imported_bytes,"One Redo restores exact imported native bytes");
    check_imported(session);
    check(parsed(imported_bytes).at("version").as_string()==native_version&&
        decode(imported_bytes)==session.document(),"Portable schema2 import keeps native086 semantics and authored roundtrip");
    const auto path=std::filesystem::temp_directory_path()/("nect-portable-macro-v2-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".nect");
    {std::ofstream file(path,std::ios::binary);file<<imported_bytes;check(file.good(),"Imported native086 saved before second-process replay");}
#ifdef _WIN32
    const auto result=_spawnl(_P_WAIT,executable.c_str(),quoted(executable).c_str(),"--cold-read",quoted(path.string()).c_str(),static_cast<const char*>(nullptr));
#else
    const auto result=std::system((quoted(executable)+" --cold-read "+quoted(path.string())).c_str());
#endif
    std::filesystem::remove(path);check(result==0,"Independent cold process recomputes the imported graph/interface geometry");
}
}
int main(int argc,char** argv) {
    const char* stage="cold reopen";
    try {
        if(argc==3&&std::string(argv[1])=="--cold-read") {cold_read(argv[2]);std::cout<<"Portable Macro v2 cold checks: "<<checks<<"\n";return 0;}
        stage="portable codec";codec_schema_contract();
        stage="atomic refusals";malformed_and_atomic_contract();
        stage="mapped import ranges";mapped_import_ranges();
        stage="JSON-lines import";json_lines_import_contract();
        stage="typed import and cold reopen";typed_import_undo_and_cold_reopen(std::filesystem::absolute(argv[0]).string());
        std::cout<<"Portable Macro v2 checks: "<<checks<<"\n";return 0;
    } catch(const std::exception& error) {std::cerr<<stage<<": "<<error.what()<<"\n";return 1;}
}
