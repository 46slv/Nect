#pragma once

#include <QSlider>
#include <cstdint>
#include <functional>
#include <optional>

namespace nect::desktop {

// The position is a transient delta-step count from the caller's authored
// baseline. The caller owns property units, validation and Session history.
class SemanticSlider final : public QSlider {
public:
    explicit SemanticSlider(int minimum_delta,int maximum_delta,QWidget* parent=nullptr);
    std::function<bool()> begin;
    // Setter-driven previews are queued/coalesced; mouse/keyboard input and
    // release flush their latest preview after Qt's input handler returns.
    std::function<void(double)> preview_delta;
    std::function<void()> commit,cancel;
    void disarm();

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void timerEvent(QTimerEvent*) override;
    void focusOutEvent(QFocusEvent*) override;

private:
    enum class Interaction { idle,mouse,keyboard };
    bool start_interaction(Interaction);
    void dispatch_preview(bool even_unchanged=false);
    void finish_interaction(bool accepted);
    Interaction interaction_=Interaction::idle;
    bool dispatching_input_=false;
    bool preview_queued_=false;
    std::optional<int> pending_delta_;
    std::uint64_t interaction_epoch_=0;
};

}
