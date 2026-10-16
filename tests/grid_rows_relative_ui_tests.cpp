#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QProcess>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(15);}
template<class T>T* control(Window& window,const char* name){
    for(auto* item:window.findChildren<T*>(name))if(item->isVisible())return item;
    throw std::runtime_error(std::string("Missing visible control: ")+name);
}
Document fixture(int mode){
    auto d=empty_document("rows-document","composition","source");
    auto& b=d.compositions.front().artboards.front();b.width=1200;b.height=900;
    b.layout=ArtboardLayout{Margin{10,20,30,40},Grid{"grid",{20.123456789012344,21.987654321098766,1000.1234567890123,800.98765432109872},3,2,7.123456789012344,8.987654321098766}};
    b.layout->grid->columns_expression=Expression{"3",1};
    Object path;path.id="sentinel";path.kind=Kind::path;Contour c;c.id="contour";
    Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};path.contours={c};
    d.objects.emplace(path.id,path);d.compositions.front().roots={path.id};
    if(mode){d.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});
        Session seeded(d);seeded.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","source",{}}}},
            ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seeded.document();}
    validate(d);check(decode(encode(d))==d,"Independent complete initial Document native-validates before Window");return d;
}
std::vector<Command> change(const Document& d,int mode,std::size_t rows){
    if(mode){const auto inherited=evaluate_artboard(d.compositions.front(),"target").layout->grid.value();
        Grid grid{inherited.id,inherited.bounds,inherited.columns,rows,inherited.column_gutter,inherited.row_gutter};
        return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.grid",std::optional<Grid>{grid}}}};}
    auto layout=*d.compositions.front().artboards.front().layout;layout.grid->rows=rows;
    return {SetArtboardLayout{"composition","source",layout}};
}
Document expected_rows(const Document& d,int mode,std::size_t rows){
    auto wanted=d;
    if(mode){const auto inherited=evaluate_artboard(d.compositions.front(),"target").layout->grid.value();
        auto& target=wanted.compositions.front().artboards.back();
        target.layout=ArtboardLayout{{},Grid{inherited.id,inherited.bounds,inherited.columns,rows,inherited.column_gutter,inherited.row_gutter}};
        target.template_assignment->grid_overridden=true;
    }else wanted.compositions.front().artboards.front().layout->grid->rows=rows;
    validate(wanted);check(decode(encode(wanted))==wanted,"Independent intended whole Document native-validates before Window");return wanted;
}
void same(const Session& a,const Session& b){
    if(!(a.document()==b.document()&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation())){
        std::cerr<<"Snapshot mismatch: document="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())
            <<" revision="<<a.revision()<<"/"<<b.revision()<<" gesture="<<a.gesture_active()<<"/"<<b.gesture_active()<<" generation="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
        for(const auto& state:a.history().states)std::cerr<<"actual History "<<state.id<<" "<<state.label<<" bytes="<<state.estimated_bytes<<"\n";
        for(const auto& state:b.history().states)std::cerr<<"expected History "<<state.id<<" "<<state.label<<" bytes="<<state.estimated_bytes<<"\n";
    }
    check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&
        a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&
        a.gesture_generation()==b.gesture_generation(),"Whole Document/History/revision/gesture equals independent canonical Session");
}
void authored_same(const Session& a,const Session& b){
    check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision(),
        "Rejected draft preserves entire authored Document/History/revision");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);
    check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),static_cast<qint64>(bytes.size()))==static_cast<qint64>(bytes.size()),"Owned independent native oracle saved once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native file readable");return f.readAll();}
void reveal(Window& window,QWidget* item){
    check(item&&item->isEnabled(),"Exact production control enabled");
    for(auto* parent=item->parentWidget();parent;parent=parent->parentWidget())if(auto* scroll=qobject_cast<QScrollArea*>(parent))scroll->ensureWidgetVisible(item);
    events();check(item->isVisible(),"Exact production control visible");
}
void open(Window& window,const Document& d,int mode){
    window.host.session=Session(d);window.host.edited();window.canvas->set_active_artboard("composition",mode?"target":"source",false);
    window.resize(1400,1000);window.show();window.activateWindow();events();
    auto* dock=window.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
    auto* edit=control<QPushButton>(window,"artboard-edit");reveal(window,edit);edit->click();events();
}
void draft(Window& window,const QString& text,bool keyboard=true,const char* name="grid-rows"){
    window.activateWindow();events();
    bool present=false;for(auto* item:window.findChildren<QLineEdit*>(name))present=present||item->isVisible();
    if(!present){auto* edit=control<QPushButton>(window,"artboard-edit");reveal(window,edit);edit->click();events();}
    auto* input=control<QLineEdit>(window,name);reveal(window,input);
    if(keyboard){input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);}
    else {input->setText(text);check(QMetaObject::invokeMethod(input,"textEdited",Qt::DirectConnection,Q_ARG(QString,text)),"Isolated draft notification delivered");}
    events();check(input->text()==text,"Raw Rows draft retained");
}
void enter(Window& window){QTest::keyClick(control<QLineEdit>(window,"grid-rows"),Qt::Key_Return);events();}
void cancel(Window& window){control<QPushButton>(window,"grid-rows-cancel")->click();events();}
void cold(const QString& root,int mode){
    const auto wanted=expected_rows(fixture(mode),mode,5);const auto prefix=root+"/mode-"+QString::number(mode);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(prefix+"-cold",std::make_unique<FolderLibrary>(settings),&settings);
    window.host.open(prefix+"-cold-input.nect");Session expected(wanted);same(window.host.session,expected);
    window.host.save(prefix+"-resaved.nect");window.host.flush();same(window.host.session,expected);
    check(read(prefix+"-cold-input.nect")==read(prefix+"-resaved.nect")&&read(prefix+"-resaved.nect")==read(prefix+"-expected.nect"),"Separate cold Host resave equals complete independent native bytes");
}
void flow(const QString& root,int mode){
    const auto initial=fixture(mode),five=expected_rows(initial,mode,5);
    Session expected(initial);const auto commands=change(initial,mode,5);
    auto preview=expected;preview.begin_gesture(0);preview.update_gesture(commands);
    // The existing Window key filter retires the owned preview when Ctrl's
    // unhandled press propagates outside the Grid scope. Ctrl+A re-entry starts
    // a fresh gesture at the same captured authored count, never at preview5.
    auto repeated=preview;repeated.cancel_gesture();repeated.begin_gesture(0);repeated.update_gesture(commands);
    auto committed=repeated;committed.commit_gesture();
    check(committed.document()==five,"Canonical command and independent whole intended Document agree BEFORE Window");
    const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",five);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,mode);
    same(window.host.session,expected);draft(window,"+=3");same(window.host.session,preview);
    draft(window,"+=3");same(window.host.session,repeated);enter(window);same(window.host.session,committed);
    check(control<QLineEdit>(window,"grid-rows")->toolTip().contains("+=integer"),"Rows documents the established relative integer syntax");
    auto& session=window.host.session;session.undo(session.revision());committed.undo(committed.revision());window.host.edited();events();same(session,committed);
    check(session.document()==initial&&!session.can_undo(),"One Undo restores exact initial authored state");
    session.redo(session.revision());committed.redo(committed.revision());window.host.edited();events();same(session,committed);
    window.host.save(prefix+"-saved.nect");window.host.flush();same(session,committed);
    // Subsequent edits normally persist to the attached destination. Retain the
    // actual saved cold-input bytes before continuing the subtraction/refusals.
    check(QFile::copy(prefix+"-saved.nect",prefix+"-cold-input.nect"),"Actual Host Save bytes retained for independent cold evidence");
    QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned separate cold process starts");
    if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}
    std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();
    check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Separate cold native reopen passed");
    draft(window,"-=3");committed.begin_gesture(committed.revision());committed.update_gesture(change(five,mode,2));same(session,committed);
    enter(window);committed.commit_gesture();same(session,committed);
    check(evaluate_artboard(session.document().compositions.front(),mode?"target":"source").layout->grid->rows==2,"5 -=3 commits ordinary integer2");
    for(const auto* invalid:{"+=","+=1.5","+=-1","+=999","-=2","+=9223372036854775807","+=18446744073709551616"}){
        const auto before=session;draft(window,invalid,false);enter(window);same(session,before);
        check(control<QLineEdit>(window,"grid-rows")->text()==invalid,"Invalid/out-of-range raw draft stays editable");cancel(window);
    }
    auto canceled=session;canceled.begin_gesture(canceled.revision());canceled.update_gesture(change(session.document(),mode,5));canceled.cancel_gesture();
    draft(window,"+=3");cancel(window);same(session,canceled);
    // The count range is valid, but the resulting row cell height is not.
    const auto bad_before=session;draft(window,"+=200",false);enter(window);authored_same(session,bad_before);
    check(window.statusBar()->currentMessage().contains("INVALID_LAYOUT"),"Nonpositive row cell height refuses explicitly");cancel(window);
    // An incoming canonical edit owns the newer revision. The old input cannot overwrite it.
    draft(window,"+=3");session.cancel_gesture();auto incoming=session.document().compositions.front().artboards.front();incoming.name="Incoming source";
    session.apply({UpdateArtboard{"composition",incoming}},session.revision());const auto newer=session;enter(window);same(session,newer);
    check(window.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Stale Rows draft refuses newer revision");
}
void driven(const QString& root,bool linked){
    auto initial=fixture(0);auto& grid=*initial.compositions.front().artboards.front().layout->grid;
    if(linked){Artboard source{"driver","Driver",1500,0,1200,900};source.layout=ArtboardLayout{{},Grid{"driver-grid",{0,0,1000,800},1,4,0,0}};
        grid.rows_driver=Ref{"driver-grid","","grid.rows"};initial.compositions.front().artboards.push_back(source);
    }else grid.rows_expression=Expression{"4",1};
    validate(initial);check(decode(encode(initial))==initial,"Driven whole fixture validates BEFORE Window");Session expected(initial);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(root+(linked?"/linked":"/expression"),std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,0);
    auto* input=control<QLineEdit>(window,"grid-rows");check(input->isReadOnly(),"Driven Rows remains read-only");
    draft(window,"+=3",false);enter(window);control<QPushButton>(window,"grid-apply")->click();events();same(window.host.session,expected);
}
void columns_smoke(const QString& root){
    auto initial=fixture(0);initial.compositions.front().artboards.front().layout->grid->columns_expression.reset();
    auto wanted=initial;wanted.compositions.front().artboards.front().layout->grid->columns=6;
    check(decode(encode(wanted))==wanted,"Shared parser Columns smoke independent Document validates BEFORE Window");
    Session expected(initial);expected.begin_gesture(0);expected.update_gesture({SetArtboardLayout{"composition","source",wanted.compositions.front().artboards.front().layout}});expected.commit_gesture();
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(root+"/columns",std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,0);
    draft(window,"+=3",true,"grid-columns");QTest::keyClick(control<QLineEdit>(window,"grid-columns"),Qt::Key_Return);events();same(window.host.session,expected);
}
void replacement_session(const QString& root){
    const auto initial=fixture(0);auto replacement=initial;replacement.compositions.front().artboards.front().name="Replacement Session";
    check(decode(encode(replacement))==replacement,"Independent replacement whole Document validates BEFORE Window");Session expected(replacement);
    write(root+"/replacement.nect",replacement);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window window(root+"/replacement-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(window,initial,0);
    draft(window,"+=3");const auto old_session=window.host.session_id;QPointer<QLineEdit> old_editor=control<QLineEdit>(window,"grid-rows");
    window.host.session.cancel_gesture(); // Host.open requires the caller to finish its owned gesture first.
    window.host.open(root+"/replacement.nect");
    check(window.host.session_id!=old_session&&window.host.session.revision()==0,"Actual Host.open replaces Session at matching IDs and revision0");
    events();same(window.host.session,expected);
    check(!old_editor||!old_editor->isVisible(),"Actual Host.open retires the old Rows editor instead of committing its draft");
    auto* edit=control<QPushButton>(window,"artboard-edit");reveal(window,edit);edit->click();events();same(window.host.session,expected);
    check(control<QLineEdit>(window,"grid-rows")->text()=="2","Fresh editor reads replacement Session count, preserving stable IDs");
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);
    try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";
        if(argc==4&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));
        else {QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GRID_ROWS_ROOT");const auto root=requested.isEmpty()?temporary.path():requested;
            flow(root,0);flow(root,1);driven(root,false);driven(root,true);columns_smoke(root);replacement_session(root);}
        std::cout<<"PASS native Grid Rows relative: "<<checks<<" checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL native Grid Rows relative: "<<e.what()<<" after "<<checks<<" checks\n";return 1;}
}
