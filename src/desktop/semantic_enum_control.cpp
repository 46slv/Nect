#include "semantic_enum_control.hpp"
#include "semantic_control.hpp"

#include <algorithm>
#include <iterator>

namespace nect::desktop {

QComboBox* semantic_enum_input(const SemanticParameterDescriptor& descriptor,const std::string& value,QWidget* parent) {
    // Reject metadata and a missing current ID before creating any QWidget.
    const auto resolution=validate_semantic_descriptor(descriptor);
    if(descriptor.value_type!="enum"||resolution.widget!=SemanticWidget::dropdown)
        throw Error("INVALID_CONTROL_DESCRIPTOR","An enum control requires a known enum dropdown descriptor");
    const auto selected=std::find_if(descriptor.choices.begin(),descriptor.choices.end(),
        [&](const auto& choice) {return choice.value==value;});
    if(selected==descriptor.choices.end())
        throw Error("INVALID_CONTROL_VALUE","The current enum value is not a declared choice ID");

    auto* input=new QComboBox(parent);
    input->setEditable(false);
    for(const auto& choice:descriptor.choices)
        input->addItem(QString::fromStdString(choice.label),QString::fromStdString(choice.value));
    input->setCurrentIndex(static_cast<int>(std::distance(descriptor.choices.begin(),selected)));
    input->setFocusPolicy(Qt::StrongFocus);
    annotate_semantic_control(input,descriptor);
    input->setAccessibleName(QString::fromStdString(descriptor.label));
    input->setAccessibleDescription(input->toolTip());
    return input;
}

}
