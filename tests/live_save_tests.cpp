#include "host.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <atomic>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
template<class F> void until(F condition,const char* why,int timeout=5000) {
    QElapsedTimer elapsed;elapsed.start();
    while(!condition()&&elapsed.elapsed()<timeout)QTest::qWait(5);
    check(condition(),why);
}
template<class F> void rejects(const char* code,F action) {
    try {action();}catch(const Error& error){check(error.code==code,"Expected storage error code");return;}
    throw std::runtime_error("Expected storage rejection");
}
QByteArray bytes(const QString& path) {QFile f(path);check(f.open(QIODevice::ReadOnly),"Read persisted fixture");return f.readAll();}
QByteArray sha256(const QByteArray& value) {return QCryptographicHash::hash(value,QCryptographicHash::Sha256).toHex();}
void put(const QString& path,const QByteArray& value) {QFile f(path);check(f.open(QIODevice::WriteOnly),"Open external writer");check(f.write(value)==value.size(),"External write");}
void add(Host& host) {
    Point p;p.id="point";p.x.literal=1;p.y.literal=2;
    const auto comp=host.session.document().compositions.front().id;
    host.session.apply({CreatePath{comp,"","path","Test path",{{"contour",false,{p}}}}},host.session.revision());host.edited();
}
void set(Host& host,double value) {host.session.apply({Set{{"path","point","x"},value}},host.session.revision());host.edited();}
double x(const QString& path) {return evaluate(load_native(path).document).at({"path","point","x"});}
struct SlowWriter {
    std::promise<void> release;
    std::shared_future<void> gate=release.get_future().share();
    std::atomic<bool> entered=false;
    std::mutex mutex;
    std::vector<std::uint64_t> revisions;
    ProtectionResult operator()(ProtectionSnapshot snapshot) {
        {std::lock_guard lock(mutex);revisions.push_back(snapshot.revision);}
        if(snapshot.revision==1&&!entered.exchange(true)) {
            if(gate.wait_for(std::chrono::seconds(8))!=std::future_status::ready)
                throw std::runtime_error("Slow storage gate timed out");
        }
        return protect_snapshot(std::move(snapshot));
    }
};
void coalescing_and_conflict(const QString& directory) {
    auto slow=std::make_shared<SlowWriter>();
    Host host(directory+"/recovery",nullptr,[slow](auto snapshot){return (*slow)(std::move(snapshot));});
    const auto path=directory+"/live.nect";host.save(path);add(host);
    until([&]{return slow->entered.load();},"Background writer starts at the one-second cadence");
    int events=0;QTimer heartbeat;heartbeat.setInterval(5);QObject::connect(&heartbeat,&QTimer::timeout,[&]{++events;});heartbeat.start();
    for(int i=2;i<=12;++i)set(host,i);
    host.session.begin_gesture(12);host.session.update_gesture({Set{{"path","point","x"},999}});
    QTest::qWait(1150);
    auto status=host.persistence();
    check(events>50,"Event loop continues while storage is deliberately blocked");
    check(status["writing_revision"].toInteger()==1&&status["pending_revision"].toInteger()==12&&
          status["saved_revision"].toInteger()==0&&status["recovery_revision"].isNull(),
          "Pending and writing revisions never claim durable success");
    auto hello=QJsonDocument::fromJson(host.dispatch("{\"op\":\"hello\"}")).object();
    check(hello["ok"].toBool()&&hello["revision"].toInteger()==12,"Live API remains usable during slow storage and gesture preview");
    slow->release.set_value();
    until([&]{return host.persistence()["saved_revision"].toInteger(-1)==12&&host.persistence()["recovery_revision"].toInteger(-1)==12;},
          "Newest committed revision follows the first job");
    {std::lock_guard lock(slow->mutex);check(slow->revisions==std::vector<std::uint64_t>{1,12},"Queue coalesces intermediate edits into only first and newest snapshots");}
    check(x(path)==12&&x(host.persistence()["recovery_file"].toString())==12,"Disk contains committed values, never the active preview");
    check(host.session.gesture_active(),"Background completion does not cancel the live gesture");host.session.cancel_gesture();
    check(!host.dirty()&&host.save_status.startsWith("Saved"),"Saved status names only the matching durable revision");
    auto external=load_native(path).document;external.objects.at("path").contours.front().points.front().x.literal=777;
    const auto external_bytes=QByteArray::fromStdString(encode(external));put(path,external_bytes);set(host,13);
    until([&]{return host.persistence()["recovery_revision"].toInteger(-1)==13;},"Recovery continues through native source conflict");
    status=host.persistence();
    check(status["native_error"].toObject()["code"]=="FILE_CHANGED"&&status["saved_revision"].toInteger()==12&&
          bytes(path)==external_bytes&&host.dirty(),"External native content is preserved, with explicit conflict and protected latest work");
    rejects("FILE_CHANGED",[&]{host.save(path);});
#ifdef _WIN32
    rejects("FILE_CHANGED",[&]{host.save(path.toUpper());});
    check(bytes(path)==external_bytes&&same_native_path(path,path.toUpper()),"Case aliases cannot bypass source conflict protection on Windows");
#endif
    const auto copy=directory+"/separate.nect";host.save(copy);host.recover();
    check(x(copy)==13&&bytes(path)==external_bytes&&!host.dirty()&&host.persistence()["native_error"].isNull(),
          "Save As resolves the conflict without overwriting the externally modified original");
    const auto meta=QJsonDocument::fromJson(bytes(directory+"/recovery/"+host.session_id+".recovery.json")).object();
    check(meta["source_file"]==native_path(copy),"Recovery receipt follows an explicit Save As target");
    const auto protected_file=host.persistence()["recovery_file"].toString();const auto protected_bytes=bytes(protected_file);
    host.open_recovery(protected_file);check(host.file_path.isEmpty()&&host.dirty(),"Opening recovery creates an unnamed document");
    set(host,19);host.recover();check(bytes(protected_file)==protected_bytes,"Recovered work does not live-save over its recovery source");
}
void typed_source_save_as(const QString& directory) {
    Host host(directory+"/typed-recovery");
    const auto composition=host.session.document().compositions.front().id;
    auto source=default_text("save-as-source-text","Source");source.layout="auto";
    source.parameters.at("font_size").literal=28;
    auto target=default_text("save-as-target-text","Target text in a fixed frame");target.layout="frame";
    target.parameters.at("frame_width").literal=96;target.parameters.at("frame_height").literal=48;
    const Ref source_layout{"save-as-source","","text.layout"},target_layout{"save-as-target","","text.layout"};
    host.session.apply({
        CreateText{composition,"","save-as-source","Source",source},
        CreateText{composition,"","save-as-target","Target",target}},host.session.revision());
    host.edited();
    host.session.apply({LinkTextLayout{target_layout,source_layout,false}},host.session.revision());host.edited();
    const auto source_path=directory+"/typed-source.nect";
    host.save(source_path);host.recover();
    const auto saved_revision=host.session.revision();
    check(host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(saved_revision)&&
          host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(saved_revision),
          "Typed source fixture is saved and protected before the external conflict");
    const auto initial_source=load_native(source_path).document;
    check(initial_source.objects.contains("save-as-source")&&initial_source.objects.contains("save-as-target")&&
          initial_source.objects.at("save-as-source").text->id=="save-as-source-text"&&
          initial_source.objects.at("save-as-target").text->id=="save-as-target-text",
          "Native source starts with both stable Text object and source IDs");
    const auto initial_layout=text_layout_property(initial_source,target_layout);
    check(initial_layout.literal=="frame"&&initial_layout.driver&&initial_layout.driver->link==source_layout&&
          initial_layout.evaluated=="auto"&&
          initial_source.objects.at("save-as-target").text->parameters.at("frame_width").literal==96&&
          initial_source.objects.at("save-as-target").text->parameters.at("frame_height").literal==48&&
          initial_source.objects.at("save-as-source").text->parameters.at("font_size").literal==28,
          "Native source retains target frame literals, typed layout Ref and a separate authored Scalar");

    auto external=initial_source;external.objects.at("save-as-source").text->content="External edit";
    const auto external_bytes=QByteArray::fromStdString(encode(external));put(source_path,external_bytes);
    const auto external_hash=sha256(external_bytes);
    const Ref source_size{"save-as-source","","text.font_size"};
    host.session.apply({Set{source_size,29}},host.session.revision());host.edited();
    const auto committed_revision=host.session.revision();
    const auto committed_document=host.session.document();
    const auto committed_bytes=QByteArray::fromStdString(encode(committed_document));
    until([&]{const auto state=host.persistence();return state["native_error"].toObject()["code"]=="FILE_CHANGED"&&
        state["recovery_revision"].toInteger(-1)==static_cast<qint64>(committed_revision);},
        "External source conflict is recorded while the typed latest revision reaches recovery");
    const auto recovery_path=host.persistence()["recovery_file"].toString();
    const auto recovery_meta=directory+"/typed-recovery/"+host.session_id+".recovery.json";
    const auto protected_bytes=bytes(recovery_path),protected_meta_bytes=bytes(recovery_meta);
    check(protected_bytes==committed_bytes&&
          QJsonDocument::fromJson(protected_meta_bytes).object()["source_file"]==native_path(source_path),
          "Protected conflict recovery contains the committed authored state and original source binding");
    check(bytes(source_path)==external_bytes&&sha256(bytes(source_path))==external_hash,
          "External original bytes and SHA-256 survive conflict detection");

    const auto failed_destination=directory+"/missing-save-as-parent/failed.nect";
    rejects("IO_ERROR",[&]{host.save(failed_destination);});
    check(host.file_path==native_path(source_path)&&host.session.revision()==committed_revision&&
          host.session.document()==committed_document&&host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(saved_revision)&&
          host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(committed_revision)&&
          !QFile::exists(failed_destination),
          "Failed Save As leaves source binding, saved revision, live authored state and destination unchanged");
    check(bytes(source_path)==external_bytes&&sha256(bytes(source_path))==external_hash&&
          bytes(recovery_path)==protected_bytes&&bytes(recovery_meta)==protected_meta_bytes,
          "Failed Save As preserves the external original and protected recovery bytes");

    const auto destination=directory+"/typed-save-as.nect";
    host.save(destination);
    check(host.file_path==native_path(destination)&&!host.dirty()&&
          host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(committed_revision)&&
          host.persistence()["native_error"].isNull()&&bytes(destination)==committed_bytes&&
          bytes(destination).contains(QByteArray::fromStdString(std::string("\"version\":\"")+native_version+"\"")),
          "Valid Save As binds the committed revision only after exact destination readback");
    const auto saved=load_native(destination).document;
    const auto saved_layout=text_layout_property(saved,target_layout);
    check(saved.objects.contains("save-as-source")&&saved.objects.contains("save-as-target")&&
          saved.objects.at("save-as-source").text->id=="save-as-source-text"&&
          saved.objects.at("save-as-target").text->id=="save-as-target-text"&&
          saved_layout.literal=="frame"&&saved_layout.driver&&saved_layout.driver->link==source_layout&&
          saved_layout.evaluated=="auto"&&
          saved.objects.at("save-as-target").text->parameters.at("frame_width").literal==96&&
          saved.objects.at("save-as-target").text->parameters.at("frame_height").literal==48&&
          saved.objects.at("save-as-source").text->parameters.at("font_size").literal==29,
          "Destination readback retains stable IDs, exact authored layout and Scalar values, and evaluates the link");
    host.recover();
    check(host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(committed_revision)&&
          bytes(recovery_path)==committed_bytes&&
          QJsonDocument::fromJson(bytes(recovery_meta)).object()["source_file"]==native_path(destination),
          "Successful Save As readback moves protected recovery provenance to the destination");
    check(bytes(source_path)==external_bytes&&sha256(bytes(source_path))==external_hash,
          "Valid Save As leaves externally changed original bytes and hash untouched");

    Host reopened(directory+"/cold-recovery");reopened.open(destination);
    const auto cold_layout=text_layout_property(reopened.session.document(),target_layout);
    check(reopened.session.document()==committed_document&&reopened.session.revision()==0&&
          reopened.file_path==native_path(destination)&&cold_layout.literal=="frame"&&cold_layout.driver&&
          cold_layout.driver->link==source_layout&&cold_layout.evaluated=="auto"&&
          reopened.session.document().objects.at("save-as-target").text->parameters.at("frame_width").literal==96&&
          reopened.session.document().objects.at("save-as-target").text->parameters.at("frame_height").literal==48,
          "Cold reopen from destination restores native 0.23 authored sources and stable layout link");
}
void point_edit_save_as(const QString& directory) {
    Host host(directory+"/point-edit-recovery");
    const auto composition=host.session.document().compositions.front().id;
    const Id object_id="save-as-circle",source_id="save-as-circle-source";
    const Ref east_x{object_id,source_id+"-east","x"};
    const Ref east_handle{object_id,source_id+"-east","out.length"};
    const Ref enabled{object_id,"","point_edit.enabled"};
    host.session.apply({CreatePrimitive{composition,"",object_id,"Circle",default_primitive(source_id,"nect.shape.circle")}},
                       host.session.revision());host.edited();
    const auto generated_x=evaluate(host.session.document()).at(east_x);
    const auto generated_handle=evaluate(host.session.document()).at(east_handle);
    const auto corrected_x=generated_x+24;
    const auto formula="ref(\"save-as-circle\",\"\",\"generator.radius\")*0.5";
    host.session.apply({Set{east_x,corrected_x},SetExpression{{east_handle},{formula,1},false},
        EnablePointEdit{object_id,false}},host.session.revision());host.edited();
    const auto committed=host.session.document();
    const auto correction=*committed.objects.at(object_id).point_edit;
    check(!correction.enabled&&correction.overrides.at(east_x.point).at("x").literal==corrected_x&&
          correction.overrides.at(east_handle.point).at(east_handle.field).expression->source==formula&&
          evaluate(committed).at(east_x)==generated_x&&evaluate(committed).at(east_handle)==generated_handle&&
          !point_edit_enabled_property(committed,enabled),
          "Disabled correction retains literal and expression overrides while evaluating generator fallback");
    const auto original=directory+"/point-edit-source.nect";
    host.save(original);host.recover();
    const auto original_bytes=bytes(original),committed_bytes=QByteArray::fromStdString(encode(committed));
    const auto revision=host.session.revision();
    check(original_bytes==committed_bytes&&host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(revision),
          "Original native file durably stores the committed disabled correction");

    const auto invalid=directory+"/missing-point-edit-parent/failed.nect";
    rejects("IO_ERROR",[&]{host.save(invalid);});
    check(!QFile::exists(invalid)&&host.file_path==native_path(original)&&host.session.revision()==revision&&
          host.session.document()==committed&&host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(revision)&&
          bytes(original)==original_bytes,
          "Failed correction Save As leaves binding, revision, authored Document and original bytes unchanged");

    const auto destination=directory+"/point-edit-destination.nect";
    host.save(destination);host.recover();
    const auto destination_bytes=bytes(destination);
    const auto saved=load_native(destination).document;
    const auto& saved_object=saved.objects.at(object_id);
    check(host.file_path==native_path(destination)&&!host.dirty()&&destination_bytes==committed_bytes&&
          bytes(original)==original_bytes&&saved==committed&&saved_object.source->id==source_id&&
          saved_object.point_edit->id==correction.id&&
          saved_object.point_edit->overrides.at(east_x.point).at("x").literal==corrected_x&&
          saved_object.point_edit->overrides.at(east_handle.point).at(east_handle.field).expression->source==formula&&
          !point_edit_enabled_property(saved,enabled)&&evaluate(saved).at(east_x)==generated_x&&
          evaluate(saved).at(east_handle)==generated_handle,
          "Save As preserves source, correction identity, literal/expression overrides, bypass and generated fallback");
    const auto meta_path=directory+"/point-edit-recovery/"+host.session_id+".recovery.json";
    check(QJsonDocument::fromJson(bytes(meta_path)).object()["source_file"]==native_path(destination)&&
          host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(revision),
          "Recovery provenance and revision follow the correction destination");

    Host reopened(directory+"/point-edit-cold-recovery");reopened.open(destination);
    check(reopened.session.document()==committed&&reopened.session.revision()==0&&
          evaluate(reopened.session.document()).at(east_x)==generated_x,
          "Cold destination reopen retains authored correction and generator fallback");
    reopened.session.apply({EnablePointEdit{object_id,true}},reopened.session.revision());
    check(evaluate(reopened.session.document()).at(east_x)==corrected_x&&
          evaluate(reopened.session.document()).at(east_handle)==
              evaluate(reopened.session.document()).at({object_id,"","generator.radius"})*0.5&&
          reopened.session.document().objects.at(object_id).point_edit->id==correction.id&&
          bytes(destination)==destination_bytes,
          "Re-enable after cold reopen restores literal and expression point overrides without changing saved bytes");
}
void artboard_size_save_as(const QString& directory) {
    Host host(directory+"/artboard-size-recovery");
    const auto composition=host.session.document().compositions.front().id;
    const auto parent=host.session.document().compositions.front().artboards.front().id;
    const auto source_size=evaluate_artboard(host.session.document().compositions.front(),parent);
    const Id child="save-as-artboard-child";
    Artboard board{child,"Dependent crop",900,0,240,180};
    board.parent_size=ArtboardParent{parent,true,false};
    host.session.apply({AddArtboard{composition,board,1}},host.session.revision());host.edited();
    const Ref width{child,"","artboard.width"},height{child,"","artboard.height"};
    const Ref parent_width{parent,"","artboard.width"};
    const auto formula="ref(\""+parent+"\",\"\",\"artboard.height\") * 2";
    host.session.apply({SetArtboardSizeExpression{width,{formula,1},true},
        LinkArtboardSize{height,parent_width,false}},host.session.revision());host.edited();
    const auto committed=host.session.document();
    check(artboard_size_property(committed,width).evaluated==source_size.height*2&&
          artboard_size_property(committed,height).evaluated==source_size.width,
          "Typed Artboard expression and cross-field link evaluate before Save As");
    const auto original=directory+"/artboard-size-source.nect";
    host.save(original);host.recover();
    const auto original_bytes=bytes(original),encoded=QByteArray::fromStdString(encode(committed));
    check(original_bytes==encoded,"Original native Artboard source matches committed authorship");
    const auto revision=host.session.revision();
    const auto invalid=directory+"/missing-artboard-size-parent/failed.nect";
    rejects("IO_ERROR",[&]{host.save(invalid);});
    check(!QFile::exists(invalid)&&host.file_path==native_path(original)&&
          host.session.revision()==revision&&host.session.document()==committed&&bytes(original)==original_bytes,
          "Failed Artboard Save As preserves source file and live typed drivers");
    const auto destination=directory+"/artboard-size-destination.nect";
    host.save(destination);host.recover();
    const auto saved=load_native(destination).document;
    const auto& stored=saved.compositions.front().artboards.at(1);
    check(bytes(destination)==encoded&&bytes(original)==original_bytes&&saved==committed&&
          stored.id==child&&stored.parent_size&&stored.parent_size->artboard==parent&&
          !stored.parent_size->width&&stored.width==240&&stored.height==180&&
          stored.width_driver&&std::holds_alternative<Expression>(stored.width_driver->value)&&
          std::get<Expression>(stored.width_driver->value).source==formula&&
          stored.height_driver&&std::holds_alternative<Ref>(stored.height_driver->value)&&
          std::get<Ref>(stored.height_driver->value)==parent_width,
          "Save As retains Artboard identity, fallback literals, parent metadata and exact typed sources");
    Host reopened(directory+"/artboard-size-cold-recovery");reopened.open(destination);
    check(reopened.session.document()==committed&&reopened.session.revision()==0&&
          artboard_size_property(reopened.session.document(),width).evaluated==source_size.height*2&&
          artboard_size_property(reopened.session.document(),height).evaluated==source_size.width,
          "Cold destination reopen restores both Artboard dependency evaluations");
    auto source=reopened.session.document().compositions.front().artboards.front();
    source.height=500;source.width=600;
    reopened.session.apply({UpdateArtboard{composition,source}},reopened.session.revision());
    check(artboard_size_property(reopened.session.document(),width).evaluated==1000&&
          artboard_size_property(reopened.session.document(),height).evaluated==600,
          "Cold-reopened Artboard sources keep evaluating after their parent dimensions change");
}
void linked_point_edit_save_as(const QString& directory) {
    Host host(directory+"/linked-point-edit-recovery");
    const auto composition=host.session.document().compositions.front().id;
    const Id source_object="linked-save-source",source_generator="linked-save-source-generator";
    const Id target_object="linked-save-target",target_generator="linked-save-target-generator";
    const Ref source_point{source_object,source_generator+"-east","x"};
    const Ref target_point{target_object,target_generator+"-east","x"};
    host.session.apply({CreatePrimitive{composition,"",source_object,"Source",default_primitive(source_generator,"nect.shape.circle")},
        CreatePrimitive{composition,"",target_object,"Target",default_primitive(target_generator,"nect.shape.circle")}},
        host.session.revision());host.edited();
    const auto fallback=evaluate(host.session.document()).at(target_point);
    const auto target_override=fallback+32;
    host.session.apply({Set{source_point,380},Set{target_point,target_override},EnablePointEdit{target_object,false}},
        host.session.revision());host.edited();
    const auto source_ref=point_edit_enabled_ref(source_object,source_generator+"-point-edit");
    const auto target_ref=point_edit_enabled_ref(target_object,target_generator+"-point-edit");
    host.session.apply({LinkPointEditEnabled{target_ref,source_ref,false}},host.session.revision());host.edited();
    const auto initial=host.session.document();
    check(point_edit_enabled_state(initial,target_ref).driver==source_ref&&
          !point_edit_enabled_state(initial,target_ref).literal&&
          point_edit_enabled_state(initial,target_ref).evaluated&&
          evaluate(initial).at(target_point)==target_override,
          "Linked correction evaluates the target override while preserving its false authored literal");
    const auto original=directory+"/linked-point-edit-source.nect";
    host.save(original);host.recover();
    const auto original_bytes=bytes(original);
    check(original_bytes==QByteArray::fromStdString(encode(initial)),
          "Original linked correction bytes match the committed Document");

    host.session.apply({EnablePointEdit{source_object,false}},host.session.revision());host.edited();
    const auto committed=host.session.document();
    const auto committed_revision=host.session.revision();
    const auto saved_revision=host.persistence()["saved_revision"].toInteger(-1);
    const auto state=point_edit_enabled_state(committed,target_ref);
    check(!state.literal&&state.driver==source_ref&&!state.evaluated&&
          evaluate(committed).at(target_point)==fallback&&host.dirty(),
          "Disabling the source retains the target driver and override while selecting generator fallback");
    const auto invalid=directory+"/missing-linked-point-edit-parent/failed.nect";
    rejects("IO_ERROR",[&]{host.save(invalid);});
    check(!QFile::exists(invalid)&&host.file_path==native_path(original)&&host.dirty()&&
          host.session.revision()==committed_revision&&host.session.document()==committed&&
          host.persistence()["saved_revision"].toInteger(-1)==saved_revision&&bytes(original)==original_bytes,
          "Failed linked correction Save As preserves binding, dirty state, revisions and authored bytes");

    const auto destination=directory+"/linked-point-edit-destination.nect";
    host.save(destination);host.recover();
    const auto destination_bytes=bytes(destination);
    const auto saved=load_native(destination).document;
    const auto& saved_source=saved.objects.at(source_object);
    const auto& saved_target=saved.objects.at(target_object);
    const auto saved_state=point_edit_enabled_state(saved,target_ref);
    check(host.file_path==native_path(destination)&&!host.dirty()&&saved==committed&&
          destination_bytes==QByteArray::fromStdString(encode(committed))&&bytes(original)==original_bytes&&
          saved_source.source->id==source_generator&&saved_target.source->id==target_generator&&
          saved_source.point_edit->id==source_generator+"-point-edit"&&
          saved_target.point_edit->id==target_generator+"-point-edit"&&
          saved_target.point_edit->overrides.at(target_point.point).at("x").literal==target_override&&
          !saved_state.literal&&saved_state.driver==source_ref&&!saved_state.evaluated&&
          evaluate(saved).at(target_point)==fallback,
          "Save As retains both correction IDs, exact driver, false target literal and generator fallback");
    const auto meta_path=directory+"/linked-point-edit-recovery/"+host.session_id+".recovery.json";
    check(QJsonDocument::fromJson(bytes(meta_path)).object()["source_file"]==native_path(destination)&&
          host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(committed_revision),
          "Linked correction recovery provenance points to the Save As destination");

    Host reopened(directory+"/linked-point-edit-cold-recovery");reopened.open(destination);
    check(reopened.session.document()==committed&&reopened.session.revision()==0&&
          point_edit_enabled_state(reopened.session.document(),target_ref).driver==source_ref&&
          !point_edit_enabled_state(reopened.session.document(),target_ref).evaluated&&
          evaluate(reopened.session.document()).at(target_point)==fallback,
          "Cold destination reopen preserves the exact linked correction and fallback");
    reopened.session.apply({EnablePointEdit{source_object,true}},reopened.session.revision());
    check(point_edit_enabled_state(reopened.session.document(),target_ref).evaluated&&
          evaluate(reopened.session.document()).at(target_point)==target_override&&
          reopened.session.document().objects.at(target_object).point_edit->id==target_generator+"-point-edit"&&
          bytes(destination)==destination_bytes,
          "Re-enabling the cold-opened source restores the target override without changing destination bytes");
}
void linked_mask_save_as(const QString& directory) {
    Host host(directory+"/linked-mask-recovery");
    const auto composition=host.session.document().compositions.front().id;
    const Id target_owner="linked-mask-save-target-owner",target_geometry="linked-mask-save-target-geometry";
    const Id source_owner="linked-mask-save-source-owner",source_geometry="linked-mask-save-source-geometry";
    const Id target_mask_id="linked-mask-save-target-mask",source_mask_id="linked-mask-save-source-mask";
    const auto rectangle=[](const Id& id,double left,double top,double right,double bottom) {
        const double corners[4][2]={{left,top},{right,top},{right,bottom},{left,bottom}};
        std::vector<Point> points(4);
        for(std::size_t i=0;i<points.size();++i) {
            points[i].id=id+"-point-"+std::to_string(i);
            points[i].x.literal=corners[i][0];points[i].y.literal=corners[i][1];
        }
        return Contour{id+"-contour",true,std::move(points)};
    };
    host.session.apply({
        CreatePath{composition,"",target_owner,"Target owner",{rectangle(target_owner,0,0,100,100)}},
        CreatePath{composition,"",target_geometry,"Target mask geometry",{rectangle(target_geometry,0,0,55,100)}},
        CreatePath{composition,"",source_owner,"Source owner",{rectangle(source_owner,120,0,220,100)}},
        CreatePath{composition,"",source_geometry,"Source mask geometry",{rectangle(source_geometry,120,0,175,100)}}
    },host.session.revision());host.edited();
    host.session.apply({
        SetMask{target_owner,GeometryMask{target_mask_id,target_geometry,1,false,"nonzero"}},
        SetMask{source_owner,GeometryMask{source_mask_id,source_geometry,1,true,"nonzero"}}
    },host.session.revision());host.edited();
    const auto target_ref=geometry_mask_enabled_ref(target_owner,target_mask_id);
    const auto source_ref=geometry_mask_enabled_ref(source_owner,source_mask_id);
    host.session.apply({LinkMaskEnabled{target_ref,source_ref,false}},host.session.revision());host.edited();
    const auto initial=host.session.document();
    const auto mask_state=geometry_mask_enabled_state(initial,target_ref);
    const auto scene_uses_mask=[](const Document& document,const Id& composition_id,const Id& owner,const Id& source) {
        const auto values=evaluate(document);
        const auto evaluated=evaluate_scene(document,composition_id,values,evaluate_transforms(document,values));
        for(const auto& root:evaluated.roots)
            if(root.id==owner)return root.mask&&root.mask->source==source&&!root.mask->paths.empty();
        return false;
    };
    check(initial.objects.at(target_owner).compositing.mask->id==target_mask_id&&
          initial.objects.at(target_owner).compositing.mask->source==target_geometry&&
          initial.objects.at(source_owner).compositing.mask->id==source_mask_id&&
          initial.objects.at(source_owner).compositing.mask->source==source_geometry&&
          !mask_state.literal&&mask_state.driver==source_ref&&mask_state.evaluated&&
          scene_uses_mask(initial,composition,target_owner,target_geometry),
          "False target literal follows the exact true source mask and resolves its clipping geometry");

    const auto original=directory+"/linked-mask-source.nect";
    host.save(original);host.recover();
    const auto original_bytes=bytes(original);
    check(original_bytes==QByteArray::fromStdString(encode(initial)),
          "Original native file stores the linked masks before source bypass");

    auto disabled_source=*initial.objects.at(source_owner).compositing.mask;disabled_source.enabled=false;
    host.session.apply({SetMask{source_owner,disabled_source}},host.session.revision());host.edited();
    const auto committed=host.session.document();
    const auto committed_revision=host.session.revision();
    const auto saved_revision=host.persistence()["saved_revision"].toInteger(-1);
    const auto committed_bytes=QByteArray::fromStdString(encode(committed));
    const auto target_state=geometry_mask_enabled_state(committed,target_ref);
    check(!target_state.literal&&target_state.driver==source_ref&&!target_state.evaluated&&
          committed.objects.at(target_owner).compositing.mask->id==target_mask_id&&
          committed.objects.at(target_owner).compositing.mask->source==target_geometry&&
          !committed.objects.at(source_owner).compositing.mask->enabled&&
          committed.objects.at(source_owner).compositing.mask->id==source_mask_id&&
          committed.objects.at(source_owner).compositing.mask->source==source_geometry&&
          !scene_uses_mask(committed,composition,target_owner,target_geometry),
          "Disabling the source bypasses clipping without changing target literal, mask IDs or geometry sources");
    const auto recovery_meta=directory+"/linked-mask-recovery/"+host.session_id+".recovery.json";

    const auto invalid=directory+"/missing-linked-mask-parent/failed.nect";
    rejects("IO_ERROR",[&]{host.save(invalid);});
    check(!QFile::exists(invalid)&&host.file_path==native_path(original)&&host.dirty()&&
          host.session.revision()==committed_revision&&host.session.document()==committed&&
          host.persistence()["saved_revision"].toInteger(-1)==saved_revision&&
          bytes(original)==original_bytes,
          "Failed linked-mask Save As preserves binding, revisions, authored state and original bytes");

    const auto destination=directory+"/linked-mask-destination.nect";
    host.save(destination);host.recover();
    const auto destination_bytes=bytes(destination);
    const auto saved=load_native(destination).document;
    const auto saved_state=geometry_mask_enabled_state(saved,target_ref);
    check(host.file_path==native_path(destination)&&!host.dirty()&&
          host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(committed_revision)&&
          saved==committed&&destination_bytes==committed_bytes&&
          saved.objects.at(target_owner).compositing.mask->id==target_mask_id&&
          saved.objects.at(target_owner).compositing.mask->source==target_geometry&&
          saved.objects.at(source_owner).compositing.mask->id==source_mask_id&&
          saved.objects.at(source_owner).compositing.mask->source==source_geometry&&
          !saved_state.literal&&saved_state.driver==source_ref&&!saved_state.evaluated&&
          bytes(original)==original_bytes,
          "Save As persists the committed Document, both mask IDs, geometry sources and exact disabled link");
    check(host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(committed_revision)&&
          QJsonDocument::fromJson(bytes(recovery_meta)).object()["source_file"]==native_path(destination),
          "Recovery provenance follows the linked-mask Save As destination");

    Host reopened(directory+"/linked-mask-cold-recovery");reopened.open(destination);
    const auto cold=reopened.session.document();
    const auto cold_state=geometry_mask_enabled_state(cold,target_ref);
    check(cold==committed&&reopened.session.revision()==0&&reopened.file_path==native_path(destination)&&
          cold.objects.at(target_owner).compositing.mask->id==target_mask_id&&
          cold.objects.at(target_owner).compositing.mask->source==target_geometry&&
          cold.objects.at(source_owner).compositing.mask->id==source_mask_id&&
          cold.objects.at(source_owner).compositing.mask->source==source_geometry&&
          !cold_state.literal&&cold_state.driver==source_ref&&!cold_state.evaluated&&
          !scene_uses_mask(cold,composition,target_owner,target_geometry),
          "Cold Host reopen restores both exact mask identities and the evaluated bypass");
    const auto cold_revision=reopened.session.revision();
    rejects("MISSING_MASK",[&]{reopened.session.apply({SetMask{source_owner,std::nullopt}},cold_revision);});
    rejects("MISSING_MASK",[&]{reopened.session.apply({SetMask{source_owner,
        GeometryMask{"linked-mask-save-replacement-mask",source_geometry,1,true,"nonzero"}}},cold_revision);});
    check(reopened.session.revision()==cold_revision&&reopened.session.document()==cold&&
          geometry_mask_enabled_state(reopened.session.document(),target_ref).driver==source_ref,
          "Missing or replaced source mask fails explicitly without owner-slot retargeting");

    auto enabled_source=*reopened.session.document().objects.at(source_owner).compositing.mask;
    enabled_source.enabled=true;
    reopened.session.apply({SetMask{source_owner,enabled_source}},reopened.session.revision());
    const auto reenabled=reopened.session.document();
    const auto reenabled_state=geometry_mask_enabled_state(reenabled,target_ref);
    check(!reenabled_state.literal&&reenabled_state.driver==source_ref&&reenabled_state.evaluated&&
          scene_uses_mask(reenabled,composition,target_owner,target_geometry)&&bytes(destination)==destination_bytes,
          "Re-enabling the cold-opened source restores clipping without changing saved destination bytes");
}
void independent_failures(const QString& directory) {
    const auto blocked=directory+"/blocked-recovery";put(blocked,"not a directory");
    Host host(blocked);const auto path=directory+"/protected-native.nect";host.save(path);add(host);
    until([&]{return host.persistence()["saved_revision"].toInteger(-1)==1&&!host.persistence()["recovery_error"].isNull();},
          "Native save succeeds independently of a failed recovery destination");
    check(x(path)==1&&!host.dirty()&&host.persistence()["recovery_revision"].isNull(),"Only the successful destination advances its known revision");
    rejects("IO_ERROR",[&]{host.recover();});
    host.flush();check(x(path)==1,"Verified native save permits orderly close even if recovery storage is unavailable");
    check(QFile::remove(blocked),"Remove owned recovery blocker");host.recover();
    check(host.persistence()["recovery_revision"].toInteger(-1)==1&&host.persistence()["recovery_error"].isNull(),"Explicit retry protects the committed state after storage repair");
}
void identity_drain(const QString& directory) {
    auto slow=std::make_shared<SlowWriter>();Host host(directory+"/identity-recovery",nullptr,[slow](auto snapshot){return (*slow)(std::move(snapshot));});
    const auto path=directory+"/outgoing.nect";host.save(path);add(host);
    until([&]{return slow->entered.load();},"Identity test has one active worker");set(host,2);
    const auto old_session=host.session_id;const auto old_recovery=host.persistence()["recovery_file"].toString();
    auto unblock=std::async(std::launch::async,[slow]{std::this_thread::sleep_for(std::chrono::milliseconds(100));slow->release.set_value();});
    host.create_document();unblock.get();
    check(x(path)==2&&x(old_recovery)==2,"New waits for old writer and protects the latest outgoing committed state");
    check(host.session_id!=old_session&&host.session.revision()==0&&host.persistence()["saved_revision"].isNull()&&
          host.persistence()["recovery_revision"].isNull(),"Old receipts cannot label the new Session saved or protected");
    host.recover();check(host.persistence()["recovery_revision"].toInteger(-1)==0,"Fresh Session has its own recovery receipt");
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        QTemporaryDir temp;check(temp.isValid(),"Create owned live-save test folder");
        coalescing_and_conflict(temp.path());typed_source_save_as(temp.path());point_edit_save_as(temp.path());artboard_size_save_as(temp.path());
        linked_point_edit_save_as(temp.path());linked_mask_save_as(temp.path());independent_failures(temp.path());identity_drain(temp.path());
        std::cout<<"PASS asynchronous snapshots, typed, Point Edit and mask Save As preservation, failure atomicity, conflict recovery and Session drain\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
