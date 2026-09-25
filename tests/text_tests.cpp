#include "nect/core.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace nect;
namespace {
int checks=0;
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);++checks;}
void near(double actual,double expected,const char* message,double tolerance=0.002) {
    if(std::abs(actual-expected)>tolerance)throw std::runtime_error(std::string(message)+": "+std::to_string(actual)+" != "+std::to_string(expected));
    ++checks;
}
std::map<std::string,double> values(const TextSource& source) {
    std::map<std::string,double> result;
    for(const auto& [name,scalar]:source.parameters)result.emplace(name,scalar.literal);
    return result;
}
bool warning(const TextLayout& layout,const std::string& code) {
    return std::any_of(layout.warnings.begin(),layout.warnings.end(),[&](const auto& text){return text.starts_with(code);});
}
struct Bounds {
    double left=std::numeric_limits<double>::infinity(),top=left,right=-left,bottom=-left;
    double width()const{return right-left;}double height()const{return bottom-top;}
};
Bounds bounds(const TextLayout& layout) {
    check(layout.contours!=nullptr,"Projection always returns an immutable contour payload");
    Bounds result;
    for(const auto& contour:*layout.contours)for(const auto& point:contour.points)for(const auto value:{point.anchor,point.incoming,point.outgoing}) {
        check(std::isfinite(value.x)&&std::isfinite(value.y),"Every emitted outline coordinate is finite");
        result.left=std::min(result.left,value.x);result.right=std::max(result.right,value.x);
        result.top=std::min(result.top,value.y);result.bottom=std::max(result.bottom,value.y);
    }
    return result;
}
template<class F>void rejects(const char* code,F action) {
    try{action();}catch(const Error& error){check(error.code==code,(std::string("Expected ")+code+", got "+error.code).c_str());return;}
    throw std::runtime_error(std::string("Expected rejection: ")+code);
}
}

int main() {
    try {
        auto source=default_text("text-source","日本語 ABC");
        check(source.id=="text-source"&&source.parameters.size()==7&&source.parameters.at("font_size").literal==48,
            "Default text creates the canonical authored parameters");
#ifndef _WIN32
        rejects("TEXT_PLATFORM_UNSUPPORTED",[&]{evaluate_text(source,values(source));});
        rejects("TEXT_PLATFORM_UNSUPPORTED",[&]{text_fonts();});
#else
        const auto fonts=text_fonts();
        check(!fonts.empty()&&std::is_sorted(fonts.begin(),fonts.end())&&std::adjacent_find(fonts.begin(),fonts.end())==fonts.end(),
            "Installed font discovery returns sorted unique family names");
        const auto mixed=evaluate_text(source,values(source));const auto mixed_bounds=bounds(mixed);
        check(mixed.glyph_count>=6&&!mixed.contours->empty()&&!mixed.used_fonts.empty(),
            "Japanese and Latin text uses shaped glyph outlines and reports actual installed families");
        check(mixed.first_line_baseline_y.has_value(),
            "Horizontal text exposes its measured first-line baseline independently of glyph bounds");
        check(mixed.width>mixed.height&&mixed_bounds.width()>100&&!mixed.overflow,
            "Automatic horizontal text measures its actual line instead of a large layout container");
        auto shifted_values=values(source);shifted_values["origin_x"]=123.25;shifted_values["origin_y"]=-17.5;
        const auto shifted=evaluate_text(source,shifted_values);
        near(shifted.x,123.25,"Layout retains authored origin X");near(shifted.y,-17.5,"Layout retains authored origin Y");
        near(*shifted.first_line_baseline_y-*mixed.first_line_baseline_y,-17.5,
            "First-line baseline follows the authored vertical origin");
        check(shifted.contours->size()==mixed.contours->size(),"Moving text does not change its shaped outline topology");
        for(std::size_t c=0;c<mixed.contours->size();++c) {
            check(mixed.contours->at(c).closed&&mixed.contours->at(c).points.size()==shifted.contours->at(c).points.size(),
                "Glyph contours remain closed and contain matching cubic points");
            for(std::size_t p=0;p<mixed.contours->at(c).points.size();++p) {
                near(shifted.contours->at(c).points[p].anchor.x-mixed.contours->at(c).points[p].anchor.x,123.25,"Origin shifts every anchor X");
                near(shifted.contours->at(c).points[p].incoming.y-mixed.contours->at(c).points[p].incoming.y,-17.5,"Origin shifts every cubic handle Y");
            }
        }

        auto cjk=default_text("cjk","上");const auto horizontal_cjk=evaluate_text(cjk,values(cjk));const auto upright=bounds(horizontal_cjk);
        cjk.direction="vertical";const auto vertical_cjk=evaluate_text(cjk,values(cjk));const auto upright_vertical=bounds(vertical_cjk);
        check(!vertical_cjk.first_line_baseline_y.has_value(),
            "Vertical text does not expose a horizontal-baseline snap metric");
        near(upright_vertical.width(),upright.width(),"CJK glyphs stay upright in Japanese vertical text");
        near(upright_vertical.height(),upright.height(),"CJK vertical shape keeps its upright proportions");
        check(horizontal_cjk.contours->size()==vertical_cjk.contours->size(),"Upright CJK uses matching outline contours");
        for(std::size_t c=0;c<horizontal_cjk.contours->size();++c) {
            const auto& horizontal_points=horizontal_cjk.contours->at(c).points;
            const auto& vertical_points=vertical_cjk.contours->at(c).points;
            check(horizontal_points.size()==vertical_points.size(),"Upright CJK keeps the same cubic outline topology");
            for(std::size_t p=0;p<horizontal_points.size();++p) {
                near(vertical_points[p].anchor.x-upright_vertical.left,horizontal_points[p].anchor.x-upright.left,"CJK upright orientation preserves normalized X");
                near(vertical_points[p].anchor.y-upright_vertical.top,horizontal_points[p].anchor.y-upright.top,"CJK upright orientation preserves normalized Y");
            }
        }
        check(upright_vertical.left>-5&&upright_vertical.right<vertical_cjk.width+5&&upright_vertical.top>-5&&upright_vertical.bottom<vertical_cjk.height+5,
            "Automatic vertical glyph positions use the measured container, not a distant right edge");
        auto latin=default_text("latin","F");
        if(std::binary_search(fonts.begin(),fonts.end(),"Arial"))latin.family="Arial";
        const auto latin_horizontal=bounds(evaluate_text(latin,values(latin)));latin.direction="vertical";
        const auto latin_vertical=bounds(evaluate_text(latin,values(latin)));
        near(latin_vertical.width(),latin_horizontal.height(),"Latin glyphs turn sideways on a vertical baseline");
        near(latin_vertical.height(),latin_horizontal.width(),"Latin sideways orientation comes from DirectWrite's run orientation");

        auto tracked=default_text("tracked","口口口口");const auto normal=evaluate_text(tracked,values(tracked));
        tracked.parameters.at("tracking").literal=6;const auto spaced=evaluate_text(tracked,values(tracked));
        check(spaced.width>normal.width+15,"Tracking increases shaped horizontal advances");
        tracked.direction="vertical";const auto vertical_spaced=evaluate_text(tracked,values(tracked));
        tracked.parameters.at("tracking").literal=0;const auto vertical_normal=evaluate_text(tracked,values(tracked));
        check(vertical_spaced.height>vertical_normal.height+15,"Tracking follows the vertical reading axis");
        auto lines=default_text("lines","H\nH");lines.family=latin.family;lines.parameters.at("line_spacing").literal=80;
        const auto uniform=evaluate_text(lines,values(lines));
        check(uniform.line_baselines_y.size()==2,"Two horizontal lines expose two measured baselines");
        near(uniform.line_baselines_y[1]-uniform.line_baselines_y[0],80,
            "Second-line baseline follows DirectWrite uniform line spacing");
        check(uniform.contours->size()>=2&&uniform.contours->size()%2==0,"Repeated letters retain matching outlines on both lines");
        const auto half=uniform.contours->size()/2;
        near(uniform.contours->at(half).points.front().anchor.y-uniform.contours->front().points.front().anchor.y,80,
            "Uniform line spacing sets the actual distance between baselines");

        auto paragraph=default_text("paragraph","");for(int i=0;i<30;++i)paragraph.content+="日本";
        const auto auto_paragraph=evaluate_text(paragraph,values(paragraph));paragraph.layout="frame";
        paragraph.parameters.at("frame_width").literal=96;paragraph.parameters.at("frame_height").literal=48;
        const auto overset=evaluate_text(paragraph,values(paragraph));const auto overset_bounds=bounds(overset);
        check(overset.overflow&&warning(overset,"TEXT_OVERFLOW:")&&overset.glyph_count==auto_paragraph.glyph_count,
            "Small wrapped frame reports overflow and retains every shaped glyph");
        near(overset.width,96,"Frame width remains authored");near(overset.height,48,"Frame height remains authored");
        check(overset_bounds.bottom>overset.height*2,"Overflow outlines remain present below the frame");
        paragraph.direction="vertical";paragraph.parameters.at("frame_width").literal=48;paragraph.parameters.at("frame_height").literal=96;
        const auto vertical_overset=evaluate_text(paragraph,values(paragraph));
        check(vertical_overset.overflow&&vertical_overset.glyph_count==auto_paragraph.glyph_count&&bounds(vertical_overset).left<0,
            "Vertical overflow keeps additional right-to-left columns beyond the frame");
        auto aligned=default_text("aligned","ABC");aligned.layout="frame";aligned.family=latin.family;
        const auto leading=bounds(evaluate_text(aligned,values(aligned)));aligned.alignment="end";
        const auto trailing=bounds(evaluate_text(aligned,values(aligned)));
        check(trailing.left>leading.left+150,"End alignment uses the actual frame width");

        auto missing=source;missing.family="Nect Deliberately Missing Family 6D1284";
        const auto fallback=evaluate_text(missing,values(missing));
        check(warning(fallback,"MISSING_FONT:")&&warning(fallback,"FONT_FALLBACK:")&&!fallback.used_fonts.empty()&&
            missing.family=="Nect Deliberately Missing Family 6D1284","Missing family falls back visibly without rewriting authored font intent");
        auto arabic=default_text("complex","سلام");arabic.locale="ar-SA";
        if(std::binary_search(fonts.begin(),fonts.end(),"Segoe UI"))arabic.family="Segoe UI";
        const auto shaped=evaluate_text(arabic,values(arabic));
        double isolated_width=0;
        for(const auto* letter:{"س","ل","ا","م"}) {
            auto isolated=arabic;isolated.content=letter;const auto result=evaluate_text(isolated,values(isolated));
            isolated_width+=result.width;
        }
        // DirectWrite can retain a blank glyph slot for a character absorbed by
        // a ligature, and glyphs can share the same number of contours. The
        // joined advance must differ substantially from per-character layout.
        check(shaped.glyph_count>0&&!shaped.contours->empty()&&shaped.width<isolated_width*0.85,
            "Complex Arabic shaping joins the word instead of placing isolated character glyphs");
        auto empty=default_text("empty","");const auto empty_layout=evaluate_text(empty,values(empty));
        check(empty_layout.glyph_count==0&&empty_layout.contours->empty(),"Empty text is a valid empty layout");
        empty.content=" \t　";const auto spaces=evaluate_text(empty,values(empty));
        check(spaces.contours->empty()&&spaces.width>0,"Whitespace and ideographic spaces retain advances without false outline errors");
        if(std::binary_search(fonts.begin(),fonts.end(),"Segoe UI Emoji")) {
            auto emoji=default_text("emoji","A😀B");emoji.family="Segoe UI Emoji";
            try {
                const auto projected=evaluate_text(emoji,values(emoji));
                check(!projected.contours->empty()&&warning(projected,"COLOR_GLYPH_MONOCHROME:"),
                    "Supported color-font base glyphs explicitly report monochrome projection");
            }catch(const Error& error) {
                check(error.code=="TEXT_COLOR_UNSUPPORTED","Unsupported color-only glyphs reject explicitly rather than dropping part of a mixed run");
            }
        }
        auto invalid=values(source);invalid["frame_width"]=-1;
        rejects("TEXT_PARAMETER",[&]{evaluate_text(source,invalid);});
        auto too_large=default_text("limit",std::string(32769,'A'));
        rejects("LIMIT",[&]{evaluate_text(too_large,values(too_large));});
        auto outlines=default_text("outline-limit",std::string(16000,'@'));outlines.family=latin.family;
        rejects("TEXT_OUTLINE_LIMIT",[&]{evaluate_text(outlines,values(outlines));});
#endif
        std::cout<<"PASS "<<checks<<" DirectWrite text projection checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
