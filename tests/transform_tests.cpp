#include "nect/core.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

using namespace nect;
namespace {
int checks=0;
void check(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);++checks;}
void near(double actual,double expected,const std::string& message) {
    check(std::abs(actual-expected)<1e-8,message+": expected "+std::to_string(expected)+", got "+std::to_string(actual));
}
void matrix_near(const Affine& actual,const Affine& expected,const std::string& message) {
    for(std::size_t i=0;i<actual.size();++i)near(actual[i],expected[i],message+" component "+std::to_string(i));
}
template<class F> void rejects(const char* code,F action) {
    try {action();}catch(const Error& error){check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
void atomic_reject(Session& session,const char* code,std::vector<Command> commands) {
    const auto before=session.document();const auto revision=session.revision();const auto history=session.history();
    rejects(code,[&]{apply(session,std::move(commands));});
    check(session.document()==before&&session.revision()==revision&&session.history()==history,"Rejected transform preserves document, revision and History");
}
Ref tf(const Id& object,const std::string& field){return {object,"","transform."+field};}
void matrix(Object& object,const Affine& values){for(std::size_t i=0;i<values.size();++i)object.transform[i].literal=values[i];}
Object path(const Id& id,Vec2 first={0,0},Vec2 second={20,10}) {
    Object object;object.id=id;object.name=id;
    Point a;a.id=id+"-first";a.x.literal=first.x;a.y.literal=first.y;
    Point b;b.id=id+"-last";b.x.literal=second.x;b.y.literal=second.y;
    object.contours={{id+"-contour",false,{a,b}}};return object;
}
Document plain() {
    auto document=empty_document("document","composition","artboard");
    document.objects.emplace("path",path("path",{10,20},{30,40}));document.objects.emplace("driver",path("driver"));
    document.compositions.front().roots={"path","driver"};return document;
}
Document hierarchy() {
    auto document=empty_document("document","composition","artboard");
    Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"child"};matrix(group,{2,0,0,2,100,30});
    auto child=path("child");matrix(child,{1,0,0,1,10,20});child.anchor={Scalar{7,{}},Scalar{9,{}}};
    auto parent=path("parent");matrix(parent,{0,1,-1,0,300,50});
    document.objects={{"group",group},{"child",child},{"parent",parent}};document.compositions.front().roots={"group","parent"};
    return document;
}
std::map<Id,EvaluatedTransform> transforms(const Document& document){return evaluate_transforms(document,evaluate(document));}
std::optional<Bounds> bounds(const Document& document,const Id& id) {
    const auto values=evaluate(document);return object_bounds(document,id,values,evaluate_transforms(document,values));
}
void box(const std::optional<Bounds>& actual,const Bounds& expected,const std::string& message) {
    check(actual.has_value(),message+" exists");near(actual->left,expected.left,message+" left");near(actual->top,expected.top,message+" top");
    near(actual->right,expected.right,message+" right");near(actual->bottom,expected.bottom,message+" bottom");
}

void anchor_and_relative_edits() {
    auto document=plain();matrix(document.objects.at("path"),{2,1,.5,3,7,-4});
    Session session(document);const auto original=transforms(session.document()).at("path").world;
    apply(session,{Set{tf("path","anchor_x"),20},Set{tf("path","anchor_y"),25}});
    matrix_near(transforms(session.document()).at("path").world,original,"Anchor-only edit leaves full world placement unchanged");
    check(property_unit(tf("path","anchor_x"))=="du","Anchor has normal distance unit");
    const auto refs=properties(session.document());check(std::find(refs.begin(),refs.end(),tf("path","anchor_y"))!=refs.end(),"Anchor is a discoverable Scalar");
    apply(session,{SetPosition{"path",100,200}});
    matrix_near(transforms(session.document()).at("path").local,{2,1,.5,3,47.5,105},"Position is the mapped Anchor, not the affine translation column");
    const auto positioned=session.document();
    apply(session,{TransformAroundAnchor{"path",90,2,-1}});
    const Affine expected{-2,4,3,-.5,65,132.5};const auto world=transforms(session.document()).at("path").world;
    matrix_near(world,expected,"Parent-space relative rotation and local-axis scale order");
    const auto pivot=map_point(world,{20,25});near(pivot.x,100,"Rotated Anchor X stays placed");near(pivot.y,200,"Rotated Anchor Y stays placed");
    const auto end=map_point(world,{30,40});near(end.x,125,"Hand-computed transformed endpoint X");near(end.y,232.5,"Hand-computed transformed endpoint Y");
    const auto rotated=session.document();session.undo(session.revision());check(session.document()==positioned,"One Undo restores full relative transform");
    session.redo(session.revision());check(session.document()==rotated,"Redo restores the authored matrix exactly");
    apply(session,{TransformAroundAnchor{"path",0,0,1}});
    rejects("SINGULAR_TRANSFORM",[&]{(void)inverse_affine(transforms(session.document()).at("path").world);});
    apply(session,{SetPosition{"path",90,80}});
    const auto collapsed=map_point(transforms(session.document()).at("path").world,{20,25});
    near(collapsed.x,90,"Singular object can still change Position without inversion");near(collapsed.y,80,"Singular Position Y");
    atomic_reject(session,"NON_FINITE",{TransformAroundAnchor{"path",std::numeric_limits<double>::infinity(),1,1}});
    atomic_reject(session,"OUT_OF_RANGE",{SetPosition{"path",1e20,0}});
}

void driven_axes_and_anchor_links() {
    auto document=plain();document.objects.at("driver").contours.front().points.front().x.literal=20;
    document.objects.at("path").anchor[0].binding=Binding{{"driver","driver-first","x"},1,0,"copy_local_value"};
    document.objects.at("path").transform[4].binding=Binding{{"driver","driver-first","x"},1,0,"copy_local_value"};
    Session session(document);
    apply(session,{CenterAnchor{"path"}});
    check(session.document().objects.at("path").anchor[0].binding.has_value(),"Center Anchor leaves an already-correct driven axis linked");
    near(evaluate(session.document()).at(tf("path","anchor_y")),30,"Center Anchor changes only the required free axis");
    apply(session,{SetPosition{"path",40,80}});
    check(property(session.document(),tf("path","tx")).binding.has_value(),"Position preserves unchanged driven translation axis");
    near(evaluate(session.document()).at(tf("path","ty")),50,"Position updates the independent free translation axis");
    atomic_reject(session,"DRIVEN_PROPERTY",{SetPosition{"path",41,80}});
    atomic_reject(session,"DRIVEN_PROPERTY",{TransformAroundAnchor{"path",90,1,1}});
    atomic_reject(session,"DRIVEN_PROPERTY",{Set{tf("path","anchor_x"),10}});
    atomic_reject(session,"UNIT_MISMATCH",{Link{tf("path","anchor_y"),{tf("driver","a"),1,0,"copy_local_value"}}});
    apply(session,{Set{{"driver","driver-first","x"},25}});
    near(evaluate(session.document()).at(tf("path","anchor_x")),25,"Anchor participates in ordinary dependency evaluation");
    atomic_reject(session,"DRIVEN_PROPERTY",{CenterAnchor{"path"}});
    const auto untouched=session.document();apply(session,{TransformAroundAnchor{"path",0,1,1}});
    check(session.document()==untouched,"Identity relative transform does not rewrite bindings or literals");

    auto coupled_document=plain();auto& coupled_path=coupled_document.objects.at("path");coupled_path.transform[4].literal=10;
    coupled_path.anchor[0].binding=Binding{tf("path","tx"),1,0,"copy_local_value"};Session coupled(coupled_document);
    atomic_reject(coupled,"TRANSFORM_PRESERVATION",{SetPosition{"path",100,0}});
    atomic_reject(coupled,"TRANSFORM_PRESERVATION",{TransformAroundAnchor{"path",180,1,1}});
    auto shear_document=plain();shear_document.objects.at("path").transform[2].binding=Binding{tf("path","a"),1,0,"copy_local_value"};Session shear(shear_document);
    atomic_reject(shear,"TRANSFORM_PRESERVATION",{TransformAroundAnchor{"path",0,2,1}});
}

void parent_replacement_and_keep_world() {
    Session session(hierarchy());const auto before=session.document();const auto old_world=transforms(before).at("child").world;
    matrix_near(old_world,{2,0,0,2,120,70},"Structural parent initially inherited once");
    apply(session,{SetTransformParent{"child",Id{"parent"},true}});
    const auto attached=transforms(session.document());
    matrix_near(attached.at("child").world,old_world,"Keep-world attach retains original matrix");
    matrix_near(attached.at("child").local,{0,-2,2,0,20,180},"Attach solves local matrix against the external parent");
    check(attached.at("child").effective_parent=="parent"&&session.document().objects.at("group").children==std::vector<Id>{"child"},"Following changes independently of Structure");
    check(session.document().objects.at("child").anchor==before.objects.at("child").anchor,"Attach preserves authored Anchor");
    apply(session,{Set{tf("group","tx"),999}});
    matrix_near(transforms(session.document()).at("child").world,old_world,"External Parent replaces structural transform, avoiding double application");
    apply(session,{Set{tf("parent","tx"),340}});auto moved=old_world;moved[4]+=40;
    matrix_near(transforms(session.document()).at("child").world,moved,"External Parent motion is followed once");
    const auto following=session.document();apply(session,{SetTransformParent{"child",{},true}});
    check(!session.document().objects.at("child").transform_parent&&transforms(session.document()).at("child").effective_parent=="group","Detach resumes the structural parent");
    matrix_near(transforms(session.document()).at("child").world,moved,"Keep-world detach retains current placement");
    session.undo(session.revision());check(session.document()==following,"Undo detach restores relationship and exact Scalars");
    session.redo(session.revision());

    Session keep_local(hierarchy());apply(keep_local,{SetTransformParent{"child",Id{"parent"},false}});
    matrix_near(transforms(keep_local.document()).at("child").world,{0,1,-1,0,280,60},"Keep-local attachment follows the new basis without rewriting affine fields");
    matrix_near(transforms(keep_local.document()).at("child").local,{1,0,0,1,10,20},"Keep-local preserves matrix authority");
    atomic_reject(keep_local,"MISSING_TRANSFORM_PARENT",{DeleteObjects{{"parent"}}});
    apply(keep_local,{SetTransformParent{"child",{},true},DeleteObjects{{"parent"}}});
    check(!keep_local.document().objects.contains("parent"),"Explicit keep-world detach allows parent deletion in one batch");
}

void parent_failures_and_effective_cycles() {
    auto document=hierarchy();document.compositions.push_back({"other-composition","Other",{"foreign"},{{"other-artboard","Other"}}});
    document.objects.emplace("foreign",path("foreign"));Session session(document);
    atomic_reject(session,"CROSS_COMPOSITION",{SetTransformParent{"child",Id{"foreign"},false}});
    atomic_reject(session,"MISSING_TRANSFORM_PARENT",{SetTransformParent{"child",Id{"missing"},false}});
    atomic_reject(session,"TRANSFORM_CYCLE",{SetTransformParent{"child",Id{"child"},false}});
    atomic_reject(session,"TRANSFORM_CYCLE",{SetTransformParent{"group",Id{"child"},false}});
    apply(session,{SetTransformParent{"child",Id{"parent"},false},SetTransformParent{"group",Id{"child"},false}});
    check(transforms(session.document()).at("group").effective_parent=="child","A structural descendant is legal when its actual effective chain does not cycle");

    auto linked=plain();linked.objects.at("path").transform[4].literal=10;
    linked.objects.at("driver").transform[4].binding=Binding{tf("path","tx"),1,0,"copy_local_value"};Session coupled(linked);
    atomic_reject(coupled,"TRANSFORM_PRESERVATION",{SetTransformParent{"path",Id{"driver"},true}});
    apply(coupled,{SetTransformParent{"path",Id{"driver"},false}});
    near(transforms(coupled.document()).at("path").world[4],20,"Keep-local remains valid for a parent whose local translation is linked to its child");

    auto driven=hierarchy();driven.objects.at("child").transform[4].binding=Binding{tf("parent","tx"),0,10,"copy_local_value"};Session fixed(driven);
    atomic_reject(fixed,"DRIVEN_PROPERTY",{SetTransformParent{"child",Id{"parent"},true}});
    apply(fixed,{SetTransformParent{"child",Id{"parent"},false}});
    check(property(fixed.document(),tf("child","tx")).binding.has_value(),"Keep-local attachment never unlinks a driven matrix");

    auto too_deep=empty_document("deep-document","deep-composition","deep-artboard");
    for(unsigned i=0;i<130;++i) {
        const auto id="node-"+std::to_string(i);auto object=path(id);
        if(i)object.transform_parent="node-"+std::to_string(i-1);
        too_deep.objects.emplace(id,std::move(object));too_deep.compositions.front().roots.push_back(id);
    }
    rejects("TRANSFORM_DEPTH",[&]{validate(too_deep);});
    auto too_large=hierarchy();matrix(too_large.objects.at("group"),{1e9,0,0,1e9,0,0});matrix(too_large.objects.at("child"),{1e9,0,0,1e9,0,0});
    validate(too_large);check(transforms(too_large).at("child").world[0]==1e18,"Finite legacy accumulated matrices retain their prior range contract");
    auto overflow=empty_document("overflow-document","overflow-composition","overflow-artboard");
    for(unsigned i=0;i<36;++i) {
        const auto id="overflow-"+std::to_string(i);auto object=path(id);matrix(object,{1e9,0,0,1e9,0,0});
        if(i)object.transform_parent="overflow-"+std::to_string(i-1);
        overflow.objects.emplace(id,std::move(object));overflow.compositions.front().roots.push_back(id);
    }
    rejects("OUTPUT_RANGE",[&]{validate(overflow);});
}

void singular_parent_contract() {
    auto document=hierarchy();matrix(document.objects.at("parent"),{0,0,0,1,300,50});Session session(document);
    atomic_reject(session,"SINGULAR_TRANSFORM",{SetTransformParent{"child",Id{"parent"},true}});
    apply(session,{SetTransformParent{"child",Id{"parent"},false}});const auto collapsed=transforms(session.document()).at("child").world;
    apply(session,{SetTransformParent{"child",{},true}});
    matrix_near(transforms(session.document()).at("child").world,collapsed,"A singular old world can detach through an invertible structural parent");
    rejects("SINGULAR_TRANSFORM",[&]{(void)inverse_affine(collapsed);});

    document=hierarchy();matrix(document.objects.at("group"),{0,0,0,2,100,30});document.objects.at("child").transform_parent="parent";
    Session structural_singular(document);atomic_reject(structural_singular,"SINGULAR_TRANSFORM",{SetTransformParent{"child",{},true}});
    apply(structural_singular,{SetTransformParent{"child",{},false}});
    check(!structural_singular.document().objects.at("child").transform_parent,"Explicit keep-local detach may enter a singular structural basis");
    const Affine matrix_value{2,3,-4,5,10,-20};matrix_near(compose(inverse_affine(matrix_value),matrix_value),identity_matrix,"Inverse composes in core column-vector order");
}

void cubic_stack_and_text_bounds() {
    auto document=plain();auto& object=document.objects.at("path");auto& points=object.contours.front().points;
    points.front().x.literal=0;points.front().y.literal=0;points.back().x.literal=100;points.back().y.literal=0;
    points.front().out_angle.literal=90;points.front().out_length.literal=100;
    points.back().in_angle.literal=90;points.back().in_length.literal=100;
    Session session(document);box(bounds(session.document(),"path"),{0,0,100,75},"Cubic extremum uses t=.5 rather than control bounds");
    const auto authored=session.document().objects.at("path").transform;apply(session,{CenterAnchor{"path"}});
    near(evaluate(session.document()).at(tf("path","anchor_x")),50,"Curve Anchor center X");near(evaluate(session.document()).at(tf("path","anchor_y")),37.5,"Curve Anchor center Y");
    check(session.document().objects.at("path").transform==authored,"Center Anchor preserves exact affine Scalars");
    auto repeat=default_operation("repeat","nect.shape.repeater");auto paint=default_operation("paint","nect.paint.stroke");paint.parameters.at("width").literal=400;
    apply(session,{AddOperation{"path",paint,0},AddOperation{"path",repeat,1}});
    box(bounds(session.document(),"path"),{0,0,300,75},"Bounds include Repeaters and paint instances, excluding stroke thickness");
    apply(session,{Set{operation_ref("path","repeat","copies"),0}});
    check(!bounds(session.document(),"path"),"Zero repeated geometry has no invented bounds");
    atomic_reject(session,"EMPTY_BOUNDS",{CenterAnchor{"path"}});

    document=plain();auto& loop=document.objects.at("path").contours.front().points;
    loop.front().x.literal=loop.front().y.literal=0;loop.back().x.literal=0;loop.back().y.literal=100;
    loop.front().out_angle.literal=0;loop.front().out_length.literal=100;
    loop.back().in_angle.literal=180;loop.back().in_length.literal=100;
    const auto extremum=100/(2*std::sqrt(3.0));box(bounds(document,"path"),{-extremum,0,extremum,100},"Both interior cubic extrema are considered");
#ifdef _WIN32
    auto text_document=empty_document("text-document","text-composition","text-artboard");
    Object text;text.id="text";text.name="Frame";text.kind=Kind::text;text.text=default_text("text-source","");text.text->layout="frame";
    text.text->parameters.at("origin_x").literal=10;text.text->parameters.at("origin_y").literal=20;
    text.text->parameters.at("frame_width").literal=200;text.text->parameters.at("frame_height").literal=100;
    text_document.objects.emplace(text.id,text);text_document.compositions.front().roots={text.id};
    box(bounds(text_document,"text"),{10,20,210,120},"Empty frame Text still contributes its authored layout rectangle");
#endif
}

void group_initial_center_and_external_bounds() {
    auto document=empty_document("document","composition","artboard");auto a=path("a"),b=path("b");
    matrix(a,{1,0,0,1,10,20});matrix(b,{1,0,0,1,50,40});document.objects={{"a",a},{"b",b}};document.compositions.front().roots={"a","b"};
    Session session(document);const auto before=transforms(session.document());
    apply(session,{GroupContiguous{"composition","",{"a","b"},"group","Group"}});
    auto values=evaluate(session.document());near(values.at(tf("group","anchor_x")),40,"New Group initializes geometric center X once");near(values.at(tf("group","anchor_y")),35,"New Group initializes geometric center Y once");
    matrix_near(transforms(session.document()).at("a").world,before.at("a").world,"Grouping preserves first member placement");
    matrix_near(transforms(session.document()).at("b").world,before.at("b").world,"Grouping preserves second member placement");
    const auto grouped=session.document();apply(session,{Set{{"a","a-last","x"},100}});
    near(evaluate(session.document()).at(tf("group","anchor_x")),40,"Later child edits do not recenter authored Group Anchor");
    apply(session,{CenterAnchor{"group"}});near(evaluate(session.document()).at(tf("group","anchor_x")),60,"Explicit Center Anchor refreshes Group geometry center");
    session.undo(session.revision());session.undo(session.revision());check(session.document()==grouped,"Undo restores child and explicit center independently");
    session.undo(session.revision());check(session.document()==document,"Grouping Undo restores exact Structure and authored member matrices");

    auto external=hierarchy();external.objects.at("child").transform_parent="parent";
    box(bounds(external,"group"),{85,15,90,25},"Group bounds use externally followed child's actual world in Group local coordinates");
    matrix(external.objects.at("group"),{0,0,0,2,100,30});
    rejects("SINGULAR_TRANSFORM",[&]{(void)bounds(external,"group");});
    external.objects.at("child").transform_parent.reset();
    box(bounds(external,"group"),{10,20,30,30},"Singular Group still bounds descendants via its known effective local chain");
    Session singular(external);apply(singular,{CenterAnchor{"group"}});
    near(evaluate(singular.document()).at(tf("group","anchor_x")),20,"Centering needs no inverse when local descendant coordinates remain defined");
}
}

int main() {
    try {
        anchor_and_relative_edits();driven_axes_and_anchor_links();parent_replacement_and_keep_world();
        parent_failures_and_effective_cycles();singular_parent_contract();cubic_stack_and_text_bounds();group_initial_center_and_external_bounds();
        std::cout<<"PASS "<<checks<<" Anchor, effective Transform Parent, bounds and atomic command checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
