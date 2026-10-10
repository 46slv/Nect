#include "window.hpp"
#include "visual_style.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <cmath>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);++checks;}
void events() {QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* named(QObject& scope,const char* name) {
    for(auto* p:scope.findChildren<T*>(name))if(p->isVisible())return p;
    throw std::runtime_error(std::string("Missing visible control: ")+name);
}
bool same(const Session& a,const Session& b) {
    return a.document()==b.document()&&a.preview_document()==b.preview_document()&&
        encode(a.document())==encode(b.document())&&a.history()==b.history()&&
        a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&
        a.gesture_active()==b.gesture_active()&&a.can_undo()==b.can_undo()&&a.can_redo()==b.can_redo();
}
Document fixture() {
    Session s(empty_document("repeater-context","comp","art"));
    auto source=default_primitive("tile-source","nect.shape.rectangle");
    source.parameters.at("center_x").literal=300;source.parameters.at("center_y").literal=180;
    source.parameters.at("width").literal=40;source.parameters.at("height").literal=24;
    auto other=default_primitive("other-source","nect.shape.rectangle");
    other.parameters.at("center_x").literal=520;other.parameters.at("center_y").literal=440;
    other.parameters.at("width").literal=30;other.parameters.at("height").literal=24;
    auto fill=default_operation("tile-fill","nect.paint.fill");fill.parameters.at("r").literal=1;
    auto repeater=default_operation("tile-repeat","nect.shape.repeater");
    repeater.parameters.at("copies").literal=2;repeater.parameters.at("position_x").literal=60;
    repeater.parameters.at("scale_x").literal=1.1;repeater.parameters.at("scale_y").literal=1.1;
    repeater.parameters.at("offset").literal=1;
    repeater.parameters.at("start_opacity").literal=0.8;repeater.parameters.at("end_opacity").literal=0.6;
    auto blue=default_operation("other-fill","nect.paint.fill");blue.parameters.at("b").literal=1;
    s.apply({CreatePrimitive{"comp","","tile","Retained Tile",source},AddOperation{"tile",fill,0},
        AddOperation{"tile",repeater,2},
        CreatePrimitive{"comp","","other","Other artwork",other},AddOperation{"other",blue,0},
        Set{{"tile","tile-source-top-left","x"},285},
        Link{{"other","","generator.width"},{{"tile","tile-source-top-left","x"},0.1,0,"copy_local_value"}}},0);
    auto d=s.document();auto& art=d.compositions.front().artboards.front();art.width=640;art.height=480;
    auto second=art;second.id="other-art";second.name="Other Artboard";second.x=700;
    d.compositions.front().artboards.push_back(second);return d;
}
void evidence(Window& w,const QString& suffix) {
    const auto prefix=qEnvironmentVariable("NECT_REPEATER_CONTEXT_EVIDENCE");
    if(!prefix.isEmpty())check(w.grab().save(prefix+suffix+".png"),"Window evidence saved");
}
void select(Window& w,const Id& id,const Id& point={}) {
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();
    auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();
    QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)
        if((*it)->data(0,Qt::UserRole).toString().toStdString()==id&&
            (*it)->data(0,Qt::UserRole+1).toString().toStdString()==point)row=*it;
    check(row,"Exact retained object row");tree->scrollToItem(row);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();
    check(w.canvas->selected_object==id,"Exact retained object selected");
    check(w.canvas->selected_point==point,"Exact stable generated point selection");
    dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();
}
void reveal(Window& w,QWidget* c) {
    auto* area=named<QScrollArea>(w,"inspector-scroll");check(c->isVisible()&&c->isEnabled(),"Actual enabled Repeater control");
    area->verticalScrollBar()->setValue(c->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Reach "<<c->objectName().toStdString()<<" viewport="<<area->viewport()->width()
        <<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()
        <<" hmax="<<area->horizontalScrollBar()->maximum()<<std::endl;
    const bool fits=area->horizontalScrollBar()->maximum()==0&&
        area->viewport()->rect().contains(QRect(c->mapTo(area->viewport(),QPoint()),c->size()))&&c->visibleRegion().contains(c->rect());
    if(!fits) {
        for(auto* x:area->widget()->findChildren<QWidget*>())
            if(x->minimumSizeHint().width()>area->viewport()->width()-60)
                std::cout<<"Minimum owner "<<x->metaObject()->className()<<" "<<x->objectName().toStdString()
                    <<" min="<<x->minimumSizeHint().width()<<std::endl;
        evidence(w,".failure");
    }
    check(fits,"Repeater control reachable with vertical scrolling only");
}
QLineEdit* field(Window& w,const Id& object,const char* name,const Id& point={}) {
    const auto key=QJsonDocument(QJsonObject{{"object",QString::fromStdString(object)},{"point",QString::fromStdString(point)},{"field",name}}).toJson(QJsonDocument::Compact);
    for(auto* p:w.findChildren<QLineEdit*>())if(p->isVisible()&&p->property("nect-reference").toByteArray()==key)return p;
    throw std::runtime_error("Missing visible Repeater field");
}
void history(Window& w,const QString& name) {
    for(auto* a:w.findChildren<QAction*>())if(a->text()==name){a->trigger();events();return;}
    throw std::runtime_error("Missing history action");
}
void canonical(Window& w,Session& expected) {
    check(same(w.host.session,expected),"Action equals complete independent Session/native/history/preview/generation");
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Undo full equality");
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Redo full equality");
}
void number(Window& w,Session& expected,const char* name,const char* text,double value,const char* operation="tile-repeat") {
    const std::string property=std::string("op.")+operation+"."+name;
    std::cout<<"Numeric "<<property<<std::endl;
    auto* c=field(w,"tile",property.c_str());reveal(w,c);QTest::mouseClick(c,Qt::LeftButton);
    QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,text);
    check(same(w.host.session,expected),"Numeric draft fully Session neutral");
    QTest::keyClick(c,Qt::Key_Return);events();
    expected.apply({EditProperties{{{"tile","",property}},value,false}},expected.revision());canonical(w,expected);
    check(evaluate(w.host.session.document()).at({"tile","",property})==value,"Property evaluated exact value");
}
void enabled(Window& w,Session& expected,bool value,const char* operation="tile-repeat") {
    const std::string control=std::string("operation-enabled-")+operation;
    auto* c=named<QCheckBox>(w,control.c_str());reveal(w,c);
    check(c->isChecked()!=value,"Actual Repeater transition");
    QTest::mouseClick(c,Qt::LeftButton);events();
    expected.apply({EnableOperation{"tile",operation,value}},expected.revision());canonical(w,expected);
    check(named<QCheckBox>(w,control.c_str())->isChecked()==value,"Operation enabled readback");
}
void angle(Window& w,Session& expected) {
    auto* knob=named<QWidget>(w,"repeater-angle-knob-tile-repeat");reveal(w,knob);
    const Ref ref{"tile","","op.tile-repeat.rotation"};const double initial=evaluate(expected.document()).at(ref);
    const QPoint top(knob->width()/2,knob->height()/2-15),right(knob->width()/2+15,knob->height()/2);
    QTest::mousePress(knob,Qt::LeftButton,Qt::NoModifier,top);expected.begin_gesture(expected.revision());
    check(same(w.host.session,expected),"Top-zero dial press equals independent gesture begin");
    QTest::mouseMove(knob,right);events();expected.update_gesture({EditProperties{{ref},initial+90,false}});
    check(same(w.host.session,expected),"Quarter-turn preview equals independent full Session");
    check(field(w,"tile","op.tile-repeat.rotation")->text().toDouble()==initial+90,"Numeric preview follows exact signed angle");
    QTest::mouseRelease(knob,Qt::LeftButton,Qt::NoModifier,right);events();expected.commit_gesture();canonical(w,expected);
}
void paint(Window& w,const Session& expected,const std::vector<QPoint>& centers,const QString& suffix,
        const std::vector<QPoint>& absent={}) {
    const auto image=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);
    check(image==Canvas::render_artboard(expected.document(),"comp","art",1,false),"Artwork equals independent canonical projection");
    for(const auto& p:centers)check(image.pixelColor(p)==QColor(Qt::red),"Independent repeated placement center");
    for(const auto& p:absent)check(image.pixelColor(p).alpha()==0,"Independent absent virtual copy");
    check(image.pixelColor(520,440)==QColor(Qt::blue)&&image.pixelColor(20,20).alpha()==0,"Unrelated artwork and transparency");
    events();const auto canvas=w.canvas->grab().toImage();
    const auto canvas_sample=[&](const QPoint& p) {
        const QPoint screen(qRound(w.canvas->width()/2.0+(p.x()-320)*w.canvas->zoom()),
            qRound(w.canvas->height()/2.0+(p.y()-240)*w.canvas->zoom()));
        return canvas.pixelColor(qRound(screen.x()*canvas.devicePixelRatio()),qRound(screen.y()*canvas.devicePixelRatio()));
    };
    check(canvas_sample({520,440})==QColor(Qt::blue),"Actual Canvas unrelated artwork color");
    for(const auto& p:centers) {
        check(canvas_sample(p)==QColor(Qt::red),
            "Actual Canvas repeated placement center");
    }
    const auto prefix=qEnvironmentVariable("NECT_REPEATER_CONTEXT_EVIDENCE");
    if(!prefix.isEmpty())check(image.save(prefix+suffix+".artwork.png"),"Artwork evidence saved");evidence(w,suffix);
}
void retained(const Document& initial,const Document& current) {
    const auto& a=initial.objects.at("tile");const auto& b=current.objects.at("tile");
    check(a.source==b.source&&a.point_edit==b.point_edit&&b.contours.empty(),"Full retained source/correction without flattened copies");
    check(current.objects.size()==initial.objects.size()&&current.compositions==initial.compositions,"Virtual copies preserve objects and ordered Artboards");
    check(current.objects.at("other")==initial.objects.at("other"),"Unrelated artwork and exact point Ref retained");
    check(b.stack.size()==3&&b.stack[0]==a.stack[0]&&b.stack[1]==a.stack[1]&&b.stack[2].id=="tile-repeat"&&
        b.stack[2].type=="nect.shape.repeater"&&b.stack[2].version==a.stack[2].version,"Exact ordered Fill/Stroke/Repeater IDs and paint retained");
}
void offset_context(const QString& scratch) {
    Session setup(fixture());
    setup.apply({Set{{"tile","","op.tile-repeat.scale_x"},2},Set{{"tile","","op.tile-repeat.scale_y"},2},
        Set{{"tile","","op.tile-repeat.anchor_x"},300},Set{{"tile","","op.tile-repeat.anchor_y"},180},
        Set{{"tile","","op.tile-repeat.position_x"},100},Set{{"tile","","op.tile-repeat.offset"},0},
        Set{{"tile","","op.tile-repeat.start_opacity"},1},Set{{"tile","","op.tile-repeat.end_opacity"},1},
        AddOperation{"tile",default_operation("tile-offset","nect.shape.offset"),3}},setup.revision());
    const auto initial=setup.document();
    Window w(scratch+"/offset-recovery");w.host.session=Session(initial);w.host.session_id="offset-context-session";
    w.host.edited();w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;
    select(w,"tile");check(same(w.host.session,expected),"Offset selection fully Session neutral");
    const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    number(w,expected,"amount","8",8,"tile-offset");number(w,expected,"miter_limit","6",6,"tile-offset");
    auto* join=named<QComboBox>(w,"operation-line-join-tile-offset");reveal(w,join);
    QTest::mouseClick(join,Qt::LeftButton);QTest::keyClick(join,Qt::Key_Down);QTest::keyClick(join,Qt::Key_Return);events();
    expected.apply({OperationOptions{"tile","tile-offset","below","nonzero","round"}},expected.revision());canonical(w,expected);
    paint(w,expected,{{300,180},{400,180},{326,180}},".offset-after",{{452,180}});
    auto* up=named<QPushButton>(w,"operation-up-tile-offset");reveal(w,up);QTest::mouseClick(up,Qt::LeftButton);events();
    expected.apply({ReorderOperations{"tile",{"tile-fill","tile-stroke","tile-offset","tile-repeat"}}},expected.revision());canonical(w,expected);
    paint(w,expected,{{300,180},{400,180},{452,180}},".offset-before");
    auto* down=named<QPushButton>(w,"operation-down-tile-offset");reveal(w,down);QTest::mouseClick(down,Qt::LeftButton);events();
    expected.apply({ReorderOperations{"tile",{"tile-fill","tile-stroke","tile-repeat","tile-offset"}}},expected.revision());canonical(w,expected);
    enabled(w,expected,false,"tile-offset");paint(w,expected,{{300,180},{400,180}},".offset-bypass",{{326,180},{452,180}});
    enabled(w,expected,true,"tile-offset");
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Offset keeps standard pane and Canvas width");
    const auto native=scratch+"/retained-offset.nect";w.host.save(native);check(same(w.host.session,expected),"Offset save fully Session neutral");
    w.host.changed={};w.hide();Window cold(scratch+"/offset-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();
    check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),"Cold complete Offset/Repeater/source/correction/Refs/Artboards");
    const Session reopened=cold.host.session;select(cold,"tile");
    for(const auto* property:{"op.tile-offset.amount","op.tile-offset.miter_limit"}) {
        auto* c=field(cold,"tile",property);reveal(cold,c);
        check(c->text().toDouble()==evaluate(expected.document()).at({"tile","",property}),"Cold exact Offset numeric readback");
    }
    join=named<QComboBox>(cold,"operation-line-join-tile-offset");reveal(cold,join);check(join->currentData().toString()=="round","Cold Offset join readback");
    check(named<QCheckBox>(cold,"operation-enabled-tile-offset")->isChecked(),"Cold Offset enabled readback");
    const auto& a=initial.objects.at("tile");const auto& b=cold.host.session.document().objects.at("tile");
    check(a.source==b.source&&a.point_edit==b.point_edit&&b.contours.empty(),"Offset/order edits retain full source/correction and virtual geometry");
    check(a.stack[0]==b.stack[0]&&a.stack[1]==b.stack[1]&&a.stack[2]==b.stack[2]&&b.stack[3].id=="tile-offset","Offset/order edits retain paint and complete Repeater");
    check(cold.host.session.document().objects.at("other")==initial.objects.at("other")&&cold.host.session.document().compositions==initial.compositions,"Offset preserves other artwork, point Ref and ordered Artboards");
    check(same(cold.host.session,reopened),"Cold Offset navigation fully Session neutral");
    paint(cold,reopened,{{300,180},{400,180},{326,180}},".offset-cold",{{452,180}});cold.host.changed={};cold.hide();
}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    QTemporaryDir scratch(qEnvironmentVariable("NECT_REPEATER_CONTEXT_SCRATCH",QDir::tempPath())+"/repeater-context-XXXXXX");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("RepeaterContext");
    try {
        check(scratch.isValid(),"Owned scratch");
        if(app.arguments().contains("--offset-context")) {
            offset_context(scratch.path());
            std::cout<<"PASS "<<checks<<" standard-pane retained Offset/Repeater stack contextual authoring; physical/subjective input NOT_RUN\n";return 0;
        }
        Window w(scratch.filePath("recovery"));const auto initial=fixture();
        w.host.session=Session(initial);w.host.session_id="repeater-context-session";w.host.edited();
        w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;
        select(w,"tile");check(same(w.host.session,expected),"Exact Structure selection fully Session neutral");
        const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
        number(w,expected,"copies","4",4);number(w,expected,"position_x","80",80);number(w,expected,"position_y","20",20);
        number(w,expected,"offset","0",0);number(w,expected,"scale_x","1",1);number(w,expected,"scale_y","1",1);
        number(w,expected,"start_opacity","1",1);number(w,expected,"end_opacity","1",1);
        paint(w,expected,{{300,180},{380,200},{460,220},{540,240}},".linear");
        number(w,expected,"position_x","0",0);number(w,expected,"position_y","0",0);
        number(w,expected,"anchor_x","200",200);number(w,expected,"anchor_y","180",180);
        number(w,expected,"rotation","-360",-360);angle(w,expected);
        check(evaluate(expected.document()).at({"tile","","op.tile-repeat.rotation"})==-270,"Signed unwrapped fixed per-copy step");
        const std::vector<QPoint> radial{{300,180},{200,280},{100,180},{200,80}};
        paint(w,expected,radial,".radial");
        enabled(w,expected,false);paint(w,expected,{{300,180}},".bypass",{{200,280},{100,180},{200,80}});
        enabled(w,expected,true);retained(initial,expected.document());paint(w,expected,radial,".enabled");
        check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Standard pane and Canvas widths retained");
        const auto native=scratch.filePath("retained-repeater.nect");w.host.save(native);
        check(same(w.host.session,expected),"Native save fully Session neutral");
        w.host.changed={};w.hide();Window cold(scratch.filePath("cold"));cold.host.open(native);
        cold.show();events();cold.canvas->fit_artboard();events();
        check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),
            "Fresh native complete retained source/correction/operations/Refs/Artboards");
        const Session reopened=cold.host.session;select(cold,"tile");
        for(const auto* name:{"copies","position_x","position_y","anchor_x","anchor_y","rotation","scale_x","scale_y","offset","start_opacity","end_opacity"}) {
            const std::string property=std::string("op.tile-repeat.")+name;
            auto* c=field(cold,"tile",property.c_str());reveal(cold,c);
            check(c->text().toDouble()==evaluate(expected.document()).at({"tile","",property}),"Fresh exact Repeater numeric readback");
        }
        reveal(cold,named<QWidget>(cold,"repeater-angle-knob-tile-repeat"));
        check(named<QCheckBox>(cold,"operation-enabled-tile-repeat")->isChecked(),"Fresh Repeater enabled readback");
        check(same(cold.host.session,reopened),"Cold selection and controls fully Session neutral");
        retained(initial,cold.host.session.document());paint(cold,reopened,radial,".cold");cold.host.changed={};cold.hide();
        std::cout<<"PASS "<<checks<<" standard-pane retained Repeater contextual authoring; physical/subjective input NOT_RUN\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
