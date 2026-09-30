#include "host.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QProcess>
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
void text_weight_expression_save_as(const QString& directory,const QString& nect_cli) {
    Host host(directory+"/weight-expression-recovery");
    const auto composition=host.session.document().compositions.front().id;
    auto source=default_text("weight-source-text","A");source.weight=300;
    auto target=default_text("weight-target-text","B");target.weight=400;
    auto relative_target=default_text("weight-relative-text","C");relative_target.weight=500;
    host.session.apply({CreateText{composition,"","weight-source","Weight A",source},
        CreateText{composition,"","weight-target","Weight B",target},
        CreateText{composition,"","weight-relative-target","Weight C",relative_target}},host.session.revision());host.edited();
    const auto original=directory+"/weight-expression-original.nect";
    host.save(original);const auto original_bytes=bytes(original);
    const Expression expression{"ref(\"weight-source\",\"\",\"text.weight\") + 100",1};
    host.session.apply({SetTextWeightExpression{{"weight-target","","text.weight"},expression,false},
        LinkTextWeight::batch_command(TextWeightBatch{TextWeightBatchMode::link,
            {Ref{"weight-relative-target","","text.weight"}},0,Ref{"weight-source","","text.weight"},true,false})},
        host.session.revision());host.edited();
    auto revised=*host.session.document().objects.at("weight-source").text;revised.weight=500;
    host.session.apply({UpdateText{"weight-source",revised}},host.session.revision());host.edited();
    const auto committed=host.session.document();
    const auto destination=directory+"/weight-expression-destination.nect";
    host.save(destination);const auto destination_bytes=bytes(destination);
    check(bytes(original)==original_bytes&&destination_bytes==QByteArray::fromStdString(encode(committed))&&
        destination_bytes.contains("\"version\":\"0.63\"")&&
        destination_bytes.contains("\"offset\":200")&&
        load_native(destination).document.objects.at("weight-target").text->weight_expression==expression&&
        load_native(destination).document.objects.at("weight-relative-target").text->weight_driver==
            TextWeightDriver{Ref{"weight-source","","text.weight"},200},
        "Host Save As retains the exact Text weight expression and relative offset without changing original bytes");
    Host reopened(directory+"/weight-expression-cold-recovery");reopened.open(destination);
    const Ref weight_ref{"weight-target","","text.weight"};
    const auto cold=text_weight_property(reopened.session.document(),weight_ref);
    check(reopened.session.document()==committed&&reopened.session.revision()==0&&
        cold.literal==400&&cold.expression==expression&&cold.evaluated==600,
        "Cold Host reopen preserves Text weight literal, exact expression and evaluated integer");
    const Ref relative_weight_ref{"weight-relative-target","","text.weight"};
    const auto cold_relative=text_weight_property(reopened.session.document(),relative_weight_ref);
    check(cold_relative.literal==500&&cold_relative.driver==TextWeightDriver{Ref{"weight-source","","text.weight"},200}&&
        cold_relative.evaluated==700,
        "Cold Host reopen preserves the authored Text weight literal, exact source and signed offset");
    QProcess process;process.start(nect_cli,{"--serve",destination});
    check(process.waitForStarted(5000),"Start a separate Nect process on Text weight Save As destination");
    const auto query=QByteArray("{\"op\":\"get\",\"ref\":{\"object\":\"weight-target\",\"point\":\"\",\"field\":\"text.weight\"}}\n"
        "{\"op\":\"get\",\"ref\":{\"object\":\"weight-relative-target\",\"point\":\"\",\"field\":\"text.weight\"}}\n");
    check(process.write(query)==query.size(),"Query Text weight from the cold Nect process");
    process.closeWriteChannel();
    check(process.waitForFinished(10000)&&process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0,
        "Cold Nect process exits after Text weight readback");
    const auto replies=process.readAllStandardOutput().trimmed().split('\n');
    const auto reply=QJsonDocument::fromJson(replies.front()).object()["result"].toObject();
    check(reply["authored"].toObject()["literal"].toInt()==400&&
        reply["authored"].toObject()["expression"].toObject()["source"].toString()==QString::fromStdString(expression.source)&&
        reply["authored"].toObject()["source_kind"]=="expression"&&
        reply["evaluated"].toInt()==600,
        "Separate process reads exact Text weight source and integer result from Save As destination");
    const auto relative_reply=QJsonDocument::fromJson(replies.back()).object()["result"].toObject();
    check(relative_reply["authored"].toObject()["literal"].toInt()==500&&
        relative_reply["authored"].toObject()["driver"].toObject()["link"].toObject()["object"]=="weight-source"&&
        relative_reply["authored"].toObject()["driver"].toObject()["offset"].toInt()==200&&
        relative_reply["authored"].toObject()["source_kind"]=="link"&&relative_reply["evaluated"].toInt()==700,
        "Separate process reads the exact relative Text weight source, signed offset and evaluated value");
}
void object_visibility_expression_save_as(const QString& directory,const QString& nect_cli) {
    Host host(directory+"/visibility-expression-recovery");
    const auto composition=host.session.document().compositions.front().id;
    host.session.apply({
        CreatePrimitive{composition,"","visibility-source","Source",default_primitive("visibility-source-primitive","nect.shape.rectangle")},
        CreatePrimitive{composition,"","visibility-target","Target",default_primitive("visibility-target-primitive","nect.shape.rectangle")}},
        host.session.revision());host.edited();
    const auto original=directory+"/visibility-expression-original.nect";
    host.save(original);const auto original_bytes=bytes(original);
    const Expression expression{" ! ref ( \"visibility-source\" , \"\" , \"object.visible\" ) ",1};
    host.session.apply({SetObjectVisibilityExpression{{"visibility-target","","object.visible"},expression,false},
        SetVisibility{"visibility-source",false}},host.session.revision());host.edited();
    const auto committed=host.session.document();
    const auto destination=directory+"/visibility-expression-destination.nect";
    host.save(destination);const auto destination_bytes=bytes(destination);
    const Ref target{"visibility-target","","object.visible"};
    const auto saved=load_native(destination).document;
    const auto saved_state=object_visibility_state(saved,target);
    check(bytes(original)==original_bytes&&destination_bytes==QByteArray::fromStdString(encode(committed))&&
        destination_bytes.contains("\"version\":\"0.63\"")&&saved==committed&&
        saved_state.literal&&!saved_state.driver&&saved_state.expression==expression&&saved_state.evaluated,
        "Host Save As keeps the exact Object visibility expression and leaves original bytes unchanged");
    Host reopened(directory+"/visibility-expression-cold-recovery");reopened.open(destination);
    const auto cold=object_visibility_state(reopened.session.document(),target);
    check(reopened.session.document()==committed&&reopened.session.revision()==0&&
        cold.literal&&!cold.driver&&cold.expression==expression&&cold.evaluated&&
        bytes(original)==original_bytes,
        "Cold Host reopen restores authored literal, exact expression source and evaluated own visibility");
    QProcess process;process.start(nect_cli,{"--serve",destination});
    check(process.waitForStarted(5000),"Start a separate Nect process on Object visibility expression Save As destination");
    const auto query=QByteArray("{\"op\":\"get\",\"ref\":{\"object\":\"visibility-target\",\"point\":\"\",\"field\":\"object.visible\"}}\n");
    check(process.write(query)==query.size(),"Query Object visibility expression from the cold Nect process");
    process.closeWriteChannel();
    check(process.waitForFinished(10000)&&process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0,
        "Cold Nect process exits after Object visibility expression readback");
    const auto reply=QJsonDocument::fromJson(process.readAllStandardOutput().trimmed()).object()["result"].toObject();
    const auto authored=reply["authored"].toObject();
    check(authored["literal"].toBool()&&authored["driver"].isNull()&&
        authored["expression"].toObject()["source"].toString()==QString::fromStdString(expression.source)&&
        authored["expression"].toObject()["version"].toInt()==1&&authored["source_kind"]=="expression"&&
        reply["evaluated"].toBool()&&reply["expression"].toBool()&&reply["link"].toBool(),
        "Separate process reads exact Object visibility source, source kind and evaluated value");
}
void composite_isolation_expression_save_as(const QString& directory,const QString& nect_cli) {
    Host host(directory+"/composite-isolation-expression-recovery");
    const auto composition=host.session.document().compositions.front().id;
    host.session.apply({
        CreatePrimitive{composition,"","composite-isolation-source","Source",default_primitive("composite-isolation-source-primitive","nect.shape.rectangle")},
        CreatePrimitive{composition,"","composite-isolation-target","Target",default_primitive("composite-isolation-target-primitive","nect.shape.rectangle")}},
        host.session.revision());host.edited();
    const auto original=directory+"/composite-isolation-expression-original.nect";
    host.save(original);const auto original_bytes=bytes(original);
    const Expression expression{" ! ref ( \"composite-isolation-source\" , \"\" , \"composite.isolated\" ) ",1};
    const Ref target{"composite-isolation-target","","composite.isolated"};
    host.session.apply({SetCompositeIsolatedExpression{target,expression,false}},host.session.revision());host.edited();
    const auto committed=host.session.document();
    const auto destination=directory+"/composite-isolation-expression-destination.nect";
    host.save(destination);const auto destination_bytes=bytes(destination);
    const auto saved=load_native(destination).document;
    const auto saved_state=composite_isolation_state(saved,target);
    check(bytes(original)==original_bytes&&destination_bytes==QByteArray::fromStdString(encode(committed))&&
        destination_bytes.contains("\"version\":\"0.63\"")&&saved==committed&&
        saved_state.literal==false&&!saved_state.driver&&saved_state.expression==expression&&saved_state.evaluated&&
        saved.objects.at("composite-isolation-target").compositing.isolated_expression==expression,
        "Host Save As keeps exact Composite isolation expression and leaves original bytes unchanged");
    Host reopened(directory+"/composite-isolation-expression-cold-recovery");reopened.open(destination);
    const auto cold=composite_isolation_state(reopened.session.document(),target);
    check(reopened.session.document()==committed&&reopened.session.revision()==0&&
        cold.literal==false&&!cold.driver&&cold.expression==expression&&cold.evaluated&&
        bytes(original)==original_bytes,
        "Cold Host reopen restores authored isolation, exact expression and evaluated value");
    QProcess process;process.start(nect_cli,{"--serve",destination});
    check(process.waitForStarted(5000),"Start a separate Nect process on Composite isolation expression Save As destination");
    const auto query=QByteArray("{\"op\":\"get\",\"ref\":{\"object\":\"composite-isolation-target\",\"point\":\"\",\"field\":\"composite.isolated\"}}\n");
    check(process.write(query)==query.size(),"Query Composite isolation expression from the cold Nect process");
    process.closeWriteChannel();
    check(process.waitForFinished(10000)&&process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0,
        "Cold Nect process exits after Composite isolation expression readback");
    const auto reply=QJsonDocument::fromJson(process.readAllStandardOutput().trimmed()).object()["result"].toObject();
    const auto authored=reply["authored"].toObject();
    check(authored["literal"].toBool()==false&&authored["driver"].isNull()&&
        authored["expression"].toObject()["source"].toString()==QString::fromStdString(expression.source)&&
        authored["expression"].toObject()["version"].toInt()==1&&authored["source_kind"]=="expression"&&
        reply["evaluated"].toBool()&&reply["expression"].toBool()&&reply["link"].toBool(),
        "Separate process reads exact Composite isolation source, source kind and evaluated value");
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
void layout_save_as(const QString& directory,const QString& nect_cli) {
    Host host(directory+"/layout-save-recovery");
    const auto composition=host.session.document().compositions.front().id;
    const auto artboard_id=host.session.document().compositions.front().artboards.front().id;
    const Id grid_id="save-as-layout-grid";
    const Id grid_columns_source_board_id="save-as-grid-columns-source-board";
    const Id grid_columns_source_grid_id="save-as-grid-columns-source-grid";
    const Ref grid_columns_ref{grid_id,"","grid.columns"};
    const Ref grid_columns_source_ref{grid_columns_source_grid_id,"","grid.columns"};
    const Id grid_columns_expression_board_id="save-as-grid-columns-expression-board";
    const Id grid_columns_expression_grid_id="save-as-grid-columns-expression-grid";
    const Ref grid_columns_expression_ref{grid_columns_expression_grid_id,"","grid.columns"};
    const Ref grid_rows_expression_ref{grid_columns_expression_grid_id,"","grid.rows"};
    const Ref grid_rows_ref{grid_id,"","grid.rows"};
    const Ref grid_rows_source_ref{grid_columns_source_grid_id,"","grid.rows"};
    const Id expression_board_id="save-as-column-expression-board",expression_grid_id="save-as-column-expression-grid";
    const Ref grid_column_gutter_ref{grid_id,"","grid.column_gutter"};
    const Ref grid_column_gutter_source_ref{"save-as-grid-column-gutter-source","","artboard.width"};
    const Ref expression_grid_column_gutter_ref{expression_grid_id,"","grid.column_gutter"};
    const Ref grid_row_gutter_ref{grid_id,"","grid.row_gutter"};
    ArtboardLayout initial_layout{
        Margin{12,16,20,24},Grid{grid_id,{32,28,560,400},4,3,16,12}};
    const Ref grid_height_ref{grid_id,"","grid.bounds.height"};
    const Expression grid_height_expression{R"(ref("save-as-grid-height-source","","artboard.height") + 10)",1};
    const Expression grid_row_gutter_expression{R"(ref("save-as-grid-column-gutter-source","","artboard.height"))",1};
    const Expression column_gutter_expression{R"(ref("save-as-grid-column-gutter-source","","artboard.width") + 10)",1};
    const Expression grid_columns_expression{R"(ref("save-as-grid-columns-source-grid","","grid.columns") + 1)",1};
    const Expression grid_rows_expression{R"(ref("save-as-grid-columns-source-grid","","grid.rows") + 1)",1};
    Artboard height_source{"save-as-grid-height-source","Grid height source",0,0,100,410};
    Artboard gutter_source{"save-as-grid-column-gutter-source","Grid column gutter source",0,0,10,20};
    Artboard expression_board{expression_board_id,"Column expression frame",0,0,400,300};
    ArtboardLayout expression_layout{std::nullopt,Grid{expression_grid_id,{20,30,320,240},3,2,12,10}};
    Artboard columns_source{grid_columns_source_board_id,"Grid columns source",0,0,100,100};
    ArtboardLayout columns_source_layout;columns_source_layout.grid=
        Grid{grid_columns_source_grid_id,{0,0,100,100},3,4,0,0};columns_source.layout=columns_source_layout;
    Artboard grid_columns_expression_board{grid_columns_expression_board_id,"Grid columns expression frame",0,0,400,300};
    ArtboardLayout grid_columns_expression_layout;
    grid_columns_expression_layout.grid=Grid{grid_columns_expression_grid_id,{0,0,400,300},2,2,0,0};
    host.session.apply({AddArtboard{composition,height_source,1},AddArtboard{composition,gutter_source,2},
        AddArtboard{composition,expression_board,3},AddArtboard{composition,columns_source,4},
        AddArtboard{composition,grid_columns_expression_board,5},
        SetArtboardLayout{composition,artboard_id,initial_layout},
        SetArtboardLayout{composition,expression_board_id,expression_layout},
        SetArtboardLayout{composition,grid_columns_expression_board_id,grid_columns_expression_layout}},host.session.revision());
    host.session.apply({GridColumnsCommand{LinkGridColumns{grid_columns_ref,grid_columns_source_ref,false}}},
        host.session.revision());
    host.session.apply({GridColumnsCommand{SetGridColumnsExpression{grid_columns_expression_ref,
        grid_columns_expression,false}}},host.session.revision());
    host.session.apply({GridRowsCommand{SetGridRowsExpression{grid_rows_expression_ref,
        grid_rows_expression,false}}},host.session.revision());
    host.session.apply({GridRowsCommand{LinkGridRows{grid_rows_ref,grid_rows_source_ref,false}}},host.session.revision());
    host.session.apply({GridBoundsHeightCommand{SetGridBoundsHeightExpression{grid_height_ref,grid_height_expression,false}}},
        host.session.revision());
    host.session.apply({GridColumnGutterCommand{LinkGridColumnGutter{grid_column_gutter_ref,
        grid_column_gutter_source_ref,false}}},host.session.revision());
    host.session.apply({GridRowGutterCommand{SetGridRowGutterExpression{grid_row_gutter_ref,
        grid_row_gutter_expression,false}}},host.session.revision());
    host.session.apply({GridColumnGutterCommand{SetGridColumnGutterExpression{expression_grid_column_gutter_ref,
        column_gutter_expression,false}}},host.session.revision());
    initial_layout.grid->bounds_height_expression=grid_height_expression;
    initial_layout.grid->column_gutter_driver=grid_column_gutter_source_ref;
    initial_layout.grid->row_gutter_expression=grid_row_gutter_expression;
    initial_layout.grid->columns_driver=grid_columns_source_ref;
    initial_layout.grid->rows_driver=grid_rows_source_ref;
    expression_layout.grid->column_gutter_expression=column_gutter_expression;
    grid_columns_expression_layout.grid->columns_expression=grid_columns_expression;
    grid_columns_expression_layout.grid->rows_expression=grid_rows_expression;
    host.edited();
    const auto initial=host.session.document();
    const auto source_path=directory+"/layout-save-source.nect";
    host.save(source_path);host.recover();
    const auto saved_revision=host.session.revision();
    const auto initial_bytes=bytes(source_path);
    const auto initial_columns_expression_board=std::find_if(initial.compositions.front().artboards.begin(),
        initial.compositions.front().artboards.end(),[&](const Artboard& value){
            return value.id==grid_columns_expression_board_id;
        });
    check(initial.compositions.front().artboards.front().id==artboard_id&&
          initial.compositions.front().artboards.front().layout&&
          *initial.compositions.front().artboards.front().layout==initial_layout&&
          initial.compositions.front().artboards[3].id==expression_board_id&&
          initial.compositions.front().artboards[3].layout&&
          *initial.compositions.front().artboards[3].layout==expression_layout&&
          initial_columns_expression_board!=initial.compositions.front().artboards.end()&&
          initial_columns_expression_board->layout&&*initial_columns_expression_board->layout==grid_columns_expression_layout&&
          host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(saved_revision)&&
          host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(saved_revision)&&
          initial_bytes==QByteArray::fromStdString(encode(initial)),
          "Initial native source saves Margin and Grid under the stable Artboard and Grid IDs");

    auto external=load_native(source_path).document;
    external.compositions.front().artboards.front().name="External Artboard change";
    const auto external_bytes=QByteArray::fromStdString(encode(external));
    put(source_path,external_bytes);
    const auto external_hash=sha256(external_bytes);

    ArtboardLayout committed_layout{
        Margin{18,22,26,30},Grid{grid_id,{40,36,520,400},4,3,16,12}};
    host.session.apply({SetArtboardLayout{composition,artboard_id,committed_layout}},host.session.revision());host.edited();
    committed_layout.grid->bounds_height_expression=grid_height_expression;
    committed_layout.grid->column_gutter_driver=grid_column_gutter_source_ref;
    committed_layout.grid->row_gutter_expression=grid_row_gutter_expression;
    committed_layout.grid->columns_driver=grid_columns_source_ref;
    committed_layout.grid->rows_driver=grid_rows_source_ref;
    auto committed_expression_layout=expression_layout;
    const auto committed=host.session.document();
    const auto committed_revision=host.session.revision();
    const auto committed_bytes=QByteArray::fromStdString(encode(committed));
    until([&]{const auto state=host.persistence();return state["native_error"].toObject()["code"]=="FILE_CHANGED"&&
        state["recovery_revision"].toInteger(-1)==static_cast<qint64>(committed_revision);},
        "External source conflict is recorded while the revised layout reaches recovery");
    const auto recovery_path=host.persistence()["recovery_file"].toString();
    const auto recovery_meta=directory+"/layout-save-recovery/"+host.session_id+".recovery.json";
    const auto protected_bytes=bytes(recovery_path);
    const auto protected_meta_bytes=bytes(recovery_meta);
    check(host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(saved_revision)&&host.dirty()&&
          protected_bytes==committed_bytes&&
          QJsonDocument::fromJson(protected_meta_bytes).object()["source_file"]==native_path(source_path),
          "Conflict protects the committed Margin and Grid while retaining the original source binding");
    check(bytes(source_path)==external_bytes&&sha256(bytes(source_path))==external_hash,
          "External original bytes and hash survive the layout source conflict");

    const auto failed_destination=directory+"/missing-layout-save-parent/failed.nect";
    rejects("IO_ERROR",[&]{host.save(failed_destination);});
    check(!QFile::exists(failed_destination)&&host.file_path==native_path(source_path)&&host.dirty()&&
          host.session.revision()==committed_revision&&host.session.document()==committed&&
          host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(saved_revision)&&
          host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(committed_revision),
          "Failed Save As leaves destination, binding, revisions and authored layout unchanged");
    check(bytes(source_path)==external_bytes&&sha256(bytes(source_path))==external_hash&&
          bytes(recovery_path)==protected_bytes&&bytes(recovery_meta)==protected_meta_bytes,
          "Failed Save As preserves external source and protected recovery bytes");

    const auto destination=directory+"/layout-save-destination.nect";
    host.save(destination);host.recover();
    const auto destination_bytes=bytes(destination);
    const auto saved=load_native(destination).document;
    const auto& saved_board=saved.compositions.front().artboards.front();
    const auto saved_expression_board=std::find_if(saved.compositions.front().artboards.begin(),
        saved.compositions.front().artboards.end(),[&](const Artboard& value){return value.id==expression_board_id;});
    const auto saved_columns_expression_board=std::find_if(saved.compositions.front().artboards.begin(),
        saved.compositions.front().artboards.end(),[&](const Artboard& value){
            return value.id==grid_columns_expression_board_id;
        });
    check(host.file_path==native_path(destination)&&!host.dirty()&&
          host.persistence()["saved_revision"].toInteger(-1)==static_cast<qint64>(committed_revision)&&
          host.persistence()["native_error"].isNull()&&saved==committed&&
          destination_bytes==committed_bytes&&
          destination_bytes.contains(QByteArray::fromStdString(std::string("\"version\":\"")+native_version+"\""))&&
          saved_board.id==artboard_id&&saved_board.layout&&*saved_board.layout==committed_layout&&
          saved_expression_board!=saved.compositions.front().artboards.end()&&saved_expression_board->layout&&
          *saved_expression_board->layout==committed_expression_layout&&
          saved_columns_expression_board!=saved.compositions.front().artboards.end()&&
          saved_columns_expression_board->layout&&*saved_columns_expression_board->layout==grid_columns_expression_layout&&
          artboard_layout_property(saved,grid_columns_ref).driver==grid_columns_source_ref&&
          std::get<std::size_t>(artboard_layout_property(saved,grid_columns_ref).literal)==4&&
          std::get<std::size_t>(artboard_layout_property(saved,grid_columns_ref).evaluated)==3&&
          artboard_layout_property(saved,grid_columns_expression_ref).expression==grid_columns_expression&&
          std::get<std::size_t>(artboard_layout_property(saved,grid_columns_expression_ref).literal)==2&&
          std::get<std::size_t>(artboard_layout_property(saved,grid_columns_expression_ref).evaluated)==4&&
          artboard_layout_property(saved,grid_rows_expression_ref).expression==grid_rows_expression&&
          std::get<std::size_t>(artboard_layout_property(saved,grid_rows_expression_ref).literal)==2&&
          std::get<std::size_t>(artboard_layout_property(saved,grid_rows_expression_ref).evaluated)==5&&
          artboard_layout_property(saved,grid_rows_ref).driver==grid_rows_source_ref&&
          std::get<std::size_t>(artboard_layout_property(saved,grid_rows_ref).literal)==3&&
          std::get<std::size_t>(artboard_layout_property(saved,grid_rows_ref).evaluated)==4&&
          artboard_layout_property(saved,expression_grid_column_gutter_ref).expression==column_gutter_expression&&
          std::get<double>(artboard_layout_property(saved,expression_grid_column_gutter_ref).evaluated)==20&&
          bytes(source_path)==external_bytes&&sha256(bytes(source_path))==external_hash,
          "Valid Save As writes exact native 0.60 bytes and retains Grid links/expressions, authored counts and stable IDs");
    check(host.persistence()["recovery_revision"].toInteger(-1)==static_cast<qint64>(committed_revision)&&
          QJsonDocument::fromJson(bytes(recovery_meta)).object()["source_file"]==native_path(destination),
          "Recovery provenance follows the Grid and Margin Save As destination");

    Host reopened(directory+"/layout-save-cold-recovery");reopened.open(destination);
    const auto cold=reopened.session.document();
    const auto& cold_board=cold.compositions.front().artboards.front();
    const auto cold_expression_board=std::find_if(cold.compositions.front().artboards.begin(),
        cold.compositions.front().artboards.end(),[&](const Artboard& value){return value.id==expression_board_id;});
    const auto cold_columns_expression_board=std::find_if(cold.compositions.front().artboards.begin(),
        cold.compositions.front().artboards.end(),[&](const Artboard& value){
            return value.id==grid_columns_expression_board_id;
        });
    check(cold==committed&&reopened.session.revision()==0&&reopened.file_path==native_path(destination)&&
          cold_board.id==artboard_id&&cold_board.layout&&*cold_board.layout==committed_layout&&
          cold_expression_board!=cold.compositions.front().artboards.end()&&cold_expression_board->layout&&
          *cold_expression_board->layout==committed_expression_layout&&
          cold_columns_expression_board!=cold.compositions.front().artboards.end()&&
          cold_columns_expression_board->layout&&*cold_columns_expression_board->layout==grid_columns_expression_layout&&
          artboard_layout_property(cold,grid_columns_ref).driver==grid_columns_source_ref&&
          std::get<std::size_t>(artboard_layout_property(cold,grid_columns_ref).literal)==4&&
          std::get<std::size_t>(artboard_layout_property(cold,grid_columns_ref).evaluated)==3&&
          artboard_layout_property(cold,grid_columns_expression_ref).expression==grid_columns_expression&&
          std::get<std::size_t>(artboard_layout_property(cold,grid_columns_expression_ref).literal)==2&&
          std::get<std::size_t>(artboard_layout_property(cold,grid_columns_expression_ref).evaluated)==4&&
          artboard_layout_property(cold,grid_rows_expression_ref).expression==grid_rows_expression&&
          std::get<std::size_t>(artboard_layout_property(cold,grid_rows_expression_ref).literal)==2&&
          std::get<std::size_t>(artboard_layout_property(cold,grid_rows_expression_ref).evaluated)==5&&
          artboard_layout_property(cold,grid_rows_ref).driver==grid_rows_source_ref&&
          std::get<std::size_t>(artboard_layout_property(cold,grid_rows_ref).literal)==3&&
          std::get<std::size_t>(artboard_layout_property(cold,grid_rows_ref).evaluated)==4&&
          artboard_layout_property(cold,expression_grid_column_gutter_ref).expression==column_gutter_expression&&
          std::get<double>(artboard_layout_property(cold,expression_grid_column_gutter_ref).evaluated)==20,
          "Cold Host reopen restores every authored Margin, Grid field and stable ID");
    const auto check_local_reads=[&](const Document& document) {
        const auto read=[&](const Id& owner,const char* field) {
            return artboard_layout_property(document,Ref{owner,"",field}).literal;
        };
        const auto left=read(artboard_id,"margin.left"),top=read(artboard_id,"margin.top");
        const auto right=read(artboard_id,"margin.right"),bottom=read(artboard_id,"margin.bottom");
        check(std::holds_alternative<double>(left)&&std::get<double>(left)==committed_layout.margin->left&&
              std::holds_alternative<double>(top)&&std::get<double>(top)==committed_layout.margin->top&&
              std::holds_alternative<double>(right)&&std::get<double>(right)==committed_layout.margin->right&&
              std::holds_alternative<double>(bottom)&&std::get<double>(bottom)==committed_layout.margin->bottom,
              "Typed Margin reads return the exact Artboard-local authored insets");
        const auto x=read(grid_id,"grid.bounds.x"),y=read(grid_id,"grid.bounds.y");
        const auto width=read(grid_id,"grid.bounds.width"),height=read(grid_id,"grid.bounds.height");
        check(std::holds_alternative<double>(x)&&std::get<double>(x)==committed_layout.grid->bounds.x&&
              std::holds_alternative<double>(y)&&std::get<double>(y)==committed_layout.grid->bounds.y&&
              std::holds_alternative<double>(width)&&std::get<double>(width)==committed_layout.grid->bounds.width,
              "Typed Grid reads return the exact Artboard-local authored position and width");
        const auto height_property=artboard_layout_property(document,grid_height_ref);
        check(std::get<double>(height_property.literal)==committed_layout.grid->bounds.height&&
              height_property.expression==grid_height_expression&&std::get<double>(height_property.evaluated)==420,
              "Typed Grid height read preserves the exact authored literal and expression beside evaluated height");
        const auto columns=read(grid_id,"grid.columns"),rows=read(grid_id,"grid.rows");
        check(std::holds_alternative<std::size_t>(columns)&&std::get<std::size_t>(columns)==committed_layout.grid->columns&&
              std::holds_alternative<std::size_t>(rows)&&std::get<std::size_t>(rows)==committed_layout.grid->rows,
              "Typed Grid count reads remain exact integers");
        const auto grid_columns_expression_property=artboard_layout_property(document,grid_columns_expression_ref);
        check(std::get<std::size_t>(grid_columns_expression_property.literal)==2&&
              grid_columns_expression_property.expression==grid_columns_expression&&
              std::get<std::size_t>(grid_columns_expression_property.evaluated)==4,
              "Typed Grid columns read preserves the expression literal and evaluated integer across Save As");
        const auto grid_rows_expression_property=artboard_layout_property(document,grid_rows_expression_ref);
        check(std::get<std::size_t>(grid_rows_expression_property.literal)==2&&
              grid_rows_expression_property.expression==grid_rows_expression&&
              std::get<std::size_t>(grid_rows_expression_property.evaluated)==5,
              "Typed Grid rows read preserves the exact expression literal and evaluated count across Save As");
        const auto column_gutter=read(grid_id,"grid.column_gutter"),row_gutter=read(grid_id,"grid.row_gutter");
        check(std::holds_alternative<double>(column_gutter)&&
              std::get<double>(column_gutter)==committed_layout.grid->column_gutter&&
              std::holds_alternative<double>(row_gutter)&&
              std::get<double>(row_gutter)==committed_layout.grid->row_gutter,
              "Typed Grid gutter reads return exact authored values");
        const auto column_gutter_property=artboard_layout_property(document,grid_column_gutter_ref);
        check(std::get<double>(column_gutter_property.literal)==committed_layout.grid->column_gutter&&
              column_gutter_property.driver==grid_column_gutter_source_ref&&
              std::get<double>(column_gutter_property.evaluated)==10,
              "Grid column gutter typed read preserves its authored literal and exact Artboard source across Save As");
        const auto rows_property=artboard_layout_property(document,grid_rows_ref);
        check(std::get<std::size_t>(rows_property.literal)==committed_layout.grid->rows&&
              rows_property.driver==grid_rows_source_ref&&std::get<std::size_t>(rows_property.evaluated)==4,
              "Grid rows typed read preserves its authored count and exact same-field source across Save As");
        const auto row_gutter_property=artboard_layout_property(document,grid_row_gutter_ref);
        check(std::get<double>(row_gutter_property.literal)==committed_layout.grid->row_gutter&&
              !row_gutter_property.driver&&row_gutter_property.expression==grid_row_gutter_expression&&
              std::get<double>(row_gutter_property.evaluated)==20,
              "Grid row gutter typed read preserves its authored literal and exact expression across Save As");
        const auto column_expression_property=artboard_layout_property(document,expression_grid_column_gutter_ref);
        check(column_expression_property.expression==column_gutter_expression&&
              std::get<double>(column_expression_property.literal)==12&&
              std::get<double>(column_expression_property.evaluated)==20,
              "Grid column gutter expression typed read preserves source and evaluated value across Save As");
    };
    check_local_reads(cold);

    QProcess process;
    process.start(nect_cli,{"--serve",destination});
    check(process.waitForStarted(5000),"Start a separate Nect JSON-lines process on the Save As destination");
    const auto get_line=[](const Id& owner,const char* field) {
        const QJsonObject ref{{"object",QString::fromStdString(owner)},{"point",""},{"field",field}};
        return QJsonDocument(QJsonObject{{"op","get"},{"ref",ref}}).toJson(QJsonDocument::Compact)+'\n';
    };
    QByteArray queries=get_line(artboard_id,"margin.left")+get_line(grid_id,"grid.columns")+
        get_line(grid_id,"grid.rows")+
        get_line(grid_id,"grid.bounds.x")+get_line(grid_id,"grid.bounds.height")+
        get_line(grid_id,"grid.column_gutter")+get_line(grid_id,"grid.row_gutter")+
        get_line(expression_grid_id,"grid.column_gutter")+
        get_line(grid_columns_expression_grid_id,"grid.columns")+
        get_line(grid_columns_expression_grid_id,"grid.rows")+
        QByteArray("{\"op\":\"inspect\"}\n");
    check(process.write(queries)==queries.size(),"Send exact typed reads to the cold Nect process");
    process.closeWriteChannel();
    check(process.waitForFinished(10000)&&process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0,
        "Cold Nect process exits after reading the Save As destination");
    const auto output=process.readAllStandardOutput().trimmed().split('\n');
    check(output.size()==11,"Cold Nect process returns one response per typed read and inspect request");
    const auto margin_read=QJsonDocument::fromJson(output[0]).object()["result"].toObject();
    const auto count_read=QJsonDocument::fromJson(output[1]).object()["result"].toObject();
    const auto rows_read=QJsonDocument::fromJson(output[2]).object()["result"].toObject();
    const auto grid_read=QJsonDocument::fromJson(output[3]).object()["result"].toObject();
    const auto height_read=QJsonDocument::fromJson(output[4]).object()["result"].toObject();
    const auto column_gutter_read=QJsonDocument::fromJson(output[5]).object()["result"].toObject();
    const auto row_gutter_read=QJsonDocument::fromJson(output[6]).object()["result"].toObject();
    const auto column_expression_read=QJsonDocument::fromJson(output[7]).object()["result"].toObject();
    const auto columns_expression_read=QJsonDocument::fromJson(output[8]).object()["result"].toObject();
    const auto rows_expression_read=QJsonDocument::fromJson(output[9]).object()["result"].toObject();
    const auto inspected=QJsonDocument::fromJson(output[10]).object()["result"].toObject();
    check(margin_read["ref"].toObject()["object"]==QString::fromStdString(artboard_id)&&
          margin_read["authored"].toObject()["literal"].toDouble()==committed_layout.margin->left&&
          margin_read["evaluated"].toDouble()==committed_layout.margin->left&&
          margin_read["space"]=="artboard_local"&&count_read["ref"].toObject()["object"]==QString::fromStdString(grid_id)&&
          count_read["type"]=="integer"&&count_read["authored"].toObject()["literal"].toInt()==4&&
          count_read["authored"].toObject()["source_kind"]=="link"&&
          count_read["authored"].toObject()["driver"].toObject()["object"].toString()==
              QString::fromStdString(grid_columns_source_grid_id)&&
          count_read["evaluated"].toInt()==3&&
          rows_read["ref"].toObject()["field"]=="grid.rows"&&rows_read["type"]=="integer"&&
          rows_read["authored"].toObject()["literal"].toInt()==3&&
          rows_read["authored"].toObject()["driver"].toObject()["object"].toString()==
              QString::fromStdString(grid_columns_source_grid_id)&&rows_read["evaluated"].toInt()==4&&rows_read["link"].toBool()&&
          grid_read["evaluated"].toDouble()==committed_layout.grid->bounds.x&&
          height_read["authored"].toObject()["literal"].toDouble()==committed_layout.grid->bounds.height&&
          height_read["authored"].toObject()["expression"].toObject()["source"].toString()==QString::fromStdString(grid_height_expression.source)&&
          height_read["evaluated"].toDouble()==420&&height_read["expression"].toBool()&&
          column_gutter_read["authored"].toObject()["literal"].toDouble()==committed_layout.grid->column_gutter&&
          column_gutter_read["authored"].toObject()["driver"].toObject()["object"].toString()==
              QString::fromStdString(grid_column_gutter_source_ref.object)&&
          column_gutter_read["evaluated"].toDouble()==10&&column_gutter_read["link"].toBool()&&
          row_gutter_read["authored"].toObject()["literal"].toDouble()==committed_layout.grid->row_gutter&&
          row_gutter_read["authored"].toObject()["expression"].toObject()["source"].toString()==
              QString::fromStdString(grid_row_gutter_expression.source)&&
          row_gutter_read["evaluated"].toDouble()==20&&row_gutter_read["expression"].toBool()&&
          column_expression_read["ref"].toObject()["object"].toString()==QString::fromStdString(expression_grid_id)&&
          column_expression_read["authored"].toObject()["expression"].toObject()["source"].toString()==
              QString::fromStdString(column_gutter_expression.source)&&
          column_expression_read["evaluated"].toDouble()==20&&column_expression_read["expression"].toBool()&&
          columns_expression_read["ref"].toObject()["object"].toString()==QString::fromStdString(grid_columns_expression_grid_id)&&
          columns_expression_read["authored"].toObject()["literal"].toInt()==2&&
          columns_expression_read["authored"].toObject()["expression"].toObject()["source"].toString()==
              QString::fromStdString(grid_columns_expression.source)&&
           columns_expression_read["authored"].toObject()["source_kind"]=="expression"&&
           columns_expression_read["evaluated"].toInt()==4&&columns_expression_read["expression"].toBool()&&
           rows_expression_read["ref"].toObject()["object"].toString()==QString::fromStdString(grid_columns_expression_grid_id)&&
           rows_expression_read["authored"].toObject()["literal"].toInt()==2&&
           rows_expression_read["authored"].toObject()["expression"].toObject()["source"].toString()==
               QString::fromStdString(grid_rows_expression.source)&&
           rows_expression_read["authored"].toObject()["source_kind"]=="expression"&&
           rows_expression_read["evaluated"].toInt()==5&&rows_expression_read["expression"].toBool()&&
           inspected==QJsonDocument::fromJson(destination_bytes).object(),
           "A separate Nect process cold-opens exact Save As bytes and reads the row link and both Grid count expressions");

    const auto local_grid=evaluate_artboard(cold.compositions.front(),artboard_id).layout->grid->bounds;
    const LayoutRect world_before{cold_board.x+local_grid.x,cold_board.y+local_grid.y,
        local_grid.width,local_grid.height};
    auto moved_board=cold_board;moved_board.x+=125;moved_board.y+=75;
    reopened.session.apply({UpdateArtboard{composition,moved_board}},reopened.session.revision());
    const auto moved=reopened.session.document();
    const auto& moved_result=moved.compositions.front().artboards.front();
    check_local_reads(moved);
    const auto moved_local=evaluate_artboard(moved.compositions.front(),artboard_id).layout->grid->bounds;
    const LayoutRect world_after{moved_result.x+moved_local.x,moved_result.y+moved_local.y,
        moved_local.width,moved_local.height};
    check(moved_result.layout&&*moved_result.layout==committed_layout&&
          world_after==LayoutRect{world_before.x+125,world_before.y+75,world_before.width,world_before.height}&&
          bytes(destination)==destination_bytes,
          "Moving the reopened Artboard translates its world Grid frame without rewriting local layout");
}
void linked_margin_left_save_as(const QString& directory,const QString& nect_cli) {
    Host host(directory+"/linked-margin-left-recovery");
    const auto composition=host.session.document().compositions.front().id;
    const auto target=host.session.document().compositions.front().artboards.front().id;
    const Id source="save-margin-left-source",upstream="save-margin-left-upstream";
    Artboard source_board{source,"Margin source",0,0,40,100};
    source_board.parent_size=ArtboardParent{upstream,true,true};
    Artboard upstream_board{upstream,"Source size",0,0,40,100};
    ArtboardLayout layout;layout.margin=Margin{40,20,40,20};
    layout.grid=Grid{"save-margin-left-grid",{40,20,880,560},2,1,20,0};
    const Ref target_ref{target,"","margin.left"};
    const Ref target_top_ref{target,"","margin.top"};
    const Ref target_right_ref{target,"","margin.right"};
    const Ref target_bottom_ref{target,"","margin.bottom"};
    const Ref grid_x_ref{"save-margin-left-grid","","grid.bounds.x"};
    const Ref grid_y_ref{"save-margin-left-grid","","grid.bounds.y"};
    const Ref grid_width_ref{"save-margin-left-grid","","grid.bounds.width"};
    const Ref grid_height_ref{"save-margin-left-grid","","grid.bounds.height"};
    const Ref source_ref{source,"","artboard.width"};
    const Ref source_top_ref{source,"","artboard.height"};
    const Ref source_bottom_ref{source,"","artboard.height"};
    host.session.apply({AddArtboard{composition,source_board,1},AddArtboard{composition,upstream_board,2},
        SetArtboardLayout{composition,target,layout},MarginLeftCommand{LinkMarginLeft{target_ref,source_ref,false}},
        MarginTopCommand{LinkMarginTop{target_top_ref,source_top_ref,false}},
        MarginRightCommand{LinkMarginRight{target_right_ref,source_ref,false}},
        MarginBottomCommand{LinkMarginBottom{target_bottom_ref,source_bottom_ref,false}},
        GridBoundsXCommand{LinkGridBoundsX{grid_x_ref,source_ref,false}},
        GridBoundsYCommand{LinkGridBoundsY{grid_y_ref,source_ref,false}},
        GridBoundsWidthCommand{LinkGridBoundsWidth{grid_width_ref,source_ref,false}},
        GridBoundsHeightCommand{LinkGridBoundsHeight{grid_height_ref,source_top_ref,false}}},
        host.session.revision());host.edited();
    const auto original=directory+"/linked-margin-left-source.nect";
    host.save(original);host.recover();const auto original_bytes=bytes(original);
    auto changed_upstream=host.session.document().compositions.front().artboards[2];
    changed_upstream.width=60;changed_upstream.height=70;
    host.session.apply({UpdateArtboard{composition,changed_upstream}},host.session.revision());host.edited();
    const Expression margin_expression{R"(ref("save-margin-left-source","","artboard.width") + 10)",1};
    const Expression margin_top_expression{R"(ref("save-margin-left-source","","artboard.height") + 10)",1};
    const Expression margin_right_expression{R"(ref("save-margin-left-source","","artboard.width") + 10)",1};
    const Expression margin_bottom_expression{R"(ref("save-margin-left-source","","artboard.height") + 10)",1};
    const Expression y_expression{R"(ref("save-margin-left-source","","artboard.width"))",1};
    const Expression grid_width_expression{R"(ref("save-margin-left-source","","artboard.width") + 10)",1};
    host.session.apply({MarginLeftCommand{SetMarginLeftExpression{target_ref,margin_expression,true}}},host.session.revision());host.edited();
    host.session.apply({MarginTopCommand{SetMarginTopExpression{target_top_ref,margin_top_expression,true}}},host.session.revision());host.edited();
    host.session.apply({MarginRightCommand{SetMarginRightExpression{target_right_ref,margin_right_expression,true}}},host.session.revision());host.edited();
    host.session.apply({MarginBottomCommand{SetMarginBottomExpression{target_bottom_ref,margin_bottom_expression,true}}},host.session.revision());host.edited();
    host.session.apply({GridBoundsYCommand{SetGridBoundsYExpression{grid_y_ref,y_expression,true}}},host.session.revision());host.edited();
    host.session.apply({GridBoundsWidthCommand{SetGridBoundsWidthExpression{grid_width_ref,grid_width_expression,true}}},host.session.revision());host.edited();
    const auto committed=host.session.document();
    const auto committed_bytes=QByteArray::fromStdString(encode(committed));
    const auto linked=artboard_layout_property(committed,target_ref);
    const auto linked_top=artboard_layout_property(committed,target_top_ref);
    const auto linked_right=artboard_layout_property(committed,target_right_ref);
    const auto linked_bottom=artboard_layout_property(committed,target_bottom_ref);
    const auto linked_grid_x=artboard_layout_property(committed,grid_x_ref);
    const auto linked_grid_y=artboard_layout_property(committed,grid_y_ref);
    const auto linked_grid_width=artboard_layout_property(committed,grid_width_ref);
    const auto linked_grid_height=artboard_layout_property(committed,grid_height_ref);
    check(std::get<double>(linked.literal)==40&&!linked.driver&&linked.expression==margin_expression&&
          linked.source_kind=="expression"&&std::get<double>(linked.evaluated)==70&&
          std::get<double>(linked_top.literal)==20&&!linked_top.driver&&linked_top.expression==margin_top_expression&&
          linked_top.source_kind=="expression"&&std::get<double>(linked_top.evaluated)==80&&
          std::get<double>(linked_right.literal)==40&&!linked_right.driver&&linked_right.expression==margin_right_expression&&
          linked_right.source_kind=="expression"&&std::get<double>(linked_right.evaluated)==70&&
          std::get<double>(linked_bottom.literal)==20&&!linked_bottom.driver&&linked_bottom.expression==margin_bottom_expression&&
          linked_bottom.source_kind=="expression"&&std::get<double>(linked_bottom.evaluated)==80&&
          std::get<double>(linked_grid_x.literal)==40&&linked_grid_x.driver==source_ref&&
          std::get<double>(linked_grid_x.evaluated)==60&&std::get<double>(linked_grid_y.literal)==20&&
          !linked_grid_y.driver&&linked_grid_y.expression==y_expression&&
          linked_grid_y.source_kind=="expression"&&std::get<double>(linked_grid_y.evaluated)==60&&
           std::get<double>(linked_grid_width.literal)==880&&!linked_grid_width.driver&&
           linked_grid_width.expression==grid_width_expression&&linked_grid_width.source_kind=="expression"&&
           std::get<double>(linked_grid_width.evaluated)==70&&std::get<double>(linked_grid_height.literal)==560&&
           linked_grid_height.driver==source_top_ref&&linked_grid_height.source_kind=="link"&&
           std::get<double>(linked_grid_height.evaluated)==70,
        "Host retains Margin and Grid sources beside authored literals after the upstream edit");

    const auto destination=directory+"/linked-margin-left-destination.nect";
    host.save(destination);host.recover();
    const auto destination_bytes=bytes(destination);
    const auto persisted=load_native(destination).document;
    const auto persisted_link=artboard_layout_property(persisted,target_ref);
    const auto persisted_top=artboard_layout_property(persisted,target_top_ref);
    const auto persisted_right=artboard_layout_property(persisted,target_right_ref);
    const auto persisted_bottom=artboard_layout_property(persisted,target_bottom_ref);
    const auto persisted_grid_x=artboard_layout_property(persisted,grid_x_ref);
    const auto persisted_grid_y=artboard_layout_property(persisted,grid_y_ref);
    const auto persisted_grid_width=artboard_layout_property(persisted,grid_width_ref);
    const auto persisted_grid_height=artboard_layout_property(persisted,grid_height_ref);
    check(host.file_path==native_path(destination)&&!host.dirty()&&persisted==committed&&
        destination_bytes==committed_bytes&&destination_bytes.contains("\"version\":\"0.63\"")&&
        bytes(original)==original_bytes&&
        std::get<double>(persisted_link.literal)==40&&!persisted_link.driver&&persisted_link.expression==margin_expression&&
        std::get<double>(persisted_link.evaluated)==70&&
        std::get<double>(persisted_top.literal)==20&&!persisted_top.driver&&persisted_top.expression==margin_top_expression&&
        std::get<double>(persisted_top.evaluated)==80&&
         std::get<double>(persisted_right.literal)==40&&!persisted_right.driver&&persisted_right.expression==margin_right_expression&&
         std::get<double>(persisted_right.evaluated)==70&&
         std::get<double>(persisted_bottom.literal)==20&&!persisted_bottom.driver&&
         persisted_bottom.expression==margin_bottom_expression&&std::get<double>(persisted_bottom.evaluated)==80&&
        std::get<double>(persisted_grid_x.literal)==40&&persisted_grid_x.driver==source_ref&&std::get<double>(persisted_grid_x.evaluated)==60&&
        std::get<double>(persisted_grid_y.literal)==20&&!persisted_grid_y.driver&&
        persisted_grid_y.expression==y_expression&&std::get<double>(persisted_grid_y.evaluated)==60&&
         std::get<double>(persisted_grid_width.literal)==880&&!persisted_grid_width.driver&&
         persisted_grid_width.expression==grid_width_expression&&std::get<double>(persisted_grid_width.evaluated)==70&&
         std::get<double>(persisted_grid_height.literal)==560&&persisted_grid_height.driver==source_top_ref&&
         std::get<double>(persisted_grid_height.evaluated)==70,
          "Host Save As writes native 0.60 Grid height/width expressions and Margin sources beside authored literals while preserving original bytes");

    Host cold(directory+"/linked-margin-left-cold-recovery");cold.open(destination);
    const auto cold_value=artboard_layout_property(cold.session.document(),target_ref);
    const auto cold_top=artboard_layout_property(cold.session.document(),target_top_ref);
    const auto cold_right=artboard_layout_property(cold.session.document(),target_right_ref);
    const auto cold_bottom=artboard_layout_property(cold.session.document(),target_bottom_ref);
    const auto cold_grid_x=artboard_layout_property(cold.session.document(),grid_x_ref);
    const auto cold_grid_y=artboard_layout_property(cold.session.document(),grid_y_ref);
    const auto cold_grid_width=artboard_layout_property(cold.session.document(),grid_width_ref);
    const auto cold_grid_height=artboard_layout_property(cold.session.document(),grid_height_ref);
    check(cold.session.revision()==0&&cold.session.document()==committed&&std::get<double>(cold_value.literal)==40&&
        !cold_value.driver&&cold_value.expression==margin_expression&&std::get<double>(cold_value.evaluated)==70&&
        std::get<double>(cold_top.literal)==20&&!cold_top.driver&&cold_top.expression==margin_top_expression&&
        std::get<double>(cold_top.evaluated)==80&&
         std::get<double>(cold_right.literal)==40&&!cold_right.driver&&cold_right.expression==margin_right_expression&&
         std::get<double>(cold_right.evaluated)==70&&
        std::get<double>(cold_bottom.literal)==20&&!cold_bottom.driver&&
        cold_bottom.expression==margin_bottom_expression&&std::get<double>(cold_bottom.evaluated)==80&&
        std::get<double>(cold_grid_x.literal)==40&&cold_grid_x.driver==source_ref&&std::get<double>(cold_grid_x.evaluated)==60&&
        std::get<double>(cold_grid_y.literal)==20&&!cold_grid_y.driver&&
        cold_grid_y.expression==y_expression&&std::get<double>(cold_grid_y.evaluated)==60&&
         std::get<double>(cold_grid_width.literal)==880&&!cold_grid_width.driver&&
         cold_grid_width.expression==grid_width_expression&&std::get<double>(cold_grid_width.evaluated)==70&&
         std::get<double>(cold_grid_height.literal)==560&&cold_grid_height.driver==source_top_ref&&
         std::get<double>(cold_grid_height.evaluated)==70,
         "A fresh Host cold-open preserves all Margin and Grid layout expressions, literals and evaluated values");

    QProcess process;process.start(nect_cli,{"--serve",destination});
    check(process.waitForStarted(5000),"Start a distinct JSON-lines process on the Margin expression Save As destination");
    const QJsonObject ref{{"object",QString::fromStdString(target)},{"point",""},{"field","margin.left"}};
    const auto query=QJsonDocument(QJsonObject{{"op","get"},{"ref",ref}}).toJson(QJsonDocument::Compact)+'\n'+
        QJsonDocument(QJsonObject{{"op","get"},{"ref",QJsonObject{{"object",QString::fromStdString(target)},{"point",""},{"field","margin.top"}}}}).toJson(QJsonDocument::Compact)+'\n'+
        QJsonDocument(QJsonObject{{"op","get"},{"ref",QJsonObject{{"object",QString::fromStdString(target)},{"point",""},{"field","margin.right"}}}}).toJson(QJsonDocument::Compact)+'\n'+
        QJsonDocument(QJsonObject{{"op","get"},{"ref",QJsonObject{{"object",QString::fromStdString(target)},{"point",""},{"field","margin.bottom"}}}}).toJson(QJsonDocument::Compact)+'\n'+
        QJsonDocument(QJsonObject{{"op","get"},{"ref",QJsonObject{{"object","save-margin-left-grid"},{"point",""},{"field","grid.bounds.x"}}}}).toJson(QJsonDocument::Compact)+'\n'+
        QJsonDocument(QJsonObject{{"op","get"},{"ref",QJsonObject{{"object","save-margin-left-grid"},{"point",""},{"field","grid.bounds.y"}}}}).toJson(QJsonDocument::Compact)+'\n'+
        QJsonDocument(QJsonObject{{"op","get"},{"ref",QJsonObject{{"object","save-margin-left-grid"},{"point",""},{"field","grid.bounds.width"}}}}).toJson(QJsonDocument::Compact)+'\n'+
        QJsonDocument(QJsonObject{{"op","get"},{"ref",QJsonObject{{"object","save-margin-left-grid"},{"point",""},{"field","grid.bounds.height"}}}}).toJson(QJsonDocument::Compact)+'\n'+
        QByteArray("{\"op\":\"inspect\"}\n");
    check(process.write(query)==query.size(),"Send Margin expression/Grid source reads and native inspect to the cold process");
    process.closeWriteChannel();
    check(process.waitForFinished(10000)&&process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0,
        "Distinct JSON-lines process exits after reading the Margin expression destination");
    const auto output=process.readAllStandardOutput().trimmed().split('\n');
    check(output.size()==9,"Cold process returns linked Margin/Grid typed reads and one native inspect reply");
    const auto read=QJsonDocument::fromJson(output[0]).object()["result"].toObject();
    const auto top_read=QJsonDocument::fromJson(output[1]).object()["result"].toObject();
    const auto right_read=QJsonDocument::fromJson(output[2]).object()["result"].toObject();
    const auto bottom_read=QJsonDocument::fromJson(output[3]).object()["result"].toObject();
    const auto grid_read=QJsonDocument::fromJson(output[4]).object()["result"].toObject();
    const auto grid_y_read=QJsonDocument::fromJson(output[5]).object()["result"].toObject();
    const auto grid_width_read=QJsonDocument::fromJson(output[6]).object()["result"].toObject();
    const auto grid_height_read=QJsonDocument::fromJson(output[7]).object()["result"].toObject();
    const auto native=QJsonDocument::fromJson(output[8]).object()["result"].toObject();
    const auto expression=read["authored"].toObject()["expression"].toObject();
    const auto grid_driver=grid_read["authored"].toObject()["driver"].toObject();
    check(read["authored"].toObject()["literal"].toDouble()==40&&read["evaluated"].toDouble()==70&&
        read["authored"].toObject()["source_kind"].toString()=="expression"&&read["expression"].toBool()&&
        expression["source"].toString()==QString::fromStdString(margin_expression.source)&&
        top_read["authored"].toObject()["literal"].toDouble()==20&&top_read["evaluated"].toDouble()==80&&
        top_read["expression"].toBool()&&top_read["authored"].toObject()["source_kind"].toString()=="expression"&&
        top_read["authored"].toObject()["expression"].toObject()["source"].toString()==QString::fromStdString(margin_top_expression.source)&&
        right_read["authored"].toObject()["literal"].toDouble()==40&&right_read["evaluated"].toDouble()==70&&
        right_read["authored"].toObject()["source_kind"].toString()=="expression"&&right_read["link"].toBool()&&
        right_read["expression"].toBool()&&right_read["authored"].toObject()["expression"].toObject()["source"].toString()==QString::fromStdString(margin_right_expression.source)&&
        bottom_read["authored"].toObject()["literal"].toDouble()==20&&bottom_read["evaluated"].toDouble()==80&&
        bottom_read["authored"].toObject()["source_kind"].toString()=="expression"&&bottom_read["link"].toBool()&&
        bottom_read["expression"].toBool()&&bottom_read["authored"].toObject()["expression"].toObject()["source"].toString()==QString::fromStdString(margin_bottom_expression.source)&&
        grid_read["authored"].toObject()["literal"].toDouble()==40&&
        grid_read["evaluated"].toDouble()==60&&grid_read["link"].toBool()&&
        grid_driver["object"].toString()==QString::fromStdString(source)&&grid_driver["field"].toString()=="artboard.width"&&
        grid_y_read["authored"].toObject()["literal"].toDouble()==20&&grid_y_read["evaluated"].toDouble()==60&&
        grid_y_read["link"].toBool()&&grid_y_read["expression"].toBool()&&
        grid_y_read["authored"].toObject()["source_kind"].toString()=="expression"&&
        grid_y_read["authored"].toObject()["expression"].toObject()["source"].toString()==QString::fromStdString(y_expression.source)&&
        grid_width_read["authored"].toObject()["literal"].toDouble()==880&&grid_width_read["evaluated"].toDouble()==70&&
         grid_width_read["authored"].toObject()["source_kind"].toString()=="expression"&&
         grid_width_read["authored"].toObject()["expression"].toObject()["source"].toString()==QString::fromStdString(grid_width_expression.source)&&
         grid_width_read["link"].toBool()&&grid_width_read["expression"].toBool()&&
        grid_height_read["authored"].toObject()["literal"].toDouble()==560&&
        grid_height_read["evaluated"].toDouble()==70&&grid_height_read["link"].toBool()&&
        grid_height_read["expression"].toBool()&&
        grid_height_read["authored"].toObject()["source_kind"].toString()=="link"&&
        grid_height_read["authored"].toObject()["driver"].toObject()["object"].toString()==QString::fromStdString(source)&&
        grid_height_read["authored"].toObject()["driver"].toObject()["field"].toString()=="artboard.height"&&
        native==QJsonDocument::fromJson(destination_bytes).object(),
        "Distinct process reads evaluated Grid height, exact source Ref and the Save As bytes; original file bytes remain unchanged");
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
        check(argc==2,"Live Save As test requires the matching Nect CLI path");
        QTemporaryDir temp;check(temp.isValid(),"Create owned live-save test folder");
        coalescing_and_conflict(temp.path());typed_source_save_as(temp.path());
        text_weight_expression_save_as(temp.path(),QString::fromLocal8Bit(argv[1]));
        object_visibility_expression_save_as(temp.path(),QString::fromLocal8Bit(argv[1]));
        composite_isolation_expression_save_as(temp.path(),QString::fromLocal8Bit(argv[1]));
        point_edit_save_as(temp.path());artboard_size_save_as(temp.path());
        layout_save_as(temp.path(),QString::fromLocal8Bit(argv[1]));
        linked_margin_left_save_as(temp.path(),QString::fromLocal8Bit(argv[1]));
        linked_point_edit_save_as(temp.path());linked_mask_save_as(temp.path());independent_failures(temp.path());identity_drain(temp.path());
        std::cout<<"PASS asynchronous snapshots, typed, linked Margin, Grid, Point Edit and mask Save As preservation, failure atomicity, conflict recovery and Session drain\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
