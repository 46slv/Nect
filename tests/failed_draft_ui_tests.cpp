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
Document base_fixture(){
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


Document fixture(){
 auto d=base_fixture();auto source=default_primitive("polygon-source","nect.shape.polygon");source.parameters.at("rotation").literal=-725.1234567890123;
 Point p;p.id="stable-point";p.x.literal=80.123456789012345;p.y.literal=70.234567890123456;p.in_angle.literal=725.1234567890123;p.in_length.literal=0;p.out_angle.literal=-725.1234567890123;p.out_length.literal=0;
 Session seed(d);seed.apply({CreatePrimitive{"composition","","primitive","Polygon",source},CreatePath{"composition","","path","Path",{{"stable-contour",false,{p}}}}},0);return seed.document();
}
QByteArray reference(const Ref& r){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(r.object)},{"point",QString::fromStdString(r.point)},{"field",QString::fromStdString(r.field)}}).toJson(QJsonDocument::Compact);}
template<class T>T* exact(Window& w,const Ref& ref){for(auto* c:w.findChildren<T*>())if(c->isVisible()&&c->property("nect-reference").toByteArray()==reference(ref))return c;throw std::runtime_error("Exact production Ref missing");}
QWidget* adjuster(Window& w,const Ref& r){for(auto* c:w.findChildren<QWidget*>())if(c->isVisible()&&c->property("nect-reference").toByteArray()==reference(r)&&(c->objectName().startsWith("repeater-angle-knob")||c->objectName()=="primitive-angle-knob"||c->objectName().startsWith("point-angle-knob")))return c;throw std::runtime_error("Exact production angle Ref dial missing");}
void mouse(QWidget* c,QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent e(type,point,QPointF(c->mapToGlobal(QPoint{}))+point,button,buttons,Qt::NoModifier);QApplication::sendEvent(c,&e);}
void begin_preview(Window& w,const Ref& ref){auto* c=adjuster(w,ref);reveal(c);mouse(c,QEvent::MouseButtonPress,QPointF(22,6),Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,QPointF(38,22),Qt::NoButton,Qt::LeftButton);events();}

double value(const Document& d,const Ref& ref){return evaluate(d).at(ref);}
Command command(const Ref& r,double v){return EditProperties{{r},v,false};}
Document goal(Document d,const Ref& ref,double v){if(ref.point.empty()){if(ref.field.starts_with("generator."))d.objects.at(ref.object).source->parameters.at(ref.field.substr(10)).literal=v;else{const auto id=ref.field.substr(3,ref.field.rfind('.')-3),key=ref.field.substr(ref.field.rfind('.')+1);for(auto& o:d.objects.at(ref.object).stack)if(o.id==id)o.parameters.at(key).literal=v;}}else{for(auto& c:d.objects.at(ref.object).contours)for(auto& p:c.points)if(p.id==ref.point)p.out_angle.literal=v;}valid(d);return d;}

Command edit(const Ref& r,double v){if(r.point.empty()||r.object=="path")return EditProperties{{r},v,false};return MacroCommand{SetMacroOverride{r.object,r.point,r.field,v}};}
Document expected(Document d,const Ref& r,double v){if(r.object=="macro"){for(auto& o:d.objects.at(r.object).stack)if(o.id==r.point)o.macro->overrides[r.field]=v;valid(d);return d;}return goal(d,r,v);}
double canonical_value(const Document& d,const Ref& r){return r.object=="macro"?macro_parameter_value(d,r.object,r.point,r.field):evaluate(d).at(r);}
QWidget* gesture_control(Window& w,const Ref& r,int kind){if(kind<4)return kind>=2?static_cast<QWidget*>(exact<QSlider>(w,r)):static_cast<QWidget*>(exact<QToolButton>(w,r));return adjuster(w,r);}
void attempt(Window& w,const Ref& ref,int kind,bool empty=false){
 auto* c=gesture_control(w,ref,kind);reveal(c);if(kind>=4){mouse(c,QEvent::MouseButtonPress,QPointF(22,6),Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,QPointF(38,22),Qt::NoButton,Qt::LeftButton);mouse(c,QEvent::MouseButtonRelease,QPointF(38,22),Qt::LeftButton,Qt::NoButton);events();}
 else if(kind<2){mouse(c,QEvent::MouseButtonPress,QPointF(12,12),Qt::LeftButton,Qt::LeftButton);mouse(c,QEvent::MouseMove,QPointF(empty?12:14,12),Qt::NoButton,Qt::LeftButton);mouse(c,QEvent::MouseButtonRelease,QPointF(empty?12:14,12),Qt::LeftButton,Qt::NoButton);events();}
 else {auto* b=static_cast<QSlider*>(c);QStyleOptionSlider o;o.initFrom(b);o.orientation=b->orientation();o.minimum=b->minimum();o.maximum=b->maximum();o.sliderPosition=b->sliderPosition();o.sliderValue=b->value();o.singleStep=b->singleStep();o.pageStep=b->pageStep();const auto point=b->style()->subControlRect(QStyle::CC_Slider,&o,QStyle::SC_SliderHandle,b).center();mouse(b,QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);if(b->isSliderDown()){b->setSliderPosition(empty?0:2);events();}mouse(b,QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton);events();}
}


void flow(const QString& root,int mode){
 const int kind=mode%5,group=mode/5;const bool recovery=group==1||group==2,gesture=group==1;
 const Ref target=kind==0?operation_ref("built","offset","amount"):kind==1?macro_parameter_ref("macro","instance","macro.offset.amount"):kind==2?operation_ref("built","repeater","copies"):kind==3?macro_parameter_ref("macro","instance","macro.repeater.copies"):operation_ref("built","repeater","rotation");
 const auto initial=fixture();valid(initial);const auto start=canonical_value(initial,target);Session base(initial);
 const auto value=start+(gesture?(kind<4?2:90):3);const auto intended=recovery?expected(initial,target,value):initial;valid(intended);auto after=base;if(recovery){if(gesture){after.begin_gesture(0);after.update_gesture({edit(target,value)});after.commit_gesture();}else after.apply({edit(target,value)},0);}
 check(after.document()==intended&&after.revision()==(recovery?1:0)&&after.gesture_generation()==(gesture?1:0),"Independent full refused or explicit recovery canonical Session BEFORE Window");
 auto undone=after,redone=after;if(recovery){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==intended,"Independent explicit recovery oneUndoRedo BEFORE Window");}
 const auto caller_ref=operation_ref("built","repeater","position_x");const auto caller_doc=expected(initial,caller_ref,8.123456789012345);auto foreign=base;foreign.begin_gesture(0);foreign.update_gesture({EditProperties{{caller_ref},8.123456789012345,false}});check(foreign.document()==initial&&foreign.preview_document()==caller_doc,"Independent full caller preview BEFORE Window");
 auto replacement=caller_doc;replacement.id+="-replacement";valid(replacement);Session replaced(replacement);
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);write(path+"-caller-expected.nect",caller_doc);write(path+"-replacement-expected.nect",replacement);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();w.activateWindow();events();w.canvas->set_selection(target.object);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
 auto enter=[&](const QString& text){auto* c=exact<QLineEdit>(w,target);reveal(c);c->setFocus();QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,text);QTest::keyClick(c,Qt::Key_Return);events();};
 check(dynamic_cast<SemanticNumberInput*>(exact<QLineEdit>(w,target)),"Exact existing semantic numeric input contract");enter("1 +");same(w.host.session,base);check(exact<QLineEdit>(w,target)->text()=="1 +"&&w.statusBar()->currentMessage().contains("INVALID_VALUE"),"Failed numeric Return keeps raw incomplete draft and full canonical initial state");
 if(gesture){auto* c=exact<QLineEdit>(w,target);QTest::keyClick(c,Qt::Key_Escape);events();same(w.host.session,base);c=exact<QLineEdit>(w,target);check(c->text()==QString::number(start,'g',17)&&!c->isModified(),"Explicit Escape alone clears rejected draft and restores exact numeric presentation");}
 if(group==3)w.host.session=foreign;else if(group==4)w.host.session=replaced;
 if(group==2)enter(QString::number(value,'g',17));else attempt(w,target,kind);
 std::cout<<"FAILED_DRAFT mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual failed draft or explicit recovery outcome rendered");
 if(!recovery){same(w.host.session,group==3?foreign:group==4?replaced:base);auto* c=exact<QLineEdit>(w,target);check(c->text()=="1 +"&&c->isModified()&&w.statusBar()->currentMessage().contains("UNCOMMITTED_INPUT"),"Failed numeric Return remains uncommitted: gesture refuses and retains raw draft until explicit correction or Escape");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.close();events();return;}
 same(w.host.session,after);check(w.statusBar()->currentMessage().isEmpty(),"Explicit valid correction or Escape plus current gesture succeeds and clears previous error");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,after);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Exact full immutable actual native explicit recovery bytes");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_FAILED_DRAFT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<25;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Failed draft cases failed: "+std::to_string(failures));std::cout<<"PASS Failed draft "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Failed draft "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
