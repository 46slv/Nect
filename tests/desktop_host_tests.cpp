#include "host.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
QByteArray bytes(const QString& path) {QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Read failed");return f.readAll();}
void exact(double actual,double expected,const char* message) {
    if(std::bit_cast<std::uint64_t>(actual)==std::bit_cast<std::uint64_t>(expected))return;
    std::ostringstream reason;reason<<std::setprecision(17)<<message<<": expected "<<expected<<", got "<<actual;
    throw std::runtime_error(reason.str());
}
QJsonObject core_call(Host& host,const QJsonObject& request) {
    const QJsonObject envelope{{"op","core"},{"session_id",host.session_id},
        {"document_id",QString::fromStdString(host.session.document().id)},{"request",request}};
    const auto response=QJsonDocument::fromJson(host.dispatch(QJsonDocument(envelope).toJson(QJsonDocument::Compact))).object();
    check(response.value("ok").toBool(),"Precision fixture core request succeeded");return response;
}
double inspected_x(const QJsonObject& response) {
    const auto objects=response.value("result").toObject().value("objects").toArray();
    for(const auto object:objects)if(object.toObject().value("id")=="precision-path")
        return object.toObject().value("contours").toArray().first().toObject().value("points").toArray().first().toObject()
            .value("x").toObject().value("literal").toDouble();
    throw std::runtime_error("Precision fixture path missing from inspect");
}
void numeric_transport(const QString& directory) {
    Host host(directory+"/precision-recovery");
    const auto composition=host.session.document().compositions.front().id;
    Point point;point.id="precision-point";
    host.session.apply({CreatePath{composition,"","precision-path","Precision",{{"precision-contour",false,{point}}}}},0);
    const Ref x{"precision-path","precision-point","x"},y{"precision-path","precision-point","y"};
    const std::array values{0.9781476007338057,std::nextafter(0.9781476007338057,0.0),-0.20791169081775931,
        std::nextafter(1.0,2.0),std::nextafter(1.0,0.0),0.10000000000000002,
        std::nextafter(1e9,0.0),-std::nextafter(1e9,0.0),1.2345678901234567e-120,
        std::numeric_limits<double>::min(),std::nextafter(std::numeric_limits<double>::min(),0.0),
        std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min(),0.0};
    for(const auto value:values) {
        // Seed directly from the C++ double, independently of either JSON parser.
        host.session.apply({Set{x,value}},host.session.revision());
        const auto native=encode(host.session.document());const auto reopened=decode(native);
        exact(property(reopened,x).literal,value,"Native decode preserves the authored double bit-for-bit");
        check(encode(reopened)==native,"Native decode/re-encode preserves exact serialized state");

        const auto direct=QJsonDocument::fromJson(QByteArray::fromStdString(request(host.session,R"({"op":"inspect"})"))).object();
        check(direct.value("ok").toBool(),"Direct inspect succeeded");
        exact(inspected_x(direct),value,"Core inspect's encoded-document parse preserves the double");
        exact(inspected_x(core_call(host,{{"op","inspect"}})),value,"Desktop local API inspect envelope preserves the double");
        const QJsonObject ref{{"object","precision-path"},{"point","precision-point"},{"field","x"}};
        const auto got=core_call(host,{{"op","get"},{"ref",ref}}).value("result").toObject();
        exact(got.value("authored").toObject().value("literal").toDouble(),value,"Desktop get preserves authored precision");
        exact(got.value("evaluated").toDouble(),value,"Desktop get preserves evaluated precision");

        // Exercise the opposite direction through Qt's local request envelope
        // and the normal Boost command parser, not only response formatting.
        const QJsonObject target{{"object","precision-path"},{"point","precision-point"},{"field","y"}};
        const auto changed=core_call(host,{{"op","apply"},{"expected_revision",static_cast<qint64>(host.session.revision())},
            {"commands",QJsonArray{QJsonObject{{"type","set"},{"ref",target},{"value",value}}}}});
        exact(property(host.session.document(),y).literal,value,"Desktop apply envelope preserves incoming double");
        check(changed.value("result").toObject().value("changed_ids").toArray().contains("precision-path"),"Exact numeric change is reported in changed_ids");
        exact(property(decode(encode(host.session.document())),y).literal,value,"API-authored value reopens exactly");
    }
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        QTemporaryDir temp;check(temp.isValid(),"Temporary directory");
        numeric_transport(temp.path());
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
        auto multiple=empty_document("multiple","empty-plane","temporary-frame");
        multiple.compositions.front().artboards.clear();
        multiple.compositions.push_back({"visible-plane","Visible",{},{{"visible-frame","Page",100,200,640,480}}});
        QFile planes(temp.path()+"/planes.nect");check(planes.open(QIODevice::WriteOnly),"Create multiple-plane fixture");
        planes.write(QByteArray::fromStdString(encode(multiple)));planes.close();host.open(planes.fileName());
        check(host.session.document().id=="multiple","Desktop accepts a visible frame after an empty Composition");
        std::cout<<"PASS desktop persistence, backup restore, preview isolation and session identity\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
