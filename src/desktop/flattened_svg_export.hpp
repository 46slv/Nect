#pragma once
#include "nect/core.hpp"
#include <QJsonObject>
#include <QString>
class QWidget;
namespace nect::desktop {
class Host;
// Explicit appearance-only derivative. This never substitutes for vector SVG.
QJsonObject export_flattened_svg(Host&,const QString& path,const Id& composition,
    const Id& artboard,double scale,std::uint64_t expected_revision);
void show_flattened_svg_export_dialog(Host&,const Id& composition,const Id& artboard,QWidget* parent=nullptr);
}
