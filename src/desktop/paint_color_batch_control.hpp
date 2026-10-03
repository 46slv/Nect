#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Capture exact op.ID.color Refs from the whole-object selection. Every target
// must be the same solid Fill/Stroke kind at the same stack position. Unsupported
// or driven targets refuse the complete selection; no target is silently omitted.
// Dialog acceptance commits one canonical SetColor batch and one Undo step.
QWidget* make_paint_color_batch_controls(Host&,const std::vector<Ref>& targets,QWidget* parent=nullptr);
}
