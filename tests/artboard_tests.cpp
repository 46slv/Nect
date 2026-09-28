#include "nect/io.hpp"
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
    check(current.find("\"version\":\"0.42\"")!=std::string::npos&&encode(decode(current))==current,
        "Native 0.42 roundtrip preserves Guide/Grid/Margin definitions and IDs");

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
    auto legacy=encode(legacy_document);
    check(replace_all(legacy,"\"version\":\"0.42\"","\"version\":\"0.13\"")==1,
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
        check(response.find(integer?"\"type\":\"integer\"":"\"type\":\"number\"")!=std::string::npos&&
            response.find(integer?"\"unit\":\"unitless\"":"\"unit\":\"du\"")!=std::string::npos&&
            response.find("\"space\":\"artboard_local\"")!=std::string::npos&&
            response.find("\"origin\":\"authored\"")!=std::string::npos&&
            response.find(ref.field=="margin.left"||ref.field=="margin.top"||ref.field=="grid.bounds.x"||ref.field=="grid.bounds.y"?"\"link\":true":"\"link\":false")!=std::string::npos&&
            response.find(ref.field=="margin.left"||ref.field=="margin.top"||ref.field=="grid.bounds.x"||ref.field=="grid.bounds.y"?"\"expression\":true":"\"expression\":false")!=std::string::npos,
            "Typed get reports each layout field's type, unit, space and link capability");
        check(response.find("\"authored\":{\"literal\":")!=std::string::npos&&
            response.find("\"evaluated\":")!=std::string::npos,
            "Typed get exposes authored literal and evaluated read separately");
        if(ref.field=="margin.left")check(response.find("\"source_kind\":\"literal\"")!=std::string::npos,
            "Literal Margin left explicitly identifies its inactive source kind");
        if(ref.field=="grid.columns")check(response.find("\"authored\":{\"literal\":2}")!=std::string::npos&&
            response.find("\"evaluated\":2")!=std::string::npos&&response.find("\"evaluated\":2.0")==std::string::npos,
            "Grid columns serialize as an integer JSON value");
        if(ref.field=="grid.rows")check(response.find("\"authored\":{\"literal\":3}")!=std::string::npos&&
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&encode(cold)==native,
        "Native 0.42 cold decode/re-encode preserves existing Grid and Margin bytes");
    auto expected_after_edit=expected_values;expected_after_edit[0].second=11.0;
    for(const auto& [ref,literal]:expected_after_edit)
        check(artboard_layout_property(cold,ref).literal==literal,
            "Native 0.42 roundtrip preserves every exact typed layout literal");

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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&
        native.find("\"left_driver\"")!=std::string::npos&&native.find("margin-left-source")!=std::string::npos&&
        encode(decode(native))==native,
        "Native 0.42 stores the exact Margin source beside the authored literal and roundtrips bytes");
    auto lied=native;check(replace_all(lied,"\"version\":\"0.42\"","\"version\":\"0.34\"")==1,
        "Version-lie fixture changes only native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto literal_document=empty_document("margin-left-legacy-doc","margin-left-legacy-comp","margin-left-legacy-art");
    ArtboardLayout literal_layout;literal_layout.margin=Margin{40,20,40,20};
    Session literal_session(literal_document);literal_session.apply({SetArtboardLayout{"margin-left-legacy-comp",
        "margin-left-legacy-art",literal_layout}},0);
    auto legacy=encode(literal_session.document());
    check(replace_all(legacy,"\"version\":\"0.42\"","\"version\":\"0.35\"")==1&&
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&native.find("\"top_driver\"")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.42 stores the closed Margin top source and byte-roundtrips");
    auto lied=native;check(replace_all(lied,"\"version\":\"0.42\"","\"version\":\"0.40\"")==1,
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&
        native.find("\"top_expression\":{\"source\":\"ref(\\\"margin-top-expression-source\\\",\\\"\\\",\\\"artboard.height\\\") + 10\",\"version\":1}")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.42 preserves exact Margin top expression source and byte-roundtrips");
    auto lied=native;check(replace_all(lied,"\"version\":\"0.42\"","\"version\":\"0.41\"")==1,
        "Version-lie fixture changes only the native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto legacy_document=session.document();
    auto& legacy_target=*std::find_if(legacy_document.compositions.front().artboards.begin(),
        legacy_document.compositions.front().artboards.end(),[&](const Artboard& value){return value.id==target.object;});
    legacy_target.layout->margin->top_expression.reset();legacy_target.layout->margin->top=40;
    auto legacy=encode(legacy_document);
    check(replace_all(legacy,"\"version\":\"0.42\"","\"version\":\"0.41\"")==1&&decode(legacy)==legacy_document,
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&
        native.find("\"left_expression\"")!=std::string::npos&&native.find("margin-expression-source-a")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.42 preserves the exact Margin expression beside its authored literal");
    auto lied=native;check(replace_all(lied,"\"version\":\"0.42\"","\"version\":\"0.37\"")==1,
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&
        native.find("\"bounds_x_driver\"")!=std::string::npos&&native.find("grid-x-source")!=std::string::npos&&
        encode(decode(native))==native,
        "Native 0.42 stores the closed Grid source beside its authored literal and roundtrips bytes");
    auto native_036=native;check(replace_all(native_036,"\"version\":\"0.42\"","\"version\":\"0.36\"")==1&&
        decode(native_036)==session.document(),
        "Native 0.36 remains readable with its existing Grid x link and no expression field");
    auto lied=native;check(replace_all(lied,"\"version\":\"0.42\"","\"version\":\"0.35\"")==1,
        "Grid version-lie fixture changes only the native writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto literal_document=empty_document("grid-x-legacy-doc","grid-x-legacy-comp","grid-x-legacy-art");
    ArtboardLayout literal_layout;literal_layout.grid=Grid{"grid-x-legacy-grid",{10,10,500,400},1,1,0,0};
    Session literal_session(literal_document);literal_session.apply({SetArtboardLayout{"grid-x-legacy-comp",
        "grid-x-legacy-art",literal_layout}},0);
    auto legacy=encode(literal_session.document());
    check(replace_all(legacy,"\"version\":\"0.42\"","\"version\":\"0.35\"")==1&&
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&
        native.find("\"bounds_y_driver\"")!=std::string::npos&&native.find("grid-y-source")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.42 preserves the exact optional Grid y link beside its authored literal");
    auto lied=native;check(replace_all(lied,"\"version\":\"0.42\"","\"version\":\"0.38\"")==1,
        "Grid y version-lie fixture changes only the writer version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    auto malformed=native;check(replace_all(malformed,"\"bounds_y_driver\":{","\"bounds_y_driver\":{\"extra\":1,")==1,
        "Malformed Grid y driver fixture adds one unknown property");
    rejects("INVALID_LAYOUT",[&]{(void)decode(malformed);});
    auto legacy_document=session.document();
    auto& legacy_grid=*std::find_if(legacy_document.compositions.front().artboards.begin(),legacy_document.compositions.front().artboards.end(),
        [](const Artboard& value){return value.id=="grid-y-target";})->layout->grid;
    legacy_grid.bounds_y_driver.reset();
    auto native_038=encode(legacy_document);
    check(replace_all(native_038,"\"version\":\"0.42\"","\"version\":\"0.38\"")==1&&decode(native_038)==legacy_document,
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&
        native.find("\"bounds_x_expression\"")!=std::string::npos&&native.find("grid-expression-source-a")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.42 preserves exact Grid expression source and roundtrips bytes");
    auto lied=native;check(replace_all(lied,"\"version\":\"0.42\"","\"version\":\"0.36\"")==1,
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&
        native.find("\"bounds_y_expression\"")!=std::string::npos&&
        encode(decode(native))==native,"Native 0.42 preserves the exact Grid y expression and stable source text");
    auto lied=native;check(replace_all(lied,"\"version\":\"0.42\"","\"version\":\"0.39\"")==1,
        "Grid y expression version-lie fixture changes only native version");
    rejects("INVALID_LAYOUT",[&]{(void)decode(lied);});
    apply({GridBoundsYCommand{UnlinkGridBoundsY{target}}});typed=artboard_layout_property(session.document(),target);
    check(std::get<double>(typed.literal)==60&&!typed.driver&&!typed.expression&&typed.source_kind=="literal"&&
        std::get<double>(typed.evaluated)==60,"Unlink freezes evaluated Grid y and clears its expression in one step");
    const auto literal_document=session.document();auto legacy_039=encode(literal_document);
    check(replace_all(legacy_039,"\"version\":\"0.42\"","\"version\":\"0.39\"")==1&&decode(legacy_039)==literal_document,
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
    check(native.find("\"version\":\"0.42\"")!=std::string::npos&&
        native.find("\"position_driver\":{\"link\":{\"object\":\"guide-source\",\"point\":\"\",\"field\":\"guide.position\"}}")!=std::string::npos&&
        encode(decode(native))==native,
        "Native 0.42 preserves the existing optional Guide position link and authored literal");
    auto legacy_with_driver=native;
    check(replace_all(legacy_with_driver,"\"version\":\"0.42\"","\"version\":\"0.22\"")==1,
        "Legacy linked fixture downgrades only its version tag");
    rejects("INVALID_GUIDE",[&]{(void)decode(legacy_with_driver);});
    auto native_033_link=encode(session.document());
    check(replace_all(native_033_link,"\"version\":\"0.42\"","\"version\":\"0.33\"")==1&&
        decode(native_033_link)==session.document(),
        "Native 0.33 still decodes the existing Guide link representation unchanged");
    auto literal_document=session.document();
    for(auto& composition:literal_document.compositions)for(auto& guide:composition.guides)guide.position_driver.reset();
    auto legacy_literal=encode(literal_document);
    check(replace_all(legacy_literal,"\"version\":\"0.42\"","\"version\":\"0.22\"")==1&&
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
    check(expression_native.find("\"version\":\"0.42\"")!=std::string::npos&&
        expression_native.find("\"position_expression\":{\"source\":\"ref(\\\"guide-source\\\",\\\"\\\",\\\"guide.position\\\") + 20\",\"version\":1}")!=std::string::npos&&
        encode(decode(expression_native))==expression_native,
        "Native 0.42 stores and byte-roundtrips the exact Guide expression while omitting an absent link");
    auto expression_lied_version=expression_native;
    check(replace_all(expression_lied_version,"\"version\":\"0.42\"","\"version\":\"0.33\"")==1,
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
    auto malformed_parent_native=encode(empty_document("parent-chain-doc","parent-chain-comp","parent-chain-art"));
    const auto parent_height_marker=malformed_parent_native.find("\"height\":");
    check(parent_height_marker!=std::string::npos,"Native fixture contains its default Artboard height");
    const auto parent_height_end=malformed_parent_native.find_first_of(",}",parent_height_marker);
    malformed_parent_native.insert(parent_height_end,
        ",\"parent_size\":{\"artboard\":\"missing-parent\",\"width\":false,\"height\":false}");
    check(replace_all(malformed_parent_native,"\"version\":\"0.42\"","\"version\":\"0.32\"")==1,
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
    check(typed_native.find("\"version\":\"0.42\"")!=std::string::npos&&
        typed_native.find("\"width_driver\":{\"expression\":")!=std::string::npos&&
        encode(decode(typed_native))==typed_native,
        "Native 0.42 stores and cold-roundtrips the additive Artboard expression driver");
    auto lied_version=typed_native;
    check(replace_all(lied_version,"\"version\":\"0.42\"","\"version\":\"0.32\"")==1,
        "Native version-lie fixture changes only the version tag");
    rejects("UNKNOWN_FIELD",[&]{(void)decode(lied_version);});
    auto literal_document=session.document();
    auto literal_child=std::find_if(literal_document.compositions.front().artboards.begin(),literal_document.compositions.front().artboards.end(),
        [](const auto& board){return board.id=="size-child";});
    literal_child->width_driver.reset();literal_child->height_driver.reset();
    auto legacy=encode(literal_document);
    check(replace_all(legacy,"\"version\":\"0.42\"","\"version\":\"0.32\"")==1&&
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
    margin_top_expression();
    margin_left_expression();
    grid_bounds_x_artboard_driver();
    grid_bounds_y_artboard_driver();
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
