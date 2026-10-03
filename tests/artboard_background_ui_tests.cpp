#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QColorDialog>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
template<class T>T* visible(Window& w,const char* name){for(auto* widget:w.findChildren<T*>(name))if(widget->isVisible())return widget;throw std::runtime_error(std::string("Missing ")+name);}
void select(Window& w,const Id& id){w.canvas->set_active_artboard("comp",id,false);events();visible<QPushButton>(w,"artboard-edit")->click();events();}
struct Snapshot{Document d;std::uint64_t r;HistoryInfo h;std::string n;explicit Snapshot(const Session& s):d(s.document()),r(s.revision()),h(s.history()),n(encode(s.document())){}bool same(const Session& s)const{return d==s.document()&&r==s.revision()&&h==s.history()&&n==encode(s.document());}};
Document fixture(){auto d=empty_document("background-ui","comp","source");d.compositions.front().artboards.push_back({"target","Target",1100,0,960,640});ColorValue c;c.rgba={.123456789012345,.43210987654321,.7654321098765,.87654321098765};d.compositions.front().artboards.front().background=c;Session s(d);s.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"template","Shared","source",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target","template",{}}}},0);return s.document();}
void dialog(Window& w,bool accept,const QColor* changed=nullptr,const std::function<void()>& during={}){bool seen=false;QTimer::singleShot(0,&w,[&]{auto* d=qobject_cast<QColorDialog*>(QApplication::activeModalWidget());if(!d){QApplication::closeAllWindows();return;}seen=true;if(during)during();if(changed)d->setCurrentColor(*changed);if(accept)d->accept();else d->reject();});visible<QPushButton>(w,"artboard-background-color")->click();events();check(seen,"Real Qt color dialog opened from Artboard Inspector");}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);try{
 QTemporaryDir files;QSettings settings(files.filePath("settings.ini"),QSettings::IniFormat);Window w(files.path(),std::make_unique<FolderLibrary>(settings));w.resize(1200,900);w.show();events();w.host.session=Session(fixture());w.host.session_id+="-fixture";w.host.edited();events();auto& s=w.host.session;
 select(w,"source");const Snapshot precise(s);dialog(w,false);check(precise.same(s),"Color Cancel preserves exact native bytes, revision and History");dialog(w,true);check(precise.same(s),"Unchanged QColor acceptance preserves full authored precision without History");
 const QColor chosen(12,34,56,78);dialog(w,true,&chosen);check(s.revision()==precise.r+1&&evaluate_artboard(s.document().compositions.front(),"source").background->rgba==std::array<double,4>{chosen.redF(),chosen.greenF(),chosen.blueF(),chosen.alphaF()},"Color selection commits the chosen RGBA once");s.undo(s.revision());w.host.edited();events();check(s.document()==precise.d,"One Undo restores original exact color");
 select(w,"target");check(visible<QLabel>(w,"artboard-background-state")->text().startsWith("Inherited"),"Inspector identifies Template inheritance");visible<QPushButton>(w,"artboard-background-none")->click();events();check(!evaluate_artboard(s.document().compositions.front(),"target").background&&artboard_background_state(s.document().compositions.front(),"target").overridden,"None creates explicit local override");check(visible<QPushButton>(w,"artboard-background-reset")->isEnabled(),"Reset is available for local override");visible<QPushButton>(w,"artboard-background-reset")->click();events();check(artboard_background_state(s.document().compositions.front(),"target").inherited,"Reset restores inherited color");
 select(w,"source");std::unique_ptr<Snapshot> swapped;dialog(w,true,&chosen,[&]{w.host.session=Session(fixture());w.host.session_id+="-replacement";swapped=std::make_unique<Snapshot>(w.host.session);});check(swapped&&swapped->same(s),"Stale color dialog cannot mutate replacement Session");check(visible<QLabel>(w,"artboard-background-error")->text().contains("another document"),"Stale Session refusal is visible");
 std::cout<<"PASS "<<checks<<" Artboard Inspector Color/None/Reset/Cancel/precision/stale-Session Qt smoke checks (physical OS input NOT_RUN)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
