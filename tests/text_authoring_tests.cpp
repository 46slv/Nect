#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void near(double actual,double expected,double tolerance,const char* why) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance)
        throw std::runtime_error(std::string(why)+" (actual="+std::to_string(actual)+", expected="+std::to_string(expected)+")");++checks;
}
std::string response_result_object(const std::string& response) {
    const auto marker=response.find("\"result\":");if(marker==std::string::npos)return {};
    const auto begin=response.find('{',marker+9);if(begin==std::string::npos)return {};
    bool in_string=false,escaped=false;std::size_t depth=0;
    for(std::size_t i=begin;i<response.size();++i) {
        const auto c=response[i];
        if(in_string) {
            if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='\"')in_string=false;
            continue;
        }
        if(c=='\"')in_string=true;
        else if(c=='{')++depth;
        else if(c=='}'&&--depth==0)return response.substr(begin,i-begin+1);
    }
    return {};
}
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
    const auto native16=encode(bool_session.document());
    check(native16.find("\"version\":\"0.65\"")!=std::string::npos&&native16.find("\"italic_driver\":{\"expression\"")!=std::string::npos&&
        encode(decode(native16))==native16,"Native 0.43 roundtrip preserves Text italic expression exactly");
    auto invalid_driver=native16;const auto driver_at=invalid_driver.find("\"italic_driver\":{\"expression\":");
    check(driver_at!=std::string::npos,"Native Text italic driver is serialized as the expression alternative");
    invalid_driver.replace(driver_at,std::string("\"italic_driver\":{\"expression\":").size(),"\"italic_driver\":{\"other\":");
    rejects("INVALID_TEXT_ITALIC_DRIVER",[&]{decode(invalid_driver);});
    auto old_with_driver=test_support::without_empty_presets_for_legacy_fixture(native16);const auto current_version=old_with_driver.find("\"version\":\"0.65\"");
    check(current_version!=std::string::npos,"Native bool driver fixture identifies version 0.23");
    old_with_driver.replace(current_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.14\"");
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
    auto native14=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_session.document()));const auto version_at=native14.find("\"version\":\"0.65\"");
    check(version_at!=std::string::npos,"Native writer emits 0.65");native14.replace(version_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.14\"");
    const auto old_text=decode(native14);check(old_text.objects.at("legacy-object").text->italic&&!old_text.objects.at("legacy-object").text->italic_driver,
        "Native 0.14 Text decodes with its literal italic value");
    auto weight_document=empty_document("weight-doc","weight-comp","weight-frame");Session weight_session(weight_document);
    auto weight_apply=[&](std::vector<Command> commands){weight_session.apply(commands,weight_session.revision());};
    auto weight_a=default_text("weight-a-source","Weight A");weight_a.weight=700;
    auto weight_b=default_text("weight-b-source","Weight B");weight_b.weight=400;
    weight_a.content="Source content A";weight_a.family="Family A";weight_a.locale="en-GB";
    weight_a.layout="frame";weight_a.direction="vertical";weight_a.alignment="center";
    weight_b.content="Source content B";weight_b.family="Family B";weight_b.locale="ja-JP";
    weight_b.layout="auto";weight_b.direction="horizontal";weight_b.alignment="end";
    Point weight_path_point;weight_path_point.id="weight-path-point";
    weight_apply({CreateText{"weight-comp","","weight-a","Weight A",weight_a},
        CreateText{"weight-comp","","weight-b","Weight B",weight_b},
        CreatePath{"weight-comp","","weight-path","Weight Path",{{"weight-path-contour",false,{weight_path_point}}}}});
    const Ref weight_a_ref{"weight-a","","text.weight"},weight_b_ref{"weight-b","","text.weight"};
    const std::array<std::string,6> readonly_fields{"text.content","text.family","text.locale","text.layout","text.direction","text.alignment"};
    const Ref weight_a_content{"weight-a","","text.content"};
    const std::array<std::string,6> values_a{"Source content A","Family A","en-GB","frame","vertical","center"};
    const std::array<std::string,6> values_b{"Source content B","Family B","ja-JP","auto","horizontal","end"};
    const auto readonly_refs=properties(weight_session.document());
    for(int object_index=0;object_index<2;++object_index) {
        const std::string object_id=object_index==0?"weight-a":"weight-b";
        const auto& expected=object_index==0?values_a:values_b;
        const auto count=std::count_if(readonly_refs.begin(),readonly_refs.end(),[&](const Ref& ref) {
            return ref.object==object_id&&std::find(readonly_fields.begin(),readonly_fields.end(),ref.field)!=readonly_fields.end();
        });
        check(count==readonly_fields.size(),"Each Text contributes exactly six typed source refs");
        for(std::size_t i=0;i<readonly_fields.size();++i) {
            const Ref ref{object_id,"",readonly_fields[i]};
            check(std::find(readonly_refs.begin(),readonly_refs.end(),ref)!=readonly_refs.end(),"Text source discovery returns the stable object Ref");
            const auto value=text_readonly_property(weight_session.document(),ref);
            check(value.literal==expected[i],"Typed Text source read returns the authored literal");
            const bool is_enum=i>=3;
            check(value.kind==(is_enum?TextPropertyKind::enumeration:TextPropertyKind::string),"Text source discovery distinguishes strings and enums");
            if(is_enum)check(!value.choices.empty(),"Text enum discovery includes its allowed choices");
        }
    }
    check(resolve_name(weight_session.document(),"Weight A","","text.weight")==weight_a_ref,
        "Name resolution discovers the stable typed Text weight Ref");
    check(resolve_name(weight_session.document(),"Weight A","","text.content")==weight_a_content,
        "Name resolution discovers the stable Text string Ref");
    const auto readonly_bytes=encode(weight_session.document());const auto readonly_revision=weight_session.revision();
    const auto readonly_properties=request(weight_session,R"({"op":"properties"})");
    for(std::size_t i=0;i<readonly_fields.size();++i) {
        const auto query=std::string("{\"op\":\"get\",\"ref\":{\"object\":\"weight-a\",\"point\":\"\",\"field\":\"")+readonly_fields[i]+"\"}}";
        const auto get=request(weight_session,query);
        const auto type=i<3?"string":"enum";
        const auto link=(i<=5)?"true":"false";
        check(get.find("\"type\":\""+std::string(type)+"\"")!=std::string::npos&&
            get.find("\"literal\":\""+values_a[i]+"\"")!=std::string::npos&&
            get.find("\"evaluated\":\""+values_a[i]+"\"")!=std::string::npos&&
            get.find("\"link\":"+std::string(link))!=std::string::npos&&get.find("\"expression\":false")!=std::string::npos&&
            (i<=5?get.find("\"driver\":null")!=std::string::npos:true),
            "JSON-lines get returns the Text literal, type, and scoped link capability");
        check(readonly_properties.find("\"object\":\"weight-a\",\"point\":\"\",\"field\":\""+readonly_fields[i]+"\"")!=std::string::npos,
            "JSON-lines properties lists each typed Text source ref");
    }
    check(readonly_properties.find("\"choices\":[\"auto\",\"frame\"]")!=std::string::npos&&
        readonly_properties.find("\"choices\":[\"horizontal\",\"vertical\"]")!=std::string::npos&&
        readonly_properties.find("\"choices\":[\"start\",\"center\",\"end\"]")!=std::string::npos,
        "JSON-lines enum discovery returns the exact Text choice domains");
    const auto resolved_text=request(weight_session,R"({"op":"resolve_name","name":"Weight A","point":"","field":"text.content"})");
    check(resolved_text.find("\"field\":\"text.content\"")!=std::string::npos&&
        weight_session.revision()==readonly_revision&&encode(weight_session.document())==readonly_bytes,
        "Text get/properties/resolve_name reads leave native bytes and Session revision unchanged");
    check(encode(decode(readonly_bytes))==readonly_bytes,"Native 0.43 roundtrip preserves Text source values exactly");
    rejects("MISSING_NAME",[&]{resolve_name(weight_session.document(),"Missing Text","","text.content");});
    auto duplicate_names=weight_session.document();duplicate_names.objects.at("weight-b").name="Weight A";
    rejects("AMBIGUOUS_NAME",[&]{resolve_name(duplicate_names,"Weight A","","text.content");});
    rejects("TYPE_MISMATCH",[&]{resolve_name(weight_session.document(),"Weight Path","","text.content");});
    rejects("UNKNOWN_TEXT_PROPERTY",[&]{resolve_name(weight_session.document(),"Weight A","","text.unregistered");});
    rejects("INVALID_TEXT_REF",[&]{resolve_name(weight_session.document(),"Weight A","a-point","text.content");});
    check(request(weight_session,R"({"op":"get","ref":{"object":"weight-a","point":"","field":"text.unregistered"}})").find("\"code\":\"UNKNOWN_TEXT_PROPERTY\"")!=std::string::npos&&
        request(weight_session,R"({"op":"get","ref":{"object":"weight-path","point":"","field":"text.content"}})").find("\"code\":\"TYPE_MISMATCH\"")!=std::string::npos&&
        request(weight_session,R"({"op":"get","ref":{"object":"weight-a","point":"p1","field":"text.content"}})").find("\"code\":\"INVALID_TEXT_REF\"")!=std::string::npos,
        "JSON-lines get rejects unknown, wrong-kind and point-specific Text refs with scoped errors");
    rejects("MISSING_REFERENCE",[&]{weight_session.apply({LinkProperties{{{"weight-a","","text.font_size"}},weight_a_content,false}},readonly_revision);});
    rejects("MISSING_REFERENCE",[&]{weight_session.apply({SetExpression{{weight_a_content},{"1",1},false}},readonly_revision);});
    check(weight_session.revision()==readonly_revision&&encode(weight_session.document())==readonly_bytes,
        "Scalar links and expressions reject typed Text refs without changing bytes or revision");
    const auto weight_discovered=properties(weight_session.document());
    check(std::find(weight_discovered.begin(),weight_discovered.end(),weight_a_ref)!=weight_discovered.end(),
        "Property discovery includes integer Text weight");
    weight_apply({LinkTextWeight{weight_b_ref,weight_a_ref,false}});
    auto linked_weight_property=text_weight_property(weight_session.document(),weight_b_ref);
    check(linked_weight_property.literal==400&&linked_weight_property.evaluated==700&&
        linked_weight_property.driver==TextWeightDriver{weight_a_ref},
        "Same-type weight link evaluates through its stable Ref and preserves the target literal");
    const auto weight_link_bytes=encode(weight_session.document());
    check(weight_link_bytes.find("\"version\":\"0.65\"")!=std::string::npos&&
        weight_link_bytes.find("\"weight_driver\":{\"link\"")!=std::string::npos&&
        weight_link_bytes.find("\"offset\"")==std::string::npos&&
        encode(decode(weight_link_bytes))==weight_link_bytes,
        "Current native writer preserves the legacy zero-offset Text weight Ref shape exactly");
    auto weight_native_060=test_support::without_empty_presets_for_legacy_fixture(weight_link_bytes);const auto weight_native_060_version=weight_native_060.find("\"version\":\"0.65\"");
    check(weight_native_060_version!=std::string::npos,"Native single-target weight link identifies the current writer");
    weight_native_060.replace(weight_native_060_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.60\"");
    const auto decoded_weight_060=decode(weight_native_060);
    check(decoded_weight_060.objects.at("weight-b").text->weight_driver==TextWeightDriver{weight_a_ref,0},
        "Native 0.60 absolute Text weight links remain readable with an omitted zero offset");
    const auto weight_get=request(weight_session,R"({"op":"get","ref":{"object":"weight-b","point":"","field":"text.weight"}})");
    const auto weight_properties=request(weight_session,R"({"op":"properties"})");
    check(weight_get.find("\"type\":\"integer\"")!=std::string::npos&&weight_get.find("\"unit\":\"unitless\"")!=std::string::npos&&
        weight_get.find("\"min\":1")!=std::string::npos&&weight_get.find("\"max\":999")!=std::string::npos&&
        weight_get.find("\"literal\":400")!=std::string::npos&&weight_get.find("\"evaluated\":700")!=std::string::npos&&
        weight_properties.find("\"field\":\"text.weight\"")!=std::string::npos,
        "JSON get and properties report integer range, authored literal, driver and evaluated weight");
    const auto weight_revision=weight_session.revision();
    const auto weight_unchanged=[&](const std::string& bytes,std::uint64_t revision,const char* message) {
        check(weight_session.revision()==revision&&encode(weight_session.document())==bytes,message);
    };
    weight_apply({LinkTextWeight{weight_b_ref,weight_a_ref,false}});
    weight_unchanged(weight_link_bytes,weight_revision,"Reapplying the exact weight link is idempotent");
    rejects("MISSING_REFERENCE",[&]{weight_apply({LinkTextWeight{weight_b_ref,Ref{"missing-weight","","text.weight"},true}});});
    weight_unchanged(weight_link_bytes,weight_revision,"Missing weight source rejection preserves authored bytes and revision");
    rejects("TYPE_MISMATCH",[&]{weight_apply({LinkTextWeight{weight_b_ref,Ref{"weight-path","","text.weight"},true}});});
    weight_unchanged(weight_link_bytes,weight_revision,"Wrong-type weight source rejection preserves authored bytes and revision");
    rejects("DEPENDENCY_CYCLE",[&]{weight_apply({LinkTextWeight{weight_a_ref,weight_b_ref,false}});});
    weight_unchanged(weight_link_bytes,weight_revision,"A cyclic weight link rejects atomically");
    rejects("MISSING_REFERENCE",[&]{weight_apply({LinkProperties{{weight_b_ref},weight_a_ref,false}});});
    rejects("MISSING_REFERENCE",[&]{weight_apply({SetExpression{{weight_b_ref},{"300",1}}});});
    weight_unchanged(weight_link_bytes,weight_revision,"Generic Scalar links and expressions cannot claim Text weight");
    auto driven_weight_edit=*weight_session.document().objects.at("weight-b").text;driven_weight_edit.weight=450;
    rejects("DRIVEN_PROPERTY",[&]{weight_apply({UpdateText{"weight-b",driven_weight_edit}});});
    weight_unchanged(weight_link_bytes,weight_revision,"A driven weight literal edit rejects without changing revision or authored bytes");
    rejects("MISSING_REFERENCE",[&]{weight_apply({DeleteObjects{{"weight-a"}}});});
    weight_unchanged(weight_link_bytes,weight_revision,"Deleting a referenced weight source rejects atomically");
    rejects("REVISION_CONFLICT",[&]{weight_session.apply({UnlinkTextWeight{weight_b_ref}},weight_revision-1);});
    weight_unchanged(weight_link_bytes,weight_revision,"A stale weight command leaves the link unchanged");
    auto source_weight_edit=*weight_session.document().objects.at("weight-a").text;source_weight_edit.weight=300;
    weight_apply({UpdateText{"weight-a",source_weight_edit},Rename{"weight-a","Renamed A"},
        ReorderObjects{"weight-comp","",{"weight-path","weight-b","weight-a"}}});
    check(evaluate_text_weight(weight_session.document(),"weight-b")==300&&
        text_weight_property(weight_session.document(),weight_b_ref).literal==400&&
        weight_session.document().objects.at("weight-b").text->weight_driver->link==weight_a_ref,
        "Weight follows stable object identity after rename/reorder while the target literal remains authored");
    const auto linked_reopen=decode(encode(weight_session.document()));
    check(linked_reopen.objects.at("weight-b").text->weight_driver->link==weight_a_ref&&
        evaluate_text_weight(linked_reopen,"weight-b")==300,
        "Native encode/decode retains the exact Text weight Ref and evaluated value");
    weight_apply({UnlinkTextWeight{weight_b_ref}});
    check(weight_session.document().objects.at("weight-b").text->weight==300&&
        !weight_session.document().objects.at("weight-b").text->weight_driver,
        "Unlink freezes the evaluated integer into the authored literal");
    weight_session.undo(weight_session.revision());
    check(weight_session.document().objects.at("weight-b").text->weight==400&&
        weight_session.document().objects.at("weight-b").text->weight_driver->link==weight_a_ref&&
        evaluate_text_weight(weight_session.document(),"weight-b")==300,
        "One Undo restores the weight link and its evaluation");
    weight_session.redo(weight_session.revision());
    source_weight_edit=*weight_session.document().objects.at("weight-a").text;source_weight_edit.weight=500;
    weight_apply({UpdateText{"weight-a",source_weight_edit}});
    check(evaluate_text_weight(weight_session.document(),"weight-b")==300,
        "A weight target stays frozen after unlink while its former source changes");
    const auto weight_before_bad=encode(weight_session.document());const auto weight_before_bad_revision=weight_session.revision();
    source_weight_edit=*weight_session.document().objects.at("weight-a").text;source_weight_edit.weight=1000;
    rejects("OUT_OF_RANGE",[&]{weight_apply({UpdateText{"weight-a",source_weight_edit}});});
    weight_unchanged(weight_before_bad,weight_before_bad_revision,"Out-of-range authored Text weight is rejected atomically");
    auto malformed_weight=weight_link_bytes;const auto weight_link_at=malformed_weight.find("\"weight_driver\":{\"link\":");
    check(weight_link_at!=std::string::npos,"Native Text weight link has the strict link alternative");
    malformed_weight.replace(weight_link_at,std::string("\"weight_driver\":{\"link\":").size(),"\"weight_driver\":{\"other\":");
    rejects("INVALID_TEXT_WEIGHT_DRIVER",[&]{decode(malformed_weight);});
    auto wrong_weight_ref=weight_link_bytes;const auto weight_ref_at=wrong_weight_ref.find("\"weight_driver\":{\"link\":");
    const auto weight_field_at=wrong_weight_ref.find("text.weight",weight_ref_at);
    check(weight_ref_at!=std::string::npos&&weight_field_at!=std::string::npos,"Native weight Ref field is explicitly present");
    wrong_weight_ref.replace(weight_field_at,std::string("text.weight").size(),"text.italic");
    rejects("TYPE_MISMATCH",[&]{decode(wrong_weight_ref);});
    auto missing_weight_source=weight_link_bytes;const auto weight_object_at=missing_weight_source.find("weight-a",weight_ref_at);
    check(weight_object_at!=std::string::npos,"Native weight Ref source ID is explicitly present");
    missing_weight_source.replace(weight_object_at,std::string("weight-a").size(),"missing");
    rejects("MISSING_REFERENCE",[&]{decode(missing_weight_source);});
    auto cyclic_weight=weight_link_bytes;const auto target_link_object=cyclic_weight.find("weight-a",weight_ref_at);
    check(target_link_object!=std::string::npos,"Native weight Ref target ID is explicitly present");
    cyclic_weight.replace(target_link_object,std::string("weight-a").size(),"weight-b");
    rejects("DEPENDENCY_CYCLE",[&]{decode(cyclic_weight);});
    auto old_weight_driver=test_support::without_empty_presets_for_legacy_fixture(weight_link_bytes);const auto weight_version_at=old_weight_driver.find("\"version\":\"0.65\"");
    check(weight_version_at!=std::string::npos,"Native weight fixture identifies version 0.23");
    old_weight_driver.replace(weight_version_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.15\"");
    rejects("UNSUPPORTED_TEXT_WEIGHT_DRIVER",[&]{decode(old_weight_driver);});
    const auto legacy_weight=decode([&]{auto value=test_support::without_empty_presets_for_legacy_fixture(encode(weight_session.document()));const auto at=value.find("\"version\":\"0.65\"");
        value.replace(at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.15\"");return value;}());
    check(legacy_weight.objects.at("weight-a").text->weight==500&&!legacy_weight.objects.at("weight-a").text->weight_driver&&
        legacy_weight.objects.at("weight-b").text->weight==300&&!legacy_weight.objects.at("weight-b").text->weight_driver,
        "Native 0.15 Text migrates authored weights as literals");
    Session expression_session(empty_document("weight-expression-doc","weight-expression-comp","weight-expression-frame"));
    auto expression_apply=[&](std::vector<Command> commands){expression_session.apply(commands,expression_session.revision());};
    auto expression_a=default_text("expression-a-source","A");expression_a.weight=300;
    auto expression_b=default_text("expression-b-source","B");expression_b.weight=400;
    expression_apply({CreateText{"weight-expression-comp","","expression-a","A",expression_a},
        CreateText{"weight-expression-comp","","expression-b","B",expression_b}});
    const Ref expression_a_ref{"expression-a","","text.weight"},expression_b_ref{"expression-b","","text.weight"};
    const Expression weight_expression{"ref(\"expression-a\",\"\",\"text.weight\") + 100",1};
    expression_apply({SetTextWeightExpression{expression_b_ref,weight_expression,false}});
    auto expression_property=text_weight_property(expression_session.document(),expression_b_ref);
    check(expression_property.literal==400&&expression_property.evaluated==400&&
        expression_property.expression==weight_expression&&!expression_property.driver,
        "Text weight expression preserves literal and evaluates the same-Document integer Ref");
    const auto expression_native=encode(expression_session.document());
    check(expression_native.find("\"version\":\"0.65\"")!=std::string::npos&&
        expression_native.find("\"weight_expression\"")!=std::string::npos&&
        encode(decode(expression_native))==expression_native,
        "Native 0.60 preserves exact Text weight expression source and byte-roundtrips");
    const auto expression_get=request(expression_session,R"({"op":"get","ref":{"object":"expression-b","point":"","field":"text.weight"}})");
    check(expression_get.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        expression_get.find("\"expression\":true")!=std::string::npos&&
        expression_get.find("\"literal\":400")!=std::string::npos,
        "JSON-lines get exposes authored Text weight expression and exact integer capability");
    const auto weight_expression_revision=expression_session.revision();
    expression_apply({SetTextWeightExpression{expression_b_ref,weight_expression,false}});
    check(expression_session.revision()==weight_expression_revision&&encode(expression_session.document())==expression_native,
        "Reapplying the same Text weight expression does not add revision or history");
    const auto expression_unchanged=[&](const char* why){check(expression_session.revision()==weight_expression_revision&&
        encode(expression_session.document())==expression_native,why);};
    rejects("DRIVEN_PROPERTY",[&]{expression_apply({LinkTextWeight{expression_b_ref,expression_a_ref,false}});});
    expression_unchanged("Implicit link replacement leaves expression intact");
    rejects("DRIVEN_PROPERTY",[&]{expression_apply({SetTextWeightExpression{expression_b_ref,Expression{"500",1},false}});});
    expression_unchanged("Implicit expression replacement leaves expression intact");
    rejects("OUT_OF_RANGE",[&]{expression_apply({SetTextWeightExpression{expression_b_ref,Expression{"1.5",1},true}});});
    expression_unchanged("Fractional expression result rejects without rounding");
    rejects("OUT_OF_RANGE",[&]{expression_apply({SetTextWeightExpression{expression_b_ref,Expression{"1000",1},true}});});
    expression_unchanged("Out-of-range integer expression rejects without clamping");
    rejects("UNIT_MISMATCH",[&]{expression_apply({SetTextWeightExpression{expression_b_ref,
        Expression{"ref(\"expression-a\",\"\",\"text.italic\")",1},true}});});
    expression_unchanged("Wrong Text field expression Ref rejects atomically");
    rejects("DEPENDENCY_CYCLE",[&]{expression_apply({LinkTextWeight{expression_a_ref,expression_b_ref,false}});});
    expression_unchanged("Mixed Text weight dependency cycle rejects atomically");
    rejects("MISSING_REFERENCE",[&]{expression_apply({DeleteObjects{{"expression-a"}}});});
    expression_unchanged("Deleting a Text weight expression source rejects atomically");
    rejects("REVISION_CONFLICT",[&]{expression_session.apply({UnlinkTextWeight{expression_b_ref}},weight_expression_revision-1);});
    expression_unchanged("Stale Text weight expression command leaves native bytes unchanged");
    auto weight_smuggled=*expression_session.document().objects.at("expression-b").text;
    weight_smuggled.weight_expression=Expression{"500",1};
    rejects("DRIVEN_PROPERTY",[&]{expression_apply({UpdateText{"expression-b",weight_smuggled}});});
    expression_unchanged("UpdateText cannot smuggle a replacement expression");
    auto old_expression_version=test_support::without_empty_presets_for_legacy_fixture(expression_native);
    const auto version_position=old_expression_version.find("\"version\":\"0.65\"");
    check(version_position!=std::string::npos,"Native Text weight expression identifies writer 0.65");
    old_expression_version.replace(version_position,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.54\"");
    rejects("UNSUPPORTED_TEXT_WEIGHT_EXPRESSION",[&]{decode(old_expression_version);});
    auto conflicting_expression=expression_native;
    const auto expression_field=conflicting_expression.find("\"weight_expression\":");
    check(expression_field!=std::string::npos,"Native Text weight expression field exists");
    conflicting_expression.insert(expression_field,"\"weight_driver\":{\"link\":{\"object\":\"expression-a\",\"point\":\"\",\"field\":\"text.weight\"}},");
    rejects("TEXT_WEIGHT_SOURCE_CONFLICT",[&]{decode(conflicting_expression);});
    auto updated_a=*expression_session.document().objects.at("expression-a").text;updated_a.weight=500;
    expression_apply({UpdateText{"expression-a",updated_a},Rename{"expression-a","Renamed A"}});
    check(evaluate_text_weight(expression_session.document(),"expression-b")==600&&
        evaluated_text_source(expression_session.document(),"expression-b").weight==600&&
        expression_session.document().objects.at("expression-b").text->weight==400,
        "Source edit and rename feed evaluated Text used by rendering while preserving the authored literal");
    const auto before_invalid_upstream=encode(expression_session.document());
    const auto before_invalid_revision=expression_session.revision();
    auto invalid_upstream=*expression_session.document().objects.at("expression-a").text;
    invalid_upstream.weight=900;
    rejects("OUT_OF_RANGE",[&]{expression_apply({UpdateText{"expression-a",invalid_upstream}});});
    check(expression_session.revision()==before_invalid_revision&&
        encode(expression_session.document())==before_invalid_upstream&&
        evaluate_text_weight(expression_session.document(),"expression-b")==600,
        "Upstream edit producing weight 1000 rejects without mutating the source or prior evaluation");
    rejects("OUT_OF_RANGE",[&]{expression_apply({Rename{"expression-a","Batch rename"},
        UpdateText{"expression-a",invalid_upstream}});});
    check(expression_session.revision()==before_invalid_revision&&
        encode(expression_session.document())==before_invalid_upstream,
        "Failed later batch command rolls back the earlier Text rename and all authored bytes");
    expression_apply({UnlinkTextWeight{expression_b_ref}});
    check(expression_session.document().objects.at("expression-b").text->weight==600&&
        !expression_session.document().objects.at("expression-b").text->weight_expression,
        "Unlink freezes the evaluated exact integer into the Text weight literal");
    expression_session.undo(expression_session.revision());
    check(expression_session.document().objects.at("expression-b").text->weight_expression==weight_expression&&
        evaluate_text_weight(expression_session.document(),"expression-b")==600,
        "Undo restores exact Text weight expression and evaluated integer");
    Session weight_batch_session(empty_document("weight-batch-doc","weight-batch-comp","weight-batch-frame"));
    auto weight_batch_apply=[&](std::vector<Command> commands){weight_batch_session.apply(commands,weight_batch_session.revision());};
    auto batch_source=default_text("weight-batch-source","Source");batch_source.weight=100;
    auto batch_b=default_text("weight-batch-b-source","B");batch_b.weight=140;
    auto batch_c=default_text("weight-batch-c-source","C");batch_c.weight=200;
    auto batch_d=default_text("weight-batch-d-source","D");batch_d.weight=300;
    weight_batch_apply({CreateText{"weight-batch-comp","","weight-batch-a","A",batch_source},
        CreateText{"weight-batch-comp","","weight-batch-b","B",batch_b},
        CreateText{"weight-batch-comp","","weight-batch-c","C",batch_c},
        CreateText{"weight-batch-comp","","weight-batch-d","D",batch_d}});
    const Ref batch_a_ref{"weight-batch-a","","text.weight"},batch_b_ref{"weight-batch-b","","text.weight"},
        batch_c_ref{"weight-batch-c","","text.weight"},batch_d_ref{"weight-batch-d","","text.weight"};
    weight_batch_apply({TextWeightBatch{TextWeightBatchMode::edit,{batch_b_ref,batch_c_ref,batch_d_ref},400}});
    check(evaluate_text_weight(weight_batch_session.document(),"weight-batch-b")==400&&
        evaluate_text_weight(weight_batch_session.document(),"weight-batch-c")==400&&
        evaluate_text_weight(weight_batch_session.document(),"weight-batch-d")==400,
        "Absolute typed batch edit applies one integer to every exact Text weight Ref");
    weight_batch_session.undo(weight_batch_session.revision());
    weight_batch_apply({TextWeightBatch{TextWeightBatchMode::edit,{batch_b_ref,batch_c_ref,batch_d_ref},10,{},true}});
    check(evaluate_text_weight(weight_batch_session.document(),"weight-batch-b")==150&&
        evaluate_text_weight(weight_batch_session.document(),"weight-batch-c")==210&&
        evaluate_text_weight(weight_batch_session.document(),"weight-batch-d")==310,
        "Relative typed batch edit snapshots each target before adding the signed delta");
    weight_batch_session.undo(weight_batch_session.revision());
    auto batch_b_999=*weight_batch_session.document().objects.at("weight-batch-b").text;batch_b_999.weight=999;
    weight_batch_apply({UpdateText{"weight-batch-b",batch_b_999}});
    const auto batch_before_range=weight_batch_session.document();const auto batch_before_range_revision=weight_batch_session.revision();
    const auto batch_before_range_history=weight_batch_session.history();
    rejects("OUT_OF_RANGE",[&]{weight_batch_apply({TextWeightBatch{TextWeightBatchMode::edit,
        {batch_c_ref,batch_b_ref},1,{},true}});});
    check(weight_batch_session.document()==batch_before_range&&weight_batch_session.revision()==batch_before_range_revision&&
        weight_batch_session.history()==batch_before_range_history,
        "A later relative weight overflow rejects the whole target snapshot and preserves Undo history");
    batch_b_999.weight=140;weight_batch_apply({UpdateText{"weight-batch-b",batch_b_999}});
    weight_batch_apply({TextWeightBatch{TextWeightBatchMode::link,{batch_b_ref},0,batch_a_ref,true}});
    check(text_weight_property(weight_batch_session.document(),batch_b_ref).literal==140&&
        text_weight_property(weight_batch_session.document(),batch_b_ref).driver==TextWeightDriver{batch_a_ref,40}&&
        evaluate_text_weight(weight_batch_session.document(),"weight-batch-b")==140,
        "Relative batch link stores each exact signed difference beside the authored literal");
    const auto batch_link_revision=weight_batch_session.revision();const auto batch_link_history=weight_batch_session.history();
    weight_batch_apply({TextWeightBatch{TextWeightBatchMode::link,{batch_b_ref},0,batch_a_ref,true}});
    check(weight_batch_session.revision()==batch_link_revision&&weight_batch_session.history()==batch_link_history,
        "Reapplying the same source and relative offset leaves revision and history unchanged");
    auto batch_source_120=*weight_batch_session.document().objects.at("weight-batch-a").text;batch_source_120.weight=120;
    weight_batch_apply({UpdateText{"weight-batch-a",batch_source_120}});
    check(evaluate_text_weight(weight_batch_session.document(),"weight-batch-b")==160&&
        weight_batch_session.document().objects.at("weight-batch-b").text->weight==140,
        "Relative linked weight follows source changes while retaining its target literal");
    weight_batch_apply({TextWeightBatch{TextWeightBatchMode::link,{batch_c_ref},0,batch_a_ref,false}});
    check(weight_batch_session.document().objects.at("weight-batch-c").text->weight_driver==TextWeightDriver{batch_a_ref,0},
        "Absolute typed link uses a zero offset");
    auto batch_source_130=*weight_batch_session.document().objects.at("weight-batch-a").text;batch_source_130.weight=130;
    weight_batch_apply({UpdateText{"weight-batch-a",batch_source_130}});
    check(evaluate_text_weight(weight_batch_session.document(),"weight-batch-b")==170&&
        evaluate_text_weight(weight_batch_session.document(),"weight-batch-c")==130,
        "Relative and absolute Text weight links preserve their separate modes");
    weight_batch_apply({TextWeightBatch{TextWeightBatchMode::link,{batch_b_ref},0,batch_d_ref,true,true}});
    check(weight_batch_session.document().objects.at("weight-batch-b").text->weight_driver==TextWeightDriver{batch_d_ref,-130}&&
        evaluate_text_weight(weight_batch_session.document(),"weight-batch-b")==170&&
        encode(weight_batch_session.document()).find("\"offset\":-130")!=std::string::npos,
        "Relative replacement retains a negative signed offset without changing the target evaluation");
    weight_batch_session.undo(weight_batch_session.revision());
    check(weight_batch_session.document().objects.at("weight-batch-b").text->weight_driver==TextWeightDriver{batch_a_ref,40},
        "One Undo restores the prior single-target absolute relationship after relative replacement");
    const auto batch_linked_native=encode(weight_batch_session.document());
    check(batch_linked_native.find("\"version\":\"0.65\"")!=std::string::npos&&
        batch_linked_native.find("\"offset\":40")!=std::string::npos&&
        batch_linked_native.find("\"weight_driver\":{\"link\"")!=std::string::npos&&
        encode(decode(batch_linked_native))==batch_linked_native,
        "Native 0.61 persists nonzero relative weight offset and byte-roundtrips it");
    auto lied_weight_offset=test_support::without_empty_presets_for_legacy_fixture(batch_linked_native);const auto batch_version_at=lied_weight_offset.find("\"version\":\"0.65\"");
    check(batch_version_at!=std::string::npos,"Native relative weight fixture identifies writer 0.65");
    auto malformed_weight_offset=batch_linked_native;const auto batch_offset_at=malformed_weight_offset.find("\"offset\":40");
    check(batch_offset_at!=std::string::npos,"Native relative weight fixture includes a signed offset");
    malformed_weight_offset.replace(batch_offset_at,std::string("\"offset\":40").size(),"\"offset\":40.5");
    rejects("INVALID_INPUT",[&]{decode(malformed_weight_offset);});
    lied_weight_offset.replace(batch_version_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.60\"");
    rejects("UNSUPPORTED_TEXT_WEIGHT_OFFSET",[&]{decode(lied_weight_offset);});
    weight_batch_apply({TextWeightBatch{TextWeightBatchMode::unlink,{batch_b_ref,batch_c_ref}}});
    check(weight_batch_session.document().objects.at("weight-batch-b").text->weight==170&&
        weight_batch_session.document().objects.at("weight-batch-c").text->weight==130&&
        !weight_batch_session.document().objects.at("weight-batch-b").text->weight_driver&&
        !weight_batch_session.document().objects.at("weight-batch-c").text->weight_driver,
        "Batch unlink freezes each evaluated integer and clears both stable sources in one command");
    weight_batch_session.undo(weight_batch_session.revision());
    check(weight_batch_session.document().objects.at("weight-batch-b").text->weight_driver==TextWeightDriver{batch_a_ref,40}&&
        weight_batch_session.document().objects.at("weight-batch-c").text->weight_driver==TextWeightDriver{batch_a_ref,0},
        "One Undo restores each batch link and its independent offset");
    rejects("DUPLICATE_TARGET",[&]{weight_batch_apply({TextWeightBatch{TextWeightBatchMode::unlink,
        {batch_b_ref,batch_b_ref}}});});
    rejects("DEPENDENCY_CYCLE",[&]{weight_batch_apply({TextWeightBatch{TextWeightBatchMode::link,
        {batch_b_ref},0,batch_b_ref,true,true}});});
    const auto batch_before_failure=weight_batch_session.document();const auto batch_failure_revision=weight_batch_session.revision();
    const auto batch_failure_history=weight_batch_session.history();
    rejects("DRIVEN_PROPERTY",[&]{weight_batch_apply({TextWeightBatch{TextWeightBatchMode::edit,
        {batch_d_ref,batch_b_ref},1,{},true}});});
    check(weight_batch_session.document()==batch_before_failure&&weight_batch_session.revision()==batch_failure_revision&&
        weight_batch_session.history()==batch_failure_history,
        "Editing one driven target rejects all otherwise-valid Text weight targets atomically");
    Session weight_batch_api(empty_document("weight-batch-api-doc","weight-batch-api-comp","weight-batch-api-frame"));
    auto weight_api_a=default_text("weight-batch-api-a-source","API A");weight_api_a.weight=100;
    auto weight_api_b=default_text("weight-batch-api-b-source","API B");weight_api_b.weight=140;
    auto weight_api_c=default_text("weight-batch-api-c-source","API C");weight_api_c.weight=200;
    weight_batch_api.apply({CreateText{"weight-batch-api-comp","","weight-batch-api-a","API A",weight_api_a},
        CreateText{"weight-batch-api-comp","","weight-batch-api-b","API B",weight_api_b},
        CreateText{"weight-batch-api-comp","","weight-batch-api-c","API C",weight_api_c}},0);
    const auto batch_api_apply=[&](std::uint64_t revision,const std::string& commands) {
        return request(weight_batch_api,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(revision)+
            ",\"commands\":["+commands+"]}");
    };
    auto batch_api_reply=batch_api_apply(1,R"({"type":"link_text_weights","targets":[{"object":"weight-batch-api-b","point":"","field":"text.weight"},{"object":"weight-batch-api-c","point":"","field":"text.weight"}],"source":{"object":"weight-batch-api-a","point":"","field":"text.weight"},"relative":true,"replace_driver":false})");
    check(batch_api_reply.find("\"ok\":true")!=std::string::npos&&
        text_weight_property(weight_batch_api.document(),Ref{"weight-batch-api-b","","text.weight"}).driver==
            TextWeightDriver{Ref{"weight-batch-api-a","","text.weight"},40},
        "JSON-lines accepts the typed shared-source relative-link command");
    auto weight_api_source=*weight_batch_api.document().objects.at("weight-batch-api-a").text;weight_api_source.weight=120;
    weight_batch_api.apply({UpdateText{"weight-batch-api-a",weight_api_source}},weight_batch_api.revision());
    const auto api_weight_get=request(weight_batch_api,R"({"op":"get","ref":{"object":"weight-batch-api-b","point":"","field":"text.weight"}})");
    check(api_weight_get.find("\"offset\":40")!=std::string::npos&&api_weight_get.find("\"evaluated\":160")!=std::string::npos,
        "Typed JSON-lines readback exposes exact link offset and evaluated integer");
    batch_api_reply=batch_api_apply(weight_batch_api.revision(),R"({"type":"edit_text_weights","targets":[{"object":"weight-batch-api-b","point":"","field":"text.weight"},{"object":"weight-batch-api-c","point":"","field":"text.weight"}],"value":400,"relative":false})");
    check(batch_api_reply.find("\"ok\":false")!=std::string::npos&&
        batch_api_reply.find("\"code\":\"DRIVEN_PROPERTY\"")!=std::string::npos&&
        evaluate_text_weight(weight_batch_api.document(),"weight-batch-api-b")==160&&
        evaluate_text_weight(weight_batch_api.document(),"weight-batch-api-c")==220,
        "JSON-lines rejects direct batch edits of driven Text weights without changing either evaluation");
    batch_api_reply=batch_api_apply(weight_batch_api.revision(),R"({"type":"unlink_text_weights","targets":[{"object":"weight-batch-api-b","point":"","field":"text.weight"},{"object":"weight-batch-api-c","point":"","field":"text.weight"}]})");
    check(batch_api_reply.find("\"ok\":true")!=std::string::npos&&
        weight_batch_api.document().objects.at("weight-batch-api-b").text->weight==160&&
        weight_batch_api.document().objects.at("weight-batch-api-c").text->weight==220&&
        !weight_batch_api.document().objects.at("weight-batch-api-b").text->weight_driver&&
        !weight_batch_api.document().objects.at("weight-batch-api-c").text->weight_driver,
        "JSON-lines batch unlink freezes each distinct evaluated integer and clears both sources");
    batch_api_reply=batch_api_apply(weight_batch_api.revision(),R"({"type":"edit_text_weights","targets":[{"object":"weight-batch-api-b","point":"","field":"text.weight"},{"object":"weight-batch-api-c","point":"","field":"text.weight"}],"value":400,"relative":false})");
    check(batch_api_reply.find("\"ok\":true")!=std::string::npos&&
        evaluate_text_weight(weight_batch_api.document(),"weight-batch-api-b")==400&&
        evaluate_text_weight(weight_batch_api.document(),"weight-batch-api-c")==400,
        "JSON-lines accepts an atomic absolute typed batch edit after explicit unlink");
    Session content_session(empty_document("content-doc","content-comp","content-frame"));
    auto content_apply=[&](std::vector<Command> commands){content_session.apply(commands,content_session.revision());};
    auto content_a=default_text("content-a-source","Source A");
    auto content_b=default_text("content-b-source","Manual B");
    Point content_path_point;content_path_point.id="content-path-point";
    content_apply({CreateText{"content-comp","","content-a","Content A",content_a},
        CreateText{"content-comp","","content-b","Content B",content_b},
        CreatePath{"content-comp","","content-path","Content Path",{{"content-path-contour",false,{content_path_point}}}}});
    const Ref content_a_ref{"content-a","","text.content"},content_b_ref{"content-b","","text.content"};
    const auto literal_bytes=encode(content_session.document());
    check(literal_bytes.find("\"version\":\"0.65\"")!=std::string::npos&&
        literal_bytes.find("content_driver")==std::string::npos&&encode(decode(literal_bytes))==literal_bytes,
        "Native 0.43 omits absent Text drivers and preserves literal-only Text");
    const auto content_properties=properties(content_session.document());
    check(std::find(content_properties.begin(),content_properties.end(),content_a_ref)!=content_properties.end()&&
        resolve_name(content_session.document(),"Content B","","text.content")==content_b_ref,
        "Text content remains a stable typed property Ref");
    const auto literal_get=request(content_session,R"({"op":"get","ref":{"object":"content-b","point":"","field":"text.content"}})");
    check(literal_get.find("\"type\":\"string\"")!=std::string::npos&&
        literal_get.find("\"authored\":{\"literal\":\"Manual B\",\"driver\":null}")!=std::string::npos&&
        literal_get.find("\"link\":true")!=std::string::npos&&literal_get.find("\"expression\":false")!=std::string::npos,
        "JSON-lines content readback exposes the authored literal and link-only capability");
    const auto content_revision=content_session.revision();
    rejects("MISSING_REFERENCE",[&]{content_apply({LinkProperties{{content_b_ref},content_a_ref,false}});});
    rejects("MISSING_REFERENCE",[&]{content_apply({SetExpression{{content_b_ref},{"1",1},false}});});
    auto smuggled=default_text("smuggled-source","Smuggled");smuggled.content_driver=TextContentDriver{content_a_ref};
    rejects("USE_TYPED_COMMAND",[&]{content_apply({CreateText{"content-comp","","smuggled","Smuggled",smuggled}});});
    check(content_session.revision()==content_revision&&encode(content_session.document())==literal_bytes,
        "Generic Scalar sources and driver smuggling reject without authored changes");
    rejects("MISSING_REFERENCE",[&]{content_apply({LinkTextContent{content_b_ref,{"missing","","text.content"},false}});});
    rejects("INVALID_TEXT_REF",[&]{content_apply({LinkTextContent{content_b_ref,{"content-a","p1","text.content"},false}});});
    rejects("TYPE_MISMATCH",[&]{content_apply({LinkTextContent{{"content-path","","text.content"},content_a_ref,false}});});
    content_apply({LinkTextContent{content_b_ref,content_a_ref,false}});
    const auto linked_content=text_content_property(content_session.document(),content_b_ref);
    check(linked_content.literal=="Manual B"&&linked_content.evaluated=="Source A"&&
        linked_content.driver==TextContentDriver{content_a_ref}&&
        evaluated_text_source(content_session.document(),"content-b").content=="Source A",
        "Same-type content link preserves the target literal and feeds the shared Text projection");
    auto content_get=request(content_session,R"({"op":"get","ref":{"object":"content-b","point":"","field":"text.content"}})");
    check(content_get.find("\"driver\":{\"link\":{\"object\":\"content-a\",\"point\":\"\",\"field\":\"text.content\"}}")!=std::string::npos&&
        content_get.find("\"literal\":\"Manual B\"")!=std::string::npos&&content_get.find("\"evaluated\":\"Source A\"")!=std::string::npos,
        "Linked Text content get reports stable source Ref, authored literal and evaluated string");
    const auto linked_content_bytes=encode(content_session.document());const auto linked_content_revision=content_session.revision();
    rejects("DRIVEN_PROPERTY",[&]{content_apply({LinkTextContent{content_b_ref,content_a_ref,false}});});
    rejects("DRIVEN_PROPERTY",[&]{auto edit=*content_session.document().objects.at("content-b").text;edit.content="Changed B";
        content_apply({UpdateText{"content-b",edit}});});
    auto unrelated_content_edit=*content_session.document().objects.at("content-b").text;unrelated_content_edit.family="Other Family";
    content_apply({UpdateText{"content-b",unrelated_content_edit}});
    check(content_session.document().objects.at("content-b").text->content=="Manual B"&&
        content_session.document().objects.at("content-b").text->content_driver->link==content_a_ref,
        "UpdateText preserves a content driver while editing unrelated Text fields");
    const auto unrelated_revision=content_session.revision();const auto unrelated_bytes=encode(content_session.document());
    rejects("DEPENDENCY_CYCLE",[&]{content_apply({LinkTextContent{content_a_ref,content_b_ref,true}});});
    check(content_session.revision()==unrelated_revision&&encode(content_session.document())==unrelated_bytes,
        "Self and indirect content cycles reject atomically");
    rejects("MISSING_REFERENCE",[&]{content_apply({DeleteObjects{{"content-a"}}});});
    rejects("REVISION_CONFLICT",[&]{content_session.apply({UnlinkTextContent{content_b_ref}},unrelated_revision-1);});
    content_apply({UpdateText{"content-a",[&]{auto edit=*content_session.document().objects.at("content-a").text;edit.content="Revised A";return edit;}()},
        Rename{"content-a","Renamed A"},ReorderObjects{"content-comp","",{"content-b","content-a","content-path"}}});
    check(evaluate_text_content(content_session.document(),"content-b")=="Revised A"&&
        content_session.document().objects.at("content-b").text->content_driver->link==content_a_ref,
        "Content follows the stable Text Ref across source edits, rename and reorder");
    const DuplicateObjects copy_both{{"content-a","content-b"},"content-copy"};
    const auto copied_roots=duplicated_roots(content_session.document(),copy_both);
    content_apply({copy_both});
    const auto copy_b_it=std::find_if(copied_roots.begin(),copied_roots.end(),[&](const auto& id){
        return content_session.document().objects.at(id).text->content_driver.has_value();});
    check(copied_roots.size()==2&&copy_b_it!=copied_roots.end(),"Duplicated roots retain source content-driver identity");
    const auto copy_b=*copy_b_it;
    const auto copy_a=*std::find_if(copied_roots.begin(),copied_roots.end(),[&](const auto& id){return id!=copy_b;});
    check(content_session.document().objects.at(copy_b).text->content_driver->link==Ref{copy_a,"","text.content"}&&
        content_session.document().objects.at("content-b").text->content_driver->link==content_a_ref,
        "Duplicating both linked Text objects remaps only the copied content driver");
    auto edit_copy=*content_session.document().objects.at(copy_a).text;edit_copy.content="Copied source";
    content_apply({UpdateText{copy_a,edit_copy}});
    check(evaluate_text_content(content_session.document(),copy_b)=="Copied source"&&
        evaluate_text_content(content_session.document(),"content-b")=="Revised A",
        "Duplicated content links evaluate independently from their originals");
    const auto copied_link_bytes=encode(content_session.document());
    check(copied_link_bytes.find("\"content_driver\":{\"link\"")!=std::string::npos&&
        encode(decode(copied_link_bytes))==copied_link_bytes,
        "Native 0.43 roundtrip retains the exact Text content driver");
    const auto linked_copy_reopen=decode(copied_link_bytes);
    check(linked_copy_reopen.objects.at(copy_b).text->content_driver->link==Ref{copy_a,"","text.content"}&&
        evaluate_text_content(linked_copy_reopen,copy_b)=="Copied source",
        "Cold codec reopen retains duplicated driver identity and evaluation");
    content_apply({UnlinkTextContent{content_b_ref}});
    check(content_session.document().objects.at("content-b").text->content=="Revised A"&&
        !content_session.document().objects.at("content-b").text->content_driver,
        "Unlink freezes evaluated UTF-8 content into the authored literal");
    content_session.undo(content_session.revision());
    check(content_session.document().objects.at("content-b").text->content_driver->link==content_a_ref&&
        evaluate_text_content(content_session.document(),"content-b")=="Revised A",
        "Undo restores the Text content link and evaluated string");
    content_session.redo(content_session.revision());
    auto changed_content_source=*content_session.document().objects.at("content-a").text;changed_content_source.content="Later A";
    content_apply({UpdateText{"content-a",changed_content_source}});
    check(content_session.document().objects.at("content-b").text->content=="Revised A"&&
        evaluate_text_content(content_session.document(),"content-b")=="Revised A",
        "Unlinked Text content remains frozen after the former source changes");
    const auto frozen_content_bytes=encode(content_session.document());const auto frozen_content_revision=content_session.revision();
    changed_content_source=*content_session.document().objects.at("content-a").text;changed_content_source.content=std::string("\xc0\xaf",2);
    rejects("INVALID_UTF8",[&]{content_apply({UpdateText{"content-a",changed_content_source}});});
    changed_content_source=*content_session.document().objects.at("content-a").text;changed_content_source.content=std::string(32769,'x');
    rejects("LIMIT",[&]{content_apply({UpdateText{"content-a",changed_content_source}});});
    check(content_session.revision()==frozen_content_revision&&encode(content_session.document())==frozen_content_bytes,
        "Invalid UTF-8 and oversized content reject without changing revision or bytes");
    auto malformed_content=copied_link_bytes;const auto content_driver_at=malformed_content.find("\"content_driver\":{\"link\":");
    check(content_driver_at!=std::string::npos,"Native Text content link uses the strict link alternative");
    malformed_content.replace(content_driver_at,std::string("\"content_driver\":{\"link\":").size(),"\"content_driver\":{\"other\":");
    rejects("INVALID_TEXT_CONTENT_DRIVER",[&]{decode(malformed_content);});
    auto wrong_content_ref=copied_link_bytes;const auto content_ref_at=wrong_content_ref.find("text.content",content_driver_at);
    check(content_ref_at!=std::string::npos,"Native Text content Ref field is explicit");
    wrong_content_ref.replace(content_ref_at,std::string("text.content").size(),"text.weight");
    rejects("TYPE_MISMATCH",[&]{decode(wrong_content_ref);});
    auto missing_content_source=copied_link_bytes;const auto content_object_at=missing_content_source.find("content-copy-content-a",content_driver_at);
    if(content_object_at!=std::string::npos)missing_content_source.replace(content_object_at,std::string("content-copy-content-a").size(),"missing-content");
    else {const auto original_source_at=missing_content_source.find("content-a",content_driver_at);check(original_source_at!=std::string::npos,"Native content source ID is explicit");
        missing_content_source.replace(original_source_at,std::string("content-a").size(),"missing");}
    rejects("MISSING_REFERENCE",[&]{decode(missing_content_source);});
    auto old_content_driver=test_support::without_empty_presets_for_legacy_fixture(copied_link_bytes);const auto content_version_at=old_content_driver.find("\"version\":\"0.65\"");
    check(content_version_at!=std::string::npos,"Native content fixture identifies version 0.23");
    old_content_driver.replace(content_version_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.16\"");
    rejects("UNSUPPORTED_TEXT_CONTENT_DRIVER",[&]{decode(old_content_driver);});
    auto legacy_content=test_support::without_empty_presets_for_legacy_fixture(literal_bytes);const auto legacy_version_at=legacy_content.find("\"version\":\"0.65\"");
    legacy_content.replace(legacy_version_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.16\"");
    const auto migrated_content=decode(legacy_content);
    check(migrated_content.objects.at("content-b").text->content=="Manual B"&&
        !migrated_content.objects.at("content-b").text->content_driver,
        "Native 0.16 migrates Text content as a literal");
    check(linked_content_bytes.find("\"content_driver\"")!=std::string::npos,
        "Text content link bytes are distinct from the original literal-only state");
    Session family_session(empty_document("family-doc","family-comp","family-frame"));
    auto family_apply=[&](std::vector<Command> commands){family_session.apply(commands,family_session.revision());};
    auto family_a=default_text("family-a-source","Family source");family_a.family="Segoe UI";
    auto family_b=default_text("family-b-source","Family target");family_b.family="Manual B";
    auto family_c=default_text("family-c-source","Third family");family_c.family="Arial";
    Point family_path_point;family_path_point.id="family-path-point";
    family_apply({CreateText{"family-comp","","family-a","Family A",family_a},
        CreateText{"family-comp","","family-b","Family B",family_b},
        CreateText{"family-comp","","family-c","Family C",family_c},
        CreatePath{"family-comp","","family-path","Family Path",{{"family-path-contour",false,{family_path_point}}}}});
    const Ref family_a_ref{"family-a","","text.family"},family_b_ref{"family-b","","text.family"},family_c_ref{"family-c","","text.family"};
    const auto family_literal_bytes=encode(family_session.document());
    check(family_literal_bytes.find("\"version\":\"0.65\"")!=std::string::npos&&
        family_literal_bytes.find("family_driver")==std::string::npos&&encode(decode(family_literal_bytes))==family_literal_bytes,
        "Native 0.43 omits an absent Text family driver and preserves literal-only family values");
    const auto family_refs=properties(family_session.document());
    check(resolve_name(family_session.document(),"Family B","","text.family")==family_b_ref&&
        std::find(family_refs.begin(),family_refs.end(),family_b_ref)!=family_refs.end(),
        "Text family discovery and name resolution retain the stable object Ref");
    const auto family_get=request(family_session,R"({"op":"get","ref":{"object":"family-b","point":"","field":"text.family"}})");
    check(family_get.find("\"type\":\"string\"")!=std::string::npos&&
        family_get.find("\"authored\":{\"literal\":\"Manual B\",\"driver\":null}")!=std::string::npos&&
        family_get.find("\"link\":true")!=std::string::npos&&family_get.find("\"expression\":false")!=std::string::npos,
        "JSON-lines family readback exposes the authored literal and link-only capability");
    const auto family_revision=family_session.revision();
    rejects("MISSING_REFERENCE",[&]{family_apply({LinkProperties{{family_b_ref},family_a_ref,false}});});
    rejects("MISSING_REFERENCE",[&]{family_apply({SetExpression{{family_b_ref},{"1",1},false}});});
    auto smuggled_family=default_text("smuggled-family-source","Smuggled");smuggled_family.family_driver=TextFamilyDriver{family_a_ref};
    rejects("USE_TYPED_COMMAND",[&]{family_apply({CreateText{"family-comp","","smuggled-family","Smuggled",smuggled_family}});});
    rejects("MISSING_REFERENCE",[&]{family_apply({LinkTextFamily{family_b_ref,{"missing-family","","text.family"},false}});});
    rejects("INVALID_TEXT_REF",[&]{family_apply({LinkTextFamily{family_b_ref,{"family-a","p1","text.family"},false}});});
    rejects("TYPE_MISMATCH",[&]{family_apply({LinkTextFamily{family_b_ref,{"family-a","","text.content"},false}});});
    rejects("TYPE_MISMATCH",[&]{family_apply({LinkTextFamily{{"family-path","","text.family"},family_a_ref,false}});});
    rejects("DEPENDENCY_CYCLE",[&]{family_apply({LinkTextFamily{family_b_ref,family_b_ref,false}});});
    check(family_session.revision()==family_revision&&encode(family_session.document())==family_literal_bytes,
        "Unsupported, malformed and self family links reject without authored changes");
    family_apply({LinkTextFamily{family_b_ref,family_a_ref,false}});
    auto linked_family=text_family_property(family_session.document(),family_b_ref);
    check(linked_family.literal=="Manual B"&&linked_family.evaluated=="Segoe UI"&&
        linked_family.driver==TextFamilyDriver{family_a_ref}&&evaluated_text_source(family_session.document(),"family-b").family=="Segoe UI",
        "Same-type family link preserves the literal and feeds the shared Text projection");
    const auto linked_family_get=request(family_session,R"({"op":"get","ref":{"object":"family-b","point":"","field":"text.family"}})");
    check(linked_family_get.find("\"driver\":{\"link\":{\"object\":\"family-a\",\"point\":\"\",\"field\":\"text.family\"}}")!=std::string::npos&&
        linked_family_get.find("\"literal\":\"Manual B\"")!=std::string::npos&&linked_family_get.find("\"evaluated\":\"Segoe UI\"")!=std::string::npos,
        "Linked Text family get reports stable source Ref, authored literal and evaluated value");
    const auto linked_family_bytes=encode(family_session.document());const auto linked_family_revision=family_session.revision();
    rejects("DRIVEN_PROPERTY",[&]{family_apply({LinkTextFamily{family_b_ref,family_c_ref,false}});});
    rejects("DRIVEN_PROPERTY",[&]{auto edit=*family_session.document().objects.at("family-b").text;edit.family="Changed B";
        family_apply({UpdateText{"family-b",edit}});});
    rejects("REVISION_CONFLICT",[&]{family_session.apply({UnlinkTextFamily{family_b_ref}},linked_family_revision-1);});
    check(family_session.revision()==linked_family_revision&&encode(family_session.document())==linked_family_bytes,
        "Rejected driver replacement, literal edit and stale unlink preserve family authored bytes and revision");
    auto unrelated_family_edit=*family_session.document().objects.at("family-b").text;unrelated_family_edit.content="Changed while family linked";
    family_apply({UpdateText{"family-b",unrelated_family_edit}});
    check(family_session.document().objects.at("family-b").text->family=="Manual B"&&
        family_session.document().objects.at("family-b").text->family_driver->link==family_a_ref,
        "UpdateText preserves the family driver while editing an unrelated Text field");
    const auto preserved_family_revision=family_session.revision();
    family_apply({LinkTextFamily{family_b_ref,family_c_ref,true},LinkTextFamily{family_b_ref,family_a_ref,true}});
    check(family_session.document().objects.at("family-b").text->family_driver->link==family_a_ref&&
        family_session.revision()==preserved_family_revision+1,
        "Explicit driver replacement commits atomically and retains the selected stable source");
    const auto family_revision_before_rename=family_session.revision();
    auto changed_family_a=*family_session.document().objects.at("family-a").text;changed_family_a.family="Revised source family";
    family_apply({UpdateText{"family-a",changed_family_a},Rename{"family-a","Renamed Family A"},
        ReorderObjects{"family-comp","",{"family-b","family-a","family-c","family-path"}}});
    check(evaluate_text_family(family_session.document(),"family-b")=="Revised source family"&&
        evaluated_text_source(family_session.document(),"family-b").family=="Revised source family"&&
        family_session.document().objects.at("family-b").text->family_driver->link==family_a_ref,
        "Family evaluation follows source edits, rename and reorder through the stable Ref");
    const auto dependent_bytes=encode(family_session.document());const auto dependent_revision=family_session.revision();
    rejects("MISSING_REFERENCE",[&]{family_apply({DeleteObjects{{"family-a"}}});});
    check(family_session.revision()==dependent_revision&&encode(family_session.document())==dependent_bytes,
        "Deleting a Text family source with a surviving dependent is atomic");
    const DuplicateObjects copy_family{{"family-a","family-b"},"family-copy"};
    const auto family_copies=duplicated_roots(family_session.document(),copy_family);
    family_apply({copy_family});
    const auto copied_family_target=*std::find_if(family_copies.begin(),family_copies.end(),[&](const auto& id){
        return family_session.document().objects.at(id).text->family_driver.has_value();});
    const auto copied_family_source=*std::find_if(family_copies.begin(),family_copies.end(),[&](const auto& id){return id!=copied_family_target;});
    check(family_session.document().objects.at(copied_family_target).text->family_driver->link==Ref{copied_family_source,"","text.family"}&&
        family_session.document().objects.at("family-b").text->family_driver->link==family_a_ref,
        "Duplicating both linked Text objects remaps the copied family Ref and preserves the original");
    const auto copied_family_bytes=encode(family_session.document());
    check(copied_family_bytes.find("\"family_driver\":{\"link\"")!=std::string::npos&&
        encode(decode(copied_family_bytes))==copied_family_bytes,
        "Native 0.43 codec roundtrip retains the strict Text family driver");
    const auto reopened_family=decode(copied_family_bytes);
    check(reopened_family.objects.at(copied_family_target).text->family_driver->link==Ref{copied_family_source,"","text.family"}&&
        evaluate_text_family(reopened_family,copied_family_target)=="Revised source family",
        "Cold codec reopen preserves family driver identity and evaluation");
    family_apply({UnlinkTextFamily{family_b_ref}});
    check(family_session.document().objects.at("family-b").text->family=="Revised source family"&&
        !family_session.document().objects.at("family-b").text->family_driver,
        "Unlink freezes the evaluated font family into the authored literal");
    family_session.undo(family_session.revision());
    check(family_session.document().objects.at("family-b").text->family_driver->link==family_a_ref&&
        evaluate_text_family(family_session.document(),"family-b")=="Revised source family",
        "Undo restores the Text family link and evaluated value");
    family_session.redo(family_session.revision());
    changed_family_a=*family_session.document().objects.at("family-a").text;changed_family_a.family="Later source family";
    family_apply({UpdateText{"family-a",changed_family_a}});
    check(family_session.document().objects.at("family-b").text->family=="Revised source family"&&
        evaluate_text_family(family_session.document(),"family-b")=="Revised source family",
        "Unlinked Text family remains frozen when the former source changes");
    const auto frozen_family_bytes=encode(family_session.document());const auto frozen_family_revision=family_session.revision();
    auto invalid_family=*family_session.document().objects.at("family-a").text;invalid_family.family=std::string("\xc0\xaf",2);
    rejects("INVALID_UTF8",[&]{family_apply({UpdateText{"family-a",invalid_family}});});
    invalid_family=*family_session.document().objects.at("family-a").text;invalid_family.family=std::string(1025,'x');
    rejects("LIMIT",[&]{family_apply({UpdateText{"family-a",invalid_family}});});
    check(family_session.revision()==frozen_family_revision&&encode(family_session.document())==frozen_family_bytes,
        "Invalid UTF-8 and oversized font-family literals reject without changing revision or bytes");
    const auto copied_target_at=copied_family_bytes.find("\"id\":\""+copied_family_target+"\"");
    check(copied_target_at!=std::string::npos,"Copied Native family target object is explicit");
    auto malformed_family=copied_family_bytes;const auto family_driver_at=malformed_family.find("\"family_driver\":{\"link\":",copied_target_at);
    check(family_driver_at!=std::string::npos,"Native family driver is serialized as a strict link alternative");
    malformed_family.replace(family_driver_at,std::string("\"family_driver\":{\"link\":").size(),"\"family_driver\":{\"other\":");
    rejects("INVALID_TEXT_FAMILY_DRIVER",[&]{decode(malformed_family);});
    auto wrong_family_ref=copied_family_bytes;const auto family_ref_at=wrong_family_ref.find("text.family",family_driver_at);
    check(family_ref_at!=std::string::npos,"Native family Ref declares its exact field");
    wrong_family_ref.replace(family_ref_at,std::string("text.family").size(),"text.content");
    rejects("TYPE_MISMATCH",[&]{decode(wrong_family_ref);});
    auto missing_family_source=copied_family_bytes;const auto copied_source_id=missing_family_source.find(copied_family_source,family_driver_at);
    check(copied_source_id!=std::string::npos,"Copied Native family driver identifies its copied source ID");
    missing_family_source.replace(copied_source_id,copied_family_source.size(),"missing-family");
    rejects("MISSING_REFERENCE",[&]{decode(missing_family_source);});
    auto old_family_driver=test_support::without_empty_presets_for_legacy_fixture(copied_family_bytes);const auto family_version_at=old_family_driver.find("\"version\":\"0.65\"");
    old_family_driver.replace(family_version_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.17\"");
    rejects("UNSUPPORTED_TEXT_FAMILY_DRIVER",[&]{decode(old_family_driver);});
    auto legacy_family=test_support::without_empty_presets_for_legacy_fixture(family_literal_bytes);const auto legacy_family_version=legacy_family.find("\"version\":\"0.65\"");
    legacy_family.replace(legacy_family_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.17\"");
    const auto migrated_family=decode(legacy_family);
    check(migrated_family.objects.at("family-b").text->family=="Manual B"&&
        !migrated_family.objects.at("family-b").text->family_driver,
        "Native 0.17 migrates Text family as its authored literal");
    check(family_revision_before_rename<family_session.revision(),"Family source edit and unlink are separate undoable revisions");
    Session family_depth_session(empty_document("family-depth-doc","family-depth-comp","family-depth-frame"));
    std::vector<Command> family_depth_creates;for(int i=0;i<130;++i) {
        auto text=default_text("family-depth-source-"+std::to_string(i),"Depth "+std::to_string(i));
        family_depth_creates.push_back(CreateText{"family-depth-comp","","family-depth-"+std::to_string(i),
            "Family depth "+std::to_string(i),std::move(text)});
    }
    family_depth_session.apply(family_depth_creates,family_depth_session.revision());
    for(int i=0;i<128;++i)family_depth_session.apply({LinkTextFamily{{"family-depth-"+std::to_string(i),"","text.family"},
        {"family-depth-"+std::to_string(i+1),"","text.family"},false}},family_depth_session.revision());
    check(evaluate_text_family(family_depth_session.document(),"family-depth-0")=="Yu Gothic",
        "Text family evaluator accepts the documented 128-link dependency depth");
    const auto family_depth_revision=family_depth_session.revision();const auto family_depth_bytes=encode(family_depth_session.document());
    rejects("DEPENDENCY_DEPTH",[&]{family_depth_session.apply({LinkTextFamily{{"family-depth-128","","text.family"},
        {"family-depth-129","","text.family"},false}},family_depth_revision);});
    check(family_depth_session.revision()==family_depth_revision&&encode(family_depth_session.document())==family_depth_bytes,
        "Text family dependency depth overflow rejects atomically");
    Session direction_session(empty_document("direction-doc","direction-comp","direction-frame"));
    auto direction_apply=[&](std::vector<Command> commands){direction_session.apply(commands,direction_session.revision());};
    auto direction_a=default_text("direction-a-source","Direction A");
    auto direction_b=default_text("direction-b-source","Direction B");direction_b.direction="vertical";
    auto direction_c=default_text("direction-c-source","Direction C");
    Point direction_path_point;direction_path_point.id="direction-path-point";
    direction_apply({CreateText{"direction-comp","","direction-a","Direction A",direction_a},
        CreateText{"direction-comp","","direction-b","Direction B",direction_b},
        CreateText{"direction-comp","","direction-c","Direction C",direction_c},
        CreatePath{"direction-comp","","direction-path","Direction Path",{{"direction-path-contour",false,{direction_path_point}}}}});
    const Ref direction_a_ref{"direction-a","","text.direction"},direction_b_ref{"direction-b","","text.direction"},
        direction_c_ref{"direction-c","","text.direction"};
    const auto direction_literal_bytes=encode(direction_session.document());
    check(direction_literal_bytes.find("\"version\":\"0.65\"")!=std::string::npos&&
        direction_literal_bytes.find("direction_driver")==std::string::npos&&encode(decode(direction_literal_bytes))==direction_literal_bytes,
        "Native 0.43 omits an absent Text direction driver and preserves the literal enum");
    const auto direction_refs=properties(direction_session.document());
    check(resolve_name(direction_session.document(),"Direction B","","text.direction")==direction_b_ref&&
        std::find(direction_refs.begin(),direction_refs.end(),direction_b_ref)!=direction_refs.end(),
        "Text direction discovery and name resolution retain the stable object Ref");
    const auto direction_get=request(direction_session,R"({"op":"get","ref":{"object":"direction-b","point":"","field":"text.direction"}})");
    check(direction_get.find("\"type\":\"enum\"")!=std::string::npos&&
        direction_get.find("\"choices\":[\"horizontal\",\"vertical\"]")!=std::string::npos&&
        direction_get.find("\"authored\":{\"literal\":\"vertical\",\"driver\":null}")!=std::string::npos&&
        direction_get.find("\"link\":true")!=std::string::npos&&direction_get.find("\"expression\":false")!=std::string::npos,
        "Text direction get exposes the authored enum and link-only capability");
    const auto direction_revision=direction_session.revision();
    rejects("MISSING_REFERENCE",[&]{direction_apply({LinkProperties{{direction_b_ref},direction_a_ref,false}});});
    rejects("MISSING_REFERENCE",[&]{direction_apply({SetExpression{{direction_b_ref},{"1",1},false}});});
    auto smuggled_direction=default_text("smuggled-direction-source","Smuggled");smuggled_direction.direction_driver=TextDirectionDriver{direction_a_ref};
    rejects("USE_TYPED_COMMAND",[&]{direction_apply({CreateText{"direction-comp","","smuggled-direction","Smuggled",smuggled_direction}});});
    rejects("MISSING_REFERENCE",[&]{direction_apply({LinkTextDirection{direction_b_ref,{"missing-direction","","text.direction"},false}});});
    rejects("INVALID_TEXT_REF",[&]{direction_apply({LinkTextDirection{direction_b_ref,{"direction-a","p1","text.direction"},false}});});
    rejects("TYPE_MISMATCH",[&]{direction_apply({LinkTextDirection{direction_b_ref,{"direction-a","","text.family"},false}});});
    rejects("TYPE_MISMATCH",[&]{direction_apply({LinkTextDirection{direction_b_ref,{"direction-path","","text.direction"},false}});});
    rejects("DEPENDENCY_CYCLE",[&]{direction_apply({LinkTextDirection{direction_b_ref,direction_b_ref,false}});});
    check(direction_session.revision()==direction_revision&&encode(direction_session.document())==direction_literal_bytes,
        "Unsupported, malformed and self Text direction links reject without authored changes");
    direction_apply({LinkTextDirection{direction_b_ref,direction_a_ref,false}});
    check(direction_session.history().states.back().label.find("Link Text direction")!=std::string::npos,
        "Text direction link has a readable History label");
    auto linked_direction=text_direction_property(direction_session.document(),direction_b_ref);
    check(linked_direction.literal=="vertical"&&linked_direction.evaluated=="horizontal"&&
        linked_direction.driver==TextDirectionDriver{direction_a_ref}&&evaluated_text_source(direction_session.document(),"direction-b").direction=="horizontal",
        "Same-type direction links preserve the literal and feed the shared Text projection");
    const auto direction_ref_history_bytes=[](const Id& source_id) {
        Session measured(empty_document("direction-history-doc","direction-history-comp","direction-history-frame"));
        const auto source_text=default_text("direction-history-source-text","Source");
        const auto target_text=default_text("direction-history-target-source","Target");
        measured.apply({CreateText{"direction-history-comp","",source_id,"Source",source_text},
            CreateText{"direction-history-comp","","direction-history-target","Target",target_text}},0);
        measured.apply({LinkTextDirection{{"direction-history-target","","text.direction"},
            {source_id,"","text.direction"},false}},1);
        return measured.history().states.back().estimated_bytes;
    };
    check(direction_ref_history_bytes("s")+
        60<direction_ref_history_bytes(std::string(96,'r')),
        "History estimates include allocated Text direction Ref storage");
    const auto linked_direction_get=request(direction_session,R"({"op":"get","ref":{"object":"direction-b","point":"","field":"text.direction"}})");
    check(linked_direction_get.find("\"driver\":{\"link\":{\"object\":\"direction-a\",\"point\":\"\",\"field\":\"text.direction\"}}")!=std::string::npos&&
        linked_direction_get.find("\"literal\":\"vertical\"")!=std::string::npos&&linked_direction_get.find("\"evaluated\":\"horizontal\"")!=std::string::npos,
        "Linked Text direction get reports its exact source Ref and evaluated enum");
    const auto linked_direction_bytes=encode(direction_session.document());const auto linked_direction_revision=direction_session.revision();
    rejects("DRIVEN_PROPERTY",[&]{direction_apply({LinkTextDirection{direction_b_ref,direction_c_ref,false}});});
    rejects("DRIVEN_PROPERTY",[&]{auto edit=*direction_session.document().objects.at("direction-b").text;edit.direction="horizontal";
        direction_apply({UpdateText{"direction-b",edit}});});
    rejects("REVISION_CONFLICT",[&]{direction_session.apply({UnlinkTextDirection{direction_b_ref}},linked_direction_revision-1);});
    check(direction_session.revision()==linked_direction_revision&&encode(direction_session.document())==linked_direction_bytes,
        "Rejected driver replacement, literal edit and stale unlink preserve direction bytes and revision");
    auto unrelated_direction_edit=*direction_session.document().objects.at("direction-b").text;unrelated_direction_edit.content="Unrelated edit";
    direction_apply({UpdateText{"direction-b",unrelated_direction_edit}});
    check(direction_session.document().objects.at("direction-b").text->direction=="vertical"&&
        direction_session.document().objects.at("direction-b").text->direction_driver->link==direction_a_ref,
        "UpdateText preserves the direction driver while editing another Text field");
    const auto direction_revision_before_source_change=direction_session.revision();
    direction_a=*direction_session.document().objects.at("direction-a").text;direction_a.direction="vertical";
    direction_apply({UpdateText{"direction-a",direction_a},Rename{"direction-a","Renamed Direction A"},
        ReorderObjects{"direction-comp","",{"direction-b","direction-a","direction-c","direction-path"}}});
    check(evaluate_text_direction(direction_session.document(),"direction-b")=="vertical"&&
        evaluated_text_source(direction_session.document(),"direction-b").direction=="vertical"&&
        direction_session.document().objects.at("direction-b").text->direction_driver->link==direction_a_ref,
        "Direction evaluation follows source edits, rename and reorder through the stable Ref");
    const auto direction_dependent_bytes=encode(direction_session.document());const auto direction_dependent_revision=direction_session.revision();
    rejects("MISSING_REFERENCE",[&]{direction_apply({DeleteObjects{{"direction-a"}}});});
    check(direction_session.revision()==direction_dependent_revision&&encode(direction_session.document())==direction_dependent_bytes,
        "Deleting a Text direction source with a surviving dependent is atomic");
    direction_apply({LinkTextDirection{direction_b_ref,direction_c_ref,true},LinkTextDirection{direction_b_ref,direction_a_ref,true}});
    check(direction_session.document().objects.at("direction-b").text->direction_driver->link==direction_a_ref,
        "Explicit Text direction driver replacement remains atomic");
    const DuplicateObjects copy_direction{{"direction-a","direction-b"},"direction-copy"};
    const auto direction_copies=duplicated_roots(direction_session.document(),copy_direction);direction_apply({copy_direction});
    const auto copied_direction_target=*std::find_if(direction_copies.begin(),direction_copies.end(),[&](const auto& id){
        return direction_session.document().objects.at(id).text->direction_driver.has_value();});
    const auto copied_direction_source=*std::find_if(direction_copies.begin(),direction_copies.end(),[&](const auto& id){return id!=copied_direction_target;});
    check(direction_session.document().objects.at(copied_direction_target).text->direction_driver->link==Ref{copied_direction_source,"","text.direction"}&&
        direction_session.document().objects.at("direction-b").text->direction_driver->link==direction_a_ref,
        "Duplicating both linked Text objects remaps the copied direction Ref and retains the original");
    const auto copied_direction_bytes=encode(direction_session.document());
    check(copied_direction_bytes.find("\"direction_driver\":{\"link\"")!=std::string::npos&&
        encode(decode(copied_direction_bytes))==copied_direction_bytes,
        "Native 0.43 codec roundtrip retains the closed Text direction link");
    auto reopened_direction=decode(copied_direction_bytes);
    check(reopened_direction.objects.at(copied_direction_target).text->direction_driver->link==Ref{copied_direction_source,"","text.direction"}&&
        evaluate_text_direction(reopened_direction,copied_direction_target)=="vertical",
        "Cold codec reopen preserves Text direction identity and evaluation");
    direction_apply({UnlinkTextDirection{direction_b_ref}});
    check(direction_session.history().states.back().label.find("Unlink Text direction")!=std::string::npos,
        "Text direction unlink has a readable History label");
    check(direction_session.document().objects.at("direction-b").text->direction=="vertical"&&
        !direction_session.document().objects.at("direction-b").text->direction_driver,
        "Unlink freezes the evaluated Text direction into its authored literal");
    direction_session.undo(direction_session.revision());
    check(direction_session.document().objects.at("direction-b").text->direction_driver->link==direction_a_ref&&
        evaluate_text_direction(direction_session.document(),"direction-b")=="vertical",
        "Undo restores the Text direction link and evaluated choice");
    direction_session.redo(direction_session.revision());
    direction_a=*direction_session.document().objects.at("direction-a").text;direction_a.direction="horizontal";
    direction_apply({UpdateText{"direction-a",direction_a}});
    check(direction_session.document().objects.at("direction-b").text->direction=="vertical"&&
        evaluate_text_direction(direction_session.document(),"direction-b")=="vertical",
        "Unlinked Text direction stays frozen after the former source changes");
    const auto frozen_direction_bytes=encode(direction_session.document());const auto frozen_direction_revision=direction_session.revision();
    auto invalid_direction=*direction_session.document().objects.at("direction-a").text;invalid_direction.direction="diagonal";
    rejects("UNSUPPORTED_TEXT_DIRECTION",[&]{direction_apply({UpdateText{"direction-a",invalid_direction}});});
    check(direction_session.revision()==frozen_direction_revision&&encode(direction_session.document())==frozen_direction_bytes,
        "Unsupported Text direction literals reject without changing revision or native bytes");
    const auto copied_direction_target_at=copied_direction_bytes.find("\"id\":\""+copied_direction_target+"\"");
    check(copied_direction_target_at!=std::string::npos,"Copied Native direction target is explicit");
    auto malformed_direction=copied_direction_bytes;const auto direction_driver_at=malformed_direction.find("\"direction_driver\":{\"link\":",copied_direction_target_at);
    check(direction_driver_at!=std::string::npos,"Native Text direction driver is serialized as a link alternative");
    malformed_direction.replace(direction_driver_at,std::string("\"direction_driver\":{\"link\":").size(),"\"direction_driver\":{\"other\":");
    rejects("INVALID_TEXT_DIRECTION_DRIVER",[&]{decode(malformed_direction);});
    auto wrong_direction_ref=copied_direction_bytes;const auto direction_ref_at=wrong_direction_ref.find("text.direction",direction_driver_at);
    check(direction_ref_at!=std::string::npos,"Native Text direction Ref declares its exact field");
    wrong_direction_ref.replace(direction_ref_at,std::string("text.direction").size(),"text.family");
    rejects("TYPE_MISMATCH",[&]{decode(wrong_direction_ref);});
    auto missing_direction_source=copied_direction_bytes;const auto copied_direction_source_at=missing_direction_source.find(copied_direction_source,direction_driver_at);
    check(copied_direction_source_at!=std::string::npos,"Copied Native direction link identifies its source ID");
    missing_direction_source.replace(copied_direction_source_at,copied_direction_source.size(),"missing-direction");
    rejects("MISSING_REFERENCE",[&]{decode(missing_direction_source);});
    auto old_direction_driver=test_support::without_empty_presets_for_legacy_fixture(copied_direction_bytes);const auto direction_version_at=old_direction_driver.find("\"version\":\"0.65\"");
    old_direction_driver.replace(direction_version_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.18\"");
    rejects("UNSUPPORTED_TEXT_DIRECTION_DRIVER",[&]{decode(old_direction_driver);});
    auto legacy_direction=test_support::without_empty_presets_for_legacy_fixture(direction_literal_bytes);const auto legacy_direction_version=legacy_direction.find("\"version\":\"0.65\"");
    legacy_direction.replace(legacy_direction_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.18\"");
    const auto migrated_direction=decode(legacy_direction);
    check(migrated_direction.objects.at("direction-b").text->direction=="vertical"&&
        !migrated_direction.objects.at("direction-b").text->direction_driver,
        "Native 0.18 migrates Text direction as its authored literal");
    Session direction_depth_session(empty_document("direction-depth-doc","direction-depth-comp","direction-depth-frame"));
    std::vector<Command> direction_depth_creates;for(int i=0;i<130;++i) {
        auto text=default_text("direction-depth-source-"+std::to_string(i),"Depth "+std::to_string(i));
        direction_depth_creates.push_back(CreateText{"direction-depth-comp","","direction-depth-"+std::to_string(i),
            "Direction depth "+std::to_string(i),std::move(text)});
    }
    direction_depth_session.apply(direction_depth_creates,direction_depth_session.revision());
    for(int i=0;i<128;++i)direction_depth_session.apply({LinkTextDirection{{"direction-depth-"+std::to_string(i),"","text.direction"},
        {"direction-depth-"+std::to_string(i+1),"","text.direction"},false}},direction_depth_session.revision());
    check(evaluate_text_direction(direction_depth_session.document(),"direction-depth-0")=="horizontal",
        "Text direction evaluator accepts the documented 128-link dependency depth");
    const auto direction_depth_revision=direction_depth_session.revision();const auto direction_depth_bytes=encode(direction_depth_session.document());
    rejects("DEPENDENCY_DEPTH",[&]{direction_depth_session.apply({LinkTextDirection{{"direction-depth-128","","text.direction"},
        {"direction-depth-129","","text.direction"},false}},direction_depth_revision);});
    check(direction_depth_session.revision()==direction_depth_revision&&encode(direction_depth_session.document())==direction_depth_bytes,
        "Text direction dependency depth overflow rejects atomically");
    check(direction_revision_before_source_change<direction_session.revision(),"Text direction source edits and unlink remain undoable revisions");
    Session layout_session(empty_document("layout-link-doc","layout-link-comp","layout-link-frame"));
    auto layout_apply=[&](std::vector<Command> commands){layout_session.apply(commands,layout_session.revision());};
    auto layout_source=default_text("layout-source-text","Source layout");layout_source.layout="auto";
    auto layout_target=default_text("layout-target-text","Target words that wrap within a fixed frame");layout_target.layout="frame";
    layout_target.parameters.at("font_size").literal=28;layout_target.parameters.at("frame_width").literal=96;
    layout_target.parameters.at("frame_height").literal=48;
    auto layout_auto_twin=layout_target;layout_auto_twin.id="layout-auto-twin-text";layout_auto_twin.layout="auto";
    auto layout_frame_twin=layout_target;layout_frame_twin.id="layout-frame-twin-text";
    auto layout_alternate=default_text("layout-alternate-text","Alternate layout");layout_alternate.layout="frame";
    const Ref layout_a_ref{"layout-a","","text.layout"},layout_b_ref{"layout-b","","text.layout"},
        layout_c_ref{"layout-c","","text.layout"};
    layout_apply({CreateText{"layout-link-comp","","layout-a","Layout A",layout_source},
        CreateText{"layout-link-comp","","layout-b","Layout B",layout_target},
        CreateText{"layout-link-comp","","layout-auto-twin","Auto twin",layout_auto_twin},
        CreateText{"layout-link-comp","","layout-frame-twin","Frame twin",layout_frame_twin},
        CreateText{"layout-link-comp","","layout-c","Layout C",layout_alternate},
        CreatePrimitive{"layout-link-comp","","layout-path","Layout path",default_primitive("layout-path-source","nect.shape.circle")}});
    const auto layout_literal_bytes=encode(layout_session.document());
    check(layout_literal_bytes.find("\"version\":\"0.65\"")!=std::string::npos&&
        layout_literal_bytes.find("\"layout_driver\"")==std::string::npos,
        "Native 0.43 omits an absent Text layout driver and retains literal-only layout choices");
    const auto discovered_layout_properties=properties(layout_session.document());
    check(resolve_name(layout_session.document(),"Layout B","","text.layout")==layout_b_ref&&
        std::find(discovered_layout_properties.begin(),discovered_layout_properties.end(),layout_b_ref)!=discovered_layout_properties.end(),
        "Text layout name resolution and property discovery retain the stable enum Ref");
    const auto layout_get=request(layout_session,R"({"op":"get","ref":{"object":"layout-b","point":"","field":"text.layout"}})");
    check(layout_get.find("\"type\":\"enum\"")!=std::string::npos&&
        layout_get.find("\"choices\":[\"auto\",\"frame\"]")!=std::string::npos&&
        layout_get.find("\"authored\":{\"literal\":\"frame\",\"driver\":null}")!=std::string::npos,
        "Typed Text layout get exposes the closed enum and authored literal");
    auto layout_metadata=request(layout_session,R"({"op":"properties"})");
    check(layout_metadata.find("\"field\":\"text.layout\"")!=std::string::npos&&
        layout_metadata.find("\"evaluated\":\"frame\"")!=std::string::npos,
        "Text layout properties expose the evaluated choice");
    auto measure_layout=[&](const Document& document,const Id& object) {
        const auto values=evaluate(document);auto evaluated=evaluated_text_source(document,object);
        std::map<std::string,double> parameters;
        for(const auto& [name,scalar]:evaluated.parameters){(void)scalar;parameters[name]=values.at({object,"","text."+name});}
        return evaluate_text(evaluated,parameters);
    };
    const auto same_layout=[](const TextLayout& a,const TextLayout& b) {
        return a.x==b.x&&a.y==b.y&&a.width==b.width&&a.height==b.height&&a.overflow==b.overflow&&
            a.glyph_count==b.glyph_count&&a.warnings==b.warnings&&a.used_fonts==b.used_fonts;
    };
    const auto layout_initial_frame=measure_layout(layout_session.document(),"layout-b");
    const auto layout_history_before=layout_session.history().retained_bytes;
    layout_apply({LinkTextLayout{layout_b_ref,layout_a_ref,false}});
    check(layout_session.document().objects.at("layout-b").text->layout=="frame"&&
        layout_session.document().objects.at("layout-b").text->parameters.at("frame_width").literal==96&&
        layout_session.document().objects.at("layout-b").text->parameters.at("frame_height").literal==48&&
        evaluate_text_layout(layout_session.document(),"layout-b")=="auto"&&
        layout_session.document().objects.at("layout-b").text->layout_driver->link==layout_a_ref&&
        same_layout(measure_layout(layout_session.document(),"layout-b"),measure_layout(layout_session.document(),"layout-auto-twin"))&&
        !same_layout(layout_initial_frame,measure_layout(layout_session.document(),"layout-b")),
        "Text layout link projects auto sizing while preserving the target literal and its own frame dimensions");
    check(layout_session.history().retained_bytes>layout_history_before&&
        layout_session.history().states.back().label.find("Link Text layout")!=std::string::npos,
        "Text layout link is admitted to History with a readable label and retained state accounting");
    auto layout_source_edit=*layout_session.document().objects.at("layout-a").text;layout_source_edit.layout="frame";
    layout_apply({UpdateText{"layout-a",layout_source_edit}});
    const auto layout_linked_frame=measure_layout(layout_session.document(),"layout-b");
    check(evaluate_text_layout(layout_session.document(),"layout-b")=="frame"&&
        same_layout(layout_linked_frame,measure_layout(layout_session.document(),"layout-frame-twin"))&&
        layout_session.document().objects.at("layout-b").text->parameters.at("frame_width").literal==96&&
        layout_session.document().objects.at("layout-b").text->parameters.at("frame_height").literal==48,
        "Linked frame sizing wraps and overflows from the target's frame dimensions");
    layout_apply({Rename{"layout-a","Renamed Layout A"},ReorderObjects{"layout-link-comp","",{"layout-b","layout-a","layout-auto-twin","layout-frame-twin","layout-c","layout-path"}}});
    check(evaluate_text_layout(layout_session.document(),"layout-b")=="frame"&&
        layout_session.document().objects.at("layout-b").text->layout_driver->link==layout_a_ref,
        "Rename and reorder retain the same-field Text layout Ref");
    const DuplicateObjects copy_layout{{"layout-a","layout-b"},"layout-copy"};
    const auto layout_copies=duplicated_roots(layout_session.document(),copy_layout);layout_apply({copy_layout});
    const auto copied_layout_target=*std::find_if(layout_copies.begin(),layout_copies.end(),[&](const auto& id){
        return layout_session.document().objects.at(id).text->layout_driver.has_value();});
    const auto copied_layout_source=*std::find_if(layout_copies.begin(),layout_copies.end(),[&](const auto& id){return id!=copied_layout_target;});
    check(layout_session.document().objects.at(copied_layout_target).text->layout_driver->link==Ref{copied_layout_source,"","text.layout"}&&
        layout_session.document().objects.at("layout-b").text->layout_driver->link==layout_a_ref,
        "Duplicating both linked Text objects remaps the copied layout Ref and retains the original");
    const auto layout_revision=layout_session.revision();const auto layout_bytes=encode(layout_session.document());
    rejects("MISSING_REFERENCE",[&]{layout_apply({DeleteObjects{{"layout-a"}}});});
    check(layout_session.revision()==layout_revision&&encode(layout_session.document())==layout_bytes,
        "Deleting a Text layout source with a surviving dependent is atomic");
    rejects("DRIVEN_PROPERTY",[&]{layout_apply({LinkTextLayout{layout_b_ref,layout_c_ref,false}});});
    auto smuggled_layout=*layout_session.document().objects.at("layout-c").text;
    smuggled_layout.id="layout-smuggled-text";smuggled_layout.layout_driver=TextLayoutDriver{layout_a_ref};
    rejects("USE_TYPED_COMMAND",[&]{layout_apply({CreateText{"layout-link-comp","","layout-smuggled","Smuggled layout",smuggled_layout}});});
    auto driven_layout_edit=*layout_session.document().objects.at("layout-b").text;driven_layout_edit.layout="auto";
    rejects("DRIVEN_PROPERTY",[&]{layout_apply({UpdateText{"layout-b",driven_layout_edit}});});
    rejects("MISSING_REFERENCE",[&]{layout_apply({LinkProperties{{layout_b_ref},layout_a_ref,false}});});
    rejects("MISSING_REFERENCE",[&]{layout_apply({LinkTextLayout{layout_b_ref,{"missing-layout","","text.layout"},true}});});
    rejects("INVALID_TEXT_REF",[&]{layout_apply({LinkTextLayout{layout_b_ref,{"layout-a","p1","text.layout"},true}});});
    rejects("TYPE_MISMATCH",[&]{layout_apply({LinkTextLayout{layout_b_ref,{"layout-a","","text.direction"},true}});});
    rejects("TYPE_MISMATCH",[&]{layout_apply({LinkTextLayout{layout_b_ref,{"layout-path","","text.layout"},true}});});
    rejects("DEPENDENCY_CYCLE",[&]{layout_apply({LinkTextLayout{layout_a_ref,layout_b_ref,false}});});
    rejects("DEPENDENCY_CYCLE",[&]{layout_apply({LinkTextLayout{layout_b_ref,layout_b_ref,true}});});
    rejects("REVISION_CONFLICT",[&]{layout_session.apply({UnlinkTextLayout{layout_b_ref}},layout_revision-1);});
    const auto layout_failure_revision=layout_session.revision();const auto layout_failure_bytes=encode(layout_session.document());
    auto invalid_layout=*layout_session.document().objects.at("layout-a").text;invalid_layout.layout="unknown";
    rejects("UNSUPPORTED_TEXT_LAYOUT",[&]{layout_apply({UpdateText{"layout-a",invalid_layout}});});
    check(layout_session.revision()==layout_failure_revision&&encode(layout_session.document())==layout_failure_bytes,
        "Invalid Text layout enums reject without changing the revision or native bytes");
    const auto layout_link_bytes=encode(layout_session.document());
    check(layout_link_bytes.find("\"layout_driver\":{\"link\":")!=std::string::npos&&
        encode(decode(layout_link_bytes))==layout_link_bytes,
        "Native 0.43 roundtrip preserves the strict Text layout driver");
    const auto reopened_layout=decode(layout_link_bytes);
    check(reopened_layout.objects.at("layout-b").text->layout=="frame"&&
        reopened_layout.objects.at("layout-b").text->layout_driver->link==layout_a_ref&&
        evaluate_text_layout(reopened_layout,"layout-b")=="frame"&&
        reopened_layout.objects.at("layout-b").text->parameters.at("frame_width").literal==96&&
        reopened_layout.objects.at("layout-b").text->parameters.at("frame_height").literal==48,
        "Cold codec reopen retains Text layout literal, driver, evaluated choice and target frame dimensions");
    auto malformed_layout=layout_link_bytes;const auto layout_driver_at=malformed_layout.find("\"layout_driver\":{\"link\":");
    check(layout_driver_at!=std::string::npos,"Native Text layout driver uses one link alternative");
    malformed_layout.replace(layout_driver_at,std::string("\"layout_driver\":{\"link\":").size(),"\"layout_driver\":{\"other\":");
    rejects("INVALID_TEXT_LAYOUT_DRIVER",[&]{decode(malformed_layout);});
    auto wrong_layout_ref=layout_link_bytes;const auto layout_field_at=wrong_layout_ref.find("text.layout",layout_driver_at);
    wrong_layout_ref.replace(layout_field_at,std::string("text.layout").size(),"text.direction");
    rejects("TYPE_MISMATCH",[&]{decode(wrong_layout_ref);});
    auto old_layout_driver=test_support::without_empty_presets_for_legacy_fixture(layout_link_bytes);const auto old_layout_version=old_layout_driver.find("\"version\":\"0.65\"");
    old_layout_driver.replace(old_layout_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.19\"");
    rejects("UNSUPPORTED_TEXT_LAYOUT_DRIVER",[&]{decode(old_layout_driver);});
    auto legacy_layout=test_support::without_empty_presets_for_legacy_fixture(layout_literal_bytes);const auto legacy_layout_version=legacy_layout.find("\"version\":\"0.65\"");
    legacy_layout.replace(legacy_layout_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.20\"");
    check(decode(legacy_layout).objects.at("layout-b").text->layout=="frame",
        "Native 0.19 migrates Text layout literals without creating a driver");
    layout_apply({LinkTextLayout{layout_b_ref,layout_c_ref,true}});
    check(layout_session.document().objects.at("layout-b").text->layout_driver->link==layout_c_ref,
        "Text layout driver replacement succeeds only through its explicit replacement flag");
    layout_apply({UnlinkTextLayout{layout_b_ref}});
    check(layout_session.document().objects.at("layout-b").text->layout=="frame"&&
        !layout_session.document().objects.at("layout-b").text->layout_driver&&
        layout_session.history().states.back().label.find("Unlink Text layout")!=std::string::npos,
        "Unlink freezes the evaluated Text layout mode with a readable History label");
    layout_session.undo(layout_session.revision());
    check(layout_session.document().objects.at("layout-b").text->layout_driver->link==layout_c_ref&&
        evaluate_text_layout(layout_session.document(),"layout-b")=="frame",
        "Undo restores the Text layout link and its evaluated mode");
    layout_session.redo(layout_session.revision());
    auto layout_alternate_edit=*layout_session.document().objects.at("layout-c").text;layout_alternate_edit.layout="auto";
    layout_apply({UpdateText{"layout-c",layout_alternate_edit}});
    check(layout_session.document().objects.at("layout-b").text->layout=="frame"&&
        evaluate_text_layout(layout_session.document(),"layout-b")=="frame",
        "Unlinked Text layout stays frozen after the former source changes");
    Session alignment_session(empty_document("alignment-link-doc","alignment-link-comp","alignment-link-frame"));
    auto alignment_apply=[&](std::vector<Command> commands){alignment_session.apply(commands,alignment_session.revision());};
    auto alignment_source=default_text("alignment-source-text","Alignment link");
    alignment_source.layout="frame";alignment_source.parameters.at("font_size").literal=28;
    alignment_source.parameters.at("frame_width").literal=240;alignment_source.parameters.at("frame_height").literal=80;
    auto alignment_target=alignment_source;alignment_target.id="alignment-target-text";alignment_target.alignment="end";
    auto alignment_start_twin=alignment_target;alignment_start_twin.id="alignment-start-twin-text";alignment_start_twin.alignment="start";
    auto alignment_center_twin=alignment_target;alignment_center_twin.id="alignment-center-twin-text";alignment_center_twin.alignment="center";
    auto alignment_end_source=default_text("alignment-end-source-text","End source");alignment_end_source.alignment="end";
    const Ref alignment_a_ref{"alignment-a","","text.alignment"},alignment_b_ref{"alignment-b","","text.alignment"},
        alignment_c_ref{"alignment-c","","text.alignment"};
    alignment_apply({CreateText{"alignment-link-comp","","alignment-a","Alignment A",alignment_source},
        CreateText{"alignment-link-comp","","alignment-b","Alignment B",alignment_target},
        CreateText{"alignment-link-comp","","alignment-start-twin","Start twin",alignment_start_twin},
        CreateText{"alignment-link-comp","","alignment-center-twin","Center twin",alignment_center_twin},
        CreateText{"alignment-link-comp","","alignment-c","Alignment C",alignment_end_source},
        CreatePrimitive{"alignment-link-comp","","alignment-path","Alignment path",default_primitive("alignment-path-source","nect.shape.circle")}});
    const auto alignment_literal_bytes=encode(alignment_session.document());
    check(alignment_literal_bytes.find("\"version\":\"0.65\"")!=std::string::npos&&
        alignment_literal_bytes.find("\"alignment_driver\"")==std::string::npos,
        "Native 0.43 omits an absent Text alignment driver and retains literal-only enum state");
    const auto alignment_discovered_properties=properties(alignment_session.document());
    check(resolve_name(alignment_session.document(),"Alignment B","","text.alignment")==alignment_b_ref&&
        std::find(alignment_discovered_properties.begin(),alignment_discovered_properties.end(),alignment_b_ref)!=alignment_discovered_properties.end(),
        "Text alignment name resolution and typed discovery retain the stable enum Ref");
    const auto alignment_get=request(alignment_session,R"({"op":"get","ref":{"object":"alignment-b","point":"","field":"text.alignment"}})");
    check(alignment_get.find("\"choices\":[\"start\",\"center\",\"end\"]")!=std::string::npos&&
        alignment_get.find("\"authored\":{\"literal\":\"end\",\"driver\":null}")!=std::string::npos&&
        alignment_get.find("\"evaluated\":\"end\"")!=std::string::npos,
        "Typed Text alignment get exposes authored, evaluated and closed choices");
    const auto alignment_metadata=request(alignment_session,R"({"op":"properties"})");
    check(alignment_metadata.find("\"field\":\"text.alignment\"")!=std::string::npos&&
        alignment_metadata.find("\"evaluated\":\"end\"")!=std::string::npos,
        "Text alignment property discovery reports its evaluated enum choice");
    auto alignment_glyph_left=[&](const Document& document,const Id& object) {
        const auto evaluated=evaluated_text_source(document,object);const auto scalar_values=evaluate(document);
        std::map<std::string,double> parameters;
        for(const auto& [name,scalar]:evaluated.parameters){(void)scalar;parameters[name]=scalar_values.at({object,"","text."+name});}
        const auto layout=evaluate_text(evaluated,parameters);double left=1e300;
        for(const auto& contour:*layout.contours)for(const auto& point:contour.points)left=std::min(left,point.anchor.x);
        return left;
    };
    const auto initial_alignment_left=alignment_glyph_left(alignment_session.document(),"alignment-b");
    const auto alignment_history_before=alignment_session.history().retained_bytes;
    alignment_apply({LinkTextAlignment{alignment_b_ref,alignment_a_ref,false}});
    check(alignment_session.document().objects.at("alignment-b").text->alignment=="end"&&
        alignment_session.document().objects.at("alignment-b").text->parameters.at("frame_width").literal==240&&
        evaluate_text_alignment(alignment_session.document(),"alignment-b")=="start"&&
        alignment_session.document().objects.at("alignment-b").text->alignment_driver->link==alignment_a_ref&&
        alignment_glyph_left(alignment_session.document(),"alignment-b")==alignment_glyph_left(alignment_session.document(),"alignment-start-twin")&&
        alignment_glyph_left(alignment_session.document(),"alignment-b")<initial_alignment_left,
        "Same-field Text alignment link moves fixed-frame glyphs while retaining target literal and frame");
    check(alignment_session.history().retained_bytes>alignment_history_before&&
        alignment_session.history().states.back().label.find("Link Text alignment")!=std::string::npos,
        "Text alignment link records retained driver bytes with a readable History label");
    auto alignment_source_edit=*alignment_session.document().objects.at("alignment-a").text;alignment_source_edit.alignment="center";
    alignment_apply({UpdateText{"alignment-a",alignment_source_edit},Rename{"alignment-a","Renamed Alignment A"},
        ReorderObjects{"alignment-link-comp","",{"alignment-b","alignment-a","alignment-start-twin","alignment-center-twin","alignment-c","alignment-path"}}});
    check(evaluate_text_alignment(alignment_session.document(),"alignment-b")=="center"&&
        alignment_glyph_left(alignment_session.document(),"alignment-b")==alignment_glyph_left(alignment_session.document(),"alignment-center-twin")&&
        alignment_session.document().objects.at("alignment-b").text->alignment_driver->link==alignment_a_ref,
        "Source alignment edits drive fixed-frame layout while rename and reorder preserve the stable Ref");
    const auto alignment_revision=alignment_session.revision();const auto alignment_bytes=encode(alignment_session.document());
    rejects("MISSING_REFERENCE",[&]{alignment_apply({DeleteObjects{{"alignment-a"}}});});
    rejects("DRIVEN_PROPERTY",[&]{alignment_apply({LinkTextAlignment{alignment_b_ref,alignment_c_ref,false}});});
    auto smuggled_alignment=*alignment_session.document().objects.at("alignment-c").text;
    smuggled_alignment.id="alignment-smuggled-text";smuggled_alignment.alignment_driver=TextAlignmentDriver{alignment_a_ref};
    rejects("USE_TYPED_COMMAND",[&]{alignment_apply({CreateText{"alignment-link-comp","","alignment-smuggled","Smuggled alignment",smuggled_alignment}});});
    auto driven_alignment_edit=*alignment_session.document().objects.at("alignment-b").text;driven_alignment_edit.alignment="center";
    rejects("DRIVEN_PROPERTY",[&]{alignment_apply({UpdateText{"alignment-b",driven_alignment_edit}});});
    rejects("MISSING_REFERENCE",[&]{alignment_apply({LinkProperties{{alignment_b_ref},alignment_a_ref,false}});});
    rejects("MISSING_REFERENCE",[&]{alignment_apply({LinkTextAlignment{alignment_b_ref,{"missing-alignment","","text.alignment"},true}});});
    rejects("INVALID_TEXT_REF",[&]{alignment_apply({LinkTextAlignment{alignment_b_ref,{"alignment-a","p1","text.alignment"},true}});});
    rejects("TYPE_MISMATCH",[&]{alignment_apply({LinkTextAlignment{alignment_b_ref,{"alignment-a","","text.direction"},true}});});
    rejects("TYPE_MISMATCH",[&]{alignment_apply({LinkTextAlignment{alignment_b_ref,{"alignment-path","","text.alignment"},true}});});
    rejects("DEPENDENCY_CYCLE",[&]{alignment_apply({LinkTextAlignment{alignment_a_ref,alignment_b_ref,false}});});
    rejects("DEPENDENCY_CYCLE",[&]{alignment_apply({LinkTextAlignment{alignment_b_ref,alignment_b_ref,true}});});
    rejects("REVISION_CONFLICT",[&]{alignment_session.apply({UnlinkTextAlignment{alignment_b_ref}},alignment_revision-1);});
    auto invalid_alignment=*alignment_session.document().objects.at("alignment-a").text;invalid_alignment.alignment="justify";
    rejects("UNSUPPORTED_TEXT_ALIGNMENT",[&]{alignment_apply({UpdateText{"alignment-a",invalid_alignment}});});
    check(alignment_session.revision()==alignment_revision&&encode(alignment_session.document())==alignment_bytes,
        "Invalid, stale, driven, wrong-type and generic Scalar alignment operations reject atomically");
    const auto alignment_link_bytes=encode(alignment_session.document());
    check(alignment_link_bytes.find("\"alignment_driver\":{\"link\":")!=std::string::npos&&
        encode(decode(alignment_link_bytes))==alignment_link_bytes,
        "Native 0.43 codec roundtrip preserves the closed Text alignment driver exactly");
    const auto reopened_alignment=decode(alignment_link_bytes);
    check(reopened_alignment.objects.at("alignment-b").text->alignment=="end"&&
        reopened_alignment.objects.at("alignment-b").text->alignment_driver->link==alignment_a_ref&&
        evaluate_text_alignment(reopened_alignment,"alignment-b")=="center"&&
        reopened_alignment.objects.at("alignment-b").text->parameters.at("frame_width").literal==240,
        "Cold codec reopen retains authored alignment, driver, evaluated choice and target frame");
    auto malformed_alignment=alignment_link_bytes;const auto alignment_driver_at=malformed_alignment.find("\"alignment_driver\":{\"link\":");
    check(alignment_driver_at!=std::string::npos,"Native Text alignment driver uses one link alternative");
    malformed_alignment.replace(alignment_driver_at,std::string("\"alignment_driver\":{\"link\":").size(),"\"alignment_driver\":{\"other\":");
    rejects("INVALID_TEXT_ALIGNMENT_DRIVER",[&]{decode(malformed_alignment);});
    auto wrong_alignment_ref=alignment_link_bytes;const auto alignment_field_at=wrong_alignment_ref.find("text.alignment",alignment_driver_at);
    wrong_alignment_ref.replace(alignment_field_at,std::string("text.alignment").size(),"text.layout");
    rejects("TYPE_MISMATCH",[&]{decode(wrong_alignment_ref);});
    auto old_alignment_driver=test_support::without_empty_presets_for_legacy_fixture(alignment_link_bytes);const auto old_alignment_version=old_alignment_driver.find("\"version\":\"0.65\"");
    old_alignment_driver.replace(old_alignment_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.20\"");
    rejects("UNSUPPORTED_TEXT_ALIGNMENT_DRIVER",[&]{decode(old_alignment_driver);});
    auto legacy_alignment=test_support::without_empty_presets_for_legacy_fixture(alignment_literal_bytes);const auto legacy_alignment_version=legacy_alignment.find("\"version\":\"0.65\"");
    legacy_alignment.replace(legacy_alignment_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.20\"");
    check(decode(legacy_alignment).objects.at("alignment-b").text->alignment=="end"&&
        !decode(legacy_alignment).objects.at("alignment-b").text->alignment_driver,
        "Native 0.23 alignment literals migrate without creating a driver");
    alignment_apply({LinkTextAlignment{alignment_b_ref,alignment_c_ref,true}});
    check(alignment_session.document().objects.at("alignment-b").text->alignment_driver->link==alignment_c_ref,
        "Text alignment driver replacement succeeds only with the explicit replacement flag");
    alignment_apply({UnlinkTextAlignment{alignment_b_ref}});
    check(alignment_session.document().objects.at("alignment-b").text->alignment=="end"&&
        !alignment_session.document().objects.at("alignment-b").text->alignment_driver&&
        alignment_session.history().states.back().label.find("Unlink Text alignment")!=std::string::npos,
        "Unlink freezes evaluated Text alignment with a readable History label");
    alignment_session.undo(alignment_session.revision());
    check(alignment_session.document().objects.at("alignment-b").text->alignment_driver->link==alignment_c_ref&&
        evaluate_text_alignment(alignment_session.document(),"alignment-b")=="end",
        "Undo restores the previous Text alignment link and evaluation");
    alignment_session.redo(alignment_session.revision());
    auto alignment_end_edit=*alignment_session.document().objects.at("alignment-c").text;alignment_end_edit.alignment="center";
    alignment_apply({UpdateText{"alignment-c",alignment_end_edit}});
    check(alignment_session.document().objects.at("alignment-b").text->alignment=="end"&&
        evaluate_text_alignment(alignment_session.document(),"alignment-b")=="end",
        "Unlinked Text alignment remains frozen after the former source changes");

    Session locale_session(empty_document("locale-link-doc","locale-link-comp","locale-link-frame"));
    auto locale_apply=[&](std::vector<Command> commands){locale_session.apply(commands,locale_session.revision());};
    auto locale_a=default_text("locale-a-source","Source");locale_a.locale="ar-SA";
    auto locale_b=default_text("locale-b-source","\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85");locale_b.locale="ja-JP";locale_b.layout="frame";
    locale_b.parameters.at("frame_width").literal=180;locale_b.parameters.at("frame_height").literal=72;
    auto locale_arabic_twin=locale_b;locale_arabic_twin.id="locale-arabic-twin-source";locale_arabic_twin.locale="ar-SA";
    auto locale_japanese_twin=locale_b;locale_japanese_twin.id="locale-japanese-twin-source";locale_japanese_twin.locale="ja-JP";
    auto locale_c=default_text("locale-c-source","Third source");locale_c.locale="fr-FR";
    locale_apply({CreateText{"locale-link-comp","","locale-a","Locale A",locale_a},
        CreateText{"locale-link-comp","","locale-b","Locale B",locale_b},
        CreateText{"locale-link-comp","","locale-arabic-twin","Arabic twin",locale_arabic_twin},
        CreateText{"locale-link-comp","","locale-japanese-twin","Japanese twin",locale_japanese_twin},
        CreateText{"locale-link-comp","","locale-c","Locale C",locale_c},
        CreatePrimitive{"locale-link-comp","","locale-path","Locale path",default_primitive("locale-path-source","nect.shape.circle")}});
    const Ref locale_a_ref{"locale-a","","text.locale"},locale_b_ref{"locale-b","","text.locale"},
        locale_c_ref{"locale-c","","text.locale"};
    const auto locale_literal_bytes=encode(locale_session.document());
    check(locale_literal_bytes.find("\"version\":\"0.65\"")!=std::string::npos&&
        locale_literal_bytes.find("\"locale_driver\"")==std::string::npos,
        "Native 0.43 omits an absent Text locale driver and preserves literal-only Text");
    const auto locale_properties=properties(locale_session.document());
    check(resolve_name(locale_session.document(),"Locale B","","text.locale")==locale_b_ref&&
        std::find(locale_properties.begin(),locale_properties.end(),locale_b_ref)!=locale_properties.end(),
        "Text locale name resolution and typed discovery retain the stable Ref");
    const auto locale_get=request(locale_session,R"({"op":"get","ref":{"object":"locale-b","point":"","field":"text.locale"}})");
    check(locale_get.find("\"authored\":{\"literal\":\"ja-JP\",\"driver\":null}")!=std::string::npos&&
        locale_get.find("\"evaluated\":\"ja-JP\"")!=std::string::npos&&locale_get.find("\"link\":true")!=std::string::npos,
        "Typed Text locale get exposes its authored literal, evaluated value and link capability");
    const auto locale_metadata=request(locale_session,R"({"op":"properties"})");
    check(locale_metadata.find("\"field\":\"text.locale\"")!=std::string::npos&&
        locale_metadata.find("\"evaluated\":\"ja-JP\"")!=std::string::npos,
        "Text locale property discovery reports its evaluated string");
    auto locale_layout=[&](const Document& document,const Id& object) {
        const auto source=evaluated_text_source(document,object);const auto scalar_values=evaluate(document);std::map<std::string,double> parameters;
        for(const auto& [name,value]:source.parameters){(void)value;parameters[name]=scalar_values.at({object,"","text."+name});}
        return evaluate_text(source,parameters);
    };
    const auto same_locale_layout=[](const TextLayout& a,const TextLayout& b) {
        if(a.x!=b.x||a.y!=b.y||a.width!=b.width||a.height!=b.height||a.first_line_baseline_y!=b.first_line_baseline_y||
           a.line_baselines_y!=b.line_baselines_y||a.column_baselines_x!=b.column_baselines_x||a.overflow!=b.overflow||
           a.glyph_count!=b.glyph_count||a.warnings!=b.warnings||a.used_fonts!=b.used_fonts||!a.contours||!b.contours||
           a.contours->size()!=b.contours->size())return false;
        const auto same_vec=[](const Vec2& x,const Vec2& y){return x.x==y.x&&x.y==y.y;};
        for(std::size_t i=0;i<a.contours->size();++i) {
            const auto& left=a.contours->at(i);const auto& right=b.contours->at(i);
            if(left.closed!=right.closed||left.points.size()!=right.points.size())return false;
            for(std::size_t j=0;j<left.points.size();++j) {
                const auto& p=left.points[j];const auto& q=right.points[j];
                if(!same_vec(p.anchor,q.anchor)||!same_vec(p.incoming,q.incoming)||!same_vec(p.outgoing,q.outgoing))return false;
            }
        }
        return true;
    };
    locale_apply({LinkTextLocale{locale_b_ref,locale_a_ref,false}});
    check(locale_session.document().objects.at("locale-b").text->locale=="ja-JP"&&
        evaluate_text_locale(locale_session.document(),"locale-b")=="ar-SA"&&
        locale_session.document().objects.at("locale-b").text->locale_driver->link==locale_a_ref&&
        same_locale_layout(locale_layout(locale_session.document(),"locale-b"),locale_layout(locale_session.document(),"locale-arabic-twin")),
        "Linked Arabic Text locale preserves its target literal and matches an equal literal twin's layout, fonts and warnings");
    auto locale_a_edit=*locale_session.document().objects.at("locale-a").text;locale_a_edit.locale="ja-JP";
    locale_apply({UpdateText{"locale-a",locale_a_edit},Rename{"locale-a","Renamed Locale A"},
        ReorderObjects{"locale-link-comp","",{"locale-b","locale-a","locale-arabic-twin","locale-japanese-twin","locale-c","locale-path"}}});
    check(evaluate_text_locale(locale_session.document(),"locale-b")=="ja-JP"&&
        locale_session.document().objects.at("locale-b").text->locale=="ja-JP"&&
        locale_session.document().objects.at("locale-b").text->locale_driver->link==locale_a_ref&&
        same_locale_layout(locale_layout(locale_session.document(),"locale-b"),locale_layout(locale_session.document(),"locale-japanese-twin")),
        "Source locale edits drive the same Text layout while rename and reorder preserve the stable Ref");
    const auto locale_link_bytes=encode(locale_session.document());
    check(locale_link_bytes.find("\"locale_driver\":{\"link\":")!=std::string::npos&&
        encode(decode(locale_link_bytes))==locale_link_bytes,
        "Native 0.43 codec roundtrip preserves the closed Text locale driver exactly");
    const auto reopened_locale=decode(locale_link_bytes);
    check(reopened_locale.objects.at("locale-b").text->locale=="ja-JP"&&
        reopened_locale.objects.at("locale-b").text->locale_driver->link==locale_a_ref&&
        evaluate_text_locale(reopened_locale,"locale-b")=="ja-JP"&&
        same_locale_layout(locale_layout(reopened_locale,"locale-b"),locale_layout(reopened_locale,"locale-japanese-twin")),
        "Cold codec reopen retains authored locales, stable Ref and evaluated Arabic-capable layout state");
    auto legacy_locale=test_support::without_empty_presets_for_legacy_fixture(locale_literal_bytes);const auto legacy_locale_version=legacy_locale.find("\"version\":\"0.65\"");
    legacy_locale.replace(legacy_locale_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.21\"");
    check(decode(legacy_locale).objects.at("locale-b").text->locale=="ja-JP"&&
        !decode(legacy_locale).objects.at("locale-b").text->locale_driver,
        "Native 0.23 and earlier retain Text locale literals without creating a driver");
    auto false_locale_version=test_support::without_empty_presets_for_legacy_fixture(locale_link_bytes);const auto false_locale_at=false_locale_version.find("\"version\":\"0.65\"");
    false_locale_version.replace(false_locale_at,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.21\"");
    rejects("UNSUPPORTED_TEXT_LOCALE_DRIVER",[&]{decode(false_locale_version);});
    auto malformed_locale=locale_link_bytes;const auto locale_driver_at=malformed_locale.find("\"locale_driver\":{\"link\":");
    check(locale_driver_at!=std::string::npos,"Native Text locale driver uses one link alternative");
    malformed_locale.replace(locale_driver_at,std::string("\"locale_driver\":{\"link\":").size(),"\"locale_driver\":{\"other\":");
    rejects("INVALID_TEXT_LOCALE_DRIVER",[&]{decode(malformed_locale);});
    auto wrong_locale_ref=locale_link_bytes;const auto locale_field_at=wrong_locale_ref.find("text.locale",locale_driver_at);
    wrong_locale_ref.replace(locale_field_at,std::string("text.locale").size(),"text.family");
    rejects("TYPE_MISMATCH",[&]{decode(wrong_locale_ref);});
    const auto locale_revision=locale_session.revision();const auto locale_atomic=encode(locale_session.document());
    rejects("DRIVEN_PROPERTY",[&]{locale_apply({LinkTextLocale{locale_b_ref,locale_c_ref,false}});});
    auto driven_locale_edit=*locale_session.document().objects.at("locale-b").text;driven_locale_edit.locale="fr-FR";
    rejects("DRIVEN_PROPERTY",[&]{locale_apply({UpdateText{"locale-b",driven_locale_edit}});});
    auto smuggled_locale=*locale_session.document().objects.at("locale-c").text;
    smuggled_locale.id="locale-smuggled-source";smuggled_locale.locale_driver=TextLocaleDriver{locale_a_ref};
    rejects("USE_TYPED_COMMAND",[&]{locale_apply({CreateText{"locale-link-comp","","locale-smuggled","Smuggled locale",smuggled_locale}});});
    rejects("MISSING_REFERENCE",[&]{locale_apply({LinkTextLocale{locale_b_ref,{"missing-locale","","text.locale"},true}});});
    rejects("INVALID_TEXT_REF",[&]{locale_apply({LinkTextLocale{locale_b_ref,{"locale-a","p1","text.locale"},true}});});
    rejects("TYPE_MISMATCH",[&]{locale_apply({LinkTextLocale{locale_b_ref,{"locale-a","","text.family"},true}});});
    rejects("TYPE_MISMATCH",[&]{locale_apply({LinkTextLocale{locale_b_ref,{"locale-path","","text.locale"},true}});});
    rejects("DEPENDENCY_CYCLE",[&]{locale_apply({LinkTextLocale{locale_a_ref,locale_b_ref,false}});});
    rejects("DEPENDENCY_CYCLE",[&]{locale_apply({LinkTextLocale{locale_b_ref,locale_b_ref,true}});});
    rejects("MISSING_REFERENCE",[&]{locale_apply({DeleteObjects{{"locale-a"}}});});
    rejects("MISSING_REFERENCE",[&]{locale_apply({LinkProperties{{locale_b_ref},locale_a_ref,false}});});
    auto invalid_locale=*locale_session.document().objects.at("locale-a").text;invalid_locale.locale.clear();
    rejects("LIMIT",[&]{locale_apply({UpdateText{"locale-a",invalid_locale}});});
    invalid_locale=*locale_session.document().objects.at("locale-a").text;invalid_locale.locale=std::string(129,'x');
    rejects("LIMIT",[&]{locale_apply({UpdateText{"locale-a",invalid_locale}});});
    invalid_locale=*locale_session.document().objects.at("locale-a").text;invalid_locale.locale=std::string(1,static_cast<char>(0xff));
    rejects("INVALID_UTF8",[&]{locale_apply({UpdateText{"locale-a",invalid_locale}});});
    rejects("REVISION_CONFLICT",[&]{locale_session.apply({UnlinkTextLocale{locale_b_ref}},locale_revision-1);});
    check(locale_session.revision()==locale_revision&&encode(locale_session.document())==locale_atomic,
        "Invalid locale values, references, cycles, driven edits and stale commands reject atomically");
    locale_apply({LinkTextLocale{locale_b_ref,locale_c_ref,true}});
    check(locale_session.document().objects.at("locale-b").text->locale_driver->link==locale_c_ref&&
        evaluate_text_locale(locale_session.document(),"locale-b")=="fr-FR",
        "Text locale replacement succeeds only with the explicit replacement flag");
    locale_apply({LinkTextLocale{locale_b_ref,locale_a_ref,true}});
    const auto unlink_revision=locale_session.revision();locale_apply({UnlinkTextLocale{locale_b_ref}});
    check(locale_session.document().objects.at("locale-b").text->locale=="ja-JP"&&
        !locale_session.document().objects.at("locale-b").text->locale_driver&&
        locale_session.history().states.back().label.find("Unlink Text locale")!=std::string::npos,
        "Unlink freezes evaluated Text locale and records a readable History label");
    locale_session.undo(locale_session.revision());
    check(locale_session.document().objects.at("locale-b").text->locale_driver->link==locale_a_ref&&
        evaluate_text_locale(locale_session.document(),"locale-b")=="ja-JP",
        "One Undo restores the prior Text locale link and its evaluation");
    locale_session.redo(locale_session.revision());
    locale_a_edit=*locale_session.document().objects.at("locale-a").text;locale_a_edit.locale="fr-FR";
    locale_apply({UpdateText{"locale-a",locale_a_edit}});
    check(locale_session.document().objects.at("locale-b").text->locale=="ja-JP"&&
        evaluate_text_locale(locale_session.document(),"locale-b")=="ja-JP"&&locale_session.revision()==unlink_revision+4,
        "Unlinked Text locale stays frozen when its former source changes");

    auto layout_depth_doc=empty_document("layout-depth-doc","layout-depth-comp","layout-depth-frame");
    Session layout_depth_session(layout_depth_doc);std::vector<Command> layout_depth_creates;
    for(int i=0;i<130;++i)layout_depth_creates.push_back(CreateText{"layout-depth-comp","","layout-depth-"+std::to_string(i),
        "Layout depth "+std::to_string(i),default_text("layout-depth-source-"+std::to_string(i),"Depth "+std::to_string(i))});
    layout_depth_session.apply(layout_depth_creates,layout_depth_session.revision());
    for(int i=0;i<128;++i)layout_depth_session.apply({LinkTextLayout{{"layout-depth-"+std::to_string(i),"","text.layout"},
        {"layout-depth-"+std::to_string(i+1),"","text.layout"},false}},layout_depth_session.revision());
    check(evaluate_text_layout(layout_depth_session.document(),"layout-depth-0")=="auto",
        "Text layout evaluator accepts the documented 128-link dependency depth");
    const auto layout_depth_revision=layout_depth_session.revision();const auto layout_depth_bytes=encode(layout_depth_session.document());
    rejects("DEPENDENCY_DEPTH",[&]{layout_depth_session.apply({LinkTextLayout{{"layout-depth-128","","text.layout"},
        {"layout-depth-129","","text.layout"},false}},layout_depth_revision);});
    check(layout_depth_session.revision()==layout_depth_revision&&encode(layout_depth_session.document())==layout_depth_bytes,
        "Text layout dependency depth overflow rejects atomically");
    Session depth_session(empty_document("depth-doc","depth-comp","depth-frame"));
    std::vector<Command> depth_creates;for(int i=0;i<130;++i) {
        auto text=default_text("depth-source-"+std::to_string(i),"Depth "+std::to_string(i));
        depth_creates.push_back(CreateText{"depth-comp","","depth-"+std::to_string(i),"Depth "+std::to_string(i),std::move(text)});
    }
    depth_session.apply(depth_creates,depth_session.revision());
    for(int i=0;i<128;++i)depth_session.apply({LinkTextContent{{"depth-"+std::to_string(i),"","text.content"},
        {"depth-"+std::to_string(i+1),"","text.content"},false}},depth_session.revision());
    check(evaluate_text_content(depth_session.document(),"depth-0")=="Depth 128",
        "Text content evaluator accepts the documented 128-link depth");
    const auto depth_revision=depth_session.revision();const auto depth_bytes=encode(depth_session.document());
    rejects("DEPENDENCY_DEPTH",[&]{depth_session.apply({LinkTextContent{{"depth-128","","text.content"},
        {"depth-129","","text.content"},false}},depth_revision);});
    check(depth_session.revision()==depth_revision&&encode(depth_session.document())==depth_bytes,
        "Text content dependency depth overflow rejects atomically");

    // The common sampler returns the evaluated cubic itself, not a polyline
    // point/tangent, and keeps authored one-sided knot rules deterministic.
    auto sampler_doc=empty_document("sampler-doc","sampler-comp","sampler-frame");Session sampler_session(sampler_doc);
    Point line_a,line_b;line_a.id="line-a";line_b.id="line-b";line_b.x.literal=200;
    sampler_session.apply({CreatePath{"sampler-comp","","line-path","Line",{{"line-contour",false,{line_a,line_b}}}}},0);
    auto line_sampler=build_path_sampler(sampler_session.document(),"line-path","line-contour",evaluate(sampler_session.document()));
    for(const auto [distance_value,expected_x]:std::array<std::pair<double,double>,3>{{{0,0},{50,50},{100,100}}}) {
        const auto sample=sample_path(line_sampler,distance_value);
        near(sample.position.x,expected_x,1e-10,"Straight path sample x matches exact distance oracle");
        near(sample.position.y,0,1e-10,"Straight path sample y matches exact distance oracle");
        near(sample.tangent.x,1,1e-10,"Straight path tangent is unit-forward");
        near(sample.normal.y,1,1e-10,"Straight path normal is clockwise");
        check(sample.path=="line-path"&&sample.contour=="line-contour","Sampler returns exact stable Path and Contour IDs");
    }
    const auto reversed_line=sample_path(line_sampler,0,true);
    near(reversed_line.position.x,200,1e-10,"Reversed traversal starts at the far endpoint");
    near(reversed_line.tangent.x,-1,1e-10,"Reversed traversal flips only the tangent direction");
    rejects("PATH_SAMPLE_RANGE",[&]{(void)sample_path(line_sampler,201);});
    sampler_session.apply({Set{{"line-path","line-b","x"},0},Set{{"line-path","line-b","y"},200}},sampler_session.revision());
    const auto vertical_line=build_path_sampler(sampler_session.document(),"line-path","line-contour",evaluate(sampler_session.document()));
    const auto vertical_sample=sample_path(vertical_line,50);
    near(vertical_sample.position.x,0,1e-10,"Vertical point edit recomputes world-space sample x");
    near(vertical_sample.position.y,50,1e-10,"Vertical point edit recomputes world-space sample y");
    near(vertical_sample.tangent.y,1,1e-10,"Vertical point edit recomputes analytic tangent");
    sampler_session.undo(sampler_session.revision());
    check(evaluate(sampler_session.document()).at({"line-path","line-b","x"})==200&&
        evaluate(sampler_session.document()).at({"line-path","line-b","y"})==0,"Path point edit has one exact Undo step");

    auto rotated_doc=empty_document("rotated-doc","rotated-comp","rotated-frame");Session rotated(rotated_doc);
    Point rotate_a,rotate_b;rotate_a.id="rotate-a";rotate_b.id="rotate-b";rotate_b.x.literal=200;
    rotated.apply({CreatePath{"rotated-comp","","rotate-path","Rotated",{{"rotate-contour",false,{rotate_a,rotate_b}}}}},0);
    rotated.apply({Set{{"rotate-path","","transform.a"},0},Set{{"rotate-path","","transform.b"},1},
        Set{{"rotate-path","","transform.c"},-1},Set{{"rotate-path","","transform.d"},0}},rotated.revision());
    const auto transformed_sample=sample_path(build_path_sampler(rotated.document(),"rotate-path","rotate-contour",evaluate(rotated.document())),50);
    near(transformed_sample.position.x,0,1e-10,"Source 90-degree transform is applied before sampling");
    near(transformed_sample.position.y,50,1e-10,"Source transform produces the exact vertical oracle");

    auto knot_doc=empty_document("knot-doc","knot-comp","knot-frame");Session knots(knot_doc);
    Point knot_a,knot_b,knot_c;knot_a.id="knot-a";knot_b.id="knot-b";knot_b.x.literal=100;
    knot_c.id="knot-c";knot_c.x.literal=100;knot_c.y.literal=100;
    knots.apply({CreatePath{"knot-comp","","knot-path","Knot",{{"knot-contour",false,{knot_a,knot_b,knot_c}}}}},0);
    const auto knot_sampler=build_path_sampler(knots.document(),"knot-path","knot-contour",evaluate(knots.document()));
    const auto at_knot=sample_path(knot_sampler,100),at_open_end=sample_path(knot_sampler,200);
    near(at_knot.position.x,100,1e-10,"Interior knot samples authored anchor");
    near(at_knot.position.y,0,1e-10,"Interior knot samples authored anchor y");
    near(at_knot.tangent.x,0,1e-10,"Interior knot selects outgoing nonzero tangent x");
    near(at_knot.tangent.y,1,1e-10,"Interior knot selects outgoing nonzero tangent");
    near(at_open_end.tangent.y,1,1e-10,"Open endpoint selects incoming nonzero tangent");

    auto long_doc=empty_document("long-knot-doc","long-knot-comp","long-knot-frame");Session long_path(long_doc);
    Point long_a,long_b,long_c;long_a.id="long-a";long_b.id="long-b";long_b.x.literal=1e9;
    long_c.id="long-c";long_c.x.literal=1e9;long_c.y.literal=1e9;
    long_path.apply({CreatePath{"long-knot-comp","","long-knot-path","Long Knot",
        {{"long-knot-contour",false,{long_a,long_b,long_c}}}}},0);
    const auto long_sampler=build_path_sampler(long_path.document(),"long-knot-path","long-knot-contour",evaluate(long_path.document()));
    const auto near_knot=sample_path(long_sampler,1e9+0.01);
    near(near_knot.position.x,1e9,0.001,"Near-knot sample remains on the long path's second cubic");
    near(near_knot.position.y,0.01,0.001,"Long-path knot snap does not erase a representable 0.01 du96 offset");
    near(near_knot.tangent.y,1,1e-10,"Near-knot sample retains the analytic outgoing segment tangent");

    // Compare curved distance inversion to an independent 200k-step oracle.
    auto curve_doc=empty_document("curve-doc","curve-comp","curve-frame");Session curve_session(curve_doc);
    Point curve_a,curve_b;curve_a.id="curve-a";curve_a.out_angle.literal=90;curve_a.out_length.literal=90;
    curve_b.id="curve-b";curve_b.x.literal=200;curve_b.in_angle.literal=90;curve_b.in_length.literal=90;
    curve_session.apply({CreatePath{"curve-comp","","curve-path","Curve",{{"curve-contour",false,{curve_a,curve_b}}}}},0);
    const auto curve_values=evaluate(curve_session.document());
    const auto curve_sampler=build_path_sampler(curve_session.document(),"curve-path","curve-contour",curve_values);
    auto curve_point=[](double t){const auto u=1-t;return Vec2{600*u*t*t+200*t*t*t,270*u*u*t+270*u*t*t};};
    constexpr std::size_t oracle_steps=200000;const auto target=curve_sampler.length*0.37;
    auto previous=curve_point(0);double accumulated=0,oracle_t=0;Vec2 oracle{};
    for(std::size_t i=1;i<=oracle_steps;++i) {
        const auto t=static_cast<double>(i)/oracle_steps;const auto point=curve_point(t);
        const auto segment=std::hypot(point.x-previous.x,point.y-previous.y);
        if(accumulated+segment>=target) {
            const auto ratio=(target-accumulated)/segment;
            oracle={previous.x+(point.x-previous.x)*ratio,previous.y+(point.y-previous.y)*ratio};
            oracle_t=(static_cast<double>(i-1)+ratio)/oracle_steps;break;
        }
        accumulated+=segment;previous=point;
    }
    const auto curved_sample=sample_path(curve_sampler,target);
    check(std::hypot(curved_sample.position.x-oracle.x,curved_sample.position.y-oracle.y)<=0.1,
        "Curved cubic arc-length inversion agrees with the independent high-resolution oracle within 0.1 du96");
    const auto curved_tangent_norm=std::hypot(curved_sample.tangent.x,curved_sample.tangent.y);
    near(curved_tangent_norm,1,1e-12,"Curved sample returns a normalized analytic derivative");
    const auto oracle_dx=1200*(1-oracle_t)*oracle_t,oracle_dy=270*(1-2*oracle_t);
    const auto oracle_tangent_norm=std::hypot(oracle_dx,oracle_dy);
    near(curved_sample.tangent.x,oracle_dx/oracle_tangent_norm,1e-4,"Curved sample tangent agrees with the analytic cubic derivative oracle");
    near(curved_sample.tangent.y,oracle_dy/oracle_tangent_norm,1e-4,"Curved sample tangent preserves the analytic cubic direction");

    auto square_doc=empty_document("square-doc","square-comp","square-frame");Session square(square_doc);
    Point sq0,sq1,sq2,sq3;sq0.id="sq0";sq1.id="sq1";sq1.x.literal=100;sq2.id="sq2";sq2.x.literal=100;sq2.y.literal=100;
    sq3.id="sq3";sq3.y.literal=100;
    square.apply({CreatePath{"square-comp","","square-path","Square",{{"square-contour",true,{sq0,sq1,sq2,sq3}}}}},0);
    const auto square_sampler=build_path_sampler(square.document(),"square-path","square-contour",evaluate(square.document()));
    const auto square_wrap=sample_path(square_sampler,450);
    near(square_wrap.position.x,50,1e-10,"Closed contour wraps a finite distance across its seam");
    near(square_wrap.position.y,0,1e-10,"Closed contour seam wrap preserves ordered geometry");

    // Retained attachment lifecycle, strict migration and transactional guards.
    auto attach_doc=empty_document("attach-doc","attach-comp","attach-frame");Session attached(attach_doc);
    Point path_start,path_end;path_start.id="attach-p0";path_end.id="attach-p1";path_end.x.literal=320;
    auto attached_source=default_text("attached-source","Path");attached_source.parameters.at("font_size").literal=24;
    attached_source.path_attachment=TextPathAttachment{"attach-path","attach-contour","distance",0,0,false};
    attached.apply({CreatePath{"attach-comp","","attach-path","Source Path",{{"attach-contour",false,{path_start,path_end}}}},
        CreateText{"attach-comp","","attached-text","Attached Text",attached_source}},0);
    const auto attached_native=encode(attached.document());
    check(attached_native.find("\"version\":\"0.65\"")!=std::string::npos&&
        attached_native.find("\"path_attachment\"")!=std::string::npos&&decode(attached_native)==attached.document(),
        "Native 0.43 codec roundtrip retains exact editable Text, Path and Contour attachment data");
    const auto native_path=std::filesystem::temp_directory_path()/
        ("nect-text-path-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".nect");
    struct TempNativeFile {std::filesystem::path path;~TempNativeFile(){std::error_code ignored;std::filesystem::remove(path,ignored);}} cleanup{native_path};
    {
        std::ofstream file(native_path,std::ios::binary|std::ios::trunc);
        check(file.good(),"Create a temporary native Text-on-Path fixture");
        file.write(attached_native.data(),static_cast<std::streamsize>(attached_native.size()));file.flush();
        check(file.good(),"Persist exact native Text-on-Path bytes to a temporary file");
    }
    std::ifstream file(native_path,std::ios::binary);
    check(file.good(),"Open native Text-on-Path fixture for cold file readback");
    const std::string file_bytes((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    Session cold_file(decode(file_bytes));
    check(file_bytes==attached_native&&cold_file.document()==attached.document()&&encode(cold_file.document())==attached_native,
        "Native 0.43 file cold readback preserves exact editable Text, Path and Contour attachment bytes");
    check(attached.document().objects.at("attached-text").text->id=="attached-source"&&
        attached.document().objects.at("attached-text").text->content=="Path"&&
        attached.document().objects.at("attached-text").contours.empty(),
        "Attachment preserves Text identity/content and never authors glyph contours");

    auto legacy_doc=empty_document("legacy23-doc","legacy23-comp","legacy23-frame");Session legacy23(legacy_doc);
    auto legacy23_source=default_text("legacy23-source","Legacy 0.23 Text");
    legacy23.apply({CreateText{"legacy23-comp","","legacy23-text","Legacy",legacy23_source}},0);
    auto native23=test_support::without_empty_presets_for_legacy_fixture(encode(legacy23.document()));const auto version25=native23.find("\"version\":\"0.65\"");
    check(version25!=std::string::npos,"Native writer emits 0.65 before the explicit 0.23 migration fixture");
    native23.replace(version25,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.23\"");
    const auto migrated23=decode(native23);
    check(migrated23.objects.at("legacy23-text").text->id=="legacy23-source"&&
        migrated23.objects.at("legacy23-text").text->content=="Legacy 0.23 Text"&&
        !migrated23.objects.at("legacy23-text").text->path_attachment,
        "Native 0.23 readback migrates to detached Text without changing source ID or content");
    auto illegal23=test_support::without_empty_presets_for_legacy_fixture(attached_native);const auto attached_version=illegal23.find("\"version\":\"0.65\"");
    illegal23.replace(attached_version,std::string("\"version\":\"0.65\"").size(),"\"version\":\"0.23\"");
    rejects("UNSUPPORTED_TEXT_PATH_ATTACHMENT",[&]{(void)decode(illegal23);});

    const auto attached_history=attached.history();
    auto assert_attachment_atomic=[&](const char* label) {
        (void)label;check(attached.revision()==1&&encode(attached.document())==attached_native&&attached.history()==attached_history,
            "Rejected Text-on-Path edit preserves document bytes, revision and Undo history");
    };
    rejects("MISSING_PATH_ATTACHMENT",[&]{attached.apply({DeleteObjects{{"attach-path"}}},attached.revision());});
    assert_attachment_atomic("source deletion");
    auto stale_source=*attached.document().objects.at("attached-text").text;stale_source.path_attachment->contour="stale-contour";
    rejects("MISSING_PATH_CONTOUR",[&]{attached.apply({UpdateText{"attached-text",stale_source}},attached.revision());});
    assert_attachment_atomic("stale contour");
    auto invalid_source=*attached.document().objects.at("attached-text").text;
    invalid_source.path_attachment->path="missing-path";
    rejects("MISSING_PATH_ATTACHMENT",[&]{attached.apply({UpdateText{"attached-text",invalid_source}},attached.revision());});
    assert_attachment_atomic("missing path");
    invalid_source.path_attachment->path="attached-text";
    rejects("INVALID_PATH_ATTACHMENT",[&]{attached.apply({UpdateText{"attached-text",invalid_source}},attached.revision());});
    assert_attachment_atomic("wrong source kind");
    Session generated_source(attached.document());
    generated_source.apply({CreatePrimitive{"attach-comp","","generated-path","Generated",
        default_primitive("generated-source","nect.shape.circle")}},generated_source.revision());
    invalid_source=*generated_source.document().objects.at("attached-text").text;
    invalid_source.path_attachment->path="generated-path";
    rejects("GENERATED_PATH_ATTACHMENT",[&]{generated_source.apply({UpdateText{"attached-text",invalid_source}},generated_source.revision());});
    check(generated_source.document().objects.at("attached-text").text->path_attachment->path=="attach-path",
        "Generated Path rejection retains the authored source reference");
    invalid_source=*attached.document().objects.at("attached-text").text;invalid_source.layout="frame";
    rejects("TEXT_PATH_LAYOUT_UNSUPPORTED",[&]{attached.apply({UpdateText{"attached-text",invalid_source}},attached.revision());});
    assert_attachment_atomic("frame layout");
    invalid_source=*attached.document().objects.at("attached-text").text;invalid_source.direction="vertical";
    rejects("TEXT_PATH_DIRECTION_UNSUPPORTED",[&]{attached.apply({UpdateText{"attached-text",invalid_source}},attached.revision());});
    assert_attachment_atomic("vertical direction");
    invalid_source=*attached.document().objects.at("attached-text").text;invalid_source.content="First\nSecond";
    rejects("TEXT_PATH_MULTILINE_UNSUPPORTED",[&]{attached.apply({UpdateText{"attached-text",invalid_source}},attached.revision());});
    assert_attachment_atomic("hard line break");
#ifdef _WIN32
    invalid_source=*attached.document().objects.at("attached-text").text;
    invalid_source.content="\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85"; // Arabic RTL shaping must not be replaced by LTR glyph placement.
    rejects("TEXT_PATH_RUN_UNSUPPORTED",[&]{attached.apply({UpdateText{"attached-text",invalid_source}},attached.revision());});
    assert_attachment_atomic("RTL shaped run");
#endif
    rejects("PATH_SAMPLE_ZERO_LENGTH",[&]{attached.apply({Set{{"attach-path","attach-p1","x"},0}},attached.revision());});
    assert_attachment_atomic("zero-length source");
    rejects("SINGULAR_TRANSFORM",[&]{attached.apply({Set{{"attached-text","","transform.a"},0}},attached.revision());});
    assert_attachment_atomic("singular Text inverse");
    rejects("REVISION_CONFLICT",[&]{attached.apply({Rename{"attached-text","Stale edit"}},attached.revision()-1);});
    assert_attachment_atomic("stale revision");
    invalid_source=*attached.document().objects.at("attached-text").text;invalid_source.path_attachment->contour="stale-contour";
    rejects("MISSING_PATH_CONTOUR",[&]{attached.apply({Rename{"attach-path","Temporarily renamed"},
        UpdateText{"attached-text",invalid_source}},attached.revision());});
    assert_attachment_atomic("failed multi-command batch");
    const auto cycle_revision=attached.revision();
    rejects("TEXT_PATH_TRANSFORM_CYCLE",[&]{attached.apply({SetTransformParent{"attach-path",std::optional<Id>{"attached-text"},false}},cycle_revision);});
    assert_attachment_atomic("transform cycle");
#ifdef _WIN32
    rejects("TEXT_PATH_OVERFLOW",[&]{attached.apply({Set{{"attach-path","attach-p1","x"},1}},attached.revision());});
    assert_attachment_atomic("path edit overflow");
#endif
    Session delete_both(attached.document());delete_both.apply({DeleteObjects{{"attach-path","attached-text"}}},0);
    check(delete_both.document().objects.empty(),"One atomic batch may delete a source Path and all dependent Text");

    auto cross_doc=empty_document("cross-doc","cross-a","cross-frame-a");
    auto other_composition=empty_document("other-doc","cross-b","cross-frame-b").compositions.front();
    cross_doc.compositions.push_back(other_composition);Session cross_session(cross_doc);
    Point cross_start,cross_end;cross_start.id="cross-p0";cross_end.id="cross-p1";cross_end.x.literal=320;
    auto cross_text=default_text("cross-source","Cross composition");cross_text.path_attachment=TextPathAttachment{"cross-path","cross-contour","distance",0,0,false};
    const auto cross_before=encode(cross_session.document());const auto cross_history=cross_session.history();
    rejects("CROSS_COMPOSITION",[&]{cross_session.apply({
        CreatePath{"cross-a","","cross-path","Cross Path",{{"cross-contour",false,{cross_start,cross_end}}}},
        CreateText{"cross-b","","cross-text","Cross Text",cross_text}},cross_session.revision());});
    check(cross_session.revision()==0&&encode(cross_session.document())==cross_before&&cross_session.history()==cross_history,
        "Cross-Composition attachment rejection preserves document, revision and history");

    Session both_copy_session(attached.document());const DuplicateObjects duplicate_both{{"attach-path","attached-text"},"both-copy"};
    both_copy_session.apply({duplicate_both},both_copy_session.revision());
    const Object* both_copy_text=nullptr;
    for(const auto& [id,object]:both_copy_session.document().objects)if(id!="attached-text"&&object.text&&object.text->path_attachment)both_copy_text=&object;
    check(both_copy_text&&both_copy_text->text->path_attachment->path!="attach-path"&&
        both_copy_session.document().objects.contains(both_copy_text->text->path_attachment->path),
        "Duplicating both Text and Path remaps the copied attachment to the copied Path");
    const auto& copied_contours=both_copy_session.document().objects.at(both_copy_text->text->path_attachment->path).contours;
    check(std::any_of(copied_contours.begin(),copied_contours.end(),[&](const Contour& c){return c.id==both_copy_text->text->path_attachment->contour;}),
        "Duplicating both Text and Path remaps the exact authored Contour ID");
    Session text_only_copy(attached.document());text_only_copy.apply({DuplicateObjects{{"attached-text"},"text-copy"}},0);
    const auto text_copy=std::find_if(text_only_copy.document().objects.begin(),text_only_copy.document().objects.end(),
        [](const auto& entry){return entry.first!="attached-text"&&entry.second.text.has_value();});
    check(text_copy!=text_only_copy.document().objects.end()&&text_copy->second.text->path_attachment->path=="attach-path"&&
        text_copy->second.text->path_attachment->contour=="attach-contour",
        "Duplicating Text alone retains the original Path and Contour IDs");
    Session path_only_copy(attached.document());path_only_copy.apply({DuplicateObjects{{"attach-path"},"path-copy"}},0);
    check(path_only_copy.document().objects.at("attached-text").text->path_attachment->path=="attach-path",
        "Duplicating the Path alone leaves the original Text attached to its original source");

#ifdef _WIN32
    auto attached_values=evaluate(attached.document());
    auto attached_shape=evaluate_shape(attached.document(),"attached-text",attached_values);
    check(attached_shape.paths.front().contours&&!attached_shape.paths.front().contours->empty(),
        "Text-on-Path is a real shared Shape consumer");

    auto mixed_doc=empty_document("mixed-path-doc","mixed-path-comp","mixed-path-frame");Session mixed_session(mixed_doc);
    Point mixed_start,mixed_end;mixed_start.id="mixed-p0";mixed_end.id="mixed-p1";mixed_end.x.literal=10000;
    auto mixed_source=default_text("mixed-source","AV \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");mixed_source.family="Arial";mixed_source.locale="en-US";
    mixed_source.parameters.at("font_size").literal=32;mixed_source.parameters.at("tracking").literal=1.25;
    mixed_source.path_attachment=TextPathAttachment{"mixed-path","mixed-contour","distance",0,0,false};
    mixed_session.apply({CreatePath{"mixed-path-comp","","mixed-path","Mixed Script Path",{{"mixed-contour",false,{mixed_start,mixed_end}}}},
        CreateText{"mixed-path-comp","","mixed-text","Mixed Script",mixed_source}},0);
    auto detached_mixed_source=mixed_source;detached_mixed_source.path_attachment.reset();
    std::map<std::string,double> mixed_parameters;
    for(const auto& [name,parameter]:mixed_source.parameters)mixed_parameters.emplace(name,parameter.literal);
    const auto detached_mixed=evaluate_text(detached_mixed_source,mixed_parameters);
    const auto shaped_mixed=evaluate_text(mixed_source,mixed_parameters);
    const auto projected_mixed=evaluate_text_projection(mixed_session.document(),"mixed-text",evaluate(mixed_session.document()));
    check(shaped_mixed.glyph_count==shaped_mixed.glyphs.size()&&shaped_mixed.glyph_count==detached_mixed.glyph_count&&
        projected_mixed.contours->size()==detached_mixed.contours->size()&&shaped_mixed.used_fonts.size()>=2,
        "Mixed Latin/Japanese fallback retains DirectWrite glyph runs on a straight path");
    const auto baseline=shaped_mixed.glyphs.front().baseline;
    for(std::size_t i=0;i<detached_mixed.contours->size();++i) {
        const auto& detached_contour=detached_mixed.contours->at(i);const auto& attached_contour=projected_mixed.contours->at(i);
        check(detached_contour.points.size()==attached_contour.points.size(),"Path projection retains each shaped glyph contour topology");
        for(std::size_t p=0;p<detached_contour.points.size();++p)for(const auto pair:{
                std::pair{detached_contour.points[p].anchor,attached_contour.points[p].anchor},
                std::pair{detached_contour.points[p].incoming,attached_contour.points[p].incoming},
                std::pair{detached_contour.points[p].outgoing,attached_contour.points[p].outgoing}}) {
            near(pair.second.x,pair.first.x-baseline.x,0.05,"Straight-path projection preserves shaped glyph run x offsets and tracking");
            near(pair.second.y,pair.first.y-baseline.y,0.05,"Straight-path projection preserves shaped glyph run baseline offsets");
        }
    }
#endif
    const auto before_path_edit=attached.document();const auto before_path_edit_bytes=encode(before_path_edit);
    attached.apply({Set{{"attach-path","attach-p1","x"},0},Set{{"attach-path","attach-p1","y"},320}},attached.revision());
    check(attached.revision()==2&&attached.document().objects.at("attached-text").text->content=="Path",
        "Path point edits recompute projection without rewriting authored Text");
    const auto vertical_path_edit=encode(attached.document());
    attached.undo(attached.revision());
    check(attached.revision()==3&&encode(attached.document())==before_path_edit_bytes,
        "Text-on-Path source geometry edit has one exact Undo step");
    attached.redo(attached.revision());
    check(attached.revision()==4&&evaluate(attached.document()).at({"attach-path","attach-p1","y"})==320&&
        encode(attached.document())==vertical_path_edit,
        "Text-on-Path source geometry edit has one exact Redo step");

    auto api_doc=empty_document("api-path-doc","api-path-comp","api-path-frame");Session api_session(api_doc);
    Point api_a,api_b;api_a.id="api-p0";api_b.id="api-p1";api_b.x.literal=320;
    api_session.apply({CreatePath{"api-path-comp","","api-path","API Path",{{"api-contour",false,{api_a,api_b}}}}},0);
    auto api_source=response_result_object(request(api_session,R"({"op":"text_defaults"})"));
    const auto source_id=api_source.find("\"id\":\"new-text-source\"");
    check(source_id!=std::string::npos,"API defaults return the authored Text source shape");
    api_source.replace(source_id+6,std::string("new-text-source").size(),"api-source");
    const auto create_api_text=request(api_session,"{\"op\":\"apply\",\"expected_revision\":1,\"commands\":[{\"type\":\"create_text\",\"composition\":\"api-path-comp\",\"parent\":\"\",\"id\":\"api-text\",\"name\":\"API Text\",\"source\":"+api_source+"}]} ");
    check(create_api_text.find("\"changed\":true")!=std::string::npos&&api_session.revision()==2,
        "JSON-lines API creates the editable Text through the shared Session");
    auto api_attached_source=api_source;const auto source_end=api_attached_source.rfind('}');
    api_attached_source.insert(source_end,R"(,"path_attachment":{"path":"api-path","contour":"api-contour","start_mode":"distance","start":0,"spacing":0,"reversed":false})");
    const auto api_attach_reply=request(api_session,"{\"op\":\"apply\",\"expected_revision\":2,\"commands\":[{\"type\":\"update_text\",\"object\":\"api-text\",\"source\":"+api_attached_source+"}]} ");
    const auto attached_layout_reply=request(api_session,R"({"op":"text_layout","object":"api-text"})");
#ifdef _WIN32
    const bool api_attached_status=attached_layout_reply.find("\"attachment_status\":\"attached\"")!=std::string::npos;
#else
    const bool api_attached_status=attached_layout_reply.find("TEXT_PLATFORM_UNSUPPORTED")!=std::string::npos;
#endif
    check(api_attach_reply.find("\"changed\":true")!=std::string::npos&&api_session.revision()==3&&api_attached_status,
        "JSON-lines API attaches by stable Path/Contour IDs and reads projected layout status");
    const auto api_detach_reply=request(api_session,"{\"op\":\"apply\",\"expected_revision\":3,\"commands\":[{\"type\":\"update_text\",\"object\":\"api-text\",\"source\":"+api_source+"}]} ");
#ifdef _WIN32
    const bool api_detached_status=request(api_session,R"({"op":"text_layout","object":"api-text"})").find("\"attachment_status\":\"detached\"")!=std::string::npos;
#else
    const bool api_detached_status=request(api_session,R"({"op":"text_layout","object":"api-text"})").find("TEXT_PLATFORM_UNSUPPORTED")!=std::string::npos;
#endif
    check(api_detach_reply.find("\"changed\":true")!=std::string::npos&&api_session.revision()==4&&
        api_detached_status&&
        api_session.document().objects.at("api-text").text->content=="Text",
        "JSON-lines API detaches in one Session edit and retains authored Text content");
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
