#include "host.hpp"
#include "composite_isolation_batch_control.hpp"
#include "multi_blend_mode_control.hpp"
#include "multi_fill_rule_control.hpp"
#include "multi_visibility_control.hpp"
#include "operation_enabled_batch_control.hpp"
#include "paint_color_batch_control.hpp"
#include "text_alignment_batch_control.hpp"
#include "text_content_batch_control.hpp"
#include "text_direction_batch_control.hpp"
#include "text_family_batch_control.hpp"
#include "text_italic_batch_control.hpp"
#include "text_layout_batch_control.hpp"
#include "text_locale_batch_control.hpp"
#include "text_string_source_batch_control.hpp"
#include "visual_style.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
constexpr int usable_width=250;
const std::vector<Id> pair={"first","second"};
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);++checks;}
template<class T>T* control(QWidget* panel,const char* name){
    auto* result=panel->findChild<T*>(QString::fromLatin1(name));
    check(result!=nullptr,std::string("Missing control: ")+name);return result;
}
Document fixture(){
    auto document=empty_document("batch-width-document","composition","artboard");
    for(const auto& id:std::vector<Id>{"source","first","second"}){
        Object object;object.id=id;object.name=id;object.kind=Kind::text;
        object.text=default_text(id+"-text",id+" literal");
        object.stack={default_operation(id+"-fill","nect.paint.fill"),
            default_operation(id+"-offset","nect.shape.offset")};
        if(id=="source")object.name="A source with a deliberately long descriptive label that must not widen the Inspector";
        if(id=="second"){
            object.visible=false;object.compositing.blend="multiply";object.compositing.isolated=true;
            object.text->italic=true;object.text->direction="vertical";object.text->layout="frame";
            object.text->alignment="end";object.text->locale="ja-JP";
            object.stack.back().enabled=false;
        }
        document.compositions.front().roots.push_back(id);document.objects.emplace(id,std::move(object));
    }
    return document;
}
void settle(){QApplication::processEvents();QTest::qWait(1);QApplication::processEvents();}

// A real widget-resizable scroll viewport exercises the same minimum-width
// negotiation as the Inspector. The 250px budget is the usable inner layout,
// excluding the group frame and layout margins. Hiding a scrollbar would not
// pass: its range, input geometry and complete button geometry are all checked.
struct NarrowPanel {
    QScrollArea scroll;QWidget* panel;
    explicit NarrowPanel(QWidget* value):panel(value){
        scroll.setFrameShape(QFrame::NoFrame);scroll.setWidgetResizable(true);
        scroll.setWidget(panel);scroll.setFixedSize(320,500);scroll.show();settle();
        const auto overhead=scroll.viewport()->width()-panel->layout()->contentsRect().width();
        scroll.setFixedWidth(usable_width+overhead);settle();
    }
    void verify(const std::vector<const char*>& names){
        settle();const auto label=panel->objectName().toStdString();
        const auto usable=panel->layout()->contentsRect();
        check(usable.width()==usable_width,label+": exactly 250px of usable content (actual "+std::to_string(usable.width())+")");
        check(scroll.horizontalScrollBar()->maximum()==0,label+": no horizontal overflow");
        for(const auto* name:names){
            auto* widget=control<QWidget>(panel,name);
            check(widget->isVisible(),std::string(name)+": visible");
            const QRect local(widget->mapTo(panel,QPoint{}),widget->size());
            check(local.left()>=usable.left()&&local.right()<=usable.right(),
                std::string(name)+": complete geometry fits the usable row");
            const QRect shown(widget->mapTo(scroll.viewport(),QPoint{}),widget->size());
            check(scroll.viewport()->rect().contains(shown),std::string(name)+": reachable without scrolling");
            if(auto* button=qobject_cast<QPushButton*>(widget)){
                check(button->width()>=button->minimumSizeHint().width(),std::string(name)+": action is not compressed");
                check(!button->text().isEmpty(),std::string(name)+": action label retained");
            }
        }
        for(auto* label_widget:panel->findChildren<QLabel*>())
            check(label_widget->wordWrap(),label+": explanation and status labels remain word-wrapped");
    }
    void verify_input_above_actions(const char* input,const char* apply,const char* cancel){
        verify({input,apply,cancel});
        auto* editor=control<QWidget>(panel,input);auto* action=control<QPushButton>(panel,apply);
        auto* discard=control<QPushButton>(panel,cancel);
        check(editor->geometry().bottom()<action->geometry().top(),panel->objectName().toStdString()+": input has its own row");
        check(action->geometry().top()==discard->geometry().top(),panel->objectName().toStdString()+": actions share a compact row");
    }
};
struct Context {
    QTemporaryDir directory;Host host;
    Context():host(directory.path()){
        check(directory.isValid(),"Owned scratch is available");host.session=Session(fixture());
    }
};
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;
    explicit Snapshot(const Session& session):document(session.document()),revision(session.revision()),history(session.history()){}
    bool unchanged(const Session& session)const{
        return document==session.document()&&revision==session.revision()&&history==session.history();
    }
};
void click(QPushButton* button){
    check(button->isEnabled(),button->objectName().toStdString()+": action is enabled");
    QTest::mouseClick(button,Qt::LeftButton,Qt::NoModifier,button->rect().center());settle();
}
void literal_content(){
    Context context;auto& session=context.host.session;const Snapshot original(session);
    NarrowPanel view(make_text_content_batch_controls(context.host,pair));
    auto* editor=control<QPlainTextEdit>(view.panel,"text-content-literal-editor");
    auto* apply=control<QPushButton>(view.panel,"text-content-literal-apply");
    auto* cancel=control<QPushButton>(view.panel,"text-content-literal-cancel");
    check(control<QLabel>(view.panel,"text-content-literal-state")->text()=="Mixed","Literal fixture is Mixed");
    check(editor->maximumHeight()==140,"Literal editor retains its bounded height");
    const QString replacement="A readable multiline literal draft that wraps in a narrow Inspector.\nSecond line 日本語.";
    editor->setPlainText(replacement);
    view.verify_input_above_actions("text-content-literal-editor","text-content-literal-apply","text-content-literal-cancel");
    check(editor->width()==usable_width&&!editor->accessibleName().isEmpty(),"Literal input uses the whole readable row and retains its accessible label");
    click(cancel);check(original.unchanged(session)&&editor->toPlainText().isEmpty(),"Reachable Cancel discards Mixed content without an edit");
    editor->setPlainText(replacement);click(apply);
    auto expected=original.document;for(const auto& id:pair)expected.objects.at(id).text->content=replacement.toStdString();
    check(session.document()==expected&&session.revision()==original.revision+1&&
        session.history().states.size()==original.history.states.size()+1,"Reachable literal Apply preserves one atomic Session transition");
    session.undo(session.revision());check(session.document()==original.document,"One Undo restores both literal sources");
}
void mixed_boolean_and_enum(){
    {
        Context context;auto& session=context.host.session;const Snapshot original(session);
        NarrowPanel view(make_composite_isolation_batch_controls(context.host,pair));
        auto* editor=control<QCheckBox>(view.panel,"composite-isolation-batch-editor");
        auto* apply=control<QPushButton>(view.panel,"composite-isolation-batch-apply");
        auto* cancel=control<QPushButton>(view.panel,"composite-isolation-batch-cancel");
        check(editor->checkState()==Qt::PartiallyChecked,"Isolation fixture preserves Mixed presentation");
        editor->click();view.verify_input_above_actions("composite-isolation-batch-editor","composite-isolation-batch-apply","composite-isolation-batch-cancel");
        click(cancel);check(original.unchanged(session)&&editor->checkState()==Qt::PartiallyChecked,"Reachable Cancel restores Mixed isolation without authoring it");
        editor->click();click(apply);auto expected=original.document;
        for(const auto& id:pair)expected.objects.at(id).compositing.isolated=true;
        check(session.document()==expected&&session.revision()==original.revision+1&&
            session.history().states.size()==original.history.states.size()+1,"Reachable boolean Apply remains one complete Session batch");
        session.undo(session.revision());check(session.document()==original.document,"One Undo restores both isolation values");
    }
    {
        Context context;auto& session=context.host.session;const Snapshot original(session);
        NarrowPanel view(make_text_direction_batch_controls(context.host,pair));
        auto* editor=control<QComboBox>(view.panel,"text-direction-batch-editor");
        auto* apply=control<QPushButton>(view.panel,"text-direction-batch-apply");
        auto* cancel=control<QPushButton>(view.panel,"text-direction-batch-cancel");
        check(editor->currentData().toString().isEmpty(),"Direction fixture preserves Mixed presentation");
        editor->setCurrentIndex(editor->findData("vertical"));
        view.verify_input_above_actions("text-direction-batch-editor","text-direction-batch-apply","text-direction-batch-cancel");
        click(cancel);check(original.unchanged(session)&&editor->currentData().toString().isEmpty(),"Reachable enum Cancel restores Mixed without an edit");
        editor->setCurrentIndex(editor->findData("vertical"));click(apply);auto expected=original.document;
        for(const auto& id:pair)expected.objects.at(id).text->direction="vertical";
        check(session.document()==expected&&session.revision()==original.revision+1&&
            session.history().states.size()==original.history.states.size()+1,"Reachable enum Apply retains one atomic Session batch");
        session.undo(session.revision());check(session.document()==original.document,"One Undo restores both enum values");
    }
}
void other_packed_controls(){
    struct Case {std::function<QWidget*(Host&)> make;const char* input;const char* apply;const char* cancel;};
    const std::vector<Case> cases={
        {[](Host& h){return make_multi_visibility_controls(h,pair);},"multi-visibility-editor","multi-visibility-apply","multi-visibility-cancel"},
        {[](Host& h){return make_multi_blend_mode_controls(h,pair);},"multi-blend-mode-selector","multi-blend-mode-apply","multi-blend-mode-cancel"},
        {[](Host& h){return make_text_italic_batch_controls(h,pair);},"text-italic-batch-editor","text-italic-batch-apply","text-italic-batch-cancel"},
        {[](Host& h){return make_text_locale_batch_controls(h,pair);},"text-locale-batch-editor","text-locale-batch-apply","text-locale-batch-cancel"},
        {[](Host& h){return make_operation_enabled_batch_controls(h,{{"first","first-offset"},{"second","second-offset"}});},
            "operation-enabled-batch-editor","operation-enabled-batch-apply","operation-enabled-batch-cancel"}
    };
    for(const auto& entry:cases){Context context;NarrowPanel view(entry.make(context.host));
        view.verify_input_above_actions(entry.input,entry.apply,entry.cancel);}
    Context context;NarrowPanel family(make_text_family_batch_controls(context.host,pair));
    auto* editor=control<QComboBox>(family.panel,"text-family-batch-editor");
    editor->addItem("A deliberately very long installed font family name that cannot set the panel minimum width");
    family.verify_input_above_actions("text-family-batch-editor","text-family-batch-apply","text-family-batch-cancel");
    check(editor->width()==usable_width,"Font-family input keeps the whole row after a long font is added");
}
void source_controls_and_short_rows(){
    {
        Context context;NarrowPanel view(make_text_string_source_batch_controls(context.host,pair));
        auto* mode=control<QComboBox>(view.panel,"text-string-source-batch-mode");mode->setCurrentIndex(mode->findData("link"));
        auto* source=control<QComboBox>(view.panel,"text-string-source-batch-source");
        check(source->count()>0,"Text source fixture includes a long source label");
        view.verify({"text-string-source-batch-mode","text-string-source-batch-source","text-string-source-batch-apply","text-string-source-batch-cancel"});
        mode->setCurrentIndex(mode->findData("unlink"));
        view.verify({"text-string-source-batch-mode","text-string-source-batch-apply","text-string-source-batch-cancel"});
    }
    {
        Context context;NarrowPanel view(make_multi_fill_rule_controls(context.host,{{"first","first-fill"},{"second","second-fill"}}));
        auto* mode=control<QComboBox>(view.panel,"multi-fill-rule-mode");mode->setCurrentIndex(mode->findData("link"));
        check(control<QComboBox>(view.panel,"multi-fill-rule-source")->count()>0,"Fill source fixture includes a long source label");
        view.verify({"multi-fill-rule-mode","multi-fill-rule-source","multi-fill-rule-apply","multi-fill-rule-cancel"});
    }
    // These factories already have short rows or a single action; retain their layout.
    {
        Context context;NarrowPanel view(make_text_alignment_batch_controls(context.host,pair));
        view.verify({"text-alignment-batch-editor","text-alignment-batch-apply"});
    }
    {
        Context context;NarrowPanel view(make_text_layout_batch_controls(context.host,pair));
        view.verify({"text-layout-batch-editor","text-layout-batch-apply"});
    }
    {
        Context context;NarrowPanel view(make_paint_color_batch_controls(context.host,
            {operation_ref("first","first-fill","color"),operation_ref("second","second-fill","color")}));
        view.verify({"paint-color-batch-choose"});
    }
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication application(argc,argv);
    application.setStyleSheet(application_style_sheet());
    try{
        literal_content();mixed_boolean_and_enum();other_packed_controls();source_controls_and_short_rows();
        std::cout<<"PASS "<<checks<<" narrow batch Inspector Qt checks (physical OS input NOT_RUN)\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
