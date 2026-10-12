#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QContextMenuEvent>
#include <QMouseEvent>
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
class ArrowEvents final:public QObject{public:QPointer<QWidget> target;int presses=0,releases=0;bool eventFilter(QObject* watched,QEvent* event)override{if(watched==target&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonRelease)&&static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton){if(event->type()==QEvent::MouseButtonPress)++presses;else ++releases;}return false;}};
void flow(const QString& root,int mode){
 auto initial=fixture(true);auto board=initial.compositions.front().artboards.front();check(board.local_guides.empty()&&!board.template_assignment,"Independent secondArtboard clone hasno aliased localGuide/template IDs BEFOREWindow");board.id="second-artboard";board.name="Second board";initial.compositions.front().artboards.push_back(board);valid(initial);Session base(initial);const std::vector<Canvas::Selection> targets{{"first",{}}},source{{"follower",{}}},outside{{"shape",{}}};for(const auto& id:{"first","second","follower","shape"})check(initial.objects.contains(id),"Independent view IDs exist BEFOREWindow");
 const Id original_board=mode==5?"second-artboard":"artboard",expected_board=(mode==3||mode==4)?"second-artboard":original_board;
 const auto expected_view=(mode==1||mode==2)?outside:(mode==4||mode==6)?source:targets;
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",base.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_active_artboard("composition",original_board,false);w.canvas->set_selections(targets);events();view(w,targets,original_board);auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original propertydock");dock->show();dock->raise();events();QPointer<QLineEdit> input;for(auto* c:w.findChildren<QLineEdit*>()){if(!c->isVisible())continue;const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")input=c;}check(input,"Exact original SingleText fontSize source field");reveal(input);same(w.host.session,base);check(!input->isModified(),"Original field unmodified");QPointer<QPushButton> pick;for(auto* candidate:w.findChildren<QPushButton*>("property-source-pick")){const auto ref=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();if(candidate->isVisible()&&ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")pick=candidate;}check(pick,"Exact original SingleTextfontSize sourcearrow");reveal(pick);QSignalSpy opened(pick,&QPushButton::clicked);ArrowEvents arrow_events;arrow_events.target=pick;qApp->installEventFilter(&arrow_events);QTest::mouseClick(pick,Qt::LeftButton);qApp->removeEventFilter(&arrow_events);events();QPointer<QDialog> picker=w.findChild<QDialog*>("property-source-picker");check(picker&&picker->isVisible(),"Original sourcearrow chooser opens");same(w.host.session,base);view(w,targets,original_board);auto* list=picker->findChild<QListWidget*>("property-source-picker-list");auto* buttons=picker->findChild<QDialogButtonBox*>();check(list&&buttons,"Original source chooser controls");QListWidgetItem* selected=nullptr;for(int i=0;i<list->count();++i){auto* row=list->item(i);const auto json=QJsonDocument::fromJson(row->data(Qt::UserRole).toByteArray()).object();if(json["object"].toString()=="follower"&&json["point"].toString().isEmpty()&&json["field"].toString()=="text.font_size")selected=row;}check(selected&&!selected->isHidden()&&(selected->flags()&Qt::ItemIsEnabled),"Exact original compatible source Ref");
 if(mode==1){w.canvas->set_selections(outside);events();view(w,outside,original_board);}
 if(mode==3){w.canvas->set_active_artboard("composition","second-artboard",false);events();view(w,targets,"second-artboard");}
 QSignalSpy browsed(list,&QListWidget::currentItemChanged);list->setCurrentItem(selected);events();const auto browse_view=w.canvas->selections();const auto browse_board=w.canvas->active_artboard();
 if(mode==2){w.canvas->set_selections(outside);events();view(w,outside,original_board);}
 if(mode==4){w.canvas->set_active_artboard("composition","second-artboard",false);events();view(w,source,"second-artboard");}
 if(mode==6){w.canvas->set_selections(outside);events();view(w,outside,original_board);w.canvas->set_selections(source);events();view(w,source,original_board);}
 check(picker&&picker->isVisible()&&picker->grab().save(path+"-picker.png"),"Original productstyled picker rendered BEFORECancel");QPointer<QPushButton> cancel=buttons->button(QDialogButtonBox::Cancel);reveal(cancel);QSignalSpy cancelled(cancel,&QPushButton::clicked);cancel->click();const auto cancel_count=cancelled.count();events();const auto observed=w.host.session;const auto actual_view=w.canvas->selections();const auto actual_board=w.canvas->active_artboard();const auto status=w.statusBar()->currentMessage();write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actual productstyled original Window/view afterCancel");
 const bool browse_equal=browse_view==((mode==1||mode==3)?(mode==1?outside:targets):source)&&browse_board==((mode==3)?Id{"second-artboard"}:original_board);
 QJsonObject state{{"arrow_signal_count",opened.count()},{"arrow_press_count",arrow_events.presses},{"arrow_release_count",arrow_events.releases},{"browse_signal_count",browsed.count()},{"cancel_signal_count",cancel_count},{"status",status},{"browse_equal",browse_equal},{"view_equal",actual_view==expected_view&&w.canvas->active_composition()=="composition"&&actual_board==expected_board},{"expected_view",view_json(expected_view)},{"observed_view",view_json(actual_view)},{"expected_board",QString::fromStdString(expected_board)},{"observed_board",QString::fromStdString(actual_board)},{"document_equal",observed.document()==base.document()},{"history_equal",observed.history()==base.history()},{"preview_equal",observed.preview_document()==base.preview_document()},{"picker_visible_after",picker&&picker->isVisible()}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutableoriginal source Browse/Cancel/view readback");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"TEXT_SINGLE_SCALAR_ARROW_SOURCE_VIEW mode="<<mode<<" browse_equal="<<browse_equal<<" view_equal="<<state["view_equal"].toBool()<<" status="<<status.toStdString()<<"\n";
 check(opened.count()==0&&arrow_events.presses==1&&arrow_events.releases==1&&browsed.count()==1&&cancel_count==1,"One exactoriginal Qt sourcearrow/list Browse/Cancel actor");same(w.host.session,base);check(browse_equal,"OriginalBrowse preserves external view or navigates own source");view(w,expected_view,expected_board);check(!picker||!picker->isVisible(),"Original Cancel closeschooser");check(status==((mode==1||mode==3)?QString{"SELECTION_CONFLICT: Source picker view changed; reopen it"}:QString{}),"Exactview context refusal orquietCancel");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual complete authorednative preserved across sourceview-only actions");w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt originalArrowSourceBrowseCancel; clipboard/physical/geometry/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_SINGLE_SCALAR_ARROW_SOURCE_VIEW_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<7;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text single scalar arrow source view cases failed: "+std::to_string(failures));std::cout<<"PASS Text single scalar arrow source view "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text single scalar arrow source view "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
