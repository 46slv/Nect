#pragma once
#include "nect/blend.hpp"

namespace nect {
// Bounded straight encoded-sRGB binary64 arithmetic for exactly linear-burn,
// linear-dodge, linear-light, vivid-light, pin-light, hard-mix, subtract, divide,
// darker-color and lighter-color. These functions do not select a profile,
// compose alpha, quantize bytes or register product capabilities.
//
// Every channel must be finite and in [0,1]. Unknown IDs are refused before
// color validation. Failures return the corresponding status and canonical
// zero RGB. No Classic alias or epsilon/ULP correction is applied.
//
// Divide checks backdrop==0 first, so 0/0 is 0 and positive/0 is 1. Vivid Light
// uses modern Burn/Dodge endpoint priority; Hard Mix separately tests b+s>=1.
// Darker/Lighter Color select one complete input triplet by the represented
// binary64 total (R+G)+B, retaining backdrop on exact represented-total ties.
// This arbitrary normalized-RGB contract does not establish exact rational
// ordering of original premultiplied-byte ratios in a separate byte adapter.
BlendRgbResult blend_arithmetic_rgb(std::string_view mode,BlendRgb backdrop,BlendRgb source) noexcept;
}
