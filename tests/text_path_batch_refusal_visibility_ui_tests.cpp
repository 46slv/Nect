#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
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
    follower.text->content_driver=TextContentDriver{{"source","","text.content"}};
    document.objects.emplace(source.id,source);document.objects.emplace(first.id,first);
    document.objects.emplace(second.id,second);document.objects.emplace(path.id,path);
    document.objects.emplace(follower.id,follower);
    document.compositions.front().roots={"source","first","second","shape","follower"};
    return document;
}




void flow(const QString& root,int mode){
 auto initial=fixture(true);initial.objects.at("source").text->family="Arial";for(const auto& id:pair){auto& t=*initial.objects.at(id).text;t.content=id=="first"?"ABC":"DEF";t.layout="auto";t.direction="horizontal";t.layout_driver.reset();t.direction_driver.reset();t.family="Arial";}valid(initial);Session base(initial);auto caller_doc=initial;caller_doc.objects.at("first").name="Changed retained Text";valid(caller_doc);auto typed=base;typed.apply({Rename{"first","Changed retained Text"}},0);check(typed.document()==caller_doc,"Independent native-valid selectedObject change equals canonical Rename BEFORE Window");Session caller(caller_doc);auto intended=caller_doc;for(const auto& id:pair)intended.objects.at(id).text->path_attachment=TextPathAttachment{"shape","contour","distance",1000,0,false};valid(intended);auto goal=caller;goal.apply({UpdateText{"first",*intended.objects.at("first").text},UpdateText{"second",*intended.objects.at("second").text}},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent full manually intended attachment preserves allotherText fields andequals canonical twoUpdateText oneHistory BEFORE Window");auto undone=goal;undone.undo(1);check(undone.document()==caller_doc,"Independent full attachment Undo BEFORE Window");auto redone=undone;redone.redo(2);check(redone.document()==intended,"Independent full attachment Redo BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections({{"first",{}},{"second",{}}});events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original Text on Path Property dock");dock->show();dock->raise();events();QPointer<QComboBox> path_choice=control<QComboBox>(w,"text-path-batch-path");check(path_choice->findData("shape")==1&&path_choice->currentIndex()==0,"Original stable Path ID available");reveal(path_choice);path_choice->setFocus(Qt::OtherFocusReason);events();QSignalSpy path_drafted(path_choice,qOverload<int>(&QComboBox::currentIndexChanged));QTest::keyClick(path_choice,Qt::Key_Down);events();check(path_choice&&path_choice->currentData().toString()=="shape"&&path_drafted.count()==1,"Original keyboard chooses authored Path stableID");QPointer<QComboBox> contour=control<QComboBox>(w,"text-path-batch-contour");check(contour->findData("contour")==1,"Originalstable Contour ID available");reveal(contour);contour->setFocus(Qt::OtherFocusReason);events();QSignalSpy contour_drafted(contour,qOverload<int>(&QComboBox::currentIndexChanged));QTest::keyClick(contour,Qt::Key_Down);events();check(contour&&contour->currentData().toString()=="contour"&&contour_drafted.count()==1,"Original keyboard chooses exact Contour ID");QPointer<QLineEdit> start=control<QLineEdit>(w,"text-path-batch-start");reveal(start);start->setFocus(Qt::OtherFocusReason);events();QSignalSpy start_drafted(start,&QLineEdit::textEdited);QTest::keyClick(start,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(start,"1000");events();check(start&&start->text()=="1000"&&start_drafted.count()==4,"Original start keyboard draft1000");same(w.host.session,base);QPointer<QPushButton> producer=control<QPushButton>(w,"text-path-batch-apply");reveal(producer);QPointer<QLabel> label=producer->parentWidget()->findChild<QLabel*>("text-path-batch-status");QPointer<QPushButton> detach=producer->parentWidget()->findChild<QPushButton*>("text-path-batch-detach"),cancel=producer->parentWidget()->findChild<QPushButton*>("text-path-batch-cancel");check(label&&detach&&cancel,"Original Path feedback/Detach/Cancel available");w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();const auto observed=w.host.session;const auto status=label?label->text():QString{};const bool pending=path_choice&&contour&&start&&path_choice->currentData().toString()=="shape"&&contour->currentData().toString()=="contour"&&start->text()=="1000";
 const auto full=[](QWidget* c){return c&&!c->rect().isEmpty()&&c->visibleRegion().boundingRect().contains(c->rect());};const bool status_visible=full(label),path_visible=full(path_choice),contour_visible=full(contour),start_visible=full(start),apply_visible=full(producer),detach_visible=full(detach),cancel_visible=full(cancel);const auto rect=label->rect(),shown=label->visibleRegion().boundingRect();const auto point=label->mapTo(&w,QPoint{});write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png")&&label->parentWidget()->grab().save(path+"-panel.png"),"Actual productstyled Window andPathpanel withoutforcedlayout/postrefusalscroll");QJsonObject metadata{{"signal_count",emitted.count()},{"path_signal_count",1},{"contour_signal_count",1},{"draft_signal_count",4},{"status",status},{"pending_preserved",pending},{"document_equal",observed.document()==caller.document()},{"history_equal",observed.history()==caller.history()},{"preview_equal",observed.preview_document()==caller.preview_document()},{"fully_visible",status_visible},{"path_visible",path_visible},{"contour_visible",contour_visible},{"start_visible",start_visible},{"apply_visible",apply_visible},{"detach_visible",detach_visible},{"cancel_visible",cancel_visible},{"label_x",point.x()},{"label_y",point.y()},{"label_width",rect.width()},{"label_height",rect.height()},{"visible_width",shown.width()},{"visible_height",shown.height()},{"window_width",w.width()},{"window_height",w.height()}};QFile state(path+"-producer.json");check(state.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable original Path producer/geometry readback");state.write(QJsonDocument(metadata).toJson());state.close();same(w.host.session,caller);check(emitted.count()==1&&pending&&status=="PROPERTY_CONFLICT: The retained Text changed; refresh before applying","Original Path Apply refuseswholecaller preserves Path/Contour/start draft");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable fullcaller native saved; producer/geometry/pending beforeSave only");std::cout<<"TEXT_PATH_BATCH_REFUSAL_VISIBILITY full="<<status_visible<<" path="<<path_visible<<" contour="<<contour_visible<<" start="<<start_visible<<" apply="<<apply_visible<<" detach="<<detach_visible<<" cancel="<<cancel_visible<<" label="<<rect.width()<<"x"<<rect.height()<<" visible="<<shown.width()<<"x"<<shown.height()<<" at="<<point.x()<<","<<point.y()<<"\n";check(status_visible&&path_visible&&contour_visible&&start_visible&&apply_visible&&detach_visible&&cancel_visible,"Original TextPath refusal/Path/Contour/start/Attach/Detach/Cancel fullyvisible after naturalQtsettling; fullcaller/native/pendingunchanged");w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt TextPath; physical/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_PATH_BATCH_REFUSAL_VISIBILITY_ROOT"),root=requested.isEmpty()?temp.path():requested;flow(root,0);std::cout<<"CASE PASS mode=0\nPASS Text path batch refusal visibility "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"CASE FAIL mode=0\nFAIL Text path batch refusal visibility "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
