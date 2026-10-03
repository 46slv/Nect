#include "macro_public_interface_control.hpp"
#include "host.hpp"
#include "semantic_control.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace nect::desktop {
namespace {
QString text(const std::string& value) { return QString::fromStdString(value); }
QString number(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString error_text(const Error& error) { return text(error.code)+": "+QString::fromUtf8(error.what()); }
QString node_label(const MacroNode& node,std::size_t index) {
    return number(index+1)+". "+text(builtin_operation_type(node.operation.type)->label)+" · "+text(node.operation.id);
}
const MacroNode* mapped_node(const MacroDefinitionRevision& graph,const Id& id) {
    const auto found=std::find_if(graph.nodes.begin(),graph.nodes.end(),
        [&](const auto& node){return node.operation.id==id;});
    return found==graph.nodes.end()?nullptr:&*found;
}
std::vector<std::string> supported_parameters(const MacroNode& node) {
    std::vector<std::string> result;
    for(const auto& [key,value]:node.operation.parameters) {
        (void)value;
        if(builtin_semantic_descriptor(node.operation.type,key))result.push_back(key);
    }
    return result;
}
QString control_label(const MacroPublicParameter& control) {
    return text(control.label)+" · "+text(control.node)+" / "+text(control.parameter);
}
}

MacroPublicInterfaceDialog::MacroPublicInterfaceDialog(Host& host,const Id& definition,QWidget* parent):
    QDialog(parent),host_(&host),session_id_(host.session_id),document_id_(host.session.document().id),
    revision_(host.session.revision()),gesture_(host.session.gesture_generation()) {
    setObjectName("macro-public-interface-dialog");setWindowTitle("Edit Macro published controls");resize(650,700);
    auto* body=new QVBoxLayout(this);auto* identity=new QFormLayout;
    definitions_=new QComboBox(this);definitions_->setObjectName("macro-public-interface-definition");
    definitions_->setAccessibleName("Macro Definition");
    definitions_->setToolTip("Choosing another Definition discards this unsaved draft.");
    for(const auto& [id,value]:host.session.document().macro_definitions) {
        definitions_->addItem(text(value.label)+" · "+text(id),text(id));
        definitions_->setItemData(definitions_->count()-1,text(id),Qt::ToolTipRole);
    }
    if(!definition.empty())definitions_->setCurrentIndex(definitions_->findData(text(definition)));
    identity->addRow("Definition",definitions_);
    source_=new QLabel(this);source_->setObjectName("macro-public-interface-revisions");source_->setTextFormat(Qt::PlainText);
    identity->addRow("Revision",source_);body->addLayout(identity);
    auto* notice=new QLabel("Publish up to 16 controls: Offset Amount, Repeater Copies or Rotation. "
        "Each internal parameter can be published once. Stable IDs stay fixed when labels or mappings change. "
        "Save creates an interface-v2 revision; portable Macro v1 export cannot carry it. "
        "Existing instances keep their pins and overrides until explicitly updated. "
        "Removed or incompatible controls may prevent an instance update; no override is silently removed. "
        "Choosing another Definition discards this draft.",this);
    notice->setTextFormat(Qt::PlainText);notice->setWordWrap(true);body->addWidget(notice);
    list_=new QListWidget(this);list_->setObjectName("macro-public-interface-controls");
    list_->setAccessibleName("Published Macro controls");list_->setSelectionMode(QAbstractItemView::SingleSelection);
    body->addWidget(list_,1);
    auto* actions=new QHBoxLayout;
    add_=new QPushButton("Add published control",this);add_->setObjectName("macro-public-interface-add");
    add_->setAutoDefault(false);actions->addWidget(add_);
    remove_=new QPushButton("Remove selected control",this);remove_->setObjectName("macro-public-interface-remove");
    remove_->setAutoDefault(false);actions->addWidget(remove_);body->addLayout(actions);
    fields_=new QGroupBox("Selected published control",this);auto* form=new QFormLayout(fields_);
    stable_id_=new QLineEdit(fields_);stable_id_->setObjectName("macro-public-interface-id");
    stable_id_->setReadOnly(true);stable_id_->setAccessibleName("Stable PublicParamID");
    stable_id_->setToolTip("Generated once. Rename or remap preserves this ID.");form->addRow("Stable ID",stable_id_);
    label_=new QLineEdit(fields_);label_->setObjectName("macro-public-interface-label");
    label_->setAccessibleName("Published control label");label_->setToolTip("1–128 UTF-8 bytes.");form->addRow("Label",label_);
    nodes_=new QComboBox(fields_);nodes_->setObjectName("macro-public-interface-node");
    nodes_->setAccessibleName("Mapped internal node");form->addRow("Node",nodes_);
    parameters_=new QComboBox(fields_);parameters_->setObjectName("macro-public-interface-parameter");
    parameters_->setAccessibleName("Mapped internal parameter");form->addRow("Parameter",parameters_);
    metadata_=new QLabel(fields_);metadata_->setObjectName("macro-public-interface-metadata");
    metadata_->setTextFormat(Qt::PlainText);metadata_->setWordWrap(true);form->addRow("Type / unit / domain",metadata_);
    default_container_=new QWidget(fields_);auto* default_layout=new QHBoxLayout(default_container_);
    default_layout->setContentsMargins(0,0,0,0);form->addRow("Literal default",default_container_);body->addWidget(fields_);
    error_=new QLabel(this);error_->setObjectName("macro-public-interface-error");
    error_->setAccessibleName("Macro published controls result");error_->setTextFormat(Qt::PlainText);
    error_->setWordWrap(true);body->addWidget(error_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    save_=buttons->button(QDialogButtonBox::Save);save_->setText("Save new revision");
    save_->setObjectName("macro-public-interface-save");
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("macro-public-interface-cancel");body->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,this,&MacroPublicInterfaceDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(definitions_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){load_definition();});
    connect(list_,&QListWidget::currentRowChanged,this,[this](int row){selected_row_=row;show_row();refresh_actions();});
    connect(add_,&QPushButton::clicked,this,[this]{add_control();});
    connect(remove_,&QPushButton::clicked,this,[this]{remove_control();});
    connect(label_,&QLineEdit::textChanged,this,[this](const QString& value){
        if(presenting_||selected_row_<0||finished_)return;
        controls_.at(selected_row_).label=value.toStdString();
        list_->item(selected_row_)->setText(control_label(controls_.at(selected_row_)));error_->clear();
    });
    connect(nodes_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
        if(presenting_||selected_row_<0||finished_)return;
        auto& control=controls_.at(selected_row_);control.node=nodes_->currentData().toString().toStdString();
        if(const auto* node=mapped_node(draft_,control.node)) {
            const auto supported=supported_parameters(*node);
            if(std::find(supported.begin(),supported.end(),control.parameter)==supported.end())
                control.parameter=supported.empty()?std::string{}:supported.front();
        }
        list_->item(selected_row_)->setText(control_label(control));
        list_->item(selected_row_)->setToolTip(text(control.id)+"\n"+text(control.node)+" / "+text(control.parameter));
        show_row();refresh_actions();error_->clear();
    });
    connect(parameters_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
        if(presenting_||selected_row_<0||finished_)return;
        auto& control=controls_.at(selected_row_);control.parameter=parameters_->currentData().toString().toStdString();
        list_->item(selected_row_)->setText(control_label(control));
        list_->item(selected_row_)->setToolTip(text(control.id)+"\n"+text(control.node)+" / "+text(control.parameter));
        show_row();refresh_actions();error_->clear();
    });
    load_definition();
}

const Document& MacroPublicInterfaceDialog::current_document() const {
    if(!host_||host_->session_id!=session_id_||host_->session.document().id!=document_id_)
        throw Error("SESSION_CONFLICT","This Macro draft belongs to another document; reopen published controls");
    if(host_->session.revision()!=revision_)
        throw Error("REVISION_CONFLICT","The document changed; reopen published controls");
    if(host_->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
    if(host_->session.gesture_generation()!=gesture_)
        throw Error("REVISION_CONFLICT","The edit context changed; reopen published controls");
    return host_->session.document();
}

void MacroPublicInterfaceDialog::load_definition() {
    if(finished_)return;
    loaded_=false;controls_.clear();defaults_.clear();source_->clear();
    try {
        const auto& document=current_document();
        if(definitions_->currentIndex()<0)throw Error("MISSING_MACRO_DEFINITION","Choose an existing Macro Definition");
        definition_id_=definitions_->currentData().toString().toStdString();
        const auto found=document.macro_definitions.find(definition_id_);
        if(found==document.macro_definitions.end())throw Error("MISSING_MACRO_DEFINITION",definition_id_);
        validate_macro_definition(found->second);
        if(found->second.latest_revision==std::numeric_limits<std::uint64_t>::max())
            throw Error("MACRO_REVISION_LIMIT","Macro revision space exhausted");
        draft_=found->second.revisions.at(found->second.latest_revision);
        draft_.revision=found->second.latest_revision+1;draft_.interface_version=2;
        controls_=draft_.public_parameters;
        for(const auto* node:macro_execution_order(draft_))for(const auto& key:supported_parameters(*node))
            defaults_.emplace(Mapping{node->operation.id,key},QString::number(node->operation.parameters.at(key).literal,'g',17));
        source_->setText("Source "+number(found->second.latest_revision)+" → New "+number(draft_.revision)+" · interface v2");
        loaded_=true;error_->clear();
    } catch(const Error& error) { error_->setText(error_text(error)); }
      catch(const std::exception& error) { error_->setText(QString::fromUtf8(error.what())); }
    rebuild_list(controls_.empty()?-1:0);
}

void MacroPublicInterfaceDialog::rebuild_list(int selection) {
    const QSignalBlocker blocker(list_);list_->clear();
    for(const auto& control:controls_) {
        auto* item=new QListWidgetItem(control_label(control),list_);item->setData(Qt::UserRole,text(control.id));
        item->setToolTip(text(control.id)+"\n"+text(control.node)+" / "+text(control.parameter));
    }
    selected_row_=selection;list_->setCurrentRow(selection);show_row();refresh_actions();
}

void MacroPublicInterfaceDialog::show_row() {
    presenting_=true;stable_id_->clear();label_->clear();nodes_->clear();parameters_->clear();metadata_->clear();
    delete default_;default_=nullptr;
    if(loaded_&&selected_row_>=0&&static_cast<std::size_t>(selected_row_)<controls_.size()) {
        const auto& control=controls_.at(selected_row_);stable_id_->setText(text(control.id));label_->setText(text(control.label));
        const bool reserved=control.id=="macro.offset.amount";
        const auto ordered=macro_execution_order(draft_);
        for(std::size_t index=0;index<ordered.size();++index) {
            const auto& node=*ordered[index];
            if(reserved&&node.operation.type!="nect.shape.offset")continue;
            nodes_->addItem(node_label(node,index),text(node.operation.id));
            nodes_->setItemData(nodes_->count()-1,text(node.operation.id),Qt::ToolTipRole);
        }
        nodes_->setCurrentIndex(nodes_->findData(text(control.node)));
        if(const auto* node=mapped_node(draft_,control.node)) {
            for(const auto& key:supported_parameters(*node)) {
                if(reserved&&key!="amount")continue;
                const auto descriptor=*builtin_semantic_descriptor(node->operation.type,key);
                parameters_->addItem(text(descriptor.label)+" · "+text(key),text(key));
            }
            parameters_->setCurrentIndex(parameters_->findData(text(control.parameter)));
            if(const auto descriptor=builtin_semantic_descriptor(node->operation.type,control.parameter)) {
                metadata_->setText(text(descriptor->value_type)+" / "+text(descriptor->unit)+" / "+text(descriptor->domain));
                const Mapping mapping{control.node,control.parameter};
                default_=semantic_number_input(*descriptor,defaults_.at(mapping),default_container_);
                default_->setObjectName("macro-public-interface-default");
                default_->setAccessibleName("Mapped literal default: "+text(descriptor->label));
                default_->setProperty("nect-node-id",text(control.node));default_->setProperty("nect-parameter",text(control.parameter));
                default_->setToolTip(default_->toolTip()+"\nInternal node literal default. This is not a new public value or an instance override.");
                default_container_->layout()->addWidget(default_);
                connect(default_,&QLineEdit::textChanged,this,[this,mapping](const QString& value){
                    if(!presenting_&&!finished_){defaults_.at(mapping)=value;error_->clear();}
                });
            }
        }
        nodes_->setToolTip(reserved?"Reserved macro.offset.amount can only map to Offset Amount.":
            "Remapping keeps the stable public ID. Existing instance overrides are not migrated automatically.");
    }
    presenting_=false;fields_->setEnabled(loaded_&&!finished_&&selected_row_>=0);
}

void MacroPublicInterfaceDialog::refresh_actions() {
    const bool active=loaded_&&!finished_&&!saved_revision_;
    save_->setEnabled(active);list_->setEnabled(active);definitions_->setEnabled(!finished_);
    bool available=false;
    if(active)for(const auto& [mapping,value]:defaults_) {
        (void)value;
        if(std::none_of(controls_.begin(),controls_.end(),[&](const auto& control){
            return control.node==mapping.first&&control.parameter==mapping.second;
        })) { available=true;break; }
    }
    add_->setEnabled(active&&controls_.size()<16&&available);remove_->setEnabled(active&&selected_row_>=0);
}

void MacroPublicInterfaceDialog::add_control() {
    if(!loaded_||finished_||saved_revision_||controls_.size()>=16)return;
    try {
        const auto& definition=current_document().macro_definitions.at(definition_id_);
        for(const auto* node:macro_execution_order(draft_))for(const auto& key:supported_parameters(*node)) {
            if(std::any_of(controls_.begin(),controls_.end(),[&](const auto& control){
                return control.node==node->operation.id&&control.parameter==key;
            }))continue;
            const auto descriptor=*builtin_semantic_descriptor(node->operation.type,key);
            std::string id;
            // Never reuse any retained public ID for a newly-created control.
            const auto used=[&](const std::string& candidate) {
                for(const auto& [revision,graph]:definition.revisions) {
                    (void)revision;
                    for(const auto& control:graph.public_parameters)if(control.id==candidate)return true;
                }
                return std::any_of(controls_.begin(),controls_.end(),[&](const auto& control){return control.id==candidate;});
            };
            do { id="macro.control."+new_id(); } while(used(id));
            controls_.push_back({id,descriptor.label,node->operation.id,key,descriptor.value_type,descriptor.unit,descriptor.domain});
            error_->clear();rebuild_list(static_cast<int>(controls_.size()-1));return;
        }
    } catch(const Error& error) { error_->setText(error_text(error)); }
      catch(const std::exception& error) { error_->setText(QString::fromUtf8(error.what())); }
}

void MacroPublicInterfaceDialog::remove_control() {
    if(!loaded_||finished_||saved_revision_||selected_row_<0)return;
    const auto row=selected_row_;controls_.erase(controls_.begin()+row);
    error_->clear();rebuild_list(controls_.empty()?-1:std::min(row,static_cast<int>(controls_.size()-1)));
}

MacroDefinitionRevision MacroPublicInterfaceDialog::edited_revision() const {
    if(!loaded_)throw Error("MISSING_MACRO_DEFINITION","Choose an existing Macro Definition");
    auto result=draft_;result.public_parameters=controls_;
    std::set<Mapping> mappings;
    for(auto& control:result.public_parameters) {
        if(control.label.empty()||control.label.size()>128||text(control.label).trimmed().isEmpty())
            throw Error("INVALID_MACRO_INTERFACE","Published labels must be 1–128 UTF-8 bytes and not blank");
        const auto* node=mapped_node(result,control.node);
        if(!node)throw Error("INVALID_MACRO_MAPPING","Choose an existing internal node");
        const auto descriptor=builtin_semantic_descriptor(node->operation.type,control.parameter);
        if(!descriptor)throw Error("INVALID_MACRO_MAPPING","Choose Offset Amount or Repeater Copies/Rotation");
        if(!mappings.emplace(control.node,control.parameter).second)
            throw Error("INVALID_MACRO_MAPPING","An internal node parameter can be published only once: "+control.node+" / "+control.parameter);
        control.value_type=descriptor->value_type;control.unit=descriptor->unit;control.domain=descriptor->domain;
    }
    for(const auto& [mapping,raw]:defaults_) {
        bool valid=false;const auto value=raw.trimmed().toDouble(&valid);
        if(!valid||!std::isfinite(value))throw Error("INVALID_VALUE",mapping.first+" / "+mapping.second+": enter a finite literal number");
        auto node=std::find_if(result.nodes.begin(),result.nodes.end(),[&](const auto& candidate){return candidate.operation.id==mapping.first;});
        node->operation.parameters.at(mapping.second).literal=value;
    }
    return result;
}

void MacroPublicInterfaceDialog::accept() {
    if(finished_||saved_revision_)return;
    try {
        (void)current_document();const auto next=edited_revision();
        host_->session.apply({MacroCommand{UpdateMacroDefinition{definition_id_,next}}},revision_);
        updated_definition_id_=definition_id_;saved_revision_=next.revision;save_->setEnabled(false);
    } catch(const Error& error) { error_->setText(error_text(error));return; }
      catch(const std::exception& error) { error_->setText(QString::fromUtf8(error.what()));return; }
    const QPointer<Host> host=host_;
    QDialog::accept();
    // accepted or the synchronous Host refresh can destroy this dialog.
    if(host)host->edited();
}

void MacroPublicInterfaceDialog::done(int result) {
    finished_=true;QDialog::done(result);
}
}
