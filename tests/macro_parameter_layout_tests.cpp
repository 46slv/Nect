#include "window.hpp"
#include "nect/io.hpp"
#include "visual_style.hpp"
#include <QApplication>
#include <QAction>
#include <QDockWidget>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
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
Document fixture(bool descriptive){
    Session s(empty_document("macro-layout","composition","board"));
    auto source=default_primitive("source","nect.shape.rectangle");
    auto other=source;other.id="other-source";
    auto offset=default_operation("node-offset","nect.shape.offset");offset.parameters.at("amount").literal=8;
    auto repeater=default_operation("node-repeater","nect.shape.repeater");repeater.parameters.at("copies").literal=2;
    MacroDefinitionRevision r;r.revision=1;r.input={"input","local_paths_and_paint"};r.output={"output","local_paths_and_paint"};
    r.nodes={{offset,"offset-in","offset-out"},{repeater,"repeater-in","repeater-out"}};
    r.edges={{{"","input"},{"node-offset","offset-in"}},{{"node-offset","offset-out"},{"node-repeater","repeater-in"}},{{"node-repeater","repeater-out"},{"","output"}}};
    r.output_mapping={"node-repeater","repeater-out"};
    r.public_parameters.push_back({"macro.offset.amount",descriptive?"Outline expansion distance before repetition":"Expansion distance","node-offset","amount","number","du","local_paths_and_paint"});
    MacroDefinition d;d.id="definition";d.label=descriptive?"Outline Expansion and Repeated Silhouette":"Custom Offset Repeat";d.revisions.emplace(1,r);
    s.apply({CreatePrimitive{"composition","","target","Macro target",source},CreatePrimitive{"composition","","other","Other retained",other},
        MacroCommand{CreateMacroDefinition{d}},MacroCommand{InstantiateMacro{"target","definition","instance",1,1}}},s.revision());
    return s.document();
}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
void history(Window& w,const QString& text){for(auto* a:w.findChildren<QAction*>())if(a->text()==text){a->trigger();events();return;}throw std::runtime_error("Missing history action");}
void canonical(Window& w,Session& expected){
    check(same(w.host.session,expected),"Actual action equals full canonical Document/native/History/revision/preview/generation");
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Undo equals full canonical state including prior history and other object");
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Redo equals full canonical state including Macro graph/public mapping/stack");
}
void reveal(QScrollArea* area,QWidget* control){
    check(control&&control->isVisible()&&control->isEnabled(),"Expected existing control is visible and enabled");
    area->verticalScrollBar()->setValue(control->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Reach "<<control->objectName().toStdString()<<" viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" x="<<control->mapTo(area->viewport(),QPoint()).x()<<" width="<<control->width()<<std::endl;
    if(!control->visibleRegion().contains(control->rect())){
        for(auto* child:area->widget()->findChildren<QWidget*>())if(child->minimumSizeHint().width()>area->viewport()->width()-40)std::cout<<"Minimum owner "<<child->metaObject()->className()<<" "<<child->objectName().toStdString()<<" min="<<child->minimumSizeHint().width()<<std::endl;
        const auto evidence=qEnvironmentVariable("NECT_MACRO_LAYOUT_EVIDENCE");if(!evidence.isEmpty())control->window()->grab().save(evidence+".failure.png");
    }
    check(area->horizontalScrollBar()->value()==0&&area->viewport()->rect().contains(QRect(control->mapTo(area->viewport(),QPoint()),control->size()))&&control->visibleRegion().contains(control->rect()),"Control fully fits default viewport with vertical scrolling only");
}
void select(Window& w){
    auto* dock=w.findChild<QDockWidget*>("structure");dock->show();dock->raise();events();
    auto* tree=dock->findChild<QTreeWidget*>();tree->expandAll();QTreeWidgetItem* item=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="target"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())item=*it;
    check(item,"Stable Macro target Structure row exists");tree->scrollToItem(item);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(item).center());events();
    dock=w.findChild<QDockWidget*>("properties");dock->show();dock->raise();events();
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir scratch;
    if(app.arguments().contains("--application-style")){app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());}
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("MacroParameterLayout");
    try{
        Window w(scratch.filePath("recovery"));w.host.session=Session(fixture(app.arguments().contains("--descriptive-labels")));w.host.session_id="macro-layout-session";w.host.edited();w.show();events();
        const Session initial=w.host.session;select(w);check(same(w.host.session,initial),"Structure selection preserves full canonical Session");
        if(app.arguments().contains("--effects-apply")){
            auto* effects=w.findChild<QDockWidget*>("effects");effects->show();effects->raise();events();
            auto* area=w.findChild<QScrollArea*>("effects-scroll");const auto pane_width=area->viewport()->width();const auto dock_width=effects->width();const auto canvas_width=w.canvas->width();
            auto* search=w.findChild<QLineEdit*>("effects-search");QTest::mouseClick(search,Qt::LeftButton);QTest::keyClicks(search,"Outline Expansion");events();
            auto* catalog=w.findChild<QListWidget*>("effects-catalog");QListWidgetItem* item=nullptr;
            for(int i=0;i<catalog->count();++i)if(catalog->item(i)->data(Qt::UserRole).toString()=="macro:definition")item=catalog->item(i);
            check(item&&!item->isHidden(),"Existing descriptive Macro catalog entry matches actual typed search");catalog->scrollToItem(item);events();
            QTest::mouseClick(catalog->viewport(),Qt::LeftButton,Qt::NoModifier,catalog->visualItemRect(item).center());events();
            auto* apply=w.findChild<QPushButton*>("effects-apply");check(apply&&apply->isEnabled(),"Existing Macro Apply action enabled");
            const auto macro_name=QString::fromStdString(initial.document().macro_definitions.at("definition").label);
            check(apply->text()=="Apply\n"+macro_name&&apply->accessibleName()=="Apply "+macro_name,"Apply keeps the full authored label and accessible action");
            area->verticalScrollBar()->setValue(apply->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
            std::cout<<"Effects initial_viewport="<<pane_width<<" viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" minimum="<<area->widget()->minimumSizeHint().width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" apply_x="<<apply->mapTo(area->viewport(),QPoint()).x()<<" apply_width="<<apply->width()<<" apply_minHint="<<apply->minimumSizeHint().width()<<std::endl;
            for(auto* child:area->widget()->findChildren<QWidget*>())if(child->minimumSizeHint().width()>pane_width-40)std::cout<<child->metaObject()->className()<<" "<<child->objectName().toStdString()<<" minHint="<<child->minimumSizeHint().width()<<std::endl;
            const auto evidence=qEnvironmentVariable("NECT_MACRO_LAYOUT_EVIDENCE");if(!evidence.isEmpty())check(w.grab().save(evidence),"Effects Macro screenshot saved");
            check(same(w.host.session,initial),"Actual search/catalog selection is completely Session neutral");
            check(area->viewport()->width()==pane_width&&w.canvas->width()==canvas_width,"Macro selection preserves standard pane and Canvas widths");
            check(area->horizontalScrollBar()->value()==0&&area->viewport()->rect().contains(QRect(apply->mapTo(area->viewport(),QPoint()),apply->size()))&&apply->visibleRegion().contains(apply->rect()),"Enabled descriptive Macro Apply fully reachable without horizontal scrolling");
            const auto full_caption=QString::fromStdString(initial.document().macro_definitions.at("definition").label)+" · Macro revision v1";
            auto* caption=w.findChild<QLabel*>("effects-operation-caption-instance");
            check(caption&&caption->text()==full_caption&&caption->wordWrap()&&caption->textFormat()==Qt::PlainText&&caption->parentWidget()->accessibleName()==full_caption,"Full authored name/revision and accessible caption retained as plain wrapped text");
            reveal(area,caption);reveal(area,apply);
            const auto index=initial.document().objects.at("target").stack.size();QTest::mouseClick(apply,Qt::LeftButton);events();
            const auto new_id=w.host.session.document().objects.at("target").stack.back().id;Session expected=initial;
            expected.apply({MacroCommand{InstantiateMacro{"target","definition",new_id,1,index}}},expected.revision());
            canonical(w,expected);
            auto* edit=w.findChild<QPushButton*>("effects-edit-properties-"+QString::fromStdString(new_id));reveal(area,edit);
            QTest::mouseClick(edit,Qt::LeftButton);events();check(same(w.host.session,expected),"Actual Edit in Properties navigation preserves full Session");
            auto* properties=w.findChild<QScrollArea*>("inspector-scroll");
            auto* amount=w.findChild<QLineEdit*>("macro-amount-"+QString::fromStdString(new_id));reveal(properties,amount);
            QTest::mouseClick(amount,Qt::LeftButton);amount->selectAll();QTest::keyClicks(amount,"22.1234567890123");QTest::keyClick(amount,Qt::Key_Return);events();
            expected.apply({MacroCommand{SetMacroOverride{"target",new_id,"macro.offset.amount",22.1234567890123}}},expected.revision());canonical(w,expected);
            auto* reset=w.findChild<QPushButton*>("macro-reset-amount-"+QString::fromStdString(new_id));reveal(properties,reset);QTest::mouseClick(reset,Qt::LeftButton);events();
            expected.apply({MacroCommand{ResetMacroOverride{"target",new_id,"macro.offset.amount"}}},expected.revision());canonical(w,expected);
            effects->raise();events();apply=w.findChild<QPushButton*>("effects-apply");reveal(area,apply);
            apply->setFocus();QTest::keyClick(apply,Qt::Key_Space);events();const auto keyboard_id=w.host.session.document().objects.at("target").stack.back().id;
            expected.apply({MacroCommand{InstantiateMacro{"target","definition",keyboard_id,1,index+1}}},expected.revision());canonical(w,expected);
            std::cout<<"After actions viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" canvas_before="<<canvas_width<<" canvas_after="<<w.canvas->width()<<std::endl;
            if(!evidence.isEmpty())check(w.grab().save(evidence+".after.png"),"After-action Effects geometry saved");
            check(effects->width()==dock_width&&w.canvas->width()==canvas_width,"Apply/Undo/Redo/property editing preserve original pane and Canvas widths");
            check(area->horizontalScrollBar()->maximum()==0,"Added Macro cards and vertical scrollbar retain complete horizontal reachability");
            const auto saved=w.host.session.document();const auto file=scratch.filePath("macro-captions.nect");w.host.save(file);
            check(same(w.host.session,expected),"Native save preserves complete Session and full authored labels");
            Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();events();select(cold);
            check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Fresh Window native reopen retains complete graph/revisions/mappings/instances/other object");
            auto* cold_effects=cold.findChild<QDockWidget*>("effects");cold_effects->show();cold_effects->raise();events();
            auto* cold_area=cold.findChild<QScrollArea*>("effects-scroll");caption=cold.findChild<QLabel*>("effects-operation-caption-"+QString::fromStdString(new_id));reveal(cold_area,caption);
            check(caption->text()==full_caption&&cold_area->horizontalScrollBar()->maximum()==0,"Cold Macro caption retains full source at standard width");
            const Session reopened=cold.host.session;auto* cold_edit=cold.findChild<QPushButton*>("effects-edit-properties-"+QString::fromStdString(new_id));reveal(cold_area,cold_edit);cold_edit->setFocus();QTest::keyClick(cold_edit,Qt::Key_Space);events();
            check(same(cold.host.session,reopened),"Cold keyboard Properties navigation remains fully Session neutral");
            if(!evidence.isEmpty())check(cold.grab().save(evidence+".cold.png"),"Cold native Properties evidence saved");
            cold.host.changed={};cold.hide();
            w.host.changed={};w.hide();std::cout<<"PASS Effects Macro discovery; physical input NOT_RUN\n";return 0;
        }
        auto* area=w.findChild<QScrollArea*>("inspector-scroll");auto* input=w.findChild<QLineEdit*>("macro-amount-instance");check(input&&input->isEnabled(),"Existing published Macro amount enabled");
        area->verticalScrollBar()->setValue(input->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
        std::cout<<"Window="<<w.width()<<" viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" minimum="<<area->widget()->minimumSizeHint().width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" input_x="<<input->mapTo(area->viewport(),QPoint()).x()<<" input_width="<<input->width()<<std::endl;
        for(auto* child:area->widget()->findChildren<QWidget*>())if(child->minimumSizeHint().width()>area->viewport()->width()-40)
            std::cout<<child->metaObject()->className()<<" "<<child->objectName().toStdString()<<" width="<<child->width()<<" minHint="<<child->minimumSizeHint().width()<<std::endl;
        const auto evidence=qEnvironmentVariable("NECT_MACRO_LAYOUT_EVIDENCE");if(!evidence.isEmpty())check(w.grab().save(evidence),"Macro Properties screenshot saved");
        check(area->horizontalScrollBar()->value()==0&&area->viewport()->rect().contains(QRect(input->mapTo(area->viewport(),QPoint()),input->size()))&&input->visibleRegion().contains(input->rect()),"Published Macro amount fully reachable at standard Properties width without horizontal scrolling");
        QTest::mouseClick(input,Qt::LeftButton);input->selectAll();QTest::keyClicks(input,"22.1234567890123");QTest::keyClick(input,Qt::Key_Return);events();
        Session expected=initial;expected.apply({MacroCommand{SetMacroOverride{"target","instance","macro.offset.amount",22.1234567890123}}},expected.revision());
        check(same(w.host.session,expected),"Actual pointer and typed amount equal complete canonical Macro command");
        w.host.changed={};w.hide();std::cout<<"PASS existing Macro published amount reachability; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
