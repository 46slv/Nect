#pragma once
#include "nect/core.hpp"
#include <QDialog>
#include <QPointer>
#include <QString>
#include <map>
#include <vector>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace nect::desktop {
class Host;

// A transient interface-v2 draft of an existing Definition's latest graph.
// IDs belong to controls, not labels or mappings. Save appends one revision;
// retained revisions and all existing instance pins/overrides stay unchanged.
class MacroPublicInterfaceDialog final : public QDialog {
public:
    explicit MacroPublicInterfaceDialog(Host&,const Id& definition={},QWidget* parent=nullptr);
    const Id& updated_definition_id() const { return updated_definition_id_; }
    std::uint64_t saved_revision() const { return saved_revision_; }
    void accept() override;
    void done(int result) override;
private:
    using Mapping=std::pair<Id,std::string>;
    QPointer<Host> host_;
    QString session_id_;
    Id document_id_,definition_id_,updated_definition_id_;
    std::uint64_t revision_=0,gesture_=0,saved_revision_=0;
    bool finished_=false,loaded_=false,presenting_=false;
    int selected_row_=-1;
    MacroDefinitionRevision draft_;
    std::vector<MacroPublicParameter> controls_;
    std::map<Mapping,QString> defaults_;
    QComboBox *definitions_=nullptr,*nodes_=nullptr,*parameters_=nullptr;
    QListWidget* list_=nullptr;
    QGroupBox* fields_=nullptr;
    QLineEdit *stable_id_=nullptr,*label_=nullptr,*default_=nullptr;
    QWidget* default_container_=nullptr;
    QLabel *source_=nullptr,*metadata_=nullptr,*error_=nullptr;
    QPushButton *add_=nullptr,*remove_=nullptr,*save_=nullptr;
    const Document& current_document() const;
    void load_definition();
    void rebuild_list(int selection);
    void show_row();
    void refresh_actions();
    void add_control();
    void remove_control();
    MacroDefinitionRevision edited_revision() const;
};
}
