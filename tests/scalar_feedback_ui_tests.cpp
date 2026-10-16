#include "window.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Production control missing: "+name).toStdString());}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible()&&c->isEnabled(),"Exact production control visible and enabled");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
void same(const Session& a,const Session& b){
 std::cout<<"SNAPSHOT doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}


template<class T>T* named(Window& w,const QString& name){for(auto* c:w.findChildren<T*>())if(c->isVisible()&&c->accessibleName()==name)return c;throw std::runtime_error(("Exact scalar control missing: "+name).toStdString());}
Document fixture(){auto d=empty_document("scalar-inline","composition","board");Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);d.compositions.front().roots={o.id};auto source=default_primitive("source","nect.shape.rectangle");source.parameters.at("width").literal=80.123456789012345;source.parameters.at("height").literal=40.234567890123456;Session seed(d);seed.apply({CreatePrimitive{"composition","","rect","Rectangle",source}},0);d=seed.document();valid(d);return d;}
Document source_goal(const Document& initial,const std::string& field,const QString& body){auto d=initial;d.objects.at("rect").source->parameters.at(field).expression=Expression{body.toStdString(),1};valid(d);return d;}

void flow(const QString& root,int mode){
 const bool width=mode%2==0;const std::string axis=width?"width":"height",other=width?"height":"width";const auto label=width?QString("Width"):QString("Height");const bool same_literal=mode>=20,successful=mode<10||same_literal,semantic_noop=mode==2,operation_field=mode==2||mode==3;auto initial=fixture();if(operation_field){auto op=default_operation("offset","nect.shape.offset");op.parameters.at("amount").literal=12.123456789012345;op.parameters.at("miter_limit").literal=4.234567890123456;initial.objects.at("rect").stack.push_back(op);valid(initial);}const Ref target=operation_field?operation_ref("rect","offset",width?"amount":"miter_limit"):Ref{"rect","",std::string("generator.")+axis};Session base(initial);const auto start=property(initial,target).literal;check(property_semantic_descriptor(initial,target).has_value()==semantic_noop,"Independent actual scalar descriptor class BEFORE Window");
 const int operation=mode/2;const auto source=QString::fromStdString("ref(\"rect\",\"\",\"generator."+other+"\") + 1");const auto value=same_literal?start:operation==0?start+3:operation==2?start+.5:start;
 auto intended=initial;if(operation==3)intended=source_goal(initial,axis,source);else if(!operation_field)intended.objects.at("rect").source->parameters.at(axis).literal=value;valid(intended);
 auto after=base;if(successful&&!semantic_noop){if(operation==3)after.apply({SetExpression{{target},{source.toStdString(),1},false}},0);else after.apply({EditProperties{{target},operation==2?.5:operation==4?0:value,operation==2||operation==4}},0);}check(after.document()==(successful?intended:initial),"Independent full numeric/relative/no-op/source intended Document BEFORE Window");
 auto undone=after,redone=after;if(after.revision()){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());}
 auto replacement=initial;replacement.id+="-replacement";replacement.objects.at("sentinel").contours.front().points.front().x.literal+=7;valid(replacement);Session replaced(replacement);
 auto caller_doc=initial;caller_doc.objects.at("sentinel").contours.front().points.front().x.literal+=7;valid(caller_doc);const auto caller=Set{{"sentinel","point","x"},caller_doc.objects.at("sentinel").contours.front().points.front().x.literal};Session advanced(initial);advanced.apply({caller},0);check(advanced.document()==caller_doc,"Independent current caller revision BEFORE Window");auto foreign=base;foreign.begin_gesture(0);foreign.update_gesture({caller});check(foreign.document()==initial&&foreign.preview_document()==caller_doc,"Independent foreign caller snapshot BEFORE Window");auto cancelled=foreign;cancelled.cancel_gesture();
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);write(path+"-replacement-expected.nect",replacement);write(path+"-caller-expected.nect",caller_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1000);w.show();w.activateWindow();events();w.canvas->set_selection("rect");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
 auto input=[&]{for(auto* c:w.findChildren<QLineEdit*>()){if(!c->isVisible())continue;const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(ref.value("object").toString()=="rect"&&ref.value("point").toString().isEmpty()&&ref.value("field").toString()==QString::fromStdString(target.field)){check(c->property("nect-semantic-key").isValid()==semantic_noop,"Exact original canonical Ref and descriptor class");return c;}}throw std::runtime_error("Exact production scalar Ref control missing");};
 auto enter=[&](const QString& text){auto* c=input();reveal(c);c->setFocus();QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,text);QTest::keyClick(c,Qt::Key_Return);events();};
 enter("1 +");same(w.host.session,base);check(w.statusBar()->currentMessage().contains("INVALID_VALUE")&&input()->text()=="1 +","Incomplete scalar stays draft/unchanged and reports error");
 const int context=(mode-10)/2;if(!successful){if(context==0)w.host.session=replaced;else if(context==1)w.host.session_id+="-other";else if(context==2)w.host.session=advanced;else if(context==3)w.host.session=foreign;else{w.host.session=foreign;w.host.edited();events();w.host.session.cancel_gesture();same(w.host.session,cancelled);}}
 const auto text=same_literal?QString::number(start,'g',17):operation==1?QString::number(start,'g',17):operation==2?QString("+=.5"):operation==3?"="+source:operation==4?QString("+=0"):QString::number(start+3,'g',17);enter(text);
 std::cout<<"SCALAR_FEEDBACK mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual scalar feedback outcome rendered");
 if(!successful){same(w.host.session,context==0?replaced:context==2?advanced:context==3?foreign:context==4?cancelled:base);check(w.statusBar()->currentMessage().contains("CONFLICT")||w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Failed old context still reports actual refusal");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.close();events();return;}
 same(w.host.session,after);check(w.statusBar()->currentMessage().isEmpty(),"Actually successful current scalar outcome retires stale INVALID_VALUE");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,after);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Full scalar intended immutable native source bytes");if(after.revision()){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_SCALAR_FEEDBACK_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<22;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Scalar feedback cases failed: "+std::to_string(failures));std::cout<<"PASS Scalar feedback "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Scalar feedback "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
