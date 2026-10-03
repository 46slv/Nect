#include "text_string_source_batch_control.hpp"
#include "host.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace nect::desktop {
namespace {
struct StringState {
    std::string literal,evaluated;
    std::optional<Ref> driver;
};
struct TextSelection { Id composition;std::vector<StringState> states; };
struct TextSourceChoice { Ref ref;QString label; };

void require_field(const std::string& field){
    if(field!="text.family"&&field!="text.locale")
        throw Error("INVALID_TEXT_FIELD","Choose Text family or locale");
}
StringState string_state(const Document& document,const Ref& ref){
    require_field(ref.field);
    if(ref.field=="text.family"){
        const auto state=text_family_property(document,ref);
        return {state.literal,state.evaluated,state.driver?std::optional<Ref>(state.driver->link):std::nullopt};
    }
    const auto state=text_locale_property(document,ref);
    return {state.literal,state.evaluated,state.driver?std::optional<Ref>(state.driver->link):std::nullopt};
}
std::map<Id,Id> composition_owners(const Document& document){
    std::map<Id,Id> owners;
    for(const auto& composition:document.compositions){
        std::vector<Id> pending=composition.roots;
        while(!pending.empty()){
            auto id=std::move(pending.back());pending.pop_back();
            if(!owners.emplace(id,composition.id).second)continue;
            const auto found=document.objects.find(id);
            if(found!=document.objects.end())
                pending.insert(pending.end(),found->second.children.begin(),found->second.children.end());
        }
    }
    return owners;
}
TextSelection require_targets(const Document& document,const std::vector<Id>& targets,const std::string& field){
    require_field(field);
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct Text objects");
    std::set<Id> unique;TextSelection selection;const auto owners=composition_owners(document);
    // Validate the complete retained selection before evaluating or planning edits.
    for(const auto& id:targets){
        if(!unique.insert(id).second)throw Error("INVALID_SELECTION","Duplicate Text target: "+id);
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(found->second.kind!=Kind::text||!found->second.text)
            throw Error("TYPE_MISMATCH","Every source target must be an editable Text object: "+id);
        const auto owner=owners.find(id);
        if(owner==owners.end())throw Error("INVALID_SELECTION","Text has no owning Composition: "+id);
        if(selection.composition.empty())selection.composition=owner->second;
        else if(selection.composition!=owner->second)
            throw Error("INCOMPATIBLE_SELECTION","Every Text target must belong to the same Composition");
    }
    selection.states.reserve(targets.size());
    for(const auto& id:targets)selection.states.push_back(string_state(document,{id,"",field}));
    return selection;
}
bool eligible_source(const Document& document,const Ref& source,const std::vector<Id>& targets,
        const Id& composition,const std::string& field,const std::map<Id,Id>& owners){
    std::set<Id> visited;const std::set<Id> target_ids(targets.begin(),targets.end());auto current=source;
    // One new edge is added by Link, so the source closure can have at most 127
    // edges. Every edge must remain same-field and in the owning Composition.
    for(unsigned depth=0;depth<128;++depth){
        if(current.field!=field||!current.point.empty()||target_ids.contains(current.object)||
                !visited.insert(current.object).second)return false;
        const auto owner=owners.find(current.object);
        if(owner==owners.end()||owner->second!=composition)return false;
        const auto object=document.objects.find(current.object);
        if(object==document.objects.end()||object->second.kind!=Kind::text||!object->second.text)return false;
        const auto& text=*object->second.text;
        const auto driver=field=="text.family"?
            (text.family_driver?std::optional<Ref>(text.family_driver->link):std::nullopt):
            (text.locale_driver?std::optional<Ref>(text.locale_driver->link):std::nullopt);
        if(!driver)return true;
        current=*driver;
    }
    return false;
}
std::vector<TextSourceChoice> collect_sources(const Document& document,const std::vector<Id>& targets,
        const TextSelection& selection,const std::string& field){
    const auto owners=composition_owners(document);
    const auto composition=std::find_if(document.compositions.begin(),document.compositions.end(),
        [&](const auto& entry){return entry.id==selection.composition;});
    std::vector<TextSourceChoice> sources;if(composition==document.compositions.end())return sources;
    std::vector<Id> pending(composition->roots.rbegin(),composition->roots.rend());std::set<Id> visited;
    while(!pending.empty()){
        auto id=std::move(pending.back());pending.pop_back();if(!visited.insert(id).second)continue;
        const auto& object=document.objects.at(id);const Ref ref{id,"",field};
        if(object.kind==Kind::text&&object.text&&eligible_source(document,ref,targets,selection.composition,field,owners))
            sources.push_back({ref,QString::fromStdString(object.name)+" · "+QString::fromStdString(id)+
                " / "+QString::fromStdString(field)});
        pending.insert(pending.end(),object.children.rbegin(),object.children.rend());
    }
    return sources;
}
std::vector<Command> source_commands(const Document& document,const std::vector<Id>& targets,
        const TextSelection& selection,const std::string& field,const std::string& mode,
        const std::optional<Ref>& source,bool replace){
    std::vector<Command> commands;commands.reserve(targets.size());
    if(mode.empty())return commands;
    if(mode!="link"&&mode!="unlink")throw Error("INVALID_EDIT_MODE","Choose Link or Unlink sources");
    if(mode=="link"){
        if(!source||!eligible_source(document,*source,targets,selection.composition,field,composition_owners(document)))
            throw Error("MISSING_REFERENCE","Choose an eligible same-field, same-composition Text source");
        (void)string_state(document,*source);
    }
    for(std::size_t index=0;index<targets.size();++index){
        const auto& driver=selection.states[index].driver;const Ref target{targets[index],"",field};
        if(mode=="unlink"){
            if(!driver)continue; // Literal targets already have their own frozen value.
            if(field=="text.family")commands.push_back(UnlinkTextFamily{target});
            else commands.push_back(UnlinkTextLocale{target});
        }else{
            if(driver&&*driver==*source)continue;
            if(driver&&!replace)
                throw Error("DRIVEN_PROPERTY","Select Replace existing sources before replacing a Text source: "+targets[index]);
            if(field=="text.family")commands.push_back(LinkTextFamily{target,*source,driver.has_value()&&replace});
            else commands.push_back(LinkTextLocale{target,*source,driver.has_value()&&replace});
        }
    }
    return commands;
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_text_string_source_batch_controls(Host& host,const std::vector<Id>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Text sources · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("text-string-source-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* field=new QComboBox(box);field->setObjectName("text-string-source-batch-field");
    field->setAccessibleName("Text string field for every retained Text target");
    field->addItem("Font family","text.family");field->addItem("Language tag (locale)","text.locale");layout->addWidget(field);
    auto* state=new QLabel(box);state->setObjectName("text-string-source-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* mode=new QComboBox(box);mode->setObjectName("text-string-source-batch-mode");
    mode->setAccessibleName("Link or Unlink every retained Text target");
    // Long source/action labels stay in the popup instead of widening the panel.
    mode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    mode->setMinimumContentsLength(12);
    mode->addItem("Choose source action",QString{});mode->addItem("Link to another Text","link");
    mode->addItem("Unlink sources (freeze each value)","unlink");layout->addWidget(mode);
    auto* search=new QLineEdit(box);search->setObjectName("text-string-source-batch-source-search");
    search->setAccessibleName("Search source Text name, ID or property path");
    search->setPlaceholderText("Search Text name, ID or property path…");layout->addWidget(search);
    auto* source=new QComboBox(box);source->setObjectName("text-string-source-batch-source");
    source->setAccessibleName("Same-field same-composition Text source");layout->addWidget(source);
    // Long source/action labels stay in the popup instead of widening the panel.
    source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    source->setMinimumContentsLength(12);
    auto* replace=new QCheckBox("Replace existing sources",box);replace->setObjectName("text-string-source-batch-replace-driver");
    replace->setChecked(false);layout->addWidget(replace);
    auto* actions=new QHBoxLayout;
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("text-string-source-batch-apply");
    apply->setToolTip("Apply every retained Text target atomically in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("text-string-source-batch-cancel");
    cancel->setToolTip("Discard the source draft without changing the document or Undo history.");cancel->setEnabled(false);
    actions->addWidget(apply);actions->addWidget(cancel);layout->addLayout(actions);
    auto* status=new QLabel(box);status->setObjectName("text-string-source-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_field(field),safe_mode(mode),safe_source(source);
    const QPointer<QLineEdit> safe_search(search);const QPointer<QCheckBox> safe_replace(replace);
    const QPointer<QLabel> safe_state(state),safe_status(status);
    const QPointer<QPushButton> safe_apply(apply),safe_cancel(cancel);
    // Retain IDs and candidate labels, never a full authored Document snapshot.
    const auto sources=std::make_shared<std::vector<TextSourceChoice>>();
    const auto current_document=[safe_host,session_id,document_id,revision,gesture]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Text source selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Text changed; refresh before applying sources");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying Text sources");
        return safe_host->session.document();
    };
    const auto refill=[=]{
        const bool selected=safe_source->currentIndex()>=0;const auto selected_data=safe_source->currentData();
        const auto terms=safe_search->text().split(' ',Qt::SkipEmptyParts);
        const QSignalBlocker blocker(safe_source.data());safe_source->clear();
        for(std::size_t index=0;index<sources->size();++index){
            const auto& entry=sources->at(index);
            if(std::all_of(terms.begin(),terms.end(),[&](const auto& term){return entry.label.contains(term,Qt::CaseInsensitive);}))
                safe_source->addItem(entry.label,static_cast<int>(index));
        }
        safe_source->setCurrentIndex(selected?safe_source->findData(selected_data):-1);
    };
    const auto selected_source=[safe_source,sources]()->std::optional<Ref>{
        if(!safe_source||safe_source->currentIndex()<0)return {};
        bool ok=false;const auto index=safe_source->currentData().toInt(&ok);
        if(!ok||index<0||static_cast<std::size_t>(index)>=sources->size())return {};
        return sources->at(static_cast<std::size_t>(index)).ref;
    };
    const auto update_draft=[=]{
        if(!safe_box)return;
        const auto selected=safe_mode->currentData().toString();const bool link=selected=="link";
        safe_search->setVisible(link);safe_source->setVisible(link);safe_replace->setVisible(link);
        safe_apply->setEnabled(false);
        safe_cancel->setEnabled(safe_field->currentIndex()!=0||safe_mode->currentIndex()!=0||
            safe_source->currentIndex()>=0||safe_replace->isChecked()||!safe_search->text().isEmpty());
        try{
            const auto& document=current_document();const auto selected_field=safe_field->currentData().toString().toStdString();
            const auto selection=require_targets(document,targets,selected_field);
            const auto& first=selection.states.front().evaluated;
            const bool shared=std::all_of(selection.states.begin(),selection.states.end(),[&](const auto& entry){return entry.evaluated==first;});
            const auto driven=std::count_if(selection.states.begin(),selection.states.end(),[](const auto& entry){return entry.driver.has_value();});
            safe_state->setText((shared?"Shared evaluated value: "+QString::fromStdString(first):QString("Mixed evaluated values"))+
                QString(" · %1 of %2 sources linked").arg(static_cast<qulonglong>(driven)).arg(targets.size()));
            const auto commands=source_commands(document,targets,selection,selected_field,selected.toStdString(),selected_source(),safe_replace->isChecked());
            safe_status->clear();safe_apply->setEnabled(!commands.empty());
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    };
    const auto reset_sources=[=]{
        if(!safe_box)return;
        const QSignalBlocker block_source(safe_source.data()),block_search(safe_search.data()),block_replace(safe_replace.data());
        safe_source->setCurrentIndex(-1);safe_search->clear();safe_replace->setChecked(false);sources->clear();
        try{
            const auto& document=current_document();const auto selected_field=safe_field->currentData().toString().toStdString();
            const auto selection=require_targets(document,targets,selected_field);
            *sources=collect_sources(document,targets,selection,selected_field);
        }catch(const std::exception& error){safe_state->setText("Unavailable");show_error(safe_status.data(),error);}
        refill();update_draft();
    };
    QObject::connect(field,qOverload<int>(&QComboBox::currentIndexChanged),box,[reset_sources](int){reset_sources();});
    QObject::connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(source,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(replace,&QCheckBox::toggled,box,[update_draft](bool){update_draft();});
    QObject::connect(search,&QLineEdit::textChanged,box,[=](const QString&){if(!safe_box)return;refill();update_draft();});
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box)return;
        const QSignalBlocker block_field(safe_field.data()),block_mode(safe_mode.data());
        safe_field->setCurrentIndex(0);safe_mode->setCurrentIndex(0);reset_sources();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box)return;
        try{
            const auto& document=current_document();const auto selected_field=safe_field->currentData().toString().toStdString();
            const auto selection=require_targets(document,targets,selected_field);
            const auto commands=source_commands(document,targets,selection,selected_field,
                safe_mode->currentData().toString().toStdString(),selected_source(),safe_replace->isChecked());
            if(commands.empty())return;
            safe_host->session.apply(commands,revision);safe_apply->setEnabled(false);safe_cancel->setEnabled(false);
            // edited() may synchronously rebuild and delete the entire Inspector.
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    reset_sources();return box;
}
}
