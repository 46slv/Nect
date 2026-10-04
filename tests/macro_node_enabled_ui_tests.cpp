#define NECT_MACRO_BOOLEAN_API_ONLY
#include "macro_boolean_window_smoke.hpp"
#include "macro_chain_control.hpp"
#include "macro_revision_control.hpp"
#include "host.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

// Focused interface3 node-default editing through the real Qt dialogs. These
// events exercise production wiring, not physical input or full regression.
using namespace nect;
using namespace nect::desktop;
namespace fixture_api=macro_boolean_window_smoke;
namespace {
using Snapshot=fixture_api::Snapshot;
constexpr const char* definition_id="boolean-definition";
int checks=0;
void check(bool ok,const char* why) { if(!ok)throw std::runtime_error(why);++checks; }
void events() { QApplication::processEvents(); }
template<class T>T* named(QWidget& root,const char* name) {
    auto* result=root.findChild<T*>(QString::fromUtf8(name));
    if(!result)throw std::runtime_error(std::string("Missing Macro node Enabled control: ")+name);
    return result;
}
void click(QWidget& root,const char* name) {
    // Direct Qt activation avoids offscreen clipping of the dialogs' last row.
    named<QPushButton>(root,name)->click();events();
}
MacroNode& node(MacroDefinitionRevision& graph,const Id& id) {
    const auto found=std::find_if(graph.nodes.begin(),graph.nodes.end(),
        [&](const auto& value){return value.operation.id==id;});
    if(found==graph.nodes.end())throw std::runtime_error("Missing fixture node: "+id);
    return *found;
}
MacroDefinitionRevision ordered(MacroDefinitionRevision graph,const std::vector<Id>& ids) {
    std::vector<MacroNode> nodes;for(const auto& id:ids)nodes.push_back(node(graph,id));
    graph.graph_version=2;graph.nodes=std::move(nodes);graph.edges.clear();
    MacroEndpoint previous{"",graph.input.id};
    for(const auto& value:graph.nodes) {
        graph.edges.push_back({previous,{value.operation.id,value.input_port}});
        previous={value.operation.id,value.output_port};
    }
    graph.edges.push_back({previous,{"",graph.output.id}});graph.output_mapping=previous;return graph;
}
Document fixture(bool repeated=false) {
    auto document=fixture_api::fixture();
    if(repeated) {
        auto& graph=document.macro_definitions.at(definition_id).revisions.at(2);
        auto extra=default_operation("repeater-second","nect.shape.repeater");
        extra.parameters.at("copies").literal=2;extra.parameters.at("position_x").literal=-70;
        graph.nodes.push_back({extra,"repeater-second-input","repeater-second-output"});
        graph=ordered(graph,{"offset-target","repeater-target","repeater-second"});
        // Occurrence names must follow edges rather than array positions.
        std::reverse(graph.nodes.begin(),graph.nodes.end());std::reverse(graph.edges.begin(),graph.edges.end());
    }
    return document;
}
Document legacy_fixture(unsigned interface_version) {
    auto document=fixture();auto& definition=document.macro_definitions.at(definition_id);
    auto legacy=definition.revisions.at(1);legacy.revision=2;legacy.interface_version=interface_version;
    if(interface_version==2)legacy.public_parameters.push_back({fixture_api::copies_id,"Copies","repeater-target","copies",
        "number","scalar","local_paths_and_paint"});
    definition.revisions.at(2)=std::move(legacy);
    if(interface_version==1)for(auto& operation:document.objects.at("path").stack)
        if(operation.macro)operation.macro->overrides.erase(fixture_api::copies_id);
    return document;
}
void load(Host& host,Document document=fixture()) {
    host.changed={};host.session=Session(std::move(document));host.session_id+="-node-enabled";
}
void select(QWidget& dialog,const Id& id) {
    auto* list=named<QListWidget>(dialog,"macro-chain-nodes");
    for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole).toString().toStdString()==id) {
        list->setCurrentRow(row);events();return;
    }
    throw std::runtime_error("Missing chain selection: "+id);
}
QCheckBox* toggle(QWidget& dialog,const char* name,const Id& id,bool value) {
    auto* input=named<QCheckBox>(dialog,name);
    check(input->property("nect-node-id").toString()==QString::fromStdString(id)&&
        input->property("nect-parameter").toString()=="enabled",
        "Enabled is addressed by stable node ID and its own boolean parameter");
    check(input->property("nect-semantic-key").toString()=="enabled"&&
        input->property("nect-widget-kind").toString()=="toggle"&&
        input->property("nect-widget-hint").toString()=="toggle"&&
        input->property("nect-value-type").toString()=="boolean"&&
        input->property("nect-unit").toString()=="boolean"&&
        input->property("nect-domain").toString()=="local_paths_and_paint"&&
        input->property("nect-control-status").toString()=="SUPPORTED"&&
        !input->property("nect-control-fallback").toBool()&&input->property("nect-exact-value").toBool()&&
        !input->isTristate()&&input->isEnabled()&&input->isChecked()==value,
        "Known Enabled boolean uses the exact two-state semantic toggle and the authored default, including false");
    return input;
}
void activate(QCheckBox* input,bool keyboard=false) {
    if(keyboard) {input->setFocus(Qt::OtherFocusReason);QTest::keyClick(input,Qt::Key_Space);}
    else input->click();
    events();
}
template<class Dialog>void saved_contract(Host& host,Dialog& dialog,const Snapshot& before,
        const MacroDefinitionRevision& expected,int& notifications,const QString& directory,const QString& name) {
    check(dialog.result()==QDialog::Accepted&&dialog.updated_definition_id()==definition_id&&dialog.saved_revision()==3&&
        host.session.revision()==before.revision+1&&notifications==1,
        "Save appends one canonical revision and emits one Host change notification");
    const auto& definition=host.session.document().macro_definitions.at(definition_id);
    check(definition.latest_revision==3&&definition.revisions.size()==3&&definition.revisions.at(3)==expected&&
        definition.revisions.at(1)==before.document.macro_definitions.at(definition_id).revisions.at(1)&&
        definition.revisions.at(2)==before.document.macro_definitions.at(definition_id).revisions.at(2),
        "Only the new revision changes: original stacks, exact defaults, stable ports and public mappings are retained");
    for(const auto* object:{"path","other","legacy"})check(
        fixture_api::instance(host.session.document(),object)==fixture_api::instance(before.document,object),
        "Every existing Macro instance keeps its exact pin and typed override maps");
    check(macro_parameter_boolean_value(host.session.document(),"path","instance",fixture_api::offset_enabled_id)&&
        !macro_parameter_boolean_value(host.session.document(),"path","instance",fixture_api::repeater_enabled_id),
        "Old interface3 pins still evaluate their original true/false Enabled defaults");
    Session canonical(before.document);
    canonical.apply({MacroCommand{UpdateMacroDefinition{definition_id,expected}}},canonical.revision());
    check(host.session.document()==canonical.document()&&host.session.history()==canonical.history()&&
        host.session.history().states.size()==before.history.states.size()+1,
        "GUI Save exactly matches one UpdateMacroDefinition state/history and Undo boundary");
    const Snapshot saved(host.session);dialog.accept();events();
    check(saved.unchanged(host.session)&&notifications==1,"A repeated Save cannot append another revision or notification");
    host.changed={};host.session.undo(host.session.revision());
    check(host.session.document()==before.document&&encode(host.session.document())==before.native,
        "One Undo restores the original revisions, pins, override presence and native bytes");
    host.session.redo(host.session.revision());
    check(host.session.document()==saved.document,"One Redo restores the exact Enabled revision");
    const auto path=directory+"/"+name+".nect";host.save(path);host.flush();
    Host reopened(directory+"/"+name+"-cold");reopened.open(path);
    check(reopened.session.document()==saved.document&&encode(reopened.session.document())==saved.native,
        "Native Save and cold Host reopen retain exact true/false defaults, revisions and old pins");
    QWidget owner;auto* controls=make_macro_revision_controls(reopened,"path","instance",&owner);
    owner.show();events();const Snapshot pinned(reopened.session);click(*controls,"macro-instance-revision-update");
    Session migrated(pinned.document);
    migrated.apply({MacroCommand{UpdateMacroInstance{"path","instance",3}}},migrated.revision());
    check(reopened.session.document()==migrated.document()&&reopened.session.history()==migrated.history()&&
        fixture_api::instance(reopened.session.document()).pinned_revision==3&&
        !macro_parameter_boolean_value(reopened.session.document(),"path","instance",fixture_api::offset_enabled_id)&&
        macro_parameter_boolean_value(reopened.session.document(),"path","instance",fixture_api::repeater_enabled_id)&&
        fixture_api::instance(reopened.session.document(),"other")==fixture_api::instance(pinned.document,"other")&&
        fixture_api::instance(reopened.session.document(),"legacy")==fixture_api::instance(pinned.document,"legacy"),
        "Explicit migration adopts new Enabled defaults only for the chosen pin and preserves numeric overrides");
    reopened.session.undo(reopened.session.revision());
    check(reopened.session.document()==pinned.document,"One migration Undo restores the old pin and its defaults");
}
void chain_lifecycle(Host& host,const QString& directory) {
    load(host);const Snapshot before(host.session);int notifications=0;host.changed=[&]{++notifications;};
    MacroChainDialog dialog(host,definition_id);dialog.show();events();
    auto* list=named<QListWidget>(dialog,"macro-chain-nodes");
    check(list->count()==2&&list->item(0)->data(Qt::UserRole).toString()=="offset-target"&&
        list->item(1)->data(Qt::UserRole).toString()=="repeater-target",
        "Chain starts in execution order even with reversed node and edge storage");
    activate(toggle(dialog,"macro-chain-default-enabled","offset-target",true),true);
    check(named<QLineEdit>(dialog,"macro-chain-default-amount")->text()=="5",
        "Enabled editing preserves the original numeric default");
    named<QLineEdit>(dialog,"macro-chain-default-amount")->setText("=12");
    select(dialog,"repeater-target");activate(toggle(dialog,"macro-chain-default-enabled","repeater-target",false));
    click(dialog,"macro-chain-up");toggle(dialog,"macro-chain-default-enabled","repeater-target",true);
    select(dialog,"offset-target");toggle(dialog,"macro-chain-default-enabled","offset-target",false);
    check(named<QLineEdit>(dialog,"macro-chain-default-amount")->text()=="=12",
        "Raw incomplete numeric text survives Enabled edits, node selection and reordering exactly");
    select(dialog,"repeater-target");toggle(dialog,"macro-chain-default-enabled","repeater-target",true);
    check(before.unchanged(host.session)&&notifications==0,
        "Both boolean edits survive selection/reorder entirely in the local draft");
    click(dialog,"macro-chain-save");
    check(dialog.saved_revision()==0&&dialog.isVisible()&&before.unchanged(host.session)&&notifications==0&&
        named<QLabel>(dialog,"macro-chain-error")->text().contains("INVALID_VALUE"),
        "Save refuses an incomplete numeric draft without committing Enabled edits or partial history");
    select(dialog,"offset-target");toggle(dialog,"macro-chain-default-enabled","offset-target",false);
    check(named<QLineEdit>(dialog,"macro-chain-default-amount")->text()=="=12",
        "Refused Save retains the raw text and the correct stable-node boolean draft");
    named<QLineEdit>(dialog,"macro-chain-default-amount")->setText("5");
    select(dialog,"repeater-target");toggle(dialog,"macro-chain-default-enabled","repeater-target",true);
    auto expected=before.document.macro_definitions.at(definition_id).revisions.at(2);expected.revision=3;
    node(expected,"offset-target").operation.enabled=false;node(expected,"repeater-target").operation.enabled=true;
    expected=ordered(expected,{"repeater-target","offset-target"});
    click(dialog,"macro-chain-save");saved_contract(host,dialog,before,expected,notifications,directory,"chain-enabled");
}
void revision_lifecycle(Host& host,const QString& directory) {
    load(host,fixture(true));const Snapshot before(host.session);int notifications=0;host.changed=[&]{++notifications;};
    MacroRevisionDialog dialog(host,definition_id);dialog.show();events();
    activate(toggle(dialog,"macro-revision-offset-enabled","offset-target",true));
    activate(toggle(dialog,"macro-revision-repeater-enabled","repeater-target",false),true);
    activate(toggle(dialog,"macro-revision-repeater-2-enabled","repeater-second",true));
    toggle(dialog,"macro-revision-offset-enabled","offset-target",false);
    toggle(dialog,"macro-revision-repeater-enabled","repeater-target",true);
    toggle(dialog,"macro-revision-repeater-2-enabled","repeater-second",false);
    check(before.unchanged(host.session)&&notifications==0,
        "Each repeated-type Enabled draft targets its own stable node, including an unpublished occurrence");
    auto expected=before.document.macro_definitions.at(definition_id).revisions.at(2);expected.revision=3;
    node(expected,"offset-target").operation.enabled=false;node(expected,"repeater-target").operation.enabled=true;
    node(expected,"repeater-second").operation.enabled=false;
    click(dialog,"macro-revision-save");saved_contract(host,dialog,before,expected,notifications,directory,"revision-enabled");
}
template<class Dialog>void cancel_and_stale(Host& host,const char* toggle_name,const char* save_name,
        const char* cancel_name,const char* error_name) {
    load(host);const Snapshot before(host.session);
    {
        Dialog dialog(host,definition_id);dialog.show();events();
        activate(toggle(dialog,toggle_name,"offset-target",true));click(dialog,cancel_name);dialog.accept();events();
        check(dialog.result()==QDialog::Rejected&&dialog.saved_revision()==0&&before.unchanged(host.session),
            "Cancel discards Enabled edits and remains terminal against a delayed Save");
    }
    {
        Dialog dialog(host,definition_id);dialog.show();events();activate(toggle(dialog,toggle_name,"offset-target",true));
        host.session.apply({Rename{"path","External authored change"}},host.session.revision());
        const Snapshot external(host.session);click(dialog,save_name);
        check(dialog.saved_revision()==0&&external.unchanged(host.session)&&
            named<QLabel>(dialog,error_name)->text().contains("REVISION_CONFLICT"),
            "A stale Enabled draft cannot overwrite external state or append partial history");
    }
    load(host);
    {
        Dialog dialog(host,definition_id);dialog.show();events();activate(toggle(dialog,toggle_name,"offset-target",true));
        host.create_document();const Snapshot replacement(host.session);click(dialog,save_name);
        check(dialog.saved_revision()==0&&replacement.unchanged(host.session)&&
            named<QLabel>(dialog,error_name)->text().contains("SESSION_CONFLICT"),
            "A replaced Session refuses the old Enabled draft without authored/native/history changes");
    }
}
void legacy_versions(Host& host) {
    for(const unsigned version:{1u,2u}) {
        load(host,legacy_fixture(version));const Snapshot before(host.session);
        MacroChainDialog chain(host,definition_id);chain.show();events();
        check(!chain.findChild<QCheckBox*>("macro-chain-default-enabled"),
            "Legacy interface1/2 Offset gains no node Enabled toggle");
        select(chain,"repeater-target");check(!chain.findChild<QCheckBox*>("macro-chain-default-enabled"),
            "Legacy interface1/2 Repeater gains no node Enabled toggle");
        select(chain,"offset-target");named<QLineEdit>(chain,"macro-chain-default-amount")->setText("7.25");
        auto expected=before.document.macro_definitions.at(definition_id).revisions.at(2);expected.revision=3;
        node(expected,"offset-target").operation.parameters.at("amount").literal=7.25;
        expected=ordered(expected,{"offset-target","repeater-target"});click(chain,"macro-chain-save");
        Session canonical(before.document);canonical.apply({MacroCommand{UpdateMacroDefinition{definition_id,expected}}},0);
        check(chain.result()==QDialog::Accepted&&host.session.document()==canonical.document()&&
            host.session.history()==canonical.history()&&expected.interface_version==version,
            "A legacy chain numeric edit preserves its exact interface and never promotes to interface3");
        load(host,legacy_fixture(version));const Snapshot original(host.session);
        MacroRevisionDialog revision(host,definition_id);revision.show();events();
        check(!revision.findChild<QCheckBox*>("macro-revision-offset-enabled")&&
            !revision.findChild<QCheckBox*>("macro-revision-repeater-enabled"),
            "Legacy interface1/2 revision editing exposes no Enabled toggles");
        named<QLineEdit>(revision,"macro-revision-offset-amount")->setText("7.25");
        auto revision_expected=original.document.macro_definitions.at(definition_id).revisions.at(2);revision_expected.revision=3;
        node(revision_expected,"offset-target").operation.parameters.at("amount").literal=7.25;
        click(revision,"macro-revision-save");Session revision_canonical(original.document);
        revision_canonical.apply({MacroCommand{UpdateMacroDefinition{definition_id,revision_expected}}},0);
        check(revision.result()==QDialog::Accepted&&host.session.document()==revision_canonical.document()&&
            host.session.history()==revision_canonical.history()&&revision_expected.interface_version==version,
            "A legacy revision numeric edit retains every Enabled default and its original interface version");
    }
}
}
int main(int argc,char** argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"Owned temporary storage is available");
        Host host(directory.path()+"/recovery");
        chain_lifecycle(host,directory.path());revision_lifecycle(host,directory.path());
        cancel_and_stale<MacroChainDialog>(host,"macro-chain-default-enabled","macro-chain-save",
            "macro-chain-cancel","macro-chain-error");
        cancel_and_stale<MacroRevisionDialog>(host,"macro-revision-offset-enabled","macro-revision-save",
            "macro-revision-cancel","macro-revision-error");
        legacy_versions(host);
        std::cout<<"Macro node Enabled UI checks: "<<checks<<'\n';return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
