#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include "semantic_control.hpp"
#include <QApplication>
#include <QMessageBox>
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

Document fixture(){auto d=empty_document("transform-action-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;for(const auto& id:{Id{"shape"},Id{"peer"},Id{"other"}}){Object object;object.id=id;object.name=id;object.source=default_primitive(id+"-source","nect.shape.rectangle");object.source->parameters.at("width").literal=80;object.source->parameters.at("height").literal=60;object.source->parameters.at("center_x").literal=id=="shape"?320:id=="peer"?180:460;object.source->parameters.at("center_y").literal=240;auto fill=default_operation(id+"-fill","nect.paint.fill");fill.parameters.at("r").literal=id=="shape"?1:0;fill.parameters.at("g").literal=id=="other"?1:0;fill.parameters.at("b").literal=id=="peer"?1:0;object.stack={fill};d.objects.emplace(id,object);d.compositions.front().roots.push_back(id);}auto& shape=d.objects.at("shape");shape.anchor={Scalar{10,{}},Scalar{20,{}}};shape.transform[4].literal=15.125;shape.transform[5].literal=-7.25;shape.compositing.opacity.literal=0.375;return d;}
void flow(const QString& root,int mode){const bool success=mode>=2;auto initial=fixture();initial.id="anchor-mode-document";valid(initial);auto current=initial;current.objects.at("other").name="Current retained";valid(current);Session base(initial);base.apply({Rename{"other","Current retained"}},0);check(base.document()==current,"Whole manual Anchor/source/affine/native/History before Window");auto foreign_initial=initial;foreign_initial.id="foreign-anchor-mode-document";foreign_initial.objects.at("shape").source->parameters.at("height").literal=90;foreign_initial.objects.at("peer").compositing.opacity.literal=0.625;valid(foreign_initial);auto foreign_current=foreign_initial;foreign_current.objects.at("other").name="Current retained";valid(foreign_current);Session foreign(foreign_initial);foreign.apply({Rename{"other","Current retained"}},0);check(foreign.document()==foreign_current&&foreign.revision()==base.revision()&&foreign.gesture_generation()==base.gesture_generation(),"Independent whole foreign native with equal Object/Host counters before Window");auto concurrent=base;concurrent.apply({Rename{"peer","Concurrent retained"}},1);auto concurrent_document=current;concurrent_document.objects.at("peer").name="Concurrent retained";valid(concurrent_document);check(concurrent.document()==concurrent_document,"Whole unrelated same-Doc caller before Window");auto expected=mode==0?foreign:mode==3?concurrent:base;const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},prefix+"-assets"),&settings);w.host.session=base;w.host.edited();w.resize(1400,1150);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("shape","");events();QPointer<QPushButton> actor=control<QPushButton>(w,"transform-edit-anchor");reveal(actor);check(!w.canvas->anchor_edit(),"Original Anchor mode initially disabled before retained producer");const auto original_host=w.host.session_id;const auto zoom=w.canvas->zoom();check(w.grab().save(prefix+"-before.png"),"Retained original Edit Anchor on Canvas producer");if(mode==0)w.host.session=foreign;if(mode==1)w.host.session_id+="-changed";if(mode==3)w.host.session=concurrent;actor->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();const bool anchor=w.canvas->anchor_edit();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual Anchor mode producer result before Save");QJsonObject v{{"mode",mode},{"status",status},{"anchor_edit",anchor},{"mode_equal",anchor==success},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"host_equal",mode==1?w.host.session_id!=original_host:w.host.session_id==original_host},{"view_equal",w.canvas->selected_object=="shape"&&w.canvas->selected_point.empty()&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"&&w.canvas->zoom()==zoom},{"save_intent",true}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable whole native/History and Anchor mode readback");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["host_equal"].toBool(),"Exact caller selection/plane/zoom/Host preserved");check(v["mode_equal"].toBool(),"Original Anchor mode producer bound to original Doc/Host; current mode remains usable");if(!success)check(status.startsWith("SESSION_CONFLICT:"),"Foreign Doc/Host Anchor mode refusal explicit");w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Exact whole native Save/snapshot, no normalization/no new History");w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Edit Anchor on Canvas; physical/clipboard/fonts/AE/pixels NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_ANCHOR_MODE_DOCUMENT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Anchor mode Document cases failed: "+std::to_string(failures));std::cout<<"PASS Anchor mode Document "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Anchor mode Document "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
