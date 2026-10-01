#include "window.hpp"
#include "inherited_grid_alignment_fixture.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void near(double a,double b){check(std::abs(a-b)<1e-8,"wrong Window Grid coordinate");}
void events(){QApplication::processEvents();}
template<class T>T* widget(Window& w,const char* name){for(auto* p:w.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(name);}
void choose(Window& w,const char* grid){
    auto* combo=widget<QComboBox>(w,"alignment-target");const auto index=combo->findData(QString("grid:")+grid);
    check(index>=0,"effective Grid appears in public selector");combo->setCurrentIndex(index);events();
}
void click(Window& w,const char* name){auto* button=widget<QPushButton>(w,name);check(button->isEnabled(),"requested Grid action enabled");button->click();events();}
QAction* action(Window& w,const QString& name){for(auto* a:w.findChildren<QAction*>())if(a->text()==name)return a;throw std::runtime_error("History action absent");}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
 try{
    check(argc==2,"core CLI path required");QTemporaryDir dir;check(dir.isValid(),"owned scratch exists");
    QSettings settings(dir.filePath("library.ini"),QSettings::IniFormat);
    Window w(dir.path(),std::make_unique<FolderLibrary>(settings));w.host.session=Session(inherited_grid_fixture::document());
    w.canvas->set_active_artboard("comp","A",false);w.canvas->set_selection("a");w.refresh();w.show();events();
    const auto original=encode(w.host.session.document());choose(w,"template-grid-A");click(w,"quick-align-x-min");
    near(inherited_grid_fixture::minimum(w.host.session.document(),"a"),1040);
    action(w,"Undo")->trigger();events();check(encode(w.host.session.document())==original,"public Undo restores original");
    w.canvas->set_selections({{"c",""},{"a",""},{"b",""}});w.refresh();events();choose(w,"template-grid-A");click(w,"quick-distribute-x");
    near(inherited_grid_fixture::minimum(w.host.session.document(),"a"),1055);near(inherited_grid_fixture::minimum(w.host.session.document(),"b"),1080);near(inherited_grid_fixture::minimum(w.host.session.document(),"c"),1115);
    choose(w,"template-grid-B");click(w,"quick-distribute-x");
    near(inherited_grid_fixture::minimum(w.host.session.document(),"a"),2055);near(inherited_grid_fixture::minimum(w.host.session.document(),"b"),2080);near(inherited_grid_fixture::minimum(w.host.session.document(),"c"),2115);
    w.host.session.apply({ReorderArtboards{"comp",{"B","source","A"}}},w.host.session.revision());w.host.edited();events();
    check(widget<QComboBox>(w,"alignment-target")->currentData().toString()=="grid:template-grid-B","reorder preserves exact Grid selection");
    choose(w,"template-grid-A");click(w,"quick-distribute-y");
    near(inherited_grid_fixture::minimum(w.host.session.document(),"a",false),240);near(inherited_grid_fixture::minimum(w.host.session.document(),"b",false),260);near(inherited_grid_fixture::minimum(w.host.session.document(),"c",false),290);
    const auto distributed=encode(w.host.session.document());action(w,"Undo")->trigger();events();action(w,"Redo")->trigger();events();
    check(encode(w.host.session.document())==distributed,"Undo/Redo restores exact Grid distribution");
    choose(w,"template-grid-A");w.host.session.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","layout.grid",std::optional<Grid>{}}}},w.host.session.revision());
    const auto suppressed=encode(w.host.session.document());w.host.edited();events();auto* selector=widget<QComboBox>(w,"alignment-target");
    check(selector->findData("grid:template-grid-A")==-1&&selector->currentData().toString()=="selection","suppressed Grid disappears and stale selection resets");
    check(selector->findData("grid:template-grid-B")>=0&&encode(w.host.session.document())==suppressed,"refresh preserves other occurrence and authored bytes");
    const auto path=dir.filePath("grid.nect");w.host.save(path);QFile file(path);check(file.open(QIODevice::ReadOnly),"native Host Save succeeds");
    const auto bytes=file.readAll();check(bytes.toStdString()==encode(w.host.session.document()),"saved bytes match canonical state");
    QProcess cold;cold.start(QString::fromLocal8Bit(argv[1]),QStringList{"--validate"});check(cold.waitForStarted(5000),"cold process starts");
    cold.write(bytes);cold.closeWriteChannel();check(cold.waitForFinished(10000)&&cold.exitStatus()==QProcess::NormalExit&&cold.exitCode()==0,"cold process validates native result");
    QProcess reopened;reopened.start(QString::fromLocal8Bit(argv[1]),QStringList{"--serve",path});
    check(reopened.waitForStarted(5000),"fresh authoring process starts");
    reopened.write(R"({"op":"apply","expected_revision":0,"commands":[{"type":"align_objects","objects":["a"],"axis":"x","alignment":"min","reference":"grid:template-grid-B"}]})");
    reopened.write("\n");reopened.write(R"({"op":"get","ref":{"object":"a","point":"","field":"transform.tx"}})");reopened.write("\n");reopened.closeWriteChannel();
    check(reopened.waitForFinished(10000)&&reopened.exitCode()==0,"cold authoring requests complete");
    const auto replies=reopened.readAllStandardOutput().trimmed().split('\n');
    check(replies.size()==2&&QJsonDocument::fromJson(replies[0]).object().value("ok").toBool(),"cold inherited Grid command succeeds");
    near(QJsonDocument::fromJson(replies[1]).object().value("result").toObject().value("evaluated").toDouble(),2040);
    file.close();check(file.open(QIODevice::ReadOnly)&&file.readAll()==bytes,"cold authoring leaves input native file unchanged");
    std::cout<<"PASS "<<checks<<" inherited Grid Window/Host checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
