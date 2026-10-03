#include "operation_enabled_batch_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

namespace nect::desktop {
namespace {
const ProcessingEntry& target_operation(const Document& document,const OperationEnabledBatchTarget& target){
    const auto object=document.objects.find(target.object);
    if(object==document.objects.end())throw Error("MISSING_OBJECT",target.object);
    const auto& stack=object->second.stack;
    const auto operation=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==target.operation;});
    if(operation==stack.end())throw Error("MISSING_OPERATION",target.object+" / "+target.operation);
    return *operation;
}
void require_targets(const Document& document,const std::vector<OperationEnabledBatchTarget>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct authored operation targets");
    std::set<OperationEnabledBatchTarget> unique;
    std::optional<std::size_t> shared_slot;
    std::string shared_type;unsigned shared_version=0;
    for(const auto& target:targets){
        const auto identity=target.object+" / "+target.operation;
        if(!unique.insert(target).second)throw Error("INVALID_SELECTION","Duplicate operation target: "+identity);
        const auto& operation=target_operation(document,target);
        const auto& object=document.objects.at(target.object);
        const auto* descriptor=builtin_operation_type(operation.type);
        if(operation.macro||!descriptor)
            throw Error("TYPE_MISMATCH","Every target must be an authored built-in operation: "+identity);
        const auto kind=object.kind;
        const bool compatible=(descriptor->target_kind=="path_or_text"&&(kind==Kind::path||kind==Kind::text))||
            (descriptor->target_kind=="group"&&kind==Kind::group);
        if(!compatible)throw Error("TYPE_MISMATCH","Incompatible built-in operation target: "+identity);
        const auto slot=static_cast<std::size_t>(&operation-object.stack.data());
        if(shared_slot&&(slot!=*shared_slot||operation.type!=shared_type||operation.version!=shared_version))
            throw Error("TYPE_MISMATCH","Choose the same built-in type and behavior version at the same stack slot: "+identity);
        shared_slot=slot;shared_type=operation.type;shared_version=operation.version;
        if(operation.enabled_driver||operation.enabled_expression)
            throw Error("DRIVEN_PROPERTY","Operation enabled is linked or expression-driven on "+identity+
                "; unlink it explicitly before a batch enabled edit");
    }
}
bool unchanged_enabled(const Document& document,const std::vector<OperationEnabledBatchTarget>& targets,bool value){
    for(const auto& target:targets)if(target_operation(document,target).enabled!=value)return false;
    return true;
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_operation_enabled_batch_controls(Host& host,
        const std::vector<OperationEnabledBatchTarget>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Effect enabled · %1 selected targets").arg(targets.size()),parent);
    box->setObjectName("operation-enabled-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("operation-enabled-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* editor=new QComboBox(box);editor->setObjectName("operation-enabled-batch-editor");
    editor->setAccessibleName("Enable or bypass every captured built-in operation");
    editor->addItem("Enable","enable");editor->addItem("Bypass","bypass");
    editor->setToolTip("Choose Enable or Bypass, then Apply to every captured operation. Mixed is a display state.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("operation-enabled-batch-apply");
    apply->setToolTip("Sets all captured operations in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("operation-enabled-batch-cancel");
    cancel->setToolTip("Discard the enabled draft without editing the document.");
    // Keep the value usable at narrow Inspector widths; actions have their own row.
    layout->addWidget(editor);
    auto* actions=new QHBoxLayout;actions->addStretch(1);
    actions->addWidget(apply);actions->addWidget(cancel);layout->addLayout(actions);
    auto* status=new QLabel(box);status->setObjectName("operation-enabled-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_editor(editor);const QPointer<QPushButton> safe_apply(apply);
    const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This operation selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Operations changed; refresh before applying enabled");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying enabled");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    int initial_index=-1;
    try{
        const auto& document=current_document();const auto& first=target_operation(document,targets.front());
        const auto* descriptor=builtin_operation_type(first.type);
        box->setTitle(QString::fromStdString(descriptor->label)+QString(" enabled · %1 selected targets").arg(targets.size()));
        if(unchanged_enabled(document,targets,first.enabled)){
            state->setText(first.enabled?"Common: Enabled":"Common: Bypassed");
            initial_index=editor->findData(first.enabled?"enable":"bypass");
        }else{
            state->setText("Mixed");editor->insertItem(0,"Mixed",QString{});initial_index=0;
        }
        editor->setCurrentIndex(initial_index);
    }catch(const std::exception& error){
        state->setText("Unavailable");editor->setCurrentIndex(-1);editor->setEnabled(false);show_error(status,error);
    }
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply||!safe_status)return;
        safe_apply->setEnabled(false);
        try{
            const auto& document=current_document();const auto choice=safe_editor->currentData().toString();
            safe_status->clear();
            safe_apply->setEnabled((choice=="enable"||choice=="bypass")&&!unchanged_enabled(document,targets,choice=="enable"));
        }catch(const std::exception& error){safe_editor->setEnabled(false);show_error(safe_status.data(),error);}
    };
    QObject::connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor||!safe_apply)return;
        {const QSignalBlocker blocker(safe_editor.data());safe_editor->setCurrentIndex(initial_index);}
        update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& document=current_document();const auto choice=safe_editor->currentData().toString();
            if(choice!="enable"&&choice!="bypass")
                throw Error("INVALID_OPERATION_ENABLED","Choose Enable or Bypass before applying enabled");
            const bool value=choice=="enable";
            if(unchanged_enabled(document,targets,value))return;
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& target:targets)commands.push_back(EnableOperation{target.object,target.operation,value});
            safe_host->session.apply(commands,revision);
            // edited() can synchronously rebuild the Inspector and delete this control.
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    return box;
}
}
