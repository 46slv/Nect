#include "nect/core.hpp"
#include <algorithm>
#include <cmath>
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
    document.objects.at("b").transform_parent="a";matrix(document.objects.at("b"),{1,0,0,1,15,20});
    Session session(document);const auto before=transforms(document);apply(session,{PutInside{"comp","","group",{"a","b"}}});const auto after=transforms(session.document());
    check(session.document().objects.at("group").children==std::vector<Id>{"a","b","source"},"Put Inside inserts moved roots before existing children");
    for(const auto* id:{"a","b","source"})same_matrix(before.at(id).world,after.at(id).world);
    check(session.document().objects.at("b").transform==document.objects.at("b").transform&&session.document().objects.at("b").transform_parent==Id{"a"},"Explicit Transform Parent and local Scalars stay unchanged");
    const auto moved=session.document();session.undo(session.revision());check(session.document()==document,"Put Inside Undo restores Structure and canonical affine values");session.redo(session.revision());check(session.document()==moved,"Put Inside Redo restores exact state");
    Session invalid(document);atomic(invalid,"NONCONTIGUOUS_GROUP",{PutInside{"comp","","group",{"a"}}});
    auto driven=document;driven.objects.at("a").transform[0].expression=Expression{"1"};Session linked(driven);atomic(linked,"DRIVEN_PROPERTY",{PutInside{"comp","","group",{"a","b"}}});
    auto singular=document;matrix(singular.objects.at("group"),{0,0,0,1,0,0});Session collapsed(singular);atomic(collapsed,"SINGULAR_TRANSFORM",{PutInside{"comp","","group",{"a","b"}}});
    auto coupled=document;matrix(coupled.objects.at("group"),identity_matrix);coupled.objects.at("group").transform[4].binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};Session dependent(coupled);
    atomic(dependent,"TRANSFORM_PRESERVATION",{PutInside{"comp","","group",{"a","b"}}});
}
}
int main() {
    try{scene_contract();mask_geometry_and_validation();mask_with_and_put_inside();std::cout<<"PASS "<<checks<<" compositing scene, mask, visibility and structure checks\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
