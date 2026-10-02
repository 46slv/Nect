#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QAbstractButton>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <exception>
#include <iostream>
#include <optional>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
    ++checks;
}
QByteArray bytes(const QString& path) {
    QFile file(path);check(file.open(QIODevice::ReadOnly),"Read owned native fixture");return file.readAll();
}
QByteArray encoded(const Document& document) {return QByteArray::fromStdString(encode(document));}
QString hash(const QByteArray& value) {
    return QString::fromLatin1(QCryptographicHash::hash(value,QCryptographicHash::Sha256).toHex());
}
Document fixture(const char* identity,double value) {
    Session session(empty_document(identity,"composition","artboard"));
    Point point;point.id="point";point.x.literal=value;point.y.literal=20;
    session.apply({CreatePath{"composition","","path","Owned fixture",{{"contour",false,{point}}}}},0);
    return session.document();
}
QJsonObject dispatch(Host& host,QJsonObject request) {
    request["session_id"]=host.session_id;
    request["document_id"]=QString::fromStdString(host.session.document().id);
    request["expected_revision"]=qint64(host.session.revision());
    const auto result=QJsonDocument::fromJson(host.dispatch(QJsonDocument(request).toJson(QJsonDocument::Compact))).object();
    check(result["ok"].toBool(),"Replacement uses the real live Host API");return result;
}
void edit(Host& host,double value) {
    if(host.session.document().objects.empty()) {
        Point point;point.id="point";point.x.literal=value;point.y.literal=20;
        const auto composition=host.session.document().compositions.front().id;
        host.session.apply({CreatePath{composition,"","path","Replacement",{{"contour",false,{point}}}}},host.session.revision());
    } else host.session.apply({Set{{"path","point","x"},value}},host.session.revision());
    host.edited();
}
struct State {
    QString identity,path;
    Document document;
    std::uint64_t revision;
    HistoryInfo history;
    QJsonValue saved_revision;
    bool dirty;
};
State state(const Host& host) {
    return {host.session_id,host.file_path,host.session.document(),host.session.revision(),
        host.session.history(),host.persistence()["saved_revision"],host.dirty()};
}
void unchanged(const Host& host,const State& before) {
    check(host.session_id==before.identity&&host.session.document()==before.document,
        "Completion preserves replacement Session identity and authored state");
    check(host.session.revision()==before.revision&&host.session.history()==before.history,
        "Completion preserves revision and exact retained Undo/Redo history");
    check(host.file_path==before.path&&host.persistence()["saved_revision"]==before.saved_revision&&host.dirty()==before.dirty,
        "Completion preserves native association, saved revision and dirty state");
}
QAction* save_action(Window& window,bool choose) {
    // Standard shortcuts can be empty under an offscreen platform theme.
    for(auto* action:window.findChildren<QAction*>()) {
        const auto label=QString(action->text()).remove('&');
        if(choose?label.startsWith("Save As"):label=="Save")return action;
    }
    throw std::runtime_error("Production Save action missing");
}

enum class Change {none,edit,new_document,open,reopen};
struct Case {
    const char* name;
    Change change=Change::none;
    bool cancel=false,unnamed=false,choose=true,existing=true,confirmation=false;
};
const Case cases[]={
    {"normal-as"},
    {"normal-unnamed-save",Change::none,false,true,false,false},
    {"normal-save",Change::none,false,false,false},
    {"same-session-edit",Change::edit,false,true},
    {"cancel",Change::none,true},
    {"cancel-after-new",Change::new_document,true},
    {"cancel-after-open",Change::open,true},
    {"stale-new-existing",Change::new_document},
    {"stale-new-missing",Change::new_document,false,false,true,false},
    {"stale-open-existing",Change::open},
    {"stale-reopen",Change::reopen},
    {"stale-confirmation",Change::new_document,false,false,true,true,true}
};
void run(const Case& scenario) {
    QTemporaryDir scratch;check(scratch.isValid(),"Own isolated temporary directory");
    QSettings settings(scratch.path()+"/library.ini",QSettings::IniFormat);
    Window window(scratch.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
    const auto source=scratch.path()+"/source.nect",replacement=scratch.path()+"/replacement.nect";
    // In stale-new-existing, the target is the initiating document's own file.
    const auto target=QString::fromLatin1(scenario.name)=="stale-new-existing"||!scenario.choose&&!scenario.unnamed?
        source:scratch.path()+"/destination.nect";
    store_native(source,encoded(fixture("source-document",11)),FileStamp{},true);
    store_native(replacement,encoded(fixture("replacement-document",29)),FileStamp{},true);
    if(target!=source&&scenario.existing)store_native(target,encoded(fixture("target-document",43)),FileStamp{},true);
    if(scenario.unnamed)edit(window.host,11);else window.host.open(source);
    window.host.recover();
    const auto source_before=bytes(source),replacement_before=bytes(replacement);
    const auto existed=QFileInfo::exists(target);
    const auto target_before=existed?bytes(target):QByteArray{};
    const auto initiating=state(window.host);
    window.show();QApplication::processEvents();
    window.statusBar()->clearMessage();
    bool dialog_seen=false,confirmation_seen=false,change_done=false;
    std::optional<State> expected;
    std::exception_ptr harness_error;
    const auto change=[&] {
        check(!change_done,"Inject replacement exactly once");change_done=true;
        switch(scenario.change) {
        case Change::none:break;
        case Change::edit:edit(window.host,53);break;
        case Change::new_document:dispatch(window.host,{{"op","new"}});edit(window.host,53);break;
        case Change::open:dispatch(window.host,{{"op","open"},{"path",replacement}});break;
        case Change::reopen:dispatch(window.host,{{"op","open"},{"path",source}});break;
        }
        window.host.recover();expected=state(window.host);
        if(scenario.change==Change::new_document||scenario.change==Change::open||scenario.change==Change::reopen)
            check(expected->identity!=initiating.identity,"Actual Host reset replaces Session identity");
        if(scenario.change==Change::reopen)
            check(expected->document.id==initiating.document.id,"Reopening the same Document still replaces the Session");
    };
    QElapsedTimer elapsed;elapsed.start();
    QTimer drive;drive.setInterval(5);
    QObject::connect(&drive,&QTimer::timeout,&window,[&] {
        try {
            check(elapsed.elapsed()<10000,"File chooser automation finishes within bounded time");
            if(!dialog_seen) {
                auto* dialog=window.findChild<QFileDialog*>();
                if(!dialog||!dialog->isVisible())return;
                dialog_seen=true;
                if(!scenario.confirmation)change();
                // Keep one real overwrite-confirmation case; other cases automate
                // the chooser directly, as existing desktop dialog tests do.
                dialog->setOption(QFileDialog::DontConfirmOverwrite,!scenario.confirmation);
                if(scenario.cancel)dialog->reject();
                else {dialog->selectFile(target);QMetaObject::invokeMethod(dialog,"accept",Qt::QueuedConnection);}
            } else if(scenario.confirmation&&!confirmation_seen) {
                auto* confirmation=window.findChild<QMessageBox*>();
                if(!confirmation||!confirmation->isVisible())return;
                confirmation_seen=true;change();
                auto* yes=confirmation->button(QMessageBox::Yes);check(yes,"Real overwrite confirmation has Yes");yes->click();
            }
        } catch(...) {
            harness_error=std::current_exception();drive.stop();
            for(auto* dialog:window.findChildren<QDialog*>())dialog->reject();
        }
    });
    if(scenario.choose||scenario.unnamed)drive.start();else {change();}
    save_action(window,scenario.choose)->trigger();drive.stop();
    if(harness_error)std::rethrow_exception(harness_error);
    check(dialog_seen==(scenario.choose||scenario.unnamed),"Expected production file chooser path ran");
    check(confirmation_seen==scenario.confirmation,"Expected overwrite-confirmation boundary ran");
    check(expected.has_value(),"Capture the state immediately before chooser completion");
    const auto exists_after=QFileInfo::exists(target);
    const auto target_after=exists_after?bytes(target):QByteArray{};
    const auto conflict=window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT:");
    const auto stale=expected->identity!=initiating.identity;
    const QJsonObject receipt{{"case",scenario.name},{"dialog_seen",dialog_seen},{"confirmation_seen",confirmation_seen},
        {"initiating_session",initiating.identity},{"completion_session",window.host.session_id},
        {"session_replaced",stale},{"session_conflict",conflict},{"cancelled",scenario.cancel},
        {"target_existed_before",existed},{"target_exists_after",exists_after},
        {"target_changed",existed!=exists_after||target_before!=target_after},
        {"target_contains_current_document",exists_after&&target_after==encoded(window.host.session.document())},
        {"association_changed",window.host.file_path!=expected->path},
        {"target_before_sha256",existed?hash(target_before):QString{}},{"target_after_sha256",exists_after?hash(target_after):QString{}}};
    std::cout<<QJsonDocument(receipt).toJson(QJsonDocument::Compact).constData()<<std::endl;
    if(scenario.cancel||stale) {
        check(conflict==(stale&&!scenario.cancel),"Accepted stale chooser reports SESSION_CONFLICT; cancel stays silent");
        check(existed==exists_after&&target_before==target_after,"Stale or cancelled Save As performs no target write");
        unchanged(window.host,*expected);
    } else {
        check(!conflict,"Current Session Save As remains accepted after ordinary same-Session edits");
        check(exists_after&&load_native(target).document==expected->document,"Actual native output is the current authored document");
        check(window.host.session_id==initiating.identity&&window.host.session.document()==expected->document&&
            window.host.session.revision()==expected->revision&&window.host.session.history()==expected->history,
            "Ordinary Save preserves document/Session/source identities and exact history");
        check(same_native_path(window.host.file_path,target)&&!window.host.dirty()&&
            window.host.persistence()["saved_revision"].toInteger(-1)==qint64(expected->revision),
            "Ordinary Save binds and acknowledges exactly the saved revision");
    }
    check(bytes(source)==source_before,"Initiating source file remains byte-identical");
    check(bytes(replacement)==replacement_before,"Replacement source file remains byte-identical");
    // Verify real Undo/Redo remains usable, including the unnamed replacement
    // Session's retained edit after stale completion or cancellation.
    if((scenario.cancel||stale)&&window.host.session.can_undo()) {
        window.host.session.undo(window.host.session.revision());window.host.session.redo(window.host.session.revision());
        check(window.host.session.document()==expected->document,"Replacement history still round-trips exact authored state");
    }
    window.close();
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QTemporaryDir app_settings;
    if(!app_settings.isValid())return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,app_settings.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,app_settings.path());
    QApplication app(argc,argv);
    try {
        const auto selected=argc==3&&QString::fromLocal8Bit(argv[1])=="--case"?QString::fromLocal8Bit(argv[2]):QString{};
        bool ran=false;
        for(const auto& scenario:cases)if(selected.isEmpty()||selected==QString::fromLatin1(scenario.name)) {run(scenario);ran=true;}
        check(ran,"Requested regression case exists");
        std::cout<<"PASS "<<checks<<" Save As Session identity checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<std::endl;return 1;}
}
