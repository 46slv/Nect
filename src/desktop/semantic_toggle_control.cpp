#include "semantic_toggle_control.hpp"
#include "semantic_control.hpp"

namespace nect::desktop {

QCheckBox* semantic_toggle_input(const SemanticParameterDescriptor& descriptor,bool value,QWidget* parent) {
    // Resolve and reject an incompatible family before creating any QWidget.
    const auto resolution=validate_semantic_descriptor(descriptor);
    if(resolution.widget!=SemanticWidget::toggle)
        throw Error("INVALID_CONTROL_DESCRIPTOR","A toggle control requires a known boolean descriptor");

    auto* input=new QCheckBox(QString::fromStdString(descriptor.label),parent);
    input->setTristate(false);
    input->setChecked(value);
    input->setFocusPolicy(Qt::StrongFocus);
    annotate_semantic_control(input,descriptor);
    input->setAccessibleName(QString::fromStdString(descriptor.label));
    input->setAccessibleDescription(input->toolTip());
    return input;
}

}
