#include "multi_blend_mode_control.hpp"
#include "host.hpp"
#include "nect/blend.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>
#include <set>

namespace nect::desktop {
namespace {
void require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct whole objects for blend mode");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate blend mode target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        const auto& object=found->second;
        if(object.kind!=Kind::group&&object.kind!=Kind::path&&object.kind!=Kind::text&&
           object.kind!=Kind::image&&object.kind!=Kind::instance)
            throw Error("UNSUPPORTED_COMPOSITING","Blend mode requires an authored whole object: "+id);
        if(object.compositing.version!=1)
            throw Error("UNSUPPORTED_COMPOSITING_VERSION","Unsupported compositing on "+id);
        if(!find_blend_mode(object.compositing.blend))
            throw Error("UNSUPPORTED_BLEND","Unsupported blend mode on "+id+": "+object.compositing.blend);
    }
}
bool unchanged_blend(const Document& document,const std::vector<Id>& targets,const std::string& value){
    for(const auto& id:targets)if(document.objects.at(id).compositing.blend!=value)return false;
    return true;
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
struct Context {
    QPointer<Host> host;
    QString session;
    Id document;
    std::uint64_t revision,gesture;
    std::vector<Id> targets;
    const Document& current_document()const{
        if(!host||host->session_id!=session||host->session.document().id!=document)
            throw Error("SESSION_CONFLICT","This blend selection belongs to another document");
        if(host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Artwork changed; refresh before applying blend mode");
        if(host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying blend mode");
        const auto& current=host->session.document();require_targets(current,targets);return current;
    }
};
}

QWidget* make_multi_blend_mode_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    // One small frozen identity/target context, shared by the callbacks. No
    // Document snapshots or evaluated isolation values belong to the widgets.
    const auto context=std::make_shared<const Context>(Context{QPointer<Host>(&host),host.session_id,
        host.session.document().id,host.session.revision(),host.session.gesture_generation(),targets});
    auto* box=new QGroupBox(QString("Blend mode · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("multi-blend-mode-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("multi-blend-mode-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* row=new QHBoxLayout;
    auto* selector=new QComboBox(box);selector->setObjectName("multi-blend-mode-selector");
    selector->setAccessibleName("Blend mode for every selected object");
    selector->setToolTip("Select a supported blend mode, then Apply. Each object's isolation, opacity and mask stay unchanged.");
    selector->addItem("Mixed",QString{});
    for(const auto& mode:blend_modes())
        selector->addItem(QString::fromUtf8(mode.label.data(),static_cast<int>(mode.label.size())),
            QString::fromLatin1(mode.id.data(),static_cast<int>(mode.id.size())));
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("multi-blend-mode-apply");
    apply->setToolTip("Sets every retained object's blend mode in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("multi-blend-mode-cancel");
    cancel->setToolTip("Discard the blend draft without editing the document.");cancel->setEnabled(false);
    row->addWidget(selector,1);row->addWidget(apply);row->addWidget(cancel);layout->addLayout(row);
    auto* status=new QLabel(box);status->setObjectName("multi-blend-mode-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    int initial_index=0;
    try{
        const auto& document=context->current_document();const auto& first=document.objects.at(targets.front()).compositing.blend;
        if(unchanged_blend(document,targets,first)){
            initial_index=selector->findData(QString::fromStdString(first));
            selector->setCurrentIndex(initial_index);state->setText("Shared: "+selector->currentText());
        }else state->setText("Mixed");
    }catch(const std::exception& error){state->setText("Unavailable");selector->setEnabled(false);show_error(status,error);}

    const QPointer<QGroupBox> safe_box(box);const QPointer<QComboBox> safe_selector(selector);
    const QPointer<QPushButton> safe_apply(apply),safe_cancel(cancel);const QPointer<QLabel> safe_status(status);
    const auto update_draft=[=]{
        if(!safe_box||!safe_selector||!safe_apply||!safe_cancel||!safe_status)return;
        safe_apply->setEnabled(false);safe_cancel->setEnabled(safe_selector->currentIndex()!=initial_index);
        try{
            const auto& document=context->current_document();safe_status->clear();
            const auto value=safe_selector->currentData().toString().toStdString();
            if(value.empty())return; // Mixed describes state, never an authored mode.
            if(!find_blend_mode(value))throw Error("UNSUPPORTED_BLEND",value);
            safe_apply->setEnabled(!unchanged_blend(document,context->targets,value));
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    };
    QObject::connect(selector,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_selector)return;
        safe_selector->setCurrentIndex(initial_index);update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_selector)return;
        try{
            const auto& document=context->current_document();
            const auto value=safe_selector->currentData().toString().toStdString();
            if(value.empty())return;
            if(!find_blend_mode(value))throw Error("UNSUPPORTED_BLEND",value);
            if(unchanged_blend(document,context->targets,value))return;
            std::vector<Command> commands;commands.reserve(context->targets.size());
            for(const auto& id:context->targets){
                // SetCompositing also takes isolation: carry each target's own
                // authored literal, including when its evaluated driver differs.
                commands.push_back(SetCompositing{id,value,document.objects.at(id).compositing.isolated});
            }
            context->host->session.apply(commands,context->revision);
            // edited() may synchronously rebuild and destroy the entire panel.
            context->host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    return box;
}
}
