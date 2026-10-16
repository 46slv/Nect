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



void flow(const QString& root,int mode){
 const bool source_mode=mode==2||mode==3||mode==5,relative=mode!=2,before=mode==2,driven=mode!=0&&mode!=4,success=mode>=4;
 auto initial=fixture(true);auto& f=*initial.objects.at("first").text;auto& s=*initial.objects.at("second").text;f.weight=500;s.weight=650;f.weight_driver.reset();f.weight_expression.reset();s.weight_driver.reset();s.weight_expression.reset();
 if(driven){f.weight_driver=TextWeightDriver{{"source","","text.weight"},40};s.weight=580;s.weight_expression=Expression{"650",1};}
 Composition other;other.id="other-composition";other.name="Other composition";other.artboards={{"other-artboard","Other canvas",0,0,640,480}};initial.compositions.push_back(other);valid(initial);Session base(initial);auto caller_doc=initial;
 const auto move=[&](const Id& id){auto& roots=caller_doc.compositions.front().roots;roots.erase(std::find(roots.begin(),roots.end(),id));caller_doc.compositions.back().roots.push_back(id);};
 if(mode<4){move("first");if(mode==1)move("second");}
 if(mode==4){Object group;group.id="same-composition-group";group.name="Same composition";group.kind=Kind::group;group.children=pair;caller_doc.objects.emplace(group.id,group);auto& roots=caller_doc.compositions.front().roots;for(const auto& id:pair)roots.erase(std::find(roots.begin(),roots.end(),id));roots.push_back(group.id);}
 if(mode==5)move("follower");valid(caller_doc);Session caller(caller_doc);
 std::vector<Ref> refs{{"first","","text.weight"},{"second","","text.weight"}};const Ref source{"follower","","text.weight"};auto intended=caller_doc;
 if(source_mode){intended.objects.at("first").text->weight_driver=TextWeightDriver{source,relative?40:0};intended.objects.at("second").text->weight_driver=TextWeightDriver{source,relative?250:0};for(const auto& id:pair)intended.objects.at(id).text->weight_expression.reset();}
 else if(driven){intended.objects.at("first").text->weight=440;intended.objects.at("second").text->weight=650;for(const auto& id:pair){auto& t=*intended.objects.at(id).text;t.weight_driver.reset();t.weight_expression.reset();}}
 else{intended.objects.at("first").text->weight+=25;intended.objects.at("second").text->weight+=25;}
 valid(intended);auto goal=caller;const auto command=source_mode?TextWeightBatch{TextWeightBatchMode::link,refs,0,source,relative,true}:driven?TextWeightBatch{TextWeightBatchMode::unlink,refs}:TextWeightBatch{TextWeightBatchMode::edit,refs,25,{},true,false};goal.apply({command},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent complete manually intended per-target native equals canonical oneHistory beforeWindow; globalSource refs retained");auto undone=goal;undone.undo(1);check(undone.document()==caller_doc,"Independent full exact owner/root/links/fallback/Expression/native Undo BEFOREWindow");auto redone=undone;redone.redo(2);check(redone.document()==intended,"Independent full intended native Redo BEFOREWindow");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?goal.document():caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections({{"first",{}},{"second",{}}});events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original weight Property dock");dock->show();dock->raise();events();QPointer<QLineEdit> input=control<QLineEdit>(w,"text-weight-batch-value");reveal(input);input->setFocus(Qt::OtherFocusReason);events();check(input,"Original draft survives focus");QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=25");check(input&&input->text()=="+=25","Original independent numeric pending draft");
 QPointer<QDialog> dialog;QString status;int opener_count=0,signal_count=0;bool pending=false;
 if(source_mode){QPointer<QCheckBox> replace=control<QCheckBox>(w,"text-weight-batch-replace");reveal(replace);QSignalSpy consent(replace,&QCheckBox::toggled);replace->setFocus(Qt::OtherFocusReason);QTest::keyClick(replace,Qt::Key_Space);check(replace&&replace->isChecked()&&consent.count()==1,"Original explicit checkbox consent");QPointer<QToolButton> drive=control<QToolButton>(w,"text-weight-batch-driver");reveal(drive);QPointer<QLabel> label=drive->parentWidget()->findChild<QLabel*>("text-weight-batch-status");check(label,"Original source local status");QPointer<QAction> action=drive->menu()->actions().at(relative?1:0);QSignalSpy opened(action,&QAction::triggered);if(before)w.host.session=caller;action->trigger();events();opener_count=opened.count();for(auto* d:w.findChildren<QDialog*>("text-source-picker"))if(d->isVisible())dialog=d;status=label?label->text():QString{};pending=input&&input->text()=="+=25";
  if(!before){check(dialog,"Current original source chooser opens");auto* list=dialog->findChild<QListWidget*>("text-source-picker-list");QListWidgetItem* chosen=nullptr;for(int i=0;list&&i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()=="follower / text.weight")chosen=list->item(i);check(chosen&&!chosen->isHidden(),"Exact visible stable chosenSource Ref");list->setCurrentItem(chosen);events();check(dialog,"Original chooser survives browsing");w.host.session=caller;same(w.host.session,caller);auto* buttons=dialog->findChild<QDialogButtonBox*>();QPointer<QPushButton> link=buttons->button(QDialogButtonBox::Apply);QPointer<QLabel> picker_status=dialog->findChild<QLabel*>("text-source-picker-status");check(link&&picker_status,"Original Link/status alive");QSignalSpy emitted(link,&QPushButton::clicked);link->click();events();signal_count=emitted.count();status=picker_status?picker_status->text():QString{};pending=false;}
 }else{QPointer<QPushButton> producer=control<QPushButton>(w,driven?"text-weight-batch-unlink":"text-weight-batch-apply");reveal(producer);QPointer<QLabel> label=producer->parentWidget()->findChild<QLabel*>("text-weight-batch-status");check(label,"Original numeric/Unlink status");w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();signal_count=emitted.count();status=label?label->text():QString{};pending=input&&input->text()=="+=25";}
 const auto observed=w.host.session;const bool picker_open=dialog&&dialog->isVisible();write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actual original productstyled Window rendered");if(picker_open)check(dialog->grab().save(path+"-picker.png"),"Actual source chooser refusal rendered");QJsonObject metadata{{"source_mode",source_mode},{"driven",driven},{"relative",relative},{"opener_count",opener_count},{"signal_count",signal_count},{"picker_open",picker_open},{"status",status},{"pending",pending},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())}};QFile state(path+"-producer.json");check(state.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable original producer readback");state.write(QJsonDocument(metadata).toJson());state.close();std::cout<<"TEXT_WEIGHT_TARGET_COMPOSITION mode="<<mode<<" picker="<<picker_open<<" doc_equal="<<(observed.document()==(success?goal.document():caller.document()))<<" status="<<status.toStdString()<<"\n";
 same(w.host.session,success?goal:caller);check(opener_count==(source_mode?1:0)&&signal_count==(before?0:1),"One original producer invocation");const QString expected="SELECTION_CONFLICT: The retained Text target moved to another composition; refresh before applying Text weights";check(success?!picker_open:(status==expected&&picker_open==(source_mode&&!before)&&((source_mode&&!before)||pending)),"Exact stale targetownership refusal or sameComposition nesting/globalchosenSource usability");if(dialog)dialog->reject();events();same(w.host.session,success?goal:caller);w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual complete immutable saved native; originalproducer readback BEFORESave");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt Source QAction and Link; physical/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_WEIGHT_TARGET_COMPOSITION_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<6;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text weight target composition cases failed: "+std::to_string(failures));std::cout<<"PASS Text weight target composition "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text weight target composition "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
