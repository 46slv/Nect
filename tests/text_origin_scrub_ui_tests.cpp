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
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods=Qt::NoModifier){check(c,"Current scrub actor alive");const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,mods);QApplication::sendEvent(c,&e);events();}

void flow(const QString& root,int mode){
 const bool multi=mode%2==1;const std::string field=mode<2?"origin_x":"origin_y";const Ref target{"first","","text."+field},second{"second","","text."+field};const double delta=mode==0?0.1:mode==1?10:1;const auto mods=mode==0?Qt::ShiftModifier:mode==1?Qt::ControlModifier:Qt::NoModifier;
 auto initial=fixture();if(mode>=2){initial.objects.at("first").text->content="Retained Text-on-Path 日本語";initial.objects.at("first").text->layout="auto";initial.objects.at("first").text->path_attachment=TextPathAttachment{"shape","contour","distance",1000,2,false};}auto& first_raw=initial.objects.at("first").text->parameters.at(field);first_raw.binding.reset();first_raw.expression.reset();first_raw.literal=-30.25;auto& second_raw=initial.objects.at("second").text->parameters.at(field);second_raw.binding.reset();second_raw.expression.reset();second_raw.literal=77.25;valid(initial);check(property_unit(target)=="du","Existing typed distance unit BEFORE Window");
 auto goal_doc=initial;goal_doc.objects.at("first").text->parameters.at(field).literal=-30.25+delta;if(multi)goal_doc.objects.at("second").text->parameters.at(field).literal=77.25+delta;valid(goal_doc);
 std::vector<Command> commands{EditProperties{{target},-30.25+delta,false}};if(multi)commands.push_back(EditProperties{{second},77.25+delta,false});
 Session base(initial),preview(initial),expected(initial);preview.begin_gesture(0);preview.update_gesture(commands);check(preview.document()==initial&&preview.preview_document()==goal_doc,"Independent full manual preview BEFORE Window");expected=preview;expected.commit_gesture();check(expected.document()==goal_doc&&expected.revision()==1&&expected.history().states.size()==2,"Canonical oneHistory/full target BEFORE Window");
 auto cancel_doc=goal_doc;cancel_doc.objects.at("first").text->parameters.at(field).literal+=1;if(multi)cancel_doc.objects.at("second").text->parameters.at(field).literal+=1;valid(cancel_doc);std::vector<Command> cancel_commands{EditProperties{{target},-30.25+delta+1,false}};if(multi)cancel_commands.push_back(EditProperties{{second},77.25+delta+1,false});auto cancel_preview=expected;cancel_preview.begin_gesture(1);cancel_preview.update_gesture(cancel_commands);check(cancel_preview.preview_document()==cancel_doc,"Independent full cancel preview BEFORE Window");auto cancelled=cancel_preview;cancelled.cancel_gesture();auto undone=cancelled;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==goal_doc&&cancelled.history()==expected.history(),"Full cancel/UndoRedo oracle BEFORE Window");
 const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",goal_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);if(multi)w.canvas->set_selections({{"first",""},{"second",""}});else w.canvas->set_selection("first");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock");dock->show();dock->raise();events();const auto selections=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
 check(w.grab().save(prefix+"-before-window.png"),"Actual current Text origin Window");const auto name="semantic-scrub-text."+QString::fromStdString(field);QPointer<SemanticScrub> actor=dynamic_cast<SemanticScrub*>(w.findChild<QToolButton*>(name));check(actor,"REQ42 existing Text origin drag row missing on actual Window");reveal(actor);
 QJsonArray target_refs{QJsonObject{{"object","first"},{"point",""},{"field",QString::fromStdString(target.field)}}};if(multi)target_refs.append(QJsonObject{{"object","second"},{"point",""},{"field",QString::fromStdString(second.field)}});check(QJsonDocument::fromJson(actor->property("nect-targets").toByteArray()).array()==target_refs,"Exact frozen Single/Multi origin Ref set");QPointer<QLineEdit> input=actor->parentWidget()->findChild<QLineEdit*>();check(input&&input->text()==(multi?"":"-30.25")&&input->property("nect-mixed").toBool()==multi&&!input->isReadOnly(),"Original numeric/mixed presentation preserved");
 MouseCounts counts;counts.target=actor;actor->installEventFilter(&counts);check(actor->parentWidget()->grab().save(prefix+"-row.png"),"Actual new origin row PNG");mouse(actor,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(actor,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton,mods);same(w.host.session,preview);mouse(actor,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);same(w.host.session,expected);check(counts.press==counts.move&&counts.move==counts.release&&counts.press==1,"Original press/move/release once");check(w.statusBar()->currentMessage().isEmpty()&&w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Successful shared gesture/view/error state");
 write(prefix+"-observed.nect",w.host.session.document());check(w.grab().save(prefix+"-window.png"),"Actual postgesture Window PNG");
 QPointer<SemanticScrub> fresh=dynamic_cast<SemanticScrub*>(control<QToolButton>(w,name));reveal(fresh);mouse(fresh,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(fresh,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);same(w.host.session,cancel_preview);QTest::keyClick(fresh,Qt::Key_Escape);events();same(w.host.session,cancelled);w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.host.edited();events();
 QPointer<SemanticScrub> current=dynamic_cast<SemanticScrub*>(control<QToolButton>(w,name));reveal(current);QPointer<QLineEdit> pending=current->parentWidget()->findChild<QLineEdit*>();check(pending,"Current exact origin numeric field");pending->setFocus();pending->selectAll();QTest::keyClicks(pending,"40.5");events();check(pending&&pending->text()=="40.5"&&pending->isModified(),"Uncommitted valid origin draft AFTER normalgesture");same(w.host.session,redone);
 QFile producer(prefix+"-producer.json");check(producer.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable origin actor readback");QJsonObject state{{"mode",mode},{"field",QString::fromStdString(field)},{"multi",multi},{"targets",target_refs},{"delta",delta},{"modifier",mode==0?"Shift":mode==1?"Control":"None"},{"mouse_press_count",counts.press},{"mouse_move_count",counts.move},{"mouse_release_count",counts.release},{"owned_cancel",true},{"undo_redo_full_session",true},{"pending_input_alive",bool(pending)},{"pending_input_text",pending->text()},{"pending_input_modified",pending->isModified()},{"document_equal",w.host.session.document()==redone.document()},{"history_equal",w.host.session.history()==redone.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"view_equal",w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard}};producer.write(QJsonDocument(state).toJson());producer.close();
 w.host.save(prefix+"-saved.nect");w.host.flush();QFile post(prefix+"-post-save-producer.json");check(post.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable postSave origin fullSession readback");QJsonObject post_state{{"document_equal",w.host.session.document()==redone.document()},{"preview_equal",w.host.session.preview_document()==redone.preview_document()},{"history_equal",w.host.session.history()==redone.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()}};post.write(QJsonDocument(post_state).toJson());post.close();same(w.host.session,redone);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Actual Save committed origin, never implicit pending40.5 AFTERactors");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch");const auto requested=qEnvironmentVariable("NECT_TEXT_ORIGIN_SCRUB_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent origin drag failures: "+std::to_string(failures));std::cout<<"PASS Text origin scrub "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text origin scrub "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
