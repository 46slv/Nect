#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* reason) {if(!ok)throw std::runtime_error(reason);++checks;}
void events() {QApplication::processEvents();}
const Ref amount_ref{"path","macro-instance","macro.offset.amount"};
double amount(const Session& session) {
    return macro_parameter_value(session.document(),amount_ref.object,amount_ref.point,amount_ref.field);
}
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())) {}
    bool unchanged(const Session& session) const {
        return session.document()==document&&session.revision()==revision&&session.history()==history&&
            encode(session.document())==native;
    }
};
QByteArray native_hash(const std::string& native) {
    return QCryptographicHash::hash(QByteArray::fromStdString(native),QCryptographicHash::Sha256).toHex();
}
Document fixture(double amount,bool overridden) {
    auto document=empty_document("macro-document","composition","artboard");
    Session session(document);
    auto source=default_primitive("source-stable-id","nect.shape.rectangle");
    auto offset=default_operation("offset-node","nect.shape.offset");
    offset.parameters.at("amount").literal=overridden?8:amount;
    auto repeater=default_operation("repeater-node","nect.shape.repeater");
    MacroDefinitionRevision revision;revision.revision=1;
    revision.input={"input","local_paths_and_paint"};revision.output={"output","local_paths_and_paint"};
    revision.nodes={{offset,"offset-in","offset-out"},{repeater,"repeater-in","repeater-out"}};
    revision.edges={{{"","input"},{"offset-node","offset-in"}},
        {{"offset-node","offset-out"},{"repeater-node","repeater-in"}},
        {{"repeater-node","repeater-out"},{"","output"}}};
    revision.output_mapping={"repeater-node","repeater-out"};
    revision.public_parameters.push_back({"macro.offset.amount","Amount","offset-node","amount",
        "number","du","local_paths_and_paint"});
    MacroDefinition definition;definition.id="macro-definition";definition.label="Precise Macro";
    definition.latest_revision=1;definition.revisions.emplace(1,revision);
    session.apply({CreatePrimitive{"composition","","path","Macro path",source},
        MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"path",definition.id,"macro-instance",1,1}}},session.revision());
    if(overridden)session.apply({MacroCommand{SetMacroOverride{"path","macro-instance","macro.offset.amount",amount}}},session.revision());
    const auto native=encode(session.document());const auto reopened=decode(native);
    check(reopened==session.document()&&encode(reopened)==native,"Native fixture round-trips the exact authored amount and identities");
    return reopened;
}
void load(Window& window,double amount,bool overridden) {
    window.host.session=Session(fixture(amount,overridden));window.host.session_id+="-macro";
    window.host.edited();window.canvas->set_selection("path");events();
}
QLineEdit* amount_control(Window& window) {
    for(auto* control:window.findChildren<QLineEdit*>())
        if(control->isVisible()&&control->objectName()=="macro-amount-macro-instance")return control;
    throw std::runtime_error("Missing visible Macro Amount editor");
}
void focus(Window& window,QLineEdit* control) {
    window.findChild<QScrollArea*>("inspector-scroll")->ensureWidgetVisible(control);
    control->setFocus(Qt::OtherFocusReason);events();check(control->hasFocus(),"Macro Amount takes focus");
}
void type(Window& window,const QString& text) {
    auto* control=amount_control(window);
    focus(window,control);control->selectAll();QTest::keyClick(control,Qt::Key_Backspace);QTest::keyClicks(control,text);
}
void edit(Window& window,const QString& text) {
    type(window,text);QTest::keyClick(amount_control(window),Qt::Key_Return);events();
}
void no_op(Window& window,double value,bool overridden) {
    load(window,value,overridden);auto& session=window.host.session;const Snapshot before(session);
    auto* control=amount_control(window);
    check(amount(session)==value&&control->text()==QString::number(value,'g',17),"Macro Amount displays the exact finite double");
    const auto encoded_ref=QJsonDocument(QJsonObject{{"object","path"},{"point","macro-instance"},
        {"field","macro.offset.amount"}}).toJson(QJsonDocument::Compact);
    check(control->property("nect-reference").toByteArray()==encoded_ref&&control->accessibleName().contains("du"),
        "Macro Amount keeps its canonical public Ref and document-unit meaning");
    focus(window,control);
    control->clearFocus();events();
    check(before.unchanged(session),"Focus/blur preserves native bytes, override presence, revision and history");
    focus(window,control);QTest::keyClick(control,Qt::Key_Return);events();
    check(before.unchanged(session),"Unmodified Return is an exact no-op");
    edit(window,QString::number(value,'g',17));
    check(before.unchanged(session),"Intentionally retyping the same exact value is a no-op");
    if(value==12.34567) {
        edit(window,"12.345670000000000");
        check(before.unchanged(session),"An equivalent numeric spelling neither creates nor changes an override");
    }
    control=amount_control(window);control->clearFocus();events();
    check(before.unchanged(session),"Repeated blur after a same-value edit is a no-op");
    if(value==12.34567)std::cout<<std::setprecision(17)<<"NOOP "<<(overridden?"override":"default")
        <<" before="<<value<<" after="<<amount(session)<<" revision="<<before.revision<<"->"<<session.revision()
        <<" history="<<before.history.states.size()<<"->"<<session.history().states.size()
        <<" native_sha256="<<native_hash(before.native).constData()<<"->"<<native_hash(encode(session.document())).constData()<<'\n';
}
void exact_edit(Window& window,double value,bool overridden) {
    load(window,12.34567,overridden);auto& session=window.host.session;const Snapshot before(session);
    Session canonical(before.document);
    canonical.apply({MacroCommand{SetMacroOverride{"path","macro-instance","macro.offset.amount",value}}},canonical.revision());
    type(window,QString::number(value,'g',17));
    if(overridden)amount_control(window)->clearFocus();
    else QTest::keyClick(amount_control(window),Qt::Key_Return);
    events();
    check(amount(session)==value&&session.document()==canonical.document()&&session.history()==canonical.history()&&
        session.revision()==before.revision+1,"Intentional precise edit equals one canonical SetMacroOverride command, including history and stable identities");
    const auto after=session.document();const auto native=encode(after);
    check(amount_control(window)->text()==QString::number(value,'g',17),"Refresh preserves the exact intentionally authored value");
    amount_control(window)->clearFocus();events();
    check(session.revision()==before.revision+1&&encode(session.document())==native,"Refresh and later blur do not add an edit");
    session.undo(session.revision());window.host.edited();events();
    check(session.document()==before.document&&encode(session.document())==before.native&&session.revision()==before.revision+2,
        "One Undo exactly restores the original default or override and all native identities");
    session.redo(session.revision());window.host.edited();events();
    check(session.document()==after&&encode(session.document())==native&&session.revision()==before.revision+3,
        "One Redo restores the precise authored override");
    QTemporaryDir files;Host writer(files.path()+"/writer");writer.session=Session(after);
    const auto path=files.path()+"/exact.nect";writer.save(path);writer.flush();
    Host reopened(files.path()+"/reopened");reopened.open(path);
    check(reopened.session.document()==after&&encode(reopened.session.document())==native&&amount(reopened.session)==value,
        "Save and cold Host reopen preserve the exact edited double, public parameter and source identities");
}
void refused_edits(Window& window) {
    for(const auto& text:{QString{},QString("abc"),QString("nan"),QString("inf"),QString("1e309"),
        QString("1e-999"),QString("1000000.0000000001"),QString("-1000000.0000000001"),
        QString("=1+2"),QString("+=1")}) {
        load(window,12.34567,false);auto& session=window.host.session;const Snapshot before(session);
        edit(window,text);
        check(before.unchanged(session),"Malformed, nonfinite, underflowing, out-of-range and unsupported expression/relative edits are atomic");
        check(window.statusBar()->currentMessage().contains(text.contains("1000000")?"OUT_OF_RANGE":"INVALID_VALUE"),
            "Rejected Macro Amount explains the numeric or range error");
        amount_control(window)->clearFocus();events();
        check(before.unchanged(session),"Blur after a rejected entry remains atomic");
        edit(window,"23.4567890123456");
        Session canonical(before.document);
        canonical.apply({MacroCommand{SetMacroOverride{"path","macro-instance","macro.offset.amount",23.4567890123456}}},0);
        check(session.document()==canonical.document()&&session.history()==canonical.history()&&session.revision()==before.revision+1,
            "Correcting rejected text commits one exact canonical Macro override");
    }
}
void cancel_and_stale(Window& window) {
    load(window,12.34567,false);auto& session=window.host.session;const Snapshot before(session);
    type(window,"98.7654321");QTest::keyClick(amount_control(window),Qt::Key_Escape);events();
    check(amount_control(window)->text()==QString::number(12.34567,'g',17)&&!amount_control(window)->isModified(),
        "Escape discards only the pending Macro Amount text");
    amount_control(window)->clearFocus();events();
    check(before.unchanged(session),"Escape then blur preserves exact document, revision, history and native bytes");

    load(window,12.34567,false);type(window,"98.7654321");
    session.apply({Rename{"path","Renamed elsewhere"}},session.revision());const Snapshot stale(session);
    QTest::keyClick(amount_control(window),Qt::Key_Return);events();
    check(stale.unchanged(session)&&window.statusBar()->currentMessage().contains("REVISION_CONFLICT"),
        "A stale editor cannot overwrite a newer revision");

    load(window,12.34567,true);type(window,"98.7654321");
    session=Session(fixture(3.141592653589793,false));window.host.session_id+="-replacement";const Snapshot replaced(session);
    QTest::keyClick(amount_control(window),Qt::Key_Return);events();
    check(replaced.unchanged(session)&&window.statusBar()->currentMessage().contains("SESSION_CONFLICT"),
        "A retained editor cannot target a replacement Session with the same object and instance IDs");
    window.host.edited();events();
}
void driven_contract(Window& window) {
    load(window,12.34567,false);auto& session=window.host.session;
    for(bool expression:{false,true}) {
        const Snapshot before(session);
        auto definition=session.document().macro_definitions.at("macro-definition");definition.id="driven-definition";
        auto& scalar=definition.revisions.at(1).nodes.front().operation.parameters.at("amount");
        if(expression)scalar.expression=Expression{"1+2",1};
        else scalar.binding=Binding{{"path","","generator.width"}};
        bool refused=false;
        try {session.apply({MacroCommand{CreateMacroDefinition{definition}}},session.revision());}
        catch(const Error& error) {refused=error.code=="INVALID_MACRO_NODE";}
        check(refused&&before.unchanged(session),"Macro v1 rejects driven node amounts atomically instead of exposing an editable driven literal");
    }
}
void reset_contract(Window& window) {
    load(window,12.34567,false);auto& session=window.host.session;const auto original=session.document();
    edit(window,"22.1234567890123");const Snapshot before(session);
    QPushButton* reset=nullptr;
    for(auto* button:window.findChildren<QPushButton*>())
        if(button->isVisible()&&button->objectName()=="macro-reset-amount-macro-instance")reset=button;
    check(reset,"An intentional exact override retains its Reset affordance");
    reset->click();events();
    check(session.document()==original&&session.revision()==before.revision+1&&amount(session)==12.34567,
        "Reset removes only the local override and recovers the precise pinned default");
    const Snapshot reset_state(session);auto* control=amount_control(window);focus(window,control);control->clearFocus();events();
    check(reset_state.unchanged(session),"Focus/blur after Reset does not recreate an override");
    session.undo(session.revision());window.host.edited();events();
    check(session.document()==before.document,"One Undo restores the exact reset override");
    session.redo(session.revision());window.host.edited();events();
    check(session.document()==original,"One Redo removes the reset override again");

    load(window,12.34567,true);const Snapshot dirty_before(session);
    type(window,"98.7654321");
    reset=nullptr;
    for(auto* button:window.findChildren<QPushButton*>())
        if(button->isVisible()&&button->objectName()=="macro-reset-amount-macro-instance")reset=button;
    check(reset,"Existing override exposes Reset while Amount text is dirty");
    window.findChild<QScrollArea*>("inspector-scroll")->ensureWidgetVisible(reset);
    QTest::mouseClick(reset,Qt::LeftButton);events();
    Session expected(dirty_before.document);
    expected.apply({MacroCommand{ResetMacroOverride{"path","macro-instance","macro.offset.amount"}}},0);
    std::cout<<"DIRTY_RESET amount="<<amount(session)<<" revision="<<dirty_before.revision<<"->"<<session.revision()<<'\n';
    check(session.document()==expected.document()&&session.history()==expected.history()&&session.revision()==dirty_before.revision+1,
        "Clicking Reset discards pending Amount text and executes only the canonical reset command");
    session.undo(session.revision());window.host.edited();events();
    check(session.document()==dirty_before.document,"Dirty Reset undoes directly to the original precise override without an intermediate typed edit");
    for(auto* button:window.findChildren<QPushButton*>())
        if(button->isVisible()&&button->objectName()=="macro-reset-amount-macro-instance")reset=button;
    check(reset->focusPolicy()&Qt::TabFocus,"Macro Reset retains keyboard focusability");
    reset->setFocus(Qt::TabFocusReason);events();check(reset->hasFocus(),"Keyboard navigation can focus Macro Reset");
    const auto keyboard_revision=session.revision();QTest::keyClick(reset,Qt::Key_Space);events();
    check(session.document()==expected.document()&&session.revision()==keyboard_revision+1,
        "Space activates the focused Reset as one canonical command");

    load(window,12.34567,true);type(window,"not-a-number");const Snapshot invalid_draft(session);
    for(auto* button:window.findChildren<QPushButton*>())
        if(button->isVisible()&&button->objectName()=="macro-reset-amount-macro-instance")reset=button;
    QTest::mouseClick(reset,Qt::LeftButton);events();
    check(amount(session)==8&&session.revision()==invalid_draft.revision+1&&
        session.document().objects.at("path").stack.at(1).macro->overrides.empty(),
        "Reset discards an invalid pending draft without an intermediate authored edit");

    load(window,12.34567,true);type(window,"98.7654321");
    for(auto* button:window.findChildren<QPushButton*>())
        if(button->isVisible()&&button->objectName()=="macro-reset-amount-macro-instance")reset=button;
    session.apply({Rename{"path","Renamed elsewhere"}},session.revision());const Snapshot stale(session);
    QTest::mouseClick(reset,Qt::LeftButton);events();
    check(stale.unchanged(session)&&window.statusBar()->currentMessage().contains("REVISION_CONFLICT"),
        "A dirty stale Reset refuses atomically without committing its pending Amount text");

    load(window,12.34567,true);type(window,"98.7654321");const Snapshot cancelled(session);
    for(auto* button:window.findChildren<QPushButton*>())
        if(button->isVisible()&&button->objectName()=="macro-reset-amount-macro-instance")reset=button;
    QTest::mousePress(reset,Qt::LeftButton);QTest::mouseRelease(reset,Qt::LeftButton,Qt::NoModifier,QPoint(-5,-5));events();
    check(cancelled.unchanged(session)&&amount_control(window)->isModified(),
        "A cancelled Reset mouse gesture preserves the authored state and pending text");
    control=amount_control(window);focus(window,control);control->clearFocus();events();
    check(amount(session)==98.7654321&&session.revision()==cancelled.revision+1,
        "Pending text after a cancelled Reset still commits once on a later ordinary blur");

    load(window,12.34567,true);type(window,"98.7654321");const auto tab_revision=session.revision();
    QTest::keyClick(amount_control(window),Qt::Key_Tab);events();
    check(amount(session)==98.7654321&&session.revision()==tab_revision+1,
        "Keyboard Tab remains an ordinary one-command Amount edit rather than activating Reset");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM",qEnvironmentVariableIsEmpty("NECT_QPA_PLATFORM")?QByteArray("offscreen"):qgetenv("NECT_QPA_PLATFORM"));
    QApplication app(argc,argv);
    QTemporaryDir settings;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    try {
        QTemporaryDir recovery;Window window(recovery.path());window.resize(1200,900);window.show();events();
        for(bool overridden:{false,true}) {
            for(double value:{12.34567,-12.3456789012345,1e-20,-1e-100,std::numeric_limits<double>::min(),
                std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min(),0.0,-1e6,1e6})
                no_op(window,value,overridden);
            for(double value:{22.1234567890123,-22.1234567890123,1e-20,-1e-100,std::numeric_limits<double>::min(),
                std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min(),0.0,-1e6,1e6})
                exact_edit(window,value,overridden);
        }
        refused_edits(window);cancel_and_stale(window);driven_contract(window);reset_contract(window);
        std::cout<<"PASS Macro Amount UI "<<checks<<" checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
