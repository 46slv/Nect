#include "composite_isolation_batch_control.hpp"
#include "host.hpp"
#include "nect/blend.hpp"
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>
#include <set>

namespace nect::desktop {
namespace {
// Mixed is presentation only. Mouse, Space and click() choose a definite bool.
class IsolationChoice final : public QCheckBox {
public:
    explicit IsolationChoice(QWidget* parent):QCheckBox("Isolate from backdrop",parent){}
protected:
    void nextCheckState() override {
        const auto next=checkState()==Qt::Checked?Qt::Unchecked:Qt::Checked;
        setTristate(false);setCheckState(next);
    }
};
void require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct whole objects for Composite isolation");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate Composite isolation target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        const auto& object=found->second;
        if(object.kind!=Kind::group&&object.kind!=Kind::path&&object.kind!=Kind::text&&
           object.kind!=Kind::image&&object.kind!=Kind::instance)
            throw Error("UNSUPPORTED_COMPOSITING","Composite isolation requires an authored whole object: "+id);
        if(object.compositing.version!=1)
            throw Error("UNSUPPORTED_COMPOSITING_VERSION","Unsupported compositing on "+id);
        if(!find_blend_mode(object.compositing.blend))
            throw Error("UNSUPPORTED_BLEND","Unsupported blend mode on "+id+": "+object.compositing.blend);
    }
}
bool driven(const Compositing& value){return value.isolated_driver.has_value()||value.isolated_expression.has_value();}
void require_literal_targets(const Document& document,const std::vector<Id>& targets){
    for(const auto& id:targets)if(driven(document.objects.at(id).compositing))
        throw Error("DRIVEN_PROPERTY","Composite isolation is linked or expression-driven on "+id+
            "; use Unlink sources explicitly before applying an isolation literal");
}
bool unchanged_isolation(const Document& document,const std::vector<Id>& targets,bool value){
    for(const auto& id:targets)if(document.objects.at(id).compositing.isolated!=value)return false;
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
            throw Error("SESSION_CONFLICT","This Composite isolation selection belongs to another document");
        if(host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Artwork changed; refresh before editing Composite isolation");
        if(host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before editing Composite isolation");
        const auto& current=host->session.document();require_targets(current,targets);return current;
    }
};
}

QWidget* make_composite_isolation_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    // Widgets retain identity and draft state, never canonical/evaluated values.
    const auto context=std::make_shared<const Context>(Context{QPointer<Host>(&host),host.session_id,
        host.session.document().id,host.session.revision(),host.session.gesture_generation(),targets});
    auto* box=new QGroupBox(QString("Composite isolation · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("composite-isolation-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("composite-isolation-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* row=new QHBoxLayout;
    auto* editor=new IsolationChoice(box);editor->setObjectName("composite-isolation-batch-editor");
    editor->setAccessibleName("Isolate every selected object from backdrop");
    editor->setToolTip("Shared/Mixed describes evaluated isolation. Click or press Space to choose On or Off, then Apply to every retained object.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("composite-isolation-batch-apply");
    apply->setToolTip("Changes only isolation on every retained object in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("composite-isolation-batch-cancel");
    cancel->setToolTip("Discard the isolation draft without editing the document.");cancel->setEnabled(false);
    row->addWidget(editor,1);row->addWidget(apply);row->addWidget(cancel);layout->addLayout(row);
    auto* source_note=new QLabel(box);source_note->setObjectName("composite-isolation-batch-sources");
    source_note->setTextFormat(Qt::PlainText);source_note->setWordWrap(true);layout->addWidget(source_note);
    auto* unlink=new QPushButton("Unlink sources",box);unlink->setObjectName("composite-isolation-batch-unlink");
    unlink->setToolTip("Explicitly unlink only retained isolation sources, freezing each object's own evaluated isolation in one Undo step.");
    unlink->setEnabled(false);layout->addWidget(unlink);
    auto* status=new QLabel(box);status->setObjectName("composite-isolation-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    Qt::CheckState initial_state=Qt::Unchecked;
    try{
        const auto& document=context->current_document();
        const auto values=evaluate_composite_isolations(document);const auto first=values.at(targets.front());
        if(std::all_of(targets.begin(),targets.end(),[&](const auto& id){return values.at(id)==first;})){
            initial_state=first?Qt::Checked:Qt::Unchecked;state->setText(first?"Shared: On":"Shared: Off");
        }else{
            initial_state=Qt::PartiallyChecked;editor->setTristate(true);state->setText("Mixed");
        }
        editor->setCheckState(initial_state);
        std::size_t source_count=0;
        for(const auto& id:targets)if(driven(document.objects.at(id).compositing))++source_count;
        if(source_count){
            source_note->setText(QString("Isolation sources: %1 of %2 selected objects. Unlink sources freezes each evaluated value; resulting values may remain Mixed.")
                .arg(source_count).arg(targets.size()));
            editor->setEnabled(false);unlink->setEnabled(true);
            require_literal_targets(document,targets);
        }else source_note->setText("Authored isolation only. Blend mode, opacity, masks and other sources stay unchanged.");
    }catch(const Error& error){
        if(error.code!="DRIVEN_PROPERTY"){state->setText("Unavailable");editor->setEnabled(false);unlink->setEnabled(false);}
        show_error(status,error);
    }catch(const std::exception& error){state->setText("Unavailable");editor->setEnabled(false);show_error(status,error);}

    const QPointer<QGroupBox> safe_box(box);const QPointer<IsolationChoice> safe_editor(editor);
    const QPointer<QPushButton> safe_apply(apply),safe_cancel(cancel),safe_unlink(unlink);const QPointer<QLabel> safe_status(status);
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply||!safe_cancel||!safe_unlink||!safe_status)return;
        safe_apply->setEnabled(false);safe_cancel->setEnabled(safe_editor->checkState()!=initial_state);
        try{
            const auto& document=context->current_document();require_literal_targets(document,context->targets);
            safe_status->clear();const auto choice=safe_editor->checkState();
            safe_apply->setEnabled(choice!=Qt::PartiallyChecked&&!unchanged_isolation(document,context->targets,choice==Qt::Checked));
        }catch(const std::exception& error){safe_editor->setEnabled(false);show_error(safe_status.data(),error);}
    };
    // stateChanged is supported on the minimum Qt 6.5, unlike checkStateChanged.
    QObject::connect(editor,&QCheckBox::stateChanged,box,[update_draft](int){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        {const QSignalBlocker blocker(safe_editor.data());safe_editor->setTristate(initial_state==Qt::PartiallyChecked);safe_editor->setCheckState(initial_state);}
        update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& document=context->current_document();require_literal_targets(document,context->targets);
            const auto choice=safe_editor->checkState();if(choice==Qt::PartiallyChecked)return;
            const bool value=choice==Qt::Checked;
            if(unchanged_isolation(document,context->targets,value))return;
            std::vector<Command> commands;commands.reserve(context->targets.size());
            for(const auto& id:context->targets)
                commands.push_back(SetCompositing{id,document.objects.at(id).compositing.blend,value});
            context->host->session.apply(commands,context->revision);
            // edited() can synchronously rebuild the Inspector and delete us.
            context->host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    QObject::connect(unlink,&QPushButton::clicked,box,[=]{
        if(!safe_box)return;
        try{
            const auto& document=context->current_document();std::vector<Command> commands;commands.reserve(context->targets.size());
            for(const auto& id:context->targets)if(driven(document.objects.at(id).compositing))
                commands.push_back(UnlinkCompositeIsolated{{id,"","composite.isolated"}});
            if(commands.empty())return;
            // Existing core evaluation freezes each value, including dependencies
            // between selected sources. No literal Apply is hidden in this action.
            context->host->session.apply(commands,context->revision);
            context->host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    QObject::connect(&host,&QObject::destroyed,box,[=]{
        if(!safe_box)return;
        safe_editor->setEnabled(false);safe_apply->setEnabled(false);safe_cancel->setEnabled(false);safe_unlink->setEnabled(false);
        safe_status->setText("SESSION_CONFLICT: The owning document is no longer open");
    });
    return box;
}
}
