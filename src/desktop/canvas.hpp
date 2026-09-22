#pragma once

#include "nect/core.hpp"

#include <QColor>
#include <QBrush>
#include <QElapsedTimer>
#include <QPainterPath>
#include <QImage>
#include <QPointF>
#include <QTransform>
#include <QWidget>
#include <functional>
#include <exception>
#include <map>
#include <vector>

namespace nect::desktop {

// This widget keeps derived geometry and view state only. Every authored edit,
// including an in-progress drag, is a command on the window's shared Session.
class Canvas final : public QWidget {
public:
    explicit Canvas(Session& session, QWidget* parent = nullptr);
    ~Canvas() override;

    Id selected_object;
    Id selected_point;
    struct Selection {Id object,point;bool operator==(const Selection&)const=default;};
    const std::vector<Selection>& selections()const{return selections_;}
    std::vector<Id> selected_objects()const;
    std::function<void()> selection_changed;
    // Sent after projecting a Canvas edit; failed projection remains explicit.
    std::function<void()> document_changed;
    std::function<void()> scope_changed;
    std::function<void(bool)> draw_mode_changed;
    std::function<void()> gradient_edit_changed;
    std::function<void(bool)> anchor_edit_changed;
    std::function<void()> active_artboard_changed;
    std::function<void(QString)> error;

    void refresh();
    bool projection_succeeded() const {return !projection_error_;}
    static QImage render_artboard(const Document&,const Id& composition,const Id& artboard,double scale,bool white_background);
    const std::map<Ref,double>& evaluated_values() const {return values_;}
    const std::map<Id,EvaluatedTransform>& evaluated_transforms() const {return transforms_;}
    void fit_selection();
    void select_all_in_context();
    void nudge_selection(double dx,double dy);
    void fit_artboard();
    void fit_all_artboards();
    const Id& active_composition() const { return active_composition_; }
    const Id& active_artboard() const { return active_artboard_; }
    void set_active_artboard(Id composition, Id artboard, bool fit = true);
    void set_selection(Id object, Id point = {});
    void set_selections(std::vector<Selection> items);
    void set_draw_mode(bool enabled);
    void set_anchor_edit(bool enabled);
    bool anchor_edit() const {return anchor_edit_;}
    void set_show_mask_outline(bool enabled) {show_mask_outline_=enabled;update();}
    bool show_mask_outline() const {return show_mask_outline_;}
    bool draw_mode() const { return draw_mode_; }
    void set_gradient_edit(Id object, Id operation);
    const Id& gradient_operation() const { return gradient_operation_; }
    void cancel_interaction();
    const Id& drill_scope() const { return scope_; }
    QString breadcrumb() const;
    void leave_group();
    double zoom() const { return zoom_; }
    void set_snap_enabled(bool enabled);
    bool snap_enabled() const { return snap_enabled_; }

    // Raw widget paint observations, not a claim about presentation/GPU latency.
    // An interval of -1 is the first paint in an input sequence. Input cadence is
    // included in intervals; callers must record their fixture and input method.
    struct FrameTiming {
        QString operation;
        double semantic_preview_ms = 0;
        double projection_ms = 0;
        double paint_ms = 0;
        double input_to_paint_ms = 0;
        double interval_ms = -1;
        int viewport_width = 0;
        int viewport_height = 0;
        double device_pixel_ratio = 1;
    };
    const std::vector<FrameTiming>& frame_timings() const { return timings_; }
    void reset_timing();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    struct EvaluatedPoint {
        Id id;
        Id contour;
        QPointF anchor;
        QPointF incoming;
        QPointF outgoing;
        double in_angle = 0;
        double out_angle = 0;
        bool driven = false;
    };
    struct Geometry {
        struct Paint {
            QPainterPath path;
            QTransform transform;
            QColor color;
            QBrush brush;
            bool fill = false;
            double width = 0;
        };
        Id id;
        std::vector<Id> ancestors;
        QPainterPath path;
        QTransform world;
        std::vector<Paint> paints;
        std::vector<EvaluatedPoint> points;
        std::optional<QRectF> image_bounds;
        QImage image;
        std::optional<QRectF> text_bounds;
        bool text_overflow=false;
        bool normal_visible=true;
    };
    enum class Drag { none, marquee, pan, anchor, pivot, incoming, outgoing, symmetric, object, gradient_start, gradient_end };
    struct Hit {
        Drag kind = Drag::none;
        Id object;
        Id point;
    };

    void paint_artwork(QPainter&,const QTransform&,QSizeF,double dpr) const;
    std::exception_ptr projection_error_;
    Session& session_;
    std::map<Ref, double> values_;
    std::map<Id,EvaluatedTransform> transforms_;
    std::vector<Geometry> geometry_;
    std::map<Id,std::size_t> geometry_index_;
    EvaluatedScene scene_;
    struct RasterProjection { Raster payload; QImage image; };
    std::map<Id,RasterProjection> rasters_;
    std::map<Id,QPainterPath> mask_paths_;
    bool show_mask_outline_=true;
    QString render_error_;
    std::map<Id, QTransform> world_;
    std::map<Id, Id> parents_;
    std::vector<Selection> selections_;
    Id scope_;
    Id active_composition_, active_artboard_;
    std::vector<Artboard> artboards_;
    struct GradientControl { Id id; QPointF start, end; QTransform world; bool radial = false; };
    Id gradient_object_, gradient_operation_;
    std::optional<GradientControl> gradient_control_;
    bool draw_mode_ = false;
    bool anchor_edit_ = false;
    Id drawing_object_;
    Id drawing_contour_;
    bool initial_fit_ = true;
    double zoom_ = 1;
    QPointF pan_{40, 40};
    bool space_down_ = false;
    Drag drag_ = Drag::none;
    QPointF press_position_;
    QPointF marquee_position_;
    std::vector<Selection> marquee_start_;
    bool marquee_extend_=false;
    QPointF press_pan_;
    QPointF start_anchor_;
    QPointF start_handle_;
    QPointF start_translation_;
    QTransform drag_inverse_;
    double start_in_angle_ = 0;
    double start_out_angle_ = 0;
    std::map<std::string, double> start_values_;
    struct PointStart {Selection target;QPointF anchor;QTransform inverse;};
    std::vector<PointStart> point_starts_;
    bool gesture_owned_ = false;
    bool drag_moved_ = false;
    bool snap_enabled_ = true;
    std::optional<QRectF> snap_bounds_;
    std::vector<double> snap_x_, snap_y_;
    std::optional<double> snap_guide_x_, snap_guide_y_;
    QRectF breadcrumb_rect_;

    QElapsedTimer clock_;
    qint64 input_started_ns_ = -1;
    double pending_preview_ms_=0,pending_projection_ms_=0;
    qint64 last_paint_ns_ = -1;
    qint64 last_wheel_ns_ = -1;
    std::uint64_t input_sequence_ = 0;
    std::uint64_t painted_sequence_ = 0;
    QString pending_operation_;
    std::vector<FrameTiming> timings_;

    QTransform view() const;
    const Geometry* geometry(const Id&) const;
    const EvaluatedPoint* point(const Geometry&, const Id&) const;
    Id selection_target(const Geometry&) const;
    const Geometry* hit_path(QPointF screen) const;
    Hit hit_control(QPointF screen) const;
    bool visible_hit(const Geometry&,QPointF screen) const;
    void select(Id object, Id point = {}, bool enter_parent = false);
    void select_many(std::vector<Selection> items,bool enter_parent=false);
    void toggle_selection(Selection item);
    void set_scope(Id scope);
    void begin_drag(Drag kind, QPointF screen);
    void prepare_snap();
    QPointF snap_delta(QPointF delta);
    void update_drag(QPointF screen);
    void finish_drag();
    void finish_marquee();
    void append_draw_point(QPointF screen);
    void report_error(const std::exception&);
    void request_frame(const QString& operation, bool new_sequence = false);
    void update_cursor();
    void clear_gradient_edit();
    void fit_bounds(QRectF bounds);
};

} // namespace nect::desktop
