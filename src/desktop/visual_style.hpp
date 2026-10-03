#pragma once
#include <QString>

namespace nect::desktop {
// Existing desktop presentation values, without theme or preference state.
QString application_style_sheet();
QString utility_button_style_sheet();
QString utility_popover_style_sheet();

struct VisualMetrics {
    static constexpr int utility_strip_min_height=42;
    static constexpr int utility_strip_max_height=58;
    static constexpr int utility_icon_size=20;
    static constexpr int utility_toggle_width=36;
    static constexpr int utility_toggle_height=32;
    static constexpr int utility_zoom_width=92;
    static constexpr int utility_readback_min_width=185;
    static constexpr int utility_readback_max_width=270;
    static constexpr int utility_popover_margin=8;
    static constexpr int utility_popover_spacing=6;
};
}
