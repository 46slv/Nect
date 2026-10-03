#include "nect/core.hpp"
#include "nect/io.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace nect;
namespace {
int checks=0;
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
void apply(Session& session,std::vector<Command> commands) {
    session.apply(commands,session.revision());
}
void atomic(Session& session,const char* code,std::vector<Command> commands) {
    const auto document=session.document();const auto revision=session.revision();const auto history=session.history();
    const auto native=encode(document);
    rejects(code,[&]{apply(session,std::move(commands));});
    check(session.document()==document&&session.revision()==revision&&session.history()==history&&
        encode(session.document())==native,"Graph refusal preserves authored state, native bytes, revision and history");
}
Document fixture() {
    auto document=empty_document("linear-doc","composition","artboard");
    Object path;path.id="path";path.name="Linear Macro path";
    Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t i=0;i<anchors.size();++i) {
        Point point;point.id="point-"+std::to_string(i+1);
        point.x.literal=anchors[i].x;point.y.literal=anchors[i].y;
        contour.points.push_back(point);
    }
    path.contours.push_back(contour);
    path.stack.emplace_back(default_operation("fill","nect.paint.fill"));
    document.objects.emplace(path.id,path);document.compositions.front().roots.push_back(path.id);
    return document;
}
MacroDefinition chain(const std::vector<std::string>& types={
    "nect.shape.offset","nect.shape.offset","nect.shape.repeater"}) {
    MacroDefinitionRevision revision;revision.graph_version=2;
    revision.input={"input","local_paths_and_paint"};revision.output={"output","local_paths_and_paint"};
    Id published;
    for(std::size_t i=0;i<types.size();++i) {
        const auto id="node-"+std::to_string(i+1);
        auto operation=default_operation(id,types[i]);
        if(types[i]=="nect.shape.offset") {
            operation.parameters.at("amount").literal=i==0?5:3;
            published=id;
        } else {
            operation.parameters.at("copies").literal=3;
            operation.parameters.at("position_x").literal=125;
            operation.parameters.at("position_y").literal=11;
            operation.parameters.at("rotation").literal=17;
            operation.parameters.at("scale_x").literal=0.8;
            operation.parameters.at("scale_y").literal=0.8;
        }
        revision.nodes.push_back({operation,id+"-input",id+"-output"});
    }
    MacroEndpoint from{"",revision.input.id};
    for(const auto& node:revision.nodes) {
        revision.edges.push_back({from,{node.operation.id,node.input_port}});
        from={node.operation.id,node.output_port};
    }
    revision.output_mapping=from;revision.edges.push_back({from,{"",revision.output.id}});
    revision.public_parameters.push_back({"macro.offset.amount","Amount",published,"amount","number","du","local_paths_and_paint"});
    // Neither node storage nor edge storage matches the execution order.
    std::reverse(revision.nodes.begin(),revision.nodes.end());
    std::reverse(revision.edges.begin(),revision.edges.end());
    MacroDefinition definition;definition.id="linear-definition";definition.label="Linear chain";
    definition.revisions.emplace(1,std::move(revision));
    return definition;
}
const MacroNode& node(const MacroDefinitionRevision& revision,const Id& id) {
    const auto found=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& item){return item.operation.id==id;});
    if(found==revision.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
MacroNode& node(MacroDefinitionRevision& revision,const Id& id) {
    return const_cast<MacroNode&>(node(static_cast<const MacroDefinitionRevision&>(revision),id));
}
EvaluatedShape evaluated(const Document& document) {
    return evaluate_shape(document,"path",evaluate(document));
}
Document ordinary(const MacroDefinitionRevision& revision,std::optional<double> override={},bool swap_repeaters=false) {
    Session session(fixture());std::vector<Command> commands;std::size_t index=1;
    // Independent oracle: fixture identities specify the expected operation
    // sequence without invoking the production graph traversal.
    for(std::size_t i=1;i<=revision.nodes.size();++i) {
        const auto number=swap_repeaters&&i>1?5-i:i;
        auto operation=node(revision,"node-"+std::to_string(number)).operation;
        if(override&&!revision.public_parameters.empty()&&revision.public_parameters.front().node==operation.id)
            operation.parameters.at("amount").literal=*override;
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
                if(!near(map_point(a[i].transform,x.points[p].anchor),map_point(b[i].transform,y.points[p].anchor))||
                    !near(map_point(a[i].transform,x.points[p].incoming),map_point(b[i].transform,y.points[p].incoming))||
                    !near(map_point(a[i].transform,x.points[p].outgoing),map_point(b[i].transform,y.points[p].outgoing)))return false;
            }
        }
    }
    return true;
}
void same_shape(const EvaluatedShape& actual,const EvaluatedShape& expected,const char* reason) {
    check(same_paths(actual.paths,expected.paths),std::string(reason)+": full cubic geometry");
    check(actual.paints.size()==expected.paints.size(),std::string(reason)+": paint count");
    for(std::size_t i=0;i<actual.paints.size();++i) {
        const auto& a=actual.paints[i];const auto& b=expected.paints[i];
        check(a.type==b.type&&a.rgba==b.rgba&&a.fill_rule==b.fill_rule&&a.transform==b.transform&&same_paths(a.paths,b.paths),
            std::string(reason)+": painted cubic geometry");
    }
}
boost::json::object parsed(const std::string& text) {return boost::json::parse(text).as_object();}
boost::json::object& serialized_revision(boost::json::object& document) {
    return document.at("macros").as_array().front().as_object().at("revisions").as_array().front().as_object();
}
void instantiate(Session& session,const MacroDefinition& definition,std::optional<double> override={}) {
    std::vector<Command> commands{MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"path",definition.id,"linear-instance",1,1}}};
    if(override)commands.emplace_back(MacroCommand{SetMacroOverride{"path","linear-instance","macro.offset.amount",*override}});
    apply(session,std::move(commands));
}
std::string quoted(const std::string& value) {
#ifdef _WIN32
    return "\""+value+"\"";
#else
    std::string result="'";
    for(const auto ch:value)result+=ch=='\''?"'\\''":std::string(1,ch);
    return result+"'";
#endif
}
void cold_read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    check(file.good(),"Cold process can read saved native file");
    const std::string native{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    Session reopened(decode(native));
    const auto& revision=reopened.document().macro_definitions.at("linear-definition").revisions.at(1);
    check(revision.graph_version==2&&revision.nodes.front().operation.id=="node-3"&&revision.edges.front().to.node.empty(),
        "Cold native reopen preserves graph version and authored reversed node/edge storage");
    check(reopened.document().objects.at("path").stack[1].macro->pinned_revision==1&&
        macro_parameter_value(reopened.document(),"path","linear-instance","macro.offset.amount")==12,
        "Cold native reopen preserves explicit pin and public override");
    same_shape(evaluated(reopened.document()),evaluated(ordinary(chain().revisions.at(1),12)),
        "Cold process recomputes the same selective-override geometry");
    check(encode(reopened.document())==native,"Cold reopen preserves exact native authored bytes");
}
void primary_flow_and_cold_reopen(const std::string& executable) {
    Session session(fixture());const auto initial=encode(session.document());const auto definition=chain();
    const auto& revision=definition.revisions.at(1);
    const auto order=macro_execution_order(revision);
    check(order.size()==3&&order[0]->operation.id=="node-1"&&order[1]->operation.id=="node-2"&&
        order[2]->operation.id=="node-3","Stable typed edges recover the three-node chain despite reversed storage");
    instantiate(session,definition);
    same_shape(evaluated(session.document()),evaluated(ordinary(revision)),"Three-node Macro matches ordinary flattened operations");
    session.undo(session.revision());check(encode(session.document())==initial,"One Undo removes definition and instance together");
    session.redo(session.revision());
    const auto before_override=encode(session.document());
    apply(session,{MacroCommand{SetMacroOverride{"path","linear-instance","macro.offset.amount",12}}});
    const auto overridden=encode(session.document());
    same_shape(evaluated(session.document()),evaluated(ordinary(revision,12)),"Public override applies only to mapped second Offset");
    auto wrong=revision;
    node(wrong,"node-1").operation.parameters.at("amount").literal=12;
    check(!same_paths(evaluated(session.document()).paths,evaluated(ordinary(wrong,12)).paths),
        "Geometry oracle distinguishes accidentally overriding every Offset");
    check(node(session.document().macro_definitions.at(definition.id).revisions.at(1),"node-1").operation.parameters.at("amount").literal==5&&
        node(session.document().macro_definitions.at(definition.id).revisions.at(1),"node-2").operation.parameters.at("amount").literal==3,
        "Instance override leaves both authored node defaults unchanged");
    const auto response=parsed(request(session,"{\"op\":\"get\",\"ref\":{\"object\":\"path\",\"point\":\"linear-instance\",\"field\":\"macro.offset.amount\"}}"));
    check(response.at("ok").as_bool()&&boost::json::value_to<double>(response.at("result").as_object().at("evaluated"))==12,
        "Shared API reads the stable public parameter from its mapped node");
    session.undo(session.revision());check(encode(session.document())==before_override,"One Undo removes only the public override");
    session.redo(session.revision());check(encode(session.document())==overridden,"Redo restores exact override bytes");
    check(parsed(overridden).at("version").as_string()=="0.83"&&decode(overridden)==session.document(),
        "Native 0.83 roundtrips the complete graph, authored order and instance");
    const auto path=std::filesystem::temp_directory_path()/
        ("nect-macro-linear-v2-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".nect");
    {std::ofstream file(path,std::ios::binary);file<<overridden;check(file.good(),"Native file is saved before cold replay");}
    const auto result=std::system((quoted(executable)+" --cold-read "+quoted(path.string())).c_str());
    std::filesystem::remove(path);
    check(result==0,"A fresh process cold-reopens native 0.83 and verifies the full geometry oracle");
}
void noncommuting_repeater_edge_order() {
    auto definition=chain({"nect.shape.offset","nect.shape.repeater","nect.shape.repeater"});
    auto& revision=definition.revisions.at(1);
    auto& second=node(revision,"node-3").operation;
    second.parameters.at("position_x").literal=-35;
    second.parameters.at("position_y").literal=70;
    second.parameters.at("rotation").literal=-29;
    second.parameters.at("scale_x").literal=1.2;
    second.parameters.at("scale_y").literal=1.2;
    Session session(fixture());instantiate(session,definition);
    const auto shape=evaluated(session.document());
    same_shape(shape,evaluated(ordinary(revision)),"Edges execute distinct noncommuting Repeaters in authored chain order");
    check(!same_paths(shape.paths,evaluated(ordinary(revision,{},true)).paths),
        "Independent ordinary oracle distinguishes reversed Repeater execution");
}
void explicit_revisions_and_detach() {
    const auto definition=chain();Session session(fixture());instantiate(session,definition,12);
    const auto original_shape=evaluated(session.document());
    auto next=definition.revisions.at(1);next.revision=2;
    next.public_parameters.front().node="node-1";
    node(next,"node-1").operation.parameters.at("amount").literal=14;
    apply(session,{MacroCommand{UpdateMacroDefinition{definition.id,next}}});
    check(session.document().macro_definitions.at(definition.id).latest_revision==2&&
        session.document().objects.at("path").stack[1].macro->pinned_revision==1,
        "Appending revision 2 preserves the existing explicit revision 1 pin");
    same_shape(evaluated(session.document()),original_shape,"Definition update cannot silently change a pinned instance");
    const auto before_migration=encode(session.document());
    apply(session,{MacroCommand{UpdateMacroInstance{"path","linear-instance",2}}});
    check(session.document().objects.at("path").stack[1].macro->pinned_revision==2&&
        macro_parameter_value(session.document(),"path","linear-instance","macro.offset.amount")==12,
        "Explicit revision migration retains the stable override when its mapped Offset changes");
    same_shape(evaluated(session.document()),evaluated(ordinary(next,12)),"Migrated override applies only to replacement mapped Offset");
    session.undo(session.revision());check(encode(session.document())==before_migration,"Migration is one Undo with exact pin and override restoration");
    session.redo(session.revision());
    auto removed=chain({"nect.shape.repeater"}).revisions.at(1);removed.revision=3;removed.public_parameters.clear();
    apply(session,{MacroCommand{UpdateMacroDefinition{definition.id,removed}}});
    atomic(session,"ORPHAN_MACRO_OVERRIDE",{MacroCommand{UpdateMacroInstance{"path","linear-instance",3}}});
    const auto before_reset=encode(session.document());
    apply(session,{MacroCommand{ResetMacroOverride{"path","linear-instance","macro.offset.amount"}},
        MacroCommand{UpdateMacroInstance{"path","linear-instance",3}}});
    check(session.document().objects.at("path").stack[1].macro->pinned_revision==3&&
        session.document().objects.at("path").stack[1].macro->overrides.empty(),
        "Later repeater-only revision is eligible after explicit public-override reset");
    same_shape(evaluated(session.document()),evaluated(ordinary(removed)),"Migrated repeater-only chain evaluates its literal defaults");
    session.undo(session.revision());check(encode(session.document())==before_reset,"Reset and migration undo atomically");

    const auto before_detach=encode(session.document());const auto shape=evaluated(session.document());
    Session reopened(decode(before_detach));
    check(reopened.document()==session.document()&&reopened.document().objects.at("path").stack[1].macro->pinned_revision==2&&
        reopened.document().macro_definitions.at(definition.id).latest_revision==3,
        "Native reopen preserves all retained revisions and the older explicit pin after interface removal");
    same_shape(evaluated(reopened.document()),shape,"Native reopen recomputes migrated selective-mapping geometry");
    apply(session,{MacroCommand{DetachMacroInstance{"path","linear-instance","fresh"}}});
    const auto& stack=session.document().objects.at("path").stack;
    check(stack.size()==4&&stack[0].id=="fill"&&stack[1].id=="fresh-detached-1"&&
        stack[2].id=="fresh-detached-2"&&stack[3].id=="fresh-detached-3"&&
        stack[1].type=="nect.shape.offset"&&stack[2].type=="nect.shape.offset"&&stack[3].type=="nect.shape.repeater"&&
        !stack[1].macro&&!stack[2].macro&&!stack[3].macro,
        "Detach materializes every node in edge order at the original stack position with fresh identities");
    check(stack[1].parameters.at("amount").literal==12&&stack[2].parameters.at("amount").literal==3,
        "Detach writes public override only to its mapped Offset and preserves other Offset default");
    same_shape(evaluated(session.document()),shape,"Detach preserves all cubic geometry and paints");
    const auto detached_native=encode(session.document());
    session.undo(session.revision());check(encode(session.document())==before_detach,"Three-node detach is one Undo");
    session.redo(session.revision());check(encode(session.document())==detached_native,"Detach Redo preserves exact fresh operation order");
}
void graph_limits_and_refusals() {
    auto refuse=[](MacroDefinition definition,const char* code) {
        Session session(fixture());atomic(session,code,{MacroCommand{CreateMacroDefinition{std::move(definition)}}});
    };
    for(const auto count:{std::size_t{1},std::size_t{16}}) {
        const auto definition=chain(std::vector<std::string>(count,"nect.shape.offset"));
        Session session(fixture());instantiate(session,definition);
        check(macro_execution_order(definition.revisions.at(1)).size()==count,"Graph v2 admits its 1-node and 16-node bounds");
        same_shape(evaluated(session.document()),evaluated(ordinary(definition.revisions.at(1))),"Boundary-length chain matches ordinary operations");
    }
    refuse(chain(std::vector<std::string>(17,"nect.shape.offset")),"INVALID_MACRO_GRAPH");
    auto empty=chain();empty.revisions.at(1).nodes.clear();refuse(empty,"INVALID_MACRO_GRAPH");
    auto cycle=chain();cycle.revisions.at(1).edges[1].to={"node-1","node-1-input"};refuse(cycle,"INVALID_MACRO_GRAPH");
    auto branch=chain();branch.revisions.at(1).edges[1].from={"node-1","node-1-output"};refuse(branch,"INVALID_MACRO_GRAPH");
    auto disconnected=chain();disconnected.revisions.at(1).edges[2].to={"node-3","node-3-input"};refuse(disconnected,"INVALID_MACRO_GRAPH");
    auto wrong_output=chain();wrong_output.revisions.at(1).output_mapping={"node-1","node-1-output"};refuse(wrong_output,"INVALID_MACRO_GRAPH");
    auto unsupported=chain();node(unsupported.revisions.at(1),"node-2").operation.type="nect.shape.unknown";refuse(unsupported,"UNSUPPORTED_MACRO_NODE");
    auto driven=chain();node(driven.revisions.at(1),"node-1").operation.parameters.at("amount").expression=Expression{"2",1};refuse(driven,"INVALID_MACRO_NODE");
    auto no_public=chain();no_public.revisions.at(1).public_parameters.clear();refuse(no_public,"INVALID_MACRO_INTERFACE");
    auto bad_mapping=chain();bad_mapping.revisions.at(1).public_parameters.front().node="node-3";refuse(bad_mapping,"INVALID_MACRO_MAPPING");
    auto old_version=chain();old_version.revisions.at(1).graph_version=1;refuse(old_version,"INVALID_MACRO_GRAPH");
    auto old_reversed=chain({"nect.shape.repeater","nect.shape.offset"});old_reversed.revisions.at(1).graph_version=1;refuse(old_reversed,"INVALID_MACRO_GRAPH");
    auto future=chain();future.revisions.at(1).graph_version=3;refuse(future,"UNSUPPORTED_MACRO_GRAPH_VERSION");
}
void native_and_portable_boundaries() {
    const auto definition=chain();Session session(fixture());instantiate(session,definition);
    const auto native=parsed(encode(session.document()));
    for(const auto* version:{"0.82","0.65","0.1"}) {
        auto older=native;older["version"]=version;
        rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(boost::json::serialize(older));});
    }
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(definition);});
    const auto portable_json=boost::json::serialize(native.at("macros").as_array().front());
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)read_canonical_macro_payload(portable_json);});
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{validate_portable_macro_definition(definition);});
    Session imported(fixture());
    atomic(imported,"UNSUPPORTED_PORTABLE_MACRO_GRAPH",{MacroCommand{InstantiateMacro{
        "path","imported-definition","imported-instance",1,1,definition,"portable-asset",41,{}}}});
    const auto before_import=encode(imported.document());const auto before_revision=imported.revision();
    const auto before_history=imported.history();
    const auto import_reply=parsed(request(imported,boost::json::serialize(boost::json::object{
        {"op","apply"},{"expected_revision",imported.revision()},{"commands",boost::json::array{
            boost::json::object{{"type","import_apply_macro"},{"definition",native.at("macros").as_array().front()},
                {"definition_id","api-imported-definition"},{"object","path"},{"instance","api-imported-instance"},
                {"pinned_revision",1},{"index",1},{"asset_id","portable-asset"},{"accepted_revision",41}}}}})));
    check(!import_reply.at("ok").as_bool()&&import_reply.at("error").as_object().at("code").as_string()=="UNSUPPORTED_PORTABLE_MACRO_GRAPH"&&
        encode(imported.document())==before_import&&imported.revision()==before_revision&&imported.history()==before_history,
        "Portable graph2 import is refused atomically through the shared JSON-lines API");

    auto legacy=chain({"nect.shape.offset","nect.shape.repeater"});legacy.revisions.at(1).graph_version=1;
    Session old(fixture());instantiate(old,legacy);
    auto old_json=parsed(encode(old.document()));
    check(!serialized_revision(old_json).contains("graph_version"),"Native writer omits the default graph v1 field");
    old_json["version"]="0.82";
    check(decode(boost::json::serialize(old_json))==old.document(),"Legacy native 0.82 keeps exact Offset-Repeater graphs readable without the new field");
    serialized_revision(old_json)["graph_version"]=1;
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(boost::json::serialize(old_json));});
    check(read_canonical_macro_payload(canonical_macro_payload(legacy))==legacy,
        "Portable Macro payload v1 retains its existing graph v1 roundtrip");
    auto mixed=legacy;auto graph2=definition.revisions.at(1);graph2.revision=2;
    mixed.revisions.emplace(2,graph2);mixed.latest_revision=2;
    atomic(imported,"UNSUPPORTED_PORTABLE_MACRO_GRAPH",{MacroCommand{InstantiateMacro{
        "path","retained-definition","retained-instance",1,1,mixed,"portable-asset",41,{}}}});
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(mixed);});
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3&&std::string(argv[1])=="--cold-read") {
            cold_read(argv[2]);std::cout<<"Macro linear v2 cold checks: "<<checks<<"\n";return 0;
        }
        primary_flow_and_cold_reopen(std::filesystem::absolute(argv[0]).string());
        noncommuting_repeater_edge_order();
        explicit_revisions_and_detach();
        graph_limits_and_refusals();
        native_and_portable_boundaries();
        std::cout<<"Macro linear v2 checks: "<<checks<<"\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<"\n";return 1;
    }
}
