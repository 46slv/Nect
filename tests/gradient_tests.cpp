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
    auto has_property=[&](const Ref& ref) {
        const auto refs=properties(s.document());
        return std::find(refs.begin(),refs.end(),ref)!=refs.end();
    };
    apply({CreatePrimitive{"comp","","rect","Gradient",{"source","nect.shape.rectangle",1,
        {{"center_x",{100,{}}},{"center_y",{100,{}}},{"width",{200,{}}},{"height",{200,{}}}}}}});
    auto fill=default_operation("fill","nect.paint.fill");fill.parameters["a"].literal=.5;
    apply({RemoveOperation{"rect",s.document().objects.at("rect").legacy_stroke},AddOperation{"rect",fill,0}});
    const auto enabled_ref=gradient_ref("rect","fill","gradient","enabled");
    rejects("MISSING_GRADIENT",[&]{(void)gradient_enabled_property(s.document(),enabled_ref);});
    check(!has_property(enabled_ref),
        "A paint operation without an authored gradient has no gradient enabled property");
    Gradient g;g.id="gradient";g.end_x.literal=200;
    GradientStop red;red.id="red";red.rgba[0].literal=1;red.rgba[3].literal=.25;
    GradientStop blue;blue.id="blue";blue.offset.literal=1;blue.rgba[2].literal=1;
    g.stops={blue,red};apply({SetGradient{"rect","fill",g}});
    const auto red_ref=gradient_ref("rect","fill","gradient","stop.red.r");
    const auto x_ref=gradient_ref("rect","fill","gradient","end_x");
    const auto operation_enabled_ref=operation_ref("rect","fill","enabled");
    check(gradient_enabled_property(s.document(),enabled_ref)&&operation_enabled_property(s.document(),operation_enabled_ref)&&
        property_unit(enabled_ref)=="boolean"&&
        !evaluate(s.document()).contains(enabled_ref)&&
        has_property(enabled_ref)&&
        resolve_name(s.document(),"Gradient","","op.fill.gradient.gradient.enabled")==enabled_ref,
        "Gradient bypass discovery is typed and distinct from its paint operation enabled bit");
    rejects("MISSING_OPERATION",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","missing","gradient","enabled"));});
    rejects("MISSING_REFERENCE",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("missing-object","fill","gradient","enabled"));});
    rejects("MISSING_GRADIENT",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","fill","missing","enabled"));});
    rejects("INVALID_GRADIENT_REF",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","fill","","enabled"));});
    rejects("INVALID_GRADIENT_REF",[&]{(void)gradient_enabled_property(s.document(),Ref{"rect","not-empty",enabled_ref.field});});
    rejects("UNKNOWN_GRADIENT_PROPERTY",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","fill","gradient","enabled.extra"));});
    rejects("MISSING_REFERENCE",[&]{apply({Set{enabled_ref,0}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{enabled_ref,{operation_ref("rect","fill","b"),1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{enabled_ref},{"1",1},false}});});
    const auto failed_batch_revision=s.revision();
    rejects("MISSING_REFERENCE",[&]{apply({Set{x_ref,25},Set{enabled_ref,0}});});
    check(s.revision()==failed_batch_revision&&property(s.document(),x_ref).literal==200,
        "An invalid Scalar edit after a valid gradient Scalar edit rejects the whole batch");
    auto current=[&]{return evaluate_shape(s.document(),"rect",evaluate(s.document()));};
    auto shape=current();
    check(shape.paints[0].gradient->stops[0].rgba[0]==1&&shape.paints[0].gradient->stops[1].rgba[2]==1,"Evaluation sorts offsets independently of authored stop order");
    check(shape.paints[0].rgba[3]==.5&&shape.paints[0].gradient->stops[0].rgba[3]==.25,"Overall alpha and stop alpha remain distinct");
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
    check(!gradient_enabled_property(s.document(),enabled_ref)&&operation_enabled_property(s.document(),operation_enabled_ref)&&
        !current().paints[0].gradient&&evaluate(s.document()).at(operation_ref("rect","fill","b"))==.8&&
        has_property(enabled_ref),
        "Solid bypass reads false while retaining its stable Ref and separate paint operation state");
    s.undo(s.revision());check(gradient_enabled_property(s.document(),enabled_ref)&&current().paints[0].gradient.has_value(),
        "Undo restores the authored gradient bypass through the same Ref");
    s.begin_gesture(s.revision());s.update_gesture({Set{x_ref,260}});s.update_gesture({Set{x_ref,300}});s.commit_gesture();
    check(property(s.document(),x_ref).literal==300,"Endpoint gesture commits its final preview");
    s.undo(s.revision());check(property(s.document(),x_ref).literal==200,"Endpoint gesture is one Undo step");
    auto repeat=default_operation("repeat","nect.shape.repeater");repeat.parameters["copies"].literal=2;repeat.parameters["end_opacity"].literal=.2;
    apply({AddOperation{"rect",repeat,1}});shape=current();
    rejects("INVALID_DOMAIN",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","repeat","gradient","enabled"));});
    check(shape.paints.size()==2&&shape.paints[0].transform[4]==100&&std::abs(shape.paints[0].rgba[3]-.1)<1e-12&&shape.paints[0].gradient->end.x==200,"Repeat after paint carries gradient with transformed paint and opacity");
    apply({ReorderOperations{"rect",{"repeat","fill"}}});shape=current();
    check(shape.paints.size()==1&&shape.paints[0].paths.size()==2&&shape.paints[0].transform==identity_matrix,"Repeat before paint shares one gradient across the compound geometry");
    apply({Rename{"rect","Renamed Gradient"}});
    check(resolve_name(s.document(),"Renamed Gradient","",enabled_ref.field)==enabled_ref&&
        gradient_enabled_property(s.document(),enabled_ref),"Rename and operation reorder preserve the nested Ref identity");
    auto wrong_kind=s.document();
    auto wrong_kind_fill=std::find_if(wrong_kind.objects.at("rect").stack.begin(),wrong_kind.objects.at("rect").stack.end(),
        [](const auto& operation){return operation.id=="fill";});
    wrong_kind_fill->type="nect.shape.repeater";
    rejects("INVALID_DOMAIN",[&]{(void)gradient_enabled_property(wrong_kind,enabled_ref);});
    apply({DuplicateObjects{{"rect"},"gradient-copy"}});
    const auto copied_object_it=std::find_if(s.document().objects.begin(),s.document().objects.end(),
        [](const auto& item){return item.first!="rect"&&item.second.name!="Gradient"&&item.second.kind==Kind::path;});
    check(copied_object_it!=s.document().objects.end(),"Object duplication creates an independently identified paint owner");
    const auto& copied_object=copied_object_it->second;
    const auto copied_fill=std::find_if(copied_object.stack.begin(),copied_object.stack.end(),
        [](const auto& operation){return operation.type=="nect.paint.fill";});
    check(copied_fill!=copied_object.stack.end()&&copied_fill->gradient&&copied_fill->gradient->id!="gradient"&&
        gradient_enabled_property(s.document(),gradient_ref(copied_object.id,copied_fill->id,copied_fill->gradient->id,"enabled")),
        "Duplicating a paint remaps its gradient identity while preserving a readable bypass");
    auto radial=*s.document().objects.at("rect").stack[1].gradient;radial.type="radial";
    apply({SetGradient{"rect","fill",radial}});
    const auto svg=export_svg(s.document(),"comp","art");
    check(svg.find("radialGradient")!=std::string::npos&&svg.find("gradientUnits=\"userSpaceOnUse\"")!=std::string::npos&&svg.find("fill-opacity=\"0.5\"")!=std::string::npos,"SVG preserves gradient coordinate space and overall opacity");
    auto invalid=s.document();invalid.objects.at("rect").stack[1].gradient->stops[0].id="source";
    rejects("DUPLICATE_ID",[&]{validate(invalid);});
    invalid=s.document();invalid.objects.at("rect").stack[1].gradient->type="conic";
    rejects("UNSUPPORTED_GRADIENT",[&]{validate(invalid);});
    apply({Unlink{operation_ref("rect","fill","b")}});
    auto replacement_gradient=*s.document().objects.at("rect").stack[1].gradient;replacement_gradient.id="replacement-gradient";
    apply({SetGradient{"rect","fill",replacement_gradient}});
    const auto replacement_ref=gradient_ref("rect","fill","replacement-gradient","enabled");
    rejects("MISSING_GRADIENT",[&]{(void)gradient_enabled_property(s.document(),enabled_ref);});
    check(gradient_enabled_property(s.document(),replacement_ref)&&
        !has_property(enabled_ref)&&has_property(replacement_ref),
        "Replacing with a new gradient ID removes the old Ref and exposes the new one");
    apply({SetGradient{"rect","fill",{}}});
    rejects("MISSING_GRADIENT",[&]{(void)gradient_enabled_property(s.document(),replacement_ref);});
    check(!current().paints[0].gradient&&!has_property(replacement_ref),
        "Removing a gradient removes its typed enabled property");
    std::cout<<"PASS "<<checks<<" gradient checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
