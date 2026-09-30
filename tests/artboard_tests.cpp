#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code+": "+e.what()).c_str());return;}throw std::runtime_error(std::string("Expected ")+code+" rejection");}
std::size_t replace_all(std::string& value,std::string_view from,std::string_view to) {
    std::size_t count=0;
    for(auto position=value.find(from);position!=std::string::npos;position=value.find(from,position+to.size())) {
        value.replace(position,from.size(),to);++count;
    }
    return count;
}
void layout_and_guide_acceptance() {
    auto document=empty_document("layout-doc","layout-comp","layout-art");
    document.compositions.push_back({"other-comp","Other plane",{},{{"other-art","Other frame",0,0,400,300}}});
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    const auto starting_objects=session.document().objects;
    apply({AddGuide{"layout-comp",{"guide-x","Vertical guide","x",100}},
        AddGuide{"layout-comp",{"guide-y","Horizontal guide","y",250}}});
    ArtboardLayout layout;
    layout.margin=Margin{40,20,40,20};
    layout.grid=Grid{"grid-main",{40,20,880,600},2,1,20,0};
    apply({SetArtboardLayout{"layout-comp","layout-art",layout}});
    const auto& saved_layout=*session.document().compositions.front().artboards.front().layout;
    check(session.document().compositions.front().guides.size()==2&&saved_layout.margin==layout.margin&&saved_layout.grid==layout.grid,
        "P02-GRID-01 authors independent stable Guides, Margin and Grid values");
    const auto cell_width=(saved_layout.grid->bounds.width-saved_layout.grid->column_gutter)/2;
    const auto second_cell=saved_layout.grid->bounds.x+cell_width+saved_layout.grid->column_gutter;
    check(saved_layout.grid->bounds.x==40&&saved_layout.grid->bounds.x+cell_width==470&&second_cell==490&&
        second_cell+cell_width==920,"Two column fixture has exact [40,470] and [490,920] cell bounds");
    auto moved=Artboard{"layout-art","Artboard 1",200,0,960,640};
    apply({UpdateArtboard{"layout-comp",moved}});
    const auto moved_board=session.document().compositions.front().artboards.front();
    check(moved_board.layout&&moved_board.layout->grid->bounds==LayoutRect{40,20,880,600}&&
        moved_board.x+moved_board.layout->grid->bounds.x==240&&
        moved_board.x+moved_board.layout->grid->bounds.x+cell_width==670&&
        moved_board.x+second_cell==690&&moved_board.x+second_cell+cell_width==1120,
        "Artboard move preserves omitted layout payload and carries Grid world bounds");
    apply({UpdateGuide{"layout-comp",{"guide-x","Vertical guide","x",120}}});
    check(session.document().compositions.front().guides.front().position==120&&session.document().objects==starting_objects,
        "Guide move does not move authored artwork");
    auto copied=*moved_board.layout;copied.margin->left=50;
    apply({SetArtboardLayout{"layout-comp","layout-art",copied}});
    check(session.document().compositions.front().artboards.front().layout->margin->left==50&&
        session.document().compositions.front().artboards.front().layout->grid->bounds.x==40,
        "Margin edits do not relink the Grid to the one-time copied margin box");
    auto grid_only=*session.document().compositions.front().artboards.front().layout;grid_only.margin.reset();
    apply({SetArtboardLayout{"layout-comp","layout-art",grid_only}});
    check(!session.document().compositions.front().artboards.front().layout->margin&&
        session.document().compositions.front().artboards.front().layout->grid==grid_only.grid,
        "Clearing Margin retains the independently authored Grid");
    auto margin_only=*session.document().compositions.front().artboards.front().layout;
    margin_only.margin=copied.margin;margin_only.grid.reset();
    apply({SetArtboardLayout{"layout-comp","layout-art",margin_only}});
    check(session.document().compositions.front().artboards.front().layout->margin==margin_only.margin&&
        !session.document().compositions.front().artboards.front().layout->grid,
        "Clearing Grid retains the independently authored Margin");
    apply({SetArtboardLayout{"layout-comp","layout-art",copied}});
    const auto current=encode(session.document());
    check(current.find("\"version\":\"0.66\"")!=std::string::npos&&encode(decode(current))==current,
        "Native 0.43 roundtrip preserves Guide/Grid/Margin definitions and IDs");

    const auto readback=request(session,R"({"op":"inspect"})");
    check(readback.find("\"guides\"")!=std::string::npos&&readback.find("\"id\":\"guide-x\"")!=std::string::npos&&
        readback.find("\"id\":\"grid-main\"")!=std::string::npos,
        "API inspect returns the same committed Guide and layout identities as native state");
    const auto artboards=request(session,R"({"op":"artboards","composition":"layout-comp"})");
    check(artboards.find("\"authored\"")!=std::string::npos&&artboards.find("\"layout\"")!=std::string::npos,
        "Artboard API readback includes authored layout");

    auto api_document=empty_document("api-layout-doc","api-layout-comp","api-layout-art");Session api(api_document);
    auto response=request(api,R"({"op":"apply","expected_revision":0,"commands":[
      {"type":"add_guide","composition":"api-layout-comp","guide":{"id":"api-guide","name":"API guide","axis":"x","position":100}},
      {"type":"set_artboard_layout","composition":"api-layout-comp","artboard_id":"api-layout-art","layout":{"margin":{"left":40,"top":20,"right":40,"bottom":20},"grid":{"id":"api-grid","bounds":{"x":40,"y":20,"width":880,"height":600},"columns":2,"rows":1,"column_gutter":20,"row_gutter":0}}}
    ]})");
    check(response.find("\"ok\":true")!=std::string::npos&&api.revision()==1,"Semantic JSON adds Guide and sets full layout atomically");
    response=request(api,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"update_guide","composition":"api-layout-comp","guide":{"id":"api-guide","name":"API guide moved","axis":"x","position":120}}
    ]})");
    check(response.find("\"ok\":true")!=std::string::npos&&api.document().compositions.front().guides.front().position==120,
        "Semantic JSON updates Guide by stable ID");
    response=request(api,R"({"op":"apply","expected_revision":2,"commands":[
      {"type":"delete_guide","composition":"api-layout-comp","guide_id":"api-guide"}
    ]})");
    check(response.find("\"ok\":true")!=std::string::npos&&api.document().compositions.front().guides.empty(),
        "Semantic JSON deletes Guide by stable ID");
    response=request(api,R"({"op":"apply","expected_revision":3,"commands":[
      {"type":"set_artboard_layout","composition":"api-layout-comp","artboard_id":"api-layout-art","layout":null}
    ]})");
    check(response.find("\"ok\":true")!=std::string::npos&&!api.document().compositions.front().artboards.front().layout,
        "Nullable set_artboard_layout explicitly clears layout");
    api.undo(api.revision());api.undo(api.revision());
    check(api.document().compositions.front().guides.front().id=="api-guide"&&
        api.document().compositions.front().artboards.front().layout->grid->id=="api-grid",
        "Undo restores exact Guide and Grid IDs after semantic JSON edits");

    const auto stable=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    auto invalid_margin=*session.document().compositions.front().artboards.front().layout;
    invalid_margin.margin->left=960;invalid_margin.margin->right=0;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{"layout-comp","layout-art",invalid_margin}});});
    auto zero_cell=*session.document().compositions.front().artboards.front().layout;
    zero_cell.grid->column_gutter=880;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{"layout-comp","layout-art",zero_cell}});});
    auto nonfinite_grid=*session.document().compositions.front().artboards.front().layout;
    nonfinite_grid.grid->bounds.x=std::numeric_limits<double>::infinity();
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{"layout-comp","layout-art",nonfinite_grid}});});
    rejects("INVALID_GUIDE",[&]{apply({AddGuide{"layout-comp",{"bad-axis","Bad","z",0}}});});
    rejects("INVALID_GUIDE",[&]{apply({AddGuide{"layout-comp",{"bad-position","Bad","x",std::numeric_limits<double>::quiet_NaN()}}});});
    rejects("DUPLICATE_ID",[&]{apply({AddGuide{"layout-comp",{"grid-main","Duplicate","x",0}}});});
    rejects("MISSING_GUIDE",[&]{apply({DeleteGuide{"layout-comp","missing-guide"}});});
    rejects("WRONG_COMPOSITION",[&]{apply({UpdateGuide{"other-comp",{"guide-x","Wrong plane","x",10}}});});
    rejects("WRONG_COMPOSITION",[&]{apply({SetArtboardLayout{"layout-comp","other-art",std::nullopt}});});
    rejects("MISSING_ARTBOARD",[&]{apply({SetArtboardLayout{"layout-comp","missing-art",std::nullopt}});});
    const auto bad_count=request(api,R"({"op":"apply","expected_revision":6,"commands":[
      {"type":"set_artboard_layout","composition":"api-layout-comp","artboard_id":"api-layout-art","layout":{"grid":{"id":"api-grid","bounds":{"x":0,"y":0,"width":10,"height":10},"columns":1.5,"rows":1,"column_gutter":0,"row_gutter":0}}}
    ]})");
    check(bad_count.find("\"code\":\"INVALID_LAYOUT\"")!=std::string::npos,
        "Semantic JSON rejects fractional Grid counts with INVALID_LAYOUT");
    const auto bad_guide=request(api,R"({"op":"apply","expected_revision":6,"commands":[
      {"type":"add_guide","composition":"api-layout-comp","guide":{"id":"bad-guide","name":"Bad","axis":3,"position":0}}
    ]})");
    check(bad_guide.find("\"code\":\"INVALID_GUIDE\"")!=std::string::npos,
        "Semantic JSON maps malformed Guide fields to INVALID_GUIDE");
    const auto bad_layout=request(api,R"({"op":"apply","expected_revision":6,"commands":[
      {"type":"set_artboard_layout","composition":"api-layout-comp","artboard_id":"api-layout-art","layout":{"grid":{"id":"api-grid","bounds":[],"columns":1,"rows":1,"column_gutter":0,"row_gutter":0}}}
    ]})");
    check(bad_layout.find("\"code\":\"INVALID_LAYOUT\"")!=std::string::npos,
        "Semantic JSON maps malformed nested Grid fields to INVALID_LAYOUT");
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable,
        "P02-GRID-NEG failures preserve native state, revision and Undo timeline");

    auto child_doc=empty_document("parent-layout-doc","parent-layout-comp","parent-layout");
    Artboard child{"child-layout","Child",0,0,100,100};child.parent_size=ArtboardParent{"parent-layout",true,true};
    ArtboardLayout child_layout;child_layout.grid=Grid{"child-grid",{40,20,880,600},2,1,20,0};child.layout=child_layout;
    child_doc.compositions.front().artboards.front().width=960;child_doc.compositions.front().artboards.front().height=640;
    Session parent_layout(child_doc);parent_layout.apply({AddArtboard{"parent-layout-comp",child,1}},0);
    const auto parent_before=encode(parent_layout.document());const auto parent_history=parent_layout.history();
    Artboard narrowed{"parent-layout","Artboard 1",0,0,900,640};
    rejects("INVALID_LAYOUT",[&]{parent_layout.apply({UpdateArtboard{"parent-layout-comp",narrowed}},1);});
    check(parent_layout.revision()==1&&encode(parent_layout.document())==parent_before&&parent_layout.history()==parent_history,
        "Parent-size evaluation revalidates child Grid bounds and rejects atomically");

    auto legacy_document=session.document();
    for(auto& comp:legacy_document.compositions) {
        comp.guides.clear();
        for(auto& board:comp.artboards)board.layout.reset();
    }
    auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.13\"")==1,
        "Legacy fixture changes only its native version");
    check(replace_all(legacy,",\"guides\":[]","")==legacy_document.compositions.size(),
        "Legacy fixture removes each v0.14 Composition Guides field");
    const auto legacy_before=legacy;
    const auto migrated=decode(legacy);
    check(migrated==legacy_document&&legacy==legacy_before,"0.13 migration supplies empty Guides and absent layouts without changing source bytes");
    Session upgraded(migrated);upgraded.apply({AddGuide{"layout-comp",{"after-migration","New","x",300}},SetArtboardLayout{"layout-comp","layout-art",layout}},0);
    const auto reopened=decode(encode(upgraded.document()));
    check(reopened==upgraded.document(),"0.13 load then edit, save and fresh reopen retains all new authored IDs and values");
    upgraded.undo(upgraded.revision());
    check(upgraded.document()==migrated,"Undo returns to the exact migrated 0.13 authored state");
    rejects("NO_UNDO",[&]{upgraded.undo(upgraded.revision());});

    auto measured_bytes=[](const std::string& grid_id) {
        Session measured(empty_document("history-layout-doc","history-layout-comp","history-layout-art"));
        ArtboardLayout value;value.grid=Grid{grid_id,{10,10,100,100},2,2,10,10};
        measured.apply({SetArtboardLayout{"history-layout-comp","history-layout-art",value}},0);
        return measured.history().retained_bytes;
    };
    const auto short_history=measured_bytes("grid");const auto long_history=measured_bytes(std::string(80,'g'));
    check(long_history>short_history+64,"History estimate includes dynamically allocated Grid IDs");
    auto limited_doc=empty_document("history-layout-doc","history-layout-comp","history-layout-art");
    Session admission(limited_doc,{16,long_history-1});ArtboardLayout large_id;large_id.grid=Grid{std::string(80,'g'),{10,10,100,100},2,2,10,10};
    const auto prior=encode(admission.document());const auto prior_history=admission.history();
    rejects("HISTORY_LIMIT",[&]{admission.apply({SetArtboardLayout{"history-layout-comp","history-layout-art",large_id}},0);});
    check(admission.revision()==0&&admission.history()==prior_history&&encode(admission.document())==prior,
        "Layout history admission failure preserves the prior document and timeline");

    auto guide_limit_document=empty_document("guide-limit-doc","guide-limit-comp","guide-limit-art");
    auto& initial_guides=guide_limit_document.compositions.front().guides;initial_guides.reserve(10000);
    for(std::size_t i=0;i<10000;++i)initial_guides.push_back({"guide-"+std::to_string(i),"Guide","x",static_cast<double>(i)});
    Session guide_limit(std::move(guide_limit_document));
    const auto guide_limit_before=encode(guide_limit.document());const auto guide_limit_history=guide_limit.history();
    rejects("LIMIT",[&]{guide_limit.apply({AddGuide{"guide-limit-comp",{"guide-over-limit","Guide","x",10000}}},guide_limit.revision());});
    check(guide_limit.revision()==0&&guide_limit.document().compositions.front().guides.size()==10000&&
        guide_limit.history()==guide_limit_history&&encode(guide_limit.document())==guide_limit_before,
        "Document Guide limit rejects atomically without committing authored state or history");
}
void layout_typed_reads() {
    auto document=empty_document("layout-read-doc","layout-read-comp","layout-read-art");
    auto& composition=document.compositions.front();
    composition.artboards.front().name="Primary layout";
    composition.artboards.push_back({"layout-read-secondary","Secondary layout",0,0,200,150});
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};

    ArtboardLayout layout;
    layout.margin=Margin{10,20,30,40};
    layout.grid=Grid{"layout-read-grid",{40,20,880,600},2,3,20,10};
    apply({SetArtboardLayout{"layout-read-comp","layout-read-art",layout}});

    const std::array<Ref,12> expected{
        Ref{"layout-read-art","","margin.left"},Ref{"layout-read-art","","margin.top"},
        Ref{"layout-read-art","","margin.right"},Ref{"layout-read-art","","margin.bottom"},
        Ref{"layout-read-grid","","grid.bounds.x"},Ref{"layout-read-grid","","grid.bounds.y"},
        Ref{"layout-read-grid","","grid.bounds.width"},Ref{"layout-read-grid","","grid.bounds.height"},
        Ref{"layout-read-grid","","grid.columns"},Ref{"layout-read-grid","","grid.rows"},
        Ref{"layout-read-grid","","grid.column_gutter"},Ref{"layout-read-grid","","grid.row_gutter"}};
    using LayoutLiteral=std::variant<double,std::size_t>;
    std::array<std::pair<Ref,LayoutLiteral>,12> expected_values{{
        {expected[0],10.0},{expected[1],20.0},{expected[2],30.0},{expected[3],40.0},
        {expected[4],40.0},{expected[5],20.0},{expected[6],880.0},{expected[7],600.0},
        {expected[8],std::size_t{2}},{expected[9],std::size_t{3}},
        {expected[10],20.0},{expected[11],10.0}}};
    const auto discovered=properties(session.document());
    for(const auto& [ref,literal]:expected_values) {
        check(std::find(discovered.begin(),discovered.end(),ref)!=discovered.end(),
            "Properties discovers each present Margin and Grid field by its stable Ref");
        const auto typed=artboard_layout_property(session.document(),ref);
        check(typed.literal==literal,"Typed layout accessor maps each Ref to its exact authored field and value");
    }
    const auto list=request(session,R"({"op":"properties"})");
    for(const auto& ref:expected) {
        const auto ref_text="\"object\":\""+ref.object+"\",\"point\":\"\",\"field\":\""+ref.field+"\"";
        check(list.find(ref_text)!=std::string::npos,"Properties JSON lists the exact stable layout Ref");
    }
    auto read=[&](const Ref& ref) {
        return request(session,"{\"op\":\"get\",\"ref\":{\"object\":\""+ref.object+
            "\",\"point\":\"\",\"field\":\""+ref.field+"\"}}");
    };
    for(const auto& ref:expected) {
        const auto response=read(ref);
        const bool integer=ref.field=="grid.columns"||ref.field=="grid.rows";
        const bool link_capable=ref.field=="margin.left"||ref.field=="margin.top"||ref.field=="margin.right"||
            ref.field=="margin.bottom"||ref.field=="grid.bounds.x"||ref.field=="grid.bounds.y"||
            ref.field=="grid.bounds.width"||ref.field=="grid.bounds.height"||ref.field=="grid.column_gutter"||
            ref.field=="grid.row_gutter"||ref.field=="grid.columns"||ref.field=="grid.rows";
        const bool expression_capable=ref.field=="margin.left"||ref.field=="margin.top"||ref.field=="margin.right"||
            ref.field=="margin.bottom"||ref.field=="grid.bounds.x"||ref.field=="grid.bounds.y"||
            ref.field=="grid.bounds.width"||ref.field=="grid.bounds.height"||
            ref.field=="grid.column_gutter"||ref.field=="grid.row_gutter"||ref.field=="grid.columns"||
            ref.field=="grid.rows";
        check(response.find(integer?"\"type\":\"integer\"":"\"type\":\"number\"")!=std::string::npos&&
            response.find(integer?"\"unit\":\"unitless\"":"\"unit\":\"du\"")!=std::string::npos&&
            response.find("\"space\":\"artboard_local\"")!=std::string::npos&&
            response.find("\"origin\":\"authored\"")!=std::string::npos&&
            response.find(link_capable?"\"link\":true":"\"link\":false")!=std::string::npos&&
            response.find(expression_capable?"\"expression\":true":"\"expression\":false")!=std::string::npos,
            "Typed get reports each layout field's type, unit, space and link capability");
        check(response.find("\"authored\":{\"literal\":")!=std::string::npos&&
            response.find("\"evaluated\":")!=std::string::npos,
            "Typed get exposes authored literal and evaluated read separately");
        if(ref.field=="margin.left")check(response.find("\"source_kind\":\"literal\"")!=std::string::npos,
            "Literal Margin left explicitly identifies its inactive source kind");
        if(ref.field=="grid.columns")check(response.find("\"authored\":{\"literal\":2,")!=std::string::npos&&
            response.find("\"evaluated\":2")!=std::string::npos&&response.find("\"evaluated\":2.0")==std::string::npos,
            "Grid columns serialize as an integer JSON value");
        if(ref.field=="grid.rows")check(response.find("\"authored\":{\"literal\":3,")!=std::string::npos&&
            response.find("\"driver\":null")!=std::string::npos&&response.find("\"source_kind\":\"literal\"")!=std::string::npos&&
            response.find("\"evaluated\":3")!=std::string::npos&&response.find("\"evaluated\":3.0")==std::string::npos,
            "Grid rows serialize as an integer JSON value");
    }
    check(resolve_name(session.document(),"Primary layout","","margin.left")==expected[0],
        "Margin name resolution returns its owning Artboard Ref");
    const auto resolved=request(session,R"({"op":"resolve_name","name":"Primary layout","point":"","field":"margin.left"})");
    check(resolved.find("\"object\":\"layout-read-art\"")!=std::string::npos&&
        resolved.find("\"field\":\"margin.left\"")!=std::string::npos,
        "JSON resolve_name exposes the Margin Artboard Ref");
    auto duplicate_names=session.document();
    duplicate_names.compositions.front().artboards.back().name="Primary layout";
    rejects("AMBIGUOUS_NAME",[&]{(void)resolve_name(duplicate_names,"Primary layout","","margin.left");});
    rejects("INVALID_LAYOUT_REF",[&]{(void)artboard_layout_property(session.document(),{"layout-read-grid","point","grid.columns"});});
    rejects("UNKNOWN_LAYOUT_PROPERTY",[&]{(void)artboard_layout_property(session.document(),{"layout-read-grid","","grid.bounds.z"});});
    rejects("TYPE_MISMATCH",[&]{(void)artboard_layout_property(session.document(),{"layout-read-art","","grid.columns"});});
    rejects("TYPE_MISMATCH",[&]{(void)artboard_layout_property(session.document(),{"layout-read-grid","","margin.left"});});
    rejects("GRID_ID_REQUIRED",[&]{(void)resolve_name(session.document(),"Primary layout","","grid.columns");});

    auto resized=session.document().compositions.front().artboards.front();
    resized.name="Renamed layout";resized.x=200;resized.y=80;resized.width=1000;resized.height=700;
    apply({UpdateArtboard{"layout-read-comp",resized},ReorderArtboards{"layout-read-comp",{"layout-read-secondary","layout-read-art"}}});
    const auto after_reorder=properties(session.document());
    check(resolve_name(session.document(),"Renamed layout","","margin.left")==expected[0]&&
        std::find(after_reorder.begin(),after_reorder.end(),expected[8])!=after_reorder.end(),
        "Artboard rename, move, resize and reorder preserve stable local layout Refs");
    for(const auto& ref:expected) {
        const auto typed=artboard_layout_property(session.document(),ref);
        if(ref.field=="grid.columns")check(std::get<std::size_t>(typed.literal)==2,
            "Resizing its Artboard does not alter the authored Grid column count");
        else if(ref.field=="grid.rows")check(std::get<std::size_t>(typed.literal)==3,
            "Resizing its Artboard does not alter the authored Grid row count");
        else if(ref.field=="margin.left")check(std::get<double>(typed.literal)==10,
            "Moving or resizing its Artboard does not alter the authored Margin inset");
        else if(ref.field=="grid.bounds.x")check(std::get<double>(typed.literal)==40,
            "Moving its Artboard does not translate the authored Grid-local bounds");
    }
    check(read(expected[8]).find("\"name\":\"Renamed layout\"")!=std::string::npos&&
        read(expected[8]).find("\"object\":\"layout-read-grid\"")!=std::string::npos,
        "Grid property display uses its Artboard as context while the Grid ID remains the Ref owner");

    auto edited=*session.document().compositions.front().artboards.back().layout;edited.margin->left=11;
    apply({SetArtboardLayout{"layout-read-comp","layout-read-art",edited}});
    check(std::get<double>(artboard_layout_property(session.document(),expected[0]).literal)==11,
        "SetArtboardLayout updates the Margin literal visible through typed get");
    session.undo(session.revision());
    check(std::get<double>(artboard_layout_property(session.document(),expected[0]).literal)==10,
        "Undo restores the exact authored Margin read");
    session.redo(session.revision());
    check(std::get<double>(artboard_layout_property(session.document(),expected[0]).literal)==11,
        "Redo restores the exact authored Margin read");

    const auto native=encode(session.document());
    const auto cold=decode(native);
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&encode(cold)==native,
        "Native 0.43 cold decode/re-encode preserves existing Grid and Margin bytes");
    auto expected_after_edit=expected_values;expected_after_edit[0].second=11.0;
    for(const auto& [ref,literal]:expected_after_edit)
        check(artboard_layout_property(cold,ref).literal==literal,
            "Native 0.43 roundtrip preserves every exact typed layout literal");

    const auto rejection_state=encode(session.document());const auto rejection_revision=session.revision();
    const auto rejection_history=session.history();
    rejects("MISSING_REFERENCE",[&]{apply({Set{expected[0],99}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{expected[8],{{"layout-read-art","","artboard.width"},1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{expected[0]},{"1",1},false}});});
    auto failed_batch=*session.document().compositions.back().artboards.back().layout;
    failed_batch.margin->left=12;
    rejects("INVALID_GUIDE",[&]{session.apply({SetArtboardLayout{"layout-read-comp","layout-read-art",failed_batch},
        AddGuide{"layout-read-comp",{"bad-layout-batch-guide","Bad axis","z",0}}},session.revision());});
    check(session.revision()==rejection_revision&&session.history()==rejection_history&&
        encode(session.document())==rejection_state,
        "Generic Scalar mutation and a failing second batch command preserve layout bytes, revision and Undo history");

    auto no_margin=*session.document().compositions.back().artboards.back().layout;no_margin.margin.reset();
    apply({SetArtboardLayout{"layout-read-comp","layout-read-art",no_margin}});
    const auto without_margin=properties(session.document());
    check(std::find(without_margin.begin(),without_margin.end(),expected[0])==without_margin.end()&&
        std::get<std::size_t>(artboard_layout_property(session.document(),expected[8]).literal)==2,
        "Clearing Margin removes its Refs while preserving the Grid sibling");
    check(read(expected[0]).find("\"code\":\"MISSING_MARGIN\"")!=std::string::npos,
        "Exact Margin get reports MISSING_MARGIN after its component is cleared");
    session.undo(session.revision());
    auto no_grid=*session.document().compositions.back().artboards.back().layout;no_grid.grid.reset();
    apply({SetArtboardLayout{"layout-read-comp","layout-read-art",no_grid}});
    const auto without_grid=properties(session.document());
    check(std::find(without_grid.begin(),without_grid.end(),expected[8])==without_grid.end()&&
        std::get<double>(artboard_layout_property(session.document(),expected[0]).literal)==11,
        "Clearing Grid removes its Refs while preserving the Margin sibling");
    check(read(expected[8]).find("\"code\":\"MISSING_GRID\"")!=std::string::npos,
        "Exact Grid get reports MISSING_GRID after its component is cleared");
}

void margin_left_artboard_driver() {
    auto document=empty_document("margin-left-doc","margin-left-comp","margin-left-target");
    document.compositions.push_back({"margin-left-other-comp","Other plane",{},
        {{"margin-left-other","Other frame",0,0,400,300}}});
    const auto composition="margin-left-comp";
    const Ref target{"margin-left-target","","margin.left"};
    const Ref source{"margin-left-source","","artboard.width"};
    const Ref upstream{"margin-left-upstream","","artboard.width"};
    Artboard source_board{"margin-left-source","Source frame",0,0,40,100};
    source_board.parent_size=ArtboardParent{"margin-left-upstream",true,false};
    Artboard upstream_board{"margin-left-upstream","Upstream frame",0,0,40,100};
    ArtboardLayout layout;layout.margin=Margin{40,20,40,20};
    layout.grid=Grid{"margin-left-grid",{40,20,880,600},2,1,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_target=[&]() -> const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),
            [&](const Artboard& value){return value.id==target.object;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,upstream_board,2},
        SetArtboardLayout{composition,"margin-left-target",layout}});

    auto no_margin_document=empty_document("no-margin-doc","no-margin-comp","no-margin-target");
    Session no_margin_session(no_margin_document);
    const Ref no_margin_target{"no-margin-target","","margin.left"};
    const Ref no_margin_source{"no-margin-source","","artboard.height"};
    no_margin_session.apply({AddArtboard{"no-margin-comp",Artboard{"no-margin-source","Source",0,0,40,100},1}},0);
    rejects("MISSING_MARGIN",[&]{no_margin_session.apply({MarginLeftCommand{
        LinkMarginLeft{no_margin_target,no_margin_source,false}}},no_margin_session.revision());});

    auto smuggled=layout;smuggled.margin->left_driver=source;
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"margin-left-target",smuggled}});});
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{
        auto board=Artboard{"margin-left-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected;injected.margin=Margin{10,0,0,0,source};board.layout=injected;
        apply({AddArtboard{composition,board,3}});
    });

    auto json_link=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_margin_left","target":{"object":"margin-left-target","point":"","field":"margin.left"},
       "source":{"object":"margin-left-source","point":"","field":"artboard.width"},"replace_driver":false}
    ]})");
    check(json_link.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session command links Margin left through the dedicated revisioned command");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==40&&
        evaluate_artboard(session.document().compositions.front(),"margin-left-target").layout->margin->left==40,
        "Typed Margin read and pure Artboard evaluation separate authored literal, exact source and evaluated inset");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"margin-left-target","point":"","field":"margin.left"}})");
    check(typed_json.find("\"literal\":4E1")!=std::string::npos&&
        typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("\"field\":\"artboard.width\"")!=std::string::npos&&
        typed_json.find("\"evaluated\":4E1")!=std::string::npos&&typed_json.find("\"link\":true")!=std::string::npos,
        ("Typed JSON read reports authored Margin left, stable source Ref, evaluated value and link status: "+typed_json).c_str());

    apply({MarginLeftCommand{LinkMarginLeft{target,source,false}}});
    check(artboard_layout_property(session.document(),target).driver==source,
        "Repeating the exact existing Margin left source does not require replacement authorization");
    rejects("DRIVEN_MARGIN_LEFT",[&]{apply({MarginLeftCommand{LinkMarginLeft{target,upstream,false}}});});
    apply({MarginLeftCommand{LinkMarginLeft{target,upstream,true}}});
    check(artboard_layout_property(session.document(),target).driver==upstream,
        "Explicit replacement changes the Margin source through the dedicated command");
    apply({MarginLeftCommand{LinkMarginLeft{target,source,true}}});

    auto upstream_changed=session.document().compositions.front().artboards[2];upstream_changed.width=60;
    apply({UpdateArtboard{composition,upstream_changed}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==60&&
        evaluate_artboard(session.document().compositions.front(),"margin-left-target").layout->margin->left==60,
        "Parent-driven Artboard width propagates into the evaluated inset without changing authored Margin left");

    auto edited=*session.document().compositions.front().artboards.front().layout;
    edited.margin->top=25;edited.margin->right=45;edited.margin->bottom=30;
    edited.grid->bounds.x=45;
    apply({SetArtboardLayout{composition,"margin-left-target",edited}});
    auto target_board=session.document().compositions.front().artboards.front();
    target_board.name="Renamed target";target_board.x=120;
    apply({UpdateArtboard{composition,target_board},ReorderArtboards{composition,
        {"margin-left-upstream","margin-left-source","margin-left-target"}}});
    auto renamed_source=session.document().compositions.front().artboards[1];
    renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed_source}});
    upstream_changed=session.document().compositions.front().artboards.front();upstream_changed.width=70;
    apply({UpdateArtboard{composition,upstream_changed}});
    const auto& reordered=session.document().compositions.front();
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==70&&
        reordered.artboards[2].layout->margin->top==25&&reordered.artboards[2].layout->grid->id=="margin-left-grid"&&
        reordered.artboards[2].x==120&&reordered.artboards[1].x==50&&
        evaluate_artboard(reordered,"margin-left-target").layout->margin->left==70,
        "Rename, move and reorder preserve stable source identity while full-layout sibling edits retain the driver");

    const auto bytes_before=encode(session.document());const auto rev_before=session.revision();const auto history_before=session.history();
    auto direct=*find_target().layout;direct.margin->left=71;
    rejects("DRIVEN_MARGIN_LEFT",[&]{apply({SetArtboardLayout{composition,"margin-left-target",direct}});});
    auto cleared=*find_target().layout;cleared.margin.reset();
    rejects("DRIVEN_MARGIN_LEFT",[&]{apply({SetArtboardLayout{composition,"margin-left-target",cleared}});});
    auto direct_update=find_target();direct_update.layout->margin->left=71;
    rejects("DRIVEN_MARGIN_LEFT",[&]{apply({UpdateArtboard{composition,direct_update}});});
    direct_update=find_target();direct_update.layout->margin.reset();
    rejects("DRIVEN_MARGIN_LEFT",[&]{apply({UpdateArtboard{composition,direct_update}});});
    rejects("DUPLICATE_TARGET",[&]{apply({MarginLeftCommand{UnlinkMarginLeft{target}},
        MarginLeftCommand{LinkMarginLeft{target,source,true}}});});
    rejects("INVALID_ARTBOARD_REF",[&]{apply({MarginLeftCommand{LinkMarginLeft{target,{source.object,"p","artboard.width"},true}}});});
    rejects("INVALID_ARTBOARD_REF",[&]{apply({MarginLeftCommand{LinkMarginLeft{target,{source.object,"","margin.left"},true}}});});
    rejects("ARTBOARD_SELF_LINK",[&]{apply({MarginLeftCommand{LinkMarginLeft{target,{target.object,"","artboard.width"},true}}});});
    rejects("WRONG_COMPOSITION",[&]{apply({MarginLeftCommand{LinkMarginLeft{target,
        {"margin-left-other","","artboard.width"},true}}});});
    rejects("MISSING_ARTBOARD",[&]{apply({MarginLeftCommand{LinkMarginLeft{target,
        {"missing-margin-source","","artboard.width"},true}}});});
    rejects("MISSING_ARTBOARD",[&]{apply({MarginLeftCommand{LinkMarginLeft{target,
        {"margin-left-grid","","artboard.width"},true}}});});
    rejects("MISSING_REFERENCE",[&]{apply({Set{target,10}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{target,Binding{source,1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{target},{"1",1},false}});});
    check(session.revision()==rev_before&&session.history()==history_before&&encode(session.document())==bytes_before&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==70,
        "Driven edit, clear, generic Scalar routes and invalid sources preserve Document, revision, history and evaluation");

    const auto batch_before=encode(session.document());const auto batch_history=session.history();const auto batch_revision=session.revision();
    rejects("INVALID_GUIDE",[&]{session.apply({MarginLeftCommand{UnlinkMarginLeft{target}},
        AddGuide{composition,{"margin-left-bad-guide","Bad axis","z",0}}},session.revision());});
    check(session.revision()==batch_revision&&session.history()==batch_history&&encode(session.document())==batch_before,
        "A failing second batch command leaves the link, bytes, revision and Undo history unchanged");
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"margin-left-source"}});});
    upstream_changed=session.document().compositions.front().artboards.front();upstream_changed.width=920;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream_changed}});});
    check(session.revision()==batch_revision&&session.history()==batch_history&&encode(session.document())==batch_before&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==70,
        "Invalid upstream width leaves the last valid link evaluation and Session state intact");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"left_driver\"")!=std::string::npos&&native.find("margin-left-source")!=std::string::npos&&
        encode(decode(native))==native,
        "Native 0.43 stores the exact Margin source beside the authored literal and roundtrips bytes");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.34\"")==1,
        "Version-lie fixture changes only native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto literal_document=empty_document("margin-left-legacy-doc","margin-left-legacy-comp","margin-left-legacy-art");
    ArtboardLayout literal_layout;literal_layout.margin=Margin{40,20,40,20};
    Session literal_session(literal_document);literal_session.apply({SetArtboardLayout{"margin-left-legacy-comp",
        "margin-left-legacy-art",literal_layout}},0);
    auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(literal_session.document()));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.35\"")==1&&
        decode(legacy)==literal_session.document(),"Native 0.35 literal-only layout remains readable by the current decoder");

    apply({MarginLeftCommand{UnlinkMarginLeft{target}}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&std::get<double>(typed.evaluated)==70,
        "Unlink freezes the current evaluated inset into the authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==70,
        "Undo restores the exact source Ref and original literal");
    session.redo(session.revision());
    upstream_changed=session.document().compositions.front().artboards.front();upstream_changed.width=80;
    apply({UpdateArtboard{composition,upstream_changed}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&std::get<double>(typed.evaluated)==70,
        "Redo keeps the frozen inset independent from later source changes");
}

void margin_top_artboard_driver() {
    auto document=empty_document("margin-top-doc","margin-top-comp","margin-top-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    document.compositions.push_back({"margin-top-other-comp","Other plane",{},{{"other-art","Other frame",0,0,100,100}}});
    ArtboardLayout layout;layout.margin=Margin{40,40,40,40};
    layout.grid=Grid{"margin-top-grid",{40,40,880,560},1,1,0,0};target_board.layout=layout;
    const auto composition="margin-top-comp";
    const Ref target{"margin-top-target","","margin.top"};
    const Ref source{"margin-top-source","","artboard.height"};
    const Ref alternate{"margin-top-alternate","","artboard.height"};
    const Ref upstream{"margin-top-upstream","","artboard.height"};
    Artboard source_board{"margin-top-source","Source",0,0,100,50};
    source_board.parent_size=ArtboardParent{"margin-top-upstream",false,true};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_target=[&]() -> const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),
            [&](const Artboard& value){return value.id==target.object;});
    };
    apply({AddArtboard{composition,source_board,1},
        AddArtboard{composition,{"margin-top-alternate","Alternate",0,0,100,55},2},
        AddArtboard{composition,{"margin-top-upstream","Upstream",0,0,100,50},3}});
    const auto applied=request(session,R"json({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_margin_top","target":{"object":"margin-top-target","point":"","field":"margin.top"},
       "source":{"object":"margin-top-source","point":"","field":"artboard.height"},"replace_driver":false}
    ]})json");
    check(applied.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session command authors a revisioned Margin top Artboard-size link");
    const auto linked_revision=session.revision();const auto linked_history=session.history();
    apply({MarginTopCommand{LinkMarginTop{target,source,false}}});
    check(session.revision()==linked_revision&&session.history()==linked_history,
        "Reapplying the same Margin top source is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&!typed.expression&&typed.source_kind=="link"&&
        std::get<double>(typed.evaluated)==50&&evaluate_artboard(session.document().compositions.front(),target.object).layout->margin->top==50,
        "Pure Artboard evaluation and typed Margin top read retain the authored literal and evaluate the linked height");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"margin-top-target","point":"","field":"margin.top"}})");
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("margin-top-source")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        typed_json.find("\"evaluated\":")!=std::string::npos,
        ("Typed Margin top get exposes the exact link, evaluated value and link-only contract: "+typed_json).c_str());
    const auto props_json=request(session,R"({"op":"properties"})");
    check(props_json.find("margin-top-target")!=std::string::npos&&props_json.find("margin.top")!=std::string::npos,
        "Properties enumeration includes the typed Margin top source read");
    rejects("MISSING_REFERENCE",[&]{apply({Set{target,10}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{target,Binding{source,1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{target},{"1",1},false}});});

    auto upstream_board=session.document().compositions.front().artboards[3];upstream_board.height=60;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==60,
        "Inherited upstream height reevaluates Margin top without replacing its authored literal or stable source Ref");
    auto sibling=*find_target().layout;sibling.margin->bottom=50;sibling.margin->right=45;sibling.grid->bounds.x=45;
    apply({SetArtboardLayout{composition,target.object,sibling}});
    auto renamed=find_target();renamed.name="Renamed target";renamed.x=120;
    auto renamed_source=session.document().compositions.front().artboards[1];renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed},UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"margin-top-alternate",target.object,"margin-top-source","margin-top-upstream"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==60&&
        find_target().layout->margin->bottom==50&&find_target().layout->grid->id=="margin-top-grid"&&find_target().x==120,
        "Full-layout sibling edits, rename, move and reorder preserve the Margin top source and stable target identity");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    auto changed=*find_target().layout;changed.margin->top=41;
    rejects("DRIVEN_MARGIN_TOP",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    changed=*find_target().layout;changed.margin.reset();
    rejects("DRIVEN_MARGIN_TOP",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    auto direct=find_target();direct.layout->margin->top=41;
    rejects("DRIVEN_MARGIN_TOP",[&]{apply({UpdateArtboard{composition,direct}});});
    changed=*find_target().layout;changed.margin->top_driver=alternate;
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    auto smuggled=Artboard{"margin-top-smuggled","Smuggled",0,0,100,100};
    ArtboardLayout injected;injected.margin=Margin{10,10,10,10};injected.margin->top_driver=source;smuggled.layout=injected;
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{apply({AddArtboard{composition,smuggled,4}});});
    rejects("DUPLICATE_TARGET",[&]{apply({MarginTopCommand{LinkMarginTop{target,source,false}},
        MarginTopCommand{UnlinkMarginTop{target}}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({MarginTopCommand{UnlinkMarginTop{target}}},stable_revision-1);});
    rejects("INVALID_ARTBOARD_REF",[&]{apply({MarginTopCommand{LinkMarginTop{target,{source.object,"p","artboard.height"},true}}});});
    rejects("INVALID_ARTBOARD_REF",[&]{apply({MarginTopCommand{LinkMarginTop{target,{source.object,"","margin.top"},true}}});});
    rejects("ARTBOARD_SELF_LINK",[&]{apply({MarginTopCommand{LinkMarginTop{target,{target.object,"","artboard.height"},true}}});});
    rejects("WRONG_COMPOSITION",[&]{apply({MarginTopCommand{LinkMarginTop{target,{"other-art","","artboard.height"},true}}});});
    rejects("DRIVEN_MARGIN_TOP",[&]{apply({MarginTopCommand{LinkMarginTop{target,alternate,false}}});});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "Rejected direct edits, payload smuggling and invalid links preserve Margin top bytes, revision and history");
    apply({MarginTopCommand{LinkMarginTop{target,alternate,true}}});
    apply({MarginTopCommand{LinkMarginTop{target,source,true}}});
    const auto after_relink_document=encode(session.document());
    const auto after_relink_revision=session.revision();const auto after_relink_history=session.history();
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"margin-top-source"}});});
    upstream_board=session.document().compositions.front().artboards[3];upstream_board.height=590;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream_board}});});
    check(session.revision()==after_relink_revision&&session.history()==after_relink_history&&
        encode(session.document())==after_relink_document&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "Rejected source, payload and invalid upstream edits preserve the last valid Margin top evaluation");
    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&native.find("\"top_driver\"")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.43 stores the closed Margin top source and byte-roundtrips");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.40\"")==1,
        "Version-lie fixture changes only the native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    apply({MarginTopCommand{UnlinkMarginTop{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==60&&!typed.driver&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==60,"Unlink freezes the evaluated Margin top and removes the source in one step");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==60,
        "Undo restores the exact Margin top source and retained authored literal");
    session.redo(session.revision());upstream_board=session.document().compositions.front().artboards[3];upstream_board.height=70;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(!typed.driver&&std::get<double>(typed.literal)==60&&std::get<double>(typed.evaluated)==60,
        "Redo keeps the frozen Margin top independent of later Artboard size changes");
}

void margin_right_artboard_driver() {
    auto document=empty_document("margin-right-doc","margin-right-comp","margin-right-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    target_board.layout=ArtboardLayout{Margin{40,30,40,35},Grid{"margin-right-grid",{40,30,880,575},2,1,20,0}};
    document.compositions.push_back({"margin-right-other-comp","Other plane",{},{{"margin-right-other","Other frame",0,0,400,300}}});
    const auto composition="margin-right-comp";
    const Ref target{"margin-right-target","","margin.right"};
    const Ref source{"margin-right-source","","artboard.width"};
    const Ref alternate{"margin-right-alternate","","artboard.height"};
    const Ref upstream{"margin-right-upstream","","artboard.width"};
    Artboard source_board{"margin-right-source","Source",0,0,50,100};
    source_board.parent_size=ArtboardParent{"margin-right-upstream",true,false};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},
        AddArtboard{composition,{"margin-right-alternate","Alternate",0,0,100,60},2},
        AddArtboard{composition,{"margin-right-upstream","Upstream",0,0,50,100},3}});
    const auto linked=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_margin_right","target":{"object":"margin-right-target","point":"","field":"margin.right"},
       "source":{"object":"margin-right-source","point":"","field":"artboard.width"},"replace_driver":false}
    ]})");
    check(linked.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session authors a revisioned Margin right Artboard-size link");
    const auto linked_revision=session.revision();const auto linked_history=session.history();
    apply({MarginRightCommand{LinkMarginRight{target,source,false}}});
    check(session.revision()==linked_revision&&session.history()==linked_history,
        "Repeating the same Margin right source is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&!typed.expression&&typed.source_kind=="link"&&
        std::get<double>(typed.evaluated)==50&&evaluate_artboard(session.document().compositions.front(),target.object).layout->margin->right==50,
        "Pure Artboard evaluation retains the right literal and evaluates the exact source through a parent-size chain");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"margin-right-target","point":"","field":"margin.right"}})");
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("margin-right-source")!=std::string::npos&&typed_json.find("\"unit\":\"du\"")!=std::string::npos&&
        typed_json.find("\"expression\":true")!=std::string::npos&&typed_json.find("\"evaluated\":")!=std::string::npos,
        ("Typed Margin right get exposes the exact source, evaluated value and link-only contract: "+typed_json).c_str());
    const auto property_list=request(session,R"({"op":"properties"})");
    check(property_list.find("margin-right-target")!=std::string::npos&&property_list.find("margin.right")!=std::string::npos,
        "Properties enumeration includes the typed Margin right source read");
    rejects("MISSING_REFERENCE",[&]{apply({Set{target,10}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{target,Binding{source,1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{target},{"1",1},false}});});

    auto upstream_board=find_board("margin-right-upstream");upstream_board.width=60;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==60,
        "An upstream size edit reevaluates Margin right without replacing its literal or stable source Ref");
    auto sibling=*find_board(target.object).layout;sibling.margin->bottom=50;sibling.grid->bounds.x=45;
    apply({SetArtboardLayout{composition,target.object,sibling}});
    auto renamed=find_board(target.object);renamed.name="Renamed target";renamed.x=120;
    auto renamed_source=find_board(source.object);renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed},UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"margin-right-alternate",target.object,source.object,"margin-right-upstream"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==60&&
        find_board(target.object).layout->margin->bottom==50&&find_board(target.object).layout->grid->id=="margin-right-grid"&&
        find_board(target.object).x==120&&find_board(source.object).name=="Renamed source"&&find_board(source.object).x==50,
        "Full-layout sibling edits and Artboard rename, move and reorder preserve Margin right identity and evaluation");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    auto changed=*find_board(target.object).layout;changed.margin->right=41;
    rejects("DRIVEN_MARGIN_RIGHT",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    changed=*find_board(target.object).layout;changed.margin.reset();
    rejects("DRIVEN_MARGIN_RIGHT",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    auto direct=find_board(target.object);direct.layout->margin->right=41;
    rejects("DRIVEN_MARGIN_RIGHT",[&]{apply({UpdateArtboard{composition,direct}});});
    changed=*find_board(target.object).layout;changed.margin->right_driver=alternate;
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    auto smuggled=Artboard{"margin-right-smuggled","Smuggled",0,0,100,100};
    ArtboardLayout injected;injected.margin=Margin{10,10,10,10};injected.margin->right_driver=source;smuggled.layout=injected;
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{apply({AddArtboard{composition,smuggled,4}});});
    rejects("DUPLICATE_TARGET",[&]{apply({MarginRightCommand{LinkMarginRight{target,source,false}},
        MarginRightCommand{UnlinkMarginRight{target}}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({MarginRightCommand{UnlinkMarginRight{target}}},stable_revision-1);});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{source.object,"point","artboard.width"},"INVALID_ARTBOARD_REF"},
            {{source.object,"","margin.right"},"INVALID_ARTBOARD_REF"},
            {{target.object,"","artboard.width"},"ARTBOARD_SELF_LINK"},
            {{composition,"","artboard.width"},"TYPE_MISMATCH"},
            {{"margin-right-other","","artboard.width"},"WRONG_COMPOSITION"},
            {{"margin-right-missing","","artboard.width"},"MISSING_ARTBOARD"}})
        rejects(bad.second.c_str(),[&]{apply({MarginRightCommand{LinkMarginRight{target,bad.first,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({MarginRightCommand{LinkMarginRight{{target.object,"point","margin.right"},source,true}}});});
    rejects("MISSING_MARGIN",[&]{apply({MarginRightCommand{LinkMarginRight{{source.object,"","margin.right"},upstream,false}}});});
    rejects("DRIVEN_MARGIN_RIGHT",[&]{apply({MarginRightCommand{LinkMarginRight{target,alternate,false}}});});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "Rejected driven edits, source injection, invalid refs and unapproved replacement preserve bytes, revision and history");
    apply({MarginRightCommand{LinkMarginRight{target,alternate,true}}});
    check(artboard_layout_property(session.document(),target).driver==alternate&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "Explicit replacement changes right to a distinct Artboard height source");
    apply({MarginRightCommand{LinkMarginRight{target,source,true}}});
    const auto after_relink_document=encode(session.document());const auto after_relink_revision=session.revision();
    const auto after_relink_history=session.history();
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,source.object}});});
    upstream_board=find_board("margin-right-upstream");upstream_board.width=1000;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream_board}});});
    auto smaller_target=find_board(target.object);smaller_target.width=90;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,smaller_target}});});
    const auto batch_bytes=encode(session.document());const auto batch_revision=session.revision();const auto batch_history=session.history();
    rejects("INVALID_GUIDE",[&]{apply({MarginRightCommand{UnlinkMarginRight{target}},
        AddGuide{composition,{"margin-right-bad-guide","Bad axis","z",0}}});});
    check(session.revision()==batch_revision&&session.history()==batch_history&&encode(session.document())==batch_bytes&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "A failing later batch command cannot partially unlink the Margin right source");
    check(session.revision()==after_relink_revision&&session.history()==after_relink_history&&
        encode(session.document())==after_relink_document&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "Rejected source deletion and width-containment violations preserve the linked Margin right");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&native.find("\"right_driver\"")!=std::string::npos&&
        native.find("margin-right-source")!=std::string::npos&&encode(decode(native))==native,
        "Native 0.43 preserves the exact Margin right source beside its authored literal");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.42\"")==1,
        "Margin right version-lie fixture changes only the native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"right_driver\":{","\"right_driver\":{\"extra\":1,")==1,
        "Malformed Margin right driver fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto legacy_document=session.document();
    std::find_if(legacy_document.compositions.front().artboards.begin(),legacy_document.compositions.front().artboards.end(),
        [&](const Artboard& value){return value.id==target.object;})->layout->margin->right_driver.reset();
    auto native_042=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(native_042,"\"version\":\"0.66\"","\"version\":\"0.42\"")==1&&
        decode(native_042)==legacy_document,"Native 0.43 remains readable when the optional Margin right source is absent");

    apply({MarginRightCommand{UnlinkMarginRight{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==60&&!typed.driver&&typed.source_kind=="literal"&&std::get<double>(typed.evaluated)==60,
        "Unlink freezes evaluated Margin right into its authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==60,
        "Undo restores the exact Margin right source and retained literal");
    session.redo(session.revision());upstream_board=find_board("margin-right-upstream");upstream_board.width=80;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(!typed.driver&&std::get<double>(typed.literal)==60&&std::get<double>(typed.evaluated)==60,
        "Redo keeps frozen Margin right independent of later Artboard-size changes");
}

void margin_bottom_artboard_driver() {
    auto document=empty_document("margin-bottom-doc","margin-bottom-comp","margin-bottom-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    target_board.layout=ArtboardLayout{Margin{40,40,40,40},Grid{"margin-bottom-grid",{40,40,880,560},2,1,20,0}};
    document.compositions.push_back({"margin-bottom-other-comp","Other plane",{},{{"margin-bottom-other","Other frame",0,0,400,300}}});
    const auto composition="margin-bottom-comp";
    const Ref target{"margin-bottom-target","","margin.bottom"};
    const Ref source{"margin-bottom-source","","artboard.height"};
    const Ref alternate{"margin-bottom-alternate","","artboard.width"};
    const Ref upstream{"margin-bottom-upstream","","artboard.height"};
    Artboard source_board{"margin-bottom-source","Source",0,0,100,50};
    source_board.parent_size=ArtboardParent{"margin-bottom-upstream",false,true};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},
        AddArtboard{composition,{"margin-bottom-alternate","Alternate",0,0,100,60},2},
        AddArtboard{composition,{"margin-bottom-upstream","Upstream",0,0,50,50},3}});
    const auto linked=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_margin_bottom","target":{"object":"margin-bottom-target","point":"","field":"margin.bottom"},
       "source":{"object":"margin-bottom-source","point":"","field":"artboard.height"},"replace_driver":false}
    ]})");
    check(linked.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session authors a revisioned Margin bottom Artboard-size link");
    const auto linked_revision=session.revision();const auto linked_history=session.history();
    apply({MarginBottomCommand{LinkMarginBottom{target,source,false}}});
    check(session.revision()==linked_revision&&session.history()==linked_history,
        "Repeating the same Margin bottom source is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&!typed.expression&&typed.source_kind=="link"&&
        std::get<double>(typed.evaluated)==50&&evaluate_artboard(session.document().compositions.front(),target.object).layout->margin->bottom==50,
        "Pure Artboard evaluation retains the bottom literal and follows a parent-size source chain");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"margin-bottom-target","point":"","field":"margin.bottom"}})");
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("margin-bottom-source")!=std::string::npos&&typed_json.find("\"unit\":\"du\"")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        typed_json.find("\"evaluated\":5E1")!=std::string::npos,
        ("Typed Margin bottom get exposes exact source and evaluated du with expression support: "+typed_json).c_str());
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(properties_json.find("margin-bottom-target")!=std::string::npos&&properties_json.find("margin.bottom")!=std::string::npos,
        "Properties enumeration includes the typed Margin bottom source read");
    rejects("MISSING_REFERENCE",[&]{apply({Set{target,10}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{target,Binding{source,1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{target},{"1",1},false}});});

    auto upstream_board=find_board("margin-bottom-upstream");upstream_board.height=60;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==60,
        "An upstream size edit reevaluates Margin bottom without replacing its literal or stable source Ref");
    auto sibling=*find_board(target.object).layout;sibling.margin->top=45;sibling.margin->right=45;sibling.grid->bounds.y=45;
    apply({SetArtboardLayout{composition,target.object,sibling}});
    auto renamed=find_board(target.object);renamed.name="Renamed target";renamed.x=120;
    auto renamed_source=find_board(source.object);renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed},UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"margin-bottom-alternate",target.object,source.object,"margin-bottom-upstream"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==60&&
        find_board(target.object).layout->margin->top==45&&find_board(target.object).layout->grid->id=="margin-bottom-grid"&&
        find_board(target.object).x==120&&find_board(source.object).name=="Renamed source"&&find_board(source.object).x==50,
        "Full-layout sibling edits and Artboard rename, move and reorder preserve Margin bottom identity and evaluation");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    auto changed=*find_board(target.object).layout;changed.margin->bottom=41;
    rejects("DRIVEN_MARGIN_BOTTOM",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    changed=*find_board(target.object).layout;changed.margin.reset();
    rejects("DRIVEN_MARGIN_BOTTOM",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    auto direct=find_board(target.object);direct.layout->margin->bottom=41;
    rejects("DRIVEN_MARGIN_BOTTOM",[&]{apply({UpdateArtboard{composition,direct}});});
    changed=*find_board(target.object).layout;changed.margin->bottom_driver=alternate;
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,target.object,changed}});});
    auto smuggled=Artboard{"margin-bottom-smuggled","Smuggled",0,0,100,100};
    ArtboardLayout injected;injected.margin=Margin{10,10,10,10};injected.margin->bottom_driver=source;smuggled.layout=injected;
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{apply({AddArtboard{composition,smuggled,4}});});
    rejects("DUPLICATE_TARGET",[&]{apply({MarginBottomCommand{LinkMarginBottom{target,source,false}},
        MarginBottomCommand{UnlinkMarginBottom{target}}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({MarginBottomCommand{UnlinkMarginBottom{target}}},stable_revision-1);});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{source.object,"point","artboard.height"},"INVALID_ARTBOARD_REF"},
            {{source.object,"","margin.bottom"},"INVALID_ARTBOARD_REF"},
            {{target.object,"","artboard.height"},"ARTBOARD_SELF_LINK"},
            {{composition,"","artboard.height"},"TYPE_MISMATCH"},
            {{"margin-bottom-other","","artboard.height"},"WRONG_COMPOSITION"},
            {{"margin-bottom-missing","","artboard.height"},"MISSING_ARTBOARD"}})
        rejects(bad.second.c_str(),[&]{apply({MarginBottomCommand{LinkMarginBottom{target,bad.first,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({MarginBottomCommand{LinkMarginBottom{{target.object,"point","margin.bottom"},source,true}}});});
    rejects("MISSING_MARGIN",[&]{apply({MarginBottomCommand{LinkMarginBottom{{source.object,"","margin.bottom"},upstream,false}}});});
    rejects("DRIVEN_MARGIN_BOTTOM",[&]{apply({MarginBottomCommand{LinkMarginBottom{target,alternate,false}}});});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "Rejected driven edits, source injection, invalid refs and unapproved replacement preserve bytes, revision and history");

    apply({MarginBottomCommand{LinkMarginBottom{target,alternate,true}}});
    check(artboard_layout_property(session.document(),target).driver==alternate&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==100,
        "Explicit replacement changes bottom to a distinct Artboard width source");
    apply({MarginBottomCommand{LinkMarginBottom{target,source,true}}});
    const auto after_relink_document=encode(session.document());const auto after_relink_revision=session.revision();
    const auto after_relink_history=session.history();
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,source.object}});});
    upstream_board=find_board("margin-bottom-upstream");upstream_board.height=1000;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream_board}});});
    auto smaller_target=find_board(target.object);smaller_target.height=90;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,smaller_target}});});
    const auto batch_bytes=encode(session.document());const auto batch_revision=session.revision();const auto batch_history=session.history();
    rejects("INVALID_GUIDE",[&]{apply({MarginBottomCommand{UnlinkMarginBottom{target}},
        AddGuide{composition,{"margin-bottom-bad-guide","Bad axis","z",0}}});});
    check(session.revision()==batch_revision&&session.history()==batch_history&&encode(session.document())==batch_bytes&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "A failing later batch command cannot partially unlink Margin bottom");
    check(session.revision()==after_relink_revision&&session.history()==after_relink_history&&
        encode(session.document())==after_relink_document&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "Rejected source deletion and vertical-containment edits preserve the linked Margin bottom");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&native.find("\"bottom_driver\"")!=std::string::npos&&
        native.find("margin-bottom-source")!=std::string::npos&&encode(decode(native))==native,
        "Native 0.60 preserves the exact Margin bottom source beside its authored literal");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.44\"")==1,
        "Margin bottom version-lie fixture changes only the native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"bottom_driver\":{","\"bottom_driver\":{\"extra\":1,")==1,
        "Malformed Margin bottom driver fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto legacy_document=session.document();
    std::find_if(legacy_document.compositions.front().artboards.begin(),legacy_document.compositions.front().artboards.end(),
        [&](const Artboard& value){return value.id==target.object;})->layout->margin->bottom_driver.reset();
    auto native_044=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(native_044,"\"version\":\"0.66\"","\"version\":\"0.44\"")==1&&
        decode(native_044)==legacy_document,"Native 0.44 remains readable when the optional Margin bottom source is absent");

    apply({MarginBottomCommand{UnlinkMarginBottom{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==60&&!typed.driver&&typed.source_kind=="literal"&&std::get<double>(typed.evaluated)==60,
        "Unlink freezes evaluated Margin bottom into its authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==60,
        "Undo restores the exact Margin bottom source and retained literal");
    session.redo(session.revision());upstream_board=find_board("margin-bottom-upstream");upstream_board.height=80;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(!typed.driver&&std::get<double>(typed.literal)==60&&std::get<double>(typed.evaluated)==60,
        "Redo keeps frozen Margin bottom independent of later Artboard-size changes");
}

void margin_top_expression() {
    auto document=empty_document("margin-top-expression-doc","margin-top-expression-comp","margin-top-expression-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    target_board.layout=ArtboardLayout{Margin{40,40,40,40},std::nullopt};
    document.compositions.push_back({"margin-top-expression-other-comp","Other plane",{},
        {{"margin-top-expression-other","Other frame",0,0,100,100}}});
    const auto composition="margin-top-expression-comp";
    const Ref target{"margin-top-expression-target","","margin.top"};
    const Ref source{"margin-top-expression-source","","artboard.height"};
    const Ref alternate{"margin-top-expression-alternate","","artboard.height"};
    const Ref upstream{"margin-top-expression-upstream","","artboard.height"};
    const Expression expression{R"(ref("margin-top-expression-source","","artboard.height") + 10)",1};
    Artboard source_board{"margin-top-expression-source","Source",0,0,100,50};
    source_board.parent_size=ArtboardParent{"margin-top-expression-upstream",false,true};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto target_board_now=[&]() -> const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),
            [&](const Artboard& value){return value.id==target.object;});
    };
    apply({AddArtboard{composition,source_board,1},
        AddArtboard{composition,{alternate.object,"Alternate",0,0,100,55},2},
        AddArtboard{composition,{upstream.object,"Upstream",0,0,100,50},3}});
    const auto applied=request(session,R"json({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_margin_top_expression","target":{"object":"margin-top-expression-target","point":"","field":"margin.top"},
       "expression":{"source":"ref(\"margin-top-expression-source\",\"\",\"artboard.height\") + 10","version":1},"replace_driver":false}
    ]})json");
    check(applied.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session command authors a revisioned Margin top expression");
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({MarginTopCommand{SetMarginTopExpression{target,expression,false}}});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the same Margin top expression is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&!typed.driver&&typed.expression==expression&&
        typed.source_kind=="expression"&&std::get<double>(typed.evaluated)==60&&
        evaluate_artboard(session.document().compositions.front(),target.object).layout->margin->top==60,
        "Pure Artboard evaluation and typed Margin top read preserve its authored literal and exact du expression");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"margin-top-expression-target","point":"","field":"margin.top"}})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("margin-top-expression-source")!=std::string::npos&&
        typed_json.find("\"unit\":\"du\"")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos,
        ("Typed Margin top get exposes the exact expression, unit and expression support: "+typed_json).c_str());
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(properties_json.find("margin-top-expression-target")!=std::string::npos&&
        properties_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        "Properties enumeration returns the same typed Margin top expression");

    rejects("DRIVEN_MARGIN_TOP",[&]{apply({MarginTopCommand{LinkMarginTop{target,alternate,false}}});});
    apply({MarginTopCommand{LinkMarginTop{target,alternate,true}}});
    rejects("DRIVEN_MARGIN_TOP",[&]{apply({MarginTopCommand{SetMarginTopExpression{target,expression,false}}});});
    apply({MarginTopCommand{SetMarginTopExpression{target,expression,true}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&!typed.driver&&std::get<double>(typed.literal)==40&&
        std::get<double>(typed.evaluated)==60,
        "Link and expression replacement require explicit authorization and retain the authored top inset");

    auto upstream_board=session.document().compositions.front().artboards[3];upstream_board.height=60;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70,
        "Inherited upstream Artboard height reevaluates Margin top without changing expression text or literal");
    auto sibling_layout=*target_board_now().layout;sibling_layout.margin->bottom=45;
    apply({SetArtboardLayout{composition,target.object,sibling_layout}});
    auto moved_target=target_board_now();moved_target.name="Renamed target";moved_target.x=120;
    auto moved_source=session.document().compositions.front().artboards[1];moved_source.name="Renamed source";moved_source.x=60;
    apply({UpdateArtboard{composition,moved_target},UpdateArtboard{composition,moved_source},
        ReorderArtboards{composition,{alternate.object,target.object,source.object,upstream.object}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70&&
        target_board_now().layout->margin->bottom==45&&target_board_now().x==120,
        "Full-layout edits, rename, move and reorder preserve the Margin top expression's stable Artboard Ref");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    const auto invalid=[&](const char* code,std::vector<Command> commands,const char* why) {
        rejects(code,[&]{apply(std::move(commands));});
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,why);
    };
    invalid("INVALID_LAYOUT_REF",{MarginTopCommand{SetMarginTopExpression{{target.object,"p","margin.top"},expression,true}}},
        "Invalid target identity leaves top expression state, native bytes, revision and history unchanged");
    invalid("MARGIN_TOP_EXPRESSION_TYPE",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("margin-top-expression-source","point","artboard.height"))",1},true}}},
        "A point-qualified Artboard Ref is rejected atomically for Margin top");
    invalid("MARGIN_TOP_EXPRESSION_TYPE",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("margin-top-expression-grid","","grid.bounds.y"))",1},true}}},
        "A wrong-kind Grid property Ref is rejected atomically for Margin top");
    invalid("ARTBOARD_SELF_LINK",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("margin-top-expression-target","","artboard.height"))",1},true}}},
        "A self-referential Margin top expression is rejected atomically");
    invalid("WRONG_COMPOSITION",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("margin-top-expression-other","","artboard.height"))",1},true}}},
        "A cross-Composition Margin top expression is rejected atomically");
    invalid("MISSING_ARTBOARD",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("missing-margin-top-source","","artboard.height"))",1},true}}},
        "A missing Artboard source is rejected atomically");
    invalid("UNIT_MISMATCH",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("margin-top-expression-source","","artboard.height") + ref("object","","generator.rotation"))",1},true}}},
        "A non-du expression is rejected atomically");
    invalid("EXPRESSION_DOMAIN",{MarginTopCommand{SetMarginTopExpression{target,{"1 / 0",1},true}}},
        "A domain-invalid Margin top expression is rejected atomically");
    invalid("NON_FINITE",{MarginTopCommand{SetMarginTopExpression{target,{"1e308 * 10",1},true}}},
        "A non-finite Margin top expression is rejected atomically");
    invalid("INVALID_LAYOUT",{MarginTopCommand{SetMarginTopExpression{target,{"-1",1},true}}},
        "A negative evaluated Margin top is rejected atomically");
    invalid("INVALID_LAYOUT",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("margin-top-expression-source","","artboard.height") + 590)",1},true}}},
        "An expression that leaves no positive target content height is rejected atomically");
    invalid("DRIVEN_MARGIN_TOP",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("margin-top-expression-source","","artboard.height") + 11)",1},false}}},
        "Replacing a different top expression requires explicit authorization");
    invalid("DRIVEN_MARGIN_TOP",{MarginTopCommand{LinkMarginTop{target,alternate,false}}},
        "Replacing an expression with a link requires explicit authorization");
    invalid("DRIVEN_MARGIN_TOP",{SetArtboardLayout{composition,target.object,[&]{auto value=*target_board_now().layout;
        value.margin->top=41;return value;}()}},"A driven Margin top literal cannot be changed through a full-layout edit");
    invalid("DRIVEN_MARGIN_TOP",{SetArtboardLayout{composition,target.object,[&]{auto value=*target_board_now().layout;
        value.margin.reset();return value;}()}},"A driven Margin top cannot be cleared through a full-layout edit");
    invalid("MARGIN_DRIVER_SMUGGLING",{[&]{auto value=*target_board_now().layout;
        value.margin->top_expression=Expression{"5",1};return Command{SetArtboardLayout{composition,target.object,value}};}()},
        "A full-layout payload cannot replace a top expression source");
    auto smuggled=Artboard{"margin-top-expression-smuggled","Smuggled",0,0,100,100};
    ArtboardLayout smuggled_layout;smuggled_layout.margin=Margin{10,10,10,10};
    smuggled_layout.margin->top_expression=expression;smuggled.layout=smuggled_layout;
    invalid("MARGIN_DRIVER_SMUGGLING",{AddArtboard{composition,smuggled,4}},
        "AddArtboard cannot inject a Margin top expression source");
    invalid("DUPLICATE_TARGET",{MarginTopCommand{SetMarginTopExpression{target,expression,true}},
        MarginTopCommand{UnlinkMarginTop{target}}},"A batch cannot change a Margin top target twice");
    invalid("MISSING_REFERENCE",{MarginTopCommand{SetMarginTopExpression{target,
        {R"(ref("margin-top-expression-source","","artboard.height") + 10)",1},true}},Set{target,1}},
        "A failed second batch command leaves no first-command Margin top expression commit");
    invalid("ARTBOARD_IN_USE",{DeleteArtboard{composition,source.object}},
        "A source Artboard cannot be deleted while a Margin top expression references it");
    auto bad_source=session.document().compositions.front().artboards[2];
    bad_source.layout=ArtboardLayout{Margin{0,30,0,30},std::nullopt};
    invalid("INVALID_LAYOUT",{UpdateArtboard{composition,bad_source}},
        "An upstream Artboard with no positive content height cannot invalidate a surviving expression");
    auto invalid_upstream=session.document().compositions.front().artboards[3];invalid_upstream.height=590;
    invalid("INVALID_LAYOUT",{UpdateArtboard{composition,invalid_upstream}},
        "An upstream size edit that closes target content height is rejected atomically");
    rejects("REVISION_CONFLICT",[&]{session.apply({MarginTopCommand{UnlinkMarginTop{target}}},stable_revision-1);});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "Stale Margin top expression commands preserve the exact Session state");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"top_expression\":{\"source\":\"ref(\\\"margin-top-expression-source\\\",\\\"\\\",\\\"artboard.height\\\") + 10\",\"version\":1}")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.43 preserves exact Margin top expression source and byte-roundtrips");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.41\"")==1,
        "Version-lie fixture changes only the native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto legacy_document=session.document();
    auto& legacy_target=*std::find_if(legacy_document.compositions.front().artboards.begin(),
        legacy_document.compositions.front().artboards.end(),[&](const Artboard& value){return value.id==target.object;});
    legacy_target.layout->margin->top_expression.reset();legacy_target.layout->margin->top=40;
    auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.41\"")==1&&decode(legacy)==legacy_document,
        "Native 0.41 remains readable for literal-only Margin top documents");

    apply({MarginTopCommand{UnlinkMarginTop{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==70,"Unlink freezes the evaluated Margin top and removes its expression in one step");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70,
        "Undo restores the exact Margin top expression and retained authored literal");
    session.redo(session.revision());upstream_board=session.document().compositions.front().artboards[3];upstream_board.height=70;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(!typed.driver&&!typed.expression&&std::get<double>(typed.literal)==70&&std::get<double>(typed.evaluated)==70,
        "Redo keeps the frozen Margin top independent of later Artboard-size changes");
}

void margin_right_expression() {
    auto document=empty_document("margin-right-expression-doc","margin-right-expression-comp","margin-right-expression-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    target_board.layout=ArtboardLayout{Margin{40,40,40,40},std::nullopt};
    document.compositions.push_back({"margin-right-expression-other-comp","Other plane",{},
        {{"margin-right-expression-other","Other frame",0,0,100,100}}});
    const auto composition="margin-right-expression-comp";
    const Ref target{"margin-right-expression-target","","margin.right"};
    const Ref source{"margin-right-expression-source","","artboard.width"};
    const Ref alternate{"margin-right-expression-alternate","","artboard.height"};
    const Ref upstream{"margin-right-expression-upstream","","artboard.width"};
    const Expression expression{R"(ref("margin-right-expression-source","","artboard.width") + 10)",1};
    Artboard source_board{"margin-right-expression-source","Source",0,0,50,100};
    source_board.parent_size=ArtboardParent{"margin-right-expression-upstream",true,false};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},
        AddArtboard{composition,{alternate.object,"Alternate",0,0,100,55},2},
        AddArtboard{composition,{upstream.object,"Upstream",0,0,50,100},3}});
    const auto applied=request(session,R"json({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_margin_right_expression","target":{"object":"margin-right-expression-target","point":"","field":"margin.right"},
       "expression":{"source":"ref(\"margin-right-expression-source\",\"\",\"artboard.width\") + 10","version":1},"replace_driver":false}
    ]})json");
    check(applied.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session command authors a revisioned Margin right expression");
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({MarginRightCommand{SetMarginRightExpression{target,expression,false}}});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the same Margin right expression is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&!typed.driver&&typed.expression==expression&&
        typed.source_kind=="expression"&&std::get<double>(typed.evaluated)==60&&
        evaluate_artboard(session.document().compositions.front(),target.object).layout->margin->right==60,
        "Pure Artboard evaluation and typed Margin right read preserve its authored literal and exact du expression");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"margin-right-expression-target","point":"","field":"margin.right"}})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("margin-right-expression-source")!=std::string::npos&&
        typed_json.find("\"unit\":\"du\"")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos,
        ("Typed Margin right get exposes the exact expression, unit and expression support: "+typed_json).c_str());
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(properties_json.find("margin-right-expression-target")!=std::string::npos&&
        properties_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        "Properties enumeration returns the same typed Margin right expression");

    apply({MarginRightCommand{LinkMarginRight{target,alternate,true}}});
    rejects("DRIVEN_MARGIN_RIGHT",[&]{apply({MarginRightCommand{SetMarginRightExpression{target,expression,false}}});});
    apply({MarginRightCommand{SetMarginRightExpression{target,expression,true}}});
    rejects("DRIVEN_MARGIN_RIGHT",[&]{apply({MarginRightCommand{LinkMarginRight{target,alternate,false}}});});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&!typed.driver&&std::get<double>(typed.literal)==40&&
        std::get<double>(typed.evaluated)==60,
        "Link and expression replacement require explicit authorization and retain the authored right inset");

    auto upstream_board=find_board(upstream.object);upstream_board.width=60;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70,
        "Inherited upstream Artboard width reevaluates Margin right without changing expression text or literal");
    auto sibling_layout=*find_board(target.object).layout;sibling_layout.margin->bottom=45;
    apply({SetArtboardLayout{composition,target.object,sibling_layout}});
    auto moved_target=find_board(target.object);moved_target.name="Renamed target";moved_target.x=120;
    auto moved_source=find_board(source.object);moved_source.name="Renamed source";moved_source.x=60;
    apply({UpdateArtboard{composition,moved_target},UpdateArtboard{composition,moved_source},
        ReorderArtboards{composition,{alternate.object,target.object,source.object,upstream.object}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70&&
        find_board(target.object).layout->margin->bottom==45&&find_board(target.object).x==120&&
        find_board(source.object).name=="Renamed source"&&find_board(source.object).x==60,
        "Full-layout edits, rename, move and reorder preserve the Margin right expression's stable Artboard Ref");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    const auto invalid=[&](const char* code,std::vector<Command> commands,const char* why) {
        rejects(code,[&]{apply(std::move(commands));});
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,why);
    };
    invalid("INVALID_LAYOUT_REF",{MarginRightCommand{SetMarginRightExpression{{target.object,"p","margin.right"},expression,true}}},
        "Invalid target identity leaves right expression state, native bytes, revision and history unchanged");
    invalid("MARGIN_RIGHT_EXPRESSION_TYPE",{MarginRightCommand{SetMarginRightExpression{target,
        {R"(ref("margin-right-expression-source","point","artboard.width"))",1},true}}},
        "A point-qualified Artboard Ref is rejected atomically for Margin right");
    invalid("MARGIN_RIGHT_EXPRESSION_TYPE",{MarginRightCommand{SetMarginRightExpression{target,
        {R"(ref("margin-right-expression-grid","","grid.bounds.y"))",1},true}}},
        "A wrong-kind Grid property Ref is rejected atomically for Margin right");
    invalid("ARTBOARD_SELF_LINK",{MarginRightCommand{SetMarginRightExpression{target,
        {R"(ref("margin-right-expression-target","","artboard.width"))",1},true}}},
        "A self-referential Margin right expression is rejected atomically");
    invalid("WRONG_COMPOSITION",{MarginRightCommand{SetMarginRightExpression{target,
        {R"(ref("margin-right-expression-other","","artboard.width"))",1},true}}},
        "A cross-Composition Margin right expression is rejected atomically");
    invalid("MISSING_ARTBOARD",{MarginRightCommand{SetMarginRightExpression{target,
        {R"(ref("missing-margin-right-source","","artboard.width"))",1},true}}},
        "A missing Artboard source is rejected atomically");
    invalid("UNIT_MISMATCH",{MarginRightCommand{SetMarginRightExpression{target,
        {R"(ref("margin-right-expression-source","","artboard.width") + ref("object","","generator.rotation"))",1},true}}},
        "A non-du expression is rejected atomically");
    invalid("EXPRESSION_SYNTAX",{MarginRightCommand{SetMarginRightExpression{target,{"ref(",1},true}}},
        "A malformed Margin right expression is rejected atomically");
    invalid("UNSUPPORTED_EXPRESSION_VERSION",{MarginRightCommand{SetMarginRightExpression{target,{"1",2},true}}},
        "An unsupported Margin right expression version is rejected atomically");
    invalid("EXPRESSION_DOMAIN",{MarginRightCommand{SetMarginRightExpression{target,{"1 / 0",1},true}}},
        "A domain-invalid Margin right expression is rejected atomically");
    invalid("NON_FINITE",{MarginRightCommand{SetMarginRightExpression{target,{"1e308 * 10",1},true}}},
        "A non-finite Margin right expression is rejected atomically");
    invalid("INVALID_LAYOUT",{MarginRightCommand{SetMarginRightExpression{target,{"-1",1},true}}},
        "A negative evaluated Margin right is rejected atomically");
    invalid("INVALID_LAYOUT",{MarginRightCommand{SetMarginRightExpression{target,
        {R"(ref("margin-right-expression-source","","artboard.width") + 910)",1},true}}},
        "An expression that leaves no positive target content width is rejected atomically");
    invalid("DRIVEN_MARGIN_RIGHT",{MarginRightCommand{SetMarginRightExpression{target,
        {R"(ref("margin-right-expression-source","","artboard.width") + 11)",1},false}}},
        "Replacing a different right expression requires explicit authorization");
    invalid("DRIVEN_MARGIN_RIGHT",{MarginRightCommand{LinkMarginRight{target,alternate,false}}},
        "Replacing a Margin right expression with a link requires explicit authorization");
    invalid("DRIVEN_MARGIN_RIGHT",{SetArtboardLayout{composition,target.object,[&]{auto value=*find_board(target.object).layout;
        value.margin->right=41;return value;}()}},"A driven Margin right literal cannot be changed through a full-layout edit");
    invalid("DRIVEN_MARGIN_RIGHT",{SetArtboardLayout{composition,target.object,[&]{auto value=*find_board(target.object).layout;
        value.margin.reset();return value;}()}},"A driven Margin right cannot be cleared through a full-layout edit");
    invalid("MARGIN_DRIVER_SMUGGLING",{[&]{auto value=*find_board(target.object).layout;
        value.margin->right_expression=Expression{"5",1};return Command{SetArtboardLayout{composition,target.object,value}};}()},
        "A full-layout payload cannot replace a right expression source");
    auto smuggled=Artboard{"margin-right-expression-smuggled","Smuggled",0,0,100,100};
    ArtboardLayout smuggled_layout;smuggled_layout.margin=Margin{10,10,10,10};
    smuggled_layout.margin->right_expression=expression;smuggled.layout=smuggled_layout;
    invalid("MARGIN_DRIVER_SMUGGLING",{AddArtboard{composition,smuggled,4}},
        "AddArtboard cannot inject a Margin right expression source");
    invalid("DUPLICATE_TARGET",{MarginRightCommand{SetMarginRightExpression{target,expression,true}},
        MarginRightCommand{UnlinkMarginRight{target}}},"A batch cannot change a Margin right target twice");
    invalid("MISSING_REFERENCE",{MarginRightCommand{SetMarginRightExpression{target,expression,true}},Set{target,1}},
        "A failed second batch command leaves no first-command Margin right expression commit");
    invalid("ARTBOARD_IN_USE",{DeleteArtboard{composition,source.object}},
        "A source Artboard cannot be deleted while a Margin right expression references it");
    auto invalid_upstream=find_board(upstream.object);invalid_upstream.width=1000;
    invalid("INVALID_LAYOUT",{UpdateArtboard{composition,invalid_upstream}},
        "An upstream size edit that closes target content width is rejected atomically");
    rejects("REVISION_CONFLICT",[&]{session.apply({MarginRightCommand{UnlinkMarginRight{target}}},stable_revision-1);});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "Stale Margin right expression commands preserve the exact Session state");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"right_expression\":{\"source\":\"ref(\\\"margin-right-expression-source\\\",\\\"\\\",\\\"artboard.width\\\") + 10\",\"version\":1}")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.60 preserves exact Margin right expression source and byte-roundtrips");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.43\"")==1,
        "Margin right expression version-lie fixture changes only the native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"right_expression\":{","\"right_expression\":{\"extra\":1,")==1,
        "Malformed Margin right expression fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto conflicting=native;check(replace_all(conflicting,"\"right_expression\":{","\"right_driver\":{\"link\":{\"object\":\"margin-right-expression-source\",\"point\":\"\",\"field\":\"artboard.width\"}},\"right_expression\":{")==1,
        "Conflicting Margin right source fixture adds a driver beside the expression");
    rejects("MARGIN_SOURCE_CONFLICT",[&]{(void)decode(conflicting);});
    auto legacy_document=session.document();
    auto& legacy_target=*std::find_if(legacy_document.compositions.front().artboards.begin(),
        legacy_document.compositions.front().artboards.end(),[&](const Artboard& value){return value.id==target.object;});
    legacy_target.layout->margin->right_expression.reset();legacy_target.layout->margin->right=40;
    auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.43\"")==1&&decode(legacy)==legacy_document,
        "Native 0.43 remains readable when the optional Margin right expression is absent");

    apply({MarginRightCommand{UnlinkMarginRight{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==70,"Unlink freezes evaluated Margin right and removes its expression in one step");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70,
        "Undo restores exact Margin right expression and retained authored literal");
    session.redo(session.revision());upstream_board=find_board(upstream.object);upstream_board.width=80;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(!typed.driver&&!typed.expression&&std::get<double>(typed.literal)==70&&std::get<double>(typed.evaluated)==70,
        "Redo keeps frozen Margin right independent of later Artboard-size changes");
}

void margin_bottom_expression() {
    auto document=empty_document("margin-bottom-expression-doc","margin-bottom-expression-comp","margin-bottom-expression-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    target_board.layout=ArtboardLayout{Margin{40,40,40,40},Grid{"margin-bottom-expression-grid",{40,40,880,560},2,1,20,0}};
    document.compositions.push_back({"margin-bottom-expression-other-comp","Other plane",{},
        {{"margin-bottom-expression-other","Other frame",0,0,100,100}}});
    const auto composition="margin-bottom-expression-comp";
    const Ref target{"margin-bottom-expression-target","","margin.bottom"};
    const Ref source{"margin-bottom-expression-source","","artboard.height"};
    const Ref alternate{"margin-bottom-expression-alternate","","artboard.height"};
    const Ref upstream{"margin-bottom-expression-upstream","","artboard.height"};
    const Expression expression{R"(ref("margin-bottom-expression-source","","artboard.height") + 10)",1};
    Artboard source_board{"margin-bottom-expression-source","Source",0,0,100,50};
    source_board.parent_size=ArtboardParent{"margin-bottom-expression-upstream",false,true};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},
        AddArtboard{composition,{"margin-bottom-expression-alternate","Alternate",0,0,100,55},2},
        AddArtboard{composition,{"margin-bottom-expression-upstream","Upstream",0,0,50,50},3}});
    const auto linked=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_margin_bottom_expression","target":{"object":"margin-bottom-expression-target","point":"","field":"margin.bottom"},
       "expression":{"source":"ref(\"margin-bottom-expression-source\",\"\",\"artboard.height\") + 10","version":1},"replace_driver":false}
    ]})");
    check(linked.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session command authors a revisioned Margin bottom expression");
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({MarginBottomCommand{SetMarginBottomExpression{target,expression,false}}});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the same Margin bottom expression is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&!typed.driver&&typed.expression==expression&&typed.source_kind=="expression"&&
        std::get<double>(typed.evaluated)==60&&
        evaluate_artboard(session.document().compositions.front(),target.object).layout->margin->bottom==60,
        "Pure Artboard evaluation and typed Margin bottom read preserve the authored literal and exact du expression");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"margin-bottom-expression-target","point":"","field":"margin.bottom"}})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("margin-bottom-expression-source")!=std::string::npos&&
        typed_json.find("\"unit\":\"du\"")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos,
        ("Typed Margin bottom get exposes exact expression, unit and evaluated du: "+typed_json).c_str());
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(properties_json.find("margin-bottom-expression-target")!=std::string::npos&&
        properties_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        "Properties enumeration returns the same typed Margin bottom expression");

    rejects("DRIVEN_MARGIN_BOTTOM",[&]{apply({MarginBottomCommand{LinkMarginBottom{target,alternate,false}}});});
    apply({MarginBottomCommand{LinkMarginBottom{target,alternate,true}}});
    rejects("DRIVEN_MARGIN_BOTTOM",[&]{apply({MarginBottomCommand{SetMarginBottomExpression{target,expression,false}}});});
    apply({MarginBottomCommand{SetMarginBottomExpression{target,expression,true}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&!typed.driver&&std::get<double>(typed.literal)==40&&
        std::get<double>(typed.evaluated)==60,
        "Link and expression replacement require explicit authorization and retain the authored bottom inset");

    auto upstream_board=find_board(upstream.object);upstream_board.height=60;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70,
        "Inherited upstream Artboard height reevaluates Margin bottom without changing its exact source or literal");
    auto sibling_layout=*find_board(target.object).layout;sibling_layout.margin->top=45;sibling_layout.margin->right=45;
    sibling_layout.grid->bounds.y=45;apply({SetArtboardLayout{composition,target.object,sibling_layout}});
    auto moved_target=find_board(target.object);moved_target.name="Renamed target";moved_target.x=120;
    auto moved_source=find_board(source.object);moved_source.name="Renamed source";moved_source.x=60;
    apply({UpdateArtboard{composition,moved_target},UpdateArtboard{composition,moved_source},
        ReorderArtboards{composition,{alternate.object,target.object,source.object,upstream.object}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70&&
        find_board(target.object).layout->margin->top==45&&find_board(target.object).layout->grid->id=="margin-bottom-expression-grid"&&
        find_board(target.object).x==120&&find_board(source.object).name=="Renamed source"&&find_board(source.object).x==60,
        "Full-layout edits, rename, move and reorder preserve Margin bottom expression identity and evaluation");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    const auto invalid=[&](const char* code,std::vector<Command> commands,const char* why) {
        rejects(code,[&]{apply(std::move(commands));});
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,why);
    };
    invalid("INVALID_LAYOUT_REF",{MarginBottomCommand{SetMarginBottomExpression{{target.object,"p","margin.bottom"},expression,true}}},
        "Invalid bottom target identity leaves expression bytes, revision and history unchanged");
    invalid("MARGIN_BOTTOM_EXPRESSION_TYPE",{MarginBottomCommand{SetMarginBottomExpression{target,
        {R"(ref("margin-bottom-expression-source","point","artboard.height"))",1},true}}},
        "A point-qualified Artboard Ref is rejected atomically for Margin bottom");
    invalid("MARGIN_BOTTOM_EXPRESSION_TYPE",{MarginBottomCommand{SetMarginBottomExpression{target,
        {R"(ref("margin-bottom-expression-grid","","grid.bounds.y"))",1},true}}},
        "A wrong-kind Grid property Ref is rejected atomically for Margin bottom");
    invalid("ARTBOARD_SELF_LINK",{MarginBottomCommand{SetMarginBottomExpression{target,
        {R"(ref("margin-bottom-expression-target","","artboard.height"))",1},true}}},
        "A self-referential Margin bottom expression is rejected atomically");
    invalid("WRONG_COMPOSITION",{MarginBottomCommand{SetMarginBottomExpression{target,
        {R"(ref("margin-bottom-expression-other","","artboard.height"))",1},true}}},
        "A cross-Composition Margin bottom expression is rejected atomically");
    invalid("MISSING_ARTBOARD",{MarginBottomCommand{SetMarginBottomExpression{target,
        {R"(ref("missing-margin-bottom-source","","artboard.height"))",1},true}}},
        "A missing Artboard source is rejected atomically");
    invalid("UNIT_MISMATCH",{MarginBottomCommand{SetMarginBottomExpression{target,
        {R"(ref("margin-bottom-expression-source","","artboard.height") + ref("object","","generator.rotation"))",1},true}}},
        "A non-du expression is rejected atomically for Margin bottom");
    invalid("EXPRESSION_SYNTAX",{MarginBottomCommand{SetMarginBottomExpression{target,{"ref(",1},true}}},
        "A malformed Margin bottom expression is rejected atomically");
    invalid("UNSUPPORTED_EXPRESSION_VERSION",{MarginBottomCommand{SetMarginBottomExpression{target,{"1",2},true}}},
        "An unsupported Margin bottom expression version is rejected atomically");
    invalid("EXPRESSION_DOMAIN",{MarginBottomCommand{SetMarginBottomExpression{target,{"1 / 0",1},true}}},
        "A domain-invalid Margin bottom expression is rejected atomically");
    invalid("NON_FINITE",{MarginBottomCommand{SetMarginBottomExpression{target,{"1e308 * 10",1},true}}},
        "A non-finite Margin bottom expression is rejected atomically");
    invalid("INVALID_LAYOUT",{MarginBottomCommand{SetMarginBottomExpression{target,{"-1",1},true}}},
        "A negative evaluated Margin bottom is rejected atomically");
    invalid("INVALID_LAYOUT",{MarginBottomCommand{SetMarginBottomExpression{target,
        {R"(ref("margin-bottom-expression-source","","artboard.height") + 600)",1},true}}},
        "An expression that closes vertical content is rejected atomically");
    invalid("DRIVEN_MARGIN_BOTTOM",{MarginBottomCommand{SetMarginBottomExpression{target,
        {R"(ref("margin-bottom-expression-source","","artboard.height") + 11)",1},false}}},
        "Replacing a different bottom expression requires explicit authorization");
    invalid("DRIVEN_MARGIN_BOTTOM",{MarginBottomCommand{LinkMarginBottom{target,alternate,false}}},
        "Replacing a Margin bottom expression with a link requires explicit authorization");
    invalid("DRIVEN_MARGIN_BOTTOM",{SetArtboardLayout{composition,target.object,[&]{auto value=*find_board(target.object).layout;
        value.margin->bottom=41;return value;}()}},"A driven Margin bottom literal cannot be changed through a full-layout edit");
    invalid("DRIVEN_MARGIN_BOTTOM",{SetArtboardLayout{composition,target.object,[&]{auto value=*find_board(target.object).layout;
        value.margin.reset();return value;}()}},"A driven Margin bottom cannot be cleared through a full-layout edit");
    invalid("MARGIN_DRIVER_SMUGGLING",{[&]{auto value=*find_board(target.object).layout;
        value.margin->bottom_expression=Expression{"5",1};return Command{SetArtboardLayout{composition,target.object,value}};}()},
        "A full-layout payload cannot replace a Margin bottom expression source");
    auto smuggled=Artboard{"margin-bottom-expression-smuggled","Smuggled",0,0,100,100};
    ArtboardLayout smuggled_layout;smuggled_layout.margin=Margin{10,10,10,10};
    smuggled_layout.margin->bottom_expression=expression;smuggled.layout=smuggled_layout;
    invalid("MARGIN_DRIVER_SMUGGLING",{AddArtboard{composition,smuggled,4}},
        "AddArtboard cannot inject a Margin bottom expression source");
    invalid("DUPLICATE_TARGET",{MarginBottomCommand{SetMarginBottomExpression{target,expression,true}},
        MarginBottomCommand{UnlinkMarginBottom{target}}},"A batch cannot change a Margin bottom target twice");
    invalid("MISSING_REFERENCE",{MarginBottomCommand{SetMarginBottomExpression{target,expression,true}},Set{target,1}},
        "A failed second batch command leaves no first-command Margin bottom expression commit");
    invalid("ARTBOARD_IN_USE",{DeleteArtboard{composition,source.object}},
        "A source Artboard cannot be deleted while a Margin bottom expression references it");
    auto invalid_upstream=find_board("margin-bottom-expression-upstream");invalid_upstream.height=1000;
    invalid("INVALID_LAYOUT",{UpdateArtboard{composition,invalid_upstream}},
        "An upstream size edit that closes target content height is rejected atomically");
    auto too_short=find_board(target.object);too_short.height=100;
    invalid("INVALID_LAYOUT",{UpdateArtboard{composition,too_short}},
        "A target height edit that closes Margin top plus evaluated bottom is rejected atomically");
    rejects("REVISION_CONFLICT",[&]{session.apply({MarginBottomCommand{UnlinkMarginBottom{target}}},stable_revision-1);});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "Stale Margin bottom expression commands preserve exact Session state");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bottom_expression\":{\"source\":\"ref(\\\"margin-bottom-expression-source\\\",\\\"\\\",\\\"artboard.height\\\") + 10\",\"version\":1}")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.60 preserves exact Margin bottom expression and byte-roundtrips");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.45\"")==1,
        "Margin bottom expression version-lie fixture changes only the writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"bottom_expression\":{","\"bottom_expression\":{\"extra\":1,")==1,
        "Malformed Margin bottom expression fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto conflicting=native;
    check(replace_all(conflicting,"\"bottom_expression\":{",
        "\"bottom_driver\":{\"link\":{\"object\":\"margin-bottom-expression-source\",\"point\":\"\",\"field\":\"artboard.height\"}},\"bottom_expression\":{")==1,
        "Conflicting Margin bottom fixture adds a link beside its expression");
    rejects("MARGIN_SOURCE_CONFLICT",[&]{(void)decode(conflicting);});
    auto legacy_document=session.document();
    std::find_if(legacy_document.compositions.front().artboards.begin(),legacy_document.compositions.front().artboards.end(),
        [&](const Artboard& value){return value.id==target.object;})->layout->margin->bottom_expression.reset();
    auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.45\"")==1&&
        decode(legacy)==legacy_document,"Native 0.45 remains readable when Margin bottom expression is absent");

    apply({MarginBottomCommand{UnlinkMarginBottom{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==70,"Unlink freezes evaluated Margin bottom and removes its expression in one step");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70,
        "Undo restores exact Margin bottom expression and retained authored literal");
    session.redo(session.revision());upstream_board=find_board(upstream.object);upstream_board.height=80;
    apply({UpdateArtboard{composition,upstream_board}});typed=artboard_layout_property(session.document(),target);
    check(!typed.driver&&!typed.expression&&std::get<double>(typed.literal)==70&&std::get<double>(typed.evaluated)==70,
        "Redo keeps frozen Margin bottom independent of later Artboard-size changes");
}

void margin_left_expression() {
    auto document=empty_document("margin-expression-doc","margin-expression-comp","margin-expression-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    document.compositions.push_back({"margin-expression-other-comp","Other plane",{},
        {{"margin-expression-other","Other frame",0,0,400,300}}});
    const auto composition="margin-expression-comp";
    const Ref target{"margin-expression-target","","margin.left"};
    const Ref source_a{"margin-expression-source-a","","artboard.width"};
    const Ref source_b{"margin-expression-source-b","","artboard.height"};
    const Ref grid_ref{"margin-expression-grid","","grid.bounds.x"};
    const Expression expression{R"(ref("margin-expression-source-a","","artboard.width") + ref("margin-expression-source-b","","artboard.height"))",1};
    Artboard source_a_board{"margin-expression-source-a","Source A",0,0,20,100};
    // Parent records store the stable source Artboard ID followed by inherited dimensions.
    source_a_board.parent_size=ArtboardParent{"margin-expression-upstream",true,false};
    ArtboardLayout layout;layout.margin=Margin{40,20,40,20};
    layout.grid=Grid{"margin-expression-grid",{40,20,880,600},2,1,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto target_artboard=[&]() -> const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),
            [&](const Artboard& value){return value.id==target.object;});
    };
    apply({AddArtboard{composition,source_a_board,1},
        AddArtboard{composition,{"margin-expression-source-b","Source B",0,0,100,30},2},
        AddArtboard{composition,{"margin-expression-upstream","Upstream",0,0,20,100},3},
        SetArtboardLayout{composition,target.object,layout}});
    const auto expression_command=[&](Expression value,bool replace=false) {
        return MarginLeftCommand{SetMarginLeftExpression{target,std::move(value),replace}};
    };
    const auto applied=request(session,R"json({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_margin_left_expression","target":{"object":"margin-expression-target","point":"","field":"margin.left"},
       "expression":{"source":"ref(\"margin-expression-source-a\",\"\",\"artboard.width\") + ref(\"margin-expression-source-b\",\"\",\"artboard.height\")","version":1},"replace_driver":false}
    ]})json");
    check(applied.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session command authors a revisioned Margin left expression");
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({expression_command(expression)});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the same Margin expression is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&!typed.driver&&typed.expression==expression&&
        typed.source_kind=="expression"&&std::get<double>(typed.evaluated)==50&&
        evaluate_artboard(session.document().compositions.front(),target.object).layout->margin->left==50,
        "Pure Artboard evaluation and typed Margin read retain the literal and exact expression while evaluating du");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"margin-expression-target","point":"","field":"margin.left"}})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("margin-expression-source-a")!=std::string::npos&&
        typed_json.find("\"unit\":\"du\"")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos,
        ("Typed Margin get exposes Artboard identity, exact source, unit and expression support: "+typed_json).c_str());
    const auto props_json=request(session,R"({"op":"properties"})");
    check(props_json.find("margin-expression-target")!=std::string::npos&&
        props_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        "Properties enumeration returns the same typed Margin left expression");

    apply({expression_command({"25",1},true)});
    check(std::get<double>(artboard_layout_property(session.document(),target).evaluated)==25,
        "Constant-only Margin expressions are allowed and evaluate in du");
    apply({expression_command(expression,true)});
    rejects("DRIVEN_MARGIN_LEFT",[&]{apply({MarginLeftCommand{LinkMarginLeft{target,source_a,false}}});});
    apply({MarginLeftCommand{LinkMarginLeft{target,source_a,true}}});
    rejects("DRIVEN_MARGIN_LEFT",[&]{apply({expression_command(expression)});});
    apply({expression_command(expression,true)});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&!typed.driver&&std::get<double>(typed.literal)==40&&
        std::get<double>(typed.evaluated)==50,
        "Link and expression replacement require explicit authorization and retain the authored inset");

    auto upstream=session.document().compositions.front().artboards[3];upstream.width=30;
    apply({UpdateArtboard{composition,upstream}});
    auto source_b_changed=session.document().compositions.front().artboards[2];source_b_changed.height=40;
    apply({UpdateArtboard{composition,source_b_changed}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.expression==expression&&std::get<double>(typed.evaluated)==70,
        "Inherited upstream width and independent height edits drive 60 then 70 without changing literal or source text");

    auto sibling_layout=*target_artboard().layout;sibling_layout.margin->top=25;sibling_layout.margin->right=45;
    sibling_layout.margin->bottom=30;sibling_layout.grid->bounds.x=45;
    apply({SetArtboardLayout{composition,target.object,sibling_layout}});
    auto moved_target=target_artboard();moved_target.name="Renamed target";moved_target.x=120;
    apply({UpdateArtboard{composition,moved_target},ReorderArtboards{composition,
        {"margin-expression-source-b",target.object,"margin-expression-source-a","margin-expression-upstream"}}});
    auto moved_source=session.document().compositions.front().artboards[2];
    moved_source.name="Renamed source A";moved_source.x=90;
    apply({UpdateArtboard{composition,moved_source}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70&&
        target_artboard().layout->margin->top==25&&target_artboard().layout->grid->id==grid_ref.object&&
        target_artboard().layout->grid->bounds.x==45&&target_artboard().x==120,
        "Full-layout edits, sibling Grid changes, Artboard moves and reorder preserve the exact Margin expression");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    auto invalid=[&](Expression bad,const char* message) {
        bool did_reject=false;
        try {apply({expression_command(std::move(bad),true)});} catch(const Error&) {did_reject=true;}
        check(did_reject,message);
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
            "Rejected Margin expression leaves Document, native bytes, revision and history unchanged");
    };
    invalid({"ref(",1},"Malformed Margin expression is rejected");
    invalid({expression.source,0},"Unsupported Margin expression version is rejected");
    rejects("UNIT_MISMATCH",[&]{apply({expression_command({R"(ref("margin-expression-source-a","","artboard.width") + ref("object","","generator.rotation"))",1},true)});});
    rejects("MARGIN_LEFT_EXPRESSION_TYPE",[&]{apply({expression_command({R"(ref("margin-expression-source-a","point","artboard.width"))",1},true)});});
    rejects("MARGIN_LEFT_EXPRESSION_TYPE",[&]{apply({expression_command({R"(ref("margin-expression-grid","","grid.bounds.x"))",1},true)});});
    rejects("ARTBOARD_SELF_LINK",[&]{apply({expression_command({R"(ref("margin-expression-target","","artboard.width"))",1},true)});});
    rejects("WRONG_COMPOSITION",[&]{apply({expression_command({R"(ref("margin-expression-other","","artboard.width"))",1},true)});});
    rejects("MISSING_ARTBOARD",[&]{apply({expression_command({R"(ref("missing-margin-source","","artboard.width"))",1},true)});});
    rejects("INVALID_LAYOUT",[&]{apply({expression_command({"-1",1},true)});});
    rejects("DRIVEN_MARGIN_LEFT",[&]{auto payload=*target_artboard().layout;payload.margin->left=71;
        apply({SetArtboardLayout{composition,target.object,payload}});});
    rejects("DRIVEN_MARGIN_LEFT",[&]{auto payload=*target_artboard().layout;payload.margin.reset();
        apply({SetArtboardLayout{composition,target.object,payload}});});
    rejects("DRIVEN_MARGIN_LEFT",[&]{auto payload=target_artboard();payload.layout->margin->left=71;
        apply({UpdateArtboard{composition,payload}});});
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{auto payload=target_artboard();payload.layout->margin->left_expression=Expression{"5",1};
        apply({UpdateArtboard{composition,payload}});});
    auto smuggled=Artboard{"margin-expression-smuggled","Smuggled",0,0,100,100};
    ArtboardLayout smuggled_layout;smuggled_layout.margin=Margin{10,0,0,0};
    smuggled_layout.margin->left_expression=expression;smuggled.layout=smuggled_layout;
    rejects("MARGIN_DRIVER_SMUGGLING",[&]{apply({AddArtboard{composition,smuggled,4}});});
    rejects("DUPLICATE_TARGET",[&]{apply({expression_command(expression,true),MarginLeftCommand{UnlinkMarginLeft{target}}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({expression_command(expression,true)},stable_revision-1);});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"margin-expression-source-a"}});});
    const auto source_a_position=std::find_if(session.document().compositions.front().artboards.begin(),
        session.document().compositions.front().artboards.end(),[](const Artboard& value){return value.id=="margin-expression-upstream";});
    auto invalid_upstream=*source_a_position;invalid_upstream.width=920;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,invalid_upstream}});});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==70,
        "Out-of-range expression and source edits leave the last valid evaluation and Session state intact");
    rejects("ARTBOARD_CYCLE",[&]{apply({LinkArtboardSize{source_a,{"margin-expression-source-b","","artboard.width"},true},
        LinkArtboardSize{{"margin-expression-source-b","","artboard.width"},source_a,false}});});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "A cyclic Artboard-size source graph rejects atomically while Margin depends on it");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"left_expression\"")!=std::string::npos&&native.find("margin-expression-source-a")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.43 preserves the exact Margin expression beside its authored literal");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.37\"")==1,
        "Margin expression version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    apply({MarginLeftCommand{UnlinkMarginLeft{target}}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==70,"Unlink freezes the evaluated Margin inset and clears the expression in one step");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70,
        "Undo restores the exact Margin expression and original literal");
    session.redo(session.revision());upstream=session.document().compositions.front().artboards[3];upstream.width=80;
    apply({UpdateArtboard{composition,upstream}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.expression&&std::get<double>(typed.evaluated)==70,
        "Redo keeps the frozen Margin inset independent from later source changes");
}

void grid_bounds_x_artboard_driver() {
    auto document=empty_document("grid-x-doc","grid-x-comp","grid-x-target");
    document.compositions.front().artboards.front().width=960;
    document.compositions.front().artboards.front().height=640;
    document.compositions.push_back({"grid-x-other-comp","Other plane",{},{{"grid-x-other","Other frame",0,0,400,300}}});
    const auto composition="grid-x-comp";
    const Ref target{"grid-x-target-grid","","grid.bounds.x"};
    const Ref source{"grid-x-source","","artboard.width"};
    const Ref upstream{"grid-x-upstream","","artboard.width"};
    Artboard source_board{"grid-x-source","Source frame",0,0,40,100};
    source_board.parent_size=ArtboardParent{"grid-x-upstream",true,false};
    Artboard upstream_board{"grid-x-upstream","Upstream frame",0,0,40,100};
    ArtboardLayout layout;layout.grid=Grid{"grid-x-target-grid",{40,20,880,600},2,1,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_target=[&]() -> const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),
            [&](const Artboard& value){return value.id=="grid-x-target";});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,upstream_board,2},
        SetArtboardLayout{composition,"grid-x-target",layout}});

    auto no_grid_document=empty_document("no-grid-doc","no-grid-comp","no-grid-target");
    Session no_grid_session(no_grid_document);
    no_grid_session.apply({AddArtboard{"no-grid-comp",Artboard{"no-grid-source","Source",0,0,40,100},1}},0);
    rejects("MISSING_GRID",[&]{no_grid_session.apply({GridBoundsXCommand{
        LinkGridBoundsX{{"no-grid-target-grid","","grid.bounds.x"},{"no-grid-source","","artboard.width"},false}}},
        no_grid_session.revision());});

    auto smuggled=layout;smuggled.grid->bounds_x_driver=source;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-x-target",smuggled}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{
        auto board=Artboard{"grid-x-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected;injected.grid=Grid{"grid-x-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected.grid->bounds_x_driver=source;board.layout=injected;
        apply({AddArtboard{composition,board,3}});
    });

    auto json_link=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_grid_bounds_x","target":{"object":"grid-x-target-grid","point":"","field":"grid.bounds.x"},
       "source":{"object":"grid-x-source","point":"","field":"artboard.width"},"replace_driver":false}
    ]})");
    check(json_link.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session command links stable Grid bounds x through its dedicated revisioned command");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==40&&
        evaluate_artboard(session.document().compositions.front(),"grid-x-target").layout->grid->bounds.x==40,
        "Typed Grid x read and pure evaluation separate authored literal, exact Artboard Ref and evaluated offset");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-x-target-grid","point":"","field":"grid.bounds.x"}})");
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("\"field\":\"artboard.width\"")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos,
        ("Typed JSON Grid x reports the literal, source Ref and link status: "+typed_json).c_str());

    apply({GridBoundsXCommand{LinkGridBoundsX{target,source,false}}});
    check(artboard_layout_property(session.document(),target).driver==source,
        "Repeating the exact Grid x source does not require replacement authorization");
    const Ref upstream_ref{"grid-x-upstream","","artboard.width"};
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({GridBoundsXCommand{LinkGridBoundsX{target,upstream_ref,false}}});});
    apply({GridBoundsXCommand{LinkGridBoundsX{target,upstream_ref,true}}});
    check(artboard_layout_property(session.document(),target).driver==upstream_ref,
        "Explicit replacement changes the Grid x source through the dedicated command");
    apply({GridBoundsXCommand{LinkGridBoundsX{target,source,true}}});

    auto upstream_changed=session.document().compositions.front().artboards[2];upstream_changed.width=60;
    apply({UpdateArtboard{composition,upstream_changed}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==60&&
        evaluate_artboard(session.document().compositions.front(),"grid-x-target").layout->grid->bounds.x==60&&
        session.document().compositions.front().artboards.front().layout->grid->bounds==LayoutRect{40,20,880,600},
        "Parent-driven Artboard width changes evaluated Grid x while preserving the literal and all other Grid bounds");

    auto edited=*find_target().layout;edited.grid->bounds.width=870;
    apply({SetArtboardLayout{composition,"grid-x-target",edited}});
    auto target_board=find_target();target_board.name="Renamed target";target_board.x=120;
    apply({UpdateArtboard{composition,target_board},ReorderArtboards{composition,
        {"grid-x-upstream","grid-x-source","grid-x-target"}}});
    auto renamed_source=session.document().compositions.front().artboards[1];
    renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed_source}});
    upstream_changed=session.document().compositions.front().artboards[0];upstream_changed.width=70;
    apply({UpdateArtboard{composition,upstream_changed}});
    const auto& reordered=session.document().compositions.front();typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==70&&
        reordered.artboards[2].layout->grid->id==target.object&&reordered.artboards[2].layout->grid->bounds.width==870&&
        reordered.artboards[2].x==120&&reordered.artboards[1].x==50&&
        evaluate_artboard(reordered,"grid-x-target").layout->grid->bounds.x==70,
        "Rename, move, reorder and full-layout sibling edits preserve stable Grid/source identity and Artboard-local x");

    const auto bytes_before=encode(session.document());const auto rev_before=session.revision();const auto history_before=session.history();
    auto direct=*find_target().layout;direct.grid->bounds.x=71;
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({SetArtboardLayout{composition,"grid-x-target",direct}});});
    direct=*find_target().layout;direct.grid->id="grid-x-replaced";
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({SetArtboardLayout{composition,"grid-x-target",direct}});});
    auto cleared=*find_target().layout;cleared.grid.reset();
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({SetArtboardLayout{composition,"grid-x-target",cleared}});});
    auto direct_update=find_target();direct_update.layout->grid->bounds.x=71;
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({UpdateArtboard{composition,direct_update}});});
    direct_update=find_target();direct_update.layout->grid.reset();
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({UpdateArtboard{composition,direct_update}});});
    rejects("DUPLICATE_TARGET",[&]{apply({GridBoundsXCommand{UnlinkGridBoundsX{target}},
        GridBoundsXCommand{LinkGridBoundsX{target,source,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridBoundsXCommand{LinkGridBoundsX{{target.object,"point","grid.bounds.x"},source,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridBoundsXCommand{LinkGridBoundsX{{target.object,"","grid.bounds.y"},source,true}}});});
    rejects("GRID_SELF_LINK",[&]{apply({GridBoundsXCommand{LinkGridBoundsX{target,
        {"grid-x-target","","artboard.width"},true}}});});
    rejects("WRONG_COMPOSITION",[&]{apply({GridBoundsXCommand{LinkGridBoundsX{target,
        {"grid-x-other","","artboard.width"},true}}});});
    rejects("MISSING_ARTBOARD",[&]{apply({GridBoundsXCommand{LinkGridBoundsX{target,
        {"grid-x-missing-source","","artboard.width"},true}}});});
    rejects("MISSING_GRID",[&]{apply({GridBoundsXCommand{UnlinkGridBoundsX{{"grid-x-missing-grid","","grid.bounds.x"}}}});});
    rejects("MISSING_REFERENCE",[&]{apply({Set{target,10}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{target,Binding{source,1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{target},{"1",1},false}});});
    check(session.revision()==rev_before&&session.history()==history_before&&encode(session.document())==bytes_before&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==70,
        "Driven Grid x edits, clears, replacement and generic Scalar routes preserve Document, bytes, revision, history and evaluation");

    const auto batch_before=encode(session.document());const auto batch_history=session.history();const auto batch_revision=session.revision();
    rejects("INVALID_GUIDE",[&]{session.apply({GridBoundsXCommand{UnlinkGridBoundsX{target}},
        AddGuide{composition,{"grid-x-bad-guide","Bad axis","z",0}}},session.revision());});
    check(session.revision()==batch_revision&&session.history()==batch_history&&encode(session.document())==batch_before,
        "A failing second batch command leaves the Grid driver, bytes, revision and Undo history unchanged");
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-x-source"}});});
    upstream_changed=session.document().compositions.front().artboards.front();upstream_changed.width=100;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream_changed}});});
    check(session.revision()==batch_revision&&session.history()==batch_history&&encode(session.document())==batch_before&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==70,
        "Invalid upstream width leaves the prior valid evaluated Grid x and all Session state intact");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bounds_x_driver\"")!=std::string::npos&&native.find("grid-x-source")!=std::string::npos&&
        encode(decode(native))==native,
        "Native 0.43 stores the closed Grid source beside its authored literal and roundtrips bytes");
    auto native_036=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(native_036,"\"version\":\"0.66\"","\"version\":\"0.36\"")==1&&
        decode(native_036)==session.document(),
        "Native 0.36 remains readable with its existing Grid x link and no expression field");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.35\"")==1,
        "Grid version-lie fixture changes only the native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto literal_document=empty_document("grid-x-legacy-doc","grid-x-legacy-comp","grid-x-legacy-art");
    ArtboardLayout literal_layout;literal_layout.grid=Grid{"grid-x-legacy-grid",{10,10,500,400},1,1,0,0};
    Session literal_session(literal_document);literal_session.apply({SetArtboardLayout{"grid-x-legacy-comp",
        "grid-x-legacy-art",literal_layout}},0);
    auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(literal_session.document()));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.35\"")==1&&
        decode(legacy)==literal_session.document(),"Native 0.35 literal-only Grid remains readable by the current decoder");

    apply({GridBoundsXCommand{UnlinkGridBoundsX{target}}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&std::get<double>(typed.evaluated)==70,
        "Unlink freezes evaluated Grid x into the authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==70,
        "Undo restores exact Grid source and original literal");
    session.redo(session.revision());
    upstream_changed=session.document().compositions.front().artboards.front();upstream_changed.width=80;
    apply({UpdateArtboard{composition,upstream_changed}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&std::get<double>(typed.evaluated)==70,
        "Redo keeps frozen Grid x independent from later source changes");
}

void grid_bounds_y_artboard_driver() {
    auto document=empty_document("grid-y-doc","grid-y-comp","grid-y-target");
    auto& target_board=document.compositions.front().artboards.front();
    target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-y-other-comp","Other plane",{},{{"grid-y-other","Other frame",0,0,400,300}}});
    const auto composition="grid-y-comp";
    const Ref target{"grid-y-target-grid","","grid.bounds.y"};
    const Ref source{"grid-y-source","","artboard.width"};
    const Ref height_source{"grid-y-height-source","","artboard.height"};
    const Ref upstream{"grid-y-upstream","","artboard.width"};
    Artboard source_board{"grid-y-source","Source frame",0,0,50,100};
    source_board.parent_size=ArtboardParent{"grid-y-upstream",true,false};
    Artboard height_board{"grid-y-height-source","Height source",0,0,100,60};
    Artboard upstream_board{"grid-y-upstream","Upstream frame",0,0,50,100};
    ArtboardLayout layout;layout.grid=Grid{"grid-y-target-grid",{40,40,880,500},2,1,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,height_board,2},
        AddArtboard{composition,upstream_board,3},SetArtboardLayout{composition,"grid-y-target",layout}});

    auto no_grid_document=empty_document("no-grid-y-doc","no-grid-y-comp","no-grid-y-target");
    Session no_grid_session(no_grid_document);
    no_grid_session.apply({AddArtboard{"no-grid-y-comp",Artboard{"no-grid-y-source","Source",0,0,40,100},1}},0);
    rejects("MISSING_GRID",[&]{no_grid_session.apply({GridBoundsYCommand{
        LinkGridBoundsY{{"no-grid-y-target-grid","","grid.bounds.y"},{"no-grid-y-source","","artboard.width"},false}}},
        no_grid_session.revision());});
    auto smuggled=layout;smuggled.grid->bounds_y_driver=source;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-y-target",smuggled}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{
        auto board=Artboard{"grid-y-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected;injected.grid=Grid{"grid-y-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected.grid->bounds_y_driver=source;board.layout=injected;
        apply({AddArtboard{composition,board,4}});
    });

    const auto json_link=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_grid_bounds_y","target":{"object":"grid-y-target-grid","point":"","field":"grid.bounds.y"},
       "source":{"object":"grid-y-source","point":"","field":"artboard.width"},"replace_driver":false}
    ]})");
    check(json_link.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session links Grid y through its dedicated revisioned Artboard-size command");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&typed.source_kind=="link"&&
        std::get<double>(typed.evaluated)==50&&
        evaluate_artboard(session.document().compositions.front(),"grid-y-target").layout->grid->bounds.y==50,
        "Grid y evaluates an Artboard parent chain while retaining the authored literal and exact stable source Ref");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-y-target-grid","point":"","field":"grid.bounds.y"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    const std::string evaluated_marker="\"evaluated\":";
    const auto typed_evaluated=typed_json.find(evaluated_marker);
    const auto grid_property=properties_json.find("\"field\":\"grid.bounds.y\"");
    const auto listed_evaluated=properties_json.find(evaluated_marker,grid_property);
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("\"field\":\"artboard.width\"")!=std::string::npos&&
        typed_evaluated!=std::string::npos&&std::stod(typed_json.substr(typed_evaluated+evaluated_marker.size()))==50&&
        typed_json.find("\"link\":true")!=std::string::npos&&
        grid_property!=std::string::npos&&listed_evaluated!=std::string::npos&&
        std::stod(properties_json.substr(listed_evaluated+evaluated_marker.size()))==50,
        "Typed get and properties expose Grid y literal, source, evaluated du and stable Grid ID");
    const auto revision=session.revision();
    apply({GridBoundsYCommand{LinkGridBoundsY{target,source,false}}});
    check(session.revision()==revision,"Repeating the exact Grid y source is idempotent");
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({GridBoundsYCommand{LinkGridBoundsY{target,height_source,false}}});});
    apply({GridBoundsYCommand{LinkGridBoundsY{target,height_source,true}}});
    check(artboard_layout_property(session.document(),target).driver==height_source&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "Explicit replacement changes Grid y to a distinct Artboard height source");
    apply({GridBoundsYCommand{LinkGridBoundsY{target,source,true}}});

    auto edited=*find_board("grid-y-target").layout;edited.grid->bounds.x=60;edited.grid->bounds.width=870;
    apply({SetArtboardLayout{composition,"grid-y-target",edited}});
    auto target_update=find_board("grid-y-target");target_update.name="Moved target";target_update.x=120;
    auto renamed_source=find_board("grid-y-source");renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,target_update},UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"grid-y-upstream","grid-y-height-source","grid-y-source","grid-y-target"}}});
    auto upstream_update=find_board("grid-y-upstream");upstream_update.width=60;
    apply({UpdateArtboard{composition,upstream_update}});
    typed=artboard_layout_property(session.document(),target);
    const auto& moved_comp=session.document().compositions.front();
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==60&&
        evaluate_artboard(moved_comp,"grid-y-target").layout->grid->bounds.y==60&&
        find_board("grid-y-target").layout->grid->id==target.object&&find_board("grid-y-target").layout->grid->bounds.width==870&&
        find_board("grid-y-target").x==120&&find_board("grid-y-source").name=="Renamed source"&&
        find_board("grid-y-source").x==50,
        "Upstream resize and full-layout sibling edits preserve evaluated y, authored literal and stable IDs across rename, move and reorder");

    const auto bytes_before=encode(session.document());const auto rev_before=session.revision();const auto history_before=session.history();
    auto direct=*find_board("grid-y-target").layout;direct.grid->bounds.y=41;
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({SetArtboardLayout{composition,"grid-y-target",direct}});});
    direct=*find_board("grid-y-target").layout;direct.grid->id="grid-y-replaced";
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({SetArtboardLayout{composition,"grid-y-target",direct}});});
    direct=*find_board("grid-y-target").layout;direct.grid.reset();
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({SetArtboardLayout{composition,"grid-y-target",direct}});});
    auto direct_update=find_board("grid-y-target");direct_update.layout->grid->bounds.y=41;
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({UpdateArtboard{composition,direct_update}});});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{"grid-y-source","point","artboard.width"},"INVALID_ARTBOARD_REF"},
            {{"grid-y-source","","grid.bounds.y"},"INVALID_ARTBOARD_REF"},
            {{"grid-y-target","","artboard.width"},"GRID_SELF_LINK"},
            {{"grid-y-comp","","artboard.width"},"TYPE_MISMATCH"},
            {{"grid-y-other","","artboard.width"},"WRONG_COMPOSITION"},
            {{"grid-y-missing","","artboard.width"},"MISSING_ARTBOARD"}}) {
        rejects(bad.second.c_str(),[&]{apply({GridBoundsYCommand{LinkGridBoundsY{target,bad.first,true}}});});
    }
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridBoundsYCommand{LinkGridBoundsY{{target.object,"point","grid.bounds.y"},source,true}}});});
    rejects("MISSING_GRID",[&]{apply({GridBoundsYCommand{UnlinkGridBoundsY{{"grid-y-missing-grid","","grid.bounds.y"}}}});});
    rejects("DUPLICATE_TARGET",[&]{apply({GridBoundsYCommand{UnlinkGridBoundsY{target}},
        GridBoundsYCommand{LinkGridBoundsY{target,source,true}}});});
    const auto stale=session.revision();
    rejects("REVISION_CONFLICT",[&]{session.apply({GridBoundsYCommand{UnlinkGridBoundsY{target}}},stale-1);});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-y-source"}});});
    const auto batch_revision=session.revision();const auto batch_bytes=encode(session.document());const auto batch_history=session.history();
    rejects("INVALID_GUIDE",[&]{apply({GridBoundsYCommand{UnlinkGridBoundsY{target}},
        AddGuide{composition,{"grid-y-bad-guide","Bad axis","z",0}}});});
    check(session.revision()==batch_revision&&session.history()==batch_history&&encode(session.document())==batch_bytes&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "A stale command or failing second batch command cannot partially unlink the validated Grid y source");
    auto invalid_source=find_board("grid-y-upstream");invalid_source.width=141;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,invalid_source}});});
    target_update=find_board("grid-y-target");target_update.height=559;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,target_update}});});
    check(session.revision()==batch_revision&&session.history()==batch_history&&encode(session.document())==batch_bytes&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "Upstream y plus Grid height and target-height violations leave bytes, revision, history and prior evaluation unchanged");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bounds_y_driver\"")!=std::string::npos&&native.find("grid-y-source")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.43 preserves the exact optional Grid y link beside its authored literal");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.38\"")==1,
        "Grid y version-lie fixture changes only the writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"bounds_y_driver\":{","\"bounds_y_driver\":{\"extra\":1,")==1,
        "Malformed Grid y driver fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto legacy_document=session.document();
    auto& legacy_grid=*std::find_if(legacy_document.compositions.front().artboards.begin(),legacy_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-y-target";})->layout->grid;
    legacy_grid.bounds_y_driver.reset();
    auto native_038=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(native_038,"\"version\":\"0.66\"","\"version\":\"0.38\"")==1&&decode(native_038)==legacy_document,
        "Native 0.38 remains readable when the optional Grid y link is absent");

    apply({GridBoundsYCommand{UnlinkGridBoundsY{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==60&&!typed.driver&&std::get<double>(typed.evaluated)==60,
        "Unlink freezes evaluated Grid y into its authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.driver==source&&std::get<double>(typed.evaluated)==60,
        "Undo restores the exact Grid y source and original literal");
    session.redo(session.revision());upstream_update=find_board("grid-y-upstream");upstream_update.width=80;
    apply({UpdateArtboard{composition,upstream_update}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==60&&!typed.driver&&std::get<double>(typed.evaluated)==60,
        "Redo leaves the frozen Grid y independent from later Artboard-size changes");
}

void grid_bounds_width_artboard_driver() {
    auto document=empty_document("grid-width-doc","grid-width-comp","grid-width-target");
    auto& target_board=document.compositions.front().artboards.front();
    target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-width-other-comp","Other plane",{},{{"grid-width-other","Other frame",0,0,400,300}}});
    const Id composition="grid-width-comp";
    const Ref target{"grid-width-target-grid","","grid.bounds.width"};
    const Ref source{"grid-width-source","","artboard.width"};
    const Ref height_source{"grid-width-height-source","","artboard.height"};
    const Ref upstream_ref{"grid-width-upstream","","artboard.width"};
    Artboard source_board{"grid-width-source","Source frame",0,0,700,100};
    source_board.parent_size=ArtboardParent{"grid-width-upstream",true,false};
    Artboard height_board{"grid-width-height-source","Height source",0,0,100,600};
    Artboard upstream_board{"grid-width-upstream","Upstream frame",0,0,700,100};
    ArtboardLayout layout;layout.grid=Grid{"grid-width-target-grid",{40,20,700,500},2,1,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,height_board,2},
        AddArtboard{composition,upstream_board,3},SetArtboardLayout{composition,"grid-width-target",layout}});

    auto no_grid_document=empty_document("no-grid-width-doc","no-grid-width-comp","no-grid-width-target");
    Session no_grid_session(no_grid_document);
    no_grid_session.apply({AddArtboard{"no-grid-width-comp",Artboard{"no-grid-width-source","Source",0,0,40,100},1}},0);
    rejects("MISSING_GRID",[&]{no_grid_session.apply({GridBoundsWidthCommand{LinkGridBoundsWidth{
        {"no-grid-width-target-grid","","grid.bounds.width"},{"no-grid-width-source","","artboard.width"},false}}},
        no_grid_session.revision());});
    auto smuggled=layout;smuggled.grid->bounds_width_driver=source;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-width-target",smuggled}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{
        auto board=Artboard{"grid-width-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected;injected.grid=Grid{"grid-width-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected.grid->bounds_width_driver=source;board.layout=injected;
        apply({AddArtboard{composition,board,4}});
    });

    const auto json_link=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_grid_bounds_width","target":{"object":"grid-width-target-grid","point":"","field":"grid.bounds.width"},
       "source":{"object":"grid-width-source","point":"","field":"artboard.width"},"replace_driver":false}
    ]})");
    check(json_link.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session links Grid width through its dedicated revisioned Artboard-size command");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==700&&typed.driver==source&&typed.source_kind=="link"&&
        !typed.expression&&std::get<double>(typed.evaluated)==700&&
        evaluate_artboard(session.document().compositions.front(),"grid-width-target").layout->grid->bounds.width==700,
        "Grid width evaluates an Artboard parent chain while retaining its authored literal and exact stable source Ref");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-width-target-grid","point":"","field":"grid.bounds.width"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("\"field\":\"artboard.width\"")!=std::string::npos&&
        typed_json.find("\"authored\":{\"literal\":")!=std::string::npos&&
        typed_json.find("\"evaluated\":")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"field\":\"grid.bounds.width\"")!=std::string::npos,
        "Typed get and properties expose Grid width literal, source, evaluated du and expression capability");
    const auto linked_revision=session.revision();
    apply({GridBoundsWidthCommand{LinkGridBoundsWidth{target,source,false}}});
    check(session.revision()==linked_revision,"Repeating the exact Grid width source is idempotent");
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{apply({GridBoundsWidthCommand{LinkGridBoundsWidth{target,height_source,false}}});});
    apply({GridBoundsWidthCommand{LinkGridBoundsWidth{target,height_source,true}}});
    check(artboard_layout_property(session.document(),target).driver==height_source&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==600,
        "Explicit replacement changes Grid width to a distinct Artboard height source");
    apply({GridBoundsWidthCommand{LinkGridBoundsWidth{target,source,true}}});

    auto upstream_update=find_board("grid-width-upstream");upstream_update.width=720;
    apply({UpdateArtboard{composition,upstream_update}});
    typed=artboard_layout_property(session.document(),target);
    const auto evaluated_grid=evaluate_artboard(session.document().compositions.front(),"grid-width-target");
    const double cell_width=(evaluated_grid.layout->grid->bounds.width-20.0)/2.0;
    check(std::get<double>(typed.literal)==700&&typed.driver==source&&std::get<double>(typed.evaluated)==720&&
        evaluated_grid.layout->grid->bounds.width==720&&cell_width==350,
        "Source resize changes evaluated Grid width and positive column cell width without changing the authored literal");

    Contour align_outline;align_outline.id="grid-width-align-contour";align_outline.closed=true;
    for(const auto& [x,y]:std::array<std::pair<double,double>,4>{{{0,0},{10,0},{10,10},{0,10}}}) {
        Point point;point.id="grid-width-align-point-"+std::to_string(align_outline.points.size());
        point.x.literal=x;point.y.literal=y;align_outline.points.push_back(point);
    }
    apply({CreatePath{composition,"","grid-width-align-shape","Align to linked Grid width",{align_outline}},
        AlignObjects{{"grid-width-align-shape"},"x","max",{},"grid:grid-width-target-grid"}});
    const auto aligned_values=evaluate(session.document());
    const auto aligned_bounds=object_bounds(session.document(),"grid-width-align-shape",aligned_values,
        evaluate_transforms(session.document(),aligned_values),true);
    check(aligned_bounds&&aligned_bounds->right==760,
        "Grid-reference horizontal Align consumes evaluated width 720, not authored literal 700");

    auto edited=*find_board("grid-width-target").layout;edited.grid->bounds.x=60;edited.grid->bounds.y=30;edited.grid->bounds.height=480;
    apply({SetArtboardLayout{composition,"grid-width-target",edited}});
    auto target_update=find_board("grid-width-target");target_update.name="Moved target";target_update.x=120;
    auto renamed_source=find_board("grid-width-source");renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,target_update},UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"grid-width-upstream","grid-width-height-source","grid-width-source","grid-width-target"}}});
    typed=artboard_layout_property(session.document(),target);
    const auto& moved_comp=session.document().compositions.front();
    check(std::get<double>(typed.literal)==700&&typed.driver==source&&std::get<double>(typed.evaluated)==720&&
        evaluate_artboard(moved_comp,"grid-width-target").layout->grid->bounds.width==720&&
        find_board("grid-width-target").layout->grid->id==target.object&&find_board("grid-width-target").layout->grid->bounds.x==60&&
        find_board("grid-width-target").x==120&&find_board("grid-width-source").name=="Renamed source"&&
        find_board("grid-width-source").x==50,
        "Sibling layout edits preserve evaluated width, authored literal and stable IDs across source rename, move and reorder");

    const auto bytes_before=encode(session.document());const auto rev_before=session.revision();const auto history_before=session.history();
    auto direct=*find_board("grid-width-target").layout;direct.grid->bounds.width=701;
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{apply({SetArtboardLayout{composition,"grid-width-target",direct}});});
    direct=*find_board("grid-width-target").layout;direct.grid->id="grid-width-replaced";
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{apply({SetArtboardLayout{composition,"grid-width-target",direct}});});
    direct=*find_board("grid-width-target").layout;direct.grid.reset();
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{apply({SetArtboardLayout{composition,"grid-width-target",direct}});});
    direct=*find_board("grid-width-target").layout;direct.grid->bounds_width_driver=height_source;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-width-target",direct}});});
    target_update=find_board("grid-width-target");target_update.layout->grid->bounds.width=701;
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{apply({UpdateArtboard{composition,target_update}});});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{"grid-width-source","point","artboard.width"},"INVALID_ARTBOARD_REF"},
            {{"grid-width-source","","grid.bounds.width"},"INVALID_ARTBOARD_REF"},
            {{"grid-width-target","","artboard.width"},"GRID_SELF_LINK"},
            {{"grid-width-comp","","artboard.width"},"TYPE_MISMATCH"},
            {{"grid-width-other","","artboard.width"},"WRONG_COMPOSITION"},
            {{"grid-width-missing","","artboard.width"},"MISSING_ARTBOARD"}}) {
        rejects(bad.second.c_str(),[&]{apply({GridBoundsWidthCommand{LinkGridBoundsWidth{target,bad.first,true}}});});
    }
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridBoundsWidthCommand{LinkGridBoundsWidth{{target.object,"point","grid.bounds.width"},source,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridBoundsWidthCommand{LinkGridBoundsWidth{{target.object,"","grid.bounds.x"},source,true}}});});
    rejects("MISSING_GRID",[&]{apply({GridBoundsWidthCommand{UnlinkGridBoundsWidth{{"grid-width-missing-grid","","grid.bounds.width"}}}});});
    rejects("DUPLICATE_TARGET",[&]{apply({GridBoundsWidthCommand{UnlinkGridBoundsWidth{target}},
        GridBoundsWidthCommand{LinkGridBoundsWidth{target,source,true}}});});
    const auto stale=session.revision();
    rejects("REVISION_CONFLICT",[&]{session.apply({GridBoundsWidthCommand{UnlinkGridBoundsWidth{target}}},stale-1);});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-width-source"}});});
    rejects("INVALID_GUIDE",[&]{apply({GridBoundsWidthCommand{UnlinkGridBoundsWidth{target}},
        AddGuide{composition,{"grid-width-bad-guide","Bad axis","z",25}}});});
    auto upstream_invalid=find_board("grid-width-upstream");upstream_invalid.width=940;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream_invalid}});});
    upstream_invalid=find_board("grid-width-upstream");upstream_invalid.width=20;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream_invalid}});});
    target_update=find_board("grid-width-target");target_update.width=700;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,target_update}});});
    auto invalid_gutter=*find_board("grid-width-target").layout;invalid_gutter.grid->column_gutter=800;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{composition,"grid-width-target",invalid_gutter}});});
    check(session.revision()==rev_before&&session.history()==history_before&&encode(session.document())==bytes_before&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==720,
        "Source deletion, stale revisions, direct writes, failed batches and width or cell invalidation leave bytes, revision and history unchanged");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bounds_width_driver\"")!=std::string::npos&&native.find("grid-width-source")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.60 preserves the exact Grid width link beside its authored literal");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.46\"")==1,
        "Grid width version-lie fixture changes only the writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"bounds_width_driver\":{","\"bounds_width_driver\":{\"extra\":1,")==1,
        "Malformed Grid width driver fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto literal_document=session.document();
    auto& literal_grid=*std::find_if(literal_document.compositions.front().artboards.begin(),literal_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-width-target";})->layout->grid;
    literal_grid.bounds_width_driver.reset();
    auto native_046=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(native_046,"\"version\":\"0.66\"","\"version\":\"0.46\"")==1&&
        decode(native_046)==literal_document,"Native 0.46 remains readable when the optional Grid width link is absent");

    apply({GridBoundsWidthCommand{UnlinkGridBoundsWidth{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==720&&!typed.driver&&std::get<double>(typed.evaluated)==720,
        "Unlink freezes evaluated Grid width into its authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==700&&typed.driver==source&&std::get<double>(typed.evaluated)==720,
        "Undo restores the exact Grid width source and original literal");
    session.redo(session.revision());upstream_update=find_board("grid-width-upstream");upstream_update.width=600;
    apply({UpdateArtboard{composition,upstream_update}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==720&&!typed.driver&&std::get<double>(typed.evaluated)==720,
        "Redo keeps frozen Grid width independent from later Artboard-size changes");
}

void grid_column_gutter_artboard_link() {
    auto document=empty_document("grid-gutter-doc","grid-gutter-comp","grid-gutter-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-gutter-other-comp","Other plane",{},{{"grid-gutter-other","Other frame",0,0,400,300}}});
    const Id composition="grid-gutter-comp";
    const Ref target{"grid-gutter-target-grid","","grid.column_gutter"};
    const Ref source{"grid-gutter-source","","artboard.width"};
    const Ref alternate{"grid-gutter-alternate","","artboard.height"};
    const Ref upstream_ref{"grid-gutter-upstream","","artboard.width"};
    Artboard source_board{"grid-gutter-source","Source frame",0,0,20,100};
    source_board.parent_size=ArtboardParent{"grid-gutter-upstream",true,false};
    Artboard alternate_board{"grid-gutter-alternate","Alternate frame",0,0,100,40};
    Artboard upstream_board{"grid-gutter-upstream","Upstream frame",0,0,20,100};
    ArtboardLayout layout;layout.grid=Grid{"grid-gutter-target-grid",{40,40,880,500},2,1,20,0};
    Contour outline;outline.id="grid-gutter-outline";outline.closed=true;
    for(const auto& [x,y]:std::array<std::pair<double,double>,4>{{{10,10},{30,10},{30,30},{10,30}}}) {
        Point point;point.id="grid-gutter-point-"+std::to_string(outline.points.size());
        point.x.literal=x;point.y.literal=y;outline.points.push_back(point);
    }
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,alternate_board,2},
        AddArtboard{composition,upstream_board,3},SetArtboardLayout{composition,"grid-gutter-target",layout},
        CreatePath{composition,"","grid-gutter-shape","Grid gutter independence",{outline}}});
    const auto artwork_before=evaluate(session.document());

    const auto no_grid_document=empty_document("no-grid-gutter-doc","no-grid-gutter-comp","no-grid-gutter-target");
    Session no_grid_session(no_grid_document);
    no_grid_session.apply({AddArtboard{"no-grid-gutter-comp",Artboard{"no-grid-gutter-source","Source",0,0,40,100},1}},0);
    rejects("MISSING_GRID",[&]{no_grid_session.apply({GridColumnGutterCommand{LinkGridColumnGutter{
        {"no-grid-gutter-target-grid","","grid.column_gutter"},{"no-grid-gutter-source","","artboard.width"},false}}},
        no_grid_session.revision());});

    auto smuggled=layout;smuggled.grid->column_gutter_driver=source;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-gutter-target",smuggled}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{
        auto board=Artboard{"grid-gutter-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected;injected.grid=Grid{"grid-gutter-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected.grid->column_gutter_driver=source;board.layout=injected;
        apply({AddArtboard{composition,board,4}});
    });

    const auto json_link=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_grid_column_gutter","target":{"object":"grid-gutter-target-grid","point":"","field":"grid.column_gutter"},
       "source":{"object":"grid-gutter-source","point":"","field":"artboard.width"},"replace_driver":false}
    ]})");
    check(json_link.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session links Grid column gutter through its dedicated revisioned Artboard-size command");
    auto typed=artboard_layout_property(session.document(),target);
    auto evaluated=evaluate_artboard(session.document().compositions.front(),"grid-gutter-target");
    auto cell_width=(evaluated.layout->grid->bounds.width-
        (evaluated.layout->grid->columns-1)*evaluated.layout->grid->column_gutter)/evaluated.layout->grid->columns;
    check(std::get<double>(typed.literal)==20&&typed.driver==source&&typed.source_kind=="link"&&
        std::get<double>(typed.evaluated)==20&&evaluated.layout->grid->column_gutter==20&&cell_width==430&&
        find_board("grid-gutter-target").layout->grid->id==target.object,
        "Grid column gutter evaluates the parent Artboard width while retaining its literal and stable Grid ID");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-gutter-target-grid","point":"","field":"grid.column_gutter"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("\"field\":\"artboard.width\"")!=std::string::npos&&
        typed_json.find("\"literal\":2E1")!=std::string::npos&&typed_json.find("\"evaluated\":2E1")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"field\":\"grid.column_gutter\"")!=std::string::npos,
        ("Typed get and properties disclose the authored gutter, exact source, evaluated value and link capability: "+
            typed_json+" | "+properties_json).c_str());
    const auto linked_revision=session.revision();
    apply({GridColumnGutterCommand{LinkGridColumnGutter{target,source,false}}});
    check(session.revision()==linked_revision,"Repeating the exact Grid column gutter source is idempotent");
    rejects("DRIVEN_GRID_COLUMN_GUTTER",[&]{apply({GridColumnGutterCommand{LinkGridColumnGutter{target,alternate,false}}});});
    apply({GridColumnGutterCommand{LinkGridColumnGutter{target,alternate,true}}});
    check(artboard_layout_property(session.document(),target).driver==alternate&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==40,
        "Explicit replacement changes Grid column gutter to a distinct Artboard height source");
    apply({GridColumnGutterCommand{LinkGridColumnGutter{target,source,true}}});

    auto upstream=find_board("grid-gutter-upstream");upstream.width=30;
    apply({UpdateArtboard{composition,upstream}});
    typed=artboard_layout_property(session.document(),target);evaluated=evaluate_artboard(session.document().compositions.front(),"grid-gutter-target");
    cell_width=(evaluated.layout->grid->bounds.width-evaluated.layout->grid->column_gutter)/2;
    check(std::get<double>(typed.literal)==20&&typed.driver==source&&std::get<double>(typed.evaluated)==30&&
        evaluated.layout->grid->column_gutter==30&&cell_width==425&&evaluate(session.document())==artwork_before,
        "Upstream resize changes the evaluated gutter and column cell without reflowing authored Objects");
    const auto bytes_before_invalid=encode(session.document());const auto revision_before_invalid=session.revision();
    const auto history_before_invalid=session.history();upstream=find_board("grid-gutter-upstream");upstream.width=880;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream}});});
    check(session.revision()==revision_before_invalid&&session.history()==history_before_invalid&&
        encode(session.document())==bytes_before_invalid&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==30,
        "A zero-width evaluated column cell rejects atomically with prior native bytes, revision and history");

    auto target_layout=*find_board("grid-gutter-target").layout;target_layout.grid->bounds.x=60;target_layout.grid->row_gutter=10;
    apply({SetArtboardLayout{composition,"grid-gutter-target",target_layout}});
    auto target_update=find_board("grid-gutter-target");target_update.name="Moved target";target_update.x=120;
    auto renamed_source=find_board("grid-gutter-source");renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,target_update},UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"grid-gutter-upstream","grid-gutter-alternate","grid-gutter-source","grid-gutter-target"}}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==20&&typed.driver==source&&std::get<double>(typed.evaluated)==30&&
        find_board("grid-gutter-target").layout->grid->id==target.object&&find_board("grid-gutter-target").layout->grid->bounds.x==60&&
        find_board("grid-gutter-target").x==120&&find_board("grid-gutter-source").name=="Renamed source"&&
        find_board("grid-gutter-source").x==50,
        "Sibling edits, source rename/move and Artboard reorder preserve the exact stable source Ref");

    const auto bytes_before=encode(session.document());const auto rev_before=session.revision();const auto history_before=session.history();
    auto direct=*find_board("grid-gutter-target").layout;direct.grid->column_gutter=21;
    rejects("DRIVEN_GRID_COLUMN_GUTTER",[&]{apply({SetArtboardLayout{composition,"grid-gutter-target",direct}});});
    direct=*find_board("grid-gutter-target").layout;direct.grid->column_gutter_driver=alternate;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-gutter-target",direct}});});
    target_update=find_board("grid-gutter-target");target_update.layout->grid->column_gutter=21;
    rejects("DRIVEN_GRID_COLUMN_GUTTER",[&]{apply({UpdateArtboard{composition,target_update}});});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{"grid-gutter-source","point","artboard.width"},"INVALID_ARTBOARD_REF"},
            {{"grid-gutter-source","","grid.column_gutter"},"INVALID_ARTBOARD_REF"},
            {{"grid-gutter-target","","artboard.width"},"GRID_SELF_LINK"},
            {{"grid-gutter-comp","","artboard.width"},"TYPE_MISMATCH"},
            {{"grid-gutter-other","","artboard.width"},"WRONG_COMPOSITION"},
            {{"grid-gutter-missing","","artboard.width"},"MISSING_ARTBOARD"}})
        rejects(bad.second.c_str(),[&]{apply({GridColumnGutterCommand{LinkGridColumnGutter{target,bad.first,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridColumnGutterCommand{LinkGridColumnGutter{{target.object,"point","grid.column_gutter"},source,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridColumnGutterCommand{LinkGridColumnGutter{{target.object,"","grid.bounds.x"},source,true}}});});
    rejects("MISSING_GRID",[&]{apply({GridColumnGutterCommand{UnlinkGridColumnGutter{{"grid-gutter-missing-grid","","grid.column_gutter"}}}});});
    rejects("DUPLICATE_TARGET",[&]{apply({GridColumnGutterCommand{UnlinkGridColumnGutter{target}},
        GridColumnGutterCommand{LinkGridColumnGutter{target,source,true}}});});
    const auto stale=session.revision();
    rejects("REVISION_CONFLICT",[&]{session.apply({GridColumnGutterCommand{UnlinkGridColumnGutter{target}}},stale-1);});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-gutter-source"}});});
    rejects("INVALID_GUIDE",[&]{apply({GridColumnGutterCommand{UnlinkGridColumnGutter{target}},
        AddGuide{composition,{"grid-gutter-bad-guide","Bad axis","z",25}}});});
    check(session.revision()==rev_before&&session.history()==history_before&&encode(session.document())==bytes_before&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==30,
        "Source deletion, direct edits, stale revisions and failed batches preserve bytes, evaluation, revision and history");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"column_gutter_driver\":{\"link\"")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.60 retains the closed Grid column gutter link beside its authored literal");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.50\"")==1,
        "Grid column gutter version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"column_gutter_driver\":{\"link\":",
        "\"column_gutter_driver\":{\"extra\":1,\"link\":")==1,"Malformed Grid column gutter driver fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto literal_document=session.document();
    auto& literal_grid=*std::find_if(literal_document.compositions.front().artboards.begin(),literal_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-gutter-target";})->layout->grid;
    literal_grid.column_gutter_driver.reset();
    auto native_050=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(native_050,"\"version\":\"0.66\"","\"version\":\"0.50\"")==1&&
        decode(native_050)==literal_document,"Native 0.50 remains readable when the optional column gutter link is absent");

    apply({GridColumnGutterCommand{UnlinkGridColumnGutter{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==30&&!typed.driver&&std::get<double>(typed.evaluated)==30,
        "Unlink freezes evaluated Grid column gutter into its authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==20&&typed.driver==source&&std::get<double>(typed.evaluated)==30,
        "Undo restores the exact Grid column gutter source and original literal");
    session.redo(session.revision());upstream=find_board("grid-gutter-upstream");upstream.width=10;
    apply({UpdateArtboard{composition,upstream}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==30&&!typed.driver&&std::get<double>(typed.evaluated)==30,
        "Redo keeps the frozen gutter independent from later Artboard-size changes");
}

void grid_columns_same_field_link() {
    auto document=empty_document("grid-columns-doc","grid-columns-comp","grid-columns-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    Artboard source_board{"grid-columns-source-board","Source",0,0,400,300};
    Artboard alternate_board{"grid-columns-alternate-board","Alternate",0,0,400,300};
    ArtboardLayout target_layout;target_layout.grid=Grid{"grid-columns-target-grid",{40,40,880,500},2,1,20,0};
    ArtboardLayout source_layout;source_layout.grid=Grid{"grid-columns-source",{0,0,400,300},2,1,0,0};
    ArtboardLayout alternate_layout;alternate_layout.grid=Grid{"grid-columns-alternate",{0,0,400,300},4,1,0,0};
    target_board.layout=target_layout;source_board.layout=source_layout;alternate_board.layout=alternate_layout;
    document.compositions.front().artboards.push_back(source_board);
    document.compositions.front().artboards.push_back(alternate_board);
    Artboard foreign_board{"grid-columns-foreign-board","Foreign",0,0,400,300};
    ArtboardLayout foreign_layout;foreign_layout.grid=Grid{"grid-columns-foreign",{0,0,400,300},5,1,0,0};
    foreign_board.layout=foreign_layout;
    document.compositions.push_back({"grid-columns-other-comp","Other plane",{}, {foreign_board}});

    const Id composition="grid-columns-comp";
    const Ref target{"grid-columns-target-grid","","grid.columns"};
    const Ref source{"grid-columns-source","","grid.columns"};
    const Ref alternate{"grid-columns-alternate","","grid.columns"};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    auto cell_width=[&] {
        const auto resolved=evaluate_artboard(session.document().compositions.front(),"grid-columns-target");
        const auto& grid=*resolved.layout->grid;
        return (grid.bounds.width-static_cast<double>(grid.columns-1)*grid.column_gutter)/grid.columns;
    };

    const auto linked_json=request(session,R"({"op":"apply","expected_revision":0,"commands":[
      {"type":"link_grid_columns","target":{"object":"grid-columns-target-grid","point":"","field":"grid.columns"},
       "source":{"object":"grid-columns-source","point":"","field":"grid.columns"},"replace_driver":false}
    ]})");
    check(linked_json.find("\"ok\":true")!=std::string::npos&&session.revision()==1,
        "JSON Session links Grid columns through its revisioned same-field command");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.driver==source&&typed.source_kind=="link"&&
        std::get<std::size_t>(typed.evaluated)==2&&cell_width()==430&&
        find_board("grid-columns-target").layout->grid->id==target.object,
        "Grid columns begins at 2 with exact source and 430-unit target cells while retaining its literal and stable ID");
    const auto get_json=request(session,R"({"op":"get","ref":{"object":"grid-columns-target-grid","point":"","field":"grid.columns"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(get_json.find("\"type\":\"integer\"")!=std::string::npos&&
        get_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        get_json.find("\"literal\":2")!=std::string::npos&&get_json.find("\"evaluated\":2")!=std::string::npos&&
        get_json.find("\"field\":\"grid.columns\"")!=std::string::npos&&
        get_json.find("\"link\":true")!=std::string::npos&&get_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"object\":\"grid-columns-target-grid\"")!=std::string::npos,
        ("Typed get/properties expose integer literal, exact source, evaluated count and link capability: "+get_json).c_str());

    const auto linked_revision=session.revision();
    apply({GridColumnsCommand{LinkGridColumns{target,source,false}}});
    check(session.revision()==linked_revision,"Repeating the exact Grid columns source is idempotent");
    rejects("DRIVEN_GRID_COLUMNS",[&]{apply({GridColumnsCommand{LinkGridColumns{target,alternate,false}}});});
    apply({GridColumnsCommand{LinkGridColumns{target,alternate,true}}});
    check(artboard_layout_property(session.document(),target).driver==alternate&&cell_width()==205,
        "Explicit replacement selects a distinct Grid source and evaluates four columns");
    apply({GridColumnsCommand{LinkGridColumns{target,source,true}}});

    auto source_update=find_board("grid-columns-source-board");source_update.layout->grid->columns=3;
    apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.driver==source&&
        std::get<std::size_t>(typed.evaluated)==3&&cell_width()==280,
        "Changing source count reevaluates cell geometry without changing the target literal");
    auto target_update=find_board("grid-columns-target");target_update.name="Renamed target";target_update.x=120;
    auto renamed_source=find_board("grid-columns-source-board");renamed_source.name="Renamed source";renamed_source.x=50;
    auto sibling_layout=*target_update.layout;sibling_layout.grid->row_gutter=10;
    target_update.layout=sibling_layout;
    apply({SetArtboardLayout{composition,target_update.id,sibling_layout},UpdateArtboard{composition,target_update},
        UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"grid-columns-alternate-board","grid-columns-source-board","grid-columns-target"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&std::get<std::size_t>(typed.evaluated)==3&&
        find_board("grid-columns-target").layout->grid->id==target.object&&
        find_board("grid-columns-target").layout->grid->row_gutter==10&&
        find_board("grid-columns-source-board").name=="Renamed source"&&cell_width()==280,
        "Sibling layout edits, source rename/move and Artboard reorder preserve the stable Grid Ref");

    auto direct=*find_board("grid-columns-target").layout;direct.grid->columns=7;
    rejects("DRIVEN_GRID_COLUMNS",[&]{apply({SetArtboardLayout{composition,"grid-columns-target",direct}});});
    auto cleared=*find_board("grid-columns-target").layout;cleared.grid.reset();
    rejects("DRIVEN_GRID_COLUMNS",[&]{apply({SetArtboardLayout{composition,"grid-columns-target",cleared}});});
    auto smuggled=*find_board("grid-columns-target").layout;smuggled.grid->columns_driver=alternate;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-columns-target",smuggled}});});
    target_update=find_board("grid-columns-target");target_update.layout->grid->columns=7;
    rejects("DRIVEN_GRID_COLUMNS",[&]{apply({UpdateArtboard{composition,target_update}});});
    auto injected=Artboard{"grid-columns-injected-board","Injected",0,0,400,300};
    ArtboardLayout injected_layout;injected_layout.grid=Grid{"grid-columns-injected",{0,0,400,300},2,1,0,0};
    injected_layout.grid->columns_driver=source;injected.layout=injected_layout;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({AddArtboard{composition,injected,3}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridColumnsCommand{LinkGridColumns{{target.object,"point","grid.columns"},source,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridColumnsCommand{LinkGridColumns{{target.object,"","grid.rows"},source,true}}});});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{"grid-columns-source","point","grid.columns"},"INVALID_GRID_COLUMNS_REF"},
            {{"grid-columns-source","","grid.rows"},"INVALID_GRID_COLUMNS_REF"},
            {{"grid-columns-target-grid","","grid.columns"},"GRID_COLUMNS_SELF_LINK"},
            {{"grid-columns-foreign","","grid.columns"},"WRONG_COMPOSITION"},
            {{"grid-columns-missing","","grid.columns"},"MISSING_GRID"}})
        rejects(bad.second.c_str(),[&]{apply({GridColumnsCommand{LinkGridColumns{target,bad.first,true}}});});
    rejects("MISSING_GRID",[&]{apply({GridColumnsCommand{UnlinkGridColumns{{"grid-columns-missing","","grid.columns"}}}});});
    rejects("DUPLICATE_TARGET",[&]{apply({GridColumnsCommand{UnlinkGridColumns{target}},
        GridColumnsCommand{LinkGridColumns{target,source,true}}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({GridColumnsCommand{UnlinkGridColumns{target}}},session.revision()-1);});
    rejects("DEPENDENCY_CYCLE",[&]{apply({GridColumnsCommand{LinkGridColumns{source,target,false}}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-columns-source-board"}});});
    auto impossible_layout=*find_board("grid-columns-target").layout;
    impossible_layout.grid->column_gutter=300;
    apply({SetArtboardLayout{composition,"grid-columns-target",impossible_layout}});
    rejects("INVALID_LAYOUT",[&]{apply({GridColumnsCommand{LinkGridColumns{target,alternate,true}}});});
    impossible_layout=*find_board("grid-columns-target").layout;impossible_layout.grid->column_gutter=20;
    apply({SetArtboardLayout{composition,"grid-columns-target",impossible_layout}});
    const auto bytes_before=encode(session.document());const auto revision_before=session.revision();const auto history_before=session.history();
    source_update=find_board("grid-columns-source-board");source_update.layout->grid->columns=1000;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});});
    rejects("INVALID_GUIDE",[&]{apply({GridColumnsCommand{UnlinkGridColumns{target}},
        AddGuide{composition,{"grid-columns-bad-guide","Bad axis","z",25}}});});
    check(session.revision()==revision_before&&session.history()==history_before&&encode(session.document())==bytes_before&&
        std::get<std::size_t>(artboard_layout_property(session.document(),target).evaluated)==3,
        "Invalid refs, replacement, cycles, source removal, count/geometry changes, stale revisions and later batch failure preserve state atomically");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"columns_driver\":{\"link\"")!=std::string::npos&&encode(decode(native))==native,
        "Native 0.60 retains the closed Grid columns link beside its authored integer");
    auto previous=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(previous,"\"version\":\"0.66\"","\"version\":\"0.57\"")==1&&decode(previous)==session.document(),
        "Native 0.57 continues to read the existing Grid columns link");
    auto previous_056=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(previous_056,"\"version\":\"0.66\"","\"version\":\"0.56\"")==1&&
        decode(previous_056)==session.document(),"Native 0.56 continues to read the existing Grid columns link");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.55\"")==1,
        "Grid columns 0.56 version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"columns_driver\":{\"link\":",
        "\"columns_driver\":{\"extra\":1,\"link\":")==1,"Malformed Grid columns driver adds an unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto literal_document=session.document();
    auto& literal_grid=*std::find_if(literal_document.compositions.front().artboards.begin(),literal_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-columns-target";})->layout->grid;
    literal_grid.columns_driver.reset();auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.57\"")==1&&decode(legacy)==literal_document,
        "Native 0.57 remains readable when the new optional Grid columns expression is absent");

    apply({GridColumnsCommand{UnlinkGridColumns{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==3&&!typed.driver&&std::get<std::size_t>(typed.evaluated)==3,
        "Unlink freezes the evaluated Grid count into the authored literal in one revision");
    source_update=find_board("grid-columns-source-board");source_update.layout->grid->columns=4;
    apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==3&&!typed.driver&&std::get<std::size_t>(typed.evaluated)==3,
        "After unlink, later source count edits do not change the frozen target");
    session.undo(session.revision());session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.driver==source&&std::get<std::size_t>(typed.evaluated)==3,
        "Undo restores the exact Grid columns link and authored literal");
    session.redo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==3&&!typed.driver&&std::get<std::size_t>(typed.evaluated)==3,
        "Redo restores the frozen evaluated count");
}

void grid_columns_expression() {
    auto document=empty_document("grid-columns-expression-doc","grid-columns-expression-comp",
        "grid-columns-expression-target-board");
    auto& target_board=document.compositions.front().artboards.front();
    target_board.width=960;target_board.height=640;
    Artboard source_board{"grid-columns-expression-source-board","Columns source",0,0,400,300};
    Artboard alternate_board{"grid-columns-expression-alternate-board","Alternate source",0,0,400,300};
    ArtboardLayout target_layout;target_layout.grid=Grid{"grid-columns-expression-target",
        {40,40,880,500},2,1,20,0};
    ArtboardLayout source_layout;source_layout.grid=Grid{"grid-columns-expression-source",
        {0,0,400,300},2,1,0,0};
    ArtboardLayout alternate_layout;alternate_layout.grid=Grid{"grid-columns-expression-alternate",
        {0,0,400,300},4,1,0,0};
    target_board.layout=target_layout;source_board.layout=source_layout;alternate_board.layout=alternate_layout;
    document.compositions.front().artboards.push_back(source_board);
    document.compositions.front().artboards.push_back(alternate_board);
    Artboard foreign_board{"grid-columns-expression-foreign-board","Foreign",0,0,400,300};
    ArtboardLayout foreign_layout;foreign_layout.grid=Grid{"grid-columns-expression-foreign",
        {0,0,400,300},5,1,0,0};foreign_board.layout=foreign_layout;
    document.compositions.push_back({"grid-columns-expression-other-comp","Other plane",{}, {foreign_board}});

    const Id composition="grid-columns-expression-comp";
    const Ref target{"grid-columns-expression-target","","grid.columns"};
    const Ref source{"grid-columns-expression-source","","grid.columns"};
    const Ref alternate{"grid-columns-expression-alternate","","grid.columns"};
    const Expression expression{R"(ref("grid-columns-expression-source","","grid.columns") + 1)",1};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    auto cell_width=[&] {
        const auto resolved=evaluate_artboard(session.document().compositions.front(),"grid-columns-expression-target-board");
        const auto& grid=*resolved.layout->grid;
        return (grid.bounds.width-static_cast<double>(grid.columns-1)*grid.column_gutter)/grid.columns;
    };
    auto expression_command=[&](Expression value,bool replace=false) {
        return GridColumnsCommand{SetGridColumnsExpression{target,std::move(value),replace}};
    };

    apply({expression_command(expression)});
    auto typed=artboard_layout_property(session.document(),target);
    check(typed.source_kind=="expression"&&!typed.driver&&typed.expression==expression&&
        std::get<std::size_t>(typed.literal)==2&&std::get<std::size_t>(typed.evaluated)==3&&cell_width()==280&&
        find_board("grid-columns-expression-target-board").layout->grid->id==target.object,
        "Grid columns expression keeps the target literal and stable ID while source 2 plus one produces three 280-unit cells");
    auto json=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_grid_columns_expression","target":{"object":"grid-columns-expression-target","point":"","field":"grid.columns"},
       "expression":{"source":"ref(\"grid-columns-expression-source\",\"\",\"grid.columns\") + 1","version":1},"replace_driver":false}
    ]})");
    check(json.find("\"ok\":true")!=std::string::npos&&session.revision()==1,
        "JSON-lines exposes the revisioned Grid columns expression command and exact replay is idempotent");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-columns-expression-target","point":"","field":"grid.columns"}})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("\"expression\":{\"source\":\"ref(\\\"grid-columns-expression-source\\\",\\\"\\\",\\\"grid.columns\\\") + 1\",\"version\":1}")!=std::string::npos&&
        typed_json.find("\"literal\":2")!=std::string::npos&&typed_json.find("\"evaluated\":3")!=std::string::npos&&
        typed_json.find("\"max\":1000")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos,
        ("Typed Grid columns get exposes exact expression, literal, range and evaluated value: "+typed_json).c_str());
    const auto stable_revision=session.revision();
    apply({expression_command(expression)});
    check(session.revision()==stable_revision,"Reapplying identical Grid columns expression does not create a revision");
    rejects("DRIVEN_GRID_COLUMNS",[&]{apply({expression_command(
        {R"(ref("grid-columns-expression-alternate","","grid.columns") + 1)",1})});});
    apply({expression_command({R"(ref("grid-columns-expression-alternate","","grid.columns") + 1)",1},true)});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==Expression{R"(ref("grid-columns-expression-alternate","","grid.columns") + 1)",1}&&
        std::get<std::size_t>(typed.evaluated)==5&&cell_width()==160,
        "Changing expression source requires explicit replacement and evaluates exact integer count five");
    apply({expression_command(expression,true)});

    auto source_update=find_board("grid-columns-expression-source-board");source_update.layout->grid->columns=3;
    apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.expression==expression&&
        std::get<std::size_t>(typed.evaluated)==4&&cell_width()==205,
        "Source count 3 reevaluates Grid columns to 4 and horizontal cell width 205 while retaining literal 2");
    auto renamed_target=find_board("grid-columns-expression-target-board");
    renamed_target.name="Renamed expression target";renamed_target.x=80;
    apply({UpdateArtboard{composition,renamed_target}});
    check(artboard_layout_property(session.document(),target).expression==expression&&
        std::get<std::size_t>(artboard_layout_property(session.document(),target).evaluated)==4&&
        find_board("grid-columns-expression-target-board").name=="Renamed expression target"&&cell_width()==205,
        "Artboard rename and move preserve the exact expression source, target ID and evaluated cell count");

    const auto stable_bytes=encode(session.document());const auto stable_history=session.history();
    const auto stable_revision_before_rejections=session.revision();
    for(const auto& invalid:std::vector<std::pair<Expression,const char*>>{
            {{R"(ref("grid-columns-expression-source","","grid.rows"))",1},"GRID_COLUMNS_EXPRESSION_TYPE"},
            {{R"(ref("grid-columns-expression-source","point","grid.columns"))",1},"GRID_COLUMNS_EXPRESSION_TYPE"},
            {{R"(ref("grid-columns-expression-foreign","","grid.columns"))",1},"WRONG_COMPOSITION"},
            {{R"(ref("grid-columns-expression-missing","","grid.columns"))",1},"MISSING_GRID"},
            {{R"(ref("grid-columns-expression-target","","grid.columns"))",1},"GRID_COLUMNS_SELF_LINK"},
            {{"1",2},"UNSUPPORTED_EXPRESSION_VERSION"},{{"1du",1},"EXPRESSION_SYNTAX"},
            {{"(",1},"EXPRESSION_SYNTAX"},{{"3.5",1},"OUT_OF_RANGE"},{{"0",1},"OUT_OF_RANGE"},
            {{"1001",1},"OUT_OF_RANGE"},{{"1 / 0",1},"EXPRESSION_DOMAIN"}})
        rejects(invalid.second,[&]{apply({expression_command(invalid.first,true)});});
    auto deep_document=empty_document("grid-columns-depth-doc","grid-columns-depth-comp","grid-columns-depth-board-0");
    constexpr std::size_t depth_nodes=130;
    for(std::size_t index=0;index<depth_nodes;++index) {
        const auto grid_id="grid-columns-depth-grid-"+std::to_string(index);
        const auto board_id="grid-columns-depth-board-"+std::to_string(index);
        Artboard* board=nullptr;
        if(index==0)board=&deep_document.compositions.front().artboards.front();
        else {
            deep_document.compositions.front().artboards.push_back(
                {board_id,"Depth "+std::to_string(index),0,0,400,300});
            board=&deep_document.compositions.front().artboards.back();
        }
        ArtboardLayout layout;layout.grid=Grid{grid_id,{0,0,400,300},2,1,0,0};
        if(index+1<depth_nodes)
            layout.grid->columns_expression=Expression{"ref(\"grid-columns-depth-grid-"+
                std::to_string(index+1)+"\",\"\",\"grid.columns\") + 0",1};
        board->layout=std::move(layout);
    }
    rejects("DEPENDENCY_DEPTH",[&]{Session deep_session(deep_document);});
    rejects("DEPENDENCY_CYCLE",[&]{apply({GridColumnsCommand{LinkGridColumns{source,target,false}}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-columns-expression-source-board"}});});
    auto direct=*find_board("grid-columns-expression-target-board").layout;direct.grid->columns=7;
    rejects("DRIVEN_GRID_COLUMNS",[&]{apply({SetArtboardLayout{composition,
        "grid-columns-expression-target-board",direct}});});
    auto smuggled=*find_board("grid-columns-expression-target-board").layout;
    smuggled.grid->columns_expression=Expression{"5",1};
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,
        "grid-columns-expression-target-board",smuggled}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{Artboard injected{"grid-columns-expression-smuggled-board",
        "Smuggled expression",0,0,400,300};ArtboardLayout injected_layout;
        injected_layout.grid=Grid{"grid-columns-expression-smuggled-grid",{0,0,400,300},2,1,0,0};
        injected_layout.grid->columns_expression=expression;injected.layout=injected_layout;
        apply({AddArtboard{composition,injected,session.document().compositions.front().artboards.size()}});});
    rejects("DUPLICATE_TARGET",[&]{apply({expression_command(expression,true),GridColumnsCommand{UnlinkGridColumns{target}}});});
    rejects("INVALID_GUIDE",[&]{apply({GridColumnsCommand{UnlinkGridColumns{target}},
        AddGuide{composition,{"grid-columns-expression-bad-guide","Bad axis","z",25}}});});
    check(session.revision()==stable_revision_before_rejections&&session.history()==stable_history&&
        encode(session.document())==stable_bytes&&std::get<std::size_t>(artboard_layout_property(session.document(),target).evaluated)==4,
        "Invalid expression type/range/unit/syntax/version, cycles, deletion, injection and failed batches preserve exact Session state");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"columns_expression\":{\"source\":\"ref")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.60 retains the exact Grid columns expression and byte-roundtrips");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.57\"")==1,
        "Grid columns 0.57 version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"columns_expression\":{\"source\":",
        "\"columns_expression\":{\"extra\":true,\"source\":")==1,
        "Malformed Grid columns expression fixture adds one unknown field");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto link_and_expression=native;check(replace_all(link_and_expression,"\"columns_expression\":{\"source\":",
        "\"columns_driver\":{\"link\":{\"object\":\"grid-columns-expression-source\",\"point\":\"\",\"field\":\"grid.columns\"}},\"columns_expression\":{\"source\":")==1,
        "Grid columns conflict fixture adds the existing link beside the expression");
    rejects("GRID_COLUMNS_SOURCE_CONFLICT",[&]{(void)decode(link_and_expression);});
    auto legacy_document=session.document();
    std::find_if(legacy_document.compositions.front().artboards.begin(),legacy_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-columns-expression-target-board";})->layout->grid->columns_expression.reset();
    auto previous=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(previous,"\"version\":\"0.66\"","\"version\":\"0.57\"")==1&&
        decode(previous)==legacy_document,"Native 0.57 remains readable without the new optional Grid columns expression");

    apply({GridColumnsCommand{UnlinkGridColumns{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==4&&!typed.driver&&!typed.expression&&
        std::get<std::size_t>(typed.evaluated)==4,"Unlink freezes expression-evaluated columns 4 into the literal");
    source_update=find_board("grid-columns-expression-source-board");source_update.layout->grid->columns=4;
    apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==4&&!typed.expression&&std::get<std::size_t>(typed.evaluated)==4,
        "An upstream count change after unlink cannot alter the frozen column count");
    session.undo(session.revision());session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.expression==expression&&
        std::get<std::size_t>(typed.evaluated)==4,"Undo restores the exact expression and original authored literal");
    session.redo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==4&&!typed.expression&&std::get<std::size_t>(typed.evaluated)==4,
        "Redo restores the frozen evaluated integer");
}


void grid_rows_expression() {
    auto document=empty_document("grid-rows-expression-doc","grid-rows-expression-comp",
        "grid-rows-expression-target-board");
    auto& target_board=document.compositions.front().artboards.front();
    target_board.width=960;target_board.height=640;
    Artboard source_board{"grid-rows-expression-source-board","Rows source",0,0,400,300};
    Artboard alternate_board{"grid-rows-expression-alternate-board","Alternate source",0,0,400,300};
    ArtboardLayout target_layout;target_layout.grid=Grid{"grid-rows-expression-target",
        {40,40,880,500},2,2,20,20};
    ArtboardLayout source_layout;source_layout.grid=Grid{"grid-rows-expression-source",
        {0,0,400,300},2,2,0,0};
    ArtboardLayout alternate_layout;alternate_layout.grid=Grid{"grid-rows-expression-alternate",
        {0,0,400,300},4,3,0,0};
    target_board.layout=target_layout;source_board.layout=source_layout;alternate_board.layout=alternate_layout;
    document.compositions.front().artboards.push_back(source_board);
    document.compositions.front().artboards.push_back(alternate_board);
    Artboard foreign_board{"grid-rows-expression-foreign-board","Foreign",0,0,400,300};
    ArtboardLayout foreign_layout;foreign_layout.grid=Grid{"grid-rows-expression-foreign",
        {0,0,400,300},5,5,0,0};foreign_board.layout=foreign_layout;
    document.compositions.push_back({"grid-rows-expression-other-comp","Other plane",{}, {foreign_board}});

    const Id composition="grid-rows-expression-comp";
    const Ref target{"grid-rows-expression-target","","grid.rows"};
    const Ref source{"grid-rows-expression-source","","grid.rows"};
    const Ref alternate{"grid-rows-expression-alternate","","grid.rows"};
    const Expression expression{R"(ref("grid-rows-expression-source","","grid.rows") + 1)",1};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    auto cell_height=[&] {
        const auto resolved=evaluate_artboard(session.document().compositions.front(),"grid-rows-expression-target-board");
        const auto& grid=*resolved.layout->grid;
        return (grid.bounds.height-static_cast<double>(grid.rows-1)*grid.row_gutter)/grid.rows;
    };
    auto expression_command=[&](Expression value,bool replace=false) {
        return GridRowsCommand{SetGridRowsExpression{target,std::move(value),replace}};
    };

    apply({expression_command(expression)});
    auto typed=artboard_layout_property(session.document(),target);
    check(typed.source_kind=="expression"&&!typed.driver&&typed.expression==expression&&
        std::get<std::size_t>(typed.literal)==2&&std::get<std::size_t>(typed.evaluated)==3&&cell_height()==460.0/3.0&&
        find_board("grid-rows-expression-target-board").layout->grid->id==target.object,
        "Grid rows expression keeps the target literal and stable ID while source 2 plus one produces three 460/3-unit cells");
    auto json=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_grid_rows_expression","target":{"object":"grid-rows-expression-target","point":"","field":"grid.rows"},
       "expression":{"source":"ref(\"grid-rows-expression-source\",\"\",\"grid.rows\") + 1","version":1},"replace_driver":false}
    ]})");
    check(json.find("\"ok\":true")!=std::string::npos&&session.revision()==1,
        "JSON-lines exposes the revisioned Grid rows expression command and exact replay is idempotent");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-rows-expression-target","point":"","field":"grid.rows"}})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("\"expression\":{\"source\":\"ref(\\\"grid-rows-expression-source\\\",\\\"\\\",\\\"grid.rows\\\") + 1\",\"version\":1}")!=std::string::npos&&
        typed_json.find("\"literal\":2")!=std::string::npos&&typed_json.find("\"evaluated\":3")!=std::string::npos&&
        typed_json.find("\"max\":1000")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos,
        ("Typed Grid rows get exposes exact expression, literal, range and evaluated value: "+typed_json).c_str());
    const auto stable_revision=session.revision();
    apply({expression_command(expression)});
    check(session.revision()==stable_revision,"Reapplying identical Grid rows expression does not create a revision");
    rejects("DRIVEN_GRID_ROWS",[&]{apply({expression_command(
        {R"(ref("grid-rows-expression-alternate","","grid.rows") + 1)",1})});});
    apply({expression_command({R"(ref("grid-rows-expression-alternate","","grid.rows") + 1)",1},true)});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==Expression{R"(ref("grid-rows-expression-alternate","","grid.rows") + 1)",1}&&
        std::get<std::size_t>(typed.evaluated)==4&&cell_height()==110,
        "Changing expression source requires explicit replacement and evaluates exact integer count four");
    apply({expression_command(expression,true)});
    apply({GridRowsCommand{LinkGridRows{target,source,true}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.source_kind=="link"&&typed.driver==source&&!typed.expression&&
        std::get<std::size_t>(typed.evaluated)==2,
        "Explicit Grid rows link replacement clears the previous expression and uses the linked count");
    apply({expression_command(expression,true)});

    auto source_update=find_board("grid-rows-expression-source-board");source_update.layout->grid->rows=3;
    apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.expression==expression&&
        std::get<std::size_t>(typed.evaluated)==4&&cell_height()==110,
        "Source count 3 reevaluates Grid rows to 4 and cell height 110 while retaining literal 2");
    auto renamed_source=find_board("grid-rows-expression-source-board");
    renamed_source.name="Renamed expression source";renamed_source.x=80;
    apply({UpdateArtboard{composition,renamed_source},ReorderArtboards{composition,
        {"grid-rows-expression-alternate-board","grid-rows-expression-target-board","grid-rows-expression-source-board"}}});
    check(artboard_layout_property(session.document(),target).expression==expression&&
        std::get<std::size_t>(artboard_layout_property(session.document(),target).evaluated)==4&&
        find_board("grid-rows-expression-source-board").name=="Renamed expression source"&&cell_height()==110,
        "Source Artboard rename and reorder preserve the exact Grid Ref and evaluated cell count");

    const auto stable_bytes=encode(session.document());const auto stable_history=session.history();
    const auto stable_revision_before_rejections=session.revision();
    for(const auto& invalid:std::vector<std::pair<Expression,const char*>>{
            {{R"(ref("grid-rows-expression-source","","grid.columns"))",1},"GRID_ROWS_EXPRESSION_TYPE"},
            {{R"(ref("grid-rows-expression-source","point","grid.rows"))",1},"GRID_ROWS_EXPRESSION_TYPE"},
            {{R"(ref("grid-rows-expression-foreign","","grid.rows"))",1},"WRONG_COMPOSITION"},
            {{R"(ref("grid-rows-expression-missing","","grid.rows"))",1},"MISSING_GRID"},
            {{R"(ref("grid-rows-expression-target","","grid.rows"))",1},"GRID_ROWS_SELF_LINK"},
            {{"1",2},"UNSUPPORTED_EXPRESSION_VERSION"},{{"1du",1},"EXPRESSION_SYNTAX"},
            {{"(",1},"EXPRESSION_SYNTAX"},{{"3.5",1},"OUT_OF_RANGE"},{{"0",1},"OUT_OF_RANGE"},
            {{"1001",1},"OUT_OF_RANGE"},{{"1 / 0",1},"EXPRESSION_DOMAIN"},{{"1000",1},"INVALID_LAYOUT"}})
        rejects(invalid.second,[&]{apply({expression_command(invalid.first,true)});});
    auto deep_document=empty_document("grid-rows-depth-doc","grid-rows-depth-comp","grid-rows-depth-board-0");
    constexpr std::size_t depth_nodes=130;
    for(std::size_t index=0;index<depth_nodes;++index) {
        const auto grid_id="grid-rows-depth-grid-"+std::to_string(index);
        const auto board_id="grid-rows-depth-board-"+std::to_string(index);
        Artboard* board=nullptr;
        if(index==0)board=&deep_document.compositions.front().artboards.front();
        else {
            deep_document.compositions.front().artboards.push_back(
                {board_id,"Depth "+std::to_string(index),0,0,400,300});
            board=&deep_document.compositions.front().artboards.back();
        }
        ArtboardLayout layout;layout.grid=Grid{grid_id,{0,0,400,300},2,2,0,0};
        if(index+1<depth_nodes)
            layout.grid->rows_expression=Expression{"ref(\"grid-rows-depth-grid-"+
                std::to_string(index+1)+"\",\"\",\"grid.rows\") + 0",1};
        board->layout=std::move(layout);
    }
    rejects("DEPENDENCY_DEPTH",[&]{Session deep_session(deep_document);});
    rejects("DEPENDENCY_CYCLE",[&]{apply({GridRowsCommand{LinkGridRows{source,target,false}}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-rows-expression-source-board"}});});
    auto direct=*find_board("grid-rows-expression-target-board").layout;direct.grid->rows=7;
    rejects("DRIVEN_GRID_ROWS",[&]{apply({SetArtboardLayout{composition,
        "grid-rows-expression-target-board",direct}});});
    auto smuggled=*find_board("grid-rows-expression-target-board").layout;
    smuggled.grid->rows_expression=Expression{"5",1};
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,
        "grid-rows-expression-target-board",smuggled}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{Artboard injected{"grid-rows-expression-smuggled-board",
        "Smuggled expression",0,0,400,300};ArtboardLayout injected_layout;
        injected_layout.grid=Grid{"grid-rows-expression-smuggled-grid",{0,0,400,300},2,2,0,0};
        injected_layout.grid->rows_expression=expression;injected.layout=injected_layout;
        apply({AddArtboard{composition,injected,session.document().compositions.front().artboards.size()}});});
    rejects("DUPLICATE_TARGET",[&]{apply({expression_command(expression,true),GridRowsCommand{UnlinkGridRows{target}}});});
    rejects("INVALID_GUIDE",[&]{apply({GridRowsCommand{UnlinkGridRows{target}},
        AddGuide{composition,{"grid-rows-expression-bad-guide","Bad axis","z",25}}});});
    check(session.revision()==stable_revision_before_rejections&&session.history()==stable_history&&
        encode(session.document())==stable_bytes&&std::get<std::size_t>(artboard_layout_property(session.document(),target).evaluated)==4,
        "Invalid expression type/range/unit/syntax/version, cycles, deletion, injection and failed batches preserve exact Session state");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"rows_expression\":{\"source\":\"ref")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.60 retains the exact Grid rows expression and byte-roundtrips");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.58\"")==1,
        "Grid rows 0.58 version-lie fixture downgrades only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"rows_expression\":{\"source\":",
        "\"rows_expression\":{\"extra\":true,\"source\":")==1,
        "Malformed Grid rows expression fixture adds one unknown field");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto link_and_expression=native;check(replace_all(link_and_expression,"\"rows_expression\":{\"source\":",
        "\"rows_driver\":{\"link\":{\"object\":\"grid-rows-expression-source\",\"point\":\"\",\"field\":\"grid.rows\"}},\"rows_expression\":{\"source\":")==1,
        "Grid rows conflict fixture adds the existing link beside the expression");
    rejects("GRID_ROWS_SOURCE_CONFLICT",[&]{(void)decode(link_and_expression);});
    auto legacy_document=session.document();
    std::find_if(legacy_document.compositions.front().artboards.begin(),legacy_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-rows-expression-target-board";})->layout->grid->rows_expression.reset();
    auto previous=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_document));
    check(replace_all(previous,"\"version\":\"0.66\"","\"version\":\"0.58\"")==1&&
        decode(previous)==legacy_document,"Native 0.60 remains readable without the new optional Grid rows expression");

    apply({GridRowsCommand{UnlinkGridRows{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==4&&!typed.driver&&!typed.expression&&
        std::get<std::size_t>(typed.evaluated)==4,"Unlink freezes expression-evaluated rows 4 into the literal");
    source_update=find_board("grid-rows-expression-source-board");source_update.layout->grid->rows=4;
    apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==4&&!typed.expression&&std::get<std::size_t>(typed.evaluated)==4,
        "An upstream count change after unlink cannot alter the frozen row count");
    session.undo(session.revision());session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.expression==expression&&
        std::get<std::size_t>(typed.evaluated)==4,"Undo restores the exact expression and original authored literal");
    session.redo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==4&&!typed.expression&&std::get<std::size_t>(typed.evaluated)==4,
        "Redo restores the frozen evaluated integer");
}

void grid_rows_same_field_link() {
    auto document=empty_document("grid-rows-doc","grid-rows-comp","grid-rows-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    Artboard source_board{"grid-rows-source-board","Source",0,0,400,300};
    Artboard alternate_board{"grid-rows-alternate-board","Alternate",0,0,400,300};
    ArtboardLayout target_layout;target_layout.grid=Grid{"grid-rows-target-grid",{40,40,880,500},2,2,20,20};
    ArtboardLayout source_layout;source_layout.grid=Grid{"grid-rows-source",{0,0,400,300},2,2,0,0};
    ArtboardLayout alternate_layout;alternate_layout.grid=Grid{"grid-rows-alternate",{0,0,400,300},4,3,0,0};
    target_board.layout=target_layout;source_board.layout=source_layout;alternate_board.layout=alternate_layout;
    document.compositions.front().artboards.push_back(source_board);
    document.compositions.front().artboards.push_back(alternate_board);
    Artboard foreign_board{"grid-rows-foreign-board","Foreign",0,0,400,300};
    ArtboardLayout foreign_layout;foreign_layout.grid=Grid{"grid-rows-foreign",{0,0,400,300},5,5,0,0};
    foreign_board.layout=foreign_layout;
    document.compositions.push_back({"grid-rows-other-comp","Other plane",{}, {foreign_board}});

    const Id composition="grid-rows-comp";
    const Ref target{"grid-rows-target-grid","","grid.rows"};
    const Ref source{"grid-rows-source","","grid.rows"};
    const Ref alternate{"grid-rows-alternate","","grid.rows"};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    auto cell_height=[&] {
        const auto resolved=evaluate_artboard(session.document().compositions.front(),"grid-rows-target");
        const auto& grid=*resolved.layout->grid;
        return (grid.bounds.height-static_cast<double>(grid.rows-1)*grid.row_gutter)/grid.rows;
    };

    const auto linked_json=request(session,R"({"op":"apply","expected_revision":0,"commands":[
      {"type":"link_grid_rows","target":{"object":"grid-rows-target-grid","point":"","field":"grid.rows"},
       "source":{"object":"grid-rows-source","point":"","field":"grid.rows"},"replace_driver":false}
    ]})");
    check(linked_json.find("\"ok\":true")!=std::string::npos&&session.revision()==1,
        "JSON Session links Grid rows through its revisioned same-field command");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.driver==source&&typed.source_kind=="link"&&
        std::get<std::size_t>(typed.evaluated)==2&&cell_height()==240&&
        find_board("grid-rows-target").layout->grid->id==target.object,
        "Grid rows begins at 2 with exact source and 240-unit target cells while retaining its literal and stable ID");
    const auto get_json=request(session,R"({"op":"get","ref":{"object":"grid-rows-target-grid","point":"","field":"grid.rows"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(get_json.find("\"type\":\"integer\"")!=std::string::npos&&
        get_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        get_json.find("\"literal\":2")!=std::string::npos&&get_json.find("\"evaluated\":2")!=std::string::npos&&
        get_json.find("\"field\":\"grid.rows\"")!=std::string::npos&&
        get_json.find("\"link\":true")!=std::string::npos&&get_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"object\":\"grid-rows-target-grid\"")!=std::string::npos,
        ("Typed get/properties expose integer literal, exact source, evaluated count and source capabilities: "+get_json).c_str());

    const auto linked_revision=session.revision();
    apply({GridRowsCommand{LinkGridRows{target,source,false}}});
    check(session.revision()==linked_revision,"Repeating the exact Grid rows source is idempotent");
    rejects("DRIVEN_GRID_ROWS",[&]{apply({GridRowsCommand{LinkGridRows{target,alternate,false}}});});
    apply({GridRowsCommand{LinkGridRows{target,alternate,true}}});
    check(artboard_layout_property(session.document(),target).driver==alternate&&
        std::abs(cell_height()-460.0/3.0)<1e-9,
        "Explicit replacement selects a distinct Grid source and evaluates three rows");
    apply({GridRowsCommand{LinkGridRows{target,source,true}}});

    auto source_update=find_board("grid-rows-source-board");source_update.layout->grid->rows=3;
    apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.driver==source&&
        std::get<std::size_t>(typed.evaluated)==3&&std::abs(cell_height()-460.0/3.0)<1e-9,
        "Changing source count reevaluates cell height without changing the target literal");
    auto target_update=find_board("grid-rows-target");target_update.name="Renamed target";target_update.x=120;
    auto renamed_source=find_board("grid-rows-source-board");renamed_source.name="Renamed source";renamed_source.x=50;
    auto sibling_layout=*target_update.layout;sibling_layout.grid->column_gutter=10;
    target_update.layout=sibling_layout;
    apply({SetArtboardLayout{composition,target_update.id,sibling_layout},UpdateArtboard{composition,target_update},
        UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"grid-rows-alternate-board","grid-rows-source-board","grid-rows-target"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&std::get<std::size_t>(typed.evaluated)==3&&
        find_board("grid-rows-target").layout->grid->id==target.object&&
        find_board("grid-rows-target").layout->grid->column_gutter==10&&
        find_board("grid-rows-source-board").name=="Renamed source"&&std::abs(cell_height()-460.0/3.0)<1e-9,
        "Sibling layout edits, source rename/move and Artboard reorder preserve the stable Grid Ref");

    auto direct=*find_board("grid-rows-target").layout;direct.grid->rows=7;
    rejects("DRIVEN_GRID_ROWS",[&]{apply({SetArtboardLayout{composition,"grid-rows-target",direct}});});
    auto cleared=*find_board("grid-rows-target").layout;cleared.grid.reset();
    rejects("DRIVEN_GRID_ROWS",[&]{apply({SetArtboardLayout{composition,"grid-rows-target",cleared}});});
    auto smuggled=*find_board("grid-rows-target").layout;smuggled.grid->rows_driver=alternate;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-rows-target",smuggled}});});
    target_update=find_board("grid-rows-target");target_update.layout->grid->rows=7;
    rejects("DRIVEN_GRID_ROWS",[&]{apply({UpdateArtboard{composition,target_update}});});
    auto injected=Artboard{"grid-rows-injected-board","Injected",0,0,400,300};
    ArtboardLayout injected_layout;injected_layout.grid=Grid{"grid-rows-injected",{0,0,400,300},2,2,0,0};
    injected_layout.grid->rows_driver=source;injected.layout=injected_layout;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({AddArtboard{composition,injected,3}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridRowsCommand{LinkGridRows{{target.object,"point","grid.rows"},source,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridRowsCommand{LinkGridRows{{target.object,"","grid.columns"},source,true}}});});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{"grid-rows-source","point","grid.rows"},"INVALID_GRID_ROWS_REF"},
            {{"grid-rows-source","","grid.columns"},"INVALID_GRID_ROWS_REF"},
            {{"grid-rows-target-grid","","grid.rows"},"GRID_ROWS_SELF_LINK"},
            {{"grid-rows-foreign","","grid.rows"},"WRONG_COMPOSITION"},
            {{"grid-rows-missing","","grid.rows"},"MISSING_GRID"}})
        rejects(bad.second.c_str(),[&]{apply({GridRowsCommand{LinkGridRows{target,bad.first,true}}});});
    rejects("MISSING_GRID",[&]{apply({GridRowsCommand{UnlinkGridRows{{"grid-rows-missing","","grid.rows"}}}});});
    rejects("DUPLICATE_TARGET",[&]{apply({GridRowsCommand{UnlinkGridRows{target}},
        GridRowsCommand{LinkGridRows{target,source,true}}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({GridRowsCommand{UnlinkGridRows{target}}},session.revision()-1);});
    rejects("DEPENDENCY_CYCLE",[&]{apply({GridRowsCommand{LinkGridRows{source,target,false}}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-rows-source-board"}});});
    auto impossible_layout=*find_board("grid-rows-target").layout;
    impossible_layout.grid->row_gutter=250;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{composition,"grid-rows-target",impossible_layout}});});
    const auto bytes_before=encode(session.document());const auto revision_before=session.revision();const auto history_before=session.history();
    source_update=find_board("grid-rows-source-board");source_update.layout->grid->rows=1000;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});});
    rejects("INVALID_GUIDE",[&]{apply({GridRowsCommand{UnlinkGridRows{target}},
        AddGuide{composition,{"grid-rows-bad-guide","Bad axis","z",25}}});});
    check(session.revision()==revision_before&&session.history()==history_before&&encode(session.document())==bytes_before&&
        std::get<std::size_t>(artboard_layout_property(session.document(),target).evaluated)==3,
        "Invalid refs, replacement, cycles, source removal, count/geometry changes, stale revisions and later batch failure preserve state atomically");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"rows_driver\":{\"link\"")!=std::string::npos&&encode(decode(native))==native,
        "Native 0.60 retains the closed Grid rows link beside its authored integer");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.56\"")==1,
        "Grid rows 0.56 version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"rows_driver\":{\"link\":",
        "\"rows_driver\":{\"extra\":1,\"link\":")==1,"Malformed Grid rows driver adds an unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto literal_document=session.document();
    auto& literal_grid=*std::find_if(literal_document.compositions.front().artboards.begin(),literal_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-rows-target";})->layout->grid;
    literal_grid.rows_driver.reset();auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.56\"")==1&&decode(legacy)==literal_document,
        "Native 0.56 remains readable when the optional Grid rows source is absent");

    apply({GridRowsCommand{UnlinkGridRows{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==3&&!typed.driver&&std::get<std::size_t>(typed.evaluated)==3,
        "Unlink freezes the evaluated Grid row count into the authored literal in one revision");
    source_update=find_board("grid-rows-source-board");source_update.layout->grid->rows=4;
    apply({SetArtboardLayout{composition,source_update.id,source_update.layout}});typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==3&&!typed.driver&&std::get<std::size_t>(typed.evaluated)==3,
        "After unlink, later source count edits do not change the frozen target");
    session.undo(session.revision());session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==2&&typed.driver==source&&std::get<std::size_t>(typed.evaluated)==3,
        "Undo restores the exact Grid rows link and authored literal");
    session.redo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<std::size_t>(typed.literal)==3&&!typed.driver&&std::get<std::size_t>(typed.evaluated)==3,
        "Redo restores the frozen evaluated row count");
}

void grid_row_gutter_artboard_link() {
    auto document=empty_document("grid-row-gutter-doc","grid-row-gutter-comp","grid-row-gutter-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-row-gutter-other-comp","Other plane",{},
        {{"grid-row-gutter-other","Other frame",0,0,400,300}}});
    const Id composition="grid-row-gutter-comp";
    const Ref target{"grid-row-gutter-target-grid","","grid.row_gutter"};
    const Ref source{"grid-row-gutter-source","","artboard.height"};
    const Ref alternate{"grid-row-gutter-alternate","","artboard.height"};
    const Ref upstream_ref{"grid-row-gutter-upstream","","artboard.height"};
    Artboard source_board{"grid-row-gutter-source","Source frame",0,0,100,20};
    source_board.parent_size=ArtboardParent{"grid-row-gutter-upstream",false,true};
    Artboard alternate_board{"grid-row-gutter-alternate","Alternate frame",0,0,100,40};
    Artboard upstream_board{"grid-row-gutter-upstream","Upstream frame",0,0,100,20};
    ArtboardLayout layout;layout.grid=Grid{"grid-row-gutter-target-grid",{40,40,880,500},2,2,0,20};
    Contour outline;outline.id="grid-row-gutter-outline";outline.closed=true;
    for(const auto& [x,y]:std::array<std::pair<double,double>,4>{{{10,10},{30,10},{30,30},{10,30}}}) {
        Point point;point.id="grid-row-gutter-point-"+std::to_string(outline.points.size());
        point.x.literal=x;point.y.literal=y;outline.points.push_back(point);
    }
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,alternate_board,2},
        AddArtboard{composition,upstream_board,3},SetArtboardLayout{composition,"grid-row-gutter-target",layout},
        CreatePath{composition,"","grid-row-gutter-shape","Grid row gutter independence",{outline}}});
    const auto artwork_before=evaluate(session.document());
    const auto no_grid_document=empty_document("no-row-grid-doc","no-row-grid-comp","no-row-grid-target");
    Session no_grid_session(no_grid_document);
    no_grid_session.apply({AddArtboard{"no-row-grid-comp",Artboard{"no-row-grid-source","Source",0,0,100,40},1}},0);
    rejects("MISSING_GRID",[&]{no_grid_session.apply({GridRowGutterCommand{LinkGridRowGutter{
        {"no-row-grid-target-grid","","grid.row_gutter"},{"no-row-grid-source","","artboard.height"},false}}},
        no_grid_session.revision());});

    auto smuggled=layout;smuggled.grid->row_gutter_driver=source;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-target",smuggled}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{
        auto board=Artboard{"grid-row-gutter-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected;injected.grid=Grid{"grid-row-gutter-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected.grid->row_gutter_driver=source;board.layout=injected;
        apply({AddArtboard{composition,board,4}});
    });

    const auto json_link=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_grid_row_gutter","target":{"object":"grid-row-gutter-target-grid","point":"","field":"grid.row_gutter"},
       "source":{"object":"grid-row-gutter-source","point":"","field":"artboard.height"},"replace_driver":false}
    ]})");
    check(json_link.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session links Grid row gutter through its dedicated revisioned Artboard-size command");
    auto typed=artboard_layout_property(session.document(),target);
    auto evaluated=evaluate_artboard(session.document().compositions.front(),"grid-row-gutter-target");
    auto cell_height=(evaluated.layout->grid->bounds.height-
        (evaluated.layout->grid->rows-1)*evaluated.layout->grid->row_gutter)/evaluated.layout->grid->rows;
    check(std::get<double>(typed.literal)==20&&typed.driver==source&&typed.source_kind=="link"&&
        std::get<double>(typed.evaluated)==20&&evaluated.layout->grid->row_gutter==20&&cell_height==240&&
        find_board("grid-row-gutter-target").layout->grid->id==target.object,
        "Grid row gutter evaluates the Artboard height while retaining its literal and stable Grid ID");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-row-gutter-target-grid","point":"","field":"grid.row_gutter"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("\"field\":\"artboard.height\"")!=std::string::npos&&
        typed_json.find("\"literal\":2E1")!=std::string::npos&&typed_json.find("\"evaluated\":2E1")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"field\":\"grid.row_gutter\"")!=std::string::npos,
        ("Typed get and properties disclose the authored row gutter, exact source, evaluated value and link capability: "+
            typed_json+" | "+properties_json).c_str());
    const auto linked_revision=session.revision();
    apply({GridRowGutterCommand{LinkGridRowGutter{target,source,false}}});
    check(session.revision()==linked_revision,"Repeating the exact Grid row gutter source is idempotent");
    rejects("DRIVEN_GRID_ROW_GUTTER",[&]{apply({GridRowGutterCommand{LinkGridRowGutter{target,alternate,false}}});});
    apply({GridRowGutterCommand{LinkGridRowGutter{target,alternate,true}}});
    check(artboard_layout_property(session.document(),target).driver==alternate&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==40,
        "Explicit replacement changes Grid row gutter to a distinct Artboard height source");
    apply({GridRowGutterCommand{LinkGridRowGutter{target,source,true}}});

    auto upstream=find_board("grid-row-gutter-upstream");upstream.height=30;
    apply({UpdateArtboard{composition,upstream}});
    typed=artboard_layout_property(session.document(),target);evaluated=evaluate_artboard(session.document().compositions.front(),"grid-row-gutter-target");
    cell_height=(evaluated.layout->grid->bounds.height-evaluated.layout->grid->row_gutter)/2;
    check(std::get<double>(typed.literal)==20&&typed.driver==source&&std::get<double>(typed.evaluated)==30&&
        evaluated.layout->grid->row_gutter==30&&cell_height==235&&evaluate(session.document())==artwork_before,
        "Upstream resize changes the evaluated gutter and row cell without reflowing authored Objects");
    const auto bytes_before_invalid=encode(session.document());const auto revision_before_invalid=session.revision();
    const auto history_before_invalid=session.history();upstream=find_board("grid-row-gutter-upstream");upstream.height=500;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream}});});
    auto short_grid_layout=*find_board("grid-row-gutter-target").layout;short_grid_layout.grid->bounds.height=30;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-target",short_grid_layout}});});
    check(session.revision()==revision_before_invalid&&session.history()==history_before_invalid&&
        encode(session.document())==bytes_before_invalid&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==30,
        "Zero-height evaluated row cells reject source and Grid-height edits atomically");

    auto target_layout=*find_board("grid-row-gutter-target").layout;target_layout.grid->bounds.x=60;target_layout.grid->column_gutter=10;
    apply({SetArtboardLayout{composition,"grid-row-gutter-target",target_layout}});
    auto target_update=find_board("grid-row-gutter-target");target_update.name="Moved target";target_update.x=120;
    auto renamed_source=find_board("grid-row-gutter-source");renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,target_update},UpdateArtboard{composition,renamed_source},
        ReorderArtboards{composition,{"grid-row-gutter-upstream","grid-row-gutter-alternate","grid-row-gutter-source","grid-row-gutter-target"}}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==20&&typed.driver==source&&std::get<double>(typed.evaluated)==30&&
        find_board("grid-row-gutter-target").layout->grid->id==target.object&&find_board("grid-row-gutter-target").layout->grid->bounds.x==60&&
        find_board("grid-row-gutter-target").x==120&&find_board("grid-row-gutter-source").name=="Renamed source"&&
        find_board("grid-row-gutter-source").x==50,
        "Sibling edits, source rename/move and Artboard reorder preserve the exact stable source Ref");

    const auto bytes_before=encode(session.document());const auto rev_before=session.revision();const auto history_before=session.history();
    auto direct=*find_board("grid-row-gutter-target").layout;direct.grid->row_gutter=21;
    rejects("DRIVEN_GRID_ROW_GUTTER",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-target",direct}});});
    direct=*find_board("grid-row-gutter-target").layout;direct.grid->row_gutter_driver=alternate;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-target",direct}});});
    target_update=find_board("grid-row-gutter-target");target_update.layout->grid->row_gutter=21;
    rejects("DRIVEN_GRID_ROW_GUTTER",[&]{apply({UpdateArtboard{composition,target_update}});});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{"grid-row-gutter-source","point","artboard.height"},"INVALID_ARTBOARD_REF"},
            {{"grid-row-gutter-source","","grid.row_gutter"},"INVALID_ARTBOARD_REF"},
            {{"grid-row-gutter-target","","artboard.height"},"GRID_SELF_LINK"},
            {{"grid-row-gutter-comp","","artboard.height"},"TYPE_MISMATCH"},
            {{"grid-row-gutter-other","","artboard.height"},"WRONG_COMPOSITION"},
            {{"grid-row-gutter-missing","","artboard.height"},"MISSING_ARTBOARD"}})
        rejects(bad.second.c_str(),[&]{apply({GridRowGutterCommand{LinkGridRowGutter{target,bad.first,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridRowGutterCommand{LinkGridRowGutter{{target.object,"point","grid.row_gutter"},source,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridRowGutterCommand{LinkGridRowGutter{{target.object,"","grid.bounds.x"},source,true}}});});
    rejects("MISSING_GRID",[&]{apply({GridRowGutterCommand{UnlinkGridRowGutter{{"grid-row-gutter-missing-grid","","grid.row_gutter"}}}});});
    rejects("DUPLICATE_TARGET",[&]{apply({GridRowGutterCommand{UnlinkGridRowGutter{target}},
        GridRowGutterCommand{LinkGridRowGutter{target,source,true}}});});
    const auto stale=session.revision();
    rejects("REVISION_CONFLICT",[&]{session.apply({GridRowGutterCommand{UnlinkGridRowGutter{target}}},stale-1);});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-row-gutter-source"}});});
    rejects("INVALID_GUIDE",[&]{apply({GridRowGutterCommand{UnlinkGridRowGutter{target}},
        AddGuide{composition,{"grid-row-gutter-bad-guide","Bad axis","z",25}}});});
    check(session.revision()==rev_before&&session.history()==history_before&&encode(session.document())==bytes_before&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==30,
        "Deletion, direct edits, stale revisions and failed batches preserve bytes, evaluation, revision and history");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"row_gutter_driver\":{\"link\"")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.60 retains the closed Grid row gutter link beside its authored literal");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.51\"")==1,
        "Grid row gutter version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"row_gutter_driver\":{\"link\":",
        "\"row_gutter_driver\":{\"extra\":1,\"link\":")==1,
        "Malformed Grid row gutter driver fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto literal_document=session.document();
    auto& literal_grid=*std::find_if(literal_document.compositions.front().artboards.begin(),literal_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-row-gutter-target";})->layout->grid;
    literal_grid.row_gutter_driver.reset();
    auto native_051=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(native_051,"\"version\":\"0.66\"","\"version\":\"0.51\"")==1&&
        decode(native_051)==literal_document,"Native 0.51 remains readable when the optional row gutter link is absent");

    apply({GridRowGutterCommand{UnlinkGridRowGutter{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==30&&!typed.driver&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==30,"Unlink freezes evaluated Grid row gutter into its authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==20&&typed.driver==source&&std::get<double>(typed.evaluated)==30,
        "Undo restores the exact Grid row gutter source and original literal");
    session.redo(session.revision());upstream=find_board("grid-row-gutter-upstream");upstream.height=10;
    apply({UpdateArtboard{composition,upstream}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==30&&!typed.driver&&std::get<double>(typed.evaluated)==30,
        "Redo keeps the frozen row gutter independent from later Artboard-size changes");
}

void grid_row_gutter_expression() {
    auto document=empty_document("grid-row-gutter-expression-doc","grid-row-gutter-expression-comp",
        "grid-row-gutter-expression-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-row-gutter-expression-other-comp","Other plane",{},
        {{"grid-row-gutter-expression-other","Other frame",0,0,400,300}}});
    const Id composition="grid-row-gutter-expression-comp";
    const Ref target{"grid-row-gutter-expression-target-grid","","grid.row_gutter"};
    const Ref source{"grid-row-gutter-expression-source","","artboard.height"};
    const Ref alternate{"grid-row-gutter-expression-alternate","","artboard.height"};
    const Ref upstream{"grid-row-gutter-expression-upstream","","artboard.height"};
    const Expression expression{R"(ref("grid-row-gutter-expression-source","","artboard.height") + 10)",1};
    Artboard source_board{"grid-row-gutter-expression-source","Source frame",0,0,100,10};
    source_board.parent_size=ArtboardParent{"grid-row-gutter-expression-upstream",false,true};
    Artboard alternate_board{"grid-row-gutter-expression-alternate","Alternate frame",0,0,100,40};
    Artboard upstream_board{"grid-row-gutter-expression-upstream","Upstream frame",0,0,100,10};
    ArtboardLayout layout;layout.grid=Grid{"grid-row-gutter-expression-target-grid",{40,40,880,500},2,2,0,20};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,alternate_board,2},
        AddArtboard{composition,upstream_board,3},SetArtboardLayout{composition,"grid-row-gutter-expression-target",layout}});
    const auto expression_command=[&](Expression value,bool replace=false) {
        return GridRowGutterCommand{SetGridRowGutterExpression{target,std::move(value),replace}};
    };
    const auto json_command=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_grid_row_gutter_expression","target":{"object":"grid-row-gutter-expression-target-grid","point":"","field":"grid.row_gutter"},
       "expression":{"source":"ref(\"grid-row-gutter-expression-source\",\"\",\"artboard.height\") + 10","version":1},"replace_driver":false}
    ]})");
    check(json_command.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session accepts the revisioned Grid row gutter expression command");
    auto typed=artboard_layout_property(session.document(),target);
    auto resolved=evaluate_artboard(session.document().compositions.front(),"grid-row-gutter-expression-target");
    auto cell_height=(resolved.layout->grid->bounds.height-
        (resolved.layout->grid->rows-1)*resolved.layout->grid->row_gutter)/resolved.layout->grid->rows;
    check(std::get<double>(typed.literal)==20&&!typed.driver&&typed.expression==expression&&
        typed.source_kind=="expression"&&std::get<double>(typed.evaluated)==20&&
        resolved.layout->grid->row_gutter==20&&cell_height==240&&
        find_board("grid-row-gutter-expression-target").layout->grid->id==target.object,
        "Grid row gutter expression keeps its literal, exact source and stable Grid ID while producing positive row cells");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-row-gutter-expression-target-grid","point":"","field":"grid.row_gutter"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("grid-row-gutter-expression-source")!=std::string::npos&&
        typed_json.find("\"literal\":2E1")!=std::string::npos&&typed_json.find("\"evaluated\":2E1")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"field\":\"grid.row_gutter\"")!=std::string::npos&&
        properties_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        ("Typed Grid row gutter get and properties expose exact expression, authored value and evaluated value: "+
            typed_json+" | "+properties_json).c_str());
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({expression_command(expression)});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the exact Grid row gutter expression is idempotent");
    rejects("DRIVEN_GRID_ROW_GUTTER",[&]{apply({GridRowGutterCommand{LinkGridRowGutter{target,source,false}}});});
    apply({GridRowGutterCommand{LinkGridRowGutter{target,source,true}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&!typed.expression&&std::get<double>(typed.evaluated)==10,
        "Explicit replacement changes Grid row gutter expression to the Artboard height link");
    rejects("DRIVEN_GRID_ROW_GUTTER",[&]{apply({expression_command(expression)});});
    apply({expression_command(expression,true)});

    auto upstream_update=find_board("grid-row-gutter-expression-upstream");upstream_update.height=20;
    apply({UpdateArtboard{composition,upstream_update}});
    typed=artboard_layout_property(session.document(),target);resolved=evaluate_artboard(
        session.document().compositions.front(),"grid-row-gutter-expression-target");
    cell_height=(resolved.layout->grid->bounds.height-resolved.layout->grid->row_gutter)/2.0;
    check(std::get<double>(typed.literal)==20&&!typed.driver&&typed.expression==expression&&
        std::get<double>(typed.evaluated)==30&&resolved.layout->grid->row_gutter==30&&cell_height==235,
        "Parent-driven Artboard height reevaluates row gutter and cell geometry without changing authored gutter");
    auto sibling_layout=*find_board("grid-row-gutter-expression-target").layout;
    sibling_layout.grid->bounds.x=60;sibling_layout.grid->column_gutter=10;
    apply({SetArtboardLayout{composition,"grid-row-gutter-expression-target",sibling_layout}});
    auto renamed_source=find_board("grid-row-gutter-expression-source");
    renamed_source.name="Renamed expression source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed_source},ReorderArtboards{composition,
        {"grid-row-gutter-expression-upstream","grid-row-gutter-expression-alternate",
         "grid-row-gutter-expression-source","grid-row-gutter-expression-target"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==20&&std::get<double>(typed.evaluated)==30&&
        find_board("grid-row-gutter-expression-target").layout->grid->id==target.object&&
        find_board("grid-row-gutter-expression-target").layout->grid->bounds.x==60&&
        find_board("grid-row-gutter-expression-source").name=="Renamed expression source",
        "Sibling edits and source rename/reorder preserve exact expression text and stable Grid identity");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();
    const auto stable_history=session.history();
    auto invalid=[&](Expression bad,const char* description) {
        bool did_reject=false;
        try {apply({expression_command(std::move(bad),true)});} catch(const Error&) {did_reject=true;}
        check(did_reject,description);
        check(session.revision()==stable_revision&&session.history()==stable_history&&
            encode(session.document())==stable_document,
            "Rejected Grid row gutter expression preserves Document, native bytes, revision and history");
    };
    invalid({"ref(",1},"Malformed Grid row gutter expression is rejected");
    invalid({expression.source,0},"Unsupported Grid row gutter expression version is rejected");
    invalid({std::string(33,'(')+"1"+std::string(33,')'),1},"Grid row gutter expression enforces bounded nesting");
    rejects("UNIT_MISMATCH",[&]{apply({expression_command({
        R"expr(ref("grid-row-gutter-expression-source","","artboard.height") + ref("object","","transform.rotation"))expr",1},true)});});
    rejects("GRID_ROW_GUTTER_EXPRESSION_TYPE",[&]{apply({expression_command({
        R"(ref("grid-row-gutter-expression-source","point","artboard.height"))",1},true)});});
    rejects("GRID_ROW_GUTTER_EXPRESSION_TYPE",[&]{apply({expression_command({
        R"(ref("grid-row-gutter-expression-source","","grid.row_gutter"))",1},true)});});
    rejects("GRID_SELF_LINK",[&]{apply({expression_command({
        R"(ref("grid-row-gutter-expression-target","","artboard.height"))",1},true)});});
    rejects("WRONG_COMPOSITION",[&]{apply({expression_command({
        R"(ref("grid-row-gutter-expression-other","","artboard.height"))",1},true)});});
    rejects("MISSING_ARTBOARD",[&]{apply({expression_command({
        R"(ref("grid-row-gutter-expression-missing","","artboard.height"))",1},true)});});
    rejects("TYPE_MISMATCH",[&]{apply({expression_command({
        R"(ref("grid-row-gutter-expression-comp","","artboard.height"))",1},true)});});
    rejects("EXPRESSION_DOMAIN",[&]{apply({expression_command({"1 / 0",1},true)});});
    rejects("DUPLICATE_TARGET",[&]{apply({expression_command(expression,true),
        GridRowGutterCommand{UnlinkGridRowGutter{target}}});});
    auto payload=*find_board("grid-row-gutter-expression-target").layout;payload.grid->row_gutter=21;
    rejects("DRIVEN_GRID_ROW_GUTTER",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-expression-target",payload}});});
    payload=*find_board("grid-row-gutter-expression-target").layout;payload.grid->id="grid-row-gutter-expression-replaced";
    rejects("DRIVEN_GRID_ROW_GUTTER",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-expression-target",payload}});});
    payload=*find_board("grid-row-gutter-expression-target").layout;payload.grid.reset();
    rejects("DRIVEN_GRID_ROW_GUTTER",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-expression-target",payload}});});
    payload=*find_board("grid-row-gutter-expression-target").layout;
    payload.grid->row_gutter_expression=Expression{"500",1};
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-expression-target",payload}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{Artboard injected{"grid-row-gutter-expression-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected_layout;injected_layout.grid=Grid{"grid-row-gutter-expression-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected_layout.grid->row_gutter_expression=expression;injected.layout=injected_layout;
        apply({AddArtboard{composition,injected,session.document().compositions.front().artboards.size()}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-row-gutter-expression-source"}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({expression_command(expression,true)},stable_revision-1);});
    rejects("INVALID_GUIDE",[&]{apply({expression_command(expression,true),
        AddGuide{composition,{"grid-row-gutter-expression-bad-guide","Bad axis","z",25}}});});
    for(const double height:{490.0,501.0}) {
        auto invalid_upstream=find_board("grid-row-gutter-expression-upstream");invalid_upstream.height=height;
        rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,invalid_upstream}});});
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
            "Zero or negative evaluated row-cell size leaves the expression state unchanged");
    }
    auto short_grid=*find_board("grid-row-gutter-expression-target").layout;short_grid.grid->bounds.height=30;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{composition,"grid-row-gutter-expression-target",short_grid}});});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "A Grid height edit leaving no positive row cell is rejected atomically");

    auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"row_gutter_expression\"")!=std::string::npos&&encode(decode(native))==native,
        "Native 0.60 preserves exact Grid row gutter expression source and roundtrips bytes");
    auto old_version=test_support::without_empty_presets_for_legacy_fixture(native);
    check(replace_all(old_version,"\"version\":\"0.66\"","\"version\":\"0.52\"")==1,
        "Grid row gutter expression version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(old_version);});
    auto malformed=native;
    check(replace_all(malformed,"\"row_gutter_expression\":{",
        "\"row_gutter_expression\":{\"extra\":true,")==1,
        "Malformed Grid row gutter expression fixture adds one unknown source field");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto conflicting=native;
    check(replace_all(conflicting,"\"row_gutter_expression\":{",
        "\"row_gutter_driver\":{\"link\":{\"object\":\"grid-row-gutter-expression-source\",\"point\":\"\",\"field\":\"artboard.height\"}},\"row_gutter_expression\":{")==1,
        "Grid row gutter schema fixture includes both mutually exclusive source kinds");
    rejects("GRID_SOURCE_CONFLICT",[&]{(void)decode(conflicting);});
    auto legacy_link_document=session.document();
    auto& legacy_grid=*std::find_if(legacy_link_document.compositions.front().artboards.begin(),
        legacy_link_document.compositions.front().artboards.end(),[](const Artboard& value){
            return value.id=="grid-row-gutter-expression-target";})->layout->grid;
    legacy_grid.row_gutter_expression.reset();legacy_grid.row_gutter_driver=source;
    auto native_052=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_link_document));
    check(replace_all(native_052,"\"version\":\"0.66\"","\"version\":\"0.52\"")==1&&
        decode(native_052)==legacy_link_document,
        "Native 0.52 Grid row gutter links remain readable when the new expression field is absent");

    apply({GridRowGutterCommand{UnlinkGridRowGutter{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==30&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==30,
        "Unlink freezes the evaluated expression gutter in one authored literal transition");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==20&&std::get<double>(typed.evaluated)==30,
        "Undo restores the exact row gutter expression and authored literal");
    session.redo(session.revision());upstream_update=find_board("grid-row-gutter-expression-upstream");
    upstream_update.height=15;apply({UpdateArtboard{composition,upstream_update}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==30&&!typed.expression&&std::get<double>(typed.evaluated)==30,
        "Redo keeps the frozen row gutter independent from later Artboard-size changes");
}
void grid_column_gutter_expression() {
    auto document=empty_document("grid-column-gutter-expression-doc","grid-column-gutter-expression-comp",
        "grid-column-gutter-expression-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-column-gutter-expression-other-comp","Other plane",{},
        {{"grid-column-gutter-expression-other","Other frame",0,0,400,300}}});
    const Id composition="grid-column-gutter-expression-comp";
    const Ref target{"grid-column-gutter-expression-target-grid","","grid.column_gutter"};
    const Ref source{"grid-column-gutter-expression-source","","artboard.width"};
    const Ref alternate{"grid-column-gutter-expression-alternate","","artboard.width"};
    const Ref upstream{"grid-column-gutter-expression-upstream","","artboard.width"};
    const Expression expression{R"(ref("grid-column-gutter-expression-source","","artboard.width") + 10)",1};
    Artboard source_board{"grid-column-gutter-expression-source","Source frame",0,0,100,10};
    source_board.parent_size=ArtboardParent{"grid-column-gutter-expression-upstream",true,false};
    Artboard alternate_board{"grid-column-gutter-expression-alternate","Alternate frame",0,0,100,40};
    Artboard upstream_board{"grid-column-gutter-expression-upstream","Upstream frame",0,0,10,100};
    ArtboardLayout layout;layout.grid=Grid{"grid-column-gutter-expression-target-grid",{40,40,880,500},2,2,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,alternate_board,2},
        AddArtboard{composition,upstream_board,3},SetArtboardLayout{composition,"grid-column-gutter-expression-target",layout}});
    const auto expression_command=[&](Expression value,bool replace=false) {
        return GridColumnGutterCommand{SetGridColumnGutterExpression{target,std::move(value),replace}};
    };
    const auto json_command=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_grid_column_gutter_expression","target":{"object":"grid-column-gutter-expression-target-grid","point":"","field":"grid.column_gutter"},
       "expression":{"source":"ref(\"grid-column-gutter-expression-source\",\"\",\"artboard.width\") + 10","version":1},"replace_driver":false}
    ]})");
    check(json_command.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session accepts the revisioned Grid column gutter expression command");
    auto typed=artboard_layout_property(session.document(),target);
    auto resolved=evaluate_artboard(session.document().compositions.front(),"grid-column-gutter-expression-target");
    auto cell_width=(resolved.layout->grid->bounds.width-
        (resolved.layout->grid->columns-1)*resolved.layout->grid->column_gutter)/resolved.layout->grid->columns;
    check(std::get<double>(typed.literal)==20&&!typed.driver&&typed.expression==expression&&
        typed.source_kind=="expression"&&std::get<double>(typed.evaluated)==20&&
        resolved.layout->grid->column_gutter==20&&cell_width==430&&
        find_board("grid-column-gutter-expression-target").layout->grid->id==target.object,
        "Grid column gutter expression keeps its literal, exact source and stable Grid ID while producing positive column cells");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-column-gutter-expression-target-grid","point":"","field":"grid.column_gutter"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("grid-column-gutter-expression-source")!=std::string::npos&&
        typed_json.find("\"literal\":2E1")!=std::string::npos&&typed_json.find("\"evaluated\":2E1")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"field\":\"grid.column_gutter\"")!=std::string::npos&&
        properties_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        ("Typed Grid column gutter get and properties expose exact expression, authored value and evaluated value: "+
            typed_json+" | "+properties_json).c_str());
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({expression_command(expression)});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the exact Grid column gutter expression is idempotent");
    rejects("DRIVEN_GRID_COLUMN_GUTTER",[&]{apply({GridColumnGutterCommand{LinkGridColumnGutter{target,source,false}}});});
    apply({GridColumnGutterCommand{LinkGridColumnGutter{target,source,true}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&!typed.expression&&std::get<double>(typed.evaluated)==10,
        "Explicit replacement changes Grid column gutter expression to the Artboard width link");
    rejects("DRIVEN_GRID_COLUMN_GUTTER",[&]{apply({expression_command(expression)});});
    apply({expression_command(expression,true)});

    auto upstream_update=find_board("grid-column-gutter-expression-upstream");upstream_update.width=20;
    apply({UpdateArtboard{composition,upstream_update}});
    typed=artboard_layout_property(session.document(),target);resolved=evaluate_artboard(
        session.document().compositions.front(),"grid-column-gutter-expression-target");
    cell_width=(resolved.layout->grid->bounds.width-resolved.layout->grid->column_gutter)/2.0;
    check(std::get<double>(typed.literal)==20&&!typed.driver&&typed.expression==expression&&
        std::get<double>(typed.evaluated)==30&&resolved.layout->grid->column_gutter==30&&cell_width==425,
        "Parent-driven Artboard width reevaluates column gutter and cell geometry without changing authored gutter");
    auto sibling_layout=*find_board("grid-column-gutter-expression-target").layout;
    sibling_layout.grid->bounds.x=60;sibling_layout.grid->row_gutter=10;
    apply({SetArtboardLayout{composition,"grid-column-gutter-expression-target",sibling_layout}});
    auto renamed_source=find_board("grid-column-gutter-expression-source");
    renamed_source.name="Renamed expression source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed_source},ReorderArtboards{composition,
        {"grid-column-gutter-expression-upstream","grid-column-gutter-expression-alternate",
         "grid-column-gutter-expression-source","grid-column-gutter-expression-target"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==20&&std::get<double>(typed.evaluated)==30&&
        find_board("grid-column-gutter-expression-target").layout->grid->id==target.object&&
        find_board("grid-column-gutter-expression-target").layout->grid->bounds.x==60&&
        find_board("grid-column-gutter-expression-source").name=="Renamed expression source",
        "Sibling edits and source rename/reorder preserve exact expression text and stable Grid identity");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();
    const auto stable_history=session.history();
    auto invalid=[&](Expression bad,const char* description) {
        bool did_reject=false;
        try {apply({expression_command(std::move(bad),true)});} catch(const Error&) {did_reject=true;}
        check(did_reject,description);
        check(session.revision()==stable_revision&&session.history()==stable_history&&
            encode(session.document())==stable_document,
            "Rejected Grid column gutter expression preserves Document, native bytes, revision and history");
    };
    invalid({"ref(",1},"Malformed Grid column gutter expression is rejected");
    invalid({expression.source,0},"Unsupported Grid column gutter expression version is rejected");
    invalid({std::string(33,'(')+"1"+std::string(33,')'),1},"Grid column gutter expression enforces bounded nesting");
    rejects("UNIT_MISMATCH",[&]{apply({expression_command({
        R"expr(ref("grid-column-gutter-expression-source","","artboard.width") + ref("object","","transform.rotation"))expr",1},true)});});
    rejects("GRID_COLUMN_GUTTER_EXPRESSION_TYPE",[&]{apply({expression_command({
        R"(ref("grid-column-gutter-expression-source","point","artboard.width"))",1},true)});});
    rejects("GRID_COLUMN_GUTTER_EXPRESSION_TYPE",[&]{apply({expression_command({
        R"(ref("grid-column-gutter-expression-source","","grid.column_gutter"))",1},true)});});
    rejects("GRID_SELF_LINK",[&]{apply({expression_command({
        R"(ref("grid-column-gutter-expression-target","","artboard.width"))",1},true)});});
    rejects("WRONG_COMPOSITION",[&]{apply({expression_command({
        R"(ref("grid-column-gutter-expression-other","","artboard.width"))",1},true)});});
    rejects("MISSING_ARTBOARD",[&]{apply({expression_command({
        R"(ref("grid-column-gutter-expression-missing","","artboard.width"))",1},true)});});
    rejects("TYPE_MISMATCH",[&]{apply({expression_command({
        R"(ref("grid-column-gutter-expression-comp","","artboard.width"))",1},true)});});
    rejects("EXPRESSION_DOMAIN",[&]{apply({expression_command({"1 / 0",1},true)});});
    rejects("DUPLICATE_TARGET",[&]{apply({expression_command(expression,true),
        GridColumnGutterCommand{UnlinkGridColumnGutter{target}}});});
    auto payload=*find_board("grid-column-gutter-expression-target").layout;payload.grid->column_gutter=21;
    rejects("DRIVEN_GRID_COLUMN_GUTTER",[&]{apply({SetArtboardLayout{composition,"grid-column-gutter-expression-target",payload}});});
    payload=*find_board("grid-column-gutter-expression-target").layout;payload.grid->id="grid-column-gutter-expression-replaced";
    rejects("DRIVEN_GRID_COLUMN_GUTTER",[&]{apply({SetArtboardLayout{composition,"grid-column-gutter-expression-target",payload}});});
    payload=*find_board("grid-column-gutter-expression-target").layout;payload.grid.reset();
    rejects("DRIVEN_GRID_COLUMN_GUTTER",[&]{apply({SetArtboardLayout{composition,"grid-column-gutter-expression-target",payload}});});
    payload=*find_board("grid-column-gutter-expression-target").layout;
    payload.grid->column_gutter_expression=Expression{"500",1};
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-column-gutter-expression-target",payload}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{Artboard injected{"grid-column-gutter-expression-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected_layout;injected_layout.grid=Grid{"grid-column-gutter-expression-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected_layout.grid->column_gutter_expression=expression;injected.layout=injected_layout;
        apply({AddArtboard{composition,injected,session.document().compositions.front().artboards.size()}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-column-gutter-expression-source"}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({expression_command(expression,true)},stable_revision-1);});
    rejects("INVALID_GUIDE",[&]{apply({expression_command(expression,true),
        AddGuide{composition,{"grid-column-gutter-expression-bad-guide","Bad axis","z",25}}});});
    for(const double width:{870.0,880.0}) {
        auto invalid_upstream=find_board("grid-column-gutter-expression-upstream");invalid_upstream.width=width;
        rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,invalid_upstream}});});
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
            "Zero or negative evaluated column-cell size leaves the expression state unchanged");
    }
    auto short_grid=*find_board("grid-column-gutter-expression-target").layout;short_grid.grid->bounds.width=30;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{composition,"grid-column-gutter-expression-target",short_grid}});});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
        "A Grid width edit leaving no positive column cell is rejected atomically");

    auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"column_gutter_expression\"")!=std::string::npos&&encode(decode(native))==native,
        "Native 0.60 preserves exact Grid column gutter expression source and roundtrips bytes");
    auto old_version=test_support::without_empty_presets_for_legacy_fixture(native);
    check(replace_all(old_version,"\"version\":\"0.66\"","\"version\":\"0.53\"")==1,
        "Grid column gutter expression version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(old_version);});
    auto malformed=native;
    check(replace_all(malformed,"\"column_gutter_expression\":{",
        "\"column_gutter_expression\":{\"extra\":true,")==1,
        "Malformed Grid column gutter expression fixture adds one unknown source field");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto conflicting=native;
    check(replace_all(conflicting,"\"column_gutter_expression\":{",
        "\"column_gutter_driver\":{\"link\":{\"object\":\"grid-column-gutter-expression-source\",\"point\":\"\",\"field\":\"artboard.width\"}},\"column_gutter_expression\":{")==1,
        "Grid column gutter schema fixture includes both mutually exclusive source kinds");
    rejects("GRID_SOURCE_CONFLICT",[&]{(void)decode(conflicting);});
    auto legacy_link_document=session.document();
    auto& legacy_grid=*std::find_if(legacy_link_document.compositions.front().artboards.begin(),
        legacy_link_document.compositions.front().artboards.end(),[](const Artboard& value){
            return value.id=="grid-column-gutter-expression-target";})->layout->grid;
    legacy_grid.column_gutter_expression.reset();legacy_grid.column_gutter_driver=source;
    auto native_052=test_support::without_empty_presets_for_legacy_fixture(encode(legacy_link_document));
    check(replace_all(native_052,"\"version\":\"0.66\"","\"version\":\"0.53\"")==1&&
        decode(native_052)==legacy_link_document,
        "Native 0.53 Grid column gutter links remain readable when the new expression field is absent");

    apply({GridColumnGutterCommand{UnlinkGridColumnGutter{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==30&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==30,
        "Unlink freezes the evaluated expression gutter in one authored literal transition");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==20&&std::get<double>(typed.evaluated)==30,
        "Undo restores the exact column gutter expression and authored literal");
    session.redo(session.revision());upstream_update=find_board("grid-column-gutter-expression-upstream");
    upstream_update.width=15;apply({UpdateArtboard{composition,upstream_update}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==30&&!typed.expression&&std::get<double>(typed.evaluated)==30,
        "Redo keeps the frozen column gutter independent from later Artboard-size changes");
}



void grid_bounds_height_artboard_link() {
    auto document=empty_document("grid-height-doc","grid-height-comp","grid-height-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-height-other-comp","Other plane",{},{{"grid-height-other","Other frame",0,0,400,300}}});
    const Id composition="grid-height-comp";
    const Ref target{"grid-height-target-grid","","grid.bounds.height"};
    const Ref source{"grid-height-source","","artboard.height"};
    const Ref alternate{"grid-height-alternate","","artboard.height"};
    Artboard source_board{"grid-height-source","Height source",0,0,100,500};
    Artboard alternate_board{"grid-height-alternate","Alternate source",0,0,100,510};
    ArtboardLayout layout;layout.grid=Grid{"grid-height-target-grid",{40,40,880,500},2,2,0,20};
    Contour align_outline;align_outline.id="grid-height-align-contour";align_outline.closed=true;
    for(const auto& [x,y]:std::array<std::pair<double,double>,4>{{{0,0},{10,0},{10,10},{0,10}}}) {
        Point point;point.id="grid-height-align-point-"+std::to_string(align_outline.points.size());
        point.x.literal=x;point.y.literal=y;align_outline.points.push_back(point);
    }
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id)->const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,alternate_board,2},
        SetArtboardLayout{composition,"grid-height-target",layout},
        CreatePath{composition,"","grid-height-align-shape","Align to linked Grid height",{align_outline}}});
    const auto no_grid_document=empty_document("no-grid-height-doc","no-grid-height-comp","no-grid-height-target");
    Session no_grid_session(no_grid_document);
    no_grid_session.apply({AddArtboard{"no-grid-height-comp",Artboard{"no-grid-height-source","Source",0,0,40,100},1}},0);
    rejects("MISSING_GRID",[&]{no_grid_session.apply({GridBoundsHeightCommand{LinkGridBoundsHeight{
        {"no-grid-height-target-grid","","grid.bounds.height"},{"no-grid-height-source","","artboard.height"},false}}},
        no_grid_session.revision());});
    auto smuggled=layout;smuggled.grid->bounds_height_driver=source;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-height-target",smuggled}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{
        Artboard injected{"grid-height-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected_layout;injected_layout.grid=Grid{"grid-height-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected_layout.grid->bounds_height_driver=source;injected.layout=injected_layout;
        apply({AddArtboard{composition,injected,3}});
    });

    const auto json_link=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_grid_bounds_height","target":{"object":"grid-height-target-grid","point":"","field":"grid.bounds.height"},
       "source":{"object":"grid-height-source","point":"","field":"artboard.height"},"replace_driver":false}
    ]})");
    check(json_link.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session links Grid height through its dedicated revisioned Artboard-size command");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==500&&typed.driver==source&&typed.source_kind=="link"&&
        !typed.expression&&std::get<double>(typed.evaluated)==500&&
        evaluate_artboard(session.document().compositions.front(),"grid-height-target").layout->grid->bounds.height==500,
        "Grid height evaluates its distinct same-Composition Artboard source while retaining literal and exact Ref");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-height-target-grid","point":"","field":"grid.bounds.height"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    const auto typed_read_details="Typed get and properties expose the height literal, exact link, evaluated du and expression capability (get="+
        typed_json+", properties="+properties_json+")";
    check(typed_json.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed_json.find("\"field\":\"artboard.height\"")!=std::string::npos&&
        typed_json.find("\"authored\":{\"literal\":")!=std::string::npos&&
        typed_json.find("\"evaluated\":5E2")!=std::string::npos&&typed_json.find("\"link\":true")!=std::string::npos&&
        typed_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"field\":\"grid.bounds.height\"")!=std::string::npos,
        typed_read_details.c_str());
    const auto linked_revision=session.revision();
    apply({GridBoundsHeightCommand{LinkGridBoundsHeight{target,source,false}}});
    check(session.revision()==linked_revision,"Repeating the exact Grid height source is idempotent");
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({GridBoundsHeightCommand{LinkGridBoundsHeight{target,alternate,false}}});});
    apply({GridBoundsHeightCommand{LinkGridBoundsHeight{target,alternate,true}}});
    check(artboard_layout_property(session.document(),target).driver==alternate&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==510,
        "Explicit replacement changes Grid height to a different Artboard source");
    apply({GridBoundsHeightCommand{LinkGridBoundsHeight{target,source,true}}});

    auto source_update=find_board("grid-height-source");source_update.height=520;
    apply({UpdateArtboard{composition,source_update}});
    typed=artboard_layout_property(session.document(),target);
    const auto evaluated_grid=evaluate_artboard(session.document().compositions.front(),"grid-height-target");
    const double cell_height=(evaluated_grid.layout->grid->bounds.height-20.0)/2.0;
    check(std::get<double>(typed.literal)==500&&typed.driver==source&&std::get<double>(typed.evaluated)==520&&
        evaluated_grid.layout->grid->bounds.height==520&&cell_height==250&&
        evaluated_grid.layout->grid->bounds.y+evaluated_grid.layout->grid->bounds.height==560&&
        find_board("grid-height-target").layout->grid->id==target.object,
        "Source resize changes evaluated height, row-cell height and bottom edge without moving or replacing the Grid");
    const auto values_before_align=evaluate(session.document());
    const auto untouched_bounds=object_bounds(session.document(),"grid-height-align-shape",values_before_align,
        evaluate_transforms(session.document(),values_before_align),true);
    check(untouched_bounds&&untouched_bounds->bottom==10,
        "Changing the Grid height source leaves existing Object positions unchanged");
    apply({AlignObjects{{"grid-height-align-shape"},"y","max",{},"grid:grid-height-target-grid"}});
    const auto aligned_values=evaluate(session.document());
    const auto aligned_bounds=object_bounds(session.document(),"grid-height-align-shape",aligned_values,
        evaluate_transforms(session.document(),aligned_values),true);
    check(aligned_bounds&&aligned_bounds->bottom==560,
        "Grid-reference vertical Align consumes evaluated height 520 rather than authored literal 500");
    auto edited=*find_board("grid-height-target").layout;edited.grid->bounds.x=60;edited.grid->bounds.width=840;
    apply({SetArtboardLayout{composition,"grid-height-target",edited}});
    auto target_update=find_board("grid-height-target");target_update.name="Moved target";target_update.x=120;
    source_update=find_board("grid-height-source");source_update.name="Renamed source";source_update.x=50;
    apply({UpdateArtboard{composition,target_update},UpdateArtboard{composition,source_update},
        ReorderArtboards{composition,{"grid-height-alternate","grid-height-source","grid-height-target"}}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==500&&typed.driver==source&&std::get<double>(typed.evaluated)==520&&
        evaluate_artboard(session.document().compositions.front(),"grid-height-target").layout->grid->bounds.height==520&&
        find_board("grid-height-target").layout->grid->id==target.object&&find_board("grid-height-target").layout->grid->bounds.x==60&&
        find_board("grid-height-target").x==120&&find_board("grid-height-source").name=="Renamed source"&&
        find_board("grid-height-source").x==50,
        "Sibling layout and Artboard edits preserve exact source, literal and stable Grid ID across source rename and reorder");

    const auto bytes_before=encode(session.document());const auto rev_before=session.revision();const auto history_before=session.history();
    auto direct=*find_board("grid-height-target").layout;direct.grid->bounds.height=501;
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({SetArtboardLayout{composition,"grid-height-target",direct}});});
    direct=*find_board("grid-height-target").layout;direct.grid->id="grid-height-replaced";
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({SetArtboardLayout{composition,"grid-height-target",direct}});});
    direct=*find_board("grid-height-target").layout;direct.grid.reset();
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({SetArtboardLayout{composition,"grid-height-target",direct}});});
    direct=*find_board("grid-height-target").layout;direct.grid->bounds_height_driver=alternate;
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-height-target",direct}});});
    target_update=find_board("grid-height-target");target_update.layout->grid->bounds.height=501;
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({UpdateArtboard{composition,target_update}});});
    for(const auto& bad:std::vector<std::pair<Ref,std::string>>{
            {{"grid-height-source","point","artboard.height"},"INVALID_ARTBOARD_REF"},
            {{"grid-height-source","","grid.bounds.height"},"INVALID_ARTBOARD_REF"},
            {{"grid-height-target","","artboard.height"},"GRID_SELF_LINK"},
            {{"grid-height-comp","","artboard.height"},"TYPE_MISMATCH"},
            {{"grid-height-other","","artboard.height"},"WRONG_COMPOSITION"},
            {{"grid-height-missing","","artboard.height"},"MISSING_ARTBOARD"}})
        rejects(bad.second.c_str(),[&]{apply({GridBoundsHeightCommand{LinkGridBoundsHeight{target,bad.first,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridBoundsHeightCommand{LinkGridBoundsHeight{{target.object,"point","grid.bounds.height"},source,true}}});});
    rejects("INVALID_LAYOUT_REF",[&]{apply({GridBoundsHeightCommand{LinkGridBoundsHeight{{target.object,"","grid.bounds.width"},source,true}}});});
    rejects("MISSING_GRID",[&]{apply({GridBoundsHeightCommand{UnlinkGridBoundsHeight{{"grid-height-missing-grid","","grid.bounds.height"}}}});});
    rejects("DUPLICATE_TARGET",[&]{apply({GridBoundsHeightCommand{UnlinkGridBoundsHeight{target}},
        GridBoundsHeightCommand{LinkGridBoundsHeight{target,source,true}}});});
    const auto stale=session.revision();
    rejects("REVISION_CONFLICT",[&]{session.apply({GridBoundsHeightCommand{UnlinkGridBoundsHeight{target}}},stale-1);});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-height-source"}});});
    rejects("INVALID_GUIDE",[&]{apply({GridBoundsHeightCommand{UnlinkGridBoundsHeight{target}},
        AddGuide{composition,{"grid-height-bad-guide","Bad axis","z",25}}});});
    auto source_invalid=find_board("grid-height-source");source_invalid.height=620;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,source_invalid}});});
    source_invalid=find_board("grid-height-source");source_invalid.height=20;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,source_invalid}});});
    auto invalid_gutter=*find_board("grid-height-target").layout;invalid_gutter.grid->row_gutter=600;
    rejects("INVALID_LAYOUT",[&]{apply({SetArtboardLayout{composition,"grid-height-target",invalid_gutter}});});
    check(session.revision()==rev_before&&session.history()==history_before&&encode(session.document())==bytes_before&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==520,
        "Source deletion, invalid containment or row geometry, stale revisions and failed batches leave bytes, revision and history unchanged");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bounds_height_driver\"")!=std::string::npos&&native.find("grid-height-source")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.60 preserves the exact Grid height link beside its authored literal");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.48\"")==1,
        "Grid height version-lie fixture changes only the writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"bounds_height_driver\":{",
        "\"bounds_height_driver\":{\"extra\":1,")==1,"Malformed Grid height driver fixture adds an unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    apply({GridBoundsHeightCommand{UnlinkGridBoundsHeight{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==520&&!typed.driver&&std::get<double>(typed.evaluated)==520,
        "Unlink freezes evaluated Grid height into its authored literal in one revision");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==500&&typed.driver==source&&std::get<double>(typed.evaluated)==520,
        "Undo restores the exact Grid height source and original literal");
    session.redo(session.revision());source_update=find_board("grid-height-source");source_update.height=600;
    apply({UpdateArtboard{composition,source_update}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==520&&!typed.driver&&std::get<double>(typed.evaluated)==520,
        "After unlink, later source edits leave the frozen evaluated height unchanged");
    auto native_048=test_support::without_empty_presets_for_legacy_fixture(encode(session.document()));
    check(replace_all(native_048,"\"version\":\"0.66\"","\"version\":\"0.48\"")==1&&
        decode(native_048)==session.document(),"Native 0.48 without the optional Grid height link remains readable");
}

void grid_bounds_height_expression() {
    auto document=empty_document("grid-height-expression-doc","grid-height-expression-comp","grid-height-expression-target");
    auto& target_board=document.compositions.front().artboards.front();target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-height-expression-other-comp","Other plane",{},
        {{"grid-height-expression-other","Other frame",0,0,400,300}}});
    const Id composition="grid-height-expression-comp";
    const Ref target{"grid-height-expression-target-grid","","grid.bounds.height"};
    const Ref source{"grid-height-expression-source","","artboard.height"};
    const Ref upstream{"grid-height-expression-upstream","","artboard.height"};
    const Expression expression{R"(ref("grid-height-expression-source","","artboard.height") + 10)",1};
    Artboard source_board{"grid-height-expression-source","Source frame",0,0,100,300};
    source_board.parent_size=ArtboardParent{"grid-height-expression-upstream",false,true};
    Artboard upstream_board{"grid-height-expression-upstream","Upstream frame",0,0,100,490};
    ArtboardLayout layout;layout.grid=Grid{"grid-height-expression-target-grid",{40,40,880,500},2,2,0,20};
    Contour align_outline;align_outline.id="grid-height-expression-align-contour";align_outline.closed=true;
    for(const auto& [x,y]:std::array<std::pair<double,double>,4>{{{0,0},{10,0},{10,10},{0,10}}}) {
        Point point;point.id="grid-height-expression-align-point-"+std::to_string(align_outline.points.size());
        point.x.literal=x;point.y.literal=y;align_outline.points.push_back(point);
    }
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto find_board=[&](const Id& id) -> Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source_board,1},AddArtboard{composition,upstream_board,2},
        SetArtboardLayout{composition,"grid-height-expression-target",layout},
        CreatePath{composition,"","grid-height-expression-align-shape","Align to Grid expression",{align_outline}}});
    const auto expression_command=[&](Expression value,bool replace=false) {
        return GridBoundsHeightCommand{SetGridBoundsHeightExpression{target,std::move(value),replace}};
    };
    const auto json_command=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_grid_bounds_height_expression","target":{"object":"grid-height-expression-target-grid","point":"","field":"grid.bounds.height"},
       "expression":{"source":"ref(\"grid-height-expression-source\",\"\",\"artboard.height\") + 10","version":1},"replace_driver":false}
    ]})");
    check(json_command.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session accepts the revisioned Grid height expression command");
    auto typed=artboard_layout_property(session.document(),target);
    auto resolved=evaluate_artboard(session.document().compositions.front(),"grid-height-expression-target");
    check(std::get<double>(typed.literal)==500&&!typed.driver&&typed.expression==expression&&
        typed.source_kind=="expression"&&std::get<double>(typed.evaluated)==500&&
        resolved.layout->grid->bounds.height==500&&(500.0-20.0)/2.0==240&&
        resolved.layout->grid->bounds.y+resolved.layout->grid->bounds.height==540,
        "Grid height expression retains authored literal and exact source while evaluating the positive row cell and bottom edge");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-height-expression-target-grid","point":"","field":"grid.bounds.height"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("grid-height-expression-source")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        properties_json.find("\"field\":\"grid.bounds.height\"")!=std::string::npos&&
        properties_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        "Typed Grid height get and properties expose exact expression, authored value and evaluated value");
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({expression_command(expression)});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the exact Grid height expression is idempotent");

    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({GridBoundsHeightCommand{LinkGridBoundsHeight{target,source,false}}});});
    apply({GridBoundsHeightCommand{LinkGridBoundsHeight{target,source,true}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source&&!typed.expression&&std::get<double>(typed.evaluated)==490,
        "Explicit replacement changes Grid height expression to the source Artboard height link");
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({expression_command(expression)});});
    apply({expression_command(expression,true)});
    auto upstream_update=find_board("grid-height-expression-upstream");upstream_update.height=510;
    apply({UpdateArtboard{composition,upstream_update}});
    typed=artboard_layout_property(session.document(),target);
    resolved=evaluate_artboard(session.document().compositions.front(),"grid-height-expression-target");
    const double cell_height=(resolved.layout->grid->bounds.height-20.0)/2.0;
    check(std::get<double>(typed.literal)==500&&typed.expression==expression&&std::get<double>(typed.evaluated)==520&&
        resolved.layout->grid->bounds.height==520&&cell_height==250&&
        resolved.layout->grid->bounds.y+resolved.layout->grid->bounds.height==560,
        "Parent-driven Artboard height changes reevaluate Grid height, row cell and bottom edge without changing authored height");
    auto sibling_layout=*find_board("grid-height-expression-target").layout;
    sibling_layout.grid->bounds.x=60;sibling_layout.grid->bounds.width=840;
    apply({SetArtboardLayout{composition,"grid-height-expression-target",sibling_layout}});
    auto renamed_source=find_board("grid-height-expression-source");renamed_source.name="Renamed expression source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed_source},ReorderArtboards{composition,
        {"grid-height-expression-upstream","grid-height-expression-target","grid-height-expression-source"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==500&&std::get<double>(typed.evaluated)==520&&
        find_board("grid-height-expression-target").layout->grid->id==target.object&&
        find_board("grid-height-expression-target").layout->grid->bounds.x==60&&
        find_board("grid-height-expression-source").name=="Renamed expression source",
        "Sibling layout edits and source rename/reorder preserve expression text and the stable Grid ID");
    const auto values_before_align=evaluate(session.document());
    const auto untouched_bounds=object_bounds(session.document(),"grid-height-expression-align-shape",values_before_align,
        evaluate_transforms(session.document(),values_before_align),true);
    check(untouched_bounds&&untouched_bounds->bottom==10,
        "Changing Grid height expression sources leaves existing Object positions unchanged");
    apply({AlignObjects{{"grid-height-expression-align-shape"},"y","max",{},"grid:grid-height-expression-target-grid"}});
    const auto aligned_values=evaluate(session.document());
    const auto aligned_bounds=object_bounds(session.document(),"grid-height-expression-align-shape",aligned_values,
        evaluate_transforms(session.document(),aligned_values),true);
    check(aligned_bounds&&aligned_bounds->bottom==560,
        "Grid-reference vertical Align consumes expression-evaluated height 520");

    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();
    const auto stable_history=session.history();
    auto invalid=[&](Expression bad,const char* message) {
        bool did_reject=false;
        try {apply({expression_command(std::move(bad),true)});} catch(const Error&) {did_reject=true;}
        check(did_reject,message);
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
            "Rejected Grid height expression preserves Document, native bytes, revision and history");
    };
    invalid({"ref(",1},"Malformed Grid height expression is rejected");
    invalid({expression.source,0},"Unsupported Grid height expression version is rejected");
    invalid({std::string(33,'(')+"1"+std::string(33,')'),1},"Grid height expression enforces the shared expression nesting limit");
    rejects("UNIT_MISMATCH",[&]{apply({expression_command({
        R"expr(ref("grid-height-expression-source","","artboard.height") + ref("object","","transform.rotation"))expr",1},true)});});
    rejects("GRID_BOUNDS_HEIGHT_EXPRESSION_TYPE",[&]{apply({expression_command({
        R"(ref("grid-height-expression-source","point","artboard.height"))",1},true)});});
    rejects("GRID_BOUNDS_HEIGHT_EXPRESSION_TYPE",[&]{apply({expression_command({
        R"(ref("grid-height-expression-source","","grid.bounds.height"))",1},true)});});
    rejects("GRID_SELF_LINK",[&]{apply({expression_command({R"(ref("grid-height-expression-target","","artboard.height"))",1},true)});});
    rejects("WRONG_COMPOSITION",[&]{apply({expression_command({R"(ref("grid-height-expression-other","","artboard.height"))",1},true)});});
    rejects("MISSING_ARTBOARD",[&]{apply({expression_command({R"(ref("grid-height-expression-missing","","artboard.height"))",1},true)});});
    rejects("EXPRESSION_DOMAIN",[&]{apply({expression_command({R"(1 / 0)",1},true)});});
    rejects("DUPLICATE_TARGET",[&]{apply({expression_command(expression,true),
        GridBoundsHeightCommand{UnlinkGridBoundsHeight{target}}});});
    auto payload=*find_board("grid-height-expression-target").layout;payload.grid->bounds.height=501;
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({SetArtboardLayout{composition,"grid-height-expression-target",payload}});});
    payload=*find_board("grid-height-expression-target").layout;payload.grid->id="grid-height-expression-replaced";
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({SetArtboardLayout{composition,"grid-height-expression-target",payload}});});
    payload=*find_board("grid-height-expression-target").layout;payload.grid.reset();
    rejects("DRIVEN_GRID_BOUNDS_HEIGHT",[&]{apply({SetArtboardLayout{composition,"grid-height-expression-target",payload}});});
    payload=*find_board("grid-height-expression-target").layout;payload.grid->bounds_height_expression=Expression{"500",1};
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-height-expression-target",payload}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{Artboard injected{"grid-height-expression-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected_layout;injected_layout.grid=Grid{"grid-height-expression-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected_layout.grid->bounds_height_expression=expression;injected.layout=injected_layout;
        apply({AddArtboard{composition,injected,session.document().compositions.front().artboards.size()}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-height-expression-source"}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({expression_command(expression,true)},stable_revision-1);});
    rejects("INVALID_GUIDE",[&]{apply({expression_command(expression,true),
        AddGuide{composition,{"grid-height-expression-bad-guide","Bad axis","z",25}}});});
    for(const double height:{610.0,10.0}) {
        auto invalid_upstream=find_board("grid-height-expression-upstream");invalid_upstream.height=height;
        rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,invalid_upstream}});});
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
            "Containment and positive-row-cell failures leave Grid height expression state unchanged");
    }
    auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bounds_height_expression\"")!=std::string::npos&&
        encode(decode(native))==native,
        "Native 0.60 preserves exact Grid height expression text and roundtrips bytes");
    auto old_version=test_support::without_empty_presets_for_legacy_fixture(native);
    check(replace_all(old_version,"\"version\":\"0.66\"","\"version\":\"0.49\"")==1,
        "Grid height expression version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(old_version);});
    auto conflict=native;
    check(replace_all(conflict,"\"bounds_height_expression\":{",
        "\"bounds_height_driver\":{\"link\":{\"object\":\"grid-height-expression-source\",\"point\":\"\",\"field\":\"artboard.height\"}},\"bounds_height_expression\":{")==1,
        "Grid height conflict fixture adds only the second mutually exclusive source");
    rejects("GRID_SOURCE_CONFLICT",[&]{(void)decode(conflict);});
    apply({GridBoundsHeightCommand{UnlinkGridBoundsHeight{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==520&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==520,"Unlink freezes expression-evaluated Grid height into the authored literal");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==500&&std::get<double>(typed.evaluated)==520,
        "Undo restores exact Grid height expression and its original literal");
    session.redo(session.revision());auto source_update=find_board("grid-height-expression-upstream");source_update.height=600;
    apply({UpdateArtboard{composition,source_update}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==520&&!typed.expression&&std::get<double>(typed.evaluated)==520,
        "Redo keeps the frozen Grid height independent from later Artboard-size changes");
    auto old_link_document=session.document();
    auto& old_link_grid=*std::find_if(old_link_document.compositions.front().artboards.begin(),
        old_link_document.compositions.front().artboards.end(),[](const Artboard& value){return value.id=="grid-height-expression-target";})->layout->grid;
    old_link_grid.bounds_height_driver=source;
    auto native_049=test_support::without_empty_presets_for_legacy_fixture(encode(old_link_document));
    check(replace_all(native_049,"\"version\":\"0.66\"","\"version\":\"0.49\"")==1&&
        decode(native_049)==old_link_document,"Native 0.49 remains readable with its Grid height link and no expression");
}

void grid_bounds_width_expression() {
    auto document=empty_document("grid-width-expression-doc","grid-width-expression-comp","grid-width-expression-target");
    auto& target_board=document.compositions.front().artboards.front();
    target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-width-expression-other-comp","Other plane",{},
        {{"grid-width-expression-other","Other frame",0,0,400,300}}});
    const Id composition="grid-width-expression-comp";
    const Ref target{"grid-width-expression-target-grid","","grid.bounds.width"};
    const Ref source_width{"grid-width-expression-source","","artboard.width"};
    const Ref source_height{"grid-width-expression-source","","artboard.height"};
    const Expression expression{R"(ref("grid-width-expression-source","","artboard.width") + 10)",1};
    Artboard source{"grid-width-expression-source","Source frame",0,0,690,300};
    source.parent_size=ArtboardParent{"grid-width-expression-upstream",true,false};
    Artboard upstream{"grid-width-expression-upstream","Upstream frame",0,0,690,100};
    ArtboardLayout layout;layout.grid=Grid{"grid-width-expression-target-grid",{40,20,700,500},2,1,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto target_artboard=[&]() -> const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),
            [&](const Artboard& value){return value.id=="grid-width-expression-target";});
    };
    auto find_board=[&](const Id& id) -> Artboard {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),
            [&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,source,1},AddArtboard{composition,upstream,2},
        SetArtboardLayout{composition,"grid-width-expression-target",layout}});
    const auto expression_command=[&](Expression value,bool replace=false) {
        return GridBoundsWidthCommand{SetGridBoundsWidthExpression{target,std::move(value),replace}};
    };
    const auto json_command=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"set_grid_bounds_width_expression","target":{"object":"grid-width-expression-target-grid","point":"","field":"grid.bounds.width"},
       "expression":{"source":"ref(\"grid-width-expression-source\",\"\",\"artboard.width\") + 10","version":1},"replace_driver":false}
    ]})");
    check(json_command.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session accepts the revisioned Grid width expression command");
    auto typed=artboard_layout_property(session.document(),target);
    const auto initial_grid=evaluate_artboard(session.document().compositions.front(),"grid-width-expression-target");
    check(std::get<double>(typed.literal)==700&&!typed.driver&&typed.expression==expression&&
        typed.source_kind=="expression"&&std::get<double>(typed.evaluated)==700&&
        initial_grid.layout->grid->bounds.width==700&&
        (initial_grid.layout->grid->bounds.width-20.0)/2.0==340,
        "Grid width expression keeps its authored literal and exact source while evaluating du and column cells");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-width-expression-target-grid","point":"","field":"grid.bounds.width"}})");
    const auto properties_json=request(session,R"({"op":"properties"})");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("grid-width-expression-source")!=std::string::npos&&
        typed_json.find("\"evaluated\":")!=std::string::npos&&
        properties_json.find("\"field\":\"grid.bounds.width\"")!=std::string::npos&&
        properties_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        "Typed Grid width get and properties expose authored, expression source and evaluated values");

    const auto before_expression_history=session.history().states.back().estimated_bytes;
    const auto before_expression_document=session.document();
    auto padded_expression=expression;padded_expression.source+="   ";
    apply({expression_command(padded_expression,true)});
    const auto after_expression_history=session.history().states.back().estimated_bytes;
    check(session.document()!=before_expression_document&&
        artboard_layout_property(session.document(),target).expression==padded_expression&&
        after_expression_history>=before_expression_history+3,
        "Grid width expression source participates in structural equality and retained history size");
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({expression_command(padded_expression)});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the exact Grid width expression is idempotent");

    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{apply({GridBoundsWidthCommand{LinkGridBoundsWidth{target,source_height,false}}});});
    apply({GridBoundsWidthCommand{LinkGridBoundsWidth{target,source_height,true}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.driver==source_height&&!typed.expression&&std::get<double>(typed.evaluated)==300,
        "Explicit replacement changes Grid width to the local Artboard height link");
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{apply({expression_command(expression)});});
    apply({expression_command(expression,true)});
    typed=artboard_layout_property(session.document(),target);
    check(!typed.driver&&typed.expression==expression&&std::get<double>(typed.literal)==700&&
        std::get<double>(typed.evaluated)==700,
        "Explicit link-to-expression replacement retains the authored fallback literal");

    auto upstream_update=find_board("grid-width-expression-upstream");upstream_update.width=710;
    apply({UpdateArtboard{composition,upstream_update}});
    typed=artboard_layout_property(session.document(),target);
    const auto evaluated_grid=evaluate_artboard(session.document().compositions.front(),"grid-width-expression-target");
    check(std::get<double>(typed.literal)==700&&typed.expression==expression&&
        std::get<double>(typed.evaluated)==720&&evaluated_grid.layout->grid->bounds.width==720&&
        (evaluated_grid.layout->grid->bounds.width-20.0)/2.0==350,
        "Parent-driven source edits reevaluate width and column cells without changing its literal or expression");

    auto sibling_layout=*target_artboard().layout;
    sibling_layout.grid->bounds.y=30;sibling_layout.grid->bounds.height=480;
    apply({SetArtboardLayout{composition,"grid-width-expression-target",sibling_layout}});
    auto renamed_source=find_board("grid-width-expression-source");renamed_source.name="Renamed source";renamed_source.x=50;
    apply({UpdateArtboard{composition,renamed_source},ReorderArtboards{composition,
        {"grid-width-expression-upstream","grid-width-expression-target","grid-width-expression-source"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==700&&std::get<double>(typed.evaluated)==720&&
        target_artboard().layout->grid->id==target.object&&target_artboard().layout->grid->bounds.y==30&&
        find_board("grid-width-expression-source").name=="Renamed source",
        "Full-layout siblings and source rename/reorder preserve the stable Grid ID and exact expression Ref");

    const auto stable_document=encode(session.document());
    const auto stable_revision=session.revision();const auto stable_history=session.history();
    auto invalid=[&](Expression bad,const char* message) {
        bool did_reject=false;
        try {apply({expression_command(std::move(bad),true)});} catch(const Error&) {did_reject=true;}
        check(did_reject,message);
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
            "Rejected Grid width expression preserves Document, native bytes, revision and history");
    };
    invalid({"ref(",1},"Malformed Grid width expression is rejected");
    invalid({expression.source,0},"Unsupported Grid width expression version is rejected");
    invalid({std::string(33,'(')+"1"+std::string(33,')'),1},
        "Grid width expression enforces the shared expression nesting limit");
    rejects("UNIT_MISMATCH",[&]{apply({expression_command({
        R"expr(ref("grid-width-expression-source","","artboard.width") + ref("object","","generator.rotation"))expr",1},true)});});
    rejects("GRID_BOUNDS_WIDTH_EXPRESSION_TYPE",[&]{apply({expression_command({
        R"(ref("grid-width-expression-source","point","artboard.width"))",1},true)});});
    rejects("GRID_BOUNDS_WIDTH_EXPRESSION_TYPE",[&]{apply({expression_command({
        R"(ref("grid-width-expression-source","","grid.bounds.width"))",1},true)});});
    rejects("GRID_SELF_LINK",[&]{apply({expression_command({
        R"(ref("grid-width-expression-target","","artboard.width"))",1},true)});});
    rejects("WRONG_COMPOSITION",[&]{apply({expression_command({
        R"(ref("grid-width-expression-other","","artboard.width"))",1},true)});});
    rejects("MISSING_ARTBOARD",[&]{apply({expression_command({
        R"(ref("grid-width-expression-missing","","artboard.width"))",1},true)});});
    rejects("DUPLICATE_TARGET",[&]{apply({expression_command(expression,true),
        GridBoundsWidthCommand{UnlinkGridBoundsWidth{target}}});});
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{auto payload=*target_artboard().layout;payload.grid->bounds.width=701;
        apply({SetArtboardLayout{composition,"grid-width-expression-target",payload}});});
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{auto payload=*target_artboard().layout;payload.grid->id="grid-width-expression-replaced";
        apply({SetArtboardLayout{composition,"grid-width-expression-target",payload}});});
    rejects("DRIVEN_GRID_BOUNDS_WIDTH",[&]{auto payload=*target_artboard().layout;payload.grid.reset();
        apply({SetArtboardLayout{composition,"grid-width-expression-target",payload}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{auto payload=*target_artboard().layout;
        payload.grid->bounds_width_expression=Expression{"500",1};
        apply({SetArtboardLayout{composition,"grid-width-expression-target",payload}});});
    rejects("GRID_DRIVER_SMUGGLING",[&]{auto board=Artboard{"grid-width-expression-smuggled","Smuggled",0,0,100,100};
        ArtboardLayout injected;injected.grid=Grid{"grid-width-expression-smuggled-grid",{0,0,100,100},1,1,0,0};
        injected.grid->bounds_width_expression=expression;board.layout=injected;
        apply({AddArtboard{composition,board,session.document().compositions.front().artboards.size()}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-width-expression-source"}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({expression_command(expression,true)},stable_revision-1);});
    rejects("INVALID_GUIDE",[&]{apply({expression_command(expression,true),
        AddGuide{composition,{"grid-width-expression-bad-guide","Bad axis","z",25}}});});
    for(const double width:{940.0,10.0}) {
        auto invalid_upstream=find_board("grid-width-expression-upstream");invalid_upstream.width=width;
        rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,invalid_upstream}});});
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
            "Containment and positive-cell failures leave Grid width expression state unchanged");
    }

    auto cyclic_document=session.document();
    auto cycle_a=Artboard{"grid-width-expression-cycle-a","Cycle A",0,0,100,100};
    auto cycle_b=Artboard{"grid-width-expression-cycle-b","Cycle B",0,0,100,100};
    Session cycle_session(cyclic_document);
    const auto cycle_revision=cycle_session.revision();
    bool cycle_rejected=false;
    try {cycle_session.apply({AddArtboard{composition,cycle_a,3},AddArtboard{composition,cycle_b,4},
        SetArtboardSizeExpression{{cycle_a.id,"","artboard.width"},
            {R"(ref("grid-width-expression-cycle-b","","artboard.width"))",1},false},
        SetArtboardSizeExpression{{cycle_b.id,"","artboard.width"},
            {R"(ref("grid-width-expression-cycle-a","","artboard.width"))",1},false},
        GridBoundsWidthCommand{SetGridBoundsWidthExpression{target,
            {R"(ref("grid-width-expression-cycle-a","","artboard.width"))",1},true}}},cycle_revision);}
    catch(const Error& error){cycle_rejected=error.code=="ARTBOARD_CYCLE";}
    check(cycle_rejected&&cycle_session.revision()==cycle_revision&&cycle_session.document()==cyclic_document,
        "Grid width expression rejects an upstream Artboard-size cycle atomically");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bounds_width_expression\"")!=std::string::npos&&
        native.find("grid-width-expression-source")!=std::string::npos&&encode(decode(native))==native,
        "Native 0.60 preserves exact Grid width expression text and roundtrips bytes");
    auto old_version=test_support::without_empty_presets_for_legacy_fixture(native);
    check(replace_all(old_version,"\"version\":\"0.66\"","\"version\":\"0.47\"")==1,
        "Grid width expression version-lie fixture changes only the native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(old_version);});
    auto conflict=native;
    check(replace_all(conflict,"\"bounds_width_expression\":{",
        "\"bounds_width_driver\":{\"link\":{\"object\":\"grid-width-expression-source\",\"point\":\"\",\"field\":\"artboard.width\"}},\"bounds_width_expression\":{")==1,
        "Grid width conflict fixture adds only the second mutually exclusive source");
    rejects("GRID_SOURCE_CONFLICT",[&]{(void)decode(conflict);});
    auto literal_document=session.document();
    auto& literal_grid=*std::find_if(literal_document.compositions.front().artboards.begin(),
        literal_document.compositions.front().artboards.end(),[](const Artboard& value){
            return value.id=="grid-width-expression-target";})->layout->grid;
    literal_grid.bounds_width_expression.reset();literal_grid.bounds_width_driver=source_height;
    auto native_047=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(native_047,"\"version\":\"0.66\"","\"version\":\"0.47\"")==1&&
        decode(native_047)==literal_document,"Native 0.47 remains readable with its Grid width link and no expression");

    apply({GridBoundsWidthCommand{UnlinkGridBoundsWidth{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==720&&!typed.driver&&!typed.expression&&
        typed.source_kind=="literal"&&std::get<double>(typed.evaluated)==720,
        "Unlink freezes expression-evaluated Grid width into the authored literal");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==700&&std::get<double>(typed.evaluated)==720,
        "Undo restores exact Grid width expression and its original literal");
    session.redo(session.revision());upstream_update=find_board("grid-width-expression-upstream");upstream_update.width=600;
    apply({UpdateArtboard{composition,upstream_update}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==720&&!typed.expression&&std::get<double>(typed.evaluated)==720,
        "Redo keeps frozen Grid width independent from later Artboard-size changes");
}

void grid_bounds_x_expression() {
    auto document=empty_document("grid-expression-doc","grid-expression-comp","grid-expression-target");
    auto& target_board=document.compositions.front().artboards.front();
    target_board.width=960;target_board.height=640;
    document.compositions.push_back({"grid-expression-other-comp","Other plane",{},{{"grid-expression-other","Other frame",0,0,400,300}}});
    const auto composition="grid-expression-comp";
    const Ref target{"grid-expression-target-grid","","grid.bounds.x"};
    const Ref source_a{"grid-expression-source-a","","artboard.width"};
    const Ref source_b{"grid-expression-source-b","","artboard.height"};
    const Expression expression{"ref(\"grid-expression-source-a\",\"\",\"artboard.width\") + ref(\"grid-expression-source-b\",\"\",\"artboard.height\")",1};
    ArtboardLayout layout;layout.grid=Grid{"grid-expression-target-grid",{40,20,880,600},2,1,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto target_artboard=[&]() -> const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),
            [&](const Artboard& value){return value.id=="grid-expression-target";});
    };
    apply({AddArtboard{composition,Artboard{"grid-expression-source-a","Source A",0,0,20,100},1},
        AddArtboard{composition,Artboard{"grid-expression-source-b","Source B",0,0,100,30},2},
        SetArtboardLayout{composition,"grid-expression-target",layout}});
    const auto expression_command=[&](Expression value,bool replace=false) {
        return GridBoundsXCommand{SetGridBoundsXExpression{target,std::move(value),replace}};
    };
    apply({expression_command(expression)});
    const auto expression_revision=session.revision();const auto expression_history=session.history();
    apply({expression_command(expression)});
    check(session.revision()==expression_revision&&session.history()==expression_history,
        "Reapplying the same Grid expression is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&!typed.driver&&typed.expression==expression&&
        typed.source_kind=="expression"&&std::get<double>(typed.evaluated)==50&&
        evaluate_artboard(session.document().compositions.front(),"grid-expression-target").layout->grid->bounds.x==50,
        "Grid x expression retains literal, exact source and du evaluation through the Artboard-size evaluator");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-expression-target-grid","point":"","field":"grid.bounds.x"}})");
    const auto evaluated_at=typed_json.find("\"evaluated\":");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("grid-expression-source-a")!=std::string::npos&&typed_json.find("\"unit\":\"du\"")!=std::string::npos&&
        evaluated_at!=std::string::npos&&std::strtod(typed_json.c_str()+evaluated_at+12,nullptr)==50,
        ("Typed JSON Grid x discloses its expression, stable ID, unit and evaluated value: "+typed_json).c_str());
    const auto props_json=request(session,R"({"op":"properties"})");
    check(props_json.find("grid-expression-target-grid")!=std::string::npos&&
        props_json.find("\"source_kind\":\"expression\"")!=std::string::npos,
        "Properties enumeration includes the same typed Grid x expression read");

    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({GridBoundsXCommand{LinkGridBoundsX{target,source_a,false}}});});
    apply({GridBoundsXCommand{LinkGridBoundsX{target,source_a,true}}});
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({expression_command(expression)});});
    apply({expression_command(expression,true)});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&!typed.driver&&std::get<double>(typed.evaluated)==50,
        "Link-to-expression replacement requires an explicit flag and preserves the authored x literal");

    auto source_a_changed=session.document().compositions.front().artboards[1];source_a_changed.width=30;
    apply({UpdateArtboard{composition,source_a_changed}});
    auto source_b_changed=session.document().compositions.front().artboards[2];source_b_changed.height=40;
    apply({UpdateArtboard{composition,source_b_changed}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.expression==expression&&std::get<double>(typed.evaluated)==70,
        "Independent upstream Artboard width and height edits reevaluate x without changing source text or literal");

    auto sibling_layout=*target_artboard().layout;sibling_layout.grid->bounds.y=30;
    sibling_layout.grid->bounds.width=870;sibling_layout.grid->bounds.height=590;
    apply({SetArtboardLayout{composition,"grid-expression-target",sibling_layout}});
    auto moved_target=target_artboard();moved_target.name="Renamed target";moved_target.x=120;
    apply({UpdateArtboard{composition,moved_target},ReorderArtboards{composition,
        {"grid-expression-source-b","grid-expression-target","grid-expression-source-a"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70&&
        target_artboard().layout->grid->id==target.object&&target_artboard().layout->grid->bounds.width==870&&target_artboard().x==120,
        "Full-layout sibling edits, Artboard updates and reorder preserve the stable Grid ID and exact expression source");

    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({GridBoundsXCommand{LinkGridBoundsX{target,source_a,false}}});});
    apply({GridBoundsXCommand{LinkGridBoundsX{target,source_a,true}}});
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{apply({expression_command(expression)});});
    apply({expression_command(expression,true)});
    const auto stable_document=encode(session.document());const auto stable_revision=session.revision();const auto stable_history=session.history();
    auto invalid=[&](Expression bad,const char* message) {
        bool did_reject=false;
        try {apply({expression_command(std::move(bad),true)});} catch(const Error&) {did_reject=true;}
        check(did_reject,message);
        check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document,
            "Rejected Grid expression leaves Document, native bytes, revision and history unchanged");
    };
    invalid({"ref(",1},"Malformed Grid expression is rejected");
    invalid({expression.source,0},"Unsupported Grid expression version is rejected");
    rejects("GRID_BOUNDS_X_EXPRESSION_TYPE",[&]{apply({expression_command({"ref(\"grid-expression-source-a\",\"point\",\"artboard.width\")",1},true)});});
    rejects("GRID_BOUNDS_X_EXPRESSION_TYPE",[&]{apply({expression_command({"ref(\"grid-expression-source-a\",\"\",\"grid.bounds.x\")",1},true)});});
    rejects("GRID_SELF_LINK",[&]{apply({expression_command({"ref(\"grid-expression-target\",\"\",\"artboard.width\")",1},true)});});
    rejects("WRONG_COMPOSITION",[&]{apply({expression_command({"ref(\"grid-expression-other\",\"\",\"artboard.width\")",1},true)});});
    rejects("MISSING_ARTBOARD",[&]{apply({expression_command({"ref(\"grid-expression-missing\",\"\",\"artboard.width\")",1},true)});});
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{auto payload=*target_artboard().layout;payload.grid->bounds.x=71;
        apply({SetArtboardLayout{composition,"grid-expression-target",payload}});});
    rejects("DRIVEN_GRID_BOUNDS_X",[&]{auto payload=target_artboard();payload.layout->grid->id="grid-expression-replaced";
        apply({UpdateArtboard{composition,payload}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-expression-source-a"}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({expression_command(expression,true)},stable_revision-1);});
    rejects("INVALID_GUIDE",[&]{apply({expression_command(expression,true),AddGuide{composition,{"grid-expression-bad-guide","Bad axis","z",0}}});});
    const auto source_a_position=std::find_if(session.document().compositions.front().artboards.begin(),
        session.document().compositions.front().artboards.end(),[](const Artboard& value){return value.id=="grid-expression-source-a";});
    source_a_changed=*source_a_position;source_a_changed.width=100;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,source_a_changed}});});
    check(session.revision()==stable_revision&&session.history()==stable_history&&encode(session.document())==stable_document&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==70,
        "Failing source edits and second batch commands preserve the prior expression evaluation atomically");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bounds_x_expression\"")!=std::string::npos&&native.find("grid-expression-source-a")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.43 preserves exact Grid expression source and roundtrips bytes");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.36\"")==1,
        "Grid expression version-lie fixture changes only native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    apply({GridBoundsXCommand{UnlinkGridBoundsX{target}}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==70,"Unlink freezes the expression's evaluated x and clears its source in one step");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==70,
        "Undo restores the exact Grid expression and retained literal");
    session.redo(session.revision());source_a_changed=*std::find_if(session.document().compositions.front().artboards.begin(),
        session.document().compositions.front().artboards.end(),[](const Artboard& value){return value.id=="grid-expression-source-a";});
    source_a_changed.width=35;apply({UpdateArtboard{composition,source_a_changed}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==70&&!typed.expression&&std::get<double>(typed.evaluated)==70,
        "Redo keeps frozen x independent from later Artboard source changes");
}

void grid_bounds_y_expression() {
    auto document=empty_document("grid-y-expression-doc","grid-y-expression-comp","grid-y-expression-target");
    auto& target_board=document.compositions.front().artboards.front();
    target_board.width=1000;target_board.height=1200;
    document.compositions.push_back({"grid-y-expression-other-comp","Other plane",{},
        {{"grid-y-expression-other","Other frame",0,0,400,300}}});
    const auto composition="grid-y-expression-comp";
    const Ref target{"grid-y-expression-grid","","grid.bounds.y"};
    const Ref source_a{"grid-y-expression-source-a","","artboard.width"};
    const Ref source_b{"grid-y-expression-source-b","","artboard.width"};
    const Expression expression{R"expr(ref("grid-y-expression-source-a","","artboard.width"))expr",1};
    ArtboardLayout layout;layout.grid=Grid{"grid-y-expression-grid",{40,40,900,1100},2,1,20,0};
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    auto board=[&](const Id& id) -> const Artboard& {
        const auto& comp=*std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
            [&](const Composition& value){return value.id==composition;});
        return *std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& value){return value.id==id;});
    };
    apply({AddArtboard{composition,Artboard{"grid-y-expression-source-a","Source A",0,0,50,100},1},
        AddArtboard{composition,Artboard{"grid-y-expression-source-b","Source B",0,0,70,100},2},
        SetArtboardLayout{composition,"grid-y-expression-target",layout}});
    apply({GridBoundsYCommand{LinkGridBoundsY{target,source_a,false}}});
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,expression,false}}});});
    const auto json_expression=request(session,R"json({"op":"apply","expected_revision":2,"commands":[
      {"type":"set_grid_bounds_y_expression","target":{"object":"grid-y-expression-grid","point":"","field":"grid.bounds.y"},
       "expression":{"source":"ref(\"grid-y-expression-source-a\",\"\",\"artboard.width\")","version":1},"replace_driver":true}
    ]})json");
    check(json_expression.find("\"ok\":true")!=std::string::npos&&session.revision()==3,
        "JSON-lines applies the dedicated Grid y expression command with explicit source replacement");
    const auto stable_revision=session.revision();const auto stable_history=session.history();
    apply({GridBoundsYCommand{SetGridBoundsYExpression{target,expression,false}}});
    check(session.revision()==stable_revision&&session.history()==stable_history,
        "Repeating the same Grid y expression is idempotent without a revision or Undo entry");
    auto typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&!typed.driver&&typed.expression==expression&&typed.source_kind=="expression"&&
        std::get<double>(typed.evaluated)==50&&
        evaluate_artboard(session.document().compositions.front(),"grid-y-expression-target").layout->grid->bounds.y==50,
        "Grid y expression retains the literal, exact source and Artboard-local du evaluation");
    const auto typed_json=request(session,R"({"op":"get","ref":{"object":"grid-y-expression-grid","point":"","field":"grid.bounds.y"}})");
    const auto evaluated_at=typed_json.find("\"evaluated\":");
    check(typed_json.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        typed_json.find("grid-y-expression-source-a")!=std::string::npos&&typed_json.find("\"unit\":\"du\"")!=std::string::npos&&
        typed_json.find("\"link\":true")!=std::string::npos&&typed_json.find("\"expression\":true")!=std::string::npos&&
        evaluated_at!=std::string::npos&&std::strtod(typed_json.c_str()+evaluated_at+12,nullptr)==50,
        ("Typed Grid y get discloses literal, expression source and evaluated value: "+typed_json).c_str());
    const auto properties_json=request(session,R"({"op":"properties"})");
    const auto grid_property=properties_json.find("\"field\":\"grid.bounds.y\"");
    check(grid_property!=std::string::npos&&properties_json.find("\"source_kind\":\"expression\"",grid_property)!=std::string::npos,
        "Properties enumeration includes the typed Grid y expression read");

    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({GridBoundsYCommand{LinkGridBoundsY{target,source_b,false}}});});
    apply({GridBoundsYCommand{LinkGridBoundsY{target,source_b,true}}});
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,expression,false}}});});
    apply({GridBoundsYCommand{SetGridBoundsYExpression{target,expression,true}}});
    auto upstream=board("grid-y-expression-source-a");upstream.width=60;apply({UpdateArtboard{composition,upstream}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==40&&typed.expression==expression&&std::get<double>(typed.evaluated)==60,
        "Source width 50 to 60 reevaluates y without changing the authored literal or source text");
    auto sibling_layout=*board("grid-y-expression-target").layout;sibling_layout.grid->bounds.x=50;
    sibling_layout.grid->bounds.width=890;apply({SetArtboardLayout{composition,"grid-y-expression-target",sibling_layout}});
    auto renamed=board("grid-y-expression-source-a");renamed.name="Renamed source A";
    auto moved_target=board("grid-y-expression-target");moved_target.name="Renamed target";moved_target.x=120;
    apply({UpdateArtboard{composition,renamed},UpdateArtboard{composition,moved_target},
        ReorderArtboards{composition,{"grid-y-expression-source-b","grid-y-expression-target",
            "grid-y-expression-source-a"}}});
    typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==60&&
        board("grid-y-expression-target").layout->grid->id==target.object&&board("grid-y-expression-target").x==120,
        "Full-layout sibling edits, source rename, target move and reorder preserve the stable Grid ID and exact source Ref");

    const auto stable_document=encode(session.document());const auto failed_revision=session.revision();const auto failed_history=session.history();
    auto invalid=[&](Expression bad,const char* message) {
        bool did_reject=false;
        try {apply({GridBoundsYCommand{SetGridBoundsYExpression{target,std::move(bad),true}}});} catch(const Error&) {did_reject=true;}
        check(did_reject,message);
        check(session.revision()==failed_revision&&session.history()==failed_history&&encode(session.document())==stable_document,
            "Rejected Grid y expression leaves Document bytes, revision and history unchanged");
    };
    invalid({"ref(",1},"Malformed Grid y expression is rejected");
    invalid({expression.source,0},"Unsupported Grid y expression version is rejected");
    rejects("UNIT_MISMATCH",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,
        {R"expr(ref("grid-y-expression-source-a","","artboard.width") + ref("object","","generator.rotation"))expr",1},true}}});});
    rejects("GRID_BOUNDS_Y_EXPRESSION_TYPE",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,
        {R"expr(ref("grid-y-expression-source-a","point","artboard.width"))expr",1},true}}});});
    rejects("GRID_BOUNDS_Y_EXPRESSION_TYPE",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,
        {R"expr(ref("grid-y-expression-source-a","","grid.bounds.y"))expr",1},true}}});});
    rejects("GRID_SELF_LINK",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,
        {R"expr(ref("grid-y-expression-target","","artboard.width"))expr",1},true}}});});
    rejects("WRONG_COMPOSITION",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,
        {R"expr(ref("grid-y-expression-other","","artboard.width"))expr",1},true}}});});
    rejects("MISSING_ARTBOARD",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,
        {R"expr(ref("grid-y-expression-missing","","artboard.width"))expr",1},true}}});});
    rejects("MISSING_ARTBOARD",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,
        {R"expr(ref("grid-y-expression-grid","","artboard.width"))expr",1},true}}});});
    rejects("INVALID_LAYOUT",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,{"-1",1},true}}});});
    rejects("INVALID_LAYOUT",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,{"1000000000",1},true}}});});
    rejects("EXPRESSION_DOMAIN",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,{"1 / 0",1},true}}});});
    rejects("ARTBOARD_CYCLE",[&]{apply({SetArtboardSizeExpression{source_a,
        {R"expr(ref("grid-y-expression-source-b","","artboard.width"))expr",1}},
        SetArtboardSizeExpression{source_b,{R"expr(ref("grid-y-expression-source-a","","artboard.width"))expr",1}}});});
    auto changed_literal=*board("grid-y-expression-target").layout;changed_literal.grid->bounds.y=41;
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({SetArtboardLayout{composition,"grid-y-expression-target",changed_literal}});});
    auto cleared=*board("grid-y-expression-target").layout;cleared.grid.reset();
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({SetArtboardLayout{composition,"grid-y-expression-target",cleared}});});
    auto changed_id=board("grid-y-expression-target");changed_id.layout->grid->id="grid-y-expression-replaced";
    rejects("DRIVEN_GRID_BOUNDS_Y",[&]{apply({UpdateArtboard{composition,changed_id}});});
    auto smuggled=*board("grid-y-expression-target").layout;smuggled.grid->bounds_y_expression=Expression{"61",1};
    rejects("GRID_DRIVER_SMUGGLING",[&]{apply({SetArtboardLayout{composition,"grid-y-expression-target",smuggled}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{composition,"grid-y-expression-source-a"}});});
    rejects("REVISION_CONFLICT",[&]{session.apply({GridBoundsYCommand{SetGridBoundsYExpression{target,expression,true}}},failed_revision-1);});
    rejects("INVALID_GUIDE",[&]{apply({GridBoundsYCommand{SetGridBoundsYExpression{target,expression,true}},
        AddGuide{composition,{"grid-y-expression-bad-guide","Bad axis","z",0}}});});
    upstream=board("grid-y-expression-source-a");upstream.width=101;
    rejects("INVALID_LAYOUT",[&]{apply({UpdateArtboard{composition,upstream}});});
    check(session.revision()==failed_revision&&session.history()==failed_history&&encode(session.document())==stable_document&&
        std::get<double>(artboard_layout_property(session.document(),target).evaluated)==60,
        "Cycle, bad source edits and failed later batch commands preserve prior y expression state atomically");

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"bounds_y_expression\"")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.43 preserves the exact Grid y expression and stable source text");
    auto lied=test_support::without_empty_presets_for_legacy_fixture(native);check(replace_all(lied,"\"version\":\"0.66\"","\"version\":\"0.39\"")==1,
        "Grid y expression version-lie fixture changes only native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    apply({GridBoundsYCommand{UnlinkGridBoundsY{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==60&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==60,"Unlink freezes evaluated Grid y and clears its expression in one step");
    const auto literal_document=session.document();auto legacy_039=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(legacy_039,"\"version\":\"0.66\"","\"version\":\"0.39\"")==1&&decode(legacy_039)==literal_document,
        "Native 0.39 literal-only Grid y remains readable after the format bump");
    session.undo(session.revision());typed=artboard_layout_property(session.document(),target);
    check(typed.expression==expression&&std::get<double>(typed.literal)==40&&std::get<double>(typed.evaluated)==60,
        "Undo restores the exact Grid y expression and retained literal");
    session.redo(session.revision());upstream=board("grid-y-expression-source-a");upstream.width=80;apply({UpdateArtboard{composition,upstream}});
    typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==60&&!typed.expression&&std::get<double>(typed.evaluated)==60,
        "Redo keeps the frozen Grid y independent from later Artboard-size changes");
}

void guide_position_links() {
    auto document=empty_document("guide-link-doc","guide-link-comp","guide-link-art");
    document.compositions.push_back({"guide-link-other","Other plane",{},{{"guide-link-other-art","Other frame",0,0,400,300}}});
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    const Ref source{"guide-source","","guide.position"};
    const Ref target{"guide-target","","guide.position"};
    const Ref alternate{"guide-alternate","","guide.position"};
    const Ref horizontal{"guide-horizontal","","guide.position"};
    const Ref other_plane{"guide-other-plane","","guide.position"};
    apply({AddGuide{"guide-link-comp",{"guide-source","Source","x",100}},
        AddGuide{"guide-link-comp",{"guide-target","Target","x",240}},
        AddGuide{"guide-link-comp",{"guide-alternate","Alternate","x",175}},
        AddGuide{"guide-link-comp",{"guide-horizontal","Horizontal","y",60}},
        AddGuide{"guide-link-other",{"guide-other-plane","Other plane guide","x",300}}});

    const auto linked=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"link_guide_position","target":{"object":"guide-target","point":"","field":"guide.position"},
       "source":{"object":"guide-source","point":"","field":"guide.position"},"replace_driver":false}
    ]})");
    check(linked.find("\"ok\":true")!=std::string::npos&&linked.find("guide-target")!=std::string::npos,
        "Semantic JSON links a Guide position through the shared Session command and returns its changed Guide ID");
    const auto position=guide_position_property(session.document(),target);
    check(position.literal==240&&position.driver==source&&!position.expression&&position.source_kind=="link"&&position.evaluated==100,
        "Guide position link preserves its authored literal and evaluates from the same-axis source");
    const auto discovered=properties(session.document());
    check(resolve_name(session.document(),"Target","","guide.position")==target&&
        std::find(discovered.begin(),discovered.end(),target)!=discovered.end(),
        "Guide positions resolve uniquely by name and appear as typed property Refs");
    const auto get=request(session,R"({"op":"get","ref":{"object":"guide-target","point":"","field":"guide.position"}})");
    auto number_after=[&](std::string_view key) {
        const auto at=get.find(key);return at==std::string::npos?std::numeric_limits<double>::quiet_NaN():
            std::strtod(get.c_str()+at+key.size(),nullptr);
    };
    check(get.find("\"type\":\"number\"")!=std::string::npos&&get.find("\"unit\":\"du\"")!=std::string::npos&&
        get.find("\"space\":\"composition\"")!=std::string::npos&&get.find("\"origin\":\"authored\"")!=std::string::npos&&
        std::abs(number_after("\"literal\":" )-240)<1e-12&&std::abs(number_after("\"evaluated\":" )-100)<1e-12&&
        get.find("\"source_kind\":\"link\"")!=std::string::npos&&
        get.find("\"link\":true")!=std::string::npos&&get.find("\"expression\":true")!=std::string::npos,
        "Guide get returns the typed authored and evaluated position view");
    const auto property_list=request(session,R"({"op":"properties"})");
    check(property_list.find("\"object\":\"guide-target\"")!=std::string::npos&&
        property_list.find("\"field\":\"guide.position\"")!=std::string::npos,
        "Properties lists Guide positions through their typed view");

    session.undo(session.revision());
    check(!guide_position_property(session.document(),target).driver&&!guide_position_property(session.document(),target).expression&&
        guide_position_property(session.document(),target).evaluated==240,
        "Undo removes only the Guide position link and restores its literal value");
    session.redo(session.revision());
    check(guide_position_property(session.document(),target).driver==source&&
        guide_position_property(session.document(),target).evaluated==100,
        "Redo restores the stable Guide position Ref and evaluation");
    {
        Session idempotent(document);
        idempotent.apply({AddGuide{"guide-link-comp",{"guide-source","Source","x",100}},
            AddGuide{"guide-link-comp",{"guide-target","Target","x",240}}},0);
        idempotent.apply({LinkGuidePosition{target,source,false}},1);
        idempotent.apply({LinkGuidePosition{target,source,false}},idempotent.revision());
        check(guide_position_property(idempotent.document(),target).driver==source&&
            guide_position_property(idempotent.document(),target).evaluated==100,
            "Repeating the same Guide link is idempotent without a replacement flag");
    }

    Contour outline;outline.id="guide-align-outline";outline.closed=true;
    for(const auto& [x,y]:std::array<std::pair<double,double>,4>{{{0,0},{10,0},{10,10},{0,10}}}) {
        Point point;point.id="guide-align-point-"+std::to_string(static_cast<int>(outline.points.size()));
        point.x.literal=x;point.y.literal=y;outline.points.push_back(point);
    }
    apply({CreatePath{"guide-link-comp","","guide-align-shape","Align to Guide",{outline}}});
    AlignObjects align_to_guide{{"guide-align-shape"},"x","min",{},"guide:guide-target"};
    apply({align_to_guide});
    const auto aligned_values=evaluate(session.document());
    const auto aligned_bounds=object_bounds(session.document(),"guide-align-shape",aligned_values,
        evaluate_transforms(session.document(),aligned_values),true);
    check(aligned_bounds&&aligned_bounds->left==100,
        "Align-to-Guide uses the evaluated linked position");

    apply({UpdateGuide{"guide-link-comp",{"guide-source","Renamed source","x",130}},
        UpdateGuide{"guide-link-comp",{"guide-target","Renamed target","x",240}}});
    check(guide_position_property(session.document(),target).literal==240&&
        guide_position_property(session.document(),target).driver==source&&
        guide_position_property(session.document(),target).evaluated==130&&
        resolve_name(session.document(),"Renamed target","","guide.position")==target,
        "Guide rename and source update preserve stable linkage and the target literal");
    session.begin_gesture(session.revision());
    session.update_gesture({UpdateGuide{"guide-link-comp",{"guide-source","Renamed source","x",155}}});
    check(evaluate_guide_position(session.preview_document(),"guide-link-comp","guide-target")==155&&
        evaluate_guide_position(session.document(),"guide-link-comp","guide-target")==130,
        "Guide evaluation reads the preview document without changing committed Session state");
    session.cancel_gesture();

    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.66\"")!=std::string::npos&&
        native.find("\"position_driver\":{\"link\":{\"object\":\"guide-source\",\"point\":\"\",\"field\":\"guide.position\"}}")!=std::string::npos&&
        encode(decode(native))==native,
        "Native 0.43 preserves the existing optional Guide position link and authored literal");
    auto legacy_with_driver=test_support::without_empty_presets_for_legacy_fixture(native);
    check(replace_all(legacy_with_driver,"\"version\":\"0.66\"","\"version\":\"0.22\"")==1,
        "Legacy linked fixture downgrades only its version tag");
    rejects("INVALID_GUIDE",[&]{(void)decode(legacy_with_driver);});
    auto native_033_link=test_support::without_empty_presets_for_legacy_fixture(encode(session.document()));
    check(replace_all(native_033_link,"\"version\":\"0.66\"","\"version\":\"0.33\"")==1&&
        decode(native_033_link)==session.document(),
        "Native 0.33 still decodes the existing Guide link representation unchanged");
    auto literal_document=session.document();
    for(auto& composition:literal_document.compositions)for(auto& guide:composition.guides)guide.position_driver.reset();
    auto legacy_literal=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(legacy_literal,"\"version\":\"0.66\"","\"version\":\"0.22\"")==1&&
        decode(legacy_literal)==literal_document,
        "Native 0.22 continues to decode literal-only Guide positions unchanged");

    const Expression source_plus_twenty{R"(ref("guide-source","","guide.position") + 20)",1};
    apply({UpdateGuide{"guide-link-comp",{"guide-source","Renamed source","x",100}}});
    rejects("DRIVEN_GUIDE_POSITION",[&]{apply({SetGuidePositionExpression{target,source_plus_twenty,false}});});
    apply({SetGuidePositionExpression{target,source_plus_twenty,true}});
    auto expressed=guide_position_property(session.document(),target);
    check(expressed.literal==240&&!expressed.driver&&expressed.expression==source_plus_twenty&&
        expressed.source_kind=="expression"&&expressed.evaluated==120,
        "Replacing a Guide link with a du expression retains the literal and evaluates from its stable Ref");
    const auto expression_get=request(session,R"({"op":"get","ref":{"object":"guide-target","point":"","field":"guide.position"}})");
    check(expression_get.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        expression_get.find("\"expression\":{\"source\":\"ref(\\\"guide-source\\\",\\\"\\\",\\\"guide.position\\\") + 20\",\"version\":1}")!=std::string::npos&&
        expression_get.find("\"driver\":null")!=std::string::npos,
        "Guide get exposes the exact optional expression and source kind");
    const auto expression_properties=request(session,R"({"op":"properties"})");
    check(expression_properties.find("\"object\":\"guide-target\"")!=std::string::npos&&
        expression_properties.find("\"expression\":true")!=std::string::npos,
        "Guide properties expose expression capability and the evaluated expression source");
    session.undo(session.revision());
    check(guide_position_property(session.document(),target).driver==source&&
        !guide_position_property(session.document(),target).expression&&
        guide_position_property(session.document(),target).evaluated==100,
        "Undo restores the linked source after a Guide link-to-expression replacement");
    session.redo(session.revision());
    check(guide_position_property(session.document(),target).expression==source_plus_twenty&&
        guide_position_property(session.document(),target).evaluated==120,
        "Redo restores the exact Guide position expression");
    apply({UpdateGuide{"guide-link-comp",{"guide-source","Renamed source","x",120}}});
    expressed=guide_position_property(session.document(),target);
    auto reordered=session.document();
    std::reverse(reordered.compositions.front().guides.begin(),reordered.compositions.front().guides.end());
    check(expressed.literal==240&&expressed.evaluated==140&&
        evaluate_guide_position(reordered,"guide-link-comp","guide-target")==140,
        "Guide expression follows source movement and stable identity across Guide array reordering");
    apply({AddGuide{"guide-link-comp",{"guide-multi","Multi-source","x",15}},
        SetGuidePositionExpression{{"guide-multi","","guide.position"},
            {R"(ref("guide-source","","guide.position") + ref("guide-alternate","","guide.position") - 175)",1},false}});
    check(guide_position_property(session.document(),{"guide-multi","","guide.position"}).evaluated==120,
        "Guide expressions support multiple same-axis sources in one Composition");
    apply({AddGuide{"guide-link-comp",{"guide-constant","Constant expression","x",15}},
        SetGuidePositionExpression{{"guide-constant","","guide.position"},{"42",1},false}});
    const auto constant_position=guide_position_property(session.document(),{"guide-constant","","guide.position"});
    check(constant_position.source_kind=="expression"&&constant_position.expression==Expression{"42",1}&&
        constant_position.literal==15&&constant_position.evaluated==42,
        "Guide position expressions accept constants while retaining their authored literal");
    apply({LinkGuidePosition{target,alternate,true}});
    check(guide_position_property(session.document(),target).source_kind=="link"&&
        guide_position_property(session.document(),target).evaluated==175,
        "A Guide link explicitly replaces an authored expression");
    session.undo(session.revision());
    check(guide_position_property(session.document(),target).expression==source_plus_twenty&&
        guide_position_property(session.document(),target).evaluated==140,
        "Undo restores the exact Guide expression and its evaluated value after link replacement");
    session.redo(session.revision());session.undo(session.revision());
    const auto expression_native=encode(session.document());
    check(expression_native.find("\"version\":\"0.66\"")!=std::string::npos&&
        expression_native.find("\"position_expression\":{\"source\":\"ref(\\\"guide-source\\\",\\\"\\\",\\\"guide.position\\\") + 20\",\"version\":1}")!=std::string::npos&&
        encode(decode(expression_native))==expression_native,
        "Native 0.43 stores and byte-roundtrips the exact Guide expression while omitting an absent link");
    auto expression_lied_version=test_support::without_empty_presets_for_legacy_fixture(expression_native);
    check(replace_all(expression_lied_version,"\"version\":\"0.66\"","\"version\":\"0.33\"")==1,
        "Native Guide expression version-lie fixture changes only its version tag");
    rejects("INVALID_GUIDE",[&]{(void)decode(expression_lied_version);});
    auto both_sources=session.document();
    auto both_target=std::find_if(both_sources.compositions.front().guides.begin(),both_sources.compositions.front().guides.end(),
        [](const auto& guide){return guide.id=="guide-target";});
    both_target->position_driver=source;
    rejects("GUIDE_SOURCE_CONFLICT",[&]{Session invalid(std::move(both_sources));});

    const auto before_failures=encode(session.document());const auto revision_before_failures=session.revision();
    const auto history_before_failures=session.history();
    rejects("DRIVEN_GUIDE_POSITION",[&]{apply({UpdateGuide{"guide-link-comp",{"guide-target","Changed value","x",250}}});});
    rejects("DRIVEN_GUIDE_POSITION",[&]{apply({LinkGuidePosition{target,alternate,false}});});
    rejects("DRIVEN_GUIDE_POSITION",[&]{apply({SetGuidePositionExpression{target,{"25",1},false}});});
    rejects("GUIDE_AXIS_MISMATCH",[&]{apply({LinkGuidePosition{target,horizontal,true}});});
    rejects("GUIDE_AXIS_MISMATCH",[&]{apply({SetGuidePositionExpression{target,
        {R"(ref("guide-horizontal","","guide.position"))",1},true}});});
    rejects("WRONG_COMPOSITION",[&]{apply({LinkGuidePosition{target,other_plane,true}});});
    rejects("WRONG_COMPOSITION",[&]{apply({SetGuidePositionExpression{target,
        {R"(ref("guide-other-plane","","guide.position"))",1},true}});});
    rejects("INVALID_GUIDE_REF",[&]{apply({LinkGuidePosition{target,{"guide-source","point","guide.position"},true}});});
    rejects("INVALID_GUIDE_REF",[&]{apply({LinkGuidePosition{target,{"guide-source","","artboard.width"},true}});});
    rejects("GUIDE_SELF_LINK",[&]{apply({LinkGuidePosition{target,target,true}});});
    rejects("MISSING_GUIDE",[&]{apply({LinkGuidePosition{target,{"missing-guide","","guide.position"},true}});});
    rejects("TYPE_MISMATCH",[&]{apply({LinkGuidePosition{target,{"guide-link-art","","guide.position"},true}});});
    rejects("GUIDE_EXPRESSION_TYPE",[&]{apply({SetGuidePositionExpression{target,
        {R"(ref("guide-link-art","","artboard.width"))",1},true}});});
    rejects("GUIDE_EXPRESSION_TYPE",[&]{apply({SetGuidePositionExpression{target,
        {R"(ref("guide-source","point","guide.position"))",1},true}});});
    rejects("MISSING_GUIDE",[&]{apply({SetGuidePositionExpression{target,
        {R"(ref("missing-guide","","guide.position"))",1},true}});});
    rejects("GUIDE_SELF_LINK",[&]{apply({SetGuidePositionExpression{target,
        {R"(ref("guide-target","","guide.position"))",1},true}});});
    rejects("GUIDE_CYCLE",[&]{apply({SetGuidePositionExpression{source,
        {R"(ref("guide-target","","guide.position") + 1)",1},false}});});
    rejects("GUIDE_POSITION_RANGE",[&]{apply({SetGuidePositionExpression{target,{"1e10",1},true}});});
    rejects("UNSUPPORTED_EXPRESSION_VERSION",[&]{apply({SetGuidePositionExpression{target,{"1",2},true}});});
    rejects("UNIT_MISMATCH",[&]{apply({SetGuidePositionExpression{target,
        {R"(ref("guide-source","","guide.position") + ref("guide-align-shape","","generator.rotation"))",1},true}});});
    rejects("EXPRESSION_SYNTAX",[&]{apply({SetGuidePositionExpression{target,{"ref(",1},true}});});
    rejects("TYPE_MISMATCH",[&]{apply({Set{target,280}});});
    rejects("TYPE_MISMATCH",[&]{apply({Link{target,{source,1,0,"copy_local_value"}}});});
    rejects("TYPE_MISMATCH",[&]{apply({Unlink{target}});});
    rejects("TYPE_MISMATCH",[&]{apply({SetExpression{{target},{"1",1},false}});});
    auto smuggled=Guide{"guide-smuggled","Smuggled","x",15};smuggled.position_driver=source;
    rejects("GUIDE_DRIVER_SMUGGLING",[&]{apply({AddGuide{"guide-link-comp",smuggled}});});
    auto smuggled_expression=Guide{"guide-smuggled-expression","Smuggled expression","x",15};
    smuggled_expression.position_expression=Expression{"10",1};
    rejects("GUIDE_DRIVER_SMUGGLING",[&]{apply({AddGuide{"guide-link-comp",smuggled_expression}});});
    rejects("GUIDE_CYCLE",[&]{apply({LinkGuidePosition{source,target,false}});});
    rejects("GUIDE_AXIS_MISMATCH",[&]{apply({UpdateGuide{"guide-link-comp",{"guide-source","Renamed source","y",120}}});});
    rejects("MISSING_GUIDE",[&]{apply({DeleteGuide{"guide-link-comp","guide-source"}});});
    rejects("TYPE_MISMATCH",[&]{apply({SetGuidePositionExpression{target,{"180",1},true},Set{target,1}});});
    check(session.revision()==revision_before_failures&&session.history()==history_before_failures&&
        encode(session.document())==before_failures,
        "Invalid Guide expressions, generic Scalar commands, bad batches and dependent deletion preserve bytes, revision and Undo history");

    const auto freeze_value=guide_position_property(session.document(),target).evaluated;
    apply({UnlinkGuidePosition{target}});
    apply({UpdateGuide{"guide-link-comp",{"guide-source","Renamed source","x",500}}});
    check(!guide_position_property(session.document(),target).driver&&
        !guide_position_property(session.document(),target).expression&&
        guide_position_property(session.document(),target).source_kind=="literal"&&
        guide_position_property(session.document(),target).literal==freeze_value&&
        guide_position_property(session.document(),target).evaluated==freeze_value,
        "Explicit Guide unlink freezes the evaluated position before later source edits");

    Session batch(document);batch.apply({AddGuide{"guide-link-comp",{"guide-source","Source","x",100}},
        AddGuide{"guide-link-comp",{"guide-target","Target","x",240}}},0);
    batch.apply({LinkGuidePosition{target,source,false}},1);
    batch.apply({UnlinkGuidePosition{target},DeleteGuide{"guide-link-comp","guide-source"}},2);
    check(batch.document().compositions.front().guides.size()==1&&
        guide_position_property(batch.document(),target).literal==100&&
        !guide_position_property(batch.document(),target).driver,
        "A single ordered Session batch can freeze a Guide and then delete its former source");

    auto deep=empty_document("guide-depth-doc","guide-depth-comp","guide-depth-art");
    auto& chain=deep.compositions.front().guides;chain.reserve(257);
    for(std::size_t i=0;i<257;++i) {
        Guide guide{"depth-"+std::to_string(i),"Depth","x",static_cast<double>(i)};
        if(i)guide.position_driver=Ref{"depth-"+std::to_string(i-1),"","guide.position"};
        chain.push_back(std::move(guide));
    }
    rejects("GUIDE_DEPTH",[&]{Session rejected(std::move(deep));});
    auto expression_deep=empty_document("guide-expression-depth-doc","guide-expression-depth-comp","guide-expression-depth-art");
    auto& expression_chain=expression_deep.compositions.front().guides;expression_chain.reserve(257);
    for(std::size_t i=0;i<257;++i) {
        Guide guide{"expression-depth-"+std::to_string(i),"Expression depth","x",static_cast<double>(i)};
        if(i)guide.position_expression=Expression{"ref(\"expression-depth-"+std::to_string(i-1)+"\",\"\",\"guide.position\") + 1",1};
        expression_chain.push_back(std::move(guide));
    }
    rejects("GUIDE_DEPTH",[&]{Session rejected(std::move(expression_deep));});
}
void artboard_size_drivers() {
    auto document=empty_document("size-driver-doc","size-driver-comp","size-parent");
    document.compositions.front().artboards.front().width=800;
    document.compositions.front().artboards.front().height=600;
    document.compositions.push_back({"size-other-comp","Other plane",{},{{"size-other-art","Other frame",0,0,400,300}}});
    Session session(document);auto apply=[&](std::vector<Command> commands){session.apply(commands,session.revision());};
    const Ref parent_width{"size-parent","","artboard.width"};
    const Ref parent_height{"size-parent","","artboard.height"};
    const Ref child_width{"size-child","","artboard.width"};
    const Ref child_height{"size-child","","artboard.height"};
    const Ref peer_width{"size-peer","","artboard.width"};
    const Ref other_width{"size-other-art","","artboard.width"};
    const Ref scalar_target{"size-circle","","transform.tx"};
    auto malformed_parent_native=test_support::without_empty_presets_for_legacy_fixture(encode(empty_document("parent-chain-doc","parent-chain-comp","parent-chain-art")));
    const auto parent_height_marker=malformed_parent_native.find("\"height\":");
    check(parent_height_marker!=std::string::npos,"Native fixture contains its default Artboard height");
    const auto parent_height_end=malformed_parent_native.find_first_of(",}",parent_height_marker);
    malformed_parent_native.insert(parent_height_end,
        ",\"parent_size\":{\"artboard\":\"missing-parent\",\"width\":false,\"height\":false}");
    check(replace_all(malformed_parent_native,"\"version\":\"0.66\"","\"version\":\"0.32\"")==1,
        "Malformed false-false legacy parent fixture uses the preserved native 0.32 gate");
    rejects("MISSING_ARTBOARD",[&]{(void)decode(malformed_parent_native);});
    Artboard peer{"size-peer","Peer",50,60,200,300};
    auto circle=default_primitive("size-circle-source","nect.shape.circle");
    apply({AddArtboard{"size-driver-comp",peer,1},
        CreatePrimitive{"size-driver-comp","","size-circle","Circle",circle}});
    Artboard child{"size-child","Child",100,120,320,240};
    child.parent_size=ArtboardParent{"size-parent",true,true};
    const auto link_json=request(session,R"({"op":"apply","expected_revision":1,"commands":[
      {"type":"add_artboard","composition":"size-driver-comp","index":2,"artboard":{"id":"size-child","name":"Child","x":100,"y":120,"width":320,"height":240,"parent_size":{"artboard":"size-parent","width":true,"height":true}}},
      {"type":"link_artboard_size","target":{"object":"size-child","point":"","field":"artboard.width"},"source":{"object":"size-parent","point":"","field":"artboard.height"},"replace_driver":true}
    ]})");
    check(link_json.find("\"ok\":true")!=std::string::npos&&session.revision()==2,
        "JSON Session can add an Artboard and attach its typed size source in one atomic batch");
    auto resolved=[&](const Id& id){return evaluate_artboard(session.document().compositions.front(),id);};
    auto child_authored=[&]() -> const Artboard& {
        const auto& boards=session.document().compositions.front().artboards;
        return *std::find_if(boards.begin(),boards.end(),[](const auto& board){return board.id=="size-child";});
    };
    const auto linked=artboard_size_property(session.document(),child_width);
    check(linked.literal==320&&linked.driver==parent_height&&linked.source_kind=="link"&&
        linked.evaluated==600&&child_authored().parent_size&&
        !child_authored().parent_size->width&&child_authored().parent_size->height,
        "Cross-field link replaces only parent-driven width and retains the literal and other parent dimension");
    const auto typed=request(session,R"({"op":"get","ref":{"object":"size-child","point":"","field":"artboard.width"}})");
    check(typed.find("\"source_kind\":\"link\"")!=std::string::npos&&
        typed.find("\"driver\":{\"object\":\"size-parent\",\"point\":\"\",\"field\":\"artboard.height\"}")!=std::string::npos&&
        typed.find("\"link\":true")!=std::string::npos&&typed.find("\"expression\":true")!=std::string::npos,
        "Typed get reports Artboard source kind, exact stable Ref and supported link/expression capabilities");
    const auto revision_before_replacement=session.revision();const auto bytes_before_replacement=encode(session.document());
    rejects("DRIVEN_ARTBOARD_SIZE",[&]{apply({LinkArtboardSize{child_width,parent_width,false}});});
    check(session.revision()==revision_before_replacement&&encode(session.document())==bytes_before_replacement,
        "A typed Artboard driver cannot be silently replaced");

    auto parent_board=session.document().compositions.front().artboards.front();
    parent_board.name="Renamed parent";parent_board.height=660;
    apply({UpdateArtboard{"size-driver-comp",parent_board},ReorderArtboards{"size-driver-comp",{"size-peer","size-child","size-parent"}}});
    check(artboard_size_property(session.document(),child_width).driver==parent_height&&
        artboard_size_property(session.document(),child_width).evaluated==660&&
        resolve_name(session.document(),"Renamed parent","","artboard.height")==parent_height,
        "Stable Artboard links survive source rename, reorder and source edits");
    const auto linked_delete_state=encode(session.document());const auto linked_delete_revision=session.revision();
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{"size-driver-comp","size-parent"}});});
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{"size-driver-comp","size-parent"},
        AddArtboard{"size-driver-comp",{"size-parent","Replacement",0,0,200,200},2}});});
    check(session.revision()==linked_delete_revision&&encode(session.document())==linked_delete_state,
        "A typed link and parent_size dependent cannot be retargeted by deleting and readding the source ID");

    const Expression combined{"ref(\"size-parent\",\"\",\"artboard.height\") + ref(\"size-peer\",\"\",\"artboard.width\")",1};
    const auto expression_json=request(session,R"json({"op":"apply","expected_revision":3,"commands":[
      {"type":"set_artboard_size_expression","target":{"object":"size-child","point":"","field":"artboard.width"},"expression":{"source":"ref(\"size-parent\",\"\",\"artboard.height\") + ref(\"size-peer\",\"\",\"artboard.width\")","version":1},"replace_driver":true}
    ]})json");
    check(expression_json.find("\"ok\":true")!=std::string::npos&&session.revision()==4,
        "JSON Session installs a versioned Artboard numeric expression through the dedicated command");
    const auto expressed=artboard_size_property(session.document(),child_width);
    check(expressed.literal==320&&!expressed.driver&&expressed.expression==combined&&
        expressed.source_kind=="expression"&&expressed.evaluated==860,
        "Expression evaluation retains the local fallback and exposes its exact authored source");
    const auto expression_get=request(session,R"({"op":"get","ref":{"object":"size-child","point":"","field":"artboard.width"}})");
    check(expression_get.find("\"source_kind\":\"expression\"")!=std::string::npos&&
        expression_get.find("\"expression\":{\"source\":\"ref(\\\"size-parent\\\",\\\"\\\",\\\"artboard.height\\\") + ref(\\\"size-peer\\\",\\\"\\\",\\\"artboard.width\\\")\",\"version\":1}")!=std::string::npos&&
        expression_get.find("\"driver\":null")!=std::string::npos,
        "Typed get returns exact expression text and leaves link driver null");

    const auto update_json=request(session,R"json({"op":"apply","expected_revision":4,"commands":[
      {"type":"update_artboard","composition":"size-driver-comp","artboard":{"id":"size-child","name":"Renamed child","x":12,"y":8,"width":320,"height":240,"parent_size":{"artboard":"size-parent","width":false,"height":true},"width_driver":{"expression":{"source":"ref(\"size-parent\",\"\",\"artboard.height\") + ref(\"size-peer\",\"\",\"artboard.width\")","version":1}}}}
    ]})json");
    check(update_json.find("\"ok\":true")!=std::string::npos&&
        artboard_size_property(session.document(),child_width).expression==combined&&resolved("size-child").x==12,
        "Full-record UpdateArtboard accepts an exact existing driver and preserves it during unrelated edits");
    const auto expression_delete_state=encode(session.document());const auto expression_delete_revision=session.revision();
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{"size-driver-comp","size-parent"},
        AddArtboard{"size-driver-comp",{"size-parent","Replacement",0,0,200,200},2}});});
    check(session.revision()==expression_delete_revision&&encode(session.document())==expression_delete_state,
        "An expression dependent cannot silently bind to a replacement with the same Artboard ID");
    const auto before_rejections=encode(session.document());const auto rejection_revision=session.revision();const auto history=session.history();
    auto edited=child_authored();auto literal_edit=edited;literal_edit.width=321;
    rejects("DRIVEN_ARTBOARD_SIZE",[&]{apply({UpdateArtboard{"size-driver-comp",literal_edit}});});
    auto driver_smuggle=edited;driver_smuggle.width_driver=ArtboardSizeDriver{parent_width};
    rejects("ARTBOARD_DRIVER_SMUGGLING",[&]{apply({UpdateArtboard{"size-driver-comp",driver_smuggle}});});
    rejects("DRIVEN_ARTBOARD_SIZE",[&]{apply({SetArtboardSizeExpression{child_width,{"1",1},false}});});
    rejects("ARTBOARD_SIZE_RANGE",[&]{apply({SetArtboardSizeExpression{child_width,{"0",1},true}});});
    rejects("UNSUPPORTED_EXPRESSION_VERSION",[&]{apply({SetArtboardSizeExpression{child_width,{"1",2},true}});});
    rejects("WRONG_COMPOSITION",[&]{apply({LinkArtboardSize{child_width,other_width,true}});});
    rejects("MISSING_ARTBOARD",[&]{apply({LinkArtboardSize{child_width,{"missing","","artboard.width"},true}});});
    rejects("TYPE_MISMATCH",[&]{apply({LinkArtboardSize{child_width,{"size-circle","","artboard.width"},true}});});
    rejects("INVALID_ARTBOARD_REF",[&]{apply({LinkArtboardSize{Ref{"size-child","point","artboard.width"},parent_width,true}});});
    rejects("INVALID_ARTBOARD_REF",[&]{apply({LinkArtboardSize{child_width,{"size-parent","","artboard.x"},true}});});
    rejects("WRONG_COMPOSITION",[&]{apply({SetArtboardSizeExpression{child_width,{"ref(\"size-other-art\",\"\",\"artboard.width\")",1},true}});});
    rejects("UNIT_MISMATCH",[&]{apply({SetArtboardSizeExpression{child_width,{"ref(\"size-circle\",\"\",\"generator.rotation\")",1},true}});});
    rejects("ARTBOARD_SELF_LINK",[&]{apply({LinkArtboardSize{child_width,child_width,true}});});
    rejects("MISSING_REFERENCE",[&]{apply({Set{child_width,500}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{child_width,{{"size-parent","","artboard.width"},1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{child_width},{"1",1},false}});});
    rejects("CROSS_TYPE_DEPENDENCY",[&]{apply({SetExpression{{scalar_target},{"ref(\"size-child\",\"\",\"artboard.width\")",1},false}});});
    check(session.revision()==rejection_revision&&session.history()==history&&encode(session.document())==before_rejections,
        "Invalid refs, units, versions, ranges, type mixing, direct edits and stale drivers reject atomically");

    apply({LinkArtboardSize{child_height,child_width,true}});
    const auto before_cycle=encode(session.document());const auto cycle_revision=session.revision();
    rejects("ARTBOARD_CYCLE",[&]{apply({SetArtboardSizeExpression{child_width,{"ref(\"size-child\",\"\",\"artboard.height\")",1},true}});});
    check(session.revision()==cycle_revision&&encode(session.document())==before_cycle,
        "Mixed cross-dimension expression and link cycles leave the previous evaluation and authored state intact");
    apply({UnlinkArtboardSize{child_height}});
    auto child_state=child_authored();child_state.parent_size->height=true;
    apply({UpdateArtboard{"size-driver-comp",child_state}});
    const auto parent_cycle_before=encode(session.document());const auto parent_cycle_revision=session.revision();
    rejects("ARTBOARD_CYCLE",[&]{apply({LinkArtboardSize{parent_height,child_height,false}});});
    check(session.revision()==parent_cycle_revision&&encode(session.document())==parent_cycle_before,
        "Parent-size and typed-link cycles are validated together");

    auto child_override=child_authored();child_override.height=300;child_override.parent_size->height=false;
    apply({UpdateArtboard{"size-driver-comp",child_override}});
    check(resolved("size-child").height==300,"Legacy parent_size local override remains available through UpdateArtboard");
    child_override=child_authored();child_override.parent_size->height=true;
    apply({UpdateArtboard{"size-driver-comp",child_override}});
    check(resolved("size-child").height==660,"Parent_size inheritance can be explicitly reset after a local override");
    apply({DetachArtboardParent{"size-driver-comp","size-child"}});
    check(!child_authored().parent_size&&child_authored().height==660&&
        child_authored().width==320&&child_authored().width_driver&&
        artboard_size_property(session.document(),child_width).source_kind=="expression"&&resolved("size-child").width==860,
        "Detach freezes inherited height but retains the independent typed width expression and fallback literal");
    parent_board=session.document().compositions.front().artboards.back();parent_board.height=900;
    apply({UpdateArtboard{"size-driver-comp",parent_board}});
    check(resolved("size-child").height==660&&resolved("size-child").width==1100,
        "After detach, only the independent expression continues following its exact source");
    apply({UnlinkArtboardSize{child_width}});
    check(artboard_size_property(session.document(),child_width).source_kind=="literal"&&
        artboard_size_property(session.document(),child_width).literal==1100,
        "Typed unlink freezes the evaluated size into the authored literal");
    session.undo(session.revision());
    check(artboard_size_property(session.document(),child_width).source_kind=="expression"&&resolved("size-child").width==1100,
        "Undo restores the exact typed expression and its evaluation");
    session.redo(session.revision());

    auto native_document=session.document();
    auto native_child=std::find_if(native_document.compositions.front().artboards.begin(),native_document.compositions.front().artboards.end(),
        [](const auto& board){return board.id=="size-child";});
    native_child->width_driver=ArtboardSizeDriver{combined};
    auto typed_native=encode(native_document);
    check(typed_native.find("\"version\":\"0.66\"")!=std::string::npos&&
        typed_native.find("\"width_driver\":{\"expression\":")!=std::string::npos&&
        encode(decode(typed_native))==typed_native,
        "Native 0.43 stores and cold-roundtrips the additive Artboard expression driver");
    auto lied_version=test_support::without_empty_presets_for_legacy_fixture(typed_native);
    check(replace_all(lied_version,"\"version\":\"0.66\"","\"version\":\"0.32\"")==1,
        "Native version-lie fixture changes only the version tag");
    rejects("UNKNOWN_FIELD",[&]{(void)decode(lied_version);});
    auto literal_document=session.document();
    auto literal_child=std::find_if(literal_document.compositions.front().artboards.begin(),literal_document.compositions.front().artboards.end(),
        [](const auto& board){return board.id=="size-child";});
    literal_child->width_driver.reset();literal_child->height_driver.reset();
    auto legacy=test_support::without_empty_presets_for_legacy_fixture(encode(literal_document));
    check(replace_all(legacy,"\"version\":\"0.66\"","\"version\":\"0.32\"")==1&&
        decode(legacy)==literal_document,
        "Native 0.32 still reopens literal and parent_size Artboards without driver fields");
    auto malformed=typed_native;
    check(replace_all(malformed,"\"width_driver\":{\"expression\":","\"width_driver\":{\"extra\":true,\"expression\":")==1,
        "Malformed native fixture adds one unknown driver field");
    rejects("UNKNOWN_FIELD",[&]{(void)decode(malformed);});
    const auto malformed_driver_request=request(session,R"({"op":"apply","expected_revision":99,"commands":[
      {"type":"link_artboard_size","target":{"object":"size-child","point":"","field":"artboard.width"},"source":{"object":"size-parent","point":"","field":"artboard.width"},"replace_driver":true,"relative":false}
    ]})");
    check(malformed_driver_request.find("\"code\":\"UNKNOWN_FIELD\"")!=std::string::npos,
        "Dedicated Artboard command rejects unrecognized fields at the JSON boundary");

    apply({DeleteArtboard{"size-driver-comp","size-parent"}});
    check(std::none_of(session.document().compositions.front().artboards.begin(),session.document().compositions.front().artboards.end(),
        [](const auto& board){return board.id=="size-parent";}),
        "Unlinking the surviving dependent before deletion permits removing its former source atomically");

    auto smuggled=Artboard{"size-smuggled","Smuggled",0,0,100,100};
    smuggled.width_driver=ArtboardSizeDriver{Ref{"size-peer","","artboard.width"}};
    rejects("ARTBOARD_DRIVER_SMUGGLING",[&]{apply({AddArtboard{"size-driver-comp",smuggled,2}});});
    auto stale=Artboard{"stale","Stale",0,0,10,10};
    rejects("REVISION_CONFLICT",[&]{session.apply({AddArtboard{"size-driver-comp",stale,2},LinkArtboardSize{{"stale","","artboard.width"},peer_width,false}},session.revision()-1);});
}
}
int main(){try{
    layout_and_guide_acceptance();
    layout_typed_reads();
    margin_left_artboard_driver();
    margin_top_artboard_driver();
    margin_right_artboard_driver();
    margin_bottom_artboard_driver();
    margin_top_expression();
    margin_right_expression();
    margin_bottom_expression();
    margin_left_expression();
    grid_bounds_x_artboard_driver();
    grid_bounds_y_artboard_driver();
    grid_bounds_width_artboard_driver();
    grid_column_gutter_artboard_link();
    grid_columns_same_field_link();
    grid_columns_expression();
    grid_rows_expression();
    grid_rows_same_field_link();
    grid_row_gutter_artboard_link();
    grid_row_gutter_expression();
    grid_column_gutter_expression();
    grid_bounds_height_artboard_link();
    grid_bounds_height_expression();
    grid_bounds_width_expression();
    grid_bounds_x_expression();
    grid_bounds_y_expression();
    guide_position_links();
    artboard_size_drivers();
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
    const Ref child_width{"second","","artboard.width"},child_height{"second","","artboard.height"};
    const auto size=artboard_size_property(s.document(),child_width);
    check(size.literal==320&&size.evaluated==960&&size.driver==Ref{"first","","artboard.width"},
        "Typed Artboard width exposes its own literal and stable parent dependency");
    check(artboard_size_property(s.document(),child_height).literal==240&&
        !artboard_size_property(s.document(),child_height).driver,
        "Unlinked height remains locally authored");
    const auto discovered=properties(s.document());
    check(resolve_name(s.document(),"Alternate crop","","artboard.width")==child_width&&
        std::find(discovered.begin(),discovered.end(),child_width)!=discovered.end(),
        "Typed Artboard dimensions are discoverable by stable Ref and name");
    const auto typed=request(s,R"({"op":"get","ref":{"object":"second","point":"","field":"artboard.width"}})");
    check(typed.find("\"type\":\"number\"")!=std::string::npos&&
        typed.find("\"driver\":{")!=std::string::npos&&typed.find("\"artboard.width\"")!=std::string::npos,
        "API typed Artboard read reports numeric type and parent dependency");
    rejects("INVALID_ARTBOARD_REF",[&]{(void)artboard_size_property(s.document(),{"second","point","artboard.width"});});
    rejects("UNKNOWN_ARTBOARD_PROPERTY",[&]{(void)artboard_size_property(s.document(),{"second","","artboard.x"});});
    rejects("MISSING_ARTBOARD",[&]{(void)artboard_size_property(s.document(),{"missing","","artboard.width"});});
    auto duplicate_names=s.document();duplicate_names.compositions.back().artboards.front().name="Alternate crop";
    rejects("AMBIGUOUS_NAME",[&]{(void)resolve_name(duplicate_names,"Alternate crop","","artboard.width");});
    const auto before_typed_rejection=encode(s.document());
    const auto revision_before_typed_rejection=s.revision();
    rejects("MISSING_REFERENCE",[&]{apply({Set{child_width,600}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{child_width,{{"first","","artboard.width"},1,0,"copy_local_value"}}});});
    check(encode(s.document())==before_typed_rejection&&s.revision()==revision_before_typed_rejection,
        "Generic Scalar commands reject Artboard dimensions without mutation");
    auto first=frame("first");first.name="Primary crop";first.width=640;first.height=480;apply({UpdateArtboard{"comp",first}});
    check(frame("second").width==640&&frame("second").height==240&&frame("third").width==640,"Editing parent propagates inherited dimensions only");
    check(artboard_size_property(s.document(),child_width).literal==320&&artboard_size_property(s.document(),child_width).evaluated==640,
        "Typed child width follows parent without replacing its authored literal");
    check(resolve_name(s.document(),"Primary crop","","artboard.width")==Ref{"first","","artboard.width"}&&
        artboard_size_property(s.document(),child_width).driver==Ref{"first","","artboard.width"},
        "Artboard rename leaves the stable parent size Ref unchanged");
    second.width=300;second.parent_size->width=false;apply({UpdateArtboard{"comp",second}});
    check(frame("third").width==300,"Child width override propagates to its own children");
    second.parent_size->width=true;apply({UpdateArtboard{"comp",second}});
    check(frame("second").width==640&&frame("third").width==640,"Reset dimension override follows parent again");
    auto crop=export_svg(s.document(),"comp","second");
    check(crop.find("viewBox=\"200 100 640 240\"")!=std::string::npos,"SVG exports resolved size and exact requested crop");
    apply({ReorderArtboards{"comp",{"third","second","first"}}});
    check(s.document().compositions.front().artboards.front().id=="third"&&export_svg(s.document(),"comp","second")==crop&&evaluate(s.document())==authored_values,"Page order changes neither crops nor any artwork property");
    const auto stored=encode(s.document());
    rejects("ARTBOARD_IN_USE",[&]{apply({DeleteArtboard{"comp","first"}});});
    auto cycle=first;cycle.parent_size=ArtboardParent{"third",true,true};
    rejects("ARTBOARD_CYCLE",[&]{apply({UpdateArtboard{"comp",cycle}});});
    auto cross=second;cross.parent_size->artboard="other";
    rejects("MISSING_ARTBOARD",[&]{apply({UpdateArtboard{"comp",cross}});});
    rejects("INVALID_ORDER",[&]{apply({ReorderArtboards{"comp",{"first","first","third"}}});});
    rejects("LAST_ARTBOARD",[&]{apply({DeleteArtboard{"second-comp","other"}});});
    check(encode(s.document())==stored,"Failed deletion/order/cycle/cross-plane mutations are atomic");
    apply({DetachArtboardParent{"comp","second"},DeleteArtboard{"comp","first"}});
    check(!frame("second").parent_size&&frame("second").width==640&&frame("second").height==240&&frame("third").width==640,"Detach freezes effective size and permits deleting former parent");
    check(!artboard_size_property(s.document(),child_width).driver&&artboard_size_property(s.document(),child_width).literal==640,
        "Typed dependency readback reflects explicit Detach freeze");
    s.undo(s.revision());check(frame("second").parent_size&&frame("first").width==640,"Undo restores parent, bindings and page order together");
    auto moved=second;moved.x=700;moved.y=-80;apply({UpdateArtboard{"comp",moved}});
    check(frame("second").x==700&&frame("second").y==-80&&evaluate(s.document())==authored_values,"Explicit frame move changes crop without moving artwork");
    const auto encoded=encode(s.document());check(encode(decode(encoded))==encoded,"Parent frame metadata and local overrides reopen exactly");
    const auto readback=request(s,R"({"op":"artboards","composition":"comp"})");
    check(readback.find("\"authored\"")!=std::string::npos&&readback.find("\"evaluated\"")!=std::string::npos,"Semantic API exposes frame inheritance independently of stored overrides");
    std::cout<<"PASS "<<checks<<" artboard checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
