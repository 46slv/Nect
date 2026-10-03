#include "compatibility_plan_control.hpp"
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QVariant>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool condition,const char* reason) {
    if(!condition)throw std::runtime_error(reason);++checks;
}
template<class T>T* named(QWidget& root,const char* name) {
    auto* result=root.findChild<T*>(QString::fromLatin1(name));
    if(!result)throw std::runtime_error(std::string("Missing compatibility control: ")+name);
    return result;
}
struct Snapshot {
    Document document;
    std::string native;
    std::uint64_t revision;
    HistoryInfo history;
    bool undo,redo,gesture;
    explicit Snapshot(const Session& session):document(session.document()),native(encode(document)),
        revision(session.revision()),history(session.history()),undo(session.can_undo()),redo(session.can_redo()),
        gesture(session.gesture_active()) {}
    bool unchanged(const Session& session) const {
        return session.document()==document&&encode(session.document())==native&&session.revision()==revision&&
            session.history()==history&&session.can_undo()==undo&&session.can_redo()==redo&&session.gesture_active()==gesture;
    }
};
Object rectangle(const Id& id) {
    Object object;object.id=id;object.name="Same label";
    Contour contour;contour.id=id+"-contour";contour.closed=true;
    const std::array<Vec2,4> corners{{{2,3},{32,3},{32,23},{2,23}}};
    for(const auto& corner:corners) {
        Point point;point.id=id+"-p"+std::to_string(contour.points.size());
        point.x.literal=corner.x;point.y.literal=corner.y;contour.points.push_back(point);
    }
    object.contours.push_back(contour);object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
    return object;
}
Document fixture(bool effect=false,bool refused=false) {
    auto document=empty_document("compatibility-ui-doc","comp","board");
    document.compositions.front().name="Same label";
    document.compositions.front().artboards.front().name="Same label";
    document.compositions.front().artboards.push_back({"board-2","Same label",100,0,120,80});
    document.objects.emplace("path",rectangle("path"));
    if(effect) {
        Object group;group.id="group";group.name="Same label";group.kind=Kind::group;group.children={"path"};
        group.compositing.isolated=true;
        group.stack.push_back(default_operation("posterize","nect.group.posterize"));
        if(refused)group.compositing.blend="multiply";
        document.objects.emplace(group.id,group);document.compositions.front().roots={"group"};
    } else document.compositions.front().roots={"path"};
    auto other=empty_document("unused-doc","other-comp","other-board").compositions.front();
    other.name="Same label";other.artboards.front().name="Same label";
    document.compositions.push_back(other);
    return document;
}
void load(Host& host,Document document) {
    host.session=Session(std::move(document));host.session_id+="-fresh";
}
QJsonObject canonical(Host& host,const QString& composition="comp",const QString& artboard="board",double scale=1) {
    const QJsonObject input{{"op","compatibility_plan"},{"composition",composition},{"artboard",artboard},
        {"target_profile","svg/1.1+css-compositing"},{"options",QJsonObject{{"raster_scale",scale}}}};
    const QJsonObject envelope{{"op","core"},{"session_id",host.session_id},
        {"document_id",QString::fromStdString(host.session.document().id)},{"request",input}};
    const auto response=QJsonDocument::fromJson(host.dispatch(QJsonDocument(envelope).toJson(QJsonDocument::Compact))).object();
    check(response.value("ok").toBool(),"Canonical compatibility request succeeded");
    return response.value("result").toObject();
}
QJsonObject receipt(QDialog& dialog) {
    return QJsonObject::fromVariantMap(dialog.property("nect-compatibility-plan").toMap());
}
void events() { QApplication::processEvents(); }
void plan(QDialog& dialog) {
    auto* request=named<QPushButton>(dialog,"compatibility-plan-request");
    check(request->isEnabled(),"Request is enabled for the selected live source");
    request->click();events();
}
void choose(QDialog& dialog,const char* name,const QString& id) {
    auto* selector=named<QComboBox>(dialog,name);const auto index=selector->findData(id);
    check(index>=0,"Stable ID is available in the selector");selector->setCurrentIndex(index);
}
void check_rows(QTreeWidget& view,const QJsonArray& expected,const char* reason) {
    check(view.topLevelItemCount()==expected.size(),reason);
    for(int i=0;i<view.topLevelItemCount();++i) {
        const auto record=QJsonDocument::fromJson(view.topLevelItem(i)->data(0,Qt::UserRole).toByteArray()).object();
        check(record==expected.at(i).toObject(),"Displayed row carries the exact canonical record, including occurrence/source IDs");
    }
}
void selections_and_repeated_reads(Host& host) {
    load(host,fixture());const Snapshot before(host.session);
    auto* dialog=create_compatibility_plan_dialog(host,"comp","board-2");dialog->show();events();
    check(named<QComboBox>(*dialog,"compatibility-plan-artboard")->currentData().toString()=="board-2",
        "Factory preselects the active Artboard by ID despite duplicate display labels");
    auto* profiles=named<QComboBox>(*dialog,"compatibility-plan-profile");
    check(profiles->count()==1&&profiles->currentData().toString()=="svg/1.1+css-compositing",
        "Only the implemented SVG planning profile is offered");
    check(named<QLabel>(*dialog,"compatibility-plan-read-only")->text().contains("AI/PDF export is unavailable"),
        "Unsupported output and read-only status are visible before requesting a plan");
    plan(*dialog);const auto expected=canonical(host,"comp","board-2");
    check(receipt(*dialog)==expected,"UI retains the actual canonical API result rather than inferred classifications");
    check_rows(*named<QTreeWidget>(*dialog,"compatibility-plan-items"),expected.value("items").toArray(),
        "Every canonical item is displayed");
    check(named<QLabel>(*dialog,"compatibility-plan-summary")->text().contains("admission: allowed"),
        "The existing encoder's admission is visible separately from planning");
    plan(*dialog);check(receipt(*dialog)==expected,"Repeated requests are deterministic and replace the old display");
    choose(*dialog,"compatibility-plan-artboard","board");
    check(receipt(*dialog).isEmpty(),"Selecting another Artboard clears the old result immediately");
    plan(*dialog);check(receipt(*dialog)==canonical(host),"Explicit Artboard selection reaches the canonical API");
    choose(*dialog,"compatibility-plan-composition","other-comp");
    check(named<QComboBox>(*dialog,"compatibility-plan-artboard")->currentData().toString()=="other-board",
        "Artboards follow their owning Composition without retargeting by labels");
    plan(*dialog);check(receipt(*dialog)==canonical(host,"other-comp","other-board"),"Another Composition is planned explicitly");
    check(before.unchanged(host.session),"All selections and repeated plans preserve native state, revision, History and Undo/Redo");
    delete dialog;
}
void losses_and_refusals(Host& host) {
    for(const bool refused:{false,true}) {
        load(host,fixture(true,refused));const Snapshot before(host.session);
        auto* dialog=create_compatibility_plan_dialog(host,"comp","board");dialog->show();events();
        named<QDoubleSpinBox>(*dialog,"compatibility-plan-scale")->setValue(2);plan(*dialog);
        const auto expected=canonical(host,"comp","board",2);
        check(receipt(*dialog)==expected,"Effect planning displays the canonical scale-specific result");
        check_rows(*named<QTreeWidget>(*dialog,"compatibility-plan-losses"),expected.value("editability_losses").toArray(),
            "Canonical planned losses are displayed without adding borrowed dependencies");
        check_rows(*named<QTreeWidget>(*dialog,"compatibility-plan-bakes"),expected.value("bake_groups").toArray(),
            "Planned bake bounds, members and dependency records are displayed");
        const auto legacy=expected.value("legacy_export_plan").toObject();
        check_rows(*named<QTreeWidget>(*dialog,"compatibility-plan-legacy-refusals"),legacy.value("unsupported_effects").toArray(),
            "The current encoder's refusal remains visible even when a derivative closure is planned");
        QJsonArray unsupported;
        for(const auto& value:expected.value("items").toArray())
            if(value.toObject().value("classification")=="unsupported")unsupported.append(value);
        check_rows(*named<QTreeWidget>(*dialog,"compatibility-plan-unsupported"),unsupported,"Unsupported item reasons are visible");
        if(refused)check(!unsupported.isEmpty()&&expected.value("bake_groups").toArray().isEmpty(),
            "Unqualified backdrop closure has a real refusal and no invented executable bake");
        else check(!expected.value("bake_groups").toArray().isEmpty()&&
            !expected.value("bake_groups").toArray().first().toObject().value("execution_available").toBool(),
            "Qualified local closure remains a non-executable plan");
        QStringList warnings;
        for(const auto& warning:expected.value("warnings").toArray())warnings<<warning.toString();
        check(named<QPlainTextEdit>(*dialog,"compatibility-plan-warnings")->isReadOnly()&&
            named<QPlainTextEdit>(*dialog,"compatibility-plan-warnings")->toPlainText()==warnings.join("\n\n"),
            "All actual warnings are displayed as plain read-only text");
        check(!expected.value("derivative_generated").toBool()&&before.unchanged(host.session),
            "Planning executes no bake and cannot change the native source or History");delete dialog;
    }
}
void interrupted_and_stale_reads(Host& host) {
    load(host,fixture());auto* dialog=create_compatibility_plan_dialog(host,"comp","board");dialog->show();events();
    const Snapshot before(host.session);
    named<QPushButton>(*dialog,"compatibility-plan-request")->click();
    named<QPushButton>(*dialog,"compatibility-plan-request")->click();
    named<QPushButton>(*dialog,"compatibility-plan-cancel")->click();events();
    check(receipt(*dialog).isEmpty()&&named<QLabel>(*dialog,"compatibility-plan-status")->text().contains("cancelled"),
        "Cancel prevents queued work from repopulating the dialog");
    check(before.unchanged(host.session),"Cancel and repeated clicks leave native state and History unchanged");
    plan(*dialog);check(!receipt(*dialog).isEmpty(),"A cancelled request can be requested again");
    named<QPushButton>(*dialog,"compatibility-plan-request")->click();
    named<QDoubleSpinBox>(*dialog,"compatibility-plan-scale")->setValue(3);events();
    check(receipt(*dialog).isEmpty(),"Settings changed after request discard the queued old result");
    plan(*dialog);check(receipt(*dialog)==canonical(host,"comp","board",3),"Only the newest settings populate the result");
    named<QPushButton>(*dialog,"compatibility-plan-request")->click();
    host.session.apply({Rename{"path","Changed"}},host.session.revision());const Snapshot changed(host.session);events();
    check(receipt(*dialog).isEmpty()&&named<QLabel>(*dialog,"compatibility-plan-status")->text().contains("Source changed"),
        "A queued stale revision cannot publish a plan or retry against a newer source silently");
    check(changed.unchanged(host.session),"Stale request does not mutate or consume the edit");
    plan(*dialog);check(receipt(*dialog)==canonical(host,"comp","board",3),"An explicit new request uses the refreshed committed revision");
    host.session.apply({Rename{"path","Changed again"}},host.session.revision());QTest::qWait(230);
    check(receipt(*dialog).isEmpty(),"A displayed result is invalidated automatically after a committed edit");
    plan(*dialog);
    named<QPushButton>(*dialog,"compatibility-plan-request")->click();
    load(host,fixture());const Snapshot reopened(host.session);events();
    check(receipt(*dialog).isEmpty()&&reopened.unchanged(host.session),
        "A replacement Session with the same document ID and revision cannot accept the old request");
    plan(*dialog);
    auto* profile=named<QComboBox>(*dialog,"compatibility-plan-profile");
    profile->addItem("Invalid test profile","svg/unknown");profile->setCurrentIndex(1);plan(*dialog);
    check(receipt(*dialog).isEmpty()&&named<QLabel>(*dialog,"compatibility-plan-status")->text().contains("UNSUPPORTED_TARGET_PROFILE"),
        "The API's typed failure is visible without retaining a previous successful result");
    profile->setCurrentIndex(0);
    host.session.begin_gesture(host.session.revision());host.session.update_gesture({Set{{"path","path-p0","x"},50}});
    const Snapshot gesture(host.session);plan(*dialog);
    check(receipt(*dialog)==canonical(host,"comp","board",3)&&gesture.unchanged(host.session),
        "Read-only UI planning uses committed source and leaves an in-progress gesture untouched");
    host.session.cancel_gesture();delete dialog;

    QPointer<QDialog> closing=create_compatibility_plan_dialog(host,"comp","board");closing->show();events();
    const Snapshot closed(host.session);named<QPushButton>(*closing,"compatibility-plan-request")->click();
    named<QPushButton>(*closing,"compatibility-plan-close")->click();events();
    check(!closing||receipt(*closing).isEmpty(),"Closing prevents a queued result from surviving dismissal");
    check(closed.unchanged(host.session),"Closing a pending plan leaves native state and History unchanged");
    if(closing)delete closing.data();
}
void missing_artboard_and_host_lifetime(const QString& directory) {
    auto* host=new Host(directory+"/lifetime");auto document=fixture();document.compositions.front().artboards.clear();
    load(*host,std::move(document));auto* dialog=create_compatibility_plan_dialog(*host,"comp","board");dialog->show();events();
    check(!named<QPushButton>(*dialog,"compatibility-plan-request")->isEnabled()&&
        named<QLabel>(*dialog,"compatibility-plan-status")->text().contains("no Artboards"),
        "A Composition without Artboards cannot submit an implicit or invented frame");
    choose(*dialog,"compatibility-plan-composition","other-comp");plan(*dialog);
    named<QPushButton>(*dialog,"compatibility-plan-request")->click();delete host;events();
    check(receipt(*dialog).isEmpty()&&!named<QPushButton>(*dialog,"compatibility-plan-request")->isEnabled()&&
        named<QLabel>(*dialog,"compatibility-plan-status")->text().contains("host closed"),
        "Host destruction discards pending work without dereferencing a dead Session");delete dialog;
}
}
int main(int argc,char** argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"Temporary test storage is available");
        Host host(directory.path()+"/recovery");
        selections_and_repeated_reads(host);losses_and_refusals(host);
        interrupted_and_stale_reads(host);missing_artboard_and_host_lifetime(directory.path());
        std::cout<<"Compatibility plan UI checks: "<<checks<<'\n';return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
