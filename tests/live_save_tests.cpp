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
          bytes(destination).contains("\"version\":\"0.23\""),
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
        coalescing_and_conflict(temp.path());typed_source_save_as(temp.path());independent_failures(temp.path());identity_drain(temp.path());
        std::cout<<"PASS asynchronous snapshots, typed Save As source preservation, failure atomicity, conflict recovery and Session drain\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
