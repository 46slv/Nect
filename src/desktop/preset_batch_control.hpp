#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retains complete whole-object targets and their pinned Macro sources, the
// chosen Preset/source pins and exact document/Session/edit context.
// Browsing/Cancel only changes a draft; Apply appends a document Preset atomically.
// Any incompatible target refuses the entire selection rather than being skipped.
QWidget* make_preset_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
