#include "folder_library.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;

namespace {
int checks=0;
constexpr const char* amount_id="macro.offset.amount";
constexpr const char* copies_id="macro.copies";
constexpr const char* rotation_id="macro.rotation";
constexpr const char* offset_enabled_id="macro.offset.enabled";
constexpr const char* repeater_enabled_id="macro.repeater.enabled";
using Numbers=std::map<std::string,double>;
using Booleans=std::map<std::string,bool>;
const Numbers numeric_overrides{{amount_id,15},{copies_id,3},{rotation_id,17}};
const Booleans boolean_overrides{{offset_enabled_id,false},{repeater_enabled_id,true}};

void check(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
    ++checks;
}
template<class F>void rejects(const char* code,F action) {
    try {action();}
    catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        return;
    }
    throw std::runtime_error("Expected refusal: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands) {session.apply(commands,session.revision());}
QByteArray read_bytes(const QString& path) {
    QFile file(path);check(file.open(QIODevice::ReadOnly),"Open exact test-owned asset or native snapshot");
    const auto bytes=file.readAll();check(file.error()==QFileDevice::NoError,"Read exact test-owned file completely");return bytes;
}
void write_bytes(const QString& path,const QByteArray& bytes) {
    QFile file(path);check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Open exact test-owned fixture for writing");
    check(file.write(bytes)==bytes.size()&&file.flush(),"Write exact test-owned fixture completely");
}
QString asset_path(const QString& root,const MacroAssetRefV1& ref) {return QDir(root).filePath(ref.asset_id+".macro.json");}
QString sha256(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
QByteArray payload_bytes(const MacroDefinition& source,unsigned schema=0) {
    return QByteArray::fromStdString(canonical_macro_payload(source,schema));
}
MacroNode& node(MacroDefinitionRevision& revision,const Id& id) {
    const auto found=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& item){return item.operation.id==id;});
    if(found==revision.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
const MacroNode& node(const MacroDefinitionRevision& revision,const Id& id) {
    const auto found=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& item){return item.operation.id==id;});
    if(found==revision.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
MacroDefinition original_definition(bool chain) {
    MacroDefinitionRevision initial;initial.graph_version=chain?2:1;
    initial.input={"input","local_paths_and_paint"};initial.output={"output","local_paths_and_paint"};
    const std::vector<Id> ids=chain?std::vector<Id>{"offset-first","offset-target","repeater-target","repeater-last"}:
        std::vector<Id>{"offset-target","repeater-target"};
    MacroEndpoint cursor{"",initial.input.id};
    for(const auto& id:ids) {
        const bool offset=id.starts_with("offset");
        auto operation=default_operation(id,offset?"nect.shape.offset":"nect.shape.repeater");
        if(offset)operation.parameters.at("amount").literal=id=="offset-target"?12:2;
        else {
            operation.parameters.at("copies").literal=2;
            operation.parameters.at("position_x").literal=id=="repeater-target"?125:21;
            operation.parameters.at("position_y").literal=id=="repeater-target"?11:80;
            operation.parameters.at("rotation").literal=id=="repeater-target"?9:-13;
        }
        initial.nodes.push_back({operation,id+"-input",id+"-output"});
        initial.edges.push_back({cursor,{id,id+"-input"}});cursor={id,id+"-output"};
    }
    initial.output_mapping=cursor;initial.edges.push_back({cursor,{"",initial.output.id}});
    initial.public_parameters={{amount_id,"Amount","offset-target","amount","number","du","local_paths_and_paint"}};
    if(chain) {
        // Edges execute the chain; retained node/edge storage order is authored state.
        std::reverse(initial.nodes.begin(),initial.nodes.end());std::reverse(initial.edges.begin(),initial.edges.end());
    }
    MacroDefinition result;result.id="source-definition";result.label="Boolean Library Macro";
    result.revisions.emplace(1,std::move(initial));return result;
}
MacroDefinitionRevision published_revision(const MacroDefinitionRevision& initial,std::uint64_t revision=2) {
    auto published=initial;published.revision=revision;published.interface_version=3;
    node(published,"repeater-target").operation.enabled=false;
    published.public_parameters.push_back({copies_id,"Copies","repeater-target","copies","number","scalar","local_paths_and_paint"});
    published.public_parameters.push_back({rotation_id,"Rotation","repeater-target","rotation","number","degree","local_paths_and_paint"});
    published.public_parameters.push_back({offset_enabled_id,"Use Offset","offset-target","enabled","boolean","boolean","local_paths_and_paint"});
    published.public_parameters.push_back({repeater_enabled_id,"Use Repeater","repeater-target","enabled","boolean","boolean","local_paths_and_paint"});
    return published;
}
MacroDefinition boolean_definition(bool chain) {
    auto result=original_definition(chain);result.revisions.emplace(2,published_revision(result.revisions.at(1)));result.latest_revision=2;
    return result;
}
Document fixture(const Id& id) {
    auto document=empty_document(id,"composition","artboard");
    Object path;path.id="path";path.name="Boolean Library target";Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t index=0;index<anchors.size();++index) {
        Point point;point.id="point-"+std::to_string(index+1);point.x.literal=anchors[index].x;point.y.literal=anchors[index].y;
        contour.points.push_back(point);
    }
    path.contours.push_back(contour);path.stack.emplace_back(default_operation("fill","nect.paint.fill"));
    document.objects.emplace(path.id,path);document.compositions.front().roots.push_back(path.id);return document;
}
const MacroInstance& instance(const Session& session) {return *session.document().objects.at("path").stack.at(1).macro;}
void import_macro(Session& target,const MacroDefinition& source,const LibraryMacroAssetV1& asset,std::uint64_t pin,
    const Numbers& numbers={},const Booleans& booleans={}) {
    InstantiateMacro command{"path","imported-definition","imported-instance",pin,1};
    command.imported_definition=source;command.asset_id=asset.ref.asset_id.toStdString();command.accepted_asset_revision=asset.accepted_revision;
    command.overrides=numbers;command.boolean_overrides=booleans;apply(target,{MacroCommand{std::move(command)}});
}
void check_envelope(const QString& path,const LibraryMacroAssetV1& asset,const MacroDefinition& source) {
    QJsonParseError error;const auto document=QJsonDocument::fromJson(read_bytes(path),&error);
    check(error.error==QJsonParseError::NoError&&document.isObject(),"Published schema3 envelope is a JSON object");
    const auto envelope=document.object();const auto payload=envelope.value("payload").toString().toUtf8();
    check(envelope.value("version").toInt()==1&&envelope.value("kind").toString()=="macro_definition"&&
        envelope.value("payload_schema").toInt()==3&&asset.payload_schema==3,
        "Envelope version remains 1 and selects portable Macro payload schema3");
    check(payload==payload_bytes(source,3)&&payload==payload_bytes(source)&&sha256(payload)==asset.sha256&&
        envelope.value("sha256").toString()==asset.sha256,"Envelope and metadata hash the exact canonical schema3 payload bytes");
    check(envelope.value("asset_id").toString()==asset.ref.asset_id&&
        envelope.value("accepted_revision").toDouble()==static_cast<double>(asset.accepted_revision)&&
        envelope.value("label").toString()==QString::fromStdString(source.label)&&asset.label==QString::fromStdString(source.label),
        "Schema3 publication preserves exact AssetID, accepted asset revision and label");
    check(asset.ref.asset_id!=QString::fromStdString(source.id)&&
        QUuid(asset.ref.asset_id).toString(QUuid::WithoutBraces)==asset.ref.asset_id,
        "Workspace AssetID is canonical and distinct from the source DefinitionID");
    const auto revisions=QJsonDocument::fromJson(payload).object().value("revisions").toArray();
    check(revisions.at(0).toObject().value("interface_version").isUndefined()&&
        revisions.at(1).toObject().value("interface_version").toInt()==3,"Original revision1 interface and explicit revision2 interface3 survive publication");
    bool found=false;
    for(const auto value:revisions.at(1).toObject().value("nodes").toArray()) {
        const auto operation=value.toObject().value("operation").toObject();
        if(operation.value("id").toString()!="repeater-target")continue;
        found=true;check(operation.value("enabled").isBool()&&!operation.value("enabled").toBool(),
            "False internal enabled default is a JSON boolean in the canonical Library payload");
    }
    check(found,"Payload retains the identified boolean target node");
}
void cold_read(const QString& native_path,const QString& settings_path,const QString& root,const QString& asset_id) {
    const auto bytes=read_bytes(native_path);Session reopened(decode(bytes.toStdString()));
    const auto& imported=reopened.document().macro_definitions.at("imported-definition");
    check(imported.latest_revision==2&&imported.revisions.at(1).interface_version==1&&imported.revisions.at(2).interface_version==3&&
        !node(imported.revisions.at(2),"repeater-target").operation.enabled&&instance(reopened).pinned_revision==2,
        "Cold native reopen retains all imported revisions, exact false node default and explicit interface3 pin");
    check(instance(reopened).overrides==numeric_overrides&&instance(reopened).boolean_overrides==boolean_overrides&&
        !macro_parameter_boolean_value(reopened.document(),"path","imported-instance",offset_enabled_id)&&
        macro_parameter_boolean_value(reopened.document(),"path","imported-instance",repeater_enabled_id)&&
        macro_parameter_value(reopened.document(),"path","imported-instance",amount_id)==15&&
        macro_parameter_value(reopened.document(),"path","imported-instance",copies_id)==3&&
        macro_parameter_value(reopened.document(),"path","imported-instance",rotation_id)==17,
        "Cold read preserves native bool false/true alongside exact unchanged numeric overrides");
    check(QByteArray::fromStdString(encode(reopened.document()))==bytes,"Cold readback preserves the exact imported native087 bytes");
    QSettings settings(settings_path,QSettings::IniFormat);FolderLibrary reader(settings,{}, {},root);LibraryMacroAssetV1 metadata;
    const auto source=reader.read_macro_asset({asset_id},&metadata);auto expected=source;expected.id=imported.id;
    check(imported==expected&&metadata.payload_schema==3&&metadata.accepted_revision==1&&metadata.sha256==sha256(payload_bytes(source)),
        "Fresh process reopens the exact schema3 Library asset independently of the saved imported document");
    apply(reopened,{MacroCommand{ResetMacroOverride{"path","imported-instance",repeater_enabled_id}}});
    check(!macro_parameter_boolean_value(reopened.document(),"path","imported-instance",repeater_enabled_id)&&
        instance(reopened).overrides==numeric_overrides&&instance(reopened).boolean_overrides==Booleans{{offset_enabled_id,false}},
        "Cold Reset recovers the pinned false default and leaves numeric and other boolean overrides intact");
}
void cold_reopen(const QString& scratch,const QString& settings_path,const QString& root,const LibraryMacroAssetV1& asset,const Session& imported) {
    const auto native_path=scratch+"/imported.nect";write_bytes(native_path,QByteArray::fromStdString(encode(imported.document())));
    QProcess child;child.start(QCoreApplication::applicationFilePath(),
        QStringList{"--cold-read",native_path,settings_path,root,asset.ref.asset_id});
    check(child.waitForStarted(10000),"Start independent schema3 Library/native reader process");
    const bool finished=child.waitForFinished(30000);
    const auto output=child.readAllStandardOutput()+child.readAllStandardError();
    check(finished&&child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,
        "Independent schema3 Library/native readback passed: "+output.toStdString());
}

void publication_import_and_update(const QString& scratch,bool chain) {
    const auto root=scratch+"/assets";const auto settings_path=scratch+"/library.ini";
    QSettings settings(settings_path,QSettings::IniFormat);FolderLibrary library(settings,{}, {},root);
    Session author(fixture("author-document"));const auto initial=original_definition(chain);
    apply(author,{MacroCommand{CreateMacroDefinition{initial}},MacroCommand{InstantiateMacro{"path",initial.id,"source-instance",1,1}}});
    apply(author,{MacroCommand{UpdateMacroDefinition{initial.id,published_revision(initial.revisions.at(1))}}});
    check(author.document().objects.at("path").stack.at(1).macro->pinned_revision==1,
        "Authoring interface3 preserves the existing original-revision pin");
    apply(author,{MacroCommand{UpdateMacroInstance{"path","source-instance",2}},
        MacroCommand{SetMacroOverride{"path","source-instance",copies_id,4}},
        MacroCommand{SetMacroBooleanOverride{"path","source-instance",repeater_enabled_id,true}}});
    const auto source=author.document().macro_definitions.at(initial.id);const auto source_native=encode(author.document());
    const auto source_revision=author.revision();const auto source_history=author.history();
    check(source==boolean_definition(chain),"Revision2 retains its false enabled default despite a source instance's true override");
    check(library.macro_assets().isEmpty()&&!QFileInfo::exists(root),"Listing the absent asset store is read-only");
    const auto asset=library.publish_macro_asset(source);const auto path=asset_path(root,asset.ref);const auto original_bytes=read_bytes(path);
    check(asset.accepted_revision==1,"First boolean publication has accepted workspace asset revision1");check_envelope(path,asset,source);
    check(encode(author.document())==source_native&&author.revision()==source_revision&&author.history()==source_history,
        "Publishing preserves the source native bytes, numeric/boolean overrides, revision and complete Undo history");
    const auto favorite=library.add_favorite(asset.ref,1);QSettings reader_settings(settings_path,QSettings::IniFormat);
    FolderLibrary reader(reader_settings,{}, {},root);LibraryMacroAssetV1 metadata;const auto loaded=reader.read_macro_asset(asset.ref,&metadata);
    check(loaded==source&&metadata.ref==asset.ref&&metadata.payload_schema==3&&metadata.sha256==asset.sha256&&
        metadata.accepted_revision==asset.accepted_revision&&metadata.label==asset.label&&metadata.available,
        "Fresh Library read retains every revision, graph-local ID, node order, default and numeric/boolean mapping");
    const auto inventory=reader.macro_assets();check(inventory.size()==1&&inventory.front().ref==asset.ref&&
        inventory.front().payload_schema==3&&inventory.front().available&&reader.favorite_for_slot(1)->favorite_id==favorite.favorite_id&&
        reader.favorite_status(favorite)=="Available","Schema3 inventory and fresh Favorite resolve the same exact available AssetID");
    Session defaults(fixture("defaults-document"));import_macro(defaults,loaded,metadata,2);
    check(instance(defaults).pinned_revision==2&&instance(defaults).overrides.empty()&&instance(defaults).boolean_overrides.empty()&&
        macro_parameter_boolean_value(defaults.document(),"path","imported-instance",offset_enabled_id)&&
        !macro_parameter_boolean_value(defaults.document(),"path","imported-instance",repeater_enabled_id)&&
        macro_parameter_value(defaults.document(),"path","imported-instance",amount_id)==12&&
        macro_parameter_value(defaults.document(),"path","imported-instance",copies_id)==2&&
        macro_parameter_value(defaults.document(),"path","imported-instance",rotation_id)==9,
        "Default import preserves true/false internal enabled literals and all three numeric defaults without creating overrides");
    Session legacy(fixture("legacy-pin-document"));import_macro(legacy,loaded,metadata,1);
    check(instance(legacy).pinned_revision==1&&legacy.document().macro_definitions.at("imported-definition").revisions.size()==2&&
        macro_parameter_value(legacy.document(),"path","imported-instance",amount_id)==12,
        "Schema3 asset can explicitly import its original pin while retaining the later boolean interface");
    const auto legacy_before=encode(legacy.document());const auto legacy_history=legacy.history();const auto legacy_revision=legacy.revision();
    rejects("MISSING_MACRO_PARAMETER",[&]{apply(legacy,{MacroCommand{SetMacroBooleanOverride{"path","imported-instance",repeater_enabled_id,true}}});});
    check(encode(legacy.document())==legacy_before&&legacy.revision()==legacy_revision&&legacy.history()==legacy_history,
        "Boolean controls are unavailable on the retained original pin and refusal is atomic");
    Session imported(fixture("imported-document"));const auto before=imported.document();const auto before_revision=imported.revision();
    const auto history_size=imported.history().states.size();import_macro(imported,loaded,metadata,2,numeric_overrides,boolean_overrides);
    auto expected=source;expected.id="imported-definition";
    check(imported.document().macro_definitions.at(expected.id)==expected&&expected.id!=source.id&&expected.id!=asset.ref.asset_id.toStdString()&&
        instance(imported).pinned_revision==2&&instance(imported).overrides==numeric_overrides&&instance(imported).boolean_overrides==boolean_overrides,
        "One compound Library import assigns a fresh DefinitionID and retains exact pin plus numeric and false/true boolean override maps");
    check(imported.revision()==before_revision+1&&imported.history().states.size()==history_size+1,
        "Definition and typed pinned instance import in one Session revision and one history entry");
    const auto imported_native=encode(imported.document());imported.undo(imported.revision());
    check(imported.document()==before,"One Undo removes the imported definition and typed instance together");
    imported.redo(imported.revision());check(encode(imported.document())==imported_native,"One Redo restores the exact typed import");
    cold_reopen(scratch,settings_path,root,asset,imported);
    for(const bool value:{false,true}) {
        const auto before_change=encode(imported.document());apply(imported,{MacroCommand{SetMacroBooleanOverride{"path","imported-instance",repeater_enabled_id,value}}});
        check(macro_parameter_boolean_value(imported.document(),"path","imported-instance",repeater_enabled_id)==value&&
            instance(imported).boolean_overrides.at(repeater_enabled_id)==value&&instance(imported).overrides==numeric_overrides,
            "Imported enabled control accepts exact bool false/true without coercing or changing numeric overrides");
        const auto after_change=encode(imported.document());imported.undo(imported.revision());
        check(encode(imported.document())==before_change,"Imported boolean edit has exact Undo");imported.redo(imported.revision());
        check(encode(imported.document())==after_change,"Imported boolean edit has exact Redo");
    }
    const auto before_reset=encode(imported.document());apply(imported,{MacroCommand{ResetMacroOverride{"path","imported-instance",repeater_enabled_id}}});
    check(!macro_parameter_boolean_value(imported.document(),"path","imported-instance",repeater_enabled_id)&&
        instance(imported).boolean_overrides==Booleans{{offset_enabled_id,false}}&&instance(imported).overrides==numeric_overrides,
        "Boolean Reset restores the pinned false default while preserving all numeric and other boolean overrides");
    imported.undo(imported.revision());check(encode(imported.document())==before_reset,"Boolean Reset has exact one-step Undo");imported.redo(imported.revision());
    apply(imported,{MacroCommand{ResetMacroOverride{"path","imported-instance",offset_enabled_id}}});
    check(macro_parameter_boolean_value(imported.document(),"path","imported-instance",offset_enabled_id)&&instance(imported).boolean_overrides.empty(),
        "Reset of the other enabled control restores its pinned true default");
    check(imported.document().macro_definitions.at(expected.id)==expected&&encode(author.document())==source_native&&
        author.revision()==source_revision&&author.history()==source_history&&read_bytes(path)==original_bytes&&reader.read_macro_asset(asset.ref)==source,
        "All imported typed edits preserve definition defaults, source Session and accepted Library bytes");
    const auto old_pin_native=encode(imported.document());const auto old_pin_revision=imported.revision();const auto old_pin_history=imported.history();
    auto replacement=source;auto third=source.revisions.at(2);third.revision=3;node(third,"repeater-target").operation.enabled=true;
    node(third,"offset-target").operation.parameters.at("amount").literal=31;
    replacement.revisions.emplace(3,third);replacement.latest_revision=3;replacement.label="Explicit boolean asset update";
    const auto updated=library.update_macro_asset(asset.ref,replacement,asset.accepted_revision,asset.sha256);
    check(updated.ref==asset.ref&&updated.accepted_revision==2&&updated.payload_schema==3&&updated.sha256!=asset.sha256,
        "Explicit schema3 update retains AssetID, advances the accepted revision and hashes the replacement payload");check_envelope(path,updated,replacement);
    check(reader.read_macro_asset(asset.ref)==replacement&&encode(imported.document())==old_pin_native&&imported.revision()==old_pin_revision&&
        imported.history()==old_pin_history&&instance(imported).pinned_revision==2&&
        !macro_parameter_boolean_value(imported.document(),"path","imported-instance",repeater_enabled_id)&&
        encode(author.document())==source_native&&author.revision()==source_revision&&author.history()==source_history&&
        library.favorite_for_slot(1)->favorite_id==favorite.favorite_id,
        "Library update preserves old imported pin/default/history, source author history and stable Favorite identity");
    const auto updated_bytes=read_bytes(path);const auto settings_bytes=settings.value("library/v1/state").toByteArray();
    rejects("MACRO_ASSET_REVISION_CONFLICT",[&]{(void)library.update_macro_asset(asset.ref,source,1,updated.sha256);});
    rejects("MACRO_ASSET_REVISION_CONFLICT",[&]{(void)library.update_macro_asset(asset.ref,source,2,asset.sha256);});
    FolderLibrary failed_write(settings,{}, {},root,[](const QString&,const QByteArray&,QString& detail){detail="injected schema3 write refusal";return false;});
    rejects("MACRO_LIBRARY_WRITE_FAILED",[&]{(void)failed_write.update_macro_asset(asset.ref,source,2,updated.sha256);});
    check(read_bytes(path)==updated_bytes&&reader.read_macro_asset(asset.ref)==replacement&&encode(imported.document())==old_pin_native&&
        imported.history()==old_pin_history&&settings.value("library/v1/state").toByteArray()==settings_bytes,
        "Stale revision/hash and failed schema3 write preserve accepted asset, imported history and Favorite settings");
    LibraryMacroAssetV1 latest_metadata;const auto latest=reader.read_macro_asset(asset.ref,&latest_metadata);Session future(fixture("future-document"));
    import_macro(future,latest,latest_metadata,3);
    check(instance(future).pinned_revision==3&&future.document().macro_definitions.at("imported-definition").revisions.size()==3&&
        macro_parameter_boolean_value(future.document(),"path","imported-instance",repeater_enabled_id)&&
        macro_parameter_value(future.document(),"path","imported-instance",amount_id)==31&&latest_metadata.accepted_revision==2,
        "Future import explicitly pins new boolean/numeric defaults while retaining both older definition revisions");
}

void retained_revision_schema_selection(const QString& scratch) {
    QSettings settings(scratch+"/library.ini",QSettings::IniFormat);const auto root=scratch+"/assets";FolderLibrary library(settings,{}, {},root);
    for(const bool numeric_latest:{false,true}) {
        auto source=boolean_definition(false);source.id=numeric_latest?"retained-with-numeric-latest":"retained-with-legacy-latest";
        auto latest=numeric_latest?source.revisions.at(2):source.revisions.at(1);latest.revision=3;
        latest.interface_version=numeric_latest?2:1;node(latest,"repeater-target").operation.enabled=true;
        if(numeric_latest)latest.public_parameters.resize(3);
        source.revisions.emplace(3,latest);source.latest_revision=3;const auto asset=library.publish_macro_asset(source);
        check(asset.payload_schema==3&&portable_macro_payload_schema(source)==3&&library.read_macro_asset(asset.ref)==source,
            "Retained interface3 selects schema3 even when the latest pin has only legacy/numeric controls");check_envelope(asset_path(root,asset.ref),asset,source);
        for(const unsigned schema:{1U,2U}) {
            rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(source,schema);});
            rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)read_canonical_macro_payload(canonical_macro_payload(source),schema);});
        }
    }
    auto internal_only=boolean_definition(false);internal_only.id="internal-enabled-definition";internal_only.revisions.at(2).public_parameters.resize(3);
    const auto internal_asset=library.publish_macro_asset(internal_only);
    check(internal_asset.payload_schema==3&&library.read_macro_asset(internal_asset.ref)==internal_only,
        "Interface3 retains false internal enabled literals and requires schema3 even with no public boolean controls");
    auto numeric=boolean_definition(true);numeric.id="numeric-only-definition";numeric.revisions.at(2).interface_version=2;
    numeric.revisions.at(2).public_parameters.resize(3);node(numeric.revisions.at(2),"repeater-target").operation.enabled=true;
    const auto numeric_bytes=payload_bytes(numeric,2);const auto numeric_asset=library.publish_macro_asset(numeric);
    check(numeric_asset.payload_schema==2&&payload_bytes(numeric)==numeric_bytes&&payload_bytes(numeric,3)==numeric_bytes&&
        library.read_macro_asset(numeric_asset.ref)==numeric,"Numeric-only payload auto-selects schema2 and explicit schema3 leaves its canonical bytes unchanged");
}

void tampering_remains_atomic(const QString& scratch) {
    QSettings settings(scratch+"/library.ini",QSettings::IniFormat);const auto root=scratch+"/assets";FolderLibrary library(settings,{}, {},root);
    const auto source=boolean_definition(true);const auto asset=library.publish_macro_asset(source);const auto favorite=library.add_favorite(asset.ref,1);
    auto neighbor_source=original_definition(false);neighbor_source.id="legacy-neighbor";const auto neighbor=library.publish_macro_asset(neighbor_source);
    const auto neighbor_path=asset_path(root,neighbor.ref);const auto neighbor_bytes=read_bytes(neighbor_path);const auto path=asset_path(root,asset.ref);
    const auto original=read_bytes(path);const auto envelope=QJsonDocument::fromJson(original).object();const auto preferences=settings.value("library/v1/state").toByteArray();
    const auto refuse=[&](const char* code,QJsonObject changed) {
        const auto tampered=QJsonDocument(changed).toJson(QJsonDocument::Compact);write_bytes(path,tampered);
        LibraryMacroAssetV1 sentinel{{"metadata-must-stay-untouched"},"unchanged",99,"unchanged-hash",99,false,"unchanged-problem"};
        rejects(code,[&]{(void)library.read_macro_asset(asset.ref,&sentinel);});
        rejects(code,[&]{(void)library.update_macro_asset(asset.ref,source,asset.accepted_revision,asset.sha256);});
        check(sentinel.ref.asset_id=="metadata-must-stay-untouched"&&sentinel.label=="unchanged"&&sentinel.accepted_revision==99&&
            sentinel.sha256=="unchanged-hash"&&sentinel.payload_schema==99&&!sentinel.available&&sentinel.problem=="unchanged-problem",
            "Rejected schema3 read leaves all caller metadata fields untouched");
        const auto inventory=library.macro_assets();const auto bad=std::find_if(inventory.begin(),inventory.end(),[&](const auto& item){return item.ref==asset.ref;});
        const auto good=std::find_if(inventory.begin(),inventory.end(),[&](const auto& item){return item.ref==neighbor.ref;});
        check(bad!=inventory.end()&&!bad->available&&bad->problem.contains(QString::fromLatin1(code))&&good!=inventory.end()&&good->available&&
            library.favorite_status(favorite).contains(QString::fromLatin1(code)),"Malformed asset is unavailable at its exact Favorite identity while the legacy neighbor stays available");
        check(read_bytes(path)==tampered&&read_bytes(neighbor_path)==neighbor_bytes&&library.read_macro_asset(neighbor.ref)==neighbor_source&&
            settings.value("library/v1/state").toByteArray()==preferences&&library.favorite_for_slot(1)->favorite_id==favorite.favorite_id,
            "Rejected read, listing and update preserve tampered bytes, neighbor bytes and Favorite state");
        write_bytes(path,original);check(library.read_macro_asset(asset.ref)==source,"Exact accepted bytes restore all interface3 boolean defaults and metadata");
    };
    auto changed=envelope;changed.insert("payload",changed.value("payload").toString()+" ");refuse("MACRO_ASSET_HASH_MISMATCH",changed);
    changed=envelope;changed.insert("sha256",QString(64,QChar('0')));refuse("MACRO_ASSET_HASH_MISMATCH",changed);
    for(const int schema:{1,2}) {changed=envelope;changed.insert("payload_schema",schema);refuse("UNSUPPORTED_PORTABLE_MACRO_GRAPH",changed);}
    changed=envelope;changed.insert("payload_schema",4);refuse("UNSUPPORTED_MACRO_SCHEMA",changed);
    changed=envelope;changed.insert("label","Forged boolean label");refuse("MACRO_ASSET_ENVELOPE_MISMATCH",changed);
    changed=envelope;changed.insert("asset_id",QUuid::createUuid().toString(QUuid::WithoutBraces));refuse("MACRO_ASSET_ID_MISMATCH",changed);
    changed=envelope;const auto noncanonical=payload_bytes(source)+"\n";changed.insert("payload",QString::fromUtf8(noncanonical));
    changed.insert("sha256",sha256(noncanonical));refuse("NONCANONICAL_MACRO_PAYLOAD",changed);
}
}

int main(int argc,char** argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        if(argc==6&&std::string(argv[1])=="--cold-read") {
            const auto arguments=QCoreApplication::arguments();
            cold_read(arguments.at(2),arguments.at(3),arguments.at(4),arguments.at(5));
            std::cout<<"Macro Library schema3 cold readback: "<<checks<<" checks passed\n";return 0;
        }
        QTemporaryDir scratch;check(scratch.isValid(),"Create isolated schema3 Library test directory");
        for(const auto* name:{"graph1","graph2","retained","tampering"})
            check(QDir().mkpath(scratch.path()+"/"+name),"Create isolated schema3 settings directory");
        publication_import_and_update(scratch.path()+"/graph1",false);publication_import_and_update(scratch.path()+"/graph2",true);
        retained_revision_schema_selection(scratch.path()+"/retained");tampering_remains_atomic(scratch.path()+"/tampering");
        std::cout<<"Macro Library schema3 contract: "<<checks<<" checks passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
