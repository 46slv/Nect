#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retains 2–1000 exact authored whole-object IDs. Selecting a supported blend
// is a draft; Apply changes only blend in one Session transaction / Undo.
// Cancel discards the draft, preserving every target's own isolation and drivers.
QWidget* make_multi_blend_mode_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
