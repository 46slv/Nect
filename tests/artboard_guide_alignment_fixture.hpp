#pragma once
#include "nect/io.hpp"
namespace guide_alignment_fixture {
inline nect::Document document(){
    using namespace nect;
    auto d=empty_document("guide-align-doc","comp","source");
    auto& c=d.compositions.front();c.artboards.push_back({"A","Same frame",1000,10,400,300});
    c.artboards.push_back({"B","Same frame",2000,10,400,300});
    c.artboards.front().local_guides={{"GX","Same guide","x",40,true},{"GY","Same guide","y",30,true}};
    c.artboards[1].local_guides={{"local","Same guide","x",15,true}};
    c.guides={{"global","Global","x",75}};
    Object o;o.id="rect";o.name="Rectangle";Contour contour;contour.id="contour";contour.closed=true;
    for(const auto p:std::vector<Vec2>{{0,0},{20,0},{20,10},{0,10}}){
        Point point;point.id="point"+std::to_string(contour.points.size());point.x.literal=p.x;point.y.literal=p.y;contour.points.push_back(point);
    }
    o.contours={contour};d.objects.emplace(o.id,o);c.roots={o.id};
    Session s(d);s.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"tmpl","Guides","source",{}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","tmpl",{}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","B","tmpl",{}}},
        ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",90.0}}},0);
    return s.document();
}
inline nect::Bounds bounds(const nect::Document& d){
    const auto values=nect::evaluate(d);
    return *nect::object_bounds(d,"rect",values,nect::evaluate_transforms(d,values),true);
}
}
