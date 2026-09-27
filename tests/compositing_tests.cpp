#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);++checks;}
void near(double a,double b,const char* message){check(std::abs(a-b)<1e-8,message);}
template<class F>void rejects(const char* code,F action) {
    try{action();}catch(const Error& error){check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
void atomic(Session& session,const char* code,std::vector<Command> commands) {
    const auto before=session.document();const auto rev=session.revision();const auto history=session.history();
    rejects(code,[&]{apply(session,std::move(commands));});
    check(session.document()==before&&session.revision()==rev&&session.history()==history,"Rejected composite command is atomic");
}
void atomic_reason(Session& session,const char* code,const char* reason,std::vector<Command> commands) {
    const auto before=session.document();const auto rev=session.revision();const auto history=session.history();
    try{apply(session,std::move(commands));}
    catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        check(std::string(error.what()).find(reason)!=std::string::npos,"Appearance refusal names boundary: "+std::string(error.what()));
        check(session.document()==before&&session.revision()==rev&&session.history()==history,"Appearance refusal retains document, revision and history");
        return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
Object rectangle(Id id,double x=0,double y=0) {
    Object object;object.id=id;object.name=id;Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(const auto& xy:std::vector<Vec2>{{x,y},{x+40,y},{x+40,y+30},{x,y+30}}) {
        Point p;p.id=id+"-p"+std::to_string(contour.points.size());p.x.literal=xy.x;p.y.literal=xy.y;contour.points.push_back(p);
    }
    object.contours.push_back(contour);object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));return object;
}
void matrix(Object& object,Affine a){for(std::size_t i=0;i<6;++i)object.transform[i].literal=a[i];}
Document fixture() {
    auto document=empty_document("doc","comp","art");
    for(const auto* id:{"a","b","source"}){document.objects.emplace(id,rectangle(id));document.compositions[0].roots.push_back(id);}return document;
}
EvaluatedScene scene(const Document& document){const auto values=evaluate(document);return evaluate_scene(document,"comp",values,evaluate_transforms(document,values));}
std::map<Id,EvaluatedTransform> transforms(const Document& document){return evaluate_transforms(document,evaluate(document));}
void same_matrix(const Affine& a,const Affine& b){for(std::size_t i=0;i<6;++i)near(a[i],b[i],"World placement preserved");}
std::vector<Id> drawable_order(const Document& document,const EvaluatedScene& evaluated) {
    std::vector<Id> result;
    std::function<void(const EvaluatedSceneNode&)> append=[&](const EvaluatedSceneNode& node) {
        if(document.objects.at(node.id).kind==Kind::group)for(const auto& child:node.children)append(child);
        else result.push_back(node.id);
    };
    for(const auto& root:evaluated.roots)append(root);
    return result;
}

void create_empty_folder() {
    auto document=fixture();document.compositions.front().roots={"source","a","b"};
    document.collections={{"collection","Stable members",{"a","b"}}};
    document.objects.at("b").transform[4].binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};
    Session session(document);const auto before=session.document();const auto before_values=evaluate(before);
    const auto before_transforms=transforms(before);const auto before_scene=scene(before);
    const auto response=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"create_folder","composition":"comp","parent":"","id":"folder","name":"Folder"}]})");
    check(response.find("\"changed\":true")!=std::string::npos,"JSON-lines API accepts Create Folder through the shared Session");
    const auto& composition=session.document().compositions.front();
    check(composition.roots==std::vector<Id>{"source","a","b","folder"},"Create Folder appends after existing roots");
    const auto& folder=session.document().objects.at("folder");
    check(folder.kind==Kind::group&&folder.name=="Folder"&&folder.children.empty()&&folder.stack.empty()&&
        folder.compositing==Compositing{}&&!folder.source&&!folder.text&&!folder.image&&!folder.transform_parent&&folder.visible,
        "New Folder is an empty neutral Group in the existing native model");
    check(session.document().collections==before.collections,"Folder creation does not change Collection membership");
    for(const auto& [id,object]:before.objects)check(session.document().objects.at(id)==object,"Existing authored object bytes remain unchanged");
    const auto after_values=evaluate(session.document());const auto after_transforms=transforms(session.document());
    check(after_transforms.at("folder").world==identity_matrix,"New Folder starts with an identity transform");
    for(const auto& [ref,value]:before_values)check(after_values.at(ref)==value,"Existing property values and stable references remain unchanged");
    for(const auto& [id,transform]:before_transforms)check(after_transforms.at(id).world==transform.world,"Existing world transforms remain unchanged");
    const auto after_scene=scene(session.document());
    check(drawable_order(before,before_scene)==drawable_order(session.document(),after_scene),
        "An empty Folder adds no drawable content and preserves paint order");
    check(decode(encode(session.document()))==session.document(),"Folder survives native 0.20 encode/decode exactly");
    const auto created=session.document();const auto created_revision=session.revision();const auto created_history=session.history();
    rejects("REVISION_CONFLICT",[&]{session.apply({CreateFolder{"comp","","stale-folder","Stale"}},0);});
    check(session.document()==created&&session.revision()==created_revision&&session.history()==created_history,
        "Stale Create Folder revision rejects without an authored delta");
    session.undo(session.revision());check(session.document()==before,"Create Folder has one exact Undo boundary");
    session.redo(session.revision());check(session.document()==created,"Create Folder has one exact Redo boundary");
    check(!created_history.states.empty()&&created_history.states.back().label=="Create Folder: Folder","History names the new Folder action");

    const auto before_move_values=evaluate(created);const auto before_move_transforms=transforms(created);
    const auto before_move_scene=scene(created);
    apply(session,{PutInside{"comp","","folder",{"a","b"}}});
    const auto moved=session.document();
    check(moved.compositions.front().roots==std::vector<Id>{"source","folder"}&&
        moved.objects.at("folder").children==std::vector<Id>{"a","b"},
        "Put Inside moves the immediately preceding A/B block into the new Folder in paint order");
    for(const auto* id:{"a","b"}) {
        check(moved.objects.at(id)==created.objects.at(id),"Put Inside retains A/B stable authored object state");
        check(transforms(moved).at(id).world==before_move_transforms.at(id).world,"Put Inside preserves A/B world transforms");
    }
    const auto moved_values=evaluate(moved);
    for(const auto& [ref,value]:before_move_values)check(moved_values.at(ref)==value,"Put Inside preserves stable property references and values");
    check(drawable_order(created,before_move_scene)==drawable_order(moved,scene(moved)),
        "Put Inside preserves the existing drawable paint order");
    check(decode(encode(moved))==moved,"Folder organization survives native 0.20 encode/decode exactly");
    session.undo(session.revision());check(session.document()==created,"Put Inside into Folder is one exact Undo");
    session.redo(session.revision());check(session.document()==moved,"Put Inside into Folder is one exact Redo");

    atomic(session,"DUPLICATE_ID",{CreateFolder{"comp","","a","Duplicate"}});
    atomic(session,"DUPLICATE_ID",{CreateFolder{"comp","","a-p0","Point collision"}});
    atomic(session,"MISSING_COMPOSITION",{CreateFolder{"missing","","missing-comp","Missing composition"}});
    atomic(session,"INVALID_PARENT",{CreateFolder{"comp","missing-parent","missing-parent-child","Missing parent"}});
    auto separate=fixture();Object foreign;foreign.id="foreign-parent";foreign.name="Foreign";foreign.kind=Kind::group;
    separate.objects.emplace(foreign.id,foreign);separate.compositions.push_back(Composition{"other","Other",{"foreign-parent"},{}});
    Session planes(separate);atomic(planes,"INVALID_PARENT",{CreateFolder{"comp","foreign-parent","foreign-child","Foreign parent"}});
}

void scene_contract() {
    Session session(fixture());auto evaluated=scene(session.document());check(!evaluated.requires_compositing&&evaluated.roots.size()==3,"Neutral scene retains direct rendering");
    apply(session,{GroupContiguous{"comp","",{"a","b"},"group","Group"}});
    evaluated=scene(session.document());check(!evaluated.roots[0].isolated&&evaluated.roots[0].children[0].id=="a"&&evaluated.roots[0].children[1].id=="b","Neutral group preserves paint order and pass-through boundary");
    apply(session,{SetExpression{{{"group","","composite.opacity"}},{".5"}}});
    evaluated=scene(session.document());check(evaluated.roots[0].isolated&&evaluated.roots[0].opacity==.5,"Opacity is an ordinary expression-capable Scalar and aggregate boundary");
    atomic(session,"DRIVEN_PROPERTY",{Set{{"group","","composite.opacity"},.8}});
    apply(session,{Unlink{{"group","","composite.opacity"}},Set{{"group","","composite.opacity"},1}});
    for(const auto* blend:{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn","hard-light","soft-light","difference","exclusion"}) {
        apply(session,{SetCompositing{"group",blend,false}});evaluated=scene(session.document());
        check(evaluated.roots[0].blend==blend&&evaluated.roots[0].isolated==(std::string(blend)!="normal"),"Supported blend resolves exact isolation semantics");
    }
    atomic(session,"UNSUPPORTED_BLEND",{SetCompositing{"group","plus",false}});
    atomic(session,"OUT_OF_RANGE",{Set{{"group","","composite.opacity"},1.1}});
    apply(session,{SetCompositing{"group","normal",true}});check(scene(session.document()).roots[0].isolated,"Explicit normal isolation remains meaningful");
    apply(session,{SetVisibility{"group",false}});evaluated=scene(session.document());
    check(!evaluated.roots[0].visible&&evaluated.shapes.contains("a"),"Hidden tree retains editable evaluated leaf geometry");
}

void mask_geometry_and_validation() {
    auto document=fixture();auto& source=document.objects.at("source");source.visible=false;source.compositing.opacity.literal=0;
    source.stack[0].parameters.at("a").literal=0;source.contours.front().closed=false;
    auto repeat=default_operation("source-repeat","nect.shape.repeater");repeat.parameters.at("copies").literal=2;repeat.parameters.at("position_x").literal=50;source.stack.push_back(repeat);
    Object parent;parent.id="parent";parent.name="Parent";parent.kind=Kind::group;matrix(parent,{0,2,-3,0,200,30});
    document.objects.emplace(parent.id,parent);document.compositions[0].roots.push_back(parent.id);source.transform_parent=parent.id;matrix(source,{1,0,0,1,10,5});
    document.objects.at("a").compositing.mask=GeometryMask{"mask","source",1,true,"evenodd"};
    Session session(document);const auto evaluated=scene(document);const auto& mask=*evaluated.roots[0].mask;
    check(mask.source=="source"&&mask.fill_rule=="evenodd"&&mask.paths.size()==2,"Mask uses repeated geometry despite hidden/transparent source appearance");
    check(mask.paths.front().contours==evaluated.shapes.at("source").paths.front().contours,"Source artwork and mask share one evaluated contour allocation");
    auto point=map_point(mask.paths.front().transform,mask.paths.front().contours->front().points.front().anchor);
    near(point.x,185,"Mask source world X respects external parenting");near(point.y,50,"Mask source world Y respects external parenting");
    point=map_point(mask.paths.back().transform,{0,0});near(point.x,185,"Repeat local X maps through rotated world basis");near(point.y,150,"Repeated mask uses final source geometry");
    check(!mask.paths.front().contours->front().closed,"Implicit mask closure never rewrites authored open contours");
    atomic(session,"MISSING_MASK_SOURCE",{DeleteObjects{{"source"}}});
    atomic(session,"INVALID_MASK_SOURCE",{SetMask{"a",GeometryMask{"other-mask","a"}}});
    atomic(session,"INVALID_MASK_SOURCE",{SetMask{"a",GeometryMask{"other-mask","parent"}}});
    atomic(session,"DUPLICATE_ID",{SetMask{"b",GeometryMask{"mask","source"}}});
    atomic(session,"UNSUPPORTED_MASK_VERSION",{SetMask{"a",GeometryMask{"mask","source",2}}});
    atomic(session,"UNSUPPORTED_FILL_RULE",{SetMask{"a",GeometryMask{"mask","source",1,true,"inverse"}}});
    auto disabled=*session.document().objects.at("a").compositing.mask;disabled.enabled=false;apply(session,{SetMask{"a",disabled}});
    check(!scene(session.document()).roots[0].mask,"Bypass removes only the evaluated mask");
    atomic(session,"MISSING_MASK_SOURCE",{DeleteObjects{{"source"}}});
    apply(session,{DeleteObjects{{"a","source"}}});check(!session.document().objects.contains("source"),"Deleting source together with its owner is valid");
    auto foreign=fixture();foreign.compositions.push_back({"other","Other",{"source"},{{"other-art","Page"}}});foreign.compositions[0].roots.pop_back();Session planes(foreign);
    atomic(planes,"CROSS_COMPOSITION",{SetMask{"a",GeometryMask{"mask","source"}}});
}

void mask_with_and_put_inside() {
    for(const bool top:{false,true}) {
        auto document=fixture();document.collections={{"collection","Selection",{"a","b"}}};Session session(document);
        apply(session,{MaskObjects{"comp","",{"a","b"},"group","mask","Masked",top}});
        const auto& group=session.document().objects.at("group");const auto source=top?"b":"a";
        check(group.children==std::vector<Id>{"a","b"}&&group.compositing.mask->source==source&&!session.document().objects.at(source).visible,"Mask With preserves order and hides the selected source");
        check(session.document().collections==document.collections,"Mask grouping preserves Collection membership");
        session.undo(session.revision());check(session.document()==document,"Mask With is one complete Undo including visibility");
    }
    auto document=fixture();Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"source"};matrix(group,{0,2,-3,0,200,30});
    document.objects.emplace(group.id,group);document.compositions[0].roots={"a","b","group"};matrix(document.objects.at("a"),{1,0,0,1,100,120});
    document.objects.at("a").compositing.opacity.literal=.5;document.objects.at("a").compositing.blend="multiply";
    document.objects.at("b").transform_parent="a";matrix(document.objects.at("b"),{1,0,0,1,15,20});
    Session session(document);const auto before=transforms(document);const auto source_appearance=document.objects.at("a").compositing;
    apply(session,{PutInside{"comp","","group",{"a","b"}}});const auto after=transforms(session.document());
    check(session.document().objects.at("group").children==std::vector<Id>{"a","b","source"},"Put Inside inserts moved roots before existing children");
    for(const auto* id:{"a","b","source"})same_matrix(before.at(id).world,after.at(id).world);
    check(session.document().objects.at("a").compositing==source_appearance,"Neutral destination allows and retains source sibling compositing");
    check(session.document().objects.at("b").transform==document.objects.at("b").transform&&session.document().objects.at("b").transform_parent==Id{"a"},"Explicit Transform Parent and local Scalars stay unchanged");
    const auto moved=session.document();session.undo(session.revision());check(session.document()==document,"Put Inside Undo restores Structure and canonical affine values");session.redo(session.revision());check(session.document()==moved,"Put Inside Redo restores exact state");
    Session invalid(document);atomic(invalid,"NONCONTIGUOUS_GROUP",{PutInside{"comp","","group",{"a"}}});
    auto hidden_invalid=document;hidden_invalid.objects.at("group").visible=false;Session hidden_selection(hidden_invalid);
    atomic(hidden_selection,"NONCONTIGUOUS_GROUP",{PutInside{"comp","","group",{"a"}}});
    auto driven=document;driven.objects.at("a").transform[0].expression=Expression{"1"};Session linked(driven);atomic(linked,"DRIVEN_PROPERTY",{PutInside{"comp","","group",{"a","b"}}});
    auto singular=document;matrix(singular.objects.at("group"),{0,0,0,1,0,0});Session collapsed(singular);atomic(collapsed,"SINGULAR_TRANSFORM",{PutInside{"comp","","group",{"a","b"}}});
    auto coupled=document;matrix(coupled.objects.at("group"),identity_matrix);coupled.objects.at("group").transform[4].binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};Session dependent(coupled);
    atomic(dependent,"TRANSFORM_PRESERVATION",{PutInside{"comp","","group",{"a","b"}}});

    auto appearance_refusal=[&](auto change,const char* reason) {
        auto unsafe=document;change(unsafe.objects.at("group"));Session blocked(unsafe);
        atomic_reason(blocked,"PUT_INSIDE_APPEARANCE",reason,{PutInside{"comp","","group",{"a","b"}}});
    };
    appearance_refusal([](Object& g){g.visible=false;},"destination Group is hidden");
    appearance_refusal([](Object& g){g.compositing.opacity.literal=.5;},"non-neutral opacity");
    appearance_refusal([](Object& g){g.compositing.opacity.expression=Expression{"1"};},"opacity is driven");
    appearance_refusal([](Object& g){g.compositing.opacity.binding=Binding{{"source","","composite.opacity"},1,0,"copy_local_value"};},"opacity is driven");
    appearance_refusal([](Object& g){g.compositing.blend="multiply";},"non-normal blend mode");
    appearance_refusal([](Object& g){g.compositing.isolated=true;},"destination Group is isolated");
    appearance_refusal([](Object& g){g.compositing.mask=GeometryMask{"group-mask","source"};},"destination Group has a mask");

    auto api_document=document;api_document.objects.at("group").visible=false;Session api(api_document);
    const auto api_before=api.document();const auto api_history=api.history();
    const auto response=request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"put_inside","composition":"comp","parent":"","group":"group","members":["a","b"]}]})");
    check(response.find("\"code\":\"PUT_INSIDE_APPEARANCE\"")!=std::string::npos&&
        response.find("Put Inside destination Group is hidden")!=std::string::npos,
        "JSON-lines Put Inside reports the same scoped appearance refusal and reason");
    check(api.document()==api_before&&api.revision()==0&&api.history()==api_history,
        "JSON-lines appearance refusal retains document, revision and history");
}
void neutral_ungroup() {
    auto document=fixture();Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"a","b"};matrix(group,{0,2,-3,0,200,30});
    document.objects.emplace(group.id,group);document.compositions[0].roots={"group","source"};document.collections={{"collection","Members",{"group","a"}}};
    document.objects.at("b").transform_parent="source";matrix(document.objects.at("b"),{1,0,0,1,15,20});
    Session session(document);const auto before=transforms(document);apply(session,{Ungroup{"comp","","group"}});const auto after=transforms(session.document());
    check(session.document().compositions[0].roots==std::vector<Id>{"a","b","source"}&&!session.document().objects.contains("group"),"Ungroup replaces exact stacking slot with children");
    for(const auto* id:{"a","b","source"}){same_matrix(before.at(id).world,after.at(id).world);check(session.document().objects.at(id).contours==document.objects.at(id).contours,"Ungroup preserves stable authored geometry");}
    check(session.document().objects.at("b")==document.objects.at("b"),"Explicit external child parent and local state remain intact");
    check(session.document().collections[0].members==std::vector<Id>{"a"},"Only removed Group collection membership pruned");
    const auto ungrouped=session.document();session.undo(session.revision());check(session.document()==document,"Ungroup exact Undo");session.redo(session.revision());check(session.document()==ungrouped,"Ungroup exact Redo");
    for(int case_id=0;case_id<5;++case_id){auto d=document;auto& g=d.objects.at("group");if(case_id==0)g.visible=false;if(case_id==1)g.compositing.opacity.literal=.5;if(case_id==2)g.compositing.blend="multiply";if(case_id==3)g.compositing.isolated=true;if(case_id==4)g.compositing.mask=GeometryMask{"mask","source"};Session blocked(d);atomic(blocked,"UNGROUP_APPEARANCE",{Ungroup{"comp","","group"}});}
    auto dynamic=document;dynamic.objects.at("group").transform[4].expression=Expression{"200"};Session driven_group(dynamic);atomic(driven_group,"UNGROUP_DYNAMIC",{Ungroup{"comp","","group"}});
    dynamic=document;dynamic.objects.at("group").transform_parent="source";Session followed(dynamic);atomic(followed,"UNGROUP_DYNAMIC",{Ungroup{"comp","","group"}});
    auto linked=document;linked.objects.at("source").contours[0].points[0].x.binding=Binding{{"group","","transform.tx"},1,0};Session referenced(linked);atomic(referenced,"MISSING_REFERENCE",{Ungroup{"comp","","group"}});
    linked=document;linked.objects.at("b").transform_parent="group";Session parented(linked);atomic(parented,"MISSING_TRANSFORM_PARENT",{Ungroup{"comp","","group"}});
    linked=document;linked.objects.at("a").transform[0].expression=Expression{"1"};Session driven_child(linked);atomic(driven_child,"DRIVEN_PROPERTY",{Ungroup{"comp","","group"}});
    linked=document;linked.objects.at("a").contours[0].points[0].x.binding=Binding{{"a","","transform.tx"},1,0};Session geometry_dependency(linked);atomic(geometry_dependency,"UNGROUP_DEPENDENCY",{Ungroup{"comp","","group"}});
    auto singular=document;matrix(singular.objects.at("group"),{0,0,0,1,20,30});Session collapsed(singular);const auto old=transforms(singular);apply(collapsed,{Ungroup{"comp","","group"}});same_matrix(old.at("a").world,transforms(collapsed.document()).at("a").world);
    auto nested=document;Object outer;outer.id="outer";outer.name="Outer";outer.kind=Kind::group;outer.children={"group"};matrix(outer,{2,0,0,2,50,60});nested.objects.emplace("outer",outer);nested.compositions[0].roots={"outer","source"};Session nesting(nested);const auto prior=transforms(nested);apply(nesting,{Ungroup{"comp","outer","group"}});same_matrix(prior.at("a").world,transforms(nesting.document()).at("a").world);check(nesting.document().objects.at("outer").children==std::vector<Id>{"a","b"},"Nested Group unwrap preserves outer container");
}

}
int main() {
    try{create_empty_folder();scene_contract();mask_geometry_and_validation();mask_with_and_put_inside();neutral_ungroup();std::cout<<"PASS "<<checks<<" compositing scene, mask, visibility and structure checks\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
