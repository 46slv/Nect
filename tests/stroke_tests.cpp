#include "nect/io.hpp"
#include <iostream>
#include <cmath>
using namespace nect;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,"Unexpected rejection code");return;}throw std::runtime_error("Expected rejection");}
Document fixture(){
    auto d=empty_document("doc","comp","board");Object o;o.id="line";o.name="Line";
    Point a,b;a.id="a";b.id="b";a.x.literal=10;b.x.literal=100;
    o.contours={{"contour",false,{a,b}}};o.stack={default_operation("stroke","nect.paint.stroke")};o.legacy_stroke="stroke";
    d.objects.emplace(o.id,o);d.compositions[0].roots={o.id};return d;
}
void apply(Session& s,const std::vector<Command>& commands){s.apply(commands,s.revision());}
void atomic(Session& s,const char* code,const Command& command){const auto d=s.document();const auto h=s.history();const auto r=s.revision();rejects(code,[&]{apply(s,{command});});check(s.document()==d&&s.history()==h&&s.revision()==r,"Failure must preserve authored state and History");}
}
int main(){try{
    Session s(fixture());const auto old=encode(s.document());check(old.find("line_cap")==std::string::npos,"v1 encoding is unchanged");
    const auto old_paint=evaluate_shape(s.document(),"line",evaluate(s.document())).paints.front();
    check(old_paint.line_cap=="butt"&&old_paint.line_join=="miter"&&old_paint.miter_limit==4,"v1 paint defaults unchanged");
    apply(s,{StrokeStyle{"line","stroke","round","bevel",7}});const auto styled=s.document();
    const auto paint=evaluate_shape(styled,"line",evaluate(styled)).paints.front();
    check(paint.line_cap=="round"&&paint.line_join=="bevel"&&paint.miter_limit==7,"Style reaches shared evaluated paint");
    check(evaluate(styled).at({"line","","stroke.miter_limit"})==7,"Legacy Stroke alias uses miter range, not color range");
    check(styled.objects.at("line").contours==fixture().objects.at("line").contours,"Style does not rewrite source geometry");
    check(decode(encode(styled))==styled,"Stroke v2 native exact roundtrip");
    const auto svg=export_svg(styled,"comp","board");
    check(svg.find("stroke-linecap=\"round\"")!=std::string::npos&&svg.find("stroke-linejoin=\"bevel\"")!=std::string::npos&&svg.find("stroke-miterlimit=\"7\"")!=std::string::npos,"SVG emits evaluated stroke style");
    s.undo(s.revision());check(encode(s.document())==old,"Style promotion Undo restores v1 exactly");s.redo(s.revision());check(s.document()==styled,"Redo restores version and style");
    atomic(s,"INVALID_OPERATOR_OPTIONS",StrokeStyle{"line","stroke","triangle","miter",4});
    atomic(s,"INVALID_OPERATOR_OPTIONS",StrokeStyle{"line","stroke","butt","arcs",4});
    atomic(s,"OUT_OF_RANGE",StrokeStyle{"line","stroke","butt","miter",.5});
    atomic(s,"OUT_OF_RANGE",StrokeStyle{"line","stroke","butt","miter",1001});
    const auto limit=operation_ref("line","stroke","miter_limit");
    apply(s,{Link{limit,{{"line","","transform.a"},7,0,"copy_local_value"}}});
    apply(s,{StrokeStyle{"line","stroke","square","round",7}});
    check(property(s.document(),limit).binding.has_value(),"Unchanged driven miter limit stays linked");
    atomic(s,"DRIVEN_PROPERTY",StrokeStyle{"line","stroke","butt","miter",8});
    apply(s,{Set{{"line","","transform.a"},2}});check(evaluate_shape(s.document(),"line",evaluate(s.document())).paints.front().miter_limit==14,"Miter limit remains a live property");
    auto bad=styled;bad.objects.at("line").stack[0].version=3;rejects("UNSUPPORTED_OPERATOR_VERSION",[&]{validate(bad);});
    bad=fixture();bad.objects.at("line").stack[0].line_cap="round";rejects("INVALID_OPERATOR_OPTIONS",[&]{validate(bad);});
    bad=fixture();bad.objects.at("line").stack[0]=default_operation("fill","nect.paint.fill");bad.objects.at("line").legacy_stroke.clear();Session filled(bad);atomic(filled,"INVALID_DOMAIN",StrokeStyle{"line","fill","round","miter",4});
    auto dots=fixture();auto& c=dots.objects.at("line").contours.front();c.points[1].x.literal=c.points[0].x.literal;
    Session dot(dots);apply(dot,{StrokeStyle{"line","stroke","round","miter",4}});
    check(evaluate_shape(dot.document(),"line",evaluate(dot.document())).paints.front().degenerate_subpaths.size()==1,"Zero-length segment exposes cap geometry");
    dots=dot.document();dots.objects.at("line").contours.front().points.resize(1);Session move_only(dots);
    check(evaluate_shape(move_only.document(),"line",evaluate(move_only.document())).paints.front().degenerate_subpaths.empty(),"A move-only subpath has no stroked segment");
    dots.objects.at("line").contours.front().closed=true;Session closed_dot(dots);
    check(evaluate_shape(closed_dot.document(),"line",evaluate(closed_dot.document())).paints.front().degenerate_subpaths.size()==1,"Zero-length closing segment exposes a cap");
    Session api(fixture());const auto response=request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"stroke_style","object":"line","operation":"stroke","line_cap":"square","line_join":"round","miter_limit":5}]})");
    check(response.find("\"ok\":true")!=std::string::npos&&api.document().objects.at("line").stack.front().version==2,"Semantic API promotes exactly one stroke");
    std::cout<<"PASS versioned stroke styles, native/Undo, links, degenerate paths and SVG/API\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
