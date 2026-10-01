#include "nect/io.hpp"
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

void common_pivot_edits() {
    const Affine quarter{0,1,-1,0,0,0};
    auto document=hierarchy();Session session(document);const auto before=transforms(document);
    apply(session,{TransformObjects{{"child","parent","group"},90,1,1,std::array<double,2>{0,0}}});
    const auto after=transforms(session.document());
    for(const auto& [id,transform]:before)matrix_near(after.at(id).world,compose(quarter,transform.world),"Selected structural descendants transform once");
    check(session.document().objects.at("child").transform==document.objects.at("child").transform,"Inherited child local transform stays exact");
    const auto result=session.document();check(decode(encode(result))==result,"Common-pivot native roundtrip");
    session.undo(session.revision());check(session.document()==document,"Common-pivot one Undo restores exact state");
    session.redo(session.revision());check(session.document()==result,"Common-pivot Redo exact");

    document=hierarchy();document.objects.at("child").transform_parent="parent";Session followed(document);
    const auto original=transforms(document);
    apply(followed,{TransformObjects{{"child","parent"},90,2,.5,std::array<double,2>{10,20}}});
    const Affine edit{0,2,-.5,0,20,0};
    for(const auto id:{"child","parent"})matrix_near(transforms(followed.document()).at(id).world,compose(edit,original.at(id).world),"World-axis scale then rotation shares explicit pivot");
    check(followed.document().objects.at("child").transform==document.objects.at("child").transform,"Selected follower local transform unchanged");
    Session single_child(document);apply(single_child,{TransformObjects{{"child"},90,1,1,std::array<double,2>{0,0}}});
    matrix_near(transforms(single_child.document()).at("child").world,compose(quarter,original.at("child").world),"Unselected external parent is compensated");

    Session mirrored(plain());const auto paths=mirrored.document().objects;
    apply(mirrored,{TransformObjects{{"path","driver"},0,-1,1,{}}});
    const Affine reflection{-1,0,0,1,30,0};
    matrix_near(transforms(mirrored.document()).at("path").world,reflection,"Bounds-center reflection uses world envelope");
    for(const auto& [id,object]:paths)check(mirrored.document().objects.at(id).contours==object.contours&&mirrored.document().objects.at(id).anchor==object.anchor,"Geometry IDs and authored Anchor preserved");
    document=hierarchy();Session collapsed(document);
    apply(collapsed,{TransformObjects{{"group","child"},0,0,1,std::array<double,2>{0,0}}});
    matrix_near(transforms(collapsed.document()).at("child").world,compose(Affine{0,0,0,1,0,0},transforms(document).at("child").world),"Zero-scale ancestor does not require a descendant inverse");

    Session invalid(plain());atomic_reject(invalid,"DUPLICATE_TARGET",{TransformObjects{{"path","path"},90,1,1,{}}});
    atomic_reject(invalid,"INVALID_BATCH",{TransformObjects{{},90,1,1,{}}});
    atomic_reject(invalid,"MISSING_OBJECT",{TransformObjects{{"missing"},90,1,1,{}}});
    atomic_reject(invalid,"OUT_OF_RANGE",{TransformObjects{{"path"},1e10,1,1,{}}});
    document=hierarchy();document.objects.at("group").transform[0].literal=0;Session singular(document);
    atomic_reject(singular,"SINGULAR_TRANSFORM",{TransformObjects{{"child"},90,1,1,std::array<double,2>{0,0}}});
    document=plain();document.objects.at("path").transform[0].binding=Binding{tf("driver","a"),1,0,"copy_local_value"};Session driven(document);
    atomic_reject(driven,"DRIVEN_PROPERTY",{TransformObjects{{"path"},90,1,1,{}}});
    // An unselected effective parent may itself depend on a selected object's
    // local matrix. The final target check must reject that hidden displacement.
    document=hierarchy();document.objects.at("child").transform_parent="parent";
    document.objects.at("parent").transform[4].binding=Binding{tf("child","tx"),1,0,"copy_local_value"};Session dependent(document);
    atomic_reject(dependent,"TRANSFORM_PRESERVATION",{TransformObjects{{"child"},90,1,1,std::array<double,2>{0,0}}});
    document=plain();auto second=document.compositions.front();second.id="other-plane";second.artboards.clear();second.roots={"driver"};document.compositions.front().roots={"path"};document.compositions.push_back(second);Session cross(document);
    atomic_reject(cross,"CROSS_COMPOSITION",{TransformObjects{{"path","driver"},90,1,1,{}}});

    Session retained(empty_document("retained-document","retained-plane","retained-artboard"));
    apply(retained,{CreatePrimitive{"retained-plane","","circle","Circle",default_primitive("circle-source","nect.shape.circle")}});
    const auto primitive=retained.document().objects.at("circle");
    apply(retained,{TransformObjects{{"circle"},35,2,.5,{}}});
    check(retained.document().objects.at("circle").source==primitive.source&&retained.document().objects.at("circle").point_edit==primitive.point_edit,"Common transform retains parametric source and corrections");
    apply(retained,{Set{{"circle","","generator.radius"},80}});
    near(evaluate(retained.document()).at({"circle","","generator.radius"}),80,"Transformed retained source remains editable");
    auto empty=plain();empty.objects.at("path").contours.clear();Session geometry_free(empty);
    atomic_reject(geometry_free,"EMPTY_BOUNDS",{TransformObjects{{"path"},90,1,1,{}}});
    apply(geometry_free,{TransformObjects{{"path"},90,1,1,std::array<double,2>{0,0}}});
    matrix_near(transforms(geometry_free.document()).at("path").world,quarter,"Explicit pivot supports geometry-free object");

    Session api(plain());const auto response=request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"transform_objects","objects":["path","driver"],"rotation":90,"scale_x":1,"scale_y":1,"pivot":[0,0]}]})");
    check(response.find("\"ok\":true")!=std::string::npos,"Common-pivot semantic API command");
    matrix_near(transforms(api.document()).at("path").world,quarter,"API uses same shared world transform");
    const auto snapshot=api.document();const auto malformed=request(api,R"({"op":"apply","expected_revision":1,"commands":[{"type":"transform_objects","objects":["path"],"rotation":90,"scale_x":1,"scale_y":1,"pivot":[0]}]})");
    check(malformed.find("INVALID_COMMAND")!=std::string::npos&&api.document()==snapshot,"Malformed API pivot rejects atomically");
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

void fresh_creation_anchor_contract() {
    auto document=empty_document("fresh-document","fresh-composition","fresh-artboard");
    Session session(document);auto curve=path("fresh-curve").contours;
    auto& points=curve.front().points;
    points.front().x.literal=0;points.front().y.literal=0;points.back().x.literal=100;points.back().y.literal=0;
    points.front().out_angle.literal=90;points.front().out_length.literal=100;
    points.back().in_angle.literal=90;points.back().in_length.literal=100;
    apply(session,{CreatePath{"fresh-composition","","fresh-curve","Curve",curve}});
    auto values=evaluate(session.document());
    near(values.at(tf("fresh-curve","anchor_x")),50,"Fresh Path initializes exact cubic center X");
    near(values.at(tf("fresh-curve","anchor_y")),37.5,"Fresh Path uses cubic extrema, not handle bounds");
    const auto created=session.document();session.undo(session.revision());check(session.document()==document,"One creation Undo removes its entire initialized state");
    session.redo(session.revision());check(session.document()==created,"Creation Redo restores exact Anchor");
    apply(session,{Set{{"fresh-curve",points.back().id,"x"},200}});
    near(evaluate(session.document()).at(tf("fresh-curve","anchor_x")),50,"Later geometry edit leaves Anchor fixed");
    apply(session,{CenterAnchor{"fresh-curve"}});
    near(evaluate(session.document()).at(tf("fresh-curve","anchor_x")),100,"Explicit Center Anchor uses edited geometry");
    auto primitive=default_primitive("fresh-source","nect.shape.ellipse");
    primitive.parameters.at("center_x").literal=70;primitive.parameters.at("center_y").literal=90;
    apply(session,{CreatePrimitive{"fresh-composition","","fresh-primitive","Ellipse",primitive}});
    near(evaluate(session.document()).at(tf("fresh-primitive","anchor_x")),70,"Direct Primitive initial center X");
    near(evaluate(session.document()).at(tf("fresh-primitive","anchor_y")),90,"Direct Primitive initial center Y");
    apply(session,{Set{{"fresh-primitive","","generator.center_x"},100}});
    near(evaluate(session.document()).at(tf("fresh-primitive","anchor_x")),70,"Generator edits do not recenter");
    const DuplicateObjects duplicate{{"fresh-primitive"},"fresh-copy"};const auto copied=duplicated_roots(session.document(),duplicate).front();
    apply(session,{duplicate,ConvertToPath{copied}});
    near(evaluate(session.document()).at(tf(copied,"anchor_x")),70,"Duplicate and Convert preserve authored Anchor rather than new bounds center");
    apply(session,{CreateFolder{"fresh-composition","","fresh-empty","Empty"}});
    near(evaluate(session.document()).at(tf("fresh-empty","anchor_x")),0,"Empty Folder remains neutral");
    atomic_reject(session,"EMPTY_BOUNDS",{CenterAnchor{"fresh-empty"}});
    auto child=path("fresh-child").contours;
    apply(session,{CreatePath{"fresh-composition","fresh-empty","fresh-child","Child",child}});
    near(evaluate(session.document()).at(tf("fresh-empty","anchor_x")),0,"Adding content does not center old empty Folder");
#ifdef _WIN32
    auto text=default_text("fresh-text-source","");text.layout="frame";
    text.parameters.at("origin_x").literal=10;text.parameters.at("origin_y").literal=20;
    text.parameters.at("frame_width").literal=200;text.parameters.at("frame_height").literal=100;
    apply(session,{CreateText{"fresh-composition","","fresh-text","Text",text}});
    near(evaluate(session.document()).at(tf("fresh-text","anchor_x")),110,"Direct frame Text center X");
    near(evaluate(session.document()).at(tf("fresh-text","anchor_y")),70,"Direct frame Text center Y");
    text.parameters.at("frame_width").literal=400;apply(session,{UpdateText{"fresh-text",text}});
    near(evaluate(session.document()).at(tf("fresh-text","anchor_x")),110,"Text content/frame edits leave Anchor fixed");
#endif
    Session reopened(decode(encode(session.document())));check(reopened.document()==session.document(),"Fresh Anchors reopen in native without a version change");
    auto forward=path("fresh-forward").contours;forward.front().points.front().y.literal=20;
    forward.front().points.back().x.literal=50;forward.front().points.back().y.literal=60;
    forward.front().points.front().x.binding=Binding{{"fresh-driver","fresh-driver-first","x"},1,0,"copy_local_value"};
    apply(session,{CreatePath{"fresh-composition","","fresh-forward","Forward",forward},
        Set{tf("fresh-forward","anchor_x"),123},CreatePath{"fresh-composition","","fresh-driver","Driver",path("fresh-driver").contours}});
    near(evaluate(session.document()).at(tf("fresh-forward","anchor_x")),123,"Forward reference batch retains explicit Anchor X");
    near(evaluate(session.document()).at(tf("fresh-forward","anchor_y")),40,"Forward reference batch still initializes untouched Anchor axis");
    auto invalid=path("fresh-invalid").contours;invalid.front().points.front().x.binding=Binding{{"absent","p","x"},1,0,"copy_local_value"};
    atomic_reject(session,"MISSING_REFERENCE",{CreatePath{"fresh-composition","","fresh-invalid","Invalid",invalid}});
    atomic_reject(session,"MISSING_COMPOSITION",{CreatePath{"missing","","fresh-invalid","Invalid",curve}});
    const auto before=session.document();rejects("REVISION_CONFLICT",[&]{session.apply({CreatePath{"fresh-composition","","stale","Stale",curve}},session.revision()+1);});
    check(session.document()==before,"Stale creation cannot initialize partial Anchor state");
    Session pivot(document);apply(pivot,{CreatePath{"fresh-composition","","pivot","Pivot",curve},SetPosition{"pivot",300,200}});
    near(evaluate(pivot.document()).at(tf("pivot","tx")),250,"Create then SetPosition consumes initialized Anchor X");
    near(evaluate(pivot.document()).at(tf("pivot","ty")),162.5,"Create then SetPosition consumes initialized Anchor Y");
    Session copies(document);auto shifted=primitive;shifted.parameters.at("center_x").literal=70;
    apply(copies,{CreatePrimitive{"fresh-composition","","original","Original",shifted},DuplicateObjects{{"original"},"copy"},
        Set{{"original","","generator.center_x"},100},ConvertToPath{"copy-1"}});
    near(evaluate(copies.document()).at(tf("copy-1","anchor_x")),70,"Same-batch copy/Convert centers its retained creation geometry");
    near(evaluate(copies.document()).at(tf("original","anchor_x")),100,"Initial committed source geometry determines original Anchor");
    auto driver=primitive;driver.id="anchor-driver-source";driver.parameters.at("center_x").literal=100;
    auto dependent=primitive;dependent.id="anchor-dependent-source";
    dependent.parameters.at("center_x").binding=Binding{tf("z-driver","anchor_x"),1,0,"copy_local_value"};
    Session ordered(document);apply(ordered,{CreatePrimitive{"fresh-composition","","a-dependent","Dependent",dependent},
        CreatePrimitive{"fresh-composition","","z-driver","Driver",driver}});
    near(evaluate(ordered.document()).at(tf("a-dependent","anchor_x")),100,"New Anchor dependency initializes its source before its dependent, independent of ID order");
    driver.parameters.at("center_x").binding=Binding{tf("a-dependent","anchor_x"),1,0,"copy_local_value"};
    Session cycle(document);atomic_reject(cycle,"CREATION_ANCHOR_CYCLE",{
        CreatePrimitive{"fresh-composition","","a-dependent","Dependent",dependent},CreatePrimitive{"fresh-composition","","z-driver","Driver",driver}});
    Session no_op(document);apply(no_op,{CreatePrimitive{"fresh-composition","","no-op","No-op",primitive},
        TransformAroundAnchor{"no-op",0,1,1},Set{{"no-op","","generator.center_x"},100}});
    near(evaluate(no_op.document()).at(tf("no-op","anchor_x")),100,"Identity pivot does not consume pending initialization");
    Session reused(document);apply(reused,{CreatePrimitive{"fresh-composition","","reused","Reused",primitive},
        DeleteObjects{{"reused"}},CreateFolder{"fresh-composition","","reused","Folder"},
        CreatePath{"fresh-composition","reused","child","Child",path("child").contours}});
    near(evaluate(reused.document()).at(tf("reused","anchor_x")),0,"Deleted creation marker cannot center a replacement Folder after adding children");
#ifdef _WIN32
    auto attached=default_text("attached-source","Path");
    attached.path_attachment=TextPathAttachment{"attached-path","attached-path-contour","distance",0,0,false};
    const auto attachment_path=path("attached-path",{0,0},{320,0}).contours;
    auto anchor_driver=primitive;anchor_driver.id="attached-driver-source";anchor_driver.parameters.at("center_x").literal=100;
    Session projected(document);apply(projected,{CreatePath{"fresh-composition","","attached-path","Path",attachment_path},
        CreateText{"fresh-composition","","a-text","Text",attached},
        CreatePrimitive{"fresh-composition","","z-driver","Driver",anchor_driver},
        Link{tf("a-text","tx"),Binding{tf("z-driver","anchor_x"),1,0,"copy_local_value"}}});
    const auto projected_anchor=evaluate(projected.document()).at(tf("a-text","anchor_x"));
    apply(projected,{CenterAnchor{"a-text"}});
    near(evaluate(projected.document()).at(tf("a-text","anchor_x")),projected_anchor,"Attached Text initializes after its effective transform's fresh Anchor dependency");
    for(const bool source_parent:{false,true}) {
        Session inherited(document);apply(inherited,{CreateFolder{"fresh-composition","","attachment-parent","Parent"},
            CreatePath{"fresh-composition",source_parent?"attachment-parent":"","attached-path","Path",attachment_path},
            CreateText{"fresh-composition",source_parent?"":"attachment-parent","a-text","Text",attached},
            CreatePrimitive{"fresh-composition","","z-driver","Driver",anchor_driver},
            Link{tf("attachment-parent","tx"),Binding{tf("z-driver","anchor_x"),1,0,"copy_local_value"}}});
        const auto anchor=evaluate(inherited.document()).at(tf("a-text","anchor_x"));apply(inherited,{CenterAnchor{"a-text"}});
        near(evaluate(inherited.document()).at(tf("a-text","anchor_x")),anchor,"Text projection follows fresh Anchor dependencies inherited by either world transform");
    }
    Session unused_path_anchor(document);apply(unused_path_anchor,{CreatePath{"fresh-composition","","attached-path","Path",attachment_path},
        CreateText{"fresh-composition","","a-text","Text",attached},
        Link{tf("attached-path","anchor_x"),Binding{tf("a-text","anchor_x"),1,0,"copy_local_value"}}});
    near(evaluate(unused_path_anchor.document()).at(tf("attached-path","anchor_x")),
        evaluate(unused_path_anchor.document()).at(tf("a-text","anchor_x")),"Unused source Path Anchor link is valid and does not create a projection cycle");
    auto dependent_path=attachment_path;
    dependent_path.front().points.back().x.binding=Binding{tf("z-driver","anchor_x"),1,0,"copy_local_value"};
    auto empty_attached=attached;empty_attached.content="";
    Session pending_length(document);apply(pending_length,{CreatePath{"fresh-composition","","attached-path","Path",dependent_path},
        CreateText{"fresh-composition","","a-text","Text",empty_attached},
        CreatePrimitive{"fresh-composition","","z-driver","Driver",anchor_driver}});
    near(evaluate(pending_length.document()).at({"attached-path","attached-path-last","x"}),100,
        "Final geometry validation waits for fresh Anchor dependencies instead of rejecting the provisional zero-length Path");
    auto unrelated=document;auto follow_path=path("unrelated-path",{0,0},{320,0});auto follower=path("unrelated-child");
    Object follow_group;follow_group.id="unrelated-group";follow_group.name="Follow";follow_group.kind=Kind::group;follow_group.children={follower.id};
    GroupPathFollow follow;follow.id="unrelated-relation";follow.path=follow_path.id;follow.contour=follow_path.contours.front().id;follow.items={{follower.id,{}}};
    follow_group.path_follow=follow;
    unrelated.objects={{follow_path.id,follow_path},{follow_group.id,follow_group},{follower.id,follower}};
    unrelated.compositions.front().roots={follow_path.id,follow_group.id};Session independent_projection(unrelated);
    apply(independent_projection,{CreatePath{"fresh-composition","","attached-path","Path",attachment_path},
        CreateText{"fresh-composition","","a-text","Text",empty_attached},
        CreatePrimitive{"fresh-composition","","z-driver","Driver",anchor_driver},
        Link{{"unrelated-path","unrelated-path-last","x"},Binding{tf("z-driver","anchor_x"),1,0,"copy_local_value"}}});
    near(evaluate(independent_projection.document()).at({"unrelated-path","unrelated-path-last","x"}),100,
        "Unrelated provisional Path Follow geometry does not block fresh Text initialization");
    check(independent_projection.document().objects.at("unrelated-group").path_follow==follow,
        "Projection evaluation never changes authored Path Follow state");
    Session followed_projection(unrelated);apply(followed_projection,{
        CreatePath{"fresh-composition","","attached-path","Path",attachment_path},
        CreateText{"fresh-composition","unrelated-group","a-text","Text",attached},
        Command{GroupPathFollowCommand{SetGroupPathFollowItem{"unrelated-group","a-text",{40,0,true}}}},
        CreatePrimitive{"fresh-composition","","z-driver","Driver",anchor_driver},
        Link{{"unrelated-path","unrelated-path-last","x"},Binding{tf("z-driver","anchor_x"),1,0,"copy_local_value"}}});
    const auto followed_anchor=evaluate(followed_projection.document()).at(tf("a-text","anchor_x"));
    apply(followed_projection,{CenterAnchor{"a-text"}});
    near(evaluate(followed_projection.document()).at(tf("a-text","anchor_x")),followed_anchor,
        "Fresh Text projection retains its own required Path Follow frame and initializes that frame's source dependencies");
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
Document rigid_follow_fixture() {
    auto document=empty_document("follow-document","follow-composition","follow-artboard");
    Object group;group.id="follow-group";group.name="Follow Group";group.kind=Kind::group;group.children={"follow-rect","follow-text"};
    matrix(group,{.8,.6,-.4,1.2,130,-40});
    Object rectangle;rectangle.id="follow-rect";rectangle.name="Rectangle A";
    rectangle.source=default_primitive("follow-rect-source","nect.shape.rectangle");
    matrix(rectangle,{1.5,.2,-.3,.8,12,8});
    Object text; text.id="follow-text";text.name="Editable Text B";text.kind=Kind::text;text.text=default_text("follow-text-source","Editable text");
    matrix(text,{.5,-.2,.7,1.1,-6,9});
    auto guide=path("follow-path",{0,0},{240,20});guide.visible=false;
    guide.contours.front().points.front().out_angle.literal=27;
    guide.contours.front().points.front().out_length.literal=34;
    guide.contours.front().points.back().in_angle.literal=196;
    guide.contours.front().points.back().in_length.literal=28;
    matrix(guide,{1.2,.2,-.1,.9,30,80});
    document.objects={{group.id,group},{rectangle.id,rectangle},{text.id,text},{guide.id,guide}};
    document.compositions.front().roots={group.id,guide.id};
    return document;
}
GroupPathFollow rigid_follow_relation() {
    GroupPathFollow relation;relation.id="follow-relation";relation.path="follow-path";relation.contour="follow-path-contour";
    relation.start_mode="distance";relation.start=40;relation.normal_offset=3;
    relation.items={{"follow-rect",{0,2,true}},{"follow-text",{120,-1,false}}};
    return relation;
}
void apply_follow(Session& session,GroupPathFollowCommand command) {
    session.apply({Command{std::move(command)}},session.revision());
}
Affine expected_follow_world(const Document& document,const Id& child) {
    const auto values=evaluate(document);const auto transforms=evaluate_transforms(document,values);
    const auto& group=document.objects.at("follow-group");const auto& relation=*group.path_follow;
    const auto& item=relation.items.at(child);
    const auto sampler=build_path_sampler(document,relation.path,relation.contour,values,transforms.at(relation.path).world);
    const auto start=relation.start_mode=="normalized"?relation.start*sampler.length:relation.start;
    const auto sample=sample_path(sampler,start+item.distance,relation.reversed);
    const auto offset=relation.normal_offset+item.normal_offset;
    const Vec2 position{sample.position.x+sample.normal.x*offset,sample.position.y+sample.normal.y*offset};
    Affine expected;
    if(item.follow_tangent) {
        const Affine frame{sample.tangent.x,sample.tangent.y,-sample.tangent.y,sample.tangent.x,position.x,position.y};
        expected=compose(frame,transforms.at(child).authored_local);
    } else {
        const auto local_position=map_point(inverse_affine(transforms.at("follow-group").world),position);
        expected=compose(transforms.at("follow-group").world,
            compose(Affine{1,0,0,1,local_position.x,local_position.y},transforms.at(child).authored_local));
    }
    return expected;
}
void rigid_group_path_follow() {
    auto document=rigid_follow_fixture();const auto authored_rect=document.objects.at("follow-rect").transform;
    const auto authored_text=document.objects.at("follow-text").transform;
    const auto authored_values=evaluate(document);const auto authored_transforms=evaluate_transforms(document,authored_values);
    const auto authored_sampler=build_path_sampler(document,"follow-path","follow-path-contour",authored_values,
        authored_transforms.at("follow-path").world);
    check(authored_sampler.length>160,"Cubic authored source Path covers both frozen sample distances");
    const auto relation=rigid_follow_relation();
    near(relation.start+relation.items.at("follow-rect").distance,40,"Rectangle A uses frozen source distance 40");
    near(relation.start+relation.items.at("follow-text").distance,160,"Text B uses frozen source distance 160");
    Session session(document);apply_follow(session,AttachGroupPathFollow{"follow-group",relation});
    auto result=evaluate_transforms(session.document(),evaluate(session.document()));
    matrix_near(result.at("follow-rect").world,expected_follow_world(session.document(),"follow-rect"),
        "Tangent-followed Rectangle uses sampled world frame and authored local exactly once");
    matrix_near(result.at("follow-text").world,expected_follow_world(session.document(),"follow-text"),
        "Tangent-off Text uses translation-only Group-local derived frame before its authored local");
    check(!session.document().objects.at("follow-path").visible,"Invisible authored Path remains a geometric source");
    check(session.document().objects.at("follow-rect").transform==authored_rect&&
        session.document().objects.at("follow-text").transform==authored_text,"Follow evaluation retains each child's authored transform");
    const auto initial_values=evaluate(session.document());
    const std::array<std::string,6> fields{"a","b","c","d","tx","ty"};
    for(std::size_t i=0;i<fields.size();++i)
        near(result.at("follow-rect").authored_local[i],initial_values.at(tf("follow-rect",fields[i])),
            "Evaluated authored matrix retains Scalar value");
    check(result.at("follow-rect").derived_local!=identity_matrix,"Evaluation exposes an ephemeral derived local frame separately");
    apply(session,{Rename{"follow-path","Renamed source"},Rename{"follow-rect","Renamed Rectangle"},
        ReorderObjects{"follow-composition","",{"follow-path","follow-group"}}});
    check(session.document().objects.at("follow-group").path_follow->path=="follow-path"&&
        session.document().objects.at("follow-group").path_follow->items.contains("follow-rect"),
        "Rename and sibling reorder do not retarget stable source or child IDs");
    matrix_near(evaluate_transforms(session.document(),evaluate(session.document())).at("follow-rect").world,
        expected_follow_world(session.document(),"follow-rect"),"Stable Path and child IDs still resolve after rename/reorder");

    auto edited_path=session.document().objects.at("follow-path").contours.front().points.back();
    apply_follow(session,UpdateGroupPathFollow{"follow-group",[&]{auto value=relation;value.start_mode="normalized";value.start=.23;return value;}()});
    const auto normalized=session.document();const auto before_path_change=evaluate_transforms(normalized,evaluate(normalized));
    apply(session,{Set{{"follow-path",edited_path.id,"y"},edited_path.y.literal+50}});
    const auto after_path_change=evaluate_transforms(session.document(),evaluate(session.document()));
    check(before_path_change.at("follow-rect").world!=after_path_change.at("follow-rect").world,
        "Editing the authored cubic Path reevaluates rigid follower position");
    const auto rectangle_bounds_before=bounds(session.document(),"follow-rect");
    apply(session,{Set{{"follow-rect","","generator.width"},320}});
    const auto rectangle_bounds_after=bounds(session.document(),"follow-rect");
    check(session.document().objects.at("follow-group").path_follow->items.contains("follow-rect")&&
        rectangle_bounds_before&&rectangle_bounds_after&&
        rectangle_bounds_after->right-rectangle_bounds_after->left>
            rectangle_bounds_before->right-rectangle_bounds_before->left,
        "Editing child A's Rectangle generator changes evaluated geometry while retaining its Path Follow relation");
    apply(session,{Set{tf("follow-text","tx"),44}});
    check(session.document().objects.at("follow-text").path_follow==std::nullopt&&
        session.document().objects.at("follow-group").path_follow->items.contains("follow-text"),
        "Editing a child's authored transform does not rewrite relation membership");
    matrix_near(evaluate_transforms(session.document(),evaluate(session.document())).at("follow-text").world,
        expected_follow_world(session.document(),"follow-text"),"Child authored transform edit composes after derived frame");
    const auto text_bounds_before=bounds(session.document(),"follow-text");
    auto edited_text=*session.document().objects.at("follow-text").text;
    edited_text.content="WWW WWW WWW Follow Content";
    apply(session,{UpdateText{"follow-text",edited_text}});
    const auto text_bounds_after=bounds(session.document(),"follow-text");
    check(session.document().objects.at("follow-group").path_follow->items.contains("follow-text")&&
        evaluated_text_source(session.document(),"follow-text").content==edited_text.content,
        "Editing child B's Text content preserves stable Group relation membership and source readback");
#ifdef _WIN32
    check(text_bounds_before&&text_bounds_after&&text_bounds_after->right-text_bounds_after->left>
        text_bounds_before->right-text_bounds_before->left,
        "Text content edit changes evaluated child B output while its rigid relation remains attached");
#endif

    auto reversed=*session.document().objects.at("follow-group").path_follow;reversed.reversed=true;reversed.normal_offset=9;
    apply_follow(session,UpdateGroupPathFollow{"follow-group",reversed});
    const auto reversed_result=evaluate_transforms(session.document(),evaluate(session.document()));
    matrix_near(reversed_result.at("follow-rect").world,expected_follow_world(session.document(),"follow-rect"),
        "Reversed traversal and relation normal offset are derived without baking");
    const auto item_before=*session.document().objects.at("follow-group").path_follow;
    auto changed_item=item_before.items.at("follow-rect");changed_item.distance=15;changed_item.normal_offset=4;changed_item.follow_tangent=false;
    apply_follow(session,SetGroupPathFollowItem{"follow-group","follow-rect",changed_item});
    matrix_near(evaluate_transforms(session.document(),evaluate(session.document())).at("follow-rect").world,
        expected_follow_world(session.document(),"follow-rect"),"Per-child distance, normal offset and tangent flag use the same evaluator");
    const auto with_item=session.document();apply_follow(session,RemoveGroupPathFollowItem{"follow-group","follow-rect"});
    check(!session.document().objects.at("follow-group").path_follow->items.contains("follow-rect"),"Removing one item retains Group relation");
    session.undo(session.revision());check(session.document()==with_item,"Item update Undo restores exact Path Follow relation");
    session.redo(session.revision());check(!session.document().objects.at("follow-group").path_follow->items.contains("follow-rect"),"Item update Redo restores removal");
    const auto after_remove=session.document();
    const auto retained_authored=session.document().objects.at("follow-text").transform;
    apply_follow(session,ClearGroupPathFollow{"follow-group"});
    const auto cleared_transforms=evaluate_transforms(session.document(),evaluate(session.document()));
    matrix_near(cleared_transforms.at("follow-text").world,
        compose(cleared_transforms.at("follow-group").world,cleared_transforms.at("follow-text").authored_local),
        "Clearing relation resumes authored structural placement without baking");
    check(session.document().objects.at("follow-text").transform==retained_authored,"Clear leaves child Scalar authorship byte-for-byte intact");
    session.undo(session.revision());check(session.document()==after_remove,"Undo Clear restores relation and remaining child items");
    session.redo(session.revision());

    auto cold=decode(encode(with_item));
    check(cold==with_item&&encode(cold)==encode(with_item),"Native 0.75 cold roundtrip retains stable relation and item IDs");
    matrix_near(evaluate_transforms(cold,evaluate(cold)).at("follow-text").world,
        evaluate_transforms(with_item,evaluate(with_item)).at("follow-text").world,"Cold reopen derives the same follower transform");
    auto legacy_fixture=rigid_follow_fixture();auto legacy=encode(legacy_fixture);
    const auto current_version=legacy.find("\"version\":\"0.75\"");check(current_version!=std::string::npos,"Native writer emits 0.75");
    legacy.replace(current_version,std::string("\"version\":\"0.75\"").size(),"\"version\":\"0.73\"");
    check(decode(legacy)==legacy_fixture,"Native 0.73 remains readable after writer upgrade");
    auto lied=encode(with_item);const auto lied_version=lied.find("\"version\":\"0.75\"");
    lied.replace(lied_version,std::string("\"version\":\"0.75\"").size(),"\"version\":\"0.73\"");
    rejects("UNKNOWN_FIELD",[&]{(void)decode(lied);});
    auto duplicate_fixture=rigid_follow_fixture();auto one_item_relation=rigid_follow_relation();
    one_item_relation.items.erase("follow-text");Session duplicate_session(duplicate_fixture);
    apply_follow(duplicate_session,AttachGroupPathFollow{"follow-group",one_item_relation});
    auto duplicate_item_native=encode(duplicate_session.document());const auto item_map=duplicate_item_native.find("\"items\":{");
    const auto item_map_end=duplicate_item_native.find("}}",item_map);
    check(item_map!=std::string::npos&&item_map_end!=std::string::npos,"Serialized Path Follow has an item map to validate");
    duplicate_item_native.insert(item_map_end+1,",\"follow-rect\":{\"distance\":1,\"normal_offset\":0,\"follow_tangent\":true}");
    rejects("DUPLICATE_KEY",[&]{(void)decode(duplicate_item_native);});

    auto command_fixture=rigid_follow_fixture();command_fixture.objects.at("follow-group").path_follow.reset();Session api(command_fixture);
    const auto response=request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"attach_group_path_follow","group":"follow-group","relation":{"id":"follow-relation","path":"follow-path","contour":"follow-path-contour","start_mode":"distance","start":20,"normal_offset":0,"reversed":false,"items":{"follow-rect":{"distance":0,"normal_offset":0,"follow_tangent":true}}}}]})");
    check(response.find("\"ok\":true")!=std::string::npos&&api.document().objects.at("follow-group").path_follow.has_value(),
        "JSON-lines API Path Follow attach routes to the same Session relation");
    const auto readback=request(api,R"({"op":"inspect"})");
    check(readback.find("\"path_follow\"")!=std::string::npos&&readback.find("follow-relation")!=std::string::npos,
        "Semantic API inspect reads back retained Group Path Follow data");
    const auto api_before=api.document();
    const auto stale=request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"clear_group_path_follow","group":"follow-group"}]})");
    check(stale.find("REVISION_CONFLICT")!=std::string::npos&&api.document()==api_before,"Stale relation command rejects atomically");

    Session invalid(rigid_follow_fixture());auto invalid_relation=rigid_follow_relation();
    auto try_attach=[&](const char* code,const GroupPathFollow& follow,const Document* replacement=nullptr) {
        Session candidate(replacement?*replacement:rigid_follow_fixture());
        atomic_reject(candidate,code,{Command{GroupPathFollowCommand{AttachGroupPathFollow{"follow-group",follow}}}});
    };
    invalid_relation.path="missing-path";try_attach("MISSING_PATH_ATTACHMENT",invalid_relation);
    invalid_relation=rigid_follow_relation();invalid_relation.contour="missing-contour";try_attach("MISSING_PATH_CONTOUR",invalid_relation);
    invalid_relation=rigid_follow_relation();invalid_relation.path="follow-text";try_attach("INVALID_PATH_ATTACHMENT",invalid_relation);
    invalid_relation=rigid_follow_relation();invalid_relation.items={{"missing-child",{0,0,true}}};try_attach("GROUP_PATH_FOLLOW_NOT_CHILD",invalid_relation);
    invalid_relation=rigid_follow_relation();invalid_relation.start=std::numeric_limits<double>::infinity();try_attach("NON_FINITE",invalid_relation);
    auto cross=rigid_follow_fixture();auto foreign=path("foreign-path");cross.objects.emplace("foreign-path",foreign);
    cross.compositions.push_back({"foreign-composition","Foreign",{"foreign-path"},{}});
    invalid_relation=rigid_follow_relation();invalid_relation.path="foreign-path";invalid_relation.contour="foreign-path-contour";
    try_attach("CROSS_COMPOSITION",invalid_relation,&cross);
    auto singular=rigid_follow_fixture();matrix(singular.objects.at("follow-group"),{0,0,0,1,0,0});
    try_attach("SINGULAR_TRANSFORM",rigid_follow_relation(),&singular);
    auto cyclic=rigid_follow_fixture();cyclic.objects.at("follow-path").transform_parent="follow-rect";
    try_attach("TRANSFORM_CYCLE",rigid_follow_relation(),&cyclic);
    auto descendant=rigid_follow_fixture();descendant.objects.at("follow-group").children.push_back("follow-path");
    descendant.compositions.front().roots.pop_back();
    try_attach("GROUP_PATH_FOLLOW_DESCENDANT_SOURCE",rigid_follow_relation(),&descendant);
    auto zero=rigid_follow_fixture();auto& points=zero.objects.at("follow-path").contours.front().points;
    points.back().x.literal=points.front().x.literal;points.back().y.literal=points.front().y.literal;
    points.front().out_length.literal=0;points.back().in_length.literal=0;
    try_attach("PATH_SAMPLE_ZERO_LENGTH",rigid_follow_relation(),&zero);
    auto open_overflow=rigid_follow_fixture();const auto overflow_values=evaluate(open_overflow);
    const auto overflow_transforms=evaluate_transforms(open_overflow,overflow_values);
    const auto overflow_sampler=build_path_sampler(open_overflow,"follow-path","follow-path-contour",overflow_values,
        overflow_transforms.at("follow-path").world);
    invalid_relation=rigid_follow_relation();invalid_relation.items.at("follow-text").distance=overflow_sampler.length-39;
    try_attach("GROUP_PATH_FOLLOW_RANGE",invalid_relation,&open_overflow);
    auto closed_wrap=rigid_follow_fixture();closed_wrap.objects.at("follow-path").contours.front().closed=true;
    const auto closed_values=evaluate(closed_wrap);const auto closed_transforms=evaluate_transforms(closed_wrap,closed_values);
    const auto closed_sampler=build_path_sampler(closed_wrap,"follow-path","follow-path-contour",closed_values,
        closed_transforms.at("follow-path").world);
    auto wrapped_relation=rigid_follow_relation();wrapped_relation.items["follow-rect"].distance=closed_sampler.length;
    Session wrapped(closed_wrap);apply_follow(wrapped,AttachGroupPathFollow{"follow-group",wrapped_relation});
    auto base_wrap_relation=wrapped_relation;base_wrap_relation.items["follow-rect"].distance=0;
    apply_follow(wrapped,UpdateGroupPathFollow{"follow-group",base_wrap_relation});
    const auto base_wrap_world=evaluate_transforms(wrapped.document(),evaluate(wrapped.document())).at("follow-rect").world;
    base_wrap_relation.items["follow-rect"].distance=closed_sampler.length;
    apply_follow(wrapped,UpdateGroupPathFollow{"follow-group",base_wrap_relation});
    matrix_near(evaluate_transforms(wrapped.document(),evaluate(wrapped.document())).at("follow-rect").world,base_wrap_world,
        "Closed source contour wraps a sample one complete contour length after distance 40");
    auto explicit_group_parent=rigid_follow_fixture();explicit_group_parent.objects.at("follow-group").transform_parent="follow-path";
    try_attach("GROUP_PATH_FOLLOW_TRANSFORM_PARENT",rigid_follow_relation(),&explicit_group_parent);
    auto explicit_child_parent=rigid_follow_fixture();explicit_child_parent.objects.at("follow-rect").transform_parent="follow-path";
    try_attach("GROUP_PATH_FOLLOW_TRANSFORM_PARENT",rigid_follow_relation(),&explicit_child_parent);
    apply_follow(invalid,AttachGroupPathFollow{"follow-group",rigid_follow_relation()});
    atomic_reject(invalid,"MISSING_PATH_ATTACHMENT",{DeleteObjects{{"follow-path"}}});
    invalid_relation=rigid_follow_relation();invalid_relation.id="new-id";
    atomic_reject(invalid,"GROUP_PATH_FOLLOW_ID_MISMATCH",{Command{GroupPathFollowCommand{UpdateGroupPathFollow{"follow-group",invalid_relation}}}});
    const auto invalid_before=invalid.document();const auto invalid_revision=invalid.revision();
    rejects("REVISION_CONFLICT",[&]{invalid.apply({Command{GroupPathFollowCommand{ClearGroupPathFollow{"follow-group"}}}},invalid_revision-1);});
    check(invalid.document()==invalid_before,"Revision-conflict Path Follow command is atomic");
}
Document deform_fixture() {
    auto d=empty_document("deform-document","deform-composition","deform-artboard");
    Object group;group.id="deform-group";group.name="Deform Group";group.kind=Kind::group;
    group.children={"deform-a","deform-b"};
    auto a=path("deform-a",{10,4},{45,4});a.contours.front().closed=true;
    Point bottom=a.contours.front().points.back();bottom.id="deform-a-bottom";bottom.y.literal=14;
    a.contours.front().points.push_back(bottom);bottom.id="deform-a-left";bottom.x.literal=10;a.contours.front().points.push_back(bottom);
    a.stack.push_back(default_operation("deform-a-fill","nect.paint.fill"));
    a.stack.push_back(default_operation("deform-a-stroke","nect.paint.stroke"));
    auto b=path("deform-b",{70,-5},{105,8});
    b.contours.front().points.front().out_angle.literal=25;b.contours.front().points.front().out_length.literal=13;
    b.contours.front().points.back().in_angle.literal=195;b.contours.front().points.back().in_length.literal=15;
    b.stack.push_back(default_operation("deform-b-stroke","nect.paint.stroke"));
    auto guide=path("deform-guide",{0,0},{500,0});guide.visible=false;
    auto target=path("deform-mask-target",{0,0},{150,150});
    target.compositing.mask=GeometryMask{"deform-mask","deform-a"};
    d.objects={{group.id,group},{a.id,a},{b.id,b},{guide.id,guide},{target.id,target}};
    d.compositions.front().roots={group.id,guide.id,target.id};return d;
}
GroupPathFollow deform_relation() {
    GroupPathFollow r;r.id="deform-relation";r.path="deform-guide";r.contour="deform-guide-contour";
    r.mode="deform";r.items={{"deform-a",{0,0,true}},{"deform-b",{0,0,false}}};return r;
}
EvaluatedScene deform_scene(const Document& d) {
    const auto values=evaluate(d);return evaluate_scene(d,"deform-composition",values,evaluate_transforms(d,values));
}
void point_near(Vec2 actual,Vec2 expected,const std::string& message,double tolerance=1e-8) {
    check(std::hypot(actual.x-expected.x,actual.y-expected.y)<tolerance,message+": expected "+
        std::to_string(expected.x)+","+std::to_string(expected.y)+" got "+std::to_string(actual.x)+","+std::to_string(actual.y));
}
PathSample quarter_oracle(double distance) {
    // Independent 40000-step arc table and analytic cubic derivative.
    constexpr double k=.5522847498307936;constexpr unsigned n=40000;
    const auto position=[](double t) {const auto u=1-t;return Vec2{3*u*u*t*k*100+3*u*t*t*100+t*t*t*100,
        3*u*t*t*(1-k)*100+t*t*t*100};};
    double length=0;auto prior=position(0);double t=0;
    for(unsigned i=1;i<=n;++i) {
        const auto next=position(double(i)/n);const auto segment=std::hypot(next.x-prior.x,next.y-prior.y);
        if(length+segment>=distance){t=(double(i-1)+(distance-length)/segment)/n;break;}
        length+=segment;prior=next;
    }
    const auto u=1-t;
    Vec2 tangent{3*u*u*k*100+6*u*t*(1-k)*100,6*u*t*(1-k)*100+3*t*t*k*100};
    const auto magnitude=std::hypot(tangent.x,tangent.y);tangent={tangent.x/magnitude,tangent.y/magnitude};
    PathSample result;result.position=position(t);result.tangent=tangent;result.normal={-tangent.y,tangent.x};return result;
}
void group_path_deform() {
    const auto source=deform_fixture();Session s(source);auto relation=deform_relation();
    apply_follow(s,AttachGroupPathFollow{"deform-group",relation});const auto attached=s.document();
    auto scene=deform_scene(s.document());
    for(const auto* id:{"deform-a","deform-b"}) {
        const auto original=evaluate_shape(source,id,evaluate(source));
        const auto& projected=scene.shapes.at(id);
        check(projected.paths.front().transform==identity_matrix,"Deform consumes path instance transform once");
        matrix_near(scene.geometry_worlds.at(id),identity_matrix,"Straight identity geometry world");
        const auto& a=original.paths.front().contours->front().points;
        const auto& b=projected.paths.front().contours->front().points;
        check(a.size()==b.size(),"Deform retains cubic topology");
        for(std::size_t i=0;i<a.size();++i) {
            point_near(b[i].anchor,a[i].anchor,"Straight identity anchor");
            point_near(b[i].incoming,a[i].incoming,"Straight identity incoming control");
            point_near(b[i].outgoing,a[i].outgoing,"Straight identity outgoing control");
        }
        check(s.document().objects.at(id)==source.objects.at(id),"Projection retains exact authored leaf data");
    }
    check(scene.deformation_points.at("deform-a").front().id=="deform-a-first"&&
        scene.deformation_points.at("deform-a").front().contour=="deform-a-contour","Projected points retain stable source IDs");
    const auto& mask=*scene.roots.back().mask;
    check(mask.paths.front().contours==scene.shapes.at("deform-a").paths.front().contours,"Mask consumes canonical deformed contours");
    matrix_near(mask.paths.front().transform,scene.geometry_worlds.at("deform-a"),"Mask uses projected geometry world");
    const auto& paints=scene.shapes.at("deform-a").paints;
    check(paints.size()==2&&std::any_of(paints.begin(),paints.end(),[](const auto& paint){return paint.type=="nect.paint.stroke"&&paint.width==2;}),
        "Fill and Stroke preserve authored paint options");
    check(scene.shapes.at("deform-a").paints.front().paths.front().contours==scene.shapes.at("deform-a").paths.front().contours,
        "Ordered paint and final geometry reuse the same warped snapshot");
    apply(s,{Set{{"deform-guide","deform-guide-last","x"},100},Set{{"deform-guide","deform-guide-last","y"},100},
        Set{{"deform-guide","deform-guide-first","out.angle"},0},Set{{"deform-guide","deform-guide-first","out.length"},.5522847498307936*100},
        Set{{"deform-guide","deform-guide-last","in.angle"},270},Set{{"deform-guide","deform-guide-last","in.length"},.5522847498307936*100}});
    scene=deform_scene(s.document());
    for(const auto* id:{"deform-a","deform-b"}) {
        const auto original=evaluate_shape(source,id,evaluate(source));
        const auto& points=scene.shapes.at(id).paths.front().contours->front().points;
        for(std::size_t i=0;i<points.size();++i) {
            const auto& p=original.paths.front().contours->front().points[i];const auto oracle=quarter_oracle(p.anchor.x);
            const Vec2 anchor{oracle.position.x+oracle.normal.x*p.anchor.y,oracle.position.y+oracle.normal.y*p.anchor.y};
            const auto handle=[&](Vec2 control){return Vec2{anchor.x+oracle.tangent.x*(control.x-p.anchor.x)+oracle.normal.x*(control.y-p.anchor.y),
                anchor.y+oracle.tangent.y*(control.x-p.anchor.x)+oracle.normal.y*(control.y-p.anchor.y)};};
            point_near(points[i].anchor,anchor,"Quarter high-resolution anchor oracle",.01);
            point_near(points[i].incoming,handle(p.incoming),"Quarter high-resolution incoming vector oracle",.01);
            point_near(points[i].outgoing,handle(p.outgoing),"Quarter high-resolution outgoing vector oracle",.01);
        }
    }
    const auto before_edit=scene.deformation_points.at("deform-a").front().anchor;
    apply(s,{Set{{"deform-a","deform-a-first","x"},12},Set{{"deform-a","deform-a-first","out.length"},3}});
    scene=deform_scene(s.document());
    check(std::hypot(scene.deformation_points.at("deform-a").front().anchor.x-before_edit.x,
        scene.deformation_points.at("deform-a").front().anchor.y-before_edit.y)>1,"Source anchor edit recomputes deformation");
    check(s.document().objects.at("deform-a").contours.front().points.front().x.literal==12,
        "Source value stays authored under its stable point ID");
    const auto warped=s.document();const auto warped_scene=scene;
    relation.mode="rigid";apply_follow(s,UpdateGroupPathFollow{"deform-group",relation});
    check(deform_scene(s.document()).deformation_points.empty(),"Rigid mode stops bending geometry");
    s.undo(s.revision());check(s.document()==warped,"One Undo restores deform mode and exact source data");
    s.redo(s.revision());check(s.document().objects.at("deform-group").path_follow->mode=="rigid","Redo restores rigid mode");
    relation.mode="deform";apply_follow(s,UpdateGroupPathFollow{"deform-group",relation});
    point_near(deform_scene(s.document()).deformation_points.at("deform-b").front().anchor,
        warped_scene.deformation_points.at("deform-b").front().anchor,"Deform mode roundtrip is deterministic");
    const auto native=encode(s.document());check(native.find("\"version\":\"0.75\"")!=std::string::npos&&decode(native)==s.document(),
        "Native 0.75 cold read retains mode, axis and authored source");
    auto lied=native;const auto version=lied.find("0.75");lied.replace(version,4,"0.74");rejects("UNKNOWN_FIELD",[&]{(void)decode(lied);});
    auto legacy=encode(attached);
    // Construct a strict 0.74 rigid relation by omitting the two new fields.
    auto rigid_document=attached;rigid_document.objects.at("deform-group").path_follow->mode="rigid";
    legacy=encode(rigid_document);check(legacy.find("\"mode\":\"rigid\"")==std::string::npos&&
        legacy.find("\"mode\":\"deform\"")==std::string::npos&&legacy.find("\"deform_axis\"")==std::string::npos,
        "Default rigid/x relation omits new fields for strict 0.74 compatibility");
    legacy.replace(legacy.find("0.75"),4,"0.74");
    check(decode(legacy)==rigid_document,"Native 0.74 rigid relation reads unchanged with default mode/axis");
    const auto api=request(s,R"({"op":"compositing_plan","composition":"deform-composition"})");
    check(api.find("\"deformation\"")!=std::string::npos&&api.find("deform-relation")!=std::string::npos&&
        api.find("deform-a-first")!=std::string::npos&&api.find("authored_source_preserved")!=std::string::npos,"API reads stable deformation provenance");
    const auto paint_plan=request(s,R"({"op":"render_plan","object":"deform-a"})");
    check(paint_plan.find("\"geometry_space\":\"group_local\"")!=std::string::npos&&paint_plan.find("deformation_group")!=std::string::npos,
        "Render plan consumes canonical deformation and labels its projection plane");
    const auto svg=export_svg(s.document(),"deform-composition","deform-artboard");
    check(svg.find("Group Path Deform derivative")!=std::string::npos&&encode(s.document())==native,"SVG emits a declared derivative without mutating source");
    auto no_mask=s.document();no_mask.objects.at("deform-mask-target").compositing.mask.reset();
    no_mask.objects.at("deform-guide").visible=true; // Remove the legacy visibility trigger too.
    check(export_svg(no_mask,"deform-composition","deform-artboard").find("Group Path Deform derivative")!=std::string::npos,
        "Plain deform forces SVG scene projection even without compositing");
    const auto detach_source=s.document();apply_follow(s,ClearGroupPathFollow{"deform-group"});
    auto expected=detach_source;expected.objects.at("deform-group").path_follow.reset();check(s.document()==expected,"Detach returns exact unchanged source hierarchy and geometry");
    s.undo(s.revision());check(s.document()==detach_source,"Detach one Undo restores exact relation");
    // Nonidentity child/Group transforms are consumed once. A singular child
    // remains renderable because no inverse child matrix is needed.
    auto affine=source;matrix(affine.objects.at("deform-group"),{2,.1,.2,1.5,20,30});
    matrix(affine.objects.at("deform-a"),{1.5,.2,.3,.7,4,2});
    affine.objects.at("deform-group").path_follow=deform_relation();auto projected=deform_scene(affine);
    const auto values=evaluate(affine);const auto tfm=evaluate_transforms(affine,values);
    const auto group_point=map_point(compose(inverse_affine(tfm.at("deform-group").world),tfm.at("deform-a").world),{10,4});
    point_near(map_point(projected.geometry_worlds.at("deform-a"),projected.deformation_points.at("deform-a").front().anchor),group_point,
        "Group inverse and final Group world are each applied once");
    const auto world_bounds=object_bounds(affine,"deform-a",values,tfm,true);
    check(world_bounds&&world_bounds->left<world_bounds->right,"World bounds consume warped geometry");
    matrix(affine.objects.at("deform-a"),{0,0,0,1,30,0});projected=deform_scene(affine);
    check(projected.deformation_points.at("deform-a").size()==4,"Singular child transform remains supported");
    const auto singular_values=evaluate(affine);const auto singular_transforms=evaluate_transforms(affine,singular_values);
    check(object_bounds(affine,"deform-a",singular_values,singular_transforms).has_value(),
        "Singular deform leaf evaluated-local bounds use Group-local projection without inverse child");
    const auto source_bounds=object_bounds(affine,"deform-a",singular_values,singular_transforms,false,true);
    check(source_bounds&&source_bounds->left==10&&source_bounds->right==45,
        "Source-local bounds remain separate from derived Group-local projection");
    Session pivot(affine);apply(pivot,{CenterAnchor{"deform-a"}});
    near(evaluate(pivot.document()).at({"deform-a","","transform.anchor_x"}),27.5,"CenterAnchor remains source-local on singular deformed leaf");
    auto hierarchy=source;Object nested;nested.id="deform-nested";nested.name="Nested";nested.kind=Kind::group;nested.children={"deform-a"};
    matrix(nested,{1,0,0,1,3,2});hierarchy.objects.emplace(nested.id,nested);hierarchy.objects.at("deform-group").children.front()=nested.id;
    relation=deform_relation();relation.items.erase("deform-a");relation.items[nested.id]={0,0,true};hierarchy.objects.at("deform-group").path_follow=relation;
    point_near(deform_scene(hierarchy).deformation_points.at("deform-a").front().anchor,{13,6},"Nested eligible Group applies authored transform before deformation");
    auto axis=source;relation=deform_relation();relation.deform_axis="y";relation.start=20;axis.objects.at("deform-group").path_follow=relation;
    point_near(deform_scene(axis).deformation_points.at("deform-a").front().anchor,{24,10},"Y axis swaps longitudinal and perpendicular coordinates");
    auto paint_fixture=source;auto repeat=default_operation("deform-paint-repeat","nect.shape.repeater");
    repeat.parameters.at("copies").literal=2;repeat.parameters.at("position_x").literal=50;
    paint_fixture.objects.at("deform-a").stack.push_back(repeat);
    Gradient gradient;gradient.id="deform-gradient";gradient.type="linear";gradient.end_x.literal=20;
    GradientStop left;left.id="deform-gradient-left";GradientStop right;right.id="deform-gradient-right";right.offset.literal=1;
    gradient.stops={left,right};paint_fixture.objects.at("deform-a").stack.front().gradient=gradient;
    matrix(paint_fixture.objects.at("deform-a"),{1.5,.2,.3,.7,4,2});
    paint_fixture.objects.at("deform-group").path_follow=deform_relation();const auto paint_scene=deform_scene(paint_fixture);
    const auto& paint_shape=paint_scene.shapes.at("deform-a");
    point_near(paint_shape.paths.at(1).contours->front().points.front().anchor,{95.2,16.8},
        "Repeater path transform and child affine are each consumed once before warp");
    check(std::any_of(paint_shape.paints.begin(),paint_shape.paints.end(),[](const auto& paint) {
        return paint.gradient&&std::abs(paint.gradient->start.x-79)<1e-8&&std::abs(paint.gradient->start.y-12)<1e-8&&
            std::abs(paint.gradient->end.x-109)<1e-8&&std::abs(paint.gradient->end.y-16)<1e-8&&paint.transform==identity_matrix;
    }),"Gradient field consumes Repeater paint affine and child affine once in Group space");
    check(std::any_of(paint_shape.paints.begin(),paint_shape.paints.end(),[&](const auto& paint) {
        return paint.paths.front().contours==paint_shape.paths.at(1).contours;
    }),"Repeated paint and final path share the same projected contour authority");
    auto reverse_fixture=source;relation=deform_relation();relation.start=380;relation.reversed=true;relation.normal_offset=3;
    reverse_fixture.objects.at("deform-group").path_follow=relation;
    point_near(deform_scene(reverse_fixture).deformation_points.at("deform-a").front().anchor,{110,-7},
        "Reversed deform uses the sampled reversed tangent and normal frame");
    auto negative=[&](const char* code,Document document,GroupPathFollow invalid) {
        Session candidate(document);atomic_reject(candidate,code,{Command{GroupPathFollowCommand{AttachGroupPathFollow{"deform-group",invalid}}}});
    };
    relation=deform_relation();relation.mode="invalid";negative("GROUP_PATH_FOLLOW_MODE",source,relation);
    relation=deform_relation();relation.deform_axis="z";negative("GROUP_PATH_DEFORM_AXIS",source,relation);
    relation=deform_relation();relation.start=450;negative("GROUP_PATH_DEFORM_RANGE",source,relation);
    auto closed=source;closed.objects.at("deform-guide").contours.front().closed=true;
    relation=deform_relation();relation.items.at("deform-b").distance=1100;negative("GROUP_PATH_DEFORM_SPAN",closed,relation);
    auto unsupported=source;unsupported.objects.at("deform-b").kind=Kind::text;
    unsupported.objects.at("deform-b").contours.clear();unsupported.objects.at("deform-b").stack.clear();
    unsupported.objects.at("deform-b").text=default_text("deform-text","Text");
    negative("GROUP_PATH_DEFORM_UNSUPPORTED_DOMAIN",unsupported,deform_relation());
    auto singular=source;matrix(singular.objects.at("deform-group"),{0,0,0,1,0,0});negative("SINGULAR_TRANSFORM",singular,deform_relation());
    auto cyclic=source;cyclic.objects.at("deform-guide").transform_parent="deform-a";negative("GROUP_PATH_FOLLOW_CYCLE",cyclic,deform_relation());
    auto zero=source;zero.objects.at("deform-guide").contours.front().points.back().x.literal=0;
    negative("PATH_SAMPLE_ZERO_LENGTH",zero,deform_relation());
    auto cross=source;cross.objects.emplace("foreign-guide",path("foreign-guide",{0,0},{500,0}));
    cross.compositions.push_back({"foreign-composition","Foreign",{"foreign-guide"},{}});
    relation=deform_relation();relation.path="foreign-guide";relation.contour="foreign-guide-contour";
    negative("CROSS_COMPOSITION",cross,relation);
    auto explicit_parent=source;explicit_parent.objects.at("deform-group").transform_parent="deform-guide";
    negative("GROUP_PATH_FOLLOW_TRANSFORM_PARENT",explicit_parent,deform_relation());
    explicit_parent=source;explicit_parent.objects.at("deform-a").transform_parent="deform-guide";
    negative("GROUP_PATH_FOLLOW_TRANSFORM_PARENT",explicit_parent,deform_relation());
    auto inside=source;inside.objects.at("deform-group").children.push_back("deform-guide");
    inside.compositions.front().roots.erase(inside.compositions.front().roots.begin()+1);
    negative("GROUP_PATH_FOLLOW_DESCENDANT_SOURCE",inside,deform_relation());
    auto resource=source;auto repeater=default_operation("deform-repeat-limit","nect.shape.repeater");
    repeater.parameters.at("copies").literal=1000;repeater.parameters.at("position_x").literal=0;
    resource.objects.at("deform-a").stack.push_back(repeater);Session limited(resource);
    auto second_repeat=default_operation("deform-repeat-overflow","nect.shape.repeater");second_repeat.parameters.at("copies").literal=5;
    atomic_reject(limited,"OUTPUT_LIMIT",{Command{GroupPathFollowCommand{AttachGroupPathFollow{"deform-group",deform_relation()}}},
        AddOperation{"deform-a",second_repeat}});
    relation=deform_relation();relation.contour="missing";negative("MISSING_PATH_CONTOUR",source,relation);
    relation=deform_relation();relation.path="missing";negative("MISSING_PATH_ATTACHMENT",source,relation);
    Session intact(attached);atomic_reject(intact,"MISSING_PATH_ATTACHMENT",{DeleteObjects{{"deform-guide"}}});
    atomic_reject(intact,"GROUP_PATH_FOLLOW_NOT_CHILD",{DeleteObjects{{"deform-a"}}});
    const auto before=intact.document();rejects("REVISION_CONFLICT",[&]{intact.apply({Command{GroupPathFollowCommand{ClearGroupPathFollow{"deform-group"}}}},intact.revision()+1);});
    check(intact.document()==before,"Stale deformation request is atomic");
}
}

int main() {
    try {
        fresh_creation_anchor_contract();common_pivot_edits();anchor_and_relative_edits();driven_axes_and_anchor_links();parent_replacement_and_keep_world();
        parent_failures_and_effective_cycles();singular_parent_contract();cubic_stack_and_text_bounds();group_initial_center_and_external_bounds();
        rigid_group_path_follow();
        group_path_deform();
        std::cout<<"PASS "<<checks<<" Anchor, effective Transform Parent, bounds and atomic command checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
