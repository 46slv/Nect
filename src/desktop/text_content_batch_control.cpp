#include "text_content_batch_control.hpp"
#include "host.hpp"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSignalBlocker>
#include <QPushButton>
#include <QVBoxLayout>
#include <set>
#include <memory>

namespace nect::desktop {
namespace {
void require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Text objects for content");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate Text content target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::text||!found->second.text)
            throw Error("TYPE_MISMATCH","Every content target must be an editable Text object: "+id);
        if(found->second.text->content_driver)
            throw Error("DRIVEN_PROPERTY","Text content is linked on "+id+"; unlink it explicitly before a batch content edit");
    }
}
bool unchanged_content(const Document& document,const std::vector<Id>& targets,const std::string& value){
    for(const auto& id:targets)if(document.objects.at(id).text->content!=value)return false;
    return true;
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_text_content_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Text content · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("text-content-literal-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("text-content-literal-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* row=new QHBoxLayout;
    auto* editor=new QPlainTextEdit(box);editor->setObjectName("text-content-literal-editor");
    editor->setMaximumHeight(140);editor->setAccessibleName("Literal content for every selected Text object");
    editor->setToolTip("Enter literal text, then Apply to every retained Text object. Other Text properties remain unchanged.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("text-content-literal-apply");
    apply->setToolTip("Sets every retained Text content in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("text-content-literal-cancel");
    cancel->setToolTip("Discard the content draft without editing the document.");cancel->setEnabled(false);
    row->addWidget(editor,1);row->addWidget(apply);row->addWidget(cancel);layout->addLayout(row);
    auto* clear=new QPushButton("Clear draft",box);clear->setObjectName("text-content-literal-clear");
    clear->setToolTip("Stage empty text for every selected Text object; Apply commits it.");layout->addWidget(clear);
    auto* status=new QLabel(box);status->setObjectName("text-content-literal-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QPlainTextEdit> safe_editor(editor);const QPointer<QPushButton> safe_apply(apply),safe_cancel(cancel);
    const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Text changed; refresh before applying content");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying content");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    QString initial_text;std::string initial_raw;bool shared=false;
    try{
        const auto& document=current_document();const auto& first=document.objects.at(targets.front()).text->content;
        if(unchanged_content(document,targets,first)){
            shared=true;initial_raw=first;initial_text=QString::fromStdString(first);state->setText("Shared content");editor->setPlainText(initial_text);
        }else{
            state->setText("Mixed");editor->setPlaceholderText("Mixed");
        }
    }catch(const std::exception& error){
        state->setText("Unavailable");editor->setEnabled(false);show_error(status,error);
    }
    const auto initial_display=editor->toPlainText();
    const auto drafted=std::make_shared<bool>(false);
    const auto value=[=]{
        const auto displayed=safe_editor->toPlainText();
        // QTextDocument normalizes line endings. Restoring an unchanged shared
        // display must not rewrite the retained source bytes.
        return shared&&displayed==initial_display?initial_raw:displayed.toStdString();
    };
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply||!safe_cancel||!safe_status)return;
        safe_apply->setEnabled(false);safe_cancel->setEnabled(*drafted);
        try{
            const auto& document=current_document();safe_status->clear();
            // No trimming or forced single line. New drafts use Qt plain-text
            // paragraph representation; untouched source bytes remain unchanged.
            // UpdateText owns validation on Apply.
            const bool changed=*drafted&&!unchanged_content(document,targets,value());
            safe_apply->setEnabled(changed);
            safe_cancel->setEnabled(*drafted);
        }catch(const std::exception& error){safe_editor->setEnabled(false);show_error(safe_status.data(),error);}
    };
    QObject::connect(editor,&QPlainTextEdit::textChanged,box,[update_draft,drafted]{*drafted=true;update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        *drafted=false;const QSignalBlocker blocker(safe_editor.data());
        safe_editor->setPlainText(initial_text);
        if(safe_apply)safe_apply->setEnabled(false);
        if(safe_cancel)safe_cancel->setEnabled(false);
        if(safe_status)safe_status->clear();
    });
    QObject::connect(clear,&QPushButton::clicked,box,[=]{
        if(!safe_editor||!safe_editor->isEnabled())return;
        *drafted=true;safe_editor->setPlainText(QString{});update_draft();
        if(safe_cancel)safe_cancel->setEnabled(true);
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor||!*drafted)return;
        try{
            const auto& document=current_document();const auto authored=value();
            if(unchanged_content(document,targets,authored))return;
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& id:targets){
                auto source=*document.objects.at(id).text;source.content=authored;
                commands.push_back(UpdateText{id,std::move(source)});
            }
            safe_host->session.apply(commands,revision);
            // edited() may synchronously rebuild the Inspector and delete this control.
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    return box;
}
}
