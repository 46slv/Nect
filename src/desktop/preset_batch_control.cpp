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
}

QWidget* make_preset_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent) {
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
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
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document& {
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Preset selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Document changed; refresh before applying a Preset");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying a Preset");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    const auto update_draft=[=] {
        if(!safe_box||!safe_catalog||!safe_apply||!safe_status)return;
        safe_apply->setEnabled(false);
        try {
            const auto& document=current_document();
            const auto preset=safe_catalog->currentData().toString().toStdString();
            if(!preset.empty()&&!document.preset_definitions.contains(preset))throw Error("MISSING_PRESET",preset);
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
