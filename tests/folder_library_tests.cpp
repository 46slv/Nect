#include "folder_library.hpp"
#include "window.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDialog>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <QUuid>
#include <iostream>
#include <cmath>
#include <set>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace nect;
using namespace nect::desktop;

namespace {
int checks = 0;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}

template<class F>
void rejects(const char* code, F&& action) {
    try { action(); }
    catch (const Error& error) {
        check(error.code == code, std::string("Expected ") + code + ", got " + error.code);
        return;
    }
    throw std::runtime_error(std::string("Expected rejection ") + code);
}

std::vector<unsigned char> png(unsigned char red, unsigned char green, unsigned char blue) {
    RasterPixels pixels{4, 3, {}};
    for (int index = 0; index < 12; ++index) pixels.rgba.insert(pixels.rgba.end(), {red, green, blue, 255});
    return encode_raster_png(pixels);
}

void write_bytes(const QString& path, const std::vector<unsigned char>& bytes) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "Open owned Folder Library fixture");
    check(file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size()),
        "Write complete Folder Library fixture");
}

std::vector<unsigned char> read_bytes(const QString& path) {
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "Read owned Folder Library fixture");
    const auto bytes = file.readAll();
    return {reinterpret_cast<const unsigned char*>(bytes.constData()),
        reinterpret_cast<const unsigned char*>(bytes.constData()) + bytes.size()};
}

bool create_directory_escape_link(const QString& target, const QString& link) {
#ifdef Q_OS_WIN
    const auto native_link = QDir::toNativeSeparators(link).toStdWString();
    const auto native_target = QDir::toNativeSeparators(target).toStdWString();
    if (CreateSymbolicLinkW(native_link.c_str(), native_target.c_str(), 0x3u)) return true; // DIRECTORY | ALLOW_UNPRIVILEGED_CREATE
    const auto escape_powershell = [](QString value) { value.replace("'", "''"); return value; };
    const auto command = QStringLiteral("$ErrorActionPreference='Stop'; New-Item -ItemType Junction -Path '%1' -Target '%2' | Out-Null")
        .arg(escape_powershell(QDir::toNativeSeparators(link)), escape_powershell(QDir::toNativeSeparators(target)));
    QProcess process;
    process.start("powershell.exe", {"-NoLogo", "-NoProfile", "-NonInteractive", "-Command", command});
    if (process.waitForFinished(10000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) return true;
    return QFileInfo(link).isDir()&&QFileInfo(link).isSymLink();
#else
    return QFile::link(target, link);
#endif
}

QTreeWidgetItem* find_library_item(QTreeWidget* tree, const LibraryItemRefV1& wanted) {
    std::function<QTreeWidgetItem*(QTreeWidgetItem*)> visit = [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
        if (item) {
            const auto ref = FolderLibrary::ref_from_json(QJsonDocument::fromJson(item->data(0, Qt::UserRole).toByteArray()).object());
            if (ref.root_id == wanted.root_id && ref.normalized_relative_path == wanted.normalized_relative_path && ref.kind == wanted.kind)
                return item;
            for (int index = 0; index < item->childCount(); ++index) if (auto* found = visit(item->child(index))) return found;
        }
        return nullptr;
    };
    for (int index = 0; index < tree->topLevelItemCount(); ++index) if (auto* found = visit(tree->topLevelItem(index))) return found;
    return nullptr;
}

void model_and_persistence(const QString& scratch) {
    const auto root_a_path = scratch + "/rootA";
    const auto root_b_path = scratch + "/rootB";
    const auto outside_path = scratch + "/outside";
    QDir().mkpath(root_a_path + "/brand");
    QDir().mkpath(root_a_path + "/thumbs");
    QDir().mkpath(root_b_path);
    QDir().mkpath(outside_path);
    const auto brand_file = root_a_path + "/brand/logo.png";
    const auto thumb_file = root_a_path + "/thumbs/logo.png";
    const auto root_b_file = root_b_path + "/logo.png";
    const auto brand_bytes = png(210, 35, 60);
    const auto thumb_bytes = png(30, 190, 70);
    const auto root_b_bytes = png(40, 80, 220);
    write_bytes(brand_file, brand_bytes);write_bytes(thumb_file, thumb_bytes);write_bytes(root_b_file, root_b_bytes);
    const auto outside_file = outside_path + "/escape.png";write_bytes(outside_file, png(90, 100, 110));
    const auto unsupported_file = root_a_path + "/notes.txt";write_bytes(unsupported_file, {'n','o','t',' ','a','n',' ','i','m','a','g','e'});

    QSettings settings(scratch + "/folder-library.ini", QSettings::IniFormat);
    FolderLibrary library(settings);
    const auto root_a = library.register_root(root_a_path, "Brand source");
    const auto root_b = library.register_root(root_b_path, "Other source");
    rejects("DUPLICATE_LIBRARY_ROOT", [&] { library.register_root(root_a_path, "Duplicate"); });
    check(library.items().isEmpty(), "Registering a root does not scan it before an explicit Refresh");
    library.refresh();

    const LibraryItemRefV1 brand_ref{root_a.root_id, "brand/logo.png", "raster"};
    const LibraryItemRefV1 thumb_ref{root_a.root_id, "thumbs/logo.png", "raster"};
    const LibraryItemRefV1 other_ref{root_b.root_id, "logo.png", "raster"};
    const auto logos = library.search("logo");
    check(logos.size() == 3, "Search returns the three same-name PNGs from registered roots");
    std::set<QString> identity_keys;
    for (const auto& item : logos) identity_keys.insert(library.comparison_key(item.ref));
    check(identity_keys.size() == 3 && !library.same_identity(brand_ref, thumb_ref) && !library.same_identity(brand_ref, other_ref),
        "Same-name files in distinct paths and roots retain distinct stable identities");
    const auto root_children = library.browse(root_a.root_id);
    check(root_children.size() == 2 && root_children[0].ref.kind == "folder" && root_children[1].ref.kind == "folder",
        "Browse preserves registered folder hierarchy");
    const auto brand_children = library.browse(root_a.root_id, "brand");
    check(brand_children.size() == 1 && brand_children[0].ref.normalized_relative_path == "brand/logo.png",
        "Browsing a folder returns its direct child");

    const auto favorite = library.add_favorite(brand_ref, 1);
    check(favorite.quick_slot == 1 && std::get<LibraryItemRefV1>(library.favorite_for_slot(1)->target).normalized_relative_path == "brand/logo.png",
        "A stable raster reference can own a Quick Access slot");
    FolderLibrary after_restart(settings);
    check(after_restart.favorites().size() == 1 && after_restart.favorites().front().favorite_id == favorite.favorite_id &&
        std::get<LibraryItemRefV1>(after_restart.favorites().front().target).root_id == root_a.root_id && after_restart.favorites().front().quick_slot == 1,
        "Favorite identity and Quick Access slot persist through a fresh library instance");
    QProcess cold_settings_reader;
    cold_settings_reader.start(QCoreApplication::applicationFilePath(),
        {"--verify-settings", settings.fileName(), root_a.root_id, favorite.favorite_id});
    check(cold_settings_reader.waitForFinished(10000) &&
        cold_settings_reader.exitStatus() == QProcess::NormalExit && cold_settings_reader.exitCode() == 0,
        "Favorite identity and Quick Access slot persist into a fresh process");
    const auto other_favorite=after_restart.add_favorite(other_ref);

    const auto& resolved = after_restart.resolve(brand_ref);
    check(resolved.absolute_path == QDir::cleanPath(brand_file) && read_bytes(brand_file) == brand_bytes,
        "Resolve returns the selected source path and does not mutate its bytes");
    check(after_restart.resolve({root_a.root_id, "brand/./logo.png", "raster"}).ref.normalized_relative_path == "brand/logo.png",
        "Workspace references normalize relative separators and dot segments");
    rejects("LIBRARY_PATH_ESCAPE", [&] { after_restart.resolve({root_a.root_id, "../outside/escape.png", "raster"}); });
    rejects("UNSUPPORTED_LIBRARY_ITEM", [&] { after_restart.resolve({root_a.root_id, "notes.txt", "raster"}); });
    const auto supported_items=after_restart.items();
    check(std::none_of(supported_items.begin(), supported_items.end(), [&](const auto& item) {
        return item.ref.normalized_relative_path == "notes.txt";
    }), "Unsupported files do not enter the raster browse index");

    const auto escape_link = root_a_path + "/escape";
    check(create_directory_escape_link(outside_path, escape_link), "Create an owned symlink or junction escape fixture");
    after_restart.refresh();
    const auto escaped_items=after_restart.items();
    const auto escape = std::find_if(escaped_items.begin(), escaped_items.end(), [&](const auto& item) {
        return item.ref.root_id == root_a.root_id && item.ref.normalized_relative_path == "escape";
    });
    check(escape != escaped_items.end() && !escape->available, "An escaping reparse path is visible as unavailable");
    rejects("LIBRARY_PATH_ESCAPE", [&] { after_restart.resolve({root_a.root_id, "escape", "folder"}); });

    const auto changed_root_path = scratch + "/rootD";
    const auto changed_root_moved = scratch + "/rootD-moved";
    const auto replacement_target = scratch + "/replacement-target";
    QDir().mkpath(changed_root_path);
    QDir().mkpath(replacement_target);
    write_bytes(changed_root_path + "/logo.png", png(13, 17, 19));
    write_bytes(replacement_target + "/logo.png", png(23, 29, 31));
    const auto changed_root = after_restart.register_root(changed_root_path, "Stable root identity");
    const LibraryItemRefV1 changed_root_ref{changed_root.root_id, "logo.png", "raster"};
    const auto changed_root_favorite = after_restart.add_favorite(changed_root_ref, 2);
    check(QDir().rename(changed_root_path, changed_root_moved), "Move the owned root before its identity replacement check");
    check(create_directory_escape_link(replacement_target, changed_root_path), "Create an owned replacement junction for a registered root");
    after_restart.refresh();
    const auto changed_root_items = after_restart.items();
    const auto changed_root_item = std::find_if(changed_root_items.begin(), changed_root_items.end(), [&](const auto& item) {
        return item.ref.root_id == changed_root.root_id && item.ref.normalized_relative_path.isEmpty();
    });
    check(changed_root_item != changed_root_items.end() && !changed_root_item->available,
        "Refresh marks a registered root replaced by an external junction as unavailable");
    rejects("LIBRARY_ROOT_CHANGED", [&] { after_restart.resolve(changed_root_ref); });
    const auto broken_quick_slot = after_restart.favorite_for_slot(2);
    check(broken_quick_slot && broken_quick_slot->favorite_id == changed_root_favorite.favorite_id &&
        after_restart.favorite_status(changed_root_favorite).contains("LIBRARY_ROOT_CHANGED"),
        "A cached Quick Access favorite remains visibly broken and cannot resolve through a replacement root junction");

    const auto moved_root_b=scratch+"/rootB-moved";
    check(QDir().rename(root_b_path,moved_root_b), "Move the owned root to create a missing-root fixture");
    after_restart.refresh();
    const auto missing_items=after_restart.items();
    const auto missing_root=std::find_if(missing_items.begin(),missing_items.end(),[&](const auto& item){
        return item.ref.root_id==root_b.root_id&&item.ref.normalized_relative_path.isEmpty();
    });
    check(missing_root!=missing_items.end()&&!missing_root->available&&
        after_restart.favorite_status(other_favorite).contains("LIBRARY_ITEM_MISSING"),
        "A missing registered root remains visible and its Favorite stays broken");
    check(QDir().rename(moved_root_b,root_b_path), "Restore the owned root after missing-root check");
    after_restart.refresh();

#ifdef Q_OS_WIN
    if (!root_a.case_sensitive) {
        const LibraryItemRefV1 alternate_case{root_a.root_id, "BRAND/LOGO.PNG", "raster"};
        check(after_restart.same_identity(brand_ref, alternate_case), "Windows comparison key folds Unicode case for one file identity");
        check(after_restart.resolve(alternate_case).available, "Alternate-case Windows lookup resolves the same file");
        const auto favorite_count_before_alternate_case = after_restart.favorites().size();
        const auto duplicate_favorite = after_restart.add_favorite(alternate_case);
        check(duplicate_favorite.favorite_id == favorite.favorite_id && after_restart.favorites().size() == favorite_count_before_alternate_case,
            "Alternate-case favorite cannot create a duplicate target");
    }
#endif

    check(QFile::remove(brand_file), "Remove the owned source file to create a broken favorite");
    after_restart.refresh();
    check(after_restart.favorite_status(favorite).contains("LIBRARY_ITEM_MISSING"), "A missing Favorite remains visible as a broken stable reference");
    const auto root_c_path = scratch + "/rootC";QDir().mkpath(root_c_path + "/brand");
    write_bytes(root_c_path + "/brand/logo.png", brand_bytes);
    after_restart.unregister_root(root_a.root_id);
    const auto root_c = after_restart.register_root(root_c_path, "Replacement source");
    after_restart.refresh();
    check(root_c.root_id != root_a.root_id && after_restart.favorite_status(favorite).contains("MISSING_LIBRARY_ROOT"),
        "An unrelated same-name root does not heal a Favorite after unregister");
    const auto retained_favorites=after_restart.favorites();
    check(retained_favorites.size() == 3 && std::any_of(retained_favorites.begin(),retained_favorites.end(),[&](const auto& item){return item.favorite_id==favorite.favorite_id;}),
        "Unregistering a root leaves document and favorite refs untouched");

    QSettings failing_settings(scratch + "/failing-library.ini", QSettings::IniFormat);
    FolderLibrary failing_library(failing_settings, [](const QByteArray&, QString& error) {
        error = "injected settings write failure";return false;
    });
    rejects("SETTINGS_WRITE_FAILED", [&] { failing_library.register_root(root_b_path); });
    check(failing_library.roots().isEmpty() && !failing_settings.contains("library/v1/state"),
        "A settings write failure leaves both the visible registry and persisted value unchanged");
}

void write_qbytes(const QString& path,const QByteArray& bytes) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Open exact Folder Library byte fixture");
    check(file.write(bytes)==bytes.size(),"Write exact Folder Library byte fixture");
}

PresetDefinition portable_preset(const std::string& id,const std::string& label,double amount=18) {
    PresetDefinition definition;definition.id=id;definition.schema_version=2;definition.label=label;
    definition.category="Workspace";definition.tags={"portable","shape"};
    auto operation=default_operation("portable-template","nect.shape.offset");
    operation.parameters.at("amount").literal=amount;
    PresetEntry entry;entry.type=operation.type;entry.version=operation.version;entry.enabled=operation.enabled;
    for(const auto& [name,value]:operation.parameters)entry.parameters.emplace(name,value.literal);
    entry.composite=operation.composite;entry.fill_rule=operation.fill_rule;
    entry.line_join=operation.line_join;entry.line_cap=operation.line_cap;
    definition.entries.push_back(std::move(entry));return definition;
}

void grouped_settings_identity(const QString& scratch) {
    const auto path=scratch+"/grouped-library.ini";
    const QByteArray prior=QJsonDocument(QJsonObject{{"version",1},{"roots",QJsonArray{}},{"favorites",QJsonArray{}}})
        .toJson(QJsonDocument::Compact);
    QSettings settings(path,QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.setValue("root-sentinel",QStringLiteral("preserve-root"));
    settings.beginGroup("workspace/preferences");
    settings.setValue("group-sentinel",QStringLiteral("preserve-group"));
    settings.setValue("library/v1/state",prior);
    settings.endGroup();
    settings.setValue("unrelated/sentinel",QStringLiteral("preserve-sibling"));
    settings.sync();
    check(settings.status()==QSettings::NoError,"Write grouped INI state and unrelated sentinels in owned scratch");

    settings.beginGroup("workspace/preferences");
    FolderLibrary library(settings);
    const auto added=library.add_favorite(BuiltinEffectTypeRefV1{"nect.shape.offset",1},2);
    check(std::get<BuiltinEffectTypeRefV1>(added.target).type_id=="nect.shape.offset"&&added.quick_slot==2,
        "A grouped QSettings mutation succeeds against its exact file and group");
    settings.sync();
    QSettings persisted(path,QSettings::IniFormat);persisted.setFallbacksEnabled(false);persisted.sync();
    check(persisted.value("root-sentinel").toString()=="preserve-root"&&
        persisted.value("unrelated/sentinel").toString()=="preserve-sibling",
        "Successful grouped settings mutation preserves root, current-group and unrelated sibling sentinels");
    persisted.beginGroup("workspace/preferences");
    check(persisted.value("group-sentinel").toString()=="preserve-group",
        "Successful grouped settings mutation preserves a sentinel in its exact scope");
    const auto after_success=persisted.value("library/v1/state").toByteArray();
    check(after_success!=prior&&QJsonDocument::fromJson(after_success).object().value("favorites").toArray().size()==1,
        "Successful grouped mutation writes the Favorite at the scoped library state key");

    settings.setValue("library/v1/state",prior);
    settings.sync();
    FolderLibrary failing(settings,{},[]{return std::optional<QByteArray>{QByteArray("wrong readback namespace")};});
    rejects("SETTINGS_WRITE_FAILED",[&]{failing.add_favorite(BuiltinEffectTypeRefV1{"nect.shape.offset",1},2);});
    persisted.sync();
    check(persisted.value("library/v1/state").toByteArray()==prior&&
        persisted.value("group-sentinel").toString()=="preserve-group",
        "Grouped readback failure restores exact prior scoped bytes and preserves unrelated sentinels");
    persisted.endGroup();persisted.sync();
    check(persisted.value("root-sentinel").toString()=="preserve-root"&&
        persisted.value("unrelated/sentinel").toString()=="preserve-sibling",
        "Grouped readback rollback leaves root and sibling sentinels unchanged");
    settings.endGroup();

#ifdef Q_OS_WIN
    const auto owned_registry_path=QStringLiteral("HKEY_CURRENT_USER\\Software\\Nect\\Tests\\R10-Favorites-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    try {
        QSettings native(owned_registry_path,QSettings::NativeFormat);
        native.setFallbacksEnabled(false);
        native.beginGroup("workspace/preferences");
        native.setValue("group-sentinel",QStringLiteral("preserve-native-group"));
        native.setValue("library/v1/state",prior);
        native.endGroup();
        native.sync();
        check(native.status()==QSettings::NoError,"Create an exact owned NativeFormat scratch key");
        native.beginGroup("workspace/preferences");
        FolderLibrary native_library(native);
        const auto native_added=native_library.add_favorite(BuiltinEffectTypeRefV1{"nect.shape.offset",1},2);
        check(native_added.quick_slot==2,"NativeFormat fresh readback accepts the explicit registry backing path");
        native.sync();
        check(native.value("library/v1/state").toByteArray()!=prior&&
            native.value("group-sentinel").toString()=="preserve-native-group",
            "NativeFormat mutation keeps its exact group and unrelated sentinel");
        native.endGroup();
    } catch (...) {
        QSettings cleanup(owned_registry_path,QSettings::NativeFormat);
        cleanup.clear();cleanup.sync();
        throw;
    }
    QSettings cleanup(owned_registry_path,QSettings::NativeFormat);
    cleanup.clear();cleanup.sync();
#endif
}

void unified_effect_favorites(const QString& scratch) {
    const auto root_path=scratch+"/legacy-root";
    QDir().mkpath(root_path+"/brand");
    write_bytes(root_path+"/brand/logo.png",png(91,37,12));

    const QString root_id="legacy-root-id";
    const QString asset_favorite_id="legacy-asset-favorite";
    const QString folder_favorite_id="legacy-folder-favorite";
    const auto legacy_state=QJsonDocument(QJsonObject{
        {"version",1},
        {"roots",QJsonArray{QJsonObject{{"root_id",root_id},{"display_name","Legacy Root"},
            {"absolute_path",QDir::cleanPath(QFileInfo(root_path).absoluteFilePath())},
            {"enabled",true},{"case_sensitive",false}}}},
        {"favorites",QJsonArray{
            QJsonObject{{"favorite_id",asset_favorite_id},{"ref",FolderLibrary::ref_to_json({root_id,"brand/logo.png","raster"})},{"quick_slot",1}},
            QJsonObject{{"favorite_id",folder_favorite_id},{"ref",FolderLibrary::ref_to_json({root_id,"brand","folder"})},{"quick_slot",0}}
        }}
    }).toJson(QJsonDocument::Compact);
    const auto settings_path=scratch+"/legacy-library.ini";
    QSettings settings(settings_path,QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.setValue("library/v1/state",legacy_state);
    settings.sync();
    check(settings.status()==QSettings::NoError,"Write an owned legacy version-1 Folder Library state");

    FolderLibrary library(settings);
    const auto loaded=library.favorites();
    check(loaded.size()==2&&loaded[0].favorite_id==asset_favorite_id&&loaded[1].favorite_id==folder_favorite_id,
        "Legacy Asset and Folder Favorite IDs load unchanged from the shared version-1 state");
    check(library.roots().size()==1&&library.roots().front().root_id==root_id&&
        library.favorite_status(loaded[0])=="Available"&&library.favorite_status(loaded[1])=="Available",
        "Legacy root identity and both legacy target refs remain resolvable");
    check(std::get<LibraryItemRefV1>(loaded[0].target).normalized_relative_path=="brand/logo.png"&&
        std::get<LibraryItemRefV1>(loaded[1].target).kind=="folder",
        "Legacy file and folder references load as the library-item target kind");

    const BuiltinEffectTypeRefV1 offset{"nect.shape.offset",1};
    const auto* descriptor=builtin_operation_type(offset.type_id.toStdString());
    check(descriptor&&descriptor->effects_catalog&&descriptor->version==offset.behavior_version,
        "Effect Favorite uses the exact stable built-in TypeID and BehaviorVersion");
    const auto effect_favorite=library.add_favorite(offset,2);
    check(std::get<BuiltinEffectTypeRefV1>(effect_favorite.target)==offset&&effect_favorite.quick_slot==2&&
        library.favorite_status(effect_favorite)=="Available",
        "A built-in Effect shares the same Favorite store and has an exact available target");

    FolderLibrary restarted(settings);
    check(restarted.favorites().size()==3&&restarted.roots().size()==1,
        "A fresh library instance reads a mixed Asset/Folder/Effect state");
    check(restarted.favorites()[0].favorite_id==asset_favorite_id&&restarted.favorites()[1].favorite_id==folder_favorite_id&&
        restarted.favorites()[2].favorite_id==effect_favorite.favorite_id&&
        restarted.favorite_for_slot(1)->favorite_id==asset_favorite_id&&
        restarted.favorite_for_slot(2)->favorite_id==effect_favorite.favorite_id,
        "Existing IDs and the nine shared Quick Access slots survive a fresh library instance");
    QProcess cold_reader;
    cold_reader.start(QCoreApplication::applicationFilePath(),{"--verify-mixed-settings",settings.fileName(),root_id,
        asset_favorite_id,folder_favorite_id,effect_favorite.favorite_id});
    check(cold_reader.waitForFinished(10000)&&cold_reader.exitStatus()==QProcess::NormalExit&&cold_reader.exitCode()==0,
        "Legacy Asset/Folder refs and the exact Effect TypeID/version persist into a fresh process");
    QSettings persisted(settings.fileName(),QSettings::IniFormat);persisted.sync();
    const auto persisted_state=QJsonDocument::fromJson(persisted.value("library/v1/state").toByteArray()).object();
    check(persisted_state.value("version").toInt()==1&&persisted_state.value("roots").toArray().size()==1,
        "The extension stays in library/v1/state and preserves the registered root");

    const auto state_before_collision=persisted.value("library/v1/state").toByteArray();
    rejects("QUICK_SLOT_OCCUPIED",[&]{library.add_favorite({"nect.group.posterize",1},1);});
    rejects("QUICK_SLOT_OCCUPIED",[&]{library.assign_quick_slot(effect_favorite.favorite_id,1);});
    rejects("QUICK_SLOT_OCCUPIED",[&]{library.assign_quick_slot(folder_favorite_id,2);});
    check(library.favorites().size()==3&&library.favorite_for_slot(1)->favorite_id==asset_favorite_id&&
        library.favorite_for_slot(2)->favorite_id==effect_favorite.favorite_id,
        "Cross-kind add and slot assignment collisions preserve every in-memory Favorite");
    persisted.sync();
    check(persisted.value("library/v1/state").toByteArray()==state_before_collision,
        "Rejected cross-kind slot collisions leave the stored state byte-for-byte unchanged");

    const auto mixed_bytes=state_before_collision;
    auto changed=QJsonDocument::fromJson(mixed_bytes).object();
    auto favorites=changed.value("favorites").toArray();
    for(int i=0;i<favorites.size();++i) {
        auto item=favorites[i].toObject();
        if(item.value("favorite_id").toString()!=effect_favorite.favorite_id)continue;
        auto target=item.value("target").toObject();target.insert("behavior_version",77);item.insert("target",target);favorites[i]=item;
    }
    changed.insert("favorites",favorites);
    persisted.setValue("library/v1/state",QJsonDocument(changed).toJson(QJsonDocument::Compact));persisted.sync();
    FolderLibrary version_mismatch(persisted);
    const auto mismatch=std::find_if(version_mismatch.favorites().begin(),version_mismatch.favorites().end(),[&](const auto& item) {
        return item.favorite_id==effect_favorite.favorite_id;
    });
    check(mismatch!=version_mismatch.favorites().end()&&
        std::get<BuiltinEffectTypeRefV1>(mismatch->target).type_id==offset.type_id&&
        version_mismatch.favorite_status(*mismatch).contains("EFFECT_BEHAVIOR_VERSION_MISMATCH"),
        "A version-mismatched Effect remains visible under its original exact target and Favorite ID");
    persisted.setValue("library/v1/state",mixed_bytes);persisted.sync();
    FolderLibrary restored(persisted);
    const auto restored_favorite=std::find_if(restored.favorites().begin(),restored.favorites().end(),[&](const auto& item) {
        return item.favorite_id==effect_favorite.favorite_id;
    });
    check(restored_favorite!=restored.favorites().end()&&std::get<BuiltinEffectTypeRefV1>(restored_favorite->target)==offset&&
        restored.favorite_status(*restored_favorite)=="Available",
        "Restoring the exact TypeID/version makes the same Favorite ID usable again");
    const auto unavailable=restored.add_favorite({"extension.missing.effect",4});
    check(restored.favorite_status(unavailable).contains("UNAVAILABLE_EFFECT_TYPE")&&
        std::get<BuiltinEffectTypeRefV1>(unavailable.target).type_id=="extension.missing.effect",
        "An unavailable exact Effect TypeID stays visible without name-based substitution");

    auto rejects_malformed_state=[&](QJsonObject malformed,const QString& name) {
        QSettings malformed_settings(scratch+"/"+name+".ini",QSettings::IniFormat);
        malformed_settings.setValue("library/v1/state",QJsonDocument(malformed).toJson(QJsonDocument::Compact));
        malformed_settings.sync();
        FolderLibrary malformed_library(malformed_settings);
        rejects("INVALID_LIBRARY_SETTINGS",[&]{(void)malformed_library.favorites();});
    };
    auto fractional_version=QJsonDocument::fromJson(mixed_bytes).object();
    auto fractional_favorites=fractional_version.value("favorites").toArray();
    for(int i=0;i<fractional_favorites.size();++i) {
        auto item=fractional_favorites[i].toObject();
        if(item.value("favorite_id").toString()==effect_favorite.favorite_id) {
            auto target=item.value("target").toObject();target.insert("behavior_version",1.5);item.insert("target",target);fractional_favorites[i]=item;
        }
    }
    fractional_version.insert("favorites",fractional_favorites);
    rejects_malformed_state(fractional_version,"fractional-effect-version");
    auto fractional_slot=QJsonDocument::fromJson(mixed_bytes).object();
    auto fractional_slot_favorites=fractional_slot.value("favorites").toArray();
    auto fractional_asset=fractional_slot_favorites[0].toObject();fractional_asset.insert("quick_slot",1.5);
    fractional_slot_favorites[0]=fractional_asset;fractional_slot.insert("favorites",fractional_slot_favorites);
    rejects_malformed_state(fractional_slot,"fractional-quick-slot");
    auto unknown_tag=QJsonDocument::fromJson(mixed_bytes).object();
    auto unknown_tag_favorites=unknown_tag.value("favorites").toArray();
    auto unknown_effect=unknown_tag_favorites[2].toObject();auto malformed_target=unknown_effect.value("target").toObject();
    malformed_target.insert("kind","display_name");unknown_effect.insert("target",malformed_target);unknown_tag_favorites[2]=unknown_effect;
    unknown_tag.insert("favorites",unknown_tag_favorites);
    rejects_malformed_state(unknown_tag,"unknown-favorite-tag");

    QSettings failing_settings(scratch+"/persist-failure.ini",QSettings::IniFormat);
    failing_settings.setValue("library/v1/state",mixed_bytes);failing_settings.sync();
    FolderLibrary failing_library(failing_settings,[](const QByteArray&,QString& error) {
        error="injected settings write failure";return false;
    });
    rejects("SETTINGS_WRITE_FAILED",[&]{failing_library.add_favorite({"nect.group.posterize",1});});
    QSettings failure_reader(failing_settings.fileName(),QSettings::IniFormat);failure_reader.sync();
    check(failing_library.favorites().size()==3&&failure_reader.value("library/v1/state").toByteArray()==mixed_bytes,
        "A refused settings write preserves the prior in-memory and persisted mixed Favorite state");

    QSettings readback_settings(scratch+"/readback-failure.ini",QSettings::IniFormat);
    readback_settings.setValue("library/v1/state",mixed_bytes);readback_settings.sync();
    FolderLibrary readback_library(readback_settings,{},[mixed_bytes]{return std::optional<QByteArray>{mixed_bytes};});
    rejects("SETTINGS_WRITE_FAILED",[&]{readback_library.add_favorite({"nect.group.posterize",1});});
    QSettings readback_reader(readback_settings.fileName(),QSettings::IniFormat);readback_reader.sync();
    check(readback_library.favorites().size()==3&&readback_reader.value("library/v1/state").toByteArray()==mixed_bytes,
        "A fresh-reader mismatch rolls back both the in-memory and independently read persisted state");

    QSettings unreadable_settings(scratch+"/unreadable-readback.ini",QSettings::IniFormat);
    unreadable_settings.setValue("library/v1/state",mixed_bytes);unreadable_settings.sync();
    FolderLibrary unreadable_library(unreadable_settings,{},[]() -> std::optional<QByteArray> {
        throw Error("SETTINGS_READ_FAILED","injected fresh-reader failure");
    });
    rejects("SETTINGS_WRITE_FAILED",[&]{unreadable_library.add_favorite({"nect.group.posterize",1});});
    QSettings unreadable_reader(unreadable_settings.fileName(),QSettings::IniFormat);unreadable_reader.sync();
    check(unreadable_library.favorites().size()==3&&
        unreadable_reader.value("library/v1/state").toByteArray()==mixed_bytes,
        "An unavailable fresh-reader confirmation rolls back the stored and in-memory prior state");
}

void portable_preset_assets(const QString& scratch) {
    const auto root=scratch+"/preset-payloads";
    const auto settings_path=scratch+"/preset-library.ini";
    QSettings settings(settings_path,QSettings::IniFormat);
    FolderLibrary library(settings,{}, {},root);
    check(library.preset_assets().isEmpty()&&!QFileInfo::exists(root),
        "Injected payload-root listing is empty and read-only until the first explicit publication");

    const auto source=portable_preset("portable-source-definition","Portable Offset");
    const auto published=library.publish_preset(source);
    const auto good_source=portable_preset("second-portable-source","Second Good Asset",31);
    const auto good_asset=library.publish_preset(good_source);
    const auto asset_path=QDir(root).filePath(published.ref.asset_id+".preset.json");
    check(QFileInfo::exists(asset_path)&&published.accepted_revision==1&&published.ref.asset_id!=QString::fromStdString(source.id)&&
        published.payload_schema==2&&published.sha256.size()==64,
        "Publication creates one revision-1 workspace AssetID distinct from the source DefinitionID");
    QSettings no_payload_store(settings_path,QSettings::IniFormat);no_payload_store.sync();
    check(!no_payload_store.contains("library/v1/state"),
        "Preset definition payload is stored as its bounded Library file, not a QSettings payload value");
    LibraryPresetAssetV1 read_metadata;
    const auto read_definition=library.read_preset_asset(published.ref,&read_metadata);
    check(canonical_preset_payload(read_definition)==canonical_preset_payload(source)&&
        read_metadata.ref==published.ref&&read_metadata.label==published.label&&
        read_metadata.accepted_revision==published.accepted_revision&&read_metadata.sha256==published.sha256&&
        read_metadata.payload_schema==published.payload_schema,
        "Exact AssetID readback returns the canonical built-in literal payload and metadata");

    const auto original=read_bytes(asset_path);
    const auto original_bytes=QByteArray(reinterpret_cast<const char*>(original.data()),static_cast<qsizetype>(original.size()));
    const auto envelope=QJsonDocument::fromJson(original_bytes).object();
    auto rejects_envelope_value=[&](const QString& name,const QJsonValue& value,const char* code) {
        auto changed=envelope;changed.insert(name,value);
        write_qbytes(asset_path,QJsonDocument(changed).toJson(QJsonDocument::Compact));
        rejects(code,[&]{(void)library.read_preset_asset(published.ref);});
        write_qbytes(asset_path,original_bytes);
    };
    rejects_envelope_value("version",1.5,"INVALID_PRESET_ASSET");
    rejects_envelope_value("version",2,"UNSUPPORTED_PRESET_ASSET_VERSION");
    rejects_envelope_value("accepted_revision",-1,"INVALID_PRESET_ASSET");
    rejects_envelope_value("accepted_revision",1.5,"INVALID_PRESET_ASSET");
    rejects_envelope_value("accepted_revision","1","INVALID_PRESET_ASSET");
    rejects_envelope_value("payload_schema",2.25,"INVALID_PRESET_ASSET");
    rejects_envelope_value("payload_schema",99,"UNSUPPORTED_PRESET_SCHEMA");
    auto corrupt=envelope;corrupt.insert("sha256",QString(64,'0'));
    write_qbytes(asset_path,QJsonDocument(corrupt).toJson(QJsonDocument::Compact));
    const auto mixed_inventory=library.preset_assets();
    const auto unavailable_first=std::find_if(mixed_inventory.begin(),mixed_inventory.end(),[&](const auto& asset){return asset.ref==published.ref;});
    const auto available_second=std::find_if(mixed_inventory.begin(),mixed_inventory.end(),[&](const auto& asset){return asset.ref==good_asset.ref;});
    check(unavailable_first!=mixed_inventory.end()&&!unavailable_first->available&&
        available_second!=mixed_inventory.end()&&available_second->available&&library.read_preset_asset(good_asset.ref).label==good_source.label,
        "A corrupt hash remains visible as unavailable while a separate valid asset stays readable");
    rejects("PRESET_ASSET_HASH_MISMATCH",[&]{(void)library.read_preset_asset(published.ref);});
    write_qbytes(asset_path,original_bytes);
    const auto inventory=library.preset_assets();
    const auto good_entry=std::find_if(inventory.begin(),inventory.end(),[&](const auto& asset){return asset.ref==good_asset.ref;});
    check(good_entry!=inventory.end()&&good_entry->available&&
        canonical_preset_payload(library.read_preset_asset(good_asset.ref))==canonical_preset_payload(good_source),
        "A separate valid asset remains usable while another record is malformed or has an unsupported envelope/schema");

    const auto alias_id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto alias_path=QDir(root).filePath(alias_id+".preset.json");
    write_qbytes(alias_path,original_bytes);
    rejects("PRESET_ASSET_ID_MISMATCH",[&]{(void)library.read_preset_asset({alias_id});});
    check(QFile::remove(alias_path),"Remove owned wrong-filename Preset fixture");

    write_qbytes(asset_path,QByteArray(512*1024+1,'x'));
    rejects("PRESET_ASSET_LIMIT",[&]{(void)library.read_preset_asset(published.ref);});
    write_qbytes(asset_path,original_bytes);

    const auto favorite=library.add_favorite(published.ref,1);
    check(library.favorite_status(favorite)=="Available"&&library.favorite_for_slot(1)->favorite_id==favorite.favorite_id,
        "A portable Preset shares the Favorite store and exact Quick Access slot");
    QProcess cold_reader;
    cold_reader.start(QCoreApplication::applicationFilePath(),{"--verify-portable-preset",settings_path,root,
        favorite.favorite_id,published.ref.asset_id,QString::fromStdString(source.id)});
    const auto cold_reader_ok=cold_reader.waitForFinished(10000)&&cold_reader.exitStatus()==QProcess::NormalExit&&cold_reader.exitCode()==0;
    const auto cold_reader_diagnostic=QString::fromUtf8(cold_reader.readAllStandardError());
    check(cold_reader_ok,"Preset AssetID, payload, fresh-target apply and shared Favorite survive a separate process: "+
        cold_reader_diagnostic.toStdString());

    auto source_edited=source;source_edited.label="Source document renamed";
    source_edited.entries.front().parameters.at("amount")=6;
    const auto original_again=library.read_preset_asset(published.ref);
    check(original_again.label==source.label&&original_again.entries.front().parameters.at("amount")==18,
        "Editing a source definition after publication does not change the independent Library bytes");
    auto updated_source=source;updated_source.label="Portable Offset Updated";
    updated_source.entries.front().parameters.at("amount")=24;
    const auto updated=library.update_preset_asset(published.ref,updated_source,published.accepted_revision,published.sha256);
    check(updated.ref==published.ref&&updated.accepted_revision==2&&
        canonical_preset_payload(library.read_preset_asset(published.ref))==canonical_preset_payload(updated_source)&&
        library.favorites().front().favorite_id==favorite.favorite_id&&library.favorite_status(favorite)=="Available",
        "Update advances only the asset revision while preserving AssetID, source payload and Favorite identity");
    rejects("PRESET_ASSET_REVISION_CONFLICT",[&]{
        (void)library.update_preset_asset(published.ref,source,published.accepted_revision,published.sha256);
    });
    rejects("PRESET_ASSET_REVISION_CONFLICT",[&]{
        library.delete_preset_asset(published.ref,published.accepted_revision,published.sha256);
    });
    FolderLibrary restarted(settings,{}, {},root);
    check(canonical_preset_payload(restarted.read_preset_asset(published.ref))==canonical_preset_payload(updated_source)&&
        restarted.favorite_for_slot(1)->favorite_id==favorite.favorite_id,
        "Fresh Library instance re-reads the updated payload and persisted shared slot");

    auto macro=source;macro.entries.front().kind="macro";
    rejects("PRESET_NONPORTABLE_SOURCE",[&]{(void)library.publish_preset(macro);});

    auto invalid=source;invalid.entries.front().parameters.at("amount")=1.0e9;
    rejects("OUT_OF_RANGE",[&]{(void)library.publish_preset(invalid);});
    invalid=source;invalid.entries.front().composite="above";
    rejects("INVALID_OPERATOR_OPTIONS",[&]{(void)library.publish_preset(invalid);});
    invalid=source;invalid.tags.push_back(invalid.tags.front());
    rejects("INVALID_PRESET_METADATA",[&]{(void)library.publish_preset(invalid);});

    auto v1=portable_preset("portable-v1-source","Portable v1");v1.schema_version=1;
    auto repeater=default_operation("portable-repeater-template","nect.shape.repeater");
    repeater.parameters.at("copies").literal=4;repeater.parameters.at("position_x").literal=36;
    PresetEntry repeater_entry;repeater_entry.type=repeater.type;repeater_entry.version=repeater.version;
    for(const auto& [name,value]:repeater.parameters)repeater_entry.parameters.emplace(name,value.literal);
    repeater_entry.composite=repeater.composite;repeater_entry.fill_rule=repeater.fill_rule;
    v1.entries.push_back(std::move(repeater_entry));
    const auto v1_payload=canonical_preset_payload(v1);
    check(canonical_preset_payload(read_canonical_preset_payload(v1_payload))==v1_payload,
        "Canonical facade reuses native schema-v1 Offset/Repeater literal validation");
    auto styled=portable_preset("portable-stroke-v2","Styled Stroke v2");
    styled.entries.clear();
    auto stroke=default_operation("portable-stroke-template","nect.paint.stroke");
    PresetEntry stroke_entry;stroke_entry.type=stroke.type;stroke_entry.version=2;
    for(const auto& [name,value]:stroke.parameters)stroke_entry.parameters.emplace(name,value.literal);
    stroke_entry.parameters.emplace("miter_limit",4);
    stroke_entry.line_join="round";stroke_entry.line_cap="square";
    styled.entries.push_back(std::move(stroke_entry));
    const auto styled_payload=canonical_preset_payload(styled);
    check(canonical_preset_payload(read_canonical_preset_payload(styled_payload))==styled_payload,
        "Canonical facade reuses native styled Stroke@2 support rather than a partial Library schema");

    const auto effect_favorite=library.add_favorite(BuiltinEffectTypeRefV1{"nect.shape.offset",1},2);
    const auto prior_favorites=library.favorites().size();
    rejects("QUICK_SLOT_OCCUPIED",[&]{(void)library.add_favorite(good_asset.ref,2);});
    check(library.favorites().size()==prior_favorites&&library.favorite_for_slot(2)->favorite_id==effect_favorite.favorite_id,
        "Cross-kind Preset/Effect slot collision leaves both stable Favorites unchanged");
    FolderLibrary failed_settings(settings,[](const QByteArray&,QString& error) {
        error="injected preference write failure";return false;
    },{},root);
    rejects("SETTINGS_WRITE_FAILED",[&]{(void)failed_settings.add_favorite(good_asset.ref,3);});
    check(library.favorites().size()==prior_favorites&&
        canonical_preset_payload(library.read_preset_asset(good_asset.ref))==canonical_preset_payload(good_source)&&
        !library.favorite_for_slot(3),
        "Failed Favorite preference persistence preserves the published payload and every prior Favorite");

    FolderLibrary fail_first(settings,{}, {},root,[](const QString&,const QByteArray&,QString& error) {
        error="injected first-write failure";return false;
    });
    const auto before_failed_update=read_bytes(asset_path);
    auto failed_candidate=updated_source;failed_candidate.label="Must Not Publish";
    rejects("PRESET_LIBRARY_WRITE_FAILED",[&]{
        (void)fail_first.update_preset_asset(published.ref,failed_candidate,updated.accepted_revision,updated.sha256);
    });
    check(read_bytes(asset_path)==before_failed_update,
        "A first-write failure preserves the exact published bytes and revision");

    const QByteArray external_replacement("uncooperative replacement");
    FolderLibrary readback_race(settings,{}, {},root,[&](const QString& path,const QByteArray&,QString&) {
        write_qbytes(path,external_replacement);return true;
    });
    rejects("PRESET_LIBRARY_ROLLBACK_FAILED",[&]{
        (void)readback_race.update_preset_asset(published.ref,failed_candidate,updated.accepted_revision,updated.sha256);
    });
    const auto replacement=read_bytes(asset_path);
    check(QByteArray(reinterpret_cast<const char*>(replacement.data()),static_cast<qsizetype>(replacement.size()))==external_replacement,
        "Readback ambiguity preserves a different observed file instead of rolling it back");
    write_qbytes(asset_path,QByteArray(reinterpret_cast<const char*>(before_failed_update.data()),
        static_cast<qsizetype>(before_failed_update.size())));

    const auto outside=scratch+"/outside";const auto escaped_root=scratch+"/preset-root-link";QDir().mkpath(outside);
    if(create_directory_escape_link(outside,escaped_root)) {
        FolderLibrary escaped(settings,{}, {},escaped_root);
        rejects("UNSAFE_PRESET_LIBRARY_ROOT",[&]{(void)escaped.read_preset_asset(published.ref);});
    }
    library.delete_preset_asset(published.ref,updated.accepted_revision,updated.sha256);
    const auto remaining_assets=library.preset_assets();
    check(std::none_of(remaining_assets.begin(),remaining_assets.end(),[&](const auto& item){return item.ref==published.ref;})&&
        std::any_of(remaining_assets.begin(),remaining_assets.end(),[&](const auto& item){return item.ref==good_asset.ref;})&&
        library.favorite_status(favorite).contains("MISSING_PRESET_ASSET"),
        "Delete removes only the selected AssetID, retains a separate good asset and leaves its Favorite as an explicit broken reference");
}

void host_and_ui_placement(const QString& scratch) {
    const auto root_path=scratch+"/assets";QDir().mkpath(root_path+"/brand");
    const auto linked_path=root_path+"/brand/logo.png";const auto embedded_path=root_path+"/paper.png";
    const auto linked_bytes=png(180,55,25),embedded_bytes=png(15,120,210);
    write_bytes(linked_path,linked_bytes);write_bytes(embedded_path,embedded_bytes);
    QSettings settings(scratch+"/placement-library.ini",QSettings::IniFormat);
    auto library=std::make_unique<FolderLibrary>(settings);
    const auto root=library->register_root(root_path,"Assets");library->refresh();

    Host host(scratch+"/host-recovery");
    const auto composition=host.session.document().compositions.front().id;
    const auto linked_entry=library->resolve({root.root_id,"brand/logo.png","raster"});
    const auto embedded_entry=library->resolve({root.root_id,"paper.png","raster"});
    const auto linked_id=new_id(),linked_object=new_id(),embedded_id=new_id(),embedded_object=new_id();
    host.import_image(linked_entry.absolute_path,"linked",composition,"",linked_id,linked_object,"logo",0,0,host.session.revision());
    host.import_image(embedded_entry.absolute_path,"embedded",composition,"",embedded_id,embedded_object,"paper",20,30,host.session.revision());
    const auto before_stale=host.session.document();
    rejects("REVISION_CONFLICT", [&] { host.import_image(linked_entry.absolute_path,"embedded",composition,"",new_id(),new_id(),"stale",0,0,0); });
    check(host.session.document()==before_stale, "A stale Host import leaves the document unchanged");
    check(host.session.document().raster_assets.at(linked_id).mode=="linked" &&
        host.session.document().raster_assets.at(linked_id).locator==linked_entry.absolute_path.toStdString() &&
        host.session.document().raster_assets.at(linked_id).payload->bytes()==linked_bytes,
        "Folder Library Linked placement retains exact locator and accepted pixels");
    check(host.session.document().raster_assets.at(embedded_id).mode=="embedded" &&
        host.session.document().raster_assets.at(embedded_id).locator.empty() &&
        host.session.document().raster_assets.at(embedded_id).payload->bytes()==embedded_bytes,
        "Folder Library Embedded placement keeps accepted bytes without an external locator");

    const auto native_path=scratch+"/placed.nect";host.save(native_path);
    const auto save_as_path=scratch+"/placed-save-as.nect";host.save(save_as_path);const auto saved=encode(host.session.document());
    Host cold_reopen(scratch+"/cold-reopen-recovery");cold_reopen.open(save_as_path);
    check(encode(cold_reopen.session.document())==saved, "Linked and Embedded placements survive Save As and a fresh Host reopen");
    check(cold_reopen.session.document().raster_assets.at(linked_id).locator==linked_entry.absolute_path.toStdString()&&
        cold_reopen.session.document().raster_assets.at(linked_id).payload->bytes()==linked_bytes&&
        cold_reopen.session.document().raster_assets.at(embedded_id).locator.empty()&&
        cold_reopen.session.document().raster_assets.at(embedded_id).payload->bytes()==embedded_bytes,
        "Fresh Host reopen preserves the exact Linked locator and accepted Linked/Embedded pixels");
    check(read_bytes(linked_path)==linked_bytes && read_bytes(embedded_path)==embedded_bytes,
        "Placement and save leave both source files unchanged");
    check(QFile::remove(linked_path) && QFile::remove(embedded_path), "Remove owned placement sources after cold reopen");
    check(cold_reopen.check_asset(linked_id)["state"]=="missing" &&
        cold_reopen.session.document().raster_assets.at(linked_id).payload->bytes()==linked_bytes &&
        cold_reopen.session.document().raster_assets.at(embedded_id).payload->bytes()==embedded_bytes,
        "A missing Linked source reports broken while both assets retain their accepted pixels");
    Host no_source_reopen(scratch+"/no-source-recovery");no_source_reopen.open(save_as_path);
    check(no_source_reopen.check_asset(linked_id)["state"]=="missing" &&
        no_source_reopen.session.document().raster_assets.at(linked_id).payload->bytes()==linked_bytes &&
        no_source_reopen.session.document().raster_assets.at(embedded_id).locator.empty() &&
        no_source_reopen.session.document().raster_assets.at(embedded_id).payload->bytes()==embedded_bytes,
        "Linked and Embedded accepted pixels survive a fresh cold reopen after source removal");
    host.flush();cold_reopen.flush();no_source_reopen.flush();

    const auto ui_path=root_path+"/brand/logo.png";write_bytes(ui_path,linked_bytes);
    auto ui_library=std::make_unique<FolderLibrary>(settings);
    Window window(scratch+"/ui-recovery",std::move(ui_library));window.show();QApplication::processEvents();
    QTimer::singleShot(0,&window,[&] {
        auto* dialog=window.findChild<QDialog*>("folder-library-dialog");check(dialog,"Folder Library menu opens its dialog");
        auto* tree=dialog->findChild<QTreeWidget*>("folder-library-tree");check(tree,"Folder Library exposes a hierarchical tree");
        const auto ref=LibraryItemRefV1{root.root_id,"brand/logo.png","raster"};
        check(!find_library_item(tree,ref), "Opening Folder Library does not scan a registered root automatically");
        auto* refresh=dialog->findChild<QPushButton*>("folder-library-refresh");check(refresh,"Folder Library exposes explicit Refresh");
        QTest::mouseClick(refresh,Qt::LeftButton);QApplication::processEvents();
        auto* item=find_library_item(tree,ref);check(item,"Folder Library tree includes the registered raster path");
        tree->setCurrentItem(item);
        auto* favorite_button=dialog->findChild<QPushButton*>("folder-library-favorite-add");check(favorite_button,"Folder Library has a Favorite action");
        const auto preference_document=encode(window.host.session.document());
        const auto preference_revision=window.host.session.revision();
        const auto preference_history=window.host.session.history();
        QTest::mouseClick(favorite_button,Qt::LeftButton);
        auto* favorite_list=dialog->findChild<QListWidget*>("folder-library-favorites");check(favorite_list&&favorite_list->count()==1,"Folder Library displays the persistent Favorite");
        check(encode(window.host.session.document())==preference_document&&window.host.session.revision()==preference_revision&&
            window.host.session.history()==preference_history,
            "Adding an Asset Favorite leaves Document bytes, revision and History unchanged");
        favorite_list->setCurrentRow(0);
        auto* slot=dialog->findChild<QComboBox*>("folder-library-slot");check(slot,"Folder Library exposes Quick Access slots");slot->setCurrentIndex(slot->findData(1));
        auto* assign=dialog->findChild<QPushButton*>("folder-library-slot-set");check(assign,"Folder Library can assign the selected Favorite to a slot");
        QTest::mouseClick(assign,Qt::LeftButton);
        check(encode(window.host.session.document())==preference_document&&window.host.session.revision()==preference_revision&&
            window.host.session.history()==preference_history,
            "Assigning an Asset Quick Access slot leaves Document bytes, revision and History unchanged");
        auto* use_slot=dialog->findChild<QPushButton*>("folder-library-use-slot");check(use_slot,"Folder Library can invoke Quick Access");
        QTest::mouseClick(use_slot,Qt::LeftButton);QApplication::processEvents();
        check(window.host.session.document().objects.size()==1,"Folder Library UI places an Image through Host");
        const auto object=window.host.session.document().objects.begin()->second;
        const auto& asset=window.host.session.document().raster_assets.at(object.image->asset);
        check(asset.mode=="linked" && asset.locator==ui_path.toStdString() && asset.payload->bytes()==linked_bytes,
            "UI placement reuses exact Linked locator and pixel semantics");
        dialog->accept();
    });
    auto* open_library=window.findChild<QAction*>("folder-library");check(open_library,"Library menu action exists");open_library->trigger();
    check(window.host.session.document().objects.size()==1,"Folder Library dialog completes the UI placement interaction");
    window.close();window.host.flush();
}

void portable_preset_library_ui(const QString& scratch) {
    const auto settings_path=scratch+"/preset-ui.ini";
    const auto payload_root=scratch+"/preset-assets";
    QSettings settings(settings_path,QSettings::IniFormat);
    FolderLibrary seeded(settings,{}, {},payload_root);
    const auto asset_a=seeded.publish_preset(portable_preset("ui-payload-a","A Portable Preset",18));
    const auto asset_b=seeded.publish_preset(portable_preset("ui-payload-b","B Portable Preset",25));
    const auto effect=seeded.add_favorite(BuiltinEffectTypeRefV1{"nect.shape.offset",1},2);
    auto injected=std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},
        FolderLibrary::ReadbackOverride{},payload_root);
    Window window(scratch+"/recovery",std::move(injected));window.show();QApplication::processEvents();

    auto document_source=portable_preset("ui-document-preset","UI Document Source",12);
    window.host.session.apply_preset_command(PresetCommand{CreatePreset{document_source}},window.host.session.revision());
    const auto composition=window.host.session.document().compositions.front().id;
    const auto object_id=new_id();
    auto primitive=default_primitive(new_id(),"nect.shape.rectangle");
    window.host.session.apply({Command{CreatePrimitive{composition,"",object_id,"Portable Preset target",primitive}}},
        window.host.session.revision());
    window.canvas->set_selection(object_id);window.host.edited();QApplication::processEvents();

    auto* publish=window.findChild<QPushButton*>("preset-publish-library");
    auto* preset_catalog=window.findChild<QListWidget*>("presets-catalog");
    check(publish&&preset_catalog,"Presets panel exposes Publish to Library for a selected document definition");
    for(int row=0;row<preset_catalog->count();++row)if(preset_catalog->item(row)->data(Qt::UserRole).toString()==
        QString::fromStdString(document_source.id))preset_catalog->setCurrentRow(row);
    const auto document_before_publish=encode(window.host.session.document());
    const auto revision_before_publish=window.host.session.revision();
    const auto history_before_publish=window.host.session.history();
    QTest::mouseClick(publish,Qt::LeftButton);QApplication::processEvents();
    check(encode(window.host.session.document())==document_before_publish&&
        window.host.session.revision()==revision_before_publish&&window.host.session.history()==history_before_publish,
        "Publishing a selected source Preset leaves Document bytes, revision and History unchanged");

    QTimer::singleShot(0,&window,[&] {
        auto* dialog=window.findChild<QDialog*>("folder-library-dialog");check(dialog,"Library opens with injected payload root");
        auto* assets=dialog->findChild<QComboBox*>("folder-library-preset-assets");
        auto* source_presets=dialog->findChild<QComboBox*>("folder-library-source-presets");
        auto* add_favorite=dialog->findChild<QPushButton*>("folder-library-favorite-add");
        auto* favorites=dialog->findChild<QListWidget*>("folder-library-favorites");
        auto* slot=dialog->findChild<QComboBox*>("folder-library-slot");
        auto* assign=dialog->findChild<QPushButton*>("folder-library-slot-set");
        auto* use_favorite=dialog->findChild<QPushButton*>("folder-library-use-favorite");
        auto* use_slot=dialog->findChild<QPushButton*>("folder-library-use-slot");
        auto* update=dialog->findChild<QPushButton*>("folder-library-preset-update");
        auto* remove=dialog->findChild<QPushButton*>("folder-library-preset-delete");
        auto* status=dialog->findChild<QLabel*>("folder-library-status");
        check(assets&&source_presets&&add_favorite&&favorites&&slot&&assign&&use_favorite&&use_slot&&update&&remove&&status,
            "Workspace asset, source Preset, Favorite and guarded apply controls are available");
        const auto asset_a_index=assets->findData(asset_a.ref.asset_id);
        const auto asset_b_index=assets->findData(asset_b.ref.asset_id);
        check(asset_a_index>0&&asset_b_index>0,"Both exact workspace AssetIDs appear in the Library selector");
        assets->setCurrentIndex(asset_a_index);
        const auto source_index=source_presets->findData(QString::fromStdString(document_source.id));
        check(source_index>0,"The selected document Preset is an explicit update source");
        source_presets->setCurrentIndex(source_index);

        const auto document_before_preferences=encode(window.host.session.document());
        const auto revision_before_preferences=window.host.session.revision();
        const auto history_before_preferences=window.host.session.history();
        QTest::mouseClick(add_favorite,Qt::LeftButton);
        check(favorites->count()==2&&encode(window.host.session.document())==document_before_preferences&&
            window.host.session.revision()==revision_before_preferences&&window.host.session.history()==history_before_preferences,
            "Adding a Preset Favorite changes only the shared preference record");
        const auto find_preset_favorite=[&]() -> QListWidgetItem* {
            for(int row=0;row<favorites->count();++row) {
                const auto target=FolderLibrary::target_from_json(QJsonDocument::fromJson(
                    favorites->item(row)->data(Qt::UserRole+1).toByteArray()).object());
                if(const auto* preset=std::get_if<PresetAssetRefV1>(&target);preset&&preset->asset_id==asset_a.ref.asset_id)
                    return favorites->item(row);
            }
            return nullptr;
        };
        auto* preset_favorite=find_preset_favorite();
        check(preset_favorite,"Favorite stores the exact workspace AssetID");
        const auto favorite_id=preset_favorite->data(Qt::UserRole).toString();
        favorites->setCurrentItem(preset_favorite);slot->setCurrentIndex(slot->findData(1));
        QTest::mouseClick(assign,Qt::LeftButton);
        check(encode(window.host.session.document())==document_before_preferences&&
            window.host.session.revision()==revision_before_preferences&&window.host.session.history()==history_before_preferences,
            "Assigning a Preset quick slot leaves Document bytes, revision and History unchanged");
        assets->setCurrentIndex(assets->findData(asset_b.ref.asset_id));
        const auto before_update_document=encode(window.host.session.document());
        const auto before_update_revision=window.host.session.revision();
        const auto before_update_history=window.host.session.history();
        QTest::mouseClick(update,Qt::LeftButton);QApplication::processEvents();
        QSettings ui_reader_settings(settings_path,QSettings::IniFormat);
        FolderLibrary ui_reader(ui_reader_settings,{}, {},payload_root);
        const auto ui_updated=ui_reader.preset_assets();
        const auto updated_b=std::find_if(ui_updated.begin(),ui_updated.end(),[&](const auto& item){return item.ref==asset_b.ref;});
        check(updated_b!=ui_updated.end()&&updated_b->accepted_revision==2&&
            canonical_preset_payload(ui_reader.read_preset_asset(asset_b.ref))==canonical_preset_payload(document_source)&&
            encode(window.host.session.document())==before_update_document&&window.host.session.revision()==before_update_revision&&
            window.host.session.history()==before_update_history,
            "Explicit Update copies the selected source Preset while preserving document state and asset identity");

        preset_favorite=find_preset_favorite();check(preset_favorite&&preset_favorite->data(Qt::UserRole).toString()==favorite_id,
            "Asset refresh keeps the exact Favorite identity after an unrelated explicit update");
        assets->setCurrentIndex(assets->findData(asset_a.ref.asset_id));
        favorites->setCurrentItem(preset_favorite);
        auto* favorite_slot=dialog->findChild<QPushButton*>("folder-library-use-favorite");
        const auto target_before=window.host.session.document().objects.at(object_id).stack.size();
        const auto imported_definitions_before=window.host.session.document().preset_definitions.size();
        const auto revision_before_import=window.host.session.revision();
        const auto history_before_import=window.host.session.history().states.size();
        QTest::mouseClick(favorite_slot,Qt::LeftButton);QApplication::processEvents();
        check(window.host.session.revision()==revision_before_import+1&&
            window.host.session.history().states.size()==history_before_import+1&&
            window.host.session.document().objects.at(object_id).stack.size()==target_before+1&&
            window.host.session.document().preset_definitions.size()==imported_definitions_before+1&&
            window.host.session.document().objects.at(object_id).stack.back().type=="nect.shape.offset"&&
            window.host.session.document().objects.at(object_id).stack.back().parameters.at("amount").literal==18,
            "Favorite imports a fresh definition and exact fixed literal to the captured target in one Session step");

        QListWidgetItem* effect_item=nullptr;
        for(int row=0;row<favorites->count();++row) {
            const auto target=FolderLibrary::target_from_json(QJsonDocument::fromJson(
                favorites->item(row)->data(Qt::UserRole+1).toByteArray()).object());
            if(std::get_if<BuiltinEffectTypeRefV1>(&target))effect_item=favorites->item(row);
        }
        check(effect_item,"The mixed Favorite list retains its exact built-in Effect target");
        favorites->setCurrentItem(effect_item);
        const auto after_import_revision=window.host.session.revision();
        QTest::mouseClick(use_favorite,Qt::LeftButton);QApplication::processEvents();
        check(window.host.session.revision()==after_import_revision+1&&
            window.host.session.document().objects.at(object_id).stack.size()==target_before+2,
            "The same dialog refreshes its expected revision after Preset use before applying an Effect Favorite");

        slot->setCurrentIndex(slot->findData(1));
        const auto after_effect_revision=window.host.session.revision();
        const auto definitions_after_first=window.host.session.document().preset_definitions.size();
        QTest::mouseClick(use_slot,Qt::LeftButton);QApplication::processEvents();
        check(window.host.session.revision()==after_effect_revision+1&&
            window.host.session.document().preset_definitions.size()==definitions_after_first+1&&
            window.host.session.document().objects.at(object_id).stack.size()==target_before+3,
            "The same dialog retains exact context for Preset Quick Access after an Effect mutation");

        QSettings external_settings(settings_path,QSettings::IniFormat);
        FolderLibrary external(external_settings,{}, {},payload_root);
        auto external_definition=portable_preset("external-source","External Accepted Revision",40);
        LibraryPresetAssetV1 external_metadata;
        (void)external.read_preset_asset(asset_a.ref,&external_metadata);
        const auto externally_updated=external.update_preset_asset(asset_a.ref,external_definition,
            external_metadata.accepted_revision,external_metadata.sha256);
        check(externally_updated.accepted_revision==2,"A second writer advanced the selected asset after GUI refresh");

        source_presets->setCurrentIndex(source_index);
        QTest::mouseClick(update,Qt::LeftButton);QApplication::processEvents();
        check(window.statusBar()->currentMessage().contains("PRESET_ASSET_REVISION_CONFLICT")&&
            external.read_preset_asset(asset_a.ref).entries.front().parameters.at("amount")==40,
            "The widget refuses an update from the stale selected revision without replacing the newer payload");

        auto* refresh=dialog->findChild<QPushButton*>("folder-library-refresh");
        check(refresh,"Explicit Refresh is available for current Library metadata");
        QTest::mouseClick(refresh,Qt::LeftButton);QApplication::processEvents();
        check(assets->currentData().toString()==asset_a.ref.asset_id&&
            assets->currentData(Qt::UserRole+1).toULongLong()==2,
            "Explicit Refresh preserves the selected AssetID and accepts its current revision");
        QTest::mouseClick(update,Qt::LeftButton);QApplication::processEvents();
        LibraryPresetAssetV1 post_refresh_metadata;
        const auto refreshed_update=external.read_preset_asset(asset_a.ref,&post_refresh_metadata);
        check(refreshed_update.entries.front().parameters.at("amount")==18&&post_refresh_metadata.accepted_revision==3,
            "After explicit Refresh, Update succeeds against the accepted current revision");

        assets->setCurrentIndex(assets->findData(asset_a.ref.asset_id));
        QTimer::singleShot(0,&window,[&] {
            auto* confirmation=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            check(confirmation,"Delete asks about one exact selected workspace asset");
            assets->setCurrentIndex(assets->findData(asset_b.ref.asset_id));
            confirmation->done(QMessageBox::Yes);
        });
        QTest::mouseClick(remove,Qt::LeftButton);QApplication::processEvents();
        check(window.statusBar()->currentMessage().contains("PRESET_ASSET_SELECTION_CHANGED")&&
            QFileInfo::exists(QDir(payload_root).filePath(asset_a.ref.asset_id+".preset.json"))&&
            QFileInfo::exists(QDir(payload_root).filePath(asset_b.ref.asset_id+".preset.json")),
            "A reentrant asset-selector change during delete confirmation refuses without deleting either identity");
        dialog->accept();
    });
    auto* open_library=window.findChild<QAction*>("folder-library");check(open_library,"Portable Preset UI uses the normal Folder Library action");open_library->trigger();

    const auto final_stack_size=window.host.session.document().objects.at(object_id).stack.size();
    const auto final_preset_count=window.host.session.document().preset_definitions.size();
    window.host.session.undo(window.host.session.revision());window.host.edited();
    check(window.host.session.document().objects.at(object_id).stack.size()==final_stack_size-1&&
        window.host.session.document().preset_definitions.size()==final_preset_count-1,
        "One Undo removes the most recent Favorite import definition and its applied entry");
    window.host.session.redo(window.host.session.revision());window.host.edited();
    check(window.host.session.document().objects.at(object_id).stack.size()==final_stack_size&&
        window.host.session.document().preset_definitions.size()==final_preset_count,
        "One Redo restores the exact Favorite import and application");
    window.close();window.host.flush();
}

void unsafe_preset_root_keeps_legacy_library_usable(const QString& scratch) {
    const auto outside=scratch+"/outside";const auto unsafe_root=scratch+"/payload-junction";
    const auto source_root=scratch+"/legacy-assets";QDir().mkpath(outside);QDir().mkpath(source_root);
    if(!create_directory_escape_link(outside,unsafe_root))return;
    const auto image_path=source_root+"/legacy.png";write_bytes(image_path,png(20,40,80));
    const auto settings_path=scratch+"/legacy.ini";QSettings settings(settings_path,QSettings::IniFormat);
    auto seeded=std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},
        FolderLibrary::ReadbackOverride{},unsafe_root);
    const auto root=seeded->register_root(source_root,"Legacy assets");seeded->refresh();
    const auto favorite=seeded->add_favorite({root.root_id,"legacy.png","raster"},1);
    auto injected=std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},
        FolderLibrary::ReadbackOverride{},unsafe_root);
    Window window(scratch+"/recovery",std::move(injected));window.show();QApplication::processEvents();
    QTimer::singleShot(0,&window,[&] {
        auto* dialog=window.findChild<QDialog*>("folder-library-dialog");
        auto* tree=dialog?dialog->findChild<QTreeWidget*>("folder-library-tree"):nullptr;
        auto* assets=dialog?dialog->findChild<QComboBox*>("folder-library-preset-assets"):nullptr;
        auto* favorites=dialog?dialog->findChild<QListWidget*>("folder-library-favorites"):nullptr;
        auto* use=dialog?dialog->findChild<QPushButton*>("folder-library-use-slot"):nullptr;
        check(dialog&&tree&&assets&&favorites&&use,
            "A reparse-point Preset payload root does not prevent the existing Folder Library dialog from opening");
        check(assets->count()==1&&assets->currentText().contains("UNSAFE_PRESET_LIBRARY_ROOT"),
            "The unsafe exact payload root appears as an explicit unavailable Preset row without fallback");
        check(favorites->count()==1&&favorites->item(0)->data(Qt::UserRole).toString()==favorite.favorite_id&&
            favorites->item(0)->text().contains("Available"),
            "Legacy Asset Favorite remains visible and resolvable beside an unavailable Preset pane");
        auto* slot=dialog->findChild<QComboBox*>("folder-library-slot");slot->setCurrentIndex(slot->findData(1));
        QTest::mouseClick(use,Qt::LeftButton);QApplication::processEvents();
        check(window.host.session.document().objects.size()==1,
            "Legacy Favorite invocation remains usable while the Preset root is unsafe");
        dialog->accept();
    });
    window.findChild<QAction*>("folder-library")->trigger();
    window.close();window.host.flush();
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);
    try {
        if (argc == 5 && QString::fromUtf8(argv[1]) == "--verify-settings") {
            QSettings persisted(QString::fromUtf8(argv[2]), QSettings::IniFormat);
            FolderLibrary reloaded(persisted);
            const auto favorite = reloaded.favorite_for_slot(1);
            return favorite && favorite->favorite_id == QString::fromUtf8(argv[4]) &&
                std::get_if<LibraryItemRefV1>(&favorite->target) &&
                std::get<LibraryItemRefV1>(favorite->target).root_id == QString::fromUtf8(argv[3]) ? 0 : 1;
        }
        if (argc == 7 && QString::fromUtf8(argv[1]) == "--verify-mixed-settings") {
            QSettings persisted(QString::fromUtf8(argv[2]),QSettings::IniFormat);
            FolderLibrary reloaded(persisted);
            const auto find=[&](const QString& id) -> const LibraryFavoriteV1* {
                const auto item=std::find_if(reloaded.favorites().begin(),reloaded.favorites().end(),[&](const auto& value) {
                    return value.favorite_id==id;
                });
                return item==reloaded.favorites().end()?nullptr:&*item;
            };
            const auto* asset=find(QString::fromUtf8(argv[4]));
            const auto* folder=find(QString::fromUtf8(argv[5]));
            const auto* effect=find(QString::fromUtf8(argv[6]));
            if(!asset||!folder||!effect||!std::get_if<LibraryItemRefV1>(&asset->target)||
                !std::get_if<LibraryItemRefV1>(&folder->target)||!std::get_if<BuiltinEffectTypeRefV1>(&effect->target))return 1;
            const auto& asset_ref=std::get<LibraryItemRefV1>(asset->target);
            const auto& folder_ref=std::get<LibraryItemRefV1>(folder->target);
            const auto& effect_ref=std::get<BuiltinEffectTypeRefV1>(effect->target);
            return asset_ref.root_id==QString::fromUtf8(argv[3])&&asset_ref.normalized_relative_path=="brand/logo.png"&&
                folder_ref.root_id==asset_ref.root_id&&folder_ref.kind=="folder"&&
                effect->favorite_id==QString::fromUtf8(argv[6])&&effect_ref.type_id=="nect.shape.offset"&&
                effect_ref.behavior_version==1&&asset->quick_slot==1&&effect->quick_slot==2?0:1;
        }
        if(argc==7&&QString::fromUtf8(argv[1])=="--verify-portable-preset") {
            const auto fail=[](const char* message) {std::cerr<<"cold Preset reader: "<<message<<'\n';return 1;};
            QSettings persisted(QString::fromUtf8(argv[2]),QSettings::IniFormat);
            FolderLibrary reloaded(persisted,{}, {},QString::fromUtf8(argv[3]));
            const auto favorite=std::find_if(reloaded.favorites().begin(),reloaded.favorites().end(),[&](const auto& value) {
                return value.favorite_id==QString::fromUtf8(argv[4]);
            });
            if(favorite==reloaded.favorites().end()||favorite->quick_slot!=1||
                !std::get_if<PresetAssetRefV1>(&favorite->target)||
                std::get<PresetAssetRefV1>(favorite->target).asset_id!=QString::fromUtf8(argv[5]))return fail("shared Favorite did not retain its exact AssetID and slot");
            LibraryPresetAssetV1 metadata;
            const auto definition=reloaded.read_preset_asset(std::get<PresetAssetRefV1>(favorite->target),&metadata);
            if(definition.id!=QString::fromUtf8(argv[6]).toStdString()||definition.label!="Portable Offset")return fail("payload identity or label changed");
            auto document=empty_document("cold-session","cold-composition","cold-artboard");
            Session session(std::move(document));
            Primitive source=default_primitive("cold-source","nect.shape.rectangle");
            session.apply({Command{CreatePrimitive{"cold-composition","","cold-target","Cold target",source}}},session.revision());
            const auto before=encode(session.document());
            const auto prior_stack_size=session.document().objects.at("cold-target").stack.size();
            const auto accepted_revision=session.revision();
            session.apply_preset_command(PresetCommand{ImportAndApplyPreset{definition,"cold-imported-definition",
                "cold-target","cold-portable-use",QString::fromUtf8(argv[5]).toStdString(),metadata.accepted_revision}},accepted_revision);
            const auto& stack=session.document().objects.at("cold-target").stack;
            if(session.revision()!=accepted_revision+1||stack.size()!=prior_stack_size+1||
                stack.back().id!="cold-portable-use-op-1"||stack.back().type!="nect.shape.offset"||
                stack.back().parameters.at("amount").literal!=18||
                !session.document().preset_definitions.contains("cold-imported-definition"))return fail("fresh Session import did not apply amount 18");
            auto without_import=session.document();
            without_import.objects.at("cold-target").stack.pop_back();
            without_import.preset_definitions.erase("cold-imported-definition");
            if(encode(without_import)!=before)return fail("import changed authored entries outside its exact append");
            session.undo(session.revision());
            if(encode(session.document())!=before||session.document().preset_definitions.contains("cold-imported-definition"))return fail("Undo did not restore the cold Session exactly");
            session.redo(session.revision());
            return session.document().objects.at("cold-target").stack.back().id=="cold-portable-use-op-1"&&
                session.document().objects.at("cold-target").stack.back().parameters.at("amount").literal==18&&
                session.document().preset_definitions.contains("cold-imported-definition")?0:fail("Redo did not restore the cold import");
        }
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch directory is available");
        model_and_persistence(scratch.path()+"/model");
        grouped_settings_identity(scratch.path()+"/grouped");
        unified_effect_favorites(scratch.path()+"/unified");
        portable_preset_assets(scratch.path()+"/portable-presets");
        host_and_ui_placement(scratch.path()+"/placement");
        portable_preset_library_ui(scratch.path()+"/portable-preset-ui");
        unsafe_preset_root_keeps_legacy_library_usable(scratch.path()+"/unsafe-preset-root-ui");
        std::cout<<"PASS "<<checks<<" Folder Library/settings/Host/UI assertions\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
