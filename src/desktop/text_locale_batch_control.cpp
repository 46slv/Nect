#include "text_locale_batch_control.hpp"
#include "host.hpp"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <set>

namespace nect::desktop {
namespace {
void require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Text objects for locale");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate Text locale target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::text||!found->second.text)
            throw Error("TYPE_MISMATCH","Every locale target must be an editable Text object: "+id);
        if(found->second.text->locale_driver)
            throw Error("DRIVEN_PROPERTY","Text locale is linked on "+id+"; unlink it explicitly before a batch locale edit");
    }
}
bool unchanged_locale(const Document& document,const std::vector<Id>& targets,const std::string& value){
    for(const auto& id:targets)if(document.objects.at(id).text->locale!=value)return false;
    return true;
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_text_locale_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Text locale · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("text-locale-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("text-locale-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* editor=new QLineEdit(box);editor->setObjectName("text-locale-batch-editor");
    editor->setAccessibleName("Language tag for every selected Text object");
    editor->setToolTip("Enter a locale, then Apply to every retained Text object. The core validates the exact text.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("text-locale-batch-apply");
    apply->setToolTip("Sets every retained Text locale in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("text-locale-batch-cancel");
    cancel->setToolTip("Discard the locale draft without editing the document.");cancel->setEnabled(false);
    // Keep the value usable at narrow Inspector widths; actions have their own row.
    layout->addWidget(editor);
    auto* actions=new QHBoxLayout;actions->addStretch(1);
    actions->addWidget(apply);actions->addWidget(cancel);layout->addLayout(actions);
    auto* status=new QLabel(box);status->setObjectName("text-locale-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QLineEdit> safe_editor(editor);const QPointer<QPushButton> safe_apply(apply),safe_cancel(cancel);
    const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Text changed; refresh before applying locale");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying locale");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    QString initial_text;
    try{
        const auto& document=current_document();const auto& first=document.objects.at(targets.front()).text->locale;
        if(unchanged_locale(document,targets,first)){
            initial_text=QString::fromStdString(first);state->setText("Shared: "+initial_text);editor->setText(initial_text);
        }else{
            state->setText("Mixed");editor->setPlaceholderText("Mixed");
        }
    }catch(const std::exception& error){
        state->setText("Unavailable");editor->setEnabled(false);show_error(status,error);
    }
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply||!safe_cancel||!safe_status)return;
        safe_apply->setEnabled(false);safe_cancel->setEnabled(safe_editor->text()!=initial_text);
        try{
            const auto& document=current_document();safe_status->clear();
            // No tag grammar, trimming, case-folding, or character-based byte limit:
            // UpdateText and core validation are authoritative on Apply.
            safe_apply->setEnabled(safe_editor->text()!=initial_text&&
                !unchanged_locale(document,targets,safe_editor->text().toStdString()));
        }catch(const std::exception& error){safe_editor->setEnabled(false);show_error(safe_status.data(),error);}
    };
    QObject::connect(editor,&QLineEdit::textChanged,box,[update_draft](const QString&){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        safe_editor->setText(initial_text);update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& document=current_document();const auto value=safe_editor->text().toStdString();
            if(unchanged_locale(document,targets,value))return;
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& id:targets){
                auto source=*document.objects.at(id).text;source.locale=value;
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
