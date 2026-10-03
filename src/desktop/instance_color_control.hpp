#pragma once
#include "nect/core.hpp"
class QWidget;
namespace nect::desktop {
class Host;
// Occurrence-local descendant solid Fill color, including Template content.
// The optional parent retains one stable Object/Fill selection as view state.
QWidget* make_instance_color_controls(Host&,const Id& instance,QWidget* parent=nullptr);
}
