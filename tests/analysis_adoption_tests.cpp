#include "nect/analysis_adoption.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace nect;
namespace {
int checks = 0;
void check(bool ok) { ++checks; if (!ok) throw std::runtime_error("analysis adoption check failed"); }
template<class F> void rejects(const char* code, F&& f) {
    try { f(); } catch (const Error& e) { check(e.code == code); return; }
    throw std::runtime_error("expected refusal");
}
}
int main() {
    const std::vector<Vec2> corners{{0,0},{8,0},{8,6},{0,6}};
    const std::vector<Id> ids{"point-a","point-b","point-c","point-d"};
    const AnalysisContourFrame frame{{-10,20},2,8,6};
    const auto result = adopt_analysis_outer_contour(corners,frame,"outline",ids);
    check(result.closed && result.id == "outline" && result.points.size() == 4);
    for (std::size_t i=0; i<4; ++i) {
        const auto& p=result.points[i];
        check(p.id == ids[i]);
        check(p.x.literal == -10 + corners[i].x/2 && p.y.literal == 20 + corners[i].y/2);
        check(!p.x.binding && !p.y.binding && !p.x.expression && !p.y.expression);
        check(p.in_length.literal == 0 && p.out_length.literal == 0);
    }
    // Pixel corners are not centers, and fractional raster extents are not
    // clamped back to the nominal Artboard rectangle.
    auto fractional=frame; fractional.scale=1.5;
    const auto f=adopt_analysis_outer_contour(corners,fractional,"other",ids);
    check(f.points[1].x.literal == -10 + 8/1.5);
    auto bad=frame; bad.scale=0;
    rejects("EXPORT_SCALE",[&]{adopt_analysis_outer_contour(corners,bad,"c",ids);});
    bad.scale=17;
    rejects("EXPORT_SCALE",[&]{adopt_analysis_outer_contour(corners,bad,"c",ids);});
    bad.scale=std::numeric_limits<double>::quiet_NaN();
    rejects("EXPORT_SCALE",[&]{adopt_analysis_outer_contour(corners,bad,"c",ids);});
    bad=frame; bad.origin.x=std::numeric_limits<double>::infinity();
    rejects("INVALID_ANALYSIS_FRAME",[&]{adopt_analysis_outer_contour(corners,bad,"c",ids);});
    bad=frame; bad.pixel_width=0;
    rejects("INVALID_ANALYSIS_FRAME",[&]{adopt_analysis_outer_contour(corners,bad,"c",ids);});
    bad=frame; bad.pixel_width=2001;bad.pixel_height=2000;
    rejects("INVALID_ANALYSIS_FRAME",[&]{adopt_analysis_outer_contour(corners,bad,"c",ids);});
    auto vertices=corners; vertices[0].x=.5;
    rejects("INVALID_ANALYSIS_CONTOUR",[&]{adopt_analysis_outer_contour(vertices,frame,"c",ids);});
    vertices=corners;vertices[1].x=9;
    rejects("INVALID_ANALYSIS_CONTOUR",[&]{adopt_analysis_outer_contour(vertices,frame,"c",ids);});
    vertices=corners;vertices[1]=vertices[0];
    rejects("INVALID_ANALYSIS_CONTOUR",[&]{adopt_analysis_outer_contour(vertices,frame,"c",ids);});
    vertices=corners;std::reverse(vertices.begin(),vertices.end());
    rejects("INVALID_ANALYSIS_CONTOUR",[&]{adopt_analysis_outer_contour(vertices,frame,"c",ids);});
    vertices=corners;vertices[1].y=1;
    rejects("INVALID_ANALYSIS_CONTOUR",[&]{adopt_analysis_outer_contour(vertices,frame,"c",ids);});
    rejects("INVALID_ANALYSIS_IDS",[&]{adopt_analysis_outer_contour(corners,frame,"c",{});});
    auto duplicate=ids;duplicate[1]=duplicate[0];
    rejects("DUPLICATE_ID",[&]{adopt_analysis_outer_contour(corners,frame,"c",duplicate);});
    rejects("DUPLICATE_ID",[&]{adopt_analysis_outer_contour(corners,frame,ids[0],ids);});
    rejects("INVALID_ID",[&]{adopt_analysis_outer_contour(corners,frame,"invalid id",ids);});
    rejects("ANALYSIS_LIMIT",[&]{adopt_analysis_outer_contour({},frame,"c",{});});
    vertices.assign(10001,{0,0});
    rejects("ANALYSIS_LIMIT",[&]{adopt_analysis_outer_contour(vertices,frame,"c",{});});
    bad=frame;bad.scale=std::numeric_limits<double>::denorm_min();
    rejects("NON_FINITE",[&]{adopt_analysis_outer_contour(corners,bad,"c",ids);});
    std::cout << checks << " checks passed\n";
}
