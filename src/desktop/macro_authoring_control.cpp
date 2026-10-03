#include "macro_authoring_control.hpp"
#include "host.hpp"
#include "semantic_control.hpp"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <cmath>

namespace nect::desktop {
namespace {
QString text(const std::string& value) { return QString::fromStdString(value); }
QLineEdit* metadata(QFormLayout* form,const QString& label,const QString& value,const QString& name) {
    auto* input=new QLineEdit(value);
    input->setObjectName(name);input->setReadOnly(true);input->setAccessibleName(label);
    form->addRow(label,input);return input;
}
QString parameter_label(const std::string& key) {
    auto label=text(key);label.replace('_',' ');
    if(!label.isEmpty())label[0]=label.at(0).toUpper();
    return label;
}
double literal(QLineEdit* input) {
    bool valid=false;const auto value=input->text().trimmed().toDouble(&valid);
    if(!valid||!std::isfinite(value))
        throw Error("INVALID_VALUE",input->accessibleName().toStdString()+": enter a finite literal number");
    return value;
}
}

MacroAuthoringDialog::MacroAuthoringDialog(Host& host,QWidget* parent):QDialog(parent),host_(&host),
    session_id_(host.session_id),document_id_(host.session.document().id),definition_id_(new_id()),
    revision_(host.session.revision()) {
    setObjectName("macro-authoring-dialog");setWindowTitle("Create Macro");resize(560,660);
    auto* layout=new QVBoxLayout(this);
    auto* identity=new QFormLayout;
    name_=new QLineEdit("Offset Repeat",this);name_->setObjectName("macro-authoring-name");
    name_->setAccessibleName("Macro name");identity->addRow("Name",name_);layout->addLayout(identity);
    auto* graph=new QLabel(QString::fromUtf8("Input → Offset@1 → Repeater@1 → Output"),this);
    graph->setObjectName("macro-authoring-graph");graph->setTextFormat(Qt::PlainText);graph->setWordWrap(true);
    layout->addWidget(graph);
    auto* boundary=new QLabel("Macro v1: fixed local paths and paint chain. Set literal node defaults below. "
        "Only Offset amount is published; other graphs and operator options are unavailable here.",this);
    boundary->setTextFormat(Qt::PlainText);boundary->setWordWrap(true);layout->addWidget(boundary);

    auto* scroll=new QScrollArea(this);scroll->setWidgetResizable(true);
    scroll->setObjectName("macro-authoring-scroll");
    auto* content=new QWidget(scroll);auto* body=new QVBoxLayout(content);
    auto* published=new QGroupBox("Published parameter",content);auto* published_form=new QFormLayout(published);
    parameter_label_=new QLineEdit("Amount",published);
    parameter_label_->setObjectName("macro-authoring-public-label");
    parameter_label_->setAccessibleName("Published parameter label");published_form->addRow("Label",parameter_label_);
    metadata(published_form,"Stable ID","macro.offset.amount","macro-authoring-public-id");
    metadata(published_form,"Mapping","Offset.amount","macro-authoring-public-mapping");
    metadata(published_form,"Value type","number","macro-authoring-public-type");
    metadata(published_form,"Unit","du","macro-authoring-public-unit");
    metadata(published_form,"Domain","local_paths_and_paint","macro-authoring-public-domain");
    body->addWidget(published);

    for(const auto* type:{"nect.shape.offset","nect.shape.repeater"}) {
        NodeDraft node;node.operation=default_operation(new_id(),type);
        const auto* owner=builtin_operation_type(type);
        auto* group=new QGroupBox(text(owner->label)+" defaults",content);
        auto* form=new QFormLayout(group);
        const auto short_type=node.operation.type=="nect.shape.offset"?QString("offset"):QString("repeater");
        for(const auto& [key,value]:node.operation.parameters) {
            const auto descriptor=builtin_semantic_descriptor(type,key);
            QLineEdit* input=descriptor?semantic_number_input(*descriptor,QString::number(value.literal,'g',17),group):
                new QLineEdit(QString::number(value.literal,'g',17),group);
            input->setObjectName("macro-authoring-"+short_type+"-"+text(key));
            const auto label=descriptor?text(descriptor->label)+" · "+text(descriptor->unit):parameter_label(key);
            input->setAccessibleName(text(owner->label)+" / "+label);
            input->setProperty("nect-node-id",text(node.operation.id));
            input->setProperty("nect-parameter",text(key));
            input->setToolTip(input->toolTip()+"\nLiteral default. Validated by the canonical Macro command on Create.");
            form->addRow(label,input);node.inputs.emplace(key,input);
        }
        nodes_.push_back(std::move(node));body->addWidget(group);
    }
    body->addStretch();scroll->setWidget(content);layout->addWidget(scroll);
    error_=new QLabel(this);error_->setObjectName("macro-authoring-error");
    error_->setAccessibleName("Macro creation result");error_->setTextFormat(Qt::PlainText);error_->setWordWrap(true);
    layout->addWidget(error_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
    create_=buttons->button(QDialogButtonBox::Ok);create_->setText("Create Macro");
    create_->setObjectName("macro-authoring-create");
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("macro-authoring-cancel");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,this,&MacroAuthoringDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    name_->setFocus();name_->selectAll();
}

MacroDefinition MacroAuthoringDialog::definition() const {
    if(name_->text().trimmed().isEmpty())throw Error("INVALID_MACRO_LABEL","Enter a Macro name");
    if(parameter_label_->text().trimmed().isEmpty())
        throw Error("INVALID_MACRO_INTERFACE","Enter a published parameter label");
    MacroDefinition result;result.id=definition_id_;result.label=name_->text().toStdString();
    MacroDefinitionRevision graph;
    graph.input={new_id(),"local_paths_and_paint"};graph.output={new_id(),"local_paths_and_paint"};
    for(const auto& draft:nodes_) {
        auto operation=draft.operation;
        for(const auto& [key,input]:draft.inputs)operation.parameters.at(key).literal=literal(input);
        graph.nodes.push_back({std::move(operation),new_id(),new_id()});
    }
    const auto& offset=graph.nodes.at(0);const auto& repeater=graph.nodes.at(1);
    graph.edges={{{"",graph.input.id},{offset.operation.id,offset.input_port}},
        {{offset.operation.id,offset.output_port},{repeater.operation.id,repeater.input_port}},
        {{repeater.operation.id,repeater.output_port},{"",graph.output.id}}};
    graph.output_mapping={repeater.operation.id,repeater.output_port};
    graph.public_parameters.push_back({"macro.offset.amount",parameter_label_->text().toStdString(),
        offset.operation.id,"amount","number","du","local_paths_and_paint"});
    result.revisions.emplace(1,std::move(graph));return result;
}

void MacroAuthoringDialog::accept() {
    // Queued/repeated clicks cannot author a second definition after success.
    if(!created_definition_id_.empty())return;
    try {
        if(!host_||host_->session_id!=session_id_||host_->session.document().id!=document_id_)
            throw Error("SESSION_CONFLICT","This Macro draft belongs to another document; reopen Create Macro");
        if(host_->session.revision()!=revision_)
            throw Error("REVISION_CONFLICT","The document changed; reopen Create Macro before creating it");
        if(host_->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        host_->session.apply({MacroCommand{CreateMacroDefinition{definition()}}},revision_);
        created_definition_id_=definition_id_;create_->setEnabled(false);
    } catch(const Error& error) {
        error_->setText(text(error.code)+": "+QString::fromUtf8(error.what()));return;
    } catch(const std::exception& error) {
        error_->setText(QString::fromUtf8(error.what()));return;
    }
    // Closing or a synchronous accepted/refresh callback may destroy this
    // dialog. Do not access its members after emitting accepted.
    const QPointer<Host> host=host_;
    QDialog::accept();
    if(host)host->edited();
}
}
