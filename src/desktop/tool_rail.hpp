#pragma once
#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QToolBar>
#include <QToolButton>
#include <functional>

namespace nect::desktop {
// Workspace presentation only: activating a slot never authors a command.
class ToolRail final : public QToolBar {
public:
    enum class Tool { selection, pen, text, anchor, guide, gradient, hand, zoom };
    std::function<void(Tool)> activate;
    std::function<void(bool)> variant_chosen;
    explicit ToolRail(QWidget* parent=nullptr) : QToolBar("Tools",parent) {
        setObjectName("tool-rail");setOrientation(Qt::Vertical);
        setMovable(false);setFloatable(false);setAllowedAreas(Qt::LeftToolBarArea);
        setStyleSheet("QToolBar#tool-rail { spacing: 2px; padding: 2px; border: 0; border-right: 1px solid #464c55; }"
            "QToolBar#tool-rail QToolButton { border: 1px solid transparent; border-radius: 2px; }"
            "QToolBar#tool-rail QToolButton:hover { background: #39424d; }"
            "QToolBar#tool-rail QToolButton:checked { background: #435365; border-color: #a5b9cc; }"
            "QToolBar#tool-rail QToolButton:focus { border-color: #d4dce6; }");
        selection_=slot(Tool::selection,"tool-selection","Selection");
        pen_=slot(Tool::pen,"tool-pen","Pen · P / G");
        text_=slot(Tool::text,"tool-text","Horizontal Text · hold for writing variants");
        auto* variants=new QMenu(text_);variants->setObjectName("text-tool-variants");
        for(const auto* direction:{"horizontal","vertical"}) {
            const bool vertical=QString::fromLatin1(direction)=="vertical";
            auto* choice=variants->addAction(vertical?"Vertical Text":"Horizontal Text");
            choice->setObjectName(vertical?"tool-text-vertical":"tool-text-horizontal");
            choice->setCheckable(true);choice->setChecked(!vertical);
            choice->setIcon(icon(Tool::text,vertical));
            connect(choice,&QAction::triggered,this,[this,vertical]{
                set_vertical_text(vertical);if(activate)activate(Tool::text);
                if(variant_chosen)variant_chosen(vertical);
            });
        }
        text_->setMenu(variants);text_->setPopupMode(QToolButton::DelayedPopup);
        anchor_=slot(Tool::anchor,"tool-anchor","Anchor Edit · Y");
        guide_=slot(Tool::guide,"tool-guide","Guide Edit");
        gradient_=slot(Tool::gradient,"tool-gradient","Gradient Edit");
        hand_=slot(Tool::hand,"tool-hand","Hand · drag to pan the view · Esc exits");
        zoom_=slot(Tool::zoom,"tool-zoom","Zoom · click to zoom in · Alt-click to zoom out · Esc exits");
        set_gradient_available(false,"Select one Object with an enabled Gradient.");
        set_active(Tool::selection);
    }
    bool vertical_text()const{return vertical_;}
    void set_vertical_text(bool vertical){vertical_=vertical;refresh_text();}
    void set_active(Tool tool) {
        selection_->setChecked(tool==Tool::selection);pen_->setChecked(tool==Tool::pen);
        text_->setChecked(tool==Tool::text);anchor_->setChecked(tool==Tool::anchor);
        guide_->setChecked(tool==Tool::guide);
        gradient_->setChecked(tool==Tool::gradient);
        hand_->setChecked(tool==Tool::hand);
        zoom_->setChecked(tool==Tool::zoom);
    }
    void set_gradient_available(bool enabled,const QString& reason) {
        gradient_->setEnabled(enabled);
        const auto label=reason.isEmpty()?QString("Gradient Edit"):QString("Gradient Edit · ")+reason;
        gradient_->setToolTip(label);gradient_->setAccessibleName(label);
    }
private:
    QToolButton *selection_,*pen_,*text_,*anchor_,*guide_,*gradient_,*hand_,*zoom_;
    bool vertical_=false;
    static QIcon icon(Tool tool,bool vertical=false) {
        QIcon result;
        for(const int size:{20,40}) {
            QPixmap pixels(size,size);pixels.fill(Qt::transparent);QPainter p(&pixels);
            p.setRenderHint(QPainter::Antialiasing);p.scale(size/20.0,size/20.0);
            p.setPen(QPen(QColor("#d7dfe8"),1.4));p.setBrush(Qt::NoBrush);
            if(tool==Tool::selection) {QPolygonF arrow;arrow<<QPointF(4,2)<<QPointF(15,11)<<QPointF(10,12)<<QPointF(8,17);p.drawPolygon(arrow);}
            else if(tool==Tool::pen) {QPolygonF nib;nib<<QPointF(4,16)<<QPointF(6,6)<<QPointF(13,3)<<QPointF(17,10);p.drawPolygon(nib);p.drawLine(4,16,10,10);p.drawEllipse(QPointF(11,9),1.5,1.5);}
            else if(tool==Tool::anchor) {p.drawEllipse(QPointF(10,10),5,5);p.drawLine(10,2,10,18);p.drawLine(2,10,18,10);}
            else if(tool==Tool::guide) {p.setPen(QPen(QColor("#d7dfe8"),1.2,Qt::DashLine));
                p.drawLine(3,8,17,8);p.drawLine(8,3,8,17);p.drawRect(QRectF(12,12,4,4));}
            else if(tool==Tool::gradient) {p.drawLine(4,14,16,6);p.drawRect(QRectF(2,12,4,4));
                p.setBrush(QColor("#d7dfe8"));p.drawEllipse(QPointF(16,6),2,2);}
            else if(tool==Tool::zoom) {p.drawEllipse(QRectF(3,3,10,10));p.drawLine(12,12,17,17);
                p.drawLine(5,8,11,8);p.drawLine(8,5,8,11);}
            else if(tool==Tool::hand) {QPainterPath hand;hand.moveTo(7,17);
                hand.lineTo(3,10);hand.cubicTo(2,8,4,7,5,9);hand.lineTo(6,11);
                hand.lineTo(6,4);hand.cubicTo(6,2,8,2,8,4);hand.lineTo(8,9);
                hand.lineTo(8,3);hand.cubicTo(8,1,10,1,10,3);hand.lineTo(10,9);
                hand.lineTo(10,4);hand.cubicTo(10,2,12,2,12,4);hand.lineTo(12,9);
                hand.lineTo(12,6);hand.cubicTo(12,4,14,4,14,6);hand.lineTo(14,13);
                hand.lineTo(12,17);hand.closeSubpath();p.drawPath(hand);}
            else {p.drawLine(4,4,14,4);p.drawLine(9,4,9,16);p.drawLine(6,16,12,16);
                if(vertical){p.drawLine(17,5,17,15);p.drawLine(15,12,17,15);p.drawLine(19,12,17,15);}}
            p.end();result.addPixmap(pixels);
        }return result;
    }
    QToolButton* slot(Tool tool,const char* name,const QString& label) {
        auto* button=new QToolButton(this);button->setObjectName(QString::fromLatin1(name));
        button->setAccessibleName(label);button->setToolTip(label);button->setCheckable(true);
        button->setFixedSize(32,32);button->setIconSize(QSize(20,20));button->setIcon(icon(tool));
        button->setFocusPolicy(Qt::StrongFocus);addWidget(button);
        connect(button,&QToolButton::clicked,this,[this,tool]{if(activate)activate(tool);});return button;
    }
    void refresh_text() {
        text_->setIcon(icon(Tool::text,vertical_));
        const auto label=QString(vertical_?"Vertical Text":"Horizontal Text")+" · hold for writing variants";
        text_->setAccessibleName(label);text_->setToolTip(label);
        const auto choices=text_->menu()->actions();
        choices[0]->setChecked(!vertical_);choices[1]->setChecked(vertical_);
    }
};
}
