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

// Exact R09-D4 pixel-center run, already selected from an authenticated analysis.
// The helper retains only its endpoints as an open straight Path contour. It
// neither discovers lines nor guarantees source thickness/appearance.
struct AnalysisLineCandidate {
    std::string direction;
    Vec2 start;
    Vec2 end;
    int length_pixels = 0;
};
Contour adopt_analysis_thin_line(const AnalysisLineCandidate& line,
    const AnalysisContourFrame& frame, const Id& contour_id,
    const std::vector<Id>& point_ids);
}
