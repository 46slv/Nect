#include "nect/blend.hpp"
#include "nect/io.hpp"
#ifndef NECT_MULTI_BLEND_MODE_CORE_ONLY
#include "host.hpp"
#include "multi_blend_mode_control.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWidget>
#endif
#include <iostream>
#include <stdexcept>

// CORE_ONLY is a source-preservation/Session smoke for a source-matched build.
// It neither executes nor qualifies the Qt controls or physical OS interaction.
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){
    try{action();}catch(const Error& error){check(error.code==code,"Expected exact refusal code");return;}
    throw std::runtime_error(std::string("Missing refusal: ")+code);
}
std::vector<Id> target_ids(){
    std::vector<Id> result={"first","second","third","instance"};
#ifdef _WIN32
    result.push_back("image");
#endif
    return result;
}
Object path(Id id){
    Object object;object.id=id;object.name="Same name";
    object.source=default_primitive(id+"-source","nect.shape.rectangle");
    object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));return object;
}
Document fixture(bool mixed=false){
    auto document=empty_document("blend-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::group;
    source.children={"source-path","source-text"};source.compositing.opacity.literal=.75;
    auto source_path=path("source-path");
    Object source_text;source_text.id="source-text";source_text.name="Source text";source_text.kind=Kind::text;
    source_text.text=default_text("source-text-content","Source text");
    Object first;first.id="first";first.name="Same name";first.kind=Kind::group;first.children={"first-child"};
    first.compositing.blend=mixed?"multiply":"normal";first.compositing.isolated=true;
    first.compositing.isolated_driver=Ref{"source","","composite.isolated"};
    first.compositing.opacity.binding=Binding{{"source","","composite.opacity"},.5,.125};
    first.compositing.mask=GeometryMask{"first-mask","source-path"};
    first.compositing.mask->enabled_expression=Expression{"true",1};
    first.visibility_driver=Ref{"source","","object.visible"};
    auto first_child=path("first-child");first_child.transform[4].literal=21;
    auto second=path("second");second.compositing.isolated=false;
    second.compositing.isolated_expression=Expression{"true",1};
    second.compositing.opacity.expression=Expression{".625",1};
    second.transform_parent="source";second.transform[4].literal=19;
    second.anchor[0].literal=13.125;second.visibility_expression=Expression{"false",1};
    Object third;third.id="third";third.name="Same name";third.kind=Kind::text;
    third.text=default_text("third-text","Authored literal");
    third.text->content_driver=TextContentDriver{{"source-text","","text.content"}};
    third.text->family_driver=TextFamilyDriver{{"source-text","","text.family"}};
    third.text->locale="ja-JP";third.text->weight_expression=Expression{"500",1};
    third.text->italic_driver=TextItalicDriver{Expression{"true",1}};
    third.text->parameters.at("tracking").expression=Expression{"2",1};
    third.text->font_features={{"kern",1,"whole_text"}};
    third.text->additional_axis_values={{"wdth",87.1234567890123}};
    third.stack.push_back(default_operation("third-fill","nect.paint.fill"));
    third.transform[4].binding=Binding{{"second","","transform.tx"},.5,0};
    Object definition;definition.id="definition-root";definition.name="Definition source";definition.kind=Kind::group;
    definition.children={"definition-path"};definition.visible=false;
    auto definition_path=path("definition-path");
    Object instance;instance.id="instance";instance.name="Same name";instance.kind=Kind::instance;
    instance.instance=DefinitionInstance{"definition",{{{"definition-path","","generator.width"},88}},{{"definition-path",false}}};
    instance.compositing.opacity.literal=.25;
    for(const auto& object:{source,source_path,source_text,first,first_child,second,third,definition,definition_path,instance})
        document.objects.emplace(object.id,object);
    document.definitions.emplace("definition",Definition{"definition","Definition","definition-root"});
    document.compositions.front().roots={"source","first","second","third","definition-root","instance"};
    document.collections.push_back({"collection","Retained selection",{"first","second"}});
#ifdef _WIN32
    RasterPixels pixels{1,1,{32,64,128,255}};auto raster=make_raster(encode_raster_png(pixels));
    document.raster_assets.emplace("asset",RasterAsset{"asset","Source image","embedded","",raster});
    Object image;image.id="image";image.name="Same name";image.kind=Kind::image;
    image.image=ImageSource{"asset",{160},{120}};image.compositing.isolated=true;
    image.image->width.binding=Binding{{"second","","generator.width"},1.5,0};
    document.objects.emplace(image.id,image);document.compositions.front().roots.push_back(image.id);
#endif
    return document;
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());
    }
};
Document reblended(Document document,const std::vector<Id>& targets,const std::string& mode){
    for(const auto& id:targets)document.objects.at(id).compositing.blend=mode;
    return document;
}
std::vector<Command> blend_commands(const Document& document,const std::vector<Id>& targets,const std::string& mode){
    std::vector<Command> commands;commands.reserve(targets.size());
    for(const auto& id:targets)commands.push_back(SetCompositing{id,mode,document.objects.at(id).compositing.isolated});
    return commands;
}
Document large_fixture(std::size_t size){
    auto document=empty_document("large-blend-document","composition","artboard");
    for(std::size_t i=0;i<size;++i){
        Object object;object.id="object-"+std::to_string(i);object.name="Same name";object.kind=Kind::group;
        object.compositing.isolated=i%2!=0;
        document.compositions.front().roots.push_back(object.id);document.objects.emplace(object.id,object);
    }
    return document;
}
void core_smoke(){
    const auto targets=target_ids();
    for(const auto& mode:blend_modes()){
        Session session(fixture(mode.id=="normal"));const Snapshot before(session);
        check(!evaluate_composite_isolation(session.document(),{"first","","composite.isolated"})&&
            evaluate_composite_isolation(session.document(),{"second","","composite.isolated"}),
            "Isolation drivers deliberately differ from the authored literals");
        session.apply(blend_commands(session.document(),targets,std::string(mode.id)),session.revision());
        const auto expected=reblended(before.document,targets,std::string(mode.id));
        check(session.document()==expected&&session.revision()==before.revision+1&&
            session.history().states.size()==before.history.states.size()+1,
            "Every supported blend changes only retained blend literals in one History step");
        check(decode(encode(session.document()))==expected,"Native roundtrip retains isolation/drivers, masks, opacity, stacks, Text, Instance and all unrelated state");
        const auto committed=encode(session.document());session.undo(session.revision());
        check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),
            "One Undo restores every complete target and unrelated authored state");
        session.redo(session.revision());check(encode(session.document())==committed,"One Redo restores the complete blend batch");
    }
    Session session(fixture());const Snapshot before(session);
    rejects("MISSING_OBJECT",[&]{session.apply({SetCompositing{"first","screen",true},SetCompositing{"missing","screen",false}},session.revision());});
    check(before.unchanged(session),"A later missing target rejects the whole blend batch atomically");
    rejects("UNSUPPORTED_BLEND",[&]{session.apply({SetCompositing{"first","screen",true},SetCompositing{"second","future-mode",false}},session.revision());});
    check(before.unchanged(session),"A later unsupported mode preserves authored/native/revision/History state");
    rejects("DRIVEN_PROPERTY",[&]{session.apply({SetCompositing{"first","screen",false},SetCompositing{"second","screen",false}},session.revision());});
    check(before.unchanged(session),"Changing isolation instead of carrying its own literal is refused");
    rejects("REVISION_CONFLICT",[&]{session.apply(blend_commands(session.document(),targets,"screen"),session.revision()+1);});
    check(before.unchanged(session),"A stale core transaction is history-free");
    Session limited(fixture(),{1024,128});const Snapshot admission(limited);
    rejects("HISTORY_LIMIT",[&]{limited.apply(blend_commands(limited.document(),targets,"screen"),limited.revision());});
    check(admission.unchanged(limited),"History admission failure is atomic for all targets");
    Session large(large_fixture(1000));const Snapshot original(large);const auto ids=large.document().compositions.front().roots;
    large.apply(blend_commands(large.document(),ids,"screen"),large.revision());
    check(large.document()==reblended(original.document,ids,"screen")&&large.revision()==1,
        "A 1000-object batch preserves differing per-target isolation");
    large.undo(large.revision());check(large.document()==original.document&&!large.can_undo(),"One Undo restores all 1000 objects");
}
#ifndef NECT_MULTI_BLEND_MODE_CORE_ONLY
using namespace nect::desktop;
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets=target_ids();
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_multi_blend_mode_controls(host,targets,&parent);}
    void load(Document document=fixture(),HistoryLimits limits={}){
        host.session=Session(std::move(document),limits);host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Blend control exists");return found;
    }
    QComboBox* selector(){return control<QComboBox>("multi-blend-mode-selector");}
    QPushButton* apply(){return control<QPushButton>("multi-blend-mode-apply");}
    QPushButton* cancel(){return control<QPushButton>("multi-blend-mode-cancel");}
    QString state(){return control<QLabel>("multi-blend-mode-state")->text();}
    QString status(){return control<QLabel>("multi-blend-mode-status")->text();}
    void choose(const std::string& value){
        const auto index=selector()->findData(QString::fromStdString(value));check(index>=0,"Supported exact blend ID exists");selector()->setCurrentIndex(index);
    }
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};
void primary_and_mixed(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot before(session);
    check(inspector.state()=="Shared: Normal"&&inspector.selector()->currentData().toString()=="normal",
        "Equal authored blends show shared mode despite mixed Object kinds and isolation");
    check(inspector.selector()->count()==static_cast<int>(blend_modes().size())+1&&!inspector.selector()->isEditable(),
        "Selector exposes only the immutable registry plus a non-authoring Mixed state");
    for(std::size_t i=0;i<blend_modes().size();++i){
        const auto& mode=blend_modes()[i];const auto index=static_cast<int>(i)+1;
        check(inspector.selector()->itemData(index).toString().toStdString()==mode.id&&
            inspector.selector()->itemText(index).toStdString()==mode.label,"Every selector label/ID follows exact registry order");
    }
    check(!inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),"Shared unchanged value has no pending draft");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(before.unchanged(session),"Forced unchanged Apply is history-free");
    inspector.choose("screen");check(before.unchanged(session)&&inspector.apply()->isEnabled()&&inspector.cancel()->isEnabled(),"Selecting a blend edits only the draft");
    inspector.cancel()->click();check(before.unchanged(session)&&inspector.selector()->currentData().toString()=="normal"&&
        !inspector.apply()->isEnabled()&&!inspector.cancel()->isEnabled(),"Cancel restores the shared draft without authored/native/revision/History changes");
    inspector.choose("screen");inspector.choose("normal");check(!inspector.apply()->isEnabled()&&before.unchanged(session),"Returning to shared blend is a no-op");
    inspector.choose("hue");QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_apply=inspector.apply();
    inspector.apply()->click();const auto expected=reblended(before.document,inspector.targets,"hue");
    check(!retired&&!retired_apply&&session.document()==expected&&session.revision()==before.revision+1&&
        session.history().states.size()==before.history.states.size()+1,"Apply survives synchronous panel destruction and commits one blend-only transaction");
    check(decode(encode(session.document()))==expected,"Qt Apply preserves every authored field and dependency except selected blend");
    const auto committed=encode(session.document());inspector.undo();
    check(session.document()==before.document&&encode(session.document())==before.native&&!session.can_undo(),"One UI Undo restores the full blend batch exactly");
    inspector.redo();check(encode(session.document())==committed&&inspector.state()=="Shared: Hue","One UI Redo restores all blends");
    inspector.load(fixture(true));const Snapshot mixed(session);
    check(inspector.state()=="Mixed"&&inspector.selector()->currentText()=="Mixed"&&
        inspector.selector()->currentData().toString().isEmpty()&&!inspector.apply()->isEnabled(),"Mixed selection never invents a shared authored blend");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(mixed.unchanged(session),"Forced Mixed Apply is history-free");
    inspector.choose("screen");inspector.cancel()->click();check(mixed.unchanged(session)&&inspector.selector()->currentText()=="Mixed"&&!inspector.apply()->isEnabled(),"Cancel restores Mixed and each original blend");
    inspector.choose("normal");inspector.apply()->click();check(session.document()==reblended(mixed.document,inspector.targets,"normal")&&session.revision()==1,"Mixed Apply sets all targets including those already carrying the selected mode");
    inspector.undo();check(session.document()==mixed.document&&inspector.state()=="Mixed","Undo restores the original mixed per-target blend literals");
    inspector.redo();check(inspector.state()=="Shared: Normal","Redo restores the shared mode");
    for(const auto& mode:blend_modes()){
        inspector.load(fixture(mode.id=="normal"));const Snapshot source(session);
        inspector.choose(std::string(mode.id));check(source.unchanged(session),"Every registry mode remains draft-only before Apply");
        inspector.apply()->click();check(session.document()==reblended(source.document,inspector.targets,std::string(mode.id)),"Every supported registry mode is applied exactly without aliases");
    }
}
void guards_and_retained_targets(){
    Inspector inspector;auto& session=inspector.host.session;
    for(const auto& targets:std::vector<std::vector<Id>>{{},{"first"},{"first","first"},{"first","missing"},
            {"first","definition-path@instance"},std::vector<Id>(1001,"first")}){
        inspector.targets=targets;inspector.load();const Snapshot refused(session);
        check(inspector.state()=="Unavailable"&&!inspector.selector()->isEnabled()&&!inspector.apply()->isEnabled()&&!inspector.status().isEmpty(),"Empty, singleton, duplicate, missing, virtual or over-limit targets refuse as a complete selection");
        inspector.choose("screen");inspector.apply()->setEnabled(true);inspector.apply()->click();
        check(refused.unchanged(session),"An unavailable or mixed valid/unsupported selection never edits its valid subset");
    }
    inspector.targets=target_ids();inspector.load();const Snapshot unsupported(session);
    inspector.selector()->addItem("Future","future-mode");inspector.selector()->setCurrentIndex(inspector.selector()->count()-1);
    check(!inspector.apply()->isEnabled()&&inspector.status().startsWith("UNSUPPORTED_BLEND"),"An injected unsupported draft is refused before Apply");
    inspector.apply()->setEnabled(true);inspector.apply()->click();check(unsupported.unchanged(session)&&inspector.status().startsWith("UNSUPPORTED_BLEND"),"Unsupported Apply does not fall back or create history");
    inspector.cancel()->click();check(unsupported.unchanged(session)&&inspector.status().isEmpty()&&inspector.selector()->currentData().toString()=="normal","Cancel after a refusal restores the original draft");
    inspector.load();inspector.choose("screen");session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale(session);
    inspector.apply()->click();check(stale.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Stale revision refuses without partial mutation");
    inspector.load();inspector.choose("screen");inspector.host.session_id+="-different";const Snapshot other_session(session);
    inspector.apply()->click();check(other_session.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Replaced Host Session refuses retained IDs");
    inspector.load();inspector.choose("screen");auto replacement=fixture();replacement.id="other-document";session=Session(replacement);const Snapshot other_document(session);
    inspector.apply()->click();check(other_document.unchanged(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Document identity guards same-revision replacements independently");
    inspector.load();inspector.choose("screen");session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.unchanged(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),"Refused Apply preserves an active gesture and its preview");
    session.cancel_gesture();inspector.apply()->click();check(gesture.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Cancelled gesture generation invalidates an old blend draft");
    inspector.load();session.begin_gesture(session.revision());inspector.rebuild();const Snapshot active(session);
    check(inspector.state()=="Unavailable"&&inspector.status().startsWith("GESTURE_ACTIVE")&&active.unchanged(session),"Opening during a gesture refuses immediately");session.cancel_gesture();
    inspector.load(fixture(),{1024,128});inspector.choose("screen");const Snapshot admission(session);inspector.apply()->click();
    check(admission.unchanged(session)&&inspector.status().startsWith("HISTORY_LIMIT")&&inspector.cancel()->isEnabled(),"History admission failure preserves sources and keeps the draft cancellable");
    inspector.cancel()->click();check(admission.unchanged(session)&&inspector.status().isEmpty(),"Cancel after History refusal remains history-free");
    inspector.load();const Snapshot retained(session);const auto original_targets=inspector.targets;inspector.choose("screen");inspector.targets={"source","first-child"};
    inspector.apply()->click();check(session.document()==reblended(retained.document,original_targets,"screen"),"Later caller selection changes cannot retarget captured authored IDs");
    inspector.targets=target_ids();inspector.load();const Snapshot dismissed(session);inspector.choose("screen");delete inspector.controls.data();
    check(dismissed.unchanged(session),"Closing a drafted panel never applies it");inspector.rebuild();
    check(inspector.selector()->currentData().toString()=="normal"&&!inspector.apply()->isEnabled(),"Reopening discards the retired draft");
    inspector.choose("screen");inspector.host.changed={};inspector.apply()->click();const Snapshot once(session);
    inspector.apply()->click();check(once.unchanged(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"Repeated Apply on an unreconstructed panel cannot commit twice");
    inspector.targets={"first","second"};inspector.load();const Snapshot two(session);inspector.choose("screen");inspector.apply()->click();
    check(session.document()==reblended(two.document,inspector.targets,"screen"),"The two-object boundary applies only those exact authored objects");
    auto large=large_fixture(1000);inspector.targets=large.compositions.front().roots;inspector.load(large);const Snapshot boundary(session);
    inspector.choose("screen");inspector.apply()->click();check(session.document()==reblended(boundary.document,inspector.targets,"screen")&&session.revision()==1,"The 1000-object boundary is one isolation-preserving UI transaction");
    inspector.undo();check(session.document()==boundary.document&&!session.can_undo(),"One UI Undo restores all 1000 objects");
}
#endif
}

#ifdef NECT_MULTI_BLEND_MODE_CORE_ONLY
int main(){
    try{core_smoke();std::cout<<"PASS "<<checks<<" source-bound multi-blend core smoke checks (Qt UI NOT_RUN; full regression NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{core_smoke();primary_and_mixed();guards_and_retained_targets();
        std::cout<<"PASS "<<checks<<" multi-blend Qt/core checks (physical OS input NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#endif
