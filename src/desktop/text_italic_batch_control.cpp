#include "text_italic_batch_control.hpp"
#include "host.hpp"
#include <QCheckBox>
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
// PartiallyChecked represents the captured Mixed value, never an authored value.
// Mouse, Space and click() all use this override to choose only On or Off.
class ItalicChoice final : public QCheckBox {
public:
    explicit ItalicChoice(QWidget* parent):QCheckBox("Italic",parent){}
protected:
    void nextCheckState() override {
        const auto next=checkState()==Qt::Checked?Qt::Unchecked:Qt::Checked;
        setTristate(false);setCheckState(next);
    }
};
void require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Text objects for italic");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate Text italic target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::text||!found->second.text)
            throw Error("TYPE_MISMATCH","Every italic target must be an editable Text object: "+id);
        if(found->second.text->italic_driver)
            throw Error("DRIVEN_PROPERTY","Text italic is linked or expression-driven on "+id+"; unlink it explicitly before a batch italic edit");
    }
}
bool unchanged_italic(const Document& document,const std::vector<Id>& targets,bool value){
    for(const auto& id:targets)if(document.objects.at(id).text->italic!=value)return false;
    return true;
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_text_italic_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Text italic · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("text-italic-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("text-italic-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* editor=new ItalicChoice(box);editor->setObjectName("text-italic-batch-editor");
    editor->setAccessibleName("Italic for every selected Text object");
    editor->setToolTip("Mixed is a display state. Click or press Space to choose On, then again for Off. Apply sets every captured Text object.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("text-italic-batch-apply");
    apply->setToolTip("Sets every captured Text target in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("text-italic-batch-cancel");
    cancel->setToolTip("Discard the italic draft without editing the document.");
    // Keep the value usable at narrow Inspector widths; actions have their own row.
    layout->addWidget(editor);
    auto* actions=new QHBoxLayout;actions->addStretch(1);
    actions->addWidget(apply);actions->addWidget(cancel);layout->addLayout(actions);
    auto* status=new QLabel(box);status->setObjectName("text-italic-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<ItalicChoice> safe_editor(editor);const QPointer<QPushButton> safe_apply(apply);
    const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Text changed; refresh before applying italic");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying italic");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    Qt::CheckState initial_state=Qt::Unchecked;
    try{
        const auto& document=current_document();const auto first=document.objects.at(targets.front()).text->italic;
        if(unchanged_italic(document,targets,first)){
            initial_state=first?Qt::Checked:Qt::Unchecked;state->setText(first?"Shared: On":"Shared: Off");
        }else{
            initial_state=Qt::PartiallyChecked;state->setText("Mixed");editor->setTristate(true);
        }
        editor->setCheckState(initial_state);
    }catch(const std::exception& error){
        state->setText("Unavailable");editor->setEnabled(false);show_error(status,error);
    }
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply||!safe_status)return;
        safe_apply->setEnabled(false);
        try{
            const auto& document=current_document();const auto choice=safe_editor->checkState();
            safe_status->clear();
            safe_apply->setEnabled(choice!=Qt::PartiallyChecked&&!unchanged_italic(document,targets,choice==Qt::Checked));
        }catch(const std::exception& error){safe_editor->setEnabled(false);show_error(safe_status.data(),error);}
    };
    // stateChanged works on the minimum supported Qt 6.5 (checkStateChanged is newer).
    QObject::connect(editor,&QCheckBox::stateChanged,box,[update_draft](int){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor||!safe_apply)return;
        {
            const QSignalBlocker blocker(safe_editor.data());
            safe_editor->setTristate(initial_state==Qt::PartiallyChecked);safe_editor->setCheckState(initial_state);
        }
        update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& document=current_document();const auto choice=safe_editor->checkState();
            if(choice!=Qt::Checked&&choice!=Qt::Unchecked)
                throw Error("INVALID_TEXT_ITALIC","Choose On or Off before applying italic");
            const auto value=choice==Qt::Checked;
            if(unchanged_italic(document,targets,value))return;
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& id:targets){
                auto source=*document.objects.at(id).text;source.italic=value;
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
