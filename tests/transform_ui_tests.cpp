#include "window.hpp"
#include <QApplication>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <cmath>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void near(double v,double expected,const char* why){check(std::abs(v-expected)<1e-6,why);}
template<class T>T* widget(Window& w,const char* name){for(auto* p:w.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(name);}
void input(Window& w,const char* name,const char* text,bool commit=true){
    auto* field=widget<QLineEdit>(w,name);w.findChild<QScrollArea*>()->ensureWidgetVisible(field);QApplication::processEvents();
    field->setFocus();field->selectAll();QTest::keyClicks(field,text);if(commit)QTest::keyClick(field,Qt::Key_Return);QApplication::processEvents();
}
void window_controls(){
    QTemporaryDir temp;Window window(temp.path());window.show();QApplication::processEvents();auto& s=window.host.session;
    window.findChild<QAction*>("add-rectangle")->trigger();QApplication::processEvents();const auto first=window.canvas->selected_object;
    auto values=evaluate(s.document());const auto ax=values.at({first,"","transform.anchor_x"}),ay=values.at({first,"","transform.anchor_y"});
    near(ax,480,"GUI Add initializes centered Anchor");near(ay,320,"GUI Add initializes centered Anchor Y");
    const auto source=s.document().objects.at(first).source;const auto revision=s.revision();
    input(window,"transform-rotate-by","90");values=evaluate(s.document());
    near(values.at({first,"","transform.a"}),0,"Rotation edits the canonical affine matrix");near(values.at({first,"","transform.b"}),1,"Positive rotation is clockwise in Y-down coordinates");
    auto tf=evaluate_transforms(s.document(),values).at(first);auto position=map_point(tf.local,{ax,ay});
    near(position.x,480,"Rotation preserves anchor position");near(position.y,320,"Rotation preserves anchor Y");
    check(s.revision()==revision+1&&s.document().objects.at(first).source==source,"One Return is one edit and retains source");
    input(window,"transform-scale-x","2",false);input(window,"transform-scale-y","-1",false);
    widget<QPushButton>(window,"transform-scale-apply")->click();QApplication::processEvents();values=evaluate(s.document());
    near(values.at({first,"","transform.b"}),2,"Scale X multiplies local X axis");near(values.at({first,"","transform.c"}),1,"Negative scale mirrors local Y axis");
    input(window,"transform-position-x","+=25");values=evaluate(s.document());position=map_point(evaluate_transforms(s.document(),values).at(first).local,{ax,ay});
    near(position.x,505,"Position relative edit uses anchor position");near(position.y,320,"Position X preserves Y");
    check(widget<QLineEdit>(window,"transform-position-x")->hasFocus(),"Position remains focused after committing and rebuilding Inspector");
    widget<QPushButton>(window,"transform-matrix-toggle")->click();QApplication::processEvents();
    QLineEdit* matrix_x=nullptr;for(auto* line:window.findChildren<QLineEdit*>())
        if(line->isVisible()&&line->property("nect-reference").toByteArray().contains("transform.tx"))matrix_x=line;
    check(matrix_x,"Canonical matrix fields remain discoverable");
    window.findChild<QScrollArea*>()->ensureWidgetVisible(matrix_x);matrix_x->setFocus();matrix_x->selectAll();QTest::keyClicks(matrix_x,"+=10");QTest::keyClick(matrix_x,Qt::Key_Return);QApplication::processEvents();
    check(widget<QPushButton>(window,"transform-matrix-toggle")->isChecked(),"Expanded matrix stays open after a property edit");
    window.findChild<QAction*>("add-circle")->trigger();QApplication::processEvents();const auto parent=window.canvas->selected_object;
    window.canvas->set_selection(first);QApplication::processEvents();const auto old_world=window.canvas->evaluated_transforms().at(first).world;
    bool picked=false;QTimer::singleShot(0,[&]{for(auto* top:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QDialog*>(top)){
        if(dialog->objectName()!="transform-parent-dialog")continue;auto* list=dialog->findChild<QListWidget*>("transform-parent-list");
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString().toStdString()==parent){list->setCurrentRow(i);picked=true;break;}
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();return;
    }});
    window.findChild<QAction*>("choose-transform-parent")->trigger();QApplication::processEvents();
    check(picked&&s.document().objects.at(first).transform_parent==parent,"Parent picker commits explicit reference");
    check(window.canvas->evaluated_transforms().at(first).world==old_world,"Default parent picker preserves world placement");
    s.apply({Set{{parent,"","transform.tx"},40}},s.revision());window.host.edited();QApplication::processEvents();
    near(window.canvas->evaluated_transforms().at(first).world[4],old_world[4]+40,"GUI uses effective transform parent on refresh");
    window.host.save(temp.path()+"/transform.nect");const auto expected=encode(s.document());window.host.open(temp.path()+"/transform.nect");
    check(encode(window.host.session.document())==expected,"Native reopen retains anchors and external parent");
}
void canvas_coordinates(){
    Session s(empty_document("doc","comp","board"));auto source=default_primitive("parent-source","nect.shape.rectangle");
    source.parameters.at("center_x").literal=20;source.parameters.at("center_y").literal=20;source.parameters.at("width").literal=20;source.parameters.at("height").literal=20;
    auto child=default_primitive("child-source","nect.shape.rectangle");child.parameters.at("center_x").literal=100;child.parameters.at("center_y").literal=120;
    child.parameters.at("width").literal=100;child.parameters.at("height").literal=80;
    auto board=s.document().compositions.front().artboards.front();board.width=640;board.height=480;
    s.apply({UpdateArtboard{"comp",board},CreatePrimitive{"comp","","parent","Parent",source},CreatePrimitive{"comp","","child","Child",child},
        GroupContiguous{"comp","",{"child"},"group","Structure"},Set{{"group","","transform.tx"},600},
        Set{{"parent","","transform.a"},0},Set{{"parent","","transform.b"},1},Set{{"parent","","transform.c"},-1},Set{{"parent","","transform.d"},0},
        Set{{"parent","","transform.tx"},420},Set{{"parent","","transform.ty"},30},SetTransformParent{"child",Id("parent"),false},CenterAnchor{"child"}},s.revision());
    Canvas canvas(s);QString error;canvas.error=[&](QString e){error=e;};canvas.resize(740,580);canvas.show();QApplication::processEvents();canvas.fit_artboard();canvas.set_selection("child");
    auto screen=[&](double x,double y){return QPoint(qRound(canvas.width()/2.0+(x-320)*canvas.zoom()),qRound(canvas.height()/2.0+(y-240)*canvas.zoom()));};
    auto drag=[&](QPoint a,QPoint b){QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,a);QTest::mouseMove(&canvas,b,1);QApplication::processEvents();QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,b);QApplication::processEvents();};
    const auto before=encode(s.document());auto revision=s.revision();drag(screen(340,130),screen(370,150));
    check(error.isEmpty()&&s.revision()==revision+1,"Externally parented object can translate in one gesture");
    auto values=evaluate(s.document());near(values.at({"child","","transform.tx"}),20,"World drag inverse uses rotated effective parent X");
    near(values.at({"child","","transform.ty"}),-30,"World drag inverse uses rotated effective parent Y");
    s.undo(s.revision());canvas.refresh();check(encode(s.document())==before,"Undo restores complete pre-drag transform");
    canvas.set_anchor_edit(true);revision=s.revision();drag(screen(300,130),screen(330,150));values=evaluate(s.document());
    check(error.isEmpty()&&s.revision()==revision+1,"Anchor crosshair commits one gesture");
    near(values.at({"child","","transform.anchor_x"}),120,"Anchor drag maps world displacement into object coordinates");
    near(values.at({"child","","transform.anchor_y"}),90,"Anchor drag preserves correct rotated coordinate direction");
    near(values.at({"child","","transform.tx"}),0,"Moving Anchor preserves artwork matrix");
    near(values.at({"child","","transform.ty"}),0,"Moving Anchor preserves artwork matrix Y");
    s.undo(s.revision());canvas.refresh();check(encode(s.document())==before,"Anchor Undo preserves complete source and appearance");
    QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,screen(300,130));QTest::mouseMove(&canvas,screen(330,150),1);QApplication::processEvents();
    QTest::keyClick(&canvas,Qt::Key_Escape);check(encode(s.document())==before&&!s.gesture_active(),"Esc cancels Anchor preview without authoring");
    QTest::keyClick(&canvas,Qt::Key_Escape);check(!canvas.anchor_edit(),"Second Escape exits Anchor mode");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try{window_controls();canvas_coordinates();std::cout<<"PASS Anchor, affine controls, parent picker, transformed drag and native reopen\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
