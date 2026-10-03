#pragma once
#include "nect/core.hpp"

namespace nect {
// Converts one already-selected analysis outer boundary. The caller authenticates
// its snapshot and allocates fresh Document identities before CreatePath. This
// does not trace curves, fill holes, simplify vertices, or mutate a Session.
struct AnalysisContourFrame {
    Vec2 origin;
    double scale = 1;
    int pixel_width = 0;
    int pixel_height = 0;
};
Contour adopt_analysis_outer_contour(const std::vector<Vec2>& pixel_corners,
    const AnalysisContourFrame& frame, const Id& contour_id,
    const std::vector<Id>& point_ids);
}
