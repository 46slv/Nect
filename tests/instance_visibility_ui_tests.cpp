#include "host.hpp"
#include "instance_visibility_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QWidget>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
Document fixture(bool empty=false){
    auto d=empty_document("visibility-doc","composition","source-board");
    d.compositions.front().artboards.push_back({"target-board","Target",100,0,64,64});
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;root.visible=false;
    Object driver;driver.id="visibility-driver";driver.name="Driver";driver.visible=false;
    driver.source=default_primitive("driver-primitive","nect.shape.rectangle");
    d.objects.emplace(root.id,root);d.objects.emplace(driver.id,driver);
    d.compositions.front().roots={root.id,driver.id};
    if(!empty){
        Object first;first.id="first-item";first.name="Same name";
        first.source=default_primitive("first-primitive","nect.shape.rectangle");
        first.visibility_driver=Ref{driver.id,"","object.visible"};
        Object group;group.id="hidden-group";group.name="Hidden folder";group.kind=Kind::group;
        group.visible=false;group.children={"second-item"};
        Object second;second.id="second-item";second.name="Same name";
        second.source=default_primitive("second-primitive","nect.shape.rectangle");
        second.visibility_expression=Expression{"!ref(\"first-item\",\"\",\"object.visible\")",1};
        d.objects.at(root.id).children={first.id,group.id};
        d.objects.emplace(first.id,first);d.objects.emplace(group.id,group);d.objects.emplace(second.id,second);
    }
    Session session(std::move(d));
    session.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target-board","template",Id{"content-instance"}}},
        DefinitionCommand{CreateInstance{"composition","","plain-instance","definition","Plain"}}},0);
    return session.document();
}
struct Snapshot{
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& s):document(s.document()),revision(s.revision()),history(s.history()),native(encode(s.document())){}
    bool unchanged(const Session& s)const{return document==s.document()&&revision==s.revision()&&history==s.history()&&native==encode(s.document());}
};
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;Id instance="content-instance";
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch available");
        host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_instance_visibility_controls(host,instance,&parent);}
    void load(Document d=fixture()){
        host.session=Session(std::move(d));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));
        check(found!=nullptr,"Visibility control exists");return found;
    }
    QComboBox* selector(){return control<QComboBox>("instance-visibility-source");}
    void select(const Id& id){const auto index=selector()->findData(QString::fromStdString(id));check(index>=0,"Stable SourceItemID available");selector()->setCurrentIndex(index);}
    QString label(const char* name){return control<QLabel>(name)->text();}
    void click(const char* name){control<QPushButton>(name)->click();}
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};
const std::map<Id,bool>& overrides(const Inspector& i){return i.host.session.document().objects.at(i.instance).instance->visibility_overrides;}

void primary_path(){
    Inspector i;const auto original=i.host.session.document();const auto before=encode(original);
    auto* source=i.selector();check(source->count()==3&&source->findData("source-root")<0&&source->findData("visibility-driver")<0,
        "Only stable Definition descendants are selectable; root and unrelated objects excluded");
    check(source->itemText(source->findData("first-item")).contains("first-item")&&
        source->itemText(source->findData("second-item")).contains("second-item"),"Duplicate names display distinct stable IDs");
    i.select("first-item");
    check(i.label("instance-visibility-source-state")=="Source: Hidden · Link"&&
        i.label("instance-visibility-local-state")=="Local: Use Source", "Source state uses evaluated typed link rather than authored true literal");
    QPointer<QWidget> old=i.controls;QPointer<QPushButton> old_button=i.control<QPushButton>("instance-visibility-hide");
    i.click("instance-visibility-hide");
    check(!old&&!old_button&&overrides(i)==std::map<Id,bool>{{"first-item",false}}&&i.host.session.revision()==1,
        "Hide is one canonical edit and tolerates synchronous destruction of its widgets");
    check(i.label("instance-visibility-local-state")=="Local: Hidden"&&i.control<QPushButton>("instance-visibility-reset")->isEnabled(),
        "Local Hide and Use Source availability are displayed after rebuild");
    check(i.host.session.document().objects.at("first-item")==original.objects.at("first-item")&&
        i.host.session.document().objects.at("plain-instance")==original.objects.at("plain-instance"),
        "Template content edit does not modify the source or sibling Instance");
    const auto hidden=encode(i.host.session.document());i.undo();
    check(encode(i.host.session.document())==before,"One Undo restores exact source inheritance");
    i.redo();check(encode(i.host.session.document())==hidden,"Redo restores the local override");
    i.host.session.apply({SetVisibility{"visibility-driver",true}},i.host.session.revision());i.host.edited();
    check(i.label("instance-visibility-source-state")=="Source: Visible · Link"&&
        i.label("instance-visibility-local-state")=="Local: Hidden", "Later source edits remain visible in state without replacing the local override");
    i.click("instance-visibility-reset");check(overrides(i).empty()&&
        i.label("instance-visibility-local-state")=="Local: Use Source", "Use Source resumes the current source value");
    i.undo();check(overrides(i).at("first-item")==false,"Use Source is independently Undoable");

    i.load();i.select("second-item");
    check(i.label("instance-visibility-source-state")=="Source: Visible · Expression", "Expression source state uses evaluated own visibility");
    i.click("instance-visibility-show");
    check(overrides(i).at("second-item")&&i.selector()->currentData().toString()=="second-item",
        "Show authors an explicit local true and keeps the selected stable descendant after rebuild");
    check(!i.host.session.document().objects.at("hidden-group").visible&&
        !i.host.session.document().objects.at("source-root").visible&&
        i.control<QPushButton>("instance-visibility-show")->toolTip().contains("Hidden ancestors"),
        "Showing a child leaves ancestor visibility unchanged and discloses suppression");
    i.click("instance-visibility-reset");check(overrides(i).empty()&&i.selector()->currentData().toString()=="second-item",
        "Reset retains selected item without a second authored value store");
    i.instance="plain-instance";i.rebuild();i.select("first-item");i.click("instance-visibility-show");
    check(overrides(i).at("first-item")&&i.host.session.document().objects.at("content-instance").instance->visibility_overrides.empty(),
        "The same factory supports an ordinary Instance independently of Template content");
}
void guards_and_empty(){
    Inspector i;
    i.host.session.apply({SetVisibility{"visibility-driver",true}},0);Snapshot stale(i.host.session);
    i.click("instance-visibility-hide");check(stale.unchanged(i.host.session)&&
        i.label("instance-visibility-error").contains("changed; refresh"),"Stale revision cannot mutate document or History");
    i.load();Snapshot identity(i.host.session);i.host.session_id+="-new-session";
    i.click("instance-visibility-hide");check(identity.unchanged(i.host.session)&&
        i.label("instance-visibility-error").contains("another document"),"Stale Host Session is refused");
    i.load();i.host.session=Session(fixture());
    auto replacement=i.host.session.document();replacement.id="other-document";i.host.session=Session(std::move(replacement));Snapshot document(i.host.session);
    i.click("instance-visibility-hide");check(document.unchanged(i.host.session)&&
        i.label("instance-visibility-error").contains("another document"),"Document identity guard rejects same-revision replacement");
    i.load();i.host.session.begin_gesture(0);i.host.session.update_gesture({SetVisibility{"visibility-driver",true}});
    Snapshot gesture(i.host.session);const auto preview=i.host.session.preview_document();
    i.click("instance-visibility-hide");check(gesture.unchanged(i.host.session)&&i.host.session.preview_document()==preview&&
        i.label("instance-visibility-error").contains("Finish or cancel"),"Active gesture and preview survive a rejected control action");
    i.host.session.cancel_gesture();i.click("instance-visibility-hide");check(gesture.unchanged(i.host.session)&&
        i.label("instance-visibility-error").contains("edit context changed"),"Cancelled gesture invalidates the old edit context");
    i.rebuild();i.click("instance-visibility-hide");check(overrides(i).at("first-item")==false,
        "Fresh controls work after gesture cancellation");
    i.load();i.select("second-item");auto changed=fixture();
    changed.objects.at("hidden-group").children.clear();changed.objects.erase("second-item");i.load(std::move(changed));
    check(i.selector()->currentData().toString()=="first-item","Removed saved item falls back to the first eligible descendant");
    i.load(fixture(true));check(i.selector()->count()==0&&!i.selector()->isEnabled()&&
        !i.control<QPushButton>("instance-visibility-hide")->isEnabled()&&
        !i.control<QPushButton>("instance-visibility-show")->isEnabled()&&
        !i.control<QPushButton>("instance-visibility-reset")->isEnabled(),"Empty Definitions produce safe disabled controls");
    Snapshot empty(i.host.session);i.click("instance-visibility-hide");i.click("instance-visibility-reset");
    check(empty.unchanged(i.host.session),"Empty item actions cannot create edits");
    const auto selection=i.parent.property("nect-instance-visibility-selection").toMap();
    check(selection.size()==3,"Selection presentation storage is one bounded record");
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try{primary_path();guards_and_empty();std::cout<<"PASS "<<checks<<" Instance visibility control checks\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
