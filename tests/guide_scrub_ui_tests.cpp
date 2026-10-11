#include "window.hpp"
#include "semantic_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <array>
#include <QLineEdit>
#include <QToolButton>
#include <QGroupBox>
#include <QMouseEvent>
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
 comp.guides={{"guide-x","Vertical","x",10.123456789012345},{"guide-y","Horizontal","y",20.234567890123456},{"peer","Untouched peer","x",30.345678901234567}};comp.guides[2].position_driver=Ref{"guide-x","","guide.position"};Guide py{"peer-y","Untouched y peer","y",40.456789012345678};py.position_driver=Ref{"guide-y","","guide.position"};comp.guides.push_back(py);
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};valid(d);return d;
}
Document wanted(const Document& initial,int field){auto d=initial;d.compositions.front().guides[field].position_expression=Expression{sources[field],1};valid(d);return d;}
std::vector<Command> command(int field,const char* body=nullptr){return {SetGuidePositionExpression{{guide_ids[field],"","guide.position"},{body?body:sources[field],1},false}};}
Session gesture_commit(const Document& d,int field){Session expected(d);expected.begin_gesture(0);expected.update_gesture(command(field));expected.commit_gesture();return expected;}
void same(const Session& a,const Session& b){if(!(a.document()==b.document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window expectation");}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
QString prefix(const QString& root,int mode){return root+"/mode-"+QString::number(mode);}
void open(Window& w,const Document& d){w.host.session=Session(d);w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}
void enter_prefix(Window& w,int field,const QString& text){auto* input=control<QLineEdit>(w,"guide-position-"+QString(guide_ids[field]));reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);events();QTest::keyClick(input,Qt::Key_Return);events();}
void enter_numeric(Window& w,int field,const QString& text,bool commit){auto* input=control<QLineEdit>(w,"guide-position-"+QString(guide_ids[field]));reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);events();if(commit){QTest::keyClick(input,Qt::Key_Return);events();}}
void cancel(Window& w,int field){auto* editor=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(editor);editor->setFocus();QTest::keyClick(editor,Qt::Key_Escape);events();}
void apply(Window& w,int field){auto* editor=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(editor);editor->setFocus();QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);events();}
void reset(Window& w,const Document& d,const QString& key){w.host.session=Session(d);w.host.session_id+=key;w.host.edited();events();}


Document goal(const Document& initial,int f,double delta){auto d=initial;d.compositions.front().guides[f].position=initial.compositions.front().guides[f].position+delta;valid(d);return d;}
std::vector<Command> update(const Document& d,int f){return {UpdateGuide{"composition",d.compositions.front().guides[f]}};}
Session preview(const Document& initial,const Document& target,int f){Session s(initial);s.begin_gesture(0);s.update_gesture(update(target,f));return s;}
QString scrub_name(int f){return "guide-position-"+QString(guide_ids[f])+"-scrub";}
QToolButton* scrub(Window& w,int f){return control<QToolButton>(w,scrub_name(f));}
void key(Window& w,int f,Qt::Key k=Qt::Key_Right){auto* c=scrub(w,f);reveal(c);c->setFocus();QTest::keyClick(c,k);events();}
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods=Qt::NoModifier){const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,mods);QApplication::sendEvent(c,&e);events();}
void cold_scrub(const QString& root,int mode){const int f=mode%2;const auto d=goal(fixture(mode),f,mode>=2?5:1);Session expected(d);const auto path=prefix(root,mode);QSettings settings(root+"/cold-settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);w.host.open(path+"-cold-input.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);check(read(path+"-cold-input.nect")==read(path+"-expected.nect")&&read(path+"-resaved.nect")==read(path+"-expected.nect"),"Separate cold exact Guide scrub native refs/source/IDs");QTest::qWait(120);}
void flow_scrub(const QString& root,int mode){
 const int f=mode%2;const auto initial=fixture(mode),target=goal(initial,f,mode>=2?5:1),half=goal(initial,f,2);Session before(initial),direct(initial);direct.apply(update(target,f),0);auto committed=preview(initial,target,f);committed.commit_gesture();auto partial=preview(initial,half,f),final_preview=preview(initial,target,f),cancelled=partial;cancelled.cancel_gesture();Session zero(initial);zero.begin_gesture(0);zero.update_gesture({});zero.commit_gesture();check(direct.document()==target&&committed.document()==target&&direct.history()==committed.history(),"Independent full native-valid scrub goal/typed/lifecycle BEFORE Window");check(evaluate_guide_position(target,"composition",f?"peer-y":"peer")==target.compositions.front().guides[f].position,"Independent stable same-axis peer follows scrub BEFORE Window");
 auto external=initial.compositions.front().artboards.front();external.name="External caller";auto foreign_doc=initial;foreign_doc.compositions.front().artboards.front().name=external.name;valid(foreign_doc);Session foreign(initial);foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",external}});check(foreign.preview_document()==foreign_doc,"Independent foreign scrub snapshot BEFORE Window");auto generation=partial;generation.cancel_gesture();generation.begin_gesture(0);generation.update_gesture({UpdateArtboard{"composition",external}});
 auto replacement=initial;replacement.id+="-replacement";valid(replacement);Session replaced(replacement),newer(initial);newer.apply({UpdateArtboard{"composition",external}},0);
 auto pending_doc=initial;pending_doc.compositions.front().guides[f].position=1;valid(pending_doc);auto pending=preview(initial,pending_doc,f);
 auto driven_doc=initial;driven_doc.compositions.front().guides[f].position_expression=Expression{"42",1};valid(driven_doc);Session driven(driven_doc);
 auto boundary=initial;boundary.compositions.front().guides[f].position=1e9;valid(boundary);Session range(boundary);range.begin_gesture(0);auto outside=boundary.compositions.front().guides[f];outside.position+=1;QString range_code;try{range.update_gesture({UpdateGuide{"composition",outside}});}catch(const Error& e){range_code=QString::fromStdString(e.code);}range.cancel_gesture();check(!range_code.isEmpty()&&range.document()==boundary&&range.revision()==0,"Independent finite scrub range refusal BEFORE Window");auto recovered=range;const auto recovered_doc=goal(boundary,f,-1);recovered.begin_gesture(0);recovered.update_gesture(update(recovered_doc,f));recovered.commit_gesture();
 const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",target);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial);same(w.host.session,before);
 auto* row=control<QGroupBox>(w,"guide-row-"+QString(guide_ids[f]));auto* number=control<QLineEdit>(w,"guide-position-"+QString(guide_ids[f]));reveal(number);auto* c=w.findChild<QToolButton*>(scrub_name(f));std::cout<<"SCRUB_CAPABILITY field="<<f<<" visible_input="<<number->isVisible()<<" tools_in_row="<<row->findChildren<QToolButton*>().size()<<" compact_scrub="<<(c!=nullptr)<<"\n";same(w.host.session,before);check(c&&c->isVisible()&&dynamic_cast<SemanticScrub*>(c),"Existing literal Guide continuous row exposes actual shared compact SemanticScrub capability");check(c->isEnabled()&&c->width()==24&&c->height()==24,"Existing shared compact enabled control");reveal(c);check(c->parentWidget()->grab().save(path+"-scrub-row.png"),"Actual shared Guide numeric/scrub row render retained");
 if(mode<2)key(w,f);else{mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);mouse(c,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);same(w.host.session,final_preview);mouse(c,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);}
 same(w.host.session,committed);check(w.host.session.document()==target,"Scrub full exact Guide/peer refs/layout/sentinel/IDs");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,committed);check(QFile::copy(path+"-saved.nect",path+"-cold-input.nect"),"Retain actual scrub bytes before Undo");QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned scrub cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Owned scrub cold passes");w.host.session.undo(w.host.session.revision());committed.undo(committed.revision());w.host.edited();events();same(w.host.session,committed);check(w.host.session.document()==initial,"One scrub Undo restores full source/refs/native");w.host.session.redo(w.host.session.revision());committed.redo(committed.revision());w.host.edited();events();same(w.host.session,committed);
 reset(w,initial,"-cancel");c=scrub(w,f);reveal(c);mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);c->setFocus();QTest::keyClick(c,Qt::Key_Escape);events();same(w.host.session,cancelled);
 reset(w,initial,"-zero");c=scrub(w,f);reveal(c);mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,8,Qt::NoButton,Qt::LeftButton);mouse(c,QEvent::MouseButtonRelease,8,Qt::LeftButton,Qt::NoButton);same(w.host.session,zero);
 reset(w,initial,"-pending");enter_numeric(w,f,"1 +",false);same(w.host.session,pending);key(w,f);same(w.host.session,pending);check(w.statusBar()->currentMessage().contains("UNCOMMITTED_INPUT"),"Scrub refuses pending numeric draft without caller mutation");
 reset(w,initial,"-session");w.host.session_id+="-other";key(w,f);same(w.host.session,before);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Old scrub Session identity refusal");
 reset(w,initial,"-document");w.host.session=Session(replacement);key(w,f);same(w.host.session,replaced);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Same Session/revision scrub replacement Document refusal");
 reset(w,initial,"-revision");w.host.session.apply({UpdateArtboard{"composition",external}},0);key(w,f);same(w.host.session,newer);check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Old scrub revision refusal");
 reset(w,initial,"-foreign");w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});w.host.edited();events();key(w,f);same(w.host.session,foreign);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Scrub preserves foreign caller before begin");
 reset(w,initial,"-generation");c=scrub(w,f);reveal(c);mouse(c,QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,10,Qt::NoButton,Qt::LeftButton);same(w.host.session,partial);w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});mouse(c,QEvent::MouseButtonRelease,10,Qt::LeftButton,Qt::NoButton);same(w.host.session,generation);
 reset(w,driven_doc,"-driven");c=scrub(w,f);check(!c->isEnabled(),"Driven Guide shared scrub disabled");key(w,f);same(w.host.session,driven);check(control<QPlainTextEdit>(w,name(f,"-expression"))->toPlainText()=="42","Disabled scrub preserves native source");
 reset(w,boundary,"-range");key(w,f);same(w.host.session,range);check(w.statusBar()->currentMessage().contains(range_code),"Existing canonical finite Guide range refusal");key(w,f,Qt::Key_Left);same(w.host.session,recovered);check(!w.statusBar()->currentMessage().contains("INVALID"),"Same exact control recovers valid left step after range failure");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";if(argc==4&&std::string(argv[1])=="--cold")cold_scrub(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));else{QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GUIDE_SCRUB_ROOT"),root=requested.isEmpty()?temporary.path():requested;int failures=0;for(int mode=0;mode<4;++mode){try{flow_scrub(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent scrub cases failed: "+std::to_string(failures));}std::cout<<"PASS Guide scrub input "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Guide scrub input "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
