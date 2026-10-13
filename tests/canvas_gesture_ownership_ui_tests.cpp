#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
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

Document fixture(){auto d=empty_document("canvas-gesture-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;Object shape;shape.id="shape";shape.name="Original";shape.source=default_primitive("shape-source","nect.shape.rectangle");shape.source->parameters.at("width").literal=80;shape.source->parameters.at("height").literal=60;shape.source->parameters.at("center_x").literal=320;shape.source->parameters.at("center_y").literal=240;auto fill=default_operation("shape-fill","nect.paint.fill");fill.parameters.at("r").literal=1;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=0;shape.stack={fill};d.objects.emplace(shape.id,shape);d.compositions.front().roots={shape.id};return d;}
QPoint ink(Canvas& canvas){const auto image=canvas.grab().toImage();double xsum=0,ysum=0;int count=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto c=image.pixelColor(x,y);if(c.red()>200&&c.green()<40&&c.blue()<40&&c.alpha()>200){xsum+=x;ysum+=y;++count;}}check(count>100,"Original rendered source provides actual body hit coordinate");return QPoint(qRound(xsum/count/image.devicePixelRatio()),qRound(ysum/count/image.devicePixelRatio()));}
void move(Canvas& canvas,QPoint p){QMouseEvent event(QEvent::MouseMove,QPointF(p),QPointF(canvas.mapToGlobal(p)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&canvas,&event);events();}
void flow(const QString& root,int mode){auto initial=fixture();valid(initial);auto current=initial;current.objects.at("shape").name="Current source";valid(current);Session base(initial);base.apply({Rename{"shape","Current source"}},0);check(base.document()==current&&base.revision()==1&&base.history().current_id==1,"Independent full base native/History BEFORE Window");auto owner=base;owner.begin_gesture(1);check(owner.document()==current&&owner.preview_document()==current&&owner.gesture_generation()==1&&owner.gesture_active(),"Independent owned press gesture BEFORE Window");auto caller=base;auto caller_doc=current;auto preview=current;
if(mode<3){auto foreign=initial;foreign.id="foreign-canvas-gesture-document";foreign.objects.at("shape").source->parameters.at("width").literal=100;caller_doc=foreign;caller_doc.objects.at("shape").name="Foreign source";valid(foreign);valid(caller_doc);caller=Session(foreign);caller.apply({Rename{"shape","Foreign source"}},0);preview=caller_doc;}
const auto caller_history=caller.history();if(mode<5||mode==7){if(mode==4){caller.begin_gesture(1);caller.cancel_gesture();}caller.begin_gesture(1);preview.objects.at("shape").source->parameters.at("height").literal=90;valid(preview);caller.update_gesture({Set{{"shape","","generator.height"},90}});check(caller.document()==caller_doc&&caller.preview_document()==preview&&caller.history()==caller_history&&caller.revision()==1&&caller.gesture_generation()==(mode==4?2:1),"Independent full unrelated external preview BEFORE Window");}
auto expected=caller;auto expected_doc=caller_doc;if(mode==5||mode==6){expected=owner;expected.update_gesture({TranslateObjects{{"shape"},12,0}});auto moved=current;moved.objects.at("shape").transform[4].literal=12;valid(moved);check(expected.preview_document()==moved&&expected.document()==current,"Independent complete ordinary body preview BEFORE Window");if(mode==5){expected.commit_gesture();expected_doc=moved;check(expected.document()==expected_doc&&expected.history().current_id==2&&expected.history().states.size()==3&&expected.revision()==2,"Independent exact body commit/one History BEFORE Window");auto undo=expected;undo.undo(2);check(undo.document()==current,"Independent full body Undo BEFORE Window");undo.redo(3);check(undo.document()==moved&&undo.history()==expected.history(),"Independent full body Redo BEFORE Window");}else{expected.cancel_gesture();expected_doc=current;check(expected.document()==current&&expected.preview_document()==current&&expected.history()==base.history()&&expected.gesture_generation()==1&&!expected.gesture_active(),"Independent owned Cancel full source BEFORE Window");}}
const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected_doc);write(prefix+"-expected-preview.nect",expected.preview_document());QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,950);w.show();events();w.canvas->fit_artboard();w.canvas->set_zoom(1);w.canvas->set_snap_enabled(false);w.canvas->set_selection("shape");events();same(w.host.session,base);const auto start=ink(*w.canvas),end=start+QPoint(12,0);check(w.canvas->zoom()==1,"Exact existing viewport scale makes12px=12du");if(mode!=7){QTest::mousePress(w.canvas,Qt::LeftButton,Qt::NoModifier,start);events();same(w.host.session,owner);}if(mode<5||mode==7){w.host.session=caller;if(mode==3)w.host.session_id="foreign-canvas-gesture-host";same(w.host.session,caller);}check(w.grab().save(prefix+"-before.png"),"Actual styled Canvas body/owned or external preview BEFORE action");if(mode==0||mode==3||mode==4){move(*w.canvas,end);QTest::mouseRelease(w.canvas,Qt::LeftButton,Qt::NoModifier,end);events();}else if(mode==1){QTest::mouseRelease(w.canvas,Qt::LeftButton,Qt::NoModifier,start);events();}else if(mode==2||mode==7)w.canvas->cancel_interaction();else{move(*w.canvas,end);if(mode==5)QTest::mouseRelease(w.canvas,Qt::LeftButton,Qt::NoModifier,end);else w.canvas->cancel_interaction();events();}
const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual Canvas gesture result BEFORE Save");QJsonObject v{{"mode",mode},{"status",status},{"document_equal",observed.document()==expected_doc},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"view_equal",w.canvas->selected_object=="shape"&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"save_run",mode==5||mode==6}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable complete Canvas ownership readback BEFORE Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool(),"Exact independent current source/output selection");check(mode<5?status=="REVISION_CONFLICT: Canvas gesture belongs to another Session, document, revision, or gesture":status.isEmpty(),"Exact foreign ownership refusal or ordinary current/no-owned success");if(mode==5||mode==6){w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Complete ordinary Canvas native Save/snapshot");}w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" product style/Canvas gesture events; physical/clipboard/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_CANVAS_GESTURE_OWNERSHIP_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Canvas gesture ownership cases failed: "+std::to_string(failures));std::cout<<"PASS Canvas gesture ownership "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Canvas gesture ownership "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
