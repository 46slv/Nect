#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace nect {
// Fixed current production profile. Descriptor arithmetic is per renderer class;
// this is not a claim of After Effects fixture parity.
inline constexpr std::string_view nonseparable_blend_profile_id="nect.srgb8-premultiplied/v1";

struct BlendProfileDescriptor {
    std::string_view id;
    unsigned channel_bits;
    unsigned alpha_bits;
    std::string_view color_space;
    std::string_view alpha_representation;
    std::string_view intermediate_precision;
    std::string_view quantization;
};

struct BlendModeDescriptor {
    std::string_view id;
    std::string_view label;
    std::string_view family;
    unsigned behavior_version;
    std::string_view color_operation;
    std::string_view alpha_behavior;
    std::string_view backdrop_scope;
    std::string_view time_dependency;
    std::span<const BlendProfileDescriptor> profiles;
    std::string_view ae_oracle_status;
    unsigned introduced_native_minor;
    std::string_view renderer;
    std::string_view svg_representation;
    std::string_view svg_reader_requirement;
};

// Single immutable product registry. Stable old IDs/order are preserved.
std::span<const BlendModeDescriptor> blend_modes() noexcept;
const BlendModeDescriptor* find_blend_mode(std::string_view id) noexcept;
// Component views refer to the same four descriptors in the product registry.
std::span<const BlendModeDescriptor> nonseparable_blend_modes() noexcept;
const BlendModeDescriptor* find_nonseparable_blend_mode(std::string_view id) noexcept;

enum class BlendKernelStatus {
    ok,
    unsupported_blend,
    unsupported_profile,
    invalid_color,
    invalid_premultiplied_pixel,
    invalid_opacity
};
constexpr std::string_view blend_kernel_status_code(BlendKernelStatus status) noexcept {
    switch(status) {
    case BlendKernelStatus::ok:return "OK";
    case BlendKernelStatus::unsupported_blend:return "UNSUPPORTED_BLEND";
    case BlendKernelStatus::unsupported_profile:return "UNSUPPORTED_BLEND_PROFILE";
    case BlendKernelStatus::invalid_color:return "BLEND_COLOR_RANGE";
    case BlendKernelStatus::invalid_premultiplied_pixel:return "BLEND_PREMULTIPLIED_RANGE";
    case BlendKernelStatus::invalid_opacity:return "BLEND_OPACITY_RANGE";
    }
    return "UNSUPPORTED_BLEND_STATUS";
}

// Normalized, straight encoded-sRGB RGB used only as binary64 formula
// intermediates for the declared profile. All input channels must be finite
// and in [0,1]. No gamma decoding, ICC conversion or HSL-library substitution.
using BlendRgb=std::array<double,3>;
struct BlendRgbResult {
    BlendKernelStatus status=BlendKernelStatus::ok;
    BlendRgb value{};
};
BlendRgbResult blend_nonseparable_rgb(std::string_view mode,BlendRgb backdrop,BlendRgb source,
    std::string_view profile=nonseparable_blend_profile_id) noexcept;

// Logical RGBA order, independently of native pixel layout/endian packing.
// Inputs are encoded-sRGB premultiplied bytes: r,g,b <= a. Alpha zero therefore
// requires zero RGB; malformed inputs are refused, never silently repaired.
struct PremultipliedSrgb8 {
    std::uint8_t r=0,g=0,b=0,a=0;
    bool operator==(const PremultipliedSrgb8&) const=default;
};
struct BlendPixelResult {
    BlendKernelStatus status=BlendKernelStatus::ok;
    PremultipliedSrgb8 value{};
};

// Blend straight RGB, then apply source-over exactly once. Premultiplied RGB
// and alpha are rounded once from the represented binary64 result to the
// nearest nonnegative byte, ties upward;
// final RGB is clamped to rounded alpha. Valid transparent endpoints preserve
// the other pixel's exact bytes. No authored opacity, masks, isolation or
// backdrop traversal is applied here; the caller owns that boundary.
// Every caller must inspect status before consuming value. Failures return
// canonical zero value and never alias another mode or profile.
// Near a quantization boundary, formula rounding can differ by one byte from
// ideal exact-rational evaluation. No epsilon/ULP tie correction is applied.
BlendPixelResult composite_nonseparable_srgb8(std::string_view mode,PremultipliedSrgb8 backdrop,
    PremultipliedSrgb8 source,std::string_view profile=nonseparable_blend_profile_id) noexcept;
// Shared deterministic-color byte adapter. New arithmetic and four HSL modes
// use the same once-rounded source-over/opacity boundary. Whole-color byte
// ordering uses exact cross-multiplied original straight-byte totals.
BlendPixelResult composite_deterministic_srgb8_opacity(std::string_view mode,PremultipliedSrgb8 backdrop,
    PremultipliedSrgb8 source,double opacity,std::string_view profile=nonseparable_blend_profile_id) noexcept;
// Consumer opacity bridge: source straight RGB is recovered from the original
// premultiplied bytes; authored opacity scales effective source alpha in binary64.
// No rounded straight-RGB or opacity-scaled byte intermediate is introduced.
// Final premultiplied RGBA rounds once using the same kernel quantizer.
// Opacity 0 preserves exact backdrop, opacity 1 preserves the reviewed adapter.
BlendPixelResult composite_nonseparable_srgb8_opacity(std::string_view mode,PremultipliedSrgb8 backdrop,
    PremultipliedSrgb8 source,double opacity,std::string_view profile=nonseparable_blend_profile_id) noexcept;
}
