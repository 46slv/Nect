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
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const char* name){for(auto* p:w.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(name);}
void reveal(QWidget* item){for(auto* p=item->parentWidget();p;p=p->parentWidget())if(auto* scroll=qobject_cast<QScrollArea*>(p))scroll->ensureWidgetVisible(item);events();check(item->isVisible(),"Production control visible");}
Document fixture(int mode){
    auto d=empty_document("margin-precision-document","composition","board");
    auto& b=d.compositions.front().artboards.front();b.width=1200;b.height=900;
    b.layout=ArtboardLayout{Margin{10,20.123456789012344,30.987654321098766,40.543210987654323},Grid{"grid",{15,20,1000,800},3,2,7,8}};
    Object sentinel;sentinel.id="sentinel";sentinel.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};sentinel.contours={c};
    d.objects.emplace(sentinel.id,sentinel);d.compositions.front().roots={sentinel.id};
    if(mode==1){
        d.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});
        Session seeded(d);seeded.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},
            ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seeded.document();
    }else if(mode==2){
        b.layout->margin->top_expression=Expression{"21",1};
        b.layout->margin->right_driver=Ref{"driver","","artboard.width"};
        d.compositions.front().artboards.push_back({"driver","Driver",1500,0,30.987654321098766,900});
    }
    validate(d);check(decode(encode(d))==d,"Independent initial complete Document native-validates BEFORE Window");return d;
}
Document intended(const Document& initial,int mode,double left){
    auto d=initial;
    if(mode==1){
        const auto resolved=evaluate_artboard(initial.compositions.front(),"target").layout->margin.value();
        auto& target=d.compositions.front().artboards.back();
        target.layout=ArtboardLayout{Margin{left,resolved.top,resolved.right,resolved.bottom},{}};
        target.template_assignment->margin_overridden=true;
    }else d.compositions.front().artboards.front().layout->margin->left=left;
    validate(d);check(decode(encode(d))==d,"Independent intended full Document native-validates BEFORE Window");return d;
}
std::vector<Command> commands(const Document& wanted,int mode){
    if(mode==1)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.margin",wanted.compositions.front().artboards.back().layout->margin}}};
    return {SetArtboardLayout{"composition","board",wanted.compositions.front().artboards.front().layout}};
}
void same(const Session& a,const Session& b){
    check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&
        a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),
        "Exact complete Document/native/preview/History/revision/gesture equals independent canonical Session");
}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent frozen oracle written once");}
void open(Window& window,const Document& initial,int mode){
    window.host.session=Session(initial);window.host.edited();window.canvas->set_active_artboard("composition",mode==1?"target":"board",false);
    window.resize(1400,1000);window.show();window.activateWindow();events();
    auto* dock=window.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
    auto* edit=control<QPushButton>(window,"artboard-edit");reveal(edit);edit->click();events();
}
void draft(Window& window,const QString& text,bool keyboard=true){
    window.activateWindow();events();
    bool present=false;for(auto* input:window.findChildren<QLineEdit*>("margin-left"))present=present||input->isVisible();
    if(!present){auto* edit=control<QPushButton>(window,"artboard-edit");reveal(edit);edit->click();events();}
    auto* input=control<QLineEdit>(window,"margin-left");reveal(input);input->setFocus();
    if(keyboard){QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);}
    else {input->setText(text);check(QMetaObject::invokeMethod(input,"textEdited",Qt::DirectConnection,Q_ARG(QString,text)),"Owned draft notification delivered");}
    events();check(input->text()==text,"Raw draft remains available"); 
}
void cold(const QString& root,int mode){
    const auto expected_doc=intended(fixture(mode),mode,15);Session expected(expected_doc);const auto prefix=root+"/mode-"+QString::number(mode);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(prefix+"-cold",std::make_unique<FolderLibrary>(settings),&settings);
    window.host.open(prefix+"-saved.nect");same(window.host.session,expected);
    window.host.save(prefix+"-resaved.nect");window.host.flush();same(window.host.session,expected);
    check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&read(prefix+"-resaved.nect")==read(prefix+"-expected.nect"),"Separate cold native resave equals retained Save and independent expected bytes");
}
void flow(const QString& root,int mode){
    const auto initial=fixture(mode),wanted=intended(initial,mode,15);Session expected(initial);
    expected.begin_gesture(0);expected.update_gesture(commands(wanted,mode));const auto preview=expected;expected.commit_gesture();
    check(expected.document()==wanted,"Independent canonical intended state agrees BEFORE Window");
    const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",wanted);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,mode);
    Session before(initial);same(window.host.session,before);draft(window,"15");same(window.host.session,preview);
    QTest::keyClick(control<QLineEdit>(window,"margin-left"),Qt::Key_Return);events();same(window.host.session,expected);
    check(window.host.session.document()==wanted,"Left-only direct input preserves precise siblings, drivers, Grid and stable IDs");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();same(window.host.session,expected);
    check(window.host.session.document()==initial&&!window.host.session.can_undo(),"One Undo restores exact initial state");
    window.host.session.redo(window.host.session.revision());expected.redo(expected.revision());window.host.edited();events();same(window.host.session,expected);
    window.host.save(prefix+"-saved.nect");window.host.flush();same(window.host.session,expected);
    QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned separate cold process starts");
    if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}
    std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();
    check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Separate cold native acceptance passes");
    draft(window,"nan",false);QTest::keyClick(control<QLineEdit>(window,"margin-left"),Qt::Key_Return);events();same(window.host.session,expected);
    check(control<QLineEdit>(window,"margin-left")->text()=="nan","Invalid nonfinite input retains draft and exact authored state");
    control<QPushButton>(window,"margin-left-cancel")->click();events();same(window.host.session,expected);
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);
    try{
        std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";
        if(argc==4&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));
        else {QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MARGIN_PRECISION_ROOT");const auto root=requested.isEmpty()?temporary.path():requested;flow(root,0);flow(root,1);flow(root,2);}
        std::cout<<"PASS Margin precision "<<checks<<" checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL Margin precision "<<e.what()<<" after "<<checks<<" checks\n";return 1;}
}
