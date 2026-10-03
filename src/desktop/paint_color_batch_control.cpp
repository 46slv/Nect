#include "paint_color_batch_control.hpp"
#include "host.hpp"
#include <QColorDialog>
#include <QGroupBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

namespace nect::desktop {
namespace {
struct PaintSelection {
    std::vector<ColorValue> colors;
    std::string kind;
    std::size_t slot=0;
};
PaintSelection require_targets(const Document& document,const std::vector<Ref>& targets){
    if(targets.size()<2||targets.size()>1000)
        throw Error("INVALID_SELECTION","Choose 2 to 1000 distinct solid Fill or Stroke targets");
    PaintSelection result;result.colors.reserve(targets.size());std::set<Id> objects;
    // Validate the complete selection before evaluating or constructing commands.
    for(const auto& target:targets){
        if(!target.point.empty()||!objects.insert(target.object).second)
            throw Error("INVALID_SELECTION","Choose distinct whole-object paint targets: "+target.object);
        const auto found=document.objects.find(target.object);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",target.object);
        const auto& object=found->second;
        if(object.kind!=Kind::path&&object.kind!=Kind::text)
            throw Error("UNSUPPORTED_PAINT","Solid paint editing requires Path or Text targets: "+target.object);
        // Look up the retained aggregate Ref, never parse names or infer an ID
        // from another object's stack or use visible object/operation names.
        const auto operation=std::find_if(object.stack.begin(),object.stack.end(),[&](const auto& entry){
            return target==operation_ref(object.id,entry.id,"color");
        });
        if(operation==object.stack.end())
            throw Error("MISSING_COLOR","The retained paint color no longer exists: "+target.object+"/"+target.field);
        if(operation->macro||(operation->type!="nect.paint.fill"&&operation->type!="nect.paint.stroke")||
           (operation->version!=1&&!(operation->type=="nect.paint.stroke"&&operation->version==2)))
            throw Error("UNSUPPORTED_PAINT","Only supported solid Fill or Stroke operations can be edited: "+target.object+"/"+operation->id);
        const auto slot=static_cast<std::size_t>(operation-object.stack.begin());
        if(objects.size()==1){result.kind=operation->type;result.slot=slot;}
        else if(result.kind!=operation->type||result.slot!=slot)
            throw Error("INCOMPATIBLE_PAINT","Every target must have the same Fill/Stroke kind at the same stack position");
        // Even an inactive gradient is retained authored state, not permission
        // to quietly expose/edit its solid fallback as a compatible solid paint.
        if(operation->gradient)
            throw Error("UNSUPPORTED_GRADIENT","Gradient paint is unavailable for this solid-color batch: "+target.object+"/"+operation->id);
        for(const auto& channel:color_channels(document,target)){
            const auto scalar=property(document,channel);
            if(scalar.binding||scalar.expression)
                throw Error("DRIVEN_PROPERTY","Unlink the color explicitly before batch editing: "+target.object+"/"+target.field);
        }
    }
    const auto values=evaluate(document);
    for(const auto& target:targets)result.colors.push_back(color_value(document,target,values));
    return result;
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

QWidget* make_paint_color_batch_controls(Host& host,const std::vector<Ref>& targets,QWidget* parent){
    const auto session_id=host.session_id;const auto document_id=host.session.document().id;
    const auto revision=host.session.revision();const auto gesture=host.session.gesture_generation();
    auto* box=new QGroupBox(QString("Solid color · %1 selected objects").arg(targets.size()),parent);
    box->setObjectName("paint-color-batch-panel");auto* layout=new QVBoxLayout(box);
    auto* state=new QLabel(box);state->setObjectName("paint-color-batch-state");
    state->setTextFormat(Qt::PlainText);state->setWordWrap(true);layout->addWidget(state);
    auto* choose=new QPushButton("Color…",box);choose->setObjectName("paint-color-batch-choose");
    choose->setAccessibleName("sRGB color for every retained solid paint target");
    choose->setToolTip("OK applies one sRGB/straight-alpha color to all retained targets in one Undo step. Cancel keeps every color.");
    choose->setEnabled(false);layout->addWidget(choose);
    auto* status=new QLabel(box);status->setObjectName("paint-color-batch-status");
    status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
    const QPointer<Host> safe_host(&host);const QPointer<QGroupBox> safe_box(box);
    const QPointer<QPushButton> safe_choose(choose);const QPointer<QLabel> safe_status(status);
    const auto current_selection=[safe_host,session_id,document_id,revision,gesture,targets]{
        if(!safe_host||safe_host->session_id!=session_id||safe_host->session.document().id!=document_id)
            throw Error("SESSION_CONFLICT","This paint selection belongs to another document");
        if(safe_host->session.revision()!=revision)
            throw Error("REVISION_CONFLICT","Paint changed; refresh before applying a color");
        if(safe_host->session.gesture_active())
            throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
        if(safe_host->session.gesture_generation()!=gesture)
            throw Error("REVISION_CONFLICT","The edit context changed; refresh before applying a color");
        return require_targets(safe_host->session.document(),targets);
    };
    PaintSelection original;
    try{
        original=current_selection();const auto& first=original.colors.front();
        const bool shared=std::all_of(original.colors.begin(),original.colors.end(),[&](const auto& color){return color==first;});
        box->setTitle(QString("%1 color · stack %2 · %3 selected objects")
            .arg(original.kind=="nect.paint.fill"?"Fill":"Stroke").arg(original.slot+1).arg(targets.size()));
        state->setText(shared?"Shared: "+rgba_text(first):QStringLiteral("Mixed"));
        choose->setAccessibleDescription(shared?rgba_text(first):QStringLiteral("Mixed colors. The picker starts at the first retained target's exact color; OK assigns it to every target."));
        const auto& rgba=first.rgba;
        if(shared)choose->setStyleSheet("border: 3px solid "+QColor::fromRgbF(rgba[0],rgba[1],rgba[2],rgba[3]).name()+";");
        choose->setEnabled(true);
    }catch(const std::exception& error){state->setText("Unavailable");show_error(status,error);}
    QObject::connect(choose,&QPushButton::clicked,box,[=]{
        if(!safe_box||!safe_host||original.colors.empty())return;
        try{
            (void)current_selection();if(safe_status)safe_status->clear();
            const auto& retained=original.colors.front();const auto& rgba=retained.rgba;
            auto* dialog=new QColorDialog(QColor::fromRgbF(rgba[0],rgba[1],rgba[2],rgba[3]),safe_box.data());
            dialog->setObjectName("paint-color-batch-dialog");dialog->setWindowTitle("Selected solid paint color");
            dialog->setOption(QColorDialog::ShowAlphaChannel);
            // Capture what this dialog actually displays after initialization.
            // QColor conversion/display precision must not rewrite untouched RGBA.
            const auto displayed_initial=dialog->currentColor();const QPointer<QColorDialog> safe_dialog(dialog);
            const bool accepted=dialog->exec()==QDialog::Accepted;
            const auto selected=safe_dialog?safe_dialog->currentColor():QColor{};
            if(safe_dialog)safe_dialog->deleteLater();
            if(!safe_box||!safe_host||!accepted||!selected.isValid())return;
            const auto current=current_selection();ColorValue value=retained;
            if(selected!=displayed_initial)value.rgba={selected.redF(),selected.greenF(),selected.blueF(),selected.alphaF()};
            if(std::all_of(current.colors.begin(),current.colors.end(),[&](const auto& color){return color==value;}))return;
            std::vector<Command> commands;commands.reserve(targets.size());
            for(const auto& target:targets)commands.push_back(SetColor{target,value});
            safe_host->session.apply(commands,revision);
            // Refresh can synchronously destroy the panel, button and dialog.
            // Do not touch any of those widgets after edited().
            safe_host->edited();return;
        }catch(const std::exception& error){
            if(safe_choose)safe_choose->setEnabled(false);
            show_error(safe_status.data(),error);
        }
    });
    return box;
}
}
