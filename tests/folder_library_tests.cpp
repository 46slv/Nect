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
#include <QLineEdit>
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
#include <algorithm>
#include <cmath>
#include <limits>
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

Bounds shape_bounds(const EvaluatedShape& shape) {
    Bounds result{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()};
    for(const auto& instance:shape.paths)for(const auto& contour:*instance.contours)for(const auto& point:contour.points) {
        const auto mapped=map_point(instance.transform,point.anchor);
        result.left=std::min(result.left,mapped.x);result.top=std::min(result.top,mapped.y);
        result.right=std::max(result.right,mapped.x);result.bottom=std::max(result.bottom,mapped.y);
    }
    return result;
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

MacroDefinition portable_macro(const std::string& id,const std::string& label,double amount=12) {
    auto offset=default_operation("portable-macro-offset","nect.shape.offset");
    offset.parameters.at("amount").literal=amount;
    auto repeater=default_operation("portable-macro-repeater","nect.shape.repeater");
    repeater.parameters.at("copies").literal=2;
    repeater.parameters.at("position_x").literal=125;
    MacroDefinitionRevision graph;graph.revision=1;
    graph.input={"portable-macro-input","local_paths_and_paint"};
    graph.output={"portable-macro-output","local_paths_and_paint"};
    graph.nodes={{offset,"portable-offset-input","portable-offset-output"},
        {repeater,"portable-repeater-input","portable-repeater-output"}};
    graph.edges={{{"","portable-macro-input"},{"portable-macro-offset","portable-offset-input"}},
        {{"portable-macro-offset","portable-offset-output"},{"portable-macro-repeater","portable-repeater-input"}},
        {{"portable-macro-repeater","portable-repeater-output"},{"","portable-macro-output"}}};
    graph.output_mapping={"portable-macro-repeater","portable-repeater-output"};
    graph.public_parameters.push_back({"macro.offset.amount","Amount","portable-macro-offset","amount",
        "number","du","local_paths_and_paint"});
    MacroDefinition definition;definition.id=id;definition.label=label;definition.latest_revision=1;
    definition.revisions.emplace(1,std::move(graph));return definition;
}

MacroDefinition portable_macro_with_revisions(const std::string& id,const std::string& label) {
    auto definition=portable_macro(id,label,12);
    auto second=definition.revisions.at(1);second.revision=2;
    second.nodes.front().operation.parameters.at("amount").literal=18;
    definition.revisions.emplace(2,std::move(second));definition.latest_revision=2;return definition;
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

void portable_preset_closure_assets(const QString& scratch) {
    QDir().mkpath(scratch);
    const auto root=scratch+"/closure-assets";
    const auto settings_path=scratch+"/closure-library.ini";
    QSettings settings(settings_path,QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    FolderLibrary library(settings,{}, {},root);

    auto macro=portable_macro_with_revisions("closure-macro","Closure Macro");
    for(auto& [pin,graph]:macro.revisions)graph.graph_version=2;
    auto& latest=macro.revisions.at(2);latest.interface_version=3;
    latest.public_parameters.push_back({"macro.offset.enabled","Use Offset","portable-macro-offset","enabled",
        "boolean","boolean","local_paths_and_paint"});
    latest.nodes.front().operation.enabled=false;
    std::reverse(latest.nodes.begin(),latest.nodes.end());
    std::reverse(latest.edges.begin(),latest.edges.end());
    auto preset=portable_preset("closure-preset","Mixed closure");
    PresetEntry pinned;pinned.kind="macro";pinned.type=macro_entry_type;
    pinned.macro_definition=macro.id;pinned.pinned_revision=1;
    pinned.overrides={{"macro.offset.amount",14}};
    preset.entries.push_back(pinned);
    pinned.pinned_revision=2;pinned.enabled=false;
    pinned.overrides.at("macro.offset.amount")=27;
    pinned.boolean_overrides={{"macro.offset.enabled",true}};
    preset.entries.push_back(pinned);
    const PortablePresetClosure closure{preset,{{macro.id,macro}}};
    const auto canonical=canonical_preset_closure_payload(closure);
    const auto published=library.publish_preset(closure);
    const auto path=QDir(root).filePath(published.ref.asset_id+".preset.json");
    const auto favorite=library.add_favorite(published.ref,3);
    check(published.payload_schema==3&&published.accepted_revision==1&&
        library.favorite_status(favorite)=="Available"&&library.preset_assets().front().available&&
        library.preset_assets().front().payload_schema==3&&library.macro_assets().isEmpty(),
        "A mixed closure publishes as one available schema-3 Preset asset and creates no dependency assets");
    LibraryPresetAssetV1 refused_metadata;refused_metadata.label="unchanged sentinel";
    rejects("PRESET_DEPENDENCY_CLOSURE_REQUIRED",[&]{(void)library.read_preset_asset(published.ref,&refused_metadata);});
    check(refused_metadata.label=="unchanged sentinel","A dependency-dropping read refusal leaves its output metadata unchanged");

    QSettings fresh_settings(settings_path,QSettings::IniFormat);fresh_settings.setFallbacksEnabled(false);
    FolderLibrary fresh(fresh_settings,{}, {},root);
    LibraryPresetAssetV1 read_metadata;
    const auto snapshot=fresh.read_preset_closure_asset(published.ref,&read_metadata);
    check(snapshot==closure&&canonical_preset_closure_payload(snapshot)==canonical&&
        snapshot.macro_definitions.size()==1&&snapshot.macro_definitions.at(macro.id).revisions.size()==2&&
        snapshot.definition.entries.at(1).pinned_revision==1&&snapshot.definition.entries.at(2).pinned_revision==2&&
        snapshot.definition.entries.at(2).boolean_overrides.at("macro.offset.enabled")&&
        read_metadata.sha256==published.sha256&&fresh.favorite_for_slot(3)->favorite_id==favorite.favorite_id,
        "A separate Library reload preserves one repeated dependency, all revisions, exact pins and typed overrides");

    auto next=closure;next.definition.label="Updated mixed closure";
    next.macro_definitions.at(macro.id).revisions.at(1).nodes.front().operation.parameters.at("amount").literal=22;
    const auto updated=library.update_preset_asset(published.ref,next,published.accepted_revision,published.sha256);
    check(updated.ref==published.ref&&updated.accepted_revision==2&&updated.sha256!=published.sha256&&
        fresh.read_preset_closure_asset(published.ref)==next&&snapshot==closure&&
        canonical_preset_closure_payload(snapshot)==canonical&&
        fresh.favorite_for_slot(3)->favorite_id==favorite.favorite_id&&fresh.favorite_status(favorite)=="Available",
        "Closure Update keeps AssetID, FavoriteID and slot while a previously read snapshot stays unchanged");
    const auto bytes=read_bytes(path);
    const QByteArray accepted(reinterpret_cast<const char*>(bytes.data()),static_cast<qsizetype>(bytes.size()));
    fresh_settings.sync();const auto preferences=fresh_settings.value("library/v1/state").toByteArray();
    const auto unchanged=[&] {
        fresh_settings.sync();
        check(read_bytes(path)==bytes&&fresh_settings.value("library/v1/state").toByteArray()==preferences&&
            fresh.favorite_for_slot(3)->favorite_id==favorite.favorite_id,
            "A refused closure update preserves accepted bytes and shared Favorite preferences");
    };
    rejects("PRESET_ASSET_REVISION_CONFLICT",[&]{
        library.update_preset_asset(published.ref,closure,published.accepted_revision,published.sha256);
    });unchanged();
    rejects("PRESET_ASSET_REVISION_CONFLICT",[&]{
        library.update_preset_asset(published.ref,closure,updated.accepted_revision,published.sha256);
    });unchanged();

    auto rejects_candidate=[&](const PortablePresetClosure& candidate) {
        bool refused=false;
        try {library.update_preset_asset(published.ref,candidate,updated.accepted_revision,updated.sha256);}
        catch(const Error&) {refused=true;}
        check(refused,"Invalid or oversized dependency closure is refused before storage mutation");unchanged();
    };
    auto missing=next;missing.macro_definitions.clear();rejects_candidate(missing);
    auto colliding=next;auto collision_definition=colliding.macro_definitions.at(macro.id);
    collision_definition.id=published.ref.asset_id.toStdString();colliding.macro_definitions.clear();
    colliding.macro_definitions.emplace(collision_definition.id,collision_definition);
    for(auto& entry:colliding.definition.entries)if(entry.kind=="macro")entry.macro_definition=collision_definition.id;
    rejects("PRESET_ASSET_ID_MISMATCH",[&]{library.update_preset_asset(published.ref,colliding,
        updated.accepted_revision,updated.sha256);});unchanged();
    auto invalid=next;invalid.macro_definitions.at(macro.id).revisions.at(1).output_mapping.port="absent-output";
    rejects_candidate(invalid);
    auto oversized=next;
    auto& many_revisions=oversized.macro_definitions.at(macro.id);
    for(std::uint64_t pin=3;pin<=128;++pin) {
        auto retained=many_revisions.revisions.at(2);retained.revision=pin;
        many_revisions.revisions.emplace(pin,std::move(retained));
    }
    many_revisions.latest_revision=128;
    auto second_dependency=many_revisions;second_dependency.id="closure-second-macro";
    oversized.macro_definitions.emplace(second_dependency.id,second_dependency);
    auto second_reference=oversized.definition.entries.at(2);second_reference.macro_definition=second_dependency.id;
    oversized.definition.entries.push_back(std::move(second_reference));
    rejects("PRESET_PAYLOAD_LIMIT",[&]{library.update_preset_asset(published.ref,oversized,
        updated.accepted_revision,updated.sha256);});unchanged();

    const auto envelope=QJsonDocument::fromJson(accepted).object();
    const auto rejects_stored=[&](QJsonObject bad,const char* code) {
        const auto bad_bytes=QJsonDocument(bad).toJson(QJsonDocument::Compact);write_qbytes(path,bad_bytes);
        rejects(code,[&]{(void)fresh.read_preset_closure_asset(published.ref);});
        const auto listed=fresh.preset_assets();
        check(listed.size()==1&&!listed.front().available&&fresh.favorite_status(favorite)!="Available",
            "Malformed closure stays visible as unavailable under the original AssetID and Favorite");
        rejects(code,[&]{library.update_preset_asset(published.ref,closure,updated.accepted_revision,updated.sha256);});
        const auto observed=read_bytes(path);
        check(QByteArray(reinterpret_cast<const char*>(observed.data()),static_cast<qsizetype>(observed.size()))==bad_bytes,
            "Refused read/update never rewrites observed malformed asset bytes");
        fresh_settings.sync();check(fresh_settings.value("library/v1/state").toByteArray()==preferences,
            "Malformed closure refusal leaves persisted shared preferences unchanged");
        write_qbytes(path,accepted);
    };
    auto bad=envelope;bad.insert("sha256",QString(64,'0'));rejects_stored(bad,"PRESET_ASSET_HASH_MISMATCH");
    bad=envelope;bad.insert("payload_schema",99);rejects_stored(bad,"UNSUPPORTED_PRESET_SCHEMA");
    bad=envelope;bad.insert("kind","macro_definition");rejects_stored(bad,"UNSUPPORTED_PRESET_ASSET_VERSION");
    const auto with_payload=[&](const QByteArray& payload) {
        auto changed=envelope;changed.insert("payload",QString::fromUtf8(payload));
        changed.insert("sha256",QString::fromLatin1(QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex()));
        return changed;
    };
    const auto next_payload=QByteArray::fromStdString(canonical_preset_closure_payload(next));
    rejects_stored(with_payload(QByteArray::fromStdString(canonical_preset_closure_payload(colliding))),"PRESET_ASSET_ID_MISMATCH");
    rejects_stored(with_payload(" "+next_payload),"UNAVAILABLE_PRESET_ASSET");
    auto missing_payload=QJsonDocument::fromJson(next_payload).object();missing_payload.insert("macro_definitions",QJsonArray{});
    rejects_stored(with_payload(QJsonDocument(missing_payload).toJson(QJsonDocument::Compact)),"UNAVAILABLE_PRESET_ASSET");
    auto invalid_payload=QJsonDocument::fromJson(next_payload).object();auto dependencies=invalid_payload.value("macro_definitions").toArray();
    auto dependency=dependencies.first().toObject();dependency.insert("latest_revision",999);dependencies[0]=dependency;
    invalid_payload.insert("macro_definitions",dependencies);
    rejects_stored(with_payload(QJsonDocument(invalid_payload).toJson(QJsonDocument::Compact)),"UNAVAILABLE_PRESET_ASSET");
    rejects_stored(with_payload(QByteArray(static_cast<qsizetype>(portable_preset_payload_limit+1),'x')),"PRESET_PAYLOAD_LIMIT");
    unchanged();

    for(unsigned schema:{1u,2u}) {
        auto literal=portable_preset("legacy-closure-"+std::to_string(schema),"Legacy closure");literal.schema_version=schema;
        if(schema==1) {
            const auto operation=default_operation("legacy-repeater","nect.shape.repeater");
            PresetEntry entry;entry.type=operation.type;entry.version=operation.version;entry.enabled=operation.enabled;
            for(const auto& [name,value]:operation.parameters)entry.parameters.emplace(name,value.literal);
            entry.composite=operation.composite;entry.fill_rule=operation.fill_rule;
            entry.line_join=operation.line_join;entry.line_cap=operation.line_cap;
            literal.entries.push_back(std::move(entry));
        }
        const auto old_payload=canonical_preset_payload(literal);
        check(canonical_preset_closure_payload(PortablePresetClosure{literal,{}})==old_payload,
            "Empty closure preserves legacy schema-1/2 canonical payload bytes");
        const auto legacy=library.publish_preset(literal);
        const auto reloaded=fresh.read_preset_closure_asset(legacy.ref);
        check(legacy.payload_schema==schema&&reloaded.macro_definitions.empty()&&reloaded.definition==literal&&
            canonical_preset_payload(fresh.read_preset_asset(legacy.ref))==old_payload,
            "Legacy schema-1/2 publication remains readable through both Library APIs");
        const auto legacy_path=QDir(root).filePath(legacy.ref.asset_id+".preset.json");
        const auto legacy_raw=read_bytes(legacy_path);
        const QByteArray legacy_bytes(reinterpret_cast<const char*>(legacy_raw.data()),static_cast<qsizetype>(legacy_raw.size()));
        auto mismatched=QJsonDocument::fromJson(legacy_bytes).object();mismatched.insert("payload_schema",static_cast<int>(3-schema));
        write_qbytes(legacy_path,QJsonDocument(mismatched).toJson(QJsonDocument::Compact));
        rejects("PRESET_ASSET_ENVELOPE_MISMATCH",[&]{(void)fresh.read_preset_closure_asset(legacy.ref);});
        write_qbytes(legacy_path,legacy_bytes);
    }
}

void portable_macro_assets(const QString& scratch) {
    const auto root=scratch+"/payloads";const auto settings_path=scratch+"/macro-library.ini";
    QSettings settings(settings_path,QSettings::IniFormat);
    FolderLibrary library(settings,{}, {},root);
    check(library.macro_assets().isEmpty()&&!QFileInfo::exists(root),
        "Injected Macro payload listing stays read-only until explicit publication");

    auto source=portable_macro_with_revisions("portable-macro-source","Portable Offset Repeat");
    const auto published=library.publish_macro_asset(source);
    const auto good_source=portable_macro("portable-macro-good","Second Good Macro",31);
    const auto good=library.publish_macro_asset(good_source);
    const auto asset_path=QDir(root).filePath(published.ref.asset_id+".macro.json");
    check(QFileInfo::exists(asset_path)&&published.accepted_revision==1&&
        published.ref.asset_id!=QString::fromStdString(source.id)&&published.payload_schema==1&&published.sha256.size()==64,
        "Macro publication assigns a distinct workspace AssetID, accepted revision 1 and canonical payload hash");
    QSettings no_payload_store(settings_path,QSettings::IniFormat);no_payload_store.sync();
    check(!no_payload_store.contains("library/v1/state"),
        "Macro bytes are stored in the bounded asset directory rather than QSettings payload state");
    LibraryMacroAssetV1 metadata;
    const auto loaded=library.read_macro_asset(published.ref,&metadata);
    check(canonical_macro_payload(loaded)==canonical_macro_payload(source)&&loaded.revisions.size()==2&&
        loaded.revisions.at(1).nodes.front().operation.id==loaded.revisions.at(2).nodes.front().operation.id&&
        metadata.ref==published.ref&&metadata.accepted_revision==1&&metadata.sha256==published.sha256&&
        metadata.label==QString::fromStdString(source.label)&&metadata.payload_schema==1,
        "Canonical Macro asset readback preserves both graph revisions, graph IDs and metadata");
    const auto original_vector=read_bytes(asset_path);
    const auto original_bytes=QByteArray(reinterpret_cast<const char*>(original_vector.data()),
        static_cast<qsizetype>(original_vector.size()));
    auto envelope=QJsonDocument::fromJson(original_bytes).object();

    const auto macro_favorite=library.add_favorite(published.ref,1);
    const auto preset=library.publish_preset(portable_preset("macro-neighbor-preset","Macro Neighbor Preset"));
    const auto preset_favorite=library.add_favorite(preset.ref,2);
    const auto effect_favorite=library.add_favorite(BuiltinEffectTypeRefV1{"nect.shape.offset",1},3);
    QDir().mkpath(scratch+"/folder-root");
    const auto folder_root=library.register_root(scratch+"/folder-root","Scratch Folder");library.refresh();
    const auto folder_favorite=library.add_favorite(LibraryItemRefV1{folder_root.root_id,{},"folder"},4);
    const auto target_roundtrip=FolderLibrary::target_from_json(FolderLibrary::target_to_json(macro_favorite.target));
    check(std::get_if<MacroAssetRefV1>(&target_roundtrip)&&
        library.same_identity(macro_favorite.target,LibraryFavoriteTargetV1{published.ref})&&
        library.favorite_status(macro_favorite)=="Available"&&library.favorite_status(preset_favorite)=="Available"&&
        library.favorite_status(effect_favorite)=="Available"&&library.favorite_status(folder_favorite)=="Available"&&
        library.favorite_for_slot(1)->favorite_id==macro_favorite.favorite_id,
        "Macro shares exact Favorite JSON, stable identity and nine-slot authority with Preset, Effect and Folder targets");
    const auto prior_favorite_count=library.favorites().size();
    rejects("QUICK_SLOT_OCCUPIED",[&]{(void)library.add_favorite(good.ref,2);});
    check(library.favorites().size()==prior_favorite_count&&library.favorite_for_slot(1)->favorite_id==macro_favorite.favorite_id&&
        library.favorite_for_slot(2)->favorite_id==preset_favorite.favorite_id,
        "Macro-versus-Preset slot collision leaves both stable Favorite identities unchanged");
    QProcess cold_reader;
    cold_reader.start(QCoreApplication::applicationFilePath(),{"--verify-portable-macro",settings_path,root,
        macro_favorite.favorite_id,published.ref.asset_id,QString::fromStdString(source.id)});
    const auto cold_ok=cold_reader.waitForFinished(15000)&&cold_reader.exitStatus()==QProcess::NormalExit&&cold_reader.exitCode()==0;
    check(cold_ok,"A separate process reloads the shared Macro Favorite and imports all retained graphs: "+
        QString::fromUtf8(cold_reader.readAllStandardError()).toStdString());

    const auto original_payload=envelope.value("payload").toString();
    const auto set_hash=[&](QJsonObject& candidate,const QString& payload) {
        candidate.insert("payload",payload);
        candidate.insert("sha256",QString::fromLatin1(QCryptographicHash::hash(payload.toUtf8(),QCryptographicHash::Sha256).toHex()));
    };
    auto malformed=envelope;set_hash(malformed,"{}");
    const auto malformed_bytes=QJsonDocument(malformed).toJson(QJsonDocument::Compact);
    write_qbytes(asset_path,malformed_bytes);
    const auto malformed_inventory=library.macro_assets();
    const auto unavailable=std::find_if(malformed_inventory.begin(),malformed_inventory.end(),[&](const auto& value){return value.ref==published.ref;});
    const auto good_available=std::find_if(malformed_inventory.begin(),malformed_inventory.end(),[&](const auto& value){return value.ref==good.ref;});
    check(unavailable!=malformed_inventory.end()&&!unavailable->available&&
        unavailable->problem.contains("UNAVAILABLE_MACRO_ASSET")&&good_available!=malformed_inventory.end()&&good_available->available&&
        library.favorite_status(macro_favorite).contains("UNAVAILABLE_MACRO_ASSET")&&read_bytes(asset_path)==
            std::vector<unsigned char>(reinterpret_cast<const unsigned char*>(malformed_bytes.constData()),
                reinterpret_cast<const unsigned char*>(malformed_bytes.constData())+malformed_bytes.size()),
        "Hash-valid missing-key payload stays unavailable by exact identity while a good neighbor and Favorite status remain usable");
    auto wrong_kinds=envelope;set_hash(wrong_kinds,"{\"id\":\"portable-macro-source\",\"label\":\"Bad\",\"latest_revision\":\"2\",\"revisions\":[]}");
    const auto wrong_kind_bytes=QJsonDocument(wrong_kinds).toJson(QJsonDocument::Compact);
    write_qbytes(asset_path,wrong_kind_bytes);
    const auto wrong_kind_inventory=library.macro_assets();
    const auto wrong_kind=std::find_if(wrong_kind_inventory.begin(),wrong_kind_inventory.end(),[&](const auto& value){return value.ref==published.ref;});
    check(wrong_kind!=wrong_kind_inventory.end()&&!wrong_kind->available&&
        library.favorite_status(macro_favorite).contains("UNAVAILABLE_MACRO_ASSET")&&read_bytes(asset_path)==
            std::vector<unsigned char>(reinterpret_cast<const unsigned char*>(wrong_kind_bytes.constData()),
                reinterpret_cast<const unsigned char*>(wrong_kind_bytes.constData())+wrong_kind_bytes.size()),
        "Wrong-kind/hash-valid payload schema errors are normalized and do not rewrite the Favorite or file");

    auto future=envelope;future.insert("version",2);
    const auto future_bytes=QJsonDocument(future).toJson(QJsonDocument::Compact);
    write_qbytes(asset_path,future_bytes);
    const auto future_inventory=library.macro_assets();
    const auto future_item=std::find_if(future_inventory.begin(),future_inventory.end(),[&](const auto& value){return value.ref==published.ref;});
    check(future_item!=future_inventory.end()&&!future_item->available&&future_item->problem.contains("UNSUPPORTED_MACRO_ASSET_VERSION")&&
        read_bytes(asset_path)==std::vector<unsigned char>(reinterpret_cast<const unsigned char*>(future_bytes.constData()),
            reinterpret_cast<const unsigned char*>(future_bytes.constData())+future_bytes.size()),
        "Future Macro asset versions remain unavailable with exact bytes preserved");

    auto unknown_node=envelope;auto unknown_payload=original_payload;
    const auto node_type=unknown_payload.indexOf("nect.shape.offset");
    check(node_type>=0,"Canonical Macro payload includes the supported Offset node type");
    unknown_payload.replace(node_type,static_cast<qsizetype>(std::string("nect.shape.offset").size()),"nect.shape.unknown");
    set_hash(unknown_node,unknown_payload);const auto unknown_bytes=QJsonDocument(unknown_node).toJson(QJsonDocument::Compact);
    write_qbytes(asset_path,unknown_bytes);
    const auto unknown_inventory=library.macro_assets();
    const auto unknown_item=std::find_if(unknown_inventory.begin(),unknown_inventory.end(),[&](const auto& value){return value.ref==published.ref;});
    check(unknown_item!=unknown_inventory.end()&&!unknown_item->available&&library.favorite_status(macro_favorite)!="Available"&&
        read_bytes(asset_path)==std::vector<unsigned char>(reinterpret_cast<const unsigned char*>(unknown_bytes.constData()),
            reinterpret_cast<const unsigned char*>(unknown_bytes.constData())+unknown_bytes.size()),
        "Unknown executable graph nodes stay unavailable under the same AssetID without altering file bytes");

    auto oversized=envelope;const QString oversized_payload(256*1024+1,QLatin1Char('x'));set_hash(oversized,oversized_payload);
    const auto oversized_bytes=QJsonDocument(oversized).toJson(QJsonDocument::Compact);
    write_qbytes(asset_path,oversized_bytes);
    rejects("MACRO_PAYLOAD_LIMIT",[&]{(void)library.read_macro_asset(published.ref);});
    check(read_bytes(asset_path)==std::vector<unsigned char>(reinterpret_cast<const unsigned char*>(oversized_bytes.constData()),
        reinterpret_cast<const unsigned char*>(oversized_bytes.constData())+oversized_bytes.size()),
        "Oversized Macro payload refusal preserves its exact bytes");

    const auto alias=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto alias_path=QDir(root).filePath(alias+".macro.json");write_qbytes(alias_path,original_bytes);
    rejects("MACRO_ASSET_ID_MISMATCH",[&]{(void)library.read_macro_asset({alias});});
    check(QFile::remove(alias_path),"Remove exact owned wrong-filename Macro fixture");
    write_qbytes(asset_path,original_bytes);

    FolderLibrary missing_reader(settings,{}, {},root);
    rejects("MISSING_MACRO_ASSET",[&]{(void)missing_reader.read_macro_asset({QUuid::createUuid().toString(QUuid::WithoutBraces)});});
    const auto before_update=read_bytes(asset_path);
    auto updated_source=source;
    auto third=updated_source.revisions.at(2);third.revision=3;
    third.nodes.front().operation.parameters.at("amount").literal=24;
    updated_source.revisions.emplace(3,std::move(third));updated_source.latest_revision=3;
    updated_source.label="Portable Offset Repeat Updated";
    auto live_document=empty_document("macro-asset-live-session","macro-asset-live-composition","macro-asset-live-artboard");
    Session live(std::move(live_document));
    const auto live_source=default_primitive("macro-live-source","nect.shape.rectangle");
    live.apply({Command{CreatePrimitive{"macro-asset-live-composition","","macro-live-target","Macro live target",live_source}}},live.revision());
    InstantiateMacro existing_import{"macro-live-target","macro-live-imported-definition","macro-live-instance",1,0};
    existing_import.imported_definition=loaded;existing_import.asset_id=published.ref.asset_id.toStdString();
    existing_import.accepted_asset_revision=metadata.accepted_revision;
    live.apply({StructuralCommand{MacroCommand{std::move(existing_import)}}},live.revision());
    const auto existing_import_snapshot=encode(live.document());
    FolderLibrary fail_write(settings,{}, {},root,[](const QString&,const QByteArray&,QString& error) {
        error="injected Macro payload write failure";return false;
    });
    rejects("MACRO_LIBRARY_WRITE_FAILED",[&]{(void)fail_write.update_macro_asset(published.ref,updated_source,1,published.sha256);});
    check(read_bytes(asset_path)==before_update,"Failed first Macro asset write preserves original bytes and accepted revision");
    const QByteArray external_replacement("uncooperative Macro replacement");
    FolderLibrary readback_race(settings,{}, {},root,[&](const QString& path,const QByteArray&,QString&) {
        write_qbytes(path,external_replacement);return true;
    });
    rejects("MACRO_LIBRARY_ROLLBACK_FAILED",[&]{(void)readback_race.update_macro_asset(published.ref,updated_source,1,published.sha256);});
    const auto after_race=read_bytes(asset_path);
    check(QByteArray(reinterpret_cast<const char*>(after_race.data()),static_cast<qsizetype>(after_race.size()))==external_replacement,
        "Ambiguous Macro readback preserves different observed bytes instead of overwriting them during rollback");
    write_qbytes(asset_path,QByteArray(reinterpret_cast<const char*>(before_update.data()),
        static_cast<qsizetype>(before_update.size())));

    auto edited_source=source;edited_source.label="Edited document only";
    edited_source.revisions.at(1).nodes.front().operation.parameters.at("amount").literal=7;
    check(canonical_macro_payload(library.read_macro_asset(published.ref))==canonical_macro_payload(source),
        "Editing a source Document Macro after publication does not alter stored Library bytes");
    const auto updated=library.update_macro_asset(published.ref,updated_source,1,published.sha256);
    LibraryMacroAssetV1 updated_metadata;
    const auto updated_definition=library.read_macro_asset(published.ref,&updated_metadata);
    check(updated.ref==published.ref&&updated.accepted_revision==2&&updated_definition.latest_revision==3&&
        updated_definition.revisions.size()==3&&updated_metadata.sha256==updated.sha256&&
        library.favorite_for_slot(1)->favorite_id==macro_favorite.favorite_id&&library.favorite_status(macro_favorite)=="Available",
        "Explicit update advances AssetID accepted revision while preserving Favorite slot and all fresh graph revisions");
    check(encode(live.document())==existing_import_snapshot&&
        macro_parameter_value(live.document(),"macro-live-target","macro-live-instance","macro.offset.amount")==12&&
        live.document().macro_definitions.at("macro-live-imported-definition").revisions.size()==2,
        "Asset update leaves a previously imported definition, pin 1 and Amount 12 unchanged");
    auto future_document=empty_document("macro-asset-future-session","macro-asset-future-composition","macro-asset-future-artboard");
    Session future_session(std::move(future_document));
    future_session.apply({Command{CreatePrimitive{"macro-asset-future-composition","","macro-future-target","Macro future target",live_source}}},future_session.revision());
    InstantiateMacro future_import{"macro-future-target","macro-future-definition","macro-future-instance",3,0};
    future_import.imported_definition=updated_definition;future_import.asset_id=published.ref.asset_id.toStdString();
    future_import.accepted_asset_revision=updated_metadata.accepted_revision;
    future_session.apply({StructuralCommand{MacroCommand{std::move(future_import)}}},future_session.revision());
    check(macro_parameter_value(future_session.document(),"macro-future-target","macro-future-instance","macro.offset.amount")==24&&
        future_session.document().objects.at("macro-future-target").stack.front().macro->pinned_revision==3,
        "A later fresh import can explicitly pin graph revision 3 and read Amount 24 from accepted asset revision 2");
    rejects("MACRO_ASSET_REVISION_CONFLICT",[&]{(void)library.update_macro_asset(published.ref,source,1,published.sha256);});
    rejects("MACRO_ASSET_REVISION_CONFLICT",[&]{library.delete_macro_asset(published.ref,1,published.sha256);});
    FolderLibrary restarted(settings,{}, {},root);
    check(canonical_macro_payload(restarted.read_macro_asset(published.ref))==canonical_macro_payload(updated_source)&&
        restarted.favorite_for_slot(1)->favorite_id==macro_favorite.favorite_id,
        "Fresh FolderLibrary reader observes accepted graph revision 3 without altering imported snapshots");

    FolderLibrary preference_failure(settings,[](const QByteArray&,QString& error) {
        error="injected Macro Favorite persistence failure";return false;
    },{},root);
    rejects("SETTINGS_WRITE_FAILED",[&]{(void)preference_failure.add_favorite(good.ref,5);});
    check(restarted.favorite_for_slot(5)==std::nullopt&&
        canonical_macro_payload(restarted.read_macro_asset(good.ref))==canonical_macro_payload(good_source),
        "Failed Macro Favorite persistence leaves payload files and prior slots unchanged");
    const auto outside=scratch+"/outside";const auto escaped_root=scratch+"/macro-root-link";QDir().mkpath(outside);
    if(create_directory_escape_link(outside,escaped_root)) {
        FolderLibrary escaped(settings,{}, {},escaped_root);
        rejects("UNSAFE_MACRO_LIBRARY_ROOT",[&]{(void)escaped.read_macro_asset(published.ref);});
    }
    library.delete_macro_asset(published.ref,updated.accepted_revision,updated.sha256);
    const auto after_delete=library.macro_assets();
    check(std::none_of(after_delete.begin(),after_delete.end(),[&](const auto& value){return value.ref==published.ref;})&&
        std::any_of(after_delete.begin(),after_delete.end(),[&](const auto& value){return value.ref==good.ref&&value.available;})&&
        library.favorite_for_slot(1)->favorite_id==macro_favorite.favorite_id&&
        library.favorite_status(macro_favorite).contains("MISSING_MACRO_ASSET"),
        "Deleting one exact Macro asset retains the valid neighbor and leaves its Favorite as a broken stable reference");

    const auto cap_root=scratch+"/combined-cap";QDir().mkpath(cap_root);
    for(int index=0;index<128;++index) {
        write_qbytes(QDir(cap_root).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)+".preset.json"),QByteArray("{}"));
        write_qbytes(QDir(cap_root).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)+".macro.json"),QByteArray("{}"));
    }
    QSettings cap_settings(scratch+"/cap.ini",QSettings::IniFormat);
    FolderLibrary capped(cap_settings,{}, {},cap_root);
    check(capped.preset_assets().size()==128&&capped.macro_assets().size()==128,
        "Shared immediate enumeration is bounded across a mixed 256-file Preset/Macro store");
    rejects("MACRO_LIBRARY_LIMIT",[&]{(void)capped.publish_macro_asset(good_source);});
    rejects("PRESET_LIBRARY_LIMIT",[&]{(void)capped.publish_preset(portable_preset("cap-source","At cap"));});
    const auto extra_path=QDir(cap_root).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)+".macro.json");
    write_qbytes(extra_path,QByteArray("{}"));
    rejects("MACRO_LIBRARY_LIMIT",[&]{(void)capped.macro_assets();});
    rejects("PRESET_LIBRARY_LIMIT",[&]{(void)capped.preset_assets();});
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
            auto* yes=confirmation->button(QMessageBox::Yes);check(yes,"Preset Delete exposes its explicit Yes action");
            QTest::mouseClick(yes,Qt::LeftButton);
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

void portable_macro_library_ui(const QString& scratch) {
    const auto settings_path=scratch+"/macro-ui.ini";const auto payload_root=scratch+"/macro-assets";
    QSettings settings(settings_path,QSettings::IniFormat);
    FolderLibrary seeded(settings,{}, {},payload_root);
    const auto asset_a=seeded.publish_macro_asset(portable_macro_with_revisions("ui-asset-a","A UI Macro"));
    auto macro_b_source=portable_macro_with_revisions("ui-asset-b","B UI Macro");
    auto macro_b_third=macro_b_source.revisions.at(2);macro_b_third.revision=3;
    macro_b_third.nodes.front().operation.parameters.at("amount").literal=24;
    macro_b_source.revisions.emplace(3,std::move(macro_b_third));macro_b_source.latest_revision=3;
    const auto asset_b=seeded.publish_macro_asset(macro_b_source);
    const auto preset=seeded.publish_preset(portable_preset("ui-macro-neighbor-preset","UI Neighbor Preset"));
    auto injected=std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},
        FolderLibrary::ReadbackOverride{},payload_root);
    Window window(scratch+"/recovery",std::move(injected));window.show();QApplication::processEvents();

    const auto document_source=portable_macro_with_revisions("ui-document-source","UI Document Macro");
    window.host.session.apply({StructuralCommand{MacroCommand{CreateMacroDefinition{document_source}}}},window.host.session.revision());
    const auto composition=window.host.session.document().compositions.front().id;
    const auto object_id=new_id();
    const auto primitive=default_primitive(new_id(),"nect.shape.rectangle");
    window.host.session.apply({Command{CreatePrimitive{composition,"",object_id,"Portable Macro target",primitive}}},
        window.host.session.revision());
    const auto initial_target_stack_size=window.host.session.document().objects.at(object_id).stack.size();
    window.canvas->set_selection(object_id);window.host.edited();QApplication::processEvents();

    QString imported_definition_id;QString imported_instance_id;QString macro_favorite_id;std::uint64_t imported_revision=0;
    QString macro_ui_callback_error;
    Document before_import_document;
    QTimer::singleShot(0,&window,[&] {
        try {
        auto* dialog=window.findChild<QDialog*>("folder-library-dialog");check(dialog,"Folder Library opens with Workspace Macro controls");
        auto* assets=dialog->findChild<QComboBox*>("folder-library-macro-assets");
        auto* source_macros=dialog->findChild<QComboBox*>("folder-library-source-macros");
        auto* pin=dialog->findChild<QComboBox*>("folder-library-macro-pin");
        auto* preset_assets=dialog->findChild<QComboBox*>("folder-library-preset-assets");
        auto* publish=dialog->findChild<QPushButton*>("folder-library-macro-publish");
        auto* update=dialog->findChild<QPushButton*>("folder-library-macro-update");
        auto* remove=dialog->findChild<QPushButton*>("folder-library-macro-delete");
        auto* favorite_macro=dialog->findChild<QPushButton*>("folder-library-macro-favorite");
        auto* apply_macro=dialog->findChild<QPushButton*>("folder-library-macro-apply");
        auto* add_favorite=dialog->findChild<QPushButton*>("folder-library-favorite-add");
        auto* override_amount=dialog->findChild<QLineEdit*>("folder-library-macro-override-value");
        auto* favorites=dialog->findChild<QListWidget*>("folder-library-favorites");
        auto* slot=dialog->findChild<QComboBox*>("folder-library-slot");
        auto* assign=dialog->findChild<QPushButton*>("folder-library-slot-set");
        auto* use_favorite=dialog->findChild<QPushButton*>("folder-library-use-favorite");
        auto* use_slot=dialog->findChild<QPushButton*>("folder-library-use-slot");
        auto* refresh=dialog->findChild<QPushButton*>("folder-library-refresh");
        check(assets&&source_macros&&pin&&preset_assets&&publish&&update&&remove&&favorite_macro&&apply_macro&&
            add_favorite&&override_amount&&favorites&&slot&&assign&&use_favorite&&use_slot&&refresh,
            "Macro source, bounded asset, retained pin, Favorite, slot and guarded apply actions are present");
        const auto source_index=source_macros->findData(QString::fromStdString(document_source.id));
        check(source_index>0,"The current Document Macro is an explicit publication/update source");
        source_macros->setCurrentIndex(source_index);
        const auto before_publish=encode(window.host.session.document());const auto before_publish_revision=window.host.session.revision();
        QTest::mouseClick(publish,Qt::LeftButton);QApplication::processEvents();
        check(encode(window.host.session.document())==before_publish&&window.host.session.revision()==before_publish_revision&&
            seeded.macro_assets().size()==3,
            "Publish Macro creates an independent asset without changing the Document or Session revision");

        const auto asset_a_index=assets->findData(asset_a.ref.asset_id);const auto asset_b_index=assets->findData(asset_b.ref.asset_id);
        check(asset_a_index>0&&asset_b_index>0,"Both exact Workspace Macro AssetIDs appear in the selector");
        assets->setCurrentIndex(asset_a_index);
        const auto pin1=pin->findData(QVariant::fromValue<qulonglong>(1));
        check(pin1>0&&pin->currentData().toULongLong()==2,
            "Selecting a Macro defaults to its latest retained pin while keeping revision 1 explicitly selectable");
        pin->setCurrentIndex(pin1);
        const auto before_favorites=encode(window.host.session.document());const auto before_favorite_revision=window.host.session.revision();
        QTest::mouseClick(favorite_macro,Qt::LeftButton);
        check(favorites->count()==1&&encode(window.host.session.document())==before_favorites&&
            window.host.session.revision()==before_favorite_revision,
            "Dedicated Favorite Macro stores its exact identity without changing Session state");
        const auto find_macro_favorite=[&](const QString& asset_id) -> QListWidgetItem* {
            for(int row=0;row<favorites->count();++row) {
                const auto target=FolderLibrary::target_from_json(QJsonDocument::fromJson(
                    favorites->item(row)->data(Qt::UserRole+1).toByteArray()).object());
                if(const auto* macro=std::get_if<MacroAssetRefV1>(&target);macro&&macro->asset_id==asset_id)return favorites->item(row);
            }
            return nullptr;
        };
        auto* favorite_a=find_macro_favorite(asset_a.ref.asset_id);check(favorite_a,"Favorite Macro persists exact AssetID A");
        macro_favorite_id=favorite_a->data(Qt::UserRole).toString();

        assets->setCurrentIndex(assets->findData(asset_b.ref.asset_id));
        check(pin->currentData().toULongLong()==3,
            "Selecting a different AssetID defaults to that asset's accepted latest pin instead of reusing pin 1");
        QTest::mouseClick(favorite_macro,Qt::LeftButton);
        check(favorites->count()==2&&find_macro_favorite(asset_b.ref.asset_id),
            "Dedicated Favorite Macro addresses the currently selected second Macro AssetID");
        preset_assets->setCurrentIndex(preset_assets->findData(preset.ref.asset_id));
        QTest::mouseClick(add_favorite,Qt::LeftButton);
        QListWidgetItem* preset_favorite_item=nullptr;
        for(int row=0;row<favorites->count();++row) {
            const auto target=FolderLibrary::target_from_json(QJsonDocument::fromJson(
                favorites->item(row)->data(Qt::UserRole+1).toByteArray()).object());
            if(const auto* ref=std::get_if<PresetAssetRefV1>(&target);ref&&ref->asset_id==preset.ref.asset_id)
                preset_favorite_item=favorites->item(row);
        }
        check(favorites->count()==3&&preset_favorite_item&&find_macro_favorite(asset_a.ref.asset_id)&&
            find_macro_favorite(asset_b.ref.asset_id),
            "With Macro and Preset selectors simultaneously populated, existing Add Favorite still records the selected Preset");
        favorites->setCurrentItem(preset_favorite_item);
        const auto selected_target=FolderLibrary::target_from_json(QJsonDocument::fromJson(
            favorites->currentItem()->data(Qt::UserRole+1).toByteArray()).object());
        check(std::get_if<PresetAssetRefV1>(&selected_target),
            "The exact newly added Preset Favorite remains selectable alongside both Macro Favorites");

        favorite_a=find_macro_favorite(asset_a.ref.asset_id);favorites->setCurrentItem(favorite_a);
        slot->setCurrentIndex(slot->findData(1));QTest::mouseClick(assign,Qt::LeftButton);
        check(favorites->currentItem()->data(Qt::UserRole).toString()==macro_favorite_id&&
            slot->currentData().toInt()==1,
            "Workspace Macro Favorite receives its exact shared Quick Access slot");

        assets->setCurrentIndex(assets->findData(asset_b.ref.asset_id));
        override_amount->setFocus();QTest::keyClicks(override_amount,"39");
        const auto before_wrong_favorite=window.host.session.document();const auto revision_wrong_favorite=window.host.session.revision();
        favorites->setCurrentItem(find_macro_favorite(asset_a.ref.asset_id));
        QTest::mouseClick(use_favorite,Qt::LeftButton);QApplication::processEvents();
        check(window.statusBar()->currentMessage().contains("MACRO_OVERRIDE_TARGET_MISMATCH")&&
            window.host.session.document()==before_wrong_favorite&&window.host.session.revision()==revision_wrong_favorite,
            "An override draft for Macro B cannot silently apply when Favorite A is invoked");

        assets->setCurrentIndex(assets->findData(asset_a.ref.asset_id));
        check(pin->currentData().toULongLong()==2,
            "Switching back to Macro A defaults to its accepted latest pin rather than retaining B's pin 3");
        pin->setCurrentIndex(pin->findData(QVariant::fromValue<qulonglong>(1)));
        override_amount->setFocus();QTest::keyClicks(override_amount,"17");
        slot->setCurrentIndex(slot->findData(1));
        const auto before_import_revision=window.host.session.revision();
        const auto before_import_history=window.host.session.history().states.size();
        before_import_document=window.host.session.document();
        QTest::mouseClick(use_slot,Qt::LeftButton);QApplication::processEvents();
        const auto& imported_stack=window.host.session.document().objects.at(object_id).stack;
        check(window.host.session.revision()==before_import_revision+1&&
            window.host.session.history().states.size()==before_import_history+1&&
            imported_stack.size()==initial_target_stack_size+1&&
            imported_stack.back().macro&&imported_stack.back().macro->pinned_revision==1&&
            imported_stack.back().macro->overrides.at("macro.offset.amount")==17&&
            window.host.session.document().macro_definitions.at(imported_stack.back().macro->definition).revisions.size()==2,
            "Quick Access imports a fresh full Macro snapshot at explicit pin 1 with its stable Amount override in one Undo");
        imported_definition_id=QString::fromStdString(imported_stack.back().macro->definition);
        imported_instance_id=QString::fromStdString(imported_stack.back().id);
        imported_revision=window.host.session.revision();

        const auto native_macro_path=scratch+"/macro-import.nect";
        const auto native_macro_bytes=encode(window.host.session.document());
        Host macro_writer(scratch+"/macro-writer-recovery");
        macro_writer.session=Session(window.host.session.document());
        macro_writer.save(native_macro_path);
        Host macro_cold_reopen(scratch+"/macro-cold-reopen-recovery");
        macro_cold_reopen.open(native_macro_path);
        const auto& reopened_document=macro_cold_reopen.session.document();
        const auto& reopened_stack=reopened_document.objects.at(object_id).stack;
        const auto reopened_values=evaluate(reopened_document);
        const auto reopened_output=shape_bounds(evaluate_shape(reopened_document,object_id,reopened_values));
        check(encode(reopened_document)==native_macro_bytes,
            "A fresh Host reopen preserves exact native bytes for the imported Macro Document");
        check(reopened_stack.size()==initial_target_stack_size+1&&reopened_stack.back().macro&&
            reopened_stack.back().id==imported_instance_id.toStdString()&&
            reopened_stack.back().macro->definition==imported_definition_id.toStdString()&&
            reopened_stack.back().macro->pinned_revision==1&&
            reopened_stack.back().macro->overrides.at("macro.offset.amount")==17&&
            reopened_document.macro_definitions.at(imported_definition_id.toStdString()).revisions.size()==2&&
            macro_parameter_value(reopened_document,object_id,reopened_stack.back().id,"macro.offset.amount")==17,
            "A fresh Host reopen preserves the imported Macro graph, retained revisions, pin, instance identity and Amount override");
        check(
            std::abs(reopened_output.left+127)<1e-7&&std::abs(reopened_output.top+87)<1e-7&&
            std::abs(reopened_output.right-252)<1e-7&&std::abs(reopened_output.bottom-87)<1e-7,
            "A fresh Host reopen evaluates centered 220x140 geometry with Offset 17 and two-copy Repeater to fixed bounds [-127,-87,252,87]");
        macro_writer.flush();macro_cold_reopen.flush();

        LibraryMacroAssetV1 updated_metadata;
        auto store_reader=FolderLibrary(settings,{}, {},payload_root);
        const auto updated_from_ui=store_reader.read_macro_asset(asset_a.ref,&updated_metadata);
        check(updated_metadata.accepted_revision==1&&updated_from_ui.latest_revision==2,
            "Favorite application reads the captured accepted metadata snapshot and selected graph pin");
        const auto document_before_update=encode(window.host.session.document());const auto revision_before_update=window.host.session.revision();
        QTest::mouseClick(update,Qt::LeftButton);QApplication::processEvents();
        store_reader.read_macro_asset(asset_a.ref,&updated_metadata);
        check(updated_metadata.accepted_revision==2&&encode(window.host.session.document())==document_before_update&&
            window.host.session.revision()==revision_before_update&&
            window.host.session.document().macro_definitions.at(imported_definition_id.toStdString()).revisions.size()==2&&
            macro_parameter_value(window.host.session.document(),object_id,imported_stack.back().id,"macro.offset.amount")==17,
            "Updating the workspace asset leaves its already imported graph, pin and override snapshot unchanged");

        FolderLibrary external(settings,{}, {},payload_root);
        LibraryMacroAssetV1 external_metadata;(void)external.read_macro_asset(asset_a.ref,&external_metadata);
        const auto external_source=portable_macro("external-macro-source","External accepted Macro",40);
        const auto externally_updated=external.update_macro_asset(asset_a.ref,external_source,
            external_metadata.accepted_revision,external_metadata.sha256);
        check(externally_updated.accepted_revision==3,"A second writer advanced the exact AssetID after the UI snapshot");
        QTest::mouseClick(update,Qt::LeftButton);QApplication::processEvents();
        check(window.statusBar()->currentMessage().contains("MACRO_ASSET_REVISION_CONFLICT")&&
            external.read_macro_asset(asset_a.ref).revisions.at(1).nodes.front().operation.parameters.at("amount").literal==40,
            "Macro Update refuses changed bytes against stale captured accepted revision/hash");
        QTest::mouseClick(refresh,Qt::LeftButton);QApplication::processEvents();
        check(assets->currentData().toString()==asset_a.ref.asset_id&&assets->currentData(Qt::UserRole+1).toULongLong()==3&&
            pin->currentData().toULongLong()==1,
            "Explicit Refresh accepts the same AssetID metadata and retains an explicit pin still supported by that payload");
        QTest::mouseClick(update,Qt::LeftButton);QApplication::processEvents();
        LibraryMacroAssetV1 refreshed_metadata;external.read_macro_asset(asset_a.ref,&refreshed_metadata);
        check(refreshed_metadata.accepted_revision==4&&
            external.read_macro_asset(asset_a.ref).id==document_source.id,
            "After explicit Refresh, Macro Update succeeds from the selected Document source");

        assets->setCurrentIndex(assets->findData(asset_a.ref.asset_id));
        QTimer::singleShot(0,&window,[&] {
            auto* confirmation=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            check(confirmation,"Macro Delete captures an exact selected asset and asks for explicit confirmation");
            assets->setCurrentIndex(assets->findData(asset_b.ref.asset_id));
            auto* yes=confirmation->button(QMessageBox::Yes);check(yes,"Macro Delete exposes its explicit Yes action");
            QTest::mouseClick(yes,Qt::LeftButton);
        });
        QTest::mouseClick(remove,Qt::LeftButton);QApplication::processEvents();
        check(window.statusBar()->currentMessage().contains("MACRO_ASSET_SELECTION_CHANGED")&&
            QFileInfo::exists(QDir(payload_root).filePath(asset_a.ref.asset_id+".macro.json"))&&
            QFileInfo::exists(QDir(payload_root).filePath(asset_b.ref.asset_id+".macro.json")),
            "Reentrant Macro selector changes during confirmation refuse deletion of either AssetID");
        assets->setCurrentIndex(assets->findData(asset_a.ref.asset_id));
        QTimer::singleShot(0,&window,[&] {
            auto* confirmation=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            check(confirmation,"Second Macro Delete confirms the reselected exact AssetID");
            auto* yes=confirmation->button(QMessageBox::Yes);check(yes,"Second Macro Delete exposes its explicit Yes action");
            QTest::mouseClick(yes,Qt::LeftButton);
        });
        const auto before_delete_revision=window.host.session.revision();const auto before_delete_history=window.host.session.history();
        QTest::mouseClick(remove,Qt::LeftButton);QApplication::processEvents();
        check(!QFileInfo::exists(QDir(payload_root).filePath(asset_a.ref.asset_id+".macro.json"))&&
            window.host.session.revision()==before_delete_revision&&window.host.session.history()==before_delete_history&&
            favorites->count()==3&&find_macro_favorite(asset_a.ref.asset_id)->data(Qt::UserRole).toString()==macro_favorite_id,
            "Confirmed Macro deletion changes only workspace bytes and retains the exact Favorite identity as unavailable");
        dialog->accept();
        } catch(const std::exception& error) {
            macro_ui_callback_error=QString::fromUtf8(error.what());
            if(auto* modal=QApplication::activeModalWidget())modal->close();
            if(auto* active_dialog=window.findChild<QDialog*>("folder-library-dialog"))active_dialog->reject();
        }
    });
    auto* open_library=window.findChild<QAction*>("folder-library");check(open_library,"Macro Library UI uses the existing Folder Library action");open_library->trigger();
    if(!macro_ui_callback_error.isEmpty())
        throw std::runtime_error("Macro Library UI callback: "+macro_ui_callback_error.toStdString());
    const auto& final_stack=window.host.session.document().objects.at(object_id).stack;
    check(final_stack.size()==initial_target_stack_size+1&&final_stack.back().macro&&
        final_stack.back().id==imported_instance_id.toStdString()&&
        final_stack.back().macro->definition==imported_definition_id.toStdString()&&
        window.host.session.revision()==imported_revision,
        "Asset update/delete and shared Favorite edits do not change the already applied Session snapshot");
    window.host.session.undo(window.host.session.revision());window.host.edited();
    check(window.host.session.document()==before_import_document&&
        !window.host.session.document().macro_definitions.contains(imported_definition_id.toStdString()),
        "One Undo removes the Macro import and fresh Document DefinitionID while restoring prior authored stack entries");
    window.host.session.redo(window.host.session.revision());window.host.edited();
    check(window.host.session.document().objects.at(object_id).stack.size()==initial_target_stack_size+1&&
        window.host.session.document().objects.at(object_id).stack.back().id==imported_instance_id.toStdString()&&
        window.host.session.document().objects.at(object_id).stack.back().macro&&
        window.host.session.document().macro_definitions.contains(imported_definition_id.toStdString()),
        "Redo restores the same imported Macro graph, pin and instance");
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
        if(argc==7&&QString::fromUtf8(argv[1])=="--verify-portable-macro") {
            const auto fail=[](const char* message) {std::cerr<<"cold Macro reader: "<<message<<'\n';return 1;};
            QSettings persisted(QString::fromUtf8(argv[2]),QSettings::IniFormat);
            FolderLibrary reloaded(persisted,{}, {},QString::fromUtf8(argv[3]));
            const auto favorite=std::find_if(reloaded.favorites().begin(),reloaded.favorites().end(),[&](const auto& value) {
                return value.favorite_id==QString::fromUtf8(argv[4]);
            });
            if(favorite==reloaded.favorites().end()||favorite->quick_slot!=1||
                !std::get_if<MacroAssetRefV1>(&favorite->target)||
                std::get<MacroAssetRefV1>(favorite->target).asset_id!=QString::fromUtf8(argv[5]))
                return fail("shared Favorite did not retain its exact AssetID and Quick Access slot");
            LibraryMacroAssetV1 metadata;
            const auto definition=reloaded.read_macro_asset(std::get<MacroAssetRefV1>(favorite->target),&metadata);
            if(definition.id!=QString::fromUtf8(argv[6]).toStdString()||definition.revisions.size()!=2||
                definition.latest_revision!=2||metadata.accepted_revision!=1)
                return fail("payload identity, graph revisions or separate accepted asset revision changed");
            auto document=empty_document("cold-session","cold-composition","cold-artboard");
            Session session(std::move(document));
            Primitive source=default_primitive("cold-source","nect.shape.rectangle");
            session.apply({Command{CreatePrimitive{"cold-composition","","cold-target","Cold Macro target",source}}},session.revision());
            const auto before=encode(session.document());const auto expected=session.revision();
            InstantiateMacro import{"cold-target","cold-imported-definition","cold-macro-instance",1,0};
            import.imported_definition=definition;import.asset_id=metadata.ref.asset_id.toStdString();
            import.accepted_asset_revision=metadata.accepted_revision;
            session.apply({StructuralCommand{MacroCommand{std::move(import)}}},expected);
            const auto& entry=session.document().objects.at("cold-target").stack.front();
            if(session.revision()!=expected+1||entry.id!="cold-macro-instance"||!entry.macro||
                entry.macro->definition!="cold-imported-definition"||entry.macro->pinned_revision!=1||
                session.document().macro_definitions.at("cold-imported-definition").revisions!=definition.revisions||
                macro_parameter_value(session.document(),"cold-target","cold-macro-instance","macro.offset.amount")!=12)
                return fail("fresh Session import did not preserve graphs, pin and default Amount 12");
            session.undo(session.revision());
            if(encode(session.document())!=before||session.document().macro_definitions.contains("cold-imported-definition"))
                return fail("one Undo did not remove the imported definition and instance together");
            session.redo(session.revision());
            return session.document().objects.at("cold-target").stack.front().id=="cold-macro-instance"&&
                session.document().macro_definitions.at("cold-imported-definition").revisions==definition.revisions?
                0:fail("Redo did not restore the cold Macro import");
        }
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch directory is available");
        model_and_persistence(scratch.path()+"/model");
        grouped_settings_identity(scratch.path()+"/grouped");
        unified_effect_favorites(scratch.path()+"/unified");
        portable_preset_assets(scratch.path()+"/portable-presets");
        portable_preset_closure_assets(scratch.path()+"/portable-preset-closures");
        portable_macro_assets(scratch.path()+"/portable-macros");
        host_and_ui_placement(scratch.path()+"/placement");
        portable_preset_library_ui(scratch.path()+"/portable-preset-ui");
        portable_macro_library_ui(scratch.path()+"/portable-macro-ui");
        unsafe_preset_root_keeps_legacy_library_usable(scratch.path()+"/unsafe-preset-root-ui");
        std::cout<<"PASS "<<checks<<" Folder Library/settings/Host/UI assertions\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
