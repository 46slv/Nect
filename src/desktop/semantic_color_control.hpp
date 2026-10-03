#pragma once

#include <QColor>
#include <QPointer>
#include <QWidget>
#include <array>
#include <cstdint>
#include <functional>

class QColorDialog;
class QLabel;
class QLineEdit;
class QPushButton;

namespace nect::desktop {

// This is an sRGB/straight-alpha input draft, not an authored color store.
// The caller supplies the exact displayed baseline, owns Session validation and
// history, and refreshes it with set_value() after external edits or refusals.
class SemanticColorInput final : public QWidget {
public:
    using Value=std::array<double,4>;
    explicit SemanticColorInput(const Value& value,QWidget* parent=nullptr);
    std::function<void(Value)> commit;
    QPushButton* swatch_button() const {return swatch_;}
    QLineEdit* hex_input() const {return hex_;}
    const Value& value() const {return baseline_;}
    void set_value(const Value& value);
    QString error_text() const;
    // Invalid QColor represents picker cancellation. A color equal to the
    // displayed baseline preserves the original doubles and requests no edit.
    // This synchronous seam is also used after the native picker closes.
    bool accept_picker_color(const QColor& selected);

protected:
    bool eventFilter(QObject*,QEvent*) override;
    void changeEvent(QEvent*) override;

private:
    void display_value(const Value& value);
    void restore_baseline();
    void show_error(const QString& message);
    void finish_hex_edit();
    void open_picker();
    bool accept_picker_color(const QColor& selected,const QColor& displayed_initial);
    bool submit_value(const Value& value);
    Value baseline_;
    QPushButton* swatch_=nullptr;
    QLineEdit* hex_=nullptr;
    QLabel* error_=nullptr;
    QPointer<QColorDialog> picker_;
    std::uint64_t edit_epoch_=0,picker_request_epoch_=0;
    bool hex_commit_queued_=false;
};

}
