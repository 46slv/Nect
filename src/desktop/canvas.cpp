#include "canvas.hpp"

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QImage>
#include <QColorSpace>
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
QPainterPath qt_stroke(const QPainterPath& path,double width,Qt::PenCapStyle cap,Qt::PenJoinStyle join,double miter_limit) {
    QPainterPathStroker stroke;stroke.setWidth(width);stroke.setCapStyle(cap);stroke.setJoinStyle(join);stroke.setMiterLimit(miter_limit);
    return stroke.createStroke(path);
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
    projection_error_ = {};
    if(drag_==Drag::guide&&!guide_context_current()) {
        if(gesture_owned_&&session_.gesture_active())session_.cancel_gesture();
        gesture_owned_=false;drag_=Drag::none;guide_drag_invalid_=false;
        report_error(Error("REVISION_CONFLICT","Guide drag belongs to an older document session or revision"));
    }
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
        if(const auto* validated=session_.preview_values())values_=*validated;
        else values_=evaluate(document);
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
                    item.text_line_baselines_y=layout.line_baselines_y;
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
                    paint.cap=layer.line_cap=="round"?Qt::RoundCap:layer.line_cap=="square"?Qt::SquareCap:Qt::FlatCap;
                    paint.join=layer.line_join=="round"?Qt::RoundJoin:layer.line_join=="bevel"?Qt::BevelJoin:Qt::SvgMiterJoin;
                    paint.miter_limit=layer.miter_limit;
                    for (const auto& instance : layer.paths) {
                        auto found = contour_paths.find(instance.contours.get());
                        if (found == contour_paths.end())
                            found = contour_paths.emplace(instance.contours.get(), qt_path(*instance.contours)).first;
                        paint.path.addPath(qt_transform(instance.transform).map(found->second));
                    }
                    paint.path.setFillRule(layer.fill_rule == "evenodd" ? Qt::OddEvenFill : Qt::WindingFill);
                    if(!paint.fill&&paint.width>0&&!layer.degenerate_subpaths.empty()) {
                        QPainterPath caps;caps.setFillRule(Qt::WindingFill);const auto radius=paint.width/2;
                        for(const auto& center:layer.degenerate_subpaths) {
                            if(paint.cap==Qt::RoundCap)caps.addEllipse(QPointF(center.x,center.y),radius,radius);
                            else caps.addRect(QRectF(center.x-radius,center.y-radius,paint.width,paint.width));
                        }
                        // One filled outline avoids applying translucent/gradient
                        // paint twice where a zero-length cap overlaps other ink.
                        paint.stroke_outline=qt_stroke(paint.path,paint.width,paint.cap,paint.join,paint.miter_limit).united(caps);
                    }
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
        projection_error_ = std::current_exception();
        report_error(exception);
    }
    update();
    if (!previous_artboard.empty() && (previous_composition != active_composition_ || previous_artboard != active_artboard_))
        fit_artboard();
    if ((previous_composition != active_composition_ || previous_artboard != active_artboard_) && active_artboard_changed)
        active_artboard_changed();
}

void Canvas::select_all_in_context() {
    if(drag_!=Drag::none||gesture_owned_||draw_mode_)return;
    std::vector<Selection> selected;
    if(!selected_point.empty()) {
        for(const auto& id:selected_objects())if(const auto* item=geometry(id))
            for(const auto& point:item->points)selected.push_back({id,point.id});
    } else {
        std::set<Id> seen;
        for(const auto& item:geometry_)if(item.normal_visible) {
            const auto target=selection_target(item);
            if(!target.empty()&&seen.insert(target).second)selected.push_back({target,{}});
        }
    }
    select_many(std::move(selected));
}

void Canvas::nudge_selection(double dx,double dy) {
    if(drag_!=Drag::none||gesture_owned_||draw_mode_||anchor_edit_||gradient_control_||selections_.empty())return;
    try {
        std::vector<Command> commands;
        if(selected_point.empty())commands.push_back(TranslateObjects{selected_objects(),dx,dy});
        else {
            for(const auto& selection:selections_) {
                const auto* g=geometry(selection.object);const auto* p=g?point(*g,selection.point):nullptr;
                if(!p)throw Error("MISSING_POINT","Selected point is no longer available");
                bool invertible=false;const auto inverse=g->world.inverted(&invertible);
                if(!invertible)throw Error("SINGULAR_TRANSFORM","Cannot move points through a singular world transform");
                const auto x=inverse.m11()*dx+inverse.m21()*dy,y=inverse.m12()*dx+inverse.m22()*dy;
                if(x!=0)commands.push_back(Set{{selection.object,selection.point,"x"},p->anchor.x()+x});
                if(y!=0)commands.push_back(Set{{selection.object,selection.point,"y"},p->anchor.y()+y});
            }
        }
        if(commands.empty())return;
        session_.apply(commands,session_.revision());refresh();
        if(document_changed)document_changed();
    } catch(const std::exception& exception){report_error(exception);}
}

void Canvas::fit_selection() {
    if(drag_!=Drag::none||gesture_owned_||draw_mode_||selections_.empty())return;
    try {
        std::optional<Bounds> bounds;
        auto include=[&](Bounds b) {
            if(!bounds)bounds=b;
            else {bounds->left=std::min(bounds->left,b.left);bounds->right=std::max(bounds->right,b.right);
                bounds->top=std::min(bounds->top,b.top);bounds->bottom=std::max(bounds->bottom,b.bottom);}
        };
        for(const auto& selection:selections_) {
            if(selection.point.empty()) {
                if(const auto b=object_bounds(session_.document(),selection.object,values_,transforms_,true))include(*b);
            } else if(const auto* g=geometry(selection.object))if(const auto* p=point(*g,selection.point)) {
                const auto world=g->world.map(p->anchor);include({world.x(),world.y(),world.x(),world.y()});
            }
        }
        if(!bounds)return;
        // A point or horizontal/vertical path must frame itself, not fall back
        // to all artwork through QRectF::isEmpty(). One du gives a bounded zoom.
        const auto w=std::max(1.0,bounds->right-bounds->left),h=std::max(1.0,bounds->bottom-bounds->top);
        fit_bounds({bounds->left+(bounds->right-bounds->left-w)/2,bounds->top+(bounds->bottom-bounds->top-h)/2,w,h});
    } catch(const std::exception& exception){report_error(exception);}
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
    if(drag_==Drag::marquee)cancel_interaction();
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
    if(zoom_changed)zoom_changed(zoom_);
}

void Canvas::set_zoom(double zoom) {
    if(!std::isfinite(zoom)||gesture_owned_||drag_==Drag::marquee)return;
    const auto center=QPointF(width()/2.0,height()/2.0);
    const auto world_center=view().inverted().map(center);
    zoom_=std::clamp(zoom,0.02,64.0);
    pan_=center-world_center*zoom_;
    initial_fit_=false;
    request_frame(QStringLiteral("zoom"),true);
    if(zoom_changed)zoom_changed(zoom_);
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
                const auto outline=paint->stroke_outline?*paint->stroke_outline:qt_stroke(paint->path,paint->width,paint->cap,paint->join,paint->miter_limit);
                if (painted_transform.map(outline).contains(screen)) return &*i;
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

QImage Canvas::render_artboard(const Document& document,const Id& composition,const Id& artboard,double scale,bool white_background) {
    if(!std::isfinite(scale)||scale<=0||scale>16)throw Error("EXPORT_SCALE","PNG scale must be greater than zero and at most 16");
    const auto comp=std::find_if(document.compositions.begin(),document.compositions.end(),[&](const auto& c){return c.id==composition;});
    if(comp==document.compositions.end())throw Error("MISSING_COMPOSITION",composition);
    const auto board=evaluate_artboard(*comp,artboard);
    const auto width=std::ceil(board.width*scale),height=std::ceil(board.height*scale);
    if(width<1||height<1||width>8192||height>8192||width*height>16777216)
        throw Error("EXPORT_LIMIT","PNG output is limited to 8192 pixels per axis and 16,777,216 pixels");
    // A committed snapshot avoids exporting a live gesture or changing selection/view.
    // The hidden projection uses exactly the Canvas evaluator and painter; it is
    // never shown and does not create a second editable authority.
    Session snapshot(document);
    Canvas projection(snapshot);
    projection.set_active_artboard(composition,artboard,false);
    projection.refresh();
    if(projection.projection_error_)std::rethrow_exception(projection.projection_error_);
    QImage image(static_cast<int>(width),static_cast<int>(height),QImage::Format_ARGB32_Premultiplied);
    if(image.isNull())throw Error("RENDER_ALLOCATION","Could not allocate PNG output");
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);
        const QTransform transform(scale,0,0,scale,-board.x*scale,-board.y*scale);
        painter.setClipRect(QRectF(0,0,board.width*scale,board.height*scale));
        projection.paint_artwork(painter,transform,image.size(),1);
    }
    if(white_background) {
        QPainter painter(&image);painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);painter.fillRect(image.rect(),Qt::white);
    }
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}

void Canvas::paint_artwork(QPainter& painter,const QTransform& transform,QSizeF size,double dpr) const {
        const auto draw_leaf=[&](QPainter& target,const Geometry& item,QPointF origin=QPointF{}) {
            if(item.image_bounds) {
                target.setWorldTransform(item.world*transform*QTransform::fromTranslate(-origin.x(),-origin.y()));
                target.setRenderHint(QPainter::SmoothPixmapTransform);target.drawImage(*item.image_bounds,item.image);return;
            }
            for (const auto& paint : item.paints) {
                if (paint.color.alphaF() <= 0 || (!paint.fill && paint.width <= 0)) continue;
                target.setWorldTransform(paint.transform * item.world * transform*QTransform::fromTranslate(-origin.x(),-origin.y()));
                if (paint.fill||paint.stroke_outline) {
                    target.setPen(Qt::NoPen);
                    target.setBrush(paint.brush);
                } else {
                    target.setBrush(Qt::NoBrush);
                    QPen pen(paint.brush, paint.width, Qt::SolidLine, paint.cap, paint.join);
                    pen.setMiterLimit(paint.miter_limit);target.setPen(pen);
                }
                target.drawPath(paint.stroke_outline?*paint.stroke_outline:paint.path);
            }
        };
            if(!scene_.requires_compositing) {
                for(const auto& item:geometry_)if(item.normal_visible)draw_leaf(painter,item);
            } else {
                // Artwork has a transparent Composition backdrop. The white
                // artboard and dark workspace are UI, never inputs to a blend.

                const auto pixel_width=std::ceil(size.width()*dpr),pixel_height=std::ceil(size.height()*dpr);
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
                                const auto outline=paint.stroke_outline?*paint.stroke_outline:qt_stroke(paint.path,paint.width,paint.cap,paint.join,paint.miter_limit);
                                // The control hull conservatively contains the
                                // generated stroke curves under affine transforms.
                                result=result.united(transform.map(outline).controlPointRect());
                            }
                        }
                        for(const auto& child:node.children)result=result.united(painted_bounds(child));
                    }
                    bounds.emplace(node.id,result);return result;
                };
                const auto pixel_region=[&](const EvaluatedSceneNode& node,const QRect& parent) {
                    const auto world=painted_bounds(node);if(world.isEmpty())return QRect{};
                    auto screen=transform.mapRect(world);
                    if(!std::isfinite(screen.left())||!std::isfinite(screen.top())||!std::isfinite(screen.right())||!std::isfinite(screen.bottom()))
                        throw Error("RENDER_RANGE","Compositing paint bounds must be finite");
                    // Keep AA coverage and align origins to physical pixels. The
                    // same phase is used by artwork, masks, nested images and DPR.
                    screen=screen.adjusted(-2/dpr,-2/dpr,2/dpr,2/dpr).intersected(QRectF(QPointF{},size));
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
                            {QPainter mask(&coverage);mask.setRenderHint(QPainter::Antialiasing);mask.setWorldTransform(transform*QTransform::fromTranslate(-offset.x(),-offset.y()));
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
        try {
            paint_artwork(painter,view(),size(),devicePixelRatioF());
            paint_layout_overlays(painter,document);
            render_error_.clear();
        } catch(const std::exception& exception) {
            const auto message=QString::fromUtf8(exception.what());
            if(render_error_!=message){render_error_=message;report_error(exception);}
        }
        painter.resetTransform();
        painter.setPen(QPen(QColor(153, 210, 225, 170), 1, Qt::DashLine));
        if (snap_guide_x_) {
            const auto x = view().map(QPointF(*snap_guide_x_, 0)).x();
            painter.drawLine(QPointF(x, 0), QPointF(x, height()));
        }
        if (snap_guide_y_) {
            const auto y = view().map(QPointF(0, *snap_guide_y_)).y();
            painter.drawLine(QPointF(0, y), QPointF(width(), y));
        }
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
        if(drag_==Drag::marquee&&drag_moved_) {
            painter.setPen(QPen(accent,1,Qt::DashLine));painter.setBrush(QColor(accent.red(),accent.green(),accent.blue(),28));
            painter.drawRect(QRectF(press_position_,marquee_position_).normalized());
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
            : tr("Empty-drag: select contained · Shift: extend · Arrows: move · Space: pan · F: fit / Shift+F: selection");
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
        sample.semantic_preview_ms=pending_preview_ms_;sample.projection_ms=pending_projection_ms_;
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

void Canvas::set_snap_enabled(bool enabled) {
    if (enabled == snap_enabled_) return;
    cancel_interaction();
    snap_enabled_ = enabled;
    update();
    if(view_state_changed)view_state_changed();
}

void Canvas::set_snap_guides_enabled(bool enabled) {
    if(enabled==snap_guides_enabled_)return;
    cancel_interaction();snap_guides_enabled_=enabled;update();if(view_state_changed)view_state_changed();
}

void Canvas::set_snap_grid_enabled(bool enabled) {
    if(enabled==snap_grid_enabled_)return;
    cancel_interaction();snap_grid_enabled_=enabled;update();if(view_state_changed)view_state_changed();
}

void Canvas::set_show_guides(bool enabled) {
    if(show_guides_==enabled)return;
    if(!enabled&&drag_==Drag::guide)cancel_interaction();
    show_guides_=enabled;update();if(view_state_changed)view_state_changed();
}

void Canvas::set_show_grid(bool enabled) {
    if(show_grid_==enabled)return;
    show_grid_=enabled;update();if(view_state_changed)view_state_changed();
}

void Canvas::set_show_margin(bool enabled) {
    if(show_margin_==enabled)return;
    show_margin_=enabled;update();
}

void Canvas::set_guide_edit_mode(bool enabled) {
    if(guide_edit_mode_==enabled)return;
    if(!enabled&&drag_==Drag::guide)cancel_interaction();
    if(enabled) {set_draw_mode(false);set_anchor_edit(false);clear_gradient_edit();}
    guide_edit_mode_=enabled;
    update_cursor();update();
}

bool Canvas::guide_context_current() const {
    if(guide_drag_document_.empty()||guide_drag_composition_.empty())return false;
    if(session_.document().id!=guide_drag_document_||session_.revision()!=guide_drag_revision_||
       active_composition_!=guide_drag_composition_)return false;
    if(session_identity_provider_&&session_identity_provider_()!=guide_drag_session_)return false;
    const auto composition=std::find_if(session_.document().compositions.begin(),session_.document().compositions.end(),
        [&](const auto& item){return item.id==guide_drag_composition_;});
    return composition!=session_.document().compositions.end()&&
        std::any_of(composition->guides.begin(),composition->guides.end(),[&](const auto& guide){return guide.id==guide_drag_start_.id;});
}

const Guide* Canvas::hit_guide(QPointF screen) const {
    if(!guide_edit_mode_||!show_guides_||active_composition_.empty())return nullptr;
    const auto& document=session_.preview_document();
    const auto composition=std::find_if(document.compositions.begin(),document.compositions.end(),
        [&](const auto& item){return item.id==active_composition_;});
    if(composition==document.compositions.end())return nullptr;
    const Guide* chosen=nullptr;double best=6.0;
    for(const auto& guide:composition->guides) {
        const auto world=guide.axis=="x"?QPointF(guide.position,0):QPointF(0,guide.position);
        const auto mapped=view().map(world);
        const auto separation=guide.axis=="x"?std::abs(screen.x()-mapped.x()):std::abs(screen.y()-mapped.y());
        if(separation>6.0)continue;
        if(!chosen||separation<best||(separation==best&&guide.id<chosen->id)) {
            chosen=&guide;best=separation;
        }
    }
    return chosen;
}

void Canvas::begin_guide_drag(const Guide& guide,QPointF screen) {
    try {
        guide_drag_start_=guide;guide_drag_inverse_view_=view().inverted();guide_drag_world_start_=guide_drag_inverse_view_.map(screen);
        guide_drag_session_=session_identity_provider_?session_identity_provider_():QString::fromStdString(session_.document().id);
        guide_drag_document_=session_.document().id;guide_drag_composition_=active_composition_;
        guide_drag_revision_=session_.revision();guide_drag_invalid_=false;
        press_position_=screen;press_pan_=pan_;drag_moved_=false;
        session_.begin_gesture(guide_drag_revision_);gesture_owned_=true;drag_=Drag::guide;
        ++input_sequence_;update_cursor();update();
    } catch(const std::exception& exception) {report_error(exception);}
}

void Canvas::paint_layout_overlays(QPainter& painter,const Document& document) const {
    const auto composition=std::find_if(document.compositions.begin(),document.compositions.end(),
        [&](const auto& item){return item.id==active_composition_;});
    if(composition==document.compositions.end())return;
    painter.save();painter.setWorldTransform(view());painter.setBrush(Qt::NoBrush);
    if(show_guides_) {
        const auto visible=view().inverted().mapRect(QRectF(rect()));
        QPen pen(QColor(44,151,205,190),1,Qt::DashLine);pen.setCosmetic(true);
        painter.setPen(pen);
        for(const auto& guide:composition->guides) {
            if(guide.axis=="x")painter.drawLine(QPointF(guide.position,visible.top()),QPointF(guide.position,visible.bottom()));
            else if(guide.axis=="y")painter.drawLine(QPointF(visible.left(),guide.position),QPointF(visible.right(),guide.position));
        }
    }
    for(const auto& source:composition->artboards) {
        const auto board=evaluate_artboard(*composition,source.id);
        if(!board.layout)continue;
        if(show_margin_&&board.id==active_artboard_&&board.layout->margin) {
            const auto& margin=*board.layout->margin;
            const QRectF inset(board.x+margin.left,board.y+margin.top,
                board.width-margin.left-margin.right,board.height-margin.top-margin.bottom);
            QPen pen(QColor(193,126,202,205),1.2,Qt::DashLine);pen.setCosmetic(true);
            painter.setPen(pen);painter.drawRect(inset);
        }
        if(!show_grid_||!board.layout->grid)continue;
        const auto& grid=*board.layout->grid;
        const QRectF bounds(board.x+grid.bounds.x,board.y+grid.bounds.y,grid.bounds.width,grid.bounds.height);
        painter.save();painter.setClipRect(bounds,Qt::IntersectClip);
        QPen border(QColor(91,158,112,220),1.25);border.setCosmetic(true);
        painter.setPen(border);painter.drawRect(bounds);
        QPen cells(QColor(91,158,112,155),1,Qt::SolidLine);cells.setCosmetic(true);
        painter.setPen(cells);
        if(grid.columns>0) {
            const auto cell=(grid.bounds.width-static_cast<double>(grid.columns-1)*grid.column_gutter)/static_cast<double>(grid.columns);
            for(std::size_t i=1;i<grid.columns;++i) {
                const auto cell_end=grid.bounds.x+static_cast<double>(i)*cell+static_cast<double>(i-1)*grid.column_gutter;
                painter.drawLine(QPointF(board.x+cell_end,bounds.top()),QPointF(board.x+cell_end,bounds.bottom()));
                if(grid.column_gutter>0)painter.drawLine(QPointF(board.x+cell_end+grid.column_gutter,bounds.top()),QPointF(board.x+cell_end+grid.column_gutter,bounds.bottom()));
            }
        }
        if(grid.rows>0) {
            const auto cell=(grid.bounds.height-static_cast<double>(grid.rows-1)*grid.row_gutter)/static_cast<double>(grid.rows);
            for(std::size_t i=1;i<grid.rows;++i) {
                const auto cell_end=grid.bounds.y+static_cast<double>(i)*cell+static_cast<double>(i-1)*grid.row_gutter;
                painter.drawLine(QPointF(bounds.left(),board.y+cell_end),QPointF(bounds.right(),board.y+cell_end));
                if(grid.row_gutter>0)painter.drawLine(QPointF(bounds.left(),board.y+cell_end+grid.row_gutter),QPointF(bounds.right(),board.y+cell_end+grid.row_gutter));
            }
        }
        painter.restore();
    }
    painter.restore();
}

void Canvas::prepare_snap(bool point_drag) {
    snap_bounds_.reset();
    snap_x_sources_.clear(); snap_y_sources_.clear();
    snap_x_targets_.clear(); snap_y_targets_.clear();
    snap_x_match_.reset(); snap_y_match_.reset();
    snap_point_world_.reset(); snap_guide_x_.reset(); snap_guide_y_.reset();
    publish_snap_feedback({});
    snap_point_mode_=point_drag; snap_start_zoom_=zoom_;
    const auto& document=session_.document();
    snap_document_=document.id; snap_composition_=active_composition_; snap_scope_=scope_;
    snap_revision_=session_.revision();
    snap_session_=session_identity_provider_?session_identity_provider_():QString::fromStdString(document.id);
    snap_prepared_=true;
    if(!snap_enabled_)return;

    const auto selected=selected_objects();
    const std::set<Id> selection(selected.begin(),selected.end());
    // Effective followers are part of the frozen moving set even when the
    // shared TranslateObjects solver reaches them outside structural ancestry.
    std::map<Id,bool> moving;
    std::function<bool(const Id&)> moves=[&](const Id& id) {
        if(id.empty())return false;
        if(const auto found=moving.find(id);found!=moving.end())return found->second;
        const auto transform=transforms_.find(id);
        const bool result=selection.contains(id)||
            (transform!=transforms_.end()&&moves(transform->second.effective_parent));
        moving.emplace(id,result);return result;
    };
    const auto unite=[](std::optional<QRectF>& result,QRectF area) {
        if(!result)result=area;
        else *result=QRectF(QPointF(std::min(result->left(),area.left()),std::min(result->top(),area.top())),
                            QPointF(std::max(result->right(),area.right()),std::max(result->bottom(),area.bottom())));
    };
    const auto geometry_bounds=[&](const Geometry& item)->std::optional<QRectF> {
        std::optional<QRectF> bounds;
        if(item.image_bounds)bounds=item.world.mapRect(*item.image_bounds);
        else {
            const auto path=[&](const PathInstance& instance,QTransform parent) {
                const auto transform=qt_transform(instance.transform)*parent;
                if(instance.contours&&!instance.contours->empty())
                    unite(bounds,transform.map(qt_path(*instance.contours)).boundingRect());
                if(item.text_bounds)unite(bounds,transform.mapRect(*item.text_bounds));
            };
            const auto shape=scene_.shapes.find(item.id);
            if(shape==scene_.shapes.end())return bounds;
            for(const auto& instance:shape->second.paths)path(instance,item.world);
            for(const auto& paint:shape->second.paints)
                for(const auto& instance:paint.paths)path(instance,qt_transform(paint.transform)*item.world);
        }
        return bounds;
    };
    const auto add_bound_candidates=[&](const Id& id,QRectF bounds,SnapKind kind) {
        const std::array<std::pair<double,const char*>,3> xs{{{bounds.left(),"min"},{bounds.center().x(),"center"},{bounds.right(),"max"}}};
        const std::array<std::pair<double,const char*>,3> ys{{{bounds.top(),"min"},{bounds.center().y(),"center"},{bounds.bottom(),"max"}}};
        for(std::size_t i=0;i<xs.size();++i)if(std::isfinite(xs[i].first))
            if(kind!=SnapKind::object_edge||i!=1)
                snap_x_targets_.push_back({xs[i].first,kind,id,QString::fromLatin1(xs[i].second),static_cast<int>(i)});
        for(std::size_t i=0;i<ys.size();++i)if(std::isfinite(ys[i].first))
            if(kind!=SnapKind::object_edge||i!=1)
                snap_y_targets_.push_back({ys[i].first,kind,id,QString::fromLatin1(ys[i].second),static_cast<int>(i)});
    };

    std::map<Id,QRectF> target_bounds;
    std::vector<std::pair<const Geometry*,QRectF>> visible_geometry;
    for(const auto& item:geometry_) {
        if(!item.normal_visible)continue;
        const auto bounds=geometry_bounds(item);
        if(!bounds||!std::isfinite(bounds->left())||!std::isfinite(bounds->top())||
           !std::isfinite(bounds->right())||!std::isfinite(bounds->bottom()))continue;
        visible_geometry.emplace_back(&item,*bounds);
        const bool in_scope=scope_.empty()||std::find(item.ancestors.begin(),item.ancestors.end(),scope_)!=item.ancestors.end();
        const auto key=in_scope?selection_target(item):Id{};
        if(key.empty())continue;
        const bool belongs=selection.contains(item.id)||std::any_of(item.ancestors.begin(),item.ancestors.end(),
            [&](const Id& id){return selection.contains(id);});
        if(moves(item.id)) {
            if(belongs) {if(snap_bounds_)unite(snap_bounds_,*bounds);else snap_bounds_=*bounds;}
        } else {
            auto found=target_bounds.find(key);
            if(found==target_bounds.end())target_bounds.emplace(key,*bounds);
            else {
                std::optional<QRectF> combined=found->second;unite(combined,*bounds);found->second=*combined;
            }
        }
    }

    if(point_drag) {
        if(point_starts_.empty())return;
        const auto active=std::find_if(point_starts_.begin(),point_starts_.end(),[&](const auto& item) {
            return item.target.object==selected_object&&item.target.point==selected_point;
        });
        if(active==point_starts_.end())return;
        const auto* geometry_item=geometry(active->target.object);
        if(!geometry_item||parents_.at(active->target.object)!=scope_)return;
        snap_point_world_=geometry_item->world.map(active->anchor);
        snap_x_sources_.push_back({snap_point_world_->x(),1,QStringLiteral("point anchor"),SnapSourceKind::point_anchor});
        snap_y_sources_.push_back({snap_point_world_->y(),1,QStringLiteral("point anchor"),SnapSourceKind::point_anchor});
    } else if(snap_bounds_) {
        for(const auto& [position,label,order]:std::array<std::tuple<double,const char*,int>,3>{{
            {snap_bounds_->left(),"min",0},{snap_bounds_->center().x(),"center",1},{snap_bounds_->right(),"max",2}}})
            snap_x_sources_.push_back({position,order,QString::fromLatin1(label),SnapSourceKind::geometry_bounds});
        for(const auto& [position,label,order]:std::array<std::tuple<double,const char*,int>,3>{{
            {snap_bounds_->top(),"min",0},{snap_bounds_->center().y(),"center",1},{snap_bounds_->bottom(),"max",2}}})
            snap_y_sources_.push_back({position,order,QString::fromLatin1(label),SnapSourceKind::geometry_bounds});
    }

    const auto composition=std::find_if(document.compositions.begin(),document.compositions.end(),
        [&](const auto& item){return item.id==active_composition_;});
    if(composition==document.compositions.end())return;
    if(snap_guides_enabled_)for(const auto& guide:composition->guides) {
        if(!std::isfinite(guide.position))continue;
        SnapCandidate candidate{guide.position,SnapKind::guide,guide.id,QStringLiteral("guide line"),0};
        if(guide.axis=="x")snap_x_targets_.push_back(candidate);
        else if(guide.axis=="y")snap_y_targets_.push_back(candidate);
    }

    for(const auto& source:composition->artboards) {
        const auto board=evaluate_artboard(*composition,source.id);
        const QRectF bounds(board.x,board.y,board.width,board.height);
        add_bound_candidates(board.id,bounds,SnapKind::artboard);
        if(!snap_grid_enabled_||!board.layout||!board.layout->grid)continue;
        const auto& grid=*board.layout->grid;
        const auto grid_id=grid.id;
        const auto add_grid_axis=[&](bool x_axis) {
            const double local_start=x_axis?grid.bounds.x:grid.bounds.y;
            const double extent=x_axis?grid.bounds.width:grid.bounds.height;
            const double gutter=x_axis?grid.column_gutter:grid.row_gutter;
            const auto count=x_axis?grid.columns:grid.rows;
            if(count==0||!std::isfinite(extent)||!std::isfinite(gutter))return;
            const double cell=(extent-static_cast<double>(count-1)*gutter)/static_cast<double>(count);
            if(!(cell>0)||!std::isfinite(cell))return;
            struct GridFeature {double position;bool center;QString name;};
            std::vector<GridFeature> features;
            const double origin=(x_axis?board.x:board.y)+local_start;
            for(std::size_t index=0;index<count;++index) {
                const double start=origin+static_cast<double>(index)*(cell+gutter);
                const double end=start+cell;
                features.push_back({start,false,QStringLiteral("%1 %2 boundary").arg(x_axis?QStringLiteral("column"):QStringLiteral("row")).arg(index+1)});
                features.push_back({start+cell/2,true,QStringLiteral("%1 %2 center").arg(x_axis?QStringLiteral("column"):QStringLiteral("row")).arg(index+1)});
                features.push_back({end,false,QStringLiteral("%1 %2 boundary").arg(x_axis?QStringLiteral("column"):QStringLiteral("row")).arg(index+1)});
            }
            std::sort(features.begin(),features.end(),[](const auto& lhs,const auto& rhs) {
                if(lhs.position!=rhs.position)return lhs.position<rhs.position;
                if(lhs.center!=rhs.center)return !lhs.center;
                return lhs.name<rhs.name;
            });
            auto& output=x_axis?snap_x_targets_:snap_y_targets_;
            double prior=std::numeric_limits<double>::quiet_NaN();int order=0;
            for(const auto& feature:features) {
                if(feature.position==prior)continue;
                prior=feature.position;
                output.push_back({feature.position,SnapKind::grid,grid_id,feature.name,order++});
            }
        };
        add_grid_axis(true);add_grid_axis(false);
    }

    for(const auto& [id,bounds]:target_bounds) {
        add_bound_candidates(id,bounds,SnapKind::object_edge);
        for(const auto position:{bounds.center().x()})
            snap_x_targets_.push_back({position,SnapKind::object_center,id,QStringLiteral("center"),1});
        snap_y_targets_.push_back({bounds.center().y(),SnapKind::object_center,id,QStringLiteral("center"),1});
    }

    const auto baseline_world=[&](const Geometry& item,bool x_axis)->std::vector<double> {
        const auto object=document.objects.find(item.id);
        if(object==document.objects.end()||!object->second.text||object->second.text->direction!="horizontal"||
           item.text_line_baselines_y.empty())return {};
        const auto shape=scene_.shapes.find(item.id);
        if(shape==scene_.shapes.end()||shape->second.paths.size()!=1)return {};
        const auto transform=qt_transform(shape->second.paths.front().transform)*item.world;
        constexpr double epsilon=1e-10;
        if(x_axis) {
            // A quarter-turn maps the measured horizontal line to a vertical
            // world line, whose fixed coordinate is X.
            if(std::abs(transform.m11())>epsilon||std::abs(transform.m22())>epsilon||
               std::abs(transform.m12())<=epsilon||std::abs(transform.m21())<=epsilon)return {};
        } else if(std::abs(transform.m12())>epsilon||std::abs(transform.m21())>epsilon||
                  std::abs(transform.m11())<=epsilon||std::abs(transform.m22())<=epsilon)return {};
        std::vector<double> baselines;
        for(const auto local:item.text_line_baselines_y) {
            const auto point=transform.map(QPointF(0,local));
            const auto baseline=x_axis?point.x():point.y();
            if(std::isfinite(baseline))baselines.push_back(baseline);
        }
        return baselines;
    };
    if(!point_drag&&selected.size()==1&&selection.contains(selected.front())) {
        const auto* source=geometry(selected.front());
        if(source&&parents_.at(source->id)==scope_) {
            for(const bool x_axis:{true,false}) {
                const auto baselines=baseline_world(*source,x_axis);
                auto& output=x_axis?snap_x_sources_:snap_y_sources_;
                for(std::size_t line=0;line<baselines.size();++line)
                    output.push_back({baselines[line],static_cast<int>(line)+1,
                        line==0?QStringLiteral("first-line baseline"):QStringLiteral("line %1 baseline").arg(line+1),
                        SnapSourceKind::text_line_baseline});
            }
        }
    }
    for(const auto& [item,bounds]:visible_geometry) {
        const auto key=selection_target(*item);
        if(key.empty()||key!=item->id||moves(item->id)||parents_.at(item->id)!=scope_)continue;
        for(const bool x_axis:{true,false}) {
            const auto baselines=baseline_world(*item,x_axis);
            auto& output=x_axis?snap_x_targets_:snap_y_targets_;
            for(std::size_t line=0;line<baselines.size();++line)
                output.push_back({baselines[line],SnapKind::text_baseline,item->id,
                    line==0?QStringLiteral("first-line baseline"):QStringLiteral("line %1 baseline").arg(line+1),
                    static_cast<int>(line)});
        }
    }

    if(!point_drag&&snap_bounds_) {
        const auto make_equal_gap=[&](bool x_axis) {
            if(target_bounds.size()<2)return;
            const double source_size=x_axis?snap_bounds_->width():snap_bounds_->height();
            std::vector<std::pair<Id,QRectF>> ordered(target_bounds.begin(),target_bounds.end());
            std::sort(ordered.begin(),ordered.end(),[&](const auto& lhs,const auto& rhs) {
                const double a=x_axis?lhs.second.left():lhs.second.top();
                const double b=x_axis?rhs.second.left():rhs.second.top();
                return a==b?lhs.first<rhs.first:a<b;
            });
            auto& output=x_axis?snap_x_targets_:snap_y_targets_;
            for(std::size_t i=0;i+1<ordered.size();++i) {
                const auto& left=ordered[i];const auto& right=ordered[i+1];
                const double trailing=x_axis?left.second.right():left.second.bottom();
                const double leading=x_axis?right.second.left():right.second.top();
                if(leading<trailing)continue;
                const double gap=leading-trailing;
                if(!std::isfinite(gap))continue;
                const auto id=left.first+"\x1f"+right.first;
                const auto axis_min=[&](const QRectF& bounds) {return x_axis?bounds.left():bounds.top();};
                const auto axis_max=[&](const QRectF& bounds) {return x_axis?bounds.right():bounds.bottom();};
                const auto add_if_clear=[&](double target_position,int source_feature_order,int feature_order,
                                            const QString& feature) {
                    const double proposed_min=source_feature_order==0?target_position:target_position-source_size;
                    const double proposed_max=proposed_min+source_size;
                    if(!std::isfinite(proposed_min)||!std::isfinite(proposed_max))return;
                    for(const auto& [other_id,other_bounds]:target_bounds) {
                        if(other_id==left.first||other_id==right.first)continue;
                        if(axis_min(other_bounds)<proposed_max&&proposed_min<axis_max(other_bounds))return;
                    }
                    output.push_back({target_position,SnapKind::equal_gap,id,feature,feature_order,source_feature_order});
                };

                // Preserve the existing centered placement when the moving bounds
                // fit inside the gap between the two stationary targets.
                if(gap>=source_size) {
                    const double target_min=(trailing+leading-source_size)/2;
                    if(std::isfinite(target_min))
                        output.push_back({target_min,SnapKind::equal_gap,id,
                            QStringLiteral("equal gap between %1 and %2").arg(QString::fromStdString(left.first),QString::fromStdString(right.first)),0,0});
                }

                // Reuse the same gap on either side of the pair. Each side is
                // constrained to its corresponding frozen source edge.
                const auto after=axis_max(right.second)+gap;
                const auto before=axis_min(left.second)-gap;
                add_if_clear(after,0,2,
                    QStringLiteral("repeated gap between %1 and %2 after %2")
                        .arg(QString::fromStdString(left.first),QString::fromStdString(right.first)));
                add_if_clear(before,2,1,
                    QStringLiteral("repeated gap between %1 and %2 before %1")
                        .arg(QString::fromStdString(left.first),QString::fromStdString(right.first)));
            }
        };
        make_equal_gap(true);make_equal_gap(false);
    }

}

bool Canvas::snap_context_current() const {
    if(!snap_prepared_||session_.document().id!=snap_document_||session_.revision()!=snap_revision_||
       active_composition_!=snap_composition_||scope_!=snap_scope_||zoom_!=snap_start_zoom_)return false;
    return !session_identity_provider_||session_identity_provider_()==snap_session_;
}

void Canvas::publish_snap_feedback(QString message) {
    if(message==snap_feedback_text_)return;
    snap_feedback_text_=std::move(message);
    if(snap_feedback)snap_feedback(snap_feedback_text_);
}

QPointF Canvas::snap_delta(QPointF delta) {
    snap_guide_x_.reset();snap_guide_y_.reset();snap_x_match_.reset();snap_y_match_.reset();
    if(!snap_prepared_||!snap_context_current())
        throw Error("REVISION_CONFLICT","Snap gesture belongs to an older document session, scope, zoom, or revision");
    if(!snap_enabled_)return delta;
    if(snap_point_mode_&&!snap_point_world_)return delta;
    if(!snap_point_mode_&&!snap_bounds_)return delta;

    const auto priority=[](SnapKind kind) {
        switch(kind) {
        case SnapKind::guide:return 0;
        case SnapKind::grid:return 1;
        case SnapKind::artboard:return 2;
        case SnapKind::text_baseline:return 3;
        case SnapKind::object_edge:return 4;
        case SnapKind::object_center:return 5;
        case SnapKind::equal_gap:return 6;
        }
        return 7;
    };
    const auto kind_name=[](SnapKind kind) {
        switch(kind) {
        case SnapKind::guide:return QStringLiteral("Guide");
        case SnapKind::grid:return QStringLiteral("Grid");
        case SnapKind::artboard:return QStringLiteral("Artboard");
        case SnapKind::text_baseline:return QStringLiteral("Text baseline");
        case SnapKind::object_edge:return QStringLiteral("Object edge");
        case SnapKind::object_center:return QStringLiteral("Object center");
        case SnapKind::equal_gap:return QStringLiteral("Equal gap");
        }
        return QStringLiteral("Snap");
    };
    const auto choose=[&](const std::vector<SnapSourceFeature>& sources,
                         const std::vector<SnapCandidate>& targets,double raw,
                         std::optional<SnapMatch>& chosen)->double {
        auto better=[&](const SnapMatch& candidate,const SnapMatch& current) {
            if(candidate.distance!=current.distance)return candidate.distance<current.distance;
            const auto candidate_priority=priority(candidate.target.kind);
            const auto current_priority=priority(current.target.kind);
            if(candidate_priority!=current_priority)return candidate_priority<current_priority;
            if(candidate.target.target_id!=current.target.target_id)return candidate.target.target_id<current.target.target_id;
            if(candidate.source.order!=current.source.order)return candidate.source.order<current.source.order;
            if(candidate.target.target_feature_order!=current.target.target_feature_order)
                return candidate.target.target_feature_order<current.target.target_feature_order;
            if(candidate.target.target_feature!=current.target.target_feature)
                return candidate.target.target_feature<current.target.target_feature;
            if(candidate.source.label!=current.source.label)return candidate.source.label<current.source.label;
            return candidate.target.position<current.target.position;
        };
        for(const auto& source:sources)for(const auto& target:targets) {
            if(target.kind==SnapKind::text_baseline&&source.kind!=SnapSourceKind::text_line_baseline)continue;
            // The first-line source already participates in ordinary Snap. Keep
            // that behavior; additional measured lines only target Text baselines.
            if(target.kind!=SnapKind::text_baseline&&source.kind==SnapSourceKind::text_line_baseline&&source.order>1)continue;
            if(target.kind==SnapKind::equal_gap&&source.order!=target.source_feature_order)continue;
            const double source_position=source.position+raw;
            const double correction=target.position-source_position;
            const double distance=std::abs(correction);
            if(!std::isfinite(distance)||distance*snap_start_zoom_>6.0)continue;
            SnapMatch candidate{target,source,correction,distance};
            if(!chosen||better(candidate,*chosen))chosen=std::move(candidate);
        }
        return raw+(chosen?chosen->correction:0);
    };
    const auto source_offset=[&](bool x_axis) {
        return x_axis?delta.x():delta.y();
    };
    const auto adjusted_x=choose(snap_x_sources_,snap_x_targets_,source_offset(true),snap_x_match_);
    const auto adjusted_y=choose(snap_y_sources_,snap_y_targets_,source_offset(false),snap_y_match_);
    if(snap_x_match_)snap_guide_x_=snap_x_match_->target.position;
    if(snap_y_match_)snap_guide_y_=snap_y_match_->target.position;

    QStringList feedback;
    const auto describe=[&](const QString& axis,const SnapMatch& match) {
        if(match.target.kind==SnapKind::equal_gap)
            return QStringLiteral("%1: %2 %3 → %4")
                .arg(axis,match.source.label,kind_name(match.target.kind),match.target.target_feature);
        const auto target_id=QString::fromStdString(match.target.target_id);
        return QStringLiteral("%1: %2 %3 → %4 %5")
            .arg(axis,match.source.label,kind_name(match.target.kind),target_id,match.target.target_feature);
    };
    if(snap_x_match_)feedback.push_back(describe(QStringLiteral("X"),*snap_x_match_));
    if(snap_y_match_)feedback.push_back(describe(QStringLiteral("Y"),*snap_y_match_));
    publish_snap_feedback(feedback.join(QStringLiteral(" · ")));
    return {adjusted_x,adjusted_y};
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
                if(!selected_anchor)throw Error("INVALID_SELECTION","A selected point no longer exists in the active gesture scope");
                if(parents_.at(selected.object)!=scope_)
                    throw Error("INVALID_SELECTION","Point Snap requires all selected anchors in the active Group scope");
                bool valid=false;const auto inverse=g->world.inverted(&valid);
                if(!valid)throw Error("SINGULAR_TRANSFORM","Cannot drag points through a singular object transform");
                point_starts_.push_back({selected,selected_anchor->anchor,inverse});
            }
        }
        if (!invertible) throw Error("SINGULAR_TRANSFORM", "Cannot drag through a singular parent/object transform");
        if(kind==Drag::object)prepare_snap(false);
        else if(kind==Drag::anchor)prepare_snap(true);
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
    if(drag_==Drag::guide) {
        if(!guide_context_current()) {
            cancel_interaction();report_error(Error("REVISION_CONFLICT","Guide drag belongs to an older document session or revision"));return;
        }
        if(!drag_moved_&&distance(screen,press_position_)<QApplication::startDragDistance())return;
        drag_moved_=true;request_frame(QStringLiteral("guide"));
        const auto world=guide_drag_inverse_view_.map(screen);
        auto changed=guide_drag_start_;
        changed.position=guide_drag_start_.position+(guide_drag_start_.axis=="x"
            ?world.x()-guide_drag_world_start_.x():world.y()-guide_drag_world_start_.y());
        try {
            session_.update_gesture({UpdateGuide{guide_drag_composition_,changed}});
            guide_drag_invalid_=false;refresh();
        } catch(const std::exception& exception) {
            guide_drag_invalid_=true;report_error(exception);
        }
        update();return;
    }
    if (drag_ == Drag::marquee) {
        marquee_position_=screen;
        if(distance(screen,press_position_)>=QApplication::startDragDistance())drag_moved_=true;
        request_frame(QStringLiteral("selection"));update();return;
    }
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
            const auto world_delta=snap_delta(world-start);
            for(const auto& item:point_starts_) {
                const auto delta=item.inverse.map(world_delta)-item.inverse.map(QPointF{});
                if(std::abs(delta.x())>1e-10)commands.push_back(Set{{item.target.object,item.target.point,"x"},item.anchor.x()+delta.x()});
                if(std::abs(delta.y())>1e-10)commands.push_back(Set{{item.target.object,item.target.point,"y"},item.anchor.y()+delta.y()});
            }
        } else if (drag_ == Drag::object) {
            const auto world_delta = snap_delta((screen - press_position_) / zoom_);
            if(std::abs(world_delta.x())>1e-10||std::abs(world_delta.y())>1e-10)
                commands.push_back(TranslateObjects{selected_objects(),world_delta.x(),world_delta.y()});
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
        const auto preview_start=clock_.nsecsElapsed();
        session_.update_gesture(commands);
        const auto projection_start=clock_.nsecsElapsed();pending_preview_ms_+=(projection_start-preview_start)/1e6;
        refresh();pending_projection_ms_+=(clock_.nsecsElapsed()-projection_start)/1e6;
    } catch (const std::exception& exception) {
        cancel_interaction();
        report_error(exception);
    }
}

void Canvas::finish_marquee() {
    const auto rectangle=QRectF(press_position_,marquee_position_).normalized();
    auto selected=marquee_extend_?marquee_start_:std::vector<Selection>{};
    auto include=[&](Selection item){if(std::find(selected.begin(),selected.end(),item)==selected.end())selected.push_back(std::move(item));};
    if(drag_moved_) {
        if(!marquee_start_.empty()&&!marquee_start_.back().point.empty()) {
            std::set<Id> objects;for(const auto& item:marquee_start_)objects.insert(item.object);
            for(const auto& id:objects)if(const auto* g=geometry(id))
                for(const auto& p:g->points)if(rectangle.contains((g->world*view()).map(p.anchor)))include({id,p.id});
        } else {
            std::set<Id> candidates;
            for(const auto& item:geometry_)if(item.normal_visible) {
                const auto target=selection_target(item);if(!target.empty())candidates.insert(target);
            }
            for(const auto& id:candidates)if(const auto b=object_bounds(session_.document(),id,values_,transforms_,true)) {
                const auto low=view().map(QPointF(b->left,b->top)),high=view().map(QPointF(b->right,b->bottom));
                if(rectangle.contains(low)&&rectangle.contains(high))include({id,{}});
            }
        }
    }
    drag_=Drag::none;marquee_start_.clear();select_many(std::move(selected));update();update_cursor();
}

void Canvas::finish_drag() {
    if(drag_==Drag::marquee) {
        try {finish_marquee();}catch(const std::exception& exception){cancel_interaction();report_error(exception);}
        return;
    }
    if(drag_==Drag::guide) {
        if(!guide_context_current()) {
            cancel_interaction();report_error(Error("REVISION_CONFLICT","Guide drag belongs to an older document session or revision"));return;
        }
        if(guide_drag_invalid_) {
            cancel_interaction();report_error(Error("INVALID_GUIDE","The last Guide position was invalid; the drag was canceled"));return;
        }
    }
    if((drag_==Drag::object||drag_==Drag::anchor)&&snap_prepared_&&!snap_context_current()) {
        cancel_interaction();
        report_error(Error("REVISION_CONFLICT","Snap gesture belongs to an older document session, scope, zoom, or revision"));
        return;
    }
    snap_guide_x_.reset(); snap_guide_y_.reset();
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
    snap_prepared_=false;snap_point_mode_=false;snap_point_world_.reset();snap_bounds_.reset();
    snap_x_sources_.clear();snap_y_sources_.clear();snap_x_targets_.clear();snap_y_targets_.clear();
    snap_x_match_.reset();snap_y_match_.reset();publish_snap_feedback({});
    update_cursor();
}

void Canvas::cancel_interaction() {
    snap_guide_x_.reset(); snap_guide_y_.reset();
    snap_prepared_=false;snap_point_mode_=false;snap_point_world_.reset();snap_bounds_.reset();
    snap_x_sources_.clear();snap_y_sources_.clear();snap_x_targets_.clear();snap_y_targets_.clear();
    snap_x_match_.reset();snap_y_match_.reset();publish_snap_feedback({});
    if (gesture_owned_ && session_.gesture_active()) session_.cancel_gesture();
    const bool had_marquee=drag_==Drag::marquee;marquee_start_.clear();
    const bool had_preview = gesture_owned_;
    gesture_owned_ = false;
    drag_ = Drag::none;
    drag_moved_ = false;
    guide_drag_invalid_=false;guide_drag_document_.clear();guide_drag_composition_.clear();guide_drag_session_.clear();
    update_cursor();
    if (had_preview) refresh();
    else if(had_marquee)update();
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
    if(const auto* guide=hit_guide(event->position())) {
        begin_guide_drag(*guide,event->position());event->accept();return;
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
        if(anchor_edit_||gradient_control_) {if(!extend)select({});}
        else {
            drag_=Drag::marquee;press_position_=marquee_position_=event->position();
            marquee_start_=selections_;marquee_extend_=extend;drag_moved_=false;
            ++input_sequence_;update_cursor();
        }
    }
    event->accept();
}

void Canvas::mouseMoveEvent(QMouseEvent* event) {
    if (drag_ != Drag::none) update_drag(event->position());
    else if (!space_down_ && !draw_mode_) {
        const auto hit = hit_control(event->position());
        setCursor((guide_edit_mode_&&hit_guide(event->position()))||hit.kind != Drag::none ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    event->accept();
}

void Canvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        if(drag_==Drag::marquee)update_drag(event->position());
        finish_drag();
    }
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
    if (gesture_owned_||drag_==Drag::marquee) { event->accept(); return; }
    const auto now = clock_.nsecsElapsed();
    request_frame(QStringLiteral("zoom"), last_wheel_ns_ < 0 || now - last_wheel_ns_ > 250000000);
    last_wheel_ns_ = now;
    const auto cursor = event->position();
    const auto under_cursor = view().inverted().map(cursor);
    const auto delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->pixelDelta().y();
    zoom_ = std::clamp(zoom_ * std::pow(1.0015, delta), 0.02, 64.0);
    pan_ = cursor - under_cursor * zoom_;
    update();
    if(zoom_changed)zoom_changed(zoom_);
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
    } else if ((event->key()==Qt::Key_Left||event->key()==Qt::Key_Right||event->key()==Qt::Key_Up||event->key()==Qt::Key_Down)&&
        (event->modifiers()==Qt::NoModifier||event->modifiers()==Qt::ShiftModifier)) {
        const double step=event->modifiers()==Qt::ShiftModifier?10.0:1.0;
        nudge_selection(event->key()==Qt::Key_Left?-step:event->key()==Qt::Key_Right?step:0,
            event->key()==Qt::Key_Up?-step:event->key()==Qt::Key_Down?step:0);
    } else if (event->matches(QKeySequence::SelectAll)) {
        select_all_in_context();
    } else if (event->key() == Qt::Key_F && event->modifiers() == Qt::ShiftModifier) {
        fit_selection();
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
    if (input_started_ns_ < 0) {input_started_ns_ = clock_.nsecsElapsed();pending_preview_ms_=0;pending_projection_ms_=0;}
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
    else if (draw_mode_ || anchor_edit_ || guide_edit_mode_ || drag_ == Drag::marquee || drag_ == Drag::anchor || drag_ == Drag::incoming ||
             drag_ == Drag::outgoing || drag_ == Drag::symmetric || drag_ == Drag::gradient_start ||
             drag_ == Drag::gradient_end) setCursor(Qt::CrossCursor);
    else if (drag_ == Drag::object) setCursor(Qt::SizeAllCursor);
    else setCursor(Qt::ArrowCursor);
}

} // namespace nect::desktop
