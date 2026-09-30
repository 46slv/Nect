#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);++checks;}
void near(double a,double b,const char* message){check(std::abs(a-b)<1e-8,
    std::string(message)+" (actual="+std::to_string(a)+", expected="+std::to_string(b)+")");}
template<class F>void rejects(const char* code,F action) {
    try{action();}catch(const Error& error){check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
void matrix(Object& object,Affine value){for(std::size_t i=0;i<value.size();++i)object.transform[i].literal=value[i];}
Object rectangle(std::string id,double x=0,double y=0) {
    Object object;object.id=id;object.name=id;Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(const auto& xy:std::vector<Vec2>{{x,y},{x+30,y},{x+30,y+20},{x,y+20}}) {
        Point point;point.id=id+"-p"+std::to_string(contour.points.size());
        point.x.literal=xy.x;point.y.literal=xy.y;contour.points.push_back(point);
    }
    object.contours.push_back(contour);
    auto fill=default_operation(id+"-fill","nect.paint.fill");
    fill.parameters.at("r").literal=0.9;fill.parameters.at("g").literal=0.1;
    fill.parameters.at("b").literal=0.1;fill.parameters.at("a").literal=1;
    object.stack.push_back(fill);return object;
}
Document fixture() {
    auto document=empty_document("definition-doc","definition-comp","definition-art");
    Object outer;outer.id="outer";outer.name="External transform parent";outer.kind=Kind::group;
    matrix(outer,{1,0,0,1,350,240});
    Object root;root.id="source-root";root.name="D";root.kind=Kind::group;
    root.children={"path-a","text-b"};matrix(root,{2,0,0,2,40,30});root.transform_parent="outer";
    root.visible=false;root.compositing.opacity.literal=0.5;
    auto path=rectangle("path-a",10,12);path.name="A";
    Object text;text.id="text-b";text.name="B";text.kind=Kind::text;text.text=default_text("text-source","Source text");
    text.transform[4].literal=15;text.transform[5].literal=55;
    document.objects.emplace(outer.id,outer);document.objects.emplace(root.id,root);
    document.objects.emplace(path.id,path);document.objects.emplace(text.id,text);
    document.compositions.front().roots={"outer","source-root"};
    return document;
}
EvaluatedScene scene(const Document& document) {
    const auto values=evaluate(document);
    return evaluate_scene(document,"definition-comp",values,evaluate_transforms(document,values));
}
const EvaluatedSceneNode* find_node(const std::vector<EvaluatedSceneNode>& nodes,const Id& id) {
    for(const auto& node:nodes) {
        if(node.id==id)return &node;
        if(const auto* child=find_node(node.children,id))return child;
    }
    return nullptr;
}
Id proxy_for(const EvaluatedScene& evaluated,const Id& instance,const Id& source) {
    for(const auto& [proxy,owner]:evaluated.instance_owners)
        if(owner==instance&&evaluated.instance_sources.at(proxy)==source)return proxy;
    throw std::runtime_error("Missing render proxy for "+instance+" / "+source);
}
void same_matrix(const Affine& a,const Affine& b,const char* message) {
    for(std::size_t i=0;i<a.size();++i)near(a[i],b[i],message);
}
void source_projection_and_overrides() {
    Session session(fixture());
    apply(session,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}}});
    apply(session,{DefinitionCommand{CreateInstance{"definition-comp","","instance-1","definition","I1"}},
        DefinitionCommand{CreateInstance{"definition-comp","","instance-2","definition","I2"}},
        Set{{"instance-1","","transform.tx"},25},Set{{"instance-1","","transform.ty"},20},
        Set{{"instance-2","","transform.tx"},105},Set{{"instance-2","","transform.ty"},20}});

    apply(session,{DefinitionCommand{SetInstanceOverride{"instance-1",{"path-a","","composite.opacity"},0.2}},
        DefinitionCommand{SetInstanceOverride{"instance-2",{"text-b","","text.font_size"},34}}});
    const auto evaluated=scene(session.document());
    check(evaluated.expanded_document&&evaluated.instance_owners.size()==6&&evaluated.instance_sources.size()==6,
        "Two Instances expand to unique transient render Objects without changing authored Object count");
    check(session.document().objects.size()==6,"Rendering never adds proxy Objects to authored state");
    const auto* master=find_node(evaluated.roots,"source-root");
    const auto* first=find_node(evaluated.roots,"instance-1");
    const auto* second=find_node(evaluated.roots,"instance-2");
    check(master&&!master->visible&&first&&first->visible&&second&&second->visible,
        "Master-root visibility is excluded while each Instance uses its own visibility");
    same_matrix(first->world,{1,0,0,1,25,20},"Instance placement is its own world transform");
    same_matrix(second->world,{1,0,0,1,105,20},"Second Instance placement is independent");
    const auto proxy_root=proxy_for(evaluated,"instance-1","source-root");
    const auto* root_node=find_node(evaluated.roots,proxy_root);
    check(root_node&&root_node->visible,"Definition root visibility is reset to visible in projection");
    near(root_node->opacity,0.5,"Definition-root compositing opacity remains in the evaluated source");
    same_matrix(root_node->world,first->world,"Source-root affine and external Transform Parent do not affect Instance content");
    const auto proxy_path_1=proxy_for(evaluated,"instance-1","path-a");
    const auto proxy_path_2=proxy_for(evaluated,"instance-2","path-a");
    const auto proxy_text_1=proxy_for(evaluated,"instance-1","text-b");
    const auto proxy_text_2=proxy_for(evaluated,"instance-2","text-b");
    check(proxy_path_1!=proxy_path_2&&evaluated.shapes.contains(proxy_path_1)&&evaluated.shapes.contains(proxy_path_2),
        "Each Instance has distinct shape-map keys and render node IDs");
    near(evaluated.expanded_values->at({proxy_path_1,"","composite.opacity"}),0.2,
        "Instance 1 applies its Path opacity override");
    near(evaluated.expanded_values->at({proxy_path_2,"","composite.opacity"}),1,
        "Instance 2 receives no unrelated Path override");
    near(evaluated.expanded_values->at({proxy_text_2,"","text.font_size"}),34,
        "Instance 2 applies its Text font-size override");
    near(evaluated.expanded_values->at({proxy_text_1,"","text.font_size"}),48,
        "Instance 1 receives no unrelated Text override");
    check(evaluated.expanded_document->objects.at(proxy_path_1).name=="A"&&
        evaluated.expanded_document->objects.at(proxy_text_2).name=="B",
        "Proxy copies retain the source item names");

    const auto baseline_bounds=object_bounds(*evaluated.expanded_document,proxy_path_1,*evaluated.expanded_values,
        *evaluated.expanded_transforms,true);
    check(baseline_bounds.has_value(),"Expanded Path has real world geometry");
    const auto native_before=encode(session.document());
    apply(session,{SetVisibility{"source-root",true},Set{{"source-root","","transform.tx"},700},
        Set{{"source-root","","transform.ty"},900},Rename{"path-a","A renamed"},
        Set{{"path-a","path-a-p1","x"},60},Set{{"path-a","","composite.opacity"},0.65},
        SetVisibility{"path-a",false},ReorderObjects{"definition-comp","source-root",{"text-b","path-a"}}});
    auto changed=scene(session.document());
    const auto changed_path=proxy_for(changed,"instance-1","path-a");
    const auto changed_path_2=proxy_for(changed,"instance-2","path-a");
    const auto changed_text=proxy_for(changed,"instance-2","text-b");
    check(changed.expanded_document->objects.at(changed_path).name=="A renamed"&&
        changed.expanded_document->objects.at("instance-1").children.front()==proxy_for(changed,"instance-1","source-root"),
        "Source rename propagates and stable source IDs preserve references");
    check(changed.expanded_document->objects.at(proxy_for(changed,"instance-1","source-root")).children==
        std::vector<Id>{proxy_for(changed,"instance-1","text-b"),changed_path},
        "Source reorder propagates to each expanded Instance");
    near(changed.expanded_values->at({changed_path,"","composite.opacity"}),0.2,
        "A source Path opacity edit does not replace Instance 1's explicit local override");
    near(changed.expanded_values->at({changed_path_2,"","composite.opacity"}),0.65,
        "A source Path opacity edit propagates to Instance 2 without a local override");
    check(!find_node(changed.roots,changed_path)->visible&&!find_node(changed.roots,changed_path_2)->visible,
        "Descendant authored visibility propagates to both derived Instances");
    apply(session,{SetVisibility{"path-a",true}});
    changed=scene(session.document());
    check(find_node(changed.roots,proxy_for(changed,"instance-1","path-a"))->visible&&
        find_node(changed.roots,proxy_for(changed,"instance-2","path-a"))->visible,
        "Restoring descendant authored visibility restores both derived Instances");
    const auto changed_bounds=object_bounds(*changed.expanded_document,changed_path,*changed.expanded_values,
        *changed.expanded_transforms,true);
    check(changed_bounds&&changed_bounds->right>baseline_bounds->right,
        "Source geometry edits update the derived Instance geometry");
    check(evaluated_text_source(*changed.expanded_document,changed_text).content=="Source text",
        "Text source content remains available on each Instance proxy");
    const auto source_values=evaluate(session.document());
    const auto source_width=source_values.at({"text-b","","text.font_size"});
    apply(session,{UpdateText{"text-b",default_text("text-source","Updated source text")},
        Set{{"text-b","","text.font_size"},source_width+8}});
    changed=scene(session.document());
    const auto text_1=proxy_for(changed,"instance-1","text-b");
    const auto text_2=proxy_for(changed,"instance-2","text-b");
    check(evaluated_text_source(*changed.expanded_document,text_1).content=="Updated source text"&&
        evaluated_text_source(*changed.expanded_document,text_2).content=="Updated source text",
        "Source Text content edits flow to both Instances");
    near(changed.expanded_values->at({text_1,"","text.font_size"}),source_width+8,
        "Source Text size edits flow to an Instance without a local override");
    near(changed.expanded_values->at({text_2,"","text.font_size"}),34,
        "Source Text size edits do not replace an explicit local override");
    check(encode(session.document())!=native_before,"Authored source edits update the native Document");

    apply(session,{DefinitionCommand{ResetInstanceOverride{"instance-1",{"path-a","","composite.opacity"}}},
        DefinitionCommand{ResetInstanceOverride{"instance-2",{"text-b","","text.font_size"}}}});
    changed=scene(session.document());
    near(changed.expanded_values->at({proxy_for(changed,"instance-1","path-a"),"","composite.opacity"}),0.65,
        "Reset removes a local Path override and restores the live source value");
    near(changed.expanded_values->at({proxy_for(changed,"instance-2","text-b"),"","text.font_size"}),source_width+8,
        "Reset removes a local Text override and restores the updated source value");
}

void detach_history_native_api_and_export() {
    Session session(fixture());
    apply(session,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        DefinitionCommand{CreateInstance{"definition-comp","","instance-1","definition","I1"}},
        Set{{"instance-1","","transform.tx"},35},Set{{"instance-1","","transform.ty"},22},
        DefinitionCommand{SetInstanceOverride{"instance-1",{"path-a","","composite.opacity"},0.3}},
        DefinitionCommand{SetInstanceOverride{"instance-1",{"text-b","","text.font_size"},33}}});
    rejects("DEFINITION_IN_USE",[&]{apply(session,{DefinitionCommand{DeleteDefinition{"definition"}}});});
    check(session.document().definitions.contains("definition")&&session.document().objects.at("instance-1").instance,
        "Explicit delete refusal retains live Definition and Instance state");
    const auto before_detach=session.document();
    const auto reopened_live=decode(encode(before_detach));
    const auto reopened_scene=scene(reopened_live);
    check(reopened_live==before_detach&&reopened_live.objects.at("instance-1").instance->definition=="definition"&&
        reopened_live.objects.at("instance-1").instance->overrides.size()==2,
        "Native cold reopen preserves the live Definition relation and both local overrides");
    near(reopened_scene.expanded_values->at({proxy_for(reopened_scene,"instance-1","path-a"),"","composite.opacity"}),0.3,
        "Native cold reopen preserves the placed Path opacity override in rendering");
    near(reopened_scene.expanded_values->at({proxy_for(reopened_scene,"instance-1","text-b"),"","text.font_size"}),33,
        "Native cold reopen preserves the placed Text font-size override in rendering");
    const auto svg=export_svg(before_detach,"definition-comp","definition-art");
    check(svg.find("id=\"instance-1\"")!=std::string::npos&&svg.find("id=\"instance-1\"")<svg.rfind("</svg>"),
        "SVG export renders the placed Instance through the modern scene path");
    const auto detaching_revision=session.revision();
    apply(session,{DefinitionCommand{DetachInstance{"instance-1","detached-copy"}}});
    const auto detached=session.document();const auto detached_root=detached.objects.at("instance-1").children.front();
    check(detached.objects.at("instance-1").kind==Kind::group&&!detached.objects.at("instance-1").instance&&
        detached.objects.at(detached_root).kind==Kind::group,"Detach converts the placed Instance into an independent Group copy");
    const auto detached_values=evaluate(detached);const auto detached_transforms=evaluate_transforms(detached,detached_values);
    same_matrix(detached_transforms.at(detached_root).world,{1,0,0,1,35,22},
        "Detach excludes source-root affine and external parent while retaining Instance placement");
    check(detached.objects.at(detached_root).visible&&detached.objects.at(detached_root).compositing.opacity.literal==0.5,
        "Detach resets source-root visibility but preserves root compositing opacity");
    const auto detached_children=detached.objects.at(detached_root).children;
    const auto detached_path=detached_children[0];const auto detached_text=detached_children[1];
    near(detached.objects.at(detached_path).compositing.opacity.literal,0.3,
        "Detach bakes the current composite.opacity override into its copy");
    near(detached_values.at({detached_text,"","text.font_size"}),33,
        "Detach bakes the current Text font-size override into its copy");
    apply(session,{UpdateText{"text-b",default_text("text-source","Later source text")},
        Set{{"text-b","","text.font_size"},40},Set{{"source-root","","transform.tx"},800},
        SetVisibility{"source-root",false}});
    const auto after_source_edit=session.document();
    check(evaluated_text_source(after_source_edit,detached_text).content=="Source text"&&
        evaluate(after_source_edit).at({detached_text,"","text.font_size"})==33,
        "Source Text content and size edits after detach do not affect the materialized copy");
    const auto current_transforms=evaluate_transforms(after_source_edit,evaluate(after_source_edit));
    same_matrix(current_transforms.at(detached_root).world,{1,0,0,1,35,22},
        "Source-root affine and visibility edits after detach do not affect the copy");
    session.undo(session.revision());
    check(session.document()==detached,"Undo first reverses the later source edits exactly");
    session.undo(session.revision());
    check(session.document().objects.at("instance-1").kind==Kind::instance&&
        session.document().objects.at("instance-1").instance->overrides.size()==2,
        "Undo of detach restores the live Instance and both local overrides");
    session.redo(session.revision());
    check(session.document()==detached,"Redo restores the exact materialized Object IDs and values");
    session.redo(session.revision());
    check(session.document()==after_source_edit,"Undo/Redo traverses detach and following source edits atomically");

    auto native=encode(session.document());
    check(native.find("\"version\":\"0.73\"")!=std::string::npos&&decode(native)==session.document(),
        "Native 0.67 cold reopen retains Definitions, Instances and materialized copies");
    auto legacy=test_support::without_empty_macro_and_definition_fields_for_legacy_fixture(
        encode(empty_document("legacy-doc","legacy-comp","legacy-art")));
    const auto version=legacy.find("\"version\":\"0.73\"");
    check(version!=std::string::npos&&legacy.find("\"definitions\":")==std::string::npos&&
        legacy.find("\"macros\":")==std::string::npos,
        "Legacy fixture removes the native 0.64 Definition and 0.65 Macro collections");
    legacy.replace(version,std::string("\"version\":\"0.73\"").size(),"\"version\":\"0.63\"");
    check(decode(legacy)==empty_document("legacy-doc","legacy-comp","legacy-art"),
        "Native 0.63 without Definition/Instance fields remains cold-readable");
    auto lying=native;const auto current=lying.find("\"version\":\"0.73\"");
    lying.replace(current,std::string("\"version\":\"0.73\"").size(),"\"version\":\"0.63\"");
    rejects("UNKNOWN_FIELD",[&]{(void)decode(lying);});

    const auto api_base=fixture();Session api(api_base);
    const auto created=request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"create_definition","id":"api-definition","name":"API D","root":"source-root"},{"type":"create_instance","composition":"definition-comp","parent":"","id":"api-instance","definition":"api-definition","name":"API I"}]})");
    check(created.find("\"changed\":true")!=std::string::npos&&created.find("api-definition")!=std::string::npos,
        "JSON-lines API applies Definition and Instance through the shared Session mutation");
    check(request(api,R"({"op":"definitions"})").find("api-definition")!=std::string::npos&&
        request(api,R"({"op":"definition","id":"api-definition"})").find("source-root")!=std::string::npos,
        "API reads Definition records by stable ID");
    check(request(api,R"({"op":"inspect"})").find("\"instance\":{\"definition\":\"api-definition\"")!=std::string::npos,
        "API inspect exposes placed Instance IDs and local authored data");
    const auto api_delete_refusal=request(api,R"({"op":"apply","expected_revision":1,"commands":[{"type":"delete_definition","definition":"api-definition"}]})");
    check(api_delete_refusal.find("\"ok\":false")!=std::string::npos&&api_delete_refusal.find("DEFINITION_IN_USE")!=std::string::npos,
        "API reports explicit Definition delete refusal while an Instance uses it");
    check(request(api,R"({"op":"apply","expected_revision":1,"commands":[{"type":"rename_definition","definition":"api-definition","name":"Renamed API D"}]})")
        .find("\"changed\":true")!=std::string::npos&&request(api,R"({"op":"definition","id":"api-definition"})").find("Renamed API D")!=std::string::npos,
        "API renames Definition metadata while preserving its stable ID");
    const auto api_override=request(api,R"({"op":"apply","expected_revision":2,"commands":[{"type":"set_instance_override","instance":"api-instance","target":{"object":"path-a","point":"","field":"composite.opacity"},"value":0.35}]})");
    const auto api_override_inspect=request(api,R"({"op":"inspect"})");
    check(api_override.find("\"changed\":true")!=std::string::npos&&api_override_inspect.find("\"id\":\"api-instance\"")!=std::string::npos&&
        api_override_inspect.find("\"definition\":\"api-definition\"")!=std::string::npos&&
        api_override_inspect.find("\"field\":\"composite.opacity\"")!=std::string::npos,
        "API applies a supported Path opacity override and preserves exact Definition/Instance identity: "+api_override+" / "+api_override_inspect);
    near(api.document().objects.at("api-instance").instance->overrides.at(Ref{"path-a","","composite.opacity"}),0.35,
        "API readback retains the exact local override Scalar");
    check(request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"set_instance_override","instance":"api-instance","target":{"object":"path-a","point":"","field":"composite.opacity"},"value":0.8}]})")
        .find("REVISION_CONFLICT")!=std::string::npos&&api.revision()==3,
        "API stale revision rejects without changing the local override");
    check(request(api,R"({"op":"apply","expected_revision":3,"commands":[{"type":"reset_instance_override","instance":"api-instance","target":{"object":"path-a","point":"","field":"composite.opacity"}}]})")
        .find("\"changed\":true")!=std::string::npos&&request(api,R"({"op":"inspect"})").find("\"overrides\":[]")!=std::string::npos,
        "API resets a local override atomically to its source value");
    const auto api_detach=request(api,R"({"op":"apply","expected_revision":4,"commands":[{"type":"detach_instance","instance":"api-instance","id_prefix":"api-detached"}]})");
    const auto detached_inspect=request(api,R"({"op":"inspect"})");
    const auto api_instance_pos=detached_inspect.find("\"id\":\"api-instance\"");
    check(api_detach.find("\"changed\":true")!=std::string::npos&&api_instance_pos!=std::string::npos&&
        detached_inspect.find("\"kind\":\"group\"",api_instance_pos)!=std::string::npos&&
        detached_inspect.find("\"instance\":",api_instance_pos)==std::string::npos,
        "API detaches the exact Instance ID into an independent Group: "+api_detach+" / "+detached_inspect);
}

void validation_and_malformed_input() {
    auto document=fixture();
    document.objects.at("path-a").contours.front().points.front().x.binding=
        Binding{{"external","external-p0","x"},1,0,"copy_local_value"};
    document.objects.emplace("external",rectangle("external"));document.compositions.front().roots.push_back("external");
    Session escaping(document);
    rejects("DEFINITION_DEPENDENCY_ESCAPE",[&]{apply(escaping,{DefinitionCommand{CreateDefinition{{"bad-definition","Bad","source-root"}}}});});
    check(escaping.revision()==0&&!escaping.document().definitions.contains("bad-definition"),
        "Escaping source dependency rejects without committing a partial Definition");

    Session session(fixture());apply(session,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        DefinitionCommand{CreateInstance{"definition-comp","","instance","definition","I"}}});
    rejects("UNSUPPORTED_OVERRIDE",[&]{apply(session,{DefinitionCommand{SetInstanceOverride{"instance",{"text-b","","text.content"},4}}});});
    rejects("DANGLING_OVERRIDE",[&]{apply(session,{DefinitionCommand{SetInstanceOverride{"instance",{"missing","","composite.opacity"},0.4}}});});
    rejects("INVALID_OVERRIDE",[&]{apply(session,{DefinitionCommand{SetInstanceOverride{"instance",{"path-a","point-id","composite.opacity"},0.4}}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({DefinitionCommand{RenameDefinition{"definition","Stale"}}},session.revision()+1);});
    auto dangling=session.document();dangling.definitions.at("definition").root="missing-root";
    rejects("MISSING_DEFINITION_ROOT",[&]{validate(dangling);});
    auto wrong_type=session.document();wrong_type.objects.at("instance").instance->overrides[Ref{"path-a","","text.font_size"}]=20;
    rejects("UNSUPPORTED_OVERRIDE",[&]{validate(wrong_type);});
    const auto unchanged=session.document();const auto unchanged_revision=session.revision();
    const auto unchanged_state=session.history().current_id;
    rejects("UNSUPPORTED_OVERRIDE",[&]{apply(session,{DefinitionCommand{SetInstanceOverride{"instance",{"source-root","","text.font_size"},20}}});});
    check(session.document()==unchanged&&session.revision()==unchanged_revision&&session.history().current_id==unchanged_state,
        "A supported field on an existing source item without that typed property rejects atomically");

    Session source_delete(fixture());
    apply(source_delete,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        DefinitionCommand{CreateInstance{"definition-comp","","instance","definition","I"}},
        DefinitionCommand{SetInstanceOverride{"instance",{"path-a","","composite.opacity"},0.4}}});
    const auto before_source_delete=source_delete.document();const auto source_delete_revision=source_delete.revision();
    const auto source_delete_state=source_delete.history().current_id;
    rejects("DANGLING_OVERRIDE",[&]{apply(source_delete,{DeleteObjects{{"path-a"}}});});
    check(source_delete.document()==before_source_delete&&source_delete.revision()==source_delete_revision&&
        source_delete.history().current_id==source_delete_state,
        "Deleting a source item used by a live override is rejected without document, revision or history changes");

    auto collision_document=fixture();Object colliding;colliding.id="collision-1";colliding.name="Collision";colliding.kind=Kind::group;
    collision_document.objects.emplace(colliding.id,colliding);
    collision_document.compositions.front().roots.push_back("collision-1");
    Session collision(collision_document);apply(collision,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        DefinitionCommand{CreateInstance{"definition-comp","","instance","definition","I"}}});
    const auto before_collision=collision.document();const auto collision_revision=collision.revision();
    const auto collision_state=collision.history().current_id;
    rejects("DUPLICATE_ID",[&]{apply(collision,{DefinitionCommand{DetachInstance{"instance","collision"}}});});
    check(collision.document()==before_collision&&collision.revision()==collision_revision&&
        collision.history().current_id==collision_state,
        "Detach ID-prefix collision rejects without partial materialization or history mutation");

    auto render_limit=fixture();render_limit.objects.erase("outer");render_limit.compositions.front().roots.erase(
        std::remove(render_limit.compositions.front().roots.begin(),render_limit.compositions.front().roots.end(),"outer"),
        render_limit.compositions.front().roots.end());
    render_limit.objects.at("source-root").transform_parent.reset();
    render_limit.definitions.emplace("definition",Definition{"definition","D","source-root"});
    Object render_instance;render_instance.id="instance";render_instance.name="I";render_instance.kind=Kind::instance;
    render_instance.instance=DefinitionInstance{"definition",{}};render_limit.objects.emplace(render_instance.id,render_instance);
    render_limit.compositions.front().roots.push_back(render_instance.id);
    for(std::size_t i=0;i<9996;++i) {
        Object filler;filler.id="filler-"+std::to_string(i);filler.name="F";filler.kind=Kind::group;
        render_limit.objects.emplace(filler.id,filler);render_limit.compositions.front().roots.push_back(filler.id);
    }
    check(render_limit.objects.size()==10000,"Render ceiling fixture reaches the authored Document Object limit");
    rejects("INSTANCE_RENDER_LIMIT",[&]{(void)scene(render_limit);});

    apply(session,{DefinitionCommand{SetInstanceOverride{"instance",{"path-a","","composite.opacity"},0.25}}});
    auto malformed=encode(session.document());
    const auto list=malformed.find("\"overrides\":[");check(list!=std::string::npos,"Native Instance has serialized override array");
    const auto begin=list+std::string("\"overrides\":[").size();const auto end=malformed.find(']',begin);
    const auto one=malformed.substr(begin,end-begin);malformed.insert(end,","+one);
    rejects("DUPLICATE_OVERRIDE_KEY",[&]{(void)decode(malformed);});

    auto duplicate_id=fixture();duplicate_id.definitions.emplace("path-a",Definition{"path-a","Duplicate","source-root"});
    rejects("DUPLICATE_ID",[&]{validate(duplicate_id);});
    auto excessive=fixture();
    for(std::size_t i=0;i<10001;++i) {
        const auto id="definition-limit-"+std::to_string(i);
        excessive.definitions.emplace(id,Definition{id,"D","source-root"});
    }
    rejects("LIMIT",[&]{validate(excessive);});
    auto second=fixture();auto other=empty_document("other-doc","other-comp","other-art");
    second.compositions.push_back(other.compositions.front());second.objects.emplace("foreign-root",rectangle("foreign-root"));
    second.compositions.back().roots.push_back("foreign-root");
    second.definitions.emplace("definition",Definition{"definition","D","source-root"});
    Object instance;instance.id="foreign-instance";instance.name="Foreign";instance.kind=Kind::instance;
    instance.instance=DefinitionInstance{"definition",{}};second.objects.emplace(instance.id,instance);
    second.compositions.back().roots.push_back(instance.id);
    rejects("CROSS_COMPOSITION",[&]{validate(second);});
}
}

int main() {
    try {
        source_projection_and_overrides();detach_history_native_api_and_export();validation_and_malformed_input();
        std::cout<<"PASS "<<checks<<" Definition/Instance core, projection, API, native and history checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
