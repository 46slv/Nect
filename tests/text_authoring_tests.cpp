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
    check(native16.find("\"version\":\"0.16\"")!=std::string::npos&&native16.find("\"italic_driver\":{\"expression\"")!=std::string::npos&&
        encode(decode(native16))==native16,"Native 0.16 roundtrip preserves Text italic expression exactly");
    auto invalid_driver=native16;const auto driver_at=invalid_driver.find("\"italic_driver\":{\"expression\":");
    check(driver_at!=std::string::npos,"Native Text italic driver is serialized as the expression alternative");
    invalid_driver.replace(driver_at,std::string("\"italic_driver\":{\"expression\":").size(),"\"italic_driver\":{\"other\":");
    rejects("INVALID_TEXT_ITALIC_DRIVER",[&]{decode(invalid_driver);});
    auto old_with_driver=native16;const auto current_version=old_with_driver.find("\"version\":\"0.16\"");
    check(current_version!=std::string::npos,"Native bool driver fixture identifies version 0.16");
    old_with_driver.replace(current_version,std::string("\"version\":\"0.16\"").size(),"\"version\":\"0.14\"");
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
    auto native14=encode(legacy_session.document());const auto version_at=native14.find("\"version\":\"0.16\"");
    check(version_at!=std::string::npos,"Native writer emits 0.16");native14.replace(version_at,std::string("\"version\":\"0.16\"").size(),"\"version\":\"0.14\"");
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
        check(get.find("\"type\":\""+std::string(type)+"\"")!=std::string::npos&&
            get.find("\"literal\":\""+values_a[i]+"\"")!=std::string::npos&&
            get.find("\"evaluated\":\""+values_a[i]+"\"")!=std::string::npos&&
            get.find("\"link\":false")!=std::string::npos&&get.find("\"expression\":false")!=std::string::npos,
            "JSON-lines get returns the Text literal, type, and explicit unsupported capabilities");
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
    check(encode(decode(readonly_bytes))==readonly_bytes,"Native 0.16 roundtrip preserves Text source values exactly");
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
    check(weight_link_bytes.find("\"version\":\"0.16\"")!=std::string::npos&&
        weight_link_bytes.find("\"weight_driver\":{\"link\"")!=std::string::npos&&
        encode(decode(weight_link_bytes))==weight_link_bytes,
        "Native 0.16 retains the optional Text weight Ref link exactly");
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
    auto old_weight_driver=weight_link_bytes;const auto weight_version_at=old_weight_driver.find("\"version\":\"0.16\"");
    check(weight_version_at!=std::string::npos,"Native weight fixture identifies version 0.16");
    old_weight_driver.replace(weight_version_at,std::string("\"version\":\"0.16\"").size(),"\"version\":\"0.15\"");
    rejects("UNSUPPORTED_TEXT_WEIGHT_DRIVER",[&]{decode(old_weight_driver);});
    const auto legacy_weight=decode([&]{auto value=encode(weight_session.document());const auto at=value.find("\"version\":\"0.16\"");
        value.replace(at,std::string("\"version\":\"0.16\"").size(),"\"version\":\"0.15\"");return value;}());
    check(legacy_weight.objects.at("weight-a").text->weight==500&&!legacy_weight.objects.at("weight-a").text->weight_driver&&
        legacy_weight.objects.at("weight-b").text->weight==300&&!legacy_weight.objects.at("weight-b").text->weight_driver,
        "Native 0.15 Text migrates authored weights as literals");
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
