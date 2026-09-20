#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection");}
}
int main(){try{
    Session s(empty_document("doc","comp","art"));
    auto apply=[&](std::vector<Command> cmds){s.apply(cmds,s.revision());};
    apply({CreatePrimitive{"comp","","rect","Gradient",{"source","nect.shape.rectangle",1,
        {{"center_x",{100,{}}},{"center_y",{100,{}}},{"width",{200,{}}},{"height",{200,{}}}}}}});
    auto fill=default_operation("fill","nect.paint.fill");fill.parameters["a"].literal=.5;
    apply({RemoveOperation{"rect",s.document().objects.at("rect").legacy_stroke},AddOperation{"rect",fill,0}});
    Gradient g;g.id="gradient";g.end_x.literal=200;
    GradientStop red;red.id="red";red.rgba[0].literal=1;red.rgba[3].literal=.25;
    GradientStop blue;blue.id="blue";blue.offset.literal=1;blue.rgba[2].literal=1;
    g.stops={blue,red};apply({SetGradient{"rect","fill",g}});
    auto current=[&]{return evaluate_shape(s.document(),"rect",evaluate(s.document()));};
    auto shape=current();
    check(shape.paints[0].gradient->stops[0].rgba[0]==1&&shape.paints[0].gradient->stops[1].rgba[2]==1,"Evaluation sorts offsets independently of authored stop order");
    check(shape.paints[0].rgba[3]==.5&&shape.paints[0].gradient->stops[0].rgba[3]==.25,"Overall alpha and stop alpha remain distinct");
    const auto red_ref=gradient_ref("rect","fill","gradient","stop.red.r");
    const auto x_ref=gradient_ref("rect","fill","gradient","end_x");
    check(property_unit(x_ref)=="du"&&property_unit(red_ref)=="scalar","Gradient positions and colors have explicit units");
    apply({Link{operation_ref("rect","fill","b"),{red_ref,1,0,"copy_local_value"}}});
    std::reverse(g.stops.begin(),g.stops.end());apply({SetGradient{"rect","fill",g},Set{red_ref,.8}});
    check(evaluate(s.document()).at(operation_ref("rect","fill","b"))==.8,"Reordering authored stops preserves reference identity");
    const auto saved=encode(s.document());
    check(encode(decode(saved))==saved,"Native retains gradient order, Scalars, bindings and IDs exactly");
    rejects("MISSING_REFERENCE",[&]{apply({SetGradient{"rect","fill",{}}});});
    auto removed=*s.document().objects.at("rect").stack[0].gradient;
    removed.stops.erase(removed.stops.begin());GradientStop replacement=red;replacement.id="new-red";removed.stops.push_back(replacement);
    rejects("MISSING_REFERENCE",[&]{apply({SetGradient{"rect","fill",removed}});});
    check(encode(s.document())==saved,"Deleting a linked stop or component rejects atomically");
    rejects("GRADIENT_STOPS",[&]{apply({Set{gradient_ref("rect","fill","gradient","stop.blue.offset"),0}});});
    rejects("GRADIENT_GEOMETRY",[&]{apply({Set{x_ref,0}});});
    rejects("OUT_OF_RANGE",[&]{apply({Set{red_ref,1.01}});});
    rejects("UNIT_MISMATCH",[&]{apply({Link{x_ref,{red_ref,1,0,"copy_local_value"}}});});
    rejects("DEPENDENCY_CYCLE",[&]{apply({Link{red_ref,{operation_ref("rect","fill","b"),1,0,"copy_local_value"}}});});
    check(encode(s.document())==saved,"All invalid gradient edits preserve authored state");
    auto bypass=*s.document().objects.at("rect").stack[0].gradient;bypass.enabled=false;
    apply({SetGradient{"rect","fill",bypass}});
    check(!current().paints[0].gradient&&evaluate(s.document()).at(operation_ref("rect","fill","b"))==.8,"Solid bypass retains stop references");
    s.undo(s.revision());check(current().paints[0].gradient.has_value(),"Undo restores gradient mode");
    s.begin_gesture(s.revision());s.update_gesture({Set{x_ref,260}});s.update_gesture({Set{x_ref,300}});s.commit_gesture();
    check(property(s.document(),x_ref).literal==300,"Endpoint gesture commits its final preview");
    s.undo(s.revision());check(property(s.document(),x_ref).literal==200,"Endpoint gesture is one Undo step");
    auto repeat=default_operation("repeat","nect.shape.repeater");repeat.parameters["copies"].literal=2;repeat.parameters["end_opacity"].literal=.2;
    apply({AddOperation{"rect",repeat,1}});shape=current();
    check(shape.paints.size()==2&&shape.paints[0].transform[4]==100&&std::abs(shape.paints[0].rgba[3]-.1)<1e-12&&shape.paints[0].gradient->end.x==200,"Repeat after paint carries gradient with transformed paint and opacity");
    apply({ReorderOperations{"rect",{"repeat","fill"}}});shape=current();
    check(shape.paints.size()==1&&shape.paints[0].paths.size()==2&&shape.paints[0].transform==identity_matrix,"Repeat before paint shares one gradient across the compound geometry");
    auto radial=*s.document().objects.at("rect").stack[1].gradient;radial.type="radial";
    apply({SetGradient{"rect","fill",radial}});
    const auto svg=export_svg(s.document(),"comp","art");
    check(svg.find("radialGradient")!=std::string::npos&&svg.find("gradientUnits=\"userSpaceOnUse\"")!=std::string::npos&&svg.find("fill-opacity=\"0.5\"")!=std::string::npos,"SVG preserves gradient coordinate space and overall opacity");
    auto invalid=s.document();invalid.objects.at("rect").stack[1].gradient->stops[0].id="source";
    rejects("DUPLICATE_ID",[&]{validate(invalid);});
    invalid=s.document();invalid.objects.at("rect").stack[1].gradient->type="conic";
    rejects("UNSUPPORTED_GRADIENT",[&]{validate(invalid);});
    apply({Unlink{operation_ref("rect","fill","b")},SetGradient{"rect","fill",{}}});
    check(!current().paints[0].gradient,"Explicit freeze permits gradient removal");
    std::cout<<"PASS "<<checks<<" gradient checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
