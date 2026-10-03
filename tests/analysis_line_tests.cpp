#include "nect/analysis_adoption.hpp"
#include "nect/io.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#ifdef NECT_ANALYSIS_LINE_DESKTOP_TESTS
#include "analysis_line_control.hpp"
#include "host.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonArray>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#endif

using namespace nect;
namespace {
int checks = 0;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); ++checks; }
template<class F> void rejects(const char* code, F&& f) {
    try { f(); } catch (const Error& error) { check(error.code == code, "Expected refusal code"); return; }
    throw std::runtime_error("Expected refusal");
}
struct Snapshot {
    Document document;
    std::uint64_t revision;
    HistoryInfo history;
    std::string native;
    explicit Snapshot(const Session& session) : document(session.document()), revision(session.revision()),
        history(session.history()), native(encode(session.document())) {}
    bool unchanged(const Session& session) const {
        return document == session.document() && revision == session.revision() &&
            history == session.history() && native == encode(session.document());
    }
};
Document fixture() {
    auto document = empty_document("analysis-line-doc", "comp", "board");
    auto& board = document.compositions.front().artboards.front();
    board.x = -10; board.y = 20; board.width = 12; board.height = 8;
    ColorValue white; white.rgba = {1, 1, 1, 1}; board.background = white;
    Object source; source.id = "source"; source.name = "Retained thin artwork";
    Contour contour; contour.id = "source-contour"; contour.closed = true;
    // At scale 2 this filled rectangle covers exactly x=1..5,y=2, making
    // one real R09-D4 five-pixel horizontal candidate, not a mock analysis.
    for (const auto xy : std::vector<Vec2>{{-9.5,21},{-7,21},{-7,21.5},{-9.5,21.5}}) {
        Point point; point.id = "source-p" + std::to_string(contour.points.size());
        point.x.literal = xy.x; point.y.literal = xy.y; contour.points.push_back(point);
    }
    source.contours = {contour};
    source.stack.push_back(default_operation("source-fill", "nect.paint.fill"));
    document.objects.emplace(source.id, source);
    document.compositions.front().roots = {source.id};
    return document;
}
void pure_geometry() {
    const AnalysisContourFrame frame{{-10, 20}, 2, 24, 16};
    const std::vector<Id> ids{"line-start", "line-end"};
    const std::vector<AnalysisLineCandidate> runs{
        {"horizontal", {1,2}, {5,2}, 5}, {"vertical", {0,0}, {0,2}, 3},
        {"down_diagonal", {0,0}, {2,2}, 3}, {"up_diagonal", {0,2}, {2,0}, 3}};
    for (const auto& run : runs) {
        const auto contour = adopt_analysis_thin_line(run, frame, "line-contour", ids);
        check(!contour.closed && contour.points.size() == 2 && contour.id == "line-contour", "Two-point open contour");
        const Vec2 endpoints[]{run.start, run.end};
        for (std::size_t i = 0; i < 2; ++i) {
            const auto& point = contour.points[i];
            check(point.id == ids[i], "Stable authored endpoint ID");
            check(point.x.literal == -10 + (endpoints[i].x + 0.5) / 2 &&
                point.y.literal == 20 + (endpoints[i].y + 0.5) / 2, "Pixel-center origin/scale conversion");
            check(!point.x.binding && !point.y.binding && !point.x.expression && !point.y.expression &&
                point.in_length.literal == 0 && point.out_length.literal == 0, "Straight literal anchors");
        }
    }
    auto fractional = frame; fractional.scale = 1.5;
    const auto contour = adopt_analysis_thin_line(runs.front(), fractional, "fractional", ids);
    check(contour.points.front().x.literal == -10 + 1.5 / 1.5 &&
        contour.points.back().x.literal == -10 + 5.5 / 1.5, "Fractional resolution retains center conversion");
    auto bad_frame = frame; bad_frame.scale = 0;
    rejects("EXPORT_SCALE", [&] { adopt_analysis_thin_line(runs.front(), bad_frame, "c", ids); });
    bad_frame.scale = 17;
    rejects("EXPORT_SCALE", [&] { adopt_analysis_thin_line(runs.front(), bad_frame, "c", ids); });
    bad_frame.scale = std::numeric_limits<double>::quiet_NaN();
    rejects("EXPORT_SCALE", [&] { adopt_analysis_thin_line(runs.front(), bad_frame, "c", ids); });
    bad_frame = frame; bad_frame.pixel_width = 0;
    rejects("INVALID_ANALYSIS_FRAME", [&] { adopt_analysis_thin_line(runs.front(), bad_frame, "c", ids); });
    bad_frame = frame; bad_frame.pixel_width = 2001; bad_frame.pixel_height = 2000;
    rejects("INVALID_ANALYSIS_FRAME", [&] { adopt_analysis_thin_line(runs.front(), bad_frame, "c", ids); });
    bad_frame = frame; bad_frame.origin.x = std::numeric_limits<double>::infinity();
    rejects("INVALID_ANALYSIS_FRAME", [&] { adopt_analysis_thin_line(runs.front(), bad_frame, "c", ids); });
    rejects("INVALID_ANALYSIS_IDS", [&] { adopt_analysis_thin_line(runs.front(), frame, "c", {}); });
    rejects("DUPLICATE_ID", [&] { adopt_analysis_thin_line(runs.front(), frame, "c", {"p", "p"}); });
    rejects("DUPLICATE_ID", [&] { adopt_analysis_thin_line(runs.front(), frame, "line-start", ids); });
    rejects("INVALID_ID", [&] { adopt_analysis_thin_line(runs.front(), frame, "invalid id", ids); });
    auto bad_run = runs.front(); bad_run.start.x = 1.5;
    rejects("INVALID_ANALYSIS_LINE", [&] { adopt_analysis_thin_line(bad_run, frame, "c", ids); });
    bad_run = runs.front(); bad_run.end.x = frame.pixel_width;
    rejects("INVALID_ANALYSIS_LINE", [&] { adopt_analysis_thin_line(bad_run, frame, "c", ids); });
    bad_run = runs.front(); bad_run.start.x = -1;
    rejects("INVALID_ANALYSIS_LINE", [&] { adopt_analysis_thin_line(bad_run, frame, "c", ids); });
    bad_run = runs.front(); bad_run.end.y = std::numeric_limits<double>::quiet_NaN();
    rejects("INVALID_ANALYSIS_LINE", [&] { adopt_analysis_thin_line(bad_run, frame, "c", ids); });
    bad_run = runs.front(); bad_run.direction = "arbitrary-angle";
    rejects("INVALID_ANALYSIS_LINE", [&] { adopt_analysis_thin_line(bad_run, frame, "c", ids); });
    bad_run = runs.front(); bad_run.length_pixels = 2;
    rejects("INVALID_ANALYSIS_LINE", [&] { adopt_analysis_thin_line(bad_run, frame, "c", ids); });
    bad_run = runs.front(); bad_run.length_pixels = 4;
    rejects("INVALID_ANALYSIS_LINE", [&] { adopt_analysis_thin_line(bad_run, frame, "c", ids); });
    bad_run = runs.front(); std::swap(bad_run.start, bad_run.end);
    rejects("INVALID_ANALYSIS_LINE", [&] { adopt_analysis_thin_line(bad_run, frame, "c", ids); });
    bad_frame = frame; bad_frame.scale = std::numeric_limits<double>::denorm_min();
    rejects("NON_FINITE", [&] { adopt_analysis_thin_line(runs.front(), bad_frame, "c", ids); });
}
Document pure_session() {
    Session session(fixture()); const Snapshot before(session);
    auto contour = adopt_analysis_thin_line({"horizontal", {1,2}, {5,2}, 5},
        {{-10,20},2,24,16}, "adopted-contour", {"adopted-start", "adopted-end"});
    session.apply({CreatePath{"comp", {}, "adopted-line", "Adopted thin line", {contour}}}, before.revision);
    const auto adopted = session.document(); const auto& object = adopted.objects.at("adopted-line");
    check(object.kind == Kind::path && !object.source && !object.contours.front().closed &&
        object.contours.front().points.size() == 2, "Canonical editable Path with two endpoints");
    check(session.revision() == before.revision + 1 &&
        session.history().states.size() == before.history.states.size() + 1, "One canonical adoption Undo entry");
    check(adopted.objects.at("source") == before.document.objects.at("source") &&
        adopted.compositions.front().roots == std::vector<Id>{"source", "adopted-line"}, "Source retained; new root Path");
    const auto& points = object.contours.front().points;
    check(points[0].x.literal == -9.25 && points[0].y.literal == 21.25 &&
        points[1].x.literal == -7.25 && points[1].y.literal == 21.25, "Exact two-point center geometry");
    session.undo(session.revision()); check(session.document() == before.document, "One Undo restores exact original document");
    session.redo(session.revision()); check(session.document() == adopted, "Redo retains endpoint and contour IDs");
    const Snapshot current(session);
    rejects("REVISION_CONFLICT", [&] { session.apply({CreatePath{"comp", {}, "stale-line", "Stale", {contour}}}, before.revision); });
    check(current.unchanged(session), "Stale canonical mutation leaves native/history unchanged");
    const auto native = encode(adopted);
    Session reopened(decode(native));
    check(reopened.document() == adopted && encode(reopened.document()) == native, "Native codec reopen preserves exact source and authored Path");
    reopened.apply({Set{{"adopted-line", "adopted-start", "x"}, -8.75}}, reopened.revision());
    check(reopened.document().objects.at("adopted-line").contours.front().points.front().x.literal == -8.75,
        "Reopened endpoint is directly editable by stable identity");
    reopened.undo(reopened.revision()); check(reopened.document() == adopted, "Reopened point edit Undo preserves source");
    return adopted;
}
void cold_core(const char* path) {
    std::ifstream file(path, std::ios::binary); check(file.good(), "Read cold-process native file");
    const std::string native{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    Session reader(decode(native)); check(encode(reader.document()) == native, "Cold process exact native reopen");
    check(reader.document().objects.at("source") == fixture().objects.at("source"), "Cold process exact source preservation");
    reader.apply({Set{{"adopted-line", "adopted-start", "x"}, -8.5}}, reader.revision());
    check(reader.document().objects.at("adopted-line").contours.front().points.front().x.literal == -8.5,
        "Cold process authored endpoint edit succeeds");
}

#ifdef NECT_ANALYSIS_LINE_DESKTOP_TESTS
using namespace nect::desktop;
struct Selection { QString analysis, line, session; std::uint64_t revision; };
Selection candidate(Host& host) {
    const Snapshot before(host.session);
    const auto analysis = host.analyze_regions("comp", "board", 2, 128, host.session.revision());
    const auto lines = analysis.value("line_candidates").toArray();
    check(lines.size() == 1, "Real Canvas analysis emits one thin line and excludes Artboard background");
    const auto line = lines.first().toObject();
    check(line.value("direction").toString() == "horizontal" && line.value("length_pixels").toInt() == 5 &&
        line.value("start").toArray() == QJsonArray{1,2} && line.value("end").toArray() == QJsonArray{5,2}, "Actual candidate exact endpoints");
    check(line.value("id").toString().startsWith("analysis.line."), "Stable line identity family");
    check(before.unchanged(host.session), "Actual analysis is read-only");
    return {analysis.value("analysis_id").toString(), line.value("id").toString(), host.session_id, host.session.revision()};
}
QJsonObject adopt(Host& host, const Selection& selection, double scale = 2, int threshold = 128,
    const Id& composition = "comp", const Id& artboard = "board") {
    return adopt_analysis_line(host, composition, artboard, scale, threshold, selection.analysis,
        selection.line, "Adopted thin line", selection.session, selection.revision);
}
template<class F> void refuse(Host& host, const char* code, F&& f) {
    const Snapshot before(host.session); rejects(code, std::forward<F>(f));
    check(before.unchanged(host.session), "Host refusal preserves exact Document/native/revision/History");
}
void desktop_primary(Host& host, const QString& executable, const QString& directory) {
    host.session = Session(fixture()); const Snapshot before(host.session); const auto selection = candidate(host);
    const auto result = adopt(host, selection); const auto id = result.value("object_id").toString().toStdString();
    const auto adopted = host.session.document(); const auto& object = adopted.objects.at(id);
    check(object.kind == Kind::path && !object.source && !object.contours.front().closed &&
        object.contours.front().points.size() == 2, "Host adopts normal open two-point Path");
    check(result.value("source_line_id").toString() == selection.line &&
        !result.value("appearance_preserved").toBool() && !result.value("curve_fitting").toBool(), "Receipt identifies exact selected run and bounded losses");
    check(host.session.revision() == before.revision + 1 &&
        host.session.history().states.size() == before.history.states.size() + 1, "Host adoption is one Undo entry");
    check(adopted.objects.at("source") == before.document.objects.at("source"), "Host preserves source artwork exactly");
    check(adopted.compositions.front().roots == std::vector<Id>{"source", id}, "Host inserts separate root above source");
    const auto& contour = object.contours.front();
    check(contour.id == result.value("contour_id").toString().toStdString(), "Receipt contains authored contour ID");
    for (int i = 0; i < 2; ++i) {
        const auto& point = contour.points.at(static_cast<std::size_t>(i));
        check(point.id == result.value("point_ids").toArray().at(i).toString().toStdString(), "Receipt contains authored endpoint IDs");
        check(point.x.literal == (i == 0 ? -9.25 : -7.25) && point.y.literal == 21.25 &&
            point.in_length.literal == 0 && point.out_length.literal == 0, "Host pixel centers map exactly");
    }
    host.session.undo(host.session.revision()); check(host.session.document() == before.document, "Host one Undo restores source-only document");
    host.session.redo(host.session.revision()); check(host.session.document() == adopted, "Host Redo preserves IDs");
    const auto path = directory + "/thin-line.nect"; host.save(path); host.flush();
    QProcess cold; cold.start(executable, {"--cold-desktop", path, QString::fromStdString(id),
        QString::fromStdString(contour.points.front().id)});
    check(cold.waitForFinished(30000) && cold.exitCode() == 0, "Fresh-process Host native reopen/edit passes");
    check(encode(host.session.document()) == encode(adopted), "Cold probe leaves live source/native unchanged");
}
void desktop_risks(Host& host) {
    host.session = Session(fixture()); const auto selection = candidate(host); auto bad = selection;
    bad.analysis = "analysis-forged"; refuse(host, "ANALYSIS_CONFLICT", [&] { adopt(host, bad); });
    bad = selection; bad.line = "analysis.line.forged"; refuse(host, "INVALID_REQUEST", [&] { adopt(host, bad); });
    bad = selection; bad.session = "old-session"; refuse(host, "SESSION_CONFLICT", [&] { adopt(host, bad); });
    bad = selection; bad.line.clear(); refuse(host, "INVALID_REQUEST", [&] { adopt(host, bad); });
    refuse(host, "ANALYSIS_CONFLICT", [&] { adopt(host, selection, 1); });
    refuse(host, "ANALYSIS_CONFLICT", [&] { adopt(host, selection, 2, 127); });
    refuse(host, "EXPORT_SCALE", [&] { adopt(host, selection, 0); });
    refuse(host, "INVALID_THRESHOLD", [&] { adopt(host, selection, 2, 0); });
    refuse(host, "MISSING_COMPOSITION", [&] { adopt(host, selection, 2, 128, "missing"); });
    refuse(host, "MISSING_ARTBOARD", [&] { adopt(host, selection, 2, 128, "comp", "missing"); });
    host.session.begin_gesture(host.session.revision());
    host.session.update_gesture({Set{{"source", "source-p0", "x"}, -9.25}});
    refuse(host, "GESTURE_ACTIVE", [&] { adopt(host, selection); });
    check(host.session.gesture_active(), "Refusal leaves another owner's gesture active"); host.session.cancel_gesture();
    host.session.apply({Rename{"source", "Concurrent edit"}}, host.session.revision());
    refuse(host, "REVISION_CONFLICT", [&] { adopt(host, selection); });
    bad = selection; bad.revision = host.session.revision();
    refuse(host, "ANALYSIS_CONFLICT", [&] { adopt(host, bad); });
}
void events() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }
void desktop_dialog(Host& host) {
    host.session = Session(fixture()); const Snapshot before(host.session); Id adopted;
    auto open = [&] { auto* dialog = make_analysis_line_dialog(host, "comp", "board", [&](const Id& id) { adopted = id; });
        dialog->show(); events(); return dialog; };
    auto analyze = [&](QDialog* dialog) { dialog->findChild<QDoubleSpinBox*>("analysis-line-scale")->setValue(2);
        dialog->findChild<QPushButton*>("analysis-line-analyze")->click(); events(); };
    auto* dialog = open(); analyze(dialog);
    auto* apply = dialog->findChild<QPushButton*>("analysis-line-adopt");
    check(apply->isEnabled() && dialog->findChild<QComboBox*>("analysis-line-choice")->count() == 1, "Dialog selects real returned candidate");
    dialog->findChild<QSpinBox*>("analysis-line-threshold")->setValue(127);
    check(!apply->isEnabled(), "Parameter edit invalidates old analysis");
    analyze(dialog); dialog->reject(); events(); check(before.unchanged(host.session) && adopted.empty(), "Dialog Cancel is exact and callback-free");
    dialog = open(); analyze(dialog); dialog->findChild<QPushButton*>("analysis-line-adopt")->click(); events();
    check(!adopted.empty() && host.session.document().objects.contains(adopted), "Dialog creates Path through shared adoption entry");
    host.session.undo(host.session.revision()); check(host.session.document() == before.document, "Dialog adoption has one Undo");
    dialog = open(); analyze(dialog); host.session.apply({Rename{"source", "External edit"}}, host.session.revision());
    const Snapshot concurrent(host.session); dialog->findChild<QPushButton*>("analysis-line-adopt")->click(); events();
    check(concurrent.unchanged(host.session) && dialog->isVisible(), "Stale dialog revision refuses atomically"); dialog->reject(); events();
    dialog = open(); analyze(dialog); host.create_document(); const Snapshot replacement(host.session);
    dialog->findChild<QPushButton*>("analysis-line-adopt")->click(); events();
    check(replacement.unchanged(host.session) && dialog->isVisible(), "Replaced Session refuses old dialog atomically"); dialog->reject(); events();
}
#endif
}

int main(int argc, char** argv) {
#ifdef NECT_ANALYSIS_LINE_DESKTOP_TESTS
    qputenv("QT_QPA_PLATFORM", "offscreen"); QApplication app(argc, argv);
#endif
    try {
        if (argc == 3 && std::string(argv[1]) == "--cold-core") cold_core(argv[2]);
#ifdef NECT_ANALYSIS_LINE_DESKTOP_TESTS
        else if (argc == 5 && std::string(argv[1]) == "--cold-desktop") {
            QTemporaryDir temporary; Host host(temporary.path() + "/cold"); host.open(QString::fromLocal8Bit(argv[2]));
            QFile file(QString::fromLocal8Bit(argv[2])); check(file.open(QIODevice::ReadOnly), "Read saved native file");
            check(host.session.document() == decode(file.readAll().toStdString()), "Cold Host exact native document");
            check(host.session.document().objects.at("source") == fixture().objects.at("source"), "Cold Host exact retained source");
            host.session.apply({Set{{argv[3], argv[4], "x"}, -8.5}}, host.session.revision());
            check(host.session.can_undo(), "Cold Host stable endpoint edit succeeds");
        }
#endif
        else {
            pure_geometry(); const auto adopted = pure_session();
            if (argc == 3 && std::string(argv[1]) == "--native-out") {
                std::ofstream file(argv[2], std::ios::binary); file << encode(adopted); check(file.good(), "Write core native cold-process fixture");
            }
#ifdef NECT_ANALYSIS_LINE_DESKTOP_TESTS
            QTemporaryDir temporary; Host host(temporary.path() + "/host");
            desktop_primary(host, app.applicationFilePath(), temporary.path()); desktop_risks(host); desktop_dialog(host);
#endif
        }
        std::cout << "PASS " << checks << " thin-line adoption checks\n"; return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL " << checks << ": " << error.what() << '\n'; return 1; }
}
