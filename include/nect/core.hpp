#pragma once
#include "nect/raster.hpp"
#include <array>
#include <cstdint>
#include <compare>
#include <utility>
#include <map>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace nect {
using Id = std::string;

struct Error : std::runtime_error {
    std::string code;
    Error(std::string code, const std::string& message);
};

struct Ref {
    Id object;
    Id point;
    std::string field;
    auto operator<=>(const Ref&) const = default;
};

struct Binding {
    Ref source;
    double scale = 1;
    double offset = 0;
    std::string mode = "copy_local_value";
    bool operator==(const Binding&) const = default;
};

struct Expression {
    std::string source;
    unsigned version = 1;
    bool operator==(const Expression&) const = default;
};

// Text Italic is a typed boolean dependency; its bounded expression subset is
// intentionally separate from the same-type integer Text weight link below.
using TextItalicDriver = std::variant<Ref,Expression>;
struct TextWeightDriver {
    Ref link;
    bool operator==(const TextWeightDriver&) const = default;
};
struct TextContentDriver {
    Ref link;
    bool operator==(const TextContentDriver&) const = default;
};
struct TextFamilyDriver {
    Ref link;
    bool operator==(const TextFamilyDriver&) const = default;
};
struct TextLocaleDriver {
    Ref link;
    bool operator==(const TextLocaleDriver&) const = default;
};
struct TextDirectionDriver {
    Ref link;
    bool operator==(const TextDirectionDriver&) const = default;
};
struct TextLayoutDriver {
    Ref link;
    bool operator==(const TextLayoutDriver&) const = default;
};
struct TextAlignmentDriver {
    Ref link;
    bool operator==(const TextAlignmentDriver&) const = default;
};
struct FillRuleDriver {
    Ref link;
    bool operator==(const FillRuleDriver&) const = default;
};

struct TextPathAttachment {
    Id path;
    Id contour;
    std::string start_mode="distance";
    double start=0;
    double spacing=0;
    bool reversed=false;
    bool operator==(const TextPathAttachment&) const = default;
};

struct Scalar {
    double literal = 0;
    std::optional<Binding> binding;
    std::optional<Expression> expression;
    bool operator==(const Scalar&) const = default;
};

struct Point {
    Id id;
    Scalar x, y, in_angle, in_length, out_angle, out_length;
    bool operator==(const Point&) const = default;
};

struct Contour {
    Id id;
    bool closed = false;
    std::vector<Point> points;
    bool operator==(const Contour&) const = default;
};

enum class Kind { group, path, text, image };

inline constexpr std::size_t document_raster_bytes_limit=24*1024*1024;
inline constexpr std::uint64_t document_raster_pixels_limit=33554432;
struct RasterAsset {
    Id id;
    std::string name;
    std::string mode="embedded"; // linked / embedded
    std::string locator; // Absolute local path for linked; never evaluated or fetched by core.
    Raster payload; // Immutable accepted original bytes, shared by snapshots/history.
    bool operator==(const RasterAsset& other) const {
        return id==other.id&&name==other.name&&mode==other.mode&&locator==other.locator&&
            (payload==other.payload||(payload&&other.payload&&*payload==*other.payload));
    }
};
struct ImageSource {
    Id asset;
    Scalar width{1},height{1};
    bool operator==(const ImageSource&) const=default;
};

struct TextSource {
    Id id;
    unsigned version=1;
    std::string content="Text",family="Yu Gothic",locale="ja-JP";
    std::optional<TextContentDriver> content_driver;
    std::optional<TextFamilyDriver> family_driver;
    std::optional<TextLocaleDriver> locale_driver;
    std::optional<TextDirectionDriver> direction_driver;
    std::optional<TextLayoutDriver> layout_driver;
    std::optional<TextAlignmentDriver> alignment_driver;
    std::optional<TextPathAttachment> path_attachment;
    std::string layout="auto",direction="horizontal",alignment="start";
    unsigned weight=400;
    std::optional<TextWeightDriver> weight_driver;
    bool italic=false;
    std::optional<TextItalicDriver> italic_driver;
    std::map<std::string,Scalar> parameters;
    bool operator==(const TextSource&) const = default;
};
TextSource default_text(Id id,std::string content="Text");
std::vector<std::string> text_fonts();

struct Primitive {
    Id id;
    std::string type; // nect.shape.circle / rectangle / polygon / star
    unsigned version = 1;
    std::map<std::string,Scalar> parameters;
    bool operator==(const Primitive&) const = default;
};
Primitive default_primitive(Id id,const std::string& type);

struct PointEdit {
    Id id;
    unsigned version = 1;
    bool enabled = true;
    // Absolute, local-space scalar overrides of stable generated point fields.
    // Fields not present continue to evaluate from the source generator.
    std::map<Id,std::map<std::string,Scalar>> overrides;
    std::optional<Ref> enabled_driver;
    bool operator==(const PointEdit&) const = default;
};

struct GradientStop {
    Id id;
    Scalar offset;
    std::array<Scalar,4> rgba{{{0,{}},{0,{}},{0,{}},{1,{}}}};
    bool operator==(const GradientStop&) const = default;
};
struct Gradient {
    Id id;
    std::string type="linear"; // linear / radial, local user-space coordinates
    unsigned version=1;
    bool enabled=true;
    Scalar start_x{0,{}},start_y{0,{}},end_x{100,{}},end_y{0,{}};
    std::vector<GradientStop> stops;
    std::optional<Ref> enabled_driver;
    bool operator==(const Gradient&) const = default;
};
struct ShapeOperation {
    Id id;
    std::string type; // nect.paint.*, nect.shape.*, nect.group.posterize
    unsigned version=1;
    bool enabled=true;
    std::optional<Ref> enabled_driver;
    std::map<std::string,Scalar> parameters;
    std::string composite="below";
    std::string fill_rule="nonzero";
    std::optional<FillRuleDriver> fill_rule_driver;
    std::optional<Gradient> gradient;
    std::string line_join="miter"; // Offset v1 / Stroke v2: miter, round, bevel.
    std::string line_cap="butt"; // Stroke v2: butt, round, square; v1 stays butt.
    bool operator==(const ShapeOperation&) const = default;
};
ShapeOperation default_operation(Id id,const std::string& type);
// Immutable descriptors for executable built-ins. External extension registration is not yet supported.
struct BuiltinOperationType {
    std::string type,label,target_kind,input,output;
    unsigned version=1;
    bool effects_catalog=false;
    std::map<std::string,double> parameter_defaults;
};
const std::vector<BuiltinOperationType>& builtin_operation_types();
const BuiltinOperationType* builtin_operation_type(const std::string& type);
Ref operation_ref(const Id& object,const Id& operation,const std::string& parameter);
// field is start_x/start_y/end_x/end_y or stop.<stable stop ID>.offset/r/g/b/a.
Ref gradient_ref(const Id& object,const Id& operation,const Id& gradient,const std::string& field);

struct GeometryMask {
    Id id,source;
    unsigned version=1;
    bool enabled=true;
    std::string fill_rule="nonzero";
    std::optional<Ref> enabled_driver;
    bool operator==(const GeometryMask&) const = default;
};
struct Compositing {
    unsigned version=1;
    Scalar opacity{1};
    std::string blend="normal";
    bool isolated=false;
    std::optional<Ref> isolated_driver;
    std::optional<GeometryMask> mask;
    bool operator==(const Compositing&) const = default;
};
struct Object {
    Id id;
    std::string name;
    Kind kind = Kind::path;
    std::vector<Id> children;
    std::vector<Contour> contours;
    std::array<Scalar,6> transform{{{1,{}},{0,{}},{0,{}},{1,{}},{0,{}},{0,{}}}};
    std::vector<ShapeOperation> stack;
    // Compatibility address only: stroke.* resolves to this stable operation.
    // All scalar authority is in stack; this never stores duplicate paint values.
    Id legacy_stroke;
    std::optional<Primitive> source;
    std::optional<PointEdit> point_edit;
    std::optional<TextSource> text;
    // Pivot is authored in object-local coordinates; changing it alone never
    // changes the canonical affine matrix or the rendered placement.
    std::array<Scalar,2> anchor{};
    // Replaces inherited structural transforms; ownership/order stay structural.
    std::optional<Id> transform_parent;
    bool visible=true;
    // Optional same-field link. The literal above remains authored state.
    std::optional<Ref> visibility_driver;
    Compositing compositing;
    std::optional<ImageSource> image;
    bool operator==(const Object&) const = default;
};

struct ArtboardParent {
    Id artboard;
    bool width=true,height=true;
    bool operator==(const ArtboardParent&) const = default;
};
struct Guide {
    Id id;
    std::string name;
    std::string axis="x"; // x is a vertical line, y is horizontal.
    double position=0;
    std::optional<Ref> position_driver;
    bool operator==(const Guide&) const = default;
};
struct LayoutRect {
    double x=0,y=0,width=0,height=0;
    bool operator==(const LayoutRect&) const = default;
};
struct Margin {
    double left=0,top=0,right=0,bottom=0;
    bool operator==(const Margin&) const = default;
};
struct Grid {
    Id id;
    LayoutRect bounds;
    std::size_t columns=1,rows=1;
    double column_gutter=0,row_gutter=0;
    bool operator==(const Grid&) const = default;
};
struct ArtboardLayout {
    std::optional<Margin> margin;
    std::optional<Grid> grid;
    bool operator==(const ArtboardLayout&) const = default;
};
struct ArtboardSizeDriver {
    std::variant<Ref,Expression> value;
    ArtboardSizeDriver(Ref link):value(std::move(link)){}
    ArtboardSizeDriver(Expression expression):value(std::move(expression)){}
    bool operator==(const ArtboardSizeDriver&) const = default;
};
struct Artboard {
    Id id;
    std::string name;
    double x=0,y=0,width=640,height=480;
    std::optional<ArtboardParent> parent_size;
    std::optional<ArtboardLayout> layout;
    using SizeDriver=ArtboardSizeDriver;
    std::optional<SizeDriver> width_driver;
    std::optional<SizeDriver> height_driver;
    bool operator==(const Artboard&) const = default;
};

struct Composition {
    Id id;
    std::string name;
    std::vector<Id> roots;
    std::vector<Artboard> artboards;
    std::vector<Guide> guides;
    bool operator==(const Composition&) const = default;
};

// Resolves only dimensions; frame position, ownership and artwork do not move.
Artboard evaluate_artboard(const Composition& composition,const Id& artboard);

struct Collection {
    Id id;
    std::string name;
    std::vector<Id> members;
    bool operator==(const Collection&) const = default;
};

struct ColorValue {
    std::string space="srgb",profile="srgb",alpha="straight";
    std::array<double,4> rgba{0,0,0,1};
    bool operator==(const ColorValue&) const = default;
};
struct NamedColor {
    Id id;
    std::string name;
    std::array<Scalar,4> rgba{{{0,{}},{0,{}},{0,{}},{1,{}}}};
    bool operator==(const NamedColor&) const = default;
};
struct Document {
    Id id;
    std::vector<Composition> compositions;
    std::map<Id,Object> objects;
    std::vector<Collection> collections;
    std::map<Id,NamedColor> named_colors;
    std::map<Id,RasterAsset> raster_assets;
    bool operator==(const Document&) const = default;
};

// Color properties aggregate ordinary Scalar channels, using the same evaluator.
// Ref.field is color, op.ID.color, or op.ID.gradient.ID.stop.ID.color.
// Named color channel fields are color.r/g/b/a.
std::string property_name(const Document&,const Ref&);
std::vector<Ref> color_properties(const Document&);
std::array<Ref,4> color_channels(const Document&,const Ref&);
ColorValue color_value(const Document&,const Ref&,const std::map<Ref,double>&);
std::optional<Ref> color_link(const Document&,const Ref&);
bool color_is_used(const Document&,const Ref&,const std::map<Ref,bool>* operation_enabled=nullptr,
    const std::map<Ref,bool>* gradient_enabled=nullptr);

struct Set { Ref ref; double value; };
struct Link { Ref target; Binding binding; };
struct Unlink { Ref target; };
struct Rename { Id object; std::string name; };
struct ReorderPoints { Id object; Id contour; std::vector<Id> order; };
struct GroupContiguous { Id composition; Id parent; std::vector<Id> members; Id id; std::string name; };
struct CreateFolder { Id composition; Id parent; Id id; std::string name; };
struct CreatePath { Id composition; Id parent; Id id; std::string name; std::vector<Contour> contours; };
struct AddPoint { Id object; Id contour; Point point; };
struct RemovePoint { Id object; Id contour; Id point; };
struct CloseContour { Id object; Id contour; bool closed; };
struct DeleteObjects { std::vector<Id> objects; };
// Independent copies in place. Prefix (1..48 identifier characters) reserves
// fresh IDs; collisions reject atomically. Selected Group descendants copy once.
struct DuplicateObjects { std::vector<Id> objects; Id prefix; };
// Pure projection of the new top-level selection; uses the command's ID plan.
std::vector<Id> duplicated_roots(const Document&,const DuplicateObjects&);
struct ReorderObjects { Id composition; Id parent; std::vector<Id> order; };
struct CreatePrimitive { Id composition; Id parent; Id id; std::string name; Primitive source; };
struct EnablePointEdit { Id object; bool enabled; };
struct ClearPointEdit { Id object; };
struct LinkPointEditEnabled { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkPointEditEnabled { Ref target; };
struct ConvertToPath { Id object; };
struct AddOperation { Id object; ShapeOperation operation; std::size_t index; };
struct RemoveOperation { Id object; Id operation; };
struct ReorderOperations { Id object; std::vector<Id> order; };
struct EnableOperation { Id object; Id operation; bool enabled; };
struct LinkOperationEnabled { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkOperationEnabled { Ref target; };
struct LinkGradientEnabled { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkGradientEnabled { Ref target; };
struct LinkMaskEnabled { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkMaskEnabled { Ref target; };
struct StrokeStyle { Id object,operation; std::string line_cap="butt",line_join="miter"; double miter_limit=4; };
struct OperationOptions { Id object; Id operation; std::string composite; std::string fill_rule; std::optional<std::string> line_join; };
struct LinkFillRule { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkFillRule { Ref target; };
struct SetGradient { Id object; Id operation; std::optional<Gradient> gradient; };

struct AddArtboard { Id composition; Artboard artboard; std::size_t index; };
struct UpdateArtboard { Id composition; Artboard artboard; };
struct DeleteArtboard { Id composition; Id artboard; };
struct ReorderArtboards { Id composition; std::vector<Id> order; };
struct DetachArtboardParent { Id composition; Id artboard; };
struct LinkArtboardSize { Ref target; Ref source; bool replace_driver=false; };
struct SetArtboardSizeExpression { Ref target; Expression expression; bool replace_driver=false; };
struct UnlinkArtboardSize { Ref target; };
struct AddGuide { Id composition; Guide guide; };
struct UpdateGuide { Id composition; Guide guide; };
struct DeleteGuide { Id composition; Id guide_id; };
struct LinkGuidePosition { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkGuidePosition { Ref target; };
struct SetArtboardLayout { Id composition; Id artboard_id; std::optional<ArtboardLayout> layout; };
struct AddRasterAsset { RasterAsset asset; };
struct ReplaceRasterAsset { RasterAsset asset; };
struct DeleteRasterAsset { Id asset; };
struct CreateImage { Id composition,parent,id; std::string name; ImageSource source; };
struct CreateText { Id composition; Id parent; Id id; std::string name; TextSource source; };
struct UpdateText { Id object; TextSource source; };
struct LinkTextItalic { Ref target; Ref source; bool replace_driver=false; };
struct SetTextItalicExpression { Ref target; Expression expression; bool replace_driver=false; };
struct UnlinkTextItalic { Ref target; };
struct LinkTextWeight { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkTextWeight { Ref target; };
struct LinkTextContent { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkTextContent { Ref target; };
struct LinkTextFamily { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkTextFamily { Ref target; };
struct LinkTextLocale { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkTextLocale { Ref target; };
struct LinkTextDirection { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkTextDirection { Ref target; };
struct LinkTextLayout { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkTextLayout { Ref target; };
struct LinkTextAlignment { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkTextAlignment { Ref target; };
struct CreateNamedColor { NamedColor color; };
struct RenameNamedColor { Id color; std::string name; };
struct DeleteNamedColor { Id color; };
struct SetColor { Ref ref; ColorValue value; };
struct LinkColor { Ref target; Ref source; };
struct UnlinkColor { Ref ref; };
struct CenterAnchor { Id object; };
struct SetPosition { Id object; double x,y; };
struct TransformAroundAnchor { Id object; double rotation=0,scale_x=1,scale_y=1; };
struct SetTransformParent { Id object; std::optional<Id> parent; bool preserve_world=true; };
// Explicit scalar batches capture one evaluated starting snapshot. Target lists
// contain 1..1000 unique scalar properties (including legacy alias identity).
struct EditProperties { std::vector<Ref> targets; double value; bool relative=false; };
struct LinkProperties { std::vector<Ref> targets; Ref source; bool relative=false; };
struct UnlinkProperties { std::vector<Ref> targets; };
struct SetExpression { std::vector<Ref> targets; Expression expression; bool replace_binding=false; };
struct SetVisibility { Id object; bool visible; };
struct LinkObjectVisibility { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkObjectVisibility { Ref target; };
struct LinkCompositeIsolated { Ref target; Ref source; bool replace_driver=false; };
struct UnlinkCompositeIsolated { Ref target; };
struct SetCompositing { Id object; std::string blend; bool isolated; };
struct SetMask { Id object; std::optional<GeometryMask> mask; };
struct MaskObjects { Id composition,parent; std::vector<Id> members; Id id,mask_id; std::string name; bool top=true; };
struct PutInside { Id composition,parent,group; std::vector<Id> members; };
struct Ungroup { Id composition,parent,group; };
// Move an ordered prefix or suffix of a Group's children beside the retained Group.
struct MoveOut { Id composition,parent,group; std::vector<Id> members; std::string placement; };
// World-space displacement, applied once per selected object across Structure
// and Transform Parent relationships. Selection is one Composition, 1..1000 IDs.
struct TranslateObjects { std::vector<Id> objects; double dx,dy; };
// One common Composition-space pivot; null uses evaluated geometric bounds center.
// Scale along world axes, then rotate clockwise in Y-down coordinates.
struct TransformObjects {
    std::vector<Id> objects; double rotation=0,scale_x=1,scale_y=1;
    std::optional<std::array<double,2>> pivot;
};
// One-shot layout from evaluated geometry, excluding stroke width. The legacy
// artboard field remains a source-compatible alias for an explicit reference.
struct DistributeObjects {
    std::vector<Id> objects;
    std::string axis;
    std::string reference="selection";
    std::optional<double> spacing;
};
struct AlignObjects {
    std::vector<Id> objects;
    std::string axis,alignment;
    // Retained as a source-compatible C++ alias. JSON callers should use reference.
    std::optional<Id> artboard;
    std::string reference="selection";
};

using Command = std::variant<Set,Link,Unlink,Rename,ReorderPoints,GroupContiguous,
    CreateFolder,CreatePath,AddPoint,RemovePoint,CloseContour,DeleteObjects,ReorderObjects,
    CreatePrimitive,EnablePointEdit,ClearPointEdit,LinkPointEditEnabled,UnlinkPointEditEnabled,ConvertToPath,AddOperation,RemoveOperation,
    ReorderOperations,EnableOperation,LinkOperationEnabled,UnlinkOperationEnabled,LinkGradientEnabled,UnlinkGradientEnabled,OperationOptions,LinkFillRule,UnlinkFillRule,StrokeStyle,SetGradient,AddArtboard,UpdateArtboard,
    DeleteArtboard,ReorderArtboards,DetachArtboardParent,LinkArtboardSize,SetArtboardSizeExpression,UnlinkArtboardSize,AddGuide,UpdateGuide,DeleteGuide,LinkGuidePosition,UnlinkGuidePosition,SetArtboardLayout,CreateText,UpdateText,
    CreateNamedColor,RenameNamedColor,DeleteNamedColor,SetColor,LinkColor,UnlinkColor,
    LinkTextItalic,SetTextItalicExpression,UnlinkTextItalic,LinkTextWeight,UnlinkTextWeight,LinkTextContent,UnlinkTextContent,LinkTextFamily,UnlinkTextFamily,LinkTextLocale,UnlinkTextLocale,LinkTextDirection,UnlinkTextDirection,LinkTextLayout,UnlinkTextLayout,LinkTextAlignment,UnlinkTextAlignment,
    CenterAnchor,SetPosition,TransformAroundAnchor,SetTransformParent,
    EditProperties,LinkProperties,UnlinkProperties,TranslateObjects,TransformObjects,SetExpression,
    SetVisibility,LinkObjectVisibility,UnlinkObjectVisibility,LinkCompositeIsolated,UnlinkCompositeIsolated,
    LinkMaskEnabled,UnlinkMaskEnabled,
    SetCompositing,SetMask,MaskObjects,PutInside,Ungroup,MoveOut,
    AddRasterAsset,ReplaceRasterAsset,DeleteRasterAsset,CreateImage,DuplicateObjects,AlignObjects,DistributeObjects>;

using Affine=std::array<double,6>;
inline constexpr Affine identity_matrix{1,0,0,1,0,0};
struct Vec2 {double x=0,y=0;};
struct CubicPoint {Vec2 anchor,incoming,outgoing;};
struct EvaluatedContour {bool closed=false;std::vector<CubicPoint> points;};
struct PathCubicSegment {
    Vec2 p0,p1,p2,p3;
    double start_distance=0,end_distance=0;
};
struct PathSampleSegment {
    std::size_t cubic=0;
    double t_start=0,t_end=1;
    double start_distance=0,end_distance=0;
};
struct PathSampler {
    Id path,contour;
    bool closed=false;
    double length=0;
    std::vector<PathCubicSegment> cubics;
    std::vector<PathSampleSegment> segments;
};
struct PathSample {
    Vec2 position,tangent,normal;
    double distance=0,length=0;
    Id path,contour;
};
PathSampler build_path_sampler(const Document&,const Id& path,const Id& contour,const std::map<Ref,double>& values);
PathSample sample_path(const PathSampler&,double distance,bool reversed=false);
struct EvaluatedTextGlyph {
    double advance=0;
    Vec2 baseline;
    std::vector<EvaluatedContour> contours;
};
struct TextLayout {
    std::shared_ptr<const std::vector<EvaluatedContour>> contours;
    double x=0,y=0,width=0,height=0;
    // Derived layout metric in the same local coordinates as the glyph outlines.
    // Available only for horizontal text with a measurable first line.
    std::optional<double> first_line_baseline_y;
    // Measured horizontal line baselines in layout order; empty for vertical text.
    std::vector<double> line_baselines_y;
    // Measured vertical column baseline origins in layout order. Empty if a
    // column has no run or its DirectWrite run origins disagree.
    std::vector<double> column_baselines_x;
    bool overflow=false;
    std::size_t glyph_count=0;
    // Present only for attached Text. Each entry is one DirectWrite shaped glyph,
    // including whitespace advances and an intact, baseline-relative outline.
    std::vector<EvaluatedTextGlyph> glyphs;
    std::vector<std::string> warnings,used_fonts;
};
// Pure projection of authored text and evaluated text.* parameters. Windows uses
// DirectWrite shaping, including vertical glyph orientation; no font is embedded.
TextLayout evaluate_text(const TextSource& source,const std::map<std::string,double>& parameters);
// Document-aware projection used by Canvas, bounds, SVG and API. Attached Text
// requires its stable Path/Contour reference and is never returned as flat text.
TextLayout evaluate_text_projection(const Document&,const Id&,const std::map<Ref,double>& values);
// Pure projection of an authored Text source with its current evaluated typed values.
TextSource evaluated_text_source(const Document& document,const Id& object);
struct PathInstance {
    std::shared_ptr<const std::vector<EvaluatedContour>> contours;
    Affine transform=identity_matrix;
};
struct EvaluatedGradientStop {double offset=0;std::array<double,4> rgba{};};
struct EvaluatedGradient {
    std::string type;
    Vec2 start,end;
    std::vector<EvaluatedGradientStop> stops;
};
struct PaintLayer {
    Id operation;
    std::string type;
    std::array<double,4> rgba{};
    double width=0;
    std::string line_cap="butt",line_join="miter";
    double miter_limit=4;
    // Zero-length stroked subpaths (at least one segment), in paint coordinates.
    // Separate from ordinary paths because some renderers discard those segments.
    std::vector<Vec2> degenerate_subpaths;
    std::string fill_rule="nonzero";
    Affine transform=identity_matrix;
    std::vector<PathInstance> paths;
    std::optional<EvaluatedGradient> gradient;
};
struct EvaluatedShape {std::vector<PathInstance> paths;std::vector<PaintLayer> paints;};
// Matrix composition is outer(inner(point)); independent of renderer convention.
Affine compose(const Affine& outer,const Affine& inner);
Vec2 map_point(const Affine& matrix,Vec2 point);
Affine inverse_affine(const Affine& matrix);
struct EvaluatedTransform {
    Affine local=identity_matrix,world=identity_matrix;
    Id effective_parent; // Empty denotes the Composition's coordinate plane.
};
std::map<Id,EvaluatedTransform> evaluate_transforms(const Document&,const std::map<Ref,double>&);
struct Bounds {double left=0,top=0,right=0,bottom=0;};
// Geometric bounds in the target object's local coordinates, including cubic
// extrema, Shape instances/paints and Text layout; excludes stroke thickness.
// world_space=true computes extrema directly in the Composition plane.
std::optional<Bounds> object_bounds(const Document&,const Id&,const std::map<Ref,double>&,
    const std::map<Id,EvaluatedTransform>&,bool world_space=false);
EvaluatedShape evaluate_shape(const Document&,const Id&,const std::map<Ref,double>&,
    const std::map<Ref,std::string>* fill_rules=nullptr,
    const std::map<Ref,bool>* operation_enabled=nullptr,
    const std::map<Ref,bool>* gradient_enabled=nullptr);
struct EvaluatedMask {
    Id source;
    std::string fill_rule;
    std::vector<PathInstance> paths; // transforms already map to Composition/world
};
struct EvaluatedSceneNode {
    Id id;
    Affine world=identity_matrix;
    double opacity=1;
    std::string blend="normal";
    bool isolated=false,visible=true; // isolated is the resolved aggregate requirement
    std::optional<EvaluatedMask> mask;
    std::vector<unsigned> posterize_levels; // ordered Group postchildren pixel operations
    std::vector<EvaluatedSceneNode> children;
};
struct EvaluatedImage { Raster payload; double width=0,height=0; };
struct EvaluatedScene {
    std::vector<EvaluatedSceneNode> roots;
    std::map<Id,EvaluatedShape> shapes;
    std::map<Id,EvaluatedImage> images;
    bool requires_compositing=false;
};
EvaluatedScene evaluate_scene(const Document&,const Id& composition,const std::map<Ref,double>&,
    const std::map<Id,EvaluatedTransform>&);
// Used by creation and legacy readers; creates one real stack operation.
void add_default_stroke(Document&,const Id& object);

std::vector<Ref> properties(const Document& document);
Scalar property(const Document& document, const Ref& ref);
struct FillRuleProperty {
    std::string literal;
    std::optional<FillRuleDriver> driver;
    std::string evaluated;
};
FillRuleProperty fill_rule_property(const Document&,const Ref&);
std::string evaluate_fill_rule(const Document&,const Ref&);
std::map<Ref,std::string> evaluate_fill_rules(const Document&);
bool gradient_enabled_property(const Document&,const Ref&);
struct GradientEnabledProperty {
    bool literal=true;
    std::optional<Ref> driver;
    bool evaluated=true;
};
GradientEnabledProperty gradient_enabled_state(const Document&,const Ref&);
std::map<Ref,GradientEnabledProperty> gradient_enabled_states(const Document&);
bool evaluate_gradient_enabled(const Document&,const Ref&);
std::map<Ref,bool> evaluate_gradient_enableds(const Document&);
bool operation_enabled_property(const Document&,const Ref&);
struct OperationEnabledProperty {
    bool literal=true;
    std::optional<Ref> driver;
    bool evaluated=true;
};
OperationEnabledProperty operation_enabled_state(const Document&,const Ref&);
bool evaluate_operation_enabled(const Document&,const Ref&);
std::map<Ref,bool> evaluate_operation_enableds(const Document&);
bool object_visibility_property(const Document&,const Ref&);
struct ObjectVisibilityProperty {
    bool literal=true;
    std::optional<Ref> driver;
    bool evaluated=true;
};
ObjectVisibilityProperty object_visibility_state(const Document&,const Ref&);
bool evaluate_object_visibility(const Document&,const Id& object);
std::map<Id,bool> evaluate_object_visibilities(const Document&);
bool composite_isolated_property(const Document&,const Ref&);
struct CompositeIsolationProperty {
    bool literal=false;
    std::optional<Ref> driver;
    bool evaluated=false;
};
CompositeIsolationProperty composite_isolation_state(const Document&,const Ref&);
bool evaluate_composite_isolation(const Document&,const Ref&);
std::map<Id,bool> evaluate_composite_isolations(const Document&);
bool geometry_mask_enabled_property(const Document&,const Ref&);
Ref geometry_mask_enabled_ref(const Id& object,const Id& mask);
struct GeometryMaskEnabledProperty {
    bool literal=true;
    std::optional<Ref> driver;
    bool evaluated=true;
};
GeometryMaskEnabledProperty geometry_mask_enabled_state(const Document&,const Ref&);
bool evaluate_geometry_mask_enabled(const Document&,const Ref&);
std::map<Ref,bool> evaluate_geometry_mask_enableds(const Document&);
bool point_edit_enabled_property(const Document&,const Ref&);
Ref point_edit_enabled_ref(const Id& object,const Id& point_edit);
struct PointEditEnabledProperty {
    bool literal=true;
    std::optional<Ref> driver;
    bool evaluated=true;
};
PointEditEnabledProperty point_edit_enabled_state(const Document&,const Ref&);
bool evaluate_point_edit_enabled(const Document&,const Ref&);
std::map<Ref,bool> evaluate_point_edit_enableds(const Document&);
struct GuidePositionProperty {
    double literal=0;
    std::optional<Ref> driver;
    double evaluated=0;
};
GuidePositionProperty guide_position_property(const Document&,const Ref&);
double evaluate_guide_position(const Document&,const Id& composition,const Id& guide);
std::map<Id,double> evaluate_guide_positions(const Document&,const Id& composition);
struct ArtboardSizeProperty {
    double literal=0;
    std::optional<Ref> driver;
    std::optional<Expression> expression;
    std::string source_kind="literal";
    double evaluated=0;
};
ArtboardSizeProperty artboard_size_property(const Document&,const Ref&);
enum class TextPropertyKind { string, enumeration };
struct TextPropertyValue {
    TextPropertyKind kind=TextPropertyKind::string;
    std::string literal;
    std::vector<std::string> choices;
    bool operator==(const TextPropertyValue&) const = default;
};
bool is_text_readonly_field(const std::string& field);
TextPropertyValue text_readonly_property(const Document&,const Ref&);
struct TextItalicProperty {
    bool literal=false;
    std::optional<TextItalicDriver> driver;
    bool evaluated=false;
};
struct TextWeightProperty {
    unsigned literal=400;
    std::optional<TextWeightDriver> driver;
    unsigned evaluated=400;
};
TextItalicProperty text_italic_property(const Document&,const Ref&);
bool evaluate_text_italic(const Document&,const Id& object);
std::map<Ref,bool> evaluate_text_italics(const Document&);
TextWeightProperty text_weight_property(const Document&,const Ref&);
unsigned evaluate_text_weight(const Document&,const Id& object);
std::map<Ref,unsigned> evaluate_text_weights(const Document&);
struct TextContentProperty {
    std::string literal;
    std::optional<TextContentDriver> driver;
    std::string evaluated;
};
struct TextFamilyProperty {
    std::string literal;
    std::optional<TextFamilyDriver> driver;
    std::string evaluated;
};
struct TextLocaleProperty {
    std::string literal;
    std::optional<TextLocaleDriver> driver;
    std::string evaluated;
};
struct TextDirectionProperty {
    std::string literal;
    std::optional<TextDirectionDriver> driver;
    std::string evaluated;
};
struct TextLayoutProperty {
    std::string literal;
    std::optional<TextLayoutDriver> driver;
    std::string evaluated;
};
struct TextAlignmentProperty {
    std::string literal;
    std::optional<TextAlignmentDriver> driver;
    std::string evaluated;
};
TextContentProperty text_content_property(const Document&,const Ref&);
std::string evaluate_text_content(const Document&,const Id& object);
std::map<Ref,std::string> evaluate_text_contents(const Document&);
TextFamilyProperty text_family_property(const Document&,const Ref&);
std::string evaluate_text_family(const Document&,const Id& object);
std::map<Ref,std::string> evaluate_text_families(const Document&);
TextLocaleProperty text_locale_property(const Document&,const Ref&);
std::string evaluate_text_locale(const Document&,const Id& object);
std::map<Ref,std::string> evaluate_text_locales(const Document&);
TextDirectionProperty text_direction_property(const Document&,const Ref&);
std::string evaluate_text_direction(const Document&,const Id& object);
std::map<Ref,std::string> evaluate_text_directions(const Document&);
TextLayoutProperty text_layout_property(const Document&,const Ref&);
std::string evaluate_text_layout(const Document&,const Id& object);
std::map<Ref,std::string> evaluate_text_layouts(const Document&);
TextAlignmentProperty text_alignment_property(const Document&,const Ref&);
std::string evaluate_text_alignment(const Document&,const Id& object);
std::map<Ref,std::string> evaluate_text_alignments(const Document&);
std::string property_origin(const Document& document, const Ref& ref);
// Returns contour topology/IDs. All resolved coordinates, including generated
// points, come from evaluate(); this is never another authored geometry store.
// A bound dynamic point count requires the caller's evaluated snapshot.
std::vector<Contour> path_contours(const Object& object,const std::map<Ref,double>* values=nullptr);
std::vector<Ref> conversion_blockers(const Document& document, const Id& object);
Ref resolve_name(const Document& document, const std::string& name, const Id& point, const std::string& field);
std::map<Ref,double> evaluate(const Document& document);
void validate(const Document& document);
Document demo_document();
Document empty_document(Id document, Id composition, Id artboard);
std::string property_unit(const Ref& ref);
std::vector<Ref> expression_dependencies(const Expression& expression);

struct HistoryLimits {
    std::size_t max_entries=1024;
    std::size_t max_bytes=64*1024*1024;
};
struct HistoryState {
    std::uint64_t id=0;
    std::string label;
    std::size_t estimated_bytes=0;
    bool operator==(const HistoryState&) const = default;
};
struct HistoryInfo {
    // The first row is the earliest retained boundary, followed by edit states.
    std::vector<HistoryState> states;
    std::uint64_t current_id=0;
    std::size_t retained_bytes=0,max_entries=0,max_bytes=0,pruned_entries=0;
    bool operator==(const HistoryInfo&) const = default;
};

class Session {
public:
    explicit Session(Document document,HistoryLimits limits={});
    const Document& document() const { return document_; }
    std::uint64_t revision() const { return revision_; }
    void apply(const std::vector<Command>& commands, std::uint64_t expected_revision);
    void undo(std::uint64_t expected_revision);
    void redo(std::uint64_t expected_revision);
    bool can_undo() const { return history_cursor_>0; }
    bool can_redo() const { return history_cursor_<history_.size(); }
    HistoryInfo history() const;
    void restore_history(std::uint64_t state_id,std::uint64_t expected_revision);
    // A gesture previews commands against its starting snapshot. Committed reads
    // remain stable; other mutations are rejected until commit or cancellation.
    void begin_gesture(std::uint64_t expected_revision);
    void update_gesture(const std::vector<Command>& commands);
    void commit_gesture();
    void cancel_gesture();
    bool gesture_active() const { return preview_.has_value(); }
    const Document& preview_document() const { return preview_ ? *preview_ : document_; }
    // Derived values from the last successful preview validation. Null before an
    // update or after an empty/reset/commit/cancel. Invalidated by gesture changes.
    const std::map<Ref,double>* preview_values() const { return preview_values_ ? &*preview_values_ : nullptr; }
private:
    template<class T> struct HistoryChange {
        Id key;
        std::optional<T> before,after;
    };
    struct HistoryEntry {
        std::uint64_t id=0;
        std::string label;
        std::size_t estimated_bytes=0;
        std::vector<HistoryChange<Object>> objects;
        std::vector<HistoryChange<NamedColor>> colors;
        std::vector<HistoryChange<RasterAsset>> assets;
        std::optional<std::pair<std::vector<Composition>,std::vector<Composition>>> compositions;
        std::optional<std::pair<std::vector<Collection>,std::vector<Collection>>> collections;
    };
    Document document_;
    std::uint64_t revision_ = 0;
    HistoryLimits history_limits_;
    std::list<HistoryEntry> history_;
    std::size_t history_cursor_=0,history_bytes_=0,pruned_entries_=0;
    std::uint64_t boundary_id_=0,next_history_id_=1;
    std::optional<Document> preview_;
    std::optional<std::map<Ref,double>> preview_values_;
    bool preview_changed_ = false;
    std::string preview_label_;
    void check_revision(std::uint64_t expected) const;
    void commit(Document candidate,std::string label);
    std::string history_label(const std::vector<Command>& commands,const Document& candidate) const;
    static std::size_t estimate_history(const HistoryEntry& entry);
    static void apply_history(Document& candidate,const HistoryEntry& entry,bool forward);
};
}
