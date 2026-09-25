#include "nect/io.hpp"
#include <cmath>
#include <iostream>
#include <limits>
using namespace nect;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void near(double a,double b){check(std::abs(a-b)<1e-7,"Unexpected aligned coordinate");}
Object rectangle(const Id& id,double x,double y,double w,double h){
 Object o;o.id=id;o.name=id;Contour c;c.id=id+"-contour";c.closed=true;
 for(auto p:std::vector<Vec2>{{x,y},{x+w,y},{x+w,y+h},{x,y+h}}){Point q;q.id=id+std::to_string(c.points.size());q.x.literal=p.x;q.y.literal=p.y;c.points.push_back(q);}
 o.contours.push_back(c);return o;
}
Document fixture(){auto d=empty_document("doc","comp","art");d.objects.emplace("a",rectangle("a",10,20,20,30));d.objects.emplace("b",rectangle("b",100,140,50,20));d.compositions[0].roots={"a","b"};return d;}
Document exact_fixture(){
 auto d=empty_document("doc","comp","art");d.objects.emplace("a",rectangle("a",0,0,10,10));
 d.objects.emplace("b",rectangle("b",30,0,20,10));d.objects.emplace("c",rectangle("c",90,0,10,10));d.compositions[0].roots={"a","b","c"};
 auto& board=d.compositions[0].artboards[0];board.width=960;board.height=480;
 board.layout=ArtboardLayout{std::nullopt,Grid{"grid",{40,0,100,100},1,1,0,0}};
 d.compositions[0].guides={{"guide-x","Vertical guide","x",100},{"guide-y","Horizontal guide","y",200}};
 return d;
}
Document text_fixture(){
 auto d=empty_document("text-doc","text-comp","text-art");
 for(const auto& [id,y]:std::vector<std::pair<Id,double>>{{"text-a",0},{"text-b",20}}) {
  Object object;object.id=id;object.name=id;object.kind=Kind::text;object.text=default_text(id+"-source","Nect baseline");object.transform[5].literal=y;
  d.objects.emplace(id,std::move(object));d.compositions[0].roots.push_back(id);
 }
 return d;
}
Bounds bounds(const Document& d,const Id& id){const auto v=evaluate(d);return *object_bounds(d,id,v,evaluate_transforms(d,v),true);}
void apply(Session& s,AlignObjects c){s.apply({c},s.revision());}
void apply(Session& s,DistributeObjects c){s.apply({c},s.revision());}
void rejects(Session& s,AlignObjects c,const std::string& code){auto before=s.document();auto revision=s.revision();try{apply(s,c);throw std::runtime_error("Expected failure: "+code);}catch(const Error& e){check(e.code==code,e.what());}check(s.document()==before&&s.revision()==revision,"Failure must be atomic");}
void rejects(Session& s,DistributeObjects c,const std::string& code){auto before=s.document();auto revision=s.revision();const auto history=s.history().states.size();try{apply(s,c);throw std::runtime_error("Expected failure: "+code);}catch(const Error& e){check(e.code==code,e.what());}check(s.document()==before&&s.revision()==revision&&s.history().states.size()==history,"Distribution failure must preserve document and history");}
std::string api_code(const std::string& response){const auto start=response.find("\"code\":\"");if(start==std::string::npos)return {};const auto value=start+8;const auto end=response.find('"',value);return response.substr(value,end-value);}
int main(){try{
 for(auto axis:{"x","y"})for(auto alignment:{"min","center","max"}){
  auto d=fixture();Session s(d);apply(s,AlignObjects{{"a","b"},axis,alignment,{}});const auto a=bounds(s.document(),"a"),b=bounds(s.document(),"b");
  auto coordinate=[&](Bounds r){auto lo=std::string(axis)=="x"?r.left:r.top,hi=std::string(axis)=="x"?r.right:r.bottom;return std::string(alignment)=="min"?lo:std::string(alignment)=="max"?hi:(lo+hi)/2;};near(coordinate(a),coordinate(b));
  check(s.document().objects.at("a").contours==d.objects.at("a").contours,"Authored geometry unchanged");
  auto aligned=s.document();check(decode(encode(aligned))==aligned,"Native roundtrip");s.undo(s.revision());check(s.document()==d,"One Undo exact restoration");s.redo(s.revision());check(s.document()==aligned,"Redo restores alignment");
 }
 auto d=fixture();d.compositions[0].artboards[0].x=50;d.compositions[0].artboards[0].y=80;d.compositions[0].artboards[0].width=200;d.compositions[0].artboards[0].height=100;
 Session art(d);apply(art,AlignObjects{{"a"},"x","center","art"});near((bounds(art.document(),"a").left+bounds(art.document(),"a").right)/2,150);
 // Effective followers must receive independently solved target displacements.
 d=fixture();d.objects.at("b").transform_parent="a";Session follower(d);apply(follower,AlignObjects{{"b","a"},"x","min",{}});near(bounds(follower.document(),"a").left,10);near(bounds(follower.document(),"b").left,10);
 // Rotation is retained; extrema are computed in world space, not by rotating a local AABB.
 d=fixture();d.objects.at("b").transform[0].literal=0;d.objects.at("b").transform[1].literal=1;d.objects.at("b").transform[2].literal=-1;d.objects.at("b").transform[3].literal=0;
 Session rotated(d);apply(rotated,AlignObjects{{"a","b"},"y","max",{}});near(bounds(rotated.document(),"a").bottom,150);near(bounds(rotated.document(),"b").bottom,150);check(rotated.document().objects.at("b").transform[1].literal==1,"Rotation preserved");
 d=fixture();d.objects.at("b").transform[4].binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};Session driven(d);rejects(driven,AlignObjects{{"a","b"},"x","min",{}},"DRIVEN_PROPERTY");
 Session bad(fixture());rejects(bad,AlignObjects{{"a","a"},"x","min",{}},"DUPLICATE_TARGET");rejects(bad,AlignObjects{{"a","b"},"z","min",{}},"INVALID_ALIGNMENT");rejects(bad,AlignObjects{{"a"},"x","min",{}},"INVALID_BATCH");rejects(bad,AlignObjects{{"a"},"x","min","missing"},"MISSING_ARTBOARD");
 d=fixture();Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"a"};d.objects.emplace("group",group);d.compositions[0].roots={"group","b"};Session grouped(d);apply(grouped,AlignObjects{{"group","b"},"x","max",{}});near(bounds(grouped.document(),"a").right,150);rejects(grouped,AlignObjects{{"group","a"},"x","min",{}},"OVERLAPPING_SELECTION");
 d=fixture();d.objects.at("b").transform_parent="a";Session follower_max(d);apply(follower_max,AlignObjects{{"b","a"},"x","max",{}});near(bounds(follower_max.document(),"a").right,150);near(bounds(follower_max.document(),"b").right,150);
 d=fixture();auto& diagonal=d.objects.at("a");diagonal.contours[0].closed=false;diagonal.contours[0].points.resize(2);diagonal.contours[0].points[0].x.literal=0;diagonal.contours[0].points[0].y.literal=0;diagonal.contours[0].points[1].x.literal=20;diagonal.contours[0].points[1].y.literal=20;
 const auto root=std::sqrt(.5);diagonal.transform[0].literal=root;diagonal.transform[1].literal=root;diagonal.transform[2].literal=-root;diagonal.transform[3].literal=root;
 near(bounds(d,"a").left,0);near(bounds(d,"a").right,0);
 auto api=request(bad,R"({"op":"apply","expected_revision":0,"commands":[{"type":"align_objects","objects":["a","b"],"axis":"x","alignment":"center","artboard":null}]})");check(api.find("\"ok\":true")!=std::string::npos,"Semantic JSON API command");
 // Unequal widths, shuffled selection order and both axes preserve outer positions.
 for(const auto axis:{"x","y"}) {
  d=fixture();d.objects.emplace("c",rectangle("c",240,260,10,10));d.compositions[0].roots.push_back("c");
  Session spacing(d);spacing.apply({DistributeObjects{{"c","a","b"},axis}},0);
  const auto a=bounds(spacing.document(),"a"),b=bounds(spacing.document(),"b"),c=bounds(spacing.document(),"c");
  if(std::string(axis)=="x"){near(a.left,10);near(c.left,240);near(b.left-a.right,c.left-b.right);near(b.top,140);}
  else {near(a.top,20);near(c.top,260);near(b.top-a.bottom,c.top-b.bottom);near(b.left,100);}
  check(spacing.document().objects.at("b").contours==d.objects.at("b").contours,"Spacing retains geometry");
  const auto result=spacing.document();check(decode(encode(result))==result,"Spacing native roundtrip");spacing.undo(1);check(spacing.document()==d,"Spacing one Undo");spacing.redo(2);check(spacing.document()==result,"Spacing Redo");
 }
 d=fixture();d.objects.emplace("c",rectangle("c",240,260,10,10));d.compositions[0].roots.push_back("c");
 d.objects.at("b").transform_parent="a";Session spaced_follower(d);spaced_follower.apply({DistributeObjects{{"b","c","a"},"x"}},0);near(bounds(spaced_follower.document(),"b").left,110);
 d.objects.at("b").transform[4].binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};
 Session spaced_driven(d);try{spaced_driven.apply({DistributeObjects{{"a","b","c"},"x"}},0);throw std::runtime_error("Expected driven spacing failure");}catch(const Error& e){check(e.code=="DRIVEN_PROPERTY","Spacing driven rejection");}check(spaced_driven.document()==d&&spaced_driven.revision()==0,"Spacing failure atomic");
 d=fixture();d.objects.emplace("c",rectangle("c",105,260,10,10));d.compositions[0].roots.push_back("c");Session overlap(d);
 auto spacing_api=request(overlap,R"({"op":"apply","expected_revision":0,"commands":[{"type":"distribute_objects","objects":["a","b","c"],"axis":"x"}]})");check(spacing_api.find("OVERLAPPING_BOUNDS")!=std::string::npos&&overlap.document()==d,"Overlapping spacing rejects through API atomically");
 spacing_api=request(overlap,R"({"op":"apply","expected_revision":0,"commands":[{"type":"distribute_objects","objects":["c","a","b"],"axis":"y"}]})");check(spacing_api.find("\"ok\":true")!=std::string::npos,"Spacing JSON API");
 // Frozen P02-C2 geometric targets and identity oracles.
 d=exact_fixture();const auto key_before=d.objects.at("b");Session key_align(d);
 AlignObjects key_align_command{{"a","b","c"},"x","min",{}};key_align_command.reference="key_object:b";apply(key_align,key_align_command);
 near(bounds(key_align.document(),"a").left,30);near(bounds(key_align.document(),"b").left,30);near(bounds(key_align.document(),"c").left,30);
 check(key_align.document().objects.at("b")==key_before,"Selected key object remains byte-identical during Align");
 d=exact_fixture();Session grid_align(d);AlignObjects grid_command{{"a"},"x","min",{}};grid_command.reference="grid:grid";apply(grid_align,grid_command);
 near(bounds(grid_align.document(),"a").left,40); // Grid coordinate includes its owning Artboard origin.
 d=exact_fixture();Session artboard_center(d);AlignObjects artboard_command{{"a"},"x","center","art"};apply(artboard_center,artboard_command);
 near((bounds(artboard_center.document(),"a").left+bounds(artboard_center.document(),"a").right)/2,480);
 d=exact_fixture();Session guide_align(d);AlignObjects guide_command{{"a"},"x","min",{}};guide_command.reference="guide:guide-x";apply(guide_align,guide_command);
 near(bounds(guide_align.document(),"a").left,100);
 rejects(guide_align,AlignObjects{{"a"},"x","min",{},"guide:guide-y"},"GUIDE_AXIS_MISMATCH");
 rejects(guide_align,AlignObjects{{"a"},"x","min",{},"grid:missing"},"MISSING_GRID");
 rejects(guide_align,AlignObjects{{"a"},"x","min",{},"artboard:missing"},"MISSING_ARTBOARD");
 rejects(guide_align,AlignObjects{{"a"},"x","min",{},"unknown:target"},"INVALID_REFERENCE");
 rejects(guide_align,AlignObjects{{"missing"},"x","min",{},"artboard:art"},"MISSING_OBJECT");
 // Selection distribution fixes its outer objects; explicit key spacing packs outwards.
 d=exact_fixture();Session key_spacing30(d);const auto key_spacing_before=key_spacing30.document().objects.at("b");
 apply(key_spacing30,DistributeObjects{{"a","b","c"},"x","key_object:b",30});
 near(bounds(key_spacing30.document(),"a").left,-10);near(bounds(key_spacing30.document(),"b").left,30);near(bounds(key_spacing30.document(),"c").left,80);
 check(key_spacing30.document().objects.at("b")==key_spacing_before,"Selected key object remains byte-identical during distribution");
 d=exact_fixture();Session key_spacing15(d);apply(key_spacing15,DistributeObjects{{"a","b","c"},"x","key_object:b",15});
 near(bounds(key_spacing15.document(),"a").left,5);near(bounds(key_spacing15.document(),"b").left,30);near(bounds(key_spacing15.document(),"c").left,65);
 d=exact_fixture();Session selection_gaps(d);apply(selection_gaps,DistributeObjects{{"c","a","b"},"x"});
 near(bounds(selection_gaps.document(),"a").left,0);near(bounds(selection_gaps.document(),"b").left,40);near(bounds(selection_gaps.document(),"c").left,90);
 d=exact_fixture();Session grid_gaps(d);apply(grid_gaps,DistributeObjects{{"c","a","b"},"x","grid:grid"});
 near(bounds(grid_gaps.document(),"a").left,55);near(bounds(grid_gaps.document(),"b").left,80);near(bounds(grid_gaps.document(),"c").left,115);
 d=exact_fixture();d.compositions[0].artboards[0].width=100;d.compositions[0].artboards[0].layout.reset();
 Session artboard_gaps(d);apply(artboard_gaps,DistributeObjects{{"c","a","b"},"x","artboard:art"});
 near(bounds(artboard_gaps.document(),"a").left,15);near(bounds(artboard_gaps.document(),"b").left,40);near(bounds(artboard_gaps.document(),"c").left,75);
 rejects(selection_gaps,DistributeObjects{{"a","b","c"},"x","guide:guide-x"},"UNSUPPORTED_DISTRIBUTION_REFERENCE");
 rejects(selection_gaps,DistributeObjects{{"a","b"},"x","key_object:b"},"MISSING_SPACING");
 rejects(selection_gaps,DistributeObjects{{"a","b"},"x","key_object:b",-1},"NEGATIVE_SPACING");
 rejects(selection_gaps,DistributeObjects{{"a","b"},"x","key_object:b",std::numeric_limits<double>::quiet_NaN()},"NON_FINITE");
 rejects(selection_gaps,DistributeObjects{{"a","b","c"},"x","key_object:missing",10},"KEY_OBJECT_NOT_SELECTED");
 d=exact_fixture();d.objects.at("c").transform[4].literal=-60;Session key_overlap(d);
 rejects(key_overlap,DistributeObjects{{"a","b","c"},"x","key_object:b",5},"OVERLAPPING_BOUNDS");
 d=exact_fixture();d.compositions[0].artboards[0].layout->grid->bounds.width=39;Session small_grid(d);
 rejects(small_grid,DistributeObjects{{"a","b","c"},"x","grid:grid"},"REFERENCE_SPAN_TOO_SMALL");
 d=exact_fixture();d.objects.emplace("empty",Object{});d.objects.at("empty").id="empty";d.compositions[0].roots.push_back("empty");Session empty_geometry(d);
 rejects(empty_geometry,AlignObjects{{"empty"},"x","min",{},"artboard:art"},"EMPTY_BOUNDS");
 // A geometry dependency that defeats the requested translation is rejected before commit.
 d=exact_fixture();d.objects.at("b").contours[0].points[0].x.binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};Session preservation(d);
 rejects(preservation,AlignObjects{{"a","b"},"x","center","art"},"ALIGNMENT_PRESERVATION");
 // References in other Compositions are distinguished from missing identities.
 d=exact_fixture();Composition other;other.id="comp2";other.name="Other";other.roots={"other"};other.artboards={{"art2","Other Artboard",0,0,100,100}};
 other.artboards[0].layout=ArtboardLayout{std::nullopt,Grid{"grid2",{0,0,80,80},1,1,0,0}};other.guides={{"guide2","Other Guide","x",50}};
 d.objects.emplace("other",rectangle("other",0,0,10,10));d.compositions.push_back(other);Session cross_plane(d);
 rejects(cross_plane,AlignObjects{{"a","other"},"x","min",{}},"CROSS_COMPOSITION");
 rejects(cross_plane,AlignObjects{{"a"},"x","min",{},"artboard:art2"},"CROSS_COMPOSITION");
 rejects(cross_plane,AlignObjects{{"a"},"x","min",{},"grid:grid2"},"CROSS_COMPOSITION");
 rejects(cross_plane,AlignObjects{{"a"},"x","min",{},"guide:guide2"},"CROSS_COMPOSITION");
 // First-line baselines use DirectWrite metrics, not glyph-box bottoms.
 d=text_fixture();
 auto baseline=[&](const Document& document,const Id& id) {
  const auto values=evaluate(document);std::map<std::string,double> params;
  for(const auto& [name,value]:document.objects.at(id).text->parameters){(void)value;params.emplace(name,values.at({id,"","text."+name}));}
  const auto metric=evaluate_text(*document.objects.at(id).text,params).first_line_baseline_y;
  check(metric.has_value(),"Fixture exposes a first-line metric");const auto& world=evaluate_transforms(document,values).at(id).world;
  return world[3]*(*metric)+world[5];
 };
 near(baseline(d,"text-b")-baseline(d,"text-a"),20);
 const auto baseline_source_before=d.objects.at("text-a");Session baseline_selection(d);
 apply(baseline_selection,AlignObjects{{"text-a","text-b"},"y","baseline",{},"selection"});
 check(baseline_selection.document().objects.at("text-a")==baseline_source_before,"Selection baseline keeps minimum-y source authored state fixed");
 near(baseline(baseline_selection.document(),"text-a"),baseline(baseline_selection.document(),"text-b"));
 d=text_fixture();const auto baseline_key_before=d.objects.at("text-b");Session baseline_key(d);
 apply(baseline_key,AlignObjects{{"text-a","text-b"},"y","baseline",{},"key_object:text-b"});
 check(baseline_key.document().objects.at("text-b")==baseline_key_before,"Key baseline keeps key Text byte-identical");
 near(baseline(baseline_key.document(),"text-a"),baseline(baseline_key.document(),"text-b"));
 d=text_fixture();d.objects.at("text-b").text->direction="vertical";Session vertical_text(d);
 rejects(vertical_text,AlignObjects{{"text-a","text-b"},"y","baseline",{},"selection"},"UNSUPPORTED_BASELINE");
 rejects(baseline_key,AlignObjects{{"text-a","text-b"},"y","baseline",{},"grid:grid"},"UNSUPPORTED_BASELINE_REFERENCE");
 // No-op arrangement does not consume a revision/history state; stale commands are atomic.
 d=exact_fixture();Session no_op(d);AlignObjects guide_no_op{{"a"},"x","min",{},"guide:guide-x"};apply(no_op,guide_no_op);
 const auto no_op_revision=no_op.revision();const auto no_op_history=no_op.history().states.size();const auto no_op_document=no_op.document();apply(no_op,guide_no_op);
 check(no_op.revision()==no_op_revision&&no_op.history().states.size()==no_op_history&&no_op.document()==no_op_document,"Layout no-op adds no revision or history entry");
 try{no_op.apply({guide_no_op},0);throw std::runtime_error("Expected stale layout rejection");}catch(const Error& e){check(e.code=="REVISION_CONFLICT","Stale layout command code");}
 check(no_op.revision()==no_op_revision&&no_op.history().states.size()==no_op_history&&no_op.document()==no_op_document,"Stale layout rejection preserves document and history");
 // JSON command path covers explicit references and legacy aliases/defaults.
 auto json_ok=[](const std::string& value){return value.find("\"ok\":true")!=std::string::npos;};
 auto json_reference=[&](const std::string& command){Session session(exact_fixture());return request(session,"{\"op\":\"apply\",\"expected_revision\":0,\"commands\":["+command+"]}");};
 check(json_ok(json_reference(R"({"type":"align_objects","objects":["a","b","c"],"axis":"x","alignment":"min","reference":"key_object:b"})")),"JSON key-object Align reference");
 check(json_ok(json_reference(R"({"type":"align_objects","objects":["a"],"axis":"x","alignment":"min","reference":"artboard:art"})")),"JSON Artboard Align reference");
 check(json_ok(json_reference(R"({"type":"align_objects","objects":["a"],"axis":"x","alignment":"min","reference":"grid:grid"})")),"JSON Grid Align reference");
 check(json_ok(json_reference(R"({"type":"align_objects","objects":["a"],"axis":"x","alignment":"min","reference":"guide:guide-x"})")),"JSON Guide Align reference");
 check(json_ok(json_reference(R"({"type":"align_objects","objects":["a","b"],"axis":"x","alignment":"min"})")),"JSON legacy no-reference Align defaults to selection");
 check(json_ok(json_reference(R"({"type":"align_objects","objects":["a"],"axis":"x","alignment":"center","artboard":"art"})")),"JSON legacy Artboard alias remains accepted");
 check(json_ok(json_reference(R"({"type":"distribute_objects","objects":["a","b","c"],"axis":"x"})")),"JSON legacy no-reference Distribute defaults to selection");
 check(json_ok(json_reference(R"({"type":"distribute_objects","objects":["a","b","c"],"axis":"x","reference":"key_object:b","spacing":30})")),"JSON key-object Distribute spacing");
 check(json_ok(json_reference(R"({"type":"distribute_objects","objects":["a"],"axis":"x","reference":"artboard:art"})")),"JSON Artboard Distribute reference");
 check(json_ok(json_reference(R"({"type":"distribute_objects","objects":["a"],"axis":"x","reference":"grid:grid"})")),"JSON Grid Distribute reference");
 Session alias_conflict(exact_fixture());const auto alias_before=alias_conflict.document();
 const auto alias_error=request(alias_conflict,R"({"op":"apply","expected_revision":0,"commands":[{"type":"align_objects","objects":["a"],"axis":"x","alignment":"min","artboard":null,"reference":"selection"}]})");
 check(api_code(alias_error)=="INVALID_REFERENCE"&&alias_conflict.document()==alias_before&&alias_conflict.revision()==0,"JSON rejects simultaneous alias and explicit reference atomically");
 Session json_baseline(text_fixture());const auto baseline_api=request(json_baseline,R"({"op":"apply","expected_revision":0,"commands":[{"type":"align_objects","objects":["text-a","text-b"],"axis":"y","alignment":"baseline","reference":"selection"}]})");
 check(json_ok(baseline_api)&&baseline_api.find("\"revision\":1")!=std::string::npos,"JSON API uses the actual baseline Session command");
 Session guide_distribution(exact_fixture());const auto guide_error=request(guide_distribution,R"({"op":"apply","expected_revision":0,"commands":[{"type":"distribute_objects","objects":["a"],"axis":"x","reference":"guide:guide-x"}]})");
 check(api_code(guide_error)=="UNSUPPORTED_DISTRIBUTION_REFERENCE"&&guide_distribution.revision()==0,"JSON Guide Distribute has explicit unsupported error");
 Session stale_api(exact_fixture());const auto first_apply=request(stale_api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"align_objects","objects":["a"],"axis":"x","alignment":"min","reference":"guide:guide-x"}]})");
 check(json_ok(first_apply)&&stale_api.revision()==1,"JSON setup command reaches revision one");const auto stale_before=stale_api.document();
 const auto stale_error=request(stale_api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"align_objects","objects":["a"],"axis":"x","alignment":"min","reference":"guide:guide-x"}]})");
 check(api_code(stale_error)=="REVISION_CONFLICT"&&stale_api.document()==stale_before&&stale_api.revision()==1,"JSON stale command rejects without mutation");
 const auto no_op_api=request(stale_api,R"({"op":"apply","expected_revision":1,"commands":[{"type":"align_objects","objects":["a"],"axis":"x","alignment":"min","reference":"guide:guide-x"}]})");
 check(json_ok(no_op_api)&&no_op_api.find("\"revision\":1")!=std::string::npos,"JSON layout no-op retains revision");
 std::cout<<"Alignment axes/targets/transforms/atomicity/undo/native/API contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
