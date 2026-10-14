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

Raster payload(unsigned width,unsigned height,unsigned char red){RasterPixels p{width,height,{}};for(unsigned i=0;i<width*height;++i)p.rgba.insert(p.rgba.end(),{red,64,128,255});return make_raster(encode_raster_png(p));}
Document fixture(){auto d=empty_document("image-fit-context","comp","art");auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;d.raster_assets.emplace("asset",RasterAsset{"asset","Shared image","embedded","",payload(4,2,192)});d.raster_assets.emplace("alternate",RasterAsset{"alternate","Alternate image","embedded","",payload(4,6,48)});for(const auto& id:{Id{"image"},Id{"copy"}}){Object object;object.id=id;object.name=id;object.kind=Kind::image;object.image=ImageSource{"asset",Scalar{id=="image"?120.0:80.0},Scalar{id=="image"?60.0:50.0}};object.anchor={Scalar{0},Scalar{0}};object.transform[4].literal=id=="image"?60:400;object.transform[5].literal=id=="image"?80:300;object.compositing.opacity.literal=id=="image"?0.375:0.625;d.objects.emplace(id,object);d.compositions.front().roots.push_back(id);}Object other;other.id="other";other.name="Other artwork";other.source=default_primitive("other-source","nect.shape.rectangle");other.source->parameters.at("center_x").literal=520;other.source->parameters.at("center_y").literal=440;other.source->parameters.at("width").literal=30;other.source->parameters.at("height").literal=24;other.stack={default_operation("other-fill","nect.paint.fill")};d.objects.emplace("other",other);d.compositions.front().roots.push_back("other");return d;}
void flow(const QString& root,int mode){const bool success=mode==4;auto initial=fixture();valid(initial);auto current=initial;current.objects.at("other").name="Current retained";valid(current);Session base(initial);base.apply({Rename{"other","Current retained"}},0);check(base.document()==current&&base.revision()==1,"Full independent Image placements/shared assets/native/History before Window");auto foreign_initial=initial;foreign_initial.id="foreign-image-fit-context";foreign_initial.raster_assets.at("asset").payload=payload(8,2,96);foreign_initial.objects.at("other").source->parameters.at("height").literal=90;valid(foreign_initial);auto foreign_current=foreign_initial;foreign_current.objects.at("other").name="Current retained";valid(foreign_current);Session foreign(foreign_initial);foreign.apply({Rename{"other","Current retained"}},0);check(foreign.document()==foreign_current&&foreign.revision()==base.revision()&&foreign.gesture_generation()==base.gesture_generation(),"Whole independent foreign Image Document equal IDs/counters before Window");auto role_initial=initial;role_initial.objects.at("image").image->asset="alternate";valid(role_initial);auto role_current=role_initial;role_current.objects.at("other").name="Current retained";valid(role_current);Session role(role_initial);role.apply({Rename{"other","Current retained"}},0);check(role.document()==role_current&&role.revision()==base.revision()&&role.gesture_generation()==base.gesture_generation(),"Full independent same-Doc changed image-to-asset association before Window");auto concurrent=base;concurrent.apply({Rename{"other","Concurrent retained"}},1);auto concurrent_doc=current;concurrent_doc.objects.at("other").name="Concurrent retained";valid(concurrent_doc);check(concurrent.document()==concurrent_doc,"Whole concurrent caller before Window");auto busy=base;busy.begin_gesture(1);busy.update_gesture({Set{{"image","","image.width"},121}});auto preview=current;preview.objects.at("image").image->width.literal=121;valid(preview);check(busy.document()==current&&busy.preview_document()==preview,"Whole active Image dimension caller preview before Window");auto transformed=current;transformed.objects.at("image").image->width.literal=640;transformed.objects.at("image").image->height.literal=320;valid(transformed);auto committed=base;committed.apply({Set{{"image","","image.width"},640},Set{{"image","","image.height"},320}},1);check(committed.document()==transformed,"Hand-computed640x320 native Fit only selected placement delta matches typed Session before Window");auto history=committed;history.undo(2);check(history.document()==current,"Whole independent Fit Undo before Window");history.redo(3);check(history.document()==transformed&&history.history()==committed.history(),"One History/full independent Fit Redo before Window");auto expected=mode==0?foreign:mode==2?concurrent:mode==3?busy:mode==5?role:success?committed:base;const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},prefix+"-assets"),&settings);w.host.session=base;w.host.edited();w.resize(1400,1150);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("image","");events();QPointer<QPushButton> actor=control<QPushButton>(w,"image-fit-width");reveal(actor);const auto original_host=w.host.session_id;const auto zoom=w.canvas->zoom();check(w.grab().save(prefix+"-before.png"),"Original retained Image Fit width to Artboard producer");if(mode==0)w.host.session=foreign;else if(mode==1)w.host.session_id+="-changed";else if(mode==2)w.host.session=concurrent;else if(mode==3)w.host.session=busy;else if(mode==5)w.host.session=role;actor->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual original Image Fit result before Save");QJsonObject v{{"mode",mode},{"status",status},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"caller_identity_preserved",mode!=3||observed.gesture_identity()==busy.gesture_identity()},{"host_equal",mode==1?w.host.session_id!=original_host:w.host.session_id==original_host},{"view_equal",w.canvas->selected_object=="image"&&w.canvas->selected_point.empty()&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"&&w.canvas->zoom()==zoom},{"save_intent",mode!=3}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable full Image/assets/native/History/view before Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["host_equal"].toBool()&&v["caller_identity_preserved"].toBool(),"Caller view/zoom/gesture identity preserved");if(mode<2)check(status.startsWith("SESSION_CONFLICT:"),"Foreign Image Doc/Host refused explicitly");if(mode==2)check(status.startsWith("REVISION_CONFLICT:"),"Changed caller revision refused explicitly");if(mode==3)check(status.startsWith("REVISION_CONFLICT:"),"Changed gesture generation refusal preserves active dimension preview/full caller");if(mode==5)check(status.startsWith("PROPERTY_CONFLICT:"),"Changed image asset role refuses old ratio explicitly");if(success){w.host.session.undo(2);check(w.host.session.document()==current,"Actual whole Image Fit Undo");w.host.session.redo(3);check(w.host.session.document()==transformed,"Actual whole Image Fit Redo");w.host.session=observed;}if(mode!=3){w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Full exact Image/native payload Save/snapshot, no normalization");}w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Image Fit width to Artboard; physical/clipboard/fonts/AE/pixel output NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_IMAGE_FIT_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<6;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Image Fit context cases failed: "+std::to_string(failures));std::cout<<"PASS Image Fit context "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Image Fit context "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
