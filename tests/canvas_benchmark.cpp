#include "window.hpp"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
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
#include <optional>
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

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void near(double actual, double expected, const char* message) {
    if (std::abs(actual - expected) > 1e-6)
        throw std::runtime_error(std::string(message)+": actual="+std::to_string(actual)+" expected="+std::to_string(expected));
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
    double press_ms = 0;
    double release_and_ui_commit_ms = 0;
};

SequenceResult sequence(Window& window, Operation operation, int frames, bool commit,
                        const QStringList& errors,bool text_transform=false,int selection_count=1,
                        std::vector<Canvas::FrameTiming>* observed_frames=nullptr) {
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
    if (observed_frames) observed_frames->clear();
    const auto zoom = canvas.zoom();
    const QPointF world_start = text_transform?QPointF(65,360):operation == Operation::handle ? QPointF(88, 70)
        : operation == Operation::transform ? QPointF(107.5, 70) : QPointF(70, 70);
    const auto start = operation == Operation::pan || operation == Operation::zoom
        ? QPointF(canvas.width() / 2.0, canvas.height() / 2.0) : screen_point(window, world_start);
    const auto button = operation == Operation::pan ? Qt::MiddleButton : Qt::LeftButton;
    QElapsedTimer press; press.start();
    if (operation != Operation::zoom) mouse(canvas, QEvent::MouseButtonPress, start, button, button);
    const auto press_ms = press.nsecsElapsed() / 1e6;
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
    result.press_ms = press_ms;
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
            const auto x=value(window,"bench-point-0-0","x"),y=value(window,"bench-point-0-0","y");
            require(std::isfinite(x)&&std::abs(x-(70+54/zoom))<=6.0/zoom+1e-6,
                "Point Snap X correction exceeds its 6-logical-pixel budget");
            require(std::isfinite(y)&&std::abs(y-(70+12/zoom))<=6.0/zoom+1e-6,
                "Point Snap Y correction exceeds its 6-logical-pixel budget");
        } else if (operation == Operation::handle) {
            near(value(window, "bench-point-0-0", "out.length"), std::hypot(18 + 28 / zoom, 18 / zoom),
                 "Handle drag produced incorrect length");
        } else {
            const auto values=evaluate(window.host.session.document());const auto object=text_transform?"bench-text-0":"bench-path-0";
            require(std::abs(values.at({object,"","transform.tx"})-54/zoom)<=6.0/zoom+1e-6,"Snap X stays within screen tolerance");
            require(std::abs(values.at({object,"","transform.ty"})-12/zoom)<=6.0/zoom+1e-6,"Snap Y stays within screen tolerance");
        }
        if(selection_count>1&&(operation==Operation::point||operation==Operation::transform)) {
            const auto values=evaluate(window.host.session.document());
            for(int i=0;i<selection_count;++i) {
                const auto object="bench-path-"+std::to_string(i),point=operation==Operation::point?"bench-point-"+std::to_string(i)+"-0":Id{};
                const Ref x{object,point,operation==Operation::point?"x":"transform.tx"},y{object,point,operation==Operation::point?"y":"transform.ty"};
                const double dx=operation==Operation::point?
                    values.at({"bench-path-0","bench-point-0-0","x"})-initial_values.at({"bench-path-0","bench-point-0-0","x"}):
                    values.at({"bench-path-0","","transform.tx"})-initial_values.at({"bench-path-0","","transform.tx"});
                const double dy=operation==Operation::point?
                    values.at({"bench-path-0","bench-point-0-0","y"})-initial_values.at({"bench-path-0","bench-point-0-0","y"}):
                    values.at({"bench-path-0","","transform.ty"})-initial_values.at({"bench-path-0","","transform.ty"});
                near(values.at(x),initial_values.at(x)+dx,"Every selected target translates once X");
                near(values.at(y),initial_values.at(y)+dy,"Every selected target translates once Y");
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
                      std::uint64_t revision_before, std::uint64_t revision_after, int commits,
                      const std::vector<Canvas::FrameTiming>* observed_frames=nullptr,
                      bool include_detailed_timing=true) {
    std::vector<double> intervals, inputs, paints, previews, projections;
    QJsonArray raw;
    const auto& frames=observed_frames?*observed_frames:canvas.frame_timings();
    for (const auto& frame : frames) {
        if (frame.operation != timing_operation(operation)) continue;
        if (include_detailed_timing) {
            previews.push_back(frame.semantic_preview_ms);
            projections.push_back(frame.projection_ms);
        }
        paints.push_back(frame.paint_ms);
        inputs.push_back(frame.input_to_paint_ms);
        if (frame.interval_ms >= 0) intervals.push_back(frame.interval_ms);
        QJsonObject raw_frame{{"paint_ms", frame.paint_ms}, {"input_to_paint_ms", frame.input_to_paint_ms},
            {"interval_ms", frame.interval_ms < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(frame.interval_ms)}};
        if (include_detailed_timing) {
            raw_frame["semantic_preview_ms"]=frame.semantic_preview_ms;
            raw_frame["projection_ms"]=frame.projection_ms;
        }
        raw.push_back(raw_frame);
    }
    const auto interval_stats = distribution(intervals);
    const bool enough = intervals.size() >= 60;
    const bool floor = enough && interval_stats["p95_ms"].toDouble() <= floor_budget_ms;
    const bool target = enough && interval_stats["p95_ms"].toDouble() <= target_budget_ms;
    return {{"operation", operation_name(operation)}, {"timing_capture_enabled",include_detailed_timing},
        {"requested_inputs", measured_frames},
        {"observed_paints", static_cast<int>(paints.size())}, {"warmup_inputs", warmup_frames},
        {"requested_interval_ms", target_interval_ms}, {"elapsed_ms", sequence.elapsed_ms},
        {"release_and_ui_commit_ms", sequence.release_and_ui_commit_ms},
        {"press_and_snap_candidates_ms", sequence.press_ms}, {"snap_enabled",canvas.snap_enabled()},
        {"revision_before", static_cast<qint64>(revision_before)}, {"revision_after", static_cast<qint64>(revision_after)},
        {"committed_notifications", commits}, {"sufficient_interval_samples", enough},
        {"meets_30fps_p95_interval_budget", floor}, {"meets_60fps_p95_interval_budget", target},
        {"viewport_width", canvas.width()}, {"viewport_height", canvas.height()},
        {"device_pixel_ratio", canvas.devicePixelRatioF()}, {"interval", interval_stats},
        {"semantic_preview",distribution(previews)},{"projection",distribution(projections)},
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

std::optional<double> p95(const QJsonObject& summary, const char* distribution_name) {
    const auto distribution=summary.value(distribution_name).toObject();
    const auto value=distribution.value("p95_ms");
    if (!value.isDouble()) return std::nullopt;
    return value.toDouble();
}

QJsonObject run_q1_aba(const QString& output, const QString& fixture_path, const QString& manifest_path) {
    QJsonObject result{{"schema","nect-visible-viewport-benchmark-1"},
        {"qualification","REQ-183 Q1 PERF-API-SEED-01 + PERF-TELEMETRY-AB-01"},
        {"started_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"measurement_boundary","Visible production Window/Canvas QWidget paint completion; input to end of paint includes Session preview/evaluation. Native compositor/GPU presentation is not measured."},
        {"input_method","QMouseEvent/QWheelEvent delivered to one owned, exposed production Canvas; normal queued updates and event loop; no forced repaint."},
        {"frame_interval_boundary","Consecutive input-triggered QWidget paint completions in each sequence; input cadence and coalescing included. First interval omitted."},
        {"quantile_method","Nearest rank"},
        {"platform",QGuiApplication::platformName()},{"qt_version",qVersion()},
        {"os",QSysInfo::prettyProductName()},{"cpu_architecture",QSysInfo::currentCpuArchitecture()},
        {"build_cpu_architecture",QSysInfo::buildCpuArchitecture()},
        {"floor_interval_budget_ms",floor_budget_ms},{"target_interval_budget_ms",target_budget_ms},
        {"q1_overhead_oracle","B p95 <= max(A1 p95 + 1.0 ms, A1 p95 * 1.10)"},
        {"q1_a2_drift_oracle","abs(A2 p95 - A1 p95) <= max(1.0 ms, A1 p95 * 0.10)"},
        {"common_frame_observer","Installed identically for A1/B/A2; it records paint, input-to-paint, interval, viewport and DPR. Detailed semantic/projection samples are enabled only for B."}};
    try {
        require(QGuiApplication::platformName()=="windows",
                "Run Q1 with the Windows Qt platform; offscreen/minimal is not visible-host evidence");
        QFile fixture(fixture_path);
        require(fixture.open(QIODevice::ReadOnly),"Cannot read the exact Q1 fixture file");
        const auto fixture_bytes=fixture.readAll();
        fixture.close();
        require(!fixture_bytes.isEmpty(),"Q1 fixture file is empty");
        const auto fixture_sha=QCryptographicHash::hash(fixture_bytes,QCryptographicHash::Sha256).toHex();
        QJsonParseError native_error;
        const auto native_document=QJsonDocument::fromJson(fixture_bytes,&native_error);
        require(native_error.error==QJsonParseError::NoError&&native_document.isObject(),
                "Q1 fixture is not valid native JSON");
        QFile manifest_file(manifest_path);
        require(manifest_file.open(QIODevice::ReadOnly),"Cannot read the Q1 fixture manifest");
        const auto manifest_bytes=manifest_file.readAll();
        manifest_file.close();
        const auto manifest_sha=QCryptographicHash::hash(manifest_bytes,QCryptographicHash::Sha256).toHex();
        QJsonParseError manifest_error;
        const auto manifest_document=QJsonDocument::fromJson(manifest_bytes,&manifest_error);
        require(manifest_error.error==QJsonParseError::NoError&&manifest_document.isObject(),
                "Q1 fixture manifest is not valid JSON");
        const auto manifest=manifest_document.object();
        require(manifest.value("fixture_sha256").toString().toLatin1()==fixture_sha,
                "Fixture SHA-256 does not match its public API manifest");
        require(manifest.value("byte_identical_second_process_replay").toBool()||
                manifest.value("semantic_second_process_replay").toBool(),
                "Manifest does not prove byte or semantic equivalence in the second process");
        const auto replay_processes=manifest.value("processes").toArray();
        require(replay_processes.size()==2,"Fixture manifest must record two independent API replay processes");
        const auto replay_a=replay_processes.at(0).toObject();
        const auto replay_b=replay_processes.at(1).toObject();
        require(replay_a.value("process_id").toInteger()!=replay_b.value("process_id").toInteger(),
                "Fixture manifest replay records do not identify two distinct processes");
        require(replay_a.value("readback_equal").toBool()&&replay_b.value("readback_equal").toBool()&&
                replay_a.value("final_revision").toInteger()==1&&replay_b.value("final_revision").toInteger()==1,
                "Independent API replays did not both verify native readback at revision one");
        require(replay_a.value("document_id").toString()==replay_b.value("document_id").toString()&&
                replay_a.value("composition_id").toString()==replay_b.value("composition_id").toString()&&
                replay_a.value("artboard_id").toString()==replay_b.value("artboard_id").toString()&&
                replay_a.value("normalized_native_sha256").toString()==replay_b.value("normalized_native_sha256").toString(),
                "Independent API process replay identities or normalized native state differ");

        const auto native=native_document.object();
        const auto compositions=native.value("compositions").toArray();
        const auto objects=native.value("objects").toArray();
        std::size_t point_count=0;
        for (const auto& object_value:objects)
            for (const auto& contour_value:object_value.toObject().value("contours").toArray())
                point_count+=static_cast<std::size_t>(contour_value.toObject().value("points").toArray().size());
        require(!compositions.isEmpty(),"Q1 fixture has no Composition");
        const auto composition=compositions.at(0).toObject();
        const auto artboards=composition.value("artboards").toArray();
        require(!artboards.isEmpty(),"Q1 fixture has no Artboard");
        const auto composition_id=composition.value("id").toString();
        const auto artboard_id=artboards.at(0).toObject().value("id").toString();
        require(objects.size()==manifest.value("object_count").toInt()&&
                static_cast<qint64>(point_count)==manifest.value("point_count").toInteger(),
                "Native fixture object/point counts disagree with its manifest");
        require(native.value("id").toString()==manifest.value("document_id").toString()&&
                composition_id==manifest.value("composition_id").toString()&&
                artboard_id==manifest.value("artboard_id").toString(),
                "Native fixture stable identity disagrees with its manifest");

        QTemporaryDir scratch(QDir::tempPath()+"/nect-perf-q1-XXXXXX");
        require(scratch.isValid(),"Cannot allocate the benchmark-owned scratch directory");
        Window window(scratch.path()+"/recovery");
        window.setWindowTitle(QStringLiteral("Nect Q1 Canvas performance qualification"));
        window.resize(1440,900);
        window.show();window.raise();window.activateWindow();
        int exposed_wait=0;
        while ((!window.windowHandle()||!window.windowHandle()->isExposed())&&exposed_wait<5000) {
            wait_events(20);exposed_wait+=20;
        }
        require(window.windowHandle()&&window.windowHandle()->isExposed(),
                "Q1 owned production Window did not become visible and exposed");
        const auto window_identity=QString::number(static_cast<quintptr>(window.winId()),16);
        const auto window_handle_identity=QString::number(static_cast<quintptr>(window.windowHandle()->winId()),16);
        const auto process_id=QCoreApplication::applicationPid();
        std::vector<Canvas::FrameTiming> observed_frames;
        window.canvas->set_frame_observer([&](const Canvas::FrameTiming& frame){observed_frames.push_back(frame);});
        QStringList errors;
        int commits=0;
        const auto original_error=window.canvas->error;
        const auto original_changed=window.canvas->document_changed;
        window.canvas->error=[&](QString message){errors.push_back(message);if(original_error)original_error(std::move(message));};
        window.canvas->document_changed=[&]{++commits;if(original_changed)original_changed();};
        InteractionCleanup cleanup{*window.canvas};

        const auto viewport_receipt=[&] {
            const auto* screen=window.screen();
            return QJsonObject{
                {"window_x",window.x()},{"window_y",window.y()},
                {"window_width",window.width()},{"window_height",window.height()},
                {"canvas_width",window.canvas->width()},{"canvas_height",window.canvas->height()},
                {"device_pixel_ratio",window.canvas->devicePixelRatioF()},
                {"screen_name",screen?screen->name():QString{}},
                {"screen_x",screen?screen->geometry().x():0},
                {"screen_y",screen?screen->geometry().y():0},
                {"screen_width",screen?screen->geometry().width():0},
                {"screen_height",screen?screen->geometry().height():0},
                {"logical_dpi",screen?screen->logicalDotsPerInch():0},
                {"reported_refresh_hz",screen?screen->refreshRate():0}};
        };
        const auto prewarm_path=QDir(scratch.path()).filePath(QStringLiteral("whole-schedule-prewarm.nect"));
        require(QFile::copy(fixture_path,prewarm_path),"Cannot create an exact-byte whole-schedule prewarm fixture copy");
        QFile prewarm_fixture(prewarm_path);
        require(prewarm_fixture.open(QIODevice::ReadOnly),"Cannot verify the whole-schedule prewarm fixture");
        require(QCryptographicHash::hash(prewarm_fixture.readAll(),QCryptographicHash::Sha256).toHex()==fixture_sha,
                "Whole-schedule prewarm fixture copy does not match the exact source SHA");
        window.host.open(prewarm_path);
        require(window.host.session.document().id==native.value("id").toString().toStdString()&&
                window.host.session.revision()==0,
                "Whole-schedule prewarm did not open the exact public fixture at revision zero");
        window.canvas->set_active_artboard(composition_id.toStdString(),artboard_id.toStdString(),true);
        window.canvas->set_timing_capture_enabled(false);
        errors.clear();commits=0;
        QJsonArray rehearsal_operations;
        for(const auto operation:{Operation::pan,Operation::zoom,Operation::point,Operation::handle,Operation::transform}) {
            const auto revision_before=window.host.session.revision();
            observed_frames.clear();
            sequence(window,operation,warmup_frames,false,errors,false,1);
            const auto warmup_paints=static_cast<int>(observed_frames.size());
            observed_frames.clear();
            sequence(window,operation,measured_frames,false,errors,false,1);
            const auto rehearsal_paints=static_cast<int>(observed_frames.size());
            require(window.host.session.revision()==revision_before&&!window.host.session.gesture_active(),
                    "Unmeasured whole-schedule rehearsal changed or left an active Session gesture");
            rehearsal_operations.push_back(QJsonObject{{"operation",operation_name(operation)},
                {"warmup_inputs",warmup_frames},{"rehearsal_inputs",measured_frames},
                {"warmup_observed_paints",warmup_paints},{"rehearsal_observed_paints",rehearsal_paints},
                {"detailed_timing_capture",false},{"revision_before",static_cast<qint64>(revision_before)},
                {"revision_after",static_cast<qint64>(window.host.session.revision())}});
        }
        check_errors(errors);
        require(window.host.session.revision()==0&&commits==0,
                "Whole-schedule rehearsal changed the public fixture's starting state");
        const auto prewarm_viewport=viewport_receipt();
        QJsonObject viewport_identity=prewarm_viewport;
        const QJsonObject whole_schedule_rehearsal{{"completed",true},{"fixture_sha256",QString::fromLatin1(fixture_sha)},
            {"opened_file_path",QFileInfo(prewarm_path).absoluteFilePath()},{"detailed_timing_capture",false},
            {"common_frame_observer",true},{"viewport_identity",prewarm_viewport},
            {"ordered_operations",QJsonArray{"pan","zoom","point","handle","transform"}},
            {"warmup_inputs_per_operation",warmup_frames},{"rehearsal_inputs_per_operation",measured_frames},
            {"rehearsal_operations",rehearsal_operations},{"starting_revision",0},
            {"ending_revision",static_cast<qint64>(window.host.session.revision())},
            {"committed_notifications",commits}};

        struct Pass { QString name; bool detailed; QJsonArray operations; QJsonObject scene; };
        std::array<Pass,3> passes{{Pass{QStringLiteral("A1-off"),false,{},{}},
            Pass{QStringLiteral("B-on"),true,{},{}},Pass{QStringLiteral("A2-off"),false,{},{}}}};
        bool all_samples=true;
        for (auto& pass:passes) {
            require(QCryptographicHash::hash([&] { QFile source(fixture_path); require(source.open(QIODevice::ReadOnly),"Cannot verify source fixture before a pass"); return source.readAll(); }(),QCryptographicHash::Sha256).toHex()==fixture_sha,
                    "Source fixture SHA changed before "+pass.name.toStdString());
            require(QCryptographicHash::hash([&] { QFile source(manifest_path); require(source.open(QIODevice::ReadOnly),"Cannot verify source manifest before a pass"); return source.readAll(); }(),QCryptographicHash::Sha256).toHex()==manifest_sha,
                    "Source fixture manifest SHA changed before "+pass.name.toStdString());
            const auto pass_path=QDir(scratch.path()).filePath(pass.name+QStringLiteral(".nect"));
            require(QFile::copy(fixture_path,pass_path),"Cannot create an owned exact-byte fixture copy for "+pass.name.toStdString());
            QFile pass_fixture(pass_path);
            require(pass_fixture.open(QIODevice::ReadOnly),"Cannot read an owned pass fixture");
            const auto pass_sha=QCryptographicHash::hash(pass_fixture.readAll(),QCryptographicHash::Sha256).toHex();
            require(pass_sha==fixture_sha,"Owned pass fixture copy does not match the exact source SHA");
            window.host.open(pass_path);
            require(window.host.session.document().id==native.value("id").toString().toStdString()&&
                    window.host.session.revision()==0,
                    "Opening the exact Q1 fixture copy changed stable document identity or starting revision");
            window.canvas->set_active_artboard(composition_id.toStdString(),artboard_id.toStdString(),true);
            window.canvas->set_timing_capture_enabled(pass.detailed);
            observed_frames.clear();errors.clear();commits=0;
            fit(window);wait_events(50);
            const auto revision_start=window.host.session.revision();
            const auto pass_window_identity=QString::number(static_cast<quintptr>(window.winId()),16);
            require(pass_window_identity==window_identity,"A/B/A pass changed the visible Window identity");
            require(window.isWindow()&&window.isVisible()&&window.windowHandle()&&window.windowHandle()->isExposed(),
                    "Q1 pass did not run in the same visible/exposed Qt top-level Window");
            const auto pass_window_visible=window.isVisible();
            const auto pass_window_exposed=window.windowHandle()->isExposed();
            const auto pass_window_active=window.isActiveWindow();
            const auto pass_window_handle_identity=QString::number(static_cast<quintptr>(window.windowHandle()->winId()),16);
            require(pass_window_handle_identity==window_handle_identity,
                    "A/B/A pass changed the native Qt Window handle identity");
            const auto pass_viewport=viewport_receipt();
            require(pass_viewport==viewport_identity,"Viewport, DPI or display identity changed between Q1 passes");
            const auto pass_start_clock=run_clock.elapsed();
            for (const auto operation:{Operation::pan,Operation::zoom,Operation::point,Operation::handle,Operation::transform}) {
                sequence(window,operation,warmup_frames,false,errors,false,1);
                const auto before=window.host.session.revision();
                const auto commits_before=commits;
                const auto measurement=sequence(window,operation,measured_frames,true,errors,false,1,&observed_frames);
                const auto after=window.host.session.revision();
                require(commits-commits_before==(edits_document(operation)?1:0),
                        "Unexpected committed notification count in Q1 "+pass.name.toStdString());
                const auto summary=summarize(*window.canvas,operation,measurement,before,after,
                    commits-commits_before,&observed_frames,pass.detailed);
                all_samples=all_samples&&summary.value("sufficient_interval_samples").toBool();
                pass.operations.push_back(summary);
                if (edits_document(operation)) {
                    window.host.session.undo(window.host.session.revision());
                    window.host.edited();wait_events(40);
                }
            }
            pass.scene={{"name","representative-public-api-fixture"},
                {"fixture_file",QFileInfo(fixture_path).fileName()},
                {"opened_file_path",QFileInfo(pass_path).absoluteFilePath()},
                {"fixture_sha256",QString::fromLatin1(fixture_sha)},
                {"fixture_copy_sha256",QString::fromLatin1(pass_sha)},
                {"command_stream_sha256",manifest.value("command_stream_sha256")},
                {"seed",manifest.value("seed")},{"config",manifest.value("config")},
                {"document_id",QString::fromStdString(window.host.session.document().id)},
                {"composition_id",composition_id},{"artboard_id",artboard_id},
                {"path_count",objects.size()},{"point_count",static_cast<qint64>(point_count)},
                {"fixture_revision",static_cast<qint64>(revision_start)},
                {"final_revision",static_cast<qint64>(window.host.session.revision())},
                {"viewport_identity",pass_viewport},
                {"window_is_top_level",window.isWindow()},{"window_visible_at_start",pass_window_visible},
                {"window_exposed_at_start",pass_window_exposed},
                {"qt_window_handle_id_hex",pass_window_handle_identity},
                {"active_window_at_start",pass_window_active},
                {"same_process_id",process_id},{"window_identity_hex",window_identity},
                {"detailed_timing_capture",pass.detailed},
                {"common_frame_observer",true},{"phase_elapsed_ms",run_clock.elapsed()-pass_start_clock},
                {"revision_after_undo_cleanup",static_cast<qint64>(window.host.session.revision())},
                {"operations",pass.operations}};
            pass.scene["active_window_at_end"]=window.isActiveWindow();
            pass.scene["window_visible_at_end"]=window.isVisible();
            pass.scene["window_exposed_at_end"]=window.windowHandle()&&window.windowHandle()->isExposed();
            require(window.isVisible()&&window.windowHandle()&&window.windowHandle()->isExposed(),
                    "Q1 owned Window lost visibility/exposure during "+pass.name.toStdString());
            window.host.flush();
            require(QCryptographicHash::hash([&] { QFile source(fixture_path); require(source.open(QIODevice::ReadOnly),"Cannot verify source fixture after a pass"); return source.readAll(); }(),QCryptographicHash::Sha256).toHex()==fixture_sha,
                    "Source fixture SHA changed during "+pass.name.toStdString());
            require(QCryptographicHash::hash([&] { QFile source(manifest_path); require(source.open(QIODevice::ReadOnly),"Cannot verify source manifest after a pass"); return source.readAll(); }(),QCryptographicHash::Sha256).toHex()==manifest_sha,
                    "Source fixture manifest SHA changed during "+pass.name.toStdString());
        }
        window.canvas->set_frame_observer({});
        window.canvas->error=original_error;window.canvas->document_changed=original_changed;
        QFile final_fixture(fixture_path);
        require(final_fixture.open(QIODevice::ReadOnly),"Cannot verify the source fixture after A/B/A");
        const auto final_fixture_sha=QCryptographicHash::hash(final_fixture.readAll(),QCryptographicHash::Sha256).toHex();
        require(final_fixture_sha==fixture_sha,"Visible benchmark modified the public API source fixture");
        QFile final_manifest(manifest_path);
        require(final_manifest.open(QIODevice::ReadOnly),"Cannot verify the source fixture manifest after A/B/A");
        const auto final_manifest_sha=QCryptographicHash::hash(final_manifest.readAll(),QCryptographicHash::Sha256).toHex();
        require(final_manifest_sha==manifest_sha,"Visible benchmark modified the public API source manifest");

        QJsonArray pass_array;
        for (const auto& pass:passes) pass_array.push_back(QJsonObject{{"name",pass.name},
            {"detailed_timing_capture",pass.detailed},{"scene",pass.scene}});
        QJsonArray comparisons;
        bool all_overhead=true,all_a2_stable=true,all_a1_floor=true,all_b_floor=true;
        for (qsizetype i=0;i<passes[0].operations.size();++i) {
            const auto a1=passes[0].operations.at(i).toObject();
            const auto b=passes[1].operations.at(i).toObject();
            const auto a2=passes[2].operations.at(i).toObject();
            const auto a1_interval=p95(a1,"interval"),b_interval=p95(b,"interval"),a2_interval=p95(a2,"interval");
            const auto a1_input=p95(a1,"input_to_paint"),b_input=p95(b,"input_to_paint"),a2_input=p95(a2,"input_to_paint");
            const auto a1_paint=p95(a1,"paint"),b_paint=p95(b,"paint"),a2_paint=p95(a2,"paint");
            const bool samples=a1_interval&&b_interval&&a2_interval&&a1_input&&b_input&&a2_input&&a1_paint&&b_paint&&a2_paint&&
                a1.value("sufficient_interval_samples").toBool()&&b.value("sufficient_interval_samples").toBool()&&a2.value("sufficient_interval_samples").toBool();
            const double overhead_limit=samples?std::max(*a1_interval+1.0,*a1_interval*1.10):0;
            const double drift_limit=samples?std::max(1.0,*a1_interval*0.10):0;
            const bool overhead=samples&&*b_interval<=overhead_limit;
            const bool a2_stable=samples&&std::abs(*a2_interval-*a1_interval)<=drift_limit;
            const bool a1_floor=samples&&*a1_interval<=floor_budget_ms;
            const bool b_floor=samples&&*b_interval<=floor_budget_ms;
            all_samples=all_samples&&samples;all_overhead=all_overhead&&overhead;
            all_a2_stable=all_a2_stable&&a2_stable;all_a1_floor=all_a1_floor&&a1_floor;
            all_b_floor=all_b_floor&&b_floor;
            QJsonObject compare{{"operation",a1.value("operation")},{"sufficient_samples",samples},
                {"A1_interval_p95_ms",samples?QJsonValue(*a1_interval):QJsonValue(QJsonValue::Null)},
                {"B_interval_p95_ms",samples?QJsonValue(*b_interval):QJsonValue(QJsonValue::Null)},
                {"B_interval_p95_limit_ms",samples?QJsonValue(overhead_limit):QJsonValue(QJsonValue::Null)},
                {"B_overhead_pass",overhead},{"A1_meets_30fps_floor",a1_floor},
                {"B_meets_30fps_floor",b_floor},
                {"A2_interval_p95_ms",samples?QJsonValue(*a2_interval):QJsonValue(QJsonValue::Null)},
                {"A2_drift_ms",samples?QJsonValue(*a2_interval-*a1_interval):QJsonValue(QJsonValue::Null)},
                {"A2_drift_bound_ms",samples?QJsonValue(drift_limit):QJsonValue(QJsonValue::Null)},
                {"A2_stable",a2_stable}};
            const auto delta=[&](const char* key,const std::optional<double>& base,const std::optional<double>& later) {
                compare[QString::fromLatin1(key)]=base&&later?QJsonValue(*later-*base):QJsonValue(QJsonValue::Null);
            };
            delta("B_input_to_paint_p95_delta_ms",a1_input,b_input);
            delta("B_paint_p95_delta_ms",a1_paint,b_paint);
            delta("A2_input_to_paint_p95_delta_ms",a1_input,a2_input);
            delta("A2_paint_p95_delta_ms",a1_paint,a2_paint);
            comparisons.push_back(compare);
        }
        const bool visible_at_completion=window.isVisible();
        const bool exposed_at_completion=window.windowHandle()&&window.windowHandle()->isExposed();
        require(visible_at_completion&&exposed_at_completion,"Q1 owned Window was not visible/exposed at the end of A/B/A");
        window.close();wait_events(20);
        result["completed"]=true;
        result["fixture"]=QJsonObject{{"path",QFileInfo(fixture_path).absoluteFilePath()},
            {"sha256",QString::fromLatin1(fixture_sha)},
            {"command_stream_sha256",manifest.value("command_stream_sha256")},
            {"manifest_sha256",QString::fromLatin1(manifest_sha)},
            {"native_version",native.value("version")},{"document_id",native.value("id")},
            {"composition_id",composition_id},{"artboard_id",artboard_id},
            {"object_count",objects.size()},{"point_count",static_cast<qint64>(point_count)},
            {"starting_revision",0},{"api_fixture_revision",manifest.value("fixture_revision")},
            {"second_process_byte_equivalent",manifest.value("byte_identical_second_process_replay")},
            {"second_process_semantic_equivalent",manifest.value("semantic_second_process_replay")}};
        result["passes"]=pass_array;result["comparisons"]=comparisons;
        result["whole_schedule_rehearsal"]=whole_schedule_rehearsal;
        result["all_operations_have_sufficient_samples"]=all_samples;
        result["all_A1_operations_meet_30fps_p95_interval_floor"]=all_a1_floor;
        result["all_B_operations_meet_30fps_p95_interval_floor"]=all_b_floor;
        result["all_B_operations_meet_telemetry_overhead_bound"]=all_overhead;
        result["all_A2_operations_within_A1_drift_bound"]=all_a2_stable;
        result["same_visible_window_for_all_passes"]=true;
        result["same_process_for_all_passes"]=true;
        result["same_input_schedule_for_all_passes"]=true;
        result["same_viewport_and_dpi_for_all_passes"]=true;
        result["viewport_identity"]=viewport_identity;
        result["visible_window_receipt"]=QJsonObject{{"process_id",process_id},
            {"qt_window_handle_id_hex",window_handle_identity},
            {"top_level",window.isWindow()},{"visible_at_completion",visible_at_completion},
            {"exposed_at_completion",exposed_at_completion},
            {"platform",QGuiApplication::platformName()}};
        result["input_schedule"]=QJsonObject{{"ordered_operations",QJsonArray{"pan","zoom","point","handle","transform"}},
            {"warmup_inputs_per_operation",warmup_frames},{"measured_inputs_per_operation",measured_frames},
            {"input_spacing_ms",target_interval_ms},{"unmeasured_full_schedule_rehearsals_before_A1",1},
            {"schedule_reused_for_all_passes",true}};
        const bool pass=all_samples&&all_a1_floor&&all_b_floor&&all_overhead&&all_a2_stable;
        result["performance_status"]=(all_a1_floor&&all_b_floor&&all_overhead)?"PASS":"RED";
        result["qualification_status"]=pass?"PASS":(!all_samples||!all_a2_stable?"INCONCLUSIVE":"RED");
        result["qualification_status_reason"]=pass?"All sampled operation oracles pass":
            !all_samples?"One or more operations lack sufficient samples":
            !all_a2_stable?"A2 exceeded the frozen A1 drift bound; the paired A/B result is inconclusive":
            "One or more performance or telemetry-overhead oracles are RED";
        result["elapsed_ms"]=run_clock.elapsed();
        write_result(output,result);
        return result;
    } catch (const std::exception& error) {
        result["completed"]=false;result["qualification_status"]="BLOCKED";
        result["execution_error"]=QString::fromUtf8(error.what());
        result["elapsed_ms"]=run_clock.elapsed();
        write_result(output,result);
        throw;
    }
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
    const bool q1_aba=app.arguments().size()==5&&app.arguments().at(2)=="--q1-aba";
    if (q1_aba) {
        const auto output=app.arguments().at(1);
        run_clock.start();
        try {
            const auto result=run_q1_aba(output,app.arguments().at(3),app.arguments().at(4));
            const auto status=result.value("qualification_status").toString();
            std::cout<<"Q1 visible A/B/A qualification "<<status.toStdString()<<": "<<output.toStdString()<<'\n';
            return 0; // RED and INCONCLUSIVE are machine results, not execution errors.
        } catch (const std::exception& error) {
            std::cerr<<"Q1 visible A/B/A benchmark blocked: "<<error.what()<<'\n';
            return 1;
        }
    }
    if (app.arguments().size() < 2 || app.arguments().size()>3 ||
        (app.arguments().size()==3&&app.arguments().at(2)!="--repeat"&&app.arguments().at(2)!="--text"&&app.arguments().at(2)!="--polystar"&&app.arguments().at(2)!="--multi"&&app.arguments().at(2)!="--expressions"&&app.arguments().at(2)!="--compositing"&&app.arguments().at(2)!="--offset"&&app.arguments().at(2)!="--assets"&&app.arguments().at(2)!="--layout")) {
        std::cerr << "Usage: canvas_benchmark <result.json> [--repeat|--text|--polystar|--multi|--expressions|--compositing|--offset|--assets|--layout]\n"
                     "   or: canvas_benchmark <result.json> --q1-aba <fixture.nect> <fixture-manifest.json>\n";
        return 2;
    }
    const auto output = app.arguments().at(1);
    const bool repeated=app.arguments().contains("--repeat");
    const bool text_scene=app.arguments().contains("--text");
    const bool polystar_scene=app.arguments().contains("--polystar");
    const bool expression_scene=app.arguments().contains("--expressions");
    const bool assets_scene=app.arguments().contains("--assets");
    const bool layout_scene=app.arguments().contains("--layout");
    const bool offset_scene=app.arguments().contains("--offset");
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
        for (const auto paths : ((repeated||text_scene||polystar_scene||offset_scene||assets_scene)?std::vector<int>{2}:std::vector<int>{2,80})) {
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
            if(layout_scene) {
                const auto& comp=window.host.session.document().compositions.front();
                ArtboardLayout layout;layout.margin=Margin{40,20,40,20};
                layout.grid=Grid{"bench-layout-grid",{40,20,880,600},2,2,20,20};
                window.host.session.apply({
                    AddGuide{comp.id,{"bench-guide-x1","Guide X1","x",100}},
                    AddGuide{comp.id,{"bench-guide-x2","Guide X2","x",480}},
                    AddGuide{comp.id,{"bench-guide-y1","Guide Y1","y",120}},
                    AddGuide{comp.id,{"bench-guide-y2","Guide Y2","y",320}},
                    SetArtboardLayout{comp.id,comp.artboards.front().id,layout}
                },window.host.session.revision());
                window.host.edited();wait_events(40);
            }
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
            if(assets_scene) {
                std::vector<Command> commands;const auto comp=window.host.session.document().compositions.front().id;
                for(int i=0;i<8;++i) {
                    RasterPixels pixels{512,384,{}};pixels.rgba.reserve(512*384*4);
                    for(unsigned y=0;y<384;++y)for(unsigned x=0;x<512;++x) {
                        const auto value=(x*73856093U)^(y*19349663U)^(unsigned(i)*83492791U);
                        pixels.rgba.insert(pixels.rgba.end(),{static_cast<unsigned char>(value),static_cast<unsigned char>(value>>8),static_cast<unsigned char>(value>>16),255});
                    }
                    const auto id="bench-asset-"+std::to_string(i);commands.push_back(AddRasterAsset{{id,id,"embedded","",make_raster(encode_raster_png(pixels))}});
                }
                for(int i=0;i<24;++i) {
                    const auto id="bench-image-"+std::to_string(i);const auto x=60+(i%6)*148,y=160+(i/6)*114;
                    commands.push_back(CreateImage{comp,"",id,"Image "+std::to_string(i),{"bench-asset-"+std::to_string(i%8),{132},{99}}});
                    commands.push_back(Set{{id,"","transform.tx"},double(x)});commands.push_back(Set{{id,"","transform.ty"},double(y)});
                    if(i%8==0) {
                        auto mask=default_primitive(id+"-source","nect.shape.circle");mask.parameters.at("center_x").literal=x+66;mask.parameters.at("center_y").literal=y+49.5;mask.parameters.at("radius").literal=45;
                        commands.push_back(CreatePrimitive{comp,"",id+"-mask","Image crop",mask});commands.push_back(SetVisibility{id+"-mask",false});
                        commands.push_back(SetMask{id,GeometryMask{id+"-clip",id+"-mask"}});commands.push_back(SetCompositing{id,"multiply",false});commands.push_back(Set{{id,"","composite.opacity"},.8});
                    }
                }
                apply_serializable(window.host.session,commands,window.host.session.revision());window.host.edited();wait_events(100);
            }
            if(offset_scene) {
                std::vector<Command> commands;const auto comp=window.host.session.document().compositions.front().id;
                for(int i=0;i<24;++i) {
                    const auto id="bench-offset-"+std::to_string(i);
                    auto source=default_primitive(id+"-source",i%2?"nect.shape.star":"nect.shape.circle");
                    source.parameters.at("center_x").literal=105+(i%6)*148;
                    source.parameters.at("center_y").literal=190+(i/6)*115;
                    source.parameters.at(i%2?"outer_radius":"radius").literal=35;
                    if(i%2){source.parameters.at("inner_radius").literal=20;source.parameters.at("points").literal=6;}
                    commands.push_back(CreatePrimitive{comp,"",id,"Offset study "+std::to_string(i),source});
                    auto offset=default_operation(id+"-op","nect.shape.offset");offset.line_join="round";
                    offset.parameters.at("amount").expression=Expression{"ref(\"bench-path-0\",\"bench-point-0-0\",\"x\") / 14",1};
                    commands.push_back(AddOperation{id,offset,1});
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
            // Keep legacy benchmark output populated after production capture
            // became opt-in by default.
            window.canvas->set_timing_capture_enabled(true);
            const auto* screen = window.screen();
            QJsonObject scene{{"name", layout_scene?(paths == 2 ? "layout-lightweight" : "layout-representative"):
                repeated?"repeated-paint":paths == 2 ? "lightweight" : "representative"},
                {"path_count", paths}, {"point_count", paths * 4}, {"points_per_path", 4},
                {"binding_count", 0}, {"group_count", 0}, {"artboard_count", 1},
                {"artboard_width", 960}, {"artboard_height", 640},
                {"fixture", layout_scene?
                    "Semantic CreatePath commands; four authored Guides, Margin 40/20/40/20 and a 2x2 Grid with 20 du gutters; visible production Canvas overlays.":
                    "Semantic CreatePath commands; deterministic 10-column grid; four cubic anchors per open path; 18 du polar handles; native default stroke."},
                {"layout_overlays", layout_scene},
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
            if(offset_scene) {
                scene["name"]="mixed-offset";scene["primitive_count"]=24;scene["offset_count"]=24;scene["expression_count"]=24;
                scene["fixture"]="Two authored four-anchor curves plus twelve Circles and twelve six-point Stars, all with retained round-join Offset after Stroke. Every Amount references the first curve point X / 14, so point dragging changes all24 evaluated outlines. All shapes use normal validation, projection and protection.";
            }
            if(assets_scene) {
                scene["name"]="mixed-image-assets";scene["image_placements"]=24;scene["unique_assets"]=8;scene["decoded_pixels"]=8*512*384;scene["mask_count"]=3;
                std::size_t bytes=0;for(const auto& [id,asset]:window.host.session.document().raster_assets){(void)id;bytes+=asset.payload->bytes().size();}scene["accepted_source_bytes"]=qint64(bytes);
                scene["fixture"]="Two editable four-anchor curves with24 image placements sharing8 embedded512x384 RGB noise PNG assets (1.57MP total), three hidden circular mask sources and three Multiply/.8 leaves. Visible full Window pan/zoom and original curve point/handle/translation with normal protection. Decoded image projections reused across gestures; this does not time import/reload or maximum-size scenes.";
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
