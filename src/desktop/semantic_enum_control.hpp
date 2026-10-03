#pragma once

#include "nect/semantic_controls.hpp"
#include <QComboBox>

namespace nect::desktop {

// Labels are presentation; Qt::UserRole retains the exact canonical choice ID.
// The caller owns Session commands and connects user-only QComboBox::activated.
QComboBox* semantic_enum_input(const SemanticParameterDescriptor&,const std::string& value,QWidget* parent=nullptr);

}
