#pragma once
#include "window.hpp"
#include "semantic_control.hpp"
#include "semantic_slider.hpp"
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <stdexcept>

// REQ-161 bounded Copies family: production Window Qt/API events and canonical
// readback. This does not qualify physical OS input or the whole requirement.
namespace semantic_slider_window_smoke {
using namespace nect;
using namespace nect::desktop;
inline int checks=0;
inline void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
inline void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
inline const Ref copies=operation_ref("built","repeater","copies");
inline const Ref second_copies=operation_ref("second","second-repeater","copies");
inline const Ref amount=operation_ref("built","offset","amount");
inline const Ref rotation=operation_ref("built","repeater","rotation");
inline const Ref mapped_copies=macro_parameter_ref("macro","instance","macro.repeater.copies");
inline const Ref mapped_amount=macro_parameter_ref("macro","instance","macro.offset.amount");
inline const Ref mapped_rotation=macro_parameter_ref("macro","instance","macro.repeater.rotation");
constexpr double exact_amount=12.34567890123456,exact_rotation=-725.1234567890123;
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),
        history(session.history()),native(encode(session.document())){}
    bool unchanged(const Session& session)const{return document==session.document()&&revision==session.revision()&&
        history==session.history()&&native==encode(session.document());}
};
inline Document fixture(){
    Session session(empty_document("slider-document","composition","artboard"));
    auto source=default_primitive("built-source","nect.shape.rectangle");
    auto offset=default_operation("offset","nect.shape.offset");offset.parameters.at("amount").literal=exact_amount;
    auto repeater=default_operation("repeater","nect.shape.repeater");repeater.parameters.at("copies").literal=4;
    repeater.parameters.at("rotation").literal=exact_rotation;repeater.parameters.at("position_x").literal=1;
    auto macro_source=source;macro_source.id="macro-source";
    auto macro_offset=offset;macro_offset.id="node-offset";
    auto macro_repeater=repeater;macro_repeater.id="node-repeater";
    MacroDefinitionRevision revision;revision.revision=2;revision.graph_version=2;revision.interface_version=2;
    revision.input={"input","local_paths_and_paint"};revision.output={"output","local_paths_and_paint"};
    revision.nodes={{macro_offset,"offset-in","offset-out"},{macro_repeater,"repeater-in","repeater-out"}};
    revision.edges={{{"","input"},{"node-offset","offset-in"}},
        {{"node-offset","offset-out"},{"node-repeater","repeater-in"}},
        {{"node-repeater","repeater-out"},{"","output"}}};
    revision.output_mapping={"node-repeater","repeater-out"};
    revision.public_parameters={{"macro.repeater.copies","Published copies","node-repeater","copies","number","scalar","local_paths_and_paint"},
        {"macro.offset.amount","Published amount","node-offset","amount","number","du","local_paths_and_paint"},
        {"macro.repeater.rotation","Published rotation","node-repeater","rotation","number","degree","local_paths_and_paint"}};
    auto initial=revision;initial.revision=1;initial.interface_version=1;
    initial.public_parameters={revision.public_parameters.at(1)};
    MacroDefinition definition;definition.id="definition";definition.label="Slider chain";definition.latest_revision=2;
    definition.revisions={{1,initial},{2,revision}};
    auto second_source=source;second_source.id="second-source";
    auto second_offset=offset;second_offset.id="second-offset";
    auto second_repeater=repeater;second_repeater.id="second-repeater";second_repeater.parameters.at("copies").literal=9;
    session.apply({CreatePrimitive{"composition","","built","Built",source},AddOperation{"built",offset,1},AddOperation{"built",repeater,2},
        CreatePrimitive{"composition","","macro","Macro",macro_source},MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"macro","definition","instance",2,1}},
        CreatePrimitive{"composition","","second","Second",second_source},AddOperation{"second",second_offset,1},
        AddOperation{"second",second_repeater,2}},0);
    return session.document();
}
inline QByteArray reference(const Ref& ref){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
    {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);}
inline QByteArray references(const std::vector<Ref>& refs){QJsonArray array;
    for(const auto& ref:refs)array.push_back(QJsonDocument::fromJson(reference(ref)).object());
    return QJsonDocument(array).toJson(QJsonDocument::Compact);}
template<class T>inline T* control(Window& window,const Ref& ref){
    for(auto* widget:window.findChildren<T*>())
        if(widget->isVisible()&&widget->property("nect-reference").toByteArray()==reference(ref))return widget;
    throw std::runtime_error("Visible production semantic control with exact Ref is missing");
}
inline QSlider* slider(Window& window,const Ref& ref){return control<QSlider>(window,ref);}
inline QLineEdit* input(Window& window,const Ref& ref){return control<QLineEdit>(window,ref);}
inline double value(const Document& document,const Ref& ref){return ref.point.empty()?evaluate(document).at(ref):
    macro_parameter_value(document,ref.object,ref.point,ref.field);}
inline Command edit_command(const Ref& ref,double next){if(ref.point.empty())return EditProperties{{ref},next,false};
    return MacroCommand{SetMacroOverride{ref.object,ref.point,ref.field,next}};}
inline void load(Window& window,const Id& selected="built"){
    window.host.session=Session(fixture());window.host.session_id+="-slider";window.host.edited();
    window.canvas->set_active_artboard("composition","artboard",false);window.canvas->set_selection(selected);events();
}
inline void focus(Window& window,QWidget* widget){
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");require(scroll!=nullptr,"Production Inspector scroll exists");
    scroll->ensureWidgetVisible(widget);widget->setFocus(Qt::OtherFocusReason);events();
}
inline QPoint handle(QSlider* slider){
    QStyleOptionSlider option;option.initFrom(slider);option.orientation=slider->orientation();
    option.minimum=slider->minimum();option.maximum=slider->maximum();option.sliderPosition=slider->sliderPosition();
    option.sliderValue=slider->value();option.singleStep=slider->singleStep();option.pageStep=slider->pageStep();
    option.upsideDown=slider->invertedAppearance();
    return slider->style()->subControlRect(QStyle::CC_Slider,&option,QStyle::SC_SliderHandle,slider).center();
}
inline void mouse(QSlider* slider,QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons buttons){
    QMouseEvent event(type,QPointF(point),QPointF(slider->mapToGlobal(point)),button,buttons,Qt::NoModifier);
    QApplication::sendEvent(slider,&event);events();
}
inline void begin(Window& window,QSlider* slider){focus(window,slider);
    mouse(slider,QEvent::MouseButtonPress,handle(slider),Qt::LeftButton,Qt::LeftButton);}
inline void preview(QSlider* slider,int delta){
    // Set the real Qt handle position while pressed: deterministic delta ticks
    // even when 1001 positions outnumber the Inspector's physical pixels.
    require(slider->isSliderDown(),"Real production slider handle is pressed");slider->setSliderPosition(delta);events();
}
inline void commit(QSlider* slider){mouse(slider,QEvent::MouseButtonRelease,handle(slider),Qt::LeftButton,Qt::NoButton);}
inline void edit(Window& window,const Ref& ref,const QString& text){auto* field=input(window,ref);focus(window,field);
    field->selectAll();QTest::keyClick(field,Qt::Key_Backspace);QTest::keyClicks(field,text);QTest::keyClick(field,Qt::Key_Return);events();}
inline void history_action(Window& window,const char* label){
    for(auto* action:window.findChildren<QAction*>())if(action->text()==QString::fromLatin1(label)){
        require(action->isEnabled(),"Production history action is enabled");action->trigger();events();return;}
    throw std::runtime_error("Production history action is missing");
}
inline void cold_readback(const Document& document,const Ref& ref,double expected){
    QTemporaryDir files;require(files.isValid(),"Owned cold native directory exists");
    Host writer(files.path()+"/writer");writer.session=Session(document);
    const auto path=files.path()+"/copies.nect";writer.save(path);writer.flush();Host cold(files.path()+"/cold");cold.open(path);
    require(cold.session.document()==document&&encode(cold.session.document())==encode(document)&&
        value(cold.session.document(),ref)==expected,"Cold native reopen preserves exact Copies, controls and every other authored field");
    const auto response=QJsonDocument::fromJson(QByteArray::fromStdString(request(cold.session,R"({"op":"properties"})"))).object();
    require(response.value("ok").toBool(),"Cold canonical properties API succeeds");bool found=false;
    for(const auto& entry:response.value("result").toArray()){
        const auto property=entry.toObject();
        if(QJsonDocument(property.value("ref").toObject()).toJson(QJsonDocument::Compact)!=reference(ref))continue;
        found=true;require(property.value("evaluated").toDouble()==expected,"Cold properties readback returns exact Copies at the same stable Ref");
    }
    require(found,"Cold properties readback retains the edited stable Ref");
}
inline void primary(Window& window,const Ref& ref){
    load(window,ref.object);auto& session=window.host.session;const Snapshot before(session);
    const auto descriptor=ref.point.empty()?*property_semantic_descriptor(session.document(),ref):macro_semantic_descriptor(session.document(),ref);
    require(descriptor.widget_hint=="slider"&&descriptor.minimum==0&&descriptor.maximum==1000&&descriptor.step==1&&
        validate_semantic_descriptor(descriptor).widget==SemanticWidget::slider,"Canonical built-in and published mapping select bounded step-one Copies sliders");
    auto* bar=slider(window,ref);auto* field=input(window,ref);
    require(dynamic_cast<SemanticSlider*>(bar)&&dynamic_cast<SemanticNumberInput*>(field)&&field->text()=="4"&&
        bar->objectName()=="semantic-slider-"+QString::fromStdString(ref.point.empty()?ref.field:ref.point)&&
        bar->property("nect-widget-hint")=="slider"&&field->property("nect-widget-hint")=="slider"&&
        bar->property("nect-targets").toByteArray()==references({ref})&&
        bar->minimum()==-4&&bar->maximum()==996&&bar->value()==0&&bar->singleStep()==1,
        "Production Inspector pairs an exact field with the shared real delta slider and stable target metadata");
    begin(window,bar);preview(bar,2);
    require(session.gesture_active()&&before.unchanged(session)&&value(session.preview_document(),ref)==6&&input(window,ref)->text()=="6"&&
        window.canvas->evaluated_values()==evaluate(session.preview_document())&&
        evaluate_shape(session.preview_document(),ref.object,evaluate(session.preview_document())).paths.size()==6,
        "Slider preview changes shared Session geometry and Canvas projection without authored or History mutation");
    QTest::keyClick(bar,Qt::Key_Escape);events();
    require(!session.gesture_active()&&before.unchanged(session)&&input(window,ref)->text()=="4"&&bar->value()==0&&
        window.canvas->evaluated_values()==evaluate(before.document),"Escape restores authored presentation and Canvas without creating an override or Undo");
    bar=slider(window,ref);begin(window,bar);preview(bar,3);commit(bar);
    Session expected(before.document);expected.apply({edit_command(ref,7)},0);
    require(!session.gesture_active()&&session.document()==expected.document()&&
        session.history().states.size()==before.history.states.size()+1&&
        session.history().states.back().label==expected.history().states.back().label&&
        session.revision()==before.revision+1,"Slider release commits exactly one canonical transaction with no hidden value");
    require(value(session.document(),amount)==exact_amount&&value(session.document(),rotation)==exact_rotation&&
        value(session.document(),mapped_amount)==exact_amount&&value(session.document(),mapped_rotation)==exact_rotation,
        "Copies editing leaves independent built-in and mapped Amount and Rotation exact");
    const auto committed=session.document();history_action(window,"Undo");
    require(session.document()==before.document&&!session.can_undo(),"One production Undo restores exact source and Macro override absence");
    history_action(window,"Redo");require(session.document()==committed&&!session.can_redo(),"One production Redo restores the exact slider result");
    cold_readback(committed,ref,7);
    const Snapshot direct(session);edit(window,ref,"7");require(direct.unchanged(session),"Exact direct re-entry is history-free");
    for(const auto* invalid:{"-1","1001","7.5"}){edit(window,ref,QString::fromLatin1(invalid));
        require(direct.unchanged(session)&&window.statusBar()->currentMessage().contains("OUT_OF_RANGE"),
            "Direct numeric entry rejects out-of-bounds and fractional Copies atomically through canonical validation");}
    edit(window,ref,"42");Session exact(direct.document);exact.apply({edit_command(ref,42)},0);
    require(session.document()==exact.document()&&input(window,ref)->text()=="42", "Direct exact integer entry authors the same canonical Copies value");
    bar=slider(window,ref);require(bar->value()==0&&bar->minimum()==-42&&bar->maximum()==958,"Direct entry rebuilds slider delta range from its authored baseline");
    begin(window,bar);preview(bar,bar->minimum());commit(bar);
    require(value(session.document(),ref)==0&&input(window,ref)->text()=="0","Actual slider reaches inclusive zero Copies");
    bar=slider(window,ref);require(bar->minimum()==0&&bar->maximum()==1000&&bar->value()==0,"Zero baseline retains the inclusive full delta range");
    begin(window,bar);preview(bar,bar->maximum());commit(bar);
    require(value(session.document(),ref)==1000&&input(window,ref)->text()=="1000","Actual slider reaches inclusive 1000 Copies without scaling or rounding");
    bar=slider(window,ref);require(bar->minimum()==-1000&&bar->maximum()==0&&bar->value()==0,"Upper-bound baseline cannot move past 1000");
    cold_readback(session.document(),ref,1000);
}
inline void mixed_and_refusal(Window& window){
    load(window);auto& session=window.host.session;
    window.canvas->set_selections({{"built",""},{"second",""}});events();const Snapshot mixed(session);
    auto* bar=slider(window,copies);auto* field=input(window,copies);
    require(field->text().isEmpty()&&field->placeholderText()=="Mixed"&&bar->property("nect-mixed").toBool()&&
        bar->property("nect-targets").toByteArray()==references({copies,second_copies})&&bar->minimum()==-4&&bar->maximum()==991,
        "Multi-target Copies slider intersects delta bounds while preserving mixed presentation and exact targets");
    begin(window,bar);preview(bar,3);commit(bar);Session expected(mixed.document);
    expected.apply({edit_command(copies,7),edit_command(second_copies,12)},0);
    require(session.document()==expected.document()&&session.history().states.size()==mixed.history.states.size()+1&&
        input(window,copies)->text().isEmpty(),"One mixed slider gesture preserves per-target differences in one canonical Undo");
    history_action(window,"Undo");require(session.document()==mixed.document,"One mixed slider Undo restores both values exactly");
    load(window);bar=slider(window,copies);
    session.apply({Rename{"built","Changed without Inspector refresh"}},session.revision());const Snapshot stale(session);
    begin(window,bar);
    require(stale.unchanged(session)&&!session.gesture_active()&&window.statusBar()->currentMessage().contains("REVISION_CONFLICT"),
        "Retained revision-stale slider refuses atomically before beginning a gesture");
    load(window);session.apply({LinkProperties{{copies},second_copies,false}},session.revision());window.host.edited();events();
    const Snapshot driven(session);bar=slider(window,copies);field=input(window,copies);
    require(field->isReadOnly()&&!bar->isEnabled(),"Driven Copies exact field is read-only and slider is disabled");
    begin(window,bar);require(driven.unchanged(session)&&!session.gesture_active(),"Disabled driven slider cannot silently replace its link");
}
inline int run(){
    checks=0;QTemporaryDir files;require(files.isValid(),"Owned Window test directory exists");
    QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
    Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1400,900);window.show();events();
    primary(window,copies);primary(window,mapped_copies);mixed_and_refusal(window);
    window.host.changed={};window.hide();return checks;
}
}
