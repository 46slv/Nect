#include "visual_style.hpp"

namespace nect::desktop {
QString application_style_sheet() {
    return QStringLiteral(
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
}
QString utility_button_style_sheet() {
    return QStringLiteral(
        "QToolButton{color:#dbe4ee;background:#252d38;border:1px solid #566373;border-radius:4px;padding:4px 8px;}"
        "QToolButton:hover{background:#354556;border-color:#63cce9;}"
        "QToolButton:checked{color:#e9fbff;background:#244b5b;border-color:#48c6e9;}"
        "QToolButton:focus{border:2px solid #f3d17a;}"
        "QToolButton:disabled{color:#77818d;background:#20252c;border-color:#38414c;}"
    );
}
QString utility_popover_style_sheet() {
    return QStringLiteral(
        "QDialog#utility-setup-popover{background:#202833;border:1px solid #566373;border-radius:6px;color:#dbe4ee;}"
    );
}
}
