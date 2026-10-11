#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QProcess>
#include <QPointer>
#include <QStatusBar>
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
    auto d=empty_document("margin-right-relative-document","composition","board");
    auto& b=d.compositions.front().artboards.front();b.width=1200;b.height=900;
    b.layout=ArtboardLayout{Margin{10.123456789012344,20.123456789012344,30.987654321098766,40.543210987654323},Grid{"grid",{15,20,1000,800},3,2,7,8}};
    Object sentinel;sentinel.id="sentinel";sentinel.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};sentinel.contours={c};
    d.objects.emplace(sentinel.id,sentinel);d.compositions.front().roots={sentinel.id};
    if(mode==1){
        d.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});
        Session seeded(d);seeded.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},
            ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seeded.document();
    }else if(mode==2){
        b.layout->margin->top_expression=Expression{"21",1};
        b.layout->margin->left_driver=Ref{"driver","","artboard.width"};
        d.compositions.front().artboards.push_back({"driver","Driver",1500,0,30.987654321098766,900});
    }
    validate(d);check(decode(encode(d))==d,"Independent initial complete Document native-validates BEFORE Window");return d;
}
Document intended(const Document& initial,int mode,double left){
    auto d=initial;
    if(mode==1){
        const auto resolved=evaluate_artboard(initial.compositions.front(),"target").layout->margin.value();
        auto& target=d.compositions.front().artboards.back();
        target.layout=ArtboardLayout{Margin{resolved.left,resolved.top,left,resolved.bottom},{}};
        target.template_assignment->margin_overridden=true;
    }else d.compositions.front().artboards.front().layout->margin->right=left;
    validate(d);check(decode(encode(d))==d,"Independent intended full Document native-validates BEFORE Window");return d;
}
std::vector<Command> commands(const Document& wanted,int mode){
    if(mode==1)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.margin",wanted.compositions.front().artboards.back().layout->margin}}};
    return {SetArtboardLayout{"composition","board",wanted.compositions.front().artboards.front().layout}};
}
void same(const Session& a,const Session& b){
    if(!(a.document()==b.document()&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))
        std::cerr<<"Snapshot mismatch document="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" revision="<<a.revision()<<"/"<<b.revision()<<" gesture="<<a.gesture_active()<<"/"<<b.gesture_active()<<" generation="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
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
    bool present=false;for(auto* input:window.findChildren<QLineEdit*>("margin-right"))present=present||input->isVisible();
    if(!present){auto* edit=control<QPushButton>(window,"artboard-edit");reveal(edit);edit->click();events();}
    auto* input=control<QLineEdit>(window,"margin-right");reveal(input);input->setFocus();
    if(keyboard){QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);}
    else {input->setText(text);check(QMetaObject::invokeMethod(input,"textEdited",Qt::DirectConnection,Q_ARG(QString,text)),"Owned draft notification delivered");}
    events();check(input->text()==text,"Raw draft remains available"); 
}
void cold(const QString& root,int mode){
    const auto expected_doc=intended(fixture(mode),mode,fixture(mode).compositions.front().artboards.front().layout->margin->right+0.5);Session expected(expected_doc);const auto prefix=root+"/mode-"+QString::number(mode);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(prefix+"-cold",std::make_unique<FolderLibrary>(settings),&settings);
    window.host.open(prefix+"-cold-input.nect");same(window.host.session,expected);
    window.host.save(prefix+"-resaved.nect");window.host.flush();same(window.host.session,expected);
    check(read(prefix+"-cold-input.nect")==read(prefix+"-expected.nect")&&read(prefix+"-resaved.nect")==read(prefix+"-expected.nect"),"Separate cold native resave equals retained Save and independent expected bytes");
}
void enter(Window& w){QTest::keyClick(control<QLineEdit>(w,"margin-right"),Qt::Key_Return);events();}
void cancel(Window& w){control<QPushButton>(w,"margin-right-cancel")->click();events();}
void authored_same(const Session& a,const Session& b){check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision(),"Rejected relative draft preserves full authored Document/History/revision");}
void flow(const QString& root,int mode){
    const auto initial=fixture(mode);const auto base=initial.compositions.front().artboards.front().layout->margin->right;
    const auto wanted=intended(initial,mode,base+0.5);Session expected(initial);
    auto preview=expected;preview.begin_gesture(0);preview.update_gesture(commands(wanted,mode));
    // Ctrl+A retires the previous layout preview; the new gesture still uses the captured authored base.
    auto repeated=preview;repeated.cancel_gesture();repeated.begin_gesture(0);repeated.update_gesture(commands(wanted,mode));
    auto committed=repeated;committed.commit_gesture();check(committed.document()==wanted,"Independent canonical command and intended complete Document agree BEFORE Window");
    const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",wanted);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,mode);
    same(window.host.session,expected);draft(window,"+=0");enter(window);same(window.host.session,expected);
    draft(window,"+=0.5");same(window.host.session,preview);draft(window,"+=0.5");same(window.host.session,repeated);enter(window);same(window.host.session,committed);
    check(control<QLineEdit>(window,"margin-right")->toolTip().contains("+="),"Right inset documents one-shot relative syntax");
    auto& session=window.host.session;session.undo(session.revision());committed.undo(committed.revision());window.host.edited();events();same(session,committed);
    check(session.document()==initial&&!session.can_undo(),"One Undo restores exact initial state and inheritance");
    session.redo(session.revision());committed.redo(committed.revision());window.host.edited();events();same(session,committed);
    window.host.save(prefix+"-saved.nect");window.host.flush();same(session,committed);
    check(QFile::copy(prefix+"-saved.nect",prefix+"-cold-input.nect"),"Actual saved bytes retained before later edits");
    QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned separate cold process starts");
    if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}
    std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Separate cold native relative acceptance passes");
    const auto restored=intended(wanted,mode,base);draft(window,"-=0.5");committed.begin_gesture(committed.revision());committed.update_gesture(commands(restored,mode));same(session,committed);enter(window);committed.commit_gesture();same(session,committed);
    check(evaluate_artboard(session.document().compositions.front(),mode==1?"target":"board").layout->margin->right==base,"Subtract restores exact captured base");
    for(const auto* invalid:{"+=","-=","+=nan","+=inf","+=1e309","-=100","-1","+=1200"}){
        const auto before=session;draft(window,invalid,false);enter(window);authored_same(session,before);check(control<QLineEdit>(window,"margin-right")->text()==invalid,"Invalid/layout/nonfinite relative raw draft retained");cancel(window);
    }
    auto canceled=session;canceled.begin_gesture(canceled.revision());canceled.update_gesture(commands(intended(session.document(),mode,base+0.5),mode));canceled.cancel_gesture();draft(window,"+=0.5");cancel(window);same(session,canceled);
    draft(window,"+=0.5");session.cancel_gesture();auto incoming=session.document().compositions.front().artboards.front();incoming.name="Incoming source";session.apply({UpdateArtboard{"composition",incoming}},session.revision());const auto newer=session;enter(window);same(session,newer);
    check(window.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Stale Right draft refuses newer canonical revision");
}
void driven(const QString& root,bool linked){
    auto initial=fixture(0);auto& margin=*initial.compositions.front().artboards.front().layout->margin;
    if(linked){margin.right_driver=Ref{"driver","","artboard.width"};initial.compositions.front().artboards.push_back({"driver","Driver",1500,0,4,900});}else margin.right_expression=Expression{"4",1};
    validate(initial);check(decode(encode(initial))==initial,"Driven Right fixture independently native-validates BEFORE Window");Session expected(initial);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(root+(linked?"/linked":"/expression"),std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,0);
    check(control<QLineEdit>(window,"margin-right")->isReadOnly(),"Driven Right stays read-only");draft(window,"+=0.5",false);enter(window);control<QPushButton>(window,"margin-apply")->click();events();same(window.host.session,expected);
}
void overflow(const QString& root){
    auto initial=fixture(0);
        validate(initial);check(decode(encode(initial))==initial,"Finite large-input rejection fixture native-validates BEFORE Window");Session expected(initial);
    auto invalid=initial;invalid.compositions.front().artboards.front().layout->margin->right+=1e308;
    // textEdited preview and Return commit each attempt the same invalid command.
    // Both cancel their owned gesture; authored state remains unchanged.
    for(int attempt=0;attempt<2;++attempt){expected.begin_gesture(0);bool rejected=false;
        try{expected.update_gesture(commands(invalid,0));}catch(const Error& e){rejected=e.code=="INVALID_LAYOUT";}
        expected.cancel_gesture();check(rejected,"Independent canonical invalid-layout attempt rejects BEFORE Window");}
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(root+"/overflow",std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,0);draft(window,"+=1e308",false);enter(window);same(window.host.session,expected);
    check(control<QLineEdit>(window,"margin-right")->text()=="+=1e308"&&window.statusBar()->currentMessage().contains("INVALID_LAYOUT"),"Finite huge amount preserves draft and rejects nonpositive content layout explicitly");
}
void explicit_template_apply(const QString& root){
    const auto initial=fixture(1);const auto wanted=intended(initial,1,initial.compositions.front().artboards.front().layout->margin->right);
    Session before(initial),expected(initial);expected.begin_gesture(0);expected.update_gesture(commands(wanted,1));expected.commit_gesture();check(expected.document()==wanted,"Independent explicit Template family override BEFORE Window");
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(root+"/explicit-template",std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,1);
    draft(window,"+=0");enter(window);same(window.host.session,before);control<QPushButton>(window,"margin-apply")->click();events();same(window.host.session,expected);
    check(window.host.session.document().compositions.front().artboards.back().template_assignment->margin_overridden,"Explicit Apply still authors inherited Margin family override");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();same(window.host.session,expected);check(window.host.session.document()==initial,"Explicit Apply Undo restores inheritance");
}
void replacement_session(const QString& root){
    const auto initial=fixture(0);auto replacement=initial;replacement.compositions.front().artboards.front().name="Replacement Session";check(decode(encode(replacement))==replacement,"Independent replacement complete Document native-validates BEFORE Window");Session expected(replacement);write(root+"/replacement.nect",replacement);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(root+"/replacement-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,0);draft(window,"+=0.5");const auto old_session=window.host.session_id;QPointer<QLineEdit> old_editor=control<QLineEdit>(window,"margin-right");window.host.session.cancel_gesture();window.host.open(root+"/replacement.nect");
    check(window.host.session_id!=old_session&&window.host.session.revision()==0,"Actual Host.open replaces Session at same IDs/revision0");events();same(window.host.session,expected);check(!old_editor||!old_editor->isVisible(),"Actual Host.open retires old Right editor");
    auto* edit=control<QPushButton>(window,"artboard-edit");reveal(edit);edit->click();events();same(window.host.session,expected);check(control<QLineEdit>(window,"margin-right")->text()==QString::number(initial.compositions.front().artboards.front().layout->margin->right,'g',15),"Fresh Right reads replacement Session");
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);
    try{
        std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";
        if(argc==4&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));
        else {QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MARGIN_RIGHT_RELATIVE_ROOT");const auto root=requested.isEmpty()?temporary.path():requested;flow(root,0);flow(root,1);flow(root,2);driven(root,false);driven(root,true);overflow(root);replacement_session(root);explicit_template_apply(root);}
        std::cout<<"PASS Margin Right relative "<<checks<<" checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL Margin Right relative "<<e.what()<<" after "<<checks<<" checks\n";return 1;}
}
