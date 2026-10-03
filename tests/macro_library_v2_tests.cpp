#include "folder_library.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
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
void apply(Session& session,std::vector<Command> commands) {
    session.apply(commands,session.revision());
}
QByteArray read_bytes(const QString& path) {
    QFile file(path);
    check(file.open(QIODevice::ReadOnly),"Open exact test-owned asset for reading");
    const auto bytes=file.readAll();
    check(file.error()==QFileDevice::NoError,"Read exact test-owned asset completely");
    return bytes;
}
void write_bytes(const QString& path,const QByteArray& bytes) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Open exact test-owned tamper fixture");
    check(file.write(bytes)==bytes.size()&&file.flush(),"Write exact test-owned tamper fixture completely");
}
QString asset_path(const QString& root,const MacroAssetRefV1& ref) {
    return QDir(root).filePath(ref.asset_id+".macro.json");
}
QString sha256(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
QByteArray payload_bytes(const MacroDefinition& definition,unsigned schema=0) {
    return QByteArray::fromStdString(canonical_macro_payload(definition,schema));
}
void replace_payload(QJsonObject& envelope,const QByteArray& payload) {
    envelope.insert("payload",QString::fromUtf8(payload));
    envelope.insert("sha256",sha256(payload));
}
MacroNode& node(MacroDefinitionRevision& revision,const Id& id) {
    const auto found=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& candidate) {
        return candidate.operation.id==id;
    });
    if(found==revision.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
MacroDefinition definition(bool advanced) {
    MacroDefinitionRevision revision;revision.graph_version=advanced?2:1;
    revision.input={"input","local_paths_and_paint"};revision.output={"output","local_paths_and_paint"};
    const std::vector<Id> ids=advanced?std::vector<Id>{"offset-a","offset-b","repeater-a","repeater-b"}:
        std::vector<Id>{"offset-b","repeater-a"};
    for(const auto& id:ids) {
        const bool offset=id.starts_with("offset");
        auto operation=default_operation(id,offset?"nect.shape.offset":"nect.shape.repeater");
        if(offset)operation.parameters.at("amount").literal=id=="offset-a"?2:12;
        else {
            operation.parameters.at("copies").literal=2;
            operation.parameters.at("position_x").literal=id=="repeater-a"?125:21;
            operation.parameters.at("position_y").literal=id=="repeater-a"?11:80;
            operation.parameters.at("rotation").literal=id=="repeater-a"?9:-13;
        }
        revision.nodes.push_back({std::move(operation),id+"-input",id+"-output"});
    }
    MacroEndpoint cursor{"",revision.input.id};
    for(const auto& item:revision.nodes) {
        revision.edges.push_back({cursor,{item.operation.id,item.input_port}});
        cursor={item.operation.id,item.output_port};
    }
    revision.output_mapping=cursor;revision.edges.push_back({cursor,{"",revision.output.id}});
    revision.public_parameters={{amount_id,"Amount","offset-b","amount","number","du","local_paths_and_paint"}};
    if(advanced) {
        // The portable format must retain authored order; graph edges execute the chain.
        std::reverse(revision.nodes.begin(),revision.nodes.end());
        std::reverse(revision.edges.begin(),revision.edges.end());
    }
    MacroDefinition result;result.id="source-definition";result.label="Mapped Library Macro";
    result.revisions.emplace(1,std::move(revision));return result;
}
MacroDefinitionRevision mapped_revision(const MacroDefinitionRevision& initial,std::uint64_t number) {
    auto mapped=initial;mapped.revision=number;mapped.interface_version=2;
    mapped.public_parameters.push_back({copies_id,"Copies","repeater-a","copies","number","scalar","local_paths_and_paint"});
    const auto repeater=initial.graph_version==2?"repeater-b":"repeater-a";
    mapped.public_parameters.push_back({rotation_id,"Rotation",repeater,"rotation","number","degree","local_paths_and_paint"});
    return mapped;
}
Document fixture(const Id& id) {
    auto document=empty_document(id,"composition","artboard");
    Object path;path.id="path";path.name="Library Macro target";
    Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t index=0;index<anchors.size();++index) {
        Point point;point.id="point-"+std::to_string(index+1);
        point.x.literal=anchors[index].x;point.y.literal=anchors[index].y;contour.points.push_back(point);
    }
    path.contours.push_back(contour);path.stack.emplace_back(default_operation("fill","nect.paint.fill"));
    document.objects.emplace(path.id,path);document.compositions.front().roots.push_back(path.id);
    return document;
}
void import_macro(Session& target,const MacroDefinition& source,const LibraryMacroAssetV1& asset,
    const Id& definition_id,const Id& instance_id,std::uint64_t pin) {
    InstantiateMacro command{"path",definition_id,instance_id,pin,1};
    command.imported_definition=source;command.asset_id=asset.ref.asset_id.toStdString();
    command.accepted_asset_revision=asset.accepted_revision;
    command.overrides={{amount_id,15},{copies_id,3},{rotation_id,17}};
    apply(target,{MacroCommand{std::move(command)}});
}
void check_envelope(const QString& path,const LibraryMacroAssetV1& asset,const MacroDefinition& source,unsigned schema) {
    const auto bytes=read_bytes(path);QJsonParseError parse_error;
    const auto document=QJsonDocument::fromJson(bytes,&parse_error);
    check(parse_error.error==QJsonParseError::NoError&&document.isObject(),"Published envelope is a JSON object");
    const auto envelope=document.object();const auto payload=envelope.value("payload").toString().toUtf8();
    check(envelope.value("version").toInt()==1&&envelope.value("kind").toString()=="macro_definition"&&
        envelope.value("payload_schema").toInt()==static_cast<int>(schema),
        "Envelope version remains 1 while payload_schema selects the exact Macro format");
    check(asset.payload_schema==schema&&payload==payload_bytes(source,schema)&&
        envelope.value("sha256").toString()==sha256(payload)&&asset.sha256==sha256(payload),
        "Envelope and returned metadata hash the exact canonical payload bytes");
    check(envelope.value("asset_id").toString()==asset.ref.asset_id&&
        envelope.value("accepted_revision").toDouble()==static_cast<double>(asset.accepted_revision)&&
        envelope.value("label").toString()==QString::fromStdString(source.label)&&asset.label==QString::fromStdString(source.label),
        "Envelope and metadata preserve exact AssetID, accepted revision and source label");
    check(asset.ref.asset_id!=QString::fromStdString(source.id)&&
        QUuid(asset.ref.asset_id).toString(QUuid::WithoutBraces)==asset.ref.asset_id,
        "Publication assigns a canonical workspace AssetID distinct from the source DefinitionID");
}

void publication_import_and_update(const QString& scratch) {
    const auto root=scratch+"/assets";const auto settings_path=scratch+"/library.ini";
    QSettings settings(settings_path,QSettings::IniFormat);FolderLibrary library(settings,{}, {},root);
    Session author(fixture("author-document"));auto initial=definition(true);
    apply(author,{MacroCommand{CreateMacroDefinition{initial}}});
    apply(author,{MacroCommand{UpdateMacroDefinition{initial.id,mapped_revision(initial.revisions.at(1),2)}},
        MacroCommand{InstantiateMacro{"path",initial.id,"source-instance",2,1}}});
    apply(author,{MacroCommand{SetMacroOverride{"path","source-instance",copies_id,4}}});
    const auto source=author.document().macro_definitions.at(initial.id);
    const auto source_native=encode(author.document());const auto source_revision=author.revision();const auto source_history=author.history();
    check(source.latest_revision==2&&source.revisions.at(1).graph_version==2&&source.revisions.at(1).interface_version==1&&
        source.revisions.at(2).interface_version==2,"Authoring appends a mapped interface revision and retains the graph2/legacy-interface pin");
    check(library.macro_assets().isEmpty()&&!QFileInfo::exists(root),"Listing the injected empty asset store is read-only");
    const auto published=library.publish_macro_asset(source);const auto path=asset_path(root,published.ref);
    check(published.accepted_revision==1,"First publication has accepted asset revision 1");check_envelope(path,published,source,2);
    check(encode(author.document())==source_native&&author.revision()==source_revision&&author.history()==source_history,
        "Publishing a Macro does not change its source Document, revision, overrides or Undo history");
    const auto favorite=library.add_favorite(published.ref,1);
    check(library.favorite_status(favorite)=="Available"&&library.favorite_for_slot(1)->favorite_id==favorite.favorite_id,
        "Advanced Macro asset resolves through its exact Favorite and Quick Access slot");
    const auto roundtrip=FolderLibrary::target_from_json(FolderLibrary::target_to_json(favorite.target));
    check(std::get<MacroAssetRefV1>(roundtrip)==published.ref&&library.same_identity(favorite.target,roundtrip),
        "Favorite serialization preserves the workspace AssetID");
    QSettings reader_settings(settings_path,QSettings::IniFormat);FolderLibrary reader(reader_settings,{}, {},root);
    LibraryMacroAssetV1 metadata;const auto loaded=reader.read_macro_asset(published.ref,&metadata);
    check(loaded==source&&metadata.ref==published.ref&&metadata.sha256==published.sha256&&metadata.payload_schema==2,
        "Fresh FolderLibrary read preserves every retained graph, node order, port, edge and mapped PublicParamID");
    check(reader.favorite_for_slot(1)->favorite_id==favorite.favorite_id&&reader.favorite_status(favorite)=="Available",
        "Fresh settings reader resolves the same advanced Macro Favorite");
    const auto inventory=reader.macro_assets();
    check(inventory.size()==1&&inventory.front().ref==published.ref&&inventory.front().payload_schema==2&&inventory.front().available,
        "Advanced Macro inventory reports the same exact available asset and schema");
    Session imported(fixture("imported-document"));const auto before=imported.document();
    const auto before_revision=imported.revision();const auto before_history=imported.history().states.size();
    import_macro(imported,loaded,metadata,"imported-definition","imported-instance",2);
    auto expected=source;expected.id="imported-definition";
    check(imported.revision()==before_revision+1&&imported.history().states.size()==before_history+1&&
        imported.document().macro_definitions.at(expected.id)==expected,
        "Fresh definition and pinned instance import all retained graphs in one Session command and one Undo");
    check(expected.id!=source.id&&expected.id!=published.ref.asset_id.toStdString()&&
        imported.document().objects.at("path").stack.at(1).macro->pinned_revision==2&&
        macro_parameter_value(imported.document(),"path","imported-instance",amount_id)==15&&
        macro_parameter_value(imported.document(),"path","imported-instance",copies_id)==3&&
        macro_parameter_value(imported.document(),"path","imported-instance",rotation_id)==17,
        "Import uses fresh document identities and applies controls to the explicitly pinned interface2 graph");
    const auto imported_native=encode(imported.document());
    imported.undo(imported.revision());check(imported.document()==before,"One Undo removes the imported definition and instance together");
    imported.redo(imported.revision());check(encode(imported.document())==imported_native,"One Redo restores the exact imported snapshot");
    const auto first_bytes=read_bytes(path);auto third=expected.revisions.at(2);third.revision=3;
    node(third,"offset-b").operation.parameters.at("amount").literal=27;
    apply(imported,{MacroCommand{RenameMacroDefinition{expected.id,"Edited import only"}},
        MacroCommand{UpdateMacroDefinition{expected.id,third}},MacroCommand{UpdateMacroInstance{"path","imported-instance",3}},
        MacroCommand{SetMacroOverride{"path","imported-instance",copies_id,5}}});
    check(imported.document().macro_definitions.at(expected.id).label=="Edited import only"&&
        imported.document().macro_definitions.at(expected.id).latest_revision==3&&
        macro_parameter_value(imported.document(),"path","imported-instance",copies_id)==5,
        "Imported graph defaults, public override, label and revision remain independently editable");
    check(encode(author.document())==source_native&&author.revision()==source_revision&&author.history()==source_history&&
        read_bytes(path)==first_bytes&&reader.read_macro_asset(published.ref)==source,
        "Editing an imported Macro preserves the source Document and published asset byte-for-byte");
    const auto independent_native=encode(imported.document());
    auto asset_source=source;auto asset_third=source.revisions.at(2);asset_third.revision=3;
    node(asset_third,"offset-b").operation.parameters.at("amount").literal=31;
    asset_source.revisions.emplace(3,asset_third);asset_source.latest_revision=3;asset_source.label="Explicitly updated asset";
    const auto updated=library.update_macro_asset(published.ref,asset_source,published.accepted_revision,published.sha256);
    check(updated.ref==published.ref&&updated.accepted_revision==2&&updated.sha256!=published.sha256&&updated.payload_schema==2,
        "Explicit update retains AssetID, advances accepted asset revision and hashes the replacement schema2 payload");
    check_envelope(path,updated,asset_source,2);
    check(reader.read_macro_asset(published.ref)==asset_source&&encode(imported.document())==independent_native&&
        encode(author.document())==source_native&&library.favorite_for_slot(1)->favorite_id==favorite.favorite_id,
        "Updating a Library asset preserves earlier document snapshots and stable Favorite identity");
    const auto updated_bytes=read_bytes(path);const auto settings_bytes=settings.value("library/v1/state").toByteArray();
    rejects("MACRO_ASSET_REVISION_CONFLICT",[&]{(void)library.update_macro_asset(published.ref,source,1,updated.sha256);});
    rejects("MACRO_ASSET_REVISION_CONFLICT",[&]{(void)library.update_macro_asset(published.ref,source,2,published.sha256);});
    rejects("MACRO_ASSET_REVISION_CONFLICT",[&]{(void)library.update_macro_asset(published.ref,source,1,published.sha256);});
    check(read_bytes(path)==updated_bytes&&reader.read_macro_asset(published.ref)==asset_source&&
        settings.value("library/v1/state").toByteArray()==settings_bytes&&encode(imported.document())==independent_native,
        "Stale revision and stale hash guards independently reject without altering asset, Favorite state or imported document");
    FolderLibrary failed_write(settings,{}, {},root,[](const QString&,const QByteArray&,QString& detail) {
        detail="injected schema2 asset write refusal";return false;
    });
    rejects("MACRO_LIBRARY_WRITE_FAILED",[&]{(void)failed_write.update_macro_asset(updated.ref,source,2,updated.sha256);});
    check(read_bytes(path)==updated_bytes&&reader.read_macro_asset(updated.ref)==asset_source,
        "Schema2 update write failure atomically preserves the last accepted asset bytes");
    LibraryMacroAssetV1 latest_metadata;const auto latest=reader.read_macro_asset(updated.ref,&latest_metadata);
    Session future(fixture("future-document"));import_macro(future,latest,latest_metadata,"future-definition","future-instance",3);
    check(future.document().macro_definitions.at("future-definition").revisions.size()==3&&
        future.document().objects.at("path").stack.at(1).macro->pinned_revision==3&&latest_metadata.accepted_revision==2,
        "A future import explicitly pins definition revision 3 from accepted workspace asset revision 2");
}

void retained_revision_schema_selection(const QString& scratch) {
    QSettings settings(scratch+"/library.ini",QSettings::IniFormat);const auto root=scratch+"/assets";
    FolderLibrary library(settings,{}, {},root);
    auto graph_only=definition(true);auto latest_legacy=definition(false).revisions.at(1);latest_legacy.revision=2;
    graph_only.revisions.emplace(2,latest_legacy);graph_only.latest_revision=2;graph_only.label="Retained graph2 only";
    const auto graph_asset=library.publish_macro_asset(graph_only);
    check(graph_asset.payload_schema==2&&library.read_macro_asset(graph_asset.ref)==graph_only,
        "A retained graph2/interface1 revision requires schema2 even when the latest pin is fully legacy");
    check_envelope(asset_path(root,graph_asset.ref),graph_asset,graph_only,2);
    auto interface_only=definition(false);interface_only.id="interface-only-definition";interface_only.label="Retained interface2 only";
    interface_only.revisions.emplace(2,mapped_revision(interface_only.revisions.at(1),2));latest_legacy.revision=3;
    interface_only.revisions.emplace(3,latest_legacy);interface_only.latest_revision=3;
    const auto interface_asset=library.publish_macro_asset(interface_only);
    check(interface_asset.payload_schema==2&&library.read_macro_asset(interface_asset.ref)==interface_only,
        "A retained graph1/interface2 revision requires schema2 even when the latest pin is fully legacy");
    check_envelope(asset_path(root,interface_asset.ref),interface_asset,interface_only,2);
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(graph_only,1);});
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)canonical_macro_payload(interface_only,1);});
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)read_canonical_macro_payload(canonical_macro_payload(graph_only),1);});
    rejects("UNSUPPORTED_PORTABLE_MACRO_GRAPH",[&]{(void)read_canonical_macro_payload(canonical_macro_payload(interface_only),1);});
}

void tampering_remains_atomic(const QString& scratch) {
    QSettings settings(scratch+"/library.ini",QSettings::IniFormat);const auto root=scratch+"/assets";
    FolderLibrary library(settings,{}, {},root);auto source=definition(true);
    source.revisions.emplace(2,mapped_revision(source.revisions.at(1),2));source.latest_revision=2;
    const auto asset=library.publish_macro_asset(source);const auto favorite=library.add_favorite(asset.ref,1);
    auto neighbor_source=definition(false);neighbor_source.id="neighbor-definition";neighbor_source.label="Good legacy neighbor";
    const auto neighbor=library.publish_macro_asset(neighbor_source);const auto neighbor_path=asset_path(root,neighbor.ref);
    const auto neighbor_bytes=read_bytes(neighbor_path);const auto path=asset_path(root,asset.ref);
    const auto original=read_bytes(path);const auto original_envelope=QJsonDocument::fromJson(original).object();
    const auto preferences=settings.value("library/v1/state").toByteArray();
    const auto refuse=[&](const char* code,QJsonObject envelope) {
        const auto tampered=QJsonDocument(envelope).toJson(QJsonDocument::Compact);write_bytes(path,tampered);
        LibraryMacroAssetV1 sentinel{{"metadata-must-stay-untouched"},"unchanged",99,"unchanged-hash",99,false,"unchanged-problem"};
        rejects(code,[&]{(void)library.read_macro_asset(asset.ref,&sentinel);});
        rejects(code,[&]{(void)library.update_macro_asset(asset.ref,source,asset.accepted_revision,asset.sha256);});
        check(sentinel.ref.asset_id=="metadata-must-stay-untouched"&&sentinel.accepted_revision==99&&sentinel.sha256=="unchanged-hash"&&
            sentinel.payload_schema==99&&sentinel.label=="unchanged"&&!sentinel.available&&sentinel.problem=="unchanged-problem",
            "Failed read does not partially replace caller metadata");
        const auto inventory=library.macro_assets();const auto unavailable=std::find_if(inventory.begin(),inventory.end(),[&](const auto& candidate) {
            return candidate.ref==asset.ref;
        });
        const auto good=std::find_if(inventory.begin(),inventory.end(),[&](const auto& candidate) {return candidate.ref==neighbor.ref;});
        check(unavailable!=inventory.end()&&!unavailable->available&&unavailable->problem.contains(QString::fromLatin1(code))&&
            good!=inventory.end()&&good->available&&library.favorite_status(favorite).contains(QString::fromLatin1(code)),
            "Tampered asset remains unavailable at its exact identity while a good neighbor remains available");
        check(read_bytes(path)==tampered&&read_bytes(neighbor_path)==neighbor_bytes&&library.read_macro_asset(neighbor.ref)==neighbor_source&&
            settings.value("library/v1/state").toByteArray()==preferences&&library.favorite_for_slot(1)->favorite_id==favorite.favorite_id,
            "Rejected read, listing and update preserve tampered bytes, good neighbor bytes and all Favorite state");
        write_bytes(path,original);
        check(library.read_macro_asset(asset.ref)==source,"Restoring exact accepted bytes recovers the same schema2 asset");
    };
    auto altered=original_envelope;altered.insert("payload",altered.value("payload").toString()+" ");
    refuse("MACRO_ASSET_HASH_MISMATCH",altered);
    altered=original_envelope;altered.insert("payload_schema",1);
    refuse("UNSUPPORTED_PORTABLE_MACRO_GRAPH",altered);
    altered=original_envelope;altered.insert("label","Forged envelope label");
    refuse("MACRO_ASSET_ENVELOPE_MISMATCH",altered);
    altered=original_envelope;altered.insert("asset_id",QUuid::createUuid().toString(QUuid::WithoutBraces));
    refuse("MACRO_ASSET_ID_MISMATCH",altered);
    auto same_identity=source;same_identity.id=asset.ref.asset_id.toStdString();altered=original_envelope;
    replace_payload(altered,payload_bytes(same_identity));refuse("MACRO_ASSET_ID_MISMATCH",altered);
    altered=original_envelope;replace_payload(altered,payload_bytes(source)+"\n");
    refuse("NONCANONICAL_MACRO_PAYLOAD",altered);
    altered=original_envelope;altered.insert("payload_schema",4);
    refuse("UNSUPPORTED_MACRO_SCHEMA",altered);
    altered=original_envelope;altered.insert("version",2);
    refuse("UNSUPPORTED_MACRO_ASSET_VERSION",altered);
}

void legacy_schema1_bytes(const QString& scratch) {
    auto source=definition(false);
    const auto canonical=payload_bytes(source,1);
    // Frozen at the pre-change schema1 byte contract (Boost.JSON 1.85 canonical numbers).
    check(sha256(canonical)=="a1f9df6baa3381f287ba3bd40316d786261db97264e1d976b9597ab30dd2ce3b",
        "Legacy canonical schema1 payload bytes retain their exact frozen SHA-256");
    check(payload_bytes(source)==canonical&&read_canonical_macro_payload(canonical.toStdString(),1)==source&&
        !canonical.contains("graph_version")&&!canonical.contains("interface_version"),
        "Legacy canonical payload remains schema1 with no new graph or interface markers");
    QSettings settings(scratch+"/library.ini",QSettings::IniFormat);const auto root=scratch+"/assets";
    FolderLibrary library(settings,{}, {},root);const auto asset=library.publish_macro_asset(source);
    check(asset.payload_schema==1&&library.read_macro_asset(asset.ref)==source,"Legacy publication still auto-selects schema1");
    check_envelope(asset_path(root,asset.ref),asset,source,1);
    auto next=source.revisions.at(1);next.revision=2;node(next,"offset-b").operation.parameters.at("amount").literal=18;
    source.revisions.emplace(2,next);source.latest_revision=2;
    const auto updated=library.update_macro_asset(asset.ref,source,asset.accepted_revision,asset.sha256);
    check(updated.ref==asset.ref&&updated.accepted_revision==2&&updated.payload_schema==1&&library.read_macro_asset(asset.ref)==source,
        "Legacy-only retained revisions update under the same AssetID and still use schema1");
    check_envelope(asset_path(root,asset.ref),updated,source,1);
}
}

int main(int argc,char** argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        QTemporaryDir scratch;check(scratch.isValid(),"Create isolated Macro Library test directory");
        for(const auto* name:{"publication","retained","tampering","legacy"})
            check(QDir().mkpath(scratch.path()+"/"+name),"Create isolated test settings directory");
        publication_import_and_update(scratch.path()+"/publication");
        retained_revision_schema_selection(scratch.path()+"/retained");
        tampering_remains_atomic(scratch.path()+"/tampering");
        legacy_schema1_bytes(scratch.path()+"/legacy");
        std::cout<<"Macro Library schema2 contract: "<<checks<<" checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
    }
}
