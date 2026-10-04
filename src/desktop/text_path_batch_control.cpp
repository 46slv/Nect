#include "text_path_batch_control.hpp"
#include "host.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

namespace nect::desktop {
namespace {
QString qs(const std::string& value){return QString::fromStdString(value);}
Id composition_for(const Document& document,const Id& target){
    for(const auto& composition:document.compositions){
        std::function<bool(const Id&)> contains=[&](const Id& id){
            if(id==target)return true;
            const auto found=document.objects.find(id);
            if(found==document.objects.end())return false;
            return std::any_of(found->second.children.begin(),found->second.children.end(),contains);
        };
        if(std::any_of(composition.roots.begin(),composition.roots.end(),contains))return composition.id;
    }
    throw Error("MISSING_OBJECT","Text target has no Composition: "+target);
}
Id require_targets(const Document& document,const std::vector<Id>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Text objects for Text on Path");
    std::set<Id> unique;Id composition;
    for(const auto& id:targets){
        if(!unique.insert(id).second)throw Error("INVALID_SELECTION","Duplicate Text target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::text||!found->second.text)
            throw Error("TYPE_MISMATCH","Every target must be an editable Text object: "+id);
        const auto owner=composition_for(document,id);
        if(composition.empty())composition=owner;
        else if(composition!=owner)throw Error("CROSS_COMPOSITION","Selected Text objects must share a Composition");
    }
    return composition;
}
void show_error(const QPointer<QLabel>& status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))status->setText(qs(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
struct Draft {
    QString path,contour,mode,start,spacing;
    Qt::CheckState reversed=Qt::Unchecked;
    bool operator==(const Draft&)const=default;
};
std::optional<double> number(const QString& text,const char* code,const char* message,bool nonnegative=false){
    if(text.trimmed().isEmpty())return std::nullopt;
    bool ok=false;const auto value=text.trimmed().toDouble(&ok);
    if(!ok||!std::isfinite(value)||(nonnegative&&value<0))throw Error(code,message);
    return value;
}
std::vector<Command> attachment_commands(const Document& document,const std::vector<Id>& targets,const Draft& draft,bool detach){
    const auto composition=require_targets(document,targets);
    std::optional<double> start,spacing;
    if(!detach){
        if(draft.path.isEmpty()||draft.contour.isEmpty())
            throw Error("MISSING_PATH_ATTACHMENT","Choose an authored Path and Contour by ID");
        const auto path=document.objects.find(draft.path.toStdString());
        if(path==document.objects.end())throw Error("MISSING_PATH_ATTACHMENT","The chosen Path no longer exists");
        if(path->second.kind!=Kind::path)throw Error("INVALID_PATH_ATTACHMENT","Choose an authored Path");
        if(path->second.source)throw Error("GENERATED_PATH_ATTACHMENT","Choose an authored Path contour");
        if(composition_for(document,path->first)!=composition)throw Error("CROSS_COMPOSITION","Text and Path must share a Composition");
        if(std::none_of(path->second.contours.begin(),path->second.contours.end(),[&](const Contour& c){return qs(c.id)==draft.contour;}))
            throw Error("MISSING_PATH_CONTOUR","The chosen Contour ID no longer exists");
        if(!draft.mode.isEmpty()&&draft.mode!="distance"&&draft.mode!="normalized")
            throw Error("TEXT_PATH_START_MODE","Choose distance or normalized start");
        start=number(draft.start,"TEXT_PATH_START_INVALID","Enter a finite start value");
        spacing=number(draft.spacing,"TEXT_PATH_SPACING","Enter finite nonnegative extra spacing",true);
    }
    std::vector<Command> commands;commands.reserve(targets.size());
    for(const auto& id:targets){
        const auto& previous=*document.objects.at(id).text;auto next=previous;
        if(detach)next.path_attachment.reset();
        else{
            auto attachment=previous.path_attachment.value_or(TextPathAttachment{});
            attachment.path=draft.path.toStdString();attachment.contour=draft.contour.toStdString();
            if(!draft.mode.isEmpty())attachment.start_mode=draft.mode.toStdString();
            if(start)attachment.start=*start;
            if(spacing)attachment.spacing=*spacing;
            if(draft.reversed!=Qt::PartiallyChecked)attachment.reversed=draft.reversed==Qt::Checked;
            next.path_attachment=std::move(attachment);
        }
        if(next!=previous)commands.push_back(UpdateText{id,std::move(next)});
    }
    return commands;
}
}

QWidget* make_text_path_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Text on Path · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("text-path-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("text-path-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* form=new QFormLayout;form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addLayout(form);
    auto combo=[&](const char* name){auto* value=new QComboBox(box);value->setObjectName(name);
        value->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);value->setMinimumContentsLength(12);return value;};
    auto* path=combo("text-path-batch-path");path->addItem("Choose authored Path…",QString{});form->addRow("Path",path);
    auto* contour=combo("text-path-batch-contour");contour->addItem("Choose Contour…",QString{});form->addRow("Contour",contour);
    auto* mode=combo("text-path-batch-start-mode");mode->addItem("Distance (du96)","distance");
    mode->addItem("Normalized fraction","normalized");form->addRow("Start mode",mode);
    auto* start=new QLineEdit(box);start->setObjectName("text-path-batch-start");form->addRow("Start",start);
    auto* spacing=new QLineEdit(box);spacing->setObjectName("text-path-batch-spacing");form->addRow("Extra spacing",spacing);
    auto* reversed=new QCheckBox("Reverse traversal",box);reversed->setObjectName("text-path-batch-reversed");form->addRow(reversed);
    path->setAccessibleName("Path for retained Text objects");contour->setAccessibleName("Contour ID for retained Text objects");
    mode->setAccessibleName("Text on Path start mode");start->setAccessibleName("Text on Path start value");
    spacing->setAccessibleName("Text on Path extra spacing");
    auto* apply=new QPushButton("Attach / update selected",box);apply->setObjectName("text-path-batch-apply");
    auto* detach=new QPushButton("Detach selected",box);detach->setObjectName("text-path-batch-detach");
    auto* cancel=new QPushButton("Cancel draft",box);cancel->setObjectName("text-path-batch-cancel");
    for(auto* button:{apply,detach,cancel}){button->setEnabled(false);layout->addWidget(button);}
    auto* status=new QLabel(box);status->setObjectName("text-path-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
    start->setToolTip("Blank keeps each Text's start; detached Text starts at 0.");
    spacing->setToolTip("Blank keeps each Text's spacing; detached Text uses 0.");
    reversed->setToolTip("Mixed keeps each Text's traversal; detached Text uses forward traversal.");
    apply->setToolTip("Attach or update the retained Text objects in one Undo step.");
    detach->setToolTip("Remove the retained Text attachments in one Undo step; ignore the draft.");
    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture,targets]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text selection belongs to another document");
        if(safe_host->session.revision()!=revision)throw Error("REVISION_CONFLICT","Text changed; refresh before applying");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying");
        const auto& document=safe_host->session.document();require_targets(document,targets);return document;
    };
    const auto populate_contours=[=](const Document& document,const QString& selected){
        const QSignalBlocker blocker(contour);contour->clear();contour->addItem("Choose Contour…",QString{});
        const auto found=document.objects.find(path->currentData().toString().toStdString());
        if(found!=document.objects.end()&&found->second.kind==Kind::path&&!found->second.source)
            for(const auto& c:found->second.contours)contour->addItem("Contour ["+qs(c.id)+"]",qs(c.id));
        const auto index=contour->findData(selected);contour->setCurrentIndex(index<0?0:index);
    };
    try{
        const auto& document=current_document();const auto composition=require_targets(document,targets);
        for(const auto& [id,object]:document.objects)
            if(object.kind==Kind::path&&!object.source&&!object.contours.empty()&&composition_for(document,id)==composition)
                path->addItem(qs(object.name)+" ["+qs(id)+"]",qs(id));
        const auto& first=document.objects.at(targets.front()).text->path_attachment;
        const auto same=[&](auto projection){const auto value=projection(first.value_or(TextPathAttachment{}));
            return std::all_of(targets.begin(),targets.end(),[&](const Id& id){return projection(document.objects.at(id).text->path_attachment.value_or(TextPathAttachment{}))==value;});};
        const bool equal=std::all_of(targets.begin(),targets.end(),[&](const Id& id){return document.objects.at(id).text->path_attachment==first;});
        state->setText(equal?(first?"Attached: "+qs(first->path)+" / "+qs(first->contour):"Detached"):
            "Mixed attachments — unspecified parameters keep each Text's value");
        const bool same_path=first&&std::all_of(targets.begin(),targets.end(),[&](const Id& id){const auto& a=document.objects.at(id).text->path_attachment;return a&&a->path==first->path&&a->contour==first->contour;});
        if(same_path)path->setCurrentIndex(path->findData(qs(first->path)));
        populate_contours(document,same_path?qs(first->contour):QString{});
        const auto initial=first.value_or(TextPathAttachment{});
        if(same([](const TextPathAttachment& a){return a.start_mode;}))mode->setCurrentIndex(mode->findData(qs(initial.start_mode)));
        else{mode->insertItem(0,"Mixed — keep each mode",QString{});mode->setCurrentIndex(0);}
        if(same([](const TextPathAttachment& a){return a.start;}))start->setText(QString::number(initial.start,'g',17));
        if(same([](const TextPathAttachment& a){return a.spacing;}))spacing->setText(QString::number(initial.spacing,'g',17));
        start->setPlaceholderText("Mixed — keep each start");spacing->setPlaceholderText("Mixed — keep each spacing");
        reversed->setTristate(!same([](const TextPathAttachment& a){return a.reversed;}));
        reversed->setCheckState(reversed->isTristate()?Qt::PartiallyChecked:(initial.reversed?Qt::Checked:Qt::Unchecked));
        detach->setEnabled(std::any_of(targets.begin(),targets.end(),[&](const Id& id){return document.objects.at(id).text->path_attachment.has_value();}));
    }catch(const std::exception& error){
        state->setText("Unavailable");for(auto* widget:std::vector<QWidget*>{path,contour,mode,start,spacing,reversed})widget->setEnabled(false);
        show_error(safe_status,error);
    }
    const auto read_draft=[=]{return Draft{path->currentData().toString(),contour->currentData().toString(),mode->currentData().toString(),start->text(),spacing->text(),reversed->checkState()};};
    const auto initial=read_draft();
    const auto update_draft=[=]{
        if(!safe_box)return;
        apply->setEnabled(false);cancel->setEnabled(read_draft()!=initial);
        try{
            const auto& document=current_document();const auto draft=read_draft();safe_status->clear();
            detach->setEnabled(!attachment_commands(document,targets,draft,true).empty());
            if(!draft.path.isEmpty()&&!draft.contour.isEmpty()){
                try{apply->setEnabled(!attachment_commands(document,targets,draft,false).empty());}
                catch(const std::exception& error){show_error(safe_status,error);}
            }
        }catch(const std::exception& error){detach->setEnabled(false);show_error(safe_status,error);}
    };
    QObject::connect(path,qOverload<int>(&QComboBox::currentIndexChanged),box,[=](int){
        if(!safe_box)return;
        try{populate_contours(current_document(),QString{});}catch(const std::exception& error){show_error(safe_status,error);}
        update_draft();
    });
    for(auto* value:{contour,mode})QObject::connect(value,qOverload<int>(&QComboBox::currentIndexChanged),box,[=](int){update_draft();});
    for(auto* value:{start,spacing})QObject::connect(value,&QLineEdit::textChanged,box,[=](const QString&){update_draft();});
    QObject::connect(reversed,&QCheckBox::stateChanged,box,[=](int){update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box)return;
        try{
            const auto& document=current_document();
            const QSignalBlocker p(path),c(contour),m(mode),s(start),g(spacing),r(reversed);
            path->setCurrentIndex(path->findData(initial.path));populate_contours(document,initial.contour);
            mode->setCurrentIndex(mode->findData(initial.mode));start->setText(initial.start);spacing->setText(initial.spacing);reversed->setCheckState(initial.reversed);
            update_draft();
        }catch(const std::exception& error){show_error(safe_status,error);}
    });
    const auto commit=[=](bool detach_only){
        if(!safe_box)return;
        try{
            auto commands=attachment_commands(current_document(),targets,read_draft(),detach_only);
            if(commands.empty())return;
            safe_host->session.apply(commands,revision);
            apply->setEnabled(false);detach->setEnabled(false);cancel->setEnabled(false);
            // edited() can synchronously destroy this entire Inspector.
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status,error);}
    };
    QObject::connect(apply,&QPushButton::clicked,box,[=]{commit(false);});
    QObject::connect(detach,&QPushButton::clicked,box,[=]{commit(true);});
    return box;
}
}
