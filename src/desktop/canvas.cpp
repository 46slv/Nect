#include "canvas.hpp"

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QImage>
#include <QPainterPathStroker>
#include <QShowEvent>
#include <QStringList>
#include <QUuid>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <set>

namespace nect::desktop {
namespace {
const QColor accent(68, 198, 233);
const QColor linked(184, 142, 232);
constexpr double hit_radius = 8;

double distance(QPointF a, QPointF b) {
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

QPointF handle(QPointF anchor, double angle, double length) {
    const auto radians = angle * std::numbers::pi / 180.0;
    return anchor + QPointF(std::cos(radians) * length, std::sin(radians) * length);
}

Id unique_id(const char* prefix) {
    return std::string(prefix) + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

QTransform qt_transform(const Affine& matrix) {
    return {matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]};
}
QPainterPath qt_path(const std::vector<EvaluatedContour>& contours,bool close_open=false) {
    QPainterPath result;
    const auto point = [](Vec2 p) { return QPointF(p.x, p.y); };
    for (const auto& contour : contours) {
        if (contour.points.empty()) continue;
        result.moveTo(point(contour.points.front().anchor));
        for (std::size_t i = 1; i < contour.points.size(); ++i)
            result.cubicTo(point(contour.points[i - 1].outgoing), point(contour.points[i].incoming),
                           point(contour.points[i].anchor));
        if (contour.closed) {
            result.cubicTo(point(contour.points.back().outgoing), point(contour.points.front().incoming),
                           point(contour.points.front().anchor));
        }
        if(contour.closed||close_open)result.closeSubpath();
    }
    return result;
}
QPainter::CompositionMode blend_mode(const std::string& blend) {
    static const std::map<std::string,QPainter::CompositionMode> modes{
        {"normal",QPainter::CompositionMode_SourceOver},{"multiply",QPainter::CompositionMode_Multiply},
        {"screen",QPainter::CompositionMode_Screen},{"overlay",QPainter::CompositionMode_Overlay},
        {"darken",QPainter::CompositionMode_Darken},{"lighten",QPainter::CompositionMode_Lighten},
        {"color-dodge",QPainter::CompositionMode_ColorDodge},{"color-burn",QPainter::CompositionMode_ColorBurn},
        {"hard-light",QPainter::CompositionMode_HardLight},{"soft-light",QPainter::CompositionMode_SoftLight},
        {"difference",QPainter::CompositionMode_Difference},{"exclusion",QPainter::CompositionMode_Exclusion}};
    const auto found=modes.find(blend);if(found==modes.end())throw Error("UNSUPPORTED_BLEND",blend);return found->second;
}
} // namespace

Canvas::Canvas(Session& session, QWidget* parent) : QWidget(parent), session_(session) {
    setMinimumSize(640, 480);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAccessibleName(tr("Canvas"));
    setAccessibleDescription(tr("Select and drag paths, points and handles. Alt-drag a point to create handles. "
                                "Middle-drag or Space-drag to pan; wheel to zoom; F to fit."));
    clock_.start();
    refresh();
}

Canvas::~Canvas() {
    if (gesture_owned_ && session_.gesture_active()) session_.cancel_gesture();
}

QTransform Canvas::view() const {
    return {zoom_, 0, 0, zoom_, pan_.x(), pan_.y()};
}

const Canvas::Geometry* Canvas::geometry(const Id& id) const {
    const auto found=geometry_index_.find(id);return found==geometry_index_.end()?nullptr:&geometry_.at(found->second);
}

const Canvas::EvaluatedPoint* Canvas::point(const Geometry& item, const Id& id) const {
    const auto found = std::find_if(item.points.begin(), item.points.end(),
                                  [&](const auto& item_point) { return item_point.id == id; });
    return found == item.points.end() ? nullptr : &*found;
}

void Canvas::refresh() {
    const auto& document = session_.preview_document();
    const auto previous_composition = active_composition_, previous_artboard = active_artboard_;
    try {
        auto composition = std::find_if(document.compositions.begin(), document.compositions.end(),
            [&](const auto& item) { return item.id == active_composition_; });
        if (composition == document.compositions.end() || composition->artboards.empty())
            composition = std::find_if(document.compositions.begin(), document.compositions.end(),
                [](const auto& item) { return !item.artboards.empty(); });
        if (composition == document.compositions.end()) composition = document.compositions.begin();
        artboards_.clear();
        if (composition != document.compositions.end()) {
            active_composition_ = composition->id;
            if (std::none_of(composition->artboards.begin(), composition->artboards.end(),
                [&](const auto& board) { return board.id == active_artboard_; }))
                active_artboard_ = composition->artboards.empty() ? Id{} : composition->artboards.front().id;
            for (const auto& board : composition->artboards) artboards_.push_back(evaluate_artboard(*composition, board.id));
        } else { active_composition_.clear(); active_artboard_.clear(); }
        values_ = evaluate(document);
        transforms_ = evaluate_transforms(document,values_);
        geometry_.clear();
        geometry_index_.clear();mask_paths_.clear();scene_={};
        world_.clear();
        parents_.clear();
        if (composition != document.compositions.end()) {
            scene_=evaluate_scene(document,composition->id,values_,transforms_);
            std::set<Id> active_assets;
            for(const auto& [id,image]:scene_.images){(void)image;active_assets.insert(document.objects.at(id).image->asset);}
            std::erase_if(rasters_,[&](const auto& entry){return !active_assets.contains(entry.first);});
            for(const auto& id:active_assets) {
                const auto& payload=document.raster_assets.at(id).payload;const auto found=rasters_.find(id);
                if(found!=rasters_.end()&&found->second.payload==payload)continue;
                auto pixels=decode_raster(*payload);
                QImage source(pixels.rgba.data(),static_cast<int>(pixels.width),static_cast<int>(pixels.height),static_cast<int>(pixels.width*4),QImage::Format_RGBA8888);
                auto projection=source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                if(projection.isNull())throw Error("RENDER_ALLOCATION","Could not allocate an Image projection");
                rasters_.insert_or_assign(id,RasterProjection{payload,std::move(projection)});
            }
            std::function<void(const EvaluatedSceneNode&)> masks=[&](const auto& node) {
                if(node.mask) {
                    QPainterPath path;
                    for(const auto& instance:node.mask->paths)path.addPath(qt_transform(instance.transform).map(qt_path(*instance.contours,true)));
                    path.setFillRule(node.mask->fill_rule=="evenodd"?Qt::OddEvenFill:Qt::WindingFill);mask_paths_.emplace(node.id,std::move(path));
                }
                for(const auto& child:node.children)masks(child);
            };
            for(const auto& node:scene_.roots)masks(node);
            std::function<void(const Id&, std::vector<Id>)> visit;
            visit = [&](const Id& id, std::vector<Id> ancestors) {
                const auto& object = document.objects.at(id);
                const auto world = qt_transform(transforms_.at(id).world);
                world_.emplace(id, world);
                parents_.emplace(id, ancestors.empty() ? Id{} : ancestors.back());
                if (object.kind == Kind::group) {
                    ancestors.push_back(id);
                    for (const auto& child : object.children) visit(child, ancestors);
                    return;
                }
                Geometry item;
                item.id = id;
                item.ancestors = std::move(ancestors);
                item.world = world;
                item.normal_visible=object.visible&&values_.at({id,"","composite.opacity"})>0;
                for(const auto& ancestor:item.ancestors)item.normal_visible=item.normal_visible&&document.objects.at(ancestor).visible&&values_.at({ancestor,"","composite.opacity"})>0;
                auto value = [&](const Id& point_id, const char* field) {
                    return values_.at({id, point_id, field});
                };
                // Core owns operation order, repeat instances and paint grouping.
                // Qt only projects each evaluated layer into its drawing types.
                if(object.image) {
                    const auto& image=scene_.images.at(id);item.image=rasters_.at(object.image->asset).image;
                    item.image_bounds=QRectF(0,0,image.width,image.height);item.path.addRect(*item.image_bounds);
                    geometry_index_.emplace(id,geometry_.size());geometry_.push_back(std::move(item));return;
                }
                const auto& shape = scene_.shapes.at(id);
                if(object.text) {
                    std::map<std::string,double> parameters;
                    for(const auto& [name,scalar]:object.text->parameters){(void)scalar;parameters[name]=values_.at({id,"","text."+name});}
                    const auto layout=evaluate_text(*object.text,parameters);
                    item.text_bounds=QRectF(layout.x,layout.y,std::max(1.0,layout.width),std::max(1.0,layout.height));
                    item.text_overflow=layout.overflow;
                }
                std::map<const std::vector<EvaluatedContour>*, QPainterPath> contour_paths;
                for (const auto& layer : shape.paints) {
                    Geometry::Paint paint;
                    paint.transform = qt_transform(layer.transform);
                    paint.color = QColor::fromRgbF(layer.rgba[0], layer.rgba[1], layer.rgba[2], layer.rgba[3]);
                    paint.brush = QBrush(paint.color);
                    if (layer.gradient) {
                        const auto& source = *layer.gradient;
                        QGradientStops stops;
                        bool visible = false;
                        for (const auto& stop : source.stops) {
                            const auto alpha = stop.rgba[3] * layer.rgba[3];
                            stops.append({stop.offset, QColor::fromRgbF(stop.rgba[0], stop.rgba[1], stop.rgba[2], alpha)});
                            visible = visible || alpha > 0;
                        }
                        auto configure = [&](QGradient& gradient) {
                            gradient.setCoordinateMode(QGradient::LogicalMode);
                            gradient.setSpread(QGradient::PadSpread);
                            gradient.setInterpolationMode(QGradient::ComponentInterpolation);
                            gradient.setStops(stops);
                            paint.brush = QBrush(gradient);
                        };
                        const QPointF start(source.start.x, source.start.y), end(source.end.x, source.end.y);
                        if (source.type == "radial") {
                            QRadialGradient gradient(start, distance(start, end));
                            configure(gradient);
                        } else {
                            QLinearGradient gradient(start, end);
                            configure(gradient);
                        }
                        if (!visible) paint.color.setAlpha(0);
                    }
                    paint.fill = layer.type == "nect.paint.fill";
                    paint.width = layer.width;
                    for (const auto& instance : layer.paths) {
                        auto found = contour_paths.find(instance.contours.get());
                        if (found == contour_paths.end())
                            found = contour_paths.emplace(instance.contours.get(), qt_path(*instance.contours)).first;
                        paint.path.addPath(qt_transform(instance.transform).map(found->second));
                    }
                    paint.path.setFillRule(layer.fill_rule == "evenodd" ? Qt::OddEvenFill : Qt::WindingFill);
                    item.paints.push_back(std::move(paint));
                }
                for (const auto& contour : path_contours(object,&values_)) {
                    const auto first = item.points.size();
                    for (const auto& authored : contour.points) {
                        EvaluatedPoint p;
                        p.id = authored.id;
                        p.contour = contour.id;
                        p.anchor = {value(p.id, "x"), value(p.id, "y")};
                        p.in_angle = value(p.id, "in.angle");
                        p.out_angle = value(p.id, "out.angle");
                        p.incoming = handle(p.anchor, p.in_angle, value(p.id, "in.length"));
                        p.outgoing = handle(p.anchor, p.out_angle, value(p.id, "out.length"));
                        p.driven = authored.x.binding.has_value() || authored.y.binding.has_value() || authored.x.expression.has_value() || authored.y.expression.has_value();
                        // Generated topology contains placeholder Scalars. Only
                        // enabled authored coordinate corrections drive anchors;
                        // inspect those directly without evaluating per property.
                        if (object.source && object.point_edit && object.point_edit->enabled) {
                            const auto correction = object.point_edit->overrides.find(p.id);
                            if (correction != object.point_edit->overrides.end()) {
                                for (const auto* field : {"x", "y"}) {
                                    const auto coordinate = correction->second.find(field);
                                    if (coordinate != correction->second.end() && (coordinate->second.binding||coordinate->second.expression))
                                        p.driven = true;
                                }
                            }
                        }
                        item.points.push_back(std::move(p));
                    }
                    if (first == item.points.size()) continue;
                    item.path.moveTo(item.points[first].anchor);
                    for (auto i = first + 1; i < item.points.size(); ++i)
                        item.path.cubicTo(item.points[i - 1].outgoing, item.points[i].incoming,
                                          item.points[i].anchor);
                    if (contour.closed) {
                        item.path.cubicTo(item.points.back().outgoing, item.points[first].incoming,
                                          item.points[first].anchor);
                        item.path.closeSubpath();
                    }
                }
                geometry_index_.emplace(id,geometry_.size());geometry_.push_back(std::move(item));
            };
            for (const auto& root : composition->roots) visit(root, {});
        }

        if (!scope_.empty() && (!world_.contains(scope_) ||
            document.objects.at(scope_).kind != Kind::group)) set_scope({});
        auto retained=selections_;
        std::erase_if(retained,[&](const auto& s){const auto* g=geometry(s.object);
            return !world_.contains(s.object)||(!s.point.empty()&&(!g||!point(*g,s.point)));});
        if(retained!=selections_)select_many(std::move(retained));
        if (!drawing_object_.empty() && !world_.contains(drawing_object_)) {
            drawing_object_.clear();
            drawing_contour_.clear();
        }
        gradient_control_.reset();
        if (!gradient_operation_.empty()) {
            if (selected_object == gradient_object_ && document.objects.contains(gradient_object_)) {
                const auto& stack = document.objects.at(gradient_object_).stack;
                const auto operation = std::find_if(stack.begin(), stack.end(), [&](const auto& op) { return op.id == gradient_operation_; });
                if (operation != stack.end() && operation->gradient && operation->gradient->enabled) {
                    const auto& gradient = *operation->gradient;
                    auto get = [&](const char* field) { return values_.at(gradient_ref(gradient_object_, gradient_operation_, gradient.id, field)); };
                    gradient_control_ = GradientControl{gradient.id, {get("start_x"), get("start_y")},
                        {get("end_x"), get("end_y")}, world_.at(gradient_object_), gradient.type == "radial"};
                }
            }
            if (!gradient_control_) clear_gradient_edit();
        }
    } catch (const std::exception& exception) {
        report_error(exception);
    }
    update();
    if (!previous_artboard.empty() && (previous_composition != active_composition_ || previous_artboard != active_artboard_))
        fit_artboard();
    if ((previous_composition != active_composition_ || previous_artboard != active_artboard_) && active_artboard_changed)
        active_artboard_changed();
}

void Canvas::fit_artboard() {
    QRectF bounds;
    for (const auto& artboard : artboards_) if (artboard.id == active_artboard_)
        bounds = {artboard.x, artboard.y, artboard.width, artboard.height};
    fit_bounds(bounds);
}

void Canvas::fit_all_artboards() {
    QRectF bounds;
    for (const auto& artboard : artboards_)
        bounds = bounds.united({artboard.x, artboard.y, artboard.width, artboard.height});
    fit_bounds(bounds);
}

void Canvas::fit_bounds(QRectF bounds) {
    if (bounds.isEmpty()) {
        for (const auto& item : geometry_) {
            bounds = bounds.united(item.world.map(item.path).boundingRect());
            for (const auto& paint : item.paints)
                bounds = bounds.united((paint.transform * item.world).map(paint.path).boundingRect());
        }
    }
    if (bounds.isEmpty()) bounds = {0, 0, 640, 480};
    zoom_ = std::clamp(std::min(std::max(1, width() - 100) / bounds.width(),
                               std::max(1, height() - 100) / bounds.height()), 0.02, 64.0);
    pan_ = QPointF(width() / 2.0, height() / 2.0) - bounds.center() * zoom_;
    initial_fit_ = false;
    request_frame(QStringLiteral("fit"), true);
}

void Canvas::set_active_artboard(Id composition, Id artboard, bool fit) {
    const auto& document = session_.document();
    const auto found = std::find_if(document.compositions.begin(), document.compositions.end(),
        [&](const auto& item) { return item.id == composition; });
    if (found == document.compositions.end() || std::none_of(found->artboards.begin(), found->artboards.end(),
        [&](const auto& item) { return item.id == artboard; }))
        throw Error("MISSING_ARTBOARD", "Choose an artboard in its owning composition");
    const bool changed = composition != active_composition_ || artboard != active_artboard_;
    cancel_interaction();
    if (changed) set_draw_mode(false);
    if (composition != active_composition_) { select({}); set_scope({}); }
    active_composition_ = std::move(composition); active_artboard_ = std::move(artboard);
    refresh();
    if (fit) fit_artboard();
    if (changed && active_artboard_changed) active_artboard_changed();
}

void Canvas::set_scope(Id scope) {
    if (scope == scope_) return;
    scope_ = std::move(scope);
    if (scope_changed) scope_changed();
    update();
}

void Canvas::select(Id object, Id point_id, bool enter_parent) {
    select_many(object.empty()?std::vector<Selection>{}:std::vector<Selection>{{object,point_id}},enter_parent);
}

std::vector<Id> Canvas::selected_objects()const {
    std::vector<Id> result;
    for(const auto& item:selections_)if(std::find(result.begin(),result.end(),item.object)==result.end())result.push_back(item.object);
    return result;
}

void Canvas::select_many(std::vector<Selection> items,bool enter_parent) {
    std::vector<Selection> valid;
    // Object and point selection are distinct editing contexts. The active
    // (last) item determines the context, including extended tree selection.
    const bool points=!items.empty()&&!items.back().point.empty();
    for(const auto& item:items) {
        if(!world_.contains(item.object)||(!item.point.empty())!=points)continue;
        const auto* g=geometry(item.object);
        if(points&&(!g||!point(*g,item.point)))continue;
        if(std::find(valid.begin(),valid.end(),item)==valid.end())valid.push_back(item);
    }
    if(enter_parent)set_scope(valid.empty()?Id{}:parents_.at(valid.back().object));
    if(valid==selections_)return;
    if(valid.size()!=1||valid.back().object!=gradient_object_||!valid.back().point.empty())clear_gradient_edit();
    selections_=std::move(valid);
    selected_object=selections_.empty()?Id{}:selections_.back().object;
    selected_point=selections_.empty()?Id{}:selections_.back().point;
    if (selection_changed) selection_changed();
    update();
}

void Canvas::set_selections(std::vector<Selection> items) {
    cancel_interaction();
    // Restoring frozen picker targets may return from another composition.
    if(!items.empty()&&!world_.contains(items.back().object)&&session_.document().objects.contains(items.back().object))
        set_selection(items.back().object,items.back().point);
    refresh();select_many(std::move(items),true);
}

void Canvas::toggle_selection(Selection item) {
    auto items=selections_;const auto found=std::find(items.begin(),items.end(),item);
    if(found==items.end())items.push_back(std::move(item));else items.erase(found);
    select_many(std::move(items));
}

void Canvas::set_selection(Id object, Id point_id) {
    cancel_interaction();
    // A property picker may deliberately navigate to another composition plane.
    if (!object.empty()) {
        const auto& document = session_.document();
        std::function<bool(const Id&)> contains = [&](const Id& id) {
            if (id == object) return true;
            const auto& children = document.objects.at(id).children;
            return std::any_of(children.begin(), children.end(), contains);
        };
        for (const auto& composition : document.compositions)
            if (composition.id != active_composition_ && !composition.artboards.empty() &&
                std::any_of(composition.roots.begin(), composition.roots.end(), contains)) {
                set_active_artboard(composition.id, composition.artboards.front().id);
                break;
            }
    }
    // A caller may select the object immediately after its core Create command,
    // before the window-wide document callback has rebuilt our derived cache.
    refresh();
    select(std::move(object), std::move(point_id), true);
}

QString Canvas::breadcrumb() const {
    const auto& document = session_.preview_document();
    QStringList names;
    for (const auto& composition : document.compositions) if (composition.id == active_composition_) {
        names.push_back(QString::fromStdString(composition.name));
        for (const auto& board : composition.artboards) if (board.id == active_artboard_)
            names.push_back(QString::fromStdString(board.name));
    }
    std::vector<Id> chain;
    for (auto id = scope_; !id.empty() && parents_.contains(id); id = parents_.at(id)) chain.push_back(id);
    for (auto i = chain.rbegin(); i != chain.rend(); ++i)
        names.push_back(QString::fromStdString(document.objects.at(*i).name));
    return names.join(QStringLiteral("  /  "));
}

void Canvas::leave_group() {
    cancel_interaction();
    const auto old = scope_;
    set_scope(old.empty() || !parents_.contains(old) ? Id{} : parents_.at(old));
    if (!old.empty()) select(old);
}

void Canvas::set_draw_mode(bool enabled) {
    cancel_interaction();
    if (enabled) {clear_gradient_edit();set_anchor_edit(false);}
    drawing_object_.clear();
    drawing_contour_.clear();
    if (draw_mode_ == enabled) return;
    draw_mode_ = enabled;
    update_cursor();
    update();
    if (draw_mode_changed) draw_mode_changed(enabled);
}

void Canvas::set_anchor_edit(bool enabled) {
    cancel_interaction();
    if(enabled) {set_draw_mode(false);clear_gradient_edit();select(selected_object,{});}
    if(anchor_edit_==enabled)return;
    anchor_edit_=enabled;update_cursor();update();
    if(anchor_edit_changed)anchor_edit_changed(enabled);
}

void Canvas::clear_gradient_edit() {
    const bool active = !gradient_operation_.empty();
    gradient_object_.clear(); gradient_operation_.clear(); gradient_control_.reset();
    if (active && gradient_edit_changed) gradient_edit_changed();
    update();
}

void Canvas::set_gradient_edit(Id object, Id operation) {
    cancel_interaction();
    set_anchor_edit(false);
    if (operation.empty() || (object == gradient_object_ && operation == gradient_operation_)) {
        clear_gradient_edit(); return;
    }
    set_draw_mode(false);
    select(object, {}, true);
    gradient_object_ = std::move(object); gradient_operation_ = std::move(operation);
    refresh();
    if (gradient_edit_changed) gradient_edit_changed();
    setFocus();
}

Id Canvas::selection_target(const Geometry& item) const {
    if (scope_.empty()) return item.ancestors.empty() ? item.id : item.ancestors.front();
    const auto scope = std::find(item.ancestors.begin(), item.ancestors.end(), scope_);
    if (scope == item.ancestors.end()) return {};
    return scope + 1 == item.ancestors.end() ? item.id : *(scope + 1);
}

bool Canvas::visible_hit(const Geometry& item,QPointF screen) const {
    if(!item.normal_visible)return false;
    const auto world=view().inverted().map(screen);
    const auto contains=[&](const Id& id){const auto mask=mask_paths_.find(id);return mask==mask_paths_.end()||mask->second.contains(world);};
    if(!contains(item.id))return false;
    for(const auto& id:item.ancestors)if(!contains(id))return false;
    return true;
}
const Canvas::Geometry* Canvas::hit_path(QPointF screen) const {
    for (auto i = geometry_.rbegin(); i != geometry_.rend(); ++i) {
        if(!visible_hit(*i,screen))continue;
        if (selection_target(*i).empty()) continue;
        const auto transform = i->world * view();
        if(i->image_bounds&&transform.map(QPolygonF(*i->image_bounds)).containsPoint(screen,Qt::OddEvenFill))return &*i;
        if(i->text_bounds&&transform.map(QPolygonF(*i->text_bounds)).containsPoint(screen,Qt::OddEvenFill))return &*i;
        for (auto paint = i->paints.rbegin(); paint != i->paints.rend(); ++paint) {
            if (paint->color.alphaF() <= 0 || (!paint->fill && paint->width <= 0)) continue;
            const auto painted_transform = paint->transform * transform;
            const auto painted_path = painted_transform.map(paint->path);
            if (paint->fill && painted_path.contains(screen)) return &*i;
            if (!paint->fill) {
                QPainterPathStroker stroke;
                stroke.setWidth(paint->width);
                stroke.setCapStyle(Qt::FlatCap);
                stroke.setJoinStyle(Qt::SvgMiterJoin);
                stroke.setMiterLimit(4);
                if (painted_transform.map(stroke.createStroke(paint->path)).contains(screen)) return &*i;
            }
            QPainterPathStroker tolerance;
            tolerance.setWidth(hit_radius * 2);
            if (tolerance.createStroke(painted_path).contains(screen)) return &*i;
        }
        // The authored source stays addressable even when a stack has no paint.
        const auto screen_path = transform.map(i->path);
        QPainterPathStroker tolerance;
        tolerance.setWidth(hit_radius * 2);
        if (tolerance.createStroke(screen_path).contains(screen)) return &*i;
        for (const auto& p : i->points)
            if (distance(transform.map(p.anchor), screen) <= hit_radius) return &*i;
    }
    return nullptr;
}

Canvas::Hit Canvas::hit_control(QPointF screen) const {
    if(anchor_edit_) {
        if(!world_.contains(selected_object))return {};
        const QPointF anchor(values_.at({selected_object,"","transform.anchor_x"}),values_.at({selected_object,"","transform.anchor_y"}));
        if(distance((world_.at(selected_object)*view()).map(anchor),screen)<=hit_radius+2)return {Drag::pivot,selected_object,{}};
        return {};
    }
    if (gradient_control_) {
        const auto transform = gradient_control_->world * view();
        if (distance(transform.map(gradient_control_->start), screen) <= hit_radius)
            return {Drag::gradient_start, gradient_object_, {}};
        if (distance(transform.map(gradient_control_->end), screen) <= hit_radius)
            return {Drag::gradient_end, gradient_object_, {}};
        return {};
    }
    for(const auto& object:selected_objects()) {
    const auto* item = geometry(object);
    if (!item) continue;
    const auto transform = item->world * view();
    if (const auto* selected = point(*item, object==selected_object?selected_point:Id{})) {
        if (distance(transform.map(selected->anchor), screen) <= 5)
            return {Drag::anchor, item->id, selected->id};
        // Zero-length handles deliberately stay at their authored angle. Alt-drag
        // the anchor creates handles instead of inventing a stored handle length.
        if (distance(selected->anchor, selected->incoming) > 1e-9 &&
            distance(transform.map(selected->incoming), screen) <= hit_radius)
            return {Drag::incoming, item->id, selected->id};
        if (distance(selected->anchor, selected->outgoing) > 1e-9 &&
            distance(transform.map(selected->outgoing), screen) <= hit_radius)
            return {Drag::outgoing, item->id, selected->id};
    }
    for (const auto& p : item->points)
        if (distance(transform.map(p.anchor), screen) <= hit_radius)
            return {Drag::anchor, item->id, p.id};
    }
    return {};
}

void Canvas::paintEvent(QPaintEvent*) {
    const auto start = clock_.nsecsElapsed();
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor(39, 42, 47));
        const auto& document = session_.preview_document();
        if (!artboards_.empty()) {
            painter.setWorldTransform(view());
            for (const auto& artboard : artboards_) {
                const QRectF area(artboard.x, artboard.y, artboard.width, artboard.height);
                painter.fillRect(area.translated(3 / zoom_, 3 / zoom_), QColor(20, 22, 26));
                painter.fillRect(area, QColor(250, 250, 250));
            }
        }
        const auto draw_leaf=[&](QPainter& target,const Geometry& item,QPointF origin=QPointF{}) {
            if(item.image_bounds) {
                target.setWorldTransform(item.world*view()*QTransform::fromTranslate(-origin.x(),-origin.y()));
                target.setRenderHint(QPainter::SmoothPixmapTransform);target.drawImage(*item.image_bounds,item.image);return;
            }
            for (const auto& paint : item.paints) {
                if (paint.color.alphaF() <= 0 || (!paint.fill && paint.width <= 0)) continue;
                target.setWorldTransform(paint.transform * item.world * view()*QTransform::fromTranslate(-origin.x(),-origin.y()));
                if (paint.fill) {
                    target.setPen(Qt::NoPen);
                    target.setBrush(paint.brush);
                } else {
                    target.setBrush(Qt::NoBrush);
                    QPen pen(paint.brush, paint.width, Qt::SolidLine, Qt::FlatCap, Qt::SvgMiterJoin);
                    pen.setMiterLimit(4);target.setPen(pen);
                }
                target.drawPath(paint.path);
            }
        };
        try {
            if(!scene_.requires_compositing) {
                for(const auto& item:geometry_)if(item.normal_visible)draw_leaf(painter,item);
            } else {
                // Artwork has a transparent Composition backdrop. The white
                // artboard and dark workspace are UI, never inputs to a blend.
                const auto dpr=devicePixelRatioF();
                const auto pixel_width=std::ceil(width()*dpr),pixel_height=std::ceil(height()*dpr);
                if(pixel_width<=0||pixel_height<=0||pixel_width>16384||pixel_height>16384)
                    throw Error("RENDER_LIMIT","Compositing viewport exceeds 16384 physical pixels per axis");
                const QRect viewport(0,0,static_cast<int>(pixel_width),static_cast<int>(pixel_height));
                constexpr std::size_t budget=128*1024*1024;std::size_t allocated=0;
                const auto byte_size=[](const QRect& region){return std::size_t(region.width())*std::size_t(region.height())*4;};
                const auto surface=[&](const QRect& region) {
                    const auto bytes=byte_size(region);
                    if(bytes>budget-allocated)throw Error("RENDER_LIMIT","Compositing surfaces exceed the 128 MiB viewport budget");
                    QImage image(region.size(),QImage::Format_ARGB32_Premultiplied);
                    if(image.isNull())throw Error("RENDER_ALLOCATION","Could not allocate a compositing surface");
                    image.setDevicePixelRatio(dpr);image.fill(Qt::transparent);allocated+=bytes;return image;
                };
                // Bounds are evaluated only when a scope needs an image. A mask
                // is an unconditional coverage bound, so masked Groups need no
                // traversal/stroking of their descendants just to size a surface.
                std::map<Id,QRectF> bounds;
                std::function<QRectF(const EvaluatedSceneNode&)> painted_bounds=[&](const EvaluatedSceneNode& node) {
                    if(!node.visible||node.opacity<=0)return QRectF{};
                    if(const auto found=bounds.find(node.id);found!=bounds.end())return found->second;
                    QRectF result;
                    if(node.mask)result=mask_paths_.at(node.id).boundingRect();
                    else {
                        if(const auto* item=geometry(node.id);item&&item->image_bounds)result=item->world.mapRect(*item->image_bounds);
                        if(const auto* item=geometry(node.id))for(const auto& paint:item->paints) {
                            if(paint.color.alphaF()<=0||(!paint.fill&&paint.width<=0))continue;
                            const auto transform=paint.transform*item->world;
                            if(paint.fill)result=result.united(transform.map(paint.path).boundingRect());
                            else {
                                QPainterPathStroker stroke;stroke.setWidth(paint.width);stroke.setCapStyle(Qt::FlatCap);
                                stroke.setJoinStyle(Qt::SvgMiterJoin);stroke.setMiterLimit(4);
                                // The control hull conservatively contains the
                                // generated stroke curves under affine transforms.
                                result=result.united(transform.map(stroke.createStroke(paint.path)).controlPointRect());
                            }
                        }
                        for(const auto& child:node.children)result=result.united(painted_bounds(child));
                    }
                    bounds.emplace(node.id,result);return result;
                };
                const auto pixel_region=[&](const EvaluatedSceneNode& node,const QRect& parent) {
                    const auto world=painted_bounds(node);if(world.isEmpty())return QRect{};
                    auto screen=view().mapRect(world);
                    if(!std::isfinite(screen.left())||!std::isfinite(screen.top())||!std::isfinite(screen.right())||!std::isfinite(screen.bottom()))
                        throw Error("RENDER_RANGE","Compositing paint bounds must be finite");
                    // Keep AA coverage and align origins to physical pixels. The
                    // same phase is used by artwork, masks, nested images and DPR.
                    screen=screen.adjusted(-2/dpr,-2/dpr,2/dpr,2/dpr).intersected(QRectF(0,0,width(),height()));
                    if(screen.isEmpty())return QRect{};
                    const int left=static_cast<int>(std::floor(screen.left()*dpr)),top=static_cast<int>(std::floor(screen.top()*dpr));
                    const int right=static_cast<int>(std::ceil(screen.right()*dpr)),bottom=static_cast<int>(std::ceil(screen.bottom()*dpr));
                    return QRect(left,top,right-left,bottom-top).intersected(parent);
                };
                const auto origin=[&](const QRect& region){return QPointF(region.x()/dpr,region.y()/dpr);};
                std::function<void(QPainter&,const EvaluatedSceneNode&,unsigned,const QRect&)> render;
                const auto content=[&](QPainter& target,const EvaluatedSceneNode& node,unsigned depth,const QRect& region) {
                    if(const auto* item=geometry(node.id))draw_leaf(target,*item,origin(region));
                    for(const auto& child:node.children)render(target,child,depth,region);
                };
                render=[&](QPainter& target,const EvaluatedSceneNode& node,unsigned depth,const QRect& parent) {
                    if(!node.visible||node.opacity<=0)return;
                    if(!node.isolated){content(target,node,depth,parent);return;}
                    if(depth>=16)throw Error("RENDER_LIMIT","Compositing isolation nesting exceeds 16");
                    const auto region=pixel_region(node,parent);if(region.isEmpty())return;
                    const auto bytes=byte_size(region);auto image=surface(region);
                    {
                        QPainter layer(&image);layer.setRenderHint(QPainter::Antialiasing);content(layer,node,depth+1,region);
                        if(node.mask) {
                            auto coverage=surface(region);const auto offset=origin(region);
                            {QPainter mask(&coverage);mask.setRenderHint(QPainter::Antialiasing);mask.setWorldTransform(view()*QTransform::fromTranslate(-offset.x(),-offset.y()));
                             mask.setPen(Qt::NoPen);mask.setBrush(Qt::white);mask.drawPath(mask_paths_.at(node.id));}
                            layer.resetTransform();layer.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                            layer.drawImage(QPointF(0,0),coverage);allocated-=bytes;
                        }
                    }
                    target.save();target.resetTransform();target.setOpacity(node.opacity);target.setCompositionMode(blend_mode(node.blend));
                    target.drawImage(origin(region)-origin(parent),image);target.restore();allocated-=bytes;
                };
                auto artwork=surface(viewport);
                {QPainter layer(&artwork);layer.setRenderHint(QPainter::Antialiasing);for(const auto& node:scene_.roots)render(layer,node,0,viewport);}
                painter.save();painter.resetTransform();painter.drawImage(QPointF(0,0),artwork);painter.restore();
            }
            render_error_.clear();
        } catch(const std::exception& exception) {
            const auto message=QString::fromUtf8(exception.what());
            if(render_error_!=message){render_error_=message;report_error(exception);}
        }
        painter.resetTransform();
        for (const auto& artboard : artboards_) {
            const auto frame = view().mapRect(QRectF(artboard.x, artboard.y, artboard.width, artboard.height));
            const bool active = artboard.id == active_artboard_;
            painter.setBrush(Qt::NoBrush); painter.setPen(QPen(active ? accent : QColor(122, 131, 145), active ? 2 : 1));
            painter.drawRect(frame);
            painter.drawText(QRectF(frame.left(), frame.top()-23, std::max(100.0, frame.width()), 20),
                Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(artboard.name) + (active ? tr(" · active") : QString{}));
        }
        if(show_mask_outline_)for(const auto& selected:selections_) {
            const auto mask=mask_paths_.find(selected.object);if(mask==mask_paths_.end())continue;
            painter.setPen(QPen(QColor(153,210,225,145),1,Qt::DashLine));painter.setBrush(Qt::NoBrush);
            painter.drawPath(view().map(mask->second));
        }
        QRectF selected_bounds;
        for (const auto& item : geometry_) {
            const bool directly_selected=std::any_of(selections_.begin(),selections_.end(),[&](const auto& s){return s.object==item.id;});
            if(!item.normal_visible&&!directly_selected)continue;
            const bool selected = std::any_of(selections_.begin(),selections_.end(),[&](const auto& s){return item.id==s.object||
                (s.point.empty()&&std::find(item.ancestors.begin(),item.ancestors.end(),s.object)!=item.ancestors.end());});
            if (!selected) continue;
            const auto transform = item.world * view();
            const auto screen_path = transform.map(item.path);
            selected_bounds = selected_bounds.united(screen_path.boundingRect());
            painter.setPen(QPen(accent, 1));
            painter.setBrush(Qt::NoBrush);
            for (const auto& paint : item.paints) {
                if (paint.color.alphaF() <= 0 || (!paint.fill && paint.width <= 0)) continue;
                const auto painted_path = (paint.transform * transform).map(paint.path);
                selected_bounds = selected_bounds.united(painted_path.boundingRect());
                painter.drawPath(painted_path);
            }
            painter.drawPath(screen_path);
            if(item.text_bounds) {
                const auto outline=transform.map(QPolygonF(*item.text_bounds));
                selected_bounds=selected_bounds.united(outline.boundingRect());
                painter.setPen(QPen(item.text_overflow?QColor("#f6a85b"):accent,1,Qt::DashLine));
                painter.drawPolygon(outline);
            }
            if (std::none_of(selections_.begin(),selections_.end(),[&](const auto& s){return s.object==item.id;}))continue;
            if (gradient_control_ && item.id == gradient_object_) continue;
            if (const auto* p = point(item, item.id==selected_object?selected_point:Id{})) {
                painter.setPen(QPen(accent, 1));
                const auto anchor = transform.map(p->anchor);
                for (const auto handle_point : {p->incoming, p->outgoing}) {
                    if (distance(handle_point, p->anchor) <= 1e-9) continue;
                    const auto endpoint = transform.map(handle_point);
                    painter.drawLine(anchor, endpoint);
                    painter.setBrush(QColor(39, 42, 47));
                    painter.drawEllipse(endpoint, 4, 4);
                }
            }
            for (const auto& p : item.points) {
                const auto screen = transform.map(p.anchor);
                const bool chosen=std::find(selections_.begin(),selections_.end(),Selection{item.id,p.id})!=selections_.end();
                const auto size = chosen ? 8.0 : 6.0;
                painter.setPen(QPen(p.driven ? linked : accent, 1.3));
                painter.setBrush(chosen ? (p.driven ? linked : accent) : QColor(250, 250, 250));
                painter.drawRect(QRectF(screen.x() - size / 2, screen.y() - size / 2, size, size));
            }
        }
        if(anchor_edit_&&world_.contains(selected_object)) {
            const QPointF anchor(values_.at({selected_object,"","transform.anchor_x"}),values_.at({selected_object,"","transform.anchor_y"}));
            const auto screen=(world_.at(selected_object)*view()).map(anchor);
            painter.setWorldTransform(QTransform{});painter.setPen(QPen(QColor("#ffc677"),1.5));painter.setBrush(QColor(39,42,47));
            painter.drawEllipse(screen,7,7);painter.drawLine(screen+QPointF(-12,0),screen+QPointF(12,0));
            painter.drawLine(screen+QPointF(0,-12),screen+QPointF(0,12));painter.drawText(screen+QPointF(13,-12),tr("Anchor"));
        }
        if (gradient_control_) {
            const auto& control = *gradient_control_;
            const auto transform = control.world * view();
            const auto start_screen = transform.map(control.start), end_screen = transform.map(control.end);
            painter.setPen(QPen(accent, 1.5)); painter.setBrush(Qt::NoBrush);
            if (control.radial) {
                QPainterPath circle;
                const auto radius = distance(control.start, control.end);
                circle.addEllipse(control.start, radius, radius);
                painter.drawPath(transform.map(circle));
            }
            painter.drawLine(start_screen, end_screen);
            painter.setBrush(QColor(39, 42, 47)); painter.drawEllipse(start_screen, 6, 6);
            painter.drawRect(QRectF(end_screen.x()-5, end_screen.y()-5, 10, 10));
            painter.drawText(start_screen + QPointF(10, -10), control.radial ? tr("Center") : tr("Start"));
            painter.drawText(end_screen + QPointF(10, -10), control.radial ? tr("Radius") : tr("End"));
        }
        if (!selected_object.empty() && document.objects.contains(selected_object) &&
            (selections_.size()>1||document.objects.at(selected_object).kind == Kind::group) && !selected_bounds.isNull()) {
            painter.setPen(QPen(accent, 1, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(selected_bounds.adjusted(-5, -5, 5, 5));
        }
        const auto label = (scope_.empty() ? QString{} : QStringLiteral("‹  ")) + breadcrumb();
        breadcrumb_rect_ = QRectF(12, 12, std::min(width() - 24.0,
            painter.fontMetrics().horizontalAdvance(label) + 24.0), 28);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(31, 34, 39, 225));
        painter.drawRoundedRect(breadcrumb_rect_, 4, 4);
        painter.setPen(QColor(217, 222, 229));
        painter.drawText(breadcrumb_rect_.adjusted(10, 0, -10, 0), Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(label, Qt::ElideRight,
                             static_cast<int>(breadcrumb_rect_.width() - 20)));
        const auto hint = draw_mode_
            ? tr("Add Path · Click for points · Click first point to close · Enter / Esc to finish")
            : anchor_edit_ ? tr("Anchor · Drag the crosshair to change the pivot; artwork stays in place · Esc exits")
            : gradient_control_ ? tr("Gradient · Drag its handles · Esc cancels a drag / exits handles · Space-drag to pan")
            : tr("Shift-click adds/removes objects or points · Drag to move · Alt: handles · Space: pan · F: fit");
        painter.setPen(QColor(166, 174, 186));
        painter.drawText(QRect(14, height() - 30, width() - 100, 22), Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(hint, Qt::ElideRight, width() - 110));
        painter.drawText(QRect(width() - 85, height() - 30, 70, 22), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(zoom_ * 100, 'f', 0) + QStringLiteral("%"));
        if(!render_error_.isEmpty()) {
            painter.fillRect(QRect(10,46,width()-20,48),QColor(65,25,27,240));painter.setPen(QColor("#ffb4ab"));
            painter.drawText(QRect(18,49,width()-36,42),Qt::TextWordWrap,tr("Rendering unavailable: ")+render_error_);
        }
    }
    const auto end = clock_.nsecsElapsed();
    if (input_started_ns_ >= 0) {
        FrameTiming sample;
        sample.operation = pending_operation_;
        sample.paint_ms = (end - start) / 1e6;
        sample.input_to_paint_ms = (end - input_started_ns_) / 1e6;
        if (last_paint_ns_ >= 0 && painted_sequence_ == input_sequence_)
            sample.interval_ms = (end - last_paint_ns_) / 1e6;
        sample.viewport_width = width();
        sample.viewport_height = height();
        sample.device_pixel_ratio = devicePixelRatioF();
        if (timings_.size() >= 4096) timings_.erase(timings_.begin(), timings_.begin() + 1024);
        timings_.push_back(std::move(sample));
        last_paint_ns_ = end;
        painted_sequence_ = input_sequence_;
        input_started_ns_ = -1;
    }
}

void Canvas::begin_drag(Drag kind, QPointF screen) {
    press_position_ = screen;
    press_pan_ = pan_;
    drag_moved_ = false;
    if (kind == Drag::pan) {
        drag_ = kind;
        ++input_sequence_;
        update_cursor();
        return;
    }
    try {
        bool invertible = false;
        start_values_.clear();
        point_starts_.clear();
        if (kind == Drag::gradient_start || kind == Drag::gradient_end) {
            if (!gradient_control_) return;
            drag_inverse_ = gradient_control_->world.inverted(&invertible);
            start_anchor_ = kind == Drag::gradient_start ? gradient_control_->start : gradient_control_->end;
            for (const auto* field : {"start_x", "start_y", "end_x", "end_y"})
                start_values_.emplace(field, values_.at(gradient_ref(gradient_object_, gradient_operation_, gradient_control_->id, field)));
        } else if(kind==Drag::pivot) {
            drag_inverse_=world_.at(selected_object).inverted(&invertible);
            start_anchor_={values_.at({selected_object,"","transform.anchor_x"}),values_.at({selected_object,"","transform.anchor_y"})};
            start_values_.emplace("transform.anchor_x",start_anchor_.x());start_values_.emplace("transform.anchor_y",start_anchor_.y());
        } else if (kind == Drag::object) {
            // The shared command solves the complete effective-parent graph so
            // selecting an ancestor and its follower never translates twice.
            drag_inverse_=QTransform{};invertible=true;
            if(selections_.size()==1) {
                const auto parent=transforms_.at(selected_object).effective_parent;
                drag_inverse_=(parent.empty()?QTransform{}:world_.at(parent)).inverted(&invertible);
                start_translation_={values_.at({selected_object,"","transform.tx"}),values_.at({selected_object,"","transform.ty"})};
                start_values_.emplace("transform.tx",start_translation_.x());start_values_.emplace("transform.ty",start_translation_.y());
            }
        } else {
            const auto* item = geometry(selected_object);
            const auto* p = item ? point(*item, selected_point) : nullptr;
            if (!p) return;
            drag_inverse_ = item->world.inverted(&invertible);
            start_anchor_ = p->anchor;
            start_handle_ = kind == Drag::incoming ? p->incoming
                : kind == Drag::outgoing ? p->outgoing : p->anchor;
            start_in_angle_ = p->in_angle;
            start_out_angle_ = p->out_angle;
            for (const auto* field : {"x", "y", "in.angle", "in.length", "out.angle", "out.length"})
                start_values_.emplace(field, values_.at({selected_object, selected_point, field}));
            if(kind==Drag::anchor)for(const auto& selected:selections_) {
                const auto* g=geometry(selected.object);const auto* selected_anchor=g?point(*g,selected.point):nullptr;
                if(!selected_anchor)continue;
                bool valid=false;const auto inverse=g->world.inverted(&valid);
                if(!valid)throw Error("SINGULAR_TRANSFORM","Cannot drag points through a singular object transform");
                point_starts_.push_back({selected,selected_anchor->anchor,inverse});
            }
        }
        if (!invertible) throw Error("SINGULAR_TRANSFORM", "Cannot drag through a singular parent/object transform");
        session_.begin_gesture(session_.revision());
        gesture_owned_ = true;
        drag_ = kind;
        ++input_sequence_;
        update_cursor();
    } catch (const std::exception& exception) {
        report_error(exception);
    }
}

void Canvas::update_drag(QPointF screen) {
    if (drag_ == Drag::none) return;
    if (drag_ == Drag::pan) {
        request_frame(QStringLiteral("pan"));
        pan_ = press_pan_ + screen - press_position_;
        update();
        return;
    }
    if (!drag_moved_ && distance(screen, press_position_) < QApplication::startDragDistance()) return;
    drag_moved_ = true;
    const bool gradient_drag = drag_ == Drag::gradient_start || drag_ == Drag::gradient_end;
    const auto operation = gradient_drag ? QStringLiteral("gradient") : drag_ == Drag::object ? QStringLiteral("transform") : QStringLiteral("point-handle");
    request_frame(operation);
    const auto local = drag_inverse_.map(view().inverted().map(screen));
    const auto press_local = drag_inverse_.map(view().inverted().map(press_position_));
    std::vector<Command> commands;
    auto set = [&](const Id& point_id, const char* field, double value) {
        const Ref ref = gradient_drag ? gradient_ref(gradient_object_, gradient_operation_, gradient_control_->id, field)
            : Ref{selected_object, point_id, field};
        // Unchanged axes need no Set and must not cause a spurious driven error.
        // Session still validates every changed target atomically.
        const auto original = start_values_.at(field);
        if (std::abs(original - value) > 1e-10) commands.push_back(Set{ref, value});
    };
    try {
        if (gradient_drag) {
            const auto target = start_anchor_ + local - press_local;
            set({}, drag_ == Drag::gradient_start ? "start_x" : "end_x", target.x());
            set({}, drag_ == Drag::gradient_start ? "start_y" : "end_y", target.y());
        } else if(drag_==Drag::pivot) {
            const auto target=start_anchor_+local-press_local;
            set({},"transform.anchor_x",target.x());set({},"transform.anchor_y",target.y());
        } else if (drag_ == Drag::anchor) {
            const auto world=view().inverted().map(screen),start=view().inverted().map(press_position_);
            for(const auto& item:point_starts_) {
                const auto delta=item.inverse.map(world)-item.inverse.map(start);
                if(std::abs(delta.x())>1e-10)commands.push_back(Set{{item.target.object,item.target.point,"x"},item.anchor.x()+delta.x()});
                if(std::abs(delta.y())>1e-10)commands.push_back(Set{{item.target.object,item.target.point,"y"},item.anchor.y()+delta.y()});
            }
        } else if (drag_ == Drag::object) {
            const auto delta=local-press_local;
            if(selections_.size()==1) {
                const auto target=start_translation_+delta;set({},"transform.tx",target.x());set({},"transform.ty",target.y());
            } else if(std::abs(delta.x())>1e-10||std::abs(delta.y())>1e-10)commands.push_back(TranslateObjects{selected_objects(),delta.x(),delta.y()});
        } else {
            const auto delta = start_handle_ + local - press_local - start_anchor_;
            const auto length = std::hypot(delta.x(), delta.y());
            const auto angle = length <= 1e-10
                ? (drag_ == Drag::incoming ? start_in_angle_ : start_out_angle_)
                : std::atan2(delta.y(), delta.x()) * 180.0 / std::numbers::pi;
            if (drag_ == Drag::incoming) {
                set(selected_point, "in.angle", angle);
                set(selected_point, "in.length", length);
            } else {
                set(selected_point, "out.angle", angle);
                set(selected_point, "out.length", length);
                if (drag_ == Drag::symmetric) {
                    set(selected_point, "in.angle", length <= 1e-10 ? start_in_angle_ : angle + 180);
                    set(selected_point, "in.length", length);
                }
            }
        }
        // An empty preview restores the start state when a drag returns home.
        session_.update_gesture(commands);
        refresh();
    } catch (const std::exception& exception) {
        cancel_interaction();
        report_error(exception);
    }
}

void Canvas::finish_drag() {
    if (gesture_owned_) {
        const auto revision = session_.revision();
        try {
            if (session_.gesture_active()) session_.commit_gesture();
        } catch (const std::exception& exception) {
            if (session_.gesture_active()) session_.cancel_gesture();
            report_error(exception);
        }
        gesture_owned_ = false;
        drag_ = Drag::none;
        if(session_.revision()!=revision) {
            // The last successful preview already projected these exact committed
            // values. The Window callback refreshes Inspector/structure once; a
            // second full projection here only adds latency to pointer release.
            if(document_changed)document_changed();
            update();
        } else refresh(); // A cancellation/failure may have restored the start state.
    }
    drag_ = Drag::none;
    update_cursor();
}

void Canvas::cancel_interaction() {
    if (gesture_owned_ && session_.gesture_active()) session_.cancel_gesture();
    const bool had_preview = gesture_owned_;
    gesture_owned_ = false;
    drag_ = Drag::none;
    drag_moved_ = false;
    update_cursor();
    if (had_preview) refresh();
}

void Canvas::append_draw_point(QPointF screen) {
    try {
        if (active_composition_.empty()) throw Error("MISSING_COMPOSITION", "Create a composition before drawing");
        const auto parent_world = !drawing_object_.empty()?world_.at(drawing_object_):scope_.empty() ? QTransform{} : world_.at(scope_);
        bool invertible = false;
        const auto inverse = parent_world.inverted(&invertible);
        if (!invertible) throw Error("SINGULAR_TRANSFORM", "Cannot draw through a singular group transform");
        const auto local = inverse.map(view().inverted().map(screen));
        if (!drawing_object_.empty()) {
            const auto* item = geometry(drawing_object_);
            if (item && item->points.size() >= 2 &&
                distance((item->world * view()).map(item->points.front().anchor), screen) <= hit_radius) {
                session_.apply({CloseContour{drawing_object_, drawing_contour_, true}}, session_.revision());
                refresh();
                set_draw_mode(false);
                if (document_changed) document_changed();
                return;
            }
        }
        Point p;
        p.id = unique_id("point-");
        p.x.literal = local.x();
        p.y.literal = local.y();
        auto object_id = drawing_object_;
        auto contour_id = drawing_contour_;
        if (object_id.empty()) {
            object_id = unique_id("path-");
            contour_id = unique_id("contour-");
            session_.apply({CreatePath{active_composition_, scope_, object_id,
                                      "Path", {{contour_id, false, {p}}}}}, session_.revision());
        } else {
            session_.apply({AddPoint{object_id, contour_id, p}}, session_.revision());
        }
        drawing_object_ = object_id;
        drawing_contour_ = contour_id;
        refresh();
        select(object_id, p.id);
        if (document_changed) document_changed();
    } catch (const std::exception& exception) {
        report_error(exception);
    }
}

void Canvas::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && space_down_)) {
        begin_drag(Drag::pan, event->position());
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    if (!scope_.empty() && breadcrumb_rect_.contains(event->position())) {
        leave_group();
        return;
    }
    if (draw_mode_) {
        append_draw_point(event->position());
        return;
    }
    const auto hit = hit_control(event->position());
    const bool extend=event->modifiers().testFlag(Qt::ShiftModifier);
    if (hit.kind != Drag::none) {
        const Selection chosen{hit.object,hit.point};
        if(extend&&hit.kind==Drag::anchor){toggle_selection(chosen);event->accept();return;}
        if(std::find(selections_.begin(),selections_.end(),chosen)==selections_.end()||hit.kind!=Drag::anchor)select(hit.object,hit.point);
        else if(selections_.back()!=chosen){auto items=selections_;std::erase(items,chosen);items.push_back(chosen);select_many(std::move(items));}
        begin_drag(hit.kind == Drag::anchor && event->modifiers().testFlag(Qt::AltModifier)
                       ? Drag::symmetric : hit.kind, event->position());
    } else if (const auto* item = hit_path(event->position())) {
        const auto target = selection_target(*item);
        if(extend&&!selected_point.empty()&&target==item->id) {
            for(const auto& p:item->points)if(distance((item->world*view()).map(p.anchor),event->position())<=hit_radius) {
                toggle_selection({item->id,p.id});event->accept();return;
            }
        }
        if(extend){toggle_selection({target,{}});event->accept();return;}
        if(std::find(selections_.begin(),selections_.end(),Selection{target,{}})==selections_.end())select(target);
        if(!anchor_edit_)begin_drag(Drag::object, event->position());
    } else {
        if(!extend)select({});
    }
    event->accept();
}

void Canvas::mouseMoveEvent(QMouseEvent* event) {
    if (drag_ != Drag::none) update_drag(event->position());
    else if (!space_down_ && !draw_mode_) {
        const auto hit = hit_control(event->position());
        setCursor(hit.kind != Drag::none ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    event->accept();
}

void Canvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) finish_drag();
    event->accept();
}

void Canvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (draw_mode_ || event->button() != Qt::LeftButton) return;
    cancel_interaction();
    if (const auto* item = hit_path(event->position())) {
        auto target = selection_target(*item);
        if (!target.empty() && session_.document().objects.at(target).kind == Kind::group) {
            set_scope(target);
            target = selection_target(*item);
        }
        select(target);
    }
    event->accept();
}

void Canvas::wheelEvent(QWheelEvent* event) {
    // Changing the view mid-edit would change the drag's inverse mapping.
    if (gesture_owned_) { event->accept(); return; }
    const auto now = clock_.nsecsElapsed();
    request_frame(QStringLiteral("zoom"), last_wheel_ns_ < 0 || now - last_wheel_ns_ > 250000000);
    last_wheel_ns_ = now;
    const auto cursor = event->position();
    const auto under_cursor = view().inverted().map(cursor);
    const auto delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->pixelDelta().y();
    zoom_ = std::clamp(zoom_ * std::pow(1.0015, delta), 0.02, 64.0);
    pan_ = cursor - under_cursor * zoom_;
    update();
    event->accept();
}

void Canvas::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (drag_ != Drag::none) cancel_interaction();
        else if (draw_mode_) set_draw_mode(false);
        else if(anchor_edit_)set_anchor_edit(false);
        else if (gradient_control_) clear_gradient_edit();
        else if (!scope_.empty()) leave_group();
        else select({});
    } else if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        space_down_ = true;
        update_cursor();
    } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && draw_mode_) {
        set_draw_mode(false);
    } else if (event->key() == Qt::Key_F && event->modifiers() == Qt::NoModifier && drag_ == Drag::none) {
        fit_artboard();
    } else {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void Canvas::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        space_down_ = false;
        update_cursor();
        event->accept();
    } else QWidget::keyReleaseEvent(event);
}

void Canvas::focusOutEvent(QFocusEvent* event) {
    space_down_ = false;
    cancel_interaction();
    QWidget::focusOutEvent(event);
}

void Canvas::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (initial_fit_) fit_artboard();
}

void Canvas::report_error(const std::exception& exception) {
    auto message = QString::fromUtf8(exception.what());
    if (const auto* core_error = dynamic_cast<const Error*>(&exception))
        message = QString::fromStdString(core_error->code) + QStringLiteral(": ") + message;
    if (error) error(message);
}

void Canvas::request_frame(const QString& operation, bool new_sequence) {
    if (new_sequence) ++input_sequence_;
    // Keep the earliest unpainted input so coalescing is visible in latency data.
    if (input_started_ns_ < 0) input_started_ns_ = clock_.nsecsElapsed();
    pending_operation_ = operation;
    update();
}

void Canvas::reset_timing() {
    timings_.clear();
    input_started_ns_ = -1;
    last_paint_ns_ = -1;
    last_wheel_ns_ = -1;
    ++input_sequence_;
}

void Canvas::update_cursor() {
    if (drag_ == Drag::pan) setCursor(Qt::ClosedHandCursor);
    else if (space_down_) setCursor(Qt::OpenHandCursor);
    else if (draw_mode_ || anchor_edit_ || drag_ == Drag::anchor || drag_ == Drag::incoming ||
             drag_ == Drag::outgoing || drag_ == Drag::symmetric || drag_ == Drag::gradient_start ||
             drag_ == Drag::gradient_end) setCursor(Qt::CrossCursor);
    else if (drag_ == Drag::object) setCursor(Qt::SizeAllCursor);
    else setCursor(Qt::ArrowCursor);
}

} // namespace nect::desktop
