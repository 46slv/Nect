#include "window.hpp"
#include "semantic_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QMouseEvent>
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
const char* names[]={"grid-width","grid-width","grid-width","grid-width"};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Production control missing: "+name).toStdString());}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible(),"Exact production control visible");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
Document fixture(int mode){
 auto d=empty_document("grid-width-scrub-"+std::to_string(mode),"composition","board");auto& b=d.compositions.front().artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{Margin{10.123456789012345,20.234567890123456,30.345678901234567,40.456789012345678},Grid{"grid",{15.123456789012345,20.234567890123456,1000.1234567890123,800.23456789012345},3,2,7.345678901234567,8.456789012345678}};
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);d.compositions.front().roots={o.id};
 if(mode%4==1){d.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});Session seeded(d);seeded.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seeded.document();}
 auto& g=*d.compositions.front().artboards.front().layout->grid;
 if(mode==2)g.bounds_height_expression=Expression{"700",1};
 if(mode==3)g.bounds_width_expression=Expression{"900",1};
 valid(d);return d;
}
double base(const Document& d,int mode){const auto& source=d.compositions.front().artboards.front();const auto& target=d.compositions.front().artboards.back();const auto& g=mode%4==1&&target.template_assignment->grid_overridden?*target.layout->grid:*source.layout->grid;return g.bounds.width;}
Document wanted(const Document& initial,int mode,double delta){
 auto d=initial;
 if(mode%4==1){auto& b=d.compositions.front().artboards.back();if(!b.template_assignment->grid_overridden){auto grid=*initial.compositions.front().artboards.front().layout->grid;grid.id=b.template_assignment->grid_id;b.layout=ArtboardLayout{{},grid};}b.template_assignment->grid_overridden=true;}
 auto& b=mode%4==1?d.compositions.front().artboards.back():d.compositions.front().artboards.front();auto& g=*b.layout->grid;
 g.bounds.width=base(initial,mode)+delta;
 valid(d);return d;
}
std::vector<Command> command(const Document& d,int mode){if(mode%4==1)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.grid",d.compositions.front().artboards.back().layout->grid}}};return {SetArtboardLayout{"composition","board",d.compositions.front().artboards.front().layout}};}
void same(const Session& a,const Session& b){
 if(!(a.document()==b.document()&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
void open(Window& w,const Document& initial,int mode){w.host.session=Session(initial);w.host.edited();w.canvas->set_active_artboard("composition",mode%4==1?"target":"board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}
SemanticScrub* scrub(Window& w,int mode){auto* s=dynamic_cast<SemanticScrub*>(control<QToolButton>(w,QString(names[mode])+"-scrub"));check(s,"Existing SemanticScrub class reused");reveal(s);return s;}
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods=Qt::NoModifier){const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,mods);QApplication::sendEvent(c,&e);events();}
void press(QWidget* c){mouse(c,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);}
void move(QWidget* c,double x,Qt::KeyboardModifiers mods=Qt::NoModifier){mouse(c,QEvent::MouseMove,x,Qt::NoButton,Qt::LeftButton,mods);}
void release(QWidget* c,double x){mouse(c,QEvent::MouseButtonRelease,x,Qt::LeftButton,Qt::NoButton);}
QString prefix(const QString& root,int mode){return root+"/mode-"+QString::number(mode);}
void cold(const QString& root,int mode){const auto expected_doc=wanted(fixture(mode),mode,2.5);Session expected(expected_doc);const auto path=prefix(root,mode);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);w.host.open(path+"-saved.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&read(path+"-resaved.nect")==read(path+"-expected.nect"),"Separate cold save/resave exact native bytes");QTest::qWait(120);}
void flow(const QString& root,int mode){
 if(mode%4==3){
  const auto initial=fixture(mode);Session before(initial);const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",initial);
  QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,mode);same(w.host.session,before);auto* s=scrub(w,mode);auto* input=control<QLineEdit>(w,names[mode]);check(!s->isEnabled()&&input->isReadOnly(),"Driven Grid Width scrub/input disabled without unlink");check(w.grab().save(path+"-initial-window.png"),"Driven production render retained");press(s);move(s,14.5);release(s,14.5);same(w.host.session,before);return;
 }
 const auto initial=fixture(mode),half=wanted(initial,mode,0.5),final=wanted(initial,mode,2.5);Session before(initial),armed(initial),preview(initial),committed(initial),canceled(initial),zero(initial),foreign(initial),invalid(initial),keyboard(initial);
 armed.begin_gesture(0);preview=armed;preview.update_gesture(command(half,mode));committed=armed;committed.update_gesture(command(final,mode));committed.commit_gesture();check(committed.document()==final,"Independent full expected command agrees BEFORE Window");canceled=preview;canceled.cancel_gesture();zero=preview;zero.update_gesture({});zero.commit_gesture();
 auto other=initial;other.compositions.front().artboards.front().name="Foreign scrub pending";valid(other);foreign=preview;foreign.cancel_gesture();foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",other.compositions.front().artboards.front()}});
 invalid=armed;invalid.cancel_gesture();keyboard=Session(final);const auto next=wanted(final,mode,1);keyboard.begin_gesture(0);keyboard.update_gesture(command(next,mode));keyboard.commit_gesture();const auto typed=wanted(next,mode,0.25);Session direct(next);direct.begin_gesture(0);direct.update_gesture(command(typed,mode));direct.commit_gesture();
 const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",final);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,mode);same(w.host.session,before);auto* s=scrub(w,mode);auto* input=control<QLineEdit>(w,names[mode]);check(w.grab().save(path+"-initial-window.png"),"Real editable Window render retained");
 check(s->isEnabled(),"Literal/inherited Grid Width scrub enabled");
 if(mode%4==0){reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=");events();press(s);move(s,14.5);release(s,14.5);same(w.host.session,before);check(input->text()=="+=","Scrub refusal preserves incomplete numeric draft");w.host.session=Session(initial);w.host.session_id+="-draft";w.host.edited();events();s=scrub(w,mode);}
 // All independent expectations above precede this real production Window.
 press(s);same(w.host.session,armed);move(s,12.5);same(w.host.session,preview);QTest::keyClick(s,Qt::Key_Escape);events();same(w.host.session,canceled);
 w.host.session=Session(initial);w.host.session_id+="-deactivate";w.host.edited();events();s=scrub(w,mode);press(s);move(s,12.5);
 check(QMetaObject::invokeMethod(qApp,"applicationStateChanged",Qt::DirectConnection,Q_ARG(Qt::ApplicationState,Qt::ApplicationInactive)),"Owned Qt application-state cancellation signal delivered");events();same(w.host.session,canceled);
 // A fresh native Session keeps each distinct contract independent.
 w.host.session=Session(initial);w.host.session_id+="-zero";w.host.edited();events();s=scrub(w,mode);press(s);move(s,12.5);move(s,12);release(s,12);same(w.host.session,zero);
 w.host.session=Session(initial);w.host.session_id+="-foreign";w.host.edited();events();s=scrub(w,mode);press(s);move(s,12.5);w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",other.compositions.front().artboards.front()}});move(s,14.5);release(s,14.5);same(w.host.session,foreign);w.host.session.cancel_gesture();
 w.host.session=Session(initial);w.host.session_id+="-invalid";w.host.edited();events();s=scrub(w,mode);press(s);move(s,-1200);release(s,-1200);same(w.host.session,invalid);
 w.host.session=Session(initial);w.host.session_id+="-commit";w.host.edited();events();s=scrub(w,mode);press(s);move(s,12.5);same(w.host.session,preview);move(s,14.5);release(s,14.5);same(w.host.session,committed);check(w.host.session.document()==final,"Fractional repeated preview uses captured base and preserves full sibling/source state");
 s=scrub(w,mode);check(w.statusBar()->currentMessage().isEmpty(),"Successful scrub clears earlier range error");check(w.grab().save(path+"-committed-window.png"),"Committed production render retained");auto* group=control<QWidget>(w,"layout-grid");auto* bottom=control<QLineEdit>(w,"grid-row-gutter");const auto row_bottom=bottom->mapTo(group,QPoint(0,bottom->height())).y();check(group->grab(QRect(0,0,group->width(),row_bottom+6)).save(path+"-grid-rows.png"),"Editable Grid rows render retained");w.host.save(path+"-saved.nect");w.host.flush();same(w.host.session,committed);QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Cold acceptance passes");
 w.host.session.undo(w.host.session.revision());committed.undo(committed.revision());w.host.edited();events();same(w.host.session,committed);check(w.host.session.document()==initial,"One Undo restores full initial inheritance/source/IDs");w.host.session.redo(w.host.session.revision());committed.redo(committed.revision());w.host.edited();events();same(w.host.session,committed);
 // Reopen the committed document as a native baseline before keyboard increment.
 w.host.session=Session(final);w.host.session_id+="-keyboard";w.host.edited();events();s=scrub(w,mode);QTest::keyClick(s,Qt::Key_Right);events();same(w.host.session,keyboard);
 // Replace the absolute value atomically through the real native editor notification;
 // individual typed prefixes (1,10) are below this fixture's cell/gutter minimum.
 w.host.session=Session(next);w.host.session_id+="-direct";w.host.edited();events();input=control<QLineEdit>(w,names[mode]);reveal(input);input->setFocus();const auto literal=QString::number(base(typed,mode),'g',17);input->setText(literal);input->setModified(true);check(QMetaObject::invokeMethod(input,"textEdited",Qt::DirectConnection,Q_ARG(QString,literal)),"Native absolute replacement delivered");QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,direct);
}

}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";if(argc==4&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));else{QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GRID_WIDTH_SCRUB_ROOT"),root=requested.isEmpty()?temporary.path():requested;int failures=0;for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent scrub cases failed: "+std::to_string(failures));}std::cout<<"PASS Grid Width scrub "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Grid Width scrub "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
