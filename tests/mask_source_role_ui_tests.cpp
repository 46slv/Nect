#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include "semantic_control.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QInputDialog>
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

Document fixture(bool linked){auto d=empty_document("point-edit-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;for(const auto& id:{Id{"shape"},Id{"peer"},Id{"other"}}){Object object;object.id=id;object.name=id;object.source=default_primitive(id+"-source","nect.shape.rectangle");object.source->parameters.at("width").literal=80;object.source->parameters.at("height").literal=60;object.source->parameters.at("center_x").literal=id=="shape"?320:id=="peer"?180:460;object.source->parameters.at("center_y").literal=240;auto fill=default_operation(id+"-fill","nect.paint.fill");fill.parameters.at("r").literal=id=="shape"?1:0;fill.parameters.at("g").literal=id=="other"?1:0;fill.parameters.at("b").literal=id=="peer"?1:0;object.stack={fill};if(id!="other"){PointEdit correction;correction.id=id+"-source-point-edit";correction.enabled=id=="shape";correction.overrides[id+"-source-top-left"]["x"].literal=id=="shape"?285:145;object.point_edit=correction;}d.objects.emplace(id,object);d.compositions.front().roots.push_back(id);}if(linked)d.objects.at("shape").point_edit->enabled_driver=point_edit_enabled_ref("peer","peer-source-point-edit");return d;}
void flow(const QString& root,int mode){const bool visibility=mode==2||mode==3||mode==5||mode==7||mode==9,success=mode==1||mode==3,foreign_case=mode==4||mode==5,association=mode==0||mode==2,replaced=mode==6||mode==7,absent=mode>=8;auto initial=fixture(false);initial.id="mask-source-role";GeometryMask mask{"mask","peer"};mask.mode="alpha";initial.objects.at("shape").compositing.mask=mask;initial.objects.at("shape").compositing.opacity.literal=0.375;valid(initial);auto current=initial;current.objects.at("other").name="Current retained";valid(current);Session base(initial);base.apply({Rename{"other","Current retained"}},0);check(base.document()==current&&base.revision()==1,"Full independently authored Mask/source roles before Window");auto caller_initial=initial;if(association)caller_initial.objects.at("shape").compositing.mask->source="other";if(foreign_case){caller_initial.id="foreign-mask-source-role";caller_initial.objects.at("other").source->parameters.at("height").literal=90;}if(replaced)caller_initial.objects.at("shape").compositing.mask->id="replacement-mask";if(absent)caller_initial.objects.at("shape").compositing.mask.reset();valid(caller_initial);auto caller_current=caller_initial;caller_current.objects.at("other").name="Current retained";valid(caller_current);Session caller(caller_initial);caller.apply({Rename{"other","Current retained"}},0);check(caller.document()==caller_current&&caller.revision()==base.revision()&&caller.gesture_generation()==base.gesture_generation(),"Whole independent changed association/Doc/Mask ID/missing caller before Window");auto transformed=caller_current;transformed.objects.at("peer").visible=false;valid(transformed);auto committed=caller;committed.apply({SetVisibility{"peer",false}},1);check(committed.document()==transformed,"Independent source visibility only delta before Window");auto history=committed;history.undo(2);check(history.document()==caller_current,"Whole source visibility Undo before Window");history.redo(3);check(history.document()==transformed&&history.history()==committed.history(),"One History/full source visibility Redo before Window");auto expected=mode==3?committed:caller;const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},prefix+"-assets"),&settings);w.host.session=base;w.host.edited();w.resize(1400,1150);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("shape","");events();QPointer<QWidget> actor=visibility?static_cast<QWidget*>(control<QCheckBox>(w,"mask-source-visible")):static_cast<QWidget*>(control<QPushButton>(w,"mask-edit-source"));reveal(actor);const auto original_host=w.host.session_id;const auto zoom=w.canvas->zoom();check(w.grab().save(prefix+"-before.png"),"Original retained source role before caller changes");if(!success)w.host.session=caller;if(visibility)qobject_cast<QCheckBox*>(actor.data())->click();else qobject_cast<QPushButton*>(actor.data())->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual original source role result before Save");QJsonObject v{{"mode",mode},{"visibility",visibility},{"status",status},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"host_equal",w.host.session_id==original_host},{"view_equal",w.canvas->selected_object==(mode==1?"peer":"shape")&&w.canvas->selected_point.empty()&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"&&w.canvas->zoom()==zoom},{"selected_object",QString::fromStdString(w.canvas->selected_object)},{"save_intent",true}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable full source role/native/History/view before Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["host_equal"].toBool(),"Original source association navigation or refusal preserves exact caller view/zoom");if(association)check(status.startsWith("PROPERTY_CONFLICT:"),"Changed Mask source association refused explicitly");if(foreign_case)check(status.startsWith("SESSION_CONFLICT:"),"Foreign Document source role refused explicitly");if(replaced||absent)check(status.startsWith("MISSING_MASK:"),"Replaced/absent Mask role refused explicitly");if(mode==3){w.host.session.undo(2);check(w.host.session.document()==caller_current,"Actual full source visibility Undo");w.host.session.redo(3);check(w.host.session.document()==transformed,"Actual full source visibility Redo");w.host.session=observed;}w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Whole exact source role native Save/snapshot/no normalization");w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Mask Edit source/Show source roles; physical/clipboard/fonts/AE/pixels NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MASK_SOURCE_ROLE_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<10;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Mask source role cases failed: "+std::to_string(failures));std::cout<<"PASS Mask source role "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Mask source role "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
