#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
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
 auto d=empty_document("parent-document","composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.width=1200;b.height=900;b.parent_size=ArtboardParent{"parent",true,true};
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
Document goal(const Document& d,int action){
 auto out=d;auto& b=out.compositions.front().artboards.front();const auto& parent=out.compositions.front().artboards.at(1);
 if(action==0)b.parent_size=ArtboardParent{"other",true,true};
 else if(action==1){b.width=parent.width;b.parent_size->width=false;}
 else if(action==2){b.height=parent.height;b.parent_size->height=false;}
 else {b.width=parent.width;b.height=parent.height;b.parent_size.reset();}
 valid(out);return out;
}
std::vector<Command> commands(const Document& intended,int action){if(action>=3)return {DetachArtboardParent{"composition","board"}};return {UpdateArtboard{"composition",intended.compositions.front().artboards.front()}};}
void act(Window& w,int action){
 if(action==0||action==4){auto* c=control<QComboBox>(w,"artboard-parent");reveal(c);const int index=c->findData(action==0?QString("other"):QString{});check(index>=0,"Exact stable parent ID exists in old selector");QSignalSpy observed_signal(c,qOverload<int>(&QComboBox::currentIndexChanged));c->setCurrentIndex(index);check(observed_signal.count()>0,"Exact old/current Parent selector signal fired");}
 else if(action==1||action==2){auto* c=control<QCheckBox>(w,action==1?"artboard-inherit-width":"artboard-inherit-height");reveal(c);check(c->isChecked(),"Existing inheritance checked");QSignalSpy observed_signal(c,&QCheckBox::toggled);QTest::mouseClick(c,Qt::LeftButton,Qt::NoModifier,QPoint(8,c->height()/2));check(observed_signal.count()>0,"Exact old/current checkbox toggled signal fired");}
 else {auto* c=control<QPushButton>(w,"artboard-detach");reveal(c);QSignalSpy observed_signal(c,&QPushButton::clicked);QTest::mouseClick(c,Qt::LeftButton);check(observed_signal.count()>0,"Exact old/current Detach clicked signal fired");}
 events();
}
void flow(const QString& root,int mode){
 const int action=mode%5;const bool active=mode>=5;const auto initial=fixture();auto replacement=initial;replacement.id+="-replacement";
 replacement.compositions.front().artboards.at(1).width=1600;replacement.compositions.front().artboards.at(1).height=1100;replacement.compositions.front().artboards.at(2).width=1900;replacement.compositions.front().artboards.at(2).height=1300;replacement.objects.at("sentinel").contours.front().points.front().x.literal+=7;valid(replacement);
 Session original(initial),idle(replacement),foreign(replacement);auto pending=replacement;pending.compositions.front().artboards.front().name="Caller pending";valid(pending);foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",pending.compositions.front().artboards.front()}});check(foreign.preview_document()==pending&&foreign.document()==replacement,"Independent foreign caller full preview BEFORE Window");
 auto fresh_base=active?foreign:idle;if(active)fresh_base.cancel_gesture();const auto intended=goal(replacement,action);auto fresh=fresh_base;fresh.apply(commands(intended,action),0);check(fresh.document()==intended,"Independent typed Parent command equals manual full intended Document BEFORE Window");
 auto undone=fresh;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 auto named_doc=intended;named_doc.compositions.front().artboards.front().name="Current frame";valid(named_doc);auto named=redone;named.apply({UpdateArtboard{"composition",named_doc.compositions.front().artboards.front()}},named.revision());check(named.document()==named_doc,"Independent current Frame name shared-apply consumer BEFORE Window");
 auto numeric_doc=named_doc;numeric_doc.compositions.front().artboards.front().x=33.5;valid(numeric_doc);auto numeric=named;numeric.apply({UpdateArtboard{"composition",numeric_doc.compositions.front().artboards.front()}},numeric.revision());check(numeric.document()==numeric_doc,"Independent current Frame numeric shared-apply consumer BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial.nect",initial);write(path+"-replacement.nect",replacement);write(path+"-pending.nect",pending);write(path+"-parent-expected.nect",intended);write(path+"-consumers-expected.nect",numeric_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=original;w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();same(w.host.session,original);
 const auto sid=w.host.session_id;w.host.session=active?foreign:idle;check(w.host.session_id==sid&&w.host.session.revision()==0,"Replacement Document exact same Session ID/revision");same(w.host.session,active?foreign:idle);
 act(w,action);std::cout<<"PARENT_DOCUMENT mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual native refusal status retained");same(w.host.session,active?foreign:idle);check(w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Old Parent controls explicitly refuse another Document");
 if(active)w.host.session.cancel_gesture();same(w.host.session,fresh_base);w.host.edited();events();act(w,action);same(w.host.session,fresh);
 w.host.save(path+"-parent-saved.nect");w.host.flush();events();same(w.host.session,fresh);check(read(path+"-parent-saved.nect")==read(path+"-parent-expected.nect"),"Exact refreshed Parent native bytes");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.host.edited();events();
 auto* name=control<QLineEdit>(w,"artboard-name");reveal(name);name->setFocus();QTest::keyClick(name,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(name,"Current frame");QTest::keyClick(name,Qt::Key_Return);events();same(w.host.session,named);
 auto* x=control<QLineEdit>(w,"artboard-x");reveal(x);x->setFocus();QTest::keyClick(x,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(x,"33.5");QTest::keyClick(x,Qt::Key_Return);events();same(w.host.session,numeric);
 w.host.save(path+"-consumers-saved.nect");w.host.flush();events();same(w.host.session,numeric);check(read(path+"-consumers-saved.nect")==read(path+"-consumers-expected.nect"),"Exact current shared-apply consumers native bytes");w.close();events();same(w.host.session,numeric);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_PARENT_DOCUMENT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<10;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Parent Document cases failed: "+std::to_string(failures));std::cout<<"PASS Parent Document "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Parent Document "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
