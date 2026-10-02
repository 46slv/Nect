#include "window.hpp"
#include "artboard_guide_alignment_fixture.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QFile>
#include <QProcess>
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
template<class T>T* control(Window&w,const char*name){for(auto* item:w.findChildren<T*>(name))if(item->isVisible())return item;throw std::runtime_error(std::string("missing visible control ")+name);}
void edit(Window&w,const QString&board,const QString&guide){
 auto* list=w.findChild<QListWidget*>("artboards");check(list,"Artboard list available");bool found=false;
 for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()==board){list->setCurrentItem(list->item(i));found=true;break;}
 check(found,"stable frame found");events();control<QPushButton>(w,"artboard-edit")->click();events();
 auto* combo=control<QComboBox>(w,"artboard-guide-selector");auto index=combo->findData(guide);check(index>=0,"stable occurrence found");combo->setCurrentIndex(index);events();
}
double pos(Window&w,const Id&board,const Id&guide){for(const auto&g:effective_artboard_guides(w.host.session.document(),"comp",board))if(g.guide_id==guide)return g.position;throw std::runtime_error("missing Guide");}
QPoint point(Window&w,double x,double y){const auto b=evaluate_artboard(w.host.session.document().compositions.front(),w.canvas->active_artboard());return {qRound(w.canvas->width()/2.0+(x-b.x-b.width/2)*w.canvas->zoom()),qRound(w.canvas->height()/2.0+(y-b.y-b.height/2)*w.canvas->zoom())};}
}
int main(int argc,char**argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
 check(argc==2,"core CLI path required");QTemporaryDir dir;check(dir.isValid(),"scratch directory available");QSettings settings(dir.filePath("library.ini"),QSettings::IniFormat);
 Window w(dir.path(),std::make_unique<FolderLibrary>(settings));w.host.session=Session(guide_alignment_fixture::document());w.refresh();w.show();events();
 edit(w,"B","GX");w.canvas->fit_artboard();events();auto* drag=control<QPushButton>(w,"artboard-guide-drag");check(drag->text().contains("override"),"inherited action explains position override");
 drag->click();events();check(w.canvas->guide_edit_mode(),"public button arms Guide mode");const auto before=encode(w.host.session.document());const auto p=point(w,2040,110);const auto delta=qRound(20*w.canvas->zoom());
 QTest::mousePress(w.canvas,Qt::LeftButton,Qt::NoModifier,p);QTest::mouseMove(w.canvas,p+QPoint(delta,0));events();check(encode(w.host.session.document())==before,"Window drag preview does not commit");
 QTest::mouseRelease(w.canvas,Qt::LeftButton,Qt::NoModifier,p+QPoint(delta,0));events();check(std::abs(pos(w,"B","GX")-(40+delta/w.canvas->zoom()))<1e-6,"public drag targets selected B occurrence");check(pos(w,"A","GX")==90&&pos(w,"source","GX")==40,"sibling and source preserved");check(w.host.session.revision()==1&&!w.canvas->guide_edit_mode(),"release commits once and disarms");
 edit(w,"A","local");check(control<QPushButton>(w,"artboard-guide-drag")->text().contains("local"),"local action labels authored edit");w.canvas->set_show_guides(false);control<QPushButton>(w,"artboard-guide-drag")->click();events();check(w.statusBar()->currentMessage().contains("GUIDES_HIDDEN")&&!w.canvas->guide_edit_mode(),"hidden overlays refusal through public button");w.canvas->set_show_guides(true);
 edit(w,"A","GX");auto* stale=control<QPushButton>(w,"artboard-guide-drag");w.host.session.apply({ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",100.0}}},w.host.session.revision());stale->click();events();check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT")&&!w.canvas->guide_edit_mode(),"stale Inspector action cannot arm newer revision");w.host.edited();events();
 edit(w,"A","GX");control<QPushButton>(w,"artboard-guide-drag")->click();events();check(w.canvas->guide_edit_mode(),"repeat action arms fresh occurrence");QTest::keyClick(w.canvas,Qt::Key_Escape);events();check(!w.canvas->guide_edit_mode(),"public Escape cancels armed-only state");
 const auto file=dir.filePath("scoped-drag.nect");w.host.save(file);QFile saved(file);check(saved.open(QIODevice::ReadOnly),"Host save readable");const auto bytes=saved.readAll();check(bytes.toStdString()==encode(w.host.session.document()),"Host save equals canonical authored state");
 QProcess cold;cold.start(QString::fromLocal8Bit(argv[1]),QStringList{"--validate"});check(cold.waitForStarted(5000),"cold process started");cold.write(bytes);cold.closeWriteChannel();check(cold.waitForFinished(10000)&&cold.exitStatus()==QProcess::NormalExit&&cold.exitCode()==0,"cold native validation succeeds");
 std::cout<<"PASS "<<checks<<" scoped Guide Window/Host checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
