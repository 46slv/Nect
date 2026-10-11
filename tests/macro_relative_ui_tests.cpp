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

void flow(const QString& root,int mode){
 const bool copies=mode%2,overridden=mode>=28,current=mode<13||overridden,noop=mode>=4&&mode<8,context_case=mode>=14&&mode<24,incomplete=mode==24||mode==25;const Ref target=macro_parameter_ref("macro","instance",copies?"macro.repeater.copies":"macro.offset.amount");
 auto initial=fixture();if(overridden)for(auto& o:initial.objects.at("macro").stack)if(o.id=="instance")o.macro->overrides[target.field]=copies?9:exact_amount+9;valid(initial);Session base(initial);const auto start=macro_parameter_value(initial,target.object,target.point,target.field);check(start==(copies?(overridden?9:4):(overridden?exact_amount+9:exact_amount)),"Independent pinned public Macro initial default BEFORE Window");
 const int group=mode/2;const double intended_value=mode==12?-2:noop?start:start+(group==1||group==5?-2:2);const auto intended=current&&!noop?goal(initial,target,intended_value):initial;valid(intended);auto after=base;if(current&&!noop)after.apply({MacroCommand{SetMacroOverride{target.object,target.point,target.field,intended_value}}},0);check(after.document()==intended&&after.revision()==(current&&!noop?1:0)&&after.gesture_generation()==0,"Independent full relative or exact zero-delta no-op canonical Session BEFORE Window");
 auto undone=after,redone=after;if(current&&!noop){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==intended,"Independent relative or negative absolute oneUndoRedo BEFORE Window");}
 const auto caller_ref=operation_ref("built","repeater","position_x");auto caller_doc=initial;for(auto& o:caller_doc.objects.at("built").stack)if(o.id=="repeater")o.parameters.at("position_x").literal=8.123456789012345;valid(caller_doc);const Command caller=EditProperties{{caller_ref},8.123456789012345,false};auto advanced=base;advanced.apply({caller},0);check(advanced.document()==caller_doc,"Independent caller revision BEFORE Window");
 auto foreign=base;foreign.begin_gesture(0);foreign.update_gesture({caller});check(foreign.document()==initial&&foreign.preview_document()==caller_doc,"Independent full caller preview BEFORE Window");auto cancelled=foreign;cancelled.cancel_gesture();
 auto replacement=caller_doc;replacement.id+="-replacement";valid(replacement);Session replaced(replacement);
 if(mode==13||mode==26||mode==27){auto rejected=base;bool refused=false;try{rejected.apply({MacroCommand{SetMacroOverride{target.object,target.point,target.field,mode==13?-2:start+(copies?1001:2000000)}}},0);}catch(const Error& e){refused=e.code=="OUT_OF_RANGE";}check(refused,"Independent Core negative Copies or relative range refusal BEFORE Window");same(rejected,base);}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);write(path+"-caller-expected.nect",caller_doc);write(path+"-replacement-expected.nect",replacement);
 std::cout<<"TRACE construct mode="<<mode<<"\n";QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();w.activateWindow();events();std::cout<<"TRACE shown\n";w.canvas->set_selection("macro");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
 auto enter=[&](const QString& text){std::cout<<"TRACE enter "<<text.toStdString()<<" lookup\n";QPointer<QLineEdit> c=input(w,target);std::cout<<"TRACE reveal\n";reveal(c);check(c,"Original exact numeric input alive after reveal");std::cout<<"TRACE focus\n";c->setFocus();events();check(c,"Original exact numeric input alive after focus events");std::cout<<"TRACE select\n";QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);check(c,"Original exact numeric input alive after select");for(const auto ch:text){std::cout<<"TRACE character "<<ch.toLatin1()<<"\n";QTest::keyClicks(c,QString(ch));events();check(c,"Original exact numeric input alive after single character");}std::cout<<"TRACE Return\n";QTest::keyClick(c,Qt::Key_Return);events();std::cout<<"TRACE entered\n";};
 enter("1 +");same(w.host.session,base);check(input(w,target)->text()=="1 +"&&input(w,target)->isModified()&&w.statusBar()->currentMessage().contains("INVALID_VALUE"),"Previous incomplete Macro draft is private/uncommitted before correction");
 const int context=(mode-14)/2;if(context_case){if(context==0)w.host.session=replaced;else if(context==1)w.host.session_id+="-other";else if(context==2)w.host.session=advanced;else if(context==3)w.host.session=foreign;else{w.host.session=foreign;w.host.edited();events();w.host.session.cancel_gesture();same(w.host.session,cancelled);}}
 const QString text=mode==12||mode==13?"-2":incomplete?"+=":overridden?"+=2":mode>=26?(copies?"+=1001":"+=2000000"):context_case?"+=2":group==0?"+=2":group==1?"-=2":group==2?"+=0":group==3?"-=0":group==4?"-=-2":"+="+QString("-2");
 enter(text);std::cout<<"MACRO_RELATIVE mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual relative Macro or refusal feedback rendered");
 if(!current){same(w.host.session,context_case?(context==0?replaced:context==2?advanced:context==3?foreign:context==4?cancelled:base):base);check(!w.statusBar()->currentMessage().isEmpty(),"Rejected relative Macro input reports actual error");if(context_case)check(w.statusBar()->currentMessage().contains("CONFLICT")||w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Original relative Macro context refuses caller mutation");else check(w.statusBar()->currentMessage().contains(incomplete?"INVALID_VALUE":"OUT_OF_RANGE")&&input(w,target)->text()==text&&input(w,target)->isModified(),"Invalid or range-rejected relative Macro input stays raw draft without authorship");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.close();events();return;}
 same(w.host.session,after);check(w.statusBar()->currentMessage().isEmpty(),"Successful one-shot relative or negative absolute input clears previous error");if(mode<12||overridden)check(input(w,target)->toolTip().contains("+=")&&input(w,target)->toolTip().contains("-="),"Published Macro input explains its one-shot relative grammar");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,after);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Exact full immutable relative authored or absent-override no-op native bytes");if(!noop){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MACRO_RELATIVE_ROOT"),root=requested.isEmpty()?temp.path():requested;std::cout.setf(std::ios::unitbuf);int failures=0;const auto first=qEnvironmentVariableIntValue("NECT_MACRO_RELATIVE_FIRST_CASE");for(int mode=first;mode<30;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Macro relative cases failed: "+std::to_string(failures));std::cout<<"PASS Macro relative "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Macro relative "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
