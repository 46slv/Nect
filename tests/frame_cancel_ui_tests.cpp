#include "window.hpp"
#include "semantic_control.hpp"
#include <QMouseEvent>
#include <QToolButton>
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

double Artboard::* const members[]={&Artboard::x,&Artboard::y,&Artboard::width,&Artboard::height};
const char* fields[]={"x","y","width","height"};
Document fixture(bool inherited,int driven=-1){
 auto d=empty_document("frame-relative","composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.x=32.123456789012345;b.y=64.234567890123456;b.width=1400.345678901234567;b.height=1000.456789012345678;b.layout=ArtboardLayout{Margin{10,20,30,40},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 comp.artboards.push_back({"parent","Parent",3500,0,1500,1100});Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};
 if(inherited){comp.artboards.push_back({"target","Target",2000,0,640,480});Session seed(d);seed.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seed.document();}
 if(driven==2)d.compositions.front().artboards.front().width_driver=ArtboardSizeDriver{Ref{"parent","","artboard.width"}};if(driven==3)d.compositions.front().artboards.front().height_driver=ArtboardSizeDriver{Ref{"parent","","artboard.height"}};valid(d);return d;
}

Document goal(const Document& initial,int f,bool inherited,double value){
 auto d=initial;auto& b=inherited?d.compositions.front().artboards.back():d.compositions.front().artboards.front();
 if(inherited){if(f==2)b.template_assignment->width_override=value;else b.template_assignment->height_override=value;}else b.*members[f]=value;valid(d);return d;
}
std::vector<Command> command(const Document& d,int f,bool inherited){const auto& b=inherited?d.compositions.front().artboards.back():d.compositions.front().artboards.front();if(inherited)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition",b.id,std::string("frame.")+fields[f],f==2?*b.template_assignment->width_override:*b.template_assignment->height_override}}};return {UpdateArtboard{"composition",b}};}
Session preview(const Document& initial,const Document& intended,int f,bool inherited){Session s(initial);s.begin_gesture(0);s.update_gesture(command(intended,f,inherited));check(s.document()==initial&&s.preview_document()==intended,"Independent full authored/preview Frame snapshot BEFORE Window");return s;}
void ready(Window& w,const std::string& id){w.host.edited();w.canvas->set_active_artboard("composition",id,false);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();bool fields_visible=false;for(auto* c:w.findChildren<QLineEdit*>("artboard-x"))fields_visible|=c->isVisible();if(!fields_visible){auto* e=control<QPushButton>(w,"artboard-edit");reveal(e);e->click();events();}}
void reset(Window& w,const Document& d,const std::string& id,const char* tag){w.host.session=Session(d);w.host.session_id+=tag;ready(w,id);}
SemanticScrub* scrub(Window& w,int f){auto* c=control<QToolButton>(w,QString("artboard-")+fields[f]+"-scrub");check(dynamic_cast<SemanticScrub*>(c),"Exact production shared SemanticScrub");return static_cast<SemanticScrub*>(c);}
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods=Qt::NoModifier){const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,mods);QApplication::sendEvent(c,&e);events();}
void key(Window& w,int f,Qt::Key k=Qt::Key_Right){auto* c=scrub(w,f);reveal(c);c->setFocus();QTest::keyClick(c,k);events();}

void flow(const QString& root,int mode){
 const bool scrub_recovery=mode>=20,boundary_mode=mode==14||mode==15||scrub_recovery;
 const bool own=mode<8||boundary_mode,source_mode=mode==6||mode==7;
 const bool inherited=mode==4||mode==5||mode==15||mode==21;const int f=mode<4?mode:mode==4||mode==5?mode-2:mode%2+2;
 const std::string id=inherited?"target":"board";auto initial=fixture(inherited);if(boundary_mode){initial=goal(initial,f,inherited,1e7);}
 Session base(initial);const auto start=evaluate_artboard(initial.compositions.front(),id).*members[f];
 const auto partial_doc=goal(initial,f,inherited,boundary_mode?start-2:start+2);auto partial=preview(initial,partial_doc,f,inherited),cancelled=partial;cancelled.cancel_gesture();
 const auto body=QString::fromStdString("  ref(\"parent\",\"\",\"artboard."+std::string(fields[f])+"\") + 1  ");
 auto intended=goal(initial,f,inherited,boundary_mode?start-1:start+3);
 if(source_mode){intended=initial;auto& b=intended.compositions.front().artboards.front();(f==2?b.width_driver:b.height_driver)=ArtboardSizeDriver{Expression{body.toStdString(),1}};valid(intended);}
 auto after=cancelled;if(own){if(scrub_recovery){after.begin_gesture(0);after.update_gesture(command(intended,f,inherited));after.commit_gesture();}else if(source_mode)after.apply({SetArtboardSizeExpression{{id,"",std::string("artboard.")+fields[f]},{body.toStdString(),1},false}},0);else after.apply(command(intended,f,inherited),0);check(after.document()==intended,"Independent full post-own-cancel intended/one History BEFORE Window");}
 auto undone=after,redone=after;if(own){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());}
 auto caller_doc=initial;caller_doc.compositions.front().artboards.front().name="Caller";valid(caller_doc);auto caller=caller_doc.compositions.front().artboards.front();
 auto foreign=base;foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",caller}});check(foreign.document()==initial&&foreign.preview_document()==caller_doc,"Independent foreign caller BEFORE Window");auto foreign_cancelled=foreign;foreign_cancelled.cancel_gesture();
 auto next_foreign=cancelled;next_foreign.begin_gesture(0);next_foreign.update_gesture({UpdateArtboard{"composition",caller}});
 auto replacement=initial;replacement.id+="-replacement";replacement.objects.at("sentinel").contours.front().points.front().x.literal+=7;valid(replacement);Session replaced(replacement),advanced(initial);advanced.apply({UpdateArtboard{"composition",caller}},0);check(advanced.document()==caller_doc,"Independent caller revision BEFORE Window");
 auto failed=base;QString range_code;if(boundary_mode){failed.begin_gesture(0);try{if(inherited)failed.update_gesture({ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition",id,std::string("frame.")+fields[f],start+1}}});else {auto b=initial.compositions.front().artboards.front();b.*members[f]=start+1;failed.update_gesture({UpdateArtboard{"composition",b}});}}catch(const Error& e){range_code=QString::fromStdString(e.code);}failed.cancel_gesture();check(!range_code.isEmpty()&&failed.history()==base.history(),"Independent range refusal and idle cancelled snapshot BEFORE Window");same(failed,cancelled);}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);write(path+"-caller-expected.nect",caller_doc);write(path+"-replacement-expected.nect",replacement);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.resize(1400,1000);w.show();w.activateWindow();ready(w,id);same(w.host.session,base);
 const auto name=QString("artboard-")+fields[f],editor_name=name+"-expression";const auto peer_name=QString("artboard-")+(f==3?"width":"height")+"-expression";auto* original=control<QLineEdit>(w,name);const QPointer<QLineEdit> old_input=original;
 if(own||mode>=16){control<QPlainTextEdit>(w,peer_name)->setPlainText("ref(");auto* c=scrub(w,f);reveal(c);
  if(boundary_mode){c->setFocus();QTest::keyClick(c,Qt::Key_Right);events();same(w.host.session,failed);check(w.statusBar()->currentMessage().contains(range_code),"Actual owned range refusal");}
  else {mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);c->setFocus();QTest::keyClick(c,Qt::Key_Escape);
   if(mode==16)w.host.session=next_foreign;else if(mode==17)w.host.session=replaced;else if(mode==18)w.host.session=advanced;else if(mode==19)w.host.session_id+="-other";
   events();same(w.host.session,mode==16?next_foreign:mode==17?replaced:mode==18?advanced:cancelled);
  }
 }else if(mode==8||mode==9){w.host.session=foreign_cancelled;}else if(mode==10||mode==11){w.host.session=foreign;}else if(mode==12||mode==13){w.host.session=foreign;w.host.edited();events();w.host.session.cancel_gesture();same(w.host.session,foreign_cancelled);}
 if(own){same(w.host.session,cancelled);check(control<QPlainTextEdit>(w,peer_name)->toPlainText()=="ref(","Own cancellation retains independent incomplete source draft");}
 auto* input=control<QLineEdit>(w,name);reveal(input);if(scrub_recovery)key(w,f,Qt::Key_Left);else {input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,source_mode||!own?"="+body:QString::number(boundary_mode?start-1:start+3,'g',17));QTest::keyClick(input,Qt::Key_Return);events();}
 std::cout<<"FRAME_CANCEL mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<" fresh_input="<<(!old_input||old_input!=control<QLineEdit>(w,name))<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual continuation status");
 if(!own){const auto& expected=mode==10||mode==11?foreign:mode==16?next_foreign:mode==17?replaced:mode==18?advanced:mode>=19?cancelled:foreign_cancelled;same(w.host.session,expected);check(w.statusBar()->currentMessage().contains("CONFLICT")||w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Foreign/preview-born/queued stale-context numeric source guard retained");check(control<QPlainTextEdit>(w,editor_name)->toPlainText().isEmpty(),"Guard refuses old numeric text instead of handing to new source");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.close();events();return;}
 if(source_mode){same(w.host.session,cancelled);check(control<QPlainTextEdit>(w,editor_name)->toPlainText()==body,"Own cancelled scrub allows exact source handoff without authoring");w.host.save(path+"-initial-saved.nect");w.host.flush();events();same(w.host.session,cancelled);check(read(path+"-initial-saved.nect")==read(path+"-initial-expected.nect")&&QFile::copy(path+"-initial-saved.nect",path+"-initial-snapshot.nect"),"Private post-cancel source excluded from native immutable bytes");auto* e=control<QPlainTextEdit>(w,editor_name);reveal(e);e->setFocus();QTest::keyClick(e,Qt::Key_Return,Qt::ControlModifier);events();}
 same(w.host.session,after);check(w.statusBar()->currentMessage().isEmpty(),"Current post-own-cancel valid input retires stale error");check(!old_input||old_input!=control<QLineEdit>(w,name),"Owned idle context refresh exposes fresh original-guarded field");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,after);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Exact full post-cancel authored native snapshot");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_FRAME_CANCEL_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<22;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Frame cancelled continuation cases failed: "+std::to_string(failures));std::cout<<"PASS Frame cancel continuation "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Frame cancel continuation "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
