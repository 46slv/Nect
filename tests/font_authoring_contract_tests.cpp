#include "nect/io.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace nect;
namespace {
int checks=0;
void check(bool value,const char* why) {
    if(!value)throw std::runtime_error(why);
    ++checks;
}
template<class F>void rejects(const char* code,F action) {
    try { action(); }
    catch(const Error& error) {
        check(error.code==code,("Expected "+std::string(code)+", got "+error.code).c_str());
        return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
template<class F>void rejects_any(F action,const char* why) {
    try { action(); }
    catch(const Error&) { ++checks; return; }
    throw std::runtime_error(why);
}
TextSource make_text(std::string id,std::string content) {
    auto source=default_text(std::move(id),std::move(content));
    source.family="Pre-authored fixture family";
    source.locale="en-US";
    source.weight=600;
    return source;
}
Document authored_text_fixture() {
    auto document=empty_document("font-doc","font-comp","font-art");
    auto primary=make_text("font-source","Portable authored text");
    auto peer=make_text("peer-source","Dependency source");
    peer.weight=500;
    peer.italic=true;
    primary.italic=false;
    primary.weight_driver=TextWeightDriver{{"font-peer","","text.weight"},0};
    primary.italic_driver=TextItalicDriver{Ref{"font-peer","","text.italic"}};
    Object source;source.id="font-text";source.name="Typography fixture";source.kind=Kind::text;source.text=primary;
    Object peer_object;peer_object.id="font-peer";peer_object.name="Typography dependency";peer_object.kind=Kind::text;peer_object.text=peer;
    Object group;group.id="font-group";group.name="Non-Text target";group.kind=Kind::group;
    document.objects.emplace(source.id,source);
    document.objects.emplace(peer_object.id,peer_object);
    document.objects.emplace(group.id,group);
    document.compositions.front().roots={source.id,peer_object.id,group.id};
    validate(document);
    return document;
}
Document font_history_fixture(std::size_t collection_size) {
    auto document=authored_text_fixture();
    auto& source=*document.objects.at("font-text").text;
    for(std::size_t i=0;i<collection_size;++i) {
        const auto suffix=std::to_string(i);
        const auto feature_tag=std::string("F")+std::string(3-suffix.size(),'0')+suffix;
        const auto axis_tag=std::string("A")+std::string(3-suffix.size(),'0')+suffix;
        source.font_features.push_back({feature_tag,static_cast<std::uint32_t>(i),"whole_text"});
        source.additional_axis_values.emplace(axis_tag,static_cast<double>(i)+0.25);
    }
    validate(document);
    return document;
}
void apply(Session& session,std::vector<Command> commands) { session.apply(commands,session.revision()); }
std::string apply_json(Session& session,const std::string& commands) {
    return request(session,"{\"op\":\"apply\",\"expected_revision\":"+
        std::to_string(session.revision())+",\"commands\":["+commands+"]}");
}
void replace_once(std::string& source,const std::string& before,const std::string& after) {
    const auto at=source.find(before);
    if(at==std::string::npos)throw std::runtime_error("Fixture replacement anchor missing: "+before);
    source.replace(at,before.size(),after);
}
}
int main() {
    try {
        const auto fixture=authored_text_fixture();
        const auto empty_native=encode(fixture);
        check(empty_native.find("\"version\":\"0.80\"")!=std::string::npos,
            "Current writer is native 0.80");
        check(empty_native.find("\"font_features\"")==std::string::npos&&
            empty_native.find("\"additional_axis_values\"")==std::string::npos,
            "Empty optional Text authoring collections are omitted");
        Session session(decode(empty_native));
        const auto original_source=*session.document().objects.at("font-text").text;

        const auto api=apply_json(session,
            "{\"type\":\"add_text_font_feature\",\"object\":\"font-text\",\"feature\":{\"feature_tag\":\"lig \",\"parameter\":0,\"scope\":\"whole_text\"}},"
            "{\"type\":\"add_text_font_feature\",\"object\":\"font-text\",\"feature\":{\"feature_tag\":\"KERN\",\"parameter\":1,\"scope\":\"whole_text\"}},"
            "{\"type\":\"update_text_font_feature\",\"object\":\"font-text\",\"feature_tag\":\"lig \",\"parameter\":4294967295},"
            "{\"type\":\"set_text_additional_axis\",\"object\":\"font-text\",\"axis_tag\":\"wdth\",\"value\":12.25},"
            "{\"type\":\"set_text_additional_axis\",\"object\":\"font-text\",\"axis_tag\":\"wdth\",\"value\":87.1234567890123},"
            "{\"type\":\"set_text_additional_axis\",\"object\":\"font-text\",\"axis_tag\":\"WIDE\",\"value\":12.25}");
        check(api.find("\"ok\":true")!=std::string::npos&&session.revision()==1,
            "JSON-lines applies all five typed feature/axis command forms through one revision");
        const auto& authored=*session.document().objects.at("font-text").text;
        check(authored.version==1&&authored.font_features.size()==2&&
            authored.font_features[0]==TextFontFeature{"lig ",0xffffffffU,"whole_text"}&&
            authored.font_features[1]==TextFontFeature{"KERN",1,"whole_text"},
            "Feature tags preserve exact case, padding, order, whole_text scope, and full uint32 range");
        check(authored.additional_axis_values.size()==2&&
            authored.additional_axis_values.at("wdth")==87.1234567890123&&
            authored.additional_axis_values.at("WIDE")==12.25,
            "Additional axes retain lexical tags and precise finite double values");
        check(authored.id==original_source.id&&authored.content==original_source.content&&
            authored.family==original_source.family&&authored.locale==original_source.locale&&
            authored.weight==original_source.weight&&authored.italic==original_source.italic&&
            authored.weight_driver==original_source.weight_driver&&authored.italic_driver==original_source.italic_driver&&
            authored.parameters==original_source.parameters,
            "Typed font edits preserve Text source identity, all prior literals, Scalars, and existing drivers");

        const auto inspect=request(session,"{\"op\":\"inspect\"}");
        check(inspect.find("\"feature_tag\":\"lig \",\"parameter\":4294967295,\"scope\":\"whole_text\"")!=std::string::npos&&
            inspect.find("\"feature_tag\":\"KERN\",\"parameter\":1,\"scope\":\"whole_text\"")!=std::string::npos&&
            inspect.find("\"additional_axis_values\":{\"WIDE\":1.225E1,\"wdth\":8.71234567890123E1}")!=std::string::npos,
            "JSON-lines inspect reads authored features and deterministic lexical axis serialization");
        const auto saved=encode(session.document());
        check(decode(saved)==session.document()&&encode(decode(saved))==saved,
            "Native cold reopen preserves full authored font intent exactly");
        Session text_update_session(decode(saved));
        auto ordinary_update=*text_update_session.document().objects.at("font-text").text;
        ordinary_update.content="Edited without changing authored typography";
        apply(text_update_session,{UpdateText{"font-text",ordinary_update}});
        check(text_update_session.document().objects.at("font-text").text->content==ordinary_update.content&&
            text_update_session.document().objects.at("font-text").text->font_features==authored.font_features&&
            text_update_session.document().objects.at("font-text").text->additional_axis_values==authored.additional_axis_values,
            "UpdateText preserves matching feature/axis collections while editing ordinary Text metadata");
        const auto history_before_undo=session.history();
        check(history_before_undo.states.size()==2&&history_before_undo.states.back().estimated_bytes>0&&
            history_before_undo.retained_bytes==history_before_undo.states.back().estimated_bytes,
            "History memory estimate includes the newly retained Text collections");
        Session sparse_history(font_history_fixture(1));
        apply(sparse_history,{UpdateTextFontFeature{"font-text","F000",2}});
        const auto sparse_bytes=sparse_history.history().retained_bytes;
        const auto dense_history_document=font_history_fixture(128);
        Session dense_history(dense_history_document);
        apply(dense_history,{UpdateTextFontFeature{"font-text","F000",2}});
        const auto dense_bytes=dense_history.history().retained_bytes;
        check(dense_bytes>sparse_bytes,
            "Same-command history estimate grows with dense feature-vector and axis-map entries");
        Session calibrated_budget(dense_history_document,HistoryLimits{8,sparse_bytes});
        const auto calibrated_bytes=encode(calibrated_budget.document());
        const auto calibrated_history=calibrated_budget.history();
        rejects("HISTORY_LIMIT",[&]{apply(calibrated_budget,{UpdateTextFontFeature{"font-text","F000",2}});});
        check(calibrated_budget.revision()==0&&encode(calibrated_budget.document())==calibrated_bytes&&
            calibrated_budget.history()==calibrated_history,
            "Calibrated history budget rejects dense font metadata when its own container allocation is counted");
        session.undo(session.revision());
        check(*session.document().objects.at("font-text").text==original_source&&session.can_redo(),
            "Undo restores the pre-authored Text source and feature/axis state together");
        session.redo(session.revision());
        check(encode(session.document())==saved,"Redo restores the exact authored font-feature and axis state");

        const auto remove_reply=apply_json(session,
            "{\"type\":\"remove_text_font_feature\",\"object\":\"font-text\",\"feature_tag\":\"lig \"},"
            "{\"type\":\"remove_text_additional_axis\",\"object\":\"font-text\",\"axis_tag\":\"WIDE\"}");
        check(remove_reply.find("\"ok\":true")!=std::string::npos&&
            session.document().objects.at("font-text").text->font_features==
                std::vector<TextFontFeature>{{"KERN",1,"whole_text"}}&&
            session.document().objects.at("font-text").text->additional_axis_values.size()==1&&
            session.document().objects.at("font-text").text->additional_axis_values.contains("wdth"),
            "Remove commands delete only the exact case-sensitive tag and preserve remaining order/state");

        const auto before_invalid=encode(session.document());
        const auto before_revision=session.revision();
        const auto before_history=session.history();
        rejects("DUPLICATE_TEXT_FONT_FEATURE",[&]{apply(session,{AddTextFontFeature{"font-text",{"KERN",99,"whole_text"}}});});
        check(session.revision()==before_revision&&encode(session.document())==before_invalid&&session.history()==before_history,
            "Duplicate feature add leaves bytes, revision, and history unchanged");
        rejects("MISSING_TEXT_FONT_FEATURE",[&]{apply(session,{UpdateTextFontFeature{"font-text","nope",2}});});
        rejects("MISSING_TEXT_FONT_FEATURE",[&]{apply(session,{RemoveTextFontFeature{"font-text","nope"}});});
        rejects("MISSING_TEXT_AXIS",[&]{apply(session,{RemoveTextAdditionalAxis{"font-text","nope"}});});
        rejects("INVALID_TEXT_FONT_TAG",[&]{apply(session,{AddTextFontFeature{"font-text",{"ab\xc3\xa9",1,"whole_text"}}});});
        rejects("TEXT_AXIS_CONFLICT",[&]{apply(session,{SetTextAdditionalAxis{"font-text","wght",10.0}});});
        rejects("TEXT_AXIS_CONFLICT",[&]{apply(session,{SetTextAdditionalAxis{"font-text","ital",1.0}});});
        rejects("NON_FINITE",[&]{apply(session,{SetTextAdditionalAxis{"font-text","wdth",std::numeric_limits<double>::infinity()}});});
        rejects("INVALID_TEXT_FONT_SCOPE",[&]{apply(session,{AddTextFontFeature{"font-text",{"liga",1,"range"}}});});
        rejects("INVALID_TEXT",[&]{apply(session,{AddTextFontFeature{"font-group",{"liga",1,"whole_text"}}});});
        check(session.revision()==before_revision&&encode(session.document())==before_invalid&&session.history()==before_history,
            "Missing, malformed, conflicting, nonfinite, wrong-kind, and invalid-scope commands are atomic");

        for(const auto& parameter:{std::string("-1"),std::string("1.5"),std::string("4294967296")}) {
            const auto reply=apply_json(session,"{\"type\":\"add_text_font_feature\",\"object\":\"font-text\",\"feature\":{\"feature_tag\":\"TEMP\",\"parameter\":"+
                parameter+",\"scope\":\"whole_text\"}}");
            check(reply.find("\"code\":\"INVALID_TEXT_FONT_FEATURE\"")!=std::string::npos,
                "JSON-lines rejects negative, fractional, and out-of-range uint32 feature parameters");
        }
        check(session.revision()==before_revision&&encode(session.document())==before_invalid&&session.history()==before_history,
            "Malformed JSON numeric parameters leave native bytes, revision, and history unchanged");

        const auto stale_revision=session.revision()-1;
        rejects("REVISION_CONFLICT",[&]{session.apply({AddTextFontFeature{"font-text",{"liga",1,"whole_text"}},
            SetTextAdditionalAxis{"font-text","wdth",90.0}},stale_revision);});
        check(session.revision()==before_revision&&encode(session.document())==before_invalid&&session.history()==before_history,
            "Stale revision batch leaves native bytes, revision, and history unchanged");
        rejects("MISSING_TEXT_AXIS",[&]{apply(session,{AddTextFontFeature{"font-text",{"TEMP",3,"whole_text"}},
            RemoveTextAdditionalAxis{"font-text","MISS"}});});
        check(session.revision()==before_revision&&encode(session.document())==before_invalid&&session.history()==before_history,
            "A later command failure rolls back earlier authored additions in the same batch");
        rejects("USE_TYPED_COMMAND",[&]{
            auto updated=*session.document().objects.at("font-text").text;
            updated.font_features.push_back({"TEMP",1,"whole_text"});
            apply(session,{UpdateText{"font-text",updated}});
        });
        check(session.revision()==before_revision&&encode(session.document())==before_invalid&&session.history()==before_history,
            "UpdateText cannot add or silently clear new typed collections");

        auto duplicate_feature_document=fixture;
        duplicate_feature_document.objects.at("font-text").text->font_features={{"liga",1,"whole_text"},{"liga",2,"whole_text"}};
        rejects("DUPLICATE_TEXT_FONT_FEATURE",[&]{validate(duplicate_feature_document);});
        auto invalid_axis_document=fixture;
        invalid_axis_document.objects.at("font-text").text->additional_axis_values={{"wght",500.0}};
        rejects("TEXT_AXIS_CONFLICT",[&]{validate(invalid_axis_document);});

        auto native078=saved;
        replace_once(native078,"\"version\":\"0.80\"","\"version\":\"0.78\"");
        check(decode(native078)==decode(saved)&&encode(decode(native078))==saved,
            "Native0.78 nonempty features/axes preserve exact intent through current0.80 roundtrip");
        auto legacy=empty_native;
        replace_once(legacy,"\"version\":\"0.80\"","\"version\":\"0.77\"");
        const auto migrated=decode(legacy);
        check(migrated.objects.at("font-text").text->font_features.empty()&&
            migrated.objects.at("font-text").text->additional_axis_values.empty(),
            "Native 0.77 Text cold migration supplies empty optional collections");
        auto lie= saved;
        replace_once(lie,"\"version\":\"0.80\"","\"version\":\"0.77\"");
        rejects("NATIVE_VERSION_MISMATCH",[&]{decode(lie);});
        auto empty_lie=empty_native;
        const auto text_at=empty_lie.find("\"text\":{");
        if(text_at==std::string::npos)throw std::runtime_error("Text fixture source is missing");
        const auto parameters_at=empty_lie.find("\"parameters\":",text_at);
        if(parameters_at==std::string::npos)throw std::runtime_error("Text fixture parameter map is missing");
        empty_lie.insert(parameters_at,"\"font_features\":[],\"additional_axis_values\":{},");
        replace_once(empty_lie,"\"version\":\"0.80\"","\"version\":\"0.77\"");
        rejects("NATIVE_VERSION_MISMATCH",[&]{decode(empty_lie);});

        auto duplicate_axis_key=saved;
        const auto axes_at=duplicate_axis_key.find("\"additional_axis_values\":{");
        if(axes_at==std::string::npos)throw std::runtime_error("Serialized axis map is missing");
        const auto object_end=duplicate_axis_key.find('}',axes_at);
        duplicate_axis_key.replace(axes_at,object_end-axes_at+1,"\"additional_axis_values\":{\"wdth\":1,\"wdth\":2}");
        rejects("DUPLICATE_KEY",[&]{decode(duplicate_axis_key);});

        auto duplicate_native_feature=saved;
        const auto feature_at=duplicate_native_feature.find("\"font_features\":[");
        if(feature_at==std::string::npos)throw std::runtime_error("Serialized feature vector is missing");
        const auto feature_array=duplicate_native_feature.find('[',feature_at);
        const auto feature_end=duplicate_native_feature.find(']',feature_array);
        duplicate_native_feature.replace(feature_array,feature_end-feature_array+1,
            "[{\"feature_tag\":\"lig \",\"parameter\":1,\"scope\":\"whole_text\"},{\"feature_tag\":\"lig \",\"parameter\":2,\"scope\":\"whole_text\"}]");
        rejects("DUPLICATE_TEXT_FONT_FEATURE",[&]{decode(duplicate_native_feature);});
        auto invalid_native_scope=saved;
        replace_once(invalid_native_scope,"\"scope\":\"whole_text\"","\"scope\":\"range\"");
        rejects("INVALID_TEXT_FONT_SCOPE",[&]{decode(invalid_native_scope);});
        auto invalid_native_parameter=saved;
        replace_once(invalid_native_parameter,"\"parameter\":4294967295","\"parameter\":4294967296");
        rejects("INVALID_TEXT_FONT_FEATURE",[&]{decode(invalid_native_parameter);});
        auto nonfinite_native_axis=saved;
        replace_once(nonfinite_native_axis,"\"wdth\":8.71234567890123E1","\"wdth\":1e9999");
        rejects_any([&]{decode(nonfinite_native_axis);},"Non-finite/overflow native axis JSON must reject");

        const auto budget_limits=HistoryLimits{8,1};
        Session budget_session(decode(empty_native),budget_limits);
        const auto budget_bytes=encode(budget_session.document());
        const auto budget_history=budget_session.history();
        rejects("HISTORY_LIMIT",[&]{apply(budget_session,{AddTextFontFeature{"font-text",{"liga",1,"whole_text"}}});});
        check(budget_session.revision()==0&&encode(budget_session.document())==budget_bytes&&
            budget_session.history()==budget_history,
            "History-budget admission failure leaves source bytes, revision, and timeline intact");

#ifndef _WIN32
        Session create_session(empty_document("create-doc","create-comp","create-art"));
        const auto create_bytes=encode(create_session.document());
        const auto create_history=create_session.history();
        auto initial=make_text("initial-source","Initial authoring");
        initial.font_features.push_back({"liga",1,"whole_text"});
        rejects("TEXT_PLATFORM_UNSUPPORTED",[&]{apply(create_session,{CreateText{"create-comp","","initial-text","Initial",initial}});});
        check(create_session.revision()==0&&encode(create_session.document())==create_bytes&&
            create_session.history()==create_history,
            "Portable authoring did not stub ordinary CreateText anchor projection on non-Windows");
#endif
        std::cout<<"Font authoring portable contract checks: "<<checks<<'\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Font authoring portable contract failed after "<<checks<<" checks: "<<error.what()<<'\n';
        return 1;
    }
}
