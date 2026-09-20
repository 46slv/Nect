#include "protection.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <algorithm>
#include <iostream>
#include <limits>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);++checks;}
QByteArray read(const QString& path) {
    QFile file(path);check(file.open(QIODevice::ReadOnly),"Read protection fixture");return file.readAll();
}
void write(const QString& path,const QByteArray& bytes) {
    QFile file(path);check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Open external protection fixture");
    check(file.write(bytes)==bytes.size(),"Write external protection fixture");file.close();
}
QByteArray digest(const QByteArray& bytes){return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();}
QString session_id(int number){return QString("10000000-0000-4000-8000-%1").arg(number,12,16,QChar('0'));}
QString base(const ProtectionSnapshot& snapshot){return QDir(snapshot.recovery_directory).filePath(snapshot.session_id);}
QByteArray encoded(const Document& document){return QByteArray::fromStdString(encode(document));}
ProtectionSnapshot snapshot(const QString& directory,int number) {
    ProtectionSnapshot result;result.document=demo_document();result.session_id=session_id(number);
    result.recovery_directory=directory+"/recovery";result.revision=17;
    return result;
}
void age_receipt(const QString& path,int order) {
    QFile file(path);check(file.open(QIODevice::ReadWrite),"Open owned recovery receipt to set fixture age");
    check(file.setFileTime(QDateTime::fromSecsSinceEpoch(1600000000+order,Qt::UTC),QFileDevice::FileModificationTime),
        "Set deterministic recovery age without sleeps");
}
QStringList backups(const QString& path){return QDir(path+".backups").entryList({"*.nect"},QDir::Files,QDir::Name);}

void exact_receipts(const QString& directory) {
    auto work=snapshot(directory,1);work.native_file=directory+"/saved.nect";work.write_native=true;
    work.expected_native=FileStamp{};work.native_backup=work.recovery_backup=true;
    Session live(work.document);live.apply({Link{{"path-B","point-B1","x"},{{"path-A","point-A1","x"},2,7,"copy_local_value"}}},0);
    work.document=live.document();const auto committed=work.document;
    live.apply({Set{{"path-A","point-A1","x"},432}},1);
    auto result=protect_snapshot(work);
    check(result.native_written&&result.recovery_written&&result.native_error.empty()&&result.recovery_error.empty(),"Both destinations independently report successful verified protection");
    check(result.session_id==work.session_id&&result.native_file==work.native_file&&result.revision==17&&result.native_backup&&result.recovery_backup,
        "Worker receipt preserves session, path, revision and requested backup policies");
    check(load_native(work.native_file).document==committed&&load_native(base(work)+".nect").document==committed,
        "Worker writes exactly the supplied committed snapshot, including bindings, without reading a later live Session");
    const auto bytes=read(work.native_file);
    check(bytes==encoded(committed)&&read(base(work)+".nect")==bytes&&result.native_stamp==load_native(work.native_file).stamp,
        "Native and recovery bytes and native stamp agree with the exact immutable snapshot");
    const auto receipt=QJsonDocument::fromJson(read(base(work)+".recovery.json")).object();
    check(receipt["format"]=="nect-recovery-1"&&receipt["session_id"]==work.session_id&&receipt["document_id"]==QString::fromStdString(committed.id)&&
        receipt["revision"].toInteger()==17&&receipt["source_file"]==work.native_file&&receipt["sha256"].toString().toLatin1()==digest(bytes)&&
        QDateTime::fromString(receipt["written_utc"].toString(),Qt::ISODateWithMs).isValid(),
        "Recovery metadata identifies its exact bytes, document, revision, source path and valid UTC write time");
    check(result.recovery_lease!=nullptr,"Successful protection returns its active-session recovery lease");
    QLockFile competing(base(work)+".active.lock");competing.setStaleLockTime(0);
    check(!competing.tryLock(0),"Returned active lease prevents another owner from acquiring this recovery session");

    work.recovery_lease=result.recovery_lease;work.expected_native=result.native_stamp;
    std::vector<QByteArray> previous{bytes};
    for(int i=0;i<3;++i) {
        work.document.objects.at("path-A").name="Retained "+std::to_string(i);++work.revision;
        result=protect_snapshot(work);check(result.native_written&&result.recovery_written,"Repeated committed snapshots protect both destinations");
        check(result.recovery_lease==work.recovery_lease,"Repeated protection retains the same active lease identity");
        work.expected_native=result.native_stamp;previous.push_back(encoded(work.document));
    }
    const auto native_backups=backups(work.native_file),recovery_backups=backups(base(work)+".nect");
    check(native_backups.size()==3&&recovery_backups.size()==3,"Native and recovery generations are retained independently");
    for(int i=0;i<3;++i)check(read(QDir(work.native_file+".backups").filePath(native_backups[i]))==previous[static_cast<std::size_t>(i)]&&
        read(QDir(base(work)+".nect.backups").filePath(recovery_backups[i]))==previous[static_cast<std::size_t>(i)],
        "Each destination backup preserves the exact previous snapshot bytes");
    ++work.revision;result=protect_snapshot(work);
    check(result.native_written&&result.recovery_written&&backups(work.native_file).size()==3&&backups(base(work)+".nect").size()==3,
        "A revision receipt without changed document bytes does not duplicate backup content");
    check(QJsonDocument::fromJson(read(base(work)+".recovery.json")).object()["revision"].toInteger()==static_cast<qint64>(work.revision),
        "Recovery metadata advances even when authored bytes are identical");
}

void independent_failures(const QString& directory) {
    auto work=snapshot(directory,2);work.native_file=directory+"/independent.nect";work.write_native=true;work.expected_native=FileStamp{};
    auto result=protect_snapshot(work);check(result.native_written&&result.recovery_written,"Create dual-destination failure fixture");
    work.expected_native=result.native_stamp;work.recovery_lease=result.recovery_lease;
    auto external=work.document;external.objects.at("path-A").name="External native edit";const auto external_bytes=encoded(external);write(work.native_file,external_bytes);
    work.document.objects.at("path-A").name="New committed snapshot";++work.revision;
    result=protect_snapshot(work);
    check(!result.native_written&&result.native_error.code=="FILE_CHANGED"&&result.recovery_written&&result.recovery_error.empty(),
        "External native conflict does not prevent independent recovery protection");
    check(read(work.native_file)==external_bytes&&load_native(base(work)+".nect").document==work.document,
        "Native conflict preserves external bytes while recovery records the newer committed snapshot");
    work.expected_native=load_native(work.native_file).stamp;
    {
        QLockFile locked(native_path(work.native_file)+".lock");locked.setStaleLockTime(0);check(locked.tryLock(0),"Lock current native independently");
        result=protect_snapshot(work);
        check(!result.native_written&&result.native_error.code=="FILE_LOCKED"&&result.recovery_written,"Locked native still permits recovery success");
    }
    auto unavailable=snapshot(directory,3);unavailable.native_file=work.native_file;unavailable.write_native=true;
    unavailable.expected_native=load_native(work.native_file).stamp;unavailable.document.objects.at("path-A").name="Native still succeeds";
    unavailable.recovery_directory=directory+"/not-a-directory";write(unavailable.recovery_directory,"blocked");
    result=protect_snapshot(unavailable);
    check(result.native_written&&result.native_error.empty()&&!result.recovery_written&&result.recovery_error.code=="IO_ERROR"&&
        load_native(work.native_file).document==unavailable.document,"Unavailable recovery directory does not prevent verified native saving");

    auto metadata_failure=snapshot(directory,4);metadata_failure.write_native=true;metadata_failure.native_file=directory+"/metadata-independent.nect";
    metadata_failure.expected_native=FileStamp{};
    check(QDir().mkpath(base(metadata_failure)+".recovery.json"),"Block recovery receipt with an owned directory");
    result=protect_snapshot(metadata_failure);
    check(result.native_written&&!result.recovery_written&&!result.recovery_error.empty()&&
        load_native(base(metadata_failure)+".nect").document==metadata_failure.document,
        "Receipt failure is not reported as complete recovery protection even if recovery data bytes were written; native still succeeds");

    auto invalid=snapshot(directory,5);invalid.write_native=true;invalid.native_file=work.native_file;
    invalid.document.objects.at("path-A").contours.front().points.front().x.literal=std::numeric_limits<double>::infinity();
    const auto before=read(work.native_file);result=protect_snapshot(invalid);
    check(!result.native_written&&!result.recovery_written&&!result.native_error.empty()&&result.native_error.code==result.recovery_error.code&&
        read(work.native_file)==before&&!QFileInfo::exists(base(invalid)+".nect"),"Encoding failure produces errors for requested destinations without writing either file");
}

void retention(const QString& directory) {
    const auto root=directory+"/retention";check(QDir().mkpath(root),"Create retention fixture root");
    auto active=snapshot(directory,100);active.recovery_directory=root;
    auto active_result=protect_snapshot(active);check(active_result.recovery_written,"Create leased active recovery fixture");
    age_receipt(base(active)+".recovery.json",0);const auto active_bytes=read(base(active)+".nect");
    auto modified=snapshot(directory,101);modified.recovery_directory=root;
    auto modified_result=protect_snapshot(modified);check(modified_result.recovery_written,"Create externally changed recovery fixture");
    modified_result.recovery_lease.reset();age_receipt(base(modified)+".recovery.json",1);
    const auto external="\n"+encoded(modified.document);write(base(modified)+".nect",external);
    const auto legacy=QDir(root).filePath(session_id(102)+".nect");write(legacy,encoded(active.document));
    const auto unrecognized=QDir(root).filePath("user-saved-copy.nect");write(unrecognized,"User copy remains outside managed retention");
    const auto foreign_base=QDir(root).filePath(session_id(103));const auto foreign_bytes=encoded(active.document);
    write(foreign_base+".nect",foreign_bytes);
    write(foreign_base+".recovery.json",QJsonDocument(QJsonObject{{"format","different-owner"},{"session_id",session_id(103)},
        {"sha256",QString::fromLatin1(digest(foreign_bytes))}}).toJson(QJsonDocument::Compact));

    std::vector<QString> inactive;
    QString saved_modified_backup,legacy_backup,owned_backup;
    for(int i=0;i<24;++i) {
        auto work=snapshot(directory,200+i);work.recovery_directory=root;work.document.objects.at("path-A").name="Inactive "+std::to_string(i);
        auto result=protect_snapshot(work);check(result.recovery_written&&result.native_error.empty()&&!result.native_written,"Recovery-only snapshot reports no native write or native error");
        result.recovery_lease.reset();age_receipt(base(work)+".recovery.json",100+i);inactive.push_back(base(work));
        if(i==0) {
            const auto versions=base(work)+".nect.backups";check(QDir().mkpath(versions),"Create retention backup ownership fixtures");
            const auto old=encoded(active.document);
            owned_backup=versions+"/20260920-010101001-"+QString::fromLatin1(digest(old))+".nect";write(owned_backup,old);
            saved_modified_backup=versions+"/20260920-010101002-"+QString::fromLatin1(digest(old))+".nect";write(saved_modified_backup,"Externally changed backup");
            legacy_backup=versions+"/old-uuid-backup.nect";write(legacy_backup,"Legacy backup");
        }
    }
    auto trigger=snapshot(directory,999);trigger.recovery_directory=root;auto trigger_result=protect_snapshot(trigger);
    check(trigger_result.recovery_written,"Current leased snapshot triggers bounded inactive retention");
    const auto remaining=std::count_if(inactive.begin(),inactive.end(),[](const auto& name){return QFileInfo::exists(name+".nect");});
    check(remaining==20,"Retention keeps twenty inactive managed recoveries, independently of active or unrecognized files");
    for(int i=0;i<24;++i)check(QFileInfo::exists(inactive[static_cast<std::size_t>(i)]+".nect")== (i>=4)&&
        QFileInfo::exists(inactive[static_cast<std::size_t>(i)]+".recovery.json")== (i>=4),
        "Retention removes oldest eligible data and receipt pairs by actual receipt age");
    check(read(base(active)+".nect")==active_bytes&&QFileInfo::exists(base(active)+".recovery.json")&&
        read(base(modified)+".nect")==external&&QFileInfo::exists(base(modified)+".recovery.json"),
        "Old active-lease recovery and externally modified recovery are preserved outside the inactive retention count");
    check(read(legacy)==encoded(active.document)&&read(unrecognized)=="User copy remains outside managed retention"&&read(foreign_base+".nect")==foreign_bytes,
        "Legacy files, unknown filenames and foreign metadata are never automatically pruned");
    check(!QFileInfo::exists(owned_backup)&&read(saved_modified_backup)=="Externally changed backup"&&read(legacy_backup)=="Legacy backup",
        "Pruning an inactive recovery removes only hash-matching managed backup generations and preserves modified or legacy backup content");
    check(QFileInfo::exists(base(trigger)+".nect")&&active_result.recovery_lease&&trigger_result.recovery_lease,"Current and older live leases both remain protected");
    std::cout<<"Recovery retention fixture: 24 small inactive snapshots, 20 retained; active leases, legacy/foreign files and modified bytes preserved. 128 MiB byte threshold is not stress-tested here.\n";
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        QTemporaryDir temporary;check(temporary.isValid(),"Create owned protection test directory");
        exact_receipts(temporary.path());independent_failures(temporary.path());retention(temporary.path());
        std::cout<<"PASS "<<checks<<" snapshot protection checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
