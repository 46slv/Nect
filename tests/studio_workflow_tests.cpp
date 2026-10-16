#include "canvas.hpp"
#include "nect/io.hpp"
#include "theme.hpp"
#include "workspace.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QLineEdit>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
namespace {
int checks = 0;
void check(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
}
void drag(QWidget *widget, QPoint from, QPoint to) {
    QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, from);
    QMouseEvent move(QEvent::MouseMove, QPointF(to), QPointF(widget->mapToGlobal(to)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &move);
    QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, to);
    QApplication::processEvents();
}
void edit(QLineEdit *input, QString text) {
    input->setFocus();
    input->selectAll();
    QTest::keyClicks(input, text);
    QTest::keyClick(input, Qt::Key_Return);
    QApplication::processEvents();
}
void layout_regressions(studio::Workspace &w) {
    auto settle = [] { QApplication::processEvents(); QTest::qWait(10); };
    auto *canvas = w.canvas;
    const auto authored = w.host.session.document();
    const auto revision = w.host.session.revision();
    const auto history = w.host.session.history().states.size();
    const auto dirty = w.host.dirty();
    auto *nodes = w.findChild<QToolButton *>("toggle-nodes");
    auto *fit = w.findChild<QToolButton *>("view-fit");
    auto *inspector_toggle = w.findChild<QToolButton *>("toggle-inspector");
    auto *objects_toggle = w.findChild<QToolButton *>("toggle-objects");
    auto *hint = w.findChild<QLabel *>("canvas-status");
    QSplitter *horizontal = nullptr;
    for (auto *splitter : w.findChildren<QSplitter *>())
        if (splitter->orientation() == Qt::Horizontal) horizontal = splitter;
    check(horizontal && hint && nodes && fit, "Layout controls are reachable");
    auto *inspector = horizontal->widget(2);
    const auto &composition = *std::find_if(authored.compositions.begin(), authored.compositions.end(),
        [&](const auto &c) { return c.id == canvas->active_composition(); });
    const auto board = evaluate_artboard(composition, canvas->active_artboard());
    auto fitted = [&] {
        const auto zoom = std::clamp(std::min(std::max(1, canvas->width()-100) / board.width,
                                            std::max(1, canvas->height()-114) / board.height), 0.02, 64.0);
        const auto center = canvas->document_position(QPointF(canvas->width()/2.0, canvas->height()/2.0+7));
        return std::abs(canvas->zoom()-zoom) < 1e-9 &&
               QLineF(center, QPointF(board.x+board.width/2, board.y+board.height/2)).length() < 1e-6;
    };
    auto separate_footer = [&] {
        const QRect canvas_rect(canvas->mapTo(&w, QPoint()), canvas->size());
        const QRect hint_rect(hint->mapTo(&w, QPoint()), hint->size());
        return w.statusBar()->height() == 22 && hint->isVisible() &&
               !canvas_rect.intersects(hint_rect) && hint->text().contains("Empty-drag");
    };
    auto title_clear = [&] {
        const auto top = (board.y - canvas->document_position({0,0}).y()) * canvas->zoom();
        return top - 23 >= 40; // The breadcrumb occupies y=12..40.
    };
    check(inspector->width() == 280, "Wide Inspector stays at 280 DIP");
    QTest::mouseClick(fit, Qt::LeftButton);
    settle();
    check(fitted() && separate_footer(), "Fitted canvas and fixed external footer");
    for (int i = 0; i < 3; ++i) {
        QTest::mouseClick(nodes, Qt::LeftButton);
        settle();
        check(fitted() && title_clear() && separate_footer(), "Nodes toggle refits below breadcrumb and outside footer");
    }
    // A real wheel input must end Fit tracking, including subsequent Nodes close/open.
    const QPointF cursor(canvas->width()/2.0, canvas->height()/2.0);
    QWheelEvent wheel(cursor, canvas->mapToGlobal(cursor.toPoint()), QPoint(), QPoint(0,120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(canvas, &wheel);
    settle();
    const auto zoom = canvas->zoom();
    const auto origin = canvas->document_position({0,0});
    check(!fitted(), "Wheel zoom leaves Fit");
    for (int i = 0; i < 2; ++i) {
        QTest::mouseClick(nodes, Qt::LeftButton);
        settle();
        check(canvas->zoom() == zoom && canvas->document_position({0,0}) == origin,
              "Nodes preserves manual zoom and pan mapping");
    }
    QTest::mouseClick(fit, Qt::LeftButton);
    settle();
    QTest::mousePress(canvas, Qt::MiddleButton, Qt::NoModifier, {100,100});
    QMouseEvent move(QEvent::MouseMove, QPointF(125,115),
                     QPointF(canvas->mapToGlobal(QPoint(125,115))), Qt::NoButton, Qt::MiddleButton,
                     Qt::NoModifier);
    QApplication::sendEvent(canvas, &move);
    QTest::mouseRelease(canvas, Qt::MiddleButton, Qt::NoModifier, {125,115});
    settle();
    const auto panned = canvas->document_position({0,0});
    check(!fitted(), "Middle pan leaves Fit");
    QTest::mouseClick(nodes, Qt::LeftButton);
    settle();
    check(canvas->document_position({0,0}) == panned, "Nodes preserves manual pan");
    QTest::keyClick(canvas, Qt::Key_F);
    settle();
    QTest::mouseClick(nodes, Qt::LeftButton);
    settle();
    check(fitted(), "Keyboard Fit restores viewport tracking");
    w.resize(683,600);
    settle();
    check(inspector->width() == 240 && fitted(), "683 logical width gives 240 DIP Inspector and refits");
    auto *x = w.findChild<QLineEdit *>("value-x");
    auto *y = w.findChild<QLineEdit *>("value-y");
    auto *width = w.findChild<QLineEdit *>("value-w");
    auto *height = w.findChild<QLineEdit *>("value-h");
    const auto xp = x->mapTo(inspector, QPoint());
    const auto yp = y->mapTo(inspector, QPoint());
    const auto wp = width->mapTo(inspector, QPoint());
    const auto hp = height->mapTo(inspector, QPoint());
    check(xp.y() == yp.y() && wp.y() == hp.y() && xp.x() == wp.x() && yp.x() == hp.x() && xp.x() < yp.x(),
          "Narrow numeric fields retain two aligned columns");
    check(x->height() == 28 && y->height() == 28 && width->height() == 28 && height->height() == 28 &&
          x->font().pixelSize() == 13 && width->font().pixelSize() == 13,
          "Numeric fields keep 28 DIP height and 13 DIP type");
    check(separate_footer(), "Narrow footer remains outside canvas");
    const auto canvas_width = canvas->width();
    QTest::mouseClick(inspector_toggle, Qt::LeftButton);
    settle();
    check(!inspector->isVisible() && canvas->width() > canvas_width,
          "Existing Inspector toggle returns width to canvas");
    QTest::mouseClick(inspector_toggle, Qt::LeftButton);
    settle();
    check(inspector->isVisible() && inspector->width() == 240, "Inspector reopens at narrow width");
    QTest::mouseClick(objects_toggle, Qt::LeftButton);
    settle();
    check(horizontal->widget(0)->isVisible() && !inspector->isVisible(), "Narrow sidebars stay mutually exclusive");
    QTest::mouseClick(inspector_toggle, Qt::LeftButton);
    settle();
    check(!horizontal->widget(0)->isVisible() && inspector->isVisible(), "Inspector toggle restores canvas-side layout");
    w.resize(799,600);
    settle();
    check(inspector->width() == 240, "799 logical boundary remains compact");
    w.resize(800,600);
    settle();
    check(inspector->width() == 280, "800 logical boundary restores 280 DIP");
    w.resize(1440,900);
    settle();
    check(inspector->width() == 280, "Wide resize retains 280 DIP");
    QTest::mouseClick(inspector_toggle, Qt::LeftButton);
    w.resize(683,600);
    settle();
    check(!inspector->isVisible(), "Resizing keeps a collapsed Inspector closed");
    QTest::mouseClick(inspector_toggle, Qt::LeftButton);
    settle();
    check(inspector->width() == 240, "Inspector collapsed across breakpoint reopens compact");
    check(w.host.session.document() == authored && w.host.session.revision() == revision &&
          w.host.session.history().states.size() == history && w.host.dirty() == dirty,
          "Layout, Fit and view inputs preserve authored document, revision, History and save state");
}
} // namespace
int main(int argc, char **argv) {
    if (qgetenv("QT_QPA_PLATFORM") == "offscreen" && qgetenv("QT_QPA_FONTDIR").isEmpty() &&
        !qgetenv("SystemRoot").isEmpty())
        qputenv("QT_QPA_FONTDIR", qgetenv("SystemRoot") + "/Fonts");
    QApplication app(argc, argv);
    studio::apply_theme(app);
    QTemporaryDir temporary;
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == "--layout-only") {
        try {
            if (argc != 4) throw std::runtime_error("Usage: --layout-only receipt.json fixture.nect");
            studio::Workspace w(temporary.path() + "/layout-recovery");
            w.open_document(QString::fromLocal8Bit(argv[3]));
            w.show();
            QApplication::processEvents();
            w.select_object(w.host.session.document().objects.begin()->first);
            layout_regressions(w);
            QFile receipt(QString::fromLocal8Bit(argv[2]));
            if (!receipt.open(QIODevice::WriteOnly)) throw std::runtime_error("Cannot write layout receipt");
            receipt.write(QJsonDocument(QJsonObject{{"checks", checks}, {"result", "PASS"},
                {"scope", "Footer, Fit/manual view on Nodes toggle, responsive Inspector; Qt events"},
                {"authored_history_save_unchanged", true}}).toJson());
            std::cout << checks << " layout checks PASS\n";
            return 0;
        } catch (const std::exception &e) {
            std::cerr << "FAIL after " << checks << ": " << e.what() << "\n";
            return 1;
        }
    }
    const auto evidence =
        argc > 1 ? QString::fromLocal8Bit(argv[1]) : temporary.path() + "/evidence";
    QDir().mkpath(evidence);
    try {
        studio::Workspace w(temporary.path() + "/recovery");
        w.show();
        QApplication::processEvents();
        w.canvas->fit_artboard();
        QApplication::processEvents();
        check(w.menuBar()->actionGeometry(w.menuBar()->actions().front()).height() <=
                  w.menuBar()->height(),
              "Menu items fit the 24 DIP menu bar");
        check(w.host.session.document().objects.empty(), "Starts with an empty authored document");
        check(!w.nodes_visible(), "Nodes starts closed");
        check(w.findChild<QWidget *>("object-name")->isHidden() == false,
              "Inspector controls exist");
        check(w.grab().save(evidence + "/blank.png"), "Blank rendered capture");
        QTest::mouseClick(w.findChild<QToolButton *>("tool-rectangle"), Qt::LeftButton);
        drag(w.canvas, {200, 220}, {440, 380});
        check(w.host.session.document().objects.size() == 1,
              "Canvas drag creates exactly one object");
        const auto id = w.canvas->selected_object;
        check(!id.empty(), "Created rectangle selected");
        check(w.host.session.history().states.size() == 2, "One rectangle drag is one Undo");
        const auto created = w.host.session.document();
        auto values = evaluate(created);
        const auto tx = values.at({id, {}, "transform.tx"});
        // Hit the actual evaluated rectangle center through the viewport mapping.
        const auto center = QPoint(320, 300);
        drag(w.canvas, center, center + QPoint(35, 20));
        values = evaluate(w.host.session.document());
        check(values.at({id, {}, "transform.tx"}) != tx,
              "Direct canvas drag moves shared authored transform");
        edit(w.findChild<QLineEdit *>("value-w"), "288");
        edit(w.findChild<QLineEdit *>("value-h"), "176");
        values = evaluate(w.host.session.document());
        check(values.at({id, {}, "generator.width"}) == 288 &&
                  values.at({id, {}, "generator.height"}) == 176,
              "Inspector size updates shared source");
        edit(w.findChild<QLineEdit *>("fill-value"), "#7098B6");
        check(w.findChild<QLineEdit *>("fill-value")->text() == "#7098B6", "Fill edits round-trip");
        const auto colored = w.host.session.document();
        QTest::keyClick(&w, Qt::Key_Z, Qt::ControlModifier);
        QApplication::processEvents();
        check(w.host.session.document() != colored,
              "Toolbar action shortcut Undo changes document");
        QTest::keySequence(&w, QKeySequence::Redo);
        QApplication::processEvents();
        check(w.host.session.document() == colored, "Redo restores exact authored document");
        auto *input = w.findChild<QLineEdit *>("value-w");
        input->setFocus();
        input->selectAll();
        QTest::keyClicks(input, "999");
        QTest::keyClick(input, Qt::Key_Escape);
        check(w.host.session.document() == colored, "Escape cancels numeric draft");
        edit(input, "-5");
        check(w.host.session.document() == colored, "Invalid size refuses atomically");
        const auto path = evidence + "/rectangle.nect";
        w.save_document(path);
        check(!w.host.dirty(), "Explicit native save marks saved");
        w.set_nodes_visible(true);
        QApplication::processEvents();
        check(w.grab().save(evidence + "/pipeline.png"), "Pipeline rendered capture");
        w.set_nodes_visible(false);
        QApplication::processEvents();
        check(w.grab().save(evidence + "/editing.png"), "Editing rendered capture");
        {
            studio::Workspace reopened(temporary.path() + "/second-recovery");
            reopened.open_document(path);
            check(reopened.host.session.document() == colored,
                  "Independent Host save/reopen retains exact native state");
            reopened.select_object(id);
            edit(reopened.findChild<QLineEdit *>("value-w"), "320");
            check(evaluate(reopened.host.session.document()).at({id, {}, "generator.width"}) == 320,
                  "Reopened source remains editable");
            reopened.save_document(evidence + "/reopened.nect");
        }
        QTest::mouseClick(w.findChild<QToolButton *>("tool-rectangle"), Qt::LeftButton);
        QTest::mousePress(w.canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::keyClick(w.canvas, Qt::Key_Escape);
        check(w.host.session.document() == colored && !w.host.session.gesture_active(),
              "Escape cancels rectangle gesture");
        w.select_object({});
        check(w.findChild<QLineEdit *>("value-w")->isVisible() == false,
              "Empty selection hides irrelevant property values");
        w.save_document(path);
        QFile receipt(evidence + "/workflow.json");
        check(receipt.open(QIODevice::WriteOnly), "Evidence receipt writable");
        receipt.write(
            QJsonDocument(
                QJsonObject{{"checks", checks},
                            {"result", "PASS"},
                            {"capture_type", "Qt widget rendering; not native desktop screenshot"},
                            {"native_roundtrip", true},
                            {"single_drag_single_undo", true}})
                .toJson());
        std::cout << checks << " checks PASS\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << ": " << e.what() << "\n";
        return 1;
    }
}
