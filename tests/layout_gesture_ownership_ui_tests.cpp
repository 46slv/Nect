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

Document fixture(){auto d=empty_document("layout-gesture-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;board.layout=ArtboardLayout{Margin{10,10,10,10},{}};Object shape;shape.id="shape";shape.name="Original";shape.source=default_primitive("shape-source","nect.shape.rectangle");shape.source->parameters.at("width").literal=80;shape.source->parameters.at("height").literal=60;shape.source->parameters.at("center_x").literal=320;shape.source->parameters.at("center_y").literal=240;auto fill=default_operation("shape-fill","nect.paint.fill");fill.parameters.at("r").literal=1;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=0;shape.stack={fill};d.objects.emplace(shape.id,shape);d.compositions.front().roots={shape.id};return d;}

void type(QLineEdit* input,const char* value){check(input,"Original live Margin input");input->setFocus();input->selectAll();QTest::keyClicks(input,value);events();}
void flow(const QString& root,int mode){auto initial=fixture();valid(initial);auto current=initial;current.objects.at("shape").name="Current source";valid(current);Session base(initial);base.apply({Rename{"shape","Current source"}},0);auto margin11=current.compositions.front().artboards.front().layout; margin11->margin->left=11;auto goal=current;goal.compositions.front().artboards.front().layout=margin11;valid(goal);auto owner=base;owner.begin_gesture(1);owner.update_gesture({SetArtboardLayout{"comp","art",margin11}});check(owner.document()==current&&owner.preview_document()==goal&&owner.revision()==1&&owner.gesture_generation()==1,"Independent entire acquired Layout preview BEFORE Window");auto external=current;external.objects.at("shape").source->parameters.at("height").literal=90;valid(external);auto caller=base;caller.begin_gesture(1);caller.update_gesture({Set{{"shape","","generator.height"},90}});check(caller.document()==current&&caller.preview_document()==external&&caller.gesture_generation()==owner.gesture_generation()&&caller.gesture_identity()!=owner.gesture_identity(),"Independent equal-counter unrelated preview/token BEFORE Window");auto expected=caller;auto expected_doc=current;if(mode==3||mode==4){expected=owner;if(mode==3){expected.commit_gesture();expected_doc=goal;auto undo=expected;undo.undo(2);check(undo.document()==current,"Independent entire Layout Undo BEFORE Window");undo.redo(3);check(undo.document()==goal&&undo.history()==expected.history(),"Independent entire Layout Redo/one History BEFORE Window");}else expected.cancel_gesture();}
const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected_doc);write(prefix+"-expected-preview.nect",expected.preview_document());QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();events();w.canvas->set_active_artboard("comp","art",false);auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();QPointer<QLineEdit> input=control<QLineEdit>(w,"margin-left");reveal(input);check(input->text()=="10"&&!input->isReadOnly(),"Original literal Margin Left producer exact before input");const auto host_id=w.host.session_id;if(mode!=5){type(input,"11");same(w.host.session,owner);}if(mode<3||mode==5){w.host.session=caller;check(w.host.session_id==host_id,"Original Host ID remains exact at independent Session collision");same(w.host.session,caller);}check(w.grab().save(prefix+"-before.png"),"Original Layout producer before action");if(mode==0){type(input,"12");if(input){auto* apply=control<QPushButton>(w,"margin-apply");reveal(apply);apply->click();events();}}else if(mode==1||mode==3){auto* apply=control<QPushButton>(w,"margin-apply");reveal(apply);apply->click();events();}else{QTest::keyClick(input,Qt::Key_Escape);events();}
const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Original Layout result before Save");QJsonObject v{{"mode",mode},{"status",status},{"document_equal",observed.document()==expected_doc},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"caller_identity_preserved",mode==3||mode==4||observed.gesture_identity()==caller.gesture_identity()},{"input_alive",bool(input)},{"view_equal",w.canvas->selections().empty()&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"save_run",mode==3||mode==4}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable entire Layout ownership readback");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["caller_identity_preserved"].toBool(),"Entire independent caller identity and current Artboard view");if(mode==3||mode==4){w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Entire finished Layout native Save/snapshot");}w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Margin text/Apply/Escape; physical/clipboard/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_LAYOUT_GESTURE_OWNERSHIP_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<6;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Layout gesture ownership cases failed: "+std::to_string(failures));std::cout<<"PASS Layout gesture ownership "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Layout gesture ownership "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
