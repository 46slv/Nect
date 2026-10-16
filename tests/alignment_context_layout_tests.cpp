#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* named(QObject& scope,const char* name){auto* p=scope.findChild<T*>(name);check(p,name);return p;}
Object rectangle(const Id& id,double x,double y,double width,double height){
    Object o;o.id=id;o.name=id=="b"?"Fixed spacing reference with retained finishing stroke":id;
    Contour c;c.id=id+"-contour";c.closed=true;
    for(auto p:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}){
        Point q;q.id=id+std::to_string(c.points.size());q.x.literal=p.x;q.y.literal=p.y;c.points.push_back(q);
    }
    o.contours.push_back(c);auto stroke=default_operation(id+"-stroke","nect.paint.stroke");stroke.parameters.at("width").literal=6;o.stack.push_back(stroke);return o;
}
Document fixture(){
    auto d=empty_document("alignment-context","comp","art");
    d.objects.emplace("a",rectangle("a",0,0,10,10));d.objects.emplace("b",rectangle("b",30,25,20,10));d.objects.emplace("c",rectangle("c",90,60,10,10));
    d.objects.emplace("other",rectangle("other",210,120,40,30));d.compositions[0].roots={"a","b","c","other"};
    auto& board=d.compositions[0].artboards[0];board.width=960;board.height=480;
    board.layout=ArtboardLayout{std::nullopt,Grid{"grid",{40,0,100,100},1,1,0,0}};
    d.compositions[0].guides={{"guide-x","Vertical guide","x",100},{"guide-y","Horizontal guide","y",200}};
    auto second=board;second.id="other-art";second.name="Other retained Artboard";second.x=1000;second.layout.reset();d.compositions[0].artboards.push_back(second);
    return d;
}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
void history(Window& w,const QString& text){for(auto* a:w.findChildren<QAction*>())if(a->text()==text){a->trigger();events();return;}throw std::runtime_error("Missing history action");}
void canonical(Window& w,Session& expected){
    check(same(w.host.session,expected),"Real alignment action equals complete canonical Session/native/history/preview");
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Undo restores complete canonical state");
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Redo restores complete canonical state");
}
void select(Window& w){
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();
    auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();
    bool first=true;for(const auto* id:{"a","b","c"}){
        QTreeWidgetItem* item=nullptr;
        for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()==id&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())item=*it;
        check(item,"Exact whole-object Structure row");tree->scrollToItem(item);events();
        QTest::mouseClick(tree->viewport(),Qt::LeftButton,first?Qt::NoModifier:Qt::ControlModifier,tree->visualItemRect(item).center());events();first=false;
    }
    check(w.canvas->selected_objects()==std::vector<Id>{"a","b","c"},"Actual Ctrl Structure selection retains exact whole-object IDs");
    dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();
}
void reveal(Window& w,QWidget* control){
    auto* area=named<QScrollArea>(w,"inspector-scroll");check(control&&control->isVisible()&&control->isEnabled(),"Existing enabled alignment control");
    area->verticalScrollBar()->setValue(control->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Reach "<<control->objectName().toStdString()<<" viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" x="<<control->mapTo(area->viewport(),QPoint()).x()<<" width="<<control->width()<<std::endl;
    const bool fits=area->horizontalScrollBar()->value()==0&&area->viewport()->rect().contains(QRect(control->mapTo(area->viewport(),QPoint()),control->size()))&&control->visibleRegion().contains(control->rect());
    if(!fits){for(auto* child:area->widget()->findChildren<QWidget*>())if(child->minimumSizeHint().width()>area->viewport()->width()-16)std::cout<<"Minimum owner "<<child->metaObject()->className()<<" "<<child->objectName().toStdString()<<" min="<<child->minimumSizeHint().width()<<std::endl;
        const auto evidence=qEnvironmentVariable("NECT_ALIGNMENT_LAYOUT_EVIDENCE");if(!evidence.isEmpty())w.grab().save(evidence+".failure.png");}
    check(fits,"Alignment control fully visible with vertical scrolling only");
}
void key_reference(Window& w){
    auto* combo=named<QComboBox>(w,"alignment-target");reveal(w,combo);
    const auto index=combo->findData("key_object:b");check(index>=0,"Stable key identity offered");combo->setFocus();QTest::keyClick(combo,Qt::Key_Home);
    for(int i=0;i<index;++i)QTest::keyClick(combo,Qt::Key_Down);events();
    check(combo->currentData().toString()=="key_object:b"&&combo->currentText().contains("Fixed spacing reference with retained finishing stroke")&&combo->currentText()==combo->currentData(Qt::ToolTipRole).toString(),"Keyboard chooses full-label exact key object");
}
void spacing(Window& w,const Session& expected,const QString& gap="7.25"){
    auto* edit=named<QLineEdit>(w,"distribution-spacing");reveal(w,edit);
    QTest::mouseClick(edit,Qt::LeftButton);QTest::keyClick(edit,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(edit,gap);QTest::keyClick(edit,Qt::Key_Return);events();
    check(edit->text()==gap&&named<QPushButton>(w,"quick-distribute-x")->isEnabled(),"Finite key gap enables explicit Distribute");
    check(same(w.host.session,expected),"Reference and spacing draft/Enter are completely Session neutral");
}
void distribute(Window& w,Session& expected,bool keyboard,double gap=7.25){
    auto* button=named<QPushButton>(w,"quick-distribute-x");reveal(w,button);const auto key=expected.document().objects.at("b");
    if(keyboard){button->setFocus();QTest::keyClick(button,Qt::Key_Space);}else QTest::mouseClick(button,Qt::LeftButton);events();
    expected.apply({DistributeObjects{{"a","b","c"},"x","key_object:b",gap}},expected.revision());
    check(w.host.session.document().objects.at("b")==key,"Distribution keeps complete key object fixed including stroke/source");canonical(w,expected);
}
void align(Window& w,Session& expected){
    auto* button=named<QPushButton>(w,"quick-align-y-min");reveal(w,button);const auto key=expected.document().objects.at("b");
    button->setFocus();QTest::keyClick(button,Qt::Key_Space);events();
    expected.apply({AlignObjects{{"a","b","c"},"y","min",{},"key_object:b"}},expected.revision());
    check(w.host.session.document().objects.at("b")==key,"Alignment keeps complete key object fixed");canonical(w,expected);
}
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());QTemporaryDir scratch;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());app.setOrganizationName("NectTest");app.setApplicationName("AlignmentContextLayout");
    try{
        Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="alignment-layout-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
        Session expected=w.host.session;select(w);check(same(w.host.session,expected),"Actual Structure navigation is Session neutral");
        const auto canvas_width=w.canvas->width();const auto dock_width=named<QDockWidget>(w,"properties")->width();key_reference(w);spacing(w,expected);
        distribute(w,expected,false);align(w,expected);
        check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Draft/action/history preserve standard dock and Canvas dimensions");
        const auto evidence=qEnvironmentVariable("NECT_ALIGNMENT_LAYOUT_EVIDENCE");if(!evidence.isEmpty())check(w.grab().save(evidence),"Alignment evidence saved");
        const auto saved=expected.document();const auto file=scratch.filePath("alignment.nect");w.host.save(file);check(same(w.host.session,expected),"Native save is Session neutral");
        w.host.changed={};w.hide();Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();select(cold);
        check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Fresh native Window preserves complete source/IDs/artboards/grid/guides/other artwork");
        Session reopened=cold.host.session;key_reference(cold);spacing(cold,reopened,"12.5");distribute(cold,reopened,true,12.5);
        if(!evidence.isEmpty())check(cold.grab().save(evidence+".cold.png"),"Cold alignment evidence saved");cold.host.changed={};cold.hide();
        std::cout<<"PASS existing alignment reference/key gap/actions; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
