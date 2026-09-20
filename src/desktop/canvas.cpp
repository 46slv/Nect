#include "canvas.hpp"

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QShowEvent>
#include <QStringList>
#include <QUuid>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

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

// Qt maps row vectors; local * parent applies the local matrix first.
QTransform local_transform(const Id& id, const std::map<Ref, double>& values) {
    auto get = [&](const char* field) { return values.at({id, {}, field}); };
    return {get("transform.a"), get("transform.b"), get("transform.c"),
            get("transform.d"), get("transform.tx"), get("transform.ty")};
}
QTransform qt_transform(const Affine& matrix) {
    return {matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]};
}
QPainterPath qt_path(const std::vector<EvaluatedContour>& contours) {
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
            result.closeSubpath();
        }
    }
    return result;
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
    const auto found = std::find_if(geometry_.begin(), geometry_.end(),
                                  [&](const auto& item) { return item.id == id; });
    return found == geometry_.end() ? nullptr : &*found;
}

const Canvas::EvaluatedPoint* Canvas::point(const Geometry& item, const Id& id) const {
    const auto found = std::find_if(item.points.begin(), item.points.end(),
                                  [&](const auto& item_point) { return item_point.id == id; });
    return found == item.points.end() ? nullptr : &*found;
}

void Canvas::refresh() {
    const auto& document = session_.preview_document();
    try {
        values_ = evaluate(document);
        geometry_.clear();
        world_.clear();
        parents_.clear();
        if (!document.compositions.empty()) {
            std::function<void(const Id&, const QTransform&, std::vector<Id>)> visit;
            visit = [&](const Id& id, const QTransform& parent, std::vector<Id> ancestors) {
                const auto& object = document.objects.at(id);
                const auto world = local_transform(id, values_) * parent;
                world_.emplace(id, world);
                parents_.emplace(id, ancestors.empty() ? Id{} : ancestors.back());
                if (object.kind == Kind::group) {
                    ancestors.push_back(id);
                    for (const auto& child : object.children) visit(child, world, ancestors);
                    return;
                }
                Geometry item;
                item.id = id;
                item.ancestors = std::move(ancestors);
                item.world = world;
                auto value = [&](const Id& point_id, const char* field) {
                    return values_.at({id, point_id, field});
                };
                // Core owns operation order, repeat instances and paint grouping.
                // Qt only projects each evaluated layer into its drawing types.
                const auto shape = evaluate_shape(document, id, values_);
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
                for (const auto& contour : path_contours(object)) {
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
                        p.driven = authored.x.binding.has_value() || authored.y.binding.has_value();
                        // Generated topology contains placeholder Scalars. Only
                        // enabled authored coordinate corrections drive anchors;
                        // inspect those directly without evaluating per property.
                        if (object.source && object.point_edit && object.point_edit->enabled) {
                            const auto correction = object.point_edit->overrides.find(p.id);
                            if (correction != object.point_edit->overrides.end()) {
                                for (const auto* field : {"x", "y"}) {
                                    const auto coordinate = correction->second.find(field);
                                    if (coordinate != correction->second.end() && coordinate->second.binding)
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
                geometry_.push_back(std::move(item));
            };
            for (const auto& root : document.compositions.front().roots) visit(root, {}, {});
        }

        if (!scope_.empty() && (!world_.contains(scope_) ||
            document.objects.at(scope_).kind != Kind::group)) set_scope({});
        if (!selected_object.empty() && !world_.contains(selected_object)) select({});
        if (!selected_point.empty()) {
            const auto* item = geometry(selected_object);
            if (!item || !point(*item, selected_point)) select(selected_object);
        }
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
}

void Canvas::fit_artboard() {
    const auto& document = session_.preview_document();
    if (document.compositions.empty()) return;
    QRectF bounds;
    for (const auto& artboard : document.compositions.front().artboards)
        bounds = bounds.united({artboard.x, artboard.y, artboard.width, artboard.height});
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

void Canvas::set_scope(Id scope) {
    if (scope == scope_) return;
    scope_ = std::move(scope);
    if (scope_changed) scope_changed();
    update();
}

void Canvas::select(Id object, Id point_id, bool enter_parent) {
    if (!object.empty() && !world_.contains(object)) object.clear();
    if (object.empty()) point_id.clear();
    if (enter_parent) set_scope(object.empty() ? Id{} : parents_.at(object));
    if (selected_object == object && selected_point == point_id) return;
    if (object != gradient_object_ || !point_id.empty()) clear_gradient_edit();
    selected_object = std::move(object);
    selected_point = std::move(point_id);
    if (selection_changed) selection_changed();
    update();
}

void Canvas::set_selection(Id object, Id point_id) {
    cancel_interaction();
    // A caller may select the object immediately after its core Create command,
    // before the window-wide document callback has rebuilt our derived cache.
    refresh();
    select(std::move(object), std::move(point_id), true);
}

QString Canvas::breadcrumb() const {
    const auto& document = session_.preview_document();
    QStringList names;
    if (!document.compositions.empty())
        names.push_back(QString::fromStdString(document.compositions.front().name));
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
    if (enabled) clear_gradient_edit();
    drawing_object_.clear();
    drawing_contour_.clear();
    if (draw_mode_ == enabled) return;
    draw_mode_ = enabled;
    update_cursor();
    update();
    if (draw_mode_changed) draw_mode_changed(enabled);
}

void Canvas::clear_gradient_edit() {
    const bool active = !gradient_operation_.empty();
    gradient_object_.clear(); gradient_operation_.clear(); gradient_control_.reset();
    if (active && gradient_edit_changed) gradient_edit_changed();
    update();
}

void Canvas::set_gradient_edit(Id object, Id operation) {
    cancel_interaction();
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

const Canvas::Geometry* Canvas::hit_path(QPointF screen) const {
    for (auto i = geometry_.rbegin(); i != geometry_.rend(); ++i) {
        if (selection_target(*i).empty()) continue;
        const auto transform = i->world * view();
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
    if (gradient_control_) {
        const auto transform = gradient_control_->world * view();
        if (distance(transform.map(gradient_control_->start), screen) <= hit_radius)
            return {Drag::gradient_start, gradient_object_, {}};
        if (distance(transform.map(gradient_control_->end), screen) <= hit_radius)
            return {Drag::gradient_end, gradient_object_, {}};
        return {};
    }
    const auto* item = geometry(selected_object);
    if (!item) return {};
    const auto transform = item->world * view();
    if (const auto* selected = point(*item, selected_point)) {
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
    return {};
}

void Canvas::paintEvent(QPaintEvent*) {
    const auto start = clock_.nsecsElapsed();
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor(39, 42, 47));
        const auto& document = session_.preview_document();
        if (!document.compositions.empty()) {
            painter.setWorldTransform(view());
            for (const auto& artboard : document.compositions.front().artboards) {
                const QRectF area(artboard.x, artboard.y, artboard.width, artboard.height);
                painter.fillRect(area.translated(3 / zoom_, 3 / zoom_), QColor(20, 22, 26));
                painter.fillRect(area, QColor(250, 250, 250));
            }
        }
        for (const auto& item : geometry_) {
            for (const auto& paint : item.paints) {
                if (paint.color.alphaF() <= 0 || (!paint.fill && paint.width <= 0)) continue;
                painter.setWorldTransform(paint.transform * item.world * view());
                if (paint.fill) {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(paint.brush);
                } else {
                    painter.setBrush(Qt::NoBrush);
                    QPen pen(paint.brush, paint.width, Qt::SolidLine, Qt::FlatCap, Qt::SvgMiterJoin);
                    pen.setMiterLimit(4);painter.setPen(pen);
                }
                painter.drawPath(paint.path);
            }
        }
        painter.resetTransform();
        QRectF selected_bounds;
        for (const auto& item : geometry_) {
            const bool selected = item.id == selected_object ||
                std::find(item.ancestors.begin(), item.ancestors.end(), selected_object) != item.ancestors.end();
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
            if (item.id != selected_object) continue;
            if (gradient_control_ && item.id == gradient_object_) continue;
            if (const auto* p = point(item, selected_point)) {
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
                const auto size = p.id == selected_point ? 8.0 : 6.0;
                painter.setPen(QPen(p.driven ? linked : accent, 1.3));
                painter.setBrush(p.id == selected_point ? (p.driven ? linked : accent) : QColor(250, 250, 250));
                painter.drawRect(QRectF(screen.x() - size / 2, screen.y() - size / 2, size, size));
            }
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
            document.objects.at(selected_object).kind == Kind::group && !selected_bounds.isNull()) {
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
            : gradient_control_ ? tr("Gradient · Drag its handles · Esc cancels a drag / exits handles · Space-drag to pan")
            : tr("Drag to move · Alt-drag point for handles · Space-drag to pan · Wheel to zoom · F to fit");
        painter.setPen(QColor(166, 174, 186));
        painter.drawText(QRect(14, height() - 30, width() - 100, 22), Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(hint, Qt::ElideRight, width() - 110));
        painter.drawText(QRect(width() - 85, height() - 30, 70, 22), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(zoom_ * 100, 'f', 0) + QStringLiteral("%"));
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
        if (kind == Drag::gradient_start || kind == Drag::gradient_end) {
            if (!gradient_control_) return;
            drag_inverse_ = gradient_control_->world.inverted(&invertible);
            start_anchor_ = kind == Drag::gradient_start ? gradient_control_->start : gradient_control_->end;
            for (const auto* field : {"start_x", "start_y", "end_x", "end_y"})
                start_values_.emplace(field, values_.at(gradient_ref(gradient_object_, gradient_operation_, gradient_control_->id, field)));
        } else if (kind == Drag::object) {
            const auto parent = parents_.at(selected_object);
            drag_inverse_ = (parent.empty() ? QTransform{} : world_.at(parent)).inverted(&invertible);
            start_translation_ = {values_.at({selected_object, {}, "transform.tx"}),
                                  values_.at({selected_object, {}, "transform.ty"})};
            start_values_.emplace("transform.tx", start_translation_.x());
            start_values_.emplace("transform.ty", start_translation_.y());
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
        } else if (drag_ == Drag::anchor) {
            const auto target = start_anchor_ + local - press_local;
            set(selected_point, "x", target.x());
            set(selected_point, "y", target.y());
        } else if (drag_ == Drag::object) {
            const auto target = start_translation_ + local - press_local;
            set({}, "transform.tx", target.x());
            set({}, "transform.ty", target.y());
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
        refresh();
        if (session_.revision() != revision && document_changed) document_changed();
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
        const auto& document = session_.document();
        if (document.compositions.empty()) throw Error("MISSING_COMPOSITION", "Create a composition before drawing");
        const auto parent_world = scope_.empty() ? QTransform{} : world_.at(scope_);
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
            session_.apply({CreatePath{document.compositions.front().id, scope_, object_id,
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
    if (hit.kind != Drag::none) {
        select(hit.object, hit.point);
        begin_drag(hit.kind == Drag::anchor && event->modifiers().testFlag(Qt::AltModifier)
                       ? Drag::symmetric : hit.kind, event->position());
    } else if (const auto* item = hit_path(event->position())) {
        const auto target = selection_target(*item);
        select(target);
        begin_drag(Drag::object, event->position());
    } else {
        select({});
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
    else if (draw_mode_ || drag_ == Drag::anchor || drag_ == Drag::incoming ||
             drag_ == Drag::outgoing || drag_ == Drag::symmetric || drag_ == Drag::gradient_start ||
             drag_ == Drag::gradient_end) setCursor(Qt::CrossCursor);
    else if (drag_ == Drag::object) setCursor(Qt::SizeAllCursor);
    else setCursor(Qt::ArrowCursor);
}

} // namespace nect::desktop
