#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace nect;
namespace {
int checks=0;
void check(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
    ++checks;
}
void near(double actual,double expected,const char* message) {
    check(std::abs(actual-expected)<1e-8,std::string(message)+" (actual="+
        std::to_string(actual)+", expected="+std::to_string(expected)+")");
}
template<class F>void rejects(const char* code,F action) {
    try {action();} catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands) {
    session.apply(commands,session.revision());
}
Document source_document() {
    auto document=empty_document("template-doc","comp","source-art");
    auto& composition=document.compositions.front();
    composition.artboards.front()={"source-art","Source",0,0,1200,900};
    ArtboardLayout layout;
    layout.margin=Margin{10,20,10,20};
    layout.grid=Grid{"source-grid",{20,20,1160,860},2,2,10,10};
    composition.artboards.front().layout=layout;
    composition.artboards.push_back({"target-a","A",400,50,640,480});
    Artboard target_b{"target-b","B",1700,50,640,480};
    ArtboardLayout local_layout;local_layout.margin=Margin{5,6,7,8};target_b.layout=local_layout;
    composition.artboards.push_back(target_b);
    Object root;root.id="source-root";root.name="Logo source root";root.kind=Kind::group;
    root.children={"logo","caption"};root.visible=false;
    root.transform[4]=Scalar{777,{},{}};root.transform[5]=Scalar{555,{},{}};
    Object logo;logo.id="logo";logo.name="Logo";logo.kind=Kind::path;
    logo.source=default_primitive("logo-rect","nect.shape.rectangle");
    logo.source->parameters.at("width").literal=60;
    logo.source->parameters.at("height").literal=40;
    logo.transform[4]=Scalar{5,{},{}};logo.transform[5]=Scalar{6,{},{}};
    auto fill=default_operation("logo-fill","nect.paint.fill");
    fill.parameters.at("r").literal=0.9;fill.parameters.at("g").literal=0.2;
    fill.parameters.at("b").literal=0.1;fill.parameters.at("a").literal=1;
    logo.stack.push_back(fill);
    Object caption;caption.id="caption";caption.name="Caption";caption.kind=Kind::text;
    caption.text=default_text("caption-source","Shared caption");
    document.objects.emplace(root.id,root);document.objects.emplace(logo.id,logo);
    document.objects.emplace(caption.id,caption);composition.roots={root.id};
    return document;
}
const Artboard& board(const Document& document,const Id& id) {
    const auto& boards=document.compositions.front().artboards;
    const auto found=std::find_if(boards.begin(),boards.end(),[&](const Artboard& item){return item.id==id;});
    if(found==boards.end())throw std::runtime_error("Missing Artboard "+id);
    return *found;
}
EvaluatedScene scene(const Document& document) {
    const auto values=evaluate(document);
    return evaluate_scene(document,"comp",values,evaluate_transforms(document,values));
}
Id proxy_for(const EvaluatedScene& evaluated,const Id& instance,const Id& source) {
    for(const auto& [proxy,owner]:evaluated.instance_owners)
        if(owner==instance&&evaluated.instance_sources.at(proxy)==source)return proxy;
    throw std::runtime_error("Missing R04 proxy for "+instance+" / "+source);
}
Ref proxy_fill_channel(const EvaluatedScene& evaluated,const Id& proxy,const std::string& channel) {
    const auto& stack=evaluated.expanded_document->objects.at(proxy).stack;
    const auto fill=std::find_if(stack.begin(),stack.end(),[](const ProcessingEntry& operation) {
        return operation.type=="nect.paint.fill";
    });
    if(fill==stack.end())throw std::runtime_error("Missing projected Fill operation for "+proxy);
    return {proxy,"","op."+fill->id+"."+channel};
}
std::string downgrade_to_075(std::string native) {
    test_support::require_native_current_writer(native);
    test_support::remove_empty_native_076_templates_for_legacy_fixture(native);
    const auto marker=native.find("\"version\":\"0.76\"");
    check(marker!=std::string::npos,"Native fixture has the current 0.76 version marker");
    native.replace(marker,std::string("\"version\":\"0.76\"").size(),"\"version\":\"0.75\"");
    return native;
}
void templates_share_typed_evaluator_and_reset_independently() {
    Session session(source_document());
    apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",ArtboardTemplate{"logo-template","Shared Logo",
            "source-art",std::optional<Id>{"logo-definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-b","logo-template",std::optional<Id>{"content-b"}}}});
    apply(session,{MarginLeftCommand{SetMarginLeftExpression{{"source-art","","margin.left"},
        {R"(ref("target-a","","artboard.width") * 0.05)",1},true}}});

    check(board(session.document(),"target-a").template_assignment&&
        board(session.document(),"target-a").template_assignment->content_instance=="content-a"&&
        board(session.document(),"target-b").template_assignment->margin_overridden,
        "Named Template assignments preserve exact content Instance ownership and B's authored Margin override");
    const auto target_grid=board(session.document(),"target-a").template_assignment->grid_id;
    check(target_grid=="template-grid-target-a"&&target_grid!="source-grid"&&
        !board(session.document(),"target-a").layout&&board(session.document(),"target-b").layout->margin,
        "Absent authored layout inherits a target-local Grid ID while B retains its independent local Margin");

    auto inherited=artboard_layout_property(session.document(),{"target-a","","margin.left"});
    near(std::get<double>(inherited.evaluated),60,"Inherited Margin expression follows target Template width through the common Ref visitor");
    check(inherited.source_kind=="template"&&inherited.template_source==Ref{"source-art","","margin.left"}&&
        std::get<double>(inherited.literal)==10,
        "Typed inherited Margin exposes its source Artboard, authored literal and actual evaluated state");
    auto inherited_grid=artboard_layout_property(session.document(),{target_grid,"","grid.columns"});
    check(inherited_grid.source_kind=="template"&&inherited_grid.template_source==Ref{"source-grid","","grid.columns"}&&
        std::get<std::size_t>(inherited_grid.literal)==2,
        "Typed inherited Grid discovery resolves the source Grid instead of omitting an absent authored Grid");
    const auto property_refs=properties(session.document());
    check(std::find(property_refs.begin(),property_refs.end(),Ref{"target-a","","margin.left"})!=property_refs.end()&&
        std::find(property_refs.begin(),property_refs.end(),Ref{target_grid,"","grid.columns"})!=property_refs.end(),
        "Property discovery includes inherited Margin and target-local Grid properties");
    const auto margin_json=request(session,R"({"op":"get","ref":{"object":"target-a","point":"","field":"margin.left"}})");
    check(margin_json.find("\"source_kind\":\"template\"")!=std::string::npos&&
        margin_json.find("\"template_source\":{\"object\":\"source-art\"")!=std::string::npos&&
        margin_json.find("\"unlink\":false")!=std::string::npos&&
        margin_json.find("\"link\":true")!=std::string::npos,
        "JSON typed discovery reports the inherited source, enables supported local drivers and hides generic Unlink");
    const auto grid_json=request(session,"{\"op\":\"get\",\"ref\":{\"object\":\""+target_grid+
        "\",\"point\":\"\",\"field\":\"grid.columns\"}}");
    check(grid_json.find("\"source_kind\":\"template\"")!=std::string::npos&&
        grid_json.find("\"unlink\":false")!=std::string::npos,
        "JSON Grid discovery preserves inherited state and does not offer a failing generic Unlink: "+grid_json);

    const auto cycle_state=encode(session.document());const auto cycle_history=session.history();
    const auto cycle_revision=session.revision();
    rejects("ARTBOARD_CYCLE",[&]{apply(session,{LinkArtboardSize{{"source-art","","artboard.width"},
        {"target-a","","artboard.width"},false}});});
    check(session.revision()==cycle_revision&&session.history()==cycle_history&&encode(session.document())==cycle_state,
        "The shared visitor rejects only the true Template plus inherited Margin mixed dependency cycle atomically");
    near(evaluate_artboard(session.document().compositions.front(),"target-a").width,1200,
        "The noncyclic Template and Margin path keeps A at the source width");

    Margin local_margin{77,20,10,20};
    apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target-a","layout.margin",
        std::optional<Margin>{local_margin}}}});
    check(board(session.document(),"target-a").template_assignment->margin_overridden&&
        !board(session.document(),"target-a").template_assignment->grid_overridden&&
        artboard_layout_property(session.document(),{"target-a","","margin.left"}).source_kind=="literal"&&
        artboard_layout_property(session.document(),{target_grid,"","grid.columns"}).source_kind=="template",
        "Making Margin local leaves Grid linked and keeps the Template relation explicit");
    const auto local_margin_json=request(session,R"({"op":"get","ref":{"object":"target-a","point":"","field":"margin.left"}})");
    check(local_margin_json.find("\"source_kind\":\"literal\"")!=std::string::npos&&
        local_margin_json.find("\"use_template\":true")!=std::string::npos,
        "Local Margin discovery keeps literal source semantics while advertising the family reset");
    apply(session,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","target-a","layout.margin"}}});
    near(std::get<double>(artboard_layout_property(session.document(),{"target-a","","margin.left"}).evaluated),60,
        "Selective Margin reset restores its inherited live expression");
    check(!board(session.document(),"target-a").template_assignment->margin_overridden&&
        !board(session.document(),"target-a").template_assignment->grid_overridden,
        "Margin reset does not mutate the independent Grid source family");

    auto local_grid=*evaluate_artboard(session.document().compositions.front(),"target-a").layout->grid;
    local_grid.bounds.x=30;
    apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target-a","layout.grid",
        std::optional<Grid>{local_grid}}}});
    check(board(session.document(),"target-a").layout->grid->id==target_grid&&
        board(session.document(),"source-art").layout->grid->id=="source-grid"&&
        board(session.document(),"target-a").template_assignment->grid_overridden,
        "Local Grid uses only its target-local stable ID and never duplicates or overwrites the source Grid");
    const auto local_grid_json=request(session,"{\"op\":\"get\",\"ref\":{\"object\":\""+target_grid+
        "\",\"point\":\"\",\"field\":\"grid.columns\"}}");
    check(local_grid_json.find("\"source_kind\":\"literal\"")!=std::string::npos&&
        local_grid_json.find("\"use_template\":true")!=std::string::npos,
        "Local Grid discovery keeps literal source semantics while advertising the family reset");
    apply(session,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","target-a","layout.grid"}}});
    check(evaluate_artboard(session.document().compositions.front(),"target-a").layout->grid->id==target_grid&&
        !board(session.document(),"target-a").template_assignment->grid_overridden,
        "Selective Grid reset restores the target-local inherited Grid identity");

    const auto authored_board=board(session.document(),"target-a");
    auto renamed=authored_board;renamed.name="A renamed";renamed.x=450;renamed.y=60;
    apply(session,{UpdateArtboard{"comp",renamed},ReorderArtboards{"comp",{"target-b","source-art","target-a"}}});
    check(board(session.document(),"target-a").template_assignment->template_id=="logo-template"&&
        board(session.document(),"target-a").template_assignment->content_instance=="content-a"&&
        board(session.document(),"target-a").x==450&&board(session.document(),"target-a").name=="A renamed",
        "Ordinary Artboard rename, move and reorder keep its exact Template and owned content relation");
    const auto before_self_assign=encode(session.document());const auto before_self_revision=session.revision();
    rejects("TEMPLATE_CYCLE",[&]{apply(session,{ArtboardTemplateCommand{AssignArtboardTemplate{
        "comp","source-art","logo-template",std::nullopt}}});});
    check(session.revision()==before_self_revision&&encode(session.document())==before_self_assign,
        "Template self-source cycles reject without changing authored state");
    rejects("ARTBOARD_TEMPLATE_IN_USE",[&]{apply(session,{ArtboardTemplateCommand{DeleteArtboardTemplate{"comp","logo-template"}}});});
}

void template_definition_root_must_share_the_composition() {
    auto document=source_document();
    Composition other;other.id="other-comp";other.name="Other";
    other.artboards.push_back({"other-art","Other",0,0,800,600});
    Object foreign_root;foreign_root.id="foreign-root";foreign_root.name="Foreign";foreign_root.kind=Kind::path;
    foreign_root.source=default_primitive("foreign-rect","nect.shape.rectangle");
    document.objects.emplace(foreign_root.id,foreign_root);other.roots.push_back(foreign_root.id);
    document.compositions.push_back(other);
    document.definitions.emplace("foreign-definition",Definition{"foreign-definition","Foreign","foreign-root"});
    Session session(std::move(document));
    const auto before=encode(session.document());const auto revision=session.revision();const auto history=session.history();
    rejects("CROSS_COMPOSITION",[&]{apply(session,{ArtboardTemplateCommand{CreateArtboardTemplate{
        "comp",{"foreign-template","Foreign Template","source-art",std::optional<Id>{"foreign-definition"}}}}});});
    check(session.revision()==revision&&session.history()==history&&encode(session.document())==before,
        "Creating an unassigned Template over a Definition owned by another Composition refuses atomically");
    const auto marker=before.find("\"templates\":[]");
    check(marker!=std::string::npos,"The cross-Composition native fixture has an empty source Composition Template list");
    auto malformed=before;
    const std::string template_record="\"templates\":[{\"id\":\"foreign-template\",\"name\":\"Foreign Template\",\"source_artboard\":\"source-art\",\"definition\":\"foreign-definition\"}]";
    malformed.replace(marker,std::string("\"templates\":[]").size(),template_record);
    rejects("CROSS_COMPOSITION",[&]{(void)decode(malformed);});
    check(encode(session.document())==before,
        "Native decode rejects a foreign-root Template even without an assignment or content Instance");
}

void frame_sources_history_and_native_ownership() {
    Session session(source_document());
    apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
            std::optional<Id>{"logo-definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-b","logo-template",std::optional<Id>{"content-b"}}}});
    apply(session,{MarginLeftCommand{SetMarginLeftExpression{{"source-art","","margin.left"},
        {R"(ref("target-a","","artboard.width") * 0.05)",1},true}}});
    const auto before_shrink=encode(session.document());const auto shrink_revision=session.revision();
    rejects("INVALID_LAYOUT",[&]{apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{
        "comp","target-a","frame.width",900.0}}});});
    check(session.revision()==shrink_revision&&encode(session.document())==before_shrink,
        "A local Template frame override cannot shrink the Artboard past its inherited Grid bounds");
    const auto assigned_state=encode(session.document());
    auto source=board(session.document(),"source-art");source.width=1300;source.height=1000;
    apply(session,{UpdateArtboard{"comp",source},ArtboardTemplateCommand{SetArtboardTemplateOverride{
        "comp","target-a","frame.width",1250.0}}});
    check(evaluate_artboard(session.document().compositions.front(),"target-a").width==1250&&
        evaluate_artboard(session.document().compositions.front(),"target-a").height==1000&&
        evaluate_artboard(session.document().compositions.front(),"target-b").width==1300,
        "Independent width override preserves the other Template axis and source Artboard updates");
    auto typed=artboard_size_property(session.document(),{"target-a","","artboard.width"});
    check(typed.source_kind=="template_override"&&typed.template_source==Ref{"source-art","","artboard.width"}&&
        typed.template_override==1250,
        "Typed frame metadata distinguishes a local Template override from its live source");
    const auto frame_json=request(session,R"({"op":"get","ref":{"object":"target-a","point":"","field":"artboard.width"}})");
    check(frame_json.find("\"link\":true")!=std::string::npos&&
        frame_json.find("\"expression\":true")!=std::string::npos&&
        frame_json.find("\"unlink\":false")!=std::string::npos&&
        frame_json.find("\"use_template\":true")!=std::string::npos,
        "Frame source capabilities advertise explicit driver transitions and dedicated Template reset");

    apply(session,{LinkArtboardSize{{"target-a","","artboard.width"},{"source-art","","artboard.width"},true}});
    check(artboard_size_property(session.document(),{"target-a","","artboard.width"}).source_kind=="link"&&
        evaluate_artboard(session.document().compositions.front(),"target-a").width==1300,
        "An explicit Artboard link replaces the selected Template axis without detaching other fields");
    auto typed_json=request(session,R"({"op":"get","ref":{"object":"target-a","point":"","field":"artboard.width"}})");
    check(typed_json.find("\"unlink\":true")!=std::string::npos&&
        typed_json.find("\"use_template\":true")!=std::string::npos,
        "Typed discovery exposes the active independent link while retaining the explicit Template relation");
    apply(session,{UnlinkArtboardSize{{"target-a","","artboard.width"}}});
    typed=artboard_size_property(session.document(),{"target-a","","artboard.width"});
    check(typed.source_kind=="template_override"&&typed.template_override==1300&&typed.literal==1300,
        "Unlink freezes the evaluated independent source over the old Template override instead of reviving 1250");
    apply(session,{SetArtboardSizeExpression{{"target-a","","artboard.width"},
        {R"(ref("source-art","","artboard.width") + 25)",1},true}});
    near(evaluate_artboard(session.document().compositions.front(),"target-a").width,1325,
        "A Template-assigned dimension accepts the existing explicit expression source");
    apply(session,{UnlinkArtboardSize{{"target-a","","artboard.width"}}});
    typed=artboard_size_property(session.document(),{"target-a","","artboard.width"});
    check(typed.source_kind=="template_override"&&typed.template_override==1325&&typed.literal==1325,
        "Expression Unlink also replaces the retained Template literal with the current evaluated value");
    apply(session,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","target-a","frame.width"}}});
    check(evaluate_artboard(session.document().compositions.front(),"target-a").width==1300&&
        evaluate_artboard(session.document().compositions.front(),"target-a").height==1000,
        "Use Template resets only width and restores live source evaluation");

    auto linked_parent=board(session.document(),"target-a");
    linked_parent.parent_size=ArtboardParent{"target-b",true,false};
    apply(session,{UpdateArtboard{"comp",linked_parent}});
    const auto parent_state=encode(session.document());const auto parent_revision=session.revision();
    rejects("DRIVEN_ARTBOARD_SIZE",[&]{apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{
        "comp","target-a","frame.width",800.0}}});});
    check(session.revision()==parent_revision&&encode(session.document())==parent_state,
        "A Template frame literal cannot be accepted while parent_size still controls that axis");
    apply(session,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","target-a","frame.width"}}});
    check(!board(session.document(),"target-a").parent_size&&
        evaluate_artboard(session.document().compositions.front(),"target-a").width==1300,
        "Explicit Use Template clears the selected parent_size source atomically and restores the Template edge");

    apply(session,{MarginLeftCommand{LinkMarginLeft{{"target-a","","margin.left"},
        {"source-art","","artboard.height"},true}}});
    auto local_property=artboard_layout_property(session.document(),{"target-a","","margin.left"});
    check(local_property.source_kind=="link"&&
        board(session.document(),"target-a").template_assignment->margin_overridden,
        "Explicit Margin link promotes just the inherited family into a local Template override");
    const auto local_json=request(session,R"({"op":"get","ref":{"object":"target-a","point":"","field":"margin.left"}})");
    check(local_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        local_json.find("\"unlink\":true")!=std::string::npos&&
        local_json.find("\"use_template\":true")!=std::string::npos,
        "After explicit promotion, typed Margin discovery offers Unlink and the family reset while preserving its live driver");
    apply(session,{MarginLeftCommand{UnlinkMarginLeft{{"target-a","","margin.left"}}}});
    local_property=artboard_layout_property(session.document(),{"target-a","","margin.left"});
    near(std::get<double>(local_property.literal),1000,"Unlink freezes the selected local Margin field");
    source=board(session.document(),"source-art");source.height=1100;source.width=1400;
    apply(session,{UpdateArtboard{"comp",source}});
    near(std::get<double>(artboard_layout_property(session.document(),{"target-a","","margin.left"}).evaluated),1000,
        "A local frozen Margin survives source changes while unrelated Template frame dimensions update");
    apply(session,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","target-a","layout.margin"}}});
    near(std::get<double>(artboard_layout_property(session.document(),{"target-a","","margin.left"}).evaluated),70,
        "Family reset restores the source expression without disturbing inherited Grid or frame sources");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.76\"")!=std::string::npos&&
        native.find("\"templates\":[{\"id\":\"logo-template\"")!=std::string::npos&&
        decode(native)==session.document()&&encode(decode(native))==native,
        "Native 0.76 cold roundtrip retains Template assignment and independent source records canonically");
    const auto before_undo=encode(session.document());
    apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target-a","frame.height",900.0}}});
    const auto overridden=encode(session.document());
    check(session.history().states.size()>2&&session.history().retained_bytes>0,
        "History tracks nested Composition Template and Assignment payloads");
    session.undo(session.revision());check(encode(session.document())==before_undo,
        "Undo restores exact Template, assignment and local source records");
    session.redo(session.revision());check(encode(session.document())==overridden,
        "Redo restores the same canonical Template override payload");

    auto malformed=native;
    const auto source_roots=malformed.find("\"roots\":[\"source-root\",\"content-a\",\"content-b\"]");
    check(source_roots!=std::string::npos,"Native fixture identifies the assigned Composition's ordered top-level roots");
    malformed.replace(source_roots,std::string("\"roots\":[\"source-root\",\"content-a\",\"content-b\"]").size(),
        "\"roots\":[\"source-root\",\"content-b\"]");
    rejects("INVALID_TEMPLATE_CONTENT",[&]{(void)decode(malformed);});
    check(encode(session.document())==overridden,
        "A native file cannot claim an orphan or other-Composition Instance as Template-owned content");

    Session small(source_document(),HistoryLimits{10,200000});
    Session large(source_document(),HistoryLimits{10,200000});
    apply(small,{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"long-template","T","source-art",std::nullopt}}}});
    apply(large,{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"long-template",std::string(3500,'T'),
        "source-art",std::nullopt}}}});
    check(large.history().retained_bytes>small.history().retained_bytes+3000,
        "History memory estimates include nested Template name storage inside Composition snapshots");
    (void)assigned_state;
}

void canonical_ab_logo_fixture_preserves_sources_and_local_overrides() {
    auto document=source_document();
    auto& composition=document.compositions.front();
    auto& source_art=composition.artboards.front();
    source_art.width=960;source_art.height=640;
    source_art.layout=ArtboardLayout{Margin{40,20,40,20},Grid{"source-grid",{40,20,880,600},2,2,20,20}};
    auto& target_a=composition.artboards[1];target_a.x=300;target_a.y=150;target_a.width=960;target_a.height=640;
    target_a.layout.reset();
    auto& target_b=composition.artboards[2];target_b.x=1500;target_b.y=700;target_b.width=960;target_b.height=640;
    target_b.layout=ArtboardLayout{Margin{60,20,40,20},std::nullopt};
    auto add_unrelated_art=[&](const Id& id,double x,double y) {
        Object artwork;artwork.id=id;artwork.name=id;artwork.kind=Kind::path;
        artwork.source=default_primitive(id+"-rect","nect.shape.rectangle");
        artwork.source->parameters.at("width").literal=15;
        artwork.source->parameters.at("height").literal=12;
        artwork.transform[4].literal=x;artwork.transform[5].literal=y;
        document.objects.emplace(id,std::move(artwork));composition.roots.push_back(id);
    };
    add_unrelated_art("unrelated-a",320,180);add_unrelated_art("unrelated-b",1520,720);
    auto& logo=document.objects.at("logo");
    logo.transform[4].literal=10;logo.source->parameters.at("width").literal=40;
    logo.stack.front().parameters.at("r").literal=0.9;
    logo.stack.front().parameters.at("g").literal=0.2;
    logo.stack.front().parameters.at("b").literal=0.1;
    Session session(std::move(document));
    apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
            std::optional<Id>{"logo-definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-b","logo-template",std::optional<Id>{"content-b"}}},
        DefinitionCommand{SetInstanceOverride{"content-a",{"logo","","transform.tx"},90}}});
    check(board(session.document(),"source-art").width==960&&board(session.document(),"source-art").height==640&&
        board(session.document(),"target-a").width==960&&board(session.document(),"target-a").height==640&&
        board(session.document(),"target-b").width==960&&board(session.document(),"target-b").height==640&&
        board(session.document(),"target-a").x==300&&board(session.document(),"target-a").y==150&&
        board(session.document(),"target-b").x==1500&&board(session.document(),"target-b").y==700&&
        board(session.document(),"target-b").template_assignment->margin_overridden,
        "The canonical A/B fixture starts at 960x640 and preserves distinct target positions and B's Margin family");
    near(std::get<double>(artboard_layout_property(session.document(),{"target-a","","margin.left"}).evaluated),40,
        "A inherits source Margin left 40");
    near(std::get<double>(artboard_layout_property(session.document(),{"target-b","","margin.left"}).evaluated),60,
        "B keeps its authored Margin left 60");
    const auto grid_id=board(session.document(),"target-a").template_assignment->grid_id;
    near(std::get<double>(artboard_layout_property(session.document(),{grid_id,"","grid.column_gutter"}).evaluated),20,
        "A inherits source Grid gutter 20");
    auto evaluated=scene(session.document());
    auto a_logo=proxy_for(evaluated,"content-a","logo");
    auto b_logo=proxy_for(evaluated,"content-b","logo");
    near(evaluated.expanded_transforms->at(a_logo).world[4],390,"Only A's Logo X override changes X from 10 to 90");
    near(evaluated.expanded_values->at({a_logo,"","generator.width"}),40,"A initially inherits Logo width 40");
    near(evaluated.expanded_transforms->at(b_logo).world[4],1510,"B initially inherits Logo X 10");
    near(evaluated.expanded_values->at(proxy_fill_channel(evaluated,b_logo,"r")),0.9,"A and B initially inherit source Fill A");

    auto updated_source=board(session.document(),"source-art");
    updated_source.width=1200;
    updated_source.layout->margin->left=50;
    updated_source.layout->grid->column_gutter=30;
    apply(session,{UpdateArtboard{"comp",updated_source},Set{{"logo","","transform.tx"},20},
        Set{{"logo","","generator.width"},60},Set{{"logo","","op.logo-fill.r"},0.05},
        Set{{"logo","","op.logo-fill.g"},0.1},Set{{"logo","","op.logo-fill.b"},0.9}});
    const auto source_values=evaluate(session.document());
    near(source_values.at({"logo","","transform.tx"}),20,"Source Logo X changes from 10 to 20");
    near(source_values.at({"logo","","generator.width"}),60,"Source Logo width changes from 40 to 60");
    near(source_values.at({"logo","","op.logo-fill.b"}),0.9,"Source Fill changes from A to B");
    check(board(session.document(),"source-art").width==1200&&board(session.document(),"source-art").height==640&&
        board(session.document(),"target-a").width==960&&board(session.document(),"target-b").width==960&&
        evaluate_artboard(session.document().compositions.front(),"target-a").width==1200&&
        evaluate_artboard(session.document().compositions.front(),"target-b").width==1200,
        "A and B preserve authored widths 960 while evaluating to the source width 1200");
    near(std::get<double>(artboard_layout_property(session.document(),{"target-a","","margin.left"}).evaluated),50,
        "A follows source Margin left 50");
    near(std::get<double>(artboard_layout_property(session.document(),{"target-b","","margin.left"}).evaluated),60,
        "B preserves its local Margin left 60");
    near(std::get<double>(artboard_layout_property(session.document(),{grid_id,"","grid.column_gutter"}).evaluated),30,
        "A follows source Grid gutter 30");
    evaluated=scene(session.document());
    a_logo=proxy_for(evaluated,"content-a","logo");b_logo=proxy_for(evaluated,"content-b","logo");
    near(evaluated.expanded_transforms->at(a_logo).world[4],390,"A keeps its sole local Logo X override at 90");
    near(evaluated.expanded_values->at({a_logo,"","generator.width"}),60,"A follows source Logo width 60");
    near(evaluated.expanded_values->at(proxy_fill_channel(evaluated,a_logo,"b")),0.9,"A follows source Fill B");
    near(evaluated.expanded_transforms->at(b_logo).world[4],1520,"B follows source Logo X 20");
    near(evaluated.expanded_values->at({b_logo,"","generator.width"}),60,"B follows source Logo width 60");
    near(evaluated.expanded_values->at(proxy_fill_channel(evaluated,b_logo,"b")),0.9,"B follows source Fill B");
    const auto a_bounds=object_bounds(*evaluated.expanded_document,a_logo,*evaluated.expanded_values,*evaluated.expanded_transforms,true);
    const auto b_bounds=object_bounds(*evaluated.expanded_document,b_logo,*evaluated.expanded_values,*evaluated.expanded_transforms,true);
    check(a_bounds.has_value()&&b_bounds.has_value(),"A and B produce fixed projected Logo geometry");
    near(a_bounds->right-a_bounds->left,60,"A projected Logo bounds read back as width 60");
    near(b_bounds->right-b_bounds->left,60,"B projected Logo bounds read back as width 60");
    check(session.document().objects.contains("unrelated-a")&&session.document().objects.contains("unrelated-b")&&
        board(session.document(),"target-a").x==300&&board(session.document(),"target-a").y==150&&
        board(session.document(),"target-b").x==1500&&board(session.document(),"target-b").y==700,
        "Source updates preserve both targets' authored x/y and unrelated artwork");

    apply(session,{DefinitionCommand{ResetInstanceOverride{"content-a",{"logo","","transform.tx"}}},
        ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","target-b","layout.margin"}}});
    evaluated=scene(session.document());a_logo=proxy_for(evaluated,"content-a","logo");b_logo=proxy_for(evaluated,"content-b","logo");
    near(evaluated.expanded_transforms->at(a_logo).world[4],320,"Reset A Logo X restores source X 20");
    near(std::get<double>(artboard_layout_property(session.document(),{"target-b","","margin.left"}).evaluated),50,
        "Reset B Margin restores the current source left 50");
    check(!board(session.document(),"target-b").template_assignment->margin_overridden&&
        !board(session.document(),"target-b").template_assignment->grid_overridden,
        "Reset B's Margin family keeps the independently inherited Grid family linked");
    apply(session,{ArtboardTemplateCommand{DetachArtboardTemplate{"comp","target-a","detached-a"}}});
    const auto detached=session.document();
    const auto detached_root=detached.objects.at("content-a").children.front();
    const auto detached_logo=detached.objects.at(detached_root).children.front();
    const auto detached_values=evaluate(detached);const auto detached_transforms=evaluate_transforms(detached,detached_values);
    const auto detached_bounds=object_bounds(detached,detached_logo,detached_values,detached_transforms,true);
    check(!board(detached,"target-a").template_assignment&&detached_bounds.has_value()&&
        detached.objects.contains("unrelated-a")&&detached.objects.contains("unrelated-b"),
        "Canonical A detach materializes its content while preserving unrelated artwork");
    near(detached_bounds->right-detached_bounds->left,60,"Detached A preserves the current source Logo width 60");
    check(board(detached,"target-b").template_assignment&&board(detached,"target-b").template_assignment->template_id=="logo-template",
        "Detaching A leaves B assigned to the shared Template");
    updated_source=board(detached,"source-art");updated_source.width=1300;
    updated_source.layout->margin->left=70;updated_source.layout->grid->column_gutter=40;
    apply(session,{UpdateArtboard{"comp",updated_source},Set{{"logo","","transform.tx"},25},
        Set{{"logo","","generator.width"},70},Set{{"logo","","op.logo-fill.r"},0.9},
        Set{{"logo","","op.logo-fill.g"},0.2},Set{{"logo","","op.logo-fill.b"},0.1}});
    const auto after_source_change=session.document();
    const auto detached_scene=scene(after_source_change);
    const auto a_group=after_source_change.objects.at("content-a").children.front();
    const auto a_materialized_logo=after_source_change.objects.at(a_group).children.front();
    const auto after_values=evaluate(after_source_change);const auto after_transforms=evaluate_transforms(after_source_change,after_values);
    const auto after_a_bounds=object_bounds(after_source_change,a_materialized_logo,after_values,after_transforms,true);
    const auto after_b_logo=proxy_for(detached_scene,"content-b","logo");
    check(board(after_source_change,"target-a").width==1200&&board(after_source_change,"target-b").width==960&&
        evaluate_artboard(after_source_change.compositions.front(),"target-b").width==1300&&after_a_bounds.has_value(),
        "Detached A keeps its frame and B preserves authored width while evaluating to source width 1300");
    near(after_a_bounds->right-after_a_bounds->left,60,"Detached A geometry remains at width 60 after source changes");
    near(after_transforms.at(a_materialized_logo).world[4],320,"Detached A keeps its reset Logo X after source changes");
    const auto& detached_stack=after_source_change.objects.at(a_materialized_logo).stack;
    const auto detached_fill=std::find_if(detached_stack.begin(),detached_stack.end(),[](const ProcessingEntry& operation) {
        return operation.type=="nect.paint.fill";
    });
    check(detached_fill!=detached_stack.end(),"Detached A retains its materialized Fill operation");
    near(after_values.at({a_materialized_logo,"","op."+detached_fill->id+".b"}),0.9,
        "Detached A keeps source Fill B after source changes");
    near(detached_scene.expanded_transforms->at(after_b_logo).world[4],1525,"Assigned B follows the later source Logo X 25");
    near(detached_scene.expanded_values->at({after_b_logo,"","generator.width"}),70,"Assigned B follows the later source width 70");
    near(detached_scene.expanded_values->at(proxy_fill_channel(detached_scene,after_b_logo,"r")),0.9,
        "Assigned B follows later source Fill A");
    near(std::get<double>(artboard_layout_property(after_source_change,{board(after_source_change,"target-b").template_assignment->grid_id,"","grid.column_gutter"}).evaluated),40,
        "Assigned B follows the later source Grid gutter 40");
}

void r04_descendant_overrides_project_geometry_and_guard_legacy_versions() {
    Session session(source_document());
    apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
            std::optional<Id>{"logo-definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-b","logo-template",std::optional<Id>{"content-b"}}},
        DefinitionCommand{SetInstanceOverride{"content-a",{"logo","","transform.tx"},90}},
        DefinitionCommand{SetInstanceOverride{"content-a",{"logo","","generator.width"},30}}});
    const auto source_values=evaluate(session.document());
    check(source_values.at({"logo","","transform.tx"})==5&&
        source_values.at({"logo","","generator.width"})==60,
        "Descendant Template overrides never rewrite source transform or Rectangle geometry");
    auto projected=scene(session.document());
    auto proxy_logo=proxy_for(projected,"content-a","logo");
    near(projected.expanded_values->at({proxy_logo,"","generator.width"}),30,
        "The existing R04 override map projects a descendant Rectangle width override");
    near(projected.expanded_transforms->at(proxy_logo).world[4],490,
        "A descendant transform.tx override composes with the target occurrence position while source-root affine is ignored");
    const auto bounds=object_bounds(*projected.expanded_document,proxy_logo,*projected.expanded_values,
        *projected.expanded_transforms,true);
    check(bounds.has_value(),"Projected descendant Logo has concrete geometry");
    near(bounds->right-bounds->left,30,"Projected world bounds use the local Rectangle width override");
    check(projected.expanded_document->objects.at(proxy_for(projected,"content-a","source-root")).visible,
        "Source-root visibility remains ordinary R04 occurrence state and is not inherited by the Template Instance");

    apply(session,{Set{{"logo","","generator.width"},80},Set{{"logo","","transform.tx"},20},
        Set{{"logo","","op.logo-fill.r"},0.4}});
    projected=scene(session.document());proxy_logo=proxy_for(projected,"content-a","logo");
    const auto proxy_b=proxy_for(projected,"content-b","logo");
    near(projected.expanded_values->at({proxy_logo,"","generator.width"}),30,
        "Source geometry updates preserve the selected local Rectangle override");
    near(projected.expanded_values->at({proxy_b,"","generator.width"}),80,
        "The unoverridden target follows live source Rectangle geometry");
    near(projected.expanded_values->at(proxy_fill_channel(projected,proxy_logo,"r")),0.4,
        "Source paint changes continue flowing through a geometry override");
    near(projected.expanded_transforms->at(proxy_logo).world[4],490,
        "Source descendant transform changes do not replace an explicit Instance transform.tx override");
    const auto updated_bounds=object_bounds(*projected.expanded_document,proxy_logo,*projected.expanded_values,
        *projected.expanded_transforms,true);
    check(updated_bounds.has_value(),"Updated projected Logo retains rendered geometry");
    near(updated_bounds->right-updated_bounds->left,30,"The live override keeps projected width at 30 after source updates");

    const auto native=encode(session.document());
    auto reopened=decode(native);projected=scene(reopened);
    proxy_logo=proxy_for(projected,"content-a","logo");
    const auto reopened_bounds=object_bounds(*projected.expanded_document,proxy_logo,*projected.expanded_values,
        *projected.expanded_transforms,true);
    check(reopened==session.document()&&reopened_bounds.has_value(),
        "Native 0.76 cold reopen preserves exact Template ownership and projected geometry");
    near(reopened_bounds->right-reopened_bounds->left,30,"Cold reopened Template projection retains width 30");

    apply(session,{DefinitionCommand{ResetInstanceOverride{"content-a",{"logo","","generator.width"}}}});
    projected=scene(session.document());proxy_logo=proxy_for(projected,"content-a","logo");
    const auto reset_bounds=object_bounds(*projected.expanded_document,proxy_logo,*projected.expanded_values,
        *projected.expanded_transforms,true);
    near(projected.expanded_values->at({proxy_logo,"","generator.width"}),80,
        "Resetting the descendant width override restores live source width 80");
    check(reset_bounds.has_value(),"Reset projection retains concrete geometry");
    near(reset_bounds->right-reset_bounds->left,80,"Reset updates the projected world bounds to width 80");
    apply(session,{DefinitionCommand{SetInstanceOverride{"content-a",{"logo","","generator.width"},30}}});
    apply(session,{ArtboardTemplateCommand{DetachArtboardTemplate{"comp","target-a","detached-a"}}});
    const auto detached=session.document();
    check(!board(detached,"target-a").template_assignment&&
        detached.objects.at("content-a").kind==Kind::group&&!detached.objects.at("content-a").instance,
        "Template detach materializes its owned R04 Instance and clears the relation");
    const auto detached_root=detached.objects.at("content-a").children.front();
    const auto detached_logo=detached.objects.at(detached_root).children.front();
    auto detached_values=evaluate(detached);auto detached_transforms=evaluate_transforms(detached,detached_values);
    const auto detached_bounds=object_bounds(detached,detached_logo,detached_values,detached_transforms,true);
    check(detached_bounds.has_value(),"Detached Logo has independent concrete geometry");
    near(detached_bounds->right-detached_bounds->left,30,
        "Detaching while overridden preserves projected Rectangle width 30");

    for(const auto& field:std::vector<std::pair<std::string,double>>{
            {"transform.tx",23},{"transform.ty",24},{"generator.width",25},{"generator.height",26}}) {
        Session bare(source_document());
        apply(bare,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
            DefinitionCommand{CreateInstance{"comp","","bare-instance","logo-definition","Bare"}},
            DefinitionCommand{SetInstanceOverride{"bare-instance",{"logo","",field.first},field.second}}});
        const auto legacy=downgrade_to_075(encode(bare.document()));
        rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(legacy);});
    }
    Session compatible(source_document());
    apply(compatible,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        DefinitionCommand{CreateInstance{"comp","","bare-instance","logo-definition","Bare"}},
        DefinitionCommand{SetInstanceOverride{"bare-instance",{"logo","","composite.opacity"},0.4}},
        DefinitionCommand{SetInstanceOverride{"bare-instance",{"caption","","text.font_size"},34}}});
    const auto legacy_compatible=downgrade_to_075(encode(compatible.document()));
    const auto compatible_reopen=decode(legacy_compatible);
    check(compatible_reopen.objects.at("bare-instance").instance->overrides.size()==2,
        "Native 0.75 continues to accept legacy root opacity and descendant Text font-size overrides");
}

void template_id_collisions_refuse_atomically() {
    auto document=source_document();
    document.compositions.front().guides.push_back({"template-grid-target-c","Collision","x",5});
    document.compositions.front().artboards.push_back({"target-c","C",2600,50,640,480});
    Session session(std::move(document));
    apply(session,{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",std::nullopt}}}});
    const auto before=encode(session.document());const auto revision=session.revision();const auto history=session.history();
    rejects("DUPLICATE_ID",[&]{apply(session,{ArtboardTemplateCommand{AssignArtboardTemplate{
        "comp","target-c","logo-template",std::nullopt}}});});
    check(session.revision()==revision&&session.history()==history&&encode(session.document())==before,
        "A target-local Grid ID collision refuses assignment atomically instead of reusing a source or Guide ID");
}

void legacy_artboard_edits_keep_template_sources_and_owned_content() {
    {
        auto document=source_document();
        auto& composition=document.compositions.front();
        auto target_b=std::find_if(composition.artboards.begin(),composition.artboards.end(),
            [](const Artboard& item){return item.id=="target-b";});
        target_b->width=1300;
        Session session(std::move(document));
        apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
            ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
                std::optional<Id>{"logo-definition"}}}},
            ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}}});
        auto edited=board(session.document(),"target-a");edited.width=1250;
        apply(session,{UpdateArtboard{"comp",edited}});
        check(board(session.document(),"target-a").template_assignment->width_override==1250&&
            !board(session.document(),"target-a").template_assignment->height_override&&
            board(session.document(),"target-a").template_assignment->template_id=="logo-template"&&
            board(session.document(),"target-a").template_assignment->content_instance=="content-a"&&
            session.document().objects.at("content-a").kind==Kind::instance&&
            session.document().objects.at("content-a").instance->definition=="logo-definition"&&
            evaluate_artboard(session.document().compositions.front(),"target-a").height==900,
            "UpdateArtboard width edits promote only that axis and preserve Template relation, height inheritance, and owned Definition Instance");
        auto source=board(session.document(),"source-art");source.width=1400;source.height=1000;
        apply(session,{UpdateArtboard{"comp",source}});
        check(evaluate_artboard(session.document().compositions.front(),"target-a").width==1250&&
            evaluate_artboard(session.document().compositions.front(),"target-a").height==1000&&
            board(session.document(),"target-a").template_assignment->content_instance=="content-a",
            "The authored width override remains fixed while the other Template axis and content relation continue live");
    }
    {
        auto document=source_document();
        auto& composition=document.compositions.front();
        auto target_b=std::find_if(composition.artboards.begin(),composition.artboards.end(),
            [](const Artboard& item){return item.id=="target-b";});
        target_b->width=1300;
        Session session(std::move(document));
        apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
            ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
                std::optional<Id>{"logo-definition"}}}},
            ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}},
            LinkArtboardSize{{"target-a","","artboard.width"},{"target-b","","artboard.width"},false}});
        const auto& linked=board(session.document(),"target-a");
        check(linked.template_assignment&&linked.template_assignment->template_id=="logo-template"&&
            linked.template_assignment->content_instance=="content-a"&&linked.width_driver&&
            std::get_if<Ref>(&linked.width_driver->value)&&
            *std::get_if<Ref>(&linked.width_driver->value)==Ref{"target-b","","artboard.width"}&&
            evaluate_artboard(session.document().compositions.front(),"target-a").width==1300,
            "An explicit Artboard size source on inherited state becomes a local driver while retaining the Template and content Instance");
        auto edit=linked;edit.width=edit.width+10;
        const auto before=encode(session.document());const auto revision=session.revision();const auto history=session.history();
        rejects("DRIVEN_ARTBOARD_SIZE",[&]{apply(session,{UpdateArtboard{"comp",edit}});});
        check(encode(session.document())==before&&session.revision()==revision&&session.history()==history&&
            board(session.document(),"target-a").template_assignment->content_instance=="content-a",
            "A legacy numeric edit refuses an independently driven frame axis without mutating native, revision, History, or Template ownership");
    }
    {
        auto document=source_document();
        auto target_b=std::find_if(document.compositions.front().artboards.begin(),document.compositions.front().artboards.end(),
            [](const Artboard& item){return item.id=="target-b";});
        target_b->width=20;
        Session session(std::move(document));
        apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
            ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
                std::optional<Id>{"logo-definition"}}}},
            ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}}});
        apply(session,{MarginLeftCommand{LinkMarginLeft{
            {"target-a","","margin.left"},{"target-b","","artboard.width"},false}}});
        const auto& margin_target=board(session.document(),"target-a");
        check(margin_target.template_assignment->margin_overridden&&margin_target.layout&&margin_target.layout->margin&&
            margin_target.layout->margin->left_driver==std::optional<Ref>{Ref{"target-b","","artboard.width"}}&&
            evaluate_artboard(session.document().compositions.front(),"target-a").layout->margin->left==20&&
            !margin_target.template_assignment->grid_overridden&&!margin_target.layout->grid&&
            evaluate_artboard(session.document().compositions.front(),"target-a").layout->grid,
            "A Margin source command safely materializes only its inherited family while retaining independent Grid inheritance");
        apply(session,{GridBoundsXCommand{LinkGridBoundsX{
            {"template-grid-target-a","","grid.bounds.x"},{"target-b","","artboard.width"},false}}});
        const auto& grid_target=board(session.document(),"target-a");
        check(grid_target.template_assignment->grid_overridden&&grid_target.layout&&grid_target.layout->grid&&
            grid_target.layout->grid->id==grid_target.template_assignment->grid_id&&
            grid_target.layout->grid->bounds_x_driver==std::optional<Ref>{Ref{"target-b","","artboard.width"}}&&
            evaluate_artboard(session.document().compositions.front(),"target-a").layout->grid->bounds.x==20&&
            grid_target.template_assignment->margin_overridden,
            "A Grid source command safely materializes only its inherited family with a target-local Grid ID and keeps Margin local");
    }
    {
        Session session(source_document());
        apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
            ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
                std::optional<Id>{"logo-definition"}}}},
            ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}},
            SetArtboardLayout{"comp","target-a",ArtboardLayout{Margin{5,6,7,8},std::nullopt}}});
        const auto& target=board(session.document(),"target-a");
        check(target.template_assignment&&target.template_assignment->template_id=="logo-template"&&
            target.template_assignment->content_instance=="content-a"&&
            target.template_assignment->margin_overridden&&target.template_assignment->grid_overridden&&
            target.layout&&target.layout->margin==std::optional<Margin>{Margin{5,6,7,8}}&&!target.layout->grid&&
            evaluate_artboard(session.document().compositions.front(),"target-a").layout->margin->left==5&&
            !evaluate_artboard(session.document().compositions.front(),"target-a").layout->grid,
            "SetArtboardLayout retains Template/content ownership and records a local Margin plus explicit absent Grid family");
    }
}

template<class F> void rejects_atomically(Session& session,const char* code,F action,const char* message) {
    const auto native=encode(session.document());const auto revision=session.revision();const auto history=session.history();
    rejects(code,std::forward<F>(action));
    check(session.revision()==revision&&session.history()==history&&encode(session.document())==native,message);
}

void template_reference_and_lifecycle_failures_are_atomic() {
    auto document=source_document();
    document.compositions.push_back(Composition{"other-comp","Other",{},{{"other-art","Other",0,0,320,240}}});
    Session session(std::move(document));
    rejects_atomically(session,"MISSING_COMPOSITION",[&]{apply(session,{ArtboardTemplateCommand{CreateArtboardTemplate{
        "missing-comp",{"missing-composition-template","Bad","source-art",std::nullopt}}}});},
        "A missing Composition ID cannot partially author a Template");
    rejects_atomically(session,"MISSING_ARTBOARD",[&]{apply(session,{ArtboardTemplateCommand{CreateArtboardTemplate{
        "comp",{"missing-source-template","Bad","missing-art",std::nullopt}}}});},
        "A missing source Artboard ID cannot partially author a Template");
    rejects_atomically(session,"MISSING_DEFINITION",[&]{apply(session,{ArtboardTemplateCommand{CreateArtboardTemplate{
        "comp",{"missing-definition-template","Bad","source-art",std::optional<Id>{"missing-definition"}}}}});},
        "A missing Definition ID cannot partially author a Template");
    apply(session,{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",std::nullopt}}}});
    rejects_atomically(session,"MISSING_ARTBOARD_TEMPLATE",[&]{apply(session,{ArtboardTemplateCommand{AssignArtboardTemplate{
        "other-comp","other-art","logo-template",std::nullopt}}});},
        "A Template ID from a different Composition does not resolve by matching human name");
    rejects_atomically(session,"MISSING_ARTBOARD",[&]{apply(session,{ArtboardTemplateCommand{AssignArtboardTemplate{
        "comp","missing-art","logo-template",std::nullopt}}});},
        "A missing target Artboard ID cannot be assigned");
    apply(session,{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::nullopt}}});
    rejects_atomically(session,"TYPE_MISMATCH",[&]{apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{
        "comp","target-a","frame.width",std::optional<Margin>{Margin{1,2,3,4}}}}});},
        "A Margin payload cannot be used for a numeric frame override");
    rejects_atomically(session,"UNSUPPORTED_TEMPLATE_OVERRIDE",[&]{apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{
        "comp","target-a","object.opacity",4.0}}});},
        "Template override fields stay in the closed frame/layout domain");
    rejects_atomically(session,"MISSING_COMPOSITION",[&]{apply(session,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{
        "missing-comp","target-a","frame.width"}}});},
        "A missing Composition cannot reset an override");
    rejects_atomically(session,"MISSING_ARTBOARD_TEMPLATE_ASSIGNMENT",[&]{apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{
        "comp","target-b","frame.width",300.0}}});},
        "A target without an assignment cannot accept an override");
    rejects_atomically(session,"ARTBOARD_TEMPLATE_IN_USE",[&]{apply(session,{ArtboardTemplateCommand{DeleteArtboardTemplate{
        "comp","logo-template"}}});},
        "An assigned Template cannot be deleted while its target remains assigned");
    rejects_atomically(session,"ARTBOARD_IN_USE",[&]{apply(session,{DeleteArtboard{"comp","source-art"}});},
        "A Template source Artboard cannot be deleted while the Template refers to it");

    const auto before_native=encode(session.document());const auto revision=session.revision();const auto history=session.history();
    const auto stale=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"set_artboard_template_override","composition":"comp","artboard":"target-a","field":"frame.width","value":700}]})");
    check(stale.find("REVISION_CONFLICT")!=std::string::npos&&session.revision()==revision&&
        session.history()==history&&encode(session.document())==before_native,
        "A stale JSON Session command preserves native bytes, revision and History");
    const auto revision_json=std::to_string(revision);
    const auto later_failure=request(session,std::string(R"({"op":"apply","expected_revision":)")+revision_json+
        R"(,"commands":[{"type":"set_artboard_template_override","composition":"comp","artboard":"target-a","field":"frame.width","value":1180},{"type":"delete_artboard_template","composition":"comp","template":"logo-template"}]})");
    check(later_failure.find("ARTBOARD_TEMPLATE_IN_USE")!=std::string::npos&&session.revision()==revision&&
        session.history()==history&&encode(session.document())==before_native,
        "A later failed JSON batch command rolls back an earlier valid Template override exactly");
}

void mixed_template_cycles_and_inactive_override_edges() {
    auto assigned=[] {
        Session session(source_document());
        apply(session,{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",std::nullopt}}},
            ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::nullopt}}});
        return session;
    };
    {
        auto session=assigned();
        rejects_atomically(session,"ARTBOARD_CYCLE",[&]{apply(session,{LinkArtboardSize{{"source-art","","artboard.width"},
            {"target-a","","artboard.width"},true}});},
            "A Template edge plus reverse explicit Link cycle is rejected atomically");
    }
    {
        auto session=assigned();auto source=board(session.document(),"source-art");
        source.parent_size=ArtboardParent{"target-a",true,false};
        rejects_atomically(session,"ARTBOARD_CYCLE",[&]{apply(session,{UpdateArtboard{"comp",source}});},
            "A Template edge plus reverse parent_size cycle is rejected atomically");
    }
    {
        auto session=assigned();
        rejects_atomically(session,"ARTBOARD_CYCLE",[&]{apply(session,{SetArtboardSizeExpression{{"source-art","","artboard.width"},
            {R"(ref("target-a","","artboard.width"))",1},true}});},
            "A Template edge plus reverse size expression cycle is rejected atomically");
    }
    {
        auto session=assigned();
        apply(session,{ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target-a","frame.width",1190.0}},
            SetArtboardSizeExpression{{"source-art","","artboard.width"},{R"(ref("target-a","","artboard.width"))",1},true}});
        near(evaluate_artboard(session.document().compositions.front(),"source-art").width,1190,
            "A local override makes its Template dependency edge inactive while a source expression reads the target");
        const auto before=encode(session.document());const auto revision=session.revision();const auto history=session.history();
        rejects("ARTBOARD_CYCLE",[&]{apply(session,{ArtboardTemplateCommand{ResetArtboardTemplateOverride{
            "comp","target-a","frame.width"}}});});
        check(encode(session.document())==before&&session.revision()==revision&&session.history()==history,
            "Restoring the inactive Template edge rechecks and rejects the real mixed cycle");
    }
}

void template_source_items_keep_stable_identity_through_delete_and_ordinary_detach() {
    auto document=source_document();
    Object collision;collision.id="detach-1";collision.name="Existing collision";collision.kind=Kind::group;
    document.objects.emplace(collision.id,collision);document.compositions.front().roots.push_back(collision.id);
    Session session(std::move(document));
    apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
            std::optional<Id>{"logo-definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}},
        DefinitionCommand{SetInstanceOverride{"content-a",{"logo","","transform.tx"},90}}});
    rejects_atomically(session,"DUPLICATE_ID",[&]{apply(session,{ArtboardTemplateCommand{DetachArtboardTemplate{
        "comp","target-a","detach"}}});},
        "A fresh-ID collision during Template detach preserves the assigned Instance and all native/history state");

    apply(session,{Rename{"logo","Shared Item"},CreatePrimitive{"comp","source-root","logo-replacement","Shared Item",
        default_primitive("replacement-generator","nect.shape.rectangle")}});
    auto projected=scene(session.document());
    const auto original_proxy=proxy_for(projected,"content-a","logo");
    const auto replacement_proxy=proxy_for(projected,"content-a","logo-replacement");
    check(projected.instance_sources.at(original_proxy)=="logo"&&projected.instance_sources.at(replacement_proxy)=="logo-replacement"&&
        projected.expanded_values->at({original_proxy,"","transform.tx"})==90&&
        projected.expanded_values->at({replacement_proxy,"","transform.tx"})==0,
        "Same-name replacement does not retarget the stable SourceItem Ref or inherit its local override");
    rejects_atomically(session,"DANGLING_OVERRIDE",[&]{apply(session,{DeleteObjects{{"logo"}}});},
        "Deleting a live overridden SourceItem cannot leave a dangling reference or retarget by name");

    Session ordinary(source_document());
    apply(ordinary,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"logo-template","Shared Logo","source-art",
            std::optional<Id>{"logo-definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target-a","logo-template",std::optional<Id>{"content-a"}}},
        DefinitionCommand{SetInstanceOverride{"content-a",{"logo","","transform.tx"},90}},
        DefinitionCommand{SetInstanceOverride{"content-a",{"logo","","generator.width"},30}}});
    apply(ordinary,{DefinitionCommand{DetachInstance{"content-a","ordinary-detach"}}});
    check(board(ordinary.document(),"target-a").template_assignment&&
        !board(ordinary.document(),"target-a").template_assignment->content_instance&&
        ordinary.document().objects.at("content-a").kind==Kind::group&&!ordinary.document().objects.at("content-a").instance,
        "Ordinary R04 item detach clears only Template ownership while keeping the ordinary authored Group");
    const auto detached_root=ordinary.document().objects.at("content-a").children.front();
    const auto detached_logo=ordinary.document().objects.at(detached_root).children.front();
    apply(ordinary,{Set{{"logo","","transform.tx"},22},Set{{"logo","","generator.width"},80}});
    check(ordinary.document().objects.at(detached_logo).transform[4].literal==90&&
        ordinary.document().objects.at(detached_logo).source->parameters.at("width").literal==30&&
        !board(ordinary.document(),"target-a").template_assignment->content_instance,
        "Later source edits do not recreate or retarget the detached ordinary item");
}

void duplicate_native_instance_override_keys_are_rejected() {
    Session session(source_document());
    apply(session,{DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        DefinitionCommand{CreateInstance{"comp","","plain-instance","logo-definition","Plain"}},
        DefinitionCommand{SetInstanceOverride{"plain-instance",{"logo","","transform.tx"},90}},
        DefinitionCommand{SetInstanceOverride{"plain-instance",{"logo","","transform.ty"},91}}});
    auto malformed=encode(session.document());
    const auto duplicate=malformed.find("\"field\":\"transform.ty\"");
    check(duplicate!=std::string::npos,"Native R04 override fixture contains the second stable target Ref");
    malformed.replace(duplicate,std::string("\"field\":\"transform.ty\"").size(),"\"field\":\"transform.tx\"");
    rejects("DUPLICATE_OVERRIDE_KEY",[&]{(void)decode(malformed);});
}
}
int main() {
    try {
        templates_share_typed_evaluator_and_reset_independently();
        template_definition_root_must_share_the_composition();
        frame_sources_history_and_native_ownership();
        canonical_ab_logo_fixture_preserves_sources_and_local_overrides();
        r04_descendant_overrides_project_geometry_and_guard_legacy_versions();
        template_id_collisions_refuse_atomically();
        legacy_artboard_edits_keep_template_sources_and_owned_content();
        template_reference_and_lifecycle_failures_are_atomic();
        mixed_template_cycles_and_inactive_override_edges();
        template_source_items_keep_stable_identity_through_delete_and_ordinary_detach();
        duplicate_native_instance_override_keys_are_rejected();
        std::cout<<"PASS "<<checks<<" Template checks\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return 1;
    }
}
