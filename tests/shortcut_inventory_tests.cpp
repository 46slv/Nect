#include "shortcut_inventory.hpp"
#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QPair>
#include <iostream>
#include <stdexcept>

using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
QString native(const char* key){return QKeySequence(QString::fromLatin1(key)).toString(QKeySequence::NativeText);}

void empty_shortcuts() {
    QAction none("No shortcut",nullptr),empty("Empty alternatives",nullptr);
    empty.setShortcuts({QKeySequence(),QKeySequence()});
    check(shortcut_inventory({}).isEmpty(),"Empty input yields no rows");
    check(shortcut_inventory({nullptr,&none,&empty,&none}).isEmpty(),
        "Null actions and actions with only empty shortcuts yield no rows");
}

void alternatives_and_read_only() {
    QAction draw("&Draw Path",nullptr);
    draw.setShortcuts({QKeySequence("P"),QKeySequence(),QKeySequence("G"),QKeySequence("Ctrl+Shift+T")});
    draw.setShortcutContext(Qt::WidgetShortcut);
    draw.setEnabled(false);
    draw.setVisible(false);
    const auto before=draw.shortcuts();
    const auto rows=shortcut_inventory({&draw,&draw});
    check(rows.size()==1,"One QAction pointer yields one row even when supplied twice");
    check(rows.front().action=="Draw Path"&&rows.front().shortcuts==QStringList{native("P"),native("G"),native("Ctrl+Shift+T")},
        "Every non-empty alternative is exposed in QAction order using native key notation");
    check(rows.front().context==Qt::WidgetShortcut,"The row reports the actual shortcut context");
    check(draw.shortcuts()==before&&draw.text()=="&Draw Path"&&draw.shortcutContext()==Qt::WidgetShortcut&&
        !draw.isEnabled()&&!draw.isVisible(),"Inventory neither changes keys nor filters or mutates disabled/hidden actions");
    draw.setShortcut(QKeySequence("H"));
    draw.setText("Changed label");
    draw.setShortcutContext(Qt::ApplicationShortcut);
    const auto changed=shortcut_inventory({&draw});
    check(changed.size()==1&&changed.front().action=="Changed label"&&changed.front().shortcuts==QStringList{native("H")}&&
        changed.front().context==Qt::ApplicationShortcut,"A fresh inventory reads live values without a second assignment registry");
}

void colliding_actions_and_sort() {
    QAction zulu("&Zulu",nullptr),alpha("&Alpha",nullptr),same("&Alpha",nullptr);
    zulu.setShortcut(QKeySequence("P"));alpha.setShortcut(QKeySequence("P"));same.setShortcut(QKeySequence("P"));
    same.setShortcutContext(Qt::WidgetShortcut);
    const auto rows=shortcut_inventory({&zulu,&alpha,&same,&alpha,nullptr,&zulu});
    check(rows.size()==3,"Different QAction pointers retain colliding keys, including equal labels");
    check(rows.at(0).action=="Alpha"&&rows.at(0).context==Qt::WidgetShortcut&&
        rows.at(1).action=="Alpha"&&rows.at(1).context==Qt::WindowShortcut&&rows.at(2).action=="Zulu",
        "Rows sort by readable label and use context to break otherwise equal rows");
    const auto reversed=shortcut_inventory({&same,&alpha,&zulu});
    for(qsizetype i=0;i<rows.size();++i)
        check(rows.at(i).action==reversed.at(i).action&&rows.at(i).shortcuts==reversed.at(i).shortcuts&&
            rows.at(i).context==reversed.at(i).context,"Visible row ordering does not depend on pointer/input order");
    QAction key_z("Same label",nullptr),key_a("Same label",nullptr);
    key_z.setShortcut(QKeySequence("Z"));key_a.setShortcut(QKeySequence("A"));
    const auto keys=shortcut_inventory({&key_z,&key_a});
    check(keys.size()==2&&keys.at(0).shortcuts==QStringList{native("A")}&&keys.at(1).shortcuts==QStringList{native("Z")},
        "Portable shortcut notation deterministically breaks equal-label ties");
}

void contexts() {
    const QList<Qt::ShortcutContext> contexts={Qt::WidgetShortcut,Qt::WidgetWithChildrenShortcut,
        Qt::WindowShortcut,Qt::ApplicationShortcut};
    for(const auto context:contexts) {
        QAction action("Context",nullptr);action.setShortcut(QKeySequence("K"));action.setShortcutContext(context);
        const auto rows=shortcut_inventory({&action});
        check(rows.size()==1&&rows.front().context==context,"Every Qt shortcut context is retained exactly");
    }
}

void label_escaping() {
    QAction action(nullptr);action.setShortcut(QKeySequence("K"));
    const QList<QPair<QString,QString>> labels={
        {"&Open && Save","Open & Save"},
        {"A&&&B&& C&","A&B& C"},
        {"&&&&","&&"},
        {QString::fromUtf8("  &日本語 && café  "),QString::fromUtf8("日本語 & café")}
    };
    for(const auto& label:labels) {
        action.setText(label.first);
        check(shortcut_inventory({&action}).front().action==label.second,
            "Mnemonic ampersands are removed while escaped pairs and Unicode remain readable");
    }
    action.setText("");action.setIconText("&Icon && label");
    check(shortcut_inventory({&action}).front().action=="Icon & label","An icon-only action uses readable icon text");
    action.setIconText("");action.setObjectName("object&name");
    check(shortcut_inventory({&action}).front().action=="object&name","An unnamed text action falls back to its literal object name");
    action.setObjectName("");
    check(shortcut_inventory({&action}).front().action=="Unnamed action","A shortcut without any action label is still readable");
}
}

int main(int argc,char** argv) {
    QApplication application(argc,argv);
    try {
        empty_shortcuts();alternatives_and_read_only();colliding_actions_and_sort();contexts();label_escaping();
        std::cout<<"PASS "<<checks<<" shortcut inventory checks\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return 1;
    }
}
