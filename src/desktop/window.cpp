#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
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
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
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
    if(parameter=="origin_x")return QStringLiteral("Origin X");
    if(parameter=="origin_y")return QStringLiteral("Origin Y");
    if(parameter=="font_size")return QStringLiteral("Font size");
    if(parameter=="frame_width")return QStringLiteral("Frame width");
    if(parameter=="frame_height")return QStringLiteral("Frame height");
    if(parameter=="tracking")return QStringLiteral("Tracking");
    if(parameter=="line_spacing")return QStringLiteral("Line advance · 0 = auto");
    if(parameter=="center_x")return QStringLiteral("Center X");
    if(parameter=="center_y")return QStringLiteral("Center Y");
    if(parameter=="radius")return QStringLiteral("Radius");
    if(parameter=="width")return QStringLiteral("Width");
    if(parameter=="height")return QStringLiteral("Height");
    if(parameter=="r")return QStringLiteral("Red");
    if(parameter=="g")return QStringLiteral("Green");
    if(parameter=="b")return QStringLiteral("Blue");
    if(parameter=="a")return QStringLiteral("Alpha");
    if(parameter=="copies")return QStringLiteral("Copies");
    if(parameter=="position_x")return QStringLiteral("Position X");
    if(parameter=="position_y")return QStringLiteral("Position Y");
    if(parameter=="anchor_x")return QStringLiteral("Anchor X");
    if(parameter=="anchor_y")return QStringLiteral("Anchor Y");
    if(parameter=="rotation")return QStringLiteral("Rotation");
    if(parameter=="scale_x")return QStringLiteral("Scale X");
    if(parameter=="scale_y")return QStringLiteral("Scale Y");
    if(parameter=="offset")return QStringLiteral("Offset");
    if(parameter=="start_opacity")return QStringLiteral("Start opacity");
    if(parameter=="end_opacity")return QStringLiteral("End opacity");
    if(parameter=="start_x")return QStringLiteral("Start X");
    if(parameter=="start_y")return QStringLiteral("Start Y");
    if(parameter=="end_x")return QStringLiteral("End X");
    if(parameter=="end_y")return QStringLiteral("End Y");
    return qs(parameter);
}
QString operation_label(const ShapeOperation& operation) {
    if(operation.type=="nect.paint.fill")return QStringLiteral("Fill");
    if(operation.type=="nect.paint.stroke")return QStringLiteral("Stroke");
    if(operation.type=="nect.shape.repeater")return QStringLiteral("Repeater");
    return qs(operation.type);
}
const ShapeOperation& find_operation(const Document& document,const Id& object,const Id& operation) {
    const auto& stack=document.objects.at(object).stack;
    const auto found=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==operation;});
    if(found==stack.end())throw Error("MISSING_OPERATION","The selected operation no longer exists");
    return *found;
}
const Composition& find_composition(const Document& document,const Id& id) {
    const auto found=std::find_if(document.compositions.begin(),document.compositions.end(),[&](const auto& entry){return entry.id==id;});
    if(found==document.compositions.end())throw Error("MISSING_COMPOSITION","Choose an artboard in a composition");
    return *found;
}
const Artboard& find_artboard(const Composition& composition,const Id& id) {
    const auto found=std::find_if(composition.artboards.begin(),composition.artboards.end(),[&](const auto& entry){return entry.id==id;});
    if(found==composition.artboards.end())throw Error("MISSING_ARTBOARD","Choose an artboard");
    return *found;
}
QString hex_color(const QColor& color) {
    return QString("#%1%2%3%4").arg(color.red(),2,16,QChar('0')).arg(color.green(),2,16,QChar('0'))
        .arg(color.blue(),2,16,QChar('0')).arg(color.alpha(),2,16,QChar('0')).toUpper();
}
QColor parse_hex_color(QString text) {
    text=text.trimmed();if(text.startsWith('#'))text.remove(0,1);
    if((text.size()!=6&&text.size()!=8)||!std::all_of(text.begin(),text.end(),[](QChar c){return QStringLiteral("0123456789abcdefABCDEF").contains(c);}))
        throw Error("INVALID_COLOR","Enter sRGB #RRGGBB or #RRGGBBAA");
    bool valid=false;const auto number=text.toUInt(&valid,16);
    if(!valid)throw Error("INVALID_COLOR","HEX color is outside its valid range");
    return text.size()==8?QColor((number>>24)&255,(number>>16)&255,(number>>8)&255,number&255)
        :QColor((number>>16)&255,(number>>8)&255,number&255);
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
    if(ref.field.starts_with("text.")) {
        path.append("Text");path.append(parameter_label(ref.field.substr(5)));
    } else if(ref.field.starts_with("generator.")) {
        path.append("Generator");path.append(parameter_label(ref.field.substr(10)));
    } else if(ref.field.starts_with("op.")) {
        const auto separator=ref.field.find('.',3);
        if(separator!=std::string::npos) {
            const auto operation=ref.field.substr(3,separator-3);
            const auto& stack=d.objects.at(ref.object).stack;
            const auto found=std::find_if(stack.begin(),stack.end(),[&](const auto& entry){return entry.id==operation;});
            if(found!=stack.end())path.append(QString::number(std::distance(stack.begin(),found)+1)+" · "+operation_label(*found));
            auto parameter=ref.field.substr(separator+1);
            if(found!=stack.end()&&found->gradient&&parameter.starts_with("gradient."+found->gradient->id+".")) {
                path.append("Gradient");parameter=parameter.substr(10+found->gradient->id.size());
                if(parameter.starts_with("stop.")) {
                    const auto dot=parameter.find('.',5);
                    const auto stop_id=parameter.substr(5,dot-5);
                    const auto& stops=found->gradient->stops;
                    const auto stop=std::find_if(stops.begin(),stops.end(),[&](const auto& entry){return entry.id==stop_id;});
                    if(stop!=stops.end())path.append("Stop "+QString::number(std::distance(stops.begin(),stop)+1));
                    parameter=parameter.substr(dot+1);
                }
            }
            path.append(parameter_label(parameter));
        } else path.append(qs(ref.field));
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
    auto* structure=new QDockWidget("Artboards & Objects",this);
    structure->setObjectName("structure");
    auto* navigator=new QWidget;auto* navigation=new QVBoxLayout(navigator);navigation->setContentsMargins(6,6,6,6);
    navigation->addWidget(new QLabel("Artboards · ordered frames"));
    artboards_=new QListWidget;artboards_->setObjectName("artboards");artboards_->setMaximumHeight(150);
    artboards_->setTextElideMode(Qt::ElideRight);artboards_->setWordWrap(false);
    artboards_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    artboards_->setMinimumHeight(82);navigation->addWidget(artboards_);
    auto* board_controls=new QWidget;auto* board_row=new QHBoxLayout(board_controls);
    board_row->setContentsMargins(0,0,0,0);board_row->setSpacing(3);
    auto board_button=[&](const char* name,const QString& text,const QString& tip,auto action) {
        auto* button=new QPushButton(text);button->setObjectName(name);button->setToolTip(tip);button->setAccessibleName(tip);
        button->setFixedWidth(32);board_row->addWidget(button);
        connect(button,&QPushButton::clicked,this,[this,action]{perform(action);});
    };
    board_button("artboard-add","+","Add artboard to the right",[this]{add_artboard(false);});
    board_button("artboard-duplicate","⧉","Duplicate frame to the right",[this]{add_artboard(true);});
    board_button("artboard-remove","×","Remove frame; artwork remains",[this]{
        canvas->cancel_interaction();host.session.apply({DeleteArtboard{canvas->active_composition(),canvas->active_artboard()}},host.session.revision());host.edited();});
    board_button("artboard-up","↑","Move earlier in artboard order; coordinates stay unchanged",[this]{move_artboard(-1);});
    board_button("artboard-down","↓","Move later in artboard order; coordinates stay unchanged",[this]{move_artboard(1);});
    board_row->addStretch();navigation->addWidget(board_controls);
    auto* frame_settings=new QPushButton("Edit active frame…");frame_settings->setObjectName("artboard-edit");
    connect(frame_settings,&QPushButton::clicked,this,[this]{canvas->set_selection({});artboard_editing_=true;rebuild_inspector();});navigation->addWidget(frame_settings);
    navigation->addWidget(new QLabel("Objects · active composition"));navigation->addWidget(tree_,1);structure->setWidget(navigator);
    connect(artboards_,&QListWidget::currentItemChanged,this,[this](QListWidgetItem* item,QListWidgetItem*) {
        if(refreshing_||!item)return;
        perform([&]{canvas->set_selection({});artboard_editing_=true;
            canvas->set_active_artboard(item->data(Qt::UserRole).toString().toStdString(),item->data(Qt::UserRole+1).toString().toStdString());
            rebuild_inspector();});
    });
    addDockWidget(Qt::LeftDockWidgetArea,structure);
    auto* right=new QDockWidget("Properties",this);
    right->setObjectName("properties");
    auto* scroll=new QScrollArea;inspector_scroll_=scroll;
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
        const auto bytes=QByteArray::fromStdString(export_svg(host.session.document(),canvas->active_composition(),canvas->active_artboard()));
        QSaveFile output(path);output.setDirectWriteFallback(false);
        if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size()||!output.commit())
            throw Error("IO_ERROR",output.errorString().toStdString());
        statusBar()->showMessage("SVG exported with text as outlines; native text remains editable",10000);
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
    auto* text=action(add,"Text",{},[this]{add_text();});text->setObjectName("add-text");
    add->addSeparator();
    auto* fill=action(add,"Fill",{},[this]{add_operation("nect.paint.fill");});fill->setObjectName("add-fill");
    auto* stroke=action(add,"Stroke",{},[this]{add_operation("nect.paint.stroke");});stroke->setObjectName("add-stroke");
    auto* repeater=action(add,"Repeater",{},[this]{add_operation("nect.shape.repeater");});repeater->setObjectName("add-repeater");
    auto* radial=action(add,"Radial Repeater · 12 × 30°",{},[this]{add_operation("nect.shape.repeater",true);});
    radial->setObjectName("add-radial-repeater");
    add->addSeparator();
    auto* add_curve_action=action(add,"Curve",QKeySequence("Ctrl+Shift+P"),[this]{add_curve();});add_curve_action->setObjectName("add-curve");
    auto* draw=action(add,"Draw Path",QKeySequence("P"),[this]{canvas->set_draw_mode(true);canvas->setFocus();statusBar()->showMessage("Click to add points · Enter finishes the path · Escape exits",10000);});
    action(view,"Fit Artboard",QKeySequence("Ctrl+0"),[this]{canvas->fit_artboard();});
    action(view,"Fit all artboards",QKeySequence("Ctrl+Shift+0"),[this]{canvas->fit_all_artboards();});
    action(view,"Return to parent Group",{},[this]{canvas->leave_group();});
    view->addAction(structure->toggleViewAction());view->addAction(right->toggleViewAction());
    auto* toolbar=addToolBar("Authoring");toolbar->setMovable(false);
    toolbar->addAction(circle);toolbar->addAction(rectangle);toolbar->addAction(text);
    auto* curve=toolbar->addAction("+ Curve"); connect(curve,&QAction::triggered,this,[this]{perform([this]{add_curve();});});
    toolbar->addAction(draw);toolbar->addSeparator();toolbar->addAction(undo_);toolbar->addAction(redo_);
    auto* fit=toolbar->addAction("Fit");connect(fit,&QAction::triggered,canvas,&Canvas::fit_artboard);
    breadcrumb_=new QLabel("Composition");toolbar->addWidget(breadcrumb_);
    status_=new QLabel;statusBar()->addPermanentWidget(status_);
    connect(tree_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem* item,QTreeWidgetItem*) {
        if(refreshing_||!item) return;
        artboard_editing_=false;
        canvas->set_selection(item->data(0,Qt::UserRole).toString().toStdString(),item->data(0,Qt::UserRole+1).toString().toStdString());
    });
    host.changed=[this]{refresh();};
    host.status_changed=[this]{status_->setText(host.save_status+"   ·   r"+QString::number(host.session.revision()));};
    canvas->document_changed=[this]{host.edited();};
    canvas->selection_changed=[this]{if(!canvas->selected_object.empty())artboard_editing_=false;rebuild_inspector();};
    canvas->active_artboard_changed=[this]{if(!refreshing_)refresh();};
    canvas->gradient_edit_changed=[this]{rebuild_inspector();};
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
    if(event->type()==QEvent::Wheel) {
        const auto* wheel=static_cast<QWheelEvent*>(event);
        auto* viewport=inspector_scroll_->viewport();
        if(viewport->rect().contains(viewport->mapFromGlobal(wheel->globalPosition().toPoint()))) {
            auto* bar=inspector_scroll_->verticalScrollBar();
            const auto delta=wheel->pixelDelta().y()!=0?wheel->pixelDelta().y():wheel->angleDelta().y();
            bar->setValue(bar->value()-delta);
            return true;
        }
    }
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
            const bool changed=canvas->selected_object!=object || canvas->selected_point!=point;
            canvas->set_selection(object,point);
            // A newly inspected object may have a much taller source/paint
            // panel. Wait for its normal layout before revealing a useful field.
            if(changed)QTimer::singleShot(0,this,[this]{reveal_whip_source();});
        }
        return true;
    }
    if(mouse->button()!=Qt::LeftButton)return true;
    const auto target=*whip_target_;const auto frozen_session=whip_session_;const auto dragged=whip_dragged_;
    QByteArray source_bytes;
    auto* viewport=inspector_scroll_->viewport();
    if(viewport->rect().contains(viewport->mapFromGlobal(position))) {
        // Inspect the actual scrolled viewport rather than a transparent whip
        // overlay or an off-viewport child whose isVisible() flag is still true.
        for(auto* under=inspector_->childAt(inspector_->mapFromGlobal(position));
            under && under!=inspector_;under=under->parentWidget()) {
            source_bytes=under->property("nect-reference").toByteArray();
            if(!source_bytes.isEmpty())break;
        }
    }
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
void Window::reveal_whip_source() {
    if(!whip_target_ || whip_session_!=host.session_id)return;
    if(auto* layout=inspector_->layout())layout->activate();
    QLineEdit* compatible=nullptr;
    for(auto* field:inspector_->findChildren<QLineEdit*>()) {
        const auto bytes=field->property("nect-reference").toByteArray();
        if(!field->isVisible() || bytes.isEmpty())continue;
        const auto ref=read_ref(bytes);
        if(ref.object!=canvas->selected_object || property_unit(ref)!=property_unit(*whip_target_))continue;
        if(!compatible)compatible=field;
        if(ref.field==whip_target_->field) {compatible=field;break;}
    }
    if(compatible)inspector_scroll_->ensureWidgetVisible(compatible,20,40);
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
    if(refreshing_)return;
    refreshing_=true;
    canvas->refresh();
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
    for(const auto& comp:d.compositions) if(comp.id==canvas->active_composition()) {
        signature+="{"+qs(comp.id);for(const auto& id:comp.roots)fingerprint(id);signature+="}";
    }
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
    for(const auto& comp:d.compositions) if(comp.id==canvas->active_composition())for(const auto& id:comp.roots) append(id,nullptr);
    tree_signature_=signature;
    }
    rebuild_artboards();
    undo_->setEnabled(host.session.can_undo());redo_->setEnabled(host.session.can_redo());
    status_->setText(host.save_status+"   ·   r"+QString::number(host.session.revision()));
    setWindowTitle((host.file_path.isEmpty()?"Untitled":QFileInfo(host.file_path).fileName())+" — Nect α");
    breadcrumb_->setText(canvas->breadcrumb());
    refreshing_=false;
    rebuild_inspector();
}

void Window::rebuild_artboards() {
    const QSignalBlocker blocker(artboards_);const auto scroll=artboards_->verticalScrollBar()->value();
    artboards_->clear();
    std::size_t selected_index=0,selected_count=0;
    const auto& compositions=host.session.document().compositions;
    const bool multiple_planes=std::count_if(compositions.begin(),compositions.end(),
        [](const auto& comp){return !comp.artboards.empty();})>1;
    for(const auto& comp:host.session.document().compositions)for(std::size_t index=0;index<comp.artboards.size();++index) {
        const auto& board=comp.artboards[index];const auto resolved=evaluate_artboard(comp,board.id);
        const auto prefix=multiple_planes?qs(comp.name)+" / ":QString{};
        auto* item=new QListWidgetItem(prefix+QString::number(index+1)+" · "+qs(board.name),artboards_);
        item->setData(Qt::UserRole,qs(comp.id));item->setData(Qt::UserRole+1,qs(board.id));
        item->setToolTip(qs(comp.name)+"\n"+qs(board.name)+" · "+display_value(resolved.width)+" × "+display_value(resolved.height)+
            "\nIndependent composition plane; changing order does not move frames or artwork.");
        if(comp.id==canvas->active_composition()&&board.id==canvas->active_artboard()) {
            artboards_->setCurrentItem(item);selected_index=index;selected_count=comp.artboards.size();
        }
    }
    artboards_->verticalScrollBar()->setValue(scroll);
    if(artboards_->currentItem())artboards_->scrollToItem(artboards_->currentItem());
    findChild<QPushButton*>("artboard-remove")->setEnabled(selected_count>1);
    findChild<QPushButton*>("artboard-up")->setEnabled(selected_count>0&&selected_index>0);
    findChild<QPushButton*>("artboard-down")->setEnabled(selected_count>0&&selected_index+1<selected_count);
}

void Window::add_artboard(bool duplicate) {
    canvas->cancel_interaction();
    const auto& comp=find_composition(host.session.document(),canvas->active_composition());
    const auto& selected=find_artboard(comp,canvas->active_artboard());
    auto board=duplicate?selected:evaluate_artboard(comp,selected.id);
    if(!duplicate)board.parent_size.reset();
    double right=board.x+board.width;
    for(const auto& entry:comp.artboards) {const auto resolved=evaluate_artboard(comp,entry.id);right=std::max(right,resolved.x+resolved.width);}
    board.id=new_id();board.name=duplicate?selected.name+" copy":"Artboard "+std::to_string(comp.artboards.size()+1);
    board.x=right+40;
    const auto index=static_cast<std::size_t>(std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const auto& entry){return entry.id==selected.id;})-comp.artboards.begin())+1;
    const auto comp_id=comp.id,board_id=board.id;
    host.session.apply({AddArtboard{comp_id,board,index}},host.session.revision());
    canvas->set_selection({});artboard_editing_=true;canvas->set_active_artboard(comp_id,board_id);host.edited();
}

void Window::move_artboard(int direction) {
    canvas->cancel_interaction();
    const auto& comp=find_composition(host.session.document(),canvas->active_composition());
    std::vector<Id> order;for(const auto& board:comp.artboards)order.push_back(board.id);
    const auto selected=std::find(order.begin(),order.end(),canvas->active_artboard());
    if(selected==order.end())throw Error("MISSING_ARTBOARD","Select a frame to reorder");
    const auto index=static_cast<std::ptrdiff_t>(selected-order.begin()),target=index+direction;
    if(target<0||target>=static_cast<std::ptrdiff_t>(order.size()))return;
    std::swap(order[index],order[target]);
    host.session.apply({ReorderArtboards{comp.id,order}},host.session.revision());host.edited();
}

void Window::edit_artboard(QVBoxLayout* layout) {
    if(canvas->active_composition().empty()||canvas->active_artboard().empty()) {
        layout->addWidget(new QLabel("No active artboard"));layout->addStretch();return;
    }
    const auto& comp=find_composition(host.session.document(),canvas->active_composition());
    const auto& board=find_artboard(comp,canvas->active_artboard());
    const auto resolved=evaluate_artboard(comp,board.id);
    const auto composition=comp.id,id=board.id;const auto frozen_session=host.session_id;
    auto apply=[this,frozen_session](const std::vector<Command>& commands) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","This frame belongs to another document");
        canvas->cancel_interaction();host.session.apply(commands,host.session.revision());host.edited();
    };
    auto read=[this,composition,id]{return find_artboard(find_composition(host.session.document(),composition),id);};
    auto* title=new QLabel("Artboard frame · "+qs(comp.name));title->setWordWrap(true);layout->addWidget(title);
    auto* note=new QLabel("Frame X/Y changes the crop only. Artwork stays at its existing composition coordinates. List order does not change placement.");
    note->setWordWrap(true);note->setStyleSheet("color: #a4acb8; font-size: 11px;");layout->addWidget(note);
    auto* group=new QGroupBox("Frame");auto* form=new QFormLayout(group);form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(group);
    auto* name=new QLineEdit(qs(board.name));name->setObjectName("artboard-name");form->addRow("Name",name);
    connect(name,&QLineEdit::editingFinished,this,[this,name,composition,read,apply]{
        if(!name->isModified())return;name->setModified(false);
        perform([&]{auto board=read();board.name=name->text().toStdString();apply({UpdateArtboard{composition,board}});});
    });
    auto number=[&](const char* key,const QString& label,double Artboard::* member) {
        auto* input=new QLineEdit(display_value(resolved.*member));input->setObjectName(QString("artboard-")+key);
        input->setAccessibleName(label);form->addRow(label,input);
        if(std::string(key)=="width"||std::string(key)=="height")
            input->setToolTip("Typing a size creates a local override. Use Inherit below to reset to the parent size.");
        else input->setToolTip("Crop position only; this does not move any artwork.");
        connect(input,&QLineEdit::editingFinished,this,[this,input,composition,read,apply,member,key=std::string(key)]{
            if(!input->isModified())return;input->setModified(false);
            perform([&]{bool valid=false;const auto value=input->text().trimmed().toDouble(&valid);
                if(!valid||!std::isfinite(value))throw Error("INVALID_VALUE","Enter a finite frame coordinate or size");
                auto board=read();board.*member=value;
                if(board.parent_size) {
                    if(key=="width")board.parent_size->width=false;
                    if(key=="height")board.parent_size->height=false;
                }
                apply({UpdateArtboard{composition,board}});
            });
        });
    };
    number("x","Frame X · crop",&Artboard::x);number("y","Frame Y · crop",&Artboard::y);
    number("width","Width",&Artboard::width);number("height","Height",&Artboard::height);
    auto* parent_group=new QGroupBox("Parent size");auto* parent_form=new QFormLayout(parent_group);
    parent_form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(parent_group);
    auto* parent=new QComboBox;parent->setObjectName("artboard-parent");
    parent->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);parent->setMinimumContentsLength(10);
    parent->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);parent->addItem("None · independent",QString{});
    for(const auto& candidate:comp.artboards)if(candidate.id!=id)parent->addItem(qs(candidate.name),qs(candidate.id));
    if(board.parent_size)parent->setCurrentIndex(parent->findData(qs(board.parent_size->artboard)));
    parent_form->addRow("Parent frame",parent);
    connect(parent,&QComboBox::currentIndexChanged,this,[this,parent,composition,id,read,apply,before=parent->currentIndex()](int){
        bool applied=false;perform([&]{const auto parent_id=parent->currentData().toString().toStdString();
            if(parent_id.empty())apply({DetachArtboardParent{composition,id}});
            else {auto board=read();board.parent_size=ArtboardParent{parent_id,true,true};apply({UpdateArtboard{composition,board}});}
            applied=true;
        });
        if(!applied){const QSignalBlocker blocker(parent);parent->setCurrentIndex(before);}
    });
    for(const bool width:{true,false}) {
        auto* inherit=new QCheckBox(width?"Inherit width · reset":"Inherit height · reset");
        inherit->setObjectName(width?"artboard-inherit-width":"artboard-inherit-height");
        inherit->setEnabled(board.parent_size.has_value());
        inherit->setChecked(board.parent_size&&(width?board.parent_size->width:board.parent_size->height));parent_form->addRow(inherit);
        inherit->setToolTip("Checked: use the parent's evaluated size. Unchecked: freeze this dimension as a local override.");
        connect(inherit,&QCheckBox::toggled,this,[this,inherit,width,composition,id,read,apply](bool checked){
            bool applied=false;perform([&]{auto board=read();
                if(!board.parent_size)throw Error("MISSING_PARENT","Choose a parent frame first");
                const auto resolved=evaluate_artboard(find_composition(host.session.document(),composition),id);
                if(width){board.parent_size->width=checked;if(!checked)board.width=resolved.width;}
                else {board.parent_size->height=checked;if(!checked)board.height=resolved.height;}
                apply({UpdateArtboard{composition,board}});applied=true;
            });
            if(!applied){const QSignalBlocker blocker(inherit);inherit->setChecked(!checked);}
        });
    }
    auto* detach=new QPushButton("Detach · keep current size");detach->setObjectName("artboard-detach");
    detach->setEnabled(board.parent_size.has_value());parent_form->addRow(detach);
    connect(detach,&QPushButton::clicked,this,[this,composition,id,apply]{perform([&]{apply({DetachArtboardParent{composition,id}});});});
    auto* parent_note=new QLabel("Only width and height inherit. Frame placement and artwork remain independent; template content is not inherited.");
    parent_note->setWordWrap(true);parent_form->addRow(parent_note);
    auto* fit=new QPushButton("Fit active frame");fit->setObjectName("artboard-fit");layout->addWidget(fit);
    connect(fit,&QPushButton::clicked,canvas,&Canvas::fit_artboard);layout->addStretch();
}

void Window::rebuild_inspector() {
    // Avoid deleting a focused field synchronously from its editingFinished signal.
    if(auto* old=inspector_->layout()) {
        while(auto* child=old->takeAt(0)) { if(child->widget()) {child->widget()->hide();child->widget()->deleteLater();}delete child; }
        delete old;
    }
    auto* layout=new QVBoxLayout(inspector_);
    if(artboard_editing_) {edit_artboard(layout);return;}
    const auto& d=host.session.document();
    if(!d.objects.contains(canvas->selected_object)) {layout->addWidget(new QLabel("Add a Circle, Rectangle, Curve or Text.\nSelect a point to edit its handles."));layout->addStretch();return;}
    const auto& o=d.objects.at(canvas->selected_object);
    inspector_values_=evaluate(d);
    auto* name=new QLineEdit(qs(o.name));name->setAccessibleName("Object name");layout->addWidget(name);
    connect(name,&QLineEdit::editingFinished,this,[this,name,id=o.id]{
        if(!name->isModified()) return;
        name->setModified(false);
        perform([&]{host.session.apply({Rename{id,name->text().toStdString()}},host.session.revision());host.edited();});
    });
    auto section=[&](const QString& title){auto* box=new QGroupBox(title);auto* form=new QFormLayout(box);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);return form;};
    if(o.text)add_text_properties(layout,o);
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
    if(o.kind!=Kind::group)add_stack(layout,o);
    auto* hint=new QLabel("Right-click a value to copy, paste or unlink.\n↗ picks a property source; += / -= adjusts once.");
    hint->setWordWrap(true);hint->setStyleSheet("color: #929aa6; font-size: 11px;");layout->addWidget(hint);layout->addStretch();
}
void Window::add_text_properties(QVBoxLayout* layout,const Object& object) {
    const auto id=object.id;const auto frozen_session=host.session_id;
    const auto& source=*object.text;
    auto* box=new QGroupBox("Text source");auto* form=new QFormLayout(box);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);
    auto* preview=new QLabel(qs(source.content).left(160));preview->setWordWrap(true);preview->setTextFormat(Qt::PlainText);
    preview->setObjectName("text-preview");form->addRow(preview);
    auto* edit=new QPushButton("Edit text…");edit->setObjectName("edit-text-content");form->addRow(edit);
    connect(edit,&QPushButton::clicked,this,[this,id]{perform([&]{edit_text_content(id);});});
    auto update=[this,id,frozen_session](const std::function<void(TextSource&)>& change) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
        const auto found=host.session.document().objects.find(id);
        if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","Text no longer exists");
        auto next=*found->second.text;change(next);
        host.session.apply({UpdateText{id,std::move(next)}},host.session.revision());host.edited();
    };
    auto* family=new QComboBox;family->setObjectName("text-family");family->setEditable(true);
    family->setInsertPolicy(QComboBox::NoInsert);family->setMinimumContentsLength(12);
    family->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    for(const auto& name:text_fonts())family->addItem(qs(name));family->setCurrentText(qs(source.family));form->addRow("Font family",family);
    connect(family->lineEdit(),&QLineEdit::editingFinished,this,[this,family,update,before=source.family]{
        const auto value=family->currentText().toStdString();if(value!=before)perform([&]{update([&](auto& s){s.family=value;});});});
    connect(family,QOverload<int>::of(&QComboBox::activated),this,[this,family,update]{perform([&]{update([&](auto& s){s.family=family->currentText().toStdString();});});});
    auto* weight=new QSpinBox;weight->setObjectName("text-weight");weight->setRange(1,999);weight->setSingleStep(100);
    weight->setValue(static_cast<int>(source.weight));weight->setKeyboardTracking(false);form->addRow("Weight",weight);
    connect(weight,&QSpinBox::editingFinished,this,[this,weight,update,before=source.weight]{
        if(static_cast<unsigned>(weight->value())!=before)perform([&]{update([&](auto& s){s.weight=static_cast<unsigned>(weight->value());});});});
    auto* italic=new QCheckBox("Italic");italic->setObjectName("text-italic");italic->setChecked(source.italic);form->addRow(italic);
    connect(italic,&QCheckBox::toggled,this,[this,update](bool value){perform([&]{update([&](auto& s){s.italic=value;});});});
    auto choices=[&](const QString& name,const QString& label,const QStringList& labels,const std::vector<std::string>& values,
                     const std::string& selected,std::string TextSource::*member) {
        auto* combo=new QComboBox;combo->setObjectName(name);combo->addItems(labels);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setMinimumContentsLength(9);
        combo->setCurrentIndex(static_cast<int>(std::distance(values.begin(),std::find(values.begin(),values.end(),selected))));form->addRow(label,combo);
        connect(combo,&QComboBox::currentIndexChanged,this,[this,update,values,member](int index){
            perform([&]{update([&](auto& s){s.*member=values.at(static_cast<std::size_t>(index));});});});
    };
    choices("text-layout","Sizing",{"Auto size","Fixed frame"},{"auto","frame"},source.layout,&TextSource::layout);
    choices("text-direction","Writing",{"Horizontal","Vertical"},{"horizontal","vertical"},source.direction,&TextSource::direction);
    choices("text-alignment","Alignment",{"Start","Center","End"},{"start","center","end"},source.alignment,&TextSource::alignment);
    auto* locale=new QLineEdit(qs(source.locale));locale->setObjectName("text-locale");form->addRow("Language tag",locale);
    connect(locale,&QLineEdit::editingFinished,this,[this,locale,update]{if(locale->isModified()){
        locale->setModified(false);perform([&]{update([&](auto& s){s.locale=locale->text().toStdString();});});}});
    for(const auto* parameter:{"origin_x","origin_y","font_size","frame_width","frame_height","tracking","line_spacing"})
        add_property(form,{id,"",std::string("text.")+parameter},parameter_label(parameter));
    std::map<std::string,double> parameters;for(const auto& [name,value]:source.parameters){(void)value;parameters[name]=inspector_values_.at({id,"","text."+name});}
    const auto result=evaluate_text(source,parameters);
    QStringList lines;lines<<QString("%1 × %2 du · %3 glyphs").arg(display_value(result.width),display_value(result.height)).arg(result.glyph_count);
    if(result.overflow)lines<<"Text extends outside its frame. Increase the frame or reduce the type size.";
    for(const auto& warning:result.warnings)lines<<qs(warning);
    QStringList fonts;for(const auto& name:result.used_fonts)fonts<<qs(name);
    lines<<"Rendered fonts: "+fonts.join(", ")<<"Native text stays editable. SVG exports glyph outlines; fonts are not embedded.";
    auto* status=new QLabel(lines.join('\n'));status->setObjectName("text-layout-status");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);form->addRow(status);
}
void Window::edit_text_content(const Id& id) {
    const auto session=host.session_id;const auto source=*host.session.document().objects.at(id).text;
    QDialog dialog(this);dialog.setObjectName("text-editor-dialog");dialog.setWindowTitle("Edit text");dialog.resize(560,340);
    auto* layout=new QVBoxLayout(&dialog);auto* editor=new QPlainTextEdit(qs(source.content));editor->setObjectName("text-content-editor");
    editor->setAccessibleName("Text content");layout->addWidget(editor);
    auto* message=new QLabel("Apply commits one undo step. Cancel discards only this draft.");message->setWordWrap(true);message->setTextFormat(Qt::PlainText);
    message->setObjectName("text-editor-status");layout->addWidget(message);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&,id,session,source]{
        try {
            if(host.session_id!=session)throw Error("SESSION_CONFLICT","The document changed. Copy this draft before closing.");
            const auto found=host.session.document().objects.find(id);
            if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","The text was removed. Copy this draft before closing.");
            auto next=*found->second.text;
            if(next.id!=source.id||next.content!=source.content)throw Error("TEXT_EDIT_CONFLICT","Text changed elsewhere. Copy this draft, cancel, and reopen the latest text.");
            const auto content=editor->toPlainText().toStdString();
            if(content!=next.content){next.content=content;host.session.apply({UpdateText{id,std::move(next)}},host.session.revision());host.edited();}
            dialog.accept();
        } catch(const std::exception& e){message->setText(QString::fromUtf8(e.what()));}
    });
    editor->setFocus();dialog.exec();
}
void Window::add_text() {
    canvas->set_draw_mode(false);const auto& comp=find_composition(host.session.document(),canvas->active_composition());
    const auto board=evaluate_artboard(comp,canvas->active_artboard());auto source=default_text(new_id());
    source.parameters.at("origin_x").literal=board.x+board.width*.15;
    source.parameters.at("origin_y").literal=board.y+board.height*.2;
    const auto id=new_id();host.session.apply({CreateText{comp.id,"",id,"Text "+std::to_string(host.session.document().objects.size()+1),source}},host.session.revision());
    canvas->set_selection(id);host.edited();canvas->setFocus();
}
void Window::add_stack(QVBoxLayout* layout,const Object& object) {
    auto* heading=new QWidget;
    auto* heading_layout=new QHBoxLayout(heading);heading_layout->setContentsMargins(0,4,0,0);
    heading_layout->addWidget(new QLabel("Shape stack"));heading_layout->addStretch();
    auto* add=new QPushButton("Add…");add->setObjectName("stack-add");
    auto* menu=new QMenu(add);
    for(const auto* name:{"add-fill","add-stroke","add-repeater","add-radial-repeater"})
        if(auto* action=findChild<QAction*>(QString::fromLatin1(name)))menu->addAction(action);
    add->setMenu(menu);heading_layout->addWidget(add);layout->addWidget(heading);
    auto* order_hint=new QLabel("Earlier paints default above later paints. Repeater affects the geometry and paints before it; copies share their source points.");
    order_hint->setWordWrap(true);order_hint->setStyleSheet("color: #a4acb8; font-size: 11px;");layout->addWidget(order_hint);
    const auto frozen_session=host.session_id;
    auto apply=[this,frozen_session](const std::vector<Command>& commands) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The shape stack belongs to another document");
        host.session.apply(commands,host.session.revision());host.edited();
    };
    for(std::size_t index=0;index<object.stack.size();++index) {
        const auto& operation=object.stack[index];
        const auto name=operation_label(operation);
        auto* group=new QGroupBox(QString::number(index+1)+" · "+name);
        group->setObjectName("stack-operation-"+qs(operation.id));
        group->setProperty("nect-operation",qs(operation.id));
        auto* form=new QFormLayout(group);form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(group);
        auto* controls=new QWidget;auto* row=new QHBoxLayout(controls);row->setContentsMargins(0,0,0,0);
        auto* enabled=new QCheckBox("Enabled");enabled->setChecked(operation.enabled);
        enabled->setObjectName("operation-enabled-"+qs(operation.id));
        enabled->setAccessibleName(name+" enabled");row->addWidget(enabled);row->addStretch();
        auto* up=new QPushButton("↑");up->setFixedWidth(28);up->setEnabled(index>0);
        up->setObjectName("operation-up-"+qs(operation.id));up->setToolTip("Move earlier in the stack");
        auto* down=new QPushButton("↓");down->setFixedWidth(28);down->setEnabled(index+1<object.stack.size());
        down->setObjectName("operation-down-"+qs(operation.id));down->setToolTip("Move later in the stack");
        auto* remove=new QPushButton("×");remove->setFixedWidth(28);
        remove->setObjectName("operation-remove-"+qs(operation.id));remove->setToolTip("Remove "+name);
        remove->setAccessibleName("Remove "+name);
        row->addWidget(up);row->addWidget(down);row->addWidget(remove);form->addRow(controls);
        connect(enabled,&QCheckBox::toggled,this,[this,enabled,apply,id=object.id,op=operation.id](bool checked) {
            bool applied=false;
            perform([&]{apply({EnableOperation{id,op,checked}});applied=true;});
            if(!applied){const QSignalBlocker blocker(enabled);enabled->setChecked(!checked);}
        });
        connect(up,&QPushButton::clicked,this,[this,id=object.id,op=operation.id,frozen_session]{perform([&]{
            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The shape stack belongs to another document");
            move_operation(id,op,-1);});});
        connect(down,&QPushButton::clicked,this,[this,id=object.id,op=operation.id,frozen_session]{perform([&]{
            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The shape stack belongs to another document");
            move_operation(id,op,1);});});
        connect(remove,&QPushButton::clicked,this,[this,apply,id=object.id,op=operation.id]{perform([&]{apply({RemoveOperation{id,op}});});});
        auto* composite=new QComboBox;
        composite->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        composite->setMinimumContentsLength(10);
        composite->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        composite->setObjectName("operation-composite-"+qs(operation.id));
        composite->addItem("Below previous paints / copies","below");composite->addItem("Above previous paints / copies","above");
        composite->setCurrentIndex(operation.composite=="above"?1:0);form->addRow("Composite",composite);
        connect(composite,&QComboBox::currentIndexChanged,this,[this,composite,apply,id=object.id,op=operation.id,before=composite->currentIndex()](int) {
            bool applied=false;
            perform([&]{const auto& current=find_operation(host.session.document(),id,op);
                apply({OperationOptions{id,op,composite->currentData().toString().toStdString(),current.fill_rule}});applied=true;});
            if(!applied){const QSignalBlocker blocker(composite);composite->setCurrentIndex(before);}
        });
        if(operation.type=="nect.paint.fill") {
            auto* rule=new QComboBox;rule->setObjectName("operation-fill-rule-"+qs(operation.id));
            rule->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            rule->setMinimumContentsLength(10);
            rule->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            rule->addItem("Nonzero winding","nonzero");rule->addItem("Even-odd","evenodd");
            rule->setCurrentIndex(operation.fill_rule=="evenodd"?1:0);form->addRow("Fill rule",rule);
            connect(rule,&QComboBox::currentIndexChanged,this,[this,rule,apply,id=object.id,op=operation.id,before=rule->currentIndex()](int) {
                bool applied=false;
                perform([&]{const auto& current=find_operation(host.session.document(),id,op);
                    apply({OperationOptions{id,op,current.composite,rule->currentData().toString().toStdString()}});applied=true;});
                if(!applied){const QSignalBlocker blocker(rule);rule->setCurrentIndex(before);}
            });
        }
        if(operation.type=="nect.paint.fill"||operation.type=="nect.paint.stroke") {
            add_gradient(form,object,operation);
            auto channel=[&](const char* parameter){return inspector_values_.at(operation_ref(object.id,operation.id,parameter));};
            const auto color=QColor::fromRgbF(channel("r"),channel("g"),channel("b"),channel("a"));
            auto* color_row=new QWidget;auto* color_layout=new QHBoxLayout(color_row);color_layout->setContentsMargins(0,0,0,0);
            auto* swatch=new QPushButton("Color…");swatch->setObjectName("operation-color-"+qs(operation.id));
            swatch->setStyleSheet("border: 3px solid "+color.name()+";");
            auto* hex=new QLineEdit(hex_color(color));hex->setObjectName("operation-hex-"+qs(operation.id));
            hex->setAccessibleName(name+" HEX RGBA");hex->setToolTip("sRGB #RRGGBB or #RRGGBBAA; linked channels require explicit unlinking before replacement.");
            color_layout->addWidget(swatch);color_layout->addWidget(hex);
            form->addRow(operation.gradient&&operation.gradient->enabled?"Solid fallback":"sRGB",color_row);
            auto apply_color=[this,apply,id=object.id,op=operation.id](const QColor& selected) {
                const auto values=evaluate(host.session.document());
                const std::array<double,4> rgba{selected.redF(),selected.greenF(),selected.blueF(),selected.alphaF()};
                const std::array<std::string,4> fields{"r","g","b","a"};
                std::vector<Command> commands;
                for(std::size_t i=0;i<fields.size();++i) {
                    const auto ref=operation_ref(id,op,fields[i]);
                    if(std::abs(values.at(ref)-rgba[i])>1e-8)commands.push_back(Set{ref,rgba[i]});
                }
                if(!commands.empty())apply(commands);
            };
            connect(swatch,&QPushButton::clicked,this,[this,color,name,apply_color] {
                const auto chosen=QColorDialog::getColor(color,this,name+" color",QColorDialog::ShowAlphaChannel);
                if(chosen.isValid())perform([&]{apply_color(chosen);});
            });
            connect(hex,&QLineEdit::editingFinished,this,[this,hex,apply_color] {
                if(!hex->isModified())return;
                hex->setModified(false);
                perform([&]{
                    apply_color(parse_hex_color(hex->text()));
                });
            });
            for(const auto* parameter:{"width","r","g","b","a"})
                if(operation.parameters.contains(parameter))add_property(form,operation_ref(object.id,operation.id,parameter),
                    std::string(parameter)=="a"?QStringLiteral("Paint opacity"):parameter_label(parameter));
        } else if(operation.type=="nect.shape.repeater") {
            for(const auto* parameter:{"copies","position_x","position_y","anchor_x","anchor_y","rotation","scale_x","scale_y","offset","start_opacity","end_opacity"})
                if(operation.parameters.contains(parameter))add_property(form,operation_ref(object.id,operation.id,parameter),parameter_label(parameter));
            auto* note=new QLabel("Rotation is a fixed step per copy; changing Copies does not divide 360°. Scale 1 is unchanged. Copies remain virtual and share source points.");
            note->setWordWrap(true);note->setStyleSheet("color: #a4acb8; font-size: 11px;");form->addRow(note);
        }
    }
}
void Window::add_gradient(QFormLayout* form,const Object& object,const ShapeOperation& operation) {
    const auto id=object.id,op=operation.id;
    const auto frozen_session=host.session_id;
    auto apply=[this,frozen_session](const std::vector<Command>& commands) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The gradient belongs to another document");
        host.session.apply(commands,host.session.revision());host.edited();
    };
    auto* mode=new QComboBox;mode->setObjectName("gradient-mode-"+qs(op));
    mode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);mode->setMinimumContentsLength(10);
    mode->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    mode->addItem("Solid");mode->addItem("Linear gradient");mode->addItem("Radial gradient");
    mode->setCurrentIndex(!operation.gradient||!operation.gradient->enabled?0:operation.gradient->type=="radial"?2:1);
    form->addRow("Paint",mode);
    connect(mode,&QComboBox::currentIndexChanged,this,[this,mode,id,op,apply,before=mode->currentIndex()](int index) {
        bool applied=false;
        perform([&]{
            auto gradient=find_operation(host.session.document(),id,op).gradient;
            if(index==0) {
                if(gradient)gradient->enabled=false;
            } else {
                if(!gradient) {
                    const auto values=evaluate(host.session.document());
                    Gradient created;created.id=new_id();
                    QRectF bounds;bool first=true;
                    for(const auto& contour:path_contours(host.session.document().objects.at(id)))for(const auto& point:contour.points) {
                        const QPointF position(values.at({id,point.id,"x"}),values.at({id,point.id,"y"}));
                        if(first){bounds=QRectF(position,position);first=false;}
                        else bounds=QRectF(QPointF(std::min(bounds.left(),position.x()),std::min(bounds.top(),position.y())),
                            QPointF(std::max(bounds.right(),position.x()),std::max(bounds.bottom(),position.y())));
                    }
                    if(const auto& selected=host.session.document().objects.at(id);selected.text) {
                        std::map<std::string,double> parameters;for(const auto& [name,value]:selected.text->parameters){(void)value;parameters[name]=values.at({id,"","text."+name});}
                        const auto layout=evaluate_text(*selected.text,parameters);bounds=QRectF(layout.x,layout.y,layout.width,layout.height);
                    }
                    const auto span=std::max(1.0,bounds.width());
                    created.start_x.literal=index==2?bounds.center().x():bounds.left();created.start_y.literal=bounds.center().y();
                    created.end_x.literal=created.start_x.literal+(index==2?span/2:span);created.end_y.literal=created.start_y.literal;
                    GradientStop start,end;start.id=new_id();end.id=new_id();end.offset.literal=1;
                    const std::array<std::string,3> channels{"r","g","b"};
                    for(std::size_t i=0;i<channels.size();++i) {
                        const auto value=values.at(operation_ref(id,op,channels[i]));
                        start.rgba[i].literal=value;end.rgba[i].literal=value+(1-value)*0.6;
                    }
                    // Paint opacity is multiplied once at rendering; stops start opaque.
                    start.rgba[3].literal=1;end.rgba[3].literal=1;
                    created.stops={start,end};gradient=std::move(created);
                }
                gradient->type=index==2?"radial":"linear";gradient->enabled=true;
            }
            apply({SetGradient{id,op,gradient}});applied=true;
        });
        if(!applied){const QSignalBlocker blocker(mode);mode->setCurrentIndex(before);}
    });
    if(!operation.gradient||!operation.gradient->enabled)return;
    const auto& gradient=*operation.gradient;
    const auto gradient_id=gradient.id;
    auto* handles=new QPushButton(canvas->gradient_operation()==op?"Finish gradient handles":"Edit gradient handles");
    handles->setObjectName("gradient-handles-"+qs(op));
    handles->setToolTip("Edit the source motif's local start/end frame. Repeated copies share this gradient. Escape cancels a drag or exits handles.");
    form->addRow(handles);
    connect(handles,&QPushButton::clicked,this,[this,id,op]{canvas->set_gradient_edit(id,op);});
    for(const auto* field:{"start_x","start_y","end_x","end_y"})
        add_property(form,gradient_ref(id,op,gradient_id,field),parameter_label(field));
    auto* note=new QLabel("Local coordinates; radial uses Start as its center and the distance to End as radius. Stops use sRGB. Paint opacity multiplies stop alpha.");
    note->setWordWrap(true);note->setStyleSheet("color: #a4acb8; font-size: 11px;");form->addRow(note);
    for(std::size_t index=0;index<gradient.stops.size();++index) {
        const auto& stop=gradient.stops[index];const auto stop_id=stop.id;
        auto* group=new QGroupBox("Stop "+QString::number(index+1));
        auto* stop_form=new QFormLayout(group);stop_form->setRowWrapPolicy(QFormLayout::WrapLongRows);form->addRow(group);
        auto ref=[&](const std::string& field){return gradient_ref(id,op,gradient_id,"stop."+stop_id+"."+field);};
        auto* row=new QWidget;auto* row_layout=new QHBoxLayout(row);row_layout->setContentsMargins(0,0,0,0);
        const auto color=QColor::fromRgbF(inspector_values_.at(ref("r")),inspector_values_.at(ref("g")),
            inspector_values_.at(ref("b")),inspector_values_.at(ref("a")));
        auto* hex=new QLineEdit(hex_color(color));hex->setObjectName("gradient-stop-hex-"+qs(stop_id));
        hex->setAccessibleName("Stop "+QString::number(index+1)+" HEX RGBA");
        hex->setToolTip("sRGB #RRGGBB or #RRGGBBAA. Linked channels must be explicitly unlinked before editing.");
        auto* remove=new QPushButton("×");remove->setFixedWidth(28);remove->setEnabled(gradient.stops.size()>2);
        remove->setObjectName("gradient-stop-remove-"+qs(stop_id));remove->setToolTip("Remove this stop; keep at least two");
        row_layout->addWidget(hex);row_layout->addWidget(remove);stop_form->addRow("sRGB",row);
        connect(hex,&QLineEdit::editingFinished,this,[this,hex,id,op,gradient_id,stop_id,apply]{
            if(!hex->isModified())return;hex->setModified(false);
            perform([&]{
                const auto color=parse_hex_color(hex->text());const auto values=evaluate(host.session.document());
                const std::array<double,4> rgba{color.redF(),color.greenF(),color.blueF(),color.alphaF()};
                const std::array<std::string,4> fields{"r","g","b","a"};std::vector<Command> commands;
                for(std::size_t i=0;i<fields.size();++i) {
                    const auto ref=gradient_ref(id,op,gradient_id,"stop."+stop_id+"."+fields[i]);
                    if(std::abs(values.at(ref)-rgba[i])>1e-8)commands.push_back(Set{ref,rgba[i]});
                }
                if(!commands.empty())apply(commands);
            });
        });
        connect(remove,&QPushButton::clicked,this,[this,id,op,stop_id,apply]{perform([&]{
            auto current=find_operation(host.session.document(),id,op).gradient;
            if(!current)throw Error("MISSING_GRADIENT","This gradient no longer exists");
            std::erase_if(current->stops,[&](const auto& entry){return entry.id==stop_id;});
            apply({SetGradient{id,op,current}});
        });});
        for(const auto* field:{"offset","r","g","b","a"})add_property(stop_form,ref(field),parameter_label(field));
    }
    auto* add=new QPushButton("Add color stop");add->setObjectName("gradient-stop-add-"+qs(op));
    add->setEnabled(gradient.stops.size()<64);form->addRow(add);
    connect(add,&QPushButton::clicked,this,[this,id,op,apply]{perform([&]{
        auto current=find_operation(host.session.document(),id,op).gradient;
        if(!current)throw Error("MISSING_GRADIENT","This gradient no longer exists");
        const auto values=evaluate(host.session.document());
        struct StopValue {double offset;std::array<double,4> rgba;};std::vector<StopValue> sorted;
        const std::array<std::string,4> channels{"r","g","b","a"};
        for(const auto& stop:current->stops) {
            auto ref=[&](const auto& field){return gradient_ref(id,op,current->id,"stop."+stop.id+"."+field);};
            StopValue value;value.offset=values.at(ref("offset"));
            for(std::size_t i=0;i<channels.size();++i)value.rgba[i]=values.at(ref(channels[i]));
            sorted.push_back(value);
        }
        std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.offset<b.offset;});
        auto left=sorted.front(),right=left;left.offset=0;
        double gap=right.offset-left.offset;
        for(std::size_t i=1;i<sorted.size();++i)if(sorted[i].offset-sorted[i-1].offset>gap) {
            left=sorted[i-1];right=sorted[i];gap=right.offset-left.offset;
        }
        if(1-sorted.back().offset>gap){left=sorted.back();right=left;right.offset=1;}
        GradientStop stop;stop.id=new_id();stop.offset.literal=(left.offset+right.offset)/2;
        for(std::size_t i=0;i<channels.size();++i)stop.rgba[i].literal=(left.rgba[i]+right.rgba[i])/2;
        current->stops.push_back(stop);apply({SetGradient{id,op,current}});
    });});
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
        const bool keep_focus=input->hasFocus();const auto scroll=inspector_scroll_->verticalScrollBar()->value();
        const auto frozen_session=host.session_id;
        perform([&]{
            auto text=input->text().trimmed();bool valid=false;double value=0;
            if(text.startsWith("+=")||text.startsWith("-=")) {
                const auto delta=text.mid(2).toDouble(&valid);
                value=evaluate(host.session.document()).at(ref)+(text.startsWith("-=")?-delta:delta);
            } else value=text.toDouble(&valid);
            if(!valid||!std::isfinite(value)) throw Error("INVALID_VALUE","Enter a number or += / -= adjustment; expression authoring is not yet supported");
            host.session.apply({Set{ref,value}},host.session.revision());host.edited();
            if(keep_focus)QTimer::singleShot(0,this,[this,ref,scroll,frozen_session]{
                if(host.session_id!=frozen_session||canvas->selected_object!=ref.object)return;
                const auto data=QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact);
                for(auto* current:inspector_->findChildren<QLineEdit*>())if(current->isVisible()&&current->property("nect-reference").toByteArray()==data) {
                    current->setFocus(Qt::OtherFocusReason);current->selectAll();
                    inspector_scroll_->verticalScrollBar()->setValue(scroll);break;
                }
            });
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
    const auto& comp=find_composition(host.session.document(),canvas->active_composition());
    const auto board=evaluate_artboard(comp,canvas->active_artboard());
    const auto object=new_id();
    Point a,b;a.id=new_id();b.id=new_id();
    a.x.literal=board.x+board.width*0.25;a.y.literal=board.y+board.height*0.4375;a.out_angle.literal=-40;a.out_length.literal=110;
    b.x.literal=board.x+board.width*0.6458333333333333;b.y.literal=board.y+board.height*0.5;b.in_angle.literal=140;b.in_length.literal=110;
    host.session.apply({CreatePath{comp.id,"",object,"Curve "+std::to_string(host.session.document().objects.size()+1),{{new_id(),false,{a,b}}}}},host.session.revision());
    canvas->set_selection(object,a.id);host.edited();canvas->setFocus();
}
void Window::add_primitive(const std::string& type) {
    canvas->set_draw_mode(false);
    const auto& document=host.session.document();
    if(document.compositions.empty())throw Error("MISSING_COMPOSITION","Create a composition before adding a shape");
    const auto& composition=find_composition(document,canvas->active_composition());
    if(composition.artboards.empty())throw Error("MISSING_ARTBOARD","An artboard is needed to place the new shape");
    const auto artboard=evaluate_artboard(composition,canvas->active_artboard());
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
void Window::add_operation(const std::string& type,bool radial) {
    canvas->cancel_interaction();
    const auto& document=host.session.document();
    const auto found=document.objects.find(canvas->selected_object);
    if(found==document.objects.end()||found->second.kind==Kind::group)
        throw Error("INVALID_DOMAIN","Select a Path, Circle, Rectangle or Text. Group stacks are not supported yet.");
    const auto& object=found->second;
    auto operation=default_operation(new_id(),type);
    if(radial) {
        const auto values=evaluate(document);
        double center_x=0,center_y=0;
        if(object.source) {
            center_x=values.at({object.id,{},"generator.center_x"});
            center_y=values.at({object.id,{},"generator.center_y"});
        } else if(object.text) {
            std::map<std::string,double> parameters;for(const auto& [name,value]:object.text->parameters){(void)value;parameters[name]=values.at({object.id,"","text."+name});}
            const auto layout=evaluate_text(*object.text,parameters);center_x=layout.x+layout.width/2;center_y=layout.y+layout.height/2;
        } else {
            QRectF bounds;bool first=true;
            for(const auto& contour:path_contours(object))for(const auto& point:contour.points) {
                const QPointF position(values.at({object.id,point.id,"x"}),values.at({object.id,point.id,"y"}));
                if(first){bounds=QRectF(position,position);first=false;}
                else {bounds.setLeft(std::min(bounds.left(),position.x()));bounds.setRight(std::max(bounds.right(),position.x()));
                    bounds.setTop(std::min(bounds.top(),position.y()));bounds.setBottom(std::max(bounds.bottom(),position.y()));}
            }
            center_x=bounds.center().x();center_y=bounds.center().y();
        }
        operation.parameters.at("copies").literal=12;
        operation.parameters.at("rotation").literal=30;
        operation.parameters.at("position_x").literal=0;
        operation.parameters.at("position_y").literal=0;
        operation.parameters.at("anchor_x").literal=center_x;
        operation.parameters.at("anchor_y").literal=center_y;
    }
    host.session.apply({AddOperation{object.id,std::move(operation),object.stack.size()}},host.session.revision());host.edited();
}
void Window::move_operation(const Id& object,const Id& operation,int direction) {
    const auto& stack=host.session.document().objects.at(object).stack;
    std::vector<Id> order;for(const auto& item:stack)order.push_back(item.id);
    const auto found=std::find(order.begin(),order.end(),operation);
    if(found==order.end())throw Error("MISSING_OPERATION","The selected operation no longer exists");
    const auto index=std::distance(order.begin(),found);
    const auto target=index+direction;
    if(target<0||target>=static_cast<std::ptrdiff_t>(order.size()))return;
    std::swap(order[static_cast<std::size_t>(index)],order[static_cast<std::size_t>(target)]);
    host.session.apply({ReorderOperations{object,std::move(order)}},host.session.revision());host.edited();
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
        "The source and active Point Edit result become path geometry. Center, radius or dimensions and their procedural links are frozen; the source parameters are removed.\n\n"
        "Stable point and contour IDs are preserved. Active Point Edit values are merged into the path, and active point bindings are kept. The Point Edit entry is removed.\n\n"
        "The later Fill, Stroke and Repeater stack remains editable.\n\n"
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
    const auto& comp=find_composition(host.session.document(),canvas->active_composition());
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
