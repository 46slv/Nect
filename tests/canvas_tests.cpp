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

Document grid_guide_snap_document(double guide_x,std::size_t columns=1,double gutter=0) {
    auto document=snap_document();
    auto& composition=document.compositions.front();
    composition.guides.push_back({"guide-snap","Snap guide","x",guide_x});
    document.compositions.front().artboards.front().layout=ArtboardLayout{
        std::nullopt,Grid{"grid-snap",{200,20,100,100},columns,1,gutter,0}};
    return document;
}

Document equal_gap_snap_document() {
    auto document=snap_document();
    document.objects.erase("target");
    std::erase(document.compositions.front().roots,Id("target"));
    const auto rectangle=[&](Id id,double center_x,double center_y,double width,double height) {
        Object object;object.id=id;object.name=id;
        object.source=default_primitive(id+"-source","nect.shape.rectangle");
        for(const auto& [field,value]:std::map<std::string,double>{{"center_x",center_x},{"center_y",center_y},
                {"width",width},{"height",height}})object.source->parameters.at(field).literal=value;
        object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
        document.objects.emplace(id,object);document.compositions.front().roots.push_back(id);
    };
    rectangle("left",120,390,40,40);  // bounds 100..140
    rectangle("right",340,390,40,40); // bounds 320..360
    return document;
}

void add_snap_rectangle(Document& document,const Id& id,double center_x,double center_y,double width,double height) {
    Object object;object.id=id;object.name=id;
    object.source=default_primitive(id+"-source","nect.shape.rectangle");
    for(const auto& [field,value]:std::map<std::string,double>{{"center_x",center_x},{"center_y",center_y},
            {"width",width},{"height",height}})object.source->parameters.at(field).literal=value;
    object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
    document.objects.emplace(id,std::move(object));document.compositions.front().roots.push_back(id);
}

Document repeated_gap_snap_document(bool vertical=false) {
    auto document=empty_document("repeated-gap-document","test-composition","test-artboard");
    document.compositions.front().artboards.front().width=640;
    document.compositions.front().artboards.front().height=480;
    if(vertical) {
        add_snap_rectangle(document,"gap-a",180,120,40,40);  // Y bounds 100..140
        add_snap_rectangle(document,"gap-b",180,200,40,40);  // Y bounds 180..220
        add_snap_rectangle(document,"moving",300,440,60,80); // Y bounds 400..480
    } else {
        add_snap_rectangle(document,"gap-a",120,180,40,40);  // X bounds 100..140
        add_snap_rectangle(document,"gap-b",200,180,40,40);  // X bounds 180..220
        add_snap_rectangle(document,"moving",440,300,80,60); // X bounds 400..480
    }
    return document;
}

Document point_snap_document(bool driven=false) {
    auto document=empty_document("point-snap-document","test-composition","test-artboard");
    document.compositions.front().artboards.front().width=640;
    document.compositions.front().artboards.front().height=480;
    document.compositions.front().guides.push_back({"point-guide","Point target","x",125});
    Object path;path.id="path";path.name="Point source";
    Point first;first.id="p1";first.x.literal=10;first.y.literal=20;
    Point second;second.id="p2";second.x.literal=20;second.y.literal=30;
    path.contours={{"point-contour",false,{first,second}}};
    if(driven)path.contours.front().points.front().x.binding=Binding{{"path","p2","x"},1,-10,"copy_local_value"};
    Object parent;parent.id="parent";parent.name="Scaled parent";parent.kind=Kind::group;parent.children={"path"};
    parent.transform={{{2,{}},{0,{}},{0,{}},{2,{}},{100,{}},{50,{}}}};
    document.objects.emplace(path.id,path);document.objects.emplace(parent.id,parent);
    document.compositions.front().roots={parent.id};
    return document;
}

Document text_baseline_snap_document(bool vertical_source=false,bool rotated_target=false) {
    auto document=empty_document("text-snap-document","test-composition","test-artboard");
    document.compositions.front().artboards.front().width=640;
    document.compositions.front().artboards.front().height=480;
    const auto add_text=[&](Id id,double x,double y) {
        Object object;object.id=id;object.name=id;object.kind=Kind::text;
        object.text=default_text(id+"-source","Baseline");
        object.text->parameters.at("origin_x").literal=x;
        object.text->parameters.at("origin_y").literal=y;
        object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
        document.objects.emplace(id,std::move(object));document.compositions.front().roots.push_back(id);
    };
    add_text("moving-text",40,80);add_text("target-text",320,96);
    if(vertical_source)document.objects.at("moving-text").text->direction="vertical";
    if(rotated_target)document.objects.at("target-text").transform={{{0,{}},{1,{}},{-1,{}},{0,{}},{0,{}},{0,{}}}};
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
        if(!qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR"))
            near(f.canvas.devicePixelRatioF(),qEnvironmentVariable("QT_SCALE_FACTOR").toDouble(),
                "Offscreen fixture honors the requested device-pixel ratio");
        f.canvas.set_selection("path");
        const auto original = encode(f.session.document());
        const auto start = f.screen(140,130);
        // Right edge approaches target left, five logical pixels short.
        const auto end = f.screen(320,157) - QPoint(5,0);
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
        near(f.value("path",{},"transform.tx"),180-5/zoom,"Snap OFF permits nearby free placement");
        f.session.undo(f.session.revision()); f.canvas.refresh(); f.canvas.set_snap_enabled(true);
        f.drag(start, f.screen(320,157) - QPoint(7,0));
        near(f.value("path",{},"transform.tx"),180-7/zoom,"Outside tolerance stays free");
        f.session.undo(f.session.revision()); f.canvas.refresh();
        f.drag(start,f.screen(320,157)-QPoint(6,0));
        near(f.value("path",{},"transform.tx"),180,"Exactly six logical pixels remains inside the Snap threshold");
        f.session.undo(f.session.revision());f.canvas.refresh();
        f.session.apply({Set{{"path","","transform.tx"},177.125}}, f.session.revision());
        f.canvas.refresh(); near(f.value("path",{},"transform.tx"),177.125,"Exact Session numeric edit never snaps");
        f.no_error();
    }
}

void snap_guide_grid_priority_visibility_and_controls() {
    {
        auto document=grid_guide_snap_document(200);
        document.compositions.front().guides.push_back({"guide-a","Stable ID tie winner","x",200});
        Fixture f(document);f.canvas.set_selection("path");
        f.canvas.set_show_guides(false);f.canvas.set_show_grid(false);
        const auto start=f.screen(140,130),end=f.screen(156,130);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"path","","transform.tx"}),20,
            "Exact Guide/Grid tie snaps the moving maximum to the shared line");
        const auto tie_feedback=f.canvas.last_snap_feedback().toStdString();
        check(f.canvas.last_snap_feedback().contains("Guide → guide-a")&&
              f.canvas.last_snap_feedback().contains("max")&&
              f.canvas.last_snap_feedback().contains("guide line"),
            "Hidden Guide feedback mismatch: "+tie_feedback);
        f.release(end);f.no_error();
    }
    {
        Fixture f(grid_guide_snap_document(202));f.canvas.set_selection("path");
        f.canvas.set_show_guides(false);f.canvas.set_show_grid(false);
        const auto start=f.screen(140,130),end=f.screen(156,130);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"path","","transform.tx"}),20,
            "Closer Grid target wins over a farther Guide target");
        check(f.canvas.last_snap_feedback().contains("Grid → grid-snap")&&
              f.canvas.last_snap_feedback().contains("column 1 boundary"),
            "Hidden Grid overlay still snaps and feedback identifies its line feature");
        f.release(end);f.no_error();
    }
    {
        Fixture f(grid_guide_snap_document(200,2,20));f.canvas.set_selection("path");
        const auto start=f.screen(140,130),end=f.screen(176,130);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"path","","transform.tx"}),40,
            "Grid cell center is a snap candidate independently of its boundary");
        check(f.canvas.last_snap_feedback().contains("Grid → grid-snap")&&
              f.canvas.last_snap_feedback().contains("column 1 center"),
            "Grid cell-center feedback names the stable Grid and column feature");
        f.release(end);f.no_error();
    }
    {
        Fixture f(grid_guide_snap_document(200));f.canvas.set_selection("path");
        f.canvas.set_snap_grid_enabled(false);
        const auto start=f.screen(140,130),end=f.screen(156,130);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"path","","transform.tx"}),20,
            "Guide Snap remains enabled when Grid Snap is disabled");
        check(f.canvas.last_snap_feedback().contains("Guide → guide-snap"),
            "Grid Snap OFF leaves the same-position Guide candidate active");
        f.release(end);f.no_error();
    }
    {
        Fixture f(grid_guide_snap_document(200));f.canvas.set_selection("path");
        f.canvas.set_snap_guides_enabled(false);f.canvas.set_snap_grid_enabled(false);
        const auto start=f.screen(140,130),end=f.screen(156,130);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"path","","transform.tx"}),16,
            "Visible overlays do not qualify after their independent Snap toggles are disabled");
        check(f.canvas.last_snap_feedback().isEmpty(),"Disabled Guide and Grid candidates publish no Snap feedback");
        f.release(end);f.no_error();
    }
    {
        Fixture f(grid_guide_snap_document(200));f.canvas.set_selection("path");
        f.canvas.set_snap_enabled(false);
        const auto start=f.screen(140,130),end=f.screen(156,130);
        f.drag(start,end);
        near(f.value("path",{},"transform.tx"),16,"Master Snap OFF keeps pointer placement free");
        check(f.canvas.last_snap_feedback().isEmpty(),"Master Snap OFF has no transient snap feedback");
        f.no_error();
    }
}

void snap_two_sided_equal_gap_and_point_world_correction() {
    {
        Fixture f(equal_gap_snap_document());f.canvas.set_selection("path");
        const auto start=f.screen(140,130),end=f.screen(228,130);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"path","","transform.tx"}),90,
            "Two-sided equal-gap candidate places the moving bounds at the computed equal-gap position");
        check(f.canvas.last_snap_feedback().contains("Equal gap")&&
              f.canvas.last_snap_feedback().contains("left")&&f.canvas.last_snap_feedback().contains("right"),
            "Equal-gap feedback identifies both stable neighboring objects");
        f.release(end);
        near(f.value("path",{},"transform.tx"),90,"Equal-gap placement commits through one object command");
        f.session.undo(f.session.revision());f.canvas.refresh();
        near(f.value("path",{},"transform.tx"),0,"One Undo restores the equal-gap gesture");f.no_error();
    }
    {
        Fixture f(point_snap_document());
        f.canvas.set_selections({{"path","p2"},{"path","p1"}});
        const auto original=encode(f.session.document());
        const auto start=f.screen(120,90),end=start+QPoint(11,0);
        f.press(start);f.move(end);
        const auto preview=evaluate(f.session.preview_document());
        near(preview.at({"path","p1","x"}),12.5,"Grabbed point world correction maps through its frozen inverse");
        near(preview.at({"path","p2","x"}),22.5,"All selected anchors receive the same world correction");
        near(preview.at({"path","p1","y"}),20,"Point Snap leaves the independent Y coordinate unchanged");
        check(f.canvas.last_snap_feedback().contains("Guide → point-guide"),
            "Point Snap feedback names the Guide target");
        f.release(end);
        check(f.session.revision()==1&&f.session.can_undo(),"Multi-point snapped gesture commits one history entry");
        f.session.undo(f.session.revision());f.canvas.refresh();
        check(encode(f.session.document())==original,"One Undo restores every selected point exactly");f.no_error();
    }
    {
        Fixture f(point_snap_document(true));f.canvas.set_selections({{"path","p2"},{"path","p1"}});
        const auto original=encode(f.session.document());const auto start=f.screen(120,90),end=start+QPoint(11,0);
        f.press(start);f.move(end);f.release(end);
        check(f.last_error.startsWith("DRIVEN_PROPERTY:")&&encode(f.session.document())==original&&
              f.session.revision()==0&&!f.session.can_undo()&&!f.session.gesture_active(),
            "A driven selected anchor rejects the full snapped multi-point edit atomically");
    }
}

void snap_repeated_gap_fixed_oracles_and_eligibility() {
    for(const bool vertical:{false,true}) {
        const auto start=vertical?QPoint(300,440):QPoint(440,300);
        const auto after_raw=vertical?QPoint(300,303):QPoint(303,300); // moving min 263, three du from 260
        const auto before_raw=vertical?QPoint(300,17):QPoint(17,300); // moving max 57, three du from 60
        const auto field=vertical?"transform.ty":"transform.tx";
        const auto axis=vertical?QStringLiteral("Y:"):QStringLiteral("X:");
        {
            Fixture f(repeated_gap_snap_document(vertical));f.canvas.set_selection("moving");
            const auto original=encode(f.session.document());
            f.press(f.screen(start.x(),start.y()));f.move(f.screen(after_raw.x(),after_raw.y()));
            const auto expected=axis+QStringLiteral(" min Equal gap");
            const auto feedback=f.canvas.last_snap_feedback();
            check(feedback.contains(expected)&&feedback.contains("gap-a")&&feedback.contains("gap-b")&&
                  feedback.contains("repeated gap between gap-a and gap-b after gap-b"),
                "After repeated-gap feedback names the axis, moving minimum, both targets and side: "+feedback.toStdString());
            f.release(f.screen(after_raw.x(),after_raw.y()));
            near(f.value("moving",{},field),-140,vertical?"Y after repeated-gap fixed coordinate":"X after repeated-gap fixed coordinate");
            check(f.session.revision()==1&&f.session.can_undo(),"Repeated-gap release commits one transaction");
            f.session.undo(f.session.revision());f.canvas.refresh();
            check(encode(f.session.document())==original,"One Undo restores the after repeated-gap gesture exactly");f.no_error();
        }
        {
            Fixture f(repeated_gap_snap_document(vertical));f.canvas.set_selection("moving");
            f.press(f.screen(start.x(),start.y()));f.move(f.screen(before_raw.x(),before_raw.y()));
            const auto expected=axis+QStringLiteral(" max Equal gap");
            const auto feedback=f.canvas.last_snap_feedback();
            check(feedback.contains(expected)&&feedback.contains("gap-a")&&feedback.contains("gap-b")&&
                  feedback.contains("repeated gap between gap-a and gap-b before gap-a"),
                "Before repeated-gap feedback names the axis, moving maximum, both targets and side: "+feedback.toStdString());
            f.release(f.screen(before_raw.x(),before_raw.y()));
            near(f.value("moving",{},field),-420,vertical?"Y before repeated-gap fixed coordinate":"X before repeated-gap fixed coordinate");
            f.no_error();
        }
        {
            Fixture f(repeated_gap_snap_document(vertical));f.canvas.set_selection("moving");
            const auto outside=vertical?QPoint(300,307):QPoint(307,300); // seven du from the after candidate
            f.drag(f.screen(start.x(),start.y()),f.screen(outside.x(),outside.y()));
            near(f.value("moving",{},field),-133,vertical?"Y repeated gap outside threshold stays free":"X repeated gap outside threshold stays free");
            check(!f.canvas.last_snap_feedback().contains("repeated gap"),"Seven-du candidate has no repeated-gap feedback");
            f.no_error();
        }
    }

    {
        auto document=repeated_gap_snap_document();
        document.objects.at("gap-b").source->parameters.at("center_x").literal=150; // [130,170] overlaps A [100,140]
        Fixture f(document);f.canvas.set_selection("moving");
        f.drag(f.screen(440,300),f.screen(203,300)); // raw moving min 163 is three du from the invalid repeated candidate 160
        near(f.value("moving",{},"transform.tx"),-237,"Overlapping pair offers no repeated-gap candidate");
        check(!f.canvas.last_snap_feedback().contains("repeated gap"),"Overlapping pair publishes no repeated-gap feedback");f.no_error();
    }

    const auto after_fixture=[](Document document,const std::string& label) {
        Fixture f(std::move(document));f.canvas.set_selection("moving");
        f.press(f.screen(440,300));f.move(f.screen(303,300));
        const auto feedback=f.canvas.last_snap_feedback();
        check(feedback.contains("X: min Equal gap")&&feedback.contains("gap-a")&&feedback.contains("gap-b")&&
              feedback.contains("repeated gap between gap-a and gap-b after gap-b"),
            label+" does not block or replace the eligible repeated-gap candidate: "+feedback.toStdString());
        f.release(f.screen(303,300));near(f.value("moving",{},"transform.tx"),-140,label+" leaves the exact repeated-gap placement");
        f.no_error();
    };
    const auto blocker=[](Document document) {
        add_snap_rectangle(document,"blocker",332.5,180,5,40); // [330,335], overlaps [260,340] but stays outside every six-du source feature
        return document;
    };

    {
        auto document=blocker(repeated_gap_snap_document());
        Fixture f(document);f.canvas.set_selection("moving");
        f.drag(f.screen(440,300),f.screen(303,300));
        near(f.value("moving",{},"transform.tx"),-137,"Visible stationary overlap omits the after candidate");
        check(!f.canvas.last_snap_feedback().contains("repeated gap"),"Blocked after candidate has no repeated-gap feedback");f.no_error();
    }
    {
        auto document=blocker(repeated_gap_snap_document());document.objects.at("blocker").visible=false;
        after_fixture(std::move(document),"Hidden object");
    }
    {
        auto document=repeated_gap_snap_document();document.objects.at("gap-a").visible=false;document.objects.at("gap-b").visible=false;
        Fixture f(document);f.canvas.set_selection("moving");
        f.drag(f.screen(440,300),f.screen(303,300));
        near(f.value("moving",{},"transform.tx"),-137,"Hidden pair cannot seed a repeated-gap candidate");
        check(!f.canvas.last_snap_feedback().contains("repeated gap"),"Hidden pair publishes no repeated-gap feedback");f.no_error();
    }
    {
        auto document=empty_document("moving-seed-document","test-composition","test-artboard");
        document.compositions.front().artboards.front().width=640;document.compositions.front().artboards.front().height=480;
        add_snap_rectangle(document,"gap-a",120,180,40,40);add_snap_rectangle(document,"moving",200,300,40,60);
        Fixture f(document);f.canvas.set_selection("moving");
        f.drag(f.screen(200,300),f.screen(283,300));
        near(f.value("moving",{},"transform.tx"),83,"Moving object cannot seed its own repeated-gap pair");
        check(!f.canvas.last_snap_feedback().contains("repeated gap"),"Moving object publishes no repeated-gap feedback as a target");f.no_error();
    }
    {
        auto document=repeated_gap_snap_document();std::erase(document.compositions.front().roots,Id("gap-b"));document.objects.erase("gap-b");
        add_snap_rectangle(document,"follower",200,180,40,40);document.objects.at("follower").transform_parent="moving";
        Fixture f(document);f.canvas.set_selection("moving");
        f.drag(f.screen(440,300),f.screen(303,300));
        near(f.value("moving",{},"transform.tx"),-137,"Effective follower cannot seed a repeated-gap pair");
        check(!f.canvas.last_snap_feedback().contains("repeated gap"),"Effective follower publishes no repeated-gap feedback as a target");f.no_error();
    }
    {
        auto document=repeated_gap_snap_document();
        std::erase(document.compositions.front().roots,Id("gap-a"));std::erase(document.compositions.front().roots,Id("gap-b"));
        auto other=document.compositions.front();other.id="other-composition";other.name="Other composition";
        other.roots={"gap-a","gap-b"};other.artboards.front().id="other-artboard";
        document.compositions.push_back(std::move(other));
        Fixture f(document);f.canvas.set_selection("moving");
        f.drag(f.screen(440,300),f.screen(303,300));
        near(f.value("moving",{},"transform.tx"),-137,"Cross-composition pair cannot seed a repeated-gap candidate");
        check(!f.canvas.last_snap_feedback().contains("repeated gap"),"Cross-composition pair publishes no repeated-gap feedback");f.no_error();
    }
    {
        auto document=repeated_gap_snap_document();
        Object group;group.id="scope";group.kind=Kind::group;group.children={"moving"};document.objects.emplace(group.id,group);
        std::erase(document.compositions.front().roots,Id("moving"));document.compositions.front().roots.push_back("scope");
        Fixture f(document);f.canvas.set_selection("moving");
        f.drag(f.screen(440,300),f.screen(303,300));
        near(f.value("moving",{},"transform.tx"),-137,"Out-of-scope pair cannot seed a repeated-gap candidate");
        check(!f.canvas.last_snap_feedback().contains("repeated gap"),"Out-of-scope pair publishes no repeated-gap feedback");f.no_error();
    }
    {
        auto document=blocker(repeated_gap_snap_document());document.objects.at("blocker").transform_parent="moving";
        after_fixture(std::move(document),"Effective follower");
    }
    {
        auto document=blocker(repeated_gap_snap_document());std::erase(document.compositions.front().roots,Id("blocker"));
        auto other=document.compositions.front();other.id="other-composition";other.name="Other composition";other.roots={"blocker"};
        other.artboards.front().id="other-artboard";document.compositions.push_back(std::move(other));
        after_fixture(std::move(document),"Cross-composition object");
    }
    {
        auto document=blocker(repeated_gap_snap_document());
        Object group;group.id="scope";group.kind=Kind::group;group.children={"gap-a","gap-b","moving"};
        document.objects.emplace(group.id,group);
        std::erase(document.compositions.front().roots,Id("gap-a"));std::erase(document.compositions.front().roots,Id("gap-b"));
        std::erase(document.compositions.front().roots,Id("moving"));document.compositions.front().roots.push_back("scope");
        after_fixture(std::move(document),"Out-of-scope object");
    }
}

void snap_scope_and_singular_point_rejections() {
    {
        auto document=snap_document();
        document.objects.at("target").source->parameters.at("center_x").literal=540;
        Object parent;parent.id="scope-parent";parent.name="Entered scope";parent.kind=Kind::group;parent.children={"path"};
        document.objects.emplace(parent.id,parent);
        document.compositions.front().roots={parent.id,"target"};
        Fixture f(document);f.canvas.set_selection("path");
        check(f.canvas.drill_scope()=="scope-parent","Selecting the nested object enters its Group scope");
        const auto start=f.screen(140,130),end=f.screen(460,157)-QPoint(5,0);
        f.drag(start,end);
        near(f.value("path",{},"transform.tx"),315,
            "An object in a different Group scope cannot attract the moving child");
        f.no_error();
    }
    {
        auto document=point_snap_document();document.objects.at("parent").transform[0].literal=0;
        Fixture f(document);f.canvas.set_selections({{"path","p2"},{"path","p1"}});
        const auto original=encode(f.session.document());const auto start=f.screen(100,90),end=start+QPoint(11,0);
        f.press(start);f.move(end);f.release(end);
        check(f.last_error.startsWith("SINGULAR_TRANSFORM:")&&encode(f.session.document())==original&&
              f.session.revision()==0&&!f.session.can_undo()&&!f.session.gesture_active(),
            "A singular selected-point inverse rejects without changing any authored point or history");
    }
    {
        Fixture f(snap_document());f.canvas.set_selection("path");
        QString identity="session-one";f.canvas.set_session_identity_provider([&]{return identity;});
        const auto original=encode(f.session.document());const auto start=f.screen(140,130),end=start+QPoint(11,0);
        f.press(start);identity="session-two";f.move(end);f.release(end);
        check(f.last_error.startsWith("REVISION_CONFLICT:")&&encode(f.session.document())==original&&
              f.session.revision()==0&&!f.session.can_undo()&&!f.session.gesture_active(),
            "A stale Snap session identity cancels without rebasing or creating history");
    }
}

void snap_text_baseline_and_unsupported_axis_omission() {
#ifdef _WIN32
    const auto text_parameters=[](const TextSource& source) {
        std::map<std::string,double> values;
        for(const auto& [name,scalar]:source.parameters)values[name]=scalar.literal;
        return values;
    };
    {
        auto document=text_baseline_snap_document();
        auto& target=*document.objects.at("target-text").text;
        target.content="H";
        target.parameters.at("origin_y").literal=80;
        target.parameters.at("line_spacing").literal=80;
        auto& source=*document.objects.at("moving-text").text;
        source.content="H\nH";source.parameters.at("line_spacing").literal=80;
        const auto target_layout=evaluate_text(target,text_parameters(target));
        const auto source_layout=evaluate_text(source,text_parameters(source));
        check(source_layout.line_baselines_y.size()==2,"Moving multiline Text exposes both measured baseline sources");
        near(target_layout.line_baselines_y[0],source_layout.line_baselines_y[0],
            "Matching line spacing gives the stationary and moving first lines the same measured baseline");
        near(source_layout.line_baselines_y[1]-source_layout.line_baselines_y[0],80,
            "Moving source line 2 uses its measured DirectWrite baseline");
        near(target_layout.line_baselines_y[0]-source_layout.line_baselines_y[1],-80,
            "Moving source line 2 is 80 du above the stationary first-line target");
        document.compositions.front().artboards.front().y=20;
        Fixture f(document);f.canvas.set_selection("moving-text");
        const auto screen=[&](double x,double y) {
            return QPoint(qRound(f.canvas.width()/2.0+(x-320)*f.canvas.zoom()),
                          qRound(f.canvas.height()/2.0+(y-260)*f.canvas.zoom()));
        };
        const auto start=screen(source_layout.x+source_layout.width/2,source_layout.y+source_layout.height/2);
        const auto end=start+QPoint(0,-79);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"moving-text","","transform.ty"}),-80,
            "Raw -79-du drag snaps moving source line 2 to target first line");
        check(f.canvas.last_snap_feedback().contains("line 2 baseline Text baseline → target-text first-line baseline"),
            "Baseline Snap feedback names source line 2 and target first line: "+f.canvas.last_snap_feedback().toStdString());
        f.release(end);
        near(f.value("moving-text",{},"transform.ty"),-80,"Line-2 Snap commits the expected shared translation");
        check(f.session.revision()==1&&f.commits==1,"Moving-baseline release is one Session transaction");
        check(decode(encode(f.session.document()))==f.session.document(),"Native serialization retains the accepted baseline translation");
        f.session.undo(f.session.revision());
        near(f.value("moving-text",{},"transform.ty"),0,"One Undo restores the moving Text transform");
        check(!f.session.can_undo(),"Moving-baseline Snap creates exactly one Undo step");f.no_error();
    }
    {
        auto document=text_baseline_snap_document();
        const auto& source=*document.objects.at("moving-text").text;
        const auto layout=evaluate_text(source,text_parameters(source));
        check(layout.first_line_baseline_y.has_value(),"Horizontal source Text exposes its first-line baseline metric");
        Fixture f(document);f.canvas.set_selection("moving-text");
        const QPointF body_center(layout.x+layout.width/2,layout.y+layout.height/2);
        const auto start=f.screen(body_center.x(),body_center.y()),end=start+QPoint(0,11);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"moving-text","","transform.ty"}),16,
            "Text source snaps its actual first-line baseline to a same-scope horizontal Text baseline");
        check(f.canvas.last_snap_feedback().contains("Text baseline → target-text")&&
              f.canvas.last_snap_feedback().contains("first-line baseline"),
            "Baseline Snap feedback names the target Text and first-line feature");
        f.release(end);near(f.value("moving-text",{},"transform.ty"),16,"Baseline Snap commits the shared object translation");f.no_error();
    }
    {
        auto document=text_baseline_snap_document();
        auto& target=*document.objects.at("target-text").text;
        target.content="H\nH";
        target.parameters.at("line_spacing").literal=80;
        auto& source=*document.objects.at("moving-text").text;
        source.content="H";
        source.parameters.at("line_spacing").literal=80;
        const auto target_layout=evaluate_text(target,text_parameters(target));
        const auto source_layout=evaluate_text(source,text_parameters(source));
        check(target_layout.line_baselines_y.size()==2,"Target exposes measured second-line baseline");
        near(target_layout.line_baselines_y[1]-source_layout.line_baselines_y[0],96,
            "Second-line baseline has a fixed 96-du target offset");
        Fixture f(document);f.canvas.set_selection("moving-text");
        const auto start=f.screen(source_layout.x+source_layout.width/2,source_layout.y+source_layout.height/2);
        const auto end=start+QPoint(0,97);
        f.press(start);f.move(end);
        near(evaluate(f.session.preview_document()).at({"moving-text","","transform.ty"}),96,
            "Raw 97-du drag snaps source first baseline to target second line");
        check(f.canvas.last_snap_feedback().contains("Text baseline → target-text")&&
              f.canvas.last_snap_feedback().contains("line 2 baseline"),
            "Second-line feedback names the actual Text line");
        f.release(end);near(f.value("moving-text",{},"transform.ty"),96,
            "Second-line Snap commits one object translation");
        check(f.session.can_undo(),"Second-line Snap creates one Undo step");
        f.session.undo(f.session.revision());near(f.value("moving-text",{},"transform.ty"),0,
            "Second-line Snap Undo restores original translation");f.no_error();
    }
    {
        auto document=text_baseline_snap_document();
        auto& target=*document.objects.at("target-text").text;
        target.content="H\nH";target.parameters.at("line_spacing").literal=80;
        auto& source=*document.objects.at("moving-text").text;
        source.content="H";source.parameters.at("line_spacing").literal=80;
        const auto layout=evaluate_text(source,text_parameters(source));
        Fixture f(document);f.canvas.set_selection("moving-text");
        const auto start=f.screen(layout.x+layout.width/2,layout.y+layout.height/2);
        const auto end=start+QPoint(0,103);
        f.press(start);f.move(end);
        check(!f.canvas.last_snap_feedback().contains("line 2 baseline"),
            "Seven-du raw gap does not choose the second-line baseline");
        QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(end);f.no_error();
    }
    {
        auto document=text_baseline_snap_document();
        auto& target=*document.objects.at("target-text").text;target.content="H";
        target.parameters.at("origin_y").literal=80;target.parameters.at("line_spacing").literal=80;
        auto& source=*document.objects.at("moving-text").text;
        source.content="H\nH";source.parameters.at("line_spacing").literal=80;
        const auto target_layout=evaluate_text(target,text_parameters(target));
        const auto layout=evaluate_text(source,text_parameters(source));
        near(target_layout.line_baselines_y[0]-layout.line_baselines_y[1],-80,
            "Negative fixture measures an exact 7-du miss from the accepted baseline alignment");
        document.compositions.front().artboards.front().y=20;
        Fixture f(document);f.canvas.set_selection("moving-text");
        const auto screen=[&](double x,double y) {
            return QPoint(qRound(f.canvas.width()/2.0+(x-320)*f.canvas.zoom()),
                          qRound(f.canvas.height()/2.0+(y-260)*f.canvas.zoom()));
        };
        const auto start=screen(layout.x+layout.width/2,layout.y+layout.height/2);
        const auto end=start+QPoint(0,-73);
        f.press(start);f.move(end);
        check(!f.canvas.last_snap_feedback().contains("Text baseline"),
            "A raw 7-du gap does not snap moving source line 2 to the Text baseline");
        QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(end);
        check(f.session.revision()==0&&!f.session.can_undo(),"Seven-du baseline miss leaves authored history unchanged");f.no_error();
    }
    {
        auto document=text_baseline_snap_document();
        auto& target=*document.objects.at("target-text").text;target.content="H";
        auto& source=*document.objects.at("moving-text").text;source.content="H";
        const auto layout=evaluate_text(source,text_parameters(source));
        Fixture f(document);f.canvas.set_selection("moving-text");
        const auto start=f.screen(layout.x+layout.width/2,layout.y+layout.height/2);
        const auto end=start+QPoint(0,-79);
        f.press(start);f.move(end);
        check(!f.canvas.last_snap_feedback().contains("line 2 baseline"),
            "A one-line moving Text has no line-2 baseline source");
        QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(end);f.no_error();
    }
    for(const bool point_source:{false,true}) {
        auto document=text_baseline_snap_document();
        auto& target=*document.objects.at("target-text").text;
        target.content="H";target.parameters.at("origin_y").literal=80;target.parameters.at("line_spacing").literal=80;
        const auto target_layout=evaluate_text(target,text_parameters(target));
        const double baseline=target_layout.line_baselines_y.front();
        auto& roots=document.compositions.front().roots;
        std::erase(roots,Id("moving-text"));document.objects.erase("moving-text");
        auto path=fixture_document().objects.at("path");
        path.id="moving-path";path.name="ordinary source";path.contours.front().id="moving-contour";
        path.stack.push_back(default_operation("moving-fill","nect.paint.fill"));
        auto& points=path.contours.front().points;
        points[0].id="moving-p1";points[1].id="moving-p2";
        points[0].y.literal=baseline+(point_source?-5:-14);
        points[1].y.literal=baseline+(point_source?20:22);
        document.objects.emplace(path.id,path);roots.insert(roots.begin(),path.id);
        document.compositions.front().artboards.front().y=20;
        Fixture f(document);f.canvas.set_selection("moving-path",point_source?"moving-p1":Id{});
        const auto screen=[&](double x,double y) {
            return QPoint(qRound(f.canvas.width()/2.0+(x-320)*f.canvas.zoom()),
                          qRound(f.canvas.height()/2.0+(y-260)*f.canvas.zoom()));
        };
        const auto start=point_source?screen(120,points[0].y.literal):screen(240,baseline+4);
        const auto end=start+QPoint(0,1);
        f.press(start);f.move(end);
        check(!f.canvas.last_snap_feedback().contains("Text baseline"),
            point_source?"A point anchor within 4 du cannot use a Text baseline target":"An ordinary bounds center within 5 du cannot use a Text baseline target");
        QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(end);
        check(f.session.revision()==0&&!f.session.can_undo(),"An ordinary source feature cannot author a Text baseline Snap");f.no_error();
    }
    {
        auto document=text_baseline_snap_document(true,false);
        const auto& source=*document.objects.at("moving-text").text;
        const auto layout=evaluate_text(source,text_parameters(source));
        check(!layout.first_line_baseline_y.has_value(),"Vertical source Text has no horizontal-baseline metric");
        Fixture f(document);f.canvas.set_selection("moving-text");
        const QPointF body_center(layout.x+layout.width/2,layout.y+layout.height/2);
        const auto start=f.screen(body_center.x(),body_center.y()),end=start+QPoint(0,11);
        f.press(start);f.move(end);
        check(!f.canvas.last_snap_feedback().contains("Text baseline"),
            "Vertical Text does not use a baseline candidate or substitute its glyph-box edge");
        QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(end);f.no_error();
    }
    {
        auto document=text_baseline_snap_document();
        auto& target=*document.objects.at("target-text").text;
        target.content="H";target.parameters.at("origin_y").literal=80;target.parameters.at("line_spacing").literal=80;
        auto& source=*document.objects.at("moving-text").text;
        source.content="H\nH";source.parameters.at("line_spacing").literal=80;
        const auto target_layout=evaluate_text(target,text_parameters(target));
        const auto source_layout=evaluate_text(source,text_parameters(source));
        const QPointF body_center(source_layout.x+source_layout.width/2,source_layout.y+source_layout.height/2);
        const double translate_y=target_layout.line_baselines_y.front()-body_center.x()-11;
        document.objects.at("moving-text").transform={{{0,{}},{1,{}},{-1,{}},{0,{}},{220,{}},{translate_y,{}}}};
        document.compositions.front().artboards.front().y=20;
        const QTransform rotated_source(0,1,-1,0,220,translate_y);
        Fixture f(document);f.canvas.set_selection("moving-text");
        const auto screen=[&](QPointF point) {
            return QPoint(qRound(f.canvas.width()/2.0+(point.x()-320)*f.canvas.zoom()),
                          qRound(f.canvas.height()/2.0+(point.y()-260)*f.canvas.zoom()));
        };
        const auto start=screen(rotated_source.map(body_center)),end=start+QPoint(0,11);
        f.press(start);f.move(end);
        check(!f.canvas.last_snap_feedback().contains("Text baseline"),
            "A rotated multiline moving Text offers no baseline even when its ordinary bounds center reaches the target baseline");
        QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(end);
        check(f.session.revision()==0&&!f.session.can_undo(),"Rotated-source baseline exclusion leaves authored history unchanged");f.no_error();
    }
    {
        auto document=text_baseline_snap_document(false,true);
        const auto& source=*document.objects.at("moving-text").text;
        const auto layout=evaluate_text(source,text_parameters(source));
        Fixture f(document);f.canvas.set_selection("moving-text");
        const QPointF body_center(layout.x+layout.width/2,layout.y+layout.height/2);
        const auto start=f.screen(body_center.x(),body_center.y()),end=start+QPoint(0,11);
        f.press(start);f.move(end);
        check(!f.canvas.last_snap_feedback().contains("Text baseline"),
            "A rotated target Text does not offer a baseline Snap candidate");
        QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(end);f.no_error();
    }
#endif
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
    document=snap_document();
    document.objects.at("target").source->parameters.at("center_x").literal=150;
    document.objects.at("target").source->parameters.at("center_y").literal=10;
    Object parent; parent.kind=Kind::group; parent.id="parent"; parent.children={"path","target"};
    parent.transform={{{0,{}},{2,{}},{-1,{}},{0,{}},{400,{}},{0,{}}}};
    document.objects.emplace(parent.id,parent); document.compositions.front().roots={"parent"};
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

void guide_drag_uses_stable_identity_and_one_session_undo() {
    auto document=fixture_document();
    document.compositions.front().artboards.push_back({"far-artboard","Far",2000,0,2000,1000});
    document.compositions.front().guides={{"z-guide","Later","x",100},{"a-guide","Earlier","x",100}};
    Fixture f(document);const auto initial_revision=f.session.revision();
    check(!f.canvas.guide_edit_mode(),"Guide dragging is off until the explicit edit mode is enabled");
    f.canvas.set_guide_edit_mode(true);
    const auto start=f.screen(100,20),finish=start+QPoint(20,0);
    f.press(start);check(f.session.gesture_active(),"Pressing a Guide in edit mode starts a Session gesture");
    f.move(finish);
    check(f.session.revision()==initial_revision&&f.session.document().compositions.front().guides[1].position==100,
        "Guide preview does not change committed state or revision");
    check(f.session.preview_document().compositions.front().guides[1].position==120,
        "Exact-distance Guide tie chooses the lexicographically earlier stable ID");
    f.release(finish);
    check(f.session.revision()==initial_revision+1&&f.session.document().compositions.front().guides[1].position==120&&
          f.session.document().compositions.front().guides[0].position==100,"Guide release commits only the chosen Guide in one revision");
    f.session.undo(f.session.revision());f.canvas.refresh();
    check(f.session.document().compositions.front().guides[1].position==100&&!f.session.can_undo(),"One Undo restores the Guide's starting coordinate");

    const auto start_zoom=f.canvas.zoom();f.press(start);f.canvas.fit_all_artboards();
    check(std::abs(f.canvas.zoom()-start_zoom)>1e-3&&f.session.gesture_active(),"View-fit does not replace an active Guide gesture");
    f.move(finish);
    check(f.session.preview_document().compositions.front().guides[1].position==120,
        "Guide drag maps pointer deltas through its frozen starting view transform");
    f.release(finish);f.session.undo(f.session.revision());f.canvas.refresh();f.canvas.fit_artboard();QApplication::processEvents();

    const auto revision=f.session.revision();f.press(start);f.move(start+QPoint(30,0));
    check(f.session.preview_document().compositions.front().guides[1].position==130,"Second Guide drag has a visible transient preview");
    QTest::keyClick(&f.canvas,Qt::Key_Escape);f.release(start+QPoint(30,0));
    check(f.session.revision()==revision&&!f.session.gesture_active()&&
          f.session.document().compositions.front().guides[1].position==100,"Escape cancels Guide preview without history");

    f.press(start);f.move(start+QPoint(25,0));f.canvas.set_show_guides(false);f.release(start+QPoint(25,0));
    check(f.session.revision()==revision&&!f.session.gesture_active()&&
          f.session.document().compositions.front().guides[1].position==100,"Hiding Guide overlay cancels an active Guide drag safely");
    f.canvas.set_show_guides(true);

    QString session_identity="session-one";f.canvas.set_session_identity_provider([&]{return session_identity;});
    f.press(start);session_identity="session-two";f.move(start+QPoint(15,0));f.release(start+QPoint(15,0));
    check(f.session.revision()==revision&&!f.session.gesture_active()&&f.session.document().compositions.front().guides[1].position==100&&
          f.last_error.startsWith("REVISION_CONFLICT"),"A stale Session identity cancels instead of rebasing a Guide drag");
}

void layout_overlays_are_view_only_and_not_exported() {
    auto document=empty_document("overlay-document","overlay-composition","overlay-artboard");
    auto& board=document.compositions.front().artboards.front();board.width=200;board.height=160;
    board.layout=ArtboardLayout{Margin{10,15,20,25},Grid{"overlay-grid",{20,25,150,110},2,2,10,10}};
    document.compositions.front().guides={{"overlay-guide-x","Vertical","x",30},{"overlay-guide-y","Horizontal","y",40}};
    Session session(document);Canvas canvas(session);canvas.resize(300,260);canvas.show();QApplication::processEvents();canvas.fit_artboard();QApplication::processEvents();
    check(canvas.show_guides()&&canvas.show_grid()&&canvas.show_margin(),"Authored layout overlays default visible per Canvas window");
    const auto committed=session.document();const auto revision=session.revision();
    const auto image=canvas.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    const double image_scale=image.width()>canvas.width()?static_cast<double>(image.width())/canvas.width():1.0;
    const int guide_x=qRound((canvas.width()/2.0+(30-100)*canvas.zoom())*image_scale);int guide_pixels=0;
    for(int y=qRound(55*image_scale);y<qRound(215*image_scale);++y)
      for(int x=guide_x-qMax(2,qRound(2*image_scale));x<=guide_x+qMax(2,qRound(2*image_scale));++x) {
        const auto pixel=image.pixelColor(x,y);if(pixel.blue()>pixel.red()+25&&pixel.blue()>pixel.green()+15)++guide_pixels;
    }
    check(guide_pixels>20,"Canvas paints Session Guides over the composition viewport");
    const auto exported=Canvas::render_artboard(session.document(),"overlay-composition","overlay-artboard",1,true);
    check(exported.pixelColor(30,70)==QColor(Qt::white),"Guide overlay is absent from the Artboard export projection");
    canvas.set_show_guides(false);check(!canvas.show_guides()&&canvas.show_grid()&&canvas.show_margin(),"Guide, Grid and Margin visibility toggle independently");
    canvas.set_show_grid(false);check(!canvas.show_grid()&&canvas.show_margin(),"Grid visibility does not change Margin visibility");
    canvas.set_show_margin(false);check(!canvas.show_margin(),"Margin visibility has its own per-window toggle");
    check(session.document()==committed&&session.revision()==revision&&!session.can_undo(),"Overlay visibility changes no authored state or history");
    Canvas second(session);check(second.show_guides()&&second.show_grid()&&second.show_margin(),"A second Canvas has independent default visibility state");
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
        snap_guide_grid_priority_visibility_and_controls();
        snap_two_sided_equal_gap_and_point_world_correction();
        snap_repeated_gap_fixed_oracles_and_eligibility();
        snap_scope_and_singular_point_rejections();
        snap_text_baseline_and_unsupported_axis_omission();
        snap_cancellation_and_artboard();
        snap_multi_and_effective_followers();
        snap_visibility_and_parent_coordinates();
        guide_drag_uses_stable_identity_and_one_session_undo();
        layout_overlays_are_view_only_and_not_exported();
        std::cout << "Canvas widget contract: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Canvas widget contract failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
