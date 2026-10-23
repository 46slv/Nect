#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <boost/json.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace nect;
namespace {
int checks=0;
void check(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
    ++checks;
}
void near(double actual,double expected,const std::string& message) {
    check(std::abs(actual-expected)<1e-8,message+" (actual="+std::to_string(actual)+
        ", expected="+std::to_string(expected)+")");
}
template<class F>void rejects(const char* code,F action) {
    try {action();} catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands) {
    session.apply(commands,session.revision());
}
std::string checked_request(Session& session,const std::string& body) {
    const auto reply=nect::request(session,body);
    check(reply.find("\"ok\":true")!=std::string::npos,body+" -> "+reply);
    return reply;
}
std::string raw_request(Session& session,const std::string& body) {
    return nect::request(session,body);
}
void failed_atomically(Session& session,const char* code,const std::vector<Command>& commands) {
    const auto bytes=encode(session.document());const auto revision=session.revision();const auto history=session.history();
    rejects(code,[&]{session.apply(commands,revision);});
    check(encode(session.document())==bytes&&session.revision()==revision&&session.history()==history,
        std::string(code)+" preserves exact authored bytes, revision, and history");
}
Document guide_document() {
    auto document=empty_document("guide-doc","comp","S");
    auto& composition=document.compositions.front();
    composition.artboards.front()=Artboard{"S","Source",10,20,400,300};
    composition.artboards.front().local_guides={{"GX","Grid X","x",40,true},{"GY","Grid Y","y",60,true}};
    composition.artboards.push_back(Artboard{"A","A",1000,100,400,300});
    composition.artboards.push_back(Artboard{"B","B",2000,200,400,300});
    composition.guides.push_back(Guide{"GG","Global","x",777});
    return document;
}
const Artboard& board(const Document& document,const Id& id) {
    const auto& artboards=document.compositions.front().artboards;
    const auto found=std::find_if(artboards.begin(),artboards.end(),[&](const Artboard& item){return item.id==id;});
    if(found==artboards.end())throw std::runtime_error("Missing Artboard "+id);
    return *found;
}
EffectiveArtboardGuide occurrence(const Document& document,const Id& artboard,const Id& guide) {
    const auto& composition=document.compositions.front();
    const auto values=effective_artboard_guides(document,composition.id,artboard);
    const auto found=std::find_if(values.begin(),values.end(),[&](const auto& item){return item.guide_id==guide;});
    if(found==values.end())throw std::runtime_error("Missing effective Guide "+guide+" on "+artboard);
    return *found;
}
Id stable_guide_id(const Id& prefix,const Id& source_guide) {
    std::uint64_t hash=14695981039346656037ull;
    for(const unsigned char byte:source_guide){hash^=byte;hash*=1099511628211ull;}
    constexpr char digits[]="0123456789abcdef";std::string encoded(16,'0');
    for(std::size_t index=0;index<16;++index) {
        encoded[15-index]=digits[hash&0xf];hash>>=4;
    }
    return prefix+"-guide-"+encoded;
}
void fixture_and_native_roundtrip() {
    Session session(guide_document());
    apply(session,{StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",
        ArtboardTemplate{"T","Shared guides","S",std::nullopt}}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","T",std::nullopt}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","B","T",std::nullopt}}},
        StructuralCommand{ArtboardGuideCommand{AddArtboardGuide{"comp","A",{"GA","Local A","x",15,true}}}},
        StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",90.0}}},
        StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","B","GY","enabled",false}}}});
    const auto& a=occurrence(session.document(),"A","GX");
    const auto& b=occurrence(session.document(),"B","GY");
    near(a.position,90,"A GX begins at its target-local override");
    near(b.position,60,"B inherits GY position independently from its enabled override");
    check(a.inherited&&a.position_overridden&&!a.enabled_overridden&&a.source_artboard=="S"&&
        a.template_source_artboard=="S"&&a.axis=="x"&&a.name=="Grid X",
        "Effective occurrence retains original identity and exposes only A's position override");
    check(b.inherited&&!b.position_overridden&&b.enabled_overridden&&!b.enabled,
        "B's enabled override leaves its position inherited");
    check(occurrence(session.document(),"A","GA").position==15&&
        evaluate_guide_positions(session.document(),"comp").at("GG")==777,
        "Target-local GA and the existing global Guide evaluator remain separate");
    near(evaluate_artboard(session.document().compositions.front(),"A").x+
        occurrence(session.document(),"A","GA").position,1015,
        "A local target Guide is measured from its frame origin");

    apply(session,{StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GX","Renamed X","x",50,true}}}},
        StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GY","Renamed Y","y",80,true}}}}});
    near(1000+occurrence(session.document(),"A","GX").position,1090,"Packet oracle A GX world x remains overridden");
    near(100+occurrence(session.document(),"A","GY").position,180,"Packet oracle A GY follows the changed source");
    near(2000+occurrence(session.document(),"B","GX").position,2050,"Packet oracle B GX follows the changed source");
    near(200+occurrence(session.document(),"B","GY").position,280,"Packet oracle B retains local GY position 80");
    near(10+occurrence(session.document(),"S","GX").position,60,"Updated source GX has fixed world x 60");
    near(20+occurrence(session.document(),"S","GY").position,100,"Updated source GY has fixed world y 100");
    check(!occurrence(session.document(),"B","GY").enabled&&occurrence(session.document(),"A","GX").name=="Renamed X",
        "Renaming propagates through position override and enabled false remains target-local");
    apply(session,{StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GY","Renamed Y","y",80,false}}}}});
    check(!occurrence(session.document(),"A","GY").enabled&&!occurrence(session.document(),"B","GY").enabled,
        "Source enabled changes propagate to A while B's independent false override remains false");
    apply(session,{StructuralCommand{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","B","GY","enabled"}}}});
    check(!occurrence(session.document(),"B","GY").enabled,
        "Reset enabled follows the current disabled source literal");
    apply(session,{StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GY","Renamed Y","y",80,true}}}}});
    check(occurrence(session.document(),"A","GY").enabled&&occurrence(session.document(),"B","GY").enabled,
        "Source re-enable reaches both unoverridden targets after reset");
    apply(session,{StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","B","GY","enabled",false}}}});
    apply(session,{StructuralCommand{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","A","GX","position"}}}});
    near(1000+occurrence(session.document(),"A","GX").position,1050,"Reset A GX position returns exactly to its source value");
    apply(session,{StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",90.0}}}});
    apply(session,{StructuralCommand{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","B","GY","enabled"}}}});
    check(occurrence(session.document(),"B","GY").enabled,
        "Reset B GY enabled restores the enabled source state independently of position");
    apply(session,{StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","B","GY","enabled",false}}}});
    apply(session,{StructuralCommand{ArtboardGuideCommand{AddArtboardGuide{"comp","S",{"GZ","Source addition","x",100,true}}}}});
    check(occurrence(session.document(),"A","GZ").inherited&&occurrence(session.document(),"B","GZ").inherited,
        "New source Guides follow every assigned target by stable identity");

    auto reordered_document=session.document();
    auto& reordered_composition=reordered_document.compositions.front();
    std::reverse(reordered_composition.artboards.front().local_guides.begin(),
        reordered_composition.artboards.front().local_guides.end());
    Session reordered(std::move(reordered_document));
    auto renamed_source=board(reordered.document(),"S");renamed_source.name="Source renamed";
    apply(reordered,{ReorderArtboards{"comp",{"B","A","S"}},
        StructuralCommand{ArtboardTemplateCommand{RenameArtboardTemplate{"comp","T","Renamed shared source"}}},
        UpdateArtboard{"comp",renamed_source}});
    const auto reordered_occurrences=effective_artboard_guides(reordered.document(),"comp","A");
    check(reordered_occurrences.size()==4&&reordered_occurrences[0].guide_id=="GZ"&&
        reordered_occurrences[1].guide_id=="GY"&&reordered_occurrences[2].guide_id=="GX"&&
        reordered_occurrences[3].guide_id=="GA"&&occurrence(reordered.document(),"A","GX").position==90&&
        occurrence(reordered.document(),"A","GX").source_artboard=="S"&&
        board(reordered.document(),"S").name=="Source renamed"&&
        reordered.document().compositions.front().templates.front().name=="Renamed shared source",
        "Guide and Artboard reorder/rename preserve scoped source IDs, order and overrides");
    const auto scoped_ref=Ref{"A","GX","artboard.guide.position"};
    const auto property_refs=properties(session.document());
    check(std::find(property_refs.begin(),property_refs.end(),scoped_ref)!=property_refs.end(),
        "Property listing exposes the scoped target/source Guide Ref");
    const auto property=artboard_guide_property(session.document(),scoped_ref);
    check(property.occurrence.source_artboard=="S"&&property.occurrence.template_source_artboard=="S"&&
        property.occurrence.position_overridden&&std::get<double>(property.evaluated)==90,
        "Typed property reports source lineage, immediate template source and effective value");

    const auto native=encode(session.document());
    check(native.find(test_support::current_native_version_marker())!=std::string::npos&&
        decode(native)==session.document()&&encode(decode(native))==native,
        "Current-writer roundtrip canonically preserves Artboard Guides and Template overrides");
    auto duplicated=native;const auto override_key=duplicated.find("\"guide_position_overrides\":[");
    check(override_key!=std::string::npos,"Native writer persists position overrides");
    const auto open=duplicated.find('[',override_key),close=duplicated.find(']',open);
    const auto entry=duplicated.substr(open+1,close-open-1);duplicated.insert(close,","+entry);
    rejects("DUPLICATE_ARTBOARD_GUIDE_OVERRIDE",[&]{(void)decode(duplicated);});
    auto lied=native;const auto current= lied.find(test_support::current_native_version_marker());
    lied.replace(current,test_support::current_native_version_marker().size(),"\"version\":\"0.76\"");
    rejects("UNKNOWN_FIELD",[&]{(void)decode(lied);});
    auto conflicting=boost::json::parse(native);
    auto& conflicting_boards=conflicting.as_object().at("compositions").as_array().front().as_object().at("artboards").as_array();
    auto& conflicting_assignment=std::find_if(conflicting_boards.begin(),conflicting_boards.end(),[](const auto& item) {
        return item.as_object().at("id").as_string()=="A";
    })->as_object().at("template_assignment").as_object();
    conflicting_assignment["detached_guides"]=boost::json::array{"GX"};
    rejects("INVALID_ARTBOARD_GUIDE_SUPPRESSION",[&]{(void)decode(boost::json::serialize(conflicting));});
    auto missing=boost::json::parse(native);
    auto& missing_boards=missing.as_object().at("compositions").as_array().front().as_object().at("artboards").as_array();
    auto& missing_assignment=std::find_if(missing_boards.begin(),missing_boards.end(),[](const auto& item) {
        return item.as_object().at("id").as_string()=="A";
    })->as_object().at("template_assignment").as_object();
    missing_assignment["guide_position_overrides"].as_array().front().as_object()["guide_id"]="MISSING";
    rejects("MISSING_ARTBOARD_GUIDE_SOURCE",[&]{(void)decode(boost::json::serialize(missing));});

    Session old(guide_document());
    auto empty=old.document();empty.compositions.front().artboards.front().local_guides.clear();
    empty.compositions.front().artboards.resize(1);empty.compositions.front().guides.clear();
    const auto old_native=encode(empty);auto legacy=old_native;
    const auto marker=legacy.find(test_support::current_native_version_marker());
    legacy.replace(marker,test_support::current_native_version_marker().size(),"\"version\":\"0.76\"");
    check(decode(legacy)==empty,"Native 0.76 reader migrates with empty Artboard Guide and override state");
    auto old_template_document=guide_document();
    for(auto& artboard:old_template_document.compositions.front().artboards)artboard.local_guides.clear();
    Session old_template(std::move(old_template_document));
    apply(old_template,{StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"old-T","Old Template","S",{}}}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","old-T",{}}}}});
    auto old_template_bytes=encode(old_template.document());
    const auto old_template_marker=old_template_bytes.find(test_support::current_native_version_marker());
    old_template_bytes.replace(old_template_marker,test_support::current_native_version_marker().size(),"\"version\":\"0.76\"");
    check(decode(old_template_bytes)==old_template.document(),
        "Native 0.76 Template assignment migrates exactly when all new Guide state is absent");
}
void source_frame_move_reprojects_guides_without_moving_artwork() {
    Session session(guide_document());
    apply(session,{CreatePrimitive{"comp","","artwork","Artwork",default_primitive("artwork-source","nect.shape.circle")}});
    const auto before=evaluate_transforms(session.document(),evaluate(session.document())).at("artwork").world;
    near(board(session.document(),"S").x+occurrence(session.document(),"S","GX").position,50,
        "Source local Guide starts at world x 50");
    auto source=board(session.document(),"S");source.x=50;source.y=60;
    apply(session,{UpdateArtboard{"comp",source}});
    const auto after=evaluate_transforms(session.document(),evaluate(session.document())).at("artwork").world;
    check(before==after,"Moving a source Artboard does not move Composition artwork");
    near(board(session.document(),"S").x+occurrence(session.document(),"S","GX").position,90,
        "Source Guide overlay follows only the moved Artboard frame origin");
}
void property_list_matches_scoped_get_for_many_occurrences() {
    auto document=empty_document("list-doc","list-comp","list-source");
    auto& composition=document.compositions.front();
    composition.artboards.front()=Artboard{"list-source","Source",0,0,400,300};
    for(int index=0;index<32;++index)composition.artboards.front().local_guides.push_back(
        {"G"+std::to_string(index),"Guide "+std::to_string(index),index%2?"y":"x",index*10.0,index%3!=0});
    for(int index=0;index<4;++index)composition.artboards.push_back(
        Artboard{"T"+std::to_string(index),"Target "+std::to_string(index),1000.0*index,0,400,300});
    Session session(std::move(document));
    std::vector<Command> commands{StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{
        "list-comp",{"list-template","Many Guides","list-source",{}}}}}};
    for(int index=0;index<4;++index)commands.push_back(
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"list-comp","T"+std::to_string(index),
            "list-template",{}}}});
    apply(session,std::move(commands));
    const auto listed=boost::json::parse(checked_request(session,R"({"op":"properties"})"))
        .as_object().at("result").as_array();
    const auto guide_count=std::count_if(listed.begin(),listed.end(),[](const auto& item) {
        return item.as_object().at("ref").as_object().at("field").as_string().starts_with("artboard.guide.");
    });
    const auto target_guide_count=std::count_if(listed.begin(),listed.end(),[](const auto& item) {
        const auto& ref=item.as_object().at("ref").as_object();
        return ref.at("field").as_string().starts_with("artboard.guide.")&&
            ref.at("object").as_string().starts_with("T");
    });
    check(guide_count==32*5*2&&target_guide_count==32*4*2,
        "Moderate property-list fixture emits both typed fields for all source and target Guides");
    const auto ref=Ref{"T3","G31","artboard.guide.position"};
    const auto listed_item=std::find_if(listed.begin(),listed.end(),[&](const auto& item) {
        const auto& candidate=item.as_object().at("ref").as_object();
        return candidate.at("object").as_string()==ref.object&&candidate.at("point").as_string()==ref.point&&
            candidate.at("field").as_string()==ref.field;
    });
    const auto gotten=boost::json::parse(checked_request(session,
        R"({"op":"get","ref":{"object":"T3","point":"G31","field":"artboard.guide.position"}})"))
        .as_object().at("result").as_object();
    check(listed_item!=listed.end()&&listed_item->as_object().at("evaluated").to_number<double>()==310&&
        listed_item->as_object().at("name").as_string()=="Target 3 / Guide 31"&&
        listed_item->as_object().at("source")==gotten.at("source")&&
        listed_item->as_object().at("evaluated")==gotten.at("evaluated")&&
        listed_item->as_object().at("ref")==gotten.at("ref"),
        "Batched property-list projection matches independent target-scoped get values and display identity");
}
void nested_sources_detach_and_history() {
    auto document=guide_document();auto& composition=document.compositions.front();
    composition.artboards.push_back(Artboard{"C","C",3000,300,400,300});
    Session session(std::move(document));
    apply(session,{
        StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",
            ArtboardTemplate{"T1","S source","S",std::nullopt}}}},
        StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",
            ArtboardTemplate{"T2","B source","B",std::nullopt}}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","B","T1",std::nullopt}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","C","T2",std::nullopt}}},
        StructuralCommand{ArtboardGuideCommand{AddArtboardGuide{"comp","B",{"LB","Local B","x",30,true}}}},
        StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","B","GX","position",90.0}}}
    });
    const auto& nested=occurrence(session.document(),"C","GX");
    near(nested.position,90,"Nested Template C inherits B's effective local position override");
    check(nested.source_artboard=="S"&&nested.template_source_artboard=="B"&&
        !nested.position_overridden,"Nested occurrence keeps authored owner S while its immediate source is B");
    const auto nested_json=checked_request(session,R"({"op":"get","ref":{"object":"C","point":"GX","field":"artboard.guide.position"}})");
    const auto nested_result=boost::json::parse(nested_json).as_object().at("result").as_object();
    const auto immediate_source=nested_result.at("template_source").as_object();
    check(nested_result.at("source_artboard").as_string()=="S"&&
        immediate_source.at("artboard").as_string()=="B"&&
        immediate_source.at("position").to_number<double>()==90&&
        !nested_result.at("position_overridden").as_bool(),
        "Nested typed readback names immediate source B and target-local override false");

    apply(session,{StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GX","Grid X","x",50,true}}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","T1",{}}}},
        StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",90.0}}}});
    apply(session,{StructuralCommand{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","A","GX","position"}}}});
    const auto before_item=encode(session.document());
    apply(session,{StructuralCommand{ArtboardGuideCommand{DetachArtboardGuide{"comp","A","GX","AD"}}}});
    const auto detached=std::find_if(session.document().compositions.front().artboards[1].local_guides.begin(),
        session.document().compositions.front().artboards[1].local_guides.end(),[](const ArtboardGuide& guide){return guide.id=="AD";});
    check(detached!=session.document().compositions.front().artboards[1].local_guides.end()&&detached->position==50&&
        std::find(session.document().compositions.front().artboards[1].template_assignment->detached_guides.begin(),
            session.document().compositions.front().artboards[1].template_assignment->detached_guides.end(),"GX")!=
            session.document().compositions.front().artboards[1].template_assignment->detached_guides.end(),
        "Item detach freezes the effective value and stores source suppression");
    session.undo(session.revision());check(encode(session.document())==before_item,"Undo restores exact item Guide occurrence/override identity");
    session.redo(session.revision());
    const auto& redone=board(session.document(),"A").local_guides;
    check(std::any_of(redone.begin(),redone.end(),[](const ArtboardGuide& guide){return guide.id=="AD"&&guide.position==50;}),
        "Redo restores caller-supplied detached identity");

    apply(session,{StructuralCommand{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","B","GX","position"}}},
        StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GX","Renamed X","x",70,true}}}},
        StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GY","Renamed Y","y",80,true}}}}});
    near(occurrence(session.document(),"A","GY").position,80,"A continues to inherit its non-detached Y occurrence");
    apply(session,{StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","C","GX","position",85.0}}}});
    const auto before_full=encode(session.document());
    failed_atomically(session,"MISSING_ARTBOARD_GUIDE_SOURCE",
        {StructuralCommand{ArtboardTemplateCommand{DetachArtboardTemplate{"comp","B","full-b"}}}});
    apply(session,{StructuralCommand{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","C","GX","position"}}},
        StructuralCommand{ArtboardTemplateCommand{DetachArtboardTemplate{"comp","B","full-b"}}}});
    const auto& detached_b=board(session.document(),"B");
    check(!detached_b.template_assignment&&detached_b.local_guides.size()==3&&
        detached_b.local_guides.front()==ArtboardGuide{"LB","Local B","x",30,true},
        "Full detach preserves pre-existing target-local Guides and materializes inherited enabled/disabled Guides");
    const auto frozen_x=std::find_if(detached_b.local_guides.begin(),detached_b.local_guides.end(),[](const auto& guide) {
        return guide.id=="full-b-guide-09022e07b59c263a";
    });
    const auto frozen_y=std::find_if(detached_b.local_guides.begin(),detached_b.local_guides.end(),[](const auto& guide) {
        return guide.id=="full-b-guide-09022f07b59c27ed";
    });
    check(frozen_x!=detached_b.local_guides.end()&&frozen_y!=detached_b.local_guides.end()&&
        frozen_x->position==70&&frozen_y->position==80,
        "Full detach uses deterministic source-ID identities and freezes current local positions");
    const auto inherited_c=occurrence(session.document(),"C","full-b-guide-09022e07b59c263a");
    check(inherited_c.inherited&&inherited_c.source_artboard=="B"&&inherited_c.position==70&&
        !inherited_c.position_overridden,
        "Reset-then-full-detach batch remaps C by stable generated source Guide ID without orphaning overrides");
    const auto after_full=encode(session.document());
    session.undo(session.revision());check(encode(session.document())==before_full,
        "One Undo restores exact Template relation, Guide override maps and source identities");
    session.redo(session.revision());check(encode(session.document())==after_full,
        "One Redo restores exact deterministic full-detach Guide identities");
    apply(session,{StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GX","Renamed X","x",90,true}}}},
        StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GY","Renamed Y","y",110,true}}}}});
    const auto& stable_b=board(session.document(),"B").local_guides;
    check(std::any_of(stable_b.begin(),stable_b.end(),[&](const auto& guide) {
        return guide.id=="full-b-guide-09022e07b59c263a"&&guide.position==70;
    })&&std::any_of(stable_b.begin(),stable_b.end(),[&](const auto& guide) {
        return guide.id=="full-b-guide-09022f07b59c27ed"&&guide.position==80;
    }),"Source edits after full detach cannot affect B's frozen local Guides");
    near(occurrence(session.document(),"A","GY").position,110,"A's unaffected inherited Y keeps following source changes");

    auto collision=guide_document();
    collision.compositions.front().guides.push_back(Guide{stable_guide_id("collision","GX"),"Collision","x",3});
    Session collision_session(std::move(collision));
    apply(collision_session,{StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"T","T","S",{}}}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","T",{}}}}});
    failed_atomically(collision_session,"DUPLICATE_ID",{StructuralCommand{ArtboardTemplateCommand{
        DetachArtboardTemplate{"comp","A","collision"}}}});
}
void command_contract_and_failure_atomicity() {
    auto document=guide_document();
    const auto other=empty_document("other-document","other-comp","other-board");
    document.compositions.push_back(other.compositions.front());
    Session session(std::move(document));
    apply(session,{StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"T","T","S",{}}}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","T",{}}}}});
    const auto local=encode(session.document());const auto local_revision=session.revision();const auto local_history=session.history();
    const auto refused=raw_request(session,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(local_revision)+
        ",\"commands\":[{\"type\":\"update_artboard\",\"composition\":\"comp\",\"artboard\":{\"id\":\"A\",\"name\":\"A\",\"x\":1000,\"y\":100,\"width\":400,\"height\":300,\"local_guides\":[]}}]}");
    check(refused.find("\"ok\":true")==std::string::npos&&encode(session.document())==local&&
        session.revision()==local_revision&&session.history()==local_history,
        "Generic UpdateArtboard explicitly refuses even an empty local_guides edit atomically");
    const auto add_refused=raw_request(session,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(local_revision)+
        ",\"commands\":[{\"type\":\"add_artboard\",\"composition\":\"comp\",\"index\":3,\"artboard\":{\"id\":\"smuggled\",\"name\":\"Smuggled\",\"x\":0,\"y\":0,\"width\":20,\"height\":20,\"local_guides\":[]}}]}");
    check(add_refused.find("\"ok\":true")==std::string::npos&&encode(session.document())==local&&
        session.revision()==local_revision&&session.history()==local_history,
        "Generic AddArtboard explicitly refuses local_guides payloads atomically");
    failed_atomically(session,"MISSING_COMPOSITION",{StructuralCommand{ArtboardGuideCommand{
        SetArtboardGuideOverride{"other","A","GX","position",5.0}}}});
    failed_atomically(session,"WRONG_COMPOSITION",{StructuralCommand{ArtboardGuideCommand{
        DeleteArtboardGuide{"other-comp","S","GX"}}}});
    failed_atomically(session,"MISSING_INHERITED_ARTBOARD_GUIDE",{StructuralCommand{ArtboardGuideCommand{
        SetArtboardGuideOverride{"comp","A","missing","position",5.0}}}});
    failed_atomically(session,"TYPE_MISMATCH",{StructuralCommand{ArtboardGuideCommand{
        SetArtboardGuideOverride{"comp","A","GX","enabled",5.0}}}});
    failed_atomically(session,"INVALID_ARTBOARD_GUIDE_OVERRIDE",{StructuralCommand{ArtboardGuideCommand{
        SetArtboardGuideOverride{"comp","A","GX","position",1e10}}}});
    failed_atomically(session,"UNSUPPORTED_ARTBOARD_GUIDE_OVERRIDE",{StructuralCommand{ArtboardGuideCommand{
        SetArtboardGuideOverride{"comp","A","GX","name",5.0}}}});
    failed_atomically(session,"INVALID_ARTBOARD_GUIDE",{StructuralCommand{ArtboardGuideCommand{
        AddArtboardGuide{"comp","A",{"bad-axis","Bad axis","z",5,true}}}}});
    failed_atomically(session,"INVALID_ARTBOARD_GUIDE",{StructuralCommand{ArtboardGuideCommand{
        AddArtboardGuide{"comp","A",{"out-of-range","Too far","x",1e10,true}}}}});
    failed_atomically(session,"DUPLICATE_ID",{StructuralCommand{ArtboardGuideCommand{
        AddArtboardGuide{"comp","A",{"GG","Global collision","x",5,true}}}}});
    failed_atomically(session,"MISSING_INHERITED_ARTBOARD_GUIDE",{
        StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",95.0}}},
        StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","absent","position",5.0}}}});
    failed_atomically(session,"UNSUPPORTED_ARTBOARD_GUIDE_SOURCE",{Set{{"A","GX","artboard.guide.position"},12.0}});
    failed_atomically(session,"UNSUPPORTED_ARTBOARD_GUIDE_SOURCE",{Link{{"A","GX","artboard.guide.position"},
        Binding{{"S","GX","artboard.guide.position"},1,0,"copy_local_value"}}});
    failed_atomically(session,"UNSUPPORTED_ARTBOARD_GUIDE_SOURCE",{SetExpression{{Ref{"A","GX","artboard.guide.position"}},
        Expression{"1 + 2",1},false}});
    const auto captured=session.revision();
    apply(session,{StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",91.0}}}});
    const auto after=encode(session.document());const auto revision=session.revision();const auto history=session.history();
    const auto stale=raw_request(session,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(captured)+
        ",\"commands\":[{\"type\":\"reset_artboard_guide_override\",\"composition\":\"comp\",\"artboard\":\"A\",\"guide_id\":\"GX\",\"field\":\"position\"}]}");
    check(stale.find("\"REVISION_CONFLICT\"")!=std::string::npos&&encode(session.document())==after&&
        session.revision()==revision&&session.history()==history,"Stale captured Guide commands fail atomically");
    const auto list=checked_request(session,R"({"op":"properties"})");
    check(list.find("artboard.guide.position")!=std::string::npos&&list.find("\"source_artboard\":\"S\"")!=std::string::npos,
        "JSON property list reports scoped Artboard Guide source and override metadata");
    const auto set=request(session,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(revision)+
        ",\"commands\":[{\"type\":\"reset_artboard_guide_override\",\"composition\":\"comp\",\"artboard\":\"A\",\"guide_id\":\"GX\",\"field\":\"position\"}]}");
    check(set.find("\"changed\":true")!=std::string::npos&&
        std::get<double>(artboard_guide_property(session.document(),{"A","GX","artboard.guide.position"}).evaluated)==40,
        "JSON command reset reaches the same atomic Session state as core commands");
    apply(session,{StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",91.0}}}});
    failed_atomically(session,"ARTBOARD_GUIDE_SOURCE_IN_USE",{StructuralCommand{ArtboardGuideCommand{
        DeleteArtboardGuide{"comp","S","GX"}}}});
    apply(session,{StructuralCommand{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","A","GX","position"}}}});
    apply(session,{StructuralCommand{ArtboardGuideCommand{UpdateArtboardGuide{"comp","S",{"GX","Grid X","x",50,true}}}}});
    failed_atomically(session,"DUPLICATE_ID",{StructuralCommand{ArtboardGuideCommand{
        DetachArtboardGuide{"comp","A","GX","GG"}}}});
    const auto before_detach=encode(session.document());const auto before_detach_revision=session.revision();
    const auto detached=raw_request(session,"{\"op\":\"apply\",\"expected_revision\":"+
        std::to_string(before_detach_revision)+",\"commands\":[{\"type\":\"detach_artboard_guide\",\"composition\":\"comp\",\"artboard\":\"A\",\"guide_id\":\"GX\",\"new_guide_id\":\"AD\"}]}");
    check(detached.find("\"ok\":true")!=std::string::npos&&
        detached.find("\"source_guide_id\":\"GX\"")!=std::string::npos&&
        detached.find("\"id\":\"AD\"")!=std::string::npos&&
        detached.find("\"source_suppressed\":true")!=std::string::npos&&
        board(session.document(),"A").local_guides.back()==ArtboardGuide{"AD","Grid X","x",50,true}&&
        board(session.document(),"A").template_assignment->guide_position_overrides.empty(),
        ("Item detach returns the old scoped occurrence/new authored ID map and reads back its frozen state and suppression: "+
            detached+"; local-count="+std::to_string(board(session.document(),"A").local_guides.size())+
            (board(session.document(),"A").local_guides.empty()?std::string{}:
                "; last="+board(session.document(),"A").local_guides.back().id+"/"+
                board(session.document(),"A").local_guides.back().name+"/"+
                std::to_string(board(session.document(),"A").local_guides.back().position))+ "; source="+
            std::to_string(board(session.document(),"S").local_guides.front().position)).c_str());
    const auto old_property=raw_request(session,R"({"op":"get","ref":{"object":"A","point":"GX","field":"artboard.guide.position"}})");
    check(old_property.find("MISSING_ARTBOARD_GUIDE_SOURCE")!=std::string::npos,
        "Detached old occurrence is no longer exposed as a live Template Guide Ref");
    const auto new_property=checked_request(session,R"({"op":"get","ref":{"object":"A","point":"AD","field":"artboard.guide.position"}})");
    const auto new_result=boost::json::parse(new_property).as_object().at("result").as_object();
    check(new_result.at("evaluated").to_number<double>()==50&&
        !new_result.at("inherited").as_bool(),
        "New detached local ID reads back as an independent authored Guide");
    failed_atomically(session,"MISSING_INHERITED_ARTBOARD_GUIDE",{StructuralCommand{ArtboardGuideCommand{
        ResetArtboardGuideOverride{"comp","A","GX","position"}}}});
    apply(session,{StructuralCommand{ArtboardGuideCommand{DeleteArtboardGuide{"comp","S","GX"}}}});
    check(encode(session.document())!=before_detach&&
        std::any_of(board(session.document(),"A").local_guides.begin(),board(session.document(),"A").local_guides.end(),
            [](const ArtboardGuide& guide){return guide.id=="AD"&&guide.position==50;})&&
        std::find(board(session.document(),"A").template_assignment->detached_guides.begin(),
            board(session.document(),"A").template_assignment->detached_guides.end(),"GX")!=
                board(session.document(),"A").template_assignment->detached_guides.end(),
        "Source deletion without a live override leaves detached local identity and inert suppression intact");
}
void source_delete_reset_batch_is_atomic_and_undoable() {
    Session session(guide_document());
    apply(session,{StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"T","T","S",{}}}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","T",{}}}},
        StructuralCommand{ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",90.0}}}});
    failed_atomically(session,"ARTBOARD_GUIDE_SOURCE_IN_USE",{StructuralCommand{ArtboardGuideCommand{
        DeleteArtboardGuide{"comp","S","GX"}}}});
    const auto before=encode(session.document());const auto revision=session.revision();
    apply(session,{StructuralCommand{ArtboardGuideCommand{ResetArtboardGuideOverride{"comp","A","GX","position"}}},
        StructuralCommand{ArtboardGuideCommand{DeleteArtboardGuide{"comp","S","GX"}}}});
    const auto after=encode(session.document());
    const auto remaining=effective_artboard_guides(session.document(),"comp","A");
    check(session.revision()==revision+1&&encode(session.document())!=before&&
        std::none_of(remaining.begin(),remaining.end(),[](const auto& item){return item.guide_id=="GX";}),
        "One Reset-then-source-delete batch removes the conflict and commits once");
    session.undo(session.revision());check(encode(session.document())==before,
        "Undo restores the exact source Guide and inherited Template relation");
    session.redo(session.revision());check(encode(session.document())==after,
        "Redo restores the exact Reset-then-delete result");
}
void generated_paint_reserves_local_guide_ids() {
    auto document=empty_document("paint-guide-doc","paint-guide-comp","paint-guide-board");
    Session session(std::move(document));
    apply(session,{
        StructuralCommand{ArtboardGuideCommand{AddArtboardGuide{"paint-guide-comp","paint-guide-board",
            {"shape-stroke","Authored Guide collision","x",25,true}}}},
        CreatePrimitive{"paint-guide-comp","","shape","Shape",
            default_primitive("shape-source","nect.shape.rectangle")}
    });
    const auto& stroke=session.document().objects.at("shape").stack.front();
    check(stroke.id=="shape-stroke-1"&&
        board(session.document(),"paint-guide-board").local_guides.front().id=="shape-stroke",
        "Default paint allocation reserves document-unique Artboard Guide identities");
}
}
int main() {
    try {
        fixture_and_native_roundtrip();
        source_frame_move_reprojects_guides_without_moving_artwork();
        property_list_matches_scoped_get_for_many_occurrences();
        nested_sources_detach_and_history();
        command_contract_and_failure_atomicity();
        source_delete_reset_batch_is_atomic_and_undoable();
        generated_paint_reserves_local_guide_ids();
        std::cout<<"Artboard Guide contract checks passed: "<<checks<<"\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Artboard Guide contract failed after "<<checks<<" checks: "<<error.what()<<"\n";
        return 1;
    }
}
