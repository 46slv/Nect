#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Retained stable IDs, never another object's operation ID or a display name.
struct MultiFillRuleTarget {
    Id object;
    Id operation;
    Ref ref() const { return operation_ref(object,operation,"fill_rule"); }
    bool operator==(const MultiFillRuleTarget&) const = default;
};
// Every target must be a distinct Path/Text Fill v1 at the same stack position.
// Literal, Link and Unlink are drafts until Apply; each is one Session batch and
// one Undo. Literal editing refuses drivers, replacing drivers is explicit, and
// Unlink freezes each target's own evaluated value. Cancel/no-op are history-free.
// Session/document/revision/gesture guards reject a retired Inspector context.
QWidget* make_multi_fill_rule_controls(Host&,const std::vector<MultiFillRuleTarget>& targets,QWidget* parent=nullptr);
}
