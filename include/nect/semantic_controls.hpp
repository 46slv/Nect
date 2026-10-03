#pragma once
#include "nect/core.hpp"

namespace nect {
// Presentation metadata projected from canonical type/public-parameter owners.
// Contains defaults and constraints, never an instance's current authored value.
struct SemanticEnumChoice { std::string value,label; };
struct SemanticParameterDescriptor {
    std::string key, value_type="number", unit, domain;
    double default_value=0;
    std::optional<bool> boolean_default;
    std::optional<ColorValue> color_default;
    std::optional<std::array<double,2>> point_default;
    std::string coordinate_space;
    std::optional<std::string> enum_default;
    std::vector<SemanticEnumChoice> choices;
    std::optional<double> minimum, maximum, step;
    std::string widget_hint="numeric", label, help;
    // Signed turns are authored unchanged; only the indicator wraps at 360.
    std::string angle_semantics;
};
enum class SemanticWidget { numeric, angle, slider, toggle, dropdown, color, point };
struct SemanticControlResolution {
    SemanticWidget widget=SemanticWidget::numeric;
    bool fallback=false;
    std::string status;
};
SemanticControlResolution validate_semantic_descriptor(const SemanticParameterDescriptor&);
std::optional<SemanticParameterDescriptor> builtin_semantic_descriptor(
    const std::string& type,const std::string& parameter);
std::optional<SemanticParameterDescriptor> property_semantic_descriptor(const Document&,const Ref&);
SemanticParameterDescriptor gradient_endpoint_semantic_descriptor(const std::string& endpoint);
SemanticParameterDescriptor macro_semantic_descriptor(const Document&,const Ref&);
}
