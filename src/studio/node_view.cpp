#include "node_view.hpp"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
namespace nect::studio {
NodeView::NodeView(Session &session, QWidget *parent) : QWidget(parent), session_(session) {
    setObjectName("node-view");
    setMinimumHeight(135);
    setMinimumWidth(240);
}
void NodeView::set_object(Id object) {
    object_ = std::move(object);
    const auto found = session_.document().objects.find(object_);
    const auto count = found == session_.document().objects.end()
                           ? 0
                           : 3 + found->second.stack.size() + (found->second.point_edit ? 1 : 0);
    setMinimumWidth(count ? static_cast<int>(count * 168 - 36 + 32) : 240);
    update();
}
void NodeView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#25272b"));
    p.setPen(QColor("#34373c"));
    for (int x = 14; x < width(); x += 20)
        for (int y = 14; y < height(); y += 20)
            p.drawPoint(x, y);
    nodes_.clear();
    const auto found = session_.document().objects.find(object_);
    if (found == session_.document().objects.end()) {
        p.setPen(QColor("#9a9fa9"));
        p.drawText(rect(), Qt::AlignCenter, "Select an object to inspect its pipeline");
        return;
    }
    const auto &o = found->second;
    auto add = [&](QString title, QString detail, QString role, QColor color) {
        nodes_.push_back({{}, title, detail, role, color});
    };
    const auto source = o.source ? QString::fromStdString(o.source->type).section('.', -1)
                        : o.text ? QString("Text")
                        : o.kind == Kind::group ? QString("Group")
                                                : QString("Path");
    add(source.left(1).toUpper() + source.mid(1),
        o.source ? "Retained source" : "Authored geometry", "shape", QColor("#789b8b"));
    if (o.point_edit)
        add("Point Edit", o.point_edit->enabled ? "Enabled" : "Bypassed", "shape",
            QColor("#789b8b"));
    for (const auto &entry : o.stack) {
        if (const auto *op = &entry; !entry.macro) {
            const auto title = QString::fromStdString(op->type).section('.', -1);
            const auto detail = !op->enabled ? QString("Bypassed")
                                : op->type == "nect.paint.stroke"
                                    ? QString("Width %1").arg(op->parameters.at("width").literal)
                                    : QString("Enabled");
            add(title.left(1).toUpper() + title.mid(1), detail,
                QString::fromStdString(op->type) == "nect.paint.fill" ? "fill" : "shape",
                QColor("#ac9476"));
        } else
            add("Processing entry", "Preserved native entry", "shape", QColor("#ac9476"));
    }
    add("Transform", "Local to composition", "transform", QColor("#8295aa"));
    add("Output", "Evaluated scene", "output", QColor("#8b8f98"));
    const qreal nodeWidth = 132, gap = 36, total = nodes_.size() * (nodeWidth + gap) - gap;
    const qreal scale = std::min(1.0, (width() - 32) / std::max(total, 1.0));
    p.translate(16, std::max(12.0, (height() - 92 * scale) / 2));
    p.scale(scale, scale);
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        auto &n = nodes_[i];
        n.rect = QRectF(i * (nodeWidth + gap), 0, nodeWidth, 92);
    }
    p.setPen(QPen(QColor("#78838b"), 1.5));
    for (std::size_t i = 1; i < nodes_.size(); ++i) {
        auto a = nodes_[i - 1].rect.topRight() + QPointF(0, 59),
             b = nodes_[i].rect.topLeft() + QPointF(0, 59);
        QPainterPath path(a);
        path.cubicTo(a + QPointF(22, 0), b - QPointF(22, 0), b);
        p.drawPath(path);
    }
    for (auto &n : nodes_) {
        p.setPen(QPen(QColor("#5e6871"), 1));
        p.setBrush(QColor("#34373c"));
        p.drawRoundedRect(n.rect, 4, 4);
        p.save();
        p.setClipRect(n.rect.adjusted(1, 1, -1, -1));
        p.fillRect(QRectF(n.rect.x(), 0, nodeWidth, 27), n.color.darker(175));
        p.restore();
        QFont f = font();
        f.setPixelSize(12);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(QColor("#e1e4e8"));
        p.drawText(n.rect.adjusted(11, 0, -7, -65), Qt::AlignVCenter, n.title);
        f.setPixelSize(11);
        f.setWeight(QFont::Normal);
        p.setFont(f);
        p.setPen(QColor("#adb3bd"));
        p.drawText(n.rect.adjusted(11, 35, -7, -36), Qt::AlignVCenter, n.detail);
        p.setPen(QColor("#9299a4"));
        p.drawText(n.rect.adjusted(11, 62, -9, -8), Qt::AlignRight | Qt::AlignVCenter, "Geometry");
        p.setBrush(n.color);
        p.setPen(QColor("#25272b"));
        p.drawEllipse(n.rect.topLeft() + QPointF(0, 59), 3.5, 3.5);
        p.drawEllipse(n.rect.topRight() + QPointF(0, 59), 3.5, 3.5);
        n.rect = QRectF(16 + n.rect.x() * scale, std::max(12.0, (height() - 92 * scale) / 2),
                        nodeWidth * scale, 92 * scale);
    }
}
void NodeView::mousePressEvent(QMouseEvent *e) {
    for (const auto &n : nodes_)
        if (n.rect.contains(e->position())) {
            if (activated)
                activated(n.role);
            break;
        }
}
} // namespace nect::studio
