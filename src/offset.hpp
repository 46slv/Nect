#pragma once
#include "nect/core.hpp"

namespace nect {
// Transient object-local projection. Authored topology and paint bases survive.
void apply_offset(EvaluatedShape& shape,double amount,double miter_limit,
                  const std::string& line_join,const std::string& fill_rule);
}
