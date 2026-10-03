#include "macro_chain_control.hpp"
#include "macro_revision_control.hpp"
#include "semantic_control.hpp"
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* reason) { if(!ok)throw std::runtime_error(reason);++checks; }
void events() { QApplication::processEvents(); }
template<class T>T* named(QWidget& root,const char* name) {
    auto* result=root.findChild<T*>(QString::fromUtf8(name));
    if(!result)throw std::runtime_error(std::string("Missing Macro chain control: ")+name);
    return result;
}
void click(QWidget& root,const char* name) {
    // Use the real Qt button path; small offscreen desktops may clip a row.
    named<QPushButton>(root,name)->click();events();
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
MacroDefinition initial_definition() {
    MacroDefinition definition;definition.id="definition";definition.label="Offset Repeat";
    MacroDefinitionRevision graph;graph.input={"graph-input","local_paths_and_paint"};
    graph.output={"graph-output","local_paths_and_paint"};
    auto offset=default_operation("offset","nect.shape.offset");offset.parameters.at("amount").literal=12.345678901234567;
    auto repeater=default_operation("repeater","nect.shape.repeater");repeater.parameters.at("copies").literal=2;
    graph.nodes={{offset,"offset-in","offset-out"},{repeater,"repeater-in","repeater-out"}};
    graph.edges={{{"","graph-input"},{"offset","offset-in"}},
        {{"offset","offset-out"},{"repeater","repeater-in"}},
        {{"repeater","repeater-out"},{"","graph-output"}}};
    graph.output_mapping={"repeater","repeater-out"};
    graph.public_parameters={{"macro.offset.amount","Amount","offset","amount","number","du","local_paths_and_paint"}};
    definition.revisions.emplace(1,std::move(graph));return definition;
}
Document fixture(bool publication=true) {
    Session session(empty_document("chain-doc","composition","artboard"));
    session.apply({CreatePrimitive{"composition","","path","Macro target",default_primitive("source","nect.shape.rectangle")}},session.revision());
    session.apply({MacroCommand{CreateMacroDefinition{initial_definition()}}},session.revision());
    session.apply({MacroCommand{InstantiateMacro{"path","definition","instance",1,0}}},session.revision());
    session.apply({MacroCommand{InstantiateMacro{"path","definition","sibling",1,1}}},session.revision());
    if(!publication) {
        auto next=initial_definition().revisions.at(1);next.revision=2;next.public_parameters.clear();
        session.apply({MacroCommand{UpdateMacroDefinition{"definition",next}}},session.revision());
    }
    return session.document();
}
void load(Host& host,bool publication=true) {
    host.changed={};host.session=Session(fixture(publication));host.session_id+="-fresh";
}
const MacroInstance& instance(const Document& document,const Id& id="instance") {
    const auto& stack=document.objects.at("path").stack;
    const auto operation=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==id;});
    if(operation==stack.end()||!operation->macro)throw std::runtime_error("Missing Macro instance");
    return *operation->macro;
}
std::vector<Id> order(const MacroDefinitionRevision& graph) {
    std::vector<Id> result;for(const auto* node:macro_execution_order(graph))result.push_back(node->operation.id);
    return result;
}
Id selected(QWidget& dialog) {
    auto* item=named<QListWidget>(dialog,"macro-chain-nodes")->currentItem();
    if(!item)throw std::runtime_error("No selected chain node");return item->data(Qt::UserRole).toString().toStdString();
}
void select(QWidget& dialog,const Id& id) {
    auto* list=named<QListWidget>(dialog,"macro-chain-nodes");
    for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole).toString().toStdString()==id) {
        list->setCurrentRow(row);events();return;
    }
    throw std::runtime_error("Node is missing from the chain list");
}
void publish(QWidget& dialog,const Id& id) {
    auto* chooser=named<QComboBox>(dialog,"macro-chain-public-node");
    const auto index=chooser->findData(QString::fromStdString(id));
    check(index>0,"The published-node chooser exposes each Offset by stable ID");
    chooser->setCurrentIndex(index);events();
}
const MacroNode& node(const MacroDefinitionRevision& graph,const Id& id) {
    const auto found=std::find_if(graph.nodes.begin(),graph.nodes.end(),[&](const auto& value){return value.operation.id==id;});
    if(found==graph.nodes.end())throw std::runtime_error("Missing graph node");return *found;
}

void lifecycle(Host& host,const QString& directory) {
    load(host);
    // Storage order deliberately disagrees with execution order.
    auto document=host.session.document();auto& original=document.macro_definitions.at("definition").revisions.at(1);
    std::reverse(original.nodes.begin(),original.nodes.end());std::reverse(original.edges.begin(),original.edges.end());
    const auto source=original;host.session=Session(document);host.session_id+="-reordered";
    const Snapshot before(host.session);int notifications=0;host.changed=[&]{++notifications;};
    MacroChainDialog dialog(host,"definition");dialog.show();events();
    auto* list=named<QListWidget>(dialog,"macro-chain-nodes");
    check(list->count()==2&&list->item(0)->data(Qt::UserRole).toString()=="offset"&&
        list->item(1)->data(Qt::UserRole).toString()=="repeater","The editor derives initial order from edges, not storage");
    check(named<QLabel>(dialog,"macro-chain-revisions")->text().contains("Source 1")&&
        named<QLabel>(dialog,"macro-chain-revisions")->text().contains("New 2"),"Source and appended revision are explicit");
    auto* amount=named<QLineEdit>(dialog,"macro-chain-default-amount");
    check(dynamic_cast<SemanticNumberInput*>(amount)&&amount->property("nect-exact-value").toBool()&&
        amount->text()=="12.345678901234567","The chain uses exact semantic numeric input and existing full-precision defaults");
    click(dialog,"macro-chain-add-offset");const auto new_offset=selected(dialog);
    edit(dialog,"macro-chain-default-amount","19.123456789012344");publish(dialog,new_offset);
    click(dialog,"macro-chain-up");
    click(dialog,"macro-chain-add-repeater");const auto new_repeater=selected(dialog);
    edit(dialog,"macro-chain-default-copies","1");edit(dialog,"macro-chain-default-rotation","-450.12345678901234");
    click(dialog,"macro-chain-up");
    select(dialog,new_offset);
    check(named<QLineEdit>(dialog,"macro-chain-default-amount")->text()=="19.123456789012344",
        "Defaults survive node selection and reordering without rounding/reset");
    edit(dialog,"macro-chain-public-label",QString::fromUtf8("Expansion 拡張"));
    check(before.unchanged(host.session)&&notifications==0,"Every chain/default/mapping edit is a local draft");
    click(dialog,"macro-chain-save");
    check(dialog.result()==QDialog::Accepted&&dialog.updated_definition_id()=="definition"&&dialog.saved_revision()==2&&
        host.session.revision()==before.revision+1&&notifications==1,"Save appends one canonical revision and notifies Host once");
    const auto& definition=host.session.document().macro_definitions.at("definition");const auto graph=definition.revisions.at(2);
    check(definition.latest_revision==2&&definition.revisions.size()==2&&definition.revisions.at(1)==source&&graph.graph_version==2,
        "Revision 1 is retained exactly while the new chain opts into graph version 2");
    check(order(graph)==std::vector<Id>{"offset",new_offset,new_repeater,"repeater"},
        "Saved edges execute the explicit UI order through both added operator types");
    check(node(graph,"offset")==node(source,"offset")&&node(graph,"repeater")==node(source,"repeater")&&
        graph.input==source.input&&graph.output==source.output,"Retained nodes/ports/defaults and graph input/output IDs are exact");
    std::set<Id> old_ids{source.input.id,source.output.id};
    for(const auto& value:source.nodes) {old_ids.insert(value.operation.id);old_ids.insert(value.input_port);old_ids.insert(value.output_port);}
    std::set<Id> fresh;
    for(const auto& id:{new_offset,new_repeater}) {
        const auto& value=node(graph,id);
        for(const auto& local:{value.operation.id,value.input_port,value.output_port})
            check(!old_ids.contains(local)&&fresh.insert(local).second,"Only new nodes receive fresh unique identities/ports");
    }
    auto expected_public=source.public_parameters.front();expected_public.node=new_offset;expected_public.label="Expansion 拡張";
    check(graph.public_parameters==std::vector<MacroPublicParameter>{expected_public}&&
        node(graph,new_offset).operation.parameters.at("amount").literal==19.123456789012344&&
        node(graph,new_repeater).operation.parameters.at("rotation").literal==-450.12345678901234,
        "Explicit mapping and edited defaults retain full precision and the stable public contract");
    check(instance(host.session.document()).pinned_revision==1&&instance(host.session.document(),"sibling").pinned_revision==1&&
        macro_parameter_value(host.session.document(),"path","instance","macro.offset.amount")==12.345678901234567,
        "Existing instances stay pinned with their old default after chain revision creation");
    Session canonical(before.document);canonical.apply({MacroCommand{UpdateMacroDefinition{"definition",graph}}},canonical.revision());
    check(host.session.document()==canonical.document()&&host.session.history()==canonical.history(),
        "GUI Save has exactly the state/history of one UpdateMacroDefinition");
    const Snapshot saved(host.session);dialog.accept();events();
    check(saved.unchanged(host.session)&&notifications==1,"Repeated Save cannot append another revision");
    host.session.undo(host.session.revision());
    check(host.session.document()==before.document&&encode(host.session.document())==before.native,"One Undo restores old graph/pins/native bytes");
    host.session.redo(host.session.revision());check(host.session.document()==saved.document,"Redo restores the exact chain");
    host.changed={};const auto path=directory+"/macro-chain.nect";host.save(path);host.flush();
    Host reopened(directory+"/cold-recovery");reopened.open(path);
    check(reopened.session.document()==saved.document&&instance(reopened.session.document()).pinned_revision==1,
        "Native Save/cold Host reopen preserve graph2, old revision and old instance pins");
    QWidget owner;auto* controls=make_macro_revision_controls(reopened,"path","instance",&owner);
    owner.show();events();click(*controls,"macro-instance-revision-update");
    check(instance(reopened.session.document()).pinned_revision==2&&instance(reopened.session.document(),"sibling").pinned_revision==1&&
        macro_parameter_value(reopened.session.document(),"path","instance","macro.offset.amount")==19.123456789012344,
        "Only the existing explicit instance update moves its pin to the chosen graph2 revision");
}

void remove_mapping_and_bounds(Host& host) {
    load(host);const Snapshot before(host.session);
    MacroChainDialog dialog(host,"definition");dialog.show();events();
    click(dialog,"macro-chain-add-offset");const auto replacement=selected(dialog);
    select(dialog,"offset");click(dialog,"macro-chain-remove");
    check(named<QComboBox>(dialog,"macro-chain-public-node")->currentIndex()==0&&
        !named<QLabel>(dialog,"macro-chain-error")->text().isEmpty(),"Removing the published node clears the choice and explains explicit remapping");
    click(dialog,"macro-chain-save");
    check(before.unchanged(host.session)&&dialog.saved_revision()==0&&
        named<QLabel>(dialog,"macro-chain-error")->text().contains("INVALID_MACRO_MAPPING"),
        "Save refuses a removed public node instead of silently retargeting Amount");
    publish(dialog,replacement);click(dialog,"macro-chain-save");
    const auto& graph=host.session.document().macro_definitions.at("definition").revisions.at(2);
    check(order(graph)==std::vector<Id>{"repeater",replacement}&&graph.public_parameters.front().node==replacement&&
        graph.input==before.document.macro_definitions.at("definition").revisions.at(1).input,
        "An explicit replacement permits removal, preserving the retained Repeater and graph boundary");
    MacroChainDialog bounded(host,"definition");bounded.show();events();
    auto* list=named<QListWidget>(bounded,"macro-chain-nodes");
    while(list->count()<16)click(bounded,"macro-chain-add-repeater");
    check(!named<QPushButton>(bounded,"macro-chain-add-offset")->isEnabled()&&
        !named<QPushButton>(bounded,"macro-chain-add-repeater")->isEnabled(),"Both add actions stop at the 16-node bound");
    while(list->count()>1)click(bounded,"macro-chain-remove");
    check(!named<QPushButton>(bounded,"macro-chain-remove")->isEnabled(),"The last node cannot be removed");
    const Snapshot unchanged(host.session);click(bounded,"macro-chain-cancel");bounded.accept();
    check(unchanged.unchanged(host.session),"Bounds/draft removal followed by Cancel never authors a revision");
    load(host,false);MacroChainDialog unpublished(host,"definition");unpublished.show();events();
    check(!named<QCheckBox>(unpublished,"macro-chain-publish-amount")->isChecked()&&
        named<QCheckBox>(unpublished,"macro-chain-publish-amount")->isEnabled()&&
        !named<QComboBox>(unpublished,"macro-chain-public-node")->isEnabled(),
        "An unpublished source stays off while offering explicit restoration of its historical interface");
    click(unpublished,"macro-chain-add-offset");click(unpublished,"macro-chain-save");
    check(host.session.document().macro_definitions.at("definition").revisions.at(3).public_parameters.empty(),
        "Adding nodes does not silently restore a previously removed public parameter");
}

void unpublish_remove_and_restore(Host& host) {
    load(host);
    const auto original=host.session.document().macro_definitions.at("definition").revisions.at(1);
    auto published=original;published.revision=2;
    published.public_parameters.front().label="Latest published Amount";
    host.session.apply({MacroCommand{UpdateMacroDefinition{"definition",published}},
        MacroCommand{SetMacroOverride{"path","instance","macro.offset.amount",27}}},host.session.revision());
    Session canonical(host.session);
    const Snapshot before(host.session);
    MacroChainDialog dialog(host,"definition");dialog.show();events();
    auto* publication=named<QCheckBox>(dialog,"macro-chain-publish-amount");
    check(publication->isChecked()&&publication->isEnabled(),"A published source starts with its explicit Amount switch on");
    publication->click();events();
    check(!publication->isChecked()&&!named<QComboBox>(dialog,"macro-chain-public-node")->isEnabled()&&
        !named<QLineEdit>(dialog,"macro-chain-public-label")->isEnabled(),
        "Explicit unpublication disables the draft's mapping and label controls");
    select(dialog,"offset");click(dialog,"macro-chain-remove");
    check(named<QListWidget>(dialog,"macro-chain-nodes")->count()==1&&selected(dialog)=="repeater"&&
        !named<QPushButton>(dialog,"macro-chain-remove")->isEnabled()&&before.unchanged(host.session),
        "Turning Amount off allows removal of the last Offset while retaining one Repeater as a local draft");
    click(dialog,"macro-chain-save");
    check(dialog.result()==QDialog::Accepted&&dialog.saved_revision()==3&&host.session.revision()==before.revision+1,
        "The unpublished Repeater-only draft saves as one new revision");
    const auto removed=host.session.document().macro_definitions.at("definition").revisions.at(3);
    const auto& definition=host.session.document().macro_definitions.at("definition");
    check(definition.latest_revision==3&&definition.revisions.size()==3&&definition.revisions.at(1)==original&&
        definition.revisions.at(2)==published&&removed.graph_version==2&&removed.public_parameters.empty()&&
        order(removed)==std::vector<Id>{"repeater"}&&node(removed,"repeater")==node(published,"repeater")&&
        removed.input==published.input&&removed.output==published.output,
        "Unpublication retains historical revisions and exact Repeater/boundary identities in graph2");
    check(instance(host.session.document())==instance(before.document)&&
        instance(host.session.document(),"sibling")==instance(before.document,"sibling")&&
        macro_parameter_value(host.session.document(),"path","instance","macro.offset.amount")==27,
        "Saving unpublication preserves both old pins and the existing published-Amount override");
    canonical.apply({MacroCommand{UpdateMacroDefinition{"definition",removed}}},canonical.revision());
    check(host.session.document()==canonical.document()&&host.session.history()==canonical.history(),
        "Unpublication Save has exactly the canonical UpdateMacroDefinition state/history");
    {
        QWidget owner;auto* controls=make_macro_revision_controls(host,"path","instance",&owner);owner.show();events();
        const Snapshot pinned(host.session);click(*controls,"macro-instance-revision-update");
        check(pinned.unchanged(host.session)&&named<QLabel>(*controls,"macro-instance-revision-error")->text().contains(
            "ORPHAN_MACRO_OVERRIDE"),"Explicit migration to the unpublished revision refuses an existing override atomically");
    }
    host.session.apply({MacroCommand{ResetMacroOverride{"path","instance","macro.offset.amount"}}},host.session.revision());
    {
        QWidget owner;auto* controls=make_macro_revision_controls(host,"path","instance",&owner);owner.show();events();
        const Snapshot reset(host.session);click(*controls,"macro-instance-revision-update");
        check(host.session.revision()==reset.revision+1&&instance(host.session.document()).pinned_revision==3&&
            instance(host.session.document()).overrides.empty()&&instance(host.session.document(),"sibling").pinned_revision==1,
            "After an explicit override reset only the requested instance can migrate to the Repeater-only revision");
    }
    MacroChainDialog restored(host,"definition");restored.show();events();
    check(!named<QCheckBox>(restored,"macro-chain-publish-amount")->isChecked(),
        "The reopened unpublished chain never restores Amount implicitly");
    click(restored,"macro-chain-add-offset");const auto replacement=selected(restored);
    named<QCheckBox>(restored,"macro-chain-publish-amount")->click();events();
    check(named<QCheckBox>(restored,"macro-chain-publish-amount")->isChecked()&&
        named<QComboBox>(restored,"macro-chain-public-node")->isEnabled()&&
        named<QComboBox>(restored,"macro-chain-public-node")->currentIndex()==0&&
        named<QLineEdit>(restored,"macro-chain-public-label")->text()==QString::fromStdString(published.public_parameters.front().label),
        "Explicit restoration recovers the latest historical label and requires a new mapping after its Offset was removed");
    const Snapshot unmapped(host.session);click(restored,"macro-chain-save");
    check(unmapped.unchanged(host.session)&&restored.saved_revision()==0&&
        named<QLabel>(restored,"macro-chain-error")->text().contains("INVALID_MACRO_MAPPING"),
        "A newly added Offset is never silently selected for a restored Amount");
    publish(restored,replacement);click(restored,"macro-chain-save");
    auto expected_public=published.public_parameters.front();expected_public.node=replacement;
    const auto& restored_definition=host.session.document().macro_definitions.at("definition");
    const auto& graph=restored_definition.revisions.at(4);
    check(restored.saved_revision()==4&&host.session.revision()==unmapped.revision+1&&
        graph.public_parameters==std::vector<MacroPublicParameter>{expected_public}&&
        order(graph)==std::vector<Id>{"repeater",replacement}&&restored_definition.revisions.at(3)==removed,
        "Explicit remapping restores the same canonical PublicParamID, latest label and metadata in one new revision");
    check(instance(host.session.document()).pinned_revision==3&&instance(host.session.document(),"sibling").pinned_revision==1,
        "Restoring the interface does not change either existing instance pin");
}

void invalid_defaults_and_chooser(Host& host) {
    load(host);auto other=initial_definition();other.id="other";other.label="Other Macro";
    host.session.apply({MacroCommand{CreateMacroDefinition{other}}},host.session.revision());const Snapshot before(host.session);
    MacroChainDialog dialog(host,"definition");dialog.show();events();
    edit(dialog,"macro-chain-default-amount","=12");select(dialog,"repeater");select(dialog,"offset");
    check(named<QLineEdit>(dialog,"macro-chain-default-amount")->text()=="=12","Incomplete literal drafts survive node changes");
    click(dialog,"macro-chain-save");check(before.unchanged(host.session)&&dialog.isVisible(),"Invalid literal save is atomic and keeps the draft open");
    auto* chooser=named<QComboBox>(dialog,"macro-chain-definition");chooser->setCurrentIndex(chooser->findData(QStringLiteral("other")));events();
    check(named<QLineEdit>(dialog,"macro-chain-default-amount")->text()=="12.345678901234567"&&
        named<QListWidget>(dialog,"macro-chain-nodes")->count()==2,"Choosing another existing Macro replaces only the unsaved draft");
    click(dialog,"macro-chain-cancel");dialog.accept();events();
    check(dialog.result()==QDialog::Rejected&&before.unchanged(host.session),"Cancel is terminal against delayed Save");
    for(const auto& value:{QString("nan"),QString("1000001")}) {
        MacroChainDialog invalid(host,"definition");invalid.show();events();edit(invalid,"macro-chain-default-amount",value);
        click(invalid,"macro-chain-save");check(before.unchanged(host.session)&&invalid.saved_revision()==0&&
            !named<QLabel>(invalid,"macro-chain-error")->text().isEmpty(),"Invalid/out-of-domain defaults are refused without state/history changes");
    }
    MacroChainDialog copies(host,"definition");copies.show();events();select(copies,"repeater");
    edit(copies,"macro-chain-default-copies","2.5");click(copies,"macro-chain-save");
    check(before.unchanged(host.session),"Canonical validation refuses fractional Repeater copies");
    edit(copies,"macro-chain-default-copies","3");click(copies,"macro-chain-save");
    check(copies.saved_revision()==2&&host.session.revision()==before.revision+1,"Correcting a refused draft saves one revision");
}

void stale_cancel_and_lifetime(Host& host) {
    load(host);
    {
        MacroChainDialog dialog(host,"definition");dialog.show();events();dialog.close();dialog.accept();events();
        check(dialog.saved_revision()==0&&host.session.document()==fixture(),"Close is terminal against a queued Save");
    }
    {
        MacroChainDialog dialog(host,"definition");dialog.show();events();
        host.session.apply({Rename{"path","Concurrent edit"}},host.session.revision());const Snapshot before(host.session);
        click(dialog,"macro-chain-save");check(before.unchanged(host.session)&&
            named<QLabel>(dialog,"macro-chain-error")->text().contains("REVISION_CONFLICT"),"Stale Save cannot replace concurrent authored state");
    }
    {
        MacroChainDialog dialog(host,"definition");dialog.show();events();host.session.begin_gesture(host.session.revision());
        const Snapshot before(host.session);click(dialog,"macro-chain-save");
        check(before.unchanged(host.session)&&host.session.gesture_active()&&
            named<QLabel>(dialog,"macro-chain-error")->text().contains("GESTURE_ACTIVE"),"A gesture blocks Save without cancelling the active edit");
        host.session.cancel_gesture();click(dialog,"macro-chain-save");
        check(before.unchanged(host.session)&&named<QLabel>(dialog,"macro-chain-error")->text().contains("REVISION_CONFLICT"),
            "An intervening cancelled gesture invalidates the draft even at unchanged document revision");
    }
    {
        MacroChainDialog dialog(host,"definition");dialog.show();events();host.create_document();const Snapshot before(host.session);
        click(dialog,"macro-chain-save");check(before.unchanged(host.session)&&
            named<QLabel>(dialog,"macro-chain-error")->text().contains("SESSION_CONFLICT"),"A replaced document/session refuses the surviving draft");
    }
    {
        QTemporaryDir directory;auto transient=std::make_unique<Host>(directory.path());load(*transient);
        MacroChainDialog dialog(*transient,"definition");QWidget owner;
        auto* controls=make_macro_chain_controls(*transient,&owner);dialog.show();owner.show();events();
        transient.reset();click(dialog,"macro-chain-save");click(*controls,"macro-chain-edit");
        check(named<QLabel>(dialog,"macro-chain-error")->text().contains("SESSION_CONFLICT")&&
            named<QLabel>(*controls,"macro-chain-controls-error")->text().contains("SESSION_CONFLICT"),
            "Surviving dialog and global hook guard a destroyed Host");
    }
    load(host);QPointer<MacroChainDialog> transient=new MacroChainDialog(host,"definition");transient->show();events();
    QObject::connect(transient.data(),&QDialog::accepted,&host,[&]{delete transient.data();});
    int notifications=0;host.changed=[&]{++notifications;};transient->accept();events();
    check(!transient&&host.session.document().macro_definitions.at("definition").latest_revision==2&&notifications==1,
        "Synchronous accepted destruction still commits and safely notifies Host once");host.changed={};
}

void global_hook(Host& host) {
    load(host);QWidget owner;auto* controls=make_macro_chain_controls(host,&owner);owner.show();events();
    const Snapshot before(host.session);click(*controls,"macro-chain-edit");
    auto* dialog=owner.findChild<QDialog*>("macro-chain-dialog");
    check(dialog&&dialog->isVisible()&&named<QComboBox>(*dialog,"macro-chain-definition")->count()==1,
        "The global Window hook opens a chooser without an object/instance selection");
    if(dialog)dialog->reject();events();check(before.unchanged(host.session),"Opening/cancelling the hook is read-only");
    host.session.apply({Rename{"path","Changed"}},host.session.revision());const Snapshot changed(host.session);
    click(*controls,"macro-chain-edit");check(changed.unchanged(host.session)&&
        named<QLabel>(*controls,"macro-chain-controls-error")->text().contains("REVISION_CONFLICT"),
        "A stale global hook reports its refresh requirement without opening a stale draft");
}
void preserve_multiple_controls(Host& host) {
    load(host);auto next=initial_definition().revisions.at(1);next.revision=2;next.interface_version=2;
    next.public_parameters.push_back({"macro.repeat.copies","Copies","repeater","copies","number","scalar","local_paths_and_paint"});
    next.public_parameters.push_back({"macro.repeat.rotation","Rotation","repeater","rotation","number","degree","local_paths_and_paint"});
    host.session.apply({MacroCommand{UpdateMacroDefinition{"definition",next}}},host.session.revision());
    MacroChainDialog dialog(host,"definition");dialog.show();events();
    check(!named<QCheckBox>(dialog,"macro-chain-publish-amount")->isVisible(),"Amount-only editor is hidden for a multi-control interface");
    click(dialog,"macro-chain-save");
    const auto& saved=host.session.document().macro_definitions.at("definition").revisions.at(3);
    check(dialog.result()==QDialog::Accepted&&saved.interface_version==2&&saved.public_parameters==next.public_parameters,
        "Editing the chain preserves every versioned public ID and mapping");
    check(instance(host.session.document()).pinned_revision==1,"Chain edit leaves the old instance pinned");
}

}

int main(int argc,char** argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"Temporary storage is available");Host host(directory.path()+"/recovery");
        lifecycle(host,directory.path());remove_mapping_and_bounds(host);unpublish_remove_and_restore(host);invalid_defaults_and_chooser(host);
        stale_cancel_and_lifetime(host);global_hook(host);preserve_multiple_controls(host);
        std::cout<<"Macro chain interaction checks: "<<checks<<'\n';return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
