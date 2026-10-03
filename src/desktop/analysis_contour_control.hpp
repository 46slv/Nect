#pragma once
#include "nect/core.hpp"
#include <functional>
class QDialog;
class QWidget;
namespace nect::desktop {
class Host;
// Draft-only chooser. Adoption uses the same Host entry as API/MCP.
QDialog* make_analysis_contour_dialog(Host&,const Id& composition,const Id& artboard,
    std::function<void(const Id&)> adopted,QWidget* parent=nullptr);
}
