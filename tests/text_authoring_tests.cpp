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
    Session bool_session(s.document());auto bool_apply=[&](std::vector<Command> commands){bool_session.apply(commands,bool_session.revision());};
    auto target_source=default_text("target-source","Target");target_source.italic=true;
    bool_apply({CreateText{"comp","","target","Target",target_source}});
    const Ref italic_a{"title","","text.italic"},italic_b{"target","","text.italic"};
    check(resolve_name(bool_session.document(),"Title","","text.italic")==italic_a,"Name resolution finds a typed Text italic Ref");
    const auto discovered=properties(bool_session.document());
    check(std::find(discovered.begin(),discovered.end(),italic_a)!=discovered.end(),
        "Typed property discovery includes Text italic");
    bool_apply({LinkTextItalic{italic_b,italic_a,false}});
    check(!evaluate_text_italic(bool_session.document(),"target")&&bool_session.document().objects.at("target").text->italic&&
        std::get<Ref>(*bool_session.document().objects.at("target").text->italic_driver)==italic_a,
        "Same-type link evaluates through stable object Ref and preserves target literal");
    rejects("DRIVEN_PROPERTY",[&]{bool_apply({LinkTextItalic{italic_b,italic_a,false}});});
    rejects("DEPENDENCY_CYCLE",[&]{bool_apply({LinkTextItalic{italic_a,italic_b,false}});});
    rejects("MISSING_REFERENCE",[&]{bool_apply({LinkTextItalic{italic_b,{"missing","","text.italic"},true}});});
    Point unrelated_point;unrelated_point.id="path-point";
    bool_apply({CreatePath{"comp","","path","Path",{{"path-contour",false,{unrelated_point}}}}});
    rejects("TYPE_MISMATCH",[&]{bool_apply({LinkTextItalic{italic_b,{"path","","text.italic"},true}});});
    auto target_update=*bool_session.document().objects.at("target").text;target_update.content="Updated while linked";
    bool_apply({UpdateText{"target",target_update}});
    check(std::get<Ref>(*bool_session.document().objects.at("target").text->italic_driver)==italic_a,
        "UpdateText preserves a driver while editing unrelated Text fields");
    const auto driven_bytes=encode(bool_session.document());const auto driven_revision=bool_session.revision();
    target_update=*bool_session.document().objects.at("target").text;target_update.italic=false;
    rejects("DRIVEN_PROPERTY",[&]{bool_apply({UpdateText{"target",target_update}});});
    check(bool_session.revision()==driven_revision&&encode(bool_session.document())==driven_bytes,"Driven literal edit rejection is atomic");
    rejects("MISSING_REFERENCE",[&]{bool_apply({LinkProperties{{italic_b},italic_a,false}});});
    auto source_update=*bool_session.document().objects.at("title").text;source_update.italic=true;
    bool_apply({UpdateText{"title",source_update},Rename{"title","Renamed A"},ReorderObjects{"comp","",{"target","title","path"}}});
    check(evaluate_text_italic(bool_session.document(),"target")&&std::get<Ref>(*bool_session.document().objects.at("target").text->italic_driver)==italic_a,
        "Rename and reorder retain the committed stable Ref");
    check(request(bool_session,R"({"op":"get","ref":{"object":"target","point":"","field":"text.italic"}})").find("\"type\":\"bool\"")!=std::string::npos&&
        request(bool_session,R"({"op":"properties"})").find("\"field\":\"text.italic\"")!=std::string::npos,
        "JSON-lines get and properties expose typed bool metadata");
    const Expression inverted{"!ref(\"title\",\"\",\"text.italic\")",1};
    bool_apply({SetTextItalicExpression{italic_b,inverted,true}});
    check(!evaluate_text_italic(bool_session.document(),"target"),"Negated stable-Ref bool expression evaluates without numeric coercion");
    const auto expression_bytes=encode(bool_session.document());const auto expression_revision=bool_session.revision();
    rejects("BOOLEAN_EXPRESSION_SYNTAX",[&]{bool_apply({SetTextItalicExpression{italic_b,{"true || false",1},true}});});
    check(bool_session.revision()==expression_revision&&encode(bool_session.document())==expression_bytes,"Invalid bool expression leaves authored bytes and revision unchanged");
    bool_session.undo(bool_session.revision());
    check(std::get<Ref>(*bool_session.document().objects.at("target").text->italic_driver)==italic_a&&evaluate_text_italic(bool_session.document(),"target"),
        "Undo restores the prior link driver and evaluated value");
    bool_apply({SetTextItalicExpression{italic_b,{"true",1},true}});
    check(evaluate_text_italic(bool_session.document(),"target")&&std::holds_alternative<Expression>(*bool_session.document().objects.at("target").text->italic_driver),
        "Boolean true expression remains authored as an expression");
    bool_apply({SetTextItalicExpression{italic_b,{"false",1},true}});
    check(!evaluate_text_italic(bool_session.document(),"target")&&std::holds_alternative<Expression>(*bool_session.document().objects.at("target").text->italic_driver),
        "Boolean false expression remains authored as an expression");
    bool_apply({SetTextItalicExpression{italic_b,inverted,true}});
    const auto native15=encode(bool_session.document());
    check(native15.find("\"version\":\"0.15\"")!=std::string::npos&&native15.find("\"italic_driver\":{\"expression\"")!=std::string::npos&&
        encode(decode(native15))==native15,"Native 0.15 roundtrip preserves Text italic expression exactly");
    auto invalid_driver=native15;const auto driver_at=invalid_driver.find("\"italic_driver\":{\"expression\":");
    check(driver_at!=std::string::npos,"Native Text italic driver is serialized as the expression alternative");
    invalid_driver.replace(driver_at,std::string("\"italic_driver\":{\"expression\":").size(),"\"italic_driver\":{\"other\":");
    rejects("INVALID_TEXT_ITALIC_DRIVER",[&]{decode(invalid_driver);});
    auto old_with_driver=native15;const auto current_version=old_with_driver.find("\"version\":\"0.15\"");
    check(current_version!=std::string::npos,"Native bool driver fixture identifies version 0.15");
    old_with_driver.replace(current_version,std::string("\"version\":\"0.15\"").size(),"\"version\":\"0.14\"");
    rejects("UNSUPPORTED_TEXT_ITALIC_DRIVER",[&]{decode(old_with_driver);});
    const auto before_delete=encode(bool_session.document());const auto before_delete_revision=bool_session.revision();
    rejects("MISSING_REFERENCE",[&]{bool_apply({DeleteObjects{{"title"}}});});
    check(bool_session.revision()==before_delete_revision&&encode(bool_session.document())==before_delete,"Deleting a referenced Text source is atomic");
    bool_apply({UnlinkTextItalic{italic_b},DeleteObjects{{"title"}}});
    check(!bool_session.document().objects.contains("title")&&!bool_session.document().objects.at("target").text->italic_driver,
        "Same-batch unlink permits source deletion");
    auto legacy=empty_document("legacy-doc","legacy-comp","legacy-frame");
    auto legacy_text=default_text("legacy-text","Legacy");legacy_text.italic=true;
    Session legacy_session(legacy);legacy_session.apply({CreateText{"legacy-comp","","legacy-object","Legacy",legacy_text}},legacy_session.revision());
    auto native14=encode(legacy_session.document());const auto version_at=native14.find("\"version\":\"0.15\"");
    check(version_at!=std::string::npos,"Native writer emits 0.15");native14.replace(version_at,std::string("\"version\":\"0.15\"").size(),"\"version\":\"0.14\"");
    const auto old_text=decode(native14);check(old_text.objects.at("legacy-object").text->italic&&!old_text.objects.at("legacy-object").text->italic_driver,
        "Native 0.14 Text decodes with its literal italic value");
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
