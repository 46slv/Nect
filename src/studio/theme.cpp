#include "theme.hpp"
#include <QApplication>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
namespace nect::studio {
void apply_theme(QApplication &app) {
    app.setStyle("Fusion");
    QFont font("Segoe UI");
    font.setPixelSize(13);
    app.setFont(font);
    QPalette p;
    p.setColor(QPalette::Window, QColor("#202124"));
    p.setColor(QPalette::WindowText, QColor("#e4e7eb"));
    p.setColor(QPalette::Base, QColor("#34373c"));
    p.setColor(QPalette::Text, QColor("#e4e7eb"));
    p.setColor(QPalette::Button, QColor("#34373c"));
    p.setColor(QPalette::ButtonText, QColor("#e4e7eb"));
    p.setColor(QPalette::Highlight, QColor("#304568"));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor("#767a82"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#767a82"));
    app.setPalette(p);
    app.setStyleSheet(QStringLiteral(R"(
QMainWindow,QWidget { color:#e4e7eb; }
QWidget#workspace { background:#202124; }
QWidget#panel { background:#292b2f; }
QWidget#strip { background:#25272b; border-bottom:1px solid #404349; }
QLabel#section { color:#b0b6bf; font-weight:500; padding:16px 12px 8px; }
QLabel#muted { color:#b0b6bf; font-size:12px; }
QLabel#brand { color:#f0f1f2; font-weight:600; font-size:15px; padding:0 14px 0 10px; }
QLabel#document { color:#c0c4ca; }
QToolButton { background:transparent; border:1px solid transparent; border-radius:3px; padding:4px 7px; min-height:22px; }
QToolButton:hover:enabled { background:#3e4249; }
QToolButton:pressed:enabled { background:#484d55; }
QToolButton:checked { background:#304568; border-color:#78a8ff; }
QToolButton:focus { border:2px solid #78a8ff; }
QToolButton:disabled { color:#737982; background:transparent; }
QToolButton#tab { border-radius:0; padding:5px 14px; color:#a8adb6; }
QToolButton#tab:checked { background:#303237; border:0; border-bottom:2px solid #9bafb9; color:#eef0f2; }
QPushButton { background:#41444a; border:1px solid #4d5057; border-radius:3px; padding:5px 9px; }
QPushButton:hover { background:#50545b; }
QPushButton:focus { border-color:#90aebc; }
QLineEdit { background:#34373c; border:1px solid transparent; border-radius:4px; padding:3px 7px; min-height:20px; selection-background-color:#304568; }
QLineEdit:hover { border-color:#51565e; }
QLineEdit:focus { background:#34373c; border:2px solid #78a8ff; padding:2px 6px; }
QLineEdit:disabled { color:#70757e; background:#2a2c30; border-color:#2a2c30; }
QTreeWidget,QListWidget { background:#292b2f; border:0; outline:0; }
QTreeWidget::item { height:28px; padding:0 6px; }
QTreeWidget::item:hover { background:#3a3d43; }
QTreeWidget::item:selected { background:#304568; color:#e4e7eb; }
QTreeWidget::branch { background:transparent; }
QListWidget::item { padding:6px 12px; }
QListWidget::item:selected { background:#3f4c55; }
QSplitter::handle { background:#202124; }
QSplitter::handle:hover { background:#404349; }
QScrollArea { border:0; background:transparent; }
QScrollBar:vertical { width:8px; background:#2a2c30; }
QScrollBar::handle:vertical { background:#555960; border-radius:3px; min-height:30px; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
QMenuBar { background:#25272b; border-bottom:1px solid #404349; padding:0 6px; }
QMenuBar::item { padding:2px 10px; background:transparent; }
QMenuBar::item:selected,QMenu::item:selected { background:#48545f; }
QMenu { background:#33363b; border:1px solid #555a62; padding:4px; }
QMenu::item { padding:6px 25px 6px 16px; }
QMenu::separator { height:1px; background:#4b4f56; margin:4px; }
QStatusBar { background:#25272b; border-top:1px solid #404349; font-size:12px; }
QStatusBar::item { border:0; }
QLabel#canvas-status { color:#b0b6bf; font-size:12px; }
QToolTip { background:#202226; color:#e2e6e9; border:1px solid #5b626b; padding:5px; }
)"));
}
QIcon icon(const QString &name) {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#c6ccd3"), 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (name == "rectangle")
        p.drawRect(QRectF(3.5, 4.5, 13, 11));
    else if (name == "select") {
        QPolygonF poly{{4, 2}, {4, 16}, {8, 12}, {11, 18}, {13, 17}, {10, 11}, {16, 11}};
        p.drawPolygon(poly);
    } else if (name == "hand") {
        QPainterPath a;
        a.moveTo(6, 10);
        a.lineTo(6, 4);
        a.cubicTo(6, 2, 9, 2, 9, 4);
        a.lineTo(9, 9);
        a.lineTo(9, 3);
        a.cubicTo(9, 1, 12, 1, 12, 4);
        a.lineTo(12, 9);
        a.lineTo(12, 5);
        a.cubicTo(12, 3, 15, 3, 15, 6);
        a.lineTo(15, 12);
        a.cubicTo(15, 19, 8, 20, 5, 14);
        a.lineTo(2, 9);
        a.cubicTo(2, 7, 4, 7, 6, 10);
        p.drawPath(a);
    } else if (name == "fit") {
        p.drawRect(QRectF(6, 6, 8, 8));
        for (int i = 0; i < 4; ++i) {
            p.save();
            p.translate(10, 10);
            p.rotate(i * 90);
            p.drawLine(QPointF(-7, -3), QPointF(-7, -7));
            p.drawLine(QPointF(-7, -7), QPointF(-3, -7));
            p.restore();
        }
    } else if (name == "undo" || name == "redo") {
        if (name == "redo") {
            p.translate(20, 0);
            p.scale(-1, 1);
        }
        QPainterPath a;
        a.moveTo(4, 7);
        a.cubicTo(17, 3, 19, 16, 9, 16);
        p.drawPath(a);
        p.drawLine(4, 7, 8, 3);
        p.drawLine(4, 7, 9, 10);
    } else if (name == "save") {
        p.drawRect(QRectF(4, 3, 12, 14));
        p.drawRect(QRectF(7, 3, 6, 5));
        p.drawRect(QRectF(7, 12, 6, 5));
    } else if (name == "node") {
        p.drawRect(QRectF(2, 3, 6, 6));
        p.drawRect(QRectF(12, 11, 6, 6));
        p.drawLine(8, 6, 12, 14);
    }
    return QIcon(px);
}
} // namespace nect::studio
