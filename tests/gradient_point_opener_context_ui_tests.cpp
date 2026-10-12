#include "window.hpp"
#include "semantic_point_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
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
ProcessingEntry& entry(Document& d){for(auto& e:d.objects.at("built").stack)if(e.id=="fill")return e;throw std::runtime_error("Independent exact paint entry missing");}
Document fixture(){
 Session setup(empty_document("gradient-mode-document","composition","artboard"));auto fill=default_operation("fill","nect.paint.fill");fill.parameters.at("r").literal=.125;fill.parameters.at("g").literal=.234;fill.parameters.at("g").expression=Expression{"0.23400000000000",1};fill.parameters.at("b").literal=.345;fill.parameters.at("b").binding=Binding{operation_ref("second","source-fill","b"),.75,.125,"copy_local_value"};fill.parameters.at("a").literal=.456;auto source=default_operation("source-fill","nect.paint.fill");source.parameters.at("r").literal=.8;source.parameters.at("b").literal=.2;Gradient g;g.id="gradient";g.start_x.literal=12.34567890123456;g.start_y.literal=2.34567890123456;g.end_x.literal=32.34567890123456;g.end_y.literal=3.34567890123456;GradientStop start,end;start.id="start";end.id="end";end.offset.literal=1;start.rgba[0].literal=.125;start.rgba[0].binding=Binding{operation_ref("second","source-fill","r"),.75,.125,"copy_local_value"};start.rgba[1].literal=.345;start.rgba[1].expression=Expression{"0.34500000000000",1};start.rgba[2].literal=.234;start.rgba[3].literal=.456;end.rgba[0].literal=.789;end.rgba[1].literal=.234;end.rgba[2].literal=.125;end.rgba[3].literal=.876;g.stops={end,start};fill.gradient=g;
 setup.apply({CreatePrimitive{"composition","","built","Built",default_primitive("shape","nect.shape.rectangle")},CreatePrimitive{"composition","","second","Second",default_primitive("source-shape","nect.shape.rectangle")},AddOperation{"second",source,1},AddOperation{"built",fill,1}},0);auto intended=setup.document();entry(intended).enabled_driver=operation_ref("second","source-fill","enabled");valid(intended);setup.apply({LinkOperationEnabled{operation_ref("built","fill","enabled"),operation_ref("second","source-fill","enabled"),false}},setup.revision());check(setup.document()==intended,"Manual full fixture paint enabled driver agrees dedicated typed command BEFORE Window");return setup.document();
}
void flow(const QString& root,int mode){
 const bool success=mode<2;const int context=success?-1:(mode-2)/2;const std::string endpoint=mode%2?"end":"start";const auto initial=fixture();valid(initial);Session base(initial);Session independent(initial);same(base,independent);
 auto preview_doc=initial;preview_doc.objects.at("second").name="External active caller preview";valid(preview_doc);auto active=base;active.begin_gesture(0);active.update_gesture({Rename{"second","External active caller preview"}});check(active.document()==initial&&active.preview_document()==preview_doc&&active.history()==base.history()&&active.revision()==0&&active.gesture_active(),"Independent full active preview caller BEFORE Window");auto cancelled=active;cancelled.cancel_gesture();check(cancelled.document()==initial&&cancelled.preview_document()==initial&&cancelled.history()==base.history()&&cancelled.revision()==0&&!cancelled.gesture_active()&&cancelled.gesture_generation()==1,"Independent whole cancelled caller BEFORE Window");
 auto caller_doc=initial;QString code;
 if(context==0){caller_doc.id="replacement-point-document";code="SESSION_CONFLICT";}
 if(context==1)code="REVISION_CONFLICT";
 if(context==2)code="GESTURE_ACTIVE";
 if(context==3){entry(caller_doc).gradient->start_x.literal+=1;code="PROPERTY_CONFLICT";}
 if(context==4){entry(caller_doc).gradient->stops.at(0).rgba[0].literal=.7;code="PROPERTY_CONFLICT";}
 if(context==5){entry(caller_doc).gradient->id="replacement-gradient";code="PROPERTY_CONFLICT";}
 if(context==6){auto& stack=caller_doc.objects.at("built").stack;stack.erase(std::remove_if(stack.begin(),stack.end(),[](const auto& e){return e.id=="fill";}),stack.end());auto removed=base;removed.apply({RemoveOperation{"built","fill"}},0);check(removed.document()==caller_doc,"Manual missing full paint entry agrees canonical removal BEFORE Window");code="PROPERTY_CONFLICT";}
 if(context==7){caller_doc.objects.erase("built");auto& roots=caller_doc.compositions.front().roots;roots.erase(std::remove(roots.begin(),roots.end(),"built"),roots.end());auto deleted=base;deleted.apply({DeleteObjects{{"built"}}},0);check(deleted.document()==caller_doc,"Manual whole missing owner agrees canonical deletion BEFORE Window");code="PROPERTY_CONFLICT";}
 valid(caller_doc);Session caller= context==1||context==2?cancelled:Session(caller_doc);check(caller.revision()==0&&!caller.gesture_active(),"Independent caller revision/active state BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=context==2?active:base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Property dock");dock->show();dock->raise();events();QPointer<QPushButton> producer=control<QPushButton>(w,"gradient-point-edit-fill-"+QString::fromStdString(endpoint));reveal(producer);w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(producer,&QPushButton::clicked);bool escaped=false;QString exception;try{producer->click();}catch(const std::exception& e){escaped=true;exception=QString::fromUtf8(e.what());}events();QPointer<QDialog> dialog=w.findChild<QDialog*>("semantic-gradient-point-dialog");const bool visible=dialog&&dialog->isVisible();const auto status=w.statusBar()->currentMessage();write(path+"-observed.nect",w.host.session.document());check(w.grab().save(path+"-status.png"),"Actual original paired opener Window/status");if(visible)check(dialog->grab().save(path+"-dialog.png"),"Actual original paired editor rendered");bool exact=false,cancelled_dialog=false;
 if(success){check(dialog&&visible,"Current actual paired editor opens");auto* editor=dialog->findChild<SemanticPointInput*>("semantic-gradient-point-input");check(editor,"Original semantic point editor exists");const std::array<double,2> intended=endpoint=="start"?std::array<double,2>{12.34567890123456,2.34567890123456}:std::array<double,2>{32.34567890123456,3.34567890123456};exact=editor->value()==intended&&editor->x_input()->text().toDouble()==intended[0]&&editor->y_input()->text().toDouble()==intended[1]&&!editor->x_input()->isModified()&&!editor->y_input()->isModified()&&editor->apply_button()->isEnabled()&&editor->cancel_button()->isEnabled();check(exact,"Original current exact Start/End baseline/precision/default unmodified/available buttons agree independent fixture");same(w.host.session,caller);QSignalSpy cancelled(editor->cancel_button(),&QPushButton::clicked);editor->cancel_button()->click();events();cancelled_dialog=cancelled.count()==1&&(!dialog||!dialog->isVisible());check(cancelled_dialog,"Actual current Cancel closes editor once without authoring");same(w.host.session,caller);}
 QJsonObject state{{"escaped_exception",escaped},{"exception",exception},{"signal_count",emitted.count()},{"status",status},{"dialog_visible_at_open",visible},{"exact_current_baseline",exact},{"actual_current_cancel",cancelled_dialog},{"document_equal",w.host.session.document()==caller.document()},{"history_equal",w.host.session.history()==caller.history()},{"endpoint",QString::fromStdString(endpoint)}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original opener immutable readback");metadata.write(QJsonDocument(state).toJson());metadata.close();same(w.host.session,caller);w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full native caller no-authoring pair");std::cout<<"GRADIENT_POINT_OPENER mode="<<mode<<" endpoint="<<endpoint<<" status="<<status.toStdString()<<" dialog_visible="<<visible<<" escaped="<<escaped<<"\n";check(!escaped&&emitted.count()==1,"Original paired opener signal1/no escaped exception");check(success?(visible&&exact&&cancelled_dialog):!visible,"Current exact paired dialog/Cancel; stale original producer refuses before creating editor");check(success?status.isEmpty():status.startsWith(code+": "),"Precise original paired opener refusal status");if(dialog)dialog->reject();events();w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GRADIENT_POINT_OPENER_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<18;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Gradient point opener cases failed: "+std::to_string(failures));std::cout<<"PASS Gradient point opener "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Gradient point opener "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
