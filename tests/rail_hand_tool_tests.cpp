#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
auto snapshot(Session& s){return std::tuple{s.document(),encode(s.document()),s.revision(),s.history()};}
void show(Window& w){w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1100,750);w.show();events();}
void click(Window& w,const char* name){
    auto* button=w.findChild<QToolButton*>(name);
    check(button&&button->isVisible()&&button->isEnabled(),"Requested real Rail tool is visible and enabled");
    QTest::mouseClick(button,Qt::LeftButton);events();
}
Document fixture(){
    auto d=empty_document("hand-document","hand-composition","hand-artboard");
    d.compositions.front().artboards.front().width=640;
    d.compositions.front().artboards.front().height=480;
    d.compositions.front().guides={{"hand-guide","Vertical","x",120}};
    Session s(d);auto fill=default_operation("hand-fill","nect.paint.fill");
    Gradient g;g.id="hand-gradient";g.enabled=true;g.end_x.literal=40;
    GradientStop a;a.id="hand-first";a.rgba[0].literal=1;a.rgba[1].literal=0;a.rgba[2].literal=0;
    GradientStop b=a;b.id="hand-last";b.offset.literal=1;g.stops={a,b};
    s.apply({CreatePrimitive{"hand-composition",{},"hand-object","Pan fixture",default_primitive("hand-shape","nect.shape.rectangle")},
        AddOperation{"hand-object",fill,0},SetGradient{"hand-object",fill.id,g},
        Set{{"hand-object",{},"transform.tx"},320},Set{{"hand-object",{},"transform.ty"},240},
        CreatePrimitive{"hand-composition",{},"hand-circle","Circle",default_primitive("hand-circle-source","nect.shape.circle")}},s.revision());
    return s.document();
}
// Observe the rendered artwork, rather than exposing a test-only viewport API.
QPointF red_center(Canvas& canvas){
    const auto image=canvas.grab().toImage();double xsum=0,ysum=0;int count=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
        const auto c=image.pixelColor(x,y);
        if(c.red()>180&&c.green()<80&&c.blue()<80){xsum+=x;ysum+=y;++count;}
    }
    check(count>100,"Rendered fixture has observable red artwork");
    return QPointF(xsum/count,ysum/count)/image.devicePixelRatio();
}
void pan(Window& w,Qt::MouseButton button,QPoint delta){
    const auto before=red_center(*w.canvas);const auto zoom=w.canvas->zoom();
    const auto authored=snapshot(w.host.session);const auto selection=w.canvas->selections();
    const QPoint start(w.canvas->width()/2,w.canvas->height()/2);
    QTest::mousePress(w.canvas,button,Qt::NoModifier,start);events();
    check(w.canvas->cursor().shape()==Qt::ClosedHandCursor,"Pan press uses closed-hand cursor");
    QTest::mouseMove(w.canvas,start+delta);events();
    check(!w.host.session.gesture_active()&&snapshot(w.host.session)==authored,
        "Pan movement never opens an authored gesture or changes full state");
    QTest::mouseRelease(w.canvas,button,Qt::NoModifier,start+delta);events();
    const auto moved=red_center(*w.canvas)-before;
    std::cout<<"Rendered pan delta="<<moved.x()<<','<<moved.y()<<" expected="<<delta.x()<<','<<delta.y()<<'\n';
    check(std::abs(moved.x()-delta.x())<0.6&&std::abs(moved.y()-delta.y())<0.6,
        "Actual pointer drag translates rendered artwork by the viewport delta");
    check(w.canvas->zoom()==zoom&&w.canvas->selections()==selection&&snapshot(w.host.session)==authored,
        "Completed pan preserves zoom, selection, complete Document/native/revision/history");
}
void exclusive(Window& w){
    int modes=w.canvas->hand_mode()+w.canvas->draw_mode()+w.canvas->text_mode()+w.canvas->anchor_edit()+
        w.canvas->guide_edit_mode()+w.canvas->circle_source_edit()+!w.canvas->gradient_operation().empty();
    int checked=0;
    for(const auto* name:{"tool-selection","tool-pen","tool-text","tool-anchor","tool-guide","tool-gradient","tool-hand"})
        checked+=w.findChild<QToolButton*>(name)->isChecked();
    check(modes==1&&checked==1,"Exactly one Canvas mode and one named Rail slot are active");
}
void key_isolation(const char* tool){
    QTemporaryDir scratch;check(scratch.isValid(),"Navigation key regression owns its files and settings");
    QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);show(w);
    auto& s=w.host.session;s=Session(fixture());w.host.edited();events();
    auto* tree=w.findChild<QTreeWidget*>();check(tree&&tree->isVisible(),"Actual Structure tree is reachable");
    QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator i(tree);*i;++i)
        if((*i)->data(0,Qt::UserRole).toString()=="hand-object"&&
            (*i)->data(0,Qt::UserRole+1).toString().isEmpty()){row=*i;break;}
    check(row!=nullptr,"Exact whole-Object Structure row exists");tree->scrollToItem(row);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();
    check(w.canvas->selected_object=="hand-object"&&w.canvas->selected_point.empty(),
        "Actual Structure pointer selects the whole artwork");
    w.canvas->fit_artboard();events();
    const auto authored=snapshot(s);click(w,tool);
    auto* button=w.findChild<QToolButton*>(tool);
    const auto active=[&]{return QString::fromLatin1(tool)=="tool-hand"?w.canvas->hand_mode():w.canvas->zoom_mode();};
    check(active()&&button->isChecked()&&snapshot(s)==authored,"Navigation activation preserves full authored state");
    const auto neutral=[&](Qt::Key key,Qt::KeyboardModifiers modifiers,bool repeat=false){
        const auto before=snapshot(s);const auto selection=w.canvas->selections();
        const auto center=red_center(*w.canvas);const auto zoom=w.canvas->zoom();
        QApplication::setActiveWindow(&w);w.canvas->setFocus();events();
        check(w.isActiveWindow()&&w.canvas->hasFocus(),"Arrow probe has actual active Window and Canvas focus");
        if(repeat){QKeyEvent event(QEvent::KeyPress,key,modifiers,QString{},true,1);QApplication::sendEvent(w.canvas,&event);}
        else QTest::keyClick(w.canvas,key,modifiers);
        events();
        std::cout<<tool<<" idle arrow: authored_equal="<<(snapshot(s)==before)<<" active="<<active()<<'\n';
        check(snapshot(s)==before&&!s.gesture_active(),"Idle navigation arrow cannot author artwork or point edits");
        check(active()&&button->isChecked()&&w.canvas->selections()==selection,
            "Arrow preserves persistent navigation Tool and exact selection");
        check(w.canvas->zoom()==zoom&&(red_center(*w.canvas)-center).manhattanLength()<0.01,
            "Idle navigation arrow leaves the viewport unchanged");
    };
    neutral(Qt::Key_Right,Qt::NoModifier);neutral(Qt::Key_Down,Qt::ShiftModifier);
    neutral(Qt::Key_Left,Qt::NoModifier,true);
    click(w,"tool-selection");
    const auto before=s.document();const auto revision=s.revision();const auto history=s.history().states.size();
    Session oracle(before);oracle.apply({TranslateObjects{{"hand-object"},1,0}},oracle.revision());
    QTest::keyClick(w.canvas,Qt::Key_Right);events();
    check(s.document()==oracle.document()&&s.revision()==revision+1&&s.history().states.size()==history+1,
        "Explicit Selection Arrow still authors one canonical TranslateObjects transaction");
    const auto moved=s.document();s.undo(s.revision());w.host.edited();events();
    check(s.document()==before,"One Undo restores the complete artwork");
    s.redo(s.revision());w.host.edited();events();check(s.document()==moved,"One Redo restores the complete movement");
    // Actual point discovery establishes a different selection context before returning to navigation.
    click(w,"tool-direct-selection");
    const auto& source=*before.objects.at("hand-object").source;
    const QPoint corner(qRound(w.canvas->width()/2.0+(1-source.parameters.at("width").literal/2)*w.canvas->zoom()),
        qRound(w.canvas->height()/2.0-source.parameters.at("height").literal/2*w.canvas->zoom()));
    QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,corner);events();
    check(w.canvas->selected_object=="hand-object"&&w.canvas->selected_point=="hand-shape-top-left",
        "Actual Direct Selection pointer discovers the retained stable anchor");
    click(w,tool);neutral(Qt::Key_Up,Qt::ShiftModifier);
    const auto file=scratch.filePath("navigation.nect.json");w.host.save(file);w.host.open(file);events();
    check(s.document()==moved&&active()&&button->isChecked(),"Same Window native reopen preserves source and navigation Tool");
    // Select through the live Structure again after Session replacement.
    tree=w.findChild<QTreeWidget*>();row=nullptr;
    for(QTreeWidgetItemIterator i(tree);*i;++i)
        if((*i)->data(0,Qt::UserRole).toString()=="hand-object"&&
            (*i)->data(0,Qt::UserRole+1).toString().isEmpty()){row=*i;break;}
    check(row!=nullptr,"Reopened exact Structure row exists");tree->scrollToItem(row);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();
    neutral(Qt::Key_Right,Qt::NoModifier);
    QTest::keyClick(w.canvas,Qt::Key_Escape);events();
    check(!active()&&w.findChild<QToolButton*>("tool-selection")->isChecked()&&s.document()==moved,
        "Explicit Escape returns navigation to Selection without authoring");
    std::cout<<"navigation_key_isolation: "<<tool<<' '<<checks<<" checks passed; DPR="<<w.devicePixelRatioF()<<'\n';
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try {
        if(app.arguments().contains("--hand-key-isolation")){key_isolation("tool-hand");return 0;}
        if(app.arguments().contains("--zoom-key-isolation")){key_isolation("tool-zoom");return 0;}
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");
        QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
        preferences.setValue("unrelated","preserve");preferences.setValue("workspace/tools/textCreationDirection","vertical");
        Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);show(w);
        auto& s=w.host.session;const auto original=fixture();s=Session(original);w.host.edited();events();
        s.apply({Set{{"hand-object",{},"transform.tx"},328}},s.revision());w.host.edited();events();
        w.canvas->set_selection("hand-object");events();w.canvas->fit_artboard();events();
        const auto before=snapshot(s);const auto selection=w.canvas->selections();
        auto* hand=w.findChild<QToolButton*>("tool-hand");auto* gradient=w.findChild<QToolButton*>("tool-gradient");
        check(hand&&hand->isVisible()&&hand->isEnabled()&&hand->accessibleName().startsWith("Hand")&&
            hand->focusPolicy()==Qt::StrongFocus&&hand->toolTip().contains("pan"),"Hand is a real visible named focusable tool");
        check(hand->mapTo(&w,QPoint{}).y()>gradient->mapTo(&w,QPoint{}).y(),"Hand appends after Gradient without moving existing slots");
        const auto position=hand->mapTo(&w,QPoint{});
        click(w,"tool-gradient");click(w,"tool-hand");exclusive(w);
        check(w.canvas->hand_mode()&&hand->isChecked()&&w.canvas->cursor().shape()==Qt::OpenHandCursor,
            "One Rail click activates persistent Hand and open-hand cursor");
        check(snapshot(s)==before&&w.canvas->selections()==selection,"Activation preserves existing authored/history state and selection");
        click(w,"tool-hand");check(w.canvas->hand_mode(),"Ordinary Hand reselect stays active");
        auto* structure=w.findChild<QDockWidget*>("structure");structure->hide();events();
        check(hand->isVisible()&&hand->mapTo(&w,QPoint{})==position,"Structure collapse preserves Hand spatial position");
        structure->show();events();w.canvas->fit_artboard();events();
        pan(w,Qt::LeftButton,QPoint(24,16));
        check(w.canvas->hand_mode()&&w.canvas->cursor().shape()==Qt::OpenHandCursor,"Release retains persistent Hand and idle cursor");
        QTest::mouseMove(w.canvas,QPoint(30,50));events();
        check(w.canvas->cursor().shape()==Qt::OpenHandCursor,"Idle hover does not replace Hand cursor with hit-test cursor");
        QTest::mouseDClick(w.canvas,Qt::LeftButton,Qt::NoModifier,QPoint(w.canvas->width()/2,w.canvas->height()/2));events();
        check(w.canvas->selections()==selection&&snapshot(s)==before,"Hand double-click cannot select or drill into artwork");
        w.canvas->set_selection("hand-circle");events();w.canvas->set_selection({});events();
        check(w.canvas->hand_mode()&&hand->isChecked(),"Selection changes and clearing never silently switch Hand");
        w.canvas->set_selection("hand-object");events();
        for(const auto* tool:{"tool-pen","tool-text","tool-anchor","tool-guide","tool-gradient"}){
            click(w,"tool-hand");click(w,tool);exclusive(w);
            check(!w.canvas->hand_mode()&&!hand->isChecked(),"Another Tool leaves Hand exclusively");
            click(w,"tool-hand");exclusive(w);
            check(snapshot(s)==before,"Both directions of Tool transitions preserve full authored/history state");
        }
        click(w,"tool-selection");check(!w.canvas->hand_mode()&&!hand->isChecked(),"Selection leaves Hand");
        click(w,"tool-hand");w.canvas->set_selection("hand-circle");w.canvas->set_circle_source_edit(true);events();
        check(!w.canvas->hand_mode()&&w.canvas->circle_source_edit(),"Existing Circle Inspector mode leaves Hand");
        click(w,"tool-hand");exclusive(w);w.canvas->set_selection("hand-object");events();
        QTest::keyClick(w.canvas,Qt::Key_Escape);events();
        check(!w.canvas->hand_mode()&&w.findChild<QToolButton*>("tool-selection")->isChecked()&&w.canvas->selected_object=="hand-object",
            "Idle Escape exits Hand to Selection without clearing selection");
        check(snapshot(s)==before,"Idle Escape is complete authored/history neutral");
        click(w,"tool-text");const auto text_name=w.findChild<QToolButton*>("tool-text")->accessibleName();
        w.canvas->fit_artboard();events();QTest::keyPress(w.canvas,Qt::Key_Space);events();
        pan(w,Qt::LeftButton,QPoint(24,16));QTest::keyRelease(w.canvas,Qt::Key_Space);events();
        check(w.canvas->text_mode()&&!w.canvas->hand_mode()&&w.findChild<QToolButton*>("tool-text")->isChecked()&&
            w.canvas->cursor().shape()==Qt::IBeamCursor,"Space temporary navigation returns to persistent Text Tool");
        pan(w,Qt::MiddleButton,QPoint(-24,-16));
        check(w.canvas->text_mode()&&snapshot(s)==before,"Middle-button temporary navigation preserves Text and full state");
        click(w,"tool-hand");QApplication::setActiveWindow(&w);w.canvas->setFocus();events();
        check(w.isActiveWindow()&&w.canvas->hasFocus(),"Shortcut fixture has a real active Window and Canvas keyboard focus");
        QTest::keyClick(w.canvas,Qt::Key_P);events();
        check(w.canvas->draw_mode()&&!w.canvas->hand_mode(),"Existing P shortcut leaves Hand for Pen");
        click(w,"tool-hand");QTest::keyClick(w.canvas,Qt::Key_G);events();
        check(w.canvas->draw_mode()&&!w.canvas->hand_mode(),"Existing G shortcut leaves Hand for Pen");
        click(w,"tool-hand");
        check(w.statusBar()->currentMessage().isEmpty(),"Hand activation removes the previous Pen operation hint");
        const QPoint start(w.canvas->width()/2,w.canvas->height()/2);
        QTest::mousePress(w.canvas,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(w.canvas,start+QPoint(8,8));events();
        QTest::keyClick(w.canvas,Qt::Key_Escape);events();
        check(w.canvas->hand_mode()&&!s.gesture_active()&&w.canvas->cursor().shape()==Qt::OpenHandCursor,
            "Escape cancels an active pan while retaining Hand");
        QTest::mouseRelease(w.canvas,Qt::LeftButton,Qt::NoModifier,start+QPoint(8,8));events();
        QFocusEvent lost(QEvent::FocusOut);QApplication::sendEvent(w.canvas,&lost);events();
        check(w.canvas->hand_mode()&&snapshot(s)==before,"Focus loss preserves persistent Hand and full authored state");
        check(w.findChild<QToolButton*>("tool-text")->accessibleName()==text_name&&
            preferences.value("workspace/tools/textCreationDirection")=="vertical"&&preferences.value("unrelated")=="preserve",
            "Navigation preserves last-used Text variant and unrelated workspace preferences");
        const auto authored=s.document();s.undo(s.revision());w.host.edited();events();
        check(s.document()==original&&w.canvas->hand_mode(),"Existing Undo still restores the complete pre-edit Document in Hand");
        s.redo(s.revision());w.host.edited();events();check(s.document()==authored,"Existing Redo restores the complete authored Document");
        const auto file=scratch.filePath("hand.nect.json");w.host.save(file);events();const auto saved=encode(s.document());
        w.host.open(file);events();const auto loaded=snapshot(s);
        check(encode(s.document())==saved&&w.canvas->hand_mode()&&hand->isChecked(),"Native reopen in same Window preserves source and viewport Tool");
        w.canvas->fit_artboard();events();pan(w,Qt::LeftButton,QPoint(24,16));
        check(snapshot(s)==loaded,"Reopened native Session pan remains fully authored/history neutral");
        Window reopened(scratch.filePath("cold"),std::make_unique<FolderLibrary>(preferences),&preferences);show(reopened);
        reopened.host.open(file);events();
        check(encode(reopened.host.session.document())==saved&&!reopened.canvas->hand_mode()&&
            reopened.findChild<QToolButton*>("tool-selection")->isChecked(),"New Window native reopen preserves source and starts in Selection");
        if(const auto capture=qEnvironmentVariable("NECT_HAND_CAPTURE");!capture.isEmpty())
            check(w.grab().save(capture),"Editable source rendered Window capture saved");
        std::cout<<"rail_hand_tool_tests: "<<checks<<" checks passed; DPR="<<w.devicePixelRatioF()<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
