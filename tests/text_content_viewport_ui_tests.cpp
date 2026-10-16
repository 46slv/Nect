#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGroupBox>
#include <QScrollBar>
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
 const auto initial=fixture(true);valid(initial);Session base(initial);const std::string pending="Pending batch draft\nsecond line";auto caller_doc=initial;
 if(mode==1||mode==2){const Id id=mode==1?"first":"second";caller_doc.objects.at(id).text->content="External retained content\r\n";auto canonical=base;canonical.apply({UpdateText{id,*caller_doc.objects.at(id).text}},0);check(canonical.document()==caller_doc,"Independent complete manually changed selected raw content equals canonical UpdateText BEFORE Window");}
 if(mode==3){caller_doc.objects.at("second").text->parameters.at("frame_width").literal=498.1234567890123;auto canonical=base;canonical.apply({UpdateText{"second",*caller_doc.objects.at("second").text}},0);check(canonical.document()==caller_doc,"Independent full selected unrelated Text frame change agrees canonical UpdateText BEFORE Window");}
 valid(caller_doc);Session caller(caller_doc);auto intended=caller_doc;for(const auto& id:pair)intended.objects.at(id).text->content=pending;valid(intended);std::vector<Command> commands;for(const auto& id:pair){auto source=*caller_doc.objects.at(id).text;source.content=pending;commands.push_back(UpdateText{id,source});}auto goal=caller;goal.apply(commands,0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent complete manually intended content-only batch preserving all raw sources/drivers/properties equals canonical oneHistory BEFORE Window");auto undone=goal;undone.undo(1);check(undone.document()==caller_doc,"Independent full caller/source/raw/otherText Undo BEFORE Window");auto redone=undone;redone.redo(2);check(redone.document()==intended,"Independent full raw intended/source Redo BEFORE Window");const bool success=mode==0||mode==3;
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?goal.document():caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections({{"first",{}},{"second",{}}});events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original batch Text Properties dock");dock->show();dock->raise();events();QPointer<QPlainTextEdit> editor=control<QPlainTextEdit>(w,"text-content-literal-editor");reveal(editor);editor->setFocus(Qt::OtherFocusReason);events();check(editor,"Original batch Text editor survives focus");QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);for(auto c:std::string("Pending batch draft")){check(editor,"Original batch draft survives Qt keyboard character");QTest::keyClicks(editor.data(),QString(QChar(c)));}QTest::keyClick(editor,Qt::Key_Return);for(auto c:std::string("second line")){check(editor,"Original batch draft survives Qt keyboard character");QTest::keyClicks(editor.data(),QString(QChar(c)));}check(editor&&editor->toPlainText()==QString::fromStdString(pending),"Original exact pending multiline batch draft");same(w.host.session,base);QPointer<QPushButton> producer=control<QPushButton>(w,"text-content-literal-apply");reveal(producer);QPointer<QLabel> label=producer->parentWidget()->findChild<QLabel*>("text-content-literal-status");check(label,"Original batch Text error label");w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();const auto observed=w.host.session;const auto status=label?label->text():QString{};const bool pending_preserved=editor&&editor->toPlainText()==QString::fromStdString(pending);write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Original batch Text Apply Window rendered");auto* scroll=w.findChild<QScrollArea*>("inspector-scroll");check(scroll&&label&&producer,"Actual wholeWindow native Inspector viewport and original producers survive refusal");const auto label_rect=label->rect(),label_visible=label->visibleRegion().boundingRect();const bool full_label=!label_rect.isEmpty()&&label_visible.contains(label_rect);QPointer<QPushButton> cancel=control<QPushButton>(w,"text-content-literal-cancel");const bool full_actions=producer->visibleRegion().boundingRect().contains(producer->rect())&&cancel->visibleRegion().boundingRect().contains(cancel->rect());QJsonArray groups;for(auto* g:scroll->widget()->findChildren<QGroupBox*>()){groups.append(QJsonObject{{"name",g->objectName()},{"title",g->title()},{"width",g->width()},{"minimum_width",g->minimumWidth()},{"minimum_hint_width",g->minimumSizeHint().width()},{"hint_width",g->sizeHint().width()}});}const auto point=label->mapTo(&w,QPoint{});check(label->parentWidget()->grab().save(path+"-panel.png"),"Actual original Text panel image; no forced layout or post-refusal scrolling");QJsonObject metadata{{"fully_visible",full_label},{"actions_fully_visible",full_actions},{"label_x",point.x()},{"label_y",point.y()},{"label_width",label_rect.width()},{"label_height",label_rect.height()},{"visible_width",label_visible.width()},{"visible_height",label_visible.height()},{"viewport_width",scroll->viewport()->width()},{"widget_width",scroll->widget()->width()},{"horizontal_scroll_max",scroll->horizontalScrollBar()->maximum()},{"groups",groups},{"signal_count",emitted.count()},{"status",status},{"pending_preserved",pending_preserved},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())}};QFile state(path+"-producer.json");check(state.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original batch Text immutable producer readback");state.write(QJsonDocument(metadata).toJson());state.close();std::cout<<"TEXT_CONTENT_VIEWPORT mode="<<mode<<" status="<<status.toStdString()<<" pending="<<pending_preserved<<" doc_equal="<<(observed.document()==(success?goal.document():caller.document()))<<" history_equal="<<(observed.history()==(success?goal.history():caller.history()))<<"\n";same(w.host.session,success?goal:caller);check(emitted.count()==1,"One original batch Text Apply");check(success?status.isEmpty():(pending_preserved&&status=="PROPERTY_CONFLICT: The retained Text content changed; refresh before applying the draft"),"Exact current atomic full source edit or stale batch refusal retaining pending draft");std::cout<<"TEXT_CONTENT_VIEWPORT full_label="<<full_label<<" full_actions="<<full_actions<<" hscroll="<<scroll->horizontalScrollBar()->maximum()<<" viewport="<<scroll->viewport()->width()<<" widget="<<scroll->widget()->width()<<"\n";check(full_label&&full_actions&&scroll->horizontalScrollBar()->maximum()==0,"Original wholeWindow Text refusal and complete action labels fit actual Inspector viewport with pending/fullcaller exact");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full raw Text/caller/native saved; pending before Save callback only");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" Qt editor input/Apply; physical/font repertoire NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_CONTENT_VIEWPORT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=1;mode<2;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text content viewport cases failed: "+std::to_string(failures));std::cout<<"PASS Text content viewport "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text content viewport "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
