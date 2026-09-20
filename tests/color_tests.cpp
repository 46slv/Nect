#include "nect/io.hpp"
#include <iostream>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection");}
ColorValue color(double r,double g,double b,double a=1){ColorValue c;c.rgba={r,g,b,a};return c;}
}
int main(){try{
    Session s(empty_document("doc","comp","art"));auto apply=[&](std::vector<Command> commands){s.apply(commands,s.revision());};
    NamedColor primary;primary.id="brand";primary.name="Brand";
    apply({CreateNamedColor{primary},CreatePrimitive{"comp","","rect","Rectangle",{"source","nect.shape.rectangle",1,
        {{"center_x",{100,{}}},{"center_y",{100,{}}},{"width",{160,{}}},{"height",{100,{}}}}}}});
    const Ref named{"brand","","color"},paint{"rect","","op.rect-stroke.color"};
    apply({SetColor{named,color(.2,.4,.6,.8)},LinkColor{paint,named}});
    auto value=[&](const Ref& ref){return color_value(s.document(),ref,evaluate(s.document()));};
    check(value(paint)==color(.2,.4,.6,.8)&&color_link(s.document(),paint)==named,"Typed color link uses all four stable normal channel references");
    check(property(s.document(),operation_ref("rect","rect-stroke","r")).binding->source==Ref{"brand","","color.r"},"Named colors use the common numeric dependency evaluator");
    apply({RenameNamedColor{"brand","Renamed Brand"},SetColor{named,color(.8,.1,.2,.5)}});
    check(value(paint)==color(.8,.1,.2,.5)&&color_link(s.document(),paint)==named,"Rename preserves color identity and explicit links propagate updates");
    check(resolve_name(s.document(),"Renamed Brand","","color.r")==Ref{"brand","","color.r"},"Named scalar properties support explicit name resolution");
    const auto saved=encode(s.document());const auto revision=s.revision();
    rejects("DRIVEN_PROPERTY",[&]{apply({SetColor{paint,color(1,1,1)}});});
    rejects("MISSING_REFERENCE",[&]{apply({DeleteNamedColor{"brand"}});});
    rejects("DEPENDENCY_CYCLE",[&]{apply({LinkColor{named,paint}});});
    auto unsupported=color(0,0,0);unsupported.space="cmyk";
    rejects("UNSUPPORTED_COLOR",[&]{apply({SetColor{named,unsupported}});});
    rejects("OUT_OF_RANGE",[&]{apply({SetColor{named,color(.1,.2,2)}});});
    check(s.revision()==revision&&encode(s.document())==saved,"Linked overwrite, removal, color-space, range and dependency errors reject atomically");
    apply({UnlinkColor{paint}});const auto frozen=value(paint);
    apply({SetColor{named,color(.1,.2,.3)}});check(value(paint)==frozen&&!color_link(s.document(),paint),"Unlink freezes exact resolved RGBA and stops propagation");
    apply({SetColor{paint,value(named)}});check(value(paint)==value(named)&&!color_link(s.document(),paint),"Copy Value stays independent despite identical channels");
    apply({SetColor{named,color(.7,.6,.5)}});check(value(paint)!=value(named),"Equal values never imply shared identity");
    Gradient g;g.id="gradient";GradientStop a;a.id="a";GradientStop b;b.id="b";b.offset.literal=1;g.stops={a,b};
    apply({SetGradient{"rect","rect-stroke",g}});
    const Ref stop=gradient_ref("rect","rect-stroke","gradient","stop.a.color");
    apply({LinkColor{stop,named}});
    check(value(stop)==value(named)&&color_link(s.document(),stop)==named,"Gradient stop typed colors share the same stable dependencies");
    check(!color_is_used(s.document(),named)&&!color_is_used(s.document(),paint)&&color_is_used(s.document(),stop),"Used-color inventory excludes palette entries and inactive solid fallback");
    apply({EnableOperation{"rect","rect-stroke",false}});check(!color_is_used(s.document(),stop),"Bypassed paint inputs are excluded from active usage");
    apply({EnableOperation{"rect","rect-stroke",true}});
    const auto bytes=encode(s.document());check(encode(decode(bytes))==bytes,"Named definitions and linked paints/stops survive native reopen without drift");
    const auto get=request(s,R"({"op":"get","ref":{"object":"brand","point":"","field":"color"}})");
    check(get.find("\"type\":\"color\"")!=std::string::npos&&get.find("\"profile\":\"srgb\"")!=std::string::npos,"Common get exposes typed color with explicit profile/alpha");
    check(request(s,R"({"op":"properties"})").find("\"color.r\"")!=std::string::npos,"Discovery includes named channel and aggregate color properties");
    check(request(s,R"({"op":"used_colors"})").find("\"equal_values_imply_link\":false")!=std::string::npos,"Usage readback distinguishes equality from identity");
    apply({UnlinkColor{stop},DeleteNamedColor{"brand"}});check(s.document().named_colors.empty(),"Explicit detach permits removing a definition");
    s.undo(s.revision());check(s.document().named_colors.contains("brand")&&color_link(s.document(),stop)==named,"Undo restores named definition and links atomically");
    auto clash=primary;clash.id="source";rejects("DUPLICATE_ID",[&]{apply({CreateNamedColor{clash}});});
    std::cout<<"PASS "<<checks<<" color checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
