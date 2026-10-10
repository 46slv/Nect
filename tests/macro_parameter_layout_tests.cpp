#include "window.hpp"
#include "nect/io.hpp"
#include "visual_style.hpp"
#include "macro_boolean_window_smoke.hpp"
#include <QApplication>
#include <QAction>
#include <QDockWidget>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QWindow>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
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
void reset_pending_pointer(const std::string& mode){
    const bool ordinary=mode.starts_with("amount-");const auto cause=ordinary?mode.substr(7):mode;
    QTemporaryDir scratch;check(scratch.isValid(),"Macro Reset owns its scratch");
    auto original=fixture(true);const std::string parameter_id=cause=="public"?"macro.published.distance":"macro.offset.amount";const std::uint64_t pin=cause=="public"?2:1;
    if(cause=="public"){
        auto bad=original;auto& mapping=bad.macro_definitions.at("definition").revisions.at(1).public_parameters.front();mapping.node="node-repeater";mapping.parameter="position_x";
        bool rejected=false;try{decode(encode(bad));}catch(const Error& error){rejected=error.code=="INVALID_MACRO_MAPPING";std::cout<<"Reserved fixture diagnostic: "<<error.code<<std::endl;}
        check(rejected,"Former reserved-ID remap is invalid fixture data before GUI delivery");
        auto& definition=original.macro_definitions.at("definition");auto graph=definition.revisions.at(1);graph.revision=pin;graph.interface_version=2;graph.public_parameters.front().id=parameter_id;
        definition.revisions.emplace(pin,graph);definition.latest_revision=pin;
        for(auto& operation:original.objects.at("target").stack)if(operation.id=="instance")operation.macro->pinned_revision=pin;
    }
    check(decode(encode(original))==original,"Owned Macro fixture round-trips through native validation");Session seed(original);
    seed.apply({MacroCommand{SetMacroOverride{"target","instance",parameter_id,22.1234567890123}},
        MacroCommand{InstantiateMacro{"other","definition","other-instance",pin,1}},
        MacroCommand{SetMacroOverride{"other","other-instance",parameter_id,11}}},seed.revision());
    Window w(scratch.filePath("recovery"));w.host.session=Session(seed.document());w.host.session_id="macro-reset-pointer-session";
    w.host.edited();w.resize(1100,750);w.show();w.activateWindow();events();select(w);
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");
    auto* input=w.findChild<QLineEdit*>("macro-amount-instance");QPointer<QPushButton> reset=w.findChild<QPushButton*>("macro-reset-amount-instance");
    if(cause=="preview-born"){
        w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Set{{"other","","transform.tx"},31}});w.host.edited();events();
        area=w.findChild<QScrollArea*>("inspector-scroll");input=w.findChild<QLineEdit*>("macro-amount-instance");reset=w.findChild<QPushButton*>("macro-reset-amount-instance");
        check(w.host.session.gesture_active(),"Preview-born Macro controls have an actual active gesture");w.host.session.cancel_gesture();
    }
    check(area&&input&&reset,"Exact existing Macro Amount and Reset controls exist");reveal(area,input);
    QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,input->mapTo(&w,input->rect().center()));
    QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,cause=="invalid"?"broken":"30");events();
    Session expected=w.host.session;check(input->hasFocus()&&input->isModified()&&same(w.host.session,expected),"Pending Macro amount preserves full source/native/history/preview");
    if(cause=="document"||cause=="source"||cause=="owner"||cause=="override"||cause=="instance"||cause=="pinned"||cause=="public"||cause=="later"||cause=="unrelated"||cause=="equivalent"){
        auto incoming=w.host.session.document();
        if(cause=="document")incoming.id="replacement-macro-reset-document";
        else if(cause=="source")incoming.macro_definitions.at("definition").revisions.at(1).nodes.front().operation.parameters.at("amount").literal=9;
        else if(cause=="owner")incoming.objects.at("target").anchor.at(0).literal=13;
        else if(cause=="override"||cause=="instance"){
            for(auto& operation:incoming.objects.at("target").stack)if(operation.id=="instance"){
                if(cause=="override")operation.macro->overrides.at(parameter_id)=23;else operation.id="replacement-instance";
            }
        }
        else if(cause=="pinned"||cause=="later"){
            auto& definition=incoming.macro_definitions.at("definition");auto next=definition.revisions.at(1);next.revision=2;next.nodes.front().operation.parameters.at("amount").literal=9;definition.revisions.emplace(2,next);definition.latest_revision=2;
            if(cause=="pinned")for(auto& operation:incoming.objects.at("target").stack)if(operation.id=="instance")operation.macro->pinned_revision=2;
        }else if(cause=="public"){
            auto& parameter=incoming.macro_definitions.at("definition").revisions.at(pin).public_parameters.front();parameter.node="node-repeater";parameter.parameter="rotation";parameter.unit="degree";
        }else if(cause=="unrelated")incoming.objects.at("other").transform.at(4).literal=31;
        check(decode(encode(incoming))==incoming,"Changed Macro context is valid complete native data before GUI delivery");w.host.session=Session(incoming);expected=w.host.session;
    }else if(cause=="generation"||cause=="preview"){
        w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Set{{"other","","transform.tx"},31}});
        if(cause=="generation")w.host.session.cancel_gesture();expected=w.host.session;
    }else if(cause=="session")w.host.session_id="replacement-macro-session";
    else if(cause=="revision"){
        w.host.session.apply({Set{{"other","","transform.tx"},31}},w.host.session.revision());expected=w.host.session;
    }
    const auto protected_graphs=expected.document().macro_definitions;
    const bool refusal=cause=="document"||cause=="source"||cause=="generation"||cause=="preview"||cause=="preview-born"||cause=="owner"||cause=="override"||cause=="instance"||cause=="pinned"||cause=="public"||cause=="session"||cause=="revision";
    if(ordinary){
        QTest::keyClick(input,Qt::Key_Return);events();
        if(refusal)check(same(w.host.session,expected)&&input->isModified()&&input->text()=="30","Stale ordinary Macro amount retains full incoming Session and pending draft");
        else{expected.apply({MacroCommand{SetMacroOverride{"target","instance",parameter_id,30}}},expected.revision());canonical(w,expected);}
        if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.changed={};w.hide();events();std::cout<<"PASS original Macro amount completion / "<<cause<<"; physical input NOT_RUN\n";return;
    }
    reveal(area,reset);const auto position=reset->mapTo(&w,reset->rect().center());
    check(w.childAt(position)==reset,"Actual Window pointer hits exact Macro Reset");
    if(mode=="cancel"){
        QTest::mousePress(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
        check(same(w.host.session,expected)&&input->isModified(),"Cancelled Reset press retains full Session and pending ordinary draft");
        QTest::mouseRelease(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position+QPoint(reset->width()+20,0));events();
        check(same(w.host.session,expected)&&input->isModified(),"Released-away Reset remains neutral");
        QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,input->mapTo(&w,input->rect().center()));QTest::keyClick(input,Qt::Key_Return);events();
        expected.apply({MacroCommand{SetMacroOverride{"target","instance",parameter_id,30}}},expected.revision());canonical(w,expected);
    }else{
        QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
        if(refusal){
            std::cout<<"Macro Reset "<<mode<<" actual_revision="<<w.host.session.revision()<<" expected_revision="<<expected.revision()<<std::endl;
            check(same(w.host.session,expected),"Stale Macro Reset preserves incoming full source/Document/history/preview/generation");
            check(input->isModified()&&input->text()=="30","Refused Macro Reset preserves the pending ordinary draft");
            if(w.host.session.gesture_active())w.host.session.cancel_gesture();
        }else{
            expected.apply({MacroCommand{ResetMacroOverride{"target","instance",parameter_id}}},expected.revision());canonical(w,expected);
            check(w.host.session.document().macro_definitions==protected_graphs,"Reset retains exact pinned source graph/public interface and unrelated revisions");
        }
    }
    w.host.changed={};w.hide();events();std::cout<<"PASS Macro Reset original Window pointer / "<<mode<<"; physical input NOT_RUN\n";
}
void boolean_context_pointer(const char* parameter,bool resetting,const std::string& cause){
    namespace boolean=macro_boolean_window_smoke;
    QTemporaryDir scratch;check(scratch.isValid(),"Boolean discovery owns scratch");
    auto document=boolean::fixture();
    const bool initial_default=std::string(parameter)==boolean::offset_enabled_id;
    if(resetting){Session seed(document);seed.apply({MacroCommand{SetMacroBooleanOverride{"path","instance",parameter,cause=="same-default"?initial_default:!initial_default}}},0);document=seed.document();}
    check(decode(encode(document))==document,"Complete Boolean fixture native-validates before Window installation");
    Window w(scratch.filePath("recovery"));w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="boolean-context-session";w.host.edited();w.show();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("path");events();
    if(cause=="preview-born"){
        w.host.session.begin_gesture(0);w.host.session.update_gesture({Rename{"other","Preview other"}});w.host.edited();events();
        w.host.session.cancel_gesture();
    }
    const QPointer<QCheckBox> toggle=boolean::toggle(w,parameter);
    QAbstractButton* button=resetting?static_cast<QAbstractButton*>(boolean::reset(w,parameter)):toggle.data();
    check(button!=nullptr,"Existing Boolean toggle/Reset exists");boolean::reveal(w,button);
    const auto position=button->mapTo(&w,QPoint(8,button->height()/2));
    check(w.childAt(position)==button,"Actual Window pointer hits the exact published Boolean control");
    const bool checked=toggle->isChecked();
    auto incoming=[&]{
        if(cause=="session")w.host.session_id+="-replacement";
        else if(cause=="revision")w.host.session.apply({Rename{"other","New other"}},w.host.session.revision());
        else if(cause=="generation"||cause=="preview"){
            w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"other","Preview other"}});
            if(cause=="generation")w.host.session.cancel_gesture();
        }else if(cause=="document"||cause=="queued-document"||cause=="owner"||cause=="source"||cause=="override"||cause=="pinned"||cause=="public"||cause=="equivalent"||cause=="later"||cause=="unrelated"){
            auto replacement=w.host.session.document();auto& definition=replacement.macro_definitions.at("boolean-definition");
            if(cause=="document"||cause=="queued-document")replacement.id="incoming-boolean-document";
            else if(cause=="owner")replacement.objects.at("path").name="Incoming owner";
            else if(cause=="source"){
                for(auto& node:definition.revisions.at(2).nodes)if(node.operation.id=="offset-target")node.operation.parameters.at("amount").literal=7;
            }else if(cause=="override"){
                for(auto& op:replacement.objects.at("path").stack)if(op.macro)op.macro->overrides.at(boolean::amount_id)=19;
            }
            else if(cause=="pinned"||cause=="later"){
                auto next=definition.revisions.at(2);next.revision=3;definition.revisions.emplace(3,next);definition.latest_revision=3;
                if(cause=="pinned")for(auto& op:replacement.objects.at("path").stack)if(op.macro)op.macro->pinned_revision=3;
            }else if(cause=="public"){
                for(auto& publication:definition.revisions.at(2).public_parameters)if(publication.value_type=="boolean")
                    publication.node=publication.node=="offset-target"?"repeater-target":"offset-target";
            }else if(cause=="unrelated")replacement.objects.at("other").name="Unrelated source";
            check((replacement==w.host.session.document())==(cause=="equivalent"),"Fixture actually changes the selected incoming context except the explicit equivalent case");
            check(decode(encode(replacement))==replacement,"Complete incoming Boolean context native-validates before GUI delivery");
            w.host.session=Session(replacement);
        }
    };
    const bool refusal=cause=="document"||cause=="queued-document"||cause=="owner"||cause=="source"||cause=="override"||cause=="pinned"||cause=="public"||cause=="session"||cause=="revision"||cause=="generation"||cause=="preview"||cause=="preview-born";
    if(cause!="queued-document")incoming();
    Session expected=w.host.session;
    if(cause=="cancel"){
        QTest::mousePress(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
        check(same(w.host.session,expected),"Press alone preserves complete Boolean Session");
        QTest::mouseRelease(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position+QPoint(button->width()+20,0));events();
        check(same(w.host.session,expected),"Released-away Boolean activation preserves complete Session");
    }else{
        if(cause=="keyboard"){button->setFocus(Qt::OtherFocusReason);QTest::keyClick(button,Qt::Key_Space);}
        else QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);
        if(cause=="queued-document"){incoming();expected=w.host.session;}
        events();
        if(refusal){
            std::cout<<"Boolean "<<parameter<<" "<<(resetting?"reset":"toggle")<<" / "<<cause<<" actual_revision="<<w.host.session.revision()<<" expected_revision="<<expected.revision()<<std::endl;
            check(same(w.host.session,expected),"Stale Boolean callback preserves full incoming Document/native/History/preview/generation");
            if(toggle)check(toggle->isChecked()==checked,"Rejected Boolean activation restores its captured presentation");
        }else{
            if(resetting)expected.apply({MacroCommand{ResetMacroOverride{"path","instance",parameter}}},expected.revision());
            else expected.apply({MacroCommand{SetMacroBooleanOverride{"path","instance",parameter,!checked}}},expected.revision());
            canonical(w,expected);
            if(cause=="valid")boolean::cold_readback(w.host.session.document());
        }
    }
    if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.changed={};w.hide();events();
    std::cout<<"PASS Boolean "<<parameter<<" "<<(resetting?"reset":"toggle")<<" / "<<cause<<"; physical input NOT_RUN\n";
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir scratch;
    if(app.arguments().contains("--application-style")){app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());}
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("MacroParameterLayout");
    try{
        if(app.arguments().contains("--runtime-metrics")){
            std::cout<<"Qt platform="<<app.platformName().toStdString()<<" font="<<app.font().toString().toStdString()
                <<" dpi="<<app.primaryScreen()->logicalDotsPerInch()<<" dpr="<<app.primaryScreen()->devicePixelRatio()<<'\n';return 0;
        }
        if(app.arguments().contains("--boolean-context-pointer")){
            bool failed=false;
            for(const auto* parameter:{macro_boolean_window_smoke::offset_enabled_id,macro_boolean_window_smoke::repeater_enabled_id})
                for(const bool resetting:{false,true})
                    for(const auto* cause:{"valid","keyboard","cancel","document","queued-document","owner","source","override","pinned","public","session","revision","generation","preview","preview-born","equivalent","later","unrelated","same-default"}){
                        if(!resetting&&std::string(cause)=="same-default")continue;
                        try{boolean_context_pointer(parameter,resetting,cause);}catch(const std::exception& error){failed=true;std::cerr<<"Boolean "<<parameter<<" "<<(resetting?"reset":"toggle")<<" / "<<cause<<": "<<error.what()<<'\n';}
                    }
            check(!failed,"Existing Boolean Window-pointer/deferred callbacks satisfy complete context protection");
            std::cout<<"macro_boolean_context_pointer: "<<checks<<" checks passed; physical input NOT_RUN\n";return 0;
        }
        if(app.arguments().contains("--boolean-existing-contract")){
            std::cout<<"macro_boolean_existing: "<<macro_boolean_window_smoke::run()<<" checks passed; physical input NOT_RUN\n";return 0;
        }
        if(app.arguments().contains("--boolean-existing-gesture")){
            Window w(scratch.filePath("gesture"));w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);w.show();events();
            try{macro_boolean_window_smoke::refusal(w,"GESTURE_ACTIVE",false);}
            catch(...){std::cerr<<"Existing Boolean gesture status: "<<w.statusBar()->currentMessage().toStdString()<<'\n';w.host.changed={};throw;}
            w.host.changed={};return 0;
        }
        if(app.arguments().contains("--reset-pending-pointer")){
            bool failed=false;for(const auto* mode:{"valid","invalid","cancel","document","source","generation","preview","preview-born","owner","override","instance","pinned","public","session","revision","equivalent","later","unrelated","amount-valid","amount-document","amount-source","amount-generation","amount-preview","amount-preview-born","amount-owner","amount-override","amount-instance","amount-pinned","amount-public","amount-session","amount-revision","amount-later","amount-unrelated"}){
                try{reset_pending_pointer(mode);}catch(const std::exception& error){failed=true;std::cerr<<"Macro Reset "<<mode<<": "<<error.what()<<"\n";}
            }
            check(!failed,"All existing Macro Reset pointer/context cases satisfy the accepted contract");std::cout<<"macro_reset_pending_pointer: "<<checks<<" checks passed; Qt Window/ordinary completion route\n";return 0;
        }
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
