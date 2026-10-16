#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHelpEvent>
#include <QDockWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolTip>
#include <QTreeWidget>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* named(QObject& scope,const char* name){auto* p=scope.findChild<T*>(name);check(p,name);return p;}
Document fixture(){
    Session s(empty_document("parent-context","comp","art"));
    auto child=default_primitive("child-source","nect.shape.rectangle");child.parameters.at("center_x").literal=150;child.parameters.at("center_y").literal=120;
    auto parent=child;parent.id="parent-source";parent.parameters.at("center_x").literal=340;
    auto other=parent;other.id="other-source";other.parameters.at("center_y").literal=260;
    s.apply({CreatePrimitive{"comp","","child","Child artwork",child},CreatePrimitive{"comp","","parent","Layout master for retained heading and margin placement",parent},CreatePrimitive{"comp","","other","Layout master for retained heading and margin placement",other},
        Set{{"parent","","transform.tx"},60},Set{{"parent","","transform.ty"},80}},s.revision());return s.document();
}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
void history(Window& w,const QString& text){for(auto* a:w.findChildren<QAction*>())if(a->text()==text){a->trigger();events();return;}throw std::runtime_error("Missing history action");}
void canonical(Window& w,Session& expected){
    check(same(w.host.session,expected),"Actual parent choice equals complete canonical Session");
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Parent Undo preserves complete source/history/preview");
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Parent Redo preserves complete source/history/preview");
}
void select(Window& w){
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();QTreeWidgetItem* item=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="child"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())item=*it;
    check(item,"Exact child Structure row");tree->scrollToItem(item);events();QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(item).center());events();
    dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();
}
void reveal(Window& w){
    auto* button=named<QPushButton>(w,"transform-parent");auto* area=named<QScrollArea>(w,"inspector-scroll");check(button->isVisible()&&button->isEnabled(),"Existing enabled parent action");
    area->verticalScrollBar()->setValue(button->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Parent viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" button="<<button->width()<<" text_px="<<button->fontMetrics().horizontalAdvance(button->text())<<" tooltip="<<button->toolTip().toStdString()<<std::endl;
    check(area->horizontalScrollBar()->value()==0&&area->viewport()->rect().contains(QRect(button->mapTo(area->viewport(),QPoint()),button->size()))&&button->visibleRegion().contains(button->rect()),"Parent action reachable with vertical scrolling only");
    const auto& object=w.host.session.document().objects.at("child");
    if(object.transform_parent){const auto label=QString::fromStdString(w.host.session.document().objects.at(*object.transform_parent).name);
        if(button->fontMetrics().horizontalAdvance(button->text())>button->contentsRect().width()-16&&!button->toolTip().contains(label)){
            const auto path=qEnvironmentVariable("NECT_PARENT_CONTEXT_EVIDENCE");if(!path.isEmpty())w.grab().save(path+".failure.png");
            check(false,"Clipped current parent name is unavailable from the contextual action tooltip");
        }
        check(button->toolTip().contains("("+QString::fromStdString(*object.transform_parent)+")"),"Current-parent tooltip disambiguates same-name objects by stable ID");
        QHelpEvent help(QEvent::ToolTip,button->rect().center(),button->mapToGlobal(button->rect().center()));QApplication::sendEvent(button,&help);events();
        check(QToolTip::isVisible()&&QToolTip::text()==button->toolTip(),"Actual Qt tooltip exposes the complete current parent name and ID");
        const auto path=qEnvironmentVariable("NECT_PARENT_CONTEXT_EVIDENCE");if(!path.isEmpty())for(auto* top:QApplication::topLevelWidgets())
            if(top->isVisible()&&QString::fromLatin1(top->metaObject()->className())=="QTipLabel")check(top->grab().save(path+".tooltip.png"),"Rendered parent tooltip evidence saved");
        QToolTip::hideText();events();
    }
}
void choose(Window& w,Session& expected,bool cancel=false,bool detach=false){
    reveal(w);bool seen=false;std::exception_ptr problem;
    QTimer::singleShot(0,[&]{
        auto* dialog=named<QDialog>(w,"transform-parent-dialog");seen=true;
        try{
            auto* list=named<QListWidget>(*dialog,"transform-parent-list");auto* search=named<QLineEdit>(*dialog,"transform-parent-search");
            if(!detach){QTest::mouseClick(search,Qt::LeftButton);QTest::keyClicks(search,"Layout master");events();}
            QListWidgetItem* item=nullptr;int same_name_count=0;
            for(int i=0;i<list->count();++i){auto* row=list->item(i);if(row->text().contains("Layout master"))++same_name_count;if(row->data(Qt::UserRole).toString()==(detach?"":"parent"))item=row;}
            check(item&&(detach||same_name_count==2),"Same-name parents retain distinct stable IDs");
            if(!detach)check(item->toolTip()=="parent","Parent row exposes exact identity");
            list->scrollToItem(item);events();QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(item).center());events();
            check(named<QCheckBox>(*dialog,"transform-parent-preserve")->isChecked()&&same(w.host.session,expected),"Search/selection/preserve draft is completely Session neutral");
            auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Parent dialog buttons");
            if(cancel)QTest::keyClick(dialog,Qt::Key_Escape);else QTest::mouseClick(buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);
        }catch(...){problem=std::current_exception();dialog->reject();}
    });
    QTest::mouseClick(named<QPushButton>(w,"transform-parent"),Qt::LeftButton);events();check(seen,"Real parent action opened production dialog");if(problem)std::rethrow_exception(problem);
    if(cancel){check(same(w.host.session,expected),"Cancel preserves complete Session");return;}
    expected.apply({SetTransformParent{"child",detach?std::optional<Id>{}:std::optional<Id>{"parent"},true}},expected.revision());canonical(w,expected);reveal(w);
}
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());QTemporaryDir scratch;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());app.setOrganizationName("NectTest");app.setApplicationName("TransformParentContext");
    try{
        Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="parent-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;
        select(w);check(same(w.host.session,expected),"Structure selection is Session neutral");const auto canvas_width=w.canvas->width();const auto dock_width=named<QDockWidget>(w,"properties")->width();
        choose(w,expected,true);choose(w,expected);
        check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Parent changes preserve dock/Canvas widths");
        const auto evidence=qEnvironmentVariable("NECT_PARENT_CONTEXT_EVIDENCE");if(!evidence.isEmpty())check(w.grab().save(evidence),"Parent evidence saved");
        const auto saved=expected.document();const auto file=scratch.filePath("parent.nect");w.host.save(file);check(same(w.host.session,expected),"Native save preserves full Session");w.host.changed={};w.hide();
        Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();select(cold);check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Fresh native Window preserves exact parent/source/sibling IDs");
        Session reopened=cold.host.session;reveal(cold);if(!evidence.isEmpty())check(cold.grab().save(evidence+".cold.png"),"Cold parent evidence saved");choose(cold,reopened,false,true);cold.host.changed={};cold.hide();std::cout<<"PASS parent contextual path; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
