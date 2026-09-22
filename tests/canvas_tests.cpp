#include "canvas.hpp"
#include "nect/io.hpp"

#include <QApplication>
#include <QTest>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

using namespace nect;
using nect::desktop::Canvas;

namespace {
int checks = 0;

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
    ++checks;
}

void near(double actual, double expected, const std::string& reason) {
    check(std::abs(actual - expected) < 1e-7,
          reason + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}

Document fixture_document() {
    auto document = empty_document("test-document", "test-composition", "test-artboard");
    document.compositions.front().artboards.front().width = 640;
    document.compositions.front().artboards.front().height = 480;
    Object path;
    path.id = "path";
    path.name = "Gesture fixture";
    Point first;
    first.id = "p1";
    first.x.literal = 120;
    first.y.literal = 160;
    first.in_angle.literal = 180;
    first.in_length.literal = 50;
    first.out_angle.literal = 0;
    first.out_length.literal = 60;
    Point second;
    second.id = "p2";
    second.x.literal = 360;
    second.y.literal = 240;
    second.in_angle.literal = 180;
    second.in_length.literal = 60;
    path.contours = {{"contour", false, {first, second}}};
    document.objects.emplace(path.id, path);
    document.compositions.front().roots.push_back(path.id);
    return document;
}

struct Fixture {
    Session session;
    Canvas canvas;
    QString last_error;
    int commits = 0;

    explicit Fixture(Document document = fixture_document())
        : session(std::move(document)), canvas(session) {
        canvas.error = [this](const QString& message) { last_error = message; };
        canvas.document_changed = [this] { ++commits; };
        // The single 640 x 480 artboard is centered at unit scale in this size.
        // These tests use document coordinates, independently of Canvas internals.
        canvas.resize(740, 580);
        canvas.show();
        canvas.setFocus();
        QApplication::processEvents();
        canvas.fit_artboard();
        QApplication::processEvents();
        near(canvas.zoom(), 1, "Fixture fits at unit scale");
        canvas.set_selection("path", "p1");
    }

    QPoint screen(double x, double y) const {
        return {qRound(canvas.width() / 2.0 + (x - 320) * canvas.zoom()),
                qRound(canvas.height() / 2.0 + (y - 240) * canvas.zoom())};
    }

    double value(const Id& object, const Id& point, const char* field) const {
        return evaluate(session.document()).at({object, point, field});
    }

    void press(QPoint position, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QTest::mousePress(&canvas, Qt::LeftButton, modifiers, position, 1);
    }

    void move(QPoint position) {
        QTest::mouseMove(&canvas, position, 1);
        QApplication::processEvents();
    }

    void release(QPoint position) {
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, position, 1);
        QApplication::processEvents();
    }

    void drag(QPoint start, QPoint end, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        press(start, modifiers);
        move(end);
        release(end);
    }

    void no_error() const {
        check(last_error.isEmpty(), "Unexpected Canvas error: " + last_error.toStdString());
    }
};

void point_drag_is_one_transaction() {
    Fixture f;
    const auto original = encode(f.session.document());
    const auto start = f.screen(120, 160);
    f.press(start);
    check(f.session.gesture_active(), "Point press begins a core gesture");
    f.move(start + QPoint(20, 10));
    f.move(start + QPoint(45, -25));
    check(encode(f.session.document()) == original && f.session.revision() == 0,
          "Point previews leave committed state and revision stable");
    near(evaluate(f.session.preview_document()).at({"path", "p1", "x"}), 165,
         "Latest point preview uses the starting snapshot");
    check(f.commits == 0, "Preview does not emit a committed document notification");
    f.release(start + QPoint(45, -25));
    near(f.value("path", "p1", "x"), 165, "Point drag commits X");
    near(f.value("path", "p1", "y"), 135, "Point drag commits Y");
    check(f.session.revision() == 1 && f.commits == 1 && !f.session.gesture_active(),
          "Multiple preview events commit exactly one revision and notification");
    const auto edited = encode(f.session.document());
    f.session.undo(f.session.revision());
    check(encode(f.session.document()) == original && !f.session.can_undo(),
          "One Undo restores the whole point gesture");
    f.session.redo(f.session.revision());
    check(encode(f.session.document()) == edited, "Redo restores the same point result");
    f.no_error();
}

void cancellation_and_return_home_do_not_commit() {
    Fixture f;
    const auto original = encode(f.session.document());
    const auto start = f.screen(120, 160);
    f.press(start);
    f.move(start + QPoint(70, 30));
    check(encode(f.session.preview_document()) != original, "Escape fixture has a real preview");
    QTest::keyClick(&f.canvas, Qt::Key_Escape);
    f.release(start + QPoint(70, 30));
    check(encode(f.session.document()) == original && f.session.revision() == 0 &&
          !f.session.gesture_active() && !f.session.can_undo() && f.commits == 0,
          "Escape restores the start state without revision, history or save notification");

    f.press(start);
    f.move(start + QPoint(60, -20));
    f.move(start);
    check(encode(f.session.preview_document()) == original, "Returning home restores the original preview");
    f.release(start);
    check(encode(f.session.document()) == original && f.session.revision() == 0 &&
          !f.session.can_undo() && f.commits == 0,
          "A drag returning home adds no undo entry or revision");
    f.no_error();
}

void independent_polar_handle_edits() {
    Fixture f;
    f.drag(f.screen(180, 160), f.screen(180, 220));
    near(f.value("path", "p1", "out.angle"), 45, "Outgoing handle derives clockwise angle");
    near(f.value("path", "p1", "out.length"), std::sqrt(7200.0), "Outgoing handle derives length");
    near(f.value("path", "p1", "in.angle"), 180, "Outgoing drag preserves incoming angle");
    near(f.value("path", "p1", "in.length"), 50, "Outgoing drag preserves incoming length");
    near(f.value("path", "p1", "x"), 120, "Handle drag preserves anchor X");
    near(f.value("path", "p1", "y"), 160, "Handle drag preserves anchor Y");
    f.session.undo(f.session.revision());
    f.canvas.refresh();
    f.drag(f.screen(70, 160), f.screen(70, 110));
    near(f.value("path", "p1", "in.angle"), -135, "Incoming handle derives negative angle");
    near(f.value("path", "p1", "in.length"), std::sqrt(5000.0), "Incoming handle derives length");
    near(f.value("path", "p1", "out.angle"), 0, "Incoming drag preserves outgoing angle");
    near(f.value("path", "p1", "out.length"), 60, "Incoming drag preserves outgoing length");
    f.no_error();
}

void create_and_collapse_handles_preserves_authored_angle() {
    auto document = fixture_document();
    auto& point = document.objects.at("path").contours.front().points.front();
    point.in_length.literal = point.out_length.literal = 0;
    point.in_angle.literal = 37;
    point.out_angle.literal = 22;
    Fixture f(std::move(document));
    f.drag(f.screen(120, 160), f.screen(160, 190), Qt::AltModifier);
    const auto angle = std::atan2(30.0, 40.0) * 180 / std::numbers::pi;
    near(f.value("path", "p1", "out.length"), 50, "Alt-drag creates outgoing handle");
    near(f.value("path", "p1", "in.length"), 50, "Alt-drag creates incoming handle");
    near(f.value("path", "p1", "out.angle"), angle, "Alt-drag sets outgoing angle");
    near(f.value("path", "p1", "in.angle"), angle + 180, "Alt-drag creates opposite incoming handle");
    f.drag(f.screen(160, 190), f.screen(120, 160));
    near(f.value("path", "p1", "out.length"), 0, "Handle can collapse to the anchor");
    near(f.value("path", "p1", "out.angle"), angle, "Zero length retains the authored angle");
    near(f.value("path", "p1", "in.length"), 50, "Collapsing outgoing handle preserves incoming handle");
    f.no_error();
}

void hierarchy_selection_and_inverse_coordinates() {
    auto document = fixture_document();
    auto& path = document.objects.at("path");
    path.transform[4].literal = 10;
    path.transform[5].literal = 15;
    auto& points = path.contours.front().points;
    points[0].x.literal = 100;
    points[0].y.literal = 100;
    points[0].in_length.literal = 20;
    points[0].out_length.literal = 40;
    points[1].x.literal = 150;
    points[1].y.literal = 100;
    points[1].in_length.literal = 20;
    Object group;
    group.id = "group";
    group.name = "Rotated group";
    group.kind = Kind::group;
    group.children = {"path"};
    // World X = 400 - 3 * (local Y + 15); world Y = 20 + 2 * (local X + 10).
    group.transform = {{{0, {}}, {2, {}}, {-3, {}}, {0, {}}, {400, {}}, {20, {}}}};
    document.objects.emplace(group.id, group);
    document.compositions.front().roots = {"group"};
    Fixture f(std::move(document));
    f.canvas.set_selection({});
    const auto midpoint = f.screen(55, 290);
    QTest::mouseClick(&f.canvas, Qt::LeftButton, Qt::NoModifier, midpoint, 1);
    check(f.canvas.selected_object == "group" && f.canvas.selected_point.empty(),
          "A transformed child hit selects its group by default");
    check(f.session.revision() == 0, "Selecting a group does not create history");
    f.drag(midpoint, midpoint + QPoint(20, 10));
    near(f.value("group", {}, "transform.tx"), 420, "Group drag translates the parent once");
    near(f.value("group", {}, "transform.ty"), 30, "Group drag commits parent Y");
    near(f.value("path", {}, "transform.tx"), 10, "Group drag retains the child's transform");
    f.session.undo(f.session.revision());
    f.canvas.refresh();
    QTest::mouseDClick(&f.canvas, Qt::LeftButton, Qt::NoModifier, midpoint, 1);
    f.release(midpoint);
    check(f.canvas.drill_scope() == "group" && f.canvas.selected_object == "path",
          "Double click deliberately enters the group and selects its child");
    check(f.canvas.breadcrumb().contains("Rotated group"), "Breadcrumb exposes the active group");
    f.canvas.set_selection("path", "p1");
    const auto before_drag = f.session.revision();
    f.drag(f.screen(55, 240), f.screen(85, 260));
    near(f.value("path", "p1", "x"), 110, "Point drag inverts the complete hierarchy for X");
    near(f.value("path", "p1", "y"), 90, "Point drag inverts nonuniform rotated hierarchy for Y");
    near(f.value("path", {}, "transform.tx"), 10, "Point drag keeps child translation authored");
    near(f.value("group", {}, "transform.tx"), 400, "Point drag does not rewrite parent translation");
    check(f.session.revision() == before_drag + 1, "Transformed point drag is one transaction");
    f.canvas.leave_group();
    check(f.canvas.drill_scope().empty() && f.canvas.selected_object == "group",
          "Return to parent restores default group selection");
    f.no_error();
}

void driven_coordinate_rejects_atomically_but_free_axis_can_move() {
    auto document = fixture_document();
    document.objects.at("path").contours.front().points.front().x.binding =
        Binding{{"path", "p2", "x"}, 1, -240, "copy_local_value"};
    Fixture f(std::move(document));
    const auto original = encode(f.session.document());
    f.drag(f.screen(120, 160), f.screen(160, 180));
    check(f.last_error.startsWith("DRIVEN_PROPERTY:"), "Driven drag rejection is visible to the window");
    check(encode(f.session.document()) == original && f.session.revision() == 0 &&
          !f.session.gesture_active() && !f.session.can_undo() && f.commits == 0,
          "Rejected multi-axis drag preserves binding, free axis, history and revision");
    f.last_error.clear();
    f.drag(f.screen(120, 160), f.screen(120, 190));
    near(f.value("path", "p1", "x"), 120, "Vertical drag preserves driven X value");
    near(f.value("path", "p1", "y"), 190, "Vertical drag edits the independent Y value");
    check(property(f.session.document(), {"path", "p1", "x"}).binding.has_value(),
          "Canvas never silently unlinks a driven axis");
    check(f.session.revision() == 1 && f.commits == 1, "Independent axis edit commits normally");
    f.no_error();
}
Document snap_document() {
    auto document = empty_document("snap-document", "test-composition", "test-artboard");
    document.compositions.front().artboards.front().width = 640;
    document.compositions.front().artboards.front().height = 480;
    const auto rectangle = [&](Id id, double x, double y) {
        Object object; object.id = id; object.name = id;
        object.source = default_primitive(id + "-source", "nect.shape.rectangle");
        for (auto [field, value] : std::map<std::string,double>{{"center_x",x},{"center_y",y},{"width",80},{"height",60}})
            object.source->parameters.at(field).literal = value;
        object.stack.push_back(default_operation(id + "-fill", "nect.paint.fill"));
        document.objects.emplace(id, object); document.compositions.front().roots.push_back(id);
    };
    rectangle("path", 140, 130); // edges 100..180, 100..160
    rectangle("target", 400, 300); // edges 360..440, 270..330
    return document;
}

void snap_tolerance_zoom_and_exact_edits() {
    for (double zoom : {0.5, 1.0, 2.0}) {
        Fixture f(snap_document());
        f.canvas.resize(qRound(640 * zoom + 100), qRound(480 * zoom + 100));
        // QWidget minimum prevents half-size resize; use a larger Artboard for zoom-out.
        if (zoom == 0.5) {
            auto board = f.session.document().compositions.front().artboards.front();
            board.x = -320; board.y = -240; board.width = 1280; board.height = 960;
            f.session.apply({UpdateArtboard{"test-composition",board}}, f.session.revision());
            f.canvas.resize(740,580); f.canvas.refresh();
        }
        f.canvas.fit_artboard(); near(f.canvas.zoom(), zoom, "Snap fixture zoom");
        f.canvas.set_selection("path");
        const auto original = encode(f.session.document());
        const auto start = f.screen(140,130);
        // Right edge approaches target left, four logical pixels short.
        const auto end = f.screen(320,157) - QPoint(4,0);
        f.press(start); f.move(end);
        near(evaluate(f.session.preview_document()).at({"path","","transform.tx"}),180,"Snap reaches target edge at each zoom");
        if (zoom == 1 && !qEnvironmentVariableIsEmpty("NECT_SNAP_SCREENSHOT"))
            check(f.canvas.grab().save(qEnvironmentVariable("NECT_SNAP_SCREENSHOT")), "Save live snap guide evidence");
        f.move(end + QPoint(1,0)); // frozen candidates do not drift with preview
        near(evaluate(f.session.preview_document()).at({"path","","transform.tx"}),180,"Repeated preview keeps target fixed");
        f.release(end);
        near(f.value("path",{},"transform.tx"),180,"Snapped gesture commits placement");
        f.session.undo(f.session.revision()); f.canvas.refresh();
        check(encode(f.session.document()) == original,"Single Undo restores snapped gesture exactly");
        f.canvas.set_snap_enabled(false);
        f.drag(start,end);
        near(f.value("path",{},"transform.tx"),180-4/zoom,"Snap OFF permits nearby free placement");
        f.session.undo(f.session.revision()); f.canvas.refresh(); f.canvas.set_snap_enabled(true);
        f.drag(start, f.screen(320,157) - QPoint(8,0));
        near(f.value("path",{},"transform.tx"),180-8/zoom,"Outside tolerance stays free");
        f.session.undo(f.session.revision()); f.canvas.refresh();
        f.session.apply({Set{{"path","","transform.tx"},177.125}}, f.session.revision());
        f.canvas.refresh(); near(f.value("path",{},"transform.tx"),177.125,"Exact Session numeric edit never snaps");
        f.no_error();
    }
}

void snap_cancellation_and_artboard() {
    Fixture f(snap_document()); f.canvas.set_selection("path");
    const auto original = encode(f.session.document()); const auto start = f.screen(140,130);
    f.press(start); f.move(f.screen(317,157));
    try { f.session.apply({Set{{"target","","transform.tx"},1}},f.session.revision()); check(false,"Concurrent edit must reject"); }
    catch (const Error&) { check(true,"Concurrent edit remains guarded"); }
    QTest::keyClick(&f.canvas,Qt::Key_Escape); f.release(f.screen(317,157));
    check(encode(f.session.document())==original&&!f.session.can_undo(),"Escape restores all snapped state and history");
    f.press(start); f.move(f.screen(317,157)); f.move(start); f.release(start);
    check(encode(f.session.document())==original&&!f.session.can_undo(),"Returning to press position restores exact start");
    f.drag(start,f.screen(177,236));
    near(f.value("path",{},"transform.ty"),110,"Selection center snaps to Artboard center Y");
    f.no_error();
}

void snap_multi_and_effective_followers() {
    auto document=snap_document();
    auto follower=document.objects.at("path"); follower.id="follower"; follower.source->id="follower-source";
    follower.stack.front().id="follower-fill";
    follower.source->parameters.at("center_x").literal=208;
    follower.source->parameters.at("center_y").literal=130;
    follower.transform_parent="path";
    document.objects.emplace(follower.id,follower); document.compositions.front().roots.push_back(follower.id);
    Fixture f(document); f.canvas.set_selection("path");
    f.drag(f.screen(140,130),f.screen(164,150));
    near(f.value("path",{},"transform.tx"),24,"Effective follower is excluded from snap targets");
    near(f.value("follower",{},"transform.tx"),0,"Unselected follower retains local transform");
    f.session.undo(f.session.revision()); f.canvas.refresh();
    f.canvas.set_selections({{"path",{}},{"follower",{}}});
    const auto original=encode(f.session.document());
    f.drag(f.screen(140,130),f.screen(249,150)); // union right 248 + 109 is near 360
    near(f.value("path",{},"transform.tx"),112,"Multi selection snaps aggregate bounds");
    near(f.value("follower",{},"transform.tx"),0,"Selected follower does not translate twice");
    const auto transforms=evaluate_transforms(f.session.document(),evaluate(f.session.document()));
    near(transforms.at("follower").world[4],112,"Follower preserves relative world placement");
    f.session.undo(f.session.revision()); check(encode(f.session.document())==original,"Multi snapped Undo is exact");
    f.no_error();
}

void snap_visibility_and_parent_coordinates() {
    auto document=snap_document(); document.objects.at("target").visible=false;
    document.objects.at("target").source->parameters.at("center_x").literal=430;
    Fixture hidden(document); hidden.canvas.set_selection("path");
    hidden.drag(hidden.screen(140,130),hidden.screen(347,157));
    near(hidden.value("path",{},"transform.tx"),207,"Hidden objects are not snap targets"); hidden.no_error();
    document=snap_document(); Object parent; parent.kind=Kind::group; parent.id="parent"; parent.children={"path"};
    parent.transform={{{0,{}},{2,{}},{-1,{}},{0,{}},{400,{}},{0,{}}}};
    document.objects.emplace(parent.id,parent); document.compositions.front().roots={"parent","target"};
    Fixture f(document); f.canvas.set_selection("path");
    // World bounds 240..300,200..360; right edge approaches target left.
    f.drag(f.screen(270,280),f.screen(327,297));
    near(f.value("path",{},"transform.tx"),10,"Snap maps through nonuniform rotated parent X");
    near(f.value("path",{},"transform.ty"),-60,"Snap maps world correction through parent Y");
    f.no_error();
}
void contextual_selection_and_framing() {
    auto d=fixture_document();Object group;group.id="group";group.kind=Kind::group;group.children={"path"};
    auto other=d.objects.at("path");other.id="other";other.contours.front().id="other-contour";
    for(auto& point:other.contours.front().points)point.id="other-"+point.id;
    auto hidden=other;hidden.id="hidden";hidden.contours.front().id="hidden-contour";hidden.visible=false;
    for(auto& point:hidden.contours.front().points)point.id="hidden-"+point.id;
    d.objects.emplace("group",group);d.objects.emplace("other",other);d.objects.emplace("hidden",hidden);
    d.compositions.front().roots={"group","other","hidden"};
    Fixture f(d);const auto before=f.session.document();
    f.canvas.set_selection("group");QTest::keyClick(&f.canvas,Qt::Key_A,Qt::ControlModifier);
    check(f.canvas.selected_objects()==std::vector<Id>({"group","other"}),"Select All chooses visible roots without descendants or hidden artwork");
    f.canvas.set_selection("path");f.canvas.select_all_in_context();
    check(f.canvas.drill_scope()=="group"&&f.canvas.selected_objects()==std::vector<Id>{"path"},"Select All respects current Group scope");
    f.canvas.set_selection("path","p1");f.canvas.select_all_in_context();
    check(f.canvas.selections().size()==2&&!f.canvas.selected_point.empty(),"Point context selects anchors in current objects");
    f.canvas.fit_selection();near(f.canvas.zoom(),640.0/240,"Point selection fits world anchor envelope");
    f.canvas.set_selection("path","p1");f.canvas.fit_selection();near(f.canvas.zoom(),64,"Single point framing is bounded and does not fit entire scene");
    f.canvas.set_selection({});const auto zoom=f.canvas.zoom();f.canvas.fit_selection();near(f.canvas.zoom(),zoom,"Empty selection leaves view unchanged");
    check(f.session.document()==before&&f.session.revision()==0,"Selection and framing never author changes");f.no_error();
}
void keyboard_world_placement() {
    Fixture f;const auto original=f.session.document();
    f.canvas.set_selection("path");QTest::keyClick(&f.canvas,Qt::Key_Right);QTest::keyClick(&f.canvas,Qt::Key_Down,Qt::ShiftModifier);
    near(f.value("path",{},"transform.tx"),1,"Arrow moves object one world du");near(f.value("path",{},"transform.ty"),10,"Shift arrow moves ten world du");
    check(f.commits==2,"Each key transaction notifies the shared host");f.session.undo(2);f.session.undo(3);f.canvas.refresh();check(f.session.document()==original,"Two key transactions undo exactly");
    auto d=fixture_document();d.objects.at("path").transform={{{0,{}},{2,{}},{-1,{}},{0,{}},{400,{}},{0,{}}}};
    Fixture transformed(d);transformed.canvas.set_selection("path","p1");
    QTest::keyClick(&transformed.canvas,Qt::Key_Right);
    near(transformed.value("path","p1","x"),120,"World horizontal nudge preserves local X under rotation");near(transformed.value("path","p1","y"),159,"World horizontal nudge inverse maps to local Y");
    transformed.canvas.select_all_in_context();QTest::keyClick(&transformed.canvas,Qt::Key_Down,Qt::ShiftModifier);
    near(transformed.value("path","p1","x"),125,"Rotated point selection first anchor moves");near(transformed.value("path","p2","x"),365,"Rotated point selection second anchor moves");transformed.no_error();
    d=fixture_document();d.objects.at("path").contours[0].points[0].x.binding=Binding{{"path","p2","x"},1,0,"copy_local_value"};Fixture driven(d);
    QTest::keyClick(&driven.canvas,Qt::Key_Down);check(driven.session.revision()==1,"Nudge does not touch unchanged driven axis");
    driven.canvas.select_all_in_context();const auto before=driven.session.document();QTest::keyClick(&driven.canvas,Qt::Key_Right);
    check(driven.session.document()==before&&driven.session.revision()==1&&driven.last_error.startsWith("DRIVEN_PROPERTY"),"Driven batch nudge rejects atomically");
}
void rectangle_selection_is_view_only() {
    Fixture f;const auto before=f.session.document();f.canvas.set_selection({});
    f.press(f.screen(80,100));f.move(f.screen(400,280));
    check(f.canvas.selections().empty()&&!f.session.gesture_active(),"Marquee delays selection and owns no authored gesture");
    f.release(f.screen(400,280));check(f.canvas.selected_objects()==std::vector<Id>{"path"},"Rectangle contains whole geometric path");
    f.drag(f.screen(80,100),f.screen(180,200));check(f.canvas.selections().empty(),"Partial whole-object bounds do not select");
    f.drag(f.screen(400,280),f.screen(80,100));check(f.canvas.selected_objects()==std::vector<Id>{"path"},"Rectangle works in reverse direction");
    f.canvas.set_selection("path","p2");f.drag(f.screen(80,100),f.screen(180,200),Qt::ShiftModifier);
    check(f.canvas.selections().size()==2&&!f.canvas.selected_point.empty(),"Shift marquee adds anchors within frozen point objects");
    f.drag(f.screen(80,100),f.screen(180,200));check(f.canvas.selections()==std::vector<Canvas::Selection>{{"path","p1"}},"Point marquee replaces selected anchors");
    f.press(f.screen(400,280));f.move(f.screen(80,100));QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(f.screen(80,100));
    check(f.canvas.selections()==std::vector<Canvas::Selection>{{"path","p1"}},"Escape preserves selection before marquee");
    QTest::mouseClick(&f.canvas,Qt::LeftButton,Qt::ShiftModifier,f.screen(600,400));check(f.canvas.selections().size()==1,"Shift empty click preserves selection");
    QTest::mouseClick(&f.canvas,Qt::LeftButton,Qt::NoModifier,f.screen(600,400));check(f.canvas.selections().empty(),"Empty click still clears selection");
    check(f.session.document()==before&&f.session.revision()==0&&f.commits==0,"Marquee never changes native/history");f.no_error();
    auto d=fixture_document();Object group;group.id="group";group.kind=Kind::group;group.children={"path"};d.objects.emplace("group",group);d.compositions.front().roots={"group"};
    Fixture grouped(d);grouped.canvas.set_selection("group");grouped.drag(grouped.screen(80,100),grouped.screen(400,280));
    check(grouped.canvas.selected_objects()==std::vector<Id>{"group"},"Marquee respects Group selection at root scope");
    grouped.canvas.set_selection("path");grouped.drag(grouped.screen(80,100),grouped.screen(400,280));check(grouped.canvas.selected_objects()==std::vector<Id>{"path"}&&grouped.canvas.drill_scope()=="group","Marquee selects within entered Group");
    d.objects.at("path").visible=false;Fixture hidden(d);hidden.canvas.set_selection({});hidden.drag(hidden.screen(80,100),hidden.screen(400,280));check(hidden.canvas.selections().empty(),"Hidden artwork does not become a marquee target");
}
} // namespace

int main(int argc, char** argv) {
    // Widget event/paint tests are deterministic offscreen by default. This is
    // explicitly not hardware frame/presentation performance acceptance.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    try {
        rectangle_selection_is_view_only();
        keyboard_world_placement();
        contextual_selection_and_framing();
        point_drag_is_one_transaction();
        cancellation_and_return_home_do_not_commit();
        independent_polar_handle_edits();
        create_and_collapse_handles_preserves_authored_angle();
        hierarchy_selection_and_inverse_coordinates();
        driven_coordinate_rejects_atomically_but_free_axis_can_move();
        snap_tolerance_zoom_and_exact_edits();
        snap_cancellation_and_artboard();
        snap_multi_and_effective_followers();
        snap_visibility_and_parent_coordinates();
        std::cout << "Canvas widget contract: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Canvas widget contract failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
