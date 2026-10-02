#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAccessible>
#include <QEvent>
#include <QHelpEvent>
#include <QEnterEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QScrollArea>
#include <QPushButton>
#include <QStatusBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolTip>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);++checks;}
void events() {QApplication::processEvents();}
QByteArray reference(const Ref& ref) {
    return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
        {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}})
        .toJson(QJsonDocument::Compact);
}
QByteArray references(const std::vector<Ref>& refs) {
    QJsonArray values;
    for(const auto& ref:refs)values.append(QJsonDocument::fromJson(reference(ref)).object());
    return QJsonDocument(values).toJson(QJsonDocument::Compact);
}
template<class T> T* for_ref(Window& window,const char* name,const Ref& ref) {
    const auto encoded=reference(ref);
    for(auto* item:window.findChildren<T*>(QString::fromLatin1(name)))
        if(item->isVisible()&&item->property("nect-reference").toByteArray()==encoded)return item;
    throw std::runtime_error(std::string("Visible ")+name+" has the exact property Ref");
}
QPushButton* expression_for_ref(Window& window,const Ref& ref) {
    auto* picker=for_ref<QPushButton>(window,"property-source-pick",ref);
    auto* expression=picker->parentWidget()->findChild<QPushButton*>("property-expression");
    if(!expression||!expression->isVisible())throw std::runtime_error("Visible expression editor is adjacent to the exact numeric Ref row");
    return expression;
}
QWidget* named(Window& window,const char* name) {
    for(auto* item:window.findChildren<QWidget*>(QString::fromLatin1(name)))if(item->isVisible())return item;
    throw std::runtime_error(std::string("Visible control missing: ")+name);
}
QWidget* batch_dial(Window& window,const std::vector<Ref>& refs) {
    const auto encoded=references(refs);
    for(auto* item:window.findChildren<QWidget*>())
        if(item->isVisible()&&item->objectName()=="batch-repeater-angle-knob"&&
           item->property("nect-targets").toByteArray()==encoded)return item;
    throw std::runtime_error("Visible Repeater batch angle dial uses the exact ordered target Refs");
}
QRect global_rect(QWidget* widget) {return QRect(widget->mapToGlobal(QPoint(0,0)),widget->size());}
void stable_feedback_transitions(QWidget* widget) {
    for(auto* parent=widget->parentWidget();parent;parent=parent->parentWidget())
        if(auto* scroll=qobject_cast<QScrollArea*>(parent)){scroll->ensureWidgetVisible(widget);events();break;}
    const bool was_enabled=widget->isEnabled();widget->setEnabled(true);events();
    const auto geometry=widget->geometry();
    const auto global=global_rect(widget);
    const auto parent_geometry=widget->parentWidget()->geometry();
    const QPoint center=widget->rect().center();
    QEnterEvent enter(QPointF(center),QPointF(center),QPointF(widget->mapToGlobal(center)));
    QApplication::sendEvent(widget,&enter);events();
    QHelpEvent help(QEvent::ToolTip,widget->rect().center(),widget->mapToGlobal(widget->rect().center()));
    QApplication::sendEvent(widget,&help);events();
    check(QToolTip::text()==widget->toolTip(),"A synthetic hover-help request displays the complete action/property tooltip");
    QToolTip::hideText();
    check(widget->geometry()==geometry&&global_rect(widget)==global&&widget->parentWidget()->geometry()==parent_geometry,
        "Synthetic pointer-enter and tooltip feedback leave control and parent hit geometry fixed");
    widget->setEnabled(false);events();
    check(widget->geometry()==geometry&&global_rect(widget)==global&&widget->parentWidget()->geometry()==parent_geometry,
        "Enabled-state feedback leaves control and parent hit geometry fixed");
    widget->setEnabled(true);widget->setFocus(Qt::OtherFocusReason);events();
    check(widget->hasFocus(),"Enabled compact control receives keyboard focus");
    check(widget->geometry()==geometry&&global_rect(widget)==global&&widget->parentWidget()->geometry()==parent_geometry,
        "Keyboard focus feedback leaves control and parent hit geometry fixed");
    auto* accessible=QAccessible::queryAccessibleInterface(widget);
    check(accessible&&accessible->text(QAccessible::Name)==widget->accessibleName(),
        "Keyboard accessibility exposes the same meaningful control name");
    widget->clearFocus();widget->setEnabled(was_enabled);
    QEvent leave(QEvent::Leave);QApplication::sendEvent(widget,&leave);events();
    check(widget->geometry()==geometry&&global_rect(widget)==global&&widget->parentWidget()->geometry()==parent_geometry,
        "Leaving hover restores feedback without shifting the hit target");
}
struct RectSnapshot {QString key;QRect geometry;QRect global;};
std::vector<RectSnapshot> compact_geometry(Window& window) {
    std::vector<RectSnapshot> out;
    for(auto* item:window.findChildren<QWidget*>())if(item->isVisible()) {
        const auto name=item->objectName();
        if(name=="property-source-pick"||name=="property-expression"||
           name.startsWith("operation-up-")||name.startsWith("operation-down-")||
           name.startsWith("operation-remove-")||name.startsWith("repeater-angle-knob-")||
           name=="primitive-angle-knob"||name.startsWith("point-angle-knob")||
           name=="batch-repeater-angle-knob")
            out.push_back({name+":"+item->toolTip()+":"+QString::fromLatin1(item->property("nect-reference").toByteArray())+
                ":"+QString::fromLatin1(item->property("nect-targets").toByteArray()),item->geometry(),global_rect(item)});
    }
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    return out;
}
void assert_persistent_state(Window& window,const std::string& bytes,std::uint64_t revision,const HistoryInfo& history) {
    check(encode(window.host.session.document())==bytes,"Hover, focus, enabled and rebuild feedback preserve native bytes");
    check(window.host.session.revision()==revision,"Feedback inspection preserves Session revision");
    check(window.host.session.history()==history,"Feedback inspection preserves the complete History state");
}
QWidget* deepest_child_at(QWidget* root,const QPoint& global) {
    QWidget* current=root;
    while(auto* child=current->childAt(current->mapFromGlobal(global)))current=child;
    return current;
}
void test_fx_remains_non_targetable(Window& window,QPushButton* picker,QPushButton* expression) {
    check(expression->property("nect-reference").toByteArray().isEmpty()&&
          expression->property("nect-targets").toByteArray().isEmpty()&&
          !expression->property("nect-pick-whip").toBool(),
          "Expression action retains no property-source hit metadata");
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    check(scroll!=nullptr,"Properties Inspector scroll viewport is available");
    scroll->ensureWidgetVisible(picker);events();
    const auto start=picker->mapToGlobal(picker->rect().center());
    const auto finish=expression->mapToGlobal(expression->rect().center());
    check(scroll->viewport()->rect().contains(scroll->viewport()->mapFromGlobal(start))&&
          scroll->viewport()->rect().contains(scroll->viewport()->mapFromGlobal(finish)),
          "Pick and fx controls are both inside the visible Inspector viewport");
    check(deepest_child_at(scroll->widget(),finish)==expression,
          "Inspector hit testing at the drop position resolves to the fx action itself");
    QMouseEvent press(QEvent::MouseButtonPress,picker->rect().center(),start,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(picker,&press);events();
    check(window.statusBar()->currentMessage().contains("Drag to a property"),"Source picker begins the normal property drag");
    QMouseEvent move(QEvent::MouseMove,expression->mapFromGlobal(finish),finish,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(expression,&move);events();
    QMouseEvent release(QEvent::MouseButtonRelease,expression->mapFromGlobal(finish),finish,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(expression,&release);events();
    check(window.statusBar()->currentMessage().startsWith("NO_SOURCE"),
        "A whip dropped on fx remains rejected as a non-property source");
}
void load(Window& window) {
    auto document=empty_document("feedback-doc","feedback-comp","feedback-art");Session setup(document);
    for(const auto* id:{"shape-a","shape-b"}) {
        const auto source=default_primitive(std::string("source-")+id,"nect.shape.polygon");
        setup.apply({CreatePrimitive{"feedback-comp","",id,id,source}},setup.revision());
        setup.apply({AddOperation{id,default_operation(std::string("offset-")+id,"nect.shape.offset"),0}},setup.revision());
        setup.apply({AddOperation{id,default_operation(std::string("repeater-")+id,"nect.shape.repeater"),1}},setup.revision());
    }
    window.host.session=Session(setup.document());window.host.session_id+="-feedback";window.host.edited();
    window.canvas->set_selection("shape-a");events();
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"Owned settings directory is available");
        QSettings settings(directory.filePath("settings.ini"),QSettings::IniFormat);
        Window window(directory.path(),std::make_unique<FolderLibrary>(settings));window.show();window.activateWindow();events();
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")check(window.devicePixelRatioF()>=1.9,"High-DPI run uses a real 2x widget scale");
        load(window);
        const Ref source_rotation{"shape-a","","generator.rotation"};
        const auto repeater_rotation=operation_ref("shape-a","repeater-shape-a","rotation");
        auto* source_pick=for_ref<QPushButton>(window,"property-source-pick",source_rotation);
        auto* source_fx=expression_for_ref(window,source_rotation);
        auto* repeater_pick=for_ref<QPushButton>(window,"property-source-pick",repeater_rotation);
        auto* repeater_fx=expression_for_ref(window,repeater_rotation);
        check(source_pick->accessibleName().contains("rotation",Qt::CaseInsensitive)&&
              source_pick->toolTip().contains("Pick source")&&source_pick->toolTip().contains("Rotation")&&
              source_pick->toolTip().contains("Drag")&&source_pick->toolTip().contains("hover Objects")&&
              source_pick->toolTip().contains("Click to search"),
              "Generator source picker names its property while preserving drag, hover and click instructions");
        check(source_fx->accessibleName().contains("expression editor",Qt::CaseInsensitive)&&
              source_fx->toolTip().contains("Rotation")&&source_fx->toolTip().contains("expression")&&
              source_fx->toolTip().contains("=prefix")&&source_fx->toolTip().contains("multiline draft"),
              "Generator expression control names its property and preserves the expression instructions");
        check(repeater_pick->accessibleName().contains("Rotation")&&repeater_pick->toolTip().contains("Pick source for")&&
              repeater_pick->toolTip().contains("Drag")&&repeater_pick->toolTip().contains("Click to search"),
              "Repeater source picker identifies its target and keeps the whip/search instructions");
        check(repeater_fx->accessibleName().contains("Rotation")&&repeater_fx->toolTip().contains("expression")&&
              repeater_fx->toolTip().contains("=prefix"),
              "Repeater expression control exposes property-specific hover and keyboard names");

        auto* offset_up=qobject_cast<QPushButton*>(named(window,"operation-up-offset-shape-a"));
        auto* offset_down=qobject_cast<QPushButton*>(named(window,"operation-down-offset-shape-a"));
        auto* repeater_up=qobject_cast<QPushButton*>(named(window,"operation-up-repeater-shape-a"));
        auto* repeater_down=qobject_cast<QPushButton*>(named(window,"operation-down-repeater-shape-a"));
        auto* repeater_remove=qobject_cast<QPushButton*>(named(window,"operation-remove-repeater-shape-a"));
        auto* stroke_down=qobject_cast<QPushButton*>(named(window,"operation-down-shape-a-stroke"));
        check(offset_up&&offset_down&&!offset_up->isEnabled()&&offset_down->isEnabled()&&
              offset_up->toolTip().contains("Offset")&&offset_up->toolTip().contains("earlier")&&
              offset_up->accessibleName()==offset_up->toolTip(),
              "Stack-up arrow names the Offset action and respects the lower boundary enabled state");
        check(repeater_up&&repeater_down&&repeater_up->isEnabled()&&repeater_down->isEnabled()&&
              repeater_down->toolTip().contains("Repeater")&&repeater_down->toolTip().contains("later")&&
              repeater_down->accessibleName()==repeater_down->toolTip(),
              "Stack-down arrow names the Repeater action and keeps the intermediate operation enabled");
        check(stroke_down&&!stroke_down->isEnabled()&&stroke_down->toolTip().contains("Stroke")&&
              stroke_down->accessibleName()==stroke_down->toolTip(),
              "Stack-down arrow at the top boundary remains disabled but still has a meaningful action name");
        check(repeater_remove&&repeater_remove->toolTip().contains("Remove Repeater from the stack")&&
              repeater_remove->accessibleName()==repeater_remove->toolTip(),
              "Removal control retains a complete operation name in hover and accessibility");

        auto* source_dial=named(window,"primitive-angle-knob");
        auto* single_dial=named(window,"repeater-angle-knob-repeater-shape-a");
        check(source_dial->accessibleName().contains("source rotation angle")&&
              source_dial->toolTip().contains("Polygon source rotation angle")&&source_dial->toolTip().contains("Drag adds signed degrees"),
              "Single primitive angle dial identifies its target and retains dial interaction guidance");
        check(single_dial->accessibleName().contains("Repeater rotation angle")&&
              single_dial->toolTip().contains("Repeater rotation angle")&&single_dial->toolTip().contains("modulo 360"),
              "Single Repeater angle dial identifies its own property and retains exact-value guidance");

        const auto initial_bytes=encode(window.host.session.document());
        const auto initial_revision=window.host.session.revision();
        const auto initial_history=window.host.session.history();
        test_fx_remains_non_targetable(window,source_pick,source_fx);
        assert_persistent_state(window,initial_bytes,initial_revision,initial_history);
        for(auto* widget:std::vector<QWidget*>{source_pick,source_fx,repeater_pick,repeater_fx,
                          offset_up,offset_down,repeater_up,repeater_down,repeater_remove,source_dial,single_dial})
            stable_feedback_transitions(widget);
        assert_persistent_state(window,initial_bytes,initial_revision,initial_history);
        const auto before_rebuild=compact_geometry(window);
        window.refresh(false);events();
        const auto after_rebuild=compact_geometry(window);
        check(before_rebuild.size()==after_rebuild.size(),"Representative Inspector rebuild retains the compact control census");
        bool rebuild_geometry_stable=before_rebuild.size()==after_rebuild.size();
        for(std::size_t i=0;i<std::min(before_rebuild.size(),after_rebuild.size());++i)
            rebuild_geometry_stable=rebuild_geometry_stable&&before_rebuild[i].key==after_rebuild[i].key&&
                before_rebuild[i].geometry==after_rebuild[i].geometry&&before_rebuild[i].global==after_rebuild[i].global;
        check(rebuild_geometry_stable,"Representative Inspector rebuild preserves compact controls’ global hit geometry");
        assert_persistent_state(window,initial_bytes,initial_revision,initial_history);

        const auto values=evaluate(window.host.session.document());
        const auto point_id=path_contours(window.host.session.document().objects.at("shape-a"),&values).front().points.front().id;
        window.canvas->set_selection("shape-a",point_id);events();
        auto* point_dial=named(window,"point-angle-knob-in.angle");
        check(point_dial->accessibleName().contains("shape-a",Qt::CaseInsensitive)&&
              point_dial->accessibleName().contains("incoming handle",Qt::CaseInsensitive)&&
              point_dial->toolTip().contains("incoming handle angle")&&point_dial->toolTip().contains("Escape cancels"),
              "Single point-angle dial identifies its selected point handle and keeps interaction instructions");
        stable_feedback_transitions(point_dial);
        assert_persistent_state(window,initial_bytes,initial_revision,initial_history);

        const auto point_values=evaluate(window.host.session.document());
        const auto point_b=path_contours(window.host.session.document().objects.at("shape-b"),&point_values).front().points.front().id;
        const std::vector<Ref> point_targets{{"shape-a",point_id,"in.angle"},{"shape-b",point_b,"in.angle"}};
        window.canvas->set_selections({{"shape-a",point_id},{"shape-b",point_b}});events();
        QWidget* point_batch=nullptr;
        for(auto* item:window.findChildren<QWidget*>())if(item->isVisible()&&
            item->objectName()=="batch-point-angle-knob-in.angle"&&item->property("nect-targets").toByteArray()==references(point_targets))
            point_batch=item;
        check(point_batch&&point_batch->accessibleName().contains("incoming handle angle")&&
              point_batch->accessibleName().contains("batch dial")&&point_batch->toolTip().contains("Adjust incoming handle angle")&&
              point_batch->toolTip().contains("2 points"),
              "Batch point-angle dial names its incoming-handle property and selected-point group");
        stable_feedback_transitions(point_batch);
        assert_persistent_state(window,initial_bytes,initial_revision,initial_history);

        window.canvas->set_selections({{"shape-a",""},{"shape-b",""}});events();
        const std::vector<Ref> primitive_targets{{"shape-a","","generator.rotation"},{"shape-b","","generator.rotation"}};
        QWidget* primitive_batch=nullptr;
        for(auto* item:window.findChildren<QWidget*>())if(item->isVisible()&&
            item->objectName()=="batch-primitive-angle-knob"&&item->property("nect-targets").toByteArray()==references(primitive_targets))
            primitive_batch=item;
        check(primitive_batch&&primitive_batch->accessibleName().contains("source rotation angle")&&
              primitive_batch->accessibleName().contains("batch dial")&&primitive_batch->toolTip().contains("Adjust source rotation angle")&&
              primitive_batch->toolTip().contains("2 objects"),
              "Batch primitive-angle dial names its generator property and selected-object group");
        stable_feedback_transitions(primitive_batch);
        assert_persistent_state(window,initial_bytes,initial_revision,initial_history);

        const std::vector<Ref> batch_targets{
            operation_ref("shape-a","repeater-shape-a","rotation"),
            operation_ref("shape-b","repeater-shape-b","rotation")};
        auto* batch=batch_dial(window,batch_targets);
        check(batch->accessibleName().contains("Repeater rotation angle")&&
              batch->accessibleName().contains("batch dial")&&batch->toolTip().contains("Adjust Repeater rotation angle")&&
              batch->toolTip().contains("2")&&batch->toolTip().contains("drag"),
              "Batch Repeater angle dial identifies the action and selected property group in hover and accessibility");
        stable_feedback_transitions(batch);
        assert_persistent_state(window,initial_bytes,initial_revision,initial_history);
        std::cout<<"PASS "<<checks<<" compact Inspector feedback checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
