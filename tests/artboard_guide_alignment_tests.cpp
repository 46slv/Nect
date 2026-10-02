#include "artboard_guide_alignment_fixture.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void near(double a,double b){check(std::abs(a-b)<1e-8,"unexpected alignment coordinate");}
void apply(Session& s,std::vector<Command> cs){s.apply(cs,s.revision());}
Command align(const Id& board,const Id& guide="GX",std::string mode="min",std::string axis="x"){
    return AlignObjects{{"rect"},axis,mode,{},"guide:"+guide,board};
}
void atomic(Session& s,std::vector<Command> cs,const char* code=nullptr){
    const auto bytes=encode(s.document());auto rev=s.revision();auto history=s.history();bool rejected=false;
    try{apply(s,cs);}catch(const Error& e){rejected=true;if(code)check(e.code==code,"wrong rejection code");}
    check(rejected&&encode(s.document())==bytes&&s.revision()==rev&&s.history()==history,"failed alignment changes state");
}
void lifecycle(){
    Session s(guide_alignment_fixture::document());const auto original=encode(s.document());
    apply(s,{align("A")});near(guide_alignment_fixture::bounds(s.document()).left,1090);
    auto after=encode(s.document());s.undo(s.revision());check(encode(s.document())==original,"Undo restores exact original");
    s.redo(s.revision());check(encode(s.document())==after,"Redo exact state");
    apply(s,{align("B")});near(guide_alignment_fixture::bounds(s.document()).left,2040);
    apply(s,{ArtboardGuideCommand{UpdateArtboardGuide{"comp","source",{"GX","Renamed","x",50,true}}}});
    apply(s,{align("A")});near(guide_alignment_fixture::bounds(s.document()).left,1090);
    apply(s,{align("B")});near(guide_alignment_fixture::bounds(s.document()).left,2050);
    apply(s,{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","A","GX","position"}},align("A")});
    near(guide_alignment_fixture::bounds(s.document()).left,1050);
    auto a=s.document().compositions.front().artboards[1];a.x=1100;
    apply(s,{UpdateArtboard{"comp",a}});near(guide_alignment_fixture::bounds(s.document()).left,1050);
    apply(s,{align("A")});near(guide_alignment_fixture::bounds(s.document()).left,1150);
    apply(s,{align("A","local","max")});near(guide_alignment_fixture::bounds(s.document()).right,1115);
    apply(s,{align("B","GX","center")});auto b=guide_alignment_fixture::bounds(s.document());near((b.left+b.right)/2,2050);
    apply(s,{align("A","GY","min","y")});near(guide_alignment_fixture::bounds(s.document()).top,40);
    apply(s,{AlignObjects{{"rect"},"x","min",{},"guide:global"}});near(guide_alignment_fixture::bounds(s.document()).left,75);
    check(encode(decode(encode(s.document())))==encode(s.document()),"native cold read retains transforms");
    const auto response=request(s,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(s.revision())+
        R"(,"commands":[{"type":"align_objects","objects":["rect"],"axis":"x","alignment":"min","reference":"guide:GX","guide_artboard":"B"}]})");
    check(response.find("\"ok\":true")!=std::string::npos,"scoped JSON alignment succeeds");near(guide_alignment_fixture::bounds(s.document()).left,2050);
    for(const auto* scope:{"",",\"guide_artboard\":null"}){
        const auto result=request(s,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(s.revision())+
            R"(,"commands":[{"type":"align_objects","objects":["rect"],"axis":"x","alignment":"min","reference":"guide:global")"+scope+"}]}");
        check(result.find("\"ok\":true")!=std::string::npos,"null/omitted scope preserves global JSON reference");near(guide_alignment_fixture::bounds(s.document()).left,75);
    }
    apply(s,{ReorderArtboards{"comp",{"B","source","A"}},align("B")});near(guide_alignment_fixture::bounds(s.document()).left,2050);
}
void negatives(){
    Session s(guide_alignment_fixture::document());
    atomic(s,{align("A","GY")},"GUIDE_AXIS_MISMATCH");
    atomic(s,{align("A","missing")},"MISSING_ARTBOARD_GUIDE");
    atomic(s,{align("missing")},"MISSING_ARTBOARD");
    atomic(s,{AlignObjects{{"rect"},"x","min",{},"selection",Id{"A"}}},"INVALID_REFERENCE");
    atomic(s,{AlignObjects{{"rect"},"x","min",Id{"A"},"selection",Id{"A"}}},"INVALID_REFERENCE");
    atomic(s,{align("A:GX")},"INVALID_ID");
    atomic(s,{align("A","GX:other")},"MISSING_ARTBOARD_GUIDE");
    atomic(s,{align("A"),Set{{"missing","","transform.tx"},1}});
    apply(s,{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","enabled",false}}});
    atomic(s,{align("A")},"DISABLED_ARTBOARD_GUIDE");
    auto d=guide_alignment_fixture::document();d.compositions.front().artboards[1].template_assignment->detached_guides={"GX"};
    d.compositions.front().artboards[1].template_assignment->guide_position_overrides.clear();Session suppressed(d);
    atomic(suppressed,{align("A")},"MISSING_ARTBOARD_GUIDE");
    d=guide_alignment_fixture::document();d.compositions.push_back({"other","Other",{},{{"other-art","Other"}}});Session cross(d);
    atomic(cross,{align("other-art")},"CROSS_COMPOSITION");
    d=guide_alignment_fixture::document();d.objects.at("rect").transform[4].expression=Expression{"0",1};Session driven(d);
    atomic(driven,{align("A")});
    d=guide_alignment_fixture::document();Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"rect"};
    d.objects.emplace(group.id,group);d.compositions.front().roots={"group"};Session overlap(d);
    atomic(overlap,{AlignObjects{{"group","rect"},"x","min",{},"guide:GX",Id{"A"}}},"OVERLAPPING_SELECTION");
    auto bytes=encode(s.document());auto rev=s.revision();auto history=s.history();bool stale=false;
    try{s.apply({align("B")},rev+1);}catch(const Error&){stale=true;}
    check(stale&&encode(s.document())==bytes&&s.revision()==rev&&s.history()==history,"stale revision is atomic");
}
}
int main(){try{lifecycle();negatives();std::cout<<"PASS "<<checks<<" scoped Guide alignment checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
