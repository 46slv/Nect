#include "nect/blend.hpp"
#include "nect/arithmetic_blend.hpp"
#include <algorithm>
#include <cmath>

namespace nect {
namespace {
constexpr std::array<BlendProfileDescriptor,1> profiles{{{
    nonseparable_blend_profile_id,8,8,"encoded-srgb","premultiplied","binary64",
    "nearest-ties-up-clamp-to-rounded-alpha"
}}};
constexpr std::array<BlendProfileDescriptor,1> qt_profiles{{{
    nonseparable_blend_profile_id,8,8,"encoded-srgb","premultiplied","qt-raster-integer",
    "qt-raster-implementation-defined"
}}};
constexpr std::array<BlendModeDescriptor,26> descriptors{{
    {"normal","Normal","normal",1,"qt-raster-normal","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"multiply","Multiply","separable",1,"qt-raster-multiply","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"screen","Screen","separable",1,"qt-raster-screen","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"overlay","Overlay","separable",1,"qt-raster-overlay","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"darken","Darken","separable",1,"qt-raster-darken","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"lighten","Lighten","separable",1,"qt-raster-lighten","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"color-dodge","Color Dodge","separable",1,"qt-raster-color-dodge","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"color-burn","Color Burn","separable",1,"qt-raster-color-burn","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"hard-light","Hard Light","separable",1,"qt-raster-hard-light","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"soft-light","Soft Light","separable",1,"qt-raster-soft-light","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"difference","Difference","separable",1,"qt-raster-difference","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"exclusion","Exclusion","separable",1,"qt-raster-exclusion","source-over","current-compositing-scope","none",qt_profiles,"unverified",11,"qt-raster","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"hue","Hue","nonseparable",1,"w3c-nonseparable","source-over","current-compositing-scope","none",profiles,"unverified",79,"w3c-binary64","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"saturation","Saturation","nonseparable",1,"w3c-nonseparable","source-over","current-compositing-scope","none",profiles,"unverified",79,"w3c-binary64","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"color","Color","nonseparable",1,"w3c-nonseparable","source-over","current-compositing-scope","none",profiles,"unverified",79,"w3c-binary64","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"luminosity","Luminosity","nonseparable",1,"w3c-nonseparable","source-over","current-compositing-scope","none",profiles,"unverified",79,"w3c-binary64","css-mix-blend-mode","SVG CSS mix-blend-mode and isolation support"},
    {"linear-burn","Linear Burn","arithmetic",1,"nect-linear-burn","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"linear-dodge","Linear Dodge","arithmetic",1,"nect-linear-dodge","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"linear-light","Linear Light","arithmetic",1,"nect-linear-light","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"vivid-light","Vivid Light","arithmetic",1,"nect-vivid-light-w3c-endpoints","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"pin-light","Pin Light","arithmetic",1,"nect-pin-light","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"hard-mix","Hard Mix","arithmetic",1,"nect-hard-mix-sum-greater-or-equal","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"subtract","Subtract","arithmetic",1,"nect-subtract-backdrop-minus-source","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"divide","Divide","arithmetic",1,"nect-divide-zero-numerator-first","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"darker-color","Darker Color","whole-color",1,"nect-straight-rgb-total-backdrop-tie","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"},
    {"lighter-color","Lighter Color","whole-color",1,"nect-straight-rgb-total-backdrop-tie","source-over","current-compositing-scope","none",profiles,"unverified",80,"deterministic-binary64","unsupported","No standard CSS blend or implemented lossless backdrop-aware projection"}
}};

// W3C Compositing and Blending Level 1, section 10.2. These coefficients are
// specifically Lum for nonseparable blending, not the separate luma-mask matrix.
double lum(const BlendRgb& color) noexcept {
    return .3*color[0]+.59*color[1]+.11*color[2];
}
double sat(const BlendRgb& color) noexcept {
    return std::max({color[0],color[1],color[2]})-std::min({color[0],color[1],color[2]});
}
BlendRgb clip_color(BlendRgb color) noexcept {
    // This helper is reachable only after SetLum to an admitted normalized
    // target. In real arithmetic Lum(color) therefore equals that target in
    // [0,1]. Separately rounded subnormal products can violate the invariant:
    // SetLum(gray6*denorm_min,0) leaves flat -denorm_min, and raw Lum equals the
    // negative minimum. Restore the derived luminance domain before scaling;
    // this is an exact domain clamp, not an epsilon or malformed-input repair.
    // It leaves ordinary in-domain values unchanged and guarantees positive
    // denominators and scale in [0,1] on each strict clipping branch.
    const double luminance=std::clamp(lum(color),0.,1.);
    // W3C snapshots both extrema on entry, before either correction.
    const double minimum=std::min({color[0],color[1],color[2]});
    const double maximum=std::max({color[0],color[1],color[2]});
    if(minimum<0) {
        const double scale=luminance/(luminance-minimum);
        for(auto& channel:color)channel=luminance+(channel-luminance)*scale;
    }
    if(maximum>1) {
        const double scale=(1-luminance)/(maximum-luminance);
        for(auto& channel:color)channel=luminance+(channel-luminance)*scale;
    }
    return color;
}
BlendRgb set_lum(BlendRgb color,double luminance) noexcept {
    const double difference=luminance-lum(color);
    for(auto& channel:color)channel+=difference;
    return clip_color(color);
}
BlendRgb set_sat(const BlendRgb& color,double saturation) noexcept {
    // Stable adjacent comparisons preserve original rank identities on ties.
    std::array<std::size_t,3> order{0,1,2};
    if(color[order[1]]<color[order[0]])std::swap(order[0],order[1]);
    if(color[order[2]]<color[order[1]])std::swap(order[1],order[2]);
    if(color[order[1]]<color[order[0]])std::swap(order[0],order[1]);
    const auto minimum=order[0],middle=order[1],maximum=order[2];
    BlendRgb result{};
    if(color[maximum]>color[minimum]) {
        // Equivalent factoring of the W3C expression avoids underflow of the
        // numerator when normalized colors contain tiny positive differences.
        result[middle]=((color[middle]-color[minimum])/(color[maximum]-color[minimum]))*saturation;
        result[maximum]=saturation;
    }
    // The minimum is zero. Equal channels stay equal, including the all-gray
    // case where min/mid/max are all zero without dividing by zero.
    return result;
}
bool valid_rgb(const BlendRgb& color) noexcept {
    return std::all_of(color.begin(),color.end(),[](double channel) {
        return std::isfinite(channel)&&channel>=0&&channel<=1;
    });
}
bool valid_pixel(PremultipliedSrgb8 pixel) noexcept {
    return pixel.r<=pixel.a&&pixel.g<=pixel.a&&pixel.b<=pixel.a;
}
BlendKernelStatus support_status(std::string_view mode,std::string_view profile) noexcept {
    const auto* descriptor=find_nonseparable_blend_mode(mode);
    if(!descriptor)return BlendKernelStatus::unsupported_blend;
    for(const auto& supported:descriptor->profiles)if(supported.id==profile)return BlendKernelStatus::ok;
    return BlendKernelStatus::unsupported_profile;
}
BlendRgbResult mix_rgb(std::string_view mode,const BlendRgb& backdrop,const BlendRgb& source) noexcept {
    BlendRgb result;
    // Only exact IDs resolved by the immutable descriptor registry reach here.
    if(mode=="hue")result=set_lum(set_sat(source,sat(backdrop)),lum(backdrop));
    else if(mode=="saturation")result=set_lum(set_sat(backdrop,sat(source)),lum(backdrop));
    else if(mode=="color")result=set_lum(source,lum(backdrop));
    else if(mode=="luminosity")result=set_lum(backdrop,lum(source));
    else return {BlendKernelStatus::unsupported_blend,{}};
    // W3C section 10 requires clamping the mixing function to the color range.
    // ClipColor precedes this final clamp; do not replace it with channel clamps.
    for(auto& channel:result)channel=std::clamp(channel,0.,1.);
    return {BlendKernelStatus::ok,result};
}
std::uint8_t quantize_byte(double channel) noexcept {
    // std::round classifies the represented scaled value directly. floor(x+.5)
    // can accidentally promote a genuinely sub-half value when the addition
    // itself rounds to the next integer.
    return static_cast<std::uint8_t>(std::round(std::clamp(channel,0.,1.)*255));
}
}

std::span<const BlendModeDescriptor> blend_modes() noexcept {return descriptors;}
const BlendModeDescriptor* find_blend_mode(std::string_view id) noexcept {
    for(const auto& descriptor:descriptors)if(descriptor.id==id)return &descriptor;
    return nullptr;
}
std::span<const BlendModeDescriptor> nonseparable_blend_modes() noexcept {return std::span(descriptors).subspan(12,4);}
const BlendModeDescriptor* find_nonseparable_blend_mode(std::string_view id) noexcept {
    const auto* descriptor=find_blend_mode(id);
    return descriptor&&descriptor->family=="nonseparable"?descriptor:nullptr;
}
BlendRgbResult blend_nonseparable_rgb(std::string_view mode,BlendRgb backdrop,BlendRgb source,
    std::string_view profile) noexcept {
    if(const auto status=support_status(mode,profile);status!=BlendKernelStatus::ok)return {status,{}};
    if(!valid_rgb(backdrop)||!valid_rgb(source))return {BlendKernelStatus::invalid_color,{}};
    return mix_rgb(mode,backdrop,source);
}
BlendPixelResult composite_nonseparable_srgb8(std::string_view mode,PremultipliedSrgb8 backdrop,
    PremultipliedSrgb8 source,std::string_view profile) noexcept {
    return composite_nonseparable_srgb8_opacity(mode,backdrop,source,1.,profile);
}
BlendPixelResult composite_nonseparable_srgb8_opacity(std::string_view mode,PremultipliedSrgb8 backdrop,
    PremultipliedSrgb8 source,double opacity,std::string_view profile) noexcept {
    if(const auto status=support_status(mode,profile);status!=BlendKernelStatus::ok)return {status,{}};
    return composite_deterministic_srgb8_opacity(mode,backdrop,source,opacity,profile);
}
BlendPixelResult composite_deterministic_srgb8_opacity(std::string_view mode,PremultipliedSrgb8 backdrop,
    PremultipliedSrgb8 source,double opacity,std::string_view profile) noexcept {
    const auto* descriptor=find_blend_mode(mode);
    if(!descriptor||(descriptor->renderer!="w3c-binary64"&&descriptor->renderer!="deterministic-binary64"))
        return {BlendKernelStatus::unsupported_blend,{}};
    if(std::none_of(descriptor->profiles.begin(),descriptor->profiles.end(),
        [&](const auto& candidate){return candidate.id==profile;}))
        return {BlendKernelStatus::unsupported_profile,{}};
    if(!valid_pixel(backdrop)||!valid_pixel(source))return {BlendKernelStatus::invalid_premultiplied_pixel,{}};
    if(!std::isfinite(opacity)||opacity<0||opacity>1)return {BlendKernelStatus::invalid_opacity,{}};
    if(source.a==0||opacity==0)return {BlendKernelStatus::ok,backdrop};
    if(backdrop.a==0) {
        if(opacity==1)return {BlendKernelStatus::ok,source};
        const auto alpha=quantize_byte((source.a/255.)*opacity);
        return {BlendKernelStatus::ok,{std::min(alpha,quantize_byte((source.r/255.)*opacity)),
            std::min(alpha,quantize_byte((source.g/255.)*opacity)),
            std::min(alpha,quantize_byte((source.b/255.)*opacity)),alpha}};
    }
    const double source_alpha=(source.a/255.)*opacity,backdrop_alpha=backdrop.a/255.;
    const BlendRgb source_rgb{double(source.r)/source.a,double(source.g)/source.a,double(source.b)/source.a};
    const BlendRgb backdrop_rgb{double(backdrop.r)/backdrop.a,double(backdrop.g)/backdrop.a,double(backdrop.b)/backdrop.a};
    BlendRgbResult mixed;
    if(descriptor->renderer=="w3c-binary64")mixed=mix_rgb(mode,backdrop_rgb,source_rgb);
    else if(mode=="darker-color"||mode=="lighter-color") {
        // Compare the exact original straight-byte totals, not rounded
        // normalized sums, premultiplied totals or weighted luminance.
        const unsigned backdrop_total=(unsigned(backdrop.r)+backdrop.g+backdrop.b)*source.a;
        const unsigned source_total=(unsigned(source.r)+source.g+source.b)*backdrop.a;
        const bool choose_source=mode=="darker-color"?source_total<backdrop_total:source_total>backdrop_total;
        mixed={BlendKernelStatus::ok,choose_source?source_rgb:backdrop_rgb};
    } else mixed=blend_arithmetic_rgb(mode,backdrop_rgb,source_rgb);
    if(mixed.status!=BlendKernelStatus::ok)return {mixed.status,{}};
    const auto alpha=quantize_byte(source_alpha+backdrop_alpha*(1-source_alpha));
    std::array<std::uint8_t,3> channels{};
    // W3C sections 6 and 10: straight-space blend followed by source-over.
    for(std::size_t i=0;i<channels.size();++i) {
        const double premultiplied=source_alpha*(1-backdrop_alpha)*source_rgb[i]
            +source_alpha*backdrop_alpha*mixed.value[i]+(1-source_alpha)*backdrop_alpha*backdrop_rgb[i];
        channels[i]=std::min(alpha,quantize_byte(premultiplied));
    }
    return {BlendKernelStatus::ok,{channels[0],channels[1],channels[2],alpha}};
}
}
