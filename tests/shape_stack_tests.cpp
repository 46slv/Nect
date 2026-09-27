#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection: "+std::string(code));}
void operation_enabled_read_contract() {
    Session session(empty_document("enabled-doc","enabled-comp","enabled-art"));
    session.apply({CreatePrimitive{"enabled-comp","","enabled-path","Enabled Path",
        default_primitive("enabled-shape","nect.shape.rectangle")}},0);
    session.apply({AddOperation{"enabled-path",default_operation("enabled-fill","nect.paint.fill"),1}},session.revision());
    const Ref ref=operation_ref("enabled-path","enabled-fill","enabled");
    const auto refs=properties(session.document());
    check(std::find(refs.begin(),refs.end(),ref)!=refs.end()&&
        resolve_name(session.document(),"Enabled Path","",ref.field)==ref&&
        operation_enabled_property(session.document(),ref),
        "Operation enabled is discoverable by its stable typed Ref");
    rejects("INVALID_OPERATION_REF",[&]{(void)operation_enabled_property(session.document(),
        {"enabled-path","point",ref.field});});
    rejects("MISSING_OPERATION",[&]{(void)operation_enabled_property(session.document(),
        operation_ref("enabled-path","missing","enabled"));});
    rejects("MISSING_REFERENCE",[&]{session.apply({Set{ref,0}},session.revision());});
    session.apply({EnableOperation{"enabled-path","enabled-fill",false}},session.revision());
    const auto shape=evaluate_shape(session.document(),"enabled-path",evaluate(session.document()));
    check(!operation_enabled_property(session.document(),ref)&&
        std::none_of(shape.paints.begin(),shape.paints.end(),[](const auto& paint){return paint.operation=="enabled-fill";}),
        "Existing enable command changes the typed value and bypasses the Fill consumer");
    session.undo(session.revision());
    check(operation_enabled_property(session.document(),ref),"Undo restores the authored operation enabled choice");
}
void fill_rule_link_contract() {
    Session session(empty_document("fill-doc","fill-comp","fill-art"));
    session.apply({CreatePrimitive{"fill-comp","","fill-source","Source",default_primitive("source-shape","nect.shape.rectangle")},
        CreatePrimitive{"fill-comp","","fill-target","Target",default_primitive("target-shape","nect.shape.rectangle")}},0);
    auto source_fill=default_operation("source-fill","nect.paint.fill");
    auto target_fill=default_operation("target-fill","nect.paint.fill");target_fill.fill_rule="nonzero";
    session.apply({AddOperation{"fill-source",source_fill,1},AddOperation{"fill-target",target_fill,1},
        OperationOptions{"fill-source","source-fill","below","evenodd"}},session.revision());
    const Ref target{"fill-target","","op.target-fill.fill_rule"},source{"fill-source","","op.source-fill.fill_rule"};
    const auto target_shape_rule=[&] {
        const auto shape=evaluate_shape(session.document(),"fill-target",evaluate(session.document()));
        const auto paint=std::find_if(shape.paints.begin(),shape.paints.end(),[](const auto& layer){return layer.operation=="target-fill";});
        if(paint==shape.paints.end())throw std::runtime_error("Target Fill layer is missing from shape evaluation");
        return paint->fill_rule;
    };
    check(resolve_name(session.document(),"Target","",target.field)==target&&
        fill_rule_property(session.document(),target).literal=="nonzero"&&
        target_shape_rule()=="nonzero",
        "Fill rule has a stable typed Ref and literal paint evaluation");
    rejects("MISSING_REFERENCE",[&]{session.apply({Set{target,1}},session.revision());});
    rejects("INVALID_DOMAIN",[&]{session.apply({LinkFillRule{target,operation_ref("fill-source","fill-source-stroke","fill_rule")}},session.revision());});
    rejects("INVALID_OPERATION_REF",[&]{session.apply({LinkFillRule{target,{"fill-source","point","op.source-fill.fill_rule"}}},session.revision());});
    rejects("TYPE_MISMATCH",[&]{session.apply({LinkFillRule{target,{"fill-source","","op.source-fill.composite"}}},session.revision());});
    const auto literal_bytes=encode(session.document());
    check(literal_bytes.find("\"version\":\"0.26\"")!=std::string::npos&&
        literal_bytes.find("fill_rule_driver")==std::string::npos&&encode(decode(literal_bytes))==literal_bytes,
        "Native 0.26 omits an absent Fill rule driver and roundtrips literal state");
    auto legacy_literal=literal_bytes;
    const auto current_version=legacy_literal.find("\"version\":\"0.26\"");
    check(current_version!=std::string::npos,"Native writer exposes 0.26 for the literal migration fixture");
    legacy_literal.replace(current_version,std::string("\"version\":\"0.26\"").size(),"\"version\":\"0.25\"");
    check(decode(legacy_literal)==session.document(),"Native 0.25 retains literal-only Fill rules");

    session.apply({LinkFillRule{target,source}},session.revision());
    auto state=fill_rule_property(session.document(),target);
    check(state.literal=="nonzero","Linked Fill retains its authored literal");
    check(state.driver==FillRuleDriver{source},"Linked Fill stores the exact same-field source Ref");
    check(state.evaluated=="evenodd","Linked Fill evaluator follows the even-odd source");
    check(target_shape_rule()=="evenodd",
        "Linked Fill shape evaluation uses the resolved enum");
    auto linked=encode(session.document());check(linked.find("\"fill_rule_driver\":{\"link\":")!=std::string::npos&&
        encode(decode(linked))==linked,"Native 0.26 retains the closed stable Fill rule Ref exactly");
    auto false_version=linked;const auto version= false_version.find("\"version\":\"0.26\"");
    false_version.replace(version,std::string("\"version\":\"0.26\"").size(),"\"version\":\"0.25\"");
    rejects("UNSUPPORTED_FILL_RULE_DRIVER",[&]{decode(false_version);});
    auto malformed=linked;const auto field=malformed.find("op.source-fill.fill_rule");
    check(field!=std::string::npos,"Native linked source Ref is present");
    malformed.replace(field,std::string("op.source-fill.fill_rule").size(),"text.direction");
    rejects("TYPE_MISMATCH",[&]{decode(malformed);});

    const auto before_bad_batch=encode(session.document());const auto revision=session.revision();
    rejects("DEPENDENCY_CYCLE",[&]{session.apply({OperationOptions{"fill-target","target-fill","above","nonzero"},
        LinkFillRule{target,target}},revision);});
    check(session.revision()==revision&&encode(session.document())==before_bad_batch,
        "An invalid second command leaves the first composite edit and all authored bytes uncommitted");
    rejects("DUPLICATE_TARGET",[&]{session.apply({LinkFillRule{target,source,false},UnlinkFillRule{target}},revision);});
    rejects("DRIVEN_PROPERTY",[&]{session.apply({LinkFillRule{target,source}},revision);});
    rejects("DRIVEN_PROPERTY",[&]{session.apply({OperationOptions{"fill-target","target-fill","above","evenodd"}},revision);});
    session.apply({OperationOptions{"fill-target","target-fill","above","nonzero"}},revision);
    check(session.document().objects.at("fill-target").stack.back().fill_rule_driver==FillRuleDriver{source},
        "An unrelated composite edit preserves the Fill rule driver");
    session.apply({OperationOptions{"fill-source","source-fill","below","nonzero"}},session.revision());
    check(fill_rule_property(session.document(),target).evaluated=="nonzero"&&
        target_shape_rule()=="nonzero",
        "Changing the source choice updates target paint evaluation");
    rejects("MISSING_OPERATION",[&]{session.apply({RemoveOperation{"fill-source","source-fill"}},session.revision());});
    const auto unlink_revision=session.revision();session.apply({UnlinkFillRule{target}},unlink_revision);
    check(fill_rule_property(session.document(),target).literal=="nonzero"&&
        !fill_rule_property(session.document(),target).driver,"Unlink freezes the evaluated choice into the target literal");
    session.undo(session.revision());
    check(fill_rule_property(session.document(),target).driver==FillRuleDriver{source},"Undo restores the Fill rule link");
    session.apply({OperationOptions{"fill-source","source-fill","below","evenodd"}},session.revision());
    check(fill_rule_property(session.document(),target).evaluated=="evenodd","Undo-restored link follows later source edits");

    Session duplicate(session.document());
    duplicate.apply({DuplicateObjects{{"fill-source","fill-target"},"fill-copy"}},0);
    const auto copied_source=std::find_if(duplicate.document().objects.begin(),duplicate.document().objects.end(),
        [](const auto& entry){return entry.second.name=="Source copy";});
    const auto copied_target=std::find_if(duplicate.document().objects.begin(),duplicate.document().objects.end(),
        [](const auto& entry){return entry.second.name=="Target copy";});
    check(copied_source!=duplicate.document().objects.end()&&copied_target!=duplicate.document().objects.end(),
        "Duplicating both Fill link endpoints creates stable copies");
    const auto copied_fill=std::find_if(copied_target->second.stack.begin(),copied_target->second.stack.end(),
        [](const auto& operation){return operation.type=="nect.paint.fill";});
    check(copied_fill!=copied_target->second.stack.end()&&copied_fill->fill_rule_driver&&
        copied_fill->fill_rule_driver->link==operation_ref(copied_source->first,
            std::find_if(copied_source->second.stack.begin(),copied_source->second.stack.end(),
                [](const auto& operation){return operation.type=="nect.paint.fill";})->id,"fill_rule")&&
        evaluate_fill_rule(duplicate.document(),operation_ref(copied_target->first,copied_fill->id,"fill_rule"))=="evenodd",
        "Duplicating source and target remaps only the copied Fill link to the copied source");
}
}
int main(){try{
    operation_enabled_read_contract();
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
    fill_rule_link_contract();
    std::cout<<"PASS "<<checks<<" ordered shape stack checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
