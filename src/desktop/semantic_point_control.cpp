#include "semantic_point_control.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace nect::desktop {
namespace {
void require_finite(SemanticPointInput::Value value) {
    if(!std::isfinite(value[0])||!std::isfinite(value[1]))
        throw std::invalid_argument("Point input requires two finite coordinates");
}
QString exact_number(double value) {
    // QString's usual 17-digit scalar presentation, retaining negative zero.
    if(value==0&&std::signbit(value))return QStringLiteral("-0");
    return QString::number(value,'g',std::numeric_limits<double>::max_digits10);
}
bool coordinate(const QString& text,double& value) {
    const auto bytes=text.trimmed().toLatin1();
    const char* first=bytes.constData();
    const char* last=first+bytes.size();
    // from_chars is locale-independent and round-trips subnormal doubles. It
    // deliberately accepts only a complete decimal literal, not expressions.
    if(first!=last&&*first=='+') {
        ++first;
        if(first!=last&&(*first=='-'||*first=='+'))return false;
    }
    if(first==last)return false;
    const auto parsed=std::from_chars(first,last,value,std::chars_format::general);
    return parsed.ec==std::errc{}&&parsed.ptr==last&&std::isfinite(value);
}
}

SemanticPointInput::SemanticPointInput(Value value,QWidget* parent)
    :QWidget(parent),baseline_(value) {
    require_finite(value);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName("Point coordinates");
    setAccessibleDescription("Edit X and Y together. Enter applies both coordinates; Escape cancels the draft.");
    setToolTip(accessibleDescription());
    setProperty("nect-exact-value",true);
    auto* layout=new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    auto* row=new QHBoxLayout;
    layout->addLayout(row);
    auto* x_label=new QLabel("X",this);
    x_input_=new QLineEdit(this);
    x_input_->setObjectName("semantic-point-x");
    x_input_->setAccessibleName("X coordinate");
    x_input_->setAccessibleDescription(accessibleDescription());
    x_label->setBuddy(x_input_);
    row->addWidget(x_label);
    row->addWidget(x_input_);
    auto* y_label=new QLabel("Y",this);
    y_input_=new QLineEdit(this);
    y_input_->setObjectName("semantic-point-y");
    y_input_->setAccessibleName("Y coordinate");
    y_input_->setAccessibleDescription(accessibleDescription());
    y_label->setBuddy(y_input_);
    row->addWidget(y_label);
    row->addWidget(y_input_);
    apply_=new QPushButton("Apply",this);
    apply_->setObjectName("semantic-point-apply");
    apply_->setAutoDefault(false);
    apply_->setAccessibleName("Apply point coordinates");
    cancel_=new QPushButton("Cancel",this);
    cancel_->setObjectName("semantic-point-cancel");
    cancel_->setAutoDefault(false);
    cancel_->setAccessibleName("Cancel point draft");
    row->addWidget(apply_);
    row->addWidget(cancel_);
    error_=new QLabel(this);
    error_->setObjectName("semantic-point-error");
    error_->setAccessibleName("Point input error");
    error_->setWordWrap(true);
    layout->addWidget(error_);
    setTabOrder(x_input_,y_input_);
    setTabOrder(y_input_,apply_);
    setTabOrder(apply_,cancel_);
    installEventFilter(this);
    for(auto* control:{static_cast<QWidget*>(x_input_),static_cast<QWidget*>(y_input_),
                       static_cast<QWidget*>(apply_),static_cast<QWidget*>(cancel_)})
        control->installEventFilter(this);
    for(auto* input:{x_input_,y_input_})
        connect(input,&QLineEdit::textChanged,this,[this] {
            ++draft_epoch_;
            show_error({});
        });
    connect(apply_,&QPushButton::clicked,this,[this] {apply_draft();});
    connect(cancel_,&QPushButton::clicked,this,[this] {cancel_draft();});
    set_value(value);
}

QString SemanticPointInput::error_text() const {return error_->text();}

void SemanticPointInput::show_error(const QString& message) {
    error_->setText(message);
    error_->setVisible(!message.isEmpty());
}

void SemanticPointInput::set_value(Value value) {
    require_finite(value);
    ++draft_epoch_;
    baseline_=value;
    const QSignalBlocker x_blocker(x_input_);
    const QSignalBlocker y_blocker(y_input_);
    x_input_->setText(exact_number(value[0]));
    y_input_->setText(exact_number(value[1]));
    x_input_->setModified(false);
    y_input_->setModified(false);
    show_error({});
}

bool SemanticPointInput::can_apply() const {
    return isEnabled()&&x_input_->isEnabled()&&y_input_->isEnabled()&&apply_->isEnabled();
}

void SemanticPointInput::cancel_draft() {set_value(baseline_);}

void SemanticPointInput::apply_draft() {
    if(!can_apply())return;
    // New activation supersedes an older, still-queued draft, even if invalid.
    const auto epoch=++draft_epoch_;
    Value candidate{};
    if(!coordinate(x_input_->text(),candidate[0])) {
        show_error("INVALID_VALUE: Enter a finite X coordinate");
        return;
    }
    if(!coordinate(y_input_->text(),candidate[1])) {
        show_error("INVALID_VALUE: Enter a finite Y coordinate");
        return;
    }
    if(candidate==baseline_) {cancel_draft();return;}
    show_error({});
    QTimer::singleShot(0,this,[this,epoch,candidate] {
        if(draft_epoch_!=epoch||!can_apply())return;
        const auto callback=commit;
        set_value(candidate);
        // All widget access finishes before application code can delete us.
        if(callback)callback(candidate);
    });
}

bool SemanticPointInput::eventFilter(QObject* watched,QEvent* event) {
    if(event->type()==QEvent::EnabledChange) {
        if(watched!=cancel_&&!static_cast<QWidget*>(watched)->isEnabled())++draft_epoch_;
    }
    if(event->type()==QEvent::KeyPress) {
        auto* key=static_cast<QKeyEvent*>(event);
        if(key->key()==Qt::Key_Escape) {
            cancel_draft();
            key->accept();
            return true;
        }
        if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter) {
            if(watched==cancel_)cancel_draft();
            else apply_draft();
            key->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched,event);
}

void SemanticPointInput::changeEvent(QEvent* event) {
    if(event->type()==QEvent::EnabledChange&&!isEnabled())++draft_epoch_;
    QWidget::changeEvent(event);
}

}
