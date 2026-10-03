#pragma once
#include "nect/core.hpp"
class QWidget;
namespace nect::desktop {
class Host;
// Occurrence-local descendant Text content, including Template content.
// Drafts remain presentation state until Apply local; Use Source resets only
// the selected stable source Object. The optional parent retains selection.
QWidget* make_instance_text_content_controls(Host&,const Id& instance,QWidget* parent=nullptr);
}
