#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include "semantic_control.hpp"
#include <QApplication>
#include <QDateTime>
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
void png_file(const QString& path,const Raster& raster){QFile f(path);const auto& bytes=raster->bytes();check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size())==qint64(bytes.size()),"Owned immutable accepted PNG file written before Window");}
void flow(const QString& root,int mode){const bool success=mode>=3;const auto prefix=root+"/mode-"+QString::number(mode);auto initial=fixture();initial.id="image-check-context";for(auto& [id,asset]:initial.raster_assets){asset.mode="linked";asset.locator=(prefix+"-"+QString::fromStdString(id)+".png").toStdString();png_file(QString::fromStdString(asset.locator),asset.payload);}valid(initial);auto current=initial;current.objects.at("other").name="Current retained";valid(current);Session base(initial);base.apply({Rename{"other","Current retained"}},0);check(base.document()==current,"Whole linked Image/source/placements/native/History before Window");auto foreign_initial=initial;foreign_initial.id="foreign-image-check-context";foreign_initial.raster_assets.at("asset").payload=payload(8,2,96);foreign_initial.raster_assets.at("asset").locator=(prefix+"-foreign.png").toStdString();png_file(QString::fromStdString(foreign_initial.raster_assets.at("asset").locator),foreign_initial.raster_assets.at("asset").payload);valid(foreign_initial);auto foreign_current=foreign_initial;foreign_current.objects.at("other").name="Current retained";valid(foreign_current);Session foreign(foreign_initial);foreign.apply({Rename{"other","Current retained"}},0);check(foreign.document()==foreign_current&&foreign.revision()==base.revision()&&foreign.gesture_generation()==base.gesture_generation(),"Whole independent equal-counter foreign linked Image native before Window");auto role_initial=initial;role_initial.objects.at("image").image->asset="alternate";valid(role_initial);auto role_current=role_initial;role_current.objects.at("other").name="Current retained";valid(role_current);Session role(role_initial);role.apply({Rename{"other","Current retained"}},0);check(role.document()==role_current&&role.revision()==base.revision(),"Whole same-Doc changed source association before Window");auto concurrent=base;concurrent.apply({Rename{"other","Concurrent retained"}},1);auto concurrent_doc=current;concurrent_doc.objects.at("other").name="Concurrent retained";valid(concurrent_doc);check(concurrent.document()==concurrent_doc,"Whole unrelated same-Doc caller before Window");auto expected=mode==0?foreign:mode==2?role:mode==4?concurrent:base;write(prefix+"-expected.nect",expected.document());write(prefix+"-expected-preview.nect",expected.preview_document());
QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);settings.setFallbacksEnabled(false);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings,FolderLibrary::PersistOverride{},FolderLibrary::ReadbackOverride{},prefix+"-assets"),&settings);w.host.session=base;w.host.edited();w.resize(1400,1150);w.show();events();w.canvas->set_active_artboard("comp","art",false);w.canvas->set_selection("image","");events();QPointer<QPushButton> actor=control<QPushButton>(w,"image-check-link");reveal(actor);const auto original_host=w.host.session_id;const auto zoom=w.canvas->zoom();check(w.grab().save(prefix+"-before.png"),"Original retained Check link source observation");if(mode==0)w.host.session=foreign;if(mode==1)w.host.session_id+="-changed";if(mode==2)w.host.session=role;if(mode==4)w.host.session=concurrent;const auto old_asset=w.host.asset_status("asset"),old_alternate=w.host.asset_status("alternate");check(old_asset["state"]=="unchecked"&&old_alternate["state"]=="unchecked"&&old_asset["checked_at"].isNull()&&old_alternate["checked_at"].isNull(),"Complete caller observations initially unobserved before original producer");actor->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();const auto asset=w.host.asset_status("asset"),alternate=w.host.asset_status("alternate");const auto label=control<QLabel>(w,"image-link-status")->text();write(prefix+"-observed.nect",observed.document());write(prefix+"-observed-preview.nect",observed.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual Check link observation before Save");const bool observation_equal=success?asset["state"]=="current"&&asset["asset"]=="asset"&&asset["display"]=="accepted_cached_pixels"&&asset["accepted_sha256"].toString()==QString::fromStdString(current.raster_assets.at("asset").payload->sha256())&&asset["observed_sha256"]==asset["accepted_sha256"]&&asset.size()==6&&QDateTime::fromString(asset["checked_at"].toString(),Qt::ISODateWithMs).isValid()&&alternate==old_alternate:asset==old_asset&&alternate==old_alternate;
QJsonObject v{{"mode",mode},{"status",status},{"asset_before",old_asset},{"alternate_before",old_alternate},{"asset_observed",asset},{"alternate_observed",alternate},{"label",label},{"observation_equal",observation_equal},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"host_equal",mode==1?w.host.session_id!=original_host:w.host.session_id==original_host},{"view_equal",w.canvas->selected_object=="image"&&w.canvas->selected_point.empty()&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"&&w.canvas->zoom()==zoom},{"save_intent",true}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable full native/History/source observation readback");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,expected);check(v["view_equal"].toBool()&&v["host_equal"].toBool(),"Exact caller view/zoom/Host preserved");check(observation_equal,"Original Check link bound to intended Document/image asset role and exact observations");if(mode<2)check(status.startsWith("SESSION_CONFLICT:"),"Foreign Doc/Host observation refused explicitly");if(mode==2)check(status.startsWith("PROPERTY_CONFLICT:"),"Changed Image source association refused explicitly");if(success)check(label.startsWith("current"),"Original current Inspector displays explicit current owned-source observation");w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Exact whole linked native payload/locator Save snapshot/no new authored History/no normalization");w.close();events();}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Image Check link; owned PNG only; physical/clipboard/fonts/AE/pixel output NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_IMAGE_CHECK_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<5;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Image Check context cases failed: "+std::to_string(failures));std::cout<<"PASS Image Check context "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL Image Check context "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
