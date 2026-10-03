#include "macro_authoring_control.hpp"
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* reason) { if(!ok)throw std::runtime_error(reason);++checks; }
void events() { QApplication::processEvents(); }
template<class T>T* named(QWidget& root,const char* name) {
    auto* result=root.findChild<T*>(QString::fromUtf8(name));
    if(!result)throw std::runtime_error(std::string("Missing authoring control: ")+name);
    return result;
}
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
    Session session(empty_document("authoring-doc","composition","artboard"));
    auto source=default_primitive("source","nect.shape.rectangle");
    session.apply({CreatePrimitive{"composition","","path","Macro target",source}},session.revision());
    return session.document();
}
void load(Host& host) { host.session=Session(fixture());host.session_id+="-fresh"; }
void edit(MacroAuthoringDialog& dialog,const char* name,const QString& value) {
    named<QLineEdit>(dialog,name)->setText(value);
}
void create(MacroAuthoringDialog& dialog) {
    QTest::mouseClick(named<QPushButton>(dialog,"macro-authoring-create"),Qt::LeftButton);events();
}
const MacroNode& node(const MacroDefinition& definition,const char* type) {
    const auto& nodes=definition.revisions.at(1).nodes;
    const auto found=std::find_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.operation.type==type;});
    if(found==nodes.end())throw std::runtime_error("Missing canonical node");
    return *found;
}

void author_undo_and_reopen(Host& host,const QString& directory) {
    load(host);const Snapshot before(host.session);int notifications=0;
    host.changed=[&]{++notifications;};
    MacroAuthoringDialog dialog(host);dialog.show();events();
    check(named<QLabel>(dialog,"macro-authoring-graph")->text().contains("Offset@1")&&
        named<QLabel>(dialog,"macro-authoring-graph")->text().contains("Repeater@1"),
        "The supported graph is visible before authoring");
    const std::vector<std::pair<const char*,QString>> metadata{
        {"macro-authoring-public-id","macro.offset.amount"},{"macro-authoring-public-mapping","Offset.amount"},
        {"macro-authoring-public-type","number"},{"macro-authoring-public-unit","du"},
        {"macro-authoring-public-domain","local_paths_and_paint"}};
    for(const auto& [name,value]:metadata) {
        const auto* input=named<QLineEdit>(dialog,name);
        check(input->isReadOnly()&&input->text()==value,"The fixed published contract is explicit and read-only");
    }
    check(named<QLineEdit>(dialog,"macro-authoring-offset-amount")->text()=="10"&&
        named<QLineEdit>(dialog,"macro-authoring-repeater-copies")->text()=="3",
        "Node drafts start with canonical operator defaults");
    edit(dialog,"macro-authoring-name",QString::fromUtf8("My Offset Repeat 日本語"));
    edit(dialog,"macro-authoring-public-label",QString::fromUtf8("Expand amount 拡張"));
    auto* amount=named<QLineEdit>(dialog,"macro-authoring-offset-amount");
    amount->setFocus();amount->selectAll();QTest::keyClicks(amount,"12.345678901234567");
    edit(dialog,"macro-authoring-offset-miter_limit","7");
    edit(dialog,"macro-authoring-repeater-copies","2");
    edit(dialog,"macro-authoring-repeater-position_x","125.12345678901234");
    edit(dialog,"macro-authoring-repeater-position_y","-25");
    edit(dialog,"macro-authoring-repeater-anchor_x","2");
    edit(dialog,"macro-authoring-repeater-anchor_y","3");
    edit(dialog,"macro-authoring-repeater-rotation","-450.12345678901234");
    edit(dialog,"macro-authoring-repeater-scale_x","1.1");
    edit(dialog,"macro-authoring-repeater-scale_y","0.9");
    edit(dialog,"macro-authoring-repeater-offset","0.5");
    edit(dialog,"macro-authoring-repeater-start_opacity","0.8");
    edit(dialog,"macro-authoring-repeater-end_opacity","0.4");
    amount->clearFocus();events();
    check(before.unchanged(host.session)&&notifications==0,"Authoring/blur is a transient draft with no partial state or history");
    create(dialog);
    const auto id=dialog.created_definition_id();
    check(dialog.result()==QDialog::Accepted&&!id.empty()&&notifications==1,
        "Create accepts the dialog, returns a stable DefinitionID and notifies Host once");
    check(host.session.revision()==before.revision+1&&host.session.document().macro_definitions.size()==1,
        "Create commits one definition in one revision");
    const auto definition=host.session.document().macro_definitions.at(id);
    const auto& graph=definition.revisions.at(1);
    const auto& offset=node(definition,"nect.shape.offset");const auto& repeater=node(definition,"nect.shape.repeater");
    check(definition.label==std::string("My Offset Repeat 日本語")&&definition.latest_revision==1&&
        definition.revisions.size()==1&&graph.revision==1&&graph.nodes.size()==2&&graph.edges.size()==3&&
        graph.input.domain=="local_paths_and_paint"&&graph.output.domain=="local_paths_and_paint",
        "Creation retains the named bounded revision and typed graph");
    const MacroPublicParameter expected_public{"macro.offset.amount","Expand amount 拡張",offset.operation.id,
        "amount","number","du","local_paths_and_paint"};
    check(graph.public_parameters==std::vector<MacroPublicParameter>{expected_public}&&
        offset.operation.parameters.at("amount").literal==12.345678901234567,
        "The exact public label/mapping and full-precision default are canonical authored state");
    auto expected_offset=default_operation(offset.operation.id,"nect.shape.offset");
    expected_offset.parameters.at("amount").literal=12.345678901234567;
    expected_offset.parameters.at("miter_limit").literal=7;
    auto expected_repeater=default_operation(repeater.operation.id,"nect.shape.repeater");
    const std::map<std::string,double> repeater_values{{"copies",2},{"position_x",125.12345678901234},
        {"position_y",-25},{"anchor_x",2},{"anchor_y",3},{"rotation",-450.12345678901234},
        {"scale_x",1.1},{"scale_y",0.9},{"offset",0.5},{"start_opacity",0.8},{"end_opacity",0.4}};
    for(const auto& [key,value]:repeater_values)expected_repeater.parameters.at(key).literal=value;
    check(offset.operation==expected_offset&&repeater.operation==expected_repeater,
        "Every displayed node default is preserved without replacing canonical operator options");
    const std::vector<MacroEdge> expected_edges{
        {{"",graph.input.id},{offset.operation.id,offset.input_port}},
        {{offset.operation.id,offset.output_port},{repeater.operation.id,repeater.input_port}},
        {{repeater.operation.id,repeater.output_port},{"",graph.output.id}}};
    check(graph.edges==expected_edges&&graph.output_mapping==MacroEndpoint{repeater.operation.id,repeater.output_port},
        "Stable ports encode the exact supported Input/Offset/Repeater/Output chain");
    Session canonical(before.document);
    canonical.apply({MacroCommand{CreateMacroDefinition{definition}}},canonical.revision());
    check(host.session.document()==canonical.document()&&host.session.history()==canonical.history(),
        "GUI creation has exactly the document and history of one canonical CreateMacroDefinition");
    const Snapshot after(host.session);dialog.accept();events();
    check(after.unchanged(host.session)&&notifications==1,"Repeated Create cannot create a duplicate or extra Undo step");
    host.session.undo(host.session.revision());
    check(host.session.document()==before.document&&encode(host.session.document())==before.native,
        "One Undo restores the entire original authored document and native bytes");
    host.session.redo(host.session.revision());
    check(host.session.document()==after.document&&encode(host.session.document())==after.native,
        "One Redo restores the same graph IDs, metadata and literal defaults");
    host.changed={};
    const auto path=directory+"/authored-macro.nect";host.save(path);host.flush();
    Host reopened(directory+"/reopened");reopened.open(path);
    check(reopened.session.document()==after.document&&encode(reopened.session.document())==after.native,
        "Real native Save and cold Host reopen preserve the full authored Macro");
    reopened.session.apply({MacroCommand{InstantiateMacro{"path",id,"authored-instance",1,0}}},reopened.session.revision());
    check(macro_parameter_value(reopened.session.document(),"path","authored-instance","macro.offset.amount")==
        12.345678901234567&&evaluate_shape(reopened.session.document(),"path",evaluate(reopened.session.document())).paths.size()==2,
        "The reopened definition is usable by the existing pinned Macro instantiation/evaluation path");
}

void cancel_and_invalid_drafts(Host& host) {
    load(host);const Snapshot before(host.session);
    {
        MacroAuthoringDialog dialog(host);dialog.show();events();
        edit(dialog,"macro-authoring-name","Unsaved draft");edit(dialog,"macro-authoring-offset-amount","42");
        QTest::mouseClick(named<QPushButton>(dialog,"macro-authoring-cancel"),Qt::LeftButton);events();
        check(dialog.result()==QDialog::Rejected&&dialog.created_definition_id().empty()&&before.unchanged(host.session),
            "Cancel discards the draft without creating a definition or history");
    }
    {
        MacroAuthoringDialog dialog(host);dialog.show();events();dialog.close();events();
        check(before.unchanged(host.session),"Closing the dialog preserves authored state");
    }
    const std::vector<std::pair<const char*,QString>> invalid{
        {"macro-authoring-name"," "},{"macro-authoring-name",QString(257,'n')},
        {"macro-authoring-name",QString::fromUtf8("日").repeated(86)},
        {"macro-authoring-public-label",""},{"macro-authoring-public-label",QString(129,'p')},
        {"macro-authoring-public-label",QString::fromUtf8("日").repeated(43)},
        {"macro-authoring-offset-amount",""},{"macro-authoring-offset-amount","nan"},
        {"macro-authoring-offset-amount","inf"},{"macro-authoring-offset-amount","1e309"},
        {"macro-authoring-offset-amount","1e-999"},{"macro-authoring-offset-amount","=1+2"},
        {"macro-authoring-offset-amount","+=1"},{"macro-authoring-offset-amount","1000000.0000000001"},
        {"macro-authoring-offset-miter_limit","0"},{"macro-authoring-repeater-copies","2.5"},
        {"macro-authoring-repeater-scale_x","0"},{"macro-authoring-repeater-end_opacity","1.1"}};
    for(const auto& [name,value]:invalid) {
        MacroAuthoringDialog dialog(host);dialog.show();events();edit(dialog,name,value);create(dialog);
        check(dialog.isVisible()&&dialog.created_definition_id().empty()&&before.unchanged(host.session)&&
            !named<QLabel>(dialog,"macro-authoring-error")->text().isEmpty(),
            "Invalid literal, range, UTF-8 byte length or label keeps the draft open and is wholly atomic");
    }
    MacroAuthoringDialog corrected(host);corrected.show();events();
    edit(corrected,"macro-authoring-repeater-copies","2.5");create(corrected);
    check(before.unchanged(host.session),"A rejected draft has no authored partial graph");
    edit(corrected,"macro-authoring-repeater-copies","2");create(corrected);
    check(host.session.revision()==before.revision+1&&host.session.document().macro_definitions.size()==1,
        "Correcting a rejected draft creates exactly one coherent definition");
}

void stale_gesture_and_lifetime(Host& host) {
    load(host);
    {
        MacroAuthoringDialog dialog(host);dialog.show();events();
        host.session.apply({Rename{"path","Changed elsewhere"}},host.session.revision());const Snapshot after(host.session);
        create(dialog);
        check(after.unchanged(host.session)&&named<QLabel>(dialog,"macro-authoring-error")->text().contains("REVISION_CONFLICT"),
            "A revision change refuses stale authoring without overwriting external edits");
    }
    load(host);
    {
        MacroAuthoringDialog dialog(host);dialog.show();events();host.create_document();const Snapshot after(host.session);
        create(dialog);
        check(after.unchanged(host.session)&&named<QLabel>(dialog,"macro-authoring-error")->text().contains("SESSION_CONFLICT"),
            "A replaced document refuses the old Macro draft even at the same revision");
    }
    load(host);
    {
        MacroAuthoringDialog dialog(host);dialog.show();events();host.session.begin_gesture(host.session.revision());
        const Snapshot before(host.session);create(dialog);
        check(before.unchanged(host.session)&&host.session.gesture_active()&&
            named<QLabel>(dialog,"macro-authoring-error")->text().contains("GESTURE_ACTIVE"),
            "Creating during an active gesture is refused without disturbing the gesture");
        host.session.cancel_gesture();create(dialog);
        check(host.session.revision()==before.revision+1&&dialog.result()==QDialog::Accepted,
            "The same draft can commit after a non-mutating gesture cancellation");
    }
    {
        QTemporaryDir directory;auto* transient=new Host(directory.path());MacroAuthoringDialog dialog(*transient);
        dialog.show();events();delete transient;create(dialog);
        check(dialog.created_definition_id().empty()&&
            named<QLabel>(dialog,"macro-authoring-error")->text().contains("SESSION_CONFLICT"),
            "An expired Host cannot be dereferenced by a surviving draft");
    }
}
}

int main(int argc,char** argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"Temporary test storage is available");
        Host host(directory.path()+"/recovery");
        author_undo_and_reopen(host,directory.path());
        cancel_and_invalid_drafts(host);
        stale_gesture_and_lifetime(host);
        std::cout<<"Macro authoring interaction checks: "<<checks<<'\n';return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
