#include "semantic_slider.hpp"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <QTimerEvent>
#include <QWheelEvent>
#include <stdexcept>

namespace nect::desktop {

SemanticSlider::SemanticSlider(int minimum_delta,int maximum_delta,QWidget* parent)
    :QSlider(Qt::Horizontal,parent) {
    const auto span=static_cast<std::int64_t>(maximum_delta)-static_cast<std::int64_t>(minimum_delta);
    if(minimum_delta>0||maximum_delta<0||span<0||span>1000000)
        throw std::invalid_argument("Semantic slider range must contain zero and span at most 1000000 delta steps");
    setRange(minimum_delta,maximum_delta);
    setValue(0);
    setSingleStep(1);
    setPageStep(10);
    setTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(120);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    setAccessibleName("Parameter adjustment slider");
    setAccessibleDescription("Adjusts a parameter relative to its current value. Escape cancels a drag.");
    setToolTip("Drag to adjust. Arrow keys adjust one step; Page Up/Down adjust ten. Home/End reach the limits. Escape cancels a drag.");
    connect(this,&QSlider::valueChanged,this,[this](int delta) {
        if(interaction_==Interaction::idle)return;
        // Application callbacks can synchronously rebuild and delete this row.
        // Let Qt finish its input handler before delivering its latest value.
        pending_delta_=delta;
        if(dispatching_input_||preview_queued_)return;
        // Programmatic sliderPosition changes can also arrive from inside Qt's
        // setters. Deliver those after that stack returns, coalescing to the
        // latest position; release always flushes before its terminal callback.
        preview_queued_=true;
        const auto epoch=interaction_epoch_;
        QTimer::singleShot(0,this,[this,epoch] {
            if(interaction_epoch_!=epoch)return;
            preview_queued_=false;
            dispatch_preview();
        });
    });
}

void SemanticSlider::disarm() {
    interaction_=Interaction::idle;
    dispatching_input_=false;
    preview_queued_=false;
    pending_delta_.reset();
    ++interaction_epoch_;
    setRepeatAction(SliderNoAction);
    const bool was_blocked=blockSignals(true);
    setSliderDown(false);
    setValue(0);
    setSliderPosition(0);
    blockSignals(was_blocked);
}

bool SemanticSlider::start_interaction(Interaction kind) {
    if(interaction_!=Interaction::idle)return false;
    disarm();
    const auto epoch=interaction_epoch_;
    const auto callback=begin;
    if(!callback)return false;
    const QPointer<SemanticSlider> alive(this);
    const bool accepted=callback();
    if(!alive||!accepted||interaction_epoch_!=epoch)return false;
    interaction_=kind;
    return true;
}

void SemanticSlider::dispatch_preview(bool even_unchanged) {
    if(interaction_==Interaction::idle) {pending_delta_.reset();return;}
    if(!pending_delta_&&!even_unchanged)return;
    const auto delta=pending_delta_.value_or(value());
    pending_delta_.reset();
    const auto callback=preview_delta;
    if(callback)callback(static_cast<double>(delta));
}

void SemanticSlider::finish_interaction(bool accepted) {
    if(interaction_==Interaction::idle)return;
    const auto callback=accepted?commit:cancel;
    disarm();
    if(callback)callback();
}

void SemanticSlider::mousePressEvent(QMouseEvent* event) {
    if(event->button()!=Qt::LeftButton||event->buttons()!=Qt::LeftButton||!isEnabled()) {event->ignore();return;}
    event->accept();
    const QPointer<SemanticSlider> alive(this);
    if(!start_interaction(Interaction::mouse)) {event->ignore();return;}
    setFocus(Qt::MouseFocusReason);
    if(!alive||interaction_!=Interaction::mouse)return;
    dispatching_input_=true;
    QSlider::mousePressEvent(event);
    if(!alive)return;
    dispatching_input_=false;
    if(!event->isAccepted()) {finish_interaction(false);return;}
    dispatch_preview();
}

void SemanticSlider::mouseMoveEvent(QMouseEvent* event) {
    if(interaction_!=Interaction::mouse) {event->ignore();return;}
    const QPointer<SemanticSlider> alive(this);
    dispatching_input_=true;
    QSlider::mouseMoveEvent(event);
    if(!alive)return;
    dispatching_input_=false;
    dispatch_preview();
}

void SemanticSlider::mouseReleaseEvent(QMouseEvent* event) {
    if(event->button()!=Qt::LeftButton||interaction_!=Interaction::mouse) {event->ignore();return;}
    event->accept();
    const QPointer<SemanticSlider> alive(this);
    const auto epoch=interaction_epoch_;
    dispatching_input_=true;
    QSlider::mouseReleaseEvent(event);
    if(!alive)return;
    dispatching_input_=false;
    dispatch_preview();
    if(!alive||interaction_epoch_!=epoch||interaction_!=Interaction::mouse)return;
    finish_interaction(true);
}

void SemanticSlider::keyPressEvent(QKeyEvent* event) {
    if(event->key()==Qt::Key_Escape) {
        event->accept();
        finish_interaction(false);
        return;
    }
    switch(event->key()) {
    case Qt::Key_Left:case Qt::Key_Right:case Qt::Key_Up:case Qt::Key_Down:
    case Qt::Key_Home:case Qt::Key_End:case Qt::Key_PageUp:case Qt::Key_PageDown:
        break;
    default:
        QSlider::keyPressEvent(event);
        return;
    }
    event->accept();
    if(!isEnabled()||interaction_!=Interaction::idle)return;
    const QPointer<SemanticSlider> alive(this);
    if(!start_interaction(Interaction::keyboard))return;
    const auto epoch=interaction_epoch_;
    dispatching_input_=true;
    QSlider::keyPressEvent(event);
    if(!alive)return;
    dispatching_input_=false;
    dispatch_preview(true);
    if(!alive||interaction_epoch_!=epoch||interaction_!=Interaction::keyboard)return;
    finish_interaction(true);
}

void SemanticSlider::wheelEvent(QWheelEvent* event) {
    // A wheel used to scroll an Inspector must never author a parameter edit.
    event->ignore();
}

void SemanticSlider::timerEvent(QTimerEvent* event) {
    if(interaction_!=Interaction::mouse) {QSlider::timerEvent(event);return;}
    const QPointer<SemanticSlider> alive(this);
    dispatching_input_=true;
    QSlider::timerEvent(event);
    if(!alive)return;
    dispatching_input_=false;
    dispatch_preview();
}

void SemanticSlider::focusOutEvent(QFocusEvent* event) {
    const QPointer<SemanticSlider> alive(this);
    QSlider::focusOutEvent(event);
    if(!alive)return;
    finish_interaction(false);
}

}
