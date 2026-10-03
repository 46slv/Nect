#include "compatibility_plan_control.hpp"
#include "host.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariant>
#include <algorithm>

namespace nect::desktop {
namespace {
QString text(const Id& id) { return QString::fromStdString(id); }
QString compact(const QJsonObject& object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
QString reference(const QJsonObject& object) {
    const auto ref=object.value("source_ref").toObject();
    QStringList parts{ref.value("object").toString()};
    if(!ref.value("point").toString().isEmpty())parts<<ref.value("point").toString();
    parts<<ref.value("field").toString();
    return parts.join(" / ");
}
QString classification(const QString& value) {
    if(value=="editable_native")return "Native editable";
    if(value=="expandable")return "Expanded";
    if(value=="rasterize_required")return "Planned raster";
    if(value=="unsupported")return "Unsupported";
    return value;
}
QTreeWidget* table(QWidget* parent,const char* name,const QStringList& columns) {
    auto* result=new QTreeWidget(parent);
    result->setObjectName(QString::fromLatin1(name));
    result->setAccessibleName(columns.join(", "));
    result->setHeaderLabels(columns);result->setRootIsDecorated(false);
    result->setSelectionMode(QAbstractItemView::SingleSelection);
    result->setEditTriggers(QAbstractItemView::NoEditTriggers);
    result->setUniformRowHeights(true);
    result->header()->setSectionResizeMode(QHeaderView::Interactive);
    result->header()->setStretchLastSection(true);
    for(int i=0;i<columns.size()-1;++i)result->setColumnWidth(i,i==1?230:150);
    return result;
}
void row(QTreeWidget* target,const QStringList& fields,const QJsonObject& record) {
    auto* item=new QTreeWidgetItem(target,fields);
    const auto encoded=QJsonDocument(record).toJson(QJsonDocument::Compact);
    item->setData(0,Qt::UserRole,encoded);
    for(int i=0;i<fields.size();++i)item->setToolTip(i,fields[i]);
    // Preserve exact occurrence/instance provenance for repeated Definition
    // projections without replacing the stable source Ref with a display label.
    item->setToolTip(0,QString::fromUtf8(encoded));
}
QLabel* label(QWidget* parent,const char* name,const QString& value={}) {
    auto* result=new QLabel(value,parent);result->setObjectName(QString::fromLatin1(name));
    result->setTextFormat(Qt::PlainText);result->setWordWrap(true);
    result->setTextInteractionFlags(Qt::TextSelectableByMouse);return result;
}

class CompatibilityPlanDialog final : public QDialog {
public:
    CompatibilityPlanDialog(Host& host,const Id& composition,const Id& artboard,QWidget* parent)
        :QDialog(parent),host_(&host) {
        setAttribute(Qt::WA_DeleteOnClose);setObjectName("compatibility-plan-dialog");
        setWindowTitle("Compatibility plan");resize(820,620);
        auto* layout=new QVBoxLayout(this);
        layout->addWidget(label(this,"compatibility-plan-read-only",
            "Read-only · committed source. No derivative is generated. Planned bakes are not executed. AI/PDF export is unavailable."));
        auto* form=new QFormLayout;
        composition_=new QComboBox(this);composition_->setObjectName("compatibility-plan-composition");
        artboard_=new QComboBox(this);artboard_->setObjectName("compatibility-plan-artboard");
        profile_=new QComboBox(this);profile_->setObjectName("compatibility-plan-profile");
        profile_->addItem("SVG 1.1 + CSS compositing / isolation","svg/1.1+css-compositing");
        for(auto* choice:{composition_,artboard_,profile_}) {
            choice->setMinimumContentsLength(18);
            choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            choice->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        }
        form->addRow("Composition",composition_);form->addRow("Artboard",artboard_);
        form->addRow("Target profile",profile_);
        scale_=new QDoubleSpinBox(this);scale_->setObjectName("compatibility-plan-scale");
        scale_->setRange(.001,16);scale_->setDecimals(3);scale_->setValue(1);
        scale_->setSuffix(" px / unit");form->addRow("Planned raster scale",scale_);
        layout->addLayout(form);
        source_=label(this,"compatibility-plan-source");layout->addWidget(source_);
        auto* controls=new QHBoxLayout;
        request_=new QPushButton("Request plan",this);request_->setObjectName("compatibility-plan-request");
        cancel_=new QPushButton("Cancel request",this);cancel_->setObjectName("compatibility-plan-cancel");
        cancel_->setToolTip("Cancels queued work. Planning is synchronous once it starts.");
        cancel_->setEnabled(false);
        auto* refresh=new QPushButton("Refresh source",this);refresh->setObjectName("compatibility-plan-refresh");
        controls->addWidget(request_);controls->addWidget(cancel_);controls->addStretch();
        controls->addWidget(refresh);layout->addLayout(controls);
        summary_=label(this,"compatibility-plan-summary");layout->addWidget(summary_);
        tabs_=new QTabWidget(this);tabs_->setObjectName("compatibility-plan-tabs");
        items_=table(this,"compatibility-plan-items",{"Classification","Source reference","Feature","Reason"});
        tabs_->addTab(items_,"Items");
        auto* unsupported_page=new QWidget(this);auto* unsupported_layout=new QVBoxLayout(unsupported_page);
        unsupported_=table(unsupported_page,"compatibility-plan-unsupported",{"Source reference","Feature","Reason"});
        unsupported_layout->addWidget(unsupported_);
        unsupported_layout->addWidget(label(unsupported_page,"compatibility-plan-legacy-label","Current SVG encoder refusals"));
        legacy_=table(unsupported_page,"compatibility-plan-legacy-refusals",{"Source","Feature","Reason"});
        unsupported_layout->addWidget(legacy_);tabs_->addTab(unsupported_page,"Unsupported / encoder");
        auto* losses_page=new QWidget(this);auto* losses_layout=new QVBoxLayout(losses_page);
        losses_=table(losses_page,"compatibility-plan-losses",{"Source reference","Reason","Planned loss"});
        losses_layout->addWidget(losses_);
        losses_layout->addWidget(label(losses_page,"compatibility-plan-bakes-label","Planned bake groups · execution unavailable"));
        bakes_=table(losses_page,"compatibility-plan-bakes",{"Root","Bounds","Scale","Members / borrowed dependencies"});
        losses_layout->addWidget(bakes_);tabs_->addTab(losses_page,"Planned losses");
        warnings_=new QPlainTextEdit(this);warnings_->setObjectName("compatibility-plan-warnings");
        warnings_->setReadOnly(true);tabs_->addTab(warnings_,"Warnings");layout->addWidget(tabs_,1);
        status_=label(this,"compatibility-plan-status","Choose a source and request a plan.");layout->addWidget(status_);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);
        buttons->button(QDialogButtonBox::Close)->setObjectName("compatibility-plan-close");
        layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
        connect(this,&QDialog::finished,this,[this] { discard("Closed."); });
        connect(composition_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this] {
            update_artboards({});discard("Source selection changed. Request a new plan.");
        });
        connect(artboard_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this] {
            discard("Source selection changed. Request a new plan.");
        });
        connect(profile_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this] {
            discard("Profile changed. Request a new plan.");
        });
        connect(scale_,&QDoubleSpinBox::valueChanged,this,[this] {
            discard("Scale changed. Request a new plan.");
        });
        connect(request_,&QPushButton::clicked,this,[this] { request_plan(); });
        connect(cancel_,&QPushButton::clicked,this,[this] { discard("Request cancelled. Source unchanged."); });
        connect(refresh,&QPushButton::clicked,this,[this] { refresh_source(); });
        auto* watcher=new QTimer(this);watcher->setInterval(200);
        connect(watcher,&QTimer::timeout,this,[this] {
            if(!current_source())refresh_source();
        });watcher->start();
        refresh_source(text(composition),text(artboard));
    }
private:
    struct Source { QString session,document;std::uint64_t revision=0; };
    QPointer<Host> host_;
    Source captured_;
    std::uint64_t generation_=0;
    bool pending_=false;
    QComboBox *composition_=nullptr,*artboard_=nullptr,*profile_=nullptr;
    QDoubleSpinBox* scale_=nullptr;
    QPushButton *request_=nullptr,*cancel_=nullptr;
    QLabel *source_=nullptr,*summary_=nullptr,*status_=nullptr;
    QTabWidget* tabs_=nullptr;
    QTreeWidget *items_=nullptr,*unsupported_=nullptr,*legacy_=nullptr,*losses_=nullptr,*bakes_=nullptr;
    QPlainTextEdit* warnings_=nullptr;

    bool current_source() const {
        return host_&&host_->session_id==captured_.session&&
            text(host_->session.document().id)==captured_.document&&host_->session.revision()==captured_.revision;
    }
    void availability() {
        request_->setEnabled(host_&&composition_->currentIndex()>=0&&artboard_->currentIndex()>=0&&!pending_);
        cancel_->setEnabled(pending_);
    }
    void discard(const QString& message) {
        ++generation_;pending_=false;
        setProperty("nect-compatibility-plan",QVariant{});
        for(auto* view:{items_,unsupported_,legacy_,losses_,bakes_})view->clear();
        warnings_->clear();summary_->clear();status_->setText(message);availability();
    }
    void update_artboards(const QString& preferred) {
        const QSignalBlocker blocker(artboard_);artboard_->clear();
        if(!current_source())return;
        const auto id=composition_->currentData().toString().toStdString();
        const auto& compositions=host_->session.document().compositions;
        const auto found=std::find_if(compositions.begin(),compositions.end(),[&](const auto& c) { return c.id==id; });
        if(found==compositions.end())return;
        for(const auto& board:found->artboards) {
            artboard_->addItem(text(board.name.empty()?board.id:board.name)+" · "+text(board.id),text(board.id));
            artboard_->setItemData(artboard_->count()-1,text(board.id),Qt::ToolTipRole);
        }
        const auto index=artboard_->findData(preferred);
        if(index>=0)artboard_->setCurrentIndex(index);
    }
    void refresh_source(QString preferred_composition={},QString preferred_artboard={}) {
        const bool changed=!captured_.session.isEmpty()&&!current_source();
        if(preferred_composition.isEmpty())preferred_composition=composition_->currentData().toString();
        if(preferred_artboard.isEmpty())preferred_artboard=artboard_->currentData().toString();
        const QSignalBlocker blocker(composition_);composition_->clear();
        if(!host_) {
            artboard_->clear();source_->setText("Source unavailable.");discard("The document host closed.");return;
        }
        captured_={host_->session_id,text(host_->session.document().id),host_->session.revision()};
        source_->setText(QString("Document %1 · committed revision %2").arg(captured_.document).arg(static_cast<qulonglong>(captured_.revision)));
        for(const auto& comp:host_->session.document().compositions) {
            composition_->addItem(text(comp.name.empty()?comp.id:comp.name)+" · "+text(comp.id),text(comp.id));
            composition_->setItemData(composition_->count()-1,text(comp.id),Qt::ToolTipRole);
        }
        const auto index=composition_->findData(preferred_composition);
        if(index>=0)composition_->setCurrentIndex(index);
        update_artboards(preferred_artboard);
        discard(changed?"Source changed. Request a new plan.":"Choose a source and request a plan.");
        if(artboard_->count()==0)status_->setText("This Composition has no Artboards.");
    }
    void request_plan() {
        if(pending_)return;
        if(!current_source()) { refresh_source();return; }
        if(composition_->currentIndex()<0||artboard_->currentIndex()<0)return;
        discard("Planning the committed source…");pending_=true;availability();
        const auto generation=generation_;
        const auto captured=captured_;
        const auto composition=composition_->currentData().toString();
        const auto artboard=artboard_->currentData().toString();
        const auto profile=profile_->currentData().toString();
        const QJsonObject request{{"op","compatibility_plan"},{"composition",composition},{"artboard",artboard},
            {"target_profile",profile},{"options",QJsonObject{{"raster_scale",scale_->value()}}}};
        // Queuing permits Cancel/Close before work starts. Host dispatch itself
        // is synchronous; there is no worker or mutation to interrupt or undo.
        QTimer::singleShot(0,this,[this,generation,captured,composition,artboard,profile,request] {
            if(generation!=generation_||!pending_)return;
            if(!current_source()) { refresh_source();return; }
            const QJsonObject envelope{{"op","core"},{"session_id",captured.session},
                {"document_id",captured.document},{"request",request}};
            const auto response=QJsonDocument::fromJson(host_->dispatch(QJsonDocument(envelope).toJson(QJsonDocument::Compact))).object();
            if(generation!=generation_||!pending_)return;
            pending_=false;availability();
            if(!current_source()) { refresh_source();return; }
            if(!response.value("ok").toBool()) {
                const auto error=response.value("error").toObject();
                status_->setText(error.value("code").toString()+": "+error.value("message").toString());return;
            }
            const auto plan=response.value("result").toObject();
            // Reject any response from another source or selection. Read-only
            // core operations have no expected_revision field; the guards here
            // deliberately keep that canonical request schema unchanged.
            if(response.value("session_id").toString()!=captured.session||
               response.value("document_id").toString()!=captured.document||
               response.value("revision").toInteger(-1)!=static_cast<qint64>(captured.revision)||
               plan.value("source_revision").toInteger(-1)!=static_cast<qint64>(captured.revision)||
               plan.value("source_document").toString()!=captured.document||
               plan.value("composition").toString()!=composition||plan.value("artboard").toString()!=artboard||
               plan.value("target_profile").toString()!=profile) {
                discard("Plan source mismatch. Request a new plan.");return;
            }
            display(plan);
        });
    }
    void display(const QJsonObject& plan) {
        setProperty("nect-compatibility-plan",plan.toVariantMap());
        const auto counts=plan.value("counts").toObject();
        summary_->setText(QString("Native editable %1 · expanded %2 · planned raster %3 · unsupported %4\n"
            "Current SVG encoder admission: %5 · no derivative generated")
            .arg(counts.value("editable_native").toInt()).arg(counts.value("expandable").toInt())
            .arg(counts.value("rasterize_required").toInt()).arg(counts.value("unsupported").toInt())
            .arg(plan.value("export_supported").toBool()?"allowed":"refused"));
        for(const auto& value:plan.value("items").toArray()) {
            const auto item=value.toObject();const auto kind=item.value("classification").toString();
            const QStringList fields{reference(item),item.value("semantic_type").toString(),item.value("reason_code").toString()};
            row(items_,QStringList{classification(kind)}+fields,item);
            if(kind=="unsupported")row(unsupported_,fields,item);
        }
        const auto legacy=plan.value("legacy_export_plan").toObject();
        for(const auto* field:{"unsupported_effects","unsupported_masks","unsupported_blends"})
            for(const auto& value:legacy.value(QString::fromLatin1(field)).toArray()) {
                const auto item=value.toObject();
                const auto feature=item.value("type").toString(item.value("mode").toString(item.value("blend").toString()));
                row(legacy_,{item.value("object").toString(),feature,item.value("reason").toString()},item);
            }
        for(const auto& value:plan.value("editability_losses").toArray()) {
            const auto loss=value.toObject();row(losses_,{reference(loss),loss.value("reason_code").toString(),loss.value("loss").toString()},loss);
        }
        for(const auto& value:plan.value("bake_groups").toArray()) {
            const auto bake=value.toObject();QStringList members,dependencies;
            for(const auto& id:bake.value("members").toArray())members<<id.toString();
            for(const auto& id:bake.value("borrowed_dependencies").toArray())dependencies<<id.toString();
            row(bakes_,{bake.value("root").toString(),compact(bake.value("bounds").toObject()),
                QString::number(bake.value("scale").toDouble()),members.join(", ")+" / "+dependencies.join(", ")},bake);
        }
        QStringList warnings;
        for(const auto& value:plan.value("warnings").toArray())warnings<<value.toString();
        warnings_->setPlainText(warnings.join("\n\n"));
        status_->setText("Plan ready · read-only · "+plan.value("plan_id").toString());
    }
};
}
QDialog* create_compatibility_plan_dialog(Host& host,const Id& composition,const Id& artboard,QWidget* parent) {
    return new CompatibilityPlanDialog(host,composition,artboard,parent);
}
}
