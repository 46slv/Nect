#include "nect/io.hpp"
#ifndef NECT_COMPOSITE_ISOLATION_BATCH_CORE_ONLY
#include "host.hpp"
#include "composite_isolation_batch_control.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>
#include <memory>
#endif
#include <algorithm>
#include <iostream>
#include <stdexcept>

// The portable mode verifies fixtures and real Session/source preservation. It
// does not execute or qualify the Qt controls, Window routing or physical input.
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){
    try{action();}catch(const Error& error){check(error.code==code,"Expected exact refusal code");return;}
    throw std::runtime_error(std::string("Missing refusal: ")+code);
}
std::vector<Id> target_ids(){
    std::vector<Id> result={"first","second","third","instance"};
#ifdef _WIN32
    result.push_back("image");
#endif
    return result;
}
Object path(Id id){
    Object object;object.id=id;object.name="Same name";
    object.source=default_primitive(id+"-rectangle","nect.shape.rectangle");
    object.source->parameters.at("width").literal=132.25;
    auto fill=default_operation(id+"-fill","nect.paint.fill");fill.parameters.at("g").literal=.375;
    fill.composite="above";fill.fill_rule="evenodd";
    auto stroke=default_operation(id+"-stroke","nect.paint.stroke");stroke.parameters.at("width").literal=4.625;
    object.stack={fill,stroke};object.anchor[0].literal=27.25;object.transform[4].literal=13.5;
    return object;
}
Document fixture(bool value=false,bool mixed=false){
    auto document=empty_document("isolation-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::group;
    source.children={"source-path","source-text"};source.compositing.opacity.literal=.75;
    auto source_path=path("source-path");source_path.compositing.isolated=true;
    Object source_text;source_text.id="source-text";source_text.name="Source Text";source_text.kind=Kind::text;
    source_text.text=default_text("source-text-content","Source 日本語");
    Object first;first.id="first";first.name="Same name";first.kind=Kind::group;first.children={"first-child"};
    first.compositing.blend="multiply";first.compositing.isolated=value;
    first.compositing.opacity.binding=Binding{{"source","","composite.opacity"},.5,.125};
    first.compositing.mask=GeometryMask{"first-mask","source-path"};
    first.compositing.mask->enabled_expression=Expression{"true",1};
    first.visibility_driver=Ref{"source","","object.visible"};
    auto first_child=path("first-child");first_child.compositing.isolated=!value;
    first_child.stack.front().enabled_expression=Expression{"false",1};
    auto second=path("second");second.compositing.blend="screen";second.compositing.isolated=mixed?!value:value;
    second.compositing.opacity.expression=Expression{".625",1};second.transform_parent="source";
    second.transform[4].literal=19;second.anchor[0].literal=13.125;
    second.visibility_expression=Expression{"false",1};
    second.source->parameters.at("height").binding=Binding{{"source-path","","generator.height"},1.5,0};
    Object third;third.id="third";third.name="Same name";third.kind=Kind::text;
    third.compositing.blend="hue";third.compositing.isolated=value;
    third.text=default_text("third-text","Authored Text literal");
    third.text->content_driver=TextContentDriver{{"source-text","","text.content"}};
    third.text->family_driver=TextFamilyDriver{{"source-text","","text.family"}};
    third.text->locale="ja-JP";third.text->weight_expression=Expression{"500",1};
    third.text->italic_driver=TextItalicDriver{Expression{"true",1}};
    third.text->parameters.at("tracking").expression=Expression{"2",1};
    third.text->font_features={{"kern",1,"whole_text"}};
    third.text->additional_axis_values={{"wdth",87.1234567890123}};
    third.stack.push_back(default_operation("third-fill","nect.paint.fill"));
    third.transform[4].binding=Binding{{"second","","transform.tx"},.5,0};
    Object definition;definition.id="definition-root";definition.name="Definition source";definition.kind=Kind::group;
    definition.children={"definition-path"};definition.visible=false;
    auto definition_path=path("definition-path");
    Object instance;instance.id="instance";instance.name="Same name";instance.kind=Kind::instance;
    instance.instance=DefinitionInstance{"definition",{{{"definition-path","","generator.width"},88}},{{"definition-path",false}}};
    instance.compositing.opacity.literal=.25;instance.compositing.isolated=value;
    auto unrelated=path("unrelated");unrelated.compositing.isolated=true;
    unrelated.compositing.isolated_driver=Ref{"source","","composite.isolated"};
    for(const auto& object:{source,source_path,source_text,first,first_child,second,third,definition,definition_path,instance,unrelated})
        document.objects.emplace(object.id,object);
    document.definitions.emplace("definition",Definition{"definition","Definition","definition-root"});
    document.compositions.front().roots={"source","first","second","third","definition-root","instance","unrelated"};
    document.collections.push_back({"collection","Retained selection",{"first","second"}});
#ifdef _WIN32
    RasterPixels pixels{1,1,{32,64,128,255}};auto raster=make_raster(encode_raster_png(pixels));
    document.raster_assets.emplace("asset",RasterAsset{"asset","Source image","embedded","",raster});
    Object image;image.id="image";image.name="Same name";image.kind=Kind::image;
    image.image=ImageSource{"asset",{160},{120}};image.compositing.isolated=value;
    image.image->width.binding=Binding{{"second","","generator.width"},1.5,0};
    document.objects.emplace(image.id,image);document.compositions.front().roots.push_back(image.id);
#endif
    return document;
}
Document driven_fixture(){
    auto document=fixture(true);auto& first=document.objects.at("first").compositing;
    first.isolated_driver=Ref{"source","","composite.isolated"};
    auto& second=document.objects.at("second").compositing;second.isolated=false;
    second.isolated_expression=Expression{"!ref(\"first\",\"\",\"composite.isolated\")",1};
    auto& instance=document.objects.at("instance").compositing;instance.isolated=false;
    instance.isolated_driver=Ref{"second","","composite.isolated"};
    return document;
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());
    }
};
Document isolated(Document document,const std::vector<Id>& targets,bool value){
    for(const auto& id:targets)document.objects.at(id).compositing.isolated=value;
    return document;
}
Document frozen(Document document,const std::vector<Id>& targets){
    // Evaluate all expectations against the original snapshot, independently of
    // command order, so dependency freeze tests cannot mirror a sequential bug.
    const auto values=evaluate_composite_isolations(document);
    for(const auto& id:targets){
        auto& value=document.objects.at(id).compositing;
        if(value.isolated_driver||value.isolated_expression){value.isolated=values.at(id);value.isolated_driver.reset();value.isolated_expression.reset();}
    }
    return document;
}
std::vector<Command> isolation_commands(const Document& document,const std::vector<Id>& targets,bool value){
    std::vector<Command> result;result.reserve(targets.size());
    for(const auto& id:targets)result.push_back(SetCompositing{id,document.objects.at(id).compositing.blend,value});
    return result;
}
std::vector<Command> unlink_commands(const Document& document,const std::vector<Id>& targets){
    std::vector<Command> result;
    for(const auto& id:targets){const auto& value=document.objects.at(id).compositing;
        if(value.isolated_driver||value.isolated_expression)result.push_back(UnlinkCompositeIsolated{{id,"","composite.isolated"}});}
    return result;
}
Document large_fixture(std::size_t size){
    auto document=empty_document("large-isolation-document","composition","artboard");
    for(std::size_t i=0;i<size;++i){
        Object object;object.id="object-"+std::to_string(i);object.name="Same name";object.kind=Kind::group;
        object.compositing.isolated=i%2!=0;object.compositing.blend=i%2?"screen":"multiply";
        document.compositions.front().roots.push_back(object.id);document.objects.emplace(object.id,object);
    }
    return document;
}
void core_smoke(){
    const auto targets=target_ids();
    for(const bool value:{false,true}){
        Session session(fixture(!value,true));const Snapshot before(session);
        session.apply(isolation_commands(session.document(),targets,value),session.revision());
        const auto expected=isolated(before.document,targets,value);
        check(session.document()==expected&&session.revision()==before.revision+1&&session.history().states.size()==before.history.states.size()+1,
            "Literal isolation sets all retained mixed-kind targets in one Session transaction");
        check(decode(encode(session.document()))==expected,"Native roundtrip retains per-target blend, mask, opacity, unrelated drivers, hierarchy and instances");
        const auto committed=encode(session.document());session.undo(session.revision());
        check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),"One Undo restores the complete isolation batch");
        session.redo(session.revision());check(encode(session.document())==committed,"One Redo restores all isolation literals");
    }
    for(const bool reverse:{false,true}){
        auto selected=targets;if(reverse)std::reverse(selected.begin(),selected.end());
        Session session(driven_fixture());const Snapshot before(session);const auto values=evaluate_composite_isolations(before.document);
        check(!values.at("first")&&values.at("second")&&values.at("instance"),"Evaluated linked/expression values deliberately differ from authored fallback literals");
        session.apply(unlink_commands(before.document,selected),session.revision());const auto expected=frozen(before.document,selected);
        check(session.document()==expected&&session.revision()==1&&session.history().states.size()==before.history.states.size()+1,
            "Explicit unlink freezes each original evaluated value even when selected sources depend on each other in either order");
        check(evaluate_composite_isolations(session.document())==values&&decode(encode(session.document()))==expected,
            "Unlink preserves evaluated appearance and every other native source, including unselected dependents");
        const auto committed=encode(session.document());session.undo(session.revision());
        check(encode(session.document())==before.native&&!session.can_undo(),"One Undo restores all linked and expression isolation sources");
        session.redo(session.revision());check(encode(session.document())==committed,"One Redo restores the per-target frozen literals");
    }
    Session session(driven_fixture());const Snapshot before(session);
    rejects("DRIVEN_PROPERTY",[&]{session.apply({SetCompositing{"third","hue",false},SetCompositing{"first","multiply",false}},0);});
    check(before.unchanged(session),"A later driven isolation target refuses the entire literal transaction atomically");
    rejects("MISSING_REFERENCE",[&]{session.apply({UnlinkCompositeIsolated{{"first","","composite.isolated"}},UnlinkCompositeIsolated{{"missing","","composite.isolated"}}},0);});
    check(before.unchanged(session),"A later missing unlink target preserves earlier sources and fallback literals");
    rejects("PROPERTY_NOT_LINKED",[&]{session.apply({UnlinkCompositeIsolated{{"first","","composite.isolated"}},UnlinkCompositeIsolated{{"third","","composite.isolated"}}},0);});
    check(before.unchanged(session),"A later unlinked target refuses without partial source removal");
}

#ifndef NECT_COMPOSITE_ISOLATION_BATCH_CORE_ONLY
using namespace nect::desktop;
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets=target_ids();
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_composite_isolation_batch_controls(host,targets,&parent);}
    void load(Document document=fixture(),HistoryLimits limits={}){
        host.session=Session(std::move(document),limits);host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Isolation control exists");return found;
    }
    QCheckBox* editor(){return control<QCheckBox>("composite-isolation-batch-editor");}
    QPushButton* apply(){return control<QPushButton>("composite-isolation-batch-apply");}
    QPushButton* cancel(){return control<QPushButton>("composite-isolation-batch-cancel");}
    QPushButton* unlink(){return control<QPushButton>("composite-isolation-batch-unlink");}
    QString state(){return control<QLabel>("composite-isolation-batch-state")->text();}
    QString status(){return control<QLabel>("composite-isolation-batch-status")->text();}
    void choose(bool value){editor()->setCheckState(value?Qt::Checked:Qt::Unchecked);}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};
void shared_and_mixed_drafts(){
    for(const bool initial:{false,true}){
        Inspector inspector;inspector.load(fixture(initial));auto& session=inspector.host.session;const Snapshot before(session);
        check(inspector.state()==(initial?"Shared: On":"Shared: Off")&&inspector.editor()->checkState()==(initial?Qt::Checked:Qt::Unchecked)&&
            !inspector.editor()->isTristate()&&!inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled()&&!inspector.unlink()->isEnabled(),
            "Shared authored isolation starts with no pending draft or source removal");
        inspector.apply()->setEnabled(true);inspector.apply()->click();inspector.unlink()->setEnabled(true);inspector.unlink()->click();
        check(before.unchanged(session),"Forced unchanged Apply and source-free Unlink are history-free");
        inspector.editor()->click();check(before.unchanged(session)&&inspector.apply()->isEnabled()&&inspector.cancel()->isEnabled(),"Click chooses a literal draft without editing the document");
        inspector.cancel()->click();check(before.unchanged(session)&&inspector.editor()->checkState()==(initial?Qt::Checked:Qt::Unchecked)&&
            !inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),"Cancel restores Shared without revision, history or native changes");
        inspector.choose(!initial);inspector.choose(initial);check(before.unchanged(session)&&!inspector.apply()->isEnabled(),"Returning to Shared is a no-op");
        inspector.choose(!initial);QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
        inspector.apply()->click();const auto expected=isolated(before.document,inspector.targets,!initial);
        check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
            session.history().states.size()==before.history.states.size()+1,"Apply commits one isolation-only transaction and survives synchronous inspector destruction");
        check(decode(encode(session.document()))==expected,"Qt Apply preserves every other authored field and dependency through native readback");
        const auto committed=encode(session.document());inspector.undo();check(encode(session.document())==before.native&&!session.can_undo(),"One UI Undo restores the complete isolation batch");
        inspector.redo();check(encode(session.document())==committed,"One UI Redo restores all isolation literals");
    }
    for(const bool value:{false,true}){
        Inspector inspector;inspector.load(fixture(false,true));auto& session=inspector.host.session;const Snapshot before(session);
        check(inspector.state()=="Mixed"&&inspector.editor()->isTristate()&&inspector.editor()->checkState()==Qt::PartiallyChecked&&!inspector.apply()->isEnabled(),
            "Mixed is a presentation state with no implicit authored bool");
        inspector.apply()->setEnabled(true);inspector.apply()->click();check(before.unchanged(session),"Forced Mixed Apply cannot create history");
        QTest::keyClick(inspector.editor(),Qt::Key_Space);
        check(!inspector.editor()->isTristate()&&inspector.editor()->checkState()==Qt::Checked&&before.unchanged(session),"Space leaves Mixed for a definite On draft");
        QTest::keyClick(inspector.editor(),Qt::Key_Space);
        check(inspector.editor()->checkState()==Qt::Unchecked&&before.unchanged(session),"Space then chooses definite Off without authoring");
        inspector.choose(value);inspector.cancel()->click();
        check(inspector.editor()->isTristate()&&inspector.editor()->checkState()==Qt::PartiallyChecked&&!inspector.apply()->isEnabled()&&before.unchanged(session),
            "Cancel restores Mixed and each original literal");
        inspector.choose(value);inspector.apply()->click();
        check(session.document()==isolated(before.document,inspector.targets,value)&&session.revision()==1,"Either Mixed choice changes all exact targets, including already-equal ones");
        inspector.undo();check(session.document()==before.document&&inspector.state()=="Mixed","One Undo restores the original Mixed selection");
    }
}
void explicit_sources_and_freeze(){
    Inspector inspector;auto& session=inspector.host.session;inspector.load(driven_fixture());const Snapshot before(session);
    check(inspector.state()=="Mixed"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&inspector.unlink()->isEnabled()&&
        inspector.status().startsWith("DRIVEN_PROPERTY")&&inspector.status().contains("first"),"Any retained isolation source disables the whole literal edit and identifies its exact target");
    check(inspector.control<QLabel>("composite-isolation-batch-sources")->text().contains("3 of"),"Source count and independent freeze semantics are visible");
    inspector.editor()->setEnabled(true);inspector.choose(false);inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(before.unchanged(session)&&inspector.status().startsWith("DRIVEN_PROPERTY"),"Even a forced literal Apply cannot silently remove or overwrite sources");
    inspector.cancel()->setEnabled(true);inspector.cancel()->click();check(before.unchanged(session),"Cancelling a driven draft leaves every source intact");
    QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_unlink=inspector.unlink();inspector.unlink()->click();
    const auto expected=frozen(before.document,inspector.targets);
    check(!retired&&!retired_unlink&&session.document()==expected&&session.revision()==1&&session.history().states.size()==before.history.states.size()+1,
        "Explicit Unlink sources freezes only retained driven values in one transaction and survives inspector destruction");
    check(inspector.state()=="Mixed"&&inspector.editor()->isEnabled()&&!inspector.unlink()->isEnabled()&&!inspector.apply()->isEnabled()&&
        evaluate_composite_isolations(session.document())==evaluate_composite_isolations(before.document),"Frozen values remain independently Mixed with unchanged evaluated appearance");
    check(decode(encode(session.document()))==expected,"Unlink preserves all other native sources, fields and unselected dependents");
    const auto committed=encode(session.document());inspector.undo();check(encode(session.document())==before.native&&!session.can_undo()&&inspector.unlink()->isEnabled(),"One Undo restores all fallback literals and exact isolation sources");
    inspector.redo();check(encode(session.document())==committed,"One Redo restores the independent frozen values");
    inspector.choose(false);const Snapshot draft(session);inspector.cancel()->click();check(draft.unchanged(session),"Cancelling a later literal draft does not undo an explicit source unlink");
    inspector.choose(false);inspector.apply()->click();check(session.document()==isolated(expected,inspector.targets,false)&&session.revision()==4,
        "A later explicit literal Apply is a separate one-Undo transaction after source freeze");
    inspector.undo();check(session.document()==expected,"One Undo of literal Apply returns to the per-target frozen values");
    for(const bool expression:{false,true}){
        auto document=fixture(true);auto& isolation=document.objects.at("second").compositing;
        if(expression)isolation.isolated_expression=Expression{"false",1};else isolation.isolated_driver=Ref{"source","","composite.isolated"};
        inspector.load(std::move(document));const Snapshot linked(session);
        check(inspector.state()=="Mixed"&&inspector.editor()->checkState()==Qt::PartiallyChecked&&!inspector.editor()->isEnabled()&&inspector.unlink()->isEnabled(),
            "Equal stored On literals with different evaluated linked/expression values show Mixed and stay source-protected");
        check(linked.unchanged(session),"Evaluated Shared/Mixed presentation never changes stored fallback literals or sources");
        inspector.unlink()->click();check(session.document()==frozen(linked.document,inspector.targets)&&!session.document().objects.at("second").compositing.isolated,
            "Link and constant-expression sources each freeze the evaluated bool instead of their fallback literal");
    }
    auto shared=fixture(false);shared.objects.at("first").compositing.isolated=true;
    shared.objects.at("first").compositing.isolated_driver=Ref{"source","","composite.isolated"};
    inspector.load(std::move(shared));const Snapshot common(session);
    check(inspector.state()=="Shared: Off"&&inspector.editor()->checkState()==Qt::Unchecked&&!inspector.editor()->isEnabled()&&common.unchanged(session),
        "Different fallback literals with equal evaluated isolation show Shared Off without authoring");
}
void refused_selections(){
    Inspector inspector;auto& session=inspector.host.session;
    for(const auto& targets:std::vector<std::vector<Id>>{{},{"first"},{"first","first"},{"first","missing"},{"first","definition-path@instance"},std::vector<Id>(1001,"first")}){
        inspector.targets=targets;inspector.load();const Snapshot before(session);
        check(inspector.state()=="Unavailable"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&!inspector.unlink()->isEnabled()&&!inspector.status().isEmpty(),
            "Empty, singleton, duplicate, missing, virtual and over-limit targets refuse the whole selection");
        inspector.choose(true);inspector.apply()->setEnabled(true);inspector.apply()->click();inspector.unlink()->setEnabled(true);inspector.unlink()->click();
        check(before.unchanged(session),"An unavailable mixed valid/invalid selection never changes its valid subset");
    }
    inspector.targets=target_ids();inspector.load();const Snapshot dismissed(session);inspector.choose(true);delete inspector.controls.data();
    check(dismissed.unchanged(session),"Dismissing an unapplied draft never creates history");inspector.rebuild();
    check(inspector.editor()->checkState()==Qt::Unchecked&&!inspector.apply()->isEnabled(),"Reopening discards the retired draft");
}
void guards_and_retained_targets(){
    Inspector inspector;auto& session=inspector.host.session;
    for(const bool unlink:{false,true}){
        const auto load=[&]{inspector.load(unlink?driven_fixture():fixture());if(!unlink)inspector.choose(true);};
        const auto action=[&]{if(unlink)inspector.unlink()->click();else inspector.apply()->click();};
        load();session.apply({Rename{"source","Later API edit"}},session.revision());const Snapshot stale(session);action();
        check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Apply and Unlink both refuse a stale revision atomically");
        load();inspector.host.session_id+="-different";const Snapshot other_session(session);action();
        check(other_session.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Both actions refuse a replaced Host Session with identical IDs");
        load();auto replacement=unlink?driven_fixture():fixture();replacement.id="other-document";session=Session(replacement);const Snapshot other_document(session);action();
        check(other_document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Both actions independently guard same-revision document replacement");
        load();session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});const Snapshot gesture(session);const auto preview=session.preview_document();action();
        check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),"Both actions preserve an active gesture and preview");
        session.cancel_gesture();action();check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Cancelled gesture generation invalidates both retained actions");
        load();const Snapshot prior(session);session.begin_gesture(session.revision());session.cancel_gesture();action();
        check(prior.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"An empty begin/cancel gesture also invalidates the context");
        load();session.begin_gesture(session.revision());inspector.rebuild();const Snapshot active(session);
        check(inspector.state()=="Unavailable"&&inspector.status().startsWith("GESTURE_ACTIVE")&&!inspector.unlink()->isEnabled()&&active.unchanged(session),"Constructing during a gesture refuses immediately");session.cancel_gesture();
        inspector.load(unlink?driven_fixture():fixture(),{1024,128});if(!unlink)inspector.choose(true);const Snapshot history(session);action();
        check(history.unchanged(session)&&inspector.status().startsWith("HISTORY_LIMIT"),"History admission failure preserves all targets and sources for either action");
        if(!unlink){inspector.cancel()->click();check(history.unchanged(session)&&!inspector.apply()->isEnabled()&&inspector.status().isEmpty(),"A refused history draft remains cancellable without edits");}
        load();const Snapshot retained(session);const auto captured=inspector.targets;inspector.targets={"source","first-child"};action();
        check(session.document()==(unlink?frozen(retained.document,captured):isolated(retained.document,captured,true)),"Later caller selection cannot retarget Apply or Unlink");
        inspector.targets=target_ids();load();inspector.host.changed={};action();const Snapshot once(session);action();
        check(once.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"An unreconstructed panel cannot commit either action twice");inspector.host.changed=[&]{inspector.rebuild();};
    }
    inspector.targets={"first","first-child"};inspector.load();const Snapshot hierarchy(session);inspector.choose(true);inspector.apply()->click();
    check(session.document()==isolated(hierarchy.document,inspector.targets,true),"Selecting a Group and its child edits only both own isolation fields without flattening or changing ownership");
    inspector.undo();check(session.document()==hierarchy.document,"One Undo restores nested selected targets");
    auto large=large_fixture(1000);inspector.targets=large.compositions.front().roots;inspector.load(large);const Snapshot boundary(session);
    inspector.choose(true);inspector.apply()->click();check(session.document()==isolated(boundary.document,inspector.targets,true)&&session.revision()==1&&session.history().states.size()==boundary.history.states.size()+1,
        "Exactly 1000 retained objects commit completely in one blend-preserving transaction");
    inspector.undo();check(session.document()==boundary.document&&!session.can_undo(),"One Undo restores all 1000 targets");
}
void host_destruction_is_safe(){
    for(const bool driven:{false,true}){
        QTemporaryDir directory;QWidget parent;auto host=std::make_unique<Host>(directory.path());host->session=Session(driven?driven_fixture():fixture());
        QPointer<QWidget> controls=make_composite_isolation_batch_controls(*host,target_ids(),&parent);
        auto* editor=controls->findChild<QCheckBox*>("composite-isolation-batch-editor");
        auto* apply=controls->findChild<QPushButton*>("composite-isolation-batch-apply");
        auto* unlink=controls->findChild<QPushButton*>("composite-isolation-batch-unlink");
        if(!driven)editor->setCheckState(Qt::Checked);
        host.reset();
        check(controls&&!editor->isEnabled()&&!apply->isEnabled()&&!unlink->isEnabled(),"Retained controls deactivate safely when their Host is destroyed");
        apply->setEnabled(true);apply->click();unlink->setEnabled(true);unlink->click();
        check(controls->findChild<QLabel*>("composite-isolation-batch-status")->text().startsWith("SESSION_CONFLICT"),"Forced retained callbacks safely refuse both actions after Host teardown");
    }
}
#endif
}

#ifdef NECT_COMPOSITE_ISOLATION_BATCH_CORE_ONLY
int main(){
    try{core_smoke();std::cout<<"PASS "<<checks<<" source-bound isolation batch core smoke checks (Qt UI NOT_RUN; full regression NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{core_smoke();shared_and_mixed_drafts();explicit_sources_and_freeze();refused_selections();guards_and_retained_targets();host_destruction_is_safe();
        std::cout<<"PASS "<<checks<<" Composite isolation batch Qt/core checks (physical OS input NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#endif
