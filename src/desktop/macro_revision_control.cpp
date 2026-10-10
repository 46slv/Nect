#include "macro_revision_control.hpp"
#include "host.hpp"
#include "semantic_control.hpp"
#include "semantic_toggle_control.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVariant>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace nect::desktop {
namespace {
QString text(const std::string& value) { return QString::fromStdString(value); }
QString number(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QLineEdit* metadata(QFormLayout* form,const QString& label,const QString& value,const QString& name) {
    auto* input=new QLineEdit(value);
    input->setObjectName(name);input->setReadOnly(true);input->setAccessibleName(label);
    form->addRow(label,input);return input;
}
double literal(QLineEdit* input) {
    bool valid=false;const auto value=input->text().trimmed().toDouble(&valid);
    if(!valid||!std::isfinite(value))
        throw Error("INVALID_VALUE",input->accessibleName().toStdString()+": enter a finite literal number");
    return value;
}
const ProcessingEntry& macro_operation(const Document& document,const Id& object,const Id& instance) {
    const auto found=document.objects.find(object);
    if(found==document.objects.end())throw Error("MISSING_OBJECT",object);
    const auto entry=std::find_if(found->second.stack.begin(),found->second.stack.end(),
        [&](const auto& operation){return operation.id==instance;});
    if(entry==found->second.stack.end()||!entry->macro)throw Error("MISSING_MACRO_INSTANCE",instance);
    return *entry;
}
QString error_text(const Error& error) { return text(error.code)+": "+QString::fromUtf8(error.what()); }
}

MacroRevisionDialog::MacroRevisionDialog(Host& host,const Id& definition,QWidget* parent):
    QDialog(parent),host_(&host),session_id_(host.session_id),document_id_(host.session.document().id),
    revision_(host.session.revision()),gesture_(host.session.gesture_generation()) {
    setObjectName("macro-revision-dialog");setWindowTitle("Edit Macro revision");resize(560,680);
    auto* layout=new QVBoxLayout(this);auto* identity=new QFormLayout;
    definitions_=new QComboBox(this);definitions_->setObjectName("macro-revision-definition");
    definitions_->setAccessibleName("Macro Definition");
    definitions_->setToolTip("Selecting another Definition replaces this unsaved draft.");
    for(const auto& [id,source]:host.session.document().macro_definitions) {
        definitions_->addItem(text(source.label)+" · "+text(id),text(id));
        definitions_->setItemData(definitions_->count()-1,text(id),Qt::ToolTipRole);
    }
    if(!definition.empty())definitions_->setCurrentIndex(definitions_->findData(text(definition)));
    identity->addRow("Definition",definitions_);layout->addLayout(identity);
    auto* graph=new QLabel(QString::fromUtf8("Input → Offset@1 → Repeater@1 → Output"),this);
    graph->setObjectName("macro-revision-graph");graph->setTextFormat(Qt::PlainText);graph->setWordWrap(true);
    layout->addWidget(graph);
    auto* boundary=new QLabel("Edit literal node defaults and the existing published label. Save creates a new "
        "revision and keeps older revisions. Instances keep their current pins until explicitly updated. "
        "Selecting another Definition replaces this unsaved draft.",this);
    boundary->setTextFormat(Qt::PlainText);boundary->setWordWrap(true);layout->addWidget(boundary);
    scroll_=new QScrollArea(this);scroll_->setObjectName("macro-revision-scroll");
    scroll_->setWidgetResizable(true);layout->addWidget(scroll_);
    error_=new QLabel(this);error_->setObjectName("macro-revision-error");
    error_->setAccessibleName("Macro revision result");error_->setTextFormat(Qt::PlainText);error_->setWordWrap(true);
    layout->addWidget(error_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    save_=buttons->button(QDialogButtonBox::Save);save_->setText("Save new revision");
    save_->setObjectName("macro-revision-save");
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("macro-revision-cancel");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,this,&MacroRevisionDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(definitions_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){load_definition();});
    load_definition();
}

const Document& MacroRevisionDialog::current_document() const {
    if(!host_||host_->session_id!=session_id_||host_->session.document().id!=document_id_)
        throw Error("SESSION_CONFLICT","This Macro draft belongs to another document; reopen Edit Macro revision");
    if(host_->session.revision()!=revision_)
        throw Error("REVISION_CONFLICT","The document changed; reopen Edit Macro revision");
    if(host_->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
    if(host_->session.gesture_generation()!=gesture_)
        throw Error("REVISION_CONFLICT","The edit context changed; reopen Edit Macro revision");
    return host_->session.document();
}

void MacroRevisionDialog::load_definition() {
    if(finished_)return;
    loaded_=false;save_->setEnabled(false);parameter_label_=nullptr;nodes_.clear();
    delete scroll_->takeWidget();
    try {
        const auto& document=current_document();
        if(definitions_->currentIndex()<0)throw Error("MISSING_MACRO_DEFINITION","Choose an existing Macro Definition");
        definition_id_=definitions_->currentData().toString().toStdString();
        const auto found=document.macro_definitions.find(definition_id_);
        if(found==document.macro_definitions.end())throw Error("MISSING_MACRO_DEFINITION",definition_id_);
        // The same canonical validator used by native/Core owns support checks.
        validate_macro_definition(found->second);
        if(found->second.latest_revision==std::numeric_limits<std::uint64_t>::max())
            throw Error("MACRO_REVISION_LIMIT","Macro revision space exhausted");
        source_=found->second.revisions.at(found->second.latest_revision);
        draft_=source_;
        draft_.revision=found->second.latest_revision+1;
        QStringList chain{"Input"};
        for(const auto* node:macro_execution_order(draft_))
            chain.push_back(node->operation.type=="nect.shape.offset"?"Offset@1":"Repeater@1");
        chain.push_back("Output");
        findChild<QLabel*>("macro-revision-graph")->setText(chain.join(QString::fromUtf8(" → ")));
        auto* content=new QWidget(scroll_);auto* body=new QVBoxLayout(content);scroll_->setWidget(content);
        auto* source=new QFormLayout;
        metadata(source,"Definition ID",text(definition_id_),"macro-revision-definition-id");
        metadata(source,"Source revision",number(found->second.latest_revision),"macro-revision-source-revision");
        metadata(source,"New revision",number(draft_.revision),"macro-revision-next-revision");body->addLayout(source);
        auto* published=new QGroupBox("Published parameter",content);auto* form=new QFormLayout(published);
        if(draft_.public_parameters.empty()) {
            auto* unavailable=new QLabel("This revision has no published parameter. Its interface is retained.",published);
            unavailable->setWordWrap(true);form->addRow(unavailable);
        } else {
            const auto& parameter=draft_.public_parameters.front();
            parameter_label_=new QLineEdit(text(parameter.label),published);
            parameter_label_->setObjectName("macro-revision-public-label");
            parameter_label_->setAccessibleName("Published parameter label");form->addRow("Label",parameter_label_);
            metadata(form,"Stable ID",text(parameter.id),"macro-revision-public-id");
            metadata(form,"Mapping",text(parameter.node)+" / "+text(parameter.parameter),"macro-revision-public-mapping");
            metadata(form,"Value type",text(parameter.value_type),"macro-revision-public-type");
            metadata(form,"Unit",text(parameter.unit),"macro-revision-public-unit");
            metadata(form,"Domain",text(parameter.domain),"macro-revision-public-domain");
        }
        body->addWidget(published);
        // Walk the actual graph: graph2 may omit either operator or repeat it.
        // Keep the first control names compatible and disambiguate later nodes.
        std::map<std::string,unsigned> type_counts;
        for(const auto* node:macro_execution_order(draft_)) {
            const auto& type=node->operation.type;
            const auto* owner=builtin_operation_type(type);NodeDraft fields;fields.node=node->operation.id;
            auto* group=new QGroupBox(text(owner->label)+" · "+text(fields.node)+" defaults",content);auto* node_form=new QFormLayout(group);
            auto short_type=type=="nect.shape.offset"?QString("offset"):QString("repeater");
            const auto occurrence=++type_counts[type];
            if(occurrence>1)short_type+="-"+QString::number(occurrence);
            if(draft_.interface_version==3) {
                const auto descriptor=builtin_semantic_descriptor(type,"enabled");
                fields.enabled=semantic_toggle_input(*descriptor,node->operation.enabled,group);
                fields.enabled->setObjectName("macro-revision-"+short_type+"-enabled");
                fields.enabled->setAccessibleName(text(owner->label)+" / Enabled");
                fields.enabled->setProperty("nect-node-id",text(fields.node));
                fields.enabled->setProperty("nect-parameter","enabled");
                fields.enabled->setToolTip("Literal node default in this draft. Save appends a new revision; existing instances stay pinned.");
                node_form->addRow("Enabled",fields.enabled);
            }
            for(const auto& [key,value]:node->operation.parameters) {
                const auto descriptor=builtin_semantic_descriptor(type,key);
                QLineEdit* input=descriptor?semantic_number_input(*descriptor,QString::number(value.literal,'g',17),group):
                    new QLineEdit(QString::number(value.literal,'g',17),group);
                input->setObjectName("macro-revision-"+short_type+"-"+text(key));
                const auto label=descriptor?text(descriptor->label)+" · "+text(descriptor->unit):text(key);
                input->setAccessibleName(text(owner->label)+" / "+label);
                input->setProperty("nect-node-id",text(fields.node));input->setProperty("nect-parameter",text(key));
                node_form->addRow(label,input);fields.inputs.emplace(key,input);
            }
            nodes_.push_back(std::move(fields));body->addWidget(group);
        }
        body->addStretch();loaded_=true;save_->setEnabled(true);error_->clear();
    } catch(const Error& error) { error_->setText(error_text(error)); }
      catch(const std::exception& error) { error_->setText(QString::fromUtf8(error.what())); }
}

MacroDefinitionRevision MacroRevisionDialog::edited_revision() const {
    if(!loaded_)throw Error("MISSING_MACRO_DEFINITION","Choose an existing Macro Definition");
    auto result=draft_;
    if(parameter_label_) {
        if(parameter_label_->text().trimmed().isEmpty())throw Error("INVALID_MACRO_INTERFACE","Enter a published parameter label");
        result.public_parameters.front().label=parameter_label_->text().toStdString();
    }
    for(const auto& fields:nodes_) {
        auto node=std::find_if(result.nodes.begin(),result.nodes.end(),
            [&](const auto& candidate){return candidate.operation.id==fields.node;});
        if(fields.enabled)node->operation.enabled=fields.enabled->isChecked();
        for(const auto& [key,input]:fields.inputs)node->operation.parameters.at(key).literal=literal(input);
    }
    return result;
}

void MacroRevisionDialog::accept() {
    if(finished_||saved_revision_)return;
    try {
        const auto& document=current_document();
        if(loaded_) {
            const auto source=document.macro_definitions.find(definition_id_);
            if(source==document.macro_definitions.end()||source->second.latest_revision!=source_.revision||
               !source->second.revisions.contains(source_.revision)||source->second.revisions.at(source_.revision)!=source_)
                throw Error("PROPERTY_CONFLICT","The Macro source revision changed; reopen Edit Macro revision");
        }
        auto next=edited_revision();
        host_->session.apply({MacroCommand{UpdateMacroDefinition{definition_id_,next}}},revision_);
        updated_definition_id_=definition_id_;saved_revision_=next.revision;save_->setEnabled(false);
    } catch(const Error& error) { error_->setText(error_text(error));return; }
      catch(const std::exception& error) { error_->setText(QString::fromUtf8(error.what()));return; }
    const QPointer<Host> host=host_;
    QDialog::accept();
    // accepted/Host refresh may synchronously destroy this dialog.
    if(host)host->edited();
}

void MacroRevisionDialog::done(int result) {
    finished_=true;
    QDialog::done(result);
}

QWidget* make_macro_revision_controls(Host& host,const Id& object,const Id& instance,QWidget* parent) {
    const auto& document=host.session.document();const auto& operation=macro_operation(document,object,instance);
    const auto definition=document.macro_definitions.find(operation.macro->definition);
    if(definition==document.macro_definitions.end())throw Error("MISSING_MACRO_DEFINITION",operation.macro->definition);
    const auto definition_id=definition->first;const auto pinned=operation.macro->pinned_revision;
    const auto document_id=document.id;const auto session_id=host.session_id;
    const auto revision=host.session.revision(),gesture=host.session.gesture_generation();
    auto* box=new QGroupBox("Macro revision",parent);box->setObjectName("macro-instance-revision-controls");
    box->setProperty("nect-object-id",text(object));box->setProperty("nect-instance-id",text(instance));
    auto* layout=new QVBoxLayout(box);
    auto* identity=new QLabel(text(definition->second.label)+" · "+text(definition_id),box);
    identity->setTextFormat(Qt::PlainText);identity->setWordWrap(true);layout->addWidget(identity);
    auto* state=new QLabel("Pinned revision "+number(pinned)+" · Latest "+number(definition->second.latest_revision),box);
    state->setObjectName("macro-instance-revision-state");state->setTextFormat(Qt::PlainText);layout->addWidget(state);
    auto* target=new QComboBox(box);target->setObjectName("macro-instance-revision-target");
    target->setAccessibleName("Target Macro revision");
    for(const auto& [number_value,graph]:definition->second.revisions) {
        (void)graph;auto label="Revision "+number(number_value);
        if(number_value==pinned)label+=" · Current";
        if(number_value==definition->second.latest_revision)label+=" · Latest";
        target->addItem(label,QVariant::fromValue(static_cast<qulonglong>(number_value)));
    }
    target->setCurrentIndex(target->findData(QVariant::fromValue(static_cast<qulonglong>(definition->second.latest_revision))));
    layout->addWidget(target);
    auto* notice=new QLabel("Only this instance will change. Local overrides are retained. Incompatible migrations "
        "are refused; no override is automatically removed.",box);
    notice->setWordWrap(true);layout->addWidget(notice);
    auto* row=new QHBoxLayout;auto* update=new QPushButton("Update this instance",box);
    update->setObjectName("macro-instance-revision-update");row->addWidget(update);
    auto* edit=new QPushButton("Edit new revision…",box);edit->setObjectName("macro-instance-revision-edit");
    row->addWidget(edit);layout->addLayout(row);
    auto* status=new QLabel(box);status->setObjectName("macro-instance-revision-error");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_target(target);const QPointer<QPushButton> safe_update(update),safe_edit(edit);
    const QPointer<QLabel> safe_status(status);const auto committed=std::make_shared<bool>(false);
    const auto current_document=[=]()->const Document& {
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Macro instance belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","The document changed; refresh before updating this instance");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before updating this instance");
        return safe_host->session.document();
    };
    const auto target_changed=[=] {
        if(!safe_box||*committed)return;
        safe_update->setEnabled(false);safe_edit->setEnabled(false);
        try {
            (void)current_document();safe_edit->setEnabled(true);
            safe_update->setEnabled(safe_target->currentIndex()>=0&&safe_target->currentData().toULongLong()!=pinned);
            safe_status->clear();
        } catch(const Error& error) { safe_status->setText(error_text(error)); }
          catch(const std::exception& error) { safe_status->setText(QString::fromUtf8(error.what())); }
    };
    QObject::connect(target,qOverload<int>(&QComboBox::currentIndexChanged),box,[target_changed](int){target_changed();});
    QObject::connect(update,&QPushButton::clicked,box,[=] {
        if(!safe_box||*committed)return;
        try {
            const auto& current=current_document();
            const auto& current_operation=macro_operation(current,object,instance);
            if(current_operation.macro->definition!=definition_id||current_operation.macro->pinned_revision!=pinned)
                throw Error("REVISION_CONFLICT","This Macro instance changed; refresh before updating");
            if(safe_target->currentIndex()<0)throw Error("MISSING_MACRO_REVISION","Choose a retained revision");
            const auto next=static_cast<std::uint64_t>(safe_target->currentData().toULongLong());
            if(next==pinned)return;
            safe_host->session.apply({MacroCommand{UpdateMacroInstance{object,instance,next}}},revision);
            *committed=true;safe_update->setEnabled(false);safe_edit->setEnabled(false);safe_target->setEnabled(false);
            // This may synchronously rebuild and destroy the controls.
            safe_host->edited();return;
        } catch(const Error& error) { if(safe_status)safe_status->setText(error_text(error)); }
          catch(const std::exception& error) { if(safe_status)safe_status->setText(QString::fromUtf8(error.what())); }
    });
    QObject::connect(edit,&QPushButton::clicked,box,[=] {
        if(!safe_box||*committed)return;
        try {
            (void)current_document();
            auto* dialog=new MacroRevisionDialog(*safe_host,definition_id,safe_box->window());
            dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->open();
        } catch(const Error& error) { if(safe_status)safe_status->setText(error_text(error)); }
          catch(const std::exception& error) { if(safe_status)safe_status->setText(QString::fromUtf8(error.what())); }
    });
    target_changed();return box;
}
}
