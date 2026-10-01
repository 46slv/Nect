#include "nect/core.hpp"
#include <algorithm>
#include <iterator>
#include <limits>
#include <type_traits>

namespace nect {
namespace {
// An admission estimate, not a measurement of process heap or resident memory.
// Count reserved container capacity, map/list nodes, allocation bookkeeping and
// every owned string capacity (including SSO, deliberately conservatively).
constexpr std::size_t allocation_overhead=32;
std::size_t add(std::size_t a,std::size_t b) {
    const auto max=std::numeric_limits<std::size_t>::max();return a>max-b?max:a+b;
}
std::size_t multiply(std::size_t a,std::size_t b) {
    const auto max=std::numeric_limits<std::size_t>::max();return b&&a>max/b?max:a*b;
}
template<class... T>std::size_t total(T... values) {std::size_t result=0;((result=add(result,values)),...);return result;}
std::size_t extra(const std::string& value){return total(value.capacity(),std::size_t{1},allocation_overhead);}
std::size_t extra(double){return 0;}
std::size_t extra(const Ref&);
std::size_t extra(const Binding&);
std::size_t extra(const Expression&);
std::size_t extra(const TextItalicDriver&);
std::size_t extra(const TextWeightDriver&);
std::size_t extra(const TextDirectionDriver&);
std::size_t extra(const TextLayoutDriver&);
std::size_t extra(const TextAlignmentDriver&);
std::size_t extra(const TextContentDriver&);
std::size_t extra(const TextFamilyDriver&);
std::size_t extra(const FillRuleDriver&);
std::size_t extra(const Scalar&);
std::size_t extra(const Point&);
std::size_t extra(const Contour&);
std::size_t extra(const TextSource&);
std::size_t extra(const Primitive&);
std::size_t extra(const PointEdit&);
std::size_t extra(const GradientStop&);
std::size_t extra(const Gradient&);
std::size_t extra(const ShapeOperation&);
std::size_t extra(const MacroPort&);
std::size_t extra(const MacroEndpoint&);
std::size_t extra(const MacroEdge&);
std::size_t extra(const MacroNode&);
std::size_t extra(const MacroPublicParameter&);
std::size_t extra(const MacroDefinitionRevision&);
std::size_t extra(const MacroDefinition&);
std::size_t extra(const MacroInstance&);
std::size_t extra(const ProcessingEntry&);
std::size_t extra(const GeometryMask&);
std::size_t extra(const Compositing&);
std::size_t extra(const Object&);
std::size_t extra(const ImageSource&);
std::size_t extra(const RasterAsset&);
std::size_t extra(const ArtboardParent&);
std::size_t extra(const Guide&);
std::size_t extra(const ArtboardGuide&);
std::size_t extra(const LayoutRect&);
std::size_t extra(const Margin&);
std::size_t extra(const Grid&);
std::size_t extra(const ArtboardLayout&);
std::size_t extra(const Artboard::SizeDriver&);
std::size_t extra(const Artboard&);
std::size_t extra(const Composition&);
std::size_t extra(const Collection&);
std::size_t extra(const NamedColor&);
std::size_t extra(const PresetEntry&);
std::size_t extra(const PresetDefinition&);
std::size_t extra(const Definition&);
std::size_t extra(const DefinitionInstance&);
std::size_t extra(const ArtboardTemplate&);
std::size_t extra(const ArtboardTemplateAssignment&);
std::size_t extra(const GroupPathFollowItem&);
std::size_t extra(const GroupPathFollow&);
template<class T>std::size_t extra(const std::optional<T>&);
template<class A,class B>std::size_t extra(const std::pair<A,B>&);
template<class T,std::size_t N>std::size_t extra(const std::array<T,N>&);
template<class T>std::size_t extra(const std::vector<T>&);
template<class K,class V>std::size_t extra(const std::map<K,V>&);
std::size_t extra(const TextLocaleDriver&);
template<class T>std::size_t extra(const std::optional<T>& value){return value?extra(*value):0;}
template<class A,class B>std::size_t extra(const std::pair<A,B>& value){return total(extra(value.first),extra(value.second));}
template<class T,std::size_t N>std::size_t extra(const std::array<T,N>& values) {
    std::size_t result=0;for(const auto& value:values)result=add(result,extra(value));return result;
}
template<class T>std::size_t extra(const std::vector<T>& values) {
    auto result=values.capacity()?total(multiply(values.capacity(),sizeof(T)),allocation_overhead):0;
    for(const auto& value:values)result=add(result,extra(value));return result;
}
template<class K,class V>std::size_t extra(const std::map<K,V>& values) {
    auto result=multiply(values.size()+1,sizeof(typename std::map<K,V>::value_type)+4*sizeof(void*)+allocation_overhead);
    for(const auto& [key,value]:values)result=total(result,extra(key),extra(value));return result;
}
std::size_t extra(const Ref& v){return total(extra(v.object),extra(v.point),extra(v.field));}
std::size_t extra(const Binding& v){return total(extra(v.source),extra(v.mode));}
std::size_t extra(const Expression& v){return extra(v.source);}
std::size_t extra(const TextItalicDriver& v){return std::visit([](const auto& value){return extra(value);},v);}
std::size_t extra(const TextWeightDriver& v){return extra(v.link);}
std::size_t extra(const TextDirectionDriver& v){return extra(v.link);}
std::size_t extra(const TextLayoutDriver& v){return extra(v.link);}
std::size_t extra(const TextAlignmentDriver& v){return extra(v.link);}
std::size_t extra(const TextContentDriver& v){return extra(v.link);}
std::size_t extra(const TextFamilyDriver& v){return extra(v.link);}
std::size_t extra(const TextLocaleDriver& v){return extra(v.link);}
std::size_t extra(const FillRuleDriver& v){return extra(v.link);}
std::size_t extra(const Scalar& v){return total(extra(v.binding),extra(v.expression));}
std::size_t extra(const Point& v){return total(extra(v.id),extra(v.x),extra(v.y),extra(v.in_angle),extra(v.in_length),extra(v.out_angle),extra(v.out_length));}
std::size_t extra(const Contour& v){return total(extra(v.id),extra(v.points));}
std::size_t extra(const TextSource& v){return total(extra(v.id),extra(v.content),extra(v.family),extra(v.locale),extra(v.layout),extra(v.direction),extra(v.alignment),extra(v.content_driver),extra(v.family_driver),extra(v.locale_driver),extra(v.direction_driver),extra(v.layout_driver),extra(v.alignment_driver),extra(v.weight_driver),extra(v.weight_expression),extra(v.italic_driver),extra(v.parameters));}
std::size_t extra(const Primitive& v){return total(extra(v.id),extra(v.type),extra(v.parameters));}
std::size_t extra(const PointEdit& v){return total(extra(v.id),extra(v.overrides),extra(v.enabled_driver),extra(v.enabled_expression));}
std::size_t extra(const GradientStop& v){return total(extra(v.id),extra(v.offset),extra(v.rgba));}
std::size_t extra(const Gradient& v){return total(extra(v.id),extra(v.type),extra(v.start_x),extra(v.start_y),extra(v.end_x),extra(v.end_y),extra(v.stops),extra(v.enabled_driver),extra(v.enabled_expression));}
std::size_t extra(const ShapeOperation& v){return total(extra(v.id),extra(v.type),extra(v.enabled_driver),extra(v.enabled_expression),extra(v.parameters),extra(v.composite),extra(v.fill_rule),extra(v.fill_rule_driver),extra(v.gradient),extra(v.line_join),extra(v.line_cap));}
std::size_t extra(const MacroPort& v){return total(extra(v.id),extra(v.domain));}
std::size_t extra(const MacroEndpoint& v){return total(extra(v.node),extra(v.port));}
std::size_t extra(const MacroEdge& v){return total(extra(v.from),extra(v.to));}
std::size_t extra(const MacroNode& v){return total(extra(v.operation),extra(v.input_port),extra(v.output_port));}
std::size_t extra(const MacroPublicParameter& v){return total(extra(v.id),extra(v.label),extra(v.node),extra(v.parameter),extra(v.value_type),extra(v.unit),extra(v.domain));}
std::size_t extra(const MacroDefinitionRevision& v){return total(extra(v.input),extra(v.output),extra(v.nodes),extra(v.edges),extra(v.output_mapping),extra(v.public_parameters));}
std::size_t extra(const MacroDefinition& v){return total(extra(v.id),extra(v.label),extra(v.revisions));}
std::size_t extra(const MacroInstance& v){return total(extra(v.definition),extra(v.overrides));}
std::size_t extra(const ProcessingEntry& v){return total(extra(static_cast<const ShapeOperation&>(v)),extra(v.macro));}
std::size_t extra(const PresetEntry& v){return total(extra(v.kind),extra(v.type),extra(v.parameters),extra(v.composite),extra(v.fill_rule),extra(v.line_join),extra(v.line_cap),extra(v.macro_definition),extra(v.overrides));}
std::size_t extra(const PresetDefinition& v){return total(extra(v.id),extra(v.label),extra(v.category),extra(v.tags),extra(v.target_domain),extra(v.entries));}
std::size_t extra(const Definition& v){return total(extra(v.id),extra(v.name),extra(v.root));}
std::size_t extra(const DefinitionInstance& v){return total(extra(v.definition),extra(v.overrides));}
std::size_t extra(const GroupPathFollowItem&){return 0;} // Scalars/bool are inline map-node storage.
std::size_t extra(const GroupPathFollow& v){return total(extra(v.id),extra(v.path),extra(v.contour),
    extra(v.start_mode),extra(v.mode),extra(v.deform_axis),extra(v.items));}
std::size_t extra(const GeometryMask& v){return total(extra(v.id),extra(v.source),extra(v.fill_rule),extra(v.mode),extra(v.mask_color_space),extra(v.enabled_driver),extra(v.enabled_expression));}
std::size_t extra(const Compositing& v){return total(extra(v.opacity),extra(v.blend),extra(v.isolated_driver),extra(v.mask));}
std::size_t extra(const ImageSource& v){return total(extra(v.asset),extra(v.width),extra(v.height));}
std::size_t extra(const RasterAsset& v){return total(extra(v.id),extra(v.name),extra(v.mode),extra(v.locator),v.payload?v.payload->bytes().size()+sizeof(RasterPayload)+allocation_overhead:0);}
std::size_t extra(const Object& v){return total(extra(v.id),extra(v.name),extra(v.children),extra(v.contours),extra(v.transform),extra(v.stack),extra(v.legacy_stroke),extra(v.source),extra(v.point_edit),extra(v.text),extra(v.anchor),extra(v.transform_parent),extra(v.visibility_driver),extra(v.visibility_expression),extra(v.compositing),extra(v.image),extra(v.instance),extra(v.path_follow));}
std::size_t extra(const ArtboardParent& v){return extra(v.artboard);}
std::size_t extra(const Guide& v){return total(extra(v.id),extra(v.name),extra(v.axis),extra(v.position_driver));}
std::size_t extra(const ArtboardGuide& v){return total(extra(v.id),extra(v.name),extra(v.axis));}
std::size_t extra(const LayoutRect&){return 0;}
std::size_t extra(const Margin& v){return total(extra(v.left_driver),extra(v.left_expression));}
std::size_t extra(const Grid& v){return total(extra(v.id),extra(v.bounds),extra(v.columns_driver),extra(v.columns_expression),extra(v.rows_driver),extra(v.rows_expression),extra(v.column_gutter_driver),extra(v.column_gutter_expression),extra(v.row_gutter_driver),extra(v.row_gutter_expression),extra(v.bounds_x_driver),extra(v.bounds_x_expression),extra(v.bounds_y_driver),extra(v.bounds_y_expression),extra(v.bounds_width_driver),extra(v.bounds_width_expression),extra(v.bounds_height_driver),extra(v.bounds_height_expression));}
std::size_t extra(const ArtboardLayout& v){return total(extra(v.margin),extra(v.grid));}
std::size_t extra(const ArtboardTemplate& v){return total(extra(v.id),extra(v.name),extra(v.source_artboard),extra(v.definition));}
std::size_t extra(const ArtboardTemplateAssignment& v){return total(extra(v.template_id),extra(v.grid_id),extra(v.content_instance),
    extra(v.width_override),extra(v.height_override),extra(v.guide_position_overrides),extra(v.guide_enabled_overrides),extra(v.detached_guides));}
std::size_t extra(const Artboard::SizeDriver& v){return std::visit([](const auto& value){return extra(value);},v.value);}
std::size_t extra(const Artboard& v){return total(extra(v.id),extra(v.name),extra(v.parent_size),extra(v.layout),extra(v.width_driver),extra(v.height_driver),extra(v.template_assignment),extra(v.local_guides));}
std::size_t extra(const Composition& v){return total(extra(v.id),extra(v.name),extra(v.roots),extra(v.artboards),extra(v.guides),extra(v.templates));}
std::size_t extra(const Collection& v){return total(extra(v.id),extra(v.name),extra(v.members));}
std::size_t extra(const NamedColor& v){return total(extra(v.id),extra(v.name),extra(v.rgba));}

std::string owner_name(const Document& before,const Document& after,const Id& id) {
    for(const auto* document:{&after,&before}) {
        if(const auto it=document->objects.find(id);it!=document->objects.end())return it->second.name;
        if(const auto it=document->named_colors.find(id);it!=document->named_colors.end())return it->second.name;
        if(const auto it=document->raster_assets.find(id);it!=document->raster_assets.end())return it->second.name;
        if(const auto it=document->preset_definitions.find(id);it!=document->preset_definitions.end())return it->second.label;
        for(const auto& comp:document->compositions) {
            if(comp.id==id)return comp.name;
            for(const auto& board:comp.artboards)if(board.id==id)return board.name;
            for(const auto& guide:comp.guides)if(guide.id==id)return guide.name;
            for(const auto& board:comp.artboards)if(board.layout&&board.layout->grid&&board.layout->grid->id==id)return "Grid";
        }
    }
    return id;
}
std::string operation_name(const std::string& type) {
    if(type=="nect.paint.fill")return "Fill";
    if(type=="nect.paint.stroke")return "Stroke";
    if(type=="nect.shape.repeater")return "Repeater";
    return "operation";
}
}

std::string Session::history_label(const std::vector<Command>& commands,const Document& candidate) const {
    const auto name=[&](const Id& id){return owner_name(document_,candidate,id);};
    const auto property_label=[&](const Ref& ref) {
        // A valid batch can edit then delete a target, including a newly created
        // target. A descriptive label must not reject that otherwise valid edit.
        std::string owner;
        try{owner=property_name(candidate,ref);}catch(const Error&){}
        if(owner.empty())try{owner=property_name(document_,ref);}catch(const Error&){}
        if(owner.empty())owner=name(ref.object);
        return owner+(ref.point.empty()?"":" / "+ref.point)+" / "+ref.field;
    };
    if(const auto* command=std::get_if<LinkTextWeight>(&commands.front());command&&command->batch) {
        const auto action=command->batch->mode==TextWeightBatchMode::edit?"Edit":
            command->batch->mode==TextWeightBatchMode::link?"Link":"Unlink";
        return std::string(action)+" Text weight batch ("+std::to_string(command->batch->targets.size())+")";
    }
    if(const auto* command=std::get_if<LinkTextWeight>(&commands.front()))
        return std::string(std::holds_alternative<Expression>(command->source)?
            "Text weight expression: ":"Link Text weight: ")+property_label(command->target);
    if(const auto* command=std::get_if<UnlinkTextWeight>(&commands.front()))
        return "Unlink Text weight: "+property_label(command->target);
    if(const auto* command=std::get_if<LinkGradientEnabled>(&commands.front()))
        return std::string(std::holds_alternative<Expression>(command->source)?
            "Set Gradient enabled expression: ":"Link Gradient enabled: ")+property_label(command->target);
    if(const auto* command=std::get_if<UnlinkGradientEnabled>(&commands.front()))
        return "Unlink Gradient enabled: "+property_label(command->target);
    if(const auto* command=std::get_if<LinkPointEditEnabled>(&commands.front()))
        return std::string(std::holds_alternative<Expression>(command->source)?
            "Set Point Edit enabled expression: ":"Link Point Edit enabled: ")+property_label(command->target);
    if(const auto* command=std::get_if<UnlinkPointEditEnabled>(&commands.front()))
        return "Unlink Point Edit enabled: "+property_label(command->target);
    if(const auto* command=std::get_if<LinkArtboardSize>(&commands.front()))
        return "Link Artboard size: "+property_label(command->target);
    if(const auto* command=std::get_if<SetArtboardSizeExpression>(&commands.front()))
        return "Artboard size expression: "+property_label(command->target);
    if(const auto* command=std::get_if<UnlinkArtboardSize>(&commands.front()))
        return "Unlink Artboard size: "+property_label(command->target);
    if(const auto* structural=std::get_if<StructuralCommand>(&commands.front())) {
        if(const auto* command=std::get_if<MacroCommand>(structural)) {
        if(!command->mutation)throw Error("INVALID_MACRO_COMMAND","Macro command has no mutation payload");
        return std::visit([&](const auto& mutation)->std::string {
            using T=std::decay_t<decltype(mutation)>;
            if constexpr(std::is_same_v<T,CreateMacroDefinition>)return "Create Macro: "+mutation.definition.label;
            else if constexpr(std::is_same_v<T,RenameMacroDefinition>)return "Rename Macro: "+mutation.label;
            else if constexpr(std::is_same_v<T,UpdateMacroDefinition>)return "Update Macro revision: "+mutation.definition;
            else if constexpr(std::is_same_v<T,DeleteMacroDefinition>)return "Delete Macro: "+mutation.definition;
            else if constexpr(std::is_same_v<T,InstantiateMacro>)return "Apply Macro: "+mutation.definition;
            else if constexpr(std::is_same_v<T,SetMacroOverride>)return "Set Macro parameter: "+mutation.public_parameter;
            else if constexpr(std::is_same_v<T,ResetMacroOverride>)return "Reset Macro parameter: "+mutation.public_parameter;
            else if constexpr(std::is_same_v<T,UpdateMacroInstance>)return "Update Macro instance revision: "+mutation.instance;
            else return "Detach Macro instance: "+mutation.instance;
        },*command->mutation);
        }
        if(const auto* command=std::get_if<ArtboardTemplateCommand>(structural))
            return std::visit([&](const auto& mutation)->std::string {
                using T=std::decay_t<decltype(mutation)>;
                if constexpr(std::is_same_v<T,CreateArtboardTemplate>)return "Create Template: "+mutation.value.name;
                else if constexpr(std::is_same_v<T,RenameArtboardTemplate>)return "Rename Template: "+mutation.name;
                else if constexpr(std::is_same_v<T,DeleteArtboardTemplate>)return "Delete Template: "+mutation.template_id;
                else if constexpr(std::is_same_v<T,AssignArtboardTemplate>)return "Assign Template: "+mutation.artboard_id;
                else if constexpr(std::is_same_v<T,SetArtboardTemplateOverride>)return "Set Template override: "+mutation.field;
                else if constexpr(std::is_same_v<T,ResetArtboardTemplateOverride>)return "Reset Template override: "+mutation.field;
                else if constexpr(std::is_same_v<T,DuplicateTemplateArtboard>)return "Duplicate Template Artboard: "+mutation.artboard_id;
                else return "Detach Template: "+mutation.artboard_id;
            },command->mutation);
        if(const auto* command=std::get_if<ArtboardGuideCommand>(structural))
            return std::visit([&](const auto& mutation)->std::string {
                using T=std::decay_t<decltype(mutation)>;
                if constexpr(std::is_same_v<T,AddArtboardGuide>)return "Add Artboard Guide: "+mutation.guide.name;
                else if constexpr(std::is_same_v<T,UpdateArtboardGuide>)return "Edit Artboard Guide: "+mutation.guide.name;
                else if constexpr(std::is_same_v<T,DeleteArtboardGuide>)return "Delete Artboard Guide: "+mutation.guide_id;
                else if constexpr(std::is_same_v<T,SetArtboardGuideOverride>)return "Override Artboard Guide: "+mutation.guide_id;
                else if constexpr(std::is_same_v<T,ResetArtboardGuideOverride>)return "Reset Artboard Guide: "+mutation.guide_id;
                else return "Detach Artboard Guide: "+mutation.guide_id;
            },command->mutation);
    }
    if(const auto* command=std::get_if<LayoutDependencyCommand>(&commands.front()))
        return std::visit([&](const auto& operation) {
            using T=std::decay_t<decltype(operation)>;
            if constexpr(std::is_same_v<T,LinkMarginLeft>)return "Link Margin left: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,UnlinkMarginLeft>)return "Unlink Margin left: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,LinkMarginBottom>)return "Link Margin bottom: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,SetMarginBottomExpression>)return "Margin bottom expression: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,UnlinkMarginBottom>)return "Unlink Margin bottom: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,LinkGridBoundsX>)return "Link Grid bounds x: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,SetGridBoundsXExpression>)return "Grid bounds x expression: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,UnlinkGridBoundsX>)return "Unlink Grid bounds x: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,SetGridColumnsExpression>)return "Grid columns expression: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,SetGridRowsExpression>)return "Grid rows expression: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,LinkGridBoundsY>)return "Link Grid bounds y: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,UnlinkGridBoundsY>)return "Unlink Grid bounds y: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,LinkGridBoundsWidth>)return "Link Grid bounds width: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,LinkGridBoundsHeight>)return "Link Grid bounds height: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,UnlinkGridBoundsHeight>)return "Unlink Grid bounds height: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,LinkGridColumnGutter>)return "Link Grid column gutter: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,SetGridColumnGutterExpression>)return "Grid column gutter expression: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,UnlinkGridColumnGutter>)return "Unlink Grid column gutter: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,LinkGridRowGutter>)return "Link Grid row gutter: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,UnlinkGridRowGutter>)return "Unlink Grid row gutter: "+property_label(operation.target);
            else if constexpr(std::is_same_v<T,SetGridBoundsWidthExpression>)return "Grid bounds width expression: "+property_label(operation.target);
            else return "Unlink Grid bounds width: "+property_label(operation.target);
        },command->operation);
    if(const auto* follow=std::get_if<GroupPathFollowCommand>(&commands.front()))
        return std::visit([&](const auto& command)->std::string {
            using F=std::decay_t<decltype(command)>;
            if constexpr(std::is_same_v<F,AttachGroupPathFollow>)return "Attach Group Path Follow: "+name(command.group);
            else if constexpr(std::is_same_v<F,UpdateGroupPathFollow>)return "Update Group Path Follow: "+name(command.group);
            else if constexpr(std::is_same_v<F,ClearGroupPathFollow>)return "Clear Group Path Follow: "+name(command.group);
            else if constexpr(std::is_same_v<F,SetGroupPathFollowItem>)return "Set Group Path Follow item: "+name(command.object);
            else return "Remove Group Path Follow item: "+name(command.object);
        },*follow);
    auto label=std::visit([&](const auto& c)->std::string {
        using T=std::decay_t<decltype(c)>;
        if constexpr(std::is_same_v<T,Set>)return "Set "+property_label(c.ref);
        else if constexpr(std::is_same_v<T,SetVisibility>)return std::string(c.visible?"Show: ":"Hide: ")+name(c.object);
        else if constexpr(std::is_same_v<T,SetCompositing>)return "Compositing: "+name(c.object);
        else if constexpr(std::is_same_v<T,SetMask>)return std::string(c.mask?"Set geometry mask: ":"Remove geometry mask: ")+name(c.object);
        else if constexpr(std::is_same_v<T,MaskObjects>)return std::string(c.top?"Mask with top: ":"Mask with bottom: ")+c.name;
        else if constexpr(std::is_same_v<T,Ungroup>)return "Ungroup: "+name(c.group);
        else if constexpr(std::is_same_v<T,MoveOut>)return "Move "+std::to_string(c.members.size())+" objects out of Folder: "+name(c.group);
        else if constexpr(std::is_same_v<T,PutInside>)return "Put "+std::to_string(c.members.size())+" objects inside: "+name(c.group);
        else if constexpr(std::is_same_v<T,SetExpression>)return "Expression: "+std::to_string(c.targets.size())+" properties: "+property_label(c.targets.front());
        else if constexpr(std::is_same_v<T,Link>)return "Link "+property_label(c.target);
        else if constexpr(std::is_same_v<T,Unlink>)return "Unlink "+property_label(c.target);
        else if constexpr(std::is_same_v<T,SetColor>)return "Set color: "+property_label(c.ref);
        else if constexpr(std::is_same_v<T,LinkColor>)return "Link color: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkColor>)return "Unlink color: "+property_label(c.ref);
        else if constexpr(std::is_same_v<T,LinkTextItalic>)return "Link Text italic: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,SetTextItalicExpression>)return "Text italic expression: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkTextItalic>)return "Unlink Text italic: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,LinkTextContent>)return "Link Text content: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkTextContent>)return "Unlink Text content: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,LinkTextLocale>)return "Link Text locale: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkTextLocale>)return "Unlink Text locale: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,LinkTextDirection>)return "Link Text direction: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkTextDirection>)return "Unlink Text direction: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,LinkTextLayout>)return "Link Text layout: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkTextLayout>)return "Unlink Text layout: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,LinkTextAlignment>)return "Link Text alignment: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkTextAlignment>)return "Unlink Text alignment: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,LinkFillRule>)return "Link Fill rule: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkFillRule>)return "Unlink Fill rule: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,Rename>)return "Rename: "+c.name;
        else if constexpr(std::is_same_v<T,RenameNamedColor>)return "Rename color: "+c.name;
        else if constexpr(std::is_same_v<T,CreateNamedColor>)return "Add named color: "+c.color.name;
        else if constexpr(std::is_same_v<T,DeleteNamedColor>)return "Delete named color: "+name(c.color);
        else if constexpr(std::is_same_v<T,CreatePath>)return "Add Path: "+c.name;
        else if constexpr(std::is_same_v<T,CreateFolder>)return "Create Folder: "+c.name;
        else if constexpr(std::is_same_v<T,CreatePrimitive>)return std::string(c.source.type=="nect.shape.circle"?"Add Circle: ":
            c.source.type=="nect.shape.rectangle"?"Add Rectangle: ":c.source.type=="nect.shape.polygon"?"Add Polygon: ":"Add Star: ")+c.name;
        else if constexpr(std::is_same_v<T,AddRasterAsset>)return "Add image asset: "+c.asset.name;
        else if constexpr(std::is_same_v<T,ReplaceRasterAsset>)return "Update image asset: "+c.asset.name;
        else if constexpr(std::is_same_v<T,DeleteRasterAsset>)return "Delete image asset: "+name(c.asset);
        else if constexpr(std::is_same_v<T,CreateImage>)return "Place Image: "+c.name;
        else if constexpr(std::is_same_v<T,CreateText>)return "Add Text: "+c.name;
        else if constexpr(std::is_same_v<T,UpdateText>)return "Edit Text: "+name(c.object);
        else if constexpr(std::is_same_v<T,GroupContiguous>)return "Group: "+c.name;
        else if constexpr(std::is_same_v<T,CenterAnchor>)return "Center Anchor: "+name(c.object);
        else if constexpr(std::is_same_v<T,SetPosition>)return "Set Position: "+name(c.object);
        else if constexpr(std::is_same_v<T,TransformAroundAnchor>)return "Transform around Anchor: "+name(c.object);
        else if constexpr(std::is_same_v<T,SetTransformParent>)return std::string(c.parent?"Attach Transform Parent: ":"Detach Transform Parent: ")+name(c.object);
        else if constexpr(std::is_same_v<T,EditProperties>)return std::string(c.relative?"Adjust ":"Set ")+std::to_string(c.targets.size())+" properties: "+property_label(c.targets.front());
        else if constexpr(std::is_same_v<T,LinkProperties>)return std::string(c.relative?"Relative link ":"Link ")+std::to_string(c.targets.size())+" properties from "+property_label(c.source);
        else if constexpr(std::is_same_v<T,UnlinkProperties>)return "Unlink "+std::to_string(c.targets.size())+" properties: "+property_label(c.targets.front());
        else if constexpr(std::is_same_v<T,DistributeObjects>) {
            auto result="Distribute "+c.axis+" to "+c.reference;
            if(c.spacing)result+=" · "+std::to_string(*c.spacing)+" du spacing";
            return result;
        }
        else if constexpr(std::is_same_v<T,AlignObjects>) {
            const auto reference=c.artboard&&c.reference=="selection"?"artboard:"+*c.artboard:c.reference;
            return "Align objects: "+c.axis+" "+c.alignment+" to "+reference;
        }
        else if constexpr(std::is_same_v<T,TransformObjects>)return "Transform "+std::to_string(c.objects.size())+" objects: "+name(c.objects.front());
        else if constexpr(std::is_same_v<T,StrokeStyle>)return "Stroke style: "+name(c.object);
        else if constexpr(std::is_same_v<T,TranslateObjects>)return "Move "+std::to_string(c.objects.size())+" objects: "+name(c.objects.front());
        else if constexpr(std::is_same_v<T,DeleteObjects>)return "Delete "+std::to_string(c.objects.size())+" object(s): "+name(c.objects.front());
        else if constexpr(std::is_same_v<T,DuplicateObjects>)return "Duplicate "+std::to_string(c.objects.size())+" object(s): "+name(c.objects.front());
        else if constexpr(std::is_same_v<T,ReorderObjects>)return "Reorder objects: "+name(c.parent.empty()?c.composition:c.parent);
        else if constexpr(std::is_same_v<T,AddArtboard>)return "Add Artboard: "+c.artboard.name;
        else if constexpr(std::is_same_v<T,UpdateArtboard>)return "Edit Artboard: "+c.artboard.name;
        else if constexpr(std::is_same_v<T,DeleteArtboard>)return "Delete Artboard: "+name(c.artboard);
        else if constexpr(std::is_same_v<T,ReorderArtboards>)return "Reorder Artboards: "+name(c.composition);
        else if constexpr(std::is_same_v<T,DetachArtboardParent>)return "Detach Artboard size: "+name(c.artboard);
        else if constexpr(std::is_same_v<T,AddGuide>)return "Add Guide: "+c.guide.name;
        else if constexpr(std::is_same_v<T,UpdateGuide>)return "Edit Guide: "+c.guide.name;
        else if constexpr(std::is_same_v<T,DeleteGuide>)return "Delete Guide: "+name(c.guide_id);
        else if constexpr(std::is_same_v<T,SetArtboardLayout>)return "Edit Layout: "+name(c.artboard_id);
        else if constexpr(std::is_same_v<T,AddPoint>)return "Add point: "+name(c.object);
        else if constexpr(std::is_same_v<T,RemovePoint>)return "Delete point: "+name(c.object);
        else if constexpr(std::is_same_v<T,CloseContour>)return std::string(c.closed?"Close contour: ":"Open contour: ")+name(c.object);
        else if constexpr(std::is_same_v<T,ReorderPoints>)return "Reorder points: "+name(c.object);
        else if constexpr(std::is_same_v<T,EnablePointEdit>)return std::string(c.enabled?"Enable Point Edit: ":"Bypass Point Edit: ")+name(c.object);
        else if constexpr(std::is_same_v<T,ClearPointEdit>)return "Clear Point Edit: "+name(c.object);
        else if constexpr(std::is_same_v<T,ConvertToPath>)return "Convert source to Path: "+name(c.object);
        else if constexpr(std::is_same_v<T,AddOperation>)return "Add "+operation_name(c.operation.type)+": "+name(c.object);
        else if constexpr(std::is_same_v<T,RemoveOperation>)return "Remove operation: "+name(c.object);
        else if constexpr(std::is_same_v<T,ReorderOperations>)return "Reorder operations: "+name(c.object);
        else if constexpr(std::is_same_v<T,EnableOperation>)return std::string(c.enabled?"Enable operation: ":"Bypass operation: ")+name(c.object);
        else if constexpr(std::is_same_v<T,LinkOperationEnabled>)return std::holds_alternative<Ref>(c.source)
            ?"Link operation enabled: "+name(c.target.object):"Set operation enabled expression: "+name(c.target.object);
        else if constexpr(std::is_same_v<T,UnlinkOperationEnabled>)return "Unlink operation enabled: "+name(c.target.object);
        else if constexpr(std::is_same_v<T,OperationOptions>)return "Edit operation options: "+name(c.object);
        else if constexpr(std::is_same_v<T,SetGradient>)return "Edit Gradient: "+name(c.object);
        else return "Edit document";
    },commands.front());
    if(commands.size()>1)label+=" ("+std::to_string(commands.size())+" commands)";
    return label;
}

std::size_t Session::estimate_history(const HistoryEntry& entry) {
    // Include a full extra list node per entry, conservatively covering its
    // sentinel allocation as well as bookkeeping on the supported STL.
    auto bytes=total(2*sizeof(HistoryEntry),8*sizeof(void*),2*allocation_overhead,extra(entry.label),extra(entry.compositions),extra(entry.collections));
    const auto changes=[&](const auto& items) {
        using Item=typename std::decay_t<decltype(items)>::value_type;
        auto result=items.capacity()?total(multiply(items.capacity(),sizeof(Item)),allocation_overhead):0;
        for(const auto& item:items)result=total(result,extra(item.key),extra(item.before),extra(item.after));
        return result;
    };
    return total(bytes,changes(entry.objects),changes(entry.colors),changes(entry.assets),changes(entry.presets),changes(entry.definitions),changes(entry.macros));
}

void Session::commit(Document candidate,std::string label) {
    HistoryEntry entry;entry.id=next_history_id_;entry.label=std::move(label);
    const auto diff=[](const auto& before,const auto& after,auto& changes) {
        auto a=before.begin(),b=after.begin();
        while(a!=before.end()||b!=after.end()) {
            if(b==after.end()||(a!=before.end()&&a->first<b->first)) {changes.push_back({a->first,a->second,{}});++a;}
            else if(a==before.end()||b->first<a->first) {changes.push_back({b->first,{},b->second});++b;}
            else {if(a->second!=b->second)changes.push_back({a->first,a->second,b->second});++a;++b;}
        }
    };
    diff(document_.objects,candidate.objects,entry.objects);diff(document_.named_colors,candidate.named_colors,entry.colors);
    diff(document_.raster_assets,candidate.raster_assets,entry.assets);diff(document_.preset_definitions,candidate.preset_definitions,entry.presets);
    diff(document_.definitions,candidate.definitions,entry.definitions);diff(document_.macro_definitions,candidate.macro_definitions,entry.macros);
    if(document_.compositions!=candidate.compositions)entry.compositions=std::pair{document_.compositions,candidate.compositions};
    if(document_.collections!=candidate.collections)entry.collections=std::pair{document_.collections,candidate.collections};
    entry.estimated_bytes=estimate_history(entry);
    if(entry.estimated_bytes>history_limits_.max_bytes)throw Error("HISTORY_LIMIT","This edit exceeds the estimated history memory budget; no edit was committed");
    if(next_history_id_==std::numeric_limits<std::uint64_t>::max())throw Error("HISTORY_LIMIT","History state ID space exhausted");

    // Allocate before touching the redo branch. Everything after insertion is
    // non-allocating, so admission/allocation failures preserve the old timeline.
    auto redo_start=std::next(history_.begin(),static_cast<std::ptrdiff_t>(history_cursor_));
    const bool has_redo=redo_start!=history_.end();
    history_.push_back(std::move(entry));const auto inserted=std::prev(history_.end());
    if(!has_redo)redo_start=inserted;
    for(auto it=redo_start;it!=inserted;) {history_bytes_-=it->estimated_bytes;it=history_.erase(it);}
    while(history_.size()>history_limits_.max_entries||history_bytes_>history_limits_.max_bytes-inserted->estimated_bytes) {
        boundary_id_=history_.front().id;history_bytes_-=history_.front().estimated_bytes;
        history_.pop_front();++pruned_entries_;
    }
    history_bytes_+=inserted->estimated_bytes;
    history_cursor_=history_.size();document_=std::move(candidate);++next_history_id_;++revision_;
}

void Session::apply_history(Document& candidate,const HistoryEntry& entry,bool forward) {
    const auto patch=[&](auto& objects,const auto& changes) {
        for(const auto& change:changes) {
            const auto& value=forward?change.after:change.before;
            if(value)objects.insert_or_assign(change.key,*value);else objects.erase(change.key);
        }
    };
    patch(candidate.objects,entry.objects);patch(candidate.named_colors,entry.colors);patch(candidate.raster_assets,entry.assets);patch(candidate.preset_definitions,entry.presets);patch(candidate.definitions,entry.definitions);patch(candidate.macro_definitions,entry.macros);
    if(entry.compositions)candidate.compositions=forward?entry.compositions->second:entry.compositions->first;
    if(entry.collections)candidate.collections=forward?entry.collections->second:entry.collections->first;
}

HistoryInfo Session::history() const {
    HistoryInfo info;info.current_id=boundary_id_;info.retained_bytes=history_bytes_;
    info.max_entries=history_limits_.max_entries;info.max_bytes=history_limits_.max_bytes;info.pruned_entries=pruned_entries_;
    info.states.reserve(history_.size()+1);info.states.push_back({boundary_id_,pruned_entries_?"Earliest retained state":"Initial document",0});
    std::size_t index=0;for(const auto& entry:history_) {
        info.states.push_back({entry.id,entry.label,entry.estimated_bytes});if(++index==history_cursor_)info.current_id=entry.id;
    }
    return info;
}

void Session::restore_history(std::uint64_t state_id,std::uint64_t expected) {
    check_revision(expected);std::size_t target=0;
    if(state_id!=boundary_id_) {
        auto found=std::find_if(history_.begin(),history_.end(),[&](const auto& entry){return entry.id==state_id;});
        if(found==history_.end())throw Error("HISTORY_STATE_NOT_FOUND","The requested history state is no longer retained");
        target=static_cast<std::size_t>(std::distance(history_.begin(),found))+1;
    }
    if(target==history_cursor_)return;
    auto candidate=document_;auto cursor=history_cursor_;
    auto at=std::next(history_.begin(),static_cast<std::ptrdiff_t>(cursor));
    while(cursor>target){--at;apply_history(candidate,*at,false);--cursor;}
    while(cursor<target){apply_history(candidate,*at,true);++at;++cursor;}
    validate(candidate);document_=std::move(candidate);history_cursor_=target;++revision_;
}

void Session::undo(std::uint64_t expected) {
    check_revision(expected);if(!can_undo())throw Error("NO_UNDO","No undo entry");
    const auto id=history_cursor_==1?boundary_id_:std::next(history_.begin(),static_cast<std::ptrdiff_t>(history_cursor_-2))->id;
    restore_history(id,expected);
}
void Session::redo(std::uint64_t expected) {
    check_revision(expected);if(!can_redo())throw Error("NO_REDO","No redo entry");
    restore_history(std::next(history_.begin(),static_cast<std::ptrdiff_t>(history_cursor_))->id,expected);
}
} // namespace nect
