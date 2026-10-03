#include "macro_chain_control.hpp"
#include "host.hpp"
#include "semantic_control.hpp"
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

namespace nect::desktop {
namespace {
QString text(const std::string& value) { return QString::fromStdString(value); }
QString number(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString error_text(const Error& error) { return text(error.code)+": "+QString::fromUtf8(error.what()); }
QString node_label(const MacroNode& node,std::size_t index) {
    return number(index+1)+". "+(node.operation.type=="nect.shape.offset"?"Offset":"Repeater")+
        " · "+text(node.operation.id);
}
double literal(const QString& value,const std::string& key) {
    bool valid=false;const auto result=value.trimmed().toDouble(&valid);
    if(!valid||!std::isfinite(result))throw Error("INVALID_VALUE",key+": enter a finite literal number");
    return result;
}
QPushButton* button(const QString& label,const char* name,QWidget* parent,QHBoxLayout* row) {
    auto* result=new QPushButton(label,parent);result->setObjectName(QString::fromUtf8(name));
    result->setAccessibleName(label);result->setAutoDefault(false);row->addWidget(result);return result;
}
}

MacroChainDialog::MacroChainDialog(Host& host,const Id& definition,QWidget* parent):
    QDialog(parent),host_(&host),session_id_(host.session_id),document_id_(host.session.document().id),
    revision_(host.session.revision()),gesture_(host.session.gesture_generation()) {
    setObjectName("macro-chain-dialog");setWindowTitle("Edit Macro chain");resize(620,760);
    auto* layout=new QVBoxLayout(this);auto* identity=new QFormLayout;
    definitions_=new QComboBox(this);definitions_->setObjectName("macro-chain-definition");
    definitions_->setAccessibleName("Macro Definition");
    definitions_->setToolTip("Choosing another Definition discards this unsaved draft.");
    for(const auto& [id,value]:host.session.document().macro_definitions)
        definitions_->addItem(text(value.label)+" · "+text(id),text(id));
    if(!definition.empty())definitions_->setCurrentIndex(definitions_->findData(text(definition)));
    identity->addRow("Definition",definitions_);
    source_=new QLabel(this);source_->setObjectName("macro-chain-revisions");source_->setTextFormat(Qt::PlainText);
    identity->addRow("Revision",source_);layout->addLayout(identity);
    auto* notice=new QLabel("Input → chain below → Output. Add, move or remove Offset/Repeater nodes (1–16). "
        "Save creates a native graph-v2 revision; portable Macro v1 export cannot carry it. "
        "Instances keep their current revision until explicitly updated. Choosing another Definition discards this draft.",this);
    notice->setTextFormat(Qt::PlainText);notice->setWordWrap(true);layout->addWidget(notice);
    chain_=new QListWidget(this);chain_->setObjectName("macro-chain-nodes");
    chain_->setAccessibleName("Macro nodes in execution order");chain_->setMaximumHeight(170);
    chain_->setSelectionMode(QAbstractItemView::SingleSelection);layout->addWidget(chain_);
    auto* actions=new QHBoxLayout;
    add_offset_=button("Add Offset","macro-chain-add-offset",this,actions);
    add_repeater_=button("Add Repeater","macro-chain-add-repeater",this,actions);
    up_=button("Move up","macro-chain-up",this,actions);
    down_=button("Move down","macro-chain-down",this,actions);
    remove_=button("Remove","macro-chain-remove",this,actions);layout->addLayout(actions);
    auto* published=new QGroupBox("Published Amount",this);auto* interface=new QFormLayout(published);
    publish_amount_=new QCheckBox("Expose Amount on instances",published);
    publish_amount_->setObjectName("macro-chain-publish-amount");
    publish_amount_->setToolTip("Turning this off removes the control only in the new revision. Existing instances keep their pins; reset old overrides before explicitly updating them.");
    interface->addRow(publish_amount_);
    public_id_=new QLabel(published);public_id_->setObjectName("macro-chain-public-id");
    public_id_->setTextFormat(Qt::PlainText);interface->addRow("Stable ID",public_id_);
    parameter_label_=new QLineEdit(published);parameter_label_->setObjectName("macro-chain-public-label");
    parameter_label_->setAccessibleName("Published parameter label");interface->addRow("Label",parameter_label_);
    mapping_=new QComboBox(published);mapping_->setObjectName("macro-chain-public-node");
    mapping_->setAccessibleName("Offset node published as Amount");
    mapping_->setToolTip("Choose the Offset whose Amount is published. Removing it requires an explicit new choice before Save.");
    interface->addRow("Offset node",mapping_);layout->addWidget(published);
    defaults_=new QScrollArea(this);defaults_->setObjectName("macro-chain-defaults");
    defaults_->setWidgetResizable(true);layout->addWidget(defaults_);
    error_=new QLabel(this);error_->setObjectName("macro-chain-error");
    error_->setAccessibleName("Macro chain result");error_->setTextFormat(Qt::PlainText);error_->setWordWrap(true);
    layout->addWidget(error_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    save_=buttons->button(QDialogButtonBox::Save);save_->setText("Save new revision");
    save_->setObjectName("macro-chain-save");buttons->button(QDialogButtonBox::Cancel)->setObjectName("macro-chain-cancel");
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,this,&MacroChainDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(definitions_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){load_definition();});
    connect(chain_,&QListWidget::currentRowChanged,this,[this](int){capture_defaults();show_defaults();refresh_actions();});
    connect(mapping_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
        mapped_node_=mapping_->currentData().toString().toStdString();
    });
    connect(publish_amount_,&QCheckBox::toggled,this,[this](bool){
        parameter_label_->setEnabled(loaded_&&publish_amount_->isChecked());
        mapping_->setEnabled(loaded_&&publish_amount_->isChecked());
        error_->clear();
    });
    connect(add_offset_,&QPushButton::clicked,this,[this]{add_node("nect.shape.offset");});
    connect(add_repeater_,&QPushButton::clicked,this,[this]{add_node("nect.shape.repeater");});
    connect(up_,&QPushButton::clicked,this,[this]{move_node(-1);});
    connect(down_,&QPushButton::clicked,this,[this]{move_node(1);});
    connect(remove_,&QPushButton::clicked,this,[this]{remove_node();});
    load_definition();
}

const Document& MacroChainDialog::current_document() const {
    if(!host_||host_->session_id!=session_id_||host_->session.document().id!=document_id_)
        throw Error("SESSION_CONFLICT","This Macro draft belongs to another document; reopen Edit Macro chain");
    if(host_->session.revision()!=revision_)
        throw Error("REVISION_CONFLICT","The document changed; reopen Edit Macro chain");
    if(host_->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
    if(host_->session.gesture_generation()!=gesture_)
        throw Error("REVISION_CONFLICT","The edit context changed; reopen Edit Macro chain");
    return host_->session.document();
}

void MacroChainDialog::load_definition() {
    if(finished_)return;
    loaded_=false;nodes_.clear();inputs_.clear();selected_node_.clear();mapped_node_.clear();
    source_->clear();parameter_label_->clear();parameter_label_->setEnabled(false);public_id_->clear();
    {const QSignalBlocker blocker(publish_amount_);publish_amount_->setChecked(false);}
    public_template_={"macro.offset.amount","Amount",{},"amount","number","du","local_paths_and_paint"};
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
        draft_.revision=found->second.latest_revision+1;
        source_->setText("Source "+number(found->second.latest_revision)+" → New "+number(draft_.revision));
        for(const auto* node:macro_execution_order(draft_)) {
            NodeDraft value;value.node=*node;
            for(const auto& [key,scalar]:node->operation.parameters)value.values.emplace(key,QString::number(scalar.literal,'g',17));
            nodes_.push_back(std::move(value));
        }
        auto history=found->second.revisions.upper_bound(found->second.latest_revision);
        while(history!=found->second.revisions.begin()) {
            --history;
            if(!history->second.public_parameters.empty()) {
                public_template_=history->second.public_parameters.front();break;
            }
        }
        mapped_node_=public_template_.node;
        parameter_label_->setText(text(public_template_.label));
        public_id_->setText(text(public_template_.id)+" · "+text(public_template_.unit));
        {const QSignalBlocker blocker(publish_amount_);publish_amount_->setChecked(!draft_.public_parameters.empty());}
        parameter_label_->setEnabled(publish_amount_->isChecked());
        loaded_=true;error_->clear();
    } catch(const Error& error) { error_->setText(error_text(error)); }
      catch(const std::exception& error) { error_->setText(QString::fromUtf8(error.what())); }
    rebuild_chain(nodes_.empty()?Id{}:nodes_.front().node.operation.id);
}

void MacroChainDialog::capture_defaults() {
    const auto node=std::find_if(nodes_.begin(),nodes_.end(),[&](const auto& value){return value.node.operation.id==selected_node_;});
    if(node!=nodes_.end())for(const auto& [key,input]:inputs_)node->values.at(key)=input->text();
}

void MacroChainDialog::show_defaults() {
    inputs_.clear();selected_node_.clear();delete defaults_->takeWidget();
    const auto row=chain_->currentRow();if(row<0||static_cast<std::size_t>(row)>=nodes_.size())return;
    const auto& draft=nodes_[row];selected_node_=draft.node.operation.id;
    auto* content=new QWidget(defaults_);auto* form=new QFormLayout(content);defaults_->setWidget(content);
    auto* title=new QLabel(node_label(draft.node,row)+" defaults",content);title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);form->addRow(title);
    for(const auto& [key,value]:draft.values) {
        const auto descriptor=builtin_semantic_descriptor(draft.node.operation.type,key);
        QLineEdit* input=descriptor?semantic_number_input(*descriptor,value,content):new SemanticNumberInput(value,content);
        input->setObjectName("macro-chain-default-"+text(key));
        const auto label=descriptor?text(descriptor->label)+" · "+text(descriptor->unit):text(key);
        input->setAccessibleName(text(builtin_operation_type(draft.node.operation.type)->label)+" / "+label);
        input->setProperty("nect-node-id",text(selected_node_));input->setProperty("nect-parameter",text(key));
        input->setToolTip(input->toolTip()+"\nLiteral default. Validated by the canonical Macro command on Save.");
        form->addRow(label,input);inputs_.emplace(key,input);
    }
}

void MacroChainDialog::rebuild_chain(const Id& selection) {
    {
        const QSignalBlocker blocker(chain_);chain_->clear();int selected=-1;
        for(std::size_t index=0;index<nodes_.size();++index) {
            const auto& node=nodes_[index].node;auto* item=new QListWidgetItem(node_label(node,index),chain_);
            item->setData(Qt::UserRole,text(node.operation.id));item->setToolTip(text(node.operation.id));
            if(node.operation.id==selection)selected=static_cast<int>(index);
        }
        chain_->setCurrentRow(selected);
    }
    {
        const QSignalBlocker blocker(mapping_);mapping_->clear();
        mapping_->addItem("Choose an Offset node…",QString{});
        for(std::size_t index=0;index<nodes_.size();++index)if(nodes_[index].node.operation.type=="nect.shape.offset")
            mapping_->addItem(node_label(nodes_[index].node,index),text(nodes_[index].node.operation.id));
        const auto mapped=mapping_->findData(text(mapped_node_));mapping_->setCurrentIndex(mapped>0?mapped:0);
        mapping_->setEnabled(loaded_&&publish_amount_->isChecked());
    }
    show_defaults();refresh_actions();
}

void MacroChainDialog::refresh_actions() {
    const auto row=chain_->currentRow();const bool active=loaded_&&!finished_&&!saved_revision_;
    save_->setEnabled(active);chain_->setEnabled(active);publish_amount_->setEnabled(active);
    add_offset_->setEnabled(active&&nodes_.size()<16);add_repeater_->setEnabled(active&&nodes_.size()<16);
    up_->setEnabled(active&&row>0);down_->setEnabled(active&&row>=0&&static_cast<std::size_t>(row+1)<nodes_.size());
    remove_->setEnabled(active&&row>=0&&nodes_.size()>1);
}

void MacroChainDialog::add_node(const std::string& type) {
    if(!loaded_||finished_||saved_revision_||nodes_.size()>=16)return;
    capture_defaults();NodeDraft value;value.node={default_operation(new_id(),type),new_id(),new_id()};
    for(const auto& [key,scalar]:value.node.operation.parameters)value.values.emplace(key,QString::number(scalar.literal,'g',17));
    const auto selection=value.node.operation.id;nodes_.push_back(std::move(value));rebuild_chain(selection);
}

void MacroChainDialog::move_node(int delta) {
    const auto row=chain_->currentRow();const auto target=row+delta;
    if(!loaded_||finished_||saved_revision_||row<0||target<0||static_cast<std::size_t>(target)>=nodes_.size())return;
    capture_defaults();const auto selection=nodes_[row].node.operation.id;
    std::swap(nodes_[row],nodes_[target]);rebuild_chain(selection);
}

void MacroChainDialog::remove_node() {
    const auto row=chain_->currentRow();
    if(!loaded_||finished_||saved_revision_||row<0||nodes_.size()<=1)return;
    capture_defaults();const bool published=nodes_[row].node.operation.id==mapped_node_&&publish_amount_->isChecked();
    nodes_.erase(nodes_.begin()+row);const auto next=std::min(static_cast<std::size_t>(row),nodes_.size()-1);
    rebuild_chain(nodes_[next].node.operation.id);
    if(published)error_->setText("The published Offset was removed. Choose an Offset node for Amount before Save.");
}

MacroDefinitionRevision MacroChainDialog::edited_revision() {
    if(!loaded_)throw Error("MISSING_MACRO_DEFINITION","Choose an existing Macro Definition");
    capture_defaults();auto result=draft_;result.graph_version=2;result.nodes.clear();result.edges.clear();
    for(const auto& value:nodes_) {
        auto node=value.node;
        for(const auto& [key,raw]:value.values)node.operation.parameters.at(key).literal=literal(raw,key);
        result.nodes.push_back(std::move(node));
    }
    if(result.nodes.empty())throw Error("INVALID_MACRO_GRAPH","A Macro chain needs at least one node");
    MacroEndpoint previous{"",result.input.id};
    for(const auto& node:result.nodes) {
        result.edges.push_back({previous,{node.operation.id,node.input_port}});
        previous={node.operation.id,node.output_port};
    }
    result.edges.push_back({previous,{"",result.output.id}});result.output_mapping=previous;
    result.public_parameters.clear();
    if(publish_amount_->isChecked()) {
        const auto selected=mapping_->currentData().toString().toStdString();
        const auto mapped=std::find_if(result.nodes.begin(),result.nodes.end(),[&](const auto& node){
            return node.operation.id==selected&&node.operation.type=="nect.shape.offset";
        });
        if(mapped==result.nodes.end())throw Error("INVALID_MACRO_MAPPING","Choose an Offset node for published Amount before Save");
        if(parameter_label_->text().trimmed().isEmpty())throw Error("INVALID_MACRO_INTERFACE","Enter a published parameter label");
        auto parameter=public_template_;parameter.node=selected;
        parameter.label=parameter_label_->text().toStdString();
        result.public_parameters.push_back(std::move(parameter));
    }
    return result;
}

void MacroChainDialog::accept() {
    if(finished_||saved_revision_)return;
    try {
        (void)current_document();const auto next=edited_revision();
        host_->session.apply({MacroCommand{UpdateMacroDefinition{definition_id_,next}}},revision_);
        updated_definition_id_=definition_id_;saved_revision_=next.revision;save_->setEnabled(false);
    } catch(const Error& error) { error_->setText(error_text(error));return; }
      catch(const std::exception& error) { error_->setText(QString::fromUtf8(error.what()));return; }
    const QPointer<Host> host=host_;
    QDialog::accept(); // accepted/Host refresh may synchronously destroy this dialog.
    if(host)host->edited();
}

void MacroChainDialog::done(int result) {
    finished_=true;QDialog::done(result);
}

QWidget* make_macro_chain_controls(Host& host,QWidget* parent) {
    const auto document_id=host.session.document().id;const auto session_id=host.session_id;
    const auto revision=host.session.revision(),gesture=host.session.gesture_generation();
    auto* box=new QGroupBox("Macro chain",parent);box->setObjectName("macro-chain-controls");
    auto* layout=new QVBoxLayout(box);auto* edit=new QPushButton("Edit Macro chain…",box);
    edit->setObjectName("macro-chain-edit");edit->setEnabled(!host.session.document().macro_definitions.empty()&&!host.session.gesture_active());
    layout->addWidget(edit);auto* status=new QLabel(box);status->setObjectName("macro-chain-controls-error");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);const QPointer<QLabel> safe_status(status);
    QObject::connect(edit,&QPushButton::clicked,box,[=] {
        if(!safe_box)return;
        try {
            if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
                throw Error("SESSION_CONFLICT","This Macro control belongs to another document");
            if(safe_host->session.revision()!=revision||safe_host->session.gesture_generation()!=gesture)
                throw Error("REVISION_CONFLICT","The document changed; refresh before editing a Macro chain");
            if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
            auto* dialog=new MacroChainDialog(*safe_host,{},safe_box->window());
            dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->open();
        } catch(const Error& error) { if(safe_status)safe_status->setText(error_text(error)); }
          catch(const std::exception& error) { if(safe_status)safe_status->setText(QString::fromUtf8(error.what())); }
    });
    return box;
}
}
