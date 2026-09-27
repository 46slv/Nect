#include "window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T* widget(Window& w,const char* name){QApplication::processEvents();for(auto* p:w.findChildren<T*>())if(p->isVisible()&&p->objectName()==name)return p;throw std::runtime_error(std::string("Missing ")+name);}
void context(Window& w,const QString& prefix){bool chosen=false;QTimer::singleShot(0,[&]{auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!menu)return;for(auto* a:menu->actions())if(a->text().startsWith(prefix)&&a->isEnabled()){chosen=true;menu->setActiveAction(a);QTest::keyClick(menu,Qt::Key_Return);return;}menu->close();});
    QMetaObject::invokeMethod(w.canvas,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,QPoint(250,250)));QApplication::processEvents();check(chosen,"Context action is discoverable and enabled");}
void controls(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("doc","comp","board"));auto& s=w.host.session;
    auto background=default_primitive("background-source","nect.shape.rectangle");auto content=default_primitive("content-source","nect.shape.rectangle");auto clip=default_primitive("clip-source","nect.shape.circle");
    s.apply({CreatePrimitive{"comp","","background","Background",background},CreatePrimitive{"comp","","content","Content",content},CreatePrimitive{"comp","","clip","Circle mask",clip}},s.revision());w.host.edited();w.show();QApplication::processEvents();
    const auto original=encode(s.document());auto revision=s.revision();w.canvas->set_selections({{"clip",""},{"content",""}});
    context(w,"Mask With Top · Circle mask");check(s.revision()==revision+1,"Mask is one transaction");const auto group=w.canvas->selected_object;
    check(s.document().objects.at(group).compositing.mask->source=="clip"&&!s.document().objects.at("clip").visible,"Paint order selects source and hides normal artwork");
    auto* outline=widget<QCheckBox>(w,"mask-show-outline");const auto authored=encode(s.document());outline->setChecked(!outline->isChecked());check(encode(s.document())==authored,"Outline is viewport state only");
    widget<QPushButton>(w,"mask-edit-source")->click();check(w.canvas->selected_object=="clip"&&!s.document().objects.at("clip").visible,"Editing selects retained hidden source without showing its paint");
    w.canvas->set_selection(group);widget<QComboBox>(w,"object-blend")->setCurrentIndex(1);check(s.document().objects.at(group).compositing.blend=="multiply","Blend row uses shared Session command");
    widget<QCheckBox>(w,"mask-enabled")->setChecked(false);check(!s.document().objects.at(group).compositing.mask->enabled,"Mask bypass preserves source");
    s.undo(s.revision());w.host.edited();s.undo(s.revision());w.host.edited();s.undo(s.revision());w.host.edited();check(encode(s.document())==original,"Mask, blend and bypass Undo restores exact source structure");
    w.canvas->set_selections({{"content",""},{"clip",""}});context(w,"Mask With Bottom · Content");
    const auto bottom=w.canvas->selected_object;check(s.document().objects.at(bottom).compositing.mask->source=="content","Bottom uses first painted object");
    check(encode(decode(encode(s.document())))==encode(s.document()),"Masked UI document survives native readback");
    const auto masked=encode(s.document());const auto masked_revision=s.revision();
    w.canvas->set_selections({{bottom,""},{"background",""}});context(w,"Put Inside · Masked Group");
    check(s.revision()==masked_revision&&encode(s.document())==masked&&
          w.statusBar()->currentMessage().startsWith("PUT_INSIDE_APPEARANCE"),
        "Put Inside rejects a masked destination without changing authored state");
    s.apply({SetMask{bottom,std::nullopt}},s.revision());w.host.edited();
    w.canvas->set_selections({{bottom,""},{"background",""}});context(w,"Put Inside · Masked Group");
    check(s.document().objects.at(bottom).children.front()=="background"&&s.document().compositions.front().roots.size()==1,
        "Put Inside targets the now neutral top selected Group and retains order");
}
void visibility_inspector(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("visibility-doc","visibility-comp","visibility-board"));
    auto& session=w.host.session;
    session.apply({CreatePrimitive{"visibility-comp","","source","Source",default_primitive("source-primitive","nect.shape.rectangle")},
        CreatePrimitive{"visibility-comp","","target","Target",default_primitive("target-primitive","nect.shape.rectangle")}},session.revision());
    w.host.edited();w.show();QApplication::processEvents();
    session.apply({SetVisibility{"source",false}},session.revision());w.host.edited();w.canvas->set_selection("target");QApplication::processEvents();
    auto* link_button=widget<QToolButton>(w,"object-visible-driver");
    check(widget<QCheckBox>(w,"object-visible")->isChecked()&&widget<QCheckBox>(w,"object-visible")->isEnabled(),
        "Visibility Inspector starts from the authored literal");
    bool picked=false;QTimer chooser;chooser.setInterval(0);
    QObject::connect(&chooser,&QTimer::timeout,&w,[&]{
        for(auto* top:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(top)) {
            if(auto* combo=dialog->findChild<QComboBox*>();combo&&combo->count()) {
                for(int i=0;i<combo->count();++i)if(combo->itemText(i).contains("source")){combo->setCurrentIndex(i);picked=true;break;}
            }
            dialog->accept();chooser.stop();return;
        }
    });
    chooser.start();
    link_button->menu()->actions().front()->trigger();QApplication::processEvents();
    const auto linked=object_visibility_state(session.document(),{"target","","object.visible"});
    check(picked&&linked.literal&&linked.driver==Ref{"source","","object.visible"}&&!linked.evaluated,
        "Inspector Link action selects a stable source and preserves the authored checkbox value");
    check(!widget<QCheckBox>(w,"object-visible")->isEnabled()&&
        widget<QLabel>(w,"object-visible-status")->text().contains("Evaluated own visibility: false"),
        "Driven Inspector disables direct editing and shows evaluated own visibility");
    auto* unlink_button=widget<QToolButton>(w,"object-visible-driver");
    check(unlink_button->menu()->actions().size()==2&&unlink_button->menu()->actions()[1]->isEnabled(),
        "Inspector exposes unlink and freeze for a driven value");
    unlink_button->menu()->actions()[1]->trigger();QApplication::processEvents();
    const auto frozen=object_visibility_state(session.document(),{"target","","object.visible"});
    check(!frozen.literal&&!frozen.driver&&widget<QCheckBox>(w,"object-visible")->isEnabled()&&
        widget<QLabel>(w,"object-visible-status")->text().contains("Evaluated own visibility: false"),
        "Inspector Unlink freezes the evaluated value and returns control to its checkbox");
    session.apply({SetVisibility{"source",true}},session.revision());w.host.edited();
    check(!object_visibility_state(session.document(),{"target","","object.visible"}).evaluated,
        "Unlinked Inspector value remains frozen when its former source changes");
}
void isolation_inspector(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("isolation-doc","isolation-comp","isolation-board"));
    auto& session=w.host.session;
    session.apply({CreatePrimitive{"isolation-comp","","source","Isolation Source",default_primitive("source-primitive","nect.shape.rectangle")},
        CreatePrimitive{"isolation-comp","","target","Isolation Target",default_primitive("target-primitive","nect.shape.rectangle")},
        SetCompositing{"source","normal",true}},session.revision());
    w.host.edited();w.show();QApplication::processEvents();w.canvas->set_selection("target");QApplication::processEvents();
    const Ref target{"target","","composite.isolated"},source{"source","","composite.isolated"};
    auto* isolation=widget<QCheckBox>(w,"object-isolated");
    check(!isolation->isChecked()&&isolation->isEnabled(),"Isolation Inspector starts from the authored literal");
    auto* driver=widget<QToolButton>(w,"object-isolated-driver");bool picked=false;QTimer chooser;chooser.setInterval(0);
    QObject::connect(&chooser,&QTimer::timeout,&w,[&]{
        for(auto* top:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(top)) {
            if(auto* combo=dialog->findChild<QComboBox*>();combo&&combo->count()) {
                for(int i=0;i<combo->count();++i)if(combo->itemText(i).contains("source")){combo->setCurrentIndex(i);picked=true;break;}
            }
            dialog->accept();chooser.stop();return;
        }
    });
    chooser.start();driver->menu()->actions().front()->trigger();QApplication::processEvents();
    auto state=composite_isolation_state(session.document(),target);
    check(picked&&state.literal==false&&state.driver==source&&state.evaluated,
        "Inspector links a stable isolation source and retains the false authored literal");
    check(!widget<QCheckBox>(w,"object-isolated")->isEnabled()&&
        widget<QLabel>(w,"object-isolated-status")->text().contains("Evaluated authored isolation: true")&&
        widget<QLabel>(w,"object-isolated-status")->text().contains("Isolation Source"),
        "Driven Inspector disables literal editing and shows source and evaluated authored value");
    session.apply({SetCompositing{"target","multiply",false}},session.revision());w.host.edited();
    state=composite_isolation_state(session.document(),target);
    check(state.literal==false&&state.driver==source&&state.evaluated&&
        session.document().objects.at("target").compositing.blend=="multiply",
        "Inspector blend edit preserves the isolation driver and authored literal");
    session.apply({SetCompositing{"source","normal",false}},session.revision());w.host.edited();
    state=composite_isolation_state(session.document(),target);
    check(!state.evaluated&&widget<QLabel>(w,"object-isolated-status")->text().contains("Evaluated authored isolation: false"),
        "Inspector follows a source edit without changing the target literal");
    driver=widget<QToolButton>(w,"object-isolated-driver");
    check(driver->menu()->actions().size()==2&&driver->menu()->actions()[1]->isEnabled(),
        "Inspector offers explicit unlink for a driven isolation value");
    driver->menu()->actions()[1]->trigger();QApplication::processEvents();
    state=composite_isolation_state(session.document(),target);
    check(!state.literal&&!state.driver&&!widget<QCheckBox>(w,"object-isolated")->isChecked()&&
        widget<QCheckBox>(w,"object-isolated")->isEnabled(),
        "Inspector unlink freezes evaluated isolation and re-enables its literal toggle");
    session.apply({SetCompositing{"source","normal",true}},session.revision());w.host.edited();
    check(!composite_isolation_state(session.document(),target).evaluated,
        "Unlinked Inspector value stays frozen when its former source changes");
}
void mask_enabled_inspector(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("mask-link-doc","mask-link-comp","mask-link-board"));
    auto& session=w.host.session;
    session.apply({CreatePrimitive{"mask-link-comp","","target","Mask Target",default_primitive("target-source","nect.shape.rectangle")},
        CreatePrimitive{"mask-link-comp","","source","Mask Source",default_primitive("source-source","nect.shape.rectangle")},
        CreatePrimitive{"mask-link-comp","","geometry","Mask Geometry",default_primitive("geometry-source","nect.shape.rectangle")},
        SetMask{"target",GeometryMask{"target-mask","source",1,false,"evenodd"}},
        SetMask{"source",GeometryMask{"source-mask","geometry",1,true,"nonzero"}}},session.revision());
    w.host.edited();w.show();QApplication::processEvents();w.canvas->set_selection("target");QApplication::processEvents();
    const auto target=geometry_mask_enabled_ref("target","target-mask");
    const auto source=geometry_mask_enabled_ref("source","source-mask");
    check(!geometry_mask_enabled_state(session.document(),target).literal&&
        widget<QCheckBox>(w,"mask-enabled")->isEnabled(),
        "Mask Inspector starts with its authored bypass literal editable");
    auto* driver=widget<QToolButton>(w,"mask-enabled-driver");bool picked=false;QTimer chooser;chooser.setInterval(0);
    QObject::connect(&chooser,&QTimer::timeout,&w,[&]{
        for(auto* top:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(top)) {
            if(auto* combo=dialog->findChild<QComboBox*>();combo&&combo->count()) {
                for(int i=0;i<combo->count();++i)if(combo->itemText(i).contains("Mask Source")){combo->setCurrentIndex(i);picked=true;break;}
            }
            dialog->accept();chooser.stop();return;
        }
    });
    chooser.start();driver->menu()->actions().front()->trigger();QApplication::processEvents();
    auto state=geometry_mask_enabled_state(session.document(),target);
    check(picked&&!state.literal&&state.driver==source&&state.evaluated,
        "Inspector links an exact mask instance while preserving the false literal");
    check(!widget<QCheckBox>(w,"mask-enabled")->isEnabled()&&
        widget<QLabel>(w,"mask-enabled-state")->text().contains("Evaluated enabled: true")&&
        widget<QLabel>(w,"mask-enabled-state")->text().contains("source-mask"),
        "Driven mask Inspector disables its checkbox and shows source and evaluated value");
    auto source_mask=*session.document().objects.at("source").compositing.mask;source_mask.enabled=false;
    session.apply({SetMask{"source",source_mask}},session.revision());w.host.edited();
    check(!geometry_mask_enabled_state(session.document(),target).evaluated&&
        widget<QLabel>(w,"mask-enabled-state")->text().contains("Evaluated enabled: false"),
        "Inspector follows source bypass changes without altering the target literal");
    driver=widget<QToolButton>(w,"mask-enabled-driver");
    check(driver->menu()->actions().size()==2&&driver->menu()->actions()[1]->isEnabled(),
        "Inspector offers explicit unlink for a driven mask value");
    driver->menu()->actions()[1]->trigger();QApplication::processEvents();
    state=geometry_mask_enabled_state(session.document(),target);
    check(!state.literal&&!state.driver&&widget<QCheckBox>(w,"mask-enabled")->isEnabled(),
        "Inspector unlink freezes evaluated false and re-enables the authored bypass checkbox");
    source_mask=*session.document().objects.at("source").compositing.mask;source_mask.enabled=true;
    session.apply({SetMask{"source",source_mask}},session.revision());w.host.edited();
    check(!geometry_mask_enabled_state(session.document(),target).evaluated,
        "Unlinked target remains frozen when its former source mask changes");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{controls();visibility_inspector();isolation_inspector();mask_enabled_inspector();std::cout<<"Compositing UI passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
