#include "nect/core.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

using namespace nect;
namespace {
int checks=0;
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);++checks;}
void near(double actual,double expected,const char* message) {check(std::abs(actual-expected)<1e-9,message);}
template<class F> void rejects(const char* code,F action) {
    try {action();}catch(const Error& error) {
        check(error.code==code,("Expected "+std::string(code)+", got "+error.code+": "+error.what()).c_str());return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands) {session.apply(commands,session.revision());}
Primitive primitive(const Id& id,const std::string& type,double count) {
    auto result=default_primitive(id,"nect.shape."+type);result.parameters.at("points").literal=count;return result;
}
Session scene(const std::string& type="polygon",double count=5) {
    Session session(empty_document("document","composition","artboard"));
    apply(session,{CreatePrimitive{"composition","","shape","Shape",primitive("source",type,count)},
        CreatePath{"composition","","driver","Driver",{{"driver-contour",false,{{"driver-point"}}}}}});
    return session;
}
std::vector<Id> point_ids(const Document& document,const Id& object="shape") {
    const auto values=evaluate(document);std::vector<Id> ids;
    for(const auto& contour:path_contours(document.objects.at(object),&values))for(const auto& point:contour.points)ids.push_back(point.id);
    return ids;
}
const Ref count{"shape","","generator.points"};
const Ref driver_count{"driver","","transform.a"};
const Ref driver_x{"driver","driver-point","x"};
const Ref fifth_x{"shape","source-outer-1-5","x"};

void geometry() {
    check(default_primitive("circle","nect.shape.circle").parameters.at("radius").literal==100,"Circle default remains 100");
    const auto rectangle=default_primitive("rectangle","nect.shape.rectangle");
    check(rectangle.parameters.at("width").literal==220&&rectangle.parameters.at("height").literal==140,"Rectangle defaults remain 220 by 140");
    check(default_primitive("polygon","nect.shape.polygon").parameters.at("points").literal==6,"Polygon defaults to six corners");
    check(default_primitive("star","nect.shape.star").parameters.at("points").literal==5,"Star defaults to five outer corners");
    auto polygon=scene("polygon",4);
    apply(polygon,{Set{{"shape","","generator.center_x"},100},Set{{"shape","","generator.center_y"},200},Set{{"shape","","generator.radius"},50}});
    const std::vector<Id> square{"source-outer-0-1","source-outer-1-4","source-outer-1-2","source-outer-3-4"};
    check(point_ids(polygon.document())==square,"Square IDs are reduced rational phases in contour order");
    const auto values=evaluate(polygon.document());
    const std::array<Vec2,4> expected{{{100,150},{150,200},{100,250},{50,200}}};
    for(std::size_t i=0;i<square.size();++i) {
        near(values.at({"shape",square[i],"x"}),expected[i].x,"Square anchor X");
        near(values.at({"shape",square[i],"y"}),expected[i].y,"Square anchor Y");
        for(const auto* field:{"in.length","out.length","in.angle","out.angle"})near(values.at({"shape",square[i],field}),0,"Straight polygon handles are zero");
    }
    apply(polygon,{Set{{"shape","","generator.rotation"},0}});
    near(evaluate(polygon.document()).at({"shape",square[0],"x"}),150,"Rotation uses degrees around generator center");
    check(property_unit(count)=="scalar"&&property_unit({"shape","","generator.rotation"})=="degree","Count and rotation expose ordinary compatible units");
    const auto shape=evaluate_shape(polygon.document(),"shape",evaluate(polygon.document()));
    check(shape.paths.size()==1&&shape.paths.front().contours->front().closed&&shape.paths.front().contours->front().points.size()==4,"Core shape projection consumes generated polygon contour");

    auto star=scene("star",2);
    const std::vector<Id> star_ids{"source-outer-0-1","source-inner-1-4","source-outer-1-2","source-inner-3-4"};
    check(point_ids(star.document())==star_ids,"Star keeps separate outer/inner phase lineage");
    const auto star_values=evaluate(star.document());
    const std::array<Vec2,4> star_expected{{{0,-100},{50,0},{0,100},{-50,0}}};
    for(std::size_t i=0;i<star_ids.size();++i) {
        near(star_values.at({"shape",star_ids[i],"x"}),star_expected[i].x,"Two-point Star X");
        near(star_values.at({"shape",star_ids[i],"y"}),star_expected[i].y,"Two-point Star Y");
        near(star_values.at({"shape",star_ids[i],"out.length"}),0,"Star handles stay straight");
    }
    apply(star,{Set{{"shape","","generator.inner_radius"},150}});
    near(evaluate(star.document()).at({"shape",star_ids[1],"x"}),150,"Inner radius larger than outer is intentional and retained");
    apply(star,{Set{{"shape","","generator.inner_radius"},0},Set{{"shape","","generator.outer_radius"},0}});
    check(point_ids(star.document())==star_ids,"Zero radius does not merge stable point identities");
    apply(star,{Set{count,256}});check(point_ids(star.document()).size()==512,"Maximum Star yields 512 distinct vertices");
    const auto stable=star.document();const auto revision=star.revision();
    for(const auto value:{1.0,257.0,3.5})rejects("OUT_OF_RANGE",[&]{apply(star,{Set{count,value}});});
    rejects("OUT_OF_RANGE",[&]{apply(star,{Set{{"shape","","generator.inner_radius"},-1}});});
    check(star.document()==stable&&star.revision()==revision,"Invalid counts and radii leave state and revision untouched");
    rejects("OUT_OF_RANGE",[&]{apply(polygon,{Set{count,2}});});
    rejects("OUT_OF_RANGE",[&]{apply(polygon,{Set{{"shape","","generator.radius"},-1}});});
}

void bindings_and_cycles() {
    auto session=scene();
    apply(session,{Set{driver_count,10},Link{count,{driver_count,1,0,"copy_local_value"}}});
    check(point_ids(session.document()).size()==10,"Bound count determines actual active topology");
    rejects("EVALUATION_REQUIRED",[&]{(void)path_contours(session.document().objects.at("shape"));});
    const auto refs=properties(session.document());
    check(std::count_if(refs.begin(),refs.end(),[](const Ref& ref){return ref.object=="shape"&&!ref.point.empty();})==60,"Discovery exposes six fields per actual generated vertex");
    check(std::find(refs.begin(),refs.end(),Ref{"shape","source-outer-1-10","x"})!=refs.end(),"Discovery includes evaluated count roles");
    apply(session,{Set{driver_count,5}});
    check(point_ids(session.document()).size()==5&&property(session.document(),fifth_x).literal>95,"Generated property lookup uses evaluated count");
    const auto before=session.document();const auto revision=session.revision();
    rejects("OUT_OF_RANGE",[&]{apply(session,{Set{driver_count,2}});});
    rejects("OUT_OF_RANGE",[&]{apply(session,{Set{driver_count,5.5}});});
    rejects("DEPENDENCY_CYCLE",[&]{apply(session,{Link{driver_count,{count,1,0,"copy_local_value"}}});});
    rejects("UNIT_MISMATCH",[&]{apply(session,{Link{count,{{"shape","","generator.radius"},1,0,"copy_local_value"}}});});
    rejects("DEPENDENCY_CYCLE",[&]{apply(session,{Link{{"shape","","generator.center_x"},{fifth_x,1,0,"copy_local_value"}}});});
    check(session.document()==before&&session.revision()==revision,"Failed dynamic count, unit and geometry cycles are atomic");
    // A generated property cannot provide the count needed to establish its own role.
    rejects("DEPENDENCY_CYCLE",[&]{apply(session,{Link{count,{fifth_x,1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{(void)property(session.document(),{"shape","source-outer-1-6","x"});});
    rejects("MISSING_REFERENCE",[&]{apply(session,{Link{fifth_x,{{"missing","point","x"},1,0,"copy_local_value"}},EnablePointEdit{"shape",false}});});
    check(!session.document().objects.at("shape").point_edit,"Rejected bypassed bindings cannot leave hidden corrections");
    apply(session,{Link{fifth_x,{fifth_x,1,0,"copy_local_value"}},EnablePointEdit{"shape",false}});
    check(!session.document().objects.at("shape").point_edit->enabled,"Dormant-only cycles preserve earlier bypass semantics");
    const auto dormant=session.document();
    rejects("DEPENDENCY_CYCLE",[&]{apply(session,{EnablePointEdit{"shape",true}});});
    rejects("UNRESOLVED_POINT_EDIT",[&]{apply(session,{Set{driver_count,6}});});
    check(session.document()==dormant,"Enabling a cycle or dropping its bypassed role still rejects atomically");
}

void identity_and_clear() {
    auto session=scene();
    apply(session,{Set{fifth_x,123},Link{driver_x,{fifth_x,1,7,"copy_local_value"}}});
    apply(session,{Set{count,10}});
    auto ids=point_ids(session.document());
    check(ids[2]==fifth_x.point&&evaluate(session.document()).at(driver_x)==130,"Five to ten retains one-fifth correction and incoming reference");
    const auto corrected=session.document();const auto revision=session.revision();
    rejects("UNRESOLVED_POINT_EDIT",[&]{apply(session,{Set{count,6}});});
    rejects("MISSING_REFERENCE",[&]{apply(session,{ClearPointEdit{"shape"},Set{count,6}});});
    check(session.document()==corrected&&session.revision()==revision,"Neither dropped correction nor external reference silently remaps");
    apply(session,{EnablePointEdit{"shape",false}});
    const auto bypassed=session.document();
    rejects("UNRESOLVED_POINT_EDIT",[&]{apply(session,{Set{count,6}});});
    check(session.document()==bypassed,"Bypass retains role protection");
    apply(session,{Unlink{driver_x},Set{count,6},ClearPointEdit{"shape"}});
    const auto cleared=session.document();
    check(!cleared.objects.at("shape").point_edit&&point_ids(cleared).size()==6,"Explicit reset may follow count change within the same atomic batch");
    session.undo(session.revision());check(session.document()==bypassed,"Undo restores source count, bypass state, corrections and bindings exactly");
    session.redo(session.revision());check(session.document()==cleared,"Redo replays the reset and topology atomically");
    check(session.history().states.back().label.find("3")!=std::string::npos,"Topology reset batch receives normal multi-command History label");

    auto bound=scene();
    apply(bound,{Set{driver_count,5},Link{count,{driver_count,1,0,"copy_local_value"}},Set{fifth_x,321}});
    apply(bound,{Set{driver_count,10}});
    check(evaluate(bound.document()).at(fifth_x)==321,"Bound five-to-ten count preserves corrected angular role");
    const auto bound_before=bound.document();
    rejects("UNRESOLVED_POINT_EDIT",[&]{apply(bound,{Set{driver_count,6}});});
    check(bound.document()==bound_before,"Indirect driver edit cannot drop corrected point");
    // Creation must not force evaluation of an intermediate transaction before
    // its explicit correction reset, even when the count is a driven property.
    apply(bound,{Set{driver_count,6},CreatePath{"composition","","new-path","New path",{{"new-contour",false,{{"new-point"}}}}},ClearPointEdit{"shape"}});
    check(point_ids(bound.document()).size()==6&&bound.document().objects.contains("new-path"),"Normal creation composes with final-state topology validation");

    auto hidden=scene();
    apply(hidden,{CreatePrimitive{"composition","","other","Other",default_primitive("other-source","nect.shape.circle")},
        Link{{"other","other-source-east","x"},{fifth_x,1,0,"copy_local_value"}},EnablePointEdit{"other",false}});
    rejects("MISSING_REFERENCE",[&]{apply(hidden,{Set{count,6}});});
    apply(hidden,{ClearPointEdit{"other"},Set{count,6}});
    check(point_ids(hidden.document()).size()==6,"Clearing a bypassed external binding explicitly permits topology change");

    auto star=scene("star",5);const Ref inner{"shape","source-inner-1-10","x"};
    apply(star,{Set{inner,77}});
    rejects("UNRESOLVED_POINT_EDIT",[&]{apply(star,{Set{count,10}});});
    check(property(star.document(),inner).literal==77,"An inner lineage never retargets an outer point at the same phase");
    rejects("GENERATED_TOPOLOGY",[&]{apply(star,{CloseContour{"shape","source-contour",false}});});
}

void conversion() {
    auto session=scene();
    apply(session,{Set{driver_count,10},Link{count,{driver_count,1,0,"copy_local_value"}},Set{driver_x,88},
        Link{fifth_x,{driver_x,1,2,"copy_local_value"}}});
    const auto before=session.document();const auto ids=point_ids(before);
    apply(session,{ConvertToPath{"shape"}});
    const auto& object=session.document().objects.at("shape");
    check(!object.source&&!object.point_edit&&point_ids(session.document())==ids,"Conversion freezes evaluated bound topology and preserves every point ID");
    check(property(session.document(),fifth_x).binding->source==driver_x&&evaluate(session.document()).at(fifth_x)==90,"Conversion retains active correction binding");
    check(object.stack==before.objects.at("shape").stack,"Conversion keeps later editable paint stack");
    session.undo(session.revision());check(session.document()==before,"Conversion Undo restores entire authored procedural state");
    apply(session,{Link{{"driver","","transform.b"},{count,1,0,"copy_local_value"}}});
    check(conversion_blockers(session.document(),"shape")==std::vector<Ref>{{"driver","","transform.b"}},"Count parameter dependents are conversion blockers");
    const auto blocked=session.document();
    rejects("CONVERSION_REFERENCE",[&]{apply(session,{ConvertToPath{"shape"}});});
    check(session.document()==blocked,"Blocked conversion preserves source and references");
}
}

int main() {
    try {geometry();bindings_and_cycles();identity_and_clear();conversion();
        std::cout<<"PASS "<<checks<<" Polygon/Star geometry, dynamic topology, identity and atomic correction checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
