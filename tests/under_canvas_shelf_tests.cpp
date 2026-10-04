#include "window.hpp"
#include "viewport_layout.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QToolButton>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);++checks;}
void settle() {QApplication::processEvents();QTest::qWait(20);QApplication::processEvents();}
struct Snapshot {
    Document document,preview;
    std::string native;
    std::uint64_t revision,generation;
    HistoryInfo history;
    bool undo,redo,gesture;
    explicit Snapshot(const Session& session):document(session.document()),preview(session.preview_document()),
        native(encode(document)),revision(session.revision()),generation(session.gesture_generation()),
        history(session.history()),undo(session.can_undo()),redo(session.can_redo()),gesture(session.gesture_active()) {}
    void unchanged(const Session& session) const {
        check(session.document()==document&&session.preview_document()==preview&&encode(session.document())==native,
            "Shelf/view operations preserve exact authored, preview and native state");
        check(session.revision()==revision&&session.history()==history&&session.can_undo()==undo&&session.can_redo()==redo&&
            session.gesture_generation()==generation&&session.gesture_active()==gesture,
            "Shelf/view operations preserve revision, History, Undo/Redo and gesture ownership");
    }
};
QAction* named_action(Window& window,const char* name) {
    const auto actions=window.findChildren<QAction*>(QString::fromLatin1(name));
    check(actions.size()==1,"Canonical command has exactly one named QAction owner");return actions.front();
}
QToolButton* named_button(Window& window,const char* name) {
    const auto buttons=window.findChildren<QToolButton*>(QString::fromLatin1(name));
    check(buttons.size()==1,"Shelf control retains one discoverable production identity");return buttons.front();
}
QToolBar* authoring_toolbar(Window& window) {
    for(auto* toolbar:window.findChildren<QToolBar*>())if(toolbar->windowTitle()=="Authoring")return toolbar;
    throw std::runtime_error("Production Authoring toolbar is missing");
}
QAction* toolbar_action(QToolBar& toolbar,const QString& label) {
    for(auto* action:toolbar.actions())if(action->text()==label)return action;
    throw std::runtime_error("Retained Authoring action is missing");
}
QRect in_widget(QWidget& widget,QWidget& parent) {return {widget.mapTo(&parent,QPoint{}),widget.size()};}
void reveal(QScrollArea& scroll,QWidget& control) {
    // Qt's ensureWidgetVisible targets an editor's input-method cursor rect.
    // Scroll the complete control bounds so the spinbox arrows are covered too.
    const auto bounds=in_widget(control,*scroll.widget());
    scroll.ensureVisible(bounds.center().x(),bounds.center().y(),bounds.width()/2+1,bounds.height()/2+1);settle();
    if(!scroll.viewport()->rect().contains(in_widget(control,*scroll.viewport()))) {
        const auto r=in_widget(control,*scroll.viewport());
        std::cerr<<"Unreached control "<<control.objectName().toStdString()<<" at "<<r.x()<<","<<r.y()<<" "<<r.width()<<"x"<<r.height()
            <<" in "<<scroll.viewport()->width()<<"x"<<scroll.viewport()->height()<<" scroll "<<scroll.horizontalScrollBar()->value()<<"/"<<scroll.horizontalScrollBar()->maximum()<<'\n';
    }
    check(scroll.viewport()->rect().contains(in_widget(control,*scroll.viewport())),
        "Every shelf control is fully reachable through horizontal scrolling");
    check(control.visibleRegion().contains(control.rect().center()),"Reached shelf control has an unclipped hit center");
}
void composition_and_geometry(Window& window,ViewportLayout& viewport) {
    auto* const scroll=viewport.utility_scroll();auto* const contents=viewport.utility_contents();
    check(window.findChild<QScrollArea*>("canvas-utility-scroll")==scroll&&
        window.findChild<QWidget*>("canvas-utility-strip")==contents,
        "Production Window reuses the named Utility scroll and strip objects");
    check(viewport.utility_placement()==UtilityPlacement::bottom,"Production Window chooses the under-Canvas shelf");
    ViewportLayout generic(new QLabel("Generic Canvas"));
    check(generic.utility_placement()==UtilityPlacement::top,"Generic ViewportLayout default remains top");
    const Snapshot before(window.host.session);
    const std::array<const char*,10> button_names{"canvas-create-circle","canvas-create-rectangle","canvas-create-text",
        "canvas-create-path","utility-show-guides","utility-show-grid","utility-snap","utility-fit","utility-setup","utility-shortcut-help"};
    std::vector<QWidget*> controls;
    for(const auto* name:button_names) {
        auto* button=named_button(window,name);check(button->parentWidget()==contents,"Creation and view controls share the existing strip");
        check(!button->accessibleName().isEmpty(),"Each shelf button has an accessible label");controls.push_back(button);
    }
    auto* zoom=window.findChild<QDoubleSpinBox*>("canvas-zoom-percent");
    check(zoom&&zoom->parentWidget()==contents&&!zoom->accessibleName().isEmpty(),"Existing accessible Zoom control remains in the shelf");
    controls.push_back(zoom);
    auto* toolbar=authoring_toolbar(window);
    check(!toolbar->isMovable(),"Authoring toolbar remains fixed");
    check(toolbar_action(*toolbar,"Undo")&&toolbar_action(*toolbar,"Redo")&&toolbar_action(*toolbar,"Fit"),
        "Authoring toolbar retains Undo, Redo and Fit");
    check(toolbar->actions().contains(named_action(window,"show-colors")),"Authoring toolbar retains the canonical Colors action");
    const auto labels=toolbar->findChildren<QLabel*>();
    check(std::any_of(labels.begin(),labels.end(),[&](const auto* label){return label->text()==window.canvas->breadcrumb();}),
        "Authoring toolbar retains the current Composition breadcrumb");
    check(window.findChild<QLabel*>("canvas-output-readback")!=nullptr,"Existing output readback identity remains discoverable");
    for(const auto size:{QSize(1400,900),QSize(1000,650)}) {
        window.resize(size);settle();check(window.size()==size,"Window honors normal and minimum supported shell sizes");
        const auto canvas_rect=in_widget(*window.canvas,viewport),shelf_rect=in_widget(*scroll,viewport);
        check(viewport.rect().contains(canvas_rect)&&viewport.rect().contains(shelf_rect),"Canvas and shelf stay inside the allocated center region");
        check(canvas_rect.bottom()<shelf_rect.top()&&!canvas_rect.intersects(shelf_rect),"Shelf is strictly below Canvas without overlap");
        check(canvas_rect.width()>0&&canvas_rect.height()>0,"Both supported sizes retain usable Canvas geometry");
        check(contents->width()<=scroll->viewport()->width()||scroll->horizontalScrollBar()->maximum()>0,
            "Shelf overflow always exposes horizontal reachability");
        if(size.width()==1000)check(scroll->horizontalScrollBar()->maximum()>0,"Minimum shell exercises real shelf overflow");
        for(std::size_t i=0;i<controls.size();++i) {
            check(controls[i]->isVisible(),"Retained shelf controls remain visible at both shell sizes");
            for(std::size_t j=i+1;j<controls.size();++j)
                check(!controls[i]->geometry().intersects(controls[j]->geometry()),"Shelf creation and view controls never overlap");
            reveal(*scroll,*controls[i]);
        }
        check(scroll->viewport()->height()>=30,"Horizontal overflow reserves a full-height control hit area");
        before.unchanged(window.host.session);
    }
    for(const auto placement:{UtilityPlacement::top,UtilityPlacement::bottom,UtilityPlacement::top,UtilityPlacement::bottom}) {
        viewport.set_utility_placement(placement);settle();
        check(viewport.utility_scroll()==scroll&&viewport.utility_contents()==contents,
            "Shelf relocation preserves existing scroll and strip identity");
        check(placement==UtilityPlacement::bottom?window.canvas->QWidget::geometry().bottom()<scroll->geometry().top():
            scroll->geometry().bottom()<window.canvas->QWidget::geometry().top(),"Shelf relocation changes only positional layout order");
        before.unchanged(window.host.session);
    }
    check(window.tabPosition(Qt::RightDockWidgetArea)==QTabWidget::North,"Properties and Effects dock tabs are on top");
    auto* properties=window.findChild<QDockWidget*>("properties");auto* effects=window.findChild<QDockWidget*>("effects");
    check(properties&&effects&&window.tabifiedDockWidgets(properties).contains(effects),"Existing Properties and Effects docks stay tabified");
    for(const auto* name:{"structure","properties","effects"}) {
        auto* dock=window.findChild<QDockWidget*>(name);check(dock!=nullptr,"Existing fixed panel names remain discoverable");
        const auto area=QString::fromLatin1(name)=="structure"?Qt::LeftDockWidgetArea:Qt::RightDockWidgetArea;
        check(window.dockWidgetArea(dock)==area&&dock->allowedAreas()==area&&
            dock->features()==QDockWidget::DockWidgetClosable&&!dock->isFloating(),"Fixed panels retain their assigned region and closable-only behavior");
    }
    before.unchanged(window.host.session);
}
void canonical_action_forwarding(Window& window) {
    auto* contents=window.findChild<QWidget*>("canvas-utility-strip");auto* toolbar=authoring_toolbar(window);
    const std::array<const char*,4> button_names{"canvas-create-circle","canvas-create-rectangle","canvas-create-text","canvas-create-path"};
    const std::array<const char*,4> action_names{"add-circle","add-rectangle","add-text","draw-path"};
    for(std::size_t i=0;i<button_names.size();++i) {
        auto* button=named_button(window,button_names[i]);auto* action=named_action(window,action_names[i]);
        check(button->defaultAction()==action&&button->parentWidget()==contents,"Shelf button forwards to the same existing canonical QAction");
        check(!toolbar->actions().contains(action),"Top Authoring toolbar omits duplicate creation affordances");
        check(button->accessibleName().contains(action->text(),Qt::CaseInsensitive),"Creation accessible label identifies its command");
        check(button->isEnabled()==action->isEnabled(),"Creation affordance follows its action's enabled state");
        const Snapshot before(window.host.session);action->setEnabled(false);settle();
        check(!button->isEnabled(),"Canonical action disabled state propagates to its shelf button");
        button->click();before.unchanged(window.host.session);action->setEnabled(true);settle();
        check(button->isEnabled(),"Canonical action re-enables its same shelf button");
    }
    check(named_button(window,"utility-snap")->defaultAction()==named_action(window,"canvas-snap"),
        "Retained Snap button still shares its menu QAction owner");
}
void primitive_click_and_undo(Window& window,const char* button_name,const char* action_name,const char* type) {
    auto& session=window.host.session;const Snapshot before(session);
    auto* button=named_button(window,button_name);auto* action=named_action(window,action_name);
    auto* scroll=window.findChild<QScrollArea*>("canvas-utility-scroll");reveal(*scroll,*button);
    QSignalSpy triggered(action,&QAction::triggered);check(triggered.isValid(),"Canonical QAction trigger observation is valid");
    QTest::mouseClick(button,Qt::LeftButton);settle();
    check(triggered.count()==1,"One shelf click triggers its canonical creation action exactly once");
    check(session.revision()==before.revision+1&&session.document().objects.size()==before.document.objects.size()+1,
        "One shelf click creates one object in one Session revision");
    const auto id=window.canvas->selected_object;check(!id.empty()&&!before.document.objects.contains(id),"Shelf creation selects the new stable object ID");
    const auto& object=session.document().objects.at(id);
    check(object.kind==Kind::path&&object.source&&object.source->type==type,"Shelf action creates the expected native primitive type");
    Session replay(before.document);
    replay.apply({CreatePrimitive{window.canvas->active_composition(),"",id,object.name,*object.source}},replay.revision());
    check(session.document()==replay.document(),"Shelf output exactly matches the shared core CreatePrimitive command");
    const auto old_state=std::find_if(before.history.states.begin(),before.history.states.end(),
        [&](const auto& state){return state.id==before.history.current_id;});
    check(old_state!=before.history.states.end(),"Prior current History boundary is retained");
    check(session.history().states.size()==static_cast<std::size_t>(old_state-before.history.states.begin())+2,
        "Shelf creation appends exactly one History boundary after the active state");
    const auto created=session.document();const auto created_state=session.history().current_id;
    toolbar_action(*authoring_toolbar(window),"Undo")->trigger();settle();
    check(session.revision()==before.revision+2&&session.document()==before.document&&encode(session.document())==before.native&&
        session.preview_document()==before.preview&&session.history().current_id==before.history.current_id&&session.can_redo(),
        "Existing Undo restores the exact pre-click document in one boundary");
    toolbar_action(*authoring_toolbar(window),"Redo")->trigger();settle();
    check(session.revision()==before.revision+3&&session.document()==created&&session.history().current_id==created_state,
        "Existing Redo restores the exact shelf-created native object in one boundary");
    toolbar_action(*authoring_toolbar(window),"Undo")->trigger();settle();
    check(session.document()==before.document,"Primitive check leaves the original authored document restored");
}
void view_controls_and_draw_mode(Window& window) {
    auto& session=window.host.session;const Snapshot before(session);
    auto* scroll=window.findChild<QScrollArea*>("canvas-utility-scroll");
    for(const auto* name:{"utility-show-guides","utility-show-grid","utility-snap"}) {
        auto* button=named_button(window,name);reveal(*scroll,*button);const bool initial=button->isChecked();
        QTest::mouseClick(button,Qt::LeftButton);settle();check(button->isChecked()!=initial,"Retained view toggle responds to a shelf click");
        if(QString::fromLatin1(name)=="utility-show-guides")check(window.canvas->show_guides()==button->isChecked(),"Guide visibility reaches Canvas view state");
        else if(QString::fromLatin1(name)=="utility-show-grid")check(window.canvas->show_grid()==button->isChecked(),"Grid visibility reaches Canvas view state");
        else check(window.canvas->snap_enabled()==button->isChecked()&&named_action(window,"canvas-snap")->isChecked()==button->isChecked(),
            "Snap shelf, canonical action and Canvas stay synchronized");
        before.unchanged(session);QTest::mouseClick(button,Qt::LeftButton);settle();
        check(button->isChecked()==initial,"Repeated view toggle returns to its previous view-only state");before.unchanged(session);
    }
    auto* zoom=window.findChild<QDoubleSpinBox*>("canvas-zoom-percent");reveal(*scroll,*zoom);zoom->setValue(150);settle();
    check(std::abs(window.canvas->zoom()-1.5)<1e-9,"Retained Zoom control still changes Canvas view scale");before.unchanged(session);
    auto* fit=named_button(window,"utility-fit");reveal(*scroll,*fit);QTest::mouseClick(fit,Qt::LeftButton);settle();
    const auto board=evaluate_artboard(session.document().compositions.front(),window.canvas->active_artboard());
    const auto expected=std::clamp(std::min(std::max(1,window.canvas->width()-100)/board.width,
        std::max(1,window.canvas->height()-100)/board.height),0.02,64.0);
    check(std::abs(window.canvas->zoom()-expected)<1e-9,"Retained Fit still uses the actual Canvas viewport");before.unchanged(session);
    auto* path=named_button(window,"canvas-create-path");auto* draw=named_action(window,"draw-path");reveal(*scroll,*path);
    QSignalSpy triggered(draw,&QAction::triggered);check(triggered.isValid(),"Draw Path canonical action observation is valid");
    QTest::mouseClick(path,Qt::LeftButton);settle();
    check(triggered.count()==1&&window.canvas->draw_mode(),"Draw Path shelf click enters the existing Canvas drawing mode");
    before.unchanged(session);QTest::mouseClick(path,Qt::LeftButton);settle();
    check(triggered.count()==2&&window.canvas->draw_mode(),"Repeated Draw Path activation keeps the existing Canvas mode active");
    before.unchanged(session);QTest::keyClick(window.canvas,Qt::Key_Escape);settle();
    check(!window.canvas->draw_mode(),"Canvas Escape exits the unchanged Draw Path mode");before.unchanged(session);
    QTest::mouseClick(path,Qt::LeftButton);settle();before.unchanged(session);
    QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,window.canvas->rect().center());settle();
    check(session.revision()==before.revision+1&&session.document().objects.size()==before.document.objects.size()+1,
        "Draw Path creates its first object only after actual Canvas input");
    const auto& drawn=session.document().objects.at(window.canvas->selected_object);
    check(drawn.kind==Kind::path&&!drawn.source&&drawn.contours.size()==1&&drawn.contours.front().points.size()==1,
        "Actual Canvas input follows the existing native one-point path creation flow");
    const auto drawn_document=session.document();QTest::keyClick(window.canvas,Qt::Key_Return);settle();
    check(!window.canvas->draw_mode()&&session.revision()==before.revision+1&&session.document()==drawn_document,
        "Enter finishes the existing Draw Path mode without another authored edit");
    toolbar_action(*authoring_toolbar(window),"Undo")->trigger();settle();
    check(session.document()==before.document&&encode(session.document())==before.native,
        "The existing Undo restores the document before actual drawing");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir files;check(files.isValid(),"Owned test scratch directory is available");
        QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
        Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
        window.show();settle();auto* viewport=dynamic_cast<ViewportLayout*>(window.centralWidget());
        check(viewport!=nullptr,"Production Window owns the shared ViewportLayout");
        composition_and_geometry(window,*viewport);canonical_action_forwarding(window);
        primitive_click_and_undo(window,"canvas-create-circle","add-circle","nect.shape.circle");
        primitive_click_and_undo(window,"canvas-create-rectangle","add-rectangle","nect.shape.rectangle");
        view_controls_and_draw_mode(window);
        window.host.changed={};window.hide();
        std::cout<<"PASS "<<checks<<" under-Canvas shelf checks (Qt contract; physical OS input and full regression NOT_RUN)\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}
}
