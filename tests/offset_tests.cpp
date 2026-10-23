#include "nect/core.hpp"
#include "../src/offset.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);++checks;}
void near(double a,double b,const char* why,double tolerance=1e-4){check(std::abs(a-b)<=tolerance,std::string(why)+": "+std::to_string(a)+" versus "+std::to_string(b));}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,"Expected "+std::string(code)+", got "+e.code+": "+e.what());return;}throw std::runtime_error("Expected "+std::string(code));}
Contour polygon(Id id,std::vector<Vec2> points,bool reverse=false) {
    if(reverse)std::reverse(points.begin(),points.end());Contour result;result.id=id;result.closed=true;
    for(const auto xy:points){Point p;p.id=id+"-p"+std::to_string(result.points.size());p.x.literal=xy.x;p.y.literal=xy.y;result.points.push_back(p);}return result;
}
Contour rectangle(Id id,double x=0,double y=0,double w=100,double h=60,bool reverse=false){return polygon(id,{{x,y},{x+w,y},{x+w,y+h},{x,y+h}},reverse);}
Document fixture() {
    auto d=empty_document("doc","comp","art");Object object;object.id="path";object.name="Rectangle";object.contours={rectangle("contour")};
    object.stack={default_operation("fill","nect.paint.fill")};d.objects.emplace(object.id,object);d.compositions[0].roots={object.id};return d;
}
void apply(Session& s,std::vector<Command> commands){s.apply(commands,s.revision());}
EvaluatedShape shape(const Document& d){return evaluate_shape(d,"path",evaluate(d));}
EvaluatedShape shape(const Session& s){return shape(s.document());}
ShapeOperation offset(double amount=10,std::string join="miter",std::string rule="nonzero") {
    auto op=default_operation("offset","nect.shape.offset");op.parameters.at("amount").literal=amount;op.line_join=join;op.fill_rule=rule;return op;
}
Bounds bounds(const std::vector<PathInstance>& paths,const Affine& basis=identity_matrix) {
    Bounds b{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()};
    for(const auto& instance:paths)for(const auto& c:*instance.contours)for(const auto& p:c.points){const auto v=map_point(compose(basis,instance.transform),p.anchor);b.left=std::min(b.left,v.x);b.top=std::min(b.top,v.y);b.right=std::max(b.right,v.x);b.bottom=std::max(b.bottom,v.y);}return b;
}
double area(const std::vector<PathInstance>& paths) {
    double result=0;for(const auto& path:paths)for(const auto& c:*path.contours)for(std::size_t i=0;i<c.points.size();++i) {
        const auto a=map_point(path.transform,c.points[i].anchor),b=map_point(path.transform,c.points[(i+1)%c.points.size()].anchor);result+=(a.x*b.y-a.y*b.x)/2;
    }return std::abs(result);
}
std::size_t points(const std::vector<PathInstance>& paths){std::size_t count=0;for(const auto& p:paths)for(const auto& c:*p.contours)count+=c.points.size();return count;}
void same_geometry(const std::vector<PathInstance>& a,const std::vector<PathInstance>& b,const Affine& basis=identity_matrix) {
    check(a.size()==b.size(),"Same instance count");
    for(std::size_t i=0;i<a.size();++i){check(a[i].contours->size()==b[i].contours->size(),"Same contour count");
        for(std::size_t j=0;j<a[i].contours->size();++j){const auto& ca=a[i].contours->at(j);const auto& cb=b[i].contours->at(j);check(ca.closed==cb.closed&&ca.points.size()==cb.points.size(),"Same closed output topology");
            for(std::size_t k=0;k<ca.points.size();++k){const auto va=map_point(a[i].transform,ca.points[k].anchor),vb=map_point(compose(basis,b[i].transform),cb.points[k].anchor);near(va.x,vb.x,"Output X",1e-8);near(va.y,vb.y,"Output Y",1e-8);}
        }
    }
}
void atomic(Session& s,const char* code,std::vector<Command> commands){const auto d=s.document();const auto r=s.revision();const auto h=s.history();rejects(code,[&]{apply(s,std::move(commands));});check(s.document()==d&&s.revision()==r&&s.history()==h,"Offset failure preserves authored state, revision and history");}

void rectangle_joins_and_signed_amount() {
    Session s(fixture());apply(s,{AddOperation{"path",offset(),1}});auto result=shape(s);const auto b=bounds(result.paths);
    near(b.left,-10,"Expand left");near(b.top,-10,"Expand top");near(b.right,110,"Expand right");near(b.bottom,70,"Expand bottom");near(area(result.paths),9600,"Miter rectangle area");
    same_geometry(result.paths,result.paints[0].paths,result.paints[0].transform);
    check(result.paths[0].contours==result.paints[0].paths[0].contours,"Current geometry and preceding Fill reuse one offset projection");
    apply(s,{Set{operation_ref("path","offset","amount"),-10}});near(area(shape(s).paths),3200,"Inward rectangle area");
    apply(s,{Set{operation_ref("path","offset","amount"),-100}});check(points(shape(s).paths)==0,"Complete erosion is empty valid output");
    auto again=offset(5);again.id="offset-again";apply(s,{AddOperation{"path",again,2}});check(points(shape(s).paths)==0,"Later offset preserves completely eroded geometry");
    apply(s,{RemoveOperation{"path","offset-again"},Set{operation_ref("path","offset","amount"),10},OperationOptions{"path","offset","below","nonzero","bevel"}});
    near(area(shape(s).paths),9400,"Beveled rectangle area");
    apply(s,{OperationOptions{"path","offset","below","nonzero","miter"},Set{operation_ref("path","offset","miter_limit"),1}});
    near(area(shape(s).paths),9400,"Exceeded miter ratio falls back to the actual bevel");
    apply(s,{OperationOptions{"path","offset","below","nonzero","round"}});
    near(area(shape(s).paths),9200+100*std::numbers::pi,"Round corners approximate quarter circles",5);
    apply(s,{OperationOptions{"path","offset","below","evenodd"}});check(s.document().objects.at("path").stack[1].line_join=="round","Older option callers preserve existing join");
}
void holes_winding_and_invalid_boundaries() {
    auto d=fixture();d.objects.at("path").contours={rectangle("outer",0,0,100,100),rectangle("hole",30,30,40,40,true)};d.objects.at("path").stack.push_back(offset(5));
    Session s(d);near(area(shape(s).paths),12100-900,"Positive Offset expands outer region and shrinks its hole");
    apply(s,{Set{operation_ref("path","offset","amount"),-5}});near(area(shape(s).paths),8100-2500,"Negative Offset shrinks outer region and expands its hole");
    d.objects.at("path").contours[1]=rectangle("hole",30,30,40,40);Session same(d);near(area(shape(same).paths),12100,"Nonzero same-winding inner ring is redundant");
    apply(same,{OperationOptions{"path","offset","below","evenodd"}});near(area(shape(same).paths),12100-900,"Evenodd keeps a same-winding hole");
    d.objects.at("path").contours.push_back(rectangle("island",150,0,20,20));Session separate(d);near(area(shape(separate).paths),12100+900,"Disjoint rings retain independent regions within one instance");
    d.objects.at("path").contours[1]=rectangle("hole",0,30,40,40);rejects("OFFSET_GEOMETRY",[&]{Session invalid(d);});
    d.objects.at("path").contours[1]=rectangle("hole",90,30,40,40);rejects("OFFSET_GEOMETRY",[&]{Session invalid(d);});
    d.objects.at("path").contours={polygon("cross",{{0,0},{100,100},{0,100},{100,0}})};rejects("OFFSET_GEOMETRY",[&]{Session invalid(d);});
    d.objects.at("path").contours={rectangle("open")};d.objects.at("path").contours[0].closed=false;rejects("OFFSET_OPEN_PATH",[&]{Session invalid(d);});
    d.objects.at("path").stack.back().parameters.at("amount").literal=0;Session zero(d);check(points(shape(zero).paths)==4&&!shape(zero).paths[0].contours->front().closed,"Amount zero is exact no-op even for unsupported open input");
}
void paint_order_basis_and_repeat_space() {
    auto d=fixture();d.objects.at("path").stack.push_back(offset());Session a(d);auto before=shape(a);
    apply(a,{ReorderOperations{"path",{"offset","fill"}}});auto after=shape(a);same_geometry(before.paths,after.paths);same_geometry(before.paints[0].paths,after.paints[0].paths);
    auto& object=d.objects.at("path");auto stroke=default_operation("stroke","nect.paint.stroke");stroke.parameters.at("width").literal=7;
    Gradient gradient;gradient.id="gradient";GradientStop first;first.id="stop-a";GradientStop last;last.id="stop-b";last.offset.literal=1;last.rgba[0].literal=1;gradient.stops={first,last};stroke.gradient=gradient;
    auto repeat=default_operation("repeat","nect.shape.repeater");repeat.parameters.at("copies").literal=2;repeat.parameters.at("position_x").literal=200;
    repeat.parameters.at("scale_x").literal=2;repeat.parameters.at("scale_y").literal=.5;
    object.stack={stroke,repeat,offset()};Session s(d);auto result=shape(s);check(result.paints.size()==2&&result.paths.size()==2,"Offset never unions separate Repeater instances");
    const auto b=bounds({result.paths[1]});near(b.left,190,"Repeat then Offset uses object-local distance X");near(b.top,-10,"Repeat then Offset uses object-local distance Y");near(b.right,410,"Repeated right offset");near(b.bottom,40,"Repeated bottom offset");
    for(const auto& paint:result.paints){check(paint.width==7&&paint.gradient.has_value(),"Stroke width and gradient survive Offset");near(paint.gradient->start.x,0,"Gradient coordinates stay in retained paint basis");near(paint.gradient->end.x,100,"Gradient end stays in retained paint basis");
        const auto index=paint.transform[4]>0?1u:0u;check(paint.paths[0].transform==identity_matrix,"Inverse-pulled contours use identity instance transform");same_geometry({result.paths[index]},paint.paths,paint.transform);
    }
    check(result.paints[0].transform==Affine{2,0,0,.5,200,0},"Nonuniform repeated paint transform remains exact");
    apply(s,{Set{operation_ref("path","repeat","position_x"),10}});check(shape(s).paths.size()==2,"Overlapping Repeater instances remain separate Offset regions");
    apply(s,{Set{operation_ref("path","repeat","position_x"),200}});
    apply(s,{ReorderOperations{"path",{"stroke","offset","repeat"}}});result=shape(s);const auto other=bounds({result.paths[1]});near(other.left,180,"Offset then Repeat scales X expansion");near(other.top,-5,"Offset then Repeat scales Y expansion");
    auto singular=result;singular.paints[0].transform={0,0,0,1,0,0};rejects("SINGULAR_TRANSFORM",[&]{apply_offset(singular,10,4,"miter","nonzero");});
    auto exact=singular;apply_offset(exact,0,4,"miter","nonzero");check(exact.paths[0].contours==singular.paths[0].contours,"Zero amount needs no inverse and retains exact derived contours");
}
void sources_links_history_and_limits() {
    Session s(empty_document("doc","comp","art"));auto circle=default_primitive("source","nect.shape.circle");circle.parameters.at("radius").literal=50;
    apply(s,{CreatePrimitive{"comp","","path","Circle",circle}});const auto initial=s.document();const auto roles=path_contours(initial.objects.at("path"));
    apply(s,{AddOperation{"path",offset(10,"round"),1}});const auto b=bounds(shape(s).paths);near(b.left,-60,"Closed cubic Circle expands left",.15);near(b.right,60,"Closed cubic Circle expands right",.15);
    const Ref east{"path","source-east","x"};apply(s,{Set{east,55}});const auto corrected=s.document();
    apply(s,{Link{operation_ref("path","offset","amount"),{{"path","","generator.radius"},.2,0,"copy_local_value"}}});near(evaluate(s.document()).at(operation_ref("path","offset","amount")),10,"Amount uses ordinary distance links");
    atomic(s,"DRIVEN_PROPERTY",{Set{operation_ref("path","offset","amount"),8}});
    atomic(s,"CONVERSION_REFERENCE",{ConvertToPath{"path"}});
    apply(s,{Unlink{operation_ref("path","offset","amount")},SetExpression{{operation_ref("path","offset","amount")},{"5 + 5"}}});
    atomic(s,"UNIT_MISMATCH",{SetExpression{{operation_ref("path","offset","amount")},{"ref(\"path\",\"\",\"generator.rotation\")"}}});
    apply(s,{ConvertToPath{"path"}});check(s.document().objects.at("path").stack.back().id=="offset"&&s.document().objects.at("path").contours[0].id==roles[0].id,"Source conversion preserves Offset and stable source roles");
    for(std::size_t i=0;i<4;++i)check(s.document().objects.at("path").contours[0].points[i].id==roles[0].points[i].id,"No output vertex becomes an authored source ID");
    s.undo(s.revision());check(s.document().objects.at("path").source.has_value()&&s.document().objects.at("path").point_edit==corrected.objects.at("path").point_edit,"Undo restores generator and Point Edit exactly");
    auto open=fixture();open.objects.at("path").contours[0].closed=false;auto disabled=offset();disabled.enabled=false;open.objects.at("path").stack.push_back(disabled);Session bypass(open);
    atomic(bypass,"OFFSET_OPEN_PATH",{EnableOperation{"path","offset",true}});
    atomic(bypass,"OUT_OF_RANGE",{Set{operation_ref("path","offset","amount"),1000001}});
    atomic(bypass,"OUT_OF_RANGE",{Set{operation_ref("path","offset","miter_limit"),.5}});
    atomic(bypass,"INVALID_OPERATOR_OPTIONS",{OperationOptions{"path","offset","above","nonzero"}});
    atomic(bypass,"INVALID_OPERATOR_OPTIONS",{OperationOptions{"path","offset","below","nonzero","future"}});
    atomic(bypass,"INVALID_OPERATOR_OPTIONS",{OperationOptions{"path","fill","below","nonzero","round"}});
    auto repeated=fixture();auto repeat=default_operation("repeat","nect.shape.repeater");repeat.parameters.at("copies").literal=1000;
    repeated.objects.at("path").stack={offset(1000000,"round"),default_operation("fill","nect.paint.fill"),repeat};
    rejects("OUTPUT_LIMIT",[&]{Session excessive(repeated);});
    Session gesture(fixture());gesture.begin_gesture(0);gesture.update_gesture({AddOperation{"path",offset(),1}});const auto preview=gesture.preview_document();
    rejects("OUT_OF_RANGE",[&]{gesture.update_gesture({AddOperation{"path",offset(1000001),1}});});check(gesture.preview_document()==preview&&gesture.revision()==0,"Failed Offset preview retains last valid preview");
    gesture.cancel_gesture();check(!gesture.can_undo()&&gesture.document()==fixture(),"Canceled Offset gesture leaves no history");
}
}
int main(){try{rectangle_joins_and_signed_amount();holes_winding_and_invalid_boundaries();paint_order_basis_and_repeat_space();sources_links_history_and_limits();std::cout<<"PASS "<<checks<<" retained local Offset checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
