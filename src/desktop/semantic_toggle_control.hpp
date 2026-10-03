#pragma once

#include "nect/semantic_controls.hpp"
#include <QCheckBox>

namespace nect::desktop {

// Only validated boolean descriptors select this native, two-state control.
// The current value is supplied by the caller; Session owns authored state and
// history. Native clicked(bool) distinguishes activation from external
// setChecked() refreshes; the factory installs no edit handler or state model.
QCheckBox* semantic_toggle_input(const SemanticParameterDescriptor&,bool value,QWidget* parent=nullptr);

}
