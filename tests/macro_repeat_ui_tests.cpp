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



QAction* history_action(Window& w,const QString& text,const QKeySequence& shortcut){QAction* result=nullptr;for(auto* a:w.findChildren<QAction*>())if(a->text()==text&&a->shortcut()==shortcut){check(!result,"Exact production history action is unique");result=a;}check(result&&result->isEnabled(),"Exact production history action available and enabled");return result;}
void flow(const QString& root,int mode){
 const bool copies=mode%2,overridden=mode>=2;const Ref target=macro_parameter_ref("macro","instance",copies?"macro.repeater.copies":"macro.offset.amount");auto initial=fixture();if(overridden)for(auto& o:initial.objects.at("macro").stack)if(o.id=="instance")o.macro->overrides[target.field]=copies?9:exact_amount+9;valid(initial);Session base(initial);const auto start=macro_parameter_value(initial,target.object,target.point,target.field);check(start==(copies?(overridden?9:4):(overridden?exact_amount+9:exact_amount)),"Independent pinned or local override start BEFORE Window");
 const auto first_value=start+2,second_value=first_value-1;const auto first_doc=goal(initial,target,first_value),second_doc=goal(first_doc,target,second_value);valid(first_doc);valid(second_doc);auto first=base;first.apply({MacroCommand{SetMacroOverride{target.object,target.point,target.field,first_value}}},0);auto second=first;second.apply({MacroCommand{SetMacroOverride{target.object,target.point,target.field,second_value}}},first.revision());check(first.document()==first_doc&&second.document()==second_doc&&first.revision()==1&&second.revision()==2&&second.gesture_generation()==0,"Independent two one-shot command full canonical snapshots BEFORE Window");
 auto undo1=second;undo1.undo(undo1.revision());auto undo2=undo1;undo2.undo(undo2.revision());auto redo1=undo2;redo1.redo(redo1.revision());auto redo2=redo1;redo2.redo(redo2.revision());check(undo1.document()==first_doc&&undo2.document()==initial&&redo1.document()==first_doc&&redo2.document()==second_doc,"Independent full Undo2/Redo2 snapshots BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-first-expected.nect",first_doc);write(path+"-second-expected.nect",second_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();w.activateWindow();events();w.canvas->set_selection("macro");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
 auto enter=[&](const QString& text){QPointer<QLineEdit> c=input(w,target);reveal(c);c->setFocus();events();check(c,"Original current numeric field alive before typing");QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);for(const auto ch:text){QTest::keyClicks(c,QString(ch));events();check(c,"Original current numeric field alive through guarded character");}QTest::keyClick(c,Qt::Key_Return);events();};
 enter("+=2");same(w.host.session,first);check(input(w,target)->text()==QString::number(first_value,'g',17)&&!input(w,target)->isModified()&&w.statusBar()->currentMessage().isEmpty(),"First relative commit refreshes exact current numeric control");
 enter("-=1");same(w.host.session,second);check(input(w,target)->text()==QString::number(second_value,'g',17)&&!input(w,target)->isModified()&&w.statusBar()->currentMessage().isEmpty(),"Second relative commit uses fresh current value and adds exactly one command");
 enter("+=0");same(w.host.session,second);check(input(w,target)->text()==QString::number(second_value,'g',17)&&!input(w,target)->isModified(),"Third zero correction uses new current initial and adds no History");
 auto save=[&](const QString& label,const QString& expected,const Session& snap){w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,snap);check(read(path+"-saved.nect")==read(path+"-"+expected+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-"+label+"-snapshot.nect"),"Actual native stage copied immutably before subsequent history mutation");};
 save("current","second",second);
 history_action(w,"Undo",QKeySequence(QKeySequence::Undo))->trigger();events();same(w.host.session,undo1);check(input(w,target)->text()==QString::number(first_value,'g',17),"Production GUI Undo restores first current Ref value");
 history_action(w,"Undo",QKeySequence(QKeySequence::Undo))->trigger();events();same(w.host.session,undo2);check(input(w,target)->text()==QString::number(start,'g',17),"Second production GUI Undo restores exact default/old local override");save("undone","initial",undo2);
 history_action(w,"Redo",QKeySequence(QKeySequence::Redo))->trigger();events();same(w.host.session,redo1);check(input(w,target)->text()==QString::number(first_value,'g',17),"Production GUI Redo restores first current Ref value");
 history_action(w,"Redo",QKeySequence(QKeySequence::Redo))->trigger();events();same(w.host.session,redo2);check(input(w,target)->text()==QString::number(second_value,'g',17),"Second production GUI Redo restores exact second result");save("redone","second",redo2);
 check(input(w,target)->grab().save(path+"-numeric.png"),"Final current numeric presentation rendered after real GUI history producers");w.close();events();same(w.host.session,redo2);std::cout<<"MACRO_REPEAT mode="<<mode<<" two_commands_and_zero_noop=PASS GUI_Undo2_Redo2=PASS\n";
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MACRO_REPEAT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Macro repeat cases failed: "+std::to_string(failures));std::cout<<"PASS Macro repeat "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Macro repeat "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
