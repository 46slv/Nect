#pragma once
#include "nect/core.hpp"
#include <QDialog>
#include <QPointer>
#include <QString>
#include <map>
#include <vector>

class QLabel;
class QLineEdit;
class QPushButton;

namespace nect::desktop {
class Host;

// A transient authoring draft for the existing Macro v1 chain. Creating a
// definition is one canonical command; applying it to an object is separate.
// No graph/schema extensions, workspace publication, or instance edits occur.
class MacroAuthoringDialog final : public QDialog {
public:
    explicit MacroAuthoringDialog(Host&,QWidget* parent=nullptr);
    const Id& created_definition_id() const { return created_definition_id_; }
    void accept() override;
private:
    struct NodeDraft {
        ShapeOperation operation;
        std::map<std::string,QLineEdit*> inputs;
    };
    QPointer<Host> host_;
    QString session_id_;
    Id document_id_,definition_id_,created_definition_id_;
    std::uint64_t revision_=0;
    QLineEdit* name_=nullptr;
    QLineEdit* parameter_label_=nullptr;
    QLabel* error_=nullptr;
    QPushButton* create_=nullptr;
    std::vector<NodeDraft> nodes_;
    MacroDefinition definition() const;
};
}
