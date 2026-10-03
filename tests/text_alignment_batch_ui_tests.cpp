#include "host.hpp"
#include "text_alignment_batch_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWidget>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
Document fixture(bool mixed=false,bool driven=false){
    auto document=empty_document("alignment-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::text;
    source.text=default_text("source-text","Source 日本語");source.text->alignment="end";
    Object first=source;first.id="first";first.name="Same name";
    first.text=default_text("first-text","First\n日本語");first.text->alignment="start";
    first.text->family="Arial";first.text->locale="en-US";first.text->layout="frame";first.text->direction="vertical";
    first.text->content_driver=TextContentDriver{{"source","","text.content"}};
    first.text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    first.text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    first.text->direction_driver=TextDirectionDriver{{"source","","text.direction"}};
    first.text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    first.text->weight_expression=Expression{"500",1};
    first.text->italic_driver=TextItalicDriver{Expression{"true",1}};
    first.text->parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};
    first.text->parameters.at("tracking").expression=Expression{"2",1};
    first.text->font_features={{"kern",1,"whole_text"},{"liga",0,"whole_text"}};
    first.text->additional_axis_values={{"wdth",87.1234567890123}};
    Object second=source;second.id="second";second.name="Same name";
    second.text=default_text("second-text","Second literal ");
    second.text->alignment=mixed?"end":"start";second.text->weight=650;second.text->italic=true;
    second.text->parameters.at("font_size").literal=32;
    if(driven)second.text->alignment_driver=TextAlignmentDriver{{"source","","text.alignment"}};
    Object path;path.id="shape";path.name="Path";path.kind=Kind::path;
    Contour contour;contour.id="contour";Point begin;begin.id="begin";begin.x.literal=10;begin.y.literal=100;
    Point end;end.id="end";end.x.literal=400;end.y.literal=100;contour.points={begin,end};path.contours={contour};
    second.text->path_attachment=TextPathAttachment{"shape","contour","distance",0,0,false};
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
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets={"first","second"};
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_text_alignment_batch_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Alignment control exists");return found;
    }
    QComboBox* editor(){return control<QComboBox>("text-alignment-batch-editor");}
    QPushButton* apply(){return control<QPushButton>("text-alignment-batch-apply");}
    QString state(){return control<QLabel>("text-alignment-batch-state")->text();}
    QString status(){return control<QLabel>("text-alignment-batch-status")->text();}
    void choose(const char* value){const auto index=editor()->findData(QString::fromLatin1(value));check(index>=0,"Canonical alignment choice exists");editor()->setCurrentIndex(index);}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};
Document aligned(Document document,const std::string& value){
    for(const auto* id:{"first","second"})document.objects.at(id).text->alignment=value;return document;
}

void primary_and_mixed(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    check(inspector.state()=="Shared: Start"&&inspector.editor()->currentData().toString()=="start",
        "Equal retained Text alignment is displayed as the shared enum");
    check(!inspector.apply()->isEnabled(),"Unchanged shared alignment cannot create a history entry");
    inspector.apply()->click();inspector.choose("end");
    check(before.unchanged(session)&&inspector.apply()->isEnabled(),"Changing the combo is a draft only");
    inspector.choose("start");check(!inspector.apply()->isEnabled()&&before.unchanged(session),
        "Returning the draft to shared state is a no-op");
    inspector.choose("center");QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
    inspector.apply()->click();const auto expected=aligned(before.document,"center");
    check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "Apply uses one canonical batch and survives synchronous Inspector destruction");
    check(decode(encode(session.document()))==expected,
        "Only alignment changes; every other Text source, driver, expression, feature, path attachment and unrelated object survives native readback");
    check(inspector.state()=="Shared: Center"&&!inspector.apply()->isEnabled(),"Rebuilt Inspector shows the committed shared value");
    const auto committed=encode(session.document());inspector.undo();
    check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
        "One Undo restores both full Text sources and exact native state");
    inspector.redo();check(encode(session.document())==committed&&inspector.state()=="Shared: Center",
        "One Redo restores the entire alignment batch");

    inspector.load(fixture(true));const Snapshot mixed(session);
    check(inspector.state()=="Mixed"&&inspector.editor()->currentText()=="Mixed"&&
        inspector.editor()->currentData().toString().isEmpty()&&!inspector.apply()->isEnabled(),
        "Unequal enum values are visibly Mixed, without an invented shared value");
    inspector.choose("end");check(mixed.unchanged(session),"Mixed selection stays authored until Apply");
    inspector.apply()->click();check(session.document()==aligned(mixed.document,"end")&&session.revision()==1,
        "An absolute enum sets every retained Text target even when one already has that value");
    inspector.undo();check(session.document()==mixed.document&&inspector.state()=="Mixed","Undo restores per-target mixed alignment");
    inspector.redo();check(inspector.state()=="Shared: End","Redo restores the resolved shared enum");
}

void refusals_and_retained_targets(){
    Inspector inspector;auto& session=inspector.host.session;
    for(const auto& targets:std::vector<std::vector<Id>>{{"first","shape"},{"first","first"},{"first","missing"}}){
        inspector.targets=targets;inspector.load();const Snapshot refused(session);
        check(inspector.state()=="Unavailable"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&!inspector.status().isEmpty(),
            "Mixed-kind, duplicate or missing targets reject explicitly without filtering");
        inspector.apply()->click();check(refused.unchanged(session),"An unavailable selection cannot partially edit a literal target");
    }
    inspector.targets={"first","second"};inspector.load(fixture(false,true));const Snapshot linked(session);
    check(inspector.status().startsWith("DRIVEN_PROPERTY")&&inspector.status().contains("second")&&!inspector.apply()->isEnabled(),
        "One linked alignment refuses the complete batch and identifies its target");
    inspector.choose("center");inspector.apply()->click();check(linked.unchanged(session),
        "Batch controls never silently remove an alignment link or change an earlier literal target");

    inspector.load();inspector.choose("center");session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale(session);
    inspector.apply()->click();check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),
        "A stale revision rejects without any document, native, revision or History changes");
    inspector.load();inspector.choose("center");inspector.host.session_id+="-different";const Snapshot other_session(session);
    inspector.apply()->click();check(other_session.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),
        "A replaced Host Session with the same IDs is refused");
    inspector.load();inspector.choose("center");auto replacement=fixture();replacement.id="other-document";
    session=Session(std::move(replacement));const Snapshot other_document(session);
    inspector.apply()->click();check(other_document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),
        "A same-revision document replacement is refused independently of Session ID");
    inspector.load();inspector.choose("center");session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),
        "An active gesture and its preview survive a refused batch Apply");
    session.cancel_gesture();inspector.apply()->click();
    check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),
        "Cancelling a gesture invalidates the old alignment edit context");

    inspector.load();const Snapshot retained(session);inspector.choose("center");inspector.targets={"source","first"};
    inspector.apply()->click();check(session.document()==aligned(retained.document,"center"),
        "Later caller selection changes cannot retarget the exact retained Text IDs");
}
}

int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{primary_and_mixed();refusals_and_retained_targets();
        std::cout<<"PASS "<<checks<<" Text alignment batch Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
