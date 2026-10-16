#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
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



const std::vector<Canvas::Selection> target_view={{"first",{}},{"second",{}}},shape_view={{"shape",{}}},source_view={{"follower",{}}};
void view(Window& w,const std::vector<Canvas::Selection>& expected,const Id& board="artboard"){check(w.canvas->selections()==expected&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()==board,"Exact independent Canvas selection IDs/Composition/Artboard oracle");}
void flow(const QString& root,int mode){
 const bool stale=mode>=1&&mode<=3,apply=mode==0||mode==3||mode==5,cancel=mode==2||mode==4;const Id born_board=mode>=4?"other-artboard":"artboard",expected_board=stale?"other-artboard":born_board;
 auto initial=fixture(true);initial.compositions.front().artboards.push_back({"other-artboard","Other canvas",800,0,640,480});initial.objects.at("first").text->weight_expression.reset();initial.objects.at("first").text->weight_driver=TextWeightDriver{{"source","","text.weight"},40};initial.objects.at("first").text->weight=500;initial.objects.at("second").text->weight_driver.reset();initial.objects.at("second").text->weight_expression=Expression{"650",1};initial.objects.at("second").text->weight=580;valid(initial);Session base(initial);auto caller_doc=initial;

 valid(caller_doc);Session caller(caller_doc);auto intended=caller_doc;for(const auto& id:pair)intended.objects.at(id).text->weight_expression.reset();intended.objects.at("first").text->weight_driver=TextWeightDriver{{"follower","","text.weight"},40};intended.objects.at("second").text->weight_driver=TextWeightDriver{{"follower","","text.weight"},250};valid(intended);auto goal=caller;goal.apply({TextWeightBatch{TextWeightBatchMode::link,{{"first","","text.weight"},{"second","","text.weight"}},0,Ref{"follower","","text.weight"},true,true}},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent fullmanualrelative Link/fallbackpreserved equalscanonical TextWeightBatch oneHistory BEFOREWindow");auto undone=goal;undone.undo(1);check(undone.document()==caller_doc,"Independent fullLink Undo BEFOREWindow");auto redone=undone;redone.redo(2);check(redone.document()==intended,"Independent fullLink Redo BEFOREWindow");
 const auto expected=(apply&&!stale)?goal:caller;const auto expected_view=mode==1?target_view:stale?source_view:target_view;const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",expected.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_active_artboard("composition",born_board,false);events();w.canvas->set_selections(target_view);events();view(w,target_view,born_board);auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"OriginalMultiTextWeight Propertydock");dock->show();dock->raise();events();QPointer<QCheckBox> replace=control<QCheckBox>(w,"text-weight-batch-replace");reveal(replace);QSignalSpy consented(replace,&QCheckBox::toggled);replace->setFocus(Qt::OtherFocusReason);QTest::keyClick(replace,Qt::Key_Space);check(replace&&replace->isChecked()&&consented.count()==1,"Original checkbox source replacement consent beforeOpen");QPointer<QToolButton> drive=control<QToolButton>(w,"text-weight-batch-driver");reveal(drive);QPointer<QAction> opener=drive->menu()->actions().at(1);const auto sid=w.host.session_id;QSignalSpy opened(opener,&QAction::triggered);opener->trigger();events();check(opened.count()==1,"One originalMultiTextContentLink opener");QPointer<QDialog> dialog;for(auto* d:w.findChildren<QDialog*>("text-source-picker"))if(d->isVisible())dialog=d;QString status=w.statusBar()->currentMessage();int browse_count=0,actor_count=0,replace_count=1;bool dialog_visible=bool(dialog);
 {
  check(dialog,"OriginalContent source chooser opened");QPointer<QListWidget> list=dialog->findChild<QListWidget*>("text-source-picker-list");QPointer<QLabel> label=dialog->findChild<QLabel*>("text-source-picker-status");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(list&&label&&buttons,"Originalsource list/status/actions");QListWidgetItem* chosen=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()=="follower / text.weight")chosen=list->item(i);check(chosen&&!chosen->isHidden(),"Exactvisible source stableID/property candidate");const auto ref=QJsonDocument::fromJson(chosen->data(Qt::UserRole).toByteArray()).object();check(ref["object"].toString()=="follower"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.weight","OriginalRef bytes bind exactfollower wholeText weight");QSignalSpy browsed(list,&QListWidget::currentItemChanged);
  if(mode==1){w.canvas->set_active_artboard("composition","other-artboard",false);events();same(w.host.session,base);view(w,target_view,expected_board);}list->setCurrentItem(chosen);events();browse_count=browsed.count();check(browse_count==1,"One original list selection signal");same(w.host.session,base);if(mode!=1)view(w,source_view,born_board);

  if(mode!=1){w.host.session=caller;if(mode==2||mode==3)w.canvas->set_active_artboard("composition","other-artboard",false);events();same(w.host.session,caller);view(w,source_view,expected_board);}check(w.host.session_id==sid&&w.host.session.revision()==0,"ExactHostSID/revision retained beforeoriginalchooseractor");
  if(apply||cancel){QPointer<QPushButton> actor=buttons->button(apply?QDialogButtonBox::Apply:QDialogButtonBox::Cancel);check(actor&&actor->isEnabled(),"Originalchooser Link/Cancel enabled");QSignalSpy emitted(actor,&QPushButton::clicked);actor->click();events();actor_count=emitted.count();check(actor_count==1,"One originalchooser Link/Cancel click");}
  status=label?label->text():QString{};dialog_visible=dialog&&dialog->isVisible();
 }
 const auto observed=w.host.session;const bool view_equal=w.canvas->selections()==expected_view&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()==expected_board;write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actualproductstyled content source Window rendered");if(dialog_visible)check(dialog->grab().save(path+"-dialog.png"),"Actualoriginal source dialog rendered");QJsonObject metadata{{"open_signal_count",opened.count()},{"browse_signal_count",browse_count},{"actor_signal_count",actor_count},{"replace_signal_count",replace_count},{"status",status},{"dialog_visible",dialog_visible},{"view_equal",view_equal},{"active_artboard",QString::fromStdString(w.canvas->active_artboard())},{"expected_artboard",QString::fromStdString(expected_board)},{"document_equal",observed.document()==expected.document()},{"history_equal",observed.history()==expected.history()},{"preview_equal",observed.preview_document()==expected.preview_document()}};QFile state(path+"-producer.json");check(state.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutableoriginal source actor/view readback");state.write(QJsonDocument(metadata).toJson());state.close();std::cout<<"TEXT_WEIGHT_SOURCE_ARTBOARD mode="<<mode<<" status="<<status.toStdString()<<" view_equal="<<view_equal<<" doc_equal="<<metadata["document_equal"].toBool()<<"\n";same(w.host.session,expected);view(w,expected_view,expected_board);if(mode==1||mode==3)check(dialog_visible&&status=="SELECTION_CONFLICT: The weight source chooser artboard changed outside this chooser","Externally changed activeArtboard refusesbrowse/Link withoutchangingview");if(cancel)check(!dialog_visible,"OriginalCancel closeschooser withoutwrongDocumentview restore");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actualimmutable fullWeight/caller native saved; actors/view beforeSave only");if(apply&&!stale){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt WeightSourceArtboard; physical/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_WEIGHT_SOURCE_ARTBOARD_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<6;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text weight source artboard cases failed: "+std::to_string(failures));std::cout<<"PASS Text weight source artboard "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text weight source artboard "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
