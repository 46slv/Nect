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




ShapeOperation& operation(Document& d,const Ref& target){for(auto& o:d.objects.at(target.object).stack)if(target.point.empty()&&target.field=="op."+o.id+".enabled")return o;throw std::runtime_error("Independent exact operation missing");}

void flow(const QString& root,int mode){
 const bool repeater=mode%2;const Id operation_id=repeater?"repeater":"offset";const Ref target=operation_ref("built",operation_id,"enabled");auto initial=fixture();valid(initial);Session base(initial);
 auto intended=initial;std::erase_if(intended.objects.at(target.object).stack,[&](const auto& entry){return entry.id==operation_id;});valid(intended);auto goal=base;goal.apply({RemoveOperation{target.object,operation_id}},0);check(goal.document()==intended&&goal.revision()==1,"Independent manually removed complete Doc and one command BEFORE Window");auto undone=goal;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==intended,"Independent full Remove UndoRedo BEFORE Window");
 auto replacement_doc=initial;replacement_doc.id="other-remove-document";valid(replacement_doc);Session replacement(replacement_doc);auto changed=base;changed.apply({Rename{"second","External committed rename"}},0);auto active=base;active.begin_gesture(0);active.update_gesture({Rename{"second","External caller preview"}});auto cancelled=active;cancelled.cancel_gesture();check(cancelled.document()==initial&&cancelled.history()==base.history()&&cancelled.revision()==0&&cancelled.gesture_generation()==1,"Independent cancelled generation snapshot BEFORE Window");
 auto retarget_doc=initial;operation(retarget_doc,target)=default_operation(operation_id,repeater?"nect.shape.offset":"nect.shape.repeater");valid(retarget_doc);Session retarget(retarget_doc);const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",intended);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=(mode==12||mode==13)?active:base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection(target.object);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
 QPointer<QPushButton> button=control<QPushButton>(w,"operation-remove-"+QString::fromStdString(operation_id));reveal(button);check(button&&button->isEnabled(),"Original exact Remove producer is available BEFORE mutation");const auto id=w.host.session_id;const Session* caller=&base;QString error;const bool success=mode<2;
 if(mode==2||mode==3){w.host.session=replacement;caller=&replacement;error="SESSION_CONFLICT";}
 if(mode==4||mode==5){w.host.session_id+="-other";error="SESSION_CONFLICT";}
 if(mode==6||mode==7){w.host.session=changed;caller=&changed;error="REVISION_CONFLICT";}
 if(mode==8||mode==9){w.host.session=active;caller=&active;error="GESTURE_ACTIVE";}
 if(mode==10||mode==11){w.host.session=cancelled;caller=&cancelled;error="REVISION_CONFLICT";}
 if(mode==12||mode==13){w.host.session=cancelled;caller=&cancelled;error="GESTURE_ACTIVE";}
 if(mode==14||mode==15){w.host.session=retarget;caller=&retarget;error="PROPERTY_CONFLICT";}
 QSignalSpy clicked(button,&QAbstractButton::clicked);QTest::mouseClick(button,Qt::LeftButton,Qt::NoModifier,QPoint(8,button->height()/2));events();check(clicked.count()==1,"Exactly one existing production Remove click signal");
 std::cout<<"OPERATION_REMOVE_CONTEXT mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";same(w.host.session,success?goal:*caller);check(w.statusBar()->grab().save(path+"-status.png"),"Actual Remove context status rendered");
 if(!success){check(button&&w.statusBar()->currentMessage().contains(error),"Original Remove button stays alive and reports real context failure");if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.session_id=id;w.close();events();return;}
 check(w.statusBar()->currentMessage().isEmpty(),"Current successful Remove has no fabricated error");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,goal);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Immutable full Remove result retains complete source/Macro pins/other stack entries and stable IDs");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_OPERATION_REMOVE_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<16;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Operation remove context cases failed: "+std::to_string(failures));std::cout<<"PASS Operation remove context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Operation remove context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
