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
Document fixture(){
 auto d=empty_document("size-document","composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.width=1200;b.height=900;
 b.width_driver=ArtboardSizeDriver{Ref{"parent","","artboard.width"}};b.height_driver=ArtboardSizeDriver{Ref{"parent","","artboard.height"}};
 b.layout=ArtboardLayout{Margin{10,20,30,40},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 comp.artboards.push_back({"parent","Parent",1600,0,1500,1000});comp.artboards.push_back({"other","Other",3200,0,1800,1200});
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};valid(d);return d;
}
void same(const Session& a,const Session& b){
 std::cout<<"SNAPSHOT doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}

std::string axis(bool width){return width?"width":"height";}
Expression expression(bool width){return {"ref(\"other\",\"\",\"artboard."+axis(width)+"\")+1",1};}
Document goal(const Document& d,int action,bool width){
 auto out=d;auto& b=out.compositions.front().artboards.front();auto& slot=width?b.width_driver:b.height_driver;
 if(action==0)slot=ArtboardSizeDriver{Ref{"other","","artboard."+axis(width)}};
 else if(action==1)slot=ArtboardSizeDriver{expression(width)};
 else {if(width)b.width=out.compositions.front().artboards.at(1).width;else b.height=out.compositions.front().artboards.at(1).height;slot.reset();}
 valid(out);return out;
}
std::vector<Command> commands(int action,bool width,bool replace=true){
 Ref target{"board","","artboard."+axis(width)};
 if(action==0)return {LinkArtboardSize{target,{"other","","artboard."+axis(width)},replace}};
 if(action==1)return {SetArtboardSizeExpression{target,expression(width),replace}};
 return {UnlinkArtboardSize{target}};
}
void prepare(Window& w,int action,bool width,bool consent){
 const auto base=QString("artboard-")+QString::fromStdString(axis(width));
 if(action==0){auto* c=control<QComboBox>(w,base+"-link-source");const auto label=QString("Other · ")+QString::fromStdString(axis(width))+" (other/artboard."+QString::fromStdString(axis(width))+")";const int index=c->findText(label);check(index>=0,"Exact captured stable source identity label exists");c->setCurrentIndex(index);}
 if(action==1){auto* c=control<QPlainTextEdit>(w,base+"-expression");c->setPlainText(QString::fromStdString(expression(width).source));}
 if(action<2){auto* c=control<QCheckBox>(w,base+"-replace");check(c->isEnabled(),"Existing driver explicit replacement checkbox enabled");c->setChecked(consent);}
 events();
}
void act(Window& w,int action,bool width){
 const auto base=QString("artboard-")+QString::fromStdString(axis(width));auto* c=control<QPushButton>(w,base+(action==0?"-link":action==1?"-apply-expression":"-unlink"));reveal(c);QSignalSpy observed_signal(c,&QPushButton::clicked);QTest::mouseClick(c,Qt::LeftButton);check(observed_signal.count()>0,"Exact old/current source commit clicked signal fired");events();
}
void flow(const QString& root,int mode){
 const int action=mode%3;const bool width=(mode%6)<3,active=mode>=6;const auto initial=fixture();auto replacement=initial;replacement.id+="-replacement";
 replacement.compositions.front().artboards.at(1).width=1600;replacement.compositions.front().artboards.at(1).height=1100;replacement.compositions.front().artboards.at(2).width=1900;replacement.compositions.front().artboards.at(2).height=1300;replacement.objects.at("sentinel").contours.front().points.front().x.literal+=7;valid(replacement);
 Session original(initial),idle(replacement),foreign(replacement);auto pending=replacement;pending.compositions.front().artboards.front().name="Caller pending";valid(pending);foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",pending.compositions.front().artboards.front()}});check(foreign.preview_document()==pending&&foreign.document()==replacement,"Independent foreign caller full preview BEFORE Window");
 auto fresh_base=active?foreign:idle;if(active)fresh_base.cancel_gesture();const auto intended=goal(replacement,action,width);auto fresh=fresh_base;fresh.apply(commands(action,width),0);check(fresh.document()==intended,"Independent typed size command equals manual full intended Document BEFORE Window");
 const auto prop=artboard_size_property(intended,{"board","","artboard."+axis(width)});const double expected=action<2?(width?1900:1300)+(action==1?1:0):(width?1600:1100);
 check(prop.evaluated==expected&&prop.literal==(action<2?(width?1200:900):expected)&&prop.source_kind==(action==0?"link":action==1?"expression":"literal"),"Independent exact driver/source/literal/evaluated contract BEFORE Window");
 if(action<2){auto refused=fresh_base;bool seen=false;try{refused.apply(commands(action,width,false),0);}catch(const Error& e){seen=e.code=="DRIVEN_ARTBOARD_SIZE";}check(seen,"Independent no-consent typed source refusal BEFORE Window");same(refused,fresh_base);}
 auto undone=fresh;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial.nect",initial);write(path+"-replacement.nect",replacement);write(path+"-pending.nect",pending);write(path+"-expected.nect",intended);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=original;w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();same(w.host.session,original);prepare(w,action,width,true);same(w.host.session,original);
 const auto sid=w.host.session_id;w.host.session=active?foreign:idle;check(w.host.session_id==sid&&w.host.session.revision()==0,"Replacement Document exact same Session ID/revision");same(w.host.session,active?foreign:idle);
 act(w,action,width);std::cout<<"SIZE_DOCUMENT mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual native refusal status retained");same(w.host.session,active?foreign:idle);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Old typed size controls explicitly refuse another Document");
 if(active)w.host.session.cancel_gesture();same(w.host.session,fresh_base);w.host.edited();events();
 if(action<2){prepare(w,action,width,false);same(w.host.session,fresh_base);act(w,action,width);same(w.host.session,fresh_base);check(w.statusBar()->currentMessage().contains("DRIVEN_ARTBOARD_SIZE"),"Current source requires explicit replacement consent");}
 prepare(w,action,width,true);same(w.host.session,fresh_base);act(w,action,width);same(w.host.session,fresh);
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,fresh);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Exact refreshed typed size source native bytes");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_SIZE_DOCUMENT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<12;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Size Document cases failed: "+std::to_string(failures));std::cout<<"PASS Size Document "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Size Document "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
