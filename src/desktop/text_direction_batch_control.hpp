#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Captures exact whole-Text IDs, Session identity, revision and gesture generation.
// Horizontal/Vertical/Mixed choices are drafts; Apply changes only direction in
// one Session transaction/Undo. Cancel discards the draft without editing.
// Any invalid selection or driven direction refuses visibly without unlinking.
QWidget* make_text_direction_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
