#include "window.hpp"
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
 const bool success=mode==0,driven=mode==1;const auto initial=fixture();valid(initial);Session base(initial);auto intended=initial;entry(intended).gradient->stops.at(0).rgba[0].literal=1;valid(intended);auto goal=base;goal.apply({Set{gradient_ref("built","fill","gradient","stop.end.r"),1}},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==base.history().states.size()+1,"Independent manual exact single-byte HEX intended native agrees canonical one atomic channel command BEFORE Window");auto undone=goal;undone.undo(1);check(undone.document()==initial,"Independent whole native Undo BEFORE Window");auto redone=undone;redone.redo(2);check(redone.document()==intended,"Independent whole native Redo BEFORE Window");
 auto caller_doc=initial;
 if(mode==2)entry(caller_doc).gradient->start_x.literal+=1;
 if(mode==3)entry(caller_doc).parameters.at("a").literal=.765;
 if(mode==4)entry(caller_doc).gradient->stops.at(0).rgba[0].literal=.7;
 if(mode==5)entry(caller_doc).gradient->id="replacement-gradient";
 if(mode==6)entry(caller_doc).gradient->stops.at(0).id="replacement-end";
 if(mode==7){auto& stack=caller_doc.objects.at("built").stack;stack.erase(std::remove_if(stack.begin(),stack.end(),[](const auto& e){return e.id=="fill";}),stack.end());auto removed=base;removed.apply({RemoveOperation{"built","fill"}},0);check(removed.document()==caller_doc,"Manual full missing entry agrees canonical removal BEFORE Window");}
 if(mode==8){caller_doc.objects.erase("built");auto& roots=caller_doc.compositions.front().roots;roots.erase(std::remove(roots.begin(),roots.end(),"built"),roots.end());auto removed=base;removed.apply({DeleteObjects{{"built"}}},0);check(removed.document()==caller_doc,"Manual full missing owner agrees canonical deletion BEFORE Window");}
 valid(caller_doc);Session caller(caller_doc);check(caller.document().id==base.document().id&&caller.revision()==0&&caller.gesture_generation()==0&&!caller.gesture_active(),"Independent exact same-ID/revision/generation replacement context BEFORE Window");
 if(driven){check(entry(caller_doc).gradient->stops.at(1).rgba[0].binding.has_value()&&entry(caller_doc).gradient->stops.at(1).rgba[1].expression.has_value(),"Current driven stop owns exact retained binding and expression BEFORE Window");bool refused=false;auto protected_caller=caller;try{protected_caller.apply({Set{gradient_ref("built","fill","gradient","stop.start.r"),1}},0);}catch(const Error& e){refused=e.code=="DRIVEN_PROPERTY";}check(refused,"Canonical current driven channel replacement refuses BEFORE Window");same(protected_caller,caller);}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?intended:caller_doc);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Property dock exists");dock->show();dock->raise();events();QPointer<QLineEdit> producer=control<QLineEdit>(w,driven?"gradient-stop-hex-start":"gradient-stop-hex-end");reveal(producer);if(!driven)check(producer->text()=="#C93C20DF","Exact old displayed bytes differ only in requested red byte; untouched precise G/B/A remain original");producer->setFocus(Qt::OtherFocusReason);events();check(producer,"Original HEX survives focus");producer->selectAll();const QString draft=driven?"#FFFFFFFF":"#FF3C20DF";for(auto c:draft){check(producer,"Original HEX survives each actual Qt keyboard character");QTest::keyClicks(producer.data(),QString(c));}check(producer&&producer->text()==draft&&producer->isModified(),"Original exact modified user draft before submission");same(w.host.session,base);w.host.session=caller;same(w.host.session,caller);
 QSignalSpy emitted(producer.data(),&QLineEdit::editingFinished);bool escaped=false;std::string exception;try{check(QMetaObject::invokeMethod(producer.data(),"editingFinished",Qt::DirectConnection),"Original HEX editingFinished invoked once");}catch(const std::exception& e){escaped=true;exception=e.what();}events();const auto status=w.statusBar()->currentMessage();const auto count=emitted.count();const auto observed=w.host.session;write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-status.png"),"Actual HEX producer status rendered");QJsonObject state{{"escaped_exception",escaped},{"exception",QString::fromStdString(exception)},{"signal_count",count},{"status",status},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original producer readback immutable");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"GRADIENT_HEX mode="<<mode<<" status="<<status.toStdString()<<" escaped="<<escaped<<" doc_equal="<<(observed.document()==(success?goal.document():caller.document()))<<" history_equal="<<(observed.history()==(success?goal.history():caller.history()))<<"\n";
 same(w.host.session,success?goal:caller);check(!escaped&&count>=1,"Original HEX producer does not escape and its signal is observed; full History proves one transaction despite focus-settling re-emission");check(success?status.isEmpty():status.startsWith(driven?"DRIVEN_PROPERTY: ":"PROPERTY_CONFLICT: "),"Current driven refusal or uncovered same-ID context reports precise status");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full native preserves intended channels/frame/source/stop order/caller");
 if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GRADIENT_HEX_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<9;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Gradient HEX context cases failed: "+std::to_string(failures));std::cout<<"PASS Gradient HEX context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Gradient HEX context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
