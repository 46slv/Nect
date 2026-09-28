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
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error(std::string("Expected ")+code+" rejection");}
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
    check(current.find("\"version\":\"0.33\"")!=std::string::npos&&encode(decode(current))==current,
        "Native 0.33 roundtrip preserves Guide/Grid/Margin definitions and IDs");

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
    check(replace_all(legacy,"\"version\":\"0.33\"","\"version\":\"0.13\"")==1,
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
    check(position.literal==240&&position.driver==source&&position.evaluated==100,
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
        get.find("\"link\":true")!=std::string::npos&&get.find("\"expression\":false")!=std::string::npos,
        "Guide get returns the typed authored and evaluated position view");
    const auto property_list=request(session,R"({"op":"properties"})");
    check(property_list.find("\"object\":\"guide-target\"")!=std::string::npos&&
        property_list.find("\"field\":\"guide.position\"")!=std::string::npos,
        "Properties lists Guide positions through their typed view");

    session.undo(session.revision());
    check(!guide_position_property(session.document(),target).driver&&guide_position_property(session.document(),target).evaluated==240,
        "Undo removes only the Guide position link and restores its literal value");
    session.redo(session.revision());
    check(guide_position_property(session.document(),target).driver==source&&
        guide_position_property(session.document(),target).evaluated==100,
        "Redo restores the stable Guide position Ref and evaluation");

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
    check(native.find("\"version\":\"0.33\"")!=std::string::npos&&
        native.find("\"position_driver\":{\"link\":{\"object\":\"guide-source\",\"point\":\"\",\"field\":\"guide.position\"}}")!=std::string::npos&&
        encode(decode(native))==native,
        "Native 0.33 preserves an optional Guide position link and its authored literal");
    auto legacy_with_driver=native;
    check(replace_all(legacy_with_driver,"\"version\":\"0.33\"","\"version\":\"0.22\"")==1,
        "Legacy linked fixture downgrades only its version tag");
    rejects("INVALID_GUIDE",[&]{(void)decode(legacy_with_driver);});
    auto literal_document=session.document();
    for(auto& composition:literal_document.compositions)for(auto& guide:composition.guides)guide.position_driver.reset();
    auto legacy_literal=encode(literal_document);
    check(replace_all(legacy_literal,"\"version\":\"0.33\"","\"version\":\"0.22\"")==1&&
        decode(legacy_literal)==literal_document,
        "Native 0.22 continues to decode literal-only Guide positions unchanged");

    const auto before_failures=encode(session.document());const auto revision_before_failures=session.revision();
    const auto history_before_failures=session.history();
    rejects("DRIVEN_GUIDE_POSITION",[&]{apply({UpdateGuide{"guide-link-comp",{"guide-target","Changed value","x",250}}});});
    rejects("DRIVEN_GUIDE_POSITION",[&]{apply({LinkGuidePosition{target,alternate,false}});});
    rejects("GUIDE_AXIS_MISMATCH",[&]{apply({LinkGuidePosition{target,horizontal,true}});});
    rejects("WRONG_COMPOSITION",[&]{apply({LinkGuidePosition{target,other_plane,true}});});
    rejects("INVALID_GUIDE_REF",[&]{apply({LinkGuidePosition{target,{"guide-source","point","guide.position"},true}});});
    rejects("INVALID_GUIDE_REF",[&]{apply({LinkGuidePosition{target,{"guide-source","","artboard.width"},true}});});
    rejects("GUIDE_SELF_LINK",[&]{apply({LinkGuidePosition{target,target,true}});});
    rejects("MISSING_GUIDE",[&]{apply({LinkGuidePosition{target,{"missing-guide","","guide.position"},true}});});
    rejects("TYPE_MISMATCH",[&]{apply({LinkGuidePosition{target,{"guide-link-art","","guide.position"},true}});});
    rejects("TYPE_MISMATCH",[&]{apply({Set{target,280}});});
    rejects("TYPE_MISMATCH",[&]{apply({Link{target,{source,1,0,"copy_local_value"}}});});
    rejects("TYPE_MISMATCH",[&]{apply({Unlink{target}});});
    rejects("TYPE_MISMATCH",[&]{apply({SetExpression{{target},{"1",1},false}});});
    auto smuggled=Guide{"guide-smuggled","Smuggled","x",15};smuggled.position_driver=source;
    rejects("GUIDE_DRIVER_SMUGGLING",[&]{apply({AddGuide{"guide-link-comp",smuggled}});});
    rejects("GUIDE_CYCLE",[&]{apply({LinkGuidePosition{source,target,false}});});
    rejects("MISSING_GUIDE",[&]{apply({DeleteGuide{"guide-link-comp","guide-source"}});});
    check(session.revision()==revision_before_failures&&session.history()==history_before_failures&&
        encode(session.document())==before_failures,
        "Invalid Guide links, generic Scalar commands and dependent deletion preserve bytes, revision and Undo history");

    const auto freeze_value=guide_position_property(session.document(),target).evaluated;
    apply({UnlinkGuidePosition{target}});
    apply({UpdateGuide{"guide-link-comp",{"guide-source","Renamed source","x",500}}});
    check(!guide_position_property(session.document(),target).driver&&
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
    check(replace_all(malformed_parent_native,"\"version\":\"0.33\"","\"version\":\"0.32\"")==1,
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
    check(typed_native.find("\"version\":\"0.33\"")!=std::string::npos&&
        typed_native.find("\"width_driver\":{\"expression\":")!=std::string::npos&&
        encode(decode(typed_native))==typed_native,
        "Native 0.33 stores and cold-roundtrips the additive Artboard expression driver");
    auto lied_version=typed_native;
    check(replace_all(lied_version,"\"version\":\"0.33\"","\"version\":\"0.32\"")==1,
        "Native version-lie fixture changes only the version tag");
    rejects("UNKNOWN_FIELD",[&]{(void)decode(lied_version);});
    auto literal_document=session.document();
    auto literal_child=std::find_if(literal_document.compositions.front().artboards.begin(),literal_document.compositions.front().artboards.end(),
        [](const auto& board){return board.id=="size-child";});
    literal_child->width_driver.reset();literal_child->height_driver.reset();
    auto legacy=encode(literal_document);
    check(replace_all(legacy,"\"version\":\"0.33\"","\"version\":\"0.32\"")==1&&
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
