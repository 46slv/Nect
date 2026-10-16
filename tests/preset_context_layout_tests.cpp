#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTabWidget>
#include <QTabBar>
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
QString evidence_path(){auto path=qEnvironmentVariable("NECT_PRESET_LAYOUT_EVIDENCE");if(QApplication::arguments().contains("--batch")&&path.endsWith(".png"))path.insert(path.size()-4,"-batch");return path;}
template<class T>T* named(QObject& scope,const char* name){auto* p=scope.findChild<T*>(name);check(p,name);return p;}
Document fixture(){
    Session s(empty_document("preset-layout","composition","board"));
    auto primitive=default_primitive("source","nect.shape.rectangle");
    auto other=primitive;other.id="other-source";auto untouched=primitive;untouched.id="untouched-source";
    untouched.parameters.at("center_x").literal=250;
    auto offset=default_operation("node-offset","nect.shape.offset");offset.parameters.at("amount").literal=5;
    MacroDefinitionRevision graph;graph.graph_version=2;
    graph.input={"input","local_paths_and_paint"};graph.output={"output","local_paths_and_paint"};
    graph.nodes={{offset,"node-in","node-out"}};
    graph.edges={{{"","input"},{"node-offset","node-in"}},{{"node-offset","node-out"},{"","output"}}};
    graph.output_mapping={"node-offset","node-out"};
    graph.public_parameters={{"macro.offset.amount","Amount","node-offset","amount","number","du","local_paths_and_paint"}};
    MacroDefinition macro;macro.id="source-macro";macro.label="Retained offset";macro.latest_revision=2;
    macro.revisions.emplace(1,graph);graph.revision=2;graph.interface_version=3;
    graph.public_parameters.push_back({"macro.offset.enabled","Use offset","node-offset","enabled","boolean","boolean","local_paths_and_paint"});
    graph.nodes.front().operation.parameters.at("amount").literal=9;macro.revisions.emplace(2,graph);
    PresetDefinition preset;preset.id="source-preset";preset.label="Pinned outline expansion and finishing stroke";preset.schema_version=2;
    PresetEntry first;first.kind="macro";first.type=macro_entry_type;first.macro_definition=macro.id;
    first.pinned_revision=1;first.overrides={{"macro.offset.amount",7}};
    auto second=first;second.pinned_revision=2;second.overrides={{"macro.offset.amount",11}};second.boolean_overrides={{"macro.offset.enabled",false}};
    PresetEntry stroke;stroke.type="nect.paint.stroke";stroke.version=1;
    for(const auto& [key,value]:default_operation("stroke-source",stroke.type).parameters)stroke.parameters.emplace(key,value.literal);
    preset.entries={first,second,stroke};
    s.apply({CreatePrimitive{"composition","","target","Preset target",primitive},CreatePrimitive{"composition","","other","Other retained",other},CreatePrimitive{"composition","","untouched","Untouched artwork",untouched},MacroCommand{CreateMacroDefinition{macro}}},s.revision());
    s.apply_preset_command(PresetCommand{CreatePreset{preset}},s.revision());return s.document();
}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
void history(Window& w,const QString& text){for(auto* a:w.findChildren<QAction*>())if(a->text()==text){a->trigger();events();return;}throw std::runtime_error("Missing history action");}
void canonical(Window& w,Session& expected){
    check(same(w.host.session,expected),"Pointer/keyboard Apply equals complete canonical Session");
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Undo restores complete canonical source/history/preview");
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Redo restores ordered entries/pins/overrides/native/history");
}
void select(Window& w,bool batch=false){
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();
    auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();QTreeWidgetItem* item=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="target"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())item=*it;
    check(item,"Exact target Structure row");tree->scrollToItem(item);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(item).center());events();
    if(batch){
        item=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="other"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())item=*it;
        check(item,"Exact second target Structure row");tree->scrollToItem(item);events();QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::ControlModifier,tree->visualItemRect(item).center());events();
        dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();return;
    }
    dock=named<QDockWidget>(w,"effects");dock->show();dock->raise();events();
    auto* tabs=named<QTabWidget>(w,"effects-tabs");QTest::mouseClick(tabs->tabBar(),Qt::LeftButton,Qt::NoModifier,tabs->tabBar()->tabRect(1).center());events();
}
void reveal(Window& w,QWidget* control,bool batch=false){
    auto* area=named<QScrollArea>(w,batch?"inspector-scroll":"presets-scroll");check(control&&control->isVisible()&&control->isEnabled(),"Existing enabled Preset control");
    area->verticalScrollBar()->setValue(control->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Reach "<<control->objectName().toStdString()<<" viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<" x="<<control->mapTo(area->viewport(),QPoint()).x()<<" width="<<control->width()<<std::endl;
    const bool fits=area->horizontalScrollBar()->value()==0&&area->viewport()->rect().contains(QRect(control->mapTo(area->viewport(),QPoint()),control->size()))&&control->visibleRegion().contains(control->rect());
    if(!fits){for(auto* child:area->widget()->findChildren<QWidget*>())if(child->minimumSizeHint().width()>area->viewport()->width()-16)std::cout<<"Minimum owner "<<child->metaObject()->className()<<" "<<child->objectName().toStdString()<<" min="<<child->minimumSizeHint().width()<<std::endl;
        const auto evidence=evidence_path();if(!evidence.isEmpty())w.grab().save(evidence+".failure.png");}
    check(fits,"Preset input/action fully visible with vertical scrolling only");
}
void apply(Window& w,Session& expected,bool keyboard){
    auto* button=named<QPushButton>(w,"preset-apply");reveal(w,button);const auto before=expected.document().objects.at("target").stack.size();
    if(keyboard){button->setFocus();QTest::keyClick(button,Qt::Key_Space);}else QTest::mouseClick(button,Qt::LeftButton);events();
    const auto& stack=w.host.session.document().objects.at("target").stack;check(stack.size()==before+3,"Apply appended the complete ordered stack");
    const auto id=stack.at(before).id;const auto split=id.rfind("-op-");check(split!=Id::npos&&id.substr(split)=="-op-1","Fresh processing prefix is available");
    expected.apply_preset_command(PresetCommand{ApplyPreset{"source-preset","target",id.substr(0,split)}},expected.revision());canonical(w,expected);
}
void batch_apply(Window& w,Session& expected,bool keyboard){
    auto* combo=named<QComboBox>(w,"preset-batch-catalog");reveal(w,combo,true);combo->setFocus();QTest::keyClick(combo,Qt::Key_End);events();
    check(combo->currentData().toString()=="source-preset"&&combo->currentText()==QString::fromStdString(expected.document().preset_definitions.at("source-preset").label),"Keyboard chooses full retained Preset label and stable ID");
    check(same(w.host.session,expected),"Batch catalog choice preserves full Session");
    auto* button=named<QPushButton>(w,"preset-batch-apply");reveal(w,button,true);
    const auto evidence=evidence_path();if(!evidence.isEmpty())check(w.grab().save(evidence+".selected.png"),"Selected full-label batch screenshot saved");
    if(keyboard){button->setFocus();QTest::keyClick(button,Qt::Key_Space);}else QTest::mouseClick(button,Qt::LeftButton);events();
    ApplyPresetBatch command;command.preset="source-preset";
    for(const auto* id:{"target","other"}){
        const auto before=expected.document().objects.at(id).stack.size();const auto& stack=w.host.session.document().objects.at(id).stack;check(stack.size()==before+3,"Batch appended complete ordered stack to exact target");
        const auto first=stack.at(before).id;const auto split=first.rfind("-op-");check(split!=Id::npos&&first.substr(split)=="-op-1","Batch fresh prefix");command.targets.push_back({id,first.substr(0,split)});
    }
    expected.apply_preset_command(PresetCommand{command},expected.revision());canonical(w,expected);
}
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());QTemporaryDir scratch;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());app.setOrganizationName("NectTest");app.setApplicationName("PresetContextLayout");
    try{
        Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="preset-layout-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();const Session initial=w.host.session;
        const bool batch=app.arguments().contains("--batch");select(w,batch);const auto canvas_width=w.canvas->width();const auto dock_width=named<QDockWidget>(w,batch?"properties":"effects")->width();
        if(batch){
            check(same(w.host.session,initial)&&w.canvas->selected_objects().size()==2,"Actual Ctrl Structure selection is Session neutral and selects exactly two retained objects");
            Session expected=initial;batch_apply(w,expected,false);batch_apply(w,expected,true);
            check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Batch actions preserve dock/Canvas dimensions");
            const auto evidence=evidence_path();if(!evidence.isEmpty())check(w.grab().save(evidence),"Batch Preset screenshot saved");
            const auto saved=expected.document();const auto file=scratch.filePath("batch-preset.nect");w.host.save(file);check(same(w.host.session,expected),"Batch native save preserves full Session");
            w.host.changed={};w.hide();Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();select(cold,true);
            check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Cold batch Window retains full definitions/order/pins/other source");
            Session reopened=cold.host.session;batch_apply(cold,reopened,true);if(!evidence.isEmpty())check(cold.grab().save(evidence+".cold.png"),"Cold batch evidence saved");cold.host.changed={};cold.hide();std::cout<<"PASS existing batch Preset context; physical input NOT_RUN\n";return 0;
        }
        auto* search=named<QLineEdit>(w,"presets-search");reveal(w,search);QTest::mouseClick(search,Qt::LeftButton);QTest::keyClicks(search,"Pinned outline");events();
        auto* list=named<QListWidget>(w,"presets-catalog");check(list->count()==1&&list->item(0)->data(Qt::UserRole).toString()=="source-preset","Typed search resolves stable Preset ID");reveal(w,list);
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(0)).center());events();check(same(w.host.session,initial),"Structure/tab/search/catalog browsing is completely Session neutral");
        Session expected=initial;apply(w,expected,false);apply(w,expected,true);
        check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"effects")->width()==dock_width,"Actions preserve original dock/Canvas dimensions");
        const auto evidence=evidence_path();if(!evidence.isEmpty())check(w.grab().save(evidence),"Preset screenshot saved");
        const auto saved=expected.document();const auto file=scratch.filePath("preset.nect");w.host.save(file);check(same(w.host.session,expected),"Native save is Session neutral");
        w.host.changed={};w.hide();Window cold(scratch.filePath("cold"));cold.host.open(file);cold.show();events();cold.canvas->fit_artboard();events();select(cold);
        check(cold.host.session.document()==saved&&encode(cold.host.session.document())==encode(saved),"Fresh native Window preserves all definitions/ordered entries/pins/source/other objects");
        const Session reopened=cold.host.session;reveal(cold,named<QPushButton>(cold,"preset-apply"));check(same(cold.host.session,reopened),"Cold navigation/reachability is Session neutral");
        if(!evidence.isEmpty())check(cold.grab().save(evidence+".cold.png"),"Cold screenshot saved");cold.host.changed={};cold.hide();std::cout<<"PASS existing Preset context; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
