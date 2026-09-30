#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
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
void point_edit_enabled_links() {
    Session s(empty_document("edit-doc","edit-comp","edit-art"));
    auto source_circle=circle();source_circle.id="source-generator";
    s.apply({CreatePrimitive{"edit-comp","","target","Target",circle()},
        CreatePrimitive{"edit-comp","","source","Source",source_circle}},0);
    const Ref target_point{"target","circle-source-east","x"},source_point{"source","source-generator-east","x"};
    s.apply({Set{target_point,210},Set{source_point,310}},1);
    const auto target_ref=point_edit_enabled_ref("target","circle-source-point-edit");
    const auto source_ref=point_edit_enabled_ref("source","source-generator-point-edit");
    s.apply({EnablePointEdit{"target",false}},2);
    s.apply({LinkPointEditEnabled{target_ref,source_ref}},3);
    auto state=point_edit_enabled_state(s.document(),target_ref);
    check(state.literal==false&&state.driver==source_ref&&state.evaluated&&
        evaluate(s.document()).at(target_point)==210&&property_origin(s.document(),target_point)=="point_edit",
        "A false-literal target follows the exact source while retaining its authored override");
    const auto legacy_ref=Ref{"target","","point_edit.enabled"};
    check(point_edit_enabled_property(s.document(),legacy_ref)==false,
        "Legacy Point Edit slot read remains the authored literal");
    const auto legacy=request(s,R"({"op":"get","ref":{"object":"target","point":"","field":"point_edit.enabled"}})");
    const auto qualified=request(s,R"({"op":"get","ref":{"object":"target","point":"","field":"point_edit.circle-source-point-edit.enabled"}})");
    const auto property_list=request(s,R"({"op":"properties"})");
    check(legacy.find("\"link\":false")!=std::string::npos&&legacy.find("\"literal\":false")!=std::string::npos&&
        qualified.find("\"link\":true")!=std::string::npos&&qualified.find("\"evaluated\":true")!=std::string::npos&&
        qualified.find("\"object\":\"source\"")!=std::string::npos&&
        property_list.find("point_edit.circle-source-point-edit.enabled")!=std::string::npos,
        "Legacy and instance-qualified API reads expose their distinct authored and evaluated states");
    check(resolve_name(s.document(),"Target","","point_edit.circle-source-point-edit.enabled")==target_ref,
        "Unique-name resolution finds the exact retained correction Ref");
    const auto linked_bytes=encode(s.document());
    check(linked_bytes.find("\"version\":\"0.65\"")!=std::string::npos&&
        linked_bytes.find("\"enabled_driver\":{\"link\":{\"object\":\"source\",\"point\":\"\",\"field\":\"point_edit.source-generator-point-edit.enabled\"}}")!=std::string::npos&&
        encode(decode(linked_bytes))==linked_bytes,
        "Native 0.43 retains the optional closed driver and roundtrips without byte drift");
    auto false_version=test_support::without_empty_presets_for_legacy_fixture(linked_bytes);
    const auto version_at=false_version.find("\"version\":\"0.65\"");
    false_version.replace(version_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.31\"");
    rejects("UNSUPPORTED_POINT_EDIT_ENABLED_DRIVER",[&]{(void)decode(false_version);});
    auto malformed=linked_bytes;
    const auto ref_at=malformed.find("point_edit.source-generator-point-edit.enabled");
    malformed.replace(ref_at,std::string("point_edit.source-generator-point-edit.enabled").size(),"point_edit.enabled");
    rejects("INVALID_POINT_EDIT_REF",[&]{(void)decode(malformed);});

    const auto prior_document=s.document();const auto prior_history=s.history();const auto prior_revision=s.revision();
    rejects("DRIVEN_PROPERTY",[&]{s.apply({EnablePointEdit{"target",true}},prior_revision);});
    rejects("DRIVEN_PROPERTY",[&]{s.apply({LinkPointEditEnabled{target_ref,source_ref}},prior_revision);});
    rejects("REVISION_CONFLICT",[&]{s.apply({UnlinkPointEditEnabled{target_ref}},prior_revision-1);});
    rejects("DRIVEN_PROPERTY",[&]{s.apply({Set{target_point,220}},prior_revision);});
    check(s.document()==prior_document&&s.history()==prior_history&&s.revision()==prior_revision,
        "Driven toggles, implicit replacement, stale revisions and false-literal edits leave state and history unchanged");
    rejects("POINT_EDIT_IN_USE",[&]{s.apply({ClearPointEdit{"source"}},prior_revision);});
    rejects("POINT_EDIT_IN_USE",[&]{s.apply({ClearPointEdit{"source"},Set{source_point,320}},prior_revision);});
    rejects("POINT_EDIT_IN_USE",[&]{s.apply({ConvertToPath{"source"}},prior_revision);});
    rejects("POINT_EDIT_IN_USE",[&]{s.apply({DeleteObjects{{"source"}}},prior_revision);});
    check(s.document()==prior_document&&s.history()==prior_history&&s.revision()==prior_revision,
        "Clear, same-batch clear/recreate, conversion and source deletion stop at the destructive command boundary");
    rejects("MISSING_POINT_EDIT",[&]{(void)point_edit_enabled_state(s.document(),
        Ref{"target","","point_edit.recreated-point-edit.enabled"});});
    rejects("INVALID_POINT_EDIT_REF",[&]{(void)point_edit_enabled_state(s.document(),
        Ref{"target","circle-source-east","point_edit.circle-source-point-edit.enabled"});});
    rejects("USE_TYPED_COMMAND",[&]{s.apply({Set{target_ref,1}},prior_revision);});

    const auto linked_before_same_id_edit=s.document();const auto same_id_revision=s.revision();
    s.apply({Set{{"source","","generator.radius"},60},Rename{"source","Renamed source"},
        ReorderObjects{"edit-comp","",{"source","target"}}},same_id_revision);
    check(point_edit_enabled_state(s.document(),target_ref).driver==source_ref&&
        point_edit_enabled_state(s.document(),target_ref).evaluated&&
        s.document().objects.at("source").point_edit->id=="source-generator-point-edit",
        "Same-ID source edits, rename and reorder preserve the qualified driver");
    s.undo(s.revision());
    check(s.document()==linked_before_same_id_edit&&point_edit_enabled_state(s.document(),target_ref).driver==source_ref,
        "Undo restores same-ID source edits without retargeting the Point Edit link");
    s.redo(s.revision());
    check(point_edit_enabled_state(s.document(),target_ref).driver==source_ref,
        "Redo preserves the exact qualified Point Edit source");

    const auto same_id_redone_revision=s.revision();
    s.apply({EnablePointEdit{"source",false}},same_id_redone_revision);
    check(!point_edit_enabled_state(s.document(),target_ref).evaluated&&
        evaluate(s.document()).at(target_point)==150&&
        s.document().objects.at("target").point_edit->overrides.at(target_point.point).at("x").literal==210,
        "False source bypasses to generator fallback without deleting target overrides");
    s.apply({EnablePointEdit{"source",true}},same_id_redone_revision+1);
    check(point_edit_enabled_state(s.document(),target_ref).evaluated&&evaluate(s.document()).at(target_point)==210,
        "Restoring the source reactivates the same target override and generated point ID");
    s.apply({UnlinkPointEditEnabled{target_ref}},same_id_redone_revision+2);
    const auto frozen=point_edit_enabled_state(s.document(),target_ref);
    check(!frozen.driver&&frozen.literal&&frozen.evaluated,
        "Unlink freezes the evaluated authored bypass bit");
    s.undo(s.revision());
    check(s.document().objects.at("target").point_edit->enabled_driver==source_ref&&
        point_edit_enabled_state(s.document(),target_ref).evaluated,
        "Undo restores the exact driver and evaluated correction state");
    s.redo(s.revision());
    check(!s.document().objects.at("target").point_edit->enabled_driver,
        "Redo restores the frozen literal after unlink");

    auto cyclic=s.document();
    cyclic.objects.at("target").point_edit->enabled_driver=target_ref;
    rejects("DEPENDENCY_CYCLE",[&]{validate(cyclic);});
    cyclic=s.document();cyclic.objects.at("target").point_edit->enabled_driver=source_ref;
    cyclic.objects.at("source").point_edit->enabled_driver=target_ref;
    rejects("DEPENDENCY_CYCLE",[&]{validate(cyclic);});
    auto foreign=s.document();
    Object foreign_object;foreign_object.id="foreign";foreign_object.name="Foreign";
    foreign_object.source=circle();foreign_object.source->id="foreign-source";
    foreign_object.point_edit=PointEdit{"foreign-source-point-edit",1,true,{{"foreign-source-east",{{"x",{1,{}}}}}}};
    foreign.objects.emplace(foreign_object.id,foreign_object);
    foreign.compositions.push_back(Composition{"foreign-comp","Foreign",{"foreign"},{},{}});
    foreign.objects.at("target").point_edit->enabled_driver=point_edit_enabled_ref("foreign","foreign-source-point-edit");
    rejects("CROSS_COMPOSITION",[&]{validate(foreign);});
    auto deep=empty_document("deep-doc","deep-comp","deep-art");
    constexpr int chain=130;
    for(int i=0;i<chain;++i) {
        const auto id="node"+std::to_string(i),source_id="generator"+std::to_string(i);
        Object object;object.id=id;object.name=id;object.source=circle();object.source->id=source_id;
        object.point_edit=PointEdit{source_id+"-point-edit",1,true,{}};
        deep.objects.emplace(id,std::move(object));deep.compositions.front().roots.push_back(id);
    }
    for(int i=0;i<chain-1;++i)deep.objects.at("node"+std::to_string(i)).point_edit->enabled_driver=
        point_edit_enabled_ref("node"+std::to_string(i+1),"generator"+std::to_string(i+1)+"-point-edit");
    rejects("DEPENDENCY_DEPTH",[&]{validate(deep);});
}
}
int main() {
    try {
        Session s(empty_document("doc","comp","art"));
        s.apply({CreatePrimitive{"comp","","circle","Circle",circle()},
            CreatePrimitive{"comp","","rect","Rectangle",{"rect-source","nect.shape.rectangle",1,
                {{"center_x",{300,{}}},{"center_y",{200,{}}},{"width",{80,{}}},{"height",{120,{}}}}}}},0);
        const Ref east{"circle","circle-source-east","x"},west{"circle","circle-source-west","x"};
        const Ref radius{"circle","","generator.radius"},rx{"rect","rect-source-top-left","x"};
        const Ref edit_enabled{"circle","","point_edit.enabled"};
        rejects("NO_POINT_EDIT",[&]{(void)point_edit_enabled_property(s.document(),edit_enabled);});
        rejects("NO_POINT_EDIT",[&]{(void)resolve_name(s.document(),"Circle","","point_edit.enabled");});
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
        const auto discovered=properties(s.document());
        check(point_edit_enabled_property(s.document(),edit_enabled)&&
            std::find(discovered.begin(),discovered.end(),edit_enabled)!=discovered.end()&&
            resolve_name(s.document(),"Circle","","point_edit.enabled")==edit_enabled,
            "Authored Point Edit is discoverable and resolves by its unique owner name");
        const auto enabled_read=request(s,R"({"op":"get","ref":{"object":"circle","point":"","field":"point_edit.enabled"}})");
        check(enabled_read.find("\"type\":\"bool\"")!=std::string::npos&&
            enabled_read.find("\"literal\":true")!=std::string::npos&&
            enabled_read.find("\"driver\":null")!=std::string::npos&&
            enabled_read.find("\"evaluated\":true")!=std::string::npos,
            "Point Edit enabled typed read exposes the authored bypass choice");
        rejects("MISSING_REFERENCE",[&]{s.apply({Set{edit_enabled,0}},2);});
        rejects("INVALID_POINT_EDIT_REF",[&]{(void)point_edit_enabled_property(s.document(),Ref{"circle","circle-source-east","point_edit.enabled"});});
        s.apply({Set{radius,80},Link{rx,{east,1,20,"copy_local_value"}}},2);
        values=evaluate(s.document());
        check(values.at(east)==180&&values.at(west)==20&&values.at(rx)==200,"Radius changes preserve overrides and driven stable references");
        s.apply({EnablePointEdit{"circle",false}},3);
        check(property_origin(s.document(),east)=="bypassed_point_edit"&&
            !point_edit_enabled_property(s.document(),edit_enabled),"Disabled correction is retained and explicit");
        s.apply({Set{radius,90}},4);
        check(evaluate(s.document()).at(east)==190&&evaluate(s.document()).at(rx)==210,"Bypass restores generated output and dependent evaluation");
        const auto bypassed=encode(s.document());
        check(encode(decode(bypassed))==bypassed,"Disabled corrections persist exactly");
        s.apply({EnablePointEdit{"circle",true},Rename{"circle","Renamed"},
            ReorderObjects{"comp","",{"rect","circle"}}},5);
        check(evaluate(s.document()).at(rx)==200,"Rename/reorder/bypass retain point identity");
        s.undo(6);check(encode(s.document())==bypassed&&!point_edit_enabled_property(s.document(),edit_enabled),
            "Undo restores source, typed correction bypass and hierarchy");s.redo(7);
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
        point_edit_enabled_links();
        std::cout<<"PASS "<<checks<<" primitive, correction and conversion checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
