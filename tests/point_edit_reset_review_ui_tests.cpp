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
void flow(const QString& root,int mode){auto initial=fixture(false);valid(initial);auto current=initial;current.objects.at("other").name="Current retained";valid(current);Session base(initial);base.apply({Rename{"other","Current retained"}},0);check(base.document()==current&&base.revision()==1,"Whole one-field original correction before Window");auto reviewed=current;reviewed.objects.at("shape").point_edit->overrides.at("shape-source-top-left")["y"].literal=215;valid(reviewed);auto caller=base;caller.apply({Set{{"shape","shape-source-top-left","y"},215}},1);check(caller.document()==reviewed&&caller.revision()==2,"Whole independently declared two-field reviewed correction matches canonical edit before Window");auto concurrent_doc=reviewed;concurrent_doc.objects.at("shape").point_edit->overrides.at("shape-source-top-left").at("x").literal=286;valid(concurrent_doc);auto concurrent=caller;concurrent.apply({Set{{"shape","shape-source-top-left","x"},286}},2);check(concurrent.document()==concurrent_doc,"Whole independent caller changed during review before Window");auto cleared=reviewed;cleared.objects.at("shape").point_edit.reset();valid(cleared);auto committed=caller;committed.apply({ClearPointEdit{"shape"}},2);check(committed.document()==cleared,"Full independent exact reviewed correction removal/source preservation before Window");auto history=committed;history.undo(3);check(history.document()==reviewed,"Whole reviewed correction Undo before Window");history.redo(4);check(history.document()==cleared&&history.history()==committed.history(),"One History/full reviewed correction Redo before Window");auto expected=mode==1?committed:mode==2?concurrent:caller;const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());const QString expected_question="Remove all 2 point/handle overrides and their bindings? The generator and appearance stay editable. This is one undoable edit.";
QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},prefix+"-assets"),&settings);w.host.session=base;w.host.edited();w.resize(1400,1150);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("shape","");events();auto* actor=control<QPushButton>(w,"point-edit-reset");reveal(actor);const auto original_host=w.host.session_id;QString callback_error,question_text;int stages=0;QTimer::singleShot(0,&w,[&]{try{auto* question=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());check(question&&question->windowTitle()=="Reset point edits"&&question->button(QMessageBox::Reset)&&question->button(QMessageBox::Cancel),"Original Reset review/choice producers");++stages;question_text=question->text();check(question->grab().save(prefix+"-dialog.png"),"Actual original reviewed count and confirmation before choice");if(mode==2)w.host.session=concurrent;question->button(mode==0?QMessageBox::Cancel:QMessageBox::Reset)->click();}catch(const std::exception& e){callback_error=QString::fromUtf8(e.what());if(auto* question=qobject_cast<QDialog*>(QApplication::activeModalWidget()))question->reject();}});w.host.session=caller;actor->click();events();check(callback_error.isEmpty()&&stages==1,"Original reviewed correction modal completes without fixture exception");const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual entire reviewed correction result before Save");QJsonObject v{{"mode",mode},{"status",status},{"question_text",question_text},{"question_equal",question_text==expected_question},{"modal_stages",stages},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"host_equal",w.host.session_id==original_host},{"view_equal",w.canvas->selected_object=="shape"&&w.canvas->selected_point.empty()&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"save_intent",true}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable whole reviewed correction/native/History before Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["host_equal"].toBool(),"Source/caller view preserved");if(mode==2)check(status.startsWith("REVISION_CONFLICT:"),"Change during reviewed confirmation refused atomically");if(mode==1){w.host.session.undo(3);check(w.host.session.document()==reviewed,"Actual complete reviewed correction Undo");w.host.session.redo(4);check(w.host.session.document()==cleared,"Actual complete reviewed correction Redo");w.host.session=observed;}w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Whole exact reviewed correction Save/snapshot/no normalization");check(question_text==expected_question,"Confirmation must describe both actual reviewed correction fields, not Inspector birth count");w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Point Edit Reset reviewed count/Cancel/Reset/concurrent; physical/clipboard/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_POINT_EDIT_RESET_REVIEW_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<3;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Point Edit Reset review cases failed: "+std::to_string(failures));std::cout<<"PASS Point Edit Reset review "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Point Edit Reset review "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
