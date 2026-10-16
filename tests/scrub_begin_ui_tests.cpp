#include "window.hpp"
#include "semantic_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QMouseEvent>
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
const char* names[]={"margin-left","margin-right","grid-x","grid-column-gutter"};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Production control missing: "+name).toStdString());}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible(),"Exact production control visible");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
Document fixture(int mode){
 auto d=empty_document("margin-scrub-"+std::to_string(mode),"composition","board");auto& b=d.compositions.front().artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{Margin{10.123456789012345,20.234567890123456,30.345678901234567,40.456789012345678},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);d.compositions.front().roots={o.id};
 if((mode%4==1||mode%4==3)){d.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});Session seeded(d);seeded.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seeded.document();}
 valid(d);return d;
}
void same(const Session& a,const Session& b){
 if(!(a.document()==b.document()&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
void open(Window& w,const Document& initial,int mode){w.host.session=Session(initial);w.host.edited();w.canvas->set_active_artboard("composition",(mode%4==1||mode%4==3)?"target":"board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods=Qt::NoModifier){const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,mods);QApplication::sendEvent(c,&e);events();}
void press(QWidget* c){mouse(c,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);}
void move(QWidget* c,double x,Qt::KeyboardModifiers mods=Qt::NoModifier){mouse(c,QEvent::MouseMove,x,Qt::NoButton,Qt::LeftButton,mods);}
void release(QWidget* c,double x){mouse(c,QEvent::MouseButtonRelease,x,Qt::LeftButton,Qt::NoButton);}

bool inherited(int mode){return mode%4==1||mode%4==3;}
Document goal(const Document& initial,int mode){
 auto d=initial;const int f=mode%4;const auto& source=initial.compositions.front().artboards.front();
 auto& b=inherited(mode)?d.compositions.front().artboards.back():d.compositions.front().artboards.front();
 if(inherited(mode)){
  b.layout=ArtboardLayout{};
  if(f==1){b.layout->margin=source.layout->margin;b.template_assignment->margin_overridden=true;}
  else{b.layout->grid=source.layout->grid;b.layout->grid->id=b.template_assignment->grid_id;b.template_assignment->grid_overridden=true;}
 }
 if(f==0)b.layout->margin->left+=1;else if(f==1)b.layout->margin->right+=1;
 else if(f==2)b.layout->grid->bounds.x+=1;else b.layout->grid->column_gutter+=1;
 valid(d);return d;
}
std::vector<Command> command(const Document& d,int mode){
 const auto& b=inherited(mode)?d.compositions.front().artboards.back():d.compositions.front().artboards.front();
 if(!inherited(mode))return {SetArtboardLayout{"composition","board",b.layout}};
 if(mode%4==1)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.margin",b.layout->margin}}};
 return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.grid",b.layout->grid}}};
}
SemanticScrub* scrub(Window& w,int mode){auto* s=dynamic_cast<SemanticScrub*>(control<QToolButton>(w,QString(names[mode%4])+"-scrub"));check(s,"Actual shared scrub control");reveal(s);return s;}
void attempt(SemanticScrub* s,int mode){
 if(mode<4){s->setFocus();QTest::keyClick(s,Qt::Key_Right);events();}
 else{press(s);move(s,13);release(s,13);}
}
void flow(const QString& root,int mode){
 const auto initial=fixture(mode),target=goal(initial,mode);
 auto pending=initial;pending.compositions.front().artboards.front().name="Foreign caller pending";valid(pending);
 const std::vector<Command> foreign_command{UpdateArtboard{"composition",pending.compositions.front().artboards.front()}};
 Session before(initial),foreign(initial),empty_foreign(initial),canceled(initial),recovery(initial),finished(initial);
 foreign.begin_gesture(0);foreign.update_gesture(foreign_command);
 check(foreign.preview_document()==pending&&foreign.document()==initial,"Independent foreign full preview BEFORE Window");
 auto attempted_begin=foreign;QString begin_code;try{attempted_begin.begin_gesture(0);}catch(const Error& e){begin_code=QString::fromStdString(e.code);}
 check(begin_code=="GESTURE_ACTIVE"&&attempted_begin.document()==foreign.document()&&attempted_begin.preview_document()==foreign.preview_document()&&attempted_begin.history()==foreign.history()&&attempted_begin.revision()==foreign.revision()&&attempted_begin.gesture_active()==foreign.gesture_active()&&attempted_begin.gesture_generation()==foreign.gesture_generation(),"Core check_revision independently refuses foreign begin BEFORE Window");
 empty_foreign.begin_gesture(0);canceled=foreign;canceled.cancel_gesture();
 recovery=canceled;recovery.begin_gesture(0);recovery.update_gesture(command(target,mode));recovery.commit_gesture();
 check(recovery.document()==target&&recovery.revision()==1,"Independent recovery native-valid full target/oneUndo BEFORE Window");
 auto undone=recovery;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 finished=foreign;finished.commit_gesture();check(finished.document()==pending,"Independent foreign owner commit BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial.nect",initial);write(path+"-pending.nect",pending);write(path+"-expected.nect",target);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);
 Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,mode);same(w.host.session,before);
 auto* s=scrub(w,mode);check(s->parentWidget()->grab().save(path+"-row.png"),"Actual same-row native render");
 w.host.session.begin_gesture(0);w.host.session.update_gesture(foreign_command);same(w.host.session,foreign);
 attempt(s,mode);
 std::cout<<"NEW_BEGIN mode="<<mode<<" document_same="<<(w.host.session.document()==foreign.document())<<" preview_same="<<(w.host.session.preview_document()==foreign.preview_document())<<" history_same="<<(w.host.session.history()==foreign.history())<<" active="<<w.host.session.gesture_active()<<" generation="<<w.host.session.gesture_generation()<<"/"<<foreign.gesture_generation()<<"\n";
 same(w.host.session,foreign);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Foreign caller begin rejected explicitly");
 QString save_code;try{w.host.save(path+"-foreign-saved.nect");}catch(const Error& e){save_code=QString::fromStdString(e.code);}
 check(save_code=="GESTURE_ACTIVE"&&!QFile::exists(path+"-foreign-saved.nect"),"Existing Host rejects Save during foreign preview, creates no native file");
 same(w.host.session,foreign);
 w.host.session.cancel_gesture();same(w.host.session,canceled);attempt(s,mode);same(w.host.session,recovery);
 check(w.statusBar()->currentMessage().isEmpty(),"Recovery after caller cancel clears refusal");
 w.host.save(path+"-recovered-saved.nect");w.host.flush();same(w.host.session,recovery);
 check(read(path+"-recovered-saved.nect")==read(path+"-expected.nect"),"Exact full inherited/native recovery bytes");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);
 w.host.session.redo(w.host.session.revision());same(w.host.session,redone);
 // Fresh own Window controls for two other new-begin states, not old proof replay.
 w.host.session=Session(initial);w.host.session_id+="-empty";w.host.edited();events();s=scrub(w,mode);
 w.host.session.begin_gesture(0);attempt(s,mode);same(w.host.session,empty_foreign);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Active empty foreign gesture preserved");
 w.host.session.cancel_gesture();w.host.session=Session(initial);w.host.session_id+="-commit";w.host.edited();events();s=scrub(w,mode);
 w.host.session.begin_gesture(0);w.host.session.update_gesture(foreign_command);attempt(s,mode);same(w.host.session,foreign);
 w.host.session.commit_gesture();same(w.host.session,finished);attempt(s,mode);same(w.host.session,finished);
 check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Foreign owner commit retains stale-row revision refusal");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{
 std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";
 QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_SCRUB_BEGIN_ROOT"),root=requested.isEmpty()?temporary.path():requested;
 int failures=0;for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
 if(failures)throw std::runtime_error("Independent new-begin failures: "+std::to_string(failures));
 std::cout<<"PASS Scrub begin "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL Scrub begin "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
