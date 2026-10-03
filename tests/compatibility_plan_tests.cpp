#include "nect/io.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Independent API acceptance for REQ-71/72/74. Expected closure IDs and bounds
// are fixture literals, not results obtained from the resolver/evaluated scene.
// This tests planning and the unchanged SVG admission seam, never raster pixels,
// Illustrator/PDF encoder conformance, or a portable SVG font backend.
using namespace nect;
namespace j=boost::json;
namespace {
int checks=0;
constexpr std::string_view svg_profile="svg/1.1+css-compositing";

void check(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
    ++checks;
}
std::string string(const j::value& value) { return std::string(value.as_string()); }
double number(const j::value& value) { return j::value_to<double>(value); }
void near(const j::value& actual,double expected,const std::string& message) {
    check(std::abs(number(actual)-expected)<1e-9,message+": expected "+std::to_string(expected)+
        ", got "+j::serialize(actual));
}
std::set<Id> id_set(const j::value& value) {
    std::set<Id> result;
    for(const auto& id:value.as_array())
        check(result.insert(string(id)).second,"Dependency/member IDs are unique");
    return result;
}
j::object plan_request(std::string_view profile=svg_profile,bool explicit_artboard=true) {
    j::object input{{"op","compatibility_plan"},{"composition","comp"},{"target_profile",j::value(profile)}};
    if(explicit_artboard)input["artboard"]="board";
    return input;
}
j::object legacy_request() { return {{"op","export_plan"},{"composition","comp"},{"artboard","board"}}; }

// Every request, including typed failures, is checked against the full authored
// Document/native bytes/revision/History, plus the undo/redo/gesture boundaries.
j::object call(Session& session,const j::object& input) {
    const auto document=session.document();
    const auto native=encode(document);
    const auto revision=session.revision();
    const auto history=session.history();
    const auto can_undo=session.can_undo(),can_redo=session.can_redo(),gesture=session.gesture_active();
    auto response=j::parse(request(session,j::serialize(input))).as_object();
    check(session.document()==document&&encode(session.document())==native&&session.revision()==revision&&
        session.history()==history&&session.can_undo()==can_undo&&session.can_redo()==can_redo&&
        session.gesture_active()==gesture,"Planning preserves Document, native bytes, revision and all History boundaries");
    check(j::value_to<std::uint64_t>(response.at("revision"))==revision,"Response identifies the unchanged Session revision");
    return response;
}
j::object result(Session& session,const j::object& input) {
    auto response=call(session,input);
    check(response.at("ok").as_bool(),"Read succeeds: "+j::serialize(response));
    return response.at("result").as_object();
}
void error(Session& session,j::object input,std::string_view expected) {
    const auto response=call(session,input);
    check(!response.at("ok").as_bool(),"Invalid request is rejected");
    const auto& detail=response.at("error").as_object();
    check(string(detail.at("code"))==expected,"Expected "+std::string(expected)+", got "+j::serialize(detail));
    check(!string(detail.at("message")).empty(),"Typed failure has a useful explanation");
}
template<class F>void rejects(std::string_view expected,F action) {
    try { action(); }
    catch(const Error& e) {
        check(e.code==expected,"Expected "+std::string(expected)+", got "+e.code+": "+e.what());
        return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(expected));
}

Object rectangle(const Id& id,double x,double y,double width,double height,bool stroke=false) {
    Object object;object.id=id;object.name="Repeated display label";
    Contour contour;contour.id=id+"-contour";contour.closed=true;
    const std::array<Vec2,4> corners{{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}};
    for(const auto& corner:corners) {
        Point point;point.id=id+"-p"+std::to_string(contour.points.size());
        point.x.literal=corner.x;point.y.literal=corner.y;contour.points.push_back(point);
    }
    object.contours.push_back(contour);
    object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
    if(stroke) {
        auto paint=default_operation(id+"-stroke","nect.paint.stroke");
        paint.parameters.at("width").literal=2;object.stack.push_back(paint);
    }
    return object;
}
Object group(const Id& id,std::vector<Id> children) {
    Object object;object.id=id;object.name="Repeated display label";object.kind=Kind::group;
    object.children=std::move(children);return object;
}
void matrix(Object& object,const Affine& transform) {
    for(std::size_t i=0;i<transform.size();++i)object.transform[i].literal=transform[i];
}
Document vector_fixture() {
    auto document=empty_document("compatibility-source","comp","board");
    document.objects.emplace("vector",rectangle("vector",2,3,30,20,true));
    document.objects.emplace("sibling",rectangle("sibling",70,5,12,15));
    document.objects.emplace("container",group("container",{"vector","sibling"}));
    document.compositions.front().roots={"container"};return document;
}
Document closure_fixture(bool posterize=true,bool appearance_mask=true) {
    auto document=empty_document("compatibility-closure","comp","board");
    document.objects.emplace("fill-a",rectangle("fill-a",1.25,2.25,10.5,6.5));
    document.objects.emplace("fill-b",rectangle("fill-b",20.25,3.75,5,8));
    auto effect=group("effect-group",{"fill-a","fill-b"});
    matrix(effect,{1,0,0,1,3,4});effect.transform_parent="transform-driver";
    effect.compositing.opacity.literal=.625;effect.compositing.isolated=true;
    if(posterize) {
        auto operation=default_operation("posterize-effect","nect.group.posterize");
        operation.parameters.at("levels").literal=4;effect.stack.push_back(operation);
    }
    if(appearance_mask) {
        GeometryMask mask;mask.id="effect-mask";mask.source="mask-source";mask.mode="alpha";
        effect.compositing.mask=mask;
    }
    document.objects.emplace(effect.id,effect);
    // Both the unrelated stroke sibling and the effect have the same structural
    // parent. The explicit transform parent is a borrowed editable root, not a
    // rendered member of the derivative. The mask has its own child closure.
    document.objects.emplace("sibling",rectangle("sibling",200,150,40,30,true));
    document.objects.emplace("container",group("container",{"effect-group","sibling"}));
    auto driver=rectangle("transform-driver",300,250,15,10);
    matrix(driver,{1,0,0,1,10,20});document.objects.emplace(driver.id,driver);
    document.objects.emplace("mask-leaf",rectangle("mask-leaf",0,0,80,80));
    document.objects.emplace("mask-source",group("mask-source",{"mask-leaf"}));
    document.compositions.front().roots={"container","mask-source","transform-driver"};return document;
}
Object external_guide() {
    Object guide;guide.id="external-guide";guide.name="External geometry input";guide.visible=false;
    Point first;first.id="external-guide-start";
    Point last;last.id="external-guide-end";last.x.literal=2000;
    guide.contours={{"external-guide-contour",false,{first,last}}};return guide;
}
Object point_edit_circle(const Id& id) {
    Object object;object.id=id;object.name="Point Edit dependency fixture";
    object.source=default_primitive(id+"-generator","nect.shape.circle");
    object.source->parameters.at("radius").literal=5;
    object.point_edit=PointEdit{id+"-generator-point-edit",1,true,{}};
    object.point_edit->overrides[id+"-generator-east"]["x"].literal=7;
    object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));return object;
}
const j::object& item(const j::object& plan,const Id& source,const Id& point,const std::string& field) {
    const j::object* found=nullptr;
    for(const auto& value:plan.at("items").as_array()) {
        const auto& candidate=value.as_object();const auto& ref=candidate.at("source_ref").as_object();
        if(string(ref.at("object"))==source&&string(ref.at("point"))==point&&string(ref.at("field"))==field) {
            check(found==nullptr,"A source Ref has only one occurrence in this non-Instance fixture");found=&candidate;
        }
    }
    check(found!=nullptr,"Plan identifies exact stable source Ref: "+source+"/"+point+"/"+field);
    return *found;
}
std::set<std::string> provenance(const j::object& plan) {
    std::set<std::string> refs;
    for(const auto& value:plan.at("items").as_array())refs.insert(j::serialize(value.as_object().at("source_ref")));
    return refs;
}
bool list_has_object(const j::value& list,const Id& id) {
    return std::any_of(list.as_array().begin(),list.as_array().end(),[&](const auto& value){
        return string(value.as_object().at("object"))==id;
    });
}
void report_contract(const j::object& plan,Session& session,std::string_view profile) {
    check(number(plan.at("behavior_version"))==1&&!string(plan.at("plan_id")).empty(),"Versioned plan has a nonempty derived identity");
    check(string(plan.at("source_document"))==session.document().id&&
        j::value_to<std::uint64_t>(plan.at("source_revision"))==session.revision(),"Plan provenance names the committed native source");
    check(string(plan.at("target_profile"))==profile&&string(plan.at("composition"))=="comp"&&
        string(plan.at("artboard"))=="board","Plan identifies the requested target and export frame");
    check(plan.at("native_source_preserved").as_bool()&&!plan.at("derivative_generated").as_bool(),
        "Analysis preserves native state and claims no generated derivative");
    check(plan.at("warnings").is_array()&&plan.at("ir_nodes").is_array()&&plan.at("bake_groups").is_array()&&
        !string(plan.at("expected_validator")).empty(),"Structured IR, warnings, bake list and external validator are explicit");
    const std::map<std::string,std::string> lists{{"editable_native","preserved"},{"expandable","expanded"},
        {"rasterize_required","rasterized"},{"unsupported","unsupported"}};
    std::map<std::string,std::size_t> observed;
    std::size_t losses=0;
    for(const auto& value:plan.at("items").as_array()) {
        const auto& record=value.as_object();const auto classification=string(record.at("classification"));
        check(lists.contains(classification),"Classification is one of the four accepted contract literals");
        const auto& ref=record.at("source_ref").as_object();
        check(!string(record.at("source_id")).empty()&&string(record.at("source_id"))==string(ref.at("object"))&&
            !string(record.at("occurrence_id")).empty()&&!string(record.at("semantic_type")).empty()&&
            number(record.at("semantic_version"))>=1&&!string(record.at("reason_code")).empty(),
            "Every item has stable provenance, semantic type/version and a reason");
        check(record.at("dependencies").is_array()&&record.at("native_source_preserved").as_bool(),
            "Each item declares dependency IDs and native-source preservation");
        ++observed[classification];
        if(classification=="expandable"||classification=="rasterize_required")++losses;
    }
    for(const auto& [classification,list]:lists) {
        check(j::value_to<std::size_t>(plan.at("counts").as_object().at(classification))==observed[classification]&&
            plan.at(list).as_array().size()==observed[classification],"Counts and consequence lists account for every "+classification+" item");
    }
    check(plan.at("editability_losses").as_array().size()==losses,"Every actual expansion/planned raster replacement has an editability loss");
}
void legacy_parity(Session& session,const j::object& plan,bool supported) {
    const auto legacy=result(session,legacy_request());
    check(plan.at("legacy_export_plan")==j::value(legacy),"Common plan preserves the entire existing export_plan result exactly");
    check(plan.at("export_supported").as_bool()==supported&&legacy.at("svg_export_supported").as_bool()==supported,
        "Planning cannot broaden the independently expected SVG encoder admission");
}

void deterministic_read_and_legacy_contract() {
    Session session(vector_fixture());
    const auto first=result(session,plan_request());report_contract(first,session,svg_profile);
    check(first.at("bake_groups").as_array().empty()&&first.at("rasterized").as_array().empty()&&
        first.at("unsupported").as_array().empty()&&first.at("expanded").as_array().empty(),
        "Ordinary vector paths, fills, strokes and grouping need no bake or lowering");
    check(first.at("items").as_array().size()==6,"Two Paths, two Fills, one Stroke and one Group are individually accounted for");
    check(string(item(first,"vector","vector-stroke","operation").at("classification"))=="editable_native", "Stroke remains target-editable outside a bake");
    legacy_parity(session,first,true);
    check(export_svg(session.document(),"comp","board").find("<svg")!=std::string::npos,"Existing all-vector SVG export remains admitted");
    check(first==result(session,plan_request()),"Repeated same source/revision/profile/options yields the entire same semantic result");
    check(first==result(session,plan_request(svg_profile,false)),"Single-Artboard omission canonicalizes to the explicit Artboard");
    auto explicit_defaults=plan_request();explicit_defaults["options"]=j::object{{"raster_scale",1}};
    check(first==result(session,explicit_defaults),"Default scale 1 and explicit integer 1 are canonical equivalents");
    explicit_defaults["options"]=j::object{{"raster_scale",1.0}};
    check(first==result(session,explicit_defaults),"Default and explicit floating scale 1 are canonical equivalents");
    Session identical(vector_fixture());check(first==result(identical,plan_request()),"Independent equal Sessions receive the same semantic plan");
    auto different=vector_fixture();different.objects.at("vector").contours.front().points.front().x.literal=2.5;
    Session changed_content(different);const auto distinct=result(changed_content,plan_request());
    check(first.at("source_document")==distinct.at("source_document")&&first.at("source_revision")==distinct.at("source_revision")&&
        first.at("plan_id")!=distinct.at("plan_id"),"Different content with the same native ID and revision cannot collide");
    auto scaled_input=plan_request();scaled_input["options"]=j::object{{"raster_scale",2}};
    check(first.at("plan_id")!=result(session,scaled_input).at("plan_id"),"Canonical raster scale participates in identity");

    session.apply({Rename{"vector","Changed label"}},session.revision());
    const auto renamed=result(session,plan_request());
    check(provenance(first)==provenance(renamed),"Changing non-unique display labels never retargets source Refs");
    session.undo(session.revision());check(session.document()==vector_fixture()&&session.can_redo(),"Fixture restores identical native content with a retained redo branch");
    const auto restored=result(session,plan_request());
    check(first.at("plan_id")!=restored.at("plan_id")&&first.at("source_document")==restored.at("source_document"),
        "Identical native content at a new committed revision has a distinct plan identity");
    check(restored==result(session,plan_request())&&session.can_redo(),"Repeated reads preserve existing undo/redo History");
    session.redo(session.revision());check(session.document().objects.at("vector").name=="Changed label","Planning did not consume or truncate the redo branch");

    const auto committed=result(session,plan_request());
    session.begin_gesture(session.revision());
    session.update_gesture({Set{{"vector","vector-p0","x"},99}});
    check(committed==result(session,plan_request()),"Read-only planning consumes committed source rather than an uncommitted gesture preview");
    check(session.gesture_active(),"Planning leaves the interaction gesture open");session.cancel_gesture();
}

void assert_local_closure(Session& session,const j::object& plan,bool has_posterize,bool has_mask,double scale) {
    report_contract(plan,session,svg_profile);legacy_parity(session,plan,false);
    check(plan.at("bake_groups").as_array().size()==1,"One isolated effect creates exactly one local planned derivative");
    const auto& bake=plan.at("bake_groups").as_array().front().as_object();
    const std::set<Id> members{"effect-group","fill-a","fill-b"};
    std::set<Id> borrowed{"transform-driver"};if(has_mask){borrowed.insert("mask-source");borrowed.insert("mask-leaf");}
    check(id_set(bake.at("members"))==members&&id_set(bake.at("occurrence_members"))==members,
        "Only the affected Group and its two required children are replaced");
    check(id_set(bake.at("borrowed_dependencies"))==borrowed,"Exact borrowed mask subtree and external Transform Parent are sampled");
    check(id_set(bake.at("losses"))==members,"Borrowed editable sources are excluded from derivative editability losses");
    check(string(bake.at("root"))=="effect-group"&&string(bake.at("occurrence_root"))=="effect-group"&&
        !string(bake.at("id")).empty(),"Derivative root retains stable source/occurrence identity");
    near(bake.at("scale"),scale,"Declared raster scale");near(bake.at("padding"),0,"Posterize/mask need no filter padding");
    near(bake.at("filter_radius"),0,"Posterize/mask need no filter radius");
    check(string(bake.at("color_space"))=="encoded-srgb"&&string(bake.at("alpha"))=="premultiplied"&&
        number(bake.at("channel_bits"))==8&&number(bake.at("alpha_bits"))==8&&
        string(bake.at("resolution_unit"))=="pixels_per_document_unit"&&string(bake.at("render_context"))=="time-independent",
        "Bake freezes encoded-sRGB premultiplied RGBA8 and resolution/render context");
    check(!bake.at("execution_available").as_bool(),"A planned derivative does not imply implemented raster export execution");
    // External translation (10,20) + Group translation (3,4) maps the two
    // rectangles to [14.25,26.25..24.75,32.75] and [33.25,27.75..38.25,35.75].
    // The hand-derived union is outward-aligned to the declared raster grid.
    const auto& bounds=bake.at("bounds").as_object();
    if(scale==1) {near(bounds.at("x"),14,"1x left");near(bounds.at("y"),26,"1x top");near(bounds.at("width"),25,"1x width");near(bounds.at("height"),10,"1x height");}
    else if(scale==2) {near(bounds.at("x"),14,"2x left");near(bounds.at("y"),26,"2x top");near(bounds.at("width"),24.5,"2x width");near(bounds.at("height"),10,"2x height");}
    else throw std::runtime_error("Unexpected test scale");
    for(const auto& value:plan.at("items").as_array()) {
        const auto& record=value.as_object();const auto id=string(record.at("source_id"));
        if(members.contains(id)) {
            check(string(record.at("classification"))=="rasterize_required"&&record.at("bake_id")==bake.at("id"),
                "Each replaced source semantic item points to the exact local bake");
            check(id_set(record.at("dependencies"))==borrowed,"Replaced source reports the exact sampled external dependencies");
        } else check(string(record.at("classification"))=="editable_native","Unrelated or borrowed vector structure remains target-editable");
    }
    for(const auto& loss:plan.at("editability_losses").as_array())
        check(members.contains(string(loss.as_object().at("source_ref").as_object().at("object"))),
            "Report never marks a borrowed source, unrelated sibling or whole-board container as lost");
    check(list_has_object(plan.at("preserved"),"sibling")&&list_has_object(plan.at("preserved"),"container")&&
        list_has_object(plan.at("preserved"),"transform-driver"),"Unrelated sibling/container and external Transform Parent stay preserved");
    if(has_mask)check(list_has_object(plan.at("preserved"),"mask-source")&&list_has_object(plan.at("preserved"),"mask-leaf"),
        "Borrowed mask Group and leaf retain editability in their original location");
    if(has_posterize) {
        const auto& effect=item(plan,"effect-group","posterize-effect","operation");
        check(string(effect.at("semantic_type"))=="nect.group.posterize"&&number(effect.at("semantic_version"))==1,
            "Exact authored Posterize operation/version is identified");
    }
    if(has_mask)check(string(item(plan,"effect-group","effect-mask","mask").at("semantic_type"))=="alpha_mask",
        "Exact authored appearance-mask relation is identified");
    const auto group_it=std::find_if(plan.at("ir_nodes").as_array().begin(),plan.at("ir_nodes").as_array().end(),[](const auto& value){
        const auto& node=value.as_object();return node.at("kind")=="group"&&node.at("source_id")=="effect-group";
    });
    check(group_it!=plan.at("ir_nodes").as_array().end(),"IR retains the isolated Group semantic record");
    const auto& group_ir=*group_it;
    near(group_ir.as_object().at("opacity"),.625,"IR preserves local opacity input");
    check(group_ir.as_object().at("isolation").as_bool()&&group_ir.as_object().at("blend")=="normal",
        "Local bake is explicitly isolated and normally composited");
    check(std::any_of(plan.at("ir_nodes").as_array().begin(),plan.at("ir_nodes").as_array().end(),[&](const auto& value){
        const auto& node=value.as_object();return node.at("kind")=="planned_raster_derivative"&&
            node.at("source_id")=="effect-group"&&node.at("bake_id")==bake.at("id")&&node.at("bounds")==bake.at("bounds");
    }),"Derived IR has one provenance-linked planned raster node with the declared bounds");
    check(plan.at("unsupported").as_array().empty(),"Qualified local closure needs no unsupported fallback");
}
void isolated_dependency_closure_contract() {
    Session combined(closure_fixture());const auto first=result(combined,plan_request());assert_local_closure(combined,first,true,true,1);
    check(first==result(combined,plan_request()),"Closure, derivative ID and report are deterministic");
    // A planned bake must not silently enable the existing SVG encoder.
    error(combined,{{"op","export_svg"},{"composition","comp"},{"artboard","board"}},"UNSUPPORTED_SVG_ALPHA_MASK");
    auto scaled_request=plan_request();scaled_request["options"]=j::object{{"raster_scale",2}};
    const auto scaled=result(combined,scaled_request);assert_local_closure(combined,scaled,true,true,2);
    check(first.at("plan_id")!=scaled.at("plan_id")&&first.at("bake_groups").as_array().front().as_object().at("id")!=
        scaled.at("bake_groups").as_array().front().as_object().at("id"),"Scale changes both plan and derivative identities");
    Session posterize_only(closure_fixture(true,false));const auto poster=result(posterize_only,plan_request());
    assert_local_closure(posterize_only,poster,true,false,1);
    rejects("UNSUPPORTED_SVG_EFFECT",[&]{(void)export_svg(posterize_only.document(),"comp","board");});
    Session mask_only(closure_fixture(false,true));assert_local_closure(mask_only,result(mask_only,plan_request()),false,true,1);
}

void assert_scoped_refusal(Document document,const std::string& reason,bool only_editable_inputs=true) {
    Session session(std::move(document));const auto plan=result(session,plan_request());report_contract(plan,session,svg_profile);
    legacy_parity(session,plan,false);
    const auto& effect=item(plan,"effect-group","posterize-effect","operation");
    check(string(effect.at("classification"))=="unsupported"&&string(effect.at("reason_code"))==reason,
        "Unqualified closure reports the exact effect and bounded refusal: "+reason);
    check(plan.at("bake_groups").as_array().empty()&&plan.at("rasterized").as_array().empty(),
        "No safe closure means explicit refusal, never a silent whole-Artboard bake");
    check(string(item(plan,"sibling","","object").at("classification"))=="editable_native"&&
        list_has_object(plan.at("preserved"),"sibling"),"Scoped refusal preserves unrelated editable sibling structure");
    if(only_editable_inputs)check(plan.at("editability_losses").as_array().empty(),
        "Refused planning does not report unperformed raster replacement as editability loss");
    for(const auto& loss:plan.at("editability_losses").as_array())check(
        string(loss.as_object().at("loss"))!="target_editability_replaced_by_planned_raster",
        "A refused bake never reports an unperformed raster replacement, even when independent procedural lowering is valid");
}
void geometry_and_typed_dependency_regressions() {
    for(const std::string mode:{"rigid","deform"}) {
        auto document=closure_fixture(true,false);const auto guide=external_guide();
        document.objects.emplace(guide.id,guide);document.compositions.front().roots.push_back(guide.id);
        GroupPathFollow relation;relation.id="external-follow";relation.path=guide.id;
        relation.contour="external-guide-contour";relation.mode=mode;
        relation.items={{"fill-a",{0,0,true}},{"fill-b",{30,0,true}}};
        document.objects.at("effect-group").transform_parent.reset();
        document.objects.at("effect-group").path_follow=relation;
        assert_scoped_refusal(document,"UNQUALIFIED_GEOMETRY_DEPENDENCY",false);
    }
    for(const bool expression:{false,true}) {
        auto document=closure_fixture(true,false);
        document.objects.at("fill-a")=point_edit_circle("fill-a");
        auto driver=point_edit_circle("point-driver");driver.visible=false;
        document.objects.emplace(driver.id,driver);document.compositions.front().roots.push_back(driver.id);
        auto& point_edit=*document.objects.at("fill-a").point_edit;
        if(expression)point_edit.enabled_expression=Expression{
            "ref(\"point-driver\",\"\",\"point_edit.point-driver-generator-point-edit.enabled\")",1};
        else point_edit.enabled_driver=point_edit_enabled_ref("point-driver","point-driver-generator-point-edit");
        assert_scoped_refusal(document,"DRIVEN_BAKE_CLOSURE_UNQUALIFIED",false);
    }
}
void zero_ink_leaf_closure_regression() {
    auto document=closure_fixture(true,false);
    auto unpainted=rectangle("unpainted-huge",-50000,-50000,100000,100000);unpainted.stack.clear();
    auto transparent=rectangle("transparent-huge",-40000,-40000,80000,80000,true);
    for(auto& paint:transparent.stack)paint.parameters.at("a").literal=0;
    document.objects.emplace(unpainted.id,unpainted);document.objects.emplace(transparent.id,transparent);
    auto& children=document.objects.at("effect-group").children;
    children.push_back(unpainted.id);children.push_back(transparent.id);
    check(unpainted.visible&&transparent.visible&&unpainted.compositing.opacity.literal==1&&
        transparent.compositing.opacity.literal==1,"Zero-ink fixture leaves are visible at full Object opacity");
    Session session(document);const auto plan=result(session,plan_request());
    // Hand-computed closure and bounds are unchanged despite huge noncontributing
    // geometry. In particular, that geometry cannot force BAKE_RESOURCE_LIMIT.
    assert_local_closure(session,plan,true,false,1);
    for(const auto& id:{unpainted.id,transparent.id}) {
        check(string(item(plan,id,"","object").at("classification"))=="editable_native"&&
            list_has_object(plan.at("preserved"),id),"Visible zero-ink leaf remains an editable vector outside the raster replacement");
        check(!list_has_object(plan.at("rasterized"),id),"Visible zero-ink leaf contributes neither bake members nor rasterized semantic items");
    }
}
#ifdef _WIN32
void windows_text_dependency_and_provenance_regressions() {
    const auto families=text_fonts();
    if(families.empty()) {std::cout<<"SKIP Windows Text regressions: no installed font families\n";return;}
    const auto preferred=std::find(families.begin(),families.end(),"Arial");
    const auto family=preferred==families.end()?families.front():*preferred;
    const auto text_object=[&](const Id& id) {
        Object object;object.id=id;object.name="Native Text provenance";object.kind=Kind::text;
        object.text=default_text(id+"-source","Native text");object.text->family=family;
        object.text->parameters.at("font_size").literal=12;
        object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));return object;
    };
    auto on_path=closure_fixture(true,false);auto text=text_object("on-path-text");
    const auto guide=external_guide();on_path.objects.emplace(guide.id,guide);
    on_path.compositions.front().roots.push_back(guide.id);
    text.text->path_attachment=TextPathAttachment{guide.id,"external-guide-contour"};
    on_path.objects.emplace(text.id,text);on_path.objects.at("effect-group").children.push_back(text.id);
    assert_scoped_refusal(on_path,"UNQUALIFIED_GEOMETRY_DEPENDENCY",false);

    auto driven=closure_fixture(true,false);auto target=text_object("driven-text");
    auto source=text_object("text-driver");source.visible=false;
    target.text->weight_driver=TextWeightDriver{{"text-driver","","text.weight"},0};
    driven.objects.emplace(target.id,target);driven.objects.emplace(source.id,source);
    driven.objects.at("effect-group").children.push_back(target.id);
    driven.compositions.front().roots.push_back(source.id);
    assert_scoped_refusal(driven,"DRIVEN_BAKE_CLOSURE_UNQUALIFIED",false);

    auto document=empty_document("instance-text-provenance","comp","board");
    auto authored=text_object("authored-text");
    auto master=group("definition-root",{authored.id});master.visible=false;
    Object instance;instance.id="text-instance";instance.name="Editable native Instance";instance.kind=Kind::instance;
    instance.instance=DefinitionInstance{"text-definition",{}};
    document.objects={{authored.id,authored},{master.id,master},{instance.id,instance}};
    document.definitions.emplace("text-definition",Definition{"text-definition","Text definition",master.id});
    document.compositions.front().roots={master.id,instance.id};
    Session session(document);const auto plan=result(session,plan_request());report_contract(plan,session,svg_profile);
    std::size_t projected_texts=0;
    for(const auto& value:plan.at("ir_nodes").as_array()) {
        const auto& node=value.as_object();
        if(node.at("kind")!="text"||string(node.at("source_id"))!=authored.id)continue;
        ++projected_texts;
        check(string(node.at("occurrence_id"))!=authored.id&&string(node.at("text_source_id"))==authored.text->id,
            "Definition Instance Text maps transient occurrence to the original native TextSource ID");
        check(string(node.at("source_ref").as_object().at("object"))==authored.id&&node.at("text_strategy")=="outlines",
            "Projected Text keeps native source provenance and an explicit target strategy");
    }
    check(projected_texts==1,"One visible Instance contributes exactly one Text provenance record");
}
#endif
void scoped_refusal_and_profile_options_contract() {
    auto backdrop=closure_fixture();backdrop.objects.at("effect-group").compositing.blend="multiply";
    assert_scoped_refusal(backdrop,"EXTERNAL_BACKDROP_CLOSURE_UNRESOLVED");
    auto stroke=closure_fixture();auto paint=default_operation("closure-stroke","nect.paint.stroke");
    paint.parameters.at("width").literal=8;stroke.objects.at("fill-a").stack.push_back(paint);
    assert_scoped_refusal(stroke,"STROKE_BAKE_BOUNDS_UNQUALIFIED");
    auto driven=closure_fixture();driven.objects.at("fill-a").contours.front().points.front().x.binding=
        Binding{{"transform-driver","transform-driver-p0","x"},1,0,"copy_local_value"};
    assert_scoped_refusal(driven,"DRIVEN_BAKE_CLOSURE_UNQUALIFIED");
    auto nested=closure_fixture();nested.objects.emplace("nested-mask-source",rectangle("nested-mask-source",0,0,80,80));
    nested.compositions.front().roots.push_back("nested-mask-source");
    GeometryMask nested_mask;nested_mask.id="nested-mask";nested_mask.source="nested-mask-source";
    nested.objects.at("mask-source").compositing.mask=nested_mask;
    assert_scoped_refusal(nested,"NESTED_MASK_CLOSURE_UNQUALIFIED");

    Session session(vector_fixture());error(session,plan_request("svg/2"),"UNSUPPORTED_TARGET_PROFILE");
    error(session,plan_request("AI/30.8"),"UNSUPPORTED_TARGET_PROFILE");
    for(const j::value bad:j::array{0,-1,16.000001,"2",true,nullptr,j::array{},j::object{}}) {
        auto input=plan_request();input["options"]=j::object{{"raster_scale",bad}};
        error(session,input,"INVALID_COMPATIBILITY_OPTIONS");
    }
    for(const j::value bad:j::array{nullptr,true,1,"options",j::array{}}) {
        auto input=plan_request();input["options"]=bad;error(session,input,"INVALID_COMPATIBILITY_OPTIONS");
    }
    auto unknown=plan_request();unknown["options"]=j::object{{"time",0}};
    error(session,unknown,"INVALID_COMPATIBILITY_OPTIONS");
    unknown=plan_request();unknown["format"]="svg";error(session,unknown,"UNKNOWN_FIELD");
    for(const double valid:{.25,16.0}) {
        auto input=plan_request();input["options"]=j::object{{"raster_scale",valid}};
        near(result(session,input).at("options").as_object().at("raster_scale"),valid,"Positive bounded raster scale is admitted");
    }
    auto missing_composition=plan_request();missing_composition["composition"]="absent";
    error(session,missing_composition,"MISSING_COMPOSITION");
    auto missing_board=plan_request();missing_board["artboard"]="absent";error(session,missing_board,"MISSING_ARTBOARD");
    auto multiple=vector_fixture();multiple.compositions.front().artboards.push_back({"second-board","Second",100,0,120,80});
    Session multi(multiple);error(multi,plan_request(svg_profile,false),"ARTBOARD_REQUIRED");
    const auto selected=result(multi,plan_request());check(selected.at("artboard")=="board","Explicit Artboard selects an unambiguous multi-board frame");

    // Unqualified AI/PDF policy planning is possible without shaping Text or
    // executing a raster renderer. The family is intentionally unavailable;
    // its native authored Text semantics remain present and unchanged.
    auto font_document=closure_fixture();Object text;text.id="authored-text";text.name="Unresolved font";text.kind=Kind::text;
    text.text=default_text("authored-text-source","Keep this native typography");
    text.text->family="Nect acceptance fixture intentionally unavailable family";
    font_document.objects.emplace(text.id,text);font_document.compositions.front().roots.push_back(text.id);
    Session no_font(font_document);
    std::set<std::string> policy_ids;
    for(const std::string_view profile:{"ai/30.8","pdf/x-4:2008"}) {
        const auto policy=result(no_font,plan_request(profile));report_contract(policy,no_font,profile);
        check(policy.at("status")=="policy_qualification_required"&&!policy.at("encoder_available").as_bool()&&
            !policy.at("export_supported").as_bool(),"Unqualified target policy and unavailable encoder are explicitly separate");
        check(policy.at("items").as_array().empty()&&policy.at("ir_nodes").as_array().empty()&&
            policy.at("bake_groups").as_array().empty()&&policy.at("editability_losses").as_array().empty(),
            "AI/PDF policy does not guess per-item representability or run font/raster evaluation");
        check(!policy.at("capability_requirements").as_array().empty()&&!policy.at("warnings").as_array().empty(),
            "Unqualified profile declares the outstanding capability requirements and limitations");
        check(policy==result(no_font,plan_request(profile)),"Unqualified profile analysis is deterministic and read-only");
        policy_ids.insert(string(policy.at("plan_id")));
        auto bad_options=plan_request(profile);bad_options["options"]=j::object{{"raster_scale",0}};
        error(no_font,bad_options,"INVALID_COMPATIBILITY_OPTIONS");
    }
    check(policy_ids.size()==2,"Target profile participates in plan identity");
    error(no_font,plan_request("future/1"),"UNSUPPORTED_TARGET_PROFILE");
}
}
int main() {
    try {
        deterministic_read_and_legacy_contract();
        isolated_dependency_closure_contract();
        geometry_and_typed_dependency_regressions();
        zero_ink_leaf_closure_regression();
#ifdef _WIN32
        windows_text_dependency_and_provenance_regressions();
#endif
        scoped_refusal_and_profile_options_contract();
        std::cout<<"PASS "<<checks<<" compatibility plan API/read-only/local-closure checks\n";return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;
    }
}
