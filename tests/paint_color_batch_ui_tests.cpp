#include "host.hpp"
#include "paint_color_batch_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QColorDialog>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
const Ref first_color=operation_ref("first","first-paint-id","color");
const Ref second_color=operation_ref("second","other-paint-id","color");
const std::vector<Ref> paint_targets{first_color,second_color};
ColorValue precise_color(){ColorValue color;color.rgba={.123456789012345,.43210987654321,.7654321098765,.87654321098765};return color;}
Gradient gradient(const Id& id){
    Gradient value;value.id=id;GradientStop first;first.id=id+"-start";
    GradientStop last;last.id=id+"-end";last.offset.literal=1;last.rgba[0].literal=.8;value.stops={first,last};return value;
}
Document fixture(bool mixed=false,const std::string& kind="nect.paint.fill"){
    auto document=empty_document("paint-document","composition","artboard");
    for(const auto* id:{"first","second"}){
        Object object;object.id=id;object.name="Same visible name";object.kind=Kind::path;
        object.source=default_primitive(object.id+"-source","nect.shape.rectangle");
        object.transform[4].literal=object.id=="first"?11.123456789:211.987654321;
        object.compositing.opacity.literal=.375;object.compositing.blend="multiply";
        auto offset=default_operation(object.id+"-offset","nect.shape.offset");offset.parameters.at("amount").literal=3.25;
        auto paint=default_operation(object.id=="first"?"first-paint-id":"other-paint-id",kind);
        paint.composite=object.id=="first"?"above":"below";
        if(kind=="nect.paint.fill")paint.fill_rule=object.id=="first"?"evenodd":"nonzero";
        else if(object.id=="second"){
            paint.version=2;paint.line_join="round";paint.line_cap="square";paint.parameters.emplace("miter_limit",Scalar{7.25,{},{}});
            paint.parameters.at("width").expression=Expression{"3.5",1};
        }
        auto color=precise_color();if(mixed&&object.id=="second")color.rgba[0]+=1e-10;
        for(std::size_t i=0;i<4;++i)paint.parameters.at(std::array<const char*,4>{"r","g","b","a"}[i]).literal=color.rgba[i];
        auto unrelated=default_operation(object.id+"-unrelated","nect.paint.fill");
        unrelated.gradient=gradient(object.id+"-unrelated-gradient");
        unrelated.parameters.at("r").expression=Expression{"0.25",1};
        object.stack={offset,paint,unrelated};document.objects.emplace(object.id,object);
    }
    document.objects.at("first").source->parameters.at("width").binding=Binding{{"second","","generator.width"},1.25,0};
    document.objects.at("first").stack[1].enabled_driver=operation_ref("second","other-paint-id","enabled");
    document.compositions.front().roots={"first","second"};return document;
}
Document recolored(Document document,const ColorValue& value){
    for(const auto& target:paint_targets){
        for(std::size_t i=0;i<4;++i){
            auto& object=document.objects.at(target.object);
            for(auto& operation:object.stack)if(target==operation_ref(object.id,operation.id,"color"))
                operation.parameters.at(std::array<const char*,4>{"r","g","b","a"}[i]).literal=value.rgba[i];
        }
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
    std::vector<Ref> targets=paint_targets;
    Inspector():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.changed=[this]{rebuild();};load();
    }
    ~Inspector(){host.changed={};}
    void rebuild(){delete controls.data();controls=make_paint_color_batch_controls(host,targets,&parent);}
    void load(Document document=fixture()){
        host.session=Session(std::move(document));host.session_id+="-replacement";rebuild();
    }
    template<class T>T* control(const char* name){
        auto* found=controls->findChild<T*>(QString::fromLatin1(name));check(found!=nullptr,"Paint color control exists");return found;
    }
    QPushButton* choose(){return control<QPushButton>("paint-color-batch-choose");}
    QString state(){return control<QLabel>("paint-color-batch-state")->text();}
    QString status(){return control<QLabel>("paint-color-batch-status")->text();}
    void dialog(bool accept,const QColor* changed=nullptr,const std::function<void(QColorDialog&)>& during={}){
        bool seen=false;const QPointer<QWidget> owner(controls);
        QTimer::singleShot(0,owner.data(),[&]{
            auto* dialog=qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
            if(!dialog){QApplication::closeAllWindows();return;}
            seen=dialog->objectName()=="paint-color-batch-dialog";const QPointer<QColorDialog> safe(dialog);
            if(changed)dialog->setCurrentColor(*changed);
            if(during)during(*dialog);
            if(safe){if(accept)safe->accept();else safe->reject();}
        });
        choose()->click();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        check(seen,"The batch opens the actual Qt color dialog");
    }
    void undo(){host.session.undo(host.session.revision());host.edited();}
    void redo(){host.session.redo(host.session.revision());host.edited();}
};

void primary_mixed_precision_cancel(){
    Inspector inspector;auto& session=inspector.host.session;const Snapshot original(session);
    check(inspector.state().startsWith("Shared: RGBA")&&inspector.state().contains("0.123456789012345")&&inspector.choose()->isEnabled(),
        "Exact equal retained colors display Shared without losing authored precision");
    const QColor chosen(12,34,56,78);inspector.dialog(false,&chosen);
    check(original.same(session),"Cancel after a draft color change preserves exact document, native bytes, revision and History");
    inspector.dialog(true);check(original.same(session),"Untouched acceptance preserves exact ColorValue despite QColor display quantization");
    inspector.dialog(true,nullptr,[&](QColorDialog& dialog){
        const auto baseline=dialog.currentColor();dialog.setCurrentColor(chosen);dialog.setCurrentColor(baseline);
    });
    check(original.same(session),"Returning the dialog to its initialized displayed baseline is history-free");
    QPointer<QWidget> retired=inspector.controls;QPointer<QPushButton> retired_button=inspector.choose();
    inspector.dialog(true,&chosen);ColorValue value;value.rgba={chosen.redF(),chosen.greenF(),chosen.blueF(),chosen.alphaF()};
    const auto expected=recolored(original.document,value);
    check(!retired&&!retired_button&&session.document()==expected&&session.revision()==original.revision+1&&
        session.history().states.size()==original.history.states.size()+1,
        "One canonical batch changes only the four target channels and survives synchronous Inspector destruction");
    check(decode(encode(session.document()))==expected,
        "Native readback preserves IDs, geometry, opacity, stack order/options, unrelated gradient and drivers");
    const auto committed=encode(session.document());inspector.undo();
    check(session.document()==original.document&&encode(session.document())==original.native&&!session.can_undo(),
        "One Undo restores exact per-target native color values and all untouched state");
    inspector.redo();check(encode(session.document())==committed,"One Redo restores the whole batch");

    inspector.load(fixture(true));const Snapshot mixed(session);
    check(inspector.state()=="Mixed","Colors differing below QColor precision are still exactly Mixed");
    inspector.dialog(false);check(mixed.same(session),"Cancel keeps Mixed values without History");
    inspector.dialog(true);check(session.document()==recolored(mixed.document,precise_color())&&session.revision()==1,
        "Explicit untouched Mixed acceptance assigns the first retained exact color to every target");
    inspector.undo();check(session.document()==mixed.document&&inspector.state()=="Mixed","One Undo restores distinct exact Mixed colors");

    inspector.load(fixture(false,"nect.paint.stroke"));const Snapshot strokes(session);inspector.dialog(true,&chosen);
    check(session.document()==recolored(strokes.document,value),
        "Stroke v1/v2 colors share one batch while width expression, cap, join and miter options stay exact");
}

void refusal_and_stale_context(){
    Inspector inspector;auto& session=inspector.host.session;
    const auto refuse=[&](Document document,std::vector<Ref> targets,const char* code){
        inspector.targets=std::move(targets);inspector.load(std::move(document));const Snapshot before(session);
        check(inspector.state()=="Unavailable"&&!inspector.choose()->isEnabled()&&inspector.status().startsWith(QString::fromLatin1(code)),
            "Unsupported selection refuses visibly without filtering any target");
        inspector.choose()->click();check(before.same(session),"Refused selection leaves native state, revision and History intact");
    };
    auto driven=fixture();driven.objects.at("second").stack[1].parameters.at("g").binding=Binding{{"first","","op.first-paint-id.g"},1,0};
    refuse(driven,paint_targets,"DRIVEN_PROPERTY");
    driven=fixture();driven.objects.at("second").stack[1].parameters.at("a").expression=Expression{"0.5",1};
    refuse(driven,paint_targets,"DRIVEN_PROPERTY");
    auto gradient_document=fixture();gradient_document.objects.at("second").stack[1].gradient=gradient("target-gradient");
    gradient_document.objects.at("second").stack[1].gradient->enabled=false;
    refuse(gradient_document,paint_targets,"UNSUPPORTED_GRADIENT");
    auto incompatible=fixture();incompatible.objects.at("second").stack[1]=default_operation("other-paint-id","nect.paint.stroke");
    refuse(incompatible,paint_targets,"INCOMPATIBLE_PAINT");
    auto different_slot=fixture();std::swap(different_slot.objects.at("second").stack[0],different_slot.objects.at("second").stack[1]);
    refuse(different_slot,paint_targets,"INCOMPATIBLE_PAINT");
    refuse(fixture(),{first_color,first_color},"INVALID_SELECTION");
    refuse(fixture(),{first_color,{"second","","op.nonexistent.color"}},"MISSING_COLOR");
    refuse(fixture(),{first_color,{"missing","","op.paint.color"}},"MISSING_OBJECT");
    inspector.targets=paint_targets;const QColor chosen(99,88,77,66);
    inspector.load();std::unique_ptr<Snapshot> stale;
    inspector.dialog(true,&chosen,[&](QColorDialog&){session.apply({Rename{"second","Later edit"}},session.revision());stale=std::make_unique<Snapshot>(session);});
    check(stale->same(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"An intervening edit refuses the entire stale batch");
    inspector.load();const Snapshot replaced_session(session);
    inspector.dialog(true,&chosen,[&](QColorDialog&){inspector.host.session_id+="-another";});
    check(replaced_session.same(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"A changed Session identity refuses stale acceptance");
    inspector.load();std::unique_ptr<Snapshot> replaced_document;
    inspector.dialog(true,&chosen,[&](QColorDialog&){auto document=fixture();document.id="another-document";session=Session(document);replaced_document=std::make_unique<Snapshot>(session);});
    check(replaced_document->same(session)&&inspector.status().startsWith("SESSION_CONFLICT"),"Document identity guards a same-revision replacement");
    inspector.load();const Snapshot before_gesture(session);Document preview;
    inspector.dialog(true,&chosen,[&](QColorDialog&){session.begin_gesture(session.revision());session.update_gesture({Rename{"second","Preview"}});preview=session.preview_document();});
    check(before_gesture.same(session)&&session.preview_document()==preview&&inspector.status().startsWith("GESTURE_ACTIVE"),
        "An active gesture refuses acceptance without canceling or modifying its preview");
    session.cancel_gesture();inspector.load();const Snapshot canceled(session);
    inspector.dialog(true,&chosen,[&](QColorDialog&){session.begin_gesture(session.revision());session.cancel_gesture();});
    check(canceled.same(session)&&inspector.status().startsWith("REVISION_CONFLICT"),"A canceled gesture invalidates the retained edit context");

    inspector.load();const Snapshot retained(session);
    inspector.dialog(true,&chosen,[&](QColorDialog&){inspector.targets={operation_ref("first","first-unrelated","color"),operation_ref("second","second-unrelated","color")};});
    ColorValue value;value.rgba={chosen.redF(),chosen.greenF(),chosen.blueF(),chosen.alphaF()};
    check(session.document()==recolored(retained.document,value),"Later caller selection changes cannot retarget captured operation IDs");
    inspector.targets=paint_targets;inspector.load();const Snapshot destroyed(session);
    inspector.dialog(true,&chosen,[&](QColorDialog&){delete inspector.controls.data();});
    check(!inspector.controls&&destroyed.same(session),"Destroyed Inspector/dialog cannot apply or dereference retired widgets");
}
}

int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication application(argc,argv);
    try{primary_mixed_precision_cancel();refusal_and_stale_context();
        std::cout<<"PASS "<<checks<<" solid paint color batch Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
