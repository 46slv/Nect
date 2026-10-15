#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Full independent native oracle valid BEFORE Window");}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation();}
void write(const QString& path,const Document& d){QFile file(path);const auto bytes=encode(d);check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&file.write(bytes.data(),bytes.size())==qint64(bytes.size()),"Immutable whole native evidence");}
QByteArray read(const QString& path){QFile file(path);check(file.open(QIODevice::ReadOnly),"Owned native bytes readable");return file.readAll();}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error("Original visible alignment producer missing");}
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* area=qobject_cast<QScrollArea*>(p))area->ensureWidgetVisible(c);events();check(alive&&alive->isVisible()&&alive->isEnabled(),"Original enabled alignment producer survives reveal");}
void history(Window& w,const QString& name){for(auto* a:w.findChildren<QAction*>())if(a->text()==name){check(a->isEnabled(),"Actual History action enabled");a->trigger();events();return;}throw std::runtime_error("Actual History action missing");}
Object rectangle(const Id& id,double x,double y,double width,double height){
    Object o;o.id=id;o.name=id;Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(auto p:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}){Point point;point.id=id+"-point-"+std::to_string(contour.points.size());point.x.literal=p.x;point.y.literal=p.y;contour.points.push_back(point);}
    o.contours={contour};auto fill=default_operation(id+"-fill","nect.paint.fill");fill.parameters.at("r").literal=id=="a"?0.75:0.125;fill.parameters.at("g").literal=id=="b"?0.75:0.125;fill.parameters.at("b").literal=id=="c"?0.75:0.125;
    auto stroke=default_operation(id+"-stroke","nect.paint.stroke");stroke.parameters.at("width").literal=2.125;o.stack={fill,stroke};o.anchor={Scalar{3.125,{}},Scalar{4.25,{}}};o.compositing.opacity.literal=0.8125;return o;
}
Document fixture(){
    auto d=empty_document("alignment-document-context","comp","art");auto& board=d.compositions.front().artboards.front();board.width=320;board.height=240;
    d.objects.emplace("a",rectangle("a",0.125,0.125,10,10));d.objects.emplace("b",rectangle("b",30.25,25.25,20,10));d.objects.emplace("c",rectangle("c",90.375,60.375,10,10));
    auto other=rectangle("other",210,120,40,30);other.transform[4].literal=0.123456789012345;d.objects.emplace("other",other);d.compositions.front().roots={"a","b","c","other"};return d;
}
void flow(const QString& root,int mode){
    const bool distribute=mode>=4;const int situation=mode%4;const bool success=situation==1||situation==3;
    auto initial=fixture();valid(initial);auto current=initial;current.objects.at("other").name="Retained caller";valid(current);
    Session base(initial);base.apply({Rename{"other","Retained caller"}},0);check(base.document()==current&&base.revision()==1,"Full independent original caller/history BEFORE Window");
    auto foreign_initial=initial;foreign_initial.id="foreign-alignment-document";foreign_initial.objects.at("a").contours.front().points.at(1).x.literal+=2;foreign_initial.objects.at("b").compositing.opacity.literal=0.625;valid(foreign_initial);
    auto foreign_current=foreign_initial;foreign_current.objects.at("other").name="Retained caller";valid(foreign_current);Session foreign(foreign_initial);foreign.apply({Rename{"other","Retained caller"}},0);
    check(foreign.document()==foreign_current&&foreign.revision()==base.revision()&&foreign.gesture_generation()==base.gesture_generation(),"Full independent foreign caller has same stable object IDs/revision/generation BEFORE Window");
    auto caller=situation==0?foreign:base;
    if(situation==3){caller.apply({Rename{"other","Unrelated current edit"}},1);current.objects.at("other").name="Unrelated current edit";valid(current);check(caller.document()==current,"Unrelated same-Document caller independently authored BEFORE Window");}
    auto authored=current;
    if(distribute){authored.objects.at("a").transform[4].literal=12.875;authored.objects.at("c").transform[4].literal=-32.875;}
    else{authored.objects.at("b").transform[4].literal=-30.125;authored.objects.at("c").transform[4].literal=-90.25;}
    valid(authored);auto committed=situation==3?caller:base;
    const Command command=distribute?Command{DistributeObjects{{"a","b","c"},"x","key_object:b",7.25}}:Command{AlignObjects{{"a","b","c"},"x","min",{},"selection"}};
    committed.apply({command},committed.revision());check(committed.document()==authored,"Manual full native coordinate/source oracle matches typed command BEFORE Window");
    auto api=situation==3?caller:base;const auto response=request(api,std::string{"{\"op\":\"apply\",\"expected_revision\":"}+std::to_string(api.revision())+",\"commands\":["+(distribute?"{\"type\":\"distribute_objects\",\"objects\":[\"a\",\"b\",\"c\"],\"axis\":\"x\",\"reference\":\"key_object:b\",\"spacing\":7.25}":"{\"type\":\"align_objects\",\"objects\":[\"a\",\"b\",\"c\"],\"axis\":\"x\",\"alignment\":\"min\",\"reference\":\"selection\"}")+"]}");
    check(response.find("\"ok\":true")!=std::string::npos&&same(api,committed),"Real semantic API parity/full History BEFORE Window");
    auto restored=committed;restored.undo(restored.revision());check(restored.document()==current,"Independent one Undo restores whole native BEFORE Window");restored.redo(restored.revision());check(restored.document()==authored&&restored.history()==committed.history(),"Independent full Redo BEFORE Window");
    const auto expected=success?committed:caller;const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
    QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);
    Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},prefix+"-assets"),&settings);
    w.host.session=base;w.host.edited();w.resize(1400,1150);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selections({{"a",{}},{"b",{}},{"c",{}}});events();
    auto* target=control<QComboBox>(w,"alignment-target");check(target->currentData().toString()=="selection","Original reference starts at Selection");
    if(distribute){const auto index=target->findData("key_object:b");check(index>=0,"Original stable key object offered");target->setCurrentIndex(index);control<QLineEdit>(w,"distribution-spacing")->setText("7.25");events();}
    QPointer<QPushButton> actor=control<QPushButton>(w,distribute?"quick-distribute-x":"quick-align-x-min");reveal(actor);check(same(w.host.session,base),"Original selection/reference/spacing navigation leaves full caller Session untouched");
    const auto original_host=w.host.session_id;const auto selection=w.canvas->selections();const auto zoom=w.canvas->zoom();check(w.grab().save(prefix+"-before.png"),"Original styled alignment producer before caller replacement");
    if(situation==0||situation==3)w.host.session=caller;if(situation==2)w.host.session_id+="-changed";
    actor->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual original caller result before Save");
    QJsonObject evidence{{"mode",mode},{"distribute",distribute},{"status",status},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"host_equal",situation==2?w.host.session_id!=original_host:w.host.session_id==original_host},{"view_equal",w.canvas->selections()==selection&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"&&w.canvas->zoom()==zoom},{"save_intent",true}};
    QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable producer/Session/History/view evidence before Save");metadata.write(QJsonDocument(evidence).toJson());metadata.close();
    check(same(observed,expected),"Whole actual caller/native/preview/History/revision/generation matches independent expected");check(evidence["view_equal"].toBool()&&evidence["host_equal"].toBool(),"Original view/zoom/selection/Host preserved");
    if(!success)check(status.startsWith("SESSION_CONFLICT:"),"Retained foreign Document or Host action explicitly refused");
    if(success){history(w,"Undo");check(w.host.session.document()==current,"Actual Undo restores entire pre-alignment native");history(w,"Redo");check(w.host.session.document()==authored,"Actual Redo restores whole alignment result");w.host.session=observed;}
    w.host.save(prefix+"-saved.nect");w.host.flush();events();check(same(w.host.session,expected),"Native Save leaves caller Session/History untouched");check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Exact full native saved bytes; no normalization");w.close();events();
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);
    try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Quick Align/Distribute; physical/fonts/clipboard/pixels NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_ALIGNMENT_DOCUMENT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;
        for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
        if(failures)throw std::runtime_error("Alignment Document/Host cases failed: "+std::to_string(failures));std::cout<<"PASS Alignment Document/Host "<<checks<<" checks\n";QTest::qWait(300);return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL Alignment Document/Host "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}
}
