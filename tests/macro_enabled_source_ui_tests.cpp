#include <QGroupBox>
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
bool near(Vec2 a,Vec2 b){return std::abs(a.x-b.x)<1e-7&&std::abs(a.y-b.y)<1e-7;}
bool same_paths(const std::vector<PathInstance>& a,const std::vector<PathInstance>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i) {
        if(a[i].contours->size()!=b[i].contours->size())return false;
        for(std::size_t c=0;c<a[i].contours->size();++c) {
            const auto& x=(*a[i].contours)[c];const auto& y=(*b[i].contours)[c];
            if(x.closed!=y.closed||x.points.size()!=y.points.size())return false;
            for(std::size_t p=0;p<x.points.size();++p) {
                const auto& xp=x.points[p];const auto& yp=y.points[p];
                if(!near(map_point(a[i].transform,xp.anchor),map_point(b[i].transform,yp.anchor))||
                    !near(map_point(a[i].transform,xp.incoming),map_point(b[i].transform,yp.incoming))||
                    !near(map_point(a[i].transform,xp.outgoing),map_point(b[i].transform,yp.outgoing)))return false;
            }
        }
    }
    return true;
}
void same_shape(const EvaluatedShape& actual,const EvaluatedShape& expected) {
    check(same_paths(actual.paths,expected.paths),"Macro evaluated cubic output equals the explicit pinned operator sequence");
    check(actual.paints.size()==expected.paints.size(),"Pinned Macro preserves expected paint count");
    for(std::size_t i=0;i<actual.paints.size();++i) {
        const auto& a=actual.paints[i];const auto& b=expected.paints[i];
        check(a.type==b.type&&a.rgba==b.rgba&&a.fill_rule==b.fill_rule&&a.transform==b.transform&&same_paths(a.paths,b.paths),
            "Pinned Macro paint geometry equals the ordinary operator oracle");
    }
}

void flow(const QString& root,int mode){
 const bool ordinary=mode==4,published=mode==3;auto initial=fixture();if(mode==0)for(auto& entry:initial.objects.at("macro").stack)if(entry.id=="instance")entry.macro->pinned_revision=1;if(mode==2)operation(initial,operation_ref("macro","instance","enabled")).enabled=false;
 if(published){auto& graph=initial.macro_definitions.at("definition").revisions.at(2);graph.interface_version=3;graph.public_parameters.push_back({"macro.offset.enabled","Use Offset","node-offset","enabled","boolean","boolean","local_paths_and_paint"});}valid(initial);Session base(initial);const Id object=ordinary?"built":"macro",op=ordinary?"offset":"instance";const auto target=operation_ref(object,op,"enabled");auto intended=initial;const bool next=!operation(initial,target).enabled;
 if(published){for(auto& entry:intended.objects.at("macro").stack)if(entry.id=="instance")entry.macro->boolean_overrides["macro.offset.enabled"]=false;}else operation(intended,target).enabled=next;valid(intended);auto goal=base;if(published)goal.apply({MacroCommand{SetMacroBooleanOverride{"macro","instance","macro.offset.enabled",false}}},0);else goal.apply({EnableOperation{object,op,next}},0);check(goal.document()==intended&&goal.revision()==1,"Independent supported direct/public Enabled full intended Doc BEFORE Window");auto undone=goal;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());check(undone.document()==initial&&redone.document()==intended,"Independent complete supported UndoRedo BEFORE Window");
 const auto source=operation_ref("second","second-offset","enabled");auto source_doc=initial;operation(source_doc,target).enabled_driver=source;
 if(!ordinary){bool invalid=false;try{validate(source_doc);}catch(const Error& e){invalid=e.code=="INVALID_MACRO_INSTANCE";}check(invalid,"Independent Macro-target driver payload rejected by native schema BEFORE Window");auto refused=base;invalid=false;try{refused.apply({LinkOperationEnabled{target,source,false}},0);}catch(const Error& e){invalid=e.code=="INVALID_MACRO_INSTANCE";}check(invalid,"Canonical Macro-target link rejects unsupported payload BEFORE Window");same(refused,base);}
 else{valid(source_doc);auto linked=base;linked.apply({LinkOperationEnabled{target,source,false}},0);check(linked.document()==source_doc,"Ordinary enabled link remains native/Core supported BEFORE Window");}
 auto raw_source=initial;std::erase_if(raw_source.objects.at("macro").stack,[](const auto& entry){return entry.id=="instance";});valid(raw_source);std::optional<EvaluatedShape> expected_shape;
 if(!ordinary&&!published){auto geometric_oracle=next?initial:raw_source;if(next)operation(geometric_oracle,target).enabled=true;valid(geometric_oracle);expected_shape=evaluate_shape(geometric_oracle,"macro",evaluate(geometric_oracle));check(expected_shape->paths.size()==(next?4u:1u),"Independent active-four/bypassed-one path cardinality BEFORE Window");same_shape(evaluate_shape(intended,"macro",evaluate(intended)),*expected_shape);}
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",intended);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection(object);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();QPointer<QPushButton> driver=control<QPushButton>(w,"operation-enabled-driver-"+QString::fromStdString(op));QPointer<QPushButton> alive_driver=driver;for(auto* parent=driver->parentWidget();parent;parent=parent->parentWidget())if(auto* scroll=qobject_cast<QScrollArea*>(parent))scroll->ensureWidgetVisible(driver);events();check(alive_driver,"Source availability control survives reveal events");check(alive_driver->isVisible(),"Supported or unsupported source availability control is visible");
 std::cout<<"MACRO_ENABLED_SOURCE mode="<<mode<<" driver_available="<<driver->isEnabled()<<" tooltip="<<driver->toolTip().toStdString()<<"\n";check(driver->isEnabled()==ordinary,"Enabled-source action is offered only to supported ordinary target");if(!ordinary)check(driver->toolTip().contains("unavailable",Qt::CaseInsensitive),"Unsupported Macro source action explains its availability");
 QPointer<QCheckBox> box=published?control<QCheckBox>(w,"macro-boolean-instance-macro.offset.enabled"):control<QCheckBox>(w,"operation-enabled-"+QString::fromStdString(op));reveal(box);check(box&&box->isEnabled()&&box->isChecked()==(published?true:operation(initial,target).enabled),"Supported Macro direct/public and ordinary Enabled control remains available");QSignalSpy clicked(box,&QAbstractButton::clicked);QTest::mouseClick(box,Qt::LeftButton,Qt::NoModifier,QPoint(8,box->height()/2));events();check(clicked.count()==1,"One supported production Enabled activation");same(w.host.session,goal);check(w.statusBar()->currentMessage().isEmpty(),"Supported Enabled succeeds without unsupported-source error");if(expected_shape)same_shape(evaluate_shape(w.host.session.document(),"macro",evaluate(w.host.session.document())),*expected_shape);
 auto* group=w.findChild<QGroupBox*>("stack-operation-"+QString::fromStdString(op));check(group&&group->grab().save(path+"-controls.png"),"Actual supported/unsupported entry controls rendered");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,goal);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Immutable actual full native retains Macro enabled flag/pin/graph/public defaults/other entries");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_MACRO_ENABLED_SOURCE_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<5;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Macro enabled source cases failed: "+std::to_string(failures));std::cout<<"PASS Macro enabled source "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Macro enabled source "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
