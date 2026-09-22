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
    std::string layout="auto",direction="horizontal",alignment="start";
    unsigned weight=400;
    bool italic=false;
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
    bool operator==(const Gradient&) const = default;
};
struct ShapeOperation {
    Id id;
    std::string type; // nect.paint.fill / nect.paint.stroke / nect.shape.repeater / nect.shape.offset
    unsigned version=1;
    bool enabled=true;
    std::map<std::string,Scalar> parameters;
    std::string composite="below";
    std::string fill_rule="nonzero";
    std::optional<Gradient> gradient;
    std::string line_join="miter"; // Offset only: miter (bevel fallback), round, bevel.
    bool operator==(const ShapeOperation&) const = default;
};
ShapeOperation default_operation(Id id,const std::string& type);
Ref operation_ref(const Id& object,const Id& operation,const std::string& parameter);
// field is start_x/start_y/end_x/end_y or stop.<stable stop ID>.offset/r/g/b/a.
Ref gradient_ref(const Id& object,const Id& operation,const Id& gradient,const std::string& field);

struct GeometryMask {
    Id id,source;
    unsigned version=1;
    bool enabled=true;
    std::string fill_rule="nonzero";
    bool operator==(const GeometryMask&) const = default;
};
struct Compositing {
    unsigned version=1;
    Scalar opacity{1};
    std::string blend="normal";
    bool isolated=false;
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
    Compositing compositing;
    std::optional<ImageSource> image;
    bool operator==(const Object&) const = default;
};

struct ArtboardParent {
    Id artboard;
    bool width=true,height=true;
    bool operator==(const ArtboardParent&) const = default;
};
struct Artboard {
    Id id;
    std::string name;
    double x=0,y=0,width=640,height=480;
    std::optional<ArtboardParent> parent_size;
    bool operator==(const Artboard&) const = default;
};

struct Composition {
    Id id;
    std::string name;
    std::vector<Id> roots;
    std::vector<Artboard> artboards;
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
bool color_is_used(const Document&,const Ref&);

struct Set { Ref ref; double value; };
struct Link { Ref target; Binding binding; };
struct Unlink { Ref target; };
struct Rename { Id object; std::string name; };
struct ReorderPoints { Id object; Id contour; std::vector<Id> order; };
struct GroupContiguous { Id composition; Id parent; std::vector<Id> members; Id id; std::string name; };
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
struct ConvertToPath { Id object; };
struct AddOperation { Id object; ShapeOperation operation; std::size_t index; };
struct RemoveOperation { Id object; Id operation; };
struct ReorderOperations { Id object; std::vector<Id> order; };
struct EnableOperation { Id object; Id operation; bool enabled; };
struct OperationOptions { Id object; Id operation; std::string composite; std::string fill_rule; std::optional<std::string> line_join; };
struct SetGradient { Id object; Id operation; std::optional<Gradient> gradient; };

struct AddArtboard { Id composition; Artboard artboard; std::size_t index; };
struct UpdateArtboard { Id composition; Artboard artboard; };
struct DeleteArtboard { Id composition; Id artboard; };
struct ReorderArtboards { Id composition; std::vector<Id> order; };
struct DetachArtboardParent { Id composition; Id artboard; };
struct AddRasterAsset { RasterAsset asset; };
struct ReplaceRasterAsset { RasterAsset asset; };
struct DeleteRasterAsset { Id asset; };
struct CreateImage { Id composition,parent,id; std::string name; ImageSource source; };
struct CreateText { Id composition; Id parent; Id id; std::string name; TextSource source; };
struct UpdateText { Id object; TextSource source; };
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
struct SetCompositing { Id object; std::string blend; bool isolated; };
struct SetMask { Id object; std::optional<GeometryMask> mask; };
struct MaskObjects { Id composition,parent; std::vector<Id> members; Id id,mask_id; std::string name; bool top=true; };
struct PutInside { Id composition,parent,group; std::vector<Id> members; };
// World-space displacement, applied once per selected object across Structure
// and Transform Parent relationships. Selection is one Composition, 1..1000 IDs.
struct TranslateObjects { std::vector<Id> objects; double dx,dy; };

using Command = std::variant<Set,Link,Unlink,Rename,ReorderPoints,GroupContiguous,
    CreatePath,AddPoint,RemovePoint,CloseContour,DeleteObjects,ReorderObjects,
    CreatePrimitive,EnablePointEdit,ClearPointEdit,ConvertToPath,AddOperation,RemoveOperation,
    ReorderOperations,EnableOperation,OperationOptions,SetGradient,AddArtboard,UpdateArtboard,
    DeleteArtboard,ReorderArtboards,DetachArtboardParent,CreateText,UpdateText,
    CreateNamedColor,RenameNamedColor,DeleteNamedColor,SetColor,LinkColor,UnlinkColor,
    CenterAnchor,SetPosition,TransformAroundAnchor,SetTransformParent,
    EditProperties,LinkProperties,UnlinkProperties,TranslateObjects,SetExpression,
    SetVisibility,SetCompositing,SetMask,MaskObjects,PutInside,
    AddRasterAsset,ReplaceRasterAsset,DeleteRasterAsset,CreateImage,DuplicateObjects>;

using Affine=std::array<double,6>;
inline constexpr Affine identity_matrix{1,0,0,1,0,0};
struct Vec2 {double x=0,y=0;};
struct CubicPoint {Vec2 anchor,incoming,outgoing;};
struct EvaluatedContour {bool closed=false;std::vector<CubicPoint> points;};
struct TextLayout {
    std::shared_ptr<const std::vector<EvaluatedContour>> contours;
    double x=0,y=0,width=0,height=0;
    bool overflow=false;
    std::size_t glyph_count=0;
    std::vector<std::string> warnings,used_fonts;
};
// Pure projection of authored text and evaluated text.* parameters. Windows uses
// DirectWrite shaping, including vertical glyph orientation; no font is embedded.
TextLayout evaluate_text(const TextSource& source,const std::map<std::string,double>& parameters);
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
std::optional<Bounds> object_bounds(const Document&,const Id&,const std::map<Ref,double>&,
    const std::map<Id,EvaluatedTransform>&);
EvaluatedShape evaluate_shape(const Document&,const Id&,const std::map<Ref,double>&);
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
    bool preview_changed_ = false;
    std::string preview_label_;
    void check_revision(std::uint64_t expected) const;
    void commit(Document candidate,std::string label);
    std::string history_label(const std::vector<Command>& commands,const Document& candidate) const;
    static std::size_t estimate_history(const HistoryEntry& entry);
    static void apply_history(Document& candidate,const HistoryEntry& entry,bool forward);
};
}
