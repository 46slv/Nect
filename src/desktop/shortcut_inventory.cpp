#include "shortcut_inventory.hpp"
#include <QAction>
#include <QKeySequence>
#include <QSet>
#include <algorithm>
#include <utility>

namespace nect::desktop {
namespace {
QString without_mnemonics(const QString& text) {
    QString result;
    result.reserve(text.size());
    for(qsizetype i=0;i<text.size();++i) {
        if(text.at(i)!=QLatin1Char('&'))result+=text.at(i);
        else if(i+1<text.size()&&text.at(i+1)==QLatin1Char('&')) {
            result+=QLatin1Char('&');
            ++i;
        }
    }
    return result.trimmed();
}

QString readable_label(const QAction& action) {
    auto label=without_mnemonics(action.text());
    if(label.isEmpty())label=without_mnemonics(action.iconText());
    if(label.isEmpty())label=action.objectName().trimmed();
    return label.isEmpty()?QStringLiteral("Unnamed action"):label;
}

struct Candidate {
    ShortcutInventoryRow row;
    QStringList portable_keys;
};
}

QList<ShortcutInventoryRow> shortcut_inventory(const QList<QAction*>& actions) {
    QSet<QAction*> seen;
    QList<Candidate> candidates;
    for(auto* action:actions) {
        if(!action||seen.contains(action))continue;
        seen.insert(action);
        Candidate candidate;
        candidate.row.action=readable_label(*action);
        candidate.row.context=action->shortcutContext();
        for(const auto& shortcut:action->shortcuts()) {
            if(shortcut.isEmpty())continue;
            candidate.row.shortcuts.push_back(shortcut.toString(QKeySequence::NativeText));
            candidate.portable_keys.push_back(shortcut.toString(QKeySequence::PortableText));
        }
        if(!candidate.row.shortcuts.isEmpty())candidates.push_back(std::move(candidate));
    }
    std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b) {
        if(a.row.action!=b.row.action)return a.row.action<b.row.action;
        if(a.portable_keys!=b.portable_keys)
            return std::lexicographical_compare(a.portable_keys.begin(),a.portable_keys.end(),
                b.portable_keys.begin(),b.portable_keys.end());
        return static_cast<int>(a.row.context)<static_cast<int>(b.row.context);
    });
    QList<ShortcutInventoryRow> rows;
    rows.reserve(candidates.size());
    for(auto& candidate:candidates)rows.push_back(std::move(candidate.row));
    return rows;
}

}
