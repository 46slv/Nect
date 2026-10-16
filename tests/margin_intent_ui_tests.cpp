#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace {
int checks=0;
const char* fields[]={"margin-left","margin-top","margin-right","margin-bottom"};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const char* name){for(auto* p:w.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(name);}
void reveal(QWidget* item){for(auto* p=item->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(item);events();check(item->isVisible(),"Exact production control visible");}
void native_valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent complete Document native-validates BEFORE Window");}
Document fixture(int mode,int axis){
 auto d=empty_document("margin-intent-"+std::to_string(mode)+"-"+std::to_string(axis),"composition","board");auto& b=d.compositions.front().artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{mode==0?std::optional<Margin>{}:std::optional<Margin>{Margin{10,20,30,40}},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);d.compositions.front().roots={o.id};
 if(mode==1){d.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});Session seeded(d);seeded.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seeded.document();}
 native_valid(d);return d;
}
double value(const Margin& m,int axis){return axis==0?m.left:axis==1?m.top:axis==2?m.right:m.bottom;}
void assign(Margin& m,int axis,double v){if(axis==0)m.left=v;else if(axis==1)m.top=v;else if(axis==2)m.right=v;else m.bottom=v;}
Document intended(const Document& initial,int mode,int axis){
 auto d=initial;if(mode==0)d.compositions.front().artboards.front().layout->margin=Margin{0,0,0,0};
 else if(mode==1){auto& target=d.compositions.front().artboards.back();target.layout=ArtboardLayout{Margin{10,20,30,40},{}};target.template_assignment->margin_overridden=true;}
 else if(mode==2)assign(*d.compositions.front().artboards.front().layout->margin,axis,value(*initial.compositions.front().artboards.front().layout->margin,axis)+0.5);
 else d.compositions.front().artboards.front().name="Foreign pending";
 native_valid(d);return d;
}
std::vector<Command> commands(const Document& d,int mode){
 if(mode==1)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.margin",d.compositions.front().artboards.back().layout->margin}}};
 if(mode==3)return {UpdateArtboard{"composition",d.compositions.front().artboards.front()}};
 return {SetArtboardLayout{"composition","board",d.compositions.front().artboards.front().layout}};
}
void same(const Session& a,const Session& b){
 if(!(a.document()==b.document()&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))
  std::cerr<<"Snapshot document="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" revision="<<a.revision()<<"/"<<b.revision()<<" gesture="<<a.gesture_active()<<"/"<<b.gesture_active()<<" generation="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session snapshot matches independent pre-Window intent");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle written once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native file readable");return f.readAll();}
void open(Window& w,const Document& initial,int mode){w.host.session=Session(initial);w.host.edited();w.canvas->set_active_artboard("composition",mode==1?"target":"board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}
void draft(Window& w,int axis,const QString& text){auto* input=control<QLineEdit>(w,fields[axis]);reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);events();check(input->text()==text,"Raw exact input intent retained");}
void enter(Window& w,int axis){QTest::keyClick(control<QLineEdit>(w,fields[axis]),Qt::Key_Return);events();}
QString prefix(const QString& root,int mode,int axis){return root+"/case-"+QString::number(mode)+"-"+QString::number(axis);}
void cold(const QString& root,int mode,int axis){const auto expected_doc=intended(fixture(mode,axis),mode,axis);Session expected(expected_doc);const auto path=prefix(root,mode,axis);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);w.host.open(path+"-saved.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&read(path+"-resaved.nect")==read(path+"-expected.nect"),"Presence/explicit override survives separate cold native resave byte-exactly");QTest::qWait(120);}
void flow(const QString& root,int mode,int axis){
 const auto initial=fixture(mode,axis),wanted=intended(initial,mode,axis);Session before(initial),expected(initial);
 if(mode<2){expected.begin_gesture(0);expected.update_gesture(commands(wanted,mode));expected.commit_gesture();check(expected.document()==wanted,"Independent canonical presence/absolute intent agrees BEFORE Window");}
 else if(mode==2){expected.begin_gesture(0);expected.update_gesture(commands(wanted,mode));expected.cancel_gesture();}
 else {const auto owned=intended(initial,2,axis);expected.begin_gesture(0);expected.update_gesture(commands(owned,2));expected.cancel_gesture();expected.begin_gesture(0);expected.update_gesture(commands(wanted,3));}
 const auto path=prefix(root,mode,axis);write(path+"-initial.nect",initial);write(path+"-expected.nect",wanted);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,mode);same(w.host.session,before);
 if(mode<2){draft(w,axis,"+=0");enter(w,axis);same(w.host.session,before);
  if(mode==0){auto* apply=control<QPushButton>(w,"margin-apply");reveal(apply);apply->click();events();}
  else {const int next=(axis+1)%4;draft(w,next,QString::number(value(*initial.compositions.front().artboards.front().layout->margin,next),'g',17));enter(w,next);}
  same(w.host.session,expected);w.host.save(path+"-saved.nect");w.host.flush();same(w.host.session,expected);
  QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode),QString::number(axis)});check(child.waitForStarted(5000),"Owned cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Owned cold acceptance passes");
  w.host.session.undo(w.host.session.revision());expected.undo(expected.revision());w.host.edited();events();same(w.host.session,expected);check(w.host.session.document()==initial,"One Undo restores absence/inheritance");w.host.session.redo(w.host.session.revision());expected.redo(expected.revision());w.host.edited();events();same(w.host.session,expected);
 }else if(mode==2){draft(w,axis,"+=0.5");auto* input=control<QLineEdit>(w,fields[axis]);input->setCursorPosition(input->text().size());QTest::keyClick(input,Qt::Key_Backspace);QTest::keyClick(input,Qt::Key_Backspace);events();check(input->text()=="+=0","In-place edit reaches valid relative zero without Ctrl reentry");same(w.host.session,expected);enter(w,axis);same(w.host.session,expected);}
 else {draft(w,axis,"+=0.5");w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture(commands(wanted,3));w.canvas->refresh();w.canvas->update();events();same(w.host.session,expected);enter(w,axis);same(w.host.session,expected);w.host.session.cancel_gesture();expected.cancel_gesture();same(w.host.session,expected);}
}
void grid_boundary(const QString& root,bool foreign){
 const auto initial=fixture(4,foreign?1:0);auto changed=initial;changed.compositions.front().artboards.front().layout->grid->row_gutter+=0.5;native_valid(changed);
 auto other=initial;other.compositions.front().artboards.front().name="Foreign Grid pending";native_valid(other);
 Session before(initial),expected(initial),preview(initial);preview.begin_gesture(0);preview.update_gesture(commands(changed,2));expected.begin_gesture(0);expected.update_gesture(commands(changed,2));expected.cancel_gesture();
 if(foreign){expected.begin_gesture(0);expected.update_gesture(commands(other,3));}
 const auto path=root+(foreign?"/grid-foreign":"/grid-zero");write(path+"-initial.nect",initial);write(path+"-expected.nect",foreign?other:initial);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,0);same(w.host.session,before);
 auto* input=control<QLineEdit>(w,"grid-row-gutter");reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=0.5");events();
 same(w.host.session,preview);
 if(foreign){w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture(commands(other,3));w.canvas->refresh();events();QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,expected);w.host.session.cancel_gesture();expected.cancel_gesture();same(w.host.session,expected);}
 else {input->setCursorPosition(input->text().size());QTest::keyClick(input,Qt::Key_Backspace);QTest::keyClick(input,Qt::Key_Backspace);events();check(input->text()=="+=0","Grid in-place zero intent");same(w.host.session,expected);QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,expected);}
 std::cout<<"CASE PASS Grid "<<(foreign?"foreign-generation":"in-place-zero")<<"\n";
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";if(argc==5&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]),std::stoi(argv[4]));else{QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MARGIN_INTENT_ROOT");const auto root=requested.isEmpty()?temporary.path():requested;int failures=0;for(int mode=0;mode<4;++mode)for(int axis=0;axis<4;++axis){try{flow(root,mode,axis);std::cout<<"CASE PASS mode="<<mode<<" axis="<<axis<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" axis="<<axis<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent intent cases failed: "+std::to_string(failures));grid_boundary(root,false);grid_boundary(root,true);}std::cout<<"PASS Margin intent "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Margin intent "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
