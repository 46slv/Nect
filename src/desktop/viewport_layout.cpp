#include "viewport_layout.hpp"
#include "visual_style.hpp"
#include <QHBoxLayout>
#include <QDockWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <stdexcept>
#include <algorithm>

namespace nect::desktop {
void configure_fixed_panel(QDockWidget* panel,Qt::DockWidgetAreas area) {
    if(!panel)throw std::invalid_argument("Fixed panel requires a dock widget");
    panel->setAllowedAreas(area);
    // Preserve resize, visibility actions and configured tabs, without exposing
    // user-driven detaching or arbitrary dock rearrangement.
    panel->setFeatures(QDockWidget::DockWidgetClosable);
}
QPoint anchored_popup_position(const QRect& anchor,const QSize& popup,const QRect& available,
    bool prefer_above,int gap) {
    const auto below=anchor.bottom()+1+gap;
    const auto above=anchor.top()-popup.height()-gap;
    const bool below_fits=below+popup.height()<=available.bottom()+1;
    const bool above_fits=above>=available.top();
    const auto y=prefer_above?(above_fits?above:below_fits?below:above):
        (below_fits?below:above_fits?above:below);
    return {std::clamp(anchor.left(),available.left(),std::max(available.left(),available.right()-popup.width()+1)),
        std::clamp(y,available.top(),std::max(available.top(),available.bottom()-popup.height()+1))};
}
ViewportLayout::ViewportLayout(QWidget* view,QWidget* parent):QWidget(parent) {
    if(!view)throw std::invalid_argument("Viewport layout requires a view");
    setObjectName("nect-viewport-layout");
    layout_=new QVBoxLayout(this);layout_->setContentsMargins(0,0,0,0);layout_->setSpacing(0);
    scroll_=new QScrollArea(this);scroll_->setObjectName("canvas-utility-scroll");
    scroll_->setFrameShape(QFrame::NoFrame);scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFixedHeight(VisualMetrics::utility_strip_min_height);
    connect(scroll_->horizontalScrollBar(),&QScrollBar::rangeChanged,this,[this](int,int maximum) {
        scroll_->setFixedHeight(maximum>0?VisualMetrics::utility_strip_max_height:VisualMetrics::utility_strip_min_height);
    });
    contents_=new QWidget;contents_->setObjectName("canvas-utility-strip");
    utility_layout_=new QHBoxLayout(contents_);utility_layout_->setContentsMargins(6,4,6,4);utility_layout_->setSpacing(4);
    scroll_->setWidget(contents_);layout_->addWidget(scroll_);layout_->addWidget(view,1);
}
void ViewportLayout::set_utility_placement(UtilityPlacement placement) {
    if(placement_==placement)return;
    layout_->removeWidget(scroll_);
    layout_->insertWidget(placement==UtilityPlacement::top?0:1,scroll_);
    placement_=placement;
}
}
