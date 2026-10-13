#include "window.hpp"
#include "visual_style.hpp"
#include "semantic_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDir>
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
int checks=0;std::string owner_kind;
const std::vector<std::string> object_fields{"transform.tx","transform.ty","transform.a","transform.b","transform.c","transform.d","transform.anchor_x","transform.anchor_y","composite.opacity"};
bool matrix_field(const std::string& field){return field=="transform.a"||field=="transform.b"||field=="transform.c"||field=="transform.d";}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full caller native-valid BEFORE Window");}
void same(const Session& a,const Session& b){check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Entire caller Session/native/History/preview/revision/generation unchanged");}
void write(const QString& path,const Document& d){QFile f(path);const auto b=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(b.data(),b.size())==static_cast<qint64>(b.size()),"Immutable actual or independent native retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error("Original visible source producer missing");}
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(alive&&alive->isVisible()&&alive->isEnabled(),"Original source producer survives reveal and is enabled");}

Scalar& coordinate(Document& document,const Ref& ref){
 auto& object=document.objects.at(ref.object);
 if(!ref.point.empty())throw std::runtime_error("Exact empty-point Object Scalar Ref");
 if(ref.field=="composite.opacity")return object.compositing.opacity;
 if(ref.field=="transform.anchor_x"||ref.field=="transform.anchor_y")return object.anchor.at(ref.field=="transform.anchor_x"?0:1);
 const std::vector<std::string> fields{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty"};
 const auto found=std::find(fields.begin(),fields.end(),ref.field);if(found==fields.end())throw std::runtime_error("Exact independent Object Scalar Ref");
 return object.transform.at(static_cast<std::size_t>(found-fields.begin()));
}
const std::string shared_content="  Literal 日本語\n\tCafé e\xcc\x81 🐈\n\n";
const std::string changed_content="\n  Replacement 日本語\n\tCafé e\xcc\x81 🐈  \n";
const std::vector<Id> pair={"first","second"};

Document text_fixture(bool mixed=false,bool driven=false){
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





Document fixture(){
 auto d=text_fixture();
 d.objects.at("first").text->content="Retained Text-on-Path 日本語";d.objects.at("first").text->layout="auto";
 d.objects.at("first").text->path_attachment=TextPathAttachment{"shape","contour","distance",1000,2,false};
 for(const auto& id:std::vector<Id>{"source","first","second"})d.objects.at(id).stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
 d.objects.at("source").transform[0].literal=1.5;d.objects.at("source").transform[1].literal=0.125;d.objects.at("source").transform[2].literal=0.25;d.objects.at("source").transform[3].literal=0.75;d.objects.at("source").transform[4].literal=20.25;d.objects.at("source").transform[5].literal=-8.75;d.objects.at("source").anchor[0].literal=20.25;d.objects.at("source").anchor[1].literal=-8.75;d.objects.at("source").compositing.opacity.literal=.35;
 d.objects.at("first").transform[0].literal=1.125;d.objects.at("first").transform[1].literal=-.25;d.objects.at("first").transform[2].literal=.375;d.objects.at("first").transform[3].literal=.875;d.objects.at("first").transform[4].literal=-30.25;d.objects.at("first").transform[5].literal=-30.25;d.objects.at("first").anchor[0].literal=-30.25;d.objects.at("first").anchor[1].literal=-30.25;d.objects.at("first").compositing.opacity.literal=.25;
 d.objects.at("second").transform[0].literal=2.25;d.objects.at("second").transform[1].literal=-.75;d.objects.at("second").transform[2].literal=.625;d.objects.at("second").transform[3].literal=1.75;d.objects.at("second").transform[4].literal=77.25;d.objects.at("second").transform[5].literal=77.25;d.objects.at("second").anchor[0].literal=77.25;d.objects.at("second").anchor[1].literal=77.25;d.objects.at("second").compositing.opacity.literal=.75;

 return d;
}
void caller_group_replacement(Document& d,const Id& id){auto& object=d.objects.at(id);object.kind=Kind::group;object.text.reset();object.source.reset();object.contours.clear();object.stack.clear();}
void caller_text_replacement(Document& d,const Id& id){auto& object=d.objects.at(id);object.kind=Kind::text;object.text=default_text(id+"-replacement-text","Replacement 日本語");object.source.reset();object.children.clear();object.contours.clear();object.stack.clear();}


void flow(const QString& root,int mode){
 const bool multi=mode%2==1,success=mode>=4,expression=mode==1||mode==5,relative=mode==4||mode==7;const std::string field=multi?"composite.opacity":"transform.tx";const Ref first{"first","",field},second{"second","",field};const std::vector<Ref> refs=multi?std::vector<Ref>{first,second}:std::vector<Ref>{first};const QString draft=expression?"=0.5":mode==4?"+=1":mode==7?"+=0.01":multi?"0.5":"64.25";
 auto initial=fixture();valid(initial);Session base(initial);auto caller_doc=initial;
 if(mode==0||mode==1)caller_doc.objects.at(multi?"second":"first").text->id="replacement-input-text-ID";
 if(mode==2)coordinate(caller_doc,first).literal+=5;if(mode==3)coordinate(caller_doc,second).literal=.625;
 if(mode==6)caller_doc.objects.at("second").text->parameters.at("frame_width").literal+=10;
 if(mode==7)caller_doc.objects.at("source").text->parameters.at("font_size").literal=32;
 valid(caller_doc);Session caller(caller_doc);auto intended=caller_doc;
 for(const auto& ref:refs){auto& value=coordinate(intended,ref);if(expression)value.expression=Expression{"0.5",1};else value.literal=relative?value.literal+(multi?.01:1):multi?.5:64.25;}
 valid(intended);auto goal=caller;if(expression)goal.apply({SetExpression{refs,{"0.5",1},false}},0);else goal.apply({EditProperties{refs,relative?(multi?.01:1):multi?.5:64.25,relative}},0);
 check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent full manual numeric/Expression oneHistory oracle BEFORE Window");auto undone=goal;undone.undo(1);auto undo_history=goal.history();undo_history.current_id=caller.history().current_id;check(undone.document()==caller_doc&&undone.history()==undo_history&&undone.revision()==2&&!undone.gesture_active(),"Independent full Undo oracle BEFORE Window");auto redone=undone;redone.redo(2);check(redone.document()==intended&&redone.history()==goal.history()&&redone.revision()==3&&!redone.gesture_active(),"Independent full Redo oracle BEFORE Window");
 const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-expected.nect",success?intended:caller_doc);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);const std::vector<Canvas::Selection> selected=multi?std::vector<Canvas::Selection>{{"first",{}},{"second",{}}}:std::vector<Canvas::Selection>{{"first",{}}};w.canvas->set_selections(selected);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original Properties dock");dock->show();dock->raise();events();
 if(!multi){auto* toggle=control<QPushButton>(w,"transform-matrix-toggle");if(!toggle->isChecked())toggle->click();events();check(toggle->isChecked(),"Actual canonical tx row expanded");}
 QPointer<QLineEdit> input;for(auto* c:w.findChildren<QLineEdit*>()){const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(c->isVisible()&&ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()==QString::fromStdString(field))input=c;}check(input&&!input->isReadOnly(),"Exact original editable Object Scalar input");const auto targets=QJsonDocument::fromJson(input->property("nect-targets").toByteArray()).array();check(targets.size()==(multi?2:1)&&targets[0].toObject()["object"].toString()=="first"&&(!multi||targets[1].toObject()["object"].toString()=="second"),"Original frozen ordered exact Single/Multi Ref targets");reveal(input);check(input->parentWidget()->grab().save(prefix+"-row.png"),"Original row before draft");input->setFocus(Qt::OtherFocusReason);events();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,draft);check(input&&input->text()==draft&&input->isModified(),"Actual private draft before native-valid caller replacement");same(w.host.session,base);w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(input,&QLineEdit::editingFinished);QTest::keyClick(w.windowHandle(),Qt::Key_Return);events();const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();const bool pending=input&&input->text()==draft;
 write(prefix+"-observed.nect",observed.document());check(w.grab().save(prefix+"-window.png"),"Actual styled Window after original Return");QJsonObject state{{"mode",mode},{"multi",multi},{"field",QString::fromStdString(field)},{"expression",expression},{"relative",relative},{"signal_count",emitted.count()},{"status",status},{"pending",pending},{"input_alive",bool(input)},{"input_text",input?input->text():QString{}},{"draft",draft},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())},{"view_equal",w.canvas->selections()==selected&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="artboard"}};QFile producer(prefix+"-producer.json");check(producer.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable original input readback BEFORE Save");producer.write(QJsonDocument(state).toJson());producer.close();same(w.host.session,success?goal:caller);check(emitted.count()==1&&state["view_equal"].toBool(),"One original editingFinished and exact frozen view");const QString error=mode<2?"TEXT_EDIT_CONFLICT: The retained Text property source was replaced; reopen this Inspector":"PROPERTY_CONFLICT: The retained Text property changed; reopen this Inspector";check(success?status.isEmpty():(pending&&status==error),"Current numeric/Expression usability or exact refusal with pending draft retained");
 w.host.save(prefix+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Full native Save retains intended or refused caller state");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" original Qt Text Object input/Return; physical/clipboard/fonts/geometry NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch");const auto requested=qEnvironmentVariable("NECT_TEXT_OBJECT_INPUT_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text Object input context failures: "+std::to_string(failures));std::cout<<"PASS Text Object input context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text Object input context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
