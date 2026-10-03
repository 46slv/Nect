#include "nect/arithmetic_blend.hpp"
#include <algorithm>
#include <cmath>

namespace nect {
namespace {
enum class ArithmeticMode {
    linear_burn,linear_dodge,linear_light,vivid_light,pin_light,hard_mix,
    subtract,divide,darker_color,lighter_color,unsupported
};
ArithmeticMode resolve(std::string_view mode) noexcept {
    if(mode=="linear-burn")return ArithmeticMode::linear_burn;
    if(mode=="linear-dodge")return ArithmeticMode::linear_dodge;
    if(mode=="linear-light")return ArithmeticMode::linear_light;
    if(mode=="vivid-light")return ArithmeticMode::vivid_light;
    if(mode=="pin-light")return ArithmeticMode::pin_light;
    if(mode=="hard-mix")return ArithmeticMode::hard_mix;
    if(mode=="subtract")return ArithmeticMode::subtract;
    if(mode=="divide")return ArithmeticMode::divide;
    if(mode=="darker-color")return ArithmeticMode::darker_color;
    if(mode=="lighter-color")return ArithmeticMode::lighter_color;
    return ArithmeticMode::unsupported;
}
bool valid_rgb(const BlendRgb& color) noexcept {
    return std::all_of(color.begin(),color.end(),[](double channel) {
        return std::isfinite(channel)&&channel>=0&&channel<=1;
    });
}
double burn(double backdrop,double source) noexcept {
    // W3C modern Burn: endpoint priority avoids evaluating the singular 0/0.
    if(backdrop==1)return 1;
    if(source==0)return 0;
    return 1-std::min(1.,(1-backdrop)/source);
}
double dodge(double backdrop,double source) noexcept {
    // W3C modern Dodge: black backdrop wins even at a white source.
    if(backdrop==0)return 0;
    if(source==1)return 1;
    return std::min(1.,backdrop/(1-source));
}
double scalar(ArithmeticMode mode,double backdrop,double source) noexcept {
    // Preserve the frozen written expression/order on represented binary64
    // inputs. Clamp blend color here, before the caller composes source-over.
    switch(mode) {
    case ArithmeticMode::linear_burn:return std::clamp(backdrop+source-1,0.,1.);
    case ArithmeticMode::linear_dodge:return std::clamp(backdrop+source,0.,1.);
    case ArithmeticMode::linear_light:return std::clamp(backdrop+2*source-1,0.,1.);
    case ArithmeticMode::vivid_light:
        return source<.5?burn(backdrop,2*source):dodge(backdrop,2*source-1);
    case ArithmeticMode::pin_light:
        return source<.5?std::min(backdrop,2*source):std::max(backdrop,2*source-1);
    case ArithmeticMode::hard_mix:return backdrop+source>=1?1:0;
    case ArithmeticMode::subtract:return std::max(0.,backdrop-source);
    case ArithmeticMode::divide:
        if(backdrop==0)return 0;
        if(source==0)return 1;
        // A positive subnormal denominator can overflow the ratio; saturation
        // still gives finite bounded output, without denominator epsilon.
        return std::min(1.,backdrop/source);
    default:return 0; // Whole-color/unsupported modes never enter scalar().
    }
}
}

BlendRgbResult blend_arithmetic_rgb(std::string_view mode,BlendRgb backdrop,BlendRgb source) noexcept {
    const auto operation=resolve(mode);
    if(operation==ArithmeticMode::unsupported)return {BlendKernelStatus::unsupported_blend,{}};
    if(!valid_rgb(backdrop)||!valid_rgb(source))return {BlendKernelStatus::invalid_color,{}};
    if(operation==ArithmeticMode::darker_color||operation==ArithmeticMode::lighter_color) {
        // This is the normalized helper's represented sum, not a rational-byte
        // comparator. Left-associative R+G+B and exact ties are intentional.
        const double backdrop_total=(backdrop[0]+backdrop[1])+backdrop[2];
        const double source_total=(source[0]+source[1])+source[2];
        const bool choose_source=operation==ArithmeticMode::darker_color
            ?source_total<backdrop_total:source_total>backdrop_total;
        return {BlendKernelStatus::ok,choose_source?source:backdrop};
    }
    BlendRgb result{};
    for(std::size_t i=0;i<result.size();++i)result[i]=scalar(operation,backdrop[i],source[i]);
    return {BlendKernelStatus::ok,result};
}
}
