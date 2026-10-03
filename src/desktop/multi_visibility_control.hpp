#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Captures 2–1000 distinct authored Object IDs. Common/Mixed is presentation;
// Show/Hide stays a draft until Apply commits one SetVisibility batch and Undo.
// Links and expressions are refused, never implicitly unlinked. This edits each
// object's own visibility, not Definition Instance descendant overrides.
QWidget* make_multi_visibility_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
