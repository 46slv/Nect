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
    session.apply({AddOperation{"built",default_operation("fill","nect.paint.fill"),3}},session.revision());
    return session.document();
}



ProcessingEntry& entry(Document& d,const Id& id){for(auto& e:d.objects.at("built").stack)if(e.id==id)return e;throw std::runtime_error("Independent full processing entry missing");}
void flow(const QString& root,int mode){
 const bool rule_control=mode>=18;const int local=mode%18;const bool round=local%2;const int context=local<2?-1:(local-2)/2;auto initial=fixture();entry(initial,"offset").line_join=rule_control?"round":(round?"round":"miter");entry(initial,"offset").fill_rule=rule_control?(round?"evenodd":"nonzero"):"evenodd";initial.objects.at("second").stack.at(1).parameters.at("amount").binding=Binding{operation_ref("built","offset","amount"),1,0,"copy_local_value"};valid(initial);Session base(initial);check(evaluate(initial).at(operation_ref("second","second-offset","amount"))==exact_amount,"Independent exact stable numeric source BEFORE Window");
 auto intended=initial;if(rule_control)entry(intended,"offset").fill_rule=round?"nonzero":"evenodd";else entry(intended,"offset").line_join=round?"miter":"round";valid(intended);auto goal=base;if(rule_control)goal.apply({OperationOptions{"built","offset","below",round?"nonzero":"evenodd"}},0);else goal.apply({OperationOptions{"built","offset","below","evenodd",round?"miter":"round"}},0);check(goal.document()==intended&&goal.revision()==1,"Manually intended full Single Offset option Document equals canonical one command BEFORE Window");auto undone=goal;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==intended,"Independent full Single Offset option UndoRedo BEFORE Window");
 auto replacement_doc=initial;replacement_doc.id="other-composite-document";valid(replacement_doc);Session replacement(replacement_doc);auto changed=base;changed.apply({Rename{"second","External committed rename"}},0);auto active=base;active.begin_gesture(0);active.update_gesture({Rename{"second","External caller preview"}});auto cancelled=active;cancelled.cancel_gesture();check(cancelled.document()==initial&&cancelled.history()==base.history()&&cancelled.revision()==0&&cancelled.gesture_generation()==1,"Independent cancelled caller BEFORE Window");
 auto payload_doc=initial;entry(payload_doc,"offset").parameters.at("amount").literal=exact_amount+1;valid(payload_doc);Session payload(payload_doc);auto rule_doc=initial;if(rule_control)entry(rule_doc,"offset").line_join="miter";else entry(rule_doc,"offset").fill_rule="nonzero";valid(rule_doc);Session rule(rule_doc);
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",intended);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=context==5?active:base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();QPointer<QComboBox> combo=control<QComboBox>(w,rule_control?"operation-fill-rule-offset":"operation-line-join-offset");reveal(combo);check(combo&&combo->currentData().toString()==(rule_control?(round?"evenodd":"nonzero"):(round?"round":"miter")),"Original existing Offset option input reflects exact frozen authored value BEFORE mutation");
 const auto id=w.host.session_id;const Session* caller=&base;QString error;const bool success=local<2;
 if(context==0){w.host.session=replacement;caller=&replacement;error="SESSION_CONFLICT";}
 if(context==1){w.host.session_id+="-other";error="SESSION_CONFLICT";}
 if(context==2){w.host.session=changed;caller=&changed;error="REVISION_CONFLICT";}
 if(context==3){w.host.session=active;caller=&active;error="GESTURE_ACTIVE";}
 if(context==4){w.host.session=cancelled;caller=&cancelled;error="REVISION_CONFLICT";}
 if(context==5){w.host.session=cancelled;caller=&cancelled;error="GESTURE_ACTIVE";}
 if(context==6){w.host.session=payload;caller=&payload;error="PROPERTY_CONFLICT";}
 if(context==7){w.host.session=rule;caller=&rule;error="PROPERTY_CONFLICT";}
 QSignalSpy changed_index(combo,qOverload<int>(&QComboBox::currentIndexChanged));QTest::keyClick(combo,round?Qt::Key_Up:Qt::Key_Down);events();check(changed_index.count()==1,"One actual original Offset option currentIndexChanged producer invocation");std::cout<<"OPERATION_OFFSET_OPTIONS_CONTEXT mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";same(w.host.session,success?goal:*caller);check(w.statusBar()->grab().save(path+"-status.png"),"Actual Offset option context diagnostic rendered");
 if(!success){check(combo&&combo->currentData().toString()==(rule_control?(round?"evenodd":"nonzero"):(round?"round":"miter"))&&w.statusBar()->currentMessage().contains(error),"Original Offset option draft restores its original choice and reports failure without mutating caller");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.session_id=id;w.close();events();return;}
 check(w.statusBar()->currentMessage().isEmpty(),"Current successful Offset option edit has no fabricated failure");check(evaluate(w.host.session.document()).at(operation_ref("second","second-offset","amount"))==exact_amount,"Single Offset option change preserves stable typed amount source");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,goal);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Immutable actual full native keeps original fill rule/all source/Macro graph/pin and stable IDs");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_OPERATION_OFFSET_OPTIONS_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<36;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Operation Offset options context cases failed: "+std::to_string(failures));std::cout<<"PASS Operation Offset options context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Operation Offset options context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
