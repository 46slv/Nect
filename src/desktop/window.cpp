#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <cmath>
#include <algorithm>
#include <set>
#include <QTreeWidgetItemIterator>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>

namespace nect::desktop {
namespace {
QString qs(const std::string& s) { return QString::fromStdString(s); }
QJsonObject ref_json(const Ref& r) { return {{"object",qs(r.object)},{"point",qs(r.point)},{"field",qs(r.field)}}; }
Ref read_ref(const QByteArray& data) {
    const auto o=QJsonDocument::fromJson(data).object();
    if(!o.value("object").isString()||!o.value("point").isString()||!o.value("field").isString())
        throw Error("INVALID_REFERENCE","Clipboard does not contain a Nect property reference");
    return {o["object"].toString().toStdString(),o["point"].toString().toStdString(),o["field"].toString().toStdString()};
}
const char* reference_mime="application/x-nect-property-reference";
class WhipOverlay final : public QWidget {
public:
    QPoint start,end;
    explicit WhipOverlay(QWidget* parent):QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#84d5eb"),2));
        p.drawLine(start,end);p.drawEllipse(end,4,4);
    }
};
QString display_value(double value) { return QString::number(value,'g',12); }
QString primitive_label(const Primitive& source) {
    return source.type=="nect.shape.circle" ? QStringLiteral("Circle") : QStringLiteral("Rectangle");
}
QString parameter_label(const std::string& parameter) {
    if(parameter=="center_x")return QStringLiteral("Center X");
    if(parameter=="center_y")return QStringLiteral("Center Y");
    if(parameter=="radius")return QStringLiteral("Radius");
    if(parameter=="width")return QStringLiteral("Width");
    if(parameter=="height")return QStringLiteral("Height");
    return qs(parameter);
}
QString point_label(const Object& object,const Point& point,std::size_t index) {
    if(object.source) {
        const auto prefix=object.source->id+"-";
        if(point.id.starts_with(prefix)) {
            auto label=qs(point.id.substr(prefix.size())).replace('-', ' ');
            if(!label.isEmpty())label[0]=label.at(0).toUpper();
            return label;
        }
    }
    return "Point "+QString::number(index+1);
}
QString property_label(const Document& d,const Ref& ref) {
    QStringList path;
    auto object=ref.object;
    for(;;) {
        path.prepend(qs(d.objects.at(object).name));
        Id parent;
        for(const auto& [id,o]:d.objects)
            if(std::find(o.children.begin(),o.children.end(),object)!=o.children.end()) {parent=id;break;}
        if(parent.empty())break;
        object=parent;
    }
    if(!ref.point.empty()) {
        const auto& o=d.objects.at(ref.object);
        const auto contours=path_contours(o);
        for(std::size_t c=0;c<contours.size();++c)for(std::size_t i=0;i<contours[c].points.size();++i)
            if(contours[c].points[i].id==ref.point) {
                if(contours.size()>1)path.append("Contour "+QString::number(c+1));
                path.append(point_label(o,contours[c].points[i],i));
            }
    }
    if(ref.field.starts_with("generator.")) {
        path.append("Generator");path.append(parameter_label(ref.field.substr(10)));
    } else path.append(qs(ref.field));
    return path.join(" / ");
}
}

Window::Window(QString recovery_directory):host(std::move(recovery_directory),this) {
    resize(1400,900);
    setMinimumSize(1000,650);
    canvas=new Canvas(host.session,this);
    setCentralWidget(canvas);
    tree_=new QTreeWidget;
    tree_->setHeaderHidden(true);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setMinimumWidth(180);
    auto* structure=new QDockWidget("Objects",this);
    structure->setObjectName("structure"); structure->setWidget(tree_);
    addDockWidget(Qt::LeftDockWidgetArea,structure);
    auto* right=new QDockWidget("Properties",this);
    right->setObjectName("properties");
    auto* scroll=new QScrollArea;
    scroll->setWidgetResizable(true); scroll->setMinimumWidth(300);
    inspector_=new QWidget; scroll->setWidget(inspector_); right->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea,right);
    resizeDocks({structure,right},{215,320},Qt::Horizontal);
    auto* file=menuBar()->addMenu("&File");
    auto* edit=menuBar()->addMenu("&Edit");
    auto* add=menuBar()->addMenu("&Add");
    auto* view=menuBar()->addMenu("&View");
    auto action=[this](QMenu* menu,const QString& label,const QKeySequence& shortcut,auto fn) {
        auto* a=menu->addAction(label); a->setShortcut(shortcut);
        connect(a,&QAction::triggered,this,[this,fn]{perform(fn);}); return a;
    };
    action(file,"New",QKeySequence::New,[this]{canvas->cancel_interaction();host.create_document();canvas->fit_artboard();});
    action(file,"Open…",QKeySequence::Open,[this]{
        const auto path=QFileDialog::getOpenFileName(this,"Open Nect document",{},"Nect (*.nect *.json)");
        if(!path.isEmpty()) { canvas->cancel_interaction();host.open(path);canvas->fit_artboard(); }
    });
    action(file,"Save",QKeySequence::Save,[this]{save(false);});
    action(file,"Save As…",QKeySequence::SaveAs,[this]{save(true);});
    action(file,"Open Recovery…",{},[this]{
        const auto path=QFileDialog::getOpenFileName(this,"Recover a protected document",host.recovery_directory(),"Nect (*.nect)");
        if(!path.isEmpty()) {canvas->cancel_interaction();host.open(path);host.file_path.clear();host.edited();canvas->fit_artboard();}
    });
    action(file,"Export SVG…",QKeySequence("Ctrl+Shift+E"),[this]{
        const auto path=QFileDialog::getSaveFileName(this,"Export current artboard",{},"SVG (*.svg)");
        if(path.isEmpty()) return;
        if(QFileInfo(path).absoluteFilePath()==host.file_path) throw Error("EXPORT_TARGET","Export cannot replace the native source file");
        const auto& comp=host.session.document().compositions.front();
        const auto bytes=QByteArray::fromStdString(export_svg(host.session.document(),comp.id,comp.artboards.front().id));
        QSaveFile output(path);output.setDirectWriteFallback(false);
        if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size()||!output.commit())
            throw Error("IO_ERROR",output.errorString().toStdString());
        statusBar()->showMessage("SVG exported; native properties remain editable",5000);
    });
    undo_=action(edit,"Undo",QKeySequence::Undo,[this]{canvas->cancel_interaction();host.session.undo(host.session.revision());host.edited();});
    redo_=action(edit,"Redo",QKeySequence::Redo,[this]{canvas->cancel_interaction();host.session.redo(host.session.revision());host.edited();});
    action(edit,"Delete object",QKeySequence::Delete,[this]{
        if(canvas->selected_object.empty()) return;
        host.session.apply({DeleteObjects{{canvas->selected_object}}},host.session.revision());host.edited();
    });
    action(edit,"Group selected siblings",QKeySequence("Ctrl+G"),[this]{group_selection();});
    action(edit,"Close / open contour",{},[this]{
        if(canvas->selected_object.empty()) return;
        const auto& o=host.session.document().objects.at(canvas->selected_object);
        const auto contours=path_contours(o);
        if(contours.empty()) throw Error("NO_CONTOUR","Select a path");
        const auto& c=contours.front();
        host.session.apply({CloseContour{o.id,c.id,!c.closed}},host.session.revision());host.edited();
    });
    auto* convert=action(edit,"Convert to Path…",{},[this]{convert_to_path();});
    convert->setObjectName("convert-to-path");
    auto* circle=action(add,"Circle",{},[this]{add_primitive("nect.shape.circle");});
    circle->setObjectName("add-circle");
    auto* rectangle=action(add,"Rectangle",{},[this]{add_primitive("nect.shape.rectangle");});
    rectangle->setObjectName("add-rectangle");
    action(add,"Curve",QKeySequence("Ctrl+Shift+P"),[this]{add_curve();});
    auto* draw=action(add,"Draw Path",QKeySequence("P"),[this]{canvas->set_draw_mode(true);canvas->setFocus();statusBar()->showMessage("Click to add points · Enter finishes the path · Escape exits",10000);});
    action(view,"Fit Artboard",QKeySequence("Ctrl+0"),[this]{canvas->fit_artboard();});
    action(view,"Return to parent Group",{},[this]{canvas->leave_group();});
    view->addAction(structure->toggleViewAction());view->addAction(right->toggleViewAction());
    auto* toolbar=addToolBar("Authoring");toolbar->setMovable(false);
    toolbar->addAction(circle);toolbar->addAction(rectangle);
    auto* curve=toolbar->addAction("+ Curve"); connect(curve,&QAction::triggered,this,[this]{perform([this]{add_curve();});});
    toolbar->addAction(draw);toolbar->addSeparator();toolbar->addAction(undo_);toolbar->addAction(redo_);
    auto* fit=toolbar->addAction("Fit");connect(fit,&QAction::triggered,canvas,&Canvas::fit_artboard);
    breadcrumb_=new QLabel("Composition");toolbar->addWidget(breadcrumb_);
    status_=new QLabel;statusBar()->addPermanentWidget(status_);
    connect(tree_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem* item,QTreeWidgetItem*) {
        if(refreshing_||!item) return;
        canvas->set_selection(item->data(0,Qt::UserRole).toString().toStdString(),item->data(0,Qt::UserRole+1).toString().toStdString());
    });
    host.changed=[this]{refresh();};
    host.status_changed=[this]{status_->setText(host.save_status+"   ·   r"+QString::number(host.session.revision()));};
    canvas->document_changed=[this]{host.edited();};
    canvas->selection_changed=[this]{rebuild_inspector();};
    canvas->scope_changed=[this]{breadcrumb_->setText(canvas->breadcrumb());};
    canvas->error=[this](const QString& message){statusBar()->showMessage(message,10000);};
    qApp->installEventFilter(this);
    refresh();
}

Window::~Window() {
    qApp->removeEventFilter(this);
    cancel_whip();
    canvas->cancel_interaction();
}

bool Window::eventFilter(QObject* watched,QEvent* event) {
    if(event->type()==QEvent::MouseButtonPress&&!whip_target_) {
        const auto* mouse=static_cast<QMouseEvent*>(event);
        if(mouse->button()==Qt::LeftButton&&watched->property("nect-pick-whip").toBool()) {
            whip_target_=read_ref(watched->property("nect-reference").toByteArray());
            whip_session_=host.session_id;whip_start_=mouse->globalPosition().toPoint();whip_dragged_=false;
            grabMouse();
            statusBar()->showMessage("Drag to a property · hover an object to inspect its source · Shift: relative link · Esc: cancel");
            return true;
        }
    }
    if(!whip_target_)return QMainWindow::eventFilter(watched,event);
    if(event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape) {cancel_whip();return true;}
    if(event->type()==QEvent::ApplicationDeactivate) {cancel_whip();return false;}
    if(event->type()!=QEvent::MouseMove&&event->type()!=QEvent::MouseButtonRelease)
        return QMainWindow::eventFilter(watched,event);
    const auto* mouse=static_cast<QMouseEvent*>(event);
    const auto position=mouse->globalPosition().toPoint();
    if((position-whip_start_).manhattanLength()>6)whip_dragged_=true;
    if(event->type()==QEvent::MouseMove) {
        if(!whip_dragged_)return true;
        if(!whip_overlay_) {whip_overlay_=new WhipOverlay(this);whip_overlay_->setGeometry(rect());whip_overlay_->show();}
        auto* overlay=static_cast<WhipOverlay*>(whip_overlay_);
        overlay->start=mapFromGlobal(whip_start_);overlay->end=mapFromGlobal(position);overlay->raise();overlay->update();
        const auto local=tree_->viewport()->mapFromGlobal(position);
        if(tree_->viewport()->rect().contains(local))if(auto* item=tree_->itemAt(local)) {
            const auto object=item->data(0,Qt::UserRole).toString().toStdString();
            auto point=item->data(0,Qt::UserRole+1).toString().toStdString();
            if(point.empty()&&!whip_target_->point.empty()&&host.session.document().objects.contains(object)) {
                const auto& o=host.session.document().objects.at(object);
                const auto contours=path_contours(o);
                if(!contours.empty())point=contours.front().points.front().id;
            }
            canvas->set_selection(object,point);
        }
        return true;
    }
    if(mouse->button()!=Qt::LeftButton)return true;
    const auto target=*whip_target_;const auto frozen_session=whip_session_;const auto dragged=whip_dragged_;
    auto* under=childAt(mapFromGlobal(position));
    const auto source_bytes=under?under->property("nect-reference").toByteArray():QByteArray{};
    cancel_whip();
    if(!dragged) {pick_source(target);return true;}
    perform([&] {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The pick-whip belongs to another document");
        if(source_bytes.isEmpty())throw Error("NO_SOURCE","Drop the pick-whip on a numeric property field");
        const auto source=read_ref(source_bytes);
        const auto values=evaluate(host.session.document());
        const auto offset=mouse->modifiers().testFlag(Qt::ShiftModifier)?values.at(target)-values.at(source):0;
        host.session.apply({Link{target,{source,1,offset,"copy_local_value"}}},host.session.revision());host.edited();
    });
    return true;
}
void Window::cancel_whip() {
    if(!whip_target_)return;
    const auto target=*whip_target_;const auto same_session=whip_session_==host.session_id;
    whip_target_.reset();releaseMouse();
    if(whip_overlay_) {whip_overlay_->hide();whip_overlay_->deleteLater();whip_overlay_=nullptr;}
    statusBar()->clearMessage();
    if(same_session&&host.session.document().objects.contains(target.object))canvas->set_selection(target.object,target.point);
}

void Window::perform(const std::function<void()>& action) {
    try {action();} catch(const Error& e) {statusBar()->showMessage(qs(e.code)+": "+QString::fromUtf8(e.what()),12000);}
    catch(const std::exception& e) {statusBar()->showMessage(QString::fromUtf8(e.what()),12000);}
}
void Window::refresh() {
    refreshing_=true;
    const QSignalBlocker blocker(tree_);
    const auto& d=host.session.document();
    QString signature=host.session_id;
    std::function<void(const Id&)> fingerprint=[&](const Id& id) {
        const auto& o=d.objects.at(id);
        signature+="("+qs(id)+":"+QString::number(o.name.size())+":"+qs(o.name);
        if(o.source)signature+="|source:"+qs(o.source->type)+":"+qs(o.source->id);
        for(const auto& c:path_contours(o)) {signature+="["+qs(c.id);for(const auto& p:c.points)signature+=":"+qs(p.id);signature+="]";}
        for(const auto& child:o.children)fingerprint(child);
        signature+=")";
    };
    for(const auto& comp:d.compositions) {signature+="{"+qs(comp.id);for(const auto& id:comp.roots)fingerprint(id);signature+="}";}
    if(signature!=tree_signature_) {
    std::set<QString> expanded;
    QTreeWidgetItemIterator previous(tree_);
    while(*previous) {if((*previous)->isExpanded()) expanded.insert((*previous)->data(0,Qt::UserRole).toString());++previous;}
    tree_->clear();
    std::function<void(const Id&,QTreeWidgetItem*)> append=[&](const Id& id,QTreeWidgetItem* parent) {
        const auto& o=d.objects.at(id);
        auto* item=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree_);
        item->setText(0,qs(o.name));item->setData(0,Qt::UserRole,qs(id));
        item->setToolTip(0,(o.source?primitive_label(*o.source)+" source · ":QString{})+qs(id));
        for(const auto& child:o.children) append(child,item);
        for(const auto& c:path_contours(o)) for(std::size_t i=0;i<c.points.size();++i) {
            auto* point=new QTreeWidgetItem(item);
            point->setText(0,point_label(o,c.points[i],i));point->setData(0,Qt::UserRole,qs(id));point->setData(0,Qt::UserRole+1,qs(c.points[i].id));
            point->setToolTip(0,qs(c.points[i].id));
            if(canvas->selected_object==id&&canvas->selected_point==c.points[i].id) tree_->setCurrentItem(point);
        }
        if(canvas->selected_object==id&&canvas->selected_point.empty()) tree_->setCurrentItem(item);
        item->setExpanded(expanded.contains(qs(id)));
    };
    for(const auto& comp:d.compositions) for(const auto& id:comp.roots) append(id,nullptr);
    tree_signature_=signature;
    }
    canvas->refresh();
    undo_->setEnabled(host.session.can_undo());redo_->setEnabled(host.session.can_redo());
    status_->setText(host.save_status+"   ·   r"+QString::number(host.session.revision()));
    setWindowTitle((host.file_path.isEmpty()?"Untitled":QFileInfo(host.file_path).fileName())+" — Nect α");
    breadcrumb_->setText(canvas->breadcrumb());
    refreshing_=false;
    rebuild_inspector();
}

void Window::rebuild_inspector() {
    // Avoid deleting a focused field synchronously from its editingFinished signal.
    if(auto* old=inspector_->layout()) {
        while(auto* child=old->takeAt(0)) { if(child->widget()) {child->widget()->hide();child->widget()->deleteLater();}delete child; }
        delete old;
    }
    auto* layout=new QVBoxLayout(inspector_);
    const auto& d=host.session.document();
    if(!d.objects.contains(canvas->selected_object)) {layout->addWidget(new QLabel("Add a Circle, Rectangle or Curve.\nSelect a point to edit its handles."));layout->addStretch();return;}
    const auto& o=d.objects.at(canvas->selected_object);
    inspector_values_=evaluate(d);
    auto* name=new QLineEdit(qs(o.name));name->setAccessibleName("Object name");layout->addWidget(name);
    connect(name,&QLineEdit::editingFinished,this,[this,name,id=o.id]{
        if(!name->isModified()) return;
        name->setModified(false);
        perform([&]{host.session.apply({Rename{id,name->text().toStdString()}},host.session.revision());host.edited();});
    });
    auto section=[&](const QString& title){auto* box=new QGroupBox(title);auto* form=new QFormLayout(box);layout->addWidget(box);return form;};
    if(o.source) {
        auto* generator=section("1 · "+primitive_label(*o.source)+" source");
        for(const auto* parameter:{"center_x","center_y","radius","width","height"})
            if(o.source->parameters.contains(parameter))
                add_property(generator,{o.id,{},std::string("generator.")+parameter},parameter_label(parameter));
        auto* correction=section("2 · Point Edit");
        auto* enabled=new QCheckBox("Enabled");
        enabled->setObjectName("point-edit-enabled");
        enabled->setAccessibleName("Point Edit enabled");
        enabled->setChecked(o.point_edit && o.point_edit->enabled);
        enabled->setEnabled(o.point_edit.has_value());
        correction->addRow(enabled);
        std::size_t field_count=0;
        if(o.point_edit)for(const auto& [point,fields]:o.point_edit->overrides) { (void)point;field_count+=fields.size(); }
        auto* summary=new QLabel(o.point_edit
            ? QString("%1 absolute local overrides across %2 points.")
                .arg(static_cast<qulonglong>(field_count)).arg(static_cast<qulonglong>(o.point_edit->overrides.size()))
            : QString("No overrides yet. Edit a point or handle to add a correction."));
        summary->setObjectName("point-edit-summary");
        summary->setWordWrap(true);correction->addRow(summary);
        auto* semantics=new QLabel(o.point_edit && !o.point_edit->enabled
            ? "Bypassed: the source shape is visible. Editing a point enables its correction again."
            : "Edited fields hold absolute local values. Other fields continue to follow the source. Disable Point Edit to see the source shape.");
        semantics->setWordWrap(true);semantics->setStyleSheet("color: #a4acb8; font-size: 11px;");correction->addRow(semantics);
        const auto frozen_session=host.session_id;
        connect(enabled,&QCheckBox::toggled,this,[this,enabled,id=o.id,frozen_session](bool checked) {
            bool applied=false;
            perform([&] {
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Point Edit belongs to another document");
                host.session.apply({EnablePointEdit{id,checked}},host.session.revision());
                applied=true;host.edited();
            });
            if(!applied) {const QSignalBlocker blocker(enabled);enabled->setChecked(!checked);}
        });
        auto* convert=new QPushButton("Convert to Path…");
        convert->setObjectName("convert-to-path-button");
        convert->setToolTip("Review conversion effects and reference blockers before replacing the source.");
        correction->addRow(convert);
        connect(convert,&QPushButton::clicked,this,[this]{perform([this]{convert_to_path();});});
    }
    if(!canvas->selected_point.empty()) {
        auto* form=section("Point && handles");
        for(const auto* field:{"x","y","in.angle","in.length","out.angle","out.length"})
            add_property(form,{o.id,canvas->selected_point,field},QString::fromLatin1(field));
    }
    auto* transform=section("Transform · local matrix");
    for(const auto* field:{"tx","ty","a","b","c","d"})
        add_property(transform,{o.id,"",std::string("transform.")+field},QString::fromLatin1(field));
    if(o.kind==Kind::path) {
        auto* stroke=section("Stroke · sRGB");
        for(const auto* field:{"width","r","g","b","a"})
            add_property(stroke,{o.id,"",std::string("stroke.")+field},QString::fromLatin1(field));
    }
    auto* hint=new QLabel("Right-click a value to copy, paste or unlink.\n↗ picks a property source; += / -= adjusts once.");
    hint->setWordWrap(true);hint->setStyleSheet("color: #929aa6; font-size: 11px;");layout->addWidget(hint);layout->addStretch();
}
void Window::add_property(QFormLayout* layout,const Ref& ref,const QString& label) {
    const auto& d=host.session.document();
    const auto origin=property_origin(d,ref);
    const auto evaluated=inspector_values_.at(ref);
    // Generated fallback has no authored Scalar to inspect. Reuse this panel's
    // evaluation snapshot instead of asking property() to evaluate it again.
    const auto scalar=origin=="generated" ? Scalar{evaluated,{}} : nect::property(d,ref);
    auto* row=new QWidget;auto* box=new QHBoxLayout(row);box->setContentsMargins(0,0,0,0);box->setSpacing(4);
    auto* input=new QLineEdit(display_value(evaluated));input->setAccessibleName(label);
    const auto reference=QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact);
    input->setProperty("nect-reference",reference);
    input->setProperty("nect-property-origin",qs(origin));
    input->setToolTip(qs(property_unit(ref))+" · local · "+qs(ref.object+"/"+ref.point+"/"+ref.field));
    if(origin=="generated")input->setToolTip(input->toolTip()+"\nGenerated by the source. Editing creates a Point Edit override and keeps the source.");
    else if(origin=="point_edit") {
        input->setStyleSheet("color: #e4be82;");
        input->setToolTip(input->toolTip()+"\nPoint Edit: absolute local override; the source remains editable.");
    } else if(origin=="bypassed_point_edit") {
        input->setToolTip(input->toolTip()+"\nPoint Edit is bypassed. This value follows the source; editing enables the correction again.");
    }
    if(scalar.binding) {
        input->setStyleSheet("color: #84d5eb;");
        input->setToolTip(input->toolTip()+(origin=="bypassed_point_edit"
            ? "\nStored Point Edit link is bypassed. Unlink explicitly before replacing it."
            : "\nLinked; unlink explicitly before replacing its value."));
    }
    box->addWidget(input);
    auto* pick=new QPushButton("↗");pick->setFixedWidth(28);pick->setToolTip("Pick property source");box->addWidget(pick);
    pick->setProperty("nect-pick-whip",true);pick->setProperty("nect-reference",reference);
    pick->setToolTip("Drag to a source field; hover Objects to inspect another source. Click to search.");
    layout->addRow(label,row);
    connect(pick,&QPushButton::clicked,this,[this,ref]{pick_source(ref);});
    connect(input,&QLineEdit::editingFinished,this,[this,input,ref] {
        if(!input->isModified()) return;
        input->setModified(false);
        perform([&]{
            auto text=input->text().trimmed();bool valid=false;double value=0;
            if(text.startsWith("+=")||text.startsWith("-=")) {
                const auto delta=text.mid(2).toDouble(&valid);
                value=evaluate(host.session.document()).at(ref)+(text.startsWith("-=")?-delta:delta);
            } else value=text.toDouble(&valid);
            if(!valid||!std::isfinite(value)) throw Error("INVALID_VALUE","Enter a number or += / -= adjustment; expression authoring is not yet supported");
            host.session.apply({Set{ref,value}},host.session.revision());host.edited();
        });
    });
    input->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(input,&QWidget::customContextMenuRequested,this,[this,input,ref](const QPoint& point) {
        QMenu menu;
        auto* copy=menu.addAction("Copy Value");auto* reference=menu.addAction("Copy Reference");
        auto* paste=menu.addAction("Paste Value");auto* link=menu.addAction("Paste Link");
        auto* relative=menu.addAction("Pick Relative Link…");auto* unlink=menu.addAction("Unlink · keep evaluated value");
        auto* chosen=menu.exec(input->mapToGlobal(point));if(!chosen)return;
        perform([&] {
            if(chosen==copy) QApplication::clipboard()->setText(display_value(evaluate(host.session.document()).at(ref)));
            else if(chosen==reference) {
                auto* mime=new QMimeData;const auto data=QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact);
                mime->setData(reference_mime,data);mime->setText(QString::fromUtf8(data));QApplication::clipboard()->setMimeData(mime);
            } else if(chosen==paste) {
                bool valid=false;const auto value=QApplication::clipboard()->text().toDouble(&valid);
                if(!valid)throw Error("INVALID_VALUE","Clipboard is not a numeric value");
                host.session.apply({Set{ref,value}},host.session.revision());host.edited();
            } else if(chosen==link) {
                const auto* mime=QApplication::clipboard()->mimeData();
                const auto source=read_ref(mime->hasFormat(reference_mime)?mime->data(reference_mime):mime->text().toUtf8());
                host.session.apply({Link{ref,{source,1,0,"copy_local_value"}}},host.session.revision());host.edited();
            } else if(chosen==unlink) {host.session.apply({Unlink{ref}},host.session.revision());host.edited();}
            else if(chosen==relative) pick_source(ref,true);
        });
    });
}

void Window::pick_source(Ref target,bool relative) {
    auto* dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(720,480);
    dialog->setWindowTitle(relative?"Pick Relative Link source":"Pick property source");
    auto* layout=new QVBoxLayout(dialog);
    layout->addWidget(new QLabel("Target: "+property_label(host.session.document(),target)));
    auto* search=new QLineEdit;search->setPlaceholderText("Search object, point, property or unit…");layout->addWidget(search);
    auto* list=new QListWidget;layout->addWidget(list);
    const auto values=evaluate(host.session.document());
    for(const auto& ref:properties(host.session.document())) {
        if(ref==target)continue;
        const auto text=property_label(host.session.document(),ref)+" ["+qs(property_unit(ref))+", local]  = "+display_value(values.at(ref));
        auto* item=new QListWidgetItem(text,list);item->setData(Qt::UserRole,QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact));
        item->setToolTip(qs(ref.object+" / "+ref.point+" / "+ref.field));
        if(property_unit(target)!=property_unit(ref)) {item->setFlags(item->flags()&~Qt::ItemIsEnabled);item->setToolTip("Incompatible unit: "+qs(property_unit(ref)));}
    }
    connect(search,&QLineEdit::textChanged,dialog,[list](const QString& text){
        const auto terms=text.split(' ',Qt::SkipEmptyParts);
        for(int i=0;i<list->count();++i) {
            const bool matches=std::all_of(terms.begin(),terms.end(),[&](const auto& term){return list->item(i)->text().contains(term,Qt::CaseInsensitive);});
            list->item(i)->setHidden(!matches);
        }
    });
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    const auto frozen_session=host.session_id;
    connect(list,&QListWidget::currentItemChanged,dialog,[this,frozen_session](QListWidgetItem* item,QListWidgetItem*){
        if(!item||host.session_id!=frozen_session)return;
        const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
        canvas->set_selection(source.object,source.point);
    });
    connect(dialog,&QDialog::rejected,this,[this,target,frozen_session]{
        if(host.session_id==frozen_session&&host.session.document().objects.contains(target.object))canvas->set_selection(target.object,target.point);
    });
    auto accept=[this,dialog,list,target,relative,frozen_session] {
        perform([&]{
            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Source picker belongs to a different document");
            if(!list->currentItem())throw Error("NO_SOURCE","Choose a source property");
            const auto source=read_ref(list->currentItem()->data(Qt::UserRole).toByteArray());
            const auto values=evaluate(host.session.document());
            const auto offset=relative?values.at(target)-values.at(source):0;
            host.session.apply({Link{target,{source,1,offset,"copy_local_value"}}},host.session.revision());
            canvas->set_selection(target.object,target.point);host.edited();dialog->accept();
        });
    };
    connect(buttons,&QDialogButtonBox::accepted,dialog,accept);
    connect(list,&QListWidget::itemDoubleClicked,dialog,[accept](QListWidgetItem*){accept();});
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    dialog->show();search->setFocus();
}
void Window::save(bool choose) {
    auto path=host.file_path;
    if(choose||path.isEmpty())path=QFileDialog::getSaveFileName(this,"Save Nect document",path,"Nect (*.nect)");
    if(!path.isEmpty())host.save(path);
}
void Window::add_curve() {
    const auto& comp=host.session.document().compositions.front();
    const auto object=new_id();
    Point a,b;a.id=new_id();b.id=new_id();
    a.x.literal=240;a.y.literal=280;a.out_angle.literal=-40;a.out_length.literal=110;
    b.x.literal=620;b.y.literal=320;b.in_angle.literal=140;b.in_length.literal=110;
    host.session.apply({CreatePath{comp.id,"",object,"Curve "+std::to_string(host.session.document().objects.size()+1),{{new_id(),false,{a,b}}}}},host.session.revision());
    canvas->set_selection(object,a.id);host.edited();canvas->setFocus();
}
void Window::add_primitive(const std::string& type) {
    canvas->set_draw_mode(false);
    const auto& document=host.session.document();
    if(document.compositions.empty())throw Error("MISSING_COMPOSITION","Create a composition before adding a shape");
    const auto& composition=document.compositions.front();
    if(composition.artboards.empty())throw Error("MISSING_ARTBOARD","An artboard is needed to place the new shape");
    const auto& artboard=composition.artboards.front();
    Primitive source;
    source.id=new_id();source.type=type;
    source.parameters.emplace("center_x",Scalar{artboard.x+artboard.width/2,{}});
    source.parameters.emplace("center_y",Scalar{artboard.y+artboard.height/2,{}});
    if(type=="nect.shape.circle")source.parameters.emplace("radius",Scalar{100,{}});
    else if(type=="nect.shape.rectangle") {
        source.parameters.emplace("width",Scalar{220,{}});
        source.parameters.emplace("height",Scalar{140,{}});
    } else throw Error("UNSUPPORTED_GENERATOR","Only Circle and Rectangle sources are available");
    const auto id=new_id();
    const auto name=primitive_label(source).toStdString()+" "+std::to_string(document.objects.size()+1);
    host.session.apply({CreatePrimitive{composition.id,{},id,name,std::move(source)}},host.session.revision());
    canvas->set_selection(id);host.edited();canvas->setFocus();
}
void Window::convert_to_path() {
    canvas->cancel_interaction();
    const auto& document=host.session.document();
    const auto found=document.objects.find(canvas->selected_object);
    if(found==document.objects.end() || !found->second.source)
        throw Error("NO_GENERATOR","Select a Circle or Rectangle source to convert");
    const auto& object=found->second;
    const auto blockers=conversion_blockers(document,object.id);
    const auto frozen_session=host.session_id;
    const auto revision=host.session.revision();
    auto* dialog=new QDialog(this);
    dialog->setObjectName("convert-to-path-dialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Convert to Path");
    dialog->setWindowModality(Qt::WindowModal);
    dialog->resize(620,420);
    auto* layout=new QVBoxLayout(dialog);
    auto* heading=new QLabel("Convert "+qs(object.name)+" to an editable path?");
    heading->setTextFormat(Qt::PlainText);heading->setWordWrap(true);layout->addWidget(heading);
    auto* plan=new QLabel(
        "The current evaluated shape becomes path geometry. Center, radius or dimensions and their procedural links are frozen; the source parameters are removed.\n\n"
        "Stable point and contour IDs are preserved. Active Point Edit values are merged into the path, and active point bindings are kept. The Point Edit entry is removed.\n\n"
        "Undo restores the source and its corrections.");
    plan->setWordWrap(true);layout->addWidget(plan);
    if(object.point_edit && !object.point_edit->enabled) {
        auto* bypassed=new QLabel("Point Edit is bypassed: conversion uses the source shape. Stored bypassed corrections are discarded.");
        bypassed->setWordWrap(true);bypassed->setStyleSheet("color: #e4be82;");layout->addWidget(bypassed);
    }
    if(!blockers.empty()) {
        auto* blocked=new QLabel("Conversion is blocked by links to this source's generator parameters. Retarget or explicitly unlink these properties, then reopen this plan.");
        blocked->setWordWrap(true);blocked->setStyleSheet("color: #e4be82;");layout->addWidget(blocked);
        auto* list=new QListWidget;
        list->setObjectName("conversion-blockers");
        for(const auto& ref:blockers) {
            auto text=property_label(document,ref);
            const auto scalar=nect::property(document,ref);
            if(scalar.binding)text+=" ← "+property_label(document,scalar.binding->source);
            auto* item=new QListWidgetItem(text,list);
            item->setToolTip(qs(ref.object+" / "+ref.point+" / "+ref.field));
        }
        layout->addWidget(list);
    }
    auto* error=new QLabel;
    error->setObjectName("conversion-error");error->setWordWrap(true);error->setStyleSheet("color: #ecaa95;");
    layout->addWidget(error);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto* convert=buttons->addButton("Convert to Path",QDialogButtonBox::AcceptRole);
    convert->setObjectName("confirm-convert-to-path");convert->setEnabled(blockers.empty());convert->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,error,id=object.id,frozen_session,revision] {
        try {
            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The conversion plan belongs to another document");
            if(host.session.revision()!=revision)throw Error("REVISION_CONFLICT","The document changed. Reopen Convert to Path to review the current shape and links.");
            host.session.apply({ConvertToPath{id}},revision);
            host.edited();dialog->accept();
        } catch(const Error& exception) {
            error->setText(qs(exception.code)+": "+QString::fromUtf8(exception.what()));
        } catch(const std::exception& exception) {
            error->setText(QString::fromUtf8(exception.what()));
        }
    });
    dialog->show();
}
void Window::group_selection() {
    std::set<Id> chosen;
    for(const auto* item:tree_->selectedItems())chosen.insert(item->data(0,Qt::UserRole).toString().toStdString());
    const auto& comp=host.session.document().compositions.front();
    const auto parent=canvas->drill_scope();
    const auto& siblings=parent.empty()?comp.roots:host.session.document().objects.at(parent).children;
    std::vector<Id> ordered;for(const auto& id:siblings)if(chosen.contains(id))ordered.push_back(id);
    if(ordered.size()!=chosen.size())throw Error("INVALID_GROUP","Select sibling objects in the current group");
    const auto id=new_id();
    host.session.apply({GroupContiguous{comp.id,parent,ordered,id,"Group"}},host.session.revision());canvas->set_selection(id);host.edited();
}
void Window::closeEvent(QCloseEvent* event) {
    cancel_whip();
    canvas->cancel_interaction();
    try {host.recover();event->accept();}
    catch(const std::exception& e) {QMessageBox::warning(this,"Recovery failed",QString::fromUtf8(e.what())+"\nSave your document before closing.");event->ignore();}
}
}
