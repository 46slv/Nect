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
void flow(const QString& root,bool width){
 const auto initial=fixture();Session base(initial);const std::string axis=width?"width":"height",other=width?"height":"width";const Ref target{"rect","",std::string("generator.")+axis};const auto label=width?QString("Width"):QString("Height");
 const auto first=QString::fromStdString("ref(\"rect\",\"\",\"generator."+other+"\")");auto body=first,twenty=first;for(int i=1;i<12;++i)body+="\n + 1";for(int i=1;i<20;++i)twenty+="\n + 1";auto wrapped=first;for(int i=0;i<20;++i)wrapped+=" + "+first;
 const auto intended=source_goal(initial,axis,body),long_intended=source_goal(initial,axis,twenty),wrap_intended=source_goal(initial,axis,wrapped),single_intended=source_goal(initial,axis,"17");auto after=base;after.apply({SetExpression{{target},{body.toStdString(),1},false}},0);check(after.document()==intended,"Independent full scalar source intended Document BEFORE Window");check(evaluate(after.document()).at(target)==initial.objects.at("rect").source->parameters.at(other).literal+11,"Independent evaluated scalar source BEFORE Window");auto undone=after;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 auto long_session=base;long_session.apply({SetExpression{{target},{twenty.toStdString(),1},false}},0);check(long_session.document()==long_intended,"Independent long source native-valid BEFORE Window");auto wrap_session=base;wrap_session.apply({SetExpression{{target},{wrapped.toStdString(),1},false}},0);check(wrap_session.document()==wrap_intended,"Independent wrapped source native-valid BEFORE Window");auto single_session=base;single_session.apply({SetExpression{{target},{"17",1},false}},0);check(single_session.document()==single_intended,"Independent single source native-valid BEFORE Window");
 const auto path=root+(width?"/width":"/height");write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);write(path+"-long-expected.nect",long_intended);write(path+"-wrap-expected.nect",wrap_intended);write(path+"-single-expected.nect",single_intended);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1000);w.show();w.activateWindow();events();w.canvas->set_selection("rect");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* fx=named<QPushButton>(w,label+" expression editor");reveal(fx);QTest::mouseClick(fx,Qt::LeftButton);events();same(w.host.session,base);
 auto editor=[&]{auto* e=named<QPlainTextEdit>(w,label+" expression");const auto reference=QJsonDocument::fromJson(e->property("nect-reference").toByteArray()).object();check(reference.value("object").toString()=="rect"&&reference.value("point").toString().isEmpty()&&reference.value("field").toString()==QString::fromStdString(target.field),"Exact stable canonical scalar Ref UI target");return e;};
 auto* e=editor();reveal(e);e->setPlainText("");events();same(w.host.session,base);const auto empty=e->height();e->setPlainText("17");events();same(w.host.session,base);const auto one=e->height();e->setPlainText("1\n+ 2");events();same(w.host.session,base);const auto two=e->height();std::cout<<"SCALAR_INLINE axis="<<axis<<" empty="<<empty<<" single="<<one<<" two="<<two<<"\n";check(e->grab().save(path+"-two.png"),"Actual generic scalar two-line editor rendered");check(two>one,"Ordinary scalar multiline inline editor grows from one to two lines");check(empty==one,"Blank and single-line generic source compact");
 e->setPlainText(body);events();same(w.host.session,base);const auto capped=e->height();e->setPlainText(twenty);events();same(w.host.session,base);check(e->height()==capped&&capped>two,"Ordinary scalar source uses existing shared expansion cap");e->setPlainText(wrapped);events();same(w.host.session,base);check(e->height()>one&&e->height()<=capped,"Generic scalar visual wrapping grows within existing cap");e->setPlainText("17");events();same(w.host.session,base);check(e->height()==one,"Generic source collapse returns compact height");
 e->setPlainText(body);events();same(w.host.session,base);w.host.save(path+"-initial-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-initial-saved.nect")==read(path+"-initial-expected.nect")&&QFile::copy(path+"-initial-saved.nect",path+"-initial-snapshot.nect"),"Expanded private scalar source excluded from immutable initial native bytes");e=editor();reveal(e);check(e->toPlainText()==body&&e->height()==capped,"Event-ready Save preserves exact private source and inline expansion");e->setFocus();QTest::keyClick(e,Qt::Key_Return,Qt::ControlModifier);events();same(w.host.session,after);w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,after);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Existing explicit scalar source Apply exact full intended immutable native bytes");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_SCALAR_INLINE_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<2;++mode){try{flow(root,mode==0);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Scalar inline cases failed: "+std::to_string(failures));std::cout<<"PASS Scalar inline "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Scalar inline "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
