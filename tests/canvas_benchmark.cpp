#include "window.hpp"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QSaveFile>
#include <QScreen>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <numbers>
#include <stdexcept>

using namespace nect;
using nect::desktop::Canvas;
using nect::desktop::Window;

namespace {
constexpr int warmup_frames = 12;
constexpr int measured_frames = 90;
constexpr int target_interval_ms = 16;
constexpr double floor_budget_ms = 1000.0 / 30;
constexpr double target_budget_ms = 1000.0 / 60;
QElapsedTimer run_clock;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void near(double actual, double expected, const char* message) {
    if (std::abs(actual - expected) > 1e-6) throw std::runtime_error(message);
}

void check_errors(const QStringList& errors) {
    if (!errors.empty()) throw std::runtime_error(errors.join(QStringLiteral("; ")).toStdString());
}

struct InteractionCleanup {
    Canvas& canvas;
    ~InteractionCleanup() {
        canvas.cancel_interaction();
        canvas.error = {};
        canvas.document_changed = {};
    }
};

// A local event loop gives normal queued update/paint, timers and recovery work
// their usual execution. No repaint(), grab(), render() or offscreen surface.
void wait_events(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(std::max(0, milliseconds), &loop, &QEventLoop::quit);
    loop.exec();
    require(run_clock.elapsed() < 55000, "Benchmark exceeded its 55-second execution budget");
}

void mouse(Canvas& canvas, QEvent::Type type, QPointF position, Qt::MouseButton button,
           Qt::MouseButtons buttons) {
    const auto global = QPointF(canvas.mapToGlobal(position.toPoint())) + position - position.toPoint();
    QMouseEvent event(type, position, global, button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &event);
}

void wheel(Canvas& canvas, QPointF position, int delta) {
    const auto global = QPointF(canvas.mapToGlobal(position.toPoint())) + position - position.toPoint();
    QWheelEvent event(position, global, {}, QPoint(0, delta), Qt::NoButton,
                      Qt::NoModifier, Qt::ScrollUpdate, false);
    QCoreApplication::sendEvent(&canvas, &event);
}

QPointF screen_point(const Window& window, QPointF world) {
    const auto& artboard = window.host.session.document().compositions.front().artboards.front();
    return QPointF(window.canvas->width() / 2.0, window.canvas->height() / 2.0) +
        (world - QPointF(artboard.x + artboard.width / 2, artboard.y + artboard.height / 2)) *
        window.canvas->zoom();
}

void fit(Window& window) {
    window.canvas->fit_artboard();
    wait_events(40);
}

double value(const Window& window, const char* point, const char* field) {
    return evaluate(window.host.session.document()).at({"bench-path-0", point, field});
}

void seed(Window& window, int paths) {
    auto& session = window.host.session;
    require(session.document().objects.empty(), "Benchmark requires its own empty document");
    const auto composition = session.document().compositions.front().id;
    std::vector<Command> commands;
    for (int i = 0; i < paths; ++i) {
        const auto suffix = std::to_string(i);
        const auto x = 70.0 + (i % 10) * 84;
        const auto y = 70.0 + (i / 10) * 66;
        std::vector<Point> points;
        for (int j = 0; j < 4; ++j) {
            Point point;
            point.id = "bench-point-" + suffix + "-" + std::to_string(j);
            point.x.literal = x + j * 25;
            point.y.literal = y + (j == 1 ? -18 : j == 2 ? 18 : 0);
            point.in_angle.literal = 180;
            point.in_length.literal = 18;
            point.out_angle.literal = 0;
            point.out_length.literal = 18;
            points.push_back(std::move(point));
        }
        commands.push_back(CreatePath{composition, {}, "bench-path-" + suffix,
            "Benchmark path " + suffix, {{"bench-contour-" + suffix, false, std::move(points)}}});
    }
    session.apply(commands, session.revision());
    window.host.edited();
    require(session.document().objects.size() == static_cast<std::size_t>(paths),
            "Semantic fixture creation produced an unexpected object count");
    fit(window);
    wait_events(100);
}

void seed_compositing(Window& window,int paths) {
    auto& session=window.host.session;
    const auto composition=session.document().compositions.front().id;
    std::vector<Command> commands;
    const auto rectangle=[&](const Id& id,const Id& parent,double x,double y,double width,double height,
                             const std::array<double,3>& color,bool painted) {
        Contour contour;contour.id=id+"-contour";contour.closed=true;
        for(const auto& xy:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}) {
            Point point;point.id=id+"-point-"+std::to_string(contour.points.size());
            point.x.literal=xy.x;point.y.literal=xy.y;contour.points.push_back(point);
        }
        commands.push_back(CreatePath{composition,parent,id,id,{contour}});
        if(painted) {
            auto fill=default_operation(id+"-fill","nect.paint.fill");
            fill.parameters.at("r").literal=color[0];fill.parameters.at("g").literal=color[1];fill.parameters.at("b").literal=color[2];
            commands.push_back(AddOperation{id,fill,0});commands.push_back(Set{{id,"","stroke.width"},0});
        }
    };
    // The edited curve stays inside a modest first-row scope. Its original
    // coordinates and handles remain unchanged, including all drag endpoints.
    std::vector<Id> members;
    for(int i=0;i<std::min(paths,8);++i)members.push_back("bench-path-"+std::to_string(i));
    commands.push_back(GroupContiguous{composition,"",members,"bench-composite-main","Masked curve study"});
    rectangle("bench-composite-paper","bench-composite-main",35,30,770,135,{.82,.9,.96},true);
    rectangle("bench-composite-mask-main","bench-composite-main",40,35,760,125,{0,0,0},false);
    std::vector<Id> order{"bench-composite-paper"};order.insert(order.end(),members.begin(),members.end());order.push_back("bench-composite-mask-main");
    commands.push_back(ReorderObjects{composition,"bench-composite-main",order});
    commands.push_back(SetVisibility{"bench-composite-mask-main",false});
    commands.push_back(SetMask{"bench-composite-main",GeometryMask{"bench-composite-main-mask","bench-composite-mask-main"}});
    commands.push_back(Set{{"bench-composite-main","","composite.opacity"},.82});

    // One additional independent mask scope and a single nested leaf blend.
    // These shapes do not cover the first curve's point/handle/body hit targets.
    rectangle("bench-composite-coral","",450,300,180,110,{.86,.32,.25},true);
    rectangle("bench-composite-teal","",500,330,170,110,{.12,.58,.65},true);
    rectangle("bench-composite-mask-study","",440,290,240,165,{0,0,0},false);
    commands.push_back(MaskObjects{composition,"",{"bench-composite-coral","bench-composite-teal","bench-composite-mask-study"},
        "bench-composite-study","bench-composite-study-mask","Masked blend study",true});
    commands.push_back(SetCompositing{"bench-composite-study","multiply",false});
    commands.push_back(Set{{"bench-composite-study","","composite.opacity"},.78});
    commands.push_back(SetCompositing{"bench-composite-teal","screen",false});
    session.apply(commands,session.revision());window.host.edited();wait_events(80);
    require(session.document().objects.size()==static_cast<std::size_t>(paths+7),"Compositing fixture object count changed");
    require(session.document().objects.at("bench-path-0").visible,"Compositing interaction target must remain visible");
}

enum class Operation { pan, zoom, point, handle, transform };

QString operation_name(Operation operation) {
    switch (operation) {
    case Operation::pan: return QStringLiteral("pan");
    case Operation::zoom: return QStringLiteral("zoom");
    case Operation::point: return QStringLiteral("point-drag");
    case Operation::handle: return QStringLiteral("outgoing-handle-drag");
    case Operation::transform: return QStringLiteral("object-translation");
    }
    return {};
}

QString timing_operation(Operation operation) {
    if (operation == Operation::point || operation == Operation::handle) return QStringLiteral("point-handle");
    if (operation == Operation::transform) return QStringLiteral("transform");
    return operation_name(operation);
}

bool edits_document(Operation operation) {
    return operation == Operation::point || operation == Operation::handle || operation == Operation::transform;
}

struct SequenceResult {
    double elapsed_ms = 0;
    double release_and_ui_commit_ms = 0;
};

SequenceResult sequence(Window& window, Operation operation, int frames, bool commit,
                        const QStringList& errors,bool text_transform=false,int selection_count=1) {
    auto& canvas = *window.canvas;
    require(window.isVisible() && window.windowHandle() && window.windowHandle()->isExposed(),
            "The benchmark window must remain visible and exposed");
    canvas.set_selection(text_transform?"bench-text-0":"bench-path-0", operation == Operation::transform ? Id{} : "bench-point-0-0");
    if(selection_count>1&&operation!=Operation::handle) {
        std::vector<Canvas::Selection> selections;
        for(int i=selection_count-1;i>=0;--i)selections.push_back({"bench-path-"+std::to_string(i),operation==Operation::transform?Id{}:"bench-point-"+std::to_string(i)+"-0"});
        canvas.set_selections(std::move(selections));
    }
    canvas.setFocus();
    fit(window);
    canvas.reset_timing();
    const auto zoom = canvas.zoom();
    const QPointF world_start = text_transform?QPointF(65,360):operation == Operation::handle ? QPointF(88, 70)
        : operation == Operation::transform ? QPointF(107.5, 70) : QPointF(70, 70);
    const auto start = operation == Operation::pan || operation == Operation::zoom
        ? QPointF(canvas.width() / 2.0, canvas.height() / 2.0) : screen_point(window, world_start);
    const auto button = operation == Operation::pan ? Qt::MiddleButton : Qt::LeftButton;
    if (operation != Operation::zoom) mouse(canvas, QEvent::MouseButtonPress, start, button, button);
    if (edits_document(operation)) require(window.host.session.gesture_active(), "Viewport edit failed to start a gesture");
    const auto revision = window.host.session.revision();
    const auto initial_values=evaluate(window.host.session.document());
    QElapsedTimer elapsed;
    elapsed.start();
    QPointF end = start;
    for (int i = 0; i < frames; ++i) {
        const auto t = static_cast<double>(i + 1) / frames;
        if (operation == Operation::zoom) {
            wheel(canvas, start, i < frames / 2 ? 4 : -4);
        } else {
            QPointF delta;
            if (operation == Operation::pan)
                delta = {50 * std::sin(t * 2 * std::numbers::pi), 35 * std::sin(t * std::numbers::pi)};
            else if (operation == Operation::handle)
                delta = {12 + 16 * t, 18 + 8 * std::sin(t * 2 * std::numbers::pi)};
            else
                delta = {14 + 40 * t, 12 + 18 * std::sin(t * 2 * std::numbers::pi)};
            end = start + delta;
            mouse(canvas, QEvent::MouseMove, end, Qt::NoButton, button);
        }
        check_errors(errors);
        wait_events(static_cast<int>(std::max<qint64>(0,
            static_cast<qint64>(i + 1) * target_interval_ms - elapsed.elapsed())));
        check_errors(errors);
        if (edits_document(operation)&&!window.host.session.gesture_active())
            throw std::runtime_error("The active gesture was interrupted during measurement: "+operation_name(operation).toStdString()+
                "; window active="+(window.isActiveWindow()?"true":"false")+"; Canvas focus="+(canvas.hasFocus()?"true":"false")+
                "; focused widget="+(QApplication::focusWidget()?std::string(QApplication::focusWidget()->metaObject()->className()):"none"));
    }
    SequenceResult result;
    result.elapsed_ms = elapsed.nsecsElapsed() / 1e6;
    QElapsedTimer release;
    release.start();
    if (!commit) canvas.cancel_interaction();
    if (operation != Operation::zoom) mouse(canvas, QEvent::MouseButtonRelease, end, button, Qt::NoButton);
    result.release_and_ui_commit_ms = release.nsecsElapsed() / 1e6;
    wait_events(40);
    check_errors(errors);
    if (commit && edits_document(operation)) {
        require(window.host.session.revision() == revision + 1,
                "Viewport edit did not commit exactly one Session revision");
        if (operation == Operation::point) {
            near(value(window, "bench-point-0-0", "x"), 70 + 54 / zoom, "Point drag produced incorrect X");
            near(value(window, "bench-point-0-0", "y"), 70 + 12 / zoom, "Point drag produced incorrect Y");
        } else if (operation == Operation::handle) {
            near(value(window, "bench-point-0-0", "out.length"), std::hypot(18 + 28 / zoom, 18 / zoom),
                 "Handle drag produced incorrect length");
        } else {
            const auto values=evaluate(window.host.session.document());const auto object=text_transform?"bench-text-0":"bench-path-0";
            near(values.at({object,"","transform.tx"}), 54 / zoom, "Object drag produced incorrect translation X");
            near(values.at({object,"","transform.ty"}), 12 / zoom, "Object drag produced incorrect translation Y");
        }
        if(selection_count>1&&(operation==Operation::point||operation==Operation::transform)) {
            const auto values=evaluate(window.host.session.document());
            for(int i=0;i<selection_count;++i) {
                const auto object="bench-path-"+std::to_string(i),point=operation==Operation::point?"bench-point-"+std::to_string(i)+"-0":Id{};
                const Ref x{object,point,operation==Operation::point?"x":"transform.tx"},y{object,point,operation==Operation::point?"y":"transform.ty"};
                near(values.at(x),initial_values.at(x)+54/zoom,"Every selected target translates once X");
                near(values.at(y),initial_values.at(y)+12/zoom,"Every selected target translates once Y");
            }
        }
    } else {
        require(window.host.session.revision() == revision, "View movement or warm-up changed committed state");
    }
    return result;
}

QJsonObject distribution(std::vector<double> values) {
    if (values.empty()) return {{"count", 0}, {"p50_ms", QJsonValue::Null}, {"p95_ms", QJsonValue::Null}};
    std::sort(values.begin(), values.end());
    const auto quantile = [&](double fraction) {
        return values[static_cast<std::size_t>(std::ceil(fraction * values.size())) - 1];
    };
    int over30 = 0;
    int over60 = 0;
    for (const auto value : values) {
        if (value > floor_budget_ms) ++over30;
        if (value > target_budget_ms) ++over60;
    }
    return {{"count", static_cast<int>(values.size())}, {"p50_ms", quantile(0.5)},
        {"p95_ms", quantile(0.95)}, {"max_ms", values.back()},
        {"over_33_333_ms", over30}, {"over_16_667_ms", over60}};
}

QJsonObject summarize(const Canvas& canvas, Operation operation, SequenceResult sequence,
                      std::uint64_t revision_before, std::uint64_t revision_after, int commits) {
    std::vector<double> intervals, inputs, paints;
    QJsonArray raw;
    for (const auto& frame : canvas.frame_timings()) {
        if (frame.operation != timing_operation(operation)) continue;
        paints.push_back(frame.paint_ms);
        inputs.push_back(frame.input_to_paint_ms);
        if (frame.interval_ms >= 0) intervals.push_back(frame.interval_ms);
        raw.push_back(QJsonObject{{"paint_ms", frame.paint_ms}, {"input_to_paint_ms", frame.input_to_paint_ms},
            {"interval_ms", frame.interval_ms < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(frame.interval_ms)}});
    }
    const auto interval_stats = distribution(intervals);
    const bool enough = intervals.size() >= 60;
    const bool floor = enough && interval_stats["p95_ms"].toDouble() <= floor_budget_ms;
    const bool target = enough && interval_stats["p95_ms"].toDouble() <= target_budget_ms;
    return {{"operation", operation_name(operation)}, {"requested_inputs", measured_frames},
        {"observed_paints", static_cast<int>(paints.size())}, {"warmup_inputs", warmup_frames},
        {"requested_interval_ms", target_interval_ms}, {"elapsed_ms", sequence.elapsed_ms},
        {"release_and_ui_commit_ms", sequence.release_and_ui_commit_ms},
        {"revision_before", static_cast<qint64>(revision_before)}, {"revision_after", static_cast<qint64>(revision_after)},
        {"committed_notifications", commits}, {"sufficient_interval_samples", enough},
        {"meets_30fps_p95_interval_budget", floor}, {"meets_60fps_p95_interval_budget", target},
        {"viewport_width", canvas.width()}, {"viewport_height", canvas.height()},
        {"device_pixel_ratio", canvas.devicePixelRatioF()}, {"interval", interval_stats},
        {"input_to_paint", distribution(inputs)}, {"paint", distribution(paints)}, {"raw_frames", raw}};
}

void write_result(const QString& path, const QJsonObject& result) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "Cannot create benchmark artifact directory");
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Indented);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(),
            "Cannot write benchmark JSON artifact");
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Nect viewport benchmark");
    app.setOrganizationName("Nect");
    app.setStyle("Fusion");
    // Match the current desktop launcher; fixture metrics record this choice.
    app.setStyleSheet(
        "QMainWindow,QDialog,QWidget{background:#25292f;color:#e1e5eb;}"
        "QLineEdit,QTreeWidget,QListWidget{background:#1c2026;border:1px solid #393f49;border-radius:3px;padding:4px;}"
        "QTreeWidget::item,QListWidget::item{padding:5px;}"
        "QTreeWidget::item:selected,QListWidget::item:selected{background:#354d5b;}"
        "QPushButton{background:#333943;border:1px solid #454d58;border-radius:3px;padding:4px;}"
        "QPushButton:hover{background:#414a56;}"
        "QGroupBox{border:1px solid #3a414b;border-radius:4px;margin-top:12px;padding-top:10px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;}"
        "QToolBar{spacing:8px;padding:4px;border-bottom:1px solid #3b424a;}"
        "QMenu{border:1px solid #49515c;}QMenu::item:selected{background:#43505f;}");
    app.setQuitOnLastWindowClosed(false);
    if (app.arguments().size() < 2 || app.arguments().size()>3 ||
        (app.arguments().size()==3&&app.arguments().at(2)!="--repeat"&&app.arguments().at(2)!="--text"&&app.arguments().at(2)!="--polystar"&&app.arguments().at(2)!="--multi"&&app.arguments().at(2)!="--expressions"&&app.arguments().at(2)!="--compositing")) {
        std::cerr << "Usage: canvas_benchmark <result.json> [--repeat|--text|--polystar|--multi|--expressions|--compositing]\n";
        return 2;
    }
    const auto output = app.arguments().at(1);
    const bool repeated=app.arguments().contains("--repeat");
    const bool text_scene=app.arguments().contains("--text");
    const bool polystar_scene=app.arguments().contains("--polystar");
    const bool expression_scene=app.arguments().contains("--expressions");
    const bool compositing_scene=app.arguments().contains("--compositing");
    const bool multi_scene=app.arguments().contains("--multi")||expression_scene;
    run_clock.start();
    QJsonObject result{{"schema", "nect-visible-viewport-benchmark-1"},
        {"started_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"measurement_boundary", "Visible production Window/Canvas QWidget paint completion; input to end of paint includes Session preview/evaluation. Native compositor/GPU presentation is not measured."},
        {"input_method", "QMouseEvent/QWheelEvent delivered to the visible production Canvas; normal queued updates and event loop; no forced repaint."},
        {"frame_interval_boundary", "Consecutive input-triggered QWidget paint completions in each sequence; input cadence and coalescing included. First interval omitted."},
        {"quantile_method", "Nearest rank"}, {"platform", QGuiApplication::platformName()},
        {"qt_version", qVersion()}, {"os", QSysInfo::prettyProductName()},
        {"cpu_architecture", QSysInfo::currentCpuArchitecture()},
        {"build_cpu_architecture", QSysInfo::buildCpuArchitecture()},
        {"style", "Fusion with the current desktop launcher stylesheet; production Window widgets and Canvas drawing"},
        {"floor_interval_budget_ms", floor_budget_ms}, {"target_interval_budget_ms", target_budget_ms}};
    QJsonArray scenes;
    try {
        require(QGuiApplication::platformName() == "windows", "Run this visible benchmark with the Windows Qt platform, not offscreen/minimal");
        QTemporaryDir scratch(QDir::tempPath() + "/nect-viewport-benchmark-XXXXXX");
        require(scratch.isValid(), "Cannot allocate the benchmark-owned recovery directory");
        bool all_floor = true;
        bool all_target = true;
        bool all_release_budget = true;
        for (const auto paths : ((repeated||text_scene||polystar_scene)?std::vector<int>{2}:std::vector<int>{2,80})) {
            Window window(scratch.path() + "/scene-" + QString::number(paths));
            window.resize(1440, 900);
            window.show();
            window.raise();
            window.activateWindow();
            int exposed_wait = 0;
            while ((!window.windowHandle() || !window.windowHandle()->isExposed()) && exposed_wait < 5000) {
                wait_events(20);
                exposed_wait += 20;
            }
            require(window.windowHandle() && window.windowHandle()->isExposed(), "Benchmark window did not become exposed");
            seed(window, paths);
            if(compositing_scene)seed_compositing(window,paths);
            if(expression_scene) {
                std::vector<Ref> targets;for(int i=0;i<paths;++i)targets.push_back({"bench-path-"+std::to_string(i),"","stroke.width"});
                window.host.session.apply({SetExpression{targets,{"ref(\"bench-path-0\",\"bench-point-0-0\",\"x\") / 100 + 1",1},false}},window.host.session.revision());
                window.host.edited();wait_events(40);
            }
            if(polystar_scene) {
                std::vector<Command> commands;const auto comp=window.host.session.document().compositions.front().id;
                for(int i=0;i<24;++i) {
                    const auto id="bench-primitive-"+std::to_string(i);
                    auto source=default_primitive(id+"-source",i%2?"nect.shape.star":"nect.shape.polygon");
                    source.parameters.at("center_x").literal=105+(i%6)*148;
                    source.parameters.at("center_y").literal=190+(i/6)*115;
                    source.parameters.at(i%2?"outer_radius":"radius").literal=42;
                    if(i%2)source.parameters.at("inner_radius").literal=19;
                    source.parameters.at("points").literal=6;
                    if(i)source.parameters.at("points").binding=Binding{{"bench-primitive-0","","generator.points"},1,0,"copy_local_value"};
                    commands.push_back(CreatePrimitive{comp,"",id,"Linked polystar "+std::to_string(i),source});
                }
                window.host.session.apply(commands,window.host.session.revision());window.host.edited();wait_events(40);
            }
            if(text_scene) {
                std::vector<Command> commands;const auto comp=window.host.session.document().compositions.front().id;
                for(int i=0;i<8;++i) {
                    auto text=default_text("bench-text-source-"+std::to_string(i),"Nect typography 2026 / \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
                    text.parameters.at("origin_x").literal=60+(i%2)*450;text.parameters.at("origin_y").literal=350+(i/2)*58;
                    text.parameters.at("font_size").literal=21;
                    commands.push_back(CreateText{comp,"","bench-text-"+std::to_string(i),"Typography",text});
                }
                window.host.session.apply(commands,window.host.session.revision());window.host.edited();wait_events(40);
            }
            if(repeated) {
                std::vector<Command> commands;
                for(int i=0;i<paths;++i) {
                    const auto object="bench-path-"+std::to_string(i);
                    auto fill=default_operation("bench-fill-"+std::to_string(i),"nect.paint.fill");
                    fill.parameters["r"].literal=.15;fill.parameters["g"].literal=.55;fill.parameters["b"].literal=.7;
                    auto repeat=default_operation("bench-repeat-"+std::to_string(i),"nect.shape.repeater");
                    repeat.parameters["copies"].literal=12;repeat.parameters["position_x"].literal=0;repeat.parameters["position_y"].literal=38;
                    commands.push_back(AddOperation{object,fill,1});commands.push_back(AddOperation{object,repeat,2});
                }
                window.host.session.apply(commands,window.host.session.revision());window.host.edited();wait_events(40);
            }
            QStringList errors;
            int commits = 0;
            const auto original_error = window.canvas->error;
            const auto original_changed = window.canvas->document_changed;
            window.canvas->error = [&](QString message) {
                errors.push_back(message);
                if (original_error) original_error(std::move(message));
            };
            window.canvas->document_changed = [&] {
                ++commits;
                if (original_changed) original_changed();
            };
            InteractionCleanup cleanup{*window.canvas};
            const auto* screen = window.screen();
            QJsonObject scene{{"name", repeated?"repeated-paint":paths == 2 ? "lightweight" : "representative"},
                {"path_count", paths}, {"point_count", paths * 4}, {"points_per_path", 4},
                {"binding_count", 0}, {"group_count", 0}, {"artboard_count", 1},
                {"artboard_width", 960}, {"artboard_height", 640},
                {"fixture", "Semantic CreatePath commands; deterministic 10-column grid; four cubic anchors per open path; 18 du polar handles; native default stroke."},
                {"fixture_revision", static_cast<qint64>(window.host.session.revision())},
                {"window_width", window.width()}, {"window_height", window.height()},
                {"active_window_at_start", window.isActiveWindow()},
                {"screen_name", screen ? screen->name() : QString{}},
                {"screen_width", screen ? screen->size().width() : 0},
                {"screen_height", screen ? screen->size().height() : 0},
                {"logical_dpi", screen ? screen->logicalDotsPerInch() : 0},
                {"reported_refresh_hz", screen ? screen->refreshRate() : 0}};
            if(repeated) {
                scene["fixture"]="Two authored four-anchor curves; each Stroke + Fill + 12-copy Repeater; 24 virtual path instances and 48 paint layers; fixed 38 du Y step.";
                scene["virtual_path_instances"]=24;scene["paint_layers"]=48;
            }
            if(text_scene) {
                scene["name"]="mixed-text";scene["text_count"]=8;
                scene["fixture"]="Two authored four-anchor curves plus eight editable mixed Japanese/Latin Text objects, Yu Gothic 21 du. Pan/zoom, curve point/handle and Text object translation in the full Window with all text visible.";
            }
            if(polystar_scene) {
                scene["name"]="linked-polystar";scene["primitive_count"]=24;scene["generated_point_count"]=216;scene["binding_count"]=23;
                scene["fixture"]="Two authored four-anchor curves plus twelve six-point Polygons and twelve six-point Stars; 23 live point-count links to the first Polygon. Pan/zoom and curve point/handle/translation exercise complete mixed-scene evaluation in the full Window. Count-changing gesture timing is not measured.";
            }
            QJsonArray operations;
            const int selection_count=multi_scene?std::min(paths,12):1;
            if(multi_scene){scene["name"]=paths==2?"multi-lightweight":"multi-representative";scene["selected_targets"]=selection_count;
                scene["fixture"]="Authored four-anchor curves on the standard grid; first 2/12 object or point targets selected together for pan/zoom/point/translation. Handle operation remains a single selected point.";}
            if(expression_scene){scene["expression_count"]=paths;scene["name"]=paths==2?"expression-lightweight":"expression-representative";
                scene["fixture"]=scene["fixture"].toString()+" Each stroke width is driven by the first point X / 100 + 1, so point dragging reevaluates all paints.";}
            if(compositing_scene) {
                scene["name"]=paths==2?"compositing-lightweight":"compositing-representative";
                scene["base_curve_count"]=paths;scene["path_count"]=paths+5;scene["leaf_count"]=paths+5;
                scene["point_count"]=(paths+5)*4;scene["visible_leaf_count"]=paths+3;
                scene["group_count"]=2;scene["mask_count"]=2;scene["hidden_mask_source_count"]=2;
                scene["isolated_group_count"]=2;scene["isolated_leaf_count"]=1;scene["maximum_isolation_depth"]=2;
                scene["main_scope_curve_count"]=std::min(paths,8);scene["blends"]=QJsonArray{"normal","multiply","screen"};
                scene["interaction_target"]="bench-path-0";
                scene["fixture"]="Standard 2/80 open-curve fixture plus five four-anchor rectangles and two Groups. First 2/8 curves and a pale plate share a rectangular geometry mask at Group opacity .82. A separate two-color masked Group uses Multiply at .78 and one Screen leaf. Both source masks are hidden. Point/handle/translation edit the original first curve inside its mask scope; other curves remain ungrouped. No full-document or deeply nested alpha Group.";
            }
            for (const auto operation : {Operation::pan, Operation::zoom, Operation::point, Operation::handle, Operation::transform}) {
                sequence(window, operation, warmup_frames, false, errors,text_scene&&operation==Operation::transform,selection_count);
                const auto before = window.host.session.revision();
                const auto commits_before = commits;
                const auto measurement = sequence(window, operation, measured_frames, true, errors,text_scene&&operation==Operation::transform,selection_count);
                const auto after = window.host.session.revision();
                require(commits - commits_before == (edits_document(operation) ? 1 : 0),
                        "Unexpected committed notification count in full application");
                const auto summary = summarize(*window.canvas, operation, measurement, before, after, commits - commits_before);
                all_floor = all_floor && summary["meets_30fps_p95_interval_budget"].toBool();
                all_target = all_target && summary["meets_60fps_p95_interval_budget"].toBool();
                all_release_budget = all_release_budget && measurement.release_and_ui_commit_ms <= 33.333;
                operations.push_back(summary);
                scene["operations"] = operations;
                result["in_progress_scene"] = scene;
                if (edits_document(operation)) {
                    window.host.session.undo(window.host.session.revision());
                    window.host.edited();
                    wait_events(40);
                }
            }
            scene["final_revision"] = static_cast<qint64>(window.host.session.revision());
            scene["active_window_at_end"] = window.isActiveWindow();
            scenes.push_back(scene);
            result["scenes"] = scenes;
            result.remove("in_progress_scene");
            window.canvas->error = original_error;
            window.canvas->document_changed = original_changed;
            require(window.close(), "Benchmark-owned window did not close cleanly");
            wait_events(20);
        }
        result["completed"] = true;
        result["all_operations_meet_30fps_p95_interval_budget"] = all_floor;
        result["all_operations_meet_60fps_p95_interval_budget"] = all_target;
        result["all_operations_meet_33ms_release_budget"] = all_release_budget;
        result["elapsed_ms"] = run_clock.elapsed();
        write_result(output, result);
        std::cout << "Visible Canvas benchmark complete: " << output.toStdString()
                  << "; 30 fps interval floor " << (all_floor ? "met" : "NOT MET") << '\n';
        return 0; // Timing-budget misses are reported in the artifact, not hidden as execution errors.
    } catch (const std::exception& error) {
        result["completed"] = false;
        result["execution_error"] = QString::fromUtf8(error.what());
        result["scenes"] = scenes;
        result["elapsed_ms"] = run_clock.elapsed();
        try { write_result(output, result); } catch (const std::exception& write_error) { std::cerr << write_error.what() << '\n'; }
        std::cerr << "Visible Canvas benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
