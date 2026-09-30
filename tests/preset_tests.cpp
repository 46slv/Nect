#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <string>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action) {
    try {action();}catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", received "+error.code+": "+error.what());return;
    }
    throw std::runtime_error("Expected "+std::string(code));
}
Contour rectangle() {
    Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> points{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t i=0;i<points.size();++i) {
        Point point;point.id="point-"+std::to_string(i);point.x.literal=points[i].x;point.y.literal=points[i].y;
        contour.points.push_back(std::move(point));
    }
    return contour;
}
Document fixture(bool closed=true) {
    auto document=empty_document("preset-doc","preset-comp","preset-art");
    Object object;object.id="path";object.name="Preset target";object.kind=Kind::path;
    auto contour=rectangle();contour.closed=closed;object.contours.push_back(std::move(contour));
    object.stack.push_back(default_operation("fill","nect.paint.fill"));
    document.objects.emplace(object.id,std::move(object));document.compositions.front().roots.push_back("path");
    return document;
}
PresetEntry entry_from(const ShapeOperation& operation) {
    PresetEntry entry;entry.type=operation.type;entry.version=operation.version;entry.enabled=operation.enabled;
    for(const auto& [name,value]:operation.parameters)entry.parameters.emplace(name,value.literal);
    entry.composite=operation.composite;entry.fill_rule=operation.fill_rule;
    entry.line_join=operation.line_join;entry.line_cap=operation.line_cap;return entry;
}
PresetDefinition sample_preset(Id id="preset",std::string label="Offset Repeater") {
    PresetDefinition definition;definition.id=std::move(id);definition.label=std::move(label);
    definition.category="Shape";definition.tags={"contour","repeat"};
    auto offset=default_operation("offset-template","nect.shape.offset");
    offset.parameters.at("amount").literal=18;
    auto repeater=default_operation("repeater-template","nect.shape.repeater");
    repeater.parameters.at("copies").literal=4;repeater.parameters.at("position_x").literal=36;
    definition.entries={entry_from(offset),entry_from(repeater)};return definition;
}
MacroDefinition macro_definition() {
    auto offset=default_operation("macro-offset-node","nect.shape.offset");
    offset.parameters.at("amount").literal=12;
    auto repeater=default_operation("macro-repeater-node","nect.shape.repeater");
    repeater.parameters.at("copies").literal=2;
    repeater.parameters.at("position_x").literal=125;
    MacroDefinitionRevision revision;revision.revision=1;
    revision.input={"macro-input","local_paths_and_paint"};
    revision.output={"macro-output","local_paths_and_paint"};
    revision.nodes={{offset,"offset-input","offset-output"},{repeater,"repeater-input","repeater-output"}};
    revision.edges={{{"macro-repeater-node","repeater-output"},{"","macro-output"}},
        {{"","macro-input"},{"macro-offset-node","offset-input"}},
        {{"macro-offset-node","offset-output"},{"macro-repeater-node","repeater-input"}}};
    revision.output_mapping={"macro-repeater-node","repeater-output"};
    revision.public_parameters.push_back({"macro.offset.amount","Amount","macro-offset-node","amount",
        "number","du","local_paths_and_paint"});
    MacroDefinition definition;definition.id="preset-macro";definition.label="Offset Repeat";
    definition.latest_revision=1;definition.revisions.emplace(1,std::move(revision));
    return definition;
}
void apply_command(Session& session,const Command& command) { session.apply({command},session.revision()); }
void expect_atomic(Session& session,const char* code,const PresetCommand& command,std::uint64_t expected) {
    const auto document=session.document();const auto native=encode(document);const auto revision=session.revision();
    const auto history=session.history();
    rejects(code,[&]{session.apply_preset_command(command,expected);});
    check(session.document()==document&&encode(session.document())==native&&session.revision()==revision&&
        session.history()==history,"Preset rejection preserves Document, native bytes, revision and history");
}
Document macro_preset_fixture() {
    auto document=fixture();
    auto& source=document.objects.at("path");source.stack.clear();
    Object target=source;target.id="target";target.name="Preset target";
    for(auto& contour:target.contours) {
        contour.id="target-contour";
        for(std::size_t i=0;i<contour.points.size();++i)contour.points[i].id="target-point-"+std::to_string(i);
    }
    document.objects.emplace(target.id,std::move(target));
    document.compositions.front().roots.push_back("target");
    return document;
}
void macro_capture_apply_and_pinned_revision_contract() {
    Session session(macro_preset_fixture());
    const auto definition=macro_definition();
    apply_command(session,Command{MacroCommand{CreateMacroDefinition{definition}}});
    auto offset=default_operation("source-offset","nect.shape.offset");
    offset.parameters.at("amount").literal=6;
    apply_command(session,Command{AddOperation{"path",offset,0}});
    apply_command(session,Command{MacroCommand{InstantiateMacro{"path",definition.id,"source-macro",1,1}}});
    apply_command(session,Command{MacroCommand{SetMacroOverride{"path","source-macro","macro.offset.amount",27}}});
    auto stroke=default_operation("source-stroke","nect.paint.stroke");
    apply_command(session,Command{AddOperation{"path",stroke,2}});

    PresetDefinition metadata;metadata.id="captured-v2";metadata.label="Offset, Macro, Stroke";
    metadata.schema_version=2;metadata.category="Shape";
    const auto capture_revision=session.revision();
    session.apply_preset_command(PresetCommand{CreatePresetFromStack{metadata,"path"}},capture_revision);
    const auto captured=session.document().preset_definitions.at(metadata.id);
    check(captured.schema_version==2&&captured.entries.size()==3&&captured.entries[0].kind=="builtin"&&
        captured.entries[0].type=="nect.shape.offset"&&captured.entries[1].kind=="macro"&&
        captured.entries[1].macro_definition==definition.id&&captured.entries[1].pinned_revision==1&&
        captured.entries[1].overrides.at("macro.offset.amount")==27&&captured.entries[2].kind=="builtin"&&
        captured.entries[2].type=="nect.paint.stroke",
        "Schema v2 captures Offset, stable Macro ID/revision/PublicParam override, and Stroke in order");
    auto malformed_builtin=captured;malformed_builtin.entries[0].macro_definition=definition.id;
    expect_atomic(session,"INVALID_PRESET_ENTRY",PresetCommand{UpdatePreset{malformed_builtin}},session.revision());
    auto malformed_macro=captured;malformed_macro.entries[1].parameters["amount"]=9;
    expect_atomic(session,"INVALID_PRESET_ENTRY",PresetCommand{UpdatePreset{malformed_macro}},session.revision());
    const auto bytes=encode(session.document());
    check(decode(bytes)==session.document(),"Native 0.67 roundtrip preserves tagged v2 Macro preset entries");
    auto lied=bytes;const auto native_version=lied.find("\"version\":\"0.70\"");
    check(native_version!=std::string::npos,"Native Macro Preset fixture uses the current writer version");
    lied.replace(native_version,std::string("\"version\":\"0.70\"").size(),"\"version\":\"0.65\"");
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(lied);});

    auto compatible=definition.revisions.at(1);compatible.revision=2;
    compatible.nodes.front().operation.id="macro-offset-node-v2";
    compatible.public_parameters.front().node="macro-offset-node-v2";
    for(auto& edge:compatible.edges) {
        if(edge.from.node=="macro-offset-node")edge.from.node="macro-offset-node-v2";
        if(edge.to.node=="macro-offset-node")edge.to.node="macro-offset-node-v2";
    }
    apply_command(session,Command{MacroCommand{UpdateMacroDefinition{definition.id,compatible}}});
    auto updated=captured;updated.entries[1].pinned_revision=2;
    session.apply_preset_command(PresetCommand{UpdatePreset{updated}},session.revision());
    check(session.document().preset_definitions.at(metadata.id).entries[1].overrides.at("macro.offset.amount")==27,
        "Preset revision pin migration keeps the stable PublicParamID literal override");

    const auto before_apply=session.revision();const auto history_before=session.history().states.size();
    session.apply_preset_command(PresetCommand{ApplyPreset{metadata.id,"target","preset-use"}},before_apply);
    const auto& applied=session.document().objects.at("target").stack;
    check(session.revision()==before_apply+1&&session.history().states.size()==history_before+1&&
        applied.size()==3&&applied[0].id=="preset-use-op-1"&&applied[1].id=="preset-use-op-2"&&
        applied[1].macro&&applied[1].macro->definition==definition.id&&applied[1].macro->pinned_revision==2&&
        applied[1].macro->overrides.at("macro.offset.amount")==27&&applied[2].id=="preset-use-op-3",
        "Apply creates fresh ordered IDs and one Session history state with a pinned Macro snapshot");
    session.undo(session.revision());
    check(session.document().objects.at("target").stack.empty()&&
        session.document().preset_definitions.contains(metadata.id),
        "One Undo removes every applied Preset entry while keeping its definition");
    session.redo(session.revision());
    check(session.document().objects.at("target").stack.size()==3&&
        session.document().objects.at("target").stack[1].macro->overrides.at("macro.offset.amount")==27,
        "One Redo restores the complete ordered Macro application");

    updated=session.document().preset_definitions.at(metadata.id);
    updated.entries[1].overrides["macro.offset.amount"]=31;
    session.apply_preset_command(PresetCommand{UpdatePreset{updated}},session.revision());
    check(session.document().objects.at("target").stack[1].macro->overrides.at("macro.offset.amount")==27,
        "Re-editing a captured Macro PublicParam leaves its existing application unchanged");
    const auto later_revision=session.revision();const auto later_history=session.history().states.size();
    session.apply_preset_command(PresetCommand{ApplyPreset{metadata.id,"target","preset-later"}},later_revision);
    const auto& later_stack=session.document().objects.at("target").stack;
    check(session.revision()==later_revision+1&&session.history().states.size()==later_history+1&&
        later_stack.size()==6&&later_stack[1].macro->overrides.at("macro.offset.amount")==27&&
        later_stack[4].macro->definition==definition.id&&later_stack[4].macro->pinned_revision==2&&
        later_stack[4].macro->overrides.at("macro.offset.amount")==31,
        "Later applications use the re-edited PublicParam value in one independent Session revision");
    session.undo(session.revision());
    check(session.document().objects.at("target").stack.size()==3&&
        session.document().objects.at("target").stack[1].macro->overrides.at("macro.offset.amount")==27,
        "One Undo removes the later application without disturbing the earlier Macro snapshot");
    session.redo(session.revision());
    check(session.document().objects.at("target").stack.size()==6&&
        session.document().objects.at("target").stack[4].macro->overrides.at("macro.offset.amount")==31,
        "One Redo restores the later application with its edited PublicParam value");

    auto incompatible=compatible;incompatible.revision=3;incompatible.public_parameters.clear();
    apply_command(session,Command{MacroCommand{UpdateMacroDefinition{definition.id,incompatible}}});
    updated=session.document().preset_definitions.at(metadata.id);updated.entries[1].pinned_revision=3;
    expect_atomic(session,"INCOMPATIBLE_MACRO_PUBLIC_PARAMETER",PresetCommand{UpdatePreset{updated}},session.revision());
    std::rotate(updated.entries.begin(),updated.entries.begin()+1,updated.entries.end());
    expect_atomic(session,"INCOMPATIBLE_MACRO_PUBLIC_PARAMETER",PresetCommand{UpdatePreset{updated}},session.revision());
    auto& source_stack=session.document().objects.at("path").stack;
    check(source_stack[1].macro->pinned_revision==1&&source_stack[1].macro->overrides.at("macro.offset.amount")==27,
        "Preset migration never silently retargets an already-applied/source Macro instance");
}
void unavailable_macro_pins_are_preserved_but_not_applied() {
    PresetDefinition missing_definition;missing_definition.id="missing-definition";
    missing_definition.schema_version=2;missing_definition.label="Unavailable definition";
    PresetEntry macro;macro.kind="macro";macro.type=macro_entry_type;
    macro.macro_definition="removed-macro";macro.pinned_revision=7;
    macro.overrides.emplace("macro.offset.amount",27);missing_definition.entries.push_back(macro);
    auto missing_doc=fixture();missing_doc.preset_definitions.emplace(missing_definition.id,missing_definition);
    Session missing(std::move(missing_doc));
    check(decode(encode(missing.document()))==missing.document(),
        "Native roundtrip retains a syntactically valid Preset whose Macro definition is unavailable");
    expect_atomic(missing,"MISSING_MACRO_DEFINITION",
        PresetCommand{ApplyPreset{missing_definition.id,"path","missing-definition-use"}},missing.revision());

    const auto definition=macro_definition();
    PresetDefinition missing_revision=missing_definition;missing_revision.id="missing-revision";
    missing_revision.entries[0].macro_definition=definition.id;
    missing_revision.entries[0].pinned_revision=99;
    auto revision_doc=fixture();revision_doc.macro_definitions.emplace(definition.id,definition);
    revision_doc.preset_definitions.emplace(missing_revision.id,missing_revision);
    Session revision(std::move(revision_doc));
    check(decode(encode(revision.document()))==revision.document(),
        "Native roundtrip retains a Preset that pins a removed Macro revision");
    expect_atomic(revision,"MISSING_MACRO_REVISION",
        PresetCommand{ApplyPreset{missing_revision.id,"path","missing-revision-use"}},revision.revision());

    auto pinned_doc=fixture();pinned_doc.macro_definitions.emplace(definition.id,definition);
    PresetDefinition dependent=missing_revision;dependent.id="macro-dependent";
    dependent.entries[0].pinned_revision=1;
    pinned_doc.preset_definitions.emplace(dependent.id,dependent);
    Session pinned(std::move(pinned_doc));
    const auto pinned_before=pinned.document();const auto pinned_native=encode(pinned.document());
    const auto pinned_revision=pinned.revision();const auto pinned_history=pinned.history();
    rejects("MACRO_IN_USE",[&]{apply_command(pinned,Command{MacroCommand{DeleteMacroDefinition{definition.id}}});});
    check(pinned.document()==pinned_before&&encode(pinned.document())==pinned_native&&
        pinned.revision()==pinned_revision&&pinned.history()==pinned_history,
        "Macro deletion is atomic when a Preset retains its stable definition reference");
}
void stack_parity_snapshot_undo_and_native_reopen() {
    Session direct(fixture()),preset(fixture());
    auto offset=default_operation("pre-op-1","nect.shape.offset");offset.parameters.at("amount").literal=18;
    auto repeater=default_operation("pre-op-2","nect.shape.repeater");
    repeater.parameters.at("copies").literal=4;repeater.parameters.at("position_x").literal=36;
    direct.apply({AddOperation{"path",offset,1},AddOperation{"path",repeater,2}},0);
    preset.apply_preset_command(PresetCommand{CreatePreset{sample_preset()}},0);
    preset.apply_preset_command(PresetCommand{ApplyPreset{"preset","path","pre"}},preset.revision());
    const auto& applied=preset.document().objects.at("path").stack;
    check(applied.size()==3&&applied[1].id=="pre-op-1"&&applied[2].id=="pre-op-2",
        "Preset appends fresh Offset then Repeater instances with stable generated IDs");
    check(export_svg(direct.document(),"preset-comp","preset-art")==
        export_svg(preset.document(),"preset-comp","preset-art"),
        "Direct Offset then Repeater and preset application produce identical exported semantics");
    const auto states=preset.history().states.size();const auto before_undo=preset.revision();
    preset.undo(before_undo);
    check(preset.revision()==before_undo+1&&preset.document().objects.at("path").stack.size()==1&&
        preset.document().preset_definitions.contains("preset"),
        "One Undo removes the whole applied stack and preserves the saved definition");
    preset.redo(preset.revision());
    check(preset.document().objects.at("path").stack.size()==3&&preset.history().states.size()==states,
        "One Redo restores both applied operations together");

    auto edited=sample_preset();edited.entries[0].parameters["amount"]=9;
    const auto update_revision=preset.revision();
    preset.apply_preset_command(PresetCommand{UpdatePreset{edited}},update_revision);
    check(preset.document().objects.at("path").stack[1].parameters.at("amount").literal==18,
        "Editing a definition leaves its already-applied operation snapshot unchanged");
    preset.apply_preset_command(PresetCommand{ApplyPreset{"preset","path","later"}},preset.revision());
    const auto& stack=preset.document().objects.at("path").stack;
    check(stack[3].parameters.at("amount").literal==9&&stack[4].parameters.at("copies").literal==4&&
        stack[4].parameters.at("position_x").literal==36,
        "Later applications use edited literal parameters and options");
    const auto bytes=encode(preset.document());
    check(decode(bytes)==preset.document(),"Native 0.67 cold reopen preserves preset definitions and applied stacks");
    auto native_065=bytes;const auto current_version=native_065.find("\"version\":\"0.70\"");
    check(current_version!=std::string::npos,"Schema v1 Preset fixture identifies current native version");
    native_065.replace(current_version,std::string("\"version\":\"0.70\"").size(),"\"version\":\"0.65\"");
    check(decode(native_065)==preset.document(),"Native 0.65 remains compatible with Preset schema v1");
    auto populated_previous=test_support::without_empty_macro_and_definition_fields_for_legacy_fixture(bytes);
    check(populated_previous.find("\"definitions\"")==std::string::npos,"Downgraded native has no Definition field");
    const auto populated_version=populated_previous.find("\"version\":\"0.70\"");
    check(populated_version!=std::string::npos,"Preset-only native document writes 0.67");
    populated_previous.replace(populated_version,std::string("\"version\":\"0.70\"").size(),"\"version\":\"0.63\"");
    check(decode(populated_previous)==preset.document(),"Native 0.63 with populated Presets remains cold-readable");
    auto previous=test_support::without_empty_presets_for_legacy_fixture(encode(fixture()));
    const auto version=previous.find("\"version\":\"0.70\"");
    check(version!=std::string::npos,"Fresh native fixture writes v0.67 and empty extension arrays");
    previous.replace(version,std::string("\"version\":\"0.70\"").size(),"\"version\":\"0.62\"");
    check(decode(previous)==fixture(),"Native 0.62 remains readable with an empty preset map");
}
void validation_and_apply_failures_are_atomic() {
    Session session(fixture());session.apply_preset_command(PresetCommand{CreatePreset{sample_preset()}},0);
    const auto base=sample_preset();
    auto invalid=base;invalid.schema_version=3;
    expect_atomic(session,"UNSUPPORTED_PRESET_SCHEMA",PresetCommand{UpdatePreset{invalid}},session.revision());
    invalid=base;invalid.target_domain="group_pixels";
    expect_atomic(session,"INVALID_PRESET_DOMAIN",PresetCommand{UpdatePreset{invalid}},session.revision());
    invalid=base;invalid.entries[0].version=2;
    expect_atomic(session,"UNSUPPORTED_PRESET_VERSION",PresetCommand{UpdatePreset{invalid}},session.revision());
    invalid=base;std::swap(invalid.entries[0],invalid.entries[1]);
    expect_atomic(session,"INVALID_PRESET_ORDER",PresetCommand{UpdatePreset{invalid}},session.revision());
    invalid=base;invalid.entries[0].parameters["extra"]=1;
    expect_atomic(session,"INVALID_PRESET_PARAMETERS",PresetCommand{UpdatePreset{invalid}},session.revision());
    invalid=base;invalid.entries[0].macro_definition="hidden-macro";
    expect_atomic(session,"INVALID_PRESET_ENTRY",PresetCommand{UpdatePreset{invalid}},session.revision());
    invalid=base;invalid.entries[0].parameters["amount"]=1e9;
    expect_atomic(session,"OUT_OF_RANGE",PresetCommand{UpdatePreset{invalid}},session.revision());
    invalid=base;invalid.entries[0].parameters["amount"]=std::numeric_limits<double>::infinity();
    expect_atomic(session,"NON_FINITE",PresetCommand{UpdatePreset{invalid}},session.revision());
    expect_atomic(session,"DUPLICATE_ID",PresetCommand{CreatePreset{base}},session.revision());
    expect_atomic(session,"REVISION_CONFLICT",PresetCommand{RenamePreset{"preset","stale"}},session.revision()-1);
    session.begin_gesture(session.revision());
    expect_atomic(session,"GESTURE_ACTIVE",PresetCommand{RenamePreset{"preset","during gesture"}},session.revision());
    session.cancel_gesture();
    expect_atomic(session,"MISSING_PRESET",PresetCommand{DeletePreset{"missing"}},session.revision());

    auto open=fixture(false);Session open_session(open);
    open_session.apply_preset_command(PresetCommand{CreatePreset{base}},0);
    expect_atomic(open_session,"OFFSET_OPEN_PATH",PresetCommand{ApplyPreset{"preset","path","open"}},open_session.revision());

    auto group_document=fixture();Object group;group.id="group";group.name="Group";group.kind=Kind::group;
    group_document.objects.emplace(group.id,group);group_document.compositions.front().roots.push_back(group.id);
    Session group_session(group_document);group_session.apply_preset_command(PresetCommand{CreatePreset{base}},0);
    expect_atomic(group_session,"INVALID_DOMAIN",PresetCommand{ApplyPreset{"preset","group","wrong-target"}},group_session.revision());

    Session missing(fixture());PresetDefinition empty_metadata;empty_metadata.id="missing-pair";empty_metadata.label="Missing";
    expect_atomic(missing,"INVALID_PRESET_ORDER",
        PresetCommand{CreatePresetFromStack{empty_metadata,"path"}},missing.revision());
    auto extra_document=fixture();auto& extra_stack=extra_document.objects.at("path").stack;
    extra_stack.push_back(default_operation("extra-offset","nect.shape.offset"));
    extra_stack.push_back(default_operation("extra-repeater","nect.shape.repeater"));
    extra_stack.push_back(default_operation("extra-offset-two","nect.shape.offset"));
    Session extra(std::move(extra_document));PresetDefinition extra_metadata;extra_metadata.id="extra-pair";extra_metadata.label="Extra";
    expect_atomic(extra,"INVALID_PRESET_ORDER",
        PresetCommand{CreatePresetFromStack{extra_metadata,"path"}},extra.revision());

    auto full=fixture();auto& stack=full.objects.at("path").stack;
    for(int i=0;i<126;++i)stack.push_back(default_operation("fill-"+std::to_string(i),"nect.paint.fill"));
    Session full_session(std::move(full));full_session.apply_preset_command(PresetCommand{CreatePreset{base}},0);
    expect_atomic(full_session,"LIMIT",PresetCommand{ApplyPreset{"preset","path","full"}},full_session.revision());

    auto collision=fixture();Object colliding_object;colliding_object.id="collision-op-1";colliding_object.name="ID collision";
    collision.objects.emplace(colliding_object.id,colliding_object);
    collision.compositions.front().roots.push_back("collision-op-1");Session collision_session(collision);
    collision_session.apply_preset_command(PresetCommand{CreatePreset{base}},0);
    expect_atomic(collision_session,"DUPLICATE_ID",PresetCommand{ApplyPreset{"preset","path","collision"}},collision_session.revision());
}
void capture_refusal_lists_exact_source_refs() {
    auto document=fixture();auto& stack=document.objects.at("path").stack;
    auto offset=default_operation("source-offset","nect.shape.offset");
    offset.enabled_driver=operation_ref("path","fill","enabled");
    offset.parameters.at("amount").expression=Expression{"2 + 3"};
    auto repeater=default_operation("source-repeater","nect.shape.repeater");
    repeater.parameters.at("copies").expression=Expression{"4"};
    repeater.enabled_expression=Expression{"false",1};
    stack.push_back(offset);stack.push_back(repeater);
    Session session(document);const auto before=session.document();const auto revision=session.revision();const auto history=session.history();
    try {
        PresetDefinition metadata;metadata.id="captured";metadata.label="Captured";
        session.apply_preset_command(PresetCommand{CreatePresetFromStack{metadata,"path"}},revision);
    } catch(const Error& error) {
        const std::vector<Ref> expected{
            operation_ref("path","source-offset","enabled"),
            operation_ref("path","source-offset","amount"),
            operation_ref("path","source-repeater","enabled"),
            operation_ref("path","source-repeater","copies")};
        std::string actual=error.code;
        for(const auto& ref:error.references)actual+=" "+ref.object+"/"+ref.point+"/"+ref.field;
        check(error.code=="PRESET_NONPORTABLE_SOURCE"&&error.references==expected,
            "Driven enabled, option, expression and linked Scalar fields report exact source operation Refs; got "+actual);
        check(session.document()==before&&session.revision()==revision&&session.history()==history,
            "Refused source capture leaves native authored state, revision and history unchanged");return;
    }
    throw std::runtime_error("Expected PRESET_NONPORTABLE_SOURCE");
}
void capture_filters_other_stack_entries_explicitly() {
    auto document=fixture();auto& stack=document.objects.at("path").stack;
    auto offset=default_operation("captured-offset","nect.shape.offset");offset.parameters.at("amount").literal=18;
    auto repeater=default_operation("captured-repeater","nect.shape.repeater");
    repeater.parameters.at("copies").literal=4;repeater.parameters.at("position_x").literal=36;
    stack.push_back(offset);stack.push_back(repeater);
    PresetDefinition metadata;metadata.id="filtered";metadata.label="Filtered";
    const auto captured=capture_preset_definition(document,metadata,"path");
    check(captured.entries.size()==2&&captured.entries[0].type=="nect.shape.offset"&&
        captured.entries[1].type=="nect.shape.repeater"&&captured.entries[0].parameters.at("amount")==18,
        "Capture selects only the exact supported Offset then Repeater instances and excludes paint stack entries");

    auto option_driven=document;option_driven.objects.at("path").stack[1].fill_rule_driver=
        FillRuleDriver{operation_ref("path","fill","fill_rule")};
    try {
        PresetDefinition option_metadata;option_metadata.id="option-driven";option_metadata.label="Option";
        (void)capture_preset_definition(option_driven,option_metadata,"path");
    } catch(const Error& error) {
        check(error.code=="PRESET_NONPORTABLE_SOURCE"&&error.references==
            std::vector<Ref>{operation_ref("path","captured-offset","fill_rule")},
            "Capture reports a driven applicable option as its exact source Ref");return;
    }
    throw std::runtime_error("Expected driven option capture refusal");
}
void json_lines_and_native_payload_contract() {
    auto legacy_document=fixture();
    legacy_document.objects.at("path").stack.push_back(default_operation("legacy-offset","nect.shape.offset"));
    legacy_document.objects.at("path").stack.push_back(default_operation("legacy-repeater","nect.shape.repeater"));
    Session legacy_session(std::move(legacy_document));
    const auto legacy_capture=request(legacy_session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"create_preset_from_stack","id":"legacy-capture","object":"path","label":"Legacy capture"}]})");
    const auto legacy_readback=request(legacy_session,R"({"op":"preset","id":"legacy-capture"})");
    check(legacy_capture.find("\"ok\":true")!=std::string::npos&&
        legacy_readback.find("\"schema_version\":1")!=std::string::npos&&
        legacy_session.document().preset_definitions.at("legacy-capture").entries.size()==2,
        "Existing JSON-lines create-from-stack defaults to schema v1 and preserves Offset/Repeater filtering");
    const auto raw=R"({"id":"preset-api","schema_version":1,"label":"API stack","category":"Shape","tags":["api"],"target_domain":"local_paths_and_paint","entries":[{"type":"nect.shape.offset","version":1,"enabled":true,"parameters":{"amount":18,"miter_limit":4},"composite":"below","fill_rule":"nonzero","line_join":"miter","line_cap":"butt"},{"type":"nect.shape.repeater","version":1,"enabled":true,"parameters":{"copies":4,"position_x":36,"position_y":0,"anchor_x":0,"anchor_y":0,"rotation":0,"scale_x":1,"scale_y":1,"offset":0,"start_opacity":1,"end_opacity":1},"composite":"below","fill_rule":"nonzero","line_join":"miter","line_cap":"butt"}]})";
    Session session(fixture());
    const auto create=request(session,"{\"op\":\"apply\",\"expected_revision\":0,\"commands\":[{\"type\":\"create_preset\",\"definition\":"+std::string(raw)+"}]}");
    check(create.find("\"ok\":true")!=std::string::npos&&create.find("\"revision\":1")!=std::string::npos,
        "JSON-lines create_preset calls the dedicated Session operation");
    const auto list=request(session,R"({"op":"presets"})");
    const auto inspect=request(session,R"({"op":"preset","id":"preset-api"})");
    check(list.find("API stack")!=std::string::npos&&inspect.find("\"schema_version\":1")!=std::string::npos,
        "JSON-lines list and inspect resolve definitions by stable preset ID");
    const auto apply=request(session,R"({"op":"apply","expected_revision":1,"commands":[{"type":"apply_preset","preset":"preset-api","object":"path","operation_id_prefix":"api-use"}]})");
    check(apply.find("\"ok\":true")!=std::string::npos&&apply.find("\"applied_presets\":[{")!=std::string::npos&&
        apply.find("api-use-op-1")!=std::string::npos&&apply.find("api-use-op-2")!=std::string::npos&&
        session.revision()==2,"JSON-lines apply reports its generated stable operation IDs and one revision");
    const auto before=session.document();const auto revision=session.revision();const auto history=session.history();
    const auto mixed=request(session,R"({"op":"apply","expected_revision":2,"commands":[{"type":"apply_preset","preset":"preset-api","object":"path","operation_id_prefix":"mixed"},{"type":"rename","object":"path","name":"No partial"}]})");
    check(mixed.find("PRESET_SINGLE_OPERATION")!=std::string::npos&&session.document()==before&&
        session.revision()==revision&&session.history()==history,
        "JSON-lines refuses to mix preset operations into generic batches without partial mutation");
    const auto captured=request(session,R"({"op":"apply","expected_revision":2,"commands":[{"type":"create_preset_from_stack","id":"preset-captured","object":"path","label":"Captured","category":"Shape","tags":["api"],"schema_version":2}]})");
    check(captured.find("\"ok\":true")!=std::string::npos&&
        captured.find("api-use-op-1")!=std::string::npos&&captured.find("api-use-op-2")!=std::string::npos&&
        captured.find("captured_source_operations")!=std::string::npos&&
        captured.find("captured_source_entries")!=std::string::npos&&captured.find("nect.paint.fill")!=std::string::npos&&
        session.revision()==3,
        "JSON-lines create-from-stack reports every exact source operation ID and tagged built-in entry");
    const auto captured_preset=request(session,R"({"op":"preset","id":"preset-captured"})");
    check(captured_preset.find("\"schema_version\":2")!=std::string::npos&&
        captured_preset.find("\"kind\":\"builtin\"")!=std::string::npos,
        "JSON-lines create-from-stack persists the ordered schema v2 form");

    auto malformed=encode(session.document());
    const auto presets_section=malformed.find("\"presets\":[");
    const auto amount=malformed.find("\"amount\":",presets_section);
    check(amount!=std::string::npos,"Native fixture contains an Offset amount field");
    const auto amount_start=malformed.find(':',amount)+1;const auto amount_end=malformed.find_first_of(",}",amount_start);
    malformed.replace(amount_start,amount_end-amount_start,"1000000000");
    rejects("OUT_OF_RANGE",[&]{(void)decode(malformed);});
    auto malformed_version=encode(session.document());
    const auto schema_version=malformed_version.find("\"schema_version\":1");
    check(schema_version!=std::string::npos,"Native definition schema version is serialized");
    malformed_version.replace(schema_version,std::string("\"schema_version\":1").size(),"\"schema_version\":99");
    rejects("UNSUPPORTED_PRESET_SCHEMA",[&]{(void)decode(malformed_version);});
}
}

int main() {
    try {
        macro_capture_apply_and_pinned_revision_contract();
        unavailable_macro_pins_are_preserved_but_not_applied();
        stack_parity_snapshot_undo_and_native_reopen();
        validation_and_apply_failures_are_atomic();
        capture_refusal_lists_exact_source_refs();
        capture_filters_other_stack_entries_explicitly();
        json_lines_and_native_payload_contract();
        std::cout<<checks<<" preset contract checks passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
