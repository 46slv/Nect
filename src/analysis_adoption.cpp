#include "nect/analysis_adoption.hpp"
#include <cmath>
#include <set>

namespace nect {
namespace {
void require_adoption(bool valid, const char* code, const char* message) {
    if (!valid) throw Error(code, message);
}
bool valid_identity(const Id& id) {
    if (id.empty() || id.size() > 96) return false;
    for (const unsigned char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
}
Contour adopt_analysis_outer_contour(const std::vector<Vec2>& vertices,
    const AnalysisContourFrame& frame, const Id& contour_id,
    const std::vector<Id>& point_ids) {
    require_adoption(vertices.size() >= 4 && vertices.size() <= 10000,
        "ANALYSIS_LIMIT", "Adoption requires 4..10000 outer contour corners");
    require_adoption(std::isfinite(frame.scale) && frame.scale > 0 && frame.scale <= 16,
        "EXPORT_SCALE", "Analysis scale must be greater than zero and at most 16");
    require_adoption(std::isfinite(frame.origin.x) && std::isfinite(frame.origin.y) &&
        frame.pixel_width > 0 && frame.pixel_width <= 8192 &&
        frame.pixel_height > 0 && frame.pixel_height <= 8192 &&
        static_cast<long long>(frame.pixel_width) * frame.pixel_height <= 4000000,
        "INVALID_ANALYSIS_FRAME", "Invalid bounded analysis output frame");
    require_adoption(point_ids.size() == vertices.size(), "INVALID_ANALYSIS_IDS",
        "Each retained corner needs one fresh authored point identity");
    std::set<Id> ids;
    auto identity = [&](const Id& id) {
        require_adoption(valid_identity(id), "INVALID_ID", "Invalid authored analysis identity");
        require_adoption(ids.insert(id).second, "DUPLICATE_ID", "Adopted identities must be unique");
    };
    identity(contour_id);
    for (const auto& id : point_ids) identity(id);
    for (const auto& vertex : vertices)
        require_adoption(std::isfinite(vertex.x) && std::isfinite(vertex.y) &&
            vertex.x == std::floor(vertex.x) && vertex.y == std::floor(vertex.y) &&
            vertex.x >= 0 && vertex.x <= frame.pixel_width &&
            vertex.y >= 0 && vertex.y <= frame.pixel_height,
            "INVALID_ANALYSIS_CONTOUR", "Expected bounded integer pixel-edge corners");
    long double twice_area = 0;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto& a = vertices[i];
        const auto& b = vertices[(i + 1) % vertices.size()];
        require_adoption((a.x == b.x) != (a.y == b.y), "INVALID_ANALYSIS_CONTOUR",
            "Outer contour must have nonzero axis-aligned edges and implicit closure");
        twice_area += static_cast<long double>(a.x) * b.y - static_cast<long double>(b.x) * a.y;
    }
    require_adoption(twice_area > 0, "INVALID_ANALYSIS_CONTOUR",
        "Expected foreground-right clockwise outer boundary, not a hole");
    Contour result;
    result.id = contour_id;
    result.closed = true;
    result.points.reserve(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        Point point;
        point.id = point_ids[i];
        point.x.literal = frame.origin.x + vertices[i].x / frame.scale;
        point.y.literal = frame.origin.y + vertices[i].y / frame.scale;
        require_adoption(std::isfinite(point.x.literal) && std::isfinite(point.y.literal),
            "NON_FINITE", "Analysis corner conversion overflowed Composition coordinates");
        result.points.push_back(std::move(point));
    }
    return result;
}
}
