#include "host.hpp"
#include "text_path_batch_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
Document fixture(bool mixed=false){
    auto document=empty_document("path-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::text;
    source.text=default_text("source-text","ABC");source.text->family="Arial";
    Object first=source;first.id="first";first.name="Same name";first.text->id="first-text";
    first.text->content_driver=TextContentDriver{{"source","","text.content"}};
    first.text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    first.text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    first.text->direction_driver=TextDirectionDriver{{"source","","text.direction"}};
    first.text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    first.text->alignment_driver=TextAlignmentDriver{{"source","","text.alignment"}};
    first.text->parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};
    first.text->parameters.at("tracking").expression=Expression{"2",1};
    first.text->weight_expression=Expression{"500",1};
    first.text->italic_driver=TextItalicDriver{Expression{"true",1}};
    first.text->font_features={{"kern",1,"whole_text"},{"liga",0,"whole_text"}};
    first.text->additional_axis_values={{"wdth",87.5}};
    Object second=source;second.id="second";second.name="Same name";second.text->id="second-text";
    second.text->content="DEF";second.text->alignment="end";second.text->weight=650;second.text->italic=true;
    second.text->parameters.at("font_size").literal=32;
    Object path;path.id="path";path.name="Same name";path.kind=Kind::path;path.visible=false;
    Point begin,end;begin.id="begin";begin.x.literal=10;begin.y.literal=100;
    end.id="end";end.x.literal=3010;end.y.literal=100;
    path.contours={{"contour",false,{begin,end}}};
    Object other_path=path;other_path.id="other-path";other_path.name="Same name";
    other_path.contours.front().id="other-contour";other_path.contours.front().points.front().id="other-begin";
    other_path.contours.front().points.back().id="other-end";
    if(mixed){
        first.text->path_attachment=TextPathAttachment{"path","contour","distance",1000,2,false};
        second.text->path_attachment=TextPathAttachment{"other-path","other-contour","normalized",0.25,4,true};
    }
    for(const auto& object:{source,first,second,path,other_path})document.objects.emplace(object.id,object);
    document.compositions.front().roots={"source","first","second","path","other-path"};
    return document;
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());}
};
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets={"first","second"};
    Inspector():host(directory.path()){check(directory.isValid(),"Owned scratch exists");host.changed=[this]{rebuild();};load();}
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_text_path_batch_controls(host,targets,&parent);}
    void load(Document document=fixture()){host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();}
    template<class T>T* get(const char* name){auto* value=controls->findChild<T*>(QString::fromLatin1(name));check(value!=nullptr,"Named Text on Path control exists");return value;}
    QComboBox* path(){return get<QComboBox>("text-path-batch-path");}
    QComboBox* contour(){return get<QComboBox>("text-path-batch-contour");}
    QComboBox* mode(){return get<QComboBox>("text-path-batch-start-mode");}
    QLineEdit* start(){return get<QLineEdit>("text-path-batch-start");}
    QLineEdit* spacing(){return get<QLineEdit>("text-path-batch-spacing");}
    QCheckBox* reversed(){return get<QCheckBox>("text-path-batch-reversed");}
    QPushButton* apply(){return get<QPushButton>("text-path-batch-apply");}
    QPushButton* detach(){return get<QPushButton>("text-path-batch-detach");}
    QPushButton* cancel(){return get<QPushButton>("text-path-batch-cancel");}
    QString state(){return get<QLabel>("text-path-batch-state")->text();}
    QString status(){return get<QLabel>("text-path-batch-status")->text();}
    void choose(QComboBox* combo,const char* id){const auto index=combo->findData(QString::fromLatin1(id));check(index>=0,"Stable ID choice exists");combo->setCurrentIndex(index);}
    void choose_path(const char* path_id="path",const char* contour_id="contour"){
        choose(path(),path_id);choose(contour(),contour_id);
    }
    void draft(){choose_path();choose(mode(),"distance");start()->setText("1000");spacing()->setText("3.5");reversed()->setCheckState(Qt::Unchecked);}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};
Document attached(Document document,const TextPathAttachment& attachment){
    for(const auto* id:{"first","second"})document.objects.at(id).text->path_attachment=attachment;
    return document;
}
Vec2 projected_anchor(const Document& document,const Id& id){
    const auto layout=evaluate_text_projection(document,id,evaluate(document));
    check(layout.contours&&!layout.contours->empty()&&!layout.contours->front().points.empty(),"Shared Text projection produces glyph geometry");
    const auto shape=evaluate_shape(document,id,evaluate(document));
    check(!shape.paths.empty()&&shape.paths.front().contours&&!shape.paths.front().contours->empty(),"Canvas/SVG Shape projection contains attached Text geometry");
    return layout.contours->front().points.front().anchor;
}
void lifecycle(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    check(inspector.state()=="Detached"&&!inspector.apply()->isEnabled()&&!inspector.detach()->isEnabled(),"Detached selection waits for explicit Path and Contour");
    check(inspector.path()->itemText(inspector.path()->findData(QStringLiteral("path"))).contains("[path]")&&
        inspector.path()->itemText(inspector.path()->findData(QStringLiteral("other-path"))).contains("[other-path]"),"Duplicate names expose disambiguating stable Path IDs");
    inspector.choose(inspector.path(),"path");
    check(inspector.contour()->currentData().toString().isEmpty()&&!inspector.apply()->isEnabled(),"Path selection does not silently choose a Contour");
    check(inspector.contour()->itemText(inspector.contour()->findData(QStringLiteral("contour"))).contains("[contour]"),"Contour selection exposes its exact stable ID");
    inspector.draft();check(before.unchanged(session)&&inspector.apply()->isEnabled(),"Path and numeric edits remain drafts until Apply");
    inspector.cancel()->click();check(before.unchanged(session)&&inspector.path()->currentData().toString().isEmpty()&&!inspector.apply()->isEnabled(),"Cancel discards the whole attachment draft without history");
    inspector.draft();QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
    inspector.targets={"source","first"}; // Source browsing must not become the retained target set.
    inspector.apply()->click();inspector.targets={"first","second"};inspector.rebuild();
    auto expected=attached(before.document,{"path","contour","distance",1000,3.5,false});
    check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&session.history().states.size()==before.history.states.size()+1,
        "One Apply updates both retained IDs atomically and survives synchronous Inspector destruction");
    check(decode(encode(session.document()))==expected,"Native readback preserves all Text styles, typed drivers, expressions, source IDs and unrelated objects");
    const auto first_anchor=projected_anchor(session.document(),"first"),second_anchor=projected_anchor(session.document(),"second");
    const auto committed=encode(session.document());inspector.undo();
    check(session.document()==before.document&&!session.can_undo(),"One Undo restores both detached editable Text sources");
    inspector.redo();check(encode(session.document())==committed,"One Redo restores the entire attachment batch");
    const Snapshot noop(session);check(!inspector.apply()->isEnabled(),"Equal attachment cannot enable a no-op Apply");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(noop.unchanged(session),"Forced unchanged Apply creates no revision or history entry");
    inspector.start()->setText("1200");const Snapshot before_update(session);inspector.apply()->click();
    const auto moved_first=projected_anchor(session.document(),"first"),moved_second=projected_anchor(session.document(),"second");
    check(std::abs(moved_first.x-first_anchor.x-200)<1e-6&&std::abs(moved_second.x-second_anchor.x-200)<1e-6&&
        std::abs(moved_first.y-first_anchor.y)<1e-6&&std::abs(moved_second.y-second_anchor.y)<1e-6,
        "Changing the shared distance start translates both glyph projections by the exact 200 du96 on a straight Path");
    check(session.revision()==before_update.revision+1&&session.history().states.size()==before_update.history.states.size()+1,"Updating both offsets is one history step");
    inspector.undo();check(session.document()==before_update.document,"One Undo reverses both updated offsets");inspector.redo();
    const auto text_first=*session.document().objects.at("first").text,text_second=*session.document().objects.at("second").text;
    session.apply({Set{{"path","begin","y"},140},Set{{"path","end","y"},140}},session.revision());inspector.host.edited();
    const auto edited_first=projected_anchor(session.document(),"first"),edited_second=projected_anchor(session.document(),"second");
    check(std::abs(edited_first.y-moved_first.y-40)<1e-6&&std::abs(edited_second.y-moved_second.y-40)<1e-6&&
        *session.document().objects.at("first").text==text_first&&*session.document().objects.at("second").text==text_second,
        "Source Path editing re-evaluates both glyph geometries without replacing either authored Text source");
    const Snapshot before_detach(session);inspector.start()->setText("invalid");
    check(inspector.detach()->isEnabled(),"Detach remains usable independently of an invalid attachment draft");
    inspector.detach()->click();auto detached=before_detach.document;
    for(const auto* id:{"first","second"})detached.objects.at(id).text->path_attachment.reset();
    check(session.document()==detached&&session.revision()==before_detach.revision+1&&session.history().states.size()==before_detach.history.states.size()+1,"Detach removes both retained attachments in one transaction and preserves all other Text fields");
    inspector.undo();check(session.document()==before_detach.document,"One Undo restores both exact attachments");inspector.redo();check(session.document()==detached,"One Redo detaches both Text objects");
    const Snapshot detached_noop(session);inspector.detach()->setEnabled(true);inspector.detach()->click();
    check(detached_noop.unchanged(session),"Repeated Detach on detached Text is a no-op");
}
void mixed_and_partial(){
    Inspector inspector;auto& session=inspector.host.session;inspector.load(fixture(true));const Snapshot before(session);
    check(inspector.state().startsWith("Mixed attachments")&&inspector.path()->currentData().toString().isEmpty()&&
        inspector.mode()->currentData().toString().isEmpty()&&inspector.start()->text().isEmpty()&&inspector.spacing()->text().isEmpty()&&
        inspector.reversed()->checkState()==Qt::PartiallyChecked&&!inspector.apply()->isEnabled(),"Mixed attachments display keep-each drafts without invented shared numeric, mode or traversal values");
    inspector.choose_path();inspector.spacing()->setText("5");inspector.cancel()->click();
    check(before.unchanged(session)&&inspector.path()->currentData().toString().isEmpty()&&inspector.spacing()->text().isEmpty()&&
        inspector.reversed()->checkState()==Qt::PartiallyChecked,"Cancel restores the original mixed draft and leaves distinct attachments untouched");
    inspector.choose_path();inspector.spacing()->setText("5");check(before.unchanged(session),"Resolving source and one mixed parameter remains a draft");
    inspector.apply()->click();auto expected=before.document;
    for(const auto* id:{"first","second"}){auto& attachment=*expected.objects.at(id).text->path_attachment;attachment.path="path";attachment.contour="contour";attachment.spacing=5;}
    check(session.document()==expected,"Applying one mixed parameter retains each Text's start mode, start, reversed flag and other source fields");
    inspector.undo();check(session.document()==before.document,"Undo restores distinct source Paths and all mixed values");inspector.redo();
    inspector.choose(inspector.mode(),"normalized");inspector.start()->setText("0.5");inspector.reversed()->setCheckState(Qt::Checked);
    const Snapshot normalized(session);inspector.apply()->click();
    expected=normalized.document;for(const auto* id:{"first","second"}){auto& attachment=*expected.objects.at(id).text->path_attachment;attachment.start_mode="normalized";attachment.start=0.5;attachment.reversed=true;}
    check(session.document()==expected,"Explicit normalized start and reverse choices update all retained Text objects");
    const Snapshot draft_before_dismissal(session);inspector.start()->setText("0.6");delete inspector.controls.data();
    check(draft_before_dismissal.unchanged(session),"Dismissing the Inspector discards its draft");
    auto partial=fixture();partial.objects.at("first").text->path_attachment=TextPathAttachment{"path","contour","distance",1000,2,false};
    inspector.load(partial);const Snapshot partial_before(session);check(inspector.detach()->isEnabled(),"A partly attached selection enables Detach");
    inspector.detach()->click();partial.objects.at("first").text->path_attachment.reset();
    check(session.document()==partial&&session.revision()==partial_before.revision+1&&session.history().states.size()==partial_before.history.states.size()+1,"Partial Detach edits attached targets only while preserving the detached target");
}
void atomic_refusals(){
    Inspector inspector;auto& session=inspector.host.session;
    for(const auto& targets:std::vector<std::vector<Id>>{{},{"first"},{"first","path"},{"first","first"},{"first","missing"}}){
        inspector.targets=targets;inspector.load();const Snapshot before(session);
        check(inspector.state()=="Unavailable"&&!inspector.path()->isEnabled()&&!inspector.apply()->isEnabled()&&!inspector.status().isEmpty(),"Invalid whole-Text target sets refuse visibly without filtering");
        inspector.apply()->setEnabled(true);inspector.apply()->click();check(before.unchanged(session),"Forced Apply with invalid targets remains atomic");
    }
    inspector.targets={"first","second"};
    for(const auto* refusal:{"frame","vertical","multiline"}){
        auto document=fixture();auto& later=*document.objects.at("second").text;
        if(std::string(refusal)=="frame")later.layout="frame";
        else if(std::string(refusal)=="vertical")later.direction="vertical";
        else later.content="Two\nlines";
        inspector.load(document);const Snapshot before(session);inspector.draft();inspector.apply()->click();
        const auto code=std::string(refusal)=="frame"?"TEXT_PATH_LAYOUT_UNSUPPORTED":std::string(refusal)=="vertical"?"TEXT_PATH_DIRECTION_UNSUPPORTED":"TEXT_PATH_MULTILINE_UNSUPPORTED";
        check(before.unchanged(session)&&inspector.status().startsWith(code),"A later unsupported Text rejects the complete candidate without partially attaching the earlier target");
    }
    inspector.load();inspector.draft();inspector.contour()->addItem("Missing contour [missing]","missing");inspector.choose(inspector.contour(),"missing");
    const Snapshot missing_contour(session);inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(missing_contour.unchanged(session)&&inspector.status().startsWith("MISSING_PATH_CONTOUR"),"A missing explicit Contour ID refuses without mutation");
    inspector.load();inspector.draft();inspector.path()->addItem("Missing Path [missing]","missing");inspector.choose(inspector.path(),"missing");
    inspector.contour()->addItem("Contour [contour]","contour");inspector.choose(inspector.contour(),"contour");
    const Snapshot missing_path(session);inspector.apply()->setEnabled(true);inspector.apply()->click();
    check(missing_path.unchanged(session)&&inspector.status().startsWith("MISSING_PATH_ATTACHMENT"),"A missing explicit Path ID refuses without mutation");
    inspector.load();inspector.draft();inspector.start()->setText("999999");const Snapshot overflow(session);inspector.apply()->click();
    check(overflow.unchanged(session)&&inspector.status().startsWith("TEXT_PATH_OVERFLOW"),"Open-path overflow refuses the whole batch");
    inspector.load();inspector.draft();inspector.spacing()->setText("-1");const Snapshot bad_spacing(session);
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(bad_spacing.unchanged(session)&&inspector.status().startsWith("TEXT_PATH_SPACING"),"Invalid spacing is checked again on forced Apply");
    inspector.load();inspector.draft();auto replacement=fixture();replacement.objects.erase("second");auto& roots=replacement.compositions.front().roots;
    roots.erase(std::remove(roots.begin(),roots.end(),"second"),roots.end());session=Session(replacement);const Snapshot missing_target(session);
    inspector.apply()->click();check(missing_target.unchanged(session)&&inspector.status().startsWith("MISSING_OBJECT"),"A disappeared later retained target refuses before any earlier update");
}
void stale_and_lifetime(){
    Inspector inspector;auto& session=inspector.host.session;inspector.draft();
    session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale(session);inspector.apply()->click();
    check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Later revision refuses the stale attachment draft");
    inspector.load(fixture(true));session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale_detach(session);inspector.detach()->click();
    check(stale_detach.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Detach independently guards the captured revision");
    inspector.load();inspector.draft();inspector.host.session_id+="-different";const Snapshot identity(session);inspector.apply()->click();
    check(identity.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Replaced Session identity refuses old controls");
    inspector.load();inspector.draft();auto other=fixture();other.id="other-document";session=Session(other);const Snapshot document(session);inspector.apply()->click();
    check(document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Document identity refuses same-revision replacement");
    inspector.load();inspector.draft();session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),"Active gesture and preview survive a refused attachment Apply");
    session.cancel_gesture();inspector.apply()->click();
    check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Cancelled gesture invalidates captured attachment context");
    inspector.load();inspector.draft();session.begin_gesture(session.revision());session.cancel_gesture();const Snapshot cancelled(session);inspector.apply()->click();
    check(cancelled.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A no-history gesture generation change still rejects the old draft");
    QTemporaryDir directory;QWidget parent;auto host=std::make_unique<Host>(directory.path());host->session=Session(fixture());
    QPointer<QWidget> controls=make_text_path_batch_controls(*host,{"first","second"},&parent);
    auto* start=controls->findChild<QLineEdit*>("text-path-batch-start");auto* apply=controls->findChild<QPushButton*>("text-path-batch-apply");
    auto* status=controls->findChild<QLabel*>("text-path-batch-status");host.reset();start->setText("7");
    check(status->text().startsWith("SESSION_CONFLICT")&&!apply->isEnabled(),"Host destruction makes retained controls refuse safely");
}
}
#include "text_path_batch_window_smoke.hpp"
#include "text_path_single_window_smoke.hpp"
int main(int argc,char** argv){
    bool pointer=false,single=false,pending_only=false;for(int i=1;i<argc;++i){
        if(std::string(argv[i])=="--context-pointer")pointer=true;
        if(std::string(argv[i])=="--single-context-pointer"){pointer=true;single=true;}
        if(std::string(argv[i])=="--single-pending-context-pointer"){pointer=true;single=true;pending_only=true;}
    }
    if(!pointer)qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    if(pointer){application.setStyle("Fusion");application.setStyleSheet(application_style_sheet());}
    try{if(single){text_path_single_window_smoke::run(pending_only);return 0;}
        if(pointer){text_path_batch_window_smoke::run();return 0;}
        lifecycle();mixed_and_partial();atomic_refusals();stale_and_lifetime();
        std::cout<<"PASS "<<checks<<" Text on Path batch Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
