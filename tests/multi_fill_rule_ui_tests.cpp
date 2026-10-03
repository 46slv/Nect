#include "host.hpp"
#include "multi_fill_rule_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
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
const std::vector<MultiFillRuleTarget> fill_targets{{"first","first-fill"},{"second","other-fill"}};
const Ref source_ref=operation_ref("source","source-fill","fill_rule");
const Ref alternate_ref=operation_ref("alternate","alternate-fill","fill_rule");
Gradient gradient(const Id& id){
    Gradient value;value.id=id;GradientStop start;start.id=id+"-start";
    GradientStop end;end.id=id+"-end";end.offset.literal=1;end.rgba[0].literal=.8;
    value.stops={start,end};return value;
}
Object source_object(const Id& id,const std::string& rule){
    Object object;object.id=id;object.name="Same visible name";object.kind=Kind::path;
    object.source=default_primitive(id+"-shape","nect.shape.rectangle");
    auto fill=default_operation(id+"-fill","nect.paint.fill");fill.fill_rule=rule;object.stack={fill};return object;
}
Document fixture(bool mixed=false,bool driven=false,bool text=false){
    auto document=empty_document("fill-rule-document","composition","artboard");
    document.objects.emplace("source",source_object("source","evenodd"));
    document.objects.emplace("alternate",source_object("alternate","nonzero"));
    for(std::size_t index=0;index<fill_targets.size();++index){
        const auto& target=fill_targets[index];auto object=source_object(target.object,"nonzero");
        object.transform[4].literal=index==0?11.123456789:211.987654321;
        object.compositing.opacity.literal=.375;object.compositing.blend=index==0?"multiply":"screen";
        auto offset=default_operation(target.object+"-offset","nect.shape.offset");offset.parameters.at("amount").literal=3.25;
        offset.fill_rule="evenodd";offset.line_join="round";
        auto fill=default_operation(target.operation,"nect.paint.fill");
        fill.composite=index==0?"above":"below";fill.fill_rule=mixed&&index==1?"evenodd":"nonzero";
        fill.parameters.at("r").literal=.123456789012345;fill.parameters.at("g").expression=Expression{"0.43210987654321",1};
        fill.parameters.at("b").binding=Binding{{"source","","op.source-fill.b"},.75,.125};
        fill.parameters.at("a").literal=.87654321098765;
        fill.enabled_driver=operation_ref("source","source-fill","enabled");
        fill.gradient=gradient(target.object+"-gradient");fill.gradient->enabled=index!=0;
        auto unrelated=default_operation(target.object+"-unrelated","nect.paint.fill");
        unrelated.fill_rule="evenodd";unrelated.composite="above";
        object.stack={offset,fill,unrelated};
        if(driven)object.stack[1].fill_rule_driver=FillRuleDriver{index==0?source_ref:alternate_ref};
        if(text&&index==1){
            object.kind=Kind::text;object.source.reset();object.text=default_text("text-source","Text 日本語");
            object.text->family="Arial";object.text->weight=650;object.text->italic=true;
            // Real font contours need not satisfy Offset's simple-region domain.
            // Retain its exact source/slot while testing the supported Text Fill.
            object.stack[0].enabled=false;
        }
        document.objects.emplace(object.id,std::move(object));
    }
    // The source picker must find nested children through the owning Composition.
    Object group;group.id="source-folder";group.name="Sources";group.kind=Kind::group;group.children={"source","alternate"};
    document.objects.emplace(group.id,std::move(group));
    document.compositions.front().roots={"source-folder","first","second"};return document;
}
ProcessingEntry& operation(Document& document,const MultiFillRuleTarget& target){
    auto& stack=document.objects.at(target.object).stack;
    const auto found=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==target.operation;});
    if(found==stack.end())throw std::runtime_error("Expected retained Fill operation is missing");return *found;
}
const ProcessingEntry& operation(const Document& document,const MultiFillRuleTarget& target){
    const auto& stack=document.objects.at(target.object).stack;
    const auto found=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==target.operation;});
    if(found==stack.end())throw std::runtime_error("Expected retained Fill operation is missing");return *found;
}
Document ruled(Document document,const std::string& rule){
    for(const auto& target:fill_targets)operation(document,target).fill_rule=rule;return document;
}
Document linked(Document document,const Ref& source){
    for(const auto& target:fill_targets)operation(document,target).fill_rule_driver=FillRuleDriver{source};return document;
}
Document unlinked(Document document){
    // Snapshot each evaluated value before changing any target's source.
    std::vector<std::string> values;for(const auto& target:fill_targets)values.push_back(evaluate_fill_rule(document,target.ref()));
    for(std::size_t index=0;index<fill_targets.size();++index){
        auto& fill=operation(document,fill_targets[index]);fill.fill_rule=values[index];fill.fill_rule_driver.reset();
    }
    return document;
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()),native(encode(session.document())){}
    bool same(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());
    }
};
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<MultiFillRuleTarget> targets=fill_targets;
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_multi_fill_rule_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Fill rule control exists");return found;
    }
    QComboBox* value(){return control<QComboBox>("multi-fill-rule-value");}
    QComboBox* mode(){return control<QComboBox>("multi-fill-rule-mode");}
    QComboBox* sources(){return control<QComboBox>("multi-fill-rule-source");}
    QLineEdit* search(){return control<QLineEdit>("multi-fill-rule-source-search");}
    QCheckBox* replace(){return control<QCheckBox>("multi-fill-rule-replace-driver");}
    QPushButton* apply(){return control<QPushButton>("multi-fill-rule-apply");}
    QPushButton* cancel(){return control<QPushButton>("multi-fill-rule-cancel");}
    QString state(){return control<QLabel>("multi-fill-rule-state")->text();}
    QString status(){return control<QLabel>("multi-fill-rule-status")->text();}
    void choose_value(const char* value_name){
        const auto index=value()->findData(QString::fromLatin1(value_name));check(index>=0,"Canonical Fill rule choice exists");value()->setCurrentIndex(index);
    }
    void choose_mode(const char* mode_name){
        const auto index=mode()->findData(QString::fromLatin1(mode_name));check(index>=0,"Fill rule mode exists");mode()->setCurrentIndex(index);
    }
    int source_index(const Id& object,const Id& fill){
        for(int index=0;index<sources()->count();++index)
            if(sources()->itemText(index).endsWith(" — "+QString::fromStdString(object))&&
                sources()->itemText(index).contains("["+QString::fromStdString(fill)+"]"))return index;
        return -1;
    }
    void choose_source(const Id& object,const Id& fill){
        const auto index=source_index(object,fill);check(index>=0,"Requested stable Fill source is visible");sources()->setCurrentIndex(index);
    }
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};

void literal_shared_mixed_cancel(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    check(inspector.state()=="Shared: Nonzero winding"&&inspector.value()->currentData().toString()=="nonzero",
        "Shared evaluated rule displays its canonical value");
    check(!inspector.apply()->isEnabled(),"An untouched shared rule is history-free");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(before.same(session),"Apply independently skips literal no-op history");
    inspector.choose_value("evenodd");check(before.same(session)&&inspector.apply()->isEnabled(),"Changing a rule only creates a draft");
    inspector.cancel()->click();check(before.same(session)&&inspector.value()->currentData().toString()=="nonzero"&&
        !inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),"Cancel restores a shared draft without history");
    inspector.choose_value("evenodd");QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
    inspector.apply()->click();const auto expected=ruled(before.document,"evenodd");
    check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,"Literal Apply is one atomic Undo and tolerates synchronous Inspector destruction");
    check(decode(encode(session.document()))==expected,"Fill IDs, order, RGBA drivers, gradients, composite and all unrelated state survive native readback");
    inspector.undo();check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),"One Undo restores every retained target exactly");
    inspector.redo();check(session.document()==expected&&inspector.state()=="Shared: Even-odd","One Redo restores the whole rule batch");

    inspector.load(fixture(true));const Snapshot mixed(session);
    check(inspector.state()=="Mixed"&&inspector.value()->currentText()=="Mixed"&&inspector.value()->currentData().toString().isEmpty()&&
        !inspector.apply()->isEnabled(),"Mixed is an explicit non-editing state");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(mixed.same(session),"Untouched Mixed Apply is history-free");
    inspector.choose_value("nonzero");inspector.cancel()->click();check(mixed.same(session)&&inspector.value()->currentText()=="Mixed","Cancel preserves every distinct mixed value");
    inspector.choose_value("evenodd");inspector.apply()->click();check(session.document()==ruled(mixed.document,"evenodd")&&session.revision()==1,"Mixed literal Apply sets all retained targets together");
    inspector.undo();check(mixed.document==session.document()&&inspector.state()=="Mixed","Undo restores each target's original rule");
    const Snapshot dismissed(session);inspector.choose_value("nonzero");delete inspector.controls.data();check(dismissed.same(session),"Panel dismissal discards its draft without history");

    inspector.load(fixture(false,false,true));const Snapshot text(session);inspector.choose_value("evenodd");inspector.apply()->click();
    check(session.document()==ruled(text.document,"evenodd"),"Path and Text Fill rows are compatible without changing Text content or typography");
}

void link_replace_unlink(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    inspector.choose_mode("link");inspector.choose_source("source","source-fill");
    check(before.same(session)&&inspector.apply()->isEnabled(),"A selected source is only a staged exact Ref");
    inspector.cancel()->click();check(before.same(session)&&inspector.mode()->currentData().toString()=="edit","Cancel discards link intent");
    inspector.choose_mode("link");inspector.choose_source("source","source-fill");inspector.apply()->click();
    const auto expected=linked(before.document,source_ref);
    check(session.document()==expected&&session.revision()==1&&session.history().states.size()==before.history.states.size()+1,
        "Link applies every retained Ref atomically without rewriting authored literals or paint");
    check(evaluate_fill_rule(session.document(),fill_targets[0].ref())=="evenodd"&&evaluate_fill_rule(session.document(),fill_targets[1].ref())=="evenodd"&&
        decode(encode(session.document()))==expected,"Both targets follow the common source and retain native source identity");
    check(inspector.status().startsWith("DRIVEN_PROPERTY")&&!inspector.value()->isEnabled()&&!inspector.apply()->isEnabled(),"Literal mode visibly refuses existing drivers");
    const Snapshot driven(session);inspector.value()->setCurrentIndex(inspector.value()->findData(QStringLiteral("nonzero")));
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(driven.same(session)&&inspector.status().startsWith("DRIVEN_PROPERTY"),"Forced literal Apply cannot silently unlink any target");
    inspector.choose_mode("link");inspector.choose_source("source","source-fill");
    check(!inspector.apply()->isEnabled(),"Re-linking every unchanged source is a no-op");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(driven.same(session),"No-op source acceptance does not create History");
    inspector.choose_source("alternate","alternate-fill");
    check(!inspector.apply()->isEnabled()&&inspector.status().startsWith("DRIVEN_PROPERTY"),"Source replacement requires an explicit unchecked-by-default choice");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(driven.same(session),"Apply independently refuses unapproved source replacement");
    inspector.replace()->setChecked(true);check(inspector.apply()->isEnabled(),"Explicit replacement enables the staged source change");
    inspector.cancel()->click();check(driven.same(session)&&!inspector.replace()->isChecked(),"Cancel clears replacement authorization for this draft");
    inspector.choose_mode("link");inspector.choose_source("alternate","alternate-fill");inspector.replace()->setChecked(true);inspector.apply()->click();
    check(session.document()==linked(expected,alternate_ref)&&session.revision()==2,"Explicit replacement changes all drivers in one transaction");
    inspector.undo();check(session.document()==expected,"One Undo restores every replaced driver");
    inspector.undo();check(session.document()==before.document,"One earlier Undo restores the original unlinked literals");
    inspector.redo();check(session.document()==expected,"Redo restores the original multi-target link");

    session.apply({OperationOptions{"source","source-fill","below","nonzero",{}}},session.revision());inspector.host.edited();
    check(inspector.state()=="Shared: Nonzero winding","Later source changes update every linked target's displayed value");
    inspector.load(fixture(false,true));const Snapshot mixed_drivers(session);
    check(inspector.state()=="Mixed","Different drivers display per-target evaluated Mixed values");
    inspector.choose_mode("unlink");check(mixed_drivers.same(session)&&inspector.apply()->isEnabled(),"Unlink is staged independently of the displayed Mixed value");
    inspector.cancel()->click();check(mixed_drivers.same(session),"Cancel of Unlink leaves both different sources intact");
    inspector.choose_mode("unlink");inspector.apply()->click();const auto frozen=unlinked(mixed_drivers.document);
    check(session.document()==frozen&&session.revision()==1&&session.history().states.size()==mixed_drivers.history.states.size()+1,
        "Unlink freezes each target's own evaluated value in one atomic Undo");
    check(operation(frozen,fill_targets[0]).fill_rule=="evenodd"&&operation(frozen,fill_targets[1]).fill_rule=="nonzero"&&
        decode(encode(session.document()))==frozen,"Unlink does not collapse Mixed values to the first target");
    inspector.undo();check(session.document()==mixed_drivers.document,"One Undo restores every original distinct driver and literal");
    inspector.redo();check(session.document()==frozen,"One Redo restores independent per-target frozen values");
}

void selection_source_and_core_refusals(){
    Inspector inspector;auto& session=inspector.host.session;
    const auto refuse=[&](const std::vector<MultiFillRuleTarget>& targets,Document document,const char* code){
        inspector.targets=targets;inspector.load(std::move(document));const Snapshot before(session);
        check(inspector.state()=="Unavailable"&&!inspector.mode()->isEnabled()&&!inspector.value()->isEnabled()&&
            !inspector.apply()->isEnabled()&&inspector.status().startsWith(QString::fromLatin1(code)),"Invalid selections refuse visibly without filtering a target");
        inspector.apply()->setEnabled(true);inspector.apply()->click();check(before.same(session),"Invalid selection cannot apply partial commands");
    };
    refuse({},fixture(),"INVALID_SELECTION");refuse({fill_targets.front()},fixture(),"INVALID_SELECTION");
    refuse({fill_targets.front(),fill_targets.front()},fixture(),"INVALID_SELECTION");
    refuse(std::vector<MultiFillRuleTarget>(1001,fill_targets.front()),fixture(),"INVALID_SELECTION");
    refuse({fill_targets.front(),{"missing","fill"}},fixture(),"MISSING_OBJECT");
    refuse({fill_targets.front(),{"second","missing"}},fixture(),"MISSING_OPERATION");
    refuse({fill_targets.front(),{"second","second-offset"}},fixture(),"UNSUPPORTED_FILL");
    auto different_slot=fixture();std::swap(different_slot.objects.at("second").stack[1],different_slot.objects.at("second").stack[2]);
    refuse(fill_targets,different_slot,"INCOMPATIBLE_FILL");
    auto group_target=fixture();refuse({fill_targets.front(),{"source-folder","no-fill"}},group_target,"TYPE_MISMATCH");
    inspector.targets=fill_targets;inspector.load();const Snapshot malformed(session);
    inspector.value()->addItem("Invalid","diagonal");inspector.choose_value("diagonal");
    check(!inspector.apply()->isEnabled(),"An unsupported literal cannot enable Apply");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(malformed.same(session)&&inspector.status().startsWith("UNSUPPORTED_FILL_RULE"),"Apply validates the exact rule independently of UI state");

    auto document=fixture();auto foreign=empty_document("unused","foreign-composition","foreign-artboard").compositions.front();
    document.objects.emplace("foreign",source_object("foreign","evenodd"));foreign.roots={"foreign"};document.compositions.push_back(foreign);
    auto dependent=source_object("dependent","nonzero");dependent.stack.front().fill_rule_driver=FillRuleDriver{fill_targets[1].ref()};
    document.objects.emplace(dependent.id,dependent);document.compositions.front().roots.push_back(dependent.id);inspector.load(document);
    inspector.choose_mode("link");const Snapshot source_snapshot(session);
    check(inspector.source_index("foreign","foreign-fill")==-1&&inspector.source_index("dependent","dependent-fill")==-1&&
        inspector.source_index("first","first-fill")==-1&&inspector.source_index("second","other-fill")==-1,
        "Sources exclude cross-composition, self and any chain back to any retained target");
    check(inspector.source_index("first","first-unrelated")>=0&&inspector.source_index("source","source-fill")>=0,
        "Other same-composition Fill rows and nested source rows remain eligible");
    inspector.choose_source("source","source-fill");inspector.search()->setText("op.source-fill.fill_rule");
    check(inspector.sources()->count()==1&&inspector.sources()->currentIndex()==0,"Search resolves a property path while retaining exact selected source identity");
    inspector.search()->setText("alternate-fill");check(inspector.sources()->currentIndex()==-1&&!inspector.apply()->isEnabled(),"Filtering away a selected source never silently chooses another source");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(source_snapshot.same(session)&&inspector.status().startsWith("MISSING_REFERENCE"),"No visible source refuses all targets atomically");
    inspector.choose_source("alternate","alternate-fill");inspector.search()->clear();
    check(inspector.source_index("alternate","alternate-fill")==inspector.sources()->currentIndex(),"Clearing search preserves the selected stable source");
    inspector.apply()->click();check(session.document()==linked(source_snapshot.document,alternate_ref),"Duplicate display names cannot retarget the retained source Ref");

    auto deep=fixture();
    for(int index=0;index<=128;++index){
        const auto id="zchain-"+std::to_string(index);auto object=source_object(id,"evenodd");
        if(index<128)object.stack.front().fill_rule_driver=FillRuleDriver{operation_ref("zchain-"+std::to_string(index+1),"zchain-"+std::to_string(index+1)+"-fill","fill_rule")};
        deep.objects.emplace(id,object);deep.compositions.front().roots.push_back(id);
    }
    inspector.load(deep);const Snapshot depth_limit(session);inspector.choose_mode("link");inspector.choose_source("zchain-0","zchain-0-fill");
    check(inspector.apply()->isEnabled(),"An individually valid source can be staged before authoritative core batch validation");
    inspector.apply()->click();check(depth_limit.same(session)&&inspector.status().startsWith("DEPENDENCY_DEPTH"),"Core refusal of a deeper target chain leaves every target, revision and History exact");
    inspector.cancel()->click();check(depth_limit.same(session),"Cancel after an authoritative refusal is history-free");
}

void stale_context_retention_lifetime_and_bounds(){
    Inspector inspector;auto& session=inspector.host.session;
    inspector.choose_value("evenodd");session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale(session);
    inspector.apply()->click();check(stale.same(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Intervening revision changes refuse the entire literal batch");
    inspector.cancel()->click();check(stale.same(session),"Cancel cannot overwrite a newer revision");
    inspector.load();inspector.choose_mode("link");inspector.choose_source("source","source-fill");inspector.host.session_id+="-other";const Snapshot identity(session);
    inspector.apply()->click();check(identity.same(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Session identity guards source acceptance at the same document IDs");
    inspector.load(fixture(false,true));inspector.choose_mode("unlink");auto replacement=fixture(false,true);replacement.id="other-document";
    session=Session(replacement);const Snapshot replaced(session);inspector.apply()->click();
    check(replaced.same(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Document identity independently guards an Unlink draft");
    inspector.load();inspector.choose_value("evenodd");session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.same(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),"An active gesture and its preview survive refused Apply");
    session.cancel_gesture();inspector.apply()->click();check(gesture.same(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A cancelled gesture invalidates a retained draft even without history");
    inspector.load();session.begin_gesture(session.revision());const Snapshot active(session);inspector.rebuild();
    check(inspector.state()=="Unavailable"&&inspector.status().startsWith("GESTURE_ACTIVE")&&active.same(session),"Construction during a gesture refuses without cancelling it");session.cancel_gesture();
    inspector.load();inspector.choose_mode("link");inspector.choose_source("source","source-fill");session.begin_gesture(session.revision());session.cancel_gesture();
    const Snapshot canceled(session);inspector.apply()->click();check(canceled.same(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A no-history gesture also invalidates source drafts");

    inspector.load();const Snapshot retained(session);inspector.choose_value("evenodd");inspector.targets={{"source","source-fill"},{"alternate","alternate-fill"}};
    inspector.apply()->click();check(session.document()==ruled(retained.document,"evenodd"),"Later caller selection changes cannot retarget the captured operation IDs");
    inspector.targets=fill_targets;

    auto thousand=empty_document("thousand","composition","artboard");std::vector<MultiFillRuleTarget> targets;
    for(int index=0;index<1000;++index){const auto id="target-"+std::to_string(index);auto object=source_object(id,"nonzero");
        thousand.objects.emplace(id,object);thousand.compositions.front().roots.push_back(id);targets.push_back({id,id+"-fill"});}
    inspector.targets=targets;inspector.load(thousand);const Snapshot maximum(session);inspector.choose_value("evenodd");inspector.apply()->click();
    auto expected=maximum.document;for(const auto& target:targets)operation(expected,target).fill_rule="evenodd";
    check(session.document()==expected&&session.revision()==1&&session.history().states.size()==maximum.history.states.size()+1,"The 1000-target boundary is one atomic Undo");
    inspector.undo();check(session.document()==maximum.document,"One Undo restores all 1000 exact Fill rows");

    QTemporaryDir directory;QWidget parent;auto host=std::make_unique<Host>(directory.path());host->session=Session(fixture());
    QPointer<QWidget> panel=make_multi_fill_rule_controls(*host,fill_targets,&parent);
    auto* value=panel->findChild<QComboBox*>("multi-fill-rule-value");auto* apply=panel->findChild<QPushButton*>("multi-fill-rule-apply");
    auto* status=panel->findChild<QLabel*>("multi-fill-rule-status");value->setCurrentIndex(value->findData(QStringLiteral("evenodd")));apply->click();
    const Snapshot committed(host->session);apply->setEnabled(true);apply->click();check(committed.same(host->session),"Repeated Apply without reconstruction cannot add History");
    host.reset();value->setCurrentIndex(value->findData(QStringLiteral("nonzero")));
    check(status->text().startsWith("SESSION_CONFLICT")&&!apply->isEnabled(),"An orphaned control safely refuses after Host destruction");
}
}

int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{literal_shared_mixed_cancel();link_replace_unlink();selection_source_and_core_refusals();stale_context_retention_lifetime_and_bounds();
        std::cout<<"PASS "<<checks<<" multi-Fill rule Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
