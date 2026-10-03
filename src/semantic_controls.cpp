#include "nect/semantic_controls.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace nect {
namespace {
void require_descriptor(bool valid,const std::string& message) {
    if(!valid)throw Error("INVALID_CONTROL_DESCRIPTOR",message);
}
}
SemanticControlResolution validate_semantic_descriptor(const SemanticParameterDescriptor& d) {
    require_descriptor(!d.key.empty()&&!d.label.empty()&&!d.unit.empty()&&!d.domain.empty(),
        "Control identity, label, unit and domain are required");
    if(d.value_type=="point") {
        require_descriptor(d.point_default.has_value()&&d.coordinate_space=="local"&&d.unit=="du","Point requires a typed local-space default");
        require_descriptor(!d.color_default&&!d.boolean_default&&!d.enum_default&&d.choices.empty()&&!d.minimum&&!d.maximum&&!d.step&&d.angle_semantics.empty(),
            "Point cannot carry constraints from another type");
        for(const auto coordinate:*d.point_default)require_descriptor(std::isfinite(coordinate),"Point coordinates must be finite");
        if(d.widget_hint=="point"||d.widget_hint=="vector")return {SemanticWidget::point,false,"SUPPORTED"};
        for(const auto* hint:{"numeric","angle","slider","range","toggle","enum","dropdown","color","curve"})
            require_descriptor(d.widget_hint!=hint,"Widget family is incompatible with point: "+d.widget_hint);
        require_descriptor(!d.widget_hint.empty(),"Widget hint is required");
        return {SemanticWidget::point,true,"UNKNOWN_WIDGET_HINT: "+d.widget_hint+"; using point for known local point type"};
    }
    require_descriptor(!d.point_default&&d.coordinate_space.empty(),"Non-point control cannot carry coordinate metadata");
    if(d.value_type=="color") {
        require_descriptor(d.color_default.has_value(),"Color requires a typed default");
        require_descriptor(d.unit=="srgb"&&!d.boolean_default&&!d.enum_default&&d.choices.empty()&&!d.minimum&&!d.maximum&&!d.step&&d.angle_semantics.empty(),
            "Color control cannot carry constraints from another type");
        const auto& color=*d.color_default;
        require_descriptor(color.space=="srgb"&&color.profile=="srgb"&&color.alpha=="straight","Unsupported color representation");
        for(const auto channel:color.rgba)require_descriptor(std::isfinite(channel)&&channel>=0&&channel<=1,"Color channels must be finite and normalized");
        if(d.widget_hint=="color")return {SemanticWidget::color,false,"SUPPORTED"};
        for(const auto* hint:{"numeric","angle","slider","range","toggle","enum","dropdown","point","vector","curve"})
            require_descriptor(d.widget_hint!=hint,"Widget family is incompatible with color: "+d.widget_hint);
        require_descriptor(!d.widget_hint.empty(),"Widget hint is required");
        return {SemanticWidget::color,true,"UNKNOWN_WIDGET_HINT: "+d.widget_hint+"; using color for known color type"};
    }
    require_descriptor(!d.color_default,"Non-color control cannot carry color metadata");
    if(d.value_type=="enum") {
        require_descriptor(d.enum_default.has_value()&&!d.choices.empty(),"Enum requires choices and a typed default");
        require_descriptor(d.unit=="enum"&&!d.boolean_default&&!d.minimum&&!d.maximum&&!d.step&&d.angle_semantics.empty(),
            "Enum control cannot carry constraints from another type");
        std::set<std::string> ids;
        for(const auto& choice:d.choices)
            require_descriptor(!choice.value.empty()&&!choice.label.empty()&&ids.insert(choice.value).second,"Enum choices require distinct nonempty IDs and labels");
        require_descriptor(ids.contains(*d.enum_default),"Enum default is not a declared choice");
        if(d.widget_hint=="dropdown"||d.widget_hint=="enum")return {SemanticWidget::dropdown,false,"SUPPORTED"};
        for(const auto* hint:{"numeric","angle","slider","range","toggle","color","point","vector","curve"})
            require_descriptor(d.widget_hint!=hint,"Widget family is incompatible with enum: "+d.widget_hint);
        require_descriptor(!d.widget_hint.empty(),"Widget hint is required");
        return {SemanticWidget::dropdown,true,"UNKNOWN_WIDGET_HINT: "+d.widget_hint+"; using dropdown for known enum type"};
    }
    require_descriptor(!d.enum_default&&d.choices.empty(),"Non-enum control cannot carry enum metadata");
    if(d.value_type=="boolean") {
        require_descriptor(d.boolean_default.has_value(),"Boolean control requires a typed default");
        require_descriptor(d.unit=="boolean"&&!d.minimum&&!d.maximum&&!d.step&&d.angle_semantics.empty(),
            "Boolean control cannot carry numeric constraints");
        if(d.widget_hint=="toggle")return {SemanticWidget::toggle,false,"SUPPORTED"};
        for(const auto* hint:{"numeric","angle","slider","range","enum","dropdown","color","point","vector","curve"})
            require_descriptor(d.widget_hint!=hint,"Widget family is incompatible with boolean: "+d.widget_hint);
        require_descriptor(!d.widget_hint.empty(),"Widget hint is required");
        return {SemanticWidget::toggle,true,"UNKNOWN_WIDGET_HINT: "+d.widget_hint+"; using toggle for known boolean type"};
    }
    require_descriptor(!d.boolean_default,"Number control cannot carry a boolean default");
    // Future known families are not compatible with a number. Unknown hints may
    // fall back only after the actual value type and its constraints are known.
    require_descriptor(d.value_type=="number","This control vertical supports only the known number type");
    require_descriptor(std::isfinite(d.default_value),"Control default must be finite");
    require_descriptor(!d.minimum||std::isfinite(*d.minimum),"Minimum must be finite");
    require_descriptor(!d.maximum||std::isfinite(*d.maximum),"Maximum must be finite");
    require_descriptor(!d.minimum||!d.maximum||*d.minimum<=*d.maximum,"Minimum exceeds maximum");
    require_descriptor((!d.minimum||d.default_value>=*d.minimum)&&(!d.maximum||d.default_value<=*d.maximum),
        "Default lies outside the declared range");
    require_descriptor(!d.step||(std::isfinite(*d.step)&&*d.step>0),"Scrub step must be finite and positive");
    if(d.widget_hint=="angle") {
        require_descriptor(d.unit=="degree"&&d.angle_semantics=="signed_turns_indicator_modulo_360",
            "Angle controls require degrees and explicit signed-turn semantics");
        return {SemanticWidget::angle,false,"SUPPORTED"};
    }
    require_descriptor(d.angle_semantics.empty(),"Angle semantics require an angle hint");
    if(d.widget_hint=="slider") {
        require_descriptor(d.minimum&&d.maximum&&d.step,"Slider requires bounded minimum, maximum and step");
        const auto ticks=(*d.maximum-*d.minimum)/ *d.step;
        require_descriptor(std::isfinite(ticks)&&ticks>=1&&ticks<=1000000,"Slider needs 1..1000000 finite steps");
        return {SemanticWidget::slider,false,"SUPPORTED"};
    }
    if(d.widget_hint=="numeric")return {SemanticWidget::numeric,false,"SUPPORTED"};
    for(const auto* hint:{"range","toggle","enum","dropdown","color","point","vector","curve"})
        require_descriptor(d.widget_hint!=hint,"Widget family is incompatible or outside this vertical: "+d.widget_hint);
    require_descriptor(!d.widget_hint.empty(),"Widget hint is required");
    return {SemanticWidget::numeric,true,"UNKNOWN_WIDGET_HINT: "+d.widget_hint+"; using numeric for known number type"};
}
std::optional<SemanticParameterDescriptor> builtin_semantic_descriptor(const std::string& type,const std::string& parameter) {
    if(parameter=="color"&&(type=="nect.paint.fill"||type=="nect.paint.stroke")) {
        const auto* owner=builtin_operation_type(type);
        if(!owner)return {};
        SemanticParameterDescriptor d;d.key="color";d.value_type="color";d.color_default=ColorValue{};
        const std::array<std::string,4> channels{"r","g","b","a"};
        for(std::size_t i=0;i<channels.size();++i)d.color_default->rgba[i]=owner->parameter_defaults.at(channels[i]);
        d.unit="srgb";d.domain=owner->input;d.widget_hint="color";d.label="Color";
        d.help="Straight-alpha sRGB color. Unchanged values retain their exact channel precision.";
        validate_semantic_descriptor(d);return d;
    }
    if(parameter=="fill_rule"&&(type=="nect.paint.fill"||type=="nect.shape.offset")) {
        const auto* owner=builtin_operation_type(type);
        if(!owner)return {};
        SemanticParameterDescriptor d;
        d.key="fill_rule";d.value_type="enum";d.enum_default=ShapeOperation{}.fill_rule;
        d.choices={{"nonzero","Nonzero winding"},{"evenodd","Even-odd"}};
        d.unit="enum";d.domain=owner->input;d.widget_hint="dropdown";d.label="Fill rule";
        d.help="Choose the winding rule used to determine which regions are filled.";
        validate_semantic_descriptor(d);return d;
    }
    if(parameter=="enabled") {
        const auto* owner=builtin_operation_type(type);
        if(!owner)return {};
        SemanticParameterDescriptor d;
        d.key="enabled";d.value_type="boolean";d.boolean_default=ShapeOperation{}.enabled;
        d.unit="boolean";d.domain=owner->input;d.widget_hint="toggle";d.label="Enabled";
        d.help="Enable or bypass this operation. Linked values are controlled by their source.";
        validate_semantic_descriptor(d);return d;
    }
    const bool amount=type=="nect.shape.offset"&&parameter=="amount";
    const bool rotation=type=="nect.shape.repeater"&&parameter=="rotation";
    const bool copies=type=="nect.shape.repeater"&&parameter=="copies";
    if(!amount&&!rotation&&!copies)return {};
    const auto* owner=builtin_operation_type(type);
    if(!owner||!owner->parameter_defaults.contains(parameter))
        throw Error("INVALID_CONTROL_DESCRIPTOR","Built-in descriptor has no canonical parameter default");
    SemanticParameterDescriptor d;
    d.key=parameter;d.default_value=owner->parameter_defaults.at(parameter);d.domain=owner->input;
    d.unit=amount?"du":copies?"scalar":"degree";d.minimum=amount?-1e6:copies?0:-1e9;d.maximum=amount?1e6:copies?1000:1e9;d.step=1;
    d.widget_hint=rotation?"angle":copies?"slider":"numeric";d.label=amount?"Amount":copies?"Copies":"Rotation";
    d.help=copies?"Whole-number copies from 0 to 1000. Escape cancels the draft.":amount?"Positive expands; negative contracts. Escape cancels the draft.":
        "Signed degrees per copy. The indicator wraps; the authored value retains all turns. Escape cancels.";
    if(rotation)d.angle_semantics="signed_turns_indicator_modulo_360";
    validate_semantic_descriptor(d);return d;
}
std::optional<SemanticParameterDescriptor> property_semantic_descriptor(const Document& document,const Ref& ref) {
    if(!ref.point.empty()||!ref.field.starts_with("op."))return {};
    const auto found=document.objects.find(ref.object);if(found==document.objects.end())return {};
    for(const auto& operation:found->second.stack) {
        if(operation.macro)continue;
        const auto prefix="op."+operation.id+".";
        if(ref.field.starts_with(prefix)&&(ref.field.substr(prefix.size())=="enabled"||ref.field.substr(prefix.size())=="fill_rule"||ref.field.substr(prefix.size())=="color"||operation.parameters.contains(ref.field.substr(prefix.size()))))
            return builtin_semantic_descriptor(operation.type,ref.field.substr(prefix.size()));
    }
    return {};
}
SemanticParameterDescriptor gradient_endpoint_semantic_descriptor(const std::string& endpoint) {
    require_descriptor(endpoint=="start"||endpoint=="end","Unknown gradient endpoint");
    const Gradient canonical;
    SemanticParameterDescriptor d;d.key=endpoint;d.value_type="point";d.unit="du";d.domain="local_gradient";
    d.coordinate_space="local";d.widget_hint="point";d.label=endpoint=="start"?"Start":"End";
    d.point_default=endpoint=="start"?std::array<double,2>{canonical.start_x.literal,canonical.start_y.literal}:
        std::array<double,2>{canonical.end_x.literal,canonical.end_y.literal};
    d.help="Edit both local-space coordinates in one change. Cancel preserves the current point.";
    validate_semantic_descriptor(d);return d;
}
SemanticParameterDescriptor macro_semantic_descriptor(const Document& document,const Ref& ref) {
    const auto object=document.objects.find(ref.object);
    if(object==document.objects.end())throw Error("MISSING_OBJECT",ref.object);
    const auto instance=std::find_if(object->second.stack.begin(),object->second.stack.end(),
        [&](const auto& entry){return entry.id==ref.point&&entry.macro.has_value();});
    if(instance==object->second.stack.end())throw Error("MISSING_MACRO_INSTANCE",ref.point);
    const auto& revision=document.macro_definitions.at(instance->macro->definition).revisions.at(instance->macro->pinned_revision);
    const auto parameter=std::find_if(revision.public_parameters.begin(),revision.public_parameters.end(),
        [&](const auto& p){return p.id==ref.field;});
    if(parameter==revision.public_parameters.end())throw Error("MISSING_MACRO_PARAMETER",ref.field);
    const auto node=std::find_if(revision.nodes.begin(),revision.nodes.end(),[&](const auto& n){return n.operation.id==parameter->node;});
    require_descriptor(node!=revision.nodes.end(),"Public parameter target node is missing");
    auto descriptor=builtin_semantic_descriptor(node->operation.type,parameter->parameter);
    require_descriptor(descriptor.has_value(),"Public parameter target has no supported semantic descriptor");
    require_descriptor(parameter->value_type==descriptor->value_type&&parameter->unit==descriptor->unit&&parameter->domain==descriptor->domain,
        "Public parameter type/unit/domain differs from its canonical target");
    descriptor->key=parameter->id;descriptor->label=parameter->label;
    descriptor->default_value=node->operation.parameters.at(parameter->parameter).literal;
    validate_semantic_descriptor(*descriptor);return *descriptor;
}
}
