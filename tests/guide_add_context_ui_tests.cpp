#include "window.hpp"
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


const Id fresh_slot="00000000-0000-0000-0000-000000000000";
Document add_goal(const Document& initial,int mode){auto d=initial;Guide added{fresh_slot,(mode==0||mode==4)?"Added":"Guide",mode==2?"y":"x",(mode==1||mode==5)?12.5:0};d.compositions.front().guides.push_back(added);valid(d);return d;}
std::vector<Command> add_command(const Document& goal){return {AddGuide{"composition",goal.compositions.front().guides.back()}};}
void add_act(Window& w,int mode){
 if(mode==0||mode==1||mode==4||mode==5){QPointer<QLineEdit> c=control<QLineEdit>(w,(mode==0||mode==4)?"guide-new-name":"guide-new-position");reveal(c);c->setFocus();QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,(mode==0||mode==4)?"Added":"12.5");events();if(mode>=4&&c){QTest::keyClick(c,Qt::Key_Return);events();}return;}
 if(mode==2){auto* c=control<QComboBox>(w,"guide-new-axis");reveal(c);c->setFocus();QTest::keyClick(c,Qt::Key_Down);events();return;}
 auto* c=control<QPushButton>(w,"guide-add");reveal(c);c->click();events();
}
Document bind_slot(Document d,const Id& id){d.compositions.front().guides.back().id=id;return d;}
void same_add(const Session& actual,const Session& expected,const Id& generated){
 const auto normalize=[&](Document d){auto& gs=d.compositions.front().guides;for(auto& g:gs)if(g.id==generated)g.id=fresh_slot;return d;};
 const auto a=normalize(actual.document()),p=normalize(actual.preview_document());
 check(a==expected.document()&&encode(a)==encode(expected.document())&&p==expected.preview_document()&&actual.history()==expected.history()&&actual.revision()==expected.revision()&&actual.gesture_active()==expected.gesture_active()&&actual.gesture_generation()==expected.gesture_generation(),"Full independent Add Session equals pre-Window oracle with only one fresh UUID slot bound");
}
void add_cold(const QString& root,int mode,const Id& generated){const auto d=bind_slot(add_goal(fixture(mode),mode),generated);valid(d);Session expected(d);const auto path=prefix(root,mode);QSettings settings(root+"/cold-settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);w.host.open(path+"-cold-input.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);check(read(path+"-cold-input.nect")==read(path+"-bound-expected.nect")&&read(path+"-resaved.nect")==read(path+"-bound-expected.nect"),"Separate cold exact Add native bytes and known UUID");QTest::qWait(120);}
void add_flow(const QString& root,int mode){
 const auto initial=fixture(mode),goal=add_goal(initial,mode);const auto commands=add_command(goal);Session before(initial),direct(initial);direct.apply(commands,0);Session positive(initial);positive.begin_gesture(0);positive.update_gesture(commands);if(mode>=3)positive.commit_gesture();check(direct.document()==goal&&positive.preview_document()==goal,"Full independent native-valid Add goal/typed command/gesture lifecycle BEFORE Window; fresh UUID slot only");
 auto external=initial.compositions.front().artboards.front();external.name="External caller";auto foreign_doc=initial;foreign_doc.compositions.front().artboards.front().name=external.name;valid(foreign_doc);Session foreign(initial);foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",external}});check(foreign.preview_document()==foreign_doc&&foreign.document()==initial&&foreign.history()==before.history(),"Independent full foreign Add refusal oracle BEFORE Window");
 auto replacement=initial;replacement.id+="-replacement";valid(replacement);Session replaced(replacement),newer(initial);newer.apply({UpdateArtboard{"composition",external}},0);
 Session generation(initial);generation.begin_gesture(0);generation.update_gesture(commands);generation.cancel_gesture();generation.begin_gesture(0);generation.update_gesture({UpdateArtboard{"composition",external}});check(generation.gesture_generation()==2&&generation.preview_document()==foreign_doc&&generation.document()==initial,"Independent replacement Add caller generation BEFORE Window");
 const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected-slot.nect",goal);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial);same(w.host.session,before);
 w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});w.host.edited();events();add_act(w,mode);same(w.host.session,foreign);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE")||w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Add refuses foreign caller explicitly");w.host.session.cancel_gesture();
 reset(w,initial,"-session");w.host.session_id+="-other";add_act(w,mode);same(w.host.session,before);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT")||w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Old Add controls refuse Session identity");
 reset(w,initial,"-document");w.host.session=Session(replacement);add_act(w,mode);std::cout<<"ADD_DOCUMENT mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";same(w.host.session,replaced);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Old Add controls refuse same Session/revision replacement Document");
 reset(w,initial,"-revision");w.host.session.apply({UpdateArtboard{"composition",external}},0);add_act(w,mode);same(w.host.session,newer);check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Old Add controls refuse revision");
 reset(w,initial,"-generation");add_act(w,0);w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});same(w.host.session,generation);add_act(w,mode);same(w.host.session,generation);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE")||w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Add preserves replacement caller generation");
 reset(w,initial,"-healthy");auto* c=control<QLineEdit>(w,"guide-new-name");reveal(c);if(mode==2||mode==5)check(c->parentWidget()->grab().save(path+"-add-row.png"),"Actual Add row render retained");add_act(w,mode);
 const auto& gs=w.host.session.preview_document().compositions.front().guides;check(gs.size()==initial.compositions.front().guides.size()+1,"Exactly one fresh Guide");const auto generated=gs.back().id;check(generated!=fresh_slot&&QRegularExpression("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$").match(QString::fromStdString(generated)).hasMatch(),"Only new identity is real generated UUID");for(const auto& g:initial.compositions.front().guides)check(g.id!=generated,"Existing stable identity never reused");same_add(w.host.session,positive,generated);
 if(mode<3){w.host.session.cancel_gesture();positive.cancel_gesture();same_add(w.host.session,positive,generated);return;}
 const auto bound=bind_slot(goal,generated);valid(bound);write(path+"-bound-expected.nect",bound);w.host.save(path+"-saved.nect");w.host.flush();events();same_add(w.host.session,positive,generated);check(read(path+"-saved.nect")==read(path+"-bound-expected.nect"),"Actual Add Save exact pre-Window values with generated UUID only bound");
 if(mode==5){check(QFile::copy(path+"-saved.nect",path+"-cold-input.nect"),"Retain Add cold bytes");QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode),QString::fromStdString(generated)});check(child.waitForStarted(5000),"Owned Add cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Owned Add cold passes");}
 w.host.session.undo(w.host.session.revision());positive.undo(positive.revision());w.host.edited();events();same_add(w.host.session,positive,generated);check(w.host.session.document()==initial,"One Add Undo full initial state");w.host.session.redo(w.host.session.revision());positive.redo(positive.revision());w.host.edited();events();same_add(w.host.session,positive,generated);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";if(argc==5&&std::string(argv[1])=="--cold")add_cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]),argv[4]);else{QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GUIDE_ADD_CONTEXT_ROOT"),root=requested.isEmpty()?temporary.path():requested;int failures=0;for(int mode=0;mode<6;++mode){try{add_flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent Add cases failed: "+std::to_string(failures));}std::cout<<"PASS Guide Add context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Guide Add context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
