#include "window.hpp"
#include "colors.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
#include <QScreen>
#include <QTimer>
#include <QWheelEvent>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QStringListModel>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <algorithm>
#include <set>
#include <tuple>
#include <numbers>
#include <QTreeWidgetItemIterator>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>

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
QByteArray refs_json(const std::vector<Ref>& refs) {
    QJsonArray array;for(const auto& ref:refs)array.append(ref_json(ref));return QJsonDocument(array).toJson(QJsonDocument::Compact);
}
std::vector<Ref> read_refs(const QByteArray& data) {
    std::vector<Ref> refs;for(const auto& value:QJsonDocument::fromJson(data).array())
        refs.push_back(read_ref(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)));
    return refs;
}
const char* reference_mime="application/x-nect-property-reference";
class PropertyInput final : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
    std::function<void(QString)> multiline;
    void keyPressEvent(QKeyEvent* event) override {
        if(event->matches(QKeySequence::Paste)) {
            const auto paste=QApplication::clipboard()->text();
            if((paste.contains('\n')||paste.contains('\r'))&&multiline) {
                auto draft=text();const auto start=selectionStart()<0?cursorPosition():selectionStart();
                draft.replace(start,selectedText().size(),paste);setModified(false);multiline(draft);event->accept();return;
            }
        }
        QLineEdit::keyPressEvent(event);
    }
};
class ExpressionInput final : public QPlainTextEdit {
public:
    std::function<void()> apply,cancel;
    void keyPressEvent(QKeyEvent* event) override {
        if((event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter)&&event->modifiers()==Qt::ControlModifier) {
            if(apply)apply();event->accept();return;
        }
        if(event->key()==Qt::Key_Escape) {if(cancel)cancel();event->accept();return;}
        QPlainTextEdit::keyPressEvent(event);
    }
};
QString expression_ref(const Ref& ref) {
    auto args=QJsonDocument(QJsonArray{qs(ref.object),qs(ref.point),qs(ref.field)}).toJson(QJsonDocument::Compact);
    return "ref("+QString::fromUtf8(args.mid(1,args.size()-2))+")";
}
class FontFamilyCombo final : public QComboBox {
    QStringListModel* families_;
public:
    explicit FontFamilyCombo(QStringListModel* families):families_(families) {
        setInsertPolicy(QComboBox::NoInsert);setMinimumContentsLength(12);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setEditable(true);
        auto* completion=new QCompleter(families_,this);
        completion->setCaseSensitivity(Qt::CaseInsensitive);completion->setCompletionMode(QCompleter::InlineCompletion);
        setCompleter(completion);
    }
    void showPopup() override {
        // Binding the complete list during every Inspector rebuild makes Qt
        // measure it repeatedly. Completion is ready immediately; the popup
        // needs the full list only when the user opens it (mouse or keyboard).
        if(model()!=families_) {
            const auto value=currentText();
            const QSignalBlocker combo_blocker(this),edit_blocker(lineEdit());
            setModel(families_);setCurrentIndex(findText(value));setEditText(value);
        }
        QComboBox::showPopup();
    }
};
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
    if(source.type=="nect.shape.circle")return QStringLiteral("Circle");
    if(source.type=="nect.shape.rectangle")return QStringLiteral("Rectangle");
    if(source.type=="nect.shape.polygon")return QStringLiteral("Polygon");
    if(source.type=="nect.shape.star")return QStringLiteral("Star");
    return qs(source.type);
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
    if(parameter=="points")return QStringLiteral("Points");
    if(parameter=="outer_radius")return QStringLiteral("Outer radius");
    if(parameter=="inner_radius")return QStringLiteral("Inner radius");
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
    if(parameter=="amount")return QStringLiteral("Amount");
    if(parameter=="miter_limit")return QStringLiteral("Miter limit");
    if(parameter=="end_x")return QStringLiteral("End X");
    if(parameter=="end_y")return QStringLiteral("End Y");
    return qs(parameter);
}
QString operation_label(const ShapeOperation& operation) {
    if(operation.type=="nect.paint.fill")return QStringLiteral("Fill");
    if(operation.type=="nect.paint.stroke")return QStringLiteral("Stroke");
    if(operation.type=="nect.shape.repeater")return QStringLiteral("Repeater");
    if(operation.type=="nect.shape.offset")return QStringLiteral("Offset Paths");
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
            const auto role=qs(point.id.substr(prefix.size()));const auto parts=role.split('-');
            if(parts.size()==3&&(parts[0]=="outer"||parts[0]=="inner")) {
                bool n_ok=false,d_ok=false;const auto n=parts[1].toUInt(&n_ok),d=parts[2].toUInt(&d_ok);
                if(n_ok&&d_ok&&d>0)return (parts[0]=="outer"?QString("Outer"):QString("Inner"))+" · "+display_value(360.0*n/d)+"°";
            }
            auto label=role;label.replace('-', ' ');
            if(!label.isEmpty())label[0]=label.at(0).toUpper();
            return label;
        }
    }
    return "Point "+QString::number(index+1);
}
QString property_label(const Document& d,const Ref& ref) {
    if(d.named_colors.contains(ref.object))return "Named color / "+qs(property_name(d,ref))+" / "+qs(ref.field);
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
        if(o.source)path.append(point_label(o,Point{ref.point},0));
        const auto contours=o.source?std::vector<Contour>{}:path_contours(o);
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

class RotationKnob final : public QWidget {
public:
    explicit RotationKnob(QWidget* parent=nullptr):QWidget(parent) {
        setObjectName("repeater-angle-knob");
        setAccessibleName("Repeater rotation angle knob");
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover,true);
        hover_effect_=new QGraphicsOpacityEffect(this);setGraphicsEffect(hover_effect_);hover_effect_->setOpacity(0.90);
        hover_animation_=new QPropertyAnimation(hover_effect_,"opacity",this);hover_animation_->setDuration(125);
        hover_animation_->setEasingCurve(QEasingCurve::OutCubic);
        setMinimumSize(44,44);
        setMaximumSize(44,44);
        setToolTip("Drag continuously to add signed degrees. The dial is modulo 360; the adjacent value remains exact.");
    }
    std::function<bool()> begin_drag;
    std::function<void(double)> preview_value;
    std::function<void()> commit_drag;
    std::function<void()> cancel_drag;
    void set_value(double value) {value_=value;update();update_accessibility();}
    double value() const {return value_;}
    QSize sizeHint() const override {return {44,44};}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing,true);
        const QRectF face=QRectF(rect()).adjusted(5,5,-5,-5);
        QColor ring=isEnabled()?QColor("#526071"):QColor("#38414c");
        painter.setPen(QPen(ring,2));painter.setBrush(QColor("#202833"));painter.drawEllipse(face);
        painter.setPen(QPen(isEnabled()?QColor("#48c6e9"):QColor("#69717a"),3,Qt::SolidLine,Qt::RoundCap));
        const auto normalized=std::fmod(value_,360.0)<0?std::fmod(value_,360.0)+360.0:std::fmod(value_,360.0);
        const auto radians=(normalized-90.0)*std::numbers::pi/180.0;
        const QPointF center=face.center();
        const QPointF tip=center+QPointF(std::cos(radians),std::sin(radians))*(face.width()*0.34);
        painter.drawLine(center,tip);painter.setPen(Qt::NoPen);painter.setBrush(isEnabled()?QColor("#48c6e9"):QColor("#69717a"));painter.drawEllipse(center,2.5,2.5);
        if(underMouse()) {painter.setBrush(Qt::NoBrush);painter.setPen(QPen(QColor("#63cce9"),1.5));painter.drawEllipse(QRectF(rect()).adjusted(2,2,-2,-2));}
        if(hasFocus()) {painter.setBrush(Qt::NoBrush);painter.setPen(QPen(QColor("#f3d17a"),1,Qt::DashLine));painter.drawEllipse(QRectF(rect()).adjusted(1,1,-1,-1));}
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton||!isEnabled()){QWidget::mousePressEvent(event);return;}
        if(begin_drag&&!begin_drag()){event->ignore();return;}
        dragging_=true;previous_angle_=angle_at(event->position());accumulated_=0;press_value_=value_;setFocus(Qt::MouseFocusReason);event->accept();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!dragging_){QWidget::mouseMoveEvent(event);return;}
        const auto current=angle_at(event->position());
        accumulated_+=std::remainder(current-previous_angle_,360.0);previous_angle_=current;
        value_=press_value_+accumulated_;update();update_accessibility();
        if(preview_value)preview_value(value_);
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton||!dragging_){QWidget::mouseReleaseEvent(event);return;}
        dragging_=false;if(commit_drag)commit_drag();event->accept();
    }
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Escape&&dragging_) {
            dragging_=false;value_=press_value_;update();update_accessibility();if(cancel_drag)cancel_drag();event->accept();return;
        }
        QWidget::keyPressEvent(event);
    }
    void focusInEvent(QFocusEvent* event) override {QWidget::focusInEvent(event);update();}
    void focusOutEvent(QFocusEvent* event) override {QWidget::focusOutEvent(event);update();}
    void enterEvent(QEnterEvent* event) override {QWidget::enterEvent(event);animate_hover(1.0);update();}
    void leaveEvent(QEvent* event) override {QWidget::leaveEvent(event);animate_hover(0.90);update();}
private:
    double value_=0,press_value_=0,previous_angle_=0,accumulated_=0;
    bool dragging_=false;
    QGraphicsOpacityEffect* hover_effect_=nullptr;
    QPropertyAnimation* hover_animation_=nullptr;
    void animate_hover(qreal opacity) {
        hover_animation_->stop();hover_animation_->setStartValue(hover_effect_->opacity());
        hover_animation_->setEndValue(opacity);hover_animation_->start();
    }
    double angle_at(QPointF point) const {
        const auto center=QPointF(width()/2.0,height()/2.0);
        return std::atan2(point.y()-center.y(),point.x()-center.x())*180.0/std::numbers::pi+90.0;
    }
    void update_accessibility() {
        const auto normalized=std::fmod(value_,360.0)<0?std::fmod(value_,360.0)+360.0:std::fmod(value_,360.0);
        setAccessibleDescription(QString("Authored rotation %1 degrees; dial indicator %2 degrees. Press Escape during a drag to cancel.")
            .arg(QString::number(value_,'g',17),QString::number(normalized,'g',15)));
    }
};

class HoverFeedback final : public QObject {
public:
    explicit HoverFeedback(QWidget* widget):QObject(widget) {
        effect_=new QGraphicsOpacityEffect(widget);widget->setGraphicsEffect(effect_);
        effect_->setOpacity(widget->isEnabled()?0.90:0.62);
        animation_=new QPropertyAnimation(effect_,"opacity",widget);animation_->setDuration(125);
        animation_->setEasingCurve(QEasingCurve::OutCubic);widget->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject* watched,QEvent* event) override {
        if(auto* widget=qobject_cast<QWidget*>(watched);widget&&
           (event->type()==QEvent::Enter||event->type()==QEvent::Leave||event->type()==QEvent::EnabledChange)) {
            const auto target=event->type()==QEvent::Enter?1.0:widget->isEnabled()?0.90:0.62;
            animation_->stop();animation_->setStartValue(effect_->opacity());animation_->setEndValue(target);animation_->start();
        }
        return QObject::eventFilter(watched,event);
    }
private:
    QGraphicsOpacityEffect* effect_=nullptr;
    QPropertyAnimation* animation_=nullptr;
};
}

Window::Window(QString recovery_directory):host(std::move(recovery_directory),this) {
    resize(1400,900);
    setMinimumSize(1000,650);
    canvas=new Canvas(host.session,this);
    canvas->set_session_identity_provider([this]{return host.session_id;});
    color_tools_=new ColorTools(*this);
    auto* central=new QWidget(this);auto* central_layout=new QVBoxLayout(central);
    central_layout->setContentsMargins(0,0,0,0);central_layout->setSpacing(0);
    utility_scroll_=new QScrollArea(central);utility_scroll_->setObjectName("canvas-utility-scroll");
    utility_scroll_->setFrameShape(QFrame::NoFrame);utility_scroll_->setWidgetResizable(true);
    utility_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);utility_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    utility_scroll_->setFixedHeight(42);
    connect(utility_scroll_->horizontalScrollBar(),&QScrollBar::rangeChanged,this,[this](int,int maximum){utility_scroll_->setFixedHeight(maximum>0?58:42);});
    auto* utility_contents=new QWidget;utility_contents->setObjectName("canvas-utility-strip");
    utility_layout_=new QHBoxLayout(utility_contents);utility_layout_->setContentsMargins(6,4,6,4);utility_layout_->setSpacing(4);
    utility_scroll_->setWidget(utility_contents);central_layout->addWidget(utility_scroll_);
    central_layout->addWidget(canvas,1);setCentralWidget(central);
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
    auto* scroll=new QScrollArea;inspector_scroll_=scroll;scroll->setObjectName("inspector-scroll");
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
        if(!path.isEmpty()) {canvas->cancel_interaction();host.open_recovery(path);canvas->fit_artboard();}
    });
    action(file,"Export SVG…",QKeySequence("Ctrl+Shift+E"),[this]{
        const auto path=QFileDialog::getSaveFileName(this,"Export current artboard",{},"SVG (*.svg)");
        if(path.isEmpty()) return;
        if(same_native_path(path,host.file_path)) throw Error("EXPORT_TARGET","Export cannot replace the native source file");
        const auto bytes=QByteArray::fromStdString(export_svg(host.session.document(),canvas->active_composition(),canvas->active_artboard()));
        QSaveFile output(path);output.setDirectWriteFallback(false);
        if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size()||!output.commit())
            throw Error("IO_ERROR",output.errorString().toStdString());
        statusBar()->showMessage("SVG exported with text as outlines; native text remains editable",10000);
    });
    action(file,"Import SVG artwork…",QKeySequence("Ctrl+I"),[this]{import_svg();})->setObjectName("import-svg");
    action(file,"Export PNG…",{},[this]{export_png();})->setObjectName("export-png");
    undo_=action(edit,"Undo",QKeySequence::Undo,[this]{canvas->cancel_interaction();host.session.undo(host.session.revision());host.edited();});
    redo_=action(edit,"Redo",QKeySequence::Redo,[this]{canvas->cancel_interaction();host.session.redo(host.session.revision());host.edited();});
    action(edit,"Delete selection",QKeySequence::Delete,[this]{
        if(canvas->selected_object.empty()) return;
        std::vector<Command> commands;
        if(canvas->selected_point.empty())commands.push_back(DeleteObjects{canvas->selected_objects()});
        else for(const auto& selection:canvas->selections())for(const auto& contour:path_contours(host.session.document().objects.at(selection.object),&canvas->evaluated_values()))
            if(std::any_of(contour.points.begin(),contour.points.end(),[&](const auto& p){return p.id==selection.point;}))
                commands.push_back(RemovePoint{selection.object,contour.id,selection.point});
        canvas->cancel_interaction();host.session.apply(commands,host.session.revision());host.edited();
    });
    action(edit,"Select all in editing context",{},[this]{canvas->select_all_in_context();})->setObjectName("select-all-context");
    action(edit,"Group selected siblings",QKeySequence("Ctrl+G"),[this]{group_selection();});
    action(edit,"Ungroup selected Groups",QKeySequence("Ctrl+Shift+G"),[this]{ungroup_selection();})->setObjectName("ungroup-objects");
    action(edit,"Duplicate objects in place",QKeySequence("Ctrl+D"),[this]{duplicate_selection();})->setObjectName("duplicate-objects");
    action(edit,"Rotate / scale selection…",QKeySequence("Ctrl+Shift+T"),[this]{transform_selection();})->setObjectName("transform-selection");
    auto* arrange=edit->addMenu("Arrange stacking order");
    for(const auto& [label,key,direction,edge,name]:std::vector<std::tuple<QString,QString,int,bool,QString>>{
        {"Bring forward","Ctrl+]",1,false,"stack-forward"},{"Send backward","Ctrl+[",-1,false,"stack-backward"},
        {"Bring to front","Ctrl+Shift+]",1,true,"stack-front"},{"Send to back","Ctrl+Shift+[",-1,true,"stack-back"}})
        action(arrange,label,QKeySequence(key),[this,direction,edge]{stack_selection(direction,edge);})->setObjectName(name);
    auto* align=edit->addMenu("Align objects (geometric bounds)");
    for(const bool to_artboard:{false,true}) {
        auto* target=align->addMenu(to_artboard?"To active Artboard":"To selection bounds");
        for(const auto& choice:std::vector<std::tuple<QString,std::string,std::string>>{
            {"Left edges","x","min"},{"Horizontal centers","x","center"},{"Right edges","x","max"},
            {"Top edges","y","min"},{"Vertical centers","y","center"},{"Bottom edges","y","max"}}) {
            const auto [label,axis,alignment]=choice;
            auto* command=action(target,label,{},[this,to_artboard,axis,alignment]{
                align_selection(axis,alignment,to_artboard?"artboard:"+canvas->active_artboard():"selection");
            });
            command->setObjectName(QString("align-%1-%2-%3").arg(to_artboard?"artboard":"selection",qs(axis),qs(alignment)));
            command->setToolTip("Align evaluated geometric bounds, excluding stroke width. Retained shapes remain editable.");
        }
    }
    for(const auto axis:{"x","y"})action(align,std::string(axis)=="x"?"Equal horizontal gaps":"Equal vertical gaps",{},[this,axis]{distribute_selection(axis);})->setObjectName(QString("distribute-%1").arg(axis));
    action(edit,"Mask With Top",{},[this]{mask_selection(true);});
    action(edit,"Mask With Bottom",{},[this]{mask_selection(false);});
    action(edit,"Put Inside top selected Group",{},[this]{put_selection_inside();});
    canvas->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(canvas,&QWidget::customContextMenuRequested,this,[this](const QPoint& point){selection_menu(canvas->mapToGlobal(point));});
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_,&QWidget::customContextMenuRequested,this,[this](const QPoint& point){selection_menu(tree_->viewport()->mapToGlobal(point));});
    auto* anchor=action(edit,"Edit Anchor",QKeySequence("Y"),[this]{canvas->set_anchor_edit(!canvas->anchor_edit());canvas->setFocus();});
    anchor->setObjectName("edit-anchor");anchor->setCheckable(true);
    canvas->anchor_edit_changed=[anchor](bool enabled){const QSignalBlocker blocker(anchor);anchor->setChecked(enabled);};
    auto* center_anchor=action(edit,"Center Anchor",{},[this]{if(canvas->selected_object.empty())return;
        canvas->cancel_interaction();host.session.apply({CenterAnchor{canvas->selected_object}},host.session.revision());host.edited();});
    center_anchor->setObjectName("center-anchor");
    auto* parent_action=action(edit,"Transform Parent…",{},[this]{choose_transform_parent();});parent_action->setObjectName("choose-transform-parent");
    action(edit,"Close / open contour",{},[this]{
        if(canvas->selected_object.empty()) return;
        const auto& o=host.session.document().objects.at(canvas->selected_object);
        const auto contours=path_contours(o,&canvas->evaluated_values());
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
    auto* polygon=action(add,"Polygon",{},[this]{add_primitive("nect.shape.polygon");});polygon->setObjectName("add-polygon");
    auto* star=action(add,"Star",{},[this]{add_primitive("nect.shape.star");});star->setObjectName("add-star");
    auto* text=action(add,"Text",{},[this]{add_text();});text->setObjectName("add-text");
    auto* linked=action(add,"Linked Image…",{},[this]{import_image(true);});linked->setObjectName("import-linked-image");
    auto* embedded=action(add,"Embedded Image…",{},[this]{import_image(false);});embedded->setObjectName("import-embedded-image");
    file->addAction(linked);file->addAction(embedded);
    action(file,"Image Assets…",{},[this]{show_assets();})->setObjectName("image-assets");
    add->addSeparator();
    auto* fill=action(add,"Fill",{},[this]{add_operation("nect.paint.fill");});fill->setObjectName("add-fill");
    auto* stroke=action(add,"Stroke",{},[this]{add_operation("nect.paint.stroke");});stroke->setObjectName("add-stroke");
    auto* offset=action(add,"Offset Paths",{},[this]{add_operation("nect.shape.offset");});offset->setObjectName("add-offset");
    auto* repeater=action(add,"Repeater",{},[this]{add_operation("nect.shape.repeater");});repeater->setObjectName("add-repeater");
    auto* radial=action(add,"Radial Repeater · 12 × 30°",{},[this]{add_operation("nect.shape.repeater",true);});
    radial->setObjectName("add-radial-repeater");
    add->addSeparator();
    auto* add_curve_action=action(add,"Curve",QKeySequence("Ctrl+Shift+P"),[this]{add_curve();});add_curve_action->setObjectName("add-curve");
    auto* draw=action(add,"Draw Path",QKeySequence("P"),[this]{canvas->set_draw_mode(true);canvas->setFocus();statusBar()->showMessage("Click to add points · Enter finishes the path · Escape exits",10000);});
    draw->setObjectName("draw-path");draw->setShortcuts({QKeySequence("P"),QKeySequence("G")});
    draw->setShortcutContext(Qt::WidgetShortcut);canvas->addAction(draw);
    action(view,"Fit Artboard",QKeySequence("Ctrl+0"),[this]{canvas->fit_artboard();});
    action(view,"Fit selection",QKeySequence("Ctrl+2"),[this]{canvas->fit_selection();})->setObjectName("fit-selection");
    action(view,"Fit all artboards",QKeySequence("Ctrl+Shift+0"),[this]{canvas->fit_all_artboards();});
    auto* snap = view->addAction("Snap ON"); snap->setObjectName("canvas-snap");
    snap->setCheckable(true); snap->setChecked(canvas->snap_enabled());
    snap->setToolTip("Snap object bounds and point anchors to Guides, Grids, Artboards and same-scope geometry within 6 logical px. Hidden Guide/Grid overlays remain Snap targets. Numeric edits stay exact.");
    connect(snap, &QAction::toggled, canvas, &Canvas::set_snap_enabled);
    connect(snap, &QAction::toggled, this, [snap](bool enabled) { snap->setText(enabled ? "Snap ON" : "Snap OFF"); });
    auto* snap_guides=view->addAction("Snap to Guides");snap_guides->setObjectName("snap-guides");
    snap_guides->setCheckable(true);snap_guides->setChecked(canvas->snap_guides_enabled());
    snap_guides->setToolTip("Use authored Composition Guides as Snap targets. This setting is independent of Show Guides.");
    connect(snap_guides,&QAction::toggled,canvas,&Canvas::set_snap_guides_enabled);
    auto* snap_grid=view->addAction("Snap to Grid");snap_grid->setObjectName("snap-grid");
    snap_grid->setCheckable(true);snap_grid->setChecked(canvas->snap_grid_enabled());
    snap_grid->setToolTip("Use authored Artboard Grid bounds, cell edges and centers as Snap targets. This setting is independent of Show Grid.");
    connect(snap_grid,&QAction::toggled,canvas,&Canvas::set_snap_grid_enabled);
    utility_guides_=new QToolButton(utility_contents);utility_guides_->setObjectName("utility-show-guides");
    utility_guides_->setCheckable(true);utility_guides_->setChecked(canvas->show_guides());utility_layout_->addWidget(utility_guides_);
    utility_grid_=new QToolButton(utility_contents);utility_grid_->setObjectName("utility-show-grid");
    utility_grid_->setCheckable(true);utility_grid_->setChecked(canvas->show_grid());utility_layout_->addWidget(utility_grid_);
    utility_snap_action_=snap;utility_snap_=new QToolButton(utility_contents);utility_snap_->setObjectName("utility-snap");
    utility_snap_->setDefaultAction(snap);utility_layout_->addWidget(utility_snap_);
    auto style_utility=[](QToolButton* button) {
        button->setMinimumHeight(30);button->setToolButtonStyle(Qt::ToolButtonTextOnly);button->setAutoRaise(false);
        button->setMouseTracking(true);new HoverFeedback(button);
        button->setStyleSheet("QToolButton{color:#dbe4ee;background:#252d38;border:1px solid #566373;border-radius:4px;padding:4px 8px;}"
            "QToolButton:hover{background:#354556;border-color:#63cce9;}"
            "QToolButton:checked{color:#e9fbff;background:#244b5b;border-color:#48c6e9;}"
            "QToolButton:focus{border:2px solid #f3d17a;}"
            "QToolButton:disabled{color:#77818d;background:#20252c;border-color:#38414c;}");
    };
    auto sync_button=[](QToolButton* button,const QString& label,const QString& state,const QString& help,bool enabled) {
        const QSignalBlocker blocker(button);button->setText(label+" "+state);button->setAccessibleName(label+" · "+state);
        button->setAccessibleDescription(help+" Current state: "+state+".");button->setToolTip(help+" Current state: "+state+".");button->setEnabled(enabled);
    };
    for(auto* button:{utility_guides_,utility_grid_,utility_snap_})style_utility(button);
    connect(utility_guides_,&QToolButton::toggled,canvas,&Canvas::set_show_guides);
    connect(utility_grid_,&QToolButton::toggled,canvas,&Canvas::set_show_grid);
    auto* fit_button=new QToolButton(utility_contents);fit_button->setObjectName("utility-fit");fit_button->setText("Fit");
    fit_button->setAccessibleName("Fit active Artboard in Canvas");fit_button->setToolTip("Fit the active Artboard in the Canvas.");style_utility(fit_button);
    connect(fit_button,&QToolButton::clicked,canvas,&Canvas::fit_artboard);utility_layout_->addWidget(fit_button);
    utility_zoom_=new QDoubleSpinBox(utility_contents);utility_zoom_->setObjectName("canvas-zoom-percent");utility_zoom_->setAccessibleName("Canvas zoom percentage");
    utility_zoom_->setRange(2,6400);utility_zoom_->setDecimals(0);utility_zoom_->setSingleStep(10);utility_zoom_->setSuffix("%");
    utility_zoom_->setKeyboardTracking(false);utility_zoom_->setFixedWidth(92);utility_zoom_->setToolTip("Canvas view zoom · 2% to 6400%. This does not change the document.");
    new HoverFeedback(utility_zoom_);
    utility_layout_->addWidget(utility_zoom_);connect(utility_zoom_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double percent){canvas->set_zoom(percent/100.0);});
    utility_artboard_=new QLabel(utility_contents);utility_artboard_->setObjectName("canvas-output-readback");
    utility_artboard_->setAccessibleName("Active Artboard and output dimensions");utility_artboard_->setMinimumWidth(185);utility_artboard_->setMaximumWidth(270);
    utility_artboard_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);utility_layout_->addWidget(utility_artboard_);
    auto* setup_button=new QToolButton(utility_contents);setup_button->setObjectName("utility-setup");setup_button->setText("Setup…");
    setup_button->setAccessibleName("Open active Artboard Margin, Grid and Guide setup");
    setup_button->setToolTip("Open the active Artboard Margin, Grid bounds/counts/gutters and Composition Guide editors.");style_utility(setup_button);
    connect(setup_button,&QToolButton::clicked,this,[this,setup_button]{show_layout_setup(setup_button);});
    utility_layout_->addWidget(setup_button);
    auto show_shortcut_help=[this] {
        auto* dialog=new QDialog(this);dialog->setObjectName("shortcut-help-dialog");dialog->setWindowTitle("Keyboard shortcuts and collisions");
        dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setModal(false);auto* layout=new QVBoxLayout(dialog);
        layout->addWidget(new QLabel("Draw Path · Nect: P and G (same action).\nAfter Effects: P = Position; G = Pen / Mask Feather.\n\nRotate / scale selection · Nect: Ctrl+Shift+T.\nAfter Effects: Ctrl+Shift+T = Effect Controls.\n\nWhen a text field has focus, typed letters edit that field; P/G act on the Canvas.",dialog));
        auto* close=new QPushButton("Close",dialog);close->setObjectName("shortcut-help-close");connect(close,&QPushButton::clicked,dialog,&QDialog::close);layout->addWidget(close);
        dialog->resize(430,225);dialog->show();
    };
    auto* keys_button=new QToolButton(utility_contents);keys_button->setObjectName("utility-shortcut-help");keys_button->setText("Keys");
    keys_button->setAccessibleName("Keyboard shortcut and collision help");keys_button->setToolTip("Show Nect shortcuts and named After Effects collisions.");style_utility(keys_button);
    connect(keys_button,&QToolButton::clicked,this,show_shortcut_help);utility_layout_->addWidget(keys_button);
    auto* shortcuts_action=view->addAction("Keyboard shortcuts and collisions…");shortcuts_action->setObjectName("shortcut-help");
    connect(shortcuts_action,&QAction::triggered,this,show_shortcut_help);
    utility_layout_->addStretch();utility_contents->setMinimumWidth(utility_layout_->sizeHint().width());
    auto* guides_button=utility_guides_;auto* grid_button=utility_grid_;auto* snap_button=utility_snap_;
    const auto update_toggle_text=[guides_button,grid_button,snap,snap_button] {
        const auto guides=guides_button->isChecked(),grid=grid_button->isChecked(),snapping=snap->isChecked();
        auto state=[](bool on){return on?QStringLiteral("ON"):QStringLiteral("OFF");};
        guides_button->setText("Guides "+state(guides));guides_button->setAccessibleName("Show Guides · "+state(guides));
        guides_button->setAccessibleDescription("Guide overlay visibility, independent of Guide Snap. Current state: "+state(guides)+".");
        guides_button->setToolTip("Show or hide Guide overlays; Guide Snap remains independent. Current state: "+state(guides)+".");
        grid_button->setText("Grid "+state(grid));grid_button->setAccessibleName("Show Grid · "+state(grid));
        grid_button->setAccessibleDescription("Grid overlay visibility, independent of Grid Snap. Current state: "+state(grid)+".");
        grid_button->setToolTip("Show or hide Grid overlays; Grid Snap remains independent. Current state: "+state(grid)+".");
        snap_button->setAccessibleName("Snap · "+state(snapping));
    };
    connect(utility_guides_,&QToolButton::toggled,this,[update_toggle_text](bool){update_toggle_text();});
    connect(utility_grid_,&QToolButton::toggled,this,[update_toggle_text](bool){update_toggle_text();});
    connect(snap,&QAction::toggled,this,[update_toggle_text](bool){update_toggle_text();});
    sync_button(utility_guides_,"Show Guides",canvas->show_guides()?"ON":"OFF","Toggle Guide overlay visibility; Guide Snap is independent",true);
    sync_button(utility_grid_,"Show Grid",canvas->show_grid()?"ON":"OFF","Toggle Grid overlay visibility; Grid Snap is independent",true);
    update_toggle_text();
    action(view,"Return to parent Group",{},[this]{canvas->leave_group();});
    auto* colors=action(view,"Colors…",{},[this]{color_tools_->show_manager();});colors->setObjectName("show-colors");
    auto* history=action(view,"History…",QKeySequence("Ctrl+Shift+H"),[this]{show_history();});history->setObjectName("show-history");
    view->addAction(structure->toggleViewAction());view->addAction(right->toggleViewAction());
    auto* toolbar=addToolBar("Authoring");toolbar->setMovable(false);
    toolbar->addAction(circle);toolbar->addAction(rectangle);toolbar->addAction(text);
    auto* curve=toolbar->addAction("+ Curve"); connect(curve,&QAction::triggered,this,[this]{perform([this]{add_curve();});});
    toolbar->addAction(draw);toolbar->addSeparator();toolbar->addAction(undo_);toolbar->addAction(redo_);
    auto* fit=toolbar->addAction("Fit");connect(fit,&QAction::triggered,canvas,&Canvas::fit_artboard);
    toolbar->addAction(colors);
    breadcrumb_=new QLabel("Composition");toolbar->addWidget(breadcrumb_);
    status_=new QLabel;statusBar()->addPermanentWidget(status_);
    connect(tree_,&QTreeWidget::itemSelectionChanged,this,[this] {
        if(refreshing_) return;
        artboard_editing_=false;
        std::vector<Canvas::Selection> items;
        auto append=[&](QTreeWidgetItem* item){items.push_back({item->data(0,Qt::UserRole).toString().toStdString(),item->data(0,Qt::UserRole+1).toString().toStdString()});};
        for(auto* item:tree_->selectedItems())if(item!=tree_->currentItem())append(item);
        if(tree_->currentItem()&&tree_->currentItem()->isSelected())append(tree_->currentItem());
        canvas->set_selections(std::move(items));
    });
    host.changed=[this]{
        const bool projected=canvas_notification_&&canvas_notification_->first==host.session_id&&
            canvas_notification_->second==host.session.revision()&&!host.session.gesture_active();
        refresh(!projected);
    };
    host.status_changed=[this]{status_->setText(host.save_status+"   ·   r"+QString::number(host.session.revision()));};
    canvas->document_changed=[this]{
        // Only this synchronous notification can reuse the Canvas edit's exact
        // projection. External edits/load/Undo, or reentrant state changes, rebuild.
        QScopedValueRollback guard(canvas_notification_);
        if(canvas->projection_succeeded())canvas_notification_=std::make_pair(host.session_id,host.session.revision());
        else canvas_notification_.reset();
        host.edited();
    };
    canvas->selection_changed=[this]{if(!canvas->selected_object.empty())artboard_editing_=false;sync_tree_selection();rebuild_inspector();};
    canvas->active_artboard_changed=[this]{if(!refreshing_)refresh();};
    canvas->view_state_changed=[this]{sync_utility_view_state();};
    canvas->zoom_changed=[this](double zoom){
        if(!utility_zoom_)return;const QSignalBlocker blocker(utility_zoom_);utility_zoom_->setValue(zoom*100.0);
    };
    canvas->gradient_edit_changed=[this]{rebuild_inspector();};
    canvas->scope_changed=[this]{breadcrumb_->setText(canvas->breadcrumb());};
    canvas->error=[this](const QString& message){statusBar()->showMessage(message,10000);};
    canvas->snap_feedback=[this](const QString& message) {
        if(message.isEmpty())statusBar()->clearMessage();
        else statusBar()->showMessage(message);
    };
    qApp->installEventFilter(this);
    refresh();
}

Window::~Window() {
    qApp->removeEventFilter(this);
    cancel_whip();
    cancel_layout_draft(false);
    canvas->cancel_interaction();
}

bool Window::eventFilter(QObject* watched,QEvent* event) {
    if(layout_preview_active_||layout_preview_invalid_) {
        auto* widget=qobject_cast<QWidget*>(watched);
        const bool inside=widget&&layout_preview_scope_&&(widget==layout_preview_scope_||layout_preview_scope_->isAncestorOf(widget));
        if(event->type()==QEvent::KeyPress) {
            const auto key=static_cast<QKeyEvent*>(event)->key();
            if(key==Qt::Key_Escape&&inside) {
                cancel_layout_draft();
                QTimer::singleShot(0,this,[this]{if(utility_setup_dialog_)rebuild_layout_setup();else if(artboard_editing_)rebuild_inspector();});
                return true;
            } else if(!inside) {
                cancel_layout_draft();
                if(utility_setup_dialog_)utility_setup_dialog_->reject();
            }
        } else if(event->type()==QEvent::MouseButtonPress&&!inside) {
            cancel_layout_draft();
            if(utility_setup_dialog_)utility_setup_dialog_->reject();
        } else if(event->type()==QEvent::ApplicationDeactivate) {
            cancel_layout_draft();if(utility_setup_dialog_)utility_setup_dialog_->reject();
        }
    }
    if(event->type()==QEvent::MouseButtonPress&&!whip_target_) {
        const auto* mouse=static_cast<QMouseEvent*>(event);
        if(mouse->button()==Qt::LeftButton&&watched->property("nect-pick-whip").toBool()) {
            whip_target_=read_ref(watched->property("nect-reference").toByteArray());
            whip_targets_=read_refs(watched->property("nect-targets").toByteArray());
            if(whip_targets_.empty())whip_targets_={*whip_target_};
            whip_selection_=canvas->selections();whip_revision_=host.session.revision();
            whip_composition_=canvas->active_composition();whip_artboard_=canvas->active_artboard();
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
                const auto contours=path_contours(o,&canvas->evaluated_values());
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
    const auto targets=whip_targets_;const auto expected_revision=whip_revision_;const auto frozen_session=whip_session_;const auto dragged=whip_dragged_;
    QByteArray source_bytes;
    auto* viewport=inspector_scroll_->viewport();
    if(viewport->rect().contains(viewport->mapFromGlobal(position))) {
        // Inspect the actual scrolled viewport rather than a transparent whip
        // overlay or an off-viewport child whose isVisible() flag is still true.
        for(auto* under=inspector_->childAt(inspector_->mapFromGlobal(position));
            under && under!=inspector_;under=under->parentWidget()) {
            // A mixed/multiple row is not one unambiguous source property.
            if(read_refs(under->property("nect-targets").toByteArray()).size()>1)break;
            source_bytes=under->property("nect-reference").toByteArray();
            if(!source_bytes.isEmpty())break;
        }
    }
    cancel_whip();
    if(!dragged) {pick_source(targets);return true;}
    perform([&] {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The pick-whip belongs to another document");
        if(source_bytes.isEmpty())throw Error("NO_SOURCE","Drop the pick-whip on a numeric property field");
        const auto source=read_ref(source_bytes);
        host.session.apply({LinkProperties{targets,source,mouse->modifiers().testFlag(Qt::ShiftModifier)}},expected_revision);host.edited();
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
    const auto selection=whip_selection_;const auto same_session=whip_session_==host.session_id;
    whip_target_.reset();whip_targets_.clear();whip_selection_.clear();releaseMouse();
    if(whip_overlay_) {whip_overlay_->hide();whip_overlay_->deleteLater();whip_overlay_=nullptr;}
    statusBar()->clearMessage();
    if(same_session) {
        perform([&]{canvas->set_active_artboard(whip_composition_,whip_artboard_,false);});
        canvas->set_selections(selection);
    }
}

void Window::perform(const std::function<void()>& action) {
    if(layout_preview_active_||layout_preview_invalid_)cancel_layout_draft();
    try {action();} catch(const Error& e) {statusBar()->showMessage(qs(e.code)+": "+QString::fromUtf8(e.what()),12000);}
    catch(const std::exception& e) {statusBar()->showMessage(QString::fromUtf8(e.what()),12000);}
}

bool Window::layout_draft_current() const {
    return layout_preview_active_&&layout_preview_session_==host.session_id&&
        layout_preview_revision_==host.session.revision();
}

bool Window::reject_stale_layout_draft() {
    if((!layout_preview_active_&&!layout_preview_invalid_)||
       (layout_preview_session_==host.session_id&&layout_preview_revision_==host.session.revision()))return false;
    cancel_layout_draft();
    statusBar()->showMessage("REVISION_CONFLICT: Discarded a stale layout draft",12000);
    QTimer::singleShot(0,this,[this]{if(utility_setup_dialog_)rebuild_layout_setup();else if(artboard_editing_)rebuild_inspector();});
    return true;
}

void Window::cancel_layout_draft(bool refresh_canvas) {
    const bool active=layout_preview_active_;
    if(active&&host.session.gesture_active())host.session.cancel_gesture();
    layout_preview_active_=false;layout_preview_invalid_=false;layout_preview_session_.clear();
    layout_preview_revision_=0;layout_preview_scope_.clear();
    if(refresh_canvas&&active&&canvas) {canvas->refresh();canvas->update();}
}

bool Window::preview_layout_draft(const std::vector<Command>& commands,QWidget* scope) {
    if(reject_stale_layout_draft())return false;
    if(commands.empty())return false;
    if(layout_preview_active_&&layout_preview_scope_!=scope)cancel_layout_draft();
    const bool starting=!layout_preview_active_;
    try {
        if(starting) {
            layout_preview_session_=host.session_id;layout_preview_revision_=host.session.revision();
            layout_preview_scope_=scope;host.session.begin_gesture(layout_preview_revision_);layout_preview_active_=true;
        }
        if(!layout_draft_current())throw Error("REVISION_CONFLICT","Layout draft belongs to an older document session or revision");
        host.session.update_gesture(commands);layout_preview_invalid_=false;
        canvas->refresh();canvas->update();return true;
    } catch(const Error& error) {
        if(starting)cancel_layout_draft();
        else layout_preview_invalid_=true;
        statusBar()->showMessage(qs(error.code)+": "+QString::fromUtf8(error.what()),12000);return false;
    } catch(const std::exception& error) {
        if(starting)cancel_layout_draft();
        else layout_preview_invalid_=true;
        statusBar()->showMessage(QString::fromUtf8(error.what()),12000);return false;
    }
}

bool Window::commit_layout_draft(const std::vector<Command>& commands,QWidget* scope) {
    if(reject_stale_layout_draft())return false;
    if(layout_preview_invalid_) {
        statusBar()->showMessage("INVALID_LAYOUT: Correct the draft before applying it",12000);return false;
    }
    if(!preview_layout_draft(commands,scope))return false;
    try {
        if(!layout_draft_current())throw Error("REVISION_CONFLICT","Layout draft belongs to an older document session or revision");
        host.session.commit_gesture();
        layout_preview_active_=false;layout_preview_invalid_=false;layout_preview_session_.clear();
        layout_preview_revision_=0;layout_preview_scope_.clear();
        host.edited();
        QTimer::singleShot(0,this,[this]{if(utility_setup_dialog_)rebuild_layout_setup();});
        return true;
    } catch(const Error& error) {
        cancel_layout_draft();statusBar()->showMessage(qs(error.code)+": "+QString::fromUtf8(error.what()),12000);return false;
    } catch(const std::exception& error) {
        cancel_layout_draft();statusBar()->showMessage(QString::fromUtf8(error.what()),12000);return false;
    }
}

void Window::refresh(bool project_canvas) {
    if(refreshing_)return;
    if(layout_preview_active_&&!layout_draft_current()) {
        cancel_layout_draft(false);statusBar()->showMessage("REVISION_CONFLICT: Discarded a stale layout preview",12000);
    }
    refreshing_=true;
    if(project_canvas)canvas->refresh();
    const QSignalBlocker blocker(tree_);
    const auto& d=host.session.document();
    QString signature=host.session_id;
    std::function<void(const Id&)> fingerprint=[&](const Id& id) {
        const auto& o=d.objects.at(id);
        signature+="("+qs(id)+":"+QString::number(o.name.size())+":"+qs(o.name)+"|visible:"+QString::number(o.visible);
        if(o.compositing.mask)signature+="|mask:"+qs(o.compositing.mask->source);
        if(o.source)signature+="|source:"+qs(o.source->type)+":"+qs(o.source->id);
        if(o.transform_parent)signature+="|follow:"+qs(*o.transform_parent);
        for(const auto& c:path_contours(o,&canvas->evaluated_values())) {signature+="["+qs(c.id);for(const auto& p:c.points)signature+=":"+qs(p.id);signature+="]";}
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
        item->setText(0,qs(o.name)+(o.visible?QString{}:QString(" ◌"))+(o.compositing.mask?QString(" [mask]"):QString{})+(o.transform_parent?QString(" ↗"):QString{}));item->setData(0,Qt::UserRole,qs(id));
        item->setToolTip(0,(o.source?primitive_label(*o.source)+" source · ":QString{})+qs(id)+
            (o.transform_parent?"\nTransform follows "+qs(d.objects.at(*o.transform_parent).name)+" · "+qs(*o.transform_parent):QString{}));
        for(const auto& child:o.children) append(child,item);
        for(const auto& c:path_contours(o,&canvas->evaluated_values())) for(std::size_t i=0;i<c.points.size();++i) {
            auto* point=new QTreeWidgetItem(item);
            point->setText(0,point_label(o,c.points[i],i));point->setData(0,Qt::UserRole,qs(id));point->setData(0,Qt::UserRole+1,qs(c.points[i].id));
            point->setToolTip(0,qs(c.points[i].id));
        }
        item->setExpanded(expanded.contains(qs(id)));
    };
    for(const auto& comp:d.compositions) if(comp.id==canvas->active_composition())for(const auto& id:comp.roots) append(id,nullptr);
    tree_signature_=signature;
    }
    sync_tree_selection();
    rebuild_artboards();
    undo_->setEnabled(host.session.can_undo());redo_->setEnabled(host.session.can_redo());
    status_->setText(host.save_status+"   ·   r"+QString::number(host.session.revision()));
    setWindowTitle((host.file_path.isEmpty()?"Untitled":QFileInfo(host.file_path).fileName())+" — Nect α");
    breadcrumb_->setText(canvas->breadcrumb());
    refreshing_=false;
    rebuild_inspector(true);
    color_tools_->refresh();
    refresh_history();
    update_utility_strip();
}

void Window::sync_utility_view_state() {
    if(!utility_guides_||!utility_grid_||!utility_snap_action_||!utility_snap_)return;
    const auto state=[](bool enabled){return enabled?QStringLiteral("ON"):QStringLiteral("OFF");};
    {
        const QSignalBlocker blocker(utility_guides_);utility_guides_->setChecked(canvas->show_guides());
        utility_guides_->setText("Guides "+state(canvas->show_guides()));
        utility_guides_->setAccessibleName("Show Guides · "+state(canvas->show_guides()));
        utility_guides_->setAccessibleDescription("Guide overlay visibility; Guide Snap is independent. Current state: "+state(canvas->show_guides())+".");
        utility_guides_->setToolTip("Show or hide Guide overlays; Guide Snap remains independent. Current state: "+state(canvas->show_guides())+".");
    }
    {
        const QSignalBlocker blocker(utility_grid_);utility_grid_->setChecked(canvas->show_grid());
        utility_grid_->setText("Grid "+state(canvas->show_grid()));
        utility_grid_->setAccessibleName("Show Grid · "+state(canvas->show_grid()));
        utility_grid_->setAccessibleDescription("Grid overlay visibility; Grid Snap is independent. Current state: "+state(canvas->show_grid())+".");
        utility_grid_->setToolTip("Show or hide Grid overlays; Grid Snap remains independent. Current state: "+state(canvas->show_grid())+".");
    }
    {
        const QSignalBlocker blocker(utility_snap_action_);utility_snap_action_->setChecked(canvas->snap_enabled());
        utility_snap_action_->setText("Snap "+state(canvas->snap_enabled()));
    }
    const auto snapping=state(canvas->snap_enabled());
    utility_snap_->setAccessibleName("Snap · "+snapping);
    utility_snap_->setAccessibleDescription("Enable or disable Snap candidates independently of Guide/Grid overlay visibility. Current state: "+snapping+".");
    utility_snap_->setToolTip("Enable or disable Snap candidates; hidden Guide/Grid overlays remain eligible if their Snap category is enabled. Current state: "+snapping+".");
}

void Window::update_utility_strip() {
    if(!utility_artboard_||!canvas)return;
    sync_utility_view_state();
    if(utility_zoom_) {const QSignalBlocker blocker(utility_zoom_);utility_zoom_->setValue(canvas->zoom()*100.0);}
    try {
        const auto& document=host.session.document();
        const auto& composition=find_composition(document,canvas->active_composition());
        const auto& board=find_artboard(composition,canvas->active_artboard());
        const auto resolved=evaluate_artboard(composition,board.id);
        const auto full=QString("%1 · %2 × %3 du").arg(qs(board.name),QString::number(resolved.width,'g',8),QString::number(resolved.height,'g',8));
        const auto metrics=utility_artboard_->fontMetrics();
        utility_artboard_->setText(metrics.elidedText(full,Qt::ElideMiddle,utility_artboard_->maximumWidth()));
        utility_artboard_->setAccessibleDescription(full+" · active output Artboard");utility_artboard_->setToolTip(full);
    } catch(const std::exception&) {
        utility_artboard_->setText("No active Artboard");utility_artboard_->setAccessibleDescription("No active Artboard or output size is available.");
    }
}

void Window::show_layout_setup(QWidget* anchor) {
    if(layout_preview_active_||layout_preview_invalid_)cancel_layout_draft();
    if(utility_setup_dialog_) {
        utility_setup_dialog_->show();utility_setup_dialog_->raise();utility_setup_dialog_->activateWindow();return;
    }
    auto* dialog=new QDialog(this);utility_setup_dialog_=dialog;
    dialog->setObjectName("utility-setup-popover");dialog->setAccessibleName("Active Artboard layout setup popover");
    dialog->setWindowTitle("Artboard layout setup");dialog->setWindowFlags(Qt::Popup|Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setStyleSheet("QDialog#utility-setup-popover{background:#202833;border:1px solid #566373;border-radius:6px;color:#dbe4ee;}");
    auto* outer=new QVBoxLayout(dialog);outer->setContentsMargins(8,8,8,8);outer->setSpacing(6);
    auto* heading=new QHBoxLayout;auto* title=new QLabel("Margin · Grid · Guides",dialog);title->setAccessibleName("Margin, Grid and Guide setup");
    heading->addWidget(title);heading->addStretch();auto* close=new QToolButton(dialog);close->setObjectName("layout-setup-close");close->setText("Close");close->setAccessibleName("Close layout setup and cancel any draft");
    new HoverFeedback(close);heading->addWidget(close);outer->addLayout(heading);
    auto* scroll=new QScrollArea(dialog);scroll->setObjectName("utility-setup-scroll");scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* body=new QWidget(scroll);body->setObjectName("utility-setup-body");scroll->setWidget(body);outer->addWidget(scroll,1);
    connect(close,&QToolButton::clicked,dialog,&QDialog::reject);
    connect(dialog,&QDialog::finished,this,[this,dialog](int){
        if(layout_preview_active_||layout_preview_invalid_)cancel_layout_draft();
        if(utility_setup_dialog_==dialog)utility_setup_dialog_.clear();
    });
    rebuild_layout_setup();
    const auto* screen=anchor?anchor->screen():QGuiApplication::primaryScreen();
    const QRect available=screen?screen->availableGeometry():QRect(0,0,1440,900);
    const auto popup_width=std::min(480,std::max(1,available.width()-16));
    const auto popup_height=std::min(700,std::max(1,available.height()-24));
    dialog->setMinimumSize(0,0);dialog->resize(popup_width,popup_height);
    QPoint position=anchor?anchor->mapToGlobal(QPoint(0,anchor->height()+2)):QPoint(available.left()+24,available.top()+24);
    position.setX(std::clamp(position.x(),available.left(),std::max(available.left(),available.right()-dialog->width()+1)));
    position.setY(std::clamp(position.y(),available.top(),std::max(available.top(),available.bottom()-dialog->height()+1)));
    dialog->move(position);dialog->show();
}

void Window::rebuild_layout_setup() {
    if(!utility_setup_dialog_)return;
    if(layout_preview_active_||layout_preview_invalid_)cancel_layout_draft();
    auto* body=utility_setup_dialog_->findChild<QWidget*>("utility-setup-body");if(!body)return;
    if(auto* old=body->layout()) {
        while(auto* item=old->takeAt(0)) {if(auto* widget=item->widget()){widget->hide();widget->deleteLater();}delete item;}
        delete old;
    }
    auto* layout=new QVBoxLayout(body);layout->setContentsMargins(8,4,8,8);layout->setSpacing(6);
    edit_artboard(layout);layout->activate();
}

void Window::sync_tree_selection() {
    const QSignalBlocker blocker(tree_);QTreeWidgetItem* active=nullptr;
    for(QTreeWidgetItemIterator i(tree_);*i;++i) {
        const Canvas::Selection item{(*i)->data(0,Qt::UserRole).toString().toStdString(),(*i)->data(0,Qt::UserRole+1).toString().toStdString()};
        (*i)->setSelected(std::find(canvas->selections().begin(),canvas->selections().end(),item)!=canvas->selections().end());
        if(item.object==canvas->selected_object&&item.point==canvas->selected_point)active=*i;
    }
    tree_->setCurrentItem(active,0,QItemSelectionModel::NoUpdate);
}

void Window::show_history() {
    if(history_dialog_) {history_dialog_->show();history_dialog_->raise();history_dialog_->activateWindow();refresh_history();return;}
    history_session_.clear();
    auto* dialog=new QDialog(this);history_dialog_=dialog;dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("history-dialog");dialog->setWindowTitle("History");dialog->resize(560,580);
    auto* layout=new QVBoxLayout(dialog);
    auto* note=new QLabel("Return to a retained operation. Later operations remain available until you make a new edit. History belongs to this open session; saved files and backups are separate.");
    note->setWordWrap(true);layout->addWidget(note);
    history_states_=new QListWidget;history_states_->setObjectName("history-states");layout->addWidget(history_states_);
    history_status_=new QLabel;history_status_->setObjectName("history-status");history_status_->setWordWrap(true);layout->addWidget(history_status_);
    auto* restore=new QPushButton("Return to selected state");restore->setObjectName("history-restore");layout->addWidget(restore);
    connect(restore,&QPushButton::clicked,this,[this]{perform([this]{
        if(history_session_!=host.session_id)throw Error("SESSION_CONFLICT","This History list belongs to another document session");
        if(!history_states_->currentItem())throw Error("NO_HISTORY_SELECTION","Select a retained operation");
        const auto id=history_states_->currentItem()->data(Qt::UserRole).toULongLong();
        canvas->cancel_interaction();const auto before=host.session.revision();
        host.session.restore_history(id,history_revision_);
        if(host.session.revision()!=before)host.edited();
    });});
    auto* close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);
    connect(close,&QDialogButtonBox::rejected,dialog,&QDialog::close);
    dialog->ensurePolished();history_states_->ensurePolished();
    refresh_history();dialog->show();
}

void Window::refresh_history() {
    if(!history_dialog_)return;
    const auto info=host.session.history();
    const bool new_session=history_session_!=host.session_id;
    const auto selected=history_session_==host.session_id&&history_states_->currentItem()?
        history_states_->currentItem()->data(Qt::UserRole).toULongLong():info.current_id;
    const auto scroll=history_states_->verticalScrollBar()->value();const QSignalBlocker blocker(history_states_);
    history_session_=host.session_id;history_revision_=host.session.revision();history_states_->clear();
    QListWidgetItem* current=nullptr;
    for(const auto& state:info.states) {
        const auto is_current=state.id==info.current_id;
        auto* item=new QListWidgetItem((is_current?"Current · ":"")+QString::number(state.id)+" · "+qs(state.label),history_states_);
        item->setData(Qt::UserRole,QVariant::fromValue<qulonglong>(state.id));
        item->setToolTip(qs(state.label)+"\nRetained change estimate: "+QString::number(state.estimated_bytes)+" bytes");
        auto font=item->font();font.setBold(is_current);item->setFont(font);
        if(is_current)current=item;if(state.id==selected)history_states_->setCurrentItem(item);
    }
    if(!history_states_->currentItem())history_states_->setCurrentItem(current);
    if(new_session&&current)history_states_->scrollToItem(current);
    else history_states_->verticalScrollBar()->setValue(scroll);
    history_status_->setText(QString("%1 retained edits / %2 · %3 MiB estimated / %4 MiB budget\n%5 older edits pruned · current session only")
        .arg(info.states.empty()?0:info.states.size()-1).arg(info.max_entries)
        .arg(static_cast<double>(info.retained_bytes)/(1024*1024),0,'f',2)
        .arg(static_cast<double>(info.max_bytes)/(1024*1024),0,'f',0).arg(info.pruned_entries));
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
    if(!duplicate){board.parent_size.reset();board.layout.reset();}
    double right=board.x+board.width;
    for(const auto& entry:comp.artboards) {const auto resolved=evaluate_artboard(comp,entry.id);right=std::max(right,resolved.x+resolved.width);}
    board.id=new_id();board.name=duplicate?selected.name+" copy":"Artboard "+std::to_string(comp.artboards.size()+1);
    if(duplicate&&board.layout&&board.layout->grid)board.layout->grid->id=new_id();
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
    const auto frozen_revision=host.session.revision();
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

    auto* overlays=new QGroupBox("Layout overlays · this window");auto* overlay_layout=new QVBoxLayout(overlays);
    auto* show_guides=new QCheckBox("Show Guides");show_guides->setObjectName("overlay-show-guides");show_guides->setChecked(canvas->show_guides());overlay_layout->addWidget(show_guides);
    auto* show_grid=new QCheckBox("Show Grid");show_grid->setObjectName("overlay-show-grid");show_grid->setChecked(canvas->show_grid());overlay_layout->addWidget(show_grid);
    auto* show_margin=new QCheckBox("Show Margin");show_margin->setObjectName("overlay-show-margin");show_margin->setChecked(canvas->show_margin());overlay_layout->addWidget(show_margin);
    auto* guide_edit=new QCheckBox("Edit Guide positions on Canvas");guide_edit->setObjectName("guide-edit-mode");guide_edit->setChecked(canvas->guide_edit_mode());overlay_layout->addWidget(guide_edit);
    connect(show_guides,&QCheckBox::toggled,canvas,&Canvas::set_show_guides);
    connect(show_grid,&QCheckBox::toggled,canvas,&Canvas::set_show_grid);
    connect(show_margin,&QCheckBox::toggled,canvas,&Canvas::set_show_margin);
    connect(guide_edit,&QCheckBox::toggled,this,[this](bool enabled){if(layout_preview_active_||layout_preview_invalid_)cancel_layout_draft();canvas->set_guide_edit_mode(enabled);});
    layout->addWidget(overlays);

    auto make_number=[&](QWidget* parent,const char* object_name,const QString& name,const QString& initial) {
        auto* input=new QLineEdit(initial,parent);input->setObjectName(QString::fromLatin1(object_name));input->setAccessibleName(name+" · du");
        input->setToolTip(name+" in Composition distance units (du). Press Enter or Apply to commit.");return input;
    };
    auto parse_number=[](QLineEdit* input) {
        bool valid=false;const auto value=input->text().trimmed().toDouble(&valid);
        if(!valid||!std::isfinite(value))throw Error("INVALID_VALUE","Enter a finite number");
        return value;
    };
    auto parse_count=[](QLineEdit* input) {
        bool valid=false;const auto value=input->text().trimmed().toDouble(&valid);
        if(!valid||!std::isfinite(value)||value<1||value>1000||std::floor(value)!=value)
            throw Error("INVALID_LAYOUT","Counts must be whole numbers from 1 to 1000");
        return static_cast<std::size_t>(value);
    };
    auto mark_invalid=[this](QWidget* scope,const QString& message) {
        layout_preview_invalid_=true;layout_preview_scope_=scope;
        if(!layout_preview_active_){layout_preview_session_=host.session_id;layout_preview_revision_=host.session.revision();}
        statusBar()->showMessage(message,12000);
    };
    auto guard_editor=[this,frozen_session,frozen_revision] {
        if(reject_stale_layout_draft())return true;
        if(host.session_id==frozen_session&&host.session.revision()==frozen_revision)return false;
        statusBar()->showMessage("REVISION_CONFLICT: Refresh layout controls before editing",12000);
        QTimer::singleShot(0,this,[this]{if(utility_setup_dialog_)rebuild_layout_setup();else if(artboard_editing_)rebuild_inspector();});
        return true;
    };
    using LayoutBuilder=std::function<std::vector<Command>()>;
    auto preview_from=[this,mark_invalid,guard_editor](QWidget* scope,const LayoutBuilder& build) {
        if(guard_editor())return;
        try {preview_layout_draft(build(),scope);}
        catch(const Error& error){mark_invalid(scope,qs(error.code)+": "+QString::fromUtf8(error.what()));}
        catch(const std::exception& error){mark_invalid(scope,QString::fromUtf8(error.what()));}
    };
    auto commit_from=[this,mark_invalid,guard_editor](QWidget* scope,const LayoutBuilder& build) {
        if(guard_editor())return;
        try {commit_layout_draft(build(),scope);}
        catch(const Error& error){mark_invalid(scope,qs(error.code)+": "+QString::fromUtf8(error.what()));}
        catch(const std::exception& error){mark_invalid(scope,QString::fromUtf8(error.what()));}
    };
    auto commit_explicit=[this,commit_from,guard_editor](QWidget* scope,const LayoutBuilder& build) {
        if(guard_editor())return;
        if(layout_preview_invalid_)cancel_layout_draft();
        commit_from(scope,build);
    };
    auto bind_number=[this,preview_from,commit_from](QLineEdit* input,QWidget* scope,const LayoutBuilder& build) {
        connect(input,&QLineEdit::textEdited,this,[preview_from,scope,build]{preview_from(scope,build);});
        connect(input,&QLineEdit::returnPressed,this,[commit_from,scope,build]{commit_from(scope,build);});
    };
    auto set_layout_command=[composition,id](const ArtboardLayout& value)->std::vector<Command> {
        auto payload=std::optional<ArtboardLayout>{value};
        if(!payload->margin&&!payload->grid)payload.reset();
        return {SetArtboardLayout{composition,id,std::move(payload)}};
    };

    auto* margin_box=new QGroupBox("Margin inset · du");margin_box->setObjectName("layout-margin");
    auto* margin_form=new QFormLayout(margin_box);margin_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const Margin initial_margin=board.layout&&board.layout->margin?*board.layout->margin:Margin{};
    auto* margin_left=make_number(margin_box,"margin-left","Left",QString::number(initial_margin.left,'g',15));
    auto* margin_top=make_number(margin_box,"margin-top","Top",QString::number(initial_margin.top,'g',15));
    auto* margin_right=make_number(margin_box,"margin-right","Right",QString::number(initial_margin.right,'g',15));
    auto* margin_bottom=make_number(margin_box,"margin-bottom","Bottom",QString::number(initial_margin.bottom,'g',15));
    margin_form->addRow("Left · du",margin_left);margin_form->addRow("Top · du",margin_top);
    margin_form->addRow("Right · du",margin_right);margin_form->addRow("Bottom · du",margin_bottom);
    const LayoutBuilder margin_builder=[read,composition,id,parse_number,set_layout_command,margin_left,margin_top,margin_right,margin_bottom] {
        auto current=read();auto value=current.layout.value_or(ArtboardLayout{});
        value.margin=Margin{parse_number(margin_left),parse_number(margin_top),parse_number(margin_right),parse_number(margin_bottom)};
        return set_layout_command(value);
    };
    auto* margin_actions=new QWidget(margin_box);auto* margin_buttons=new QHBoxLayout(margin_actions);margin_buttons->setContentsMargins(0,0,0,0);
    auto* margin_apply=new QPushButton("Apply Margin",margin_actions);margin_apply->setObjectName("margin-apply");margin_buttons->addWidget(margin_apply);
    auto* margin_clear=new QPushButton("Clear Margin",margin_actions);margin_clear->setObjectName("margin-clear");margin_clear->setEnabled(board.layout&&board.layout->margin);margin_buttons->addWidget(margin_clear);margin_form->addRow(margin_actions);
    for(auto* input:{margin_left,margin_top,margin_right,margin_bottom})bind_number(input,margin_box,margin_builder);
    connect(margin_apply,&QPushButton::clicked,this,[commit_from,margin_box,margin_builder]{commit_from(margin_box,margin_builder);});
    connect(margin_clear,&QPushButton::clicked,this,[this,read,composition,id,commit_explicit,margin_box] {
        auto current=read();auto value=current.layout.value_or(ArtboardLayout{});value.margin.reset();
        const auto payload=(value.grid?std::optional<ArtboardLayout>(value):std::nullopt);
        commit_explicit(margin_box,[composition,id,payload]{return std::vector<Command>{SetArtboardLayout{composition,id,payload}};});
    });
    layout->addWidget(margin_box);

    auto* grid_box=new QGroupBox("Grid · Artboard-local bounds and cells · du");grid_box->setObjectName("layout-grid");
    auto* grid_form=new QFormLayout(grid_box);grid_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const Grid initial_grid=board.layout&&board.layout->grid?*board.layout->grid:Grid{new_id(),{0,0,resolved.width,resolved.height},1,1,0,0};
    const Id grid_id=initial_grid.id;
    auto* grid_x=make_number(grid_box,"grid-x","Grid X",QString::number(initial_grid.bounds.x,'g',15));
    auto* grid_y=make_number(grid_box,"grid-y","Grid Y",QString::number(initial_grid.bounds.y,'g',15));
    auto* grid_width=make_number(grid_box,"grid-width","Grid width",QString::number(initial_grid.bounds.width,'g',15));
    auto* grid_height=make_number(grid_box,"grid-height","Grid height",QString::number(initial_grid.bounds.height,'g',15));
    auto* grid_columns=make_number(grid_box,"grid-columns","Columns",QString::number(static_cast<qulonglong>(initial_grid.columns)));
    auto* grid_rows=make_number(grid_box,"grid-rows","Rows",QString::number(static_cast<qulonglong>(initial_grid.rows)));
    auto* grid_column_gutter=make_number(grid_box,"grid-column-gutter","Column gutter",QString::number(initial_grid.column_gutter,'g',15));
    auto* grid_row_gutter=make_number(grid_box,"grid-row-gutter","Row gutter",QString::number(initial_grid.row_gutter,'g',15));
    grid_form->addRow("X · du",grid_x);grid_form->addRow("Y · du",grid_y);
    grid_form->addRow("Width · du",grid_width);grid_form->addRow("Height · du",grid_height);
    grid_form->addRow("Columns · integer",grid_columns);grid_form->addRow("Rows · integer",grid_rows);
    grid_form->addRow("Column gutter · du",grid_column_gutter);grid_form->addRow("Row gutter · du",grid_row_gutter);
    const LayoutBuilder grid_builder=[read,composition,id,parse_number,parse_count,set_layout_command,grid_id,grid_x,grid_y,grid_width,grid_height,grid_columns,grid_rows,grid_column_gutter,grid_row_gutter] {
        auto current=read();auto value=current.layout.value_or(ArtboardLayout{});
        Grid grid{grid_id,{parse_number(grid_x),parse_number(grid_y),parse_number(grid_width),parse_number(grid_height)},
            parse_count(grid_columns),parse_count(grid_rows),parse_number(grid_column_gutter),parse_number(grid_row_gutter)};
        value.grid=std::move(grid);return set_layout_command(value);
    };
    auto* grid_actions=new QWidget(grid_box);auto* grid_buttons=new QHBoxLayout(grid_actions);grid_buttons->setContentsMargins(0,0,0,0);
    auto* grid_apply=new QPushButton("Apply Grid",grid_actions);grid_apply->setObjectName("grid-apply");grid_buttons->addWidget(grid_apply);
    auto* grid_copy=new QPushButton("Set Grid to margin box",grid_actions);grid_copy->setObjectName("grid-copy-margin-box");grid_copy->setToolTip("Copy the authored Margin box once; later Margin edits do not change Grid.");grid_buttons->addWidget(grid_copy);
    auto* grid_clear=new QPushButton("Clear Grid",grid_actions);grid_clear->setObjectName("grid-clear");grid_clear->setEnabled(board.layout&&board.layout->grid);grid_buttons->addWidget(grid_clear);
    grid_form->addRow(grid_actions);
    for(auto* input:{grid_x,grid_y,grid_width,grid_height,grid_columns,grid_rows,grid_column_gutter,grid_row_gutter})bind_number(input,grid_box,grid_builder);
    connect(grid_apply,&QPushButton::clicked,this,[commit_from,grid_box,grid_builder]{commit_from(grid_box,grid_builder);});
    connect(grid_copy,&QPushButton::clicked,this,[this,read,composition,id,grid_id,commit_explicit,guard_editor,grid_box] {
        if(guard_editor())return;
        const auto current=read();
        if(!current.layout||!current.layout->margin) {statusBar()->showMessage("INVALID_LAYOUT: Add an authored Margin before copying its box",12000);return;}
        const auto board_now=evaluate_artboard(find_composition(host.session.document(),composition),id);
        const auto& margin=*current.layout->margin;auto value=current.layout.value();
        auto grid=value.grid.value_or(Grid{grid_id,{},1,1,0,0});
        grid.bounds={margin.left,margin.top,board_now.width-margin.left-margin.right,board_now.height-margin.top-margin.bottom};
        value.grid=std::move(grid);
        commit_explicit(grid_box,[composition,id,value]{return std::vector<Command>{SetArtboardLayout{composition,id,value}};});
    });
    connect(grid_clear,&QPushButton::clicked,this,[this,read,composition,id,commit_explicit,grid_box] {
        auto current=read();auto value=current.layout.value_or(ArtboardLayout{});value.grid.reset();
        const auto payload=(value.margin?std::optional<ArtboardLayout>(value):std::nullopt);
        commit_explicit(grid_box,[composition,id,payload]{return std::vector<Command>{SetArtboardLayout{composition,id,payload}};});
    });
    layout->addWidget(grid_box);

    auto* guides_box=new QGroupBox("Composition Guides");guides_box->setObjectName("composition-guides");auto* guides_layout=new QVBoxLayout(guides_box);
    auto build_guide_row=[&](const Guide& source) {
        auto* row=new QGroupBox(qs(source.name),guides_box);row->setObjectName("guide-row-"+qs(source.id));auto* form=new QFormLayout(row);
        auto* name_input=new QLineEdit(qs(source.name),row);name_input->setObjectName("guide-name-"+qs(source.id));
        auto* axis_input=new QComboBox(row);axis_input->setObjectName("guide-axis-"+qs(source.id));axis_input->addItem("Vertical · x",QStringLiteral("x"));axis_input->addItem("Horizontal · y",QStringLiteral("y"));axis_input->setCurrentIndex(axis_input->findData(qs(source.axis)));
        auto* position_input=make_number(row,("guide-position-"+source.id).c_str(),"Guide position",QString::number(source.position,'g',15));
        form->addRow("Name",name_input);form->addRow("Axis",axis_input);form->addRow("Position · du",position_input);
        auto* actions=new QWidget(row);auto* buttons=new QHBoxLayout(actions);buttons->setContentsMargins(0,0,0,0);
        auto* apply=new QPushButton("Apply Guide",actions);apply->setObjectName("guide-apply-"+qs(source.id));buttons->addWidget(apply);
        auto* remove=new QPushButton("Delete Guide",actions);remove->setObjectName("guide-delete-"+qs(source.id));buttons->addWidget(remove);form->addRow(actions);
        const LayoutBuilder builder=[composition,source,name_input,axis_input,position_input,parse_number] {
            auto changed=source;changed.name=name_input->text().toStdString();changed.axis=axis_input->currentData().toString().toStdString();changed.position=parse_number(position_input);
            return std::vector<Command>{UpdateGuide{composition,std::move(changed)}};
        };
        bind_number(name_input,row,builder);bind_number(position_input,row,builder);
        connect(axis_input,qOverload<int>(&QComboBox::currentIndexChanged),this,[preview_from,row,builder](int){preview_from(row,builder);});
        connect(apply,&QPushButton::clicked,this,[commit_from,row,builder]{commit_from(row,builder);});
        connect(remove,&QPushButton::clicked,this,[commit_explicit,row,composition,guide_id=source.id]{commit_explicit(row,[composition,guide_id]{return std::vector<Command>{DeleteGuide{composition,guide_id}};});});
        guides_layout->addWidget(row);
    };
    for(const auto& guide:comp.guides)build_guide_row(guide);
    auto* add_guide_box=new QGroupBox("Add Guide");add_guide_box->setObjectName("guide-add-row");auto* add_guide_form=new QFormLayout(add_guide_box);
    const Guide new_guide{new_id(),"Guide","x",0};
    auto* new_guide_name=new QLineEdit(QString::fromStdString(new_guide.name),add_guide_box);new_guide_name->setObjectName("guide-new-name");
    auto* new_guide_axis=new QComboBox(add_guide_box);new_guide_axis->setObjectName("guide-new-axis");new_guide_axis->addItem("Vertical · x",QStringLiteral("x"));new_guide_axis->addItem("Horizontal · y",QStringLiteral("y"));
    auto* new_guide_position=make_number(add_guide_box,"guide-new-position","Guide position","0");
    add_guide_form->addRow("Name",new_guide_name);add_guide_form->addRow("Axis",new_guide_axis);add_guide_form->addRow("Position · du",new_guide_position);
    auto* add_guide=new QPushButton("Add Guide",add_guide_box);add_guide->setObjectName("guide-add");add_guide_form->addRow(add_guide);
    const LayoutBuilder add_guide_builder=[composition,new_guide,new_guide_name,new_guide_axis,new_guide_position,parse_number] {
        auto value=new_guide;value.name=new_guide_name->text().toStdString();value.axis=new_guide_axis->currentData().toString().toStdString();value.position=parse_number(new_guide_position);
        return std::vector<Command>{AddGuide{composition,std::move(value)}};
    };
    bind_number(new_guide_name,add_guide_box,add_guide_builder);bind_number(new_guide_position,add_guide_box,add_guide_builder);
    connect(new_guide_axis,qOverload<int>(&QComboBox::currentIndexChanged),this,[preview_from,add_guide_box,add_guide_builder](int){preview_from(add_guide_box,add_guide_builder);});
    connect(add_guide,&QPushButton::clicked,this,[commit_from,add_guide_box,add_guide_builder]{commit_from(add_guide_box,add_guide_builder);});
    guides_layout->addWidget(add_guide_box);layout->addWidget(guides_box);

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

void Window::rebuild_inspector(bool use_canvas_values) {
    std::erase_if(expression_drafts_,[&](const auto& item){return item.second.session!=host.session_id;});
    QString context=host.session_id+(artboard_editing_?"/frame/"+qs(canvas->active_artboard()):QString{});
    for(const auto& item:canvas->selections())context+="/"+qs(item.object)+":"+qs(item.point);
    const auto scroll=context==inspector_context_?inspector_scroll_->verticalScrollBar()->value():0;
    inspector_context_=context;
    // Qt may scroll to a disappearing focused field while the new form lays out.
    // Restore the previous viewport only for the same editing context.
    QTimer::singleShot(0,this,[this,context,scroll]{if(inspector_context_==context)inspector_scroll_->verticalScrollBar()->setValue(scroll);});
    // Avoid deleting a focused field synchronously from its editingFinished signal.
    if(auto* old=inspector_->layout()) {
        while(auto* child=old->takeAt(0)) { if(child->widget()) {child->widget()->hide();child->widget()->deleteLater();}delete child; }
        delete old;
    }
    auto* layout=new QVBoxLayout(inspector_);
    if(artboard_editing_) {edit_artboard(layout);return;}
    const auto& d=host.session.document();
    if(!d.objects.contains(canvas->selected_object)) {layout->addWidget(new QLabel("Add a shape, Curve or Text.\nSelect a point to edit its handles."));layout->addStretch();return;}
    const auto& o=d.objects.at(canvas->selected_object);
    // A complete Window refresh has just evaluated this same committed Session
    // for Canvas. Reuse those numbers; standalone selection/draft refreshes and
    // an active preview still read committed values through the core evaluator.
    inspector_values_=use_canvas_values&&!host.session.gesture_active()?canvas->evaluated_values():evaluate(d);
    if(canvas->selections().size()>1){add_multi_properties(layout);return;}
    if(canvas->selections().size()==1&&canvas->selections().front().point.empty())
        add_alignment_controls(layout,canvas->selections());
    auto* name=new QLineEdit(qs(o.name));name->setAccessibleName("Object name");layout->addWidget(name);
    connect(name,&QLineEdit::editingFinished,this,[this,name,id=o.id]{
        if(!name->isModified()) return;
        name->setModified(false);
        perform([&]{host.session.apply({Rename{id,name->text().toStdString()}},host.session.revision());host.edited();});
    });
    if(!o.stack.empty()) {
        auto* jump=new QPushButton(QString("Shape stack · %1").arg(o.stack.size()));jump->setObjectName("stack-jump");
        jump->setToolTip("Go directly to an existing paint or path operation");auto* menu=new QMenu(jump);
        for(std::size_t index=0;index<o.stack.size();++index) {
            const auto& operation=o.stack[index];auto* entry=menu->addAction(QString::number(index+1)+" · "+operation_label(operation));
            connect(entry,&QAction::triggered,this,[this,id=operation.id]{reveal_operation(id);});
        }
        jump->setMenu(menu);layout->addWidget(jump);
    }
    auto section=[&](const QString& title){auto* box=new QGroupBox(title);auto* form=new QFormLayout(box);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);return form;};
    if(o.text)add_text_properties(layout,o);
    if(o.image)add_image_properties(layout,o);
    if(o.source) {
        auto* generator=section("1 · "+primitive_label(*o.source)+" source");
        for(const auto* parameter:{"center_x","center_y","points","rotation","radius","outer_radius","inner_radius","width","height"})
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
        if(o.source->type=="nect.shape.polygon"||o.source->type=="nect.shape.star") {
            auto* topology=new QLabel("Point edits stay at their angular positions. Changing Points is blocked if an edited or linked vertex would disappear.");
            topology->setObjectName("primitive-topology-note");topology->setWordWrap(true);correction->addRow(topology);
        }
        if(o.point_edit) {
            auto* reset=new QPushButton("Reset point edits…");reset->setObjectName("point-edit-reset");correction->addRow(reset);
            connect(reset,&QPushButton::clicked,this,[this,id=o.id,frozen_session=host.session_id,field_count]{perform([&]{
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Point Edit belongs to another document");
                const auto before=host.session.revision();
                const auto choice=QMessageBox::question(this,"Reset point edits",QString("Remove all %1 point/handle overrides and their bindings? The generator and appearance stay editable. This is one undoable edit.").arg(field_count),QMessageBox::Reset|QMessageBox::Cancel,QMessageBox::Cancel);
                if(choice!=QMessageBox::Reset)return;
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Document changed while reviewing Point Edit reset");
                canvas->cancel_interaction();host.session.apply({ClearPointEdit{id}},before);host.edited();
            });});
        }
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
    if(o.compositing.mask)add_compositing_properties(layout,o);
    add_transform_properties(layout,o);
    if(!o.compositing.mask)add_compositing_properties(layout,o);
    if(o.kind==Kind::path||o.kind==Kind::text)add_stack(layout,o);
    auto* hint=new QLabel("Right-click a value to copy, paste or unlink.\n↗ picks a property source; += / -= adjusts once.");
    hint->setWordWrap(true);hint->setStyleSheet("color: #929aa6; font-size: 11px;");layout->addWidget(hint);layout->addStretch();
}
void Window::add_compositing_properties(QVBoxLayout* layout,const Object& object) {
    auto* box=new QGroupBox("Compositing");auto* form=new QFormLayout(box);form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);
    const auto id=object.id;const auto session=host.session_id;
    auto apply=[this,session](Command command){if(host.session_id!=session)throw Error("SESSION_CONFLICT","Compositing belongs to another document");canvas->cancel_interaction();host.session.apply({std::move(command)},host.session.revision());host.edited();};
    auto* visible=new QCheckBox("Show artwork");visible->setObjectName("object-visible");visible->setChecked(object.visible);form->addRow(visible);
    connect(visible,&QCheckBox::toggled,this,[this,visible,id,apply](bool enabled){bool ok=false;perform([&]{apply(SetVisibility{id,enabled});ok=true;});if(!ok){QSignalBlocker b(visible);visible->setChecked(!enabled);}});
    add_property(form,{id,"","composite.opacity"},"Object opacity");
    auto* blend=new QComboBox;blend->setObjectName("object-blend");
    for(const auto* mode:{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn","hard-light","soft-light","difference","exclusion"})blend->addItem(QString::fromLatin1(mode),QString::fromLatin1(mode));
    blend->setCurrentIndex(blend->findData(qs(object.compositing.blend)));form->addRow("Blend",blend);
    connect(blend,&QComboBox::currentIndexChanged,this,[this,blend,id,apply](int){perform([&]{const auto& current=host.session.document().objects.at(id).compositing;apply(SetCompositing{id,blend->currentData().toString().toStdString(),current.isolated});});});
    auto* isolate=new QCheckBox("Isolate from backdrop");isolate->setObjectName("object-isolated");isolate->setChecked(object.compositing.isolated);form->addRow(isolate);
    connect(isolate,&QCheckBox::toggled,this,[this,isolate,id,apply](bool value){bool ok=false;perform([&]{apply(SetCompositing{id,host.session.document().objects.at(id).compositing.blend,value});ok=true;});if(!ok){QSignalBlocker b(isolate);isolate->setChecked(!value);}});
    auto* scope=new QLabel("Opacity, masks and blending apply to the composed result. Neutral Groups pass through.");scope->setWordWrap(true);scope->setStyleSheet("color:#9ea7b4;");form->addRow(scope);
    if(!object.compositing.mask)return;
    const auto mask=*object.compositing.mask;
    auto* mask_box=new QGroupBox("Geometry mask");auto* mask_form=new QFormLayout(mask_box);mask_form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(mask_box);
    auto* enabled=new QCheckBox("Mask enabled");enabled->setObjectName("mask-enabled");enabled->setChecked(mask.enabled);mask_form->addRow(enabled);
    connect(enabled,&QCheckBox::toggled,this,[this,enabled,id,apply](bool value){bool ok=false;perform([&]{auto mask=*host.session.document().objects.at(id).compositing.mask;mask.enabled=value;apply(SetMask{id,mask});ok=true;});if(!ok){QSignalBlocker b(enabled);enabled->setChecked(!value);}});
    auto* edit=new QPushButton("Edit: "+qs(host.session.document().objects.at(mask.source).name));edit->setObjectName("mask-edit-source");edit->setToolTip("Select the retained source to edit its points and parameters. Its normal visibility stays unchanged.");mask_form->addRow(edit);
    connect(edit,&QPushButton::clicked,this,[this,source=mask.source]{canvas->set_selection(source);});
    auto* rule=new QComboBox;rule->setObjectName("mask-fill-rule");rule->addItem("Nonzero","nonzero");rule->addItem("Even–odd","evenodd");rule->setCurrentIndex(mask.fill_rule=="evenodd"?1:0);mask_form->addRow("Fill rule",rule);
    connect(rule,&QComboBox::currentIndexChanged,this,[this,rule,id,apply](int){perform([&]{auto mask=*host.session.document().objects.at(id).compositing.mask;mask.fill_rule=rule->currentData().toString().toStdString();apply(SetMask{id,mask});});});
    auto* outline=new QCheckBox("Show mask outline");outline->setObjectName("mask-show-outline");outline->setChecked(canvas->show_mask_outline());mask_form->addRow(outline);
    connect(outline,&QCheckBox::toggled,canvas,&Canvas::set_show_mask_outline);
    auto* source_visible=new QCheckBox("Show source artwork");source_visible->setObjectName("mask-source-visible");source_visible->setChecked(host.session.document().objects.at(mask.source).visible);mask_form->addRow(source_visible);
    connect(source_visible,&QCheckBox::toggled,this,[this,source_visible,source=mask.source,apply](bool value){bool ok=false;perform([&]{apply(SetVisibility{source,value});ok=true;});if(!ok){QSignalBlocker b(source_visible);source_visible->setChecked(!value);}});
    auto* remove=new QPushButton("Remove mask");remove->setObjectName("mask-remove");remove->setToolTip("Remove clipping; keep source object and its current visibility. Undo restores the mask.");mask_form->addRow(remove);
    connect(remove,&QPushButton::clicked,this,[this,id,apply]{perform([&]{apply(SetMask{id,{}});});});
    auto* note=new QLabel("Uses final source geometry in Composition space. Source paint and opacity do not affect this mask; open paths close implicitly.");note->setWordWrap(true);note->setStyleSheet("color:#9ea7b4;");mask_form->addRow(note);
}
void Window::add_transform_properties(QVBoxLayout* layout,const Object& object) {
    const auto id=object.id;const auto frozen_session=host.session_id;
    auto* box=new QGroupBox("Transform && Anchor");auto* form=new QFormLayout(box);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);
    auto apply=[this,id,frozen_session](const Command& command){
        if(host.session_id!=frozen_session||!host.session.document().objects.contains(id))throw Error("SESSION_CONFLICT","Transform belongs to another document");
        canvas->cancel_interaction();host.session.apply({command},host.session.revision());host.edited();
    };
    const auto position=map_point(canvas->evaluated_transforms().at(id).local,
        {inspector_values_.at({id,"","transform.anchor_x"}),inspector_values_.at({id,"","transform.anchor_y"})});
    for(int axis=0;axis<2;++axis) {
        auto* input=new QLineEdit(display_value(axis?position.y:position.x));
        input->setObjectName(axis?"transform-position-y":"transform-position-x");input->setAccessibleName(axis?"Position Y":"Position X");
        input->setToolTip("Anchor position in the effective parent's coordinates. Enter a value or += / -= adjustment.");form->addRow(axis?"Position Y":"Position X",input);
        connect(input,&QLineEdit::editingFinished,this,[this,id,input,axis,apply,frozen_session]{
            if(!input->isModified())return;input->setModified(false);
            const auto focused=input->hasFocus();const auto name=input->objectName();const auto scroll=inspector_scroll_->verticalScrollBar()->value();
            perform([&]{const auto values=evaluate(host.session.document());const auto tf=evaluate_transforms(host.session.document(),values).at(id);
                auto p=map_point(tf.local,{values.at({id,"","transform.anchor_x"}),values.at({id,"","transform.anchor_y"})});
                auto text=input->text().trimmed();const bool relative=text.startsWith("+=")||text.startsWith("-=");bool ok=false;
                auto value=(relative?text.mid(2):text).toDouble(&ok);if(!ok||!std::isfinite(value))throw Error("INVALID_VALUE","Enter a finite position or += / -= adjustment");
                if(relative)value=(axis?p.y:p.x)+(text.startsWith("-=")?-value:value);
                if(axis)p.y=value;else p.x=value;apply(SetPosition{id,p.x,p.y});
                if(focused)QTimer::singleShot(0,this,[this,id,frozen_session,name,scroll]{
                    if(host.session_id!=frozen_session||canvas->selected_object!=id)return;
                    for(auto* field:inspector_->findChildren<QLineEdit*>(name))if(field->isVisible()){
                        field->setFocus();field->selectAll();inspector_scroll_->verticalScrollBar()->setValue(scroll);break;
                    }
                });
            });
        });
    }
    add_property(form,{id,"","transform.anchor_x"},"Anchor X");add_property(form,{id,"","transform.anchor_y"},"Anchor Y");
    auto* center=new QPushButton("Center Anchor");center->setObjectName("transform-center-anchor");
    center->setToolTip("Use evaluated geometry bounds, excluding stroke width. Artwork stays in place.");form->addRow(center);
    connect(center,&QPushButton::clicked,this,[this,id,apply]{perform([&]{apply(CenterAnchor{id});});});
    auto* edit_anchor=new QPushButton("Edit Anchor on Canvas · Y");edit_anchor->setObjectName("transform-edit-anchor");form->addRow(edit_anchor);
    connect(edit_anchor,&QPushButton::clicked,this,[this]{canvas->set_anchor_edit(true);canvas->setFocus();});
    auto* rotate_row=new QWidget;auto* rotate_layout=new QHBoxLayout(rotate_row);rotate_layout->setContentsMargins(0,0,0,0);
    auto* rotation=new QLineEdit("0");rotation->setObjectName("transform-rotate-by");rotation->setAccessibleName("Rotate by degrees");
    auto* rotate=new QPushButton("Apply");rotate->setObjectName("transform-rotate-apply");rotate_layout->addWidget(rotation);rotate_layout->addWidget(rotate);form->addRow("Rotate by °",rotate_row);
    auto rotate_action=[this,id,rotation,apply]{perform([&]{bool ok=false;const auto angle=rotation->text().toDouble(&ok);
        if(!ok||!std::isfinite(angle))throw Error("INVALID_VALUE","Enter a finite rotation in degrees");if(angle!=0)apply(TransformAroundAnchor{id,angle,1,1});});};
    connect(rotate,&QPushButton::clicked,this,rotate_action);connect(rotation,&QLineEdit::returnPressed,this,rotate_action);
    auto* sx=new QLineEdit("1");sx->setObjectName("transform-scale-x");sx->setAccessibleName("Scale X factor");form->addRow("Scale X ×",sx);
    auto* sy=new QLineEdit("1");sy->setObjectName("transform-scale-y");sy->setAccessibleName("Scale Y factor");form->addRow("Scale Y ×",sy);
    auto* scale=new QPushButton("Apply scale");scale->setObjectName("transform-scale-apply");form->addRow(scale);
    auto scale_action=[this,id,sx,sy,apply]{perform([&]{bool a=false,b=false;const auto x=sx->text().toDouble(&a),y=sy->text().toDouble(&b);
        if(!a||!b||!std::isfinite(x)||!std::isfinite(y))throw Error("INVALID_VALUE","Enter finite scale factors");
        if(x!=1||y!=1)apply(TransformAroundAnchor{id,0,x,y});});};
    connect(scale,&QPushButton::clicked,this,scale_action);connect(sx,&QLineEdit::returnPressed,this,scale_action);connect(sy,&QLineEdit::returnPressed,this,scale_action);
    auto* parent=new QPushButton(object.transform_parent?"Follows: "+qs(host.session.document().objects.at(*object.transform_parent).name):"Follow structure…");
    parent->setObjectName("transform-parent");parent->setToolTip("Choose Transform Parent; structure still controls order and grouping.");
    parent->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);form->addRow("Parent",parent);
    connect(parent,&QPushButton::clicked,this,[this]{perform([this]{choose_transform_parent();});});
    auto* note=new QLabel("Anchor moves preserve artwork. Rotation and scale apply once about that anchor; they are not persistent formulas.");note->setWordWrap(true);note->setStyleSheet("color:#a4acb8;font-size:11px;");form->addRow(note);
    auto* matrix_toggle=new QPushButton("Affine matrix…");matrix_toggle->setObjectName("transform-matrix-toggle");matrix_toggle->setCheckable(true);form->addRow(matrix_toggle);
    auto* matrix=new QWidget;auto* matrix_form=new QFormLayout(matrix);matrix_form->setContentsMargins(0,0,0,0);matrix_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    for(const auto* field:{"tx","ty","a","b","c","d"})add_property(matrix_form,{id,"",std::string("transform.")+field},QString::fromLatin1(field));
    form->addRow(matrix);matrix_toggle->setChecked(matrix_expanded_);matrix->setVisible(matrix_expanded_);
    connect(matrix_toggle,&QPushButton::toggled,this,[this,matrix](bool visible){matrix_expanded_=visible;matrix->setVisible(visible);});
}

void Window::choose_transform_parent() {
    const auto id=canvas->selected_object;if(id.empty())throw Error("NO_SELECTION","Select an object to choose its Transform Parent");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto& document=host.session.document();
    QDialog dialog(this);dialog.setWindowTitle("Transform Parent");dialog.setObjectName("transform-parent-dialog");dialog.resize(430,460);
    auto* layout=new QVBoxLayout(&dialog);auto* note=new QLabel("Choose what this object follows. Structure, paint order and group membership stay the same.");note->setWordWrap(true);layout->addWidget(note);
    auto* search=new QLineEdit;search->setPlaceholderText("Find object…");search->setObjectName("transform-parent-search");layout->addWidget(search);
    auto* list=new QListWidget;list->setObjectName("transform-parent-list");layout->addWidget(list);
    auto* structure=new QListWidgetItem("Follow structure (detach explicit parent)",list);structure->setData(Qt::UserRole,QString{});
    std::function<void(const Id&,const QString&)> append=[&](const Id& item,const QString& path){const auto& o=document.objects.at(item);const auto label=path+qs(o.name);
        if(item!=id){auto* row=new QListWidgetItem(label,list);row->setData(Qt::UserRole,qs(item));row->setToolTip(qs(item));if(document.objects.at(id).transform_parent==item)list->setCurrentItem(row);}
        for(const auto& child:o.children)append(child,label+" / ");};
    for(const auto& root:find_composition(document,canvas->active_composition()).roots)append(root,{});
    if(!list->currentItem())list->setCurrentItem(structure);
    connect(search,&QLineEdit::textChanged,list,[list](const QString& text){for(int i=0;i<list->count();++i)list->item(i)->setHidden(!list->item(i)->text().contains(text,Qt::CaseInsensitive));});
    auto* preserve=new QCheckBox("Keep artwork in place");preserve->setObjectName("transform-parent-preserve");preserve->setChecked(true);layout->addWidget(preserve);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Document changed while choosing Transform Parent");
    const auto* selected=list->currentItem();if(!selected||selected->isHidden())throw Error("NO_SELECTION","Choose a visible transform parent");
    const auto target=selected->data(Qt::UserRole).toString().toStdString();canvas->cancel_interaction();
    host.session.apply({SetTransformParent{id,target.empty()?std::optional<Id>{}:std::optional<Id>{target},preserve->isChecked()}},revision);host.edited();
}
void Window::import_image(bool linked) {
    canvas->cancel_interaction();const auto identity=host.session_id;const auto revision=host.session.revision();
    const auto comp=canvas->active_composition();const auto board=evaluate_artboard(find_composition(host.session.document(),comp),canvas->active_artboard());
    const auto path=QFileDialog::getOpenFileName(this,linked?"Import Linked Image":"Import Embedded Image",{},"PNG / JPEG (*.png *.jpg *.jpeg)");
    if(path.isEmpty())return;
    if(host.session_id!=identity)throw Error("SESSION_CONFLICT","Document changed while choosing an Image");
    const auto id=new_id();host.import_image(path,linked?"linked":"embedded",comp,"",new_id(),id,QFileInfo(path).completeBaseName().toStdString(),board.x,board.y,revision);
    canvas->set_selection(id);host.edited();canvas->setFocus();
}
void Window::add_image_properties(QVBoxLayout* layout,const Object& object) {
    const auto& asset=host.session.document().raster_assets.at(object.image->asset);const auto id=asset.id;
    const auto identity=host.session_id;const auto revision=host.session.revision();
    auto* box=new QGroupBox("Image");auto* form=new QFormLayout(box);form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);
    auto* details=new QLabel(qs(asset.name)+QString(" · %1 × %2 px\n%3 · %4").arg(asset.payload->width()).arg(asset.payload->height()).arg(qs(asset.mode),qs(asset.payload->color_interpretation())));
    details->setWordWrap(true);form->addRow(details);
    auto* status=new QLabel;status->setObjectName("image-link-status");status->setWordWrap(true);
    const auto describe=[&]{const auto state=host.asset_status(id);return state.value("state").toString()+
        (asset.mode=="linked"?QString(" · accepted pixels shown\nCheck link to compare the current file."):QString(" · self-contained"));};
    status->setText(describe());form->addRow(status);
    if(asset.mode=="linked") {auto* location=new QLabel(qs(asset.locator));location->setWordWrap(true);location->setTextInteractionFlags(Qt::TextSelectableByMouse);form->addRow(location);}
    add_property(form,{object.id,"","image.width"},"Width");add_property(form,{object.id,"","image.height"},"Height");
    auto* fit=new QPushButton("Fit width to Artboard");fit->setObjectName("image-fit-width");form->addRow(fit);
    connect(fit,&QPushButton::clicked,this,[this,identity,revision,object_id=object.id,id]{perform([&]{
        if(host.session_id!=identity)throw Error("SESSION_CONFLICT","Image belongs to another document");
        const auto& asset=host.session.document().raster_assets.at(id);const auto board=evaluate_artboard(find_composition(host.session.document(),canvas->active_composition()),canvas->active_artboard());
        host.session.apply({Set{{object_id,"","image.width"},board.width},Set{{object_id,"","image.height"},board.width*asset.payload->height()/asset.payload->width()}},revision);host.edited();
    });});
    if(asset.mode=="linked") {
        auto* check=new QPushButton("Check link");check->setObjectName("image-check-link");form->addRow(check);
        connect(check,&QPushButton::clicked,this,[this,identity,id,status]{perform([&]{
            if(host.session_id!=identity)throw Error("SESSION_CONFLICT","Image belongs to another document");
            const auto result=host.check_asset(id);status->setText(result.value("state").toString()+" · accepted pixels shown\nChecked "+result.value("checked_at").toString());
        });});
    }
    const auto operation=[&](const QString& label,const char* action) {
        auto* button=new QPushButton(label);button->setObjectName("image-"+QString::fromLatin1(action));form->addRow(button);
        connect(button,&QPushButton::clicked,this,[this,identity,revision,id,action=std::string(action)]{perform([&]{
            QString path;
            if(action=="relink") {path=QFileDialog::getOpenFileName(this,"Relink Image",{},"PNG / JPEG (*.png *.jpg *.jpeg)");if(path.isEmpty())return;}
            if(host.session_id!=identity)throw Error("SESSION_CONFLICT","Image belongs to another document");
            host.update_asset(id,action,path,revision);
        });});
    };
    if(asset.mode=="linked") {operation("Reload from link","reload");operation("Embed accepted image","embed");}
    operation("Relink…","relink");
    std::size_t placements=0;for(const auto& [object_id,item]:host.session.document().objects){(void)object_id;if(item.image&&item.image->asset==id)++placements;}
    auto* shared=new QLabel(QString("%1 placement(s) share this asset. Reload and Relink update all placements; display sizes stay unchanged.").arg(placements));shared->setWordWrap(true);form->addRow(shared);
    auto* library=new QPushButton("Image Assets…");form->addRow(library);connect(library,&QPushButton::clicked,this,[this]{perform([this]{show_assets();});});
}
void Window::show_assets() {
    canvas->cancel_interaction();QDialog dialog(this);dialog.setWindowTitle("Image Assets");dialog.setObjectName("image-assets-dialog");dialog.resize(700,470);
    auto* layout=new QVBoxLayout(&dialog);auto* hint=new QLabel("Linked images keep their accepted pixels. Check links to detect file changes; Reload or Relink accepts a new version. Deleting an object keeps its asset available here.");hint->setWordWrap(true);layout->addWidget(hint);
    auto* list=new QListWidget;list->setObjectName("image-assets-list");layout->addWidget(list);
    auto* buttons=new QHBoxLayout;layout->addLayout(buttons);auto* check=new QPushButton("Check links"),*place=new QPushButton("Place selected"),*remove=new QPushButton("Delete unused"),*close=new QPushButton("Close");
    check->setObjectName("assets-check");place->setObjectName("assets-place");remove->setObjectName("assets-delete");buttons->addWidget(check);buttons->addWidget(place);buttons->addWidget(remove);buttons->addStretch();buttons->addWidget(close);
    const auto identity=host.session_id;auto revision=host.session.revision();
    const auto refresh=[&] {
        const auto selected=list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString{};list->clear();
        for(const auto& [id,asset]:host.session.document().raster_assets) {
            std::size_t count=0;for(const auto& [object_id,object]:host.session.document().objects){(void)object_id;if(object.image&&object.image->asset==id)++count;}
            auto* row=new QListWidgetItem(qs(asset.name)+QString(" · %1 × %2 · %3 · %4 · %5 placed").arg(asset.payload->width()).arg(asset.payload->height()).arg(qs(asset.mode),host.asset_status(id).value("state").toString()).arg(count),list);
            row->setData(Qt::UserRole,qs(id));row->setToolTip(qs(asset.locator));if(qs(id)==selected)list->setCurrentItem(row);
        }
        if(!list->currentItem()&&list->count())list->setCurrentRow(0);revision=host.session.revision();
    };
    const auto guard=[&]{if(host.session_id!=identity)throw Error("SESSION_CONFLICT","Asset list belongs to another document");if(host.session.revision()!=revision)throw Error("REVISION_CONFLICT","Close and reopen Image Assets to refresh changes");};
    connect(check,&QPushButton::clicked,&dialog,[&]{perform([&]{guard();for(const auto& [id,asset]:host.session.document().raster_assets){(void)asset;host.check_asset(id);}refresh();});});
    connect(place,&QPushButton::clicked,&dialog,[&]{perform([&]{guard();if(!list->currentItem())return;
        const auto asset_id=list->currentItem()->data(Qt::UserRole).toString().toStdString();const auto& asset=host.session.document().raster_assets.at(asset_id);
        const auto comp=canvas->active_composition();const auto board=evaluate_artboard(find_composition(host.session.document(),comp),canvas->active_artboard());const auto id=new_id();
        host.session.apply({CreateImage{comp,"",id,asset.name,{asset_id,{double(asset.payload->width())},{double(asset.payload->height())}}},Set{{id,"","transform.tx"},board.x},Set{{id,"","transform.ty"},board.y}},revision);
        canvas->set_selection(id);host.edited();refresh();
    });});
    connect(remove,&QPushButton::clicked,&dialog,[&]{perform([&]{guard();if(!list->currentItem())return;const auto id=list->currentItem()->data(Qt::UserRole).toString().toStdString();
        host.session.apply({DeleteRasterAsset{id}},revision);host.edited();refresh();
    });});
    connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);refresh();dialog.exec();
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
        // Return can deliver both combo activation and editingFinished.
        if(next==*found->second.text)return;
        host.session.apply({UpdateText{id,std::move(next)}},host.session.revision());host.edited();
    };
    if(!font_families_) {
        font_families_=new QStringListModel(this);
        auto reload=[this]{QStringList names;for(const auto& name:text_fonts())names<<qs(name);font_families_->setStringList(names);};
        reload();connect(qApp,&QGuiApplication::fontDatabaseChanged,this,[this,reload]{perform(reload);});
    }
    auto* family=new FontFamilyCombo(font_families_);family->setObjectName("text-family");
    family->addItem(qs(source.family));
    family->setCurrentText(qs(source.family));form->addRow("Font family",family);
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
    const auto id=new_id();host.session.apply({CreateText{comp.id,"",id,"Text "+std::to_string(host.session.document().objects.size()+1),source},CenterAnchor{id}},host.session.revision());
    canvas->set_selection(id);host.edited();canvas->setFocus();
}
void Window::add_stack(QVBoxLayout* layout,const Object& object) {
    auto* heading=new QWidget;
    auto* heading_layout=new QHBoxLayout(heading);heading_layout->setContentsMargins(0,4,0,0);
    heading_layout->addWidget(new QLabel("Shape stack"));heading_layout->addStretch();
    auto* add=new QPushButton("Add…");add->setObjectName("stack-add");
    auto* menu=new QMenu(add);
    for(const auto* name:{"add-fill","add-stroke","add-offset","add-repeater","add-radial-repeater"})
        if(auto* action=findChild<QAction*>(QString::fromLatin1(name)))menu->addAction(action);
    add->setMenu(menu);heading_layout->addWidget(add);layout->addWidget(heading);
    auto* order_hint=new QLabel("Earlier paints default above later paints. Path modifiers affect geometry before paint is applied. Repeater affects paths and paints before it; copies share their source points.");
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
        if(operation.type!="nect.shape.offset") {
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
        }
        if(operation.type=="nect.paint.fill"||operation.type=="nect.shape.offset") {
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
            form->addRow(color_tools_->menu_button(operation_ref(object.id,operation.id,"color")));
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
            if(operation.type=="nect.paint.stroke") {
                const auto stroke_session=host.session_id;
                const auto stroke_revision=host.session.revision();
                auto apply_style=[this,id=object.id,op=operation.id,stroke_session,stroke_revision](
                    const std::string& line_cap,const std::string& line_join,bool promote) {
                    if(host.session_id!=stroke_session)throw Error("SESSION_CONFLICT","The stroke belongs to another document");
                    if(host.session.revision()!=stroke_revision)throw Error("REVISION_CONFLICT","The stroke changed elsewhere; reopen the Inspector");
                    if(host.session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current gesture before changing stroke style");
                    const auto& current=find_operation(host.session.document(),id,op);
                    if(current.type!="nect.paint.stroke")throw Error("INVALID_DOMAIN","The selected operation is no longer a stroke");
                    const auto values=evaluate(host.session.document());
                    const auto miter=current.version==1?4.0:values.at(operation_ref(id,op,"miter_limit"));
                    if(!promote&&current.line_cap==line_cap&&current.line_join==line_join)return;
                    host.session.apply({StrokeStyle{id,op,line_cap,line_join,miter}},stroke_revision);
                    host.edited();
                };
                auto* cap=new QComboBox;
                cap->setObjectName("stroke-line-cap-"+qs(operation.id));cap->setAccessibleName("Line cap");
                cap->addItem("Butt","butt");cap->addItem("Round","round");cap->addItem("Square","square");
                {const QSignalBlocker blocker(cap);cap->setCurrentIndex(cap->findData(qs(operation.line_cap)));}
                form->addRow("Line cap",cap);
                const QPointer<QComboBox> safe_cap(cap);
                connect(cap,&QComboBox::currentIndexChanged,this,[this,safe_cap,cap,apply_style,id=object.id,op=operation.id,before=cap->currentIndex()](int) {
                    const auto selected=cap->currentData().toString().toStdString();bool applied=false;
                    perform([&]{const auto& current=find_operation(host.session.document(),id,op);
                        apply_style(selected,current.line_join,false);applied=true;});
                    if(!applied&&safe_cap){const QSignalBlocker blocker(safe_cap);safe_cap->setCurrentIndex(before);}
                });
                auto* join=new QComboBox;
                join->setObjectName("stroke-line-join-"+qs(operation.id));join->setAccessibleName("Line join");
                join->addItem("Miter","miter");join->addItem("Round","round");join->addItem("Bevel","bevel");
                {const QSignalBlocker blocker(join);join->setCurrentIndex(join->findData(qs(operation.line_join)));}
                form->addRow("Line join",join);
                const QPointer<QComboBox> safe_join(join);
                connect(join,&QComboBox::currentIndexChanged,this,[this,safe_join,join,apply_style,id=object.id,op=operation.id,before=join->currentIndex()](int) {
                    const auto selected=join->currentData().toString().toStdString();bool applied=false;
                    perform([&]{const auto& current=find_operation(host.session.document(),id,op);
                        apply_style(current.line_cap,selected,false);applied=true;});
                    if(!applied&&safe_join){const QSignalBlocker blocker(safe_join);safe_join->setCurrentIndex(before);}
                });
                if(operation.version>=2&&operation.parameters.contains("miter_limit")) {
                    add_property(form,operation_ref(object.id,operation.id,"miter_limit"),QStringLiteral("Miter limit"));
                } else {
                    auto* miter_row=new QWidget;auto* box=new QHBoxLayout(miter_row);box->setContentsMargins(0,0,0,0);
                    auto* value=new QLabel("4 (v1 default)");value->setObjectName("stroke-miter-limit-"+qs(operation.id));box->addWidget(value);
                    auto* enable=new QPushButton("Enable miter limit");enable->setObjectName("stroke-enable-miter-"+qs(operation.id));
                    enable->setToolTip("Promote this native stroke to v2 without changing its evaluated appearance");box->addWidget(enable);
                    form->addRow("Miter limit",miter_row);
                    connect(enable,&QPushButton::clicked,this,[this,apply_style]{perform([&]{apply_style("butt","miter",true);});});
                }
            }
        } else if(operation.type=="nect.shape.offset") {
            for(const auto* parameter:{"amount","miter_limit"})
                add_property(form,operation_ref(object.id,operation.id,parameter),parameter_label(parameter));
            auto* join=new QComboBox;join->setObjectName("operation-line-join-"+qs(operation.id));
            join->addItem("Miter","miter");join->addItem("Round","round");join->addItem("Bevel","bevel");
            join->setCurrentIndex(join->findData(qs(operation.line_join)));form->addRow("Line join",join);
            connect(join,&QComboBox::currentIndexChanged,this,[this,join,apply,id=object.id,op=operation.id,before=join->currentIndex()](int) {
                bool applied=false;
                perform([&]{const auto& current=find_operation(host.session.document(),id,op);
                    apply({OperationOptions{id,op,current.composite,current.fill_rule,join->currentData().toString().toStdString()}});applied=true;});
                if(!applied){const QSignalBlocker blocker(join);join->setCurrentIndex(before);}
            });
            auto* note=new QLabel("Positive Amount expands; negative contracts. Closed simple outlines only. Source points stay editable. Offset modifies geometry for all paints; move it across Repeater to change the result.");
            note->setWordWrap(true);note->setStyleSheet("color: #a4acb8; font-size: 11px;");form->addRow(note);
        } else if(operation.type=="nect.shape.repeater") {
            for(const auto* parameter:{"copies","position_x","position_y","anchor_x","anchor_y","rotation","scale_x","scale_y","offset","start_opacity","end_opacity"})
                if(operation.parameters.contains(parameter)&&std::string(parameter)!="rotation")
                    add_property(form,operation_ref(object.id,operation.id,parameter),parameter_label(parameter));
            if(operation.parameters.contains("rotation")) {
                const auto rotation_ref=operation_ref(object.id,operation.id,"rotation");
                add_property(form,rotation_ref,QStringLiteral("Rotation · degrees"));
                const auto reference=QJsonDocument(ref_json(rotation_ref)).toJson(QJsonDocument::Compact);
                QLineEdit* numeric=nullptr;
                for(auto* input:form->findChildren<QLineEdit*>())
                    if(input->property("nect-reference").toByteArray()==reference){numeric=input;break;}
                auto* dial_row=new QWidget;auto* dial_layout=new QHBoxLayout(dial_row);dial_layout->setContentsMargins(0,0,0,0);dial_layout->setSpacing(8);
                auto* knob=new RotationKnob(dial_row);knob->setObjectName("repeater-angle-knob-"+qs(operation.id));
                knob->setProperty("nect-reference",reference);
                const auto initial_rotation=inspector_values_.at(rotation_ref);knob->set_value(initial_rotation);
                const auto& scalar=nect::property(host.session.document(),rotation_ref);
                const bool driven=scalar.binding.has_value()||scalar.expression.has_value();
                knob->setEnabled(!driven);
                if(driven)knob->setToolTip("Rotation is driven by a binding or expression. Unlink it in the numeric editor before using the dial.");
                auto* dial_note=new QLabel("Dial · modulo 360",dial_row);dial_note->setAccessibleName("Dial shows rotation modulo 360; numeric value is exact");
                dial_layout->addWidget(knob);dial_layout->addWidget(dial_note);dial_layout->addStretch();form->addRow("Angle dial",dial_row);
                const auto rotation_session_id=host.session_id;const auto rotation_revision=host.session.revision();
                auto gesture_active=std::make_shared<bool>(false);auto last_valid=std::make_shared<double>(initial_rotation);
                auto report=[this](const std::exception& exception) {
                    if(const auto* error=dynamic_cast<const Error*>(&exception))statusBar()->showMessage(qs(error->code)+": "+QString::fromUtf8(error->what()),12000);
                    else statusBar()->showMessage(QString::fromUtf8(exception.what()),12000);
                };
                knob->begin_drag=[this,rotation_ref,object_id=object.id,operation_id=operation.id,knob,numeric,rotation_session_id,rotation_revision,gesture_active,driven,report] {
                    try {
                        if(driven)throw Error("DRIVEN_PROPERTY","Unlink the Repeater rotation before using the dial");
                        if(host.session_id!=rotation_session_id)throw Error("SESSION_CONFLICT","Repeater rotation belongs to another document");
                        if(host.session.revision()!=rotation_revision)throw Error("REVISION_CONFLICT","Repeater rotation changed; reopen the Inspector");
                        const auto& current=find_operation(host.session.document(),object_id,operation_id);
                        if(current.type!="nect.shape.repeater"||!current.parameters.contains("rotation"))
                            throw Error("MISSING_PROPERTY","Repeater rotation Ref is no longer available");
                        const auto& current_scalar=nect::property(host.session.document(),rotation_ref);
                        if(current_scalar.binding||current_scalar.expression)throw Error("DRIVEN_PROPERTY","Unlink the Repeater rotation before using the dial");
                        host.session.begin_gesture(rotation_revision);*gesture_active=true;return true;
                    } catch(const std::exception& exception) {
                        if(numeric)numeric->setFocus(Qt::OtherFocusReason);report(exception);
                        knob->set_value(inspector_values_.contains(rotation_ref)?inspector_values_.at(rotation_ref):knob->value());return false;
                    }
                };
                knob->preview_value=[this,knob,numeric,rotation_ref,rotation_session_id,rotation_revision,gesture_active,last_valid,report](double value) {
                    if(!*gesture_active)return;
                    if(host.session_id!=rotation_session_id||host.session.revision()!=rotation_revision) {
                        if(host.session_id==rotation_session_id&&host.session.gesture_active())host.session.cancel_gesture();
                        *gesture_active=false;canvas->refresh();canvas->update();knob->set_value(*last_valid);
                        statusBar()->showMessage("REVISION_CONFLICT: Repeater angle drag became stale and was canceled",12000);return;
                    }
                    try {
                        host.session.update_gesture({EditProperties{{rotation_ref},value,false}});
                        *last_valid=value;canvas->refresh();canvas->update();
                        if(numeric){numeric->setText(QString::number(value,'g',17));numeric->setModified(false);}
                    } catch(const std::exception& exception) {
                        if(const auto* error=dynamic_cast<const Error*>(&exception);error&&error->code=="OUT_OF_RANGE") {
                            knob->set_value(*last_valid);if(numeric)numeric->setText(QString::number(*last_valid,'g',17));report(exception);return;
                        }
                        if(host.session_id==rotation_session_id&&host.session.gesture_active())host.session.cancel_gesture();
                        *gesture_active=false;canvas->refresh();canvas->update();knob->set_value(*last_valid);
                        if(numeric)numeric->setText(QString::number(*last_valid,'g',17));report(exception);
                    }
                };
                knob->commit_drag=[this,knob,rotation_session_id,rotation_revision,gesture_active,last_valid,report] {
                    if(!*gesture_active)return;
                    if(host.session_id!=rotation_session_id||host.session.revision()!=rotation_revision||!host.session.gesture_active()) {
                        if(host.session_id==rotation_session_id&&host.session.gesture_active())host.session.cancel_gesture();
                        *gesture_active=false;canvas->refresh();canvas->update();knob->set_value(*last_valid);
                        statusBar()->showMessage("REVISION_CONFLICT: Repeater angle drag became stale and was canceled",12000);return;
                    }
                    try {host.session.commit_gesture();*gesture_active=false;host.edited();}
                    catch(const std::exception& exception) {
                        if(host.session.gesture_active())host.session.cancel_gesture();*gesture_active=false;
                        canvas->refresh();canvas->update();knob->set_value(*last_valid);report(exception);
                    }
                };
                knob->cancel_drag=[this,knob,numeric,rotation_session_id,gesture_active,initial_rotation] {
                    if(host.session_id==rotation_session_id&&host.session.gesture_active())host.session.cancel_gesture();
                    *gesture_active=false;canvas->refresh();canvas->update();knob->set_value(initial_rotation);
                    if(numeric){numeric->setText(QString::number(initial_rotation,'g',17));numeric->setModified(false);}
                };
            }
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
                    for(const auto& contour:path_contours(host.session.document().objects.at(id),&values))for(const auto& point:contour.points) {
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
        stop_form->addRow(color_tools_->menu_button(ref("color")));
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
    add_properties(layout,{ref},label);
}

void Window::transform_selection() {
    if(canvas->selections().empty()||std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return !item.point.empty();}))
        throw Error("INVALID_SELECTION","Select whole objects or Groups to rotate or scale together");
    canvas->cancel_interaction();
    const auto objects=canvas->selected_objects();const auto revision=host.session.revision();const auto identity=host.session_id;
    QDialog dialog(this);dialog.setObjectName("selection-transform-dialog");dialog.setWindowTitle("Rotate / scale selection");dialog.resize(460,380);
    auto* layout=new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QString("Transform %1 selected object(s) together").arg(objects.size()),&dialog));
    auto* form=new QFormLayout;layout->addLayout(form);
    auto number=[&](const char* name,const QString& label,double initial,double limit,const QString& suffix) {
        auto* input=new QDoubleSpinBox(&dialog);input->setObjectName(name);input->setAccessibleName(label);
        input->setDecimals(3);input->setRange(-limit,limit);input->setValue(initial);input->setSuffix(suffix);form->addRow(label,input);return input;
    };
    auto* rotation=number("selection-rotation","Rotation clockwise",0,1e9,"°");
    auto* sx=number("selection-scale-x","Scale X",100,1e6," %");
    auto* sy=number("selection-scale-y","Scale Y",100,1e6," %");
    auto* linked=new QCheckBox("Keep X and Y scale equal",&dialog);linked->setObjectName("selection-scale-linked");linked->setChecked(true);form->addRow(linked);
    connect(sx,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[=](double value){if(linked->isChecked()){const QSignalBlocker block(sy);sy->setValue(value);}});
    connect(sy,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[=](double value){if(linked->isChecked()){const QSignalBlocker block(sx);sx->setValue(value);}});
    connect(linked,&QCheckBox::toggled,&dialog,[=](bool enabled){if(enabled)sy->setValue(sx->value());});
    auto* mirrors=new QWidget(&dialog);auto* mirror_layout=new QHBoxLayout(mirrors);mirror_layout->setContentsMargins(0,0,0,0);
    for(const bool horizontal:{true,false}) {
        auto* button=new QPushButton(horizontal?"Flip X":"Flip Y",mirrors);button->setObjectName(horizontal?"selection-flip-x":"selection-flip-y");mirror_layout->addWidget(button);
        connect(button,&QPushButton::clicked,&dialog,[=]{linked->setChecked(false);auto* axis=horizontal?sx:sy;axis->setValue(-axis->value());});
    }
    form->addRow("Reflect",mirrors);
    auto* pivot=new QComboBox(&dialog);pivot->setObjectName("selection-pivot-mode");pivot->addItems({"Selection center · geometric bounds","Custom canvas coordinates"});form->addRow("Shared pivot",pivot);
    auto* px=number("selection-pivot-x","Pivot X",0,1e9,"");auto* py=number("selection-pivot-y","Pivot Y",0,1e9,"");px->setEnabled(false);py->setEnabled(false);
    connect(pivot,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[=](int index){px->setEnabled(index==1);py->setEnabled(index==1);});
    auto* hint=new QLabel("Scale along canvas X/Y, then rotate about one shared pivot.\nSelection center excludes stroke width. Authored Anchors stay unchanged.\n0% collapses an axis; negative scale reflects it.",&dialog);hint->setWordWrap(true);layout->addWidget(hint);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Ok)->setText("Apply");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(identity!=host.session_id)throw Error("SESSION_CONFLICT","Document changed while selection transform was open");
    if(revision!=host.session.revision())throw Error("REVISION_CONFLICT","Artwork changed while selection transform was open; reopen it");
    if(std::remainder(rotation->value(),360.0)==0&&sx->value()==100&&sy->value()==100)return;
    std::optional<std::array<double,2>> center;
    if(pivot->currentIndex()==1)center=std::array<double,2>{px->value(),py->value()};
    host.session.apply({TransformObjects{objects,rotation->value(),sx->value()/100,sy->value()/100,center}},revision);
    host.edited();canvas->setFocus();
}

void Window::distribute_selection(const std::string& axis,const std::string& reference,std::optional<double> spacing) {
    if(std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& selection){return !selection.point.empty();}))
        throw Error("INVALID_SELECTION","Select whole objects to distribute their bounds");
    const auto revision=host.session.revision();
    host.session.apply({DistributeObjects{canvas->selected_objects(),axis,reference,spacing}},revision);
    if(host.session.revision()!=revision)host.edited();
}

void Window::align_selection(const std::string& axis,const std::string& alignment,const std::string& reference) {
    if(std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& selection){return !selection.point.empty();}))
        throw Error("INVALID_SELECTION","Select whole objects to align their bounds");
    const auto revision=host.session.revision();
    host.session.apply({AlignObjects{canvas->selected_objects(),axis,alignment,{},reference}},revision);
    if(host.session.revision()!=revision)host.edited();
}

void Window::add_alignment_controls(QVBoxLayout* layout,const std::vector<Canvas::Selection>& selected) {
    const auto& d=host.session.document();
    auto* alignment_box=new QGroupBox("Align · geometric bounds");auto* alignment_layout=new QVBoxLayout(alignment_box);
    auto* alignment_target=new QComboBox;alignment_target->setObjectName("alignment-target");
    alignment_target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    alignment_target->setMinimumContentsLength(16);
    alignment_target->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    alignment_target->addItem("Selection bounds","selection");
    const auto& active_composition=find_composition(d,canvas->active_composition());
    const auto active_artboard=canvas->active_artboard();
    for(const auto& board:active_composition.artboards)if(board.id==active_artboard)
        alignment_target->addItem(QString("Artboard: %1 (%2)").arg(qs(board.name),qs(board.id)),qs("artboard:"+board.id));
    for(const auto& board:active_composition.artboards)if(board.id!=active_artboard)
        alignment_target->addItem(QString("Artboard: %1 (%2)").arg(qs(board.name),qs(board.id)),qs("artboard:"+board.id));
    const auto selected_objects=canvas->selected_objects();
    for(const auto& id:selected_objects) {
        const auto& object=d.objects.at(id);
        alignment_target->addItem(QString("Key object: %1 (%2)").arg(qs(object.name),qs(id)),qs("key_object:"+id));
    }
    for(const auto& board:active_composition.artboards)if(board.layout&&board.layout->grid)
        alignment_target->addItem(QString("Grid: %1 · Artboard: %2 (%3)")
            .arg(qs(board.layout->grid->id),qs(board.name),qs(board.id)),qs("grid:"+board.layout->grid->id));
    for(const auto& guide:active_composition.guides)
        alignment_target->addItem(QString("Guide: %1 (%2) · %3=%4")
            .arg(qs(guide.name),qs(guide.id),qs(guide.axis),QString::number(guide.position,'g',15)),qs("guide:"+guide.id));
    for(int index=0;index<alignment_target->count();++index)
        alignment_target->setItemData(index,alignment_target->itemText(index),Qt::ToolTipRole);
    auto target_index=alignment_target->findData(qs(alignment_reference_));
    if(target_index<0){alignment_reference_="selection";target_index=0;}
    alignment_target->setCurrentIndex(target_index);
    alignment_target->setToolTip(alignment_target->itemText(target_index));
    alignment_layout->addWidget(alignment_target);
    connect(alignment_target,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,alignment_target](int index){
        alignment_reference_=alignment_target->itemData(index).toString().toStdString();
        alignment_target->setToolTip(alignment_target->itemText(index));
    });
    auto* spacing_row=new QVBoxLayout;alignment_layout->addLayout(spacing_row);
    spacing_row->addWidget(new QLabel("Key spacing (du):"));
    auto* spacing_input=new QLineEdit;spacing_input->setObjectName("distribution-spacing");
    spacing_input->setPlaceholderText("Enter explicit gap");
    spacing_input->setToolTip("Explicit nonnegative spacing in Composition du. Text entry is a draft; a Distribute button applies it.");
    spacing_row->addWidget(spacing_input);
    const bool selected_has_points=std::any_of(selected.begin(),selected.end(),[](const auto& item){return !item.point.empty();});
    const bool selected_text_only=!selected_objects.empty()&&std::all_of(selected_objects.begin(),selected_objects.end(),[&](const auto& id){return d.objects.at(id).kind==Kind::text&&d.objects.at(id).text.has_value();});
    const auto apply_enabled=[alignment_target,spacing_input,selected_objects,selected_has_points,selected_text_only]() {
        const auto reference=alignment_target->currentData().toString().toStdString();
        const bool selection_or_key=reference=="selection"||reference.starts_with("key_object:");
        const bool key=reference.starts_with("key_object:");
        bool spacing_valid=false;bool spacing_ok=false;
        const auto spacing_value=spacing_input->text().trimmed().toDouble(&spacing_ok);
        spacing_valid=spacing_ok&&std::isfinite(spacing_value)&&spacing_value>=0;
        const bool count_for_align=selected_objects.size()>=(selection_or_key?2u:1u);
        const bool count_for_distribute=selected_objects.size()>=(reference=="selection"?3u:key?2u:1u);
        return std::tuple{!selected_has_points&&count_for_align,
            !selected_has_points&&count_for_align&&selection_or_key&&selected_text_only,
            !selected_has_points&&count_for_distribute&&!reference.starts_with("guide:")&&(!key||spacing_valid),
            spacing_valid};
    };
    auto* spacing_hint=new QLabel("Reference is explicit in the selector; disabled actions explain unsupported axis or selection combinations.");
    spacing_hint->setWordWrap(true);alignment_layout->addWidget(spacing_hint);
    for(const auto axis:{"x","y"}) {
        auto* row=new QHBoxLayout;alignment_layout->addLayout(row);
        for(int index=0;index<3;++index) {
            const std::string mode=index==0?"min":index==1?"center":"max";
            const auto label=std::string(axis)=="x"?(index==0?"Left":index==1?"H center":"Right"):(index==0?"Top":index==1?"V center":"Bottom");
            auto* button=new QPushButton(label);button->setObjectName(QString("quick-align-%1-%2").arg(axis,qs(mode)));
            button->setToolTip("Align evaluated geometric bounds in Composition du; excludes stroke width.");row->addWidget(button);
            connect(button,&QPushButton::clicked,this,[this,axis,mode,alignment_target]{
                const auto reference=alignment_target->currentData().toString().toStdString();
                perform([&]{align_selection(axis,mode,reference);});
            });
        }
    }
    auto* baseline_button=new QPushButton("Align first-line baseline");
    baseline_button->setObjectName("quick-align-y-baseline");
    baseline_button->setToolTip("Align actual first-line baselines for horizontal, axis-aligned Text. Selection keeps the source with minimum Composition y fixed; a key-object reference keeps that Text fixed.");
    connect(baseline_button,&QPushButton::clicked,this,[this,alignment_target]{
        const auto reference=alignment_target->currentData().toString().toStdString();
        perform([&]{align_selection("y","baseline",reference);});
    });alignment_layout->addWidget(baseline_button);
    auto* distribute_row=new QHBoxLayout;alignment_layout->addLayout(distribute_row);
    for(const auto axis:{"x","y"}) {
        auto* button=new QPushButton(std::string(axis)=="x"?"Distribute H":"Distribute V");button->setObjectName(QString("quick-distribute-%1").arg(axis));
        button->setToolTip("Apply equal gaps for the selected reference. Key-object distribution requires explicit nonnegative spacing; Guide distribution is unsupported.");distribute_row->addWidget(button);
        connect(button,&QPushButton::clicked,this,[this,axis,alignment_target,spacing_input]{
            perform([&]{
                const auto reference=alignment_target->currentData().toString().toStdString();std::optional<double> spacing;
                if(reference.starts_with("key_object:")) {
                    bool ok=false;const auto value=spacing_input->text().trimmed().toDouble(&ok);
                    if(!ok||!std::isfinite(value))throw Error("INVALID_SPACING","Enter a finite explicit spacing in Composition du");
                    spacing=value;
                }
                distribute_selection(axis,reference,spacing);
            });
        });
    }
    const auto active_composition_id=canvas->active_composition();
    const auto update_alignment_actions=[this,alignment_box,alignment_target,spacing_input,spacing_hint,apply_enabled,active_composition_id] {
        const auto [ordinary,baseline_ok,distribution,spacing_valid]=apply_enabled();
        const auto reference=alignment_target->currentData().toString().toStdString();
        const auto& composition=find_composition(host.session.document(),active_composition_id);
        const auto guide=reference.starts_with("guide:")?std::find_if(composition.guides.begin(),composition.guides.end(),[&](const auto& item){return "guide:"+item.id==reference;}):composition.guides.end();
        for(const auto axis:{"x","y"})for(const auto mode:{"min","center","max"})
            if(auto* button=alignment_box->findChild<QPushButton*>(QString("quick-align-%1-%2").arg(axis,mode)))
                button->setEnabled(ordinary&&(!reference.starts_with("guide:")||(guide!=composition.guides.end()&&guide->axis==axis)));
        if(auto* button=alignment_box->findChild<QPushButton*>("quick-align-y-baseline"))button->setEnabled(baseline_ok);
        for(const auto axis:{"x","y"})if(auto* button=alignment_box->findChild<QPushButton*>(QString("quick-distribute-%1").arg(axis)))button->setEnabled(distribution);
        spacing_input->setEnabled(reference.starts_with("key_object:"));
        spacing_hint->setText(reference.starts_with("key_object:")?
            (spacing_valid?"Key object is fixed; explicit spacing is ready in Composition du.":"Enter finite, nonnegative spacing to enable Distribute."):
            reference.starts_with("guide:")?"Guide can align only on its axis; Guide distribution is unsupported.":
            "Selection, key object, Artboard, and Grid references are explicit; input edits apply only after a button click.");
    };
    connect(alignment_target,qOverload<int>(&QComboBox::currentIndexChanged),alignment_box,[update_alignment_actions](int){update_alignment_actions();});
    connect(spacing_input,&QLineEdit::textChanged,alignment_box,[update_alignment_actions](const QString&){update_alignment_actions();});
    update_alignment_actions();
    layout->addWidget(alignment_box);
}

void Window::add_multi_properties(QVBoxLayout* layout) {
    const auto& d=host.session.document();const auto selected=canvas->selections();
    auto* heading=new QLabel(QString::number(selected.size())+(canvas->selected_point.empty()?" objects selected":" points selected"));
    heading->setObjectName("selection-summary");layout->addWidget(heading);
    auto* note=new QLabel("Mixed values are blank. Enter a value to set every target; += / -= keeps their differences. Coordinates are local to each target.");
    note->setWordWrap(true);layout->addWidget(note);
    auto section=[&](const QString& title){auto* box=new QGroupBox(title);auto* form=new QFormLayout(box);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);return form;};
    auto common=[&](QFormLayout* form,const std::string& field,const QString& label){
        std::vector<Ref> refs;for(const auto& item:selected)refs.push_back({item.object,item.point,field});add_properties(form,refs,label);};
    if(!canvas->selected_point.empty()) {
        auto* form=section("Points && handles");
        for(const auto* field:{"x","y","in.angle","in.length","out.angle","out.length"})common(form,field,QString::fromLatin1(field));
        layout->addStretch();return;
    }
    add_alignment_controls(layout,selected);
    auto* selection_transform=new QPushButton("Rotate / scale selection…");selection_transform->setObjectName("selection-transform-open");
    layout->addWidget(selection_transform);connect(selection_transform,&QPushButton::clicked,this,[this]{perform([&]{transform_selection();});});
    auto* transform=section("Transform · each object");
    common(transform,"composite.opacity","Object opacity");
    common(transform,"transform.tx","Translation X");common(transform,"transform.ty","Translation Y");
    common(transform,"transform.anchor_x","Anchor X");common(transform,"transform.anchor_y","Anchor Y");
    auto* matrix_toggle=new QPushButton("Affine matrix…");matrix_toggle->setObjectName("batch-matrix-toggle");matrix_toggle->setCheckable(true);matrix_toggle->setChecked(matrix_expanded_);transform->addRow(matrix_toggle);
    auto* matrix_box=new QWidget;auto* matrix_form=new QFormLayout(matrix_box);matrix_form->setContentsMargins(0,0,0,0);matrix_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    for(const auto* field:{"a","b","c","d"})common(matrix_form,std::string("transform.")+field,QString("Matrix ")+field);
    transform->addRow(matrix_box);matrix_box->setVisible(matrix_expanded_);
    connect(matrix_toggle,&QPushButton::toggled,this,[this,matrix_box](bool shown){matrix_expanded_=shown;matrix_box->setVisible(shown);});
    const auto& first=d.objects.at(selected.front().object);
    for(const bool text:{false,true}) {
        const bool compatible=std::all_of(selected.begin(),selected.end(),[&](const auto& item){const auto& o=d.objects.at(item.object);return text?o.text.has_value():o.source.has_value();});
        if(!compatible)continue;
        const auto& parameters=text?first.text->parameters:first.source->parameters;
        auto* form=section(text?"Common Text parameters":"Common source parameters");
        for(const auto& [name,scalar]:parameters) {
            (void)scalar;
            if(std::all_of(selected.begin(),selected.end(),[&](const auto& item){const auto& o=d.objects.at(item.object);return (text?o.text->parameters:o.source->parameters).contains(name);}))
                common(form,(text?"text.":"generator.")+name,parameter_label(name));
        }
    }
    for(std::size_t slot=0;slot<first.stack.size();++slot) {
        const auto& operation=first.stack[slot];
        if(!std::all_of(selected.begin(),selected.end(),[&](const auto& item){const auto& stack=d.objects.at(item.object).stack;
            return stack.size()>slot&&stack[slot].type==operation.type;}))continue;
        auto* form=section("Stack "+QString::number(slot+1)+" · "+operation_label(operation));
        for(const auto& [name,scalar]:operation.parameters) {
            (void)scalar;std::vector<Ref> refs;
            for(const auto& item:selected)refs.push_back({item.object,"","op."+d.objects.at(item.object).stack[slot].id+"."+name});
            add_properties(form,refs,parameter_label(name));
        }
    }
    auto* hint=new QLabel("↗ freezes all these targets while you choose a source. Paint rows match the same operation type at the same stack position.");
    hint->setWordWrap(true);layout->addWidget(hint);layout->addStretch();
}

void Window::add_expression_editor(QVBoxLayout* layout,const QByteArray& key,const std::vector<Ref>& targets,const QString& label) {
    const auto draft=expression_drafts_.at(key);
    auto* panel=new QWidget;panel->setObjectName("nect-expression-panel");auto* column=new QVBoxLayout(panel);
    column->setContentsMargins(0,3,0,3);column->setSpacing(4);layout->addWidget(panel);
    auto* editor=new ExpressionInput;editor->setPlainText(draft.source);editor->setAccessibleName(label+" expression");
    editor->setProperty("nect-targets",key);editor->setProperty("nect-reference",QJsonDocument(ref_json(targets.front())).toJson(QJsonDocument::Compact));
    editor->setTabChangesFocus(true);editor->setMinimumHeight(82);editor->setMaximumHeight(180);column->addWidget(editor);
    auto* result=new QLabel("Draft · canvas keeps the committed result");result->setWordWrap(true);result->setObjectName("nect-expression-result");column->addWidget(result);
    auto* replace=new QCheckBox("Replace existing link");replace->setChecked(draft.replace_binding);
    replace->setVisible(std::any_of(targets.begin(),targets.end(),[&](const Ref& ref){return property_origin(host.session.document(),ref)!="generated"&&nect::property(host.session.document(),ref).binding.has_value();}));column->addWidget(replace);
    auto* actions=new QHBoxLayout;auto* insert=new QPushButton("Insert reference…");auto* apply=new QPushButton("Apply");auto* cancel=new QPushButton("Cancel");
    apply->setToolTip("Apply expression · Ctrl+Enter");cancel->setToolTip("Discard draft · Esc");actions->addWidget(insert);actions->addStretch();actions->addWidget(apply);actions->addWidget(cancel);column->addLayout(actions);
    auto* timer=new QTimer(editor);timer->setSingleShot(true);timer->setInterval(250);
    const auto preview=[this,key,targets,result] {
        const auto found=expression_drafts_.find(key);if(found==expression_drafts_.end())return;
        const auto& current=found->second;
        try {
            if(host.session_id!=current.session||host.session.revision()!=current.revision)
                throw Error("DRAFT_CONFLICT","Document changed. Cancel and reopen the draft against the current values.");
            Session preview(host.session.document());preview.apply({SetExpression{targets,{current.source.toStdString(),1},current.replace_binding}},preview.revision());
            const auto values=evaluate(preview.document());const auto first=values.at(targets.front());
            const bool mixed=std::any_of(targets.begin(),targets.end(),[&](const auto& r){return values.at(r)!=first;});
            result->setText("Draft result: "+(mixed?QString("Mixed"):display_value(first))+" · not applied");result->setStyleSheet("color: #84d5eb;");
        } catch(const std::exception& e) {result->setText(QString::fromUtf8(e.what())+"\nCommitted result is unchanged.");result->setStyleSheet("color: #e4be82;");}
    };
    connect(timer,&QTimer::timeout,editor,preview);
    connect(editor,&QPlainTextEdit::textChanged,this,[this,key,editor,timer]{
        if(auto it=expression_drafts_.find(key);it!=expression_drafts_.end()){it->second.source=editor->toPlainText();timer->start();}
    });
    connect(replace,&QCheckBox::toggled,this,[this,key,timer](bool enabled){if(auto it=expression_drafts_.find(key);it!=expression_drafts_.end()){it->second.replace_binding=enabled;timer->start();}});
    auto commit=[this,key,targets,result] {
        const auto found=expression_drafts_.find(key);if(found==expression_drafts_.end())return;
        const auto current=found->second;
        try {
            if(host.session_id!=current.session)throw Error("SESSION_CONFLICT","Expression belongs to another document");
            canvas->cancel_interaction();host.session.apply({SetExpression{targets,{current.source.toStdString(),1},current.replace_binding}},current.revision);
            expression_drafts_.erase(key);host.edited();
        } catch(const std::exception& e){result->setText(QString::fromUtf8(e.what())+"\nCommitted result is unchanged.");result->setStyleSheet("color: #e4be82;");}
    };
    auto discard=[this,key]{expression_drafts_.erase(key);rebuild_inspector();};
    editor->apply=commit;editor->cancel=discard;connect(apply,&QPushButton::clicked,this,commit);connect(cancel,&QPushButton::clicked,this,discard);
    connect(insert,&QPushButton::clicked,this,[this,editor] {
        auto* dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("Insert expression reference");dialog->resize(660,450);
        auto* content=new QVBoxLayout(dialog);auto* search=new QLineEdit;search->setPlaceholderText("Search properties");content->addWidget(search);auto* list=new QListWidget;content->addWidget(list);
        for(const auto& ref:properties(host.session.document())) {auto* item=new QListWidgetItem(property_label(host.session.document(),ref),list);item->setData(Qt::UserRole,expression_ref(ref));}
        connect(search,&QLineEdit::textChanged,dialog,[list](const QString& text){const auto terms=text.split(' ',Qt::SkipEmptyParts);for(int i=0;i<list->count();++i)list->item(i)->setHidden(!std::all_of(terms.begin(),terms.end(),[&](const auto& term){return list->item(i)->text().contains(term,Qt::CaseInsensitive);}));});
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);content->addWidget(buttons);
        const QPointer<ExpressionInput> safe_editor(editor);
        auto accept=[safe_editor,list,dialog]{if(safe_editor&&list->currentItem()){safe_editor->insertPlainText(list->currentItem()->data(Qt::UserRole).toString());safe_editor->setFocus();dialog->accept();}};
        connect(buttons,&QDialogButtonBox::accepted,dialog,accept);connect(list,&QListWidget::itemDoubleClicked,dialog,[accept](QListWidgetItem*){accept();});connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);dialog->show();search->setFocus();
    });
    timer->start();editor->setFocus();
}
void Window::add_properties(QFormLayout* layout,const std::vector<Ref>& targets,const QString& label) {
    const auto& ref=targets.front();
    const auto& d=host.session.document();
    const auto origin=property_origin(d,ref);
    const auto evaluated=inspector_values_.at(ref);
    // Generated fallback has no authored Scalar to inspect. Reuse this panel's
    // evaluation snapshot instead of asking property() to evaluate it again.
    const bool mixed=std::any_of(targets.begin(),targets.end(),[&](const auto& target){return inspector_values_.at(target)!=evaluated;});
    const bool driven=std::any_of(targets.begin(),targets.end(),[&](const auto& target){if(property_origin(d,target)=="generated")return false;const auto& s=nect::property(d,target);return s.binding.has_value()||s.expression.has_value();});
    const auto formula=origin=="generated"?std::optional<Expression>{}:nect::property(d,ref).expression;
    auto* row=new QWidget;auto* column=new QVBoxLayout(row);column->setContentsMargins(0,0,0,0);column->setSpacing(0);
    auto* box=new QHBoxLayout;column->addLayout(box);box->setContentsMargins(0,0,0,0);box->setSpacing(4);
    auto* input=new PropertyInput(mixed?QString{}:display_value(evaluated));input->setAccessibleName(label);
    input->setPlaceholderText(mixed?"Mixed":QString{});input->setProperty("nect-mixed",mixed);
    const auto reference=QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact);
    input->setProperty("nect-reference",reference);
    const auto target_data=refs_json(targets);input->setProperty("nect-targets",target_data);
    input->setProperty("nect-property-origin",qs(origin));
    input->setToolTip(qs(property_unit(ref))+" · local · "+qs(ref.object+"/"+ref.point+"/"+ref.field));
    if(origin=="generated")input->setToolTip(input->toolTip()+"\nGenerated by the source. Editing creates a Point Edit override and keeps the source.");
    else if(origin=="point_edit") {
        input->setStyleSheet("color: #e4be82;");
        input->setToolTip(input->toolTip()+"\nPoint Edit: absolute local override; the source remains editable.");
    } else if(origin=="bypassed_point_edit") {
        input->setToolTip(input->toolTip()+"\nPoint Edit is bypassed. This value follows the source; editing enables the correction again.");
    }
    if(driven) {
        input->setStyleSheet("color: #84d5eb;");
        input->setToolTip(input->toolTip()+(origin=="bypassed_point_edit"
            ? "\nStored Point Edit source is bypassed. Unlink explicitly before replacing its value."
            : "\nDriven; unlink explicitly before replacing its value."));
    }
    if(formula)input->setToolTip(input->toolTip()+"\nExpression: "+qs(formula->source)+"\nDisplayed number is the evaluated result.");
    input->setToolTip(input->toolTip()+"\nEnter =expression or use fx. += / -= makes a one-time relative edit.");
    box->addWidget(input);
    auto* fx=new QPushButton("fx");fx->setFixedWidth(26);fx->setAccessibleName(label+" expression editor");fx->setToolTip("Edit expression · =prefix · multiline draft");box->addWidget(fx);
    if(formula)fx->setStyleSheet("color: #84d5eb;");
    auto* pick=new QPushButton("↗");pick->setFixedWidth(28);pick->setToolTip("Pick property source");box->addWidget(pick);
    pick->setProperty("nect-pick-whip",true);pick->setProperty("nect-reference",reference);
    pick->setProperty("nect-targets",target_data);
    pick->setToolTip("Drag to a source field; hover Objects to inspect another source. Click to search.");
    layout->addRow(label,row);
    connect(pick,&QPushButton::clicked,this,[this,targets]{pick_source(targets);});
    const auto field_session=host.session_id;
    const auto field_revision=host.session.revision();
    auto expand=[this,column,row,input,targets,target_data,label,field_session,field_revision](QString source) {
        if(host.session_id!=field_session)return;
        if(source.startsWith('='))source.remove(0,1);
        input->setModified(false);
        if(!expression_drafts_.contains(target_data))expression_drafts_.emplace(target_data,ExpressionDraft{field_session,source,field_revision,false});
        if(!row->findChild<QWidget*>("nect-expression-panel"))add_expression_editor(column,target_data,targets,label);
    };
    const auto initial_expression=formula?qs(formula->source):(mixed?QString{}:QString::number(evaluated,'g',17));
    connect(fx,&QPushButton::clicked,this,[expand,initial_expression]{expand(initial_expression);});
    input->multiline=expand;
    if(expression_drafts_.contains(target_data))expand(expression_drafts_.at(target_data).source);
    connect(input,&QLineEdit::editingFinished,this,[this,input,ref,targets,target_data,field_session,field_revision,expand] {
        if(!input->isModified()) return;
        input->setModified(false);
        const bool keep_focus=input->hasFocus();const auto scroll=inspector_scroll_->verticalScrollBar()->value();
        const auto frozen_session=host.session_id;
        perform([&]{
            if(host.session_id!=field_session)throw Error("SESSION_CONFLICT","These properties belong to another document");
            if(host.session.revision()!=field_revision)throw Error("REVISION_CONFLICT","These properties changed elsewhere; reopen the Inspector");
            auto text=input->text().trimmed();bool valid=false;const bool relative=text.startsWith("+=")||text.startsWith("-=");
            if(text.startsWith('=')) {
                try {canvas->cancel_interaction();host.session.apply({SetExpression{targets,{text.mid(1).toStdString(),1},false}},field_revision);host.edited();}
                catch(const Error& e){if(e.code=="REVISION_CONFLICT"||e.code=="SESSION_CONFLICT")throw;expand(text);input->setText(display_value(inspector_values_.at(ref)));}
                return;
            }
            auto value=(relative?text.mid(2):text).toDouble(&valid);if(relative&&text.startsWith("-="))value=-value;
            if(!valid||!std::isfinite(value)) throw Error("INVALID_VALUE","Enter a number, += / -= adjustment, or =expression");
            canvas->cancel_interaction();host.session.apply({EditProperties{targets,value,relative}},field_revision);host.edited();
            if(keep_focus)QTimer::singleShot(0,this,[this,target_data,scroll,frozen_session]{
                if(host.session_id!=frozen_session)return;
                for(auto* current:inspector_->findChildren<QLineEdit*>())if(current->isVisible()&&current->property("nect-targets").toByteArray()==target_data) {
                    current->setFocus(Qt::OtherFocusReason);current->selectAll();
                    inspector_scroll_->verticalScrollBar()->setValue(scroll);break;
                }
            });
        });
    });
    input->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(input,&QWidget::customContextMenuRequested,this,[this,input,ref,targets,mixed,driven,field_session,field_revision,expand,initial_expression](const QPoint& point) {
        QMenu menu;
        auto* copy=menu.addAction("Copy Value");auto* reference=menu.addAction("Copy Reference");
        auto* paste=menu.addAction("Paste Value");auto* link=menu.addAction("Paste Link");
        auto* relative=menu.addAction("Pick Relative Link…");auto* unlink=menu.addAction("Unlink · keep evaluated value");
        menu.addSeparator();auto* expression=menu.addAction("Edit expression…");auto* paste_expression=menu.addAction("Paste expression…");
        copy->setEnabled(!mixed);reference->setEnabled(targets.size()==1);unlink->setEnabled(driven);
        auto* chosen=menu.exec(input->mapToGlobal(point));if(!chosen)return;
        perform([&] {
            if(host.session_id!=field_session)throw Error("SESSION_CONFLICT","These properties belong to another document");
            if(host.session.revision()!=field_revision)throw Error("REVISION_CONFLICT","These properties changed elsewhere; reopen the Inspector");
            if(chosen==expression)expand(initial_expression);
            else if(chosen==paste_expression)expand(QApplication::clipboard()->text());
            else if(chosen==copy) QApplication::clipboard()->setText(QString::number(evaluate(host.session.document()).at(ref),'g',17));
            else if(chosen==reference) {
                auto* mime=new QMimeData;const auto data=QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact);
                mime->setData(reference_mime,data);mime->setText(QString::fromUtf8(data));QApplication::clipboard()->setMimeData(mime);
            } else if(chosen==paste) {
                bool valid=false;const auto value=QApplication::clipboard()->text().toDouble(&valid);
                if(!valid)throw Error("INVALID_VALUE","Clipboard is not a numeric value");
                host.session.apply({EditProperties{targets,value,false}},field_revision);host.edited();
            } else if(chosen==link) {
                const auto* mime=QApplication::clipboard()->mimeData();
                const auto source=read_ref(mime->hasFormat(reference_mime)?mime->data(reference_mime):mime->text().toUtf8());
                host.session.apply({LinkProperties{targets,source,false}},field_revision);host.edited();
            } else if(chosen==unlink) {host.session.apply({UnlinkProperties{targets}},field_revision);host.edited();}
            else if(chosen==relative) pick_source(targets,true);
        });
    });
}

void Window::pick_source(std::vector<Ref> targets,bool relative) {
    const auto target=targets.front();const auto selection=canvas->selections();const auto expected_revision=host.session.revision();
    const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
    auto* dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(720,480);
    dialog->setWindowTitle(relative?"Pick Relative Link source":"Pick property source");
    auto* layout=new QVBoxLayout(dialog);
    auto* target_note=new QLabel(targets.size()==1?"Target: "+property_label(host.session.document(),target):QString::number(targets.size())+" frozen targets · "+qs(target.field));
    target_note->setWordWrap(true);layout->addWidget(target_note);
    auto* search=new QLineEdit;search->setPlaceholderText("Search object, point, property or unit…");layout->addWidget(search);
    auto* list=new QListWidget;layout->addWidget(list);
    const auto values=evaluate(host.session.document());
    for(const auto& ref:properties(host.session.document())) {
        if(std::find(targets.begin(),targets.end(),ref)!=targets.end())continue;
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
        if(host.session.document().objects.contains(source.object))canvas->set_selection(source.object,source.point);
    });
    connect(dialog,&QDialog::rejected,this,[this,selection,frozen_session,composition,artboard]{
        if(host.session_id==frozen_session){perform([&]{canvas->set_active_artboard(composition,artboard,false);});canvas->set_selections(selection);}
    });
    auto accept=[this,dialog,list,targets,selection,relative,frozen_session,expected_revision,composition,artboard] {
        perform([&]{
            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Source picker belongs to a different document");
            if(!list->currentItem())throw Error("NO_SOURCE","Choose a source property");
            const auto source=read_ref(list->currentItem()->data(Qt::UserRole).toByteArray());
            host.session.apply({LinkProperties{targets,source,relative}},expected_revision);
            canvas->set_active_artboard(composition,artboard,false);
            canvas->set_selections(selection);host.edited();dialog->accept();
        });
    };
    connect(buttons,&QDialogButtonBox::accepted,dialog,accept);
    connect(list,&QListWidget::itemDoubleClicked,dialog,[accept](QListWidgetItem*){accept();});
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    dialog->show();search->setFocus();
}
void Window::import_svg() {
    const auto identity=host.session_id;const auto revision=host.session.revision();
    const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
    const auto plane=std::find_if(host.session.document().compositions.begin(),host.session.document().compositions.end(),[&](const auto& c){return c.id==composition;});
    if(plane==host.session.document().compositions.end())throw Error("MISSING_COMPOSITION","Select a Composition");
    const auto board=evaluate_artboard(*plane,artboard);
    QDialog dialog(this);dialog.setObjectName("svg-import-dialog");dialog.setWindowTitle("Import SVG artwork");auto* layout=new QVBoxLayout(&dialog);
    auto* hint=new QLabel("Static paths, basic shapes and Groups, solid fills/strokes, affine transforms.\nShapes become editable paths; curves from arcs/shapes use cubic approximation.\nUnsupported SVG content rejects the whole import. Text, images and CSS stylesheets are not supported.\nArtwork is placed at the active Artboard origin. SVG viewport maps coordinates; it does not crop the imported Group.",&dialog);hint->setWordWrap(true);layout->addWidget(hint);
    auto* row=new QHBoxLayout;auto* path=new QLineEdit(&dialog);path->setObjectName("svg-import-path");path->setMinimumWidth(400);path->setPlaceholderText("Absolute path to .svg");auto* browse=new QPushButton("Browse…",&dialog);row->addWidget(path);row->addWidget(browse);layout->addLayout(row);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Ok)->setText("Import editable artwork");buttons->button(QDialogButtonBox::Ok)->setEnabled(false);layout->addWidget(buttons);
    connect(path,&QLineEdit::textChanged,&dialog,[&]{buttons->button(QDialogButtonBox::Ok)->setEnabled(!path->text().trimmed().isEmpty());});
    connect(browse,&QPushButton::clicked,&dialog,[&]{const auto file=QFileDialog::getOpenFileName(&dialog,"SVG artwork",{},"SVG (*.svg)");if(!file.isEmpty())path->setText(file);});
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(identity!=host.session_id)throw Error("SESSION_CONFLICT","Document changed while SVG import was open");
    const auto result=host.import_svg(path->text().trimmed(),composition,new_id(),QFileInfo(path->text().trimmed()).completeBaseName().toStdString(),board.x,board.y,revision);
    canvas->set_selection(result.value("root").toString().toStdString());canvas->fit_selection();
    statusBar()->showMessage(QString("Imported %1 editable SVG paths in one Group; one Undo removes the import").arg(result.value("paths").toInt()),10000);
}

void Window::export_png() {
    const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
    const auto revision=host.session.revision();const auto identity=host.session_id;
    if(host.session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current gesture before exporting");
    const auto& compositions=host.session.document().compositions;
    const auto comp=std::find_if(compositions.begin(),compositions.end(),[&](const auto& c){return c.id==composition;});
    if(comp==compositions.end())throw Error("MISSING_COMPOSITION",composition);
    const auto board=evaluate_artboard(*comp,artboard);
    QDialog dialog(this);dialog.setObjectName("png-export-dialog");dialog.setWindowTitle("Export PNG");
    auto* layout=new QVBoxLayout(&dialog);
    auto* frame=new QLabel(QString("Artboard: %1").arg(qs(board.name)),&dialog);layout->addWidget(frame);
    auto* form=new QFormLayout;layout->addLayout(form);
    auto* scale=new QDoubleSpinBox(&dialog);scale->setObjectName("png-scale");scale->setDecimals(3);scale->setRange(.001,16);scale->setSingleStep(.25);scale->setValue(png_scale_);
    form->addRow("Pixels per document unit",scale);
    auto* background=new QComboBox(&dialog);background->setObjectName("png-background");background->addItems({"Transparent","White"});background->setCurrentIndex(png_white_?1:0);form->addRow("Background",background);
    auto* dimensions=new QLabel(&dialog);dimensions->setObjectName("png-dimensions");form->addRow("Output",dimensions);
    auto* destination=new QWidget(&dialog);auto* path_layout=new QHBoxLayout(destination);path_layout->setContentsMargins(0,0,0,0);
    auto* path=new QLineEdit(png_path_,destination);path->setObjectName("png-path");path->setMinimumWidth(360);
    if(path->text().isEmpty()&&!host.file_path.isEmpty())path->setText(QFileInfo(host.file_path).absolutePath()+"/"+QFileInfo(host.file_path).completeBaseName()+".png");
    auto* browse=new QPushButton("Browse…",destination);path_layout->addWidget(path);path_layout->addWidget(browse);form->addRow("Save to",destination);
    auto* hint=new QLabel("8-bit sRGB · Maximum 8192 pixels per axis / 16 MP.\nNative artwork remains editable.",&dialog);layout->addWidget(hint);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Save)->setText("Export");layout->addWidget(buttons);
    const auto update=[&]{
        const auto width=std::ceil(board.width*scale->value()),height=std::ceil(board.height*scale->value());
        const auto valid=width>=1&&height>=1&&width<=8192&&height<=8192&&width*height<=16777216;
        dimensions->setText(QString("%1 × %2 px%3").arg(width,0,'f',0).arg(height,0,'f',0).arg(valid?"":" — exceeds output limit"));
        buttons->button(QDialogButtonBox::Save)->setEnabled(valid&&!path->text().trimmed().isEmpty());
    };
    connect(scale,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&]{update();});
    connect(path,&QLineEdit::textChanged,&dialog,[&]{update();});
    connect(browse,&QPushButton::clicked,&dialog,[&]{
        const auto selected=QFileDialog::getSaveFileName(&dialog,"PNG destination",path->text(),"PNG (*.png)",nullptr,QFileDialog::DontConfirmOverwrite);
        if(!selected.isEmpty())path->setText(selected);
    });
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);update();
    if(dialog.exec()!=QDialog::Accepted)return;
    auto output=path->text().trimmed();if(QFileInfo(output).suffix().isEmpty())output+=".png";
    if(QFileInfo::exists(output)&&QMessageBox::question(this,"Replace PNG?","Replace the existing output file?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(identity!=host.session_id)throw Error("SESSION_CONFLICT","Document changed while export settings were open");
    const auto result=host.export_png(output,composition,artboard,scale->value(),background->currentIndex()==1,revision);
    png_scale_=scale->value();png_white_=background->currentIndex()==1;png_path_=output;
    statusBar()->showMessage(QString("PNG exported: %1 × %2 px, sRGB, %3 background").arg(result["width"].toInt()).arg(result["height"].toInt()).arg(png_white_?"white":"transparent"),10000);
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
    host.session.apply({CreatePath{comp.id,"",object,"Curve "+std::to_string(host.session.document().objects.size()+1),{{new_id(),false,{a,b}}}},CenterAnchor{object}},host.session.revision());
    canvas->set_selection(object,a.id);host.edited();canvas->setFocus();
}
void Window::add_primitive(const std::string& type) {
    canvas->set_draw_mode(false);
    const auto& document=host.session.document();
    if(document.compositions.empty())throw Error("MISSING_COMPOSITION","Create a composition before adding a shape");
    const auto& composition=find_composition(document,canvas->active_composition());
    if(composition.artboards.empty())throw Error("MISSING_ARTBOARD","An artboard is needed to place the new shape");
    const auto artboard=evaluate_artboard(composition,canvas->active_artboard());
    auto source=default_primitive(new_id(),type);
    source.parameters.at("center_x").literal=artboard.x+artboard.width/2;
    source.parameters.at("center_y").literal=artboard.y+artboard.height/2;
    const auto id=new_id();
    const auto name=primitive_label(source).toStdString()+" "+std::to_string(document.objects.size()+1);
    host.session.apply({CreatePrimitive{composition.id,{},id,name,std::move(source)},CenterAnchor{id}},host.session.revision());
    canvas->set_selection(id);host.edited();canvas->setFocus();
}
void Window::add_operation(const std::string& type,bool radial) {
    canvas->cancel_interaction();
    const auto& document=host.session.document();
    const auto found=document.objects.find(canvas->selected_object);
    if(found==document.objects.end()||(found->second.kind!=Kind::path&&found->second.kind!=Kind::text))
        throw Error("INVALID_DOMAIN","Select a Path, primitive or Text. Image and Group shape stacks are unsupported.");
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
            for(const auto& contour:path_contours(object,&values))for(const auto& point:contour.points) {
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
    const auto added=operation.id;
    host.session.apply({AddOperation{object.id,std::move(operation),object.stack.size()}},host.session.revision());host.edited();
    QTimer::singleShot(0,this,[this,added]{reveal_operation(added);});
}
void Window::reveal_operation(const Id& operation) {
    for(auto* group:inspector_->findChildren<QGroupBox*>())
        if(group->isVisible()&&group->objectName()=="stack-operation-"+qs(operation)) {
            inspector_scroll_->ensureWidgetVisible(group,10,20);
            // Revealing a full-width section must not pan its labels offscreen.
            inspector_scroll_->horizontalScrollBar()->setValue(0);return;
        }
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
            if(scalar.expression)text+=" · fx "+qs(scalar.expression->source);
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
std::vector<Id> Window::selected_siblings(Id& parent,std::size_t minimum) const {
    const auto selected=canvas->selected_objects();
    if(selected.size()<minimum||std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return !item.point.empty();}))throw Error("INVALID_SELECTION",minimum==1?"Select sibling objects, not points":"Select two or more sibling objects");
    const std::set<Id> chosen(selected.begin(),selected.end());const auto& d=host.session.document();std::vector<Id> result;
    std::function<void(const std::vector<Id>&,const Id&)> search=[&](const std::vector<Id>& siblings,const Id& owner){
        std::vector<Id> found;for(const auto& id:siblings)if(chosen.contains(id))found.push_back(id);
        if(found.size()==chosen.size()){result=std::move(found);parent=owner;return;}
        for(const auto& id:siblings)if(d.objects.at(id).kind==Kind::group)search(d.objects.at(id).children,id);
    };
    search(find_composition(d,canvas->active_composition()).roots,{});
    if(result.empty())throw Error("INVALID_SELECTION","Select sibling objects in the same Composition");return result;
}
void Window::stack_selection(int direction,bool to_edge) {
    Id parent;const auto selected=selected_siblings(parent,1);const std::set<Id> chosen(selected.begin(),selected.end());
    const auto& d=host.session.document();const auto& comp=find_composition(d,canvas->active_composition());
    const auto original=parent.empty()?comp.roots:d.objects.at(parent).children;auto order=original;
    if(to_edge)std::stable_partition(order.begin(),order.end(),[&](const Id& id){return direction>0?!chosen.contains(id):chosen.contains(id);});
    else if(direction>0){for(std::size_t i=order.size()-1;i>0;--i)if(chosen.contains(order[i-1])&&!chosen.contains(order[i]))std::swap(order[i-1],order[i]);}
    else {for(std::size_t i=1;i<order.size();++i)if(chosen.contains(order[i])&&!chosen.contains(order[i-1]))std::swap(order[i-1],order[i]);}
    if(order==original)return;
    canvas->cancel_interaction();host.session.apply({ReorderObjects{comp.id,parent,std::move(order)}},host.session.revision());host.edited();
    statusBar()->showMessage("Stacking order changed within the parent Group/Composition; coordinates are unchanged",5000);
}
void Window::mask_selection(bool top) {
    Id parent;const auto members=selected_siblings(parent);const auto id=new_id();
    canvas->cancel_interaction();host.session.apply({MaskObjects{canvas->active_composition(),parent,members,id,new_id(),"Masked Group",top}},host.session.revision());
    canvas->set_selection(id);host.edited();
}
void Window::put_selection_inside() {
    Id parent;auto members=selected_siblings(parent);const auto group=members.back();members.pop_back();
    canvas->cancel_interaction();host.session.apply({PutInside{canvas->active_composition(),parent,group,members}},host.session.revision());canvas->set_selection(group);host.edited();
}
void Window::selection_menu(const QPoint& global) {
    const auto menu_session=host.session_id;const auto menu_revision=host.session.revision();
    QMenu menu;auto* duplicate=menu.addAction("Duplicate objects in place");duplicate->setEnabled(!canvas->selected_objects().empty()&&canvas->selected_point.empty());
    auto* transform=menu.addAction("Rotate / scale selection…");
    transform->setEnabled(!canvas->selections().empty()&&std::all_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return item.point.empty();}));
    auto* stacking=menu.addMenu("Arrange stacking order");
    std::map<QAction*,std::pair<int,bool>> stack_actions;
    for(const auto& [label,direction,edge]:std::vector<std::tuple<QString,int,bool>>{{"Bring forward",1,false},{"Send backward",-1,false},{"Bring to front",1,true},{"Send to back",-1,true}})
        stack_actions.emplace(stacking->addAction(label),std::pair{direction,edge});
    try{Id parent;(void)selected_siblings(parent,1);}catch(const Error&){stacking->setEnabled(false);}
    auto* group=menu.addAction("Group selected siblings");
    auto* ungroup=menu.addAction("Ungroup selected Groups");
    try{Id parent;const auto members=selected_siblings(parent,1);ungroup->setEnabled(std::all_of(members.begin(),members.end(),[&](const Id& id){return host.session.document().objects.at(id).kind==Kind::group;}));}catch(const Error&){ungroup->setEnabled(false);}
    auto* top=menu.addAction("Mask With Top");auto* bottom=menu.addAction("Mask With Bottom");auto* inside=menu.addAction("Put Inside top selected Group");
    try {
        Id parent;const auto members=selected_siblings(parent);const auto& d=host.session.document();
        top->setText("Mask With Top · "+qs(d.objects.at(members.back()).name));bottom->setText("Mask With Bottom · "+qs(d.objects.at(members.front()).name));
        inside->setText("Put Inside · "+qs(d.objects.at(members.back()).name));
        top->setEnabled((d.objects.at(members.back()).kind==Kind::path||d.objects.at(members.back()).kind==Kind::text));bottom->setEnabled((d.objects.at(members.front()).kind==Kind::path||d.objects.at(members.front()).kind==Kind::text));inside->setEnabled(d.objects.at(members.back()).kind==Kind::group);
    } catch(const Error&) {group->setEnabled(false);top->setEnabled(false);bottom->setEnabled(false);inside->setEnabled(false);}
    auto* chosen=menu.exec(global);if(!chosen)return;
    perform([&]{if(host.session_id!=menu_session||host.session.revision()!=menu_revision)throw Error("STALE_CONTEXT","Document changed while the menu was open; reopen the selection menu");if(stack_actions.contains(chosen)){const auto [direction,edge]=stack_actions.at(chosen);stack_selection(direction,edge);}else if(chosen==transform)transform_selection();else if(chosen==duplicate)duplicate_selection();else if(chosen==top)mask_selection(true);else if(chosen==bottom)mask_selection(false);else if(chosen==inside)put_selection_inside();else if(chosen==group)group_selection();else if(chosen==ungroup)ungroup_selection();});
}
void Window::duplicate_selection() {
    if(canvas->selected_objects().empty()||!canvas->selected_point.empty())throw Error("INVALID_SELECTION","Select objects or Groups to duplicate");
    canvas->cancel_interaction();const DuplicateObjects command{canvas->selected_objects(),new_id()};
    const auto roots=duplicated_roots(host.session.document(),command);
    host.session.apply({command},host.session.revision());
    std::vector<Canvas::Selection> selection;for(const auto& id:roots)selection.push_back({id,{}});
    canvas->set_selections(std::move(selection));host.edited();canvas->setFocus();
    statusBar()->showMessage("Duplicated in place; drag the selected copies to move them",6000);
}
void Window::ungroup_selection() {
    Id parent;const auto groups=selected_siblings(parent,1);std::vector<Command> commands;std::vector<Canvas::Selection> children;
    for(const auto& id:groups){const auto& object=host.session.document().objects.at(id);if(object.kind!=Kind::group)throw Error("INVALID_GROUP","Select Groups to ungroup");
        commands.push_back(Ungroup{canvas->active_composition(),parent,id});for(const auto& child:object.children)children.push_back({child,{}});}
    canvas->cancel_interaction();host.session.apply(commands,host.session.revision());canvas->set_selections(std::move(children));host.edited();
    statusBar()->showMessage("Ungrouped with child geometry and order preserved; Undo restores the container",6000);
}
void Window::group_selection() {
    if(!canvas->selected_point.empty())throw Error("INVALID_GROUP","Select objects, not points, to group");
    const auto objects=canvas->selected_objects();std::set<Id> chosen(objects.begin(),objects.end());
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
    try {host.flush();event->accept();}
    catch(const std::exception& e) {QMessageBox::warning(this,"Recovery failed",QString::fromUtf8(e.what())+"\nSave your document before closing.");event->ignore();}
}
}
