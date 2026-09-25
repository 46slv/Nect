#include "window.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QDebug>
#include <QTemporaryDir>
#include <QTest>
#include <QStatusBar>
#include <QPlainTextEdit>
#include <QTimer>
#include <QFile>
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
    choose("operation-fill-rule-",blue,1);
    const auto& blue_operation=session.document().objects.at(object).stack.back();
    check(blue_operation.fill_rule=="evenodd","Fill-rule control updates authored operation options");
    const auto enabled_name="operation-enabled-"+blue;
    auto* enabled=visible_child<QCheckBox>(window,enabled_name.c_str());reveal(window,enabled);
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
    const auto first=current().gradient->stops.front().id,last=current().gradient->stops.back().id;
    auto ref=[&](const std::string& field){return gradient_ref(object,op,gid,field);};
    check(current().gradient->stops.size()==2&&first!=last&&current().gradient->stops[0].rgba[3].literal==1&&
        current().gradient->stops[1].rgba[3].literal==1,"Initial stable stops do not duplicate the paint opacity");
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
    choose(0);check(!current().gradient->enabled&&current().gradient->id==gid,"Solid bypass retains the authored gradient");
    choose(1);check(current().gradient->id==gid&&current().gradient->enabled,"Re-enabling restores the same gradient identity");
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
        if(window.findChild<QScrollArea*>()->widget()->isAncestorOf(control))reveal(window,control);
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
        auto* control=visible_child<QLineEdit>(window,name);reveal(window,control);control->setFocus();
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

    input(position_name.c_str(),"nan",false);const auto before_delete=session.revision();
    check(window.statusBar()->currentMessage().contains("INVALID_VALUE")&&session.document().compositions.front().guides.front().position==25,
        "Nonfinite Guide input is rejected while the invalid draft remains visible");
    const std::string delete_name="guide-delete-"+guide_id;click(delete_name.c_str());
    check(session.revision()==before_delete+1&&session.document().compositions.front().guides.empty(),
        "Explicit Delete Guide discards an invalid draft and removes only that stable ID");

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
    visible_child<QComboBox>(window,"text-direction")->setCurrentIndex(1);QApplication::processEvents();
    check(session.document().objects.at(id).text->direction=="vertical","Writing mode changes through shared Session");
    visible_child<QComboBox>(window,"text-layout")->setCurrentIndex(1);QApplication::processEvents();
    check(session.document().objects.at(id).text->layout=="frame","Frame text retains content and source identity");
    check(visible_child<QLabel>(window,"text-layout-status")->text().contains("SVG exports glyph outlines"),"Inspector discloses export text projection");
    QApplication::setActiveWindow(&window);QApplication::processEvents();
    const Ref font_size{id,"","text.font_size"};auto* size=field<QLineEdit>(window,font_size);reveal(window,size);size->setFocus();
    const auto scroll=window.findChild<QScrollArea*>()->verticalScrollBar()->value();const auto focused_before=size->hasFocus();
    size->selectAll();QTest::keyClicks(size,"52");QTest::keyClick(size,Qt::Key_Return);QApplication::processEvents();
    if(!field<QLineEdit>(window,font_size)->hasFocus()||window.findChild<QScrollArea*>()->verticalScrollBar()->value()!=scroll)
        std::cerr<<"TEXT SCROLL "<<focused_before<<" "<<field<QLineEdit>(window,font_size)->hasFocus()<<" "<<scroll<<" "<<window.findChild<QScrollArea*>()->verticalScrollBar()->value()<<'\n';
    check(field<QLineEdit>(window,font_size)->hasFocus()&&window.findChild<QScrollArea*>()->verticalScrollBar()->value()==scroll,
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
            auto* scroll=w.findChild<QScrollArea*>();
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
        w.hide();Window gradients(temp.path()+"/gradient");gradients.show();QApplication::processEvents();gradient_authoring(gradients);
        gradients.hide();Window boards(temp.path()+"/artboards");boards.show();QApplication::processEvents();artboard_authoring(boards);
        boards.hide();Window texts(temp.path()+"/texts");texts.show();QApplication::processEvents();text_authoring(texts);
        texts.hide();Window layout(temp.path()+"/layout");layout.show();QApplication::processEvents();
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
        layout.hide();Window stacking(temp.path()+"/stacking");stacking.show();QApplication::processEvents();stacking_authoring(stacking);
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
        std::cout<<"PASS Inspector, pick-whip, shapes/gradients, frames, Text editing and draft/focus preservation\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
