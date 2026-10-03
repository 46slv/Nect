#include "artboard_background_control.hpp"
#include "host.hpp"
#include <QColorDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
namespace nect::desktop {
QWidget* make_artboard_background_controls(Host& host,const Id& composition,const Id& artboard,QWidget* parent){
    const auto& d=host.session.document();const auto comp=std::find_if(d.compositions.begin(),d.compositions.end(),[&](const auto& c){return c.id==composition;});
    if(comp==d.compositions.end())throw Error("MISSING_COMPOSITION",composition);
    const auto board=std::find_if(comp->artboards.begin(),comp->artboards.end(),[&](const auto& b){return b.id==artboard;});
    if(board==comp->artboards.end())throw Error("MISSING_ARTBOARD",artboard);
    const auto state=artboard_background_state(*comp,artboard);const auto original=state.value;
    const auto session_id=host.session_id;const auto document_id=d.id;const auto revision=host.session.revision();
    const bool assigned=board->template_assignment.has_value();const auto authored=board->background;
    auto* box=new QGroupBox("Background",parent);box->setObjectName("artboard-background-controls");
    auto* layout=new QVBoxLayout(box);auto* origin=new QLabel(box);origin->setTextFormat(Qt::PlainText);
    origin->setObjectName("artboard-background-state");origin->setWordWrap(true);
    QString text=state.inherited?"Inherited":state.overridden?"Local override":"Local";
    if(state.inherited){const auto source=std::find_if(comp->artboards.begin(),comp->artboards.end(),[&](const auto& b){return b.id==state.source_artboard;});
        text+=" · "+QString::fromStdString(source==comp->artboards.end()?state.source_artboard:source->name);
    }
    text+=original?" · Color":" · None";origin->setText(text);layout->addWidget(origin);
    auto* row=new QHBoxLayout;auto* choose=new QPushButton("Color…",box);choose->setObjectName("artboard-background-color");
    auto* clear=new QPushButton("None",box);clear->setObjectName("artboard-background-none");
    auto* reset=new QPushButton("Reset",box);reset->setObjectName("artboard-background-reset");reset->setEnabled(assigned&&state.overridden);
    row->addWidget(choose);row->addWidget(clear);row->addWidget(reset);layout->addLayout(row);
    auto* status=new QLabel(box);status->setTextFormat(Qt::PlainText);status->setWordWrap(true);status->setObjectName("artboard-background-error");layout->addWidget(status);
    const QPointer<QLabel> safe_status(status);const QPointer<QGroupBox> safe_box(box);
    const auto apply=[&host,composition,artboard,session_id,document_id,revision,safe_status,safe_box](ArtboardTemplateMutation mutation){
        if(!safe_box)return;
        try{
            if(host.session_id!=session_id||host.session.document().id!=document_id)throw Error("SESSION_CONFLICT","This background belongs to another document");
            if(host.session.revision()!=revision)throw Error("REVISION_CONFLICT","This Artboard changed; refresh before applying the color");
            if(host.session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current edit first");
            host.session.apply({ArtboardTemplateCommand{std::move(mutation)}},revision);host.edited();
        }catch(const std::exception& e){if(safe_status)safe_status->setText(QString::fromUtf8(e.what()));}
    };
    QColor initial=Qt::white;
    if(original){const auto& rgba=original->rgba;initial=QColor::fromRgbF(rgba[0],rgba[1],rgba[2],rgba[3]);
        choose->setToolTip(QString("RGBA %1, %2, %3, %4").arg(rgba[0],0,'g',17).arg(rgba[1],0,'g',17).arg(rgba[2],0,'g',17).arg(rgba[3],0,'g',17));
    }
    QObject::connect(choose,&QPushButton::clicked,box,[=]{
        if(!safe_box)return;
        auto* dialog=new QColorDialog(initial,safe_box);dialog->setWindowTitle("Artboard background");
        dialog->setOption(QColorDialog::ShowAlphaChannel);
        const auto displayed_initial=dialog->currentColor();const QPointer<QColorDialog> safe_dialog(dialog);
        const bool accepted=dialog->exec()==QDialog::Accepted;
        const auto selected=safe_dialog?safe_dialog->currentColor():QColor{};
        if(safe_dialog)safe_dialog->deleteLater();
        if(!safe_box||!accepted||!selected.isValid())return;
        ColorValue value;
        // QColor's display precision must not rewrite an untouched authored color.
        if(original&&selected==displayed_initial)value=*original;
        else value.rgba={selected.redF(),selected.greenF(),selected.blueF(),selected.alphaF()};
        if((!assigned||state.overridden)&&authored==std::optional<ColorValue>{value})return;
        apply(SetArtboardBackground{composition,artboard,value});
    });
    QObject::connect(clear,&QPushButton::clicked,box,[=]{
        if((!assigned||state.overridden)&&!authored)return;
        apply(SetArtboardBackground{composition,artboard,std::nullopt});
    });
    QObject::connect(reset,&QPushButton::clicked,box,[=]{apply(ResetArtboardTemplateOverride{composition,artboard,"background"});});
    return box;
}
}
