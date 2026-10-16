#pragma once
#include <QIcon>
class QApplication;
namespace nect::studio {
void apply_theme(QApplication &);
QIcon icon(const QString &name);
} // namespace nect::studio
