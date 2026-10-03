#include "macro_public_interface_control.hpp"
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
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
    if(!result)throw std::runtime_error(std::string("Missing published-control widget: ")+name);
    return result;
}
void click(QWidget& root,const char* name) { named<QPushButton>(root,name)->click();events(); }
void edit(QWidget& root,const char* name,const QString& value) { named<QLineEdit>(root,name)->setText(value); }
void choose(QWidget& root,const char* name,const std::string& value) {
    auto* input=named<QComboBox>(root,name);const auto index=input->findData(QString::fromStdString(value));
    check(index>=0,"The chooser contains the requested stable identity");input->setCurrentIndex(index);events();
}
void map(QWidget& root,const Id& node,const std::string& parameter) {
    choose(root,"macro-public-interface-node",node);choose(root,"macro-public-interface-parameter",parameter);
}
std::string selected_id(QWidget& root) { return named<QLineEdit>(root,"macro-public-interface-id")->text().toStdString(); }
void select(QWidget& root,const std::string& id) {
    auto* list=named<QListWidget>(root,"macro-public-interface-controls");
    for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole).toString().toStdString()==id) {
        list->setCurrentRow(row);events();return;
    }
    throw std::runtime_error("Missing published row: "+id);
}
struct Snapshot {
    Document document;
    std::uint64_t revision;
    HistoryInfo history;
    std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())) {}
    bool unchanged(const Session& session) const {
        return session.document()==document&&session.revision()==revision&&session.history()==history&&encode(session.document())==native;
    }
};
MacroNode& node(MacroDefinitionRevision& graph,const Id& id) {
    const auto found=std::find_if(graph.nodes.begin(),graph.nodes.end(),[&](const auto& candidate){return candidate.operation.id==id;});
    if(found==graph.nodes.end())throw std::runtime_error("Missing test node");return *found;
}
MacroDefinitionRevision graph(const std::vector<std::pair<Id,std::string>>& specification,std::uint64_t revision) {
    MacroDefinitionRevision result;result.revision=revision;result.graph_version=revision==1?1:2;
    result.input={"input","local_paths_and_paint"};result.output={"output","local_paths_and_paint"};
    MacroEndpoint previous{"",result.input.id};
    for(const auto& [id,type]:specification) {
        auto operation=default_operation(id,type);
        if(type=="nect.shape.offset")operation.parameters.at("amount").literal=id=="offset-first"?12.345678901234567:8;
        else { operation.parameters.at("copies").literal=id=="repeat-first"?2:3;operation.parameters.at("position_x").literal=125; }
        result.nodes.push_back({operation,id+"-in",id+"-out"});
        result.edges.push_back({previous,{id,id+"-in"}});previous={id,id+"-out"};
    }
    result.edges.push_back({previous,{"",result.output.id}});result.output_mapping=previous;
    if(revision==1||specification.front().second=="nect.shape.offset")
        result.public_parameters={{"macro.offset.amount","Amount","offset-first","amount","number","du","local_paths_and_paint"}};
    // Presentation must follow execution edges, never node/edge array indexes.
    std::reverse(result.nodes.begin(),result.nodes.end());std::reverse(result.edges.begin(),result.edges.end());return result;
}
MacroDefinition definition(bool expanded=true) {
    MacroDefinition result;result.id="definition";result.label="Published controls";
    result.revisions.emplace(1,graph({{"offset-first","nect.shape.offset"},{"repeat-first","nect.shape.repeater"}},1));
    if(expanded) {
        result.latest_revision=2;
        result.revisions.emplace(2,graph({{"offset-first","nect.shape.offset"},{"repeat-first","nect.shape.repeater"},
            {"offset-second","nect.shape.offset"},{"repeat-second","nect.shape.repeater"}},2));
    }
    return result;
}
Document fixture(const MacroDefinition& macro) {
    Session session(empty_document("public-doc","composition","artboard"));
    session.apply({CreatePrimitive{"composition","","path","Macro target",default_primitive("source","nect.shape.rectangle")},
        MacroCommand{CreateMacroDefinition{macro}},MacroCommand{InstantiateMacro{"path",macro.id,"instance",1,0}},
        MacroCommand{SetMacroOverride{"path","instance","macro.offset.amount",27}}},session.revision());
    return session.document();
}
void load(Host& host,const MacroDefinition& macro=definition()) {
    host.changed={};host.session=Session(fixture(macro));host.session_id+="-fresh";
}
const MacroInstance& instance(const Document& document) {
    const auto& stack=document.objects.at("path").stack;
    const auto entry=std::find_if(stack.begin(),stack.end(),[](const auto& value){return value.id=="instance";});
    if(entry==stack.end()||!entry->macro)throw std::runtime_error("Missing instance");return *entry->macro;
}
const MacroPublicParameter& public_control(const MacroDefinitionRevision& source,const std::string& id) {
    const auto found=std::find_if(source.public_parameters.begin(),source.public_parameters.end(),[&](const auto& value){return value.id==id;});
    if(found==source.public_parameters.end())throw std::runtime_error("Missing public control");return *found;
}
void lifecycle(Host& host,const QString& directory) {
    load(host);const Snapshot before(host.session);const auto original=definition();int notifications=0;host.changed=[&]{++notifications;};
    MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();
    check(named<QLabel>(dialog,"macro-public-interface-revisions")->text().contains("Source 2")&&
        named<QLabel>(dialog,"macro-public-interface-revisions")->text().contains("New 3"),"The dialog edits the actual latest revision");
    check(named<QLineEdit>(dialog,"macro-public-interface-id")->isReadOnly()&&selected_id(dialog)=="macro.offset.amount",
        "The existing reserved public ID is preserved and read-only");
    auto* offset_nodes=named<QComboBox>(dialog,"macro-public-interface-node");
    check(offset_nodes->count()==2&&offset_nodes->itemData(0).toString()=="offset-first"&&
        offset_nodes->itemData(1).toString()=="offset-second"&&offset_nodes->itemText(0)!=offset_nodes->itemText(1),
        "Reserved Amount offers distinguishable actual Offset nodes in execution order");
    check(named<QLineEdit>(dialog,"macro-public-interface-default")->text()=="12.345678901234567",
        "Mapped defaults come from the actual full-precision node literal");
    edit(dialog,"macro-public-interface-label",QString::fromUtf8("Expansion 拡張"));
    edit(dialog,"macro-public-interface-default","17.25");
    click(dialog,"macro-public-interface-add");const auto copies_id=selected_id(dialog);
    check(copies_id.starts_with("macro.")&&copies_id.size()<=96&&copies_id!="macro.offset.amount",
        "New controls receive a distinct bounded public ID");
    map(dialog,"repeat-first","copies");edit(dialog,"macro-public-interface-label","Copies shown");
    edit(dialog,"macro-public-interface-default","3");
    choose(dialog,"macro-public-interface-node","repeat-second");
    check(selected_id(dialog)==copies_id&&named<QLineEdit>(dialog,"macro-public-interface-default")->text()=="3",
        "Remapping preserves identity and reads the selected second Repeater's own literal");
    edit(dialog,"macro-public-interface-default","4");choose(dialog,"macro-public-interface-node","repeat-first");
    check(selected_id(dialog)==copies_id&&named<QLineEdit>(dialog,"macro-public-interface-default")->text()=="3",
        "Returning to an internal target retains its uncommitted default draft");
    check(named<QLineEdit>(dialog,"macro-public-interface-default")->property("nect-unit").toString()=="scalar",
        "Copies uses canonical scalar presentation metadata");
    click(dialog,"macro-public-interface-add");const auto rotation_id=selected_id(dialog);map(dialog,"repeat-first","rotation");
    edit(dialog,"macro-public-interface-label","Turn per copy");edit(dialog,"macro-public-interface-default","-450.125");
    check(named<QLineEdit>(dialog,"macro-public-interface-default")->property("nect-unit").toString()=="degree"&&
        named<QLineEdit>(dialog,"macro-public-interface-default")->property("nect-widget-hint").toString()=="angle",
        "Rotation uses canonical degree and angle metadata");
    click(dialog,"macro-public-interface-add");const auto second_amount_id=selected_id(dialog);
    check(named<QComboBox>(dialog,"macro-public-interface-node")->currentData().toString()=="offset-second",
        "Add selects a free actual parameter rather than duplicating a mapping");
    edit(dialog,"macro-public-interface-label","Second Amount");edit(dialog,"macro-public-interface-default","21");
    select(dialog,copies_id);
    check(named<QLineEdit>(dialog,"macro-public-interface-label")->text()=="Copies shown"&&selected_id(dialog)==copies_id,
        "Selection changes preserve each row's stable ID and label draft");
    check(before.unchanged(host.session)&&notifications==0,"All publication, remap and default edits remain local until Save");
    click(dialog,"macro-public-interface-save");
    check(dialog.result()==QDialog::Accepted&&dialog.updated_definition_id()=="definition"&&dialog.saved_revision()==3&&
        notifications==1&&host.session.revision()==before.revision+1,"Save appends exactly one revision and notifies Host once");
    auto expected=original.revisions.at(2);expected.revision=3;expected.interface_version=2;
    expected.public_parameters={{"macro.offset.amount","Expansion 拡張","offset-first","amount","number","du","local_paths_and_paint"},
        {copies_id,"Copies shown","repeat-first","copies","number","scalar","local_paths_and_paint"},
        {rotation_id,"Turn per copy","repeat-first","rotation","number","degree","local_paths_and_paint"},
        {second_amount_id,"Second Amount","offset-second","amount","number","du","local_paths_and_paint"}};
    node(expected,"offset-first").operation.parameters.at("amount").literal=17.25;
    node(expected,"offset-second").operation.parameters.at("amount").literal=21;
    node(expected,"repeat-first").operation.parameters.at("copies").literal=3;
    node(expected,"repeat-second").operation.parameters.at("copies").literal=4;
    node(expected,"repeat-first").operation.parameters.at("rotation").literal=-450.125;
    const auto& saved_definition=host.session.document().macro_definitions.at("definition");
    check(saved_definition.revisions.at(1)==original.revisions.at(1)&&saved_definition.revisions.at(2)==original.revisions.at(2)&&
        saved_definition.revisions.at(3)==expected,"New interface/defaults are exact; retained revisions, ports, edges and node identities are unchanged");
    check(instance(host.session.document())==instance(before.document)&&macro_parameter_value(
        host.session.document(),"path","instance","macro.offset.amount")==27,"Existing pins and overrides remain exact and continue to evaluate");
    Session canonical(before.document);canonical.apply({MacroCommand{UpdateMacroDefinition{"definition",expected}}},canonical.revision());
    check(host.session.document()==canonical.document()&&host.session.history()==canonical.history(),
        "UI Save has exactly one canonical UpdateMacroDefinition command's state and history");
    const Snapshot saved(host.session);dialog.accept();events();check(saved.unchanged(host.session)&&notifications==1,"Repeated Save cannot append another revision");
    host.session.undo(host.session.revision());check(host.session.document()==before.document,"One Undo removes the whole published-interface revision");
    host.session.redo(host.session.revision());check(host.session.document()==saved.document,"Redo restores the exact public IDs, defaults and pins");
    host.changed={};const auto path=directory+"/published-controls.nect";host.save(path);host.flush();
    Host reopened(directory+"/cold");reopened.open(path);
    check(reopened.session.document()==saved.document,"Native Save and cold Host reopen retain the whole interface and old pin");
    MacroPublicInterfaceDialog again(reopened,"definition");again.show();events();select(again,copies_id);
    check(named<QLineEdit>(again,"macro-public-interface-label")->text()=="Copies shown"&&selected_id(again)==copies_id,
        "Reopening the editor preserves the existing generated IDs");again.reject();
}
void removal_and_invalid(Host& host) {
    load(host);const Snapshot before(host.session);MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();
    click(dialog,"macro-public-interface-add");const auto id=selected_id(dialog);map(dialog,"offset-first","amount");
    click(dialog,"macro-public-interface-save");
    check(before.unchanged(host.session)&&named<QLabel>(dialog,"macro-public-interface-error")->text().contains("INVALID_MACRO_MAPPING"),
        "Duplicate node/parameter mappings refuse atomically without closing the draft");
    map(dialog,"repeat-first","copies");edit(dialog,"macro-public-interface-default","2.5");click(dialog,"macro-public-interface-save");
    check(before.unchanged(host.session)&&!named<QLabel>(dialog,"macro-public-interface-error")->text().isEmpty(),
        "Noninteger Copies refuses through canonical validation");
    for(const auto* invalid:{"-1","1001","nan","not a number"}) {
        edit(dialog,"macro-public-interface-default",QString::fromUtf8(invalid));click(dialog,"macro-public-interface-save");
        check(before.unchanged(host.session)&&!named<QLabel>(dialog,"macro-public-interface-error")->text().isEmpty(),
            "Out-of-range and nonfinite Copies never commit authored data");
    }
    edit(dialog,"macro-public-interface-default","0");edit(dialog,"macro-public-interface-label",QString(129,'x'));
    click(dialog,"macro-public-interface-save");check(before.unchanged(host.session)&&named<QLabel>(dialog,"macro-public-interface-error")->text().contains("INVALID_MACRO_INTERFACE"),
        "Labels exceeding 128 UTF-8 bytes refuse atomically");
    edit(dialog,"macro-public-interface-label",QString::fromUtf8("界").repeated(43));click(dialog,"macro-public-interface-save");
    check(before.unchanged(host.session),"Label bounds count UTF-8 bytes rather than displayed characters");
    edit(dialog,"macro-public-interface-label","   ");click(dialog,"macro-public-interface-save");check(before.unchanged(host.session),"Blank labels refuse without mutating the document");
    edit(dialog,"macro-public-interface-label","Valid Copies");select(dialog,"macro.offset.amount");click(dialog,"macro-public-interface-remove");
    check(named<QListWidget>(dialog,"macro-public-interface-controls")->count()==1&&selected_id(dialog)==id,
        "Removing one publication preserves remaining public identities");
    click(dialog,"macro-public-interface-save");
    check(dialog.saved_revision()==3&&instance(host.session.document())==instance(before.document),
        "Removing a publication appends one revision without migrating or resetting an old override");
    const Snapshot saved(host.session);
    try { host.session.apply({MacroCommand{UpdateMacroInstance{"path","instance",3}}},host.session.revision());
        throw std::runtime_error("Orphan override should refuse migration");
    } catch(const Error& error) { check(error.code=="ORPHAN_MACRO_OVERRIDE"&&saved.unchanged(host.session),
        "Explicit migration refuses an orphaned override without silently removing it"); }
    MacroPublicInterfaceDialog empty(host,"definition");empty.show();events();click(empty,"macro-public-interface-remove");click(empty,"macro-public-interface-save");
    check(empty.saved_revision()==4&&host.session.document().macro_definitions.at("definition").revisions.at(4).public_parameters.empty(),
        "An explicitly empty interface is supported in a new revision");
}
void bounds_and_remap(Host& host) {
    auto source=definition();std::vector<std::pair<Id,std::string>> repeaters;
    for(unsigned i=0;i<8;++i)repeaters.emplace_back("repeater-"+std::to_string(i),"nect.shape.repeater");
    source.revisions.at(2)=graph(repeaters,2);source.revisions.at(2).interface_version=2;
    load(host,source);MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();
    for(unsigned i=0;i<16;++i)click(dialog,"macro-public-interface-add");
    check(named<QListWidget>(dialog,"macro-public-interface-controls")->count()==16&&!named<QPushButton>(dialog,"macro-public-interface-add")->isEnabled(),
        "Sixteen published rows stay selectable and Add is bounded");
    click(dialog,"macro-public-interface-save");const auto& saved=host.session.document().macro_definitions.at("definition").revisions.at(3);
    std::set<std::string> ids;std::set<std::pair<Id,std::string>> mappings;
    for(const auto& control:saved.public_parameters) { ids.insert(control.id);mappings.emplace(control.node,control.parameter); }
    check(saved.public_parameters.size()==16&&ids.size()==16&&mappings.size()==16,"Bounded repeated-type publication has unique IDs and actual mappings");
    load(host);MacroPublicInterfaceDialog remap(host,"definition");remap.show();events();click(remap,"macro-public-interface-add");
    const auto id=selected_id(remap);map(remap,"repeat-first","copies");edit(remap,"macro-public-interface-label","Remapped control");
    map(remap,"repeat-second","rotation");edit(remap,"macro-public-interface-default","-720");click(remap,"macro-public-interface-save");
    const auto& control=public_control(host.session.document().macro_definitions.at("definition").revisions.at(3),id);
    check(control.id==id&&control.label=="Remapped control"&&control.node=="repeat-second"&&control.parameter=="rotation"&&control.unit=="degree",
        "Changing numeric mapping keeps the public identity and derives the new canonical unit");
}
void chooser_cancel_and_guards(Host& host) {
    load(host,definition(false));const Snapshot before(host.session);
    {
        MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();click(dialog,"macro-public-interface-add");
        edit(dialog,"macro-public-interface-label","Discarded draft");click(dialog,"macro-public-interface-cancel");dialog.accept();events();
        check(before.unchanged(host.session)&&dialog.saved_revision()==0,"Cancel is terminal against repeated or queued Save");
    }
    {
        MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();dialog.close();dialog.accept();events();
        check(before.unchanged(host.session),"Window Close discards the whole publication draft");
    }
    {
        MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();click(dialog,"macro-public-interface-save");
        check(dialog.saved_revision()==2&&host.session.document().macro_definitions.at("definition").revisions.at(2).interface_version==2&&
            host.session.document().macro_definitions.at("definition").revisions.at(1).interface_version==1,
            "Editing legacy revision 1 creates interface v2 only on revision 2 and retains the old interface");
    }
    load(host);auto other=definition();other.id="other";other.label="Other Definition";
    host.session.apply({MacroCommand{CreateMacroDefinition{other}}},host.session.revision());
    {
        const Snapshot start(host.session);MacroPublicInterfaceDialog dialog(host);dialog.show();events();
        click(dialog,"macro-public-interface-add");choose(dialog,"macro-public-interface-definition","other");
        check(named<QListWidget>(dialog,"macro-public-interface-controls")->count()==1&&start.unchanged(host.session),
            "Choosing another Definition discards only the unsaved draft");click(dialog,"macro-public-interface-save");
        check(dialog.updated_definition_id()=="other"&&host.session.document().macro_definitions.at("definition")==start.document.macro_definitions.at("definition"),
            "The chooser saves only the explicitly selected Definition");
    }
    {
        MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();
        host.session.apply({Rename{"path","Concurrent edit"}},host.session.revision());const Snapshot current(host.session);
        click(dialog,"macro-public-interface-add");click(dialog,"macro-public-interface-save");
        check(current.unchanged(host.session)&&named<QLabel>(dialog,"macro-public-interface-error")->text().contains("REVISION_CONFLICT"),
            "Stale Add/Save reports a conflict instead of throwing or overwriting concurrent edits");
    }
    {
        MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();host.session.begin_gesture(host.session.revision());const Snapshot current(host.session);
        click(dialog,"macro-public-interface-save");check(current.unchanged(host.session)&&host.session.gesture_active()&&
            named<QLabel>(dialog,"macro-public-interface-error")->text().contains("GESTURE_ACTIVE"),"Active gestures refuse Save without cancellation");
        host.session.cancel_gesture();click(dialog,"macro-public-interface-save");check(current.unchanged(host.session)&&
            named<QLabel>(dialog,"macro-public-interface-error")->text().contains("REVISION_CONFLICT"),"A cancelled intervening gesture invalidates the old draft");
    }
    {
        MacroPublicInterfaceDialog dialog(host,"definition");dialog.show();events();host.create_document();const Snapshot current(host.session);
        click(dialog,"macro-public-interface-save");check(current.unchanged(host.session)&&
            named<QLabel>(dialog,"macro-public-interface-error")->text().contains("SESSION_CONFLICT"),"Replacing Session refuses a surviving draft");
    }
    {
        MacroPublicInterfaceDialog empty(host);empty.show();events();
        check(!named<QPushButton>(empty,"macro-public-interface-save")->isEnabled()&&
            named<QLabel>(empty,"macro-public-interface-error")->text().contains("MISSING_MACRO_DEFINITION"),"An empty document exposes no fictitious editable Definition");
    }
    {
        QTemporaryDir directory;auto transient=std::make_unique<Host>(directory.path());load(*transient);
        MacroPublicInterfaceDialog dialog(*transient,"definition");dialog.show();events();transient.reset();click(dialog,"macro-public-interface-save");
        check(named<QLabel>(dialog,"macro-public-interface-error")->text().contains("SESSION_CONFLICT"),"A destroyed Host is guarded by QPointer");
    }
    load(host);QPointer<MacroPublicInterfaceDialog> transient=new MacroPublicInterfaceDialog(host,"definition");transient->show();events();
    QObject::connect(transient.data(),&QDialog::accepted,&host,[&]{delete transient.data();});
    int notifications=0;host.changed=[&]{++notifications;};transient->accept();events();
    check(!transient&&host.session.document().macro_definitions.at("definition").latest_revision==3&&notifications==1,
        "Synchronous accepted destruction safely finishes one Save and one Host notification");host.changed={};
}
}
int main(int argc,char** argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"Temporary storage is available");Host host(directory.path()+"/recovery");
        lifecycle(host,directory.path());removal_and_invalid(host);bounds_and_remap(host);chooser_cancel_and_guards(host);
        std::cout<<"Macro published-interface interaction checks: "<<checks<<'\n';return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
