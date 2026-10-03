#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Exact authored object/operation IDs, not an evaluated Macro node or a source
// row. The pair maps directly to EnableOperation without interpreting Ref.field.
struct OperationEnabledBatchTarget {
    Id object;
    Id operation;
    auto operator<=>(const OperationEnabledBatchTarget&) const = default;
};
// Captures 2–1000 distinct pairs of the same built-in type/behavior version at
// the same stack slot. Enable/Bypass stays a local draft until Apply, then uses
// one Session transaction/Undo. A driven target refuses the entire selection;
// this control never filters targets, converts source rows or unlinks drivers.
// Callers must rotate Host.session_id when replacing the Session, as Host does.
QWidget* make_operation_enabled_batch_controls(Host&,
    const std::vector<OperationEnabledBatchTarget>& targets,QWidget* parent=nullptr);
}
