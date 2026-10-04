#include "folder_library.hpp"

#include "nect/io.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace nect::desktop {
namespace {
constexpr qint64 max_preset_asset_file_bytes=512*1024;
constexpr int max_preset_assets=256;
constexpr int max_workspace_assets=256;

QString normalize_unicode(QString value) {
    return value.normalized(QString::NormalizationForm_C);
}

QString path_key(QString path, bool case_sensitive) {
    path = normalize_unicode(QDir::cleanPath(QDir::fromNativeSeparators(path)));
    return case_sensitive ? path : normalize_unicode(path.toCaseFolded());
}

QString canonical_existing_path(const QString& path) {
#ifdef Q_OS_WIN
    const auto native = QDir::toNativeSeparators(path);
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return {};
    const auto required = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (required == 0) {
        CloseHandle(handle);
        return {};
    }
    std::wstring buffer(static_cast<size_t>(required) + 1, L'\0');
    const auto written = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(handle);
    if (written == 0 || written >= buffer.size()) return {};
    auto resolved = QString::fromWCharArray(buffer.data(), static_cast<int>(written));
    if (resolved.startsWith(QStringLiteral("\\\\?\\UNC\\"), Qt::CaseInsensitive))
        resolved = QStringLiteral("//") + resolved.mid(8);
    else if (resolved.startsWith(QStringLiteral("\\\\?\\")))
        resolved.remove(0, 4);
    return QDir::cleanPath(QDir::fromNativeSeparators(resolved));
#else
    return QFileInfo(path).canonicalFilePath();
#endif
}

bool is_reparse_point(const QString& path) {
#ifdef Q_OS_WIN
    const auto native = QDir::toNativeSeparators(path);
    const auto attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    return QFileInfo(path).isSymLink();
#endif
}

bool root_case_sensitive(const QString& canonical_path) {
#ifdef Q_OS_WIN
    const auto native = QDir::toNativeSeparators(canonical_path);
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    struct CaseSensitiveInfo { ULONG Flags; } info{};
    constexpr auto file_case_sensitive_info = static_cast<FILE_INFO_BY_HANDLE_CLASS>(23);
    const bool queried = GetFileInformationByHandleEx(handle, file_case_sensitive_info, &info, sizeof(info)) != 0;
    CloseHandle(handle);
    return queried && (info.Flags & 0x1u) != 0;
#else
    (void)canonical_path;
    return true;
#endif
}

QString parent_path(const QString& relative_path) {
    const auto slash = relative_path.lastIndexOf('/');
    return slash < 0 ? QString{} : relative_path.left(slash);
}

QString leaf_name(const QString& relative_path) {
    if (relative_path.isEmpty()) return {};
    const auto slash = relative_path.lastIndexOf('/');
    return slash < 0 ? relative_path : relative_path.mid(slash + 1);
}

QString error_message(const Error& error) {
    return QString::fromStdString(error.code) + ": " + QString::fromUtf8(error.what());
}

bool canonical_asset_id(const QString& asset_id) {
    if(asset_id.isEmpty())return false;
    const QUuid uuid(asset_id);
    return !uuid.isNull()&&uuid.toString(QUuid::WithoutBraces)==asset_id;
}

struct StoredPresetAsset {
    LibraryPresetAssetV1 metadata;
    PortablePresetClosure closure;
    QByteArray raw_envelope;
    QByteArray payload;
};

struct StoredMacroAsset {
    LibraryMacroAssetV1 metadata;
    MacroDefinition definition;
    QByteArray raw_envelope;
    QByteArray payload;
};

int workspace_asset_file_count(const QString& root) {
    return QDir(root).entryInfoList({"*.preset.json","*.macro.json"},
        QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDir::Name).size();
}

QByteArray checked_file_bytes(const QString& root,const QString& path);
std::unique_ptr<QLockFile> lock_preset_asset_root(const QString& root);

[[noreturn]] void throw_macro_error(const Error& error) {
    auto code=error.code;
    for(std::size_t at=0;(at=code.find("PRESET",at))!=std::string::npos;at+=5)code.replace(at,6,"MACRO");
    auto detail=std::string(error.what());
    for(std::size_t at=0;(at=detail.find("Preset",at))!=std::string::npos;at+=5)detail.replace(at,6,"Macro");
    throw Error(std::move(code),std::move(detail));
}

QByteArray checked_macro_file_bytes(const QString& root,const QString& path) {
    try {return checked_file_bytes(root,path);}
    catch(const Error& error) {throw_macro_error(error);}
}

std::unique_ptr<QLockFile> lock_macro_asset_root(const QString& root) {
    try {return lock_preset_asset_root(root);}
    catch(const Error& error) {throw_macro_error(error);}
}

QByteArray preset_asset_envelope(const LibraryPresetAssetV1& metadata,const QByteArray& payload) {
    QJsonObject envelope{
        {"version",1},{"kind","preset_definition"},{"asset_id",metadata.ref.asset_id},
        {"accepted_revision",static_cast<double>(metadata.accepted_revision)},
        {"sha256",metadata.sha256},{"payload_schema",static_cast<int>(metadata.payload_schema)},
        {"payload",QString::fromUtf8(payload.constData(),payload.size())},{"label",metadata.label}};
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

QByteArray checked_file_bytes(const QString& root,const QString& path) {
    const QFileInfo root_info(root),info(path);
    if(!root_info.exists())throw Error("MISSING_PRESET_ASSET","Preset Library payload root or exact asset is missing");
    if(is_reparse_point(root)||!root_info.isDir())
        throw Error("UNSAFE_PRESET_LIBRARY_ROOT","Preset Library payload root is missing or is a reparse point");
    const auto canonical_root=canonical_existing_path(root);
    if(canonical_root.isEmpty()||path_key(canonical_root,false)!=path_key(QDir::cleanPath(root),false))
        throw Error("UNSAFE_PRESET_LIBRARY_ROOT","Preset Library payload root does not resolve to its owned path");
    if(!info.exists())throw Error("MISSING_PRESET_ASSET","The exact Preset Library asset file is missing");
    if(is_reparse_point(path)||!info.isFile())
        throw Error("UNSAFE_PRESET_ASSET_PATH","Preset Library asset must be a regular immediate file");
    const auto canonical_file=canonical_existing_path(path);
    const auto canonical_parent=QFileInfo(canonical_file).dir().canonicalPath();
    if(canonical_file.isEmpty()||canonical_parent.isEmpty()||path_key(canonical_parent,false)!=path_key(canonical_root,false))
        throw Error("UNSAFE_PRESET_ASSET_PATH","Preset Library asset does not resolve as a direct child of its owned root");
    if(info.size()<0||info.size()>max_preset_asset_file_bytes)
        throw Error("PRESET_ASSET_LIMIT","Preset Library asset file exceeds the 512 KiB limit");
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))throw Error("PRESET_ASSET_READ_FAILED","Preset Library asset could not be opened");
    const auto size=file.size();
    if(size<0||size>max_preset_asset_file_bytes)
        throw Error("PRESET_ASSET_LIMIT","Preset Library asset file exceeds the 512 KiB limit");
    const auto bytes=file.read(max_preset_asset_file_bytes+1);
    if(bytes.size()>max_preset_asset_file_bytes)
        throw Error("PRESET_ASSET_LIMIT","Preset Library asset file grew beyond the 512 KiB limit while reading");
    if(file.error()!=QFileDevice::NoError||bytes.size()!=size)
        throw Error("PRESET_ASSET_READ_FAILED","Preset Library asset could not be read completely");
    return bytes;
}

StoredPresetAsset read_stored_preset_asset(const QString& root,const QString& asset_id) {
    if(!canonical_asset_id(asset_id))throw Error("INVALID_PRESET_ASSET_ID","Preset Library needs the exact canonical AssetID");
    const auto path=QDir(root).filePath(asset_id+QStringLiteral(".preset.json"));
    const auto raw=checked_file_bytes(root,path);
    const std::string_view envelope_bytes(raw.constData(),static_cast<std::size_t>(raw.size()));
    const auto envelope=read_portable_preset_asset_envelope(envelope_bytes);
    if(envelope.version!=1||envelope.kind!="preset_definition")
        throw Error("UNSUPPORTED_PRESET_ASSET_VERSION","Preset asset envelope kind or version is unsupported");
    if(QString::fromStdString(envelope.asset_id)!=asset_id)
        throw Error("PRESET_ASSET_ID_MISMATCH","Preset asset envelope AssetID does not match the exact requested file identity");
    const auto revision=envelope.accepted_revision;
    const auto schema=envelope.payload_schema;
    if(schema!=1&&schema!=2&&schema!=3)
        throw Error("UNSUPPORTED_PRESET_SCHEMA","Preset payload schema is not supported by this build");
    const auto& payload_text=envelope.payload;
    const QByteArray payload(payload_text.data(),static_cast<qsizetype>(payload_text.size()));
    if(payload.size()>static_cast<qint64>(portable_preset_payload_limit))
        throw Error("PRESET_PAYLOAD_LIMIT","Portable Preset payload exceeds 256 KiB");
    const auto& claimed_hash=envelope.sha256;
    const auto actual_hash=QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex();
    if(QString::fromStdString(claimed_hash)!=QString::fromLatin1(actual_hash))
        throw Error("PRESET_ASSET_HASH_MISMATCH","Preset asset payload SHA-256 does not match its envelope");
    PortablePresetClosure closure;
    try {
        closure=read_canonical_preset_closure_payload(
            std::string_view(payload.constData(),static_cast<std::size_t>(payload.size())),static_cast<unsigned>(schema));
        if(QByteArray::fromStdString(canonical_preset_closure_payload(closure))!=payload)
            throw Error("NONCANONICAL_PRESET_PAYLOAD","Preset asset payload must use exact canonical bytes");
    }
    catch(const Error& error) {
        if(error.code=="UNSUPPORTED_PRESET_SCHEMA"||error.code=="PRESET_PAYLOAD_LIMIT")throw;
        if(error.code=="PRESET_SCHEMA_MISMATCH")
            throw Error("PRESET_ASSET_ENVELOPE_MISMATCH","Preset asset schema does not match its canonical payload");
        throw Error("UNAVAILABLE_PRESET_ASSET",std::string("Preset asset payload is unavailable: ")+error.code+": "+error.what());
    } catch(const std::exception& error) {
        throw Error("UNAVAILABLE_PRESET_ASSET",std::string("Preset asset payload is malformed: ")+error.what());
    }
    if(portable_preset_closure_schema(closure)!=schema||QString::fromStdString(closure.definition.label)!=QString::fromStdString(envelope.label))
        throw Error("PRESET_ASSET_ENVELOPE_MISMATCH","Preset asset label or schema does not match its canonical payload");
    if(closure.definition.id==asset_id.toStdString()||closure.macro_definitions.contains(asset_id.toStdString()))
        throw Error("PRESET_ASSET_ID_MISMATCH","Workspace AssetID must be distinct from source Document definition IDs");
    validate_portable_preset_closure(closure);
    LibraryPresetAssetV1 metadata{{asset_id},QString::fromStdString(envelope.label),revision,
        QString::fromStdString(claimed_hash),static_cast<unsigned>(schema),true,{}};
    return {std::move(metadata),std::move(closure),raw,payload};
}

QByteArray macro_asset_envelope(const LibraryMacroAssetV1& metadata,const QByteArray& payload) {
    QJsonObject envelope{{"version",1},{"kind","macro_definition"},{"asset_id",metadata.ref.asset_id},
        {"accepted_revision",static_cast<double>(metadata.accepted_revision)},
        {"sha256",metadata.sha256},{"payload_schema",static_cast<int>(metadata.payload_schema)},
        {"payload",QString::fromUtf8(payload.constData(),payload.size())},{"label",metadata.label}};
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

StoredMacroAsset read_stored_macro_asset(const QString& root,const QString& asset_id) {
    if(!canonical_asset_id(asset_id))throw Error("INVALID_MACRO_ASSET_ID","Macro Library needs the exact canonical AssetID");
    const auto path=QDir(root).filePath(asset_id+QStringLiteral(".macro.json"));
    QByteArray raw;
    try {raw=checked_file_bytes(root,path);}
    catch(const Error& error) {throw_macro_error(error);}
    PortablePresetAssetEnvelope envelope;
    try {envelope=read_portable_preset_asset_envelope(std::string_view(raw.constData(),static_cast<std::size_t>(raw.size())));}
    catch(const Error& error) {
        throw Error("INVALID_MACRO_ASSET",std::string("Macro asset envelope is unavailable: ")+error.code+": "+error.what());
    }
    if(envelope.version!=1||envelope.kind!="macro_definition")
        throw Error("UNSUPPORTED_MACRO_ASSET_VERSION","Macro asset envelope kind or version is unsupported");
    if(QString::fromStdString(envelope.asset_id)!=asset_id)
        throw Error("MACRO_ASSET_ID_MISMATCH","Macro asset envelope AssetID does not match the exact requested file identity");
    if(envelope.payload_schema!=1&&envelope.payload_schema!=2&&envelope.payload_schema!=3)
        throw Error("UNSUPPORTED_MACRO_SCHEMA","Macro payload schema is not supported by this build");
    const QByteArray payload(envelope.payload.data(),static_cast<qsizetype>(envelope.payload.size()));
    if(static_cast<std::size_t>(payload.size())>portable_macro_payload_limit)
        throw Error("MACRO_PAYLOAD_LIMIT","Portable Macro payload exceeds 256 KiB");
    const auto actual_hash=QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex();
    if(QString::fromStdString(envelope.sha256)!=QString::fromLatin1(actual_hash))
        throw Error("MACRO_ASSET_HASH_MISMATCH","Macro asset payload SHA-256 does not match its envelope");
    auto definition=read_canonical_macro_payload(std::string_view(payload.constData(),static_cast<std::size_t>(payload.size())),static_cast<unsigned>(envelope.payload_schema));
    if(QString::fromStdString(definition.label)!=QString::fromStdString(envelope.label))
        throw Error("MACRO_ASSET_ENVELOPE_MISMATCH","Macro asset label does not match its canonical payload");
    if(definition.id==asset_id.toStdString())
        throw Error("MACRO_ASSET_ID_MISMATCH","Workspace AssetID must be distinct from the source MacroDefinitionID");
    LibraryMacroAssetV1 metadata{{asset_id},QString::fromStdString(envelope.label),envelope.accepted_revision,
        QString::fromStdString(envelope.sha256),static_cast<unsigned>(envelope.payload_schema),true,{}};
    return {std::move(metadata),std::move(definition),raw,payload};
}

LibraryMacroAssetV1 metadata_for_macro(const MacroDefinition& definition,const QString& asset_id,std::uint64_t revision) {
    validate_portable_macro_definition(definition,portable_macro_payload_schema(definition));
    if(definition.id==asset_id.toStdString())
        throw Error("MACRO_ASSET_ID_MISMATCH","Workspace AssetID must be distinct from its source MacroDefinitionID");
    const auto payload=QByteArray::fromStdString(canonical_macro_payload(definition));
    const auto hash=QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex();
    return {{asset_id},QString::fromStdString(definition.label),revision,QString::fromLatin1(hash),portable_macro_payload_schema(definition),true,{}};
}

void write_preset_asset_file(const QString& root,const QString& path,const QByteArray& bytes,
    const FolderLibrary::PayloadWriteOverride& override_write,const std::optional<QByteArray>& expected_old_bytes);

void write_macro_asset_file(const QString& root,const QString& path,const QByteArray& bytes,
    const FolderLibrary::PayloadWriteOverride& override_write,const std::optional<QByteArray>& expected_old_bytes) {
    try {write_preset_asset_file(root,path,bytes,override_write,expected_old_bytes);}
    catch(const Error& error) {throw_macro_error(error);}
}

void verify_expected_preset_file(const QString& root,const QString& path,const std::optional<QByteArray>& expected_old_bytes) {
    const QFileInfo info(path);
    if(!expected_old_bytes) {
        if(info.exists())throw Error("PRESET_ASSET_REVISION_CONFLICT","A Preset asset appeared before publication commit");
        return;
    }
    if(!info.exists())throw Error("PRESET_ASSET_REVISION_CONFLICT","The selected Preset asset disappeared before update commit");
    if(checked_file_bytes(root,path)!=*expected_old_bytes)
        throw Error("PRESET_ASSET_REVISION_CONFLICT","The selected Preset asset bytes changed before update commit");
}

void write_preset_asset_file(const QString& root,const QString& path,const QByteArray& bytes,
    const FolderLibrary::PayloadWriteOverride& override_write,const std::optional<QByteArray>& expected_old_bytes) {
    const QFileInfo root_info(root);
    if(is_reparse_point(root)||!root_info.exists()||!root_info.isDir())
        throw Error("UNSAFE_PRESET_LIBRARY_ROOT","Preset Library payload root is missing or is a reparse point");
    const auto canonical_root=canonical_existing_path(root);
    if(canonical_root.isEmpty()||path_key(canonical_root,false)!=path_key(QDir::cleanPath(root),false))
        throw Error("UNSAFE_PRESET_LIBRARY_ROOT","Preset Library payload root does not resolve to its owned path");
    const QFileInfo existing(path);
    if(existing.exists()&&(is_reparse_point(path)||!existing.isFile()))
        throw Error("UNSAFE_PRESET_ASSET_PATH","Refusing to replace a reparse point or non-file Preset asset target");
    if(existing.exists()) {
        const auto canonical=canonical_existing_path(path);
        const auto parent=QFileInfo(canonical).dir().canonicalPath();
        if(canonical.isEmpty()||parent.isEmpty()||path_key(parent,false)!=path_key(canonical_root,false))
            throw Error("UNSAFE_PRESET_ASSET_PATH","Refusing to replace a Preset asset outside its owned root");
    }
    if(bytes.size()>max_preset_asset_file_bytes)throw Error("PRESET_ASSET_LIMIT","Preset asset envelope exceeds 512 KiB");
    verify_expected_preset_file(root,path,expected_old_bytes);
    if(override_write) {
        QString detail;
        if(!override_write(path,bytes,detail))
            throw Error("PRESET_LIBRARY_WRITE_FAILED",detail.isEmpty()?"Preset Library payload write was refused":detail.toStdString());
        return;
    }
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly))throw Error("PRESET_LIBRARY_WRITE_FAILED","Preset Library atomic payload file could not be opened");
    if(file.write(bytes)!=bytes.size()) {
        file.cancelWriting();throw Error("PRESET_LIBRARY_WRITE_FAILED","Preset Library atomic payload file was not written completely");
    }
    verify_expected_preset_file(root,path,expected_old_bytes);
    if(!file.commit())throw Error("PRESET_LIBRARY_WRITE_FAILED","Preset Library atomic payload replacement failed");
}

QString preset_asset_lock_path(const QString& root) {
    return QDir(root).filePath(QStringLiteral(".preset-library.lock"));
}

std::unique_ptr<QLockFile> lock_preset_asset_root(const QString& root) {
    const auto lock_path=preset_asset_lock_path(root);
    const QFileInfo lock_info(lock_path);
    if(lock_info.exists()&&(is_reparse_point(lock_path)||!lock_info.isFile()))
        throw Error("UNSAFE_PRESET_LIBRARY_LOCK","Preset Library lock path is unsafe");
    auto lock=std::make_unique<QLockFile>(lock_path);
    lock->setStaleLockTime(30000);
    if(!lock->tryLock(5000))throw Error("PRESET_LIBRARY_BUSY","Preset Library is locked by another writer");
    return lock;
}

void validate_publishable_preset(const PortablePresetClosure& closure) {
    validate_portable_preset_closure(closure);
    (void)canonical_preset_closure_payload(closure);
}

LibraryPresetAssetV1 metadata_for(const PortablePresetClosure& closure,const QString& asset_id,std::uint64_t revision) {
    validate_publishable_preset(closure);
    if(closure.definition.id==asset_id.toStdString()||closure.macro_definitions.contains(asset_id.toStdString()))
        throw Error("PRESET_ASSET_ID_MISMATCH","Workspace AssetID must be distinct from source Document definition IDs");
    const auto payload=QByteArray::fromStdString(canonical_preset_closure_payload(closure));
    const auto hash=QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex();
    return {{asset_id},QString::fromStdString(closure.definition.label),revision,QString::fromLatin1(hash),portable_preset_closure_schema(closure),true,{}};
}

}

FolderLibrary::FolderLibrary()
    : owned_settings_(std::make_unique<QSettings>(QSettings::NativeFormat, QSettings::UserScope, "Nect", "Nect")),
      settings_(owned_settings_.get()) {
    settings_->setFallbacksEnabled(false);
    settings_->setAtomicSyncRequired(true);
}

FolderLibrary::FolderLibrary(QSettings& settings, PersistOverride persist_override,
    ReadbackOverride readback_override, QString preset_payload_root, PayloadWriteOverride payload_write_override)
    : settings_(&settings),preset_payload_root_(std::move(preset_payload_root)),
      persist_override_(std::move(persist_override)),readback_override_(std::move(readback_override)),
      payload_write_override_(std::move(payload_write_override)) {
    settings_->setFallbacksEnabled(false);
    settings_->setAtomicSyncRequired(true);
}

QString FolderLibrary::resolved_preset_payload_root(bool create) const {
    QString requested=preset_payload_root_;
    if(requested.isEmpty()) {
        const auto app_data=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if(app_data.isEmpty())throw Error("PRESET_LIBRARY_ROOT_UNAVAILABLE","Application data path is unavailable");
        requested=QDir(app_data).filePath("preset-library/v1");
    }
    const auto absolute=QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(requested).absoluteFilePath()));
    QString cursor=absolute;
    while(!cursor.isEmpty()) {
        const QFileInfo info(cursor);
        if(info.exists()&&is_reparse_point(cursor))
            throw Error("UNSAFE_PRESET_LIBRARY_ROOT","Preset Library root or an ancestor is a reparse point");
        const auto parent=QDir::cleanPath(QDir::fromNativeSeparators(info.dir().absolutePath()));
        if(parent==cursor)break;
        cursor=parent;
    }
    if(create&&!QDir().mkpath(absolute))throw Error("PRESET_LIBRARY_ROOT_UNAVAILABLE","Preset Library root could not be created");
    const QFileInfo root_info(absolute);
    if(!root_info.exists())return absolute;
    if(!root_info.isDir()||is_reparse_point(absolute))
        throw Error("UNSAFE_PRESET_LIBRARY_ROOT","Preset Library root must be an owned directory without reparse points");
    const auto canonical=canonical_existing_path(absolute);
    if(canonical.isEmpty()||path_key(canonical,false)!=path_key(absolute,false))
        throw Error("UNSAFE_PRESET_LIBRARY_ROOT","Preset Library root resolves outside its configured location");
    return absolute;
}

QString FolderLibrary::resolved_macro_payload_root(bool create) const {
    try {return resolved_preset_payload_root(create);}
    catch(const Error& error) {throw_macro_error(error);}
}

const QList<LibraryRootV1>& FolderLibrary::roots() const {
    ensure_loaded();
    return roots_;
}

const QList<LibraryFavoriteV1>& FolderLibrary::favorites() const {
    ensure_loaded();
    return favorites_;
}

void FolderLibrary::ensure_loaded() const {
    if (loaded_) return;
    settings_->sync();
    if (settings_->status() != QSettings::NoError)
        throw Error("SETTINGS_READ_FAILED", "Folder Library settings could not be read");
    if (!settings_->contains(QLatin1String(settings_key))) {
        loaded_ = true;
        return;
    }
    const auto bytes = settings_->value(QLatin1String(settings_key)).toByteArray();
    QJsonParseError parse_error;
    const auto document = QJsonDocument::fromJson(bytes, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject())
        throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library settings are malformed");
    const auto state = document.object();
    if (!state.value("version").isDouble() || state.value("version").toDouble() != 1 || !state.value("roots").isArray() ||
        !state.value("favorites").isArray())
        throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library settings use an unsupported version or shape");

    QList<LibraryRootV1> roots;
    for (const auto& value : state.value("roots").toArray()) {
        if (!value.isObject()) throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library root is malformed");
        const auto object = value.toObject();
        if (!object.value("root_id").isString() || !object.value("display_name").isString() ||
            !object.value("absolute_path").isString() || !object.value("enabled").isBool() ||
            !object.value("case_sensitive").isBool())
            throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library root fields are malformed");
        LibraryRootV1 root{object.value("root_id").toString(), object.value("display_name").toString(),
            object.value("absolute_path").toString(), object.value("enabled").toBool(),
            object.value("case_sensitive").toBool()};
        if (root.root_id.isEmpty() || root.absolute_path.isEmpty() ||
            std::any_of(roots.begin(), roots.end(), [&](const auto& previous) { return previous.root_id == root.root_id; }))
            throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library has an empty or duplicate root identity");
        roots.push_back(std::move(root));
    }

    QList<LibraryFavoriteV1> favorites;
    std::set<int> used_slots;
    for (const auto& value : state.value("favorites").toArray()) {
        if (!value.isObject()) throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library favorite is malformed");
        const auto object = value.toObject();
        const bool has_legacy_ref = object.contains("ref");
        const bool has_tagged_target = object.contains("target");
        if (!object.value("favorite_id").isString() || has_legacy_ref == has_tagged_target ||
            !object.value("quick_slot").isDouble())
            throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library favorite fields are malformed");
        const double quick_slot_number = object.value("quick_slot").toDouble();
        if (!std::isfinite(quick_slot_number) || std::floor(quick_slot_number) != quick_slot_number ||
            quick_slot_number < 0 || quick_slot_number > 9)
            throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library Quick Access slot is malformed");
        if ((has_legacy_ref && !object.value("ref").isObject()) ||
            (has_tagged_target && !object.value("target").isObject()))
            throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library favorite target is malformed");
        auto target = has_tagged_target ? target_from_json(object.value("target").toObject()) :
            LibraryFavoriteTargetV1{ref_from_json(object.value("ref").toObject())};
        LibraryFavoriteV1 favorite{object.value("favorite_id").toString(), std::move(target),
            static_cast<int>(quick_slot_number)};
        if (favorite.favorite_id.isEmpty() || favorite.quick_slot < 0 || favorite.quick_slot > 9 ||
            (favorite.quick_slot != 0 && !used_slots.insert(favorite.quick_slot).second) ||
            std::any_of(favorites.begin(), favorites.end(), [&](const auto& previous) { return previous.favorite_id == favorite.favorite_id; }))
            throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library has a duplicate favorite or Quick Access slot");
        favorites.push_back(std::move(favorite));
    }
    roots_ = std::move(roots);
    favorites_ = std::move(favorites);
    loaded_ = true;
}

QByteArray FolderLibrary::serialize_state(const QList<LibraryRootV1>& roots,
    const QList<LibraryFavoriteV1>& favorites) const {
    QJsonArray root_values;
    for (const auto& root : roots) root_values.append(QJsonObject{
        {"root_id", root.root_id}, {"display_name", root.display_name},
        {"absolute_path", root.absolute_path}, {"enabled", root.enabled},
        {"case_sensitive", root.case_sensitive}});
    QJsonArray favorite_values;
    for (const auto& favorite : favorites) favorite_values.append(QJsonObject{
        {"favorite_id", favorite.favorite_id}, {"target", target_to_json(favorite.target)},
        {"quick_slot", favorite.quick_slot}});
    return QJsonDocument(QJsonObject{{"version", 1}, {"roots", root_values}, {"favorites", favorite_values}})
        .toJson(QJsonDocument::Compact);
}

void FolderLibrary::persist_state(const QList<LibraryRootV1>& roots,
    const QList<LibraryFavoriteV1>& favorites) {
    const auto bytes = serialize_state(roots, favorites);
    QString detail;
    if (persist_override_) {
        if (!persist_override_(bytes, detail))
            throw Error("SETTINGS_WRITE_FAILED", (detail.isEmpty() ? QStringLiteral("Folder Library settings were not saved") : detail).toStdString());
        return;
    }

    settings_->sync();
    if (settings_->status() != QSettings::NoError)
        throw Error("SETTINGS_WRITE_FAILED", "Folder Library settings could not be synchronized");
    const auto key = QLatin1String(settings_key);
    const auto previous = read_state_from_fresh_settings(false);
    settings_->setValue(key, bytes);
    settings_->sync();
    std::optional<QByteArray> readback;
    if (settings_->status() == QSettings::NoError) {
        try {
            readback = read_state_from_fresh_settings(true);
        } catch (const Error&) {
            // Treat inability to confirm the write like a mismatch so the prior
            // setting is restored before reporting failure.
        }
    }
    if (readback && *readback == bytes) return;

    if (previous) settings_->setValue(key, *previous);
    else settings_->remove(key);
    settings_->sync();
    const auto restored = settings_->status() == QSettings::NoError ? read_state_from_fresh_settings(false) : std::nullopt;
    if (restored != previous)
        throw Error("SETTINGS_WRITE_FAILED", "Folder Library settings failed readback and the prior value could not be confirmed after restoration");
    throw Error("SETTINGS_WRITE_FAILED", "Folder Library settings failed fresh readback and the prior value was restored");
}

std::optional<QByteArray> FolderLibrary::read_state_from_fresh_settings(bool use_override) const {
    if (use_override && readback_override_) return readback_override_();

    std::unique_ptr<QSettings> reader;
    // Preserve the exact backing store for both INI files and explicit native
    // registry paths. Reconstructing NativeFormat from organization/app names
    // can silently redirect an injected fileName-backed QSettings elsewhere.
    reader = std::make_unique<QSettings>(settings_->fileName(), settings_->format());
    reader->beginGroup(settings_->group());
    reader->setFallbacksEnabled(false);
    reader->sync();
    if (reader->status() != QSettings::NoError)
        throw Error("SETTINGS_READ_FAILED", "Folder Library settings could not be read back from a fresh QSettings instance");
    const auto key = QLatin1String(settings_key);
    if (!reader->contains(key)) return {};
    return reader->value(key).toByteArray();
}

const LibraryRootV1& FolderLibrary::root(const QString& root_id) const {
    ensure_loaded();
    const auto found = std::find_if(roots_.begin(), roots_.end(), [&](const auto& value) { return value.root_id == root_id; });
    if (found == roots_.end()) throw Error("MISSING_LIBRARY_ROOT", "The registered Folder Library root no longer exists");
    if (!found->enabled) throw Error("LIBRARY_ROOT_DISABLED", "The Folder Library root is disabled");
    return *found;
}

QString FolderLibrary::normalize_relative_path(const QString& path) const {
    if (path.contains(QChar(0))) throw Error("INVALID_LIBRARY_PATH", "Folder Library paths cannot contain a null character");
    auto normalized = QDir::fromNativeSeparators(path);
    normalized.replace('\\', '/');
    if (QDir::isAbsolutePath(normalized) || QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(normalized).hasMatch())
        throw Error("INVALID_LIBRARY_PATH", "A Folder Library item path must be relative to its registered root");
    normalized = QDir::cleanPath(normalized);
    if (normalized == ".") return {};
    if (normalized == ".." || normalized.startsWith("../"))
        throw Error("LIBRARY_PATH_ESCAPE", "A Folder Library item path cannot leave its registered root");
    return normalized;
}

QString FolderLibrary::normalized_absolute_path(const QString& path) const {
    const QFileInfo info(path);
    if (!info.exists()) throw Error("LIBRARY_ITEM_MISSING", "The Folder Library path is missing");
    const auto canonical = canonical_existing_path(path);
    if (canonical.isEmpty()) throw Error("LIBRARY_ITEM_UNRESOLVED", "The Folder Library path could not be resolved");
    return QDir::cleanPath(QDir::fromNativeSeparators(canonical));
}

bool FolderLibrary::contained_by(const QString& canonical_root, const QString& canonical_candidate, bool case_sensitive) const {
    const auto root_path = path_key(canonical_root, case_sensitive);
    const auto candidate_path = path_key(canonical_candidate, case_sensitive);
    if (candidate_path == root_path) return true;
    return candidate_path.startsWith(root_path.endsWith('/') ? root_path : root_path + '/', Qt::CaseSensitive);
}

bool FolderLibrary::supported_raster(const QString& path) const {
    const auto suffix = QFileInfo(path).suffix().toCaseFolded();
    return suffix == "png" || suffix == "jpg" || suffix == "jpeg";
}

LibraryRootV1 FolderLibrary::register_root(const QString& path, const QString& display_name) {
    ensure_loaded();
    const QFileInfo supplied(path);
    if (!supplied.exists() || !supplied.isDir()) throw Error("INVALID_LIBRARY_ROOT", "Choose an existing folder as the library root");
    const auto canonical = normalized_absolute_path(path);
    const bool case_sensitive = root_case_sensitive(canonical);
    for (const auto& existing : roots_) {
        const bool compare_case = existing.case_sensitive && case_sensitive;
        if (path_key(existing.absolute_path, compare_case) == path_key(canonical, compare_case))
            throw Error("DUPLICATE_LIBRARY_ROOT", "This folder is already registered");
    }
    const auto display = display_name.trimmed().isEmpty() ? QFileInfo(canonical).fileName() : display_name.trimmed();
    LibraryRootV1 created{QUuid::createUuid().toString(QUuid::WithoutBraces), display, canonical, true, case_sensitive};
    auto candidate = roots_;
    candidate.push_back(created);
    persist_state(candidate, favorites_);
    roots_ = std::move(candidate);
    index_.clear();
    return created;
}

void FolderLibrary::unregister_root(const QString& root_id) {
    ensure_loaded();
    const auto found = std::find_if(roots_.begin(), roots_.end(), [&](const auto& value) { return value.root_id == root_id; });
    if (found == roots_.end()) throw Error("MISSING_LIBRARY_ROOT", "The registered Folder Library root no longer exists");
    auto candidate = roots_;
    candidate.erase(std::remove_if(candidate.begin(), candidate.end(), [&](const auto& value) { return value.root_id == root_id; }), candidate.end());
    persist_state(candidate, favorites_);
    roots_ = std::move(candidate);
    index_.clear();
}

QString FolderLibrary::comparison_key(const LibraryItemRefV1& ref, const LibraryRootV1& root_value) const {
    const auto relative = normalize_relative_path(ref.normalized_relative_path);
    const auto path = root_value.case_sensitive ? relative : normalize_unicode(relative.toCaseFolded());
    return root_value.root_id + QChar(0x1f) + ref.kind + QChar(0x1f) + path;
}

QString FolderLibrary::comparison_key(const LibraryItemRefV1& ref) const {
    return comparison_key(ref, root(ref.root_id));
}

bool FolderLibrary::same_identity(const LibraryItemRefV1& left, const LibraryItemRefV1& right) const {
    return comparison_key(left) == comparison_key(right);
}

bool FolderLibrary::same_identity(const LibraryFavoriteTargetV1& left,
    const LibraryFavoriteTargetV1& right) const {
    if (left.index() != right.index()) return false;
    if (const auto* left_item = std::get_if<LibraryItemRefV1>(&left))
        return same_identity(*left_item, std::get<LibraryItemRefV1>(right));
    if (const auto* left_effect = std::get_if<BuiltinEffectTypeRefV1>(&left))
        return *left_effect == std::get<BuiltinEffectTypeRefV1>(right);
    if(const auto* left_preset=std::get_if<PresetAssetRefV1>(&left))
        return *left_preset==std::get<PresetAssetRefV1>(right);
    return std::get<MacroAssetRefV1>(left)==std::get<MacroAssetRefV1>(right);
}

LibraryItemV1 FolderLibrary::resolve(const LibraryItemRefV1& supplied_ref) const {
    const auto& root_value = root(supplied_ref.root_id);
    if (supplied_ref.kind != "folder" && supplied_ref.kind != "raster")
        throw Error("UNSUPPORTED_LIBRARY_ITEM", "This Folder Library item type cannot be placed");
    const auto relative = normalize_relative_path(supplied_ref.normalized_relative_path);
    const auto lexical_path = relative.isEmpty() ? root_value.absolute_path : QDir(root_value.absolute_path).absoluteFilePath(relative);
    const QFileInfo candidate_info(lexical_path);
    if (!candidate_info.exists()) throw Error("LIBRARY_ITEM_MISSING", "The selected Folder Library item is missing");
    const auto canonical_root = normalized_absolute_path(root_value.absolute_path);
    if (path_key(canonical_root, root_value.case_sensitive) != path_key(root_value.absolute_path, root_value.case_sensitive))
        throw Error("LIBRARY_ROOT_CHANGED", "The registered folder now resolves to a different location");
    const auto canonical_candidate = normalized_absolute_path(lexical_path);
    if (!contained_by(canonical_root, canonical_candidate, root_value.case_sensitive))
        throw Error("LIBRARY_PATH_ESCAPE", "The selected Folder Library item resolves outside its registered root");
    if (supplied_ref.kind == "folder" && !candidate_info.isDir())
        throw Error("LIBRARY_ITEM_KIND_CHANGED", "The selected Folder Library folder is no longer a folder");
    if (supplied_ref.kind == "raster" && (!candidate_info.isFile() || !supported_raster(lexical_path)))
        throw Error("UNSUPPORTED_LIBRARY_ITEM", "Only existing PNG and JPEG files can be placed from the Folder Library");
    const LibraryItemRefV1 normalized_ref{root_value.root_id, relative, supplied_ref.kind};
    const auto name = relative.isEmpty() ? root_value.display_name : leaf_name(relative);
    return {normalized_ref, name, QDir::cleanPath(lexical_path), true, {}};
}

void FolderLibrary::scan_root(const LibraryRootV1& root_value, QList<LibraryItemV1>& result) const {
    const LibraryItemRefV1 root_ref{root_value.root_id, {}, "folder"};
    const QFileInfo root_info(root_value.absolute_path);
    if (!root_info.exists() || !root_info.isDir()) {
        result.push_back({root_ref, root_value.display_name, root_value.absolute_path, false, "Registered folder is missing"});
        return;
    }
    QString canonical_root;
    try { canonical_root = normalized_absolute_path(root_value.absolute_path); }
    catch (const Error& error) {
        result.push_back({root_ref, root_value.display_name, root_value.absolute_path, false, error_message(error)});
        return;
    }
    if (path_key(canonical_root, root_value.case_sensitive) != path_key(root_value.absolute_path, root_value.case_sensitive)) {
        result.push_back({root_ref, root_value.display_name, root_value.absolute_path, false, "Registered folder now resolves to a different location"});
        return;
    }
    result.push_back({root_ref, root_value.display_name, root_value.absolute_path, true, {}});
    std::set<QString> visited;
    visited.insert(path_key(canonical_root, root_value.case_sensitive));
    std::function<void(const QString&, const QString&)> walk = [&](const QString& absolute_dir, const QString& relative_dir) {
        const QDir directory(absolute_dir);
        const auto entries = directory.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const auto& info : entries) {
            const auto name = info.fileName();
            const auto relative = normalize_relative_path(relative_dir.isEmpty() ? name : relative_dir + "/" + name);
            const bool is_directory = info.isDir();
            if (!is_directory && !supported_raster(info.fileName())) continue;
            const auto kind = is_directory ? QStringLiteral("folder") : QStringLiteral("raster");
            const LibraryItemRefV1 ref{root_value.root_id, relative, kind};
            QString canonical;
            try { canonical = normalized_absolute_path(info.absoluteFilePath()); }
            catch (const Error&) {
                result.push_back({ref, name, info.absoluteFilePath(), false, "Library path cannot be resolved"});
                continue;
            }
            if (canonical.isEmpty()) {
                result.push_back({ref, name, info.absoluteFilePath(), false, "Library path cannot be resolved"});
                continue;
            }
            const auto normalized_canonical = QDir::cleanPath(QDir::fromNativeSeparators(canonical));
            if (!contained_by(canonical_root, normalized_canonical, root_value.case_sensitive)) {
                result.push_back({ref, name, info.absoluteFilePath(), false, "Path resolves outside the registered folder"});
                continue;
            }
            result.push_back({ref, name, info.absoluteFilePath(), true, {}});
            if (!is_directory || is_reparse_point(info.absoluteFilePath())) continue;
            const auto visit_key = path_key(normalized_canonical, root_value.case_sensitive);
            if (!visited.insert(visit_key).second) continue;
            walk(info.absoluteFilePath(), relative);
        }
    };
    walk(root_value.absolute_path, {});
}

void FolderLibrary::refresh() {
    ensure_loaded();
    QList<LibraryItemV1> refreshed;
    for (const auto& root_value : roots_) if (root_value.enabled) scan_root(root_value, refreshed);
    std::stable_sort(refreshed.begin(), refreshed.end(), [&](const auto& left, const auto& right) {
        const auto left_root = std::find_if(roots_.begin(), roots_.end(), [&](const auto& value) { return value.root_id == left.ref.root_id; });
        const auto right_root = std::find_if(roots_.begin(), roots_.end(), [&](const auto& value) { return value.root_id == right.ref.root_id; });
        const auto left_index = std::distance(roots_.begin(), left_root);
        const auto right_index = std::distance(roots_.begin(), right_root);
        if (left_index != right_index) return left_index < right_index;
        const auto left_depth = left.ref.normalized_relative_path.count('/');
        const auto right_depth = right.ref.normalized_relative_path.count('/');
        if (left_depth != right_depth) return left_depth < right_depth;
        return QString::compare(left.ref.normalized_relative_path, right.ref.normalized_relative_path, Qt::CaseInsensitive) < 0;
    });
    index_ = std::move(refreshed);
}

QList<LibraryItemV1> FolderLibrary::items() const {
    ensure_loaded();
    return index_;
}

QList<LibraryItemV1> FolderLibrary::browse(const QString& root_id, const QString& parent_relative_path) const {
    ensure_loaded();
    const auto parent = normalize_relative_path(parent_relative_path);
    QList<LibraryItemV1> children;
    for (const auto& item : index_) {
        if (item.ref.root_id != root_id || item.ref.normalized_relative_path.isEmpty()) continue;
        if (parent_path(item.ref.normalized_relative_path) == parent) children.push_back(item);
    }
    return children;
}

QList<LibraryItemV1> FolderLibrary::search(const QString& query) const {
    ensure_loaded();
    const auto terms = query.split(' ', Qt::SkipEmptyParts);
    QList<LibraryItemV1> matches;
    for (const auto& item : index_) {
        const auto root_found = std::find_if(roots_.begin(), roots_.end(), [&](const auto& value) { return value.root_id == item.ref.root_id; });
        const auto searchable = item.display_name + " " + item.ref.normalized_relative_path +
            (root_found == roots_.end() ? QString{} : " " + root_found->display_name);
        if (std::all_of(terms.begin(), terms.end(), [&](const auto& term) { return searchable.contains(term, Qt::CaseInsensitive); }))
            matches.push_back(item);
    }
    return matches;
}

LibraryFavoriteV1 FolderLibrary::add_favorite(const LibraryItemRefV1& supplied_ref, int quick_slot) {
    ensure_loaded();
    if (quick_slot < 0 || quick_slot > 9) throw Error("INVALID_QUICK_SLOT", "Quick Access slot must be between 1 and 9, or 0 for none");
    const auto resolved = resolve(supplied_ref);
    return add_favorite_target(LibraryFavoriteTargetV1{resolved.ref}, quick_slot);
}

LibraryFavoriteV1 FolderLibrary::add_favorite(const BuiltinEffectTypeRefV1& effect, int quick_slot) {
    ensure_loaded();
    if (quick_slot < 0 || quick_slot > 9)
        throw Error("INVALID_QUICK_SLOT", "Quick Access slot must be between 1 and 9, or 0 for none");
    if (effect.type_id.trimmed().isEmpty() || effect.behavior_version == 0)
        throw Error("INVALID_EFFECT_TYPE_REF", "A built-in Effect Favorite needs a TypeID and positive BehaviorVersion");
    return add_favorite_target(LibraryFavoriteTargetV1{effect}, quick_slot);
}

LibraryFavoriteV1 FolderLibrary::add_favorite_target(const LibraryFavoriteTargetV1& target, int quick_slot) {
    auto candidate = favorites_;
    auto found = std::find_if(candidate.begin(), candidate.end(), [&](const auto& favorite) {
        return same_identity(favorite.target, target);
    });
    if (quick_slot != 0) {
        const auto occupied = std::find_if(candidate.begin(), candidate.end(), [&](const auto& favorite) {
            return favorite.quick_slot == quick_slot && (found == candidate.end() || favorite.favorite_id != found->favorite_id);
        });
        if (occupied != candidate.end())
            throw Error("QUICK_SLOT_OCCUPIED", "That Quick Access slot already belongs to another Favorite");
    }

    LibraryFavoriteV1 result;
    if (found != candidate.end()) {
        result = *found;
        if (quick_slot != 0) result.quick_slot = quick_slot;
        *found = result;
    } else {
        result = {QUuid::createUuid().toString(QUuid::WithoutBraces), target, quick_slot};
        candidate.push_back(result);
    }
    persist_state(roots_, candidate);
    favorites_ = std::move(candidate);
    return result;
}

void FolderLibrary::remove_favorite(const QString& favorite_id) {
    ensure_loaded();
    auto candidate = favorites_;
    const auto old_size = candidate.size();
    candidate.erase(std::remove_if(candidate.begin(), candidate.end(), [&](const auto& favorite) { return favorite.favorite_id == favorite_id; }), candidate.end());
    if (candidate.size() == old_size) throw Error("MISSING_FAVORITE", "The Folder Library favorite no longer exists");
    persist_state(roots_, candidate);
    favorites_ = std::move(candidate);
}

void FolderLibrary::assign_quick_slot(const QString& favorite_id, int quick_slot) {
    ensure_loaded();
    if (quick_slot < 0 || quick_slot > 9) throw Error("INVALID_QUICK_SLOT", "Quick Access slot must be between 1 and 9, or 0 for none");
    auto candidate = favorites_;
    const auto selected = std::find_if(candidate.begin(), candidate.end(), [&](const auto& favorite) { return favorite.favorite_id == favorite_id; });
    if (selected == candidate.end()) throw Error("MISSING_FAVORITE", "The Folder Library favorite no longer exists");
    if (quick_slot != 0 && std::any_of(candidate.begin(), candidate.end(), [&](const auto& favorite) {
        return favorite.favorite_id != favorite_id && favorite.quick_slot == quick_slot;
    }))
        throw Error("QUICK_SLOT_OCCUPIED", "That Quick Access slot already belongs to another Favorite");
    selected->quick_slot = quick_slot;
    persist_state(roots_, candidate);
    favorites_ = std::move(candidate);
}

std::optional<LibraryFavoriteV1> FolderLibrary::favorite_for_slot(int quick_slot) const {
    ensure_loaded();
    if (quick_slot < 1 || quick_slot > 9) throw Error("INVALID_QUICK_SLOT", "Quick Access slot must be between 1 and 9");
    const auto found = std::find_if(favorites_.begin(), favorites_.end(), [&](const auto& favorite) { return favorite.quick_slot == quick_slot; });
    if (found == favorites_.end()) return {};
    return *found;
}

QString FolderLibrary::favorite_status(const LibraryFavoriteV1& favorite) const {
    if (const auto* effect = std::get_if<BuiltinEffectTypeRefV1>(&favorite.target)) {
        const auto* descriptor = builtin_operation_type(effect->type_id.toStdString());
        if (!descriptor || !descriptor->effects_catalog)
            return "UNAVAILABLE_EFFECT_TYPE: " + effect->type_id + " behavior v" + QString::number(effect->behavior_version) + " is not registered in the built-in Effects catalog";
        if (descriptor->version != effect->behavior_version)
            return "EFFECT_BEHAVIOR_VERSION_MISMATCH: Favorite requires " + effect->type_id + " behavior v" +
                QString::number(effect->behavior_version) + "; this build provides v" + QString::number(descriptor->version);
        return "Available";
    }
    if(const auto* preset=std::get_if<PresetAssetRefV1>(&favorite.target)) {
        try {(void)read_preset_closure_asset(*preset);return "Available";}
        catch(const Error& error) {return error_message(error);}
    }
    if(const auto* macro=std::get_if<MacroAssetRefV1>(&favorite.target)) {
        try {(void)read_macro_asset(*macro);return "Available";}
        catch(const Error& error) {return error_message(error);}
    }
    try {
        const auto resolved = resolve(std::get<LibraryItemRefV1>(favorite.target));
        return resolved.available ? QStringLiteral("Available") : resolved.problem;
    } catch (const Error& error) {
        return error_message(error);
    }
}

QList<LibraryPresetAssetV1> FolderLibrary::preset_assets() const {
    const auto root_path=resolved_preset_payload_root(false);
    if(!QFileInfo::exists(root_path))return {};
    if(workspace_asset_file_count(root_path)>max_workspace_assets)
        throw Error("PRESET_LIBRARY_LIMIT","Workspace asset store exceeds 256 immediate Preset and Macro assets");
    QDir directory(root_path);
    const auto files=directory.entryInfoList({"*.preset.json"},
        QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDir::Name);
    if(files.size()>max_preset_assets)throw Error("PRESET_LIBRARY_LIMIT","Workspace Preset Library is limited to 256 immediate assets");
    QList<LibraryPresetAssetV1> assets;
    const auto suffix=QStringLiteral(".preset.json");
    for(const auto& file:files) {
        const auto name=file.fileName();
        if(!name.endsWith(suffix,Qt::CaseSensitive))continue;
        const auto asset_id=name.left(name.size()-suffix.size());
        if(!canonical_asset_id(asset_id))continue;
        try { assets.push_back(read_stored_preset_asset(root_path,asset_id).metadata); }
        catch(const Error& error) {
            assets.push_back({{asset_id},QStringLiteral("Unavailable Preset"),0,{},0,false,error_message(error)});
        }
    }
    std::stable_sort(assets.begin(),assets.end(),[](const auto& left,const auto& right) {
        const auto by_label=QString::compare(left.label,right.label,Qt::CaseInsensitive);
        return by_label==0?left.ref.asset_id<right.ref.asset_id:by_label<0;
    });
    return assets;
}

PresetDefinition FolderLibrary::read_preset_asset(const PresetAssetRefV1& ref,LibraryPresetAssetV1* metadata) const {
    LibraryPresetAssetV1 read_metadata;
    auto closure=read_preset_closure_asset(ref,&read_metadata);
    if(!closure.macro_definitions.empty())
        throw Error("PRESET_DEPENDENCY_CLOSURE_REQUIRED","Preset asset includes Macro dependencies; read its complete portable closure");
    if(metadata)*metadata=std::move(read_metadata);
    return std::move(closure.definition);
}

PortablePresetClosure FolderLibrary::read_preset_closure_asset(const PresetAssetRefV1& ref,LibraryPresetAssetV1* metadata) const {
    const auto root_path=resolved_preset_payload_root(false);
    auto stored=read_stored_preset_asset(root_path,ref.asset_id);
    if(metadata)*metadata=stored.metadata;
    return std::move(stored.closure);
}

LibraryPresetAssetV1 FolderLibrary::publish_preset(const PresetDefinition& definition) {
    validate_portable_literal_preset(definition);
    return publish_preset(PortablePresetClosure{definition,{}});
}

LibraryPresetAssetV1 FolderLibrary::publish_preset(const PortablePresetClosure& closure) {
    validate_publishable_preset(closure);
    const auto root_path=resolved_preset_payload_root(true);
    auto lock=lock_preset_asset_root(root_path);
    (void)lock;
    const auto files=QDir(root_path).entryInfoList({"*.preset.json"},
        QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDir::Name);
    if(workspace_asset_file_count(root_path)>=max_workspace_assets||files.size()>=max_preset_assets)
        throw Error("PRESET_LIBRARY_LIMIT","Workspace asset store is limited to 256 immediate Preset and Macro assets");
    QString asset_id;
    QString path;
    do {
        asset_id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        path=QDir(root_path).filePath(asset_id+QStringLiteral(".preset.json"));
    } while(QFileInfo::exists(path));
    auto metadata=metadata_for(closure,asset_id,1);
    const auto payload=QByteArray::fromStdString(canonical_preset_closure_payload(closure));
    const auto envelope=preset_asset_envelope(metadata,payload);
    write_preset_asset_file(root_path,path,envelope,payload_write_override_,std::nullopt);
    bool removed_candidate=false;
    try {
        const auto readback=read_stored_preset_asset(root_path,asset_id);
        if(readback.raw_envelope!=envelope)throw Error("PRESET_LIBRARY_READBACK_FAILED","Published Preset asset failed exact fresh readback");
    } catch(const std::exception& error) {
        try {
            if(checked_file_bytes(root_path,path)==envelope)removed_candidate=QFile::remove(path)&&!QFileInfo::exists(path);
        } catch(const std::exception&) {}
        throw Error("PRESET_LIBRARY_WRITE_FAILED",std::string("Published Preset asset could not be confirmed; exact candidate cleanup ")+
            (removed_candidate?"succeeded: ":"was skipped because observed bytes differed or could not be read: ")+error.what());
    }
    return metadata;
}

LibraryPresetAssetV1 FolderLibrary::update_preset_asset(const PresetAssetRefV1& ref,
    const PresetDefinition& definition,std::uint64_t expected_revision,const QString& expected_sha256) {
    validate_portable_literal_preset(definition);
    return update_preset_asset(ref,PortablePresetClosure{definition,{}},expected_revision,expected_sha256);
}

LibraryPresetAssetV1 FolderLibrary::update_preset_asset(const PresetAssetRefV1& ref,
    const PortablePresetClosure& closure,std::uint64_t expected_revision,const QString& expected_sha256) {
    validate_publishable_preset(closure);
    const auto root_path=resolved_preset_payload_root(false);
    if(!QFileInfo::exists(root_path))throw Error("MISSING_PRESET_ASSET","The exact Preset Library asset file is missing");
    auto lock=lock_preset_asset_root(root_path);
    (void)lock;
    auto current=read_stored_preset_asset(root_path,ref.asset_id);
    if(current.metadata.accepted_revision!=expected_revision||current.metadata.sha256!=expected_sha256)
        throw Error("PRESET_ASSET_REVISION_CONFLICT","Preset asset changed after selection; refresh before updating");
    if(expected_revision>=9007199254740991ULL)
        throw Error("PRESET_ASSET_REVISION_LIMIT","Preset asset revision reached the supported integer limit");
    auto next=metadata_for(closure,ref.asset_id,expected_revision+1);
    const auto payload=QByteArray::fromStdString(canonical_preset_closure_payload(closure));
    const auto envelope=preset_asset_envelope(next,payload);
    const auto path=QDir(root_path).filePath(ref.asset_id+QStringLiteral(".preset.json"));
    write_preset_asset_file(root_path,path,envelope,payload_write_override_,current.raw_envelope);
    try {
        const auto readback=read_stored_preset_asset(root_path,ref.asset_id);
        if(readback.raw_envelope!=envelope)throw Error("PRESET_LIBRARY_READBACK_FAILED","Updated Preset asset failed exact fresh readback");
    } catch(const std::exception& error) {
        try {
            if(checked_file_bytes(root_path,path)!=envelope)
                throw Error("PRESET_LIBRARY_READBACK_FAILED","Observed Preset bytes no longer match this update candidate; rollback was skipped");
            write_preset_asset_file(root_path,path,current.raw_envelope,{},envelope);
            const auto restored=checked_file_bytes(root_path,path);
            if(restored!=current.raw_envelope)throw Error("PRESET_LIBRARY_ROLLBACK_FAILED","Prior Preset bytes did not verify after rollback");
        } catch(const std::exception& restore_error) {
            throw Error("PRESET_LIBRARY_ROLLBACK_FAILED",std::string("Updated Preset readback failed and prior bytes could not be confirmed: ")+restore_error.what());
        }
        throw Error("PRESET_LIBRARY_WRITE_FAILED",std::string("Updated Preset asset failed readback; prior bytes were restored: ")+error.what());
    }
    return next;
}

void FolderLibrary::delete_preset_asset(const PresetAssetRefV1& ref,
    std::uint64_t expected_revision,const QString& expected_sha256) {
    const auto root_path=resolved_preset_payload_root(false);
    if(!QFileInfo::exists(root_path))throw Error("MISSING_PRESET_ASSET","The exact Preset Library asset file is missing");
    auto lock=lock_preset_asset_root(root_path);
    (void)lock;
    const auto current=read_stored_preset_asset(root_path,ref.asset_id);
    if(current.metadata.accepted_revision!=expected_revision||current.metadata.sha256!=expected_sha256)
        throw Error("PRESET_ASSET_REVISION_CONFLICT","Preset asset changed after selection; refresh before deleting");
    const auto path=QDir(root_path).filePath(ref.asset_id+QStringLiteral(".preset.json"));
    verify_expected_preset_file(root_path,path,current.raw_envelope);
    const auto tombstone=QDir(root_path).filePath(ref.asset_id+QStringLiteral(".delete-")+QUuid::createUuid().toString(QUuid::WithoutBraces));
    if(!QFile::rename(path,tombstone))throw Error("PRESET_LIBRARY_DELETE_FAILED","Preset asset could not be moved atomically for deletion");
    if(QFile::remove(tombstone)&&!QFileInfo::exists(path))return;
    if(!QFileInfo::exists(path))QFile::rename(tombstone,path);
    throw Error("PRESET_LIBRARY_DELETE_FAILED","Preset asset deletion failed; the original asset was restored when possible");
}

LibraryFavoriteV1 FolderLibrary::add_favorite(const PresetAssetRefV1& preset,int quick_slot) {
    if(quick_slot<0||quick_slot>9)throw Error("INVALID_QUICK_SLOT","Quick Access slot must be between 1 and 9, or 0 for none");
    (void)read_preset_closure_asset(preset);
    return add_favorite_target(LibraryFavoriteTargetV1{preset},quick_slot);
}

QList<LibraryMacroAssetV1> FolderLibrary::macro_assets() const {
    const auto root_path=resolved_macro_payload_root(false);
    if(!QFileInfo::exists(root_path))return {};
    if(workspace_asset_file_count(root_path)>max_workspace_assets)
        throw Error("MACRO_LIBRARY_LIMIT","Workspace asset store exceeds 256 immediate Preset and Macro assets");
    QDir directory(root_path);
    const auto files=directory.entryInfoList({"*.macro.json"},
        QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDir::Name);
    if(files.size()>max_workspace_assets)
        throw Error("MACRO_LIBRARY_LIMIT","Workspace asset store is limited to 256 immediate Preset and Macro assets");
    QList<LibraryMacroAssetV1> assets;
    const auto suffix=QStringLiteral(".macro.json");
    for(const auto& file:files) {
        const auto name=file.fileName();
        if(!name.endsWith(suffix,Qt::CaseSensitive))continue;
        const auto asset_id=name.left(name.size()-suffix.size());
        if(!canonical_asset_id(asset_id))continue;
        try {assets.push_back(read_stored_macro_asset(root_path,asset_id).metadata);}
        catch(const Error& error) {
            assets.push_back({{asset_id},QStringLiteral("Unavailable Macro"),0,{},0,false,error_message(error)});
        }
    }
    std::stable_sort(assets.begin(),assets.end(),[](const auto& left,const auto& right) {
        const auto by_label=QString::compare(left.label,right.label,Qt::CaseInsensitive);
        return by_label==0?left.ref.asset_id<right.ref.asset_id:by_label<0;
    });
    return assets;
}

MacroDefinition FolderLibrary::read_macro_asset(const MacroAssetRefV1& ref,LibraryMacroAssetV1* metadata) const {
    auto stored=read_stored_macro_asset(resolved_macro_payload_root(false),ref.asset_id);
    if(metadata)*metadata=stored.metadata;
    return std::move(stored.definition);
}

LibraryMacroAssetV1 FolderLibrary::publish_macro_asset(const MacroDefinition& definition) {
    validate_portable_macro_definition(definition,portable_macro_payload_schema(definition));
    const auto root_path=resolved_macro_payload_root(true);
    auto lock=lock_macro_asset_root(root_path);(void)lock;
    if(workspace_asset_file_count(root_path)>=max_workspace_assets)
        throw Error("MACRO_LIBRARY_LIMIT","Workspace asset store is limited to 256 immediate Preset and Macro assets");
    QString asset_id,path;
    do {
        asset_id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        path=QDir(root_path).filePath(asset_id+QStringLiteral(".macro.json"));
    } while(QFileInfo::exists(path));
    auto metadata=metadata_for_macro(definition,asset_id,1);
    const auto payload=QByteArray::fromStdString(canonical_macro_payload(definition));
    const auto envelope=macro_asset_envelope(metadata,payload);
    write_macro_asset_file(root_path,path,envelope,payload_write_override_,std::nullopt);
    try {
        const auto readback=read_stored_macro_asset(root_path,asset_id);
        if(readback.raw_envelope!=envelope)
            throw Error("MACRO_LIBRARY_READBACK_FAILED","Published Macro asset failed exact fresh readback");
    } catch(const std::exception& error) {
        bool removed=false;
        try {if(checked_macro_file_bytes(root_path,path)==envelope)removed=QFile::remove(path)&&!QFileInfo::exists(path);}catch(const std::exception&){}
        throw Error("MACRO_LIBRARY_WRITE_FAILED",std::string("Published Macro asset could not be confirmed; exact candidate cleanup ")+
            (removed?"succeeded: ":"was skipped because observed bytes differed or could not be read: ")+error.what());
    }
    return metadata;
}

LibraryMacroAssetV1 FolderLibrary::update_macro_asset(const MacroAssetRefV1& ref,const MacroDefinition& definition,
    std::uint64_t expected_revision,const QString& expected_sha256) {
    validate_portable_macro_definition(definition,portable_macro_payload_schema(definition));
    const auto root_path=resolved_macro_payload_root(false);
    if(!QFileInfo::exists(root_path))throw Error("MISSING_MACRO_ASSET","The exact Macro Library asset file is missing");
    auto lock=lock_macro_asset_root(root_path);(void)lock;
    auto current=read_stored_macro_asset(root_path,ref.asset_id);
    if(current.metadata.accepted_revision!=expected_revision||current.metadata.sha256!=expected_sha256)
        throw Error("MACRO_ASSET_REVISION_CONFLICT","Macro asset changed after selection; refresh before updating");
    if(expected_revision>=9007199254740991ULL)
        throw Error("MACRO_ASSET_REVISION_LIMIT","Macro asset revision reached the supported integer limit");
    auto next=metadata_for_macro(definition,ref.asset_id,expected_revision+1);
    const auto payload=QByteArray::fromStdString(canonical_macro_payload(definition));
    const auto envelope=macro_asset_envelope(next,payload);
    const auto path=QDir(root_path).filePath(ref.asset_id+QStringLiteral(".macro.json"));
    write_macro_asset_file(root_path,path,envelope,payload_write_override_,current.raw_envelope);
    try {
        const auto readback=read_stored_macro_asset(root_path,ref.asset_id);
        if(readback.raw_envelope!=envelope)
            throw Error("MACRO_LIBRARY_READBACK_FAILED","Updated Macro asset failed exact fresh readback");
    } catch(const std::exception& error) {
        try {
            if(checked_macro_file_bytes(root_path,path)!=envelope)
                throw Error("MACRO_LIBRARY_READBACK_FAILED","Observed Macro bytes no longer match this update candidate; rollback was skipped");
            write_macro_asset_file(root_path,path,current.raw_envelope,{},envelope);
            if(checked_macro_file_bytes(root_path,path)!=current.raw_envelope)
                throw Error("MACRO_LIBRARY_ROLLBACK_FAILED","Prior Macro bytes did not verify after rollback");
        } catch(const std::exception& restore_error) {
            throw Error("MACRO_LIBRARY_ROLLBACK_FAILED",std::string("Updated Macro readback failed and prior bytes could not be confirmed: ")+restore_error.what());
        }
        throw Error("MACRO_LIBRARY_WRITE_FAILED",std::string("Updated Macro asset failed readback; prior bytes were restored: ")+error.what());
    }
    return next;
}

void FolderLibrary::delete_macro_asset(const MacroAssetRefV1& ref,std::uint64_t expected_revision,const QString& expected_sha256) {
    const auto root_path=resolved_macro_payload_root(false);
    if(!QFileInfo::exists(root_path))throw Error("MISSING_MACRO_ASSET","The exact Macro Library asset file is missing");
    auto lock=lock_macro_asset_root(root_path);(void)lock;
    const auto current=read_stored_macro_asset(root_path,ref.asset_id);
    if(current.metadata.accepted_revision!=expected_revision||current.metadata.sha256!=expected_sha256)
        throw Error("MACRO_ASSET_REVISION_CONFLICT","Macro asset changed after selection; refresh before deleting");
    const auto path=QDir(root_path).filePath(ref.asset_id+QStringLiteral(".macro.json"));
    const auto raw=checked_macro_file_bytes(root_path,path);
    if(raw!=current.raw_envelope)throw Error("MACRO_ASSET_REVISION_CONFLICT","Macro asset bytes changed before deletion");
    const auto tombstone=QDir(root_path).filePath(ref.asset_id+QStringLiteral(".delete-")+QUuid::createUuid().toString(QUuid::WithoutBraces));
    if(!QFile::rename(path,tombstone))throw Error("MACRO_LIBRARY_DELETE_FAILED","Macro asset could not be moved atomically for deletion");
    if(QFile::remove(tombstone)&&!QFileInfo::exists(path))return;
    if(!QFileInfo::exists(path))QFile::rename(tombstone,path);
    throw Error("MACRO_LIBRARY_DELETE_FAILED","Macro asset deletion failed; the original asset was restored when possible");
}

LibraryFavoriteV1 FolderLibrary::add_favorite(const MacroAssetRefV1& macro,int quick_slot) {
    if(quick_slot<0||quick_slot>9)
        throw Error("INVALID_QUICK_SLOT","Quick Access slot must be between 1 and 9, or 0 for none");
    (void)read_macro_asset(macro);
    return add_favorite_target(LibraryFavoriteTargetV1{macro},quick_slot);
}

QJsonObject FolderLibrary::ref_to_json(const LibraryItemRefV1& ref) {
    return {{"root_id", ref.root_id}, {"normalized_relative_path", ref.normalized_relative_path}, {"kind", ref.kind}};
}

LibraryItemRefV1 FolderLibrary::ref_from_json(const QJsonObject& json) {
    if (!json.value("root_id").isString() || !json.value("normalized_relative_path").isString() || !json.value("kind").isString())
        throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library item reference is malformed");
    const auto kind = json.value("kind").toString();
    if (kind != "folder" && kind != "raster") throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library item reference has an unsupported kind");
    return {json.value("root_id").toString(), json.value("normalized_relative_path").toString(), kind};
}

QJsonObject FolderLibrary::target_to_json(const LibraryFavoriteTargetV1& target) {
    if (const auto* item = std::get_if<LibraryItemRefV1>(&target))
        return {{"kind", "library_item_v1"}, {"ref", ref_to_json(*item)}};
    if(const auto* effect=std::get_if<BuiltinEffectTypeRefV1>(&target))
        return {{"kind", "effect_type_v1"}, {"type_id", effect->type_id},
            {"behavior_version", static_cast<double>(effect->behavior_version)}};
    if(const auto* preset=std::get_if<PresetAssetRefV1>(&target))
        return {{"kind","preset_asset_v1"},{"asset_id",preset->asset_id}};
    return {{"kind","macro_asset_v1"},{"asset_id",std::get<MacroAssetRefV1>(target).asset_id}};
}

LibraryFavoriteTargetV1 FolderLibrary::target_from_json(const QJsonObject& json) {
    if (!json.value("kind").isString())
        throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library favorite target needs a tagged kind");
    const auto kind = json.value("kind").toString();
    if (kind == "library_item_v1") {
        if (!json.value("ref").isObject())
            throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library item target is malformed");
        return ref_from_json(json.value("ref").toObject());
    }
    if(kind=="preset_asset_v1") {
        const auto asset_id=json.value("asset_id").toString();
        if(!json.value("asset_id").isString()||!canonical_asset_id(asset_id))
            throw Error("INVALID_LIBRARY_SETTINGS","Folder Library Preset AssetID is malformed");
        return PresetAssetRefV1{asset_id};
    }
    if(kind=="macro_asset_v1") {
        const auto asset_id=json.value("asset_id").toString();
        if(!json.value("asset_id").isString()||!canonical_asset_id(asset_id))
            throw Error("INVALID_LIBRARY_SETTINGS","Folder Library Macro AssetID is malformed");
        return MacroAssetRefV1{asset_id};
    }
    if (kind != "effect_type_v1" || !json.value("type_id").isString() ||
        !json.value("behavior_version").isDouble())
        throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library favorite target uses an unsupported or malformed tag");
    const auto type_id = json.value("type_id").toString();
    const double version = json.value("behavior_version").toDouble();
    if (type_id.trimmed().isEmpty() || !std::isfinite(version) || std::floor(version) != version ||
        version < 1 || version > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
        throw Error("INVALID_LIBRARY_SETTINGS", "Folder Library Effect target identity is malformed");
    return BuiltinEffectTypeRefV1{type_id, static_cast<std::uint32_t>(version)};
}

}
