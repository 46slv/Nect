#include "host.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
QByteArray bytes(const QString& path) {QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Read failed");return f.readAll();}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        QTemporaryDir temp;check(temp.isValid(),"Temporary directory");
        Host host(temp.path()+"/recovery");
        const auto& comp=host.session.document().compositions.front();
        Point p;p.id="p";p.x.literal=12;p.y.literal=24;
        host.session.apply({CreatePath{comp.id,"","path","Path",{{"contour",false,{p}}}}},0);
        host.edited();
        const auto path=temp.path()+"/drawing.nect";
        host.save(path);const auto original=bytes(path);
        host.session.apply({Set{{"path","p","x"},88}},1);host.edited();
        host.save(path);const auto changed=bytes(path);
        check(original!=changed,"Saved edit differs");
        const QDir backups(path+".backups");const auto files=backups.entryList({"*.nect"},QDir::Files);
        check(files.size()==1&&bytes(backups.filePath(files.front()))==original,"Backup retains exact previous file");
        const auto session=host.session_id;
        host.recover();
        check(bytes(temp.path()+"/recovery/"+session+".nect")==changed,"Recovery is committed native state");
        host.session.begin_gesture(2);host.session.update_gesture({Set{{"path","p","x"},123}});host.recover();
        check(bytes(temp.path()+"/recovery/"+session+".nect")==changed,"Draft gesture never becomes recovery authority");
        host.session.cancel_gesture();
        host.session.apply({Set{{"path","p","x"},99}},2);host.edited();
        try {host.save(temp.path()+"/missing/drawing.nect");throw std::runtime_error("Expected write failure");}
        catch(const Error& e) {check(e.code=="IO_ERROR","Write failure is explicit");}
        check(host.dirty()&&bytes(path)==changed,"Failed save preserves source and dirty status");
        const auto old_doc=host.session.document().id;
        host.open(backups.filePath(files.front()));
        check(evaluate(host.session.document()).at({"path","p","x"})==12,"Backup restores actual earlier state");
        check(host.session_id!=session,"Open rotates Session identity");
        QJsonObject stale{{"op","core"},{"session_id",session},{"document_id",QString::fromStdString(old_doc)},
            {"request",QJsonObject{{"op","undo"},{"expected_revision",0}}}};
        auto response=QJsonDocument::fromJson(host.dispatch(QJsonDocument(stale).toJson())).object();
        check(response["error"].toObject()["code"]=="SESSION_CONFLICT","Old session cannot target new document");
        auto duplicate=QJsonDocument::fromJson(host.dispatch("{\"op\":\"hello\",\"op\":\"new\"}")).object();
        check(duplicate["error"].toObject()["code"]=="DUPLICATE_KEY","IPC rejects duplicate keys");
        QFile invalid(temp.path()+"/bad.nect");check(invalid.open(QIODevice::WriteOnly),"Create bad fixture");invalid.write("{\"format\":\"future\"}");invalid.close();
        const auto before=encode(host.session.document());
        try {host.open(invalid.fileName());throw std::runtime_error("Expected open failure");} catch(const Error&) {}
        check(encode(host.session.document())==before,"Failed open preserves live document");
        std::cout<<"PASS desktop persistence, backup restore, preview isolation and session identity\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
