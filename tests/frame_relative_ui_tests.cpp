#include "window.hpp"
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
std::vector<Command> commands(const Document& intended,int f,bool inherited){
 if(inherited){const auto& a=*intended.compositions.front().artboards.back().template_assignment;return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target",std::string("frame.")+fields[f],f==2?*a.width_override:*a.height_override}}};}
 return {UpdateArtboard{"composition",intended.compositions.front().artboards.front()}};
}
void flow(const QString& root,int mode){
 const bool inherited=mode==4||mode==5,driven=mode>=6;const int f=mode<4?mode:mode%2+2;const auto initial=fixture(inherited,driven?f:-1);Session base(initial);const auto first_doc=driven?initial:goal(initial,f,inherited,initial.compositions.front().artboards.front().*members[f]+0.5),second_doc=driven?initial:goal(initial,f,inherited,initial.compositions.front().artboards.front().*members[f]);
 auto first=base;auto second=base;auto undone=base;auto redone=base;if(!driven){first.apply(commands(first_doc,f,inherited),0);check(first.document()==first_doc,"Independent relative + full intended Document BEFORE Window");second=first;second.apply(commands(second_doc,f,inherited),second.revision());check(second.document()==second_doc,"Independent relative - full intended Document BEFORE Window");undone=second;undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());}
 const auto negative_doc=f<2?goal(second_doc,f,false,-20.25):second_doc;auto negative=redone;if(f<2){negative.apply(commands(negative_doc,f,false),negative.revision());check(negative.document()==negative_doc,"Independent negative absolute command distinct from subtraction BEFORE Window");}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-first-expected.nect",first_doc);write(path+"-second-expected.nect",second_doc);if(f<2)write(path+"-negative-expected.nect",negative_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.canvas->set_active_artboard("composition",inherited?"target":"board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();same(w.host.session,base);
 const auto name=QString("artboard-")+fields[f];auto input=[&]{return control<QLineEdit>(w,name);};auto enter=[&](const QString& text){auto* e=input();reveal(e);e->setFocus();QTest::keyClick(e,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(e,text);QTest::keyClick(e,Qt::Key_Return);events();};
 if(driven){check(input()->isReadOnly(),"Typed source visibly protects Frame numeric field");const auto before=input()->text();enter("+=0.5");same(w.host.session,base);check(input()->text()==before,"Actual Qt typing cannot replace driven source");w.host.save(path+"-initial-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-initial-saved.nect")==read(path+"-initial-expected.nect"),"Read-only source remains exact native bytes");w.close();events();same(w.host.session,base);return;}
 control<QPlainTextEdit>(w,"artboard-height-expression")->setPlainText("ref(");events();same(w.host.session,base);
 enter("+=");same(w.host.session,base);check(input()->text()=="+="&&w.statusBar()->currentMessage().contains("INVALID_VALUE"),"Incomplete relative text stays draft, never zero/History");
 enter("+=0");same(w.host.session,base);check(w.statusBar()->currentMessage().isEmpty(),"Relative zero retires prior error without History or inherited override");
 enter("+=0.5");std::cout<<"FRAME_RELATIVE mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual relative input status retained");same(w.host.session,first);check(w.statusBar()->currentMessage().isEmpty(),"Successful relative commit clears old error");check(control<QPlainTextEdit>(w,"artboard-height-expression")->toPlainText()=="ref(","Numeric commit preserves separate incomplete source draft");
 w.host.save(path+"-first-saved.nect");w.host.flush();events();same(w.host.session,first);check(read(path+"-first-saved.nect")==read(path+"-first-expected.nect"),"Exact first relative native bytes including untouched precision/source");
 enter("-=0.5");same(w.host.session,second);w.host.save(path+"-second-saved.nect");w.host.flush();events();same(w.host.session,second);check(read(path+"-second-saved.nect")==read(path+"-second-expected.nect"),"Exact second relative native bytes, inherited override intent preserved");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.host.edited();events();
 if(f<2){enter("-20.25");same(w.host.session,negative);w.host.save(path+"-negative-saved.nect");w.host.flush();events();same(w.host.session,negative);check(read(path+"-negative-saved.nect")==read(path+"-negative-expected.nect"),"Negative absolute is distinct from -= relative");}
 w.close();events();same(w.host.session,f<2?negative:redone);
}

void feedback(const QString& root){
 const auto initial=fixture(false);Session base(initial);const auto intended=goal(initial,0,false,35.25);auto applied=base;applied.apply(commands(intended,0,false),0);check(applied.document()==intended,"Independent finite correction full Document BEFORE Window");const auto path=root+"/mode-8";write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();same(w.host.session,base);
 auto* input=control<QLineEdit>(w,"artboard-x");reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=");QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,base);check(w.statusBar()->currentMessage().contains("INVALID_VALUE"),"Actual malformed source error present before valid correction");
 QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"35.25");QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,applied);std::cout<<"FRAME_FEEDBACK current_valid_doc=1 status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual finite correction status render");check(w.statusBar()->currentMessage().isEmpty(),"Successful finite Frame correction clears prior invalid-input error");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,applied);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Finite correction exact native bytes");w.close();events();same(w.host.session,applied);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_FRAME_RELATIVE_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<9;++mode){try{if(mode==8)feedback(root);else flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Frame relative cases failed: "+std::to_string(failures));std::cout<<"PASS Frame relative "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Frame relative "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
