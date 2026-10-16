#include <QAction>
#include <QMenu>
#include <QFontMetrics>
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
    auto source_fill=default_operation("source-fill","nect.paint.fill");source_fill.fill_rule="evenodd";session.apply({AddOperation{"built",default_operation("fill","nect.paint.fill"),3},AddOperation{"second",source_fill,3},AddOperation{"second",default_operation("alternate-fill","nect.paint.fill"),4}},session.revision());
    return session.document();
}





ProcessingEntry& entry(Document& d){for(auto& e:d.objects.at("built").stack)if(e.id=="fill")return e;throw std::runtime_error("Independent exact Fill missing");}
void flow(const QString& root,int mode){
 const int action=mode%4,context=mode<4?-1:(mode-4)/4;const bool success=mode<4;auto initial=fixture();auto& fill=entry(initial);fill.composite="below";fill.parameters.at("r").literal=.125;fill.parameters.at("g").literal=.234;fill.parameters.at("b").literal=.345;fill.parameters.at("a").literal=.456;fill.enabled_driver=operation_ref("second","second-offset","enabled");initial.objects.at("second").stack.at(1).parameters.at("amount").binding=Binding{operation_ref("built","offset","amount"),1,0,"copy_local_value"};const auto target=operation_ref("built","fill","fill_rule"),source=operation_ref("second","source-fill","fill_rule"),alternate=operation_ref("second","alternate-fill","fill_rule");
 if(action>=2){auto unlinked=initial;entry(initial).fill_rule_driver=FillRuleDriver{source};Session linked(unlinked);linked.apply({LinkFillRule{target,source,false}},0);check(linked.document()==initial,"Independent full initial linked Fill before Window");}valid(initial);Session base(initial);auto intended=initial;std::vector<Command> commands;
 if(action==0){entry(intended).fill_rule="evenodd";commands={OperationOptions{"built","fill","below","evenodd"}};}
 if(action==1){entry(intended).fill_rule_driver=FillRuleDriver{source};commands={LinkFillRule{target,source,false}};}
 if(action==2){entry(intended).fill_rule_driver.reset();entry(intended).fill_rule="evenodd";commands={UnlinkFillRule{target}};}
 if(action==3){entry(intended).fill_rule_driver.reset();entry(intended).fill_rule="nonzero";commands={UnlinkFillRule{target},OperationOptions{"built","fill","below","nonzero"}};}
 valid(intended);auto goal=base;goal.apply(commands,0);check(goal.document()==intended&&goal.revision()==1,"Independent manually intended full literal/link/unlink/explicit unlink+edit Doc equals atomic Core commands BEFORE Window");auto undone=goal;undone.undo(1);auto redone=undone;redone.redo(2);check(undone.document()==initial&&redone.document()==intended,"Independent full source/literal UndoRedo BEFORE Window");check(evaluate(initial).at(operation_ref("second","second-offset","amount"))==exact_amount,"Independent stable typed numeric binding BEFORE Window");
 auto replacement_doc=initial;replacement_doc.id="other-fill-rule-document";valid(replacement_doc);Session replacement(replacement_doc);auto committed_doc=initial;committed_doc.objects.at("second").name="External committed rename";valid(committed_doc);auto changed=base;changed.apply({Rename{"second","External committed rename"}},0);check(changed.document()==committed_doc,"Independent full committed caller BEFORE Window");auto preview_doc=initial;preview_doc.objects.at("second").name="External caller preview";valid(preview_doc);auto active=base;active.begin_gesture(0);active.update_gesture({Rename{"second","External caller preview"}});check(active.document()==initial&&active.preview_document()==preview_doc&&active.history()==base.history()&&active.gesture_active()&&active.revision()==0,"Independent full active caller BEFORE Window");auto cancelled=active;cancelled.cancel_gesture();check(cancelled.document()==initial&&cancelled.preview_document()==initial&&cancelled.history()==base.history()&&!cancelled.gesture_active()&&cancelled.revision()==0&&cancelled.gesture_generation()==1,"Independent full cancelled caller BEFORE Window");
 auto payload_doc=initial;entry(payload_doc).parameters.at("a").literal=.567;valid(payload_doc);Session payload(payload_doc);auto literal_doc=initial;entry(literal_doc).fill_rule="evenodd";valid(literal_doc);Session literal(literal_doc);auto driver_doc=initial;entry(driver_doc).fill_rule_driver=FillRuleDriver{alternate};valid(driver_doc);Session driver_changed(driver_doc);
 const Session* caller=&base;QString error;if(context==0){caller=&replacement;error="Operation control belongs to another document";}if(context==1)error="Fill belongs to another document";if(context==2){caller=&changed;error="The Fill changed while its rule editor was open; reopen it";}if(context==3){caller=&active;error="Finish or cancel the current gesture before another edit";}if(context==4){caller=&cancelled;error="Operation context changed; reopen the Inspector";}if(context==5){caller=&cancelled;error="Refresh the Inspector after finishing the active edit";}if(context==6){caller=&payload;error="The original operation or its enabled source changed";}if(context==7){caller=&literal;error="The original operation or its enabled source changed";}if(context==8){caller=&driver_changed;error="The original operation or its enabled source changed";}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",intended);write(path+"-caller.nect",caller->document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=context==5?active:base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();QPointer<QToolButton> driver=control<QToolButton>(w,"operation-fill-rule-driver-fill");reveal(driver);auto* menu=driver->menu();check(menu&&menu->actions().size()==1,"Original Fill rule menu/action exists");QPointer<QAction> opener=menu->actions().front();const auto sid=w.host.session_id;bool visited=false,stayed_open=false;QString message,callback_error;
 if(context==5){w.host.session=*caller;QSignalSpy blocked(opener,&QAction::triggered);opener->trigger();events();check(blocked.count()==1&&!w.findChild<QDialog*>("fill-rule-dialog-fill"),"Preview-born Fill rule opener refuses before modal after caller cancellation");check(w.statusBar()->currentMessage()=="GESTURE_ACTIVE: Refresh the Inspector after finishing the active edit","Preview-born early Fill refusal preserves original context diagnostic");same(w.host.session,*caller);write(path+"-observed.nect",w.host.session.document());check(read(path+"-observed.nect")==read(path+"-caller.nect"),"Early refusal preserves immutable full caller native");check(w.grab().save(path+"-opener-status.png"),"Actual original early Fill opener refusal rendered");w.close();events();same(w.host.session,*caller);std::cout<<"FILL_RULE_SOURCE mode="<<mode<<" early_opener_refusal=1\n";return;}
 QTimer::singleShot(0,&w,[&]{QPointer<QDialog> dialog=w.findChild<QDialog*>("fill-rule-dialog-fill");try{
  check(dialog&&dialog->isVisible(),"Original Fill rule native modal loop entered");visited=true;auto* modes=dialog->findChild<QComboBox*>("fill-rule-mode-fill");auto* value=dialog->findChild<QComboBox*>("fill-rule-value-fill");auto* sources=dialog->findChild<QComboBox*>("fill-rule-source-fill");auto* search=dialog->findChild<QLineEdit*>("fill-rule-source-search-fill");auto* consent=dialog->findChild<QCheckBox*>("fill-rule-unlink-before-edit-fill");auto* status=dialog->findChild<QLabel*>("fill-rule-status-fill");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(modes&&value&&sources&&search&&consent&&status&&buttons,"Exact original Fill rule staged controls exist");const auto selected=modes->findData(action==1?"link":action==2?"unlink":"edit");check(selected>=0,"Original intended Fill action offered");modes->setCurrentIndex(selected);
  if(action==1){search->setText("op.source-fill.fill_rule");check(sources->count()==1&&sources->itemText(0).contains("[source-fill]"),"Exact stable typed Fill source found through original search");sources->setCurrentIndex(0);}
  if(action==3){check(consent->isVisible()&&!consent->isChecked()&&!value->isEnabled(),"Linked literal edit requires explicit unlink consent");QSignalSpy consented(consent,&QCheckBox::toggled);QTest::mouseClick(consent,Qt::LeftButton,Qt::NoModifier,QPoint(8,consent->height()/2));check(consented.count()==1&&consent->isChecked()&&value->isEnabled(),"Original real checkbox grants explicit unlink+edit consent");}
  if(action==0||action==3){const auto next=value->findData(action==0?"evenodd":"nonzero");check(next>=0&&value->isEnabled(),"Original exact literal rule choice editable");value->setCurrentIndex(next);}same(w.host.session,context==5?active:base);check(dialog->grab().save(path+"-draft.png"),"Actual original staged rule/source/consent rendered before mutation");
  w.host.session=*caller;if(context==1)w.host.session_id+="-other";auto* apply=buttons->button(QDialogButtonBox::Apply);check(apply&&apply->isEnabled(),"Original Fill rule Apply enabled");QSignalSpy applied(apply,&QAbstractButton::clicked);QTest::mouseClick(apply,Qt::LeftButton);check(applied.count()==1,"One actual original staged Fill Apply");stayed_open=dialog&&dialog->isVisible();message=status->text();if(stayed_open){check(dialog->grab().save(path+"-diagnostic.png"),"Original refusal draft remains editable with rendered diagnostic");QSignalSpy cancel(buttons,&QDialogButtonBox::rejected);QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel),Qt::LeftButton);check(cancel.count()==1&&!dialog->isVisible(),"Original actual refusal Cancel preserves caller");}
 }catch(const std::exception& e){callback_error=QString::fromUtf8(e.what());}if(dialog&&dialog->isVisible())dialog->reject();});
 QSignalSpy triggered(opener,&QAction::triggered);opener->trigger();events();check(callback_error.isEmpty(),callback_error.toUtf8().constData());check(visited&&triggered.count()==1,"One exact original QAction trigger and bounded modal completion; Qt API not physical input");write(path+"-observed.nect",w.host.session.document());std::cout<<"FILL_RULE_SOURCE mode="<<mode<<" action="<<action<<" open="<<stayed_open<<" diagnostic="<<message.toStdString()<<"\n";same(w.host.session,success?goal:*caller);
 if(!success){check(stayed_open&&message==error,"Original Fill rule Apply refuses stale context with exact existing diagnostic preserved");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.session_id=sid;w.close();events();return;}
 check(!stayed_open,"Current original Fill staged Apply commits and closes");check(evaluate(w.host.session.document()).at(operation_ref("second","second-offset","amount"))==exact_amount,"Rule/source-only authoring preserves stable typed amount binding");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,goal);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Immutable actual native retains exact rule source/literal/companion style/RGBA/enabled source/Macro graphs and pins");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_FILL_RULE_SOURCE_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<40;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Fill rule source cases failed: "+std::to_string(failures));std::cout<<"PASS Fill rule source "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Fill rule source "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
