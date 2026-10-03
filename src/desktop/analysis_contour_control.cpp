#include "analysis_contour_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <memory>

namespace nect::desktop {
QDialog* make_analysis_contour_dialog(Host& host,const Id& composition,const Id& artboard,
    std::function<void(const Id&)> adopted,QWidget* parent) {
    auto* dialog=new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("analysis-contour-dialog");
    dialog->setWindowTitle("Outer contour to Path");
    dialog->resize(480,330);
    auto* layout=new QVBoxLayout(dialog);
    auto* explanation=new QLabel("Analyze the active Artboard's artwork, then choose one outer contour. "
        "Creates a separate editable Path with straight anchors. Source artwork is kept. "
        "Holes are omitted; no smoothing or curve fitting is applied.",dialog);
    explanation->setWordWrap(true);layout->addWidget(explanation);
    auto* form=new QFormLayout;
    auto* scale=new QDoubleSpinBox(dialog);scale->setObjectName("analysis-contour-scale");
    scale->setRange(0.001,16);scale->setDecimals(3);scale->setValue(1);scale->setSuffix(" px / unit");
    form->addRow("Resolution",scale);
    auto* threshold=new QSpinBox(dialog);threshold->setObjectName("analysis-contour-threshold");
    threshold->setRange(1,255);threshold->setValue(128);form->addRow("Alpha threshold",threshold);
    auto* analyze=new QPushButton("Analyze artwork",dialog);analyze->setObjectName("analysis-contour-analyze");
    form->addRow(analyze);
    auto* choices=new QComboBox(dialog);choices->setObjectName("analysis-contour-choice");
    form->addRow("Outer contour",choices);
    auto* name=new QLineEdit("Traced outer contour",dialog);name->setObjectName("analysis-contour-name");
    form->addRow("Path name",name);layout->addLayout(form);
    auto* status=new QLabel("Analysis is read-only. Cancel leaves artwork and history unchanged.",dialog);
    status->setObjectName("analysis-contour-status");status->setWordWrap(true);layout->addWidget(status);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Cancel,dialog);
    auto* apply=buttons->addButton("Create Path",QDialogButtonBox::AcceptRole);
    apply->setObjectName("analysis-contour-adopt");apply->setEnabled(false);layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    struct Draft {QString analysis_id;};
    auto draft=std::make_shared<Draft>();
    const auto identity=host.session_id;
    const auto revision=host.session.revision();
    auto invalidate=[draft,choices,apply,status] {
        draft->analysis_id.clear();choices->clear();apply->setEnabled(false);
        status->setText("Settings changed. Analyze artwork to refresh the contours.");
    };
    QObject::connect(scale,&QDoubleSpinBox::valueChanged,dialog,invalidate);
    QObject::connect(threshold,&QSpinBox::valueChanged,dialog,invalidate);
    QObject::connect(analyze,&QPushButton::clicked,dialog,[&,composition,artboard,identity,revision,
        draft,scale,threshold,choices,apply,status] {
        draft->analysis_id.clear();choices->clear();apply->setEnabled(false);
        try {
            if(host.session_id!=identity)throw Error("SESSION_CONFLICT","Document changed; close and reopen this dialog");
            const auto result=host.analyze_regions(composition,artboard,scale->value(),threshold->value(),revision);
            draft->analysis_id=result.value("analysis_id").toString();
            for(const auto& value:result.value("outer_contours").toArray()) {
                const auto contour=value.toObject();
                choices->addItem(QString("Region %1 - %2 anchors").arg(contour.value("region_index").toInt()+1)
                    .arg(contour.value("vertices").toArray().size()),contour.value("id").toString());
            }
            apply->setEnabled(choices->count()>0);
            status->setText(choices->count()?QString("%1 outer contours. Create Path adds one Undo step.").arg(choices->count()):
                "No outer contours at this threshold.");
        } catch(const std::exception& error) {status->setText(QString::fromUtf8(error.what()));}
    });
    QObject::connect(apply,&QPushButton::clicked,dialog,[&,composition,artboard,identity,revision,
        draft,scale,threshold,choices,name,apply,status,dialog,adopted] {
        apply->setEnabled(false);
        try {
            const auto result=host.adopt_analysis_contour(composition,artboard,scale->value(),threshold->value(),
                draft->analysis_id,choices->currentData().toString(),name->text().toStdString(),identity,revision);
            const auto object=result.value("object_id").toString().toStdString();
            dialog->accept();
            if(adopted)adopted(object);
        } catch(const std::exception& error) {status->setText(QString::fromUtf8(error.what()));}
    });
    return dialog;
}
}
