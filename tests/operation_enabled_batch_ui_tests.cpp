#include "host.hpp"
#include "operation_enabled_batch_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
using Target=OperationEnabledBatchTarget;
const std::vector<Target> default_targets={{"first","first-offset"},{"second","second-offset"}};

Object path(const Id& id,bool enabled){
    Object object;object.id=id;object.name="Same object name";
    object.source=default_primitive(id+"-rectangle","nect.shape.rectangle");
    object.source->parameters.at("width").literal=132.25;
    auto fill=default_operation(id+"-fill","nect.paint.fill");
    fill.parameters.at("g").literal=0.375;fill.composite="above";fill.fill_rule="evenodd";
    auto offset=default_operation(id+"-offset","nect.shape.offset");
    offset.enabled=enabled;offset.parameters.at("amount").literal=5.25;offset.line_join="round";
    auto stroke=default_operation(id+"-stroke","nect.paint.stroke");
    stroke.parameters.at("width").literal=4.625;
    object.stack={fill,offset,stroke};object.legacy_stroke=stroke.id;
    object.anchor[0].literal=27.25;object.transform[4].literal=13.5;
    return object;
}
Document fixture(bool first=true,bool second=true){
    auto document=empty_document("operation-document","composition","artboard");
    auto source=path("source",true);auto a=path("first",first);auto b=path("second",second);
    a.stack.at(1).parameters.at("amount").binding=Binding{operation_ref("source","source-offset","amount"),1.25,0.5};
    b.stack.at(1).parameters.at("amount").expression=Expression{"7.125",1};
    a.stack.front().enabled_driver=operation_ref("source","source-fill","enabled");
    b.stack.back().enabled_expression=Expression{"true",1};
    Gradient gradient;gradient.id="first-gradient";
    GradientStop begin;begin.id="first-gradient-begin";begin.offset.literal=0;
    GradientStop end;end.id="first-gradient-end";end.offset.literal=1;end.rgba[0].literal=0.8;
    gradient.stops={begin,end};gradient.start_x.literal=12.75;gradient.end_y.literal=117.5;
    a.stack.front().gradient=gradient;
    document.objects.emplace(source.id,std::move(source));document.objects.emplace(a.id,std::move(a));
    document.objects.emplace(b.id,std::move(b));
    Object unrelated;unrelated.id="unrelated";unrelated.name="Unrelated Text";unrelated.kind=Kind::text;
    unrelated.text=default_text("unrelated-text","日本語 retained text");
    unrelated.text->italic=true;unrelated.text->alignment="center";
    document.objects.emplace(unrelated.id,std::move(unrelated));
    document.compositions.front().roots={"source","first","second","unrelated"};return document;
}
Document enabled_document(Document document,const std::vector<Target>& targets,bool value){
    for(const auto& target:targets){
        auto& stack=document.objects.at(target.object).stack;
        auto operation=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==target.operation;});
        check(operation!=stack.end(),"Expected exact operation exists");operation->enabled=value;
    }
    return document;
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());
    }
};
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Target> targets=default_targets;
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_operation_enabled_batch_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Operation enabled control exists");return found;
    }
    QComboBox* editor(){return control<QComboBox>("operation-enabled-batch-editor");}
    QPushButton* apply(){return control<QPushButton>("operation-enabled-batch-apply");}
    QPushButton* cancel(){return control<QPushButton>("operation-enabled-batch-cancel");}
    QString state(){return control<QLabel>("operation-enabled-batch-state")->text();}
    QString status(){return control<QLabel>("operation-enabled-batch-status")->text();}
    void choose(bool value){editor()->setCurrentIndex(editor()->findData(value?"enable":"bypass"));}
    void undo(){host.session.undo(host.session.revision());host.edited();}
};

void common_choices_and_cancel(){
    for(const bool initial:{false,true}){
        Inspector inspector;inspector.load(fixture(initial,initial));auto& session=inspector.host.session;
        const Snapshot before(session);
        check(inspector.state()==(initial?"Common: Enabled":"Common: Bypassed")&&
            inspector.editor()->currentData().toString()==(initial?"enable":"bypass")&&!inspector.apply()->isEnabled(),
            "Common values present an exact enabled literal and unchanged Apply is disabled");
        inspector.apply()->click();inspector.choose(!initial);
        check(before.unchanged(session)&&inspector.apply()->isEnabled(),"Explicit Enable/Bypass is only a draft until Apply");
        inspector.cancel()->click();
        check(before.unchanged(session)&&inspector.editor()->currentData().toString()==(initial?"enable":"bypass")&&
            !inspector.apply()->isEnabled(),"Cancel restores the common choice without changing document, revision or history");
        inspector.choose(!initial);inspector.choose(initial);
        check(before.unchanged(session)&&!inspector.apply()->isEnabled(),"Returning to Common is a no-op");
        inspector.choose(!initial);QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
        inspector.apply()->click();const auto expected=enabled_document(before.document,default_targets,!initial);
        check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
            session.history().states.size()==before.history.states.size()+1,
            "Both directions use one Session transaction and tolerate synchronous Inspector destruction");
        check(decode(encode(session.document()))==expected,
            "Native roundtrip retains complete objects, ordered stacks, stable IDs, all parameters, sources, gradients and unrelated drivers");
        check(inspector.state()==(!initial?"Common: Enabled":"Common: Bypassed")&&!inspector.apply()->isEnabled(),
            "Committed Common state has no pending draft");
        const auto committed=encode(session.document());inspector.apply()->click();
        check(encode(session.document())==committed&&session.revision()==before.revision+1,
            "Repeated Apply does not create another history state");
        inspector.undo();check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
            "One Undo restores all original operation literals and complete native state");
        session.redo(session.revision());inspector.host.edited();
        check(encode(session.document())==committed,"One Redo restores the complete enabled batch");
    }
}
void mixed_is_presentation_only(){
    for(const bool value:{false,true}){
        Inspector inspector;inspector.load(fixture(false,true));auto& session=inspector.host.session;const Snapshot before(session);
        check(inspector.state()=="Mixed"&&inspector.editor()->currentText()=="Mixed"&&!inspector.apply()->isEnabled(),
            "Different authored enabled literals show Mixed without an implicit bool");
        inspector.editor()->setFocus();QTest::keyClick(inspector.editor(),Qt::Key_Down);
        check(inspector.editor()->currentData().toString()=="enable"&&before.unchanged(session)&&inspector.apply()->isEnabled(),
            "Keyboard activation resolves Mixed to definite Enable without committing");
        inspector.choose(value);inspector.cancel()->click();
        check(inspector.editor()->currentText()=="Mixed"&&!inspector.apply()->isEnabled()&&before.unchanged(session),
            "Cancel restores Mixed without touching history");
        inspector.choose(value);inspector.apply()->click();
        check(session.document()==enabled_document(before.document,default_targets,value)&&session.revision()==before.revision+1,
            "Explicit Enable and Bypass each affect every exact captured pair, including already-equal targets");
        inspector.undo();check(session.document()==before.document&&inspector.state()=="Mixed","One Undo restores the mixed literals");
    }
    Inspector inspector;const Snapshot before(inspector.host.session);inspector.editor()->setCurrentIndex(-1);
    check(!inspector.apply()->isEnabled()&&before.unchanged(inspector.host.session),"An unset draft cannot author enabled");
    inspector.apply()->click();inspector.choose(false);delete inspector.controls.data();
    check(before.unchanged(inspector.host.session),"Dismissing an unapplied draft leaves document and history unchanged");
}
void require_unavailable(Inspector& inspector,const char* code){
    const Snapshot before(inspector.host.session);
    check(inspector.state()=="Unavailable"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&
        inspector.status().startsWith(QString::fromLatin1(code)),"Unsupported selection visibly refuses the whole batch");
    inspector.apply()->click();inspector.cancel()->click();
    check(before.unchanged(inspector.host.session),"Refusal never changes an earlier valid target or unlinks a driver");
}
MacroDefinition macro_definition(){
    MacroDefinitionRevision revision;revision.revision=1;
    revision.input={"macro-input","local_paths_and_paint"};revision.output={"macro-output","local_paths_and_paint"};
    revision.nodes={{default_operation("macro-offset","nect.shape.offset"),"macro-offset-in","macro-offset-out"},
        {default_operation("macro-repeater","nect.shape.repeater"),"macro-repeater-in","macro-repeater-out"}};
    revision.edges={{{"","macro-input"},{"macro-offset","macro-offset-in"}},
        {{"macro-offset","macro-offset-out"},{"macro-repeater","macro-repeater-in"}},
        {{"macro-repeater","macro-repeater-out"},{"","macro-output"}}};
    revision.output_mapping={"macro-repeater","macro-repeater-out"};
    revision.public_parameters.push_back({"macro.offset.amount","Amount","macro-offset","amount",
        "number","du","local_paths_and_paint"});
    MacroDefinition definition;definition.id="macro-definition";definition.label="Macro";definition.revisions.emplace(1,std::move(revision));
    return definition;
}
void refused_targets_and_drivers(){
    Inspector inspector;
    for(const auto& targets:std::vector<std::vector<Target>>{
            {},{{"first","first-offset"}},{{"first","first-offset"},{"first","first-offset"}},
            {{"first","first-offset"},{"missing","second-offset"}},
            {{"first","first-offset"},{"second","missing"}},
            {{"first","first-offset"},{"second","second-fill"}},
            {{"first","first-offset"},{"second","second-rectangle"}},
            {{"first","first-offset"},{"unrelated","unrelated-text"}}}){
        inspector.targets=targets;inspector.load();
        const auto expected=targets.size()<2||(targets.size()>1&&targets.front()==targets.back())?"INVALID_SELECTION":
            targets.back().object=="missing"?"MISSING_OBJECT":
            targets.back().operation=="second-fill"?"TYPE_MISMATCH":"MISSING_OPERATION";
        require_unavailable(inspector,expected);
    }
    inspector.targets=default_targets;
    auto reordered=fixture();std::swap(reordered.objects.at("second").stack.at(1),reordered.objects.at("second").stack.at(2));
    inspector.load(std::move(reordered));require_unavailable(inspector,"TYPE_MISMATCH");
    auto macro=fixture();macro.macro_definitions.emplace("macro-definition",macro_definition());
    ProcessingEntry entry;entry.id="second-offset";entry.type=macro_entry_type;entry.macro=MacroInstance{"macro-definition",1,{}};
    macro.objects.at("second").stack.at(1)=std::move(entry);
    inspector.load(std::move(macro));require_unavailable(inspector,"TYPE_MISMATCH");
    for(const bool expression:{false,true}){
        auto driven=fixture();auto& target=driven.objects.at("second").stack.at(1);
        if(expression)target.enabled_expression=Expression{"false",1};
        else target.enabled_driver=operation_ref("source","source-offset","enabled");
        inspector.load(std::move(driven));require_unavailable(inspector,"DRIVEN_PROPERTY");
        check(inspector.status().contains("second / second-offset"),"Driver refusal identifies the exact authored pair");
    }
    inspector.targets={{"first","first-stroke"},{"second","second-stroke"}};
    auto versions=fixture();versions.objects.at("second").stack.back().enabled_expression.reset();
    auto& stroke=versions.objects.at("second").stack.back();stroke.version=2;stroke.parameters.emplace("miter_limit",Scalar{4,{}});
    inspector.load(std::move(versions));require_unavailable(inspector,"TYPE_MISMATCH");
}
void stale_context_and_captured_selection(){
    Inspector inspector;auto& session=inspector.host.session;
    inspector.choose(false);session.apply({Rename{"source","Later API edit"}},session.revision());const Snapshot stale(session);
    inspector.apply()->click();check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Stale revision refuses atomically");
    inspector.load();inspector.choose(false);inspector.host.session_id+="-different";const Snapshot other_session(session);
    inspector.apply()->click();check(other_session.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Stale Session refuses with identical target IDs");
    inspector.load();inspector.choose(false);auto replacement=fixture();replacement.id="replacement-document";
    session=Session(std::move(replacement));const Snapshot other_document(session);
    inspector.apply()->click();check(other_document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Same-revision document replacement refuses");
    inspector.load();inspector.choose(false);session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),
        "An active gesture and its preview survive refused Apply");
    session.cancel_gesture();inspector.apply()->click();
    check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Cancelled gesture invalidates the old draft");
    inspector.load();session.begin_gesture(session.revision());inspector.rebuild();const Snapshot initial_gesture(session);
    require_unavailable(inspector,"GESTURE_ACTIVE");session.cancel_gesture();
    check(initial_gesture.unchanged(session),"Constructing during a gesture never authors a bool");
    inspector.load();const Snapshot retained(session);inspector.choose(false);
    inspector.targets={{"source","source-offset"},{"first","first-offset"}};inspector.apply()->click();
    check(session.document()==enabled_document(retained.document,default_targets,false)&&
        session.document().objects.at("source").stack.at(1).enabled,
        "Later caller selection never retargets the captured exact pairs");
}
void supported_builtin_domains_and_bounds(){
    Inspector inspector;
    for(const auto& descriptor:builtin_operation_types()){
        auto document=empty_document("builtins-document","composition","artboard");std::vector<Target> targets;
        for(const auto* id:{"first","second"}){
            Object object;object.id=id;object.name="Same object name";
            if(descriptor.target_kind=="group")object.kind=Kind::group;
            else object.source=default_primitive(object.id+"-rectangle","nect.shape.rectangle");
            object.stack.push_back(default_operation(object.id+"-operation",descriptor.type));
            document.compositions.front().roots.push_back(object.id);targets.push_back({object.id,object.stack.front().id});
            document.objects.emplace(object.id,std::move(object));
        }
        inspector.targets=targets;inspector.load(std::move(document));const Snapshot before(inspector.host.session);
        inspector.choose(false);inspector.apply()->click();
        check(inspector.host.session.document()==enabled_document(before.document,targets,false)&&inspector.host.session.revision()==1,
            "Every supported built-in can be bypassed with its original domain and payload retained");
        inspector.undo();check(before.document==inspector.host.session.document(),"One Undo restores each built-in domain batch");
    }
    auto mixed_sources=fixture();auto& text=mixed_sources.objects.at("second");
    text.kind=Kind::text;text.source.reset();text.text=default_text("second-text","Retained Text source 日本語");
    inspector.targets=default_targets;inspector.load(std::move(mixed_sources));const Snapshot mixed_before(inspector.host.session);
    inspector.choose(false);inspector.apply()->click();
    check(inspector.host.session.document()==enabled_document(mixed_before.document,default_targets,false),
        "Compatible Path and Text targets retain their original source types without conversion");
    inspector.undo();check(inspector.host.session.document()==mixed_before.document,"One Undo restores compatible mixed source kinds");
    auto document=empty_document("large-document","composition","artboard");std::vector<Target> targets;
    for(int index=0;index<1000;++index){
        const auto id="object-"+std::to_string(index);Object object;object.id=id;object.name="Repeated name";
        object.source=default_primitive(id+"-rectangle","nect.shape.rectangle");object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
        document.compositions.front().roots.push_back(id);targets.push_back({id,id+"-fill"});document.objects.emplace(id,std::move(object));
    }
    inspector.targets=targets;inspector.load(std::move(document));const Snapshot before(inspector.host.session);
    inspector.choose(false);inspector.apply()->click();
    check(inspector.host.session.document()==enabled_document(before.document,targets,false)&&inspector.host.session.revision()==1&&
        inspector.host.session.history().states.size()==before.history.states.size()+1,
        "Exactly 1000 distinct authored targets commit completely in one transaction");
    inspector.undo();check(inspector.host.session.document()==before.document,"One Undo restores all 1000 targets");
    inspector.targets.push_back({"first","first-offset"});inspector.rebuild();require_unavailable(inspector,"INVALID_SELECTION");
}
void host_destruction_is_safe(){
    QTemporaryDir directory;QWidget parent;auto host=std::make_unique<Host>(directory.path());
    host->session=Session(fixture());QPointer<QWidget> controls=make_operation_enabled_batch_controls(*host,default_targets,&parent);
    auto* editor=controls->findChild<QComboBox*>("operation-enabled-batch-editor");
    auto* apply=controls->findChild<QPushButton*>("operation-enabled-batch-apply");
    editor->setCurrentIndex(editor->findData("bypass"));check(apply->isEnabled(),"Host teardown fixture has a pending draft");
    host.reset();apply->click();
    check(controls->findChild<QLabel*>("operation-enabled-batch-status")->text().startsWith("SESSION_CONFLICT"),
        "A retained control safely refuses after its Host is destroyed");
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{common_choices_and_cancel();mixed_is_presentation_only();refused_targets_and_drivers();
        stale_context_and_captured_selection();supported_builtin_domains_and_bounds();host_destruction_is_safe();
        std::cout<<"PASS "<<checks<<" operation enabled batch Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
