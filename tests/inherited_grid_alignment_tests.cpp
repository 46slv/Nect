#include "inherited_grid_alignment_fixture.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
namespace {
int checks=0;void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);++checks;}
void near(double a,double b){check(std::abs(a-b)<1e-8,"wrong inherited Grid coordinate");}
void apply(Session& s,std::vector<Command> cs){s.apply(cs,s.revision());}
Command align(Id grid="template-grid-A",std::string mode="min",std::string axis="x"){
    return AlignObjects{{"a"},axis,mode,{},"grid:"+grid};
}
Command distribute(Id grid="template-grid-A",std::string axis="x"){
    return DistributeObjects{{"c","a","b"},axis,"grid:"+grid,{}};
}
void atomic(Session& s,Command c,const char* code){
    const auto bytes=encode(s.document());auto revision=s.revision();auto history=s.history();bool rejected=false;
    try{apply(s,{c});}catch(const Error& e){rejected=true;check(e.code==code,"wrong refusal code");}
    check(rejected&&encode(s.document())==bytes&&s.revision()==revision&&s.history()==history,"refusal changed authored state");
}
void basic(){
    Session s(inherited_grid_fixture::document());const auto original=encode(s.document());const auto source=s.document().compositions.front().artboards.front();
    apply(s,{align()});near(inherited_grid_fixture::minimum(s.document(),"a"),1040);
    const auto aligned=encode(s.document());s.undo(s.revision());check(encode(s.document())==original,"one Undo restores bytes");
    s.redo(s.revision());check(encode(s.document())==aligned,"one Redo restores bytes");
    apply(s,{align("template-grid-A","center")});near(inherited_grid_fixture::minimum(s.document(),"a"),1085);
    apply(s,{align("template-grid-A","max")});near(inherited_grid_fixture::minimum(s.document(),"a"),1130);
    apply(s,{align("template-grid-A","min","y")});near(inherited_grid_fixture::minimum(s.document(),"a",false),230);
    apply(s,{align("template-grid-A","center","y")});near(inherited_grid_fixture::minimum(s.document(),"a",false),265);
    apply(s,{align("template-grid-A","max","y")});near(inherited_grid_fixture::minimum(s.document(),"a",false),300);
    check(!s.document().compositions.front().artboards[1].layout&&s.document().compositions.front().artboards.front()==source,
        "alignment preserves absent local layout and source bytes");
    check(encode(decode(encode(s.document())))==encode(s.document()),"native roundtrip retains authored result");
    for(const auto grid:{"template-grid-A","template-grid-B"}){
        Session d(inherited_grid_fixture::document());apply(d,{distribute(grid)});const double add=std::string(grid).ends_with("A")?0:1000;
        near(inherited_grid_fixture::minimum(d.document(),"a"),1055+add);near(inherited_grid_fixture::minimum(d.document(),"b"),1080+add);near(inherited_grid_fixture::minimum(d.document(),"c"),1115+add);
    }
    Session vertical(inherited_grid_fixture::document());apply(vertical,{distribute("template-grid-A","y")});
    near(inherited_grid_fixture::minimum(vertical.document(),"a",false),240);near(inherited_grid_fixture::minimum(vertical.document(),"b",false),260);near(inherited_grid_fixture::minimum(vertical.document(),"c",false),290);
}
void lifecycle(){
    Session s(inherited_grid_fixture::document());auto layout=*s.document().compositions.front().artboards[0].layout;layout.grid->bounds.x=60;
    apply(s,{SetArtboardLayout{"comp","source",layout},align()});near(inherited_grid_fixture::minimum(s.document(),"a"),1060);
    auto source=s.document().compositions.front().artboards[0];source.x=-800;
    apply(s,{UpdateArtboard{"comp",source},align("template-grid-B")});near(inherited_grid_fixture::minimum(s.document(),"a"),2060);
    auto a=s.document().compositions.front().artboards[1];a.x=1100;a.name="Renamed";
    apply(s,{UpdateArtboard{"comp",a},ReorderArtboards{"comp",{"B","source","A"}},align()});near(inherited_grid_fixture::minimum(s.document(),"a"),1160);
    apply(s,{ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","layout.grid",std::optional<Grid>{Grid{"template-grid-A",{10,20,120,90},1,1,0,0}}}},align()});
    near(inherited_grid_fixture::minimum(s.document(),"a"),1110);apply(s,{align("template-grid-B")});near(inherited_grid_fixture::minimum(s.document(),"a"),2060);
    apply(s,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","A","layout.grid"}},align()});near(inherited_grid_fixture::minimum(s.document(),"a"),1160);
    apply(s,{ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","layout.grid",std::optional<Grid>{}}}});
    atomic(s,align(),"MISSING_GRID");atomic(s,distribute(),"MISSING_GRID");
    apply(s,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","A","layout.grid"}},
        ArtboardTemplateCommand{DetachArtboardTemplate{"comp","A","detached"}},SetArtboardLayout{"comp","source",{}}});
    apply(s,{align()});near(inherited_grid_fixture::minimum(s.document(),"a"),1160);atomic(s,align("template-grid-B"),"MISSING_GRID");
    atomic(s,DeleteArtboard{"comp","source"},"ARTBOARD_IN_USE");
}
void driven_bounds(){
    auto d=inherited_grid_fixture::document();d.compositions.front().artboards.push_back({"measure","Measure",3000,0,200,300});
    auto& grid=*d.compositions.front().artboards[0].layout->grid;
    grid.bounds_x_driver=Ref{"measure","","artboard.width"};
    grid.bounds_width_expression=Expression{R"(ref("measure","","artboard.width") * 0.5)",1};
    Session s(d);apply(s,{align("template-grid-A","max")});near(inherited_grid_fixture::minimum(s.document(),"a"),1290);
    auto measure=s.document().compositions.front().artboards.back();measure.width=400;
    apply(s,{UpdateArtboard{"comp",measure},align("template-grid-A","max")});near(inherited_grid_fixture::minimum(s.document(),"a"),1590);
    check(!s.document().compositions.front().artboards[1].layout,"driven inherited bounds are not materialized locally");
}
void negatives(){
    auto d=inherited_grid_fixture::document();Composition other;other.id="other";other.name="Other";
    Artboard source{"foreign-source","Source"};source.layout=ArtboardLayout{{},Grid{"foreign-source-grid",{0,0,100,100},1,1,0,0}};
    Artboard target{"foreign-target","Target"};ArtboardTemplateAssignment assignment;assignment.template_id="foreign-template";assignment.grid_id="foreign-grid";target.template_assignment=assignment;
    other.artboards={source,target};other.templates={{"foreign-template","Template","foreign-source",{}}};d.compositions.push_back(other);Session s(d);
    atomic(s,align("foreign-grid"),"CROSS_COMPOSITION");atomic(s,distribute("foreign-grid"),"CROSS_COMPOSITION");atomic(s,align("missing"),"MISSING_GRID");
    auto layout=*s.document().compositions.front().artboards[0].layout;layout.grid->bounds.width=20;
    apply(s,{SetArtboardLayout{"comp","source",layout}});atomic(s,distribute(),"REFERENCE_SPAN_TOO_SMALL");
}
}
int main(){try{basic();lifecycle();driven_bounds();negatives();std::cout<<"PASS "<<checks<<" inherited Grid alignment checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
