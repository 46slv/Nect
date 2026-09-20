#include "nect/io.hpp"
#include <iostream>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection");}
}
int main(){try{
    auto document=empty_document("doc","comp","first");
    document.compositions.push_back({"second-comp","Other coordinate plane",{},{{"other","Other frame",0,0,400,400}}});
    Session s(document);auto apply=[&](std::vector<Command> cmds){s.apply(cmds,s.revision());};
    auto frame=[&](const Id& id){return evaluate_artboard(s.document().compositions.front(),id);};
    apply({CreatePrimitive{"comp","","circle","Artwork",{"source","nect.shape.circle",1,
        {{"center_x",{200,{}}},{"center_y",{200,{}}},{"radius",{100,{}}}}}}});
    const auto authored_values=evaluate(s.document());
    Artboard second{"second","Alternate crop",200,100,320,240};second.parent_size=ArtboardParent{"first",true,false};
    Artboard third{"third","Nested crop",1300,0,10,20};third.parent_size=ArtboardParent{"second",true,true};
    apply({AddArtboard{"comp",second,1},AddArtboard{"comp",third,2}});
    check(frame("second").width==960&&frame("second").height==240&&frame("third").width==960&&frame("third").height==240,"Parent size resolves each attribute through a chain");
    auto first=frame("first");first.width=640;first.height=480;apply({UpdateArtboard{"comp",first}});
    check(frame("second").width==640&&frame("second").height==240&&frame("third").width==640,"Editing parent propagates inherited dimensions only");
    second.width=300;second.parent_size->width=false;apply({UpdateArtboard{"comp",second}});
    check(frame("third").width==300,"Child width override propagates to its own children");
    second.parent_size->width=true;apply({UpdateArtboard{"comp",second}});
    check(frame("second").width==640&&frame("third").width==640,"Reset dimension override follows parent again");
    auto crop=export_svg(s.document(),"comp","second");
    check(crop.find("viewBox=\"200 100 640 240\"")!=std::string::npos,"SVG exports resolved size and exact requested crop");
    apply({ReorderArtboards{"comp",{"third","second","first"}}});
    check(s.document().compositions.front().artboards.front().id=="third"&&export_svg(s.document(),"comp","second")==crop&&evaluate(s.document())==authored_values,"Page order changes neither crops nor any artwork property");
    const auto stored=encode(s.document());
    rejects("MISSING_ARTBOARD",[&]{apply({DeleteArtboard{"comp","first"}});});
    auto cycle=first;cycle.parent_size=ArtboardParent{"third",true,true};
    rejects("ARTBOARD_CYCLE",[&]{apply({UpdateArtboard{"comp",cycle}});});
    auto cross=second;cross.parent_size->artboard="other";
    rejects("MISSING_ARTBOARD",[&]{apply({UpdateArtboard{"comp",cross}});});
    rejects("INVALID_ORDER",[&]{apply({ReorderArtboards{"comp",{"first","first","third"}}});});
    rejects("LAST_ARTBOARD",[&]{apply({DeleteArtboard{"second-comp","other"}});});
    check(encode(s.document())==stored,"Failed deletion/order/cycle/cross-plane mutations are atomic");
    apply({DetachArtboardParent{"comp","second"},DeleteArtboard{"comp","first"}});
    check(!frame("second").parent_size&&frame("second").width==640&&frame("second").height==240&&frame("third").width==640,"Detach freezes effective size and permits deleting former parent");
    s.undo(s.revision());check(frame("second").parent_size&&frame("first").width==640,"Undo restores parent, bindings and page order together");
    auto moved=second;moved.x=700;moved.y=-80;apply({UpdateArtboard{"comp",moved}});
    check(frame("second").x==700&&frame("second").y==-80&&evaluate(s.document())==authored_values,"Explicit frame move changes crop without moving artwork");
    const auto encoded=encode(s.document());check(encode(decode(encoded))==encoded,"Parent frame metadata and local overrides reopen exactly");
    const auto readback=request(s,R"({"op":"artboards","composition":"comp"})");
    check(readback.find("\"authored\"")!=std::string::npos&&readback.find("\"evaluated\"")!=std::string::npos,"Semantic API exposes frame inheritance independently of stored overrides");
    std::cout<<"PASS "<<checks<<" artboard checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
