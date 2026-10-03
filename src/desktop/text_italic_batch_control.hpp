#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Captures whole-Text IDs and keeps On/Off/Mixed choices local until Apply.
// The batch edits only literal italic in one Session transaction and one Undo;
// linked or expression-driven targets require a separate explicit unlink.
QWidget* make_text_italic_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
