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
class ArrowEvents final:public QObject{public:QPointer<QWidget> target;int presses=0,releases=0,key_presses=0,key_releases=0;bool eventFilter(QObject* watched,QEvent* event)override{if(watched==target&&(event->type()==QEvent::KeyPress||event->type()==QEvent::KeyRelease)&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Space){if(event->type()==QEvent::KeyPress)++key_presses;else ++key_releases;}if(watched==target&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonRelease)&&static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton){if(event->type()==QEvent::MouseButtonPress)++presses;else ++releases;}return false;}};
void flow(const QString& root,int mode){

 const bool success=mode==2;auto initial=fixture(true);auto& first=initial.objects.at("first").text->parameters.at("font_size");auto& second=initial.objects.at("second").text->parameters.at("font_size");first.literal=20.25;first.binding=Binding{{"source","","text.font_size"},1.25,0};first.expression.reset();second.literal=32.5;second.binding.reset();second.expression=Expression{"32.5",1};valid(initial);Session base(initial);auto caller_doc=initial;auto& changed=caller_doc.objects.at("follower").text->parameters.at("font_size");if(mode==0)changed.binding=Binding{{"first","","text.font_size"},1,0};if(mode==1)changed.expression=Expression{"ref(\"first\",\"\",\"text.font_size\")",1};if(mode==2)changed.literal=32;valid(caller_doc);auto changed_source=base;if(mode==0)changed_source.apply({UpdateText{"follower",*caller_doc.objects.at("follower").text}},0);else if(mode==1)changed_source.apply({SetExpression{{{"follower","","text.font_size"}},*changed.expression,false}},0);else changed_source.apply({Set{{"follower","","text.font_size"},32}},0);check(changed_source.document()==caller_doc,"Independent sourceBinding/Expression/value replacement canonicalcommand BEFOREWindow");check(evaluate(caller_doc).at({"first","","text.font_size"})==30&&evaluate(caller_doc).at({"follower","","text.font_size"})==(success?32:30),"Independent currentcaller source dependency isvalid BEFOREWindow");Session caller(caller_doc);const std::vector<Ref> refs{{"first","","text.font_size"}};auto intended=caller_doc;intended.objects.at("first").text->parameters.at("font_size").binding=Binding{{"follower","","text.font_size"},1,0};auto goal=caller,undone=caller,redone=caller;
 if(success){valid(intended);goal.apply({LinkProperties{refs,{"follower","","text.font_size"},false}},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent fullmanual source32 absoluteBinding/fallback/secondExpression/canonicaloneHistory BEFOREWindow");undone=goal;undone.undo(1);auto undo_history=goal.history();undo_history.current_id=caller.history().current_id;check(undone.document()==caller_doc&&undone.history()==undo_history&&undone.revision()==2&&!undone.gesture_active()&&undone.preview_document()==caller_doc,"Independent fullUndo sourcevaluecaller BEFOREWindow");redone=undone;redone.redo(2);check(redone.document()==intended&&redone.history()==goal.history()&&redone.revision()==3&&!redone.gesture_active()&&redone.preview_document()==intended,"Independent fullRedo manualsourcevalue goal BEFOREWindow");}
 else {bool manual_cycle=false,command_cycle=false;try{(void)evaluate(intended);}catch(const Error& e){manual_cycle=e.code=="DEPENDENCY_CYCLE"&&std::string(e.what())=="Property dependency cycle";}check(manual_cycle,"Independent manual candidate closesrawBinding/Expression cycle BEFOREWindow");auto rejected=caller;try{rejected.apply({LinkProperties{refs,{"follower","","text.font_size"},false}},0);}catch(const Error& e){command_cycle=e.code=="DEPENDENCY_CYCLE"&&std::string(e.what())=="Property dependency cycle";}check(command_cycle,"Independent canonical LinkProperties exactcycle refusal BEFOREWindow");same(rejected,caller);}
 const std::vector<Canvas::Selection> targets{{"first",{}}},source{{"follower",{}}};const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?intended:caller_doc);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections(targets);events();view(w,targets,"artboard");auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original Properties dock");dock->show();dock->raise();events();QPointer<QLineEdit> input;for(auto* c:w.findChildren<QLineEdit*>()){if(!c->isVisible())continue;const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")input=c;}check(input,"Exact original fontSize field");reveal(input);check(!input->isModified(),"Original input neutral beforearrow");same(w.host.session,base);
 QPointer<QPushButton> pick;for(auto* c:w.findChildren<QPushButton*>("property-source-pick")){const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(c->isVisible()&&ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")pick=c;}check(pick,"Exact original sourcearrow");reveal(pick);QSignalSpy opened(pick,&QPushButton::clicked);ArrowEvents arrow_events;arrow_events.target=pick;qApp->installEventFilter(&arrow_events);QTest::mouseClick(pick,Qt::LeftButton);qApp->removeEventFilter(&arrow_events);events();QPointer<QDialog> picker=w.findChild<QDialog*>("property-source-picker");check(picker&&picker->isVisible(),"Original sourcearrow chooser opens BEFOREcaller replacement");same(w.host.session,base);view(w,targets,"artboard");auto* list=picker->findChild<QListWidget*>("property-source-picker-list");auto* buttons=picker->findChild<QDialogButtonBox*>();check(list&&buttons,"Original chooser controls");QListWidgetItem* selected=nullptr;for(int i=0;i<list->count();++i){auto* row=list->item(i);const auto ref=QJsonDocument::fromJson(row->data(Qt::UserRole).toByteArray()).object();if(ref["object"].toString()=="follower"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")selected=row;}check(selected&&!selected->isHidden()&&(selected->flags()&Qt::ItemIsEnabled),"Exact original compatible sourceRef");QSignalSpy browsed(list,&QListWidget::currentItemChanged);list->setCurrentItem(selected);events();view(w,source,"artboard");same(w.host.session,base);w.host.session=caller;same(w.host.session,caller);check(picker->grab().save(path+"-picker.png"),"Original styledchooser beforeLink");QPointer<QPushButton> ok=buttons->button(QDialogButtonBox::Ok);check(ok,"OriginalOk button");reveal(ok);QSignalSpy linked(ok,&QPushButton::clicked);ok->click();events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();const auto expected_view=success?targets:source;const auto observed_view=w.canvas->selections();write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Original styledWindow afterLink");
 QJsonObject state{{"arrow_signal_count",opened.count()},{"arrow_press_count",arrow_events.presses},{"arrow_release_count",arrow_events.releases},{"arrow_key_press_count",arrow_events.key_presses},{"arrow_key_release_count",arrow_events.key_releases},{"browse_signal_count",browsed.count()},{"link_signal_count",linked.count()},{"status",status},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())},{"view_equal",observed_view==expected_view&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="artboard"},{"expected_view",view_json(expected_view)},{"observed_view",view_json(observed_view)},{"picker_visible_after",picker&&picker->isVisible()}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable originalarrow/Link targetproperty readback");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"TEXT_SINGLE_SCALAR_ARROW_SOURCE_CYCLE mode="<<mode<<" doc_equal="<<state["document_equal"].toBool()<<" status="<<status.toStdString()<<"\n";
 check(opened.count()==0&&arrow_events.presses==1&&arrow_events.releases==1&&arrow_events.key_presses==0&&arrow_events.key_releases==0&&browsed.count()==1&&linked.count()==1,"Exact original mousearrow/list/Ok actor");same(w.host.session,success?goal:caller);view(w,expected_view,"artboard");check(bool(picker&&picker->isVisible())==!success,"Stale target refusal preserveschooser orcurrentLink closes");const QString expected="DEPENDENCY_CYCLE: Property dependency cycle";check(success?status.isEmpty():status==expected,"ExacttargetDoc/Source/Scalar refusal orcurrentabsoluteLink");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual complete caller/Link nativeSave");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt originalArrowSourceDependencyCycleLink; clipboard/physical/geometry/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_SINGLE_SCALAR_ARROW_SOURCE_CYCLE_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<3;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text single scalar arrow source cycle cases failed: "+std::to_string(failures));std::cout<<"PASS Text single scalar arrow source cycle "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text single scalar arrow source cycle "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
