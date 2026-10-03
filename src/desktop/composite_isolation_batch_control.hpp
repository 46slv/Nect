#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retains 2–1000 distinct authored whole-object IDs, including Groups and
// Instances. Shared/Mixed describes evaluated isolation. Apply changes only the
// literal in one transaction / Undo, and refuses any linked/expression target.
// Unlink sources explicitly freezes each driven target's evaluated value using
// UnlinkCompositeIsolated; it never normalizes the resulting per-object values.
// Callers must rotate Host.session_id whenever they replace its Session.
QWidget* make_composite_isolation_batch_controls(Host&,const std::vector<Id>& targets,QWidget* parent=nullptr);
}
