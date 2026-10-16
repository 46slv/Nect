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





ProcessingEntry& entry(Document& d){for(auto& e:d.objects.at("built").stack)if(e.id=="fill")return e;throw std::runtime_error("Exact independent target missing");}
void flow(const QString& root,int mode){
 auto initial=fixture();const auto source=operation_ref("second","source-fill","fill_rule");if(mode==1){auto original=initial;entry(initial).fill_rule_driver=FillRuleDriver{source};Session linked(original);linked.apply({LinkFillRule{operation_ref("built","fill","fill_rule"),source,false}},0);check(linked.document()==initial,"Independent exact current linked Fill Doc before Window");}valid(initial);Session base(initial);
 auto replacement_doc=initial;replacement_doc.id="other-opener-document";valid(replacement_doc);Session replacement(replacement_doc);auto changed_doc=initial;changed_doc.objects.at("second").name="External committed rename";valid(changed_doc);auto changed=base;changed.apply({Rename{"second","External committed rename"}},0);check(changed.document()==changed_doc,"Independent committed caller Doc and full History before Window");
 auto preview_doc=initial;preview_doc.objects.at("second").name="External caller preview";valid(preview_doc);auto active=base;active.begin_gesture(0);active.update_gesture({Rename{"second","External caller preview"}});check(active.document()==initial&&active.preview_document()==preview_doc&&active.history()==base.history()&&active.revision()==0&&active.gesture_active(),"Independent full active caller before Window");auto cancelled=active;cancelled.cancel_gesture();check(cancelled.document()==initial&&cancelled.preview_document()==initial&&cancelled.history()==base.history()&&cancelled.revision()==0&&!cancelled.gesture_active()&&cancelled.gesture_generation()==1,"Independent full cancelled caller before Window");
 auto payload_doc=initial;entry(payload_doc).parameters.at("a").literal=.567;valid(payload_doc);Session payload(payload_doc);auto literal_doc=initial;entry(literal_doc).fill_rule="evenodd";valid(literal_doc);Session literal(literal_doc);
 auto removed_doc=initial;auto& stack=removed_doc.objects.at("built").stack;stack.erase(std::find_if(stack.begin(),stack.end(),[](const auto& e){return e.id=="fill";}));valid(removed_doc);Session removed(removed_doc);auto removed_command=base;removed_command.apply({RemoveOperation{"built","fill"}},0);check(removed_command.document()==removed_doc,"Independent full target removal Doc before Window");
 auto owner_doc=initial;owner_doc.objects.erase("built");auto& roots=owner_doc.compositions.at(0).roots;roots.erase(std::find(roots.begin(),roots.end(),"built"));valid(owner_doc);Session owner_removed(owner_doc);auto owner_command=base;owner_command.apply({DeleteObjects{{"built"}}},0);check(owner_command.document()==owner_doc,"Independent full owner removal Doc before Window");
 const int context=mode<2?-1:mode-2;const Session* caller=&base;QString expected_error;if(context==0){caller=&replacement;expected_error="SESSION_CONFLICT: Operation control belongs to another document";}if(context==1)expected_error="SESSION_CONFLICT: Operation control belongs to another document";if(context==2){caller=&changed;expected_error="REVISION_CONFLICT: Operation context changed; reopen the Inspector";}if(context==3){caller=&active;expected_error="GESTURE_ACTIVE: Refresh the Inspector after finishing the active edit";}if(context==4){caller=&cancelled;expected_error="REVISION_CONFLICT: Operation context changed; reopen the Inspector";}if(context==5){caller=&cancelled;expected_error="GESTURE_ACTIVE: Refresh the Inspector after finishing the active edit";}if(context==6){caller=&payload;expected_error="PROPERTY_CONFLICT: The original operation or its enabled source changed";}if(context==7){caller=&literal;expected_error="PROPERTY_CONFLICT: The original operation or its enabled source changed";}if(context==8){caller=&removed;expected_error="PROPERTY_CONFLICT: The original operation or its enabled source changed";}if(context==9){caller=&owner_removed;expected_error="PROPERTY_CONFLICT: The original operation owner changed";}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",caller->document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=context==5?active:base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();QPointer<QToolButton> driver=control<QToolButton>(w,"operation-fill-rule-driver-fill");reveal(driver);auto* menu=driver->menu();check(menu&&menu->actions().size()==1,"Original Fill rule opener QAction exists");QPointer<QAction> opener=menu->actions().front();const auto sid=w.host.session_id;
 w.host.session=*caller;if(context==1)w.host.session_id+="-other";same(w.host.session,*caller);bool visited=false;QString callback_error;
 QTimer::singleShot(0,&w,[&]{QPointer<QDialog> dialog=w.findChild<QDialog*>("fill-rule-dialog-fill");if(!dialog||!dialog->isVisible())return;visited=true;try{check(dialog->grab().save(path+"-dialog.png"),"Actual opened dialog retained before Cancel");if(mode<2){auto* value=dialog->findChild<QComboBox*>("fill-rule-value-fill");auto* actions=dialog->findChild<QComboBox*>("fill-rule-mode-fill");auto* sources=dialog->findChild<QComboBox*>("fill-rule-source-fill");auto* consent=dialog->findChild<QCheckBox*>("fill-rule-unlink-before-edit-fill");check(value&&actions&&sources&&consent&&actions->findData("link")>=0&&actions->currentData().toString()=="edit","Current original Fill rule choices/default mode preserved");check(value->currentData().toString()==(mode==1?"evenodd":"nonzero")&&value->isEnabled()==(mode==0)&&consent->isVisible()==(mode==1)&&!consent->isChecked(),"Current original literal/evaluated rule and explicit unlink consent preserved");if(mode==1)check(actions->findData("unlink")>=0&&sources->currentText().contains("[source-fill]"),"Current linked Fill exact stable Ref selection retained");else check(actions->findData("unlink")<0,"Current literal Fill has no fabricated unlink action");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons&&buttons->button(QDialogButtonBox::Cancel),"Original Cancel exists");QSignalSpy cancelled_signal(buttons,&QDialogButtonBox::rejected);QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel),Qt::LeftButton);check(cancelled_signal.count()==1&&!dialog->isVisible(),"Current original actual Cancel leaves context unchanged");}}catch(const std::exception& e){callback_error=QString::fromUtf8(e.what());}if(dialog&&dialog->isVisible())dialog->reject();});
 QSignalSpy opened(opener,&QAction::triggered);opener->trigger();events();const auto message=w.statusBar()->currentMessage();check(callback_error.isEmpty(),callback_error.toUtf8().constData());check(opened.count()==1,"One actual original dependency opener activation");same(w.host.session,*caller);write(path+"-snapshot.nect",w.host.session.document());check(read(path+"-snapshot.nect")==read(path+"-expected.nect"),"Immutable full caller native bytes preserved despite stale opener");check(w.grab().save(path+"-status.png"),"Actual original opener/status Window rendered");
 if(mode<2){w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,*caller);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Current real Save retains original full authored source");}
 std::cout<<"FILL_RULE_OPENER mode="<<mode<<" visited="<<visited<<" status="<<message.toStdString()<<"\n";if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.session_id=sid;w.close();events();check(mode<2?visited:!visited,"Current opener reaches original dialog; stale opener refused before any modal source enumeration");if(mode>=2)check(message==expected_error,"Precise original shared context error rendered outside unopened dialog");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_FILL_RULE_OPENER_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<12;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Fill rule opener cases failed: "+std::to_string(failures));std::cout<<"PASS Fill rule opener "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Fill rule opener "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
