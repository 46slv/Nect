#include "window.hpp"
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
void type(QLineEdit* input,const char* text){reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);events();}
void flow(const QString& root,int mode){
 const bool inherited=mode>=4;const int f=mode%4;const auto initial=fixture(inherited?1:0);
 const auto first_doc=goal(initial,f,inherited,0.5),recovered_doc=goal(initial,f,inherited,-0.5);
 Session before(initial),first(initial);first.begin_gesture(0);first.update_gesture(commands(first_doc,inherited));
 check(first.preview_document()==first_doc&&first.document()==initial,"Independent valid first preview BEFORE Window");
 auto invalid=first;invalid.cancel_gesture();auto recovered=invalid;recovered.begin_gesture(0);recovered.update_gesture(commands(recovered_doc,inherited));
 auto committed=recovered;committed.commit_gesture();auto undone=committed;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 auto external=initial.compositions.front().artboards.front();external.name="Foreign status caller";auto pending=initial;pending.compositions.front().artboards.front().name=external.name;valid(pending);
 Session foreign(initial);foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",external}});
 check(foreign.preview_document()==pending,"Independent full foreign feedback refusal BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",recovered_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,inherited?1:0);same(w.host.session,before);
 auto* input=control<QLineEdit>(w,names[f]);type(input,"+=0.5");same(w.host.session,first);
 std::cout<<"VALID_FEEDBACK mode="<<mode<<" draft="<<input->text().toStdString()<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";
 check(input->text()=="+=0.5","Valid exact editable draft preserved");check(w.statusBar()->grab().save(path+"-first-status.png"),"Actual valid-preview status render");
 check(w.statusBar()->currentMessage().isEmpty(),"Valid current Margin preview clears stale incomplete-prefix error");
 type(input,"+=");same(w.host.session,invalid);check(input->text()=="+="&&w.statusBar()->currentMessage().contains("INVALID_VALUE"),"Genuine incomplete refusal still shown and author/History preserved");
 type(input,"-=0.5");same(w.host.session,recovered);check(w.statusBar()->currentMessage().isEmpty(),"Correction recovers exact valid own preview/status");
 QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,committed);
 w.host.save(path+"-saved.nect");w.host.flush();same(w.host.session,committed);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Exact corrected Margin native values and siblings");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);
 w.host.session=Session(initial);w.host.session_id+="-foreign";w.host.edited();events();input=control<QLineEdit>(w,names[f]);
 w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",external}});
 type(input,"+=0.5");same(w.host.session,foreign);check(w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Actual foreign refusal never cleared as valid own feedback");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{
 std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");
 const auto requested=qEnvironmentVariable("NECT_MARGIN_STATUS_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;
 for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
 if(failures)throw std::runtime_error("Independent feedback cases failed: "+std::to_string(failures));std::cout<<"PASS Margin feedback "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL Margin feedback "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
