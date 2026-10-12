#include "window.hpp"
#include "visual_style.hpp"
#include "semantic_control.hpp"
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
#include <QTextDocument>
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
const std::string shared_content="  Literal 日本語\n\tCafé e\xcc\x81 🐈\n\n";
const std::string changed_content="\n  Replacement 日本語\n\tCafé e\xcc\x81 🐈  \n";
const std::vector<Id> pair={"first","second"};

Document fixture(bool mixed=false,bool driven=false){
    auto document=empty_document("literal-content-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::text;
    source.text=default_text("source-text","Unrelated source 日本語\n");
    source.text->parameters.at("font_size").literal=24;
    Object first=source;first.id="first";first.name="Same name";
    first.text=default_text("first-text",shared_content);
    first.text->family="Arial";first.text->locale="en-US";first.text->layout="frame";
    first.text->direction="vertical";first.text->alignment="center";
    first.text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    first.text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    first.text->direction_driver=TextDirectionDriver{{"source","","text.direction"}};
    first.text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    first.text->alignment_driver=TextAlignmentDriver{{"source","","text.alignment"}};
    first.text->weight_expression=Expression{"500",1};
    first.text->italic_driver=TextItalicDriver{Expression{"true",1}};
    first.text->parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};
    first.text->parameters.at("tracking").expression=Expression{"2",1};
    first.text->parameters.at("frame_width").literal=271.25;
    first.text->parameters.at("frame_height").literal=136.5;
    first.text->font_features={{"kern",1,"whole_text"},{"liga",0,"whole_text"}};
    first.text->additional_axis_values={{"wdth",87.1234567890123}};
    Object second=source;second.id="second";second.name="Same name";
    second.text=default_text("second-text",mixed?"":shared_content);
    second.text->family="Consolas";second.text->locale="ja-JP";second.text->alignment="end";
    second.text->weight=650;second.text->weight_driver=TextWeightDriver{{"source","","text.weight"},20};
    second.text->italic=true;second.text->italic_driver=TextItalicDriver{Ref{"source","","text.italic"}};
    second.text->parameters.at("font_size").literal=32;
    second.text->parameters.at("frame_width").literal=481.75;
    second.text->parameters.at("frame_height").literal=223.5;
    second.text->font_features={{"kern",0,"whole_text"}};
    second.text->additional_axis_values={{"opsz",21.75}};
    if(driven)second.text->content_driver=TextContentDriver{{"source","","text.content"}};
    Object path;path.id="shape";path.name="Unrelated path";path.kind=Kind::path;
    Contour contour;contour.id="contour";Point begin;begin.id="begin";begin.x.literal=10;begin.y.literal=100;
    Point end;end.id="end";end.x.literal=2010;end.y.literal=100;contour.points={begin,end};path.contours={contour};
    Object follower=source;follower.id="follower";follower.name="Unrelated linked Text";
    follower.text=default_text("follower-text","Retained unrelated literal\r\n");
    follower.text->parameters.at("font_size").literal=24;
    follower.text->content_driver=TextContentDriver{{"source","","text.content"}};
    document.objects.emplace(source.id,source);document.objects.emplace(first.id,first);
    document.objects.emplace(second.id,second);document.objects.emplace(path.id,path);
    document.objects.emplace(follower.id,follower);
    document.compositions.front().roots={"source","first","second","shape","follower"};
    return document;
}




struct MouseCounts:QObject{QPointer<QWidget> target;int press=0,move=0,release=0;bool eventFilter(QObject* watched,QEvent* e)override{if(watched==target){if(e->type()==QEvent::MouseButtonPress)++press;if(e->type()==QEvent::MouseMove)++move;if(e->type()==QEvent::MouseButtonRelease)++release;}return false;}};
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons){check(c,"Current scrub actor alive");const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,Qt::NoModifier);QApplication::sendEvent(c,&e);events();}
void flow(const QString& root,int mode){
 const Ref target{"first","","text.font_size"};auto initial=fixture();auto& raw=initial.objects.at("first").text->parameters.at("font_size");raw.binding.reset();raw.literal=24;initial.objects.at("second").text->parameters.at("font_size").expression=Expression{"32.5",1};valid(initial);
 auto caller_doc=initial;if(mode==0)caller_doc.objects.at("first").text->id="replacement-text";if(mode==1)caller_doc.objects.at("first").text->parameters.at("font_size").literal=36;valid(caller_doc);
 auto goal_doc=caller_doc;if(mode==2)goal_doc.objects.at("first").text->parameters.at("font_size").literal=25;valid(goal_doc);
 Session base(initial),caller(caller_doc),expected(caller_doc),preview(caller_doc);check(property_unit(target)=="du","Independent fontSize unit; bounded ordinary drag baseline step1 BEFORE Window");
 if(mode==2){preview.begin_gesture(0);preview.update_gesture({EditProperties{{target},25,false}});check(preview.document()==caller_doc&&preview.preview_document()==goal_doc,"Independent manual full preview BEFORE Window");expected=preview;expected.commit_gesture();check(expected.document()==goal_doc&&expected.revision()==1&&expected.history().states.size()==2,"Independent full goal/oneHistory BEFORE Window");}
 auto cancel_preview=expected,cancelled=expected;if(mode==2){cancel_preview.begin_gesture(cancel_preview.revision());cancel_preview.update_gesture({EditProperties{{target},26,false}});cancelled=cancel_preview;cancelled.cancel_gesture();check(cancelled.document()==goal_doc&&cancelled.history()==expected.history()&&cancelled.revision()==expected.revision(),"Independent full cancel snapshot BEFORE Window");}
 auto undone=cancelled,redone=cancelled;if(mode==2){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());check(undone.document()==caller_doc&&redone.document()==goal_doc,"Independent full Undo/Redo BEFORE Window");}
 const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-initial.nect",initial);write(prefix+"-caller.nect",caller_doc);write(prefix+"-expected.nect",goal_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("first");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock");dock->show();dock->raise();events();
 const auto selections=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
 check(w.grab().save(prefix+"-before-window.png"),"Actual styled Text numeric row BEFORE scrub query");std::cout<<"SCRUB_DISCOVERY mode="<<mode<<" input_count="<<w.findChildren<QLineEdit*>().size()<<" scrub_present="<<(w.findChild<QToolButton*>("semantic-scrub-text.font_size")!=nullptr)<<"\n";QPointer<SemanticScrub> actor=dynamic_cast<SemanticScrub*>(w.findChild<QToolButton*>("semantic-scrub-text.font_size"));check(actor,"REQ42 Text fontSize drag control missing on actual Window");reveal(actor);check(actor->property("nect-reference").toByteArray()==QByteArray("{\"object\":\"first\",\"point\":\"\",\"field\":\"text.font_size\"}")||QJsonDocument::fromJson(actor->property("nect-reference").toByteArray()).object()==QJsonObject{{"object","first"},{"point",""},{"field","text.font_size"}},"Exact scrub canonical Ref");
 MouseCounts counts;counts.target=actor;actor->installEventFilter(&counts);check(actor->parentWidget()->grab().save(prefix+"-row.png"),"Actual bound styled row PNG");
 w.host.session=caller;same(w.host.session,caller);
 mouse(actor,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);
 if(mode==2){auto begun=caller;begun.begin_gesture(0);same(w.host.session,begun);check(w.host.session.gesture_active()&&w.host.session.document()==caller_doc&&w.host.session.preview_document()==caller_doc,"Original scrub starts own empty preview");}
 mouse(actor,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);
 if(mode==2)same(w.host.session,preview);
 mouse(actor,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);
 const auto status=w.statusBar()->currentMessage();const auto expected_error=mode==0?QString("TEXT_EDIT_CONFLICT: The retained Text property source was replaced; reopen this Inspector"):mode==1?QString("PROPERTY_CONFLICT: The retained Text property changed; reopen this Inspector"):QString{};
 const bool full_equal=w.host.session.document()==expected.document()&&w.host.session.preview_document()==expected.preview_document()&&w.host.session.history()==expected.history()&&w.host.session.revision()==expected.revision()&&w.host.session.gesture_generation()==expected.gesture_generation()&&w.host.session.gesture_active()==expected.gesture_active();
 const bool view_equal=w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard;
 write(prefix+"-observed.nect",w.host.session.document());check(w.grab().save(prefix+"-window.png"),"Actual postactor Window PNG");
 QFile json(prefix+"-producer.json");check(json.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable actor readback");
 QJsonObject state{{"mode",mode},{"mouse_press_count",counts.press},{"mouse_move_count",counts.move},{"mouse_release_count",counts.release},{"original_alive_after",bool(actor)},{"document_equal",w.host.session.document()==expected.document()},{"history_equal",w.host.session.history()==expected.history()},{"preview_equal",w.host.session.preview_document()==expected.preview_document()},{"full_session_equal",full_equal},{"view_equal",view_equal},{"status",status},{"expected_status",expected_error},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()}};json.write(QJsonDocument(state).toJson());json.close();
 std::cout<<"SCRUB_RESULT mode="<<mode<<" full_session_equal="<<full_equal<<" status="<<status.toStdString()<<" original_alive="<<bool(actor)<<" mouse="<<counts.press<<"/"<<counts.move<<"/"<<counts.release<<"\n";
 check(counts.press==1&&counts.move==1&&counts.release==1,"Original scrub press/move/release exactly once");check(full_equal,"Full caller Session or independent normal gesture goal");check(view_equal&&status==expected_error,"Exact caller view and refusal/status");
 same(w.host.session,expected);
 if(mode==2){
  QPointer<SemanticScrub> fresh=dynamic_cast<SemanticScrub*>(control<QToolButton>(w,"semantic-scrub-text.font_size"));reveal(fresh);mouse(fresh,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(fresh,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);same(w.host.session,cancel_preview);
  QTest::keyClick(fresh,Qt::Key_Escape);events();same(w.host.session,cancelled);
  check(w.host.session.document()==goal_doc&&w.host.session.history()==expected.history()&&w.host.session.revision()==expected.revision(),"Escape cancels only owned preview with full native/History retained");
  QFile cancel_json(prefix+"-cancel-producer.json");check(cancel_json.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable full cancel readback");QJsonObject cancelled_state{{"document_equal",w.host.session.document()==cancelled.document()},{"preview_equal",w.host.session.preview_document()==cancelled.preview_document()},{"history_equal",w.host.session.history()==cancelled.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()},{"view_equal",w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard},{"escape_dispatch",1}};cancel_json.write(QJsonDocument(cancelled_state).toJson());cancel_json.close();
  w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}
 w.host.save(prefix+"-saved.nect");w.host.flush();check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Actual Save exact full native AFTER actors");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch");const auto requested=qEnvironmentVariable("NECT_TEXT_SINGLE_SCALAR_SCRUB_TARGET_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<3;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent original scrub failures: "+std::to_string(failures));std::cout<<"PASS Text single scalar scrub target "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text single scalar scrub target "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
