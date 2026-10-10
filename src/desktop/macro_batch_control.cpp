#include "macro_batch_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <set>
#include <memory>

namespace nect::desktop {
namespace {
void require_targets(const Document& document,const std::vector<Id>& targets) {
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Path or Text objects");
    std::set<Id> unique;
    for(const auto& id:targets) {
        if(!unique.insert(id).second)throw Error("INVALID_SELECTION","Duplicate Macro target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::path&&found->second.kind!=Kind::text)
            throw Error("INVALID_DOMAIN","Every Macro target must be a Path or Text object: "+id);
    }
}
void show_error(QLabel* status,const std::exception& error) {
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
const MacroDefinition& require_definition(const Document& document,const Id& id) {
    const auto found=document.macro_definitions.find(id);
    if(found==document.macro_definitions.end())throw Error("MISSING_MACRO_DEFINITION",id);
    return found->second;
}
std::uint64_t require_pin(const MacroDefinition& definition,const QComboBox& pins) {
    bool valid=false;const auto pin=pins.currentData().toULongLong(&valid);
    if(!valid||!definition.revisions.contains(pin))
        throw Error("MISSING_MACRO_REVISION","Choose a retained revision of "+definition.id);
    validate_macro_definition(definition);return pin;
}
}

QWidget* make_macro_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent) {
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    const auto frozen_targets=std::make_shared<std::map<Id,Object>>();
    const auto frozen_macro_sources=std::make_shared<std::map<std::pair<Id,std::uint64_t>,MacroDefinitionRevision>>();
    for(const auto& id:targets) {
        const auto found=host.session.document().objects.find(id);
        if(found==host.session.document().objects.end())continue; // Existing refusal owns missing targets.
        frozen_targets->emplace(id,found->second);
        for(const auto& operation:found->second.stack)if(operation.macro) {
            const auto definition=host.session.document().macro_definitions.find(operation.macro->definition);
            if(definition==host.session.document().macro_definitions.end())continue;
            const auto graph=definition->second.revisions.find(operation.macro->pinned_revision);
            if(graph!=definition->second.revisions.end())
                frozen_macro_sources->emplace(std::pair{definition->first,graph->first},graph->second);
        }
    }
    struct SourceDraft {Id definition;std::uint64_t pin=0;MacroDefinitionRevision graph;bool loaded=false;};
    const auto source_draft=std::make_shared<SourceDraft>();
    auto* box=new QGroupBox("Apply Macro to Selection",parent);box->setObjectName("macro-batch-panel");
    auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(QString("%1 retained objects").arg(targets.size()),box);
    state->setObjectName("macro-batch-state");state->setTextFormat(Qt::PlainText);state->setWordWrap(true);
    layout->addWidget(state);
    auto* form=new QFormLayout;
    auto* catalog=new QComboBox(box);catalog->setObjectName("macro-batch-catalog");
    catalog->setAccessibleName("Document Macro Definition for every selected Path or Text object");
    catalog->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    catalog->setMinimumContentsLength(12);catalog->addItem("Choose a document Macro…",QString{});
    for(const auto& [id,definition]:host.session.document().macro_definitions) {
        catalog->addItem(QString::fromStdString(definition.label)+" ["+QString::fromStdString(id)+"]",
            QString::fromStdString(id));
        catalog->setItemData(catalog->count()-1,QString::fromStdString(id),Qt::ToolTipRole);
    }
    form->addRow("Definition",catalog);
    auto* pins=new QComboBox(box);pins->setObjectName("macro-batch-revision");
    pins->setAccessibleName("Retained Macro revision pin for every selected object");
    pins->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    pins->setMinimumContentsLength(8);form->addRow("Revision",pins);layout->addLayout(form);
    auto* actions=new QHBoxLayout;actions->addStretch(1);
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("macro-batch-apply");
    apply->setToolTip("Append a fresh Macro instance with this pin to every retained target in one Undo step.");
    apply->setEnabled(false);actions->addWidget(apply);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("macro-batch-cancel");actions->addWidget(cancel);
    layout->addLayout(actions);
    auto* status=new QLabel(box);status->setObjectName("macro-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_catalog(catalog),safe_pins(pins);
    const QPointer<QPushButton> safe_apply(apply);const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets,frozen_targets,frozen_macro_sources]()->const Document& {
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Macro selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Document changed; refresh before applying a Macro");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying a Macro");
        const auto& document=safe_host->session.document();require_targets(document,targets);
        for(const auto& [id,object]:*frozen_targets)if(document.objects.at(id)!=object)
            throw Error("PROPERTY_CONFLICT","The retained Macro target changed; refresh before applying");
        for(const auto& [key,graph]:*frozen_macro_sources) {
            const auto definition=document.macro_definitions.find(key.first);
            if(definition==document.macro_definitions.end()||!definition->second.revisions.contains(key.second)||
               definition->second.revisions.at(key.second)!=graph)
                throw Error("PROPERTY_CONFLICT","The retained target Macro source changed; refresh before applying");
        }
        return document;
    };
    const auto update_draft=[=] {
        if(!safe_box||!safe_catalog||!safe_pins||!safe_apply||!safe_status)return;
        safe_apply->setEnabled(false);
        try {
            const auto& document=current_document();
            const auto id=safe_catalog->currentData().toString().toStdString();
            safe_pins->setEnabled(!id.empty());
            source_draft->loaded=false;
            if(!id.empty()) {
                const auto& definition=require_definition(document,id);
                const auto pin=require_pin(definition,*safe_pins);
                *source_draft={id,pin,definition.revisions.at(pin),true};
            }
            safe_status->setText(document.macro_definitions.empty()?"No document Macros. Create a Macro Definition first.":
                "Apply appends this pinned Macro to all retained objects.");
            safe_apply->setEnabled(!id.empty());
        } catch(const std::exception& error) {show_error(safe_status.data(),error);}
    };
    const auto load_pins=[=] {
        if(!safe_catalog||!safe_pins||!safe_apply)return;
        safe_apply->setEnabled(false);const QSignalBlocker blocker(safe_pins.data());safe_pins->clear();
        source_draft->loaded=false;
        try {
            const auto& document=current_document();
            const auto id=safe_catalog->currentData().toString().toStdString();
            if(!id.empty()) {
                const auto& definition=require_definition(document,id);
                for(const auto& [pin,graph]:definition.revisions) {
                    (void)graph;
                    safe_pins->addItem(QString("Revision %1%2").arg(static_cast<qulonglong>(pin))
                        .arg(pin==definition.latest_revision?" (latest)":""),QVariant::fromValue(static_cast<qulonglong>(pin)));
                }
                const auto latest=safe_pins->findData(QVariant::fromValue(static_cast<qulonglong>(definition.latest_revision)));
                safe_pins->setCurrentIndex(latest); // -1 explicitly represents an unavailable latest pin.
            }
            update_draft();
        } catch(const std::exception& error) {safe_pins->setEnabled(false);show_error(safe_status.data(),error);}
    };
    QObject::connect(catalog,qOverload<int>(&QComboBox::currentIndexChanged),box,[load_pins](int){load_pins();});
    QObject::connect(pins,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[safe_catalog,load_pins] {
        if(!safe_catalog)return;safe_catalog->setCurrentIndex(0);load_pins();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=] {
        if(!safe_box||!safe_catalog||!safe_pins)return;
        try {
            const auto& document=current_document();
            const auto id=safe_catalog->currentData().toString().toStdString();
            if(id.empty())throw Error("NO_SELECTION","Choose a document Macro before applying");
            const auto pin=require_pin(require_definition(document,id),*safe_pins);
            if(!source_draft->loaded||source_draft->definition!=id||source_draft->pin!=pin||
               source_draft->graph!=document.macro_definitions.at(id).revisions.at(pin))
                throw Error("PROPERTY_CONFLICT","The selected Macro revision changed; choose it again before applying");
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& target:targets)
                commands.push_back(MacroCommand{InstantiateMacro{target,id,new_id(),pin,document.objects.at(target).stack.size()}});
            safe_host->session.apply(commands,revision);
            // Inspector refresh can synchronously destroy all these controls.
            safe_host->edited();return;
        } catch(const std::exception& error) {show_error(safe_status.data(),error);}
    });
    load_pins();return box;
}
}
