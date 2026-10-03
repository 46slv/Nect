#pragma once
#include <QWidget>
#include <QRect>
#include <QSize>

class QHBoxLayout;
class QVBoxLayout;
class QScrollArea;
class QDockWidget;

namespace nect::desktop {
enum class UtilityPlacement { top, bottom };
void configure_fixed_panel(QDockWidget* panel,Qt::DockWidgetAreas area);
QPoint anchored_popup_position(const QRect& anchor,const QSize& popup,const QRect& available,
    bool prefer_above=false,int gap=2);

// Presentation composition only. The supplied view and existing Utility controls
// retain their identity; no Session, selection or authored state lives here.
class ViewportLayout final : public QWidget {
public:
    explicit ViewportLayout(QWidget* view,QWidget* parent=nullptr);
    QWidget* utility_contents() const {return contents_;}
    QHBoxLayout* utility_layout() const {return utility_layout_;}
    QScrollArea* utility_scroll() const {return scroll_;}
    UtilityPlacement utility_placement() const {return placement_;}
    void set_utility_placement(UtilityPlacement placement);
private:
    QVBoxLayout* layout_=nullptr;
    QScrollArea* scroll_=nullptr;
    QWidget* contents_=nullptr;
    QHBoxLayout* utility_layout_=nullptr;
    UtilityPlacement placement_=UtilityPlacement::top;
};
}
