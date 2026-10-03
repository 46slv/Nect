#pragma once
#include "nect/core.hpp"
class QWidget;
namespace nect::desktop {
class Host;
// Generated from the canonical selected Artboard state; no second value store.
QWidget* make_artboard_background_controls(Host&,const Id& composition,const Id& artboard,QWidget* parent=nullptr);
}
