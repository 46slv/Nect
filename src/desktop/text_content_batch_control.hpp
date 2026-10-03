#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retains the exact whole-Text selection. Content drafts never edit the Document.
// Apply changes only the literal content in one Session transaction / Undo;
// Cancel discards the draft. Linked content and unsupported contexts are refused.
QWidget* make_text_content_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
