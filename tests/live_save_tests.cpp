#include "host.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
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
        coalescing_and_conflict(temp.path());independent_failures(temp.path());identity_drain(temp.path());
        std::cout<<"PASS asynchronous committed snapshots, bounded queue, live API, independent failure, conflict, recovery and Session drain\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
