#include "nect/core.hpp"
#include <cmath>
#include <iostream>
#include <limits>

using namespace nect;
namespace {
int checks=0;
void check(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);++checks;}
void near(double actual,double expected,const std::string& message){check(std::abs(actual-expected)<1e-8,message);}
template<class F> void rejects(const char* code,F action) {
    try {action();}catch(const Error& error){check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
void atomic_reject(Session& session,const char* code,std::vector<Command> commands) {
    const auto before=session.document();const auto revision=session.revision();const auto history=session.history();
    rejects(code,[&]{apply(session,std::move(commands));});
    check(session.document()==before&&session.revision()==revision&&session.history()==history,"Failed multi-target command preserves state, revision and History");
}
Ref x(const Id& id){return {id,id+"-point","x"};}
Ref tf(const Id& id,const std::string& field){return {id,"","transform."+field};}
Object path(const Id& id,double value=0) {
    Object object;object.id=id;object.name=id;Point point;point.id=id+"-point";point.x.literal=value;
    object.contours={{id+"-contour",false,{point}}};return object;
}
void matrix(Object& object,const Affine& values){for(std::size_t i=0;i<values.size();++i)object.transform[i].literal=values[i];}
Document properties_fixture() {
    auto document=empty_document("document","composition","artboard");
    document.objects={{"a",path("a",100)},{"b",path("b",200)},{"c",path("c",300)},{"source",path("source",50)}};
    document.compositions.front().roots={"a","b","c","source"};return document;
}
const std::vector<Ref> targets{x("a"),x("b"),x("c")};
void values_are(const Document& document,std::array<double,3> expected,const char* message) {
    const auto values=evaluate(document);for(std::size_t i=0;i<targets.size();++i)near(values.at(targets[i]),expected[i],message);
}
std::map<Id,EvaluatedTransform> transforms(const Document& document){return evaluate_transforms(document,evaluate(document));}
void displaced(const Affine& actual,const Affine& before,double dx,double dy,const std::string& message) {
    for(std::size_t i=0;i<4;++i)near(actual[i],before[i],message+" linear matrix unchanged");
    near(actual[4],before[4]+dx,message+" world X moves once");near(actual[5],before[5]+dy,message+" world Y moves once");
}

void numeric_batches_and_history() {
    Session session(properties_fixture());const auto original=session.document();
    apply(session,{EditProperties{targets,400,false}});values_are(session.document(),{400,400,400},"Absolute edit replaces each selected value");
    check(session.history().states.size()==2&&session.history().states.back().label.find("3 properties")!=std::string::npos,"One scalar batch produces one meaningful History entry");
    session.undo(session.revision());check(session.document()==original,"One Undo restores the complete scalar batch");
    apply(session,{EditProperties{targets,10,true}});values_are(session.document(),{110,210,310},"Relative edit preserves each starting difference");
    const auto adjusted=session.document();session.undo(session.revision());session.redo(session.revision());check(session.document()==adjusted,"Redo preserves the same authored batch result");
    near(evaluate(session.document()).at(x("source")),50,"Unselected property stays unchanged");

    Session gesture(properties_fixture());gesture.begin_gesture(0);
    gesture.update_gesture({EditProperties{targets,10,true}});gesture.update_gesture({EditProperties{targets,25,true}});
    values_are(gesture.preview_document(),{125,225,325},"Preview recomputes all targets from gesture start");
    check(gesture.document()==original&&gesture.revision()==0,"Batch preview has no committed side effects");
    gesture.cancel_gesture();check(gesture.document()==original&&!gesture.can_undo(),"Cancellation restores every selected field without History");
    gesture.begin_gesture(0);gesture.update_gesture({EditProperties{targets,10,true}});gesture.commit_gesture();
    check(gesture.revision()==1&&gesture.history().states.size()==2,"Committed multi-property gesture is one edit");
    gesture.undo(1);check(gesture.document()==original,"Gesture Undo restores all targets together");
}

void links_and_snapshot_corrections() {
    Session session(properties_fixture());apply(session,{LinkProperties{targets,x("source"),true}});
    values_are(session.document(),{100,200,300},"Relative links preserve each starting difference");
    for(std::size_t i=0;i<targets.size();++i) {
        const auto binding=property(session.document(),targets[i]).binding;
        check(binding&&binding->source==x("source")&&binding->scale==1&&binding->offset==50+100*i&&binding->mode=="copy_local_value","Relative link stores an explicit per-target offset");
    }
    apply(session,{Set{x("source"),80}});values_are(session.document(),{130,230,330},"Source changes propagate through relative links");
    apply(session,{UnlinkProperties{{x("a"),x("c")}},Set{x("source"),100}});
    values_are(session.document(),{130,250,330},"Unlink freezes every selected starting value independently");
    apply(session,{LinkProperties{targets,x("source"),false}});values_are(session.document(),{100,100,100},"Absolute links replace old bindings with zero offsets");
    apply(session,{UnlinkProperties{targets}});
    for(const auto& target:targets)check(!property(session.document(),target).binding,"Batch Unlink removes each binding explicitly");
    apply(session,{Set{x("source"),20}});values_are(session.document(),{100,100,100},"Unlinked values stay frozen after later source edits");

    auto document=properties_fixture();Object circle;circle.id="circle";circle.name="Circle";
    circle.source=default_primitive("circle-source","nect.shape.circle");
    circle.source->parameters.at("center_x").literal=100;circle.source->parameters.at("center_y").literal=200;circle.source->parameters.at("radius").literal=50;
    const Ref east_x{"circle","circle-source-east","x"},east_y{"circle","circle-source-east","y"};
    circle.point_edit=PointEdit{"circle-source-point-edit",1,false,{}};
    circle.point_edit->overrides[east_x.point]={{"x",Scalar{900,{}}},{"y",Scalar{800,{}}}};
    document.objects.emplace(circle.id,circle);document.compositions.front().roots.push_back(circle.id);
    Session edits(document);apply(edits,{EditProperties{{east_x,east_y},10,true}});
    auto values=evaluate(edits.document());near(values.at(east_x),160,"Relative edit captures generated X before enabling Point Edit");near(values.at(east_y),210,"Later target uses the same snapshot, not a newly enabled hidden override");
    Session unlink(document);apply(unlink,{UnlinkProperties{{east_x,east_y}}});values=evaluate(unlink.document());
    near(values.at(east_x),150,"Batch Unlink freezes generated starting X");near(values.at(east_y),200,"Batch Unlink freezes starting Y before the first target enables corrections");
    Session linked(document);apply(linked,{LinkProperties{{east_x,east_y},x("source"),true}});
    near(property(linked.document(),east_x).binding->offset,100,"First relative link captures starting difference");
    near(property(linked.document(),east_y).binding->offset,150,"Later relative link shares the initial snapshot across Point Edit activation");
}

void property_failures_are_atomic() {
    Session session(properties_fixture());
    atomic_reject(session,"INVALID_BATCH",{EditProperties{{},4,false}});
    atomic_reject(session,"INVALID_BATCH",{UnlinkProperties{std::vector<Ref>(1001,x("a"))}});
    atomic_reject(session,"DUPLICATE_TARGET",{EditProperties{{x("a"),x("a")},10,true}});
    atomic_reject(session,"MISSING_REFERENCE",{EditProperties{{x("a"),{"missing","point","x"}},10,true}});
    atomic_reject(session,"UNIT_MISMATCH",{EditProperties{{x("a"),{"b","b-point","out.angle"}},10,false}});
    atomic_reject(session,"UNIT_MISMATCH",{LinkProperties{targets,{"source","source-point","in.angle"},false}});
    atomic_reject(session,"UNIT_MISMATCH",{UnlinkProperties{{x("a"),tf("b","a")}}});
    atomic_reject(session,"DEPENDENCY_CYCLE",{LinkProperties{targets,x("a"),false}});
    atomic_reject(session,"OUT_OF_RANGE",{EditProperties{targets,1e9,true}});
    atomic_reject(session,"NON_FINITE",{EditProperties{targets,std::numeric_limits<double>::quiet_NaN(),false}});
    apply(session,{Link{x("b"),{x("source"),1,0,"copy_local_value"}}});
    atomic_reject(session,"DRIVEN_PROPERTY",{EditProperties{targets,400,false}});
    atomic_reject(session,"DRIVEN_PROPERTY",{EditProperties{targets,10,true}});
    auto aliases=properties_fixture();add_default_stroke(aliases,"a");Session alias_session(aliases);
    const Ref legacy{"a","","stroke.width"},canonical=operation_ref("a",aliases.objects.at("a").legacy_stroke,"width");
    atomic_reject(alias_session,"DUPLICATE_TARGET",{EditProperties{{legacy,canonical},1,true}});
}

Document translation_fixture() {
    auto document=empty_document("document","composition","artboard");
    Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"child","sibling"};matrix(group,{0,2,-3,0,400,50});
    auto child=path("child");matrix(child,{1,0,0,1,10,20});child.anchor={Scalar{3,{}},Scalar{7,{}}};
    auto sibling=path("sibling");matrix(sibling,{2,.5,1,1,-10,5});
    Object folder;folder.id="folder";folder.name="Folder";folder.kind=Kind::group;folder.children={"follower"};matrix(folder,{2,0,0,2,1000,500});
    auto follower=path("follower");follower.transform_parent="child";matrix(follower,{1,0,0,1,5,10});
    document.objects={{"group",group},{"child",child},{"sibling",sibling},{"folder",folder},{"follower",follower}};
    document.compositions.front().roots={"group","folder"};return document;
}

void world_translation_and_selection_overlap() {
    const auto document=translation_fixture();const auto before=transforms(document);Session single(document);
    apply(single,{TranslateObjects{{"child"},30,40}});const auto moved=transforms(single.document());
    displaced(moved.at("child").world,before.at("child").world,30,40,"Rotated-parent child");
    near(moved.at("child").local[4],30,"World delta maps through rotated parent to local X");near(moved.at("child").local[5],10,"World delta maps through rotated parent to local Y");
    displaced(moved.at("follower").world,before.at("follower").world,30,40,"Unselected external follower");
    for(std::size_t i=0;i<4;++i)check(single.document().objects.at("child").transform[i]==document.objects.at("child").transform[i],"Translation preserves exact unrelated affine Scalars");
    check(single.document().objects.at("child").anchor==document.objects.at("child").anchor,"World translation preserves authored Anchor");

    for(const auto& selection:std::vector<std::vector<Id>>{{"follower","child","group"},{"group","child","follower"},{"follower","group"},{"folder","group","follower"}}) {
        Session session(document);apply(session,{TranslateObjects{selection,30,40}});const auto after=transforms(session.document());
        for(const auto& id:selection)displaced(after.at(id).world,before.at(id).world,30,40,"Overlapping selection "+id);
        check(session.document().objects.at("child").transform==document.objects.at("child").transform,"Selected ancestor carries child without a local rewrite");
        check(session.document().objects.at("follower").transform==document.objects.at("follower").transform,"External follower is moved once, across an independent structural folder");
        const auto changed=session.document();session.undo(session.revision());check(session.document()==document,"One Undo restores all selected object translations");
        session.redo(session.revision());check(session.document()==changed,"Redo restores exact multi-object authored state");
    }
    Session siblings(document);apply(siblings,{TranslateObjects{{"sibling","child"},-15,20}});
    const auto sibling_values=transforms(siblings.document());for(const auto* id:{"sibling","child"})displaced(sibling_values.at(id).world,before.at(id).world,-15,20,"Two selected siblings");
    Session tiny(properties_fixture());apply(tiny,{TranslateObjects{{"a"},1e-20,0}});
    check(property(tiny.document(),tf("a","tx")).literal==1e-20,"A representable small free translation is retained, not discarded as approximate zero");

    Session gesture(document);gesture.begin_gesture(0);gesture.update_gesture({TranslateObjects{{"group","child","follower"},30,40}});
    check(gesture.document()==document&&gesture.preview_document()!=document,"World multi-move previews only");gesture.cancel_gesture();
    check(gesture.document()==document&&!gesture.can_undo()&&gesture.revision()==0,"World multi-move cancellation restores the entire selection");
}

void translation_failures_and_singular_inheritance() {
    auto document=translation_fixture();Session session(document);
    atomic_reject(session,"INVALID_BATCH",{TranslateObjects{{},1,2}});
    atomic_reject(session,"DUPLICATE_TARGET",{TranslateObjects{{"child","child"},1,2}});
    atomic_reject(session,"MISSING_OBJECT",{TranslateObjects{{"child","missing"},1,2}});
    atomic_reject(session,"INVALID_BATCH",{TranslateObjects{std::vector<Id>(1001,"child"),1,2}});
    atomic_reject(session,"NON_FINITE",{TranslateObjects{{"child"},std::numeric_limits<double>::infinity(),1}});
    atomic_reject(session,"OUT_OF_RANGE",{TranslateObjects{{"group","child"},1e20,1}});
    document.objects.emplace("foreign",path("foreign"));document.compositions.push_back({"other-composition","Other",{"foreign"},{{"other-artboard","Page"}}});Session planes(document);
    atomic_reject(planes,"CROSS_COMPOSITION",{TranslateObjects{{"child","foreign"},10,20}});

    document=translation_fixture();document.objects.at("child").transform[4].binding=Binding{tf("folder","tx"),0,10,"copy_local_value"};Session driven(document);
    atomic_reject(driven,"DRIVEN_PROPERTY",{TranslateObjects{{"child"},30,40}});
    apply(driven,{TranslateObjects{{"group","child"},30,40}});
    check(driven.document().objects.at("child").transform==document.objects.at("child").transform,"An unchanged driven child matrix remains linked when its parent supplies the move");

    document=translation_fixture();matrix(document.objects.at("group"),{0,0,0,2,400,50});Session singular(document);
    atomic_reject(singular,"SINGULAR_TRANSFORM",{TranslateObjects{{"child"},10,20}});
    apply(singular,{TranslateObjects{{"child"},0,0}});check(singular.document()==document,"Zero displacement needs no singular inverse");
    const auto before=transforms(document);apply(singular,{TranslateObjects{{"child","group"},10,20}});
    const auto after=transforms(singular.document());for(const auto* id:{"group","child"})displaced(after.at(id).world,before.at(id).world,10,20,"Singular selected ancestor inheritance");
    check(singular.document().objects.at("child").transform==document.objects.at("child").transform,"No inverse or scalar rewrite when singular parent already supplies the full displacement");

    auto coupled=properties_fixture();coupled.objects.at("a").transform[4].literal=100;
    coupled.objects.at("b").transform_parent="a";coupled.objects.at("b").transform[4].binding=Binding{tf("a","tx"),1,0,"copy_local_value"};Session bindings(coupled);
    atomic_reject(bindings,"TRANSFORM_PRESERVATION",{TranslateObjects{{"a","b"},10,0}});
}

void translation_resolves_generated_dependencies() {
    auto document=translation_fixture();
    auto driver=path("driver",600);driver.stack.push_back(default_operation("driver-fill","nect.paint.fill"));
    driver.stack.front().parameters.at("r").literal=.5;
    Object polygon;polygon.id="polygon";polygon.name="Polygon";polygon.source=default_primitive("polygon-source","nect.shape.polygon");
    polygon.source->parameters.at("points").binding=Binding{operation_ref("driver","driver-fill","r"),10,0,"copy_local_value"};
    polygon.source->parameters.at("center_x").literal=300;polygon.source->parameters.at("rotation").literal=0;
    const Ref east{"polygon","polygon-source-outer-0-1","x"};
    document.objects.emplace(driver.id,driver);document.objects.emplace(polygon.id,polygon);
    document.compositions.front().roots.insert(document.compositions.front().roots.end(),{"driver","polygon"});
    document.objects.at("group").transform[4].binding=Binding{east,1,0,"copy_local_value"};

    for(int correction=0;correction<3;++correction) {
        auto fixture=document;
        if(correction) {
            auto& object=fixture.objects.at("polygon");object.point_edit=PointEdit{"polygon-source-point-edit",1,correction==2,{}};
            object.point_edit->overrides[east.point]["x"]=Scalar{900,Binding{x("driver"),1,0,"copy_local_value"}};
        }
        Session session(fixture);const auto before=transforms(fixture);
        near(before.at("group").world[4],correction==2?600:400,"Transform uses the active generated fallback or enabled Point Edit binding");
        apply(session,{TranslateObjects{{"child","sibling"},30,40}});const auto after=transforms(session.document());
        for(const auto* id:{"child","sibling"})displaced(after.at(id).world,before.at(id).world,30,40,"Translation follows generated-point/count/paint dependencies");
        check(session.document().objects.at("group")==fixture.objects.at("group")&&
            session.document().objects.at("polygon")==fixture.objects.at("polygon"),"Translation keeps generated dependency owners and correction state exact");
        session.undo(session.revision());check(session.document()==fixture,"Generated dependency translation is one exact Undo");
        atomic_reject(session,"OUT_OF_RANGE",{Set{x("sibling"),1e10},TranslateObjects{{"child"},30,40}});
    }

    // Re-evaluate the same dependency chain after solving. A generated source
    // depending on the edited local translation can move the parent itself.
    document.objects.at("polygon").source->parameters.at("center_x").binding=Binding{tf("child","tx"),1,0,"copy_local_value"};
    Session coupled(document);
    atomic_reject(coupled,"TRANSFORM_PRESERVATION",{TranslateObjects{{"child"},30,40}});
}
}
int main() {
    try {
        numeric_batches_and_history();links_and_snapshot_corrections();property_failures_are_atomic();
        world_translation_and_selection_overlap();translation_failures_and_singular_inheritance();
        translation_resolves_generated_dependencies();
        std::cout<<"PASS "<<checks<<" scalar batch, snapshot, world translation and atomicity checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
