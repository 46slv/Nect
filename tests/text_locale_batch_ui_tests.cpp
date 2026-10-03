#include "nect/io.hpp"
#ifndef NECT_LOCALE_BATCH_CORE_ONLY
#include "host.hpp"
#include "text_locale_batch_control.hpp"
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWidget>
#endif
#include <iostream>
#include <stdexcept>

// Define NECT_LOCALE_BATCH_CORE_ONLY to run the source-preservation/core smoke
// against a source-matched nect_io / nect_core build when Qt is unavailable.
// That path does not execute or qualify the desktop controls below.
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
Document fixture(bool mixed=false,bool driven=false){
    auto document=empty_document("locale-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::text;
    source.text=default_text("source-text","Source 日本語");source.text->locale="ar-SA";
    Object first=source;first.id="first";first.name="Same name";
    first.text=default_text("first-text","First\n日本語");first.text->locale="ja-JP";
    first.text->family="Arial";first.text->layout="frame";first.text->direction="vertical";
    first.text->content_driver=TextContentDriver{{"source","","text.content"}};
    first.text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    first.text->direction_driver=TextDirectionDriver{{"source","","text.direction"}};
    first.text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    first.text->alignment_driver=TextAlignmentDriver{{"source","","text.alignment"}};
    first.text->weight_expression=Expression{"500",1};
    first.text->italic_driver=TextItalicDriver{Expression{"true",1}};
    first.text->parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};
    first.text->parameters.at("tracking").expression=Expression{"2",1};
    first.text->font_features={{"kern",1,"whole_text"},{"liga",0,"whole_text"}};
    first.text->additional_axis_values={{"wdth",87.1234567890123}};
    Object second=source;second.id="second";second.name="Same name";
    second.text=default_text("second-text","Second literal ");
    second.text->locale=mixed?"en-US":"ja-JP";second.text->weight=650;second.text->italic=true;
    second.text->parameters.at("font_size").literal=32;
    if(driven)second.text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    Object path;path.id="shape";path.name="Path";path.kind=Kind::path;
    Contour contour;contour.id="contour";Point begin;begin.id="begin";begin.x.literal=10;begin.y.literal=100;
    Point end;end.id="end";end.x.literal=2010;end.y.literal=100;contour.points={begin,end};path.contours={contour};
    second.text->path_attachment=TextPathAttachment{"shape","contour","distance",1000,0,false};
    document.objects.emplace(source.id,source);document.objects.emplace(first.id,first);
    document.objects.emplace(second.id,second);document.objects.emplace(path.id,path);
    document.compositions.front().roots={"source","first","second","shape"};return document;
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());
    }
};
Document localized(Document document,const std::string& value){
    for(const auto* id:{"first","second"})document.objects.at(id).text->locale=value;
    return document;
}
std::vector<Command> locale_commands(const Document& document,const std::string& value){
    std::vector<Command> commands;
    for(const auto* id:{"first","second"}){
        auto source=*document.objects.at(id).text;source.locale=value;
        commands.push_back(UpdateText{id,std::move(source)});
    }
    return commands;
}
void core_smoke(){
    Session session(fixture());const Snapshot before(session);
    session.apply(locale_commands(session.document()," EN_us "),session.revision());
    const auto expected=localized(before.document," EN_us ");
    check(session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "Core commits a locale-only batch once, preserving exact case, whitespace and all rich Text sources");
    check(decode(encode(session.document()))==expected,"Core native readback preserves the exact full locale batch");
    const auto committed=encode(session.document());session.undo(session.revision());
    check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
        "One core Undo restores both complete Text sources and all unrelated state");
    session.redo(session.revision());check(encode(session.document())==committed,"One core Redo restores the exact locale batch");
    for(const auto& value:std::vector<std::string>{"",std::string(129,'a'),std::string(1,'\x01'),std::string("\xc0\x80",2)}){
        const Snapshot invalid(session);bool rejected=false;
        try{session.apply(locale_commands(session.document(),value),session.revision());}
        catch(const Error& error){rejected=error.code=="LIMIT"||error.code=="INVALID_TEXT"||error.code=="INVALID_UTF8";}
        check(rejected&&invalid.unchanged(session),"Core invalid locale refuses the entire batch without partial authored/native/history mutation");
    }
    std::string multibyte;for(int i=0;i<43;++i)multibyte+="日";
    const Snapshot too_long(session);bool rejected=false;
    try{session.apply(locale_commands(session.document(),multibyte),session.revision());}
    catch(const Error& error){rejected=error.code=="LIMIT";}
    check(rejected&&too_long.unchanged(session),"Core locale limit counts UTF-8 bytes, not characters");
    const auto boundary=std::string(126,'a')+"é";
    session.apply(locale_commands(session.document(),boundary),session.revision());
    check(session.document().objects.at("first").text->locale==boundary&&boundary.size()==128,
        "Core accepts and preserves an exact 128-byte locale without adding a tag grammar");
    session=Session(fixture(false,true));const Snapshot linked(session);rejected=false;
    try{session.apply(locale_commands(session.document(),"fr-FR"),session.revision());}
    catch(const Error& error){rejected=error.code=="DRIVEN_PROPERTY";}
    check(rejected&&linked.unchanged(session),"A later linked locale prevents an earlier literal update atomically");
}
#ifndef NECT_LOCALE_BATCH_CORE_ONLY
using namespace nect::desktop;
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets={"first","second"};
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_text_locale_batch_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Locale control exists");return found;
    }
    QLineEdit* editor(){return control<QLineEdit>("text-locale-batch-editor");}
    QPushButton* apply(){return control<QPushButton>("text-locale-batch-apply");}
    QPushButton* cancel(){return control<QPushButton>("text-locale-batch-cancel");}
    QString state(){return control<QLabel>("text-locale-batch-state")->text();}
    QString status(){return control<QLabel>("text-locale-batch-status")->text();}
    void choose(const std::string& value){editor()->setText(QString::fromStdString(value));}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};

void primary_and_mixed(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    check(inspector.state()=="Shared: ja-JP"&&inspector.editor()->text()=="ja-JP",
        "Equal retained Text locales are displayed as an exact shared value");
    check(!inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),"Unchanged shared locale cannot create history");
    inspector.apply()->click();inspector.choose("en-US");
    check(before.unchanged(session)&&inspector.apply()->isEnabled()&&inspector.cancel()->isEnabled(),"Typing is a draft only");
    inspector.cancel()->click();check(before.unchanged(session)&&inspector.editor()->text()=="ja-JP"&&
        !inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),"Cancel discards the draft without authored, native, revision or History changes");
    inspector.choose("en-US");inspector.choose("ja-JP");
    check(!inspector.apply()->isEnabled()&&before.unchanged(session),"Returning to the shared value is a no-op");
    inspector.choose(" EN_us ");QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
    inspector.apply()->click();const auto expected=localized(before.document," EN_us ");
    check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "Apply uses one canonical batch and survives synchronous Inspector destruction");
    check(decode(encode(session.document()))==expected,
        "Only locale changes; other Text literals, links, expressions, features, axes, paths and unrelated objects survive");
    check(inspector.state()=="Shared:  EN_us "&&!inspector.apply()->isEnabled(),"No case folding, trimming or invented tag grammar alters the authored locale");
    const auto committed=encode(session.document());inspector.undo();
    check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),"One Undo restores both full Text sources and exact native state");
    inspector.redo();check(encode(session.document())==committed&&inspector.state()=="Shared:  EN_us ","One Redo restores the locale batch");
    const Snapshot noop(session);inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(noop.unchanged(session),"Even a forced no-op Apply creates no revision or History entry");

    inspector.load(fixture(true));const Snapshot mixed(session);
    check(inspector.state()=="Mixed"&&inspector.editor()->text().isEmpty()&&inspector.editor()->placeholderText()=="Mixed"&&
        !inspector.apply()->isEnabled(),"Mixed locales do not invent a shared literal");
    inspector.choose("fr-FR");inspector.cancel()->click();
    check(mixed.unchanged(session)&&inspector.editor()->text().isEmpty()&&inspector.state()=="Mixed"&&!inspector.apply()->isEnabled(),
        "Cancel restores the original Mixed draft and preserves both original locales");
    inspector.choose("en-US");check(mixed.unchanged(session),"Mixed selection stays authored until Apply");
    inspector.apply()->click();check(session.document()==localized(mixed.document,"en-US")&&session.revision()==1,
        "Apply sets every retained Text locale even when one already has that exact value");
    inspector.undo();check(session.document()==mixed.document&&inspector.state()=="Mixed","Undo restores per-target mixed locales");
    inspector.redo();check(inspector.state()=="Shared: en-US","Redo restores the shared locale");
    auto case_mixed=fixture();case_mixed.objects.at("first").text->locale="en-US";case_mixed.objects.at("second").text->locale="en-us";
    inspector.load(case_mixed);check(inspector.state()=="Mixed","Shared/Mixed comparison is case-sensitive");
}

void invalid_drafts(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    std::string multibyte;for(int i=0;i<43;++i)multibyte+="日";
    for(const auto& value:std::vector<std::string>{"",std::string(129,'a'),multibyte,std::string(1,'\x01')}){
        inspector.choose(value);check(inspector.apply()->isEnabled()&&before.unchanged(session),"An invalid draft never edits the document before Apply");
        inspector.apply()->click();check(before.unchanged(session)&&
            (inspector.status().startsWith("LIMIT")||inspector.status().startsWith("INVALID_TEXT")),
            "Core validation refuses empty, oversized UTF-8 or unsupported-control drafts atomically");
        inspector.cancel()->click();check(before.unchanged(session)&&inspector.editor()->text()=="ja-JP"&&inspector.status().isEmpty(),
            "Cancel after an invalid Apply restores the draft without retaining an error or changing history");
    }
    const auto boundary=std::string(126,'a')+"é";inspector.choose(boundary);inspector.apply()->click();
    check(session.document()==localized(before.document,boundary),"A 128-byte UTF-8 value is committed exactly through core rules");
}

void refusals_and_retained_targets(){
    Inspector inspector;auto& session=inspector.host.session;
    auto targets=std::vector<std::vector<Id>>{{},{"first"},{"first","shape"},{"first","first"},{"first","missing"}};
    targets.push_back(std::vector<Id>(1001,"first"));
    for(const auto& selection:targets){
        inspector.targets=selection;inspector.load();const Snapshot refused(session);
        check(inspector.state()=="Unavailable"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&!inspector.status().isEmpty(),
            "Invalid-size, mixed-kind, duplicate or missing selections refuse without filtering");
        inspector.apply()->setEnabled(true);inspector.apply()->click();
        check(refused.unchanged(session),"An unavailable selection cannot partially edit a literal target");
    }
    inspector.targets={"first","second"};inspector.load(fixture(false,true));const Snapshot linked(session);
    check(inspector.status().startsWith("DRIVEN_PROPERTY")&&inspector.status().contains("second")&&!inspector.apply()->isEnabled(),
        "One linked locale refuses the complete batch and identifies the target");
    inspector.choose("fr-FR");inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(linked.unchanged(session),"Batch controls never silently remove a locale link or modify an earlier literal target");

    inspector.load();inspector.choose("fr-FR");session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale(session);
    inspector.apply()->click();check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A stale revision refuses without any state mutation");
    inspector.load();inspector.choose("fr-FR");inspector.host.session_id+="-different";const Snapshot other_session(session);
    inspector.apply()->click();check(other_session.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"A replaced Host Session with the same IDs is refused");
    inspector.load();inspector.choose("fr-FR");auto replacement=fixture();replacement.id="other-document";
    session=Session(std::move(replacement));const Snapshot other_document(session);
    inspector.apply()->click();check(other_document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"A same-revision document replacement refuses independently of Session ID");
    inspector.load();inspector.choose("fr-FR");session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),"Active gesture and its preview survive a refused locale Apply");
    session.cancel_gesture();inspector.apply()->click();
    check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Cancelling a gesture invalidates the old locale edit context");
    inspector.load();session.begin_gesture(session.revision());inspector.rebuild();const Snapshot initial_gesture(session);
    check(inspector.state()=="Unavailable"&&inspector.status().startsWith("GESTURE_ACTIVE")&&initial_gesture.unchanged(session),
        "A panel opened during a gesture refuses immediately");session.cancel_gesture();

    inspector.load();const Snapshot retained(session);inspector.choose("fr-FR");inspector.targets={"source","first"};
    inspector.apply()->click();check(session.document()==localized(retained.document,"fr-FR"),"Later caller selection changes cannot retarget the exact captured Text IDs");
    inspector.targets={"first","second"};inspector.load();const Snapshot dismissed(session);inspector.choose("de-DE");
    delete inspector.controls.data();check(dismissed.unchanged(session),"Closing a drafted panel never applies it");inspector.rebuild();
    check(inspector.editor()->text()=="ja-JP"&&!inspector.apply()->isEnabled(),"Reopened panel has no retired draft");
    inspector.choose("fr-FR");inspector.host.changed={};inspector.apply()->click();const Snapshot once(session);
    inspector.apply()->click();check(once.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Repeated Apply on an unreconstructed panel cannot commit twice");
}
#endif
}

#ifdef NECT_LOCALE_BATCH_CORE_ONLY
int main(){
    try{core_smoke();std::cout<<"PASS "<<checks<<" source-bound locale core smoke checks (Qt UI NOT_RUN; full regression NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{core_smoke();primary_and_mixed();invalid_drafts();refusals_and_retained_targets();
        std::cout<<"PASS "<<checks<<" Text locale batch Qt/core checks (physical OS input NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#endif
