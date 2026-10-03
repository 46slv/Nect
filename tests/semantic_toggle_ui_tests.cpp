#include "semantic_toggle_control.hpp"

#include <QApplication>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTest>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace nect;
using namespace nect::desktop;

namespace {
int checks=0;
void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
    ++checks;
}
void events() {QApplication::processEvents();}
SemanticParameterDescriptor descriptor() {
    const auto result=builtin_semantic_descriptor("nect.shape.offset","enabled");
    check(result.has_value(),"Built-in enabled has a canonical semantic descriptor");
    return *result;
}
void prepare(QCheckBox& input) {
    input.resize(280,40);
    input.show();
    input.setFocus(Qt::OtherFocusReason);
    events();
}
QPoint indicator(const QCheckBox& input) {
    QStyleOptionButton option;
    option.initFrom(&input);
    option.text=input.text();
    return input.style()->subElementRect(QStyle::SE_CheckBoxIndicator,&option,&input).center();
}
void click(QCheckBox& input) {
    QTest::mouseClick(&input,Qt::LeftButton,Qt::NoModifier,indicator(input));
    events();
}
void space(QCheckBox& input) {
    QTest::keyClick(&input,Qt::Key_Space);
    events();
}
void refuses(const SemanticParameterDescriptor& invalid) {
    QWidget parent;
    const auto before=parent.children().size();
    try {
        auto* input=semantic_toggle_input(invalid,false,&parent);
        (void)input;
    } catch(const Error& error) {
        check(error.code=="INVALID_CONTROL_DESCRIPTOR","Descriptor mismatch has a typed refusal");
        check(parent.children().size()==before,"Descriptor mismatch allocates no child widget");
        return;
    }
    throw std::runtime_error("Toggle factory accepted an incompatible descriptor");
}

void construction_and_metadata() {
    const auto d=descriptor();
    check(d.value_type=="boolean"&&d.unit=="boolean"&&d.widget_hint=="toggle"&&d.boolean_default==true,
          "Built-in enabled describes a typed boolean toggle with an exact default");
    check(d.domain==builtin_operation_type("nect.shape.offset")->input,
          "Toggle domain comes from its canonical built-in owner");
    check(validate_semantic_descriptor(d).widget==SemanticWidget::toggle,
          "Validated boolean metadata resolves the shared toggle family");
    QWidget parent;
    auto* disabled_value=semantic_toggle_input(d,false,&parent);
    auto* enabled_value=semantic_toggle_input(d,true,&parent);
    check(disabled_value->parentWidget()==&parent&&enabled_value->parentWidget()==&parent,
          "Toggle follows ordinary Qt parent ownership");
    check(!disabled_value->isChecked()&&enabled_value->isChecked(),
          "Both exact authored boolean values override the descriptor default");
    check(!disabled_value->isTristate()&&!enabled_value->isTristate()&&
          disabled_value->checkState()==Qt::Unchecked&&enabled_value->checkState()==Qt::Checked,
          "Scalar toggle has exactly two native checked states");
    check(enabled_value->text()==QString::fromStdString(d.label)&&
          enabled_value->accessibleName()==QString::fromStdString(d.label),
          "Visible and accessible labels come from the descriptor");
    check(enabled_value->toolTip()==QString::fromStdString(d.help)&&
          enabled_value->accessibleDescription()==QString::fromStdString(d.help),
          "Help remains available visually and to assistive technology");
    check(enabled_value->focusPolicy()==Qt::StrongFocus,
          "Toggle supports native keyboard focus");
    check(enabled_value->property("nect-semantic-key").toString()==QString::fromStdString(d.key)&&
          enabled_value->property("nect-value-type").toString()=="boolean"&&
          enabled_value->property("nect-unit").toString()=="boolean"&&
          enabled_value->property("nect-domain").toString()==QString::fromStdString(d.domain)&&
          enabled_value->property("nect-widget-hint").toString()=="toggle"&&
          enabled_value->property("nect-widget-kind").toString()=="toggle"&&
          enabled_value->property("nect-exact-value").toBool(),
          "Factory uses the shared semantic annotation with exact boolean identity");
    check(enabled_value->property("nect-control-status").toString()=="SUPPORTED"&&
          !enabled_value->property("nect-control-fallback").toBool(),
          "Canonical toggle exposes supported status without fallback");
    auto false_default=d;
    false_default.boolean_default=false;
    std::unique_ptr<QCheckBox> still_true(semantic_toggle_input(false_default,true));
    check(still_true->isChecked(),"A false default cannot replace a supplied true authored value");
}

void descriptor_mismatch_and_fallback() {
    const auto d=descriptor();
    refuses(*builtin_semantic_descriptor("nect.shape.offset","amount"));
    refuses(*builtin_semantic_descriptor("nect.shape.repeater","rotation"));
    refuses(*builtin_semantic_descriptor("nect.shape.repeater","copies"));
    auto invalid=d;
    invalid.value_type="text";refuses(invalid);
    invalid=d;invalid.unit="scalar";refuses(invalid);
    invalid=d;invalid.widget_hint="numeric";refuses(invalid);
    invalid=d;invalid.widget_hint="slider";refuses(invalid);
    invalid=d;invalid.widget_hint="angle";refuses(invalid);
    invalid=d;invalid.widget_hint="color";refuses(invalid);
    invalid=d;invalid.boolean_default.reset();refuses(invalid);
    invalid=d;invalid.minimum=0;refuses(invalid);
    invalid=d;invalid.maximum=1;refuses(invalid);
    invalid=d;invalid.step=1;refuses(invalid);
    invalid=d;invalid.angle_semantics="signed_turns_indicator_modulo_360";refuses(invalid);
    invalid=d;invalid.key.clear();refuses(invalid);
    invalid=d;invalid.label.clear();refuses(invalid);
    invalid=d;invalid.domain.clear();refuses(invalid);
    invalid=d;invalid.widget_hint.clear();refuses(invalid);
    auto future=d;
    future.widget_hint="future-boolean-control";
    const auto resolution=validate_semantic_descriptor(future);
    check(resolution.widget==SemanticWidget::toggle&&resolution.fallback&&
          resolution.status.find("UNKNOWN_WIDGET_HINT")!=std::string::npos,
          "Unknown boolean hint resolves only the same-known-type toggle fallback");
    std::unique_ptr<QCheckBox> input(semantic_toggle_input(future,false));
    check(!input->isChecked()&&input->property("nect-widget-kind").toString()=="toggle"&&
          input->property("nect-control-fallback").toBool(),
          "Fallback checkbox preserves exact false and exposes its resolved family");
    check(input->text()==QString::fromStdString(future.label)&&
          input->accessibleName()==QString::fromStdString(future.label)&&
          input->toolTip().contains(QString::fromStdString(future.help))&&
          input->toolTip().contains(QString::fromStdString(resolution.status))&&
          input->accessibleDescription()==input->toolTip(),
          "Fallback retains label and help and explains its status accessibly");
}

void native_user_input_and_external_refresh() {
    std::unique_ptr<QCheckBox> input(semantic_toggle_input(descriptor(),false));
    std::vector<bool> clicked_values;
    int toggles=0;
    QObject::connect(input.get(),&QCheckBox::clicked,input.get(),[&](bool value) {clicked_values.push_back(value);});
    QObject::connect(input.get(),&QCheckBox::toggled,input.get(),[&](bool) {++toggles;});
    prepare(*input);
    check(clicked_values.empty()&&toggles==0,"Showing and focusing a toggle does not request an edit");
    click(*input);
    check(input->isChecked()&&clicked_values==std::vector<bool>{true}&&toggles==1,
          "Native mouse click emits one exact true user edit");
    click(*input);
    check(!input->isChecked()&&clicked_values==std::vector<bool>({true,false})&&toggles==2,
          "Repeated mouse click emits one exact false user edit");
    space(*input);
    check(input->isChecked()&&clicked_values==std::vector<bool>({true,false,true})&&toggles==3,
          "Native Space activates the checked state once");
    space(*input);
    check(!input->isChecked()&&clicked_values==std::vector<bool>({true,false,true,false})&&toggles==4,
          "Repeated native Space emits the exact unchecked edit once");

    const auto edits=clicked_values.size();
    input->setChecked(true);
    events();
    check(input->isChecked()&&clicked_values.size()==edits&&toggles==5,
          "External checked refresh emits native state feedback without a clicked edit");
    input->setChecked(true);
    events();
    check(clicked_values.size()==edits&&toggles==5,"Repeated identical refresh emits no additional edit or state change");
    input->setChecked(false);
    events();
    check(!input->isChecked()&&clicked_values.size()==edits&&toggles==6,
          "External unchecked refresh preserves exact false without a user edit");

    input->setEnabled(false);
    click(*input);
    space(*input);
    input->click();
    events();
    check(!input->isChecked()&&clicked_values.size()==edits&&toggles==6,
          "Disabled native checkbox cannot request a mouse, Space or click edit");
    input->setChecked(true);
    events();
    check(input->isChecked()&&clicked_values.size()==edits&&toggles==7,
          "Disabled external refresh still displays exact state without editing");
    input->setEnabled(true);
    input->setFocus(Qt::OtherFocusReason);
    events();
    space(*input);
    check(!input->isChecked()&&clicked_values.size()==edits+1&&clicked_values.back()==false&&toggles==8,
          "Reenabled native input resumes from the latest external value");
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        construction_and_metadata();
        descriptor_mismatch_and_fallback();
        native_user_input_and_external_refresh();
        std::cout<<"PASS "<<checks<<" semantic toggle Qt checks (physical OS input NOT_RUN)\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';
        return 1;
    }
}
