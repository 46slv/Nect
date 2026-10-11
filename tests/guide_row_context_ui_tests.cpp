#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <array>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPointer>
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

Document intended(const Document& initial,int field,int action){
 auto d=initial;auto& gs=d.compositions.front().guides;
 if(action==0||action==5)gs[field].name="Renamed";
 if(action==2)gs[field].axis=field?"x":"y";
 if(action==3)gs.erase(gs.begin()+field);
 if(action==4){gs[field].position=42;gs[field].position_expression.reset();}
 valid(d);return d;
}
std::vector<Command> row_command(const Document& initial,int field,int action){
 if(action==3)return {DeleteGuide{"composition",guide_ids[field]}};
 if(action==4)return {UnlinkGuidePosition{{guide_ids[field],"","guide.position"}}};
 const auto d=intended(initial,field,action);return {UpdateGuide{"composition",d.compositions.front().guides[field]}};
}
void act(Window& w,int field,int action){
 std::cout<<"ACT field="<<field<<" action="<<action<<" rev="<<w.host.session.revision()<<" gen="<<w.host.session.gesture_generation()<<" active="<<w.host.session.gesture_active()<<"\n";
 if(action==0||action==5){QPointer<QLineEdit> c=control<QLineEdit>(w,"guide-name-"+QString(guide_ids[field]));reveal(c);c->setFocus();QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,"Renamed");events();if(action==5&&c){QTest::keyClick(c,Qt::Key_Return);events();}return;}
 if(action==2){auto* c=control<QComboBox>(w,"guide-axis-"+QString(guide_ids[field]));reveal(c);c->setFocus();QTest::keyClick(c,field?Qt::Key_Up:Qt::Key_Down);events();return;}
 const char* tag=action==1?"guide-apply-":action==3?"guide-delete-":"guide-unlink-position-";
 auto* c=control<QPushButton>(w,QString(tag)+guide_ids[field]);reveal(c);c->click();events();
}
void cold_row(const QString& root,int mode){const int f=mode%2,a=mode/2;const auto d=intended(fixture(mode),f,a);Session expected(d);const auto path=prefix(root,mode);QSettings settings(root+"/cold-settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);w.host.open(path+"-cold-input.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);check(read(path+"-cold-input.nect")==read(path+"-expected.nect")&&read(path+"-resaved.nect")==read(path+"-expected.nect"),"Separate cold row native bytes");QTest::qWait(120);}
void flow(const QString& root,int mode){
 const int f=mode%2,a=mode/2;const auto initial=fixture(mode),goal=intended(initial,f,a);const auto commands=row_command(initial,f,a);Session before(initial),direct(initial);direct.apply(commands,0);Session positive(initial);positive.begin_gesture(0);positive.update_gesture(commands);if(a!=0&&a!=2)positive.commit_gesture();check(direct.document()==goal&&positive.preview_document()==goal,"Full independent typed-command goal and canonical row lifecycle BEFORE Window");
 auto external=initial.compositions.front().artboards.front();external.name="External caller";
 Session foreign(initial);foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",external}});auto foreign_document=initial;foreign_document.compositions.front().artboards.front().name=external.name;valid(foreign_document);check(foreign.preview_document()==foreign_document&&foreign.document()==initial&&foreign.history()==before.history(),"Independent foreign full preview BEFORE Window");
 auto replacement=initial;replacement.id+="-replacement";valid(replacement);Session replaced(replacement),newer(initial);newer.apply({UpdateArtboard{"composition",external}},0);
 auto sibling_doc=initial;sibling_doc.compositions.front().guides[1-f].name="Renamed";valid(sibling_doc);Session sibling(initial);sibling.begin_gesture(0);sibling.update_gesture({UpdateGuide{"composition",sibling_doc.compositions.front().guides[1-f]}});check(sibling.preview_document()==sibling_doc,"Independent other-row preview BEFORE Window");
 // Existing Window event filter cancels its own preview on a keyboard event
 // outside that row. This is a permitted owned transition, not a foreign caller.
 auto sibling_keyboard=sibling;sibling_keyboard.cancel_gesture();sibling_keyboard.begin_gesture(0);sibling_keyboard.update_gesture(commands);if(a==5)sibling_keyboard.commit_gesture();check(sibling_keyboard.preview_document()==goal&&sibling_keyboard.gesture_generation()==2,"Independent owned keyboard row transition BEFORE Window");
 Session generation(initial);generation.begin_gesture(0);generation.update_gesture(row_command(initial,f,0));generation.cancel_gesture();generation.begin_gesture(0);generation.update_gesture({UpdateArtboard{"composition",external}});check(generation.gesture_generation()==2&&generation.preview_document()==foreign_document,"Independent replacement caller generation BEFORE Window");
 const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",goal);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial);same(w.host.session,before);
 w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});w.host.edited();events();same(w.host.session,foreign);act(w,f,a);std::cout<<"FOREIGN mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";same(w.host.session,foreign);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE")||w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Existing row refuses foreign caller explicitly");w.host.session.cancel_gesture();
 reset(w,initial,"-session");w.host.session_id+="-other";act(w,f,a);same(w.host.session,before);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT")||w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Old row explicitly refuses replacement Session");
 reset(w,initial,"-document");w.host.session=Session(replacement);act(w,f,a);same(w.host.session,replaced);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Old row same Session/revision replacement Document refusal");
 reset(w,initial,"-revision");w.host.session.apply({UpdateArtboard{"composition",external}},0);act(w,f,a);same(w.host.session,newer);check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Old row revision refusal");
 reset(w,initial,"-other-row");act(w,1-f,0);same(w.host.session,sibling);std::cout<<"STAGE other-row\n";act(w,f,a);if(a==0||a==2||a==5){same(w.host.session,sibling_keyboard);}else{same(w.host.session,sibling);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Callback refuses another row owned preview without an outside keyboard event");}
 reset(w,initial,"-generation");act(w,f,0);w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});same(w.host.session,generation);std::cout<<"STAGE replacement-generation\n";act(w,f,a);same(w.host.session,generation);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE")||w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Existing row preserves replacement caller generation");
 std::cout<<"STAGE healthy\n";reset(w,initial,"-positive");if(a==5){auto* c=control<QLineEdit>(w,"guide-name-"+QString(guide_ids[f]));reveal(c);check(c->parentWidget()->grab().save(path+"-healthy-row.png"),"Actual healthy Guide row render retained");}act(w,f,a);same(w.host.session,positive);check((a==0||a==2)?w.host.session.gesture_active():!w.host.session.gesture_active(),"Healthy existing row owns exactly its expected lifecycle");
 if(a==0||a==2){w.host.session.cancel_gesture();positive.cancel_gesture();same(w.host.session,positive);return;}
 check(w.host.session.document()==goal&&w.host.session.revision()==1,"Healthy row authors exactly one canonical Undo");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,positive);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Actual row native Save exact independent full goal");
 if(a==5){check(QFile::copy(path+"-saved.nect",path+"-cold-input.nect"),"Retain row bytes before Undo");QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned row cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Owned cold row passes");}
 w.host.session.undo(w.host.session.revision());positive.undo(positive.revision());w.host.edited();events();same(w.host.session,positive);check(w.host.session.document()==initial,"One row Undo full initial state");w.host.session.redo(w.host.session.revision());positive.redo(positive.revision());w.host.edited();events();same(w.host.session,positive);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";if(argc==4&&std::string(argv[1])=="--cold")cold_row(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));else{QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GUIDE_ROW_CONTEXT_ROOT"),root=requested.isEmpty()?temporary.path():requested;int failures=0;for(int mode=0;mode<12;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent row cases failed: "+std::to_string(failures));}std::cout<<"PASS Guide row context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Guide row context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
