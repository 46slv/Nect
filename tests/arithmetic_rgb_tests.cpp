#include "nect/arithmetic_blend.hpp"
#include <array>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace nect;
namespace {
std::size_t checks=0;
constexpr std::array<std::string_view,10> modes{
    "linear-burn","linear-dodge","linear-light","vivid-light","pin-light",
    "hard-mix","subtract","divide","darker-color","lighter-color"
};
static_assert(noexcept(blend_arithmetic_rgb({},BlendRgb{},BlendRgb{})));
void check(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
    ++checks;
}
BlendRgb blend(std::string_view mode,BlendRgb backdrop,BlendRgb source) {
    const auto result=blend_arithmetic_rgb(mode,backdrop,source);
    check(result.status==BlendKernelStatus::ok,"Admitted RGB succeeds: "+std::string(mode));
    return result.value;
}
void exact_rgb(BlendRgb actual,BlendRgb expected,const std::string& message) {
    for(std::size_t i=0;i<actual.size();++i)
        check(actual[i]==expected[i],message+" channel "+std::to_string(i));
}
void canonical_failure(const BlendRgbResult& result,BlendKernelStatus status,const std::string& message) {
    check(result.status==status,message+" status");
    for(double channel:result.value)
        check(channel==0&&!std::signbit(channel),message+" canonical positive-zero RGB");
}

void independent_scalar_literals() {
    // Fixed values transcribed from scalar_cases in the frozen research packet
    // fixtures.json SHA256 e5c5287549b86df440d90f02012bbb73a5c8f7bfb1a70f6eeca8e850f04cc6ab.
    // Eleven dyadic input pairs per scalar mode, including all four endpoints.
    // These 88 expectations compare exactly. The ideal-rational non-dyadic
    // rows are not relabeled as represented binary64 equality or given a
    // tolerance. No reference blend function or generator runs in this test.
    struct Fixture {double b,s;std::array<double,8> expected;};
    constexpr std::array<Fixture,11> fixtures{{
        {0,0,{0,0,0,0,0,0,0,0}},
        {1,0,{0,1,0,1,0,1,1,1}},
        {0,1,{0,1,1,0,1,1,0,0}},
        {1,1,{1,1,1,1,1,1,0,1}},
        {.5,.5,{0,1,.5,.5,.5,1,0,1}},
        {.25,.25,{0,.5,0,0,.25,0,0,1}},
        {.75,.25,{0,1,.25,.5,.5,1,.5,1}},
        {.25,.75,{0,1,.75,.5,.5,1,0,1./3}},
        {.75,.75,{.5,1,1,1,.75,1,0,1}},
        {.375,.25,{0,.625,0,0,.375,0,.125,1}},
        {.375,.75,{.125,1,.875,.75,.5,1,0,.5}}
    }};
    for(const auto& fixture:fixtures)for(std::size_t i=0;i<8;++i) {
        const double expected=fixture.expected[i];
        exact_rgb(blend(modes[i],{fixture.b,fixture.b,fixture.b},{fixture.s,fixture.s,fixture.s}),
            {expected,expected,expected},"Independent scalar literal "+std::string(modes[i]));
    }
    // Batch distinct fixed rows across channels: accidental grayscale-only or
    // cross-channel implementations cannot satisfy these eight triplets.
    for(std::size_t i=0;i<8;++i)
        exact_rgb(blend(modes[i],{.75,.25,.375},{.25,.75,.75}),
            {fixtures[6].expected[i],fixtures[7].expected[i],fixtures[10].expected[i]},
            "Independent channelwise literal "+std::string(modes[i]));
}

void represented_boundaries_and_priority() {
    // Named represented-input checks derived independently from the packet's
    // written binary64 expression/order. Hex literals preserve its output;
    // they deliberately do not substitute ideal 1/255 or epsilon-repair it.
    const BlendRgb lower{254./255,254./255,254./255},tiny_byte{1./255,1./255,1./255};
    exact_rgb(blend("linear-light",lower,tiny_byte),
        {0x1.0101010101000p-8,0x1.0101010101000p-8,0x1.0101010101000p-8},
        "Represented Linear Light lower byte-ratio expression");
    exact_rgb(blend("linear-light",tiny_byte,lower),
        {0x1.fdfdfdfdfdfe0p-1,0x1.fdfdfdfdfdfe0p-1,0x1.fdfdfdfdfdfe0p-1},
        "Represented Linear Light upper byte-ratio expression");
    exact_rgb(blend("pin-light",tiny_byte,lower),
        {0x1.fbfbfbfbfbfc0p-1,0x1.fbfbfbfbfbfc0p-1,0x1.fbfbfbfbfbfc0p-1},
        "Represented Pin Light upper byte-ratio expression");

    // Packet sum-boundary fixture: below, exact equality, above, respectively.
    exact_rgb(blend("hard-mix",{1./255,127./255,128./255},{253./255,128./255,128./255}),
        {0,1,1},"Hard Mix direct sum threshold");
    exact_rgb(blend("hard-mix",{0,1,.5},{1,0,.5}),{1,1,1},"Hard Mix includes equality");
    exact_rgb(blend("vivid-light",{0,1,.5},{1,0,.5}),{0,1,.5},"Vivid has distinct modern endpoint priority");
    exact_rgb(blend("divide",{0,.25,1},{0,0,0}),{0,1,1},"Divide zero numerator has priority");

    // Exact binary64 values adjacent to source 1/2: source chooses the branch.
    const double half_below=std::nextafter(.5,0.),half_above=std::nextafter(.5,1.);
    exact_rgb(blend("pin-light",{1,0,.375},{half_below,half_above,.5}),
        {0x1.fffffffffffffp-1,0x1p-52,.375},"Pin Light source-half neighboring branches");
    exact_rgb(blend("vivid-light",{1,0,.375},{half_below,half_above,.5}),
        {1,0,.375},"Vivid Light source-half neighbors retain endpoint priority");

    std::feclearexcept(FE_INVALID|FE_DIVBYZERO);
    exact_rgb(blend("vivid-light",{0,1,0},{1,0,0}),{0,1,0},"No singular Vivid division is evaluated");
    exact_rgb(blend("divide",{0,1,0},{0,0,1}),{0,1,0},"No singular Divide division is evaluated");
    check(std::fetestexcept(FE_INVALID|FE_DIVBYZERO)==0,"Endpoints avoid invalid/zero-divisor evaluation");
    const double t=std::numeric_limits<double>::denorm_min();
    exact_rgb(blend("divide",{1,t,0},{t,t,t}),{1,1,0},"Divide saturates subnormal-ratio overflow");
    exact_rgb(blend("vivid-light",{.5,1,0},{t,t,t}),{0,1,0},"Burn saturates subnormal-ratio overflow");
}

void independent_whole_triplets() {
    // Fixed whole-color metric, tie and unpremultiply-order research fixtures.
    // Expectations select one full triplet by total straight channels, never
    // weighted luma or per-channel darken/lighten.
    const BlendRgb red{204./255,0,0},green{0,153./255,0};
    exact_rgb(blend("darker-color",red,green),green,"Darker Color total metric");
    exact_rgb(blend("lighter-color",red,green),red,"Lighter Color total metric");
    const BlendRgb primary_red{1,0,0},primary_green{0,1,0};
    for(auto mode:{"darker-color","lighter-color"}) {
        exact_rgb(blend(mode,primary_red,primary_green),primary_red,"Equal total retains backdrop");
        exact_rgb(blend(mode,primary_green,primary_red),primary_green,"Equal total reversed retains backdrop");
    }
    const BlendRgb unpremultiplied_b{20./32,0,0},unpremultiplied_s{0,40./128,0};
    exact_rgb(blend("darker-color",unpremultiplied_b,unpremultiplied_s),unpremultiplied_s,
        "Darker Color compares straight normalized inputs");
    exact_rgb(blend("lighter-color",unpremultiplied_b,unpremultiplied_s),unpremultiplied_b,
        "Lighter Color compares straight normalized inputs");

    // Whole-color byte-order fixture has equal *rational* original-byte totals
    // but unequal represented normalized sums. This helper intentionally picks
    // source for Darker and backdrop for Lighter. Root's exact byte adapter is
    // a separate boundary; this test makes no byte-composition parity claim.
    const BlendRgb ratio_b{50./70,0,0},ratio_s{10./70,40./70,0};
    exact_rgb(blend("darker-color",ratio_b,ratio_s),ratio_s,"Normalized represented sum is lower");
    exact_rgb(blend("lighter-color",ratio_b,ratio_s),ratio_b,"Normalized represented sum is higher");

    // Left-to-right represented addition loses each half-ULP separately;
    // reassociation would change ordering. Exact represented ties retain b.
    const BlendRgb addition_order_b{1,0x1p-53,0x1p-53},addition_order_s{1,0,0};
    for(auto mode:{"darker-color","lighter-color"})
        exact_rgb(blend(mode,addition_order_b,addition_order_s),addition_order_b,
            "Whole-color (R+G)+B order and represented tie");
}

void invalid_inputs_and_exact_support() {
    const double nan=std::numeric_limits<double>::quiet_NaN();
    const std::array<double,6> invalid{
        nan,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::denorm_min(),std::nextafter(1.,2.),-1.
    };
    for(auto mode:modes)for(double bad:invalid)for(std::size_t channel=0;channel<3;++channel) {
        BlendRgb malformed{.25,.5,.75};malformed[channel]=bad;
        canonical_failure(blend_arithmetic_rgb(mode,malformed,{}),BlendKernelStatus::invalid_color,
            "Invalid backdrop before any color shortcut");
        canonical_failure(blend_arithmetic_rgb(mode,{},malformed),BlendKernelStatus::invalid_color,
            "Invalid source before any color shortcut");
    }
    for(auto mode:{"","normal","hue","color-burn","color-dodge","difference","plus-lighter",
        "Linear-Burn","linear_burn","linear-burn ","classic-color-burn","classic-color-dodge","classic-difference"}) {
        canonical_failure(blend_arithmetic_rgb(mode,{.25,.5,.75},{.75,.5,.25}),
            BlendKernelStatus::unsupported_blend,"Only ten exact IDs admitted");
        canonical_failure(blend_arithmetic_rgb(mode,{nan,0,0},{}),
            BlendKernelStatus::unsupported_blend,"Support failure precedes invalid color");
    }
    for(auto mode:modes) {
        const auto result=blend_arithmetic_rgb(mode,{-0.,0,1},{0,-0.,1});
        check(result.status==BlendKernelStatus::ok,"Signed zero is a valid normalized input");
    }
}

void bounded_invariants() {
    const double t=std::numeric_limits<double>::denorm_min();
    const std::array<double,15> levels{0,t,2*t,std::numeric_limits<double>::min(),
        1./255,.125,.25,.375,std::nextafter(.5,0.),.5,std::nextafter(.5,1.),
        .75,254./255,std::nextafter(1.,0.),1};
    for(std::size_t b=0;b<levels.size();++b)for(std::size_t s=0;s<levels.size();++s) {
        const BlendRgb backdrop{levels[b],levels[(b+4)%levels.size()],levels[(b+9)%levels.size()]};
        const BlendRgb source{levels[s],levels[(s+7)%levels.size()],levels[(s+11)%levels.size()]};
        for(auto mode:modes) {
            const auto actual=blend(mode,backdrop,source);
            for(double channel:actual)
                check(std::isfinite(channel)&&channel>=0&&channel<=1,"Admitted output stays finite and bounded");
            exact_rgb(blend(mode,backdrop,source),actual,"Same represented inputs are deterministic");
            if(mode=="darker-color"||mode=="lighter-color")
                check(actual==backdrop||actual==source,"Whole-color operation returns an entire candidate triplet");
        }
        for(auto mode:{"linear-dodge","subtract","divide"}) {
            const BlendRgb identity_source=mode==std::string_view("divide")?BlendRgb{1,1,1}:BlendRgb{};
            exact_rgb(blend(mode,backdrop,identity_source),backdrop,"Exact source identity");
        }
        for(auto mode:{"vivid-light","pin-light"})
            exact_rgb(blend(mode,backdrop,{.5,.5,.5}),backdrop,"Exact source-half identity");
    }
}
}

int main() {
    try {
        independent_scalar_literals();
        represented_boundaries_and_priority();
        independent_whole_triplets();
        invalid_inputs_and_exact_support();
        bounded_invariants();
        std::cout<<"arithmetic_rgb_tests: "<<checks<<" checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"arithmetic_rgb_tests: "<<error.what()<<'\n';
        return 1;
    }
}
