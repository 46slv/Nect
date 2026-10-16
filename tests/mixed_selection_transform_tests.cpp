#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);QApplication::processEvents();}
bool reachable(QWidget* widget){return widget&&widget->isVisible()&&widget->visibleRegion().contains(widget->rect());}
struct Snapshot {
    Document document;std::string native;std::uint64_t revision,generation;HistoryInfo history;bool gesture;
    explicit Snapshot(const Session& s):document(s.document()),native(encode(document)),revision(s.revision()),generation(s.gesture_generation()),history(s.history()),gesture(s.gesture_active()){}
    void unchanged(const Session& s)const{
        check(s.document()==document&&s.preview_document()==document&&encode(s.document())==native,
            "Transform navigation preserves complete source/native/preview");
        check(s.revision()==revision&&s.history()==history&&s.gesture_generation()==generation&&s.gesture_active()==gesture,
            "Transform navigation preserves revision/history/gesture ownership");
    }
};
Document fixture(){
    auto d=empty_document("mixed-document","composition","board");
    d.compositions.front().artboards.front().width=640;d.compositions.front().artboards.front().height=480;
    Object path;path.id="path";path.name="Editable Path";
    Point a;a.id="kept-first";a.x.literal=350;a.y.literal=280;
    Point b;b.id="kept-last";b.x.literal=410;b.y.literal=300;
    path.contours={{"kept-contour",false,{a,b}}};path.stack={default_operation("kept-stroke","nect.paint.stroke")};
    d.objects.emplace(path.id,path);d.compositions.front().roots={path.id};
    auto rectangle=default_primitive("retained-source","nect.shape.rectangle");
    rectangle.parameters.at("center_x").literal=240;rectangle.parameters.at("center_y").literal=200;
    rectangle.parameters.at("width").literal=80;rectangle.parameters.at("height").literal=60;
    auto circle=default_primitive("unrelated-source","nect.shape.circle");
    circle.parameters.at("center_x").literal=70;circle.parameters.at("center_y").literal=390;circle.parameters.at("radius").literal=20;
    Session s(d);s.apply({CreatePrimitive{"composition",{},"rectangle","Retained Rectangle",rectangle},
        CreateText{"composition",{},"text","Editable Text",default_text("kept-text-source","Mixed ABC")},
        Set{{"text",{},"transform.tx"},300},Set{{"text",{},"transform.ty"},340},
        CreatePrimitive{"composition",{},"unrelated","Unrelated Circle",circle}},s.revision());return s.document();
}
void evidence(QWidget& widget,const QString& name){const auto dir=qEnvironmentVariable("NECT_MIXED_TRANSFORM_EVIDENCE");
    if(!dir.isEmpty())check(widget.grab().save(dir+"/"+name+".png"),"Actual edited-source UI evidence saved");}
void tool(Window& w){check(w.findChild<QToolButton*>("tool-direct-selection")->isChecked()&&w.canvas->direct_selection_mode(),
    "Contextual Transform preserves explicit Direct Selection Tool");}
void history(Window& w,const QString& label){
    for(auto* action:w.findChildren<QAction*>())if(action->text()==label){action->trigger();events();return;}
    throw std::runtime_error("Existing History action missing");
}
void group_follow_context(QTemporaryDir& scratch,QSettings& preferences){
    Window w(scratch.filePath("follow-recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
    w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1000,650);w.show();events();
    auto document=fixture();document.objects.at("rectangle").anchor[0].literal=240;
    document.objects.at("rectangle").anchor[1].literal=200;
    // Deform samples the retained geometry's full local extent along the contour.
    document.objects.at("path").contours.front().points.front().x.literal=0;
    document.objects.at("path").contours.front().points.back().x.literal=1000;
    document.objects.at("path").contours.front().points.back().y.literal=280;
    Session setup(document);
    setup.apply({GroupContiguous{"composition",{}, {"rectangle","text"},"group","Retained followers"}},setup.revision());
    GroupPathFollow relation;relation.id="retained-relation";relation.path="path";relation.contour="kept-contour";
    relation.items={{"rectangle",{5,2,true}},{"text",{15,-3,false}}};
    setup.apply({GroupPathFollowCommand{AttachGroupPathFollow{"group",relation}}},setup.revision());
    auto& s=w.host.session;s=Session(setup.document());w.host.edited();w.canvas->set_selection("group");events();
    QApplication::setActiveWindow(&w);events();
    const auto field=[&](const char* name){
        for(auto* input:w.findChildren<QDoubleSpinBox*>(name))if(input->isVisible())return input;
        throw std::runtime_error("Visible Group Path Follow child field missing");
    };
    const auto draft=[&](const char* name,const char* text){
        QPointer<QDoubleSpinBox> input=field(name);auto* scroll=w.findChild<QScrollArea*>("inspector-scroll");
        check(scroll!=nullptr,"Production Group Path Follow Inspector scroll exists");scroll->ensureWidgetVisible(input);events();
        check(input&&reachable(input)&&input->isEnabled(),"Actual child numeric field is enabled and fully reachable");
        input->setFocus();events();check(input&&input->hasFocus(),"Actual child numeric field owns focus");
        input->selectAll();QTest::keyClicks(input,text);events();
        auto* editor=input->findChild<QLineEdit*>();
        check(input&&input->hasFocus()&&editor&&editor->text()==text,
            "Actual focused child field receives exact unfinished keyboard draft");return input;
    };
    const Snapshot initial(s);const auto selection=w.canvas->selections();const auto session_id=w.host.session_id;
    auto old=draft("group-path-follow-distance-rectangle","25");initial.unchanged(s);
    auto item=relation.items.at("rectangle");item.distance=35;
    s.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","rectangle",item}}},s.revision());const Snapshot incoming(s);
    check(old&&old->hasFocus()&&old->findChild<QLineEdit*>()->text()=="25",
        "External canonical item edit arrives while the old numeric draft is focused");
    w.host.edited();events();
    std::cout<<"external Group child refresh distance="<<s.document().objects.at("group").path_follow->items.at("rectangle").distance
        <<" revision="<<s.revision()<<" expected="<<incoming.revision<<std::endl;
    incoming.unchanged(s);
    check(w.host.session_id==session_id&&w.canvas->selections()==selection&&field("group-path-follow-distance-rectangle")->value()==35,
        "Replacement child Inspector displays external value and retains Session/selection");
    history(w,"Undo");check(s.document()==initial.document&&encode(s.document())==initial.native,
        "External child edit Undo restores complete original relation/source");
    history(w,"Redo");check(s.document()==incoming.document&&encode(s.document())==incoming.native,
        "External child edit Redo restores complete incoming relation/source");
    const Snapshot before_offset(s);auto old_offset=draft("group-path-follow-item-offset-rectangle","9");before_offset.unchanged(s);
    item.normal_offset=6;
    s.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","rectangle",item}}},s.revision());const Snapshot incoming_offset(s);
    check(old_offset&&old_offset->hasFocus()&&old_offset->findChild<QLineEdit*>()->text()=="9",
        "External offset arrives while distinct unfinished numeric draft is focused");
    w.host.edited();events();incoming_offset.unchanged(s);
    check(field("group-path-follow-item-offset-rectangle")->value()==6,"Replacement child offset shows external canonical value");
    const auto commit=[&](const char* name,const char* text,bool distance,double value){
        const Snapshot before(s);auto input=draft(name,text);before.unchanged(s);
        auto expected=before.document.objects.at("group").path_follow->items.at("rectangle");
        if(distance)expected.distance=value;else expected.normal_offset=value;
        Session oracle(before.document);oracle.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","rectangle",expected}}},oracle.revision());
        QTest::keyClick(input,Qt::Key_Return);events();
        check(s.document()==oracle.document()&&s.preview_document()==oracle.document()&&encode(s.document())==encode(oracle.document())&&
            s.revision()==before.revision+1&&s.history().states.size()==before.history.states.size()+1,
            "Child numeric Return commits exactly one complete canonical item transaction");
        const auto& after=s.document();auto group=after.objects.at("group");group.path_follow=before.document.objects.at("group").path_follow;
        auto untouched=*after.objects.at("group").path_follow;
        untouched.items.at("rectangle")=before.document.objects.at("group").path_follow->items.at("rectangle");
        check(group==before.document.objects.at("group")&&after.objects.at("rectangle")==before.document.objects.at("rectangle")&&
            after.objects.at("text")==before.document.objects.at("text")&&after.objects.at("path")==before.document.objects.at("path")&&
            after.objects.at("unrelated")==before.document.objects.at("unrelated")&&
            after.objects.at("group").path_follow->id=="retained-relation"&&
            untouched==*before.document.objects.at("group").path_follow,
            "Child numeric edit retains hierarchy/relation identity/Anchor/geometry/Text/source Path and every unrelated item");
        const Snapshot applied(s);history(w,"Undo");check(s.document()==before.document&&encode(s.document())==before.native,
            "Child numeric Undo restores complete previous source");history(w,"Redo");
        check(s.document()==applied.document&&encode(s.document())==applied.native,"Child numeric Redo restores complete canonical source");
    };
    commit("group-path-follow-distance-rectangle","40",true,40);
    commit("group-path-follow-item-offset-rectangle","-4",false,-4);
    // The changed numeric branches are shared by rigid placement and retained deformation.
    auto deform=*s.document().objects.at("group").path_follow;deform.items.erase("text");deform.mode="deform";
    s.apply({GroupPathFollowCommand{UpdateGroupPathFollow{"group",deform}}},s.revision());w.host.edited();events();
    const Snapshot before_deform(s);auto old_deform=draft("group-path-follow-distance-rectangle","45");before_deform.unchanged(s);
    auto deform_item=deform.items.at("rectangle");deform_item.distance=30;
    s.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","rectangle",deform_item}}},s.revision());const Snapshot incoming_deform(s);
    check(old_deform&&old_deform->hasFocus(),"Retained Deform draft remains focused before external edit refresh");
    w.host.edited();events();incoming_deform.unchanged(s);
    check(s.document().objects.at("group").path_follow->mode=="deform","Child numeric refresh retains Deform mode");
    commit("group-path-follow-item-offset-rectangle","7",false,7);
    const auto native=scratch.filePath("follow.nect");const Snapshot saved(s);w.host.save(native);events();saved.unchanged(s);
    w.host.open(native);events();check(s.document()==saved.document&&s.preview_document()==saved.document&&encode(s.document())==saved.native&&
        s.revision()==0&&s.history().states.size()==1,"Same Window native reopen retains complete relation/children/Anchor/source");
    const Snapshot before_gesture(s);auto gesture_draft=draft("group-path-follow-distance-rectangle","50");before_gesture.unchanged(s);
    auto preview_item=s.document().objects.at("group").path_follow->items.at("rectangle");preview_item.distance=20;
    s.begin_gesture(s.revision());s.update_gesture({GroupPathFollowCommand{SetGroupPathFollowItem{"group","rectangle",preview_item}}});
    const auto preview=s.preview_document();const auto generation=s.gesture_generation();const auto gesture_history=s.history();
    check(gesture_draft&&gesture_draft->hasFocus()&&preview!=s.document(),"Distinct external item preview begins while old draft is focused");
    w.host.edited();events();
    const auto preview_unchanged=[&]{
        check(s.document()==before_gesture.document&&encode(s.document())==before_gesture.native&&s.preview_document()==preview&&
            s.revision()==before_gesture.revision&&s.history()==gesture_history&&s.gesture_generation()==generation&&s.gesture_active(),
            "Child numeric input preserves complete external preview/source/history/gesture ownership");
    };
    preview_unchanged();auto active_draft=draft("group-path-follow-item-offset-rectangle","12");
    QTest::keyClick(active_draft,Qt::Key_Return);events();preview_unchanged();
    s.cancel_gesture();
    check(s.document()==saved.document&&s.preview_document()==saved.document&&s.revision()==before_gesture.revision,
        "Canonical preview cancellation itself retains complete native-restored source");
    std::cout<<"cancel before child refresh offset="<<s.document().objects.at("group").path_follow->items.at("rectangle").normal_offset
        <<" revision="<<s.revision()<<" generation="<<s.gesture_generation()<<std::endl;
    w.host.edited();events();
    std::cout<<"cancel after child refresh offset="<<s.document().objects.at("group").path_follow->items.at("rectangle").normal_offset
        <<" revision="<<s.revision()<<" expected="<<before_gesture.revision<<std::endl;
    check(s.document()==saved.document&&s.preview_document()==saved.document,"Owned preview cancellation retains complete native-restored source");
    commit("group-path-follow-distance-rectangle","32",true,32);
    check(preferences.value("unrelated")=="preserved"&&preferences.value("workspace/tools/textCreationDirection")=="vertical",
        "Group numeric editing/native paths retain unrelated workspace preferences");
}
void position_context(QTemporaryDir& scratch,QSettings& preferences){
    Window w(scratch.filePath("position-recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
    w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1000,650);w.show();events();
    auto document=fixture();auto& rectangle=document.objects.at("rectangle");
    rectangle.anchor[0].literal=240;rectangle.anchor[1].literal=200;rectangle.transform_parent="text";
    auto& parent=document.objects.at("text");
    parent.transform[0].literal=0;parent.transform[1].literal=1;
    parent.transform[2].literal=-1;parent.transform[3].literal=0;
    auto& s=w.host.session;s=Session(document);w.host.edited();w.canvas->set_selection("rectangle");events();
    QApplication::setActiveWindow(&w);events();
    const auto field=[&](const char* name){
        for(auto* input:w.findChildren<QLineEdit*>(name))if(input->isVisible())return input;
        throw std::runtime_error("Visible Position field missing");
    };
    const auto draft=[&](const char* name,const char* text){
        QPointer<QLineEdit> input=field(name);auto* scroll=w.findChild<QScrollArea*>("inspector-scroll");
        check(scroll!=nullptr,"Production Position Inspector scroll exists");scroll->ensureWidgetVisible(input);events();
        check(input&&reachable(input),"Actual Position field is fully reachable");
        input->setFocus();events();check(input&&input->hasFocus(),"Actual Position field owns focus");
        input->selectAll();QTest::keyClicks(input,text);events();
        check(input&&input->hasFocus()&&input->isModified()&&input->text()==text,
            "Actual focused Position receives exact unfinished modified keyboard draft");return input;
    };
    const Snapshot initial(s);const auto session_id=w.host.session_id;const auto selection=w.canvas->selections();
    auto old=draft("transform-position-x","700");initial.unchanged(s);
    s.apply({SetPosition{"rectangle",400,300}},s.revision());const Snapshot incoming(s);
    check(old&&old->hasFocus()&&old->text()=="700"&&old->isModified(),
        "External canonical SetPosition arrives before refresh while old draft remains focused");
    w.host.edited();events();
    const auto position=[&]{const auto values=evaluate(s.document());return map_point(evaluate_transforms(s.document(),values).at("rectangle").local,
        {values.at({"rectangle","","transform.anchor_x"}),values.at({"rectangle","","transform.anchor_y"})});};
    std::cout<<"external Position refresh local="<<position().x<<','<<position().y
        <<" revision="<<s.revision()<<" expected="<<incoming.revision<<std::endl;
    incoming.unchanged(s);
    check(w.host.session_id==session_id&&w.canvas->selections()==selection,
        "Position refresh keeps exact Session and stable selection");
    check(field("transform-position-x")->text().toDouble()==400&&field("transform-position-y")->text().toDouble()==300,
        "Replacement Inspector displays incoming effective-parent Position");
    history(w,"Undo");check(s.document()==initial.document&&encode(s.document())==initial.native,
        "External Position Undo restores complete original source");
    history(w,"Redo");check(s.document()==incoming.document&&encode(s.document())==incoming.native,
        "External Position Redo restores complete incoming source");
    const Snapshot y_draft(s);auto old_y=draft("transform-position-y","+=10");y_draft.unchanged(s);
    s.apply({SetPosition{"rectangle",400,350}},s.revision());const Snapshot incoming_y(s);
    check(old_y&&old_y->hasFocus()&&old_y->text()=="+=10"&&old_y->isModified(),
        "External Y Position arrives while unfinished relative draft is still focused");
    w.host.edited();events();incoming_y.unchanged(s);
    check(field("transform-position-y")->text().toDouble()==350,"Replacement Y field shows external canonical Position");
    const auto commit=[&](const char* name,const char* text,double x,double y){
        const Snapshot before(s);auto input=draft(name,text);before.unchanged(s);
        Session oracle(before.document);oracle.apply({SetPosition{"rectangle",x,y}},oracle.revision());
        QTest::keyClick(input,Qt::Key_Return);events();
        check(s.document()==oracle.document()&&s.preview_document()==oracle.document()&&
            s.revision()==before.revision+1&&s.history().states.size()==before.history.states.size()+1,
            "Position Return commits one complete canonical SetPosition transaction");
        check(std::abs(position().x-x)<1e-9&&std::abs(position().y-y)<1e-9,
            "Absolute and relative Position preserve effective-parent coordinates");
        const auto world=map_point(evaluate_transforms(s.document(),evaluate(s.document())).at("rectangle").world,{240,200});
        check(std::abs(world.x-(300-y))<1e-9&&std::abs(world.y-(340+x))<1e-9,
            "Independent rotated-parent coordinates retain the authored Anchor");
        auto source=s.document().objects.at("rectangle");source.transform=before.document.objects.at("rectangle").transform;
        check(source==before.document.objects.at("rectangle")&&s.document().objects.at("path")==before.document.objects.at("path")&&
            s.document().objects.at("text")==before.document.objects.at("text")&&s.document().objects.at("unrelated")==before.document.objects.at("unrelated"),
            "Position preserves retained geometry/Anchor/stable IDs and every unrelated source");
        const Snapshot applied(s);history(w,"Undo");check(s.document()==before.document&&encode(s.document())==before.native,
            "Position Undo restores complete previous source");history(w,"Redo");
        check(s.document()==applied.document&&encode(s.document())==applied.native,"Position Redo restores complete canonical source");
    };
    commit("transform-position-x","425",425,350);commit("transform-position-y","-=75",425,275);
    const auto native=scratch.filePath("position.nect");const Snapshot saved(s);w.host.save(native);events();saved.unchanged(s);
    w.host.open(native);events();check(s.document()==saved.document&&s.preview_document()==saved.document&&encode(s.document())==saved.native&&
        s.revision()==0&&s.history().states.size()==1,"Same Window native reopen retains complete Position/Anchor/parent/source");
    const Snapshot before_gesture(s);auto gesture_draft=draft("transform-position-x","+=10");before_gesture.unchanged(s);
    s.begin_gesture(s.revision());s.update_gesture({SetPosition{"rectangle",500,350}});
    const auto preview=s.preview_document();const auto generation=s.gesture_generation();const auto gesture_history=s.history();
    check(gesture_draft&&gesture_draft->hasFocus()&&preview!=s.document(),"Distinct canonical preview begins while Position draft is focused");
    w.host.edited();events();
    check(s.document()==before_gesture.document&&encode(s.document())==before_gesture.native&&s.preview_document()==preview&&
        s.revision()==before_gesture.revision&&s.history()==gesture_history&&s.gesture_generation()==generation&&s.gesture_active(),
        "Stale Position refresh preserves complete external preview and gesture ownership");
    auto active_draft=draft("transform-position-y","600");QTest::keyClick(active_draft,Qt::Key_Return);events();
    check(s.document()==before_gesture.document&&encode(s.document())==before_gesture.native&&s.preview_document()==preview&&
        s.revision()==before_gesture.revision&&s.history()==gesture_history&&s.gesture_generation()==generation&&s.gesture_active(),
        "Current Position Return during an external gesture preserves complete source/preview/history/ownership");
    s.cancel_gesture();w.host.edited();events();
    check(s.document()==saved.document&&s.preview_document()==saved.document,"Owned preview cancellation retains native-restored complete source");
}
void run(QTemporaryDir& scratch,QSettings& preferences){
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
    w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1000,650);w.show();w.activateWindow();events();
    auto& s=w.host.session;s=Session(fixture());w.host.edited();events();
    QTest::mouseClick(w.findChild<QToolButton*>("tool-direct-selection"),Qt::LeftButton);events();
    const std::vector<Canvas::Selection> selected{{"rectangle",{}},{"path",{}},{"text",{}}};
    w.canvas->set_selections(selected);events();w.canvas->fit_artboard();events();const Snapshot before(s);
    const auto selection=w.canvas->selections();const auto targets=w.canvas->selected_objects();tool(w);
    auto dialog=[&](const std::function<void(QDialog&)>& edit){
        auto* button=w.findChild<QPushButton*>("selection-transform-open");auto* scroll=w.findChild<QScrollArea*>("inspector-scroll");
        check(button&&scroll&&button->isEnabled(),"Actual mixed whole-object Inspector exposes contextual Transform entry");
        const Snapshot navigation(s);scroll->ensureWidgetVisible(button);events();
        check(reachable(button),"Contextual Transform entry is fully reachable in narrow real Window");navigation.unchanged(s);
        bool seen=false;std::exception_ptr failure;
        QTimer::singleShot(100,&w,[&]{
            auto* form=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!form)return;
            try{
                check(form->objectName()=="selection-transform-dialog","Pointer opens the actual canonical Transform dialog");seen=true;
                navigation.unchanged(s);tool(w);edit(*form);
            }catch(...){failure=std::current_exception();form->reject();}
        });
        QTest::mouseClick(button,Qt::LeftButton);events();if(failure)std::rethrow_exception(failure);
        check(seen,"Real contextual dialog input was delivered exactly once");
    };
    auto number=[](QDialog& form,const char* name,const char* text){
        auto* input=form.findChild<QDoubleSpinBox*>(name);check(reachable(input)&&input->isEnabled(),"Real dialog numeric source control is fully reachable");
        input->setFocus();input->selectAll();QTest::keyClicks(input,text);QTest::keyClick(input,Qt::Key_Tab);events();return input;
    };
    auto finish=[](QDialog& form,QDialogButtonBox::StandardButton choice){
        auto* button=form.findChild<QDialogButtonBox*>()->button(choice);check(reachable(button),"Actual dialog completion button is reachable");
        QTest::mouseClick(button,Qt::LeftButton);
    };
    dialog([&](QDialog& form){number(form,"selection-rotation","45");finish(form,QDialogButtonBox::Cancel);});before.unchanged(s);
    dialog([&](QDialog& form){finish(form,QDialogButtonBox::Ok);});before.unchanged(s);
    evidence(w,"mixed-entry");
    const auto image_before=Canvas::render_artboard(s.document(),"composition","board",w.devicePixelRatioF(),false);
    Session oracle(before.document);oracle.apply({TransformObjects{targets,90,1.25,0.75,std::array<double,2>{320,240}}},oracle.revision());
    dialog([&](QDialog& form){
        check(number(form,"selection-rotation","90")->value()==90,"Actual rotation keyboard input is exact");
        auto* linked=form.findChild<QCheckBox*>("selection-scale-linked");check(reachable(linked)&&linked->isChecked(),"Scale starts linked with visible control");
        QTest::mouseClick(linked,Qt::LeftButton,Qt::NoModifier,QPoint(8,linked->height()/2));events();
        check(!linked->isChecked(),"Real pointer explicitly permits unequal axes");
        check(number(form,"selection-scale-x","125")->value()==125&&number(form,"selection-scale-y","75")->value()==75,
            "Actual scale keyboard input retains independent exact axes");
        auto* pivot=form.findChild<QComboBox*>("selection-pivot-mode");check(reachable(pivot),"Actual shared-pivot control is reachable");
        pivot->setFocus();QTest::keyClick(pivot,Qt::Key_Down);events();check(pivot->currentIndex()==1,"Keyboard chooses explicit custom pivot");
        check(number(form,"selection-pivot-x","320")->value()==320&&number(form,"selection-pivot-y","240")->value()==240,
            "Actual custom pivot fields are enabled and receive exact canvas coordinates");
        before.unchanged(s);evidence(form,"mixed-transform-dialog");finish(form,QDialogButtonBox::Ok);
    });
    check(s.document()==oracle.document()&&s.revision()==before.revision+1&&s.history().current_id!=before.history.current_id,
        "Real Apply commits exactly one canonical TransformObjects edit for the complete Document");
    check(w.canvas->selections()==selection,"Transform preserves complete stable whole-object selection");tool(w);
    const auto old_transforms=evaluate_transforms(before.document,evaluate(before.document));
    const auto transforms=evaluate_transforms(s.document(),evaluate(s.document()));
    for(const auto& id:targets){
        auto actual=s.document().objects.at(id);actual.transform=before.document.objects.at(id).transform;
        check(actual==before.document.objects.at(id),"Only canonical affine source changes; complete geometry/Text/style/Anchors/IDs remain");
        for(const auto point:{Vec2{0,0},Vec2{240,200},Vec2{350,280}}){
            const auto old=map_point(old_transforms.at(id).world,point),now=map_point(transforms.at(id).world,point);
            check(std::abs(now.x-(500-0.75*old.y))<1e-9&&std::abs(now.y-(-160+1.25*old.x))<1e-9,
                "Independent quarter-turn/unequal-scale arithmetic qualifies actual world coordinates");
        }
    }
    check(s.document().objects.at("unrelated")==before.document.objects.at("unrelated"),"Unselected complete retained Circle is unchanged");
    const auto image=Canvas::render_artboard(s.document(),"composition","board",w.devicePixelRatioF(),false);
    check(image!=image_before,"Actual production-rendered mixed artwork moves after Transform");
    // Rectangle center(240,200) maps to(350,140); top-right(280,170) maps to(372.5,190).
    const auto scale=w.devicePixelRatioF();int painted=0;
    for(int y=qRound(188*scale);y<=qRound(192*scale);++y)for(int x=qRound(370*scale);x<=qRound(375*scale);++x)
        if(image.pixelColor(x,y).alpha()>128)++painted;
    check(painted>0,"Production renderer paints the retained Rectangle at the independently calculated transformed corner");
    std::cout<<"mixed DPR="<<scale<<" rectangle center=350,140 corner=372.5,190 painted="<<painted<<std::endl;
    evidence(w,"mixed-transformed");const Snapshot applied(s);
    history(w,"Undo");check(s.document()==before.document&&s.history().current_id==before.history.current_id,
        "Existing Undo restores the complete mixed source before one Transform");tool(w);
    history(w,"Redo");check(s.document()==applied.document&&s.history().current_id==applied.history.current_id,
        "Existing Redo restores complete exact mixed affine source");tool(w);
    const auto native=scratch.filePath("mixed-transform.nect");const Snapshot saved(s);w.host.save(native);events();saved.unchanged(s);
    w.host.open(native);events();check(s.document()==oracle.document(),"Same Window native reopen retains all mixed source and IDs");
    Window reopened(scratch.filePath("reopened"),std::make_unique<FolderLibrary>(preferences),&preferences);
    reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1000,650);reopened.show();reopened.host.open(native);events();
    check(reopened.host.session.document()==oracle.document(),"New Window native reopen restores complete mixed transforms and retained source");
    check(Canvas::render_artboard(reopened.host.session.document(),"composition","board",scale,false)==image,
        "Native-restored production pixels are exact for the complete mixed artwork");
    check(preferences.value("unrelated")=="preserved"&&preferences.value("workspace/tools/textCreationDirection")=="vertical",
        "Contextual Transform/native paths preserve unrelated and Text preferences");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");QSettings preferences(scratch.filePath("preferences.ini"),QSettings::IniFormat);
        preferences.setValue("unrelated","preserved");preferences.setValue("workspace/tools/textCreationDirection","vertical");
        if(app.arguments().contains("--group-follow-context")){group_follow_context(scratch,preferences);
            std::cout<<"PASS "<<checks<<" actual Window Group Path Follow child context checks; physical OS input NOT_RUN\n";return 0;}
        if(app.arguments().contains("--position-context")){position_context(scratch,preferences);
            std::cout<<"PASS "<<checks<<" actual Window Position context checks; physical OS input NOT_RUN\n";return 0;}
        run(scratch,preferences);
        std::cout<<"PASS "<<checks<<" real mixed-selection contextual Transform checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}}
