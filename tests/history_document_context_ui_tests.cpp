#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
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
Scalar& coordinate(Document& document,const Ref& ref){
 return document.objects.at(ref.object).source->parameters.at(ref.field.substr(10));
}
Document fixture(){
 auto d=empty_document("ellipse-dimension-document","composition","artboard");
 auto upstream=default_primitive("source-source","nect.shape.ellipse");upstream.parameters.at("width").literal=20.25;upstream.parameters.at("height").literal=8.75;upstream.parameters.at("center_x").literal=20.25;upstream.parameters.at("center_y").literal=-8.75;
 auto first=default_primitive("first-source","nect.shape.ellipse");first.parameters.at("width").literal=100.25;first.parameters.at("height").literal=100.25;first.parameters.at("center_x").literal=-30.25;first.parameters.at("center_y").literal=-30.25;
 auto second=default_primitive("second-source","nect.shape.ellipse");second.parameters.at("width").literal=200.25;second.parameters.at("height").literal=200.25;second.parameters.at("center_x").literal=77.25;second.parameters.at("center_y").literal=77.25;
 Session setup(d);setup.apply({CreatePrimitive{"composition","","source","Same name",upstream},CreatePrimitive{"composition","","first","Same name",first},CreatePrimitive{"composition","","second","Same name",second}},0);
 d=setup.document();d.objects.at("first").stack.push_back(default_operation("first-fill","nect.paint.fill"));d.objects.at("second").stack.push_back(default_operation("second-fill","nect.paint.fill"));
 d.objects.at("first").transform[4].literal=100.25;d.objects.at("second").transform[4].literal=400.25;
 auto follower=d.objects.at("source");follower.id="follower";follower.source->id="follower-source";if(!follower.legacy_stroke.empty())follower.legacy_stroke="follower-"+follower.legacy_stroke;for(auto& operation:follower.stack)operation.id="follower-"+operation.id;d.objects.emplace(follower.id,follower);d.compositions.front().roots.push_back(follower.id);
 for(const auto& id:std::vector<Id>{"first","second"}){auto& obj=d.objects.at(id);obj.transform[0].literal=.9;obj.transform[1].literal=.2;obj.transform[2].literal=-.1;obj.transform[3].literal=1.1;obj.anchor[0].literal=7.25;obj.anchor[1].literal=-2.75;obj.compositing.opacity.literal=.65;}
 return d;
}

QJsonArray view_json(const std::vector<Canvas::Selection>& selection){QJsonArray result;for(const auto& v:selection)result.append(QJsonObject{{"object",QString::fromStdString(v.object)},{"point",QString::fromStdString(v.point)}});return result;}
void view(Window& w,const std::vector<Canvas::Selection>& selection,const Id& board){check(w.canvas->selections()==selection&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()==board,"Exact independent manualCanvas selectedIDs/Composition/Artboard view");}

void flow(const QString& root,int mode){
 auto initial=fixture();coordinate(initial,{"first","","generator.height"}).binding=Binding{{"source","","generator.height"},1.25,.5};coordinate(initial,{"second","","generator.width"}).expression=Expression{"32.5",1};valid(initial);
 const Ref width{"first","","generator.width"},center{"first","","generator.center_y"};auto first_doc=initial;coordinate(first_doc,width).literal=144.25;valid(first_doc);auto current_doc=first_doc;coordinate(current_doc,center).literal=-80.25;valid(current_doc);
 Session base(initial);base.apply({Set{width,144.25}},0);check(base.document()==first_doc&&base.history().current_id==1,"Independent full first state BEFORE Window");base.apply({Set{center,-80.25}},1);check(base.document()==current_doc&&base.revision()==2&&base.history().current_id==2&&base.history().states.size()==3,"Independent full second state/History BEFORE Window");
 auto caller=base;auto caller_doc=current_doc;
 if(mode==0){auto foreign=initial;foreign.id="foreign-history-document";auto foreign_first=foreign;coordinate(foreign_first,width).literal=86.25;auto foreign_current=foreign_first;coordinate(foreign_current,center).literal=72.25;valid(foreign);valid(foreign_first);valid(foreign_current);caller=Session(foreign);caller.apply({Set{width,86.25}},0);check(caller.document()==foreign_first,"Independent foreign first state BEFORE Window");caller.apply({Set{center,72.25}},1);caller_doc=foreign_current;check(caller.document()==caller_doc&&caller.revision()==base.revision()&&caller.history().current_id==base.history().current_id,"Native foreign History shares numeric state IDs/revision BEFORE Window");}
 if(mode==2){coordinate(caller_doc,{"second","","generator.center_x"}).literal=91.25;valid(caller_doc);caller.apply({Set{{"second","","generator.center_x"},91.25}},2);check(caller.document()==caller_doc&&caller.revision()==3,"Independent stale list revision BEFORE Window");}
 if(mode==5){auto preview_doc=current_doc;coordinate(preview_doc,width).literal=188.25;valid(preview_doc);caller.begin_gesture(2);caller.update_gesture({Set{width,188.25}});check(caller.document()==current_doc&&caller.preview_document()==preview_doc&&caller.gesture_active()&&caller.history()==base.history(),"Independent foreign preview/History BEFORE Window");}
 const bool success=mode==3||mode==4;const auto selected_id=mode==4?2ULL:1ULL;auto expected=caller;auto expected_doc=caller_doc;
 if(mode==3){expected_doc=first_doc;expected.restore_history(selected_id,2);auto history=base.history();history.current_id=1;check(expected.document()==expected_doc&&expected.history()==history&&expected.revision()==3&&expected.preview_document()==expected_doc&&!expected.gesture_active(),"Independent manual full History navigation BEFORE Window");}
 if(mode==4){expected.restore_history(selected_id,2);same(expected,caller);check(expected.revision()==2,"Current state is full Session no-op BEFORE Window");}
 auto undone=expected,redone=expected;if(success){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());check(undone.document()==(mode==3?initial:first_doc)&&redone.document()==expected_doc&&redone.history()==expected.history()&&undone.revision()==expected.revision()+1&&redone.revision()==expected.revision()+2,"Independent full UndoRedo native/History BEFORE Window");}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",expected_doc);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();const std::vector<Canvas::Selection> selection{{"first",{}}};w.canvas->set_selections(selection);events();view(w,selection,"artboard");
 auto* action=w.findChild<QAction*>("show-history");check(action,"Original View History action");QSignalSpy opened(action,&QAction::triggered);action->trigger();events();QPointer<QDialog> dialog=w.findChild<QDialog*>("history-dialog");check(dialog&&dialog->isVisible(),"Original History dialog visible");auto* list=dialog->findChild<QListWidget*>("history-states");QPointer<QPushButton> restore=dialog->findChild<QPushButton*>("history-restore");check(list&&restore&&list->count()==3,"Original three native History states");QListWidgetItem* row=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toULongLong()==selected_id)row=list->item(i);check(row,"Exact retained History state ID");QSignalSpy selected(list,&QListWidget::currentItemChanged);list->setCurrentItem(row);events();same(w.host.session,base);const auto host_session=w.host.session_id;w.host.session=caller;if(mode==1)w.host.session_id="foreign-history-host-session";same(w.host.session,caller);view(w,selection,"artboard");check(dialog->grab().save(path+"-history.png"),"Original styled History selected row BEFORE Return");reveal(restore);QSignalSpy returned(restore,&QPushButton::clicked);restore->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actual Window after original History Return");
 QJsonObject v{{"mode",mode},{"selected_history_id",static_cast<qint64>(selected_id)},{"open_signal_count",opened.count()},{"return_signal_count",returned.count()},{"status",status},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",static_cast<qint64>(observed.revision())},{"expected_revision",static_cast<qint64>(expected.revision())},{"gesture_generation",static_cast<qint64>(observed.gesture_generation())},{"expected_gesture_generation",static_cast<qint64>(expected.gesture_generation())},{"gesture_active",observed.gesture_active()},{"expected_gesture_active",expected.gesture_active()},{"dialog_visible_after",dialog&&dialog->isVisible()},{"view_equal",w.canvas->selections()==selection&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="artboard"},{"save_run",mode!=5}};QFile state(path+"-producer.json");check(state.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable History actor readback BEFORE Save");state.write(QJsonDocument(v).toJson());state.close();
 check(opened.count()==1&&returned.count()==1&&dialog&&dialog->isVisible(),"Original History open/Return once and dialog retained");same(w.host.session,expected);view(w,selection,"artboard");const QString error=mode<2?"SESSION_CONFLICT: This History list belongs to another document session":mode==2?"REVISION_CONFLICT: Refresh revision before editing":"GESTURE_ACTIVE: Finish or cancel the current gesture before another edit";check(success?status.isEmpty():status==error,"Exact History context refusal or current navigation");
 if(mode!=5){w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Full History caller/returned native Save");}
 if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{
 std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" actual product style/Qt History action; physical/clipboard/fonts/geometry NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_HISTORY_DOCUMENT_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;
 for(int mode=0;mode<6;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
 if(failures)throw std::runtime_error("History document context cases failed: "+std::to_string(failures));std::cout<<"PASS History document context "<<checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL History document context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
