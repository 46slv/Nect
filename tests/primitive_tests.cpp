#include "nect/io.hpp"
#include <cmath>
#include <iostream>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* reason) {if(!ok)throw std::runtime_error(reason);++checks;}
template<class F> void rejects(const char* code,F action) {
    try {action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
Primitive circle() {return {"circle-source","nect.shape.circle",1,
    {{"center_x",{100,{}}},{"center_y",{200,{}}},{"radius",{50,{}}}}};}
}
int main() {
    try {
        Session s(empty_document("doc","comp","art"));
        s.apply({CreatePrimitive{"comp","","circle","Circle",circle()},
            CreatePrimitive{"comp","","rect","Rectangle",{"rect-source","nect.shape.rectangle",1,
                {{"center_x",{300,{}}},{"center_y",{200,{}}},{"width",{80,{}}},{"height",{120,{}}}}}}},0);
        const Ref east{"circle","circle-source-east","x"},west{"circle","circle-source-west","x"};
        const Ref radius{"circle","","generator.radius"},rx{"rect","rect-source-top-left","x"};
        auto values=evaluate(s.document());
        check(values.at(east)==150&&values.at(west)==50,"Circle uses hand-specified anchor coordinates");
        check(std::abs(values.at({"circle","circle-source-east","out.length"})-27.61423749153968)<1e-10,
            "Circle cubic handle length");
        check(values.at(rx)==260&&values.at({"rect","rect-source-bottom-right","y"})==260,"Rectangle corners");
        check(s.document().objects.at("circle").contours.empty(),"No duplicated generated authored geometry");
        const auto generated=encode(s.document());
        check(generated.find("circle-source-east")==std::string::npos,"Generated role IDs are derived, not saved geometry");
        check(property_origin(s.document(),east)=="generated","Generated origin is discoverable");
        const auto get=request(s,R"({"op":"get","ref":{"object":"circle","point":"circle-source-east","field":"x"}})");
        check(get.find("\"authored\":null")!=std::string::npos&&get.find("\"origin\":\"generated\"")!=std::string::npos,"Generated API value is not mislabeled authored");
        rejects("OUT_OF_RANGE",[&]{s.apply({Set{east,180},Set{radius,-1}},1);});
        check(encode(s.document())==generated&&s.revision()==1,"Failed batch cannot leave a correction instance");
        rejects("GENERATED_TOPOLOGY",[&]{s.apply({CloseContour{"circle","circle-source-contour",false}},1);});
        rejects("DEPENDENCY_CYCLE",[&]{s.apply({Link{radius,{east,1,0,"copy_local_value"}}},1);});
        s.apply({Set{east,180}},1);
        check(s.document().objects.at("circle").source.has_value()&&property_origin(s.document(),east)=="point_edit",
            "Direct edit retains generator and authors a downstream correction");
        s.apply({Set{radius,80},Link{rx,{east,1,20,"copy_local_value"}}},2);
        values=evaluate(s.document());
        check(values.at(east)==180&&values.at(west)==20&&values.at(rx)==200,"Radius changes preserve overrides and driven stable references");
        s.apply({EnablePointEdit{"circle",false}},3);
        check(property_origin(s.document(),east)=="bypassed_point_edit","Disabled correction is retained and explicit");
        s.apply({Set{radius,90}},4);
        check(evaluate(s.document()).at(east)==190&&evaluate(s.document()).at(rx)==210,"Bypass restores generated output and dependent evaluation");
        const auto bypassed=encode(s.document());
        check(encode(decode(bypassed))==bypassed,"Disabled corrections persist exactly");
        s.apply({EnablePointEdit{"circle",true},Rename{"circle","Renamed"},
            ReorderObjects{"comp","",{"rect","circle"}}},5);
        check(evaluate(s.document()).at(rx)==200,"Rename/reorder/bypass retain point identity");
        s.undo(6);check(encode(s.document())==bypassed,"Undo restores source, correction bypass and hierarchy");s.redo(7);
        s.apply({Link{rx,{radius,1,0,"copy_local_value"}}},8);
        check(conversion_blockers(s.document(),"circle")==std::vector<Ref>{rx},"Conversion lists generator dependents");
        const auto blocked=encode(s.document());
        rejects("CONVERSION_REFERENCE",[&]{s.apply({ConvertToPath{"circle"}},9);});
        check(encode(s.document())==blocked&&s.revision()==9,"Blocked conversion is atomic");
        const Ref east_y{"circle","circle-source-east","y"};
        const Ref rect_y{"rect","","generator.center_y"};
        s.apply({Unlink{rx},Link{east_y,{rect_y,1,10,"copy_local_value"}},ConvertToPath{"circle"}},9);
        const auto& converted=s.document().objects.at("circle");
        check(!converted.source&&!converted.point_edit&&converted.contours.front().id=="circle-source-contour",
            "Explicit conversion keeps contour identity and removes procedural source");
        check(property(s.document(),east_y).binding->source==rect_y&&evaluate(s.document()).at(east_y)==210,
            "Active correction bindings survive conversion");
        check(evaluate(s.document()).at(east)==180&&evaluate(s.document()).at(rx)==90,"Conversion freezes fallback geometry and explicit unlinks");
        s.undo(10);check(encode(s.document())==blocked,"Conversion undo restores all authored procedural state");
        s.redo(11);check(encode(decode(encode(s.document())))==encode(s.document()),"Converted scene reopens deterministically");
        check(export_svg(s.document(),"comp","art").find("M 180 210 C ")!=std::string::npos,"SVG uses corrected primitive geometry");

        auto invalid=decode(generated);
        invalid.objects.at("circle").source->version=2;
        rejects("UNSUPPORTED_OPERATOR_VERSION",[&]{validate(invalid);});
        invalid=decode(generated);invalid.objects.at("circle").source->type="nect.future.shape";
        rejects("UNSUPPORTED_OPERATOR",[&]{validate(invalid);});
        invalid=decode(generated);invalid.objects.at("circle").point_edit=PointEdit{"circle-source-point-edit",1,true,{{"missing",{{"x",{12,{}}}}}}};
        rejects("UNRESOLVED_POINT_EDIT",[&]{validate(invalid);});
        invalid=decode(generated);invalid.objects.at("circle").source->parameters.erase("radius");
        rejects("INVALID_GENERATOR_PARAMETERS",[&]{validate(invalid);});
        invalid=decode(generated);invalid.objects.at("circle").source->parameters.emplace("future",Scalar{});
        rejects("INVALID_GENERATOR_PARAMETERS",[&]{validate(invalid);});
        std::cout<<"PASS "<<checks<<" primitive, correction and conversion checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
