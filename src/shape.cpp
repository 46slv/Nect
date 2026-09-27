#include "nect/core.hpp"
#include "offset.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>

namespace nect {
const std::vector<BuiltinOperationType>& builtin_operation_types() {
    static const std::vector<BuiltinOperationType> types{
        {"nect.paint.fill","Fill","path_or_text","local_paths_and_paint","local_paths_and_paint",1,false,
            {{"r",0},{"g",0},{"b",0},{"a",1}}},
        {"nect.paint.stroke","Stroke","path_or_text","local_paths_and_paint","local_paths_and_paint",1,false,
            {{"r",0},{"g",0},{"b",0},{"a",1},{"width",2}}},
        {"nect.shape.repeater","Repeater","path_or_text","local_paths_and_paint","local_paths_and_paint",1,false,
            {{"copies",3},{"position_x",100},{"position_y",0},{"anchor_x",0},{"anchor_y",0},{"rotation",0},
             {"scale_x",1},{"scale_y",1},{"offset",0},{"start_opacity",1},{"end_opacity",1}}},
        {"nect.shape.offset","Offset Paths","path_or_text","local_paths_and_paint","local_paths_and_paint",1,true,
            {{"amount",10},{"miter_limit",4}}},
        {"nect.group.posterize","Group Posterize","group","postchildren_premultiplied_srgb_rgba","premultiplied_srgb_rgba",1,true,
            {{"levels",2}}},
    };
    return types;
}
const BuiltinOperationType* builtin_operation_type(const std::string& type) {
    const auto& types=builtin_operation_types();
    const auto it=std::find_if(types.begin(),types.end(),[&](const auto& entry){return entry.type==type;});
    return it==types.end()?nullptr:&*it;
}
ShapeOperation default_operation(Id id,const std::string& type) {
    const auto* descriptor=builtin_operation_type(type);
    if(!descriptor)throw Error("UNSUPPORTED_OPERATOR",type);
    ShapeOperation op;op.id=std::move(id);op.type=descriptor->type;op.version=descriptor->version;
    for(const auto& [name,value]:descriptor->parameter_defaults)op.parameters.emplace(name,Scalar{value,{}});
    return op;
}
Ref operation_ref(const Id& object,const Id& operation,const std::string& parameter) {
    return {object,"","op."+operation+"."+parameter};
}
Ref gradient_ref(const Id& object,const Id& operation,const Id& gradient,const std::string& field) {
    return operation_ref(object,operation,"gradient."+gradient+"."+field);
}
Vec2 map_point(const Affine& m,Vec2 p) {return {m[0]*p.x+m[2]*p.y+m[4],m[1]*p.x+m[3]*p.y+m[5]};}
Affine compose(const Affine& a,const Affine& b) {
    return {a[0]*b[0]+a[2]*b[1],a[1]*b[0]+a[3]*b[1],
        a[0]*b[2]+a[2]*b[3],a[1]*b[2]+a[3]*b[3],
        a[0]*b[4]+a[2]*b[5]+a[4],a[1]*b[4]+a[3]*b[5]+a[5]};
}
namespace {
constexpr double path_length_tolerance=0.05;
constexpr unsigned path_subdivision_depth=20;
constexpr std::size_t path_leaf_limit=4096;
struct CubicSegment {Vec2 p0,p1,p2,p3;};
double distance(Vec2 a,Vec2 b) {
    const auto value=std::hypot(a.x-b.x,a.y-b.y);
    if(!std::isfinite(value))throw Error("PATH_SAMPLE_NON_FINITE","Path segment length is non-finite");
    return value;
}
Vec2 cubic_position(const PathCubicSegment& c,double t) {
    const auto u=1-t,u2=u*u,t2=t*t;
    return {u2*u*c.p0.x+3*u2*t*c.p1.x+3*u*t2*c.p2.x+t2*t*c.p3.x,
        u2*u*c.p0.y+3*u2*t*c.p1.y+3*u*t2*c.p2.y+t2*t*c.p3.y};
}
Vec2 cubic_derivative(const PathCubicSegment& c,double t) {
    const auto u=1-t;
    return {3*u*u*(c.p1.x-c.p0.x)+6*u*t*(c.p2.x-c.p1.x)+3*t*t*(c.p3.x-c.p2.x),
        3*u*u*(c.p1.y-c.p0.y)+6*u*t*(c.p2.y-c.p1.y)+3*t*t*(c.p3.y-c.p2.y)};
}
double cubic_arc_length(const PathCubicSegment& c,double from,double to) {
    // Five-point Gauss-Legendre is evaluated only on adaptive leaves whose
    // control-polygon excess is bounded by the contour's recorded tolerance.
    constexpr std::array<double,3> nodes{0.0,0.5384693101056831,0.9061798459386640};
    constexpr std::array<double,3> weights{0.5688888888888889,0.4786286704993665,0.2369268850561891};
    const auto middle=(from+to)/2,half=(to-from)/2;
    auto speed=[&](double t){const auto d=cubic_derivative(c,t);return std::hypot(d.x,d.y);};
    double sum=weights[0]*speed(middle);
    for(std::size_t i=1;i<nodes.size();++i)sum+=weights[i]*(speed(middle-half*nodes[i])+speed(middle+half*nodes[i]));
    const auto result=sum*half;
    if(!std::isfinite(result))throw Error("PATH_SAMPLE_NON_FINITE","Path cubic arc length is non-finite");
    return result;
}
Vec2 unit_tangent(const PathCubicSegment& c,double t) {
    auto derivative=cubic_derivative(c,t);auto magnitude=std::hypot(derivative.x,derivative.y);
    if(magnitude==0) {
        // At a stationary endpoint, use the first nonzero derivative term in
        // the direction of traversal. This preserves the authored one-sided
        // tangent at knots with a zero-length adjacent handle.
        if(t<=0) {
            for(const auto point:{c.p1,c.p2,c.p3}) {
                derivative={point.x-c.p0.x,point.y-c.p0.y};magnitude=std::hypot(derivative.x,derivative.y);
                if(magnitude>0)break;
            }
        } else if(t>=1) {
            for(const auto point:{c.p2,c.p1,c.p0}) {
                derivative={c.p3.x-point.x,c.p3.y-point.y};magnitude=std::hypot(derivative.x,derivative.y);
                if(magnitude>0)break;
            }
        }
    }
    if(!std::isfinite(magnitude)||magnitude==0)
        throw Error("PATH_SAMPLE_TANGENT","Path sample has no resolvable one-sided tangent");
    return {derivative.x/magnitude,derivative.y/magnitude};
}
Vec2 midpoint(Vec2 a,Vec2 b) {return {(a.x+b.x)/2,(a.y+b.y)/2};}
std::pair<CubicSegment,CubicSegment> split_half(const CubicSegment& c) {
    const auto a=midpoint(c.p0,c.p1),b=midpoint(c.p1,c.p2),d=midpoint(c.p2,c.p3);
    const auto e=midpoint(a,b),f=midpoint(b,d),g=midpoint(e,f);
    return {{c.p0,a,e,g},{g,f,d,c.p3}};
}
void finite_point(Vec2 p) {
    if(!std::isfinite(p.x)||!std::isfinite(p.y))throw Error("PATH_SAMPLE_NON_FINITE","Path contains a non-finite world coordinate");
}
Vec2 local_handle(Vec2 anchor,double angle,double length) {
    const auto radians=angle*std::numbers::pi/180;
    return {anchor.x+std::cos(radians)*length,anchor.y+std::sin(radians)*length};
}
void append_leaves(const CubicSegment& curve,const PathCubicSegment& source,std::size_t cubic_index,
    double t0,double t1,double tolerance,unsigned depth,PathSampler& sampler,std::size_t& leaves) {
    const auto chord=distance(curve.p0,curve.p3);
    const auto polygon=distance(curve.p0,curve.p1)+distance(curve.p1,curve.p2)+distance(curve.p2,curve.p3);
    if(!std::isfinite(polygon))throw Error("PATH_SAMPLE_NON_FINITE","Path control polygon length is non-finite");
    if(polygon-chord<=tolerance) {
        if(++leaves>path_leaf_limit)throw Error("PATH_SAMPLE_BUDGET","Path sampling exceeded 4096 leaves per contour");
        const auto leaf_length=cubic_arc_length(source,t0,t1);
        if(leaf_length>0) {
            const auto from=sampler.length,next=from+leaf_length;
            if(!std::isfinite(next))throw Error("PATH_SAMPLE_NON_FINITE","Path contour length is non-finite");
            sampler.segments.push_back({cubic_index,t0,t1,from,next});sampler.length=next;
        }
        return;
    }
    if(depth>=path_subdivision_depth)throw Error("PATH_SAMPLE_BUDGET","Path sampling could not meet the 0.05 du96 error tolerance by depth 20");
    const auto [left,right]=split_half(curve);
    const auto middle=(t0+t1)/2;
    append_leaves(left,source,cubic_index,t0,middle,tolerance/2,depth+1,sampler,leaves);
    append_leaves(right,source,cubic_index,middle,t1,tolerance/2,depth+1,sampler,leaves);
}
std::shared_ptr<const std::vector<EvaluatedContour>> project_text_on_path(const Document& document,const Id& text_id,
    const TextSource& source,const TextLayout& shaped,const std::map<Ref,double>& values,TextLayout& result) {
    if(!source.path_attachment)throw Error("INVALID_PATH_ATTACHMENT","Text-on-Path projection requires an attachment");
    const auto& attachment=*source.path_attachment;
    const auto path=document.objects.find(attachment.path);
    if(path==document.objects.end())throw Error("MISSING_PATH_ATTACHMENT","Text-on-Path source Path no longer exists");
    if(path->second.kind!=Kind::path)throw Error("INVALID_PATH_ATTACHMENT","Text-on-Path source must be a Path object");
    if(path->second.source)throw Error("GENERATED_PATH_ATTACHMENT","Text-on-Path requires an authored Path contour; convert generated geometry first");
    const auto transforms=evaluate_transforms(document,values);
    const auto& text_transform=transforms.at(text_id);
    const auto& path_transform=transforms.at(attachment.path);
    auto parent=path_transform.effective_parent;
    for(unsigned depth=0;!parent.empty()&&depth<=128;++depth) {
        if(parent==text_id)throw Error("TEXT_PATH_TRANSFORM_CYCLE","A Text-on-Path source cannot inherit its consumer's transform");
        const auto found=transforms.find(parent);if(found==transforms.end())break;parent=found->second.effective_parent;
    }
    const auto sampler=build_path_sampler(document,attachment.path,attachment.contour,values);
    const auto world_to_text=inverse_affine(text_transform.world);
    const double anchor=attachment.start_mode=="normalized"?attachment.start*sampler.length:attachment.start;
    if(!std::isfinite(anchor))throw Error("TEXT_PATH_START_INVALID","Text-on-Path start resolves to a non-finite distance");
    if(!sampler.closed&&((attachment.start_mode=="normalized"&&(attachment.start<0||attachment.start>1))||
        (attachment.start_mode=="distance"&&(attachment.start<0||attachment.start>sampler.length))))
        throw Error("TEXT_PATH_OVERFLOW","Text-on-Path start anchor is outside the open contour");
    if(!std::isfinite(attachment.spacing)||attachment.spacing<0)
        throw Error("TEXT_PATH_SPACING","Text-on-Path spacing must be finite and nonnegative");
    if(shaped.glyphs.size()!=shaped.glyph_count)
        throw Error("TEXT_PATH_RUN_UNSUPPORTED","DirectWrite did not provide a complete shaped glyph run");
    const auto base_baseline=shaped.glyphs.empty()?Vec2{}:shaped.glyphs.front().baseline;
    if(!std::isfinite(base_baseline.x)||!std::isfinite(base_baseline.y))
        throw Error("TEXT_PATH_ADVANCE_INVALID","DirectWrite returned a non-finite glyph baseline");
    std::vector<Vec2> baseline_offsets;baseline_offsets.reserve(shaped.glyphs.size());
    double span_start=0,span_end=0;
    for(std::size_t i=0;i<shaped.glyphs.size();++i) {
        const auto& glyph=shaped.glyphs[i];
        if(!std::isfinite(glyph.advance)||glyph.advance<0)throw Error("TEXT_PATH_ADVANCE_INVALID","DirectWrite returned an invalid shaped glyph advance");
        if(!std::isfinite(glyph.baseline.x)||!std::isfinite(glyph.baseline.y))
            throw Error("TEXT_PATH_ADVANCE_INVALID","DirectWrite returned a non-finite glyph baseline");
        const Vec2 offset{glyph.baseline.x-base_baseline.x+attachment.spacing*static_cast<double>(i),
            glyph.baseline.y-base_baseline.y};
        if(!std::isfinite(offset.x)||!std::isfinite(offset.y))
            throw Error("TEXT_PATH_ADVANCE_INVALID","Shaped glyph offsets exceed finite layout bounds");
        baseline_offsets.push_back(offset);
        span_start=std::min(span_start,offset.x);span_end=std::max(span_end,offset.x+glyph.advance);
    }
    const auto span=span_end-span_start;
    if(!std::isfinite(span))throw Error("TEXT_PATH_ADVANCE_INVALID","Text advance span is non-finite");
    if(sampler.closed&&span>sampler.length)throw Error("TEXT_PATH_OVERFLOW","Text span cannot exceed one closed-contour lap");
    const auto align_offset=source.alignment=="center"?(span_start+span_end)/2:
        source.alignment=="end"?span_end:0;
    const double first=anchor-align_offset,last=first+span_end;
    if(!std::isfinite(first)||!std::isfinite(last))throw Error("TEXT_PATH_START_INVALID","Aligned Text span is non-finite");
    if(!sampler.closed&&(first+span_start<0||last>sampler.length))
        throw Error("TEXT_PATH_OVERFLOW","Text advance span does not fit on the open contour");

    auto projected=std::make_shared<std::vector<EvaluatedContour>>();
    std::size_t anchors=0;
    for(std::size_t i=0;i<shaped.glyphs.size();++i) {
        const auto& glyph=shaped.glyphs[i];
        const auto offset=baseline_offsets[i];
        const auto sample=sample_path(sampler,first+offset.x+glyph.advance/2,attachment.reversed);
        const Vec2 baseline_world{sample.position.x-sample.tangent.x*glyph.advance/2,
            sample.position.y-sample.tangent.y*glyph.advance/2};
        const Vec2 normal{-sample.tangent.y,sample.tangent.x};
        for(const auto& contour:glyph.contours) {
            EvaluatedContour placed;placed.closed=contour.closed;placed.points.reserve(contour.points.size());
            auto map=[&](Vec2 point) {
                const Vec2 relative{point.x-glyph.baseline.x,point.y-glyph.baseline.y};
                const Vec2 world{baseline_world.x+sample.tangent.x*relative.x+normal.x*relative.y,
                    baseline_world.y+sample.tangent.y*relative.x+normal.y*(relative.y+offset.y)};
                const auto local=map_point(world_to_text,world);
                finite_point(local);return local;
            };
            for(const auto& point:contour.points) {
                placed.points.push_back({map(point.anchor),map(point.incoming),map(point.outgoing)});
                if(++anchors>250000)throw Error("TEXT_OUTLINE_LIMIT","Text-on-Path projection exceeds 250000 cubic anchors");
            }
            projected->push_back(std::move(placed));
        }
    }
    result=shaped;
    result.contours=projected;
    result.glyphs.clear();
    result.first_line_baseline_y.reset();result.line_baselines_y.clear();result.column_baselines_x.clear();result.overflow=false;
    bool has_bounds=false;double left=0,top=0,right=0,bottom=0;
    auto include=[&](Vec2 point) {
        if(!has_bounds){left=right=point.x;top=bottom=point.y;has_bounds=true;return;}
        left=std::min(left,point.x);right=std::max(right,point.x);top=std::min(top,point.y);bottom=std::max(bottom,point.y);
    };
    for(const auto& contour:*projected)for(const auto& point:contour.points){include(point.anchor);include(point.incoming);include(point.outgoing);}
    if(has_bounds){result.x=left;result.y=top;result.width=right-left;result.height=bottom-top;}
    else result.x=result.y=result.width=result.height=0;
    return projected;
}
}
PathSampler build_path_sampler(const Document& document,const Id& path_id,const Id& contour_id,const std::map<Ref,double>& values) {
    const auto object=document.objects.find(path_id);
    if(object==document.objects.end())throw Error("MISSING_PATH_ATTACHMENT","Path sampling source no longer exists");
    if(object->second.kind!=Kind::path)throw Error("INVALID_PATH_ATTACHMENT","Path sampling source must be a Path object");
    if(object->second.source)throw Error("GENERATED_PATH_ATTACHMENT","Path sampling requires an authored contour");
    const auto found=std::find_if(object->second.contours.begin(),object->second.contours.end(),[&](const Contour& contour){return contour.id==contour_id;});
    if(found==object->second.contours.end())throw Error("MISSING_PATH_CONTOUR","Path sampling contour ID no longer exists");
    const auto transforms=evaluate_transforms(document,values);
    const auto& world=transforms.at(path_id).world;
    EvaluatedContour contour;contour.closed=found->closed;contour.points.reserve(found->points.size());
    for(const auto& point:found->points) {
        const auto anchor=map_point(world,{values.at({path_id,point.id,"x"}),values.at({path_id,point.id,"y"})});
        const Vec2 local_anchor{values.at({path_id,point.id,"x"}),values.at({path_id,point.id,"y"})};
        auto incoming=map_point(world,local_handle(local_anchor,values.at({path_id,point.id,"in.angle"}),values.at({path_id,point.id,"in.length"})));
        auto outgoing=map_point(world,local_handle(local_anchor,values.at({path_id,point.id,"out.angle"}),values.at({path_id,point.id,"out.length"})));
        finite_point(anchor);finite_point(incoming);finite_point(outgoing);contour.points.push_back({anchor,incoming,outgoing});
    }
    if(contour.points.empty())throw Error("PATH_SAMPLE_ZERO_LENGTH","Path contour has no segments");
    PathSampler sampler;sampler.path=path_id;sampler.contour=contour_id;sampler.closed=contour.closed;
    const std::size_t segment_count=contour.closed?contour.points.size():contour.points.size()-1;
    if(segment_count==0)throw Error("PATH_SAMPLE_ZERO_LENGTH","Open Path contour needs at least two points");
    std::size_t leaves=0;
    for(std::size_t i=0;i<segment_count;++i) {
        const auto& from=contour.points[i];const auto& to=contour.points[(i+1)%contour.points.size()];
        PathCubicSegment cubic{from.anchor,from.outgoing,to.incoming,to.anchor,sampler.length,sampler.length};
        const auto whole_length=cubic_arc_length(cubic,0,1);
        if(whole_length==0)continue; // Keep authored point order; skip only a zero-length cubic.
        const auto cubic_index=sampler.cubics.size();sampler.cubics.push_back(cubic);
        append_leaves({cubic.p0,cubic.p1,cubic.p2,cubic.p3},sampler.cubics.back(),cubic_index,
            0,1,path_length_tolerance/static_cast<double>(segment_count),0,sampler,leaves);
        if(sampler.length==cubic.start_distance) {
            sampler.cubics.pop_back(); // Arc length underflowed to zero in the cumulative representation.
            continue;
        }
        sampler.cubics.back().end_distance=sampler.length;
    }
    if(!std::isfinite(sampler.length)||sampler.length<=0)throw Error("PATH_SAMPLE_ZERO_LENGTH","Path contour length must be finite and nonzero");
    return sampler;
}
PathSample sample_path(const PathSampler& sampler,double distance_value,bool reversed) {
    if(!std::isfinite(distance_value))throw Error("PATH_SAMPLE_NON_FINITE","Path sample distance must be finite");
    if(!std::isfinite(sampler.length)||sampler.length<=0||sampler.segments.empty())
        throw Error("PATH_SAMPLE_ZERO_LENGTH","Path sampler has no finite nonzero contour");
    double resolved=distance_value;
    if(sampler.closed) {
        resolved=std::fmod(resolved,sampler.length);
        if(resolved<0)resolved+=sampler.length;
    } else if(resolved<0||resolved>sampler.length)throw Error("PATH_SAMPLE_RANGE","Open Path sample distance must be within the contour");
    const auto along=reversed?sampler.length-resolved:resolved;
    const auto epsilon=std::min(0.005,8*std::numeric_limits<double>::epsilon()*std::max({1.0,std::abs(along),sampler.length}));
    const auto near=[&](double a,double b){return std::abs(a-b)<=epsilon;};
    std::size_t cubic_index=0;double t=0;bool at_knot=false;
    if(reversed) {
        if(sampler.closed&&near(along,sampler.length)) {
            cubic_index=sampler.cubics.size()-1;t=1;at_knot=true;
        } else {
            for(std::size_t i=0;i<sampler.cubics.size();++i)if(near(along,sampler.cubics[i].end_distance)) {
                cubic_index=i;t=1;at_knot=true;break;
            }
            if(!at_knot&&sampler.closed&&near(along,0)) {
                cubic_index=sampler.cubics.size()-1;t=1;at_knot=true;
            } else if(!at_knot&&!sampler.closed&&near(along,0)) {
                cubic_index=0;t=0;at_knot=true;
            }
        }
    } else {
        for(std::size_t i=0;i<sampler.cubics.size();++i)if(near(along,sampler.cubics[i].start_distance)) {
            cubic_index=i;t=0;at_knot=true;break;
        }
        if(!at_knot&&!sampler.closed&&near(along,sampler.length)) {
            cubic_index=sampler.cubics.size()-1;t=1;at_knot=true;
        }
    }
    PathSampleSegment const* leaf=nullptr;
    if(!at_knot) {
        const auto found=std::upper_bound(sampler.segments.begin(),sampler.segments.end(),along,
            [](double value,const PathSampleSegment& segment){return value<segment.end_distance;});
        if(found==sampler.segments.end())leaf=&sampler.segments.back();else leaf=&*found;
        cubic_index=leaf->cubic;
        const auto& cubic=sampler.cubics.at(cubic_index);
        const auto wanted=std::clamp(along-leaf->start_distance,0.0,leaf->end_distance-leaf->start_distance);
        auto low=leaf->t_start,high=leaf->t_end;
        for(unsigned i=0;i<56;++i) {
            const auto middle=(low+high)/2;
            if(middle==low||middle==high)break;
            if(cubic_arc_length(cubic,leaf->t_start,middle)<wanted)low=middle;else high=middle;
        }
        t=(low+high)/2;
    }
    const auto& cubic=sampler.cubics.at(cubic_index);
    const auto position=cubic_position(cubic,t);
    auto tangent=unit_tangent(cubic,t);
    if(reversed){tangent.x=-tangent.x;tangent.y=-tangent.y;}
    finite_point(position);finite_point(tangent);
    return {position,tangent,{-tangent.y,tangent.x},resolved,sampler.length,sampler.path,sampler.contour};
}
TextLayout evaluate_text_projection(const Document& document,const Id& id,const std::map<Ref,double>& values) {
    const auto found=document.objects.find(id);
    if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
    if(!found->second.text)throw Error("NOT_TEXT",id);
    std::map<std::string,double> parameters;
    for(const auto& [name,scalar]:found->second.text->parameters){(void)scalar;parameters.emplace(name,values.at({id,"","text."+name}));}
    const auto source=evaluated_text_source(document,id);
    const auto shaped=evaluate_text(source,parameters);
    if(!source.path_attachment)return shaped;
    TextLayout projected;
    project_text_on_path(document,id,source,shaped,values,projected);
    return projected;
}
EvaluatedShape evaluate_shape(const Document& d,const Id& id,const std::map<Ref,double>& values,
    const std::map<Ref,std::string>* fill_rules,const std::map<Ref,bool>* operation_enabled,
    const std::map<Ref,bool>* gradient_enabled) {
    const auto& o=d.objects.at(id);
    if(o.kind!=Kind::path&&o.kind!=Kind::text)throw Error("INVALID_DOMAIN","Shape stack accepts one Path or Text source");
    std::map<Ref,bool> computed_operation_enabled;
    if(!operation_enabled) {
        computed_operation_enabled=evaluate_operation_enableds(d);
        operation_enabled=&computed_operation_enabled;
    }
    std::map<Ref,bool> computed_gradient_enabled;
    if(!gradient_enabled) {
        computed_gradient_enabled=evaluate_gradient_enableds(d);
        gradient_enabled=&computed_gradient_enabled;
    }
    auto contours=std::make_shared<std::vector<EvaluatedContour>>();
    for(const auto& contour:path_contours(o,&values)) {
        EvaluatedContour result;result.closed=contour.closed;
        for(const auto& p:contour.points) {
            auto v=[&](const char* field){return values.at({id,p.id,field});};
            const Vec2 anchor{v("x"),v("y")};
            auto handle=[&](double angle,double length){const auto a=angle*std::numbers::pi/180;
                return Vec2{anchor.x+std::cos(a)*length,anchor.y+std::sin(a)*length};};
            result.points.push_back({anchor,handle(v("in.angle"),v("in.length")),handle(v("out.angle"),v("out.length"))});
        }
        contours->push_back(std::move(result));
    }
    std::shared_ptr<const std::vector<EvaluatedContour>> source=contours;
    if(o.text) {
        source=evaluate_text_projection(d,id,values).contours;
    }
    EvaluatedShape shape;shape.paths.push_back({source,identity_matrix});
    const auto anchors=[](const std::vector<PathInstance>& paths) {
        std::size_t count=0;for(const auto& path:paths)for(const auto& contour:*path.contours)count+=contour.points.size();return count;
    };
    const auto painted_anchors=[&] {std::size_t count=0;for(const auto& paint:shape.paints)count+=anchors(paint.paths);return count;};
    for(const auto& op:o.stack) {
        if(!operation_enabled->at(operation_ref(id,op.id,"enabled")))continue;
        auto v=[&](const char* name){return values.at(operation_ref(id,op.id,name));};
        if(op.type=="nect.paint.fill"||op.type=="nect.paint.stroke") {
            if(painted_anchors()+anchors(shape.paths)>250000)
                throw Error("OUTPUT_LIMIT","Shape paint output exceeds 250000 cubic anchors per object");
            PaintLayer paint;paint.operation=op.id;paint.type=op.type;
            paint.rgba={v("r"),v("g"),v("b"),v("a")};paint.paths=shape.paths;
            paint.fill_rule=op.type=="nect.paint.fill"?
                (fill_rules?fill_rules->at(operation_ref(id,op.id,"fill_rule")):evaluate_fill_rule(d,operation_ref(id,op.id,"fill_rule"))):op.fill_rule;
            if(op.gradient&&gradient_enabled->at(gradient_ref(id,op.id,op.gradient->id,"enabled"))) {
                const auto& g=*op.gradient;EvaluatedGradient resolved;resolved.type=g.type;
                auto gv=[&](const std::string& field){return values.at(gradient_ref(id,op.id,g.id,field));};
                resolved.start={gv("start_x"),gv("start_y")};resolved.end={gv("end_x"),gv("end_y")};
                for(const auto& stop:g.stops)resolved.stops.push_back({gv("stop."+stop.id+".offset"),
                    {gv("stop."+stop.id+".r"),gv("stop."+stop.id+".g"),gv("stop."+stop.id+".b"),gv("stop."+stop.id+".a")}});
                std::stable_sort(resolved.stops.begin(),resolved.stops.end(),[](const auto& a,const auto& b){return a.offset<b.offset;});
                paint.gradient=std::move(resolved);
            }
            if(op.type=="nect.paint.stroke") {
                paint.width=v("width");paint.line_cap=op.line_cap;paint.line_join=op.line_join;
                if(op.version==2)paint.miter_limit=v("miter_limit");

            }
            if(op.composite=="above")shape.paints.push_back(std::move(paint));
            else shape.paints.insert(shape.paints.begin(),std::move(paint));
        } else if(op.type=="nect.shape.repeater") {
            const auto copies=static_cast<unsigned>(v("copies"));
            if(shape.paths.size()*copies>4096||shape.paints.size()*copies>8192)
                throw Error("OUTPUT_LIMIT","Repeated output exceeds 4096 path instances or 8192 paint layers per object");
            if(anchors(shape.paths)*copies>250000||painted_anchors()*copies>250000)
                throw Error("OUTPUT_LIMIT","Repeated output exceeds 250000 cubic anchors per object");
            const auto paths=std::move(shape.paths);const auto paints=std::move(shape.paints);
            shape.paths.clear();shape.paints.clear();
            auto matrix=[&](unsigned index) {
                const auto n=static_cast<double>(index)+v("offset");
                const auto angle=n*v("rotation")*std::numbers::pi/180;
                const auto sx=std::pow(v("scale_x"),n),sy=std::pow(v("scale_y"),n);
                Affine m{std::cos(angle)*sx,std::sin(angle)*sx,-std::sin(angle)*sy,std::cos(angle)*sy,0,0};
                const Vec2 anchor{v("anchor_x"),v("anchor_y")};const auto mapped=map_point(m,anchor);
                m[4]=anchor.x-mapped.x+n*v("position_x");m[5]=anchor.y-mapped.y+n*v("position_y");
                for(auto number:m)if(!std::isfinite(number)||std::abs(number)>1e12)
                    throw Error("OUTPUT_RANGE","Repeater transform exceeds finite output bounds");
                return m;
            };
            for(unsigned i=0;i<copies;++i) {
                const auto transform=matrix(i);
                for(const auto& path:paths)shape.paths.push_back({path.contours,compose(transform,path.transform)});
            }
            for(unsigned i=0;i<copies;++i) {
                const auto index=op.composite=="above"?i:copies-1-i;
                const auto transform=matrix(index);
                const auto t=copies<2?0:static_cast<double>(index)/(copies-1);
                const auto opacity=v("start_opacity")+(v("end_opacity")-v("start_opacity"))*t;
                for(auto paint:paints) {
                    paint.transform=compose(transform,paint.transform);paint.rgba[3]*=opacity;
                    shape.paints.push_back(std::move(paint));
                }
            }
        } else if(op.type=="nect.shape.offset") {
            apply_offset(shape,v("amount"),v("miter_limit"),op.line_join,op.fill_rule);
        } else throw Error("UNSUPPORTED_OPERATOR",op.type);
        if(shape.paints.size()>8192)throw Error("OUTPUT_LIMIT","Paint layer limit 8192 per object");
        if(shape.paths.size()>4096||anchors(shape.paths)>250000||painted_anchors()>250000)
            throw Error("OUTPUT_LIMIT","Shape output exceeds 4096 instances or 250000 geometry/paint anchors per object");
    }
    // Derive degenerate subpaths after the entire stack: downstream Offset can
    // replace earlier paint geometry; Repeater can transform whole paint layers.
    for(auto& paint:shape.paints)if(paint.type=="nect.paint.stroke"&&paint.line_cap!="butt")for(const auto& instance:paint.paths)for(const auto& contour:*instance.contours) {
        if(contour.points.empty()||(!contour.closed&&contour.points.size()==1))continue;
        const auto first=map_point(instance.transform,contour.points.front().anchor);
        const auto same=[&](Vec2 point){const auto p=map_point(instance.transform,point);return p.x==first.x&&p.y==first.y;};
        const auto count=contour.closed?contour.points.size():contour.points.size()-1;
        bool degenerate=true;
        for(std::size_t i=0;i<count;++i) {
            const auto& a=contour.points[i];const auto& b=contour.points[(i+1)%contour.points.size()];
            if(!same(a.outgoing)||!same(b.incoming)||!same(b.anchor)){degenerate=false;break;}
        }
        if(degenerate)paint.degenerate_subpaths.push_back(first);
    }
    return shape;
}
}
