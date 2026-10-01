#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QSettings>
#include <QString>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace nect::desktop {

struct LibraryRootV1 {
    QString root_id;
    QString display_name;
    QString absolute_path;
    bool enabled = true;
    bool case_sensitive = false;
};

struct LibraryItemRefV1 {
    QString root_id;
    QString normalized_relative_path;
    QString kind; // "folder" or "raster"
};

struct BuiltinEffectTypeRefV1 {
    QString type_id;
    std::uint32_t behavior_version = 1;
    bool operator==(const BuiltinEffectTypeRefV1&) const = default;
};

using LibraryFavoriteTargetV1 = std::variant<LibraryItemRefV1, BuiltinEffectTypeRefV1>;

struct LibraryItemV1 {
    LibraryItemRefV1 ref;
    QString display_name;
    QString absolute_path;
    bool available = true;
    QString problem;
};

struct LibraryFavoriteV1 {
    QString favorite_id;
    LibraryFavoriteTargetV1 target = LibraryItemRefV1{};
    int quick_slot = 0;
};

class FolderLibrary {
public:
    using PersistOverride = std::function<bool(const QByteArray&, QString&)>;
    using ReadbackOverride = std::function<std::optional<QByteArray>()>;

    FolderLibrary();
    explicit FolderLibrary(QSettings& settings, PersistOverride persist_override = {},
        ReadbackOverride readback_override = {});

    const QList<LibraryRootV1>& roots() const;
    const QList<LibraryFavoriteV1>& favorites() const;

    LibraryRootV1 register_root(const QString& path, const QString& display_name = {});
    void unregister_root(const QString& root_id);
    void refresh();

    QList<LibraryItemV1> items() const;
    QList<LibraryItemV1> browse(const QString& root_id, const QString& parent_relative_path = {}) const;
    QList<LibraryItemV1> search(const QString& query) const;
    LibraryItemV1 resolve(const LibraryItemRefV1& ref) const;

    QString comparison_key(const LibraryItemRefV1& ref) const;
    bool same_identity(const LibraryItemRefV1& left, const LibraryItemRefV1& right) const;
    bool same_identity(const LibraryFavoriteTargetV1& left, const LibraryFavoriteTargetV1& right) const;

    LibraryFavoriteV1 add_favorite(const LibraryItemRefV1& ref, int quick_slot = 0);
    LibraryFavoriteV1 add_favorite(const BuiltinEffectTypeRefV1& effect, int quick_slot = 0);
    void remove_favorite(const QString& favorite_id);
    void assign_quick_slot(const QString& favorite_id, int quick_slot);
    std::optional<LibraryFavoriteV1> favorite_for_slot(int quick_slot) const;
    QString favorite_status(const LibraryFavoriteV1& favorite) const;

    static QJsonObject ref_to_json(const LibraryItemRefV1& ref);
    static LibraryItemRefV1 ref_from_json(const QJsonObject& json);
    static QJsonObject target_to_json(const LibraryFavoriteTargetV1& target);
    static LibraryFavoriteTargetV1 target_from_json(const QJsonObject& json);

private:
    static constexpr const char* settings_key = "library/v1/state";
    std::unique_ptr<QSettings> owned_settings_;
    QSettings* settings_ = nullptr;
    PersistOverride persist_override_;
    ReadbackOverride readback_override_;
    mutable bool loaded_ = false;
    mutable QList<LibraryRootV1> roots_;
    mutable QList<LibraryFavoriteV1> favorites_;
    QList<LibraryItemV1> index_;

    void ensure_loaded() const;
    QByteArray serialize_state(const QList<LibraryRootV1>& roots,
        const QList<LibraryFavoriteV1>& favorites) const;
    std::optional<QByteArray> read_state_from_fresh_settings(bool use_override) const;
    LibraryFavoriteV1 add_favorite_target(const LibraryFavoriteTargetV1& target, int quick_slot);
    void persist_state(const QList<LibraryRootV1>& roots,
        const QList<LibraryFavoriteV1>& favorites);
    const LibraryRootV1& root(const QString& root_id) const;
    QString normalize_relative_path(const QString& path) const;
    QString comparison_key(const LibraryItemRefV1& ref, const LibraryRootV1& root) const;
    QString normalized_absolute_path(const QString& path) const;
    bool contained_by(const QString& canonical_root, const QString& canonical_candidate, bool case_sensitive) const;
    bool supported_raster(const QString& path) const;
    void scan_root(const LibraryRootV1& root, QList<LibraryItemV1>& result) const;
};

}
