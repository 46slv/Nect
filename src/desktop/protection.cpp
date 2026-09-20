#include "protection.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>

namespace nect::desktop {
namespace {
StorageError failure(const std::exception& error) {
    const auto* typed=dynamic_cast<const Error*>(&error);
    return {typed?QString::fromStdString(typed->code):QString("IO_ERROR"),QString::fromUtf8(error.what())};
}
QByteArray read_bytes(const QString& path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>8*1024*1024)return {};
    return file.readAll();
}
QByteArray digest(const QByteArray& bytes) {return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();}
// Only files created with a matching receipt are eligible. Legacy, externally
// modified and active-session recovery files are never automatically removed.
void prune_recovery(const QString& directory) {
    QLockFile pruning(QDir(directory).filePath(".retention.lock"));pruning.setStaleLockTime(0);
    if(!pruning.tryLock(0))return;
    struct Entry {QString base,metadata;QDateTime modified;qint64 size=0;};
    std::vector<Entry> entries;
    const QDir dir(directory);
    const QRegularExpression uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    const QRegularExpression backup("^[0-9]{8}-[0-9]{9}-[0-9a-f]{64}\\.nect$");
    for(const auto& info:dir.entryInfoList({"*.recovery.json"},QDir::Files,QDir::Time)) {
        if(info.isSymLink())continue;
        const auto session=info.fileName().chopped(QString(".recovery.json").size());
        if(!uuid.match(session).hasMatch())continue;
        const auto meta=QJsonDocument::fromJson(read_bytes(info.absoluteFilePath())).object();
        const auto base=dir.filePath(session);
        if(meta["format"]!="nect-recovery-1"||meta["session_id"]!=session||QFileInfo(base+".nect").isSymLink()||
           meta["sha256"].toString().toLatin1()!=digest(read_bytes(base+".nect")))continue;
        QLockFile active(base+".active.lock");active.setStaleLockTime(0);
        if(!active.tryLock(0))continue;
        Entry entry{base,info.absoluteFilePath(),info.lastModified(),QFileInfo(base+".nect").size()+info.size()};
        const QDir versions(base+".nect.backups");
        if(QFileInfo(versions.absolutePath()).isSymLink())continue;
        for(const auto& version:versions.entryInfoList({"*.nect"},QDir::Files))
            if(!version.isSymLink()&&backup.match(version.fileName()).hasMatch())entry.size+=version.size();
        entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.modified>b.modified;});
    qint64 retained=0;std::size_t count=0;
    for(const auto& entry:entries) {
        retained+=entry.size;++count;
        if(count<=20&&retained<=128*1024*1024)continue;
        QLockFile active(entry.base+".active.lock"),writing(entry.base+".nect.lock");
        active.setStaleLockTime(0);writing.setStaleLockTime(0);
        if(!active.tryLock(0)||!writing.tryLock(0))continue;
        const auto meta=QJsonDocument::fromJson(read_bytes(entry.metadata)).object();
        if(meta["sha256"].toString().toLatin1()!=digest(read_bytes(entry.base+".nect")))continue;
        if(!QFile::remove(entry.base+".nect"))continue;
        QFile::remove(entry.metadata);
        const QDir versions(entry.base+".nect.backups");
        for(const auto& version:versions.entryInfoList({"*.nect"},QDir::Files)) {
            if(version.isSymLink()||!backup.match(version.fileName()).hasMatch())continue;
            if(version.fileName().chopped(5).right(64).toLatin1()==digest(read_bytes(version.absoluteFilePath())))
                QFile::remove(version.absoluteFilePath());
        }
        QDir().rmdir(versions.absolutePath()); // succeeds only when empty
    }
}
}
ProtectionResult protect_snapshot(ProtectionSnapshot snapshot) {
    ProtectionResult result;result.session_id=snapshot.session_id;result.native_file=snapshot.native_file;
    result.revision=snapshot.revision;result.recovery_lease=std::move(snapshot.recovery_lease);
    QByteArray bytes;
    try {
        bytes=QByteArray::fromStdString(encode(snapshot.document));
        if(bytes.size()>8*1024*1024)throw Error("OUTPUT_LIMIT","Native file exceeds 8 MiB");
    } catch(const std::exception& error) {
        result.recovery_error=failure(error);if(snapshot.write_native)result.native_error=failure(error);return result;
    }
    try {
        if(!QDir().mkpath(snapshot.recovery_directory))throw Error("IO_ERROR","Cannot create recovery directory");
        const auto base=QDir(snapshot.recovery_directory).filePath(snapshot.session_id);
        if(!result.recovery_lease) {
            auto lease=std::make_shared<QLockFile>(base+".active.lock");lease->setStaleLockTime(0);
            if(!lease->tryLock(0))throw Error("FILE_LOCKED","Recovery session is in use");
            result.recovery_lease=std::move(lease);
        }
        store_native(base+".nect",bytes,std::nullopt,snapshot.recovery_backup);
        const QJsonObject receipt{{"format","nect-recovery-1"},{"session_id",snapshot.session_id},
            {"document_id",QString::fromStdString(snapshot.document.id)},{"revision",static_cast<qint64>(snapshot.revision)},
            {"source_file",snapshot.native_file},{"sha256",QString::fromLatin1(digest(bytes))},
            {"written_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        store_native(base+".recovery.json",QJsonDocument(receipt).toJson(QJsonDocument::Compact),std::nullopt,false);
        result.recovery_written=true;result.recovery_backup=snapshot.recovery_backup;
        prune_recovery(snapshot.recovery_directory);
    } catch(const std::exception& error) {result.recovery_error=failure(error);}
    // A failure in one destination must not prevent protecting the other.
    if(snapshot.write_native)try {
        result.native_stamp=store_native(snapshot.native_file,bytes,snapshot.expected_native,snapshot.native_backup);
        result.native_written=true;result.native_backup=snapshot.native_backup;
    } catch(const std::exception& error) {result.native_error=failure(error);}
    return result;
}
}
