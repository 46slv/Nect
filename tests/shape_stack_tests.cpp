#include "nect/io.hpp"
#include <cmath>
#include <iostream>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection: "+std::string(code));}
}
int main(){try{
    Session s(empty_document("doc","comp","art"));
    s.apply({CreatePrimitive{"comp","","rect","Motif",{"source","nect.shape.rectangle",1,
        {{"center_x",{10,{}}},{"center_y",{10,{}}},{"width",{20,{}}},{"height",{10,{}}}}}}},0);
    const auto stroke=s.document().objects.at("rect").legacy_stroke;
    auto fill=default_operation("fill","nect.paint.fill");fill.parameters["r"].literal=1;fill.parameters["a"].literal=.5;
    auto repeat=default_operation("repeat","nect.shape.repeater");
    s.apply({AddOperation{"rect",fill,0},AddOperation{"rect",repeat,2}},1);
    auto evaluate_current=[&]{return evaluate_shape(s.document(),"rect",evaluate(s.document()));};
    auto shape=evaluate_current();
    check(shape.paths.size()==3&&shape.paints.size()==6,"Repeat after paint produces three independently painted copies");
    check(shape.paints[0].operation==stroke&&shape.paints[1].operation=="fill","Earlier paint entries composite above later by default");
    check(shape.paints.front().transform[4]==200&&shape.paints.back().transform[4]==0,"Repeater below places original on top");
    const auto top_left=map_point(shape.paths[2].transform,shape.paths[2].contours->front().points.front().anchor);
    check(top_left.x==200&&top_left.y==5,"Third virtual copy has hand-specified translated coordinates");
    s.apply({ReorderOperations{"rect",{"repeat","fill",stroke}}},2);
    shape=evaluate_current();
    check(shape.paints.size()==2&&shape.paints[0].paths.size()==3,"Repeat before paint produces a compound path per paint");
    check(shape.paints[0].transform==identity_matrix,"Compound paint is applied after path transforms");
    s.apply({OperationOptions{"rect","fill","above","evenodd"},EnableOperation{"rect","repeat",false}},3);
    shape=evaluate_current();check(shape.paths.size()==1&&shape.paints.size()==2&&shape.paints.back().fill_rule=="evenodd","Bypass preserves input; fill rule persists into evaluation");
    s.undo(4);check(evaluate_current().paths.size()==3,"Stack operation Undo restores evaluation");s.redo(5);
    const auto rr=operation_ref("rect","repeat","rotation");
    s.apply({EnableOperation{"rect","repeat",true},Set{operation_ref("rect","repeat","position_x"),0},
        Set{operation_ref("rect","repeat","anchor_x"),10},Set{operation_ref("rect","repeat","anchor_y"),10},
        Set{rr,90}},6);
    shape=evaluate_current();const auto rotated=map_point(shape.paths[1].transform,{20,10});
    check(std::abs(rotated.x-10)<1e-10&&std::abs(rotated.y-20)<1e-10,"Y-down clockwise rotation around authored repeat anchor");
    s.apply({Set{operation_ref("rect","repeat","scale_x"),2},Set{operation_ref("rect","repeat","scale_y"),2},
        Set{operation_ref("rect","repeat","offset"),1}},7);
    shape=evaluate_current();const auto scaled=map_point(shape.paths[0].transform,{20,10});
    check(std::abs(scaled.x-10)<1e-10&&std::abs(scaled.y-30)<1e-10,"Offset shifts transform exponent; scale is multiplicative");
    auto stored=encode(s.document());check(encode(decode(stored))==stored,"Ordered operators and parameters reopen deterministically");
    rejects("OUT_OF_RANGE",[&]{s.apply({Set{operation_ref("rect","repeat","copies"),2.5}},8);});
    rejects("OUT_OF_RANGE",[&]{s.apply({Set{operation_ref("rect","repeat","scale_x"),0}},8);});
    auto nested=default_operation("nested","nect.shape.repeater");nested.parameters["copies"].literal=1000;
    rejects("OUTPUT_LIMIT",[&]{s.apply({Set{operation_ref("rect","repeat","scale_x"),1},Set{operation_ref("rect","repeat","scale_y"),1},
        Set{operation_ref("rect","repeat","copies"),1000},AddOperation{"rect",nested,3}},8);});
    check(encode(s.document())==stored&&s.revision()==8,"Output-limit and parameter failures are atomic");
    const Ref point{"rect","source-top-left","x"};
    s.apply({Link{point,{{"rect","","stroke.width"},1,4,"copy_local_value"}}},8);
    s.apply({Set{operation_ref("rect",stroke,"width"),7}},9);
    check(evaluate(s.document()).at(point)==11&&property(s.document(),{"rect","","stroke.width"}).literal==7,"Legacy address and canonical operation share one Scalar authority");
    stored=encode(s.document());
    rejects("MISSING_REFERENCE",[&]{s.apply({RemoveOperation{"rect",stroke}},10);});
    check(encode(s.document())==stored,"Removing an operation cannot break stable references");
    s.apply({Unlink{point},RemoveOperation{"rect",stroke}},10);
    check(evaluate(s.document()).at(point)==11&&s.document().objects.at("rect").legacy_stroke.empty(),"Explicit freeze permits removing referenced legacy stroke");
    s.apply({Set{operation_ref("rect","repeat","copies"),0}},11);
    check(evaluate_current().paths.empty(),"Zero copies produces no shape geometry");
    s.apply({ConvertToPath{"rect"}},12);
    check(!s.document().objects.at("rect").source&&s.document().objects.at("rect").stack.size()==2&&
        s.document().objects.at("rect").stack.front().id=="repeat","Source conversion preserves the later editable shape stack");
    auto invalid=s.document();invalid.objects.at("rect").stack.front().version=2;
    rejects("UNSUPPORTED_OPERATOR_VERSION",[&]{validate(invalid);});
    std::cout<<"PASS "<<checks<<" ordered shape stack checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
