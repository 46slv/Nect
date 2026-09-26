#include "nect/io.hpp"
#include <algorithm>
#include <array>
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
    const auto native16=encode(bool_session.document());
    check(native16.find("\"version\":\"0.19\"")!=std::string::npos&&native16.find("\"italic_driver\":{\"expression\"")!=std::string::npos&&
        encode(decode(native16))==native16,"Native 0.19 roundtrip preserves Text italic expression exactly");
    auto invalid_driver=native16;const auto driver_at=invalid_driver.find("\"italic_driver\":{\"expression\":");
    check(driver_at!=std::string::npos,"Native Text italic driver is serialized as the expression alternative");
    invalid_driver.replace(driver_at,std::string("\"italic_driver\":{\"expression\":").size(),"\"italic_driver\":{\"other\":");
    rejects("INVALID_TEXT_ITALIC_DRIVER",[&]{decode(invalid_driver);});
    auto old_with_driver=native16;const auto current_version=old_with_driver.find("\"version\":\"0.19\"");
    check(current_version!=std::string::npos,"Native bool driver fixture identifies version 0.19");
    old_with_driver.replace(current_version,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.14\"");
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
    auto native14=encode(legacy_session.document());const auto version_at=native14.find("\"version\":\"0.19\"");
    check(version_at!=std::string::npos,"Native writer emits 0.19");native14.replace(version_at,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.14\"");
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
        check(count==readonly_fields.size(),"Each Text contributes exactly six read-only source refs");
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
        const auto link=(i<=1||i==4)?"true":"false";
        check(get.find("\"type\":\""+std::string(type)+"\"")!=std::string::npos&&
            get.find("\"literal\":\""+values_a[i]+"\"")!=std::string::npos&&
            get.find("\"evaluated\":\""+values_a[i]+"\"")!=std::string::npos&&
            get.find("\"link\":"+std::string(link))!=std::string::npos&&get.find("\"expression\":false")!=std::string::npos&&
            (i<=1||i==4?get.find("\"driver\":null")!=std::string::npos:true),
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
    check(encode(decode(readonly_bytes))==readonly_bytes,"Native 0.19 roundtrip preserves Text source values exactly");
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
    check(weight_link_bytes.find("\"version\":\"0.19\"")!=std::string::npos&&
        weight_link_bytes.find("\"weight_driver\":{\"link\"")!=std::string::npos&&
        encode(decode(weight_link_bytes))==weight_link_bytes,
        "Native 0.19 retains the optional Text weight Ref link exactly");
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
    rejects("DRIVEN_PROPERTY",[&]{weight_apply({LinkTextWeight{weight_b_ref,weight_a_ref,false}});});
    weight_unchanged(weight_link_bytes,weight_revision,"Replacing an existing weight driver without replace_driver is atomic");
    rejects("MISSING_REFERENCE",[&]{weight_apply({LinkTextWeight{weight_b_ref,{"missing-weight","","text.weight"},true}});});
    weight_unchanged(weight_link_bytes,weight_revision,"Missing weight source rejection preserves authored bytes and revision");
    rejects("TYPE_MISMATCH",[&]{weight_apply({LinkTextWeight{weight_b_ref,{"weight-path","","text.weight"},true}});});
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
    auto old_weight_driver=weight_link_bytes;const auto weight_version_at=old_weight_driver.find("\"version\":\"0.19\"");
    check(weight_version_at!=std::string::npos,"Native weight fixture identifies version 0.19");
    old_weight_driver.replace(weight_version_at,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.15\"");
    rejects("UNSUPPORTED_TEXT_WEIGHT_DRIVER",[&]{decode(old_weight_driver);});
    const auto legacy_weight=decode([&]{auto value=encode(weight_session.document());const auto at=value.find("\"version\":\"0.19\"");
        value.replace(at,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.15\"");return value;}());
    check(legacy_weight.objects.at("weight-a").text->weight==500&&!legacy_weight.objects.at("weight-a").text->weight_driver&&
        legacy_weight.objects.at("weight-b").text->weight==300&&!legacy_weight.objects.at("weight-b").text->weight_driver,
        "Native 0.15 Text migrates authored weights as literals");
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
    check(literal_bytes.find("\"version\":\"0.19\"")!=std::string::npos&&
        literal_bytes.find("content_driver")==std::string::npos&&encode(decode(literal_bytes))==literal_bytes,
        "Native 0.19 omits absent Text drivers and preserves literal-only Text");
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
        "Native 0.19 roundtrip retains the exact Text content driver");
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
    auto old_content_driver=copied_link_bytes;const auto content_version_at=old_content_driver.find("\"version\":\"0.19\"");
    check(content_version_at!=std::string::npos,"Native content fixture identifies version 0.19");
    old_content_driver.replace(content_version_at,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.16\"");
    rejects("UNSUPPORTED_TEXT_CONTENT_DRIVER",[&]{decode(old_content_driver);});
    auto legacy_content=literal_bytes;const auto legacy_version_at=legacy_content.find("\"version\":\"0.19\"");
    legacy_content.replace(legacy_version_at,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.16\"");
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
    check(family_literal_bytes.find("\"version\":\"0.19\"")!=std::string::npos&&
        family_literal_bytes.find("family_driver")==std::string::npos&&encode(decode(family_literal_bytes))==family_literal_bytes,
        "Native 0.19 omits an absent Text family driver and preserves literal-only family values");
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
        "Native 0.19 codec roundtrip retains the strict Text family driver");
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
    auto old_family_driver=copied_family_bytes;const auto family_version_at=old_family_driver.find("\"version\":\"0.19\"");
    old_family_driver.replace(family_version_at,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.17\"");
    rejects("UNSUPPORTED_TEXT_FAMILY_DRIVER",[&]{decode(old_family_driver);});
    auto legacy_family=family_literal_bytes;const auto legacy_family_version=legacy_family.find("\"version\":\"0.19\"");
    legacy_family.replace(legacy_family_version,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.17\"");
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
    check(direction_literal_bytes.find("\"version\":\"0.19\"")!=std::string::npos&&
        direction_literal_bytes.find("direction_driver")==std::string::npos&&encode(decode(direction_literal_bytes))==direction_literal_bytes,
        "Native 0.19 omits an absent Text direction driver and preserves the literal enum");
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
        "Native 0.19 codec roundtrip retains the closed Text direction link");
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
    auto old_direction_driver=copied_direction_bytes;const auto direction_version_at=old_direction_driver.find("\"version\":\"0.19\"");
    old_direction_driver.replace(direction_version_at,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.18\"");
    rejects("UNSUPPORTED_TEXT_DIRECTION_DRIVER",[&]{decode(old_direction_driver);});
    auto legacy_direction=direction_literal_bytes;const auto legacy_direction_version=legacy_direction.find("\"version\":\"0.19\"");
    legacy_direction.replace(legacy_direction_version,std::string("\"version\":\"0.19\"").size(),"\"version\":\"0.18\"");
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
