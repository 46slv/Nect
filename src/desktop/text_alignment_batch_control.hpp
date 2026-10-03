#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retains the exact whole-Text selection. Draft choices do not edit the Document;
// Apply changes every target through one Session transaction and one Undo.
// Unsupported selections and driven alignment are shown as explicit refusals.
QWidget* make_text_alignment_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
