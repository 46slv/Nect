#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QtMath>
#include <cmath>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
const Id contour_a="117a738e-06ad-4df0-b7ad-2c84c5080c2b",contour_b="db17d798-16e8-42b3-98f8-4344cf027a76";
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* named(QObject& scope,const char* name){auto* p=scope.findChild<T*>(name);check(p,name);return p;}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active()&&a.can_undo()==b.can_undo()&&a.can_redo()==b.can_redo();}
Contour line(const Id& id,double y){Contour c;c.id=id;for(auto xy:std::vector<Vec2>{{100,y},{400,y}}){Point p;p.id=id+std::to_string(c.points.size());p.x.literal=xy.x;p.y.literal=xy.y;c.points.push_back(p);}return c;}
Document fixture(){
    Session s(empty_document("follow-context","comp","art"));
    auto child=default_primitive("child-source","nect.shape.rectangle");child.parameters.at("center_x").literal=0;child.parameters.at("center_y").literal=0;child.parameters.at("width").literal=20;child.parameters.at("height").literal=16;
    auto second=child;second.id="second-source";second.parameters.at("center_y").literal=40;
    auto other=child;other.id="other-source";other.parameters.at("center_x").literal=520;other.parameters.at("center_y").literal=100;
    s.apply({CreatePrimitive{"comp","","child","Label background",child},CreatePrimitive{"comp","","second","Editable accent",second},GroupContiguous{"comp","",{"child","second"},"group","Followed heading"},
        CreatePath{"comp","","decoy","Baseline guide for retained heading and margin placement",{line("decoy-contour",300)}},CreatePath{"comp","","guide","Baseline guide for retained heading and margin placement",{line(contour_a,150),line(contour_b,250)}},CreatePrimitive{"comp","","other","Other artwork",other}},s.revision());
    auto d=s.document();d.compositions.front().artboards.front().width=640;d.compositions.front().artboards.front().height=480;
    auto art=d.compositions.front().artboards.front();art.id="other-art";art.name="Other Artboard";art.x=700;d.compositions.front().artboards.push_back(art);
    auto red=default_operation("child-fill","nect.paint.fill");red.parameters.at("r").literal=1;red.parameters.at("g").literal=0;red.parameters.at("b").literal=0;d.objects.at("child").stack.push_back(red);
    auto blue=red;blue.id="other-fill";blue.parameters.at("r").literal=0;blue.parameters.at("b").literal=1;d.objects.at("other").stack.push_back(blue);return d;
}
void evidence(Window& w,const QString& suffix){const auto path=qEnvironmentVariable("NECT_FOLLOW_CONTEXT_EVIDENCE");if(!path.isEmpty())check(w.grab().save(path+suffix),"Window evidence saved");}
void select(Window& w,const Id& target="group",const Id& point={}){
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()==QString::fromStdString(target)&&(*it)->data(0,Qt::UserRole+1).toString()==QString::fromStdString(point))row=*it;
    check(row,"Exact Structure row");tree->scrollToItem(row);events();QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();check(w.canvas->selected_object==target&&w.canvas->selected_point==point,"Exact object/source point selected");dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();
}
void reveal(Window& w,QWidget* c){
    auto* area=named<QScrollArea>(w,"inspector-scroll");check(c->isVisible()&&c->isEnabled(),"Actual enabled Path Follow control");area->verticalScrollBar()->setValue(c->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Reach "<<c->objectName().toStdString()<<" viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" width="<<c->width()<<std::endl;
    const bool fits=area->horizontalScrollBar()->maximum()==0&&area->viewport()->rect().contains(QRect(c->mapTo(area->viewport(),QPoint()),c->size()))&&c->visibleRegion().contains(c->rect());
    if(!fits||area->horizontalScrollBar()->maximum()>0){for(auto* x:area->widget()->findChildren<QWidget*>())if(x->minimumSizeHint().width()>area->viewport()->width()-60)std::cout<<"Minimum owner "<<x->metaObject()->className()<<" "<<x->objectName().toStdString()<<" min="<<x->minimumSizeHint().width()<<std::endl;evidence(w,".failure.png");}
    check(fits,"Path Follow control reachable with vertical scrolling only");
}
void choose(Window& w,const char* name,const QString& data,bool pointer){
    auto* c=named<QComboBox>(w,name);reveal(w,c);const auto idx=c->findData(data);check(idx>=0,"Exact stable choice exists");
    if(pointer){QTest::mouseClick(c,Qt::LeftButton);events();auto* view=c->view();const auto mi=c->model()->index(idx,0);view->scrollTo(mi);events();check(view->isVisible()&&view->viewport()->rect().contains(view->visualRect(mi).center()),"Popup exact row reachable");QTest::mouseMove(view->viewport(),view->visualRect(mi).center());QTest::qWait(QApplication::doubleClickInterval()+10);QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(mi).center());}
    else{c->setFocus();QTest::keyClick(c,Qt::Key_Home);for(int i=0;i<idx;++i)QTest::keyClick(c,Qt::Key_Down);}events();check(c->currentData().toString()==data,"Actual input chooses exact ID");
}
void number(Window& w,const char* name,const char* value){auto* c=named<QDoubleSpinBox>(w,name);reveal(w,c);QTest::mouseClick(c,Qt::LeftButton);QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,value);QTest::keyClick(c,Qt::Key_Return);events();}
void click(Window& w,const char* name){auto* c=named<QPushButton>(w,name);reveal(w,c);QTest::mouseClick(c,Qt::LeftButton);events();}
void history(Window& w,const QString& text){for(auto* a:w.findChildren<QAction*>())if(a->text()==text){a->trigger();events();return;}throw std::runtime_error("Missing history action");}
void canonical(Window& w,Session& expected){check(same(w.host.session,expected),"Action equals complete canonical Session/native/history/preview");history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Undo full equality");history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Redo full equality");}
void draft(Window& w,const Session& expected,bool pointer){
    choose(w,"group-path-follow-source","guide",pointer);check(same(w.host.session,expected),"Source draft is fully Session neutral");choose(w,"group-path-follow-contour",QString::fromStdString(contour_b),!pointer);check(same(w.host.session,expected),"Contour draft is fully Session neutral");
    auto* contour=named<QComboBox>(w,"group-path-follow-contour");check(contour->currentText()==QString::fromStdString(contour_b)&&contour->toolTip()==contour->currentText()&&contour->currentData(Qt::ToolTipRole).toString()==contour->currentText(),"Bounded contour retains complete stable label/item ID and tooltip");
    choose(w,"group-path-follow-start-mode","normalized",false);number(w,"group-path-follow-start","0.2");check(same(w.host.session,expected),"Normalized mode/value draft is fully Session neutral");
    choose(w,"group-path-follow-start-mode","distance",false);number(w,"group-path-follow-start","40");number(w,"group-path-follow-normal-offset","10");check(same(w.host.session,expected),"Finite draft/Enter leaves full Session/native/history untouched");
    check(named<QComboBox>(w,"group-path-follow-source")->currentText().contains("guide"),"Same-name source remains ID disambiguated");
}
void attach(Window& w,Session& expected,bool pointer){draft(w,expected,pointer);click(w,"group-path-follow-apply");check(w.host.session.document().objects.at("group").path_follow.has_value(),"Actual Attach creates relation");
    // Only the generated identity is observed; every other expected value is independent.
    GroupPathFollow r;r.id=w.host.session.document().objects.at("group").path_follow->id;r.path="guide";r.contour=contour_b;r.start=40;r.normal_offset=10;expected.apply({GroupPathFollowCommand{AttachGroupPathFollow{"group",r}}},expected.revision());canonical(w,expected);
}
void child(Window& w,Session& expected){auto* c=named<QCheckBox>(w,"group-path-follow-item-child");reveal(w,c);QTest::mouseClick(c,Qt::LeftButton,Qt::NoModifier,QPoint(8,c->height()/2));events();expected.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","child",GroupPathFollowItem{}}}},expected.revision());canonical(w,expected);
    const auto image=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);check(image==Canvas::render_artboard(expected.document(),"comp","art",1,false),"Actual painted placement equals canonical renderer");
    check(image.pixelColor(140,260)==QColor(Qt::red)&&image.pixelColor(0,0).alpha()==0&&image.pixelColor(520,100)==QColor(Qt::blue),"Followed child paints at independent straight-contour start/offset; other artwork retained");
    const auto path=qEnvironmentVariable("NECT_FOLLOW_CONTEXT_EVIDENCE");if(!path.isEmpty())check(image.save(path+".artwork.png"),"Derived painted evidence saved");
    number(w,"group-path-follow-distance-child","12");expected.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","child",{12,0,true}}}},expected.revision());canonical(w,expected);
    number(w,"group-path-follow-item-offset-child","3");expected.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","child",{12,3,true}}}},expected.revision());canonical(w,expected);
    auto* tangent=named<QCheckBox>(w,"group-path-follow-tangent-child");reveal(w,tangent);QTest::mouseClick(tangent,Qt::LeftButton,Qt::NoModifier,QPoint(8,tangent->height()/2));events();expected.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","child",{12,3,false}}}},expected.revision());canonical(w,expected);
    const auto moved=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);check(moved.pixelColor(152,263)==QColor(Qt::red)&&moved.pixelColor(140,260).alpha()==0,"Child numeric edits produce independent painted distance/offset");
    number(w,"group-path-follow-distance-child","0");expected.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","child",{0,3,false}}}},expected.revision());canonical(w,expected);
    number(w,"group-path-follow-item-offset-child","0");expected.apply({GroupPathFollowCommand{SetGroupPathFollowItem{"group","child",{0,0,false}}}},expected.revision());canonical(w,expected);
}
void update_clear(Window& w,Session& expected){
    number(w,"group-path-follow-start","60");number(w,"group-path-follow-normal-offset","5");check(same(w.host.session,expected),"Update draft/Enter is Session neutral");click(w,"group-path-follow-apply");auto r=*expected.document().objects.at("group").path_follow;r.start=60;r.normal_offset=5;expected.apply({GroupPathFollowCommand{UpdateGroupPathFollow{"group",r}}},expected.revision());canonical(w,expected);
    const auto image=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);check(image.pixelColor(160,255)==QColor(Qt::red)&&image.pixelColor(140,260).alpha()==0&&image.pixelColor(520,100)==QColor(Qt::blue),"Update moves evaluated painted child and retains other artwork");evidence(w,".updated.png");
    click(w,"group-path-follow-clear");expected.apply({GroupPathFollowCommand{ClearGroupPathFollow{"group"}}},expected.revision());canonical(w,expected);history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Clear Undo restores exact relation/children/source/history");
}
Document deform_context_fixture() {
    auto d=fixture();
    d.objects.at("child").source->parameters.at("width").literal=40;
    const Affine group{0,1,-1,0,500,0},child_matrix{1,0,0,1,16,16};
    for(std::size_t i=0;i<6;++i) {
        d.objects.at("group").transform[i].literal=group[i];
        d.objects.at("child").transform[i].literal=child_matrix[i];
    }
    // Authored L contour: a 100-unit eastward leg followed by 200 units south.
    auto& guide=d.objects.at("guide");guide.visible=false;
    auto& bend=guide.contours.front();bend.points.back().x.literal=200;
    Point end=bend.points.back();end.id="bend-end";end.y.literal=350;bend.points.push_back(end);
    auto green=default_operation("second-fill","nect.paint.fill");
    green.parameters.at("r").literal=0;green.parameters.at("g").literal=1;green.parameters.at("b").literal=0;
    d.objects.at("second").stack.push_back(green);
    Session s(d);
    s.apply({Set{{"child","child-source-top-left","x"},-19.25},
        Link{{"other","","generator.width"},{{"child","child-source-top-left","x"},-1,0,"copy_local_value"}}},s.revision());
    GroupPathFollow r;r.id="retained-bend-relation";r.path="guide";r.contour=contour_a;
    r.start=80.12567891234567;r.normal_offset=-32.37567891234567;
    r.items={{"child",{4,-3.125,true}},{"second",{100,-30,false}}};
    s.apply({GroupPathFollowCommand{AttachGroupPathFollow{"group",r}}},s.revision());
    return s.document();
}
void retained_deform(const Document& initial,const Document& actual) {
    auto expected=initial;expected.objects.at("group").path_follow=actual.objects.at("group").path_follow;
    check(actual==expected&&encode(actual)==encode(expected),"Only retained relation changes; full child sources/Point Edit/Ref/transforms/paint/order/Artboards preserved");
    const auto& a=*initial.objects.at("group").path_follow;const auto& b=*actual.objects.at("group").path_follow;
    check(a.id==b.id&&a.path==b.path&&a.contour==b.contour&&a.start_mode==b.start_mode&&a.start==b.start&&
        a.normal_offset==b.normal_offset&&a.reversed==b.reversed&&a.items==b.items,
        "Mode/axis preserve exact relation identity, precise start/offset and complete stable item map");
}
struct DeformOraclePoint {Id id;Vec2 source,world,local;};
std::vector<DeformOraclePoint> deform_oracle(const GroupPathFollow& r,const Id& id,double width=40,double top_left_x=-19.25) {
    // No actual evaluation output enters this oracle. Rectangle anchors, local
    // translation, L-contour distance/normal and inverse Group matrix are explicit.
    const bool child=id=="child",y=r.deform_axis=="y";
    const std::vector<Vec2> anchors=child?std::vector<Vec2>{{top_left_x,-8},{width/2,-8},{width/2,8},{-width/2,8}}:
        std::vector<Vec2>{{-10,32},{10,32},{10,48},{-10,48}};
    const std::vector<std::string> roles={"top-left","top-right","bottom-right","bottom-left"};
    std::vector<DeformOraclePoint> result;
    for(std::size_t i=0;i<anchors.size();++i) {
        auto p=anchors[i];if(child){p.x+=16;p.y+=16;}
        const auto& item=r.items.at(id);
        const double distance=(r.start_mode=="normalized"?r.start*300:r.start)+item.distance+(y?p.y:p.x);
        const double normal=r.normal_offset+item.normal_offset+(y?p.x:p.y);
        const Vec2 world=r.reversed
            ?(distance<200?Vec2{200+normal,350-distance}:Vec2{400-distance,150-normal})
            :(distance<100?Vec2{100+distance,150+normal}:Vec2{200-normal,150+distance-100});
        result.push_back({(child?"child-source-":"second-source-")+roles[i],anchors[i],world,{world.y,500-world.x}});
    }
    return result;
}
void deform_pixels(Window& w,const Session& independent,const Document& initial,const QString& suffix) {
    check(same(w.host.session,independent),"Projection observation remains full Session neutral");
    retained_deform(initial,w.host.session.document());
    const auto& d=independent.document();const auto& r=*d.objects.at("group").path_follow;
    const auto values=evaluate(w.host.session.document());
    const auto scene=evaluate_scene(w.host.session.document(),"comp",values,evaluate_transforms(w.host.session.document(),values));
    const auto image=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);
    check(image==Canvas::render_artboard(d,"comp","art",1,false),"Full artwork equals independent canonical Session");
    const auto canvas=w.canvas->grab().toImage();
    for(const Id id:{"child","second"}) {
        const auto predicted=deform_oracle(r,id);const auto& actual=scene.deformation_points.at(id);
        check(actual.size()==predicted.size()&&scene.deformation_owners.at(id)=="group","Projected source point topology and Group owner retained");
        QPainterPath polygon;polygon.moveTo(predicted.front().world.x,predicted.front().world.y);
        double cx=0,cy=0;
        for(std::size_t i=0;i<predicted.size();++i) {
            const auto& p=predicted[i];
            const auto near=[&](Vec2 a,Vec2 b){return std::hypot(a.x-b.x,a.y-b.y)<1e-7;};
            check(actual[i].id==p.id&&near(actual[i].anchor,p.local)&&near(actual[i].incoming,p.local)&&near(actual[i].outgoing,p.local),
                "Analytical L-contour axis/child affine/inverse Group oracle matches stable anchors and controls");
            const auto world=map_point(scene.geometry_worlds.at(id),actual[i].anchor);
            check(near(world,p.world),"Group world consumed exactly once");
            polygon.lineTo(p.world.x,p.world.y);cx+=p.world.x/4;cy+=p.world.y/4;
        }
        polygon.closeSubpath();
        // The three probes are chosen from our independently predicted polygon,
        // far from its boundary and source selection overlays.
        for(const QPointF probe:{QPointF(cx,cy),QPointF(cx+2,cy),QPointF(cx,cy+2)}) {
            check(polygon.contains(probe),"Analytical polygon contains independent interior pixel");
            const QColor color=id=="child"?QColor(Qt::red):QColor(Qt::green);
            check(image.pixelColor(qFloor(probe.x()),qFloor(probe.y()))==color,"Analytical deformed child interior color");
            const double z=w.canvas->zoom();
            const QPoint screen(qRound(w.canvas->width()/2.0+(probe.x()-320)*z),qRound(w.canvas->height()/2.0+(probe.y()-240)*z));
            const auto visible=canvas.pixelColor(qRound(screen.x()*canvas.devicePixelRatio()),qRound(screen.y()*canvas.devicePixelRatio()));
            check(visible==color,"Actual Canvas independently predicted deformed child interior color");
        }
    }
    check(image.pixelColor(520,100)==QColor(Qt::blue)&&image.pixelColor(20,20).alpha()==0,"Linked other artwork and transparent background retained");
    const auto prefix=qEnvironmentVariable("NECT_FOLLOW_CONTEXT_EVIDENCE");
    if(!prefix.isEmpty())check(image.save(prefix+suffix+".artwork.png"),"Deform artwork evidence saved");
    evidence(w,suffix+".png");check(same(w.host.session,independent),"Geometry/pixels observations preserve full Session");
}
void deform_context(const QString& scratch) {
    const auto initial=deform_context_fixture();Window w(scratch+"/deform-recovery");
    w.host.session=Session(initial);w.host.session_id="deform-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
    Session expected=w.host.session;select(w);check(same(w.host.session,expected),"Retained Group selection is Session neutral");
    const int canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    check(!named<QComboBox>(w,"group-path-deform-axis")->isEnabled(),"Rigid relation disables axis editing");
    choose(w,"group-path-follow-mode","deform",false);check(same(w.host.session,expected),"Deform mode draft is Session neutral");
    choose(w,"group-path-deform-axis","y",false);check(same(w.host.session,expected),"Y-axis draft is Session neutral");
    select(w,"other");select(w);check(same(w.host.session,expected),"Reselection discards uncommitted mode/axis without authored or History change");
    check(named<QComboBox>(w,"group-path-follow-mode")->currentData().toString()=="rigid"&&
        named<QComboBox>(w,"group-path-deform-axis")->currentData().toString()=="x"&&
        !named<QComboBox>(w,"group-path-deform-axis")->isEnabled(),"Discard restores retained mode, axis and eligibility");
    auto apply_axis=[&](Window& window,Session& oracle,const char* axis) {
        choose(window,"group-path-follow-mode","deform",false);
        choose(window,"group-path-deform-axis",axis,false);
        check(same(window.host.session,oracle),"Actual mode/axis input is fully uncommitted before Apply");
        auto r=*oracle.document().objects.at("group").path_follow;r.mode="deform";r.deform_axis=axis;
        click(window,"group-path-follow-apply");
        oracle.apply({GroupPathFollowCommand{UpdateGroupPathFollow{"group",r}}},oracle.revision());canonical(window,oracle);
        check(named<QComboBox>(window,"group-path-follow-mode")->currentData().toString()=="deform"&&
            named<QComboBox>(window,"group-path-deform-axis")->currentData().toString()==axis&&
            !named<QCheckBox>(window,"group-path-follow-tangent-second")->isEnabled(),"Applied deform axis/disabled tangent reads retained relation");
    };
    apply_axis(w,expected,"x");deform_pixels(w,expected,initial,".deform-x");
    apply_axis(w,expected,"y");deform_pixels(w,expected,initial,".deform-y");
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Axis Undo restores full source, relation and History");
    deform_pixels(w,expected,initial,".deform-x-undo");
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Axis Redo full equality");
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Deform workflow preserves standard dock and Canvas widths");
    const auto file=scratch+"/retained-deform.nect";w.host.save(file);check(same(w.host.session,expected),"Native save is fully Session neutral");
    w.host.changed={};w.hide();Window cold(scratch+"/deform-cold");cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();
    check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),"Cold complete retained source/Point Edit/Ref/paint/transforms/relation/items/Artboard equality");
    Session reopened=cold.host.session;select(cold);
    check(same(cold.host.session,reopened)&&named<QComboBox>(cold,"group-path-follow-mode")->currentData().toString()=="deform"&&
        named<QComboBox>(cold,"group-path-deform-axis")->currentData().toString()=="y"&&named<QComboBox>(cold,"group-path-deform-axis")->isEnabled(),
        "Cold contextual mode/axis/eligibility and navigation neutral");
    deform_pixels(cold,reopened,initial,".deform-cold-y");
    apply_axis(cold,reopened,"x");deform_pixels(cold,reopened,initial,".deform-cold-x");
    cold.host.changed={};cold.hide();
}
void deform_traversal_context(const QString& scratch) {
    auto initial=deform_context_fixture();
    initial.objects.at("group").path_follow->mode="deform";
    initial.objects.at("group").path_follow->deform_axis="y";
    Window w(scratch+"/traversal-recovery");w.host.session=Session(initial);
    w.host.session_id="deform-traversal-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
    Session expected=w.host.session;select(w);
    const int canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    check(same(w.host.session,expected),"Traversal selection fully Session neutral");
    choose(w,"group-path-follow-start-mode","normalized",false);
    number(w,"group-path-follow-start","0.3");
    check(same(w.host.session,expected),"Normalized start draft/Enter preserves full retained Session/History");
    auto relation=*expected.document().objects.at("group").path_follow;
    relation.start_mode="normalized";relation.start=0.3;
    click(w,"group-path-follow-apply");
    expected.apply({GroupPathFollowCommand{UpdateGroupPathFollow{"group",relation}}},expected.revision());canonical(w,expected);
    auto protected_source=initial;protected_source.objects.at("group").path_follow=relation;
    deform_pixels(w,expected,protected_source,".normalized-y");
    auto* reverse=named<QCheckBox>(w,"group-path-follow-reversed");reveal(w,reverse);
    QTest::mouseClick(reverse,Qt::LeftButton,Qt::NoModifier,QPoint(8,reverse->height()/2));events();
    check(reverse->isChecked()&&same(w.host.session,expected),"Reverse traversal draft changes only local controls");
    relation.reversed=true;click(w,"group-path-follow-apply");
    expected.apply({GroupPathFollowCommand{UpdateGroupPathFollow{"group",relation}}},expected.revision());canonical(w,expected);
    protected_source.objects.at("group").path_follow=relation;
    deform_pixels(w,expected,protected_source,".normalized-reversed-y");
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Traversal preserves standard pane/Canvas widths");
    const auto file=scratch+"/retained-deform-traversal.nect";w.host.save(file);
    check(same(w.host.session,expected),"Traversal native save fully Session neutral");
    w.host.changed={};w.hide();
    Window cold(scratch+"/traversal-cold");cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();select(cold);
    check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),"Cold normalized/reversed source, relation, items, Ref, Point Edit and Artboards equality");
    const Session reopened=cold.host.session;
    check(named<QComboBox>(cold,"group-path-follow-start-mode")->currentData().toString()=="normalized"&&
        named<QDoubleSpinBox>(cold,"group-path-follow-start")->value()==0.3&&
        named<QCheckBox>(cold,"group-path-follow-reversed")->isChecked()&&
        named<QComboBox>(cold,"group-path-deform-axis")->currentData().toString()=="y",
        "Cold retained normalized start, reverse and axis controls read back");
    deform_pixels(cold,reopened,protected_source,".traversal-cold");
    cold.host.changed={};cold.hide();
}
QLineEdit* source_field(Window& w,const Ref& ref) {
    const auto key=QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
        {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);
    for(auto* input:w.findChildren<QLineEdit*>())
        if(input->isVisible()&&input->property("nect-reference").toByteArray()==key)return input;
    throw std::runtime_error("Missing visible retained source field");
}
void source_pixels(Window& w,const Session& independent,const Document& protected_source,
                   double width,double top_left_x,const QString& suffix) {
    check(same(w.host.session,independent),"Source task matches full independent Session/native/history/preview/generation");
    auto expected=protected_source;
    expected.objects.at("child").source->parameters.at("width").literal=width;
    expected.objects.at("child").point_edit->overrides.at("child-source-top-left").at("x").literal=top_left_x;
    check(w.host.session.document()==expected&&encode(w.host.session.document())==encode(expected),
        "Only intended authored width/absolute x changes; complete relation/items/source identities/Refs/transforms/paint/order/Artboards retained");
    const auto values=evaluate(w.host.session.document());
    check(values.at({"child","","generator.width"})==width&&
        values.at({"child","child-source-top-left","x"})==top_left_x&&
        values.at({"other","","generator.width"})==-top_left_x,
        "Exact source values and linked other width reevaluate independently of projection/display rounding");
    const auto scene=evaluate_scene(w.host.session.document(),"comp",values,evaluate_transforms(w.host.session.document(),values));
    const auto image=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);
    check(image==Canvas::render_artboard(independent.document(),"comp","art",1,false),"Complete source-edit artwork equals independent canonical renderer");
    const auto canvas=w.canvas->grab().toImage();
    const auto sample=[&](QPointF probe) {
        const QPoint pixel(qRound(w.canvas->width()/2.0+(probe.x()-320)*w.canvas->zoom()),
                           qRound(w.canvas->height()/2.0+(probe.y()-240)*w.canvas->zoom()));
        return canvas.pixelColor(qRound(pixel.x()*canvas.devicePixelRatio()),qRound(pixel.y()*canvas.devicePixelRatio()));
    };
    for(const Id id:{"child","second"}) {
        const auto predicted=deform_oracle(*protected_source.objects.at("group").path_follow,id,width,top_left_x);
        const auto& actual=scene.deformation_points.at(id);
        check(actual.size()==predicted.size()&&scene.deformation_owners.at(id)=="group","Retained source point topology/Group owner survive source edit");
        QPainterPath polygon;polygon.moveTo(predicted.front().world.x,predicted.front().world.y);
        QPointF center;
        for(std::size_t i=0;i<predicted.size();++i) {
            const auto& p=predicted[i];
            const auto near=[](Vec2 a,Vec2 b){return std::hypot(a.x-b.x,a.y-b.y)<1e-7;};
            check(values.at({id,p.id,"x"})==p.source.x&&values.at({id,p.id,"y"})==p.source.y,
                "Authored source anchors equal independently supplied source values");
            check(actual[i].id==p.id&&near(actual[i].anchor,p.local)&&near(actual[i].incoming,p.local)&&near(actual[i].outgoing,p.local),
                "Source edit reevaluates analytical L-contour/leaf affine/inverse Group anchor/control projection");
            check(near(map_point(scene.geometry_worlds.at(id),actual[i].anchor),p.world),"Source edit consumes final Group world once");
            polygon.lineTo(p.world.x,p.world.y);center+=QPointF(p.world.x/4,p.world.y/4);
        }
        polygon.closeSubpath();
        for(const QPointF probe:{center,center+QPointF(2,0),center+QPointF(0,2)}) {
            check(polygon.contains(probe),"Independent source-edit polygon contains interior probe");
            const QColor color=id=="child"?QColor(Qt::red):QColor(Qt::green);
            check(image.pixelColor(qFloor(probe.x()),qFloor(probe.y()))==color,"Independently predicted source-edit interior pixel");
            check(sample(probe)==color,"Actual Canvas displays independently predicted source-edit pixel");
        }
    }
    // The linked rectangle is centered at 520 with exact width -top_left_x.
    const double edge=520-top_left_x/2;
    check(image.pixelColor(520,100)==QColor(Qt::blue)&&
        image.pixelColor(qFloor(edge-3),100)==QColor(Qt::blue)&&
        image.pixelColor(qCeil(edge+3),100).alpha()==0&&image.pixelColor(20,20).alpha()==0,
        "Linked exact Ref controls other source width and independently predicted edge pixels");
    check(sample({520,100})==QColor(Qt::blue),"Actual Canvas retains linked unrelated artwork");
    const auto prefix=qEnvironmentVariable("NECT_FOLLOW_CONTEXT_EVIDENCE");
    if(!prefix.isEmpty())check(image.save(prefix+suffix+".artwork.png"),"Source-edit artwork evidence saved");
    evidence(w,suffix+".png");check(same(w.host.session,independent),"Source geometry/pixel observations fully Session neutral");
}
void edit_source(Window& w,Session& independent,const Ref& ref,const char* text,double value) {
    auto* input=source_field(w,ref);reveal(w,input);evidence(w,".source-before.png");
    QTest::mouseClick(input,Qt::LeftButton);QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);
    check(same(w.host.session,independent),"Actual retained source scalar draft is full Session neutral");
    QTest::keyClick(input,Qt::Key_Return);events();
    independent.apply({EditProperties{{ref},value,false}},independent.revision());
    check(same(w.host.session,independent),"Actual Enter commits one independent canonical authored-source command");
}
void deform_source_context(const QString& scratch) {
    auto initial=deform_context_fixture();initial.objects.at("group").path_follow->mode="deform";
    initial.objects.at("group").path_follow->deform_axis="x";
    Window w(scratch+"/source-recovery");w.host.session=Session(initial);w.host.session_id="deform-source-context-session";
    w.host.edited();w.show();events();w.canvas->fit_artboard();events();Session independent(initial);
    select(w,"child");check(same(w.host.session,independent),"Expanded Structure exact deformed child selection is Session neutral");
    const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    constexpr double width=64.25,correction=-27.12567891234567;
    edit_source(w,independent,{"child","","generator.width"},"64.25",width);
    source_pixels(w,independent,initial,width,-19.25,".source-width");
    history(w,"Undo");independent.undo(independent.revision());source_pixels(w,independent,initial,40,-19.25,".source-width-undo");
    history(w,"Redo");independent.redo(independent.revision());source_pixels(w,independent,initial,width,-19.25,".source-width-redo");
    select(w,"child","child-source-top-left");check(same(w.host.session,independent),"Exact stable source point selection is full Session neutral");
    edit_source(w,independent,{"child","child-source-top-left","x"},"-27.12567891234567",correction);
    source_pixels(w,independent,initial,width,correction,".source-point");
    history(w,"Undo");independent.undo(independent.revision());source_pixels(w,independent,initial,width,-19.25,".source-point-undo");
    history(w,"Redo");independent.redo(independent.revision());source_pixels(w,independent,initial,width,correction,".source-point-redo");
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Source task preserves standard Canvas/Properties widths");
    const auto file=scratch+"/retained-deform-source.nect";w.host.save(file);
    check(same(w.host.session,independent),"Native source save preserves complete Session/history");w.host.changed={};w.hide();
    Window cold(scratch+"/source-cold");cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();
    Session reopened(independent.document());check(same(cold.host.session,reopened),"Fresh native Window restores complete independent authored Document with fresh Session");
    select(cold,"child","child-source-top-left");auto* x=source_field(cold,{"child","child-source-top-left","x"});reveal(cold,x);
    check(std::abs(x->text().toDouble()-correction)<0.0001&&x->isEnabled()&&
        named<QCheckBox>(cold,"point-edit-enabled")->isChecked(),"Cold Point Edit field reads retained absolute source x and eligibility");
    source_pixels(cold,reopened,initial,width,correction,".source-cold-point");
    select(cold,"child");auto* generator=source_field(cold,{"child","","generator.width"});reveal(cold,generator);
    check(generator->text().toDouble()==width&&generator->isEnabled(),"Cold generator field reads retained width and eligibility");
    source_pixels(cold,reopened,initial,width,correction,".source-cold-width");
    cold.host.changed={};cold.hide();
}
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());QTemporaryDir scratch;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());app.setOrganizationName("NectTest");app.setApplicationName("FollowContext");
    if(argc>1&&std::string(argv[1])=="--deform-source-context")try{deform_source_context(qEnvironmentVariable("NECT_FOLLOW_CONTEXT_SCRATCH",scratch.path()));std::cout<<"PASS standard-pane retained Deform child/source point edits, exact linked Ref, analytical geometry/pixels and native/Undo; physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    if(argc>1&&std::string(argv[1])=="--deform-traversal-context")try{deform_traversal_context(qEnvironmentVariable("NECT_FOLLOW_CONTEXT_SCRATCH",scratch.path()));std::cout<<"PASS standard-pane retained Group Deform normalized/reversed traversal, analytical geometry/pixels and native/Undo; physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    if(argc>1&&std::string(argv[1])=="--deform-context")try{deform_context(qEnvironmentVariable("NECT_FOLLOW_CONTEXT_SCRATCH",scratch.path()));std::cout<<"PASS standard-pane retained Group Deform mode/axis, analytical geometry/pixels and native/Undo; physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    try{Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="follow-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;select(w);check(same(w.host.session,expected),"Selection Session neutral");const auto canvas=w.canvas->width(),dock=named<QDockWidget>(w,"properties")->width();attach(w,expected,true);child(w,expected);update_clear(w,expected);check(w.canvas->width()==canvas&&named<QDockWidget>(w,"properties")->width()==dock,"Standard dock/Canvas widths preserved");
        const auto saved=expected.document();const auto file=scratch.filePath("follow.nect");w.host.save(file);check(same(w.host.session,expected),"Native save preserves complete Session");w.host.changed={};w.hide();Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();select(cold);check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Fresh native Window retains full authored state/IDs/stacks/Artboards");
        auto* cold_start=named<QDoubleSpinBox>(cold,"group-path-follow-start");reveal(cold,cold_start);check(cold_start->value()==60&&named<QDoubleSpinBox>(cold,"group-path-follow-normal-offset")->value()==5&&named<QComboBox>(cold,"group-path-follow-contour")->currentData().toString()==QString::fromStdString(contour_b),"Fresh Window controls read retained source/contour/start/offset");const auto cold_pixels=Canvas::render_artboard(cold.host.session.document(),"comp","art",1,false);check(cold_pixels.pixelColor(160,255)==QColor(Qt::red)&&cold_pixels.pixelColor(520,100)==QColor(Qt::blue),"Fresh native Window paints retained derived placement and unrelated artwork");evidence(cold,".cold-native.png");
        Session reopened=cold.host.session;update_clear(cold,reopened);click(cold,"group-path-follow-clear");reopened.apply({GroupPathFollowCommand{ClearGroupPathFollow{"group"}}},reopened.revision());canonical(cold,reopened);attach(cold,reopened,false);evidence(cold,".cold.png");cold.host.changed={};cold.hide();std::cout<<"PASS standard-pane Path Follow contextual authoring; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
