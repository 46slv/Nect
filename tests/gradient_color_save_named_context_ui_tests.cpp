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
#include <QLabel>
#include <QListWidget>
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
GradientStop& stop(Document& d,const std::string& id){for(auto& s:entry(d).gradient->stops)if(s.id==id)return s;throw std::runtime_error("Independent exact authored stop missing");}
void flow(const QString& root,int mode){
 const int kind=mode%2,context=mode/2;const auto target=gradient_ref("built","fill","gradient","stop.start.color"),red_ref=gradient_ref("built","fill","gradient","stop.start.r");auto initial=fixture();if(kind==1){auto manual=initial;stop(manual,"start").rgba[0].binding.reset();stop(manual,"start").rgba[0].expression=Expression{" 0.25000000000000 ",1};valid(manual);Session typed(initial);typed.apply({SetExpression{{red_ref},{" 0.25000000000000 ",1},true}},0);check(typed.document()==manual,"Independent full Expression source agrees typed replacement BEFORE Window");initial=manual;}
 valid(initial);Session base(initial);
 auto committed_doc=initial;committed_doc.objects.at("second").name="External committed Color unlink caller";valid(committed_doc);auto committed=base;committed.apply({Rename{"second","External committed Color unlink caller"}},0);check(committed.document()==committed_doc&&committed.revision()==1,"Independent full committed caller agrees canonical Rename BEFORE Window");auto preview_doc=initial;preview_doc.objects.at("second").name="External Color unlink preview";valid(preview_doc);auto active=base;active.begin_gesture(0);active.update_gesture({Rename{"second","External Color unlink preview"}});check(active.document()==initial&&active.preview_document()==preview_doc&&active.history()==base.history()&&active.gesture_active()&&active.revision()==0,"Independent full active unlink caller BEFORE Window");auto cancelled=active;cancelled.cancel_gesture();check(cancelled.document()==initial&&cancelled.preview_document()==initial&&cancelled.history()==base.history()&&!cancelled.gesture_active()&&cancelled.revision()==0&&cancelled.gesture_generation()==1,"Independent full cancelled unlink caller BEFORE Window");
 auto caller_doc=initial;QString message;
 if(context==0){caller_doc.id="replacement-unlink-document";message="SESSION_CONFLICT: Color action belongs to another document";}
 if(context==1)message="SESSION_CONFLICT: This color action belongs to another document session";
 if(context==2)message="REVISION_CONFLICT: Color target changed; reopen its actions";
 if(context==3||context==5)message="GESTURE_ACTIVE: Finish or cancel the current gesture before another edit";
 if(context==4)message="REVISION_CONFLICT: Color context changed; reopen its actions";
 if(context==6){stop(caller_doc,"start").rgba[0].literal=.4567890123456789;message="PROPERTY_CONFLICT: The original Color channels changed";}
 if(context==7){auto& channel=stop(caller_doc,"start").rgba[0];channel.expression.reset();channel.binding=Binding{operation_ref("second","source-fill","b"),.25,.125,"copy_local_value"};message="PROPERTY_CONFLICT: The original Color channels changed";}
 valid(caller_doc);Session caller=context==2?committed:context==3?active:context==4||context==5?cancelled:Session(caller_doc);
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=context==5?active:base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock exists");dock->show();dock->raise();events();QPointer<QPushButton> menu_button;for(auto* b:w.findChildren<QPushButton*>("color-actions")){auto ref=QJsonDocument::fromJson(b->property("nect-color-reference").toByteArray()).object();if(b->isVisible()&&ref.value("object").toString()=="built"&&ref.value("point").toString().isEmpty()&&ref.value("field").toString()==QString::fromStdString(target.field)){menu_button=b;break;}}check(menu_button,"Original exact stable Gradient start colorRef action button");reveal(menu_button);QPointer<QAction> producer=menu_button->menu()->findChild<QAction*>("color-create-named");check(producer&&producer->isEnabled(),"Original explicit Save value as named Color action enabled");same(w.host.session,context==5?active:base);const auto sid=w.host.session_id;w.host.session=caller;if(context==1)w.host.session_id+="-other";same(w.host.session,caller);QSignalSpy emitted(producer,&QAction::triggered);producer->trigger();events();const auto status=w.statusBar()->currentMessage();QPointer<QDialog> manager=w.findChild<QDialog*>("color-manager");const bool manager_visible=manager&&manager->isVisible();write(path+"-observed.nect",w.host.session.document());check(w.grab().save(path+"-status.png"),"Actual original Save named action Window/status rendered");QJsonObject state{{"manager_visible",manager_visible},{"named_color_count",static_cast<int>(w.host.session.document().named_colors.size())},{"signal_count",emitted.count()},{"status",status},{"document_equal",w.host.session.document()==(caller.document())},{"history_equal",w.host.session.history()==(caller.history())},{"preview_equal",w.host.session.preview_document()==(caller.preview_document())},{"gesture_active",w.host.session.gesture_active()},{"source_kind",kind}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original Unlink producer immutable readback");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"GRADIENT_COLOR_SAVE_NAMED mode="<<mode<<" context="<<context<<" source_kind="<<kind<<" status="<<status.toStdString()<<" doc_equal="<<(w.host.session.document()==(caller.document()))<<" history_equal="<<(w.host.session.history()==(caller.history()))<<"\n";same(w.host.session,caller);check(emitted.count()==1,"One original actual Save named Color action producer");check(status==message&&!manager_visible,"Stale original Save named Color refuses before evaluation/new_ID/palette/manager authoring");
 if(!w.host.session.gesture_active()){w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full native retained evaluatedRGBA/source/whole caller");}else{check(read(path+"-observed.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-observed.nect",path+"-snapshot.nect"),"Actual committed native encode/caller active preview preserved; activeHostSaveNOTRUN");same(w.host.session,caller);}
 if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.session_id=sid;w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GRADIENT_COLOR_SAVE_NAMED_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<16;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Gradient Color Save named cases failed: "+std::to_string(failures));std::cout<<"PASS Gradient Color Save named "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Gradient Color Save named "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
