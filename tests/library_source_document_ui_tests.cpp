#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include "semantic_control.hpp"
#include <QApplication>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QInputDialog>
#include <QListWidget>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <iostream>
#include <algorithm>
#include <memory>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace{
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full caller native-valid BEFORE Window");}
void same(const Session& a,const Session& b){check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Entire caller Session/native/History/preview/revision/generation unchanged");}
void write(const QString& path,const Document& d){QFile f(path);const auto b=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(b.data(),b.size())==static_cast<qint64>(b.size()),"Immutable actual or independent native retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error("Original visible source producer missing");}
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(alive&&alive->isVisible()&&alive->isEnabled(),"Original source producer survives reveal and is enabled");}

MacroDefinition custom_macro() {
    auto offset=default_operation("effects-macro-offset","nect.shape.offset");
    offset.parameters.at("amount").literal=8;
    auto repeater=default_operation("effects-macro-repeater","nect.shape.repeater");
    repeater.parameters.at("copies").literal=2;
    repeater.parameters.at("position_x").literal=120;
    MacroDefinitionRevision revision;revision.revision=1;
    revision.input={"effects-macro-input","local_paths_and_paint"};
    revision.output={"effects-macro-output","local_paths_and_paint"};
    revision.nodes={{offset,"effects-offset-in","effects-offset-out"},
        {repeater,"effects-repeater-in","effects-repeater-out"}};
    revision.edges={{{"","effects-macro-input"},{"effects-macro-offset","effects-offset-in"}},
        {{"effects-macro-offset","effects-offset-out"},{"effects-macro-repeater","effects-repeater-in"}},
        {{"effects-macro-repeater","effects-repeater-out"},{"","effects-macro-output"}}};
    revision.output_mapping={"effects-macro-repeater","effects-repeater-out"};
    revision.public_parameters.push_back({"macro.offset.amount","Amount","effects-macro-offset","amount",
        "number","du","local_paths_and_paint"});
    MacroDefinition definition;definition.id="effects-custom-macro";definition.label="Custom Offset Repeat";
    definition.revisions.emplace(1,std::move(revision));
    return definition;
}
PresetDefinition source_preset(){PresetDefinition d;d.id="portable-offset-preset";d.label="Portable Offset";d.schema_version=2;PresetEntry e;e.type="nect.shape.offset";e.parameters={{"amount",10},{"miter_limit",4}};d.entries={e};return d;}
Document fixture(){auto d=empty_document("effects-panel-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;Object shape;shape.id="shape";shape.name="Original";shape.source=default_primitive("shape-source","nect.shape.rectangle");shape.source->parameters.at("width").literal=80;shape.source->parameters.at("height").literal=60;shape.source->parameters.at("center_x").literal=320;shape.source->parameters.at("center_y").literal=240;auto fill=default_operation("shape-fill","nect.paint.fill");fill.parameters.at("r").literal=1;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=0;shape.stack={fill};d.objects.emplace(shape.id,shape);d.compositions.front().roots={shape.id};auto macro=custom_macro();d.macro_definitions.emplace(macro.id,macro);auto preset=source_preset();d.preset_definitions.emplace(preset.id,preset);return d;}
using Files=std::map<QString,QByteArray>;
Files files(const QString& root){Files result;QDirIterator it(root,{"*.json"},QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){const auto p=it.next();result.emplace(QDir(root).relativeFilePath(p),read(p));}return result;}
void raw(const QString& path,const QByteArray& bytes){QFile f(path);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes)==bytes.size(),"Immutable whole workspace asset bytes retained");}
QByteArray envelope(bool macro,const QString& id,unsigned revision,const MacroDefinition& m,const PresetDefinition& p){const auto payload=QByteArray::fromStdString(macro?canonical_macro_payload(m):canonical_preset_closure_payload({p,{}}));QJsonObject value{{"version",1},{"kind",macro?"macro_definition":"preset_definition"},{"asset_id",id},{"accepted_revision",int(revision)},{"sha256",QString::fromLatin1(QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex())},{"payload_schema",int(macro?portable_macro_payload_schema(m):portable_preset_closure_schema({p,{}}))},{"payload",QString::fromUtf8(payload)},{"label",QString::fromStdString(macro?m.label:p.label)}};return QJsonDocument(value).toJson(QJsonDocument::Compact);}
void flow(const QString& root,int mode){const bool macro=mode!=1&&mode!=3;const bool success=mode==2||mode==3||mode==6;const auto prefix=root+"/mode-"+QString::number(mode);auto initial=fixture();valid(initial);auto current=initial;current.objects.at("shape").name="Current source";valid(current);Session base(initial);base.apply({Rename{"shape","Current source"}},0);check(base.document()==current,"Entire base native/History BEFORE Window");auto foreign_initial=initial;foreign_initial.id="foreign-source-document";foreign_initial.macro_definitions.at("effects-custom-macro").revisions.at(1).nodes.front().operation.parameters.at("amount").literal=90;foreign_initial.macro_definitions.at("effects-custom-macro").label="Foreign Macro";foreign_initial.preset_definitions.at("portable-offset-preset").entries.front().parameters.at("amount")=77;foreign_initial.preset_definitions.at("portable-offset-preset").label="Foreign Preset";valid(foreign_initial);auto foreign_current=foreign_initial;foreign_current.objects.at("shape").name="Current source";valid(foreign_current);Session foreign(foreign_initial);foreign.apply({Rename{"shape","Current source"}},0);check(foreign.document()==foreign_current&&foreign.revision()==base.revision()&&foreign.gesture_generation()==base.gesture_generation(),"Independent colliding source IDs and equal Host/revision/generation BEFORE Window");auto concurrent=base;concurrent.apply({Rename{"shape","Concurrent source"}},1);auto concurrent_doc=current;concurrent_doc.objects.at("shape").name="Concurrent source";valid(concurrent_doc);check(concurrent.document()==concurrent_doc,"Entire concurrent source caller BEFORE Window");auto next=current.macro_definitions.at("effects-custom-macro").revisions.at(1);next.revision=2;next.nodes.front().operation.parameters.at("amount").literal=33;auto preview=current;preview.macro_definitions.at("effects-custom-macro").latest_revision=2;preview.macro_definitions.at("effects-custom-macro").revisions.emplace(2,next);valid(preview);auto busy=base;busy.begin_gesture(1);busy.update_gesture({MacroCommand{UpdateMacroDefinition{"effects-custom-macro",next}}});check(busy.document()==current&&busy.preview_document()==preview,"Independent whole source Macro preview; committed source remains old BEFORE Window");auto expected=base;if(mode<2)expected=foreign;else if(mode==5)expected=concurrent;else if(mode==6)expected=busy;write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);const auto asset_root=prefix+"-assets";auto library=std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},asset_root);auto* assets=library.get();auto baseline_macro=custom_macro();baseline_macro.label="Baseline Macro";baseline_macro.revisions.at(1).nodes.front().operation.parameters.at("amount").literal=3;auto baseline_preset=source_preset();baseline_preset.label="Baseline Preset";baseline_preset.entries.front().parameters.at("amount")=4;const auto ma=assets->publish_macro_asset(baseline_macro);const auto pa=assets->publish_preset(baseline_preset);assets->add_favorite(ma.ref,1);assets->add_favorite(pa.ref,2);settings.sync();const auto preferences=settings.value("library/v1/state");const auto before_files=files(asset_root);check(before_files.size()==2,"Two entire independent owned baseline asset files BEFORE Window");QString target_file;for(const auto& [name,bytes]:before_files){const bool is_macro=name.contains(ma.ref.asset_id);check(bytes==envelope(is_macro,is_macro?ma.ref.asset_id:pa.ref.asset_id,1,baseline_macro,baseline_preset),"Complete independent baseline asset envelope agrees BEFORE Window");if(is_macro==macro)target_file=name;}check(!target_file.isEmpty(),"Exact stable asset filename identified BEFORE Window");auto expected_files=before_files;if(success)expected_files.at(target_file)=envelope(macro,macro?ma.ref.asset_id:pa.ref.asset_id,2,custom_macro(),source_preset());int index=0;for(const auto& [name,bytes]:expected_files){raw(prefix+"-asset-expected-"+QString::number(index)+".json",bytes);raw(prefix+"-asset-before-"+QString::number(index)+".json",before_files.at(name));++index;}
Window w(prefix+"-recovery",std::move(library),&settings);w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("shape","");events();auto* actor=w.findChild<QAction*>("folder-library");check(actor&&actor->isEnabled(),"Original Folder Library action available");const auto original_host=w.host.session_id;QString callback_error,status;int stages=0;QTimer::singleShot(0,&w,[&]{auto* dialog=w.findChild<QDialog*>("folder-library-dialog");try{check(dialog&&dialog->isVisible(),"Original Library source dialog exists");++stages;auto* target=dialog->findChild<QComboBox*>(macro?"folder-library-macro-assets":"folder-library-preset-assets");auto* source=dialog->findChild<QComboBox*>(macro?"folder-library-source-macros":"folder-library-source-presets");check(target&&source,"Original stable asset/source combo controls exist");const auto asset_index=target->findData(macro?ma.ref.asset_id:pa.ref.asset_id),source_index=source->findData(macro?QStringLiteral("effects-custom-macro"):QStringLiteral("portable-offset-preset"));check(asset_index>=0&&source_index>=0,"Original exact workspace AssetID and Document source ID rows");target->setCurrentIndex(asset_index);source->setCurrentIndex(source_index);events();auto* apply=dialog->findChild<QPushButton*>(macro?"folder-library-macro-update":"folder-library-preset-update");check(apply&&apply->isVisible()&&apply->isEnabled(),"Original update source producer available");check(dialog->grab().save(prefix+"-before.png"),"Actual original Library target/source snapshot BEFORE caller change");if(mode<2)w.host.session=foreign;else if(mode==4)w.host.session_id+="-changed";else if(mode==5)w.host.session=concurrent;else if(mode==6)w.host.session=busy;apply->click();events();status=dialog->findChild<QLabel*>("folder-library-status")->text();check(dialog->grab().save(prefix+"-dialog.png"),"Actual Library source update result BEFORE close/Save");dialog->findChild<QPushButton*>("folder-library-close")->click();}catch(const std::exception& e){callback_error=QString::fromUtf8(e.what());if(dialog)dialog->reject();}});actor->trigger();events();check(callback_error.isEmpty()&&stages==1,"Original source update completes without fixture exception");const auto observed=w.host.session;const auto after_files=files(asset_root);index=0;for(const auto& [name,bytes]:after_files){raw(prefix+"-asset-observed-"+QString::number(index)+".json",bytes);++index;}LibraryMacroAssetV1 mm;LibraryPresetAssetV1 pm;const auto actual_macro=assets->read_macro_asset(ma.ref,&mm);const auto actual_preset=assets->read_preset_asset(pa.ref,&pm);const auto expected_macro=success&&macro?custom_macro():baseline_macro;const auto expected_preset=success&&!macro?source_preset():baseline_preset;write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual original caller source state BEFORE Save");QJsonObject v{{"mode",mode},{"status",status},{"window_status",w.statusBar()->currentMessage()},{"asset_files_equal",after_files==expected_files},{"source_payload_equal",actual_macro==expected_macro&&actual_preset==expected_preset},{"macro_asset_revision",qint64(mm.accepted_revision)},{"preset_asset_revision",qint64(pm.accepted_revision)},{"preferences_equal",settings.value("library/v1/state")==preferences},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"caller_identity_preserved",mode!=6||observed.gesture_identity()==busy.gesture_identity()},{"host_equal",mode==4?w.host.session_id!=original_host:w.host.session_id==original_host},{"view_equal",w.canvas->selected_object=="shape"&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"save_intent",mode!=6}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable full source/native/asset readback BEFORE Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["host_equal"].toBool()&&v["caller_identity_preserved"].toBool()&&v["preferences_equal"].toBool(),"Entire caller view/gesture/preference state unchanged");check(after_files==expected_files&&actual_macro==expected_macro&&actual_preset==expected_preset,"Full independent workspace asset files and decoded source payload exact");check(mm.ref.asset_id==ma.ref.asset_id&&pm.ref.asset_id==pa.ref.asset_id&&mm.accepted_revision==(success&&macro?2:1)&&pm.accepted_revision==(success&&!macro?2:1),"Stable AssetIDs and exact accepted revisions preserved");if(!success)check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Source-context refusal explicit in original Window perform status");else check(status.startsWith("Updated Workspace "),"Current committed source update explicit");if(mode!=6){w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Whole unmodified caller native Save/snapshot");}w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Folder Library source Update; physical/clipboard/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_LIBRARY_SOURCE_DOCUMENT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<7;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Library source Document cases failed: "+std::to_string(failures));std::cout<<"PASS Library source Document "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Library source Document "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
