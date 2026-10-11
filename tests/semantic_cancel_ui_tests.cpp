#include "window.hpp"
#include "semantic_control.hpp"
#include "semantic_slider.hpp"
#include <QMouseEvent>
#include <QPointer>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolButton>
#include <QJsonDocument>
#include <QJsonObject>
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Production control missing: "+name).toStdString());}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible()&&c->isEnabled(),"Exact production control visible and enabled");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
void same(const Session& a,const Session& b){
 std::cout<<"SNAPSHOT doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}



constexpr double exact_amount=12.34567890123456,exact_rotation=-725.1234567890123;
Document fixture(){
    Session session(empty_document("slider-document","composition","artboard"));
    auto source=default_primitive("built-source","nect.shape.rectangle");
    auto offset=default_operation("offset","nect.shape.offset");offset.parameters.at("amount").literal=exact_amount;
    auto repeater=default_operation("repeater","nect.shape.repeater");repeater.parameters.at("copies").literal=4;
    repeater.parameters.at("rotation").literal=exact_rotation;repeater.parameters.at("position_x").literal=1;
    auto macro_source=source;macro_source.id="macro-source";
    auto macro_offset=offset;macro_offset.id="node-offset";
    auto macro_repeater=repeater;macro_repeater.id="node-repeater";
    MacroDefinitionRevision revision;revision.revision=2;revision.graph_version=2;revision.interface_version=2;
    revision.input={"input","local_paths_and_paint"};revision.output={"output","local_paths_and_paint"};
    revision.nodes={{macro_offset,"offset-in","offset-out"},{macro_repeater,"repeater-in","repeater-out"}};
    revision.edges={{{"","input"},{"node-offset","offset-in"}},
        {{"node-offset","offset-out"},{"node-repeater","repeater-in"}},
        {{"node-repeater","repeater-out"},{"","output"}}};
    revision.output_mapping={"node-repeater","repeater-out"};
    revision.public_parameters={{"macro.repeater.copies","Published copies","node-repeater","copies","number","scalar","local_paths_and_paint"},
        {"macro.offset.amount","Published amount","node-offset","amount","number","du","local_paths_and_paint"},
        {"macro.repeater.rotation","Published rotation","node-repeater","rotation","number","degree","local_paths_and_paint"}};
    auto initial=revision;initial.revision=1;initial.interface_version=1;
    initial.public_parameters={revision.public_parameters.at(1)};
    MacroDefinition definition;definition.id="definition";definition.label="Slider chain";definition.latest_revision=2;
    definition.revisions={{1,initial},{2,revision}};
    auto second_source=source;second_source.id="second-source";
    auto second_offset=offset;second_offset.id="second-offset";
    auto second_repeater=repeater;second_repeater.id="second-repeater";second_repeater.parameters.at("copies").literal=9;
    session.apply({CreatePrimitive{"composition","","built","Built",source},AddOperation{"built",offset,1},AddOperation{"built",repeater,2},
        CreatePrimitive{"composition","","macro","Macro",macro_source},MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"macro","definition","instance",2,1}},
        CreatePrimitive{"composition","","second","Second",second_source},AddOperation{"second",second_offset,1},
        AddOperation{"second",second_repeater,2}},0);
    return session.document();
}

QByteArray reference(const Ref& r){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(r.object)},{"point",QString::fromStdString(r.point)},{"field",QString::fromStdString(r.field)}}).toJson(QJsonDocument::Compact);}
template<class T>T* exact(Window& w,const Ref& ref){for(auto* c:w.findChildren<T*>())if(c->isVisible()&&c->property("nect-reference").toByteArray()==reference(ref))return c;throw std::runtime_error("Exact production Ref missing");}
QWidget* adjuster(Window& w,const Ref& r,bool slider){if(slider){auto* c=exact<QSlider>(w,r);check(dynamic_cast<SemanticSlider*>(c),"Exact production SemanticSlider");return c;}auto* c=exact<QToolButton>(w,r);check(dynamic_cast<SemanticScrub*>(c),"Exact production SemanticScrub");return c;}
void mouse(QWidget* c,QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent e(type,point,QPointF(c->mapToGlobal(QPoint{}))+point,button,buttons,Qt::NoModifier);QApplication::sendEvent(c,&e);}
QPoint handle(QSlider* c){QStyleOptionSlider o;o.initFrom(c);o.orientation=c->orientation();o.minimum=c->minimum();o.maximum=c->maximum();o.sliderPosition=c->sliderPosition();o.sliderValue=c->value();o.singleStep=c->singleStep();o.pageStep=c->pageStep();o.upsideDown=c->invertedAppearance();return c->style()->subControlRect(QStyle::CC_Slider,&o,QStyle::SC_SliderHandle,c).center();}
void begin_preview(Window& w,const Ref& ref,bool slider){auto* c=adjuster(w,ref,slider);reveal(c);if(slider){auto* bar=static_cast<QSlider*>(c);mouse(bar,QEvent::MouseButtonPress,handle(bar),Qt::LeftButton,Qt::LeftButton);check(bar->isSliderDown(),"Real slider press owns handle");bar->setSliderPosition(2);events();}else{mouse(c,QEvent::MouseButtonPress,QPointF(12,12),Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,QPointF(14,12),Qt::NoButton,Qt::LeftButton);events();}}
double value(const Document& d,const Ref& ref){return ref.point.empty()?evaluate(d).at(ref):macro_parameter_value(d,ref.object,ref.point,ref.field);}
Command command(const Ref& r,double v){if(r.point.empty())return EditProperties{{r},v,false};return MacroCommand{SetMacroOverride{r.object,r.point,r.field,v}};}
Document goal(Document d,const Ref& ref,double v){if(ref.point.empty()){const auto id=ref.field.substr(3,ref.field.rfind('.')-3),key=ref.field.substr(ref.field.rfind('.')+1);for(auto& o:d.objects.at(ref.object).stack)if(o.id==id)o.parameters.at(key).literal=v;}else{for(auto& o:d.objects.at(ref.object).stack)if(o.id==ref.point)o.macro->overrides[ref.field]=v;}valid(d);return d;}
void flow(const QString& root,int mode){
 const int kind=mode<8?mode%4:mode%2;const bool slider=kind%2,macro=kind>=2,numeric=mode<4,repeat=mode>=4&&mode<8,guard=mode>=8&&mode<14,queued=mode>=14;
 const Ref target=macro?macro_parameter_ref("macro","instance",slider?"macro.repeater.copies":"macro.offset.amount"):operation_ref("built",slider?"repeater":"offset",slider?"copies":"amount");
 const auto initial=fixture();valid(initial);Session base(initial);const auto start=value(initial,target);const auto descriptor=macro?macro_semantic_descriptor(initial,target):*property_semantic_descriptor(initial,target);
 check(validate_semantic_descriptor(descriptor).widget==(slider?SemanticWidget::slider:SemanticWidget::numeric)&&descriptor.step==1,"Actual native-valid semantic descriptor BEFORE Window");
 const auto preview_doc=goal(initial,target,start+2),intended=goal(initial,target,numeric?start+3:start-1);
 auto preview=base;preview.begin_gesture(0);preview.update_gesture({command(target,start+2)});check(preview.document()==initial&&preview.preview_document()==preview_doc,"Independent full preview and canonical initial BEFORE Window");
 auto cancelled=preview;cancelled.cancel_gesture();auto after=cancelled;
 if(numeric)after.apply({command(target,start+3)},0);else if(repeat){after.begin_gesture(0);after.update_gesture({command(target,start-1)});after.commit_gesture();}
 check(after.document()==((numeric||repeat)?intended:initial),"Independent full authored command goal BEFORE Window");auto undone=after,redone=after;if(numeric||repeat){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==intended,"Independent single UndoRedo BEFORE Window");}
 const auto caller_ref=operation_ref("built","repeater","position_x");const auto caller_doc=goal(initial,caller_ref,8.123456789012345);const auto caller=command(caller_ref,8.123456789012345);auto foreign=base;foreign.begin_gesture(0);foreign.update_gesture({caller});check(foreign.preview_document()==caller_doc,"Independent full foreign preview BEFORE Window");auto foreign_cancelled=foreign;foreign_cancelled.cancel_gesture();
 auto advanced=cancelled;advanced.apply({caller},0);check(advanced.document()==caller_doc,"Independent caller revision BEFORE Window");auto foreign_after=cancelled;foreign_after.begin_gesture(0);foreign_after.update_gesture({caller});auto replacement=initial;replacement.id+="-other";valid(replacement);Session replaced(replacement);
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);write(path+"-preview-expected.nect",preview_doc);write(path+"-caller-expected.nect",caller_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();w.activateWindow();events();w.canvas->set_selection(target.object);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
 auto enter=[&](const QString& text){auto* c=exact<QLineEdit>(w,target);reveal(c);c->setFocus();QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,text);QTest::keyClick(c,Qt::Key_Return);events();};
 if(guard){const int type=(mode-8)/2;w.host.session=foreign;if(type==1)w.host.session.cancel_gesture();if(type==2){w.host.edited();events();w.host.session.cancel_gesture();}enter(QString::number(start+3,'g',17));same(w.host.session,type==0?foreign:foreign_cancelled);check(w.statusBar()->currentMessage().contains("CONFLICT")||w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Foreign or preview-born numeric row remains refused");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.close();events();return;}
 begin_preview(w,target,slider);same(w.host.session,preview);auto* c=adjuster(w,target,slider);QPointer<QWidget> old_control=c;QPointer<QLineEdit> old_input=exact<QLineEdit>(w,target);
 QTest::keyClick(c,Qt::Key_Escape);same(w.host.session,cancelled);
 if(queued){const int type=(mode-14)/2;if(type==0)w.host.session=replaced;else if(type==1)w.host.session_id+="-other";else if(type==2)w.host.session=advanced;else if(type==3)w.host.session=foreign_after;else w.canvas->set_selection("second");events();same(w.host.session,type==0?replaced:type==2?advanced:type==3?foreign_after:cancelled);if(type<4)check(old_control&&old_input&&old_control->isVisible()&&old_input->isVisible(),"Own cancelled queued callback cannot refresh a foreign current context");else check(w.canvas->selections()==std::vector<Canvas::Selection>{{"second",{}}},"Queued callback never restores previous selection");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.close();events();return;}
 events();same(w.host.session,cancelled);check(exact<QLineEdit>(w,target)->text()==QString::number(start,'g',17),"Cancellation restores original exact visible value");
 if(numeric)enter(QString::number(start+3,'g',17));else{auto* current=adjuster(w,target,slider);reveal(current);QTest::keyClick(current,Qt::Key_Left);events();}
 std::cout<<"SEMANTIC_CANCEL mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual continuation feedback rendered");same(w.host.session,after);
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,after);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Full intended immutable actual native byte pair");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_SEMANTIC_CANCEL_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<24;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Semantic cancel cases failed: "+std::to_string(failures));std::cout<<"PASS Semantic cancel "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Semantic cancel "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
