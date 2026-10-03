#pragma once
#include "nect/core.hpp"

class QDialog;
class QWidget;

namespace nect::desktop {
class Host;

// Transient, read-only presentation of the canonical compatibility_plan API.
// The selected source is the committed Session, never a gesture preview.
// The caller may show/open the dialog; it deletes itself when closed.
QDialog* create_compatibility_plan_dialog(Host&,const Id& composition={},
    const Id& artboard={},QWidget* parent=nullptr);
}
