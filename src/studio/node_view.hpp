#pragma once
#include "nect/core.hpp"
#include <QWidget>
#include <functional>
namespace nect::studio {
// A projection of the selected authored object's real processing pipeline.
// It owns no Document, History, evaluation, or duplicate editable graph.
class NodeView final : public QWidget {
  public:
    explicit NodeView(Session &, QWidget *parent = nullptr);
    void set_object(Id);
    std::function<void(QString)> activated;

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

  private:
    Session &session_;
    Id object_;
    struct Node {
        QRectF rect;
        QString title, detail, role;
        QColor color;
    };
    std::vector<Node> nodes_;
};
} // namespace nect::studio
