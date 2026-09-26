#include "nect/core.hpp"
#include "offset.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace nect {
ShapeOperation default_operation(Id id,const std::string& type) {
    ShapeOperation op;op.id=std::move(id);op.type=type;
    if(type=="nect.paint.fill"||type=="nect.paint.stroke") {
        op.parameters={{"r",{0,{}}},{"g",{0,{}}},{"b",{0,{}}},{"a",{1,{}}}};
        if(type=="nect.paint.stroke")op.parameters["width"]={2,{}};
    } else if(type=="nect.shape.repeater") {
        op.parameters={{"copies",{3,{}}},{"position_x",{100,{}}},{"position_y",{0,{}}},
            {"anchor_x",{0,{}}},{"anchor_y",{0,{}}},{"rotation",{0,{}}},
            {"scale_x",{1,{}}},{"scale_y",{1,{}}},{"offset",{0,{}}},
            {"start_opacity",{1,{}}},{"end_opacity",{1,{}}}};
    } else if(type=="nect.shape.offset") {
        op.parameters={{"amount",{10,{}}},{"miter_limit",{4,{}}}};
    } else throw Error("UNSUPPORTED_OPERATOR",type);
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
EvaluatedShape evaluate_shape(const Document& d,const Id& id,const std::map<Ref,double>& values) {
    const auto& o=d.objects.at(id);
    if(o.kind!=Kind::path&&o.kind!=Kind::text)throw Error("INVALID_DOMAIN","Shape stack accepts one Path or Text source");
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
        std::map<std::string,double> parameters;
        for(const auto& [name,scalar]:o.text->parameters){(void)scalar;parameters.emplace(name,values.at({id,"","text."+name}));}
        auto text=*o.text;text.italic=evaluate_text_italic(d,id);text.weight=evaluate_text_weight(d,id);
        source=evaluate_text(text,parameters).contours;
    }
    EvaluatedShape shape;shape.paths.push_back({source,identity_matrix});
    const auto anchors=[](const std::vector<PathInstance>& paths) {
        std::size_t count=0;for(const auto& path:paths)for(const auto& contour:*path.contours)count+=contour.points.size();return count;
    };
    const auto painted_anchors=[&] {std::size_t count=0;for(const auto& paint:shape.paints)count+=anchors(paint.paths);return count;};
    for(const auto& op:o.stack) {
        if(!op.enabled)continue;
        auto v=[&](const char* name){return values.at(operation_ref(id,op.id,name));};
        if(op.type=="nect.paint.fill"||op.type=="nect.paint.stroke") {
            if(painted_anchors()+anchors(shape.paths)>250000)
                throw Error("OUTPUT_LIMIT","Shape paint output exceeds 250000 cubic anchors per object");
            PaintLayer paint;paint.operation=op.id;paint.type=op.type;
            paint.rgba={v("r"),v("g"),v("b"),v("a")};paint.paths=shape.paths;paint.fill_rule=op.fill_rule;
            if(op.gradient&&op.gradient->enabled) {
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
