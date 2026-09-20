#pragma once
#include <array>
#include <cstdint>
#include <compare>
#include <utility>
#include <map>
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
};

struct Scalar {
    double literal = 0;
    std::optional<Binding> binding;
};

struct Point {
    Id id;
    Scalar x, y, in_angle, in_length, out_angle, out_length;
};

struct Contour {
    Id id;
    bool closed = false;
    std::vector<Point> points;
};

enum class Kind { group, path };

struct Primitive {
    Id id;
    std::string type; // nect.shape.circle / nect.shape.rectangle
    unsigned version = 1;
    std::map<std::string,Scalar> parameters;
};

struct PointEdit {
    Id id;
    unsigned version = 1;
    bool enabled = true;
    // Absolute, local-space scalar overrides of stable generated point fields.
    // Fields not present continue to evaluate from the source generator.
    std::map<Id,std::map<std::string,Scalar>> overrides;
};

struct Object {
    Id id;
    std::string name;
    Kind kind = Kind::path;
    std::vector<Id> children;
    std::vector<Contour> contours;
    std::array<Scalar,6> transform{{{1,{}},{0,{}},{0,{}},{1,{}},{0,{}},{0,{}}}};
    std::array<Scalar,4> color{{{0,{}},{0,{}},{0,{}},{1,{}}}};
    Scalar stroke_width{2,{}};
    std::optional<Primitive> source;
    std::optional<PointEdit> point_edit;
};

struct Artboard {
    Id id;
    std::string name;
    double x=0,y=0,width=640,height=480;
};

struct Composition {
    Id id;
    std::string name;
    std::vector<Id> roots;
    std::vector<Artboard> artboards;
};

struct Collection {
    Id id;
    std::string name;
    std::vector<Id> members;
};

struct Document {
    Id id;
    std::vector<Composition> compositions;
    std::map<Id,Object> objects;
    std::vector<Collection> collections;
};

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
struct ReorderObjects { Id composition; Id parent; std::vector<Id> order; };
struct CreatePrimitive { Id composition; Id parent; Id id; std::string name; Primitive source; };
struct EnablePointEdit { Id object; bool enabled; };
struct ConvertToPath { Id object; };

using Command = std::variant<Set,Link,Unlink,Rename,ReorderPoints,GroupContiguous,
    CreatePath,AddPoint,RemovePoint,CloseContour,DeleteObjects,ReorderObjects,
    CreatePrimitive,EnablePointEdit,ConvertToPath>;

std::vector<Ref> properties(const Document& document);
Scalar property(const Document& document, const Ref& ref);
std::string property_origin(const Document& document, const Ref& ref);
// Returns contour topology/IDs. All resolved coordinates, including generated
// points, come from evaluate(); this is never another authored geometry store.
std::vector<Contour> path_contours(const Object& object);
std::vector<Ref> conversion_blockers(const Document& document, const Id& object);
Ref resolve_name(const Document& document, const std::string& name, const Id& point, const std::string& field);
std::map<Ref,double> evaluate(const Document& document);
void validate(const Document& document);
Document demo_document();
Document empty_document(Id document, Id composition, Id artboard);
std::string property_unit(const Ref& ref);

class Session {
public:
    explicit Session(Document document);
    const Document& document() const { return document_; }
    std::uint64_t revision() const { return revision_; }
    void apply(const std::vector<Command>& commands, std::uint64_t expected_revision);
    void undo(std::uint64_t expected_revision);
    void redo(std::uint64_t expected_revision);
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    // A gesture previews commands against its starting snapshot. Committed reads
    // remain stable; other mutations are rejected until commit or cancellation.
    void begin_gesture(std::uint64_t expected_revision);
    void update_gesture(const std::vector<Command>& commands);
    void commit_gesture();
    void cancel_gesture();
    bool gesture_active() const { return preview_.has_value(); }
    const Document& preview_document() const { return preview_ ? *preview_ : document_; }
private:
    Document document_;
    std::uint64_t revision_ = 0;
    std::vector<Document> undo_, redo_;
    std::optional<Document> preview_;
    bool preview_changed_ = false;
    void check_revision(std::uint64_t expected) const;
    void commit(Document candidate);
};
}
