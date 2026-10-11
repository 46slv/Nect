#include "window.hpp"
#include <QPointer>
#include <QTreeWidget>
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace {
int checks=0;
const char* names[]={"margin-left","margin-top","margin-right","margin-bottom"};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Production control missing: "+name).toStdString());}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible(),"Exact production control visible");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
Document fixture(int mode){
 auto d=empty_document("margin-scrub-"+std::to_string(mode),"composition","board");auto& b=d.compositions.front().artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{Margin{10.123456789012345,20.234567890123456,30.345678901234567,40.456789012345678},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);d.compositions.front().roots={o.id};
 if(mode==1){d.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});Session seeded(d);seeded.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seeded.document();}
 d.compositions.front().guides={{"continuity-guide","Guide","x",10.123456789012345}};
 valid(d);return d;
}
void same(const Session& a,const Session& b){
 if(!(a.document()==b.document()&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
void open(Window& w,const Document& initial,int mode){w.host.session=Session(initial);w.host.edited();w.canvas->set_active_artboard("composition",mode==1?"target":"board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}

Document goal(const Document& initial,int f,bool inherited,double amount){
 auto d=initial;auto& b=inherited?d.compositions.front().artboards.back():d.compositions.front().artboards.front();
 if(inherited){b.layout=ArtboardLayout{initial.compositions.front().artboards.front().layout->margin,{}};b.template_assignment->margin_overridden=true;}
 auto& m=*b.layout->margin;if(f==0)m.left+=amount;else if(f==1)m.top+=amount;else if(f==2)m.right+=amount;else m.bottom+=amount;
 valid(d);return d;
}
std::vector<Command> commands(const Document& d,bool inherited){
 if(inherited)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.margin",d.compositions.front().artboards.back().layout->margin}}};
 return {SetArtboardLayout{"composition","board",d.compositions.front().artboards.front().layout}};
}

const char* fields[]={"grid-x","margin-left","guide-position-continuity-guide","grid-x"};
void record(Window& w,const QString& path,int mode,const char* stage){
 int frame_fields=0;for(auto* c:w.findChildren<QLineEdit*>())if(c->isVisible()&&(c->objectName()=="grid-x"||c->objectName()=="margin-left"||c->objectName()=="guide-position-continuity-guide"))++frame_fields;
 std::cout<<"FRAME_STAGE mode="<<mode<<" stage="<<stage<<" visible_fields="<<frame_fields<<" object="<<w.canvas->selected_object<<" active_board="<<w.canvas->active_artboard()<<" focus="<<(QApplication::focusWidget()?QApplication::focusWidget()->objectName().toStdString():"none")<<"\n";
 check(w.grab().save(path+"-"+stage+".png"),"Actual exact Window stage retained");
 check(frame_fields==3,"Current Frame context remains visible through native authoring/Save stage");
}
void flow(const QString& root,int mode){
 const auto initial=fixture(0);auto target=initial;std::vector<Command> cmd;
 if(mode==0){target.compositions.front().artboards.front().layout->grid->bounds.x+=0.5;cmd={SetArtboardLayout{"composition","board",target.compositions.front().artboards.front().layout}};}
 else if(mode==1){target.compositions.front().artboards.front().layout->margin->left+=0.5;cmd={SetArtboardLayout{"composition","board",target.compositions.front().artboards.front().layout}};}
 else if(mode==2){target.compositions.front().guides.front().position+=1;cmd={UpdateGuide{"composition",target.compositions.front().guides.front()}};}
 valid(target);Session before(initial),preview(initial),committed(initial);
 if(mode!=3){preview.begin_gesture(0);preview.update_gesture(cmd);committed=preview;committed.commit_gesture();}
 check(committed.document()==target,"Full independent intended Doc/canonical expected lifecycle BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",target);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);
 QString stage="open";
 for(auto* tree:w.findChildren<QTreeWidget*>())QObject::connect(tree,&QTreeWidget::itemSelectionChanged,&w,[&w,tree,&stage]{
  std::cout<<"TREE_SIGNAL stage="<<stage.toStdString()<<" selected="<<tree->selectedItems().size()<<" current="<<(tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toString().toStdString():"none")<<" canvas="<<w.canvas->selected_object<<"\n";
 });
 open(w,initial,0);same(w.host.session,before);record(w,path,mode,"open");
 if(mode!=3){
  QPointer<QLineEdit> input=control<QLineEdit>(w,fields[mode]);reveal(input);input->setFocus();events();stage="focus";same(w.host.session,before);record(w,path,mode,"focus");
  stage="select-all";QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);events();same(w.host.session,before);record(w,path,mode,"select-all");
  stage="typing";QTest::keyClicks(input,mode==2?"+=1":"+=0.5");events();same(w.host.session,preview);record(w,path,mode,"preview");
  stage="commit";QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,committed);record(w,path,mode,"commit");
 }
 stage="save";w.host.save(path+"-saved.nect");events();same(w.host.session,committed);record(w,path,mode,"save");
 stage="flush";w.host.flush();events();same(w.host.session,committed);record(w,path,mode,"flush");
 check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Exact native bytes through saved Frame context");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{
 std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");
 const auto requested=qEnvironmentVariable("NECT_FRAME_CONTINUITY_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;
 for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
 if(failures)throw std::runtime_error("Independent Frame context cases failed: "+std::to_string(failures));std::cout<<"PASS Frame continuity "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL Frame continuity "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
