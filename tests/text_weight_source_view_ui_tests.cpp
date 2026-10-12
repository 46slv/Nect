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
#include <QJsonArray>
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



QJsonObject view(Canvas* c){QJsonArray selections;for(const auto& a:c->selections())selections.push_back(QJsonObject{{"object",QString::fromStdString(a.object)},{"point",QString::fromStdString(a.point)}});return {{"composition",QString::fromStdString(c->active_composition())},{"artboard",QString::fromStdString(c->active_artboard())},{"selections",selections}};}
void flow(const QString& root,int mode){
 auto initial=fixture(true);initial.objects.at("first").text->weight_expression.reset();initial.objects.at("first").text->weight_driver=TextWeightDriver{{"source","","text.weight"},40};initial.objects.at("first").text->weight=500;initial.objects.at("second").text->weight_driver.reset();initial.objects.at("second").text->weight_expression=Expression{"650",1};initial.objects.at("second").text->weight=580;valid(initial);Session base(initial);auto caller_doc=initial;if(mode){caller_doc.id="new-view-document";caller_doc.compositions.front().id="new-composition";caller_doc.compositions.front().artboards.front().id="new-artboard";}valid(caller_doc);Session caller(caller_doc);check(evaluate_text_weight(caller_doc,"first")==440&&evaluate_text_weight(caller_doc,"second")==650,"Independent full native-valid replacement Document retains exact original weight links/evaluation BEFORE Window");const std::vector<Canvas::Selection> target_selection{{"first",{}},{"second",{}}},source_selection{{"follower",{}}},caller_selection{{"shape",{}}};const Id expected_composition=mode?"new-composition":"composition",expected_artboard=mode?"new-artboard":"artboard";const auto expected_selection=mode?caller_selection:target_selection;check(caller.revision()==0&&caller.history()==base.history()&&caller.preview_document()==caller_doc&&!caller.gesture_active(),"Independent complete canonical replacement caller Session/native/History/preview BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",caller_doc);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections(target_selection);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original Source view Property dock");dock->show();dock->raise();events();QPointer<QToolButton> drive=control<QToolButton>(w,"text-weight-batch-driver");reveal(drive);QPointer<QAction> action=drive->menu()->actions().at(1);QSignalSpy opened(action,&QAction::triggered);action->trigger();events();check(opened.count()==1,"One original Source QAction opens chooser");QPointer<QDialog> dialog;for(auto* d:w.findChildren<QDialog*>("text-source-picker"))if(d->isVisible())dialog=d;check(dialog,"Original retained source chooser");auto* list=dialog->findChild<QListWidget*>("text-source-picker-list");QListWidgetItem* chosen=nullptr;for(int i=0;list&&i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()=="follower / text.weight")chosen=list->item(i);check(chosen&&!chosen->isHidden(),"Exact visible source stable Ref from original chooser");QSignalSpy browsed(list,&QListWidget::currentItemChanged);if(mode==1){w.host.session=caller;w.canvas->set_active_artboard(expected_composition,expected_artboard,false);w.canvas->set_selections(caller_selection);events();same(w.host.session,caller);check(w.canvas->active_composition()==expected_composition&&w.canvas->active_artboard()==expected_artboard&&w.canvas->selections()==caller_selection,"Exact current caller view established before original stale browse");}list->setCurrentItem(chosen);events();same(w.host.session,mode==1?caller:base);const auto after_browse=view(w.canvas);const bool browse_equal=w.canvas->selections()==(mode==1?caller_selection:source_selection);check(browsed.count()==1,"One original source list selection signal");int cancel_count=0;
 if(mode!=1){if(mode==2){w.host.session=caller;w.canvas->set_active_artboard(expected_composition,expected_artboard,false);w.canvas->set_selections(caller_selection);events();same(w.host.session,caller);check(w.canvas->active_composition()==expected_composition&&w.canvas->active_artboard()==expected_artboard&&w.canvas->selections()==caller_selection,"Exact current caller view established before original stale Cancel");}auto* buttons=dialog->findChild<QDialogButtonBox*>();QPointer<QPushButton> cancel=buttons->button(QDialogButtonBox::Cancel);QSignalSpy cancelled(dialog,&QDialog::rejected);QSignalSpy pressed(cancel,&QPushButton::clicked);cancel->click();events();cancel_count=cancelled.count();check(pressed.count()==1&&cancel_count==1,"One original chooser Cancel and rejected signal");}
 const auto observed=w.host.session;const auto actual_view=view(w.canvas);const bool view_equal=w.canvas->active_composition()==expected_composition&&w.canvas->active_artboard()==expected_artboard&&w.canvas->selections()==expected_selection;write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actual productstyled original Source browse/Cancel Window rendered");if(dialog&&dialog->isVisible())check(dialog->grab().save(path+"-picker.png"),"Actual retained original Source picker rendered");QJsonObject metadata{{"opener_count",1},{"browse_count",browsed.count()},{"cancel_count",cancel_count},{"browse_equal",browse_equal},{"view_equal",view_equal},{"after_browse",after_browse},{"view",actual_view},{"document_equal",observed.document()==caller.document()},{"history_equal",observed.history()==caller.history()},{"preview_equal",observed.preview_document()==caller.preview_document()}};QFile state(path+"-producer.json");check(state.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable original source browse/Cancel producer readback");state.write(QJsonDocument(metadata).toJson());state.close();std::cout<<"TEXT_WEIGHT_SOURCE_VIEW mode="<<mode<<" browse_equal="<<browse_equal<<" view_equal="<<view_equal<<" caller_equal="<<(observed.document()==caller.document())<<"\n";same(w.host.session,caller);check(browse_equal&&view_equal,"Current sameDocument browse/restoration or stale crossDocument view preserved");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full view-neutral caller native saved; producer/view readback before Save only");if(dialog)dialog->reject();events();w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt originalSource browse/Cancel; physical/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_WEIGHT_SOURCE_VIEW_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<3;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text weight Source view cases failed: "+std::to_string(failures));std::cout<<"PASS Text weight Source view "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text weight Source view "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
