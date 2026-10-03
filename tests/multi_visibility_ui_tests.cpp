#include "host.hpp"
#include "multi_visibility_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWidget>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
Object rectangle(const Id& id){
    Object object;object.id=id;object.name="Same name";
    object.source=default_primitive(id+"-primitive","nect.shape.rectangle");
    object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));return object;
}
Document fixture(bool first_visible=true,bool second_visible=true){
    auto document=empty_document("multi-visibility-document","composition","artboard");
    auto driver=rectangle("driver");driver.visible=false;driver.transform[4].literal=23.125;
    auto first=rectangle("first");first.visible=first_visible;
    first.source->parameters.at("width").expression=Expression{"107.125",1};
    first.transform[4].binding=Binding{{"driver","","transform.tx"},1.25,13.5};
    first.transform[5].expression=Expression{"17.271234567890123",1};
    first.anchor[0].literal=37.1234567890123;first.anchor[1].literal=18.25;
    first.compositing.opacity.expression=Expression{"0.75",1};first.compositing.blend="multiply";
    first.compositing.mask=GeometryMask{"first-mask","driver"};
    Object second;second.id="second";second.name="Same name";second.kind=Kind::group;
    second.visible=second_visible;second.children={"child"};second.compositing.blend="screen";
    second.compositing.isolated=true;
    auto child=rectangle("child");child.visibility_expression=Expression{"!ref(\"driver\",\"\",\"object.visible\")",1};
    auto unrelated=rectangle("unrelated");unrelated.visibility_driver=Ref{"driver","","object.visible"};
    Object definition_root;definition_root.id="definition-root";definition_root.name="Source";
    definition_root.kind=Kind::group;definition_root.children={"definition-child"};
    auto definition_child=rectangle("definition-child");definition_child.visible=false;
    for(const auto& object:{driver,first,second,child,unrelated,definition_root,definition_child})
        document.objects.emplace(object.id,object);
    document.compositions.front().roots={"driver","first","second","unrelated","definition-root"};
    Session session(std::move(document));
    session.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",definition_root.id}}},
        DefinitionCommand{CreateInstance{"composition","","instance","definition","Occurrence"}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance","definition-child",true}}},0);
    return session.document();
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());
    }
};
Document visibility(Document document,const std::vector<Id>& targets,bool visible){
    for(const auto& id:targets)document.objects.at(id).visible=visible;return document;
}
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets={"first","second"};
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_multi_visibility_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Visibility control exists");return found;
    }
    QComboBox* editor(){return control<QComboBox>("multi-visibility-editor");}
    QPushButton* apply(){return control<QPushButton>("multi-visibility-apply");}
    QPushButton* cancel(){return control<QPushButton>("multi-visibility-cancel");}
    QString state(){return control<QLabel>("multi-visibility-state")->text();}
    QString status(){return control<QLabel>("multi-visibility-status")->text();}
    void choose(bool visible){const int index=editor()->findData(visible?"show":"hide");check(index>=0,"Explicit Show/Hide choice exists");editor()->setCurrentIndex(index);}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};

void common_choices_and_cancel(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    check(inspector.state()=="Common: Shown"&&inspector.editor()->currentData().toString()=="show"&&
        !inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),"Common true is explicit and starts without a draft");
    inspector.apply()->click();inspector.choose(false);
    check(before.unchanged(session)&&inspector.apply()->isEnabled()&&inspector.cancel()->isEnabled(),
        "Hide is a local draft with no source or History changes");
    inspector.cancel()->click();
    check(before.unchanged(session)&&inspector.editor()->currentData().toString()=="show"&&!inspector.apply()->isEnabled(),
        "Cancel restores Common Show without creating history");
    inspector.choose(false);inspector.choose(true);
    check(before.unchanged(session)&&!inspector.apply()->isEnabled(),"Returning to the common value is a no-op");
    inspector.editor()->setCurrentIndex(-1);inspector.apply()->click();
    check(before.unchanged(session)&&!inspector.apply()->isEnabled(),"An unspecified draft cannot author visibility");
    inspector.choose(false);QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
    inspector.apply()->click();const auto expected=visibility(before.document,{"first","second"},false);
    check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "Hide changes both exact objects in one transaction and safely survives synchronous Inspector destruction");
    check(decode(encode(session.document()))==expected,
        "Only visibility changes; transforms, masks, links, expressions, paint, hierarchy, Definition and local Instance overrides survive native readback");
    check(inspector.state()=="Common: Hidden"&&!inspector.apply()->isEnabled(),"The rebuilt Inspector has no pending draft");
    const auto hidden=encode(session.document());inspector.undo();
    check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
        "One Undo restores every original source and exact native state");
    inspector.redo();check(encode(session.document())==hidden,"One Redo restores the visibility batch");
    inspector.choose(true);inspector.apply()->click();
    check(session.document()==before.document&&inspector.state()=="Common: Shown","Show also authors one explicit true batch");
}

void mixed_and_captured_targets(){
    for(const bool visible:{false,true}){
        Inspector inspector;inspector.load(fixture(false,true));auto& session=inspector.host.session;const Snapshot before(session);
        check(inspector.state()=="Mixed"&&inspector.editor()->currentText()=="Mixed"&&
            inspector.editor()->currentData().toString().isEmpty()&&!inspector.apply()->isEnabled(),
            "Mixed is presentation and has no implicit authored value");
        inspector.apply()->click();inspector.choose(visible);
        check(before.unchanged(session)&&inspector.apply()->isEnabled(),"Either absolute choice stays a draft until Apply");
        inspector.cancel()->click();
        check(before.unchanged(session)&&inspector.editor()->currentText()=="Mixed"&&!inspector.apply()->isEnabled(),
            "Cancel restores Mixed and every original literal");
        inspector.choose(visible);inspector.apply()->click();
        check(session.document()==visibility(before.document,{"first","second"},visible)&&session.revision()==1,
            "Show and Hide both update the captured mixed-kind targets, including an already-equal target");
        inspector.undo();check(session.document()==before.document&&inspector.state()=="Mixed","One Undo restores mixed source visibility");
    }
    Inspector inspector;const Snapshot retained(inspector.host.session);inspector.choose(false);
    inspector.targets={"driver","first"};inspector.apply()->click();
    check(inspector.host.session.document()==visibility(retained.document,{"first","second"},false),
        "Later caller selection cannot retarget the copied stable object IDs");
    inspector.targets={"first","second"};inspector.load();const Snapshot dismissed(inspector.host.session);
    inspector.choose(false);delete inspector.controls.data();
    check(dismissed.unchanged(inspector.host.session),"Dismissing an unapplied draft leaves source and History unchanged");
}

void authored_objects_and_ancestors(){
    Inspector inspector;inspector.targets={"first","instance"};inspector.rebuild();
    const Snapshot before(inspector.host.session);inspector.choose(false);inspector.apply()->click();
    check(inspector.host.session.document()==visibility(before.document,{"first","instance"},false)&&
        inspector.host.session.document().objects.at("instance").instance->visibility_overrides==
        before.document.objects.at("instance").instance->visibility_overrides,
        "Selecting an Instance edits the authored instance object's own bool and preserves descendant overrides and Definition sources");
    inspector.undo();check(inspector.host.session.document()==before.document,"One Undo restores authored Instance visibility");
    auto document=fixture(false,false);document.objects.at("child").visibility_expression.reset();document.objects.at("child").visible=false;
    inspector.targets={"first","child"};inspector.load(std::move(document));const Snapshot ancestors(inspector.host.session);
    inspector.choose(true);inspector.apply()->click();
    check(inspector.host.session.document()==visibility(ancestors.document,{"first","child"},true)&&
        !inspector.host.session.document().objects.at("second").visible,
        "Show edits own bools only and preserves the unselected hidden ancestor");
    check(inspector.editor()->toolTip().contains("Hidden ancestors"),"The control discloses ancestor suppression");
}

void invalid_and_driven_targets(){
    Inspector inspector;auto& session=inspector.host.session;
    std::vector<Id> excessive;for(int n=0;n<1001;++n)excessive.push_back("target-"+std::to_string(n));
    for(const auto& targets:std::vector<std::vector<Id>>{{},{"first"},{"first","first"},{"first","missing"},excessive}){
        inspector.targets=targets;inspector.load();const Snapshot before(session);
        check(inspector.state()=="Unavailable"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&!inspector.status().isEmpty(),
            "Empty, single, duplicate, missing and excessive selections reject the whole batch without filtering");
        inspector.apply()->click();inspector.cancel()->click();check(before.unchanged(session),"An unavailable selection cannot change source or History");
    }
    inspector.targets={"first","second"};
    for(const bool expression:{false,true}){
        auto document=fixture();
        if(expression)document.objects.at("second").visibility_expression=Expression{"true",1};
        else document.objects.at("second").visibility_driver=Ref{"driver","","object.visible"};
        inspector.load(std::move(document));const Snapshot before(session);
        check(inspector.status().startsWith("DRIVEN_PROPERTY")&&inspector.status().contains("second")&&
            !inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled(),
            "A link or expression refuses the complete selection and identifies its exact driven target");
        inspector.choose(false);inspector.apply()->click();
        check(before.unchanged(session),"A batch never clears a visibility source implicitly or edits an earlier literal target");
    }
    inspector.load();inspector.choose(false);auto replacement=fixture();
    replacement.objects.at("second").visibility_expression=Expression{"false",1};session=Session(std::move(replacement));
    const Snapshot changed_driver(session);inspector.apply()->click();
    check(changed_driver.unchanged(session)&&inspector.status().startsWith("DRIVEN_PROPERTY"),
        "The captured targets are revalidated immediately before mutation, even at the same document ID and revision");
    inspector.load();inspector.choose(false);replacement=fixture();
    replacement.objects.erase("second");replacement.objects.erase("child");
    std::erase(replacement.compositions.front().roots,Id{"second"});session=Session(std::move(replacement));
    const Snapshot missing_target(session);inspector.apply()->click();
    check(missing_target.unchanged(session)&&inspector.status().startsWith("MISSING_OBJECT"),
        "A captured object removed before Apply refuses without editing an earlier remaining object");
}

void stale_and_gesture_guards(){
    Inspector inspector;auto& session=inspector.host.session;
    inspector.choose(false);session.apply({Rename{"driver","Later edit"}},session.revision());const Snapshot stale(session);
    inspector.apply()->click();check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A stale revision refuses atomically");
    inspector.cancel()->click();check(stale.unchanged(session)&&!inspector.apply()->isEnabled()&&inspector.status().startsWith("REVISION_CONFLICT"),
        "Cancel can discard a stale draft while preserving the intervening source and History");
    inspector.load();inspector.choose(false);inspector.host.session_id+="-other";const Snapshot other_session(session);
    inspector.apply()->click();check(other_session.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"A stale Host Session refuses even with identical object IDs");
    inspector.load();inspector.choose(false);auto replacement=fixture();replacement.id="other-document";session=Session(std::move(replacement));
    const Snapshot other_document(session);inspector.apply()->click();
    check(other_document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Same-revision document replacement refuses");
    inspector.load();inspector.choose(false);session.begin_gesture(session.revision());session.update_gesture({Rename{"driver","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),
        "The active gesture and its preview survive a rejected visibility batch");
    session.cancel_gesture();inspector.apply()->click();
    check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A cancelled gesture invalidates the old draft context");
    inspector.load();session.begin_gesture(session.revision());inspector.rebuild();const Snapshot active_capture(session);
    check(inspector.state()=="Unavailable"&&inspector.status().startsWith("GESTURE_ACTIVE")&&!inspector.editor()->isEnabled(),
        "Controls captured during an active gesture are unavailable");
    session.cancel_gesture();inspector.cancel()->click();check(active_capture.unchanged(session),"Cancellation does not commit a gesture-captured control");
}

void maximum_selection(){
    Inspector inspector;auto document=empty_document("maximum-document","composition","artboard");inspector.targets.clear();
    for(int n=0;n<1000;++n){
        Object object;object.id="target-"+std::to_string(n);object.kind=Kind::group;object.visible=n%2==0;
        document.compositions.front().roots.push_back(object.id);inspector.targets.push_back(object.id);document.objects.emplace(object.id,object);
    }
    inspector.load(std::move(document));const Snapshot before(inspector.host.session);
    check(inspector.state()=="Mixed"&&inspector.editor()->isEnabled(),"The maximum 1000 unique authored objects are accepted");
    inspector.choose(false);inspector.apply()->click();
    check(inspector.host.session.document()==visibility(before.document,inspector.targets,false)&&inspector.host.session.revision()==1&&
        inspector.host.session.history().states.size()==before.history.states.size()+1,"The maximum selection commits once without truncation");
    inspector.undo();check(inspector.host.session.document()==before.document&&encode(inspector.host.session.document())==before.native,
        "One Undo restores all 1000 original literals exactly");
}

void host_destruction(){
    QTemporaryDir directory;QWidget parent;auto host=std::make_unique<Host>(directory.path());host->session=Session(fixture());
    auto* controls=make_multi_visibility_controls(*host,{"first","second"},&parent);
    auto* editor=controls->findChild<QComboBox*>("multi-visibility-editor");
    auto* apply=controls->findChild<QPushButton*>("multi-visibility-apply");
    auto* cancel=controls->findChild<QPushButton*>("multi-visibility-cancel");
    auto* status=controls->findChild<QLabel*>("multi-visibility-status");
    check(editor&&apply&&cancel&&status,"Host lifetime fixture controls exist");editor->setCurrentIndex(editor->findData("hide"));
    check(apply->isEnabled(),"The lifetime fixture has a pending draft");QPointer<QWidget> surviving=controls;host.reset();
    check(surviving&&!editor->isEnabled()&&!apply->isEnabled()&&!cancel->isEnabled()&&status->text().startsWith("SESSION_CONFLICT"),
        "Destroying Host safely invalidates controls owned by a surviving parent");
    editor->setCurrentIndex(editor->findData("show"));apply->click();cancel->click();
    check(surviving&&status->text().startsWith("SESSION_CONFLICT"),"Programmatic draft changes and later clicks never dereference a destroyed Host");
}
}

int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{
        common_choices_and_cancel();mixed_and_captured_targets();authored_objects_and_ancestors();invalid_and_driven_targets();
        stale_and_gesture_guards();maximum_selection();host_destruction();
        std::cout<<"PASS "<<checks<<" multi-object visibility Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
