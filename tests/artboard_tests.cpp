#include "nect/io.hpp"
#include <iostream>
#include <limits>
#include <utility>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection");}
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
    check(current.find("\"version\":\"0.15\"")!=std::string::npos&&encode(decode(current))==current,
        "Native 0.15 roundtrip preserves Guide/Grid/Margin definitions and IDs");

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
    check(replace_all(legacy,"\"version\":\"0.15\"","\"version\":\"0.13\"")==1,
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
}
int main(){try{
    layout_and_guide_acceptance();
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
