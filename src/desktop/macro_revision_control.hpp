#pragma once
#include "nect/core.hpp"
#include <QDialog>
#include <QPointer>
#include <QString>
#include <map>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;

namespace nect::desktop {
class Host;

// A transient copy of an existing Definition's latest supported graph. Save
// appends exactly one canonical revision; existing instances stay pinned.
// An empty definition selects the first existing Definition in the chooser.
class MacroRevisionDialog final : public QDialog {
public:
    explicit MacroRevisionDialog(Host&,const Id& definition={},QWidget* parent=nullptr);
    const Id& updated_definition_id() const { return updated_definition_id_; }
    std::uint64_t saved_revision() const { return saved_revision_; }
    void accept() override;
    void done(int result) override;
private:
    struct NodeDraft {
        Id node;
        std::map<std::string,QLineEdit*> inputs;
    };
    QPointer<Host> host_;
    QString session_id_;
    Id document_id_,definition_id_,updated_definition_id_;
    std::uint64_t revision_=0,gesture_=0,saved_revision_=0;
    bool finished_=false,loaded_=false;
    MacroDefinitionRevision draft_;
    QComboBox* definitions_=nullptr;
    QScrollArea* scroll_=nullptr;
    QLineEdit* parameter_label_=nullptr;
    QLabel* error_=nullptr;
    QPushButton* save_=nullptr;
    std::vector<NodeDraft> nodes_;
    const Document& current_document() const;
    void load_definition();
    MacroDefinitionRevision edited_revision() const;
};

// Effects-panel hook for one selected Macro stack entry. Offers retained
// revision selection and explicit migration, delegating all override/refusal
// rules to UpdateMacroInstance. Never resets overrides or changes the source.
QWidget* make_macro_revision_controls(Host&,const Id& object,const Id& instance,QWidget* parent=nullptr);
}
