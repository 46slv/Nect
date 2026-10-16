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
#include <QInputDialog>
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

Document fixture(){auto d=empty_document("collection-rename-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;Object shape;shape.id="shape";shape.name="Original";shape.source=default_primitive("shape-source","nect.shape.rectangle");shape.source->parameters.at("width").literal=80;shape.source->parameters.at("height").literal=60;shape.source->parameters.at("center_x").literal=320;shape.source->parameters.at("center_y").literal=240;auto fill=default_operation("shape-fill","nect.paint.fill");fill.parameters.at("r").literal=1;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=0;shape.stack={fill};d.objects.emplace(shape.id,shape);d.compositions.front().roots={shape.id};d.collections={{"first","Duplicate label",{shape.id}},{"target","Duplicate label",{shape.id}}};return d;}

void flow(const QString& root,int mode){auto initial=fixture();valid(initial);auto current=initial;current.objects.at("shape").name="Current source";valid(current);Session base(initial);base.apply({Rename{"shape","Current source"}},0);check(base.document()==current&&base.revision()==1,"Entire current Session BEFORE Window");auto foreign_initial=initial;foreign_initial.id="caller-collection-document";foreign_initial.objects.at("shape").source->parameters.at("height").literal=90;foreign_initial.collections.at(1).name="Caller collection";valid(foreign_initial);auto foreign_current=foreign_initial;foreign_current.objects.at("shape").name="Current source";valid(foreign_current);Session foreign(foreign_initial);foreign.apply({Rename{"shape","Current source"}},0);check(foreign.document()==foreign_current&&foreign.revision()==base.revision()&&foreign.gesture_generation()==base.gesture_generation(),"Independent same-counter caller Document with colliding stable Collection ID BEFORE Window");auto concurrent=base;concurrent.apply({Rename{"shape","Concurrent source"}},1);auto concurrent_doc=current;concurrent_doc.objects.at("shape").name="Concurrent source";valid(concurrent_doc);check(concurrent.document()==concurrent_doc,"Full concurrent native oracle BEFORE Window");auto busy=base;busy.begin_gesture(1);busy.update_gesture({Set{{"shape","","generator.height"},90}});auto preview=current;preview.objects.at("shape").source->parameters.at("height").literal=90;valid(preview);check(busy.document()==current&&busy.preview_document()==preview,"Independent entire busy caller preview BEFORE Window");auto renamed=current;renamed.collections.at(1).name="Renamed";valid(renamed);auto committed=base;committed.apply({CollectionCommand{RenameCollection{"target","Renamed"}}},1);check(committed.document()==renamed,"Exact independent Collection name command/native BEFORE Window");auto history=committed;history.undo(2);check(history.document()==current,"Full Collection name Undo BEFORE Window");history.redo(3);check(history.document()==renamed&&history.history()==committed.history(),"Full one History/Redo BEFORE Window");auto expected=base;if(mode<2)expected=foreign;else if(mode==3)expected=concurrent;else if(mode==4)expected=busy;else if(mode==5)expected=committed;
const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("shape","");events();QPointer<QAction> actor=w.findChild<QAction*>("rename-collection");check(actor&&actor->isEnabled(),"Original Collection rename menu action enabled");const auto original_host=w.host.session_id;int stages=0;QString callback_error;const auto step=[&](int stage){try{auto* d=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());check(d&&d->windowTitle()=="Rename Collection","Original modal Collection dialog exact");++stages;if(stage==0){auto* choices=d->findChild<QComboBox*>();check(choices&&choices->count()==2&&choices->itemText(1).contains("target"),"Duplicate labels present exact stable-ID target in second original row");choices->setCurrentIndex(1);if(mode==0)w.host.session=foreign;check(w.grab().save(prefix+"-before.png"),"Original Collection choice actual render");d->accept();}else{check(d->textValue()=="Duplicate label","Original retained target name prompt");if(mode==1)w.host.session=foreign;else if(mode==2)w.host.session_id+="-changed";else if(mode==3)w.host.session=concurrent;else if(mode==4)w.host.session=busy;d->setTextValue("Renamed");check(w.grab().save(prefix+"-name.png"),"Original Collection name actual render");if(mode==6)d->reject();else d->accept();}}catch(const std::exception& e){callback_error=QString::fromUtf8(e.what());if(auto* d=qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))d->reject();}};
QTimer::singleShot(0,&w,[&]{step(0);if(callback_error.isEmpty())QTimer::singleShot(0,&w,[&]{step(1);});});actor->trigger();events();check(callback_error.isEmpty()&&stages==2,"Both original modal stages complete without fixture exception");const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual original rename result BEFORE Save");QJsonObject v{{"mode",mode},{"status",status},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"caller_identity_preserved",mode!=4||observed.gesture_identity()==busy.gesture_identity()},{"actor_alive",bool(actor)},{"host_equal",mode==2?w.host.session_id!=original_host:w.host.session_id==original_host},{"view_equal",w.canvas->selected_object=="shape"&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"save_run",mode!=4}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable entire Collection name readback BEFORE Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["host_equal"].toBool()&&v["caller_identity_preserved"].toBool(),"Exact caller identity/view and ordered non-owning members remain protected");if(mode!=4){w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Whole native Collection rename/refusal Save/snapshot");}w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Collection chooser/name dialogs; physical/clipboard/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_COLLECTION_RENAME_DOCUMENT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<7;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Collection rename Document cases failed: "+std::to_string(failures));std::cout<<"PASS Collection rename Document "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Collection rename Document "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
