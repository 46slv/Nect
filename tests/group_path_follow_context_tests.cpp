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
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
const Id contour_a="117a738e-06ad-4df0-b7ad-2c84c5080c2b",contour_b="db17d798-16e8-42b3-98f8-4344cf027a76";
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* named(QObject& scope,const char* name){auto* p=scope.findChild<T*>(name);check(p,name);return p;}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
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
void select(Window& w){
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="group"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())row=*it;
    check(row,"Exact Group row");tree->scrollToItem(row);events();QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();check(w.canvas->selected_object=="group","Exact Group selected");dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();
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
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());QTemporaryDir scratch;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());app.setOrganizationName("NectTest");app.setApplicationName("FollowContext");
    try{Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="follow-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;select(w);check(same(w.host.session,expected),"Selection Session neutral");const auto canvas=w.canvas->width(),dock=named<QDockWidget>(w,"properties")->width();attach(w,expected,true);child(w,expected);update_clear(w,expected);check(w.canvas->width()==canvas&&named<QDockWidget>(w,"properties")->width()==dock,"Standard dock/Canvas widths preserved");
        const auto saved=expected.document();const auto file=scratch.filePath("follow.nect");w.host.save(file);check(same(w.host.session,expected),"Native save preserves complete Session");w.host.changed={};w.hide();Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();select(cold);check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Fresh native Window retains full authored state/IDs/stacks/Artboards");
        auto* cold_start=named<QDoubleSpinBox>(cold,"group-path-follow-start");reveal(cold,cold_start);check(cold_start->value()==60&&named<QDoubleSpinBox>(cold,"group-path-follow-normal-offset")->value()==5&&named<QComboBox>(cold,"group-path-follow-contour")->currentData().toString()==QString::fromStdString(contour_b),"Fresh Window controls read retained source/contour/start/offset");const auto cold_pixels=Canvas::render_artboard(cold.host.session.document(),"comp","art",1,false);check(cold_pixels.pixelColor(160,255)==QColor(Qt::red)&&cold_pixels.pixelColor(520,100)==QColor(Qt::blue),"Fresh native Window paints retained derived placement and unrelated artwork");evidence(cold,".cold-native.png");
        Session reopened=cold.host.session;update_clear(cold,reopened);click(cold,"group-path-follow-clear");reopened.apply({GroupPathFollowCommand{ClearGroupPathFollow{"group"}}},reopened.revision());canonical(cold,reopened);attach(cold,reopened,false);evidence(cold,".cold.png");cold.host.changed={};cold.hide();std::cout<<"PASS standard-pane Path Follow contextual authoring; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
