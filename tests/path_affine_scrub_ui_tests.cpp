#include "window.hpp"
#include "visual_style.hpp"
#include "semantic_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
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
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full caller native-valid BEFORE Window");}
void same(const Session& a,const Session& b){check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Entire caller Session/native/History/preview/revision/generation unchanged");}
void write(const QString& path,const Document& d){QFile f(path);const auto b=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(b.data(),b.size())==static_cast<qint64>(b.size()),"Immutable actual or independent native retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error("Original visible source producer missing");}
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(alive&&alive->isVisible()&&alive->isEnabled(),"Original source producer survives reveal and is enabled");}

Scalar& coordinate(Document& document,const Ref& ref){
 const std::vector<std::string> fields{"transform.a","transform.b","transform.c","transform.d"};
 const auto found=std::find(fields.begin(),fields.end(),ref.field);if(found==fields.end())throw std::runtime_error("Exact independent affine component Ref");
 return document.objects.at(ref.object).transform.at(static_cast<std::size_t>(found-fields.begin()));
}
Document fixture(){
 auto d=empty_document("path-affine-document","composition","artboard");
 Point source;source.id="source-point";source.x.literal=20.25;source.y.literal=-8.75;
 Object upstream;upstream.id="source";upstream.name="Same name";upstream.kind=Kind::path;upstream.contours={{"source-contour",false,{source}}};
 Point before;before.id="untouched-before";before.x.literal=10.1234567890123;before.y.literal=50.9876543210123;
 Point target;target.id="first-anchor";target.x.literal=-30.25;target.y.literal=-30.25;
 target.in_angle.literal=725.1234567890123;target.out_angle.literal=-725.25;target.in_length.literal=3.25;target.out_length.literal=7.125;
 Point after;after.id="untouched-after";after.x.literal=250.1234567890123;after.y.literal=100.9876543210123;
 Object first;first.id="first";first.name="Same name";first.kind=Kind::path;first.contours={{"first-contour",false,{before,target,after}}};
 Point decoy=before;decoy.id="untouched-first-contour";decoy.x.literal=100;decoy.y.literal=300;
 first.contours.insert(first.contours.begin(),Contour{"decoy-contour",false,{decoy}});
 first.stack.push_back(default_operation("first-fill","nect.paint.fill"));
 target.id="second-anchor";target.x.literal=77.25;target.y.literal=77.25;
 before.id="second-before";after.id="second-after";
 Object second;second.id="second";second.name="Same name";second.kind=Kind::path;second.contours={{"second-contour",false,{target,after,before}}};
 second.stack.push_back(default_operation("second-fill","nect.paint.fill"));
 first.transform[0].literal=1.125;first.transform[1].literal=-0.25;first.transform[2].literal=0.375;first.transform[3].literal=0.875;
 first.transform[4].literal=-30.25;first.transform[5].literal=-30.25;second.transform[4].literal=77.25;second.transform[5].literal=77.25;
 second.transform[0].literal=2.25;second.transform[1].literal=-0.75;second.transform[2].literal=0.625;second.transform[3].literal=1.75;upstream.transform[0].literal=1.5;upstream.transform[1].literal=0.125;upstream.transform[2].literal=0.25;upstream.transform[3].literal=0.75;first.anchor[0].literal=-5.125;first.anchor[1].literal=8.25;second.anchor[0].literal=11.75;second.anchor[1].literal=-15.25;upstream.transform[4].literal=20.25;upstream.transform[5].literal=-8.75;
 d.objects.emplace("source",upstream);d.objects.emplace("first",first);d.objects.emplace("second",second);
 d.compositions.front().roots={"source","first","second"};return d;
}

struct MouseCounts:QObject{QPointer<QWidget> target;int press=0,move=0,release=0;bool eventFilter(QObject* watched,QEvent* e)override{if(watched==target){if(e->type()==QEvent::MouseButtonPress)++press;if(e->type()==QEvent::MouseMove)++move;if(e->type()==QEvent::MouseButtonRelease)++release;}return false;}};
void mouse(QWidget* c,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods=Qt::NoModifier){check(c,"Current scrub actor alive");const QPointF p(x,12);QMouseEvent e(type,p,QPointF(c->mapToGlobal(QPoint{}))+p,button,buttons,mods);QApplication::sendEvent(c,&e);events();}


void open_matrix_row(Window& w,bool multi,const Ref& target){
 auto* toggle=control<QPushButton>(w,multi?"batch-matrix-toggle":"transform-matrix-toggle");if(!toggle->isChecked())toggle->click();events();check(toggle->isChecked(),"Actual Single/Multi Affine matrix expanded via Qt button");
 QPointer<QLineEdit> ordinary;for(auto* input:w.findChildren<QLineEdit*>()){const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();if(input->isVisible()&&ref.value("object")==QString::fromStdString(target.object)&&ref.value("point")==""&&ref.value("field")==QString::fromStdString(target.field))ordinary=input;}
 check(ordinary,"Original exact affine component numeric input visible BEFORE drag lookup");reveal(ordinary);check(!ordinary->isReadOnly()&&ordinary->text()==(multi?QString{}:QString::number(nect::property(w.host.session.document(),target).literal,'g',17)),"Original Single/Mixed affine input editable and exact");
}
void retained_raw_flow(const QString& root,int mode){
 const Ref target{"first","",std::vector<std::string>{"transform.a","transform.b","transform.c","transform.d"}.at(mode)};const bool multi=mode>=2;auto initial=fixture();const QString initial_text=multi?QString{}:QString::number(coordinate(initial,target).literal,'g',17);valid(initial);
 auto incoming=initial;const Ref changed{multi?"second":"first","",target.field};auto& raw=coordinate(incoming,changed);if(mode%2==0)raw.literal+=0.5;else raw.expression=Expression{multi?"1.75":"-0.25",1};valid(incoming);
 Session base(initial),caller(incoming),unchanged=caller;same(caller,unchanged);
 const auto prefix=root+"/mode-"+QString::number(mode+8);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",incoming);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);if(multi)w.canvas->set_selections({{"first",""},{"second",""}});else w.canvas->set_selection("first","");events();
 auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock");dock->show();dock->raise();events();open_matrix_row(w,multi,target);
 QPointer<SemanticScrub> actor;for(auto* button:w.findChildren<QToolButton*>()){const auto ref=QJsonDocument::fromJson(button->property("nect-reference").toByteArray()).object();if(button->isVisible()&&button->objectName().startsWith("semantic-scrub-")&&ref.value("object")=="first"&&ref.value("point")==""&&ref.value("field")==QString::fromStdString(target.field))actor=dynamic_cast<SemanticScrub*>(button);}
 check(actor,"Current original Path matrix scrub actor exists");reveal(actor);QPointer<QLineEdit> input=actor->parentWidget()->findChild<QLineEdit*>();check(input&&input->text()==initial_text,"Original exact Path matrix producer");const auto selection=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
 check(actor->parentWidget()->grab().save(prefix+"-row.png")&&w.grab().save(prefix+"-before-window.png"),"Original refusal row and Window captured");
 MouseCounts counts;counts.target=actor;actor->installEventFilter(&counts);w.host.session=caller;
 mouse(actor,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(actor,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);mouse(actor,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);
 same(w.host.session,caller);check(actor&&input&&!input->isModified()&&input->text()==initial_text&&counts.press==1&&counts.move==1&&counts.release==1,"Refusal retains original actor/input without new gesture");check(w.statusBar()->currentMessage()=="PROPERTY_CONFLICT: The retained Path matrix changed; reopen this Inspector"&&w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Exact raw coordinate conflict and full view retained");
 write(prefix+"-observed.nect",w.host.session.document());check(w.grab().save(prefix+"-window.png"),"Full refused Window captured");
 QFile producer(prefix+"-producer.json");check(producer.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable retained raw point actor readback");QJsonObject state{{"mode",mode+8},{"multi",multi},{"changed_object",QString::fromStdString(changed.object)},{"field",QString::fromStdString(target.field)},{"cause",mode%2==0?"literal-replaced":"same-evaluated-expression-replaced"},{"actor_alive",bool(actor)},{"input_alive",bool(input)},{"input_text",input->text()},{"input_modified",input->isModified()},{"mouse_press_count",counts.press},{"mouse_move_count",counts.move},{"mouse_release_count",counts.release},{"document_equal",w.host.session.document()==caller.document()},{"preview_equal",w.host.session.preview_document()==caller.preview_document()},{"history_equal",w.host.session.history()==caller.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()},{"view_equal",w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard},{"status",w.statusBar()->currentMessage()}};producer.write(QJsonDocument(state).toJson());producer.close();
 w.host.save(prefix+"-saved.nect");w.host.flush();same(w.host.session,caller);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Refused raw point target/fullsource saved without authoring");
}
void flow(const QString& root,int mode){
 if(mode>=8){retained_raw_flow(root,mode-8);return;}
 const bool multi=mode%2==1;const std::string field=std::vector<std::string>{"transform.a","transform.b","transform.c","transform.d"}.at(mode/2);const Ref target{"first","",field},second{"second","",field};const double delta=mode==0?0.001:mode==1?0.1:0.01;const auto mods=mode==0?Qt::ShiftModifier:mode==1?Qt::ControlModifier:Qt::NoModifier;
 auto initial=fixture();const double first_value=coordinate(initial,target).literal,second_value=coordinate(initial,second).literal;const auto sibling=field=="transform.a"?"transform.d":field=="transform.d"?"transform.a":field=="transform.b"?"transform.c":"transform.b";coordinate(initial,{"first","",sibling}).binding=Binding{{"source","",sibling},1.25,0.5};coordinate(initial,{"second","",sibling}).expression=Expression{"10+4",1};valid(initial);check(property_unit(target)=="scalar","Existing typed distance unit BEFORE Window");
 auto goal_doc=initial;coordinate(goal_doc,target).literal=first_value+delta;if(multi)coordinate(goal_doc,second).literal=second_value+delta;valid(goal_doc);
 std::vector<Command> commands{EditProperties{{target},first_value+delta,false}};if(multi)commands.push_back(EditProperties{{second},second_value+delta,false});
 Session base(initial),preview(initial),expected(initial);preview.begin_gesture(0);preview.update_gesture(commands);check(preview.document()==initial&&preview.preview_document()==goal_doc,"Independent full manual preview BEFORE Window");expected=preview;expected.commit_gesture();check(expected.document()==goal_doc&&expected.revision()==1&&expected.history().states.size()==2,"Canonical oneHistory/full target BEFORE Window");
 auto cancel_doc=goal_doc;coordinate(cancel_doc,target).literal+=0.01;if(multi)coordinate(cancel_doc,second).literal+=0.01;valid(cancel_doc);std::vector<Command> cancel_commands{EditProperties{{target},first_value+delta+0.01,false}};if(multi)cancel_commands.push_back(EditProperties{{second},second_value+delta+0.01,false});auto cancel_preview=expected;cancel_preview.begin_gesture(1);cancel_preview.update_gesture(cancel_commands);check(cancel_preview.preview_document()==cancel_doc,"Independent full cancel preview BEFORE Window");auto cancelled=cancel_preview;cancelled.cancel_gesture();auto undone=cancelled;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==goal_doc&&cancelled.history()==expected.history(),"Full cancel/UndoRedo oracle BEFORE Window");
 const auto prefix=root+"/mode-"+QString::number(mode);write(prefix+"-initial.nect",initial);write(prefix+"-expected.nect",goal_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(prefix+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.setStyleSheet(application_style_sheet());w.host.session=base;w.host.edited();w.resize(1400,1050);w.show();w.activateWindow();events();w.canvas->set_active_artboard("composition","artboard",false);if(multi)w.canvas->set_selections({{"first",""},{"second",""}});else w.canvas->set_selection("first","");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Properties dock");dock->show();dock->raise();events();open_matrix_row(w,multi,target);const auto selections=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
 check(w.grab().save(prefix+"-before-window.png"),"Actual current Path Path matrix Window");const auto find_actor=[&]()->SemanticScrub*{for(auto* button:w.findChildren<QToolButton*>()){const auto ref=QJsonDocument::fromJson(button->property("nect-reference").toByteArray()).object();if(button->isVisible()&&button->objectName().startsWith("semantic-scrub-")&&ref.value("object")=="first"&&ref.value("point")==""&&ref.value("field")==QString::fromStdString(field))return dynamic_cast<SemanticScrub*>(button);}return nullptr;};QPointer<SemanticScrub> actor=find_actor();check(actor,"REQ42 existing Path Path matrix drag row missing on actual Window");reveal(actor);
 QJsonArray target_refs{QJsonObject{{"object","first"},{"point",""},{"field",QString::fromStdString(target.field)}}};if(multi)target_refs.append(QJsonObject{{"object","second"},{"point",""},{"field",QString::fromStdString(second.field)}});check(QJsonDocument::fromJson(actor->property("nect-targets").toByteArray()).array()==target_refs,"Exact frozen Single/Multi Path matrix Ref set");QPointer<QLineEdit> input=actor->parentWidget()->findChild<QLineEdit*>();check(input&&input->text()==(multi?QString{}:QString::number(first_value,'g',17))&&input->property("nect-mixed").toBool()==multi&&!input->isReadOnly(),"Original numeric/mixed presentation preserved");
 MouseCounts counts;counts.target=actor;actor->installEventFilter(&counts);check(actor->parentWidget()->grab().save(prefix+"-row.png"),"Actual new Path matrix row PNG");mouse(actor,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(actor,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton,mods);same(w.host.session,preview);mouse(actor,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);same(w.host.session,expected);check(counts.press==counts.move&&counts.move==counts.release&&counts.press==1,"Original press/move/release once");check(w.statusBar()->currentMessage().isEmpty()&&w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Successful shared gesture/view/error state");
 write(prefix+"-observed.nect",w.host.session.document());check(w.grab().save(prefix+"-window.png"),"Actual postgesture Window PNG");
 QPointer<SemanticScrub> fresh=find_actor();reveal(fresh);mouse(fresh,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(fresh,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);same(w.host.session,cancel_preview);QTest::keyClick(fresh,Qt::Key_Escape);events();same(w.host.session,cancelled);w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.host.edited();events();
 QPointer<SemanticScrub> current=find_actor();reveal(current);QPointer<QLineEdit> pending=current->parentWidget()->findChild<QLineEdit*>();check(pending,"Current exact Path matrix numeric field");pending->setFocus();pending->selectAll();QTest::keyClicks(pending,"40.5");events();check(pending&&pending->text()=="40.5"&&pending->isModified(),"Uncommitted valid Path matrix draft AFTER normalgesture");same(w.host.session,redone);
 QFile producer(prefix+"-producer.json");check(producer.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable Path matrix actor readback");QJsonObject state{{"mode",mode},{"field",QString::fromStdString(field)},{"multi",multi},{"targets",target_refs},{"delta",delta},{"modifier",mode==0?"Shift":mode==1?"Control":"None"},{"mouse_press_count",counts.press},{"mouse_move_count",counts.move},{"mouse_release_count",counts.release},{"owned_cancel",true},{"undo_redo_full_session",true},{"pending_input_alive",bool(pending)},{"pending_input_text",pending->text()},{"pending_input_modified",pending->isModified()},{"document_equal",w.host.session.document()==redone.document()},{"history_equal",w.host.session.history()==redone.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"view_equal",w.canvas->selections()==selections&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard}};producer.write(QJsonDocument(state).toJson());producer.close();
 w.host.save(prefix+"-saved.nect");w.host.flush();QFile post(prefix+"-post-save-producer.json");check(post.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable postSave Path matrix fullSession readback");QJsonObject post_state{{"document_equal",w.host.session.document()==redone.document()},{"preview_equal",w.host.session.preview_document()==redone.preview_document()},{"history_equal",w.host.session.history()==redone.history()},{"revision",static_cast<qint64>(w.host.session.revision())},{"gesture_generation",static_cast<qint64>(w.host.session.gesture_generation())},{"gesture_active",w.host.session.gesture_active()}};post.write(QJsonDocument(post_state).toJson());post.close();same(w.host.session,redone);check(read(prefix+"-saved.nect")==read(prefix+"-expected.nect"),"Actual Save committed Path matrix, never implicit pending40.5 AFTERactors");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch");const auto requested=qEnvironmentVariable("NECT_PATH_AFFINE_SCRUB_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<12;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Independent Path matrix drag failures: "+std::to_string(failures));std::cout<<"PASS Path matrix scrub "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Path matrix scrub "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
