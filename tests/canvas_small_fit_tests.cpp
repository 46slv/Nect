#include "canvas.hpp"
#include "viewport_layout.hpp"
#include "nect/io.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QPushButton>
#include <QScrollArea>
#include <QTest>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;

namespace {
int checks = 0;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}
void near(double actual, double expected, const char* message, double tolerance = 1e-7) {
    check(std::abs(actual - expected) < tolerance, message);
}
void settle() {
    QApplication::processEvents();
    QApplication::processEvents();
}

Document fixture() {
    auto document = empty_document("small-fit-document", "composition", "artboard");
    auto& artboard = document.compositions.front().artboards.front();
    // Nonzero origin also checks that Fit uses the Artboard's center.
    artboard.x = 173;
    artboard.y = -90;
    artboard.width = 640;
    artboard.height = 480;
    ColorValue background;
    background.rgba = {1, 0, 1, 1};
    artboard.background = background;
    Object path;
    path.id = "path";
    Point point;
    point.id = "point";
    point.x.literal = 250;
    point.y.literal = 100;
    path.contours = {{"contour", false, {point}}};
    document.objects.emplace(path.id, path);
    document.compositions.front().roots = {path.id};
    return document;
}

struct Snapshot {
    std::string native;
    std::uint64_t revision;
    HistoryInfo history;
    bool undo, redo;
    explicit Snapshot(const Session& session)
        : native(encode(session.document())), revision(session.revision()), history(session.history()),
          undo(session.can_undo()), redo(session.can_redo()) {}
    void unchanged(const Session& session) const {
        check(encode(session.document()) == native && encode(session.preview_document()) == native,
              "Canvas sizing/Fit/pan/zoom preserve the native and preview document");
        check(session.revision() == revision && session.history() == history &&
              session.can_undo() == undo && session.can_redo() == redo && !session.gesture_active(),
              "Canvas view operations preserve revision, complete History and Undo/Redo");
    }
};

// Observe the real paint transform without exposing Canvas internals. The empty
// magenta Artboard has no paint operations that could hide its interior bounds.
QRect painted_artboard(Canvas& canvas) {
    QImage image(canvas.size(), QImage::Format_RGB32);
    image.fill(Qt::black);
    canvas.render(&image);
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) if (row[x] == qRgb(255, 0, 255)) {
            left = std::min(left, x); right = std::max(right, x);
            top = std::min(top, y); bottom = std::max(bottom, y);
        }
    }
    check(right >= left && bottom >= top, "Fitted Artboard is actually painted");
    return {QPoint(left, top), QPoint(right, bottom)};
}

void verify_fit(Canvas& canvas, const Session& session) {
    const auto artboard = evaluate_artboard(session.document().compositions.front(), canvas.active_artboard());
    const auto expected_zoom = std::clamp(std::min(std::max(1, canvas.width() - 100) / artboard.width,
        std::max(1, canvas.height() - 100) / artboard.height), 0.02, 64.0);
    near(canvas.zoom(), expected_zoom, "Fit retains the existing padding and zoom limits");
    const QRectF mapped(canvas.width() / 2.0 - artboard.width * canvas.zoom() / 2.0,
        canvas.height() / 2.0 - artboard.height * canvas.zoom() / 2.0,
        artboard.width * canvas.zoom(), artboard.height * canvas.zoom());
    check(QRectF(canvas.rect()).contains(mapped), "Mapped Artboard fits inside the actual Canvas");
    const auto painted = painted_artboard(canvas);
    // The active outline and antialiasing consume up to two edge pixels.
    near(painted.left(), mapped.left(), "Painted left edge matches the fitted view", 3.0);
    near(painted.top(), mapped.top(), "Painted top edge matches the fitted view", 3.0);
    near(painted.right() + 1, mapped.right(), "Painted right edge matches the fitted view", 3.0);
    near(painted.bottom() + 1, mapped.bottom(), "Painted bottom edge matches the fitted view", 3.0);
    check(mapped.left() >= 50 - 1e-7 && mapped.top() >= 50 - 1e-7 &&
          mapped.right() <= canvas.width() - 50 + 1e-7 &&
          mapped.bottom() <= canvas.height() - 50 + 1e-7,
          "Fitted Artboard retains at least 50px visible padding on every side");
}

void constrained_viewport(Session& session) {
    const Snapshot before(session);
    QWidget parent;
    auto* parent_layout = new QVBoxLayout(&parent);
    parent_layout->setContentsMargins(0, 0, 0, 0);
    auto* canvas = new Canvas(session);
    auto* viewport = new ViewportLayout(canvas, &parent);
    parent_layout->addWidget(viewport);
    auto* fit = new QPushButton("Fit", viewport->utility_contents());
    viewport->utility_layout()->addWidget(fit);
    QObject::connect(fit, &QPushButton::clicked, canvas, &Canvas::fit_artboard);
    // Exercise the same horizontal Utility overflow used by narrow center regions.
    auto* wide_utility = new QWidget(viewport->utility_contents());
    wide_utility->setMinimumWidth(700);
    viewport->utility_layout()->addWidget(wide_utility);
    canvas->set_show_grid(false);
    canvas->set_show_guides(false);
    canvas->set_show_margin(false);
    parent.setFixedSize(900, 680);
    parent.show();
    settle();
    int document_notifications = 0;
    canvas->document_changed = [&] { ++document_notifications; };
    for (const auto size : {QSize(900, 680), QSize(498, 540), QSize(498, 430), QSize(900, 680)}) {
        parent.setFixedSize(size);
        settle();
        check(parent.size() == size && viewport->size() == size, "Parent keeps the requested available viewport");
        check(viewport->rect().contains(canvas->QWidget::geometry()) &&
              parent.rect().contains(QRect(canvas->mapTo(&parent, QPoint{}), canvas->size())),
              "Canvas geometry stays completely within its allocated visible region");
        check(canvas->width() == size.width(), "Canvas genuinely shrinks to the available center width");
        if (size.height() == 430) check(canvas->height() < 480, "Canvas also shrinks below the old height minimum");
        for (const auto placement : {UtilityPlacement::top, UtilityPlacement::bottom}) {
            viewport->set_utility_placement(placement);
            settle();
            QTest::mouseClick(fit, Qt::LeftButton);
            settle();
            verify_fit(*canvas, session);
            before.unchanged(session);
        }
    }
    check(document_notifications == 0 && canvas->projection_succeeded(),
          "Resizing and fitting never emit authored edits or projection errors");
}

void normal_view_and_inputs(Session& session) {
    const Snapshot before(session);
    Canvas canvas(session);
    canvas.set_show_grid(false);
    canvas.set_show_guides(false);
    canvas.set_show_margin(false);
    canvas.resize(740, 580);
    canvas.show();
    settle();
    canvas.fit_artboard();
    near(canvas.zoom(), 1.0, "Normal 740x580 Canvas retains unit-scale Fit");
    verify_fit(canvas, session);
    const auto original = painted_artboard(canvas);
    const QPoint start(320, 240), delta(20, 15);
    QTest::mousePress(&canvas, Qt::MiddleButton, Qt::NoModifier, start);
    QTest::mouseMove(&canvas, start + delta);
    QTest::mouseRelease(&canvas, Qt::MiddleButton, Qt::NoModifier, start + delta);
    settle();
    check(painted_artboard(canvas) == original.translated(delta), "Middle-drag still pans the view");
    canvas.set_zoom(0.5);
    near(canvas.zoom(), 0.5, "Explicit view zoom remains available");
    const QPointF cursor(320, 240);
    QWheelEvent wheel(cursor, canvas.mapToGlobal(cursor.toPoint()), {}, {0, 120},
        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&canvas, &wheel);
    near(canvas.zoom(), 0.5 * std::pow(1.0015, 120), "Wheel keeps its existing zoom response");
    QTest::keyClick(&canvas, Qt::Key_F);
    settle();
    verify_fit(canvas, session);
    before.unchanged(session);
}
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    try {
        Session session(fixture());
        const Ref x{"path", "point", "x"};
        session.apply({Set{x, 260.0}}, session.revision());
        session.apply({Set{x, 270.0}}, session.revision());
        session.undo(session.revision());
        check(session.can_undo() && session.can_redo(), "Fixture retains both History directions");
        normal_view_and_inputs(session);
        constrained_viewport(session);
        std::cout << "PASS " << checks << " Canvas small-Fit geometry/view checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
