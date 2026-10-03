#pragma once
#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <memory>
#include <stdexcept>

// Bounded production Window Qt/API contract; physical OS input is not exercised.
namespace multi_text_content_unlink_window_smoke {
using namespace nect;
using namespace nect::desktop;
inline int checks=0;
inline void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);++checks;}
inline void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
inline const std::vector<Canvas::Selection> targets={{"second",""},{"literal",""},{"first",""}};
inline const std::string first_value=" \r\nA\xc2\xa0" "B\xe2\x80\xa8" "C\r\n\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xf0\x9f\x90\x88\n ";
inline const std::string second_value="\nSecond\xe2\x80\xa9" "source\r\n\r\n";
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{return document==session.document()&&revision==session.revision()&&
        history==session.history()&&native==encode(session.document());}
};
inline Document fixture(){
    Session session(empty_document("unlink-document","composition","artboard"));
    auto first=default_text("first-text","First stored\r\nliteral");first.weight=650;first.italic=true;
    first.locale="en-US";first.parameters.at("font_size").literal=31.25;
    auto second=default_text("second-text","Second stored\n  literal  ");second.alignment="end";
    session.apply({CreateText{"composition","","first","First",first},
        CreateText{"composition","","second","Second",second},
        CreateText{"composition","","literal","Literal",default_text("literal-text","Keep\r\n\xc2\xa0" "literal  ")},
        CreateText{"composition","","source-first","First source",default_text("source-first-text",first_value)},
        CreateText{"composition","","source-second","Second source",default_text("source-second-text",second_value)},
        LinkTextContent{{"first","","text.content"},{"source-first","","text.content"}},
        LinkTextContent{{"second","","text.content"},{"source-second","","text.content"}}},0);
    return session.document();
}
inline QPushButton* unlink(Window& window){
    for(auto* button:window.findChildren<QPushButton*>("text-content-batch-unlink"))
        if(button->isVisible())return button;
    throw std::runtime_error("Visible production Text content batch Unlink is missing");
}
inline void history_action(Window& window,const char* label){
    for(auto* action:window.findChildren<QAction*>())if(action->text()==QString::fromLatin1(label)){
        require(action->isEnabled(),"Production history action is enabled");action->trigger();events();return;
    }
    throw std::runtime_error("Production history action is missing");
}
inline void load(Window& window){
    window.host.session=Session(fixture());window.host.session_id+="-unlink";window.host.edited();
    window.canvas->set_active_artboard("composition","artboard",false);window.canvas->set_selections(targets);events();
}
inline void primary(Window& window){
    load(window);auto& session=window.host.session;const Snapshot before(session);
    require(window.canvas->selections()==targets,"Fixture selects two driven Texts and one literal in explicit order");
    auto* button=unlink(window);
    require(button->text()=="Unlink sources (freeze each value)"&&button->isEnabled(),
        "Production Unlink explicitly offers per-target freeze for mixed driven/literal Texts");
    require(evaluate_text_content(before.document,"first")==first_value&&evaluate_text_content(before.document,"second")==second_value,
        "Distinct source Refs evaluate exact Unicode, NBSP, CRLF, separators and leading/trailing paragraphs");
    button->click();events();
    auto expected=before.document;
    expected.objects.at("first").text->content=first_value;expected.objects.at("first").text->content_driver.reset();
    expected.objects.at("second").text->content=second_value;expected.objects.at("second").text->content_driver.reset();
    require(session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "One production click unlinks only driven targets atomically and preserves every other authored field");
    require(session.document().objects.at("literal")==before.document.objects.at("literal")&&
        session.document().objects.at("source-first")==before.document.objects.at("source-first")&&
        session.document().objects.at("source-second")==before.document.objects.at("source-second"),
        "Literal target and both source objects are unchanged");
    require(window.canvas->selections()==targets,"Unlink and Inspector rebuild preserve the exact mixed selection");
    require(!unlink(window)->isEnabled(),"All-literal selection disables production Unlink after freeze");
    const Snapshot frozen(session);unlink(window)->click();events();
    require(frozen.unchanged(session),"Disabled all-literal Unlink produces no document or History change");
    history_action(window,"Undo");
    require(session.document()==before.document&&!session.can_undo()&&window.canvas->selections()==targets,
        "One production Undo restores both distinct drivers, stored literals and selection exactly");
    require(unlink(window)->isEnabled(),"Undo re-enables Unlink for the restored driven selection");
    history_action(window,"Redo");
    require(session.document()==expected&&!session.can_redo()&&window.canvas->selections()==targets,
        "One production Redo restores both frozen values and exact selection");
    auto source_first=*session.document().objects.at("source-first").text;source_first.content="Later first source";
    auto source_second=*session.document().objects.at("source-second").text;source_second.content="Later second source";
    session.apply({UpdateText{"source-first",source_first},UpdateText{"source-second",source_second}},session.revision());window.host.edited();events();
    require(evaluate_text_content(session.document(),"first")==first_value&&evaluate_text_content(session.document(),"second")==second_value&&
        session.document().objects.at("literal")==before.document.objects.at("literal"),
        "Each frozen target stays byte-exact after its distinct former source changes");
}
inline void retained_guards(Window& window){
    load(window);auto& session=window.host.session;auto* button=unlink(window);
    // Deliberately retain the visible Inspector callback without notifying Host.
    session.apply({Rename{"source-first","Changed while Inspector was open"}},session.revision());
    const Snapshot revised(session);button->click();events();
    require(revised.unchanged(session)&&window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT")&&
        window.canvas->selections()==targets,"Retained revision-stale Unlink refuses every target without mutation");
    window.host.edited();events();button=unlink(window);const auto identity=window.host.session_id;
    window.host.session_id+="-other-session";const Snapshot swapped(session);button->click();events();
    require(swapped.unchanged(session)&&window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT"),
        "Retained Session-stale Unlink refuses matching target IDs without mutation");
    window.host.session_id=identity;window.host.edited();events();button=unlink(window);
    session.begin_gesture(session.revision());session.update_gesture({Rename{"source-second","Preview only"}});
    const Snapshot gesturing(session);const auto preview=session.preview_document();button->click();events();
    require(gesturing.unchanged(session)&&session.gesture_active()&&session.preview_document()==preview&&
        window.statusBar()->currentMessage().startsWith("GESTURE_ACTIVE"),
        "Unlink refuses an active gesture without canceling its preview or committing targets");
    session.cancel_gesture();const Snapshot cancelled(session);button->click();events();
    require(cancelled.unchanged(session)&&window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT")&&
        window.canvas->selections()==targets,
        "Retained Unlink refuses a changed gesture generation even at the same revision");
    window.host.edited();events();
}
inline int run(){
    checks=0;QTemporaryDir files;require(files.isValid(),"Owned Window recovery directory exists");
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1400,900);window.show();events();
    primary(window);retained_guards(window);window.host.changed={};window.hide();return checks;
}
}
