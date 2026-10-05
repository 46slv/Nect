#include "instance_text_content_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVariantMap>
#include <QVBoxLayout>
#include <functional>
#include <memory>

namespace nect::desktop {
namespace {
constexpr const char* selection_property="nect-instance-text-content-selection";
const Definition& require_definition(const Document& document,const Id& instance){
    const auto found=document.objects.find(instance);
    if(found==document.objects.end())throw Error("MISSING_OBJECT",instance);
    if(found->second.kind!=Kind::instance||!found->second.instance)
        throw Error("TYPE_MISMATCH","Text content requires a Definition Instance");
    const auto definition=document.definitions.find(found->second.instance->definition);
    if(definition==document.definitions.end())throw Error("MISSING_DEFINITION",found->second.instance->definition);
    return definition->second;
}
void require_target(const Document& document,const Id& instance,const Id& source){
    const auto& definition=require_definition(document,instance);
    std::function<bool(const Id&)> descendant=[&](const Id& id){
        for(const auto& child:document.objects.at(id).children)
            if(child==source||descendant(child))return true;
        return false;
    };
    if(source==definition.root||!descendant(definition.root))
        throw Error("UNSUPPORTED_OVERRIDE","Choose a descendant source Text Object");
    const auto& object=document.objects.at(source);
    if(object.kind!=Kind::text||!object.text)
        throw Error("UNSUPPORTED_OVERRIDE","Text content overrides support descendant Text Objects only");
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
struct Draft {
    Id source;
    std::string raw;
    QString displayed;
};
}

QWidget* make_instance_text_content_controls(Host& host,const Id& instance,QWidget* parent){
    const auto& document=host.session.document();const auto& definition=require_definition(document,instance);
    const auto session_id=host.session_id;const auto document_id=document.id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox("Item Text content",parent);box->setObjectName("instance-text-content-controls");
    box->setProperty("nect-instance-id",QString::fromStdString(instance));auto* layout=new QVBoxLayout(box);
    auto* source=new QComboBox(box);source->setObjectName("instance-text-content-source");
    source->setAccessibleName("Descendant source Text Object");
    source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    source->setMinimumContentsLength(12);source->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    std::function<void(const Id&)> add_descendants=[&](const Id& id){
        for(const auto& child:document.objects.at(id).children){
            const auto& object=document.objects.at(child);
            if(object.kind==Kind::text&&object.text){
                const auto name=QString::fromStdString(object.name.empty()?child:object.name);
                source->addItem(name+" · "+QString::fromStdString(child),QString::fromStdString(child));
                source->setItemData(source->count()-1,QString::fromStdString(child),Qt::ToolTipRole);
            }
            add_descendants(child);
        }
    };
    add_descendants(definition.root);
    auto* source_label=new QLabel("Source Text",box);source_label->setBuddy(source);
    layout->addWidget(source_label);layout->addWidget(source);
    if(parent){
        const auto saved=parent->property(selection_property).toMap();
        if(saved.value("document").toString()==QString::fromStdString(document_id)&&
           saved.value("instance").toString()==QString::fromStdString(instance)){
            const auto index=source->findData(saved.value("source"));if(index>=0)source->setCurrentIndex(index);
        }
    }
    auto* source_state=new QLabel(box);source_state->setObjectName("instance-text-content-source-state");
    source_state->setToolTip("Evaluated authored source content, before this Instance's local content overrides.");
    auto* local_state=new QLabel(box);local_state->setObjectName("instance-text-content-local-state");
    for(auto* label:{source_state,local_state}){label->setTextFormat(Qt::PlainText);label->setWordWrap(true);layout->addWidget(label);}
    auto* editor=new QPlainTextEdit(box);editor->setObjectName("instance-text-content-editor");
    editor->setAccessibleName("Local Text content draft for this Instance");
    editor->setToolTip("Starts from current occurrence content, including local upstream Text links. Typing edits a draft; Apply local writes only this Instance's selected Text content.");
    layout->addWidget(editor);
    auto* row=new QGridLayout;
    auto* apply=new QPushButton("Apply local",box);apply->setObjectName("instance-text-content-apply");
    apply->setToolTip("Sets a literal content override in one Undo step. Unchanged inherited text can be frozen locally; source links remain unchanged.");
    auto* cancel=new QPushButton("Cancel draft",box);cancel->setObjectName("instance-text-content-cancel");
    cancel->setToolTip("Restores the selected Text's local or inherited content without editing the document.");
    auto* clear=new QPushButton("Clear draft",box);clear->setObjectName("instance-text-content-clear");
    clear->setToolTip("Makes the draft explicitly empty. Apply local commits empty content; this does not reset inheritance.");
    auto* reset=new QPushButton("Use Source",box);reset->setObjectName("instance-text-content-reset");
    reset->setToolTip("Removes only the selected Text's local override and follows its current source content.");
    // Keep complete action labels reachable at the standard Properties width.
    row->addWidget(apply,0,0);row->addWidget(cancel,0,1);
    row->addWidget(clear,1,0);row->addWidget(reset,1,1);layout->addLayout(row);
    auto* note=new QLabel("Descendant Text only. Source content links stay intact. Unsupported Text-on-Path line breaks are refused on Apply.",box);
    note->setTextFormat(Qt::PlainText);note->setWordWrap(true);layout->addWidget(note);
    auto* status=new QLabel(box);status->setObjectName("instance-text-content-error");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QWidget> safe_parent(parent);const QPointer<QComboBox> safe_source(source);
    const QPointer<QPlainTextEdit> safe_editor(editor);
    const QPointer<QLabel> safe_source_state(source_state),safe_local_state(local_state),safe_status(status);
    const QPointer<QPushButton> safe_apply(apply),safe_cancel(cancel),safe_clear(clear),safe_reset(reset);
    const auto draft=std::make_shared<Draft>();
    const auto current_document=[safe_host,session_id,document_id,revision,gesture]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","This Instance changed; refresh before editing");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before editing");
        return safe_host->session.document();
    };
    const auto selected_target=[safe_source]{
        if(!safe_source||safe_source->currentIndex()<0)
            throw Error("NO_TEXT_TARGET","Choose a descendant source Text Object");
        return safe_source->currentData().toString().toStdString();
    };
    const auto draft_value=[safe_editor,draft]{
        if(!safe_editor)throw Error("NO_TEXT_TARGET","The Text draft is no longer available");
        const auto displayed=safe_editor->toPlainText();
        // QTextDocument normalizes CRLF and paragraph/line separators (and
        // some spaces). Unchanged or restored display retains original bytes.
        return displayed==draft->displayed?draft->raw:displayed.toStdString();
    };
    const auto disable=[=]{
        if(safe_editor)safe_editor->setEnabled(false);
        for(const auto& button:{safe_apply,safe_cancel,safe_clear,safe_reset})if(button)button->setEnabled(false);
    };
    const auto update_draft=[=]{
        if(!safe_box||!safe_editor||!safe_apply||!safe_cancel||!safe_clear||!safe_reset)return;
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            if(target!=draft->source)throw Error("REVISION_CONFLICT","The Text draft belongs to another source Object");
            const auto& overrides=current.objects.at(instance).instance->text_content_overrides;const auto local=overrides.find(target);
            const auto value=draft_value();safe_editor->setEnabled(true);
            // Explicit Apply without an override intentionally freezes even
            // equal-source content; an identical existing local value is a noop.
            safe_apply->setEnabled(local==overrides.end()||local->second!=value);
            safe_cancel->setEnabled(safe_editor->toPlainText()!=draft->displayed);
            safe_clear->setEnabled(!safe_editor->toPlainText().isEmpty());safe_reset->setEnabled(local!=overrides.end());
            if(safe_status)safe_status->clear();
        }catch(const std::exception& error){disable();show_error(safe_status.data(),error);}
    };
    const auto load_selection=[=]{
        if(!safe_box||!safe_source||!safe_editor||!safe_source_state||!safe_local_state)return;
        disable();draft->source.clear();
        if(safe_source->currentIndex()<0){
            safe_source->setEnabled(false);const QSignalBlocker blocker(safe_editor.data());safe_editor->clear();
            safe_source_state->setText("Source: —");safe_local_state->setText("Local: —");
            if(safe_status)safe_status->setText("No descendant Text Objects");return;
        }
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            const auto state=text_content_property(current,{target,"","text.content"});
            const auto source_text=QString::fromStdString(state.evaluated);
            QString label="Source: "+source_text.left(160);if(source_text.size()>160)label+="…";
            if(state.driver)label+=" · Link";safe_source_state->setText(label);
            const auto& overrides=current.objects.at(instance).instance->text_content_overrides;const auto local=overrides.find(target);
            const bool overridden=local!=overrides.end();safe_local_state->setText(overridden?"Local: Override":"Local: Inherited in this instance");
            draft->source=target;
            // Inheritance can follow another descendant's local content link.
            // The typed core evaluator owns that occurrence-local dependency.
            draft->raw=overridden?local->second:evaluate_instance_text_content(current,instance,target);
            {const QSignalBlocker blocker(safe_editor.data());safe_editor->setPlainText(QString::fromStdString(draft->raw));}
            draft->displayed=safe_editor->toPlainText();
            if(safe_parent)safe_parent->setProperty(selection_property,QVariantMap{
                {"document",QString::fromStdString(document_id)},{"instance",QString::fromStdString(instance)},
                {"source",QString::fromStdString(target)}});
            update_draft();
        }catch(const std::exception& error){
            safe_source_state->setText("Source: Unavailable");safe_local_state->setText("Local: Unavailable");show_error(safe_status.data(),error);
        }
    };
    QObject::connect(source,qOverload<int>(&QComboBox::currentIndexChanged),box,[load_selection](int){load_selection();});
    QObject::connect(editor,&QPlainTextEdit::textChanged,box,[update_draft]{update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            if(target!=draft->source)throw Error("REVISION_CONFLICT","The Text draft belongs to another source Object");
            {const QSignalBlocker blocker(safe_editor.data());safe_editor->setPlainText(draft->displayed);}update_draft();
        }catch(const std::exception& error){disable();show_error(safe_status.data(),error);}
    });
    QObject::connect(clear,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            if(target!=draft->source)throw Error("REVISION_CONFLICT","The Text draft belongs to another source Object");
            safe_editor->clear();update_draft();
        }catch(const std::exception& error){disable();show_error(safe_status.data(),error);}
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_editor)return;
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            if(target!=draft->source)throw Error("REVISION_CONFLICT","The Text draft belongs to another source Object");
            const auto value=draft_value();const auto& overrides=current.objects.at(instance).instance->text_content_overrides;
            const auto local=overrides.find(target);if(local!=overrides.end()&&local->second==value)return;
            // Core validation owns UTF-8/size and direct or indirectly linked
            // Text-on-Path multiline refusal. No source driver is removed here.
            safe_host->session.apply({DefinitionCommand{SetInstanceTextContentOverride{instance,target,value}}},revision);
            safe_host->edited();return; // Refresh can synchronously destroy every widget.
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    QObject::connect(reset,&QPushButton::clicked,box,[=]{
        if(!safe_box)return;
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            if(!current.objects.at(instance).instance->text_content_overrides.contains(target))return;
            safe_host->session.apply({DefinitionCommand{ResetInstanceTextContentOverride{instance,target}}},revision);
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    load_selection();return box;
}
}
