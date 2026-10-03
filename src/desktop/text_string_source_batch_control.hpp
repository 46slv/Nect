#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retains the complete 2..1000 whole-Text selection. Family/locale source drafts
// commit through the existing typed commands in one Session edit and one Undo.
QWidget* make_text_string_source_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
