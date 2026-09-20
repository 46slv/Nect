#include "nect/io.hpp"
#include <iostream>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection");}
}
int main(){try{
    Session s(empty_document("doc","comp","frame"));auto apply=[&](std::vector<Command> commands){s.apply(commands,s.revision());};
    auto source=default_text("source","Editable\nText");source.parameters.at("font_size").literal=32;
    apply({CreateText{"comp","","title","Title",source}});
    check(s.document().objects.at("title").kind==Kind::text&&s.document().objects.at("title").stack.front().type=="nect.paint.fill","Text creates editable source with a real Fill");
    check(s.document().objects.at("title").contours.empty(),"No duplicate authored glyph outlines");
    const Ref size{"title","","text.font_size"},width{"title","","text.frame_width"};
    apply({Link{width,{size,10,0,"copy_local_value"}}});
    source=*s.document().objects.at("title").text;source.direction="vertical";source.content="Changed text";
    apply({UpdateText{"title",source},Set{size,40}});
    check(evaluate(s.document()).at(width)==400&&property(s.document(),width).binding.has_value(),"Content/layout changes retain property links");
    const auto saved=encode(s.document());check(encode(decode(saved))==saved,"Native text, font metadata and linked Scalars reopen exactly");
    const auto revision=s.revision();auto invalid=source;invalid.id="replacement";
    rejects("ID_MISMATCH",[&]{apply({UpdateText{"title",invalid}});});
    invalid=source;invalid.content=std::string("\xc0\xaf",2);rejects("INVALID_UTF8",[&]{apply({UpdateText{"title",invalid}});});
    invalid=source;invalid.content=std::string("\0",1);rejects("INVALID_TEXT",[&]{apply({UpdateText{"title",invalid}});});
    rejects("OUT_OF_RANGE",[&]{apply({Set{size,0}});});
    rejects("DEPENDENCY_CYCLE",[&]{apply({Link{size,{width,1,0,"copy_local_value"}}});});
    check(s.revision()==revision&&encode(s.document())==saved,"Invalid text/source edits are atomic");
    s.undo(s.revision());check(s.document().objects.at("title").text->content=="Editable\nText"&&evaluate(s.document()).at(width)==320,"Undo restores text content, layout and driven values together");
    s.redo(s.revision());check(encode(s.document())==saved,"Redo restores exact authored text");
#ifdef _WIN32
    auto repeat=default_operation("repeat","nect.shape.repeater");repeat.parameters.at("copies").literal=3;
    apply({AddOperation{"title",repeat,1}});
    const auto shape=evaluate_shape(s.document(),"title",evaluate(s.document()));
    check(shape.paints.size()==3&&!shape.paints.front().paths.front().contours->empty(),"Text outlines feed the common ordered paint/repeat stack");
    const auto svg=export_svg(s.document(),"comp","frame");
    check(svg.find("<text")==std::string::npos&&svg.find("Text outlined for SVG")!=std::string::npos,"SVG explicitly discloses outlined text projection");
    check(request(s,R"({"op":"text_layout","object":"title"})").find("\"glyph_count\"")!=std::string::npos,"API exposes layout diagnostics");
    check(request(s,R"({"op":"export_plan","composition":"comp","artboard":"frame"})").find("\"text_policy\":\"outlines\"")!=std::string::npos,"Export plan discloses text editability loss before export");
#endif
    check(request(s,R"({"op":"text_defaults"})").find("\"frame_width\"")!=std::string::npos,"API exposes complete text defaults");
    std::cout<<"PASS "<<checks<<" text authoring checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
