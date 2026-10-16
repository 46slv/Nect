#include "canvas.hpp"
#include "nect/io.hpp"
#include "theme.hpp"
#include "workspace.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenuBar>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
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
} // namespace
int main(int argc, char **argv) {
    if (qgetenv("QT_QPA_PLATFORM") == "offscreen" && qgetenv("QT_QPA_FONTDIR").isEmpty() &&
        !qgetenv("SystemRoot").isEmpty())
        qputenv("QT_QPA_FONTDIR", qgetenv("SystemRoot") + "/Fonts");
    QApplication app(argc, argv);
    studio::apply_theme(app);
    QTemporaryDir temporary;
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
