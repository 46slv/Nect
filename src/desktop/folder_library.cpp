#include "folder_library.hpp"

#include "nect/core.hpp"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
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

}

FolderLibrary::FolderLibrary()
    : owned_settings_(std::make_unique<QSettings>(QSettings::NativeFormat, QSettings::UserScope, "Nect", "Nect")),
      settings_(owned_settings_.get()) {
    settings_->setFallbacksEnabled(false);
    settings_->setAtomicSyncRequired(true);
}

FolderLibrary::FolderLibrary(QSettings& settings, PersistOverride persist_override,
    ReadbackOverride readback_override)
    : settings_(&settings), persist_override_(std::move(persist_override)),
      readback_override_(std::move(readback_override)) {
    settings_->setFallbacksEnabled(false);
    settings_->setAtomicSyncRequired(true);
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
    return std::get<BuiltinEffectTypeRefV1>(left) == std::get<BuiltinEffectTypeRefV1>(right);
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
    try {
        const auto resolved = resolve(std::get<LibraryItemRefV1>(favorite.target));
        return resolved.available ? QStringLiteral("Available") : resolved.problem;
    } catch (const Error& error) {
        return error_message(error);
    }
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
    const auto& effect = std::get<BuiltinEffectTypeRefV1>(target);
    return {{"kind", "effect_type_v1"}, {"type_id", effect.type_id},
        {"behavior_version", static_cast<double>(effect.behavior_version)}};
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
