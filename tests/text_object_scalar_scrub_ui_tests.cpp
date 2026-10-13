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

struct MouseCounts:QObject{QPointer<QWidget> target;int press=0,move=0,release=0;bool eventFilter(QObject* watched,QEvent* e)override{if(watched==target){if(e->type()==QEvent::MouseButtonPress)++press;if(e->type()==QEvent::MouseMove)++move;if(e->type()==QEvent::MouseButtonRelease)++release;}return false;}};
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods=Qt::NoModifier){check(c,"Current scrub actor alive");const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,mods);QApplication::sendEvent(c,&e);events();}


void open_object_row(Window& w,bool multi,const Ref& target){
 if(matrix_field(target.field)||(!multi&&(target.field=="transform.tx"||target.field=="transform.ty"))){auto* toggle=control<QPushButton>(w,multi?"batch-matrix-toggle":"transform-matrix-toggle");if(!toggle->isChecked())toggle->click();events();check(toggle->isChecked(),"Actual necessary Single/Multi matrix expanded via Qt button");}
 QPointer<QLineEdit> ordinary;for(auto* input:w.findChildren<QLineEdit*>()){const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();if(input->isVisible()&&ref.value("object")==QString::fromStdString(target.object)&&ref.value("point")==""&&ref.value("field")==QString::fromStdString(target.field))ordinary=input;}
 check(ordinary,"Original exact affine component numeric input visible BEFORE drag lookup");reveal(ordinary);check(!ordinary->isReadOnly()&&ordinary->text()==(multi?QString{}:QString::number(nect::property(w.host.session.document(),target).literal,'g',17)),"Original Single/Mixed affine input editable and exact");
}
void retained_raw_flow(const QString& root,int mode){
 const Ref target{"first","",std::vector<std::string>{"transform.tx","transform.ty","transform.a","composite.opacity","transform.anchor_x","transform.anchor_y","transform.tx","composite.opacity","transform.tx","composite.opacity"}.at(mode)};const bool multi=mode<4?mode>=2:mode%2==1;auto initial=fixture();if(mode>=8){caller_group_replacement(initial,"first");if(multi)caller_group_replacement(initial,"second");}const QString initial_text=multi?QString{}:QString::number(coordinate(initial,target).literal,'g',17);valid(initial);
 auto incoming=initial;const Ref changed{multi?"second":"first","",target.field};auto& raw=coordinate(incoming,changed);if(mode<4){if(mode%2==0)raw.literal+=0.5;else raw.expression=Expression{QString::number(coordinate(initial,changed).literal,'g',17).toStdString(),1};}else if(mode<6)incoming.objects.at(changed.object).text->id=changed.object+"-replacement-text-ID";else if(mode<8)caller_group_replacement(incoming,changed.object);else caller_text_replacement(incoming,changed.object);valid(incoming);
 Session base(initial),caller(incoming),unchanged=caller;same(caller,unchanged);
 const auto prefix=root+"/mode-"+QString::number(mode<8?mode+18:mode+20);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",incoming);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);if(multi)w.canvas->set_selections({{"first",""},{"second",""}});else w.canvas->set_selection("first","");events();
 auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock");dock->show();dock->raise();events();open_object_row(w,multi,target);
 QPointer<SemanticScrub> actor;for(auto* button:w.findChildren<QToolButton*>()){const auto ref=QJsonDocument::fromJson(button->property("nect-reference").toByteArray()).object();if(button->isVisible()&&button->objectName().startsWith("semantic-scrub-")&&ref.value("object")=="first"&&ref.value("point")==""&&ref.value("field")==QString::fromStdString(target.field))actor=dynamic_cast<SemanticScrub*>(button);}
 check(actor,"Current original Text Object scalar scrub actor exists");reveal(actor);QPointer<QLineEdit> input=actor->parentWidget()->findChild<QLineEdit*>();check(input&&input->text()==initial_text,"Original exact Text Object scalar producer");const auto selection=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
 check(actor->parentWidget()->grab().save(prefix+"-row.png")&&w.grab().save(prefix+"-before-window.png"),"Original refusal row and Window captured");
 MouseCounts counts;counts.target=actor;actor->installEventFilter(&counts);w.host.session=caller;
 mouse(actor,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(actor,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);mouse(actor,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);
 same(w.host.session,caller);check(actor&&input&&!input->isModified()&&input->text()==initial_text&&counts.press==1&&counts.move==1&&counts.release==1,"Refusal retains original actor/input without new gesture");check(w.statusBar()->currentMessage().startsWith("PROPERTY_CONFLICT: The retained "+QString::fromStdString(owner_kind=="text"?"Text":"Group")+" ")&&w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Exact raw coordinate conflict and full view retained");
 write(prefix+"-observed.nect",w.host.session.document());check(w.grab().save(prefix+"-window.png"),"Full refused Window captured");
 QFile producer(prefix+"-producer.json");check(producer.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable retained raw point actor readback");QJsonObject state{{"born_kind",QString::fromStdString(owner_kind)},{"mode",mode<8?mode+18:mode+20},{"multi",multi},{"changed_object",QString::fromStdString(changed.object)},{"field",QString::fromStdString(target.field)},{"cause",mode<4?(mode%2==0?"literal-replaced":"same-evaluated-expression-replaced"):(mode<6?(multi?"second-text-ID-replaced":"first-text-ID-replaced"):(multi?"second-kind-replaced":"first-kind-replaced"))},{"actor_alive",bool(actor)},{"input_alive",bool(input)},{"input_text",input->text()},{"input_modified",input->isModified()},{"mouse_press_count",counts.press},{"mouse_move_count",counts.move},{"mouse_release_count",counts.release},{"document_equal",w.host.session.document()==caller.document()},{"preview_equal",w.host.session.preview_document()==caller.preview_document()},{"history_equal",w.host.session.history()==caller.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()},{"view_equal",w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard},{"status",w.statusBar()->currentMessage()}};producer.write(QJsonDocument(state).toJson());producer.close();
 w.host.save(prefix+"-saved.nect");w.host.flush();same(w.host.session,caller);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Refused raw point target/fullsource saved without authoring");
}
void bound_flow(const QString& root,int mode){
 const bool upper=mode==26;const Ref first{"first","","composite.opacity"},second{"second","","composite.opacity"};auto initial=fixture();valid(initial);check(property_unit(first)=="scalar","Normalized opacity unit BEFORE Window");
 Session base(initial),preview(initial);preview.begin_gesture(0);auto retained=preview;bool rejected=false;try{preview.update_gesture({EditProperties{{first},upper?0.75:-0.25,false},EditProperties{{second},upper?1.25:0.25,false}});}catch(const Error& e){rejected=e.code=="OUT_OF_RANGE";}check(rejected,"Independent native Multi opacity bounds reject BEFORE Window");same(preview,retained);auto committed=retained;committed.commit_gesture();check(committed.document()==initial&&committed.revision()==0&&committed.history()==base.history(),"Independent rejected preview commits no History BEFORE Window");
 const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",initial);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selections({{"first",""},{"second",""}});events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock");dock->show();dock->raise();events();open_object_row(w,true,first);const auto selection=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();check(w.grab().save(prefix+"-before-window.png"),"Actual bounds Window before drag");
 QPointer<SemanticScrub> actor;for(auto* button:w.findChildren<QToolButton*>()){const auto ref=QJsonDocument::fromJson(button->property("nect-reference").toByteArray()).object();if(button->isVisible()&&button->objectName().startsWith("semantic-scrub-")&&ref.value("object")=="first"&&ref.value("point")==""&&ref.value("field")=="composite.opacity")actor=dynamic_cast<SemanticScrub*>(button);}check(actor,"REQ42 existing Text Object scalar bounds drag row missing on actual Window");reveal(actor);QPointer<QLineEdit> input=actor->parentWidget()->findChild<QLineEdit*>();check(input&&input->text().isEmpty(),"Original mixed opacity field");check(actor->parentWidget()->grab().save(prefix+"-row.png"),"Original bounds row PNG");MouseCounts counts;counts.target=actor;actor->installEventFilter(&counts);
 mouse(actor,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(actor,QEvent::MouseMove,upper?62:-38,Qt::NoButton,Qt::LeftButton);same(w.host.session,retained);check(actor&&input&&!input->isModified()&&input->text().isEmpty()&&counts.press==1&&counts.move==1&&counts.release==0,"Original bounds actor/input before release, no partial target");check(w.statusBar()->currentMessage()=="OUT_OF_RANGE: Compositing opacity must be in [0,1]"&&w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Exact bound error and view before release");check(w.grab().save(prefix+"-window.png"),"Full rejected active bounds Window");
 QFile producer(prefix+"-producer.json");check(producer.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable actor evidence BEFORE release/Save");QJsonObject state{{"born_kind",QString::fromStdString(owner_kind)},{"mode",mode},{"field","composite.opacity"},{"bound",upper?"upper":"lower"},{"actor_alive",bool(actor)},{"input_alive",bool(input)},{"input_text",input->text()},{"input_modified",input->isModified()},{"mouse_press_count",counts.press},{"mouse_move_count",counts.move},{"mouse_release_count",counts.release},{"document_equal",w.host.session.document()==initial},{"preview_equal",w.host.session.preview_document()==initial},{"history_equal",w.host.session.history()==base.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()},{"view_equal",w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard},{"status",w.statusBar()->currentMessage()}};producer.write(QJsonDocument(state).toJson());producer.close();
 mouse(actor,QEvent::MouseButtonRelease,upper?62:-38,Qt::LeftButton,Qt::NoButton);same(w.host.session,committed);write(prefix+"-observed.nect",w.host.session.document());w.host.save(prefix+"-saved.nect");w.host.flush();same(w.host.session,committed);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Bounds Save AFTER release preserves full initial native");
}
void flow(const QString& root,int mode){
 if(mode>=28){retained_raw_flow(root,mode-20);return;}
 if(mode>=26){bound_flow(root,mode);return;}
 if(mode>=18){retained_raw_flow(root,mode-18);return;}
 const bool multi=mode%2==1;const std::string field=object_fields.at(mode/2);const Ref target{"first","",field},second{"second","",field};const double step=matrix_field(field)||field=="composite.opacity"?0.01:1;const bool modified=field=="transform.tx"||field=="transform.a"||field=="transform.anchor_x"||field=="composite.opacity";const auto mods=modified?(multi?Qt::ControlModifier:Qt::ShiftModifier):Qt::NoModifier;const double delta=step*(modified?(multi?10:0.1):1);
 auto initial=fixture();const double first_value=coordinate(initial,target).literal,second_value=coordinate(initial,second).literal;const auto sibling=field=="transform.ty"?"transform.tx":"transform.ty";coordinate(initial,{"first","",sibling}).binding=Binding{{"source","",sibling},1.25,0.5};coordinate(initial,{"second","",sibling}).expression=Expression{"10+4",1};valid(initial);check(property_unit(target)==(matrix_field(field)||field=="composite.opacity"?"scalar":"du"),"Existing typed distance unit BEFORE Window");
 auto goal_doc=initial;coordinate(goal_doc,target).literal=first_value+delta;if(multi)coordinate(goal_doc,second).literal=second_value+delta;valid(goal_doc);
 std::vector<Command> commands{EditProperties{{target},first_value+delta,false}};if(multi)commands.push_back(EditProperties{{second},second_value+delta,false});
 Session base(initial),preview(initial),expected(initial);preview.begin_gesture(0);preview.update_gesture(commands);check(preview.document()==initial&&preview.preview_document()==goal_doc,"Independent full manual preview BEFORE Window");expected=preview;expected.commit_gesture();check(expected.document()==goal_doc&&expected.revision()==1&&expected.history().states.size()==2,"Canonical oneHistory/full target BEFORE Window");
 auto cancel_doc=goal_doc;coordinate(cancel_doc,target).literal+=step;if(multi)coordinate(cancel_doc,second).literal+=step;valid(cancel_doc);std::vector<Command> cancel_commands{EditProperties{{target},first_value+delta+step,false}};if(multi)cancel_commands.push_back(EditProperties{{second},second_value+delta+step,false});auto cancel_preview=expected;cancel_preview.begin_gesture(1);cancel_preview.update_gesture(cancel_commands);check(cancel_preview.preview_document()==cancel_doc,"Independent full cancel preview BEFORE Window");auto cancelled=cancel_preview;cancelled.cancel_gesture();auto undone=cancelled;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==goal_doc&&cancelled.history()==expected.history(),"Full cancel/UndoRedo oracle BEFORE Window");
 const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",goal_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);if(multi)w.canvas->set_selections({{"first",""},{"second",""}});else w.canvas->set_selection("first","");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock");dock->show();dock->raise();events();open_object_row(w,multi,target);const auto selections=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
 check(w.grab().save(prefix+"-before-window.png"),"Actual current Path Text Object scalar Window");const auto find_actor=[&]()->SemanticScrub*{for(auto* button:w.findChildren<QToolButton*>()){const auto ref=QJsonDocument::fromJson(button->property("nect-reference").toByteArray()).object();if(button->isVisible()&&button->objectName().startsWith("semantic-scrub-")&&ref.value("object")=="first"&&ref.value("point")==""&&ref.value("field")==QString::fromStdString(field))return dynamic_cast<SemanticScrub*>(button);}return nullptr;};QPointer<SemanticScrub> actor=find_actor();check(actor,"REQ42 existing Path Text Object scalar drag row missing on actual Window");reveal(actor);
 QJsonArray target_refs{QJsonObject{{"object","first"},{"point",""},{"field",QString::fromStdString(target.field)}}};if(multi)target_refs.append(QJsonObject{{"object","second"},{"point",""},{"field",QString::fromStdString(second.field)}});check(QJsonDocument::fromJson(actor->property("nect-targets").toByteArray()).array()==target_refs,"Exact frozen Single/Multi Text Object scalar Ref set");QPointer<QLineEdit> input=actor->parentWidget()->findChild<QLineEdit*>();check(input&&input->text()==(multi?QString{}:QString::number(first_value,'g',17))&&input->property("nect-mixed").toBool()==multi&&!input->isReadOnly(),"Original numeric/mixed presentation preserved");
 MouseCounts counts;counts.target=actor;actor->installEventFilter(&counts);check(actor->parentWidget()->grab().save(prefix+"-row.png"),"Actual new Text Object scalar row PNG");mouse(actor,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(actor,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton,mods);same(w.host.session,preview);mouse(actor,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);same(w.host.session,expected);check(counts.press==counts.move&&counts.move==counts.release&&counts.press==1,"Original press/move/release once");check(w.statusBar()->currentMessage().isEmpty()&&w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Successful shared gesture/view/error state");
 write(prefix+"-observed.nect",w.host.session.document());check(w.grab().save(prefix+"-window.png"),"Actual postgesture Window PNG");
 QPointer<SemanticScrub> fresh=find_actor();reveal(fresh);mouse(fresh,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(fresh,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);same(w.host.session,cancel_preview);QTest::keyClick(fresh,Qt::Key_Escape);events();same(w.host.session,cancelled);w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.host.edited();events();
 QPointer<SemanticScrub> current=find_actor();reveal(current);QPointer<QLineEdit> pending=current->parentWidget()->findChild<QLineEdit*>();check(pending,"Current exact Text Object scalar numeric field");pending->setFocus();pending->selectAll();QTest::keyClicks(pending,"40.5");events();check(pending&&pending->text()=="40.5"&&pending->isModified(),"Uncommitted valid Text Object scalar draft AFTER normalgesture");same(w.host.session,redone);
 QFile producer(prefix+"-producer.json");check(producer.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable Text Object scalar actor readback");QJsonObject state{{"born_kind",QString::fromStdString(owner_kind)},{"mode",mode},{"field",QString::fromStdString(field)},{"multi",multi},{"targets",target_refs},{"delta",delta},{"modifier",modified?(multi?"Control":"Shift"):"None"},{"mouse_press_count",counts.press},{"mouse_move_count",counts.move},{"mouse_release_count",counts.release},{"owned_cancel",true},{"undo_redo_full_session",true},{"pending_input_alive",bool(pending)},{"pending_input_text",pending->text()},{"pending_input_modified",pending->isModified()},{"document_equal",w.host.session.document()==redone.document()},{"history_equal",w.host.session.history()==redone.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"view_equal",w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard}};producer.write(QJsonDocument(state).toJson());producer.close();
 w.host.save(prefix+"-saved.nect");w.host.flush();QFile post(prefix+"-post-save-producer.json");check(post.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable postSave Text Object scalar fullSession readback");QJsonObject post_state{{"document_equal",w.host.session.document()==redone.document()},{"preview_equal",w.host.session.preview_document()==redone.preview_document()},{"history_equal",w.host.session.history()==redone.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()}};post.write(QJsonDocument(post_state).toJson());post.close();same(w.host.session,redone);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Actual Save committed Text Object scalar, never implicit pending40.5 AFTERactors");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{
 std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch");const auto requested=qEnvironmentVariable("NECT_TEXT_OBJECT_SCALAR_SCRUB_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;
 for(int mode=0;mode<30;++mode){owner_kind=mode>=28?"group":"text";try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
 if(failures)throw std::runtime_error("Independent Text Object Scalar failures: "+std::to_string(failures));std::cout<<"PASS Text Object scalar scrub "<<checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL Text Object scalar scrub "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
