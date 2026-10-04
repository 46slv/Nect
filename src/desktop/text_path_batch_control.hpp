#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retains whole-Text IDs and the exact Session/document/revision/gesture context.
// Explicit Path/Contour choices and parameters remain drafts until one atomic
// UpdateText batch. Mixed parameter placeholders keep each target's value.
// Detach removes only the selected Text attachments; Cancel discards drafts.
QWidget* make_text_path_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
