#include "nect/io.hpp"
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#ifndef NECT_INSTANCE_COLOR_MODEL_TESTS
#include "host.hpp"
#include "instance_color_control.hpp"
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantMap>
#include <QWidget>
#include <functional>
#include <memory>
#endif

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
const Ref first_color=operation_ref("first-item","first-fill","color");
const Ref other_color=operation_ref("first-item","other-fill","color");
const Ref second_color=operation_ref("second-item","second-fill","color");
const Ref driver_color=operation_ref("color-driver","driver-fill","color");
ColorValue precise_color(){ColorValue color;color.rgba={.123456789012345,.43210987654321,.7654321098765,.87654321098765};return color;}
Gradient gradient(const Id& id){
    Gradient value;value.id=id;GradientStop first;first.id=id+"-start";
    GradientStop last;last.id=id+"-end";last.offset.literal=1;last.rgba[0].literal=.8;value.stops={first,last};return value;
}
Object rectangle(const Id& id,const Id& fill){
    Object object;object.id=id;object.name="Same visible name";
    object.source=default_primitive(id+"-rectangle","nect.shape.rectangle");
    object.stack={default_operation(fill,"nect.paint.fill")};return object;
}
Document fixture(bool empty=false){
    auto document=empty_document("instance-color-doc","composition","source-board");
    document.compositions.front().artboards.push_back({"target-board","Target",100,0,64,64});
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;
    auto outside=rectangle("outside","outside-fill");
    if(empty)root=rectangle("source-root","root-fill"); // Root paint must never become a selectable descendant.
    else{
        auto first=rectangle("first-item","first-fill");
        auto driver=rectangle("color-driver","driver-fill");
        const auto precise=precise_color();constexpr std::array<const char*,4> channels{"r","g","b","a"};
        for(std::size_t i=0;i<4;++i){
            driver.stack[0].parameters.at(channels[i]).literal=precise.rgba[i];
            first.stack[0].parameters.at(channels[i]).literal=.01;
            first.stack[0].parameters.at(channels[i]).binding=Binding{operation_ref(driver.id,"driver-fill",channels[i]),1,0};
        }
        auto other=default_operation("other-fill","nect.paint.fill");other.parameters.at("r").expression=Expression{"0.25",1};
        auto gradient_fill=default_operation("inactive-gradient-fill","nect.paint.fill");gradient_fill.gradient=gradient("inactive-gradient");
        gradient_fill.gradient->enabled=false;
        first.stack.insert(first.stack.begin(),default_operation("offset","nect.shape.offset"));
        first.stack.push_back(other);first.stack.push_back(gradient_fill);
        Object folder;folder.id="folder";folder.name="Folder";folder.kind=Kind::group;folder.children={"second-item"};
        auto second=rectangle("second-item","second-fill");second.stack[0].parameters.at("r").expression=Expression{"0.75",1};
        second.stack[0].parameters.at("g").binding=Binding{operation_ref(driver.id,"driver-fill","g"),1,0};
        auto stroke=rectangle("stroke-only","discarded");stroke.stack={default_operation("stroke","nect.paint.stroke")};
        auto gradients=rectangle("gradient-only","gradient-fill");gradients.stack[0].gradient=gradient("gradient");
        root.children={first.id,folder.id,driver.id,stroke.id,gradients.id};
        for(const auto& object:{first,folder,second,driver,stroke,gradients})document.objects.emplace(object.id,object);
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
const std::map<Ref,ColorValue>& overrides(const Session& session,const Id& instance="content-instance"){
    return session.document().objects.at(instance).instance->color_overrides;
}
Document locally_colored(Document document,const Ref& target,const ColorValue& color,const Id& instance="content-instance"){
    document.objects.at(instance).instance->color_overrides.insert_or_assign(target,color);return document;
}

void fixture_model_path(){
    Session session(fixture());const Snapshot original(session);
    check(color_value(session.document(),first_color,evaluate(session.document()))==precise_color(),
        "Driven source fixture evaluates exact baseline rather than its retained literal channels");
    session.apply({DefinitionCommand{SetInstanceColorOverride{"content-instance",first_color,precise_color()}}},0);
    const auto expected=locally_colored(original.document,first_color,precise_color());
    check(session.document()==expected&&session.revision()==1&&session.history().states.size()==original.history.states.size()+1,
        "Canonical same-source local freeze changes only the selected Instance map in one History step");
    check(decode(encode(session.document()))==expected,"Native084 cold readback preserves exact local doubles and source links");
    const auto frozen=encode(session.document());session.undo(session.revision());
    check(session.document()==original.document&&encode(session.document())==original.native,"One Undo restores exact inheritance");
    session.redo(session.revision());check(encode(session.document())==frozen,"One Redo restores exact local freeze");
    ColorValue other;other.rgba={.9,.8,.7,.6};
    session.apply({DefinitionCommand{SetInstanceColorOverride{"content-instance",other_color,other}},
        DefinitionCommand{ResetInstanceColorOverride{"content-instance",first_color}}},session.revision());
    check(overrides(session)==std::map<Ref,ColorValue>{{other_color,other}}&&session.document().objects.at("first-item")==original.document.objects.at("first-item")&&
        session.document().objects.at("plain-instance")==original.document.objects.at("plain-instance"),
        "Selected Fill reset preserves another local Fill, authored source and sibling Instance");
    auto changed=precise_color();changed.rgba[0]=.5;session.apply({SetColor{driver_color,changed}},session.revision());
    check(color_value(session.document(),first_color,evaluate(session.document()))==changed&&overrides(session).at(other_color)==other,
        "Inheritance resumes the current driven source without disturbing another local color");
    Session empty(fixture(true));check(empty.document().definitions.at("definition").root=="source-root"&&
        empty.document().objects.at("source-root").children.empty(),"Empty descendant fixture has root-only paint for exclusion coverage");
}

#ifndef NECT_INSTANCE_COLOR_MODEL_TESTS
using namespace nect::desktop;
struct Inspector{
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> controls;Id instance="content-instance";
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_instance_color_controls(host,instance,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Instance Fill color control exists");return found;
    }
    QComboBox* objects(){return control<QComboBox>("instance-color-source-object");}
    QComboBox* fills(){return control<QComboBox>("instance-color-source-fill");}
    QPushButton* choose(){return control<QPushButton>("instance-color-choose");}
    QPushButton* reset(){return control<QPushButton>("instance-color-reset");}
    QString label(const char* name){return control<QLabel>(name)->text();}
    QString error(){return label("instance-color-error");}
    void select(const Id& object,const Id& fill){
        const auto object_index=objects()->findData(QString::fromStdString(object));check(object_index>=0,"Stable source Object exists");
        objects()->setCurrentIndex(object_index);const auto fill_index=fills()->findData(QString::fromStdString(fill));
        check(fill_index>=0,"Stable source Fill exists");fills()->setCurrentIndex(fill_index);
    }
    void dialog(bool accept,const QColor* changed=nullptr,const std::function<void(QColorDialog&)>& during={}){
        bool seen=false;std::exception_ptr failure;const QPointer<QWidget> owner(controls);
        QTimer::singleShot(0,owner.data(),[&]{
            auto* dialog=qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
            if(!dialog){QApplication::closeAllWindows();return;}
            seen=dialog->objectName()=="instance-color-dialog";const QPointer<QColorDialog> safe(dialog);
            try{
                if(changed)dialog->setCurrentColor(*changed);
                if(during)during(*dialog);
            }catch(...){failure=std::current_exception();if(safe)safe->reject();return;}
            if(safe){if(accept)safe->accept();else safe->reject();}
        });
        choose()->click();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        check(seen,"Local color opens the actual Qt picker");
        if(failure)std::rethrow_exception(failure);
    }
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};
ColorValue selected_color(const QColor& color){ColorValue value;value.rgba={color.redF(),color.greenF(),color.blueF(),color.alphaF()};return value;}
bool displays(const QColor& displayed,const ColorValue& exact){
    const auto value=selected_color(displayed);
    for(std::size_t i=0;i<4;++i)if(std::abs(value.rgba[i]-exact.rgba[i])>1./255)return false;
    return true;
}

void primary_precision_and_reset(){
    Inspector i;auto& session=i.host.session;const Snapshot original(session);
    check(i.objects()->count()==3&&i.objects()->findData("source-root")<0&&i.objects()->findData("outside")<0&&
        i.objects()->findData("stroke-only")<0&&i.objects()->findData("gradient-only")<0,
        "Selectors contain only descendant Objects with solid Fill, excluding root, unrelated, Stroke and gradients");
    check(i.objects()->itemText(i.objects()->findData("first-item")).contains("first-item")&&
        i.objects()->itemText(i.objects()->findData("second-item")).contains("second-item"),
        "Duplicate visible names remain distinguishable by stable Object IDs");
    i.select("first-item","first-fill");
    check(i.fills()->count()==2&&i.fills()->findData("inactive-gradient-fill")<0&&
        i.fills()->itemText(0).contains("first-fill")&&i.fills()->itemText(1).contains("other-fill"),
        "Multiple Fill operations expose exact stable IDs, including stack positions, without inactive gradient fallback");
    check(i.label("instance-color-source-state").contains("0.123456789012345")&&
        i.label("instance-color-source-state").endsWith(" · Link")&&i.label("instance-color-local-state")=="Local: Use Source"&&
        !i.reset()->isEnabled(),"Source displays exact evaluated linked color and local inheritance");
    const QColor chosen(12,34,56,78);
    i.dialog(false,&chosen);check(original.same(session),"Cancel preserves document, native bytes, revision and History");
    QPointer<QWidget> retired=i.controls;QPointer<QPushButton> retired_button=i.choose();
    i.dialog(true,nullptr,[&](QColorDialog& dialog){
        check(displays(dialog.currentColor(),precise_color()),"Picker starts from evaluated linked source color at dialog display precision");
    });
    check(!retired&&!retired_button&&session.document()==locally_colored(original.document,first_color,precise_color())&&session.revision()==1,
        "Untouched OK freezes exact original doubles locally and survives synchronous rebuild destruction");
    check(i.label("instance-color-local-state").contains("0.123456789012345")&&i.reset()->isEnabled()&&
        i.objects()->currentData().toString()=="first-item"&&i.fills()->currentData().toString()=="first-fill",
        "Exact local state and both stable selectors survive rebuild");
    const Snapshot frozen(session);i.dialog(true);check(frozen.same(session),"Accepting an identical existing override is History-free");
    i.dialog(false,&chosen);check(frozen.same(session),"Cancel after a local draft change preserves an existing override exactly");
    i.dialog(true,nullptr,[&](QColorDialog& dialog){
        const auto initial=dialog.currentColor();dialog.setCurrentColor(chosen);dialog.setCurrentColor(initial);
    });
    check(frozen.same(session),"Moving away and returning to initialized QColor preserves exact local doubles");
    i.dialog(true,&chosen,[&](QColorDialog& dialog){
        check(displays(dialog.currentColor(),selected_color(chosen)),"Explicit changed QColor is the picker draft");
    });
    check(session.document()==locally_colored(original.document,first_color,selected_color(chosen))&&session.revision()==2,
        "Changed QColor authors only the selected literal occurrence override in one edit");
    const auto committed=encode(session.document());i.undo();check(encode(session.document())==frozen.native,"Undo restores exact previous local color");
    i.redo();check(encode(session.document())==committed,"Redo restores exact changed local color");
    auto changed=precise_color();changed.rgba[0]=.5;session.apply({SetColor{driver_color,changed}},session.revision());i.host.edited();
    check(i.label("instance-color-source-state").contains("RGBA 0.5,")&&overrides(session).at(first_color)==selected_color(chosen),
        "Source updates are displayed without replacing a local override or its source driver");
    const Snapshot local_baseline(session);i.dialog(false,nullptr,[&](QColorDialog& dialog){
        check(displays(dialog.currentColor(),selected_color(chosen))&&!displays(dialog.currentColor(),changed),
            "Existing override initializes the picker from exact local map rather than the later source value");
    });
    check(local_baseline.same(session),"Inspecting then canceling the existing local baseline has no History effect");
    i.select("first-item","other-fill");check(i.label("instance-color-source-state").endsWith(" · Expression"),"Expression color is evaluated and labeled");
    i.dialog(true);const auto retained_other=overrides(session).at(other_color);i.select("first-item","first-fill");
    const Snapshot before_reset(session);i.reset()->click();
    check(!overrides(session).contains(first_color)&&overrides(session).at(other_color)==retained_other&&
        i.label("instance-color-local-state")=="Local: Use Source"&&i.label("instance-color-source-state").contains("RGBA 0.5,"),
        "Use Source resets only selected Fill and resumes its current source while keeping another local Fill");
    i.undo();check(session.document()==before_reset.document,"Selected reset is independently Undoable");
    check(session.document().objects.at("first-item")==original.document.objects.at("first-item")&&
        session.document().objects.at("plain-instance")==original.document.objects.at("plain-instance"),
        "All picker edits and reset preserve source bindings/expressions and a sibling Instance");
    check(decode(encode(session.document()))==session.document(),"Native084 readback preserves exact local maps and authored sources");

    i.load();i.select("second-item","second-fill");
    check(i.label("instance-color-source-state").contains(" · Link · Expression"),"Mixed driven channels disclose both Link and Expression");
    i.instance="plain-instance";i.rebuild();i.select("second-item","second-fill");i.dialog(true,&chosen);
    check(overrides(session,"plain-instance").at(second_color)==selected_color(chosen)&&overrides(session).empty(),
        "Factory edits an ordinary Instance independently of Template content");
}

void interruption_and_identity_guards(){
    Inspector i;auto& session=i.host.session;const QColor chosen(99,88,77,66);std::unique_ptr<Snapshot> intervening;
    i.dialog(true,&chosen,[&](QColorDialog&){session.apply({Rename{"outside","Later"}},session.revision());intervening=std::make_unique<Snapshot>(session);});
    check(intervening->same(session)&&i.error().startsWith("REVISION_CONFLICT"),"Intervening revision refuses stale modal acceptance");
    i.load();const Snapshot identity(session);
    i.dialog(true,&chosen,[&](QColorDialog&){i.host.session_id+="-new-session";});
    check(identity.same(session)&&i.error().startsWith("SESSION_CONFLICT"),"Changed Session identity refuses stale acceptance");
    i.load();std::unique_ptr<Snapshot> replacement;
    i.dialog(true,&chosen,[&](QColorDialog&){auto document=fixture();document.id="other-document";session=Session(document);replacement=std::make_unique<Snapshot>(session);});
    check(replacement->same(session)&&i.error().startsWith("SESSION_CONFLICT"),"Document identity protects same-revision replacement");
    i.load();const Snapshot gesture(session);Document preview;
    i.dialog(true,&chosen,[&](QColorDialog&){session.begin_gesture(0);session.update_gesture({Rename{"outside","Preview"}});preview=session.preview_document();});
    check(gesture.same(session)&&session.preview_document()==preview&&i.error().startsWith("GESTURE_ACTIVE"),
        "Active gesture refuses stale acceptance without changing its preview or History");
    session.cancel_gesture();i.load();const Snapshot canceled(session);
    i.dialog(true,&chosen,[&](QColorDialog&){session.begin_gesture(0);session.cancel_gesture();});
    check(canceled.same(session)&&i.error().startsWith("REVISION_CONFLICT"),"Canceled gesture generation invalidates retained modal context");
    i.load();const Snapshot retarget(session);
    i.dialog(true,&chosen,[&](QColorDialog&){i.select("first-item","other-fill");i.choose()->click();i.reset()->click();});
    check(session.document()==locally_colored(retarget.document,first_color,selected_color(chosen)),
        "Selection changes and repeated clicks during picker cannot retarget the captured Fill or create nested edits");
    i.load();const Snapshot destroyed(session);
    i.dialog(true,&chosen,[&](QColorDialog&){delete i.controls.data();});
    check(!i.controls&&destroyed.same(session),"Destroyed Inspector/dialog cannot commit or touch retired widgets");
    i.rebuild();i.select("first-item","first-fill");i.dialog(true);
    session.apply({Rename{"outside","Stale reset"}},session.revision());const Snapshot stale_reset(session);i.reset()->click();
    check(stale_reset.same(session)&&i.error().startsWith("REVISION_CONFLICT"),"Use Source also refuses a stale context");

    // Host can die independently of a still-open Inspector/dialog.
    QTemporaryDir directory;QWidget parent;auto host=std::make_unique<Host>(directory.path());host->session=Session(fixture());
    QPointer<QWidget> controls=make_instance_color_controls(*host,"content-instance",&parent);
    auto* choose=controls->findChild<QPushButton*>("instance-color-choose");
    bool seen=false;QTimer::singleShot(0,controls.data(),[&]{
        auto* dialog=qobject_cast<QColorDialog*>(QApplication::activeModalWidget());if(!dialog){QApplication::closeAllWindows();return;}
        seen=true;host.reset();dialog->accept();
    });
    choose->click();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    check(seen&&controls,"Host lifetime guard survives acceptance without dereferencing a destroyed Host");
}

void linked_occurrence_picker_baseline(){
    Inspector i;auto& session=i.host.session;const auto original=session.document();
    ColorValue local;local.rgba={.91,.82,.73,.64};
    session.apply({DefinitionCommand{SetInstanceColorOverride{"content-instance",driver_color,local}}},session.revision());
    i.host.edited();i.select("first-item","first-fill");
    check(!overrides(session).contains(first_color)&&
        color_value(session.document(),first_color,evaluate(session.document()))==precise_color(),
        "Linked Fill has no own override and its authored source still evaluates the original color");
    check(i.label("instance-color-source-state").contains("0.123456789012345")&&
        i.choose()->accessibleDescription().contains("0.91"),
        "Source label stays source-only while the picker swatch describes the visible occurrence");
    i.dialog(true,nullptr,[&](QColorDialog& dialog){
        check(displays(dialog.currentColor(),local),"Linked Fill picker starts from another descendant's occurrence-local color");
    });
    check(overrides(session).at(first_color)==local&&overrides(session).at(driver_color)==local&&
        session.document().objects.at("first-item")==original.objects.at("first-item")&&
        session.document().objects.at("color-driver")==original.objects.at("color-driver"),
        "Unchanged OK freezes the exact visible occurrence color and preserves both authored source Objects");
}

void stable_selection_and_empty(){
    Inspector i;i.select("first-item","other-fill");auto document=fixture();
    document.objects.at("first-item").name="Renamed source";auto& stack=document.objects.at("first-item").stack;
    std::swap(stack[1],stack[2]);i.load(document);
    check(i.objects()->currentData().toString()=="first-item"&&i.fills()->currentData().toString()=="other-fill"&&
        i.fills()->currentText().contains("stack 2"),"Rename and stack reorder retain exact Object/Fill IDs rather than names or positions");
    document=fixture();document.objects.at("first-item").stack.erase(document.objects.at("first-item").stack.begin()+2);i.load(document);
    check(i.objects()->currentData().toString()=="first-item"&&i.fills()->currentData().toString()=="first-fill",
        "Removed retained Fill falls back to another eligible Fill without retargeting by slot");
    i.select("second-item","second-fill");document=fixture();document.objects.at("folder").children.clear();document.objects.erase("second-item");i.load(document);
    check(i.objects()->currentData().toString()=="first-item","Removed retained Object falls back to first eligible descendant");
    const auto selection=i.parent.property("nect-instance-color-selection").toMap();check(selection.size()==4,"Selection storage is one bounded Object/Fill presentation record");
    i.load(fixture(true));check(i.objects()->count()==0&&i.fills()->count()==0&&!i.objects()->isEnabled()&&!i.fills()->isEnabled()&&
        !i.choose()->isEnabled()&&!i.reset()->isEnabled(),"Root-only Definition produces safe empty disabled descendant controls");
    const Snapshot empty(i.host.session);i.choose()->click();i.reset()->click();check(empty.same(i.host.session),"Empty selection cannot create History");
    i.load();i.host.session.begin_gesture(0);i.rebuild();
    check(!i.choose()->isEnabled()&&!i.reset()->isEnabled()&&i.error().startsWith("GESTURE_ACTIVE"),"Factory refuses editing while gesture is active");
    i.host.session.cancel_gesture();i.rebuild();i.dialog(true);
    check(overrides(i.host.session).contains(first_color),"Fresh controls resume after gesture cancellation");
}
#endif
}

#ifdef NECT_INSTANCE_COLOR_MODEL_TESTS
int main(){
    try{fixture_model_path();std::cout<<"PASS "<<checks<<" Instance color fixture/model checks (Qt interaction NOT_RUN)\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication application(argc,argv);
    try{fixture_model_path();primary_precision_and_reset();linked_occurrence_picker_baseline();interruption_and_identity_guards();stable_selection_and_empty();
        std::cout<<"PASS "<<checks<<" Instance solid Fill color Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
#endif
