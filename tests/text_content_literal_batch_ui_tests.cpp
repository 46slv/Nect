#include "nect/io.hpp"
#ifndef NECT_CONTENT_LITERAL_BATCH_CORE_ONLY
#include "host.hpp"
#include "text_content_batch_control.hpp"
#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWidget>
#endif
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// The optional core-only path must link source-matched native 0.83 libraries.
// It never qualifies the Qt component or replaces its desktop regression run.
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
const std::string shared_content="  Literal 日本語\n\tCafé e\xcc\x81 🐈\n\n";
const std::string changed_content="\n  Replacement 日本語\n\tCafé e\xcc\x81 🐈  \n";
const std::vector<Id> pair={"first","second"};

Document fixture(bool mixed=false,bool driven=false){
    auto document=empty_document("literal-content-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::text;
    source.text=default_text("source-text","Unrelated source 日本語\n");
    Object first=source;first.id="first";first.name="Same name";
    first.text=default_text("first-text",shared_content);
    first.text->family="Arial";first.text->locale="en-US";first.text->layout="frame";
    first.text->direction="vertical";first.text->alignment="center";
    first.text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    first.text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    first.text->direction_driver=TextDirectionDriver{{"source","","text.direction"}};
    first.text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    first.text->alignment_driver=TextAlignmentDriver{{"source","","text.alignment"}};
    first.text->weight_expression=Expression{"500",1};
    first.text->italic_driver=TextItalicDriver{Expression{"true",1}};
    first.text->parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};
    first.text->parameters.at("tracking").expression=Expression{"2",1};
    first.text->parameters.at("frame_width").literal=271.25;
    first.text->parameters.at("frame_height").literal=136.5;
    first.text->font_features={{"kern",1,"whole_text"},{"liga",0,"whole_text"}};
    first.text->additional_axis_values={{"wdth",87.1234567890123}};
    Object second=source;second.id="second";second.name="Same name";
    second.text=default_text("second-text",mixed?"":shared_content);
    second.text->family="Consolas";second.text->locale="ja-JP";second.text->alignment="end";
    second.text->weight=650;second.text->weight_driver=TextWeightDriver{{"source","","text.weight"},20};
    second.text->italic=true;second.text->italic_driver=TextItalicDriver{Ref{"source","","text.italic"}};
    second.text->parameters.at("font_size").literal=32;
    second.text->parameters.at("frame_width").literal=481.75;
    second.text->parameters.at("frame_height").literal=223.5;
    second.text->font_features={{"kern",0,"whole_text"}};
    second.text->additional_axis_values={{"opsz",21.75}};
    if(driven)second.text->content_driver=TextContentDriver{{"source","","text.content"}};
    Object path;path.id="shape";path.name="Unrelated path";path.kind=Kind::path;
    Contour contour;contour.id="contour";Point begin;begin.id="begin";begin.x.literal=10;begin.y.literal=100;
    Point end;end.id="end";end.x.literal=2010;end.y.literal=100;contour.points={begin,end};path.contours={contour};
    Object follower=source;follower.id="follower";follower.name="Unrelated linked Text";
    follower.text=default_text("follower-text","Retained unrelated literal\r\n");
    follower.text->content_driver=TextContentDriver{{"source","","text.content"}};
    document.objects.emplace(source.id,source);document.objects.emplace(first.id,first);
    document.objects.emplace(second.id,second);document.objects.emplace(path.id,path);
    document.objects.emplace(follower.id,follower);
    document.compositions.front().roots={"source","first","second","shape","follower"};
    return document;
}
Document contentized(Document document,const std::string& value,const std::vector<Id>& targets=pair){
    for(const auto& id:targets)document.objects.at(id).text->content=value;
    return document;
}
Document path_fixture(){
    auto document=contentized(fixture(),"Retained path literal");
    document.objects.at("second").text->path_attachment=TextPathAttachment{"shape","contour","distance",1000,0,false};
    return document;
}
std::vector<Command> content_commands(const Document& document,const std::string& value){
    std::vector<Command> commands;
    for(const auto& id:pair){auto source=*document.objects.at(id).text;source.content=value;
        commands.push_back(UpdateText{id,std::move(source)});}
    return commands;
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;bool undo,redo;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())),undo(session.can_undo()),redo(session.can_redo()){}
    bool unchanged(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history()&&
            native==encode(session.document())&&undo==session.can_undo()&&redo==session.can_redo();
    }
};
void core_smoke(){
    check(std::string(native_version)=="0.83","Smoke is source-bound to native 0.83");
    Session session(fixture());const Snapshot before(session);
    session.apply(content_commands(session.document(),changed_content),session.revision());
    const auto expected=contentized(before.document,changed_content);
    check(session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "Core literal batch changes only content in one transaction, preserving every other source field and driver");
    check(decode(encode(session.document()))==expected,"Native roundtrip retains exact multiline UTF-8 content and rich Text sources");
    const auto committed=encode(session.document());session.undo(session.revision());
    check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
        "One core Undo restores both complete Text sources and unrelated state");
    session.redo(session.revision());check(encode(session.document())==committed,"One core Redo restores the exact literal batch");
    const Snapshot clear(session);session.apply(content_commands(session.document(),""),session.revision());
    check(session.document()==contentized(clear.document,"")&&session.revision()==clear.revision+1,
        "Empty literal content is legal and clears both targets atomically");
    session.undo(session.revision());check(session.document()==clear.document,"One Undo restores both texts after clearing");
    for(const auto& value:std::vector<std::string>{std::string(32769,'x'),std::string(1,'\x01'),std::string("\xc0\x80",2)}){
        const Snapshot invalid(session);bool rejected=false;
        try{session.apply(content_commands(session.document(),value),session.revision());}
        catch(const Error& error){rejected=error.code=="LIMIT"||error.code=="INVALID_TEXT"||error.code=="INVALID_UTF8";}
        check(rejected&&invalid.unchanged(session),"Invalid content rejects without changing authored/native/history state or the redo branch");
    }
    session=Session(fixture(false,true));const Snapshot driven(session);bool rejected=false;
    try{session.apply(content_commands(session.document(),changed_content),session.revision());}
    catch(const Error& error){rejected=error.code=="DRIVEN_PROPERTY";}
    check(rejected&&driven.unchanged(session),"A later driven content target atomically prevents the earlier literal update");
    session=Session(path_fixture());const Snapshot path(session);
    session.apply(content_commands(session.document(),"Updated path literal"),session.revision());
    check(session.document()==contentized(path.document,"Updated path literal"),
        "A single-line content batch preserves a retained target's Text-on-Path attachment");
    session.undo(session.revision());const Snapshot path_redo(session);rejected=false;
    try{session.apply(content_commands(session.document(),changed_content),session.revision());}
    catch(const Error& error){rejected=error.code=="TEXT_PATH_MULTILINE_UNSUPPORTED";}
    check(rejected&&path_redo.unchanged(session),
        "Existing Text-on-Path multiline refusal preserves all target sources, native bytes, History and redo atomically");
}

#ifndef NECT_CONTENT_LITERAL_BATCH_CORE_ONLY
using namespace nect::desktop;
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets=pair;
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_text_content_batch_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        check(controls!=nullptr,"Literal panel exists");auto* found=controls->findChild<T*>(QString::fromLatin1(name));
        check(found!=nullptr,"Literal content control exists");return found;
    }
    QPlainTextEdit* editor(){return control<QPlainTextEdit>("text-content-literal-editor");}
    QPushButton* apply(){return control<QPushButton>("text-content-literal-apply");}
    QPushButton* cancel(){return control<QPushButton>("text-content-literal-cancel");}
    QPushButton* clear(){return control<QPushButton>("text-content-literal-clear");}
    QString state(){return control<QLabel>("text-content-literal-state")->text();}
    QString status(){return control<QLabel>("text-content-literal-status")->text();}
    void choose(const std::string& value){editor()->setPlainText(QString::fromStdString(value));}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};
void shared_draft_and_history(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    check(inspector.controls->objectName()=="text-content-literal-panel"&&
        inspector.state()=="Shared content"&&inspector.editor()->toPlainText().toStdString()==shared_content,
        "Shared content uses the exact multiline UTF-8 draft in the named panel");
    check(!inspector.editor()->accessibleName().isEmpty()&&
        inspector.control<QLabel>("text-content-literal-state")->textFormat()==Qt::PlainText&&
        inspector.control<QLabel>("text-content-literal-status")->textFormat()==Qt::PlainText,
        "Literal editor is identified for accessibility and display labels cannot interpret content as markup");
    check(!inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),"Unchanged shared draft cannot create history");
    inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(before.unchanged(session),"Even a forced unchanged shared Apply is a no-op");
    inspector.choose(changed_content);
    check(before.unchanged(session)&&inspector.apply()->isEnabled()&&inspector.cancel()->isEnabled(),
        "Typing multiline content stages only a draft");
    inspector.cancel()->click();
    check(before.unchanged(session)&&inspector.editor()->toPlainText().toStdString()==shared_content&&
        !inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled()&&inspector.status().isEmpty(),
        "Cancel restores the exact shared draft without authored/native/revision/history mutation");
    inspector.choose(changed_content);inspector.choose(shared_content);
    check(!inspector.apply()->isEnabled()&&before.unchanged(session),"Returning to shared content is a no-op");
    inspector.choose(changed_content);QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
    inspector.apply()->click();const auto expected=contentized(before.document,changed_content);
    check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "Apply uses one canonical transaction and survives synchronous Inspector destruction");
    check(decode(encode(session.document()))==expected,
        "Content alone changes; source identity, all other literals, links, expressions, features, axes, paths and unrelated objects survive");
    check(inspector.state()=="Shared content"&&inspector.editor()->toPlainText().toStdString()==changed_content&&
        !inspector.apply()->isEnabled(),"Apply preserves tabs, whitespace, combining characters, emoji and trailing newlines exactly");
    const auto committed=encode(session.document());inspector.undo();
    check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
        "One Undo restores both full Text sources and exact native state");
    inspector.redo();check(encode(session.document())==committed,"One Redo restores the content batch byte-exactly");
}
void mixed_and_clear(){
    Inspector inspector;auto& session=inspector.host.session;inspector.load(fixture(true));const Snapshot mixed(session);
    check(inspector.state()=="Mixed"&&inspector.editor()->toPlainText().isEmpty()&&
        inspector.editor()->placeholderText()=="Mixed"&&!inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),
        "Mixed content has an untouched empty placeholder, never an invented shared literal");
    inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(mixed.unchanged(session),"Forced Apply on an untouched Mixed placeholder cannot clear content");
    inspector.choose(shared_content);check(inspector.apply()->isEnabled()&&mixed.unchanged(session),
        "Choosing one existing literal still stages a complete Mixed batch");
    inspector.cancel()->click();check(mixed.unchanged(session)&&inspector.state()=="Mixed"&&
        inspector.editor()->toPlainText().isEmpty()&&!inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),
        "Cancel restores the untouched Mixed placeholder and both distinct originals");
    inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(mixed.unchanged(session),"A cancelled Mixed draft cannot later apply an empty placeholder");
    inspector.clear()->click();check(mixed.unchanged(session)&&inspector.editor()->toPlainText().isEmpty()&&
        inspector.apply()->isEnabled()&&inspector.cancel()->isEnabled(),
        "Explicit Clear stages legal empty content even when Mixed already renders empty");
    inspector.cancel()->click();check(mixed.unchanged(session)&&!inspector.apply()->isEnabled(),
        "Cancel discards an explicit empty Mixed draft without History");
    inspector.clear()->click();inspector.apply()->click();
    check(session.document()==contentized(mixed.document,"")&&session.revision()==mixed.revision+1&&
        session.history().states.size()==mixed.history.states.size()+1,
        "Explicit empty Mixed Apply commits every retained target once, including an already-empty target");
    check(inspector.state()=="Shared content"&&inspector.editor()->toPlainText().isEmpty()&&!inspector.apply()->isEnabled(),
        "Shared empty content remains distinct from Mixed");
    const Snapshot empty(session);inspector.clear()->click();inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(empty.unchanged(session),"Clearing an already-shared empty selection cannot create a no-op transaction");
    inspector.undo();check(session.document()==mixed.document&&inspector.state()=="Mixed","One Undo restores per-target Mixed content");
    inspector.redo();check(session.document()==contentized(mixed.document,""),"One Redo restores the complete empty batch");
    inspector.load(fixture(true));const Snapshot literal(session);inspector.choose(shared_content);inspector.apply()->click();
    check(session.document()==contentized(literal.document,shared_content)&&session.revision()==literal.revision+1,
        "Mixed Apply fills all retained objects even when one already has the requested exact bytes");
}
void exact_authored_comparison(){
    Inspector inspector;auto& session=inspector.host.session;
    // QPlainTextEdit renders CRLF as paragraph breaks. Opening, cancelling and
    // forcing an unstaged Apply must still preserve the original authored bytes.
    auto crlf=contentized(fixture(),"Retained\r\n日本語\r\n");inspector.load(crlf);const Snapshot before(session);
    check(inspector.state()=="Shared content"&&!inspector.apply()->isEnabled(),"Equal CRLF literals are shared and initially untouched");
    inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(before.unchanged(session),"Editor paragraph rendering cannot rewrite untouched authored CRLF content");
    inspector.choose(changed_content);inspector.cancel()->click();inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(before.unchanged(session),"Cancel preserves original CRLF bytes rather than committing the rendered representation");
    auto newline_mixed=crlf;newline_mixed.objects.at("second").text->content="Retained\n日本語\n";
    inspector.load(newline_mixed);check(inspector.state()=="Mixed","Shared/Mixed compares authored line endings byte-exactly");
    auto unicode_mixed=fixture();unicode_mixed.objects.at("first").text->content="é";
    unicode_mixed.objects.at("second").text->content="e\xcc\x81";
    inspector.load(unicode_mixed);check(inspector.state()=="Mixed","Shared/Mixed does not normalize distinct Unicode spellings");
}
void invalid_drafts(){
    Inspector inspector;auto& session=inspector.host.session;
    session.apply({Rename{"source","Temporary"}},session.revision());session.undo(session.revision());inspector.rebuild();
    const Snapshot before(session);check(session.can_redo(),"Validation fixture retains a redo branch");
    std::string multibyte;for(int i=0;i<10923;++i)multibyte+="日";
    for(const auto& value:std::vector<std::string>{std::string(32769,'x'),multibyte,std::string(1,'\x01')}){
        inspector.choose(value);check(inspector.apply()->isEnabled()&&before.unchanged(session),"Invalid content remains a draft until Apply");
        inspector.apply()->click();check(before.unchanged(session)&&
            (inspector.status().startsWith("LIMIT")||inspector.status().startsWith("INVALID_TEXT")),
            "Core byte limits and unsupported-control validation reject the whole batch while retaining redo");
        inspector.cancel()->click();check(before.unchanged(session)&&inspector.editor()->toPlainText().toStdString()==shared_content&&
            inspector.status().isEmpty()&&!inspector.apply()->isEnabled(),"Cancel after refusal restores the original draft and clears only the error");
    }
}
void path_content_validation(){
    Inspector inspector;auto& session=inspector.host.session;inspector.load(path_fixture());const Snapshot before(session);
    inspector.choose(changed_content);inspector.apply()->click();
    check(before.unchanged(session)&&inspector.status().startsWith("TEXT_PATH_MULTILINE_UNSUPPORTED"),
        "A later Text-on-Path target refuses a multiline draft without partially changing the earlier free Text");
    inspector.cancel()->click();check(before.unchanged(session)&&inspector.status().isEmpty()&&
        inspector.editor()->toPlainText()=="Retained path literal"&&!inspector.apply()->isEnabled(),
        "Cancel after a path validation refusal restores the source draft without History");
    inspector.choose("Updated path literal");inspector.apply()->click();
    check(session.document()==contentized(before.document,"Updated path literal")&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "A valid single-line batch changes content alone and preserves the exact retained path attachment");
    inspector.undo();check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
        "One Undo restores both full sources and the exact Text-on-Path state");
}
void refusals_and_stale_contexts(){
    Inspector inspector;auto& session=inspector.host.session;
    for(const auto& selection:std::vector<std::vector<Id>>{{},{"first"},{"first","shape"},{"first","first"},{"first","missing"}}){
        inspector.targets=selection;inspector.load();const Snapshot refused(session);
        check(inspector.state()=="Unavailable"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&!inspector.status().isEmpty(),
            "Empty, single, mixed-kind, duplicate and missing selections refuse without filtering");
        inspector.clear()->click();inspector.apply()->setEnabled(true);inspector.apply()->click();
        check(refused.unchanged(session),"Unavailable selections cannot Clear or partially edit a valid target");
    }
    inspector.targets=pair;inspector.load(fixture(false,true));const Snapshot linked(session);
    check(inspector.state()=="Unavailable"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&
        inspector.status().startsWith("DRIVEN_PROPERTY")&&inspector.status().contains("second"),
        "A single linked content target refuses the complete batch and identifies that target");
    inspector.choose(changed_content);inspector.clear()->click();inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(linked.unchanged(session),"Literal controls preserve content drivers instead of unlinking or editing earlier targets");
    inspector.load();inspector.choose(changed_content);session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale(session);
    inspector.apply()->click();check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),
        "Stale revision refuses without authored/native/history mutation");
    inspector.load();inspector.choose(changed_content);inspector.host.session_id+="-different";const Snapshot other_session(session);
    inspector.apply()->click();check(other_session.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),
        "Another Host Session with identical target IDs is refused");
    inspector.load();inspector.choose(changed_content);auto replacement=fixture();replacement.id="different-document";
    session=Session(std::move(replacement));const Snapshot other_document(session);
    inspector.apply()->click();check(other_document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),
        "Same-revision Document replacement is refused independently of Host Session ID");
    inspector.load();inspector.choose(changed_content);session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),
        "Active gesture and preview survive a refused literal Apply");
    session.cancel_gesture();inspector.apply()->click();check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),
        "Cancelling a gesture invalidates the captured edit context even without a revision change");
    inspector.load();session.begin_gesture(session.revision());inspector.rebuild();const Snapshot opened_gesture(session);
    check(inspector.state()=="Unavailable"&&inspector.status().startsWith("GESTURE_ACTIVE")&&opened_gesture.unchanged(session),
        "A panel opened during a gesture refuses immediately");session.cancel_gesture();
}
Document many_texts(std::size_t count,std::vector<Id>& ids){
    auto document=empty_document("many-text-document","composition","artboard");ids.clear();
    for(std::size_t i=0;i<count;++i){const auto id="text-"+std::to_string(i);Object object;
        object.id=id;object.name="Same name";object.kind=Kind::text;object.text=default_text(id+"-source","");
        document.objects.emplace(id,std::move(object));document.compositions.front().roots.push_back(id);ids.push_back(id);}
    return document;
}
void selection_bounds_and_retention(){
    Inspector inspector;auto& session=inspector.host.session;std::vector<Id> ids;
    auto thousand=many_texts(1000,ids);inspector.targets=ids;inspector.load(thousand);const Snapshot before(session);
    check(inspector.state()=="Shared content"&&inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled(),
        "Exactly 1000 distinct present Text targets are admitted");
    const std::string bulk="Bulk\n日本語 🐈\n";inspector.choose(bulk);inspector.apply()->click();
    check(session.document()==contentized(before.document,bulk,ids)&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,"1000 retained targets commit as one complete content-only transaction");
    inspector.undo();check(session.document()==before.document&&!session.can_undo(),"One Undo restores all 1000 complete Text sources");
    auto too_many=many_texts(1001,ids);inspector.targets=ids;inspector.load(too_many);const Snapshot refused(session);
    check(inspector.state()=="Unavailable"&&inspector.status().startsWith("INVALID_SELECTION")&&!inspector.editor()->isEnabled(),
        "1001 distinct present Text targets exceed the exact selection bound");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(refused.unchanged(session),"Oversized selection cannot create partial History");
    inspector.targets=pair;inspector.load();const Snapshot retained(session);inspector.choose(changed_content);inspector.targets={"source","first"};
    inspector.apply()->click();check(session.document()==contentized(retained.document,changed_content),
        "Caller selection changes cannot retarget the captured stable Text IDs");
    inspector.targets=pair;inspector.load();const Snapshot dismissed(session);inspector.choose(changed_content);
    delete inspector.controls.data();check(dismissed.unchanged(session),"Closing a drafted panel never applies its content");inspector.rebuild();
    check(inspector.editor()->toPlainText().toStdString()==shared_content&&!inspector.apply()->isEnabled(),"Reopening does not resurrect a retired draft");
    inspector.choose(changed_content);inspector.host.changed={};inspector.apply()->click();const Snapshot once(session);
    inspector.apply()->click();check(once.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),
        "Repeated Apply on an unreconstructed panel cannot commit twice");
}
#endif
}

#ifdef NECT_CONTENT_LITERAL_BATCH_CORE_ONLY
int main(){try{core_smoke();std::cout<<"PASS "<<checks<<" source-bound literal content core smoke checks (Qt UI NOT_RUN; full regression NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}}
#else
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{core_smoke();shared_draft_and_history();mixed_and_clear();exact_authored_comparison();invalid_drafts();
        path_content_validation();refusals_and_stale_contexts();selection_bounds_and_retention();
        std::cout<<"PASS "<<checks<<" Text literal content batch Qt/core checks (physical OS input NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}}
#endif
