#include "multi_visibility_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <set>

namespace nect::desktop {
namespace {
void require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct authored objects for visibility");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate visibility target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.visibility_driver||found->second.visibility_expression)
            throw Error("DRIVEN_PROPERTY","Visibility is linked or expression-driven on "+id+"; unlink it explicitly before a multi-object visibility edit");
    }
}
bool unchanged_visibility(const Document& document,const std::vector<Id>& targets,bool visible){
    for(const auto& id:targets)if(document.objects.at(id).visible!=visible)return false;
    return true;
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_multi_visibility_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Object visibility · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("multi-visibility-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("multi-visibility-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* editor=new QComboBox(box);editor->setObjectName("multi-visibility-editor");
    editor->setAccessibleName("Own visibility for every selected object");
    editor->addItem("Show","show");editor->addItem("Hide","hide");
    editor->setToolTip("Choose Show or Hide, then Apply to every captured authored object. Hidden ancestors still suppress shown children.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("multi-visibility-apply");
    apply->setToolTip("Sets every captured object's own visibility in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("multi-visibility-cancel");
    cancel->setToolTip("Discard the visibility draft without changing the document or Undo history.");cancel->setEnabled(false);
    // Keep the value usable at narrow Inspector widths; actions have their own row.
    layout->addWidget(editor);
    auto* actions=new QHBoxLayout;actions->addStretch(1);
    actions->addWidget(apply);actions->addWidget(cancel);layout->addLayout(actions);
    auto* note=new QLabel("Own visibility only. Hidden ancestors still apply.",box);
    note->setWordWrap(true);layout->addWidget(note);
    auto* status=new QLabel(box);status->setObjectName("multi-visibility-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_editor(editor);const QPointer<QPushButton> safe_apply(apply),safe_cancel(cancel);
    const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This object selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Objects changed; refresh before applying visibility");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying visibility");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    try{
        const auto& document=current_document();const auto first=document.objects.at(targets.front()).visible;
        if(unchanged_visibility(document,targets,first)){
            state->setText(first?"Common: Shown":"Common: Hidden");
            editor->setCurrentIndex(editor->findData(first?QStringLiteral("show"):QStringLiteral("hide")));
        }else{
            state->setText("Mixed");editor->insertItem(0,"Mixed",QString{});editor->setCurrentIndex(0);
        }
    }catch(const std::exception& error){
        state->setText("Unavailable");editor->setCurrentIndex(-1);editor->setEnabled(false);show_error(status,error);
    }
    const int initial_index=editor->currentIndex();
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply||!safe_cancel||!safe_status)return;
        safe_apply->setEnabled(false);safe_cancel->setEnabled(safe_editor->currentIndex()!=initial_index);
        try{
            const auto& document=current_document();const auto choice=safe_editor->currentData().toString();
            safe_status->clear();
            safe_apply->setEnabled((choice=="show"||choice=="hide")&&!unchanged_visibility(document,targets,choice=="show"));
        }catch(const std::exception& error){safe_editor->setEnabled(false);show_error(safe_status.data(),error);}
    };
    QObject::connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        {const QSignalBlocker blocker(safe_editor.data());safe_editor->setCurrentIndex(initial_index);}
        update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& document=current_document();const auto choice=safe_editor->currentData().toString();
            if(choice!="show"&&choice!="hide")throw Error("INVALID_VISIBILITY","Choose Show or Hide before applying visibility");
            const bool visible=choice=="show";
            if(unchanged_visibility(document,targets,visible))return;
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& id:targets)commands.push_back(SetVisibility{id,visible});
            safe_host->session.apply(commands,revision);
            safe_apply->setEnabled(false);safe_cancel->setEnabled(false);
            // edited() can synchronously rebuild the Inspector and delete this control.
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    QObject::connect(&host,&QObject::destroyed,box,[=]{
        if(!safe_box)return;
        safe_editor->setEnabled(false);safe_apply->setEnabled(false);safe_cancel->setEnabled(false);
        safe_status->setText("SESSION_CONFLICT: The owning document is no longer open");
    });
    return box;
}
}
