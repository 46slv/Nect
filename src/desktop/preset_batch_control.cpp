#include "preset_batch_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
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
        if(!unique.insert(id).second)throw Error("INVALID_SELECTION","Duplicate Preset target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::path&&found->second.kind!=Kind::text)
            throw Error("INVALID_DOMAIN","Every Preset target must be a Path or Text object: "+id);
    }
}
void show_error(QLabel* status,const std::exception& error) {
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
using MacroSources=std::map<std::pair<Id,std::uint64_t>,std::optional<MacroDefinitionRevision>>;
std::optional<MacroDefinitionRevision> macro_source(const Document& document,const std::pair<Id,std::uint64_t>& key) {
    const auto definition=document.macro_definitions.find(key.first);
    if(definition==document.macro_definitions.end())return std::nullopt;
    const auto revision=definition->second.revisions.find(key.second);
    if(revision==definition->second.revisions.end())return std::nullopt;
    return revision->second;
}
void retain_macro_source(MacroSources& sources,const Document& document,const Id& definition,std::uint64_t pin) {
    const auto key=std::pair{definition,pin};sources.emplace(key,macro_source(document,key));
}
bool same_macro_sources(const MacroSources& sources,const Document& document) {
    for(const auto& [key,revision]:sources)if(macro_source(document,key)!=revision)return false;
    return true;
}
}

QWidget* make_preset_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent) {
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    const auto frozen_targets=std::make_shared<std::map<Id,Object>>();
    const auto frozen_macro_sources=std::make_shared<MacroSources>();
    for(const auto& id:targets) {
        const auto found=host.session.document().objects.find(id);
        if(found==host.session.document().objects.end())continue; // Existing missing-target refusal remains authoritative.
        frozen_targets->emplace(id,found->second);
        for(const auto& operation:found->second.stack)if(operation.macro)
            retain_macro_source(*frozen_macro_sources,host.session.document(),operation.macro->definition,operation.macro->pinned_revision);
    }
    struct SourceDraft {PresetDefinition definition;MacroSources sources;bool loaded=false;};
    const auto source_draft=std::make_shared<SourceDraft>();
    auto* box=new QGroupBox("Apply Preset to Selection",parent);box->setObjectName("preset-batch-panel");
    auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(QString("%1 retained objects").arg(targets.size()),box);
    state->setObjectName("preset-batch-state");state->setTextFormat(Qt::PlainText);state->setWordWrap(true);
    layout->addWidget(state);
    auto* catalog=new QComboBox(box);catalog->setObjectName("preset-batch-catalog");
    catalog->setAccessibleName("Document Preset for every selected Path or Text object");
    catalog->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    catalog->setMinimumContentsLength(12);
    catalog->addItem("Choose a document Preset…",QString{});
    for(const auto& [id,definition]:host.session.document().preset_definitions) {
        catalog->addItem(QString::fromStdString(definition.label),QString::fromStdString(id));
        catalog->setItemData(catalog->count()-1,QString::fromStdString(id),Qt::ToolTipRole);
    }
    layout->addWidget(catalog);
    auto* actions=new QHBoxLayout;actions->addStretch(1);
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("preset-batch-apply");
    apply->setToolTip("Append fresh processing entries to every retained target in one Undo step.");
    apply->setEnabled(false);actions->addWidget(apply);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("preset-batch-cancel");actions->addWidget(cancel);
    layout->addLayout(actions);
    auto* status=new QLabel(box);status->setObjectName("preset-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_catalog(catalog);const QPointer<QPushButton> safe_apply(apply);
    const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets,frozen_targets,frozen_macro_sources]()->const Document& {
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Preset selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Document changed; refresh before applying a Preset");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying a Preset");
        const auto& document=safe_host->session.document();require_targets(document,targets);
        for(const auto& [id,object]:*frozen_targets)if(document.objects.at(id)!=object)
            throw Error("PROPERTY_CONFLICT","The retained Preset target changed; refresh before applying");
        if(!same_macro_sources(*frozen_macro_sources,document))
            throw Error("PROPERTY_CONFLICT","The retained target Macro source changed; refresh before applying");
        return document;
    };
    const auto update_draft=[=] {
        if(!safe_box||!safe_catalog||!safe_apply||!safe_status)return;
        safe_apply->setEnabled(false);
        source_draft->loaded=false;
        try {
            const auto& document=current_document();
            const auto preset=safe_catalog->currentData().toString().toStdString();
            if(!preset.empty()&&!document.preset_definitions.contains(preset))throw Error("MISSING_PRESET",preset);
            if(!preset.empty()) {
                source_draft->definition=document.preset_definitions.at(preset);source_draft->sources.clear();
                for(const auto& entry:source_draft->definition.entries)if(entry.kind=="macro")
                    retain_macro_source(source_draft->sources,document,entry.macro_definition,entry.pinned_revision);
                source_draft->loaded=true;
            }
            safe_status->setText(document.preset_definitions.empty()?"No document Presets. Save a supported stack as a Preset first.":
                "Apply appends the Preset to all retained objects.");
            safe_apply->setEnabled(!preset.empty());
        } catch(const std::exception& error) {safe_catalog->setEnabled(false);show_error(safe_status.data(),error);}
    };
    QObject::connect(catalog,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[safe_catalog,update_draft] {
        if(!safe_catalog)return;safe_catalog->setCurrentIndex(0);update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=] {
        if(!safe_box||!safe_catalog)return;
        try {
            const auto& document=current_document();
            const auto preset=safe_catalog->currentData().toString().toStdString();
            if(preset.empty())throw Error("NO_SELECTION","Choose a document Preset before applying");
            if(!document.preset_definitions.contains(preset))throw Error("MISSING_PRESET",preset);
            if(!source_draft->loaded||source_draft->definition!=document.preset_definitions.at(preset)||
               !same_macro_sources(source_draft->sources,document))
                throw Error("PROPERTY_CONFLICT","The selected Preset or its Macro source changed; choose it again before applying");
            ApplyPresetBatch batch;batch.preset=preset;batch.targets.reserve(targets.size());
            for(const auto& id:targets)batch.targets.push_back({id,new_id()});
            safe_host->session.apply_preset_command(PresetCommand{std::move(batch)},revision);
            // edited() can synchronously delete this control while refreshing Inspector.
            safe_host->edited();return;
        } catch(const std::exception& error) {show_error(safe_status.data(),error);}
    });
    update_draft();return box;
}
}
