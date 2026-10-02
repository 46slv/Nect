#include "nect/blend.hpp"
#include "nect/io.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,std::string("Expected ")+code+", got "+e.code);return;}throw std::runtime_error(std::string("Missing rejection: ")+code);}
Document fixture(){auto d=empty_document("doc","comp","board");Object a;a.id="a";a.name="A";a.kind=Kind::group;d.objects.emplace(a.id,a);d.compositions.front().roots.push_back(a.id);return d;}
std::string versioned(std::string native,unsigned minor){const std::string from="\"version\":\"0.79\"";const auto at=native.find(from);check(at!=std::string::npos,"Writer emits native0.79");native.replace(at,from.size(),"\"version\":\"0."+std::to_string(minor)+"\"");return native;}
constexpr std::array<std::string_view,16> ids{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn","hard-light","soft-light","difference","exclusion","hue","saturation","color","luminosity"};
void registry(){
    const auto descriptors=blend_modes();check(descriptors.size()==ids.size(),"Exactly16 product modes");
    Session s(fixture());const auto discovery=request(s,R"({"op":"compositing_types"})");
    check(discovery.find("\"blends\":[\"normal\",\"multiply\",\"screen\",\"overlay\",\"darken\",\"lighten\",\"color-dodge\",\"color-burn\",\"hard-light\",\"soft-light\",\"difference\",\"exclusion\",\"hue\",\"saturation\",\"color\",\"luminosity\"]")!=std::string::npos,"API compatibility list comes from exact registry order");
    for(std::size_t i=0;i<ids.size();++i){const auto& d=descriptors[i];check(d.id==ids[i]&&find_blend_mode(ids[i])==&d,"Immutable descriptor lookup and order");
        check(d.behavior_version==1&&d.alpha_behavior=="source-over"&&d.backdrop_scope=="current-compositing-scope"&&d.time_dependency=="none"&&d.ae_oracle_status=="unverified","Frozen per-mode semantics and honest AE status");
        check(d.profiles.size()==1&&d.profiles.front().id==nonseparable_blend_profile_id&&d.profiles.front().channel_bits==8&&d.profiles.front().alpha_bits==8&&d.profiles.front().alpha_representation=="premultiplied","Only current RGBA8 production profile");
        check(d.svg_representation=="css-mix-blend-mode"&&!d.svg_reader_requirement.empty(),"Descriptor-owned SVG target representation");
        check(d.introduced_native_minor==(i<12?11u:79u)&&d.renderer==(i<12?"qt-raster":"w3c-binary64"),"Historical introduction gate and per-mode renderer class");
        check(d.profiles.front().quantization==(i<12?"qt-raster-implementation-defined":"nearest-ties-up-clamp-to-rounded-alpha"),"Qt arithmetic cannot claim kernel rounding metadata");
        check(discovery.find("\"id\":\""+std::string(d.id)+"\"")!=std::string::npos,"Discovery descriptor exact ID");
    }
    check(nonseparable_blend_modes().data()==descriptors.data()+12&&nonseparable_blend_modes().size()==4,"Component view shares one registry");
    for(const auto mode:{"Hue","colour","luminosity ","unknown","softlight"})check(!find_blend_mode(mode)&&!find_nonseparable_blend_mode(mode),"No case/alias/future success");
    check(discovery.find("\"channel_bits\":16,\"supported\":false")!=std::string::npos&&discovery.find("\"channel_bits\":32,\"supported\":false")!=std::string::npos&&discovery.find("fixed-production-profile")!=std::string::npos,"Unavailable profile cells explicit");
    const auto plan=request(s,R"({"op":"export_plan","composition":"comp","artboard":"board"})");
    check(plan.find("blend_capabilities")!=std::string::npos&&plan.find("\"cross_reader_pixel_identity\":false")!=std::string::npos&&plan.find("\"nect_svg_blend_intake_supported\":false")!=std::string::npos,"SVG plan does not claim cross-reader identity or reimport");
}
void authored(){
    for(std::size_t i=0;i<ids.size();++i){const auto mode=std::string(ids[i]);Session s(fixture());const auto original=s.document();
        s.apply({SetCompositing{"a",mode,false}},s.revision());const auto saved=s.document();const auto native=encode(saved);
        check(decode(native)==saved&&encode(decode(native))==native,"Each supported ID exact native roundtrip");
        const auto values=evaluate(saved);const auto scene=evaluate_scene(saved,"comp",values,evaluate_transforms(saved,values));
        check(scene.roots.front().blend==mode&&scene.roots.front().isolated==(mode!="normal"),"Session/evaluation exact ID and implicit scope");
        if(mode!="normal"){s.undo(s.revision());check(s.document()==original,"Blend Undo exact");s.redo(s.revision());check(s.document()==saved,"Blend Redo exact");}
        const auto revision=s.revision();const auto history=s.history();
        rejects("MISSING_OBJECT",[&]{s.apply({SetCompositing{"a","normal",false},SetCompositing{"missing",mode,false}},s.revision());});
        check(s.document()==saved&&s.revision()==revision&&s.history()==history,"Failed-later-batch blend rollback");
        if(i<12)check(decode(versioned(native,78))==saved,"All old12 IDs retain native0.78 readability");
        else {
            for(const auto minor:{1u,10u,11u,64u,77u,78u})rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(native,minor));});
            // Hidden definition sources are decoded Objects even when not used.
            s.apply({DefinitionCommand{CreateDefinition{{"definition","D","a"}}},SetVisibility{"a",false}},s.revision());
            check(decode(encode(s.document()))==s.document(),"Definition-source native roundtrip");
            rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(encode(s.document()),78));});
            auto unknown=encode(s.document());const auto at=unknown.find("\"blend\":\""+mode+"\"");unknown.replace(at,mode.size()+10,"\"blend\":\"future-mode\"");
            rejects("UNSUPPORTED_BLEND",[&]{(void)decode(unknown);});
        }
    }
    Session s(fixture());const auto before=s.document();for(const auto mode:{"Hue","colour","bad","saturation ","Color"})rejects("UNSUPPORTED_BLEND",[&]{s.apply({SetCompositing{"a",mode,false}},s.revision());});
    const auto profile=request(s,R"({"op":"apply","expected_revision":0,"commands":[{"type":"set_compositing","object":"a","blend":"hue","isolated":false,"profile":"linear-srgb16"}]})");
    check(profile.find("UNSUPPORTED_BLEND_PROFILE")!=std::string::npos&&s.document()==before&&s.revision()==0,"Selected unavailable profile rejects atomically");
    const auto stale=request(s,R"({"op":"apply","expected_revision":1,"commands":[{"type":"set_compositing","object":"a","blend":"hue","isolated":false}]})");
    check(stale.find("REVISION_CONFLICT")!=std::string::npos&&s.document()==before,"Stale blend API draft does not edit");
    const auto good=request(s,R"({"op":"apply","expected_revision":0,"commands":[{"type":"set_compositing","object":"a","blend":"hue","isolated":false,"profile":"nect.srgb8-premultiplied/v1"}]})");
    check(good.find("\"changed\":true")!=std::string::npos&&s.document().objects.at("a").compositing.blend=="hue","Fixed production profile explicitly accepted without adding authored profile state");
}
}
int main(){try{registry();authored();std::cout<<"PASS "<<checks<<" nonseparable product registry/native/Session safety checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
