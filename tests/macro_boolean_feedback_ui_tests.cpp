#include <QAction>
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
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(alive,"Exact production control survives reveal events");check(alive->isVisible()&&alive->isEnabled(),"Exact production control visible and enabled");}
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
QLineEdit* input(Window& w,const Ref& ref){for(auto* c:w.findChildren<QLineEdit*>())if(c->isVisible()&&c->property("nect-reference").toByteArray()==reference(ref))return c;throw std::runtime_error("Exact production Macro parameter Ref missing");}
Document goal(Document d,const Ref& ref,double v){for(auto& o:d.objects.at("macro").stack)if(o.id==ref.point)o.macro->overrides[ref.field]=v;valid(d);return d;}




constexpr const char* offset_enabled="macro.offset.enabled";constexpr const char* repeater_enabled="macro.repeater.enabled";
Document boolean_fixture(){auto d=fixture();auto& graph=d.macro_definitions.at("definition").revisions.at(2);graph.interface_version=3;for(auto& n:graph.nodes)if(n.operation.id=="node-repeater")n.operation.enabled=false;graph.public_parameters.push_back({offset_enabled,"Use Offset","node-offset","enabled","boolean","boolean","local_paths_and_paint"});graph.public_parameters.push_back({repeater_enabled,"Use Repeater","node-repeater","enabled","boolean","boolean","local_paths_and_paint"});valid(d);return d;}
MacroInstance& instance(Document& d){for(auto& o:d.objects.at("macro").stack)if(o.id=="instance")return *o.macro;throw std::runtime_error("Manual instance missing");}
QCheckBox* toggle(Window& w,const std::string& field){return control<QCheckBox>(w,QString::fromStdString("macro-boolean-instance-"+field));}
QPushButton* reset(Window& w,const std::string& field){return control<QPushButton>(w,QString::fromStdString("macro-reset-boolean-instance-"+field));}
void flow(const QString& root,int mode){
 const bool success=mode<8,resetting=success?mode>=4:(mode-8)%2;const bool repeater=success?mode%2:false;const std::string field=repeater?repeater_enabled:offset_enabled;const bool pinned=!repeater;
 auto initial=boolean_fixture();if(resetting||(success&&mode>=2))instance(initial).boolean_overrides[field]=success&&mode>=6?!pinned:pinned;valid(initial);Session base(initial);const bool start=macro_parameter_boolean_value(initial,"macro","instance",field);check(start==(initial.objects.at("macro").stack.back().macro->boolean_overrides.contains(field)?instance(initial).boolean_overrides.at(field):pinned),"Independent typed initial boolean and override presence BEFORE Window");
 auto intended=initial;if(resetting)instance(intended).boolean_overrides.erase(field);else instance(intended).boolean_overrides[field]=!start;valid(intended);auto goal=base;if(resetting)goal.apply({MacroCommand{ResetMacroOverride{"macro","instance",field}}},0);else goal.apply({MacroCommand{SetMacroBooleanOverride{"macro","instance",field,!start}}},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==base.history().states.size()+1,"Independent typed full success snapshot BEFORE Window");auto undone=goal;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==intended,"Independent full Undo/Redo BEFORE Window");
 auto replacement_doc=initial;replacement_doc.id="other-boolean-document";valid(replacement_doc);Session replacement(replacement_doc);auto changed=base;changed.apply({Rename{"macro","External committed rename"}},0);auto active=base;active.begin_gesture(0);active.update_gesture({Rename{"macro","External caller preview"}});check(active.document()==initial&&active.gesture_active(),"Independent full active caller BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",intended);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("macro");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
 const Ref amount=macro_parameter_ref("macro","instance","macro.offset.amount");QPointer<QLineEdit> numeric=input(w,amount);reveal(numeric);numeric->setFocus();events();check(numeric,"Numeric field alive before private error");QTest::keyClick(numeric,Qt::Key_A,Qt::ControlModifier);for(const auto ch:QString("1+")){QTest::keyClicks(numeric,QString(ch));events();check(numeric,"Numeric field alive through private error");}QTest::keyClick(numeric,Qt::Key_Return);events();same(w.host.session,base);check(numeric->text()=="1+"&&numeric->isModified()&&w.statusBar()->currentMessage().contains("INVALID_VALUE"),"Actual rejected numeric error retains private input");
 QTest::keyClick(numeric,Qt::Key_Escape);events();same(w.host.session,base);check(numeric->text()==QString::number(exact_amount,'g',17)&&!numeric->isModified()&&w.statusBar()->currentMessage().contains("INVALID_VALUE"),"Explicit numeric Escape cancels draft and leaves genuine previous diagnostic before boolean action");
 QPointer<QCheckBox> box=toggle(w,field);QPointer<QAbstractButton> button=resetting?static_cast<QAbstractButton*>(reset(w,field)):static_cast<QAbstractButton*>(box);reveal(button);const auto original_id=w.host.session_id;
 const int context=success?-1:(mode-8)/2;const Session* caller=&base;QString error;
 if(context==0){w.host.session=replacement;caller=&replacement;error="SESSION_CONFLICT";}
 if(context==1){w.host.session_id+="-other";error="SESSION_CONFLICT";}
 if(context==2){w.host.session=changed;caller=&changed;error="REVISION_CONFLICT";}
 if(context==3){w.host.session=active;caller=&active;error="GESTURE_ACTIVE";}
 QSignalSpy spy(button,&QAbstractButton::clicked);QTest::mouseClick(button,Qt::LeftButton,Qt::NoModifier,QPoint(8,button->height()/2));events();check(spy.count()==1,"Exactly one existing production boolean/Reset clicked signal");
 same(w.host.session,success?goal:*caller);std::cout<<"MACRO_BOOLEAN_FEEDBACK mode="<<mode<<" reset="<<resetting<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";
 check(w.statusBar()->grab().save(path+"-status.png"),"Actual status bar after boolean action rendered");
 if(!success){check(w.statusBar()->currentMessage().contains(error)&&box&&box->isChecked()==start,"Actual context error and captured boolean survive deferred failure");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.session_id=original_id;w.close();events();return;}
 check(w.statusBar()->currentMessage().isEmpty(),"Successful typed boolean action clears the previous cancelled numeric error");
 check(toggle(w,field)->isChecked()==(resetting?pinned:!start),"Current production boolean matches independent intended value");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,goal);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Immutable actual full native boolean result preserves numeric/default/typed override state");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MACRO_BOOLEAN_FEEDBACK_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<16;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Macro boolean feedback cases failed: "+std::to_string(failures));std::cout<<"PASS Macro boolean feedback "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Macro boolean feedback "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
