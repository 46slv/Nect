#include "window.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <iostream>

using namespace nect;
using namespace nect::desktop;

namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void pump() {QApplication::processEvents();}
const ShapeOperation& find_operation(const Document& document,const Id& object,const Id& operation) {
    const auto& stack=document.objects.at(object).stack;
    const auto found=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==operation;});
    check(found!=stack.end(),"Required Stroke operation missing");
    return *found;
}
QByteArray ref_json(const Ref& ref) {
    return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
        {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);
}
QAction* action(Window& window,const char* name) {
    auto* result=window.findChild<QAction*>(QString::fromLatin1(name));check(result!=nullptr,"Required production action missing");return result;
}
template<class T>T* visible(Window& window,const QString& name) {
    for(auto* widget:window.findChildren<T*>(name))if(widget->isVisible())return widget;
    throw std::runtime_error(("Required visible widget missing: "+name).toStdString());
}
template<class T>T* property_widget(Window& window,const Ref& ref) {
    const auto expected=ref_json(ref);
    for(auto* widget:window.findChildren<T*>())if(widget->isVisible()&&widget->property("nect-reference").toByteArray()==expected)return widget;
    throw std::runtime_error("Required property widget missing");
}
void select(Window& window,const Id& object) {
    window.canvas->set_selection(object);window.host.edited();pump();
}
Id add_curve(Window& window) {
    action(window,"add-curve")->trigger();pump();check(!window.canvas->selected_object.empty(),"Curve action selects its new object");
    return window.canvas->selected_object;
}
Id add_stroke(Window& window,const Id& object) {
    select(window,object);action(window,"add-stroke")->trigger();pump();
    const auto& stack=window.host.session.document().objects.at(object).stack;check(!stack.empty()&&stack.back().type=="nect.paint.stroke","Stroke action creates a Stroke operation");
    return stack.back().id;
}
void choose(Window& window,const char* prefix,const Id& operation,const char* value) {
    auto* combo=visible<QComboBox>(window,QString::fromLatin1(prefix)+QString::fromStdString(operation));
    const auto index=combo->findData(QString::fromLatin1(value));check(index>=0,"Requested Stroke option exists");combo->setCurrentIndex(index);pump();
}
void edit_number(Window& window,const Ref& ref,const char* text) {
    auto* input=property_widget<QLineEdit>(window,ref);input->setFocus();input->selectAll();QTest::keyClicks(input,text);QTest::keyClick(input,Qt::Key_Return);pump();
}
void toggle_matrix(Window& window) {
    auto* toggle=visible<QPushButton>(window,"transform-matrix-toggle");if(!toggle->isChecked()){toggle->click();pump();}
}
void history(Window& window,const char* label) {
    for(auto* candidate:window.findChildren<QAction*>())if(candidate->text()==QString::fromLatin1(label)) {
        check(candidate->isEnabled(),"Requested history action is enabled");candidate->trigger();pump();return;
    }
    throw std::runtime_error("Requested history action missing");
}
void promote(Window& window,const Id& operation) {
    auto* button=visible<QPushButton>(window,"stroke-enable-miter-"+QString::fromStdString(operation));button->click();pump();
}
void make_visible_style_contract(Window& window) {
    const auto target=add_curve(window),source=add_curve(window);const auto target_stroke=add_stroke(window,target),source_stroke=add_stroke(window,source);
    select(window,target);promote(window,target_stroke);select(window,source);promote(window,source_stroke);select(window,target);
    const auto target_miter=operation_ref(target,target_stroke,"miter_limit");const auto source_transform=Ref{source,"","transform.a"};
    select(window,source);toggle_matrix(window);edit_number(window,source_transform,"1");select(window,target);
    for(const auto limit:{"1","4","1000"}) {edit_number(window,target_miter,limit);check(evaluate(window.host.session.document()).at(target_miter)==QString::fromLatin1(limit).toDouble(),"Miter limit boundary edits through the Inspector");}
    window.host.session.apply({Link{target_miter,{source_transform,8,0,"copy_local_value"}}},window.host.session.revision());window.host.edited();pump();
    const auto linked=property(window.host.session.document(),target_miter);check(linked.binding&&linked.binding->source==source_transform&&linked.binding->scale==8,"Binding source and scale are authored on the real miter property");
    check(evaluate(window.host.session.document()).at(target_miter)==8,"Driven miter evaluates from its source");
    const std::vector<const char*> caps{"butt","round","square"},joins{"miter","round","bevel"};
    for(const auto cap:caps)for(const auto join:joins) {
        choose(window,"stroke-line-cap-",target_stroke,cap);choose(window,"stroke-line-join-",target_stroke,join);
        const auto& operation=find_operation(window.host.session.document(),target,target_stroke);
        check(operation.line_cap==cap&&operation.line_join==join,"Every cap/join branch reaches the selected Stroke");
        check(evaluate(window.host.session.document()).at(target_miter)==8&&property(window.host.session.document(),target_miter).binding==linked.binding,
            "Cap/join changes preserve the driven miter Binding and evaluated value");
    }
    const auto stale_revision=window.host.session.revision();auto* stale_join=visible<QComboBox>(window,"stroke-line-join-"+QString::fromStdString(target_stroke));
    window.host.session.apply({Set{source_transform,3}},stale_revision);check(evaluate(window.host.session.document()).at(target_miter)==24,"Changing the Binding source value updates the evaluated miter");stale_join->setCurrentIndex(stale_join->findData("round"));pump();
    check(window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT")&&find_operation(window.host.session.document(),target,target_stroke).line_join!="round","Stale join is rejected without a retarget");
    window.host.edited();pump();select(window,target);auto* stale_miter=property_widget<QLineEdit>(window,target_miter);const auto stale_miter_revision=window.host.session.revision();
    window.host.session.apply({Set{source_transform,4}},stale_miter_revision);check(evaluate(window.host.session.document()).at(target_miter)==32,"A second source value change updates the same driven miter");stale_miter->setFocus();stale_miter->selectAll();QTest::keyClicks(stale_miter,"6");QTest::keyClick(stale_miter,Qt::Key_Return);pump();
    check(window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT")&&property(window.host.session.document(),target_miter).binding.has_value(),"Stale miter edit is rejected atomically");
    window.host.edited();pump();select(window,target);window.host.session.begin_gesture(window.host.session.revision());
    auto* gesture_cap=visible<QComboBox>(window,"stroke-line-cap-"+QString::fromStdString(target_stroke));gesture_cap->setCurrentIndex(gesture_cap->findData("round"));pump();
    check(window.statusBar()->currentMessage().startsWith("GESTURE_ACTIVE")&&find_operation(window.host.session.document(),target,target_stroke).line_cap!="round","Active gesture rejects style mutation");
    window.host.session.cancel_gesture();window.host.edited();pump();select(window,target);choose(window,"stroke-line-cap-",target_stroke,"round");
    check(find_operation(window.host.session.document(),target,target_stroke).line_cap=="round","Style succeeds after session reacquire");
    const auto before_style=window.host.session.document();choose(window,"stroke-line-cap-",target_stroke,"square");const auto after_style=window.host.session.document();
    history(window,"Undo");check(window.host.session.document()==before_style,"UI Undo restores the prior Stroke style");history(window,"Redo");check(window.host.session.document()==after_style,"UI Redo restores the Stroke style");
    window.host.session.apply({Unlink{target_miter}},window.host.session.revision());window.host.edited();pump();select(window,target);edit_number(window,target_miter,"=4 + 4");
    const auto expression=property(window.host.session.document(),target_miter).expression;check(expression&&expression->source=="4 + 4"&&expression->version==1&&evaluate(window.host.session.document()).at(target_miter)==8,"Expression source/version and evaluation are authored exactly");
    const auto expression_document=window.host.session.document();const auto expression_revision=window.host.session.revision();edit_number(window,target_miter,"6");
    check(window.statusBar()->currentMessage().startsWith("DRIVEN_PROPERTY")&&window.host.session.revision()==expression_revision&&window.host.session.document()==expression_document,"Literal replacement of a driven miter is rejected");
    choose(window,"stroke-line-join-",target_stroke,"bevel");check(property(window.host.session.document(),target_miter).expression==expression,"Join edit preserves the Expression source");
    const auto native=window.host.recovery_directory()+"/stroke-ui.nect";window.host.save(native);window.host.recover();const auto recovery=window.host.persistence().value("recovery_file").toString();
    const auto saved_session=window.host.session_id;Host reopened(window.host.recovery_directory()+"/reopen");reopened.open(native);check(reopened.session.document()==window.host.session.document()&&reopened.session_id!=saved_session,"Native reopen preserves driven Stroke style and gets a fresh Session identity");
    Host recovered(window.host.recovery_directory()+"/recovered");recovered.open_recovery(recovery);check(recovered.session.document()==window.host.session.document()&&recovered.file_path.isEmpty()&&recovered.session_id!=saved_session,"Recovery reopen preserves driven Stroke style, detaches source and gets a fresh Session identity");
    history(window,"Undo");check(!property(window.host.session.document(),target_miter).expression,"Expression Undo removes only the latest source edit");history(window,"Redo");check(property(window.host.session.document(),target_miter).expression&&property(window.host.session.document(),target_miter).expression->source=="4 + 4","Expression Redo restores the exact source");
}
void identity_and_deleted_contract(Window& window) {
    const auto object=add_curve(window),stroke=add_stroke(window,object);select(window,object);auto old=QPointer<QComboBox>(visible<QComboBox>(window,"stroke-line-join-"+QString::fromStdString(stroke)));
    window.host.session.apply({DeleteObjects{{object}}},window.host.session.revision());window.host.edited();pump();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);pump();check(!old,"Deleted Stroke widget is not retained after object removal");
    const auto deleted_session=window.host.session_id;window.host.create_document();pump();check(window.host.session.revision()==0&&window.host.session_id!=deleted_session,"Reopened/new Session starts from its own revision and identity");
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir temp;check(temp.isValid(),"Allocate Stroke UI scratch");check(QDir().mkpath(temp.path()+"/recovery"),"Create Stroke UI persistence scratch");Window window(temp.path()+"/recovery");window.resize(1200,800);window.show();pump();
        make_visible_style_contract(window);identity_and_deleted_contract(window);
        std::cout<<"PASS Stroke UI Curve/cap-join/miter/stale-gesture/identity/Binding/Expression/native/recovery contract\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
