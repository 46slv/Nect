#pragma once
#include "nect/core.hpp"
#include <QJsonObject>
#include <QString>
#include <functional>
class QDialog;
class QWidget;
namespace nect::desktop {
class Host;
// GUI/API/MCP adoption entry. Re-renders the exact committed analysis snapshot;
// callers supply stable analysis/line identities, never endpoints or an index.
QJsonObject adopt_analysis_line(Host&, const Id& composition, const Id& artboard,
    double scale, int threshold, const QString& analysis_id, const QString& line_id,
    const std::string& name, const QString& expected_session, std::uint64_t expected_revision);
// Draft-only chooser; Cancel has no authored or History effects.
QDialog* make_analysis_line_dialog(Host&, const Id& composition, const Id& artboard,
    std::function<void(const Id&)> adopted, QWidget* parent = nullptr);
}
