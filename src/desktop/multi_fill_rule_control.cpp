#include "multi_fill_rule_control.hpp"
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
#include <set>

namespace nect::desktop {
namespace {
struct FillSelection {
    std::vector<FillRuleProperty> states;
    std::size_t slot=0;
    bool driven=false;
};
const ProcessingEntry& target_operation(const Document& document,const MultiFillRuleTarget& target){
    const auto object=document.objects.find(target.object);
    if(object==document.objects.end())throw Error("MISSING_OBJECT",target.object);
    if(object->second.kind!=Kind::path&&object->second.kind!=Kind::text)
        throw Error("TYPE_MISMATCH","Fill rule targets must be Path or Text objects: "+target.object);
    const auto operation=std::find_if(object->second.stack.begin(),object->second.stack.end(),
        [&](const auto& entry){return entry.id==target.operation;});
    if(operation==object->second.stack.end())throw Error("MISSING_OPERATION",target.operation);
    if(operation->macro||operation->type!="nect.paint.fill"||operation->version!=1)
        throw Error("UNSUPPORTED_FILL","Every target must be a supported Fill operation: "+target.object+"/"+target.operation);
    return *operation;
}
FillSelection require_targets(const Document& document,const std::vector<MultiFillRuleTarget>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct same-slot Fill targets");
    FillSelection result;std::set<Id> objects;
    // Validate the complete selection before evaluating or constructing commands.
    for(const auto& target:targets){
        if(!objects.insert(target.object).second)
            throw Error("INVALID_SELECTION","Choose distinct whole-object Fill targets: "+target.object);
        const auto& operation=target_operation(document,target);
        const auto& stack=document.objects.at(target.object).stack;
        const auto slot=static_cast<std::size_t>(&operation-stack.data());
        if(objects.size()==1)result.slot=slot;
        else if(result.slot!=slot)
            throw Error("INCOMPATIBLE_FILL","Every Fill target must be at the same stack position");
    }
    result.states.reserve(targets.size());
    for(const auto& target:targets){
        auto state=fill_rule_property(document,target.ref());
        result.driven=result.driven||state.driver.has_value();result.states.push_back(std::move(state));
    }
    return result;
}
QString rule_label(const std::string& rule){return rule=="evenodd"?"Even-odd":"Nonzero winding";}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
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
struct FillSource { Ref ref;QString label; };
bool eligible_source(const Document& document,const Ref& source,const std::set<Ref>& targets){
    // A single source is installed on every target. Any path back to any target
    // would make that atomic link batch cyclic, even if the source is another
    // Fill on a selected object. Core still validates the actual commit.
    std::set<Ref> visited;auto current=source;
    for(unsigned depth=0;depth<=128;++depth){
        if(targets.contains(current)||!visited.insert(current).second)return false;
        const auto state=fill_rule_property(document,current);
        if(!state.driver)return true;
        current=state.driver->link;
    }
    return false;
}
std::vector<FillSource> collect_sources(const Document& document,const std::vector<MultiFillRuleTarget>& targets){
    const auto owners=composition_owners(document);std::set<Ref> target_refs;Id composition;
    for(const auto& target:targets){
        target_refs.insert(target.ref());const auto owner=owners.find(target.object);
        if(owner==owners.end())return {};
        if(composition.empty())composition=owner->second;
        else if(composition!=owner->second)return {};
    }
    std::vector<FillSource> sources;
    const auto selected=std::find_if(document.compositions.begin(),document.compositions.end(),
        [&](const auto& candidate){return candidate.id==composition;});
    if(selected==document.compositions.end())return sources;
    // Preserve the established source picker's composition tree/stack order.
    std::vector<Id> pending(selected->roots.rbegin(),selected->roots.rend());std::set<Id> visited;
    while(!pending.empty()){
        auto id=std::move(pending.back());pending.pop_back();if(!visited.insert(id).second)continue;
        const auto& object=document.objects.at(id);
        if(object.kind==Kind::path||object.kind==Kind::text)
            for(const auto& operation:object.stack){
                if(operation.macro||operation.type!="nect.paint.fill"||operation.version!=1)continue;
                const auto source=operation_ref(id,operation.id,"fill_rule");
                if(!eligible_source(document,source,target_refs))continue;
                sources.push_back({source,QString::fromStdString(object.name)+" — Fill ["+
                    QString::fromStdString(operation.id)+"] — "+QString::fromStdString(id)});
            }
        pending.insert(pending.end(),object.children.rbegin(),object.children.rend());
    }
    return sources;
}
std::vector<Command> rule_commands(const Document& document,const std::vector<MultiFillRuleTarget>& targets,
    const FillSelection& selection,const QString& mode,const std::string& value,const std::optional<Ref>& source,bool replace){
    std::vector<Command> commands;commands.reserve(targets.size());
    if(mode=="edit"){
        if(selection.driven){
            const auto index=static_cast<std::size_t>(std::find_if(selection.states.begin(),selection.states.end(),
                [](const auto& state){return state.driver.has_value();})-selection.states.begin());
            throw Error("DRIVEN_PROPERTY","Unlink the Fill rule driver explicitly before applying a literal: "+
                targets[index].object+"/"+targets[index].operation);
        }
        if(value.empty())return commands; // Mixed is a display state, not an edit.
        if(value!="nonzero"&&value!="evenodd")throw Error("UNSUPPORTED_FILL_RULE","Choose Nonzero winding or Even-odd");
        for(std::size_t index=0;index<targets.size();++index){
            const auto& target=targets[index];const auto& operation=target_operation(document,target);
            if(selection.states[index].literal!=value)
                commands.push_back(OperationOptions{target.object,target.operation,operation.composite,value,{}});
        }
    }else if(mode=="unlink"){
        for(std::size_t index=0;index<targets.size();++index)
            if(selection.states[index].driver)commands.push_back(UnlinkFillRule{targets[index].ref()});
    }else if(mode=="link"){
        if(!source)throw Error("MISSING_REFERENCE","Choose a visible Fill rule source");
        for(std::size_t index=0;index<targets.size();++index){
            const auto& driver=selection.states[index].driver;
            if(driver&&driver->link==*source)continue;
            if(driver&&!replace)throw Error("DRIVEN_PROPERTY","Select Replace existing drivers before replacing a Fill rule source");
            commands.push_back(LinkFillRule{targets[index].ref(),*source,driver.has_value()&&replace});
        }
    }else throw Error("INVALID_EDIT_MODE","Choose a Fill rule editing mode");
    return commands;
}
}

QWidget* make_multi_fill_rule_controls(Host& host,const std::vector<MultiFillRuleTarget>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Fill rule · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("multi-fill-rule-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("multi-fill-rule-state");state->setTextFormat(Qt::PlainText);
    state->setWordWrap(true);layout->addWidget(state);
    auto* mode=new QComboBox(box);mode->setObjectName("multi-fill-rule-mode");mode->addItem("Edit literal","edit");
    mode->setAccessibleName("Fill rule edit mode for every retained Fill target");layout->addWidget(mode);
    // Long source/action labels stay in the popup instead of widening the panel.
    mode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    mode->setMinimumContentsLength(12);
    auto* value=new QComboBox(box);value->setObjectName("multi-fill-rule-value");
    value->setAccessibleName("Fill rule for every retained Fill target");
    value->addItem("Nonzero winding","nonzero");value->addItem("Even-odd","evenodd");layout->addWidget(value);
    auto* search=new QLineEdit(box);search->setObjectName("multi-fill-rule-source-search");
    search->setPlaceholderText("Search object ID, Fill ID or property path");layout->addWidget(search);
    auto* source=new QComboBox(box);source->setObjectName("multi-fill-rule-source");
    source->setAccessibleName("Same-composition Fill rule source");layout->addWidget(source);
    // Long source/action labels stay in the popup instead of widening the panel.
    source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    source->setMinimumContentsLength(12);
    auto* replace=new QCheckBox("Replace existing drivers",box);replace->setObjectName("multi-fill-rule-replace-driver");
    layout->addWidget(replace);
    auto* actions=new QHBoxLayout;
    auto* apply=new QPushButton("Apply",box);apply->setObjectName("multi-fill-rule-apply");
    apply->setToolTip("Applies the retained Fill targets atomically in one Undo step.");apply->setEnabled(false);
    auto* cancel=new QPushButton("Cancel",box);cancel->setObjectName("multi-fill-rule-cancel");
    cancel->setToolTip("Discards this draft without changing Fill rules or Undo history.");cancel->setEnabled(false);
    actions->addWidget(apply);actions->addWidget(cancel);layout->addLayout(actions);
    auto* status=new QLabel(box);status->setObjectName("multi-fill-rule-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QComboBox> safe_mode(mode),safe_value(value),safe_source(source);
    const QPointer<QLineEdit> safe_search(search);const QPointer<QCheckBox> safe_replace(replace);
    const QPointer<QPushButton> safe_apply(apply),safe_cancel(cancel);const QPointer<QLabel> safe_status(status);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Fill selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Fill rules changed; refresh before applying this draft");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying Fill rules");
        return safe_host->session.document();
    };
    FillSelection initial;std::vector<FillSource> sources;bool available=false;
    try{
        const auto& document=current_document();initial=require_targets(document,targets);sources=collect_sources(document,targets);
        box->setTitle(QString("Fill rule · stack %1 · %2 selected objects").arg(initial.slot+1).arg(targets.size()));
        const auto& first=initial.states.front().evaluated;
        const bool shared=std::all_of(initial.states.begin(),initial.states.end(),[&](const auto& entry){return entry.evaluated==first;});
        state->setText(shared?"Shared: "+rule_label(first):QStringLiteral("Mixed"));
        if(shared)value->setCurrentIndex(value->findData(QString::fromStdString(first)));
        else{value->insertItem(0,"Mixed",QString{});value->setCurrentIndex(0);}
        if(!sources.empty())mode->addItem("Link to another Fill","link");
        if(initial.driven)mode->addItem("Unlink drivers (freeze each value)","unlink");
        for(std::size_t index=0;index<sources.size();++index)source->addItem(sources[index].label,static_cast<int>(index));
        const auto& first_driver=initial.states.front().driver;
        if(first_driver&&std::all_of(initial.states.begin(),initial.states.end(),[&](const auto& entry){return entry.driver==first_driver;})){
            const auto found=std::find_if(sources.begin(),sources.end(),[&](const auto& entry){return entry.ref==first_driver->link;});
            if(found!=sources.end())source->setCurrentIndex(static_cast<int>(found-sources.begin()));
        }
        available=true;
    }catch(const std::exception& error){
        state->setText("Unavailable");value->setCurrentIndex(-1);show_error(status,error);
    }
    const int initial_value=value->currentIndex(),initial_source=source->currentIndex();
    const auto selected_source=[safe_source,sources]()->std::optional<Ref>{
        if(!safe_source||safe_source->currentIndex()<0)return {};
        const auto index=safe_source->currentData().toInt();
        if(index<0||static_cast<std::size_t>(index)>=sources.size())return {};
        return sources[static_cast<std::size_t>(index)].ref;
    };
    const auto update_draft=[=]{
        if(!safe_box)return;
        const auto selected=safe_mode->currentData().toString();const bool link=selected=="link";
        safe_mode->setEnabled(available);safe_value->setEnabled(available&&selected=="edit"&&!initial.driven);
        safe_search->setVisible(link);safe_search->setEnabled(available&&link);
        safe_source->setVisible(link);safe_source->setEnabled(available&&link);
        safe_replace->setVisible(link&&initial.driven);safe_replace->setEnabled(available&&link);
        safe_apply->setEnabled(false);
        safe_cancel->setEnabled(available&&(safe_mode->currentIndex()!=0||safe_value->currentIndex()!=initial_value||
            safe_source->currentIndex()!=initial_source||safe_replace->isChecked()||!safe_search->text().isEmpty()));
        if(!available)return;
        try{
            const auto& document=current_document();const auto selection=require_targets(document,targets);
            const auto commands=rule_commands(document,targets,selection,selected,
                safe_value->currentData().toString().toStdString(),selected_source(),safe_replace->isChecked());
            safe_status->clear();safe_apply->setEnabled(!commands.empty());
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    };
    QObject::connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(value,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(source,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_draft](int){update_draft();});
    QObject::connect(replace,&QCheckBox::toggled,box,[update_draft](bool){update_draft();});
    QObject::connect(search,&QLineEdit::textChanged,box,[=](const QString& query){
        if(!safe_box)return;
        const bool had_selection=safe_source->currentIndex()>=0;const auto selected=safe_source->currentData();
        {const QSignalBlocker blocker(safe_source.data());safe_source->clear();
            for(std::size_t index=0;index<sources.size();++index){
                const auto& entry=sources[index];const auto path=QString::fromStdString(entry.ref.object)+" / "+QString::fromStdString(entry.ref.field);
                if(entry.label.contains(query,Qt::CaseInsensitive)||path.contains(query,Qt::CaseInsensitive))
                    safe_source->addItem(entry.label,static_cast<int>(index));
            }
            safe_source->setCurrentIndex(had_selection?safe_source->findData(selected):-1);
        }
        update_draft();
    });
    QObject::connect(cancel,&QPushButton::clicked,box,[=]{
        if(!safe_box)return;
        const QSignalBlocker block_mode(safe_mode.data()),block_value(safe_value.data()),block_source(safe_source.data());
        const QSignalBlocker block_search(safe_search.data()),block_replace(safe_replace.data());
        safe_mode->setCurrentIndex(0);safe_value->setCurrentIndex(initial_value);safe_search->clear();safe_replace->setChecked(false);
        safe_source->clear();for(std::size_t index=0;index<sources.size();++index)
            safe_source->addItem(sources[index].label,static_cast<int>(index));
        safe_source->setCurrentIndex(initial_source);update_draft();
    });
    QObject::connect(apply,&QPushButton::clicked,box,[=]{
        if(!safe_box)return;
        try{
            const auto& document=current_document();const auto selection=require_targets(document,targets);
            const auto next_source=selected_source();
            if(safe_mode->currentData().toString()=="link"){
                const auto current_sources=collect_sources(document,targets);
                if(!next_source||std::none_of(current_sources.begin(),current_sources.end(),[&](const auto& entry){return entry.ref==*next_source;}))
                    throw Error("MISSING_REFERENCE","Choose an eligible same-composition Fill rule source");
            }
            auto commands=rule_commands(document,targets,selection,safe_mode->currentData().toString(),
                safe_value->currentData().toString().toStdString(),next_source,safe_replace->isChecked());
            if(commands.empty())return;
            safe_host->session.apply(commands,revision);safe_apply->setEnabled(false);safe_cancel->setEnabled(false);
            // edited() can synchronously rebuild the Inspector and delete every
            // captured widget. Do not dereference them after this notification.
            safe_host->edited();return;
        }catch(const std::exception& error){show_error(safe_status.data(),error);}
    });
    update_draft();return box;
}
}
