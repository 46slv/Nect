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



QJsonArray view_json(const std::vector<Canvas::Selection>& selection){QJsonArray result;for(const auto& v:selection)result.append(QJsonObject{{"object",QString::fromStdString(v.object)},{"point",QString::fromStdString(v.point)}});return result;}
void view(Window& w,const std::vector<Canvas::Selection>& selection,const Id& board){check(w.canvas->selections()==selection&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()==board,"Exact independent manualCanvas selectedIDs/Composition/Artboard view");}


class ArrowEvents final:public QObject{public:QPointer<QWidget> target;int presses=0,releases=0,mouse=0;bool eventFilter(QObject* watched,QEvent* event)override{if(watched==target){if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonRelease)++mouse;if((event->type()==QEvent::KeyPress||event->type()==QEvent::KeyRelease)&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Space){if(event->type()==QEvent::KeyPress)++presses;else ++releases;}}return false;}};

void flow(const QString& root,int mode){
 const QString draft=mode==0?"+=2":"26.125",expected_editor=mode==0?"26":"26.125";const double own=mode==0?26:26.125;auto initial=fixture(true);auto& first=initial.objects.at("first").text->parameters.at("font_size");first.literal=24;first.binding.reset();first.expression.reset();auto& second=initial.objects.at("second").text->parameters.at("font_size");second.literal=32.5;second.binding.reset();second.expression=Expression{"32.5",1};valid(initial);Session base(initial);auto prepared_doc=initial;prepared_doc.objects.at("first").text->parameters.at("font_size").literal=own;valid(prepared_doc);auto prepared=base;prepared.apply({EditProperties{{{"first","","text.font_size"}},mode==0?2:own,mode==0}},0);check(prepared.document()==prepared_doc&&prepared.revision()==1&&prepared.history().states.size()==base.history().states.size()+1,"Independent fullmanual ownscalar/canonicalEditProperties oneHistory BEFOREWindow");auto undone=prepared;undone.undo(1);auto undo_history=prepared.history();undo_history.current_id=base.history().current_id;check(undone.document()==initial&&undone.history()==undo_history&&undone.revision()==2&&undone.preview_document()==initial&&!undone.gesture_active(),"Independent fullUndo caller/History/preview BEFOREWindow");auto redone=undone;redone.redo(2);check(redone.document()==prepared_doc&&redone.history()==prepared.history()&&redone.revision()==3&&redone.preview_document()==prepared_doc&&!redone.gesture_active(),"Independent fullRedo ownscalar/History/preview BEFOREWindow");
 const std::vector<Canvas::Selection> targets{{"first",{}}};const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",prepared_doc);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections(targets);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original Properties dock");dock->show();dock->raise();events();QPointer<QLineEdit> input;auto find_input=[&]{QPointer<QLineEdit> value;for(auto* c:w.findChildren<QLineEdit*>()){const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(c->isVisible()&&ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")value=c;}return value;};input=find_input();check(input,"Exactoriginal fontSize field");QPointer<QPushButton> fx=input->parentWidget()->findChild<QPushButton*>("property-expression");check(fx&&!w.findChild<QWidget*>("nect-expression-panel"),"Exactparentrow originalfx/noinitialpanel");reveal(fx);reveal(input);input->setFocus(Qt::OtherFocusReason);events();input->selectAll();QTest::keyClicks(input,draft);check(input->text()==draft&&input->isModified(),"OwnedQt validnumericdraft pending");same(w.host.session,base);view(w,targets,"artboard");QPointer<QLineEdit> old_input=input;QPointer<QPushButton> old_fx=fx;fx->setFocus(Qt::OtherFocusReason);events();const auto observed_prepared=w.host.session;write(path+"-prepared-observed.nect",observed_prepared.document());check(w.grab().save(path+"-prepared-window.png"),"StyledWindow afterownnumeric focusout");const bool old_input_survives=bool(old_input),old_fx_survives=bool(old_fx);same(w.host.session,prepared);view(w,targets,"artboard");input=find_input();check(input&&!input->isModified(),"Exactcurrent originalfield afterowncommit/rebuild");fx=input->parentWidget()->findChild<QPushButton*>("property-expression");check(fx,"Exactcurrent originalrow fx afterrebuild");reveal(fx);fx->setFocus(Qt::OtherFocusReason);events();check(fx&&QApplication::focusWidget()==fx,"Currentoriginal fx ownskeyboardfocus");QSignalSpy clicked(fx,&QPushButton::clicked);ArrowEvents actor;actor.target=fx;qApp->installEventFilter(&actor);QTest::keyClick(fx,Qt::Key_Space);qApp->removeEventFilter(&actor);events();QPointer<QWidget> panel=w.findChild<QWidget*>("nect-expression-panel");check(panel&&panel->isVisible(),"Currentoriginal fx opensinline expressionpanel");QPointer<QPlainTextEdit> editor=panel->findChild<QPlainTextEdit*>();check(editor&&editor->isVisible(),"Original inline expressioneditor");const auto ref=QJsonDocument::fromJson(editor->property("nect-reference").toByteArray()).object();check(ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size","Editor remainsbound exactcurrent firstfontSize Ref");const auto actual_editor=editor->toPlainText();const auto opened=w.host.session;const bool open_neutral=opened.document()==prepared_doc&&opened.history()==prepared.history()&&opened.preview_document()==prepared_doc&&opened.revision()==1&&!opened.gesture_active();QPointer<QPushButton> cancel;for(auto* c:panel->findChildren<QPushButton*>())if(c->text()=="Cancel")cancel=c;check(cancel,"Originalinline Cancel control");check(panel->grab().save(path+"-panel.png"),"Styledactual currentexpressionpanel BEFORECancel");QSignalSpy cancelled(cancel,&QPushButton::clicked);cancel->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"StyledWindow afteroriginaldraft Cancel");QJsonObject state{{"draft",draft},{"expected_editor",expected_editor},{"observed_editor",actual_editor},{"key_press_count",actor.presses},{"key_release_count",actor.releases},{"mouse_event_count",actor.mouse},{"fx_signal_count",clicked.count()},{"cancel_signal_count",cancelled.count()},{"old_input_survives_focus",old_input_survives},{"old_fx_survives_focus",old_fx_survives},{"open_neutral",open_neutral},{"panel_visible_after_cancel",panel&&panel->isVisible()},{"status",status},{"document_equal",observed.document()==prepared_doc},{"history_equal",observed.history()==prepared.history()},{"preview_equal",observed.preview_document()==prepared_doc},{"view_equal",w.canvas->selections()==targets&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="artboard"}};QFile meta(path+"-producer.json");check(meta.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable originalnumericfocus/currentfx/draft/Cancel readback");meta.write(QJsonDocument(state).toJson());meta.close();std::cout<<"TEXT_SINGLE_SCALAR_FX_KEY_FOCUS mode="<<mode<<" editor="<<actual_editor.toStdString()<<" openneutral="<<open_neutral<<" doc="<<state["document_equal"].toBool()<<"\n";check(actor.presses==actor.releases&&actor.presses==1&&actor.mouse==0&&clicked.count()==1&&cancelled.count()==1,"Exactcurrent originalfx Spacepress/release/clicked1/mouse0/Cancel1");check(actual_editor==expected_editor&&open_neutral&&!(panel&&panel->isVisible()),"Draft initializedfrommanualcurrentownvalue andopening/Cancel authoredneutral");same(w.host.session,prepared);view(w,targets,"artboard");check(status.isEmpty(),"Successful numericfocus/fx/Cancel hasnoerror");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,prepared);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actualfullownscalar/draftCancel nativeSave AFTERactors");w.host.session.undo(1);same(w.host.session,undone);w.host.session.redo(2);same(w.host.session,redone);w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" actualproductstyle/Qt originalnumericfocus/currentfx Space/draftCancel; clipboard/physical/geometry/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_SINGLE_SCALAR_FX_KEY_FOCUS_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<2;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text single scalar fx key focus cases failed: "+std::to_string(failures));std::cout<<"PASS Text single scalar fx key focus "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text single scalar fx key focus "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
