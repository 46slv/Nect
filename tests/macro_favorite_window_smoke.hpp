#pragma once

#include "window.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace macro_favorite_closed {
using namespace nect;
using namespace nect::desktop;
inline int checks=0;
inline void check(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
    ++checks;
}
inline QByteArray bytes(const QString& path) {
    QFile file(path);check(file.open(QIODevice::ReadOnly),"Read owned exact bytes");
    auto result=file.readAll();check(file.error()==QFileDevice::NoError,"Complete owned byte read");return result;
}
inline MacroDefinition definition() {
    auto operation=default_operation("published-offset","nect.shape.offset");
    operation.parameters.at("amount").literal=12;
    MacroDefinitionRevision graph;graph.graph_version=2;
    graph.input={"published-input","local_paths_and_paint"};
    graph.output={"published-output","local_paths_and_paint"};
    graph.nodes={{operation,"offset-input","offset-output"}};
    graph.edges={{{"","published-input"},{"published-offset","offset-input"}},
        {{"published-offset","offset-output"},{"","published-output"}}};
    graph.output_mapping={"published-offset","offset-output"};
    graph.public_parameters={{"macro.offset.amount","Amount","published-offset","amount","number","du","local_paths_and_paint"}};
    MacroDefinition result;result.id="closed-source-definition";result.label="Identical display name";
    result.revisions.emplace(1,graph);graph.revision=2;graph.nodes.front().operation.parameters.at("amount").literal=18;
    result.revisions.emplace(2,graph);result.latest_revision=2;return result;
}
inline Document document(bool source) {
    auto result=empty_document(source?"closed-source-document":"fresh-target-document","closed-composition","closed-artboard");
    Object path;path.id="compatible-target";path.name="Fresh compatible target";
    Contour contour;contour.id="retained-contour";contour.closed=true;
    const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t i=0;i<anchors.size();++i){Point point;point.id="retained-point-"+std::to_string(i);
        point.x.literal=anchors[i].x;point.y.literal=anchors[i].y;contour.points.push_back(point);}
    path.contours={contour};result.objects.emplace(path.id,path);result.compositions.front().roots={path.id};
    auto local=definition();
    if(!source)for(auto& [revision,graph]:local.revisions){(void)revision;graph.nodes.front().operation.parameters.at("amount").literal=999;}
    result.macro_definitions.emplace(local.id,local);return result;
}
inline Document applied(const Document& before,const Id& imported_id,const Id& instance_id) {
    auto result=before;auto imported=definition();imported.id=imported_id;
    result.macro_definitions.emplace(imported_id,imported);
    ProcessingEntry entry;entry.id=instance_id;entry.type=macro_entry_type;
    entry.macro=MacroInstance{imported_id,2,{},{}};
    result.objects.at("compatible-target").stack.push_back(entry);return result;
}
inline void native_valid(const Document& value) {
    const auto encoded=encode(value);check(decode(encoded)==value,"Independent entire Document native roundtrip before Window");
    Session accepted(value);check(accepted.document()==value,"Independent entire Document accepted by Session before Window");
}
template<class T>inline T* named(QObject& parent,const char* name) {
    auto* result=parent.findChild<T*>(name);check(result!=nullptr,name);return result;
}
inline void state(const Session& actual,const Session& reference,const Document& expected,const Id& imported_id) {
    check(actual.document()==expected&&encode(actual.document())==encode(expected),"Entire authored Document equals independent oracle");
    auto expected_history=reference.history();
    for(auto& row:expected_history.states)
        if(row.label=="Apply Macro: 00000000-0000-4000-8000-000000000001")row.label="Apply Macro: "+imported_id;
    const auto actual_history=actual.history();
    if(actual_history!=expected_history){
        std::cerr<<"History actual bytes="<<actual_history.retained_bytes<<" expected="<<expected_history.retained_bytes<<'\n';
        for(const auto& row:actual_history.states)std::cerr<<"Actual "<<row.id<<' '<<row.label<<' '<<row.estimated_bytes<<'\n';
        for(const auto& row:expected_history.states)std::cerr<<"Expected "<<row.id<<' '<<row.label<<' '<<row.estimated_bytes<<'\n';
    }
    check(actual.revision()==reference.revision()&&actual_history==expected_history,"Entire revision and History equal independent canonical oracle");
    check(actual.can_undo()==reference.can_undo()&&actual.can_redo()==reference.can_redo()&&
        actual.gesture_active()==reference.gesture_active()&&actual.gesture_generation()==reference.gesture_generation(),
        "Undo/Redo and gesture state equal independent oracle");
}
inline int child(const QString& scratch,const QString& asset_id,const QString& favorite_id) {
    check(QApplication::platformName()=="windows","Actual native Windows platform");
    const auto before=document(false);const auto source=definition();
    const Id oracle_definition="00000000-0000-4000-8000-000000000001";
    const Id oracle_instance="00000000-0000-4000-8000-000000000002";
    const auto expected_template=applied(before,oracle_definition,oracle_instance);
    native_valid(document(true));native_valid(before);native_valid(expected_template);
    QSettings settings(scratch+"/library.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);
    FolderLibrary loaded(settings,{}, {},scratch+"/assets");LibraryMacroAssetV1 metadata;
    const auto payload=loaded.read_macro_asset({asset_id},&metadata);
    const auto favorite=loaded.favorite_for_slot(4);
    check(payload==source&&metadata.ref.asset_id==asset_id&&metadata.accepted_revision==1&&metadata.payload_schema==2,
        "Cold workspace exact AssetID/revision retains every published graph revision");
    check(metadata.sha256==QString::fromLatin1(QCryptographicHash::hash(
        QByteArray::fromStdString(canonical_macro_payload(source,2)),QCryptographicHash::Sha256).toHex()),
        "Cold metadata hash equals independent canonical payload");
    check(favorite&&favorite->favorite_id==favorite_id&&favorite->quick_slot==4&&
        std::get<MacroAssetRefV1>(favorite->target).asset_id==asset_id&&loaded.favorite_status(*favorite)=="Available",
        "Cold shared Favorite and slot retain exact workspace target");
    const auto preferences=settings.value("library/v1/state").toByteArray();
    const auto settings_bytes=bytes(settings.fileName());
    const auto asset_path=scratch+"/assets/"+asset_id+".macro.json";
    const auto asset_bytes=bytes(asset_path);const auto source_bytes=bytes(scratch+"/source.nect");
    check(decode(source_bytes.toStdString())==document(true),"Closed source native file equals independent source oracle");
    Session reference(before);
    InstantiateMacro import{"compatible-target",oracle_definition,oracle_instance,2,0};
    import.imported_definition=read_canonical_macro_payload(canonical_macro_payload(source,2),2);
    import.asset_id=asset_id.toStdString();import.accepted_asset_revision=1;
    reference.apply({StructuralCommand{MacroCommand{std::move(import)}}},reference.revision());
    check(reference.document()==expected_template,"Canonical import equals manually authored entire oracle before Window");
    check(reference.revision()==1&&reference.history().states.size()==2,"Canonical import is one revision and one Undo before Window");
    // Only the generated fresh UUIDs are rebound after UI execution. Graphs,
    // geometry, target, pin, defaults, existing local definition and history
    // are independently fixed and native-validated before constructing Window.
    Window window(scratch+"/target-recovery",std::make_unique<FolderLibrary>(settings,
        FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},scratch+"/assets"));
    window.host.session=Session(before);window.host.edited();window.refresh();
    window.canvas->set_selection("compatible-target");window.show();QApplication::processEvents();
    const auto session_id=window.host.session_id;const auto neutral_history=window.host.session.history();
    std::exception_ptr failure;bool entered=false;QString applied_message;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("folder-library-dialog");
        if(!dialog){failure=std::make_exception_ptr(std::runtime_error("Missing production Library dialog"));window.close();return;}
        try {
            entered=true;
            check(window.host.session.document()==before&&window.host.session.history()==neutral_history&&window.host.session.revision()==0,
                "Opening production Library is authored-state neutral");
            auto* assets=named<QComboBox>(*dialog,"folder-library-macro-assets");
            const auto index=assets->findData(asset_id);check(index>0,"Exact published AssetID is reachable after source closure");
            check(assets->itemData(index,Qt::UserRole+1).toULongLong()==1&&assets->itemData(index,Qt::UserRole+2).toString()==metadata.sha256,
                "Production selector snapshots exact accepted revision/hash");
            check(assets->currentData().toString().isEmpty(),"Quick Access starts without an asset selector or pin selection");
            auto* local=named<QComboBox>(*dialog,"folder-library-source-macros");
            check(local->findData(QString::fromStdString(source.id))>0,
                "New document has a same-ID same-label local decoy, not the closed source");
            auto* slot=named<QComboBox>(*dialog,"folder-library-slot");slot->setCurrentIndex(slot->findData(4));
            check(window.host.session.document()==before&&window.host.session.history()==neutral_history,
                "Choosing Quick slot is authored-state neutral");
            auto* invoke=named<QPushButton>(*dialog,"folder-library-use-slot");check(invoke->isEnabled(),"Production Quick Access enabled");
            QTest::mouseClick(invoke,Qt::LeftButton);QApplication::processEvents();
            applied_message=named<QLabel>(*dialog,"folder-library-status")->text();
        }catch(...){failure=std::current_exception();}
        dialog->accept();
    });
    named<QAction>(window,"folder-library")->trigger();QApplication::processEvents();
    if(failure)std::rethrow_exception(failure);check(entered,"Entered actual production Window Library");
    check(applied_message=="Imported and applied Macro Favorite "+asset_id+" at retained revision 2 in one Undo step.",
        "Exact existing production success message identifies applied asset and latest retained pin");
    const auto& stack=window.host.session.document().objects.at("compatible-target").stack;
    check(stack.size()==1&&stack.front().macro.has_value(),"Quick Access imports exactly one Macro instance");
    const auto instance_id=stack.front().id;const auto imported_id=stack.front().macro->definition;
    check(instance_id.size()==oracle_instance.size()&&imported_id.size()==oracle_definition.size()&&
        imported_id!=source.id&&imported_id!=asset_id.toStdString()&&instance_id!=imported_id&&
        before.macro_definitions.count(imported_id)==0,"Fresh UUIDs do not alias source, workspace asset, decoy or each other");
    const auto expected=applied(before,imported_id,instance_id);state(window.host.session,reference,expected,imported_id);
    check(window.host.session_id==session_id&&window.canvas->selected_object=="compatible-target", "Invocation retains Session and selected target identity");
    const auto values=evaluate(expected);
    const auto bounds=object_bounds(expected,"compatible-target",values,evaluate_transforms(expected,values));
    check(bounds&&std::abs(bounds->left+18)<1e-7&&std::abs(bounds->top+18)<1e-7&&std::abs(bounds->right-118)<1e-7&&std::abs(bounds->bottom-78)<1e-7,
        "Whole oracle evaluates fixed Offset18 bounds, never decoy999");
    window.host.session.undo(window.host.session.revision());reference.undo(reference.revision());window.host.edited();
    state(window.host.session,reference,before,imported_id);
    window.host.session.redo(window.host.session.revision());reference.redo(reference.revision());window.host.edited();
    state(window.host.session,reference,expected,imported_id);
    window.host.save(scratch+"/applied.nect");
    {Host cold(scratch+"/cold-recovery");cold.open(scratch+"/applied.nect");
        check(cold.session.document()==expected&&encode(cold.session.document())==encode(expected),"Fresh native Host preserves whole imported Document");cold.flush();}
    settings.sync();QSettings persisted(settings.fileName(),QSettings::IniFormat);persisted.setFallbacksEnabled(false);persisted.sync();
    check(persisted.value("library/v1/state").toByteArray()==preferences&&bytes(settings.fileName())==settings_bytes,
        "Quick Access, Undo, Redo and native save leave all Favorite/settings bytes unchanged");
    check(bytes(asset_path)==asset_bytes&&bytes(scratch+"/source.nect")==source_bytes&&loaded.read_macro_asset({asset_id})==source,
        "Entire published envelope/payload and closed source remain byte-identical");
    window.close();window.host.flush();
    std::cout<<"PASS native Windows closed-source Macro Quick Access: "<<checks<<" checks\n";return 0;
}
inline int run() {
    check(QApplication::platformName()=="windows","Parent uses native Windows platform");
    QTemporaryDir scratch;check(scratch.isValid(),"Owned closed-source scratch");
    const auto source_doc=document(true);native_valid(source_doc);native_valid(document(false));
    native_valid(applied(document(false),"00000000-0000-4000-8000-000000000001","00000000-0000-4000-8000-000000000002"));
    QSettings settings(scratch.path()+"/library.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);
    FolderLibrary library(settings,{}, {},scratch.path()+"/assets");LibraryMacroAssetV1 asset;LibraryFavoriteV1 favorite;
    QPointer<Host> source_pointer;
    {Host source(scratch.path()+"/source-recovery");source_pointer=&source;source.session=Session(source_doc);
        source.save(scratch.path()+"/source.nect");const auto history=source.session.history();
        asset=library.publish_macro_asset(source.session.document().macro_definitions.at(definition().id));
        favorite=library.add_favorite(asset.ref,4);
        check(source.session.document()==source_doc&&source.session.history()==history&&source.session.revision()==0,
            "Publication/setup is preference-only and preserves complete source Session");source.flush();}
    check(source_pointer.isNull(),"Source Host and its Session are destroyed before cold process starts");
    QProcess restart;restart.start(QCoreApplication::applicationFilePath(),
        {"--macro-favorite-source-closed-child",scratch.path(),asset.ref.asset_id,favorite.favorite_id});
    check(restart.waitForStarted(10000),"Start cold native Window process after source closure");
    const bool finished=restart.waitForFinished(45000);
    if(!finished){restart.kill();restart.waitForFinished(5000);}
    std::cout<<restart.readAllStandardOutput().toStdString();std::cerr<<restart.readAllStandardError().toStdString();
    check(finished&&restart.exitStatus()==QProcess::NormalExit&&restart.exitCode()==0,"Cold native Window Quick Access accepted independent oracles");
    std::cout<<"PASS source closure/restart setup: "<<checks<<" checks\n";return 0;
}
}
