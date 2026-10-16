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
Document normalized(Document d,const Id& generated){if(!generated.empty()){auto& stack=d.objects.at("shape").stack;check(stack.size()==2&&stack.back().id==generated,"Only one generated processing-entry ID is normalized");stack.back().id="00000000-0000-4000-8000-000000000001";}return d;}
void same_result(const Session& a,const Session& b,const Id& generated){check(normalized(a.document(),generated)==b.document()&&encode(normalized(a.document(),generated))==encode(b.document())&&normalized(a.preview_document(),generated)==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Entire caller Session/native/History/preview/revision/generation exact with explicit single-ID bijection");}
Document fixture(){auto d=empty_document("effects-panel-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;Object shape;shape.id="shape";shape.name="Original";shape.source=default_primitive("shape-source","nect.shape.rectangle");shape.source->parameters.at("width").literal=80;shape.source->parameters.at("height").literal=60;shape.source->parameters.at("center_x").literal=320;shape.source->parameters.at("center_y").literal=240;auto fill=default_operation("shape-fill","nect.paint.fill");fill.parameters.at("r").literal=1;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=0;shape.stack={fill};d.objects.emplace(shape.id,shape);d.compositions.front().roots={shape.id};auto macro=custom_macro();d.macro_definitions.emplace(macro.id,macro);return d;}
void flow(const QString& root,int mode){const bool macro=mode>=5;const bool success=mode==4||mode==6;const bool foreign_case=mode==0||mode==5;auto initial=fixture();valid(initial);auto current=initial;current.objects.at("shape").name="Current source";valid(current);Session base(initial);base.apply({Rename{"shape","Current source"}},0);check(base.document()==current&&base.revision()==1,"Entire base native/History BEFORE Window");auto foreign_initial=initial;foreign_initial.id="foreign-effects-document";foreign_initial.objects.at("shape").source->parameters.at("height").literal=90;valid(foreign_initial);auto foreign_current=foreign_initial;foreign_current.objects.at("shape").name="Current source";valid(foreign_current);Session foreign(foreign_initial);foreign.apply({Rename{"shape","Current source"}},0);check(foreign.document()==foreign_current&&foreign.revision()==base.revision()&&foreign.gesture_generation()==base.gesture_generation(),"Full equal-counter caller Document with colliding target ID BEFORE Window");auto concurrent=base;concurrent.apply({Rename{"shape","Concurrent source"}},1);auto concurrent_doc=current;concurrent_doc.objects.at("shape").name="Concurrent source";valid(concurrent_doc);check(concurrent.document()==concurrent_doc,"Full concurrent revision oracle BEFORE Window");auto busy=base;busy.begin_gesture(1);busy.update_gesture({Set{{"shape","","generator.height"},90}});auto preview=current;preview.objects.at("shape").source->parameters.at("height").literal=90;valid(preview);check(busy.document()==current&&busy.preview_document()==preview,"Independent whole busy preview BEFORE Window");ProcessingEntry entry;entry.id="00000000-0000-4000-8000-000000000001";
if(macro){entry.type=macro_entry_type;entry.macro=MacroInstance{"effects-custom-macro",1,{},{}};}else{entry.type="nect.shape.offset";entry.parameters.emplace("amount",Scalar{10,{}});entry.parameters.emplace("miter_limit",Scalar{4,{}});}
auto transformed=current;transformed.objects.at("shape").stack.push_back(entry);valid(transformed);auto committed=base;if(macro)committed.apply({MacroCommand{InstantiateMacro{"shape","effects-custom-macro","00000000-0000-4000-8000-000000000001",1,1}}},1);else committed.apply({AddOperation{"shape",entry,1}},1);check(committed.document()==transformed,"Independent entire literal processing-entry delta agrees with canonical command BEFORE Window");auto history=committed;history.undo(2);check(history.document()==current,"Entire processing-entry Undo BEFORE Window");history.redo(3);check(history.document()==transformed&&history.history()==committed.history(),"One History/full Redo BEFORE Window");auto expected=base;if(foreign_case)expected=foreign;else if(mode==2)expected=concurrent;else if(mode==3)expected=busy;else if(success)expected=committed;const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("shape","");events();auto* dock=w.findChild<QDockWidget*>("effects");check(dock,"Original Effects dock exists");dock->show();dock->raise();events();auto* tabs=w.findChild<QTabWidget*>("effects-tabs");check(tabs&&tabs->count()==2,"Original Effects/Presets tab set exists");tabs->setCurrentIndex(0);events();auto* catalog=control<QListWidget>(w,"effects-catalog");const auto type=macro?QStringLiteral("macro:effects-custom-macro"):QStringLiteral("nect.shape.offset");QListWidgetItem* chosen=nullptr;for(int i=0;i<catalog->count();++i)if(catalog->item(i)->data(Qt::UserRole).toString()==type)chosen=catalog->item(i);check(chosen&&!chosen->isHidden(),"Original exact Effects stable-ID catalog row");catalog->setCurrentItem(chosen);events();QPointer<QPushButton> actor=control<QPushButton>(w,"effects-apply");reveal(actor);check(actor&&actor->isEnabled(),"Original existing Effects Apply enabled");const auto original_host=w.host.session_id;check(w.grab().save(prefix+"-before.png"),"Actual original Effects producer before caller change");
if(foreign_case)w.host.session=foreign;else if(mode==1)w.host.session_id+="-changed";else if(mode==2)w.host.session=concurrent;else if(mode==3)w.host.session=busy;
actor->click();events();const auto observed=w.host.session;const auto status=control<QLabel>(w,"effects-status")->text();Id generated;if(success&&observed.document().objects.at("shape").stack.size()==2){generated=observed.document().objects.at("shape").stack.back().id;check(!generated.empty()&&generated!="shape-fill"&&generated!="00000000-0000-4000-8000-000000000001","New operation ID distinct, sole explicit bijection");}write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());auto actual_doc=normalized(observed.document(),generated),actual_preview=normalized(observed.preview_document(),generated);write(prefix+"-normalized.nect",actual_doc);write(prefix+"-normalized-preview.nect",actual_preview);check(w.grab().save(prefix+"-window.png"),"Actual original Effects result BEFORE Save");QJsonObject v{{"mode",mode},{"status",status},{"generated_id",QString::fromStdString(generated)},{"document_equal",actual_doc==expected.document()},{"preview_equal",actual_preview==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"history_bytes",qint64(observed.history().retained_bytes)},{"expected_history_bytes",qint64(expected.history().retained_bytes)},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"caller_identity_preserved",mode!=3||observed.gesture_identity()==busy.gesture_identity()},{"host_equal",mode==1?w.host.session_id!=original_host:w.host.session_id==original_host},{"view_equal",w.canvas->selected_object=="shape"&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"save_intent",mode!=3}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable whole readback BEFORE Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same_result(observed,expected,generated);check(v["view_equal"].toBool()&&v["host_equal"].toBool()&&v["caller_identity_preserved"].toBool(),"Entire caller view/preview identity preserved");
if(mode==0||mode==1||mode==5)check(status.contains("SESSION_CONFLICT"),"Document/Host refusal explicit");if(mode==2)check(status.contains("REVISION_CONFLICT"),"Revision refusal explicit");if(mode==3)check(status.contains("GESTURE_ACTIVE"),"Busy caller refusal explicit");if(success){check(status.startsWith("Applied "),"Original current Apply reports success");w.host.session.undo(2);check(w.host.session.document()==current,"Actual UI result whole Undo");w.host.session.redo(3);check(normalized(w.host.session.document(),generated)==transformed,"Actual UI result whole Redo");w.host.session=observed;}
if(mode!=3){w.host.save(prefix+"-saved.nect");w.host.flush();events();same_result(w.host.session,expected,generated);const auto saved=decode(read(prefix+"-saved.nect").toStdString());check(normalized(saved,generated)==expected.document()&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Entire raw/native Save and snapshot retained; only explicit operation ID bijection");}w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Effects catalog/Apply; physical/clipboard/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_EFFECTS_PANEL_DOCUMENT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<7;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Effects panel Document cases failed: "+std::to_string(failures));std::cout<<"PASS Effects panel Document "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Effects panel Document "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
