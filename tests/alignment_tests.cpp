#include "nect/io.hpp"
#include <cmath>
#include <iostream>
using namespace nect;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void near(double a,double b){check(std::abs(a-b)<1e-7,"Unexpected aligned coordinate");}
Object rectangle(const Id& id,double x,double y,double w,double h){
 Object o;o.id=id;o.name=id;Contour c;c.id=id+"-contour";c.closed=true;
 for(auto p:std::vector<Vec2>{{x,y},{x+w,y},{x+w,y+h},{x,y+h}}){Point q;q.id=id+std::to_string(c.points.size());q.x.literal=p.x;q.y.literal=p.y;c.points.push_back(q);}
 o.contours.push_back(c);return o;
}
Document fixture(){auto d=empty_document("doc","comp","art");d.objects.emplace("a",rectangle("a",10,20,20,30));d.objects.emplace("b",rectangle("b",100,140,50,20));d.compositions[0].roots={"a","b"};return d;}
Bounds bounds(const Document& d,const Id& id){const auto v=evaluate(d);return *object_bounds(d,id,v,evaluate_transforms(d,v),true);}
void apply(Session& s,AlignObjects c){s.apply({c},s.revision());}
void rejects(Session& s,AlignObjects c,const std::string& code){auto before=s.document();auto revision=s.revision();try{apply(s,c);throw std::runtime_error("Expected failure");}catch(const Error& e){check(e.code==code,e.what());}check(s.document()==before&&s.revision()==revision,"Failure must be atomic");}
int main(){try{
 for(auto axis:{"x","y"})for(auto alignment:{"min","center","max"}){
  auto d=fixture();Session s(d);apply(s,{{"a","b"},axis,alignment,{}});const auto a=bounds(s.document(),"a"),b=bounds(s.document(),"b");
  auto coordinate=[&](Bounds r){auto lo=std::string(axis)=="x"?r.left:r.top,hi=std::string(axis)=="x"?r.right:r.bottom;return std::string(alignment)=="min"?lo:std::string(alignment)=="max"?hi:(lo+hi)/2;};near(coordinate(a),coordinate(b));
  check(s.document().objects.at("a").contours==d.objects.at("a").contours,"Authored geometry unchanged");
  auto aligned=s.document();check(decode(encode(aligned))==aligned,"Native roundtrip");s.undo(s.revision());check(s.document()==d,"One Undo exact restoration");s.redo(s.revision());check(s.document()==aligned,"Redo restores alignment");
 }
 auto d=fixture();d.compositions[0].artboards[0].x=50;d.compositions[0].artboards[0].y=80;d.compositions[0].artboards[0].width=200;d.compositions[0].artboards[0].height=100;
 Session art(d);apply(art,{{"a"},"x","center","art"});near((bounds(art.document(),"a").left+bounds(art.document(),"a").right)/2,150);
 // Effective followers must receive independently solved target displacements.
 d=fixture();d.objects.at("b").transform_parent="a";Session follower(d);apply(follower,{{"b","a"},"x","min",{}});near(bounds(follower.document(),"a").left,10);near(bounds(follower.document(),"b").left,10);
 // Rotation is retained; extrema are computed in world space, not by rotating a local AABB.
 d=fixture();d.objects.at("b").transform[0].literal=0;d.objects.at("b").transform[1].literal=1;d.objects.at("b").transform[2].literal=-1;d.objects.at("b").transform[3].literal=0;
 Session rotated(d);apply(rotated,{{"a","b"},"y","max",{}});near(bounds(rotated.document(),"a").bottom,150);near(bounds(rotated.document(),"b").bottom,150);check(rotated.document().objects.at("b").transform[1].literal==1,"Rotation preserved");
 d=fixture();d.objects.at("b").transform[4].binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};Session driven(d);rejects(driven,{{"a","b"},"x","min",{}},"DRIVEN_PROPERTY");
 Session bad(fixture());rejects(bad,{{"a","a"},"x","min",{}},"DUPLICATE_TARGET");rejects(bad,{{"a","b"},"z","min",{}},"INVALID_ALIGNMENT");rejects(bad,{{"a"},"x","min",{}},"INVALID_BATCH");rejects(bad,{{"a"},"x","min","missing"},"MISSING_ARTBOARD");
 d=fixture();Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"a"};d.objects.emplace("group",group);d.compositions[0].roots={"group","b"};Session grouped(d);apply(grouped,{{"group","b"},"x","max",{}});near(bounds(grouped.document(),"a").right,150);rejects(grouped,{{"group","a"},"x","min",{}},"OVERLAPPING_SELECTION");
 d=fixture();d.objects.at("b").transform_parent="a";Session follower_max(d);apply(follower_max,{{"b","a"},"x","max",{}});near(bounds(follower_max.document(),"a").right,150);near(bounds(follower_max.document(),"b").right,150);
 d=fixture();auto& diagonal=d.objects.at("a");diagonal.contours[0].closed=false;diagonal.contours[0].points.resize(2);diagonal.contours[0].points[0].x.literal=0;diagonal.contours[0].points[0].y.literal=0;diagonal.contours[0].points[1].x.literal=20;diagonal.contours[0].points[1].y.literal=20;
 const auto root=std::sqrt(.5);diagonal.transform[0].literal=root;diagonal.transform[1].literal=root;diagonal.transform[2].literal=-root;diagonal.transform[3].literal=root;
 near(bounds(d,"a").left,0);near(bounds(d,"a").right,0);
 auto api=request(bad,R"({"op":"apply","expected_revision":0,"commands":[{"type":"align_objects","objects":["a","b"],"axis":"x","alignment":"center","artboard":null}]})");check(api.find("\"ok\":true")!=std::string::npos,"Semantic JSON API command");
 std::cout<<"Alignment axes/targets/transforms/atomicity/undo/native/API contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
