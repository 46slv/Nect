#include "offset.hpp"
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/strategies/agnostic/buffer_distance_symmetric.hpp>
#include <boost/geometry/strategies/cartesian/buffer_end_flat.hpp>
#include <boost/geometry/strategies/cartesian/buffer_join_round.hpp>
#include <boost/geometry/strategies/cartesian/buffer_point_circle.hpp>
#include <boost/geometry/strategies/cartesian/buffer_side_straight.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>

namespace nect {
namespace {
namespace bg=boost::geometry;
using BPoint=bg::model::d2::point_xy<double>;
using Polygon=bg::model::polygon<BPoint>;
using MultiPolygon=bg::model::multi_polygon<Polygon>;
using Line=bg::model::linestring<BPoint>;
using Contours=std::vector<EvaluatedContour>;
constexpr double tolerance=.1;
constexpr std::size_t input_limit=32768,ring_limit=256,work_limit=1000000,output_limit=250000;
void require(bool ok,const char* code,const char* message){if(!ok)throw Error(code,message);}
void finite(Vec2 point) {
    require(std::isfinite(point.x)&&std::isfinite(point.y)&&std::abs(point.x)<=1e12&&std::abs(point.y)<=1e12,
        "OUTPUT_RANGE","Offset geometry must be finite and within magnitude 1e12");
}
Vec2 middle(Vec2 a,Vec2 b){return {a.x+(b.x-a.x)/2,a.y+(b.y-a.y)/2};}
double segment_distance(Vec2 p,Vec2 a,Vec2 b) {
    const auto dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
    const auto t=length?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/length,0.0,1.0):0;
    return std::hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);
}
struct Flatten {
    std::size_t count=0;
    void append(Line& line,Vec2 point) {
        finite(point);
        if(!line.empty()&&bg::get<0>(line.back())==point.x&&bg::get<1>(line.back())==point.y)return;
        require(++count<=input_limit,"OFFSET_LIMIT","Offset flattened input limit 32768 vertices per instance");
        line.emplace_back(point.x,point.y);
    }
    void cubic(Line& line,Vec2 a,Vec2 b,Vec2 c,Vec2 d,unsigned depth=0) {
        finite(a);finite(b);finite(c);finite(d);
        if(std::max(segment_distance(b,a,d),segment_distance(c,a,d))<=tolerance){append(line,d);return;}
        require(depth<24,"OFFSET_LIMIT","Offset cubic subdivision depth limit 24");
        const auto ab=middle(a,b),bc=middle(b,c),cd=middle(c,d),abc=middle(ab,bc),bcd=middle(bc,cd),m=middle(abc,bcd);
        cubic(line,a,ab,abc,m,depth+1);cubic(line,m,bcd,cd,d,depth+1);
    }
};
// Boost's stock policy simplifies by abs(amount)/1000. Offset v1 declares
// its own .1 du cubic flattening and must not erase more input implicitly.
struct Distance : bg::strategy::buffer::distance_symmetric<double> {
    explicit Distance(double amount):distance_symmetric(amount){}
    double simplify_distance()const{return 0;}
};
// A true bevel fallback, unlike Boost join_miter's shortened miter tip.
struct Join {
    double limit;
    bool bevel;
    template<class Point,class DistanceType,class Range>
    bool apply(const Point& intersection,const Point& vertex,const Point& first,const Point& second,
               const DistanceType& distance,Range& output)const {
        if(bg::equals(first,second)||bg::equals(intersection,vertex))return false;
        output.push_back(first);
        const auto length=std::hypot(bg::get<0>(intersection)-bg::get<0>(vertex),bg::get<1>(intersection)-bg::get<1>(vertex));
        if(!bevel&&length<=std::abs(distance)*limit)output.push_back(intersection);
        output.push_back(second);return true;
    }
    template<class Numeric>Numeric max_distance(const Numeric& distance)const{return distance*(bevel?1:limit);}
};
struct Ring {
    Line boundary;
    Polygon polygon;
    double area=0;
    int sign=0,parent=-1,winding=0,depth=0;
};
std::shared_ptr<const Contours> offset_contours(const Contours& source,const Affine& matrix,double amount,
    double miter_limit,const std::string& join,const std::string& rule,std::size_t& work) {
    if(source.empty())return std::make_shared<const Contours>();
    require(source.size()<=ring_limit,"OFFSET_LIMIT","Offset compound limit 256 contours per instance");
    Flatten flatten;std::vector<Ring> rings;
    for(const auto& contour:source) {
        require(contour.closed,"OFFSET_OPEN_PATH","Offset v1 requires explicitly closed contours");
        require(contour.points.size()>=2,"OFFSET_GEOMETRY","Offset needs a nondegenerate closed region");
        Ring ring;flatten.append(ring.boundary,map_point(matrix,contour.points.front().anchor));
        for(std::size_t i=0;i<contour.points.size();++i) {
            const auto& a=contour.points[i];const auto& b=contour.points[(i+1)%contour.points.size()];
            flatten.cubic(ring.boundary,map_point(matrix,a.anchor),map_point(matrix,a.outgoing),map_point(matrix,b.incoming),map_point(matrix,b.anchor));
        }
        require(ring.boundary.size()>=4,"OFFSET_GEOMETRY","Offset needs at least three distinct region vertices");
        long double area=0;
        // Translate to the first vertex to avoid cancellation for small artwork
        // placed far from the origin; winding remains the authored winding.
        const auto ox=bg::get<0>(ring.boundary.front()),oy=bg::get<1>(ring.boundary.front());
        for(std::size_t i=1;i<ring.boundary.size();++i) {
            const auto& a=ring.boundary[i-1];const auto& b=ring.boundary[i];
            area+=(static_cast<long double>(bg::get<0>(a))-ox)*(bg::get<1>(b)-oy)
                -(static_cast<long double>(bg::get<0>(b))-ox)*(bg::get<1>(a)-oy);
        }
        require(area!=0,"OFFSET_GEOMETRY","Offset contour has zero signed area");
        ring.sign=area>0?1:-1;ring.area=static_cast<double>(std::abs(area)/2);
        ring.polygon.outer().assign(ring.boundary.begin(),ring.boundary.end());bg::correct(ring.polygon);
        require(bg::is_valid(ring.polygon),"OFFSET_GEOMETRY","Offset rejects self-intersecting or degenerate contours");
        rings.push_back(std::move(ring));
    }
    for(std::size_t i=0;i<rings.size();++i)for(std::size_t j=i+1;j<rings.size();++j)
        require(!bg::intersects(rings[i].boundary,rings[j].boundary),"OFFSET_GEOMETRY","Offset compound boundaries may not cross or touch");
    for(std::size_t i=0;i<rings.size();++i)for(std::size_t j=0;j<rings.size();++j)if(i!=j&&rings[j].area>rings[i].area&&
        bg::within(rings[i].boundary.front(),rings[j].polygon)) {
        if(rings[i].parent<0||rings[j].area<rings[static_cast<std::size_t>(rings[i].parent)].area)rings[i].parent=static_cast<int>(j);
    }
    std::vector<std::size_t> order;for(std::size_t i=0;i<rings.size();++i)order.push_back(i);
    std::sort(order.begin(),order.end(),[&](auto a,auto b){return rings[a].area>rings[b].area;});
    MultiPolygon input;std::map<std::size_t,std::size_t> regions;
    for(const auto i:order) {
        auto& ring=rings[i];const auto parent=ring.parent;
        const int outside=parent<0?0:rings[static_cast<std::size_t>(parent)].winding;
        ring.winding=outside+ring.sign;ring.depth=parent<0?1:rings[static_cast<std::size_t>(parent)].depth+1;
        const bool was_filled=rule=="evenodd"?(ring.depth%2==0):outside!=0;
        const bool is_filled=rule=="evenodd"?(ring.depth%2!=0):ring.winding!=0;
        if(!was_filled&&is_filled){regions.emplace(i,input.size());input.push_back(ring.polygon);}
        else if(was_filled&&!is_filled) {
            auto owner=parent;while(owner>=0&&!regions.contains(static_cast<std::size_t>(owner)))owner=rings[static_cast<std::size_t>(owner)].parent;
            require(owner>=0,"OFFSET_GEOMETRY","Offset hole has no enclosing region");
            auto hole=ring.polygon.outer();std::reverse(hole.begin(),hole.end());
            input[regions.at(static_cast<std::size_t>(owner))].inners().push_back(std::move(hole));
        }
    }
    require(bg::is_valid(input),"OFFSET_GEOMETRY","Offset compound region is invalid");
    std::size_t circle_points=4;
    if(join=="round") {
        const auto radius=std::abs(amount);
        const auto step=radius<=tolerance?std::numbers::pi/2:2*std::acos(std::clamp(1-tolerance/radius,-1.0,1.0));
        circle_points=std::max<std::size_t>(4,static_cast<std::size_t>(std::ceil(2*std::numbers::pi/step)));
    }
    const auto estimate=flatten.count*(join=="round"?circle_points:std::size_t{3});
    require(estimate<=work_limit-work,"OFFSET_LIMIT","Offset work limit 1000000 estimated join vertices per operation");work+=estimate;
    MultiPolygon output;
    if(!input.empty()) {
        const Distance distance(amount);const bg::strategy::buffer::side_straight side;
        const bg::strategy::buffer::end_flat end;const bg::strategy::buffer::point_circle point(circle_points);
        if(join=="round")bg::buffer(input,output,distance,side,bg::strategy::buffer::join_round(circle_points),end,point);
        else bg::buffer(input,output,distance,side,Join{miter_limit,join=="bevel"},end,point);
    }
    require(amount<0||input.empty()||!output.empty(),"OFFSET_GEOMETRY","Positive Offset could not represent its expanded region");
    require(output.empty()||bg::is_valid(output),"OFFSET_GEOMETRY","Offset could not produce a valid region");
    auto result=std::make_shared<Contours>();std::size_t vertices=0;
    const auto append=[&](const auto& ring) {
        if(ring.empty())return;
        require(ring.size()>=4,"OFFSET_GEOMETRY","Offset produced a degenerate output ring");
        EvaluatedContour contour;contour.closed=true;
        for(std::size_t i=0;i+1<ring.size();++i) {
            require(++vertices<=output_limit,"OUTPUT_LIMIT","Offset output exceeds 250000 anchors");
            const Vec2 p{bg::get<0>(ring[i]),bg::get<1>(ring[i])};finite(p);contour.points.push_back({p,p,p});
        }
        result->push_back(std::move(contour));
    };
    for(const auto& polygon:output){append(polygon.outer());for(const auto& hole:polygon.inners())append(hole);}
    return result;
}
}

void apply_offset(EvaluatedShape& shape,double amount,double miter_limit,const std::string& line_join,const std::string& fill_rule) {
    if(amount==0)return;
    using Key=std::pair<std::uintptr_t,Affine>;
    std::map<Key,std::shared_ptr<const Contours>> cache,pulled;
    std::map<Affine,Affine> inverses;
    for(const auto& paint:shape.paints)if(!inverses.contains(paint.transform))inverses.emplace(paint.transform,inverse_affine(paint.transform));
    std::vector<std::shared_ptr<const Contours>> inputs;std::size_t work=0;
    const auto offset=[&](const PathInstance& instance,const Affine& basis) {
        const auto matrix=compose(basis,instance.transform);const Key key{reinterpret_cast<std::uintptr_t>(instance.contours.get()),matrix};
        if(const auto found=cache.find(key);found!=cache.end())return found->second;
        inputs.push_back(instance.contours);
        auto result=offset_contours(*instance.contours,matrix,amount,miter_limit,line_join,fill_rule,work);
        cache.emplace(key,result);return result;
    };
    std::size_t geometry_count=0,paint_count=0;
    const auto count=[](const Contours& contours,std::size_t& total) {
        for(const auto& contour:contours)total+=contour.points.size();
        require(total<=output_limit,"OUTPUT_LIMIT","Offset geometry or paint exceeds 250000 anchors");
    };
    for(auto& instance:shape.paths) {
        auto result=offset(instance,identity_matrix);count(*result,geometry_count);instance={result,identity_matrix};
    }
    for(auto& paint:shape.paints) {
        // Keep gradient coordinates, stroke metrics and paint transform exactly.
        // A singular retained basis has no well-defined inverse pullback.
        const auto& inverse=inverses.at(paint.transform);
        for(auto& instance:paint.paths) {
            auto result=offset(instance,paint.transform);count(*result,paint_count);
            if(paint.transform!=identity_matrix&&!result->empty()) {
                const Key key{reinterpret_cast<std::uintptr_t>(result.get()),paint.transform};
                if(const auto found=pulled.find(key);found!=pulled.end())result=found->second;
                else {
                    auto local=std::make_shared<Contours>(*result);
                    for(auto& contour:*local)for(auto& point:contour.points) {
                        point.anchor=map_point(inverse,point.anchor);finite(point.anchor);point.incoming=point.outgoing=point.anchor;
                    }
                    pulled.emplace(key,local);result=std::move(local);
                }
            }
            instance={result,identity_matrix};
        }
    }
}
}
