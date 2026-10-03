#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retains the exact whole-Text selection. Locale drafts never edit the Document.
// Apply changes only the literal locale in one Session transaction / Undo;
// Cancel discards the draft. Linked locale and unsupported contexts are refused.
QWidget* make_text_locale_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
