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
const char* controls[]={"margin-left","margin-top","margin-right","margin-bottom"};
const char* fields[]={"margin.left","margin.top","margin.right","margin.bottom"};
const char* sources[]={"10\n+ 0.5","20\n+ 0.5","30\n+ 0.5","40\n+ 0.5"};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(15);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Missing control: "+name).toStdString());}
QString name(int field,const char* suffix){return QString(controls[field])+suffix;}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible(),"Exact production control visible");}
std::optional<Expression>& expression(Margin& g,int field){std::optional<Expression>* field_sources[]={&g.left_expression,&g.top_expression,&g.right_expression,&g.bottom_expression};return *field_sources[field];}
std::optional<Ref>& driver(Margin& g,int field){std::optional<Ref>* field_drivers[]={&g.left_driver,&g.top_driver,&g.right_driver,&g.bottom_driver};return *field_drivers[field];}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
Document fixture(int mode){
 const int field=mode/2;const bool linked=mode%2;auto d=empty_document("margin-expression-keys-"+std::to_string(mode),"composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{Margin{10.123456789012345,20.234567890123456,30.345678901234567,40.456789012345678},Grid{"grid",{15.123456789012345,20.234567890123456,1000.1234567890123,800.23456789012345},3,2,7.345678901234567,8.456789012345678}};
 auto& g=*b.layout->margin;const double literals[]={g.left,g.top,g.right,g.bottom};const int sibling=(field+1)%4;expression(g,sibling)=Expression{QString::number(literals[sibling],'g',17).toStdString(),1};
 const double driven[]={11,21,31,41};if(linked)driver(g,field)=Ref{"driver","","artboard.width"};comp.artboards.push_back({"driver","Driver",1500,0,driven[field],900});
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};valid(d);return d;
}
Document wanted(const Document& initial,int field){auto d=initial;auto& g=*d.compositions.front().artboards.front().layout->margin;driver(g,field).reset();expression(g,field)=Expression{sources[field],1};valid(d);return d;}
std::vector<Command> command(int field,bool replace){const Ref ref{"board","",fields[field]};const Expression e{sources[field],1};switch(field){case 0:return {MarginLeftCommand{SetMarginLeftExpression{ref,e,replace}}};case 1:return {MarginTopCommand{SetMarginTopExpression{ref,e,replace}}};case 2:return {MarginRightCommand{SetMarginRightExpression{ref,e,replace}}};default:return {MarginBottomCommand{SetMarginBottomExpression{ref,e,replace}}};}}
void same(const Session& a,const Session& b){if(!(a.document()==b.document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window expectation");}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
QString prefix(const QString& root,int mode){return root+"/mode-"+QString::number(mode);}
void open(Window& w,const Document& d){w.host.session=Session(d);w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}
void draft(Window& w,int field){auto* editor=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(editor);editor->setFocus();QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);const auto lines=QString(sources[field]).split('\n');QTest::keyClicks(editor,lines[0]);QTest::keyClick(editor,Qt::Key_Return);QTest::keyClicks(editor,lines[1]);events();check(editor->toPlainText()==sources[field],"Exact two-line source draft retained");}
void apply(Window& w,int field){auto* editor=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(editor);editor->setFocus();QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);events();}
void reset(Window& w,const Document& d,const QString& key){w.host.session=Session(d);w.host.session_id+=key;w.host.edited();events();}
void cold(const QString& root,int mode){const auto d=wanted(fixture(mode),mode/2);Session expected(d);const auto path=prefix(root,mode);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);w.host.open(path+"-cold-input.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);check(read(path+"-cold-input.nect")==read(path+"-expected.nect")&&read(path+"-resaved.nect")==read(path+"-expected.nect"),"Separate cold exact expression native bytes");QTest::qWait(120);}
void flow(const QString& root,int mode){
 const int field=mode/2;const bool linked=mode%2;const auto initial=fixture(mode),intended=wanted(initial,field);Session before(initial),committed(initial);committed.apply(command(field,linked),0);check(committed.document()==intended,"Canonical command agrees with full independent Document BEFORE Window");
 const auto button_expected=committed;
 QString refusal;if(linked){Session denied(initial);try{denied.apply(command(field,false),0);}catch(const Error& e){refusal=QString::fromStdString(e.code);}check(!refusal.isEmpty(),"Independent existing link replacement requires explicit authorization BEFORE Window");same(denied,before);}
 auto foreign=before;foreign.begin_gesture(0);auto incoming=initial.compositions.front().artboards.front();incoming.name="Foreign pending";foreign.update_gesture({UpdateArtboard{"composition",incoming}});auto newer=before;newer.apply({UpdateArtboard{"composition",incoming}},0);
 const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",intended);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial);same(w.host.session,before);draft(w,field);same(w.host.session,before);if(linked)control<QCheckBox>(w,name(field,"-replace"))->setChecked(true);apply(w,field);same(w.host.session,committed);
 auto* source=control<QPlainTextEdit>(w,name(field,"-expression"));reveal(source);check(source->toPlainText()==sources[field],"Committed source preserved verbatim");check(source->parentWidget()->grab().save(path+"-source.png"),"Actual editable source/checkbox/actions render retained");check(w.grab().save(path+"-committed-window.png"),"Actual committed Window render retained");w.host.save(path+"-saved.nect");w.host.flush();same(w.host.session,committed);check(QFile::copy(path+"-saved.nect",path+"-cold-input.nect"),"Actual saved bytes retained before later edits");QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Separate cold acceptance passes");
 w.host.session.undo(w.host.session.revision());committed.undo(committed.revision());w.host.edited();events();same(w.host.session,committed);check(w.host.session.document()==initial,"One Undo restores full source/literals/link/IDs");w.host.session.redo(w.host.session.revision());committed.redo(committed.revision());w.host.edited();events();same(w.host.session,committed);
 reset(w,initial,"-escape");draft(w,field);auto* canceled=control<QPlainTextEdit>(w,name(field,"-expression"));QTest::keyClick(canceled,Qt::Key_Escape);events();same(w.host.session,before);check(control<QPlainTextEdit>(w,name(field,"-expression"))->toPlainText().isEmpty(),"Escape discards source draft without authored mutation");
 if(linked){reset(w,initial,"-denied");draft(w,field);apply(w,field);same(w.host.session,before);check(w.statusBar()->currentMessage().contains(refusal),"Keyboard refuses unconfirmed existing link replacement");check(control<QPlainTextEdit>(w,name(field,"-expression"))->toPlainText()==sources[field],"Refused source draft remains editable");}
 reset(w,initial,"-button");draft(w,field);if(linked)control<QCheckBox>(w,name(field,"-replace"))->setChecked(true);auto* button=control<QPushButton>(w,name(field,"-apply-expression"));reveal(button);button->click();events();same(w.host.session,button_expected);
 reset(w,initial,"-foreign");draft(w,field);if(linked)control<QCheckBox>(w,name(field,"-replace"))->setChecked(true);w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",incoming}});apply(w,field);same(w.host.session,foreign);w.host.session.cancel_gesture();
 reset(w,initial,"-revision");draft(w,field);if(linked)control<QCheckBox>(w,name(field,"-replace"))->setChecked(true);w.host.session.apply({UpdateArtboard{"composition",incoming}},0);apply(w,field);same(w.host.session,newer);check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Keyboard stale revision cannot replace canonical source");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";if(argc==4&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));else{QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MARGIN_EXPRESSION_KEYS_ROOT"),root=requested.isEmpty()?temporary.path():requested;int failures=0;for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent expression key cases failed: "+std::to_string(failures));}std::cout<<"PASS Margin expression keys "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Margin expression keys "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
