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
 const Ref target{"first","","text.font_size"};auto initial=fixture();auto& raw=initial.objects.at("first").text->parameters.at("font_size");raw.binding.reset();raw.literal=24;initial.objects.at("second").text->parameters.at("font_size").expression=Expression{"32.5",1};if(mode==2){auto& second=initial.objects.at("second").text->parameters.at("font_size");second.expression.reset();second.literal=48;}valid(initial);
 Session base(initial),caller(initial);auto preview_doc=initial;
 if(mode==1){preview_doc.objects.at("shape").name="Independent foreign Text preview";valid(preview_doc);caller.begin_gesture(0);caller.update_gesture({Rename{"shape","Independent foreign Text preview"}});check(caller.document()==initial&&caller.preview_document()==preview_doc,"Independent foreign full preview BEFORE Window");auto denied=caller;QString code;try{denied.begin_gesture(0);}catch(const Error& e){code=QString::fromStdString(e.code);}check(code=="GESTURE_ACTIVE","Canonical begin atomically refuses foreign active gesture BEFORE Window");same(denied,caller);}
 const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",initial);write(prefix+"-preview-expected.nect",preview_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);if(mode==2)w.canvas->set_selections({{"first",""},{"second",""}});else w.canvas->set_selection("first");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock");dock->show();dock->raise();events();const auto selections=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
 QPointer<SemanticScrub> actor=dynamic_cast<SemanticScrub*>(control<QToolButton>(w,"semantic-scrub-text.font_size"));reveal(actor);QPointer<QLineEdit> input=actor->parentWidget()->findChild<QLineEdit*>();check(input&&input->text()==(mode==2?"":"24")&&!input->isReadOnly()&&QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object()==QJsonObject{{"object","first"},{"point",""},{"field","text.font_size"}},"Original PropertyInput and exact Text Ref");reveal(input);input->setFocus();events();
 if(mode!=1){input->selectAll();QTest::keyClicks(input,"40.5");events();check(input&&input->text()=="40.5"&&input->isModified(),"Original valid numeric input is uncommitted draft");same(w.host.session,base);}
 w.host.session=caller;same(w.host.session,caller);MouseCounts counts;counts.target=actor;actor->installEventFilter(&counts);check(actor->parentWidget()->grab().save(prefix+"-row.png"),"Actual pending/foreign row PNG");
 mouse(actor,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(actor,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);mouse(actor,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);
 const auto expected_status=mode!=1?QString("UNCOMMITTED_INPUT: Commit or cancel the numeric draft before scrubbing"):QString("GESTURE_ACTIVE: Finish or cancel the current gesture before another edit");const auto status=w.statusBar()->currentMessage();const bool full_equal=w.host.session.document()==caller.document()&&w.host.session.preview_document()==caller.preview_document()&&w.host.session.history()==caller.history()&&w.host.session.revision()==caller.revision()&&w.host.session.gesture_active()==caller.gesture_active()&&w.host.session.gesture_generation()==caller.gesture_generation();const bool view_equal=w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard;
 write(prefix+"-observed.nect",w.host.session.document());write(prefix+"-preview-observed.nect",w.host.session.preview_document());check(w.grab().save(prefix+"-window.png"),"Actual refusal Window PNG");
 QFile file(prefix+"-producer.json");check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable entry actor readback");QJsonObject state{{"mode",mode},{"mouse_press_count",counts.press},{"mouse_move_count",counts.move},{"mouse_release_count",counts.release},{"actor_alive",bool(actor)},{"input_alive",bool(input)},{"input_text",input?input->text():QString{}},{"input_modified",input&&input->isModified()},{"input_has_focus",input&&input->hasFocus()},{"status",status},{"expected_status",expected_status},{"full_session_equal",full_equal},{"document_equal",w.host.session.document()==caller.document()},{"preview_equal",w.host.session.preview_document()==caller.preview_document()},{"history_equal",w.host.session.history()==caller.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_active",w.host.session.gesture_active()},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"view_equal",view_equal},{"actual_save",mode!=1}};file.write(QJsonDocument(state).toJson());file.close();
 std::cout<<"ENTRY_RESULT mode="<<mode<<" full_session_equal="<<full_equal<<" status="<<status.toStdString()<<" input="<<(input?input->text().toStdString():"DELETED")<<" modified="<<(input&&input->isModified())<<"\n";
 check(counts.press==counts.move&&counts.move==counts.release&&counts.press==1,"Original scrub press/move/release one attempt");check(actor&&input&&full_equal&&view_equal,"Full caller and original widgets retained");check(input->text()==(mode!=1?"40.5":"24")&&input->isModified()==(mode!=1),"Numeric draft/committed presentation unchanged");check(status==expected_status,"Exact begin refusal owner message");same(w.host.session,caller);
 if(mode!=1){w.host.save(prefix+"-saved.nect");w.host.flush();QFile post(prefix+"-post-save-producer.json");check(post.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable postSave fullSession readback");QJsonObject post_state{{"document_equal",w.host.session.document()==caller.document()},{"preview_equal",w.host.session.preview_document()==caller.preview_document()},{"history_equal",w.host.session.history()==caller.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()}};post.write(QJsonDocument(post_state).toJson());post.close();same(w.host.session,caller);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Actual Save keeps24 notuncommitted40.5 AFTER actors");}else{check(!QFile::exists(prefix+"-saved.nect"),"Active caller Save NOT_RUN withoutcancel");std::cout<<"ACTIVE_GESTURE_SAVE NOT_RUN mode=1\n";}
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch");const auto requested=qEnvironmentVariable("NECT_TEXT_FONT_SIZE_SCRUB_ENTRY_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<3;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent scrub entry failures: "+std::to_string(failures));std::cout<<"PASS Text font size scrub entry "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text font size scrub entry "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
