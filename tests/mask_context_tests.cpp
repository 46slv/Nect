#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QHelpEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolTip>
#include <QTreeWidget>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* named(QObject& scope,const char* name){auto* p=scope.findChild<T*>(name);check(p,name);return p;}
Object rectangle(const Id& id,double x,double y,double width,double height,QColor color){
    Object o;o.id=id;o.name=id;Contour c;c.id=id+"-contour";c.closed=true;
    for(auto xy:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}){Point p;p.id=id+std::to_string(c.points.size());p.x.literal=xy.x;p.y.literal=xy.y;c.points.push_back(p);}
    o.contours.push_back(c);auto fill=default_operation(id+"-fill","nect.paint.fill");
    fill.parameters.at("r").literal=color.redF();fill.parameters.at("g").literal=color.greenF();fill.parameters.at("b").literal=color.blueF();o.stack.push_back(fill);return o;
}
Document fixture(){
    auto d=empty_document("mask-context","comp","art");d.compositions.front().artboards.front().width=640;d.compositions.front().artboards.front().height=480;
    d.objects.emplace("content",rectangle("content",100,100,200,200,Qt::red));d.compositions.front().roots={"content"};
    Session s(d);auto source=default_primitive("clip-generator","nect.shape.circle");source.parameters.at("center_x").literal=200;source.parameters.at("center_y").literal=200;source.parameters.at("radius").literal=65;
    // Same-name sibling proves that the contextual source identity cannot rely on a label alone.
    s.apply({CreatePrimitive{"comp","","clip","Circular clipping source for retained heading and margin artwork",source}},s.revision());
    d=s.document();d.objects.emplace("other",rectangle("other",400,100,80,80,Qt::blue));d.compositions.front().roots.push_back("other");d.objects.at("other").name=d.objects.at("clip").name;
    Session masked(d);masked.apply({MaskObjects{"comp","",{"content","clip"},"masked","mask","Masked artwork",true}},masked.revision());return masked.document();
}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
void select(Window& w,const Id& id){
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()==QString::fromStdString(id)&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())row=*it;
    check(row,"Exact Structure object row");tree->scrollToItem(row);events();QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();check(w.canvas->selected_object==id,"Structure selects exact object");
    dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();
}
void evidence(Window& w,const QString& suffix){const auto path=qEnvironmentVariable("NECT_MASK_CONTEXT_EVIDENCE");if(!path.isEmpty())check(w.grab().save(path+suffix),"Window evidence saved");}
void reveal(Window& w,QWidget* control){
    auto* area=named<QScrollArea>(w,"inspector-scroll");check(control->isVisible()&&control->isEnabled(),"Actual enabled Mask control");area->verticalScrollBar()->setValue(control->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Reach "<<control->objectName().toStdString()<<" viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" width="<<control->width()<<"\n";
    const bool fits=area->horizontalScrollBar()->value()==0&&area->viewport()->rect().contains(QRect(control->mapTo(area->viewport(),QPoint()),control->size()))&&control->visibleRegion().contains(control->rect());
    if(!fits){for(auto* child:area->widget()->findChildren<QWidget*>())if(child->minimumSizeHint().width()>area->viewport()->width()-16)std::cout<<"Minimum owner "<<child->metaObject()->className()<<" "<<child->objectName().toStdString()<<" min="<<child->minimumSizeHint().width()<<"\n";evidence(w,".failure.png");}
    check(fits,"Mask action reachable with vertical scrolling only");
}
QImage render(const Document& d){return Canvas::render_artboard(d,"comp","art",1,false);}
void pixels(Window& w,const Session& expected){
    const auto image=render(w.host.session.document());check(image==render(expected.document()),"Actual composed pixels equal canonical Session renderer");
    check(image.pixelColor(200,200)==QColor(Qt::red)&&image.pixelColor(110,110).alpha()==0&&image.pixelColor(440,140)==QColor(Qt::blue),"Circle clips Path content while preserving unrelated artwork");
    const auto path=qEnvironmentVariable("NECT_MASK_CONTEXT_EVIDENCE");if(!path.isEmpty())check(image.save(path+".artwork.png"),"Composed artwork evidence saved");
}
void history(Window& w,const QString& text){for(auto* a:w.findChildren<QAction*>())if(a->text()==text){a->trigger();events();return;}throw std::runtime_error("Missing history action");}
void rule(Window& w,Session& expected,bool pointer){
    auto* combo=named<QComboBox>(w,"mask-fill-rule");reveal(w,combo);const auto requested=pointer?"evenodd":"nonzero";
    check(combo->currentData().toString()==(pointer?"nonzero":"evenodd"),"Initial Geometry rule");
    if(pointer){
        QTest::mouseClick(combo,Qt::LeftButton);events();auto* view=combo->view();const auto index=combo->model()->index(1,0);
        check(view->isVisible()&&view->viewport()->rect().contains(view->visualRect(index).center()),"Actual fill-rule popup row is reachable");
        // Match the existing Text pointer path: Qt guards the opening release until movement/time.
        QTest::mouseMove(view->viewport(),view->visualRect(index).center());QTest::qWait(QApplication::doubleClickInterval()+10);
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(index).center());
    }
    else{combo->setFocus();QTest::keyClick(combo,Qt::Key_Up);}events();
    auto mask=*expected.document().objects.at("masked").compositing.mask;mask.fill_rule=requested;expected.apply({SetMask{"masked",mask}},expected.revision());
    if(!same(w.host.session,expected))std::cout<<"Actual rule="<<w.host.session.document().objects.at("masked").compositing.mask->fill_rule<<" revision="<<w.host.session.revision()<<" expected="<<expected.revision()<<" doc_equal="<<(w.host.session.document()==expected.document())<<" history_equal="<<(w.host.session.history()==expected.history())<<" generation="<<w.host.session.gesture_generation()<<" expected_generation="<<expected.gesture_generation()<<"\n";
    check(same(w.host.session,expected),"Actual fill-rule equals complete canonical SetMask Session");pixels(w,expected);
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Mask Undo retains full source/history/preview");pixels(w,expected);
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Mask Redo retains full source/history/preview");pixels(w,expected);
}
void views(Window& w,const Session& expected,const QString& suffix={}){
    auto* outline=named<QCheckBox>(w,"mask-show-outline");reveal(w,outline);const auto before=render(expected.document());const bool old=w.canvas->show_mask_outline();QTest::mouseClick(outline,Qt::LeftButton,Qt::NoModifier,QPoint(8,outline->height()/2));events();
    check(w.canvas->show_mask_outline()!=old&&same(w.host.session,expected)&&render(w.host.session.document())==before,"Outline only changes viewport; full Session and composed artwork unchanged");
    auto* edit=named<QPushButton>(w,"mask-edit-source");reveal(w,edit);const auto& source=expected.document().objects.at("clip");
    check(edit->text()=="Edit source"&&edit->fontMetrics().horizontalAdvance(edit->text())+16<=edit->contentsRect().width(),"Edit source action name remains readable at standard pane width");
    std::cout<<"Edit source caption_px="<<edit->fontMetrics().horizontalAdvance(edit->text())<<" button="<<edit->width()<<" tooltip="<<edit->toolTip().toStdString()<<"\n";
    evidence(w,suffix);check(edit->toolTip().contains(QString::fromStdString(source.name))&&edit->toolTip().contains("(clip)"),"Contextual source tooltip exposes full authored name and exact retained ID");
    QHelpEvent help(QEvent::ToolTip,edit->rect().center(),edit->mapToGlobal(edit->rect().center()));QApplication::sendEvent(edit,&help);events();check(QToolTip::isVisible()&&QToolTip::text()==edit->toolTip(),"Actual rendered Qt tooltip shows retained source identity");
    const auto path=qEnvironmentVariable("NECT_MASK_CONTEXT_EVIDENCE");if(!path.isEmpty())for(auto* top:QApplication::topLevelWidgets())if(top->isVisible()&&QString::fromLatin1(top->metaObject()->className())=="QTipLabel")check(top->grab().save(path+suffix+".tooltip.png"),"Source tooltip image saved");QToolTip::hideText();
    QTest::mouseClick(edit,Qt::LeftButton);events();check(w.canvas->selected_object=="clip"&&!w.host.session.document().objects.at("clip").visible&&same(w.host.session,expected),"Edit source selects exact hidden retained source without authored mutation");pixels(w,expected);select(w,"masked");
    edit=named<QPushButton>(w,"mask-edit-source");reveal(w,edit);edit->setFocus();QTest::keyClick(edit,Qt::Key_Space);events();check(w.canvas->selected_object=="clip"&&!w.host.session.document().objects.at("clip").visible&&same(w.host.session,expected),"Keyboard Edit source retains full hidden artwork/Session");select(w,"masked");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());QTemporaryDir scratch;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());app.setOrganizationName("NectTest");app.setApplicationName("MaskContext");
    try{
        Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="mask-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;
        select(w,"masked");check(same(w.host.session,expected),"Structure selection is Session neutral");const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
        auto* path=named<QComboBox>(w,"group-path-follow-source");reveal(w,path);path->setFocus();QTest::keyClick(path,Qt::Key_Home);events();check(path->currentData().toString()=="other"&&path->currentText()==path->toolTip()&&path->currentText()==path->currentData(Qt::ToolTipRole).toString()&&same(w.host.session,expected),"Long-name existing Path chooser retains complete label/exact ID and Session-neutral draft");
        rule(w,expected,true);views(w,expected);check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Mask edits preserve standard dock/Canvas widths");
        const auto file=scratch.filePath("mask.nect");w.host.save(file);check(same(w.host.session,expected),"Native save preserves complete live Session");const auto saved=expected.document();w.host.changed={};w.hide();
        Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();select(cold,"masked");check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Fresh native Window preserves complete retained Mask/source/other artwork");Session reopened=cold.host.session;pixels(cold,reopened);views(cold,reopened,".cold-context.png");evidence(cold,".cold.png");
        rule(cold,reopened,false);cold.host.changed={};cold.hide();std::cout<<"PASS Mask contextual path; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
