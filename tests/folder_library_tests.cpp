#include "folder_library.hpp"
#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDialog>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QListWidget>
#include <QPushButton>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <iostream>
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
    check(favorite.quick_slot == 1 && library.favorite_for_slot(1)->ref.normalized_relative_path == "brand/logo.png",
        "A stable raster reference can own a Quick Access slot");
    FolderLibrary after_restart(settings);
    check(after_restart.favorites().size() == 1 && after_restart.favorites().front().favorite_id == favorite.favorite_id &&
        after_restart.favorites().front().ref.root_id == root_a.root_id && after_restart.favorites().front().quick_slot == 1,
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
        QTest::mouseClick(favorite_button,Qt::LeftButton);
        auto* favorite_list=dialog->findChild<QListWidget*>("folder-library-favorites");check(favorite_list&&favorite_list->count()==1,"Folder Library displays the persistent Favorite");
        favorite_list->setCurrentRow(0);
        auto* slot=dialog->findChild<QComboBox*>("folder-library-slot");check(slot,"Folder Library exposes Quick Access slots");slot->setCurrentIndex(slot->findData(1));
        auto* assign=dialog->findChild<QPushButton*>("folder-library-slot-set");check(assign,"Folder Library can assign the selected Favorite to a slot");
        QTest::mouseClick(assign,Qt::LeftButton);
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
                favorite->ref.root_id == QString::fromUtf8(argv[3]) ? 0 : 1;
        }
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch directory is available");
        model_and_persistence(scratch.path()+"/model");
        host_and_ui_placement(scratch.path()+"/placement");
        std::cout<<"PASS "<<checks<<" Folder Library/settings/Host/UI assertions\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
