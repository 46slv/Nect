#include "macro_revision_control.hpp"
#include "macro_authoring_control.hpp"
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* reason) { if(!ok)throw std::runtime_error(reason);++checks; }
void events() { QApplication::processEvents(); }
template<class T>T* named(QWidget& root,const char* name) {
    auto* result=root.findChild<T*>(QString::fromUtf8(name));
    if(!result)throw std::runtime_error(std::string("Missing Macro revision control: ")+name);
    return result;
}
void click(QWidget& root,const char* name) {
    QTest::mouseClick(named<QPushButton>(root,name),Qt::LeftButton);events();
}
void edit(QWidget& root,const char* name,const QString& value) { named<QLineEdit>(root,name)->setText(value); }
struct Snapshot {
    Document document;
    std::uint64_t revision;
    HistoryInfo history;
    std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())) {}
    bool unchanged(const Session& session) const {
        return session.document()==document&&session.revision()==revision&&session.history()==history&&
            encode(session.document())==native;
    }
};
Document fixture() {
    Session session(empty_document("revision-doc","composition","artboard"));
    session.apply({CreatePrimitive{"composition","","path","Macro target",
        default_primitive("source","nect.shape.rectangle")}},session.revision());
    return session.document();
}
void load(Host& host) { host.session=Session(fixture());host.session_id+="-fresh";host.changed={}; }
Id create(Host& host) {
    MacroAuthoringDialog dialog(host);dialog.show();events();
    edit(dialog,"macro-authoring-offset-amount","12.345678901234567");
    edit(dialog,"macro-authoring-repeater-copies","2");
    edit(dialog,"macro-authoring-repeater-position_x","125");
    click(dialog,"macro-authoring-create");
    check(dialog.result()==QDialog::Accepted&&!dialog.created_definition_id().empty(),
        "The existing authoring UI creates the starting Macro Definition");
    return dialog.created_definition_id();
}
void instantiate(Host& host,const Id& definition,bool override_amount=false) {
    host.session.apply({MacroCommand{InstantiateMacro{"path",definition,"instance",1,0}}},host.session.revision());
    if(override_amount)host.session.apply({MacroCommand{SetMacroOverride{
        "path","instance","macro.offset.amount",27}}},host.session.revision());
}
const MacroInstance& instance(const Document& document) {
    const auto& entries=document.objects.at("path").stack;
    const auto found=std::find_if(entries.begin(),entries.end(),[](const auto& entry){return entry.id=="instance";});
    if(found==entries.end()||!found->macro)throw std::runtime_error("Missing test Macro instance");
    return *found->macro;
}
MacroNode& node(MacroDefinitionRevision& graph,const char* type) {
    const auto found=std::find_if(graph.nodes.begin(),graph.nodes.end(),
        [&](const auto& value){return value.operation.type==type;});
    if(found==graph.nodes.end())throw std::runtime_error("Missing test Macro node");
    return *found;
}
void choose(QWidget& controls,std::uint64_t revision) {
    auto* target=named<QComboBox>(controls,"macro-instance-revision-target");
    const auto index=target->findData(QVariant::fromValue(static_cast<qulonglong>(revision)));
    check(index>=0,"The target chooser exposes the retained revision");target->setCurrentIndex(index);events();
}
void lifecycle(Host& host,const QString& directory) {
    load(host);const auto id=create(host);instantiate(host,id);
    host.session.apply({MacroCommand{InstantiateMacro{"path",id,"other-instance",1,1}}},host.session.revision());
    // Reversed storage order/edges are valid. UI editing must use stable IDs,
    // rather than treating array positions as Offset/Repeater identities.
    auto reordered=host.session.document();auto& source=reordered.macro_definitions.at(id).revisions.at(1);
    std::reverse(source.nodes.begin(),source.nodes.end());std::reverse(source.edges.begin(),source.edges.end());
    host.session=Session(reordered);host.session_id+="-reordered";
    const Snapshot before(host.session);const auto original_graph=source;int notifications=0;
    host.changed=[&]{++notifications;};
    MacroRevisionDialog dialog(host,id);dialog.show();events();
    check(named<QLineEdit>(dialog,"macro-revision-source-revision")->text()=="1"&&
        named<QLineEdit>(dialog,"macro-revision-next-revision")->text()=="2",
        "Editing clearly names the source and next revision");
    check(named<QLineEdit>(dialog,"macro-revision-public-id")->isReadOnly()&&
        named<QLineEdit>(dialog,"macro-revision-public-id")->text()=="macro.offset.amount",
        "The stable public contract cannot be accidentally renamed");
    check(named<QLineEdit>(dialog,"macro-revision-offset-amount")->text()=="12.345678901234567",
        "The editor reads the existing full-precision default, not an operator reset");
    edit(dialog,"macro-revision-offset-amount","19.123456789012344");
    edit(dialog,"macro-revision-repeater-position_y","-25");
    edit(dialog,"macro-revision-repeater-rotation","-450.12345678901234");
    edit(dialog,"macro-revision-public-label",QString::fromUtf8("Expansion 拡張"));
    check(before.unchanged(host.session)&&notifications==0,"Editing a revision is only a local draft");
    click(dialog,"macro-revision-save");
    check(dialog.result()==QDialog::Accepted&&dialog.updated_definition_id()==id&&dialog.saved_revision()==2&&
        notifications==1&&host.session.revision()==before.revision+1,
        "Saving appends one canonical revision and notifies Host once");
    auto expected=original_graph;expected.revision=2;
    node(expected,"nect.shape.offset").operation.parameters.at("amount").literal=19.123456789012344;
    node(expected,"nect.shape.repeater").operation.parameters.at("position_y").literal=-25;
    node(expected,"nect.shape.repeater").operation.parameters.at("rotation").literal=-450.12345678901234;
    expected.public_parameters.front().label="Expansion 拡張";
    const auto& definition=host.session.document().macro_definitions.at(id);
    check(definition.latest_revision==2&&definition.revisions.size()==2&&definition.revisions.at(1)==original_graph&&
        definition.revisions.at(2)==expected,
        "Only requested defaults/label/revision change; old graph and all stable IDs/ports/mappings remain exact");
    check(instance(host.session.document()).pinned_revision==1&&macro_parameter_value(
        host.session.document(),"path","instance","macro.offset.amount")==12.345678901234567,
        "Creating revision 2 leaves the existing instance pinned and evaluated at revision 1");
    Session canonical(before.document);
    canonical.apply({MacroCommand{UpdateMacroDefinition{id,expected}}},canonical.revision());
    check(host.session.document()==canonical.document()&&host.session.history()==canonical.history(),
        "GUI save has exactly the document/history of one UpdateMacroDefinition");
    const Snapshot saved(host.session);dialog.accept();events();
    check(saved.unchanged(host.session)&&notifications==1,"Repeated Save cannot append another revision");
    host.session.undo(host.session.revision());
    check(host.session.document()==before.document&&encode(host.session.document())==before.native,
        "One Undo removes the new revision and restores the original graph/pin/native bytes");
    host.session.redo(host.session.revision());
    check(host.session.document()==saved.document,"Redo restores the exact appended graph");
    host.changed={};const auto saved_path=directory+"/pinned-macro.nect";host.save(saved_path);host.flush();
    Host reopened(directory+"/cold-recovery");reopened.open(saved_path);
    check(reopened.session.document()==saved.document&&instance(reopened.session.document()).pinned_revision==1,
        "Real native Save and cold Host reopen retain both revisions and the old pin");
    QWidget owner;auto* controls=make_macro_revision_controls(reopened,"path","instance",&owner);
    owner.show();events();
    check(named<QLabel>(*controls,"macro-instance-revision-state")->text().contains("Pinned revision 1")&&
        named<QComboBox>(*controls,"macro-instance-revision-target")->count()==2,
        "The selected instance shows its old pin and retained update targets");
    const Snapshot pinned(reopened.session);click(*controls,"macro-instance-revision-update");
    check(reopened.session.revision()==pinned.revision+1&&instance(reopened.session.document()).pinned_revision==2&&
        macro_parameter_value(reopened.session.document(),"path","instance","macro.offset.amount")==19.123456789012344,
        "Only the requested explicit instance update moves its pin and defaults");
    const auto& other_entries=reopened.session.document().objects.at("path").stack;
    const auto other=std::find_if(other_entries.begin(),other_entries.end(),[](const auto& entry){return entry.id=="other-instance";});
    check(other!=other_entries.end()&&other->macro&&other->macro->pinned_revision==1,
        "Updating the chosen instance leaves a sibling instance pinned to the old revision");
    Session updated_canonical(pinned.document);
    updated_canonical.apply({MacroCommand{UpdateMacroInstance{"path","instance",2}}},updated_canonical.revision());
    check(reopened.session.document()==updated_canonical.document()&&
        reopened.session.history()==updated_canonical.history(),
        "Instance UI has the exact semantics/history of one UpdateMacroInstance");
    const Snapshot migrated(reopened.session);named<QPushButton>(*controls,"macro-instance-revision-update")->click();events();
    check(migrated.unchanged(reopened.session),"Queued/repeated instance update cannot create another Undo step");
    reopened.session.undo(reopened.session.revision());
    check(reopened.session.document()==pinned.document&&encode(reopened.session.document())==pinned.native,
        "One Undo of migration restores the exact previous pin and authored native state");
    reopened.session.redo(reopened.session.revision());
    const auto migrated_path=directory+"/updated-macro.nect";reopened.save(migrated_path);reopened.flush();
    Host updated_reopen(directory+"/updated-cold-recovery");updated_reopen.open(migrated_path);
    check(updated_reopen.session.document()==migrated.document,
        "Cold reopen after explicit migration preserves the requested pin and stable retained source");
}
void chooser_cancel_invalid(Host& host) {
    load(host);const auto first=create(host);const auto second=create(host);const Snapshot before(host.session);
    {
        MacroRevisionDialog dialog(host);dialog.show();events();
        auto* chooser=named<QComboBox>(dialog,"macro-revision-definition");
        check(chooser->count()==2,"An existing Definition can be chosen without a selected instance");
        chooser->setCurrentIndex(chooser->findData(QString::fromStdString(first)));
        edit(dialog,"macro-revision-offset-amount","42");
        chooser->setCurrentIndex(chooser->findData(QString::fromStdString(second)));events();
        check(named<QLineEdit>(dialog,"macro-revision-offset-amount")->text()=="12.345678901234567",
            "Choosing another Definition loads its own defaults without applying the discarded draft");
        edit(dialog,"macro-revision-offset-amount","35");click(dialog,"macro-revision-cancel");
        dialog.accept();events();
        check(dialog.result()==QDialog::Rejected&&dialog.saved_revision()==0&&before.unchanged(host.session),
            "Cancel is terminal: a delayed Save cannot commit the cancelled draft");
    }
    {
        MacroRevisionDialog dialog(host,first);dialog.show();events();dialog.close();dialog.accept();events();
        check(before.unchanged(host.session),"Closing the draft prevents a subsequent queued Save");
    }
    const std::vector<std::pair<const char*,QString>> invalid{
        {"macro-revision-offset-amount","nan"},{"macro-revision-offset-amount","=12"},
        {"macro-revision-offset-amount","1000001"},{"macro-revision-repeater-copies","2.5"},
        {"macro-revision-public-label"," "},{"macro-revision-public-label",QString(129,'x')}};
    for(const auto& [name,value]:invalid) {
        MacroRevisionDialog dialog(host,first);dialog.show();events();edit(dialog,name,value);click(dialog,"macro-revision-save");
        check(dialog.isVisible()&&dialog.saved_revision()==0&&before.unchanged(host.session)&&
            !named<QLabel>(dialog,"macro-revision-error")->text().isEmpty(),
            "Invalid defaults or published label leave the draft open with no partial revision/history");
    }
    MacroRevisionDialog corrected(host,first);corrected.show();events();
    edit(corrected,"macro-revision-repeater-copies","2.5");click(corrected,"macro-revision-save");
    edit(corrected,"macro-revision-repeater-copies","3");click(corrected,"macro-revision-save");
    check(corrected.result()==QDialog::Accepted&&host.session.revision()==before.revision+1,
        "A corrected refused draft saves exactly one new revision");
}
void migration_refusal_and_overrides(Host& host) {
    load(host);const auto id=create(host);instantiate(host,id,true);
    auto next=host.session.document().macro_definitions.at(id).revisions.at(1);next.revision=2;
    node(next,"nect.shape.offset").operation.parameters.at("amount").literal=19;
    host.session.apply({MacroCommand{UpdateMacroDefinition{id,next}}},host.session.revision());
    {
        QWidget owner;auto* controls=make_macro_revision_controls(host,"path","instance",&owner);owner.show();events();
        const Snapshot before(host.session);click(*controls,"macro-instance-revision-update");
        check(instance(host.session.document()).pinned_revision==2&&
            instance(host.session.document()).overrides==instance(before.document).overrides&&
            macro_parameter_value(host.session.document(),"path","instance","macro.offset.amount")==27,
            "Compatible migration preserves the stable PublicParamID override");
        host.session.undo(host.session.revision());
        check(host.session.document()==before.document,"Migration Undo restores both old pin and local override");
    }
    next.revision=3;next.public_parameters.clear();
    host.session.apply({MacroCommand{UpdateMacroDefinition{id,next}}},host.session.revision());
    {
        QWidget owner;auto* controls=make_macro_revision_controls(host,"path","instance",&owner);owner.show();events();
        const Snapshot before(host.session);click(*controls,"macro-instance-revision-update");
        check(before.unchanged(host.session)&&named<QLabel>(*controls,"macro-instance-revision-error")->text().contains(
            "ORPHAN_MACRO_OVERRIDE"),"The canonical orphan-override refusal stays visible and wholly atomic");
        choose(*controls,2);click(*controls,"macro-instance-revision-update");
        check(instance(host.session.document()).pinned_revision==2&&
            macro_parameter_value(host.session.document(),"path","instance","macro.offset.amount")==27,
            "After refusal the user can explicitly choose a compatible retained revision");
    }
    {
        MacroRevisionDialog dialog(host,id);dialog.show();events();
        check(!dialog.findChild<QLineEdit*>("macro-revision-public-label"),
            "A source revision with removed publication does not silently gain a new interface");
        click(dialog,"macro-revision-save");
        check(host.session.document().macro_definitions.at(id).revisions.at(4).public_parameters.empty(),
            "Editing literal defaults preserves an already removed publication");
    }
}
void stale_and_lifetime(Host& host) {
    load(host);const auto id=create(host);instantiate(host,id);
    {
        MacroRevisionDialog dialog(host,id);dialog.show();events();
        host.session.apply({Rename{"path","External change"}},host.session.revision());const Snapshot before(host.session);
        click(dialog,"macro-revision-save");
        check(before.unchanged(host.session)&&named<QLabel>(dialog,"macro-revision-error")->text().contains("REVISION_CONFLICT"),
            "A stale definition draft cannot overwrite external edits or append history");
    }
    {
        QWidget owner;auto* controls=make_macro_revision_controls(host,"path","instance",&owner);owner.show();events();
        host.session.apply({Rename{"path","Another external change"}},host.session.revision());const Snapshot before(host.session);
        // Direct signal invocation exercises the guard even if the target is the current revision.
        named<QPushButton>(*controls,"macro-instance-revision-update")->setEnabled(true);
        click(*controls,"macro-instance-revision-update");
        check(before.unchanged(host.session)&&named<QLabel>(*controls,"macro-instance-revision-error")->text().contains("REVISION_CONFLICT"),
            "A stale instance control cannot update a different document revision");
    }
    {
        MacroRevisionDialog dialog(host,id);dialog.show();events();host.session.begin_gesture(host.session.revision());
        const Snapshot before(host.session);click(dialog,"macro-revision-save");
        check(before.unchanged(host.session)&&named<QLabel>(dialog,"macro-revision-error")->text().contains("GESTURE_ACTIVE"),
            "An active gesture blocks revision save without cancelling the gesture");
        host.session.cancel_gesture();click(dialog,"macro-revision-save");
        check(before.unchanged(host.session)&&named<QLabel>(dialog,"macro-revision-error")->text().contains("REVISION_CONFLICT"),
            "A cancelled intervening gesture invalidates the old draft context even at the same revision");
    }
    {
        MacroRevisionDialog dialog(host,id);dialog.show();events();host.create_document();const Snapshot before(host.session);
        click(dialog,"macro-revision-save");
        check(before.unchanged(host.session)&&named<QLabel>(dialog,"macro-revision-error")->text().contains("SESSION_CONFLICT"),
            "Replacing the Session refuses an old definition draft");
    }
    {
        QTemporaryDir directory;auto transient=std::make_unique<Host>(directory.path());load(*transient);
        const auto temporary_id=create(*transient);instantiate(*transient,temporary_id);
        MacroRevisionDialog dialog(*transient,temporary_id);QWidget owner;
        auto* controls=make_macro_revision_controls(*transient,"path","instance",&owner);
        dialog.show();owner.show();events();transient.reset();click(dialog,"macro-revision-save");
        named<QPushButton>(*controls,"macro-instance-revision-update")->setEnabled(true);
        click(*controls,"macro-instance-revision-update");
        check(named<QLabel>(dialog,"macro-revision-error")->text().contains("SESSION_CONFLICT")&&
            named<QLabel>(*controls,"macro-instance-revision-error")->text().contains("SESSION_CONFLICT"),
            "A destroyed Host is guarded by both surviving draft and instance controls");
    }
}
void synchronous_refresh(Host& host) {
    load(host);const auto id=create(host);instantiate(host,id);
    auto next=host.session.document().macro_definitions.at(id).revisions.at(1);next.revision=2;
    host.session.apply({MacroCommand{UpdateMacroDefinition{id,next}}},host.session.revision());
    QWidget owner;QPointer<QWidget> controls=make_macro_revision_controls(host,"path","instance",&owner);
    QPointer<QWidget> old=controls;
    host.changed=[&] {
        delete controls.data();controls=make_macro_revision_controls(host,"path","instance",&owner);
    };
    named<QPushButton>(*controls,"macro-instance-revision-update")->click();events();
    check(!old&&controls&&instance(host.session.document()).pinned_revision==2&&
        !named<QPushButton>(*controls,"macro-instance-revision-update")->isEnabled(),
        "Explicit update tolerates synchronous Inspector destruction and rebuild");
    host.changed={};
}
}
int main(int argc,char** argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"Temporary storage is available");
        Host host(directory.path()+"/recovery");
        lifecycle(host,directory.path());chooser_cancel_invalid(host);
        migration_refusal_and_overrides(host);stale_and_lifetime(host);synchronous_refresh(host);
        std::cout<<"Macro revision interaction checks: "<<checks<<'\n';return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
