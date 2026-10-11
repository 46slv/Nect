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


class TeardownObserver final:public QObject{
public:QObject* target=nullptr;bool destroying=false;int added=0;
protected:bool eventFilter(QObject* object,QEvent* event)override{if(destroying&&object==target&&event->type()==QEvent::ChildAdded)++added;return false;}
};
void flow(const QString& root,int mode){
 const bool copies=mode%2,range=mode>=2;const Ref target=macro_parameter_ref("macro","instance",copies?"macro.repeater.copies":"macro.offset.amount");auto initial=fixture();for(auto& o:initial.objects.at("macro").stack)if(o.id=="instance")o.macro->overrides[target.field]=copies?9:exact_amount+9;valid(initial);Session base(initial);
 const QString text=range?(copies?"1001":"2000000"):"1 +";if(range){auto rejected=base;bool refused=false;try{rejected.apply({MacroCommand{SetMacroOverride{target.object,target.point,target.field,text.toDouble()}}},0);}catch(const Error& e){refused=e.code=="OUT_OF_RANGE";}check(refused,"Independent invalid Macro range command refuses BEFORE Window");same(rejected,base);}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",initial);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);auto owner=std::make_unique<Window>(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);auto& w=*owner;w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();w.activateWindow();events();w.canvas->set_selection("macro");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable native initial state saved before private draft");
 QPointer<QLineEdit> c=input(w,target);reveal(c);c->setFocus();events();check(c,"Original input alive before draft typing");QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);for(const auto ch:text){QTest::keyClicks(c,QString(ch));events();check(c,"Original input alive through guarded character");}QTest::keyClick(c,Qt::Key_Return);events();same(w.host.session,base);
 c=input(w,target);check(c->hasFocus()&&c->isModified()&&c->text()==text&&w.statusBar()->currentMessage().contains(range?"OUT_OF_RANGE":"INVALID_VALUE"),"Focused private Macro draft and actual error remain before normal Window teardown");check(c->parentWidget()->findChild<QPushButton*>("macro-reset-amount-instance"),"Existing override owns exact Reset-linked numeric row");
 TeardownObserver observed;observed.target=owner.get();qApp->installEventFilter(&observed);QPointer<QWidget> alive=owner.get();observed.destroying=true;std::cout<<"TEARDOWN begin mode="<<mode<<"\n";owner.reset();observed.destroying=false;qApp->removeEventFilter(&observed);events();check(!alive,"Owned Window and children destroyed normally");check(observed.added==0,"Teardown never creates new Window children through retired callbacks");check(read(path+"-saved.nect")==read(path+"-expected.nect")&&read(path+"-snapshot.nect")==read(path+"-expected.nect"),"Private invalid Macro draft never changes actual saved native state during teardown");
 QWidget probe;probe.show();events();check(probe.isVisible()&&probe.winId()!=0,"Native qwindows surface/event loop remains usable after normal teardown");probe.close();events();check(!probe.isVisible(),"Owned post-teardown native probe closes normally");std::cout<<"TEARDOWN completed mode="<<mode<<" child_additions="<<observed.added<<"\n";
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MACRO_TEARDOWN_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Macro teardown cases failed: "+std::to_string(failures));std::cout<<"PASS Macro teardown "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Macro teardown "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
