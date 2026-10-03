#include "text_family_batch_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <set>

namespace nect::desktop {
namespace {
void require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Text objects for family");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate Text family target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::text||!found->second.text)
            throw Error("TYPE_MISMATCH","Every family target must be an editable Text object: "+id);
        if(found->second.text->family_driver)
            throw Error("DRIVEN_PROPERTY","Text family is linked on "+id+"; unlink it explicitly before a batch family edit");
    }
}
bool unchanged_family(const Document& document,const std::vector<Id>& targets,const std::string& value){
    for(const auto& id:targets)if(document.objects.at(id).text->family!=value)return false;
    return true;
}
QString family_label(const std::string& value){
    return QString::fromStdString(value);
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_text_family_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Text family · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("text-family-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("text-family-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* editor=new QComboBox(box);editor->setObjectName("text-family-batch-editor");
    editor->setAccessibleName("Font family for every selected Text object");
    editor->setEditable(true);
    // Installed font names must not determine the Inspector's minimum width.
    editor->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    editor->setMinimumContentsLength(12);
    editor->setInsertPolicy(QComboBox::NoInsert);
    try{for(const auto& name:text_fonts())editor->addItem(QString::fromStdString(name));}catch(const std::exception&){}
    editor->setCurrentIndex(-1);
    editor->setToolTip("Requested font family; unavailable fonts may use the existing fallback behavior.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("text-family-batch-apply");
    apply->setToolTip("Sets every retained Text target in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("text-family-batch-cancel");
    // Keep the value usable at narrow Inspector widths; actions have their own row.
    layout->addWidget(editor);
    auto* actions=new QHBoxLayout;actions->addStretch(1);
    actions->addWidget(apply);actions->addWidget(cancel);layout->addLayout(actions);
    auto* status=new QLabel(box);status->setObjectName("text-family-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_editor(editor);const QPointer<QPushButton> safe_apply(apply);
    const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Text changed; refresh before applying family");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying family");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    try{
        const auto& document=current_document();const auto& first=document.objects.at(targets.front()).text->family;
        if(unchanged_family(document,targets,first)){
            state->setText("Shared: "+family_label(first));editor->setEditText(QString::fromStdString(first));
        }else{
            state->setText("Mixed");editor->setCurrentIndex(-1);editor->setEditText({});editor->lineEdit()->setPlaceholderText("Mixed");
        }
    }catch(const std::exception& error){
        state->setText("Unavailable");editor->setCurrentIndex(-1);editor->setEnabled(false);show_error(status,error);
    }
    const auto initial_text=editor->currentText();
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply)return;
        safe_apply->setEnabled(false);
        try{
            const auto& document=current_document();const auto value=safe_editor->currentText().toStdString();
            safe_status->clear();
            safe_apply->setEnabled(!value.empty()&&!unchanged_family(document,targets,value));
        }catch(const std::exception& error){safe_editor->setEnabled(false);show_error(safe_status.data(),error);}
    };
    QObject::connect(editor,&QComboBox::currentTextChanged,box,[update_draft](const QString&){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[safe_editor,initial_text,update_draft]{
        if(!safe_editor)return;safe_editor->setEditText(initial_text);update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& document=current_document();const auto value=safe_editor->currentText().toStdString();
            if(value.empty())
                throw Error("INVALID_TEXT_FAMILY","Choose or enter a font family before applying");
            if(unchanged_family(document,targets,value))return;
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& id:targets){
                auto source=*document.objects.at(id).text;source.family=value;
                commands.push_back(UpdateText{id,std::move(source)});
            }
            safe_host->session.apply(commands,revision);
            // edited() can synchronously rebuild the Inspector and delete this control.
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    return box;
}
}
