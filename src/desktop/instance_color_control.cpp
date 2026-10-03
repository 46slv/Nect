#include "instance_color_control.hpp"
#include "host.hpp"
#include <QColorDialog>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVariantMap>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <memory>

namespace nect::desktop {
namespace {
constexpr const char* selection_property="nect-instance-color-selection";
bool solid_fill(const ProcessingEntry& entry){
    // An inactive gradient is still authored gradient paint, not a solid Fill.
    return entry.type=="nect.paint.fill"&&!entry.macro&&!entry.gradient;
}
const Definition& require_definition(const Document& document,const Id& instance){
    const auto found=document.objects.find(instance);
    if(found==document.objects.end())throw Error("MISSING_OBJECT",instance);
    if(found->second.kind!=Kind::instance||!found->second.instance)
        throw Error("TYPE_MISMATCH","Fill color requires a Definition Instance");
    const auto definition=document.definitions.find(found->second.instance->definition);
    if(definition==document.definitions.end())throw Error("MISSING_DEFINITION",found->second.instance->definition);
    return definition->second;
}
void require_target(const Document& document,const Id& instance,const Ref& target){
    const auto& definition=require_definition(document,instance);
    std::function<bool(const Id&)> descendant=[&](const Id& id){
        for(const auto& child:document.objects.at(id).children)
            if(child==target.object||descendant(child))return true;
        return false;
    };
    if(!target.point.empty()||target.object==definition.root||!descendant(definition.root))
        throw Error("UNSUPPORTED_OVERRIDE","Choose a descendant source Object and solid Fill");
    const auto& object=document.objects.at(target.object);
    const auto found=std::find_if(object.stack.begin(),object.stack.end(),[&](const auto& entry){
        return target==operation_ref(object.id,entry.id,"color");
    });
    if(found==object.stack.end()||!solid_fill(*found))
        throw Error("UNSUPPORTED_OVERRIDE","Color overrides support descendant solid Fill operations only");
}
QString rgba_text(const ColorValue& color){
    const auto& rgba=color.rgba;
    return QString("RGBA %1, %2, %3, %4").arg(rgba[0],0,'g',17).arg(rgba[1],0,'g',17)
        .arg(rgba[2],0,'g',17).arg(rgba[3],0,'g',17);
}
void show_error(QLabel* status,const std::exception& error){
    if(!status)return;
    if(const auto* typed=dynamic_cast<const Error*>(&error))
        status->setText(QString::fromStdString(typed->code)+": "+QString::fromUtf8(error.what()));
    else status->setText(QString::fromUtf8(error.what()));
}
}

QWidget* make_instance_color_controls(Host& host,const Id& instance,QWidget* parent){
    const auto& document=host.session.document();const auto& definition=require_definition(document,instance);
    const auto session_id=host.session_id;const auto document_id=document.id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox("Item Fill color",parent);box->setObjectName("instance-color-controls");
    box->setProperty("nect-instance-id",QString::fromStdString(instance));auto* layout=new QVBoxLayout(box);
    auto* source=new QComboBox(box);source->setObjectName("instance-color-source-object");
    source->setAccessibleName("Source Object with solid Fill");
    auto* fill=new QComboBox(box);fill->setObjectName("instance-color-source-fill");
    fill->setAccessibleName("Source Fill operation");
    for(auto* selector:{source,fill}){
        selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        selector->setMinimumContentsLength(12);selector->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    }
    std::function<void(const Id&)> add_descendants=[&](const Id& id){
        for(const auto& child:document.objects.at(id).children){
            const auto& object=document.objects.at(child);
            if(std::any_of(object.stack.begin(),object.stack.end(),solid_fill)){
                const auto name=QString::fromStdString(object.name.empty()?child:object.name);
                source->addItem(name+" · "+QString::fromStdString(child),QString::fromStdString(child));
                source->setItemData(source->count()-1,QString::fromStdString(child),Qt::ToolTipRole);
            }
            add_descendants(child);
        }
    };
    add_descendants(definition.root);
    auto* object_label=new QLabel("Source Object",box);object_label->setBuddy(source);
    auto* fill_label=new QLabel("Source Fill",box);fill_label->setBuddy(fill);
    layout->addWidget(object_label);layout->addWidget(source);layout->addWidget(fill_label);layout->addWidget(fill);
    Id saved_fill;
    if(parent){
        const auto saved=parent->property(selection_property).toMap();
        if(saved.value("document").toString()==QString::fromStdString(document_id)&&
           saved.value("instance").toString()==QString::fromStdString(instance)){
            const auto index=source->findData(saved.value("object"));
            if(index>=0){source->setCurrentIndex(index);saved_fill=saved.value("fill").toString().toStdString();}
        }
    }
    auto* source_state=new QLabel(box);source_state->setObjectName("instance-color-source-state");
    auto* local_state=new QLabel(box);local_state->setObjectName("instance-color-local-state");
    for(auto* label:{source_state,local_state}){label->setTextFormat(Qt::PlainText);label->setWordWrap(true);layout->addWidget(label);}
    auto* row=new QHBoxLayout;
    auto* choose=new QPushButton("Color…",box);choose->setObjectName("instance-color-choose");
    choose->setAccessibleName("Local sRGB Fill color for this Instance");
    choose->setToolTip("OK sets a literal sRGB/straight-alpha color for this Fill in this Instance only. Source links and expressions stay unchanged. Cancel keeps the current local/inherited color.");
    auto* reset=new QPushButton("Use Source",box);reset->setObjectName("instance-color-reset");
    reset->setToolTip("Removes only the selected Fill's local color override and follows the current source color.");
    row->addWidget(choose);row->addWidget(reset);layout->addLayout(row);
    auto* note=new QLabel("Descendant solid Fill only. Stroke and gradient paint are unavailable. OK can freeze the current source color locally.",box);
    note->setTextFormat(Qt::PlainText);note->setWordWrap(true);layout->addWidget(note);
    auto* status=new QLabel(box);status->setObjectName("instance-color-error");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);

    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QWidget> safe_parent(parent);const QPointer<QComboBox> safe_source(source),safe_fill(fill);
    const QPointer<QLabel> safe_source_state(source_state),safe_local_state(local_state),safe_status(status);
    const QPointer<QPushButton> safe_choose(choose),safe_reset(reset);
    const auto picker_active=std::make_shared<bool>(false);
    const auto current_document=[safe_host,session_id,document_id,revision,gesture]()->const Document&{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This Fill belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","This Instance changed; refresh before editing");
        if(safe_host->session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before editing");
        return safe_host->session.document();
    };
    const auto selected_target=[safe_source,safe_fill]{
        if(!safe_source||!safe_fill||safe_source->currentIndex()<0||safe_fill->currentIndex()<0)
            throw Error("NO_COLOR_TARGET","Choose a descendant source Object and solid Fill");
        return operation_ref(safe_source->currentData().toString().toStdString(),
            safe_fill->currentData().toString().toStdString(),"color");
    };
    const auto update_state=[=]{
        if(!safe_box||!safe_source||!safe_fill||!safe_choose||!safe_reset||!safe_source_state||!safe_local_state)return;
        safe_choose->setEnabled(false);safe_reset->setEnabled(false);
        if(safe_source->currentIndex()<0||safe_fill->currentIndex()<0){
            safe_source->setEnabled(safe_source->count()>0);safe_fill->setEnabled(false);
            safe_source_state->setText("Source: —");safe_local_state->setText("Local: —");
            if(safe_status)safe_status->setText("No descendant solid Fill colors");
            return;
        }
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            const auto evaluated=color_value(current,target,evaluate(current));
            bool linked=false,expression=false;
            for(const auto& channel:color_channels(current,target)){
                const auto scalar=property(current,channel);linked=linked||scalar.binding.has_value();expression=expression||scalar.expression.has_value();
            }
            QString text="Source: "+rgba_text(evaluated);
            if(linked)text+=" · Link";
            if(expression)text+=" · Expression";
            safe_source_state->setText(text);
            const auto& overrides=current.objects.at(instance).instance->color_overrides;const auto local=overrides.find(target);
            const bool overridden=local!=overrides.end();const auto exact=overridden?local->second:evaluated;
            safe_local_state->setText(overridden?"Local: "+rgba_text(exact):"Local: Use Source");
            safe_choose->setAccessibleDescription(rgba_text(exact));const auto& rgba=exact.rgba;
            safe_choose->setStyleSheet("border: 3px solid "+QColor::fromRgbF(rgba[0],rgba[1],rgba[2],rgba[3]).name()+";");
            safe_choose->setEnabled(!*picker_active);safe_reset->setEnabled(overridden&&!*picker_active);
            safe_fill->setEnabled(true);if(safe_status)safe_status->clear();
            if(safe_parent)safe_parent->setProperty(selection_property,QVariantMap{
                {"document",QString::fromStdString(document_id)},{"instance",QString::fromStdString(instance)},
                {"object",QString::fromStdString(target.object)},{"fill",safe_fill->currentData()}});
        }catch(const std::exception& error){
            safe_source_state->setText("Source: Unavailable");safe_local_state->setText("Local: Unavailable");show_error(safe_status.data(),error);
        }
    };
    const auto refill=[=](const Id& retained){
        if(!safe_box||!safe_source||!safe_fill)return;
        try{
            const auto& current=current_document();const QSignalBlocker blocker(safe_fill.data());safe_fill->clear();
            if(safe_source->currentIndex()>=0){
                const auto id=safe_source->currentData().toString().toStdString();const auto& object=current.objects.at(id);
                for(std::size_t slot=0;slot<object.stack.size();++slot){
                    const auto& entry=object.stack[slot];if(!solid_fill(entry))continue;
                    const auto target=operation_ref(id,entry.id,"color");require_target(current,instance,target);
                    safe_fill->addItem(QString("Fill · stack %1 · %2").arg(slot+1).arg(QString::fromStdString(entry.id)),QString::fromStdString(entry.id));
                    safe_fill->setItemData(safe_fill->count()-1,QString::fromStdString(target.field),Qt::ToolTipRole);
                }
                const auto index=safe_fill->findData(QString::fromStdString(retained));if(index>=0)safe_fill->setCurrentIndex(index);
            }
            update_state();
        }catch(const std::exception& error){
            if(safe_choose)safe_choose->setEnabled(false);
            if(safe_reset)safe_reset->setEnabled(false);
            if(safe_source_state)safe_source_state->setText("Source: Unavailable");
            if(safe_local_state)safe_local_state->setText("Local: Unavailable");
            show_error(safe_status.data(),error);
        }
    };
    QObject::connect(source,qOverload<int>(&QComboBox::currentIndexChanged),box,[refill](int){refill({});});
    QObject::connect(fill,qOverload<int>(&QComboBox::currentIndexChanged),box,[update_state](int){update_state();});
    QObject::connect(choose,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_host||*picker_active)return;
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            const auto& overrides=current.objects.at(instance).instance->color_overrides;const auto local=overrides.find(target);
            const auto retained=local!=overrides.end()?local->second:color_value(current,target,evaluate(current));
            const auto& rgba=retained.rgba;
            auto* dialog=new QColorDialog(QColor::fromRgbF(rgba[0],rgba[1],rgba[2],rgba[3]),safe_box.data());
            dialog->setObjectName("instance-color-dialog");dialog->setWindowTitle("Local Instance Fill color");
            dialog->setOption(QColorDialog::ShowAlphaChannel);
            // Retain exact doubles when the displayed QColor is unchanged,
            // including a user moving away and then returning to this baseline.
            const auto displayed_initial=dialog->currentColor();const QPointer<QColorDialog> safe_dialog(dialog);
            *picker_active=true;update_state();const bool accepted=dialog->exec()==QDialog::Accepted;*picker_active=false;
            const auto selected=safe_dialog?safe_dialog->currentColor():QColor{};if(safe_dialog)safe_dialog->deleteLater();
            if(!safe_box||!safe_host)return;
            if(!accepted||!selected.isValid()){update_state();return;}
            const auto& latest=current_document();require_target(latest,instance,target);ColorValue value=retained;
            if(selected!=displayed_initial)value.rgba={selected.redF(),selected.greenF(),selected.blueF(),selected.alphaF()};
            const auto& latest_overrides=latest.objects.at(instance).instance->color_overrides;const auto existing=latest_overrides.find(target);
            if(existing!=latest_overrides.end()&&existing->second==value){update_state();return;}
            // No override + explicit same-color OK intentionally freezes the
            // exact evaluated source color. It does not unlink the source.
            safe_host->session.apply({DefinitionCommand{SetInstanceColorOverride{instance,target,value}}},revision);
            safe_host->edited();return; // Refresh may synchronously destroy all widgets.
        }catch(const std::exception& error){
            *picker_active=false;
            if(safe_choose)safe_choose->setEnabled(false);
            if(safe_reset)safe_reset->setEnabled(false);
            show_error(safe_status.data(),error);
        }
    });
    QObject::connect(reset,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_host||*picker_active)return;
        try{
            const auto& current=current_document();const auto target=selected_target();require_target(current,instance,target);
            if(!current.objects.at(instance).instance->color_overrides.contains(target))return;
            safe_host->session.apply({DefinitionCommand{ResetInstanceColorOverride{instance,target}}},revision);
            safe_host->edited();return;
        }catch(const std::exception& error){
            if(safe_choose)safe_choose->setEnabled(false);
            if(safe_reset)safe_reset->setEnabled(false);
            show_error(safe_status.data(),error);
        }
    });
    refill(saved_fill);return box;
}
}
