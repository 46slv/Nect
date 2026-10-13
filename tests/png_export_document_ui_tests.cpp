#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QColorSpace>
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

Document fixture(){auto d=empty_document("png-document","comp","art");auto& board=d.compositions.front().artboards.front();board.width=8;board.height=6;Object shape;shape.id="shape";shape.name="Original";shape.source=default_primitive("source","nect.shape.rectangle");for(auto p:std::vector<std::pair<std::string,double>>{{"width",4},{"height",2},{"center_x",4},{"center_y",3}})shape.source->parameters.at(p.first).literal=p.second;auto fill=default_operation("fill","nect.paint.fill");fill.parameters.at("r").literal=1;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=0;shape.stack={fill};d.objects.emplace(shape.id,shape);d.compositions.front().roots={shape.id};return d;}
QImage pixels(bool white){QImage result(8,6,QImage::Format_ARGB32);result.fill(white?Qt::white:Qt::transparent);for(int y=2;y<4;++y)for(int x=2;x<6;++x)result.setPixelColor(x,y,Qt::red);return result;}
bool same_pixels(const QImage& actual,const QImage& expected){if(actual.size()!=expected.size())return false;for(int y=0;y<actual.height();++y)for(int x=0;x<actual.width();++x)if(actual.pixelColor(x,y)!=expected.pixelColor(x,y))return false;return true;}
void flow(const QString& root,int mode){
 auto initial=fixture();valid(initial);auto current=initial;current.objects.at("shape").name="Current exact name";valid(current);Session base(initial);base.apply({Rename{"shape","Current exact name"}},0);check(base.document()==current&&base.revision()==1&&base.history().current_id==1,"Independent full initial History BEFORE Window");auto caller=base;auto expected_doc=current;auto undo_doc=initial;
 if(mode<2){auto foreign=initial;foreign.id="foreign-png-document";foreign.objects.at("shape").stack.front().parameters.at("r").literal=0;foreign.objects.at("shape").stack.front().parameters.at("g").literal=1;undo_doc=foreign;expected_doc=foreign;expected_doc.objects.at("shape").name="Foreign exact name";valid(foreign);valid(expected_doc);caller=Session(foreign);caller.apply({Rename{"shape","Foreign exact name"}},0);check(caller.document()==expected_doc&&caller.revision()==base.revision(),"Independent full foreign native/coincident revision BEFORE Window");}
 if(mode==3){expected_doc.objects.at("shape").name="Externally committed";valid(expected_doc);caller.apply({Rename{"shape","Externally committed"}},1);check(caller.document()==expected_doc&&caller.revision()==2,"Independent external History BEFORE Window");}
 if(mode==4){auto preview=current;preview.objects.at("shape").source->parameters.at("width").literal=6;valid(preview);caller.begin_gesture(1);caller.update_gesture({Set{{"shape","","generator.width"},6}});check(caller.document()==current&&caller.preview_document()==preview&&caller.history()==base.history(),"Independent full active preview BEFORE Window");}
 if(mode!=4){auto undo=caller;undo.undo(undo.revision());check(undo.document()==(mode==3?current:undo_doc),"Independent complete Undo BEFORE Window");undo.redo(undo.revision());check(undo.document()==expected_doc&&undo.history()==caller.history(),"Independent complete Redo BEFORE Window");}
 const bool success=mode==5||mode==6,white=mode==6;const auto expected_pixels=pixels(white);const auto prefix=root+"/mode-"+QString::number(mode),output=prefix+"-output.png";const QByteArray sentinel="owned existing derivative bytes\n";if(mode<2){QFile f(output);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(sentinel)==sentinel.size(),"Owned immutable preexisting output");}write(prefix+"-expected.nect",expected_doc);
 QSettings settings(prefix+"-settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,950);w.show();events();w.canvas->fit_artboard();w.canvas->set_selection("shape");events();same(w.host.session,base);auto* action=w.findChild<QAction*>("export-png");check(action&&action->isEnabled(),"Original Export PNG action");const auto view=w.canvas->selections();const auto identity=w.host.session_id;QString actor_error;int settings_clicks=0,confirmation_clicks=0;QTimer::singleShot(0,&w,[&]{try{auto* dialog=w.findChild<QDialog*>("png-export-dialog");check(dialog&&dialog->isVisible(),"Original modal settings");auto* path=dialog->findChild<QLineEdit*>("png-path");auto* scale=dialog->findChild<QDoubleSpinBox*>("png-scale");auto* background=dialog->findChild<QComboBox*>("png-background");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(path&&scale&&background&&buttons,"Original named export controls");path->setText(output);scale->setValue(1);background->setCurrentIndex(white?1:0);check(dialog->grab().save(prefix+"-dialog.png"),"Actual product-styled PNG dialog");if(mode==7){dialog->reject();return;}if(mode!=1){w.host.session=caller;if(mode==2)w.host.session_id="foreign-png-host-session";same(w.host.session,caller);}if(mode<2)QTimer::singleShot(0,&w,[&]{try{auto* message=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());check(message&&message->windowTitle()=="Replace PNG?","Original owned replacement confirmation");if(mode==1){w.host.session=caller;same(w.host.session,caller);}auto* yes=message->button(QMessageBox::Yes);check(yes&&yes->isVisible(),"Original replacement Yes");++confirmation_clicks;yes->click();}catch(const std::exception& e){actor_error=e.what();if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();}});check(buttons->button(QDialogButtonBox::Save)->isEnabled(),"Original Export enabled");++settings_clicks;buttons->button(QDialogButtonBox::Save)->click();}catch(const std::exception& e){actor_error=e.what();if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();}});
 QSignalSpy invoked(action,&QAction::triggered);action->trigger();events();check(actor_error.isEmpty(),"Original actors completed without fixture error");const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(prefix+"-observed.nect",observed.document());check(w.grab().save(prefix+"-window.png"),"Actual PNG result BEFORE Save");const bool output_equal=success?(same_pixels(QImage(output),expected_pixels)):(mode<2?read(output)==sentinel:!QFile::exists(output));QJsonObject v{{"mode",mode},{"settings_clicks",settings_clicks},{"confirmation_clicks",confirmation_clicks},{"action_signal_count",invoked.count()},{"status",status},{"document_equal",observed.document()==expected_doc},{"preview_equal",observed.preview_document()==caller.preview_document()},{"history_equal",observed.history()==caller.history()},{"revision",qint64(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",qint64(observed.gesture_generation())},{"view_equal",w.canvas->selections()==view&&w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="art"},{"output_equal",output_equal},{"save_run",mode!=4}};QFile metadata(prefix+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable actual native/export readback BEFORE Save");metadata.write(QJsonDocument(v).toJson());metadata.close();same(observed,caller);check(invoked.count()==1&&settings_clicks==(mode==7?0:1)&&confirmation_clicks==(mode<2?1:0),"Original action/control counts");check(v["view_equal"].toBool(),"Exact independent selected object/output frame view");check(output_equal,"Export preserves refused existing destination or exact independent output pixels");check(success?status.startsWith("PNG exported: 8 × 6 px, sRGB, "):(mode==7?status.isEmpty():status==QString(mode<3?"SESSION_CONFLICT: Document changed while export settings were open":mode==3?"REVISION_CONFLICT: Refresh revision before exporting":"GESTURE_ACTIVE: Finish or cancel the current gesture before exporting")),"Exact export refusal/success");if(success)check(QImage(output).colorSpace()==QColorSpace(QColorSpace::SRgb),"Encoded PNG carries sRGB");if(mode!=4){w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,caller);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect")&&QFile::copy(prefix+"-saved.nect",prefix+"-snapshot.nect"),"Exact export source native Save/snapshot");}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" product style/PNG controls; physical/clipboard/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_PNG_EXPORT_DOCUMENT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("PNG export Document cases failed: "+std::to_string(failures));std::cout<<"PASS PNG export Document "<<checks<<" checks\n";QTest::qWait(300);return 0;}catch(const std::exception& e){std::cerr<<"FAIL PNG export Document "<<e.what()<<" after "<<checks<<" checks\n";QTest::qWait(300);return 1;}}
