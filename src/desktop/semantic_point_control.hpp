#pragma once

#include <QString>
#include <QWidget>
#include <array>
#include <cstdint>
#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;

namespace nect::desktop {

// A paired literal draft, supplied and refreshed by its canonical owner. This
// widget retains only a transient Cancel baseline; Session owns authored values,
// driver validation, atomic two-coordinate edits and history.
class SemanticPointInput final : public QWidget {
public:
    using Value=std::array<double,2>;

    explicit SemanticPointInput(Value,QWidget* parent=nullptr);
    QLineEdit* x_input() const {return x_input_;}
    QLineEdit* y_input() const {return y_input_;}
    QPushButton* apply_button() const {return apply_;}
    QPushButton* cancel_button() const {return cancel_;}
    Value value() const {return baseline_;}
    // Refreshes both fields, drops queued drafts and never requests an edit.
    // Nonfinite values throw before changing any existing presentation.
    void set_value(Value);
    QString error_text() const;
    // Apply/Enter deliver a single finite pair after Qt's input stack returns.
    // The callback may synchronously rebuild or delete this widget's parent.
    std::function<void(Value)> commit;

protected:
    bool eventFilter(QObject*,QEvent*) override;
    void changeEvent(QEvent*) override;

private:
    void apply_draft();
    void cancel_draft();
    void show_error(const QString&);
    bool can_apply() const;

    Value baseline_;
    QLineEdit* x_input_=nullptr;
    QLineEdit* y_input_=nullptr;
    QPushButton* apply_=nullptr;
    QPushButton* cancel_=nullptr;
    QLabel* error_=nullptr;
    std::uint64_t draft_epoch_=0;
};

}
