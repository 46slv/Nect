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
 const bool inherited=mode==4||mode==5||mode==8||mode==9,readonly=mode>=6;const int f=mode<4?mode:mode%2+2;auto initial=fixture(inherited,mode==6||mode==7?f:-1);const std::string id=inherited?"target":"board";
 if(mode==8||mode==9){initial.compositions.front().artboards.back().parent_size=ArtboardParent{"parent",f==2,f==3};valid(initial);}
 Session base(initial);const auto start=readonly?(f==2?1500.0:1100.0):(initial.compositions.front().artboards.front().*members[f]);check(evaluate_artboard(initial.compositions.front(),id).*members[f]==start,"Independent fixture exact resolved start BEFORE Window");const auto partial_doc=goal(initial,f,inherited,start+2),target=goal(initial,f,inherited,start+5);
 auto partial=readonly?base:preview(initial,partial_doc,f,inherited),final_preview=readonly?base:preview(initial,target,f,inherited),committed=final_preview,cancelled=partial,zero=base;
 if(!readonly){committed.commit_gesture();cancelled.cancel_gesture();zero.begin_gesture(0);zero.update_gesture({});zero.commit_gesture();check(committed.document()==target&&committed.revision()==1&&cancelled.document()==initial&&zero.history()==base.history(),"Independent one History/cancel/no-op BEFORE Window");}
 auto undone=committed,redone=committed;if(!readonly){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());}
 const auto step_doc=goal(initial,f,inherited,start+1);auto step=readonly?base:preview(initial,step_doc,f,inherited);if(!readonly)step.commit_gesture();
 auto replacement=initial;replacement.id+="-replacement";replacement.objects.at("sentinel").contours.front().points.front().x.literal+=7;valid(replacement);Session replaced(replacement);
 auto caller_doc=initial;caller_doc.compositions.front().artboards.front().name="Caller";valid(caller_doc);const auto caller=caller_doc.compositions.front().artboards.front();Session advanced(initial);advanced.apply({UpdateArtboard{"composition",caller}},0);check(advanced.document()==caller_doc,"Independent caller full authored snapshot BEFORE Window");
 auto foreign=base;foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",caller}});check(foreign.preview_document()==caller_doc&&foreign.document()==initial,"Independent foreign caller BEFORE Window");auto foreign_cancelled=foreign;foreign_cancelled.cancel_gesture();
 auto next_generation=partial;if(!readonly){next_generation.cancel_gesture();next_generation.begin_gesture(0);next_generation.update_gesture({UpdateArtboard{"composition",caller}});}
 const auto boundary=goal(initial,f,inherited,f<2?1e9:1e7);Session range(boundary);range.begin_gesture(0);QString range_code;if(!readonly){const auto& b=inherited?boundary.compositions.front().artboards.back():boundary.compositions.front().artboards.front();auto outside=b;outside.*members[f]=(f<2?1e9:1e7)+1;try{if(inherited)range.update_gesture({ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition",id,std::string("frame.")+fields[f],1e7+1}}});else range.update_gesture({UpdateArtboard{"composition",outside}});}catch(const Error& e){range_code=QString::fromStdString(e.code);}check(!range_code.isEmpty()&&range.document()==boundary,"Independent finite Frame boundary refusal BEFORE Window");}range.cancel_gesture();
 const auto recovery_doc=goal(boundary,f,inherited,(f<2?1e9:1e7)-1);auto recovered=range;if(!readonly){recovered.begin_gesture(0);recovered.update_gesture(command(recovery_doc,f,inherited));recovered.commit_gesture();check(recovered.document()==recovery_doc,"Independent boundary recovery BEFORE Window");}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",readonly?initial:target);write(path+"-replacement-expected.nect",replacement);write(path+"-caller-expected.nect",caller_doc);write(path+"-boundary-expected.nect",boundary);write(path+"-recovery-expected.nect",recovery_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.resize(1400,1000);w.show();w.activateWindow();ready(w,id);same(w.host.session,base);auto* input=control<QLineEdit>(w,QString("artboard-")+fields[f]);reveal(input);
 auto* c=w.findChild<QToolButton*>(QString("artboard-")+fields[f]+"-scrub");std::cout<<"FRAME_SCRUB mode="<<mode<<" present="<<(c!=nullptr)<<" readonly="<<input->isReadOnly()<<"\n";check(c&&c->isVisible()&&dynamic_cast<SemanticScrub*>(c),"Frame continuous numeric row exposes existing compact SemanticScrub");check(c->width()==24&&c->height()==24&&c->isEnabled()==!readonly,"Exact compact source-protected capability");if(!readonly)reveal(c);check(c->parentWidget()->grab().save(path+"-row.png"),"Actual native Frame row render");
 if(readonly){QTest::keyClick(c,Qt::Key_Right);events();same(w.host.session,base);w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Actual disabled Frame scrub exact original source native");w.close();events();same(w.host.session,base);return;}
 auto source_name=QString("artboard-height-expression");control<QPlainTextEdit>(w,source_name)->setPlainText("ref(");same(w.host.session,base);
 mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);mouse(c,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);same(w.host.session,final_preview);mouse(c,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);same(w.host.session,committed);check(control<QPlainTextEdit>(w,source_name)->toPlainText()=="ref(","Scrub commit retains separate incomplete source draft");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,committed);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Full intended native bytes exclude private source");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.save(path+"-initial-saved.nect");w.host.flush();events();same(w.host.session,undone);check(read(path+"-initial-saved.nect")==read(path+"-initial-expected.nect"),"One scrub Undo restores complete authored native source/fallback");check(QFile::copy(path+"-initial-saved.nect",path+"-undo-snapshot.nect")&&read(path+"-undo-snapshot.nect")==read(path+"-initial-expected.nect"),"Retain exact actual Undo saved bytes before later continuous-save revisions");w.host.session.redo(w.host.session.revision());same(w.host.session,redone);
 reset(w,initial,id,"-cancel");c=scrub(w,f);reveal(c);mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);c->setFocus();QTest::keyClick(c,Qt::Key_Escape);events();same(w.host.session,cancelled);
 reset(w,initial,id,"-zero");c=scrub(w,f);reveal(c);mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);mouse(c,QEvent::MouseMove,8,Qt::NoButton,Qt::LeftButton);mouse(c,QEvent::MouseButtonRelease,8,Qt::LeftButton,Qt::NoButton);same(w.host.session,zero);check(w.host.session.document()==initial,"Reverse-to-zero keeps inheritance/no implicit override");
 reset(w,initial,id,"-keyboard");key(w,f);same(w.host.session,step);
 reset(w,initial,id,"-pending");input=control<QLineEdit>(w,QString("artboard-")+fields[f]);reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"1 +");QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,base);key(w,f);same(w.host.session,base);check(w.statusBar()->currentMessage().contains("UNCOMMITTED_INPUT")&&control<QLineEdit>(w,QString("artboard-")+fields[f])->text()=="1 +","Finished invalid numeric draft survives rejected scrub; no zero");
 reset(w,initial,id,"-session");w.host.session_id+="-other";key(w,f);same(w.host.session,base);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Original Session scrub refusal");
 reset(w,initial,id,"-document");w.host.session=replaced;key(w,f);same(w.host.session,replaced);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Original Document scrub refusal");
 reset(w,initial,id,"-revision");w.host.session=advanced;key(w,f);same(w.host.session,advanced);check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Original revision scrub refusal");
 reset(w,initial,id,"-foreign");w.host.session=foreign;key(w,f);same(w.host.session,foreign);check(w.statusBar()->currentMessage().contains("CONFLICT")||w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Foreign active caller remains untouched");w.host.session.cancel_gesture();same(w.host.session,foreign_cancelled);
 reset(w,initial,id,"-born");w.host.session=foreign;ready(w,id);w.host.session.cancel_gesture();key(w,f);same(w.host.session,foreign_cancelled);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Preview-born scrub ineligible after caller cancel");
 reset(w,initial,id,"-generation");c=scrub(w,f);reveal(c);mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",caller}});mouse(c,QEvent::MouseButtonRelease,10,Qt::LeftButton,Qt::NoButton);same(w.host.session,next_generation);w.host.session.cancel_gesture();
 reset(w,initial,id,"-refresh");c=scrub(w,f);reveal(c);mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);w.host.edited();events();same(w.host.session,cancelled);
 reset(w,boundary,id,"-range");key(w,f);same(w.host.session,range);check(w.statusBar()->currentMessage().contains(range_code),"Canonical finite Frame range error; owned preview cancelled");key(w,f,Qt::Key_Left);same(w.host.session,recovered);check(w.statusBar()->currentMessage().isEmpty(),"Same control recovers after own cancelled failure");w.close();events();same(w.host.session,recovered);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_FRAME_SCRUB_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<10;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Frame scrub cases failed: "+std::to_string(failures));std::cout<<"PASS Frame scrub "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Frame scrub "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
