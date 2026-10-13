#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <iostream>
#include <algorithm>
#include <memory>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace{
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full caller native-valid BEFORE Window");}
void same(const Session& a,const Session& b){check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Entire caller Session/native/History/preview/revision/generation unchanged");}
void write(const QString& path,const Document& d){QFile f(path);const auto b=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(b.data(),b.size())==static_cast<qint64>(b.size()),"Immutable actual or independent native retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error("Original visible source producer missing");}
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(alive&&alive->isVisible()&&alive->isEnabled(),"Original source producer survives reveal and is enabled");}
const Id contour_a="117a738e-06ad-4df0-b7ad-2c84c5080c2b",contour_b="db17d798-16e8-42b3-98f8-4344cf027a76";
Contour line(const Id& id,double y){Contour c;c.id=id;for(auto xy:std::vector<Vec2>{{100,y},{400,y}}){Point p;p.id=id+std::to_string(c.points.size());p.x.literal=xy.x;p.y.literal=xy.y;c.points.push_back(p);}return c;}
Document fixture(){
    Session s(empty_document("follow-context","comp","art"));
    auto child=default_primitive("child-source","nect.shape.rectangle");child.parameters.at("center_x").literal=0;child.parameters.at("center_y").literal=0;child.parameters.at("width").literal=20;child.parameters.at("height").literal=16;
    auto second=child;second.id="second-source";second.parameters.at("center_y").literal=40;
    auto other=child;other.id="other-source";other.parameters.at("center_x").literal=520;other.parameters.at("center_y").literal=100;
    s.apply({CreatePrimitive{"comp","","child","Label background",child},CreatePrimitive{"comp","","second","Editable accent",second},GroupContiguous{"comp","",{"child","second"},"group","Followed heading"},
        CreatePath{"comp","","decoy","Baseline guide for retained heading and margin placement",{line("decoy-contour",300)}},CreatePath{"comp","","guide","Baseline guide for retained heading and margin placement",{line(contour_a,150),line(contour_b,250)}},CreatePrimitive{"comp","","other","Other artwork",other}},s.revision());
    auto d=s.document();d.compositions.front().artboards.front().width=640;d.compositions.front().artboards.front().height=480;
    auto art=d.compositions.front().artboards.front();art.id="other-art";art.name="Other Artboard";art.x=700;d.compositions.front().artboards.push_back(art);
    auto red=default_operation("child-fill","nect.paint.fill");red.parameters.at("r").literal=1;red.parameters.at("g").literal=0;red.parameters.at("b").literal=0;d.objects.at("child").stack.push_back(red);
    auto blue=red;blue.id="other-fill";blue.parameters.at("r").literal=0;blue.parameters.at("b").literal=1;d.objects.at("other").stack.push_back(blue);return d;
}

void view(Window& w){check(w.canvas->selected_object=="group"&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art","Exact independent Group/Composition/Artboard view");}
void flow(const QString& root,int mode){
 auto initial=fixture();initial.objects.at("child").source->parameters.at("width").binding=Binding{{"other","","generator.width"},1.25,.5};initial.objects.at("other").source->parameters.at("height").expression=Expression{"16.25",1};valid(initial);
 GroupPathFollow relation;relation.id="retained-relation";relation.path="guide";relation.contour=contour_b;relation.start=40.1234567890123;relation.normal_offset=10.2345678901234;relation.items={{"child",{4.3456789012345,-3.4567890123456,true}},{"second",{60.5678901234567,8.6789012345678,false}}};auto current=initial;current.objects.at("group").path_follow=relation;valid(current);Session base(initial);base.apply({GroupPathFollowCommand{AttachGroupPathFollow{"group",relation}}},0);check(base.document()==current&&base.preview_document()==current&&base.revision()==1&&base.history().current_id==1,"Independent full retained relation/Hierarchy/Session/History BEFORE Window");
 auto caller=base;auto caller_doc=current;if(mode<2){auto foreign=initial;foreign.id="foreign-follow-document";auto f=current;f.id=foreign.id;auto rr=relation;rr.start=55.125;f.objects.at("group").path_follow=rr;valid(foreign);valid(f);caller=Session(foreign);caller.apply({GroupPathFollowCommand{AttachGroupPathFollow{"group",rr}}},0);caller_doc=f;check(caller.document()==f&&caller.revision()==base.revision(),"Independent foreign Document/coincident revision BEFORE Window");}
 if(mode==3){auto rr=relation;rr.start=55.125;caller_doc.objects.at("group").path_follow=rr;valid(caller_doc);caller.apply({GroupPathFollowCommand{UpdateGroupPathFollow{"group",rr}}},1);check(caller.document()==caller_doc&&caller.revision()==2,"Independent externally committed relation BEFORE Window");}
 if(mode==4){auto rr=relation;rr.id="replacement-follow-relation";rr.start=55.125;caller_doc.objects.at("group").path_follow=rr;valid(caller_doc);caller=Session(initial);caller.apply({GroupPathFollowCommand{AttachGroupPathFollow{"group",rr}}},0);check(caller.document()==caller_doc&&caller.revision()==1,"Independent raw relation replacement at coincident revision BEFORE Window");}
 if(mode==5){auto preview=current;preview.objects.at("second").source->parameters.at("center_y").literal=48;valid(preview);caller.begin_gesture(1);caller.update_gesture({Set{{"second","","generator.center_y"},48}});check(caller.document()==current&&caller.preview_document()==preview&&caller.history()==base.history(),"Independent complete active preview BEFORE Window");}
 if(mode==8){auto changed=initial;changed.objects.at("guide").contours.back().points.front().y.literal=270;changed.objects.at("guide").contours.back().points.back().y.literal=270;valid(changed);caller_doc=changed;caller_doc.objects.at("group").path_follow=relation;valid(caller_doc);caller=Session(changed);caller.apply({GroupPathFollowCommand{AttachGroupPathFollow{"group",relation}}},0);check(caller.document()==caller_doc&&caller.revision()==1,"Independent changed source Path reevaluation at retained IDs/relation BEFORE Window");}
 const bool success=mode>=6;const bool clear=mode==1||mode==7;auto expected=caller;auto expected_doc=caller_doc;auto target=relation;target.start=60.25;
 if(success){if(clear){expected_doc.objects.at("group").path_follow.reset();expected.apply({GroupPathFollowCommand{ClearGroupPathFollow{"group"}}},1);}else{expected_doc.objects.at("group").path_follow=target;expected.apply({GroupPathFollowCommand{UpdateGroupPathFollow{"group",target}}},1);}valid(expected_doc);check(expected.document()==expected_doc&&expected.preview_document()==expected_doc&&expected.revision()==2&&expected.history().current_id==2&&expected.history().states.size()==3,"Independent manual full Apply/Clear/one History BEFORE Window");auto undo=expected;undo.undo(2);check(undo.document()==caller_doc,"Independent full relation Undo BEFORE Window");undo.redo(3);check(undo.document()==expected_doc&&undo.history()==expected.history(),"Independent full relation Redo BEFORE Window");}
 const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected_doc);QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("group");events();view(w);QPointer<QPushButton> button=control<QPushButton>(w,clear?"group-path-follow-clear":"group-path-follow-apply");auto* start=control<QDoubleSpinBox>(w,"group-path-follow-start");reveal(button);if(!clear){start->setValue(60.25);events();}same(w.host.session,base);const auto host_id=w.host.session_id;w.host.session=caller;if(mode==2)w.host.session_id="foreign-follow-host-session";same(w.host.session,caller);view(w);check(w.grab().save(prefix+"-before.png"),"Actual styled retained Follow controls BEFORE action");QSignalSpy invoked(button,&QPushButton::clicked);button->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());check(w.grab().save(prefix+"-window.png"),"Actual styled Follow result BEFORE Save");QJsonObject v{{"mode",mode},{"action",clear?"clear":"apply"},{"invoke_signal_count",invoked.count()},{"status",status},{"document_equal",observed.document()==expected_doc},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_generation",qint64(observed.gesture_generation())},{"gesture_active",observed.gesture_active()},{"view_equal",w.canvas->selected_object=="group"&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"save_run",mode!=5}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable original actor/native readback BEFORE Save");metadata.write(QJsonDocument(v).toJson());metadata.close();check(invoked.count()==1,"Original Apply/Clear once");same(observed,expected);view(w);
 const auto error=mode<3?"SESSION_CONFLICT: Group Path Follow belongs to another document":mode==3?"REVISION_CONFLICT: Group Path Follow changed; refresh the current Group":mode==4?"GROUP_PATH_FOLLOW_ID_MISMATCH: Updating a Group Path Follow relation must retain its stable ID":"GESTURE_ACTIVE: Finish or cancel the current gesture before another edit";check(success?status.isEmpty():status==error,"Exact retained Follow refusal/current success");
 if(mode!=5){w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Full Follow caller/returned native Save");}if(success){w.host.session.undo(w.host.session.revision());check(w.host.session.document()==caller_doc,"Actual full Follow Undo");w.host.session.redo(w.host.session.revision());check(w.host.session.document()==expected_doc&&w.host.session.history()==expected.history(),"Actual full Follow Redo");}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" actual product style/Qt Follow Apply-Clear; physical/clipboard/fonts/geometry NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GROUP_FOLLOW_ACTION_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<9;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Group Follow action context cases failed: "+std::to_string(failures));std::cout<<"PASS Group Follow action context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Group Follow action context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
