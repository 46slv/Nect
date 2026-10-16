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
void flow(const QString& root,int mode){const int kind=mode<10?mode/2:mode-10;const bool absent=mode>=10,replaced=mode<10&&mode%2==0,success=mode<10&&mode%2==1;auto initial=fixture(false);initial.id="mask-source-identity";GeometryMask mask{"mask","peer"};mask.mode=kind==3?"geometry":"alpha";initial.objects.at("shape").compositing.mask=mask;initial.objects.at("shape").compositing.opacity.literal=0.375;valid(initial);auto current=initial;current.objects.at("other").name="Current retained";valid(current);Session base(initial);base.apply({Rename{"other","Current retained"}},0);check(base.document()==current&&base.revision()==1,"Entire independent installed Mask/source/artwork/History before Window");auto caller_initial=initial;if(replaced){caller_initial.objects.at("shape").compositing.mask->id="replacement-mask";caller_initial.objects.at("shape").compositing.mask->source="other";}else if(absent)caller_initial.objects.at("shape").compositing.mask.reset();valid(caller_initial);auto caller_current=caller_initial;caller_current.objects.at("other").name="Current retained";valid(caller_current);Session caller(caller_initial);caller.apply({Rename{"other","Current retained"}},0);check(caller.document()==caller_current&&caller.document().id==base.document().id&&caller.revision()==base.revision()&&caller.gesture_generation()==base.gesture_generation(),"Whole same-Document equal-counter replacement/absent Mask caller before Window");if(replaced||absent){bool refused=false;try{(void)geometry_mask_enabled_state(caller_current,geometry_mask_enabled_ref("shape","mask"));}catch(const Error& e){refused=e.code=="MISSING_MASK";}check(refused,"Canonical pure original Mask Ref absent/replaced before Window");}auto transformed=caller_current;auto committed=caller;if(!absent){if(kind==4)transformed.objects.at("shape").compositing.mask.reset();else{auto& changed=*transformed.objects.at("shape").compositing.mask;if(kind==0)changed.mode="luma";else if(kind==1)changed.invert=true;else if(kind==2)changed.enabled=false;else changed.fill_rule="evenodd";}valid(transformed);committed.apply({SetMask{"shape",transformed.objects.at("shape").compositing.mask}},1);check(committed.document()==transformed,"Whole independently declared exact Mask field/removal delta matches canonical command before Window");auto history=committed;history.undo(2);check(history.document()==caller_current,"Full Mask/source Undo before Window");history.redo(3);check(history.document()==transformed&&history.history()==committed.history(),"One History/full Mask/source Redo before Window");}auto expected=success?committed:caller;const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},prefix+"-assets"),&settings);w.host.session=base;w.host.edited();w.resize(1400,1150);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("shape","");events();QPointer<QWidget> actor;if(kind==0||kind==3)actor=control<QComboBox>(w,kind==0?"mask-mode":"mask-fill-rule");else if(kind==4)actor=control<QPushButton>(w,"mask-remove");else actor=control<QCheckBox>(w,kind==1?"mask-invert":"mask-enabled");reveal(actor);check(w.grab().save(prefix+"-before.png"),"Original retained Mask actor before source replacement");const auto original_host=w.host.session_id;if(replaced||absent)w.host.session=caller;if(kind==0||kind==3){auto* selector=qobject_cast<QComboBox*>(actor.data());const auto index=selector->findData(kind==0?"luma":"evenodd");check(index>=0,"Original supported Mask choice");selector->setCurrentIndex(index);}else if(kind==4)qobject_cast<QPushButton*>(actor.data())->click();else qobject_cast<QCheckBox*>(actor.data())->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Whole original Mask result before Save");QJsonObject v{{"mode",mode},{"kind",kind},{"absent",absent},{"status",status},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"host_equal",w.host.session_id==original_host},{"view_equal",w.canvas->selected_object=="shape"&&w.canvas->selected_point.empty()&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"save_intent",true}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable whole Mask/source/native/History before Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["host_equal"].toBool(),"Same Document/Host and source view preserved");if(replaced||absent)check(status.startsWith("MISSING_MASK:"),"Original typed Mask Ref required before nullable payload read/command");else{w.host.session.undo(2);check(w.host.session.document()==caller_current,"Actual complete Mask/source Undo");w.host.session.redo(3);check(w.host.session.document()==transformed,"Actual complete Mask/source Redo");w.host.session=observed;}w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Whole exact Mask/source native Save/snapshot/no normalization");w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{const auto limit=qEnvironmentVariableIntValue("NECT_MASK_SOURCE_IDENTITY_CASES");const int count=limit==0?15:limit;check(count==10||count==15,"Frozen mask matrix: first10 RED, five missing-mask negatives added after repair");std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Mask Mode/Invert/Enabled/Fill rule/Remove, cases="<<count<<"; physical/clipboard/fonts/AE/pixels NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MASK_SOURCE_IDENTITY_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<count;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Mask source identity cases failed: "+std::to_string(failures));std::cout<<"PASS Mask source identity "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Mask source identity "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
