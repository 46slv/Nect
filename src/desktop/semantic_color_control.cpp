#include "semantic_color_control.hpp"

#include <QColorDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nect::desktop {
namespace {
using Value=SemanticColorInput::Value;
void validate(const Value& value) {
    if(!std::all_of(value.begin(),value.end(),[](double channel) {
        return std::isfinite(channel)&&channel>=0&&channel<=1;
    }))throw std::invalid_argument("sRGB color channels must be finite numbers from 0 to 1");
}
QColor displayed_color(const Value& value) {
    return QColor::fromRgbF(value[0],value[1],value[2],value[3]);
}
QString hex_color(const Value& value) {
    QString text="#";
    for(const auto channel:value)text+=QString("%1").arg(qRound(channel*255),2,16,QChar('0'));
    return text.toUpper();
}
QString precise_color(const Value& value) {
    return QString("sRGB · straight alpha\nRGBA: %1, %2, %3, %4")
        .arg(value[0],0,'g',17).arg(value[1],0,'g',17).arg(value[2],0,'g',17).arg(value[3],0,'g',17);
}
QIcon swatch_icon(const Value& value) {
    QPixmap pixmap(28,20);QPainter painter(&pixmap);
    for(int y=0;y<20;y+=5)for(int x=0;x<28;x+=5)
        painter.fillRect(x,y,5,5,((x+y)/5)%2?QColor(225,225,225):QColor(170,170,170));
    painter.fillRect(pixmap.rect(),displayed_color(value));
    painter.setPen(QColor(95,95,95));painter.drawRect(pixmap.rect().adjusted(0,0,-1,-1));painter.end();
    QIcon icon;
    // A color swatch must retain its pixels when the editor is disabled.
    for(const auto mode:{QIcon::Normal,QIcon::Active,QIcon::Selected,QIcon::Disabled})
        for(const auto state:{QIcon::Off,QIcon::On})icon.addPixmap(pixmap,mode,state);
    return icon;
}
// The existing Window/ColorTools HEX helpers are private to their translation
// units. Keep their accepted syntax here, parsing bytes directly into doubles.
Value parse_hex(QString text,const Value& baseline) {
    text=text.trimmed();if(text.startsWith('#'))text.remove(0,1);
    if((text.size()!=6&&text.size()!=8)||!std::all_of(text.begin(),text.end(),[](QChar channel) {
        return QStringLiteral("0123456789abcdefABCDEF").contains(channel);
    }))throw std::invalid_argument("Enter sRGB #RRGGBB or #RRGGBBAA");
    Value value=baseline;
    for(int i=0;i<text.size()/2;++i) {
        bool valid=false;const auto byte=text.mid(i*2,2).toUInt(&valid,16);
        if(!valid)throw std::invalid_argument("Invalid HEX channel");
        const auto index=static_cast<std::size_t>(i);
        // Merely retyping a displayed byte cannot quantize its exact baseline.
        if(static_cast<int>(byte)!=qRound(baseline[index]*255))value[index]=byte/255.0;
    }
    // The six-digit form explicitly selects opaque alpha, as in the existing
    // parser. Eight digits preserve exact alpha when its displayed byte is untouched.
    if(text.size()==6)value[3]=1;
    return value;
}
}

SemanticColorInput::SemanticColorInput(const Value& value,QWidget* parent)
    :QWidget(parent),baseline_(value) {
    validate(value);
    setObjectName("semantic-color-input");
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(2);
    auto* row=new QHBoxLayout;row->setContentsMargins(0,0,0,0);
    swatch_=new QPushButton("Color…",this);swatch_->setObjectName("semantic-color-swatch");
    swatch_->setAccessibleName("Choose color");swatch_->setFocusPolicy(Qt::StrongFocus);swatch_->installEventFilter(this);
    hex_=new QLineEdit(this);hex_->setObjectName("semantic-color-hex");
    hex_->setAccessibleName("HEX RGBA");
    hex_->setToolTip("sRGB #RRGGBB or #RRGGBBAA. Escape restores the displayed baseline.");
    hex_->setAccessibleDescription(hex_->toolTip());hex_->installEventFilter(this);
    row->addWidget(swatch_);row->addWidget(hex_,1);layout->addLayout(row);
    error_=new QLabel(this);error_->setObjectName("semantic-color-error");
    error_->setTextFormat(Qt::PlainText);error_->setWordWrap(true);error_->hide();layout->addWidget(error_);
    setFocusProxy(hex_);QWidget::setTabOrder(swatch_,hex_);
    set_value(value);
    connect(swatch_,&QPushButton::clicked,this,[this] {
        // Focus-out can queue a HEX submission before the swatch activation.
        // Settle it first, then open against the latest baseline after Qt has
        // finished the button handler. A canonical commit may delete this row.
        const auto epoch=picker_request_epoch_;
        QTimer::singleShot(0,this,[this,epoch] {
            if(picker_request_epoch_!=epoch||!isEnabled())return;
            const QPointer<SemanticColorInput> alive(this);
            finish_hex_edit();
            if(!alive)return;
            if(picker_request_epoch_==epoch&&isEnabled())open_picker();
        });
    });
    connect(hex_,&QLineEdit::editingFinished,this,[this] {
        if(!isEnabled()||!hex_->isModified()||hex_commit_queued_)return;
        hex_commit_queued_=true;const auto epoch=edit_epoch_;
        // A Session callback may synchronously replace the Inspector. Finish
        // Qt's key/focus handler before invoking it; Return/focus-out coalesce.
        QTimer::singleShot(0,this,[this,epoch] {
            if(edit_epoch_!=epoch)return;
            hex_commit_queued_=false;
            finish_hex_edit();
        });
    });
}

void SemanticColorInput::set_value(const Value& value) {
    validate(value);
    ++picker_request_epoch_;display_value(value);
}

void SemanticColorInput::display_value(const Value& value) {
    ++edit_epoch_;hex_commit_queued_=false;
    if(picker_)picker_->reject();
    baseline_=value;
    const QSignalBlocker blocker(hex_);hex_->setText(hex_color(value));hex_->setModified(false);
    swatch_->setIcon(swatch_icon(value));swatch_->setIconSize(QSize(28,20));
    swatch_->setToolTip(precise_color(value));swatch_->setAccessibleDescription(swatch_->toolTip());
    show_error({});
}

QString SemanticColorInput::error_text() const {return error_->text();}

void SemanticColorInput::show_error(const QString& message) {
    error_->setText(message);error_->setVisible(!message.isEmpty());
    error_->setAccessibleName(message);hex_->setProperty("nect-color-input-error",!message.isEmpty());
}

void SemanticColorInput::restore_baseline() {set_value(baseline_);}

bool SemanticColorInput::eventFilter(QObject* watched,QEvent* event) {
    if((watched==hex_||watched==swatch_)&&event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape) {
        restore_baseline();event->accept();return true;
    }
    return QWidget::eventFilter(watched,event);
}

void SemanticColorInput::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if(event->type()==QEvent::EnabledChange&&!isEnabled())restore_baseline();
}

void SemanticColorInput::finish_hex_edit() {
    if(!isEnabled()||!hex_->isModified())return;
    Value value;
    try {value=parse_hex(hex_->text(),baseline_);}
    catch(const std::invalid_argument& error) {show_error(QString::fromUtf8(error.what()));return;}
    // Reset even a no-op draft before the callback, which may delete this widget.
    hex_->setModified(false);show_error({});
    if(!submit_value(value))display_value(baseline_);
}

bool SemanticColorInput::submit_value(const Value& value) {
    if(!isEnabled()||value==baseline_)return false;
    const auto callback=commit;
    display_value(value);
    if(callback)callback(value);
    // Do not access any member after callback: the row may have been destroyed.
    return true;
}

bool SemanticColorInput::accept_picker_color(const QColor& selected) {
    return accept_picker_color(selected,displayed_color(baseline_));
}

bool SemanticColorInput::accept_picker_color(const QColor& selected,const QColor& displayed_initial) {
    if(!isEnabled()||!selected.isValid())return false;
    const Value source_value{selected.redF(),selected.greenF(),selected.blueF(),selected.alphaF()};
    // Validate before toRgb(), which may clamp an ExtendedRgb result.
    try {validate(source_value);}
    catch(const std::invalid_argument& error) {show_error(QString::fromUtf8(error.what()));return false;}
    const auto rgb=selected.toRgb();const auto initial=displayed_initial.toRgb();
    const Value selected_value{rgb.redF(),rgb.greenF(),rgb.blueF(),rgb.alphaF()};
    const Value displayed_value{initial.redF(),initial.greenF(),initial.blueF(),initial.alphaF()};
    Value value=baseline_;
    for(std::size_t i=0;i<value.size();++i)
        if(selected_value[i]!=displayed_value[i])value[i]=selected_value[i];
    show_error({});
    if(submit_value(value))return true;
    display_value(baseline_);return false;
}

void SemanticColorInput::open_picker() {
    if(!isEnabled())return;
    if(picker_) {picker_->raise();picker_->activateWindow();return;}
    auto* dialog=new QColorDialog(displayed_color(baseline_),this);picker_=dialog;
    dialog->setObjectName("semantic-color-picker");dialog->setWindowTitle("sRGB color");
    dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setOption(QColorDialog::ShowAlphaChannel);
    // Capture what this dialog actually displays, rather than converting its
    // unchanged result back into the caller's higher-precision authored value.
    const auto displayed_initial=dialog->currentColor();const auto epoch=edit_epoch_;
    const QPointer<QColorDialog> safe_dialog(dialog);
    connect(dialog,&QColorDialog::accepted,this,[this,safe_dialog,displayed_initial,epoch] {
        if(!safe_dialog)return;
        const auto selected=safe_dialog->currentColor();picker_.clear();
        QTimer::singleShot(0,this,[this,selected,displayed_initial,epoch] {
            if(edit_epoch_!=epoch||!isEnabled())return;
            (void)accept_picker_color(selected,displayed_initial);
        });
    });
    connect(dialog,&QColorDialog::rejected,this,[this] {picker_.clear();});
    dialog->open();
}

}
