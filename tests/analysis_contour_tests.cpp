#include "window.hpp"
#include "analysis_contour_control.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& s):document(s.document()),revision(s.revision()),history(s.history()),native(encode(s.document())){}
    bool unchanged(const Session& s)const{return document==s.document()&&revision==s.revision()&&history==s.history()&&native==encode(s.document());}
};
Document fixture(){
    auto d=empty_document("analysis-doc","comp","board");
    auto& board=d.compositions.front().artboards.front();board.x=-10;board.y=20;board.width=12;board.height=8;
    ColorValue white;white.rgba={1,1,1,1};board.background=white;
    Object source;source.id="source";source.name="Original artwork";
    Contour c;c.id="source-contour";c.closed=true;
    for(auto xy:std::vector<Vec2>{{-9,21},{-5,21},{-5,24},{-9,24}}){Point p;p.id="source-p"+std::to_string(c.points.size());p.x.literal=xy.x;p.y.literal=xy.y;c.points.push_back(p);}
    source.contours={c};source.stack.push_back(default_operation("source-fill","nect.paint.fill"));
    d.objects.emplace(source.id,source);d.compositions.front().roots={source.id};return d;
}
QJsonObject api(Host& host,QJsonObject request){
    if(!request.contains("session_id"))request.insert("session_id",host.session_id);
    request.insert("document_id",QString::fromStdString(host.session.document().id));
    return QJsonDocument::fromJson(host.dispatch(QJsonDocument(request).toJson(QJsonDocument::Compact))).object();
}
QJsonObject candidate(Host& host){
    const auto analysis=api(host,{{"op","analyze_regions"},{"composition","comp"},{"artboard","board"},
        {"scale",2},{"threshold",128},{"expected_revision",static_cast<qint64>(host.session.revision())}});
    check(analysis.value("ok").toBool(),"Actual analysis API succeeds");
    const auto result=analysis.value("result").toObject();
    const auto contours=result.value("outer_contours").toArray();
    check(contours.size()==1,"Actual analysis excludes the opaque Artboard background");
    return {{"op","adopt_analysis_contour"},{"composition","comp"},{"artboard","board"},
        {"scale",2},{"threshold",128},{"expected_revision",static_cast<qint64>(host.session.revision())},
        {"analysis_id",result.value("analysis_id")},{"contour_id",contours.first().toObject().value("id")},{"name","Adopted outline"}};
}
void refusal(Host& host,QJsonObject request,const char* code){const Snapshot before(host.session);const auto result=api(host,request);
    check(!result.value("ok").toBool()&&result.value("error").toObject().value("code").toString()==code,"Expected adoption refusal");
    check(before.unchanged(host.session),"Refusal preserves exact native state, revision and history");}
void primary(Host& host,const QString& executable,const QString& directory){
    host.session=Session(fixture());const Snapshot before(host.session);const auto request=candidate(host);
    check(before.unchanged(host.session),"Analysis stays read-only");
    const auto response=api(host,request);check(response.value("ok").toBool(),"Adoption API succeeds");
    const auto result=response.value("result").toObject();const auto id=result.value("object_id").toString().toStdString();
    const auto adopted=host.session.document();const auto& object=adopted.objects.at(id);const auto& contour=object.contours.front();
    check(object.kind==Kind::path&&!object.source&&contour.closed&&contour.points.size()==4,"Normal editable closed Path with four retained corners");
    check(host.session.revision()==before.revision+1&&host.session.history().states.size()==before.history.states.size()+1,"Exactly one canonical Undo entry");
    check(adopted.objects.at("source")==before.document.objects.at("source"),"Source artwork exact preservation");
    check(adopted.compositions.front().roots==std::vector<Id>{"source",id},"Separate root Path above retained source");
    const std::vector<Vec2> expected{{-9,21},{-5,21},{-5,24},{-9,24}};
    for(std::size_t i=0;i<4;++i){const auto& p=contour.points[i];
        check(p.x.literal==expected[i].x&&p.y.literal==expected[i].y&&p.in_length.literal==0&&p.out_length.literal==0,"Artboard origin and scale map to exact straight anchors");
        check(p.id==result.value("point_ids").toArray().at(static_cast<int>(i)).toString().toStdString(),"Receipt gives stable authored point identity");}
    const auto point=contour.points.front().id;
    host.session.apply({Set{{id,point,"x"},-8.5}},host.session.revision());host.edited();
    check(host.session.document().objects.at(id).contours.front().points.front().x.literal==-8.5,"Adopted point editable with canonical Set");
    host.session.undo(host.session.revision());check(host.session.document()==adopted,"Undo point edit restores adopted Path");
    host.session.undo(host.session.revision());check(host.session.document()==before.document,"One adoption Undo restores exact original document");
    host.session.redo(host.session.revision());check(host.session.document()==adopted,"Redo preserves authored IDs and geometry");
    const auto path=directory+"/adopted.nect";host.save(path);host.flush();
    QProcess cold;cold.start(executable,{"--cold",path,QString::fromStdString(id),QString::fromStdString(point)});
    check(cold.waitForFinished(30000)&&cold.exitCode()==0,"Fresh process native reopen and point edit succeed");
    check(encode(host.session.document())==encode(adopted),"Save/reopen probe leaves live source unchanged");
}
void risks(Host& host){
    host.session=Session(fixture());auto request=candidate(host);
    auto bad=request;bad.insert("analysis_id","analysis-stale");refusal(host,bad,"ANALYSIS_CONFLICT");
    bad=request;bad.insert("contour_id","contour-foreign");refusal(host,bad,"INVALID_REQUEST");
    bad=request;bad.insert("session_id","old-session");refusal(host,bad,"SESSION_CONFLICT");
    bad=request;bad.insert("vertices",QJsonArray{});refusal(host,bad,"UNKNOWN_FIELD");
    bad=request;bad.insert("threshold",128.5);refusal(host,bad,"INVALID_THRESHOLD");
    bad=request;bad.insert("scale",1);refusal(host,bad,"ANALYSIS_CONFLICT");
    host.session.begin_gesture(host.session.revision());
    host.session.update_gesture({Set{{"source","source-p0","x"},-8.5}});
    refusal(host,request,"GESTURE_ACTIVE");check(host.session.gesture_active(),"Refusal does not cancel another owner's gesture");host.session.cancel_gesture();
    host.session.apply({Rename{"source","Changed"}},host.session.revision());refusal(host,request,"REVISION_CONFLICT");
    bad=request;bad.insert("expected_revision",static_cast<qint64>(host.session.revision()));refusal(host,bad,"ANALYSIS_CONFLICT");
    auto ring=fixture();auto hole=ring.objects.at("source").contours.front();hole.id="hole";
    const std::vector<Vec2> inner{{-8,22},{-8,23},{-6,23},{-6,22}};
    for(std::size_t i=0;i<inner.size();++i){hole.points[i].id="hole-p"+std::to_string(i);hole.points[i].x.literal=inner[i].x;hole.points[i].y.literal=inner[i].y;}
    ring.objects.at("source").contours.push_back(hole);host.session=Session(ring);
    request=candidate(host);const auto adopted=api(host,request);
    check(adopted.value("ok").toBool()&&!adopted.value("result").toObject().value("holes_preserved").toBool(),"Ring adoption explicitly omits holes");
    const auto id=adopted.value("result").toObject().value("object_id").toString().toStdString();
    check(host.session.document().objects.at(id).contours.size()==1&&host.session.document().objects.at("source")==ring.objects.at("source"),"Only outer cycle adopted; source hole remains authored");
}
QDialog* open(Window& w){w.findChild<QAction*>("analysis-contour-to-path")->trigger();events();auto* d=w.findChild<QDialog*>("analysis-contour-dialog");check(d&&d->isVisible(),"Window menu opens contour dialog");return d;}
void analyze(QDialog* d){d->findChild<QPushButton*>("analysis-contour-analyze")->click();events();}
void ui(Window& w){
    w.host.session=Session(fixture());w.host.session_id+="-ui";w.canvas->set_active_artboard("comp","board",false);w.host.edited();events();
    const Snapshot before(w.host.session);auto* d=open(w);analyze(d);
    auto* apply=d->findChild<QPushButton*>("analysis-contour-adopt");check(apply->isEnabled(),"Analysis enables explicit adoption");
    d->findChild<QDoubleSpinBox*>("analysis-contour-scale")->setValue(2);check(!apply->isEnabled(),"Parameter change invalidates analysis selection");
    analyze(d);d->reject();events();check(before.unchanged(w.host.session),"Cancel after analysis leaves exact source/history unchanged");
    d=open(w);analyze(d);d->findChild<QPushButton*>("analysis-contour-adopt")->click();events();
    check(w.host.session.document().objects.size()==2&&w.canvas->selections().size()==1,"UI creates and selects adopted Path");
    const auto selected=w.canvas->selections().front().object;check(selected!="source","UI selects new Path for editing");
    w.host.session.undo(w.host.session.revision());w.host.edited();events();check(w.host.session.document()==before.document,"UI adoption has one Undo");
    d=open(w);analyze(d);w.host.session.apply({Rename{"source","External edit"}},w.host.session.revision());
    const Snapshot concurrent(w.host.session);d->findChild<QPushButton*>("analysis-contour-adopt")->click();events();
    check(concurrent.unchanged(w.host.session)&&d->isVisible(),"Stale UI revision refuses adoption");d->reject();events();
    d=open(w);analyze(d);w.host.create_document();const Snapshot replacement(w.host.session);
    d->findChild<QPushButton*>("analysis-contour-adopt")->click();events();
    check(replacement.unchanged(w.host.session)&&d->isVisible(),"Stale UI Session refuses adoption");d->reject();events();
    auto* macro=w.findChild<QPushButton*>("effects-create-macro");check(macro&&macro->isEnabled(),"Macro creation entry works with no selected object");
    macro->click();events();auto* macro_dialog=w.findChild<QDialog*>("macro-authoring-dialog");
    check(macro_dialog&&macro_dialog->isVisible(),"Effects entry opens standalone Macro component");macro_dialog->reject();events();
    check(replacement.unchanged(w.host.session),"Macro entry cancel leaves empty document unchanged");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {QTemporaryDir temporary;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path()+"/settings");
        if(argc==5&&QString(argv[1])=="--cold") {Host reader(temporary.path()+"/cold");reader.open(QString::fromLocal8Bit(argv[2]));
            QFile file(QString::fromLocal8Bit(argv[2]));check(file.open(QIODevice::ReadOnly),"Read saved native");
            check(reader.session.document()==decode(file.readAll().toStdString()),"Cold native exact document");
            const Id object=argv[3],point=argv[4];check(reader.session.document().objects.contains("source"),"Cold retained artwork");
            reader.session.apply({Set{{object,point,"x"},-8}},reader.session.revision());check(reader.session.can_undo(),"Cold Path remains editable");
        } else {Host host(temporary.path()+"/host");primary(host,app.applicationFilePath(),temporary.path());risks(host);
            Window window(temporary.path()+"/window");window.show();events();ui(window);}
        std::cout<<"PASS "<<checks<<" contour adoption checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
