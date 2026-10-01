#include "window.hpp"
#include "artboard_guide_alignment_fixture.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void events(){QApplication::processEvents();}
template<class T>T* child(Window& w,const char* name){
    for(auto* item:w.findChildren<T*>(name))if(item->isVisible())return item;
    throw std::runtime_error(std::string("missing visible control ")+name);
}
void select(Window& w,const QString& board,const QString& guide){
    auto* combo=child<QComboBox>(w,"alignment-target");
    for(int i=0;i<combo->count();++i)if(combo->itemData(i).toString()=="guide:"+guide&&
        combo->itemData(i,Qt::UserRole+1).toString()==board){
        check(combo->itemText(i).contains(board),"scoped reference label identifies target frame");
        combo->setCurrentIndex(i);events();return;
    }
    throw std::runtime_error("scoped Guide reference absent");
}
void left(Window& w,double expected){
    auto* button=child<QPushButton>(w,"quick-align-x-min");check(button->isEnabled(),"matching axis enabled");
    check(!child<QPushButton>(w,"quick-align-y-min")->isEnabled(),"wrong axis disabled");
    check(!child<QPushButton>(w,"quick-distribute-x")->isEnabled(),"Guide distribution stays disabled");
    button->click();events();check(std::abs(guide_alignment_fixture::bounds(w.host.session.document()).left-expected)<1e-8,"button aligns exact scoped coordinate");
}
QAction* action(Window& w,const QString& name){for(auto* a:w.findChildren<QAction*>())if(a->text()==name)return a;throw std::runtime_error("missing History action");}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try{
        check(argc==2,"core CLI path required for fresh-process native validation");
        QTemporaryDir dir;check(dir.isValid(),"owned scratch available");
        QSettings settings(dir.filePath("library.ini"),QSettings::IniFormat);
        Window w(dir.path(),std::make_unique<FolderLibrary>(settings));
        w.host.session=Session(guide_alignment_fixture::document());
        w.canvas->set_active_artboard("comp","A",false);w.canvas->set_selection("rect");w.refresh();w.show();events();
        const auto original=encode(w.host.session.document());
        select(w,"A","GX");left(w,1090);const auto aligned=encode(w.host.session.document());
        action(w,"Undo")->trigger();events();check(encode(w.host.session.document())==original,"public Undo restores authored bytes");
        action(w,"Redo")->trigger();events();check(encode(w.host.session.document())==aligned,"public Redo restores authored bytes");
        w.canvas->set_selection("rect");w.refresh();events();select(w,"B","GX");left(w,2040);
        w.refresh();events();auto* combo=child<QComboBox>(w,"alignment-target");
        check(combo->currentData().toString()=="guide:GX"&&combo->currentData(Qt::UserRole+1).toString()=="B","refresh retains exact occurrence, not another same Guide");
        select(w,"A","local");left(w,1015);
        select(w,"B","GX");
        w.host.session.apply({ArtboardGuideCommand{SetArtboardGuideOverride{"comp","B","GX","enabled",false}}},w.host.session.revision());
        w.host.edited();events();combo=child<QComboBox>(w,"alignment-target");
        check(combo->currentData().toString()=="selection"&&!combo->currentData(Qt::UserRole+1).isValid(),"disabled occurrence resets selector without retargeting");
        const auto before_disabled=encode(w.host.session.document());
        check(!child<QPushButton>(w,"quick-align-x-min")->isEnabled(),"single-selection fallback cannot accidentally align");
        child<QPushButton>(w,"quick-align-x-min")->click();events();check(encode(w.host.session.document())==before_disabled,"disabled button does not mutate");
        const auto file=dir.filePath("aligned.nect");w.host.save(file);QFile saved(file);
        check(saved.open(QIODevice::ReadOnly),"Host native Save wrote owned destination");const auto bytes=saved.readAll();
        check(bytes.toStdString()==encode(w.host.session.document()),"Host save bytes match canonical native");
        QProcess cold;cold.start(QString::fromLocal8Bit(argv[1]),QStringList{"--validate"});
        check(cold.waitForStarted(5000),"fresh core process started");cold.write(bytes);cold.closeWriteChannel();
        check(cold.waitForFinished(10000)&&cold.exitStatus()==QProcess::NormalExit&&cold.exitCode()==0,"fresh process validates saved alignment");
        std::cout<<"PASS "<<checks<<" scoped Guide Window/Host checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
