#pragma once
#include "nect/io.hpp"
namespace inherited_grid_fixture {
inline nect::Document document(){
    using namespace nect;
    auto d=empty_document("grid-align-doc","comp","source");auto& c=d.compositions.front();
    c.artboards.front().x=-500;c.artboards.front().y=-600;
    c.artboards.front().layout=ArtboardLayout{{},Grid{"source-grid",{40,30,100,80},1,1,0,0}};
    c.artboards.push_back({"A","Same frame",1000,200,400,300});c.artboards.push_back({"B","Same frame",2000,400,400,300});
    for(int n=0;n<3;++n){
        const auto id=std::string(1,static_cast<char>('a'+n));const double origin=n==0?0:n==1?30:90;const double size=n==1?20:10;
        Object object;object.id=id;object.name=id;Contour contour;contour.id=id+"-contour";contour.closed=true;
        for(const auto p:std::vector<Vec2>{{origin,origin},{origin+size,origin},{origin+size,origin+size},{origin,origin+size}}){
            Point point;point.id=id+"-p"+std::to_string(contour.points.size());point.x.literal=p.x;point.y.literal=p.y;contour.points.push_back(point);
        }
        object.contours={contour};d.objects.emplace(id,object);c.roots.push_back(id);
    }
    Session s(d);s.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"tmpl","Template","source",{}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","tmpl",{}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","B","tmpl",{}}}},0);return s.document();
}
inline double minimum(const nect::Document& d,const nect::Id& id,bool x=true){
    const double origin=id=="a"?0:id=="b"?30:90;
    return origin+d.objects.at(id).transform[x?4:5].literal;
}
}
