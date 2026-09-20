#include "host.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLocalSocket>
#include <QSaveFile>
#include <QUuid>
#include <QDateTime>
#include <limits>
#include <memory>
#include <algorithm>

namespace nect::desktop {
Id new_id() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
namespace {
void write_atomic(const QString& path,const QByteArray& bytes) {
    QSaveFile out(path);
    out.setDirectWriteFallback(false);
    if(!out.open(QIODevice::WriteOnly) || out.write(bytes)!=bytes.size() || !out.commit())
        throw Error("IO_ERROR",out.errorString().toStdString());
}
Document read_native(const QString& path) {
    QFile input(path);
    if(!input.open(QIODevice::ReadOnly)) throw Error("IO_ERROR",input.errorString().toStdString());
    if(input.size()>8*1024*1024) throw Error("INPUT_LIMIT","Native file exceeds 8 MiB");
    const auto data=input.readAll();
    if(input.error()!=QFileDevice::NoError) throw Error("IO_ERROR",input.errorString().toStdString());
    return decode(std::string_view(data.constData(),static_cast<std::size_t>(data.size())));
}
QString string(const QJsonObject& o,const char* key) {
    if(!o.value(key).isString()) throw Error("INVALID_REQUEST",std::string("String required: ")+key);
    return o.value(key).toString();
}
}

Host::Host(QString recovery_directory,QObject* parent)
    :QObject(parent),session(empty_document(new_id(),new_id(),new_id())),
     session_id(QString::fromStdString(new_id())),recovery_directory_(std::move(recovery_directory)) {
    recovery_timer_.setSingleShot(true);
    recovery_timer_.setInterval(1000);
    connect(&recovery_timer_,&QTimer::timeout,this,[this] {
        try { protect(); } catch(const std::exception& e) { save_status="Recovery failed: "+QString::fromUtf8(e.what()); }
        if(status_changed) status_changed();
    });
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server_,&QLocalServer::newConnection,this,[this] {
        while(auto* socket=server_.nextPendingConnection()) {
            auto buffer=std::make_shared<QByteArray>();
            connect(socket,&QLocalSocket::readyRead,this,[this,socket,buffer] {
                buffer->append(socket->readAll());
                if(buffer->size()>8*1024*1024) { socket->abort(); return; }
                for(;;) {
                    const auto newline=buffer->indexOf('\n');
                    if(newline<0) break;
                    const auto line=buffer->left(newline);
                    buffer->remove(0,newline+1);
                    socket->write(dispatch(line)+'\n');
                }
            });
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        }
    });
}
void Host::listen(const QString& endpoint) {
    if(!server_.listen(endpoint)) throw Error("IPC_ERROR",server_.errorString().toStdString());
}
void Host::edited() {
    save_status="Unsaved · recovery pending";
    // Periodic protection under continuous editing: don't postpone indefinitely.
    if(!recovery_timer_.isActive()) recovery_timer_.start();
    if(changed) changed();
}
void Host::protect() {
    if(protected_session_==session_id && protected_revision_==session.revision()) return;
    if(!QDir().mkpath(recovery_directory_)) throw Error("IO_ERROR","Cannot create recovery directory");
    auto bytes=QByteArray::fromStdString(encode(session.document()));
    const auto base=QDir(recovery_directory_).filePath(session_id);
    write_atomic(base+".nect",bytes);
    protected_session_=session_id; protected_revision_=session.revision();
    save_status=dirty()?(file_path.isEmpty()?"Recovery protected · not saved to a file":"Changes protected in recovery"):
        "Saved · revision "+QString::number(saved_revision_);
}
void Host::reset(Document document,const QString& path) {
    if(session.gesture_active()) throw Error("GESTURE_ACTIVE","Finish the current gesture first");
    if(std::none_of(document.compositions.begin(),document.compositions.end(),[](const auto& c){return !c.artboards.empty();}))
        throw Error("DESKTOP_DOCUMENT_REQUIREMENT","The desktop needs a Composition with an Artboard; source file is unchanged");
    // Protect the outgoing document before replacing the live Session.
    protect();
    session=Session(std::move(document));
    session_id=QString::fromStdString(new_id());
    file_path=path; saved_revision_=0;
    save_status=path.isEmpty()?"New document":"Opened · saved";
    recovery_timer_.stop();
    if(changed) changed();
}
void Host::create_document() { reset(empty_document(new_id(),new_id(),new_id()),{}); }
void Host::open(const QString& path) { reset(read_native(path),QFileInfo(path).absoluteFilePath()); }
void Host::save(const QString& path) {
    if(session.gesture_active()) throw Error("GESTURE_ACTIVE","Finish the gesture before saving");
    if(path.isEmpty()) throw Error("IO_ERROR","Choose a native file path");
    const auto absolute=QFileInfo(path).absoluteFilePath();
    const auto bytes=QByteArray::fromStdString(encode(session.document()));
    // Retain the previous file before replacement; failure leaves it untouched.
    if(QFile::exists(absolute)) {
        const auto backups=absolute+".backups";
        if(!QDir().mkpath(backups)) throw Error("IO_ERROR","Cannot create backup directory");
        const auto name=QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmsszzz")+"-"+
            QString::fromStdString(new_id())+".nect";
        if(!QFile::copy(absolute,QDir(backups).filePath(name))) throw Error("IO_ERROR","Cannot retain previous save");
        // No pruning before the replacement has succeeded.
    }
    write_atomic(absolute,bytes);
    file_path=absolute; saved_revision_=session.revision();
    save_status="Saved · revision "+QString::number(saved_revision_);
    const QDir backups(absolute+".backups");
    const auto files=backups.entryList({"*.nect"},QDir::Files,QDir::Name);
    for(qsizetype i=0;i<files.size()-10;++i) QFile::remove(backups.filePath(files[i]));
    if(changed) changed();
}
void Host::recover() { protect(); }

QByteArray Host::dispatch(const QByteArray& input) {
    QJsonObject response;
    try {
        validate_json(std::string_view(input.constData(),static_cast<std::size_t>(input.size())));
        auto outer=QJsonDocument::fromJson(input).object();
        const auto operation=string(outer,"op");
        const QStringList allowed=operation=="hello"?QStringList{"op"}:
            (operation=="core"?QStringList{"op","session_id","document_id","request"}:
                QStringList{"op","session_id","document_id","expected_revision","path"});
        for(auto it=outer.begin();it!=outer.end();++it)
            if(!allowed.contains(it.key()))throw Error("UNKNOWN_FIELD",it.key().toStdString());
        if(string(outer,"op")=="hello") {
            response={{"ok",true},{"session_id",session_id},{"document_id",QString::fromStdString(session.document().id)},
                {"revision",static_cast<qint64>(session.revision())},{"file",file_path},{"save_status",save_status},
                {"transport","nect-local-session"},{"protocol",1}};
        } else {
            if(string(outer,"session_id")!=session_id || string(outer,"document_id").toStdString()!=session.document().id)
                throw Error("SESSION_CONFLICT","Refresh the live document/session identity");
            const auto op=string(outer,"op");
            const auto before=session.revision();
            if(op=="core") {
                if(!outer.value("request").isObject()) throw Error("INVALID_REQUEST","Request object required");
                const auto bytes=QJsonDocument(outer.value("request").toObject()).toJson(QJsonDocument::Compact);
                response=QJsonDocument::fromJson(QByteArray::fromStdString(request(session,
                    std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size()))))).object();
                if(session.revision()!=before) edited();
            } else {
                const auto expected=outer.value("expected_revision");
                if(!expected.isDouble() || expected.toDouble()!=static_cast<double>(session.revision()))
                    throw Error("REVISION_CONFLICT","Refresh revision before file/session operations");
                if(op=="save") save(string(outer,"path"));
                else if(op=="open") open(string(outer,"path"));
                else if(op=="new") create_document();
                else if(op=="recover") recover();
                else throw Error("UNSUPPORTED_OPERATION",op.toStdString());
                response={{"ok",true}};
            }
            response["session_id"]=session_id;
            response["document_id"]=QString::fromStdString(session.document().id);
            response["revision"]=static_cast<qint64>(session.revision());
        }
    } catch(const Error& e) {
        response={{"ok",false},{"error",QJsonObject{{"code",QString::fromStdString(e.code)},
            {"message",QString::fromUtf8(e.what())}}},{"revision",static_cast<qint64>(session.revision())}};
    } catch(const std::exception& e) {
        response={{"ok",false},{"error",QJsonObject{{"code","INVALID_REQUEST"},{"message",QString::fromUtf8(e.what())}}}};
    }
    return QJsonDocument(response).toJson(QJsonDocument::Compact);
}
}
