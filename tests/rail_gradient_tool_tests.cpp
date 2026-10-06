#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QDir>
#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
auto snapshot(Session& s){return std::tuple{s.document(),encode(s.document()),s.revision(),s.history()};}
Document fixture(){
    auto d=empty_document("gradient-rail-document","gradient-rail-composition","gradient-rail-artboard");
    d.compositions.front().artboards.front().width=640;
    d.compositions.front().artboards.front().height=480;
    d.compositions.front().guides={{"gradient-guide","Vertical","x",280}};
    Session s(d);
    auto fill=default_operation("gradient-fill","nect.paint.fill");
    Gradient g;g.id="gradient-source";g.enabled=true;
    g.start_x.literal=-40;g.start_y.literal=-20;g.end_x.literal=40;g.end_y.literal=-20;
    GradientStop a;a.id="gradient-first";a.rgba[0].literal=1;
    GradientStop b;b.id="gradient-last";b.offset.literal=1;b.rgba[2].literal=1;g.stops={a,b};
    s.apply({CreatePrimitive{"gradient-rail-composition",{},"gradient-object","Gradient object",
        default_primitive("gradient-shape","nect.shape.rectangle")},
        AddOperation{"gradient-object",fill,0},SetGradient{"gradient-object",fill.id,g},
        Set{{"gradient-object",{},"transform.tx"},320},Set{{"gradient-object",{},"transform.ty"},240},
        CreatePrimitive{"gradient-rail-composition",{},"plain-object","Plain object",
            default_primitive("plain-shape","nect.shape.rectangle")}},s.revision());
    return s.document();
}
struct HexSnapshot {
    Document document,preview;std::string native;std::uint64_t revision,generation;HistoryInfo history;bool gesture;
    explicit HexSnapshot(const Session& s):document(s.document()),preview(s.preview_document()),native(encode(document)),
        revision(s.revision()),generation(s.gesture_generation()),history(s.history()),gesture(s.gesture_active()){}
    void unchanged(const Session& s)const {
        check(s.document()==document&&s.preview_document()==preview&&encode(s.document())==native,
            "HEX input preserves complete canonical source/native/external preview");
        check(s.revision()==revision&&s.history()==history&&s.gesture_generation()==generation&&s.gesture_active()==gesture,
            "HEX input preserves revision/history/external gesture ownership");
    }
};
void hex_context() {
    QTemporaryDir scratch;check(scratch.isValid(),"HEX regression owns recovery/settings/native files");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    settings.setValue("unrelated","preserved");
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();
    const auto events=[] {QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QTest::qWait(20);QApplication::processEvents();};
    auto& session=window.host.session;session=Session(fixture());window.host.edited();events();
    QApplication::setActiveWindow(&window);events();
    auto* tree=window.findChild<QTreeWidget*>();check(tree&&tree->isVisible(),"Actual Structure tree is reachable");
    QTreeWidgetItem* target=nullptr;
    for(QTreeWidgetItemIterator i(tree);*i;++i)
        if((*i)->data(0,Qt::UserRole).toString()=="gradient-object"&&(*i)->data(0,Qt::UserRole+1).toString().isEmpty()){target=*i;break;}
    check(target!=nullptr,"Exact retained Gradient Object row exists");tree->scrollToItem(target);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(target).center());events();
    check(window.canvas->selected_object=="gradient-object"&&window.canvas->selected_point.empty(),
        "Actual Structure click selects retained Gradient Object");
    const auto field=[&] {
        for(auto* input:window.findChildren<QLineEdit*>("gradient-stop-hex-gradient-first"))if(input->isVisible())return input;
        throw std::runtime_error("Current visible Gradient HEX field missing");
    };
    const auto draft=[&](const char* text) {
        auto* input=field();auto* parent=input->parentWidget();
        while(parent&&!qobject_cast<QScrollArea*>(parent))parent=parent->parentWidget();
        auto* scroll=qobject_cast<QScrollArea*>(parent);check(scroll&&input->isEnabled(),"Actual enabled HEX editor is in Inspector");
        scroll->ensureWidgetVisible(input);events();QTest::mouseClick(input,Qt::LeftButton);
        QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);events();
        check(input->hasFocus()&&input->isModified()&&input->text()==text,
            "Actual keyboard input leaves exact unfinished modified HEX draft focused");
        return QPointer<QLineEdit>(input);
    };
    const auto commands=[](const std::array<double,4>& rgba) {
        std::vector<Command> result;const std::array<std::string,4> fields{"r","g","b","a"};
        for(std::size_t i=0;i<fields.size();++i)result.push_back(Set{
            gradient_ref("gradient-object","gradient-fill","gradient-source","stop.gradient-first."+fields[i]),rgba[i]});
        return result;
    };
    const auto history=[&](const QString& name) {
        for(auto* action:window.findChildren<QAction*>())if(action->text()==name){action->trigger();events();return;}
        throw std::runtime_error("Existing History action missing");
    };
    const HexSnapshot initial(session);auto old=draft("#11223344");initial.unchanged(session);
    session.apply(commands({0.4,0.5,0.6,0.7}),session.revision());const HexSnapshot incoming(session);
    check(old&&old->hasFocus()&&old->text()=="#11223344"&&old->isModified(),
        "Complete external same-stop transaction arrives before unfinished focused HEX draft loses focus");
    window.host.edited();events();
    std::cout<<"HEX refresh red="<<session.document().objects.at("gradient-object").stack.front().gradient->stops.front().rgba[0].literal
        <<" revision="<<session.revision()<<" expected="<<incoming.revision<<std::endl;
    incoming.unchanged(session);
    check(field()->text()=="#668099B3","Replacement HEX editor shows incoming RGBA including alpha");
    history("Undo");check(session.document()==initial.document&&encode(session.document())==initial.native,
        "External HEX Undo restores complete prior source");
    history("Redo");check(session.document()==incoming.document&&encode(session.document())==incoming.native,
        "External HEX Redo restores complete incoming source");
    const auto commit=[&](const char* text,const std::array<double,4>& rgba) {
        const HexSnapshot before(session);auto input=draft(text);before.unchanged(session);
        Session oracle(before.document);oracle.apply(commands(rgba),oracle.revision());
        QTest::keyClick(input,Qt::Key_Return);events();
        check(session.document()==oracle.document()&&session.preview_document()==oracle.document()&&encode(session.document())==encode(oracle.document())&&
            session.revision()==before.revision+1&&session.history().states.size()==before.history.states.size()+1,
            "HEX Return commits one complete canonical RGBA transaction preserving all other source and stable IDs");
        const HexSnapshot after(session);history("Undo");
        check(session.document()==before.document&&encode(session.document())==before.native,"HEX Undo restores complete previous source");
        history("Redo");check(session.document()==after.document&&encode(session.document())==after.native,"HEX Redo restores complete edited source");
    };
    // Changed Qt channels use its normalized float representation. The displayed
    // blue byte remains 0x99, so its authored incoming double 0.6 must stay exact.
    commit("#33669980",{static_cast<float>(51/255.0),static_cast<float>(102/255.0),0.6,static_cast<float>(128/255.0)});
    const auto native=scratch.filePath("hex.nect");const HexSnapshot saved(session);window.host.save(native);events();saved.unchanged(session);
    window.host.open(native);events();
    check(session.document()==saved.document&&encode(session.document())==saved.native&&session.revision()==0&&session.history().states.size()==1,
        "Same Window native reopen preserves complete retained Gradient/geometry/stop/operation/Object identity");
    window.canvas->set_selection("gradient-object");events();const HexSnapshot before_preview(session);
    auto preview_old=draft("#AABBCCDD");before_preview.unchanged(session);
    session.begin_gesture(session.revision());session.update_gesture(commands({0.8,0.7,0.6,0.5}));const HexSnapshot preview(session);
    check(preview_old&&preview_old->hasFocus()&&preview.preview!=preview.document,"External RGBA preview starts while old HEX draft stays focused");
    window.host.edited();events();preview.unchanged(session);
    auto preview_return=draft("#22446688");preview.unchanged(session);QTest::keyClick(preview_return,Qt::Key_Return);events();preview.unchanged(session);
    // A fresh form born during a preview must remain ineligible after cancel, even when revision/generation match.
    auto cancel_old=draft("#778899AA");preview.unchanged(session);session.cancel_gesture();const HexSnapshot cancelled(session);
    check(cancel_old&&cancel_old->hasFocus()&&session.gesture_generation()==preview.generation&&session.revision()==preview.revision,
        "Canonical cancel retains revision/generation with a focused preview-born HEX draft");
    window.host.edited();events();cancelled.unchanged(session);
    check(session.document()==saved.document,"Preview cancellation/refresh retains complete native-restored source");
    commit("#102030",{static_cast<float>(16/255.0),static_cast<float>(32/255.0),static_cast<float>(48/255.0),1});
    const HexSnapshot before_reload(session);auto reload_old=draft("#DEADBEEF");before_reload.unchanged(session);
    const auto old_session=window.host.session_id;
    check(reload_old&&reload_old->hasFocus()&&reload_old->isModified(),"Same-ID native reload begins with unfinished HEX draft focused");
    window.host.open(native);events();
    // Host::open flushes current canonical edits before reloading the same native file.
    std::cout<<"reload session_changed="<<(window.host.session_id!=old_session)<<" document_equal="<<(session.document()==before_reload.document)
        <<" preview_equal="<<(session.preview_document()==before_reload.document)<<" native_equal="<<(encode(session.document())==before_reload.native)
        <<" revision="<<session.revision()<<" history="<<session.history().states.size()<<" gesture="<<session.gesture_active()<<std::endl;
    check(window.host.session_id!=old_session&&session.document()==before_reload.document&&session.preview_document()==before_reload.document&&
        encode(session.document())==before_reload.native&&session.revision()==0&&session.history().states.size()==1&&!session.gesture_active(),
        "Same-ID native reload rejects old Session draft and preserves complete flushed source without extra history");
    window.canvas->set_selection("gradient-object");events();
    const auto red=gradient_ref("gradient-object","gradient-fill","gradient-source","stop.gradient-first.r");
    session.apply({SetExpression{{red},{"0.25",1},false}},session.revision());window.host.edited();events();const HexSnapshot driven(session);
    auto driven_input=draft("#ABCDEF80");driven.unchanged(session);QTest::keyClick(driven_input,Qt::Key_Return);events();driven.unchanged(session);
    check(window.statusBar()->currentMessage().contains("DRIVEN_PROPERTY"),"Driven RGBA channel refuses complete HEX transaction atomically");
    check(settings.value("unrelated")=="preserved","HEX editing preserves unrelated workspace settings");
}
void group_drill_context() {
    QTemporaryDir scratch(qEnvironmentVariable("NECT_GRADIENT_CONTEXT_SCRATCH",QDir::tempPath())+"/gradient-group-XXXXXX");
    check(scratch.isValid(),"Gradient Group navigation owns recovery/settings/native files");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);settings.setValue("unrelated","preserved");
    Session setup(fixture());
    auto plain_fill=default_operation("plain-fill","nect.paint.fill");plain_fill.parameters.at("r").literal=1;
    auto guard_fill=default_operation("guard-fill","nect.paint.fill");guard_fill.parameters.at("b").literal=1;
    setup.apply({Set{{"plain-object",{},"transform.tx"},120},Set{{"plain-object",{},"transform.ty"},160},
        AddOperation{"plain-object",plain_fill,0},Set{{"gradient-object","gradient-shape-top-left","x"},-52},
        GroupContiguous{"gradient-rail-composition",{}, {"gradient-object","plain-object"},"gradient-group","Gradient Group"},
        CreatePrimitive{"gradient-rail-composition",{},"guard","Other artwork",default_primitive("guard-source","nect.shape.rectangle")},
        AddOperation{"guard",guard_fill,0},Set{{"guard",{},"transform.tx"},520},Set{{"guard",{},"transform.ty"},440},
        Set{{"guard",{},"generator.height"},24},
        Link{{"guard",{},"generator.width"},{{"gradient-object","gradient-shape-top-left","x"},-0.5,0,"copy_local_value"}}},setup.revision());
    auto original=setup.document();auto second=original.compositions.front().artboards.front();
    second.id="other-artboard";second.name="Other Artboard";second.x=700;original.compositions.front().artboards.push_back(second);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto& session=window.host.session;session=Session(original);session.apply({Set{{"guard",{},"transform.tx"},530}},session.revision());
    Session expected=session;window.host.edited();window.show();
    const auto events=[] {QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);};
    events();window.canvas->fit_artboard();events();window.canvas->set_snap_enabled(false);
    const auto equal=[](const Session& a,const Session& b) {
        return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&
            a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&
            a.gesture_active()==b.gesture_active()&&a.can_undo()==b.can_undo()&&a.can_redo()==b.can_redo();
    };
    const auto action=[&](const QString& name) {
        for(auto* a:window.findChildren<QAction*>())if(a->text()==name){a->trigger();events();return;}
        throw std::runtime_error("Actual Group navigation/History action missing");
    };
    const auto evidence=[&](const char* suffix) {
        const auto prefix=qEnvironmentVariable("NECT_GRADIENT_CONTEXT_EVIDENCE");
        if(!prefix.isEmpty())check(window.grab().save(prefix+suffix+".png"),"Actual Gradient Group Window evidence saved");
    };
    auto* tree=window.findChild<QTreeWidget*>();check(tree&&tree->isVisible(),"Actual Structure is reachable");tree->expandAll();
    QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator i(tree);*i;++i)if((*i)->data(0,Qt::UserRole).toString()=="gradient-object"&&
        (*i)->data(0,Qt::UserRole+1).toString().isEmpty())row=*i;
    check(row,"Exact whole Gradient child row exists");tree->scrollToItem(row);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();
    check(window.canvas->drill_scope()=="gradient-group"&&window.canvas->selected_object=="gradient-object"&&equal(session,expected),
        "Actual Structure selects exact child and its Group scope without authoring");
    auto* tool=window.findChild<QToolButton*>("tool-gradient");check(tool&&tool->isVisible()&&tool->isEnabled(),"Child exposes enabled Gradient Rail tool");
    QTest::mouseClick(tool,Qt::LeftButton);events();
    check(tool->isChecked()&&window.canvas->gradient_edit_mode()&&window.canvas->gradient_operation()=="gradient-fill"&&equal(session,expected),
        "Gradient activation targets exact existing child paint without authoring");
    action("Return to parent Group");
    check(window.canvas->drill_scope().empty()&&window.canvas->selected_object=="gradient-group"&&tool->isChecked()&&!tool->isEnabled()&&
        window.canvas->gradient_edit_mode()&&window.canvas->gradient_operation().empty()&&equal(session,expected),
        "Actual return selects ineligible parent while retaining explicit Gradient Tool and complete Session");
    const auto canvas_width=window.canvas->width();const auto* properties=window.findChild<QDockWidget*>("properties");
    check(properties,"Standard Properties pane exists");const auto pane_width=properties->width();evidence(".parent");
    const auto zoom=window.canvas->zoom();const QPoint body(qRound(window.canvas->width()/2.0),qRound(window.canvas->height()/2.0));
    QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,body);events();
    check(equal(session,expected),"First Gradient Group body click starts no authored gesture");
    QTest::mouseDClick(window.canvas,Qt::LeftButton,Qt::NoModifier,body);QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,body);events();
    std::cout<<"Gradient Group double-click scope="<<window.canvas->drill_scope()<<" object="<<window.canvas->selected_object
        <<" operation="<<window.canvas->gradient_operation()<<" revision="<<session.revision()<<" generation="<<session.gesture_generation()<<std::endl;
    evidence(".drilled");
    check(window.canvas->drill_scope()=="gradient-group"&&window.canvas->selected_object=="gradient-object"&&window.canvas->selected_point.empty(),
        "Actual Canvas double-click enters Group and selects exact whole Gradient child");
    check(tool->isChecked()&&tool->isEnabled()&&window.canvas->gradient_edit_mode()&&window.canvas->gradient_operation()=="gradient-fill"&&equal(session,expected),
        "Group drill restores exact eligible Gradient handles and preserves full source/native/History/preview/generation/Undo");
    check(session.document().objects.at("gradient-object")==original.objects.at("gradient-object")&&
        session.document().objects.at("gradient-group")==original.objects.at("gradient-group")&&
        session.document().objects.at("plain-object")==original.objects.at("plain-object"),"Drill retains entire child generator/correction/paint/Gradient stops and Group order");
    const auto values=evaluate(session.document());
    check(session.document().objects.at("guard").source->parameters.at("width").binding->source==Ref{"gradient-object","gradient-shape-top-left","x"}&&
        values.at({"guard",{},"generator.width"})==26,"Incoming stable child point Ref remains exact and evaluates independently");
    const auto artwork=Canvas::render_artboard(session.document(),"gradient-rail-composition","gradient-rail-artboard",1,false);
    check(artwork==Canvas::render_artboard(expected.document(),"gradient-rail-composition","gradient-rail-artboard",1,false)&&
        artwork.pixelColor(530,440)==QColor(Qt::blue)&&artwork.pixelColor(120,160)==QColor(Qt::red),"Drill preserves independent other artwork and canonical projection");
    const auto canvas=window.canvas->grab().toImage();const QPoint guard(qRound(canvas_width/2.0+(530-320)*zoom),
        qRound(window.canvas->height()/2.0+(440-240)*zoom));
    check(canvas.pixelColor(qRound(guard.x()*canvas.devicePixelRatio()),qRound(guard.y()*canvas.devicePixelRatio()))==QColor(Qt::blue),
        "Actual Canvas retains unrelated artwork during child handle navigation");
    action("Return to parent Group");
    check(window.canvas->drill_scope().empty()&&window.canvas->selected_object=="gradient-group"&&tool->isChecked()&&!tool->isEnabled()&&
        window.canvas->gradient_operation().empty()&&equal(session,expected),"Return after Canvas drill stays navigation-only and releases child handle target");
    action("Undo");expected.undo(expected.revision());check(equal(session,expected)&&session.document()==original,"One Undo reaches preexisting scalar edit without a Group navigation History entry");
    action("Redo");expected.redo(expected.revision());check(equal(session,expected),"Scalar Redo restores full Session after drill/return");
    const auto native=scratch.filePath("gradient-group.nect");window.host.save(native);check(equal(session,expected),"Group navigation native save is complete Session neutral");
    const auto refresh=window.host.changed;window.host.changed={};window.hide();
    Window cold(scratch.filePath("cold"),std::make_unique<FolderLibrary>(settings),&settings);cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();
    check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document())&&
        !cold.canvas->gradient_edit_mode()&&cold.canvas->drill_scope().empty(),"Fresh native Window preserves complete Group/child/source/refs/Artboards and excludes transient Tool/scope");
    check(Canvas::render_artboard(cold.host.session.document(),"gradient-rail-composition","gradient-rail-artboard",1,false)==artwork,
        "Fresh native projection preserves full artwork");cold.host.changed={};cold.hide();window.host.changed=refresh;
    check(window.canvas->width()==canvas_width&&properties->width()==pane_width&&settings.value("unrelated")=="preserved",
        "Group navigation keeps standard pane/Canvas widths and unrelated owned preferences");
    window.host.changed={};
}
void pointer_isolation() {
    QTemporaryDir scratch;check(scratch.isValid(),"Pointer regression owns recovery, settings and native files");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();
    const auto events=[] {QApplication::processEvents();};
    Session setup(fixture());setup.apply({Set{{"plain-object",{},"transform.tx"},120},
        Set{{"plain-object",{},"transform.ty"},160},
        AddOperation{"plain-object",default_operation("plain-fill","nect.paint.fill"),
            setup.document().objects.at("plain-object").stack.size()}},setup.revision());
    const auto original=setup.document();
    auto& session=window.host.session;session=Session(original);window.host.edited();events();
    window.canvas->set_snap_enabled(false);window.canvas->fit_artboard();events();
    const auto screen=[&](double x,double y){return QPoint(qRound(window.canvas->width()/2.0+(x-320)*window.canvas->zoom()),
        qRound(window.canvas->height()/2.0+(y-240)*window.canvas->zoom()));};
    const auto select_object=[&](const char* id) {
        auto* tree=window.findChild<QTreeWidget*>();check(tree&&tree->isVisible(),"Actual Structure tree is reachable");
        QTreeWidgetItem* target=nullptr;
        for(QTreeWidgetItemIterator i(tree);*i;++i)
            if((*i)->data(0,Qt::UserRole).toString()==id&&(*i)->data(0,Qt::UserRole+1).toString().isEmpty()){target=*i;break;}
        check(target!=nullptr,"Exact whole-Object row exists");tree->scrollToItem(target);events();
        QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(target).center());events();
        check(window.canvas->selected_object==id&&window.canvas->selected_point.empty(),"Actual Structure click selects exact whole Object");
    };
    select_object("gradient-object");auto* tool=window.findChild<QToolButton*>("tool-gradient");
    check(tool&&tool->isVisible()&&tool->isEnabled(),"Real Gradient Rail tool is reachable");
    const auto initial=snapshot(session);QTest::mouseClick(tool,Qt::LeftButton);events();
    check(window.canvas->gradient_edit_mode()&&tool->isChecked()&&snapshot(session)==initial,
        "Actual Gradient activation preserves complete source/revision/history");
    select_object("plain-object");
    check(tool->isChecked()&&!tool->isEnabled()&&window.canvas->gradient_operation().empty(),
        "Ineligible selection retains disabled Gradient Tool with no handle target");
    const auto probe=[&](QPoint start,const char* label) {
        check(window.canvas->rect().contains(start),"Regression pointer target lies in actual Canvas");
        const auto before=snapshot(session);const auto selection=window.canvas->selections();
        const auto finish=start+QPoint(24,18);
        QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,start);events();
        QTest::mouseMove(window.canvas,finish);events();
        const bool preview=session.gesture_active();
        QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,finish);events();
        std::cout<<label<<": preview="<<preview<<" authored_equal="<<(snapshot(session)==before)
            <<" active_gradient="<<window.canvas->gradient_edit_mode()<<" selected_point="<<window.canvas->selected_point<<'\n';
        check(!preview&&!session.gesture_active()&&snapshot(session)==before,
            "Gradient Tool pointer cannot author ordinary Path geometry or object translation");
        check(window.canvas->gradient_edit_mode()&&tool->isChecked()&&window.canvas->selections()==selection,
            "Unavailable/away-from-handle Gradient drag retains Tool and whole-Object selection");
    };
    const auto& source=*original.objects.at("plain-object").source;
    probe(screen(120-source.parameters.at("width").literal/2,160-source.parameters.at("height").literal/2),"Idle Gradient anchor drag");
    probe(screen(120,160),"Idle Gradient body drag");
    select_object("gradient-object");
    check(tool->isEnabled()&&tool->isChecked()&&window.canvas->gradient_operation()=="gradient-fill",
        "Eligible Structure selection restores exact Gradient handles");
    probe(screen(320,240),"Eligible Gradient body drag");
    // A real endpoint drag must still use the canonical Session, not become inert.
    const auto start=screen(280,220),finish=start+QPoint(24,18);const auto before=session.document();
    Session oracle(before);const auto revision=session.revision();
    const auto history=session.history().states.size();
    QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,start);events();
    QTest::mouseMove(window.canvas,finish);events();
    check(session.document()==before&&session.gesture_active(),"Actual Gradient endpoint still previews without committing source");
    QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,finish);events();
    const auto& actual=*session.document().objects.at("gradient-object").stack.front().gradient;
    const auto& old=*before.objects.at("gradient-object").stack.front().gradient;
    check(std::abs(actual.start_x.literal-old.start_x.literal-24/window.canvas->zoom())<1e-9&&
        std::abs(actual.start_y.literal-old.start_y.literal-18/window.canvas->zoom())<1e-9,
        "Independent viewport delta gives exact Gradient endpoint coordinates");
    oracle.apply({Set{gradient_ref("gradient-object","gradient-fill","gradient-source","start_x"),actual.start_x.literal},
        Set{gradient_ref("gradient-object","gradient-fill","gradient-source","start_y"),actual.start_y.literal}},oracle.revision());
    check(session.document()==oracle.document()&&session.revision()==revision+1&&session.history().states.size()==history+1,
        "Endpoint commit changes only canonical Gradient source in one undoable transaction");
    const auto authored=session.document();session.undo(session.revision());window.host.edited();events();
    check(session.document()==before,"One Undo restores complete source before endpoint drag");
    session.redo(session.revision());window.host.edited();events();check(session.document()==authored,"One Redo restores complete endpoint source");
    const auto file=scratch.filePath("gradient-pointer.nect.json");window.host.save(file);window.host.open(file);events();
    check(session.document()==authored,"Same Window native reopen preserves complete authored source");
    Window cold(scratch.filePath("cold"),std::make_unique<FolderLibrary>(settings),&settings);
    cold.setAttribute(Qt::WA_DontShowOnScreen);cold.resize(1100,750);cold.show();cold.host.open(file);events();
    check(cold.host.session.document()==authored,"New Window native reopen preserves complete authored source");
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
        if(app.arguments().contains("--group-drill-only")){group_drill_context();std::cout<<"gradient_group_drill: "<<checks<<" checks passed; physical OS input NOT_RUN\n";return 0;}
        if(app.arguments().contains("--hex-context-only")){hex_context();std::cout<<"gradient_hex_context: "<<checks<<" checks passed; physical OS input NOT_RUN\n";return 0;}
        if(app.arguments().contains("--pointer-isolation-only")){pointer_isolation();std::cout<<"gradient_pointer_isolation: "<<checks<<" checks passed\n";return 0;}
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");
        QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
        Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings));
        window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();
        auto& session=window.host.session;const auto original=fixture();
        session=Session(original);window.host.edited();window.canvas->fit_artboard();QApplication::processEvents();
        auto* gradient=window.findChild<QToolButton*>("tool-gradient");
        check(gradient&&gradient->isVisible()&&!gradient->isEnabled()&&gradient->toolTip().contains("Select one"),
            "Fixed Gradient slot is visible with an explicit no-selection reason");
        const auto click=[&](const char* name){
            auto* button=window.findChild<QToolButton*>(name);
            check(button&&button->isVisible()&&button->isEnabled(),"Requested Rail tool is visible and enabled");
            QTest::mouseClick(button,Qt::LeftButton);QApplication::processEvents();
        };
        window.canvas->set_selection("gradient-object");QApplication::processEvents();
        const auto before=snapshot(session);
        check(gradient->isEnabled()&&gradient->accessibleName().contains("Gradient")&&
            gradient->focusPolicy()==Qt::StrongFocus,"Selected eligible Object enables a named, focusable Gradient tool");
        window.canvas->set_guide_edit_mode(true);
        window.canvas->set_gradient_edit("gradient-object","gradient-fill");
        std::cout<<"Guide-to-Gradient observed guide="<<window.canvas->guide_edit_mode()
            <<" operation="<<window.canvas->gradient_operation()<<'\n';
        check(!window.canvas->guide_edit_mode()&&window.canvas->gradient_operation()=="gradient-fill",
            "Gradient activation must leave Guide mode exclusively");
        check(snapshot(session)==before,"Activation is full document/native/revision/history neutral");
        check(gradient->isChecked(),"Inspector/Canvas gradient activation synchronizes the Rail");
        QApplication::processEvents();
        QPushButton* inspector=nullptr;
        for(auto* button:window.findChildren<QPushButton*>("gradient-handles-gradient-fill"))
            if(button->isVisible())inspector=button;
        check(inspector&&inspector->text()=="Finish gradient handles","Gradient callback also preserves Inspector synchronization");
        if(app.arguments().contains("--mode-conflict-only"))return 0;
        if(app.arguments().contains("--selection-continuity-only")) {
            const auto select_object=[&](const char* id) {
                auto* tree=window.findChild<QTreeWidget*>();check(tree&&tree->isVisible(),"Real Structure navigation is reachable");
                QTreeWidgetItem* target=nullptr;
                for(QTreeWidgetItemIterator i(tree);*i;++i)
                    if((*i)->data(0,Qt::UserRole).toString()==id&&(*i)->data(0,Qt::UserRole+1).toString().isEmpty()){target=*i;break;}
                check(target!=nullptr,"Exact whole-Object Structure row exists");tree->scrollToItem(target);QApplication::processEvents();
                QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(target).center());QApplication::processEvents();
                check(window.canvas->selected_object==id&&window.canvas->selected_point.empty(),"Actual Structure click selects the exact Object");
            };
            select_object("plain-object");
            std::cout<<"Plain selection: checked="<<gradient->isChecked()<<" disabled="<<!gradient->isEnabled()
                <<" target="<<window.canvas->gradient_operation()<<" source_unchanged="<<(snapshot(session)==before)<<'\n';
            check(gradient->isChecked()&&!window.findChild<QToolButton*>("tool-selection")->isChecked(),
                "Selecting an ineligible Object retains Gradient Edit as the active Tool");
            check(!gradient->isEnabled()&&gradient->toolTip().contains("no enabled Gradient")&&
                window.canvas->gradient_operation().empty()&&snapshot(session)==before,
                "Ineligible selection clears handle target and exposes its reason without authoring");
            QTest::keyClick(window.canvas,Qt::Key_Right);QApplication::processEvents();
            check(snapshot(session)==before,"Idle Gradient Edit retains the no-artwork-nudge contract");
            select_object("gradient-object");
            check(gradient->isChecked()&&window.canvas->gradient_operation()=="gradient-fill"&&snapshot(session)==before,
                "Selecting the sole eligible Gradient restores its exact handles without reselecting the Rail");
            const auto screen=[&](double x,double y){return QPoint(qRound(window.canvas->width()/2.0+(x-320)*window.canvas->zoom()),
                qRound(window.canvas->height()/2.0+(y-240)*window.canvas->zoom()));};
            window.canvas->set_snap_enabled(false);
            window.canvas->fit_artboard();QApplication::processEvents();
            std::cout<<"Retargeted viewport="<<window.canvas->width()<<'x'<<window.canvas->height()
                <<" zoom="<<window.canvas->zoom()<<" target="<<window.canvas->gradient_operation()<<'\n';
            const auto start=screen(280,220);const auto finish=start+QPoint(20,10);const auto revision=session.revision();
            const auto history=session.history().states.size();
            QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,start);QApplication::processEvents();
            QTest::mouseMove(window.canvas,finish);QApplication::processEvents();
            check(session.document()==original&&session.gesture_active(),"Retargeted handles preview through the canonical Session gesture");
            QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,finish);QApplication::processEvents();
            auto expected_gradient=*original.objects.at("gradient-object").stack.front().gradient;
            const auto& actual=*session.document().objects.at("gradient-object").stack.front().gradient;
            std::cout.precision(17);std::cout<<"Retargeted drag actual="<<actual.start_x.literal<<','<<actual.start_y.literal
                <<" expected="<<expected_gradient.start_x.literal+20/window.canvas->zoom()<<','<<expected_gradient.start_y.literal+10/window.canvas->zoom()<<'\n';
            check(std::abs(actual.start_x.literal-(expected_gradient.start_x.literal+20/window.canvas->zoom()))<1e-8&&
                std::abs(actual.start_y.literal-(expected_gradient.start_y.literal+10/window.canvas->zoom()))<1e-8,
                "Retargeted handle uses independent screen/local displacement");
            expected_gradient.start_x.literal=actual.start_x.literal;expected_gradient.start_y.literal=actual.start_y.literal;
            Session oracle(original);oracle.apply({SetGradient{"gradient-object","gradient-fill",expected_gradient}},oracle.revision());
            check(session.document()==oracle.document()&&session.revision()==revision+1&&session.history().states.size()==history+1&&!session.gesture_active(),
                "Retargeted actual handle drag matches the complete canonical source command with one revision");
            session.undo(session.revision());window.host.edited();check(session.document()==original,"One Undo restores complete source after retargeted drag");
            session.redo(session.revision());window.host.edited();check(session.document()==oracle.document(),"One Redo restores complete retargeted edit");
            const auto edited=snapshot(session);select_object("plain-object");QTest::keyClick(window.canvas,Qt::Key_Escape);QApplication::processEvents();
            check(!gradient->isChecked()&&window.findChild<QToolButton*>("tool-selection")->isChecked()&&snapshot(session)==edited,
                "Escape exits idle Gradient Edit without modifying source/history");
            select_object("gradient-object");click("tool-gradient");select_object("plain-object");
            auto second=default_operation("second-paint","nect.paint.stroke");second.gradient=expected_gradient;
            second.gradient->id="second-gradient";second.gradient->stops[0].id="second-first";second.gradient->stops[1].id="second-last";
            session.apply({AddOperation{"gradient-object",second,1}},session.revision());window.host.edited();
            const auto ambiguous=snapshot(session);select_object("gradient-object");
            check(gradient->isChecked()&&gradient->isEnabled()&&window.canvas->gradient_operation().empty()&&
                gradient->toolTip().contains("Choose")&&snapshot(session)==ambiguous,
                "Retained Gradient Tool never chooses an implicit first paint when the next Object has multiple candidates");
            select_object("plain-object");const auto idle_source=session.document();const auto old_session=window.host.session_id;
            const auto idle_file=scratch.filePath("idle-gradient.nect.json");window.host.save(idle_file);window.host.open(idle_file);QApplication::processEvents();
            check(window.host.session_id!=old_session&&!gradient->isChecked()&&!window.canvas->gradient_edit_mode()&&
                window.canvas->gradient_operation().empty()&&session.document()==idle_source,
                "Same-ID native reopen expires idle Tool/target ownership while preserving all source");
            std::cout<<"gradient_selection_continuity: "<<checks<<" checks passed\n";return 0;
        }
        QTest::keyClick(window.canvas,Qt::Key_Escape);QApplication::processEvents();
        check(window.canvas->gradient_operation().empty()&&!gradient->isChecked()&&
            window.findChild<QToolButton*>("tool-selection")->isChecked(),"Idle Escape returns gradient handles to Selection");
        click("tool-gradient");click("tool-gradient");
        check(gradient->isChecked()&&window.canvas->gradient_operation()=="gradient-fill"&&
            window.canvas->selected_object=="gradient-object","One click activates the exact sole target; ordinary reselect keeps it active");
        check(snapshot(session)==before,"Click, reselect and Escape remain authored/native/history neutral");
        const auto position=gradient->mapTo(&window,QPoint{});
        auto* guide=window.findChild<QToolButton*>("tool-guide");
        check(gradient->mapTo(&window,QPoint{}).y()>guide->mapTo(&window,QPoint{}).y(),"Gradient is appended after existing Guide slot");
        auto* structure=window.findChild<QDockWidget*>("structure");structure->hide();QApplication::processEvents();
        check(gradient->isVisible()&&gradient->mapTo(&window,QPoint{})==position,"Structure collapse preserves Gradient slot position");
        structure->show();QApplication::processEvents();
        for(const auto* name:{"tool-pen","tool-text","tool-anchor","tool-guide","tool-selection"}){
            click("tool-gradient");click(name);
            check(window.canvas->gradient_operation().empty()&&!gradient->isChecked(),"Other Rail tools exclusively leave Gradient mode");
            check(snapshot(session)==before,"Every mode switch preserves the full document/native/revision/history");
        }
        click("tool-gradient");window.canvas->set_selection("plain-object");QApplication::processEvents();
        check(!gradient->isEnabled()&&window.canvas->gradient_operation().empty()&&gradient->toolTip().contains("no enabled Gradient"),
            "Selection change clears the exact handle target and exposes the disabled reason without creating a Gradient");
        window.canvas->set_selections({{"gradient-object",{}},{"plain-object",{}}});QApplication::processEvents();
        check(!gradient->isEnabled()&&gradient->toolTip().contains("one whole Object"),"Multiple Object selection never silently chooses a target");
        check(snapshot(session)==before,"Selection changes never author or retarget a Gradient source");
        window.canvas->set_selection("gradient-object");click("tool-gradient");window.canvas->set_snap_enabled(false);
        window.canvas->fit_artboard();QApplication::processEvents();
        if(const auto capture=qEnvironmentVariable("NECT_GRADIENT_CAPTURE");!capture.isEmpty())window.grab().save(capture);
        const QPoint start(qRound(window.canvas->width()/2.0-40*window.canvas->zoom()),
            qRound(window.canvas->height()/2.0-20*window.canvas->zoom()));
        const QPoint finish=start+QPoint(24,16);
        QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(window.canvas,finish);QApplication::processEvents();
        QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,finish);QApplication::processEvents();
        auto expected=original;auto& expected_gradient=*expected.objects.at("gradient-object").stack.front().gradient;
        expected_gradient.start_x.literal+=24/window.canvas->zoom();expected_gradient.start_y.literal+=16/window.canvas->zoom();
        const auto& actual_gradient=*session.document().objects.at("gradient-object").stack.front().gradient;
        std::cout<<"Gradient drag expected="<<expected_gradient.start_x.literal<<','<<expected_gradient.start_y.literal
            <<" actual="<<actual_gradient.start_x.literal<<','<<actual_gradient.start_y.literal<<" revision="<<session.revision()<<'\n';
        check(std::abs(actual_gradient.start_x.literal-expected_gradient.start_x.literal)<1e-7&&
            std::abs(actual_gradient.start_y.literal-expected_gradient.start_y.literal)<1e-7&&session.revision()==1,
            "Real Canvas Gradient-start drag commits the two expected coordinates once");
        expected_gradient.start_x.literal=actual_gradient.start_x.literal;
        expected_gradient.start_y.literal=actual_gradient.start_y.literal;
        check(session.document()==expected,"Gradient drag preserves every other authored field and source identity exactly");
        session.undo(session.revision());window.host.edited();check(session.document()==original,"One Undo restores the complete original Document");
        session.redo(session.revision());window.host.edited();check(session.document()==expected,"One Redo restores the complete Gradient edit");
        const auto file=scratch.filePath("gradient.nect.json");window.host.save(file);
        Host reopened(scratch.filePath("cold"));reopened.open(file);
        check(reopened.session.document()==expected,"Native Host reopen preserves complete Gradient/source/operation/stop identity");
        QApplication::processEvents();
        if(const auto capture=qEnvironmentVariable("NECT_GRADIENT_CAPTURE");!capture.isEmpty())
            check(window.grab().save(capture),"Rendered editable Window capture saved");
        const auto old_session=window.host.session_id;window.host.open(file);QApplication::processEvents();
        check(window.host.session_id!=old_session&&window.canvas->gradient_operation().empty()&&!gradient->isChecked(),
            "Native load with identical Object/operation IDs never carries an old Session handle target");
        const auto loaded=snapshot(session);
        click("tool-gradient");QTest::keyClick(window.canvas,Qt::Key_Escape);QApplication::processEvents();
        check(snapshot(session)==loaded,"Reopened activation/Escape preserves native/document/revision/history");
        auto bypassed=*session.document().objects.at("gradient-object").stack.front().gradient;bypassed.enabled=false;
        session.apply({SetGradient{"gradient-object","gradient-fill",bypassed}},session.revision());window.host.edited();
        check(!gradient->isEnabled(),"Retained disabled Gradient is not silently enabled by the Rail");
        const auto enabled_ref=gradient_ref("gradient-object","gradient-fill","gradient-source","enabled");
        session.apply({SetGradientEnabledExpression{enabled_ref,{"true",1},false}},session.revision());window.host.edited();
        check(gradient->isEnabled(),"Eligibility follows the canonical evaluated enabled expression rather than the retained literal");
        const auto driven=snapshot(session);click("tool-gradient");
        check(snapshot(session)==driven,"Activation preserves the Gradient enabled source/expression");
        QTest::keyClick(window.canvas,Qt::Key_Escape);QApplication::processEvents();
        auto stroke=default_operation("gradient-stroke","nect.paint.stroke");
        auto second=bypassed;second.id="second-gradient";second.enabled=true;second.stops[0].id="second-first";second.stops[1].id="second-last";
        session.apply({AddOperation{"gradient-object",stroke,1},SetGradient{"gradient-object",stroke.id,second}},session.revision());window.host.edited();
        const auto multiple=snapshot(session);click("tool-gradient");
        auto* menu=window.findChild<QMenu*>("gradient-tool-targets");
        check(menu&&menu->isVisible()&&menu->actions().size()==2&&window.canvas->gradient_operation().empty(),
            "Multiple eligible paints expose a choice and activate no implicit first candidate");
        auto* target=window.findChild<QAction*>("gradient-tool-target-gradient-stroke");
        check(target&&target->text().contains("Stroke")&&target->text().contains("gradient-stroke"),"Choice labels identify exact paint operation");
        QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(target).center());QApplication::processEvents();
        check(gradient->isChecked()&&window.canvas->gradient_operation()=="gradient-stroke"&&
            snapshot(session)==multiple,"Actual menu click activates only the chosen operation without authoring");
        click("tool-gradient");target=window.findChild<QAction*>("gradient-tool-target-gradient-fill");
        window.canvas->set_selection("plain-object");const auto stale_selection=snapshot(session);
        target->trigger();menu->hide();QApplication::processEvents();
        check(window.canvas->gradient_operation().empty()&&window.canvas->selected_object=="plain-object"&&
            snapshot(session)==stale_selection&&window.statusBar()->currentMessage().contains("target changed"),
            "Stale menu choice refuses after selection changes and never retargets");
        window.canvas->set_selection("gradient-object");click("tool-gradient");
        target=window.findChild<QAction*>("gradient-tool-target-gradient-fill");
        session.apply({Set{{"plain-object",{},"transform.tx"},25}},session.revision());window.host.edited();const auto stale_revision=snapshot(session);
        target->trigger();menu->hide();QApplication::processEvents();
        check(window.canvas->gradient_operation().empty()&&snapshot(session)==stale_revision,"Stale menu choice refuses after an intervening revision");
        window.canvas->set_selection("gradient-object");click("tool-gradient");
        target=window.findChild<QAction*>("gradient-tool-target-gradient-fill");
        window.host.save(file);window.host.open(file);const auto stale_load=snapshot(session);
        target->trigger();menu->hide();QApplication::processEvents();
        check(window.canvas->gradient_operation().empty()&&snapshot(session)==stale_load,"Stale menu choice refuses after same-ID native Session reopen");
        std::cout<<"rail_gradient_tool_tests: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
