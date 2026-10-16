#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QFile>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
const char* guide_ids[]={"guide-x","guide-y"};
const char* sources[]={"10\n+ 0.5","20\n+ 0.5"};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(15);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Missing control: "+name).toStdString());}
QString name(int field,const char* suffix){
 if(QString(suffix)=="-expression")return "guide-position-expression-"+QString(guide_ids[field]);
 if(QString(suffix)=="-apply-expression")return "guide-position-apply-expression-"+QString(guide_ids[field]);
 return "guide-position-"+QString(guide_ids[field]);
}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible(),"Exact production control visible");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
Document fixture(int mode){
 auto d=empty_document("guide-inline-"+std::to_string(mode),"composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{Margin{10.123456789012345,20.234567890123456,30.345678901234567,40.456789012345678},Grid{"grid",{15.123456789012345,20.234567890123456,1000.1234567890123,800.23456789012345},3,2,7.345678901234567,8.456789012345678}};
 comp.guides={{"guide-x","Vertical","x",10.123456789012345},{"guide-y","Horizontal","y",20.234567890123456},{"peer","Untouched peer","x",30.345678901234567}};comp.guides[2].position_expression=Expression{"30 + 0.25",1};
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};valid(d);return d;
}
Document wanted(const Document& initial,int field){auto d=initial;d.compositions.front().guides[field].position_expression=Expression{sources[field],1};valid(d);return d;}
std::vector<Command> command(int field,bool=false){return {SetGuidePositionExpression{{guide_ids[field],"","guide.position"},{sources[field],1},false}};}
void same(const Session& a,const Session& b){if(!(a.document()==b.document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window expectation");}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
QString prefix(const QString& root,int mode){return root+"/mode-"+QString::number(mode);}
void open(Window& w,const Document& d){w.host.session=Session(d);w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}
void draft(Window& w,int field){auto* editor=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(editor);editor->setFocus();QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);const auto lines=QString(sources[field]).split('\n');QTest::keyClicks(editor,lines[0]);QTest::keyClick(editor,Qt::Key_Return);QTest::keyClicks(editor,lines[1]);events();check(editor->toPlainText()==sources[field],"Exact two-line source draft retained");}
void apply(Window& w,int field){auto* editor=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(editor);editor->setFocus();QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);events();}
void reset(Window& w,const Document& d,const QString& key){w.host.session=Session(d);w.host.session_id+=key;w.host.edited();events();}
void cold(const QString& root,int mode){const auto d=wanted(fixture(mode),mode);Session expected(d);const auto path=prefix(root,mode);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);w.host.open(path+"-cold-input.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);check(read(path+"-cold-input.nect")==read(path+"-expected.nect")&&read(path+"-resaved.nect")==read(path+"-expected.nect"),"Separate cold exact expression native bytes");QTest::qWait(120);}
void flow(const QString& root,int mode){
 const int field=mode;const auto initial=fixture(mode),intended=wanted(initial,field);Session before(initial),direct(initial),committed(initial);direct.apply(command(field),0);check(direct.document()==intended,"Independent full Guide source command expectation BEFORE Window");committed.begin_gesture(0);committed.update_gesture(command(field));committed.commit_gesture();check(committed.document()==intended&&committed.history()==direct.history()&&committed.revision()==direct.revision()&&!committed.gesture_active(),"Independent canonical Guide gesture lifecycle preserves direct command authoring BEFORE Window");
 const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",intended);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial);same(w.host.session,before);
 auto* editor=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(editor);editor->setFocus();QTest::keyClicks(editor,QString(sources[field]).split('\n').first());events();same(w.host.session,before);const auto one=editor->height();check(editor->document()->blockCount()==1,"Actual single-line Guide source input");check(editor->parentWidget()->grab().save(path+"-one-line.png"),"Actual Guide one-line source render retained");QTest::keyClick(editor,Qt::Key_Return);QTest::keyClicks(editor,QString(sources[field]).split('\n')[1]);events();same(w.host.session,before);check(editor->toPlainText()==sources[field]&&editor->document()->blockCount()==2,"Actual two-line Guide source remains unauthored");const auto two=editor->height();check(editor->parentWidget()->grab().save(path+"-two-line.png"),"Actual Guide two-line source render retained");std::cout<<"SIZE mode="<<mode<<" one="<<one<<" two="<<two<<" width="<<editor->width()<<"\n";check(two>one,"Two-line Guide source expands inline beyond one-line height");
 QTest::keyClick(editor,Qt::Key_End,Qt::ControlModifier);QTest::keyClick(editor,Qt::Key_Return);QTest::keyClick(editor,Qt::Key_Return);events();same(w.host.session,before);check(editor->document()->blockCount()==4&&editor->height()>two,"Four-line Guide draft expands further");check(editor->toPlainText()==QString(sources[field])+"\n\n","Guide expansion preserves exact source body");check(editor->parentWidget()->grab().save(path+"-four-line.png"),"Actual Guide four-line source render retained");draft(w,field);apply(w,field);same(w.host.session,committed);
 editor=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(editor);check(editor->toPlainText()==sources[field]&&editor->height()>one,"Committed Guide multiline source remains expanded");check(editor->parentWidget()->grab().save(path+"-committed.png"),"Actual committed Guide source/literal/evaluation render retained");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,committed);check(QFile::copy(path+"-saved.nect",path+"-cold-input.nect"),"Actual saved Guide source retained before later edits");QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Separate cold Guide source native acceptance passes");
 w.host.session.undo(w.host.session.revision());committed.undo(committed.revision());w.host.edited();events();same(w.host.session,committed);editor=control<QPlainTextEdit>(w,name(field,"-expression"));check(w.host.session.document()==initial&&editor->toPlainText().isEmpty()&&editor->height()==one,"One Undo restores full initial Guide IDs/siblings/source and compact empty field");w.host.session.redo(w.host.session.revision());committed.redo(committed.revision());w.host.edited();events();same(w.host.session,committed);check(control<QPlainTextEdit>(w,name(field,"-expression"))->height()>one,"Redo restores committed Guide multiline source expansion");
}

}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";if(argc==4&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));else{QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GUIDE_INLINE_EXPANSION_ROOT"),root=requested.isEmpty()?temporary.path():requested;int failures=0;for(int mode=0;mode<2;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent expression key cases failed: "+std::to_string(failures));}std::cout<<"PASS Guide inline expansion "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Guide inline expansion "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
