#pragma once
#include "nect/core.hpp"
#include <vector>

class QWidget;
namespace nect::desktop {
class Host;
// Definition/revision browsing is a draft. Apply appends one fresh instance
// to each frozen whole-object target through a single atomic Session command vector.
QWidget* make_macro_batch_controls(Host&,const std::vector<Id>&,QWidget* parent=nullptr);
}
