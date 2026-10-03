#pragma once
#include "nect/core.hpp"

namespace nect {
// Presentation metadata projected from canonical type/public-parameter owners.
// Contains defaults and constraints, never an instance's current authored value.
struct SemanticParameterDescriptor {
    std::string key, value_type="number", unit, domain;
    double default_value=0;
    std::optional<double> minimum, maximum, step;
    std::string widget_hint="numeric", label, help;
    // Signed turns are authored unchanged; only the indicator wraps at 360.
    std::string angle_semantics;
};
enum class SemanticWidget { numeric, angle };
struct SemanticControlResolution {
    SemanticWidget widget=SemanticWidget::numeric;
    bool fallback=false;
    std::string status;
};
SemanticControlResolution validate_semantic_descriptor(const SemanticParameterDescriptor&);
std::optional<SemanticParameterDescriptor> builtin_semantic_descriptor(
    const std::string& type,const std::string& parameter);
std::optional<SemanticParameterDescriptor> property_semantic_descriptor(const Document&,const Ref&);
SemanticParameterDescriptor macro_semantic_descriptor(const Document&,const Ref&);
}
