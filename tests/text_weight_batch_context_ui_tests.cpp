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
 auto initial=fixture(true);initial.objects.at("first").text->weight_expression.reset();initial.objects.at("first").text->weight_driver.reset();initial.objects.at("first").text->weight=500;initial.objects.at("second").text->weight_expression.reset();initial.objects.at("second").text->weight_driver.reset();initial.objects.at("second").text->weight=650;valid(initial);Session base(initial);auto caller_doc=initial;
 if(mode==1)caller_doc.id="other-weight-document";
 if(mode==3){caller_doc.objects.at("second").text->weight=700;auto typed=base;typed.apply({UpdateText{"second",*caller_doc.objects.at("second").text}},0);check(typed.document()==caller_doc,"Independent complete selected native literal weight replacement agrees canonical UpdateText BEFORE Window");}
 if(mode==4){caller_doc.objects.at("second").text->parameters.at("frame_width").literal=498.1234567890123;auto typed=base;typed.apply({UpdateText{"second",*caller_doc.objects.at("second").text}},0);check(typed.document()==caller_doc,"Independent complete selected unrelated frame-only replacement agrees canonical UpdateText BEFORE Window");}
 valid(caller_doc);Session caller(caller_doc);if(mode==2){caller.begin_gesture(0);caller.update_gesture({Rename{"source","Temporary preview"}});check(caller.document()==initial&&caller.preview_document()!=initial,"Independent full gesture committed versus preview BEFORE Window");caller.cancel_gesture();check(caller.document()==initial&&caller.preview_document()==initial&&caller.history()==base.history()&&caller.revision()==0&&caller.gesture_generation()!=base.gesture_generation(),"Independent cancelledgesture retains full caller/History with changed generation BEFORE Window");}
 auto intended=caller_doc;intended.objects.at("first").text->weight+=25;intended.objects.at("second").text->weight+=25;valid(intended);std::vector<Ref> refs{{"first","","text.weight"},{"second","","text.weight"}};auto goal=caller;goal.apply({TextWeightBatch{TextWeightBatchMode::edit,refs,25,{},true,false}},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent complete manually intended relative +25 preserves target differences/all source fields/native agrees canonical oneHistory TextWeightBatch BEFORE Window");auto undone=goal;undone.undo(1);check(undone.document()==caller_doc,"Independent complete original source/raw/weight/native Undo BEFORE Window");auto redone=undone;redone.redo(2);check(redone.document()==intended,"Independent full intended relative-weight source/raw Redo BEFORE Window");const bool success=mode==0||mode==4;
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?goal.document():caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections({{"first",{}},{"second",{}}});events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual original weight batch Property dock");dock->show();dock->raise();events();QPointer<QLineEdit> input=control<QLineEdit>(w,"text-weight-batch-value");reveal(input);input->setFocus(Qt::OtherFocusReason);events();check(input,"Original weight draft survives focus");QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=25");check(input&&input->text()=="+=25","Original Qt keyboard relative numeric draft");same(w.host.session,base);QPointer<QPushButton> producer=control<QPushButton>(w,"text-weight-batch-apply");reveal(producer);QPointer<QLabel> label=producer->parentWidget()->findChild<QLabel*>("text-weight-batch-status");check(label,"Original weight batch local status");w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();const auto observed=w.host.session;const auto status=label?label->text():QString{};const bool pending_preserved=input&&input->text()=="+=25";write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actual product-styled original weight batch Apply rendered");QJsonObject metadata{{"signal_count",emitted.count()},{"status",status},{"pending_preserved",pending_preserved},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())}};QFile state(path+"-producer.json");check(state.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original weight batch immutable producer readback");state.write(QJsonDocument(metadata).toJson());state.close();std::cout<<"TEXT_WEIGHT_BATCH_CONTEXT mode="<<mode<<" status="<<status.toStdString()<<" pending="<<pending_preserved<<" doc_equal="<<(observed.document()==(success?goal.document():caller.document()))<<" history_equal="<<(observed.history()==(success?goal.history():caller.history()))<<"\n";same(w.host.session,success?goal:caller);check(emitted.count()==1,"One original weight batch Apply");const QString expected=mode==1?"SESSION_CONFLICT: Text weight draft belongs to another document":mode==2?"REVISION_CONFLICT: The edit context changed; refresh before applying Text weights":"PROPERTY_CONFLICT: The retained Text weight changed; refresh before applying the draft";check(success?label==nullptr:(pending_preserved&&status==expected),"Current panel rebuild or exact retained numeric draft/context refusal");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full Text/caller/native saved; pending before Save callback only");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt numeric input/Apply; physical/font repertoire NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_WEIGHT_BATCH_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<5;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text weight batch context cases failed: "+std::to_string(failures));std::cout<<"PASS Text weight batch context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text weight batch context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
