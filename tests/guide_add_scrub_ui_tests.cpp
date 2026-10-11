#include "window.hpp"
#include "semantic_control.hpp"
#include <QGroupBox>
#include <QMouseEvent>
#include "nect/io.hpp"
#include <QApplication>
#include <array>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPointer>
#include <QRegularExpression>
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
const char* sources[]={"10 + 0.5","20 + 0.5"};
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
 auto d=empty_document("guide-prefix-"+std::to_string(mode),"composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{Margin{10.123456789012345,20.234567890123456,30.345678901234567,40.456789012345678},Grid{"grid",{15.123456789012345,20.234567890123456,1000.1234567890123,800.23456789012345},3,2,7.345678901234567,8.456789012345678}};
 comp.guides={{"guide-x","Vertical","x",10.123456789012345},{"guide-y","Horizontal","y",20.234567890123456},{"peer","Untouched peer","x",30.345678901234567}};comp.guides[2].position_expression=Expression{"30 + 0.25",1};comp.guides[0].position_expression=Expression{"42",1};comp.guides[1].position_expression=Expression{"42",1};
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};valid(d);return d;
}
void same(const Session& a,const Session& b){if(!(a.document()==b.document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window expectation");}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
QString prefix(const QString& root,int mode){return root+"/mode-"+QString::number(mode);}
void open(Window& w,const Document& d){w.host.session=Session(d);w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}
void reset(Window& w,const Document& d,const QString& key){w.host.session=Session(d);w.host.session_id+=key;w.host.edited();events();}



const Id fresh_slot="00000000-0000-0000-0000-000000000000";
double amount(int mode){return mode==0?1:mode==1?-1:mode==2?5:-5;}
Document add_goal(const Document& initial,double value,const char* name="Guide"){
 auto d=initial;d.compositions.front().guides.push_back({fresh_slot,name,"x",value});valid(d);return d;
}
std::vector<Command> add_command(const Document& goal){return {AddGuide{"composition",goal.compositions.front().guides.back()}};}
Session add_preview(const Document& initial,const Document& goal){Session s(initial);s.begin_gesture(0);s.update_gesture(add_command(goal));return s;}
Document bind_slot(Document d,const Id& id){d.compositions.front().guides.back().id=id;return d;}
void same_add(const Session& actual,const Session& expected,const Id& generated){
 const auto normalize=[&](Document d){auto& gs=d.compositions.front().guides;for(auto& g:gs)if(g.id==generated)g.id=fresh_slot;return d;};
 const auto a=normalize(actual.document()),p=normalize(actual.preview_document());
 check(a==expected.document()&&encode(a)==encode(expected.document())&&p==expected.preview_document()&&actual.history()==expected.history()&&actual.revision()==expected.revision()&&actual.gesture_active()==expected.gesture_active()&&actual.gesture_generation()==expected.gesture_generation(),"Full independent Add Session equals pre-Window oracle with only one fresh UUID slot bound");
}
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons){
 const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,Qt::NoModifier);QApplication::sendEvent(c,&e);events();
}

Id generated(const Session& actual,const Document& initial){
 const auto& gs=actual.preview_document().compositions.front().guides;
 check(gs.size()==initial.compositions.front().guides.size()+1,"Exactly one new preview/authored Guide");
 const auto id=gs.back().id;
 check(id!=fresh_slot&&QRegularExpression("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$").match(QString::fromStdString(id)).hasMatch(),"Only fresh real generated UUID slot");
 for(const auto& g:initial.compositions.front().guides)check(g.id!=id,"No stable existing Guide identity reuse");
 return id;
}
SemanticScrub* control_scrub(Window& w){auto* s=dynamic_cast<SemanticScrub*>(control<QToolButton>(w,"guide-new-position-scrub"));check(s,"Actual shared Add Guide scrub control");reveal(s);return s;}
void key(SemanticScrub* s,bool left=false){s->setFocus();QTest::keyClick(s,left?Qt::Key_Left:Qt::Key_Right);events();}
void press(SemanticScrub* s){mouse(s,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);}
void move(SemanticScrub* s,double delta){mouse(s,QEvent::MouseMove,8+delta,Qt::NoButton,Qt::LeftButton);}
void release(SemanticScrub* s,double delta){mouse(s,QEvent::MouseButtonRelease,8+delta,Qt::LeftButton,Qt::NoButton);}
void cold(const QString& root,int mode,const Id& id){
 const auto d=bind_slot(add_goal(fixture(mode),amount(mode)),id);valid(d);Session expected(d);const auto path=prefix(root,mode);
 QSettings settings(root+"/cold-settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);
 w.host.open(path+"-cold-input.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);
 check(read(path+"-cold-input.nect")==read(path+"-bound-expected.nect")&&read(path+"-resaved.nect")==read(path+"-bound-expected.nect"),"Separate cold exact new Guide native bytes with bound UUID only");QTest::qWait(120);
}
void flow(const QString& root,int mode){
 const auto initial=fixture(mode),target=add_goal(initial,amount(mode)),partial_goal=add_goal(initial,mode==3?-2:2);
 Session before(initial),partial=add_preview(initial,partial_goal),final_preview=add_preview(initial,target),committed=final_preview;
 committed.commit_gesture();check(committed.document()==target,"Independent native-valid exact creation command BEFORE Window");
 auto cancelled=partial;cancelled.cancel_gesture();Session zero(initial);zero.begin_gesture(0);zero.update_gesture({});zero.commit_gesture();
 auto undone=committed;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 const auto pending_number=add_preview(initial,add_goal(initial,1));
 const auto pending_name=add_preview(initial,add_goal(initial,0,"Draft"));
 auto axis_goal=add_goal(initial,0);axis_goal.compositions.front().guides.back().axis="y";valid(axis_goal);const auto pending_axis=add_preview(initial,axis_goal);
 auto external=initial.compositions.front().artboards.front();external.name="Foreign caller";
 auto foreign_doc=initial;foreign_doc.compositions.front().artboards.front().name=external.name;valid(foreign_doc);
 Session foreign(initial);foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",external}});
 check(foreign.preview_document()==foreign_doc,"Independent full foreign Add scrub BEFORE Window");
 auto replaced_doc=initial;replaced_doc.id+="-replacement";valid(replaced_doc);Session replaced(replaced_doc),newer(initial);newer.apply({UpdateArtboard{"composition",external}},0);
 auto replacement_generation=partial;replacement_generation.cancel_gesture();replacement_generation.begin_gesture(0);replacement_generation.update_gesture({UpdateArtboard{"composition",external}});
 Session range(initial);range.begin_gesture(0);QString range_code;auto outside=target.compositions.front().guides.back();outside.position=1e9+1;
 try{range.update_gesture({AddGuide{"composition",outside}});}catch(const Error& e){range_code=QString::fromStdString(e.code);}
 range.cancel_gesture();check(!range_code.isEmpty(),"Independent Add Guide core range refusal BEFORE Window");
 auto recovery=range;const auto recovered_goal=add_goal(initial,-1);recovery.begin_gesture(0);recovery.update_gesture(add_command(recovered_goal));recovery.commit_gesture();
 const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected-slot.nect",target);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial);same(w.host.session,before);
 auto* number=control<QLineEdit>(w,"guide-new-position");reveal(number);auto* row=control<QGroupBox>(w,"guide-add-row");auto* raw=w.findChild<QToolButton*>("guide-new-position-scrub");
 std::cout<<"ADD_SCRUB_CAPABILITY mode="<<mode<<" visible_input="<<number->isVisible()<<" tools_in_row="<<row->findChildren<QToolButton*>().size()<<" shared_scrub="<<(raw!=nullptr)<<"\n";
 same(w.host.session,before);check(raw&&raw->isVisible()&&dynamic_cast<SemanticScrub*>(raw),"Actual Add Guide continuous position has shared same-row scrub");
 auto* s=control_scrub(w);check(s->parentWidget()->grab().save(path+"-row.png"),"Actual Add numeric/shared scrub crop");
 Id id;
 if(mode<2){key(s,mode==1);id=generated(w.host.session,initial);}
 else{press(s);move(s,mode==3?-2:2);id=generated(w.host.session,initial);same_add(w.host.session,partial,id);move(s,amount(mode));same_add(w.host.session,final_preview,id);release(s,amount(mode));}
 same_add(w.host.session,committed,id);
 const auto bound=bind_slot(target,id);write(path+"-bound-expected.nect",bound);w.host.save(path+"-saved.nect");w.host.flush();same_add(w.host.session,committed,id);
 check(QFile::copy(path+"-saved.nect",path+"-cold-input.nect"),"Immutable actual Add scrub cold bytes");
 QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode),QString::fromStdString(id)});
 check(child.waitForStarted(5000),"Owned Add scrub cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Cold deadline");}
 std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Actual cold acceptance");
 w.host.session.undo(w.host.session.revision());w.host.edited();events();same_add(w.host.session,undone,id);check(w.host.session.document()==initial,"One Undo removes exactly new Guide/all original state");
 w.host.session.redo(w.host.session.revision());w.host.edited();events();same_add(w.host.session,redone,id);
 reset(w,initial,"-cancel");s=control_scrub(w);press(s);move(s,mode==3?-2:2);id=generated(w.host.session,initial);same_add(w.host.session,partial,id);key(s); // Right while dragging does not create another command or commit.
 same_add(w.host.session,partial,id);QTest::keyClick(s,Qt::Key_Escape);events();same(w.host.session,cancelled);
 reset(w,initial,"-zero");s=control_scrub(w);press(s);move(s,2);id=generated(w.host.session,initial);move(s,0);release(s,0);same(w.host.session,zero);
 reset(w,initial,"-pending-number");number=control<QLineEdit>(w,"guide-new-position");reveal(number);number->setFocus();QTest::keyClick(number,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(number,"1 +");events();
 id=generated(w.host.session,initial);same_add(w.host.session,pending_number,id);key(control_scrub(w));same_add(w.host.session,pending_number,id);check(number->text()=="1 +"&&w.statusBar()->currentMessage().contains("UNCOMMITTED_INPUT"),"Pending numeric exact draft/caller preserved");
 reset(w,initial,"-pending-name");auto* name=control<QLineEdit>(w,"guide-new-name");reveal(name);name->setFocus();QTest::keyClick(name,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(name,"Draft");events();
 id=generated(w.host.session,initial);same_add(w.host.session,pending_name,id);key(control_scrub(w));same_add(w.host.session,pending_name,id);
 check(w.statusBar()->currentMessage().contains("UNCOMMITTED_INPUT"),"Pending creation name not committed by scrub");
 reset(w,initial,"-pending-axis");auto* axis=control<QComboBox>(w,"guide-new-axis");reveal(axis);axis->setFocus();QTest::keyClick(axis,Qt::Key_Down);events();
 id=generated(w.host.session,initial);same_add(w.host.session,pending_axis,id);key(control_scrub(w));same_add(w.host.session,pending_axis,id);
 check(axis->currentData().toString()=="y"&&w.statusBar()->currentMessage().contains("UNCOMMITTED_INPUT"),"Pending horizontal creation axis not committed by scrub");
 reset(w,initial,"-foreign");w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});key(control_scrub(w));same(w.host.session,foreign);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Foreign begin unchanged");
 reset(w,initial,"-session");s=control_scrub(w);w.host.session_id+="-other";key(s);same(w.host.session,before);
 reset(w,initial,"-document");s=control_scrub(w);w.host.session=Session(replaced_doc);key(s);same(w.host.session,replaced);
 reset(w,initial,"-revision");s=control_scrub(w);w.host.session.apply({UpdateArtboard{"composition",external}},0);key(s);same(w.host.session,newer);
 reset(w,initial,"-generation");s=control_scrub(w);press(s);move(s,mode==3?-2:2);w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});release(s,2);same(w.host.session,replacement_generation);
 reset(w,initial,"-range");s=control_scrub(w);press(s);move(s,1e9+1);release(s,1e9+1);same(w.host.session,range);check(w.statusBar()->currentMessage().contains(range_code),"Existing Add Guide range error retained");
 key(s,true);id=generated(w.host.session,initial);same_add(w.host.session,recovery,id);check(w.statusBar()->currentMessage().isEmpty(),"Same-control recovery clears range error");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{
 std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";
 if(argc==5&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]),argv[4]);else{
 QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GUIDE_ADD_SCRUB_ROOT"),root=requested.isEmpty()?temporary.path():requested;
 int failures=0;for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
 if(failures)throw std::runtime_error("Independent Add scrub failures: "+std::to_string(failures));}
 std::cout<<"PASS Guide Add scrub "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL Guide Add scrub "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
