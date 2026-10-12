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



QJsonArray view_json(const std::vector<Canvas::Selection>& selection){QJsonArray result;for(const auto& v:selection)result.append(QJsonObject{{"object",QString::fromStdString(v.object)},{"point",QString::fromStdString(v.point)}});return result;}
void view(Window& w,const std::vector<Canvas::Selection>& selection,const Id& board){check(w.canvas->selections()==selection&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()==board,"Exact independent manualCanvas selectedIDs/Composition/Artboard view");}


class ArrowEvents final:public QObject{public:QPointer<QWidget> target;int presses=0,releases=0,mouse=0;bool eventFilter(QObject* watched,QEvent* event)override{if(watched==target){if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonRelease)++mouse;if((event->type()==QEvent::KeyPress||event->type()==QEvent::KeyRelease)){if(event->type()==QEvent::KeyPress)++presses;else ++releases;}}return false;}};



void flow(const QString& root,int mode){
 const QString source="10+\n16";auto initial=fixture(true);auto& first=initial.objects.at("first").text->parameters.at("font_size");first.literal=24;first.binding.reset();first.expression.reset();auto& second=initial.objects.at("second").text->parameters.at("font_size");second.literal=32.5;second.binding.reset();second.expression=Expression{"32.5",1};valid(initial);Session base(initial);auto denied=base;bool syntax=false;try{denied.apply({SetExpression{{{"first","","text.font_size"}},{"10+",1},false}},0);}catch(const Error& e){syntax=e.code=="EXPRESSION_SYNTAX"&&std::string(e.what())=="Expected an expression value";}check(syntax,"Independent failedprefix exactCore syntax neutralrefusal BEFOREWindow");same(denied,base);
 auto caller_doc=initial,preview_doc=initial;QString error;Session caller=base;const bool active=mode==0;
 if(mode<2){preview_doc.objects.at("shape").name="Independent external expression preview";valid(preview_doc);caller.begin_gesture(0);caller.update_gesture({Rename{"shape","Independent external expression preview"}});check(caller.document()==initial&&caller.preview_document()==preview_doc&&caller.history()==base.history()&&caller.revision()==0&&caller.gesture_active()&&caller.gesture_generation()==1,"Independent fullactive externalpreview/history BEFOREWindow");if(mode==1){caller.cancel_gesture();preview_doc=initial;}error="The property edit context changed; edit it again";}
 if(mode==2){caller_doc.objects.at("source").text->parameters.at("font_size").literal=36;preview_doc=caller_doc;valid(caller_doc);caller.apply({EditProperties{{{"source","","text.font_size"}},36,false}},0);check(caller.document()==caller_doc&&caller.preview_document()==caller_doc&&caller.revision()==1&&caller.history().states.size()==base.history().states.size()+1&&!caller.gesture_active()&&caller.gesture_generation()==0,"Independent manualupstream canonicalcommitted fullcaller BEFOREWindow");error="The property edit context changed; edit it again";}
 check(caller.document()==caller_doc&&caller.preview_document()==preview_doc&&caller.gesture_active()==active&&caller.revision()==(mode==2?1:0)&&caller.gesture_generation()==(mode<2?1:0),"Independent active/cancelled/committed fullcaller BEFOREWindow");const bool success=false;auto intended=caller_doc;auto goal=caller;
 const std::vector<Canvas::Selection> targets{{"first",{}}};const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",intended);write(path+"-preview-expected.nect",preview_doc);QSettings settings(root+"/settings-"+QString::number(mode)+".ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections(targets);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original Properties dock");dock->show();dock->raise();events();QPointer<QLineEdit> input;for(auto* c:w.findChildren<QLineEdit*>()){const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(c->isVisible()&&ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")input=c;}check(input&&!input->isModified(),"Exact originalneutralfontSize");reveal(input);input->setFocus(Qt::OtherFocusReason);events();input->selectAll();ArrowEvents actor;actor.target=input;qApp->installEventFilter(&actor);QTest::keyClicks(input,"=10+");QTest::keyClick(input,Qt::Key_Return);qApp->removeEventFilter(&actor);QTest::qWait(300);events();check(input&&input->text()=="24"&&!input->isModified(),"Failedprefix restoresoriginalnumeric24 withauthoredunchanged");QPointer<QWidget> panel=w.findChild<QWidget*>("nect-expression-panel");check(panel&&panel->isVisible(),"Original inlinepanel");QPointer<QPlainTextEdit> editor=panel->findChild<QPlainTextEdit*>();QPointer<QLabel> result=panel->findChild<QLabel*>("nect-expression-result");QPointer<QPushButton> apply;for(auto* c:panel->findChildren<QPushButton*>())if(c->text()=="Apply")apply=c;check(editor&&result&&apply&&editor->toPlainText()=="10+"&&result->text()=="Expected an expression value\nCommitted result is unchanged.","Originalfailedprefix draft/exactsyntax preview/Apply");
 const auto editor_ref=QJsonDocument::fromJson(editor->property("nect-reference").toByteArray()).object();check(editor_ref["object"].toString()=="first"&&editor_ref["point"].toString().isEmpty()&&editor_ref["field"].toString()=="text.font_size","Originaleditor exact Ref");editor->setFocus(Qt::OtherFocusReason);editor->selectAll();QTest::keyClicks(editor,"10+");QTest::keyClick(editor,Qt::Key_Return);QTest::keyClicks(editor,"16");QTest::qWait(300);events();check(editor->toPlainText()==source&&result->text()=="Draft result: 26 · not applied","Original fulltwo-line draft beforecallerreplacement");same(w.host.session,base);view(w,targets,"artboard");
 w.host.session=caller;same(w.host.session,caller);QSignalSpy applied(apply,&QPushButton::clicked);apply->click();events();const auto observed=w.host.session;const QString draft=editor?editor->toPlainText():QString{},feedback=result?result->text():QString{};const bool panel_visible=panel&&panel->isVisible();write(path+"-observed.nect",observed.document());write(path+"-preview-observed.nect",observed.preview_document());check(w.grab().save(path+"-window.png"),"StyledWindow exact afterApply");if(panel)check(panel->grab().save(path+"-panel.png"),"Retained original draftfeedback");
 QJsonObject state{{"mode",mode},{"success",success},{"expected_error",error},{"feedback",feedback},{"draft",draft},{"panel_visible_after",panel_visible},{"key_press_count",actor.presses},{"key_release_count",actor.releases},{"mouse_event_count",actor.mouse},{"prefix_source",QString("=10+")},{"apply_signal_count",applied.count()},{"document_equal",observed.document()==intended},{"history_equal",observed.history()==goal.history()},{"preview_equal",observed.preview_document()==preview_doc},{"gesture_active",observed.gesture_active()},{"gesture_generation",static_cast<qint64>(observed.gesture_generation())},{"revision",static_cast<qint64>(observed.revision())},{"actual_save",!active},{"view_equal",w.canvas->selections()==targets&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="artboard"},{"status",w.statusBar()->currentMessage()}};QFile meta(path+"-producer.json");check(meta.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable targetproducer readback");meta.write(QJsonDocument(state).toJson());meta.close();std::cout<<"TEXT_SINGLE_SCALAR_EXPRESSION_PREFIX_GESTURE mode="<<mode<<" doc="<<state["document_equal"].toBool()<<" history="<<state["history_equal"].toBool()<<" panel="<<panel_visible<<" feedback="<<feedback.toStdString()<<"\n";
 check(actor.presses==actor.releases&&actor.presses==5&&actor.mouse==0&&applied.count()==1,"Exactoriginalnumeric prefix5keys/mouse0 andsameApply");if(success)check(!panel_visible&&w.statusBar()->currentMessage().isEmpty(),"Currentpositive closesdraft");else check(panel_visible&&draft==source&&feedback==error+"\nCommitted result is unchanged.","Stale target refuses andpreserves fulloriginaldraft/error");
 same(w.host.session,goal);view(w,targets,"artboard");if(!active){w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,goal);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual fullnativeSave AFTERactor/liveness");}else std::cout<<"ACTIVE_GESTURE_SAVE NOT_RUN mode=0; preservecallerpreview withoutcancel\n";w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" actualproductstyle/Qt originalnumeric prefix gesturecontext; physical/clipboard NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_SINGLE_SCALAR_EXPRESSION_PREFIX_GESTURE_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<3;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text single scalar expression prefix gesture cases failed: "+std::to_string(failures));std::cout<<"PASS Text single scalar expression prefix gesture "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text single scalar expression prefix gesture "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
