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
} // namespace

int main(int argc, char** argv) {
    // Widget event/paint tests are deterministic offscreen by default. This is
    // explicitly not hardware frame/presentation performance acceptance.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    try {
        point_drag_is_one_transaction();
        cancellation_and_return_home_do_not_commit();
        independent_polar_handle_edits();
        create_and_collapse_handles_preserves_authored_angle();
        hierarchy_selection_and_inverse_coordinates();
        driven_coordinate_rejects_atomically_but_free_axis_can_move();
        std::cout << "Canvas widget contract: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Canvas widget contract failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
