#include "window.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTest>
#include <QStatusBar>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
QByteArray reference(const Ref& r) {
    return QJsonDocument(QJsonObject{{"object",QString::fromStdString(r.object)},
        {"point",QString::fromStdString(r.point)},{"field",QString::fromStdString(r.field)}}).toJson(QJsonDocument::Compact);
}
template<class T> T* field(Window& w,const Ref& ref) {
    const auto data=reference(ref);
    for(auto* widget:w.findChildren<T*>())
        if(widget->isVisible()&&widget->property("nect-reference").toByteArray()==data)return widget;
    throw std::runtime_error("Visible property widget missing");
}
void move(Window& w,QPoint global) {
    QMouseEvent event(QEvent::MouseMove,w.mapFromGlobal(global),global,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&w,&event);QApplication::processEvents();
}
void release(Window& w,QPoint global) {
    QMouseEvent event(QEvent::MouseButtonRelease,w.mapFromGlobal(global),global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&w,&event);QApplication::processEvents();
}
template<class T> T* visible_child(Window& window,const char* name) {
    for(auto* widget:window.findChildren<T*>(QString::fromLatin1(name)))
        if(widget->isVisible())return widget;
    throw std::runtime_error(std::string("Visible widget missing: ")+name);
}
QAction* named_action(Window& window,const char* name) {
    auto* action=window.findChild<QAction*>(QString::fromLatin1(name));
    check(action!=nullptr,"Named production action exists");
    return action;
}
void history_action(Window& window,const char* text) {
    for(auto* action:window.findChildren<QAction*>())if(action->text()==QString::fromLatin1(text)) {
        check(action->isEnabled(),"History action is enabled");action->trigger();QApplication::processEvents();return;
    }
    throw std::runtime_error("History action missing");
}
void reveal(Window& window,QWidget* widget) {
    auto* scroll=window.findChild<QScrollArea*>();
    check(scroll!=nullptr,"Inspector scroll area exists");
    scroll->ensureWidgetVisible(widget);QApplication::processEvents();
}
void edit_number(Window& window,const Ref& ref,const char* text) {
    auto* input=field<QLineEdit>(window,ref);reveal(window,input);input->setFocus();
    QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);
    QTest::keyClicks(input,text);QTest::keyClick(input,Qt::Key_Return);QApplication::processEvents();
}
void toggle_correction(Window& window) {
    auto* checkbox=visible_child<QCheckBox>(window,"point-edit-enabled");
    check(checkbox->isEnabled(),"Authored Point Edit can be toggled");reveal(window,checkbox);
    QTest::mouseClick(checkbox,Qt::LeftButton,Qt::NoModifier,QPoint(8,checkbox->height()/2));
    QApplication::processEvents();
}
void primitive_authoring(Window& window) {
    auto& session=window.host.session;
    auto revision=session.revision();
    named_action(window,"add-circle")->trigger();QApplication::processEvents();
    const auto object=window.canvas->selected_object;
    check(session.revision()==revision+1&&!object.empty(),"Add Circle creates and selects one real object");
    const auto& circle=session.document().objects.at(object);
    check(circle.source&&circle.source->type=="nect.shape.circle"&&circle.contours.empty(),
        "Add Circle retains an authored generator instead of flattening a path");
    const auto source_id=circle.source->id;
    const auto east=source_id+"-east";
    const Ref radius{object,"","generator.radius"},point_x{object,east,"x"};
    const auto& artboard=session.document().compositions.front().artboards.front();
    const auto center_x=artboard.x+artboard.width/2;
    const auto center_y=artboard.y+artboard.height/2;
    check(evaluate(session.document()).at(radius)==100&&evaluate(session.document()).at(point_x)==center_x+100,
        "Circle defaults to radius 100 at the current artboard center");
    check(evaluate(session.document()).at({object,east,"y"})==center_y,"Circle center Y follows the artboard default");
    auto* correction=visible_child<QCheckBox>(window,"point-edit-enabled");
    check(!correction->isEnabled()&&!correction->isChecked(),"A new source shows an empty correction entry without inventing overrides");
    revision=session.revision();
    edit_number(window,radius,"150");
    check(session.revision()==revision+1&&evaluate(session.document()).at(radius)==150&&
        evaluate(session.document()).at(point_x)==center_x+150,"Radius field edits the source through one Session command");

    auto* tree=window.findChild<QTreeWidget*>();
    QTreeWidgetItem* source_row=nullptr;
    for(int i=0;i<tree->topLevelItemCount();++i)
        if(tree->topLevelItem(i)->data(0,Qt::UserRole).toString().toStdString()==object)source_row=tree->topLevelItem(i);
    check(source_row&&source_row->childCount()==4,"Generated stable point topology appears in the object tree");
    QTreeWidgetItem* east_row=nullptr;
    for(int i=0;i<source_row->childCount();++i)
        if(source_row->child(i)->data(0,Qt::UserRole+1).toString().toStdString()==east)east_row=source_row->child(i);
    check(east_row&&east_row->text(0)=="East","Generated role names resolve to stable point IDs");
    source_row->setExpanded(true);tree->scrollToItem(east_row);QApplication::processEvents();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(east_row).center());
    QApplication::processEvents();
    check(window.canvas->selected_object==object&&window.canvas->selected_point==east,
        "Clicking a generated point selects its actual stable property context");
    check(field<QLineEdit>(window,point_x)->property("nect-property-origin")=="generated",
        "Generated coordinate is identified before any correction");
    const auto override_x=center_x+190;
    const auto override_text=QString::number(override_x).toLatin1();
    revision=session.revision();
    edit_number(window,point_x,override_text.constData());
    const auto& corrected=session.document().objects.at(object);
    check(session.revision()==revision+1&&corrected.source&&corrected.point_edit&&corrected.point_edit->enabled,
        "A generated point numeric edit adds and enables Point Edit while retaining the source");
    check(corrected.point_edit->overrides.at(east).at("x").literal==override_x&&
        evaluate(session.document()).at(radius)==150,"Point Edit stores an absolute coordinate without rewriting radius");
    check(field<QLineEdit>(window,point_x)->property("nect-property-origin")=="point_edit",
        "The changed field immediately reports its Point Edit origin");
    check(visible_child<QLabel>(window,"point-edit-summary")->text().contains("1 absolute local overrides"),
        "Inspector shows the authored correction count immediately after the edit");
    revision=session.revision();toggle_correction(window);
    check(session.revision()==revision+1&&!session.document().objects.at(object).point_edit->enabled&&
        evaluate(session.document()).at(point_x)==center_x+150,"Point Edit bypass restores the current generated geometry");
    check(field<QLineEdit>(window,point_x)->property("nect-property-origin")=="bypassed_point_edit",
        "Bypassed authored correction remains visible as a distinct property origin");
    history_action(window,"Undo");
    check(session.document().objects.at(object).point_edit->enabled&&evaluate(session.document()).at(point_x)==override_x,
        "UI Undo restores the enabled correction");
    history_action(window,"Redo");
    check(!session.document().objects.at(object).point_edit->enabled&&evaluate(session.document()).at(point_x)==center_x+150,
        "UI Redo restores source-only evaluation");
    toggle_correction(window);

    const auto topology=path_contours(session.document().objects.at(object));
    revision=session.revision();named_action(window,"convert-to-path")->trigger();QApplication::processEvents();
    auto* dialog=visible_child<QDialog>(window,"convert-to-path-dialog");
    auto* convert=dialog->findChild<QPushButton*>("confirm-convert-to-path");
    check(convert&&convert->isEnabled()&&session.revision()==revision,"Conversion requires the explicit review dialog and does not mutate on opening");
    QTest::mouseClick(convert,Qt::LeftButton);QApplication::processEvents();
    const auto& converted=session.document().objects.at(object);
    check(session.revision()==revision+1&&!converted.source&&!converted.point_edit,
        "Confirmed conversion removes source and correction only through the core command");
    check(converted.contours.size()==1&&converted.contours.front().id==topology.front().id&&
        converted.contours.front().points.size()==topology.front().points.size(),"Conversion retains generated contour topology");
    for(std::size_t i=0;i<topology.front().points.size();++i)
        check(converted.contours.front().points[i].id==topology.front().points[i].id,"Conversion retains every stable point ID");
    check(evaluate(session.document()).at(point_x)==override_x&&window.canvas->selected_point==east,
        "Conversion preserves visible correction and point selection");
    history_action(window,"Undo");
    check(session.document().objects.at(object).source&&session.document().objects.at(object).point_edit&&
        evaluate(session.document()).at(point_x)==override_x,"Undo conversion restores the generator and authored correction");

    // Seed a real semantic dependency, then verify the user-facing conversion
    // plan refuses to remove the generator that this external field references.
    const Ref dependent{"a","a1","y"};
    session.apply({Link{dependent,{radius,1,0,"copy_local_value"}}},session.revision());
    window.host.edited();QApplication::processEvents();revision=session.revision();
    named_action(window,"convert-to-path")->trigger();QApplication::processEvents();
    dialog=visible_child<QDialog>(window,"convert-to-path-dialog");
    convert=dialog->findChild<QPushButton*>("confirm-convert-to-path");
    const auto* blockers=dialog->findChild<QListWidget*>("conversion-blockers");
    check(blockers&&blockers->count()==1&&convert&&!convert->isEnabled(),
        "Conversion plan names external generator references and disables conversion");
    QTest::mouseClick(convert,Qt::LeftButton);QApplication::processEvents();
    check(session.revision()==revision&&session.document().objects.at(object).source&&
        nect::property(session.document(),dependent).binding->source==radius,
        "Blocked conversion preserves generator, dependency and revision");
    auto* buttons=dialog->findChild<QDialogButtonBox*>();
    check(buttons!=nullptr,"Conversion review has a cancellation action");
    QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel),Qt::LeftButton);QApplication::processEvents();
    check(session.revision()==revision,"Cancelling the blocked plan does not mutate the document");

    named_action(window,"add-rectangle")->trigger();QApplication::processEvents();
    const auto rectangle=window.canvas->selected_object;
    const auto& rectangle_object=session.document().objects.at(rectangle);
    check(rectangle_object.source&&rectangle_object.source->type=="nect.shape.rectangle"&&
        evaluate(session.document()).at({rectangle,"","generator.width"})==220&&
        evaluate(session.document()).at({rectangle,"","generator.height"})==140&&
        path_contours(rectangle_object).front().points.size()==4,
        "Add Rectangle creates the usable default source with stable four-point topology");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir temp;Window w(temp.path());w.show();QApplication::processEvents();
        auto& s=w.host.session;const auto comp=s.document().compositions.front().id;
        Point a,b;a.id="a1";a.x.literal=100;a.y.literal=180;b.id="b1";b.x.literal=200;b.y.literal=250;
        s.apply({CreatePath{comp,"","a","Target",{{"ca",false,{a}}}},CreatePath{comp,"","b","Source",{{"cb",false,{b}}}}},0);
        w.canvas->set_selection("a","a1");w.host.edited();QApplication::processEvents();
        const Ref target{"a","a1","x"},source{"b","b1","y"};
        auto* input=field<QLineEdit>(w,target);input->setFocus();QTest::keyClicks(input,"draft");
        const auto draft=input->text();QTest::qWait(1100);
        check(input->text()==draft&&input->hasFocus(),"Recovery status must not replace focused input drafts");
        check(s.revision()==1,"Incomplete draft is not committed by recovery");
        input->setText("100");input->setModified(false);
        auto* whip=field<QPushButton>(w,target);
        QTest::mousePress(whip,Qt::LeftButton,Qt::NoModifier,whip->rect().center());
        auto* tree=w.findChild<QTreeWidget*>();check(tree!=nullptr,"Object tree exists");
        QTreeWidgetItem* item=nullptr;
        for(int i=0;i<tree->topLevelItemCount();++i)if(tree->topLevelItem(i)->data(0,Qt::UserRole)=="b")item=tree->topLevelItem(i);
        check(item!=nullptr,"Source row exists");
        move(w,tree->viewport()->mapToGlobal(tree->visualItemRect(item).center()));
        check(w.canvas->selected_object=="b","Pick-whip can inspect another object without changing its target");
        auto* source_field=field<QLineEdit>(w,source);
        release(w,source_field->mapToGlobal(source_field->rect().center()));
        if(s.revision()!=2||evaluate(s.document()).at(target)!=250)
            std::cerr<<"revision="<<s.revision()<<" value="<<evaluate(s.document()).at(target)
                <<" status="<<w.statusBar()->currentMessage().toStdString()<<'\n';
        check(s.revision()==2&&evaluate(s.document()).at(target)==250,"Pick-whip commits one shared command");
        check(nect::property(s.document(),target).binding->source==source,"Pick-whip persists stable source ID");
        check(w.canvas->selected_object=="a","Pick-whip returns to target context");
        s.undo(2);w.host.edited();QApplication::processEvents();
        check(evaluate(s.document()).at(target)==100&&!nect::property(s.document(),target).binding,"Pick-whip undo restores target");
        whip=field<QPushButton>(w,target);QTest::mousePress(whip,Qt::LeftButton,Qt::NoModifier,whip->rect().center());
        move(w,tree->viewport()->mapToGlobal(tree->visualItemRect(tree->topLevelItem(1)).center()));
        QTest::keyClick(&w,Qt::Key_Escape);QApplication::processEvents();
        check(s.revision()==3&&!nect::property(s.document(),target).binding&&w.canvas->selected_object=="a",
            "Pick-whip cancellation preserves document and restores context");
        primitive_authoring(w);
        std::cout<<"PASS Inspector recovery draft, cross-object pick-whip, primitive source/correction and conversion UI\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
