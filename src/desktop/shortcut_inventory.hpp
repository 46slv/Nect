#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <Qt>

class QAction;

namespace nect::desktop {

struct ShortcutInventoryRow {
    QString action;
    QStringList shortcuts;
    Qt::ShortcutContext context=Qt::WindowShortcut;
};

// Read-only presentation of the supplied live actions. One row per QAction,
// with all non-empty alternatives in QAction order and native key notation.
// Repeated pointers are ignored; distinct actions with the same keys remain.
// Rows are sorted by label, portable keys, then context, independent of input order.
QList<ShortcutInventoryRow> shortcut_inventory(const QList<QAction*>& actions);

}
