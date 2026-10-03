#include "semantic_enum_control.hpp"

#include <QApplication>
#include <QTest>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
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
    SemanticParameterDescriptor d;
    d.key="test.enum";d.value_type="enum";d.unit="enum";d.domain="local_paths_and_paint";
    d.widget_hint="dropdown";d.label="Exact choice";d.help="Select a declared stable value.";
    d.choices={{"stable.value/zeta","First display choice"},{"stable.value/alpha","Second display choice"},
               {"stable.value/middle","Third display choice"}};
    d.enum_default=d.choices.front().value;
    return d;
}
void prepare(QComboBox& input) {
    input.resize(300,40);
    input.show();
    input.setFocus(Qt::OtherFocusReason);
    events();
}
void key(QComboBox& input,Qt::Key code) {
    QTest::keyClick(&input,code);
    events();
}
void refuses(const SemanticParameterDescriptor& d,const std::string& value,
             const std::string& expected_code="INVALID_CONTROL_DESCRIPTOR") {
    QWidget parent;
    const auto before=parent.children().size();
    try {
        auto* input=semantic_enum_input(d,value,&parent);
        (void)input;
    } catch(const Error& error) {
        check(error.code==expected_code,"Enum refusal has the expected typed error");
        check(parent.children().size()==before,"Enum refusal allocates no child widget");
        return;
    }
    throw std::runtime_error("Enum factory accepted an invalid descriptor or current value");
}

void construction_and_metadata() {
    const auto d=descriptor();
    const auto resolution=validate_semantic_descriptor(d);
    check(resolution.widget==SemanticWidget::dropdown&&!resolution.fallback&&resolution.status=="SUPPORTED",
          "Validated enum descriptor resolves the shared supported dropdown family");
    QWidget parent;
    auto* input=semantic_enum_input(d,d.choices[1].value,&parent);
    check(input->parentWidget()==&parent&&input->count()==static_cast<int>(d.choices.size()),
          "Dropdown follows Qt parent ownership and contains exactly the declared choices");
    check(!input->isEditable()&&input->focusPolicy()==Qt::StrongFocus,
          "Enum permits only declared choices and supports native keyboard focus");
    for(int i=0;i<input->count();++i) {
        check(input->itemText(i)==QString::fromStdString(d.choices[i].label),
              "Dropdown preserves the descriptor's visible labels in declared order");
        check(input->itemData(i,Qt::UserRole).toString()==QString::fromStdString(d.choices[i].value),
              "Each item retains the exact stable ID independently of display label and index");
    }
    check(input->currentIndex()==1&&input->currentData(Qt::UserRole).toString()==QString::fromStdString(d.choices[1].value)&&
          input->currentText()==QString::fromStdString(d.choices[1].label),
          "Supplied current stable ID selects its exact item rather than the descriptor default");
    check(input->accessibleName()==QString::fromStdString(d.label)&&
          input->toolTip()==QString::fromStdString(d.help)&&input->accessibleDescription()==input->toolTip(),
          "Visible help and accessible name and description come from the descriptor");
    check(input->property("nect-semantic-key").toString()==QString::fromStdString(d.key)&&
          input->property("nect-value-type").toString()=="enum"&&input->property("nect-unit").toString()=="enum"&&
          input->property("nect-domain").toString()==QString::fromStdString(d.domain)&&
          input->property("nect-widget-hint").toString()=="dropdown"&&
          input->property("nect-widget-kind").toString()=="dropdown"&&input->property("nect-exact-value").toBool(),
          "Factory uses shared annotation with exact enum identity and resolved widget family");
    check(input->property("nect-control-status").toString()=="SUPPORTED"&&
          !input->property("nect-control-fallback").toBool(),
          "Supported enum annotation has no fallback status");
    auto alias=d;alias.widget_hint="enum";
    std::unique_ptr<QComboBox> enum_hint(semantic_enum_input(alias,d.choices[1].value));
    check(enum_hint->property("nect-widget-hint").toString()=="enum"&&
          enum_hint->property("nect-widget-kind").toString()=="dropdown"&&!enum_hint->property("nect-control-fallback").toBool()&&
          enum_hint->currentData().toString()==QString::fromStdString(d.choices[1].value),
          "Known enum hint resolves the supported dropdown family without retargeting the current value");

    auto reordered=d;
    std::reverse(reordered.choices.begin(),reordered.choices.end());
    for(auto& choice:reordered.choices)choice.label="Renamed "+choice.label;
    std::unique_ptr<QComboBox> stable(semantic_enum_input(reordered,d.choices.front().value));
    check(stable->currentIndex()==2&&stable->currentData().toString()==QString::fromStdString(d.choices.front().value)&&
          stable->currentText()==QString::fromStdString(reordered.choices.back().label),
          "Reordering choices and renaming display labels cannot retarget the selected stable ID");
    auto localized=d;
    localized.choices[1].label=localized.choices[0].label;
    std::unique_ptr<QComboBox> duplicate_labels(semantic_enum_input(localized,localized.choices[1].value));
    check(duplicate_labels->itemText(0)==duplicate_labels->itemText(1)&&duplicate_labels->currentIndex()==1&&
          duplicate_labels->itemData(0).toString()!=duplicate_labels->itemData(1).toString()&&
          duplicate_labels->currentData().toString()==QString::fromStdString(localized.choices[1].value),
          "Duplicate localized display labels still select only the exact distinct stable ID");
    auto singleton=d;
    singleton.choices={d.choices[1]};singleton.enum_default=singleton.choices.front().value;
    std::unique_ptr<QComboBox> only(semantic_enum_input(singleton,singleton.choices.front().value));
    check(only->count()==1&&only->currentIndex()==0&&only->currentData().toString()==QString::fromStdString(*singleton.enum_default),
          "A one-choice enum still preserves its exact declared stable ID");
}

void builtin_metadata() {
    for(const auto* type:{"nect.paint.fill","nect.shape.offset"}) {
        const auto result=builtin_semantic_descriptor(type,"fill_rule");
        check(result.has_value(),"Fill and Offset fill rule have canonical enum metadata");
        const auto& d=*result;
        check(d.key=="fill_rule"&&d.value_type=="enum"&&d.unit=="enum"&&d.widget_hint=="dropdown"&&
              d.enum_default=="nonzero"&&d.choices.size()==2&&
              d.choices[0].value=="nonzero"&&d.choices[1].value=="evenodd",
              "Built-in fill rule declares exact nonzero/evenodd choices in canonical order and a nonzero default");
        const auto* owner=builtin_operation_type(type);
        check(owner&&d.domain==owner->input,"Enum domain comes from the canonical operation input owner");
        std::unique_ptr<QComboBox> input(semantic_enum_input(d,"evenodd"));
        check(input->currentIndex()==1&&input->currentData().toString()=="evenodd"&&
              input->currentText()==QString::fromStdString(d.choices[1].label),
              "Built-in current evenodd selects the exact canonical ID instead of its nonzero default");
    }
}

void invalid_descriptors_and_current_values() {
    const auto d=descriptor();
    const auto value=d.choices.front().value;
    refuses(*builtin_semantic_descriptor("nect.shape.offset","amount"),value);
    refuses(*builtin_semantic_descriptor("nect.shape.repeater","rotation"),value);
    refuses(*builtin_semantic_descriptor("nect.shape.repeater","copies"),value);
    auto invalid=d;
    invalid.value_type="text";refuses(invalid,value);
    invalid=d;invalid.unit="scalar";refuses(invalid,value);
    for(const auto* hint:{"numeric","angle","slider","toggle","color","point","vector","curve","range"}) {
        invalid=d;invalid.widget_hint=hint;refuses(invalid,value);
    }
    invalid=d;invalid.enum_default.reset();refuses(invalid,value);
    invalid=d;invalid.enum_default="missing.value";refuses(invalid,value);
    invalid=d;invalid.choices.clear();refuses(invalid,value);
    invalid=d;invalid.choices[0].value.clear();refuses(invalid,value);
    invalid=d;invalid.choices[1].value=invalid.choices[0].value;refuses(invalid,value);
    invalid=d;invalid.choices[0].label.clear();refuses(invalid,value);
    invalid=d;invalid.minimum=0;refuses(invalid,value);
    invalid=d;invalid.maximum=1;refuses(invalid,value);
    invalid=d;invalid.step=1;refuses(invalid,value);
    invalid=d;invalid.angle_semantics="signed_turns_indicator_modulo_360";refuses(invalid,value);
    invalid=d;invalid.key.clear();refuses(invalid,value);
    invalid=d;invalid.label.clear();refuses(invalid,value);
    invalid=d;invalid.unit.clear();refuses(invalid,value);
    invalid=d;invalid.domain.clear();refuses(invalid,value);
    invalid=d;invalid.widget_hint.clear();refuses(invalid,value);
    refuses(d,"missing.value","INVALID_CONTROL_VALUE");
    refuses(d,"","INVALID_CONTROL_VALUE");
    refuses(d,d.choices.front().label,"INVALID_CONTROL_VALUE");
    refuses(d,"1","INVALID_CONTROL_VALUE");
    refuses(d,"STABLE.VALUE/ZETA","INVALID_CONTROL_VALUE");
}

void unknown_hint_fallback() {
    auto future=descriptor();
    future.widget_hint="future-enum-control";
    const auto resolution=validate_semantic_descriptor(future);
    check(resolution.widget==SemanticWidget::dropdown&&resolution.fallback&&
          resolution.status.find("UNKNOWN_WIDGET_HINT")!=std::string::npos,
          "Unknown enum hint falls back only to the same-known-type dropdown family");
    std::unique_ptr<QComboBox> input(semantic_enum_input(future,future.choices.back().value));
    check(input->currentData().toString()==QString::fromStdString(future.choices.back().value)&&
          input->property("nect-widget-kind").toString()=="dropdown"&&input->property("nect-control-fallback").toBool(),
          "Fallback retains the supplied exact enum ID and exposes the resolved dropdown family");
    check(input->property("nect-widget-hint").toString()==QString::fromStdString(future.widget_hint)&&
          input->property("nect-control-status").toString()==QString::fromStdString(resolution.status)&&
          input->accessibleName()==QString::fromStdString(future.label)&&
          input->toolTip().contains(QString::fromStdString(future.help))&&
          input->toolTip().contains(QString::fromStdString(resolution.status))&&input->accessibleDescription()==input->toolTip(),
          "Fallback preserves original hint, labels and help and explains its status accessibly");
    future.value_type="unknown-enum-type";
    refuses(future,future.choices.front().value);
}

void native_user_input_and_external_refresh() {
    const auto d=descriptor();
    std::unique_ptr<QComboBox> input(semantic_enum_input(d,d.choices.front().value));
    std::vector<std::string> activated_values;
    int index_changes=0;
    QObject::connect(input.get(),&QComboBox::activated,input.get(),[&](int index) {
        activated_values.push_back(input->itemData(index,Qt::UserRole).toString().toStdString());
    });
    QObject::connect(input.get(),&QComboBox::currentIndexChanged,input.get(),[&](int) {++index_changes;});
    prepare(*input);
    input->clearFocus();events();input->setFocus(Qt::OtherFocusReason);events();
    check(activated_values.empty()&&index_changes==0,"Showing, focusing and blurring an enum never requests an edit");
    key(*input,Qt::Key_Down);
    check(input->currentIndex()==1&&activated_values==std::vector<std::string>{d.choices[1].value}&&index_changes==1,
          "Native Down activates one exact stable ID from its UserRole payload");
    key(*input,Qt::Key_Down);
    check(input->currentIndex()==2&&activated_values==std::vector<std::string>({d.choices[1].value,d.choices[2].value})&&index_changes==2,
          "Repeated native Down activates the next exact enum value once");
    key(*input,Qt::Key_Up);
    check(input->currentIndex()==1&&activated_values.back()==d.choices[1].value&&activated_values.size()==3&&index_changes==3,
          "Native Up reselects the earlier stable ID without any label or index conversion");

    const auto edits=activated_values.size();
    input->setCurrentIndex(0);events();
    check(input->currentData().toString()==QString::fromStdString(d.choices[0].value)&&
          activated_values.size()==edits&&index_changes==4,
          "External setCurrentIndex refresh emits state feedback without a user activation edit");
    input->setCurrentIndex(0);events();
    check(activated_values.size()==edits&&index_changes==4,
          "Repeating an identical programmatic refresh emits no additional edit or index change");
    input->setCurrentIndex(2);events();
    check(input->currentData().toString()==QString::fromStdString(d.choices[2].value)&&
          activated_values.size()==edits&&index_changes==5,
          "External refresh can select any exact declared ID without a user edit");

    input->setEnabled(false);
    key(*input,Qt::Key_Up);
    QTest::mouseClick(input.get(),Qt::LeftButton,Qt::NoModifier,input->rect().center());events();
    check(input->currentIndex()==2&&activated_values.size()==edits&&index_changes==5,
          "Disabled dropdown ignores native keyboard and mouse input without requesting an edit");
    input->setCurrentIndex(0);events();
    check(input->currentIndex()==0&&activated_values.size()==edits&&index_changes==6,
          "Disabled external refresh still displays the latest exact value without activation");
    input->setEnabled(true);input->setFocus(Qt::OtherFocusReason);events();
    key(*input,Qt::Key_Down);
    check(input->currentIndex()==1&&activated_values.size()==edits+1&&activated_values.back()==d.choices[1].value&&index_changes==7,
          "Reenabled native input resumes from the latest externally supplied value");
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        construction_and_metadata();
        builtin_metadata();
        invalid_descriptors_and_current_values();
        unknown_hint_fallback();
        native_user_input_and_external_refresh();
        std::cout<<"PASS "<<checks<<" semantic enum Qt checks (physical OS input NOT_RUN)\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';
        return 1;
    }
}
