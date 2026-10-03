#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& message) {
    if(!ok)throw std::runtime_error(message);
    ++checks;
}
void apply(Session& session,std::vector<Command> commands) {
    session.apply(commands,session.revision());
}
template<class F> void rejects(const char* code,F action) {
    try{action();}catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
struct State {
    std::string native;
    std::uint64_t revision;
    HistoryInfo history;
};
State state(const Session& session) {
    return {encode(session.document()),session.revision(),session.history()};
}
void unchanged(const Session& session,const State& before,const char* message) {
    check(encode(session.document())==before.native&&session.revision()==before.revision&&
        session.history()==before.history,message);
}
void atomic(Session& session,const char* code,std::vector<Command> commands,
    std::optional<std::uint64_t> expected={}) {
    const auto before=state(session);
    rejects(code,[&]{session.apply(commands,expected.value_or(session.revision()));});
    unchanged(session,before,"Refusal preserves exact native bytes, revision and history");
}
Object rectangle(const Id& id,double y=0) {
    Object object;object.id=id;object.name=id;
    object.source=default_primitive(id+"-rectangle","nect.shape.rectangle");
    object.source->parameters.at("width").literal=30;
    object.source->parameters.at("height").literal=20;
    object.transform[5].literal=y;
    object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
    return object;
}
Document source_document(bool driven=false) {
    auto document=empty_document("visibility-doc","comp","source-board");
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;
    root.children={"control","logo","peer","folder"};root.transform[4].literal=10;
    Object control;control.id="control";control.name="Control";control.kind=Kind::group;
    control.visible=!driven;
    auto logo=rectangle("logo");auto peer=rectangle("peer",30);
    if(driven) {
        logo.visibility_driver=Ref{"control","","object.visible"};
        peer.visibility_expression=Expression{" ! ref ( \"control\" , \"\" , \"object.visible\" ) ",1};
    }
    Object folder;folder.id="folder";folder.name="Folder";folder.kind=Kind::group;
    folder.children={"nested"};folder.visible=false;
    auto nested=rectangle("nested",60);
    Object outside;outside.id="outside";outside.name="Outside";outside.kind=Kind::group;
    for(const auto& object:{root,control,logo,peer,folder,nested,outside})
        document.objects.emplace(object.id,object);
    document.compositions.front().roots={"source-root","outside"};
    return document;
}
Document fixture(bool driven=false) {
    Session session(source_document(driven));
    apply(session,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        DefinitionCommand{CreateInstance{"comp","","instance-a","definition","A"}},
        DefinitionCommand{CreateInstance{"comp","","instance-b","definition","B"}},
        Set{{"instance-a","","transform.tx"},120},Set{{"instance-b","","transform.tx"},240}});
    return session.document();
}
EvaluatedScene scene(const Document& document) {
    const auto values=evaluate(document);
    return evaluate_scene(document,"comp",values,evaluate_transforms(document,values));
}
const EvaluatedSceneNode& node(const std::vector<EvaluatedSceneNode>& nodes,const Id& id) {
    for(const auto& item:nodes) {
        if(item.id==id)return item;
        if(!item.children.empty()) {
            try{return node(item.children,id);}catch(const std::out_of_range&){}
        }
    }
    throw std::out_of_range("Missing scene node: "+id);
}
Id proxy(const EvaluatedScene& evaluated,const Id& owner,const Id& source) {
    for(const auto& [id,instance]:evaluated.instance_owners)
        if(instance==owner&&evaluated.instance_sources.at(id)==source)return id;
    throw std::runtime_error("Missing occurrence: "+owner+" / "+source);
}
bool visible(const EvaluatedScene& evaluated,const Id& owner,const Id& source) {
    return node(evaluated.roots,proxy(evaluated,owner,source)).visible;
}
bool svg_has(const std::string& svg,const Id& id) {
    return svg.find("id=\""+id+"\"")!=std::string::npos;
}
void same_sources(const Document& actual,const Document& expected) {
    for(const auto* id:{"source-root","control","logo","peer","folder","nested","outside"})
        check(actual.objects.at(id)==expected.objects.at(id),"Authored source item remains byte-for-byte equivalent: "+Id{id});
}
const DefinitionInstance& instance(const Document& document,const Id& id="instance-a") {
    return *document.objects.at(id).instance;
}

void local_projection_and_selective_reset() {
    Session session(fixture());const auto source=session.document();const auto before=state(session);
    apply(session,{DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","logo",false}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","peer",false}},
        DefinitionCommand{SetInstanceOverride{"instance-a",{"logo","","transform.tx"},90}}});
    const auto overridden=state(session);
    same_sources(session.document(),source);
    auto evaluated=scene(session.document());
    check(node(evaluated.roots,"logo").visible&&!visible(evaluated,"instance-a","logo")&&
        visible(evaluated,"instance-b","logo"),"Plain source and two Instances have independent effective item visibility");
    const auto svg=export_svg(session.document(),"comp","source-board");
    check(svg_has(svg,"logo")&&!svg_has(svg,proxy(evaluated,"instance-a","logo"))&&
        svg_has(svg,proxy(evaluated,"instance-b","logo")),"SVG output agrees with local occurrence visibility");
    unchanged(session,overridden,"Projection and SVG leave authored native state and history pure");
    session.undo(session.revision());
    check(encode(session.document())==before.native,"One Undo restores the complete override batch");
    session.redo(session.revision());
    check(encode(session.document())==overridden.native,"One Redo restores exact bool and Scalar overrides");

    apply(session,{SetVisibility{"logo",false}});evaluated=scene(session.document());
    check(!node(evaluated.roots,"logo").visible&&!visible(evaluated,"instance-a","logo")&&
        !visible(evaluated,"instance-b","logo"),"Source hide reaches only the unoverridden occurrence");
    apply(session,{SetVisibility{"logo",true}});evaluated=scene(session.document());
    check(node(evaluated.roots,"logo").visible&&!visible(evaluated,"instance-a","logo")&&
        visible(evaluated,"instance-b","logo"),"Later source show leaves the active local hide intact");
    apply(session,{Rename{"logo","Renamed Logo"},ReorderObjects{"comp","source-root",{"folder","peer","logo","control"}}});
    check(instance(session.document()).visibility_overrides.at("logo")==false,
        "Source rename/reorder keeps the same SourceItemID override");
    const auto before_reset=encode(session.document());
    apply(session,{DefinitionCommand{ResetInstanceVisibilityOverride{"instance-a","logo"}}});
    evaluated=scene(session.document());
    check(visible(evaluated,"instance-a","logo")&&!visible(evaluated,"instance-a","peer")&&
        instance(session.document()).visibility_overrides==std::map<Id,bool>{{"peer",false}}&&
        instance(session.document()).overrides.at({"logo","","transform.tx"})==90,
        "Reset restores selected inheritance and retains sibling bool and Scalar overrides");
    const auto after_reset=encode(session.document());
    session.undo(session.revision());check(encode(session.document())==before_reset,"Undo restores only the removed local bool");
    session.redo(session.revision());check(encode(session.document())==after_reset,"Redo repeats the exact selective Reset");
    apply(session,{SetVisibility{"logo",false}});
    check(!visible(scene(session.document()),"instance-a","logo"),"Source edits resume propagation after Reset");
}

void linked_and_expression_source_purity() {
    Session session(fixture(true));const auto before=session.document();
    apply(session,{DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","logo",true}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","peer",false}}});
    same_sources(session.document(),before);
    auto evaluated=scene(session.document());
    check(!node(evaluated.roots,"logo").visible&&node(evaluated.roots,"peer").visible&&
        visible(evaluated,"instance-a","logo")&&!visible(evaluated,"instance-a","peer")&&
        !visible(evaluated,"instance-b","logo")&&visible(evaluated,"instance-b","peer"),
        "Local bool supersedes source link/expression only in its selected occurrence");
    const auto& local_logo=evaluated.expanded_document->objects.at(proxy(evaluated,"instance-a","logo"));
    const auto& local_peer=evaluated.expanded_document->objects.at(proxy(evaluated,"instance-a","peer"));
    check(local_logo.visible&&!local_logo.visibility_driver&&!local_logo.visibility_expression&&
        !local_peer.visible&&!local_peer.visibility_driver&&!local_peer.visibility_expression,
        "Only projected overridden copies become literals with cleared visibility sources");
    check(evaluated.expanded_document->objects.at(proxy(evaluated,"instance-b","logo")).visibility_driver.has_value()&&
        evaluated.expanded_document->objects.at(proxy(evaluated,"instance-b","peer")).visibility_expression.has_value(),
        "Unmodified occurrence retains remapped visibility link and expression");
    apply(session,{SetVisibility{"control",true}});evaluated=scene(session.document());
    check(visible(evaluated,"instance-a","logo")&&!visible(evaluated,"instance-a","peer")&&
        visible(evaluated,"instance-b","logo")&&!visible(evaluated,"instance-b","peer"),
        "Source driver changes remain live without replacing local bools");
    apply(session,{DefinitionCommand{ResetInstanceVisibilityOverride{"instance-a","logo"}}});
    evaluated=scene(session.document());
    check(evaluated.expanded_document->objects.at(proxy(evaluated,"instance-a","logo")).visibility_driver==
        Ref{proxy(evaluated,"instance-a","control"),"","object.visible"}&&
        instance(session.document()).visibility_overrides.at("peer")==false,
        "Reset restores the current remapped driver and keeps sibling override");
    apply(session,{DefinitionCommand{ResetInstanceVisibilityOverride{"instance-a","peer"}},SetVisibility{"control",false}});
    evaluated=scene(session.document());
    check(!visible(evaluated,"instance-a","logo")&&visible(evaluated,"instance-a","peer"),
        "Reset restores live link and expression evaluation");
    check(session.document().objects.at("logo")==before.objects.at("logo")&&
        session.document().objects.at("peer")==before.objects.at("peer")&&
        session.document().objects.at("peer").visibility_expression->source==
            " ! ref ( \"control\" , \"\" , \"object.visible\" ) ",
        "Set, projection and Reset preserve original literals, Ref and exact expression bytes");
}

void ancestor_suppression_and_root_placement() {
    Session session(fixture());
    apply(session,{DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","nested",true}}});
    auto evaluated=scene(session.document());auto svg=export_svg(session.document(),"comp","source-board");
    check(visible(evaluated,"instance-a","nested")&&!visible(evaluated,"instance-a","folder")&&
        !svg_has(svg,proxy(evaluated,"instance-a","nested")),
        "A true local child remains suppressed by its hidden source ancestor");
    apply(session,{DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","folder",true}}});
    evaluated=scene(session.document());svg=export_svg(session.document(),"comp","source-board");
    check(svg_has(svg,proxy(evaluated,"instance-a","nested"))&&
        !svg_has(svg,proxy(evaluated,"instance-b","nested"))&&!svg_has(svg,"nested"),
        "Showing the local descendant parent affects only that occurrence subtree");
    apply(session,{SetVisibility{"source-root",false}});evaluated=scene(session.document());
    check(!node(evaluated.roots,"source-root").visible&&visible(evaluated,"instance-a","source-root")&&
        visible(evaluated,"instance-b","source-root")&&node(evaluated.roots,"instance-a").world[4]==120,
        "Source-root visibility/placement stay outside descendant inheritance");
    apply(session,{SetVisibility{"instance-a",false}});evaluated=scene(session.document());
    svg=export_svg(session.document(),"comp","source-board");
    check(visible(evaluated,"instance-a","nested")&&!svg_has(svg,proxy(evaluated,"instance-a","nested"))&&
        svg_has(svg,proxy(evaluated,"instance-b","logo")),
        "Occurrence visibility suppresses the entire subtree even with a true child override");
}

void duplicate_template_target_and_detach() {
    auto document=source_document();document.compositions.front().artboards.push_back({"target","Target",100,100,640,480});
    Session target(document);
    apply(target,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"template","T","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target","template",Id{"content"}}},
        DefinitionCommand{SetInstanceVisibilityOverride{"content","logo",false}},
        DefinitionCommand{SetInstanceVisibilityOverride{"content","peer",true}},
        DefinitionCommand{SetInstanceOverride{"content",{"logo","","transform.tx"},90}}});
    const auto original=target.document();const auto before_duplicate=encode(original);
    apply(target,{ArtboardTemplateCommand{DuplicateTemplateArtboard{"comp","target","copy",700,100,2}}});
    check(instance(target.document(),"copy-content-1").visibility_overrides==instance(original,"content").visibility_overrides&&
        instance(target.document(),"copy-content-1").visibility_overrides.contains("logo")&&
        instance(target.document(),"copy-content-1").overrides==instance(original,"content").overrides,
        "Duplicated Template target retains bool and Scalar maps keyed by original SourceItemIDs");
    same_sources(target.document(),original);
    const auto duplicated=encode(target.document());const auto cold=decode(duplicated);
    check(encode(cold)==duplicated&&!visible(scene(cold),"copy-content-1","logo"),
        "Native cold decode retains duplicate content relation and local hidden output");
    target.undo(target.revision());check(encode(target.document())==before_duplicate,"One Undo removes only the duplicated target");
    target.redo(target.revision());check(encode(target.document())==duplicated,"One Redo restores the same target/Instance identities");
    apply(target,{DefinitionCommand{ResetInstanceVisibilityOverride{"copy-content-1","logo"}}});
    check(visible(scene(target.document()),"copy-content-1","logo")&&
        !visible(scene(target.document()),"content","logo"),"Reset on duplicate does not change original target local visibility");

    Session session(fixture(true));
    apply(session,{SetVisibility{"source-root",false},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","logo",true}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","peer",false}},
        DefinitionCommand{SetInstanceOverride{"instance-a",{"logo","","transform.tx"},90}}});
    const auto live=session.document();const auto live_bytes=encode(live);
    apply(session,{DefinitionCommand{DetachInstance{"instance-a","detached"}}});
    const auto detached=session.document();const auto detached_bytes=encode(detached);
    const auto& placed=detached.objects.at("instance-a");const auto& root=detached.objects.at(placed.children.front());
    const auto& logo=detached.objects.at(root.children[1]);const auto& peer=detached.objects.at(root.children[2]);
    check(placed.kind==Kind::group&&!placed.instance&&placed.transform[4].literal==120&&root.visible&&
        root.transform[4].literal==0,"Detach retains occurrence identity/placement and excludes source-root visibility/placement");
    check(logo.visible&&!logo.visibility_driver&&!logo.visibility_expression&&
        !peer.visible&&!peer.visibility_driver&&!peer.visibility_expression&&logo.transform[4].literal==90,
        "Whole Detach freezes effective local bool and Scalar overrides in materialized children");
    same_sources(detached,live);
    session.undo(session.revision());check(encode(session.document())==live_bytes,"One Undo of detach restores exact live bool maps and sources");
    session.redo(session.revision());check(encode(session.document())==detached_bytes,"One Redo of detach restores exact materialized identities/state");
    check(encode(decode(detached_bytes))==detached_bytes,"Cold decode preserves materialized visibility literals");
    apply(session,{SetVisibility{"control",true},Set{{"logo","","transform.tx"},200}});
    check(session.document().objects.at(logo.id)==logo&&session.document().objects.at(peer.id)==peer,
        "Later source driver/geometry edits do not alter detached visibility or materialized source-free copies");
}

std::string versioned(std::string native,const char* version) {
    const auto marker=test_support::current_native_version_marker();const auto at=native.find(marker);
    check(at!=std::string::npos,"Fixture uses current native writer");
    native.replace(at,marker.size(),std::string("\"version\":\"")+version+"\"");return native;
}
std::string visibility_array(std::string native,const std::string& entries) {
    const std::string field="\"visibility_overrides\":[";const auto at=native.find(field);
    check(at!=std::string::npos,"Native fixture has one visibility override array");
    const auto begin=at+field.size(),end=native.find(']',begin);
    check(end!=std::string::npos,"Native fixture has a closed visibility array");
    native.replace(begin,end-begin,entries);return native;
}
void native_contract_and_atomic_refusals() {
    Session session(fixture());const auto legacy_document=session.document();
    const auto old=versioned(encode(legacy_document),"0.81");
    check(decode(old)==legacy_document,"Native 0.81 without the optional field remains readable");
    apply(session,{DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","logo",false}}});
    const auto saved=encode(session.document());
    check(saved.find("\"visibility_overrides\":[{\"source\":\"logo\",\"visible\":false}]")!=std::string::npos,
        "Native writer serializes closed source/visible bool entries on the Instance payload");
    const auto reopened=decode(versioned(saved,"0.82"));
    check(reopened==session.document()&&!visible(scene(reopened),"instance-a","logo")&&
        visible(scene(reopened),"instance-b","logo"),"Native 0.82 cold decode restores authored map and independent output visibility");
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(saved,"0.81"));});
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(visibility_array(saved,""),"0.81"));});
    for(const auto* value:{"0","\"false\"","null"}) {
        rejects("INVALID_INPUT",[&]{(void)decode(visibility_array(saved,
            std::string("{\"source\":\"logo\",\"visible\":")+value+"}"));});
    }
    rejects("DUPLICATE_OVERRIDE_KEY",[&]{(void)decode(visibility_array(saved,
        R"({"source":"logo","visible":false},{"source":"logo","visible":true})"));});
    rejects("DUPLICATE_KEY",[&]{(void)decode(visibility_array(saved,
        R"({"source":"logo","visible":false,"visible":true})"));});
    rejects("UNKNOWN_FIELD",[&]{(void)decode(visibility_array(saved,
        R"({"source":"logo","visible":false,"target":"peer"})"));});
    const auto native_before=state(session);
    rejects("UNSUPPORTED_OVERRIDE",[&]{(void)decode(visibility_array(saved,R"({"source":"source-root","visible":false})"));});
    rejects("DANGLING_OVERRIDE",[&]{(void)decode(visibility_array(saved,R"({"source":"missing","visible":false})"));});
    rejects("DANGLING_OVERRIDE",[&]{(void)decode(visibility_array(saved,R"({"source":"outside","visible":false})"));});
    unchanged(session,native_before,"Invalid native decode leaves the live Session untouched");
    atomic(session,"UNSUPPORTED_OVERRIDE",{DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","source-root",false}}});
    atomic(session,"DANGLING_OVERRIDE",{DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","missing",false}}});
    atomic(session,"DANGLING_OVERRIDE",{DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","outside",false}}});
    atomic(session,"TYPE_MISMATCH",{DefinitionCommand{SetInstanceVisibilityOverride{"logo","peer",false}}});
    atomic(session,"MISSING_OBJECT",{DefinitionCommand{SetInstanceVisibilityOverride{"missing","logo",false}}});
    atomic(session,"NO_OVERRIDE",{DefinitionCommand{ResetInstanceVisibilityOverride{"instance-a","peer"}}});
    atomic(session,"REVISION_CONFLICT",{DefinitionCommand{ResetInstanceVisibilityOverride{"instance-a","logo"}}},session.revision()+1);
    atomic(session,"MISSING_OBJECT",{DefinitionCommand{SetInstanceVisibilityOverride{"instance-b","peer",false}},
        SetVisibility{"missing",false}});
}

std::string api_apply(Session& session,const std::string& commands) {
    return request(session,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(session.revision())+
        ",\"commands\":["+commands+"]}");
}
void api_atomic(Session& session,const char* code,const std::string& commands) {
    const auto before=state(session);const auto response=api_apply(session,commands);
    check(response.find("\"ok\":false")!=std::string::npos&&
        response.find(std::string("\"code\":\"")+code+"\"")!=std::string::npos,"API reports "+std::string(code)+": "+response);
    unchanged(session,before,"Invalid API request preserves exact native bytes/revision/history");
}
void canonical_api() {
    Session session(fixture());const auto initial=session.document();
    const auto set=api_apply(session,R"({"type":"set_instance_visibility_override","instance":"instance-a","source":"logo","visible":false},{"type":"set_instance_visibility_override","instance":"instance-a","source":"peer","visible":true})");
    check(set.find("\"ok\":true")!=std::string::npos&&instance(session.document()).visibility_overrides==
        std::map<Id,bool>{{"logo",false},{"peer",true}},"Canonical API applies explicit true and false local overrides in one Session batch");
    same_sources(session.document(),initial);
    check(request(session,R"({"op":"inspect"})").find("\"visibility_overrides\":[{\"source\":\"logo\",\"visible\":false}")!=std::string::npos,
        "Inspect exposes the same authored visibility override map");
    api_atomic(session,"UNSUPPORTED_OVERRIDE",R"({"type":"set_instance_visibility_override","instance":"instance-a","source":"source-root","visible":false})");
    api_atomic(session,"DANGLING_OVERRIDE",R"({"type":"set_instance_visibility_override","instance":"instance-a","source":"outside","visible":false})");
    api_atomic(session,"INVALID_REQUEST",R"({"type":"set_instance_visibility_override","instance":"instance-a","source":"logo","visible":0})");
    api_atomic(session,"INVALID_REQUEST",R"({"type":"set_instance_visibility_override","instance":"instance-a","source":"logo","visible":"false"})");
    api_atomic(session,"DUPLICATE_KEY",R"({"type":"set_instance_visibility_override","instance":"instance-a","source":"logo","visible":false,"visible":true})");
    api_atomic(session,"UNKNOWN_FIELD",R"({"type":"reset_instance_visibility_override","instance":"instance-a","source":"logo","visible":true})");
    api_atomic(session,"MISSING_OBJECT",R"({"type":"reset_instance_visibility_override","instance":"instance-a","source":"logo"},{"type":"set_visibility","object":"missing","visible":false})");
    const auto before=state(session);const auto stale=request(session,
        "{\"op\":\"apply\",\"expected_revision\":"+std::to_string(session.revision()+1)+
        R"(,"commands":[{"type":"reset_instance_visibility_override","instance":"instance-a","source":"logo"}]})");
    check(stale.find("REVISION_CONFLICT")!=std::string::npos,"API rejects stale visibility Reset");
    unchanged(session,before,"Stale API Reset preserves exact native state/revision/history");
    const auto reset=api_apply(session,R"({"type":"reset_instance_visibility_override","instance":"instance-a","source":"logo"})");
    check(reset.find("\"ok\":true")!=std::string::npos&&instance(session.document()).visibility_overrides==
        std::map<Id,bool>{{"peer",true}}&&visible(scene(session.document()),"instance-a","logo"),
        "Canonical API Reset returns selected item to inheritance and retains sibling override");
}
}
int main() {
    try {
        local_projection_and_selective_reset();linked_and_expression_source_purity();
        ancestor_suppression_and_root_placement();duplicate_template_target_and_detach();
        native_contract_and_atomic_refusals();canonical_api();
        std::cout<<"PASS "<<checks<<" Instance visibility override checks\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
    }
}
