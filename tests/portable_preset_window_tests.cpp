#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
template<class T>T* named(QObject& scope,const char* name){
    auto* control=scope.findChild<T*>(name);check(control!=nullptr,name);return control;
}
void click(QObject& scope,const char* name){auto* button=named<QPushButton>(scope,name);check(button->isEnabled(),"Public control enabled");button->click();events();}
void library_dialog(Window& window,const std::function<void(QDialog&)>& action){
    bool entered=false;std::exception_ptr failure;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("folder-library-dialog");
        if(!dialog){failure=std::make_exception_ptr(std::runtime_error("Missing Library dialog"));return;}
        entered=true;
        try{action(*dialog);}catch(...){failure=std::current_exception();
            std::cerr<<window.statusBar()->currentMessage().toStdString()<<'\n';
            std::cerr<<named<QLabel>(*dialog,"folder-library-status")->text().toStdString()<<'\n';}
        dialog->accept();
    });
    named<QAction>(window,"folder-library")->trigger();events();
    if(failure)std::rethrow_exception(failure);check(entered,"Opened production Folder Library");
}
void choose_favorite(QDialog& dialog,const QString& id){
    auto* list=named<QListWidget>(dialog,"folder-library-favorites");
    for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole).toString()==id){list->setCurrentRow(row);return;}
    throw std::runtime_error("Missing exact Favorite");
}
Document fixture(Document document){
    document.objects.clear();document.compositions.front().roots={"path"};
    Object path;path.id="path";path.name="Portable target";path.kind=Kind::path;
    Contour contour;contour.id="contour";contour.closed=true;
    const std::array<Vec2,4> anchors{{{0,0},{100,0},{100,60},{0,60}}};
    for(std::size_t i=0;i<anchors.size();++i){Point point;point.id="p"+std::to_string(i);
        point.x.literal=anchors[i].x;point.y.literal=anchors[i].y;contour.points.push_back(point);}
    path.contours={contour};document.objects.emplace(path.id,path);
    MacroDefinitionRevision graph;graph.graph_version=2;
    graph.input={"input","local_paths_and_paint"};graph.output={"output","local_paths_and_paint"};
    auto offset=default_operation("offset","nect.shape.offset");offset.parameters.at("amount").literal=5;
    graph.nodes={{offset,"node-in","node-out"}};
    graph.edges={{{"","input"},{"offset","node-in"}},{{"offset","node-out"},{"","output"}}};
    graph.output_mapping={"offset","node-out"};
    graph.public_parameters={{"macro.offset.amount","Amount","offset","amount","number","du","local_paths_and_paint"}};
    MacroDefinition macro;macro.id="source-macro";macro.label="Retained offset";macro.latest_revision=2;
    macro.revisions.emplace(1,graph);graph.revision=2;graph.interface_version=3;
    graph.public_parameters.push_back({"macro.offset.enabled","Use offset","offset","enabled","boolean","boolean","local_paths_and_paint"});
    graph.nodes.front().operation.parameters.at("amount").literal=9;
    macro.revisions.emplace(2,graph);document.macro_definitions.emplace(macro.id,macro);
    PresetDefinition preset;preset.id="source-preset";preset.label="Pinned pair";preset.schema_version=2;
    PresetEntry first;first.kind="macro";first.type=macro_entry_type;first.macro_definition=macro.id;
    first.pinned_revision=1;first.overrides={{"macro.offset.amount",7}};
    auto second=first;second.pinned_revision=2;second.overrides={{"macro.offset.amount",11}};second.boolean_overrides={{"macro.offset.enabled",false}};
    preset.entries={first,second};document.preset_definitions.emplace(preset.id,preset);return document;
}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Read owned file");return f.readAll();}
void write(const QString& path,const QByteArray& bytes){QFile f(path);check(f.open(QIODevice::WriteOnly|QIODevice::Truncate),"Write owned file");check(f.write(bytes)==bytes.size(),"Write complete bytes");}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
        QTemporaryDir scratch;check(scratch.isValid(),"Owned workspace exists");
        QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
        auto library=std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},
            FolderLibrary::ReadbackOverride{},scratch.filePath("assets"));auto* assets=library.get();
        Window window(scratch.filePath("recovery"),std::move(library));window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1100,750);window.show();events();
        auto& session=window.host.session;session=Session(fixture(session.document()));window.host.edited();
        window.canvas->set_selection("path");events();
        auto* effects=named<QDockWidget>(window,"effects");effects->show();effects->raise();
        named<QTabWidget>(window,"effects-tabs")->setCurrentIndex(1);events();
        auto* catalog=named<QListWidget>(window,"presets-catalog");
        for(int i=0;i<catalog->count();++i)if(catalog->item(i)->data(Qt::UserRole).toString()=="source-preset")catalog->setCurrentRow(i);
        events();const auto original=session.document();const auto original_history=session.history();
        click(window,"preset-publish-library");
        const auto published=assets->preset_assets();check(published.size()==1&&published.front().payload_schema==3,"Window publishes self-contained schema3 asset");
        const auto ref=published.front().ref;
        check(assets->read_preset_closure_asset(ref)==capture_portable_preset_closure(original,"source-preset"),"Publish retains all graph revisions, pins and overrides");
        check(session.document()==original&&session.history()==original_history&&session.revision()==0,"Publish leaves Document/history untouched");
        const auto favorite=assets->add_favorite(ref);const auto favorite_state=settings.value("library/v1/state");
        auto updated=original.preset_definitions.at("source-preset");updated.label="Updated pinned pair";
        session.apply_preset_command(PresetCommand{UpdatePreset{updated}},session.revision());window.host.edited();events();
        const auto before_update=session.document();const auto history_before_update=session.history();
        library_dialog(window,[&](QDialog& dialog){
            auto* target=named<QComboBox>(dialog,"folder-library-preset-assets");target->setCurrentIndex(target->findData(ref.asset_id));
            auto* source=named<QComboBox>(dialog,"folder-library-source-presets");source->setCurrentIndex(source->findData(QStringLiteral("source-preset")));
            click(dialog,"folder-library-preset-update");
        });
        LibraryPresetAssetV1 metadata;const auto closure=assets->read_preset_closure_asset(ref,&metadata);
        check(metadata.accepted_revision==2&&closure.definition.label==updated.label&&closure.macro_definitions==original.macro_definitions,"Window update preserves closure and AssetID");
        check(session.document()==before_update&&session.history()==history_before_update&&settings.value("library/v1/state")==favorite_state,"Update leaves Document/history/Favorite settings untouched");
        library_dialog(window,[&](QDialog& dialog){choose_favorite(dialog,favorite.favorite_id);click(dialog,"folder-library-use-favorite");});
        const auto applied=session.document();const auto& stack=applied.objects.at("path").stack;
        check(session.revision()==2&&session.history().states.size()==history_before_update.states.size()+1&&stack.size()==2,"Favorite imports and applies in one revision/Undo");
        check(applied.macro_definitions.size()==2&&applied.preset_definitions.size()==2,"One fresh outer Macro and Preset imported");
        check(stack[0].macro->definition!=std::string("source-macro")&&stack[0].macro->definition==stack[1].macro->definition&&stack[0].macro->pinned_revision==1&&stack[1].macro->pinned_revision==2,"Both pins reference one fresh imported dependency");
        auto expected_macro=original.macro_definitions.at("source-macro");expected_macro.id=stack[0].macro->definition;
        check(applied.macro_definitions.at(expected_macro.id)==expected_macro&&stack[0].macro->overrides.at("macro.offset.amount")==7&&!stack[1].macro->boolean_overrides.at("macro.offset.enabled"),"All retained revisions and numeric/boolean values exact");
        session.undo(session.revision());window.host.edited();check(session.document()==before_update,"One Undo removes imported dependency, Preset and applications");
        session.redo(session.revision());window.host.edited();check(session.document()==applied,"One Redo restores exact IDs");
        const auto native=scratch.filePath("portable.nect.json");window.host.save(native);
        Host reopened(scratch.filePath("reopen"));reopened.open(native);
        check(reopened.session.document()==applied&&encode(reopened.session.document())==read(native).toStdString(),"Owned Host native reopen retains exact authored closure");
        if(const auto output=qEnvironmentVariable("NECT_PORTABLE_PRESET_DOCUMENT");!output.isEmpty()) {
            QFile file(output);const auto bytes=encode(applied);
            check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&file.write(bytes.data(),static_cast<qint64>(bytes.size()))==static_cast<qint64>(bytes.size()),"Owned portable candidate saved");
        }
        library_dialog(window,[&](QDialog& dialog){
            choose_favorite(dialog,favorite.favorite_id);const auto before=session.document();const auto history=session.history();const auto revision=session.revision();
            window.canvas->set_selection("");click(dialog,"folder-library-use-favorite");
            check(session.document()==before&&session.history()==history&&session.revision()==revision,"Changed target refuses without partial import");
            check(named<QLabel>(dialog,"folder-library-status")->text().contains("REVISION_CONFLICT"),"Changed Effects generation refusal visible");
        });
        window.canvas->set_selection("path");events();
        const auto path=scratch.filePath("assets/"+ref.asset_id+".preset.json");const auto bytes=read(path);write(path,"{}");
        const auto before_bad=session.document();const auto history_bad=session.history();const auto revision_bad=session.revision();
        library_dialog(window,[&](QDialog& dialog){choose_favorite(dialog,favorite.favorite_id);click(dialog,"folder-library-use-favorite");});
        check(session.document()==before_bad&&session.history()==history_bad&&session.revision()==revision_bad&&settings.value("library/v1/state")==favorite_state,"Corrupt Favorite preserves Document/history/settings");
        write(path,bytes);check(read(path)==bytes,"Owned asset bytes restored exactly");
        std::cout<<"portable_preset_window_tests: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
