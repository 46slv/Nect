#include "semantic_control.hpp"
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMouseEvent>

namespace nect::desktop {
SemanticNumberInput::SemanticNumberInput(const QString& text,QWidget* parent):QLineEdit(text,parent),initial_text_(text) {}
void SemanticNumberInput::cancel_draft() {setText(initial_text_);setModified(false);}
void SemanticNumberInput::keyPressEvent(QKeyEvent* event) {
    if(event->key()==Qt::Key_Escape) {cancel_draft();event->accept();return;}
    if(event->matches(QKeySequence::Paste)&&multiline) {
        const auto paste=QApplication::clipboard()->text();
        if(paste.contains('\n')||paste.contains('\r')) {
            auto draft=text();const auto start=selectionStart()<0?cursorPosition():selectionStart();
            draft.replace(start,selectedText().size(),paste);setModified(false);multiline(draft);event->accept();return;
        }
    }
    QLineEdit::keyPressEvent(event);
}
void annotate_semantic_control(QWidget* control,const SemanticParameterDescriptor& d) {
    const auto resolution=validate_semantic_descriptor(d);
    control->setProperty("nect-semantic-key",QString::fromStdString(d.key));
    control->setProperty("nect-value-type",QString::fromStdString(d.value_type));
    control->setProperty("nect-unit",QString::fromStdString(d.unit));
    control->setProperty("nect-domain",QString::fromStdString(d.domain));
    control->setProperty("nect-widget-hint",QString::fromStdString(d.widget_hint));
    control->setProperty("nect-widget-kind",resolution.widget==SemanticWidget::dropdown?"dropdown":resolution.widget==SemanticWidget::toggle?"toggle":resolution.widget==SemanticWidget::angle?"angle":resolution.widget==SemanticWidget::slider?"slider":"numeric");
    control->setProperty("nect-control-status",QString::fromStdString(resolution.status));
    control->setProperty("nect-control-fallback",resolution.fallback);
    control->setProperty("nect-exact-value",true);
    if(resolution.fallback)control->setAccessibleDescription(QString::fromStdString(resolution.status));
    control->setToolTip(QString::fromStdString(d.help)+(resolution.fallback?"\n"+QString::fromStdString(resolution.status):QString{}));
}
SemanticNumberInput* semantic_number_input(const SemanticParameterDescriptor& d,const QString& text,QWidget* parent) {
    // Validate before allocating or selecting a widget.
    if(d.value_type!="number")throw Error("INVALID_CONTROL_DESCRIPTOR","Number input requires a numeric descriptor");
    validate_semantic_descriptor(d);
    auto* input=new SemanticNumberInput(text,parent);annotate_semantic_control(input,d);return input;
}
SemanticScrub::SemanticScrub(QWidget* parent):QToolButton(parent) {
    setText(QString::fromUtf8("↔"));setFixedSize(24,24);setFocusPolicy(Qt::StrongFocus);
    setToolTip("Drag horizontally to adjust. Shift is fine; Control is coarse. Left/Right adjusts one step. Escape cancels.");
}
void SemanticScrub::disarm() {dragging_=false;setDown(false);}
void SemanticScrub::mousePressEvent(QMouseEvent* event) {
    if(event->button()!=Qt::LeftButton||!isEnabled()) {event->ignore();return;}
    if(begin&&!begin()) {event->ignore();return;}
    dragging_=true;delta_=0;previous_=event->position();setDown(true);setFocus(Qt::MouseFocusReason);event->accept();
}
void SemanticScrub::mouseMoveEvent(QMouseEvent* event) {
    if(!dragging_) {event->ignore();return;}
    const double scale=event->modifiers().testFlag(Qt::ShiftModifier)?0.1:event->modifiers().testFlag(Qt::ControlModifier)?10:1;
    delta_+=(event->position().x()-previous_.x())*scale;previous_=event->position();
    if(preview_delta)preview_delta(delta_);event->accept();
}
void SemanticScrub::mouseReleaseEvent(QMouseEvent* event) {
    if(event->button()!=Qt::LeftButton||!dragging_) {event->ignore();return;}
    disarm();if(commit)commit();event->accept();
}
void SemanticScrub::keyPressEvent(QKeyEvent* event) {
    if(event->key()==Qt::Key_Escape&&dragging_) {disarm();if(cancel)cancel();event->accept();return;}
    if(event->key()==Qt::Key_Left||event->key()==Qt::Key_Right) {
        if(!dragging_&&(!begin||begin())) {if(preview_delta)preview_delta(event->key()==Qt::Key_Left?-1:1);if(commit)commit();}
        event->accept();return;
    }
    QToolButton::keyPressEvent(event);
}
}
