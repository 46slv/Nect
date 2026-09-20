#include "storage.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace nect::desktop {
namespace {
constexpr qint64 native_limit=64*1024*1024;
constexpr auto backup_time_format="yyyyMMdd-HHmmsszzz";
struct ReadFile {QByteArray bytes;FileStamp stamp;};
FileStamp stamp(const QByteArray& bytes){return {true,QCryptographicHash::hash(bytes,QCryptographicHash::Sha256)};}
[[noreturn]] void io_error(const QString& message){throw Error("IO_ERROR",message.toStdString());}
ReadFile read_file(const QString& path,bool missing_allowed) {
    const QFileInfo info(path);
    if(!info.exists()) {
        if(missing_allowed)return {};
        io_error("Native file does not exist: "+path);
    }
    if(!info.isFile())io_error("Native path is not a regular file: "+path);
    QFile file(path);if(!file.open(QIODevice::ReadOnly))io_error(file.errorString());
    if(file.size()>native_limit)throw Error("INPUT_LIMIT","Native file exceeds 64 MiB");
    auto bytes=file.read(native_limit+1);
    if(file.error()!=QFileDevice::NoError)io_error(file.errorString());
    if(bytes.size()>native_limit)throw Error("INPUT_LIMIT","Native file exceeds 64 MiB");
    return {bytes,stamp(bytes)};
}
void write_atomic(const QString& path,const QByteArray& bytes) {
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly))io_error(file.errorString());
    if(file.write(bytes)!=bytes.size()) {const auto error=file.errorString();file.cancelWriting();io_error(error);}
    if(!file.commit())io_error(file.errorString());
}
QStringList owned_backups(const QDir& directory) {
    // Legacy timestamp/UUID names are intentionally outside this ownership set.
    static const QRegularExpression pattern("^[0-9]{8}-[0-9]{9}-[0-9a-f]{64}\\.nect$");
    QStringList result;
    for(const auto& name:directory.entryList(QDir::Files|QDir::NoSymLinks,QDir::Name))
        if(pattern.match(name).hasMatch())result.push_back(name);
    return result;
}
void keep_backup(const QString& path,const ReadFile& previous) {
    const auto directory_path=path+".backups";
    if(QFileInfo(directory_path).isSymLink())io_error("Backup directory must not be a symbolic link: "+directory_path);
    if(!QDir().mkpath(directory_path))io_error("Cannot create native backup directory: "+directory_path);
    QDir directory(directory_path);const auto owned=owned_backups(directory);
    const auto suffix="-"+QString::fromLatin1(previous.stamp.sha256.toHex())+".nect";
    for(const auto& name:owned)if(name.endsWith(suffix)) {
        if(read_file(directory.filePath(name),false).stamp!=previous.stamp)
            io_error("Existing backup content does not match its hash: "+directory.filePath(name));
        return; // Failed save retries must not manufacture duplicate generations.
    }
    auto time=QDateTime::currentDateTimeUtc();
    if(!owned.isEmpty()) {
        auto latest=QDateTime::fromString(owned.back().left(18),backup_time_format);latest.setTimeSpec(Qt::UTC);
        if(latest.isValid()&&time<=latest)time=latest.addMSecs(1);
    }
    const auto backup=directory.filePath(time.toString(backup_time_format)+suffix);
    write_atomic(backup,previous.bytes);
    if(read_file(backup,false).stamp!=previous.stamp)io_error("Backup readback verification failed: "+backup);
}
void prune_backups(const QString& path) {
    if(QFileInfo(path+".backups").isSymLink())return;
    QDir directory(path+".backups");const auto owned=owned_backups(directory);
    // Cleanup is best effort after verified replacement. A locked old backup
    // must not turn a successful save into an ambiguous failure or be forced off disk.
    for(qsizetype i=0;i<owned.size()-10;++i)try {
        const auto backup=directory.filePath(owned[i]);
        if(read_file(backup,false).stamp.sha256.toHex()==owned[i].chopped(5).right(64).toLatin1())
            (void)QFile::remove(backup);
    } catch(const std::exception&) {} // Preserve unreadable or externally modified generations.
}
}

QString native_path(const QString& path) {
    if(path.trimmed().isEmpty())io_error("Choose a native file path");
    const QFileInfo file(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    const auto existing=file.canonicalFilePath();if(!existing.isEmpty())return existing;
    const auto parent=QFileInfo(file.absolutePath()).canonicalFilePath();
    return parent.isEmpty()?file.absoluteFilePath():QDir(parent).filePath(file.fileName());
}

LoadedNative load_native(const QString& path) {
    auto file=read_file(native_path(path),false);
    auto document=decode(std::string_view(file.bytes.constData(),static_cast<std::size_t>(file.bytes.size())));
    return {std::move(document),std::move(file.stamp)};
}
bool same_native_path(const QString& first,const QString& second) {
    if(first.isEmpty()||second.isEmpty())return false;
#ifdef _WIN32
    return native_path(first).compare(native_path(second),Qt::CaseInsensitive)==0;
#else
    return native_path(first)==native_path(second);
#endif
}

FileStamp store_native(const QString& path,const QByteArray& bytes,const std::optional<FileStamp>& expected,bool keep_previous) {
    if(bytes.size()>native_limit)throw Error("OUTPUT_LIMIT","Native file exceeds 64 MiB");
    const auto target=native_path(path);
    if(!QFileInfo(QFileInfo(target).absolutePath()).isDir())io_error("Native parent directory does not exist: "+QFileInfo(target).absolutePath());
    QLockFile lock(target+".lock");lock.setStaleLockTime(0);
    if(!lock.tryLock(0))throw Error("FILE_LOCKED","Cannot acquire the native file save lock: "+target.toStdString());
    const auto previous=read_file(target,true);
    if(expected&&*expected!=previous.stamp)throw Error("FILE_CHANGED","The native file changed or was removed outside this session; reopen it or use explicit Save As");
    const auto next=stamp(bytes);
    if(previous.stamp==next)return next;
    if(keep_previous&&previous.stamp.exists)keep_backup(target,previous);
    QSaveFile file(target);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly))io_error(file.errorString());
    if(file.write(bytes)!=bytes.size()){const auto error=file.errorString();file.cancelWriting();io_error(error);}
    // The sidecar protects cooperating writers; this second content check also
    // catches external edits made while the backup/staging write was in flight.
    if(read_file(target,true).stamp!=previous.stamp) {
        file.cancelWriting();throw Error("FILE_CHANGED","The native file changed while the replacement was being prepared");
    }
    if(!file.commit())io_error(file.errorString());
    try {
        if(read_file(target,false).stamp!=next)throw Error("IO_ERROR","Saved content hash differs");
    } catch(const std::exception& error) {
        throw Error("IO_VERIFY_FAILED","Native replacement may have succeeded; readback could not be verified: "+std::string(error.what()));
    }
    if(keep_previous)prune_backups(target);
    return next;
}
} // namespace nect::desktop
