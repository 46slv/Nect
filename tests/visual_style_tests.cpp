#include "visual_style.hpp"
#include <QByteArray>
#include <iostream>
#include <stdexcept>

using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why) {
    if(!ok)throw std::runtime_error(why);
    ++checks;
}

// Frozen runtime bytes from main.cpp and Window at 6ebfbb4. Keep independent
// of visual_style.cpp so accidental presentation changes fail these contracts.
const QByteArray original_application_style_sheet(
    "QMainWindow,QDialog,QWidget{background:#25292f;color:#e1e5eb;}"
    "QLineEdit,QTreeWidget,QListWidget{background:#1c2026;border:1px solid #393f49;border-radius:3px;padding:4px;}"
    "QTreeWidget::item,QListWidget::item{padding:5px;}"
    "QTreeWidget::item:selected,QListWidget::item:selected{background:#354d5b;}"
    "QPushButton{background:#333943;border:1px solid #454d58;border-radius:3px;padding:4px;}"
    "QPushButton:hover{background:#414a56;}"
    "QGroupBox{border:1px solid #3a414b;border-radius:4px;margin-top:12px;padding-top:10px;}"
    "QGroupBox::title{subcontrol-origin:margin;left:8px;}"
    "QToolBar{spacing:8px;padding:4px;border-bottom:1px solid #3b424a;}"
    "QMenu{border:1px solid #49515c;}QMenu::item:selected{background:#43505f;}"
);
const QByteArray original_utility_button_style_sheet(
    "QToolButton{color:#dbe4ee;background:#252d38;border:1px solid #566373;border-radius:4px;padding:4px 8px;}"
    "QToolButton:hover{background:#354556;border-color:#63cce9;}"
    "QToolButton:checked{color:#e9fbff;background:#244b5b;border-color:#48c6e9;}"
    "QToolButton:focus{border:2px solid #f3d17a;}"
    "QToolButton:disabled{color:#77818d;background:#20252c;border-color:#38414c;}"
);
const QByteArray original_utility_popover_style_sheet(
    "QDialog#utility-setup-popover{background:#202833;border:1px solid #566373;border-radius:6px;color:#dbe4ee;}"
);
}

int main() {
    try {
        check(application_style_sheet().toUtf8()==original_application_style_sheet,
            "Application stylesheet preserves the exact original bytes");
        check(utility_button_style_sheet().toUtf8()==original_utility_button_style_sheet,
            "Utility button stylesheet preserves exact bytes before IconOnly padding");
        check(utility_popover_style_sheet().toUtf8()==original_utility_popover_style_sheet,
            "Utility popover stylesheet preserves the exact original bytes");
        check(VisualMetrics::utility_strip_min_height==42,"utility_strip_min_height preserves the original value");
        check(VisualMetrics::utility_strip_max_height==58,"utility_strip_max_height preserves the original value");
        check(VisualMetrics::utility_icon_size==20,"utility_icon_size preserves the original value");
        check(VisualMetrics::utility_toggle_width==36,"utility_toggle_width preserves the original value");
        check(VisualMetrics::utility_toggle_height==32,"utility_toggle_height preserves the original value");
        check(VisualMetrics::utility_zoom_width==92,"utility_zoom_width preserves the original value");
        check(VisualMetrics::utility_readback_min_width==185,"utility_readback_min_width preserves the original value");
        check(VisualMetrics::utility_readback_max_width==270,"utility_readback_max_width preserves the original value");
        check(VisualMetrics::utility_popover_margin==8,"utility_popover_margin preserves the original value");
        check(VisualMetrics::utility_popover_spacing==6,"utility_popover_spacing preserves the original value");
        std::cout<<"PASS "<<checks<<" visual style preservation checks\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL "<<checks<<": "<<e.what()<<'\n';
        return 1;
    }
}
