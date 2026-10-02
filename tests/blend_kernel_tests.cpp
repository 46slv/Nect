#include "nect/blend.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace nect;
namespace {
std::size_t checks=0;
// Conservative absolute bound for bounded [0,1] binary64 helper arithmetic;
// independent fixed-rational expectations do not assert bitwise double parity.
constexpr double formula_tolerance=128*std::numeric_limits<double>::epsilon();
void check(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
    ++checks;
}
void rgb_close(BlendRgb actual,BlendRgb expected,const std::string& message,double tolerance=formula_tolerance) {
    for(std::size_t i=0;i<3;++i)
        check(std::isfinite(actual[i])&&std::abs(actual[i]-expected[i])<=tolerance,message+" channel "+std::to_string(i));
}
constexpr std::array<std::string_view,4> modes{"hue","saturation","color","luminosity"};
BlendRgb blend(std::string_view mode,BlendRgb backdrop,BlendRgb source) {
    const auto result=blend_nonseparable_rgb(mode,backdrop,source);
    check(result.status==BlendKernelStatus::ok,"RGB kernel succeeds");
    return result.value;
}
PremultipliedSrgb8 composite(std::string_view mode,PremultipliedSrgb8 backdrop,PremultipliedSrgb8 source) {
    const auto result=composite_nonseparable_srgb8(mode,backdrop,source);
    check(result.status==BlendKernelStatus::ok,"Premultiplied kernel succeeds");
    return result.value;
}
void descriptor_contract() {
    const auto descriptors=nonseparable_blend_modes();
    check(descriptors.size()==4,"This bounded registry fragment contains exactly four modes");
    const std::array<std::string_view,4> labels{"Hue","Saturation","Color","Luminosity"};
    for(std::size_t i=0;i<descriptors.size();++i) {
        const auto& d=descriptors[i];
        check(d.id==modes[i]&&d.label==labels[i],"Stable exact mode ID and label");
        check(find_nonseparable_blend_mode(d.id)==&d,"Lookup uses the immutable descriptor registry");
        check(d.family=="nonseparable"&&d.behavior_version==1,"Nonseparable behavior version is explicit");
        check(d.color_operation=="w3c-nonseparable"&&d.alpha_behavior=="source-over","Color and alpha classes are explicit");
        check(d.backdrop_scope=="current-compositing-scope"&&d.time_dependency=="none","Scope/time contract is explicit");
        check(d.ae_oracle_status=="unverified","Formula tests never assert AE parity");
        check(d.profiles.size()==1,"Only the bounded first profile is declared");
        const auto& p=d.profiles.front();
        check(p.id==nonseparable_blend_profile_id&&p.channel_bits==8&&p.alpha_bits==8,"Versioned RGB/alpha precision");
        check(p.color_space=="encoded-srgb"&&p.alpha_representation=="premultiplied","No hidden linearization or straight-alpha input");
        check(p.intermediate_precision=="binary64"&&p.quantization=="nearest-ties-up-clamp-to-rounded-alpha","Intermediate and byte policies are explicit");
    }
    check(nonseparable_blend_modes().data()==descriptors.data(),"Registry storage is stable and read-only");
}
void independent_rational_vectors() {
    // Independent exact-rational calculations of W3C section 10.2. The expected
    // values are fixed constants; no production helper computes this oracle.
    struct Vector {BlendRgb b,s;std::array<BlendRgb,4> expected;};
    const std::array<Vector,8> vectors{{
        {{.2,.6,.8},{.9,.1,.4},{{
            {3589./4000,1189./4000,2089./4000},
            {149./1500,949./1500,1349./1500},
            {1,643./2635,278./527},
            {71./1000,471./1000,671./1000}}}},
        {{.5,.5,.5},{1,0,0},{{{.5,.5,.5},{.5,.5,.5},{1,2./7,2./7},{.3,.3,.3}}}},
        {{1,0,0},{.5,.5,.5},{{{.3,.3,.3},{.3,.3,.3},{.3,.3,.3},{1,2./7,2./7}}}},
        {{0,0,1},{1,1,0},{{{11./89,11./89,0},{0,0,1},{11./89,11./89,0},{78./89,78./89,1}}}},
        {{1,1,0},{0,0,1},{{{78./89,78./89,1},{1,1,0},{78./89,78./89,1},{11./89,11./89,0}}}},
        {{.2,.2,.8},{.9,.1,.1},{{{.686,.086,.086},{.178,.178,.978},{.826,.026,.026},{.274,.274,.874}}}},
        {{.8,.8,.2},{.1,.9,.9},{{{.314,.914,.914},{.822,.822,.022},{.174,.974,.974},{.726,.726,.126}}}},
        {{0,1,0},{1,0,0},{{{1,29./70,29./70},{0,1,0},{1,29./70,29./70},{0,30./59,0}}}}
    }};
    for(const auto& v:vectors)for(std::size_t i=0;i<modes.size();++i)
        rgb_close(blend(modes[i],v.b,v.s),v.expected[i],"Independent rational vector "+std::string(modes[i]));
    for(const auto level:{0.,.5,1.})for(const auto other:{0.,.5,1.}) {
        const BlendRgb b{level,level,level},s{other,other,other};
        for(const auto mode:modes)rgb_close(blend(mode,b,s),mode=="luminosity"?s:b,"Black/white/gray flat-color endpoints");
    }
    for(const auto mode:modes) {
        rgb_close(blend(mode,{0,0,0},{.2,.6,.8}),mode=="luminosity"?BlendRgb{.502,.502,.502}:BlendRgb{0,0,0},"Zero-luminance clipping denominator");
        rgb_close(blend(mode,{1,1,1},{.2,.6,.8}),mode=="luminosity"?BlendRgb{.502,.502,.502}:BlendRgb{1,1,1},"Unit-luminance clipping denominator");
        rgb_close(blend(mode,{.2,.6,.8},{0,0,0}),mode=="luminosity"?BlendRgb{0,0,0}:BlendRgb{.502,.502,.502},"Zero-saturation/black source");
        rgb_close(blend(mode,{.2,.6,.8},{1,1,1}),mode=="luminosity"?BlendRgb{1,1,1}:BlendRgb{.502,.502,.502},"Zero-saturation/white source");
    }
}
void rank_ties_and_clipping_boundaries() {
    // All six orderings, both equal-low and equal-high ties. SetSat is specified
    // by original ranks; tied equal channels must remain equal in any ordering.
    const std::array<std::array<std::size_t,3>,6> permutations{{{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}}};
    for(const auto base:{BlendRgb{.2,.2,.8},BlendRgb{.8,.8,.2},BlendRgb{.2,.5,.8}})for(const auto& order:permutations) {
        const BlendRgb source{base[order[0]],base[order[1]],base[order[2]]};
        const auto hue=blend("hue",{.4,.5,.6},source);
        const double minimum=std::min({source[0],source[1],source[2]});
        const double maximum=std::max({source[0],source[1],source[2]});
        BlendRgb independently_scaled{};
        for(std::size_t i=0;i<3;++i)independently_scaled[i]=(source[i]-minimum)/(maximum-minimum)*.2;
        const double shift=.481-(.3*independently_scaled[0]+.59*independently_scaled[1]+.11*independently_scaled[2]);
        for(auto& channel:independently_scaled)channel+=shift;
        rgb_close(hue,independently_scaled,"Rank/saturation oracle without production sorting or helper calls");
    }
    const BlendRgb red{1,0,0};
    for(const auto target:{std::nextafter(.3,0.),.3,std::nextafter(.3,1.)}) {
        const auto c=blend("color",{target,target,target},red);
        for(const auto value:c)check(value>=0&&value<=1,"Both sides of clipping branch boundary stay in gamut");
        rgb_close(c,red,"Clipping boundary is continuous",5e-15);
    }
    const double tiny=std::numeric_limits<double>::denorm_min();
    for(const auto mode:modes) {
        const auto result=blend(mode,{0,tiny,tiny},{tiny,0,tiny});
        for(const auto value:result)check(std::isfinite(value)&&value>=0&&value<=1,"Tiny rank differences remain finite and in gamut");
    }
}
void admitted_subnormal_and_near_achromatic_inputs_are_finite() {
    const double t=std::numeric_limits<double>::denorm_min();
    const BlendRgb black{};
    // RED repro: each weighted gray6t component rounds separately, so Lum
    // becomes7t. SetLum to zero leaves flat -t, which previously made the
    // ClipColor luminance-minus-minimum denominator zero and returned OK/NaN.
    rgb_close(blend("color",black,{6*t,6*t,6*t}),black,"Subnormal achromatic source to zero luminance",0);
    rgb_close(blend("luminosity",{6*t,6*t,6*t},black),black,"Subnormal achromatic backdrop to zero luminance",0);
    for(const auto gray:{.6,std::numeric_limits<double>::min(),std::nextafter(std::numeric_limits<double>::min(),0.),32*t}) {
        const BlendRgb color{gray,gray,gray};
        rgb_close(blend("color",black,color),black,"Achromatic source to black within normalized formula tolerance");
        rgb_close(blend("luminosity",color,black),black,"Achromatic backdrop to black within normalized formula tolerance");
    }
    // Bounded exhaustive cube includes every channel ordering, tie family and
    // uniform/nonuniform separately-rounded luminance among these subnormals.
    for(unsigned red=0;red<=16;++red)for(unsigned green=0;green<=16;++green)for(unsigned blue=0;blue<=16;++blue) {
        const BlendRgb tiny{red*t,green*t,blue*t};
        for(const auto mode:modes)for(const auto reverse:{false,true}) {
            const auto out=blend(mode,reverse?tiny:black,reverse?black:tiny);
            for(const auto value:out)check(std::isfinite(value)&&value>=0&&value<=1,"Every admitted subnormal cube result is finite and in gamut");
        }
    }
    for(const auto base:{t,6*t,32*t,std::numeric_limits<double>::min(),std::nextafter(std::numeric_limits<double>::min(),0.),.3,.6,std::nextafter(1.,0.),1.}) {
        const double down=std::nextafter(base,0.);
        const double up=base==1?1:std::nextafter(base,1.);
        for(const auto red:{down,base,up})for(const auto green:{down,base,up})for(const auto blue:{down,base,up}) {
            const BlendRgb near_gray{red,green,blue};
            for(const auto endpoint:{black,BlendRgb{t,t,t},BlendRgb{1,1,1}})for(const auto mode:modes)for(const auto reverse:{false,true}) {
                const auto out=blend(mode,reverse?near_gray:endpoint,reverse?endpoint:near_gray);
                for(const auto value:out)check(std::isfinite(value)&&value>=0&&value<=1,"Adjacent near-achromatic/normal-boundary RGB remains finite and in gamut");
            }
        }
    }
}
void independent_byte_and_alpha_vectors() {
    // Opaque vectors derived independently from the rational vectors above.
    const PremultipliedSrgb8 b{51,153,204,255},s{230,26,102,255};
    // Actual sRGB8 straight source is (230,26,102)/255, not (.9,.1,.4).
    const std::array<PremultipliedSrgb8,4> expected_opaque{{{229,76,133,255},{25,161,229,255},{255,62,134,255},{19,121,172,255}}};
    for(std::size_t i=0;i<modes.size();++i)
        check(composite(modes[i],b,s)==expected_opaque[i],"Independent opaque byte vector");
    // Unequal-alpha stored inputs are already premultiplied. Fixed expectations
    // are exact-rational W3C source-over results rounded once at the byte boundary.
    const PremultipliedSrgb8 mixed_b{30,90,120,150},mixed_s{100,20,60,125};
    const std::array<PremultipliedSrgb8,4> expected_mixed{{{122,75,129,201},{70,99,146,201},{124,74,130,201},{63,90,136,201}}};
    for(std::size_t i=0;i<modes.size();++i)
        check(composite(modes[i],mixed_b,mixed_s)==expected_mixed[i],"Independent mixed-alpha byte vector");
    const PremultipliedSrgb8 empty{};
    for(const auto mode:modes) {
        check(composite(mode,mixed_b,empty)==mixed_b,"Transparent source exactly preserves backdrop bytes");
        check(composite(mode,empty,mixed_s)==mixed_s,"Transparent backdrop exactly preserves source bytes");
        check(composite(mode,empty,empty)==empty,"Both transparent inputs are canonical zero");
        check(composite(mode,{0,0,0,255},{255,0,0,255})==(mode=="luminosity"?PremultipliedSrgb8{77,77,77,255}:PremultipliedSrgb8{0,0,0,255}),"76.5 byte tie rounds upward");
    }
    // Two alpha-128 samples: ao=128+128*127/255=191.749..., nearest 192.
    for(const auto mode:modes)check(composite(mode,{0,0,0,128},{0,0,0,128})==PremultipliedSrgb8{0,0,0,192},"Mixed alpha uses source-over, rounded once");
}
void dense_invariants() {
    // Deterministic, bounded coverage of all alpha bytes and channel orderings.
    // No duplicated reference blend implementation is used for these invariants.
    for(unsigned alpha=0;alpha<=255;++alpha)for(unsigned seed=0;seed<31;++seed) {
        const auto byte=[](unsigned value){return static_cast<std::uint8_t>(value);};
        const unsigned other=(alpha*73+seed*19)%256;
        const PremultipliedSrgb8 b{byte((seed*17)%(alpha+1)),byte((seed*29)%(alpha+1)),byte((seed*43)%(alpha+1)),byte(alpha)};
        const PremultipliedSrgb8 s{byte((seed*31)%(other+1)),byte((seed*47)%(other+1)),byte((seed*59)%(other+1)),byte(other)};
        const unsigned expected_alpha=alpha+(other*(255-alpha)+127)/255;
        for(const auto mode:modes) {
            const auto out=composite(mode,b,s);
            check(out.a==expected_alpha,"All alpha bytes obey independent integer source-over oracle");
            check(out.r<=out.a&&out.g<=out.a&&out.b<=out.a,"Final byte color is bounded by rounded alpha");
            check(out.a!=0||out==PremultipliedSrgb8{},"Output alpha zero is canonical");
            check(composite(mode,b,s)==out,"Same inputs/profile are deterministic");
        }
        const BlendRgb cb{b.r/255.,b.g/255.,b.b/255.},cs{s.r/255.,s.g/255.,s.b/255.};
        rgb_close(blend("color",cb,cs),blend("luminosity",cs,cb),"Color/luminosity inverse argument identity");
        for(const auto mode:modes) {
            const auto out=blend(mode,cb,cs);
            const double lum=.3*out[0]+.59*out[1]+.11*out[2];
            const auto& luminance_source=mode=="luminosity"?cs:cb;
            const double expected=.3*luminance_source[0]+.59*luminance_source[1]+.11*luminance_source[2];
            check(std::abs(lum-expected)<formula_tolerance,"W3C Lum survives SetLum/ClipColor");
            for(const auto value:out)check(std::isfinite(value)&&value>=0&&value<=1,"Every formula result is finite and clamped");
            rgb_close(blend(mode,cb,cb),cb,"Equal-color straight/premultiplied conversion identity");
        }
    }
}
void conversion_and_quantization_boundaries() {
    // Same straight primary/white color represented at every alpha byte must
    // remain that color, with premultiplied channels exactly equal to output
    // alpha where the corresponding straight channel is one.
    const std::array<std::array<unsigned,3>,5> colors{{{0,0,0},{1,0,0},{0,1,0},{0,0,1},{1,1,1}}};
    for(unsigned a=0;a<=255;++a)for(const auto& color:colors) {
        const unsigned other=(a*43+17)%256;
        const auto b=static_cast<std::uint8_t>(a),s=static_cast<std::uint8_t>(other);
        const auto ba=static_cast<std::uint8_t>(other+(a*(255-other)+127)/255);
        const PremultipliedSrgb8 backdrop{static_cast<std::uint8_t>(color[0]*b),static_cast<std::uint8_t>(color[1]*b),static_cast<std::uint8_t>(color[2]*b),b};
        const PremultipliedSrgb8 source{static_cast<std::uint8_t>(color[0]*s),static_cast<std::uint8_t>(color[1]*s),static_cast<std::uint8_t>(color[2]*s),s};
        const PremultipliedSrgb8 expected{static_cast<std::uint8_t>(color[0]*ba),static_cast<std::uint8_t>(color[1]*ba),static_cast<std::uint8_t>(color[2]*ba),ba};
        for(const auto mode:modes)check(composite(mode,backdrop,source)==expected,"Straight decode/re-premultiply preserves common primary color at any alpha");
    }
    for(const unsigned a:{0U,2U,64U,128U,254U})for(const unsigned s:{0U,2U,64U,128U,254U}) {
        const auto channel=static_cast<std::uint8_t>((255*(a+s)-a*s+255)/510);
        const auto alpha=static_cast<std::uint8_t>(s+(a*(255-s)+127)/255);
        const PremultipliedSrgb8 expected{channel,channel,channel,alpha};
        for(const auto mode:modes)
            check(composite(mode,{static_cast<std::uint8_t>(a/2),static_cast<std::uint8_t>(a/2),static_cast<std::uint8_t>(a/2),static_cast<std::uint8_t>(a)},
                {static_cast<std::uint8_t>(s/2),static_cast<std::uint8_t>(s/2),static_cast<std::uint8_t>(s/2),static_cast<std::uint8_t>(s)})==expected,
                "Exactly representable straight half-gray is rounded after source-over, not before");
    }
    // Explicit numerical limitation: these rational values are exactly 19.5
    // and 25.5 bytes, but binary64 formula evaluation lands below each tie.
    // The adapter rounds its represented result, without an arbitrary epsilon.
    check(composite("luminosity",{0,0,0,255},{65,0,0,255})==PremultipliedSrgb8{19,19,19,255},"red65 binary64 boundary regression; ideal-rational expectation is20");
    check(composite("luminosity",{0,0,0,255},{85,0,0,255})==PremultipliedSrgb8{25,25,25,255},"red85 binary64 boundary regression; ideal-rational expectation is26");
    std::size_t rational_mismatches=0;
    for(unsigned r=0;r<=255;++r) {
        const auto out=composite("luminosity",{0,0,0,255},{static_cast<std::uint8_t>(r),0,0,255});
        const unsigned rational_nearest=(3*r+5)/10;
        const auto error=std::abs(int(out.r)-int(rational_nearest));
        check(error<=1,"Opaque red/gray byte oracle has at most one-byte rational boundary drift");
        if(error) {
            ++rational_mismatches;
            check((3*r)%10==5,"Observed rational drift is restricted to exact half ties in this exhaustive ramp");
        }
    }
    check(rational_mismatches==8,"Exhaustive 256-step rational red ramp records all eight binary64 tie differences");
}
void refusal_contract() {
    const BlendRgb valid{.2,.4,.6};
    const PremultipliedSrgb8 pixel{10,20,30,100};
    for(const auto id:{"","normal","multiply","Hue","hue ","future-mode","hsl"}) {
        check(find_nonseparable_blend_mode(id)==nullptr,"No mode aliases or implicit normal");
        check(blend_nonseparable_rgb(id,valid,valid).status==BlendKernelStatus::unsupported_blend,"Unsupported RGB mode is explicit");
        check(composite_nonseparable_srgb8(id,{},{}).status==BlendKernelStatus::unsupported_blend,"Transparent endpoints cannot bypass mode refusal");
    }
    for(const auto profile:{"","srgb16-premultiplied","srgb32-premultiplied","linear-srgb8-premultiplied","nect.srgb8-premultiplied/v2"})for(const auto mode:modes) {
        check(blend_nonseparable_rgb(mode,valid,valid,profile).status==BlendKernelStatus::unsupported_profile,"Unsupported RGB profile is explicit");
        check(composite_nonseparable_srgb8(mode,{},pixel,profile).status==BlendKernelStatus::unsupported_profile,"Endpoint cannot bypass profile refusal");
    }
    for(const auto invalid:{-1.,std::nextafter(0.,-1.),std::nextafter(1.,2.),2.,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        for(std::size_t channel=0;channel<3;++channel) {
            auto malformed=valid;malformed[channel]=invalid;
            check(blend_nonseparable_rgb("hue",malformed,valid).status==BlendKernelStatus::invalid_color,"Invalid backdrop RGB refused");
            check(blend_nonseparable_rgb("hue",valid,malformed).status==BlendKernelStatus::invalid_color,"Invalid source RGB refused");
        }
    for(const auto malformed:{PremultipliedSrgb8{101,0,0,100},PremultipliedSrgb8{0,101,0,100},PremultipliedSrgb8{0,0,101,100},PremultipliedSrgb8{1,0,0,0}}) {
        check(composite_nonseparable_srgb8("color",malformed,{}).status==BlendKernelStatus::invalid_premultiplied_pixel,"Malformed backdrop is not silently repaired");
        check(composite_nonseparable_srgb8("color",{},malformed).status==BlendKernelStatus::invalid_premultiplied_pixel,"Malformed source is not silently repaired");
    }
    check(blend_kernel_status_code(BlendKernelStatus::unsupported_blend)=="UNSUPPORTED_BLEND","Unsupported mode has stable error code");
    check(blend_kernel_status_code(BlendKernelStatus::unsupported_profile)=="UNSUPPORTED_BLEND_PROFILE","Unsupported profile has stable error code");
}
}
int main() {
    try {
        descriptor_contract();independent_rational_vectors();rank_ties_and_clipping_boundaries();admitted_subnormal_and_near_achromatic_inputs_are_finite();
        independent_byte_and_alpha_vectors();dense_invariants();conversion_and_quantization_boundaries();refusal_contract();
        std::cout<<"blend_kernel_tests: "<<checks<<" checks passed\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"blend_kernel_tests: "<<error.what()<<" after "<<checks<<" checks\n";
        return 1;
    }
}
