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
 const bool expression=mode==1||mode==3,success=mode>=3;const std::string name=mode==0||mode==4?"font_size":mode==2?"frame_width":"tracking",field="text."+name;const QString draft=expression?"=4.25":"+=0.5";
 auto initial=fixture(true);for(const auto& key:std::vector<std::string>{"font_size","tracking","frame_width"}){auto& scalar=initial.objects.at("first").text->parameters.at(key);scalar.binding.reset();scalar.expression.reset();}initial.objects.at("first").text->parameters.at("font_size").literal=20.25;initial.objects.at("first").text->parameters.at("tracking").literal=2.25;initial.objects.at("first").text->parameters.at("frame_width").literal=271.25;valid(initial);Session base(initial);auto caller_doc=initial;if(!success){auto& changed=caller_doc.objects.at("first").text->parameters.at(name);auto canonical=base;if(mode==1){changed.expression=Expression{"2.25",1};canonical.apply({SetExpression{{{"first","",field}},{"2.25",1},false}},0);check(evaluate(caller_doc).at({"first","",field})==2.25,"Independent sameevaluated SingleTracking Expression sourcekind BEFOREWindow");}else{changed.literal+=7;canonical.apply({Set{{"first","",field},changed.literal}},0);}check(canonical.document()==caller_doc,"Independent fullsameSourceID Singleproperty caller agreescanonical Set/SetExpression BEFOREWindow");}check(caller_doc.objects.at("first").text->id==initial.objects.at("first").text->id,"Same internalTextSource ID isolatesSingleproperty context BEFOREWindow");
 valid(caller_doc);Session caller(caller_doc);std::vector<Ref> refs{{"first","",field}};auto intended=caller_doc;for(const auto& id:std::vector<Id>{"first"}){auto& scalar=intended.objects.at(id).text->parameters.at(name);if(expression)scalar.expression=Expression{"4.25",1};else scalar.literal+=0.5;}valid(intended);auto goal=caller;if(expression)goal.apply({SetExpression{refs,{"4.25",1},false}},0);else goal.apply({EditProperties{refs,0.5,true}},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent full manually intended Scalar relative/Expression equalscanonical oneHistory BEFOREWindow");auto undone=goal;undone.undo(1);check(undone.document()==caller_doc,"Independent exactfull caller/fallback/sourceIDs/native Undo BEFOREWindow");auto redone=undone;redone.redo(2);check(redone.document()==intended,"Independent fullintended Scalar native Redo BEFOREWindow");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?goal.document():caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("first");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original SingleText Property dock");dock->show();dock->raise();events();QPointer<QLineEdit> input;
 for(auto* c:w.findChildren<QLineEdit*>()){if(!c->isVisible())continue;const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()==QString::fromStdString(field))input=c;}
 check(input,"Exact original SingleText typedScalar Ref control");const auto targets=QJsonDocument::fromJson(input->property("nect-targets").toByteArray()).array();check(targets.size()==1&&targets[0].toObject()["object"].toString()=="first"&&targets[0].toObject()["point"].toString().isEmpty()&&targets[0].toObject()["field"].toString()==QString::fromStdString(field),"Original exactcanonical SingleText typedScalar target");reveal(input);input->setFocus(Qt::OtherFocusReason);events();check(input,"Original commonText draft survivesfocus");QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,draft);check(input&&input->text()==draft&&input->isModified(),"Exactprivate original numeric/Expression draft beforecallerreplacement");same(w.host.session,base);w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(input,&QLineEdit::editingFinished);QTest::keyClick(input,Qt::Key_Return);events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();const bool pending=input&&input->text()==draft;write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actualproductstyled originalSingleText scalar Window rendered");QJsonObject state{{"field",QString::fromStdString(field)},{"expression",expression},{"signal_count",emitted.count()},{"status",status},{"pending",pending},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable originalinlineScalar producer readback");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"TEXT_SINGLE_SCALAR_PROPERTY mode="<<mode<<" status="<<status.toStdString()<<" doc_equal="<<state["document_equal"].toBool()<<" pending="<<pending<<"\n";same(w.host.session,success?goal:caller);check(emitted.count()==1,"One original inlineScalar editingFinished producer");check(success?status.isEmpty():(pending&&status=="PROPERTY_CONFLICT: The retained Text property changed; reopen this Inspector"),"Current scalar relative/Expression usability or exactstale source refusal withpendingdraft retained");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable complete fullScalar/caller native save; pending beforeSave only");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt originalTextScalarEnter; physical/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_SINGLE_SCALAR_PROPERTY_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<5;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Single Text scalar property cases failed: "+std::to_string(failures));std::cout<<"PASS Single Text scalar property "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Single Text scalar property "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
