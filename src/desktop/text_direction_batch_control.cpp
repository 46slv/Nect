#include "text_direction_batch_control.hpp"
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
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Text objects for direction");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate Text direction target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::text||!found->second.text)
            throw Error("TYPE_MISMATCH","Every direction target must be an editable Text object: "+id);
        if(found->second.text->direction_driver)
            throw Error("DRIVEN_PROPERTY","Text direction is linked on "+id+"; unlink it explicitly before a batch direction edit");
    }
}
bool unchanged_direction(const Document& document,const std::vector<Id>& targets,const std::string& value){
    for(const auto& id:targets)if(document.objects.at(id).text->direction!=value)return false;
    return true;
}
std::vector<Command> direction_commands(const Document& document,const std::vector<Id>& targets,const std::string& value){
    require_targets(document,targets);
    if(value!="horizontal"&&value!="vertical")
        throw Error("INVALID_TEXT_DIRECTION","Choose Horizontal or Vertical before applying direction");
    if(unchanged_direction(document,targets,value))return {};
    std::vector<Command> commands;commands.reserve(targets.size());
    for(const auto& id:targets){
        auto source=*document.objects.at(id).text;source.direction=value;
        commands.push_back(UpdateText{id,std::move(source)});
    }
    return commands;
}
QString direction_label(const std::string& value){
    return value=="vertical"?"Vertical":"Horizontal";
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_text_direction_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Text direction · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("text-direction-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("text-direction-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* row=new QHBoxLayout;
    auto* editor=new QComboBox(box);editor->setObjectName("text-direction-batch-editor");
    editor->setAccessibleName("Writing direction for every selected Text object");
    editor->addItem("Horizontal","horizontal");editor->addItem("Vertical","vertical");
    editor->setToolTip("Choose an absolute direction, then Apply to every selected Text object.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("text-direction-batch-apply");
    apply->setToolTip("Sets every retained Text target in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("text-direction-batch-cancel");
    cancel->setToolTip("Discards the direction draft without changing the document or Undo history.");
    cancel->setEnabled(false);
    row->addWidget(editor,1);row->addWidget(apply);row->addWidget(cancel);layout->addLayout(row);
    auto* status=new QLabel(box);status->setObjectName("text-direction-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_editor(editor);const QPointer<QPushButton> safe_apply(apply);
    const QPointer<QLabel> safe_status(status);const QPointer<QPushButton> safe_cancel(cancel);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Text changed; refresh before applying direction");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying direction");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    try{
        const auto& document=current_document();const auto& first=document.objects.at(targets.front()).text->direction;
        if(unchanged_direction(document,targets,first)){
            state->setText("Shared: "+direction_label(first));editor->setCurrentIndex(editor->findData(QString::fromStdString(first)));
        }else{
            state->setText("Mixed");editor->insertItem(0,"Mixed",QString{});editor->setCurrentIndex(0);
        }
    }catch(const std::exception& error){
        state->setText("Unavailable");editor->setCurrentIndex(-1);editor->setEnabled(false);show_error(status,error);
    }
    const int initial_index=editor->currentIndex();
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply||!safe_cancel||!safe_status)return;
        safe_apply->setEnabled(false);
        safe_cancel->setEnabled(safe_editor->currentIndex()!=initial_index);
        try{
            const auto& document=current_document();const auto value=safe_editor->currentData().toString().toStdString();
            safe_status->clear();
            safe_apply->setEnabled((value=="horizontal"||value=="vertical")&&!unchanged_direction(document,targets,value));
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
            const auto& document=current_document();const auto value=safe_editor->currentData().toString().toStdString();
            auto commands=direction_commands(document,targets,value);
            if(commands.empty())return;
            safe_host->session.apply(commands,revision);
            safe_apply->setEnabled(false);safe_cancel->setEnabled(false);
            // edited() can synchronously rebuild the Inspector and delete this control.
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    return box;
}
}
