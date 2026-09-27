#include "window.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QComboBox>
#include <QCompleter>
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QGraphicsOpacityEffect>
#include <QGroupBox>
#include <QGuiApplication>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QDebug>
#include <QTemporaryDir>
#include <QTest>
#include <QStatusBar>
#include <QPlainTextEdit>
#include <QInputDialog>
#include <QMenu>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QFile>
#include <algorithm>
#include <cmath>
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
void stacking_authoring(Window& w) {
    auto& s=w.host.session;const auto comp=s.document().compositions.front().id;std::vector<Command> commands;
    for(const auto* id:{"a","b","c","d"}){Point p;p.id=std::string(id)+"-point";p.x.literal=10;commands.push_back(CreatePath{comp,"",id,id,{{std::string(id)+"-contour",false,{p}}}});}
    commands.push_back(Link{{"d","d-point","x"},{{"a","a-point","x"},2,3,"copy_local_value"}});s.apply(commands,0);w.host.edited();
    auto order=[&](){return s.document().compositions.front().roots;};
    auto act=[&](const char* name){named_action(w,name)->trigger();};
    auto undo=[&](){s.undo(s.revision());w.host.edited();};
    const auto authored=s.document();w.canvas->set_selection("b");act("stack-forward");
    check(order()==std::vector<Id>{"a","c","b","d"},"Bring forward changes only neighboring stacking order");
    check(s.document().objects==authored.objects&&evaluate(s.document()).at({"d","d-point","x"})==23,"Stacking preserves points, references and transforms");undo();check(s.document()==authored,"Stacking is one exact Undo");
    w.canvas->set_selections({{"d",""},{"b",""}});act("stack-back");check(order()==std::vector<Id>{"b","d","a","c"},"Send to back preserves sibling order, not click order");undo();
    w.canvas->set_selections({{"b",""},{"a",""}});act("stack-forward");check(order()==std::vector<Id>{"c","a","b","d"},"Selected block moves one unselected neighbor");
    act("stack-front");check(order()==std::vector<Id>{"c","d","a","b"},"Front moves selected block past all remaining siblings");
    const auto edge_revision=s.revision();act("stack-front");check(s.revision()==edge_revision,"Stacking boundary is history-free no-op");
    act("stack-backward");check(order()==std::vector<Id>{"c","a","b","d"},"Send backward moves block one neighbor");
    act("stack-back");check(order()==std::vector<Id>{"a","b","c","d"},"Back restores original stacking order");
    s.apply({GroupContiguous{comp,"",{"b","c"},"group","Group"}},s.revision());w.host.edited();w.canvas->set_selection("c");
    const auto grouped=s.document();act("stack-backward");check(s.document().objects.at("group").children==std::vector<Id>{"c","b"}&&order()==std::vector<Id>{"a","group","d"},"Nested selection only reorders within its parent");undo();check(s.document()==grouped,"Nested stacking exact Undo");
    const auto revision=s.revision();w.canvas->set_selections({{"a",""},{"b",""}});act("stack-front");check(s.revision()==revision&&w.statusBar()->currentMessage().startsWith("INVALID_SELECTION"),"Mixed parents refuse atomically");
    w.canvas->set_selections({{"a","a-point"},{"d",""}});act("stack-front");check(s.revision()==revision,"Mixed point/object selection cannot reorder objects silently");
    w.canvas->set_selection("group");act("stack-front");check(order()==std::vector<Id>{"a","d","group"},"Groups reorder as structural units");
    check(decode(encode(s.document()))==s.document(),"Stacking survives native serialization");
    const auto before_ungroup=s.document();const auto ungroup_revision=s.revision();named_action(w,"ungroup-objects")->trigger();
    check(s.revision()==ungroup_revision+1&&!s.document().objects.contains("group")&&order()==std::vector<Id>{"a","d","b","c"},"GUI Ungroup commits shared structural command");
    check(w.canvas->selected_objects()==std::vector<Id>{"b","c"},"Ungroup selects surviving children");
    check(decode(encode(s.document()))==s.document(),"Ungroup native codec exact");undo();check(s.document()==before_ungroup,"GUI Ungroup restores Group in one Undo");

    check(named_action(w,"stack-forward")->shortcut()==QKeySequence("Ctrl+]")&&named_action(w,"stack-back")->shortcut()==QKeySequence("Ctrl+Shift+["),"Stacking keyboard accelerators exposed");
}
void folder_action(Window& window) {
    auto& session=window.host.session;const auto composition=window.canvas->active_composition();
    const auto artboard=window.canvas->active_artboard();
    Point p0,p1,p2,p3;p0.id="folder-p0";p0.x.literal=80;p0.y.literal=80;
    p1.id="folder-p1";p1.x.literal=200;p1.y.literal=80;
    p2.id="folder-p2";p2.x.literal=200;p2.y.literal=180;
    p3.id="folder-p3";p3.x.literal=80;p3.y.literal=180;
    Contour contour{"folder-contour",true,{p0,p1,p2,p3}};
    session.apply({CreatePath{composition,"","folder-child","Child",{contour}},
        GroupContiguous{composition,"",{"folder-child"},"folder-parent","Parent"}},session.revision());
    window.host.edited();window.canvas->set_selection("folder-child");QApplication::processEvents();
    check(window.canvas->drill_scope()=="folder-parent","Selecting a child enters its current Folder drill scope");
    auto* tree=window.findChild<QTreeWidget*>();check(tree!=nullptr,"Folder action keeps the Structure tree available");
    QTreeWidgetItem* parent_item=nullptr;
    for(int i=0;i<tree->topLevelItemCount();++i)if(tree->topLevelItem(i)->data(0,Qt::UserRole)=="folder-parent")parent_item=tree->topLevelItem(i);
    check(parent_item!=nullptr,"Nested Folder parent appears in the Structure tree");
    parent_item->setExpanded(false);
    check(!parent_item->isExpanded(),"Folder action starts with its parent row collapsed");
    const auto before_render=Canvas::render_artboard(session.document(),composition,artboard,1,false);
    const auto revision=session.revision();named_action(window,"create-folder")->trigger();QApplication::processEvents();
    const auto folder=window.canvas->selected_object;
    check(session.revision()==revision+1&&folder.size()>0&&folder!="folder-parent"&&folder!="folder-child",
        "Create Folder uses one shared Session command and selects its stable new ID");
    check(session.document().objects.at("folder-parent").children==std::vector<Id>{"folder-child",folder},
        "Create Folder appends inside the current Canvas drill scope");
    check(session.document().objects.at(folder).kind==Kind::group&&session.document().objects.at(folder).name=="Folder"&&
        session.document().objects.at(folder).children.empty(),"Desktop action creates an empty Group named Folder");
    QTreeWidgetItem* folder_item=nullptr;
    QTreeWidgetItemIterator iterator(tree);while(*iterator){if((*iterator)->data(0,Qt::UserRole)==QString::fromStdString(folder))folder_item=*iterator;++iterator;}
    check(folder_item&&folder_item->isSelected()&&tree->currentItem()==folder_item,
        "New Folder is selected in the tree while its Canvas selection is active");
    parent_item=nullptr;
    for(int i=0;i<tree->topLevelItemCount();++i)if(tree->topLevelItem(i)->data(0,Qt::UserRole)=="folder-parent")parent_item=tree->topLevelItem(i);
    check(parent_item&&parent_item->isExpanded(),"Tree reveals the newly selected Folder inside its parent");
    const auto after_render=Canvas::render_artboard(session.document(),composition,artboard,1,false);
    check(before_render==after_render,"Creating an empty Folder leaves the rendered frame pixel-identical");
    const auto created=session.document();session.undo(session.revision());window.host.edited();
    check(session.document().objects.at("folder-parent").children==std::vector<Id>{"folder-child"}&&!session.document().objects.contains(folder),
        "Desktop Folder creation has one exact Undo boundary");
    session.redo(session.revision());window.host.edited();
    check(session.document()==created,"Desktop Folder creation has one exact Redo boundary");
    auto find_parent=[&]() -> QTreeWidgetItem* {
        QTreeWidgetItemIterator it(tree);
        while(*it) {if((*it)->data(0,Qt::UserRole)=="folder-parent"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())return *it;++it;}
        return nullptr;
    };
    const auto before_collapse=session.document();const auto collapse_revision=session.revision();
    const auto before_collapse_native=encode(before_collapse);
    const auto before_collapse_render=Canvas::render_artboard(before_collapse,composition,artboard,1,false);
    parent_item=find_parent();check(parent_item!=nullptr,"Parent Folder row exists before collapse");
    parent_item->setExpanded(false);QApplication::processEvents();
    check(!parent_item->isExpanded()&&session.revision()==collapse_revision&&session.document()==before_collapse&&
        encode(session.document())==before_collapse_native&&
        Canvas::render_artboard(session.document(),composition,artboard,1,false)==before_collapse_render,
        "Collapsing a Folder changes only Structure view state, not authored native or pixels");
    session.apply({Rename{"folder-child","Renamed child"}},session.revision());window.host.edited();QApplication::processEvents();
    parent_item=find_parent();check(parent_item&&!parent_item->isExpanded(),"Collapsed Folder remains collapsed across a tree rebuild");
    check(Canvas::render_artboard(session.document(),composition,artboard,1,false)==before_collapse_render,
        "Child rename under a collapsed Folder keeps rendered pixels unchanged");
    const auto rename_revision=session.revision();const auto renamed_native=encode(session.document());
    parent_item->setExpanded(true);QApplication::processEvents();
    check(parent_item->isExpanded()&&session.revision()==rename_revision&&encode(session.document())==renamed_native&&
        Canvas::render_artboard(session.document(),composition,artboard,1,false)==before_collapse_render,
        "Expanding a Folder changes only Structure view state");
}
void move_out_action(Window& window) {
    auto& session=window.host.session;const auto composition=window.canvas->active_composition();
    std::vector<Command> setup;
    for(const auto* id:{"move-out-x","move-out-a","move-out-b","move-out-c","move-out-y"}) {
        Point point;point.id=std::string(id)+"-point";point.x.literal=20;point.y.literal=40;
        setup.push_back(CreatePath{composition,"",id,id,{{std::string(id)+"-contour",false,{point}}}});
    }
    setup.push_back(GroupContiguous{composition,"",{"move-out-a","move-out-b","move-out-c"},"move-out-inner","Folder"});
    setup.push_back(GroupContiguous{composition,"",{"move-out-x","move-out-inner","move-out-y"},"move-out-outer","Folder"});
    setup.push_back(Set{{"move-out-inner","","transform.tx"},70});
    session.apply(setup,session.revision());window.host.edited();QApplication::processEvents();
    const auto before=session.document();const auto before_world=evaluate_transforms(before,evaluate(before));
    auto* move_out=named_action(window,"move-out-of-folder");
    check(move_out->text()=="Move selected out of Folder","Edit menu exposes the Move Out action by its bounded label");

    window.canvas->set_selection("move-out-b");QApplication::processEvents();const auto revision=session.revision();
    move_out->trigger();
    check(session.revision()==revision&&session.document()==before&&window.statusBar()->currentMessage().startsWith("MOVE_OUT_SELECTION"),
        "A middle child selection refuses with a scoped error and no authored delta");
    window.canvas->set_selection("move-out-a","move-out-a-point");QApplication::processEvents();move_out->trigger();
    check(session.revision()==revision&&session.document()==before&&window.statusBar()->currentMessage().startsWith("MOVE_OUT_SELECTION"),
        "Point selection refuses Move Out atomically");

    window.canvas->set_selection("move-out-a");QApplication::processEvents();move_out->trigger();QApplication::processEvents();
    check(session.revision()==revision+1&&session.document().objects.at("move-out-inner").children==std::vector<Id>{"move-out-b","move-out-c"}&&
        session.document().objects.at("move-out-outer").children==std::vector<Id>{"move-out-x","move-out-a","move-out-inner","move-out-y"},
        "Edit action extracts a selected prefix next to its nested Folder through one Session command");
    check(window.canvas->drill_scope()=="move-out-outer","Canvas drill scope follows the moved child to its new structural parent");
    const auto moved_world=evaluate_transforms(session.document(),evaluate(session.document()));
    for(const auto& [id,transform]:before_world)for(std::size_t i=0;i<6;++i)
        check(std::abs(transform.world[i]-moved_world.at(id).world[i])<1e-8,"Desktop Move Out preserves every world transform");
    auto* tree=window.findChild<QTreeWidget*>();QTreeWidgetItem* selected_item=nullptr;
    QTreeWidgetItemIterator iterator(tree);while(*iterator){if((*iterator)->data(0,Qt::UserRole)=="move-out-a"&&(*iterator)->data(0,Qt::UserRole+1).toString().isEmpty())selected_item=*iterator;++iterator;}
    check(selected_item!=nullptr,"Structure tree contains the extracted object row");
    check(selected_item->isSelected(),"Structure tree selects the extracted object row");
    check(tree->currentItem()==selected_item,"Structure tree makes the extracted object row current");
    const auto moved=session.document();session.undo(session.revision());window.host.edited();
    check(session.document()==before,"Desktop prefix Move Out has one exact Undo");
    session.redo(session.revision());window.host.edited();check(session.document()==moved,"Desktop prefix Move Out Redo restores the exact state");
    session.undo(session.revision());window.host.edited();

    window.canvas->set_selection("move-out-c");QApplication::processEvents();bool context_action=false;
    const auto context_revision=session.revision();
    QTimer::singleShot(0,&window,[&]{
        for(auto* widget:QApplication::topLevelWidgets())if(auto* menu=qobject_cast<QMenu*>(widget))
            for(auto* action:menu->actions())if(action->objectName()=="move-out-of-folder-context") {
                context_action=true;menu->setActiveAction(action);QTest::keyClick(menu,Qt::Key_Return);return;
            }
    });
    const auto point=window.canvas->rect().center();QContextMenuEvent context(QContextMenuEvent::Mouse,point,window.canvas->mapToGlobal(point));
    QApplication::sendEvent(window.canvas,&context);QApplication::processEvents();
    check(context_action&&session.revision()==context_revision+1&&session.document().objects.at("move-out-inner").children==std::vector<Id>{"move-out-a","move-out-b"}&&
        session.document().objects.at("move-out-outer").children==std::vector<Id>{"move-out-x","move-out-inner","move-out-c","move-out-y"},
        "Selection menu exposes and executes the same Move Out Session command for a suffix");
    check(window.canvas->drill_scope()=="move-out-outer","Selection-menu extraction also follows the new parent scope");
    session.undo(session.revision());window.host.edited();check(session.document()==before,"Selection-menu Move Out has one exact Undo");
}
void adjacent_folder_transfer_action(Window& window) {
    auto& session=window.host.session;const auto composition=window.canvas->active_composition();
    std::vector<Command> setup;
    for(const auto* id:{"transfer-a","transfer-b"}) {
        Point point;point.id=std::string(id)+"-point";point.x.literal=40;point.y.literal=50;
        setup.push_back(CreatePath{composition,"",id,id,{{std::string(id)+"-contour",false,{point}}}});
    }
    setup.push_back(GroupContiguous{composition,"",{"transfer-a","transfer-b"},"transfer-source","Folder"});
    setup.push_back(CreateFolder{composition,"","transfer-next","Folder"});
    setup.push_back(Set{{"transfer-source","","transform.tx"},70});
    session.apply(setup,session.revision());window.host.edited();QApplication::processEvents();
    const auto before=session.document();const auto before_world=evaluate_transforms(before,evaluate(before));
    const auto before_render=Canvas::render_artboard(before,composition,window.canvas->active_artboard(),1,false);
    auto* action=named_action(window,"move-to-next-folder");
    auto* reverse=named_action(window,"move-to-previous-folder");
    window.canvas->set_selection("transfer-a");QApplication::processEvents();const auto missing_previous_revision=session.revision();reverse->trigger();
    check(session.revision()==missing_previous_revision&&session.document()==before&&window.statusBar()->currentMessage().startsWith("FOLDER_TRANSFER_SELECTION"),
        "Reverse transfer refuses when no previous sibling Folder exists");
    window.canvas->set_selection("transfer-a");QApplication::processEvents();const auto revision=session.revision();action->trigger();
    check(session.revision()==revision&&session.document()==before&&window.statusBar()->currentMessage().startsWith("FOLDER_TRANSFER_SELECTION"),
        "Non-suffix Folder selection refuses without authored delta");
    window.canvas->set_selection("transfer-b","transfer-b-point");QApplication::processEvents();action->trigger();
    check(session.revision()==revision&&session.document()==before&&window.statusBar()->currentMessage().startsWith("FOLDER_TRANSFER_SELECTION"),
        "Point selection refuses adjacent Folder transfer");
    window.canvas->set_selection("transfer-b");QApplication::processEvents();action->trigger();QApplication::processEvents();
    check(session.revision()==revision+1&&session.document().objects.at("transfer-source").children==std::vector<Id>{"transfer-a"}&&
        session.document().objects.at("transfer-next").children==std::vector<Id>{"transfer-b"},
        "Desktop action moves a suffix to its next sibling Folder in one Session edit");
    check(window.canvas->drill_scope()=="transfer-next"&&window.canvas->selected_object=="transfer-b",
        "Canvas selection follows the moved object into its destination Folder");
    auto* tree=window.findChild<QTreeWidget*>();QTreeWidgetItem* selected_item=nullptr;
    QTreeWidgetItemIterator item(tree);while(*item){if((*item)->data(0,Qt::UserRole)=="transfer-b"&&(*item)->data(0,Qt::UserRole+1).toString().isEmpty())selected_item=*item;++item;}
    check(selected_item&&selected_item->isSelected()&&tree->currentItem()==selected_item,
        "Structure tree selects the moved object in its destination Folder");
    const auto after_world=evaluate_transforms(session.document(),evaluate(session.document()));
    for(const auto& [id,transform]:before_world)for(std::size_t i=0;i<6;++i)
        check(std::abs(transform.world[i]-after_world.at(id).world[i])<1e-8,"Desktop Folder transfer preserves every world transform");
    check(Canvas::render_artboard(session.document(),composition,window.canvas->active_artboard(),1,false)==before_render,
        "Adjacent Folder transfer leaves rendered pixels unchanged");
    check(decode(encode(session.document()))==session.document(),"Desktop Folder transfer survives native roundtrip");
    const auto moved=session.document();session.undo(session.revision());window.host.edited();
    check(session.document()==before,"Desktop Folder transfer has one exact Undo");
    session.redo(session.revision());window.host.edited();check(session.document()==moved,"Desktop Folder transfer has one exact Redo");
    session.undo(session.revision());window.host.edited();window.canvas->set_selection("transfer-b");QApplication::processEvents();
    bool context_enabled=false;
    QTimer::singleShot(0,&window,[&]{
        for(auto* widget:QApplication::topLevelWidgets())if(auto* menu=qobject_cast<QMenu*>(widget))
            for(auto* candidate:menu->actions())if(candidate->objectName()=="move-to-next-folder-context") {
                context_enabled=candidate->isEnabled();menu->setActiveAction(candidate);QTest::keyClick(menu,Qt::Key_Return);return;
            }
    });
    const auto point=window.canvas->rect().center();QContextMenuEvent context(QContextMenuEvent::Mouse,point,window.canvas->mapToGlobal(point));
    QApplication::sendEvent(window.canvas,&context);QApplication::processEvents();
    check(context_enabled&&session.document()==moved,"Selection menu reaches the same adjacent Folder transfer");
    check(reverse->text()=="Move selected to previous Folder","Edit menu exposes the reverse bounded transfer");
    bool reverse_context_enabled=false;
    QTimer::singleShot(0,&window,[&]{
        for(auto* widget:QApplication::topLevelWidgets())if(auto* menu=qobject_cast<QMenu*>(widget))
            for(auto* candidate:menu->actions())if(candidate->objectName()=="move-to-previous-folder-context") {
                reverse_context_enabled=candidate->isEnabled();menu->close();return;
            }
    });
    QContextMenuEvent reverse_context(QContextMenuEvent::Mouse,point,window.canvas->mapToGlobal(point));
    QApplication::sendEvent(window.canvas,&reverse_context);QApplication::processEvents();
    check(reverse_context_enabled,"Selection menu enables reverse transfer for a valid prefix");
    const auto reverse_revision_before=session.revision();
    window.canvas->set_selection("transfer-b","transfer-b-point");QApplication::processEvents();reverse->trigger();
    check(session.revision()==reverse_revision_before&&session.document()==moved&&window.statusBar()->currentMessage().startsWith("FOLDER_TRANSFER_SELECTION"),
        "Reverse transfer refuses point selection without authored delta");
    window.canvas->set_selection("transfer-b");QApplication::processEvents();
    const auto reverse_revision=session.revision();reverse->trigger();QApplication::processEvents();
    check(session.revision()==reverse_revision+1&&session.document().objects.at("transfer-source").children==std::vector<Id>{"transfer-a","transfer-b"}&&
        session.document().objects.at("transfer-next").children.empty(),"Reverse Edit action moves a prefix into the previous Folder");
    check(window.canvas->drill_scope()=="transfer-source"&&window.canvas->selected_object=="transfer-b",
        "Reverse transfer follows the selected object into its previous Folder");
    check(Canvas::render_artboard(session.document(),composition,window.canvas->active_artboard(),1,false)==before_render,
        "Reverse transfer preserves rendered pixels");
    const auto reverse_result=session.document();session.undo(session.revision());window.host.edited();
    check(session.document()==moved,"Reverse transfer is one exact Undo");
    session.redo(session.revision());window.host.edited();check(session.document()==reverse_result,"Reverse transfer is one exact Redo");
}
void explicit_folder_transfer_action(Window& window) {
    auto& session=window.host.session;const auto composition=window.canvas->active_composition();
    std::vector<Command> setup;
    auto path=[&](const char* id,double x) {
        Contour contour;contour.id=std::string(id)+"-contour";contour.closed=true;
        for(const auto& xy:std::vector<Vec2>{{x,80},{x+30,80},{x+30,110},{x,110}}) {
            Point point;point.id=std::string(id)+"-point-"+std::to_string(contour.points.size());
            point.x.literal=xy.x;point.y.literal=xy.y;contour.points.push_back(point);
        }
        setup.push_back(CreatePath{composition,"",id,id,{contour}});
    };
    path("chosen-a",30);path("chosen-b",80);path("chosen-c",130);path("chosen-d",180);path("barrier",230);
    setup.push_back(GroupContiguous{composition,"",{"chosen-a","chosen-b","chosen-c"},"source-folder","Source"});
    setup.push_back(CreateFolder{composition,"","empty-gap","Empty"});
    setup.push_back(GroupContiguous{composition,"",{"chosen-d"},"destination-folder","Destination"});
    setup.push_back(CreateFolder{composition,"","blocked-folder","Blocked"});
    setup.push_back(ReorderObjects{composition,"",{"source-folder","empty-gap","destination-folder","barrier","blocked-folder"}});
    session.apply(setup,session.revision());window.host.edited();QApplication::processEvents();
    auto* action=named_action(window,"move-to-folder");
    check(action->text()=="Move selected to Folder…","Edit menu exposes the explicit destination action");
    const auto before=session.document();const auto before_world=evaluate_transforms(before,evaluate(before));
    const auto before_render=Canvas::render_artboard(before,composition,window.canvas->active_artboard(),1,false);
    window.canvas->set_selection("chosen-b");QApplication::processEvents();action->trigger();
    check(session.document()==before&&window.statusBar()->currentMessage().startsWith("FOLDER_TRANSFER_SELECTION"),
        "Middle child selection refuses without authored delta");
    window.canvas->set_selection("chosen-c","chosen-c-point-0");QApplication::processEvents();action->trigger();
    check(session.document()==before&&window.statusBar()->currentMessage().startsWith("FOLDER_TRANSFER_SELECTION"),
        "Point selection refuses explicit Folder movement");
    window.canvas->set_selection("chosen-c");QApplication::processEvents();
    bool offered=false,blocked_offered=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        auto* combo=dialog->findChild<QComboBox*>();
        if(combo)for(int i=0;i<combo->count();++i) {
            if(combo->itemText(i).contains("destination-folder")){offered=true;combo->setCurrentIndex(i);}
            blocked_offered|=combo->itemText(i).contains("blocked-folder");
        }
        dialog->accept();return;
    }});
    const auto revision=session.revision();action->trigger();QApplication::processEvents();
    check(offered&&!blocked_offered,"Destination chooser offers the paint-order-safe Folder and excludes a Folder across a drawable");
    check(session.revision()==revision+1&&session.document().objects.at("source-folder").children==std::vector<Id>{"chosen-a","chosen-b"}&&
        session.document().objects.at("destination-folder").children==std::vector<Id>{"chosen-c","chosen-d"},
        "Explicit nonadjacent transfer moves the suffix in one Session edit");
    check(window.canvas->drill_scope()=="destination-folder"&&window.canvas->selected_object=="chosen-c",
        "Selection follows the explicitly chosen Folder");
    for(const auto& [id,transform]:before_world) {
        const auto after_world=evaluate_transforms(session.document(),evaluate(session.document())).at(id).world;
        for(std::size_t i=0;i<6;++i)check(std::abs(transform.world[i]-after_world[i])<1e-8,"Explicit transfer preserves world coordinates");
    }
    check(Canvas::render_artboard(session.document(),composition,window.canvas->active_artboard(),1,false)==before_render&&
        decode(encode(session.document()))==session.document(),"Explicit transfer preserves pixels and native state");
    const auto moved=session.document();session.undo(session.revision());window.host.edited();check(session.document()==before,"Explicit transfer is one exact Undo");
    session.redo(session.revision());window.host.edited();check(session.document()==moved,"Explicit transfer is one exact Redo");
    session.apply({SetVisibility{"barrier",false},
        LinkObjectVisibility{{"empty-gap","","object.visible"},{"barrier","","object.visible"},false}},session.revision());
    window.host.edited();window.canvas->set_selection("chosen-b");QApplication::processEvents();
    bool hidden_gap_offered=false,after_hidden_gap_offered=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        auto* combo=dialog->findChild<QComboBox*>();
        if(combo)for(int i=0;i<combo->count();++i) {
            hidden_gap_offered|=combo->itemText(i).contains("empty-gap");
            after_hidden_gap_offered|=combo->itemText(i).contains("destination-folder");
        }
        dialog->reject();return;
    }});
    const auto linked_hidden=session.document();const auto linked_revision=session.revision();action->trigger();QApplication::processEvents();
    check(hidden_gap_offered&&!after_hidden_gap_offered&&session.document()==linked_hidden&&session.revision()==linked_revision,
        "A linked-hidden intermediate Folder blocks Desktop transfer eligibility by evaluated own visibility");
    Point blocked_point;blocked_point.id="blocked-child-point";blocked_point.x.literal=260;blocked_point.y.literal=90;
    session.apply({CreatePath{composition,"blocked-folder","blocked-child","Blocked child",{{"blocked-child-contour",false,{blocked_point}}}}},session.revision());
    window.host.edited();window.canvas->set_selection("blocked-child");QApplication::processEvents();
    const auto blocked_document=session.document();const auto blocked_revision=session.revision();const auto blocked_history=session.history();
    action->trigger();
    check(session.document()==blocked_document&&session.revision()==blocked_revision&&session.history()==blocked_history&&
        window.statusBar()->currentMessage().startsWith("FOLDER_TRANSFER_ORDER"),
        "A painted sibling barrier leaves no eligible Folder and refuses without an authored delta");
}
void batch_folder_preview_action(Window& window) {
    auto& session=window.host.session;const auto composition=window.canvas->active_composition();
    std::vector<Command> setup;
    for(const auto* id:{"preview-u","preview-a","preview-b","preview-c","preview-v"}) {
        Point point;point.id=std::string(id)+"-point";point.x.literal=60;point.y.literal=60;
        setup.push_back(CreatePath{composition,"",id,id,{{std::string(id)+"-contour",false,{point}}}});
    }
    session.apply(setup,session.revision());window.host.edited();QApplication::processEvents();
    auto* action=named_action(window,"create-folder-from-selection");
    check(action->text()=="Create Folder from selected…","Edit menu exposes previewed batch Folder creation");
    const auto before=session.document();const auto before_history=session.history();
    const auto before_world=evaluate_transforms(before,evaluate(before));
    const auto before_render=Canvas::render_artboard(before,composition,window.canvas->active_artboard(),1,false);
    window.canvas->set_selections({{"preview-a",""},{"preview-c",""}});QApplication::processEvents();action->trigger();
    check(session.document()==before&&window.statusBar()->currentMessage().startsWith("FOLDER_GROUP_SELECTION"),
        "Discontiguous Folder preview selection refuses without authored delta");
    window.canvas->set_selection("preview-b","preview-b-point");QApplication::processEvents();action->trigger();
    check(session.document()==before&&window.statusBar()->currentMessage().startsWith("FOLDER_GROUP_SELECTION"),
        "Point selection refuses Folder preview");
    window.canvas->set_selections({{"preview-c",""},{"preview-a",""},{"preview-b",""}});QApplication::processEvents();
    bool context_enabled=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* menu=qobject_cast<QMenu*>(widget))
        for(auto* candidate:menu->actions())if(candidate->objectName()=="create-folder-from-selection-context") {
            context_enabled=candidate->isEnabled();menu->close();return;
        }
    });
    const auto point=window.canvas->rect().center();QContextMenuEvent context(QContextMenuEvent::Mouse,point,window.canvas->mapToGlobal(point));
    QApplication::sendEvent(window.canvas,&context);QApplication::processEvents();
    check(context_enabled,"Selection menu enables previewed Folder creation for contiguous siblings");
    bool preview_order=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("create-folder-from-selection-dialog");
        if(!dialog)return;
        const auto text=dialog->findChild<QPlainTextEdit*>("create-folder-from-selection-preview")->toPlainText();
        preview_order=text.indexOf("preview-a")<text.indexOf("preview-b")&&text.indexOf("preview-b")<text.indexOf("preview-c")&&
            text.contains("Flattened paint order: unchanged");
        dialog->reject();
    });
    action->trigger();QApplication::processEvents();
    check(preview_order&&session.document()==before&&session.history()==before_history,
        "Folder preview orders stable IDs by siblings and Cancel leaves Document and History unchanged");
    bool empty_refused=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("create-folder-from-selection-dialog");
        if(!dialog)return;
        auto* name=dialog->findChild<QLineEdit*>("create-folder-from-selection-name");
        auto* buttons=dialog->findChild<QDialogButtonBox*>();name->clear();buttons->button(QDialogButtonBox::Ok)->click();
        empty_refused=dialog->isVisible()&&dialog->findChild<QLabel*>("create-folder-from-selection-error")->text().contains("name")&&session.document()==before;
        name->setText("Artwork");buttons->button(QDialogButtonBox::Ok)->click();
    });
    const auto revision=session.revision();action->trigger();QApplication::processEvents();
    check(empty_refused&&session.revision()==revision+1,"Empty Folder name is rejected before one atomic Apply");
    const auto& result=session.document();const auto id=window.canvas->selected_object;
    check(result.compositions[0].roots==std::vector<Id>{"preview-u",id,"preview-v"}&&
        result.objects.at(id).children==std::vector<Id>{"preview-a","preview-b","preview-c"}&&
        result.objects.at(id).name=="Artwork","Previewed Folder replaces the contiguous block in sibling order");
    for(const auto* old_id:{"preview-u","preview-a","preview-b","preview-c","preview-v"})
        check(result.objects.at(old_id)==before.objects.at(old_id),"Existing authored objects and IDs remain exact");
    const auto after_world=evaluate_transforms(result,evaluate(result));
    for(const auto& [old_id,transform]:before_world)for(std::size_t i=0;i<6;++i)
        check(std::abs(transform.world[i]-after_world.at(old_id).world[i])<1e-8,"Batch Folder preserves world placement");
    check(Canvas::render_artboard(result,composition,window.canvas->active_artboard(),1,false)==before_render,
        "Batch Folder preserves rendered pixels");
    check(decode(encode(result))==result,"Previewed Folder survives native roundtrip");
    const auto grouped=result;session.undo(session.revision());window.host.edited();check(session.document()==before,"Previewed Folder has one exact Undo");
    session.redo(session.revision());window.host.edited();check(session.document()==grouped,"Previewed Folder has one exact Redo");
}
void batch_rename_action(Window& window) {
    auto& session=window.host.session;const auto composition=window.canvas->active_composition();
    std::vector<Command> setup;
    auto add_rectangle=[&](const std::string& id,const std::string& name,double x) {
        Contour contour;contour.id=id+"-contour";contour.closed=true;
        for(const auto& xy:std::vector<Vec2>{{x,80},{x+35,80},{x+35,115},{x,115}}) {
            Point point;point.id=id+"-point-"+std::to_string(contour.points.size());
            point.x.literal=xy.x;point.y.literal=xy.y;contour.points.push_back(point);
        }
        setup.push_back(CreatePath{composition,"",id,name,{contour}});
    };
    add_rectangle("batch-before","Before",30);
    add_rectangle("batch-a","Batch A",90);
    add_rectangle("batch-b","Batch B",150);
    add_rectangle("batch-c","Batch C",210);
    add_rectangle("batch-after","After",270);
    add_rectangle("cross-left","Cross left",330);
    add_rectangle("cross-inner","Cross inner",390);
    setup.push_back(GroupContiguous{composition,"",{"cross-left","cross-inner"},"cross-folder","Nested"});
    setup.push_back(Link{{"batch-c","batch-c-point-0","x"},{{"batch-a","batch-a-point-0","x"},2,3,"copy_local_value"}});
    session.apply(setup,session.revision());window.host.edited();QApplication::processEvents();

    auto* action=named_action(window,"batch-rename-selection");
    check(action->text()=="Batch rename selected…","Edit menu exposes the bounded Batch rename action");
    window.canvas->set_selections({{"batch-c",""},{"batch-a",""},{"batch-b",""}});QApplication::processEvents();
    check(action->isEnabled(),"Batch rename is enabled only for a valid sibling selection");
    auto* selection_menu_action=static_cast<QAction*>(nullptr);bool menu_action_enabled=false;
    QTimer::singleShot(0,&window,[&]{
        for(auto* widget:QApplication::topLevelWidgets())if(auto* menu=qobject_cast<QMenu*>(widget))
            for(auto* candidate:menu->actions())if(candidate->objectName()=="batch-rename-selection-context") {
                selection_menu_action=candidate;menu_action_enabled=candidate->isEnabled();menu->close();return;
            }
    });
    const auto menu_point=window.canvas->rect().center();
    QContextMenuEvent menu_event(QContextMenuEvent::Mouse,menu_point,window.canvas->mapToGlobal(menu_point));
    QApplication::sendEvent(window.canvas,&menu_event);QApplication::processEvents();
    check(selection_menu_action&&menu_action_enabled,"Selection menu offers Batch rename for the same valid sibling selection");

    const auto before=session.document();const auto before_revision=session.revision();
    const auto before_values=evaluate(before);const auto before_image=Canvas::render_artboard(before,composition,window.canvas->active_artboard(),1,false);
    auto* base_action=named_action(window,"batch-rename-selection");
    bool cancel_preview_ok=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("batch-rename-dialog");
        auto* base=dialog?dialog->findChild<QLineEdit*>("batch-rename-base"):nullptr;
        std::vector<QLineEdit*> names;
        for(int i=0;i<3;++i)names.push_back(dialog?dialog->findChild<QLineEdit*>(QString("batch-rename-name-%1").arg(i)):nullptr);
        cancel_preview_ok=dialog&&dialog->isVisible()&&base&&base->text()=="Batch A"&&
            std::all_of(names.begin(),names.end(),[](const auto* item){return item!=nullptr;});
        if(cancel_preview_ok) {
            cancel_preview_ok=names[0]->text()=="Batch A 1"&&names[1]->text()=="Batch A 2"&&names[2]->text()=="Batch A 3"&&
                dialog->findChild<QLabel*>("batch-rename-id-0")->text()=="batch-a"&&
                dialog->findChild<QLabel*>("batch-rename-id-1")->text()=="batch-b"&&
                dialog->findChild<QLabel*>("batch-rename-id-2")->text()=="batch-c"&&
                dialog->findChild<QLabel*>("batch-rename-old-0")->text()=="Batch A"&&
                dialog->findChild<QLabel*>("batch-rename-old-1")->text()=="Batch B"&&
                dialog->findChild<QLabel*>("batch-rename-old-2")->text()=="Batch C";
        }
        if(auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr)
            buttons->button(QDialogButtonBox::Cancel)->click();
    });
    base_action->trigger();QApplication::processEvents();
    check(cancel_preview_ok&&session.revision()==before_revision&&session.document()==before,
        "Batch rename previews IDs/names in structural order and Cancel leaves the Document untouched");

    bool applied_preview_ok=false;bool empty_refusal_ok=false;bool apply_clicked=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("batch-rename-dialog");
        auto* base=dialog?dialog->findChild<QLineEdit*>("batch-rename-base"):nullptr;
        std::vector<QLineEdit*> names;
        for(int i=0;i<3;++i)names.push_back(dialog?dialog->findChild<QLineEdit*>(QString("batch-rename-name-%1").arg(i)):nullptr);
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!base||!buttons||std::any_of(names.begin(),names.end(),[](auto* item){return item==nullptr;}))return;
        base->setText("First pass");QApplication::processEvents();
        applied_preview_ok=names[0]->text()=="First pass 1"&&names[1]->text()=="First pass 2"&&names[2]->text()=="First pass 3";
        auto replace=[&](QLineEdit* input,const char* text) {
            input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);QApplication::processEvents();
        };
        replace(names[1],"Duplicate");replace(names[2],"Duplicate");
        base->setText("Final");QApplication::processEvents();
        applied_preview_ok=applied_preview_ok&&names[0]->text()=="Final 1"&&names[1]->text()=="Duplicate"&&names[2]->text()=="Duplicate";
        names[0]->setFocus();QTest::keyClick(names[0],Qt::Key_A,Qt::ControlModifier);QTest::keyClick(names[0],Qt::Key_Backspace);
        buttons->button(QDialogButtonBox::Ok)->click();QApplication::processEvents();
        auto* error=dialog->findChild<QLabel*>("batch-rename-error");
        empty_refusal_ok=dialog->isVisible()&&error&&error->text().contains("non-empty")&&
            session.revision()==before_revision&&session.document()==before;
        replace(names[0],"Final 1");
        applied_preview_ok=applied_preview_ok&&names[1]->text()=="Duplicate"&&names[2]->text()=="Duplicate";
        buttons->button(QDialogButtonBox::Ok)->click();QApplication::processEvents();apply_clicked=true;
    });
    base_action->trigger();QApplication::processEvents();
    check(apply_clicked&&applied_preview_ok&&empty_refusal_ok,
        "Base edits update only untouched proposals, duplicate names are allowed, and empty names keep the dialog open without mutation");
    auto expected=before;
    expected.objects.at("batch-a").name="Final 1";
    expected.objects.at("batch-b").name="Duplicate";
    expected.objects.at("batch-c").name="Duplicate";
    check(session.revision()==before_revision+1&&session.document()==expected,
        "Batch Apply commits exactly three Rename commands through one Session revision and preserves stable IDs/references/order");
    check(evaluate(session.document())==before_values&&
        Canvas::render_artboard(session.document(),composition,window.canvas->active_artboard(),1,false)==before_image,
        "Batch rename preserves evaluated property values and rendered appearance");
    check(decode(encode(session.document()))==session.document(),"Batch-renamed source round-trips through native serialization");
    check(window.canvas->selected_objects()==std::vector<Id>{"batch-c","batch-a","batch-b"},
        "Batch rename retains the user's stable object selection");
    const auto after=session.document();
    QTemporaryDir saved_dir;check(saved_dir.isValid(),"Batch rename creates a disposable native save directory");
    const auto saved_path=saved_dir.path()+"/batch-renamed.nect";
    window.host.save(saved_path);
    Host reopened(saved_dir.path()+"/cold-recovery");reopened.open(saved_path);
    check(reopened.session.document()==after,"Batch rename saves and cold-reopens exact native names and stable references");
    session.undo(session.revision());window.host.edited();
    check(session.document()==before,"Batch rename is one exact Undo for all names");
    session.redo(session.revision());window.host.edited();
    check(session.document()==after,"Batch rename Redo restores the exact renamed state");

    Document stale_document;std::uint64_t stale_revision=0;bool stale_dialog_seen=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("batch-rename-dialog");
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(dialog&&buttons) {
            session.apply({Rename{"batch-after","External update"}},session.revision());window.host.edited();
            stale_document=session.document();stale_revision=session.revision();stale_dialog_seen=true;
            buttons->button(QDialogButtonBox::Ok)->click();
        }
    });
    base_action->trigger();QApplication::processEvents();
    check(stale_dialog_seen&&session.revision()==stale_revision&&session.document()==stale_document&&
        window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT"),
        "A revision-stale batch dialog refuses without partially renaming its targets");

    window.canvas->set_selection("batch-a");QApplication::processEvents();
    check(!base_action->isEnabled(),"Batch rename is not offered for fewer than two selected siblings");
    window.canvas->set_selections({{"batch-a","batch-a-point-0"},{"batch-b","batch-b-point-0"}});QApplication::processEvents();
    check(!base_action->isEnabled(),"Batch rename is not offered for point selections");
    window.canvas->set_selections({{"batch-a",""},{"cross-left",""}});QApplication::processEvents();
    check(!base_action->isEnabled(),"Batch rename is not offered for selected objects under different parents");

    window.canvas->set_selections({{"batch-a",""},{"batch-b",""}});QApplication::processEvents();
    bool session_stale_seen=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("batch-rename-dialog");
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(dialog&&buttons) {
            session_stale_seen=true;
            window.host.create_document();
            buttons->button(QDialogButtonBox::Ok)->click();
        }
    });
    base_action->trigger();QApplication::processEvents();
    check(session_stale_seen&&session.revision()==0&&session.document().objects.empty()&&
        window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT"),
        "A session-replaced batch dialog refuses without changing the new Document");
}
void sort_paint_order_action(Window& window) {
    auto& session=window.host.session;const auto composition=window.canvas->active_composition();
    std::vector<Command> setup;
    auto add=[&](const std::string& id,const std::string& name,double x) {
        Point point;point.id=id+"-point";point.x.literal=x;point.y.literal=120;
        setup.push_back(CreatePath{composition,"",id,name,{{id+"-contour",false,{point}}}});
    };
    add("sort-u","Untouched before",30);
    add("sort-c","Same",60);
    add("sort-x","Untouched middle",90);
    add("sort-b","alpha",120);
    add("sort-a","same",150);
    add("sort-v","Untouched after",180);
    setup.push_back(Link{{"sort-v","sort-v-point","x"},{{"sort-a","sort-a-point","x"},2,3,"copy_local_value"}});
    session.apply(setup,session.revision());window.host.edited();QApplication::processEvents();

    auto* action=named_action(window,"sort-selected-name-paint-order");
    check(action->text()=="Sort selected by name (paint order)…"&&!action->isEnabled(),
        "Edit menu exposes the explicit paint-order sort and starts disabled");
    window.canvas->set_selections({{"sort-a",""},{"sort-b",""},{"sort-c",""}});QApplication::processEvents();
    check(action->isEnabled(),"Paint-order sort is enabled for whole sibling selections regardless of click order");

    bool menu_found=false,menu_enabled=false;
    QTimer::singleShot(0,&window,[&]{
        for(auto* widget:QApplication::topLevelWidgets())if(auto* menu=qobject_cast<QMenu*>(widget))
            for(auto* candidate:menu->actions())if(candidate->objectName()=="sort-selected-name-paint-order-context") {
                menu_found=true;menu_enabled=candidate->isEnabled();menu->close();return;
            }
    });
    const auto menu_point=window.canvas->rect().center();
    QContextMenuEvent menu_event(QContextMenuEvent::Mouse,menu_point,window.canvas->mapToGlobal(menu_point));
    QApplication::sendEvent(window.canvas,&menu_event);QApplication::processEvents();
    check(menu_found&&menu_enabled,"Selection menu offers the same paint-order sort for valid siblings");

    const auto before=session.document();const auto before_revision=session.revision();
    const auto before_values=evaluate(before);
    bool cancel_preview_ok=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("sort-paint-order-dialog");
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        const std::vector<std::string> old_ids{"sort-u","sort-c","sort-x","sort-b","sort-a","sort-v"};
        const std::vector<std::string> old_names{"Untouched before","Same","Untouched middle","alpha","same","Untouched after"};
        const std::vector<std::string> new_ids{"sort-u","sort-b","sort-x","sort-c","sort-a","sort-v"};
        const std::vector<std::string> new_names{"Untouched before","alpha","Untouched middle","Same","same","Untouched after"};
        cancel_preview_ok=dialog&&dialog->isVisible()&&buttons;
        if(cancel_preview_ok) {
            auto label_text=[&](const QString& name) {auto* label=dialog->findChild<QLabel*>(name);return label?label->text():QString();};
            auto* heading=dialog->findChild<QLabel*>("sort-paint-order-heading");
            cancel_preview_ok=heading&&heading->text().contains("actual draw / paint order");
            for(std::size_t i=0;i<old_ids.size()&&cancel_preview_ok;++i) {
                cancel_preview_ok=label_text(QString("sort-paint-order-before-id-%1").arg(i))==QString::fromStdString(old_ids[i])&&
                    label_text(QString("sort-paint-order-before-name-%1").arg(i))==QString::fromStdString(old_names[i])&&
                    label_text(QString("sort-paint-order-after-id-%1").arg(i))==QString::fromStdString(new_ids[i])&&
                    label_text(QString("sort-paint-order-after-name-%1").arg(i))==QString::fromStdString(new_names[i]);
            }
            buttons->button(QDialogButtonBox::Cancel)->click();
        }
    });
    action->trigger();QApplication::processEvents();
    check(cancel_preview_ok&&session.revision()==before_revision&&session.document()==before,
        "Preview lists every full sibling slot with before/after stable IDs and names, while Cancel is history-free");

    bool context_applied=false,context_preview_ok=false,context_menu_enabled=false;
    QTimer::singleShot(0,&window,[&]{
        for(auto* widget:QApplication::topLevelWidgets())if(auto* menu=qobject_cast<QMenu*>(widget))
            for(auto* candidate:menu->actions())if(candidate->objectName()=="sort-selected-name-paint-order-context") {
                context_menu_enabled=candidate->isEnabled();
                QTimer::singleShot(0,&window,[&]{
                    auto* dialog=window.findChild<QDialog*>("sort-paint-order-dialog");
                    auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
                    if(dialog) {
                        auto label_text=[&](const char* name) {auto* label=dialog->findChild<QLabel*>(name);return label?label->text():QString();};
                        context_preview_ok=buttons&&label_text("sort-paint-order-before-id-1")=="sort-c"&&
                            label_text("sort-paint-order-after-id-1")=="sort-b"&&label_text("sort-paint-order-after-id-3")=="sort-c";
                    }
                    if(buttons) {buttons->button(QDialogButtonBox::Ok)->click();context_applied=true;}
                });
                menu->setActiveAction(candidate);QTest::keyClick(menu,Qt::Key_Return);return;
            }
    });
    QContextMenuEvent sort_event(QContextMenuEvent::Mouse,menu_point,window.canvas->mapToGlobal(menu_point));
    QApplication::sendEvent(window.canvas,&sort_event);QApplication::processEvents();
    const std::vector<Id> expected_order{"sort-u","sort-b","sort-x","sort-c","sort-a","sort-v"};
    auto expected=before;expected.compositions.front().roots=expected_order;
    check(context_applied&&context_menu_enabled&&context_preview_ok&&session.revision()==before_revision+1&&session.document()==expected,
        "Selection-menu Apply sorts only selected slots by case-insensitive name with stable equal-name order");
    check(evaluate(session.document())==before_values&&session.document().objects==before.objects&&
        window.canvas->selected_objects()==std::vector<Id>{"sort-a","sort-b","sort-c"},
        "Paint-order sort preserves object data, references, evaluated values and the original selection");
    check(decode(encode(session.document()))==session.document(),"Paint-order sort survives native serialization");
    QTemporaryDir saved_dir;check(saved_dir.isValid(),"Paint-order sort creates a disposable native save directory");
    const auto saved_path=saved_dir.path()+"/sorted-paint-order.nect";window.host.save(saved_path);
    Host reopened(saved_dir.path()+"/cold-recovery");reopened.open(saved_path);
    check(reopened.session.document()==session.document(),"Native save and independent reopen retain the full sorted sibling order");

    const auto sorted=session.document();const auto sorted_revision=session.revision();
    session.undo(sorted_revision);window.host.edited();
    check(session.document()==before,"Paint-order sort is one exact Undo");
    session.redo(session.revision());window.host.edited();
    check(session.document()==sorted,"Paint-order Redo restores the exact sorted order");
    const auto redone_revision=session.revision();
    window.canvas->set_selections({{"sort-a",""},{"sort-b",""},{"sort-c",""}});QApplication::processEvents();
    check(action->isEnabled(),"Sorted selection remains eligible after history navigation");

    bool no_op_preview=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("sort-paint-order-dialog");
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(dialog) {
            auto* before=dialog->findChild<QLabel*>("sort-paint-order-before-id-1");
            auto* after=dialog->findChild<QLabel*>("sort-paint-order-after-id-1");
            no_op_preview=buttons&&before&&after&&before->text()=="sort-b"&&after->text()=="sort-b";
        }
        if(buttons)buttons->button(QDialogButtonBox::Ok)->click();
    });
    action->trigger();QApplication::processEvents();
    check(no_op_preview&&session.revision()==redone_revision&&session.document()==sorted&&
        window.statusBar()->currentMessage().startsWith("No change"),
        "Applying an already sorted selection reports no change without a revision or history entry");

    Point nested_z,nested_a;nested_z.id="sort-nested-z-point";nested_z.x.literal=120;nested_z.y.literal=130;
    nested_a.id="sort-nested-a-point";nested_a.x.literal=160;nested_a.y.literal=130;
    session.apply({CreatePath{composition,"","sort-nested-z","zulu",{{"sort-nested-z-contour",false,{nested_z}}}},
        CreatePath{composition,"","sort-nested-a","Beta",{{"sort-nested-a-contour",false,{nested_a}}}},
        GroupContiguous{composition,"",{"sort-nested-z","sort-nested-a"},"sort-folder","Folder"}},session.revision());
    window.host.edited();QApplication::processEvents();
    const auto before_nested=session.document();const auto nested_revision=session.revision();
    window.canvas->set_selections({{"sort-nested-z",""},{"sort-nested-a",""}});QApplication::processEvents();
    check(action->isEnabled(),"Paint-order sort accepts sibling children of a Folder");
    bool nested_preview_ok=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("sort-paint-order-dialog");
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(dialog) {
            auto* before=dialog->findChild<QLabel*>("sort-paint-order-before-id-0");
            auto* after=dialog->findChild<QLabel*>("sort-paint-order-after-id-0");
            nested_preview_ok=buttons&&before&&after&&before->text()=="sort-nested-z"&&after->text()=="sort-nested-a";
        }
        if(buttons)buttons->button(QDialogButtonBox::Ok)->click();
    });
    action->trigger();QApplication::processEvents();
    auto expected_nested=before_nested;expected_nested.objects.at("sort-folder").children={"sort-nested-a","sort-nested-z"};
    check(nested_preview_ok&&session.revision()==nested_revision+1&&session.document()==expected_nested&&
        session.document().compositions.front().roots==before_nested.compositions.front().roots,
        "Nested Folder sort changes only the selected child sibling order");

    window.canvas->set_selection("sort-a");QApplication::processEvents();
    check(!action->isEnabled(),"Paint-order sort is disabled for fewer than two selected siblings");
    window.canvas->set_selections({{"sort-a","sort-a-point"},{"sort-b",""}});QApplication::processEvents();
    check(!action->isEnabled(),"Paint-order sort is disabled for point selections");
    window.canvas->set_selections({{"sort-a",""},{"sort-nested-a",""}});QApplication::processEvents();
    check(!action->isEnabled(),"Paint-order sort is disabled for selections under different parents");

    window.canvas->set_selections({{"sort-a",""},{"sort-b",""}});QApplication::processEvents();
    Document stale_document;std::uint64_t stale_revision=0;bool stale_dialog_seen=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("sort-paint-order-dialog");
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(dialog&&buttons) {
            session.apply({Rename{"sort-v","External update"}},session.revision());window.host.edited();
            stale_document=session.document();stale_revision=session.revision();stale_dialog_seen=true;
            buttons->button(QDialogButtonBox::Ok)->click();
        }
    });
    action->trigger();QApplication::processEvents();
    check(stale_dialog_seen&&session.revision()==stale_revision&&session.document()==stale_document&&
        window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT"),
        "A revision-stale paint-order preview refuses without changing the updated Document");

    bool session_stale_seen=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("sort-paint-order-dialog");
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(dialog&&buttons) {session_stale_seen=true;window.host.create_document();buttons->button(QDialogButtonBox::Ok)->click();}
    });
    action->trigger();QApplication::processEvents();
    check(session_stale_seen&&session.revision()==0&&session.document().objects.empty()&&
        window.statusBar()->currentMessage().startsWith("SESSION_CONFLICT"),
        "A Session-replaced paint-order preview refuses without mutating the new Document");
}
void history_action(Window& window,const char* text) {
    for(auto* action:window.findChildren<QAction*>())if(action->text()==QString::fromLatin1(text)) {
        check(action->isEnabled(),"History action is enabled");action->trigger();QApplication::processEvents();return;
    }
    throw std::runtime_error("History action missing");
}
void reveal(Window& window,QWidget* widget) {
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
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
    window.canvas->set_selection(object);window.host.edited();QApplication::processEvents();
    named_action(window,"add-stroke")->trigger();QApplication::processEvents();
    const auto circle_stroke=session.document().objects.at(object).stack.back().id;
    auto* circle_cap=visible_child<QComboBox>(window,("stroke-line-cap-"+circle_stroke).c_str());reveal(window,circle_cap);
    circle_cap->setCurrentIndex(circle_cap->findData("round"));QApplication::processEvents();
    check(session.document().objects.at(object).source&&session.document().objects.at(object).stack.back().line_cap=="round"&&
        session.document().objects.at(object).stack.back().version==2,
        "Circle source retains topology while its Stroke Inspector commits style");
    window.canvas->set_selection(rectangle);window.host.edited();QApplication::processEvents();
}
void stack_authoring(Window& window) {
    auto& session=window.host.session;
    const auto object=window.canvas->selected_object;
    const auto original_stack_size=session.document().objects.at(object).stack.size();
    auto last_operation=[&] {return session.document().objects.at(object).stack.back().id;};
    auto edit_hex=[&](const Id& operation,const char* text) {
        const auto name="operation-hex-"+operation;
        auto* input=visible_child<QLineEdit>(window,name.c_str());reveal(window,input);input->setFocus();
        QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);
        QTest::keyClick(input,Qt::Key_Return);QApplication::processEvents();
    };
    auto control=[&](const char* prefix,const Id& operation) {
        const auto name=std::string(prefix)+operation;
        auto* button=visible_child<QPushButton>(window,name.c_str());reveal(window,button);
        QTest::mouseClick(button,Qt::LeftButton);QApplication::processEvents();
    };
    auto choose=[&](const char* prefix,const Id& operation,int index) {
        const auto name=std::string(prefix)+operation;
        auto* combo=visible_child<QComboBox>(window,name.c_str());reveal(window,combo);
        combo->setCurrentIndex(index);QApplication::processEvents();
    };
    auto screen=[&](double x,double y) {
        const auto& artboard=session.document().compositions.front().artboards.front();
        return QPoint(qRound(window.canvas->width()/2.0+(x-artboard.x-artboard.width/2)*window.canvas->zoom()),
            qRound(window.canvas->height()/2.0+(y-artboard.y-artboard.height/2)*window.canvas->zoom()));
    };
    auto pixel=[&](double x,double y) {
        window.canvas->fit_artboard();QApplication::processEvents();
        const auto pixmap=window.canvas->grab();const auto image=pixmap.toImage();
        const auto position=QPointF(screen(x,y))*pixmap.devicePixelRatio();
        return image.pixelColor(qRound(position.x()),qRound(position.y()));
    };
    named_action(window,"add-fill")->trigger();QApplication::processEvents();const auto red=last_operation();
    check(session.document().objects.at(object).stack.size()==original_stack_size+1,
        "Add Fill appends a real stack operation through the production action");
    edit_hex(red,"#EF3340FF");
    check(std::abs(evaluate(session.document()).at(operation_ref(object,red,"r"))-239.0/255)<1e-5,
        "HEX RGBA edits address the real operation Scalars");
    named_action(window,"add-fill")->trigger();QApplication::processEvents();const auto blue=last_operation();
    edit_hex(blue,"#2040E0FF");
    auto color=pixel(480,320);
    check(std::abs(color.red()-239)<=2&&std::abs(color.blue()-64)<=2,
        "Canvas paints earlier default Fill above a later Fill");
    choose("operation-composite-",blue,1);
    color=pixel(480,320);
    check(std::abs(color.red()-32)<=2&&std::abs(color.blue()-224)<=2,
        "Composite Above changes the actual Canvas paint order");
    auto fill_driver_button=[&] {
        const auto buttons=window.findChildren<QToolButton*>(QString::fromStdString("operation-fill-rule-driver-"+blue));
        check(!buttons.empty(),"Fill rule driver button exists for the selected operation");
        auto* button=buttons.back();reveal(window,button);
        check(button->isVisible(),"Current Fill rule driver button is visible after scrolling");return button;
    };
    auto* fill_driver=fill_driver_button();
    const auto fill_ref=operation_ref(object,blue,"fill_rule");
    const auto fill_revision=session.revision();bool fill_cancel_staged=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>(QString::fromStdString("fill-rule-dialog-"+blue));
        auto* value=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("fill-rule-value-"+blue)):nullptr;
        if(!dialog||!value){if(dialog)dialog->reject();else if(auto* active=qobject_cast<QDialog*>(QApplication::activeModalWidget()))active->reject();return;}
        value->setCurrentIndex(1);fill_cancel_staged=session.revision()==fill_revision&&
            fill_rule_property(session.document(),fill_ref).literal=="nonzero";dialog->reject();});
    fill_driver->menu()->actions().front()->trigger();QApplication::processEvents();
    check(fill_cancel_staged&&session.revision()==fill_revision&&
        fill_rule_property(session.document(),fill_ref).literal=="nonzero",
        "Fill rule Inspector stages its literal and Cancel leaves Session bytes and revision unchanged");
    fill_driver=fill_driver_button();bool fill_apply_modal=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>(QString::fromStdString("fill-rule-dialog-"+blue));
        auto* value=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("fill-rule-value-"+blue)):nullptr;
        fill_apply_modal=dialog&&value;
        if(!dialog||!value){if(dialog)dialog->reject();else if(auto* active=qobject_cast<QDialog*>(QApplication::activeModalWidget()))active->reject();return;}
        value->setCurrentIndex(1);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
    fill_driver->menu()->actions().front()->trigger();QApplication::processEvents();
    check(fill_apply_modal&&fill_rule_property(session.document(),fill_ref).literal=="evenodd"&&session.revision()==fill_revision+1,
        "Applying a staged Fill rule literal commits one shared Session revision");
    const Ref source_fill_ref=operation_ref(object,red,"fill_rule");
    session.apply({OperationOptions{object,red,"below","evenodd"}},session.revision());window.host.edited();QApplication::processEvents();
    fill_driver=fill_driver_button();
    const auto link_revision=session.revision();bool link_draft_staged=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>(QString::fromStdString("fill-rule-dialog-"+blue));
        auto* mode=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("fill-rule-mode-"+blue)):nullptr;
        auto* source=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("fill-rule-source-"+blue)):nullptr;
        if(!dialog||!mode||!source){if(dialog)dialog->reject();return;}
        mode->setCurrentIndex(mode->findData("link"));source->setCurrentIndex(0);
        link_draft_staged=session.revision()==link_revision&&!fill_rule_property(session.document(),fill_ref).driver;
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
    fill_driver->menu()->actions().front()->trigger();QApplication::processEvents();
    check(link_draft_staged&&fill_rule_property(session.document(),fill_ref).driver==FillRuleDriver{source_fill_ref}&&
        fill_rule_property(session.document(),fill_ref).evaluated=="evenodd",
        "Fill Inspector stages a same-field source and commits it through Session");
    session.apply({OperationOptions{object,red,"below","nonzero"}},session.revision());window.host.edited();
    check(fill_rule_property(session.document(),fill_ref).evaluated=="nonzero","Fill Inspector link follows source edits");
    fill_driver=fill_driver_button();
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>(QString::fromStdString("fill-rule-dialog-"+blue));
        auto* mode=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("fill-rule-mode-"+blue)):nullptr;
        if(!dialog||!mode){if(dialog)dialog->reject();return;}
        mode->setCurrentIndex(mode->findData("unlink"));dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
    fill_driver->menu()->actions().front()->trigger();QApplication::processEvents();
    session.apply({OperationOptions{object,red,"below","evenodd"}},session.revision());window.host.edited();
    check(!fill_rule_property(session.document(),fill_ref).driver&&fill_rule_property(session.document(),fill_ref).literal=="nonzero"&&
        fill_rule_property(session.document(),fill_ref).evaluated=="nonzero",
        "Fill Inspector unlink freezes the evaluated choice against later source edits");
    const auto source_object=std::find_if(session.document().objects.begin(),session.document().objects.end(),
        [&](const auto& entry){return entry.first!=object&&entry.second.name=="Source";});
    check(source_object!=session.document().objects.end()&&!source_object->second.stack.empty(),
        "A second operation in the selected object's Composition can drive enabled state");
    const auto source_object_id=source_object->first;
    const auto source_operation=source_object->second.stack.front().id;
    const Ref source_enabled=operation_ref(source_object_id,source_operation,"enabled");
    const Ref target_enabled=operation_ref(object,blue,"enabled");
    session.apply({EnableOperation{source_object_id,source_operation,false}},session.revision());window.host.edited();QApplication::processEvents();
    auto enabled_driver_button=[&] {
        auto* button=visible_child<QPushButton>(window,("operation-enabled-driver-"+blue).c_str());
        reveal(window,button);
        check(button->isEnabled(),"Operation enabled source picker remains enabled for a Composition-wide source");return button;
    };
    auto* enabled_driver=enabled_driver_button();const auto operation_link_revision=session.revision();bool enabled_cancel_staged=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>(QString::fromStdString("operation-enabled-dialog-"+blue));
        auto* mode=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("operation-enabled-mode-"+blue)):nullptr;
        auto* source=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("operation-enabled-source-"+blue)):nullptr;
        if(!dialog||!mode||!source){if(dialog)dialog->reject();return;}
        const auto label=QString("[%1]").arg(QString::fromStdString(source_operation));int selected=-1;
        for(int index=0;index<source->count();++index)if(source->itemText(index).contains(label))selected=index;
        if(selected<0){dialog->reject();return;}
        mode->setCurrentIndex(mode->findData("link"));source->setCurrentIndex(selected);
        enabled_cancel_staged=session.revision()==operation_link_revision&&!operation_enabled_state(session.document(),target_enabled).driver;
        dialog->reject();});
    QTest::mouseClick(enabled_driver,Qt::LeftButton);QApplication::processEvents();
    check(enabled_cancel_staged&&session.revision()==operation_link_revision&&!operation_enabled_state(session.document(),target_enabled).driver,
        "Operation enabled Inspector Cancel keeps the Session and authored target unchanged");
    enabled_driver=enabled_driver_button();
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>(QString::fromStdString("operation-enabled-dialog-"+blue));
        auto* mode=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("operation-enabled-mode-"+blue)):nullptr;
        auto* source=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("operation-enabled-source-"+blue)):nullptr;
        if(!dialog||!mode||!source){if(dialog)dialog->reject();return;}
        const auto label=QString("[%1]").arg(QString::fromStdString(source_operation));int selected=-1;
        for(int index=0;index<source->count();++index)if(source->itemText(index).contains(label))selected=index;
        if(selected<0){dialog->reject();return;}
        mode->setCurrentIndex(mode->findData("link"));source->setCurrentIndex(selected);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
    QTest::mouseClick(enabled_driver,Qt::LeftButton);QApplication::processEvents();
    check(operation_enabled_state(session.document(),target_enabled).driver==source_enabled&&
        !operation_enabled_state(session.document(),target_enabled).evaluated&&session.revision()==operation_link_revision+1,
        "Operation enabled Inspector links to another object in the owning Composition");
    auto* linked_checkbox=visible_child<QCheckBox>(window,("operation-enabled-"+blue).c_str());
    check(!linked_checkbox->isEnabled(),"A linked enabled checkbox prevents implicit literal edits");
    session.apply({EnableOperation{source_object_id,source_operation,true}},session.revision());window.host.edited();QApplication::processEvents();
    check(operation_enabled_state(session.document(),target_enabled).evaluated,
        "Operation enabled Inspector follows a source edit from another object");
    enabled_driver=enabled_driver_button();
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>(QString::fromStdString("operation-enabled-dialog-"+blue));
        auto* mode=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("operation-enabled-mode-"+blue)):nullptr;
        if(!dialog||!mode){if(dialog)dialog->reject();return;}
        mode->setCurrentIndex(mode->findData("unlink"));dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
    QTest::mouseClick(enabled_driver,Qt::LeftButton);QApplication::processEvents();
    session.apply({EnableOperation{source_object_id,source_operation,false}},session.revision());window.host.edited();
    check(!operation_enabled_state(session.document(),target_enabled).driver&&
        operation_enabled_state(session.document(),target_enabled).literal&&operation_enabled_state(session.document(),target_enabled).evaluated,
        "Operation enabled Inspector unlink freezes the evaluated value against later source edits");
    const auto enabled_name="operation-enabled-"+blue;
    const auto enabled_widgets=window.findChildren<QCheckBox*>(QString::fromStdString(enabled_name));
    check(!enabled_widgets.empty(),"Enabled checkbox exists for the selected Fill operation");
    auto* enabled=enabled_widgets.back();reveal(window,enabled);
    check(enabled->isVisible(),"Current Fill enabled checkbox is visible after scrolling");
    QTest::mouseClick(enabled,Qt::LeftButton,Qt::NoModifier,QPoint(8,enabled->height()/2));QApplication::processEvents();
    color=pixel(480,320);
    check(std::abs(color.red()-239)<=2,"Disabling a paint removes its actual rendered contribution");
    history_action(window,"Undo");
    control("operation-up-",blue);
    const auto& reordered=session.document().objects.at(object).stack;
    check(reordered[original_stack_size].id==blue&&reordered[original_stack_size+1].id==red&&
        std::abs(evaluate(session.document()).at(operation_ref(object,blue,"b"))-224.0/255)<1e-5,
        "Stack reorder preserves stable operation references and authored values");
    control("operation-remove-",blue);
    check(session.document().objects.at(object).stack.size()==original_stack_size+1,
        "Remove deletes only the selected stack entry");

    named_action(window,"add-repeater")->trigger();QApplication::processEvents();const auto repeater=last_operation();
    edit_number(window,operation_ref(object,repeater,"copies"),"3");
    window.canvas->fit_artboard();QApplication::processEvents();
    const auto copy_position=screen(760,320);
    QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,copy_position);QApplication::processEvents();
    check(window.canvas->selected_object==object&&window.canvas->selected_point.empty(),
        "A painted virtual copy selects its owner without inventing point identities");
    color=pixel(760,320);
    check(std::abs(color.red()-239)<=2&&std::abs(color.blue()-64)<=2,
        "Core-derived repeated paint appears outside the source rectangle");
    control("operation-up-",repeater);
    const auto values=evaluate(session.document());
    const auto shape=evaluate_shape(session.document(),object,values);
    check(shape.paths.size()==3,"Moving Repeater before paint keeps core-owned repeated geometry");
    named_action(window,"add-stroke")->trigger();QApplication::processEvents();const auto stroke=last_operation();
    check(session.document().objects.at(object).stack.back().type=="nect.paint.stroke","Add Stroke is a real additional paint");
    edit_number(window,operation_ref(object,stroke,"width"),"7");
    check(evaluate(session.document()).at(operation_ref(object,stroke,"width"))==7,
        "Additional Stroke width is independently addressable in the Inspector");
    auto* enable_miter=visible_child<QPushButton>(window,("stroke-enable-miter-"+stroke).c_str());reveal(window,enable_miter);
    const auto promotion_revision=session.revision();QTest::mouseClick(enable_miter,Qt::LeftButton);QApplication::processEvents();
    check(session.revision()==promotion_revision+1&&session.document().objects.at(object).stack.back().version==2&&
        evaluate(session.document()).at(operation_ref(object,stroke,"miter_limit"))==4,
        "Native v1 Stroke promotes to editable v2 miter without changing appearance");
    auto* stale_cap=visible_child<QComboBox>(window,("stroke-line-cap-"+stroke).c_str());reveal(window,stale_cap);
    const auto stale_revision=session.revision();session.apply({Set{operation_ref(object,stroke,"width"),8}},stale_revision);
    stale_cap->setCurrentIndex(stale_cap->findData("round"));QApplication::processEvents();
    check(session.revision()==stale_revision+1&&session.document().objects.at(object).stack.back().line_cap=="butt"&&
        window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT"),
        "A stale Stroke Inspector handler rejects without retargeting the operation");
    window.host.edited();QApplication::processEvents();
    const auto miter_ref=operation_ref(object,stroke,"miter_limit");
    session.apply({SetExpression{{miter_ref},{"4 + 4",1},false}},session.revision());window.host.edited();QApplication::processEvents();
    auto* cap=visible_child<QComboBox>(window,("stroke-line-cap-"+stroke).c_str());reveal(window,cap);
    cap->setCurrentIndex(cap->findData("round"));QApplication::processEvents();
    check(session.document().objects.at(object).stack.back().line_cap=="round"&&
        evaluate(session.document()).at(miter_ref)==8&&nect::property(session.document(),miter_ref).expression.has_value(),
        "Changing cap preserves an expression-driven evaluated miter");
    cap=visible_child<QComboBox>(window,("stroke-line-cap-"+stroke).c_str());reveal(window,cap);
    const auto no_op_revision=session.revision();cap->setCurrentIndex(cap->findData("butt"));QApplication::processEvents();
    check(session.document().objects.at(object).stack.back().line_cap=="butt"&&session.revision()==no_op_revision+1,
        "Inspector line-cap control commits shared StrokeStyle");
    cap=visible_child<QComboBox>(window,("stroke-line-cap-"+stroke).c_str());reveal(window,cap);
    const auto same_cap_revision=session.revision();cap->setCurrentIndex(cap->findData("butt"));QApplication::processEvents();
    check(session.revision()==same_cap_revision,"Choosing the existing line cap is a history-free no-op");
    auto* join=visible_child<QComboBox>(window,("stroke-line-join-"+stroke).c_str());reveal(window,join);join->setCurrentIndex(join->findData("bevel"));QApplication::processEvents();
    check(session.document().objects.at(object).stack.back().line_join=="bevel"&&
        evaluate(session.document()).at(operation_ref(object,stroke,"miter_limit"))==8&&
        nect::property(session.document(),miter_ref).expression.has_value(),
        "Inspector line-join control preserves the evaluated miter limit");
}
void single_operation_enabled_source(Window& window) {
    auto& session=window.host.session;const auto composition=session.document().compositions.front().id;
    Point target_point,source_point;target_point.id="single-target-point";source_point.id="single-source-point";
    target_point.x.literal=70;target_point.y.literal=80;source_point.x.literal=180;source_point.y.literal=80;
    session.apply({CreatePath{composition,"","single-target","Single target",{{"single-target-contour",false,{target_point}}}},
        CreatePath{composition,"","single-source","Single source",{{"single-source-contour",false,{source_point}}}}},0);
    const auto& target_object=session.document().objects.at("single-target");
    const auto& source_object=session.document().objects.at("single-source");
    check(target_object.stack.size()==1&&source_object.stack.size()==1,
        "The Composition source-picker fixture has exactly one operation on each Object");
    const auto target_operation=target_object.stack.front().id;
    const auto source_operation=source_object.stack.front().id;
    const Ref target=operation_ref("single-target",target_operation,"enabled");
    const Ref source=operation_ref("single-source",source_operation,"enabled");
    session.apply({EnableOperation{"single-source",source_operation,false}},session.revision());
    window.canvas->set_selection("single-target");window.host.edited();QApplication::processEvents();
    auto* link=visible_child<QPushButton>(window,("operation-enabled-driver-"+target_operation).c_str());
    reveal(window,link);
    check(link->isEnabled(),"Link stays enabled when target Object has one operation and another Object is the source");
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>(QString::fromStdString("operation-enabled-dialog-"+target_operation));
        auto* mode=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("operation-enabled-mode-"+target_operation)):nullptr;
        auto* source_combo=dialog?dialog->findChild<QComboBox*>(QString::fromStdString("operation-enabled-source-"+target_operation)):nullptr;
        if(!dialog||!mode||!source_combo){if(dialog)dialog->reject();return;}
        const auto label=QString("[%1]").arg(QString::fromStdString(source_operation));int index=-1;
        for(int i=0;i<source_combo->count();++i)if(source_combo->itemText(i).contains(label))index=i;
        if(index<0){dialog->reject();return;}
        mode->setCurrentIndex(mode->findData("link"));source_combo->setCurrentIndex(index);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
    QTest::mouseClick(link,Qt::LeftButton);QApplication::processEvents();
    check(operation_enabled_state(session.document(),target).driver==source&&
        !operation_enabled_state(session.document(),target).evaluated,
        "A single-operation target links to a same-composition source outside its Object");
}
void gradient_authoring(Window& window) {
    auto& session=window.host.session;
    named_action(window,"add-rectangle")->trigger();QApplication::processEvents();
    const auto object=window.canvas->selected_object;
    named_action(window,"add-fill")->trigger();QApplication::processEvents();
    const auto op=session.document().objects.at(object).stack.back().id;
    auto current=[&]() -> const ShapeOperation& {
        for(const auto& operation:session.document().objects.at(object).stack)if(operation.id==op)return operation;
        throw std::runtime_error("Gradient operation missing");
    };
    auto choose=[&](int index) {
        auto* mode=visible_child<QComboBox>(window,("gradient-mode-"+op).c_str());reveal(window,mode);
        mode->setCurrentIndex(index);QApplication::processEvents();
    };
    auto click=[&](const std::string& name) {
        auto* button=visible_child<QPushButton>(window,name.c_str());reveal(window,button);
        QTest::mouseClick(button,Qt::LeftButton);QApplication::processEvents();
    };
    auto hex=[&](const std::string& name,const char* text) {
        auto* input=visible_child<QLineEdit>(window,name.c_str());reveal(window,input);input->setFocus();
        QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);
        QTest::keyClick(input,Qt::Key_Return);QApplication::processEvents();
    };
    hex("operation-hex-"+op,"#00000080");choose(1);
    check(current().gradient&&current().gradient->enabled&&current().gradient->type=="linear",
        "Paint mode creates an authored linear gradient");
    const auto gid=current().gradient->id;
    const auto gradient_enabled_ref=gradient_ref(object,op,gid,"enabled");
    const auto operation_enabled_ref=operation_ref(object,op,"enabled");
    const auto first=current().gradient->stops.front().id,last=current().gradient->stops.back().id;
    auto ref=[&](const std::string& field){return gradient_ref(object,op,gid,field);};
    check(current().gradient->stops.size()==2&&first!=last&&current().gradient->stops[0].rgba[3].literal==1&&
        current().gradient->stops[1].rgba[3].literal==1,"Initial stable stops do not duplicate the paint opacity");
    check(gradient_enabled_property(session.document(),gradient_enabled_ref)&&
        operation_enabled_property(session.document(),operation_enabled_ref)&&
        !evaluate(session.document()).contains(gradient_enabled_ref),
        "Inspector-created gradient has a typed bypass Ref separate from numeric picker values and operation enabled");
    hex("gradient-stop-hex-"+first,"#000000FF");hex("gradient-stop-hex-"+last,"#FFFFFFFF");
    window.canvas->fit_artboard();QApplication::processEvents();
    const auto& artboard=session.document().compositions.front().artboards.front();
    const auto cx=artboard.x+artboard.width/2,cy=artboard.y+artboard.height/2;
    auto screen=[&](double x,double y) {
        return QPoint(qRound(window.canvas->width()/2.0+(x-cx)*window.canvas->zoom()),
            qRound(window.canvas->height()/2.0+(y-cy)*window.canvas->zoom()));
    };
    auto pixel=[&](double x,double y) {
        QApplication::processEvents();const auto image=window.canvas->grab().toImage();
        const auto position=QPointF(screen(x,y))*image.devicePixelRatio();
        return image.pixelColor(qRound(position.x()),qRound(position.y()));
    };
    const auto left=pixel(cx-55,cy+10),right=pixel(cx+55,cy+10);
    check(std::abs(left.red()-157)<=5&&std::abs(right.red()-220)<=5,
        "Canvas linear gradient interpolates sRGB and multiplies overall opacity exactly once");
    choose(2);
    check(current().gradient->id==gid&&current().gradient->stops[0].id==first&&
        current().gradient->stops[1].id==last&&current().gradient->type=="radial",
        "Changing gradient type preserves stop identities and authored colors");
    const auto center_text=QString::number(cx).toLatin1();edit_number(window,ref("start_x"),center_text.constData());
    check(pixel(cx,cy+10).red()+35<pixel(cx+55,cy+10).red(),"Radial paint uses the authored center and radius frame");
    choose(0);check(!current().gradient->enabled&&current().gradient->id==gid&&
        !gradient_enabled_property(session.document(),gradient_enabled_ref)&&
        operation_enabled_property(session.document(),operation_enabled_ref),
        "Solid mode exposes the retained gradient's false bypass without changing operation enabled");
    choose(1);check(current().gradient->id==gid&&current().gradient->enabled&&
        gradient_enabled_property(session.document(),gradient_enabled_ref),
        "Returning to Gradient restores true through the same stable bypass Ref");
    const auto start_text=QString::number(cx-110).toLatin1();edit_number(window,ref("start_x"),start_text.constData());
    click("gradient-stop-add-"+op);
    const auto middle=current().gradient->stops.back().id;
    check(current().gradient->stops.size()==3&&std::abs(evaluate(session.document()).at(ref("stop."+middle+".offset"))-0.5)<1e-8,
        "Add stop splits the widest evaluated interval with a new stable ID");
    click("gradient-stop-add-"+op);
    const auto quarter=current().gradient->stops.back().id;
    check(current().gradient->stops.size()==4&&quarter!=middle&&
        std::abs(evaluate(session.document()).at(ref("stop."+quarter+".offset"))-0.25)<1e-8,
        "Repeated stop insertion chooses a distinct largest-gap midpoint");
    check(current().gradient->stops[0].id==first&&current().gradient->stops[1].id==last,
        "Adding stops never reorders the authored stop vector or retargets old IDs");
    edit_number(window,ref("stop."+middle+".offset"),"0.65");
    check(current().gradient->stops[2].id==middle&&current().gradient->stops[2].offset.literal==0.65,
        "Numeric stop editing addresses stable identity independently of evaluation order");
    click("gradient-stop-remove-"+quarter);history_action(window,"Undo");
    check(current().gradient->stops.back().id==quarter,"Undo stop removal restores the same identity");
    history_action(window,"Redo");click("gradient-stop-remove-"+middle);
    check(current().gradient->stops.size()==2&&
        !visible_child<QPushButton>(window,("gradient-stop-remove-"+first).c_str())->isEnabled(),
        "Stop removal keeps the two-stop minimum visible in the controls");

    // Actual Canvas handle events preview through the shared Session; only release
    // notifies Host, so Inspector replacement cannot interrupt a gesture.
    click("gradient-handles-"+op);window.canvas->fit_artboard();QApplication::processEvents();
    check(window.canvas->gradient_operation()==op,"Inspector enables handles for its specific paint operation");
    const auto initial=evaluate(session.document()).at(ref("end_x"));
    auto endpoint=screen(initial,cy);const auto revision=session.revision();
    QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,endpoint);
    QTest::mouseMove(window.canvas,endpoint+QPoint(20,0));QApplication::processEvents();
    QTest::mouseMove(window.canvas,endpoint+QPoint(40,0));QApplication::processEvents();
    check(session.gesture_active()&&session.revision()==revision&&
        evaluate(session.preview_document()).at(ref("end_x"))>initial+20,
        "Gradient handle motion previews without committing an authored revision");
    QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,endpoint+QPoint(40,0));QApplication::processEvents();
    check(session.revision()==revision+1&&std::abs(evaluate(session.document()).at(ref("end_x"))-
        (initial+40/window.canvas->zoom()))<1e-7,"A full gradient handle gesture commits once using local coordinates");
    history_action(window,"Undo");
    check(evaluate(session.document()).at(ref("end_x"))==initial,"One Undo restores the entire gradient gesture");
    const auto cancel_revision=session.revision();window.canvas->setFocus();
    QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,endpoint);
    QTest::mouseMove(window.canvas,endpoint+QPoint(30,0));QApplication::processEvents();
    QTest::keyClick(window.canvas,Qt::Key_Escape);
    QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,endpoint+QPoint(30,0));QApplication::processEvents();
    check(session.revision()==cancel_revision&&!session.gesture_active()&&evaluate(session.document()).at(ref("end_x"))==initial,
        "Escape cancels a gradient preview without a history entry");
    const Ref center{object,"","generator.center_x"};
    session.apply({Link{ref("end_x"),{center,1,110,"copy_local_value"}}},session.revision());window.host.edited();QApplication::processEvents();
    const auto driven_revision=session.revision();window.canvas->setFocus();
    QTest::mousePress(window.canvas,Qt::LeftButton,Qt::NoModifier,endpoint);
    QTest::mouseMove(window.canvas,endpoint+QPoint(35,0));QApplication::processEvents();
    QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,endpoint+QPoint(35,0));QApplication::processEvents();
    check(session.revision()==driven_revision&&nect::property(session.document(),ref("end_x")).binding&&
        window.statusBar()->currentMessage().contains("DRIVEN"),"Dragging a driven gradient frame rejects visibly without unlinking");
    auto* tree=window.findChild<QTreeWidget*>();QTreeWidgetItem* source_row=nullptr;
    for(int i=0;i<tree->topLevelItemCount();++i)
        if(tree->topLevelItem(i)->data(0,Qt::UserRole).toString().toStdString()==object)source_row=tree->topLevelItem(i);
    check(source_row&&source_row->childCount()>0,"Gradient owner retains its editable source points");
    auto* point_row=source_row->child(0);source_row->setExpanded(true);tree->scrollToItem(point_row);QApplication::processEvents();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(point_row).center());QApplication::processEvents();
    check(window.canvas->gradient_operation().empty()&&window.canvas->selected_object==object&&
        window.canvas->selected_point==point_row->data(0,Qt::UserRole+1).toString().toStdString(),
        "Selecting a source point in the tree leaves gradient handles and restores direct point editing");
}
void artboard_authoring(Window& window) {
    auto& session=window.host.session;
    auto button=[&](const char* name) {
        auto* control=visible_child<QPushButton>(window,name);
        if(window.findChild<QScrollArea*>("inspector-scroll")->widget()->isAncestorOf(control))reveal(window,control);
        QTest::mouseClick(control,Qt::LeftButton);QApplication::processEvents();
    };
    auto input=[&](const char* name,const char* value) {
        auto* control=visible_child<QLineEdit>(window,name);reveal(window,control);control->setFocus();
        QTest::keyClick(control,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(control,value);
        QTest::keyClick(control,Qt::Key_Return);QApplication::processEvents();
    };
    auto select=[&](const Id& board) {
        auto* list=window.findChild<QListWidget*>("artboards");QListWidgetItem* selected=nullptr;
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString().toStdString()==board)selected=list->item(i);
        check(selected!=nullptr,"Frame navigator contains its stable board ID");list->scrollToItem(selected);QApplication::processEvents();
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(selected).center());QApplication::processEvents();
        button("artboard-edit");
    };
    const auto original=session.document().compositions.front().artboards.front().id;
    auto authored=[&](const Id& id) {
        for(const auto& board:session.document().compositions.front().artboards)if(board.id==id)return board;
        throw std::runtime_error("Authored artboard missing");
    };
    auto resolved=[&](const Id& id){return evaluate_artboard(session.document().compositions.front(),id);};
    named_action(window,"add-circle")->trigger();QApplication::processEvents();
    const auto geometry=evaluate(session.document());const auto roots=session.document().compositions.front().roots;
    button("artboard-add");const auto child=window.canvas->active_artboard();
    check(child!=original&&session.document().compositions.front().artboards.size()==2&&authored(child).x==1000,
        "Add frame creates a unique ordered board to the right with a gap");
    input("artboard-name","Alternative crop");input("artboard-x","1234");input("artboard-y","34");
    check(authored(child).name=="Alternative crop"&&authored(child).x==1234&&authored(child).y==34&&
        evaluate(session.document())==geometry,"Frame name and crop position edit without moving any artwork");
    window.canvas->fit_artboard();const auto frame_zoom=window.canvas->zoom();window.canvas->fit_all_artboards();
    check(window.canvas->zoom()<frame_zoom,"Fit all includes the resolved union of frames in the active composition");
    window.canvas->fit_artboard();
    const auto before_reorder=authored(child);button("artboard-up");
    check(session.document().compositions.front().artboards.front().id==child&&authored(child).x==before_reorder.x&&
        authored(child).y==before_reorder.y&&authored(original).x==0&&evaluate(session.document())==geometry,
        "Changing frame order changes neither crop coordinates nor authored geometry");
    ArtboardLayout child_layout;
    child_layout.margin=Margin{10,10,10,10};
    child_layout.grid=Grid{"source-grid",{10,10,100,100},2,2,10,10};
    session.apply({SetArtboardLayout{session.document().compositions.front().id,child,child_layout}},session.revision());
    window.host.edited();QApplication::processEvents();
    button("artboard-duplicate");const auto copy=window.canvas->active_artboard();
    check(copy!=child&&authored(copy).width==authored(child).width&&authored(copy).x>authored(child).x&&
        session.document().compositions.front().roots==roots,"Duplicate frame copies settings and leaves artwork ownership unchanged");
    check(authored(copy).layout&&authored(copy).layout->margin==child_layout.margin&&
        authored(copy).layout->grid&&authored(copy).layout->grid->bounds==child_layout.grid->bounds&&
        authored(copy).layout->grid->id!=child_layout.grid->id,
        "Duplicate frame copies layout values and assigns a fresh Grid identity");
    button("artboard-remove");
    check(window.canvas->active_artboard()==child&&session.document().compositions.front().artboards.size()==2,
        "Removing the active frame reconciles to a surviving frame");
    select(child);
    auto* parent=visible_child<QComboBox>(window,"artboard-parent");reveal(window,parent);
    parent->setCurrentIndex(parent->findData(QString::fromStdString(original)));QApplication::processEvents();
    check(authored(child).parent_size&&authored(child).parent_size->width&&authored(child).parent_size->height,
        "Parent picker authors explicit width and height inheritance in the same composition");
    select(original);input("artboard-width","800");input("artboard-height","450");
    check(resolved(child).width==800&&resolved(child).height==450,"Parent frame resizing propagates evaluated child size");
    const auto protected_revision=session.revision();button("artboard-remove");
    check(session.revision()==protected_revision&&authored(child).parent_size&&window.statusBar()->currentMessage().contains("MISSING_ARTBOARD"),
        "Removing a referenced parent rejects visibly without changing the document");
    select(child);input("artboard-width","900");
    check(!authored(child).parent_size->width&&authored(child).parent_size->height&&resolved(child).width==900,
        "Typing inherited width explicitly creates only a local width override");
    select(original);input("artboard-width","700");input("artboard-height","500");
    check(resolved(child).width==900&&resolved(child).height==500,"An overridden dimension stays fixed while the other still inherits");
    select(child);auto* inherit=visible_child<QCheckBox>(window,"artboard-inherit-width");reveal(window,inherit);
    QTest::mouseClick(inherit,Qt::LeftButton,Qt::NoModifier,QPoint(8,inherit->height()/2));QApplication::processEvents();
    check(resolved(child).width==700&&authored(child).parent_size->width,"Inherit reset restores the current parent dimension");
    auto* detach=visible_child<QPushButton>(window,"artboard-detach");reveal(window,detach);button("artboard-detach");
    check(!authored(child).parent_size&&authored(child).width==700&&authored(child).height==500,"Detach freezes both evaluated dimensions");
    history_action(window,"Undo");check(authored(child).parent_size.has_value(),"Undo detach restores the parent relationship");
    history_action(window,"Redo");select(original);input("artboard-width","650");input("artboard-height","400");
    check(resolved(child).width==700&&resolved(child).height==500&&evaluate(session.document())==geometry,
        "Detached dimensions and artwork stay fixed when the former parent changes");

    // Open a real native fixture with an empty leading composition and two
    // independent planes. Empty legacy compositions must not break navigation.
    auto document=empty_document("navigation-doc","plane-a","board-a");
    document.compositions.insert(document.compositions.begin(),Composition{"empty-plane","Empty plane",{}, {}});
    document.compositions.push_back(Composition{"plane-b","Second plane",{},{{"board-b","Offset frame",2000,100,400,300}}});
    QTemporaryDir files;const auto path=files.filePath("planes.nect");QFile file(path);
    check(file.open(QIODevice::WriteOnly),"Multi-plane fixture can be written");const auto bytes=QByteArray::fromStdString(encode(document));
    check(file.write(bytes)==bytes.size(),"Multi-plane fixture write is complete");file.close();window.host.open(path);QApplication::processEvents();
    check(window.canvas->active_composition()=="plane-a"&&window.canvas->active_artboard()=="board-a",
        "Open reconciles invalid view state to the first composition with an artboard");
    named_action(window,"add-circle")->trigger();QApplication::processEvents();const auto first_circle=window.canvas->selected_object;
    select("board-b");
    check(window.canvas->active_composition()=="plane-b"&&window.findChild<QTreeWidget*>()->topLevelItemCount()==0,
        "Switching frames switches composition planes and hides other-plane objects");
    named_action(window,"add-rectangle")->trigger();QApplication::processEvents();const auto rectangle=window.canvas->selected_object;
    const auto values=evaluate(session.document());
    check(values.at({rectangle,"","generator.center_x"})==2200&&values.at({rectangle,"","generator.center_y"})==250&&
        session.document().compositions.back().roots==std::vector<Id>{rectangle},
        "New primitives use the active frame center and active composition ownership");
    named_action(window,"add-curve")->trigger();QApplication::processEvents();const auto curve=window.canvas->selected_object;
    const auto curve_point=path_contours(session.document().objects.at(curve)).front().points.front().id;
    check(evaluate(session.document()).at({curve,curve_point,"x"})==2100&&session.document().compositions.back().roots.size()==2,
        "New Curve uses the active crop and composition instead of the first plane");
    window.canvas->fit_artboard();window.canvas->set_draw_mode(true);window.canvas->setFocus();
    QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,window.canvas->rect().center());QApplication::processEvents();
    window.canvas->set_draw_mode(false);
    check(session.document().compositions.back().roots.size()==3&&session.document().compositions[1].roots==std::vector<Id>{first_circle},
        "Canvas draw-path commands stay on the active plane");
    auto* tree=window.findChild<QTreeWidget*>();tree->clearSelection();
    for(int index=0;index<tree->topLevelItemCount();++index)tree->topLevelItem(index)->setSelected(true);
    for(auto* action:window.findChildren<QAction*>())if(action->text()=="Group selected siblings")action->trigger();
    QApplication::processEvents();
    check(session.document().compositions.back().roots.size()==1&&session.document().objects.at(session.document().compositions.back().roots.front()).children.size()==3,
        "Grouping selected roots uses their active composition instead of the first plane");
    const auto svg=export_svg(session.document(),window.canvas->active_composition(),window.canvas->active_artboard());
    check(svg.find("viewBox=\"2000 100 400 300\"")!=std::string::npos&&svg.find(first_circle)==std::string::npos,
        "Active export identifiers select the offset frame and exclude other composition content");
    // Compare the two fit commands in the same viewport: dock content can
    // resize the Canvas after the earlier frame fit above.
    window.canvas->fit_artboard();
    const auto active_zoom=window.canvas->zoom();window.canvas->fit_all_artboards();
    check(std::abs(window.canvas->zoom()-active_zoom)<1e-8,"Fit all uses only the active composition plane");
    window.host.create_document();QApplication::processEvents();
    check(window.canvas->active_composition()==session.document().compositions.front().id&&
        window.canvas->active_artboard()==session.document().compositions.front().artboards.front().id,
        "New document resets stale composition and artboard view identities");
}
}
void layout_setup_previews_commit_and_recovers(Window& window) {
    auto& session=window.host.session;
    auto click=[&](const char* name) {
        auto* control=visible_child<QPushButton>(window,name);reveal(window,control);
        QTest::mouseClick(control,Qt::LeftButton);QApplication::processEvents();
    };
    QTest::mouseClick(visible_child<QPushButton>(window,"artboard-edit"),Qt::LeftButton);QApplication::processEvents();
    auto input=[&](const char* name,const char* value,bool enter) {
        auto candidates=window.findChildren<QLineEdit*>(QString::fromLatin1(name));
        QLineEdit* control=nullptr;for(auto* candidate:candidates)if(candidate->isVisible()){control=candidate;break;}
        check(control!=nullptr,(std::string("Layout editor remains visible: ")+name).c_str());reveal(window,control);control->setFocus();
        QTest::keyClick(control,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(control,value);QApplication::processEvents();
        if(enter){QTest::keyClick(control,Qt::Key_Return);QApplication::processEvents();}
        return control;
    };
    auto board=[&]() -> const Artboard& {return session.document().compositions.front().artboards.front();};
    auto preview_board=[&]() -> const Artboard& {return session.preview_document().compositions.front().artboards.front();};
    const auto baseline=session.revision();

    auto* guides=visible_child<QCheckBox>(window,"overlay-show-guides");
    QTest::mouseClick(guides,Qt::LeftButton);QApplication::processEvents();
    check(!window.canvas->show_guides()&&window.canvas->show_grid()&&window.canvas->show_margin()&&session.revision()==baseline,
        "Guide, Grid and Margin visibility remain independent per-window view state");
    guides->setChecked(true);QApplication::processEvents();
    auto* snap_guides=window.findChild<QAction*>("snap-guides");
    auto* snap_grid=window.findChild<QAction*>("snap-grid");
    check(snap_guides&&snap_grid&&!snap_guides->toolTip().isEmpty()&&!snap_grid->toolTip().isEmpty(),
        "Guide Snap and Grid Snap have distinct named controls and help text");
    snap_guides->setChecked(false);snap_grid->setChecked(false);QApplication::processEvents();
    const auto snap_state_error="Guide/Grid Snap state failure: guide_snap="+std::to_string(window.canvas->snap_guides_enabled())+
        " grid_snap="+std::to_string(window.canvas->snap_grid_enabled())+
        " guides_visible="+std::to_string(window.canvas->show_guides())+
        " grid_visible="+std::to_string(window.canvas->show_grid())+
        " revision="+std::to_string(session.revision())+
        " baseline="+std::to_string(baseline);
    check(!window.canvas->snap_guides_enabled()&&!window.canvas->snap_grid_enabled()&&
          window.canvas->show_guides()&&window.canvas->show_grid()&&session.revision()==baseline,
        snap_state_error.c_str());
    snap_guides->setChecked(true);snap_grid->setChecked(true);QApplication::processEvents();

    input("margin-left","10",false);input("margin-top","10",false);input("margin-right","10",false);auto* bottom=input("margin-bottom","10",false);
    check(session.revision()==baseline&&!board().layout&&preview_board().layout&&preview_board().layout->margin->left==10&&
          !session.can_undo(),"Valid Margin drafts preview in the live Session without changing committed state or history");
    QTest::keyClick(bottom,Qt::Key_Return);QApplication::processEvents();
    check(session.revision()==baseline+1&&board().layout&&board().layout->margin==Margin{10,10,10,10}&&session.can_undo(),
        "Enter commits a complete Margin through one Session command");

    const auto before_copy=session.revision();
    const auto evaluated_before_copy=evaluate_artboard(session.document().compositions.front(),board().id);
    click("grid-copy-margin-box");
    const auto copied=board().layout->grid.value();
    check(session.revision()==before_copy+1&&copied.bounds==LayoutRect{10,10,evaluated_before_copy.width-20,evaluated_before_copy.height-20}&&copied.columns==1&&!copied.id.empty(),
        "Set Grid to margin box copies evaluated local bounds once and creates a stable Grid ID");
    check(decode(encode(session.document()))==session.document(),"Committed Guide/Grid/Margin state survives native encode/decode");

    input("margin-left","20",false);
    check(board().layout->margin->left==10&&preview_board().layout->margin->left==20&&preview_board().layout->grid->bounds.x==10,
        "Margin preview is independent of the one-time copied Grid bounds");
    auto* left=visible_child<QLineEdit>(window,"margin-left");QTest::keyClick(left,Qt::Key_Return);QApplication::processEvents();
    check(board().layout->margin->left==20&&board().layout->grid->bounds.x==10,
        "A later Margin edit does not maintain a persistent link to Grid");

    auto* columns=input("grid-columns","2",false);const auto invalid_revision=session.revision();
    check(session.preview_document().compositions.front().artboards.front().layout->grid->columns==2&&
          session.revision()==invalid_revision&&session.document().compositions.front().artboards.front().layout->grid->columns==1,
        "A valid Grid count draft previews without changing committed authored state");
    columns=input("grid-columns","2.5",false);
    check(session.preview_document().compositions.front().artboards.front().layout->grid->columns==2&&
          session.document().compositions.front().artboards.front().layout->grid->columns==1,
        "An invalid fractional count leaves the latest valid Session preview visible");
    QTest::keyClick(columns,Qt::Key_Return);QApplication::processEvents();
    check(session.revision()==invalid_revision&&session.document().compositions.front().artboards.front().layout->grid->columns==1&&
          columns->text()=="2.5"&&window.statusBar()->currentMessage().contains("INVALID_LAYOUT"),
        "Invalid numeric draft stays visible with an error and never becomes authored");
    click("grid-clear");
    check(session.revision()==invalid_revision+1&&board().layout&&board().layout->margin&&!board().layout->grid,
        "Explicit Clear Grid discards the invalid draft and preserves Margin");

    input("guide-new-position","25",false);
    check(session.revision()==invalid_revision+1&&session.document().compositions.front().guides.empty()&&
          session.preview_document().compositions.front().guides.size()==1&&session.preview_document().compositions.front().guides.front().position==25,
        "New Guide position previews without committing an ID or revision");
    click("guide-add");const auto guide_id=session.document().compositions.front().guides.front().id;
    check(!guide_id.empty()&&session.revision()==invalid_revision+2&&session.document().compositions.front().guides.front().position==25,
        "Apply Guide creation commits one stable Guide identity through the shared Session");
    const std::string position_name="guide-position-"+guide_id;
    auto* guide_position=window.findChild<QLineEdit*>(QString::fromStdString(position_name));
    check(guide_position!=nullptr,"Committed Guide appears in the refreshed Guide editor list");
    reveal(window,guide_position);
    auto* edited_position=input(position_name.c_str(),"100",false);
    check(session.document().compositions.front().guides.front().position==25&&
          session.preview_document().compositions.front().guides.front().id==guide_id&&
          session.preview_document().compositions.front().guides.front().position==100,
        "Guide edit preview retains stable identity and does not commit early");
    QTest::keyClick(edited_position,Qt::Key_Escape);QApplication::processEvents();QApplication::processEvents();
    check(session.revision()==invalid_revision+2&&!session.gesture_active()&&session.document().compositions.front().guides.front().position==25,
        "Escape cancels numeric Guide preview without history");
    auto* restored_position=visible_child<QLineEdit>(window,position_name.c_str());int visible_position_fields=0;
    for(auto* field:window.findChildren<QLineEdit*>(QString::fromStdString(position_name)))if(field->isVisible())++visible_position_fields;
    check(visible_position_fields==1&&restored_position->text()=="25",
        "After queued Escape refresh settles, one current Guide editor is visible with authored state");

    const Id guide_source_id="guide-ui-source";
    const Ref guide_target_ref{guide_id,"","guide.position"};
    const Ref guide_source_ref{guide_source_id,"","guide.position"};
    session.apply({AddGuide{session.document().compositions.front().id,
            {guide_source_id,"UI Guide source","x",100}},
        LinkGuidePosition{guide_target_ref,guide_source_ref,false}},session.revision());
    window.host.edited();QApplication::processEvents();
    auto* linked_position=visible_child<QLineEdit>(window,position_name.c_str());
    auto* linked_status=visible_child<QLabel>(window,("guide-position-status-"+guide_id).c_str());
    auto* unlink_position=visible_child<QPushButton>(window,("guide-unlink-position-"+guide_id).c_str());
    check(linked_position->isReadOnly()&&linked_position->text()=="100"&&
          linked_status->text().contains("UI Guide source")&&linked_status->text().contains("25")&&
          unlink_position->text()=="Unlink position",
        "Guide editor shows the evaluated coordinate, source name, authored literal and explicit Unlink action");
    const auto before_unlink=session.revision();QTest::mouseClick(unlink_position,Qt::LeftButton);QApplication::processEvents();
    auto* unlinked_position=visible_child<QLineEdit>(window,position_name.c_str());
    auto* unlinked_status=window.findChild<QLabel*>(QString::fromStdString("guide-position-status-"+guide_id));
    auto* hidden_unlink=window.findChild<QPushButton*>(QString::fromStdString("guide-unlink-position-"+guide_id));
    const auto frozen_position=guide_position_property(session.document(),guide_target_ref);
    check(session.revision()==before_unlink+1&&frozen_position.literal==100&&!frozen_position.driver&&
          frozen_position.evaluated==100&&!unlinked_position->isReadOnly()&&unlinked_position->text()=="100"&&
          unlinked_status&&!unlinked_status->isVisible()&&hidden_unlink&&!hidden_unlink->isVisible(),
        "Guide Unlink freezes the evaluated coordinate and refreshes the editor to editable literal state");

    input(position_name.c_str(),"nan",false);const auto before_delete=session.revision();
    check(window.statusBar()->currentMessage().contains("INVALID_VALUE")&&
          guide_position_property(session.document(),guide_target_ref).literal==100,
        "Nonfinite Guide input is rejected while the invalid draft remains visible");
    const std::string delete_name="guide-delete-"+guide_id;click(delete_name.c_str());
    check(session.revision()==before_delete+1&&session.document().compositions.front().guides.size()==1&&
          session.document().compositions.front().guides.front().id==guide_source_id,
        "Explicit Delete Guide discards an invalid draft and removes only the selected stable ID");
    click(("guide-delete-"+guide_source_id).c_str());
    check(session.revision()==before_delete+2&&session.document().compositions.front().guides.empty(),
        "Explicit Delete Guide can then remove the independent source Guide");

    click("margin-clear");const auto before_copy_without_margin=session.revision();click("grid-copy-margin-box");
    check(session.revision()==before_copy_without_margin&&window.statusBar()->currentMessage().contains("INVALID_LAYOUT"),
        "Grid copy without authored Margin reports a visible error and leaves revision unchanged");

    const auto& composition=session.document().compositions.front();
    const auto composition_id=composition.id,board_id=composition.artboards.front().id;
    auto external_margin=[&](double left) {
        // An external host owner can end the preview before advancing Session.
        if(session.gesture_active())session.cancel_gesture();
        session.apply({SetArtboardLayout{composition_id,board_id,ArtboardLayout{Margin{left,0,0,0},std::nullopt}}},session.revision());
    };
    auto* stale_field=input("margin-left","30",false);
    check(session.gesture_active()&&board().layout==std::nullopt,
        "Margin preview remains a draft before an external revision");
    const auto before_external=session.revision();external_margin(15);
    QTest::keyClicks(stale_field,"1");QApplication::processEvents();QApplication::processEvents();
    check(session.revision()==before_external+1&&!session.gesture_active()&&board().layout->margin->left==15&&
          session.preview_document()==session.document()&&window.statusBar()->currentMessage().contains("REVISION_CONFLICT")&&
          visible_child<QLineEdit>(window,"margin-left")->text()=="15",
        "Typing into a stale layout draft rejects and refreshes without silently rebasing old fields");

    stale_field=input("margin-left","35",false);const auto before_stale_apply=session.revision();external_margin(20);
    QTest::keyClick(stale_field,Qt::Key_Return);QApplication::processEvents();QApplication::processEvents();
    check(session.revision()==before_stale_apply+1&&!session.gesture_active()&&board().layout->margin->left==20&&
          session.preview_document()==session.document()&&window.statusBar()->currentMessage().contains("REVISION_CONFLICT")&&
          visible_child<QLineEdit>(window,"margin-left")->text()=="20",
        "Applying a stale layout draft rejects and requires a fresh editor gesture");

    const auto before_stale_editor=session.revision();external_margin(25);
    input("margin-left","45",false);QApplication::processEvents();
    check(session.revision()==before_stale_editor+1&&!session.gesture_active()&&board().layout->margin->left==25&&
          session.preview_document()==session.document()&&window.statusBar()->currentMessage().contains("REVISION_CONFLICT")&&
          visible_child<QLineEdit>(window,"margin-left")->text()=="25",
        "An untouched Inspector built at an older revision cannot overwrite current layout values");

    session.apply({SetArtboardLayout{composition_id,board_id,
                       ArtboardLayout{Margin{25,0,0,0},Grid{"recovery-grid",{25,0,100,100},2,1,10,0}}},
                   AddGuide{composition_id,{"recovery-guide","Recovery","y",25}}},session.revision());
    window.host.edited();window.host.recover();
    const auto recovery_path=window.host.persistence()["recovery_file"].toString();
    const auto protected_document=session.document();
    check(window.host.persistence()["recovery_revision"].toInteger(-1)==session.revision()&&
          load_native(recovery_path).document==protected_document,
        "Recovery protects the committed native layout and Guide state at the exact revision");
    window.host.open_recovery(recovery_path);QApplication::processEvents();
    check(session.document()==protected_document&&board().layout->margin->left==25&&
          board().layout->grid->id=="recovery-grid"&&session.document().compositions.front().guides.front().id=="recovery-guide"&&
          window.host.file_path.isEmpty(),
        "Opening the owned recovery restores authored layout without overwriting its source");
}

void text_authoring(Window& window) {
    auto& session=window.host.session;named_action(window,"add-text")->trigger();QApplication::processEvents();
    const auto id=window.canvas->selected_object;
    check(session.document().objects.at(id).text.has_value(),"Add Text creates editable source in active composition");
    auto* content=visible_child<QPushButton>(window,"edit-text-content");const auto revision=session.revision();
    bool typed=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-editor-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");if(!editor){dialog->reject();return;}
        editor->setPlainText(QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\nNect 2026"));
        typed=true;dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    });
    content->click();QApplication::processEvents();
    check(typed&&session.revision()==revision+1&&session.document().objects.at(id).text->content.find("Nect 2026")!=std::string::npos,"Unicode content editor commits one shared undo step");
    const auto authored=session.document().objects.at(id).text->content;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-editor-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");editor->setPlainText("Uncommitted draft");
        window.host.recover();window.refresh();typed=editor->toPlainText()=="Uncommitted draft";
        dialog->reject();
    });
    visible_child<QPushButton>(window,"edit-text-content")->click();QApplication::processEvents();
    check(typed&&session.document().objects.at(id).text->content==authored&&session.revision()==revision+1,"Recovery and Inspector refresh retain IME draft; cancel leaves document unchanged");
    bool conflict=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-editor-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");editor->setPlainText("GUI draft");
        auto next=*session.document().objects.at(id).text;next.content="External edit";
        session.apply({UpdateText{id,next}},session.revision());window.host.edited();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        conflict=dialog->isVisible()&&editor->toPlainText()=="GUI draft"&&dialog->findChild<QLabel*>("text-editor-status")->text().contains("changed elsewhere");dialog->reject();
    });
    visible_child<QPushButton>(window,"edit-text-content")->click();QApplication::processEvents();
    check(conflict&&session.document().objects.at(id).text->content=="External edit","Concurrent API content edit rejects overwrite and preserves the draft for copying");
    const auto direction_draft_revision=session.revision();bool direction_draft_staged=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-direction-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-direction-editor");if(!editor){dialog->reject();return;}
        editor->setCurrentIndex(1);direction_draft_staged=session.revision()==direction_draft_revision&&
            session.document().objects.at(id).text->direction=="horizontal";dialog->reject();
    });
    visible_child<QToolButton>(window,"text-direction-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(direction_draft_staged&&session.revision()==direction_draft_revision&&session.document().objects.at(id).text->direction=="horizontal",
        "Text direction selector stages a draft and Cancel leaves Session bytes and revision unchanged");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-direction-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-direction-editor");if(!editor){dialog->reject();return;}
        editor->setCurrentIndex(1);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    });
    visible_child<QToolButton>(window,"text-direction-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(session.document().objects.at(id).text->direction=="vertical"&&session.revision()==direction_draft_revision+1,
        "Applying the Text direction draft commits one shared Session revision");
    const auto initial_layout_revision=session.revision();
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-layout-dialog");if(!dialog)return;
        dialog->findChild<QComboBox*>("text-layout-editor")->setCurrentIndex(1);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    });
    visible_child<QToolButton>(window,"text-layout-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(session.document().objects.at(id).text->layout=="frame"&&session.revision()==initial_layout_revision+1,
        "Applying a staged Text sizing choice commits one Session revision");
    check(visible_child<QLabel>(window,"text-layout-status")->text().contains("SVG exports glyph outlines"),"Inspector discloses export text projection");
    QApplication::setActiveWindow(&window);QApplication::processEvents();
    const Ref font_size{id,"","text.font_size"};auto* size=field<QLineEdit>(window,font_size);reveal(window,size);size->setFocus();
    const auto scroll=window.findChild<QScrollArea*>("inspector-scroll")->verticalScrollBar()->value();const auto focused_before=size->hasFocus();
    size->selectAll();QTest::keyClicks(size,"52");QTest::keyClick(size,Qt::Key_Return);QApplication::processEvents();
    check(field<QLineEdit>(window,font_size)->hasFocus()&&window.findChild<QScrollArea*>("inspector-scroll")->verticalScrollBar()->value()==scroll,
        "Numeric Return preserves focus and Inspector scroll position after rebuilding text controls");
    named_action(window,"add-stroke")->trigger();QApplication::processEvents();check(session.document().objects.at(id).stack.size()==2,"Text supports the common editable paint stack");
    const auto text_stroke=session.document().objects.at(id).stack.back().id;
    auto* text_cap=visible_child<QComboBox>(window,("stroke-line-cap-"+text_stroke).c_str());reveal(window,text_cap);
    text_cap->setCurrentIndex(text_cap->findData("square"));QApplication::processEvents();
    check(session.document().objects.at(id).text&&session.document().objects.at(id).stack.back().line_cap=="square"&&
        session.document().objects.at(id).stack.back().version==2,
        "Text source remains editable while its Stroke Inspector commits style");
    auto* family=visible_child<QComboBox>(window,"text-family");reveal(window,family);
    const auto original_family=family->currentText();auto* font_model=family->completer()->model();
    check(font_model&&font_model->rowCount()>1,"Font completion can discover installed families before opening the dropdown");
    auto font_revision=session.revision();family->setFocus();QTest::keyClick(family,Qt::Key_F4);QApplication::processEvents();
    check(family->view()->isVisible()&&family->model()==font_model&&family->currentText()==original_family&&session.revision()==font_revision,
        "Keyboard font popup attaches the shared installed model without changing the authored family or revision");
    family->hidePopup();QApplication::processEvents();
    const QString missing_family="Nect Missing Font Family 2026";
    family->lineEdit()->setFocus();family->lineEdit()->selectAll();QTest::keyClicks(family->lineEdit(),missing_family);
    QTest::keyClick(family->lineEdit(),Qt::Key_Return);QApplication::processEvents();
    check(session.document().objects.at(id).text->family==missing_family.toStdString()&&session.revision()==font_revision+1,
        "Manual missing-family entry preserves the exact authored string as one edit");
    family=visible_child<QComboBox>(window,"text-family");reveal(window,family);
    check(family->completer()->model()==font_model,"Inspector refresh reuses the same installed-font model for completion");
    font_revision=session.revision();family->showPopup();QApplication::processEvents();
    check(family->model()==font_model&&family->currentText()==missing_family&&session.revision()==font_revision,
        "Opening a missing-family dropdown keeps its authored value instead of selecting an installed replacement");
    family->hidePopup();QApplication::processEvents();
    window.refresh();QApplication::processEvents();family=visible_child<QComboBox>(window,"text-family");reveal(window,family);
    check(family->model()!=font_model&&family->completer()->model()==font_model,
        "Rebuilt family editor keeps completion available without eagerly attaching the popup model");
    family->lineEdit()->setFocus();family->lineEdit()->selectAll();QTest::keyClicks(family->lineEdit(),original_family);
    QTest::keyClick(family->lineEdit(),Qt::Key_Return);QApplication::processEvents();
    check(session.document().objects.at(id).text->family==original_family.toStdString()&&session.revision()==font_revision+1,
        "An installed family entered with inline completion commits exactly once");
    auto family_source_text=default_text(new_id(),"Weight source");family_source_text.weight=700;family_source_text.family="Linked Family A";family_source_text.locale="ar-SA";
    const auto source_id=new_id(),source_name=std::string("Weight source");const auto composition=session.document().compositions.front().id;
    session.apply({CreateText{composition,"",source_id,source_name,family_source_text}},session.revision());window.host.edited();QApplication::processEvents();
    auto* direction_driver_button=visible_child<QToolButton>(window,"text-direction-driver");bool chose_direction_source=false;
    check(!visible_child<QComboBox>(window,"text-direction")->isEnabled()&&direction_driver_button->menu()->actions().size()==3,
        "Text direction Inspector exposes a staged enum display and explicit edit/link/unlink menu");
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        if(auto* combo=dialog->findChild<QComboBox*>()) {
            combo->setCurrentText(QString::fromStdString(source_name)+" — "+QString::fromStdString(source_id));
            dialog->accept();chose_direction_source=true;return;
        }
    }});
    direction_driver_button->menu()->actions().at(1)->trigger();QApplication::processEvents();
    check(chose_direction_source&&session.document().objects.at(id).text->direction=="vertical"&&
        session.document().objects.at(id).text->direction_driver->link==Ref{source_id,"","text.direction"}&&
        evaluate_text_direction(session.document(),id)=="horizontal"&&
        visible_child<QLabel>(window,"text-direction-state")->text().contains("Literal: vertical")&&
        visible_child<QLabel>(window,"text-direction-state")->text().contains("Evaluated: horizontal"),
        "Text Direction Inspector links the same field and reports literal, source and evaluated enum");
    auto changed_direction_source=*session.document().objects.at(source_id).text;changed_direction_source.direction="vertical";
    session.apply({UpdateText{source_id,changed_direction_source}},session.revision());window.host.edited();QApplication::processEvents();
    const auto linked_direction_revision=session.revision();bool direction_cancel_safe=false,direction_apply_failed=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-direction-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-direction-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-direction-driver");
        if(!editor||!unlink)return;
        unlink->setChecked(true);editor->setCurrentIndex(0);
        direction_cancel_safe=session.revision()==linked_direction_revision&&
            session.document().objects.at(id).text->direction_driver->link==Ref{source_id,"","text.direction"};dialog->reject();
    });
    visible_child<QToolButton>(window,"text-direction-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(direction_cancel_safe&&session.revision()==linked_direction_revision&&
        session.document().objects.at(id).text->direction_driver->link==Ref{source_id,"","text.direction"},
        "Cancel discards both a direction draft and its staged unlink");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-direction-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-direction-editor");auto* status=dialog->findChild<QLabel*>("text-direction-editor-status");
        editor->setCurrentIndex(0);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        direction_apply_failed=dialog->isVisible()&&session.revision()==linked_direction_revision&&
            session.document().objects.at(id).text->direction_driver.has_value()&&status->text().contains("unlink option");dialog->reject();
    });
    visible_child<QToolButton>(window,"text-direction-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(direction_apply_failed,"Failed driven direction Apply retains the driver and Session revision");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-direction-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-direction-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-direction-driver");
        if(!editor||!unlink)return;
        unlink->setChecked(true);editor->setCurrentIndex(0);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    });
    visible_child<QToolButton>(window,"text-direction-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(!session.document().objects.at(id).text->direction_driver&&session.document().objects.at(id).text->direction=="horizontal"&&
        session.revision()==linked_direction_revision+1,"Direction unlink and edit commit as one atomic Session revision");
    changed_direction_source=*session.document().objects.at(source_id).text;changed_direction_source.direction="horizontal";
    session.apply({UpdateText{source_id,changed_direction_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(session.document().objects.at(id).text->direction=="horizontal"&&!session.document().objects.at(id).text->direction_driver,
        "Unlinked direction remains frozen when its former source changes");
    auto* layout_driver_button=visible_child<QToolButton>(window,"text-layout-driver");bool chose_layout_source=false;
    check(!visible_child<QComboBox>(window,"text-layout")->isEnabled()&&layout_driver_button->menu()->actions().size()==3,
        "Text sizing Inspector exposes a staged enum display and explicit edit/link/unlink menu");
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        if(auto* combo=dialog->findChild<QComboBox*>()) {
            combo->setCurrentText(QString::fromStdString(source_name)+" — "+QString::fromStdString(source_id));
            dialog->accept();chose_layout_source=true;return;
        }
    }});
    layout_driver_button->menu()->actions().at(1)->trigger();QApplication::processEvents();
    check(chose_layout_source&&session.document().objects.at(id).text->layout=="frame"&&
        session.document().objects.at(id).text->layout_driver->link==Ref{source_id,"","text.layout"}&&
        evaluate_text_layout(session.document(),id)=="auto"&&
        visible_child<QLabel>(window,"text-layout-state")->text().contains("Literal: frame")&&
        visible_child<QLabel>(window,"text-layout-state")->text().contains("Evaluated: auto"),
        "Text Sizing Inspector links the same field and reports literal, source and evaluated enum");
    auto changed_layout_source=*session.document().objects.at(source_id).text;changed_layout_source.layout="frame";
    session.apply({UpdateText{source_id,changed_layout_source}},session.revision());window.host.edited();QApplication::processEvents();
    const auto linked_layout_revision=session.revision();bool layout_cancel_safe=false,layout_apply_failed=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-layout-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-layout-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-layout-driver");
        if(!editor||!unlink)return;
        unlink->setChecked(true);editor->setCurrentIndex(0);
        layout_cancel_safe=session.revision()==linked_layout_revision&&
            session.document().objects.at(id).text->layout_driver->link==Ref{source_id,"","text.layout"};dialog->reject();
    });
    layout_driver_button=visible_child<QToolButton>(window,"text-layout-driver");layout_driver_button->menu()->actions().front()->trigger();QApplication::processEvents();
    check(layout_cancel_safe&&session.revision()==linked_layout_revision&&
        session.document().objects.at(id).text->layout_driver->link==Ref{source_id,"","text.layout"},
        "Cancel discards both a sizing draft and its staged unlink");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-layout-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-layout-editor");auto* status=dialog->findChild<QLabel*>("text-layout-editor-status");
        editor->setCurrentIndex(0);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        layout_apply_failed=dialog->isVisible()&&session.revision()==linked_layout_revision&&
            session.document().objects.at(id).text->layout_driver.has_value()&&status->text().contains("unlink option");dialog->reject();
    });
    visible_child<QToolButton>(window,"text-layout-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(layout_apply_failed,"Failed driven sizing Apply retains the driver and Session revision");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-layout-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-layout-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-layout-driver");
        if(!editor||!unlink)return;
        unlink->setChecked(true);editor->setCurrentIndex(1);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    });
    visible_child<QToolButton>(window,"text-layout-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(!session.document().objects.at(id).text->layout_driver&&session.document().objects.at(id).text->layout=="frame"&&
        session.revision()==linked_layout_revision+1,"Sizing unlink and edit commit as one atomic Session revision");
    changed_layout_source=*session.document().objects.at(source_id).text;changed_layout_source.layout="auto";
    session.apply({UpdateText{source_id,changed_layout_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(session.document().objects.at(id).text->layout=="frame"&&!session.document().objects.at(id).text->layout_driver,
        "Unlinked Text sizing remains frozen when its former source changes");
    auto* alignment_driver_button=visible_child<QToolButton>(window,"text-alignment-driver");
    check(!visible_child<QComboBox>(window,"text-alignment")->isEnabled()&&alignment_driver_button->menu()->actions().size()==3,
        "Text alignment Inspector exposes a staged enum control and explicit edit/link/unlink menu");
    const auto alignment_draft_revision=session.revision();bool alignment_cancel_safe=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-alignment-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-alignment-editor");if(!editor){dialog->reject();return;}
        editor->setCurrentIndex(1);alignment_cancel_safe=session.revision()==alignment_draft_revision&&
            session.document().objects.at(id).text->alignment=="start";dialog->reject();
    });
    alignment_driver_button->menu()->actions().front()->trigger();QApplication::processEvents();
    check(alignment_cancel_safe&&session.revision()==alignment_draft_revision&&session.document().objects.at(id).text->alignment=="start",
        "Text alignment Cancel discards the staged choice without changing authored state or revision");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-alignment-dialog");if(!dialog)return;
        dialog->findChild<QComboBox*>("text-alignment-editor")->setCurrentIndex(1);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    });
    alignment_driver_button->menu()->actions().front()->trigger();QApplication::processEvents();
    check(session.document().objects.at(id).text->alignment=="center"&&session.revision()==alignment_draft_revision+1,
        "Applying a staged Text alignment choice commits one Session revision");
    bool chose_alignment_source=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        if(auto* combo=dialog->findChild<QComboBox*>()) {
            combo->setCurrentText(QString::fromStdString(source_name)+" — "+QString::fromStdString(source_id));
            dialog->accept();chose_alignment_source=true;return;
        }
    }});
    alignment_driver_button=visible_child<QToolButton>(window,"text-alignment-driver");alignment_driver_button->menu()->actions().at(1)->trigger();QApplication::processEvents();
    check(chose_alignment_source&&session.document().objects.at(id).text->alignment=="center"&&
        session.document().objects.at(id).text->alignment_driver->link==Ref{source_id,"","text.alignment"}&&
        evaluate_text_alignment(session.document(),id)=="start"&&
        visible_child<QLabel>(window,"text-alignment-state")->text().contains("Literal: center")&&
        visible_child<QLabel>(window,"text-alignment-state")->text().contains("Evaluated: start"),
        "Text alignment Inspector links the same field and reports literal, source and evaluated choices");
    auto changed_alignment_source=*session.document().objects.at(source_id).text;changed_alignment_source.alignment="center";
    session.apply({UpdateText{source_id,changed_alignment_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(evaluate_text_alignment(session.document(),id)=="center"&&
        visible_child<QComboBox>(window,"text-alignment")->currentText()=="Center",
        "Text alignment Inspector follows its linked source edit");
    const auto linked_alignment_revision=session.revision();bool alignment_cancel_with_driver=false,alignment_apply_failed=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-alignment-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-alignment-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-alignment-driver");
        if(!editor||!unlink)return;
        unlink->setChecked(true);editor->setCurrentIndex(2);
        alignment_cancel_with_driver=session.revision()==linked_alignment_revision&&
            session.document().objects.at(id).text->alignment_driver->link==Ref{source_id,"","text.alignment"};dialog->reject();
    });
    visible_child<QToolButton>(window,"text-alignment-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(alignment_cancel_with_driver&&session.revision()==linked_alignment_revision&&
        session.document().objects.at(id).text->alignment_driver->link==Ref{source_id,"","text.alignment"},
        "Cancel discards staged Text alignment unlink and edit");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-alignment-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-alignment-editor");auto* status=dialog->findChild<QLabel*>("text-alignment-editor-status");
        editor->setCurrentIndex(0);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        alignment_apply_failed=dialog->isVisible()&&session.revision()==linked_alignment_revision&&
            session.document().objects.at(id).text->alignment_driver.has_value()&&status->text().contains("unlink option");dialog->reject();
    });
    visible_child<QToolButton>(window,"text-alignment-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(alignment_apply_failed,"Failed driven Text alignment Apply retains its driver and Session revision");
    bool alignment_stale_preserved=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-alignment-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-alignment-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-alignment-driver");
        auto* status=dialog->findChild<QLabel*>("text-alignment-editor-status");if(!editor||!unlink||!status)return;
        unlink->setChecked(true);editor->setCurrentIndex(2);
        auto next=*session.document().objects.at(source_id).text;next.alignment="end";
        session.apply({UpdateText{source_id,next}},session.revision());window.host.edited();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        alignment_stale_preserved=dialog->isVisible()&&session.document().objects.at(id).text->alignment_driver.has_value()&&
            session.revision()==linked_alignment_revision+1&&status->text().contains("changed while the alignment editor was open");dialog->reject();
    });
    visible_child<QToolButton>(window,"text-alignment-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(alignment_stale_preserved,"Stale Text alignment Apply leaves the committed driver and evaluation untouched");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-alignment-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-alignment-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-alignment-driver");
        if(!editor||!unlink)return;
        unlink->setChecked(true);editor->setCurrentIndex(2);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    });
    visible_child<QToolButton>(window,"text-alignment-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(!session.document().objects.at(id).text->alignment_driver&&session.document().objects.at(id).text->alignment=="end"&&
        session.revision()==linked_alignment_revision+2,"Text alignment unlink and edit commit as one atomic Session revision");
    changed_alignment_source=*session.document().objects.at(source_id).text;changed_alignment_source.alignment="center";
    session.apply({UpdateText{source_id,changed_alignment_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(session.document().objects.at(id).text->alignment=="end"&&!session.document().objects.at(id).text->alignment_driver,
        "Unlinked Text alignment stays frozen when its former source changes");
    auto* locale_driver_button=visible_child<QToolButton>(window,"text-locale-driver");
    check(visible_child<QLineEdit>(window,"text-locale")->isReadOnly()&&locale_driver_button->menu()->actions().size()==3,
        "Text locale Inspector exposes a staged literal display and explicit edit/link/unlink menu");
    const auto locale_initial_revision=session.revision();bool locale_cancel_safe=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("text-locale-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QLineEdit*>("text-locale-editor");if(!editor){dialog->reject();return;}
        editor->setText("fr-FR");locale_cancel_safe=session.revision()==locale_initial_revision&&
            session.document().objects.at(id).text->locale=="ja-JP";dialog->reject();});
    locale_driver_button->menu()->actions().front()->trigger();QApplication::processEvents();
    check(locale_cancel_safe&&session.revision()==locale_initial_revision&&session.document().objects.at(id).text->locale=="ja-JP",
        "Text locale Cancel discards its staged string without changing authored state or revision");
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("text-locale-dialog");if(!dialog)return;
        dialog->findChild<QLineEdit*>("text-locale-editor")->setText("fr-FR");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
    locale_driver_button=visible_child<QToolButton>(window,"text-locale-driver");locale_driver_button->menu()->actions().front()->trigger();QApplication::processEvents();
    check(session.document().objects.at(id).text->locale=="fr-FR"&&session.revision()==locale_initial_revision+1,
        "Applying a staged Text locale commits one shared Session revision");
    bool chose_locale_source=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget))
        if(auto* combo=dialog->findChild<QComboBox*>()) {
            combo->setCurrentText(QString::fromStdString(source_name)+" — "+QString::fromStdString(source_id));
            dialog->accept();chose_locale_source=true;return;
        }});
    locale_driver_button=visible_child<QToolButton>(window,"text-locale-driver");locale_driver_button->menu()->actions().at(1)->trigger();QApplication::processEvents();
    check(chose_locale_source&&session.document().objects.at(id).text->locale=="fr-FR"&&
        session.document().objects.at(id).text->locale_driver->link==Ref{source_id,"","text.locale"}&&
        evaluate_text_locale(session.document(),id)=="ar-SA"&&
        visible_child<QLineEdit>(window,"text-locale")->text()=="ar-SA"&&
        visible_child<QLabel>(window,"text-locale-state")->text().contains("Literal: fr-FR")&&
        visible_child<QLabel>(window,"text-locale-state")->text().contains("Evaluated: ar-SA"),
        "Text locale Inspector links the same field and shows literal, source and evaluated value");
    const auto linked_locale_revision=session.revision();bool locale_apply_failed=false,locale_cancel_with_driver=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("text-locale-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QLineEdit*>("text-locale-editor");auto* status=dialog->findChild<QLabel*>("text-locale-editor-status");
        editor->setText("de-DE");dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        locale_apply_failed=dialog->isVisible()&&session.revision()==linked_locale_revision&&
            session.document().objects.at(id).text->locale_driver.has_value()&&status->text().contains("unlink option");dialog->reject();});
    visible_child<QToolButton>(window,"text-locale-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(locale_apply_failed,"Failed driven Text locale Apply retains its driver and Session revision");
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("text-locale-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QLineEdit*>("text-locale-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-locale-driver");
        unlink->setChecked(true);editor->setText("de-DE");locale_cancel_with_driver=session.revision()==linked_locale_revision&&
            session.document().objects.at(id).text->locale_driver->link==Ref{source_id,"","text.locale"};dialog->reject();});
    visible_child<QToolButton>(window,"text-locale-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(locale_cancel_with_driver&&session.revision()==linked_locale_revision&&
        session.document().objects.at(id).text->locale_driver->link==Ref{source_id,"","text.locale"},
        "Cancel discards both a staged locale edit and its staged unlink");
    bool locale_stale_preserved=false;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("text-locale-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QLineEdit*>("text-locale-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-locale-driver");
        auto* status=dialog->findChild<QLabel*>("text-locale-editor-status");unlink->setChecked(true);editor->setText("de-DE");
        auto next=*session.document().objects.at(source_id).text;next.locale="ja-JP";
        session.apply({UpdateText{source_id,next}},session.revision());window.host.edited();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        locale_stale_preserved=dialog->isVisible()&&session.document().objects.at(id).text->locale_driver.has_value()&&
            session.revision()==linked_locale_revision+1&&status->text().contains("changed while the locale editor was open");dialog->reject();});
    visible_child<QToolButton>(window,"text-locale-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(locale_stale_preserved,"Stale Text locale Apply preserves the committed driver and evaluation");
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("text-locale-dialog");if(!dialog)return;
        dialog->findChild<QCheckBox*>("unlink-text-locale-driver")->setChecked(true);
        dialog->findChild<QLineEdit*>("text-locale-editor")->setText("en-GB");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
    visible_child<QToolButton>(window,"text-locale-driver")->menu()->actions().front()->trigger();QApplication::processEvents();
    check(!session.document().objects.at(id).text->locale_driver&&session.document().objects.at(id).text->locale=="en-GB"&&
        session.revision()==linked_locale_revision+2,"Text locale unlink and edit commit as one atomic Session revision");
    auto* locale_menu_button=visible_child<QToolButton>(window,"text-locale-driver");bool chose_locale_source_again=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget))
        if(auto* combo=dialog->findChild<QComboBox*>()) {
            combo->setCurrentText(QString::fromStdString(source_name)+" — "+QString::fromStdString(source_id));
            dialog->accept();chose_locale_source_again=true;return;
        }});
    locale_menu_button->menu()->actions().at(1)->trigger();QApplication::processEvents();
    locale_menu_button=visible_child<QToolButton>(window,"text-locale-driver");locale_menu_button->menu()->actions().at(2)->trigger();QApplication::processEvents();
    auto changed_locale_source=*session.document().objects.at(source_id).text;changed_locale_source.locale="ar-SA";
    session.apply({UpdateText{source_id,changed_locale_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(chose_locale_source_again&&!session.document().objects.at(id).text->locale_driver&&
        session.document().objects.at(id).text->locale=="ja-JP",
        "Unlink locale freezes the evaluated value while later source changes leave it unchanged");
    auto* family_driver=visible_child<QToolButton>(window,"text-family-driver");bool chose_family_source=false;
    check(visible_child<QComboBox>(window,"text-family")->isEnabled()&&family_driver->menu()->actions().size()==2,
        "Text family Inspector exposes a literal control and link/edit menu");
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        if(auto* combo=dialog->findChild<QComboBox*>()) {
            combo->setCurrentText(QString::fromStdString(source_name)+" — "+QString::fromStdString(source_id));
            dialog->accept();chose_family_source=true;return;
        }
    }});
    family_driver->menu()->actions().front()->trigger();QApplication::processEvents();
    check(chose_family_source&&session.document().objects.at(id).text->family==original_family.toStdString()&&
        session.document().objects.at(id).text->family_driver->link==Ref{source_id,"","text.family"}&&
        evaluate_text_family(session.document(),id)=="Linked Family A"&&
        !visible_child<QComboBox>(window,"text-family")->isEnabled()&&
        visible_child<QLabel>(window,"text-family-state")->text().contains("Evaluated: Linked Family A"),
        "Text Family Inspector links through Session and reports literal, source and evaluated family");
    auto changed_family_source=*session.document().objects.at(source_id).text;changed_family_source.family="Linked Family revised";
    session.apply({UpdateText{source_id,changed_family_source}},session.revision());window.host.edited();QApplication::processEvents();
    bool family_apply_failed=false,family_cancel_safe=false;
    const auto before_family_draft=session.revision();
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-family-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-family-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-family-driver");
        if(!editor||!unlink)return;
        editor->setEditText("");unlink->click();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        family_apply_failed=dialog->isVisible()&&session.revision()==before_family_draft&&
            session.document().objects.at(id).text->family_driver&&dialog->findChild<QLabel*>("text-family-editor-status")->text().contains("limit");
        editor->setEditText("Canceled family draft");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
        family_cancel_safe=!dialog->isVisible()&&session.revision()==before_family_draft&&
            session.document().objects.at(id).text->family==original_family.toStdString()&&
            session.document().objects.at(id).text->family_driver->link==Ref{source_id,"","text.family"};
    });
    family_driver=visible_child<QToolButton>(window,"text-family-driver");family_driver->menu()->actions().back()->trigger();QApplication::processEvents();
    check(family_apply_failed&&family_cancel_safe,
        "Invalid Apply and Cancel keep the linked family literal, driver and Session revision intact");
    bool family_unlink_applied=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-family-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QComboBox*>("text-family-editor");auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-family-driver");
        if(!editor||!unlink)return;
        editor->setEditText("Chosen family");unlink->click();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        family_unlink_applied=!dialog->isVisible()&&session.revision()==before_family_draft+1&&
            session.document().objects.at(id).text->family=="Chosen family"&&
            !session.document().objects.at(id).text->family_driver;
    });
    family_driver=visible_child<QToolButton>(window,"text-family-driver");family_driver->menu()->actions().back()->trigger();QApplication::processEvents();
    check(family_unlink_applied&&visible_child<QComboBox>(window,"text-family")->isEnabled(),
        "Family unlink and selected font commit together in one Session revision");
    changed_family_source=*session.document().objects.at(source_id).text;changed_family_source.family="Later Family source";
    session.apply({UpdateText{source_id,changed_family_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(!session.document().objects.at(id).text->family_driver&&evaluate_text_family(session.document(),id)=="Chosen family",
        "Inspector family unlink freezes the chosen family after later source changes");
    auto weight_source=*session.document().objects.at(source_id).text;
    auto* weight_driver=visible_child<QToolButton>(window,"text-weight-driver");
    check(visible_child<QSpinBox>(window,"text-weight")->isEnabled()&&weight_driver->menu()->actions().size()==2,
        "Text Weight Inspector exposes a literal control and a link/unlink menu");
    bool chose_weight_source=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        if(auto* combo=dialog->findChild<QComboBox*>()) {
            combo->setCurrentText(QString::fromStdString(source_name)+" — "+QString::fromStdString(source_id));
            dialog->accept();chose_weight_source=true;return;
        }
    }});
    weight_driver->menu()->actions().front()->trigger();QApplication::processEvents();
    check(chose_weight_source&&session.document().objects.at(id).text->weight_driver->link==Ref{source_id,"","text.weight"}&&
        evaluate_text_weight(session.document(),id)==700&&visible_child<QSpinBox>(window,"text-weight")->value()==700&&
        !visible_child<QSpinBox>(window,"text-weight")->isEnabled()&&visible_child<QLabel>(window,"text-weight-state")->text().contains("Evaluated: 700"),
        "Text Weight Inspector links through Session and displays the evaluated integer");
    auto changed_weight_source=*session.document().objects.at(source_id).text;changed_weight_source.weight=300;
    session.apply({UpdateText{source_id,changed_weight_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(visible_child<QSpinBox>(window,"text-weight")->value()==300&&
        visible_child<QLabel>(window,"text-weight-state")->text().contains("Literal: 400"),
        "Text Weight Inspector follows source edits while retaining the target literal");
    weight_driver=visible_child<QToolButton>(window,"text-weight-driver");weight_driver->menu()->actions().back()->trigger();QApplication::processEvents();
    changed_weight_source=*session.document().objects.at(source_id).text;changed_weight_source.weight=500;
    session.apply({UpdateText{source_id,changed_weight_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(!session.document().objects.at(id).text->weight_driver&&session.document().objects.at(id).text->weight==300&&
        evaluate_text_weight(session.document(),id)==300&&visible_child<QSpinBox>(window,"text-weight")->isEnabled(),
        "Inspector unlink freezes the evaluated weight and restores its literal editor");
    auto content_source=*session.document().objects.at(source_id).text;content_source.content="Linked source content";
    session.apply({UpdateText{source_id,content_source}},session.revision());window.host.edited();QApplication::processEvents();
    auto* content_driver=visible_child<QToolButton>(window,"text-content-driver");bool chose_content_source=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        if(auto* combo=dialog->findChild<QComboBox*>()) {
            combo->setCurrentText(QString::fromStdString(source_name)+" — "+QString::fromStdString(source_id));
            dialog->accept();chose_content_source=true;return;
        }
    }});
    content_driver->menu()->actions().front()->trigger();QApplication::processEvents();
    check(chose_content_source&&session.document().objects.at(id).text->content=="External edit"&&
        session.document().objects.at(id).text->content_driver->link==Ref{source_id,"","text.content"}&&
        evaluate_text_content(session.document(),id)=="Linked source content"&&
        visible_child<QLabel>(window,"text-preview")->text()=="Linked source content",
        "Text Content Inspector links through Session and previews the evaluated string while preserving its literal");
    content_source=*session.document().objects.at(source_id).text;content_source.content="Revised linked source";
    session.apply({UpdateText{source_id,content_source}},session.revision());window.host.edited();QApplication::processEvents();
    bool canceled_unlink=false,blocked_draft=false,unlink_staged=false,committed_draft=false;
    const auto before_content_edit=session.revision();
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-editor-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");if(!editor)return;
        editor->setPlainText("Canceled draft");
        auto* unlink_button=dialog->findChild<QPushButton*>("unlink-text-content-driver");
        if(!unlink_button||!unlink_button->isVisible())return;
        unlink_button->click();
        canceled_unlink=editor->toPlainText()=="Canceled draft"&&
            session.document().objects.at(id).text->content_driver->link==Ref{source_id,"","text.content"}&&
            session.document().objects.at(id).text->content=="External edit"&&session.revision()==before_content_edit;
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
    });
    visible_child<QPushButton>(window,"edit-text-content")->click();QApplication::processEvents();
    check(canceled_unlink&&session.revision()==before_content_edit&&session.document().objects.at(id).text->content_driver,
        "Cancel discards a staged unlink and text draft without changing the Session");
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-editor-dialog");if(!dialog)return;
        auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");if(!editor)return;
        editor->setPlainText("Draft across unlink");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        blocked_draft=dialog->isVisible()&&editor->toPlainText()=="Draft across unlink"&&
            dialog->findChild<QLabel*>("text-editor-status")->text().contains("Unlink")&&session.revision()==before_content_edit;
        auto* unlink_button=dialog->findChild<QPushButton*>("unlink-text-content-driver");
        if(!unlink_button||!unlink_button->isVisible())return;
        unlink_button->click();
        unlink_staged=editor->toPlainText()=="Draft across unlink"&&
            session.document().objects.at(id).text->content_driver->link==Ref{source_id,"","text.content"}&&
            session.document().objects.at(id).text->content=="External edit"&&session.revision()==before_content_edit&&
            dialog->findChild<QLabel*>("text-editor-status")->text().contains("staged");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        committed_draft=!dialog->isVisible()&&session.document().objects.at(id).text->content=="Draft across unlink"&&
            !session.document().objects.at(id).text->content_driver&&session.revision()==before_content_edit+1;
    });
    visible_child<QPushButton>(window,"edit-text-content")->click();QApplication::processEvents();
    check(blocked_draft&&unlink_staged&&committed_draft,
        "The content editor stages unlink, then commits the freeze and text draft in one Session revision");
    content_source=*session.document().objects.at(source_id).text;content_source.content="Later source content";
    session.apply({UpdateText{source_id,content_source}},session.revision());window.host.edited();QApplication::processEvents();
    check(!session.document().objects.at(id).text->content_driver&&
        evaluate_text_content(session.document(),id)=="Draft across unlink",
        "Inspector content unlink keeps the committed draft frozen after later source edits");
}
void text_path_authoring(Window& window) {
    auto& session=window.host.session;const auto composition=session.document().compositions.front().id;
    Point start,end;start.id="ui-path-start";start.x.literal=40;start.y.literal=100;
    end.id="ui-path-end";end.x.literal=520;end.y.literal=100;
    auto source=default_text("ui-text-source","Canvas");source.parameters.at("font_size").literal=24;
    session.apply({CreatePath{composition,"","ui-path","Guide Curve",{{"ui-contour",false,{start,end}}}},
        CreateText{composition,"","ui-text","Path Label",source}},session.revision());
    window.host.edited();window.canvas->set_selection("ui-text");QApplication::processEvents();
    auto* contour=visible_child<QComboBox>(window,"text-path-contour");
    const auto stable=contour->findData(QStringLiteral("ui-path\nui-contour"));
    check(stable>=0&&contour->itemText(stable).contains("ui-path")&&contour->itemText(stable).contains("ui-contour"),
        "Text on Path Inspector displays explicit stable Path and Contour IDs");
    const auto before_refusal=session.document();const auto refusal_revision=session.revision();
    contour->setCurrentIndex(0);visible_child<QPushButton>(window,"text-path-apply")->click();QApplication::processEvents();
    check(session.revision()==refusal_revision&&session.document()==before_refusal&&
        window.statusBar()->currentMessage().startsWith("MISSING_PATH_ATTACHMENT"),
        "Inspector refuses attachment until a specific same-Composition Path/Contour is chosen");

    const auto detached_pixels=Canvas::render_artboard(session.document(),composition,window.canvas->active_artboard(),1,false);
    contour=visible_child<QComboBox>(window,"text-path-contour");contour->setCurrentIndex(stable);
    const auto attach_revision=session.revision();visible_child<QPushButton>(window,"text-path-apply")->click();QApplication::processEvents();
    QTest::qWait(25); // Let the deferred Text Inspector and Qt layout/show cycle settle.
    check(session.revision()==attach_revision+1&&
        session.document().objects.at("ui-text").text->path_attachment==TextPathAttachment{"ui-path","ui-contour","distance",0,0,false},
        "Inspector attaches by stable IDs through one shared Session edit");
    const auto attached_pixels=Canvas::render_artboard(session.document(),composition,window.canvas->active_artboard(),1,false);
    const auto attached_svg=export_svg(session.document(),composition,window.canvas->active_artboard());
    check(attached_pixels!=detached_pixels&&attached_svg.find("Text outlined for SVG")!=std::string::npos&&
        attached_svg.find("<text") == std::string::npos,
        "Canvas and SVG render the same real Text-on-Path shape while native retains editable Text");
    QTemporaryDir native_dir;check(native_dir.isValid(),"Text-on-Path creates a disposable cold-reopen directory");
    const auto native_path=native_dir.path()+"/text-path.nect";window.host.save(native_path);
    QApplication::processEvents();QTest::qWait(25);
    visible_child<QPushButton>(window,"text-path-detach");
    Host cold_reopen(native_dir.path()+"/recovery");cold_reopen.open(native_path);
    check(cold_reopen.session.document()==session.document()&&
        cold_reopen.session.document().objects.at("ui-text").text->path_attachment->contour=="ui-contour",
        "Native 0.27 cold reopen preserves exact editable Text and stable Contour attachment IDs");
    const auto attached_document=session.document();const auto detach_revision=session.revision();
    visible_child<QPushButton>(window,"text-path-detach")->click();QApplication::processEvents();
    check(session.revision()==detach_revision+1&&!session.document().objects.at("ui-text").text->path_attachment&&
        session.document().objects.at("ui-text").text->content=="Canvas",
        "Inspector detaches in one Session edit and preserves authored Text");
    session.undo(session.revision());window.host.edited();QApplication::processEvents();
    check(session.document()==attached_document,"Inspector detach has one exact Undo step");
}
QPointF knob_point(double degrees) {
    const auto radians=degrees*std::acos(-1.0)/180.0;
    return {22.0+16.0*std::sin(radians),22.0-16.0*std::cos(radians)};
}
void knob_mouse(QWidget* knob,QEvent::Type type,QPointF local,Qt::MouseButton button,Qt::MouseButtons buttons) {
    const QPointF global=QPointF(knob->mapToGlobal(QPoint(0,0)))+local;
    QMouseEvent event(type,local,global,button,buttons,Qt::NoModifier);
    QApplication::sendEvent(knob,&event);QApplication::processEvents();
}
void p02d_utility_acceptance(Window& window) {
    auto& session=window.host.session;
    const auto composition=session.document().compositions.front().id;
    const auto artboard=session.document().compositions.front().artboards.front().id;
    auto find_board=[&](const Document& document)->const Artboard& {
        const auto owner=std::find_if(document.compositions.begin(),document.compositions.end(),[&](const Composition& value){return value.id==composition;});
        check(owner!=document.compositions.end(),"Layout fixture composition remains present");
        const auto item=std::find_if(owner->artboards.begin(),owner->artboards.end(),[&](const Artboard& value){return value.id==artboard;});
        check(item!=owner->artboards.end(),"Layout fixture Artboard remains present");return *item;
    };
    auto board=session.document().compositions.front().artboards.front();
    board.width=960;board.height=640;
    session.apply({UpdateArtboard{composition,board},
        SetArtboardLayout{composition,artboard,ArtboardLayout{Margin{40,30,50,35},Grid{"utility-grid",{40,30,870,575},3,2,10,10}}},
        AddGuide{composition,Guide{"utility-guide","Utility guide","x",70}}},session.revision());
    window.host.edited();QApplication::processEvents();
    const Document setup_document=session.document();
    const auto revision=session.revision();
    auto* strip=window.findChild<QScrollArea*>("canvas-utility-scroll");
    auto* guides=window.findChild<QToolButton*>("utility-show-guides");
    auto* grid=window.findChild<QToolButton*>("utility-show-grid");
    auto* snap=window.findChild<QToolButton*>("utility-snap");
    auto* zoom=window.findChild<QDoubleSpinBox*>("canvas-zoom-percent");
    auto* readback=window.findChild<QLabel*>("canvas-output-readback");
    auto* fit=window.findChild<QToolButton*>("utility-fit");
    auto* setup=window.findChild<QToolButton*>("utility-setup");
    auto* keys=window.findChild<QToolButton*>("utility-shortcut-help");
    check(strip&&guides&&grid&&snap&&zoom&&readback&&fit&&setup&&keys,"Utility strip exposes every fixed control");
    check(readback->text().contains("960 × 640")||readback->accessibleDescription().contains("960 × 640"),
        (std::string("Utility readback names active Artboard output dimensions: ")+readback->text().toStdString()+" / "+readback->accessibleDescription().toStdString()).c_str());
    guides->setChecked(false);grid->setChecked(true);snap->setChecked(true);QApplication::processEvents();
    check(!window.canvas->show_guides()&&window.canvas->show_grid()&&window.canvas->snap_enabled(),
        "Guide visibility, Grid visibility and Snap remain independent view states");
    snap->click();QApplication::processEvents();
    check(!window.canvas->snap_enabled()&&window.canvas->show_grid(),"Snap can turn OFF while the Grid overlay stays ON");
    zoom->setValue(75);QApplication::processEvents();
    check(std::abs(window.canvas->zoom()-0.75)<1e-9,"Zoom control updates view scale");
    fit->click();QApplication::processEvents();
    check(session.revision()==revision,"Overlay, Snap, Fit and zoom changes create no document revision");

    window.resize(1000,650);QApplication::processEvents();
    auto* bar=strip->horizontalScrollBar();
    check(bar->maximum()>0&&strip->height()>=58&&strip->viewport()->height()>=30,
        "Narrow Utility Strip scrolls horizontally with enough height for controls and scrollbar");
    const auto stable_setup_rect=setup->geometry();
    strip->ensureWidgetVisible(setup);QApplication::processEvents();
    check(setup->visibleRegion().contains(setup->rect().center()),"Setup remains reachable at the horizontal end of the narrow strip");
    QTest::mouseMove(setup,setup->rect().center());QTest::qWait(150);
    check(setup->geometry()==stable_setup_rect,"Hover feedback leaves the Setup hit target geometry stable");
    strip->ensureWidgetVisible(keys);QApplication::processEvents();
    check(keys->visibleRegion().contains(keys->rect().center()),"Shortcut help remains reachable at narrow width");

    strip->ensureWidgetVisible(setup);QApplication::processEvents();setup->click();QApplication::processEvents();
    auto* popup=visible_child<QDialog>(window,"utility-setup-popover");
    auto* screen=popup->screen();check(screen!=nullptr,"Setup popup has an assigned screen");
    const auto available=screen->availableGeometry();
    check(popup->width()<=std::min(480,std::max(1,available.width()-16))&&
        popup->geometry().left()>=available.left()&&popup->geometry().right()<=available.right(),
        "Setup popup width and position fit a narrow available screen");
    check(visible_child<QGroupBox>(window,"layout-margin")&&visible_child<QGroupBox>(window,"layout-grid")&&
        visible_child<QGroupBox>(window,"composition-guides"),"Setup popover reuses the existing Margin, Grid and Guide editors");
    auto* popup_scroll=popup->findChild<QScrollArea*>("utility-setup-scroll");
    check(popup_scroll!=nullptr,"Setup popover scrolls its existing editor content");
    auto popup_field=[&](const char* name)->QLineEdit* {
        for(auto* field:window.findChildren<QLineEdit*>(QString::fromLatin1(name)))if(popup->isAncestorOf(field))return field;
        return nullptr;
    };
    auto* margin=popup_field("margin-left");check(margin!=nullptr,"Setup popover owns an existing Margin editor");
    popup_scroll->ensureWidgetVisible(margin);QApplication::processEvents();
    check(margin->isVisible(),"Setup popover exposes its Margin field inside the scroll viewport");
    margin->setFocus();QTest::keyClick(margin,Qt::Key_A,Qt::ControlModifier);
    QTest::keyClicks(margin,"50");QApplication::processEvents();
    const auto& preview=find_board(session.preview_document());
    check(preview.layout&&preview.layout->margin&&preview.layout->margin->left==50&&session.revision()==revision,
        "Margin editing previews transiently without changing authored revision");
    QTest::keyClick(margin,Qt::Key_Escape);QApplication::processEvents();
    check(!session.gesture_active()&&session.preview_document()==session.document()&&session.revision()==revision,
        "Escape cancels a Margin preview atomically");

    margin=popup_field("margin-left");check(margin!=nullptr,"Escape refresh keeps the Margin editor in Setup");popup_scroll->ensureWidgetVisible(margin);margin->setFocus();
    QTest::keyClick(margin,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(margin,"55");QApplication::processEvents();
    popup->reject();QApplication::processEvents();
    check(!session.gesture_active()&&session.preview_document()==session.document()&&session.revision()==revision,
        "Closing Setup cancels its active transient draft");

    setup->click();QApplication::processEvents();popup=visible_child<QDialog>(window,"utility-setup-popover");
    popup_scroll=popup->findChild<QScrollArea*>("utility-setup-scroll");
    margin=popup_field("margin-left");check(margin!=nullptr,"Reopened Setup exposes its Margin editor");popup_scroll->ensureWidgetVisible(margin);margin->setFocus();
    QTest::keyClick(margin,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(margin,"50");QApplication::processEvents();
    visible_child<QPushButton>(window,"margin-apply")->click();QApplication::processEvents();
    const auto& committed=find_board(session.document());
    check(session.revision()==revision+1&&committed.layout&&committed.layout->margin&&committed.layout->margin->left==50,
        "Margin Apply commits one authored transaction");
    check(committed.layout->grid&&committed.layout->grid->id=="utility-grid"&&committed.layout->grid->bounds.x==40,
        "Margin edits preserve the independently authored Grid bounds and identity");
    session.undo(session.revision());window.host.edited();QApplication::processEvents();
    check(session.document()==setup_document,"Setup commit is one exact Undo");
    if(auto* active=window.findChild<QDialog*>("utility-setup-popover"))active->reject();QApplication::processEvents();
}
void p02d_shortcut_acceptance(Window& window) {
    auto* draw=named_action(window,"draw-path");
    check(draw->shortcuts().contains(QKeySequence("P"))&&draw->shortcuts().contains(QKeySequence("G")),
        "P and G are aliases on the existing Draw Path action");
    check(named_action(window,"transform-selection")->shortcut()==QKeySequence("Ctrl+Shift+T"),
        "Existing Nect rotate/scale shortcut remains Ctrl+Shift+T");
    QLineEdit text(&window);text.show();text.setFocus();QApplication::processEvents();QTest::keyClick(&text,Qt::Key_G);
    check(text.text()=="g"&&!window.canvas->draw_mode(),"G typed in a focused text field stays text input");text.hide();
    window.canvas->setFocus();QApplication::processEvents();QTest::keyClick(window.canvas,Qt::Key_P);
    check(window.canvas->draw_mode(),"P enters Draw Path on the Canvas");QTest::keyClick(window.canvas,Qt::Key_Escape);
    QTest::keyClick(window.canvas,Qt::Key_G);check(window.canvas->draw_mode(),"G enters the same Draw Path mode");
    QTest::keyClick(window.canvas,Qt::Key_Escape);
    named_action(window,"shortcut-help")->trigger();QApplication::processEvents();
    auto* help=visible_child<QDialog>(window,"shortcut-help-dialog");QString copy;
    for(auto* label:help->findChildren<QLabel*>())copy+=label->text()+"\n";
    check(copy.contains("P = Position")&&copy.contains("G = Pen / Mask Feather")&&copy.contains("Ctrl+Shift+T = Effect Controls"),
        "Shortcut help explicitly names both Nect shortcuts and their AE collisions");help->close();QApplication::processEvents();
}
void p02d_repeater_knob_acceptance(Window& window) {
    auto& session=window.host.session;const auto composition=session.document().compositions.front().id;
    Point point;point.id="knob-point";point.x.literal=120;point.y.literal=120;
    const Ref rotation=operation_ref("knob-object","knob-repeater","rotation");
    session.apply({CreatePath{composition,"","knob-object","Knob",{{"knob-contour",false,{point}}}},
        AddOperation{"knob-object",default_operation("knob-repeater","nect.shape.repeater"),1}},session.revision());
    window.host.edited();window.canvas->set_selection("knob-object");QApplication::processEvents();
    auto* numeric=field<QLineEdit>(window,rotation);
    auto* knob=visible_child<QWidget>(window,"repeater-angle-knob-knob-repeater");reveal(window,knob);
    check(knob->property("nect-reference").toByteArray()==reference(rotation),"Knob and numeric editor carry the identical stable Rotation Ref");
    const auto knob_hit_target=knob->geometry();
    const auto knob_global=QPointF(knob->mapToGlobal(QPoint(0,0)))+QPointF(22,22);
    QEnterEvent pointer_enter({22,22},{22,22},knob_global);QApplication::sendEvent(knob,&pointer_enter);QTest::qWait(150);
    auto* hover=qobject_cast<QGraphicsOpacityEffect*>(knob->graphicsEffect());
    check(hover&&hover->opacity()>=0.99&&knob->geometry()==knob_hit_target,
        "Knob hover fades smoothly without changing its stable hit target");
    QEvent pointer_leave(QEvent::Leave);QApplication::sendEvent(knob,&pointer_leave);QTest::qWait(150);
    knob->setFocus();QApplication::processEvents();
    check(knob->hasFocus()&&std::abs(hover->opacity()-0.90)<0.01&&knob->geometry()==knob_hit_target,
        "Knob focus stays available as hover fades away without geometry movement");
    edit_number(window,rotation,"725");
    check(std::abs(evaluate(session.document()).at(rotation)-725)<1e-12,"Numeric rotation preserves exact unwrapped 725 degrees");
    knob=visible_child<QWidget>(window,"repeater-angle-knob-knob-repeater");reveal(window,knob);
    check(knob->accessibleDescription().contains("indicator 5 degrees"),"Knob indicates 725 degrees modulo 360");
    const auto plus_ten_revision=session.revision();
    knob_mouse(knob,QEvent::MouseButtonPress,{22,6},Qt::LeftButton,Qt::LeftButton);
    knob_mouse(knob,QEvent::MouseMove,knob_point(10),Qt::NoButton,Qt::LeftButton);
    knob_mouse(knob,QEvent::MouseButtonRelease,knob_point(10),Qt::LeftButton,Qt::NoButton);
    check(session.revision()==plus_ten_revision+1&&std::abs(evaluate(session.document()).at(rotation)-735)<1e-9,
        "A +10 degree dial drag commits 735 degrees in exactly one revision");
    knob=visible_child<QWidget>(window,"repeater-angle-knob-knob-repeater");reveal(window,knob);
    numeric=field<QLineEdit>(window,rotation);
    check(numeric->text().toDouble()==735&&knob->accessibleDescription().contains("indicator 15 degrees"),
        "Knob drag refreshes exact numeric editor and modulo indicator from the same Ref");
    session.undo(session.revision());window.host.edited();QApplication::processEvents();
    check(std::abs(evaluate(session.document()).at(rotation)-725)<1e-12,"One Undo restores numeric rotation 725");

    knob=visible_child<QWidget>(window,"repeater-angle-knob-knob-repeater");reveal(window,knob);
    const auto whole_turn_revision=session.revision();
    knob_mouse(knob,QEvent::MouseButtonPress,knob_point(0),Qt::LeftButton,Qt::LeftButton);
    for(int angle=45;angle<=360;angle+=45)knob_mouse(knob,QEvent::MouseMove,knob_point(angle%360),Qt::NoButton,Qt::LeftButton);
    knob_mouse(knob,QEvent::MouseButtonRelease,knob_point(0),Qt::LeftButton,Qt::NoButton);
    check(session.revision()==whole_turn_revision+1&&std::abs(evaluate(session.document()).at(rotation)-1085)<1e-8,
        "Sampled full-circle travel adds 360 degrees without wrapping the authored value");

    knob=visible_child<QWidget>(window,"repeater-angle-knob-knob-repeater");reveal(window,knob);
    const auto cancel_revision=session.revision();
    knob_mouse(knob,QEvent::MouseButtonPress,knob_point(0),Qt::LeftButton,Qt::LeftButton);
    knob_mouse(knob,QEvent::MouseMove,knob_point(25),Qt::NoButton,Qt::LeftButton);
    check(std::abs(evaluate(session.preview_document()).at(rotation)-1110)<0.5,"Live angle drag previews through the shared Session gesture");
    QTest::keyClick(knob,Qt::Key_Escape);QApplication::processEvents();
    check(session.revision()==cancel_revision&&!session.gesture_active()&&std::abs(evaluate(session.document()).at(rotation)-1085)<1e-8,
        "Escape cancels knob preview with no revision or partial authored change");

    const auto old_knob_revision=session.revision();
    session.apply({Set{rotation,1100}},session.revision());
    knob_mouse(knob,QEvent::MouseButtonPress,knob_point(0),Qt::LeftButton,Qt::LeftButton);
    knob_mouse(knob,QEvent::MouseButtonRelease,knob_point(0),Qt::LeftButton,Qt::NoButton);
    check(session.revision()==old_knob_revision+1&&!session.gesture_active()&&
        window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT")&&evaluate(session.document()).at(rotation)==1100,
        "A stale knob refuses to start and leaves external authored value intact");
    window.host.edited();QApplication::processEvents();

    edit_number(window,rotation,"-999999999");
    check(evaluate(session.document()).at(rotation)==-999999999,
        "Signed near-limit numeric rotation stays unwrapped in the authored Ref");
    const auto bounded_revision=session.revision();
    edit_number(window,rotation,"1000000001");
    check(session.revision()==bounded_revision&&evaluate(session.document()).at(rotation)==-999999999,
        "Out-of-range rotation rejects without a partial authored edit");
    window.host.edited();QApplication::processEvents();
    const Ref source_rotation=operation_ref("knob-object","knob-repeater-source","rotation");
    session.apply({AddOperation{"knob-object",default_operation("knob-repeater-source","nect.shape.repeater"),2},
        Set{source_rotation,120},Link{rotation,{source_rotation,1,0,"copy_local_value"}}},session.revision());
    window.host.edited();QApplication::processEvents();
    knob=visible_child<QWidget>(window,"repeater-angle-knob-knob-repeater");reveal(window,knob);
    check(!knob->isEnabled()&&std::abs(evaluate(session.document()).at(rotation)-120)<1e-12,
        "Driven rotation disables the dial and uses the same evaluated Ref");
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
        if(!source_field->visibleRegion().contains(source_field->rect().center())) {
            auto* scroll=w.findChild<QScrollArea*>("inspector-scroll");
            qWarning()<<"WHIP GEOMETRY"<<"field"<<source_field->geometry()<<"global"<<source_field->mapToGlobal(QPoint())
                <<"region"<<source_field->visibleRegion()<<"viewport"<<scroll->viewport()->geometry()
                <<"viewport-global"<<scroll->viewport()->mapToGlobal(QPoint())<<"content"<<scroll->widget()->geometry()
                <<"scroll"<<scroll->verticalScrollBar()->value()<<scroll->verticalScrollBar()->maximum();
            for(QWidget* widget=source_field;widget&&widget!=scroll->viewport();widget=widget->parentWidget())
                qWarning()<<"WHIP PARENT"<<widget->metaObject()->className()<<widget->geometry()<<widget->minimumSizeHint()<<widget->minimumSize();
            for(auto* combo:w.findChildren<QComboBox*>())if(combo->isVisible())
                qWarning()<<"WHIP COMBO"<<combo->geometry()<<combo->minimumSizeHint()<<combo->sizeHint();
        }
        check(source_field->visibleRegion().contains(source_field->rect().center()),
            "Cross-object pick-whip exposes source coordinates inside the scrolled Inspector");
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
        stack_authoring(w);
        w.hide();{Window enabled_links(temp.path()+"/enabled-links");enabled_links.show();QApplication::processEvents();
            single_operation_enabled_source(enabled_links);enabled_links.hide();}
        Window gradients(temp.path()+"/gradient");gradients.show();QApplication::processEvents();gradient_authoring(gradients);
        gradients.hide();Window boards(temp.path()+"/artboards");boards.show();QApplication::processEvents();artboard_authoring(boards);
        boards.hide();Window texts(temp.path()+"/texts");texts.show();QApplication::processEvents();text_authoring(texts);
        texts.hide();Window path_text(temp.path()+"/text-path");path_text.show();QApplication::processEvents();text_path_authoring(path_text);
        path_text.hide();Window layout(temp.path()+"/layout");layout.show();QApplication::processEvents();
        auto& layout_session=layout.host.session;const auto layout_comp=layout_session.document().compositions.front().id;
        Point left,right;left.id="layout-left-point";left.x.literal=30;left.y.literal=40;right.id="layout-right-point";right.x.literal=160;right.y.literal=100;
        layout_session.apply({CreatePath{layout_comp,"","layout-left","Left",{{"layout-left-contour",false,{left}}}},CreatePath{layout_comp,"","layout-right","Right",{{"layout-right-contour",false,{right}}}}},0);
        layout.host.edited();layout.canvas->set_selections({{"layout-left",""},{"layout-right",""}});
        const auto layout_before=layout_session.document();named_action(layout,"align-selection-x-min")->trigger();
        check(layout_session.revision()==2&&evaluate(layout_session.document()).at({"layout-right","","transform.tx"})==-130,"GUI alignment uses Session world translation");
        layout_session.undo(2);layout.host.edited();check(layout_session.document()==layout_before,"GUI alignment is one Undo");
        layout.canvas->set_selection("layout-left","layout-left-point");named_action(layout,"align-artboard-x-center")->trigger();
        check(layout_session.revision()==3&&layout.statusBar()->currentMessage().startsWith("INVALID_SELECTION"),"Point selection cannot silently align whole object");
        layout.canvas->set_selections({{"layout-left",""},{"layout-right",""}});QApplication::processEvents();
        visible_child<QComboBox>(layout,"alignment-target")->setCurrentIndex(1);
        visible_child<QPushButton>(layout,"quick-align-x-center")->click();
        const auto aligned_values=evaluate(layout_session.document());
        const auto active_board=evaluate_artboard(layout_session.document().compositions.front(),layout.canvas->active_artboard());
        const auto board_center=active_board.x+active_board.width/2;
        check(layout_session.revision()==4&&aligned_values.at({"layout-left","","transform.tx"})==board_center-30&&aligned_values.at({"layout-right","","transform.tx"})==board_center-160,"Quick alignment buttons share the Artboard Session command");
        layout_session.undo(4);layout.host.edited();
        Point third;third.id="layout-third-point";third.x.literal=400;third.y.literal=200;
        layout_session.apply({CreatePath{layout_comp,"","layout-third","Third",{{"layout-third-contour",false,{third}}}}},5);layout.host.edited();
        layout.canvas->set_selections({{"layout-third",""},{"layout-left",""},{"layout-right",""}});QApplication::processEvents();
        const auto before_spacing=layout_session.document();
        auto* layout_reference=visible_child<QComboBox>(layout,"alignment-target");layout_reference->setCurrentIndex(layout_reference->findData("selection"));
        visible_child<QPushButton>(layout,"quick-distribute-x")->click();
        check(layout_session.revision()==7&&evaluate(layout_session.document()).at({"layout-right","","transform.tx"})==55,"Selection distribution uses the explicit Selection reference");
        layout_session.undo(7);layout.host.edited();check(layout_session.document()==before_spacing,"GUI spacing is one Undo");
        layout.canvas->set_selection("layout-left");QApplication::processEvents();
        // Ordinary text editing retains Ctrl+A; Canvas selection remains intact.
        QLineEdit shortcut_text(&layout);shortcut_text.setText("editable draft");shortcut_text.show();shortcut_text.setFocus();QApplication::processEvents();
        QTest::keyClick(&shortcut_text,Qt::Key_A,Qt::ControlModifier);
        check(shortcut_text.selectedText()=="editable draft"&&layout.canvas->selected_objects()==std::vector<Id>{"layout-left"},"Ctrl+A remains local to a text field");
        shortcut_text.hide();layout.canvas->setFocus();
        named_action(layout,"fit-selection")->trigger();check(layout.canvas->zoom()==64,"Fit Selection menu frames degenerate point geometry");
        check(layout_session.revision()==8,"View/selection actions preserve authored revision");
        layout.canvas->set_selection("layout-left","layout-left-point");
        layout.canvas->nudge_selection(3,4);
        check(layout.canvas->evaluated_values()==evaluate(layout_session.document()),"Canvas notification retains exact committed projection");
        const auto original_status=layout.host.status_changed;bool reentered=false;
        layout.host.status_changed=[&]{
            if(!reentered){reentered=true;layout_session.apply({Set{{"layout-right","layout-right-point","x"},777}},layout_session.revision());}
            if(original_status)original_status();
        };
        layout.canvas->nudge_selection(1,0);
        layout.host.status_changed=original_status;
        check(reentered&&layout.canvas->evaluated_values().at({"layout-right","layout-right-point","x"})==777,"Reentrant revision change forces normal full projection");
        layout_session.undo(layout_session.revision());layout.host.edited();
        check(layout.canvas->evaluated_values()==evaluate(layout_session.document()),"External Undo refreshes projection after Canvas notification scope ends");
        layout.hide();Window folders(temp.path()+"/folders");folders.show();QApplication::processEvents();folder_action(folders);
        folders.hide();Window batch_rename(temp.path()+"/batch-rename");batch_rename.show();QApplication::processEvents();batch_rename_action(batch_rename);
        batch_rename.hide();Window sort_paint_order(temp.path()+"/sort-paint-order");sort_paint_order.show();QApplication::processEvents();sort_paint_order_action(sort_paint_order);
        sort_paint_order.hide();Window move_out(temp.path()+"/move-out");move_out.show();QApplication::processEvents();move_out_action(move_out);
        move_out.hide();Window transfer(temp.path()+"/folder-transfer");transfer.show();QApplication::processEvents();adjacent_folder_transfer_action(transfer);
        transfer.hide();Window chosen_transfer(temp.path()+"/chosen-transfer");chosen_transfer.show();QApplication::processEvents();explicit_folder_transfer_action(chosen_transfer);
        chosen_transfer.hide();Window folder_preview(temp.path()+"/folder-preview");folder_preview.show();QApplication::processEvents();batch_folder_preview_action(folder_preview);
        folder_preview.hide();Window stacking(temp.path()+"/stacking");stacking.show();QApplication::processEvents();stacking_authoring(stacking);
        stacking.hide();Window layout_setup(temp.path()+"/layout-setup");layout_setup.show();QApplication::processEvents();
        layout_setup_previews_commit_and_recovers(layout_setup);layout_setup.hide();
        Window layout_refs(temp.path()+"/layout-references");layout_refs.show();QApplication::processEvents();
        auto& refs_session=layout_refs.host.session;const auto refs_comp=refs_session.document().compositions.front().id;
        const auto refs_art=refs_session.document().compositions.front().artboards.front().id;
        Point ref_a;ref_a.id="ref-a-point";ref_a.x.literal=30;ref_a.y.literal=30;
        Point ref_b;ref_b.id="ref-b-point";ref_b.x.literal=100;ref_b.y.literal=30;
        refs_session.apply({SetArtboardLayout{refs_comp,refs_art,ArtboardLayout{std::nullopt,Grid{"layout-grid",{0,0,100,100},1,1,0,0}}},
            AddGuide{refs_comp,Guide{"layout-guide","Vertical guide","x",80}},
            CreatePath{refs_comp,"","ref-a","Reference A",{{"ref-a-contour",false,{ref_a}}}},
            CreatePath{refs_comp,"","ref-b","Reference B",{{"ref-b-contour",false,{ref_b}}}}},0);
        layout_refs.host.edited();layout_refs.canvas->set_selections({{"ref-a",""},{"ref-b",""}});QApplication::processEvents();
        auto* reference_picker=visible_child<QComboBox>(layout_refs,"alignment-target");
        check(reference_picker->findData("grid:layout-grid")>=0&&reference_picker->findData("guide:layout-guide")>=0&&
            reference_picker->findData("key_object:ref-a")>=0&&reference_picker->itemText(reference_picker->findData("grid:layout-grid")).contains("layout-grid"),
            "Inspector exposes stable Artboard/Grid/Guide/key identities before Apply");
        const auto refs_before=refs_session.document();const auto refs_revision=refs_session.revision();
        reference_picker->setCurrentIndex(reference_picker->findData("guide:layout-guide"));
        check(visible_child<QPushButton>(layout_refs,"quick-align-x-min")->isEnabled()&&
            !visible_child<QPushButton>(layout_refs,"quick-align-y-min")->isEnabled(),"Guide alignment enables only its declared axis");
        visible_child<QPushButton>(layout_refs,"quick-align-x-min")->click();
        check(refs_session.revision()==refs_revision+1,"Guide alignment commits through one Session command");
        refs_session.undo(refs_session.revision());layout_refs.host.edited();QApplication::processEvents();check(refs_session.document()==refs_before,"Guide alignment is one exact Undo");
        reference_picker=visible_child<QComboBox>(layout_refs,"alignment-target");reference_picker->setCurrentIndex(reference_picker->findData("key_object:ref-a"));
        auto* spacing_draft=visible_child<QLineEdit>(layout_refs,"distribution-spacing");auto* key_distribute=visible_child<QPushButton>(layout_refs,"quick-distribute-x");
        check(!key_distribute->isEnabled(),"Key-object Distribute is disabled until explicit spacing is entered");
        spacing_draft->setText("15");check(refs_session.revision()==refs_revision+2&&refs_session.document()==refs_before,"Spacing entry remains a non-mutating draft");
        check(key_distribute->isEnabled(),"Finite nonnegative key spacing enables Distribute");
        const auto key_object_before=refs_session.document().objects.at("ref-a");key_distribute->click();
        check(refs_session.revision()==refs_revision+3&&refs_session.document().objects.at("ref-a")==key_object_before,
            "Key-object UI distribution applies explicit spacing and keeps its key fixed");
        const auto key_result=refs_session.document();refs_session.undo(refs_session.revision());layout_refs.host.edited();
        check(refs_session.document()==refs_before&&key_result!=refs_before,"Key-object distribution is one Undo");layout_refs.hide();
        Window p02d(temp.path()+"/p02d");p02d.show();QApplication::processEvents();
        p02d_utility_acceptance(p02d);p02d_shortcut_acceptance(p02d);p02d.hide();
        Window repeater(temp.path()+"/p02d-repeater");repeater.show();QApplication::processEvents();
        p02d_repeater_knob_acceptance(repeater);repeater.hide();
        std::cout<<"PASS Inspector, P02-D strip/setup/knob/shortcuts, shapes/gradients, frames, Text editing and draft/focus preservation\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
