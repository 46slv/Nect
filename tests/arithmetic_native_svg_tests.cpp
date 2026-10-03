#include "nect/blend.hpp"
#include "nect/io.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Independent contract literals, not a registry-derived expected list or an
// export-plan-derived SVG oracle. Root registers this target with nect_io and
// the existing Boost include directory. No arithmetic pixel oracle lives here.
using namespace nect;
namespace j=boost::json;
namespace {
int checks=0;
constexpr std::array<std::string_view,16> css_ids{
    "normal","multiply","screen","overlay","darken","lighten","color-dodge",
    "color-burn","hard-light","soft-light","difference","exclusion",
    "hue","saturation","color","luminosity"};
constexpr std::array<std::string_view,10> arithmetic_ids{
    "linear-burn","linear-dodge","linear-light","vivid-light","pin-light",
    "hard-mix","subtract","divide","darker-color","lighter-color"};
constexpr std::array<std::string_view,3> classic_ids{
    "classic-color-burn","classic-color-dodge","classic-difference"};
constexpr auto earlier_minors=[] { std::array<unsigned,79> values{}; for(unsigned i=0;i<79;++i)values[i]=i+1; return values; }();

void check(bool ok,const std::string& why) {
    if(!ok)throw std::runtime_error(why);
    ++checks;
}
template<class F>void rejects(std::string_view code,F action) {
    try {action();}
    catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        return;
    }
    throw std::runtime_error("Missing rejection: "+std::string(code));
}
template<class F>void unchanged(Session& session,F action,const std::string& why) {
    const auto document=session.document();const auto bytes=encode(document);
    const auto revision=session.revision();const auto history=session.history();
    action();
    check(session.document()==document&&encode(session.document())==bytes&&
        session.revision()==revision&&session.history()==history,why);
}
void apply(Session& session,std::vector<Command> commands) {
    session.apply(commands,session.revision());
}
std::string str(const j::value& value) {return std::string(value.as_string());}
j::object result(Session& session,const std::string& command) {
    const auto response=j::parse(request(session,command)).as_object();
    check(response.at("ok").as_bool(),"API read succeeds: "+j::serialize(response));
    return response.at("result").as_object();
}
Object rectangle(const Id& id) {
    Object object;object.id=id;object.name=id;
    Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(const auto& xy:std::array<Vec2,4>{{{2,2},{30,2},{30,22},{2,22}}}) {
        Point point;point.id=id+"-p"+std::to_string(contour.points.size());
        point.x.literal=xy.x;point.y.literal=xy.y;contour.points.push_back(point);
    }
    object.contours.push_back(contour);
    object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
    return object;
}
Object group(const Id& id,std::vector<Id> children) {
    Object object;object.id=id;object.name=id;object.kind=Kind::group;
    object.children=std::move(children);return object;
}
Document fixture() {
    auto document=empty_document("arithmetic-doc","comp","board");
    document.objects.emplace("backdrop",rectangle("backdrop"));
    document.objects.emplace("leaf",rectangle("leaf"));
    document.compositions.front().roots={"backdrop","leaf"};return document;
}
Document nested_fixture() {
    auto document=fixture();document.objects.emplace("inner",group("inner",{"leaf"}));
    document.objects.emplace("outer",group("outer",{"inner"}));
    document.compositions.front().roots={"backdrop","outer"};return document;
}
std::string versioned(const std::string& native,unsigned minor) {
    auto value=j::parse(native);value.as_object()["version"]="0."+std::to_string(minor);
    return j::serialize(value);
}
std::string changed_blend(const std::string& native,const Id& id,std::string_view mode) {
    auto value=j::parse(native);
    for(auto& entry:value.as_object().at("objects").as_array()) {
        auto& object=entry.as_object();
        if(str(object.at("id"))==id) {
            object.at("compositing").as_object()["blend"]=j::value(mode);
            return j::serialize(value);
        }
    }
    throw std::runtime_error("Native fixture lacks Object "+id);
}

void registry_contract() {
    const auto descriptors=blend_modes();
    check(descriptors.size()==26,"Closed registry has old12 + HSL4 + arithmetic10");
    Session session(fixture());const auto discovery=result(session,R"({"op":"compositing_types"})");
    const auto& list=discovery.at("blends").as_array();
    const auto& exposed=discovery.at("blend_descriptors").as_array();
    check(list.size()==26&&exposed.size()==26,"Discovery exposes all and only26 modes");
    for(std::size_t i=0;i<26;++i) {
        const auto id=i<16?css_ids[i]:arithmetic_ids[i-16];
        const auto& descriptor=descriptors[i];const auto& api=exposed[i].as_object();
        check(descriptor.id==id&&find_blend_mode(id)==&descriptor&&str(list[i])==id&&
            str(api.at("id"))==id,"Stable exact registry/discovery order: "+std::string(id));
        const auto introduced=i<12?11u:(i<16?79u:80u);
        check(descriptor.introduced_native_minor==introduced&&
            str(api.at("introduced_native_version"))=="0."+std::to_string(introduced),
            "Independent native introduction literal: "+std::string(id));
        check(descriptor.behavior_version==1&&descriptor.ae_oracle_status=="unverified"&&
            descriptor.alpha_behavior=="source-over"&&descriptor.time_dependency=="none"&&
            descriptor.backdrop_scope=="current-compositing-scope","Profile remains bounded and AE unverified");
        check(descriptor.profiles.size()==1&&descriptor.profiles[0].id=="nect.srgb8-premultiplied/v1"&&
            descriptor.profiles[0].channel_bits==8&&descriptor.profiles[0].alpha_bits==8&&
            descriptor.profiles[0].alpha_representation=="premultiplied","Only the admitted RGBA8 profile");
        const auto svg=i<16?"css-mix-blend-mode":"unsupported";
        check(descriptor.svg_representation==svg&&
            str(api.at("svg").as_object().at("representation"))==svg,"Exact SVG descriptor policy: "+std::string(id));
        if(i>=16)check(descriptor.profiles[0].quantization=="nearest-ties-up-clamp-to-rounded-alpha",
            "Arithmetic descriptor states the once-rounded byte contract");
    }
    const auto component=nonseparable_blend_modes();
    check(component.size()==4&&component.data()==descriptors.data()+12,"HSL view does not grow with arithmetic registry");
    for(std::size_t i=0;i<4;++i)check(component[i].id==css_ids[i+12],"HSL component exact old ID");
    for(const auto id:arithmetic_ids)check(!find_nonseparable_blend_mode(id),"Arithmetic is not an HSL alias");
    for(const auto id:classic_ids)check(!find_blend_mode(id)&&!find_nonseparable_blend_mode(id),"Classic remains unregistered");
    for(const auto id:{"Linear-Dodge","linear_dodge","linear-dodge ","plus-lighter","future-mode"})
        check(!find_blend_mode(id),"No spelling/CSS/unknown alias admission");
}

void session_and_native_contract() {
    check(std::string(native_version)=="0.80","This isolated candidate writes native0.80");
    for(const auto id:arithmetic_ids) {
        const auto mode=std::string(id);Session session(fixture());const auto original=session.document();
        apply(session,{SetCompositing{"leaf",mode,false}});const auto authored=session.document();
        const auto saved=encode(authored);
        check(j::parse(saved).as_object().at("version")=="0.80","Serialized writer literal0.80");
        check(authored.objects.at("leaf").compositing.blend==mode&&decode(saved)==authored&&
            encode(decode(saved))==saved,"Exact ID/native0.80 roundtrip: "+mode);
        session.undo(session.revision());check(session.document()==original,"Undo exact blend state: "+mode);
        session.redo(session.revision());check(session.document()==authored,"Redo exact blend state: "+mode);
        const auto revision=session.revision();
        apply(session,{SetCompositing{"leaf",mode,false}});
        check(session.document()==authored&&session.revision()==revision+1,
            "Exact-ID reapply preserves document and existing Session commit semantics");
        unchanged(session,[&]{rejects("MISSING_OBJECT",[&]{apply(session,{
            SetCompositing{"leaf","normal",false},SetCompositing{"missing",mode,false}});});},
            "Failed-later-batch leaves Document/revision/History/native bytes intact");
        unchanged(session,[&]{rejects("UNSUPPORTED_BLEND",[&]{apply(session,{
            SetCompositing{"leaf","normal",false},SetCompositing{"backdrop","future-mode",false}});});},
            "Failed-later unknown blend never commits the first edit");
        unchanged(session,[&]{rejects("REVISION_CONFLICT",[&]{session.apply({SetCompositing{"leaf","normal",false}},session.revision()+1);});},
            "Stale draft leaves authored/native/History identity intact");
        for(const auto minor:earlier_minors)
            rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(saved,minor));});
        // A source remains part of native authoring even when no visible root
        // or Instance currently consumes it. The reader gate is not SVG policy.
        apply(session,{DefinitionCommand{CreateDefinition{{"definition","D","leaf"}}},SetVisibility{"leaf",false}});
        const auto hidden=encode(session.document());
        check(decode(hidden)==session.document(),"Hidden Definition source0.80 readback");
        for(const auto minor:earlier_minors)
            rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(hidden,minor));});
        rejects("UNSUPPORTED_BLEND",[&]{(void)decode(changed_blend(hidden,"leaf","future-mode"));});
        for(const auto classic:classic_ids)
            rejects("UNSUPPORTED_BLEND",[&]{(void)decode(changed_blend(hidden,"leaf",classic));});
        unchanged(session,[&]{
            const auto response=j::parse(request(session,"{\"op\":\"apply\",\"expected_revision\":"+
                std::to_string(session.revision())+",\"commands\":[{\"type\":\"set_compositing\",\"object\":\"leaf\",\"blend\":\""+
                mode+"\",\"isolated\":false,\"profile\":\"linear-srgb16\"}]}"));
            check(!response.as_object().at("ok").as_bool()&&
                str(response.as_object().at("error").as_object().at("code"))=="UNSUPPORTED_BLEND_PROFILE",
                "Unavailable arithmetic profile is explicit");
        },"API profile refusal cannot mutate authored data");
    }
    Session session(fixture());
    for(const auto id:{"Linear-Dodge","linear_dodge","linear-dodge ","plus-lighter","future-mode",
        "classic-color-burn","classic-color-dodge","classic-difference"}) {
        unchanged(session,[&]{rejects("UNSUPPORTED_BLEND",[&]{apply(session,{SetCompositing{"leaf",id,false}});});},
            "Alias/unknown/Classic Session rejection is atomic");
        rejects("UNSUPPORTED_BLEND",[&]{(void)decode(changed_blend(encode(session.document()),"leaf",id));});
    }
}

void legacy_typography_contract(bool include_arithmetic=true) {
    auto document=fixture();Object text;text.id="text";text.name="Retained typography";text.kind=Kind::text;
    text.text=default_text("text-source","Authored font intent");auto& authored=*text.text;
    authored.family="Pre-authored fixture family";authored.locale="en-US";authored.weight=650;authored.italic=true;
    authored.parameters.at("font_size").literal=27.25;
    authored.font_features={{"lig ",0xffffffffU,"whole_text"},{"KERN",1,"whole_text"}};
    authored.additional_axis_values={{"WIDE",12.25},{"wdth",87.1234567890123}};
    document.objects.emplace(text.id,text);document.compositions.front().roots.push_back(text.id);
    const auto old=versioned(encode(document),78);const auto reopened=decode(old);
    check(reopened==document&&*reopened.objects.at("text").text==authored,
        "Native0.78 retains nonempty padded/case-sensitive font feature and precise axis fields");
    Session session(reopened);
    if(include_arithmetic)for(const auto id:arithmetic_ids) {
        apply(session,{SetCompositing{"leaf",std::string(id),false}});
        const auto native=encode(session.document());const auto decoded=decode(native);
        check(decoded==session.document()&&decoded.objects.at("text")==text,
            "Arithmetic/native roundtrip preserves every prior typography authored field");
    }
    // Existing old12 remain readable at0.78; HSL retains its own0.79 gate.
    for(std::size_t i=0;i<css_ids.size();++i) {
        auto inherited=fixture();inherited.objects.at("leaf").compositing.blend=std::string(css_ids[i]);
        const auto bytes=encode(inherited);
        check(decode(versioned(bytes,i<12?78u:79u))==inherited,"Existing mode native introduction unchanged");
        if(i>=12)rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(bytes,78));});
    }
}

using ExpectedBlend=std::pair<Id,std::string>;
void svg_contract(Session& session,const std::vector<ExpectedBlend>& expected) {
    unchanged(session,[&]{
        const auto plan=result(session,R"({"op":"export_plan","composition":"comp","artboard":"board"})");
        check(plan.at("svg_export_supported").as_bool()==expected.empty(),"Plan support agrees with independently specified rendered content");
        const auto& entries=plan.at("unsupported_blends").as_array();
        check(entries.size()==expected.size(),"Plan lists every effectively rendered unsupported blend exactly once");
        auto remaining=expected;
        for(const auto& entry:entries) {
            const auto& refusal=entry.as_object();const auto object=str(refusal.at("object"));
            const auto blend=str(refusal.at("blend"));const auto reason=str(refusal.at("reason"));
            check(!object.empty()&&!reason.empty(),"Unsupported blend entry names Object and reason");
            const auto match=std::find_if(remaining.begin(),remaining.end(),[&](const auto& item){
                return (item.first.empty()||item.first==object)&&item.second==blend;});
            check(match!=remaining.end(),"Plan has only the expected exact blend/Object pair");remaining.erase(match);
        }
        check(remaining.empty(),"Plan does not omit a rendered unsupported node");
        std::string output="caller-owned output sentinel";
        if(expected.empty()) {
            output=export_svg(session.document(),"comp","board");
            check(output.starts_with("<svg ")&&output.ends_with("</svg>\n"),"Supported SVG exports complete output");
        } else {
            rejects("UNSUPPORTED_SVG_BLEND",[&]{output=export_svg(session.document(),"comp","board");});
            check(output=="caller-owned output sentinel","SVG refusal returns no partial output to persist");
        }
    },"Plan/SVG reads or refusals preserve Session Document/revision/History/native bytes");
}
Session definition_fixture(std::string_view mode,bool blend_on_root,bool create_instance,bool nested_instance=false) {
    auto document=fixture();document.objects.emplace("source",group("source",{"leaf"}));
    document.compositions.front().roots={"backdrop","source"};
    if(nested_instance) {
        document.objects.emplace("outer",group("outer",{}));
        document.compositions.front().roots.push_back("outer");
    }
    Session session(document);
    apply(session,{SetCompositing{blend_on_root?"source":"leaf",std::string(mode),false},
        DefinitionCommand{CreateDefinition{{"definition","D","source"}}},SetVisibility{"source",false}});
    if(create_instance)apply(session,{DefinitionCommand{CreateInstance{"comp",nested_instance?"outer":"","instance","definition","Visible Instance"}}});
    return session;
}
void svg_blend_contract() {
    for(const auto id:arithmetic_ids) {
        const auto mode=std::string(id);
        Session direct(fixture());apply(direct,{SetCompositing{"leaf",mode,false}});svg_contract(direct,{{"leaf",mode}});
        Session nested(nested_fixture());apply(nested,{SetCompositing{"leaf",mode,false}});svg_contract(nested,{{"leaf",mode}});
        apply(nested,{SetCompositing{"leaf","normal",false},SetCompositing{"inner",mode,false}});svg_contract(nested,{{"inner",mode}});
        // Each suppression boundary independently bypasses its entire subtree.
        apply(nested,{SetVisibility{"outer",false}});svg_contract(nested,{});
        apply(nested,{SetVisibility{"outer",true},Set{{"outer","","composite.opacity"},0}});svg_contract(nested,{});
        apply(nested,{Set{{"outer","","composite.opacity"},1},SetVisibility{"inner",false}});svg_contract(nested,{});
        apply(nested,{SetVisibility{"inner",true},Set{{"inner","","composite.opacity"},0}});svg_contract(nested,{});
        apply(direct,{SetVisibility{"leaf",false}});svg_contract(direct,{});
        apply(direct,{SetVisibility{"leaf",true},Set{{"leaf","","composite.opacity"},0}});svg_contract(direct,{});
        for(const auto root:{false,true}) {
            auto unused=definition_fixture(id,root,false);svg_contract(unused,{});
            auto used=definition_fixture(id,root,true);svg_contract(used,{{"",mode}});
            apply(used,{SetVisibility{"instance",false}});svg_contract(used,{});
            apply(used,{SetVisibility{"instance",true},Set{{"instance","","composite.opacity"},0}});svg_contract(used,{});
            apply(used,{Set{{"instance","","composite.opacity"},1},DefinitionCommand{
                SetInstanceOverride{"instance",{root?"source":"leaf","","composite.opacity"},0}}});
            svg_contract(used,{});
            auto nested_instance=definition_fixture(id,root,true,true);svg_contract(nested_instance,{{"",mode}});
            apply(nested_instance,{SetVisibility{"outer",false}});svg_contract(nested_instance,{});
            apply(nested_instance,{SetVisibility{"outer",true},Set{{"outer","","composite.opacity"},0}});svg_contract(nested_instance,{});
        }
    }
    // Multiple siblings and Group scopes must not stop the plan at the first
    // refusal, nor deduplicate distinct nodes just because the blend is equal.
    auto document=nested_fixture();document.objects.emplace("peer",rectangle("peer"));
    document.objects.at("inner").children.push_back("peer");Session several(document);
    apply(several,{SetCompositing{"outer","divide",false},SetCompositing{"leaf","hard-mix",false},
        SetCompositing{"peer","hard-mix",false}});
    svg_contract(several,{{"outer","divide"},{"leaf","hard-mix"},{"peer","hard-mix"}});
    // A tiny positive opacity is still rendered; do not invent an epsilon.
    apply(several,{Set{{"outer","","composite.opacity"},0.000001}});
    svg_contract(several,{{"outer","divide"},{"leaf","hard-mix"},{"peer","hard-mix"}});
    // Evaluated suppression must agree with literal suppression. The visible
    // and nonzero authored target literals here deliberately stay unchanged.
    Session driven_visibility(nested_fixture());
    apply(driven_visibility,{SetCompositing{"leaf","hard-mix",false},
        SetObjectVisibilityExpression{{"outer","","object.visible"},{"false",1},false}});
    check(driven_visibility.document().objects.at("outer").visible,"Visibility fixture authored literal remains true");
    svg_contract(driven_visibility,{});
    Session driven_opacity(nested_fixture());
    apply(driven_opacity,{SetCompositing{"leaf","hard-mix",false},
        Set{{"backdrop","","composite.opacity"},0},
        Link{{"outer","","composite.opacity"},Binding{{"backdrop","","composite.opacity"}}}});
    check(driven_opacity.document().objects.at("outer").compositing.opacity.literal==1,
        "Opacity fixture authored target literal remains nonzero");
    svg_contract(driven_opacity,{});
    for(const auto id:css_ids) {
        Session supported(fixture());apply(supported,{SetCompositing{"leaf",std::string(id),false}});svg_contract(supported,{});
        const auto svg=export_svg(supported.document(),"comp","board");
        if(id!="normal")check(svg.find("mix-blend-mode:"+std::string(id))!=std::string::npos,
            "Every existing CSS mode retains its exact SVG ID");
        auto supported_instance=definition_fixture(id,false,true,true);svg_contract(supported_instance,{});
    }
}
void inherited_svg_refusals() {
    for(const auto mode:{"alpha","luma"}) {
        Session session(fixture());GeometryMask mask;mask.id="mask";mask.source="backdrop";mask.mode=mode;
        apply(session,{SetMask{"leaf",mask}});
        unchanged(session,[&]{
            const auto plan=result(session,R"({"op":"export_plan","composition":"comp","artboard":"board"})");
            check(!plan.at("svg_export_supported").as_bool()&&plan.at("unsupported_masks").as_array().size()==1,
                "Existing enabled appearance mask remains an SVG refusal");
            rejects(mode==std::string_view("alpha")?"UNSUPPORTED_SVG_ALPHA_MASK":"UNSUPPORTED_SVG_LUMA_MASK",
                [&]{(void)export_svg(session.document(),"comp","board");});
        },"Inherited mask refusal is side-effect free");
        mask.enabled=false;apply(session,{SetMask{"leaf",mask}});
        check(!export_svg(session.document(),"comp","board").empty(),"Disabled appearance mask retains existing SVG bypass");
    }
    Session session(nested_fixture());auto effect=default_operation("posterize","nect.group.posterize");
    apply(session,{AddOperation{"inner",effect,0}});
    unchanged(session,[&]{
        const auto plan=result(session,R"({"op":"export_plan","composition":"comp","artboard":"board"})");
        check(!plan.at("svg_export_supported").as_bool()&&plan.at("unsupported_effects").as_array().size()==1,
            "Existing Group-effect plan refusal unchanged");
        rejects("UNSUPPORTED_SVG_EFFECT",[&]{(void)export_svg(session.document(),"comp","board");});
    },"Inherited Group-effect refusal is side-effect free");
    apply(session,{EnableOperation{"inner","posterize",false}});
    check(!export_svg(session.document(),"comp","board").empty(),"Disabled Group effect retains existing SVG bypass");
}
}
int main(int argc,char** argv) {
    try {
        const std::string selection=argc>1?argv[1]:"all";
        check(argc<=2&&(selection=="all"||selection=="registry"||selection=="native"||
            selection=="legacy"||selection=="legacy-readback"||selection=="svg"||selection=="inherited-svg"),"Known focused test selection");
        if(selection=="all"||selection=="registry")registry_contract();
        if(selection=="all"||selection=="native")session_and_native_contract();
        if(selection=="all"||selection=="legacy")legacy_typography_contract();
        if(selection=="legacy-readback")legacy_typography_contract(false);
        if(selection=="all"||selection=="svg")svg_blend_contract();
        if(selection=="all"||selection=="inherited-svg")inherited_svg_refusals();
        std::cout<<"PASS "<<checks<<" arithmetic registry/native/Session/SVG contract checks ("<<selection<<")\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<'\n';return 1;
    }
}
