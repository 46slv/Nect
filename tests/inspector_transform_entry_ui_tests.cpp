#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QTimer>
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
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error("Original visible Inspector transform opener missing");}
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* area=qobject_cast<QScrollArea*>(p))area->ensureWidgetVisible(c);events();check(alive&&alive->isVisible()&&alive->isEnabled(),"Original enabled Inspector transform opener survives reveal");}
void history(Window& w,const QString& name){for(auto* a:w.findChildren<QAction*>())if(a->text()==name){check(a->isEnabled(),"Actual History action enabled");a->trigger();events();return;}throw std::runtime_error("Actual History action missing");}
Object rectangle(const Id& id,double x,double y,double width,double height){
    Object o;o.id=id;o.name=id;Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(auto p:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}){Point point;point.id=id+"-point-"+std::to_string(contour.points.size());point.x.literal=p.x;point.y.literal=p.y;contour.points.push_back(point);}
    o.contours={contour};auto fill=default_operation(id+"-fill","nect.paint.fill");fill.parameters.at("r").literal=id=="a"?0.75:0.125;fill.parameters.at("g").literal=id=="b"?0.75:0.125;fill.parameters.at("b").literal=id=="c"?0.75:0.125;
    auto stroke=default_operation(id+"-stroke","nect.paint.stroke");stroke.parameters.at("width").literal=2.125;o.stack={fill,stroke};o.anchor={Scalar{3.125,{}},Scalar{4.25,{}}};o.compositing.opacity.literal=0.8125;return o;
}
Document fixture(){
    auto d=empty_document("inspector-transform-entry-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=320;board.height=240;
    d.objects.emplace("a",rectangle("a",0.125,0.125,10,10));d.objects.emplace("b",rectangle("b",30.25,25.25,20,10));d.objects.emplace("c",rectangle("c",90.375,60.375,10,10));
    auto other=rectangle("other",210,120,40,30);other.transform[4].literal=0.123456789012345;d.objects.emplace("other",other);d.compositions.front().roots={"a","b","c","other"};return d;
}
void flow(const QString& root,int mode){
    const bool success=mode==2||mode==3,stale=mode==0||mode==1||mode==5,busy_case=mode==5;
    auto initial=fixture();valid(initial);auto current=initial;current.objects.at("other").name="Retained caller";valid(current);
    Session base(initial);base.apply({Rename{"other","Retained caller"}},0);check(base.document()==current,"Whole independent original caller BEFORE Window");
    auto foreign_initial=initial;foreign_initial.id="foreign-inspector-transform-entry";foreign_initial.objects.at("b").compositing.opacity.literal=0.625;valid(foreign_initial);
    auto foreign_current=foreign_initial;foreign_current.objects.at("other").name="Retained caller";valid(foreign_current);Session foreign(foreign_initial);foreign.apply({Rename{"other","Retained caller"}},0);
    check(foreign.document()==foreign_current&&foreign.revision()==base.revision()&&foreign.gesture_generation()==base.gesture_generation(),"Full independent foreign native/counter oracle BEFORE Window");
    auto caller=mode==0||busy_case?foreign:base;
    if(mode==3){caller.apply({Rename{"other","Unrelated current edit"}},1);current.objects.at("other").name="Unrelated current edit";valid(current);check(caller.document()==current,"Unrelated same-Document oracle BEFORE Window");}
    if(busy_case){caller.begin_gesture(caller.revision());caller.update_gesture({Set{{"a","a-point-0","x"},7.125}});auto preview=foreign_current;preview.objects.at("a").contours.front().points.front().x.literal=7.125;valid(preview);check(caller.document()==foreign_current&&caller.preview_document()==preview,"Full independent foreign busy native/preview/token BEFORE Window");}
    auto authored=current;for(const auto* id:{"a","b","c"}){authored.objects.at(id).transform[0].literal=2;authored.objects.at(id).transform[3].literal=2;}valid(authored);
    auto committed=mode==3?caller:base;committed.apply({TransformObjects{{"a","b","c"},0,2,2,std::array<double,2>{0,0}}},committed.revision());
    check(committed.document()==authored,"Manual whole native 2x affine about explicit origin equals typed Session BEFORE Window");
    auto api=mode==3?caller:base;const auto response=request(api,std::string{"{\"op\":\"apply\",\"expected_revision\":"}+std::to_string(api.revision())+",\"commands\":[{\"type\":\"transform_objects\",\"objects\":[\"a\",\"b\",\"c\"],\"rotation\":0,\"scale_x\":2,\"scale_y\":2,\"pivot\":[0,0]}]}");check(response.find("\"ok\":true")!=std::string::npos&&same(api,committed),"Whole actual semantic API/History oracle BEFORE Window");
    auto undo=committed;undo.undo(undo.revision());check(undo.document()==current,"Whole independent Undo BEFORE Window");undo.redo(undo.revision());check(undo.document()==authored&&undo.history()==committed.history(),"Whole independent Redo BEFORE Window");
    const auto expected=success?committed:caller;const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
    QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},prefix+"-assets"),&settings);
    w.host.session=base;w.host.edited();w.resize(1400,1150);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selections({{"a",{}},{"b",{}},{"c",{}}});events();
    QPointer<QPushButton> actor=control<QPushButton>(w,"selection-transform-open");reveal(actor);check(same(w.host.session,base),"Original Inspector opener navigation Session neutral");const auto selection=w.canvas->selections();const auto zoom=w.canvas->zoom();const auto original_host=w.host.session_id;
    check(w.grab().save(prefix+"-before.png"),"Original styled multi-Inspector opener BEFORE caller replacement");if(mode==0||mode==3||busy_case)w.host.session=caller;if(mode==1)w.host.session_id+="-changed";
    int modal_count=0;QString callback_error;
    QTimer::singleShot(0,&w,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
        try{check(dialog->objectName()=="selection-transform-dialog","Exact production transform modal");++modal_count;auto* sx=dialog->findChild<QDoubleSpinBox*>("selection-scale-x");auto* sy=dialog->findChild<QDoubleSpinBox*>("selection-scale-y");auto* pivot=dialog->findChild<QComboBox*>("selection-pivot-mode");auto* px=dialog->findChild<QDoubleSpinBox*>("selection-pivot-x");auto* py=dialog->findChild<QDoubleSpinBox*>("selection-pivot-y");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(sx&&sy&&pivot&&px&&py&&buttons,"Original complete scale/pivot/Apply actors");sx->setValue(200);pivot->setCurrentIndex(1);px->setValue(0);py->setValue(0);check(sy->value()==200&&px->isEnabled()&&py->isEnabled(),"Original linked scale and explicit world pivot");check(dialog->grab().save(prefix+"-dialog.png"),"Original modal captured before Apply/Cancel");if(mode==4)buttons->button(QDialogButtonBox::Cancel)->click();else buttons->button(QDialogButtonBox::Ok)->click();}
        catch(const std::exception& error){callback_error=QString::fromUtf8(error.what());dialog->reject();}});
    actor->click();events();check(callback_error.isEmpty(),"No modal fixture exception");const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual original pre-modal producer result BEFORE Save");
    QJsonObject evidence{{"mode",mode},{"modal_count",modal_count},{"status",status},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"caller_token_equal",observed.gesture_identity()==expected.gesture_identity()},{"host_equal",mode==1?w.host.session_id!=original_host:w.host.session_id==original_host},{"view_equal",w.canvas->selections()==selection&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"&&w.canvas->zoom()==zoom},{"save_intent",!busy_case}};
    QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable whole caller/modal/preview/History/view before Save");metadata.write(QJsonDocument(evidence).toJson());metadata.close();
    check(same(observed,expected),"Whole actual Inspector-entry caller/native/History/preview matches independent expected");check(evidence["view_equal"].toBool()&&evidence["host_equal"].toBool()&&evidence["caller_token_equal"].toBool(),"Actual entire caller selection/zoom/Host/preview token retained");check(modal_count==(stale?0:1),"Stale Inspector opener refuses BEFORE modal admission; current opens exactly once");if(stale)check(status.startsWith("SESSION_CONFLICT:"),"Stale Inspector opener explicitly refused");
    if(success){history(w,"Undo");check(w.host.session.document()==current,"Actual one Undo restores complete original caller");history(w,"Redo");check(w.host.session.document()==authored,"Actual Redo restores whole current transform");w.host.session=observed;}
    if(!busy_case){w.host.save(prefix+"-saved.nect");w.host.flush();events();check(same(w.host.session,expected),"Save preserves full caller/History");check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Exact saved whole native bytes");}w.host.changed={};w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);
    try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original multi-Inspector transform opener; physical/clipboard/fonts/pixels NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_INSPECTOR_TRANSFORM_ENTRY_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<6;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& error){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<error.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Inspector transform entry cases failed: "+std::to_string(failures));std::cout<<"PASS Inspector transform entry "<<checks<<" checks\n";QTest::qWait(300);return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL Inspector transform entry "<<error.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
