#include "host.hpp"
#include "text_italic_batch_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
Document fixture(bool first_italic=false,bool second_italic=false){
    auto document=empty_document("italic-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::text;
    source.text=default_text("source-text","Source 日本語");source.text->italic=true;
    Object first=source;first.id="first";first.name="Same name";
    first.text=default_text("first-text","First\n日本語");first.text->italic=first_italic;
    first.text->family="Arial";first.text->locale="en-US";first.text->layout="frame";
    first.text->direction="vertical";first.text->alignment="center";
    first.text->content_driver=TextContentDriver{{"source","","text.content"}};
    first.text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    first.text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    first.text->direction_driver=TextDirectionDriver{{"source","","text.direction"}};
    first.text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    first.text->alignment_driver=TextAlignmentDriver{{"source","","text.alignment"}};
    first.text->weight_expression=Expression{"500",1};
    first.text->parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};
    first.text->parameters.at("tracking").expression=Expression{"2",1};
    first.text->parameters.at("frame_width").literal=271.25;
    first.text->parameters.at("frame_height").literal=136.5;
    first.text->font_features={{"kern",1,"whole_text"},{"liga",0,"whole_text"}};
    first.text->additional_axis_values={{"wdth",87.1234567890123}};
    Object second=source;second.id="second";second.name="Same name";
    second.text=default_text("second-text","Second literal ");second.text->italic=second_italic;
    second.text->weight=650;second.text->alignment="end";
    second.text->parameters.at("font_size").literal=32;
    second.text->parameters.at("frame_width").literal=481.75;
    second.text->parameters.at("frame_height").literal=223.5;
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
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets={"first","second"};
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_text_italic_batch_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Italic control exists");return found;
    }
    QCheckBox* editor(){return control<QCheckBox>("text-italic-batch-editor");}
    QPushButton* apply(){return control<QPushButton>("text-italic-batch-apply");}
    QPushButton* cancel(){return control<QPushButton>("text-italic-batch-cancel");}
    QString state(){return control<QLabel>("text-italic-batch-state")->text();}
    QString status(){return control<QLabel>("text-italic-batch-status")->text();}
    void undo(){host.session.undo(host.session.revision());host.edited();}
};
Document italicized(Document document,bool value){
    for(const auto* id:{"first","second"})document.objects.at(id).text->italic=value;return document;
}

void shared_choices_and_cancel(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    check(inspector.state()=="Shared: Off"&&inspector.editor()->checkState()==Qt::Unchecked&&!inspector.editor()->isTristate(),
        "Shared false shows an ordinary unchecked checkbox");
    check(!inspector.apply()->isEnabled(),"An unchanged shared bool cannot create history");
    inspector.apply()->click();inspector.editor()->click();
    check(before.unchanged(session)&&inspector.editor()->checkState()==Qt::Checked&&inspector.apply()->isEnabled(),
        "Choosing true is a draft until Apply");
    inspector.cancel()->click();
    check(before.unchanged(session)&&inspector.editor()->checkState()==Qt::Unchecked&&!inspector.apply()->isEnabled(),
        "Cancel restores the original checkbox without document or history edits");
    inspector.editor()->click();inspector.editor()->click();
    check(before.unchanged(session)&&!inspector.apply()->isEnabled(),"Returning to shared false is a no-op");
    inspector.editor()->click();QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
    inspector.apply()->click();const auto expected=italicized(before.document,true);
    check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,
        "True applies in one transaction and survives synchronous Inspector destruction");
    check(decode(encode(session.document()))==expected,
        "Only italic changes; all other sources, drivers, axes, features, frame dimensions, stable IDs and unrelated objects survive native readback");
    check(inspector.state()=="Shared: On"&&inspector.editor()->checkState()==Qt::Checked&&!inspector.apply()->isEnabled(),
        "Committed true rebuilds as checked and has no pending draft");
    const auto committed=encode(session.document());inspector.undo();
    check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
        "One Undo restores every original Text source and exact native state");
    session.redo(session.revision());inspector.host.edited();
    check(encode(session.document())==committed,"One Redo restores the true batch");
    inspector.editor()->click();inspector.apply()->click();
    check(session.document()==before.document&&inspector.state()=="Shared: Off", "Checked toggles back to an explicit false batch");
}

void mixed_is_presentation_only(){
    for(const bool value:{false,true}){
        Inspector inspector;inspector.load(fixture(false,true));auto& session=inspector.host.session;const Snapshot before(session);
        check(inspector.state()=="Mixed"&&inspector.editor()->checkState()==Qt::PartiallyChecked&&
            inspector.editor()->isTristate()&&!inspector.apply()->isEnabled(),
            "Different literals show Mixed with no implicit authored bool");
        inspector.editor()->click();
        check(inspector.editor()->checkState()==Qt::Checked&&!inspector.editor()->isTristate(),
            "First activation leaves Mixed for definite true and disables tri-state cycling");
        if(!value)QTest::keyClick(inspector.editor(),Qt::Key_Space);
        check(before.unchanged(session)&&inspector.editor()->checkState()==(value?Qt::Checked:Qt::Unchecked),
            "Click and keyboard activation choose definite booleans without editing the document");
        inspector.cancel()->click();
        check(before.unchanged(session)&&inspector.editor()->checkState()==Qt::PartiallyChecked&&!inspector.apply()->isEnabled(),
            "Cancelling a resolved Mixed draft restores Mixed without history");
        inspector.editor()->click();if(!value)inspector.editor()->click();
        inspector.apply()->click();
        check(session.document()==italicized(before.document,value)&&session.revision()==1,
            "Both explicit bool choices update every exact target, including an already-equal one");
        inspector.undo();check(session.document()==before.document&&inspector.state()=="Mixed", "One Undo restores the mixed literals");
    }
    Inspector inspector;const Snapshot before(inspector.host.session);inspector.editor()->setCheckState(Qt::PartiallyChecked);
    check(!inspector.apply()->isEnabled()&&before.unchanged(inspector.host.session),
        "Even a programmatically indeterminate draft cannot author a bool");
    inspector.apply()->click();delete inspector.controls.data();
    check(before.unchanged(inspector.host.session),"Dismissing the component with an unapplied draft leaves history unchanged");
}

void refused_batches(){
    Inspector inspector;auto& session=inspector.host.session;
    for(const auto& targets:std::vector<std::vector<Id>>{{},{"first"},{"first","shape"},{"first","first"},{"first","missing"}}){
        inspector.targets=targets;inspector.load();const Snapshot before(session);
        check(inspector.state()=="Unavailable"&&!inspector.editor()->isEnabled()&&!inspector.apply()->isEnabled()&&!inspector.status().isEmpty(),
            "Invalid, mixed-kind, duplicate and missing targets reject without filtering");
        inspector.apply()->click();inspector.cancel()->click();
        check(before.unchanged(session),"An unavailable batch cannot change an earlier literal target");
    }
    inspector.targets={"first","second"};
    for(const auto& driver:std::vector<TextItalicDriver>{Ref{"source","","text.italic"},Expression{"true",1}}){
        auto document=fixture();document.objects.at("second").text->italic_driver=driver;inspector.load(std::move(document));const Snapshot before(session);
        check(inspector.status().startsWith("DRIVEN_PROPERTY")&&inspector.status().contains("second")&&!inspector.apply()->isEnabled(),
            "Link and expression italic drivers refuse the entire batch with the target ID");
        inspector.editor()->setCheckState(Qt::Checked);inspector.apply()->click();
        check(before.unchanged(session),"Batch editing never unlinks a bool driver implicitly");
    }
    inspector.load();inspector.editor()->click();session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale(session);
    inspector.apply()->click();check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"), "Stale revision refuses atomically");
    inspector.load();inspector.editor()->click();inspector.host.session_id+="-different";const Snapshot other_session(session);
    inspector.apply()->click();check(other_session.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"), "Stale Session refuses even with identical Text IDs");
    inspector.load();inspector.editor()->click();auto replacement=fixture();replacement.id="other-document";session=Session(std::move(replacement));const Snapshot other_document(session);
    inspector.apply()->click();check(other_document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"), "Same-revision document replacement refuses");
    inspector.load();inspector.editor()->click();session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),
        "Active gesture and preview survive a refused bool batch");
    session.cancel_gesture();inspector.apply()->click();
    check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Cancelled gesture invalidates the old draft context");
    inspector.load();const Snapshot retained(session);inspector.editor()->click();inspector.targets={"source","first"};inspector.apply()->click();
    check(session.document()==italicized(retained.document,true),"Later caller selection cannot retarget captured unique Text IDs");
}
}

int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{shared_choices_and_cancel();mixed_is_presentation_only();refused_batches();
        std::cout<<"PASS "<<checks<<" Text italic batch Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
