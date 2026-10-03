#include "host.hpp"
#include "text_string_source_batch_control.hpp"
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
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
const std::vector<Id> text_targets{"first","second"};
Object text_object(const Id& id,const std::string& family,const std::string& locale){
    Object object;object.id=id;object.name="Same visible name";object.kind=Kind::text;
    object.text=default_text(id+"-text",id+" 日本語");object.text->family=family;object.text->locale=locale;
    return object;
}
Document fixture(){
    auto document=empty_document("string-source-document","composition","artboard");
    document.objects.emplace("source",text_object("source","Source Family","en-GB"));
    document.objects.emplace("alternate",text_object("alternate","Alternate Family","ja-JP"));
    for(std::size_t index=0;index<text_targets.size();++index){
        const auto& id=text_targets[index];auto object=text_object(id,index==0?"Arial":"Consolas",index==0?"en-US":"ja");
        auto& text=*object.text;
        text.content_driver=TextContentDriver{{"source","","text.content"}};
        text.weight_expression=Expression{"500",1};text.italic_driver=TextItalicDriver{Expression{"true",1}};
        text.direction_driver=TextDirectionDriver{{"source","","text.direction"}};
        text.layout_driver=TextLayoutDriver{{"source","","text.layout"}};
        text.alignment_driver=TextAlignmentDriver{{"source","","text.alignment"}};
        text.parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};
        text.parameters.at("tracking").expression=Expression{"2",1};
        text.font_features={{"kern",1,"whole_text"},{"liga",0,"whole_text"}};
        text.additional_axis_values={{"wdth",87.1234567890123}};
        object.transform[4].literal=index==0?11.123456789:211.987654321;
        object.compositing.opacity.literal=.375;object.compositing.blend="multiply";
        auto fill=default_operation(id+"-fill","nect.paint.fill");fill.fill_rule="evenodd";
        fill.parameters.at("g").expression=Expression{"0.43210987654321",1};object.stack={fill};
        document.objects.emplace(id,std::move(object));
    }
    Object group;group.id="source-folder";group.name="Source folder";group.kind=Kind::group;group.children={"source","alternate"};
    document.objects.emplace(group.id,std::move(group));
    document.compositions.front().roots={"source-folder","first","second"};return document;
}
Document linked(Document document,const std::string& field,const Id& source){
    for(const auto& id:text_targets){auto& text=*document.objects.at(id).text;
        if(field=="text.family")text.family_driver=TextFamilyDriver{{source,"",field}};
        else text.locale_driver=TextLocaleDriver{{source,"",field}};
    }
    return document;
}
Document unlinked(Document document,const std::string& field){
    // Compute the independent values before changing any selected driver.
    std::vector<std::string> values;
    for(const auto& id:text_targets)values.push_back(field=="text.family"?
        evaluate_text_family(document,id):evaluate_text_locale(document,id));
    for(std::size_t index=0;index<text_targets.size();++index){
        auto& text=*document.objects.at(text_targets[index]).text;
        if(field=="text.family"&&text.family_driver){text.family=values[index];text.family_driver.reset();}
        if(field=="text.locale"&&text.locale_driver){text.locale=values[index];text.locale_driver.reset();}
    }
    return document;
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()),native(encode(session.document())){}
    bool same(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());
    }
};
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;
    std::vector<Id> targets=text_targets;
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_text_string_source_batch_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* result=controls->findChild<T*>(QString::fromLatin1(name));
        if(!result)throw std::runtime_error("Expected Text string source control is missing");return result;
    }
    QComboBox* field(){return control<QComboBox>("text-string-source-batch-field");}
    QComboBox* mode(){return control<QComboBox>("text-string-source-batch-mode");}
    QComboBox* sources(){return control<QComboBox>("text-string-source-batch-source");}
    QLineEdit* search(){return control<QLineEdit>("text-string-source-batch-source-search");}
    QCheckBox* replace(){return control<QCheckBox>("text-string-source-batch-replace-driver");}
    QPushButton* apply(){return control<QPushButton>("text-string-source-batch-apply");}
    QPushButton* cancel(){return control<QPushButton>("text-string-source-batch-cancel");}
    QString state(){return control<QLabel>("text-string-source-batch-state")->text();}
    QString status(){return control<QLabel>("text-string-source-batch-status")->text();}
    void choose(QComboBox* combo,const std::string& value){
        const auto index=combo->findData(QString::fromStdString(value));check(index>=0,"Canonical source choice exists");combo->setCurrentIndex(index);
    }
    void choose_field(const std::string& value){choose(field(),value);}
    void choose_mode(const std::string& value){choose(mode(),value);}
    int source_index(const Id& id){
        const auto path=" · "+QString::fromStdString(id)+" / "+field()->currentData().toString();
        for(int index=0;index<sources()->count();++index)if(sources()->itemText(index).endsWith(path))return index;
        return -1;
    }
    void choose_source(const Id& id){const auto index=source_index(id);check(index>=0,"Requested stable source is visible");sources()->setCurrentIndex(index);}
    void draft_link(const std::string& selected_field,const Id& id){choose_field(selected_field);choose_mode("link");choose_source(id);}
    void force_apply(){apply()->setEnabled(true);apply()->click();}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};

void link_cancel_history_and_preservation(){
    Inspector inspector;auto& session=inspector.host.session;
    check(inspector.state().startsWith("Mixed evaluated values")&&!inspector.apply()->isEnabled()&&
        !inspector.replace()->isChecked()&&inspector.sources()->currentIndex()==-1,"Initial mixed selection has no implicit action or source");
    for(const std::string field:{"text.family","text.locale"}){
        auto document=fixture();
        // The other string field has retained sources too.
        document=linked(document,field=="text.family"?"text.locale":"text.family","alternate");
        inspector.load(document);const Snapshot before(session);
        inspector.draft_link(field,"source");inspector.search()->setText("source text.");
        check(before.same(session)&&inspector.apply()->isEnabled(),"Source search and selection are only a draft");
        inspector.cancel()->click();
        check(before.same(session)&&inspector.field()->currentData().toString()=="text.family"&&
            inspector.mode()->currentData().toString().isEmpty()&&inspector.sources()->currentIndex()==-1&&
            !inspector.replace()->isChecked()&&!inspector.apply()->isEnabled(),"Cancel discards the complete source draft without History");
        inspector.draft_link(field,"source");QPointer<QWidget> retired=inspector.controls;
        inspector.apply()->click();const auto expected=linked(before.document,field,"source");
        check(!retired&&session.document()==expected&&session.revision()==before.revision+1&&
            session.history().states.size()==before.history.states.size()+1,"Typed Link changes only the selected field in one Undo and survives Inspector deletion");
        check(decode(encode(session.document()))==expected,"Native roundtrip retains exact strings, drivers and unrelated source state");
        const Snapshot noop(session);inspector.draft_link(field,"source");
        check(!inspector.apply()->isEnabled(),"Already-linked all-target source is a no-op");inspector.force_apply();
        check(noop.same(session),"No-op Apply cannot add a revision or History");
        inspector.undo();check(session.document()==before.document,"One Undo restores every target and unrelated driver");
        inspector.redo();check(session.document()==expected,"One Redo restores the exact atomic Link");
    }
}

void explicit_replace_and_independent_unlink(){
    Inspector inspector;auto& session=inspector.host.session;
    for(const std::string field:{"text.family","text.locale"}){
        auto document=fixture();auto& second=*document.objects.at("second").text;
        if(field=="text.family")second.family_driver=TextFamilyDriver{{"alternate","",field}};
        else second.locale_driver=TextLocaleDriver{{"alternate","",field}};
        inspector.load(document);const Snapshot mixed(session);inspector.draft_link(field,"source");
        check(!inspector.replace()->isChecked()&&!inspector.apply()->isEnabled()&&inspector.status().startsWith("DRIVEN_PROPERTY"),
            "One different existing source refuses the entire Link draft by default");
        inspector.force_apply();check(mixed.same(session),"Refused replacement does not link the earlier literal target");
        inspector.replace()->setChecked(true);check(inspector.apply()->isEnabled(),"Explicit replacement authorizes a mixed-source Link");
        inspector.apply()->click();check(session.document()==linked(mixed.document,field,"source"),"Replace touches only the selected field's source");
        inspector.undo();check(session.document()==mixed.document,"Replacement has one exact Undo");

        document=linked(fixture(),field,"source");
        if(field=="text.family")document.objects.at("second").text->family_driver=TextFamilyDriver{{"alternate","",field}};
        else document.objects.at("second").text->locale_driver=TextLocaleDriver{{"alternate","",field}};
        // Keep the other field driven while unlinking this field.
        document=linked(document,field=="text.family"?"text.locale":"text.family","source");
        inspector.load(document);const Snapshot driven(session);inspector.choose_field(field);inspector.choose_mode("unlink");
        check(inspector.apply()->isEnabled()&&driven.same(session),"Unlink is an explicit history-free draft");
        inspector.apply()->click();const auto expected=unlinked(driven.document,field);
        check(session.document()==expected&&session.revision()==1&&session.history().states.size()==driven.history.states.size()+1,
            "Unlink freezes each target's own different evaluated string in one transaction");
        inspector.choose_field(field);inspector.choose_mode("unlink");const Snapshot literal(session);inspector.force_apply();
        check(literal.same(session),"Already-literal Unlink is History-free");
        inspector.undo();check(session.document()==driven.document,"Unlink Undo restores each independent source");
        inspector.redo();check(session.document()==expected,"Unlink Redo preserves all unrelated drivers");

        document=fixture();
        if(field=="text.family")document.objects.at("first").text->family_driver=TextFamilyDriver{{"second","",field}};
        else document.objects.at("first").text->locale_driver=TextLocaleDriver{{"second","",field}};
        inspector.load(document);const Snapshot chain(session);inspector.choose_field(field);inspector.choose_mode("unlink");inspector.apply()->click();
        check(session.document()==unlinked(chain.document,field),"Unlink a selected-target chain freezes its own value and leaves literal targets exact");
    }
    inspector.load(linked(fixture(),"text.family","alternate"));inspector.draft_link("text.family","source");inspector.replace()->setChecked(true);
    inspector.choose_field("text.locale");check(!inspector.replace()->isChecked()&&inspector.sources()->currentIndex()==-1,
        "Changing field clears previous replacement consent and source identity");
}

void source_eligibility_search_and_selection_refusals(){
    Inspector inspector;auto& session=inspector.host.session;auto document=fixture();
    auto foreign=empty_document("unused","foreign-composition","foreign-artboard").compositions.front();foreign.roots={"foreign"};
    document.objects.emplace("foreign",text_object("foreign","Foreign Family","fr"));document.compositions.push_back(foreign);
    auto dependent=text_object("dependent","Dependent","en");dependent.text->family_driver=TextFamilyDriver{{"second","","text.family"}};
    document.objects.emplace(dependent.id,dependent);document.compositions.front().roots.push_back(dependent.id);
    auto external=text_object("external-chain","External","en");external.text->family_driver=TextFamilyDriver{{"foreign","","text.family"}};
    document.objects.emplace(external.id,external);document.compositions.front().roots.push_back(external.id);
    inspector.load(document);const Snapshot before(session);inspector.choose_mode("link");
    check(inspector.source_index("foreign")==-1&&inspector.source_index("dependent")==-1&&inspector.source_index("external-chain")==-1&&
        inspector.source_index("first")==-1&&inspector.source_index("second")==-1,
        "Source picker excludes foreign sources, external chains, selected targets and all cycles through targets");
    inspector.choose_source("source");inspector.search()->setText("SOURCE text.family");
    check(inspector.sources()->count()==1&&inspector.sources()->currentIndex()==0,"Multi-term case-insensitive search keeps the stable source identity");
    inspector.search()->setText("alternate");check(inspector.sources()->currentIndex()==-1&&!inspector.apply()->isEnabled(),
        "Filtering away a source never silently selects another row");
    inspector.force_apply();check(before.same(session)&&inspector.status().startsWith("MISSING_REFERENCE"),"A hidden or absent source refuses every target");
    inspector.choose_source("alternate");inspector.search()->clear();check(inspector.sources()->currentIndex()==inspector.source_index("alternate"),
        "Clearing a filter retains the exact source among duplicate display names");
    inspector.apply()->click();check(session.document()==linked(before.document,"text.family","alternate"),"Duplicate names cannot retarget a source ID");
    inspector.load(document);inspector.choose_field("text.locale");check(inspector.source_index("dependent")>=0&&inspector.source_index("external-chain")>=0,
        "Eligibility uses only the selected string field's dependency edges");

    inspector.load();inspector.choose_mode("link");inspector.sources()->addItem("Injected invalid source",999999);
    inspector.sources()->setCurrentIndex(inspector.sources()->count()-1);const Snapshot injected(session);inspector.force_apply();
    check(injected.same(session)&&inspector.status().startsWith("MISSING_REFERENCE"),"A forged picker index cannot supply an external or malformed source");
    inspector.field()->addItem("Unsupported","text.content");inspector.choose_field("text.content");inspector.force_apply();
    check(injected.same(session)&&inspector.status().startsWith("INVALID_TEXT_FIELD"),"Only family and locale are supported fields");

    const auto refuse=[&](std::vector<Id> targets,Document candidate,const char* code){
        inspector.targets=std::move(targets);inspector.load(std::move(candidate));const Snapshot snapshot(session);
        check(!inspector.apply()->isEnabled()&&inspector.state()=="Unavailable"&&inspector.status().startsWith(code),"An invalid selection is explicitly unavailable");
        inspector.force_apply();check(snapshot.same(session),"Invalid selection cannot partially mutate, revise or add History");
    };
    refuse({},fixture(),"INVALID_SELECTION");refuse({"first"},fixture(),"INVALID_SELECTION");
    refuse({"first","first"},fixture(),"INVALID_SELECTION");refuse(std::vector<Id>(1001,"first"),fixture(),"INVALID_SELECTION");
    refuse({"first","missing"},fixture(),"MISSING_OBJECT");refuse({"first","source-folder"},fixture(),"TYPE_MISMATCH");
    refuse({"first","foreign"},document,"INCOMPATIBLE_SELECTION");inspector.targets=text_targets;
}

void stale_context_retention_lifetime_and_bounds(){
    Inspector inspector;auto& session=inspector.host.session;inspector.draft_link("text.family","source");
    session.apply({Rename{"source","Later edit"}},session.revision());const Snapshot stale(session);inspector.apply()->click();
    check(stale.same(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"An intervening revision refuses all retained Text targets");
    inspector.cancel()->click();check(stale.same(session),"Cancel cannot overwrite a newer revision");
    inspector.load();inspector.draft_link("text.locale","source");inspector.host.session_id+="-other";const Snapshot identity(session);inspector.apply()->click();
    check(identity.same(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Same-document Session identity is guarded");
    inspector.load(linked(fixture(),"text.family","source"));inspector.choose_mode("unlink");auto replacement=fixture();replacement.id="other-document";
    session=Session(replacement);const Snapshot replaced(session);inspector.apply()->click();
    check(replaced.same(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Document identity guards an Unlink draft independently");
    inspector.load();inspector.draft_link("text.family","source");session.begin_gesture(session.revision());session.update_gesture({Rename{"source","Preview"}});
    const Snapshot gesture(session);const auto preview=session.preview_document();inspector.apply()->click();
    check(gesture.same(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),"An active gesture and its preview survive refused Apply");
    session.cancel_gesture();inspector.apply()->click();
    check(gesture.same(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A cancelled gesture invalidates the draft without changing History");
    inspector.load();session.begin_gesture(session.revision());inspector.rebuild();const Snapshot active(session);
    check(inspector.state()=="Unavailable"&&inspector.status().startsWith("GESTURE_ACTIVE")&&active.same(session),"Construction during a gesture refuses without cancelling it");
    session.cancel_gesture();inspector.load();inspector.draft_link("text.locale","source");session.begin_gesture(session.revision());session.cancel_gesture();
    const Snapshot cancelled(session);inspector.apply()->click();check(cancelled.same(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A no-history gesture also invalidates the frozen context");
    inspector.load();const Snapshot retained(session);inspector.draft_link("text.family","source");inspector.targets={"source","alternate"};inspector.apply()->click();
    check(session.document()==linked(retained.document,"text.family","source"),"Later caller selection changes cannot retarget captured IDs");inspector.targets=text_targets;

    auto thousand=empty_document("thousand","composition","artboard");thousand.objects.emplace("source",text_object("source","Source Family","en"));
    thousand.compositions.front().roots.push_back("source");std::vector<Id> targets;
    for(int index=0;index<1000;++index){const auto id="target-"+std::to_string(index);thousand.objects.emplace(id,text_object(id,"Arial","ja"));
        thousand.compositions.front().roots.push_back(id);targets.push_back(id);}
    inspector.targets=targets;inspector.load(thousand);const Snapshot maximum(session);inspector.draft_link("text.family","source");inspector.apply()->click();
    auto expected=maximum.document;for(const auto& id:targets)expected.objects.at(id).text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    check(session.document()==expected&&session.revision()==1&&session.history().states.size()==maximum.history.states.size()+1,"1000 retained Text targets link atomically in one Undo");
    inspector.undo();check(session.document()==maximum.document,"One Undo restores all 1000 exact Text sources");

    QTemporaryDir directory;QWidget parent;auto host=std::make_unique<Host>(directory.path());host->session=Session(fixture());
    QPointer<QWidget> panel=make_text_string_source_batch_controls(*host,text_targets,&parent);
    auto* mode=panel->findChild<QComboBox*>("text-string-source-batch-mode");auto* source=panel->findChild<QComboBox*>("text-string-source-batch-source");
    auto* apply=panel->findChild<QPushButton*>("text-string-source-batch-apply");auto* status=panel->findChild<QLabel*>("text-string-source-batch-status");
    mode->setCurrentIndex(mode->findData(QStringLiteral("link")));source->setCurrentIndex(0);apply->click();const Snapshot committed(host->session);
    apply->setEnabled(true);apply->click();check(committed.same(host->session),"Repeated Apply without Inspector reconstruction cannot create History");
    host.reset();mode->setCurrentIndex(mode->findData(QStringLiteral("unlink")));
    check(status->text().startsWith("SESSION_CONFLICT")&&!apply->isEnabled(),"Orphaned controls safely refuse after Host destruction");
}
}

int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    try{link_cancel_history_and_preservation();explicit_replace_and_independent_unlink();
        source_eligibility_search_and_selection_refusals();stale_context_retention_lifetime_and_bounds();
        std::cout<<"PASS "<<checks<<" Text family/locale source Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
