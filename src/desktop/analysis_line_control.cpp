#include "analysis_line_control.hpp"
#include "host.hpp"
#include "nect/analysis_adoption.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>

namespace nect::desktop {
QJsonObject adopt_analysis_line(Host& host, const Id& composition, const Id& artboard,
    double scale, int threshold, const QString& analysis_id, const QString& line_id,
    const std::string& name, const QString& expected_session, std::uint64_t expected_revision) {
    if (expected_session != host.session_id)
        throw Error("SESSION_CONFLICT", "Document changed; analyze the current document again");
    if (analysis_id.isEmpty() || line_id.isEmpty())
        throw Error("INVALID_REQUEST", "Analysis and thin-line IDs required");
    // analyze_regions owns revision/gesture/parameter/target validation. It also
    // authenticates output bytes from linked sources that may change at a fixed
    // revision; no client-provided geometry can enter the authored Document.
    const auto analysis = host.analyze_regions(composition, artboard, scale, threshold, expected_revision);
    if (analysis.value("analysis_id").toString() != analysis_id)
        throw Error("ANALYSIS_CONFLICT", "Analysis snapshot changed; analyze again before adopting");
    QJsonObject selected;
    for (const auto& candidate : analysis.value("line_candidates").toArray()) {
        const auto line = candidate.toObject();
        if (line.value("id").toString() == line_id) { selected = line; break; }
    }
    if (selected.isEmpty())
        throw Error("INVALID_REQUEST", "Thin line does not belong to this analysis snapshot");
    const auto comp = std::find_if(host.session.document().compositions.begin(),
        host.session.document().compositions.end(), [&](const auto& value) { return value.id == composition; });
    const auto board = evaluate_artboard(*comp, artboard);
    const auto start = selected.value("start").toArray(), end = selected.value("end").toArray();
    const AnalysisLineCandidate line{selected.value("direction").toString().toStdString(),
        {start.at(0).toDouble(), start.at(1).toDouble()},
        {end.at(0).toDouble(), end.at(1).toDouble()}, selected.value("length_pixels").toInt()};
    const auto object_id = new_id(), contour_id = new_id();
    const std::vector<Id> point_ids{new_id(), new_id()};
    auto contour = adopt_analysis_thin_line(line,
        {{board.x, board.y}, scale, analysis.value("width").toInt(), analysis.value("height").toInt()},
        contour_id, point_ids);
    // Root insertion preserves Composition coordinates and every source object.
    // The one canonical command supplies normal Path defaults and atomic History.
    host.session.apply({CreatePath{composition, {}, object_id, name, {std::move(contour)}}}, expected_revision);
    QJsonArray authored_point_ids;
    for (const auto& id : point_ids) authored_point_ids.append(QString::fromStdString(id));
    QJsonObject result{{"object_id", QString::fromStdString(object_id)},
        {"contour_id", QString::fromStdString(contour_id)}, {"point_ids", authored_point_ids},
        {"source_analysis_id", analysis_id}, {"source_line_id", line_id},
        {"source_revision", static_cast<qint64>(expected_revision)},
        {"revision", static_cast<qint64>(host.session.revision())},
        {"coordinate_space", "composition"}, {"geometry", "thin-line-two-point-open-path"},
        {"appearance_preserved", false}, {"curve_fitting", false}};
    host.edited();
    return result;
}

QDialog* make_analysis_line_dialog(Host& host, const Id& composition, const Id& artboard,
    std::function<void(const Id&)> adopted, QWidget* parent) {
    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("analysis-line-dialog");
    dialog->setWindowTitle("Thin line to Path");
    dialog->resize(480, 330);
    auto* layout = new QVBoxLayout(dialog);
    auto* explanation = new QLabel("Analyze the active Artboard's artwork, then choose an exact thin pixel run. "
        "Creates a separate editable open Path with two pixel-center endpoints. Source artwork is kept. "
        "No curve fitting or thickness/appearance guarantee is applied.", dialog);
    explanation->setWordWrap(true); layout->addWidget(explanation);
    auto* form = new QFormLayout;
    auto* scale = new QDoubleSpinBox(dialog); scale->setObjectName("analysis-line-scale");
    scale->setRange(0.001, 16); scale->setDecimals(3); scale->setValue(1); scale->setSuffix(" px / unit");
    form->addRow("Resolution", scale);
    auto* threshold = new QSpinBox(dialog); threshold->setObjectName("analysis-line-threshold");
    threshold->setRange(1, 255); threshold->setValue(128); form->addRow("Alpha threshold", threshold);
    auto* analyze = new QPushButton("Analyze artwork", dialog); analyze->setObjectName("analysis-line-analyze");
    form->addRow(analyze);
    auto* choices = new QComboBox(dialog); choices->setObjectName("analysis-line-choice");
    form->addRow("Thin line", choices);
    auto* name = new QLineEdit("Adopted thin line", dialog); name->setObjectName("analysis-line-name");
    form->addRow("Path name", name); layout->addLayout(form);
    auto* status = new QLabel("Analysis is read-only. Cancel leaves artwork and history unchanged.", dialog);
    status->setObjectName("analysis-line-status"); status->setWordWrap(true); layout->addWidget(status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, dialog);
    auto* apply = buttons->addButton("Create Path", QDialogButtonBox::AcceptRole);
    apply->setObjectName("analysis-line-adopt"); apply->setEnabled(false); layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    struct Draft { QString analysis_id; };
    auto draft = std::make_shared<Draft>();
    const auto identity = host.session_id;
    const auto revision = host.session.revision();
    auto invalidate = [draft, choices, apply, status] {
        draft->analysis_id.clear(); choices->clear(); apply->setEnabled(false);
        status->setText("Settings changed. Analyze artwork to refresh the lines.");
    };
    QObject::connect(scale, &QDoubleSpinBox::valueChanged, dialog, invalidate);
    QObject::connect(threshold, &QSpinBox::valueChanged, dialog, invalidate);
    QObject::connect(analyze, &QPushButton::clicked, dialog,
        [&, composition, artboard, identity, revision, draft, scale, threshold, choices, apply, status] {
        draft->analysis_id.clear(); choices->clear(); apply->setEnabled(false);
        try {
            if (host.session_id != identity)
                throw Error("SESSION_CONFLICT", "Document changed; close and reopen this dialog");
            const auto result = host.analyze_regions(composition, artboard, scale->value(), threshold->value(), revision);
            draft->analysis_id = result.value("analysis_id").toString();
            for (const auto& value : result.value("line_candidates").toArray()) {
                const auto line = value.toObject();
                const auto start = line.value("start").toArray(), end = line.value("end").toArray();
                choices->addItem(QString("%1: (%2,%3) to (%4,%5), %6 pixels")
                    .arg(line.value("direction").toString()).arg(start.at(0).toInt()).arg(start.at(1).toInt())
                    .arg(end.at(0).toInt()).arg(end.at(1).toInt()).arg(line.value("length_pixels").toInt()),
                    line.value("id").toString());
            }
            apply->setEnabled(choices->count() > 0);
            status->setText(choices->count() ? QString("%1 exact thin lines. Create Path adds one Undo step.").arg(choices->count()) :
                "No exact one-pixel-wide runs of at least three pixels at this threshold.");
        } catch (const std::exception& error) { status->setText(QString::fromUtf8(error.what())); }
    });
    QObject::connect(apply, &QPushButton::clicked, dialog,
        [&, composition, artboard, identity, revision, draft, scale, threshold, choices, name, apply, status, dialog, adopted] {
        apply->setEnabled(false);
        try {
            const auto result = adopt_analysis_line(host, composition, artboard, scale->value(), threshold->value(),
                draft->analysis_id, choices->currentData().toString(), name->text().toStdString(), identity, revision);
            const auto object = result.value("object_id").toString().toStdString();
            dialog->accept();
            if (adopted) adopted(object);
        } catch (const std::exception& error) { status->setText(QString::fromUtf8(error.what())); }
    });
    return dialog;
}
}
