#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTimer>
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



ProcessingEntry& entry(Document& d,const Id& object,const Id& id){for(auto& e:d.objects.at(object).stack)if(e.id==id)return e;throw std::runtime_error("Independent exact full processing entry missing");}
void flow(const QString& root,int mode){
 const bool expression=mode%2;const int context=mode<2?-1:(mode-2)/2;const auto target=operation_ref("built","offset","enabled"),source=operation_ref("second","second-offset","enabled");const Expression expr{" ! ref ( \"second\" , \"\" , \"op.second-offset.enabled\" ) ",1};auto initial=fixture();entry(initial,"built","offset").enabled=false;entry(initial,"second","second-offset").enabled=false;entry(initial,"second","second-offset").parameters.at("amount").binding=Binding{operation_ref("built","offset","amount"),1,0,"copy_local_value"};valid(initial);Session base(initial);
 auto intended=initial;if(expression)entry(intended,"built","offset").enabled_expression=expr;else entry(intended,"built","offset").enabled_driver=source;valid(intended);auto goal=base;if(expression)goal.apply({SetOperationEnabledExpression{target,expr,false}},0);else goal.apply({LinkOperationEnabled{target,source,false}},0);check(goal.document()==intended&&goal.revision()==1,"Independent manually intended full authored source Doc equals one canonical command BEFORE Window");const auto state=operation_enabled_state(intended,target);check(!state.literal&&state.evaluated==expression&&state.driver==(expression?std::optional<Ref>{}:std::optional<Ref>{source})&&state.expression==(expression?std::optional<Expression>{expr}:std::optional<Expression>{}),"Independent exact source Ref/expression/literal/evaluation BEFORE Window");auto undone=goal;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==intended,"Independent full enabled source UndoRedo BEFORE Window");
 auto replacement_doc=initial;replacement_doc.id="other-source-document";valid(replacement_doc);Session replacement(replacement_doc);auto changed=base;changed.apply({Rename{"second","External committed rename"}},0);auto active=base;active.begin_gesture(0);active.update_gesture({Rename{"second","External caller preview"}});auto cancelled=active;cancelled.cancel_gesture();check(cancelled.document()==initial&&cancelled.history()==base.history()&&cancelled.revision()==0&&cancelled.gesture_generation()==1,"Independent full cancelled caller BEFORE Window");auto payload_doc=initial;entry(payload_doc,"built","offset").parameters.at("amount").literal=exact_amount+1;valid(payload_doc);Session payload(payload_doc);auto literal_doc=initial;entry(literal_doc,"built","offset").enabled=true;valid(literal_doc);Session literal(literal_doc);
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",intended);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=context==5?active:base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();QPointer<QPushButton> driver=control<QPushButton>(w,"operation-enabled-driver-offset");reveal(driver);const auto id=w.host.session_id;const Session* caller=&base;QString expected_error;const bool success=mode<2;bool visited=false,stayed_open=false;QString message,callback_error;
 if(context==5){w.host.session=cancelled;QSignalSpy blocked(driver,&QAbstractButton::clicked);QTest::mouseClick(driver,Qt::LeftButton);events();check(blocked.count()==1&&!w.findChild<QDialog*>("operation-enabled-dialog-offset"),"Preview-born source opener refuses before modal after caller cancellation");check(w.statusBar()->currentMessage()=="GESTURE_ACTIVE: Refresh the Inspector after finishing the active edit","Preview-born early refusal preserves shared original-context diagnostic");same(w.host.session,cancelled);check(w.grab().save(path+"-opener-diagnostic.png"),"Actual preview-born early refusal rendered");w.close();events();same(w.host.session,cancelled);std::cout<<"OPERATION_SOURCE_CONTEXT mode="<<mode<<" early_opener_refusal=1\n";return;}
 QTimer::singleShot(0,&w,[&]{QPointer<QDialog> dialog=w.findChild<QDialog*>("operation-enabled-dialog-offset");try{
  check(dialog&&dialog->isVisible(),"Actual original existing source dialog entered its native modal loop");visited=true;auto* action=dialog->findChild<QComboBox*>("operation-enabled-mode-offset");auto* sources=dialog->findChild<QComboBox*>("operation-enabled-source-offset");auto* search=dialog->findChild<QLineEdit*>("operation-enabled-source-search-offset");auto* editor=dialog->findChild<QPlainTextEdit*>("operation-enabled-expression-source-offset");auto* status=dialog->findChild<QLabel*>("operation-enabled-status-offset");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(action&&sources&&search&&editor&&status&&buttons,"Exact production source editor controls exist");const auto selected=action->findData(expression?"expression":"link");check(selected>=0,"Existing intended source action offered");action->setCurrentIndex(selected);
  if(expression)editor->setPlainText(QString::fromStdString(expr.source));else{search->setText("op.second-offset.enabled");check(sources->count()==1&&sources->itemText(0).contains("[second-offset]"),"Exact stable typed source selected through existing path search");sources->setCurrentIndex(0);}check(dialog->grab().save(path+"-draft.png"),"Actual source draft rendered before caller mutation/Apply");
  if(context==0){w.host.session=replacement;caller=&replacement;expected_error="Operation control belongs to another document";}
  if(context==1){w.host.session_id+="-other";expected_error="Operation stack belongs to another document";}
  if(context==2){w.host.session=changed;caller=&changed;expected_error="Operation enabled source changed while its editor was open; reopen it";}
  if(context==3){w.host.session=active;caller=&active;expected_error="Finish or cancel the current gesture before another edit";}
  if(context==4){w.host.session=cancelled;caller=&cancelled;expected_error="Operation context changed; reopen the Inspector";}
  if(context==5){w.host.session=cancelled;caller=&cancelled;expected_error="Refresh the Inspector after finishing the active edit";}
  if(context==6){w.host.session=payload;caller=&payload;expected_error="The original operation or its enabled source changed";}
  if(context==7){w.host.session=literal;caller=&literal;expected_error="The original operation or its enabled source changed";}
  auto* apply=buttons->button(QDialogButtonBox::Apply);check(apply&&apply->isEnabled(),"Original source Apply producer is enabled");QSignalSpy applied(apply,&QAbstractButton::clicked);QTest::mouseClick(apply,Qt::LeftButton);check(applied.count()==1,"One actual existing source Apply activation");stayed_open=dialog&&dialog->isVisible();message=status->text();if(stayed_open)check(dialog->grab().save(path+"-diagnostic.png"),"Actual refusal dialog remains editable with rendered diagnostic");
 }catch(const std::exception& e){callback_error=QString::fromUtf8(e.what());}if(dialog&&dialog->isVisible())dialog->reject();});
 QSignalSpy opened(driver,&QAbstractButton::clicked);QTest::mouseClick(driver,Qt::LeftButton);events();check(callback_error.isEmpty(),callback_error.toUtf8().constData());check(visited&&opened.count()==1,"One actual original source opener and completed bounded modal interaction");std::cout<<"OPERATION_SOURCE_CONTEXT mode="<<mode<<" open_after_apply="<<stayed_open<<" diagnostic="<<message.toStdString()<<"\n";same(w.host.session,success?goal:*caller);
 if(!success){check(stayed_open&&message.contains(expected_error),"Original source Apply refuses changed caller context with exact original-revision diagnostic preserved");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.session_id=id;w.close();events();return;}
 check(!stayed_open,"Current source Apply accepts its original context");check(evaluate(w.host.session.document()).at(operation_ref("second","second-offset","amount"))==exact_amount,"Enabled-source authoring retains unrelated stable numeric source");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,goal);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Immutable full native retains exact expression/literal/source Ref/Macro graphs/pins");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_OPERATION_SOURCE_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<18;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Operation source context cases failed: "+std::to_string(failures));std::cout<<"PASS Operation source context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Operation source context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
