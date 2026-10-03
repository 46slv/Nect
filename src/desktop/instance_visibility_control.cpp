#include "instance_visibility_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSizePolicy>
#include <QVariantMap>
#include <QVBoxLayout>
#include <functional>

namespace nect::desktop {
namespace {
constexpr const char* selection_property="nect-instance-visibility-selection";
QString visibility_text(bool visible){return visible?"Visible":"Hidden";}
}
QWidget* make_instance_visibility_controls(Host& host,const Id& instance,QWidget* parent){
    const auto& document=host.session.document();
    const auto found=document.objects.find(instance);
    if(found==document.objects.end())throw Error("MISSING_OBJECT",instance);
    if(found->second.kind!=Kind::instance||!found->second.instance)
        throw Error("TYPE_MISMATCH","Item visibility requires a Definition Instance");
    const auto definition=document.definitions.find(found->second.instance->definition);
    if(definition==document.definitions.end())throw Error("MISSING_DEFINITION",found->second.instance->definition);
    const auto document_id=document.id;const auto session_id=host.session_id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();

    auto* box=new QGroupBox("Item visibility",parent);box->setObjectName("instance-visibility-controls");
    box->setProperty("nect-instance-id",QString::fromStdString(instance));
    auto* layout=new QVBoxLayout(box);
    auto* source=new QComboBox(box);source->setObjectName("instance-visibility-source");
    source->setAccessibleName("Source item");
    source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    source->setMinimumContentsLength(12);source->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    std::function<void(const Id&)> add_descendants=[&](const Id& id){
        for(const auto& child:document.objects.at(id).children){
            const auto& object=document.objects.at(child);
            const auto label=QString::fromStdString(object.name.empty()?child:object.name);
            source->addItem(label+" · "+QString::fromStdString(child),QString::fromStdString(child));
            source->setItemData(source->count()-1,QString::fromStdString(child),Qt::ToolTipRole);
            add_descendants(child);
        }
    };
    add_descendants(definition->second.root);layout->addWidget(source);
    if(parent){
        const auto saved=parent->property(selection_property).toMap();
        if(saved.value("document").toString()==QString::fromStdString(document_id)&&
           saved.value("instance").toString()==QString::fromStdString(instance)){
            const auto selected=source->findData(saved.value("source"));
            if(selected>=0)source->setCurrentIndex(selected);
        }
    }
    auto* source_state=new QLabel(box);source_state->setObjectName("instance-visibility-source-state");
    source_state->setTextFormat(Qt::PlainText);source_state->setWordWrap(true);layout->addWidget(source_state);
    auto* local_state=new QLabel(box);local_state->setObjectName("instance-visibility-local-state");
    local_state->setTextFormat(Qt::PlainText);local_state->setWordWrap(true);layout->addWidget(local_state);
    auto* row=new QHBoxLayout;
    auto* hide=new QPushButton("Hide",box);hide->setObjectName("instance-visibility-hide");
    auto* show=new QPushButton("Show",box);show->setObjectName("instance-visibility-show");
    auto* reset=new QPushButton("Use Source",box);reset->setObjectName("instance-visibility-reset");
    show->setToolTip("Sets this item's own visibility. Hidden ancestors still suppress the item.");
    hide->setToolTip("Hides this item only in this Instance.");
    reset->setToolTip("Removes this local override and follows the current source visibility.");
    row->addWidget(hide);row->addWidget(show);row->addWidget(reset);layout->addLayout(row);
    auto* ancestry=new QLabel("Hidden ancestors still apply",box);ancestry->setWordWrap(true);layout->addWidget(ancestry);
    auto* status=new QLabel(box);status->setObjectName("instance-visibility-error");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QWidget> safe_parent(parent);const QPointer<QComboBox> safe_source(source);
    const QPointer<QLabel> safe_source_state(source_state),safe_local_state(local_state),safe_status(status);
    const QPointer<QPushButton> safe_hide(hide),safe_show(show),safe_reset(reset);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This item belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","This Instance changed; refresh before editing");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before editing");
        return safe_host->session.document();
    };
    const auto update_state=[=]{
        if(!safe_box)return;
        safe_hide->setEnabled(false);safe_show->setEnabled(false);safe_reset->setEnabled(false);
        if(safe_source->currentIndex()<0){
            safe_source->setEnabled(false);safe_source_state->setText("Source: —");
            safe_local_state->setText("Local: —");safe_status->setText("No descendant items");return;
        }
        try{
            const auto& current=current_document();const auto id=safe_source->currentData().toString().toStdString();
            const auto state=object_visibility_state(current,{id,"","object.visible"});
            QString source_label="Source: "+visibility_text(state.evaluated);
            if(state.driver)source_label+=" · Link";else if(state.expression)source_label+=" · Expression";
            safe_source_state->setText(source_label);
            const auto& overrides=current.objects.at(instance).instance->visibility_overrides;
            const auto local=overrides.find(id);const bool overridden=local!=overrides.end();
            safe_local_state->setText(overridden?"Local: "+visibility_text(local->second):"Local: Use Source");
            safe_hide->setEnabled(!overridden||local->second);safe_show->setEnabled(!overridden||!local->second);
            safe_reset->setEnabled(overridden);safe_status->clear();
            if(safe_parent)safe_parent->setProperty(selection_property,QVariantMap{
                {"document",QString::fromStdString(document_id)},{"instance",QString::fromStdString(instance)},
                {"source",QString::fromStdString(id)}});
        }catch(const std::exception& error){if(safe_status)safe_status->setText(QString::fromUtf8(error.what()));}
    };
    QObject::connect(source,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_state](int){update_state();});
    const auto apply=[=](std::optional<bool> visible){
        if(!safe_box||!safe_source||safe_source->currentIndex()<0)return;
        try{
            (void)current_document();const auto id=safe_source->currentData().toString().toStdString();
            if(visible)safe_host->session.apply({DefinitionCommand{SetInstanceVisibilityOverride{instance,id,*visible}}},revision);
            else safe_host->session.apply({DefinitionCommand{ResetInstanceVisibilityOverride{instance,id}}},revision);
            // This may synchronously rebuild and destroy every captured widget.
            safe_host->edited();return;
        }catch(const std::exception& error){if(safe_status)safe_status->setText(QString::fromUtf8(error.what()));}
    };
    QObject::connect(hide,&QPushButton::clicked,box,[apply]{apply(false);});
    QObject::connect(show,&QPushButton::clicked,box,[apply]{apply(true);});
    QObject::connect(reset,&QPushButton::clicked,box,[apply]{apply(std::nullopt);});
    update_state();return box;
}
}
