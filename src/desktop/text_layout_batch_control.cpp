#include "text_layout_batch_control.hpp"
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
void require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Text objects for layout");
    std::set<Id> unique;
    for(const auto& id:targets){
        if(!unique.insert(id).second)
            throw Error("INVALID_SELECTION","Duplicate Text layout target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::text||!found->second.text)
            throw Error("TYPE_MISMATCH","Every layout target must be an editable Text object: "+id);
        if(found->second.text->layout_driver)
            throw Error("DRIVEN_PROPERTY","Text layout is linked on "+id+"; unlink it explicitly before a batch layout edit");
    }
}
bool unchanged_layout(const Document& document,const std::vector<Id>& targets,const std::string& value){
    for(const auto& id:targets)if(document.objects.at(id).text->layout!=value)return false;
    return true;
}
QString layout_label(const std::string& value){
    return value=="frame"?"Fixed frame":"Auto";
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_text_layout_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Text sizing mode · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("text-layout-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("text-layout-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* row=new QHBoxLayout;
    auto* editor=new QComboBox(box);editor->setObjectName("text-layout-batch-editor");
    editor->setAccessibleName("Sizing mode for every selected Text object");
    editor->addItem("Auto","auto");editor->addItem("Fixed frame","frame");
    editor->setToolTip("Changes sizing mode only; each Text keeps its frame dimensions and other sources.");
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("text-layout-batch-apply");
    apply->setToolTip("Sets every retained Text target in one Undo step.");apply->setEnabled(false);
    row->addWidget(editor,1);row->addWidget(apply);layout->addLayout(row);
    auto* status=new QLabel(box);status->setObjectName("text-layout-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_editor(editor);const QPointer<QPushButton> safe_apply(apply);
    const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Text changed; refresh before applying layout");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying layout");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    try{
        const auto& document=current_document();const auto& first=document.objects.at(targets.front()).text->layout;
        if(unchanged_layout(document,targets,first)){
            state->setText("Shared: "+layout_label(first));editor->setCurrentIndex(editor->findData(QString::fromStdString(first)));
        }else{
            state->setText("Mixed");editor->insertItem(0,"Mixed",QString{});editor->setCurrentIndex(0);
        }
    }catch(const std::exception& error){
        state->setText("Unavailable");editor->setCurrentIndex(-1);editor->setEnabled(false);show_error(status,error);
    }
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply)return;
        safe_apply->setEnabled(false);
        try{
            const auto& document=current_document();const auto value=safe_editor->currentData().toString().toStdString();
            safe_status->clear();
            safe_apply->setEnabled(!value.empty()&&!unchanged_layout(document,targets,value));
        }catch(const std::exception& error){safe_editor->setEnabled(false);show_error(safe_status.data(),error);}
    };
    QObject::connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& document=current_document();const auto value=safe_editor->currentData().toString().toStdString();
            if(value!="auto"&&value!="frame")
                throw Error("INVALID_TEXT_LAYOUT","Choose Auto or Fixed frame before applying sizing mode");
            if(unchanged_layout(document,targets,value))return;
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& id:targets){
                auto source=*document.objects.at(id).text;source.layout=value;
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
