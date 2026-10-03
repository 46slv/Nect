#include "nect/io.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#ifndef NECT_INSTANCE_TEXT_CONTENT_MODEL_TESTS
#include "host.hpp"
#include "instance_text_content_control.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QWidget>
#include "instance_text_content_window_smoke.hpp"
#endif

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
const std::string exact_source="  Source\r\n日本語 🐈\xe2\x80\xa8line\xe2\x80\xa9tail\xc2\xa0\n";
Object text(const Id& id,const std::string& content){
    Object object;object.id=id;object.name="Same visible name";object.kind=Kind::text;
    object.text=default_text(id+"-text",content);return object;
}
Document fixture(bool empty=false,bool attached=false){
    auto document=empty_document("instance-text-doc","composition","source-board");
    document.compositions.front().artboards.push_back({"target-board","Target",100,0,64,64});
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;
    auto outside=text("outside","Outside");
    if(empty)root=text("source-root","Root-only Text must not be selectable");
    else{
        auto first=text("first-item","Retained first literal\r\n");
        first.text->content_driver=TextContentDriver{{"content-driver","","text.content"}};
        first.text->family="Arial";first.text->locale="en-US";
        first.text->weight_expression=Expression{"650",1};first.text->italic=true;
        first.text->font_features={{"kern",1,"whole_text"}};
        first.text->additional_axis_values={{"wdth",87.1234567890123}};
        auto second=text("second-item","Second  literal\n");
        auto driver=text("content-driver",attached?"Single line":exact_source);
        Object folder;folder.id="folder";folder.kind=Kind::group;folder.children={second.id};
        Object shape;shape.id="shape";shape.source=default_primitive("shape-source","nect.shape.rectangle");
        root.children={first.id,folder.id,driver.id,shape.id};
        if(attached){
            Object path;path.id="path";path.kind=Kind::path;
            Contour contour;contour.id="contour";Point begin;begin.id="begin";
            Point end;end.id="end";end.x.literal=2000;contour.points={begin,end};path.contours={contour};
            root.children.push_back(path.id);document.objects.emplace(path.id,path);
            first.text->path_attachment=TextPathAttachment{"path","contour","distance",0,0,false};
            second.text->content_driver=TextContentDriver{{first.id,"","text.content"}};
            second.text->path_attachment=TextPathAttachment{"path","contour","distance",0,0,false};
        }
        for(const auto& object:{first,folder,second,driver,shape})document.objects.emplace(object.id,object);
    }
    document.objects.emplace(root.id,root);document.objects.emplace(outside.id,outside);
    document.compositions.front().roots={root.id,outside.id};
    Session session(std::move(document));
    session.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target-board","template",Id{"content-instance"}}},
        DefinitionCommand{CreateInstance{"composition","","plain-instance","definition","Plain"}}},0);
    return session.document();
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()),native(encode(session.document())){}
    bool same(const Session& session)const{return document==session.document()&&revision==session.revision()&&history==session.history()&&native==encode(session.document());}
};
const std::map<Id,std::string>& overrides(const Session& session,const Id& instance="content-instance"){
    return session.document().objects.at(instance).instance->text_content_overrides;
}
Document locally_edited(Document document,const Id& source,const std::string& content,const Id& instance="content-instance"){
    document.objects.at(instance).instance->text_content_overrides.insert_or_assign(source,content);return document;
}
std::string occurrence_text(const Document& document,const Id& instance,const Id& source){
    const auto values=evaluate(document);
    const auto projection=project_definition_instances(document,"composition",values,evaluate_transforms(document,values));
    for(const auto& [proxy,owner]:projection.instance_owners)
        if(owner==instance&&projection.instance_sources.at(proxy)==source)return evaluate_text_content(*projection.document,proxy);
    throw std::runtime_error("Occurrence Text proxy missing");
}
void fixture_model_path(){
    Session session(fixture());const Snapshot original(session);
    check(evaluate_text_content(session.document(),"first-item")==exact_source,"Inherited source evaluates exact driven UTF-8, CRLF and paragraph bytes");
    session.apply({DefinitionCommand{SetInstanceTextContentOverride{"content-instance","first-item",exact_source}}},0);
    check(session.document()==locally_edited(original.document,"first-item",exact_source)&&session.revision()==1&&
        session.history().states.size()==original.history.states.size()+1,"Same-source local freeze changes only selected Instance map in one History step");
    check(occurrence_text(session.document(),"content-instance","first-item")==exact_source&&
        occurrence_text(session.document(),"plain-instance","first-item")==exact_source,"Portable Definition projection evaluates both local and inherited occurrence content without Text geometry");
    const auto frozen=encode(session.document());
    check(frozen.find("\"version\":\"0.85\"")!=std::string::npos&&decode(frozen)==session.document(),"Native085 cold decode retains exact local text and source drivers");
    session.undo(session.revision());check(session.document()==original.document&&encode(session.document())==original.native,"One Undo restores exact inheritance");
    session.redo(session.revision());check(encode(session.document())==frozen,"One Redo restores exact raw local freeze");
    session.apply({DefinitionCommand{SetInstanceTextContentOverride{"content-instance","second-item","Another local"}},
        DefinitionCommand{SetInstanceTextContentOverride{"content-instance","first-item",""}}},session.revision());
    check(overrides(session).contains("first-item")&&overrides(session).at("first-item").empty(),"Empty local content is distinct from no override");
    check(occurrence_text(session.document(),"content-instance","first-item").empty()&&
        occurrence_text(session.document(),"plain-instance","first-item")==exact_source,"Projected explicit empty override is occurrence-local");
    session.apply({DefinitionCommand{ResetInstanceTextContentOverride{"content-instance","first-item"}}},session.revision());
    check(overrides(session)==std::map<Id,std::string>{{"second-item","Another local"}}&&
        session.document().objects.at("first-item")==original.document.objects.at("first-item")&&
        session.document().objects.at("plain-instance")==original.document.objects.at("plain-instance"),"Selected reset preserves another local Text, source driver and sibling Instance");
    auto changed=*session.document().objects.at("content-driver").text;changed.content="Later source\n";
    session.apply({UpdateText{"content-driver",changed}},session.revision());
    check(evaluate_text_content(session.document(),"first-item")==changed.content&&overrides(session).at("second-item")=="Another local","Reset resumes current source without removing its driver or another local override");
    Session path(fixture(false,true));
    for(const auto* source:{"first-item","content-driver","second-item"}){
        const Snapshot before(path);bool refused=false;
        try{path.apply({DefinitionCommand{SetInstanceTextContentOverride{"content-instance",source,"Two\nlines"}}},path.revision());}
        catch(const Error& error){refused=error.code=="TEXT_PATH_MULTILINE_UNSUPPORTED";}
        check(refused&&before.same(path),"Direct and indirect occurrence Text-on-Path multiline refusal preserves complete authored state and History");
    }
    Session empty(fixture(true));check(empty.document().objects.at("source-root").children.empty(),"Root-only Text fixture excludes all descendants");
}

#ifndef NECT_INSTANCE_TEXT_CONTENT_MODEL_TESTS
using namespace nect::desktop;
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;Id instance="content-instance";
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_instance_text_content_controls(host,instance,&parent);}
    void load(Document document=fixture()){host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();}
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Instance Text control exists");return found;
    }
    QComboBox* sources(){return control<QComboBox>("instance-text-content-source");}
    QPlainTextEdit* editor(){return control<QPlainTextEdit>("instance-text-content-editor");}
    QPushButton* apply(){return control<QPushButton>("instance-text-content-apply");}
    QPushButton* cancel(){return control<QPushButton>("instance-text-content-cancel");}
    QPushButton* clear(){return control<QPushButton>("instance-text-content-clear");}
    QPushButton* reset(){return control<QPushButton>("instance-text-content-reset");}
    QString label(const char* name){return control<QLabel>(name)->text();}
    QString error(){return label("instance-text-content-error");}
    void select(const Id& source){const auto index=sources()->findData(QString::fromStdString(source));check(index>=0,"Stable source Text exists");sources()->setCurrentIndex(index);}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};
void primary_precision_and_reset(){
    Inspector i;auto& session=i.host.session;const Snapshot original(session);
    check(i.sources()->count()==3&&i.sources()->findData("source-root")<0&&i.sources()->findData("outside")<0&&
        i.sources()->findData("folder")<0&&i.sources()->findData("shape")<0,"Picker contains only descendant Text objects, excluding root, outside and non-Text");
    check(i.sources()->itemText(i.sources()->findData("first-item")).contains("first-item")&&
        i.sources()->itemText(i.sources()->findData("second-item")).contains("second-item"),"Duplicate visible names expose distinct stable Object IDs");
    i.select("first-item");const auto displayed=i.editor()->toPlainText();
    check(i.label("instance-text-content-source-state").endsWith(" · Link")&&
        i.label("instance-text-content-local-state")=="Local: Inherited in this instance"&&i.apply()->isEnabled()&&!i.reset()->isEnabled(),"Linked inherited source is labeled and explicit equal-source freeze is available");
    i.editor()->setPlainText("A draft\n  日本語 🐈  \n");check(original.same(session),"Exact multiline Unicode draft does not edit Document, native bytes, revision or History");
    i.cancel()->click();check(i.editor()->toPlainText()==displayed&&original.same(session),"Cancel restores initialized display with no edit");
    QPointer<QWidget> retired=i.controls;QPointer<QPushButton> retired_button=i.apply();i.apply()->click();
    check(!retired&&!retired_button&&session.document()==locally_edited(original.document,"first-item",exact_source)&&session.revision()==1,"Untouched Apply retains raw CRLF/paragraph/NBSP bytes and survives synchronous widget destruction");
    check(i.sources()->currentData().toString()=="first-item"&&i.reset()->isEnabled()&&
        i.label("instance-text-content-local-state")=="Local: Override","Selected stable ID survives rebuild and local override is explicit");
    const Snapshot frozen(session);i.apply()->click();check(frozen.same(session),"Identical existing local content is History-free");
    i.editor()->setPlainText("Away");i.editor()->setPlainText(displayed);i.apply()->click();
    check(frozen.same(session),"Moving away and returning to the Qt baseline preserves exact original bytes");
    i.clear()->click();check(i.editor()->toPlainText().isEmpty()&&frozen.same(session)&&i.reset()->isEnabled(),"Clear draft explicitly proposes empty content without resetting or editing");
    i.cancel()->click();check(i.editor()->toPlainText()==displayed&&frozen.same(session),"Cancel after Clear restores raw local baseline without History");
    i.clear()->click();i.apply()->click();
    check(session.document()==locally_edited(original.document,"first-item","")&&session.revision()==2&&i.reset()->isEnabled(),"Applying cleared draft commits an empty selected override in one step");
    i.undo();check(encode(session.document())==frozen.native,"Undo empty local content restores exact raw prior override");
    i.redo();check(overrides(session).at("first-item").empty(),"Redo restores explicit empty override");
    const std::string new_content="  Local\n日本語 🐈\n  ";i.editor()->setPlainText(QString::fromStdString(new_content));i.apply()->click();
    check(session.document()==locally_edited(original.document,"first-item",new_content),"Changed draft keeps exact Unicode, whitespace and trailing newline locally");
    auto changed=*session.document().objects.at("content-driver").text;changed.content="Later source\r\n";
    session.apply({UpdateText{"content-driver",changed}},session.revision());i.host.edited();
    check(i.label("instance-text-content-source-state").contains("Later source")&&i.editor()->toPlainText()==QString::fromStdString(new_content)&&
        overrides(session).at("first-item")==new_content,"Source updates display current inherited text without changing the retained local draft baseline");
    i.select("second-item");i.editor()->setPlainText("Other local");i.apply()->click();i.select("first-item");
    const Snapshot before_reset(session);i.reset()->click();
    check(!overrides(session).contains("first-item")&&overrides(session).at("second-item")=="Other local"&&
        i.label("instance-text-content-local-state")=="Local: Inherited in this instance"&&i.editor()->toPlainText()==QString("Later source\n"),"Use Source resets only selected override and resumes current driven inheritance");
    i.undo();check(session.document()==before_reset.document,"Selected-only reset is independently Undoable");
    check(session.document().objects.at("first-item")==original.document.objects.at("first-item")&&
        session.document().objects.at("plain-instance")==original.document.objects.at("plain-instance"),"All local edits preserve source content driver, rich Text settings and sibling Instance");
    check(decode(encode(session.document()))==session.document(),"Native085 decode preserves all exact local maps and authored sources");
    i.load();i.instance="plain-instance";i.rebuild();i.select("second-item");i.editor()->setPlainText("Plain local");i.apply()->click();
    check(overrides(session,"plain-instance").at("second-item")=="Plain local"&&overrides(session).empty(),"Ordinary Instance edits are independent from Template content");
}
void inherited_local_driver_baseline(){
    Inspector i;auto& session=i.host.session;const auto authored_first=session.document().objects.at("first-item");
    const std::string local_driver="Upstream local\r\n日本語\xe2\x80\xa9tail\n";
    session.apply({DefinitionCommand{SetInstanceTextContentOverride{"content-instance","content-driver",local_driver}}},session.revision());i.host.edited();
    i.select("first-item");QPlainTextEdit expected;expected.setPlainText(QString::fromStdString(local_driver));
    check(i.editor()->toPlainText()==expected.toPlainText()&&i.label("instance-text-content-source-state").contains("Source")&&
        i.label("instance-text-content-local-state")=="Local: Inherited in this instance","Inherited editor uses current occurrence content from an upstream local override, while source state remains source-only");
    i.apply()->click();check(overrides(session).at("first-item")==local_driver&&
        overrides(session).at("content-driver")==local_driver,"Untouched Apply freezes exact upstream local occurrence content rather than original source text");
    session.apply({DefinitionCommand{SetInstanceTextContentOverride{"content-instance","content-driver","New upstream local\r\n"}}},session.revision());i.host.edited();
    check(overrides(session).at("first-item")==local_driver&&i.editor()->toPlainText()==expected.toPlainText(),"A frozen descendant remains local after its upstream occurrence override changes");
    i.reset()->click();check(!overrides(session).contains("first-item")&&
        overrides(session).at("content-driver")=="New upstream local\r\n"&&i.editor()->toPlainText()=="New upstream local\n"&&
        session.document().objects.at("first-item")==authored_first,"Selected Use Source resumes current upstream local inheritance without removing either source driver or upstream override");
}
void interruption_and_identity_guards(){
    Inspector i;auto& session=i.host.session;
    i.editor()->setPlainText("Draft");session.apply({Rename{"outside","Later"}},session.revision());const Snapshot revised(session);i.apply()->click();
    check(revised.same(session)&&i.error().startsWith("REVISION_CONFLICT"),"Intervening revision refuses stale Apply");
    i.load();i.editor()->setPlainText("Draft");i.host.session_id+="-replacement";const Snapshot identity(session);i.apply()->click();
    check(identity.same(session)&&i.error().startsWith("SESSION_CONFLICT"),"Changed Session identity refuses stale Apply");
    i.load();i.editor()->setPlainText("Draft");auto document=fixture();document.id="different-document";session=Session(document);const Snapshot replacement(session);i.apply()->click();
    check(replacement.same(session)&&i.error().startsWith("SESSION_CONFLICT"),"Document ID protects same-revision Session replacement");
    i.load();i.editor()->setPlainText("Draft");const Snapshot gesture(session);session.begin_gesture(0);session.update_gesture({Rename{"outside","Preview"}});const auto preview=session.preview_document();i.apply()->click();
    check(gesture.same(session)&&session.preview_document()==preview&&i.error().startsWith("GESTURE_ACTIVE"),"Active gesture refuses Apply without touching preview or History");
    session.cancel_gesture();i.load();i.editor()->setPlainText("Draft");session.begin_gesture(0);session.cancel_gesture();const Snapshot canceled(session);i.apply()->click();
    check(canceled.same(session)&&i.error().startsWith("REVISION_CONFLICT"),"Canceled gesture generation invalidates the old draft context");
    i.load();i.apply()->click();session.apply({Rename{"outside","Stale reset"}},session.revision());const Snapshot stale_reset(session);i.reset()->click();
    check(stale_reset.same(session)&&i.error().startsWith("REVISION_CONFLICT"),"Use Source independently refuses stale context");
    i.load();i.editor()->setPlainText("Old source draft");i.select("second-item");const Snapshot retarget(session);i.apply()->click();
    check(session.document()==locally_edited(retarget.document,"second-item","Second  literal\n"),"Switching stable Text resets the draft instead of retargeting old unsaved content");
    // The Host may disappear while the Inspector remains alive.
    QTemporaryDir directory;QWidget parent;auto host=std::make_unique<Host>(directory.path());
    host->session=Session(locally_edited(fixture(),"first-item","Initial local"));
    QPointer<QWidget> controls=make_instance_text_content_controls(*host,"content-instance",&parent);
    auto* editor=controls->findChild<QPlainTextEdit*>("instance-text-content-editor");
    auto* apply=controls->findChild<QPushButton*>("instance-text-content-apply");
    auto* reset=controls->findChild<QPushButton*>("instance-text-content-reset");editor->setPlainText("Draft");
    check(apply->isEnabled()&&reset->isEnabled(),"Surviving Inspector offers both local actions before Host destruction");
    host.reset();apply->click();
    check(controls&&controls->findChild<QLabel*>("instance-text-content-error")->text().startsWith("SESSION_CONFLICT"),"Destroyed Host cannot be dereferenced by a surviving Inspector callback");
    controls->findChild<QLabel*>("instance-text-content-error")->clear();reset->click();
    check(controls&&controls->findChild<QLabel*>("instance-text-content-error")->text().startsWith("SESSION_CONFLICT"),"Destroyed Host is also refused by the surviving Reset callback");
    const QPointer<QPlainTextEdit> retired_editor(editor);delete controls.data();
    check(!controls&&!retired_editor,"Inspector destruction retires its draft editor and context-bound callbacks");
}
void stable_selection_empty_and_core_refusal(){
    Inspector i;i.select("second-item");auto document=fixture();document.objects.at("second-item").name="Renamed Text";
    auto& order=document.objects.at("source-root").children;std::swap(order[0],order[1]);i.load(document);
    check(i.sources()->currentData().toString()=="second-item"&&i.sources()->currentText().contains("Renamed Text"),"Rename and hierarchy reorder retain exact source ID rather than name or position");
    document=fixture();document.objects.at("folder").children.clear();document.objects.erase("second-item");i.load(document);
    check(i.sources()->currentData().toString()=="first-item","Removed retained Text falls back to first eligible descendant");
    check(i.parent.property("nect-instance-text-content-selection").toMap().size()==3,"Selection state is one bounded source-ID record");
    i.load(fixture(true));check(i.sources()->count()==0&&!i.sources()->isEnabled()&&!i.editor()->isEnabled()&&
        !i.apply()->isEnabled()&&!i.cancel()->isEnabled()&&!i.clear()->isEnabled()&&!i.reset()->isEnabled(),"Root-only Text creates safe empty disabled descendant controls");
    const Snapshot empty(i.host.session);i.apply()->click();i.reset()->click();check(empty.same(i.host.session),"Empty controls cannot create History");
    i.load();i.host.session.begin_gesture(0);i.rebuild();
    check(!i.editor()->isEnabled()&&!i.apply()->isEnabled()&&i.error().startsWith("GESTURE_ACTIVE"),"Factory refuses draft editing during an active gesture");
    i.host.session.cancel_gesture();i.rebuild();i.apply()->click();check(overrides(i.host.session).contains("first-item"),"Fresh controls resume after gesture cancellation");
    i.load(fixture(false,true));auto& session=i.host.session;
    for(const auto* source:{"first-item","content-driver","second-item"}){
        i.select(source);const Snapshot before(session);i.editor()->setPlainText("Two\nlines");i.apply()->click();
        check(before.same(session)&&i.error().startsWith("TEXT_PATH_MULTILINE_UNSUPPORTED")&&i.editor()->toPlainText()=="Two\nlines","Direct and indirect path multiline refusal is atomic and preserves the repairable draft");
        i.cancel()->click();check(before.same(session)&&i.editor()->toPlainText()=="Single line","Cancel after core refusal restores baseline without removing source drivers");
    }
    i.select("first-item");i.editor()->setPlainText("One line local");i.apply()->click();
    check(overrides(session).at("first-item")=="One line local"&&session.document().objects.at("first-item").text->path_attachment.has_value()&&
        session.document().objects.at("first-item").text->content_driver.has_value(),"Corrected single-line local Apply retains path attachment and source content driver");
}
#endif
}

#ifdef NECT_INSTANCE_TEXT_CONTENT_MODEL_TESTS
int main(){
    try{fixture_model_path();std::cout<<"PASS "<<checks<<" Instance Text fixture/model checks (Qt interaction NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv){
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication application(argc,argv);
    if(argc==3&&std::string(argv[1])=="--window-smoke"){
        try{application.setStyle("Fusion");instance_text_window_smoke::run(QString::fromLocal8Bit(argv[2]));return 0;}
        catch(const std::exception& error){std::cerr<<"FAIL Window smoke: "<<error.what()<<'\n';return 1;}
    }
    try{fixture_model_path();primary_precision_and_reset();inherited_local_driver_baseline();interruption_and_identity_guards();stable_selection_empty_and_core_refusal();
        std::cout<<"PASS "<<checks<<" Instance Text content Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#endif
