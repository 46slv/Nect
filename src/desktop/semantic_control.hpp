#pragma once
#include "nect/semantic_controls.hpp"
#include <QLineEdit>
#include <QToolButton>
#include <functional>

namespace nect::desktop {
class SemanticNumberInput final : public QLineEdit {
public:
    explicit SemanticNumberInput(const QString& text,QWidget* parent=nullptr);
    std::function<void(QString)> multiline;
    void cancel_draft();
protected:
    void keyPressEvent(QKeyEvent*) override;
private:
    QString initial_text_; // transient cancel presentation, never an authored value
};
// Both scalar properties and published Macro parameters use this factory.
SemanticNumberInput* semantic_number_input(const SemanticParameterDescriptor&,const QString& text,QWidget* parent=nullptr);
void annotate_semantic_control(QWidget*,const SemanticParameterDescriptor&);
class SemanticScrub final : public QToolButton {
public:
    explicit SemanticScrub(QWidget* parent=nullptr);
    std::function<bool()> begin;
    std::function<void(double)> preview_delta;
    std::function<void()> commit,cancel;
    void disarm();
protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    bool dragging_=false;
    double delta_=0;
    QPointF previous_;
};
}
