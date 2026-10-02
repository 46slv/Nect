#include "nect/blend.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
constexpr std::array<const char*,4> modes{"hue","saturation","color","luminosity"};
constexpr std::array<PremultipliedSrgb8,4> opaque_half{{{105,80,122,255},{41,104,168,255},{122,71,122,255},{56,107,158,255}}};
constexpr std::array<PremultipliedSrgb8,4> opaque_full{{{160,58,92,255},{30,107,183,255},{194,41,92,255},{61,112,163,255}}};
constexpr std::array<PremultipliedSrgb8,4> alpha_half{{{77,80,80,168},{48,90,104,168},{79,80,79,168},{45,85,98,168}}};
constexpr std::array<PremultipliedSrgb8,4> alpha_full{{{135,81,59,208},{77,101,107,208},{137,80,57,208},{70,90,95,208}}};
void exact(const BlendPixelResult& result,PremultipliedSrgb8 expected){check(result.status==BlendKernelStatus::ok,"Opacity result must be successful");check(result.value==expected,"Opacity must match fixed independent Fraction oracle");}
}
int main(){try {
    // Independent rational formula fixtures, no exact final half ties. The
    // reviewed giant corpus stays in its existing target; these are new bridge
    // consumers, not a copied blend implementation.
    for(std::size_t i=0;i<modes.size();++i) {
        const auto mode=modes[i];const PremultipliedSrgb8 b{51,102,153,255},s{204,51,102,255},ab{20,80,100,128},as{120,40,10,160};
        for(const auto opacity:{0.,.5,1.}) {
            exact(composite_nonseparable_srgb8_opacity(mode,b,s,opacity),opacity==0?b:opacity==.5?opaque_half[i]:opaque_full[i]);
            exact(composite_nonseparable_srgb8_opacity(mode,ab,as,opacity),opacity==0?ab:opacity==.5?alpha_half[i]:alpha_full[i]);
            exact(composite_nonseparable_srgb8_opacity(mode,{},as,opacity),opacity==0?PremultipliedSrgb8{}:opacity==.5?PremultipliedSrgb8{60,20,5,80}:as);
            exact(composite_nonseparable_srgb8_opacity(mode,ab,{},opacity),ab);
        }
        check(composite_nonseparable_srgb8_opacity(mode,ab,as,1).value==composite_nonseparable_srgb8(mode,ab,as).value,"Opacity1 preserves reviewed adapter bytes");
        for(const auto opacity:{-1.,1.1,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
            const auto result=composite_nonseparable_srgb8_opacity(mode,ab,as,opacity);
            check(result.status==BlendKernelStatus::invalid_opacity&&result.value==PremultipliedSrgb8{},"Invalid opacity must refuse with canonical zero");
        }
        const auto unsupported=composite_nonseparable_srgb8_opacity(mode,ab,as,.5,"linear-srgb16");
        check(unsupported.status==BlendKernelStatus::unsupported_profile&&unsupported.value==PremultipliedSrgb8{},"Unsupported profile cannot become zero-pixel success");
    }
    std::cout<<"PASS "<<checks<<" nonseparable authored-opacity bridge checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
