#pragma once
#include "nect/core.hpp"
#include <QDialog>
#include <QPointer>
#include <QString>
#include <map>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;

namespace nect::desktop {
class Host;

// A local draft of an existing Macro's latest graph. The list is execution
// order; Save appends one graph-v2 revision through UpdateMacroDefinition.
// Retained nodes/ports and the published interface retain their identities.
class MacroChainDialog final : public QDialog {
public:
    explicit MacroChainDialog(Host&,const Id& definition={},QWidget* parent=nullptr);
    const Id& updated_definition_id() const { return updated_definition_id_; }
    std::uint64_t saved_revision() const { return saved_revision_; }
    void accept() override;
    void done(int result) override;
private:
    struct NodeDraft {
        MacroNode node;
        // Raw text survives selection/reorder, including incomplete drafts.
        std::map<std::string,QString> values;
    };
    QPointer<Host> host_;
    QString session_id_;
    Id document_id_,definition_id_,updated_definition_id_,selected_node_,mapped_node_;
    std::uint64_t revision_=0,gesture_=0,saved_revision_=0;
    bool finished_=false,loaded_=false;
    MacroDefinitionRevision draft_;
    MacroPublicParameter public_template_;
    std::vector<NodeDraft> nodes_;
    std::map<std::string,QLineEdit*> inputs_;
    QCheckBox* publish_amount_=nullptr;
    QCheckBox* node_enabled_=nullptr;
    QComboBox* definitions_=nullptr;
    QComboBox* mapping_=nullptr;
    QListWidget* chain_=nullptr;
    QScrollArea* defaults_=nullptr;
    QLineEdit* parameter_label_=nullptr;
    QLabel* source_=nullptr;
    QLabel* public_id_=nullptr;
    QLabel* error_=nullptr;
    QPushButton *save_=nullptr,*add_offset_=nullptr,*add_repeater_=nullptr;
    QPushButton *up_=nullptr,*down_=nullptr,*remove_=nullptr;
    const Document& current_document() const;
    void load_definition();
    void capture_defaults();
    void show_defaults();
    void rebuild_chain(const Id& selection);
    void refresh_actions();
    void add_node(const std::string& type);
    void move_node(int delta);
    void remove_node();
    MacroDefinitionRevision edited_revision();
};

// Global Window hook, independent of object/instance selection. Opens a
// nonmodal chooser; existing instance pins are never changed by this control.
QWidget* make_macro_chain_controls(Host&,QWidget* parent=nullptr);
}
