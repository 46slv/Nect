#include "window.hpp"
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
Document fixture(){
 auto d=empty_document("size-document","composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.width=1200;b.height=900;
 b.width_driver=ArtboardSizeDriver{Ref{"parent","","artboard.width"}};b.height_driver=ArtboardSizeDriver{Ref{"parent","","artboard.height"}};
 b.layout=ArtboardLayout{Margin{10,20,30,40},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 comp.artboards.push_back({"parent","Parent",1600,0,1500,1000});comp.artboards.push_back({"other","Other",3200,0,1800,1200});
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};valid(d);return d;
}
void same(const Session& a,const Session& b){
 std::cout<<"SNAPSHOT doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}



void flow(const QString& root,int mode){
 const bool width=mode==0;const auto initial=fixture();Session base(initial);const std::string field=width?"artboard.width":"artboard.height",axis=width?"width":"height";
 const QString single="1800",two=QString::fromStdString("ref(\"other\",\"\",\""+field+"\") +\n1"),many=QString("1\n").repeated(11)+"1",more=QString("1\n").repeated(19)+"1",wrapped=QString("1 + ").repeated(200)+"1";
 const Expression source{two.toStdString(),1};auto intended=initial;auto& board=intended.compositions.front().artboards.front();(width?board.width_driver:board.height_driver)=ArtboardSizeDriver{source};valid(intended);auto applied=base;applied.apply({SetArtboardSizeExpression{{"board","",field},source,true}},0);check(applied.document()==intended,"Independent typed source equals full manual intended Document BEFORE Window");auto undone=applied;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();same(w.host.session,base);
 const auto name=QString::fromStdString("artboard-"+axis+"-expression");auto input=[&]{return control<QPlainTextEdit>(w,name);};auto set=[&](const QString& text){input()->setPlainText(text);events();same(w.host.session,base);check(input()->toPlainText()==text,"Exact private source text retained");};
 const int empty_height=input()->height();set(single);const int single_height=input()->height();check(single_height==empty_height,"Empty and short single-line editors stay compact");set(two);const int two_height=input()->height();std::cout<<"SIZE_INLINE mode="<<mode<<" empty="<<empty_height<<" single="<<single_height<<" two="<<two_height<<"\n";check(input()->grab().save(path+"-two.png"),"Actual two-line native editor retained");check(two_height>single_height,"Two explicit source lines expand inside existing row");
 set(many);const int capped=input()->height();check(capped>=two_height,"Additional explicit lines expand within bounded row");set(more);check(input()->height()==capped,"Long sources stop expanding at existing shared cap");
 set(wrapped);check(input()->height()>single_height&&input()->height()<=capped,"Long single source wraps and grows within existing cap");set(single);check(input()->height()==single_height,"Source reduction collapses to compact height");
 set(two);auto* replace=control<QCheckBox>(w,QString::fromStdString("artboard-"+axis+"-replace"));replace->setChecked(true);events();same(w.host.session,base);
 w.host.save(path+"-initial-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-initial-saved.nect")==read(path+"-initial-expected.nect"),"Private multiline source stays out of exact native Save");check(input()->toPlainText()==two&&input()->height()>=two_height,"Event-ready Save restores exact draft and expanded row");replace=control<QCheckBox>(w,QString::fromStdString("artboard-"+axis+"-replace"));check(replace->isChecked(),"Inline sizing preserves private replacement consent");
 auto* e=input();reveal(e);e->setFocus();QTest::keyClick(e,Qt::Key_Return,Qt::ControlModifier);events();same(w.host.session,applied);check(input()->toPlainText()==two&&input()->height()>single_height,"Committed multiline source remains expanded");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,applied);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Full intended multiline source native bytes exact");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_SIZE_INLINE_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<2;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Size inline cases failed: "+std::to_string(failures));std::cout<<"PASS Size inline "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Size inline "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
