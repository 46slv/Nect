#pragma once
#include "nect/core.hpp"
class QWidget;
namespace nect::desktop {
class Host;
// Canonical occurrence-local visibility, including a Template content Instance.
// The optional parent retains only one stable-ID item selection as view state.
QWidget* make_instance_visibility_controls(Host&,const Id& instance,QWidget* parent=nullptr);
}
