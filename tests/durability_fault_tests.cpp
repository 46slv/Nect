#include "host.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <atomic>
#include <future>
#include <iostream>
#include <map>

using namespace nect;
using namespace nect::desktop;
namespace {
using Phase=NativeWritePhase;
using Fault=NativeWriteFault;
int checks=0;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
QByteArray bytes(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Read owned fixture");return f.readAll();}
QByteArray hash(const QByteArray& value){return QCryptographicHash::hash(value,QCryptographicHash::Sha256).toHex();}
QString hash_file(const QString& path){return QString::fromLatin1(hash(bytes(path)));}
void put(const QString& path,const QByteArray& value){QFile f(path);check(f.open(QIODevice::WriteOnly|QIODevice::Truncate),"Open owned copy");check(f.write(value)==value.size(),"Write owned copy");}
void report(QJsonObject receipt){std::cout<<QJsonDocument(receipt).toJson(QJsonDocument::Compact).constData()<<'\n'<<std::flush;}
template<class F>void until(F predicate,const char* why,int timeout=8000){QElapsedTimer timer;timer.start();while(!predicate()&&timer.elapsed()<timeout)QTest::qWait(5);check(predicate(),why);}
template<class F>Error rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,"Expected explicit error code");return e;}throw std::runtime_error("Expected explicit refusal");}
QString phase_name(Phase p){switch(p){case Phase::before_open:return "before_open";case Phase::before_write:return "before_write";case Phase::after_write:return "after_write";case Phase::before_commit:return "before_commit";case Phase::after_commit:return "after_commit";case Phase::before_readback:return "before_readback";}return "unknown";}
QString fault_name(Fault f){switch(f){case Fault::none:return "none";case Fault::permission_denied:return "PERMISSION_DENIED";case Fault::no_space:return "NO_SPACE";case Fault::short_write:return "SHORT_WRITE";}return "unknown";}
Document geometry(double value){
    Session s(empty_document("durability-document","composition","artboard"));
    Point source;source.id="source-point";source.x.literal=value;source.y.literal=2;
    Point target;target.id="dependent-point";target.x.literal=8;target.y.literal=4;
    s.apply({CreatePath{"composition","","geometry","Geometry",{{"contour",false,{source,target}}}},
        Link{{"geometry","dependent-point","x"},{{"geometry","source-point","x"},2,7,"copy_local_value"}}},0);
    return s.document();
}
QByteArray encoded(const Document& document){return QByteArray::fromStdString(encode(document));}
void edit(Host& host,double value){host.session.apply({Set{{"geometry","source-point","x"},value}},host.session.revision());host.edited();}
void semantics(const Document& document,double value){
    check(document.id=="durability-document"&&document.compositions.front().id=="composition"&&document.compositions.front().artboards.front().id=="artboard","Stable document/composition/artboard identities");
    check(document.objects.at("geometry").contours.front().id=="contour","Stable object/contour source identity");
    const auto values=evaluate(document);check(values.at({"geometry","source-point","x"})==value&&values.at({"geometry","dependent-point","x"})==value*2+7,"Committed geometry and stable dependency survive native roundtrip");
    check(document.objects.at("geometry").contours.front().points[1].x.binding==geometry(value).objects.at("geometry").contours.front().points[1].x.binding,"Authored dependency source is preserved exactly");
}
std::map<QString,QByteArray> backups(const QString& path){std::map<QString,QByteArray> result;QDir dir(path+".backups");for(const auto& name:dir.entryList({"*.nect"},QDir::Files,QDir::Name))result.emplace(name,bytes(dir.filePath(name)));return result;}
QJsonArray backup_receipt(const QString& path){QJsonArray result;for(const auto& [name,value]:backups(path))result.append(QJsonObject{{"name",name},{"sha256",QString::fromLatin1(hash(value))}});return result;}
void retained(const std::map<QString,QByteArray>& before,const QString& path){const auto after=backups(path);for(const auto& [name,value]:before)check(after.contains(name)&&after.at(name)==value,"Failed write never alters or prunes an existing generation");}
struct Injection {
    QString target;Phase phase;Fault fault;bool enabled=true;int hits=0;
    Fault operator()(const QString& path,Phase at){if(enabled&&path==target&&phase==at){++hits;return fault;}return Fault::none;}
};
void refusals(const QString& root){
    const std::vector<std::pair<Phase,Fault>> cases{{Phase::before_open,Fault::permission_denied},{Phase::before_write,Fault::permission_denied},
        {Phase::before_write,Fault::no_space},{Phase::before_write,Fault::short_write},{Phase::before_commit,Fault::permission_denied},{Phase::before_commit,Fault::no_space}};
    int index=0;
    for(const auto& [phase,fault]:cases){
        const auto dir=root+"/refusal-"+QString::number(index++);check(QDir().mkpath(dir),"Create owned fault case");const auto path=dir+"/native.nect";
        auto stamp=store_native(path,encoded(geometry(0)),FileStamp{},true);
        for(int n=1;n<=12;++n)stamp=store_native(path,encoded(geometry(n)),stamp,true);
        const auto original=bytes(path);const auto before=backups(path);check(before.size()==10,"Start fault at full generation retention");
        auto injection=std::make_shared<Injection>(Injection{native_path(path),phase,fault});
        Host host(dir+"/recovery",nullptr,[injection](auto snapshot){ScopedNativeWriteHook hook([injection](auto p,auto at){return (*injection)(p,at);});return protect_snapshot(std::move(snapshot));});
        host.open(path);host.recover();edit(host,42);const auto committed=host.session.document();
        host.session.begin_gesture(host.session.revision());host.session.update_gesture({Set{{"geometry","source-point","x"},999}});
        until([&]{return host.persistence()["native_error"].toObject()["code"]=="IO_ERROR";},"Background writer reports injected refusal");
        const auto status=host.persistence();const auto recovery=status["recovery_file"].toString();
        check(injection->hits==1&&status["phase"]=="failed"&&status["saved_revision"].toInteger(-1)==0&&status["recovery_revision"].toInteger(-1)==1,"Actual writer refusal does not advance saved revision; independent recovery succeeds");
        check(status["native_error"].toObject()["code"]=="IO_ERROR"&&status["native_error"].toObject()["message"].toString().contains(fault_name(fault)),"Explicit fault class is returned through production protection result");
        if(fault==Fault::short_write)check(status["native_error"].toObject()["message"].toString().contains("staged "+QString::number(encoded(committed).size()/2)+" of "+QString::number(encoded(committed).size())),"Short-write injected bytes were actually written to temporary staging");
        check(bytes(path)==original&&load_native(path).stamp==stamp&&host.dirty(),"Authoritative bytes/hash and saved binding survive failure");
        check(load_native(recovery).document==committed&&host.session.gesture_active(),"Recovery excludes active gesture without canceling it");semantics(load_native(recovery).document,42);
        retained(before,path);check(backups(path).size()==11,"Failure retains ten old generations plus exact current baseline without pruning");
        const auto after_first=backups(path);host.recover();check(injection->hits==2&&backups(path)==after_first,"Retry refusal neither duplicates nor prunes generations");
        host.session.cancel_gesture();const auto recovery_bytes=bytes(recovery);const auto recovery_meta=QJsonDocument::fromJson(bytes(dir+"/recovery/"+host.session_id+".recovery.json")).object();
        {
            ScopedNativeWriteHook hook([injection](auto p,auto at){return (*injection)(p,at);});rejects("IO_ERROR",[&]{host.save(path);});
        }
        check(host.file_path==native_path(path)&&host.persistence()["saved_revision"].toInteger(-1)==0&&bytes(path)==original&&bytes(recovery)==recovery_bytes,"Explicit Save refusal keeps binding, saved receipt, native and recovery bytes");
        report({{"case","refusal"},{"scratch",dir},{"fault",fault_name(fault)},{"phase",phase_name(phase)},
            {"before_sha256",QString::fromLatin1(hash(original))},{"after_sha256",hash_file(path)},{"status",status},{"recovery_receipt",recovery_meta},{"generations",backup_receipt(path)}});
        // Save As succeeds even while the source-specific fault remains enabled.
        const auto copy=dir+"/save-as.nect";{
            ScopedNativeWriteHook hook([injection](auto p,auto at){return (*injection)(p,at);});host.save(copy);
        }
        host.recover();check(!host.dirty()&&bytes(copy)==encoded(committed)&&bytes(path)==original,"Save As saves committed work without touching failed source");
        check(QJsonDocument::fromJson(bytes(dir+"/recovery/"+host.session_id+".recovery.json")).object()["source_file"]==native_path(copy),"Recovery provenance follows successful Save As");
        Host retry(dir+"/retry-recovery",nullptr,[injection](auto snapshot){
            ScopedNativeWriteHook hook([injection](auto p,auto at){return (*injection)(p,at);});
            return protect_snapshot(std::move(snapshot));
        });
        retry.open(path);edit(retry,42);const auto prior_hits=injection->hits;retry.recover();
        check(injection->hits==prior_hits+1&&retry.persistence()["saved_revision"].toInteger(-1)==0&&retry.dirty(),"Retry fixture first observes the same fault on its still-bound original source");
        injection->enabled=false;{
            ScopedNativeWriteHook hook([injection](auto p,auto at){return (*injection)(p,at);});retry.save(path);
        }retry.recover();
        check(!retry.dirty()&&bytes(path)==encoded(committed)&&retry.persistence()["saved_revision"].toInteger(-1)==1&&backups(path).size()==10,"Fault removal permits same-Host retry without reopen/rebind and resumes bounded retention");
        report({{"case","refusal_resolved"},{"scratch",dir},{"fault",fault_name(fault)},{"phase",phase_name(phase)},{"retry_sha256",hash_file(path)},{"save_as_sha256",hash_file(copy)},{"status",retry.persistence()}});
    }
}
void hook_scope(const QString& root){
    int outer=0,inner=0;
    {
        ScopedNativeWriteHook first([&](const QString&,Phase){++outer;return Fault::none;});
        {
            ScopedNativeWriteHook second([&](const QString&,Phase){++inner;return Fault::none;});
            store_native(root+"/nested.nect",encoded(geometry(1)),FileStamp{},false);
        }
        check(inner==6&&outer==0,"Nested scope owns all six writer checkpoints");
        // A caller-thread hook must not leak into an unrelated writer thread.
        auto isolated=std::async(std::launch::async,[&]{return store_native(root+"/unhooked-thread.nect",encoded(geometry(1)),FileStamp{},false);});
        check(isolated.get().exists&&outer==0,"Hook ownership is thread-local");
        store_native(root+"/outer.nect",encoded(geometry(1)),FileStamp{},false);
        check(outer==6,"Nested hook restores previous scope");
    }
    store_native(root+"/unhooked.nect",encoded(geometry(1)),FileStamp{},false);
    check(outer==6&&inner==6,"Scope destruction removes callback completely");
}
void uncertain_readback(const QString& root){
    const auto path=root+"/readback.nect";const auto original=encoded(geometry(1));store_native(path,original,FileStamp{},true);
    auto injection=std::make_shared<Injection>(Injection{native_path(path),Phase::before_readback,Fault::permission_denied});
    Host host(root+"/readback-recovery",nullptr,[injection](auto snapshot){
        ScopedNativeWriteHook hook([injection](auto p,auto at){return (*injection)(p,at);});
        return protect_snapshot(std::move(snapshot));
    });
    host.open(path);host.recover();edit(host,2);const auto replacement=encoded(host.session.document());
    until([&]{return host.persistence()["native_error"].toObject()["code"]=="IO_VERIFY_FAILED";},"Background post-commit readback failure reaches Host");
    const auto status=host.persistence();
    check(injection->hits==1&&status["saved_revision"].toInteger(-1)==0&&status["recovery_revision"].toInteger(-1)==1&&host.dirty(),"Unverified native commit never advances Host saved revision; recovery remains independently verified");
    check(status["native_error"].toObject()["message"].toString().contains("may have succeeded"),"Unverified post-commit result explicitly reports uncertainty");
    check(bytes(path)==replacement&&backups(path).size()==1&&host.file_path==native_path(path),"Readback refusal follows actual replacement while retaining baseline backup and source binding");
    const auto recovery=status["recovery_file"].toString();check(bytes(recovery)==replacement,"Post-commit uncertainty does not lose committed recovery");
    semantics(load_native(path).document,2);
    injection->enabled=false;
    // The Host's old stamp is intentionally not replaced by an unverified claim.
    rejects("FILE_CHANGED",[&]{host.save(path);});
    check(host.persistence()["saved_revision"].toInteger(-1)==0&&bytes(path)==replacement&&bytes(recovery)==replacement,"Blind retry refuses stale expectation without damaging either verified candidate");
    const auto copy=root+"/readback-save-as.nect";host.save(copy);host.recover();
    check(!host.dirty()&&host.persistence()["saved_revision"].toInteger(-1)==1&&bytes(copy)==replacement&&bytes(path)==replacement,"Explicit Save As resolves uncertain native outcome without overwriting its source");
    Host reopened(root+"/readback-reopened-recovery");reopened.open(path);edit(reopened,3);reopened.save(path);reopened.recover();
    check(!reopened.dirty()&&reopened.persistence()["saved_revision"].toInteger(-1)==1,"Fresh reopen reestablishes verified stamp and permits ordinary retry");semantics(load_native(path).document,3);
    report({{"case","uncertain_readback"},{"phase","before_readback"},{"error","IO_VERIFY_FAILED"},{"before_sha256",QString::fromLatin1(hash(original))},
        {"after_commit_sha256",QString::fromLatin1(hash(replacement))},{"status_at_failure",status},{"save_as_sha256",hash_file(copy)},{"retry_sha256",hash_file(path)}});
}
struct OwnedProcess {
    QProcess process;
    ~OwnedProcess(){if(process.state()!=QProcess::NotRunning){process.kill();if(!process.waitForFinished(5000))std::cerr<<"FAIL: owned child cleanup did not finish\n";}}
    void start(const QStringList& args){process.start(QCoreApplication::applicationFilePath(),args);check(process.waitForStarted(5000),"Start only an owned durability subprocess");}
    void kill(){check(process.state()!=QProcess::NotRunning,"Owned child is still at its gate");process.kill();check(process.waitForFinished(5000)&&process.state()==QProcess::NotRunning,"Owned child termination and reap complete");}
    QJsonObject line(){QElapsedTimer timer;timer.start();while(!process.canReadLine()&&timer.elapsed()<12000&&process.state()!=QProcess::NotRunning)process.waitForReadyRead(100);check(process.canReadLine(),("Owned child did not report: "+process.readAllStandardError()).constData());QJsonParseError error;auto value=QJsonDocument::fromJson(process.readLine(),&error);check(error.error==QJsonParseError::NoError&&value.isObject(),"Owned child reports a structured receipt");return value.object();}
};
Phase crash_phase(const QString& name){for(const auto phase:{Phase::before_open,Phase::after_write,Phase::before_commit,Phase::after_commit})if(phase_name(phase)==name)return phase;throw std::runtime_error("Unknown crash phase");}
int crash_child(const QString& root,const QString& phase_text){
    const auto path=native_path(root+"/native.nect");const auto phase=crash_phase(phase_text);std::atomic<bool> entered=false;
    std::promise<void> release;auto gate=release.get_future().share();
    Host host(root+"/recovery",nullptr,[&](auto snapshot){ScopedNativeWriteHook hook([&](const QString& p,Phase at){if(p==path&&at==phase){entered=true;if(gate.wait_for(std::chrono::seconds(20))!=std::future_status::ready)throw Error("IO_ERROR","Owned crash gate timed out");}return Fault::none;});return protect_snapshot(std::move(snapshot));});
    host.open(path);host.recover();edit(host,42);const auto pending=host.persistence();
    host.session.begin_gesture(host.session.revision());host.session.update_gesture({Set{{"geometry","source-point","x"},999}});
    QElapsedTimer elapsed;elapsed.start();until([&]{return entered.load();},"Actual native writer reaches requested crash phase");
    const auto status=host.persistence();check(pending["pending_revision"].toInteger(-1)==1&&status["writing_revision"].toInteger(-1)==1&&status["saved_revision"].toInteger(-1)==0,"Crash child proves committed pending/writing revision is not acknowledged saved");
    const auto recovery=status["recovery_file"].toString();
    const auto metadata=QJsonDocument::fromJson(bytes(root+"/recovery/"+host.session_id+".recovery.json")).object();
    check(metadata["revision"].toInteger(-1)==1&&metadata["sha256"]==hash_file(recovery),"New committed recovery receipt is on disk before native gate");
    report({{"case","crash_ready"},{"scratch",root},{"pid",QCoreApplication::applicationPid()},{"phase",phase_text},{"pending_status",pending},{"status",status},
        {"observed_gap_revisions",1},{"edit_to_gate_ms",elapsed.elapsed()},{"committed_sha256",QString::fromLatin1(hash(encoded(host.session.document())))},{"native_sha256",hash_file(path)},{"recovery_sha256",hash_file(recovery)},{"recovery_receipt",metadata},{"generations",backup_receipt(path)}});
    // Parent owns termination; this watchdog also bounds lifetime if it disappears.
    QTimer::singleShot(25000,&host,[]{QCoreApplication::exit(66);});
    return QCoreApplication::exec();
}
int inspect_child(const QString& root,const QString& recovery,const QString& native_value){
    const auto path=root+"/native.nect";const auto native_before=bytes(path),recovery_before=bytes(recovery);
    const auto meta_path=recovery.chopped(5)+".recovery.json";const auto metadata=QJsonDocument::fromJson(bytes(meta_path)).object();
    const auto native=load_native(path),protected_copy=load_native(recovery);
    semantics(native.document,native_value.toDouble());semantics(protected_copy.document,42);
    check(metadata["revision"].toInteger(-1)==1&&metadata["document_id"]==QString::fromStdString(protected_copy.document.id)&&metadata["source_file"]==native_path(path)&&metadata["sha256"]==hash_file(recovery),"Fresh process validates recovery revision, source identity and exact bytes before selection");
    Host reopened(root+"/fresh-recovery");reopened.open_recovery(recovery);
    check(reopened.file_path.isEmpty()&&reopened.dirty()&&reopened.persistence()["saved_revision"].isNull()&&reopened.session.document()==protected_copy.document,"Recovered Session does not claim pending predecessor revision was saved");
    const auto copy=root+"/restored.nect";reopened.save(copy);reopened.recover();
    check(bytes(copy)==recovery_before&&bytes(path)==native_before&&bytes(recovery)==recovery_before,"Recovery Save As preserves both original candidates");
    report({{"case","fresh_process_readback"},{"selected_provenance",metadata},{"selected",recovery},{"native_sha256",hash_file(path)},{"restored_sha256",hash_file(copy)},{"status",reopened.persistence()}});return 0;
}
void crashes(const QString& root){
    for(const auto phase:{Phase::before_open,Phase::after_write,Phase::before_commit,Phase::after_commit}){
        const auto dir=root+"/crash-"+phase_name(phase);check(QDir().mkpath(dir),"Create isolated crash root");const auto path=dir+"/native.nect";
        Session intended(geometry(1));const auto original=encoded(intended.document());
        intended.apply({Set{{"geometry","source-point","x"},42}},0);const auto replacement=encoded(intended.document());
        store_native(path,original,FileStamp{},true);
        OwnedProcess child;child.start({"--crash-child",dir,phase_name(phase)});const auto ready=child.line();
        check(ready["case"]=="crash_ready"&&ready["pid"].toInteger()==child.process.processId()&&ready["phase"]==phase_name(phase),"Gate receipt identifies exact owned process and phase");
        check(ready["status"].toObject()["writing_revision"].toInteger(-1)==1&&ready["status"].toObject()["saved_revision"].toInteger(-1)==0,"Parent verifies no false durable claim before termination");
        report(ready);check(ready["committed_sha256"]==QString::fromLatin1(hash(replacement)),"Independent canonical command replay matches child committed payload before termination");
        child.kill();const auto after=bytes(path);
        report({{"case","crash_bytes"},{"phase",phase_name(phase)},{"actual_sha256",QString::fromLatin1(hash(after))},{"expected_sha256",QString::fromLatin1(hash(phase==Phase::after_commit?replacement:original))}});
        check(after==(phase==Phase::after_commit?replacement:original),"Actual store_native replacement leaves exact old-valid or new-valid authoritative bytes");
        const auto recovery=ready["status"].toObject()["recovery_file"].toString();
        OwnedProcess inspect;inspect.start({"--inspect-child",dir,recovery,phase==Phase::after_commit?"42":"1"});const auto result=inspect.line();
        check((inspect.process.state()==QProcess::NotRunning||inspect.process.waitForFinished(5000))&&inspect.process.exitStatus()==QProcess::NormalExit&&inspect.process.exitCode()==0,"Fresh readback process exits successfully");report(result);
        const auto observed=load_native(path);store_native(path,encoded(geometry(43)),observed.stamp,true);semantics(load_native(path).document,43);
        check(!QFileInfo::exists(path+".lock"),"Retry reclaims killed writer's stale lock and releases it");
        report({{"case","crash_reaped"},{"phase",phase_name(phase)},{"before_sha256",QString::fromLatin1(hash(original))},{"after_crash_sha256",QString::fromLatin1(hash(after))},{"retry_sha256",hash_file(path)},{"children_running",false}});
    }
}
void corrupt_backup_restore(const QString& root){
    const auto path=root+"/backup-native.nect";auto stamp=store_native(path,encoded(geometry(1)),FileStamp{},true);
    stamp=store_native(path,encoded(geometry(2)),stamp,true);store_native(path,encoded(geometry(3)),stamp,true);
    const auto original=bytes(path);const auto generations=backups(path);check(generations.size()==2,"Independent valid backup generations exist");
    const auto damaged_source=generations.begin();const auto chosen=std::prev(generations.end());
    const auto valid=QDir(path+".backups").filePath(chosen->first);const auto corrupt=root+"/corrupt-copy.nect";
    check(QFile::copy(QDir(path+".backups").filePath(damaged_source->first),corrupt),"Copy one backup before corruption, preserving another valid generation");
    put(corrupt,damaged_source->second.left(damaged_source->second.size()/2));
    Host host(root+"/restore-recovery");host.open(path);host.recover();const auto document=host.session.document();const auto status=host.persistence();
    bool rejected=false;std::string rejection;try{host.open_recovery(corrupt);}catch(const Error& error){rejected=true;rejection=error.code;}
    check(rejected&&!rejection.empty()&&host.session.document()==document&&host.file_path==native_path(path)&&host.persistence()==status,"Malformed backup copy is rejected before replacing Session, binding or receipts");
    check(bytes(path)==original&&backups(path)==generations,"Corrupt candidate rejection leaves every valid source/generation intact");
    host.open_recovery(valid);check(host.file_path.isEmpty()&&host.dirty(),"Valid generation restores as an unnamed document");semantics(host.session.document(),2);
    host.session.begin_gesture(host.session.revision());host.session.update_gesture({Set{{"geometry","source-point","x"},999}});host.recover();
    check(bytes(host.persistence()["recovery_file"].toString())==chosen->second&&host.session.gesture_active(),"Restored recovery excludes gesture preview and preserves exact authored source");host.session.cancel_gesture();
    const auto restored=root+"/backup-restored.nect";host.save(restored);host.recover();check(bytes(restored)==chosen->second&&bytes(path)==original&&backups(path)==generations,"Restore Save As preserves original and all retained generations");
    report({{"case","backup_restore"},{"corrupt_candidate",corrupt},{"corrupt_error",QString::fromStdString(rejection)},{"selected",valid},{"selected_sha256",hash_file(valid)},
        {"original_sha256",hash_file(path)},{"restored_sha256",hash_file(restored)},{"status",host.persistence()},{"generations",backup_receipt(path)}});
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()>1)try{if(args.size()==4&&args[1]=="--crash-child")return crash_child(args[2],args[3]);if(args.size()==5&&args[1]=="--inspect-child")return inspect_child(args[2],args[3],args[4]);throw std::runtime_error("Unexpected child arguments");}catch(const std::exception& e){std::cerr<<"FAIL child: "<<e.what()<<'\n';return 1;}
    QTemporaryDir temporary;
    try{
        check(temporary.isValid(),"Create owned scratch root");report({{"case","build"},{"source_base",NECT_DURABILITY_SOURCE_SHA},{"build",NECT_DURABILITY_BUILD},{"native_version",QString::fromUtf8(native_version)},{"scratch",temporary.path()}});
        hook_scope(temporary.path());refusals(temporary.path());uncertain_readback(temporary.path());crashes(temporary.path());corrupt_backup_restore(temporary.path());
        const auto root=temporary.path();check(temporary.remove()&&!QFileInfo::exists(root),"Explicit cleanup removes all owned files and crash staging remnants");
        report({{"case","complete"},{"checks",checks},{"cleanup",true},{"children_running",false},{"power_loss_measured",false}});return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';const auto root=temporary.path();const bool clean=temporary.remove()&&!QFileInfo::exists(root);std::cerr<<"cleanup="<<clean<<'\n';return 1;}
}
