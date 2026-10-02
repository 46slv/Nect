#include "window.hpp"
#include "colors.hpp"
#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QColorDialog>
#include <QColor>
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
#include <QHash>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QIcon>
#include <QPixmap>
#include <QPainterPath>
#include <QLineEdit>
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
#include <QSet>
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
#include <limits>
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
struct TextSourcePicker {
    QDialog* dialog=nullptr;
    QLineEdit* search=nullptr;
    QListWidget* list=nullptr;
    QLabel* status=nullptr;
    QDialogButtonBox* buttons=nullptr;
};
void choose_boolean_source(QWidget* parent,const QString& name,const QString& title,const QString& target,
        const std::vector<Ref>& refs,const QStringList& labels,const std::function<void(const Ref&)>& apply) {
    QDialog dialog(parent);dialog.setObjectName(name);dialog.setWindowTitle(title);dialog.resize(700,240);
    auto* layout=new QVBoxLayout(&dialog);
    auto* target_label=new QLabel("Target: "+target,&dialog);target_label->setWordWrap(true);layout->addWidget(target_label);
    auto* search=new QLineEdit(&dialog);search->setObjectName(name+"-search");
    search->setPlaceholderText("Search label or stable Ref path…");layout->addWidget(search);
    auto* source=new QComboBox(&dialog);source->setObjectName(name+"-source");layout->addWidget(source);
    auto* status=new QLabel("Choose a visible source.",&dialog);status->setObjectName(name+"-status");
    status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Link");layout->addWidget(buttons);
    const auto refill=[source,refs,labels](const QString& query) {
        const int selected=source->currentIndex()<0?-1:source->currentData().toInt();
        const QSignalBlocker blocker(source);source->clear();
        const auto terms=query.split(' ',Qt::SkipEmptyParts);
        for(int i=0;i<labels.size();++i) {
            const auto& ref=refs.at(static_cast<std::size_t>(i));
            const auto path=qs(ref.object)+" / "+qs(ref.field);
            const auto searchable=labels.at(i)+" "+path;
            if(std::all_of(terms.begin(),terms.end(),[&](const auto& term){
                    return searchable.contains(term,Qt::CaseInsensitive);}))source->addItem(labels.at(i)+"  ·  "+path,i);
        }
        source->setCurrentIndex(selected<0?-1:source->findData(selected));
    };
    refill({});
    QObject::connect(search,&QLineEdit::textChanged,&dialog,[refill,status](const QString& query){
        refill(query);status->setText("Choose a visible source.");
    });
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
        [source,status,refs,apply,&dialog] {
            try {
                const int index=source->currentIndex()<0?-1:source->currentData().toInt();
                if(index<0||static_cast<std::size_t>(index)>=refs.size())
                    throw Error("MISSING_REFERENCE","Choose a visible boolean source");
                apply(refs.at(static_cast<std::size_t>(index)));dialog.accept();
            } catch(const Error& error) {status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
              catch(const std::exception& error) {status->setText(QString::fromUtf8(error.what()));}
        });
    dialog.exec();
}
TextSourcePicker make_text_source_picker(QWidget* parent,const Document& document,const Id& target,const std::string& field,
        const std::vector<Id>& source_ids,const QString& title) {
    auto* dialog=new QDialog(parent);dialog->setObjectName("text-source-picker");dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(title);dialog->resize(720,480);
    auto* layout=new QVBoxLayout(dialog);
    auto* target_note=new QLabel("Target: "+qs(target)+" / "+qs(field),dialog);target_note->setWordWrap(true);layout->addWidget(target_note);
    auto* search=new QLineEdit(dialog);search->setObjectName("text-source-picker-search");
    search->setPlaceholderText("Search Text label or stable Ref path…");layout->addWidget(search);
    auto* list=new QListWidget(dialog);list->setObjectName("text-source-picker-list");list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list);
    for(const auto& source_id:source_ids) {
        const auto found=document.objects.find(source_id);if(found==document.objects.end())continue;
        const auto stable_path=qs(source_id)+" / "+qs(field);
        auto* item=new QListWidgetItem(qs(found->second.name)+" — "+qs(source_id)+"  ·  "+stable_path,list);
        const Ref ref{source_id,"",field};item->setData(Qt::UserRole,QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact));
        item->setData(Qt::UserRole+1,stable_path);item->setToolTip(stable_path);
    }
    auto* status=new QLabel("Choose a visible source Text.",dialog);status->setObjectName("text-source-picker-status");
    status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Link");layout->addWidget(buttons);
    QObject::connect(search,&QLineEdit::textChanged,dialog,[list,status](const QString& text){
        const auto terms=text.split(' ',Qt::SkipEmptyParts);
        for(int i=0;i<list->count();++i) {
            auto* item=list->item(i);const auto searchable=item->text()+" "+item->data(Qt::UserRole+1).toString();
            const bool matches=std::all_of(terms.begin(),terms.end(),[&](const auto& term){return searchable.contains(term,Qt::CaseInsensitive);});
            if(!matches&&list->currentItem()==item)list->setCurrentItem(nullptr);
            item->setHidden(!matches);
        }
        status->setText("Choose a visible source Text.");
    });
    QObject::connect(list,&QListWidget::currentItemChanged,dialog,[status](QListWidgetItem* item,QListWidgetItem*){
        const bool visible=item&&!item->isHidden();
        if(visible)status->setText("Ready to link the selected Text source.");
        else status->setText("Choose a visible source Text.");
    });
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    return {dialog,search,list,status,buttons};
}
QLineEdit* add_artboard_source_search(QWidget* parent,QVBoxLayout* layout,QComboBox* source,
        const std::vector<Ref>& refs,const QStringList& labels,const std::optional<Ref>& selected_ref) {
    auto* search=new QLineEdit(parent);
    search->setObjectName(source->objectName()+"-search");
    search->setAccessibleName("Search Artboard source name, ID or field path");
    search->setPlaceholderText("Search Artboard name, ID or field path…");
    layout->addWidget(search);

    int selected_candidate=-1;
    if(selected_ref) {
        const auto found=std::find(refs.begin(),refs.end(),*selected_ref);
        if(found!=refs.end())selected_candidate=static_cast<int>(std::distance(refs.begin(),found));
    }
    for(int candidate=0;candidate<static_cast<int>(refs.size());++candidate) {
        const auto& ref=refs.at(static_cast<std::size_t>(candidate));
        source->addItem(labels.at(candidate));
        const int row=source->count()-1;
        source->setItemData(row,candidate,Qt::UserRole);
        source->setItemData(row,qs(ref.object+"/"+ref.field),Qt::ToolTipRole);
    }
    source->setCurrentIndex(selected_candidate);

    const auto refill=[source,refs,labels](const QString& query) {
        bool valid_selection=false;
        const int selected=source->currentIndex()<0?-1:source->currentData(Qt::UserRole).toInt(&valid_selection);
        const int selected_candidate=valid_selection?selected:-1;
        const auto terms=query.simplified().split(' ',Qt::SkipEmptyParts);
        const QSignalBlocker blocker(source);
        source->clear();
        int selected_row=-1;
        for(int candidate=0;candidate<static_cast<int>(refs.size());++candidate) {
            const auto& ref=refs.at(static_cast<std::size_t>(candidate));
            const auto path=qs(ref.object)+" "+qs(ref.field)+" "+qs(ref.object+"/"+ref.field);
            const auto searchable=labels.at(candidate)+" "+path;
            if(!std::all_of(terms.begin(),terms.end(),[&](const QString& term){
                    return searchable.contains(term,Qt::CaseInsensitive);}))continue;
            source->addItem(labels.at(candidate));
            const int row=source->count()-1;
            source->setItemData(row,candidate,Qt::UserRole);
            source->setItemData(row,qs(ref.object+"/"+ref.field),Qt::ToolTipRole);
            if(candidate==selected_candidate)selected_row=row;
        }
        source->setCurrentIndex(selected_row);
    };
    QObject::connect(search,&QLineEdit::textChanged,parent,[refill](const QString& query){refill(query);});
    return search;
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
    if(source.type=="nect.shape.ellipse")return QStringLiteral("Ellipse");
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
    if(parameter=="levels")return QStringLiteral("Levels");
    if(parameter=="miter_limit")return QStringLiteral("Miter limit");
    if(parameter=="end_x")return QStringLiteral("End X");
    if(parameter=="end_y")return QStringLiteral("End Y");
    return qs(parameter);
}
QString operation_label(const ShapeOperation& operation) {
    if(const auto* descriptor=builtin_operation_type(operation.type))return qs(descriptor->label);
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
QString artboard_choice_label(const Composition& composition,const Artboard& board) {
    const auto matches=std::count_if(composition.artboards.begin(),composition.artboards.end(),[&](const Artboard& item) {
        return item.name==board.name;
    });
    if(matches<2)return qs(board.name);
    int ordinal=0;
    for(const auto& item:composition.artboards)if(item.name==board.name) {
        ++ordinal;if(item.id==board.id)break;
    }
    return qs(board.name)+" · frame "+QString::number(ordinal);
}
QString definition_choice_label(const Document& document,const Id& definition_id) {
    const auto& definition=document.definitions.at(definition_id);
    const auto matches=std::count_if(document.definitions.begin(),document.definitions.end(),[&](const auto& item) {
        return item.second.name==definition.name;
    });
    if(matches<2)return qs(definition.name);
    int ordinal=0;
    for(const auto& [id,item]:document.definitions)if(item.name==definition.name) {
        ++ordinal;if(id==definition_id)break;
    }
    return qs(definition.name)+" · Definition "+QString::number(ordinal);
}
QString template_base_choice_label(const Document& document,const Composition& composition,
        const ArtboardTemplate& item) {
    const auto source=artboard_choice_label(composition,find_artboard(composition,item.source_artboard));
    const QString definition=item.definition?QString("Definition ")+definition_choice_label(document,*item.definition):QString("No Definition");
    return qs(item.name)+" · source "+source+" · "+definition;
}
QString template_choice_label(const Document& document,const Composition& composition,const ArtboardTemplate& item) {
    const auto base=template_base_choice_label(document,composition,item);
    int ordinal=0,total=0;
    for(const auto& candidate:composition.templates)if(template_base_choice_label(document,composition,candidate)==base) {
        ++total;if(candidate.id==item.id)ordinal=total;
    }
    return total<2?base:base+" · option "+QString::number(ordinal);
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
    void set_display_zero(double degrees) {display_zero_=degrees;update();}
    void disarm_drag(double value) {dragging_=false;set_value(value);}
    QSize sizeHint() const override {return {44,44};}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing,true);
        const QRectF face=QRectF(rect()).adjusted(5,5,-5,-5);
        QColor ring=isEnabled()?QColor("#526071"):QColor("#38414c");
        painter.setPen(QPen(ring,2));painter.setBrush(QColor("#202833"));painter.drawEllipse(face);
        painter.setPen(QPen(isEnabled()?QColor("#48c6e9"):QColor("#69717a"),3,Qt::SolidLine,Qt::RoundCap));
        const auto normalized=std::fmod(value_,360.0)<0?std::fmod(value_,360.0)+360.0:std::fmod(value_,360.0);
        const auto radians=(normalized+display_zero_)*std::numbers::pi/180.0;
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
    double value_=0,press_value_=0,previous_angle_=0,accumulated_=0,display_zero_=-90.0;
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

enum class UtilityToggleGlyph { guides, grid, snap };
QIcon utility_toggle_icon(UtilityToggleGlyph glyph) {
    QIcon icon;
    for(const auto mode:{QIcon::Normal,QIcon::Active,QIcon::Disabled,QIcon::Selected})
        for(const auto state:{QIcon::Off,QIcon::On})for(const int dpr:{1,2,3}) {
            QPixmap pixels(20*dpr,20*dpr);pixels.setDevicePixelRatio(dpr);pixels.fill(Qt::transparent);
            QPainter painter(&pixels);painter.setRenderHint(QPainter::Antialiasing);
            const bool on=state==QIcon::On;
            const auto ink=mode==QIcon::Disabled?QColor(on?"#9ca7b1":"#77818d"):
                QColor(on||mode==QIcon::Active||mode==QIcon::Selected?"#e9fbff":"#adb9c7");
            QPen pen(ink,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
            painter.setPen(pen);painter.setBrush(Qt::NoBrush);
            if(glyph==UtilityToggleGlyph::guides) {
                painter.drawLine(QPointF(3.5,6.5),QPointF(3.5,3.5));
                painter.drawLine(QPointF(3.5,3.5),QPointF(6.5,3.5));
                painter.drawLine(QPointF(13.5,16.5),QPointF(16.5,16.5));
                painter.drawLine(QPointF(16.5,16.5),QPointF(16.5,13.5));
                pen.setStyle(Qt::DashLine);painter.setPen(pen);
                painter.drawLine(QPointF(10,2.5),QPointF(10,17.5));
                painter.drawLine(QPointF(2.5,10),QPointF(17.5,10));
            } else if(glyph==UtilityToggleGlyph::grid) {
                painter.drawRect(QRectF(3.5,3.5,13,13));
                for(const auto position:{8.0,12.0}) {
                    painter.drawLine(QPointF(position,3.5),QPointF(position,16.5));
                    painter.drawLine(QPointF(3.5,position),QPointF(16.5,position));
                }
            } else {
                pen.setWidthF(3);pen.setCapStyle(Qt::FlatCap);painter.setPen(pen);
                QPainterPath magnet;magnet.moveTo(5,3);magnet.lineTo(5,10.5);
                magnet.cubicTo(5,18,15,18,15,10.5);magnet.lineTo(15,3);painter.drawPath(magnet);
                pen.setWidthF(1);pen.setColor(QColor("#252d38"));painter.setPen(pen);
                painter.drawLine(QPointF(3.5,7),QPointF(6.5,7));
                painter.drawLine(QPointF(13.5,7),QPointF(16.5,7));
            }
            painter.end();icon.addPixmap(pixels,mode,state);
        }
    return icon;
}

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

void Window::register_angle_adapter(QWidget* control,std::function<void(bool)> cancel) {
    std::erase_if(angle_adapters_,[](const auto& entry){return entry.control.isNull();});
    angle_adapters_.push_back({control,std::move(cancel)});
}

void Window::cancel_angle_adapters(bool dispose) {
    for(auto it=angle_adapters_.begin();it!=angle_adapters_.end();) {
        if(it->control.isNull()||!it->cancel) {it=angle_adapters_.erase(it);continue;}
        it->cancel(dispose);
        if(dispose)it=angle_adapters_.erase(it);else ++it;
    }
}

void Window::bind_angle_adapter(QWidget* control,QLineEdit* numeric,const Ref& ref,double initial,
        std::function<void()> validate_target,bool keep_last_valid_on_range,bool refuse_numeric_draft) {
    auto* knob=dynamic_cast<RotationKnob*>(control);
    if(!knob)throw std::invalid_argument("Angle adapter requires a RotationKnob");
    struct Interaction {bool live=true,owned=false;std::uint64_t generation=0;};
    const auto state=std::make_shared<Interaction>();
    const auto identity=host.session_id;const auto revision=host.session.revision();
    const auto document=host.session.document().id;
    QPointer<RotationKnob> safe_knob=knob;QPointer<QLineEdit> safe_numeric=numeric;
    auto owns=[this,state,identity,revision,document] {
        return state->owned&&host.session_id==identity&&host.session.document().id==document&&
            host.session.revision()==revision&&host.session.gesture_active()&&
            host.session.gesture_generation()==state->generation;
    };
    auto validate=[this,state,identity,revision,document,validate_target] {
        if(!state->live||host.session_id!=identity||host.session.document().id!=document)
            throw Error("SESSION_CONFLICT","Angle control belongs to another editing context");
        if(host.session.revision()!=revision)throw Error("REVISION_CONFLICT","Angle changed; reopen the Inspector");
        validate_target();
    };
    auto current_value=[this,identity,document,ref,initial] {
        try {
            if(host.session_id!=identity||host.session.document().id!=document)return initial;
            const auto& current=host.session.gesture_active()?host.session.preview_document():host.session.document();
            const auto values=evaluate(current);const auto found=values.find(ref);
            return found==values.end()?initial:found->second;
        } catch(...) {return initial;}
    };
    auto report=[this](const std::exception& exception) {
        if(const auto* error=dynamic_cast<const Error*>(&exception))statusBar()->showMessage(qs(error->code)+": "+QString::fromUtf8(error->what()),12000);
        else statusBar()->showMessage(QString::fromUtf8(exception.what()),12000);
    };
    auto cancel=[this,state,owns,safe_knob,safe_numeric,current_value] {
        const bool local_interaction=state->owned;const bool active=owns();
        if(active)host.session.cancel_gesture();
        state->owned=false;
        if(local_interaction) {
            const auto value=current_value();
            if(safe_knob)safe_knob->disarm_drag(value);
            if(safe_numeric){safe_numeric->setText(QString::number(value,'g',17));safe_numeric->setModified(false);}
        }
        if(active){canvas->refresh();canvas->update();}
    };
    register_angle_adapter(knob,[state,cancel](bool dispose){if(dispose)state->live=false;cancel();});
    auto last_valid=std::make_shared<double>(initial);
    knob->begin_drag=[this,state,validate,report,safe_knob,safe_numeric,refuse_numeric_draft,last_valid,initial] {
        try {
            if(refuse_numeric_draft&&safe_numeric&&safe_numeric->isModified())
                throw Error("UNCOMMITTED_INPUT","Commit or cancel the numeric draft before using the dial");
            validate();host.session.begin_gesture(host.session.revision());
            *last_valid=initial;
            if(safe_knob)safe_knob->set_value(initial);
            if(safe_numeric){safe_numeric->setText(QString::number(initial,'g',17));safe_numeric->setModified(false);}
            state->generation=host.session.gesture_generation();state->owned=true;return true;
        } catch(const std::exception& exception) {
            if(safe_numeric)safe_numeric->setFocus(Qt::OtherFocusReason);report(exception);return false;
        }
    };
    knob->preview_value=[this,state,owns,validate,cancel,report,ref,initial,safe_numeric,safe_knob,last_valid,keep_last_valid_on_range](double value) {
        if(!owns()){if(state->owned)cancel();return;}
        try {
            validate();
            if(std::abs(value-initial)<=1e-10) {
                value=initial;if(safe_knob)safe_knob->set_value(initial);host.session.update_gesture({});
            } else host.session.update_gesture({EditProperties{{ref},value,false}});
            *last_valid=value;canvas->refresh();canvas->update();
            if(safe_numeric){safe_numeric->setText(QString::number(value,'g',17));safe_numeric->setModified(false);}
        } catch(const std::exception& exception) {
            if(keep_last_valid_on_range)if(const auto* error=dynamic_cast<const Error*>(&exception);error&&error->code=="OUT_OF_RANGE") {
                if(safe_knob)safe_knob->set_value(*last_valid);
                if(safe_numeric){safe_numeric->setText(QString::number(*last_valid,'g',17));safe_numeric->setModified(false);}
                report(exception);return;
            }
            cancel();report(exception);
        }
    };
    knob->commit_drag=[this,state,owns,validate,cancel,report] {
        if(!owns()){if(state->owned)cancel();return;}
        try {validate();host.session.commit_gesture();state->owned=false;host.edited();}
        catch(const std::exception& exception){cancel();report(exception);}
    };
    knob->cancel_drag=cancel;
}

Window::Window(QString recovery_directory, std::unique_ptr<FolderLibrary> folder_library)
    : host(std::move(recovery_directory),this), folder_library_(std::move(folder_library)) {
    if (!folder_library_) folder_library_ = std::make_unique<FolderLibrary>();
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
    effects_dock_=new QDockWidget("Effects",this);
    effects_dock_->setObjectName("effects");
    auto* effects_body=new QWidget(effects_dock_);
    auto* effects_root_layout=new QVBoxLayout(effects_body);
    effects_root_layout->setContentsMargins(8,8,8,8);effects_root_layout->setSpacing(6);
    effects_tabs_=new QTabWidget(effects_body);effects_tabs_->setObjectName("effects-tabs");
    auto* effects_page=new QWidget(effects_tabs_);
    auto* effects_layout=new QVBoxLayout(effects_page);
    effects_layout->setContentsMargins(4,4,4,4);effects_layout->setSpacing(6);
    auto* effects_heading=new QLabel("Built-in effects",effects_page);effects_heading->setObjectName("effects-heading");
    effects_layout->addWidget(effects_heading);
    effects_search_=new QLineEdit(effects_page);effects_search_->setObjectName("effects-search");
    effects_search_->setPlaceholderText("Search effects…");effects_search_->setClearButtonEnabled(true);
    effects_layout->addWidget(effects_search_);
    effects_catalog_=new QListWidget(effects_page);effects_catalog_->setObjectName("effects-catalog");
    effects_catalog_->setSelectionMode(QAbstractItemView::SingleSelection);
    for(const auto& descriptor:builtin_operation_types())if(descriptor.effects_catalog) {
        auto* entry=new QListWidgetItem(qs(descriptor.label),effects_catalog_);
        entry->setData(Qt::UserRole,qs(descriptor.type));
        entry->setData(Qt::UserRole+1,"builtin");
        entry->setData(Qt::UserRole+2,static_cast<qulonglong>(descriptor.version));
        entry->setToolTip(qs(descriptor.target_kind)+" · "+qs(descriptor.input)+" → "+qs(descriptor.output)+
            " · "+qs(descriptor.type)+" · behavior v"+QString::number(descriptor.version));
    }
    if(effects_catalog_->count())effects_catalog_->setCurrentRow(0);
    effects_layout->addWidget(effects_catalog_);
    auto* no_results=new QLabel("No supported effects match this search.",effects_page);
    no_results->setObjectName("effects-no-results");no_results->setWordWrap(true);no_results->hide();effects_layout->addWidget(no_results);
    effects_target_=new QLabel(effects_page);effects_target_->setObjectName("effects-target");
    effects_target_->setWordWrap(true);effects_target_->setTextFormat(Qt::PlainText);effects_layout->addWidget(effects_target_);
    effects_apply_=new QPushButton("Apply Offset Paths",effects_page);effects_apply_->setObjectName("effects-apply");
    effects_layout->addWidget(effects_apply_);
    effects_favorite_=new QPushButton("Favorite selected built-in",effects_page);effects_favorite_->setObjectName("effects-favorite");
    effects_favorite_->setToolTip("Save this exact built-in TypeID and BehaviorVersion to the shared workspace Favorites.");
    effects_layout->addWidget(effects_favorite_);
    effects_status_=new QLabel(effects_page);effects_status_->setObjectName("effects-status");
    effects_status_->setWordWrap(true);effects_status_->setTextFormat(Qt::PlainText);effects_layout->addWidget(effects_status_);
    auto* applied=new QGroupBox("Applied effect instances",effects_page);applied->setObjectName("effects-applied");
    effects_operations_=new QWidget(applied);effects_operations_layout_=new QVBoxLayout(effects_operations_);
    effects_operations_layout_->setContentsMargins(0,0,0,0);effects_operations_layout_->setSpacing(4);
    auto* applied_layout=new QVBoxLayout(applied);applied_layout->addWidget(effects_operations_);
    effects_layout->addWidget(applied);effects_layout->addStretch();
    effects_tabs_->addTab(effects_page,"Effects");

    auto* presets_page=new QWidget(effects_tabs_);
    auto* presets_layout=new QVBoxLayout(presets_page);
    presets_layout->setContentsMargins(4,4,4,4);presets_layout->setSpacing(6);
    auto* presets_heading=new QLabel("Document Presets · ordered Path/Text stack",presets_page);
    presets_heading->setObjectName("presets-heading");presets_layout->addWidget(presets_heading);
    presets_search_=new QLineEdit(presets_page);presets_search_->setObjectName("presets-search");
    presets_search_->setPlaceholderText("Search presets by name, category, tag, or ID…");
    presets_search_->setClearButtonEnabled(true);presets_layout->addWidget(presets_search_);
    presets_catalog_=new QListWidget(presets_page);presets_catalog_->setObjectName("presets-catalog");
    presets_catalog_->setSelectionMode(QAbstractItemView::SingleSelection);presets_layout->addWidget(presets_catalog_);
    presets_status_=new QLabel(presets_page);presets_status_->setObjectName("presets-status");
    presets_status_->setWordWrap(true);presets_status_->setTextFormat(Qt::PlainText);presets_layout->addWidget(presets_status_);
    presets_save_=new QPushButton("Save Current Stack as Preset",presets_page);presets_save_->setObjectName("preset-save");
    presets_save_->setToolTip("Captures the supported built-in and pinned Macro entries in the current Path/Text processing order. Driven or unsupported entries are reported.");
    presets_layout->addWidget(presets_save_);
    presets_publish_=new QPushButton("Publish Selected Preset to Library",presets_page);presets_publish_->setObjectName("preset-publish-library");
    presets_publish_->setToolTip("Copy selected Preset to workspace Library; Macro entries are not supported yet.");
    presets_layout->addWidget(presets_publish_);
    presets_apply_=new QPushButton("Apply Preset",presets_page);presets_apply_->setObjectName("preset-apply");
    presets_layout->addWidget(presets_apply_);
    presets_rename_=new QPushButton("Rename Preset",presets_page);presets_rename_->setObjectName("preset-rename");
    presets_layout->addWidget(presets_rename_);
    presets_update_=new QPushButton("Update from Current Stack",presets_page);presets_update_->setObjectName("preset-update");
    presets_layout->addWidget(presets_update_);
    presets_delete_=new QPushButton("Delete Preset",presets_page);presets_delete_->setObjectName("preset-delete");
    presets_layout->addWidget(presets_delete_);presets_layout->addStretch();
    effects_tabs_->addTab(presets_page,"Presets");
    effects_root_layout->addWidget(effects_tabs_);
    effects_dock_->setWidget(effects_body);
    addDockWidget(Qt::RightDockWidgetArea,effects_dock_);
    tabifyDockWidget(right,effects_dock_);
    right->raise();
    connect(effects_search_,&QLineEdit::textChanged,this,[this](const QString&){rebuild_effects_panel();});
    connect(effects_catalog_,&QListWidget::currentItemChanged,this,[this](QListWidgetItem*,QListWidgetItem*){rebuild_effects_panel();});
    connect(presets_search_,&QLineEdit::textChanged,this,[this](const QString&){rebuild_effects_panel();});
    connect(presets_catalog_,&QListWidget::currentItemChanged,this,[this](QListWidgetItem*,QListWidgetItem*){rebuild_effects_panel();});
    connect(effects_favorite_,&QPushButton::clicked,this,[this]{perform([this]{
        const auto* entry=effects_catalog_->currentItem();
        if(!entry||entry->data(Qt::UserRole+1).toString()!="builtin")
            throw Error("INVALID_OPERATOR","Choose a built-in effect from the catalog before saving a Favorite");
        const auto type_id=entry->data(Qt::UserRole).toString();
        bool version_ok=false;
        const auto version=entry->data(Qt::UserRole+2).toULongLong(&version_ok);
        const auto* descriptor=builtin_operation_type(type_id.toStdString());
        if(!version_ok||!descriptor||!descriptor->effects_catalog||descriptor->version!=version)
            throw Error("UNAVAILABLE_EFFECT_TYPE","The selected built-in Effect TypeID and BehaviorVersion are no longer available");
        const auto saved=folder_library_->add_favorite({type_id,static_cast<std::uint32_t>(version)});
        const auto message="Favorite saved · "+qs(descriptor->label)+" · "+type_id+" behavior v"+QString::number(version);
        effects_status_->setText(message);
        statusBar()->showMessage(message,8000);
        (void)saved;
    });});
    connect(effects_apply_,&QPushButton::clicked,this,[this]{
        const auto frozen_session=effects_session_;const auto frozen_target=effects_target_id_;
        const auto frozen_revision=effects_revision_;const auto frozen_generation=effects_generation_;
        auto* entry=effects_catalog_->currentItem();
        const auto effect_type=entry?entry->data(Qt::UserRole).toString():QString{};
        const auto effect_name=entry?entry->text():QStringLiteral("Unavailable effect");
        const auto target_label=effects_target_->text();
        try {
            if(effect_type.startsWith("macro:")) {
                if(frozen_generation!=effects_generation_)throw Error("REVISION_CONFLICT","Effects panel changed; refresh the target before applying");
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Effects target belongs to another document");
                if(canvas->selected_object!=frozen_target)throw Error("TARGET_CONFLICT","Effects target changed; choose the current target");
                if(host.session.revision()!=frozen_revision)throw Error("REVISION_CONFLICT","Effects target changed elsewhere; refresh the panel before applying");
                const auto definition_id=effect_type.mid(6).toStdString();
                const auto& document=host.session.document();
                const auto definition=document.macro_definitions.find(definition_id);
                if(definition==document.macro_definitions.end())throw Error("MISSING_MACRO_DEFINITION",definition_id);
                const auto object=document.objects.find(frozen_target);
                if(object==document.objects.end()||(object->second.kind!=Kind::path&&object->second.kind!=Kind::text))
                    throw Error("INVALID_DOMAIN","Macro target must be a Path or Text object");
                host.session.apply({MacroCommand{InstantiateMacro{frozen_target,definition_id,new_id(),
                    definition->second.latest_revision,object->second.stack.size()}}},frozen_revision);
                host.edited();
            } else {
                if(effect_type.isEmpty())throw Error("INVALID_OPERATOR","Choose a supported built-in effect");
                bool version_ok=false;
                const auto version=entry->data(Qt::UserRole+2).toULongLong(&version_ok);
                if(!version_ok||version>std::numeric_limits<std::uint32_t>::max())
                    throw Error("INVALID_OPERATOR","The selected built-in effect has no valid BehaviorVersion");
                apply_builtin_effect_favorite({effect_type,static_cast<std::uint32_t>(version)},
                    frozen_session,frozen_target,frozen_revision,frozen_generation);
            }
            effects_status_->setText("Applied "+effect_name+" · "+target_label);
        } catch(const Error& error) {
            const auto message=(frozen_target.empty()?QStringLiteral("Target: none"):target_label)+
                " · "+effect_type+" ("+effect_name+") · "+qs(error.code)+": "+QString::fromUtf8(error.what());
            effects_status_->setText(message);statusBar()->showMessage(message,12000);
        } catch(const std::exception& error) {
            const auto message=(frozen_target.empty()?QStringLiteral("Target: none"):target_label)+
                " · "+effect_type+" ("+effect_name+") · "+QString::fromUtf8(error.what());
            effects_status_->setText(message);statusBar()->showMessage(message,12000);
        }
    });
    const auto preset_context_current=[this](std::uint64_t generation,const QString& session,const Id& target,std::uint64_t revision) {
        if(host.session_id!=session)throw Error("SESSION_CONFLICT","Preset selection belongs to another document");
        if(generation!=effects_generation_)throw Error("REVISION_CONFLICT","Preset browser changed; refresh the target before editing");
        if(canvas->selected_object!=target)throw Error("TARGET_CONFLICT","Preset target changed; choose the current target");
        if(host.session.revision()!=revision)throw Error("REVISION_CONFLICT","Preset document changed elsewhere; refresh the browser before editing");
    };
    const auto set_preset_status=[this](const QString& message){presets_status_->setText(message);statusBar()->showMessage(message,12000);};
    connect(presets_publish_,&QPushButton::clicked,this,[this,preset_context_current,set_preset_status]{
        const auto generation=effects_generation_;const auto session=effects_session_;const auto target=effects_target_id_;
        const auto revision=effects_revision_;const auto* item=presets_catalog_->currentItem();
        if(!item){set_preset_status("Choose a document Preset to publish.");return;}
        const auto id=item->data(Qt::UserRole).toString().toStdString();
        try {
            preset_context_current(generation,session,target,revision);
            const auto found=host.session.document().preset_definitions.find(id);
            if(found==host.session.document().preset_definitions.end())throw Error("MISSING_PRESET",id);
            const auto published=folder_library_->publish_preset(found->second);
            set_preset_status("Published “"+QString::fromStdString(found->second.label)+"” to Workspace Preset Library · AssetID "+
                published.ref.asset_id+" · revision 1.");
        } catch(const Error& error) {set_preset_status(qs(error.code)+": "+QString::fromUtf8(error.what()));}
        catch(const std::exception& error) {set_preset_status(QString::fromUtf8(error.what()));}
    });
    connect(presets_save_,&QPushButton::clicked,this,[this,preset_context_current,set_preset_status]{
        const auto generation=effects_generation_;const auto session=effects_session_;const auto target=effects_target_id_;
        const auto revision=effects_revision_;
        const auto object=host.session.document().objects.find(target);
        if(object==host.session.document().objects.end()) {set_preset_status("Select a Path or Text with supported processing entries.");return;}
        bool accepted=false;
        const auto label=QInputDialog::getText(this,"Save Preset","Preset name:",QLineEdit::Normal,{},&accepted);
        if(!accepted)return;
        try {
            preset_context_current(generation,session,target,revision);
            QStringList source_operations;
            for(const auto& operation:host.session.document().objects.at(target).stack) {
                if(operation.macro)source_operations.push_back(qs(operation.id)+" (Macro "+qs(operation.macro->definition)+
                    "@"+QString::number(operation.macro->pinned_revision)+")");
                else source_operations.push_back(qs(operation.id)+" ("+qs(operation.type)+")");
            }
            PresetDefinition metadata;metadata.id=new_id();metadata.schema_version=2;metadata.label=label.toStdString();
            host.session.apply_preset_command(PresetCommand{CreatePresetFromStack{metadata,target}},revision);
            host.edited();set_preset_status("Saved “"+label+"” from these source operations: "+source_operations.join(" → ")+".");
        } catch(const Error& error) {
            auto message=qs(error.code)+": "+QString::fromUtf8(error.what());
            for(const auto& ref:error.references)message+=" · Ref{object:\""+qs(ref.object)+"\",point:\""+
                qs(ref.point)+"\",field:\""+qs(ref.field)+"\"}";
            set_preset_status(message);
        } catch(const std::exception& error) {set_preset_status(QString::fromUtf8(error.what()));}
    });
    connect(presets_apply_,&QPushButton::clicked,this,[this,preset_context_current,set_preset_status]{
        const auto generation=effects_generation_;const auto session=effects_session_;const auto target=effects_target_id_;
        const auto revision=effects_revision_;const auto* item=presets_catalog_->currentItem();
        if(!item){set_preset_status("Choose a document preset to apply.");return;}
        const auto preset=item->data(Qt::UserRole).toString().toStdString();const auto label=item->text();
        try {
            preset_context_current(generation,session,target,revision);
            host.session.apply_preset_command(PresetCommand{ApplyPreset{preset,target,new_id()}},revision);
            host.edited();set_preset_status("Applied “"+label+"” as fresh ordered processing entries.");
        } catch(const Error& error) {set_preset_status(qs(error.code)+": "+QString::fromUtf8(error.what()));}
        catch(const std::exception& error) {set_preset_status(QString::fromUtf8(error.what()));}
    });
    connect(presets_rename_,&QPushButton::clicked,this,[this,preset_context_current,set_preset_status]{
        const auto generation=effects_generation_;const auto session=effects_session_;const auto target=effects_target_id_;
        const auto revision=effects_revision_;const auto* item=presets_catalog_->currentItem();
        if(!item){set_preset_status("Choose a document preset to rename.");return;}
        const auto id=item->data(Qt::UserRole).toString().toStdString();const auto old_label=item->text();bool accepted=false;
        const auto label=QInputDialog::getText(this,"Rename Preset","Preset name:",QLineEdit::Normal,old_label,&accepted);
        if(!accepted)return;
        try {
            preset_context_current(generation,session,target,revision);
            host.session.apply_preset_command(PresetCommand{RenamePreset{id,label.toStdString()}},revision);
            host.edited();set_preset_status("Renamed preset to “"+label+"”.");
        } catch(const Error& error) {set_preset_status(qs(error.code)+": "+QString::fromUtf8(error.what()));}
        catch(const std::exception& error) {set_preset_status(QString::fromUtf8(error.what()));}
    });
    connect(presets_update_,&QPushButton::clicked,this,[this,preset_context_current,set_preset_status]{
        const auto generation=effects_generation_;const auto session=effects_session_;const auto target=effects_target_id_;
        const auto revision=effects_revision_;const auto* item=presets_catalog_->currentItem();
        if(!item){set_preset_status("Choose a document preset to update.");return;}
        const auto id=item->data(Qt::UserRole).toString().toStdString();const auto label=item->text();
        try {
            preset_context_current(generation,session,target,revision);
            const auto& current=host.session.document().preset_definitions.at(id);
            auto metadata=current;metadata.schema_version=2;metadata.entries.clear();
            auto definition=capture_preset_definition(host.session.document(),std::move(metadata),target);
            QStringList source_operations;
            for(const auto& operation:host.session.document().objects.at(target).stack) {
                if(operation.macro)source_operations.push_back(qs(operation.id)+" (Macro "+qs(operation.macro->definition)+
                    "@"+QString::number(operation.macro->pinned_revision)+")");
                else source_operations.push_back(qs(operation.id)+" ("+qs(operation.type)+")");
            }
            host.session.apply_preset_command(PresetCommand{UpdatePreset{std::move(definition)}},revision);
            host.edited();set_preset_status("Updated “"+label+"” from source operations "+source_operations.join(" → ")+"; existing applied snapshots are unchanged.");
        } catch(const Error& error) {
            auto message=qs(error.code)+": "+QString::fromUtf8(error.what());
            for(const auto& ref:error.references)message+=" · Ref{object:\""+qs(ref.object)+"\",point:\""+
                qs(ref.point)+"\",field:\""+qs(ref.field)+"\"}";
            set_preset_status(message);
        } catch(const std::exception& error) {set_preset_status(QString::fromUtf8(error.what()));}
    });
    connect(presets_delete_,&QPushButton::clicked,this,[this,preset_context_current,set_preset_status]{
        const auto generation=effects_generation_;const auto session=effects_session_;const auto target=effects_target_id_;
        const auto revision=effects_revision_;const auto* item=presets_catalog_->currentItem();
        if(!item){set_preset_status("Choose a document preset to delete.");return;}
        const auto id=item->data(Qt::UserRole).toString().toStdString();const auto label=item->text();
        try {
            preset_context_current(generation,session,target,revision);
            host.session.apply_preset_command(PresetCommand{DeletePreset{id}},revision);
            host.edited();set_preset_status("Deleted “"+label+"”. Undo restores its definition.");
        } catch(const Error& error) {set_preset_status(qs(error.code)+": "+QString::fromUtf8(error.what()));}
        catch(const std::exception& error) {set_preset_status(QString::fromUtf8(error.what()));}
    });
    resizeDocks({structure,right},{215,320},Qt::Horizontal);
    auto* file=menuBar()->addMenu("&File");
    auto* edit=menuBar()->addMenu("&Edit");
    auto* add=menuBar()->addMenu("&Add");
    auto* view=menuBar()->addMenu("&View");
    auto* library_menu=menuBar()->addMenu("&Library");
    auto* definitions=menuBar()->addMenu("&Definitions");
    auto* collections_menu=menuBar()->addMenu("&Collections");
    auto action=[this](QMenu* menu,const QString& label,const QKeySequence& shortcut,auto fn) {
        auto* a=menu->addAction(label); a->setShortcut(shortcut);
        connect(a,&QAction::triggered,this,[this,fn]{perform(fn);}); return a;
    };
    action(library_menu,"Folder Library…",{},[this]{show_folder_library();})->setObjectName("folder-library");
    action(definitions,"Create Definition from selected Group…",{},[this]{create_definition_from_selection();})
        ->setObjectName("create-definition-from-selection");
    action(definitions,"Rename Definition…",{},[this]{rename_definition();})
        ->setObjectName("rename-definition");
    action(definitions,"Place Definition Instance…",{},[this]{place_definition_instance();})
        ->setObjectName("place-definition-instance");
    action(definitions,"Set selected Instance override…",{},[this]{set_instance_override();})
        ->setObjectName("set-instance-override");
    action(definitions,"Reset selected Instance override…",{},[this]{reset_instance_override();})
        ->setObjectName("reset-instance-override");
    action(definitions,"Detach selected Instance",{},[this]{detach_instance();})
        ->setObjectName("detach-instance");
    action(definitions,"Delete Definition…",{},[this]{delete_definition();})
        ->setObjectName("delete-definition");
    action(collections_menu,"Browse Collections…",{},[this]{browse_collections();})
        ->setObjectName("browse-collections");
    action(collections_menu,"Create from selection…",{},[this]{create_collection_from_selection();})
        ->setObjectName("create-collection-from-selection");
    action(collections_menu,"Rename Collection…",{},[this]{rename_collection();})
        ->setObjectName("rename-collection");
    action(collections_menu,"Add selection to Collection…",{},[this]{add_selection_to_collection();})
        ->setObjectName("add-selection-to-collection");
    action(collections_menu,"Remove selection from Collection…",{},[this]{remove_selection_from_collection();})
        ->setObjectName("remove-selection-from-collection");
    action(collections_menu,"Delete Collection…",{},[this]{delete_collection();})
        ->setObjectName("delete-collection");
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
    batch_rename_action_=action(edit,"Batch rename selected…",{},[this]{batch_rename_selection();});
    batch_rename_action_->setObjectName("batch-rename-selection");
    batch_rename_action_->setEnabled(false);
    sort_paint_order_action_=action(edit,"Sort selected by name (paint order)…",{},[this]{sort_selection_by_name_paint_order();});
    sort_paint_order_action_->setObjectName("sort-selected-name-paint-order");
    sort_paint_order_action_->setEnabled(false);
    action(edit,"Create Folder",{},[this]{create_folder();})->setObjectName("create-folder");
    action(edit,"Create Folder from selected…",{},[this]{create_folder_from_selection();})->setObjectName("create-folder-from-selection");
    action(edit,"Ungroup selected Groups",QKeySequence("Ctrl+Shift+G"),[this]{ungroup_selection();})->setObjectName("ungroup-objects");
    action(edit,"Move selected out of Folder",{},[this]{move_selection_out();})->setObjectName("move-out-of-folder");
    action(edit,"Move selected to next Folder",{},[this]{move_selection_to_next_folder();})->setObjectName("move-to-next-folder");
    action(edit,"Move selected to previous Folder",{},[this]{move_selection_to_previous_folder();})->setObjectName("move-to-previous-folder");
    action(edit,"Move selected to Folder…",{},[this]{move_selection_to_folder();})->setObjectName("move-to-folder");
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
    auto* ellipse=action(add,"Ellipse",{},[this]{add_primitive("nect.shape.ellipse");});
    ellipse->setObjectName("add-ellipse");
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
    utility_guides_->setIcon(utility_toggle_icon(UtilityToggleGlyph::guides));
    utility_grid_->setIcon(utility_toggle_icon(UtilityToggleGlyph::grid));
    // Keep the icon on the owner action: subsequent text/checked updates must
    // not restore an empty default-action icon on the Snap tool button.
    snap->setIcon(utility_toggle_icon(UtilityToggleGlyph::snap));
    for(auto* button:{utility_guides_,utility_grid_,utility_snap_}) {
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);button->setIconSize(QSize(20,20));
        button->setFixedSize(36,32);
        button->setStyleSheet(button->styleSheet()+"QToolButton{padding:4px;}");
    }
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
    view->addAction(effects_dock_->toggleViewAction());
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
        const auto selected=canvas->selected_object;
        const auto found=host.session.document().objects.find(selected);
        if(found!=host.session.document().objects.end()&&found->second.text&&found->second.text->path_attachment) {
            const auto session=host.session_id;
            QTimer::singleShot(0,this,[this,session,selected]{
                const auto current=host.session.document().objects.find(selected);
                if(host.session_id==session&&canvas->selected_object==selected&&
                   current!=host.session.document().objects.end()&&current->second.text&&current->second.text->path_attachment)
                    rebuild_inspector(true);
            });
        }
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
    canvas->selection_changed=[this]{if(!canvas->selected_object.empty())artboard_editing_=false;sync_tree_selection();rebuild_inspector();rebuild_effects_panel();update_batch_rename_action();update_sort_paint_order_action();};
    canvas->active_artboard_changed=[this]{if(!refreshing_)refresh();};
    canvas->view_state_changed=[this]{sync_utility_view_state();};
    canvas->zoom_changed=[this](double zoom){
        if(!utility_zoom_)return;const QSignalBlocker blocker(utility_zoom_);utility_zoom_->setValue(zoom*100.0);
    };
    canvas->gradient_edit_changed=[this]{rebuild_inspector();};
    canvas->circle_source_edit_changed=[this](bool){rebuild_inspector();};
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
    cancel_angle_adapters(true);
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
    QTimer::singleShot(0,this,[this]{refresh();if(utility_setup_dialog_)rebuild_layout_setup();});
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
    const auto visibility=evaluate_object_visibilities(d);
    QString signature=host.session_id;
    std::function<void(const Id&)> fingerprint=[&](const Id& id) {
        const auto& o=d.objects.at(id);
        signature+="("+qs(id)+":"+QString::number(o.name.size())+":"+qs(o.name)+"|visible:"+QString::number(o.visible)+
            "|evaluated-visible:"+QString::number(visibility.at(id));
        if(o.visibility_driver)signature+="|visibility-driver:"+qs(o.visibility_driver->object);
        if(o.visibility_expression)signature+="|visibility-expression:"+qs(o.visibility_expression->source);
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
        item->setText(0,qs(o.name)+(visibility.at(id)?QString{}:QString(" ◌"))+
            (o.visibility_driver?QString(" ⇢"):o.visibility_expression?QString(" ƒ"):QString{})+
            (o.compositing.mask?QString(" [mask]"):QString{})+(o.transform_parent?QString(" ↗"):QString{}));item->setData(0,Qt::UserRole,qs(id));
        QString tooltip=(o.source?primitive_label(*o.source)+" source · ":QString{})+qs(id);
        tooltip+="\nVisibility authored: "+(o.visible?QString("true"):QString("false"));
        tooltip+=" · evaluated own: "+(visibility.at(id)?QString("true"):QString("false"));
        if(o.visibility_driver) {
            const auto& source=d.objects.at(o.visibility_driver->object);
            tooltip+=" · linked to "+qs(source.name)+" ("+qs(source.id)+")";
        }
        if(o.visibility_expression)tooltip+=" · expression "+qs(o.visibility_expression->source);
        if(o.transform_parent)tooltip+="\nTransform follows "+qs(d.objects.at(*o.transform_parent).name)+" · "+qs(*o.transform_parent);
        item->setToolTip(0,tooltip);
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
    rebuild_effects_panel();
    color_tools_->refresh();
    refresh_history();
    update_utility_strip();
}

void Window::rebuild_effects_panel() {
    if(!effects_catalog_||!effects_status_||!effects_operations_layout_||!presets_catalog_||!presets_status_)return;
    ++effects_generation_;
    effects_session_=host.session_id;
    effects_target_id_=canvas->selected_object;
    effects_revision_=host.session.revision();
    const auto& document=host.session.document();

    const auto selected_key=effects_catalog_->currentItem()?effects_catalog_->currentItem()->data(Qt::UserRole).toString():QString{};
    std::set<QString> macro_ids;
    for(int i=effects_catalog_->count()-1;i>=0;--i) {
        auto* item=effects_catalog_->item(i);
        if(item->data(Qt::UserRole+1).toString()!="macro")continue;
        const auto id=item->data(Qt::UserRole).toString().mid(6);
        const auto definition=document.macro_definitions.find(id.toStdString());
        if(definition==document.macro_definitions.end()) {
            delete effects_catalog_->takeItem(i);
            continue;
        }
        item->setText(qs(definition->second.label));
        item->setToolTip("Macro definition · "+qs(definition->second.id)+" · latest revision "+
            QString::number(definition->second.latest_revision)+" · local_paths_and_paint");
        macro_ids.insert(id);
    }
    for(const auto& [id,definition]:document.macro_definitions)if(!macro_ids.contains(qs(id))) {
        auto* item=new QListWidgetItem(qs(definition.label),effects_catalog_);
        item->setData(Qt::UserRole,"macro:"+qs(id));item->setData(Qt::UserRole+1,"macro");
        item->setToolTip("Macro definition · "+qs(id)+" · latest revision "+
            QString::number(definition.latest_revision)+" · local_paths_and_paint");
    }
    if(!selected_key.isEmpty())for(int i=0;i<effects_catalog_->count();++i)
        if(effects_catalog_->item(i)->data(Qt::UserRole).toString()==selected_key) {
            const QSignalBlocker blocker(effects_catalog_);effects_catalog_->setCurrentRow(i);break;
        }

    const auto query=effects_search_?effects_search_->text().trimmed():QString{};
    QListWidgetItem* first_match=nullptr;
    for(int i=0;i<effects_catalog_->count();++i) {
        auto* item=effects_catalog_->item(i);
        const bool matches=item->text().contains(query,Qt::CaseInsensitive)||item->data(Qt::UserRole).toString().contains(query,Qt::CaseInsensitive);
        item->setHidden(!matches);
        if(matches&&!first_match)first_match=item;
    }
    if(first_match&&(!effects_catalog_->currentItem()||effects_catalog_->currentItem()->isHidden())) {
        const QSignalBlocker blocker(effects_catalog_);
        effects_catalog_->setCurrentItem(first_match);
    }
    auto* entry=effects_catalog_->currentItem();
    const auto effect_type=entry?entry->data(Qt::UserRole).toString():QString{};
    const auto effect_name=entry?entry->text():QStringLiteral("Unavailable effect");
    const bool macro_effect=effect_type.startsWith("macro:");
    const bool matches=entry&&!entry->isHidden();
    effects_apply_->setText("Apply "+effect_name);
    if(auto* empty=effects_dock_->findChild<QLabel*>("effects-no-results")) {
        empty->setVisible(!matches);
        if(!first_match)empty->setText("No supported effect matches “"+query+"”.");
    }
    effects_apply_->setEnabled(matches);
    if(effects_favorite_)effects_favorite_->setEnabled(matches&&!macro_effect&&entry&&entry->data(Qt::UserRole+1).toString()=="builtin");

    const auto selected=document.objects.find(effects_target_id_);
    QString target_text="Target: none";
    QString state_text;
    auto state_prefix=effect_type.isEmpty()?QStringLiteral("effect"):effect_type;
    if(effects_target_id_.empty()) {
        state_text="TARGET_UNAVAILABLE · "+state_prefix+" · Select a target for this effect.";
    } else if(selected==document.objects.end()) {
        target_text="Unavailable target: missing object ["+qs(effects_target_id_)+"]";
        state_text="TARGET_UNAVAILABLE · "+state_prefix+" · The selected object no longer exists in this document.";
    } else {
        const auto& object=selected->second;
        const auto identity=qs(object.name)+" ["+qs(object.id)+"]";
        const auto kind=object.kind==Kind::group?QStringLiteral("Group"):
            object.text?QStringLiteral("Text"):object.source?primitive_label(*object.source):
            object.kind==Kind::image?QStringLiteral("Image"):QStringLiteral("Path");
        target_text=(effect_type=="nect.group.posterize"&&object.kind!=Kind::group?QStringLiteral("Unavailable target: "):QStringLiteral("Target: "))+
            identity+" · "+kind;
        if(macro_effect) {
            if(object.kind==Kind::path||object.kind==Kind::text) {
                const auto definition_id=effect_type.mid(6).toStdString();
                const auto definition=document.macro_definitions.find(definition_id);
                if(definition!=document.macro_definitions.end())
                    state_text="Ready · Macro "+qs(definition->second.id)+" latest revision "+
                        QString::number(definition->second.latest_revision)+" · local_paths_and_paint stack entry.";
                else state_text="Unavailable Macro definition · refresh the document and choose an available Macro.";
            } else state_text="TARGET_UNAVAILABLE · Macro effects require a Path or Text target.";
        } else if(effect_type=="nect.group.posterize") {
            if(object.kind==Kind::group)state_text="Ready · nect.group.posterize v1 · Postchildren Group pixels.";
            else state_text="TARGET_UNAVAILABLE · nect.group.posterize · Select a Group; "+kind+" targets are unsupported.";
        } else if(effect_type=="nect.shape.offset") {
            if(object.kind==Kind::path||object.kind==Kind::text)
                state_text="Ready · nect.shape.offset v1 · Object-local closed-path processing.";
            else state_text="TARGET_UNAVAILABLE · nect.shape.offset · "+kind+
                " shape stacks are unsupported; select a Path, primitive, or Text.";
        } else {
            state_text="TARGET_UNAVAILABLE · "+state_prefix+" · Choose a supported built-in effect.";
        }
    }
    effects_target_->setText(target_text);
    effects_status_->setText(first_match?state_text:"No supported effect matches “"+query+"”.");

    while(auto* item=effects_operations_layout_->takeAt(0)) {
        if(auto* widget=item->widget()) {widget->hide();widget->deleteLater();}
        delete item;
    }
    if(selected==document.objects.end()) {
        auto* empty=new QLabel("No applied effect instances for this target.",effects_operations_);
        empty->setObjectName("effects-no-applied");empty->setWordWrap(true);effects_operations_layout_->addWidget(empty);
    } else {
        const auto& object=selected->second;
        std::size_t count=0;
        for(const auto& operation:object.stack)if(operation.type=="nect.shape.offset"||operation.type=="nect.group.posterize"||operation.macro) {
            ++count;
            const auto generation=effects_generation_;const auto session=effects_session_;
            const auto target=effects_target_id_;const auto revision=effects_revision_;const auto operation_id=operation.id;
            const auto operation_type=qs(operation.type);
            const auto operation_name=operation.macro?qs(document.macro_definitions.at(operation.macro->definition).label):operation_label(operation);
            const auto expected_macro=operation.macro;
            const auto panel_target_label=effects_target_->text();
            const auto card_title=operation.macro?operation_name+" · Macro revision v"+
                QString::number(operation.macro->pinned_revision):operation_name+" · behavior v"+QString::number(operation.version);
            auto* card=new QGroupBox(card_title,effects_operations_);
            card->setObjectName("effects-operation-"+qs(operation.id));
            auto* card_layout=new QVBoxLayout(card);
            auto* identity=new QLabel("Instance ID: "+qs(operation.id),card);
            identity->setObjectName("effects-operation-id-"+qs(operation.id));
            identity->setTextFormat(Qt::PlainText);identity->setWordWrap(true);card_layout->addWidget(identity);
            auto* edit=new QPushButton("Edit in Properties",card);
            edit->setObjectName("effects-edit-properties-"+qs(operation.id));
            edit->setToolTip("Open the normal parameter, reorder, bypass and remove controls in Properties.");
            card_layout->addWidget(edit);effects_operations_layout_->addWidget(card);
            connect(edit,&QPushButton::clicked,this,[this,generation,session,target,revision,operation_id,operation_type,operation_name,expected_macro,panel_target_label]{
                try {
                    if(generation!=effects_generation_)throw Error("REVISION_CONFLICT","Effects panel changed; reopen the current effect instance");
                    if(host.session_id!=session)throw Error("SESSION_CONFLICT","Effect belongs to another document");
                    if(canvas->selected_object!=target)throw Error("TARGET_CONFLICT","Effects target changed; select the original target again");
                    if(host.session.revision()!=revision)throw Error("REVISION_CONFLICT","Effect changed elsewhere; refresh the panel before opening it");
                    const auto object=host.session.document().objects.find(target);
                    if(object==host.session.document().objects.end())throw Error("MISSING_OBJECT",target);
                    const auto current=std::find_if(object->second.stack.begin(),object->second.stack.end(),[&](const auto& item) {
                        return item.id==operation_id;
                    });
                    if(current==object->second.stack.end()||qs(current->type)!=operation_type||current->macro!=expected_macro)
                        throw Error("INVALID_OPERATOR","The selected instance is no longer "+operation_name.toStdString());
                    auto* properties=findChild<QDockWidget*>("properties");
                    if(!properties)throw Error("MISSING_PANEL","Properties panel is unavailable");
                    properties->show();properties->raise();
                    effects_status_->setText("Opened "+operation_name+" instance "+qs(operation_id)+" in Properties.");
                    QTimer::singleShot(0,this,[this,generation,session,target,revision,operation_id]{
                        if(generation!=effects_generation_||host.session_id!=session||canvas->selected_object!=target||host.session.revision()!=revision)return;
                        reveal_operation(operation_id);
                    });
                } catch(const Error& error) {
                    const auto message=panel_target_label+" · "+operation_type+" · "+
                        qs(error.code)+": "+QString::fromUtf8(error.what());
                    effects_status_->setText(message);statusBar()->showMessage(message,12000);
                }
            });
        }
        if(count==0) {
            auto* empty=new QLabel("No applied effect instances for this target.",effects_operations_);
            empty->setObjectName("effects-no-applied");empty->setWordWrap(true);effects_operations_layout_->addWidget(empty);
        }
    }
    effects_operations_layout_->addStretch();

    const auto* old_preset_item=presets_catalog_->currentItem();
    const auto old_preset_id=old_preset_item?old_preset_item->data(Qt::UserRole).toString():QString{};
    const auto preset_query=presets_search_?presets_search_->text().trimmed():QString{};
    QListWidgetItem* matching_preset=nullptr;
    {
        const QSignalBlocker blocker(presets_catalog_);
        presets_catalog_->clear();
        for(const auto& [id,definition]:document.preset_definitions) {
            QString searchable=qs(id)+" "+qs(definition.label)+" "+qs(definition.category);
            for(const auto& tag:definition.tags)searchable+=" "+qs(tag);
            if(!preset_query.isEmpty()&&!searchable.contains(preset_query,Qt::CaseInsensitive))continue;
            auto* item=new QListWidgetItem(qs(definition.label),presets_catalog_);
            item->setData(Qt::UserRole,qs(id));
            const auto entry_summary=QString("%1 entries · schema v%2 · %3")
                .arg(definition.entries.size()).arg(definition.schema_version).arg(qs(definition.target_domain));
            item->setToolTip("ID: "+qs(id)+"\nCategory: "+qs(definition.category)+"\n"+entry_summary+
                "\nOrdered literal processing snapshot · editing this definition does not change applied stacks");
            if(id==old_preset_id.toStdString())matching_preset=item;
            if(!matching_preset)matching_preset=item;
        }
        if(matching_preset)presets_catalog_->setCurrentItem(matching_preset);
    }
    const auto selected_preset=presets_catalog_->currentItem();
    const bool target_supports_presets=selected!=document.objects.end()&&
        (selected->second.kind==Kind::path||selected->second.kind==Kind::text);
    const bool has_preset=selected_preset!=nullptr;
    presets_save_->setEnabled(target_supports_presets);
    presets_publish_->setEnabled(has_preset);
    presets_apply_->setEnabled(has_preset&&target_supports_presets);
    presets_rename_->setEnabled(has_preset);
    presets_update_->setEnabled(has_preset&&target_supports_presets);
    presets_delete_->setEnabled(has_preset);
    if(!has_preset) {
        presets_status_->setText(document.preset_definitions.empty()?
            "No document presets yet. Save a selected Path or Text with supported literal built-in and Macro entries.":
            "No presets match “"+preset_query+"”.");
    } else {
        const auto id=selected_preset->data(Qt::UserRole).toString().toStdString();
        const auto& definition=document.preset_definitions.at(id);
        if(target_supports_presets)
            presets_status_->setText("Selected “"+qs(definition.label)+"” · applies to "+qs(selected->second.name)+
                " · "+QString::number(definition.entries.size())+" ordered entries · append only");
        else presets_status_->setText("Selected “"+qs(definition.label)+"” · choose a Path or Text target to apply or update it.");
    }
}

void Window::sync_utility_view_state() {
    for(auto* box:findChildren<QCheckBox*>("guide-edit-mode")) {
        const QSignalBlocker blocker(box);box->setChecked(canvas->guide_edit_mode());
    }
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
    if(duplicate&&selected.template_assignment&&selected.template_assignment->content_instance) {
        const auto resolved=evaluate_artboard(comp,selected.id);
        double right=resolved.x+resolved.width;
        for(const auto& entry:comp.artboards) {
            const auto frame=evaluate_artboard(comp,entry.id);
            right=std::max(right,frame.x+frame.width);
        }
        auto prefix=new_id();std::erase(prefix,'-');
        const auto composition=comp.id,source=selected.id,target=prefix+"-artboard";
        const auto index=static_cast<std::size_t>(std::distance(comp.artboards.begin(),
            std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const auto& item){return item.id==source;})))+1;
        host.session.apply({ArtboardTemplateCommand{DuplicateTemplateArtboard{
            composition,source,prefix,right+40,resolved.y,index}}},host.session.revision());
        canvas->set_selection({});artboard_editing_=true;
        canvas->set_active_artboard(composition,target);host.edited();return;
    }
    auto board=duplicate?selected:evaluate_artboard(comp,selected.id);
    std::vector<ArtboardGuide> copied_local_guides;
    if(duplicate)for(const auto& guide:selected.local_guides) {
        auto copied=guide;copied.id=new_id();copied_local_guides.push_back(std::move(copied));
    }
    // AddArtboard accepts only frame/layout/Template state. Authored local
    // Guides use their typed command path and copied Guides get fresh IDs.
    board.local_guides.clear();
    if(!duplicate){
        board.parent_size.reset();board.layout.reset();board.width_driver.reset();board.height_driver.reset();
        board.template_assignment.reset();
    }
    double right=board.x+board.width;
    for(const auto& entry:comp.artboards) {const auto resolved=evaluate_artboard(comp,entry.id);right=std::max(right,resolved.x+resolved.width);}
    board.id=new_id();board.name=duplicate?selected.name+" copy":"Artboard "+std::to_string(comp.artboards.size()+1);
    if(duplicate&&board.template_assignment) {
        board.template_assignment->grid_id=new_id();
        if(board.layout&&board.layout->grid)board.layout->grid->id=board.template_assignment->grid_id;
    } else if(duplicate&&board.layout&&board.layout->grid)board.layout->grid->id=new_id();
    board.x=right+40;
    const auto index=static_cast<std::size_t>(std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const auto& entry){return entry.id==selected.id;})-comp.artboards.begin())+1;
    const auto comp_id=comp.id,board_id=board.id;
    const auto width_driver=board.width_driver,height_driver=board.height_driver;
    const auto margin_left_driver=duplicate&&board.layout&&board.layout->margin?
        board.layout->margin->left_driver:std::optional<Ref>{};
    const auto margin_left_expression=duplicate&&board.layout&&board.layout->margin?
        board.layout->margin->left_expression:std::optional<Expression>{};
    const auto margin_top_driver=duplicate&&board.layout&&board.layout->margin?
        board.layout->margin->top_driver:std::optional<Ref>{};
    const auto margin_top_expression=duplicate&&board.layout&&board.layout->margin?
        board.layout->margin->top_expression:std::optional<Expression>{};
    const auto margin_right_driver=duplicate&&board.layout&&board.layout->margin?
        board.layout->margin->right_driver:std::optional<Ref>{};
    const auto margin_right_expression=duplicate&&board.layout&&board.layout->margin?
        board.layout->margin->right_expression:std::optional<Expression>{};
    const auto margin_bottom_driver=duplicate&&board.layout&&board.layout->margin?
        board.layout->margin->bottom_driver:std::optional<Ref>{};
    const auto margin_bottom_expression=duplicate&&board.layout&&board.layout->margin?
        board.layout->margin->bottom_expression:std::optional<Expression>{};
    const auto grid_bounds_x_driver=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->bounds_x_driver:std::optional<Ref>{};
    const auto grid_bounds_x_expression=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->bounds_x_expression:std::optional<Expression>{};
    const auto grid_bounds_y_driver=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->bounds_y_driver:std::optional<Ref>{};
    const auto grid_bounds_y_expression=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->bounds_y_expression:std::optional<Expression>{};
    const auto grid_bounds_width_driver=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->bounds_width_driver:std::optional<Ref>{};
    const auto grid_bounds_width_expression=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->bounds_width_expression:std::optional<Expression>{};
    const auto grid_bounds_height_driver=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->bounds_height_driver:std::optional<Ref>{};
    const auto grid_bounds_height_expression=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->bounds_height_expression:std::optional<Expression>{};
    const auto grid_column_gutter_driver=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->column_gutter_driver:std::optional<Ref>{};
    const auto grid_column_gutter_expression=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->column_gutter_expression:std::optional<Expression>{};
    const auto grid_columns_driver=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->columns_driver:std::optional<Ref>{};
    const auto grid_columns_expression=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->columns_expression:std::optional<Expression>{};
    const auto grid_rows_driver=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->rows_driver:std::optional<Ref>{};
    const auto grid_rows_expression=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->rows_expression:std::optional<Expression>{};
    const auto grid_row_gutter_driver=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->row_gutter_driver:std::optional<Ref>{};
    const auto grid_row_gutter_expression=duplicate&&board.layout&&board.layout->grid?
        board.layout->grid->row_gutter_expression:std::optional<Expression>{};
    if(duplicate&&board.layout&&board.layout->margin) {
        board.layout->margin->left_driver.reset();board.layout->margin->left_expression.reset();
        board.layout->margin->top_driver.reset();board.layout->margin->top_expression.reset();
        board.layout->margin->right_driver.reset();board.layout->margin->right_expression.reset();
        board.layout->margin->bottom_driver.reset();
        board.layout->margin->bottom_expression.reset();
    }
    if(duplicate&&board.layout&&board.layout->grid) {
        board.layout->grid->columns_driver.reset();
        board.layout->grid->columns_expression.reset();
        board.layout->grid->rows_driver.reset();
        board.layout->grid->rows_expression.reset();
        board.layout->grid->column_gutter_driver.reset();
        board.layout->grid->column_gutter_expression.reset();
        board.layout->grid->row_gutter_driver.reset();
        board.layout->grid->row_gutter_expression.reset();
        board.layout->grid->bounds_x_driver.reset();board.layout->grid->bounds_x_expression.reset();
        board.layout->grid->bounds_y_driver.reset();
        board.layout->grid->bounds_y_expression.reset();
        board.layout->grid->bounds_width_driver.reset();
        board.layout->grid->bounds_width_expression.reset();
        board.layout->grid->bounds_height_driver.reset();
        board.layout->grid->bounds_height_expression.reset();
    }
    board.width_driver.reset();board.height_driver.reset();
    std::vector<Command> commands{AddArtboard{comp_id,board,index}};
    for(const auto& guide:copied_local_guides)
        commands.push_back(ArtboardGuideCommand{AddArtboardGuide{comp_id,board_id,guide}});
    const auto add_driver=[&](bool width,const std::optional<Artboard::SizeDriver>& driver) {
        if(!driver)return;
        const Ref target{board_id,"",width?"artboard.width":"artboard.height"};
        if(const auto* link=std::get_if<Ref>(&driver->value))commands.push_back(LinkArtboardSize{target,*link,false});
        else commands.push_back(SetArtboardSizeExpression{target,std::get<Expression>(driver->value),false});
    };
    add_driver(true,width_driver);add_driver(false,height_driver);
    if(margin_left_driver)commands.push_back(MarginLeftCommand{LinkMarginLeft{{board_id,"","margin.left"},*margin_left_driver,false}});
    else if(margin_left_expression)commands.push_back(MarginLeftCommand{SetMarginLeftExpression{{board_id,"","margin.left"},*margin_left_expression,false}});
    if(margin_top_driver)commands.push_back(MarginTopCommand{LinkMarginTop{{board_id,"","margin.top"},*margin_top_driver,false}});
    else if(margin_top_expression)commands.push_back(MarginTopCommand{SetMarginTopExpression{
        {board_id,"","margin.top"},*margin_top_expression,false}});
    if(margin_right_driver)commands.push_back(MarginRightCommand{LinkMarginRight{{board_id,"","margin.right"},*margin_right_driver,false}});
    else if(margin_right_expression)commands.push_back(MarginRightCommand{SetMarginRightExpression{
        {board_id,"","margin.right"},*margin_right_expression,false}});
    if(margin_bottom_driver)commands.push_back(MarginBottomCommand{LinkMarginBottom{
        {board_id,"","margin.bottom"},*margin_bottom_driver,false}});
    else if(margin_bottom_expression)commands.push_back(MarginBottomCommand{SetMarginBottomExpression{
        {board_id,"","margin.bottom"},*margin_bottom_expression,false}});
    if(grid_columns_driver)commands.push_back(GridColumnsCommand{LinkGridColumns{
        {board.layout->grid->id,"","grid.columns"},*grid_columns_driver,false}});
    else if(grid_columns_expression)commands.push_back(GridColumnsCommand{SetGridColumnsExpression{
        {board.layout->grid->id,"","grid.columns"},*grid_columns_expression,false}});
    if(grid_rows_driver)commands.push_back(GridRowsCommand{LinkGridRows{
        {board.layout->grid->id,"","grid.rows"},*grid_rows_driver,false}});
    else if(grid_rows_expression)commands.push_back(GridRowsCommand{SetGridRowsExpression{
        {board.layout->grid->id,"","grid.rows"},*grid_rows_expression,false}});
    if(grid_bounds_x_driver)commands.push_back(GridBoundsXCommand{LinkGridBoundsX{{board.layout->grid->id,"","grid.bounds.x"},*grid_bounds_x_driver,false}});
    else if(grid_bounds_x_expression)commands.push_back(GridBoundsXCommand{SetGridBoundsXExpression{{board.layout->grid->id,"","grid.bounds.x"},*grid_bounds_x_expression,false}});
    if(grid_bounds_y_driver)commands.push_back(GridBoundsYCommand{LinkGridBoundsY{{board.layout->grid->id,"","grid.bounds.y"},*grid_bounds_y_driver,false}});
    else if(grid_bounds_y_expression)commands.push_back(GridBoundsYCommand{SetGridBoundsYExpression{{board.layout->grid->id,"","grid.bounds.y"},*grid_bounds_y_expression,false}});
    if(grid_bounds_width_driver)commands.push_back(GridBoundsWidthCommand{LinkGridBoundsWidth{
        {board.layout->grid->id,"","grid.bounds.width"},*grid_bounds_width_driver,false}});
    else if(grid_bounds_width_expression)commands.push_back(GridBoundsWidthCommand{SetGridBoundsWidthExpression{
        {board.layout->grid->id,"","grid.bounds.width"},*grid_bounds_width_expression,false}});
    if(grid_bounds_height_driver)commands.push_back(GridBoundsHeightCommand{LinkGridBoundsHeight{
        {board.layout->grid->id,"","grid.bounds.height"},*grid_bounds_height_driver,false}});
    else if(grid_bounds_height_expression)commands.push_back(GridBoundsHeightCommand{SetGridBoundsHeightExpression{
        {board.layout->grid->id,"","grid.bounds.height"},*grid_bounds_height_expression,false}});
    if(grid_column_gutter_driver)commands.push_back(GridColumnGutterCommand{LinkGridColumnGutter{
        {board.layout->grid->id,"","grid.column_gutter"},*grid_column_gutter_driver,false}});
    else if(grid_column_gutter_expression)commands.push_back(GridColumnGutterCommand{SetGridColumnGutterExpression{
        {board.layout->grid->id,"","grid.column_gutter"},*grid_column_gutter_expression,false}});
    if(grid_row_gutter_driver)commands.push_back(GridRowGutterCommand{LinkGridRowGutter{
        {board.layout->grid->id,"","grid.row_gutter"},*grid_row_gutter_driver,false}});
    else if(grid_row_gutter_expression)commands.push_back(GridRowGutterCommand{SetGridRowGutterExpression{
        {board.layout->grid->id,"","grid.row_gutter"},*grid_row_gutter_expression,false}});
    host.session.apply(commands,host.session.revision());
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
    const auto guide_positions=evaluate_guide_positions(host.session.document(),comp.id);
    const auto& board=find_artboard(comp,canvas->active_artboard());
    const auto resolved=evaluate_artboard(comp,board.id);
    const auto composition=comp.id,id=board.id;const auto frozen_session=host.session_id;
    const auto frozen_revision=host.session.revision();
    auto apply=[this,frozen_session,frozen_revision](const std::vector<Command>& commands) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","This frame belongs to another document");
        if(host.session.revision()!=frozen_revision)
            throw Error("REVISION_CONFLICT","This Artboard inspector is stale; refresh the captured target before applying");
        canvas->cancel_interaction();host.session.apply(commands,frozen_revision);host.edited();
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
        if(std::string(key)=="width"||std::string(key)=="height") {
            const bool width=std::string(key)=="width";
            const bool typed_driven=width?board.width_driver.has_value():board.height_driver.has_value();
            const bool assigned_parent=board.template_assignment&&board.parent_size&&
                (width?board.parent_size->width:board.parent_size->height);
            input->setReadOnly(typed_driven||assigned_parent);
            input->setToolTip(assigned_parent?"Use Template Reset or Parent size controls before entering a literal size.":
                typed_driven?"Unlink the typed size source before entering a literal size.":
                board.template_assignment?"Typing a size creates an override for this Template axis only. Reset override restores Template inheritance.":
                "Typing a size creates a local override. Use Inherit below to reset to the parent size.");
        }
        else input->setToolTip("Crop position only; this does not move any artwork.");
        connect(input,&QLineEdit::editingFinished,this,[this,input,composition,read,apply,member,key=std::string(key)]{
            if(!input->isModified())return;input->setModified(false);
            perform([&]{bool valid=false;const auto value=input->text().trimmed().toDouble(&valid);
                if(!valid||!std::isfinite(value))throw Error("INVALID_VALUE","Enter a finite frame coordinate or size");
                auto board=read();
                if(board.template_assignment&&(key=="width"||key=="height")) {
                    // An explicit field edit must not disappear when its value
                    // equals the retained authored fallback used by UpdateArtboard.
                    apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{
                        composition,board.id,"frame."+key,value}}});
                    return;
                }
                board.*member=value;
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

    auto size_source=[&,this](bool width) {
        const QString axis=width?"width":"height";
        const Ref target{id,"",width?"artboard.width":"artboard.height"};
        const auto driver=width?board.width_driver:board.height_driver;
        const bool parent_driven=board.parent_size&&(width?board.parent_size->width:board.parent_size->height);
        auto* source_box=new QGroupBox(axis+" source");source_box->setObjectName("artboard-"+axis+"-source");
        auto* source_layout=new QVBoxLayout(source_box);
        auto* status=new QLabel(source_box);status->setObjectName("artboard-"+axis+"-source-state");
        const auto kind=qs(artboard_size_property(host.session.document(),target).source_kind);
        status->setText("Source: "+kind+" · Literal: "+display_value(width?board.width:board.height)+
            " du · Evaluated: "+display_value(width?resolved.width:resolved.height)+" du");
        status->setWordWrap(true);source_layout->addWidget(status);
        auto* source=new QComboBox(source_box);source->setObjectName("artboard-"+axis+"-link-source");
        source->setAccessibleName(axis+" Artboard source");
        std::vector<Ref> source_refs;
        QStringList source_labels;
        for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
            Ref ref{candidate.id,"",source_width?"artboard.width":"artboard.height"};
            if(ref==target)continue;
            source_refs.push_back(ref);
            source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
                " ("+qs(candidate.id)+"/"+qs(ref.field)+")");
        }
        const auto selected_source=driver&&std::holds_alternative<Ref>(driver->value)?
            std::optional<Ref>{std::get<Ref>(driver->value)}:std::nullopt;
        auto* source_search=add_artboard_source_search(source_box,source_layout,source,source_refs,source_labels,selected_source);
        source_layout->addWidget(source);
        auto* expression=new ExpressionInput;expression->setObjectName("artboard-"+axis+"-expression");
        expression->setAccessibleName(axis+" expression");expression->setFixedHeight(58);
        expression->setPlaceholderText("Numeric expression in du; Ctrl+Enter to apply");
        if(driver&&std::holds_alternative<Expression>(driver->value))expression->setPlainText(qs(std::get<Expression>(driver->value).source));
        source_layout->addWidget(expression);
        auto* replace=new QCheckBox("Replace current source",source_box);
        replace->setObjectName("artboard-"+axis+"-replace");replace->setEnabled(parent_driven||driver.has_value());
        source_layout->addWidget(replace);
        auto* actions=new QHBoxLayout;source_layout->addLayout(actions);
        auto* link=new QPushButton("Link",source_box);link->setObjectName("artboard-"+axis+"-link");actions->addWidget(link);
        auto* set_expression=new QPushButton("Apply expression",source_box);
        set_expression->setObjectName("artboard-"+axis+"-apply-expression");actions->addWidget(set_expression);
        auto* unlink=new QPushButton("Unlink · keep size",source_box);
        unlink->setObjectName("artboard-"+axis+"-unlink");unlink->setEnabled(driver.has_value());actions->addWidget(unlink);
        auto* cancel=new QPushButton("Cancel draft",source_box);
        cancel->setObjectName("artboard-"+axis+"-cancel");actions->addWidget(cancel);
        const bool link_available=!source_refs.empty();
        link->setEnabled(link_available&&source->currentIndex()>=0);
        connect(source,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [link,link_available](int index){link->setEnabled(link_available&&index>=0);});
        connect(source_search,&QLineEdit::textChanged,this,[source,link,link_available](const QString&){
            link->setEnabled(link_available&&source->currentIndex()>=0);
        });
        auto commit=[this,frozen_session,frozen_revision](const std::vector<Command>& commands) {
            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Artboard source belongs to another document");
            canvas->cancel_interaction();host.session.apply(commands,frozen_revision);host.edited();
        };
        connect(link,&QPushButton::clicked,this,[this,source,source_refs,target,replace,commit]{perform([&]{
            bool valid=false;const auto candidate=source->currentData(Qt::UserRole).toInt(&valid);
            if(source->currentIndex()<0||!valid||candidate<0||static_cast<std::size_t>(candidate)>=source_refs.size())
                throw Error("NO_SOURCE","Choose an Artboard size source");
            commit({LinkArtboardSize{target,source_refs[static_cast<std::size_t>(candidate)],replace->isChecked()}});
        });});
        auto apply_expression=[this,expression,target,replace,commit]{perform([&]{
            commit({SetArtboardSizeExpression{target,{expression->toPlainText().toStdString(),1},replace->isChecked()}});
        });};
        expression->apply=apply_expression;expression->cancel=[this]{rebuild_inspector();};
        connect(set_expression,&QPushButton::clicked,this,apply_expression);
        connect(unlink,&QPushButton::clicked,this,[this,target,commit]{perform([&]{commit({UnlinkArtboardSize{target}});});});
        connect(cancel,&QPushButton::clicked,this,[this]{rebuild_inspector();});
        form->addRow("",source_box);
    };
    size_source(true);size_source(false);

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
        QTimer::singleShot(0,this,[this]{refresh();if(utility_setup_dialog_)rebuild_layout_setup();});
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
        if(input->isReadOnly())return;
        connect(input,&QLineEdit::textEdited,this,[preview_from,scope,build]{preview_from(scope,build);});
        connect(input,&QLineEdit::returnPressed,this,[commit_from,scope,build]{commit_from(scope,build);});
    };
    auto set_layout_command=[composition,id,assigned=board.template_assignment.has_value()](
        const ArtboardLayout& value,const std::string& family)->std::vector<Command> {
        if(assigned) {
            if(family=="layout.margin")return {ArtboardTemplateCommand{SetArtboardTemplateOverride{
                composition,id,family,value.margin}}};
            return {ArtboardTemplateCommand{SetArtboardTemplateOverride{composition,id,family,value.grid}}};
        }
        auto payload=std::optional<ArtboardLayout>{value};
        if(!payload->margin&&!payload->grid)payload.reset();
        return {SetArtboardLayout{composition,id,std::move(payload)}};
    };

    const auto cancel_layout_editor=[this] {
        cancel_layout_draft();
        if(utility_setup_dialog_)rebuild_layout_setup();else rebuild_inspector();
    };
    auto* margin_box=new QGroupBox("Margin inset · du");margin_box->setObjectName("layout-margin");
    auto* margin_form=new QFormLayout(margin_box);margin_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const Margin initial_margin=board.layout&&board.layout->margin?*board.layout->margin:
        resolved.layout&&resolved.layout->margin?*resolved.layout->margin:Margin{};
    auto* margin_left=make_number(margin_box,"margin-left","Left",QString::number(initial_margin.left,'g',15));
    auto* margin_top=make_number(margin_box,"margin-top","Top",QString::number(initial_margin.top,'g',15));
    auto* margin_right=make_number(margin_box,"margin-right","Right",QString::number(initial_margin.right,'g',15));
    auto* margin_bottom=make_number(margin_box,"margin-bottom","Bottom",QString::number(initial_margin.bottom,'g',15));
    const Ref margin_left_ref{id,"","margin.left"};
    const Ref margin_top_ref{id,"","margin.top"};
    const auto margin_left_driver=board.layout&&board.layout->margin?board.layout->margin->left_driver:std::optional<Ref>{};
    const auto margin_left_expression=board.layout&&board.layout->margin?board.layout->margin->left_expression:std::optional<Expression>{};
    const auto margin_top_driver=board.layout&&board.layout->margin?board.layout->margin->top_driver:std::optional<Ref>{};
    const auto margin_top_authored_expression=board.layout&&board.layout->margin?board.layout->margin->top_expression:std::optional<Expression>{};
    const auto margin_right_driver=board.layout&&board.layout->margin?board.layout->margin->right_driver:std::optional<Ref>{};
    const auto margin_right_expression=board.layout&&board.layout->margin?board.layout->margin->right_expression:std::optional<Expression>{};
    const auto margin_bottom_driver=board.layout&&board.layout->margin?board.layout->margin->bottom_driver:std::optional<Ref>{};
    const auto margin_bottom_expression=board.layout&&board.layout->margin?board.layout->margin->bottom_expression:std::optional<Expression>{};
    const bool margin_left_is_driven=margin_left_driver.has_value()||margin_left_expression.has_value();
    margin_left->setReadOnly(margin_left_is_driven);
    margin_left->setToolTip(margin_left_is_driven?"This authored literal is read-only while its source is active. Unlink to edit it.":"Artboard-local Margin inset in du.");
    const bool margin_top_is_driven=margin_top_driver.has_value()||margin_top_authored_expression.has_value();
    margin_top->setReadOnly(margin_top_is_driven);
    margin_top->setToolTip(margin_top_is_driven?"This authored literal is read-only while its source is active. Unlink to edit it.":"Artboard-local Margin inset in du.");
    const bool margin_right_is_driven=margin_right_driver.has_value()||margin_right_expression.has_value();
    margin_right->setReadOnly(margin_right_is_driven);
    margin_right->setToolTip(margin_right_is_driven?"This authored literal is read-only while its source is active. Unlink to edit it.":"Artboard-local Margin inset in du.");
    const bool margin_bottom_is_driven=margin_bottom_driver.has_value()||margin_bottom_expression.has_value();
    margin_bottom->setReadOnly(margin_bottom_is_driven);
    margin_bottom->setToolTip(margin_bottom_is_driven?"This authored literal is read-only while its source is active. Unlink to edit it.":"Artboard-local Margin inset in du.");
    margin_form->addRow("Left · du",margin_left);margin_form->addRow("Top · du",margin_top);
    margin_form->addRow("Right · du",margin_right);margin_form->addRow("Bottom · du",margin_bottom);
    const auto authored_margin_left=initial_margin.left;
    const auto authored_margin_top=initial_margin.top;
    const auto authored_margin_right=initial_margin.right;
    const auto authored_margin_bottom=initial_margin.bottom;
    const LayoutBuilder margin_builder=[read,composition,id,parse_number,set_layout_command,margin_left,margin_top,margin_right,margin_bottom,
        margin_left_driver,margin_left_expression,margin_left_is_driven,authored_margin_left,
        margin_top_driver,margin_top_authored_expression,margin_top_is_driven,authored_margin_top,
        margin_right_driver,margin_right_expression,margin_right_is_driven,authored_margin_right,
        margin_bottom_driver,margin_bottom_expression,margin_bottom_is_driven,authored_margin_bottom] {
        auto current=read();auto value=current.layout.value_or(ArtboardLayout{});
        value.margin=Margin{margin_left_is_driven?authored_margin_left:parse_number(margin_left),
            margin_top_is_driven?authored_margin_top:parse_number(margin_top),
            margin_right_is_driven?authored_margin_right:parse_number(margin_right),
            margin_bottom_is_driven?authored_margin_bottom:parse_number(margin_bottom)};
        value.margin->left_driver=margin_left_driver;value.margin->left_expression=margin_left_expression;
        value.margin->top_driver=margin_top_driver;value.margin->top_expression=margin_top_authored_expression;
        value.margin->right_driver=margin_right_driver;value.margin->right_expression=margin_right_expression;
        value.margin->bottom_driver=margin_bottom_driver;
        value.margin->bottom_expression=margin_bottom_expression;
        return set_layout_command(value,"layout.margin");
    };
    auto* margin_source_box=new QGroupBox("Left source",margin_box);margin_source_box->setObjectName("margin-left-source");
    auto* margin_source_layout=new QVBoxLayout(margin_source_box);
    auto* margin_source_state=new QLabel(margin_source_box);margin_source_state->setObjectName("margin-left-source-state");
    QString margin_source_description="literal";
    if(margin_left_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==margin_left_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        margin_source_description="link · "+source_name+" ("+qs(margin_left_driver->object)+"/"+
            qs(margin_left_driver->field)+")";
    } else if(margin_left_expression)margin_source_description="expression · "+qs(margin_left_expression->source);
    margin_source_state->setWordWrap(true);
    margin_source_state->setText("Source: "+margin_source_description+" · Literal: "+display_value(initial_margin.left)+
        " du · Evaluated: "+display_value(resolved.layout&&resolved.layout->margin?resolved.layout->margin->left:initial_margin.left)+" du");
    margin_source_layout->addWidget(margin_source_state);
    auto* margin_left_source=new QComboBox(margin_source_box);margin_left_source->setObjectName("margin-left-link-source");
    margin_left_source->setAccessibleName("Margin left Artboard size source");
    std::vector<Ref> margin_left_sources;
    QStringList margin_left_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        if(candidate.id==id)continue;
        margin_left_sources.push_back(source);
        margin_left_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* margin_left_search=add_artboard_source_search(margin_source_box,margin_source_layout,
        margin_left_source,margin_left_sources,margin_left_source_labels,margin_left_driver);
    margin_source_layout->addWidget(margin_left_source);
    auto* margin_replace=new QCheckBox("Replace current left source",margin_source_box);
    margin_replace->setObjectName("margin-left-replace");margin_replace->setEnabled(margin_left_is_driven);
    margin_source_layout->addWidget(margin_replace);
    auto* margin_source_actions=new QHBoxLayout;margin_source_layout->addLayout(margin_source_actions);
    auto* margin_link=new QPushButton("Link",margin_source_box);margin_link->setObjectName("margin-left-link");
    const bool margin_link_available=board.layout&&board.layout->margin&&!margin_left_sources.empty();
    margin_link->setEnabled(margin_link_available&&margin_left_source->currentIndex()>=0);margin_source_actions->addWidget(margin_link);
    connect(margin_left_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [margin_link,margin_link_available](int index){margin_link->setEnabled(margin_link_available&&index>=0);});
    connect(margin_left_search,&QLineEdit::textChanged,this,[margin_left_source,margin_link,margin_link_available](const QString&){
        margin_link->setEnabled(margin_link_available&&margin_left_source->currentIndex()>=0);
    });
    auto* margin_unlink=new QPushButton("Unlink · keep value",margin_source_box);margin_unlink->setObjectName("margin-left-unlink");
    margin_unlink->setEnabled(margin_left_is_driven);margin_source_actions->addWidget(margin_unlink);
    auto* margin_source_cancel=new QPushButton("Cancel draft",margin_source_box);margin_source_cancel->setObjectName("margin-left-cancel");
    margin_source_actions->addWidget(margin_source_cancel);
    auto* margin_expression=new ExpressionInput;margin_expression->setObjectName("margin-left-expression");
    margin_expression->setAccessibleName("Margin left expression draft");margin_expression->setFixedHeight(58);
    margin_expression->setPlaceholderText("du expression using Artboard width/height ref() values");
    if(margin_left_expression)margin_expression->setPlainText(qs(margin_left_expression->source));
    margin_source_layout->addWidget(margin_expression);
    auto* margin_expression_actions=new QHBoxLayout;margin_source_layout->addLayout(margin_expression_actions);
    auto* margin_expression_apply=new QPushButton("Apply expression",margin_source_box);
    margin_expression_apply->setObjectName("margin-left-apply-expression");
    margin_expression_apply->setEnabled(board.layout&&board.layout->margin);margin_expression_actions->addWidget(margin_expression_apply);
    auto* margin_expression_cancel=new QPushButton("Cancel expression",margin_source_box);
    margin_expression_cancel->setObjectName("margin-left-cancel-expression");margin_expression_actions->addWidget(margin_expression_cancel);
    margin_form->addRow(margin_source_box);
    auto margin_source_commit=[this,frozen_session,frozen_revision](const std::vector<Command>& commands) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Margin source belongs to another document");
        canvas->cancel_interaction();host.session.apply(commands,frozen_revision);host.edited();
    };
    connect(margin_link,&QPushButton::clicked,this,[this,margin_left_source,margin_left_sources,margin_left_ref,margin_replace,margin_source_commit]{perform([&]{
        bool valid=false;const auto candidate=margin_left_source->currentData(Qt::UserRole).toInt(&valid);
        if(margin_left_source->currentIndex()<0||!valid||candidate<0||static_cast<std::size_t>(candidate)>=margin_left_sources.size())
            throw Error("NO_SOURCE","Choose an Artboard size source");
        margin_source_commit({MarginLeftCommand{LinkMarginLeft{margin_left_ref,
            margin_left_sources[static_cast<std::size_t>(candidate)],margin_replace->isChecked()}}});
    });});
    connect(margin_unlink,&QPushButton::clicked,this,[this,margin_left_ref,margin_source_commit]{
        perform([&]{margin_source_commit({MarginLeftCommand{UnlinkMarginLeft{margin_left_ref}}});});
    });
    connect(margin_source_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(margin_expression_apply,&QPushButton::clicked,this,[this,margin_expression,margin_replace,margin_left_ref,margin_source_commit]{perform([&]{
        margin_source_commit({MarginLeftCommand{SetMarginLeftExpression{margin_left_ref,
            {margin_expression->toPlainText().toStdString(),1},margin_replace->isChecked()}}});
    });});
    connect(margin_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    auto* margin_top_source_box=new QGroupBox("Top source",margin_box);margin_top_source_box->setObjectName("margin-top-source");
    auto* margin_top_source_layout=new QVBoxLayout(margin_top_source_box);
    auto* margin_top_source_state=new QLabel(margin_top_source_box);margin_top_source_state->setObjectName("margin-top-source-state");
    QString margin_top_source_description="literal";
    if(margin_top_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==margin_top_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        margin_top_source_description="link · "+source_name+" ("+qs(margin_top_driver->object)+"/"+qs(margin_top_driver->field)+")";
    } else if(margin_top_authored_expression)margin_top_source_description="expression · "+qs(margin_top_authored_expression->source);
    margin_top_source_state->setWordWrap(true);
    margin_top_source_state->setText("Source: "+margin_top_source_description+" · Literal: "+display_value(initial_margin.top)+
        " du · Evaluated: "+display_value(resolved.layout&&resolved.layout->margin?resolved.layout->margin->top:initial_margin.top)+" du");
    margin_top_source_layout->addWidget(margin_top_source_state);
    auto* margin_top_source=new QComboBox(margin_top_source_box);margin_top_source->setObjectName("margin-top-link-source");
    margin_top_source->setAccessibleName("Margin top Artboard size source");
    std::vector<Ref> margin_top_sources;QStringList margin_top_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        margin_top_sources.push_back(source);
        margin_top_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* margin_top_search=add_artboard_source_search(margin_top_source_box,margin_top_source_layout,
        margin_top_source,margin_top_sources,margin_top_source_labels,margin_top_driver);
    margin_top_source_layout->addWidget(margin_top_source);
    auto* margin_top_replace=new QCheckBox("Replace current top source",margin_top_source_box);
    margin_top_replace->setObjectName("margin-top-replace");margin_top_replace->setEnabled(margin_top_is_driven);
    margin_top_source_layout->addWidget(margin_top_replace);
    auto* margin_top_actions=new QHBoxLayout;margin_top_source_layout->addLayout(margin_top_actions);
    auto* margin_top_link=new QPushButton("Link",margin_top_source_box);margin_top_link->setObjectName("margin-top-link");
    const bool margin_top_link_available=board.layout&&board.layout->margin&&!margin_top_sources.empty();
    margin_top_link->setEnabled(margin_top_link_available&&margin_top_source->currentIndex()>=0);margin_top_actions->addWidget(margin_top_link);
    connect(margin_top_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [margin_top_link,margin_top_link_available](int index){margin_top_link->setEnabled(margin_top_link_available&&index>=0);});
    connect(margin_top_search,&QLineEdit::textChanged,this,[margin_top_source,margin_top_link,margin_top_link_available](const QString&){
        margin_top_link->setEnabled(margin_top_link_available&&margin_top_source->currentIndex()>=0);
    });
    auto* margin_top_unlink=new QPushButton("Unlink · keep value",margin_top_source_box);margin_top_unlink->setObjectName("margin-top-unlink");
    margin_top_unlink->setEnabled(margin_top_is_driven);margin_top_actions->addWidget(margin_top_unlink);
    auto* margin_top_cancel=new QPushButton("Cancel draft",margin_top_source_box);margin_top_cancel->setObjectName("margin-top-cancel");
    margin_top_actions->addWidget(margin_top_cancel);
    auto* margin_top_expression=new ExpressionInput;margin_top_expression->setObjectName("margin-top-expression");
    margin_top_expression->setAccessibleName("Margin top expression draft");margin_top_expression->setFixedHeight(58);
    margin_top_expression->setPlaceholderText("du expression using Artboard width/height ref() values");
    if(margin_top_authored_expression)margin_top_expression->setPlainText(qs(margin_top_authored_expression->source));
    margin_top_source_layout->addWidget(margin_top_expression);
    auto* margin_top_expression_actions=new QHBoxLayout;margin_top_source_layout->addLayout(margin_top_expression_actions);
    auto* margin_top_expression_apply=new QPushButton("Apply expression",margin_top_source_box);
    margin_top_expression_apply->setObjectName("margin-top-apply-expression");
    margin_top_expression_apply->setEnabled(board.layout&&board.layout->margin);
    margin_top_expression_actions->addWidget(margin_top_expression_apply);
    auto* margin_top_expression_cancel=new QPushButton("Cancel expression",margin_top_source_box);
    margin_top_expression_cancel->setObjectName("margin-top-cancel-expression");
    margin_top_expression_actions->addWidget(margin_top_expression_cancel);margin_form->addRow(margin_top_source_box);
    connect(margin_top_link,&QPushButton::clicked,this,[this,margin_top_source,margin_top_sources,margin_top_ref,margin_top_replace,margin_source_commit]{perform([&]{
        bool valid=false;const auto candidate=margin_top_source->currentData(Qt::UserRole).toInt(&valid);
        if(margin_top_source->currentIndex()<0||!valid||candidate<0||static_cast<std::size_t>(candidate)>=margin_top_sources.size())
            throw Error("NO_SOURCE","Choose an Artboard size source");
        margin_source_commit({MarginTopCommand{LinkMarginTop{margin_top_ref,
            margin_top_sources[static_cast<std::size_t>(candidate)],margin_top_replace->isChecked()}}});
    });});
    connect(margin_top_unlink,&QPushButton::clicked,this,[this,margin_top_ref,margin_source_commit]{
        perform([&]{margin_source_commit({MarginTopCommand{UnlinkMarginTop{margin_top_ref}}});});
    });
    connect(margin_top_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(margin_top_expression_apply,&QPushButton::clicked,this,[this,margin_top_expression,margin_top_replace,margin_top_ref,margin_source_commit]{perform([&]{
        margin_source_commit({MarginTopCommand{SetMarginTopExpression{margin_top_ref,
            {margin_top_expression->toPlainText().toStdString(),1},margin_top_replace->isChecked()}}});
    });});
    connect(margin_top_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    const Ref margin_right_ref{id,"","margin.right"};
    auto* margin_right_source_box=new QGroupBox("Right source",margin_box);margin_right_source_box->setObjectName("margin-right-source");
    auto* margin_right_source_layout=new QVBoxLayout(margin_right_source_box);
    auto* margin_right_source_state=new QLabel(margin_right_source_box);margin_right_source_state->setObjectName("margin-right-source-state");
    QString margin_right_source_description="literal";
    if(margin_right_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==margin_right_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        margin_right_source_description="link · "+source_name+" ("+qs(margin_right_driver->object)+"/"+qs(margin_right_driver->field)+")";
    } else if(margin_right_expression)margin_right_source_description="expression · "+qs(margin_right_expression->source);
    margin_right_source_state->setWordWrap(true);
    margin_right_source_state->setText("Source: "+margin_right_source_description+" · Literal: "+display_value(initial_margin.right)+
        " du · Evaluated: "+display_value(resolved.layout&&resolved.layout->margin?resolved.layout->margin->right:initial_margin.right)+" du");
    margin_right_source_layout->addWidget(margin_right_source_state);
    auto* margin_right_source=new QComboBox(margin_right_source_box);margin_right_source->setObjectName("margin-right-link-source");
    margin_right_source->setAccessibleName("Margin right Artboard size source");
    std::vector<Ref> margin_right_sources;QStringList margin_right_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        margin_right_sources.push_back(source);
        margin_right_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* margin_right_search=add_artboard_source_search(margin_right_source_box,margin_right_source_layout,
        margin_right_source,margin_right_sources,margin_right_source_labels,margin_right_driver);
    margin_right_source_layout->addWidget(margin_right_source);
    auto* margin_right_replace=new QCheckBox("Replace current right source",margin_right_source_box);
    margin_right_replace->setObjectName("margin-right-replace");margin_right_replace->setEnabled(margin_right_is_driven);
    margin_right_source_layout->addWidget(margin_right_replace);
    auto* margin_right_actions=new QHBoxLayout;margin_right_source_layout->addLayout(margin_right_actions);
    auto* margin_right_link=new QPushButton("Link",margin_right_source_box);margin_right_link->setObjectName("margin-right-link");
    const bool margin_right_link_available=board.layout&&board.layout->margin&&!margin_right_sources.empty();
    margin_right_link->setEnabled(margin_right_link_available&&margin_right_source->currentIndex()>=0);
    margin_right_actions->addWidget(margin_right_link);
    connect(margin_right_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [margin_right_link,margin_right_link_available](int index){margin_right_link->setEnabled(margin_right_link_available&&index>=0);});
    connect(margin_right_search,&QLineEdit::textChanged,this,[margin_right_source,margin_right_link,margin_right_link_available](const QString&){
        margin_right_link->setEnabled(margin_right_link_available&&margin_right_source->currentIndex()>=0);
    });
    auto* margin_right_unlink=new QPushButton("Unlink · keep value",margin_right_source_box);
    margin_right_unlink->setObjectName("margin-right-unlink");margin_right_unlink->setEnabled(margin_right_is_driven);
    margin_right_actions->addWidget(margin_right_unlink);
    auto* margin_right_cancel=new QPushButton("Cancel draft",margin_right_source_box);
    margin_right_cancel->setObjectName("margin-right-cancel");margin_right_actions->addWidget(margin_right_cancel);
    auto* margin_right_expression_input=new ExpressionInput;margin_right_expression_input->setObjectName("margin-right-expression");
    margin_right_expression_input->setAccessibleName("Margin right expression draft");margin_right_expression_input->setFixedHeight(58);
    margin_right_expression_input->setPlaceholderText("du expression using Artboard width/height ref() values");
    if(margin_right_expression)margin_right_expression_input->setPlainText(qs(margin_right_expression->source));
    margin_right_source_layout->addWidget(margin_right_expression_input);
    auto* margin_right_expression_actions=new QHBoxLayout;margin_right_source_layout->addLayout(margin_right_expression_actions);
    auto* margin_right_expression_apply=new QPushButton("Apply expression",margin_right_source_box);
    margin_right_expression_apply->setObjectName("margin-right-apply-expression");
    margin_right_expression_apply->setEnabled(board.layout&&board.layout->margin);
    margin_right_expression_actions->addWidget(margin_right_expression_apply);
    auto* margin_right_expression_cancel=new QPushButton("Cancel expression",margin_right_source_box);
    margin_right_expression_cancel->setObjectName("margin-right-cancel-expression");
    margin_right_expression_actions->addWidget(margin_right_expression_cancel);
    margin_form->addRow(margin_right_source_box);
    connect(margin_right_link,&QPushButton::clicked,this,[this,margin_right_source,margin_right_sources,margin_right_ref,
        margin_right_replace,margin_source_commit]{perform([&]{
        bool valid=false;const auto candidate=margin_right_source->currentData(Qt::UserRole).toInt(&valid);
        if(margin_right_source->currentIndex()<0||!valid||candidate<0||static_cast<std::size_t>(candidate)>=margin_right_sources.size())
            throw Error("NO_SOURCE","Choose an Artboard size source");
        margin_source_commit({MarginRightCommand{LinkMarginRight{margin_right_ref,
            margin_right_sources[static_cast<std::size_t>(candidate)],margin_right_replace->isChecked()}}});
    });});
    connect(margin_right_unlink,&QPushButton::clicked,this,[this,margin_right_ref,margin_source_commit]{
        perform([&]{margin_source_commit({MarginRightCommand{UnlinkMarginRight{margin_right_ref}}});});
    });
    connect(margin_right_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(margin_right_expression_apply,&QPushButton::clicked,this,[this,margin_right_expression_input,margin_right_replace,
        margin_right_ref,margin_source_commit]{perform([&]{
        margin_source_commit({MarginRightCommand{SetMarginRightExpression{margin_right_ref,
            {margin_right_expression_input->toPlainText().toStdString(),1},margin_right_replace->isChecked()}}});
    });});
    connect(margin_right_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    const Ref margin_bottom_ref{id,"","margin.bottom"};
    auto* margin_bottom_source_box=new QGroupBox("Bottom source",margin_box);
    margin_bottom_source_box->setObjectName("margin-bottom-source");
    auto* margin_bottom_source_layout=new QVBoxLayout(margin_bottom_source_box);
    auto* margin_bottom_source_state=new QLabel(margin_bottom_source_box);
    margin_bottom_source_state->setObjectName("margin-bottom-source-state");
    QString margin_bottom_source_description="literal";
    if(margin_bottom_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==margin_bottom_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        margin_bottom_source_description="link · "+source_name+" ("+qs(margin_bottom_driver->object)+"/"+
            qs(margin_bottom_driver->field)+")";
    } else if(margin_bottom_expression)margin_bottom_source_description="expression · "+qs(margin_bottom_expression->source);
    margin_bottom_source_state->setWordWrap(true);
    margin_bottom_source_state->setText("Source: "+margin_bottom_source_description+" · Literal: "+
        display_value(initial_margin.bottom)+" du · Evaluated: "+
        display_value(resolved.layout&&resolved.layout->margin?resolved.layout->margin->bottom:initial_margin.bottom)+" du");
    margin_bottom_source_layout->addWidget(margin_bottom_source_state);
    auto* margin_bottom_source=new QComboBox(margin_bottom_source_box);
    margin_bottom_source->setObjectName("margin-bottom-link-source");
    margin_bottom_source->setAccessibleName("Margin bottom Artboard size source");
    std::vector<Ref> margin_bottom_sources;QStringList margin_bottom_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        margin_bottom_sources.push_back(source);
        margin_bottom_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* margin_bottom_search=add_artboard_source_search(margin_bottom_source_box,margin_bottom_source_layout,
        margin_bottom_source,margin_bottom_sources,margin_bottom_source_labels,margin_bottom_driver);
    margin_bottom_source_layout->addWidget(margin_bottom_source);
    auto* margin_bottom_replace=new QCheckBox("Replace current bottom source",margin_bottom_source_box);
    margin_bottom_replace->setObjectName("margin-bottom-replace");
    margin_bottom_replace->setEnabled(margin_bottom_is_driven);margin_bottom_source_layout->addWidget(margin_bottom_replace);
    auto* margin_bottom_actions=new QHBoxLayout;margin_bottom_source_layout->addLayout(margin_bottom_actions);
    auto* margin_bottom_link=new QPushButton("Link",margin_bottom_source_box);
    margin_bottom_link->setObjectName("margin-bottom-link");
    const bool margin_bottom_link_available=board.layout&&board.layout->margin&&!margin_bottom_sources.empty();
    margin_bottom_link->setEnabled(margin_bottom_link_available&&margin_bottom_source->currentIndex()>=0);
    margin_bottom_actions->addWidget(margin_bottom_link);
    connect(margin_bottom_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [margin_bottom_link,margin_bottom_link_available](int index){
            margin_bottom_link->setEnabled(margin_bottom_link_available&&index>=0);
        });
    connect(margin_bottom_search,&QLineEdit::textChanged,this,
        [margin_bottom_source,margin_bottom_link,margin_bottom_link_available](const QString&){
            margin_bottom_link->setEnabled(margin_bottom_link_available&&margin_bottom_source->currentIndex()>=0);
        });
    auto* margin_bottom_unlink=new QPushButton("Unlink · keep value",margin_bottom_source_box);
    margin_bottom_unlink->setObjectName("margin-bottom-unlink");
    margin_bottom_unlink->setEnabled(margin_bottom_is_driven);margin_bottom_actions->addWidget(margin_bottom_unlink);
    auto* margin_bottom_cancel=new QPushButton("Cancel draft",margin_bottom_source_box);
    margin_bottom_cancel->setObjectName("margin-bottom-cancel");margin_bottom_actions->addWidget(margin_bottom_cancel);
    auto* margin_bottom_expression_input=new ExpressionInput;
    margin_bottom_expression_input->setObjectName("margin-bottom-expression");
    margin_bottom_expression_input->setAccessibleName("Margin bottom expression draft");
    margin_bottom_expression_input->setFixedHeight(58);
    margin_bottom_expression_input->setPlaceholderText("du expression using Artboard width/height ref() values");
    if(margin_bottom_expression)margin_bottom_expression_input->setPlainText(qs(margin_bottom_expression->source));
    margin_bottom_source_layout->addWidget(margin_bottom_expression_input);
    auto* margin_bottom_expression_actions=new QHBoxLayout;margin_bottom_source_layout->addLayout(margin_bottom_expression_actions);
    auto* margin_bottom_expression_apply=new QPushButton("Apply expression",margin_bottom_source_box);
    margin_bottom_expression_apply->setObjectName("margin-bottom-apply-expression");
    margin_bottom_expression_apply->setEnabled(board.layout&&board.layout->margin);
    margin_bottom_expression_actions->addWidget(margin_bottom_expression_apply);
    auto* margin_bottom_expression_cancel=new QPushButton("Cancel expression",margin_bottom_source_box);
    margin_bottom_expression_cancel->setObjectName("margin-bottom-cancel-expression");
    margin_bottom_expression_actions->addWidget(margin_bottom_expression_cancel);
    margin_form->addRow(margin_bottom_source_box);
    connect(margin_bottom_link,&QPushButton::clicked,this,[this,margin_bottom_source,margin_bottom_sources,
        margin_bottom_ref,margin_bottom_replace,margin_source_commit]{perform([&]{
        bool valid=false;const auto candidate=margin_bottom_source->currentData(Qt::UserRole).toInt(&valid);
        if(margin_bottom_source->currentIndex()<0||!valid||candidate<0||
            static_cast<std::size_t>(candidate)>=margin_bottom_sources.size())
            throw Error("NO_SOURCE","Choose an Artboard size source");
        margin_source_commit({MarginBottomCommand{LinkMarginBottom{margin_bottom_ref,
            margin_bottom_sources[static_cast<std::size_t>(candidate)],margin_bottom_replace->isChecked()}}});
    });});
    connect(margin_bottom_unlink,&QPushButton::clicked,this,[this,margin_bottom_ref,margin_source_commit]{
        perform([&]{margin_source_commit({MarginBottomCommand{UnlinkMarginBottom{margin_bottom_ref}}});});
    });
    connect(margin_bottom_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(margin_bottom_expression_apply,&QPushButton::clicked,this,[this,margin_bottom_expression_input,
        margin_bottom_replace,margin_bottom_ref,margin_source_commit]{perform([&]{
        margin_source_commit({MarginBottomCommand{SetMarginBottomExpression{margin_bottom_ref,
            {margin_bottom_expression_input->toPlainText().toStdString(),1},margin_bottom_replace->isChecked()}}});
    });});
    connect(margin_bottom_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    auto* margin_actions=new QWidget(margin_box);auto* margin_buttons=new QHBoxLayout(margin_actions);margin_buttons->setContentsMargins(0,0,0,0);
    auto* margin_apply=new QPushButton("Apply Margin",margin_actions);margin_apply->setObjectName("margin-apply");margin_buttons->addWidget(margin_apply);
    auto* margin_clear=new QPushButton("Clear Margin",margin_actions);margin_clear->setObjectName("margin-clear");margin_clear->setEnabled(resolved.layout&&resolved.layout->margin);margin_buttons->addWidget(margin_clear);margin_form->addRow(margin_actions);
    if(!margin_left_is_driven)bind_number(margin_left,margin_box,margin_builder);
    if(!margin_top_is_driven)bind_number(margin_top,margin_box,margin_builder);
    if(!margin_right_is_driven)bind_number(margin_right,margin_box,margin_builder);
    if(!margin_bottom_is_driven)bind_number(margin_bottom,margin_box,margin_builder);
    connect(margin_apply,&QPushButton::clicked,this,[commit_from,margin_box,margin_builder]{commit_from(margin_box,margin_builder);});
    connect(margin_clear,&QPushButton::clicked,this,[read,set_layout_command,commit_explicit,guard_editor,margin_box] {
        if(guard_editor())return;
        auto current=read();auto value=current.layout.value_or(ArtboardLayout{});value.margin.reset();
        commit_explicit(margin_box,[set_layout_command,value]{return set_layout_command(value,"layout.margin");});
    });
    layout->addWidget(margin_box);

    auto* grid_box=new QGroupBox("Grid · Artboard-local bounds and cells · du");grid_box->setObjectName("layout-grid");
    auto* grid_form=new QFormLayout(grid_box);grid_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const Grid initial_grid=board.layout&&board.layout->grid?*board.layout->grid:
        resolved.layout&&resolved.layout->grid?*resolved.layout->grid:
        Grid{board.template_assignment?board.template_assignment->grid_id:new_id(),{0,0,resolved.width,resolved.height},1,1,0,0};
    const Id grid_id=initial_grid.id;
    const auto grid_bounds_x_driver=board.layout&&board.layout->grid?board.layout->grid->bounds_x_driver:std::optional<Ref>{};
    const auto grid_bounds_x_expression=board.layout&&board.layout->grid?board.layout->grid->bounds_x_expression:std::optional<Expression>{};
    const auto grid_bounds_y_driver=board.layout&&board.layout->grid?board.layout->grid->bounds_y_driver:std::optional<Ref>{};
    const auto grid_bounds_y_expression=board.layout&&board.layout->grid?board.layout->grid->bounds_y_expression:std::optional<Expression>{};
    const auto grid_bounds_width_driver=board.layout&&board.layout->grid?board.layout->grid->bounds_width_driver:std::optional<Ref>{};
    const auto grid_bounds_width_expression=board.layout&&board.layout->grid?board.layout->grid->bounds_width_expression:std::optional<Expression>{};
    const auto grid_bounds_height_driver=board.layout&&board.layout->grid?board.layout->grid->bounds_height_driver:std::optional<Ref>{};
    const auto grid_bounds_height_expression=board.layout&&board.layout->grid?board.layout->grid->bounds_height_expression:std::optional<Expression>{};
    const auto grid_column_gutter_driver=board.layout&&board.layout->grid?board.layout->grid->column_gutter_driver:std::optional<Ref>{};
    const auto grid_column_gutter_expression=board.layout&&board.layout->grid?board.layout->grid->column_gutter_expression:std::optional<Expression>{};
    const auto grid_columns_driver=board.layout&&board.layout->grid?board.layout->grid->columns_driver:std::optional<Ref>{};
    const auto grid_columns_expression=board.layout&&board.layout->grid?board.layout->grid->columns_expression:std::optional<Expression>{};
    const auto grid_rows_driver=board.layout&&board.layout->grid?board.layout->grid->rows_driver:std::optional<Ref>{};
    const auto grid_rows_expression=board.layout&&board.layout->grid?board.layout->grid->rows_expression:std::optional<Expression>{};
    const auto grid_row_gutter_driver=board.layout&&board.layout->grid?board.layout->grid->row_gutter_driver:std::optional<Ref>{};
    const auto grid_row_gutter_expression=board.layout&&board.layout->grid?board.layout->grid->row_gutter_expression:std::optional<Expression>{};
    const bool grid_bounds_x_is_driven=grid_bounds_x_driver.has_value()||grid_bounds_x_expression.has_value();
    const bool grid_bounds_y_is_driven=grid_bounds_y_driver.has_value()||grid_bounds_y_expression.has_value();
    const bool grid_bounds_width_is_driven=grid_bounds_width_driver.has_value()||grid_bounds_width_expression.has_value();
    const bool grid_bounds_height_is_driven=grid_bounds_height_driver.has_value()||grid_bounds_height_expression.has_value();
    const bool grid_columns_is_driven=grid_columns_driver.has_value()||grid_columns_expression.has_value();
    const bool grid_rows_is_driven=grid_rows_driver.has_value()||grid_rows_expression.has_value();
    const bool grid_column_gutter_is_driven=grid_column_gutter_driver.has_value()||grid_column_gutter_expression.has_value();
    const bool grid_row_gutter_is_driven=grid_row_gutter_driver.has_value()||grid_row_gutter_expression.has_value();
    auto* grid_x=make_number(grid_box,"grid-x","Grid X",QString::number(initial_grid.bounds.x,'g',15));
    grid_x->setReadOnly(grid_bounds_x_is_driven);
    grid_x->setToolTip(grid_bounds_x_is_driven?"This authored literal is read-only while its source is active. Unlink to edit it.":"Artboard-local Grid x offset in du.");
    auto* grid_y=make_number(grid_box,"grid-y","Grid Y",QString::number(initial_grid.bounds.y,'g',15));
    grid_y->setReadOnly(grid_bounds_y_is_driven);
    grid_y->setToolTip(grid_bounds_y_is_driven?"This authored literal is read-only while its source is active. Unlink to edit it.":"Artboard-local Grid y offset in du.");
    auto* grid_width=make_number(grid_box,"grid-width","Grid width",QString::number(initial_grid.bounds.width,'g',15));
    grid_width->setReadOnly(grid_bounds_width_is_driven);
    grid_width->setToolTip(grid_bounds_width_is_driven?"This authored literal is read-only while its source is active. Unlink to edit it.":"Artboard-local Grid width in du.");
    auto* grid_height=make_number(grid_box,"grid-height","Grid height",QString::number(initial_grid.bounds.height,'g',15));
    grid_height->setReadOnly(grid_bounds_height_is_driven);
    grid_height->setToolTip(grid_bounds_height_is_driven?"This authored literal is read-only while its source is active. Unlink to edit it.":"Artboard-local Grid height in du.");
    const auto evaluated_grid_columns=resolved.layout&&resolved.layout->grid?resolved.layout->grid->columns:initial_grid.columns;
    auto* grid_columns=make_number(grid_box,"grid-columns","Columns",QString::number(static_cast<qulonglong>(grid_columns_is_driven?evaluated_grid_columns:initial_grid.columns)));
    grid_columns->setReadOnly(grid_columns_is_driven);
    grid_columns->setToolTip(grid_columns_is_driven?
        "This authored integer is read-only while its Grid source is active. Unlink to edit it.":
        "Unitless Grid column count from 1 to 1000.");
    const auto evaluated_grid_rows=resolved.layout&&resolved.layout->grid?resolved.layout->grid->rows:initial_grid.rows;
    auto* grid_rows=make_number(grid_box,"grid-rows","Rows",QString::number(static_cast<qulonglong>(grid_rows_is_driven?evaluated_grid_rows:initial_grid.rows)));
    grid_rows->setReadOnly(grid_rows_is_driven);
    grid_rows->setToolTip(grid_rows_is_driven?
        "This authored integer is read-only while its Grid source is active. Unlink to edit it.":
        "Unitless Grid row count from 1 to 1000.");
    auto* grid_column_gutter=make_number(grid_box,"grid-column-gutter","Column gutter",QString::number(initial_grid.column_gutter,'g',15));
    grid_column_gutter->setReadOnly(grid_column_gutter_is_driven);
    grid_column_gutter->setToolTip(grid_column_gutter_is_driven?
        "This authored literal is read-only while its source is active. Unlink to edit it.":
        "Artboard-local Grid column gutter in du.");
    auto* grid_row_gutter=make_number(grid_box,"grid-row-gutter","Row gutter",QString::number(initial_grid.row_gutter,'g',15));
    grid_row_gutter->setReadOnly(grid_row_gutter_is_driven);
    grid_row_gutter->setToolTip(grid_row_gutter_is_driven?
        "This authored literal is read-only while its source is active. Unlink to edit it.":
        "Artboard-local Grid row gutter in du.");
    grid_form->addRow("X · du",grid_x);grid_form->addRow("Y · du",grid_y);
    grid_form->addRow("Width · du",grid_width);grid_form->addRow("Height · du",grid_height);
    grid_form->addRow("Columns · integer",grid_columns);grid_form->addRow("Rows · integer",grid_rows);
    grid_form->addRow("Column gutter · du",grid_column_gutter);grid_form->addRow("Row gutter · du",grid_row_gutter);
    const auto authored_grid_x=initial_grid.bounds.x;
    const auto authored_grid_y=initial_grid.bounds.y;
    const auto authored_grid_width=initial_grid.bounds.width;
    const LayoutBuilder grid_builder=[read,composition,id,parse_number,parse_count,set_layout_command,grid_id,grid_x,grid_y,grid_width,grid_height,grid_columns,grid_rows,grid_column_gutter,grid_row_gutter,
        grid_columns_driver,grid_columns_expression,grid_columns_is_driven,authored_grid_columns=initial_grid.columns,
        grid_rows_driver,grid_rows_expression,grid_rows_is_driven,authored_grid_rows=initial_grid.rows,
        grid_bounds_x_driver,grid_bounds_x_expression,grid_bounds_x_is_driven,authored_grid_x,grid_bounds_y_driver,grid_bounds_y_expression,grid_bounds_y_is_driven,authored_grid_y,
        grid_bounds_width_driver,grid_bounds_width_expression,grid_bounds_width_is_driven,authored_grid_width,
        grid_bounds_height_driver,grid_bounds_height_expression,grid_bounds_height_is_driven,
        grid_column_gutter_driver,grid_column_gutter_expression,grid_column_gutter_is_driven,authored_grid_column_gutter=initial_grid.column_gutter,
        grid_row_gutter_driver,grid_row_gutter_expression,grid_row_gutter_is_driven,
        authored_grid_row_gutter=initial_grid.row_gutter,
        authored_grid_height=initial_grid.bounds.height] {
        auto current=read();auto value=current.layout.value_or(ArtboardLayout{});
        Grid grid{grid_id,{grid_bounds_x_is_driven?authored_grid_x:parse_number(grid_x),
            grid_bounds_y_is_driven?authored_grid_y:parse_number(grid_y),
            grid_bounds_width_is_driven?authored_grid_width:parse_number(grid_width),
            grid_bounds_height_is_driven?authored_grid_height:parse_number(grid_height)},
            grid_columns_is_driven?authored_grid_columns:parse_count(grid_columns),
            grid_rows_is_driven?authored_grid_rows:parse_count(grid_rows),grid_column_gutter_is_driven?authored_grid_column_gutter:parse_number(grid_column_gutter),
            grid_row_gutter_is_driven?authored_grid_row_gutter:parse_number(grid_row_gutter)};
        grid.columns_driver=grid_columns_driver;
        grid.columns_expression=grid_columns_expression;
        grid.rows_driver=grid_rows_driver;
        grid.rows_expression=grid_rows_expression;
        grid.column_gutter_driver=grid_column_gutter_driver;
        grid.column_gutter_expression=grid_column_gutter_expression;
        grid.row_gutter_driver=grid_row_gutter_driver;
        grid.row_gutter_expression=grid_row_gutter_expression;
        grid.bounds_x_driver=grid_bounds_x_driver;grid.bounds_x_expression=grid_bounds_x_expression;
        grid.bounds_y_driver=grid_bounds_y_driver;
        grid.bounds_y_expression=grid_bounds_y_expression;
        grid.bounds_width_driver=grid_bounds_width_driver;
        grid.bounds_width_expression=grid_bounds_width_expression;
        grid.bounds_height_driver=grid_bounds_height_driver;
        grid.bounds_height_expression=grid_bounds_height_expression;
        value.grid=std::move(grid);return set_layout_command(value,"layout.grid");
    };
    auto* grid_source_box=new QGroupBox("X source",grid_box);grid_source_box->setObjectName("grid-bounds-x-source");
    auto* grid_source_layout=new QVBoxLayout(grid_source_box);
    auto* grid_source_state=new QLabel(grid_source_box);grid_source_state->setObjectName("grid-bounds-x-source-state");
    QString grid_source_description="literal";
    if(grid_bounds_x_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==grid_bounds_x_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        grid_source_description="link · "+source_name+" ("+qs(grid_bounds_x_driver->object)+"/"+qs(grid_bounds_x_driver->field)+")";
    } else if(grid_bounds_x_expression)grid_source_description="expression · "+qs(grid_bounds_x_expression->source);
    const auto evaluated_grid_x=resolved.layout&&resolved.layout->grid?resolved.layout->grid->bounds.x:initial_grid.bounds.x;
    grid_source_state->setWordWrap(true);
    grid_source_state->setText("Source: "+grid_source_description+" · Literal: "+display_value(initial_grid.bounds.x)+
        " du · Evaluated: "+display_value(evaluated_grid_x)+" du");
    grid_source_layout->addWidget(grid_source_state);
    auto* grid_x_source=new QComboBox(grid_source_box);grid_x_source->setObjectName("grid-bounds-x-link-source");
    grid_x_source->setAccessibleName("Grid bounds x Artboard size source");
    std::vector<Ref> grid_x_sources;
    QStringList grid_x_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        grid_x_sources.push_back(source);
        grid_x_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* grid_x_search=add_artboard_source_search(grid_source_box,grid_source_layout,
        grid_x_source,grid_x_sources,grid_x_source_labels,grid_bounds_x_driver);
    grid_source_layout->addWidget(grid_x_source);
    auto* grid_x_replace=new QCheckBox("Replace current X source",grid_source_box);
    grid_x_replace->setObjectName("grid-bounds-x-replace");grid_x_replace->setEnabled(grid_bounds_x_is_driven);
    grid_source_layout->addWidget(grid_x_replace);
    auto* grid_source_actions=new QHBoxLayout;grid_source_layout->addLayout(grid_source_actions);
    auto* grid_x_link=new QPushButton("Link",grid_source_box);grid_x_link->setObjectName("grid-bounds-x-link");
    const bool grid_link_available=board.layout&&board.layout->grid&&!grid_x_sources.empty();
    grid_x_link->setEnabled(grid_link_available&&grid_x_source->currentIndex()>=0);grid_source_actions->addWidget(grid_x_link);
    connect(grid_x_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [grid_x_link,grid_link_available](int index){grid_x_link->setEnabled(grid_link_available&&index>=0);});
    connect(grid_x_search,&QLineEdit::textChanged,this,[grid_x_source,grid_x_link,grid_link_available](const QString&){
        grid_x_link->setEnabled(grid_link_available&&grid_x_source->currentIndex()>=0);
    });
    auto* grid_x_unlink=new QPushButton("Unlink · keep value",grid_source_box);grid_x_unlink->setObjectName("grid-bounds-x-unlink");
    grid_x_unlink->setEnabled(grid_bounds_x_is_driven);grid_source_actions->addWidget(grid_x_unlink);
    auto* grid_x_cancel=new QPushButton("Cancel draft",grid_source_box);grid_x_cancel->setObjectName("grid-bounds-x-cancel");
    grid_source_actions->addWidget(grid_x_cancel);
    auto* grid_x_expression=new ExpressionInput;grid_x_expression->setObjectName("grid-bounds-x-expression");
    grid_x_expression->setAccessibleName("Grid bounds x expression draft");grid_x_expression->setPlaceholderText("du expression using Artboard width/height ref() values");
    if(grid_bounds_x_expression)grid_x_expression->setPlainText(qs(grid_bounds_x_expression->source));
    grid_source_layout->addWidget(grid_x_expression);
    auto* grid_expression_actions=new QHBoxLayout;grid_source_layout->addLayout(grid_expression_actions);
    auto* grid_x_expression_apply=new QPushButton("Apply expression",grid_source_box);
    grid_x_expression_apply->setObjectName("grid-bounds-x-apply-expression");
    grid_x_expression_apply->setEnabled(board.layout&&board.layout->grid);grid_expression_actions->addWidget(grid_x_expression_apply);
    auto* grid_x_expression_cancel=new QPushButton("Cancel expression",grid_source_box);
    grid_x_expression_cancel->setObjectName("grid-bounds-x-cancel-expression");grid_expression_actions->addWidget(grid_x_expression_cancel);
    grid_form->addRow(grid_source_box);
    auto grid_source_commit=[this,frozen_session,frozen_revision](const std::vector<Command>& commands) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Grid source belongs to another document");
        canvas->cancel_interaction();host.session.apply(commands,frozen_revision);host.edited();
    };
    const Ref grid_columns_ref{grid_id,"","grid.columns"};
    auto* grid_columns_source_box=new QGroupBox("Columns source",grid_box);
    grid_columns_source_box->setObjectName("grid-columns-source");
    auto* grid_columns_source_layout=new QVBoxLayout(grid_columns_source_box);
    auto* grid_columns_source_state=new QLabel(grid_columns_source_box);
    grid_columns_source_state->setObjectName("grid-columns-source-state");
    QString grid_columns_source_description="literal";
    if(grid_columns_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.layout&&candidate.layout->grid&&candidate.layout->grid->id==grid_columns_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Grid"):
            qs(source_board->name)+" · Grid";
        grid_columns_source_description="link · "+source_name+" ("+qs(grid_columns_driver->object)+"/grid.columns)";
    } else if(grid_columns_expression)grid_columns_source_description="expression · "+qs(grid_columns_expression->source);
    grid_columns_source_state->setWordWrap(true);
    grid_columns_source_state->setText("Source: "+grid_columns_source_description+
        " · Literal: "+QString::number(static_cast<qulonglong>(initial_grid.columns))+
        " · Evaluated: "+QString::number(static_cast<qulonglong>(evaluated_grid_columns)));
    grid_columns_source_layout->addWidget(grid_columns_source_state);
    auto* grid_columns_source=new QComboBox(grid_columns_source_box);
    grid_columns_source->setObjectName("grid-columns-link-source");
    grid_columns_source->setAccessibleName("Grid columns source Grid");
    std::vector<Ref> grid_columns_sources;QStringList grid_columns_source_labels;
    for(const auto& candidate:comp.artboards)if(candidate.id!=id&&candidate.layout&&candidate.layout->grid) {
        const auto& source_grid=*candidate.layout->grid;
        grid_columns_sources.push_back(Ref{source_grid.id,"","grid.columns"});
        grid_columns_source_labels.push_back(qs(candidate.name)+" · Grid columns ("+qs(source_grid.id)+")");
    }
    auto* grid_columns_search=add_artboard_source_search(grid_columns_source_box,grid_columns_source_layout,
        grid_columns_source,grid_columns_sources,grid_columns_source_labels,grid_columns_driver);
    grid_columns_source_layout->addWidget(grid_columns_source);
    auto* grid_columns_replace=new QCheckBox("Replace current columns source",grid_columns_source_box);
    grid_columns_replace->setObjectName("grid-columns-replace");
    grid_columns_replace->setEnabled(grid_columns_is_driven);grid_columns_source_layout->addWidget(grid_columns_replace);
    auto* grid_columns_actions=new QHBoxLayout;grid_columns_source_layout->addLayout(grid_columns_actions);
    auto* grid_columns_link=new QPushButton("Link",grid_columns_source_box);
    grid_columns_link->setObjectName("grid-columns-link");
    const bool grid_columns_link_available=board.layout&&board.layout->grid&&!grid_columns_sources.empty();
    grid_columns_link->setEnabled(grid_columns_link_available&&grid_columns_source->currentIndex()>=0);
    grid_columns_actions->addWidget(grid_columns_link);
    connect(grid_columns_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [grid_columns_link,grid_columns_link_available](int index) {
            grid_columns_link->setEnabled(grid_columns_link_available&&index>=0);
        });
    connect(grid_columns_search,&QLineEdit::textChanged,this,
        [grid_columns_source,grid_columns_link,grid_columns_link_available](const QString&) {
            grid_columns_link->setEnabled(grid_columns_link_available&&grid_columns_source->currentIndex()>=0);
        });
    auto* grid_columns_unlink=new QPushButton("Unlink · keep value",grid_columns_source_box);
    grid_columns_unlink->setObjectName("grid-columns-unlink");
    grid_columns_unlink->setEnabled(grid_columns_is_driven);grid_columns_actions->addWidget(grid_columns_unlink);
    auto* grid_columns_cancel=new QPushButton("Cancel draft",grid_columns_source_box);
    grid_columns_cancel->setObjectName("grid-columns-cancel");grid_columns_actions->addWidget(grid_columns_cancel);
    auto* grid_columns_expression_input=new ExpressionInput;
    grid_columns_expression_input->setObjectName("grid-columns-expression");
    grid_columns_expression_input->setAccessibleName("Grid columns expression draft");
    grid_columns_expression_input->setPlaceholderText("unitless expression using Grid columns ref() values");
    if(grid_columns_expression)grid_columns_expression_input->setPlainText(qs(grid_columns_expression->source));
    grid_columns_source_layout->addWidget(grid_columns_expression_input);
    auto* grid_columns_expression_actions=new QHBoxLayout;grid_columns_source_layout->addLayout(grid_columns_expression_actions);
    auto* grid_columns_expression_apply=new QPushButton("Apply expression",grid_columns_source_box);
    grid_columns_expression_apply->setObjectName("grid-columns-apply-expression");
    grid_columns_expression_apply->setEnabled(board.layout&&board.layout->grid);
    grid_columns_expression_actions->addWidget(grid_columns_expression_apply);
    auto* grid_columns_expression_cancel=new QPushButton("Cancel expression",grid_columns_source_box);
    grid_columns_expression_cancel->setObjectName("grid-columns-cancel-expression");
    grid_columns_expression_actions->addWidget(grid_columns_expression_cancel);
    grid_form->addRow(grid_columns_source_box);
    connect(grid_columns_link,&QPushButton::clicked,this,
        [this,grid_columns_source,grid_columns_sources,grid_columns_ref,grid_columns_replace,grid_source_commit]{perform([&]{
        bool valid=false;const auto candidate=grid_columns_source->currentData(Qt::UserRole).toInt(&valid);
        if(grid_columns_source->currentIndex()<0||!valid||candidate<0||
            static_cast<std::size_t>(candidate)>=grid_columns_sources.size())
            throw Error("NO_SOURCE","Choose a Grid columns source");
        grid_source_commit({GridColumnsCommand{LinkGridColumns{grid_columns_ref,
            grid_columns_sources[static_cast<std::size_t>(candidate)],grid_columns_replace->isChecked()}}});
    });});
    connect(grid_columns_unlink,&QPushButton::clicked,this,[this,grid_columns_ref,grid_source_commit] {
        perform([&]{grid_source_commit({GridColumnsCommand{UnlinkGridColumns{grid_columns_ref}}});});
    });
    connect(grid_columns_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(grid_columns_expression_apply,&QPushButton::clicked,this,
        [this,grid_columns_expression_input,grid_columns_replace,grid_columns_ref,grid_source_commit]{perform([&]{
        grid_source_commit({GridColumnsCommand{SetGridColumnsExpression{grid_columns_ref,
            {grid_columns_expression_input->toPlainText().toStdString(),1},grid_columns_replace->isChecked()}}});
    });});
    connect(grid_columns_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    const Ref grid_rows_ref{grid_id,"","grid.rows"};
    auto* grid_rows_source_box=new QGroupBox("Rows source",grid_box);
    grid_rows_source_box->setObjectName("grid-rows-source");
    auto* grid_rows_source_layout=new QVBoxLayout(grid_rows_source_box);
    auto* grid_rows_source_state=new QLabel(grid_rows_source_box);
    grid_rows_source_state->setObjectName("grid-rows-source-state");
    QString grid_rows_source_description="literal";
    if(grid_rows_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.layout&&candidate.layout->grid&&candidate.layout->grid->id==grid_rows_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Grid"):
            qs(source_board->name)+" · Grid";
        grid_rows_source_description="link · "+source_name+" ("+qs(grid_rows_driver->object)+"/grid.rows)";
    } else if(grid_rows_expression)grid_rows_source_description="expression · "+qs(grid_rows_expression->source);
    grid_rows_source_state->setWordWrap(true);
    grid_rows_source_state->setText("Source: "+grid_rows_source_description+
        " · Literal: "+QString::number(static_cast<qulonglong>(initial_grid.rows))+" · Evaluated: "+
        QString::number(static_cast<qulonglong>(evaluated_grid_rows)));
    grid_rows_source_layout->addWidget(grid_rows_source_state);
    auto* grid_rows_source=new QComboBox(grid_rows_source_box);
    grid_rows_source->setObjectName("grid-rows-link-source");
    grid_rows_source->setAccessibleName("Grid rows source Grid");
    std::vector<Ref> grid_rows_sources;QStringList grid_rows_source_labels;
    for(const auto& candidate:comp.artboards)if(candidate.id!=id&&candidate.layout&&candidate.layout->grid) {
        const auto& source_grid=*candidate.layout->grid;
        grid_rows_sources.push_back(Ref{source_grid.id,"","grid.rows"});
        grid_rows_source_labels.push_back(qs(candidate.name)+" · Grid rows ("+qs(source_grid.id)+")");
    }
    auto* grid_rows_search=add_artboard_source_search(grid_rows_source_box,grid_rows_source_layout,
        grid_rows_source,grid_rows_sources,grid_rows_source_labels,grid_rows_driver);
    grid_rows_source_layout->addWidget(grid_rows_source);
    auto* grid_rows_replace=new QCheckBox("Replace current rows source",grid_rows_source_box);
    grid_rows_replace->setObjectName("grid-rows-replace");
    grid_rows_replace->setEnabled(grid_rows_is_driven);grid_rows_source_layout->addWidget(grid_rows_replace);
    auto* grid_rows_actions=new QHBoxLayout;grid_rows_source_layout->addLayout(grid_rows_actions);
    auto* grid_rows_link=new QPushButton("Link",grid_rows_source_box);
    grid_rows_link->setObjectName("grid-rows-link");
    const bool grid_rows_link_available=board.layout&&board.layout->grid&&!grid_rows_sources.empty();
    grid_rows_link->setEnabled(grid_rows_link_available&&grid_rows_source->currentIndex()>=0);
    grid_rows_actions->addWidget(grid_rows_link);
    connect(grid_rows_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [grid_rows_link,grid_rows_link_available](int index) {
            grid_rows_link->setEnabled(grid_rows_link_available&&index>=0);
        });
    connect(grid_rows_search,&QLineEdit::textChanged,this,
        [grid_rows_source,grid_rows_link,grid_rows_link_available](const QString&) {
            grid_rows_link->setEnabled(grid_rows_link_available&&grid_rows_source->currentIndex()>=0);
        });
    auto* grid_rows_unlink=new QPushButton("Unlink · keep value",grid_rows_source_box);
    grid_rows_unlink->setObjectName("grid-rows-unlink");
    grid_rows_unlink->setEnabled(grid_rows_is_driven);grid_rows_actions->addWidget(grid_rows_unlink);
    auto* grid_rows_cancel=new QPushButton("Cancel draft",grid_rows_source_box);
    grid_rows_cancel->setObjectName("grid-rows-cancel");grid_rows_actions->addWidget(grid_rows_cancel);
    grid_form->addRow(grid_rows_source_box);
    connect(grid_rows_link,&QPushButton::clicked,this,
        [this,grid_rows_source,grid_rows_sources,grid_rows_ref,grid_rows_replace,grid_source_commit]{perform([&]{
        bool valid=false;const auto candidate=grid_rows_source->currentData(Qt::UserRole).toInt(&valid);
        if(grid_rows_source->currentIndex()<0||!valid||candidate<0||
            static_cast<std::size_t>(candidate)>=grid_rows_sources.size())
            throw Error("NO_SOURCE","Choose a Grid rows source");
        grid_source_commit({GridRowsCommand{LinkGridRows{grid_rows_ref,
            grid_rows_sources[static_cast<std::size_t>(candidate)],grid_rows_replace->isChecked()}}});
    });});
    connect(grid_rows_unlink,&QPushButton::clicked,this,[this,grid_rows_ref,grid_source_commit] {
        perform([&]{grid_source_commit({GridRowsCommand{UnlinkGridRows{grid_rows_ref}}});});
    });
    connect(grid_rows_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    auto* grid_rows_expression_input=new ExpressionInput;
    grid_rows_expression_input->setParent(grid_rows_source_box);
    grid_rows_expression_input->setObjectName("grid-rows-expression");
    grid_rows_expression_input->setAccessibleName("Grid rows expression draft");
    grid_rows_expression_input->setPlaceholderText("unitless expression using Grid rows ref() values");
    if(grid_rows_expression)grid_rows_expression_input->setPlainText(qs(grid_rows_expression->source));
    grid_rows_source_layout->addWidget(grid_rows_expression_input);
    auto* grid_rows_expression_actions=new QHBoxLayout;grid_rows_source_layout->addLayout(grid_rows_expression_actions);
    auto* grid_rows_expression_apply=new QPushButton("Apply expression",grid_rows_source_box);
    grid_rows_expression_apply->setObjectName("grid-rows-apply-expression");
    grid_rows_expression_apply->setEnabled(board.layout&&board.layout->grid);
    grid_rows_expression_actions->addWidget(grid_rows_expression_apply);
    auto* grid_rows_expression_cancel=new QPushButton("Cancel expression",grid_rows_source_box);
    grid_rows_expression_cancel->setObjectName("grid-rows-cancel-expression");
    grid_rows_expression_actions->addWidget(grid_rows_expression_cancel);
    connect(grid_rows_expression_apply,&QPushButton::clicked,this,
        [this,grid_rows_expression_input,grid_rows_replace,grid_rows_ref,grid_source_commit]{perform([&]{
        grid_source_commit({GridRowsCommand{SetGridRowsExpression{grid_rows_ref,
            {grid_rows_expression_input->toPlainText().toStdString(),1},grid_rows_replace->isChecked()}}});
    });});
    connect(grid_rows_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    const Ref grid_bounds_x_ref{grid_id,"","grid.bounds.x"};
    const Ref grid_bounds_y_ref{grid_id,"","grid.bounds.y"};
    auto* grid_y_source_box=new QGroupBox("Y source",grid_box);grid_y_source_box->setObjectName("grid-bounds-y-source");
    auto* grid_y_source_layout=new QVBoxLayout(grid_y_source_box);
    auto* grid_y_source_state=new QLabel(grid_y_source_box);grid_y_source_state->setObjectName("grid-bounds-y-source-state");
    QString grid_y_source_description="literal";
    if(grid_bounds_y_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==grid_bounds_y_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        grid_y_source_description="link · "+source_name+" ("+qs(grid_bounds_y_driver->object)+"/"+qs(grid_bounds_y_driver->field)+")";
    } else if(grid_bounds_y_expression)grid_y_source_description="expression · "+qs(grid_bounds_y_expression->source);
    const auto evaluated_grid_y=resolved.layout&&resolved.layout->grid?resolved.layout->grid->bounds.y:initial_grid.bounds.y;
    grid_y_source_state->setWordWrap(true);
    grid_y_source_state->setText("Source: "+grid_y_source_description+" · Literal: "+display_value(initial_grid.bounds.y)+
        " du · Evaluated: "+display_value(evaluated_grid_y)+" du");
    grid_y_source_layout->addWidget(grid_y_source_state);
    auto* grid_y_source=new QComboBox(grid_y_source_box);grid_y_source->setObjectName("grid-bounds-y-link-source");
    grid_y_source->setAccessibleName("Grid bounds y Artboard size source");
    std::vector<Ref> grid_y_sources;
    QStringList grid_y_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        grid_y_sources.push_back(source);
        grid_y_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* grid_y_search=add_artboard_source_search(grid_y_source_box,grid_y_source_layout,
        grid_y_source,grid_y_sources,grid_y_source_labels,grid_bounds_y_driver);
    grid_y_source_layout->addWidget(grid_y_source);
    auto* grid_y_replace=new QCheckBox("Replace current Y source",grid_y_source_box);
    grid_y_replace->setObjectName("grid-bounds-y-replace");grid_y_replace->setEnabled(grid_bounds_y_is_driven);
    grid_y_source_layout->addWidget(grid_y_replace);
    auto* grid_y_source_actions=new QHBoxLayout;grid_y_source_layout->addLayout(grid_y_source_actions);
    auto* grid_y_link=new QPushButton("Link",grid_y_source_box);grid_y_link->setObjectName("grid-bounds-y-link");
    const bool grid_y_link_available=board.layout&&board.layout->grid&&!grid_y_sources.empty();
    grid_y_link->setEnabled(grid_y_link_available&&grid_y_source->currentIndex()>=0);grid_y_source_actions->addWidget(grid_y_link);
    connect(grid_y_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [grid_y_link,grid_y_link_available](int index){grid_y_link->setEnabled(grid_y_link_available&&index>=0);});
    connect(grid_y_search,&QLineEdit::textChanged,this,[grid_y_source,grid_y_link,grid_y_link_available](const QString&){
        grid_y_link->setEnabled(grid_y_link_available&&grid_y_source->currentIndex()>=0);
    });
    auto* grid_y_unlink=new QPushButton("Unlink · keep value",grid_y_source_box);grid_y_unlink->setObjectName("grid-bounds-y-unlink");
    grid_y_unlink->setEnabled(grid_bounds_y_is_driven);grid_y_source_actions->addWidget(grid_y_unlink);
    auto* grid_y_cancel=new QPushButton("Cancel draft",grid_y_source_box);grid_y_cancel->setObjectName("grid-bounds-y-cancel");
    grid_y_source_actions->addWidget(grid_y_cancel);
    auto* grid_y_expression=new ExpressionInput;grid_y_expression->setObjectName("grid-bounds-y-expression");
    grid_y_expression->setAccessibleName("Grid bounds y expression draft");
    grid_y_expression->setPlaceholderText("du expression using Artboard width/height ref() values");
    if(grid_bounds_y_expression)grid_y_expression->setPlainText(qs(grid_bounds_y_expression->source));
    grid_y_source_layout->addWidget(grid_y_expression);
    auto* grid_y_expression_actions=new QHBoxLayout;grid_y_source_layout->addLayout(grid_y_expression_actions);
    auto* grid_y_expression_apply=new QPushButton("Apply expression",grid_y_source_box);
    grid_y_expression_apply->setObjectName("grid-bounds-y-apply-expression");
    grid_y_expression_apply->setEnabled(board.layout&&board.layout->grid);grid_y_expression_actions->addWidget(grid_y_expression_apply);
    auto* grid_y_expression_cancel=new QPushButton("Cancel expression",grid_y_source_box);
    grid_y_expression_cancel->setObjectName("grid-bounds-y-cancel-expression");grid_y_expression_actions->addWidget(grid_y_expression_cancel);
    grid_form->addRow(grid_y_source_box);
    connect(grid_y_link,&QPushButton::clicked,this,[this,grid_y_source,grid_y_sources,grid_bounds_y_ref,grid_y_replace,grid_source_commit]{perform([&]{
        bool valid=false;const auto candidate=grid_y_source->currentData(Qt::UserRole).toInt(&valid);
        if(grid_y_source->currentIndex()<0||!valid||candidate<0||static_cast<std::size_t>(candidate)>=grid_y_sources.size())
            throw Error("NO_SOURCE","Choose an Artboard size source");
        grid_source_commit({GridBoundsYCommand{LinkGridBoundsY{grid_bounds_y_ref,
            grid_y_sources[static_cast<std::size_t>(candidate)],grid_y_replace->isChecked()}}});
    });});
    connect(grid_y_unlink,&QPushButton::clicked,this,[this,grid_bounds_y_ref,grid_source_commit]{
        perform([&]{grid_source_commit({GridBoundsYCommand{UnlinkGridBoundsY{grid_bounds_y_ref}}});});
    });
    connect(grid_y_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(grid_y_expression_apply,&QPushButton::clicked,this,[this,grid_y_expression,grid_y_replace,grid_bounds_y_ref,grid_source_commit]{perform([&]{
        grid_source_commit({GridBoundsYCommand{SetGridBoundsYExpression{grid_bounds_y_ref,
            {grid_y_expression->toPlainText().toStdString(),1},grid_y_replace->isChecked()}}});
    });});
    connect(grid_y_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(grid_x_link,&QPushButton::clicked,this,[this,grid_x_source,grid_x_sources,grid_bounds_x_ref,grid_x_replace,grid_source_commit]{perform([&]{
        bool valid=false;const auto candidate=grid_x_source->currentData(Qt::UserRole).toInt(&valid);
        if(grid_x_source->currentIndex()<0||!valid||candidate<0||static_cast<std::size_t>(candidate)>=grid_x_sources.size())
            throw Error("NO_SOURCE","Choose an Artboard size source");
        grid_source_commit({GridBoundsXCommand{LinkGridBoundsX{grid_bounds_x_ref,
            grid_x_sources[static_cast<std::size_t>(candidate)],grid_x_replace->isChecked()}}});
    });});
    connect(grid_x_unlink,&QPushButton::clicked,this,[this,grid_bounds_x_ref,grid_source_commit]{
        perform([&]{grid_source_commit({GridBoundsXCommand{UnlinkGridBoundsX{grid_bounds_x_ref}}});});
    });
    connect(grid_x_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(grid_x_expression_apply,&QPushButton::clicked,this,[this,grid_x_expression,grid_x_replace,grid_bounds_x_ref,grid_source_commit]{perform([&]{
        grid_source_commit({GridBoundsXCommand{SetGridBoundsXExpression{grid_bounds_x_ref,
            {grid_x_expression->toPlainText().toStdString(),1},grid_x_replace->isChecked()}}});
    });});
    connect(grid_x_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    const Ref grid_bounds_width_ref{grid_id,"","grid.bounds.width"};
    auto* grid_width_source_box=new QGroupBox("Width source",grid_box);grid_width_source_box->setObjectName("grid-bounds-width-source");
    auto* grid_width_source_layout=new QVBoxLayout(grid_width_source_box);
    auto* grid_width_source_state=new QLabel(grid_width_source_box);grid_width_source_state->setObjectName("grid-bounds-width-source-state");
    QString grid_width_source_description="literal";
    if(grid_bounds_width_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==grid_bounds_width_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        grid_width_source_description="link · "+source_name+" ("+qs(grid_bounds_width_driver->object)+"/"+
            qs(grid_bounds_width_driver->field)+")";
    } else if(grid_bounds_width_expression)
        grid_width_source_description="expression · "+qs(grid_bounds_width_expression->source);
    const auto evaluated_grid_width=resolved.layout&&resolved.layout->grid?
        resolved.layout->grid->bounds.width:initial_grid.bounds.width;
    grid_width_source_state->setWordWrap(true);
    grid_width_source_state->setText("Source: "+grid_width_source_description+" · Literal: "+display_value(initial_grid.bounds.width)+
        " du · Evaluated: "+display_value(evaluated_grid_width)+" du");
    grid_width_source_layout->addWidget(grid_width_source_state);
    auto* grid_width_source=new QComboBox(grid_width_source_box);grid_width_source->setObjectName("grid-bounds-width-link-source");
    grid_width_source->setAccessibleName("Grid bounds width Artboard size source");
    std::vector<Ref> grid_width_sources;
    QStringList grid_width_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        grid_width_sources.push_back(source);
        grid_width_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* grid_width_search=add_artboard_source_search(grid_width_source_box,grid_width_source_layout,
        grid_width_source,grid_width_sources,grid_width_source_labels,grid_bounds_width_driver);
    grid_width_source_layout->addWidget(grid_width_source);
    auto* grid_width_replace=new QCheckBox("Replace current width source",grid_width_source_box);
    grid_width_replace->setObjectName("grid-bounds-width-replace");grid_width_replace->setEnabled(grid_bounds_width_is_driven);
    grid_width_source_layout->addWidget(grid_width_replace);
    auto* grid_width_source_actions=new QHBoxLayout;grid_width_source_layout->addLayout(grid_width_source_actions);
    auto* grid_width_link=new QPushButton("Link",grid_width_source_box);grid_width_link->setObjectName("grid-bounds-width-link");
    const bool grid_width_link_available=board.layout&&board.layout->grid&&!grid_width_sources.empty();
    grid_width_link->setEnabled(grid_width_link_available&&grid_width_source->currentIndex()>=0);
    grid_width_source_actions->addWidget(grid_width_link);
    connect(grid_width_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [grid_width_link,grid_width_link_available](int index){grid_width_link->setEnabled(grid_width_link_available&&index>=0);});
    connect(grid_width_search,&QLineEdit::textChanged,this,[grid_width_source,grid_width_link,grid_width_link_available](const QString&){
        grid_width_link->setEnabled(grid_width_link_available&&grid_width_source->currentIndex()>=0);
    });
    auto* grid_width_unlink=new QPushButton("Unlink · keep value",grid_width_source_box);
    grid_width_unlink->setObjectName("grid-bounds-width-unlink");
    grid_width_unlink->setEnabled(grid_bounds_width_is_driven);grid_width_source_actions->addWidget(grid_width_unlink);
    auto* grid_width_cancel=new QPushButton("Cancel draft",grid_width_source_box);
    grid_width_cancel->setObjectName("grid-bounds-width-cancel");grid_width_source_actions->addWidget(grid_width_cancel);
    auto* grid_width_expression=new QPlainTextEdit(grid_width_source_box);
    grid_width_expression->setObjectName("grid-bounds-width-expression");
    grid_width_expression->setAccessibleName("Grid bounds width expression draft");
    grid_width_expression->setPlaceholderText("du expression using ref(\"artboard-id\",\"\",\"artboard.width\")");
    if(grid_bounds_width_expression)grid_width_expression->setPlainText(qs(grid_bounds_width_expression->source));
    grid_width_source_layout->addWidget(grid_width_expression);
    auto* grid_width_expression_actions=new QHBoxLayout;grid_width_source_layout->addLayout(grid_width_expression_actions);
    auto* grid_width_expression_apply=new QPushButton("Apply expression",grid_width_source_box);
    grid_width_expression_apply->setObjectName("grid-bounds-width-apply-expression");
    grid_width_expression_apply->setEnabled(board.layout&&board.layout->grid);
    grid_width_expression_actions->addWidget(grid_width_expression_apply);
    auto* grid_width_expression_cancel=new QPushButton("Cancel expression",grid_width_source_box);
    grid_width_expression_cancel->setObjectName("grid-bounds-width-cancel-expression");
    grid_width_expression_actions->addWidget(grid_width_expression_cancel);
    grid_form->addRow(grid_width_source_box);
    connect(grid_width_link,&QPushButton::clicked,this,[this,grid_width_source,grid_width_sources,grid_bounds_width_ref,
        grid_width_replace,grid_source_commit]{perform([&]{
        bool valid=false;const auto candidate=grid_width_source->currentData(Qt::UserRole).toInt(&valid);
        if(grid_width_source->currentIndex()<0||!valid||candidate<0||static_cast<std::size_t>(candidate)>=grid_width_sources.size())
            throw Error("NO_SOURCE","Choose an Artboard size source");
        grid_source_commit({GridBoundsWidthCommand{LinkGridBoundsWidth{grid_bounds_width_ref,
            grid_width_sources[static_cast<std::size_t>(candidate)],grid_width_replace->isChecked()}}});
    });});
    connect(grid_width_unlink,&QPushButton::clicked,this,[this,grid_bounds_width_ref,grid_source_commit]{
        perform([&]{grid_source_commit({GridBoundsWidthCommand{UnlinkGridBoundsWidth{grid_bounds_width_ref}}});});
    });
    connect(grid_width_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(grid_width_expression_apply,&QPushButton::clicked,this,[this,grid_bounds_width_ref,grid_width_expression,
        grid_width_replace,grid_source_commit]{perform([&]{
        grid_source_commit({GridBoundsWidthCommand{SetGridBoundsWidthExpression{grid_bounds_width_ref,
            {grid_width_expression->toPlainText().toStdString(),1},grid_width_replace->isChecked()}}});
    });});
    connect(grid_width_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    const Ref grid_bounds_height_ref{grid_id,"","grid.bounds.height"};
    auto* grid_height_source_box=new QGroupBox("Height source",grid_box);
    grid_height_source_box->setObjectName("grid-bounds-height-source");
    auto* grid_height_source_layout=new QVBoxLayout(grid_height_source_box);
    auto* grid_height_source_state=new QLabel(grid_height_source_box);
    grid_height_source_state->setObjectName("grid-bounds-height-source-state");
    QString grid_height_source_description="literal";
    if(grid_bounds_height_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==grid_bounds_height_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        grid_height_source_description="link · "+source_name+" ("+qs(grid_bounds_height_driver->object)+"/"+
            qs(grid_bounds_height_driver->field)+")";
    } else if(grid_bounds_height_expression)
        grid_height_source_description="expression · "+qs(grid_bounds_height_expression->source);
    const auto evaluated_grid_height=resolved.layout&&resolved.layout->grid?
        resolved.layout->grid->bounds.height:initial_grid.bounds.height;
    grid_height_source_state->setWordWrap(true);
    grid_height_source_state->setText("Source: "+grid_height_source_description+" · Literal: "+display_value(initial_grid.bounds.height)+
        " du · Evaluated: "+display_value(evaluated_grid_height)+" du");
    grid_height_source_layout->addWidget(grid_height_source_state);
    auto* grid_height_source=new QComboBox(grid_height_source_box);
    grid_height_source->setObjectName("grid-bounds-height-link-source");
    grid_height_source->setAccessibleName("Grid bounds height Artboard size source");
    std::vector<Ref> grid_height_sources;
    QStringList grid_height_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        grid_height_sources.push_back(source);
        grid_height_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* grid_height_search=add_artboard_source_search(grid_height_source_box,grid_height_source_layout,
        grid_height_source,grid_height_sources,grid_height_source_labels,grid_bounds_height_driver);
    grid_height_source_layout->addWidget(grid_height_source);
    auto* grid_height_replace=new QCheckBox("Replace current height source",grid_height_source_box);
    grid_height_replace->setObjectName("grid-bounds-height-replace");
    grid_height_replace->setEnabled(grid_bounds_height_is_driven);
    grid_height_source_layout->addWidget(grid_height_replace);
    auto* grid_height_source_actions=new QHBoxLayout;
    grid_height_source_layout->addLayout(grid_height_source_actions);
    auto* grid_height_link=new QPushButton("Link",grid_height_source_box);
    grid_height_link->setObjectName("grid-bounds-height-link");
    const bool grid_height_link_available=board.layout&&board.layout->grid&&!grid_height_sources.empty();
    grid_height_link->setEnabled(grid_height_link_available&&grid_height_source->currentIndex()>=0);
    grid_height_source_actions->addWidget(grid_height_link);
    connect(grid_height_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [grid_height_link,grid_height_link_available](int index){grid_height_link->setEnabled(grid_height_link_available&&index>=0);});
    connect(grid_height_search,&QLineEdit::textChanged,this,[grid_height_source,grid_height_link,grid_height_link_available](const QString&){
        grid_height_link->setEnabled(grid_height_link_available&&grid_height_source->currentIndex()>=0);
    });
    auto* grid_height_unlink=new QPushButton("Unlink · keep value",grid_height_source_box);
    grid_height_unlink->setObjectName("grid-bounds-height-unlink");
    grid_height_unlink->setEnabled(grid_bounds_height_is_driven);
    grid_height_source_actions->addWidget(grid_height_unlink);
    auto* grid_height_cancel=new QPushButton("Cancel draft",grid_height_source_box);
    grid_height_cancel->setObjectName("grid-bounds-height-cancel");
    grid_height_source_actions->addWidget(grid_height_cancel);
    auto* grid_height_expression=new QPlainTextEdit(grid_height_source_box);
    grid_height_expression->setObjectName("grid-bounds-height-expression");
    grid_height_expression->setAccessibleName("Grid bounds height expression draft");
    grid_height_expression->setPlaceholderText("du expression using ref(\"artboard-id\",\"\",\"artboard.height\")");
    if(grid_bounds_height_expression)grid_height_expression->setPlainText(qs(grid_bounds_height_expression->source));
    grid_height_source_layout->addWidget(grid_height_expression);
    auto* grid_height_expression_actions=new QHBoxLayout;
    grid_height_source_layout->addLayout(grid_height_expression_actions);
    auto* grid_height_expression_apply=new QPushButton("Apply expression",grid_height_source_box);
    grid_height_expression_apply->setObjectName("grid-bounds-height-apply-expression");
    grid_height_expression_apply->setEnabled(board.layout&&board.layout->grid);
    grid_height_expression_actions->addWidget(grid_height_expression_apply);
    auto* grid_height_expression_cancel=new QPushButton("Cancel expression",grid_height_source_box);
    grid_height_expression_cancel->setObjectName("grid-bounds-height-cancel-expression");
    grid_height_expression_actions->addWidget(grid_height_expression_cancel);
    grid_form->addRow(grid_height_source_box);
    connect(grid_height_link,&QPushButton::clicked,this,[this,grid_height_source,grid_height_sources,grid_bounds_height_ref,
        grid_height_replace,grid_source_commit]{perform([&]{
        bool valid=false;const auto candidate=grid_height_source->currentData(Qt::UserRole).toInt(&valid);
        if(grid_height_source->currentIndex()<0||!valid||candidate<0||static_cast<std::size_t>(candidate)>=grid_height_sources.size())
            throw Error("NO_SOURCE","Choose an Artboard size source");
        grid_source_commit({GridBoundsHeightCommand{LinkGridBoundsHeight{grid_bounds_height_ref,
            grid_height_sources[static_cast<std::size_t>(candidate)],grid_height_replace->isChecked()}}});
    });});
    connect(grid_height_unlink,&QPushButton::clicked,this,[this,grid_bounds_height_ref,grid_source_commit]{
        perform([&]{grid_source_commit({GridBoundsHeightCommand{UnlinkGridBoundsHeight{grid_bounds_height_ref}}});});
    });
    connect(grid_height_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    connect(grid_height_expression_apply,&QPushButton::clicked,this,[this,grid_bounds_height_ref,grid_height_expression,
        grid_height_replace,grid_source_commit]{perform([&]{
        grid_source_commit({GridBoundsHeightCommand{SetGridBoundsHeightExpression{grid_bounds_height_ref,
            {grid_height_expression->toPlainText().toStdString(),1},grid_height_replace->isChecked()}}});
    });});
    connect(grid_height_expression_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    const Ref grid_column_gutter_ref{grid_id,"","grid.column_gutter"};
    auto* grid_column_gutter_source_box=new QGroupBox("Column gutter source",grid_box);
    grid_column_gutter_source_box->setObjectName("grid-column-gutter-source");
    auto* grid_column_gutter_source_layout=new QVBoxLayout(grid_column_gutter_source_box);
    auto* grid_column_gutter_source_state=new QLabel(grid_column_gutter_source_box);
    grid_column_gutter_source_state->setObjectName("grid-column-gutter-source-state");
    QString grid_column_gutter_source_description="literal";
    if(grid_column_gutter_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==grid_column_gutter_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        grid_column_gutter_source_description="link · "+source_name+" ("+qs(grid_column_gutter_driver->object)+"/"+
            qs(grid_column_gutter_driver->field)+")";
    } else if(grid_column_gutter_expression)
        grid_column_gutter_source_description="expression · "+qs(grid_column_gutter_expression->source);
    const auto evaluated_grid_column_gutter=resolved.layout&&resolved.layout->grid?
        resolved.layout->grid->column_gutter:initial_grid.column_gutter;
    grid_column_gutter_source_state->setWordWrap(true);
    grid_column_gutter_source_state->setText("Source: "+grid_column_gutter_source_description+" · Literal: "+
        display_value(initial_grid.column_gutter)+" du · Evaluated: "+display_value(evaluated_grid_column_gutter)+" du");
    grid_column_gutter_source_layout->addWidget(grid_column_gutter_source_state);
    auto* grid_column_gutter_source=new QComboBox(grid_column_gutter_source_box);
    grid_column_gutter_source->setObjectName("grid-column-gutter-link-source");
    grid_column_gutter_source->setAccessibleName("Grid column gutter Artboard size source");
    std::vector<Ref> grid_column_gutter_sources;
    QStringList grid_column_gutter_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        grid_column_gutter_sources.push_back(source);
        grid_column_gutter_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* grid_column_gutter_search=add_artboard_source_search(grid_column_gutter_source_box,
        grid_column_gutter_source_layout,grid_column_gutter_source,grid_column_gutter_sources,
        grid_column_gutter_source_labels,grid_column_gutter_driver);
    grid_column_gutter_source_layout->addWidget(grid_column_gutter_source);
    auto* grid_column_gutter_replace=new QCheckBox("Replace current column gutter source",grid_column_gutter_source_box);
    grid_column_gutter_replace->setObjectName("grid-column-gutter-replace");
    grid_column_gutter_replace->setEnabled(grid_column_gutter_is_driven);
    grid_column_gutter_source_layout->addWidget(grid_column_gutter_replace);
    auto* grid_column_gutter_source_actions=new QHBoxLayout;
    grid_column_gutter_source_layout->addLayout(grid_column_gutter_source_actions);
    auto* grid_column_gutter_link=new QPushButton("Link",grid_column_gutter_source_box);
    grid_column_gutter_link->setObjectName("grid-column-gutter-link");
    const bool grid_column_gutter_link_available=board.layout&&board.layout->grid&&!grid_column_gutter_sources.empty();
    grid_column_gutter_link->setEnabled(grid_column_gutter_link_available&&grid_column_gutter_source->currentIndex()>=0);
    grid_column_gutter_source_actions->addWidget(grid_column_gutter_link);
    connect(grid_column_gutter_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [grid_column_gutter_link,grid_column_gutter_link_available](int index) {
            grid_column_gutter_link->setEnabled(grid_column_gutter_link_available&&index>=0);
        });
    connect(grid_column_gutter_search,&QLineEdit::textChanged,this,
        [grid_column_gutter_source,grid_column_gutter_link,grid_column_gutter_link_available](const QString&) {
            grid_column_gutter_link->setEnabled(grid_column_gutter_link_available&&grid_column_gutter_source->currentIndex()>=0);
        });
    auto* grid_column_gutter_unlink=new QPushButton("Unlink · keep value",grid_column_gutter_source_box);
    grid_column_gutter_unlink->setObjectName("grid-column-gutter-unlink");
    grid_column_gutter_unlink->setEnabled(grid_column_gutter_is_driven);
    grid_column_gutter_source_actions->addWidget(grid_column_gutter_unlink);
    auto* grid_column_gutter_cancel=new QPushButton("Cancel draft",grid_column_gutter_source_box);
    grid_column_gutter_cancel->setObjectName("grid-column-gutter-cancel");
    grid_column_gutter_source_actions->addWidget(grid_column_gutter_cancel);
    grid_form->addRow(grid_column_gutter_source_box);
    connect(grid_column_gutter_link,&QPushButton::clicked,this,
        [this,grid_column_gutter_source,grid_column_gutter_sources,grid_column_gutter_ref,
            grid_column_gutter_replace,grid_source_commit]{perform([&]{
            bool valid=false;const auto candidate=grid_column_gutter_source->currentData(Qt::UserRole).toInt(&valid);
            if(grid_column_gutter_source->currentIndex()<0||!valid||candidate<0||
                static_cast<std::size_t>(candidate)>=grid_column_gutter_sources.size())
                throw Error("NO_SOURCE","Choose an Artboard size source");
            grid_source_commit({GridColumnGutterCommand{LinkGridColumnGutter{grid_column_gutter_ref,
                grid_column_gutter_sources[static_cast<std::size_t>(candidate)],grid_column_gutter_replace->isChecked()}}});
        });});
    connect(grid_column_gutter_unlink,&QPushButton::clicked,this,[this,grid_column_gutter_ref,grid_source_commit]{
        perform([&]{grid_source_commit({GridColumnGutterCommand{UnlinkGridColumnGutter{grid_column_gutter_ref}}});});
    });
    connect(grid_column_gutter_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    auto* grid_column_gutter_expression_input=new ExpressionInput;
    grid_column_gutter_expression_input->setObjectName("grid-column-gutter-expression");
    grid_column_gutter_expression_input->setAccessibleName("Grid column gutter expression draft");
    grid_column_gutter_expression_input->setFixedHeight(58);
    grid_column_gutter_expression_input->setPlaceholderText("du expression using Artboard width/height ref() values");
    if(grid_column_gutter_expression)grid_column_gutter_expression_input->setPlainText(qs(grid_column_gutter_expression->source));
    grid_column_gutter_source_layout->addWidget(grid_column_gutter_expression_input);
    auto* grid_column_gutter_apply_expression=new QPushButton("Apply expression",grid_column_gutter_source_box);
    grid_column_gutter_apply_expression->setObjectName("grid-column-gutter-apply-expression");
    grid_column_gutter_apply_expression->setEnabled(board.layout&&board.layout->grid);
    grid_column_gutter_source_layout->addWidget(grid_column_gutter_apply_expression);
    connect(grid_column_gutter_apply_expression,&QPushButton::clicked,this,
        [this,grid_column_gutter_expression_input,grid_column_gutter_ref,grid_column_gutter_replace,grid_source_commit]{perform([&]{
            grid_source_commit({GridColumnGutterCommand{SetGridColumnGutterExpression{grid_column_gutter_ref,
                {grid_column_gutter_expression_input->toPlainText().toStdString(),1},grid_column_gutter_replace->isChecked()}}});
        });});
    const Ref grid_row_gutter_ref{grid_id,"","grid.row_gutter"};
    auto* grid_row_gutter_source_box=new QGroupBox("Row gutter source",grid_box);
    grid_row_gutter_source_box->setObjectName("grid-row-gutter-source");
    auto* grid_row_gutter_source_layout=new QVBoxLayout(grid_row_gutter_source_box);
    auto* grid_row_gutter_source_state=new QLabel(grid_row_gutter_source_box);
    grid_row_gutter_source_state->setObjectName("grid-row-gutter-source-state");
    QString grid_row_gutter_source_description="literal";
    if(grid_row_gutter_driver) {
        const auto source_board=std::find_if(comp.artboards.begin(),comp.artboards.end(),[&](const Artboard& candidate) {
            return candidate.id==grid_row_gutter_driver->object;
        });
        const auto source_name=source_board==comp.artboards.end()?QString("Missing Artboard"):qs(source_board->name);
        grid_row_gutter_source_description="link · "+source_name+" ("+qs(grid_row_gutter_driver->object)+"/"+
            qs(grid_row_gutter_driver->field)+")";
    } else if(grid_row_gutter_expression)
        grid_row_gutter_source_description="expression · "+qs(grid_row_gutter_expression->source);
    const auto evaluated_grid_row_gutter=resolved.layout&&resolved.layout->grid?
        resolved.layout->grid->row_gutter:initial_grid.row_gutter;
    grid_row_gutter_source_state->setWordWrap(true);
    grid_row_gutter_source_state->setText("Source: "+grid_row_gutter_source_description+" · Literal: "+
        display_value(initial_grid.row_gutter)+" du · Evaluated: "+display_value(evaluated_grid_row_gutter)+" du");
    grid_row_gutter_source_layout->addWidget(grid_row_gutter_source_state);
    auto* grid_row_gutter_source=new QComboBox(grid_row_gutter_source_box);
    grid_row_gutter_source->setObjectName("grid-row-gutter-link-source");
    grid_row_gutter_source->setAccessibleName("Grid row gutter Artboard size source");
    std::vector<Ref> grid_row_gutter_sources;
    QStringList grid_row_gutter_source_labels;
    for(const auto& candidate:comp.artboards)for(const bool source_width:{true,false}) {
        if(candidate.id==id)continue;
        Ref source{candidate.id,"",source_width?"artboard.width":"artboard.height"};
        grid_row_gutter_sources.push_back(source);
        grid_row_gutter_source_labels.push_back(qs(candidate.name)+" · "+(source_width?"width":"height")+
            " ("+qs(candidate.id)+"/"+qs(source.field)+")");
    }
    auto* grid_row_gutter_search=add_artboard_source_search(grid_row_gutter_source_box,
        grid_row_gutter_source_layout,grid_row_gutter_source,grid_row_gutter_sources,
        grid_row_gutter_source_labels,grid_row_gutter_driver);
    grid_row_gutter_source_layout->addWidget(grid_row_gutter_source);
    auto* grid_row_gutter_replace=new QCheckBox("Replace current row gutter source",grid_row_gutter_source_box);
    grid_row_gutter_replace->setObjectName("grid-row-gutter-replace");
    grid_row_gutter_replace->setEnabled(grid_row_gutter_is_driven);
    grid_row_gutter_source_layout->addWidget(grid_row_gutter_replace);
    auto* grid_row_gutter_source_actions=new QHBoxLayout;
    grid_row_gutter_source_layout->addLayout(grid_row_gutter_source_actions);
    auto* grid_row_gutter_link=new QPushButton("Link",grid_row_gutter_source_box);
    grid_row_gutter_link->setObjectName("grid-row-gutter-link");
    const bool grid_row_gutter_link_available=board.layout&&board.layout->grid&&!grid_row_gutter_sources.empty();
    grid_row_gutter_link->setEnabled(grid_row_gutter_link_available&&grid_row_gutter_source->currentIndex()>=0);
    grid_row_gutter_source_actions->addWidget(grid_row_gutter_link);
    connect(grid_row_gutter_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
        [grid_row_gutter_link,grid_row_gutter_link_available](int index) {
            grid_row_gutter_link->setEnabled(grid_row_gutter_link_available&&index>=0);
        });
    connect(grid_row_gutter_search,&QLineEdit::textChanged,this,
        [grid_row_gutter_source,grid_row_gutter_link,grid_row_gutter_link_available](const QString&) {
            grid_row_gutter_link->setEnabled(grid_row_gutter_link_available&&grid_row_gutter_source->currentIndex()>=0);
        });
    auto* grid_row_gutter_unlink=new QPushButton("Unlink · keep value",grid_row_gutter_source_box);
    grid_row_gutter_unlink->setObjectName("grid-row-gutter-unlink");
    grid_row_gutter_unlink->setEnabled(grid_row_gutter_is_driven);
    grid_row_gutter_source_actions->addWidget(grid_row_gutter_unlink);
    auto* grid_row_gutter_cancel=new QPushButton("Cancel draft",grid_row_gutter_source_box);
    grid_row_gutter_cancel->setObjectName("grid-row-gutter-cancel");
    grid_row_gutter_source_actions->addWidget(grid_row_gutter_cancel);
    grid_form->addRow(grid_row_gutter_source_box);
    connect(grid_row_gutter_link,&QPushButton::clicked,this,
        [this,grid_row_gutter_source,grid_row_gutter_sources,grid_row_gutter_ref,
            grid_row_gutter_replace,grid_source_commit]{perform([&]{
            bool valid=false;const auto candidate=grid_row_gutter_source->currentData(Qt::UserRole).toInt(&valid);
            if(grid_row_gutter_source->currentIndex()<0||!valid||candidate<0||
                static_cast<std::size_t>(candidate)>=grid_row_gutter_sources.size())
                throw Error("NO_SOURCE","Choose an Artboard size source");
            grid_source_commit({GridRowGutterCommand{LinkGridRowGutter{grid_row_gutter_ref,
                grid_row_gutter_sources[static_cast<std::size_t>(candidate)],grid_row_gutter_replace->isChecked()}}});
        });});
    connect(grid_row_gutter_unlink,&QPushButton::clicked,this,[this,grid_row_gutter_ref,grid_source_commit]{
        perform([&]{grid_source_commit({GridRowGutterCommand{UnlinkGridRowGutter{grid_row_gutter_ref}}});});
    });
    auto* grid_row_gutter_expression_input=new ExpressionInput;
    grid_row_gutter_expression_input->setObjectName("grid-row-gutter-expression");
    grid_row_gutter_expression_input->setAccessibleName("Grid row gutter expression draft");
    grid_row_gutter_expression_input->setFixedHeight(58);
    grid_row_gutter_expression_input->setPlaceholderText("du expression using Artboard width/height ref() values");
    if(grid_row_gutter_expression)grid_row_gutter_expression_input->setPlainText(qs(grid_row_gutter_expression->source));
    grid_row_gutter_source_layout->addWidget(grid_row_gutter_expression_input);
    auto* grid_row_gutter_apply_expression=new QPushButton("Apply expression",grid_row_gutter_source_box);
    grid_row_gutter_apply_expression->setObjectName("grid-row-gutter-apply-expression");
    grid_row_gutter_apply_expression->setEnabled(board.layout&&board.layout->grid);
    grid_row_gutter_source_layout->addWidget(grid_row_gutter_apply_expression);
    connect(grid_row_gutter_apply_expression,&QPushButton::clicked,this,
        [this,grid_row_gutter_expression_input,grid_row_gutter_ref,grid_row_gutter_replace,grid_source_commit]{perform([&]{
            grid_source_commit({GridRowGutterCommand{SetGridRowGutterExpression{grid_row_gutter_ref,
                {grid_row_gutter_expression_input->toPlainText().toStdString(),1},grid_row_gutter_replace->isChecked()}}});
        });});
    connect(grid_row_gutter_cancel,&QPushButton::clicked,this,cancel_layout_editor);
    auto* grid_actions=new QWidget(grid_box);auto* grid_buttons=new QHBoxLayout(grid_actions);grid_buttons->setContentsMargins(0,0,0,0);
    auto* grid_apply=new QPushButton("Apply Grid",grid_actions);grid_apply->setObjectName("grid-apply");grid_buttons->addWidget(grid_apply);
    auto* grid_copy=new QPushButton("Set Grid to margin box",grid_actions);grid_copy->setObjectName("grid-copy-margin-box");grid_copy->setToolTip("Copy the evaluated Margin box once; later Margin edits do not change Grid.");grid_buttons->addWidget(grid_copy);
    auto* grid_clear=new QPushButton("Clear Grid",grid_actions);grid_clear->setObjectName("grid-clear");grid_clear->setEnabled(resolved.layout&&resolved.layout->grid);grid_buttons->addWidget(grid_clear);
    grid_form->addRow(grid_actions);
    for(auto* input:{grid_x,grid_y,grid_width,grid_height,grid_columns,grid_rows,grid_column_gutter,grid_row_gutter})bind_number(input,grid_box,grid_builder);
    connect(grid_apply,&QPushButton::clicked,this,[commit_from,grid_box,grid_builder]{commit_from(grid_box,grid_builder);});
    connect(grid_copy,&QPushButton::clicked,this,[this,read,composition,id,grid_id,set_layout_command,commit_explicit,guard_editor,grid_box] {
        if(guard_editor())return;
        const auto current=read();
        const auto board_now=evaluate_artboard(find_composition(host.session.document(),composition),id);
        if(!board_now.layout||!board_now.layout->margin) {statusBar()->showMessage("INVALID_LAYOUT: Add a Margin before copying its box",12000);return;}
        const auto& margin=*board_now.layout->margin;auto value=current.layout.value_or(ArtboardLayout{});
        auto grid=value.grid.value_or(Grid{grid_id,{},1,1,0,0});
        if(!value.grid&&board_now.layout->grid) {
            // Inherited source metadata belongs to its authored owner. This is a
            // one-shot literal family override, not a cloned source dependency.
            const auto& inherited=*board_now.layout->grid;
            grid=Grid{grid_id,inherited.bounds,inherited.columns,inherited.rows,
                inherited.column_gutter,inherited.row_gutter};
        }
        grid.bounds={margin.left,margin.top,board_now.width-margin.left-margin.right,
            board_now.height-margin.top-margin.bottom};
        value.grid=std::move(grid);
        commit_explicit(grid_box,[set_layout_command,value]{return set_layout_command(value,"layout.grid");});
    });
    connect(grid_clear,&QPushButton::clicked,this,[read,set_layout_command,commit_explicit,guard_editor,grid_box] {
        if(guard_editor())return;
        auto current=read();auto value=current.layout.value_or(ArtboardLayout{});value.grid.reset();
        commit_explicit(grid_box,[set_layout_command,value]{return set_layout_command(value,"layout.grid");});
    });
    layout->addWidget(grid_box);

    auto* guides_box=new QGroupBox("Composition Guides");guides_box->setObjectName("composition-guides");auto* guides_layout=new QVBoxLayout(guides_box);
    auto build_guide_row=[&](const Guide& source) {
        auto* row=new QGroupBox(qs(source.name),guides_box);row->setObjectName("guide-row-"+qs(source.id));auto* form=new QFormLayout(row);
        auto* name_input=new QLineEdit(qs(source.name),row);name_input->setObjectName("guide-name-"+qs(source.id));
        auto* axis_input=new QComboBox(row);axis_input->setObjectName("guide-axis-"+qs(source.id));axis_input->addItem("Vertical · x",QStringLiteral("x"));axis_input->addItem("Horizontal · y",QStringLiteral("y"));axis_input->setCurrentIndex(axis_input->findData(qs(source.axis)));
        const Ref position_ref{source.id,"","guide.position"};
        auto* position_input=make_number(row,("guide-position-"+source.id).c_str(),"Guide position",QString::number(guide_positions.at(source.id),'g',15));
        const bool position_driven=source.position_driver.has_value()||source.position_expression.has_value();
        position_input->setReadOnly(position_driven);
        auto* position_status=new QLabel(row);position_status->setObjectName("guide-position-status-"+qs(source.id));
        auto* unlink_position=new QPushButton("Unlink position",row);unlink_position->setObjectName("guide-unlink-position-"+qs(source.id));
        auto* expression=new ExpressionInput;expression->setObjectName("guide-position-expression-"+qs(source.id));
        expression->setAccessibleName(qs(source.name)+" position expression");expression->setFixedHeight(58);
        expression->setPlaceholderText("Numeric expression in du; Ctrl+Enter to apply");
        if(source.position_expression)expression->setPlainText(qs(source.position_expression->source));
        auto* replace_source=new QCheckBox("Replace current position source",row);
        replace_source->setObjectName("guide-position-replace-"+qs(source.id));
        replace_source->setVisible(position_driven);
        auto* apply_expression=new QPushButton("Apply position expression",row);
        apply_expression->setObjectName("guide-position-apply-expression-"+qs(source.id));
        if(position_driven) {
            QString source_description;
            if(source.position_driver) {
            auto source_name=source.position_driver->object;
            for(const auto& candidate:comp.guides)if(candidate.id==source.position_driver->object){source_name=candidate.name;break;}
                source_description="Linked to "+qs(source_name);
            } else source_description="Expression · "+qs(source.position_expression->source);
            position_status->setText(source_description+" · authored "+QString::number(source.position,'g',15)+" du · evaluated "+
                QString::number(guide_positions.at(source.id),'g',15)+" du");
            position_status->setVisible(true);unlink_position->setVisible(true);
        } else {position_status->setVisible(false);unlink_position->setVisible(false);}
        form->addRow("Name",name_input);form->addRow("Axis",axis_input);form->addRow("Position · du",position_input);
        form->addRow("Position source",position_status);form->addRow("Position expression · du",expression);
        form->addRow("",replace_source);form->addRow("",apply_expression);form->addRow(unlink_position);
        auto* actions=new QWidget(row);auto* buttons=new QHBoxLayout(actions);buttons->setContentsMargins(0,0,0,0);
        auto* apply=new QPushButton("Apply Guide",actions);apply->setObjectName("guide-apply-"+qs(source.id));buttons->addWidget(apply);
        auto* remove=new QPushButton("Delete Guide",actions);remove->setObjectName("guide-delete-"+qs(source.id));buttons->addWidget(remove);form->addRow(actions);
        const LayoutBuilder builder=[composition,source,name_input,axis_input,position_input,parse_number] {
            auto changed=source;changed.name=name_input->text().toStdString();changed.axis=axis_input->currentData().toString().toStdString();
            changed.position=(source.position_driver||source.position_expression)?source.position:parse_number(position_input);
            return std::vector<Command>{UpdateGuide{composition,std::move(changed)}};
        };
        bind_number(name_input,row,builder);bind_number(position_input,row,builder);
        connect(axis_input,qOverload<int>(&QComboBox::currentIndexChanged),this,[preview_from,row,builder](int){preview_from(row,builder);});
        connect(apply,&QPushButton::clicked,this,[commit_from,row,builder]{commit_from(row,builder);});
        connect(unlink_position,&QPushButton::clicked,this,[commit_explicit,row,position_ref]{
            commit_explicit(row,[position_ref]{return std::vector<Command>{UnlinkGuidePosition{position_ref}};});
        });
        const auto expression_command=[position_ref,expression,replace_source] {
            return std::vector<Command>{SetGuidePositionExpression{position_ref,
                {expression->toPlainText().toStdString(),1},replace_source->isChecked()}};
        };
        expression->apply=[commit_explicit,row,expression_command]{commit_explicit(row,expression_command);};
        expression->cancel=[this]{rebuild_inspector();};
        connect(apply_expression,&QPushButton::clicked,this,[commit_explicit,row,expression_command]{commit_explicit(row,expression_command);});
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
        inherit->setEnabled(board.parent_size.has_value()&&!(width?board.width_driver:board.height_driver).has_value());
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
    auto* parent_note=new QLabel("Parent size links remain separate from Template inheritance. A Template can share a source frame, layout and optional Definition content.");
    parent_note->setWordWrap(true);parent_form->addRow(parent_note);
    const ArtboardTemplateContext template_context{host.session_id,composition,id,frozen_revision};
    auto* template_box=new QGroupBox("Artboard Template");template_box->setObjectName("artboard-template-panel");
    auto* template_form=new QFormLayout(template_box);template_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto* template_status=new QLabel(template_box);template_status->setObjectName("artboard-template-state");
    if(board.template_assignment) {
        const auto found=std::find_if(comp.templates.begin(),comp.templates.end(),[&](const ArtboardTemplate& item) {
            return item.id==board.template_assignment->template_id;
        });
        template_status->setText(found==comp.templates.end()?"Assigned Template is missing":
            "Assigned: "+qs(found->name)+" · source Artboard "+qs(find_artboard(comp,found->source_artboard).name));
    } else template_status->setText("No Template assigned to this Artboard");
    template_status->setWordWrap(true);template_form->addRow(template_status);
    auto* template_selector=new QComboBox(template_box);template_selector->setObjectName("artboard-template-selector");
    template_selector->addItem("Choose a Template…",QString{});
    for(const auto& item:comp.templates)template_selector->addItem(
        template_choice_label(host.session.document(),comp,item),qs(item.id));
    if(board.template_assignment)template_selector->setCurrentIndex(template_selector->findData(qs(board.template_assignment->template_id)));
    template_form->addRow("Template",template_selector);
    auto* template_actions=new QGridLayout;template_form->addRow(template_actions);
    const auto template_button=[&](const QString& label,const char* object_name,int row,int column,
                                   const std::function<void(const ArtboardTemplateContext&)>& action,bool enabled=true) {
        auto* button=new QPushButton(label,template_box);button->setObjectName(QString::fromLatin1(object_name));
        button->setEnabled(enabled);template_actions->addWidget(button,row,column);
        connect(button,&QPushButton::clicked,this,[this,action,template_context]{perform([&]{action(template_context);});});
    };
    template_button("Create from source…","artboard-template-create",0,0,
        [this](const auto& context){create_artboard_template(context);});
    template_button("Rename…","artboard-template-rename",0,1,
        [this](const auto& context){rename_artboard_template(context);},!comp.templates.empty());
    template_button("Delete…","artboard-template-delete",0,2,
        [this](const auto& context){delete_artboard_template(context);},!comp.templates.empty());
    template_button("Assign selected","artboard-template-assign",1,0,
        [this,template_selector](const auto& context) {
            const auto selected=template_selector->currentData().toString().toStdString();
            assign_artboard_template(context,selected.empty()?std::nullopt:std::optional<Id>{selected});
        },!comp.templates.empty());
    template_button("Set frame / layout…","artboard-template-set-override",1,1,
        [this](const auto& context){set_artboard_template_override(context);},board.template_assignment.has_value());
    template_button("Reset override…","artboard-template-reset-override",1,2,
        [this](const auto& context){reset_artboard_template_override(context);},board.template_assignment.has_value());
    template_button("Detach Template","artboard-template-detach",2,0,
        [this](const auto& context){detach_artboard_template(context);},board.template_assignment.has_value());
    auto* template_note=new QLabel("Choose a source Artboard or Definition, assign a Template to this frame, and reset individual fields to restore inheritance.",template_box);
    template_note->setWordWrap(true);template_form->addRow(template_note);
    auto* guide_box=new QGroupBox("Artboard Guides",template_box);guide_box->setObjectName("artboard-guide-panel");
    auto* guide_form=new QFormLayout(guide_box);
    auto* guide_selector=new QComboBox(guide_box);guide_selector->setObjectName("artboard-guide-selector");
    const auto occurrences=effective_artboard_guides(host.session.document(),composition,id);
    for(const auto& guide:occurrences) {
        const auto label=(guide.inherited?QStringLiteral("Inherited"):QStringLiteral("Local"))+QStringLiteral(" · ")+
            qs(guide.name)+QStringLiteral(" · ")+qs(guide.axis)+QStringLiteral(" · ")+QString::number(guide.position)+
            QStringLiteral(" · ")+qs(guide.guide_id);
        guide_selector->addItem(label,qs(guide.guide_id));
    }
    guide_form->addRow("Guide occurrence",guide_selector);
    auto* guide_actions=new QGridLayout;guide_form->addRow(guide_actions);
    auto* guide_add=new QPushButton("Add local…",guide_box);guide_add->setObjectName("artboard-guide-add");
    auto* guide_edit_button=new QPushButton("Edit local…",guide_box);guide_edit_button->setObjectName("artboard-guide-edit");
    auto* guide_delete=new QPushButton("Delete local",guide_box);guide_delete->setObjectName("artboard-guide-delete");
    auto* guide_override=new QPushButton("Override field…",guide_box);guide_override->setObjectName("artboard-guide-override");
    auto* guide_reset=new QPushButton("Reset field…",guide_box);guide_reset->setObjectName("artboard-guide-reset");
    auto* guide_detach=new QPushButton("Detach occurrence",guide_box);guide_detach->setObjectName("artboard-guide-detach");
    auto* guide_drag=new QPushButton("Drag once…",guide_box);guide_drag->setObjectName("artboard-guide-drag");
    guide_drag->setToolTip("Arm only the selected visible Guide occurrence, then drag its clipped line once on Canvas. Escape cancels.");
    guide_actions->addWidget(guide_drag,2,0,1,3);
    guide_actions->addWidget(guide_add,0,0);guide_actions->addWidget(guide_edit_button,0,1);guide_actions->addWidget(guide_delete,0,2);
    guide_actions->addWidget(guide_override,1,0);guide_actions->addWidget(guide_reset,1,1);guide_actions->addWidget(guide_detach,1,2);
    const auto update_guide_buttons=[guide_selector,guide_edit_button,guide_delete,guide_override,guide_reset,guide_detach,guide_drag,
        occurrences,assignment=board.template_assignment](int) {
        const auto selected=guide_selector->currentData().toString().toStdString();
        const auto found=std::find_if(occurrences.begin(),occurrences.end(),[&](const auto& value) {
            return value.guide_id==selected;
        });
        const bool present=found!=occurrences.end();
        const bool local=present&&!found->inherited;
        const bool inherited=present&&found->inherited;
        guide_edit_button->setEnabled(local);guide_delete->setEnabled(local);
        guide_drag->setEnabled(present&&found->enabled);
        guide_drag->setText(inherited?"Drag position override once…":"Drag local position once…");
        guide_override->setEnabled(inherited);guide_detach->setEnabled(inherited);
        const bool has_reset=inherited&&assignment&&
            (assignment->guide_position_overrides.contains(selected)||assignment->guide_enabled_overrides.contains(selected));
        guide_reset->setEnabled(has_reset);
    };
    update_guide_buttons(guide_selector->currentIndex());
    connect(guide_selector,qOverload<int>(&QComboBox::currentIndexChanged),this,update_guide_buttons);
    connect(guide_drag,&QPushButton::clicked,this,[this,template_context,guide_selector]{
        const auto guide=guide_selector->currentData().toString().toStdString();
        perform([&]{
            verify_artboard_guide_context(template_context);
            if(layout_preview_active_||layout_preview_invalid_)cancel_layout_draft();
            canvas->arm_artboard_guide_drag(template_context.composition,template_context.artboard,guide);
            canvas->setFocus(Qt::OtherFocusReason);
        });
    });
    connect(guide_add,&QPushButton::clicked,this,[this,template_context]{perform([&]{add_artboard_guide(template_context);});});
    connect(guide_edit_button,&QPushButton::clicked,this,[this,template_context,guide_selector]{
        const auto guide=guide_selector->currentData().toString().toStdString();
        perform([&]{edit_artboard_guide(template_context,guide);});
    });
    connect(guide_delete,&QPushButton::clicked,this,[this,template_context,guide_selector]{
        const auto guide=guide_selector->currentData().toString().toStdString();
        perform([&]{delete_artboard_guide(template_context,guide);});
    });
    connect(guide_override,&QPushButton::clicked,this,[this,template_context,guide_selector]{
        const auto guide=guide_selector->currentData().toString().toStdString();
        perform([&]{set_artboard_guide_override(template_context,guide);});
    });
    connect(guide_reset,&QPushButton::clicked,this,[this,template_context,guide_selector]{
        const auto guide=guide_selector->currentData().toString().toStdString();
        perform([&]{reset_artboard_guide_override(template_context,guide);});
    });
    connect(guide_detach,&QPushButton::clicked,this,[this,template_context,guide_selector]{
        const auto guide=guide_selector->currentData().toString().toStdString();
        perform([&]{detach_artboard_guide(template_context,guide);});
    });
    auto* guide_note=new QLabel("Inherited Guide position and enabled state override independently. Detached occurrences become ordinary local Guides.",guide_box);
    guide_note->setWordWrap(true);guide_form->addRow(guide_note);template_form->addRow(guide_box);layout->addWidget(template_box);
    auto* fit=new QPushButton("Fit active frame");fit->setObjectName("artboard-fit");layout->addWidget(fit);
    connect(fit,&QPushButton::clicked,canvas,&Canvas::fit_artboard);layout->addStretch();
}

void Window::verify_artboard_template_context(const ArtboardTemplateContext& context) const {
    if(host.session_id!=context.session)
        throw Error("SESSION_CONFLICT","Template command belongs to another document session");
    if(host.session.revision()!=context.revision)
        throw Error("REVISION_CONFLICT","Template command captured revision "+std::to_string(context.revision)+
            " but the current revision is "+std::to_string(host.session.revision())+
            "; discard it and refresh the captured Artboard");
    const auto& composition=find_composition(host.session.document(),context.composition);
    (void)find_artboard(composition,context.artboard);
}

void Window::apply_artboard_template_command(const ArtboardTemplateContext& context,ArtboardTemplateCommand command) {
    verify_artboard_template_context(context);
    canvas->cancel_interaction();
    host.session.apply({Command{std::move(command)}},context.revision);
    host.edited();
}

void Window::verify_artboard_guide_context(const ArtboardTemplateContext& context) const {
    if(host.session_id!=context.session)
        throw Error("SESSION_CONFLICT","Artboard Guide command belongs to another document session");
    if(host.session.revision()!=context.revision)
        throw Error("REVISION_CONFLICT","Artboard Guide command belongs to a stale captured revision");
    const auto& composition=find_composition(host.session.document(),context.composition);
    (void)find_artboard(composition,context.artboard);
}

void Window::apply_artboard_guide_command(const ArtboardTemplateContext& context,ArtboardGuideCommand command) {
    verify_artboard_guide_context(context);
    canvas->cancel_interaction();
    host.session.apply({Command{StructuralCommand{std::move(command)}}},context.revision);
    host.edited();
}

void Window::add_artboard_guide(const ArtboardTemplateContext& context) {
    verify_artboard_guide_context(context);
    QDialog dialog(this);dialog.setObjectName("add-artboard-guide-dialog");dialog.setWindowTitle("Add local Artboard Guide");
    auto* form=new QFormLayout(&dialog);auto* name=new QLineEdit("Guide",&dialog);name->setObjectName("artboard-guide-name");
    auto* axis=new QComboBox(&dialog);axis->setObjectName("artboard-guide-axis");
    axis->addItem("Vertical · x",QStringLiteral("x"));axis->addItem("Horizontal · y",QStringLiteral("y"));
    auto* position=new QDoubleSpinBox(&dialog);position->setObjectName("artboard-guide-position");
    position->setDecimals(3);position->setRange(-1000000000,1000000000);
    auto* enabled=new QCheckBox("Enabled",&dialog);enabled->setObjectName("artboard-guide-enabled");enabled->setChecked(true);
    form->addRow("Name",name);form->addRow("Axis",axis);form->addRow("Position · local du",position);form->addRow(enabled);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Add Guide");form->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto label=name->text().trimmed();if(label.isEmpty())throw Error("INVALID_ARTBOARD_GUIDE","Enter a Guide name");
    apply_artboard_guide_command(context,ArtboardGuideCommand{AddArtboardGuide{context.composition,context.artboard,
        ArtboardGuide{new_id(),label.toStdString(),axis->currentData().toString().toStdString(),position->value(),enabled->isChecked()}}});
    statusBar()->showMessage("Local Artboard Guide added to the captured frame",6000);
}

void Window::edit_artboard_guide(const ArtboardTemplateContext& context,Id guide_id) {
    verify_artboard_guide_context(context);
    const auto& composition=find_composition(host.session.document(),context.composition);
    const auto& board=find_artboard(composition,context.artboard);
    const auto found=std::find_if(board.local_guides.begin(),board.local_guides.end(),[&](const ArtboardGuide& guide) {
        return guide.id==guide_id;
    });
    if(found==board.local_guides.end())throw Error("INHERITED_ARTBOARD_GUIDE_READ_ONLY","Detach an inherited Guide before editing it");
    const auto original=*found;
    QDialog dialog(this);dialog.setObjectName("edit-artboard-guide-dialog");dialog.setWindowTitle("Edit local Artboard Guide");
    auto* form=new QFormLayout(&dialog);auto* name=new QLineEdit(qs(original.name),&dialog);name->setObjectName("artboard-guide-name");
    auto* axis=new QComboBox(&dialog);axis->setObjectName("artboard-guide-axis");
    axis->addItem("Vertical · x",QStringLiteral("x"));axis->addItem("Horizontal · y",QStringLiteral("y"));
    axis->setCurrentIndex(axis->findData(qs(original.axis)));
    auto* position=new QDoubleSpinBox(&dialog);position->setObjectName("artboard-guide-position");
    position->setDecimals(3);position->setRange(-1000000000,1000000000);position->setValue(original.position);
    bool position_changed=false;
    connect(position,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&position_changed](double){position_changed=true;});
    auto* enabled=new QCheckBox("Enabled",&dialog);enabled->setObjectName("artboard-guide-enabled");enabled->setChecked(original.enabled);
    form->addRow("Name",name);form->addRow("Axis",axis);form->addRow("Position · local du",position);form->addRow(enabled);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Apply Guide edit");form->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto label=name->text().trimmed();if(label.isEmpty())throw Error("INVALID_ARTBOARD_GUIDE","Enter a Guide name");
    apply_artboard_guide_command(context,ArtboardGuideCommand{UpdateArtboardGuide{context.composition,context.artboard,
        ArtboardGuide{guide_id,label.toStdString(),axis->currentData().toString().toStdString(),
            position_changed?position->value():original.position,enabled->isChecked()}}});
    statusBar()->showMessage("Local Artboard Guide edited",6000);
}

void Window::delete_artboard_guide(const ArtboardTemplateContext& context,Id guide_id) {
    apply_artboard_guide_command(context,ArtboardGuideCommand{DeleteArtboardGuide{context.composition,context.artboard,guide_id}});
    statusBar()->showMessage("Local Artboard Guide deleted",6000);
}

void Window::set_artboard_guide_override(const ArtboardTemplateContext& context,Id guide_id) {
    verify_artboard_guide_context(context);
    const auto occurrences=effective_artboard_guides(host.session.document(),context.composition,context.artboard);
    const auto occurrence=std::find_if(occurrences.begin(),occurrences.end(),[&](const auto& item) {
        return item.guide_id==guide_id&&item.inherited;
    });
    if(occurrence==occurrences.end())throw Error("MISSING_INHERITED_ARTBOARD_GUIDE",guide_id);
    QDialog dialog(this);dialog.setObjectName("set-artboard-guide-override-dialog");dialog.setWindowTitle("Override inherited Artboard Guide field");
    auto* form=new QFormLayout(&dialog);auto* field=new QComboBox(&dialog);field->setObjectName("artboard-guide-override-field");
    field->addItem("Position",QStringLiteral("position"));field->addItem("Enabled",QStringLiteral("enabled"));
    auto* position=new QDoubleSpinBox(&dialog);position->setObjectName("artboard-guide-override-position");
    const auto original_position=occurrence->position;
    position->setDecimals(3);position->setRange(-1000000000,1000000000);position->setValue(original_position);
    bool position_changed=false;
    connect(position,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&position_changed](double){position_changed=true;});
    auto* enabled=new QCheckBox("Enabled",&dialog);enabled->setObjectName("artboard-guide-override-enabled");enabled->setChecked(occurrence->enabled);
    form->addRow("Field",field);form->addRow("Position · local du",position);form->addRow(enabled);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Set selected field");form->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto selected=field->currentData().toString();
    std::variant<double,bool> value=selected=="position"?
        std::variant<double,bool>{position_changed?position->value():original_position}:
        std::variant<double,bool>{enabled->isChecked()};
    apply_artboard_guide_command(context,ArtboardGuideCommand{SetArtboardGuideOverride{
        context.composition,context.artboard,guide_id,selected.toStdString(),std::move(value)}});
    statusBar()->showMessage("Inherited Artboard Guide field override applied",6000);
}

void Window::reset_artboard_guide_override(const ArtboardTemplateContext& context,Id guide_id) {
    verify_artboard_guide_context(context);
    const auto& board=find_artboard(find_composition(host.session.document(),context.composition),context.artboard);
    if(!board.template_assignment)throw Error("MISSING_ARTBOARD_TEMPLATE_ASSIGNMENT",context.artboard);
    const auto& assignment=*board.template_assignment;std::vector<std::pair<QString,std::string>> fields;
    if(assignment.guide_position_overrides.contains(guide_id))fields.emplace_back("Position","position");
    if(assignment.guide_enabled_overrides.contains(guide_id))fields.emplace_back("Enabled","enabled");
    if(fields.empty())throw Error("MISSING_ARTBOARD_GUIDE_OVERRIDE",guide_id);
    QDialog dialog(this);dialog.setObjectName("reset-artboard-guide-override-dialog");dialog.setWindowTitle("Reset one Artboard Guide field");
    auto* form=new QFormLayout(&dialog);auto* field=new QComboBox(&dialog);field->setObjectName("artboard-guide-reset-field");
    for(const auto& [label,value]:fields)field->addItem(label,QString::fromStdString(value));form->addRow("Reset",field);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Reset selected field");form->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    apply_artboard_guide_command(context,ArtboardGuideCommand{ResetArtboardGuideOverride{
        context.composition,context.artboard,guide_id,field->currentData().toString().toStdString()}});
    statusBar()->showMessage("Selected Artboard Guide field now follows its source",6000);
}

void Window::detach_artboard_guide(const ArtboardTemplateContext& context,Id guide_id) {
    apply_artboard_guide_command(context,ArtboardGuideCommand{DetachArtboardGuide{
        context.composition,context.artboard,guide_id,new_id()}});
    statusBar()->showMessage("Inherited Artboard Guide detached as an independent local Guide",6000);
}

void Window::create_artboard_template(const ArtboardTemplateContext& context) {
    verify_artboard_template_context(context);
    const auto document=host.session.document();
    const auto& composition=find_composition(document,context.composition);
    const auto& target=find_artboard(composition,context.artboard);
    const auto template_id=new_id();
    QDialog dialog(this);dialog.setObjectName("create-artboard-template-dialog");
    dialog.setWindowTitle("Create Artboard Template");
    auto* layout=new QFormLayout(&dialog);
    auto* name=new QLineEdit(qs(target.name)+" Template",&dialog);name->setObjectName("template-create-name");
    auto* source=new QComboBox(&dialog);source->setObjectName("template-source-artboard");
    for(const auto& item:composition.artboards)
        source->addItem(artboard_choice_label(composition,item),qs(item.id));
    source->setCurrentIndex(source->findData(qs(context.artboard)));
    auto* definition=new QComboBox(&dialog);definition->setObjectName("template-definition-selector");
    definition->addItem("No Definition",QString{});
    std::set<Id> composition_items;
    std::function<void(const Id&)> collect_items=[&](const Id& object_id) {
        if(!composition_items.insert(object_id).second)return;
        for(const auto& child:document.objects.at(object_id).children)collect_items(child);
    };
    for(const auto& root:composition.roots)collect_items(root);
    for(const auto& [id,item]:document.definitions)if(composition_items.contains(item.root))
        definition->addItem(definition_choice_label(document,id),qs(id));
    layout->addRow("Template name",name);layout->addRow("Source Artboard",source);layout->addRow("Definition",definition);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Create Template");layout->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto label=name->text().trimmed();
    if(label.isEmpty())throw Error("INVALID_ARTBOARD_TEMPLATE","Enter a Template name");
    const auto source_id=source->currentData().toString().toStdString();
    const auto definition_id=definition->currentData().toString().toStdString();
    apply_artboard_template_command(context,ArtboardTemplateCommand{CreateArtboardTemplate{context.composition,
        ArtboardTemplate{template_id,label.toStdString(),source_id,
            definition_id.empty()?std::nullopt:std::optional<Id>{definition_id}}}});
    statusBar()->showMessage("Artboard Template created from the selected stable source Artboard",6000);
}

void Window::rename_artboard_template(const ArtboardTemplateContext& context) {
    verify_artboard_template_context(context);
    const auto document=host.session.document();
    const auto& composition=find_composition(document,context.composition);
    if(composition.templates.empty())throw Error("MISSING_ARTBOARD_TEMPLATE","Create a Template first");
    const auto templates=composition.templates;
    QDialog dialog(this);dialog.setObjectName("rename-artboard-template-dialog");dialog.setWindowTitle("Rename Artboard Template");
    auto* layout=new QFormLayout(&dialog);auto* selector=new QComboBox(&dialog);
    selector->setObjectName("template-rename-selector");
    for(const auto& item:templates)selector->addItem(template_choice_label(document,composition,item),qs(item.id));
    auto* name=new QLineEdit(qs(templates.front().name),&dialog);name->setObjectName("template-rename-name");
    connect(selector,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[templates,name,selector](int index) {
        if(index<0)return;
        const auto id=selector->currentData().toString().toStdString();
        const auto found=std::find_if(templates.begin(),templates.end(),[&](const ArtboardTemplate& item){return item.id==id;});
        if(found!=templates.end())name->setText(qs(found->name));
    });
    layout->addRow("Template",selector);layout->addRow("New name",name);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Rename Template");layout->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto label=name->text().trimmed();if(label.isEmpty())throw Error("INVALID_ARTBOARD_TEMPLATE","Enter a Template name");
    apply_artboard_template_command(context,ArtboardTemplateCommand{RenameArtboardTemplate{context.composition,
        selector->currentData().toString().toStdString(),label.toStdString()}});
    statusBar()->showMessage("Template renamed; its stable ID and Artboard assignments are unchanged",6000);
}

void Window::delete_artboard_template(const ArtboardTemplateContext& context) {
    verify_artboard_template_context(context);
    const auto document=host.session.document();
    const auto& composition=find_composition(document,context.composition);
    if(composition.templates.empty())throw Error("MISSING_ARTBOARD_TEMPLATE","There are no Templates to delete");
    QDialog dialog(this);dialog.setObjectName("delete-artboard-template-dialog");dialog.setWindowTitle("Delete Artboard Template");
    auto* layout=new QVBoxLayout(&dialog);auto* selector=new QComboBox(&dialog);selector->setObjectName("template-delete-selector");
    for(const auto& item:composition.templates)selector->addItem(template_choice_label(document,composition,item),qs(item.id));
    layout->addWidget(selector);auto* note=new QLabel("An assigned Template must be detached from every target before deletion.",&dialog);
    note->setWordWrap(true);layout->addWidget(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Delete Template");layout->addWidget(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    apply_artboard_template_command(context,ArtboardTemplateCommand{DeleteArtboardTemplate{context.composition,
        selector->currentData().toString().toStdString()}});
    statusBar()->showMessage("Artboard Template deleted; Undo restores the shared source",6000);
}

void Window::assign_artboard_template(const ArtboardTemplateContext& context,std::optional<Id> preferred_template) {
    verify_artboard_template_context(context);
    const auto document=host.session.document();
    const auto& composition=find_composition(document,context.composition);
    if(composition.templates.empty())throw Error("MISSING_ARTBOARD_TEMPLATE","Create a Template first");
    const auto content_instance_id=new_id();
    QDialog dialog(this);dialog.setObjectName("assign-artboard-template-dialog");dialog.setWindowTitle("Assign Artboard Template");
    auto* layout=new QFormLayout(&dialog);auto* selector=new QComboBox(&dialog);selector->setObjectName("template-assign-selector");
    for(const auto& item:composition.templates)selector->addItem(template_choice_label(document,composition,item),qs(item.id));
    if(preferred_template) {
        const auto preferred_index=selector->findData(qs(*preferred_template));
        if(preferred_index>=0)selector->setCurrentIndex(preferred_index);
    }
    auto* content=new QCheckBox("Create a fresh Definition Instance at the captured Artboard origin",&dialog);
    content->setObjectName("template-create-content-instance");
    const auto update_content=[&composition,selector,content] {
        const auto selected_id=selector->currentData().toString().toStdString();
        const auto selected_template=std::find_if(composition.templates.begin(),composition.templates.end(),[&](const ArtboardTemplate& item){return item.id==selected_id;});
        const bool supports_content=selected_template!=composition.templates.end()&&selected_template->definition.has_value();
        content->setEnabled(supports_content);content->setChecked(supports_content);
    };
    update_content();connect(selector,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[update_content](int){update_content();});
    layout->addRow("Template",selector);layout->addRow("Content",content);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Assign to captured Artboard");layout->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto template_id=selector->currentData().toString().toStdString();
    const auto content_id=content->isChecked()?std::optional<Id>{content_instance_id}:std::nullopt;
    apply_artboard_template_command(context,ArtboardTemplateCommand{AssignArtboardTemplate{
        context.composition,context.artboard,template_id,content_id}});
    statusBar()->showMessage("Template assigned to the Artboard captured when the selector opened",6000);
}

void Window::set_artboard_template_override(const ArtboardTemplateContext& context) {
    verify_artboard_template_context(context);
    const auto& composition=find_composition(host.session.document(),context.composition);
    const auto& board=find_artboard(composition,context.artboard);
    if(!board.template_assignment)throw Error("MISSING_ARTBOARD_TEMPLATE_ASSIGNMENT","Assign a Template first");
    const auto evaluated=evaluate_artboard(composition,context.artboard);
    const Margin margin=evaluated.layout&&evaluated.layout->margin?*evaluated.layout->margin:Margin{};
    Grid grid;
    if(evaluated.layout&&evaluated.layout->grid) {
        const auto& value=*evaluated.layout->grid;
        grid=Grid{board.template_assignment->grid_id,value.bounds,value.columns,value.rows,
            value.column_gutter,value.row_gutter};
    }
    else grid=Grid{board.template_assignment->grid_id,{20,20,std::max(1.0,evaluated.width-40),
        std::max(1.0,evaluated.height-40)},2,2,10,10};
    grid.id=board.template_assignment->grid_id;
    QDialog dialog(this);dialog.setObjectName("set-artboard-template-override-dialog");
    dialog.setWindowTitle("Set Template frame or layout override");
    auto* layout=new QFormLayout(&dialog);auto* field=new QComboBox(&dialog);field->setObjectName("template-override-field");
    field->addItem("Frame width",QStringLiteral("frame.width"));field->addItem("Frame height",QStringLiteral("frame.height"));
    field->addItem("Margin family",QStringLiteral("layout.margin"));
    field->addItem("No Margin · local override",QStringLiteral("layout.margin.absent"));
    field->addItem("Grid family",QStringLiteral("layout.grid"));
    field->addItem("No Grid · local override",QStringLiteral("layout.grid.absent"));
    auto make_value=[&dialog](const char* name,double value,double minimum,double maximum,double step) {
        auto* spin=new QDoubleSpinBox(&dialog);spin->setObjectName(QString::fromLatin1(name));spin->setDecimals(3);
        spin->setRange(minimum,maximum);spin->setSingleStep(step);spin->setValue(value);return spin;
    };
    auto* frame_width=make_value("template-frame-width",evaluated.width,0.001,10000000,10);
    auto* frame_height=make_value("template-frame-height",evaluated.height,0.001,10000000,10);
    auto* margin_left=make_value("template-margin-left",margin.left,0,10000000,1);
    auto* margin_top=make_value("template-margin-top",margin.top,0,10000000,1);
    auto* margin_right=make_value("template-margin-right",margin.right,0,10000000,1);
    auto* margin_bottom=make_value("template-margin-bottom",margin.bottom,0,10000000,1);
    auto* grid_x=make_value("template-grid-x",grid.bounds.x,0,10000000,1);
    auto* grid_y=make_value("template-grid-y",grid.bounds.y,0,10000000,1);
    auto* grid_width=make_value("template-grid-width",grid.bounds.width,0.001,10000000,10);
    auto* grid_height=make_value("template-grid-height",grid.bounds.height,0.001,10000000,10);
    auto* grid_columns=new QSpinBox(&dialog);grid_columns->setObjectName("template-grid-columns");grid_columns->setRange(1,1000);grid_columns->setValue(static_cast<int>(grid.columns));
    auto* grid_rows=new QSpinBox(&dialog);grid_rows->setObjectName("template-grid-rows");grid_rows->setRange(1,1000);grid_rows->setValue(static_cast<int>(grid.rows));
    auto* grid_column_gutter=make_value("template-grid-column-gutter",grid.column_gutter,0,10000000,1);
    auto* grid_row_gutter=make_value("template-grid-row-gutter",grid.row_gutter,0,10000000,1);
    layout->addRow("Override",field);layout->addRow("Frame width · du",frame_width);layout->addRow("Frame height · du",frame_height);
    layout->addRow("Margin left · du",margin_left);layout->addRow("Margin top · du",margin_top);
    layout->addRow("Margin right · du",margin_right);layout->addRow("Margin bottom · du",margin_bottom);
    layout->addRow("Grid X · du",grid_x);layout->addRow("Grid Y · du",grid_y);
    layout->addRow("Grid width · du",grid_width);layout->addRow("Grid height · du",grid_height);
    layout->addRow("Grid columns",grid_columns);layout->addRow("Grid rows",grid_rows);
    layout->addRow("Column gutter · du",grid_column_gutter);layout->addRow("Row gutter · du",grid_row_gutter);
    auto* note=new QLabel("Frame and layout families remain independent. Choose a local absence to suppress a source family, or reset that field to inherit it again.",&dialog);
    note->setWordWrap(true);layout->addRow(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Set selected override");layout->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto selected_field=field->currentData().toString();
    if(selected_field=="frame.width")apply_artboard_template_command(context,ArtboardTemplateCommand{SetArtboardTemplateOverride{
        context.composition,context.artboard,"frame.width",frame_width->value()}});
    else if(selected_field=="frame.height")apply_artboard_template_command(context,ArtboardTemplateCommand{SetArtboardTemplateOverride{
        context.composition,context.artboard,"frame.height",frame_height->value()}});
    else if(selected_field=="layout.margin")apply_artboard_template_command(context,ArtboardTemplateCommand{SetArtboardTemplateOverride{
        context.composition,context.artboard,"layout.margin",std::optional<Margin>{Margin{
            margin_left->value(),margin_top->value(),margin_right->value(),margin_bottom->value()}}}});
    else if(selected_field=="layout.margin.absent")apply_artboard_template_command(context,ArtboardTemplateCommand{SetArtboardTemplateOverride{
        context.composition,context.artboard,"layout.margin",std::optional<Margin>{}}});
    else if(selected_field=="layout.grid.absent")apply_artboard_template_command(context,ArtboardTemplateCommand{SetArtboardTemplateOverride{
        context.composition,context.artboard,"layout.grid",std::optional<Grid>{}}});
    else {
        grid.bounds={grid_x->value(),grid_y->value(),grid_width->value(),grid_height->value()};
        grid.columns=static_cast<std::size_t>(grid_columns->value());grid.rows=static_cast<std::size_t>(grid_rows->value());
        grid.column_gutter=grid_column_gutter->value();grid.row_gutter=grid_row_gutter->value();
        apply_artboard_template_command(context,ArtboardTemplateCommand{SetArtboardTemplateOverride{context.composition,context.artboard,
            "layout.grid",std::optional<Grid>{grid}}});
    }
    statusBar()->showMessage("Template override applied to the captured Artboard",6000);
}

void Window::reset_artboard_template_override(const ArtboardTemplateContext& context) {
    verify_artboard_template_context(context);
    const auto& composition=find_composition(host.session.document(),context.composition);
    const auto& board=find_artboard(composition,context.artboard);
    if(!board.template_assignment)throw Error("MISSING_ARTBOARD_TEMPLATE_ASSIGNMENT","Assign a Template first");
    std::vector<std::pair<QString,std::string>> fields;
    const auto& assignment=*board.template_assignment;
    if(assignment.width_override||board.width_driver||(board.parent_size&&board.parent_size->width))fields.emplace_back("Frame width", "frame.width");
    if(assignment.height_override||board.height_driver||(board.parent_size&&board.parent_size->height))fields.emplace_back("Frame height", "frame.height");
    if(assignment.margin_overridden||(board.layout&&board.layout->margin))fields.emplace_back("Margin family", "layout.margin");
    if(assignment.grid_overridden||(board.layout&&board.layout->grid))fields.emplace_back("Grid family", "layout.grid");
    if(fields.empty())throw Error("MISSING_OVERRIDE","This Artboard has no local Template overrides to reset");
    QDialog dialog(this);dialog.setObjectName("reset-artboard-template-override-dialog");
    dialog.setWindowTitle("Reset one Template override");auto* layout=new QFormLayout(&dialog);
    auto* selector=new QComboBox(&dialog);selector->setObjectName("template-reset-field");
    for(const auto& [label,field]:fields)selector->addItem(label,QString::fromStdString(field));
    layout->addRow("Use Template for",selector);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Apply)->setText("Reset selected field");layout->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    apply_artboard_template_command(context,ArtboardTemplateCommand{ResetArtboardTemplateOverride{context.composition,context.artboard,
        selector->currentData().toString().toStdString()}});
    statusBar()->showMessage("Selected Artboard field now follows its Template source again",6000);
}

void Window::detach_artboard_template(const ArtboardTemplateContext& context) {
    apply_artboard_template_command(context,ArtboardTemplateCommand{DetachArtboardTemplate{context.composition,context.artboard,
        "template-"+new_id()}});
    statusBar()->showMessage("Template content and frame are independent; Undo restores the live assignment",7000);
}

void Window::rebuild_inspector(bool use_canvas_values) {
    if(rebuilding_inspector_)return;
    QScopedValueRollback guard(rebuilding_inspector_,true);
    cancel_angle_adapters(true);
    std::erase_if(expression_drafts_,[&](const auto& item){return item.second.session!=host.session_id;});
    QString context=host.session_id+(artboard_editing_?"/frame/"+qs(canvas->active_artboard()):QString{});
    for(const auto& item:canvas->selections())context+="/"+qs(item.object)+":"+qs(item.point);
    const auto scroll=context==inspector_context_?inspector_scroll_->verticalScrollBar()->value():0;
    const auto selected_text=host.session.document().objects.find(canvas->selected_object);
    const bool text_form=!artboard_editing_&&canvas->selections().size()<=1&&
        selected_text!=host.session.document().objects.end()&&selected_text->second.text.has_value();
    inspector_context_=context;
    // Qt may scroll to a disappearing focused field while the new form lays out.
    // Restore the previous viewport only for the same editing context.
    const auto restore_scroll=[this,context,scroll,text_form]{QTimer::singleShot(0,this,[this,context,scroll,text_form]{
        if(inspector_context_!=context)return;
        if(auto* current_layout=inspector_->layout()) {
            current_layout->activate();
            inspector_->setMinimumHeight(text_form?current_layout->minimumSize().height():0);
            inspector_->updateGeometry();
        }
        inspector_scroll_->verticalScrollBar()->setValue(scroll);
    });};
    // Avoid deleting a focused field synchronously from its editingFinished signal.
    if(auto* old=inspector_->layout()) {
        while(auto* child=old->takeAt(0)) { if(child->widget()) {child->widget()->hide();child->widget()->deleteLater();}delete child; }
        delete old;
    }
    auto* layout=new QVBoxLayout(inspector_);
    if(artboard_editing_) {edit_artboard(layout);restore_scroll();return;}
    const auto& d=host.session.document();
    if(!d.objects.contains(canvas->selected_object)) {layout->addWidget(new QLabel("Add a shape, Curve or Text.\nSelect a point to edit its handles."));layout->addStretch();restore_scroll();return;}
    const auto& o=d.objects.at(canvas->selected_object);
    // A complete Window refresh has just evaluated this same committed Session
    // for Canvas. Reuse those numbers; standalone selection/draft refreshes and
    // an active preview still read committed values through the core evaluator.
    inspector_values_=use_canvas_values&&!host.session.gesture_active()?canvas->evaluated_values():evaluate(d);
    if(canvas->selections().size()>1){add_multi_properties(layout);restore_scroll();return;}
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
    if(o.kind==Kind::group) {
        auto* follow_box=new QGroupBox("Rigid Path Follow");follow_box->setObjectName("group-path-follow");
        auto* follow_form=new QFormLayout(follow_box);follow_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        const auto composition_id=canvas->active_composition();
        const auto& composition=find_composition(d,composition_id);
        std::set<Id> group_subtree;
        std::function<void(const Id&)> collect_group=[&](const Id& current) {
            if(!group_subtree.insert(current).second)return;
            for(const auto& child:d.objects.at(current).children)collect_group(child);
        };
        collect_group(o.id);
        std::vector<Id> path_ids;
        std::function<void(const Id&)> collect_paths=[&](const Id& current) {
            const auto& source=d.objects.at(current);
            if(source.kind==Kind::path&&!source.source&&!group_subtree.contains(current))path_ids.push_back(current);
            for(const auto& child:source.children)collect_paths(child);
        };
        for(const auto& root:composition.roots)collect_paths(root);
        auto* path_picker=new QComboBox(follow_box);path_picker->setObjectName("group-path-follow-source");
        for(const auto& path_id:path_ids) {
            const auto& source=d.objects.at(path_id);
            path_picker->addItem(qs(source.name)+" · "+qs(path_id),qs(path_id));
        }
        follow_form->addRow("Authored Path",path_picker);
        auto* contour_picker=new QComboBox(follow_box);contour_picker->setObjectName("group-path-follow-contour");
        follow_form->addRow("Contour",contour_picker);
        auto populate_contours=[this,path_picker,contour_picker](const QString& preferred) {
            const QSignalBlocker blocker(contour_picker);contour_picker->clear();
            const auto selected=path_picker->currentData().toString().toStdString();
            const auto& document=host.session.document();
            if(document.objects.contains(selected))for(const auto& contour:document.objects.at(selected).contours)
                contour_picker->addItem(qs(contour.id),qs(contour.id));
            const auto index=contour_picker->findData(preferred);if(index>=0)contour_picker->setCurrentIndex(index);
        };
        auto* start_mode=new QComboBox(follow_box);start_mode->setObjectName("group-path-follow-start-mode");
        start_mode->addItem("Distance",QStringLiteral("distance"));start_mode->addItem("Normalized",QStringLiteral("normalized"));
        follow_form->addRow("Start mode",start_mode);
        auto make_follow_number=[&](const char* name,double value) {
            auto* input=new QDoubleSpinBox(follow_box);input->setObjectName(QString::fromLatin1(name));
            const auto limit=std::max(1e9,std::abs(value));
            input->setRange(-limit,limit);input->setDecimals(4);input->setValue(value);input->setKeyboardTracking(false);return input;
        };
        const auto existing=o.path_follow;
        auto* follow_mode=new QComboBox(follow_box);follow_mode->setObjectName("group-path-follow-mode");
        follow_mode->addItem("Rigid placement","rigid");follow_mode->addItem("Deform geometry","deform");
        follow_mode->setCurrentIndex(follow_mode->findData(qs(existing?existing->mode:"rigid")));
        follow_form->addRow("Mode",follow_mode);
        auto* deform_axis=new QComboBox(follow_box);deform_axis->setObjectName("group-path-deform-axis");
        deform_axis->addItem("X longitudinal / Y normal","x");deform_axis->addItem("Y longitudinal / X normal","y");
        deform_axis->setCurrentIndex(deform_axis->findData(qs(existing?existing->deform_axis:"x")));
        deform_axis->setEnabled(follow_mode->currentData().toString()=="deform");
        connect(follow_mode,qOverload<int>(&QComboBox::currentIndexChanged),deform_axis,[follow_mode,deform_axis](int) {
            deform_axis->setEnabled(follow_mode->currentData().toString()=="deform");
        });
        follow_form->addRow("Deform axis",deform_axis);
        auto* start=make_follow_number("group-path-follow-start",existing?existing->start:0);
        auto* relation_offset=make_follow_number("group-path-follow-normal-offset",existing?existing->normal_offset:0);
        const auto displayed_start=start->value(),displayed_offset=relation_offset->value();
        follow_form->addRow("Start",start);follow_form->addRow("Normal offset",relation_offset);
        auto* reversed=new QCheckBox("Reverse traversal",follow_box);reversed->setObjectName("group-path-follow-reversed");
        reversed->setChecked(existing&&existing->reversed);follow_form->addRow(reversed);
        if(existing) {
            const auto path_index=path_picker->findData(qs(existing->path));if(path_index>=0)path_picker->setCurrentIndex(path_index);
            start_mode->setCurrentIndex(start_mode->findData(qs(existing->start_mode)));
        }
        populate_contours(existing?qs(existing->contour):QString{});
        connect(path_picker,qOverload<int>(&QComboBox::currentIndexChanged),this,[populate_contours](int){populate_contours({});});
        auto* follow_buttons=new QWidget(follow_box);auto* follow_button_layout=new QHBoxLayout(follow_buttons);
        follow_button_layout->setContentsMargins(0,0,0,0);
        auto* apply_follow=new QPushButton(existing?"Update Path Follow":"Attach to Path",follow_buttons);
        apply_follow->setObjectName("group-path-follow-apply");apply_follow->setEnabled(path_picker->count()>0);
        follow_button_layout->addWidget(apply_follow);
        auto* clear_follow=new QPushButton("Clear",follow_buttons);clear_follow->setObjectName("group-path-follow-clear");
        clear_follow->setEnabled(existing.has_value());follow_button_layout->addWidget(clear_follow);
        follow_form->addRow(follow_buttons);
        const auto session_id=host.session_id;
        connect(apply_follow,&QPushButton::clicked,this,[this,path_picker,contour_picker,start_mode,start,relation_offset,reversed,follow_mode,deform_axis,
            id=o.id,session_id,existing,displayed_start,displayed_offset] {perform([&] {
            if(host.session_id!=session_id)throw Error("SESSION_CONFLICT","Group Path Follow belongs to another document");
            GroupPathFollow relation=existing.value_or(GroupPathFollow{});
            if(!existing)relation.id=new_id();
            relation.path=path_picker->currentData().toString().toStdString();
            relation.contour=contour_picker->currentData().toString().toStdString();
            relation.start_mode=start_mode->currentData().toString().toStdString();
            if(!existing||start->value()!=displayed_start)relation.start=start->value();
            if(!existing||relation_offset->value()!=displayed_offset)relation.normal_offset=relation_offset->value();
            relation.reversed=reversed->isChecked();
            relation.mode=follow_mode->currentData().toString().toStdString();
            relation.deform_axis=deform_axis->currentData().toString().toStdString();
            canvas->cancel_interaction();
            if(existing)host.session.apply({GroupPathFollowCommand{UpdateGroupPathFollow{id,relation}}},host.session.revision());
            else host.session.apply({GroupPathFollowCommand{AttachGroupPathFollow{id,relation}}},host.session.revision());
            host.edited();
        });});
        connect(clear_follow,&QPushButton::clicked,this,[this,id=o.id,session_id]{perform([&] {
            if(host.session_id!=session_id)throw Error("SESSION_CONFLICT","Group Path Follow belongs to another document");
            canvas->cancel_interaction();host.session.apply({GroupPathFollowCommand{ClearGroupPathFollow{id}}},host.session.revision());host.edited();
        });});
        if(existing) {
            auto* item_note=new QLabel("Child placement stays authored; the relation adds a derived Group-local frame during evaluation.",follow_box);
            item_note->setWordWrap(true);follow_form->addRow(item_note);
            for(const auto& child_id:o.children) {
                const auto& child=d.objects.at(child_id);
                const auto found=existing->items.find(child_id);const bool active=found!=existing->items.end();
                const GroupPathFollowItem item=active?found->second:GroupPathFollowItem{};
                auto* row=new QWidget(follow_box);auto* row_layout=new QHBoxLayout(row);row_layout->setContentsMargins(0,0,0,0);
                auto* enabled=new QCheckBox(qs(child.name),row);enabled->setObjectName("group-path-follow-item-"+qs(child_id));
                enabled->setToolTip(qs(child_id));enabled->setChecked(active);row_layout->addWidget(enabled);
                auto* distance=make_follow_number(("group-path-follow-distance-"+child_id).c_str(),item.distance);
                distance->setToolTip("Distance along the authored source contour");row_layout->addWidget(distance);
                auto* offset=make_follow_number(("group-path-follow-item-offset-"+child_id).c_str(),item.normal_offset);
                offset->setToolTip("Normal offset from this Path Follow relation");row_layout->addWidget(offset);
                auto* tangent=new QCheckBox("Tangent",row);tangent->setObjectName("group-path-follow-tangent-"+qs(child_id));
                tangent->setChecked(item.follow_tangent);tangent->setEnabled(active&&existing->mode=="rigid");
                tangent->setToolTip("Rigid placement only; Deform always uses the sampled tangent/normal frame");row_layout->addWidget(tangent);
                distance->setEnabled(active);offset->setEnabled(active);follow_form->addRow(row);
                connect(enabled,&QCheckBox::toggled,this,[this,enabled,id=o.id,child_id,session_id,active](bool checked) {
                    bool applied=false;perform([&] {
                        if(host.session_id!=session_id)throw Error("SESSION_CONFLICT","Group Path Follow belongs to another document");
                        if(checked)host.session.apply({GroupPathFollowCommand{SetGroupPathFollowItem{id,child_id,GroupPathFollowItem{}}}},host.session.revision());
                        else if(active)host.session.apply({GroupPathFollowCommand{RemoveGroupPathFollowItem{id,child_id}}},host.session.revision());
                        applied=true;host.edited();
                    });
                    if(!applied){const QSignalBlocker blocker(enabled);enabled->setChecked(active);}
                });
                const auto update_item=[this,id=o.id,child_id,session_id,enabled,distance,offset,tangent](int changed) {
                    if(!enabled->isChecked())return;
                    perform([&] {
                        if(host.session_id!=session_id)throw Error("SESSION_CONFLICT","Group Path Follow belongs to another document");
                        const auto& relation=host.session.document().objects.at(id).path_follow;
                        if(!relation)throw Error("MISSING_GROUP_PATH_FOLLOW","Group Path Follow was cleared");
                        const auto current=relation->items.find(child_id);
                        if(current==relation->items.end())throw Error("MISSING_GROUP_PATH_FOLLOW_ITEM","Group Path Follow child is no longer attached");
                        auto value=current->second;
                        if(changed==0)value.distance=distance->value();
                        else if(changed==1)value.normal_offset=offset->value();
                        else value.follow_tangent=tangent->isChecked();
                        host.session.apply({GroupPathFollowCommand{SetGroupPathFollowItem{id,child_id,value}}},host.session.revision());
                        if(changed<2)QTimer::singleShot(0,this,[this,session_id]{if(host.session_id==session_id)host.edited();});
                        else host.edited();
                    });
                };
                connect(distance,&QDoubleSpinBox::editingFinished,this,[update_item]{update_item(0);});
                connect(offset,&QDoubleSpinBox::editingFinished,this,[update_item]{update_item(1);});
                connect(tangent,&QCheckBox::toggled,this,[update_item,active](bool checked) {
                    if(!active&&!checked)return;update_item(2);
                });
            }
        }
        if(path_ids.empty()) {
            auto* note=new QLabel("Add an authored Path outside this Group in the same Composition.",follow_box);
            note->setWordWrap(true);follow_form->addRow(note);
        }
        layout->addWidget(follow_box);
    }
    if(o.text)add_text_properties(layout,o);
    if(o.image)add_image_properties(layout,o);
    if(o.source) {
        auto* generator=section("1 · "+primitive_label(*o.source)+" source");
        if(o.source->type=="nect.shape.circle") {
            auto* handles=new QPushButton(canvas->circle_source_edit()?"Finish Circle source handles":"Edit Circle source handles");
            handles->setObjectName("circle-source-handles");
            handles->setAccessibleName(canvas->circle_source_edit()?"Finish Circle source handles":"Edit Circle source handles");
            handles->setToolTip("Temporarily show Center and Radius controls on the Canvas. Escape exits; this mode is not saved.");
            generator->addRow(handles);
            connect(handles,&QPushButton::clicked,this,[this]{
                canvas->set_circle_source_edit(!canvas->circle_source_edit());canvas->setFocus();
            });
        }
        for(const auto* parameter:{"center_x","center_y","points","rotation","radius","outer_radius","inner_radius","width","height"})
            if(o.source->parameters.contains(parameter)) {
                const Ref ref{o.id,{},std::string("generator.")+parameter};
                add_property(generator,ref,parameter_label(parameter));
                if(std::string(parameter)=="rotation"&&(o.source->type=="nect.shape.polygon"||o.source->type=="nect.shape.star"))
                    add_primitive_angle(generator,ref,*o.source);
            }
        auto* correction=section("2 · Point Edit");
        std::optional<PointEditEnabledProperty> point_edit_state;
        Ref point_edit_ref;
        if(o.point_edit) {
            point_edit_ref=point_edit_enabled_ref(o.id,o.point_edit->id);
            point_edit_state=point_edit_enabled_state(host.session.document(),point_edit_ref);
        }
        auto* enabled_row=new QWidget;
        auto* enabled_layout=new QHBoxLayout(enabled_row);enabled_layout->setContentsMargins(0,0,0,0);
        auto* enabled=new QCheckBox("Enabled");
        enabled->setObjectName("point-edit-enabled");
        enabled->setAccessibleName("Point Edit enabled");
        enabled->setChecked(o.point_edit && o.point_edit->enabled);
        const bool point_edit_driven=point_edit_state&&(point_edit_state->driver||point_edit_state->expression);
        enabled->setEnabled(o.point_edit.has_value()&&!point_edit_driven);
        enabled_layout->addWidget(enabled);
        auto* enabled_driver=new QToolButton(enabled_row);enabled_driver->setObjectName("point-edit-enabled-driver");
        enabled_driver->setText(point_edit_state&&point_edit_state->expression?"Expression…":
            point_edit_state&&point_edit_state->driver?"Driver…":"Link…");
        enabled_driver->setPopupMode(QToolButton::InstantPopup);
        auto* enabled_driver_menu=new QMenu(enabled_driver);enabled_driver->setMenu(enabled_driver_menu);
        auto* link_enabled=enabled_driver_menu->addAction(point_edit_driven
            ?"Replace enabled source…":"Link enabled source…");
        auto* expression_enabled=enabled_driver_menu->addAction(point_edit_driven
            ?"Replace enabled source with expression…":"Set enabled expression…");
        auto* unlink_enabled=enabled_driver_menu->addAction("Unlink and freeze evaluated value");
        unlink_enabled->setEnabled(point_edit_driven);
        enabled_layout->addWidget(enabled_driver);enabled_layout->addStretch();correction->addRow(enabled_row);
        auto* enabled_status=new QLabel;enabled_status->setObjectName("point-edit-enabled-state");
        enabled_status->setWordWrap(true);
        QString enabled_source="none";
        if(point_edit_state&&point_edit_state->driver) {
            const auto source_object=host.session.document().objects.find(point_edit_state->driver->object);
            const auto source_name=source_object==host.session.document().objects.end()
                ?qs(point_edit_state->driver->object):qs(source_object->second.name);
            const auto field=point_edit_state->driver->field;
            const auto point_edit_id=field.substr(std::string("point_edit.").size(),
                field.size()-std::string("point_edit.").size()-std::string(".enabled").size());
            enabled_source=source_name+" / Point Edit ["+qs(point_edit_id)+"]";
        } else if(point_edit_state&&point_edit_state->expression)
            enabled_source="expression: "+qs(point_edit_state->expression->source);
        enabled_status->setText(point_edit_state
            ?QString("Correction: present · Authored literal: %1 · Evaluated enabled: %2 · Source: %3")
                .arg(point_edit_state->literal?"true":"false",point_edit_state->evaluated?"true":"false",enabled_source)
            :QString("Correction: absent · Authored literal: n/a · Evaluated enabled: n/a · Source: none"));
        correction->addRow(QStringLiteral("Enabled state"),enabled_status);
        std::vector<Ref> point_edit_sources;QStringList point_edit_source_labels;
        if(o.point_edit) {
            const auto& composition=find_composition(host.session.document(),canvas->active_composition());
            std::function<void(const Id&)> append_source=[&](const Id& source_id) {
                const auto& source_object=host.session.document().objects.at(source_id);
                if(source_object.source&&source_object.point_edit) {
                    const auto source_ref=point_edit_enabled_ref(source_id,source_object.point_edit->id);
                    if(source_ref!=point_edit_ref) {
                        point_edit_sources.push_back(source_ref);
                        point_edit_source_labels<<qs(source_object.name)+" — Point Edit ["+
                            qs(source_object.point_edit->id)+"] — "+qs(source_id);
                    }
                }
                for(const auto& child:source_object.children)append_source(child);
            };
            for(const auto& root:composition.roots)append_source(root);
        }
        link_enabled->setEnabled(o.point_edit.has_value()&&!point_edit_sources.empty());
        enabled_driver->setEnabled(o.point_edit.has_value());
        const auto point_edit_session=host.session_id;
        const auto point_edit_revision=host.session.revision();
        connect(link_enabled,&QAction::triggered,this,[this,point_edit_ref,point_edit_session,
            point_edit_revision,point_edit_sources,point_edit_source_labels,
            replace=point_edit_driven]( ) {
            choose_boolean_source(this,"point-edit-enabled-source-dialog",
                replace?"Replace Point Edit enabled link":"Link Point Edit enabled",
                qs(point_edit_ref.object)+" / "+qs(point_edit_ref.field),point_edit_sources,point_edit_source_labels,
                [this,point_edit_ref,point_edit_session,point_edit_revision,replace](const Ref& source) {
                if(host.session_id!=point_edit_session)throw Error("SESSION_CONFLICT","Point Edit belongs to another document");
                if(host.session.revision()!=point_edit_revision)throw Error("REVISION_CONFLICT","Point Edit enabled state changed while the source chooser was open");
                host.session.apply({LinkPointEditEnabled{point_edit_ref,source,replace}},point_edit_revision);
                host.edited();
            });
        });
        connect(expression_enabled,&QAction::triggered,this,[this,point_edit_ref,point_edit_session,
            point_edit_revision,initial=point_edit_state?point_edit_state->expression:std::optional<Expression>{},
            replace_available=point_edit_driven] {
            if(host.session_id!=point_edit_session) {
                statusBar()->showMessage("SESSION_CONFLICT: Point Edit belongs to another document",12000);return;
            }
            QDialog dialog(this);dialog.setObjectName("point-edit-enabled-expression-dialog");
            dialog.setWindowTitle(replace_available?"Replace Point Edit enabled source":"Set Point Edit enabled expression");
            dialog.resize(560,220);auto* draft_layout=new QVBoxLayout(&dialog);
            auto* target_label=new QLabel("Target: "+qs(point_edit_ref.object)+" / "+qs(point_edit_ref.field),&dialog);
            target_label->setWordWrap(true);draft_layout->addWidget(target_label);
            auto* source=new QPlainTextEdit(&dialog);source->setObjectName("point-edit-enabled-expression-source");
            source->setPlaceholderText("true, false, ref(\"object-id\",\"\",\"point_edit.correction-id.enabled\"), or !ref(…)");
            source->setPlainText(initial?qs(initial->source):"true");source->setMinimumHeight(58);draft_layout->addWidget(source);
            auto* replace=new QCheckBox("Replace existing enabled source",&dialog);
            replace->setObjectName("point-edit-enabled-expression-replace");replace->setEnabled(replace_available);
            draft_layout->addWidget(replace);
            auto* status=new QLabel("Version 1 accepts true, false, or an optional negation of a distinct installed Point Edit enabled Ref in this Composition. Apply commits one Session command; Cancel leaves the Session unchanged.",&dialog);
            status->setObjectName("point-edit-enabled-expression-status");status->setWordWrap(true);
            status->setTextFormat(Qt::PlainText);draft_layout->addWidget(status);
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);draft_layout->addWidget(buttons);
            connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
            connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
                [this,&dialog,point_edit_ref,point_edit_session,point_edit_revision,source,replace,status] {
                    try {
                        if(host.session_id!=point_edit_session)throw Error("SESSION_CONFLICT","Point Edit belongs to another document");
                        if(host.session.revision()!=point_edit_revision)
                            throw Error("REVISION_CONFLICT","Point Edit enabled state changed while its expression draft was open");
                        host.session.apply({SetPointEditEnabledExpression{point_edit_ref,
                            {source->toPlainText().toStdString(),1},replace->isChecked()}},point_edit_revision);
                        host.edited();dialog.accept();
                    } catch(const Error& error) {
                        status->setText(QString::fromLatin1(error.code.c_str())+": "+QString::fromUtf8(error.what()));
                    }
                });
            dialog.exec();
        });
        connect(unlink_enabled,&QAction::triggered,this,[this,point_edit_ref,point_edit_session,point_edit_revision]{perform([&]{
            if(host.session_id!=point_edit_session)throw Error("SESSION_CONFLICT","Point Edit belongs to another document");
            if(host.session.revision()!=point_edit_revision)throw Error("REVISION_CONFLICT","Point Edit enabled state changed before unlinking");
            host.session.apply({UnlinkPointEditEnabled{point_edit_ref}},point_edit_revision);host.edited();
        });});
        std::size_t field_count=0;
        if(o.point_edit)for(const auto& [point,fields]:o.point_edit->overrides) { (void)point;field_count+=fields.size(); }
        auto* summary=new QLabel(o.point_edit
            ? QString("%1 absolute local overrides across %2 points.")
                .arg(static_cast<qulonglong>(field_count)).arg(static_cast<qulonglong>(o.point_edit->overrides.size()))
            : QString("No overrides yet. Edit a point or handle to add a correction."));
        summary->setObjectName("point-edit-summary");
        summary->setWordWrap(true);correction->addRow(summary);
        const auto semantics_text=point_edit_driven
            ?"The driven value selects saved overrides or generator fallback. Unlink freezes the evaluated bypass value."
            :point_edit_state&&!point_edit_state->evaluated
                ?"Bypassed: the source shape is visible. Editing a point enables its correction again."
                :"Edited fields hold absolute local values. Other fields continue to follow the source. Disable Point Edit to see the source shape.";
        auto* semantics=new QLabel(semantics_text);
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
        for(const auto* field:{"x","y","in.angle","in.length","out.angle","out.length"}) {
            const Ref ref{o.id,canvas->selected_point,field};
            add_property(form,ref,QString::fromLatin1(field));
            if(ref.field=="in.angle"||ref.field=="out.angle")add_point_angle(form,ref,o);
        }
    }
    if(o.compositing.mask)add_compositing_properties(layout,o);
    add_transform_properties(layout,o);
    if(!o.compositing.mask)add_compositing_properties(layout,o);
    if(o.kind==Kind::path||o.kind==Kind::text||o.kind==Kind::group)add_stack(layout,o);
    auto* hint=new QLabel("Right-click a value to copy, paste or unlink.\n↗ picks a property source; += / -= adjusts once.");
    hint->setWordWrap(true);hint->setStyleSheet("color: #929aa6; font-size: 11px;");layout->addWidget(hint);layout->addStretch();restore_scroll();
}
void Window::add_compositing_properties(QVBoxLayout* layout,const Object& object) {
    auto* box=new QGroupBox("Compositing");auto* form=new QFormLayout(box);form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);
    const auto id=object.id;const auto session=host.session_id;
    auto apply=[this,session](Command command){if(host.session_id!=session)throw Error("SESSION_CONFLICT","Compositing belongs to another document");canvas->cancel_interaction();host.session.apply({std::move(command)},host.session.revision());host.edited();};
    const auto visibility_revision=host.session.revision();
    auto apply_visibility=[this,session,visibility_revision](Command command) {
        if(host.session_id!=session)throw Error("SESSION_CONFLICT","Object visibility belongs to another document");
        canvas->cancel_interaction();host.session.apply({std::move(command)},visibility_revision);host.edited();
    };
    auto add_visibility_control=[this,session,visibility_revision,apply_visibility](QWidget* parent,QFormLayout* target_form,const Id& target,const QString& title,const QString& prefix) {
        const Ref ref{target,"","object.visible"};const auto state=object_visibility_state(host.session.document(),ref);
        auto* row=new QWidget(parent);auto* row_layout=new QHBoxLayout(row);row_layout->setContentsMargins(0,0,0,0);
        const bool driven=state.driver.has_value()||state.expression.has_value();
        auto* toggle=new QCheckBox(title,row);toggle->setObjectName(prefix);toggle->setChecked(state.literal);toggle->setEnabled(!driven);row_layout->addWidget(toggle);
        auto* driver=new QToolButton(row);driver->setObjectName(prefix+"-driver");driver->setText(driven?"Source…":"Link…");driver->setPopupMode(QToolButton::InstantPopup);
        auto* menu=new QMenu(driver);driver->setMenu(menu);row_layout->addWidget(driver);target_form->addRow(row);
        connect(toggle,&QCheckBox::toggled,this,[this,toggle,target,apply_visibility](bool enabled){
            bool ok=false;perform([&]{apply_visibility(SetVisibility{target,enabled});ok=true;});
            if(!ok){QSignalBlocker blocker(toggle);toggle->setChecked(!enabled);}
        });
        auto* link=menu->addAction(driven?"Replace visibility source with link…":"Link visibility…");
        auto* expression=menu->addAction(driven?"Replace visibility source with expression…":"Set visibility expression…");
        auto* unlink=menu->addAction("Unlink and freeze evaluated visibility");unlink->setEnabled(driven);
        std::vector<Ref> source_refs;QStringList source_labels;
        const auto& composition=find_composition(host.session.document(),canvas->active_composition());
        std::function<void(const Id&)> append=[&](const Id& source_id) {
            const auto& source=host.session.document().objects.at(source_id);
            if(source_id!=target){source_refs.push_back({source_id,"","object.visible"});source_labels<<qs(source.name)+" — "+qs(source_id);}
            for(const auto& child:source.children)append(child);
        };
        for(const auto& root:composition.roots)append(root);
        link->setEnabled(!source_refs.empty());
        connect(link,&QAction::triggered,this,[this,target,session,visibility_revision,source_refs,source_labels,
            replace=driven,apply_visibility]{
            choose_boolean_source(this,"object-visible-source-dialog",
                replace?"Replace Object visibility source with link":"Link Object visibility",
                qs(target)+" / object.visible",source_refs,source_labels,
                [this,target,session,visibility_revision,replace,apply_visibility](const Ref& source) {
                if(host.session_id!=session)throw Error("SESSION_CONFLICT","Object visibility belongs to another document");
                if(host.session.revision()!=visibility_revision)throw Error("REVISION_CONFLICT","Object visibility changed while the source chooser was open");
                apply_visibility(LinkObjectVisibility{{target,"","object.visible"},source,replace});
            });
        });
        connect(expression,&QAction::triggered,this,[this,target,session,visibility_revision,replace=driven,
            initial=state.expression,apply_visibility]{
            if(host.session_id!=session) {statusBar()->showMessage("SESSION_CONFLICT: Object visibility belongs to another document",12000);return;}
            QDialog dialog(this);dialog.setObjectName("object-visible-expression-dialog");
            dialog.setWindowTitle(replace?"Replace Object visibility source with expression":"Set Object visibility expression");
            dialog.resize(560,210);auto* layout=new QVBoxLayout(&dialog);
            auto* target_label=new QLabel("Target: "+qs(target)+" / object.visible",&dialog);target_label->setWordWrap(true);layout->addWidget(target_label);
            auto* source=new QPlainTextEdit(&dialog);source->setObjectName("object-visible-expression-source");
            source->setPlaceholderText("true, false, ref(\"id\",\"\",\"object.visible\"), or !ref(…)");
            if(initial)source->setPlainText(qs(initial->source));else source->setPlainText("true");
            source->setMinimumHeight(58);layout->addWidget(source);
            auto* status=new QLabel("Version 1 accepts true, false, or an optional negation of a same-Composition object.visible Ref.",&dialog);
            status->setObjectName("object-visible-expression-status");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
            buttons->button(QDialogButtonBox::Apply)->setText("Apply");layout->addWidget(buttons);
            connect(buttons->button(QDialogButtonBox::Cancel),&QPushButton::clicked,&dialog,&QDialog::reject);
            connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[this,&dialog,source,status,target,session,visibility_revision,replace,apply_visibility]{
                try {
                    if(host.session_id!=session)throw Error("SESSION_CONFLICT","Object visibility belongs to another document");
                    if(host.session.revision()!=visibility_revision)throw Error("REVISION_CONFLICT","Object visibility changed while the expression draft was open");
                    apply_visibility(SetObjectVisibilityExpression{{target,"","object.visible"},
                        {source->toPlainText().toStdString(),1},replace});dialog.accept();
                } catch(const Error& error) {status->setText(QString::fromLatin1(error.code.c_str())+": "+QString::fromUtf8(error.what()));}
            });
            dialog.exec();
        });
        connect(unlink,&QAction::triggered,this,[this,target,session,visibility_revision,apply_visibility]{perform([&]{
            if(host.session_id!=session)throw Error("SESSION_CONFLICT","Object visibility belongs to another document");
            if(host.session.revision()!=visibility_revision)throw Error("REVISION_CONFLICT","Object visibility changed before unlinking");
            apply_visibility(UnlinkObjectVisibility{{target,"","object.visible"}});
        });});
        auto* status=new QLabel(parent);status->setObjectName(prefix+"-status");status->setWordWrap(true);
        auto describe=[&](bool value){return value?QString("true"):QString("false");};
        auto details=QString("Authored literal: %1 · Evaluated own visibility: %2").arg(describe(state.literal),describe(state.evaluated));
        if(state.driver) {
            const auto source=host.session.document().objects.find(state.driver->object);
            const auto name=source==host.session.document().objects.end()?qs(state.driver->object):qs(source->second.name)+" ("+qs(state.driver->object)+")";
            details+=" · Linked to "+name;
        }
        if(state.expression)details+=" · Expression: "+qs(state.expression->source);
        status->setText(details);target_form->addRow("Visibility state",status);
    };
    add_visibility_control(box,form,id,"Show artwork","object-visible");
    add_property(form,{id,"","composite.opacity"},"Object opacity");
    auto* blend=new QComboBox;blend->setObjectName("object-blend");
    for(const auto* mode:{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn","hard-light","soft-light","difference","exclusion"})blend->addItem(QString::fromLatin1(mode),QString::fromLatin1(mode));
    blend->setCurrentIndex(blend->findData(qs(object.compositing.blend)));form->addRow("Blend",blend);
    connect(blend,&QComboBox::currentIndexChanged,this,[this,blend,id,apply](int){perform([&]{const auto& current=host.session.document().objects.at(id).compositing;apply(SetCompositing{id,blend->currentData().toString().toStdString(),current.isolated});});});
    const Ref isolation_ref{id,"","composite.isolated"};const auto isolation_state=composite_isolation_state(host.session.document(),isolation_ref);
    const bool isolation_driven=isolation_state.driver.has_value()||isolation_state.expression.has_value();
    auto* isolation_row=new QWidget(box);auto* isolation_layout=new QHBoxLayout(isolation_row);isolation_layout->setContentsMargins(0,0,0,0);
    auto* isolate=new QCheckBox("Isolate from backdrop",isolation_row);isolate->setObjectName("object-isolated");
    isolate->setChecked(isolation_state.literal);isolate->setEnabled(!isolation_driven);isolation_layout->addWidget(isolate);
    auto* isolation_driver=new QToolButton(isolation_row);isolation_driver->setObjectName("object-isolated-driver");
    isolation_driver->setText(isolation_driven?"Source…":"Link…");isolation_driver->setPopupMode(QToolButton::InstantPopup);
    auto* isolation_menu=new QMenu(isolation_driver);isolation_driver->setMenu(isolation_menu);isolation_layout->addWidget(isolation_driver);form->addRow(isolation_row);
    const auto isolation_revision=host.session.revision();
    const auto link_isolation=isolation_menu->addAction(isolation_driven?"Replace isolation source with link…":"Link isolation…");
    const auto expression_isolation=isolation_menu->addAction(isolation_driven?"Replace isolation source with expression…":"Set isolation expression…");
    const auto unlink_isolation=isolation_menu->addAction("Unlink and freeze evaluated isolation");
    unlink_isolation->setEnabled(isolation_driven);
    const auto& composition=find_composition(host.session.document(),canvas->active_composition());
    std::vector<Ref> isolation_sources;QStringList isolation_source_labels;
    std::function<void(const Id&)> append_isolation_source=[&](const Id& source_id) {
        const auto& source=host.session.document().objects.at(source_id);
        if(source_id!=id) {isolation_sources.push_back({source_id,"","composite.isolated"});isolation_source_labels<<qs(source.name)+" — "+qs(source_id);}
        for(const auto& child:source.children)append_isolation_source(child);
    };
    for(const auto& root:composition.roots)append_isolation_source(root);
    link_isolation->setEnabled(!isolation_sources.empty());
    connect(link_isolation,&QAction::triggered,this,[this,id,session,isolation_revision,isolation_sources,
        isolation_source_labels,replace=isolation_driven,apply] {
        choose_boolean_source(this,"object-isolated-source-dialog",
            replace?"Replace Composite isolation source with link":"Link Composite isolation",
            qs(id)+" / composite.isolated",isolation_sources,isolation_source_labels,
            [this,id,session,isolation_revision,replace,apply](const Ref& source) {
            if(host.session_id!=session)throw Error("SESSION_CONFLICT","Composite isolation belongs to another document");
            if(host.session.revision()!=isolation_revision)throw Error("REVISION_CONFLICT","Composite isolation changed while the source chooser was open");
            apply(LinkCompositeIsolated{{id,"","composite.isolated"},source,replace});
        });
    });
    connect(expression_isolation,&QAction::triggered,this,[this,id,session,isolation_revision,
        replace=isolation_driven,initial=isolation_state.expression,apply] {
        if(host.session_id!=session) {statusBar()->showMessage("SESSION_CONFLICT: Composite isolation belongs to another document",12000);return;}
        QDialog dialog(this);dialog.setObjectName("composite-isolated-expression-dialog");
        dialog.setWindowTitle(replace?"Replace Composite isolation source with expression":"Set Composite isolation expression");
        dialog.resize(560,210);auto* draft_layout=new QVBoxLayout(&dialog);
        auto* target_label=new QLabel("Target: "+qs(id)+" / composite.isolated",&dialog);target_label->setWordWrap(true);draft_layout->addWidget(target_label);
        auto* source=new QPlainTextEdit(&dialog);source->setObjectName("composite-isolated-expression-source");
        source->setPlaceholderText("true, false, ref(\"id\",\"\",\"composite.isolated\"), or !ref(…)");
        if(initial)source->setPlainText(qs(initial->source));else source->setPlainText("true");
        source->setMinimumHeight(58);draft_layout->addWidget(source);
        auto* status=new QLabel("Version 1 accepts true, false, or an optional negation of a same-Composition composite.isolated Ref.",&dialog);
        status->setObjectName("composite-isolated-expression-status");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);draft_layout->addWidget(status);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
        buttons->button(QDialogButtonBox::Apply)->setText("Apply");draft_layout->addWidget(buttons);
        connect(buttons->button(QDialogButtonBox::Cancel),&QPushButton::clicked,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[this,&dialog,source,status,id,session,isolation_revision,replace,apply] {
            try {
                if(host.session_id!=session)throw Error("SESSION_CONFLICT","Composite isolation belongs to another document");
                if(host.session.revision()!=isolation_revision)throw Error("REVISION_CONFLICT","Composite isolation changed while the expression draft was open");
                apply(SetCompositeIsolatedExpression{{id,"","composite.isolated"},
                    {source->toPlainText().toStdString(),1},replace});dialog.accept();
            } catch(const Error& error) {status->setText(QString::fromLatin1(error.code.c_str())+": "+QString::fromUtf8(error.what()));}
        });
        dialog.exec();
    });
    connect(unlink_isolation,&QAction::triggered,this,[this,id,session,isolation_revision,apply] {perform([&]{
        if(host.session_id!=session)throw Error("SESSION_CONFLICT","Composite isolation belongs to another document");
        if(host.session.revision()!=isolation_revision)throw Error("REVISION_CONFLICT","Composite isolation changed before unlinking");
        apply(UnlinkCompositeIsolated{{id,"","composite.isolated"}});
    });});
    connect(isolate,&QCheckBox::toggled,this,[this,isolate,id,apply](bool value){bool ok=false;perform([&]{
        apply(SetCompositing{id,host.session.document().objects.at(id).compositing.blend,value});ok=true;
    });if(!ok){QSignalBlocker blocker(isolate);isolate->setChecked(!value);}});
    auto* isolation_status=new QLabel(box);isolation_status->setObjectName("object-isolated-status");isolation_status->setWordWrap(true);
    const auto flag=[](bool value){return value?QString("true"):QString("false");};
    auto isolation_details=QString("Authored literal: %1 · Evaluated authored isolation: %2")
        .arg(flag(isolation_state.literal),flag(isolation_state.evaluated));
    if(isolation_state.driver) {
        const auto source=host.session.document().objects.find(isolation_state.driver->object);
        const auto name=source==host.session.document().objects.end()?qs(isolation_state.driver->object):
            qs(source->second.name)+" ("+qs(isolation_state.driver->object)+")";
        isolation_details+=" · Linked to "+name;
    }
    if(isolation_state.expression)isolation_details+=" · Expression: "+qs(isolation_state.expression->source);
    isolation_status->setText(isolation_details);form->addRow("Isolation state",isolation_status);
    auto* scope=new QLabel("Opacity, masks and blending apply to the composed result. Neutral Groups pass through.");scope->setWordWrap(true);scope->setStyleSheet("color:#9ea7b4;");form->addRow(scope);
    if(!object.compositing.mask)return;
    const auto mask=*object.compositing.mask;
    auto* mask_box=new QGroupBox("Mask");auto* mask_form=new QFormLayout(mask_box);mask_form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(mask_box);
    auto* mode=new QComboBox(mask_box);mode->setObjectName("mask-mode");
    mode->addItem("Geometry","geometry");mode->addItem("Alpha","alpha");mode->addItem("Luma","luma");
    mode->setCurrentIndex(mask.mode=="alpha"?1:mask.mode=="luma"?2:0);mask_form->addRow("Mode",mode);
    auto* profile=new QLabel(QString::fromStdString(mask.mask_color_space)+" · 8-bit",mask_box);
    profile->setObjectName("mask-color-space");mask_form->addRow("Color profile",profile);
    auto* invert=new QCheckBox("Invert mask",mask_box);invert->setObjectName("mask-invert");invert->setChecked(mask.invert);
    invert->setEnabled(mask.mode=="alpha"||mask.mode=="luma");mask_form->addRow(invert);
      connect(mode,&QComboBox::currentIndexChanged,this,[this,mode,invert,id,apply](int) {
          const auto requested=mode->currentData().toString().toStdString();
          invert->setEnabled(requested=="alpha"||requested=="luma");bool ok=false;
          perform([&]{auto current=*host.session.document().objects.at(id).compositing.mask;current.mode=requested;
              if(requested=="geometry")current.invert=false;apply(SetMask{id,current});ok=true;});
        if(!ok) {
            const auto current=host.session.document().objects.at(id).compositing.mask->mode;
            QSignalBlocker blocker(mode);mode->setCurrentIndex(current=="alpha"?1:current=="luma"?2:0);
            invert->setEnabled(current=="alpha"||current=="luma");
        }
    });
    connect(invert,&QCheckBox::toggled,this,[this,invert,id,apply](bool value){bool ok=false;perform([&]{
        auto current=*host.session.document().objects.at(id).compositing.mask;current.invert=value;apply(SetMask{id,current});ok=true;
    });if(!ok){QSignalBlocker blocker(invert);invert->setChecked(!value);}});
    const auto mask_ref=geometry_mask_enabled_ref(id,mask.id);
    const auto mask_state=geometry_mask_enabled_state(host.session.document(),mask_ref);
    const bool mask_enabled_driven=mask_state.driver.has_value()||mask_state.expression.has_value();
    auto* enabled_row=new QWidget(mask_box);auto* enabled_layout=new QHBoxLayout(enabled_row);enabled_layout->setContentsMargins(0,0,0,0);
    auto* enabled=new QCheckBox("Mask enabled",enabled_row);enabled->setObjectName("mask-enabled");
    enabled->setChecked(mask_state.literal);enabled->setEnabled(!mask_enabled_driven);enabled_layout->addWidget(enabled);
    auto* driver_button=new QToolButton(enabled_row);driver_button->setObjectName("mask-enabled-driver");
    driver_button->setText(mask_state.driver?"Driver…":mask_state.expression?"Expression…":"Link…");driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* driver_menu=new QMenu(driver_button);driver_button->setMenu(driver_menu);enabled_layout->addWidget(driver_button);enabled_layout->addStretch();
    mask_form->addRow(enabled_row);
    connect(enabled,&QCheckBox::toggled,this,[this,enabled,id,apply](bool value){bool ok=false;perform([&]{auto mask=*host.session.document().objects.at(id).compositing.mask;mask.enabled=value;apply(SetMask{id,mask});ok=true;});if(!ok){QSignalBlocker b(enabled);enabled->setChecked(!value);}});
    auto* link_mask=driver_menu->addAction(mask_enabled_driven?"Replace enabled source with link…":"Link enabled source…");
    auto* expression_mask=driver_menu->addAction(mask_enabled_driven?"Replace enabled source with expression…":"Set enabled expression…");
    auto* unlink_mask=driver_menu->addAction("Unlink and freeze evaluated value");unlink_mask->setEnabled(mask_enabled_driven);
    const auto mask_revision=host.session.revision();
    std::vector<Ref> mask_sources;QStringList mask_source_labels;
    const auto& mask_composition=find_composition(host.session.document(),canvas->active_composition());
    std::function<void(const Id&)> append_mask_source=[&](const Id& source_id) {
        const auto& source_object=host.session.document().objects.at(source_id);
        if(source_object.compositing.mask) {
            const auto source_ref=geometry_mask_enabled_ref(source_id,source_object.compositing.mask->id);
            if(source_ref!=mask_ref) {
                mask_sources.push_back(source_ref);
                mask_source_labels<<qs(source_object.name)+" — Mask ["+qs(source_object.compositing.mask->id)+"] — "+qs(source_id);
            }
        }
        for(const auto& child:source_object.children)append_mask_source(child);
    };
    for(const auto& root:mask_composition.roots)append_mask_source(root);
    link_mask->setEnabled(!mask_sources.empty());
    connect(link_mask,&QAction::triggered,this,[this,mask_ref,session,mask_revision,mask_sources,mask_source_labels,
        replace=mask_enabled_driven,apply] {
        choose_boolean_source(this,"mask-enabled-source-dialog",
            replace?"Replace Geometry mask enabled source with link":"Link Geometry mask enabled",
            qs(mask_ref.object)+" / "+qs(mask_ref.field),mask_sources,mask_source_labels,
            [this,mask_ref,session,mask_revision,replace,apply](const Ref& source) {
            if(host.session_id!=session)throw Error("SESSION_CONFLICT","Geometry mask belongs to another document");
            if(host.session.revision()!=mask_revision)throw Error("REVISION_CONFLICT","Geometry mask enabled state changed while the source chooser was open");
            apply(LinkMaskEnabled{mask_ref,source,replace});
        });
    });
    connect(expression_mask,&QAction::triggered,this,[this,mask_ref,session,mask_revision,
        replace=mask_enabled_driven,initial=mask_state.expression,apply] {
        if(host.session_id!=session) {statusBar()->showMessage("SESSION_CONFLICT: Geometry mask belongs to another document",12000);return;}
        QDialog dialog(this);dialog.setObjectName("mask-enabled-expression-dialog");
        dialog.setWindowTitle(replace?"Replace Geometry mask enabled source with expression":"Set Geometry mask enabled expression");
        dialog.resize(560,210);auto* draft_layout=new QVBoxLayout(&dialog);
        auto* target_label=new QLabel("Target: "+qs(mask_ref.object)+" / "+qs(mask_ref.field),&dialog);
        target_label->setWordWrap(true);draft_layout->addWidget(target_label);
        auto* source=new QPlainTextEdit(&dialog);source->setObjectName("mask-enabled-expression-source");
        source->setPlaceholderText("true, false, ref(\"object-id\",\"\",\"mask.mask-id.enabled\"), or !ref(…)");
        if(initial)source->setPlainText(qs(initial->source));else source->setPlainText("true");
        source->setMinimumHeight(58);draft_layout->addWidget(source);
        auto* status=new QLabel("Version 1 accepts true, false, or an optional negation of an installed GeometryMask.enabled Ref in this Composition. Apply commits one Session command; Cancel leaves it unchanged.",&dialog);
        status->setObjectName("mask-enabled-expression-status");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);draft_layout->addWidget(status);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
        buttons->button(QDialogButtonBox::Apply)->setText("Apply");draft_layout->addWidget(buttons);
        connect(buttons->button(QDialogButtonBox::Cancel),&QPushButton::clicked,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
            [this,&dialog,source,status,mask_ref,session,mask_revision,replace,apply] {
            try {
                if(host.session_id!=session)throw Error("SESSION_CONFLICT","Geometry mask belongs to another document");
                if(host.session.revision()!=mask_revision)throw Error("REVISION_CONFLICT","Geometry mask enabled state changed while the expression draft was open");
                apply(SetMaskEnabledExpression{mask_ref,{source->toPlainText().toStdString(),1},replace});dialog.accept();
            } catch(const Error& error) {
                status->setText(QString::fromLatin1(error.code.c_str())+": "+QString::fromUtf8(error.what()));
            }
        });
        dialog.exec();
    });
    connect(unlink_mask,&QAction::triggered,this,[this,mask_ref,session,mask_revision,apply]{perform([&]{
        if(host.session_id!=session)throw Error("SESSION_CONFLICT","Geometry mask belongs to another document");
        if(host.session.revision()!=mask_revision)throw Error("REVISION_CONFLICT","Geometry mask enabled state changed before unlinking");
        apply(UnlinkMaskEnabled{mask_ref});
    });});
    auto* mask_status=new QLabel(mask_box);mask_status->setObjectName("mask-enabled-state");mask_status->setWordWrap(true);
    QString mask_source="none";
    if(mask_state.driver) {
        const auto source_object=host.session.document().objects.find(mask_state.driver->object);
        const auto source_name=source_object==host.session.document().objects.end()?qs(mask_state.driver->object):qs(source_object->second.name);
        const auto field=mask_state.driver->field;const auto dot=field.rfind('.');
        const auto source_mask_id=field.substr(5,dot-5);
        mask_source=source_name+" / Mask ["+qs(source_mask_id)+"]";
    } else if(mask_state.expression)mask_source="expression: "+qs(mask_state.expression->source);
    mask_status->setText(QString("Authored literal: %1 · Source: %2 · Evaluated enabled: %3")
        .arg(mask_state.literal?"true":"false",mask_source,mask_state.evaluated?"true":"false"));
    mask_form->addRow("Mask enabled state",mask_status);
    auto* edit=new QPushButton("Edit: "+qs(host.session.document().objects.at(mask.source).name));edit->setObjectName("mask-edit-source");edit->setToolTip("Select the retained source to edit its points and parameters. Its normal visibility stays unchanged.");mask_form->addRow(edit);
    connect(edit,&QPushButton::clicked,this,[this,source=mask.source]{canvas->set_selection(source);});
    auto* rule=new QComboBox;rule->setObjectName("mask-fill-rule");rule->addItem("Nonzero","nonzero");rule->addItem("Even–odd","evenodd");
    rule->setCurrentIndex(mask.fill_rule=="evenodd"?1:0);rule->setEnabled(mask.mode=="geometry");mask_form->addRow("Fill rule",rule);
    connect(rule,&QComboBox::currentIndexChanged,this,[this,rule,id,apply](int){perform([&]{auto mask=*host.session.document().objects.at(id).compositing.mask;mask.fill_rule=rule->currentData().toString().toStdString();apply(SetMask{id,mask});});});
    auto* outline=new QCheckBox("Show mask outline");outline->setObjectName("mask-show-outline");outline->setChecked(canvas->show_mask_outline());mask_form->addRow(outline);
    connect(outline,&QCheckBox::toggled,canvas,&Canvas::set_show_mask_outline);
    add_visibility_control(mask_box,mask_form,mask.source,"Show source artwork","mask-source-visible");
    auto* remove=new QPushButton("Remove mask");remove->setObjectName("mask-remove");remove->setToolTip("Remove clipping; keep source object and its current visibility. Undo restores the mask.");mask_form->addRow(remove);
    connect(remove,&QPushButton::clicked,this,[this,id,apply]{perform([&]{apply(SetMask{id,{}});});});
    auto* note=new QLabel(mask.mode=="alpha"
        ?"Uses the source's isolated RGBA appearance in Composition space. Source root visibility and blend are ignored; source opacity and internal Group content are included."
        :mask.mode=="luma"
            ?"Uses isolated source RGBA in 8-bit sRGB. Luminance coefficients 0.2125, 0.7154 and 0.0721 are multiplied by source alpha before optional inversion."
            :"Uses final source geometry in Composition space. Source paint and opacity do not affect this mask; open paths close implicitly.");
    note->setWordWrap(true);note->setStyleSheet("color:#9ea7b4;");mask_form->addRow(note);
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
void Window::show_folder_library() {
    canvas->cancel_interaction();
    auto& library=*folder_library_;

    QDialog dialog(this);dialog.setWindowTitle("Folder Library");dialog.setObjectName("folder-library-dialog");dialog.resize(1120,700);
    auto* layout=new QVBoxLayout(&dialog);
    auto* hint=new QLabel("Browse registered folders and shared workspace Favorites. Built-in Effect Favorites keep their exact TypeID and BehaviorVersion.",&dialog);
    hint->setWordWrap(true);layout->addWidget(hint);

    auto* toolbar=new QHBoxLayout;layout->addLayout(toolbar);
    auto* add_root=new QPushButton("Add folder…",&dialog);add_root->setObjectName("folder-library-add-root");toolbar->addWidget(add_root);
    auto* remove_root=new QPushButton("Unregister selected root",&dialog);remove_root->setObjectName("folder-library-remove-root");toolbar->addWidget(remove_root);
    auto* refresh_button=new QPushButton("Refresh",&dialog);refresh_button->setObjectName("folder-library-refresh");toolbar->addWidget(refresh_button);
    auto* search=new QLineEdit(&dialog);search->setObjectName("folder-library-search");search->setPlaceholderText("Search registered folders…");search->setClearButtonEnabled(true);toolbar->addWidget(search,1);

    auto* preset_row=new QHBoxLayout;layout->addLayout(preset_row);
    preset_row->addWidget(new QLabel("Workspace Presets:",&dialog));
    auto* preset_assets=new QComboBox(&dialog);preset_assets->setObjectName("folder-library-preset-assets");preset_row->addWidget(preset_assets,2);
    auto* source_presets=new QComboBox(&dialog);source_presets->setObjectName("folder-library-source-presets");
    source_presets->addItem("Document Preset to update…",QString{});
    for(const auto& [id,definition]:host.session.document().preset_definitions)
        source_presets->addItem(QString::fromStdString(definition.label)+" · "+QString::fromStdString(id),QString::fromStdString(id));
    preset_row->addWidget(source_presets,2);
    auto* update_preset=new QPushButton("Update Asset",&dialog);update_preset->setObjectName("folder-library-preset-update");preset_row->addWidget(update_preset);
    auto* delete_preset=new QPushButton("Delete Asset",&dialog);delete_preset->setObjectName("folder-library-preset-delete");preset_row->addWidget(delete_preset);

    auto* macro_row=new QHBoxLayout;layout->addLayout(macro_row);
    macro_row->addWidget(new QLabel("Workspace Macros:",&dialog));
    auto* macro_assets=new QComboBox(&dialog);macro_assets->setObjectName("folder-library-macro-assets");macro_row->addWidget(macro_assets,2);
    auto* source_macros=new QComboBox(&dialog);source_macros->setObjectName("folder-library-source-macros");
    source_macros->addItem("Document Macro to publish/update…",QString{});
    for(const auto& [id,definition]:host.session.document().macro_definitions)
        source_macros->addItem(QString::fromStdString(definition.label)+" · "+QString::fromStdString(id),QString::fromStdString(id));
    macro_row->addWidget(source_macros,2);
    auto* macro_pin=new QComboBox(&dialog);macro_pin->setObjectName("folder-library-macro-pin");
    macro_pin->addItem("Choose retained revision…",QVariant{});macro_row->addWidget(macro_pin);
    auto* publish_macro=new QPushButton("Publish Macro",&dialog);publish_macro->setObjectName("folder-library-macro-publish");macro_row->addWidget(publish_macro);
    auto* update_macro=new QPushButton("Update Asset",&dialog);update_macro->setObjectName("folder-library-macro-update");macro_row->addWidget(update_macro);
    auto* delete_macro=new QPushButton("Delete Asset",&dialog);delete_macro->setObjectName("folder-library-macro-delete");macro_row->addWidget(delete_macro);
    auto* favorite_macro=new QPushButton("Favorite Macro",&dialog);favorite_macro->setObjectName("folder-library-macro-favorite");macro_row->addWidget(favorite_macro);
    auto* apply_macro=new QPushButton("Apply Macro",&dialog);apply_macro->setObjectName("folder-library-macro-apply");macro_row->addWidget(apply_macro);

    auto* panes=new QHBoxLayout;layout->addLayout(panes,1);
    auto* tree=new QTreeWidget(&dialog);tree->setObjectName("folder-library-tree");tree->setHeaderLabel("Registered folders");
    tree->setSelectionMode(QAbstractItemView::SingleSelection);panes->addWidget(tree,3);
    auto* favorites=new QListWidget(&dialog);favorites->setObjectName("folder-library-favorites");favorites->setSelectionMode(QAbstractItemView::SingleSelection);
    favorites->setMinimumWidth(280);panes->addWidget(favorites,2);

    auto* status=new QLabel("Select an image or folder. Favorites keep stable references and show broken items explicitly.",&dialog);
    status->setObjectName("folder-library-status");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);

    auto* controls=new QHBoxLayout;layout->addLayout(controls);
    auto* add_favorite=new QPushButton("Add Favorite",&dialog);add_favorite->setObjectName("folder-library-favorite-add");controls->addWidget(add_favorite);
    auto* remove_favorite=new QPushButton("Remove Favorite",&dialog);remove_favorite->setObjectName("folder-library-favorite-remove");controls->addWidget(remove_favorite);
    auto* slot=new QComboBox(&dialog);slot->setObjectName("folder-library-slot");slot->addItem("No slot",0);
    for(int number=1;number<=9;++number)slot->addItem("Quick slot "+QString::number(number),number);
    controls->addWidget(slot);
    auto* assign_slot=new QPushButton("Set slot",&dialog);assign_slot->setObjectName("folder-library-slot-set");controls->addWidget(assign_slot);
    auto* place_linked=new QPushButton("Place Linked",&dialog);place_linked->setObjectName("folder-library-place-linked");controls->addWidget(place_linked);
    auto* place_embedded=new QPushButton("Place Embedded",&dialog);place_embedded->setObjectName("folder-library-place-embedded");controls->addWidget(place_embedded);
    auto* use_favorite=new QPushButton("Use Favorite",&dialog);use_favorite->setObjectName("folder-library-use-favorite");controls->addWidget(use_favorite);
    auto* use_slot=new QPushButton("Use Quick Access",&dialog);use_slot->setObjectName("folder-library-use-slot");controls->addWidget(use_slot);
    auto* close=new QPushButton("Close",&dialog);close->setObjectName("folder-library-close");controls->addWidget(close);

    auto* macro_overrides=new QHBoxLayout;layout->addLayout(macro_overrides);
    macro_overrides->addWidget(new QLabel("Optional Offset Amount override (blank uses the pinned default):",&dialog));
    auto* macro_override_value=new QLineEdit(&dialog);macro_override_value->setObjectName("folder-library-macro-override-value");
    macro_override_value->setPlaceholderText("du");macro_overrides->addWidget(macro_override_value);
    QString macro_override_asset_id;

    QHash<QString,QTreeWidgetItem*> node_by_identity;
    const auto relative_parent=[](const QString& path) {
        const auto slash=path.lastIndexOf('/');return slash<0?QString{}:path.left(slash);
    };
    const auto relative_leaf=[](const QString& path) {
        const auto slash=path.lastIndexOf('/');return slash<0?path:path.mid(slash+1);
    };
    auto ref_for_item=[](const QTreeWidgetItem* item) {
        if(!item)throw Error("NO_SELECTION","Choose a visible Folder Library item");
        return FolderLibrary::ref_from_json(QJsonDocument::fromJson(item->data(0,Qt::UserRole).toByteArray()).object());
    };
    auto display_ref=[&](const LibraryItemRefV1& ref) {
        const auto found=std::find_if(library.roots().begin(),library.roots().end(),[&](const auto& root){return root.root_id==ref.root_id;});
        const auto root_label=found==library.roots().end()?QString("Missing root · ")+ref.root_id:found->display_name;
        return ref.normalized_relative_path.isEmpty()?root_label:root_label+" / "+ref.normalized_relative_path;
    };
    auto refresh_preset_assets=[&] {
        const auto selected=preset_assets->currentData().toString();
        const QSignalBlocker blocker(preset_assets);
        preset_assets->clear();preset_assets->addItem("Choose a Workspace Preset asset…",QString{});
        try {
            for(const auto& asset:library.preset_assets()) {
                const auto label=asset.label+" · r"+QString::number(asset.accepted_revision)+" · "+asset.ref.asset_id;
                preset_assets->addItem(label,asset.ref.asset_id);
                const auto index=preset_assets->count()-1;
                preset_assets->setItemData(index,static_cast<qulonglong>(asset.accepted_revision),Qt::UserRole+1);
                preset_assets->setItemData(index,asset.sha256,Qt::UserRole+2);
                preset_assets->setItemData(index,asset.available?"Available":asset.problem,Qt::ToolTipRole);
                if(!asset.available)preset_assets->setItemData(index,QColor(226,143,143),Qt::ForegroundRole);
                if(asset.ref.asset_id==selected)preset_assets->setCurrentIndex(index);
            }
        } catch(const Error& error) {
            const auto detail=qs(error.code)+": "+QString::fromUtf8(error.what());
            preset_assets->clear();preset_assets->addItem("Workspace Presets unavailable · "+detail,QString{});
            preset_assets->setItemData(0,detail,Qt::ToolTipRole);
            preset_assets->setItemData(0,QColor(226,143,143),Qt::ForegroundRole);
            status->setText("Workspace Presets unavailable · "+detail+". Registered folders and existing Favorites remain available.");
        } catch(const std::exception& error) {
            const auto detail=QString::fromUtf8(error.what());
            preset_assets->clear();preset_assets->addItem("Workspace Presets unavailable · "+detail,QString{});
            preset_assets->setItemData(0,detail,Qt::ToolTipRole);
            preset_assets->setItemData(0,QColor(226,143,143),Qt::ForegroundRole);
            status->setText("Workspace Presets unavailable · "+detail+". Registered folders and existing Favorites remain available.");
        }
    };
    QHash<QString,QPair<qulonglong,QString>> macro_asset_snapshots;
    QString macro_pin_asset_id;
    auto refresh_macro_assets=[&] {
        const auto selected=macro_assets->currentData().toString();
        const QSignalBlocker blocker(macro_assets);
        macro_assets->clear();macro_assets->addItem("Choose a Workspace Macro asset…",QString{});
        macro_asset_snapshots.clear();
        try {
            for(const auto& asset:library.macro_assets()) {
                const auto label=asset.label+" · r"+QString::number(asset.accepted_revision)+" · "+asset.ref.asset_id;
                macro_assets->addItem(label,asset.ref.asset_id);
                const auto index=macro_assets->count()-1;
                macro_assets->setItemData(index,static_cast<qulonglong>(asset.accepted_revision),Qt::UserRole+1);
                macro_assets->setItemData(index,asset.sha256,Qt::UserRole+2);
                macro_assets->setItemData(index,asset.available?"Available":asset.problem,Qt::ToolTipRole);
                if(!asset.available)macro_assets->setItemData(index,QColor(226,143,143),Qt::ForegroundRole);
                macro_asset_snapshots.insert(asset.ref.asset_id,{static_cast<qulonglong>(asset.accepted_revision),asset.sha256});
                if(asset.ref.asset_id==selected)macro_assets->setCurrentIndex(index);
            }
        } catch(const Error& error) {
            const auto detail=qs(error.code)+": "+QString::fromUtf8(error.what());
            macro_assets->clear();macro_assets->addItem("Workspace Macros unavailable · "+detail,QString{});
            macro_assets->setItemData(0,detail,Qt::ToolTipRole);
            macro_assets->setItemData(0,QColor(226,143,143),Qt::ForegroundRole);
            status->setText("Workspace Macros unavailable · "+detail+". Existing Favorites remain visible.");
            macro_asset_snapshots.clear();
        } catch(const std::exception& error) {
            const auto detail=QString::fromUtf8(error.what());
            macro_assets->clear();macro_assets->addItem("Workspace Macros unavailable · "+detail,QString{});
            macro_assets->setItemData(0,detail,Qt::ToolTipRole);
            macro_assets->setItemData(0,QColor(226,143,143),Qt::ForegroundRole);
            status->setText("Workspace Macros unavailable · "+detail+". Existing Favorites remain visible.");
            macro_asset_snapshots.clear();
        }
    };
    auto refresh_macro_pins=[&] {
        const auto selected=macro_pin->currentData();
        const QSignalBlocker blocker(macro_pin);
        macro_pin->clear();macro_pin->addItem("Choose retained revision…",QVariant{});
        const auto asset_id=macro_assets->currentData().toString();
        if(asset_id.isEmpty())return;
        const auto snapshot=macro_asset_snapshots.constFind(asset_id);
        if(snapshot==macro_asset_snapshots.cend())return;
        try {
            LibraryMacroAssetV1 metadata;
            const auto definition=library.read_macro_asset({asset_id},&metadata);
            if(metadata.accepted_revision!=snapshot->first||metadata.sha256!=snapshot->second)
                throw Error("MACRO_ASSET_SELECTION_CHANGED","Workspace Macro changed since the last explicit Refresh; refresh before selecting a retained pin");
            for(const auto& [revision,graph]:definition.revisions) {
                (void)graph;
                const auto label=QString("Revision %1%2").arg(revision).arg(revision==definition.latest_revision?" · accepted latest":"");
                macro_pin->addItem(label,QVariant::fromValue<qulonglong>(static_cast<qulonglong>(revision)));
                if(selected.isValid()&&selected.toULongLong()==revision)macro_pin->setCurrentIndex(macro_pin->count()-1);
                else if(!selected.isValid()&&revision==definition.latest_revision)macro_pin->setCurrentIndex(macro_pin->count()-1);
            }
        } catch(const Error& error) {
            macro_pin->setItemText(0,"Refresh required · "+qs(error.code));
            macro_pin->setItemData(0,QString::fromUtf8(error.what()),Qt::ToolTipRole);
            macro_pin->setItemData(0,QColor(226,143,143),Qt::ForegroundRole);
        } catch(const std::exception& error) {
            macro_pin->setItemText(0,"Workspace Macro unavailable");
            macro_pin->setItemData(0,QString::fromUtf8(error.what()),Qt::ToolTipRole);
            macro_pin->setItemData(0,QColor(226,143,143),Qt::ForegroundRole);
        }
    };
    auto display_target=[&](const LibraryFavoriteTargetV1& target) {
        if(const auto* item=std::get_if<LibraryItemRefV1>(&target))return display_ref(*item);
        if(const auto* effect=std::get_if<BuiltinEffectTypeRefV1>(&target)) {
            const auto* descriptor=builtin_operation_type(effect->type_id.toStdString());
            if(descriptor&&descriptor->effects_catalog&&descriptor->version==effect->behavior_version)
                return qs(descriptor->label)+" · "+effect->type_id+" behavior v"+QString::number(effect->behavior_version);
            return QString("Unavailable effect · ")+effect->type_id+" behavior v"+QString::number(effect->behavior_version);
        }
        if(const auto* preset=std::get_if<PresetAssetRefV1>(&target))return QString("Workspace Preset · ")+preset->asset_id;
        return QString("Workspace Macro · ")+std::get<MacroAssetRefV1>(target).asset_id;
    };
    auto rebuild_favorites=[&] {
        const auto selected=favorites->currentItem()?favorites->currentItem()->data(Qt::UserRole).toString():QString{};
        favorites->clear();
        for(const auto& favorite:library.favorites()) {
            const auto state=library.favorite_status(favorite);
            const auto label=display_target(favorite.target);
            auto* item=new QListWidgetItem(label+QString(" · %1%2").arg(state,
                favorite.quick_slot?" · Quick slot "+QString::number(favorite.quick_slot):QString{}),favorites);
            item->setData(Qt::UserRole,favorite.favorite_id);
            item->setData(Qt::UserRole+1,QJsonDocument(FolderLibrary::target_to_json(favorite.target)).toJson(QJsonDocument::Compact));
            item->setData(Qt::UserRole+2,favorite.quick_slot);
            item->setToolTip(label+"\n"+state);
            if(state!="Available")item->setForeground(QColor(226,143,143));
            if(favorite.favorite_id==selected)favorites->setCurrentItem(item);
        }
    };
    auto sync_preset_controls=[&] {
        const bool has_asset=!preset_assets->currentData().toString().isEmpty();
        update_preset->setEnabled(has_asset&&!source_presets->currentData().toString().isEmpty());
        delete_preset->setEnabled(has_asset);
    };
    auto sync_macro_controls=[&] {
        const bool has_asset=!macro_assets->currentData().toString().isEmpty();
        const bool has_source=!source_macros->currentData().toString().isEmpty();
        publish_macro->setEnabled(has_source);
        update_macro->setEnabled(has_asset&&has_source);
        delete_macro->setEnabled(has_asset);
        favorite_macro->setEnabled(has_asset);
        apply_macro->setEnabled(has_asset&&macro_pin->currentData().isValid());
    };
    refresh_preset_assets();
    refresh_macro_assets();refresh_macro_pins();
    connect(preset_assets,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{sync_preset_controls();});
    connect(source_presets,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{sync_preset_controls();});
    connect(macro_assets,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{
        const auto current_asset_id=macro_assets->currentData().toString();
        if(current_asset_id!=macro_pin_asset_id) {
            const QSignalBlocker reset_pin(macro_pin);
            macro_pin->setCurrentIndex(0);
            macro_pin_asset_id=current_asset_id;
        }
        refresh_macro_pins();macro_override_asset_id=macro_assets->currentData().toString();macro_override_value->clear();sync_macro_controls();
    });
    connect(macro_override_value,&QLineEdit::textEdited,&dialog,[&]{macro_override_asset_id=macro_assets->currentData().toString();});
    connect(source_macros,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{sync_macro_controls();});
    connect(macro_pin,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{sync_macro_controls();});
    sync_preset_controls();
    sync_macro_controls();
    auto rebuild_tree=[&] {
        tree->clear();node_by_identity.clear();
        QHash<QString,QTreeWidgetItem*> root_nodes;
        for(const auto& root:library.roots()) {
            const LibraryItemRefV1 ref{root.root_id,{},"folder"};
            auto* item=new QTreeWidgetItem(tree,{root.display_name});
            const auto identity=library.comparison_key(ref);
            item->setData(0,Qt::UserRole,QJsonDocument(FolderLibrary::ref_to_json(ref)).toJson(QJsonDocument::Compact));
            item->setData(0,Qt::UserRole+1,identity);item->setToolTip(0,root.absolute_path);
            node_by_identity.insert(identity,item);root_nodes.insert(root.root_id,item);
        }
        for(const auto& entry:library.items()) {
            if(entry.ref.normalized_relative_path.isEmpty()) {
                if(!entry.available) {
                    auto* item=root_nodes.value(entry.ref.root_id,nullptr);
                    if(item){item->setText(0,item->text(0)+" · missing");item->setForeground(0,QColor(226,143,143));item->setToolTip(0,entry.problem+"\n"+entry.absolute_path);}
                }
                continue;
            }
            auto* root_node=root_nodes.value(entry.ref.root_id,nullptr);if(!root_node)continue;
            const auto identity=library.comparison_key(entry.ref);
            const auto parent_ref=LibraryItemRefV1{entry.ref.root_id,relative_parent(entry.ref.normalized_relative_path),"folder"};
            auto* parent=relative_parent(entry.ref.normalized_relative_path).isEmpty()?root_node:node_by_identity.value(library.comparison_key(parent_ref),root_node);
            auto* item=new QTreeWidgetItem(parent,{relative_leaf(entry.ref.normalized_relative_path)});
            item->setData(0,Qt::UserRole,QJsonDocument(FolderLibrary::ref_to_json(entry.ref)).toJson(QJsonDocument::Compact));
            item->setData(0,Qt::UserRole+1,identity);item->setData(0,Qt::UserRole+2,entry.available);
            item->setToolTip(0,entry.available?entry.absolute_path:entry.problem+"\n"+entry.absolute_path);
            if(!entry.available)item->setForeground(0,QColor(226,143,143));
            node_by_identity.insert(identity,item);
        }
        tree->expandToDepth(1);
        rebuild_favorites();
    };
    auto apply_search=[&](const QString& query) {
        const auto found=library.search(query);
        QSet<QString> matched;
        for(const auto& entry:found)matched.insert(library.comparison_key(entry.ref));
        std::function<bool(QTreeWidgetItem*)> filter=[&](QTreeWidgetItem* item) {
            bool child_match=false;
            for(int index=0;index<item->childCount();++index)child_match=filter(item->child(index))||child_match;
            const bool self_match=query.trimmed().isEmpty()||matched.contains(item->data(0,Qt::UserRole+1).toString());
            item->setHidden(!self_match&&!child_match);
            return self_match||child_match;
        };
        for(int index=0;index<tree->topLevelItemCount();++index)filter(tree->topLevelItem(index));
    };
    auto place_ref=[&](const LibraryItemRefV1& ref,const QString& mode,QString frozen_session,std::uint64_t& expected_revision) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Document changed while the Folder Library was open; close and reopen it to choose a current target");
        if(host.session.revision()!=expected_revision)throw Error("REVISION_CONFLICT","Document changed while the Folder Library was open; close and reopen it to choose a current target");
        const auto resolved=library.resolve(ref);
        if(resolved.ref.kind!="raster")throw Error("UNSUPPORTED_LIBRARY_ITEM","Choose a PNG or JPEG file to place");
        const auto composition=canvas->active_composition();
        const auto board=evaluate_artboard(find_composition(host.session.document(),composition),canvas->active_artboard());
        const auto object_id=new_id();
        host.import_image(resolved.absolute_path,mode.toStdString(),composition,"",new_id(),object_id,
            QFileInfo(resolved.absolute_path).completeBaseName().toStdString(),board.x,board.y,expected_revision);
        expected_revision=host.session.revision();
        canvas->set_selection(object_id);canvas->setFocus();
        status->setText("Placed "+display_ref(resolved.ref)+" as "+mode+"; source files are read-only inputs.");
    };
    QString frozen_session=host.session_id;
    std::uint64_t expected_revision=host.session.revision();
    const QString frozen_effect_session=host.session_id;
    const Id frozen_effect_target=canvas->selected_object;
    auto frozen_effect_revision=host.session.revision();
    auto frozen_effect_generation=effects_generation_;
    auto selected_macro_overrides=[&] {
        std::map<std::string,double> overrides;
        const auto value_text=macro_override_value->text().trimmed();
        if(value_text.isEmpty())return overrides;
        if(macro_override_asset_id.isEmpty())throw Error("INVALID_MACRO_OVERRIDES","Choose a Workspace Macro before entering an override");
        if(macro_override_asset_id!=macro_assets->currentData().toString())
            throw Error("MACRO_OVERRIDE_TARGET_MISMATCH","Offset override draft belongs to a different Workspace Macro");
        bool valid=false;const auto value=value_text.toDouble(&valid);
        if(!valid||!std::isfinite(value))throw Error("INVALID_MACRO_OVERRIDES","Macro override must be a finite number");
        overrides.emplace("macro.offset.amount",value);
        return overrides;
    };
    auto apply_macro_asset=[&](const MacroAssetRefV1& ref,std::optional<std::uint64_t> requested_pin) {
        if(host.session_id!=frozen_effect_session)
            throw Error("SESSION_CONFLICT","Macro Favorite belongs to another document");
        if(frozen_effect_generation!=effects_generation_)
            throw Error("REVISION_CONFLICT","Effects panel changed; refresh the target before applying");
        if(canvas->selected_object!=frozen_effect_target)
            throw Error("TARGET_CONFLICT","Macro target changed; choose the current target");
        if(host.session.revision()!=frozen_effect_revision)
            throw Error("REVISION_CONFLICT","Document changed while the Folder Library was open; close and reopen it to choose a current target");
        if(macro_override_value->text().trimmed().size()&&macro_override_asset_id!=ref.asset_id)
            throw Error("MACRO_OVERRIDE_TARGET_MISMATCH","Offset override draft belongs to a different Workspace Macro; select that Macro before applying the draft");
        const auto snapshot=macro_asset_snapshots.constFind(ref.asset_id);
        if(snapshot==macro_asset_snapshots.cend()||snapshot->first==0||snapshot->second.size()!=64)
            throw Error("UNAVAILABLE_MACRO_ASSET","Refresh the Library and choose an available Macro asset");
        LibraryMacroAssetV1 metadata;
        auto definition=library.read_macro_asset(ref,&metadata);
        if(metadata.accepted_revision!=snapshot->first||metadata.sha256!=snapshot->second)
            throw Error("MACRO_ASSET_SELECTION_CHANGED","Workspace Macro changed since the last explicit Refresh; refresh before applying it");
        const auto pin=requested_pin.value_or(definition.latest_revision);
        if(!definition.revisions.contains(pin))throw Error("MISSING_MACRO_REVISION","Choose an explicit retained Macro revision");
        auto overrides=selected_macro_overrides();
        Id definition_id;
        do {definition_id=new_id();}
        while(definition_id==definition.id||definition_id==ref.asset_id.toStdString()||
            host.session.document().macro_definitions.contains(definition_id)||
            host.session.document().objects.contains(definition_id));
        Id instance_id;
        do {instance_id=new_id();}
        while(instance_id==definition.id||instance_id==definition_id||instance_id==ref.asset_id.toStdString()||
            host.session.document().macro_definitions.contains(instance_id)||
            host.session.document().objects.contains(instance_id));
        const auto object=host.session.document().objects.find(frozen_effect_target);
        if(object==host.session.document().objects.end())throw Error("MISSING_OBJECT",frozen_effect_target);
        InstantiateMacro import{frozen_effect_target,definition_id,instance_id,pin,object->second.stack.size()};
        import.imported_definition=std::move(definition);
        import.asset_id=ref.asset_id.toStdString();
        import.accepted_asset_revision=metadata.accepted_revision;
        import.overrides=std::move(overrides);
        host.session.apply({StructuralCommand{MacroCommand{std::move(import)}}},frozen_effect_revision);
        host.edited();
        frozen_effect_revision=host.session.revision();
        expected_revision=frozen_effect_revision;
        frozen_effect_generation=effects_generation_;
        status->setText("Imported and applied Macro Favorite "+ref.asset_id+" at retained revision "+
            QString::number(pin)+" in one Undo step.");
    };
    auto invoke_favorite=[&](const LibraryFavoriteV1& favorite) {
        if(const auto* item_ref=std::get_if<LibraryItemRefV1>(&favorite.target)) {
            if(item_ref->kind=="folder") {
                const auto identity=library.comparison_key(*item_ref);auto* item=node_by_identity.value(identity,nullptr);
                if(!item)throw Error("MISSING_LIBRARY_ROOT","The Favorite folder is no longer registered");
                tree->setCurrentItem(item);tree->scrollToItem(item);
                status->setText("Favorite opened "+display_ref(*item_ref));return;
            }
            place_ref(*item_ref,"linked",frozen_session,expected_revision);
            frozen_effect_revision=host.session.revision();frozen_effect_generation=effects_generation_;
            expected_revision=frozen_effect_revision;return;
        }
        if(const auto* effect=std::get_if<BuiltinEffectTypeRefV1>(&favorite.target)) {
            const auto label=display_target(favorite.target);
            try {
                apply_builtin_effect_favorite(*effect,frozen_effect_session,frozen_effect_target,
                    frozen_effect_revision,frozen_effect_generation);
                frozen_effect_revision=host.session.revision();
                expected_revision=frozen_effect_revision;
                frozen_effect_generation=effects_generation_;
                status->setText("Applied Favorite "+label+" to "+effects_target_->text());
            } catch(const Error& error) {
                status->setText(label+" · "+qs(error.code)+": "+QString::fromUtf8(error.what()));
                throw;
            }
            return;
        }
        if(const auto* macro=std::get_if<MacroAssetRefV1>(&favorite.target)) {
            const auto label=display_target(favorite.target);
            try {
                std::optional<std::uint64_t> pin;
                if(macro_assets->currentData().toString()==macro->asset_id&&macro_pin->currentData().isValid())
                    pin=macro_pin->currentData().toULongLong();
                apply_macro_asset(*macro,pin);
            } catch(const Error& error) {
                status->setText(label+" · "+qs(error.code)+": "+QString::fromUtf8(error.what()));
                throw;
            }
            return;
        }
        const auto& preset=std::get<PresetAssetRefV1>(favorite.target);
        const auto label=display_target(favorite.target);
        try {
            if(host.session_id!=frozen_effect_session)
                throw Error("SESSION_CONFLICT","Preset Favorite belongs to another document");
            if(frozen_effect_generation!=effects_generation_)
                throw Error("REVISION_CONFLICT","Effects panel changed; refresh the target before applying");
            if(canvas->selected_object!=frozen_effect_target)
                throw Error("TARGET_CONFLICT","Preset target changed; choose the current target");
            if(host.session.revision()!=frozen_effect_revision)
                throw Error("REVISION_CONFLICT","Document changed while the Folder Library was open; close and reopen it to choose a current target");
            LibraryPresetAssetV1 metadata;
            auto definition=library.read_preset_asset(preset,&metadata);
            Id fresh_definition_id;
            do {fresh_definition_id=new_id();}
            while(fresh_definition_id==definition.id||QString::fromStdString(fresh_definition_id)==preset.asset_id);
            host.session.apply_preset_command(PresetCommand{ImportAndApplyPreset{std::move(definition),
                fresh_definition_id,frozen_effect_target,new_id(),preset.asset_id.toStdString(),metadata.accepted_revision}},frozen_effect_revision);
            host.edited();
            frozen_effect_revision=host.session.revision();
            expected_revision=frozen_effect_revision;
            frozen_effect_generation=effects_generation_;
            status->setText("Imported and applied Favorite "+label+" to "+effects_target_->text()+" in one Undo step.");
        } catch(const Error& error) {
            status->setText(label+" · "+qs(error.code)+": "+QString::fromUtf8(error.what()));
            throw;
        }
    };
    connect(add_root,&QPushButton::clicked,&dialog,[&,this] {
        const auto path=QFileDialog::getExistingDirectory(&dialog,"Register Folder Library root");if(path.isEmpty())return;
        perform([&]{const auto created=library.register_root(path);rebuild_tree();apply_search(search->text());status->setText("Registered "+created.display_name+". Click Refresh to index this folder and its descendants.");});
    });
    connect(remove_root,&QPushButton::clicked,&dialog,[&,this] {
        const auto* current=tree->currentItem();if(!current)return;
        perform([&]{const auto ref=ref_for_item(current);library.unregister_root(ref.root_id);rebuild_tree();apply_search(search->text());status->setText("Unregistered the folder. Favorites remain as explicit broken references.");});
    });
    connect(refresh_button,&QPushButton::clicked,&dialog,[&,this] {
        perform([&]{library.refresh();rebuild_tree();apply_search(search->text());
            refresh_preset_assets();refresh_macro_assets();refresh_macro_pins();rebuild_favorites();
            sync_preset_controls();sync_macro_controls();
            status->setText("Registered folders, Workspace Presets and Workspace Macros refreshed.");});
    });
    connect(search,&QLineEdit::textChanged,&dialog,[&](const QString& query){apply_search(query);});
    connect(update_preset,&QPushButton::clicked,&dialog,[&,this] {
        perform([&]{
            if(host.session_id!=frozen_effect_session||host.session.revision()!=frozen_effect_revision||
                effects_generation_!=frozen_effect_generation)
                throw Error("REVISION_CONFLICT","Document changed while the Folder Library was open; close and reopen it to choose a current Preset and target");
            const auto asset_id=preset_assets->currentData().toString();
            const auto source_id=source_presets->currentData().toString().toStdString();
            if(asset_id.isEmpty()||source_id.empty())throw Error("NO_SELECTION","Choose both a Workspace asset and a document Preset");
            bool revision_ok=false;
            const auto accepted_revision=preset_assets->currentData(Qt::UserRole+1).toULongLong(&revision_ok);
            const auto expected_hash=preset_assets->currentData(Qt::UserRole+2).toString();
            if(!revision_ok||accepted_revision==0||expected_hash.size()!=64)
                throw Error("UNAVAILABLE_PRESET_ASSET","Refresh the Library and choose an available Preset asset");
            const auto source=host.session.document().preset_definitions.find(source_id);
            if(source==host.session.document().preset_definitions.end())throw Error("MISSING_PRESET",source_id);
            const auto updated=library.update_preset_asset({asset_id},source->second,
                accepted_revision,expected_hash);
            refresh_preset_assets();rebuild_favorites();sync_preset_controls();
            status->setText("Updated Workspace Preset asset “"+updated.label+"” · revision "+QString::number(updated.accepted_revision)+
                ". Existing applications and Favorite identity are unchanged.");
        });
    });
    connect(delete_preset,&QPushButton::clicked,&dialog,[&,this] {
        const auto asset_id=preset_assets->currentData().toString();
        bool revision_ok=false;
        const auto accepted_revision=preset_assets->currentData(Qt::UserRole+1).toULongLong(&revision_ok);
        const auto expected_hash=preset_assets->currentData(Qt::UserRole+2).toString();
        if(asset_id.isEmpty()||!revision_ok||accepted_revision==0||expected_hash.size()!=64) {
            status->setText("Choose an available Workspace Preset asset to delete.");return;
        }
        const auto accepted_asset_id=asset_id;
        if(QMessageBox::question(&dialog,"Delete Workspace Preset",
            "Delete “"+preset_assets->currentText()+"” (AssetID "+accepted_asset_id+", revision "+QString::number(accepted_revision)+")? Existing Favorites will remain as unavailable references.",
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
        perform([&]{
            if(preset_assets->currentData().toString()!=accepted_asset_id||
                preset_assets->currentData(Qt::UserRole+1).toULongLong()!=accepted_revision||
                preset_assets->currentData(Qt::UserRole+2).toString()!=expected_hash)
                throw Error("PRESET_ASSET_SELECTION_CHANGED","Workspace Preset selection changed during delete confirmation; nothing was deleted");
            library.delete_preset_asset({accepted_asset_id},accepted_revision,expected_hash);
            refresh_preset_assets();rebuild_favorites();sync_preset_controls();
            status->setText("Deleted Workspace Preset asset "+accepted_asset_id+". Its Favorites remain as unavailable references.");
        });
    });
    connect(publish_macro,&QPushButton::clicked,&dialog,[&,this] {
        perform([&]{
            if(host.session_id!=frozen_effect_session||host.session.revision()!=frozen_effect_revision||
                effects_generation_!=frozen_effect_generation)
                throw Error("REVISION_CONFLICT","Document changed while the Folder Library was open; close and reopen it to publish a current Macro");
            const auto source_id=source_macros->currentData().toString().toStdString();
            const auto source=host.session.document().macro_definitions.find(source_id);
            if(source_id.empty()||source==host.session.document().macro_definitions.end())
                throw Error("NO_SELECTION","Choose a document Macro to publish");
            const auto created=library.publish_macro_asset(source->second);
            refresh_macro_assets();refresh_macro_pins();rebuild_favorites();sync_macro_controls();
            status->setText("Published Workspace Macro “"+created.label+"” · AssetID "+created.ref.asset_id+
                " · accepted revision "+QString::number(created.accepted_revision)+".");
        });
    });
    connect(update_macro,&QPushButton::clicked,&dialog,[&,this] {
        perform([&]{
            if(host.session_id!=frozen_effect_session||host.session.revision()!=frozen_effect_revision||
                effects_generation_!=frozen_effect_generation)
                throw Error("REVISION_CONFLICT","Document changed while the Folder Library was open; close and reopen it to update a current Macro");
            const auto asset_id=macro_assets->currentData().toString();
            const auto source_id=source_macros->currentData().toString().toStdString();
            if(asset_id.isEmpty()||source_id.empty())throw Error("NO_SELECTION","Choose both a Workspace Macro asset and a document Macro");
            const auto snapshot=macro_asset_snapshots.constFind(asset_id);
            if(snapshot==macro_asset_snapshots.cend()||snapshot->first==0||snapshot->second.size()!=64)
                throw Error("UNAVAILABLE_MACRO_ASSET","Refresh the Library and choose an available Macro asset");
            const auto source=host.session.document().macro_definitions.find(source_id);
            if(source==host.session.document().macro_definitions.end())throw Error("MISSING_MACRO_DEFINITION",source_id);
            const auto updated=library.update_macro_asset({asset_id},source->second,snapshot->first,snapshot->second);
            refresh_macro_assets();refresh_macro_pins();rebuild_favorites();sync_macro_controls();
            status->setText("Updated Workspace Macro asset “"+updated.label+"” · accepted revision "+
                QString::number(updated.accepted_revision)+". Existing imports and Favorite identity are unchanged.");
        });
    });
    connect(delete_macro,&QPushButton::clicked,&dialog,[&] {
        const auto asset_id=macro_assets->currentData().toString();
        const auto snapshot=macro_asset_snapshots.constFind(asset_id);
        if(asset_id.isEmpty()||snapshot==macro_asset_snapshots.cend()||snapshot->first==0||snapshot->second.size()!=64) {
            status->setText("Choose an available Workspace Macro asset to delete.");return;
        }
        const auto accepted_asset_id=asset_id;const auto accepted_revision=static_cast<std::uint64_t>(snapshot->first);
        const auto expected_hash=snapshot->second;
        if(QMessageBox::question(&dialog,"Delete Workspace Macro",
            "Delete “"+macro_assets->currentText()+"” (AssetID "+accepted_asset_id+", accepted revision "+
            QString::number(accepted_revision)+")? Existing Favorites will remain as unavailable references.",
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
        perform([&]{
            const auto current=macro_asset_snapshots.constFind(accepted_asset_id);
            if(macro_assets->currentData().toString()!=accepted_asset_id||current==macro_asset_snapshots.cend()||
                current->first!=accepted_revision||current->second!=expected_hash)
                throw Error("MACRO_ASSET_SELECTION_CHANGED","Workspace Macro selection changed during delete confirmation; nothing was deleted");
            library.delete_macro_asset({accepted_asset_id},accepted_revision,expected_hash);
            refresh_macro_assets();refresh_macro_pins();rebuild_favorites();sync_macro_controls();
            status->setText("Deleted Workspace Macro asset "+accepted_asset_id+". Its Favorites remain as unavailable references.");
        });
    });
    connect(apply_macro,&QPushButton::clicked,&dialog,[&,this] {
        perform([&]{
            bool pin_ok=false;const auto pin=macro_pin->currentData().toULongLong(&pin_ok);
            if(!pin_ok||pin==0)throw Error("MISSING_MACRO_REVISION","Choose an explicit retained Macro revision");
            apply_macro_asset({macro_assets->currentData().toString()},static_cast<std::uint64_t>(pin));
        });
    });
    connect(favorite_macro,&QPushButton::clicked,&dialog,[&] {
        const auto asset_id=macro_assets->currentData().toString();
        if(asset_id.isEmpty()) {status->setText("Choose a Workspace Macro asset to Favorite.");return;}
        perform([&]{
            const auto snapshot=macro_asset_snapshots.constFind(asset_id);
            if(snapshot==macro_asset_snapshots.cend()||snapshot->first==0||snapshot->second.size()!=64)
                throw Error("UNAVAILABLE_MACRO_ASSET","Refresh the Library and choose an available Macro asset");
            LibraryMacroAssetV1 metadata;(void)library.read_macro_asset({asset_id},&metadata);
            if(metadata.accepted_revision!=snapshot->first||metadata.sha256!=snapshot->second)
                throw Error("MACRO_ASSET_SELECTION_CHANGED","Workspace Macro changed since the last explicit Refresh; refresh before Favoriting it");
            const auto created=library.add_favorite(MacroAssetRefV1{asset_id});
            rebuild_favorites();status->setText("Macro Favorite saved: "+display_target(created.target));
        });
    });
    connect(add_favorite,&QPushButton::clicked,&dialog,[&,this] {
        const auto asset_id=preset_assets->currentData().toString();
        const auto* current=tree->currentItem();if(asset_id.isEmpty()&&!current)return;
        perform([&]{
            const auto created=asset_id.isEmpty()?library.add_favorite(ref_for_item(current)):
                library.add_favorite(PresetAssetRefV1{asset_id});
            rebuild_favorites();status->setText("Favorite saved: "+display_target(created.target));
        });
    });
    connect(remove_favorite,&QPushButton::clicked,&dialog,[&,this] {
        const auto* current=favorites->currentItem();if(!current)return;
        perform([&]{library.remove_favorite(current->data(Qt::UserRole).toString());rebuild_favorites();status->setText("Favorite removed.");});
    });
    connect(assign_slot,&QPushButton::clicked,&dialog,[&,this] {
        const auto* current=favorites->currentItem();if(!current)return;
        perform([&]{const auto favorite_id=current->data(Qt::UserRole).toString();library.assign_quick_slot(favorite_id,slot->currentData().toInt());rebuild_favorites();
            status->setText(slot->currentData().toInt()?"Quick Access slot saved.":"Quick Access slot cleared.");});
    });
    connect(place_linked,&QPushButton::clicked,&dialog,[&,this] {
        const auto* current=tree->currentItem();if(!current)return;
        perform([&]{place_ref(ref_for_item(current),"linked",frozen_session,expected_revision);
            frozen_effect_revision=host.session.revision();frozen_effect_generation=effects_generation_;expected_revision=frozen_effect_revision;});
    });
    connect(place_embedded,&QPushButton::clicked,&dialog,[&,this] {
        const auto* current=tree->currentItem();if(!current)return;
        perform([&]{place_ref(ref_for_item(current),"embedded",frozen_session,expected_revision);
            frozen_effect_revision=host.session.revision();frozen_effect_generation=effects_generation_;expected_revision=frozen_effect_revision;});
    });
    connect(use_favorite,&QPushButton::clicked,&dialog,[&,this] {
        const auto* current=favorites->currentItem();if(!current)return;
        perform([&]{
            const auto favorite=std::find_if(library.favorites().begin(),library.favorites().end(),[&](const auto& value) {
                return value.favorite_id==current->data(Qt::UserRole).toString();
            });
            if(favorite==library.favorites().end())throw Error("MISSING_FAVORITE","The selected Favorite no longer exists");
            invoke_favorite(*favorite);
        });
    });
    connect(use_slot,&QPushButton::clicked,&dialog,[&,this] {
        perform([&]{const auto favorite=library.favorite_for_slot(slot->currentData().toInt());
            if(!favorite)throw Error("EMPTY_QUICK_SLOT","Choose a Quick Access slot that has a Favorite");
            invoke_favorite(*favorite);
        });
    });
    connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);
    rebuild_tree();apply_search({});dialog.exec();
}
void Window::add_text_properties(QVBoxLayout* layout,const Object& object) {
    const auto id=object.id;const auto frozen_session=host.session_id;
    const auto& source=*object.text;
    auto* box=new QGroupBox("Text source");auto* form=new QFormLayout(box);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(box);
    auto* preview=new QLabel(qs(evaluate_text_content(host.session.document(),id)).left(160));preview->setWordWrap(true);preview->setTextFormat(Qt::PlainText);
    preview->setObjectName("text-preview");form->addRow(preview);
    const Ref content_ref{id,"","text.content"};const auto content_state=text_content_property(host.session.document(),content_ref);
    const auto content_revision=host.session.revision();
    auto* content_row=new QWidget(box);auto* content_layout=new QHBoxLayout(content_row);content_layout->setContentsMargins(0,0,0,0);
    auto* edit=new QPushButton("Edit text…");edit->setObjectName("edit-text-content");content_layout->addWidget(edit);
    auto* content_driver_button=new QToolButton(content_row);content_driver_button->setObjectName("text-content-driver");
    content_driver_button->setText(content_state.driver?"Driver…":"Drive…");content_driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* content_menu=new QMenu(content_driver_button);content_driver_button->setMenu(content_menu);content_layout->addWidget(content_driver_button);
    auto* link_content=content_menu->addAction("Link to Text content…");
    auto* unlink_content=content_menu->addAction("Unlink content");unlink_content->setEnabled(content_state.driver.has_value());
    std::vector<Id> content_source_ids;
    for(const auto& [source_id,source_object]:host.session.document().objects)if(source_object.kind==Kind::text&&source_object.text&&source_id!=id) {
        content_source_ids.push_back(source_id);
    }
    link_content->setEnabled(!content_source_ids.empty());
    const bool replace_content_driver=content_state.driver.has_value();
    connect(link_content,&QAction::triggered,this,[this,id,frozen_session,content_revision,replace_content_driver,content_source_ids]{
        const auto target=Ref{id,"","text.content"};const auto selection=canvas->selections();
        const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
        auto picker=make_text_source_picker(this,host.session.document(),id,target.field,content_source_ids,"Link Text content");
        auto* dialog=picker.dialog;auto* list=picker.list;auto* status=picker.status;
        connect(list,&QListWidget::currentItemChanged,this,[this,frozen_session](QListWidgetItem* item,QListWidgetItem*){
            if(!item||item->isHidden()||host.session_id!=frozen_session)return;
            const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
            if(host.session.document().objects.contains(source.object))canvas->set_selection(source.object,{});
        });
        connect(dialog,&QDialog::rejected,this,[this,selection,frozen_session,composition,artboard]{
            if(host.session_id==frozen_session){perform([&]{canvas->set_active_artboard(composition,artboard,false);});canvas->set_selections(selection);}
        });
        connect(picker.buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,dialog,
            [this,dialog,list,status,target,frozen_session,content_revision,replace_content_driver,selection,composition,artboard]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text source chooser belongs to a different document");
                    auto* item=list->currentItem();
                    if(!item||item->isHidden())throw Error("NO_SOURCE","Choose a visible Text source");
                    const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
                    if(source.field!=target.field||!source.point.empty()||source.object==target.object)
                        throw Error("INVALID_REFERENCE","Choose a different Text with the same property");
                    if(host.session.revision()!=content_revision)throw Error("REVISION_CONFLICT","Text changed while the source chooser was open");
                    host.session.apply({LinkTextContent{target,source,replace_content_driver}},content_revision);
                    canvas->set_active_artboard(composition,artboard,false);canvas->set_selections(selection);host.edited();dialog->accept();
                } catch(const Error& error){status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
                catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog->show();picker.search->setFocus();
    });
    connect(unlink_content,&QAction::triggered,this,[this,id,frozen_session,content_revision]{
        perform([&]{if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
            host.session.apply({UnlinkTextContent{{id,"","text.content"}}},content_revision);host.edited();});
    });
    form->addRow("Content",content_row);
    const auto content_driver_name=[this](const std::optional<TextContentDriver>& driver) {
        if(!driver)return QString("none");
        const auto found=host.session.document().objects.find(driver->link.object);
        return QString("link to ")+(found==host.session.document().objects.end()?qs(driver->link.object):qs(found->second.name)+" ("+qs(driver->link.object)+")");
    };
    auto* content_status=new QLabel(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
        .arg(qs(content_state.literal).left(80),content_driver_name(content_state.driver),qs(content_state.evaluated).left(80)));
    content_status->setObjectName("text-content-state");content_status->setWordWrap(true);content_status->setTextFormat(Qt::PlainText);form->addRow("",content_status);
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
    auto* path_group=new QGroupBox("Text on Path",box);auto* path_form=new QFormLayout(path_group);
    path_form->setRowWrapPolicy(QFormLayout::WrapLongRows);form->addRow(path_group);
    auto* path_contour=new QComboBox(path_group);path_contour->setObjectName("text-path-contour");
    path_contour->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);path_contour->setMinimumContentsLength(20);
    path_contour->addItem("Choose an authored Path contour…",QString{});
    const auto& path_composition=find_composition(host.session.document(),canvas->active_composition());
    std::function<void(const Id&)> add_path_choices=[&](const Id& path_id) {
        const auto& candidate=host.session.document().objects.at(path_id);
        if(candidate.kind==Kind::path&&!candidate.source)for(const auto& contour:candidate.contours) {
            const auto data=qs(path_id)+"\n"+qs(contour.id);
            path_contour->addItem(qs(candidate.name)+" ["+qs(path_id)+"] · Contour "+qs(contour.id),data);
            if(source.path_attachment&&source.path_attachment->path==path_id&&source.path_attachment->contour==contour.id)
                path_contour->setCurrentIndex(path_contour->count()-1);
        }
        for(const auto& child:candidate.children)add_path_choices(child);
    };
    for(const auto& root:path_composition.roots)add_path_choices(root);
    path_form->addRow("Source contour",path_contour);
    auto* path_start_mode=new QComboBox(path_group);path_start_mode->setObjectName("text-path-start-mode");
    path_start_mode->addItem("Distance (du96)","distance");path_start_mode->addItem("Normalized fraction","normalized");
    if(source.path_attachment)path_start_mode->setCurrentIndex(source.path_attachment->start_mode=="normalized"?1:0);
    path_form->addRow("Start mode",path_start_mode);
    auto* path_start=new QLineEdit(path_group);path_start->setObjectName("text-path-start");
    path_start->setText(QString::number(source.path_attachment?source.path_attachment->start:0,'g',17));
    path_start->setPlaceholderText("Finite distance or fraction");path_form->addRow("Start",path_start);
    auto* path_spacing=new QLineEdit(path_group);path_spacing->setObjectName("text-path-spacing");
    path_spacing->setText(QString::number(source.path_attachment?source.path_attachment->spacing:0,'g',17));
    path_spacing->setPlaceholderText("Nonnegative du96 gap");path_form->addRow("Extra spacing",path_spacing);
    auto* path_reversed=new QCheckBox("Reverse contour traversal",path_group);path_reversed->setObjectName("text-path-reversed");
    path_reversed->setChecked(source.path_attachment&&source.path_attachment->reversed);path_form->addRow(path_reversed);
    auto* path_actions=new QWidget(path_group);auto* path_action_row=new QHBoxLayout(path_actions);path_action_row->setContentsMargins(0,0,0,0);
    auto* path_apply=new QPushButton("Attach / update",path_actions);path_apply->setObjectName("text-path-apply");path_action_row->addWidget(path_apply);
    auto* path_detach=new QPushButton("Detach",path_actions);path_detach->setObjectName("text-path-detach");
    path_detach->setEnabled(source.path_attachment.has_value());path_action_row->addWidget(path_detach);path_action_row->addStretch();
    path_form->addRow(path_actions);
    auto* path_status=new QLabel(source.path_attachment?
        "Attached by stable Path and Contour IDs. Text remains editable; projection uses the shared Canvas and SVG shape flow.":
        "Detached. Choose a specific same-Composition authored Path contour to attach.",path_group);
    path_status->setObjectName("text-path-status");path_status->setWordWrap(true);path_status->setTextFormat(Qt::PlainText);path_form->addRow(path_status);
    auto refresh_path_inspector=[this,id,frozen_session]{QTimer::singleShot(0,this,[this,id,frozen_session]{
        if(host.session_id==frozen_session&&canvas->selected_object==id)rebuild_inspector(true);
    });};
    connect(path_apply,&QPushButton::clicked,this,[this,path_contour,path_start_mode,path_start,path_spacing,path_reversed,update,refresh_path_inspector]{perform([&]{
        const auto parts=path_contour->currentData().toString().split('\n');
        if(parts.size()!=2||parts[0].isEmpty()||parts[1].isEmpty())throw Error("MISSING_PATH_ATTACHMENT","Choose an authored Path contour by ID");
        bool start_ok=false,spacing_ok=false;
        const auto start=path_start->text().trimmed().toDouble(&start_ok);
        const auto spacing=path_spacing->text().trimmed().toDouble(&spacing_ok);
        if(!start_ok||!std::isfinite(start))throw Error("TEXT_PATH_START_INVALID","Enter a finite Text-on-Path start value");
        if(!spacing_ok||!std::isfinite(spacing)||spacing<0)throw Error("TEXT_PATH_SPACING","Enter finite nonnegative extra spacing");
        update([&](TextSource& next){next.path_attachment=TextPathAttachment{parts[0].toStdString(),parts[1].toStdString(),
            path_start_mode->currentData().toString().toStdString(),start,spacing,path_reversed->isChecked()};});
    });});
    connect(path_detach,&QPushButton::clicked,this,[this,update,refresh_path_inspector]{perform([&]{update([](TextSource& next){next.path_attachment.reset();});refresh_path_inspector();});});
    if(!font_families_) {
        font_families_=new QStringListModel(this);
        auto reload=[this]{QStringList names;for(const auto& name:text_fonts())names<<qs(name);font_families_->setStringList(names);};
        reload();connect(qApp,&QGuiApplication::fontDatabaseChanged,this,[this,reload]{perform(reload);});
    }
    const Ref family_ref{id,"","text.family"};const auto family_state=text_family_property(host.session.document(),family_ref);
    const auto family_revision=host.session.revision();
    auto* family_row=new QWidget(box);auto* family_layout=new QHBoxLayout(family_row);family_layout->setContentsMargins(0,0,0,0);
    auto* family=new FontFamilyCombo(font_families_);family->setObjectName("text-family");
    family->addItem(qs(family_state.driver?family_state.evaluated:family_state.literal));
    family->setCurrentText(qs(family_state.driver?family_state.evaluated:family_state.literal));
    family->setEnabled(!family_state.driver);family_layout->addWidget(family);
    auto* family_driver_button=new QToolButton(family_row);family_driver_button->setObjectName("text-family-driver");
    family_driver_button->setText(family_state.driver?"Driver…":"Drive…");family_driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* family_menu=new QMenu(family_driver_button);family_driver_button->setMenu(family_menu);family_layout->addWidget(family_driver_button);
    auto* link_family=family_menu->addAction("Link to Text family…");
    auto* edit_linked_family=family_menu->addAction("Unlink and edit family…");edit_linked_family->setEnabled(family_state.driver.has_value());
    std::vector<Id> family_source_ids;
    for(const auto& [source_id,source_object]:host.session.document().objects)if(source_object.kind==Kind::text&&source_object.text&&source_id!=id) {
        family_source_ids.push_back(source_id);
    }
    link_family->setEnabled(!family_source_ids.empty());
    const bool replace_family_driver=family_state.driver.has_value();
    connect(link_family,&QAction::triggered,this,[this,id,frozen_session,family_revision,replace_family_driver,family_source_ids]{
        const auto target=Ref{id,"","text.family"};const auto selection=canvas->selections();
        const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
        auto picker=make_text_source_picker(this,host.session.document(),id,target.field,family_source_ids,"Link Text family");
        auto* dialog=picker.dialog;auto* list=picker.list;auto* status=picker.status;
        connect(list,&QListWidget::currentItemChanged,this,[this,frozen_session](QListWidgetItem* item,QListWidgetItem*){
            if(!item||item->isHidden()||host.session_id!=frozen_session)return;
            const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
            if(host.session.document().objects.contains(source.object))canvas->set_selection(source.object,{});
        });
        connect(dialog,&QDialog::rejected,this,[this,selection,frozen_session,composition,artboard]{
            if(host.session_id==frozen_session){perform([&]{canvas->set_active_artboard(composition,artboard,false);});canvas->set_selections(selection);}
        });
        connect(picker.buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,dialog,
            [this,dialog,list,status,target,frozen_session,family_revision,replace_family_driver,selection,composition,artboard]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text source chooser belongs to a different document");
                    auto* item=list->currentItem();
                    if(!item||item->isHidden())throw Error("NO_SOURCE","Choose a visible Text source");
                    const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
                    if(source.field!=target.field||!source.point.empty()||source.object==target.object)
                        throw Error("INVALID_REFERENCE","Choose a different Text with the same property");
                    if(host.session.revision()!=family_revision)throw Error("REVISION_CONFLICT","Text changed while the source chooser was open");
                    host.session.apply({LinkTextFamily{target,source,replace_family_driver}},family_revision);
                    canvas->set_active_artboard(composition,artboard,false);canvas->set_selections(selection);host.edited();dialog->accept();
                } catch(const Error& error){status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
                catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog->show();picker.search->setFocus();
    });
    connect(edit_linked_family,&QAction::triggered,this,[this,id,frozen_session,family_revision,family_state]{
        QDialog dialog(this);dialog.setObjectName("text-family-dialog");dialog.setWindowTitle("Edit linked font family");
        auto* box_layout=new QVBoxLayout(&dialog);
        auto* editor=new FontFamilyCombo(font_families_);editor->setObjectName("text-family-editor");
        editor->addItem(qs(family_state.evaluated));editor->setCurrentText(qs(family_state.evaluated));editor->setEnabled(false);box_layout->addWidget(editor);
        auto* unlink=new QCheckBox("Unlink the driver and edit this font family",&dialog);unlink->setObjectName("unlink-text-family-driver");box_layout->addWidget(unlink);
        auto* status=new QLabel("Apply commits the unlink and family edit together. Cancel keeps the current driver.",&dialog);
        status->setObjectName("text-family-editor-status");status->setWordWrap(true);box_layout->addWidget(status);
        connect(unlink,&QCheckBox::toggled,editor,&QWidget::setEnabled);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);box_layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[this,&dialog,id,frozen_session,family_revision,family_state,editor,unlink,status]{
            try {
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
                if(host.session.revision()!=family_revision)throw Error("STALE_CONTEXT","Text changed while the family editor was open; copy this value and reopen the editor");
                const auto found=host.session.document().objects.find(id);
                if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","Text no longer exists");
                auto commands=std::vector<Command>{UnlinkTextFamily{{id,"","text.family"}}};
                const auto value=editor->currentText().toStdString();
                if(value!=family_state.evaluated) {
                    auto next=*found->second.text;next.family=value;next.family_driver.reset();
                    commands.push_back(UpdateText{id,std::move(next)});
                }
                if(!unlink->isChecked())throw Error("DRIVEN_PROPERTY","Select the unlink option before applying a family edit");
                host.session.apply(commands,family_revision);host.edited();dialog.accept();
            } catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
        });
        dialog.exec();
    });
    form->addRow("Font family",family_row);
    connect(family->lineEdit(),&QLineEdit::editingFinished,this,[this,family,update,before=family_state.literal,linked=family_state.driver.has_value()]{
        if(linked)return;
        const auto value=family->currentText().toStdString();if(value!=before)perform([&]{update([&](auto& s){s.family=value;});});});
    connect(family,QOverload<int>::of(&QComboBox::activated),this,[this,family,update]{perform([&]{update([&](auto& s){s.family=family->currentText().toStdString();});});});
    const auto family_driver_name=[this](const std::optional<TextFamilyDriver>& driver) {
        if(!driver)return QString("none");
        const auto found=host.session.document().objects.find(driver->link.object);
        return QString("link to ")+(found==host.session.document().objects.end()?qs(driver->link.object):qs(found->second.name)+" ("+qs(driver->link.object)+")");
    };
    auto* family_status=new QLabel(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
        .arg(qs(family_state.literal),family_driver_name(family_state.driver),qs(family_state.evaluated)));
    family_status->setObjectName("text-family-state");family_status->setWordWrap(true);family_status->setTextFormat(Qt::PlainText);form->addRow("",family_status);
    const Ref weight_ref{id,"","text.weight"};const auto weight_state=text_weight_property(host.session.document(),weight_ref);
    const auto weight_revision=host.session.revision();
    auto* weight_row=new QWidget(box);auto* weight_layout=new QHBoxLayout(weight_row);weight_layout->setContentsMargins(0,0,0,0);
    auto* weight=new QSpinBox;weight->setObjectName("text-weight");weight->setRange(1,999);weight->setSingleStep(100);
    weight->setValue(static_cast<int>(weight_state.evaluated));weight->setKeyboardTracking(false);
    const bool weight_is_driven=weight_state.driver.has_value()||weight_state.expression.has_value();
    weight->setEnabled(!weight_is_driven);
    if(weight_is_driven)weight->setToolTip("Unlink the source before editing the authored weight.");
    weight_layout->addWidget(weight);
    auto* weight_driver_button=new QToolButton(weight_row);weight_driver_button->setObjectName("text-weight-driver");
    weight_driver_button->setText(weight_is_driven?"Source…":"Drive…");weight_driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* weight_menu=new QMenu(weight_driver_button);weight_driver_button->setMenu(weight_menu);weight_layout->addWidget(weight_driver_button);
    auto* link_weight=weight_menu->addAction("Link to Text weight…");
    auto* expression_weight=weight_menu->addAction("Set expression…");
    auto* unlink_weight=weight_menu->addAction("Unlink weight");unlink_weight->setEnabled(weight_is_driven);
    std::vector<Id> weight_source_ids;
    for(const auto& [source_id,source_object]:host.session.document().objects)if(source_object.kind==Kind::text&&source_object.text&&source_id!=id) {
        weight_source_ids.push_back(source_id);
    }
    link_weight->setEnabled(!weight_source_ids.empty());
    const bool replace_weight_driver=weight_is_driven;
    connect(link_weight,&QAction::triggered,this,[this,id,frozen_session,weight_revision,replace_weight_driver,weight_source_ids]{
        const auto target=Ref{id,"","text.weight"};const auto selection=canvas->selections();
        const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
        auto picker=make_text_source_picker(this,host.session.document(),id,target.field,weight_source_ids,"Link Text weight");
        auto* dialog=picker.dialog;auto* list=picker.list;auto* status=picker.status;
        connect(list,&QListWidget::currentItemChanged,this,[this,frozen_session](QListWidgetItem* item,QListWidgetItem*){
            if(!item||item->isHidden()||host.session_id!=frozen_session)return;
            const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
            if(host.session.document().objects.contains(source.object))canvas->set_selection(source.object,{});
        });
        connect(dialog,&QDialog::rejected,this,[this,selection,frozen_session,composition,artboard]{
            if(host.session_id==frozen_session){perform([&]{canvas->set_active_artboard(composition,artboard,false);});canvas->set_selections(selection);}
        });
        connect(picker.buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,dialog,
            [this,dialog,list,status,target,frozen_session,weight_revision,replace_weight_driver,selection,composition,artboard]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text source chooser belongs to a different document");
                    auto* item=list->currentItem();
                    if(!item||item->isHidden())throw Error("NO_SOURCE","Choose a visible Text source");
                    const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
                    if(source.field!=target.field||!source.point.empty()||source.object==target.object)
                        throw Error("INVALID_REFERENCE","Choose a different Text with the same property");
                    if(host.session.revision()!=weight_revision)throw Error("REVISION_CONFLICT","Text changed while the source chooser was open");
                    host.session.apply({LinkTextWeight{target,source,replace_weight_driver}},weight_revision);
                    canvas->set_active_artboard(composition,artboard,false);canvas->set_selections(selection);host.edited();dialog->accept();
                } catch(const Error& error){status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
                catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog->show();picker.search->setFocus();
    });
    connect(expression_weight,&QAction::triggered,this,[this,id,frozen_session,weight_revision,weight_state,weight_is_driven]{
        QDialog dialog(this);dialog.setObjectName("text-weight-expression-dialog");dialog.setWindowTitle("Text weight expression");
        auto* layout=new QVBoxLayout(&dialog);
        auto* editor=new ExpressionInput;editor->setObjectName("text-weight-expression-draft");
        editor->setAccessibleName("Text weight expression draft");editor->setFixedHeight(68);
        editor->setPlaceholderText("Unitless expression using ref(\"Text ID\",\"\",\"text.weight\")");
        if(weight_state.expression)editor->setPlainText(qs(weight_state.expression->source));
        layout->addWidget(editor);
        auto* replace=new QCheckBox("Replace the current weight source",&dialog);
        replace->setObjectName("text-weight-expression-replace");replace->setVisible(weight_is_driven);
        layout->addWidget(replace);
        auto* status=new QLabel("Apply commits; Cancel keeps the current weight source.",&dialog);
        status->setObjectName("text-weight-expression-status");status->setWordWrap(true);layout->addWidget(status);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);
        layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
            [this,&dialog,id,frozen_session,weight_revision,editor,replace,status]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
                    host.session.apply({SetTextWeightExpression{{id,"","text.weight"},
                        Expression{editor->toPlainText().toStdString(),1},replace->isChecked()}},weight_revision);
                    host.edited();dialog.accept();
                } catch(const Error& error){status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
                catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog.exec();
    });
    connect(unlink_weight,&QAction::triggered,this,[this,id,frozen_session,weight_revision]{
        perform([&]{if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
            host.session.apply({UnlinkTextWeight{{id,"","text.weight"}}},weight_revision);host.edited();});
    });
    form->addRow("Weight",weight_row);
    auto* weight_status=new QLabel(weight_row);weight_status->setObjectName("text-weight-state");
    QString weight_driver_description="none";
    if(weight_state.driver) {
        const auto& link=weight_state.driver->link;const auto found=host.session.document().objects.find(link.object);
        const auto offset=weight_state.driver->offset;
        const auto signed_offset=(offset>=0?QString("+"):QString())+QString::number(static_cast<qlonglong>(offset));
        weight_driver_description="link to "+(found==host.session.document().objects.end()?qs(link.object):qs(found->second.name)+" ("+qs(link.object)+")")+
            " · offset "+signed_offset;
    }
    else if(weight_state.expression)weight_driver_description="expression · "+qs(weight_state.expression->source);
    weight_status->setText(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
        .arg(weight_state.literal).arg(weight_driver_description).arg(weight_state.evaluated));
    weight_status->setWordWrap(true);form->addRow("",weight_status);
    connect(weight,&QSpinBox::editingFinished,this,[this,weight,update,weight_state]{
        if(weight_state.driver||weight_state.expression)return;
        if(static_cast<unsigned>(weight->value())!=weight_state.literal)perform([&]{update([&](auto& s){s.weight=static_cast<unsigned>(weight->value());});});});
    const Ref italic_ref{id,"","text.italic"};const auto italic_state=text_italic_property(host.session.document(),italic_ref);
    const auto italic_revision=host.session.revision();
    auto* italic_row=new QWidget(box);auto* italic_layout=new QHBoxLayout(italic_row);italic_layout->setContentsMargins(0,0,0,0);
    auto* italic=new QCheckBox("Italic");italic->setObjectName("text-italic");italic->setChecked(italic_state.evaluated);
    italic->setEnabled(!italic_state.driver);if(italic_state.driver)italic->setToolTip("Unlink or replace the driver before editing the literal.");
    italic_layout->addWidget(italic);
    auto* italic_driver_button=new QToolButton(italic_row);italic_driver_button->setObjectName("text-italic-driver");
    italic_driver_button->setText(italic_state.driver?"Driver…":"Drive…");italic_driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* italic_menu=new QMenu(italic_driver_button);italic_driver_button->setMenu(italic_menu);italic_layout->addWidget(italic_driver_button);italic_layout->addStretch();
    auto* link_italic=italic_menu->addAction("Link to Text italic…");
    auto* expression_italic=italic_menu->addAction("Set expression…");
    auto* unlink_italic=italic_menu->addAction("Unlink italic");unlink_italic->setEnabled(italic_state.driver.has_value());
    std::vector<Id> italic_source_ids;
    for(const auto& [source_id,source_object]:host.session.document().objects)if(source_object.kind==Kind::text&&source_object.text&&source_id!=id) {
        italic_source_ids.push_back(source_id);
    }
    link_italic->setEnabled(!italic_source_ids.empty());
    const bool replace_italic_driver=italic_state.driver.has_value();
    connect(link_italic,&QAction::triggered,this,[this,id,frozen_session,italic_revision,replace_italic_driver,italic_source_ids]{
        const auto target=Ref{id,"","text.italic"};const auto selection=canvas->selections();
        const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
        auto picker=make_text_source_picker(this,host.session.document(),id,target.field,italic_source_ids,"Link Text italic");
        auto* dialog=picker.dialog;auto* list=picker.list;auto* status=picker.status;
        connect(list,&QListWidget::currentItemChanged,this,[this,frozen_session](QListWidgetItem* item,QListWidgetItem*){
            if(!item||item->isHidden()||host.session_id!=frozen_session)return;
            const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
            if(host.session.document().objects.contains(source.object))canvas->set_selection(source.object,{});
        });
        connect(dialog,&QDialog::rejected,this,[this,selection,frozen_session,composition,artboard]{
            if(host.session_id==frozen_session){perform([&]{canvas->set_active_artboard(composition,artboard,false);});canvas->set_selections(selection);}
        });
        connect(picker.buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,dialog,
            [this,dialog,list,status,target,frozen_session,italic_revision,replace_italic_driver,selection,composition,artboard]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text source chooser belongs to a different document");
                    auto* item=list->currentItem();
                    if(!item||item->isHidden())throw Error("NO_SOURCE","Choose a visible Text source");
                    const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
                    if(source.field!=target.field||!source.point.empty()||source.object==target.object)
                        throw Error("INVALID_REFERENCE","Choose a different Text with the same property");
                    if(host.session.revision()!=italic_revision)throw Error("REVISION_CONFLICT","Text changed while the source chooser was open");
                    host.session.apply({LinkTextItalic{target,source,replace_italic_driver}},italic_revision);
                    canvas->set_active_artboard(composition,artboard,false);canvas->set_selections(selection);host.edited();dialog->accept();
                } catch(const Error& error){status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
                catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog->show();picker.search->setFocus();
    });
    const auto initial_italic_expression=italic_state.driver&&std::holds_alternative<Expression>(*italic_state.driver)?
        qs(std::get<Expression>(*italic_state.driver).source):QStringLiteral("false");
    connect(expression_italic,&QAction::triggered,this,[this,id,frozen_session,italic_revision,replace_italic_driver,initial_italic_expression]{
        bool accepted=false;const auto expression=QInputDialog::getText(this,"Text italic expression","Expression",
            QLineEdit::Normal,initial_italic_expression,&accepted);if(!accepted)return;
        perform([&]{if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
            host.session.apply({SetTextItalicExpression{{id,"","text.italic"},{expression.toStdString(),1},replace_italic_driver}},italic_revision);host.edited();});
    });
    connect(unlink_italic,&QAction::triggered,this,[this,id,frozen_session,italic_revision]{
        perform([&]{if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
            host.session.apply({UnlinkTextItalic{{id,"","text.italic"}}},italic_revision);host.edited();});
    });
    form->addRow("Italic",italic_row);
    auto* italic_status=new QLabel(italic_row);italic_status->setObjectName("text-italic-state");
    const auto driver_description=[&] {
        if(!italic_state.driver)return QStringLiteral("none");
        if(const auto* link=std::get_if<Ref>(&*italic_state.driver)) {
            const auto found=host.session.document().objects.find(link->object);
            return QStringLiteral("link to ")+(found==host.session.document().objects.end()?qs(link->object):qs(found->second.name)+" ("+qs(link->object)+")");
        }
        return QStringLiteral("expression ")+qs(std::get<Expression>(*italic_state.driver).source);
    }();
    italic_status->setText(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
        .arg(italic_state.literal?"true":"false",driver_description,italic_state.evaluated?"true":"false"));
    italic_status->setWordWrap(true);form->addRow("",italic_status);
    connect(italic,&QCheckBox::toggled,this,[this,update,italic_state](bool value){if(italic_state.driver)return;
        perform([&]{update([&](auto& s){s.italic=value;});});});
    auto choices=[&](const QString& name,const QString& label,const QStringList& labels,const std::vector<std::string>& values,
                     const std::string& selected,std::string TextSource::*member) {
        auto* combo=new QComboBox;combo->setObjectName(name);combo->addItems(labels);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setMinimumContentsLength(9);
        combo->setCurrentIndex(static_cast<int>(std::distance(values.begin(),std::find(values.begin(),values.end(),selected))));form->addRow(label,combo);
        connect(combo,&QComboBox::currentIndexChanged,this,[this,update,values,member](int index){
            perform([&]{update([&](auto& s){s.*member=values.at(static_cast<std::size_t>(index));});});});
    };
    const Ref layout_ref{id,"","text.layout"};const auto layout_state=text_layout_property(host.session.document(),layout_ref);
    const auto layout_revision=host.session.revision();
    auto* layout_row=new QWidget(box);auto* layout_row_layout=new QHBoxLayout(layout_row);layout_row_layout->setContentsMargins(0,0,0,0);
    auto* layout_choice=new QComboBox;layout_choice->setObjectName("text-layout");layout_choice->addItems({"Auto size","Fixed frame"});
    layout_choice->setCurrentIndex(layout_state.evaluated=="frame"?1:0);layout_choice->setEnabled(false);
    layout_choice->setToolTip("Use Edit sizing to stage and apply a change.");layout_row_layout->addWidget(layout_choice);
    auto* layout_driver_button=new QToolButton(layout_row);layout_driver_button->setObjectName("text-layout-driver");
    layout_driver_button->setText(layout_state.driver?"Driver…":"Drive…");layout_driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* layout_menu=new QMenu(layout_driver_button);layout_driver_button->setMenu(layout_menu);layout_row_layout->addWidget(layout_driver_button);
    auto* edit_layout=layout_menu->addAction("Edit sizing…");
    auto* link_layout=layout_menu->addAction("Link to Text sizing…");
    auto* unlink_layout=layout_menu->addAction("Unlink sizing");unlink_layout->setEnabled(layout_state.driver.has_value());
    connect(edit_layout,&QAction::triggered,this,[this,id,frozen_session,layout_revision,layout_ref,layout_state]{
        QDialog dialog(this);dialog.setObjectName("text-layout-dialog");dialog.setWindowTitle("Edit Text sizing");
        auto* box_layout=new QVBoxLayout(&dialog);
        auto* editor=new QComboBox(&dialog);editor->setObjectName("text-layout-editor");editor->addItems({"Auto size","Fixed frame"});
        editor->setCurrentIndex(layout_state.evaluated=="frame"?1:0);editor->setEnabled(!layout_state.driver);box_layout->addWidget(editor);
        auto* unlink=new QCheckBox("Unlink the driver and edit this sizing mode",&dialog);
        unlink->setObjectName("unlink-text-layout-driver");unlink->setVisible(layout_state.driver.has_value());box_layout->addWidget(unlink);
        auto* status=new QLabel("Apply commits the sizing mode. Cancel keeps the current mode.",&dialog);
        status->setObjectName("text-layout-editor-status");status->setWordWrap(true);box_layout->addWidget(status);
        connect(unlink,&QCheckBox::toggled,editor,&QWidget::setEnabled);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);box_layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[this,&dialog,id,frozen_session,layout_revision,layout_ref,layout_state,editor,unlink,status]{
            try {
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
                if(host.session.revision()!=layout_revision)throw Error("STALE_CONTEXT","Text changed while the sizing editor was open; reopen it");
                if(layout_state.driver&&!unlink->isChecked())throw Error("DRIVEN_PROPERTY","Select the unlink option before applying a sizing edit");
                const auto found=host.session.document().objects.find(id);
                if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","Text no longer exists");
                const auto value=editor->currentIndex()==1?std::string("frame"):std::string("auto");
                std::vector<Command> commands;
                if(layout_state.driver)commands.push_back(UnlinkTextLayout{layout_ref});
                if(value!=(layout_state.driver?layout_state.evaluated:layout_state.literal)) {
                    auto next=*found->second.text;next.layout=value;next.layout_driver.reset();
                    commands.push_back(UpdateText{id,std::move(next)});
                }
                if(!commands.empty()){host.session.apply(commands,layout_revision);host.edited();}
                dialog.accept();
            } catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
        });
        dialog.exec();
    });
    QStringList layout_source_labels;std::vector<Id> layout_source_ids;
    for(const auto& [source_id,source_object]:host.session.document().objects)if(source_object.kind==Kind::text&&source_object.text&&source_id!=id) {
        layout_source_ids.push_back(source_id);layout_source_labels<<qs(source_object.name)+" — "+qs(source_id);
    }
    link_layout->setEnabled(!layout_source_ids.empty());const bool replace_layout_driver=layout_state.driver.has_value();
    connect(link_layout,&QAction::triggered,this,[this,id,frozen_session,layout_revision,replace_layout_driver,layout_source_ids,layout_source_labels]{
        QDialog dialog(this);dialog.setObjectName("text-layout-source-dialog");dialog.setWindowTitle("Link Text sizing");
        auto* layout=new QVBoxLayout(&dialog);
        auto* search=new QLineEdit(&dialog);search->setObjectName("text-layout-source-search");
        search->setPlaceholderText("Search Text name, object ID or text.layout");layout->addWidget(search);
        auto* source=new QComboBox(&dialog);source->setObjectName("text-layout-source");
        for(int i=0;i<layout_source_labels.size();++i)source->addItem(layout_source_labels.at(i),i);
        source->setCurrentIndex(-1);layout->addWidget(source);
        auto* status=new QLabel("Choose a visible Text sizing source. Cancel keeps the current driver.",&dialog);
        status->setObjectName("text-layout-source-status");status->setWordWrap(true);layout->addWidget(status);
        connect(search,&QLineEdit::textChanged,&dialog,[source,layout_source_labels,layout_source_ids](const QString& query){
            const int selected=source->currentIndex()<0?-1:source->currentData().toInt();
            const QSignalBlocker blocker(source);source->clear();
            for(int i=0;i<layout_source_labels.size();++i){
                const auto path=qs(layout_source_ids.at(static_cast<std::size_t>(i)))+" / text.layout";
                if(layout_source_labels.at(i).contains(query,Qt::CaseInsensitive)||path.contains(query,Qt::CaseInsensitive))
                    source->addItem(layout_source_labels.at(i),i);
            }
            source->setCurrentIndex(selected<0?-1:source->findData(selected));
        });
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
            [this,&dialog,id,frozen_session,layout_revision,replace_layout_driver,layout_source_ids,source,status]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
                    if(host.session.revision()!=layout_revision)throw Error("STALE_CONTEXT","Text changed while the source chooser was open; reopen it");
                    if(source->currentIndex()<0)throw Error("MISSING_REFERENCE","Choose a visible Text sizing source");
                    const int index=source->currentData().toInt();
                    if(index<0||static_cast<std::size_t>(index)>=layout_source_ids.size())
                        throw Error("MISSING_REFERENCE","Choose a valid Text sizing source");
                    host.session.apply({LinkTextLayout{{id,"","text.layout"},
                        {layout_source_ids.at(static_cast<std::size_t>(index)),"","text.layout"},replace_layout_driver}},layout_revision);
                    host.edited();dialog.accept();
                } catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog.exec();
    });
    connect(unlink_layout,&QAction::triggered,this,[this,frozen_session,layout_revision,layout_ref]{
        perform([&]{if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
            host.session.apply({UnlinkTextLayout{layout_ref}},layout_revision);host.edited();});
    });
    layout_row_layout->addStretch();form->addRow("Sizing",layout_row);
    const auto layout_driver_name=[this](const std::optional<TextLayoutDriver>& driver) {
        if(!driver)return QString("none");
        const auto found=host.session.document().objects.find(driver->link.object);
        return QString("link to ")+(found==host.session.document().objects.end()?qs(driver->link.object):qs(found->second.name)+" ("+qs(driver->link.object)+")");
    };
    auto* layout_state_label=new QLabel(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
        .arg(qs(layout_state.literal),layout_driver_name(layout_state.driver),qs(layout_state.evaluated)));
    layout_state_label->setObjectName("text-layout-state");layout_state_label->setWordWrap(true);layout_state_label->setTextFormat(Qt::PlainText);form->addRow("",layout_state_label);
    const Ref direction_ref{id,"","text.direction"};const auto direction_state=text_direction_property(host.session.document(),direction_ref);
    const auto direction_revision=host.session.revision();
    auto* direction_row=new QWidget(box);auto* direction_layout=new QHBoxLayout(direction_row);direction_layout->setContentsMargins(0,0,0,0);
    auto* direction=new QComboBox;direction->setObjectName("text-direction");direction->addItems({"Horizontal","Vertical"});
    direction->setCurrentIndex(direction_state.evaluated=="vertical"?1:0);direction->setEnabled(false);
    direction->setToolTip("Use Edit writing direction to stage and apply a change.");direction_layout->addWidget(direction);
    auto* direction_driver_button=new QToolButton(direction_row);direction_driver_button->setObjectName("text-direction-driver");
    direction_driver_button->setText(direction_state.driver?"Driver…":"Drive…");direction_driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* direction_menu=new QMenu(direction_driver_button);direction_driver_button->setMenu(direction_menu);direction_layout->addWidget(direction_driver_button);
    auto* edit_direction=direction_menu->addAction("Edit writing direction…");
    auto* link_direction=direction_menu->addAction("Link to Text direction…");
    auto* unlink_direction=direction_menu->addAction("Unlink direction");unlink_direction->setEnabled(direction_state.driver.has_value());
    connect(edit_direction,&QAction::triggered,this,[this,id,frozen_session,direction_revision,direction_ref,direction_state]{
        QDialog dialog(this);dialog.setObjectName("text-direction-dialog");dialog.setWindowTitle("Edit writing direction");
        auto* box_layout=new QVBoxLayout(&dialog);
        auto* editor=new QComboBox(&dialog);editor->setObjectName("text-direction-editor");editor->addItems({"Horizontal","Vertical"});
        editor->setCurrentIndex(direction_state.evaluated=="vertical"?1:0);editor->setEnabled(!direction_state.driver);box_layout->addWidget(editor);
        auto* unlink=new QCheckBox("Unlink the driver and edit this writing direction",&dialog);
        unlink->setObjectName("unlink-text-direction-driver");unlink->setVisible(direction_state.driver.has_value());box_layout->addWidget(unlink);
        auto* status=new QLabel("Apply commits the direction change. Cancel keeps the current direction.",&dialog);
        status->setObjectName("text-direction-editor-status");status->setWordWrap(true);box_layout->addWidget(status);
        connect(unlink,&QCheckBox::toggled,editor,&QWidget::setEnabled);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);box_layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[this,&dialog,id,frozen_session,direction_revision,direction_ref,direction_state,editor,unlink,status]{
            try {
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
                if(host.session.revision()!=direction_revision)throw Error("STALE_CONTEXT","Text changed while the direction editor was open; reopen it");
                if(direction_state.driver&&!unlink->isChecked())throw Error("DRIVEN_PROPERTY","Select the unlink option before applying a direction edit");
                const auto found=host.session.document().objects.find(id);
                if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","Text no longer exists");
                const auto value=editor->currentIndex()==1?std::string("vertical"):std::string("horizontal");
                std::vector<Command> commands;
                if(direction_state.driver)commands.push_back(UnlinkTextDirection{direction_ref});
                if(value!=(direction_state.driver?direction_state.evaluated:direction_state.literal)) {
                    auto next=*found->second.text;next.direction=value;next.direction_driver.reset();
                    commands.push_back(UpdateText{id,std::move(next)});
                }
                if(!commands.empty()){host.session.apply(commands,direction_revision);host.edited();}
                dialog.accept();
            } catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
        });
        dialog.exec();
    });
    QStringList direction_source_labels;std::vector<Id> direction_source_ids;
    for(const auto& [source_id,source_object]:host.session.document().objects)if(source_object.kind==Kind::text&&source_object.text&&source_id!=id) {
        direction_source_ids.push_back(source_id);direction_source_labels<<qs(source_object.name)+" — "+qs(source_id);
    }
    link_direction->setEnabled(!direction_source_ids.empty());const bool replace_direction_driver=direction_state.driver.has_value();
    connect(link_direction,&QAction::triggered,this,[this,id,frozen_session,direction_revision,replace_direction_driver,direction_source_ids,direction_source_labels]{
        QDialog dialog(this);dialog.setObjectName("text-direction-source-dialog");dialog.setWindowTitle("Link Text direction");
        auto* layout=new QVBoxLayout(&dialog);
        auto* search=new QLineEdit(&dialog);search->setObjectName("text-direction-source-search");
        search->setPlaceholderText("Search Text name, object ID or text.direction");layout->addWidget(search);
        auto* source=new QComboBox(&dialog);source->setObjectName("text-direction-source");
        for(int i=0;i<direction_source_labels.size();++i)source->addItem(direction_source_labels.at(i),i);
        source->setCurrentIndex(-1);layout->addWidget(source);
        auto* status=new QLabel("Choose a visible Text direction source. Cancel keeps the current driver.",&dialog);
        status->setObjectName("text-direction-source-status");status->setWordWrap(true);layout->addWidget(status);
        connect(search,&QLineEdit::textChanged,&dialog,[source,direction_source_labels,direction_source_ids](const QString& query){
            const int selected=source->currentIndex()<0?-1:source->currentData().toInt();
            const QSignalBlocker blocker(source);source->clear();
            for(int i=0;i<direction_source_labels.size();++i){
                const auto path=qs(direction_source_ids.at(static_cast<std::size_t>(i)))+" / text.direction";
                if(direction_source_labels.at(i).contains(query,Qt::CaseInsensitive)||path.contains(query,Qt::CaseInsensitive))
                    source->addItem(direction_source_labels.at(i),i);
            }
            source->setCurrentIndex(selected<0?-1:source->findData(selected));
        });
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
            [this,&dialog,id,frozen_session,direction_revision,replace_direction_driver,direction_source_ids,source,status]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
                    if(host.session.revision()!=direction_revision)throw Error("STALE_CONTEXT","Text changed while the source chooser was open; reopen it");
                    if(source->currentIndex()<0)throw Error("MISSING_REFERENCE","Choose a visible Text direction source");
                    const int index=source->currentData().toInt();
                    if(index<0||static_cast<std::size_t>(index)>=direction_source_ids.size())
                        throw Error("MISSING_REFERENCE","Choose a valid Text direction source");
                    host.session.apply({LinkTextDirection{{id,"","text.direction"},
                        {direction_source_ids.at(static_cast<std::size_t>(index)),"","text.direction"},replace_direction_driver}},direction_revision);
                    host.edited();dialog.accept();
                } catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog.exec();
    });
    connect(unlink_direction,&QAction::triggered,this,[this,frozen_session,direction_revision,direction_ref]{
        perform([&]{if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
            host.session.apply({UnlinkTextDirection{direction_ref}},direction_revision);host.edited();});
    });
    direction_layout->addStretch();form->addRow("Writing",direction_row);
    const auto direction_driver_name=[this](const std::optional<TextDirectionDriver>& driver) {
        if(!driver)return QString("none");
        const auto found=host.session.document().objects.find(driver->link.object);
        return QString("link to ")+(found==host.session.document().objects.end()?qs(driver->link.object):qs(found->second.name)+" ("+qs(driver->link.object)+")");
    };
    auto* direction_status=new QLabel(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
        .arg(qs(direction_state.literal),direction_driver_name(direction_state.driver),qs(direction_state.evaluated)));
    direction_status->setObjectName("text-direction-state");direction_status->setWordWrap(true);direction_status->setTextFormat(Qt::PlainText);form->addRow("",direction_status);
    const Ref alignment_ref{id,"","text.alignment"};const auto alignment_state=text_alignment_property(host.session.document(),alignment_ref);
    const auto alignment_revision=host.session.revision();
    auto* alignment_row=new QWidget(box);auto* alignment_row_layout=new QHBoxLayout(alignment_row);alignment_row_layout->setContentsMargins(0,0,0,0);
    auto* text_alignment=new QComboBox;text_alignment->setObjectName("text-alignment");text_alignment->addItems({"Start","Center","End"});
    text_alignment->setCurrentIndex(alignment_state.evaluated=="center"?1:alignment_state.evaluated=="end"?2:0);text_alignment->setEnabled(false);
    text_alignment->setToolTip("Use Edit alignment to stage and apply a change.");alignment_row_layout->addWidget(text_alignment);
    auto* alignment_driver_button=new QToolButton(alignment_row);alignment_driver_button->setObjectName("text-alignment-driver");
    alignment_driver_button->setText(alignment_state.driver?"Driver…":"Drive…");alignment_driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* alignment_menu=new QMenu(alignment_driver_button);alignment_driver_button->setMenu(alignment_menu);alignment_row_layout->addWidget(alignment_driver_button);
    auto* edit_alignment=alignment_menu->addAction("Edit alignment…");
    auto* link_alignment=alignment_menu->addAction("Link to Text alignment…");
    auto* unlink_alignment=alignment_menu->addAction("Unlink alignment");unlink_alignment->setEnabled(alignment_state.driver.has_value());
    connect(edit_alignment,&QAction::triggered,this,[this,id,frozen_session,alignment_revision,alignment_ref,alignment_state]{
        QDialog dialog(this);dialog.setObjectName("text-alignment-dialog");dialog.setWindowTitle("Edit Text alignment");
        auto* box_layout=new QVBoxLayout(&dialog);
        auto* editor=new QComboBox(&dialog);editor->setObjectName("text-alignment-editor");editor->addItems({"Start","Center","End"});
        editor->setCurrentIndex(alignment_state.evaluated=="center"?1:alignment_state.evaluated=="end"?2:0);
        editor->setEnabled(!alignment_state.driver);box_layout->addWidget(editor);
        auto* unlink=new QCheckBox("Unlink the driver and edit this alignment",&dialog);
        unlink->setObjectName("unlink-text-alignment-driver");unlink->setVisible(alignment_state.driver.has_value());box_layout->addWidget(unlink);
        auto* status=new QLabel("Apply commits the alignment. Cancel keeps the current alignment.",&dialog);
        status->setObjectName("text-alignment-editor-status");status->setWordWrap(true);box_layout->addWidget(status);
        connect(unlink,&QCheckBox::toggled,editor,&QWidget::setEnabled);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);box_layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[this,&dialog,id,frozen_session,alignment_revision,alignment_ref,alignment_state,editor,unlink,status]{
            try {
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
                if(host.session.revision()!=alignment_revision)throw Error("STALE_CONTEXT","Text changed while the alignment editor was open; reopen it");
                if(alignment_state.driver&&!unlink->isChecked())throw Error("DRIVEN_PROPERTY","Select the unlink option before applying an alignment edit");
                const auto found=host.session.document().objects.find(id);
                if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","Text no longer exists");
                const std::array<std::string,3> values{"start","center","end"};
                const auto value=values.at(static_cast<std::size_t>(editor->currentIndex()));
                std::vector<Command> commands;
                if(alignment_state.driver)commands.push_back(UnlinkTextAlignment{alignment_ref});
                if(value!=(alignment_state.driver?alignment_state.evaluated:alignment_state.literal)) {
                    auto next=*found->second.text;next.alignment=value;next.alignment_driver.reset();
                    commands.push_back(UpdateText{id,std::move(next)});
                }
                if(!commands.empty()){host.session.apply(commands,alignment_revision);host.edited();}
                dialog.accept();
            } catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
        });
        dialog.exec();
    });
    std::vector<Id> alignment_source_ids;
    for(const auto& [source_id,source_object]:host.session.document().objects)if(source_object.kind==Kind::text&&source_object.text&&source_id!=id) {
        alignment_source_ids.push_back(source_id);
    }
    link_alignment->setEnabled(!alignment_source_ids.empty());const bool replace_alignment_driver=alignment_state.driver.has_value();
    connect(link_alignment,&QAction::triggered,this,[this,id,frozen_session,alignment_revision,alignment_ref,replace_alignment_driver,alignment_source_ids]{
        const auto selection=canvas->selections();
        const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
        auto picker=make_text_source_picker(this,host.session.document(),id,alignment_ref.field,alignment_source_ids,"Link Text alignment");
        auto* dialog=picker.dialog;auto* list=picker.list;auto* status=picker.status;
        connect(list,&QListWidget::currentItemChanged,this,[this,frozen_session](QListWidgetItem* item,QListWidgetItem*){
            if(!item||item->isHidden()||host.session_id!=frozen_session)return;
            const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
            if(host.session.document().objects.contains(source.object))canvas->set_selection(source.object,{});
        });
        connect(dialog,&QDialog::rejected,this,[this,selection,frozen_session,composition,artboard]{
            if(host.session_id==frozen_session){perform([&]{canvas->set_active_artboard(composition,artboard,false);});canvas->set_selections(selection);}
        });
        connect(picker.buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,dialog,
            [this,dialog,list,status,alignment_ref,frozen_session,alignment_revision,replace_alignment_driver,selection,composition,artboard]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text source chooser belongs to a different document");
                    auto* item=list->currentItem();
                    if(!item||item->isHidden())throw Error("NO_SOURCE","Choose a visible Text source");
                    const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
                    if(source.field!=alignment_ref.field||!source.point.empty()||source.object==alignment_ref.object)
                        throw Error("INVALID_REFERENCE","Choose a different Text with the same property");
                    if(host.session.revision()!=alignment_revision)throw Error("REVISION_CONFLICT","Text changed while the source chooser was open");
                    host.session.apply({LinkTextAlignment{alignment_ref,source,replace_alignment_driver}},alignment_revision);
                    canvas->set_active_artboard(composition,artboard,false);canvas->set_selections(selection);host.edited();dialog->accept();
                } catch(const Error& error){status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
                catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog->show();picker.search->setFocus();
    });
    connect(unlink_alignment,&QAction::triggered,this,[this,frozen_session,alignment_revision,alignment_ref]{
        perform([&]{if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
            host.session.apply({UnlinkTextAlignment{alignment_ref}},alignment_revision);host.edited();});
    });
    alignment_row_layout->addStretch();form->addRow("Alignment",alignment_row);
    const auto alignment_driver_name=[this](const std::optional<TextAlignmentDriver>& driver) {
        if(!driver)return QString("none");
        const auto found=host.session.document().objects.find(driver->link.object);
        return QString("link to ")+(found==host.session.document().objects.end()?qs(driver->link.object):qs(found->second.name)+" ("+qs(driver->link.object)+")");
    };
    auto* alignment_status=new QLabel(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
        .arg(qs(alignment_state.literal),alignment_driver_name(alignment_state.driver),qs(alignment_state.evaluated)));
    alignment_status->setObjectName("text-alignment-state");alignment_status->setWordWrap(true);alignment_status->setTextFormat(Qt::PlainText);form->addRow("",alignment_status);
    const Ref locale_ref{id,"","text.locale"};const auto locale_state=text_locale_property(host.session.document(),locale_ref);
    const auto locale_revision=host.session.revision();
    auto* locale_row=new QWidget(box);auto* locale_layout=new QHBoxLayout(locale_row);locale_layout->setContentsMargins(0,0,0,0);
    auto* locale_value=new QLineEdit(qs(locale_state.driver?locale_state.evaluated:locale_state.literal));
    locale_value->setObjectName("text-locale");locale_value->setReadOnly(true);
    locale_value->setToolTip("Use Edit locale to stage and apply a change.");locale_layout->addWidget(locale_value);
    auto* locale_driver_button=new QToolButton(locale_row);locale_driver_button->setObjectName("text-locale-driver");
    locale_driver_button->setText(locale_state.driver?"Driver…":"Drive…");locale_driver_button->setPopupMode(QToolButton::InstantPopup);
    auto* locale_menu=new QMenu(locale_driver_button);locale_driver_button->setMenu(locale_menu);locale_layout->addWidget(locale_driver_button);
    auto* edit_locale=locale_menu->addAction("Edit locale…");
    auto* link_locale=locale_menu->addAction("Link to Text locale…");
    auto* unlink_locale=locale_menu->addAction("Unlink locale");unlink_locale->setEnabled(locale_state.driver.has_value());
    connect(edit_locale,&QAction::triggered,this,[this,id,frozen_session,locale_revision,locale_state,locale_ref]{
        QDialog dialog(this);dialog.setObjectName("text-locale-dialog");dialog.setWindowTitle("Edit Text locale");
        auto* box_layout=new QVBoxLayout(&dialog);
        auto* editor=new QLineEdit(qs(locale_state.driver?locale_state.evaluated:locale_state.literal),&dialog);
        editor->setObjectName("text-locale-editor");editor->setEnabled(!locale_state.driver);box_layout->addWidget(editor);
        auto* unlink=new QCheckBox("Unlink the driver and edit this locale",&dialog);
        unlink->setObjectName("unlink-text-locale-driver");unlink->setVisible(locale_state.driver.has_value());box_layout->addWidget(unlink);
        auto* status=new QLabel("Apply commits the locale. Cancel keeps the current locale.",&dialog);
        status->setObjectName("text-locale-editor-status");status->setWordWrap(true);box_layout->addWidget(status);
        connect(unlink,&QCheckBox::toggled,editor,&QWidget::setEnabled);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);box_layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[this,&dialog,id,frozen_session,locale_revision,locale_state,locale_ref,editor,unlink,status]{
            try {
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
                if(host.session.revision()!=locale_revision)throw Error("STALE_CONTEXT","Text changed while the locale editor was open; reopen it");
                if(locale_state.driver&&!unlink->isChecked())throw Error("DRIVEN_PROPERTY","Select the unlink option before applying a locale edit");
                const auto found=host.session.document().objects.find(id);
                if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","Text no longer exists");
                const auto value=editor->text().toStdString();
                std::vector<Command> commands;
                if(locale_state.driver)commands.push_back(UnlinkTextLocale{locale_ref});
                if(value!=(locale_state.driver?locale_state.evaluated:locale_state.literal)) {
                    auto next=*found->second.text;next.locale=value;next.locale_driver.reset();
                    commands.push_back(UpdateText{id,std::move(next)});
                }
                if(!commands.empty()){host.session.apply(commands,locale_revision);host.edited();}
                dialog.accept();
            } catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
        });
        dialog.exec();
    });
    std::vector<Id> locale_source_ids;
    for(const auto& [source_id,source_object]:host.session.document().objects)if(source_object.kind==Kind::text&&source_object.text&&source_id!=id) {
        locale_source_ids.push_back(source_id);
    }
    link_locale->setEnabled(!locale_source_ids.empty());const bool replace_locale_driver=locale_state.driver.has_value();
    connect(link_locale,&QAction::triggered,this,[this,id,frozen_session,locale_revision,replace_locale_driver,locale_source_ids]{
        const auto target=Ref{id,"","text.locale"};const auto selection=canvas->selections();
        const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
        auto picker=make_text_source_picker(this,host.session.document(),id,target.field,locale_source_ids,"Link Text locale");
        auto* dialog=picker.dialog;auto* list=picker.list;auto* status=picker.status;
        connect(list,&QListWidget::currentItemChanged,this,[this,frozen_session](QListWidgetItem* item,QListWidgetItem*){
            if(!item||item->isHidden()||host.session_id!=frozen_session)return;
            const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
            if(host.session.document().objects.contains(source.object))canvas->set_selection(source.object,{});
        });
        connect(dialog,&QDialog::rejected,this,[this,selection,frozen_session,composition,artboard]{
            if(host.session_id==frozen_session){perform([&]{canvas->set_active_artboard(composition,artboard,false);});canvas->set_selections(selection);}
        });
        connect(picker.buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,dialog,
            [this,dialog,list,status,target,frozen_session,locale_revision,replace_locale_driver,selection,composition,artboard]{
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text source chooser belongs to a different document");
                    auto* item=list->currentItem();
                    if(!item||item->isHidden())throw Error("NO_SOURCE","Choose a visible Text source");
                    const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
                    if(source.field!=target.field||!source.point.empty()||source.object==target.object)
                        throw Error("INVALID_REFERENCE","Choose a different Text with the same property");
                    if(host.session.revision()!=locale_revision)throw Error("REVISION_CONFLICT","Text changed while the source chooser was open");
                    host.session.apply({LinkTextLocale{target,source,replace_locale_driver}},locale_revision);
                    canvas->set_active_artboard(composition,artboard,false);canvas->set_selections(selection);host.edited();dialog->accept();
                } catch(const Error& error){status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
                catch(const std::exception& error){status->setText(QString::fromUtf8(error.what()));}
            });
        dialog->show();picker.search->setFocus();
    });
    connect(unlink_locale,&QAction::triggered,this,[this,id,frozen_session,locale_revision,locale_ref]{
        perform([&]{if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text belongs to another document");
            host.session.apply({UnlinkTextLocale{locale_ref}},locale_revision);host.edited();});
    });
    form->addRow("Language tag",locale_row);
    const auto locale_driver_name=[this](const std::optional<TextLocaleDriver>& driver) {
        if(!driver)return QString("none");
        const auto found=host.session.document().objects.find(driver->link.object);
        return QString("link to ")+(found==host.session.document().objects.end()?qs(driver->link.object):qs(found->second.name)+" ("+qs(driver->link.object)+")");
    };
    auto* locale_status=new QLabel(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
        .arg(qs(locale_state.literal),locale_driver_name(locale_state.driver),qs(locale_state.evaluated)));
    locale_status->setObjectName("text-locale-state");locale_status->setWordWrap(true);locale_status->setTextFormat(Qt::PlainText);form->addRow("",locale_status);
    for(const auto* parameter:{"origin_x","origin_y","font_size","frame_width","frame_height","tracking","line_spacing"})
        add_property(form,{id,"",std::string("text.")+parameter},parameter_label(parameter));
    const auto current_values=evaluate(host.session.document());
    const auto result=evaluate_text_projection(host.session.document(),id,current_values);
    QStringList lines;lines<<QString("%1 × %2 du · %3 glyphs").arg(display_value(result.width),display_value(result.height)).arg(result.glyph_count);
    if(result.overflow)lines<<"Text extends outside its frame. Increase the frame or reduce the type size.";
    for(const auto& warning:result.warnings)lines<<qs(warning);
    QStringList fonts;for(const auto& name:result.used_fonts)fonts<<qs(name);
    lines<<"Rendered fonts: "+fonts.join(", ")<<"Native text stays editable. SVG exports glyph outlines; fonts are not embedded.";
    auto* status=new QLabel(lines.join('\n'));status->setObjectName("text-layout-status");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);form->addRow(status);
}
void Window::edit_text_content(const Id& id) {
    const auto session=host.session_id;auto source=*host.session.document().objects.at(id).text;
    const auto displayed=evaluate_text_content(host.session.document(),id);
    QDialog dialog(this);dialog.setObjectName("text-editor-dialog");dialog.setWindowTitle("Edit text");dialog.resize(560,340);
    auto* layout=new QVBoxLayout(&dialog);auto* editor=new QPlainTextEdit(qs(displayed));editor->setObjectName("text-content-editor");
    editor->setAccessibleName("Text content");layout->addWidget(editor);
    auto* message=new QLabel("Apply commits one undo step. Cancel discards only this draft.");message->setWordWrap(true);message->setTextFormat(Qt::PlainText);
    message->setObjectName("text-editor-status");layout->addWidget(message);
    auto* unlink=new QPushButton("Unlink driver and edit");unlink->setObjectName("unlink-text-content-driver");
    unlink->setVisible(source.content_driver.has_value());layout->addWidget(unlink);
    bool unlink_requested=false;
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(unlink,&QPushButton::clicked,&dialog,[&,id,session]{
        try {
            if(host.session_id!=session)throw Error("SESSION_CONFLICT","The document changed. Copy this draft before closing.");
            const auto found=host.session.document().objects.find(id);
            if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","The text was removed. Copy this draft before closing.");
            const auto& current=*found->second.text;
            if(!source.content_driver||current.content_driver!=source.content_driver||current.content!=source.content||
               evaluate_text_content(host.session.document(),id)!=displayed)
                throw Error("TEXT_EDIT_CONFLICT","Text content changed elsewhere. Copy this draft, cancel, and reopen the latest text.");
            unlink_requested=true;
            unlink->setEnabled(false);
            message->setText("Driver unlink is staged. Apply commits the unlink and text edit together; Cancel discards both.");
        } catch(const std::exception& e){message->setText(QString::fromUtf8(e.what()));}
    });
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&,id,session]{
        try {
            if(host.session_id!=session)throw Error("SESSION_CONFLICT","The document changed. Copy this draft before closing.");
            const auto found=host.session.document().objects.find(id);
            if(found==host.session.document().objects.end()||!found->second.text)throw Error("NOT_TEXT","The text was removed. Copy this draft before closing.");
            auto next=*found->second.text;
            if(next.id!=source.id||next.content!=source.content||next.content_driver!=source.content_driver)
                throw Error("TEXT_EDIT_CONFLICT","Text changed elsewhere. Copy this draft, cancel, and reopen the latest text.");
            const auto content=editor->toPlainText().toStdString();
            if(next.content_driver&&!unlink_requested&&content!=displayed)
                throw Error("DRIVEN_PROPERTY","Unlink the Text content driver before changing its authored literal.");
            if(next.content_driver&&unlink_requested) {
                if(evaluate_text_content(host.session.document(),id)!=displayed)
                    throw Error("TEXT_EDIT_CONFLICT","Text content changed elsewhere. Copy this draft, cancel, and reopen the latest text.");
                std::vector<Command> commands{UnlinkTextContent{{id,"","text.content"}}};
                if(content!=displayed) {
                    next.content=content;next.content_driver.reset();
                    commands.push_back(UpdateText{id,std::move(next)});
                }
                host.session.apply(commands,host.session.revision());host.edited();
            } else if(!next.content_driver&&content!=next.content) {
                next.content=content;host.session.apply({UpdateText{id,std::move(next)}},host.session.revision());host.edited();
            }
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
    heading_layout->addWidget(new QLabel(object.kind==Kind::group?"Group effects":"Shape stack"));heading_layout->addStretch();
    auto* add=new QPushButton("Add…");add->setObjectName("stack-add");
    auto* menu=new QMenu(add);
    if(object.kind==Kind::group) {
        add->setVisible(false);
    } else {
        for(const auto* name:{"add-fill","add-stroke","add-offset","add-repeater","add-radial-repeater"})
            if(auto* action=findChild<QAction*>(QString::fromLatin1(name)))menu->addAction(action);
        add->setMenu(menu);
    }
    heading_layout->addWidget(add);layout->addWidget(heading);
    auto* order_hint=new QLabel(object.kind==Kind::group?
        "Group effects run after child content is composited and before Group mask, opacity and blend.":
        "Earlier paints default above later paints. Path modifiers affect geometry before paint is applied. Repeater affects paths and paints before it; copies share their source points.");
    order_hint->setWordWrap(true);order_hint->setStyleSheet("color: #a4acb8; font-size: 11px;");layout->addWidget(order_hint);
    const auto frozen_session=host.session_id;
    const auto& document=host.session.document();
    const Composition* owner_composition=nullptr;
    const std::function<bool(const Id&)> contains_object=[&](const Id& id) {
        if(id==object.id)return true;
        for(const auto& child:document.objects.at(id).children)if(contains_object(child))return true;
        return false;
    };
    for(const auto& composition:document.compositions) {
        if(std::any_of(composition.roots.begin(),composition.roots.end(),contains_object)) {
            owner_composition=&composition;break;
        }
    }
    if(!owner_composition)throw Error("ORPHAN_OBJECT","Operation Inspector target is not owned by a Composition");
    const auto enabled_composition=owner_composition->id;
    auto apply=[this,frozen_session](const std::vector<Command>& commands) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The shape stack belongs to another document");
        host.session.apply(commands,host.session.revision());host.edited();
    };
    for(std::size_t index=0;index<object.stack.size();++index) {
        const auto& operation=object.stack[index];
        const auto name=operation.macro?qs(host.session.document().macro_definitions.at(operation.macro->definition).label):operation_label(operation);
        auto* group=new QGroupBox(QString::number(index+1)+" · "+name);
        group->setObjectName("stack-operation-"+qs(operation.id));
        group->setProperty("nect-operation",qs(operation.id));
        auto* form=new QFormLayout(group);form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addWidget(group);
        auto* controls=new QWidget;auto* row=new QHBoxLayout(controls);row->setContentsMargins(0,0,0,0);
        const auto enabled_ref=operation_ref(object.id,operation.id,"enabled");
        const auto enabled_state=operation_enabled_state(host.session.document(),enabled_ref);
        const bool enabled_driven=enabled_state.driver.has_value()||enabled_state.expression.has_value();
        auto* enabled=new QCheckBox("Enabled");enabled->setChecked(enabled_state.literal);enabled->setEnabled(!enabled_driven);
        enabled->setObjectName("operation-enabled-"+qs(operation.id));
        enabled->setAccessibleName(name+" enabled");row->addWidget(enabled);row->addStretch();
        auto* enabled_driver=new QPushButton(enabled_state.driver?"Driver…":enabled_state.expression?"Expression…":"Link…");
        enabled_driver->setObjectName("operation-enabled-driver-"+qs(operation.id));
        enabled_driver->setEnabled(true);
        row->addWidget(enabled_driver);
        auto* up=new QPushButton("↑");up->setFixedWidth(28);up->setEnabled(index>0);
        up->setObjectName("operation-up-"+qs(operation.id));up->setToolTip("Move earlier in the stack");
        auto* down=new QPushButton("↓");down->setFixedWidth(28);down->setEnabled(index+1<object.stack.size());
        down->setObjectName("operation-down-"+qs(operation.id));down->setToolTip("Move later in the stack");
        auto* remove=new QPushButton("×");remove->setFixedWidth(28);
        remove->setObjectName("operation-remove-"+qs(operation.id));remove->setToolTip("Remove "+name);
        remove->setAccessibleName("Remove "+name);
        row->addWidget(up);row->addWidget(down);row->addWidget(remove);
        if(operation.macro) {
            auto* detach=new QPushButton("Detach");detach->setObjectName("macro-detach-"+qs(operation.id));
            detach->setToolTip("Replace this Macro entry in place with fresh ordinary Offset and Repeater operations.");
            row->addWidget(detach);
            connect(detach,&QPushButton::clicked,this,[this,id=object.id,instance=operation.id,apply]{perform([&]{
                apply({MacroCommand{DetachMacroInstance{id,instance,new_id()}}});
            });});
        }
        form->addRow(controls);
        QString enabled_source_name="none";
        if(enabled_state.driver) {
            const auto source_object=host.session.document().objects.find(enabled_state.driver->object);
            const auto source_name=source_object==host.session.document().objects.end()?qs(enabled_state.driver->object):qs(source_object->second.name);
            enabled_source_name=QString("%1 / %2").arg(source_name,qs(enabled_state.driver->field));
        } else if(enabled_state.expression)enabled_source_name="expression: "+qs(enabled_state.expression->source);
        auto* enabled_state_label=new QLabel(QString("Literal: %1 · Source: %2 · Evaluated: %3")
            .arg(enabled_state.literal?"true":"false",enabled_source_name,enabled_state.evaluated?"true":"false"));
        enabled_state_label->setObjectName("operation-enabled-state-"+qs(operation.id));
        enabled_state_label->setWordWrap(true);enabled_state_label->setTextFormat(Qt::PlainText);
        form->addRow("",enabled_state_label);
        connect(enabled_driver,&QPushButton::clicked,this,[this,object_id=object.id,operation_id=operation.id,
            macro_entry=operation.macro.has_value(),
            enabled_ref,enabled_state,frozen_session,enabled_composition,frozen_revision=host.session.revision()] {
            QDialog dialog(this);dialog.setObjectName("operation-enabled-dialog-"+qs(operation_id));
            dialog.setWindowTitle("Operation enabled dependency");auto* dialog_layout=new QVBoxLayout(&dialog);
            auto* mode=new QComboBox(&dialog);mode->setObjectName("operation-enabled-mode-"+qs(operation_id));
            mode->addItem("Choose an action","none");
            const auto& current_document=host.session.document();
            const auto& composition=find_composition(current_document,enabled_composition);
            std::vector<Ref> source_refs;QStringList source_labels;
            std::function<void(const Id&)> collect=[&](const Id& source_id) {
                const auto& source_object=current_document.objects.at(source_id);
                for(const auto& source_operation:source_object.stack) {
                    const auto source_ref=operation_ref(source_id,source_operation.id,"enabled");
                    if(source_ref==enabled_ref)continue;
                    source_refs.push_back(source_ref);
                    source_labels<<qs(source_object.name)+" — "+operation_label(source_operation)+" ["+
                        qs(source_operation.id)+"] — "+qs(source_id);
                }
                for(const auto& child:source_object.children)collect(child);
            };
            for(const auto& root:composition.roots)collect(root);
            const bool has_source=enabled_state.driver.has_value()||enabled_state.expression.has_value();
            if(!source_refs.empty())mode->addItem(has_source?"Replace with another operation":"Link to another operation","link");
            if(!macro_entry)mode->addItem(has_source?"Replace with an expression":"Set an expression","expression");
            if(has_source)mode->addItem("Unlink and freeze evaluated value","unlink");
            dialog_layout->addWidget(mode);
            auto* source_search=new QLineEdit(&dialog);
            source_search->setObjectName("operation-enabled-source-search-"+qs(operation_id));
            source_search->setPlaceholderText("Search object ID, operation ID or property path");
            dialog_layout->addWidget(source_search);
            auto* source=new QComboBox(&dialog);source->setObjectName("operation-enabled-source-"+qs(operation_id));
            for(int i=0;i<source_labels.size();++i)source->addItem(source_labels.at(i),i);
            if(enabled_state.driver) {
                const auto found=std::find(source_refs.begin(),source_refs.end(),*enabled_state.driver);
                if(found!=source_refs.end())source->setCurrentIndex(static_cast<int>(std::distance(source_refs.begin(),found)));
            }
            dialog_layout->addWidget(source);
            connect(source_search,&QLineEdit::textChanged,&dialog,[source,source_labels,source_refs](const QString& query) {
                const auto selected=source->currentData().toInt();
                const bool had_selection=source->currentIndex()>=0;
                const QSignalBlocker blocker(source);
                source->clear();
                for(int i=0;i<source_labels.size();++i) {
                    const auto& ref=source_refs.at(static_cast<std::size_t>(i));
                    const auto path=qs(ref.object)+" / "+qs(ref.field);
                    if(source_labels.at(i).contains(query,Qt::CaseInsensitive)||path.contains(query,Qt::CaseInsensitive))
                        source->addItem(source_labels.at(i),i);
                }
                const auto retained=had_selection?source->findData(selected):-1;
                source->setCurrentIndex(retained);
            });
            auto* expression=new QPlainTextEdit(&dialog);
            expression->setObjectName("operation-enabled-expression-source-"+qs(operation_id));
            expression->setPlaceholderText("true, false, ref(\"object-id\",\"\",\"op.operation-id.enabled\"), or !ref(…)");
            if(enabled_state.expression)expression->setPlainText(qs(enabled_state.expression->source));
            else expression->setPlainText("true");
            expression->setMinimumHeight(64);dialog_layout->addWidget(expression);
            auto* status=new QLabel("The expression can use true, false, or an optional negation of a built-in operation enabled Ref in this Composition. Apply commits one Session command; Cancel leaves it unchanged.",&dialog);
            if(source_refs.empty()&&!has_source)
                status->setText(macro_entry?
                    "No other operation in this Composition can drive the enabled state. Cancel leaves the Session unchanged.":
                    "No link source is available in this Composition. An expression can still use true or false; Cancel leaves the Session unchanged.");
            status->setObjectName("operation-enabled-status-"+qs(operation_id));status->setWordWrap(true);dialog_layout->addWidget(status);
            const auto update_mode=[mode,source,source_search,expression] {
                const auto selected=mode->currentData().toString();
                source->setVisible(selected=="link");source_search->setVisible(selected=="link");
                expression->setVisible(selected=="expression");
            };
            connect(mode,&QComboBox::currentIndexChanged,&dialog,[update_mode](int){update_mode();});update_mode();
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);dialog_layout->addWidget(buttons);
            connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
            connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
                [this,&dialog,object_id,operation_id,enabled_ref,enabled_state,frozen_session,frozen_revision,mode,source,source_refs,expression,status] {
                    try {
                        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Operation stack belongs to another document");
                        if(host.session.revision()!=frozen_revision)throw Error("STALE_CONTEXT","Operation enabled source changed while its editor was open; reopen it");
                        const auto selected=mode->currentData().toString();std::vector<Command> commands;
                        if(selected=="link") {
                            if(source->currentIndex()<0)throw Error("MISSING_REFERENCE","Choose a visible operation enabled source");
                            const auto source_index=source->currentData().toInt();
                            if(source_index<0||static_cast<std::size_t>(source_index)>=source_refs.size())
                                throw Error("MISSING_REFERENCE","Choose a valid operation enabled source");
                            commands.push_back(LinkOperationEnabled{enabled_ref,source_refs.at(static_cast<std::size_t>(source_index)),
                                enabled_state.driver.has_value()||enabled_state.expression.has_value()});
                        } else if(selected=="expression") {
                            commands.push_back(SetOperationEnabledExpression{enabled_ref,
                                {expression->toPlainText().toStdString(),1},
                                enabled_state.driver.has_value()||enabled_state.expression.has_value()});
                        } else if(selected=="unlink")commands.push_back(UnlinkOperationEnabled{enabled_ref});
                        else throw Error("INVALID_COMMAND","Choose a link or unlink action");
                        host.session.apply(commands,frozen_revision);host.edited();dialog.accept();
                    } catch(const std::exception& error) {status->setText(QString::fromUtf8(error.what()));}
                });
            dialog.exec();
        });
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
        if(!operation.macro&&operation.type!="nect.shape.offset") {
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
        if(operation.macro) {
            const auto& definition=host.session.document().macro_definitions.at(operation.macro->definition);
            const auto& macro_revision=definition.revisions.at(operation.macro->pinned_revision);
            const auto parameter=std::find_if(macro_revision.public_parameters.begin(),macro_revision.public_parameters.end(),
                [](const auto& item){return item.id=="macro.offset.amount";});
            if(parameter!=macro_revision.public_parameters.end()) {
                const auto parameter_id=parameter->id;
                const auto amount_ref=macro_parameter_ref(object.id,operation.id,parameter_id);
                const auto initial=macro_parameter_value(host.session.document(),object.id,operation.id,parameter_id);
                const auto frozen_macro_session=host.session_id;
                const auto frozen_macro_revision=host.session.revision();
                auto* editor=new QDoubleSpinBox(group);editor->setObjectName("macro-amount-"+qs(operation.id));
                editor->setAccessibleName(name+" / "+qs(parameter->label));editor->setDecimals(3);
                editor->setRange(-1e6,1e6);editor->setSingleStep(1);editor->setSuffix(" "+qs(parameter->unit));
                editor->setValue(initial);
                editor->setProperty("nect-reference",QJsonDocument(ref_json(amount_ref)).toJson(QJsonDocument::Compact));
                connect(editor,&QDoubleSpinBox::editingFinished,this,[this,editor,initial,id=object.id,
                    instance=operation.id,parameter_id,frozen_macro_session,frozen_macro_revision]{perform([&]{
                    if(host.session_id!=frozen_macro_session)throw Error("SESSION_CONFLICT","Macro Amount belongs to another document");
                    if(host.session.revision()!=frozen_macro_revision)throw Error("REVISION_CONFLICT","Macro Amount changed; reopen the Inspector");
                    if(editor->value()==initial)return;
                    host.session.apply({MacroCommand{SetMacroOverride{id,instance,parameter_id,editor->value()}}},frozen_macro_revision);
                    host.edited();
                });});
                auto* amount_row=new QWidget(group);auto* amount_layout=new QHBoxLayout(amount_row);
                amount_layout->setContentsMargins(0,0,0,0);amount_layout->addWidget(editor);
                if(operation.macro->overrides.contains(parameter_id)) {
                    auto* reset=new QPushButton("Reset");reset->setObjectName("macro-reset-amount-"+qs(operation.id));
                    reset->setToolTip("Restore the value published by the pinned Macro revision.");amount_layout->addWidget(reset);
                    connect(reset,&QPushButton::clicked,this,[this,id=object.id,instance=operation.id,parameter_id,
                        frozen_macro_session,frozen_macro_revision]{perform([&]{
                        if(host.session_id!=frozen_macro_session)throw Error("SESSION_CONFLICT","Macro Amount belongs to another document");
                        if(host.session.revision()!=frozen_macro_revision)throw Error("REVISION_CONFLICT","Macro Amount changed; reopen the Inspector");
                        host.session.apply({MacroCommand{ResetMacroOverride{id,instance,parameter_id}}},frozen_macro_revision);
                        host.edited();
                    });});
                }
                form->addRow(qs(parameter->label)+" · "+qs(parameter->unit),amount_row);
            } else {
                auto* no_parameters=new QLabel("This pinned Macro revision has no published controls.",group);
                no_parameters->setObjectName("macro-no-public-parameters-"+qs(operation.id));
                no_parameters->setWordWrap(true);form->addRow(no_parameters);
            }
        } else if(operation.type=="nect.paint.fill") {
            const auto target=operation_ref(object.id,operation.id,"fill_rule");
            const auto state=fill_rule_property(host.session.document(),target);
            const auto rule_index=state.evaluated=="evenodd"?1:0;
            auto* rule_row=new QWidget(group);auto* rule_layout=new QHBoxLayout(rule_row);rule_layout->setContentsMargins(0,0,0,0);
            auto* rule=new QComboBox;rule->setObjectName("operation-fill-rule-"+qs(operation.id));
            rule->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            rule->setMinimumContentsLength(10);rule->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            rule->addItem("Nonzero winding","nonzero");rule->addItem("Even-odd","evenodd");
            rule->setCurrentIndex(rule_index);rule->setEnabled(false);
            rule->setToolTip("Use the Fill rule editor to stage a literal, link or unlink change.");rule_layout->addWidget(rule);
            auto* driver_button=new QToolButton(rule_row);driver_button->setObjectName("operation-fill-rule-driver-"+qs(operation.id));
            driver_button->setText(state.driver?"Driver…":"Drive…");driver_button->setPopupMode(QToolButton::InstantPopup);
            auto* driver_menu=new QMenu(driver_button);driver_button->setMenu(driver_menu);rule_layout->addWidget(driver_button);
            auto* edit_rule=driver_menu->addAction("Edit, link or unlink…");
            const auto frozen_revision=host.session.revision();
            connect(edit_rule,&QAction::triggered,this,[this,object_id=object.id,operation_id=operation.id,target,state,frozen_session,frozen_revision,enabled_composition] {
                QDialog dialog(this);dialog.setObjectName("fill-rule-dialog-"+qs(operation_id));dialog.setWindowTitle("Edit Fill rule");
                auto* dialog_layout=new QVBoxLayout(&dialog);
                auto* mode=new QComboBox(&dialog);mode->setObjectName("fill-rule-mode-"+qs(operation_id));
                mode->addItem("Edit literal","edit");
                QStringList source_labels;std::vector<Ref> source_refs;
                const auto& current_document=host.session.document();
                const auto& composition=find_composition(current_document,enabled_composition);
                std::function<void(const Id&)> collect=[&](const Id& source_id) {
                    const auto& source_object=current_document.objects.at(source_id);
                    if(source_object.kind==Kind::path||source_object.kind==Kind::text)
                        for(const auto& source_operation:source_object.stack)if(source_operation.type=="nect.paint.fill") {
                            const auto source_ref=operation_ref(source_id,source_operation.id,"fill_rule");
                            if(source_ref==target)continue;
                            source_refs.push_back(source_ref);
                            source_labels<<qs(source_object.name)+" — Fill ["+qs(source_operation.id)+"] — "+qs(source_id);
                        }
                    for(const auto& child:source_object.children)collect(child);
                };
                for(const auto& root:composition.roots)collect(root);
                if(!source_refs.empty())mode->addItem("Link to another Fill","link");
                if(state.driver)mode->addItem("Unlink driver","unlink");
                dialog_layout->addWidget(mode);
                auto* value=new QComboBox(&dialog);value->setObjectName("fill-rule-value-"+qs(operation_id));
                value->addItem("Nonzero winding","nonzero");value->addItem("Even-odd","evenodd");
                value->setCurrentIndex(state.evaluated=="evenodd"?1:0);dialog_layout->addWidget(value);
                auto* source_search=new QLineEdit(&dialog);
                source_search->setObjectName("fill-rule-source-search-"+qs(operation_id));
                source_search->setPlaceholderText("Search object ID, Fill ID or property path");
                dialog_layout->addWidget(source_search);
                auto* source=new QComboBox(&dialog);source->setObjectName("fill-rule-source-"+qs(operation_id));
                for(int i=0;i<source_labels.size();++i)source->addItem(source_labels.at(i),i);
                if(state.driver) {
                    const auto found=std::find(source_refs.begin(),source_refs.end(),state.driver->link);
                    if(found!=source_refs.end())source->setCurrentIndex(static_cast<int>(std::distance(source_refs.begin(),found)));
                }
                dialog_layout->addWidget(source);
                connect(source_search,&QLineEdit::textChanged,&dialog,[source,source_labels,source_refs](const QString& query) {
                    const auto selected=source->currentData().toInt();
                    const bool had_selection=source->currentIndex()>=0;
                    const QSignalBlocker blocker(source);
                    source->clear();
                    for(int i=0;i<source_labels.size();++i) {
                        const auto& ref=source_refs.at(static_cast<std::size_t>(i));
                        const auto path=qs(ref.object)+" / "+qs(ref.field);
                        if(source_labels.at(i).contains(query,Qt::CaseInsensitive)||path.contains(query,Qt::CaseInsensitive))
                            source->addItem(source_labels.at(i),i);
                    }
                    source->setCurrentIndex(had_selection?source->findData(selected):-1);
                });
                auto* unlink_edit=new QCheckBox("Unlink the driver before editing the literal",&dialog);
                unlink_edit->setObjectName("fill-rule-unlink-before-edit-"+qs(operation_id));
                unlink_edit->setVisible(state.driver.has_value());dialog_layout->addWidget(unlink_edit);
                auto* status=new QLabel("Apply commits the staged Fill rule change. Cancel keeps the Session unchanged.",&dialog);
                status->setObjectName("fill-rule-status-"+qs(operation_id));status->setWordWrap(true);dialog_layout->addWidget(status);
                const auto update_mode=[mode,value,source,unlink_edit,state] {
                    const auto selected=mode->currentData().toString();
                    value->setEnabled(selected=="edit"&&(!state.driver||unlink_edit->isChecked()));
                    source->setEnabled(selected=="link");
                    unlink_edit->setVisible(selected=="edit"&&state.driver.has_value());
                };
                connect(mode,&QComboBox::currentIndexChanged,&dialog,[update_mode](int){update_mode();});
                connect(unlink_edit,&QCheckBox::toggled,&dialog,[update_mode](bool){update_mode();});
                update_mode();
                auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);dialog_layout->addWidget(buttons);
                connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
                connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
                    [this,&dialog,object_id,operation_id,target,state,frozen_session,frozen_revision,mode,value,source,source_refs,unlink_edit,status] {
                        try {
                            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Fill belongs to another document");
                            if(host.session.revision()!=frozen_revision)throw Error("STALE_CONTEXT","The Fill changed while its rule editor was open; reopen it");
                            const auto object=host.session.document().objects.find(object_id);
                            if(object==host.session.document().objects.end())throw Error("MISSING_OBJECT",object_id);
                            const auto current=std::find_if(object->second.stack.begin(),object->second.stack.end(),
                                [&](const auto& candidate){return candidate.id==operation_id&&candidate.type=="nect.paint.fill";});
                            if(current==object->second.stack.end())throw Error("MISSING_OPERATION",operation_id);
                            const auto selected=mode->currentData().toString();std::vector<Command> commands;
                            if(selected=="link") {
                                if(source->currentIndex()<0)throw Error("MISSING_REFERENCE","Choose a visible Fill rule source");
                                const auto source_index=source->currentData().toInt();
                                if(source_index<0||static_cast<std::size_t>(source_index)>=source_refs.size())
                                    throw Error("MISSING_REFERENCE","Choose a valid Fill rule source");
                                commands.push_back(LinkFillRule{target,source_refs.at(static_cast<std::size_t>(source_index)),state.driver.has_value()});
                            } else if(selected=="unlink") {
                                commands.push_back(UnlinkFillRule{target});
                            } else {
                                if(state.driver&&!unlink_edit->isChecked())throw Error("DRIVEN_PROPERTY","Select unlink before editing a linked Fill rule");
                                if(state.driver)commands.push_back(UnlinkFillRule{target});
                                const auto next=value->currentData().toString().toStdString();
                                const auto frozen=state.driver?state.evaluated:state.literal;
                                if(next!=frozen)commands.push_back(OperationOptions{object_id,operation_id,current->composite,next});
                            }
                            if(!commands.empty()){host.session.apply(commands,frozen_revision);host.edited();}
                            dialog.accept();
                        } catch(const std::exception& error) {status->setText(QString::fromUtf8(error.what()));}
                    });
                dialog.exec();
            });
            rule_layout->addStretch();form->addRow("Fill rule",rule_row);
            QString driver_name="none";
            if(state.driver) {
                const auto found=host.session.document().objects.find(state.driver->link.object);
                const auto source_name=found==host.session.document().objects.end()?qs(state.driver->link.object):qs(found->second.name);
                driver_name=QString("link to %1 / %2").arg(source_name,qs(state.driver->link.field));
            }
            auto* state_label=new QLabel(QString("Literal: %1 · Driver: %2 · Evaluated: %3")
                .arg(qs(state.literal),driver_name,qs(state.evaluated)));
            state_label->setObjectName("operation-fill-rule-state-"+qs(operation.id));
            state_label->setWordWrap(true);state_label->setTextFormat(Qt::PlainText);form->addRow("",state_label);
        } else if(operation.type=="nect.shape.offset") {
            auto* rule=new QComboBox;rule->setObjectName("operation-fill-rule-"+qs(operation.id));
            rule->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            rule->setMinimumContentsLength(10);rule->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
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
            form->addRow(operation.gradient&&gradient_enabled_state(host.session.document(),
                gradient_ref(object.id,operation.id,operation.gradient->id,"enabled")).evaluated?"Solid fallback":"sRGB",color_row);
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
        } else if(operation.type=="nect.group.posterize") {
            add_property(form,operation_ref(object.id,operation.id,"levels"),QStringLiteral("Levels"));
            auto* note=new QLabel("Quantizes the postchildren Group pixels before the Group mask, opacity and blend.");
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
                for(auto* input:form->parentWidget()->findChildren<QLineEdit*>())
                    if(input->property("nect-reference").toByteArray()==reference){numeric=input;break;}
                auto* dial_row=new QWidget;auto* dial_layout=new QHBoxLayout(dial_row);dial_layout->setContentsMargins(0,0,0,0);dial_layout->setSpacing(8);
                auto* knob=new RotationKnob(dial_row);knob->setObjectName("repeater-angle-knob-"+qs(operation.id));
                knob->setProperty("nect-reference",reference);
                const auto initial_rotation=inspector_values_.at(rotation_ref);knob->set_value(initial_rotation);
                if(numeric){numeric->setProperty("nect-exact-value",true);numeric->setText(QString::number(initial_rotation,'g',17));numeric->setModified(false);}
                const auto& scalar=nect::property(host.session.document(),rotation_ref);
                const bool driven=scalar.binding.has_value()||scalar.expression.has_value();
                knob->setEnabled(!driven);
                if(driven)knob->setToolTip("Rotation is driven by a binding or expression. Unlink it in the numeric editor before using the dial.");
                auto* dial_note=new QLabel("Dial · modulo 360",dial_row);dial_note->setAccessibleName("Dial shows rotation modulo 360; numeric value is exact");
                dial_layout->addWidget(knob);dial_layout->addWidget(dial_note);dial_layout->addStretch();form->addRow("Angle dial",dial_row);
                const auto frozen_kind=host.session.document().objects.at(object.id).kind;
                const auto frozen_source=host.session.document().objects.at(object.id).source;
                const auto frozen_operation_version=operation.version;
                auto validate_target=[this,rotation_ref,object_id=object.id,operation_id=operation.id,frozen_kind,frozen_source,frozen_operation_version] {
                    const auto found=host.session.document().objects.find(object_id);
                    if(found==host.session.document().objects.end()||found->second.kind!=frozen_kind||
                       found->second.source.has_value()!=frozen_source.has_value()||
                       (frozen_source&&(found->second.source->id!=frozen_source->id||found->second.source->type!=frozen_source->type||
                           found->second.source->version!=frozen_source->version)))
                        throw Error("MISSING_PROPERTY","Repeater source identity changed");
                    const auto& current=find_operation(host.session.document(),object_id,operation_id);
                    if(current.type!="nect.shape.repeater"||current.version!=frozen_operation_version||!current.parameters.contains("rotation"))
                        throw Error("MISSING_PROPERTY","Repeater rotation Ref is no longer available");
                    const auto& current_scalar=nect::property(host.session.document(),rotation_ref);
                    if(current_scalar.binding||current_scalar.expression)
                        throw Error("DRIVEN_PROPERTY","Unlink the Repeater rotation before using the dial");
                };
                bind_angle_adapter(knob,numeric,rotation_ref,initial_rotation,std::move(validate_target),true,true);
            }
            auto* note=new QLabel("Rotation is a fixed step per copy; changing Copies does not divide 360°. Scale 1 is unchanged. Copies remain virtual and share source points.");
            note->setWordWrap(true);note->setStyleSheet("color: #a4acb8; font-size: 11px;");form->addRow(note);
        }
    }
}
void Window::add_gradient(QFormLayout* form,const Object& object,const ShapeOperation& operation) {
    const auto id=object.id,op=operation.id;
    const auto frozen_session=host.session_id;
    const auto& document=host.session.document();
    auto apply=[this,frozen_session](const std::vector<Command>& commands) {
        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The gradient belongs to another document");
        host.session.apply(commands,host.session.revision());host.edited();
    };
    auto* mode=new QComboBox;mode->setObjectName("gradient-mode-"+qs(op));
    mode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);mode->setMinimumContentsLength(10);
    mode->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    mode->addItem("Solid");mode->addItem("Linear gradient");mode->addItem("Radial gradient");
    const auto gradient_ref_value=operation.gradient?std::optional<Ref>{gradient_ref(id,op,operation.gradient->id,"enabled")}:std::nullopt;
    const auto gradient_state=gradient_ref_value?std::optional<GradientEnabledProperty>{gradient_enabled_state(document,*gradient_ref_value)}:std::nullopt;
    const auto gradient_active=gradient_state?gradient_state->evaluated:false;
    const bool gradient_driven=gradient_state&&(gradient_state->driver.has_value()||gradient_state->expression.has_value());
    mode->setCurrentIndex(!operation.gradient||!gradient_active?0:operation.gradient->type=="radial"?2:1);
    mode->setEnabled(!gradient_driven);
    if(gradient_driven)mode->setToolTip("Unlink the Gradient enabled source before changing Paint mode.");
    form->addRow("Paint",mode);
    connect(mode,&QComboBox::currentIndexChanged,this,[this,mode,id,op,apply,before=mode->currentIndex()](int index) {
        bool applied=false;
        perform([&]{
            auto gradient=find_operation(host.session.document(),id,op).gradient;
            if(gradient&&(gradient->enabled_driver||gradient->enabled_expression))
                throw Error("DRIVEN_PROPERTY","Unlink the Gradient enabled source before changing Paint mode");
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
                        const auto layout=evaluate_text_projection(host.session.document(),id,values);bounds=QRectF(layout.x,layout.y,layout.width,layout.height);
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
    if(operation.gradient&&gradient_state&&gradient_ref_value) {
        auto* driver_row=new QWidget;auto* driver_layout=new QHBoxLayout(driver_row);driver_layout->setContentsMargins(0,0,0,0);
        auto* driver_button=new QToolButton(driver_row);driver_button->setObjectName("gradient-enabled-driver-"+qs(op));
        driver_button->setText(gradient_state->driver?"Driver…":gradient_state->expression?"Expression…":"Drive…");
        driver_button->setPopupMode(QToolButton::InstantPopup);
        auto* driver_menu=new QMenu(driver_button);driver_button->setMenu(driver_menu);driver_layout->addWidget(driver_button);
        const bool has_enabled_source=gradient_state->driver.has_value()||gradient_state->expression.has_value();
        auto* link_enabled=driver_menu->addAction(has_enabled_source?"Replace with link…":"Link enabled source…");
        auto* set_expression=driver_menu->addAction(has_enabled_source?"Replace with expression…":"Set enabled expression…");
        auto* unlink_enabled=driver_menu->addAction("Unlink and freeze evaluated value");
        unlink_enabled->setEnabled(has_enabled_source);driver_layout->addStretch();form->addRow("Gradient enabled",driver_row);
        const Composition* owner_composition=nullptr;
        const std::function<bool(const Id&)> contains_target=[&](const Id& source_id) {
            if(source_id==id)return true;
            for(const auto& child:document.objects.at(source_id).children)if(contains_target(child))return true;
            return false;
        };
        for(const auto& composition:document.compositions)if(
            std::any_of(composition.roots.begin(),composition.roots.end(),contains_target)) {
            owner_composition=&composition;break;
        }
        if(!owner_composition)throw Error("ORPHAN_OBJECT","Gradient Inspector target is not owned by a Composition");
        std::vector<Ref> source_refs;QStringList source_labels;
        std::function<void(const Id&)> collect_sources=[&](const Id& source_id) {
            const auto& source_object=document.objects.at(source_id);
            for(const auto& source_operation:source_object.stack)if(source_operation.gradient) {
                const auto ref=gradient_ref(source_id,source_operation.id,source_operation.gradient->id,"enabled");
                if(ref==*gradient_ref_value)continue;
                source_refs.push_back(ref);
                source_labels<<qs(source_object.name)+" — "+operation_label(source_operation)+" / Gradient ["+
                    qs(source_operation.gradient->id)+"] — "+qs(source_id);
            }
            for(const auto& child:source_object.children)collect_sources(child);
        };
        for(const auto& root:owner_composition->roots)collect_sources(root);
        link_enabled->setEnabled(!source_refs.empty());
        QString driver_source="none";
        if(gradient_state->driver) {
            const auto& source=*gradient_state->driver;const auto source_object=document.objects.find(source.object);
            driver_source=QString("%1 / %2").arg(source_object==document.objects.end()?qs(source.object):qs(source_object->second.name),qs(source.field));
        } else if(gradient_state->expression)driver_source="expression · "+qs(gradient_state->expression->source);
        auto* state_label=new QLabel(QString("Literal: %1 · Source: %2 · Evaluated: %3")
            .arg(gradient_state->literal?"true":"false",driver_source,gradient_state->evaluated?"true":"false"));
        state_label->setObjectName("gradient-enabled-state-"+qs(op));
        state_label->setWordWrap(true);state_label->setTextFormat(Qt::PlainText);form->addRow("",state_label);
        const auto frozen_revision=host.session.revision();const auto target_ref=*gradient_ref_value;
        connect(link_enabled,&QAction::triggered,this,[this,target_ref,frozen_session,frozen_revision,
            replace_driver=has_enabled_source,source_refs,source_labels] {
            choose_boolean_source(this,"gradient-enabled-source-dialog","Link Gradient enabled",
                qs(target_ref.object)+" / "+qs(target_ref.field),source_refs,source_labels,
                [this,target_ref,frozen_session,frozen_revision,replace_driver](const Ref& source) {
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Gradient belongs to another document");
                if(host.session.revision()!=frozen_revision)throw Error("STALE_CONTEXT","Gradient enabled source changed while its editor was open; reopen it");
                host.session.apply({LinkGradientEnabled{target_ref,source,replace_driver}},frozen_revision);
                host.edited();
            });
        });
        connect(set_expression,&QAction::triggered,this,[this,target_ref,frozen_session,frozen_revision,
            initial_expression=gradient_state->expression,has_enabled_source,operation_id=op] {
            QDialog dialog(this);dialog.setObjectName("gradient-enabled-expression-dialog-"+qs(operation_id));
            dialog.setWindowTitle("Gradient enabled expression");auto* layout=new QVBoxLayout(&dialog);
            auto* expression=new QPlainTextEdit(&dialog);
            expression->setObjectName("gradient-enabled-expression-source-"+qs(operation_id));
            expression->setPlaceholderText("true, false, ref(\"object-id\",\"\",\"op.paint-operation.gradient.gradient-id.enabled\"), or !ref(…)");
            expression->setPlainText(initial_expression?qs(initial_expression->source):"true");
            expression->setMinimumHeight(72);layout->addWidget(expression);
            auto* replace=new QCheckBox("Replace existing source",&dialog);replace->setObjectName("gradient-enabled-expression-replace-"+qs(operation_id));
            replace->setEnabled(has_enabled_source);layout->addWidget(replace);
            auto* status=new QLabel("Use true, false, or a Gradient enabled Ref in this Composition. Apply commits one command; Cancel leaves the current source and revision unchanged.",&dialog);
            status->setObjectName("gradient-enabled-expression-status-"+qs(operation_id));status->setWordWrap(true);layout->addWidget(status);
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
            connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
            connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,
                [this,&dialog,target_ref,frozen_session,frozen_revision,expression,replace,status] {
                    try {
                        if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Gradient belongs to another document");
                        if(host.session.revision()!=frozen_revision)
                            throw Error("STALE_CONTEXT","Gradient enabled source changed while its editor was open; reopen it");
                        host.session.apply({SetGradientEnabledExpression{target_ref,
                            {expression->toPlainText().toStdString(),1},replace->isChecked()}},frozen_revision);
                        host.edited();dialog.accept();
                    } catch(const std::exception& error) {status->setText(QString::fromUtf8(error.what()));}
                });
            dialog.exec();
        });
        connect(unlink_enabled,&QAction::triggered,this,[this,target_ref,frozen_session,frozen_revision] {
            perform([&]{
                if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Gradient belongs to another document");
                if(host.session.revision()!=frozen_revision)throw Error("STALE_CONTEXT","Gradient enabled state changed while its editor was open; reopen it");
                host.session.apply({UnlinkGradientEnabled{target_ref}},frozen_revision);host.edited();
            });
        });
    }
    if(!operation.gradient||!gradient_active)return;
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
void Window::add_primitive_angle(QFormLayout* form,const Ref& ref,const Primitive& source) {
    const auto reference=QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact);
    QPointer<QLineEdit> numeric;
    for(auto* input:form->parentWidget()->findChildren<QLineEdit*>())
        if(input->property("nect-reference").toByteArray()==reference){numeric=input;break;}
    auto* row=new QWidget;auto* layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);
    QPointer<RotationKnob> knob=new RotationKnob(row);
    knob->setObjectName("primitive-angle-knob");knob->setAccessibleName(primitive_label(source)+" source rotation angle knob");
    knob->set_display_zero(0);knob->setProperty("nect-reference",reference);
    const auto initial=inspector_values_.at(ref);knob->set_value(initial);
    // This angle row promises exact numeric round-trip, unlike generic compact labels.
    if(numeric){numeric->setProperty("nect-exact-value",true);if(!numeric->isModified()){numeric->setText(QString::number(initial,'g',17));numeric->setModified(false);}}
    const auto& scalar=nect::property(host.session.document(),ref);
    const bool driven=scalar.binding.has_value()||scalar.expression.has_value();knob->setEnabled(!driven);
    knob->setToolTip(driven?"Rotation is driven. Unlink its numeric source before using the dial.":
        "Zero points right (+X); positive degrees turn clockwise. Drag adds signed degrees; whole turns stay authored. Escape cancels.");
    layout->addWidget(knob);layout->addWidget(new QLabel("Dial · +X zero · modulo 360",row));layout->addStretch();form->addRow("Angle dial",row);
    const auto source_id=source.id,source_type=source.type;const auto source_version=source.version;
    auto validate_target=[this,ref,source_id,source_type,source_version] {
        const auto object=host.session.document().objects.find(ref.object);
        if(object==host.session.document().objects.end()||!object->second.source||object->second.source->id!=source_id||
           object->second.source->type!=source_type||object->second.source->version!=source_version)
            throw Error("MISSING_PROPERTY","Primitive source identity changed");
        const auto& current=nect::property(host.session.document(),ref);
        if(current.binding||current.expression)throw Error("DRIVEN_PROPERTY","Unlink the primitive rotation before using the dial");
    };
    bind_angle_adapter(knob,numeric,ref,initial,std::move(validate_target),false,true);
}

void Window::add_point_angle(QFormLayout* form,const Ref& ref,const Object& object) {
    const auto encoded=QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact);
    QPointer<QLineEdit> numeric;
    for(auto* input:form->parentWidget()->findChildren<QLineEdit*>())
        if(input->property("nect-reference").toByteArray()==encoded){numeric=input;break;}

    auto* row=new QWidget;auto* row_layout=new QHBoxLayout(row);row_layout->setContentsMargins(0,0,0,0);
    QPointer<RotationKnob> knob=new RotationKnob(row);
    knob->setObjectName("point-angle-knob-"+qs(ref.field));
    const auto handle_name=ref.field=="in.angle"?QStringLiteral("incoming handle"):QStringLiteral("outgoing handle");
    knob->setAccessibleName(qs(object.name)+" "+handle_name+" angle knob");
    knob->set_display_zero(0);knob->setProperty("nect-reference",encoded);
    const auto initial=inspector_values_.at(ref);knob->set_value(initial);
    // Point angles use exact signed degrees just like their adjacent numeric field.
    if(numeric) {
        numeric->setProperty("nect-exact-value",true);
        if(!numeric->isModified()) {numeric->setText(QString::number(initial,'g',17));numeric->setModified(false);}
    }

    const auto& document=host.session.document();
    const auto origin=property_origin(document,ref);
    const bool generated=object.source.has_value()&&origin!="authored";
    bool driven=false;
    if(origin!="generated") {
        const auto& scalar=nect::property(document,ref);
        driven=scalar.binding.has_value()||scalar.expression.has_value();
    }
    knob->setEnabled(!driven);
    knob->setToolTip(driven
        ?"This handle angle has a stored binding or expression. Unlink it before using the dial."
        :"Zero points right along local +X; positive degrees turn clockwise. Drag adds signed degrees; whole turns stay authored. Escape cancels.");
    row_layout->addWidget(knob);row_layout->addWidget(new QLabel("Dial · +X zero · modulo 360",row));row_layout->addStretch();
    form->addRow(handle_name+" dial",row);

    const auto object_id=object.id;
    const auto contour_id=[&] {
        const auto contours=object.source?path_contours(object,&inspector_values_):object.contours;
        for(const auto& contour:contours)
            if(std::any_of(contour.points.begin(),contour.points.end(),[&](const auto& point){return point.id==ref.point;}))
                return contour.id;
        throw Error("MISSING_PROPERTY","Selected point identity is no longer present");
    }();
    const auto source_id=object.source?object.source->id:std::string{};
    const auto source_type=object.source?object.source->type:std::string{};
    const auto source_version=object.source?object.source->version:0U;
    const bool had_point_edit=object.point_edit.has_value();
    const auto point_edit_id=object.point_edit?object.point_edit->id:
        object.source?object.source->id+"-point-edit":std::string{};
    const auto point_edit_version=object.point_edit?object.point_edit->version:1U;
    auto validate_target=[this,object_id,ref,generated,contour_id,source_id,source_type,source_version,
        had_point_edit,point_edit_id,point_edit_version] {
        const auto found=host.session.document().objects.find(object_id);
        if(found==host.session.document().objects.end()||found->second.kind!=Kind::path)
            throw Error("MISSING_PROPERTY","Selected point object identity changed");
        const auto& current=found->second;
        if(generated) {
            if(!current.source||current.source->id!=source_id||current.source->type!=source_type||
               current.source->version!=source_version)
                throw Error("MISSING_PROPERTY","Primitive source identity changed");
            if(current.point_edit.has_value()!=had_point_edit||
               (current.point_edit&&(current.point_edit->id!=point_edit_id||current.point_edit->version!=point_edit_version)))
                throw Error("MISSING_PROPERTY","Canonical Point Edit destination changed");
        } else if(current.source)throw Error("MISSING_PROPERTY","Authored Path source identity changed");
        const auto values=evaluate(host.session.document());
        if(!values.contains(ref))throw Error("MISSING_PROPERTY","Selected point or angle Ref is no longer active");
        const auto current_contours=current.source?path_contours(current,&values):current.contours;
        bool same_point=false;
        for(const auto& contour:current_contours)if(contour.id==contour_id&&
            std::any_of(contour.points.begin(),contour.points.end(),[&](const auto& point){return point.id==ref.point;}))
            same_point=true;
        if(!same_point)throw Error("MISSING_PROPERTY","Selected point identity changed");
        if(property_origin(host.session.document(),ref)!="generated") {
            const auto& scalar=nect::property(host.session.document(),ref);
            if(scalar.binding||scalar.expression)
                throw Error("DRIVEN_PROPERTY","Unlink the handle angle before using its dial");
        }
    };
    bind_angle_adapter(knob,numeric,ref,initial,std::move(validate_target),false,true);
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

void Window::align_selection(const std::string& axis,const std::string& alignment,const std::string& reference,
    const std::optional<Id>& guide_artboard) {
    if(std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& selection){return !selection.point.empty();}))
        throw Error("INVALID_SELECTION","Select whole objects to align their bounds");
    const auto revision=host.session.revision();
    host.session.apply({AlignObjects{canvas->selected_objects(),axis,alignment,{},reference,guide_artboard}},revision);
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
    for(const auto& board:active_composition.artboards) {
        const auto frame=evaluate_artboard(active_composition,board.id);
        if(frame.layout&&frame.layout->grid)
            alignment_target->addItem(QString("Grid: %1 · Artboard: %2 (%3)")
                .arg(qs(frame.layout->grid->id),qs(board.name),qs(board.id)),qs("grid:"+frame.layout->grid->id));
    }
    const auto guide_positions=evaluate_guide_positions(d,active_composition.id);
    for(const auto& guide:active_composition.guides)
        alignment_target->addItem(QString("Guide: %1 (%2) · %3=%4")
            .arg(qs(guide.name),qs(guide.id),qs(guide.axis),QString::number(guide_positions.at(guide.id),'g',15)),qs("guide:"+guide.id));
    for(const auto& board:active_composition.artboards) {
        const auto frame=evaluate_artboard(active_composition,board.id);
        for(const auto& guide:effective_artboard_guides(d,active_composition.id,board.id))if(guide.enabled) {
            const auto position=(guide.axis=="x"?frame.x:frame.y)+guide.position;
            alignment_target->addItem(QString("Guide: %1 (%2) · Artboard: %3 (%4) · %5=%6")
                .arg(qs(guide.name),qs(guide.guide_id),qs(board.name),qs(board.id),qs(guide.axis),QString::number(position,'g',15)),
                qs("guide:"+guide.guide_id));
            alignment_target->setItemData(alignment_target->count()-1,qs(board.id),Qt::UserRole+1);
            alignment_target->setItemData(alignment_target->count()-1,qs(guide.axis),Qt::UserRole+2);
        }
    }
    for(int index=0;index<alignment_target->count();++index)
        alignment_target->setItemData(index,alignment_target->itemText(index),Qt::ToolTipRole);
    int target_index=-1;
    for(int i=0;i<alignment_target->count();++i) {
        const auto scope=alignment_target->itemData(i,Qt::UserRole+1).toString();
        if(alignment_target->itemData(i).toString()==qs(alignment_reference_)&&
            (alignment_guide_artboard_?scope==qs(*alignment_guide_artboard_):scope.isEmpty())){target_index=i;break;}
    }
    if(target_index<0){alignment_reference_="selection";alignment_guide_artboard_.reset();target_index=0;}
    alignment_target->setCurrentIndex(target_index);
    alignment_target->setToolTip(alignment_target->itemText(target_index));
    alignment_layout->addWidget(alignment_target);
    connect(alignment_target,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,alignment_target](int index){
        alignment_reference_=alignment_target->itemData(index).toString().toStdString();
        const auto scope=alignment_target->itemData(index,Qt::UserRole+1).toString();
        alignment_guide_artboard_=scope.isEmpty()?std::optional<Id>{}:std::optional<Id>{scope.toStdString()};
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
                const auto scope=alignment_target->currentData(Qt::UserRole+1).toString();
                perform([&]{align_selection(axis,mode,reference,
                    scope.isEmpty()?std::optional<Id>{}:std::optional<Id>{scope.toStdString()});});
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
        const auto scope=alignment_target->currentData(Qt::UserRole+1).toString();
        const auto guide_axis=scope.isEmpty()?(guide!=composition.guides.end()?qs(guide->axis):QString{}):
            alignment_target->currentData(Qt::UserRole+2).toString();
        for(const auto axis:{"x","y"})for(const auto mode:{"min","center","max"})
            if(auto* button=alignment_box->findChild<QPushButton*>(QString("quick-align-%1-%2").arg(axis,mode)))
                button->setEnabled(ordinary&&(!reference.starts_with("guide:")||guide_axis==QString::fromLatin1(axis)));
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
    add_multi_text_weight(layout,selected);
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

void Window::add_multi_text_weight(QVBoxLayout* layout,const std::vector<Canvas::Selection>& selected) {
    const auto& document=host.session.document();
    if(selected.size()<2||std::any_of(selected.begin(),selected.end(),[&](const auto& item) {
        const auto found=document.objects.find(item.object);
        return !item.point.empty()||found==document.objects.end()||found->second.kind!=Kind::text||!found->second.text;
    }))return;

    std::vector<Ref> targets;std::set<Id> target_ids;std::vector<unsigned> evaluated;
    bool any_driven=false;
    for(const auto& item:selected) {
        const Ref ref{item.object,"","text.weight"};targets.push_back(ref);target_ids.insert(item.object);
        const auto state=text_weight_property(document,ref);evaluated.push_back(state.evaluated);
        any_driven=any_driven||state.driver.has_value()||state.expression.has_value();
    }
    const auto frozen_session=host.session_id;const auto frozen_revision=host.session.revision();
    const auto composition=canvas->active_composition(),artboard=canvas->active_artboard();
    auto* group=new QGroupBox(QString("Text weight · %1 selected Text objects").arg(targets.size()));
    group->setObjectName("text-weight-batch-panel");auto* column=new QVBoxLayout(group);
    auto* note=new QLabel("Enter one absolute value or += / -= to preserve each selected Text's weight difference.");
    note->setWordWrap(true);column->addWidget(note);
    auto* edit_row=new QHBoxLayout;auto* value=new QLineEdit(group);value->setObjectName("text-weight-batch-value");
    value->setAccessibleName("Shared Text weight draft");value->setPlaceholderText("400, +=10, or -=10");
    if(std::all_of(evaluated.begin(),evaluated.end(),[&](unsigned current){return current==evaluated.front();}))
        value->setText(QString::number(evaluated.front()));
    edit_row->addWidget(value);
    auto* apply=new QPushButton("Apply",group);apply->setObjectName("text-weight-batch-apply");
    apply->setEnabled(!any_driven);if(any_driven)apply->setToolTip("Unlink driven Text weights before editing the batch.");
    edit_row->addWidget(apply);
    auto* cancel=new QPushButton("Cancel",group);cancel->setObjectName("text-weight-batch-cancel");edit_row->addWidget(cancel);
    column->addLayout(edit_row);

    auto* source_row=new QHBoxLayout;
    auto* drive=new QToolButton(group);drive->setObjectName("text-weight-batch-driver");drive->setText(any_driven?"Source…":"Drive…");
    drive->setPopupMode(QToolButton::InstantPopup);auto* menu=new QMenu(drive);drive->setMenu(menu);
    auto* absolute_link=menu->addAction("Link absolute source…");
    auto* relative_link=menu->addAction("Link relative source…");source_row->addWidget(drive);
    auto* replace=new QCheckBox("Replace current sources",group);replace->setObjectName("text-weight-batch-replace");
    replace->setVisible(any_driven);source_row->addWidget(replace);
    auto* unlink=new QPushButton("Unlink",group);unlink->setObjectName("text-weight-batch-unlink");
    unlink->setEnabled(any_driven);source_row->addWidget(unlink);source_row->addStretch();column->addLayout(source_row);
    auto* status=new QLabel("The target Refs, Session and revision are captured for this draft.",group);
    status->setObjectName("text-weight-batch-status");status->setWordWrap(true);column->addWidget(status);
    layout->addWidget(group);

    const auto parse_draft=[](const QString& raw)->std::pair<std::int64_t,bool> {
        const auto text=raw.trimmed();
        const bool relative=text.startsWith("+=")||text.startsWith("-=");
        const auto digits=relative?text.mid(2):text;
        if(digits.isEmpty()||std::any_of(digits.begin(),digits.end(),[](QChar ch){return ch<'0'||ch>'9';}))
            throw Error("INVALID_INTEGER","Enter an integer weight, +=integer or -=integer");
        bool ok=false;const auto magnitude=digits.toLongLong(&ok,10);
        if(!ok)throw Error("OUT_OF_RANGE","Text weight edit amount must fit a signed 64-bit integer");
        const auto amount=relative&&text.startsWith("-=")?-magnitude:magnitude;
        if(!relative&&(amount<1||amount>999))throw Error("OUT_OF_RANGE","Font weight must be 1..999");
        return {static_cast<std::int64_t>(amount),relative};
    };
    connect(apply,&QPushButton::clicked,this,[this,value,status,targets,frozen_session,frozen_revision,parse_draft] {
        try {
            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text weight draft belongs to another document");
            if(host.session.revision()!=frozen_revision)throw Error("REVISION_CONFLICT","Text weights changed while the draft was open");
            const auto [amount,relative]=parse_draft(value->text());canvas->cancel_interaction();
            host.session.apply({TextWeightBatch{TextWeightBatchMode::edit,targets,amount,{},relative,false}},frozen_revision);
            host.edited();
        } catch(const Error& error) {status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
          catch(const std::exception& error) {status->setText(QString::fromUtf8(error.what()));}
    });
    connect(cancel,&QPushButton::clicked,this,[value,evaluated,status] {
        value->clear();QStringList values;for(const auto item:evaluated)values.push_back(QString::number(item));
        status->setText("Draft cancelled; committed weights remain "+values.join(", ")+".");
    });
    connect(unlink,&QPushButton::clicked,this,[this,targets,frozen_session,frozen_revision,status] {
        try {
            if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text weights belong to another document");
            if(host.session.revision()!=frozen_revision)throw Error("REVISION_CONFLICT","Text weights changed while the Inspector was open");
            canvas->cancel_interaction();host.session.apply({TextWeightBatch{TextWeightBatchMode::unlink,targets}},frozen_revision);host.edited();
        } catch(const Error& error) {status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
          catch(const std::exception& error) {status->setText(QString::fromUtf8(error.what()));}
    });

    std::vector<Id> source_ids;
    for(const auto& [id,object]:document.objects)
        if(object.kind==Kind::text&&object.text&&!target_ids.contains(id))source_ids.push_back(id);
    absolute_link->setEnabled(!source_ids.empty());relative_link->setEnabled(!source_ids.empty());
    const auto open_picker=[this,targets,target_ids,source_ids,selected,frozen_session,frozen_revision,composition,artboard](bool relative,bool replace_driver) {
        auto picker=make_text_source_picker(this,host.session.document(),targets.front().object,"text.weight",source_ids,
            relative?"Link relative Text weights":"Link absolute Text weights");
        auto* dialog=picker.dialog;auto* list=picker.list;auto* picker_status=picker.status;
        connect(list,&QListWidget::currentItemChanged,this,[this,frozen_session](QListWidgetItem* item,QListWidgetItem*) {
            if(!item||item->isHidden()||host.session_id!=frozen_session)return;
            const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
            if(host.session.document().objects.contains(source.object))canvas->set_selection(source.object,{});
        });
        connect(dialog,&QDialog::rejected,this,[this,selected,frozen_session,composition,artboard] {
            if(host.session_id==frozen_session) {
                perform([&]{canvas->set_active_artboard(composition,artboard,false);});canvas->set_selections(selected);
            }
        });
        connect(picker.buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,dialog,
            [this,dialog,list,picker_status,targets,target_ids,frozen_session,frozen_revision,relative,replace_driver,selected,composition,artboard] {
                try {
                    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Text source chooser belongs to another document");
                    if(host.session.revision()!=frozen_revision)throw Error("REVISION_CONFLICT","Text weights changed while the source chooser was open");
                    auto* item=list->currentItem();if(!item||item->isHidden())throw Error("NO_SOURCE","Choose a visible Text source");
                    const auto source=read_ref(item->data(Qt::UserRole).toByteArray());
                    if(source.point!=""||source.field!="text.weight"||target_ids.contains(source.object))
                        throw Error("INVALID_REFERENCE","Choose a distinct Text weight source for every target");
                    canvas->cancel_interaction();
                    host.session.apply({TextWeightBatch{TextWeightBatchMode::link,targets,0,source,relative,replace_driver}},frozen_revision);
                    canvas->set_active_artboard(composition,artboard,false);canvas->set_selections(selected);host.edited();dialog->accept();
                } catch(const Error& error) {picker_status->setText(qs(error.code)+": "+QString::fromUtf8(error.what()));}
                  catch(const std::exception& error) {picker_status->setText(QString::fromUtf8(error.what()));}
            });
        dialog->show();picker.search->setFocus();
    };
    connect(absolute_link,&QAction::triggered,this,[open_picker,replace]{open_picker(false,replace->isChecked());});
    connect(relative_link,&QAction::triggered,this,[open_picker,replace]{open_picker(true,replace->isChecked());});
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
        const auto scalar_values=evaluate(host.session.document());
        for(const auto& ref:properties(host.session.document())) {
            if(!scalar_values.contains(ref))continue;
            auto* item=new QListWidgetItem(property_label(host.session.document(),ref),list);item->setData(Qt::UserRole,expression_ref(ref));
            item->setData(Qt::UserRole+1,qs(ref.object+"/"+ref.point+"/"+ref.field));
        }
        connect(search,&QLineEdit::textChanged,dialog,[list](const QString& text){const auto terms=text.split(' ',Qt::SkipEmptyParts);for(int i=0;i<list->count();++i){
            auto* item=list->item(i);const auto searchable=item->text()+" "+item->data(Qt::UserRole+1).toString();
            item->setHidden(!std::all_of(terms.begin(),terms.end(),[&](const auto& term){return searchable.contains(term,Qt::CaseInsensitive);}));
        }if(list->currentItem()&&list->currentItem()->isHidden())list->setCurrentItem(nullptr);});
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
                catch(const Error& e){if(e.code=="REVISION_CONFLICT"||e.code=="SESSION_CONFLICT")throw;expand(text);input->setText(input->property("nect-exact-value").toBool()?QString::number(inspector_values_.at(ref),'g',17):display_value(inspector_values_.at(ref)));}
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
    auto* dialog=new QDialog(this);dialog->setObjectName("property-source-picker");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(720,480);
    dialog->setWindowTitle(relative?"Pick Relative Link source":"Pick property source");
    auto* layout=new QVBoxLayout(dialog);
    auto* target_note=new QLabel(targets.size()==1?"Target: "+property_label(host.session.document(),target):QString::number(targets.size())+" frozen targets · "+qs(target.field));
    target_note->setWordWrap(true);layout->addWidget(target_note);
    auto* search=new QLineEdit;search->setObjectName("property-source-picker-search");search->setPlaceholderText("Search object, point, property or unit…");layout->addWidget(search);
    auto* list=new QListWidget;list->setObjectName("property-source-picker-list");layout->addWidget(list);
    const auto values=evaluate(host.session.document());
    for(const auto& ref:properties(host.session.document())) {
        if(!values.contains(ref))continue;
        if(std::find(targets.begin(),targets.end(),ref)!=targets.end())continue;
        const auto text=property_label(host.session.document(),ref)+" ["+qs(property_unit(ref))+", local]  = "+display_value(values.at(ref));
        const auto stable_path=qs(ref.object+" / "+ref.point+" / "+ref.field);
        auto* item=new QListWidgetItem(text,list);item->setData(Qt::UserRole,QJsonDocument(ref_json(ref)).toJson(QJsonDocument::Compact));
        item->setData(Qt::UserRole+1,stable_path);item->setToolTip(stable_path);
        if(property_unit(target)!=property_unit(ref)) {
            item->setFlags(item->flags()&~Qt::ItemIsEnabled);
            item->setToolTip(stable_path+"\nIncompatible unit: "+qs(property_unit(ref)));
        }
    }
    connect(search,&QLineEdit::textChanged,dialog,[list](const QString& text){
        const auto terms=text.split(' ',Qt::SkipEmptyParts);
        for(int i=0;i<list->count();++i) {
            auto* item=list->item(i);
            const auto searchable=item->text()+" "+item->data(Qt::UserRole+1).toString();
            const bool matches=std::all_of(terms.begin(),terms.end(),[&](const auto& term){return searchable.contains(term,Qt::CaseInsensitive);});
            if(!matches&&list->currentItem()==item)list->setCurrentItem(nullptr);
            item->setHidden(!matches);
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
            if(!list->currentItem()||list->currentItem()->isHidden()||!(list->currentItem()->flags()&Qt::ItemIsEnabled))
                throw Error("NO_SOURCE","Choose a visible compatible source property");
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
    auto source=default_primitive(new_id(),type);
    source.parameters.at("center_x").literal=artboard.x+artboard.width/2;
    source.parameters.at("center_y").literal=artboard.y+artboard.height/2;
    const auto id=new_id();
    const auto name=primitive_label(source).toStdString()+" "+std::to_string(document.objects.size()+1);
    host.session.apply({CreatePrimitive{composition.id,{},id,name,std::move(source)}},host.session.revision());
    canvas->set_selection(id);host.edited();canvas->setFocus();
}
void Window::apply_builtin_effect_favorite(const BuiltinEffectTypeRefV1& effect,
    const QString& expected_session, const Id& expected_target, std::uint64_t expected_revision,
    std::uint64_t expected_generation) {
    if(host.session_id!=expected_session)
        throw Error("SESSION_CONFLICT","Effects target belongs to another document");
    if(expected_generation!=effects_generation_)
        throw Error("REVISION_CONFLICT","Effects panel changed; refresh the target before applying");
    if(canvas->selected_object!=expected_target)
        throw Error("TARGET_CONFLICT","Effects target changed; choose the current target");
    if(host.session.revision()!=expected_revision)
        throw Error("REVISION_CONFLICT","Effects target changed elsewhere; refresh the panel before applying");

    const auto* descriptor=builtin_operation_type(effect.type_id.toStdString());
    if(!descriptor||!descriptor->effects_catalog)
        throw Error("UNAVAILABLE_EFFECT_TYPE","The Favorite TypeID is not in the built-in Effects catalog");
    if(descriptor->version!=effect.behavior_version)
        throw Error("EFFECT_BEHAVIOR_VERSION_MISMATCH","The Favorite requires behavior v"+
            std::to_string(effect.behavior_version)+" but the registered Effect provides v"+
            std::to_string(descriptor->version));

    // Preserve the normal Effects path, including its target compatibility checks,
    // default operation construction, Session revision check and History entry.
    add_operation(effect.type_id.toStdString());
}

void Window::add_operation(const std::string& type,bool radial) {
    canvas->cancel_interaction();
    const auto& document=host.session.document();
    const auto found=document.objects.find(canvas->selected_object);
    if(found==document.objects.end())throw Error("INVALID_DOMAIN","Select an existing object before applying an effect");
    const bool group_posterize=type=="nect.group.posterize"&&found->second.kind==Kind::group;
    const bool shape_operation=type!="nect.group.posterize"&&
        (found->second.kind==Kind::path||found->second.kind==Kind::text);
    if(!group_posterize&&!shape_operation)
        throw Error("INVALID_DOMAIN",type=="nect.group.posterize"?
            "Group Posterize requires a Group target.":"Shape operations require a Path, primitive or Text target.");
    const auto& object=found->second;
    auto operation=default_operation(new_id(),type);
    if(radial) {
        const auto values=evaluate(document);
        double center_x=0,center_y=0;
        if(object.source) {
            center_x=values.at({object.id,{},"generator.center_x"});
            center_y=values.at({object.id,{},"generator.center_y"});
        } else if(object.text) {
            const auto layout=evaluate_text_projection(document,object.id,values);center_x=layout.x+layout.width/2;center_y=layout.y+layout.height/2;
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
    if(object.point_edit && !evaluate_point_edit_enabled(document,point_edit_enabled_ref(object.id,object.point_edit->id))) {
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
void Window::update_batch_rename_action() {
    if(!batch_rename_action_)return;
    try{Id parent;(void)selected_siblings(parent,2);batch_rename_action_->setEnabled(true);}
    catch(const Error&){batch_rename_action_->setEnabled(false);}
}
void Window::update_sort_paint_order_action() {
    if(!sort_paint_order_action_)return;
    try{Id parent;(void)selected_siblings(parent,2);sort_paint_order_action_->setEnabled(true);}
    catch(const Error&){sort_paint_order_action_->setEnabled(false);}
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
void Window::move_selection_out() {
    if(canvas->selected_objects().empty()||
       std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return !item.point.empty();}))
        throw Error("MOVE_OUT_SELECTION","Select whole objects, not points, inside a Folder");
    Id folder;std::vector<Id> members;
    try{members=selected_siblings(folder,1);}
    catch(const Error&){throw Error("MOVE_OUT_SELECTION","Select a contiguous prefix or suffix of children inside a Folder");}
    const auto& document=host.session.document();
    if(folder.empty()||!document.objects.contains(folder)||document.objects.at(folder).kind!=Kind::group)
        throw Error("MOVE_OUT_SELECTION","Select children inside a Folder");
    const auto& children=document.objects.at(folder).children;
    const bool prefix=members.size()<=children.size()&&std::equal(members.begin(),members.end(),children.begin());
    const bool suffix=members.size()<=children.size()&&
        std::equal(members.begin(),members.end(),children.begin()+static_cast<std::ptrdiff_t>(children.size()-std::min(children.size(),members.size())));
    if(!prefix&&!suffix)throw Error("MOVE_OUT_SELECTION","Move selected must be a contiguous prefix or suffix of Folder children");
    const auto placement=members.size()==children.size()||suffix?std::string("after"):std::string("before");
    const auto& composition=find_composition(document,canvas->active_composition());
    Id folder_parent;bool found=false;
    std::function<bool(const std::vector<Id>&,const Id&)> locate=[&](const std::vector<Id>& siblings,const Id& owner) {
        for(const auto& id:siblings) {
            if(id==folder){folder_parent=owner;return true;}
            if(locate(document.objects.at(id).children,id))return true;
        }
        return false;
    };
    found=locate(composition.roots,{});
    if(!found)throw Error("MOVE_OUT_SELECTION","Selected Folder is outside the active Composition");
    std::vector<Canvas::Selection> selection;for(const auto& id:members)selection.push_back({id,{}});
    canvas->cancel_interaction();
    host.session.apply({MoveOut{composition.id,folder_parent,folder,members,placement}},host.session.revision());
    canvas->set_selections(std::move(selection));host.edited();
    statusBar()->showMessage("Moved selected objects out of Folder; Undo restores the original structure",6000);
}
void Window::move_selection_to_next_folder() {
    if(canvas->selected_objects().empty()||
       std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return !item.point.empty();}))
        throw Error("FOLDER_TRANSFER_SELECTION","Select whole objects forming a suffix inside a Folder");
    Id source;std::vector<Id> members;
    try{members=selected_siblings(source,1);}
    catch(const Error&){throw Error("FOLDER_TRANSFER_SELECTION","Select siblings inside one Folder");}
    const auto& document=host.session.document();
    if(source.empty()||!document.objects.contains(source)||document.objects.at(source).kind!=Kind::group)
        throw Error("FOLDER_TRANSFER_SELECTION","Selection must be inside a Folder");
    const auto& children=document.objects.at(source).children;
    const bool suffix=members.size()<=children.size()&&
        std::equal(members.begin(),members.end(),children.end()-static_cast<std::ptrdiff_t>(members.size()));
    if(!suffix)throw Error("FOLDER_TRANSFER_SELECTION","Selection must be an ordered suffix of Folder children");
    const auto composition=canvas->active_composition();
    const auto& roots=find_composition(document,composition).roots;
    Id parent,destination;
    std::function<bool(const std::vector<Id>&,const Id&)> locate=[&](const std::vector<Id>& siblings,const Id& owner) {
        for(std::size_t i=0;i<siblings.size();++i) {
            if(siblings[i]==source) {
                parent=owner;
                if(i+1<siblings.size()&&document.objects.at(siblings[i+1]).kind==Kind::group)destination=siblings[i+1];
                return true;
            }
            if(locate(document.objects.at(siblings[i]).children,siblings[i]))return true;
        }
        return false;
    };
    if(!locate(roots,{})||destination.empty())
        throw Error("FOLDER_TRANSFER_SELECTION","Source Folder must have a next sibling Folder in the active Composition");
    std::vector<Canvas::Selection> selection;for(const auto& id:members)selection.push_back({id,{}});
    canvas->cancel_interaction();
    host.session.apply({MoveOut{composition,parent,source,members,"after"},
                        PutInside{composition,parent,destination,members}},host.session.revision());
    canvas->set_selections(std::move(selection));host.edited();
    statusBar()->showMessage("Moved selected objects to the next Folder; Undo restores both Folders",6000);
}
void Window::move_selection_to_previous_folder() {
    if(canvas->selected_objects().empty()||
       std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return !item.point.empty();}))
        throw Error("FOLDER_TRANSFER_SELECTION","Select whole objects forming a prefix inside a Folder");
    Id source;std::vector<Id> members;
    try{members=selected_siblings(source,1);}
    catch(const Error&){throw Error("FOLDER_TRANSFER_SELECTION","Select siblings inside one Folder");}
    const auto& document=host.session.document();
    if(source.empty()||!document.objects.contains(source)||document.objects.at(source).kind!=Kind::group)
        throw Error("FOLDER_TRANSFER_SELECTION","Selection must be inside a Folder");
    const auto& children=document.objects.at(source).children;
    const bool prefix=members.size()<=children.size()&&std::equal(members.begin(),members.end(),children.begin());
    if(!prefix)throw Error("FOLDER_TRANSFER_SELECTION","Selection must be an ordered prefix of Folder children");
    const auto composition=canvas->active_composition();
    const auto& roots=find_composition(document,composition).roots;
    Id parent,destination;std::vector<Id> reordered;
    std::function<bool(const std::vector<Id>&,const Id&)> locate=[&](const std::vector<Id>& siblings,const Id& owner) {
        for(std::size_t i=0;i<siblings.size();++i) {
            if(siblings[i]==source) {
                parent=owner;
                if(i>0&&document.objects.at(siblings[i-1]).kind==Kind::group) {
                    destination=siblings[i-1];reordered=siblings;
                    reordered.insert(reordered.begin()+static_cast<std::ptrdiff_t>(i-1),members.begin(),members.end());
                }
                return true;
            }
            if(locate(document.objects.at(siblings[i]).children,siblings[i]))return true;
        }
        return false;
    };
    if(!locate(roots,{})||destination.empty())
        throw Error("FOLDER_TRANSFER_SELECTION","Source Folder must have a previous sibling Folder in the active Composition");
    auto destination_order=document.objects.at(destination).children;
    destination_order.insert(destination_order.end(),members.begin(),members.end());
    std::vector<Canvas::Selection> selection;for(const auto& id:members)selection.push_back({id,{}});
    canvas->cancel_interaction();
    host.session.apply({MoveOut{composition,parent,source,members,"before"},
                        ReorderObjects{composition,parent,std::move(reordered)},
                        PutInside{composition,parent,destination,members},
                        ReorderObjects{composition,destination,std::move(destination_order)}},host.session.revision());
    canvas->set_selections(std::move(selection));host.edited();
    statusBar()->showMessage("Moved selected objects to the previous Folder; Undo restores both Folders",6000);
}
void Window::move_selection_to_folder() {
    if(canvas->selected_objects().empty()||
       std::any_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return !item.point.empty();}))
        throw Error("FOLDER_TRANSFER_SELECTION","Select whole objects forming a prefix or suffix inside a Folder");
    Id source;std::vector<Id> members;
    try{members=selected_siblings(source,1);}
    catch(const Error&){throw Error("FOLDER_TRANSFER_SELECTION","Select siblings inside one Folder");}
    const auto& document=host.session.document();
    if(source.empty()||!document.objects.contains(source)||document.objects.at(source).kind!=Kind::group)
        throw Error("FOLDER_TRANSFER_SELECTION","Selection must be inside a Folder");
    const auto& children=document.objects.at(source).children;
    const bool prefix=members.size()<=children.size()&&std::equal(members.begin(),members.end(),children.begin());
    const bool suffix=members.size()<=children.size()&&
        std::equal(members.begin(),members.end(),children.end()-static_cast<std::ptrdiff_t>(members.size()));
    if(!prefix&&!suffix)throw Error("FOLDER_TRANSFER_SELECTION","Select an ordered prefix or suffix of Folder children");

    const auto composition=canvas->active_composition();
    const auto& roots=find_composition(document,composition).roots;
    Id parent;std::vector<Id> siblings;std::size_t source_index=0;bool found=false;
    std::function<void(const std::vector<Id>&,const Id&)> locate=[&](const std::vector<Id>& scope,const Id& owner) {
        if(found)return;
        for(std::size_t i=0;i<scope.size();++i)if(scope[i]==source) {
            parent=owner;siblings=scope;source_index=i;found=true;return;
        }
        for(const auto& id:scope)locate(document.objects.at(id).children,id);
    };
    locate(roots,{});
    if(!found)throw Error("FOLDER_TRANSFER_SELECTION","Source Folder is outside the active Composition");
    struct Destination {Id id;std::size_t index;};
    std::vector<Destination> destinations;QStringList labels;
    for(std::size_t i=0;i<siblings.size();++i) {
        if(i==source_index||document.objects.at(siblings[i]).kind!=Kind::group||
           (i<source_index&&!prefix)||(i>source_index&&!suffix))continue;
        const auto begin=std::min(i,source_index)+1,end=std::max(i,source_index);
        bool clear=true;
        for(auto k=begin;k<end;++k) {
            const auto& between=document.objects.at(siblings[k]);
            if(between.kind!=Kind::group||!between.children.empty()||!evaluate_object_visibility(document,between.id)||
               between.compositing.opacity.literal!=1||between.compositing.opacity.binding||between.compositing.opacity.expression||
               between.compositing.blend!="normal"||evaluate_composite_isolation(document,{between.id,"","composite.isolated"})||
               between.compositing.mask||!between.stack.empty()) {
                clear=false;break;
            }
        }
        if(!clear)continue;
        destinations.push_back({siblings[i],i});
        labels.push_back(qs(document.objects.at(siblings[i]).name)+" ["+qs(siblings[i])+"]");
    }
    if(destinations.empty())throw Error("FOLDER_TRANSFER_ORDER","No sibling Folder is reachable without crossing painted content; keep paint order or choose a separate stacking operation");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto generation=effects_generation_;
    bool accepted=false;
    const auto choice=QInputDialog::getItem(this,"Move selected to Folder","Destination Folder · paint order preserved",labels,0,false,&accepted);
    if(!accepted)return;
    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The Folder choice belongs to another document");
    if(host.session.revision()!=revision)throw Error("REVISION_CONFLICT","The document changed while choosing a Folder");
    const auto chosen=labels.indexOf(choice);
    if(chosen<0)throw Error("FOLDER_TRANSFER_SELECTION","Choose a listed destination Folder");
    const auto destination=destinations[static_cast<std::size_t>(chosen)];
    const bool later=destination.index>source_index;
    const auto placement=later?std::string("after"):std::string("before");
    std::vector<Command> commands{MoveOut{composition,parent,source,members,placement}};
    auto after_move=siblings;
    after_move.insert(after_move.begin()+static_cast<std::ptrdiff_t>(source_index+(later?1:0)),members.begin(),members.end());
    auto before_put=siblings;
    before_put.insert(before_put.begin()+static_cast<std::ptrdiff_t>(destination.index),members.begin(),members.end());
    if(before_put!=after_move)commands.push_back(ReorderObjects{composition,parent,std::move(before_put)});
    commands.push_back(PutInside{composition,parent,destination.id,members});
    if(!later) {
        auto destination_order=document.objects.at(destination.id).children;
        destination_order.insert(destination_order.end(),members.begin(),members.end());
        commands.push_back(ReorderObjects{composition,destination.id,std::move(destination_order)});
    }
    std::vector<Canvas::Selection> selection;for(const auto& id:members)selection.push_back({id,{}});
    canvas->cancel_interaction();host.session.apply(commands,revision);
    canvas->set_selections(std::move(selection));host.edited();
    statusBar()->showMessage("Moved selected objects to the chosen Folder without changing paint order; Undo restores both Folders",7000);
}
void Window::batch_rename_selection() {
    Id parent;
    const auto ids=selected_siblings(parent,2);
    const auto frozen_session=host.session_id;
    const auto frozen_revision=host.session.revision();
    const auto selection=canvas->selections();
    const auto& document=host.session.document();

    QDialog dialog(this);
    dialog.setObjectName("batch-rename-dialog");
    dialog.setWindowTitle("Batch rename selected objects");
    dialog.resize(680,380);
    auto* layout=new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Review stable IDs and proposed names. Changes are applied together.",&dialog));
    auto* base_row=new QHBoxLayout;
    base_row->addWidget(new QLabel("Base name",&dialog));
    auto* base=new QLineEdit(qs(document.objects.at(ids.front()).name),&dialog);
    base->setObjectName("batch-rename-base");
    base_row->addWidget(base);
    layout->addLayout(base_row);

    auto* preview=new QWidget(&dialog);
    auto* grid=new QGridLayout(preview);
    grid->addWidget(new QLabel("Stable ID",&dialog),0,0);
    grid->addWidget(new QLabel("Current name",&dialog),0,1);
    grid->addWidget(new QLabel("Proposed name",&dialog),0,2);
    std::vector<QLineEdit*> proposed;
    std::vector<bool> edited(ids.size(),false);
    proposed.reserve(ids.size());
    auto generated_name=[&](std::size_t index) {
        const auto prefix=base->text().trimmed();
        return prefix.isEmpty()?QString::number(index+1):prefix+QString(" %1").arg(index+1);
    };
    for(std::size_t i=0;i<ids.size();++i) {
        const auto row=static_cast<int>(i+1);
        auto* id_label=new QLabel(qs(ids[i]),&dialog);
        id_label->setObjectName(QString("batch-rename-id-%1").arg(i));
        id_label->setTextFormat(Qt::PlainText);
        id_label->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
        grid->addWidget(id_label,row,0);
        auto* old_name=new QLabel(qs(document.objects.at(ids[i]).name),&dialog);
        old_name->setObjectName(QString("batch-rename-old-%1").arg(i));
        old_name->setTextFormat(Qt::PlainText);
        grid->addWidget(old_name,row,1);
        auto* name=new QLineEdit(generated_name(i),&dialog);
        name->setObjectName(QString("batch-rename-name-%1").arg(i));
        proposed.push_back(name);
        grid->addWidget(name,row,2);
        connect(name,&QLineEdit::textEdited,&dialog,[&edited,i](const QString&){edited[i]=true;});
    }
    grid->setColumnStretch(0,1);grid->setColumnStretch(1,1);grid->setColumnStretch(2,2);
    auto* preview_scroll=new QScrollArea(&dialog);
    preview_scroll->setWidgetResizable(true);
    preview_scroll->setWidget(preview);
    layout->addWidget(preview_scroll,1);
    auto* error=new QLabel(&dialog);
    error->setObjectName("batch-rename-error");
    error->setTextFormat(Qt::PlainText);
    error->setWordWrap(true);
    layout->addWidget(error);
    auto update_generated=[&] {
        for(std::size_t i=0;i<proposed.size();++i)
            if(!edited[i])proposed[i]->setText(generated_name(i));
    };
    connect(base,&QLineEdit::textChanged,&dialog,[&](const QString&){update_generated();});

    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Apply");
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&] {
        if(std::any_of(proposed.begin(),proposed.end(),[](const auto* name){return name->text().trimmed().isEmpty();})) {
            error->setText("Every proposed name must be non-empty.");
            return;
        }
        dialog.accept();
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;

    if(host.session_id!=frozen_session)
        throw Error("SESSION_CONFLICT","The document changed while batch rename was open; reopen the dialog");
    if(host.session.revision()!=frozen_revision)
        throw Error("REVISION_CONFLICT","The document changed while batch rename was open; reopen the dialog");

    std::vector<Command> commands;
    commands.reserve(ids.size());
    for(std::size_t i=0;i<ids.size();++i) {
        const auto name=proposed[i]->text().toStdString();
        if(name.empty())throw Error("INVALID_NAME","Every proposed name must be non-empty");
        commands.push_back(Rename{ids[i],name});
    }
    canvas->cancel_interaction();
    host.session.apply(commands,frozen_revision);
    host.edited();
    canvas->set_selections(selection);
}
void Window::sort_selection_by_name_paint_order() {
    Id parent;
    const auto selected=selected_siblings(parent,2);
    const auto frozen_session=host.session_id;
    const auto frozen_revision=host.session.revision();
    const auto selection=canvas->selections();
    const auto& document=host.session.document();
    const auto& composition=find_composition(document,canvas->active_composition());
    const auto original=parent.empty()?composition.roots:document.objects.at(parent).children;
    auto reordered=original;
    auto sorted=selected;
    std::stable_sort(sorted.begin(),sorted.end(),[&](const Id& left,const Id& right) {
        return QString::compare(qs(document.objects.at(left).name),qs(document.objects.at(right).name),Qt::CaseInsensitive)<0;
    });
    std::set<Id> chosen(selected.begin(),selected.end());
    auto next=sorted.begin();
    for(auto& id:reordered)if(chosen.contains(id))id=*next++;

    QDialog dialog(this);
    dialog.setObjectName("sort-paint-order-dialog");
    dialog.setWindowTitle("Sort Selected by Name (Paint Order)");
    dialog.resize(900,480);
    auto* layout=new QVBoxLayout(&dialog);
    auto* heading=new QLabel("Review the full sibling paint order. Apply changes the actual draw / paint order.",&dialog);
    heading->setObjectName("sort-paint-order-heading");
    heading->setWordWrap(true);layout->addWidget(heading);

    auto* preview=new QWidget(&dialog);
    auto* grid=new QGridLayout(preview);
    grid->addWidget(new QLabel("Slot",preview),0,0);
    grid->addWidget(new QLabel("Before ID",preview),0,1);
    grid->addWidget(new QLabel("Before name",preview),0,2);
    grid->addWidget(new QLabel("After ID",preview),0,3);
    grid->addWidget(new QLabel("After name",preview),0,4);
    for(std::size_t i=0;i<original.size();++i) {
        const auto row=static_cast<int>(i+1);
        auto* slot=new QLabel(QString::number(i+1),preview);
        slot->setObjectName(QString("sort-paint-order-slot-%1").arg(i));
        auto* before_id=new QLabel(qs(original[i]),preview);
        before_id->setObjectName(QString("sort-paint-order-before-id-%1").arg(i));
        before_id->setTextFormat(Qt::PlainText);
        before_id->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
        auto* before_name=new QLabel(qs(document.objects.at(original[i]).name),preview);
        before_name->setObjectName(QString("sort-paint-order-before-name-%1").arg(i));
        before_name->setTextFormat(Qt::PlainText);
        auto* after_id=new QLabel(qs(reordered[i]),preview);
        after_id->setObjectName(QString("sort-paint-order-after-id-%1").arg(i));
        after_id->setTextFormat(Qt::PlainText);
        after_id->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
        auto* after_name=new QLabel(qs(document.objects.at(reordered[i]).name),preview);
        after_name->setObjectName(QString("sort-paint-order-after-name-%1").arg(i));
        after_name->setTextFormat(Qt::PlainText);
        grid->addWidget(slot,row,0);grid->addWidget(before_id,row,1);grid->addWidget(before_name,row,2);
        grid->addWidget(after_id,row,3);grid->addWidget(after_name,row,4);
    }
    grid->setColumnStretch(1,1);grid->setColumnStretch(2,1);grid->setColumnStretch(3,1);grid->setColumnStretch(4,1);
    auto* preview_scroll=new QScrollArea(&dialog);
    preview_scroll->setObjectName("sort-paint-order-preview-scroll");
    preview_scroll->setWidgetResizable(true);preview_scroll->setWidget(preview);layout->addWidget(preview_scroll,1);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Apply");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;

    if(host.session_id!=frozen_session)
        throw Error("SESSION_CONFLICT","The paint-order preview belongs to another document; reopen it");
    if(host.session.revision()!=frozen_revision)
        throw Error("REVISION_CONFLICT","The document changed while the paint-order preview was open; reopen it");
    if(reordered==original) {
        canvas->set_selections(selection);
        statusBar()->showMessage("No change: selected objects are already in case-insensitive name order",5000);
        return;
    }
    canvas->cancel_interaction();
    host.session.apply({ReorderObjects{composition.id,parent,std::move(reordered)}},frozen_revision);
    host.edited();
    canvas->set_selections(selection);
    statusBar()->showMessage("Paint order changed; Undo restores the previous sibling order",5000);
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
    auto* folder_from_selection=menu.addAction("Create Folder from selected…");folder_from_selection->setObjectName("create-folder-from-selection-context");
    try {
        Id parent;const auto members=selected_siblings(parent,2);const auto& d=host.session.document();
        const auto& comp=find_composition(d,canvas->active_composition());
        const auto& siblings=parent.empty()?comp.roots:d.objects.at(parent).children;
        folder_from_selection->setEnabled(std::search(siblings.begin(),siblings.end(),members.begin(),members.end())!=siblings.end());
    } catch(const Error&) {folder_from_selection->setEnabled(false);}
    auto* batch_rename=menu.addAction("Batch rename selected…");batch_rename->setObjectName("batch-rename-selection-context");
    try{Id parent;(void)selected_siblings(parent,2);}catch(const Error&){batch_rename->setEnabled(false);}
    auto* sort_paint_order=menu.addAction("Sort selected by name (paint order)…");sort_paint_order->setObjectName("sort-selected-name-paint-order-context");
    try{Id parent;(void)selected_siblings(parent,2);}catch(const Error&){sort_paint_order->setEnabled(false);}
    auto* ungroup=menu.addAction("Ungroup selected Groups");
    auto* move_out=menu.addAction("Move selected out of Folder");move_out->setObjectName("move-out-of-folder-context");
    auto* move_to_next=menu.addAction("Move selected to next Folder");move_to_next->setObjectName("move-to-next-folder-context");
    move_to_next->setEnabled(false);
    auto* move_to_previous=menu.addAction("Move selected to previous Folder");move_to_previous->setObjectName("move-to-previous-folder-context");
    move_to_previous->setEnabled(false);
    auto* move_to_folder=menu.addAction("Move selected to Folder…");move_to_folder->setObjectName("move-to-folder-context");move_to_folder->setEnabled(false);
    try {
        Id folder;const auto members=selected_siblings(folder,1);const auto& d=host.session.document();
        if(!folder.empty()&&d.objects.contains(folder)&&d.objects.at(folder).kind==Kind::group) {
            const auto& children=d.objects.at(folder).children;
            const bool prefix=members.size()<=children.size()&&std::equal(members.begin(),members.end(),children.begin());
            const bool suffix=members.size()<=children.size()&&
                std::equal(members.begin(),members.end(),children.begin()+static_cast<std::ptrdiff_t>(children.size()-std::min(children.size(),members.size())));
            move_out->setEnabled(prefix||suffix);
            move_to_folder->setEnabled(prefix||suffix);
            if(suffix&&std::all_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return item.point.empty();})) {
                const auto& composition=find_composition(d,canvas->active_composition());
                std::function<bool(const std::vector<Id>&)> has_next=[&](const std::vector<Id>& siblings) {
                    for(std::size_t i=0;i<siblings.size();++i) {
                        if(siblings[i]==folder)return i+1<siblings.size()&&d.objects.at(siblings[i+1]).kind==Kind::group;
                        if(has_next(d.objects.at(siblings[i]).children))return true;
                    }
                    return false;
                };
                move_to_next->setEnabled(has_next(composition.roots));
            }
            if(prefix&&std::all_of(canvas->selections().begin(),canvas->selections().end(),[](const auto& item){return item.point.empty();})) {
                const auto& composition=find_composition(d,canvas->active_composition());
                std::function<bool(const std::vector<Id>&)> has_previous=[&](const std::vector<Id>& siblings) {
                    for(std::size_t i=0;i<siblings.size();++i) {
                        if(siblings[i]==folder)return i>0&&d.objects.at(siblings[i-1]).kind==Kind::group;
                        if(has_previous(d.objects.at(siblings[i]).children))return true;
                    }
                    return false;
                };
                move_to_previous->setEnabled(has_previous(composition.roots));
            }
        } else move_out->setEnabled(false);
    } catch(const Error&) {move_out->setEnabled(false);}
    try{Id parent;const auto members=selected_siblings(parent,1);ungroup->setEnabled(std::all_of(members.begin(),members.end(),[&](const Id& id){return host.session.document().objects.at(id).kind==Kind::group;}));}catch(const Error&){ungroup->setEnabled(false);}
    auto* top=menu.addAction("Mask With Top");auto* bottom=menu.addAction("Mask With Bottom");auto* inside=menu.addAction("Put Inside top selected Group");
    try {
        Id parent;const auto members=selected_siblings(parent);const auto& d=host.session.document();
        top->setText("Mask With Top · "+qs(d.objects.at(members.back()).name));bottom->setText("Mask With Bottom · "+qs(d.objects.at(members.front()).name));
        inside->setText("Put Inside · "+qs(d.objects.at(members.back()).name));
        top->setEnabled((d.objects.at(members.back()).kind==Kind::path||d.objects.at(members.back()).kind==Kind::text));bottom->setEnabled((d.objects.at(members.front()).kind==Kind::path||d.objects.at(members.front()).kind==Kind::text));inside->setEnabled(d.objects.at(members.back()).kind==Kind::group);
    } catch(const Error&) {group->setEnabled(false);top->setEnabled(false);bottom->setEnabled(false);inside->setEnabled(false);}
    auto* chosen=menu.exec(global);if(!chosen)return;
    perform([&]{if(host.session_id!=menu_session||host.session.revision()!=menu_revision)throw Error("STALE_CONTEXT","Document changed while the menu was open; reopen the selection menu");if(stack_actions.contains(chosen)){const auto [direction,edge]=stack_actions.at(chosen);stack_selection(direction,edge);}else if(chosen==transform)transform_selection();else if(chosen==duplicate)duplicate_selection();else if(chosen==top)mask_selection(true);else if(chosen==bottom)mask_selection(false);else if(chosen==inside)put_selection_inside();else if(chosen==move_out)move_selection_out();else if(chosen==move_to_next)move_selection_to_next_folder();else if(chosen==move_to_previous)move_selection_to_previous_folder();else if(chosen==move_to_folder)move_selection_to_folder();else if(chosen==folder_from_selection)create_folder_from_selection();else if(chosen==group)group_selection();else if(chosen==ungroup)ungroup_selection();else if(chosen==batch_rename)batch_rename_selection();else if(chosen==sort_paint_order)sort_selection_by_name_paint_order();});
}
void Window::create_definition_from_selection() {
    const auto selected=canvas->selected_objects();
    if(selected.size()!=1||!canvas->selected_point.empty())throw Error("INVALID_DEFINITION","Select one whole Group to create a Definition");
    const auto& document=host.session.document();const auto& source=document.objects.at(selected.front());
    if(source.kind!=Kind::group)throw Error("INVALID_DEFINITION","A Definition must use an existing Group as its source root");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto generation=effects_generation_;
    bool accepted=false;const auto name=QInputDialog::getText(this,"Create Definition","Definition name:",
        QLineEdit::Normal,qs(source.name),&accepted).trimmed();
    if(!accepted)return;
    if(name.isEmpty())throw Error("INVALID_DEFINITION","Enter a Definition name");
    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","Selection belongs to another document");
    if(host.session.revision()!=revision||effects_generation_!=generation||canvas->selected_objects()!=selected)
        throw Error("REVISION_CONFLICT","Selection or document changed while naming the Definition");
    const auto id=new_id();
    host.session.apply({DefinitionCommand{CreateDefinition{Definition{id,name.toStdString(),selected.front()}}}},revision);
    host.edited();statusBar()->showMessage("Definition created from the selected Group; its source remains in place",6000);
}
void Window::rename_definition() {
    const auto document=host.session.document();
    if(document.definitions.empty())throw Error("MISSING_DEFINITION","There are no Definitions to rename");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto generation=effects_generation_;
    QStringList labels;std::vector<Id> ids;
    for(const auto& [id,definition]:document.definitions){ids.push_back(id);labels.push_back(qs(definition.name)+" · "+qs(id));}
    bool accepted=false;const auto chosen=QInputDialog::getItem(this,"Rename Definition","Definition:",labels,0,false,&accepted);
    if(!accepted)return;
    const auto index=labels.indexOf(chosen);
    if(index<0||static_cast<std::size_t>(index)>=ids.size())throw Error("MISSING_DEFINITION","Choose a Definition");
    const auto id=ids.at(static_cast<std::size_t>(index));
    const auto previous=document.definitions.at(id).name;
    const auto name=QInputDialog::getText(this,"Rename Definition","New name:",QLineEdit::Normal,qs(previous),&accepted).trimmed();
    if(!accepted)return;
    if(name.isEmpty())throw Error("INVALID_DEFINITION","Enter a Definition name");
    if(host.session_id!=frozen_session||host.session.revision()!=revision||effects_generation_!=generation)
        throw Error("REVISION_CONFLICT","Document changed while renaming the Definition");
    host.session.apply({DefinitionCommand{RenameDefinition{id,name.toStdString()}}},revision);
    host.edited();statusBar()->showMessage("Definition renamed; its stable ID and placed Instances are unchanged",6000);
}
void Window::place_definition_instance() {
    const auto document=host.session.document();
    if(document.definitions.empty())throw Error("MISSING_DEFINITION","Create a Definition before placing an Instance");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto generation=effects_generation_;
    const auto composition=canvas->active_composition(),parent=canvas->drill_scope();
    QStringList labels;std::vector<Id> ids;
    for(const auto& [id,definition]:document.definitions){ids.push_back(id);labels.push_back(qs(definition.name)+" · "+qs(id));}
    bool accepted=false;const auto chosen=QInputDialog::getItem(this,"Place Definition Instance","Definition:",labels,0,false,&accepted);
    if(!accepted)return;
    const auto index=labels.indexOf(chosen);
    if(index<0||static_cast<std::size_t>(index)>=ids.size())throw Error("MISSING_DEFINITION","Choose a Definition");
    if(host.session_id!=frozen_session||host.session.revision()!=revision||effects_generation_!=generation)
        throw Error("REVISION_CONFLICT","Document changed while choosing a Definition");
    const auto definition_id=ids.at(static_cast<std::size_t>(index));
    const auto instance_id=new_id();
    host.session.apply({DefinitionCommand{CreateInstance{composition,parent,instance_id,definition_id,
        document.definitions.at(definition_id).name+" Instance"}}},revision);
    canvas->set_selection(instance_id);host.edited();
    statusBar()->showMessage("Placed a live Definition Instance; source edits continue to flow through",6000);
}
void Window::set_instance_override() {
    const auto selected=canvas->selected_objects();
    if(selected.size()!=1||!canvas->selected_point.empty())throw Error("INVALID_INSTANCE","Select one whole Instance");
    const auto document=host.session.document();const auto instance_it=document.objects.find(selected.front());
    if(instance_it==document.objects.end()||instance_it->second.kind!=Kind::instance||!instance_it->second.instance)
        throw Error("TYPE_MISMATCH","Set override requires a Definition Instance");
    const auto definition_it=document.definitions.find(instance_it->second.instance->definition);
    if(definition_it==document.definitions.end())throw Error("MISSING_DEFINITION",instance_it->second.instance->definition);
    std::vector<Id> source_ids;
    std::function<void(const Id&)> append_source=[&](const Id& id) {
        const auto& source=document.objects.at(id);
        if(source.kind==Kind::path||source.kind==Kind::group||source.kind==Kind::text)source_ids.push_back(id);
        for(const auto& child:source.children)append_source(child);
    };
    append_source(definition_it->second.root);
    if(source_ids.empty())throw Error("UNSUPPORTED_OVERRIDE","Definition contains no supported scalar property targets");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto generation=effects_generation_;
    const auto captured_instance=selected.front();
    QDialog dialog(this);dialog.setObjectName("set-instance-override-dialog");dialog.setWindowTitle("Set Instance override");
    auto* layout=new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Choose one source item and a supported Scalar. The value is local to this Instance.",&dialog));
    auto* source=new QComboBox(&dialog);source->setObjectName("set-instance-override-source");layout->addWidget(source);
    for(const auto& id:source_ids) {
        const auto& object=document.objects.at(id);
        const auto kind=object.kind==Kind::group?QStringLiteral("Group"):
            object.kind==Kind::text?QStringLiteral("Text"):QStringLiteral("Path");
        const auto shape=object.source?primitive_label(*object.source)+" · ":QString{};
        source->addItem(qs(object.name)+" · "+shape+kind,qs(id));
    }
    auto* property=new QComboBox(&dialog);property->setObjectName("set-instance-override-property");layout->addWidget(property);
    auto* value=new QDoubleSpinBox(&dialog);value->setObjectName("set-instance-override-value");
    value->setDecimals(4);value->setSingleStep(0.1);layout->addWidget(value);
    const auto refresh_fields=[this,&document,source,property,value,instance_id=captured_instance] {
        const auto source_id=source->currentData().toString().toStdString();
        const auto& object=document.objects.at(source_id);const QSignalBlocker block(property);property->clear();
        property->addItem("composite.opacity","composite.opacity");
        if(object.kind==Kind::text&&object.text)property->addItem("text.font_size","text.font_size");
        const auto& definition=document.definitions.at(document.objects.at(instance_id).instance->definition);
        if(source_id!=definition.root) {
            property->addItem("transform.tx","transform.tx");property->addItem("transform.ty","transform.ty");
            if(object.kind==Kind::path&&object.source&&object.source->type=="nect.shape.rectangle") {
                property->addItem("generator.width","generator.width");property->addItem("generator.height","generator.height");
            }
        }
        const auto initialize_value=[this,&document,value,instance_id,source_id](const QString& field) {
            const auto key=Ref{source_id,"",field.toStdString()};
            if(field=="composite.opacity"){value->setRange(0,1);value->setSingleStep(0.05);}
            else if(field=="transform.tx"||field=="transform.ty"){value->setRange(-10000000,10000000);value->setSingleStep(1);}
            else {value->setRange(0.0001,10000000);value->setSingleStep(1);}
            const auto& overrides=document.objects.at(instance_id).instance->overrides;
            const auto override=overrides.find(key);
            value->setValue(override==overrides.end()?evaluate(document).at(key):override->second);
        };
        initialize_value(property->currentData().toString());
        QObject::disconnect(property,nullptr,nullptr,nullptr);
        QObject::connect(property,qOverload<int>(&QComboBox::currentIndexChanged),property,
            [initialize_value,property](int){initialize_value(property->currentData().toString());});
    };
    QObject::connect(source,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[refresh_fields](int){refresh_fields();});
    refresh_fields();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
    QObject::connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(host.session_id!=frozen_session||host.session.revision()!=revision||effects_generation_!=generation||
       canvas->selected_objects()!=selected||canvas->selected_point.size()!=0)
        throw Error("REVISION_CONFLICT","Instance selection or document changed while setting an override");
    const auto source_id=source->currentData().toString().toStdString();
    const Ref target{source_id,"",property->currentData().toString().toStdString()};
    host.session.apply({DefinitionCommand{SetInstanceOverride{captured_instance,target,value->value()}}},revision);
    host.edited();statusBar()->showMessage("Local Instance override set; source values remain live for other properties",6000);
}
void Window::reset_instance_override() {
    const auto selected=canvas->selected_objects();
    if(selected.size()!=1||!canvas->selected_point.empty())throw Error("INVALID_INSTANCE","Select one whole Instance");
    const auto& document=host.session.document();const auto found=document.objects.find(selected.front());
    if(found==document.objects.end()||found->second.kind!=Kind::instance||!found->second.instance)
        throw Error("TYPE_MISMATCH","Reset override requires a Definition Instance");
    if(found->second.instance->overrides.empty())throw Error("MISSING_OVERRIDE","The selected Instance has no local overrides");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto generation=effects_generation_;
    std::vector<Ref> refs;QStringList labels;
    for(const auto& [ref,value]:found->second.instance->overrides) {
        (void)value;refs.push_back(ref);
        const auto source=document.objects.find(ref.object);
        labels.push_back((source==document.objects.end()?qs(ref.object):qs(source->second.name))+" · "+qs(ref.field));
    }
    bool accepted=false;const auto chosen=QInputDialog::getItem(this,"Reset Instance override","Override:",labels,0,false,&accepted);
    if(!accepted)return;
    const auto index=labels.indexOf(chosen);
    if(index<0||static_cast<std::size_t>(index)>=refs.size())throw Error("MISSING_OVERRIDE","Choose an override");
    if(host.session_id!=frozen_session||host.session.revision()!=revision||effects_generation_!=generation||canvas->selected_objects()!=selected)
        throw Error("REVISION_CONFLICT","Instance selection or document changed while choosing an override");
    host.session.apply({DefinitionCommand{ResetInstanceOverride{selected.front(),refs.at(static_cast<std::size_t>(index))}}},revision);
    host.edited();statusBar()->showMessage("Local override reset to the current Definition source value",6000);
}
void Window::detach_instance() {
    const auto selected=canvas->selected_objects();
    if(selected.size()!=1||!canvas->selected_point.empty())throw Error("INVALID_INSTANCE","Select one whole Instance to detach");
    const auto& object=host.session.document().objects.at(selected.front());
    if(object.kind!=Kind::instance||!object.instance)throw Error("TYPE_MISMATCH","Detach requires a Definition Instance");
    canvas->cancel_interaction();host.session.apply({DefinitionCommand{DetachInstance{selected.front(),"detach-"+new_id()}}},
        host.session.revision());
    canvas->set_selection(selected.front());host.edited();
    statusBar()->showMessage("Detached to independent editable Objects; Undo restores the live Instance",7000);
}
void Window::delete_definition() {
    const auto& document=host.session.document();
    if(document.definitions.empty())throw Error("MISSING_DEFINITION","There are no Definitions to delete");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto generation=effects_generation_;
    QStringList labels;std::vector<Id> ids;
    for(const auto& [id,definition]:document.definitions){ids.push_back(id);labels.push_back(qs(definition.name)+" · "+qs(id));}
    bool accepted=false;const auto chosen=QInputDialog::getItem(this,"Delete Definition","Definition:",labels,0,false,&accepted);
    if(!accepted)return;
    const auto index=labels.indexOf(chosen);
    if(index<0||static_cast<std::size_t>(index)>=ids.size())throw Error("MISSING_DEFINITION","Choose a Definition");
    if(host.session_id!=frozen_session||host.session.revision()!=revision||effects_generation_!=generation)
        throw Error("REVISION_CONFLICT","Document changed while choosing a Definition");
    host.session.apply({DefinitionCommand{DeleteDefinition{ids.at(static_cast<std::size_t>(index))}}},revision);
    host.edited();statusBar()->showMessage("Definition deleted; Undo restores the named source",6000);
}
std::optional<Id> Window::choose_collection(const QString& title,const std::vector<Collection>& collections) {
    if(collections.empty())throw Error("MISSING_COLLECTION","There are no Collections");
    QStringList labels;
    for(const auto& collection:collections)
        labels.push_back(qs(collection.name)+" · "+qs(collection.id)+" ("+QString::number(collection.members.size())+")");
    bool accepted=false;const auto chosen=QInputDialog::getItem(this,title,"Collection:",labels,0,false,&accepted);
    if(!accepted)return std::nullopt;
    const auto index=labels.indexOf(chosen);
    if(index<0||static_cast<std::size_t>(index)>=collections.size())throw Error("MISSING_COLLECTION","Choose a Collection");
    return collections.at(static_cast<std::size_t>(index)).id;
}
void Window::browse_collections() {
    const auto collections=host.session.document().collections;
    const auto chosen=choose_collection("Browse Collections",collections);if(!chosen)return;
    const auto found=std::find_if(collections.begin(),collections.end(),[&](const Collection& item){return item.id==*chosen;});
    QStringList members;for(const auto& id:found->members)members.push_back(qs(id));
    QMessageBox::information(this,"Collection members",qs(found->name)+" · "+qs(found->id)+"\n\n"+
        (members.empty()?QString("No members"):members.join("\n")));
}
void Window::create_collection_from_selection() {
    const auto selected=canvas->selected_objects();
    if(selected.empty()||!canvas->selected_point.empty())throw Error("INVALID_SELECTION","Select whole Objects to create a Collection");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();
    bool accepted=false;const auto name=QInputDialog::getText(this,"Create Collection","Collection name:",
        QLineEdit::Normal,{},&accepted).trimmed();if(!accepted)return;
    if(name.isEmpty())throw Error("INVALID_COLLECTION","Enter a Collection name");
    if(host.session_id!=frozen_session||host.session.revision()!=revision||canvas->selected_objects()!=selected)
        throw Error("REVISION_CONFLICT","Selection or document changed while creating the Collection");
    host.session.apply({CollectionCommand{CreateCollection{{new_id(),name.toStdString(),selected}}}},revision);
    host.edited();statusBar()->showMessage("Collection created; selected Objects remain in place",6000);
}
void Window::rename_collection() {
    const auto collections=host.session.document().collections;
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();
    const auto chosen=choose_collection("Rename Collection",collections);if(!chosen)return;
    const auto found=std::find_if(collections.begin(),collections.end(),[&](const Collection& item){return item.id==*chosen;});
    bool accepted=false;const auto name=QInputDialog::getText(this,"Rename Collection","New name:",
        QLineEdit::Normal,qs(found->name),&accepted).trimmed();if(!accepted)return;
    if(name.isEmpty())throw Error("INVALID_COLLECTION","Enter a Collection name");
    if(host.session_id!=frozen_session||host.session.revision()!=revision)
        throw Error("REVISION_CONFLICT","Document changed while renaming the Collection");
    host.session.apply({CollectionCommand{RenameCollection{*chosen,name.toStdString()}}},revision);
    host.edited();statusBar()->showMessage("Collection renamed; member IDs are unchanged",6000);
}
void Window::add_selection_to_collection() {
    const auto selected=canvas->selected_objects();
    if(selected.empty()||!canvas->selected_point.empty())throw Error("INVALID_SELECTION","Select whole Objects to add to a Collection");
    const auto collections=host.session.document().collections;
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();
    const auto chosen=choose_collection("Add to Collection",collections);if(!chosen)return;
    const auto found=std::find_if(collections.begin(),collections.end(),[&](const Collection& item){return item.id==*chosen;});
    auto members=found->members;
    for(const auto& id:selected)if(std::find(members.begin(),members.end(),id)==members.end())members.push_back(id);
    if(host.session_id!=frozen_session||host.session.revision()!=revision||canvas->selected_objects()!=selected)
        throw Error("REVISION_CONFLICT","Selection or document changed while adding Collection members");
    host.session.apply({CollectionCommand{SetCollectionMembers{*chosen,std::move(members)}}},revision);
    host.edited();statusBar()->showMessage("Selected Objects added to Collection without moving artwork",6000);
}
void Window::remove_selection_from_collection() {
    const auto selected=canvas->selected_objects();
    if(selected.empty()||!canvas->selected_point.empty())throw Error("INVALID_SELECTION","Select whole Objects to remove from a Collection");
    const auto collections=host.session.document().collections;
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();
    const auto chosen=choose_collection("Remove from Collection",collections);if(!chosen)return;
    const auto found=std::find_if(collections.begin(),collections.end(),[&](const Collection& item){return item.id==*chosen;});
    auto members=found->members;
    for(const auto& id:selected)std::erase(members,id);
    if(host.session_id!=frozen_session||host.session.revision()!=revision||canvas->selected_objects()!=selected)
        throw Error("REVISION_CONFLICT","Selection or document changed while removing Collection members");
    host.session.apply({CollectionCommand{SetCollectionMembers{*chosen,std::move(members)}}},revision);
    host.edited();statusBar()->showMessage("Selected Objects removed from Collection without moving artwork",6000);
}
void Window::delete_collection() {
    const auto collections=host.session.document().collections;
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();
    const auto chosen=choose_collection("Delete Collection",collections);if(!chosen)return;
    if(host.session_id!=frozen_session||host.session.revision()!=revision)
        throw Error("REVISION_CONFLICT","Document changed while deleting the Collection");
    host.session.apply({CollectionCommand{DeleteCollection{*chosen}}},revision);
    host.edited();statusBar()->showMessage("Collection deleted; member Objects remain in place",6000);
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
void Window::create_folder_from_selection() {
    Id parent;std::vector<Id> members;
    try{members=selected_siblings(parent,2);}
    catch(const Error&){throw Error("FOLDER_GROUP_SELECTION","Select at least two whole sibling objects");}
    const auto composition=canvas->active_composition();
    const auto document=host.session.document();
    const auto& scope=find_composition(document,composition);
    const auto& siblings=parent.empty()?scope.roots:document.objects.at(parent).children;
    const auto start=std::search(siblings.begin(),siblings.end(),members.begin(),members.end());
    if(start==siblings.end())throw Error("FOLDER_GROUP_SELECTION","Select one contiguous sibling block to preserve paint order");
    const auto frozen_session=host.session_id;const auto revision=host.session.revision();const auto id=new_id();
    QDialog dialog(this);dialog.setObjectName("create-folder-from-selection-dialog");dialog.setWindowTitle("Create Folder from selected");dialog.resize(640,410);
    auto* layout=new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("The Folder replaces this sibling block. Stable IDs, pixels and flattened paint order stay the same.",&dialog));
    auto* name=new QLineEdit("Folder",&dialog);name->setObjectName("create-folder-from-selection-name");layout->addWidget(name);
    auto* preview=new QPlainTextEdit(&dialog);preview->setObjectName("create-folder-from-selection-preview");preview->setReadOnly(true);layout->addWidget(preview,1);
    auto update_preview=[&] {
        QStringList old_order,new_order;
        for(const auto& sibling:siblings)old_order.push_back(qs(sibling)+" · "+qs(document.objects.at(sibling).name));
        for(const auto& sibling:siblings) {
            if(sibling==*start) {
                new_order.push_back(qs(id)+" · "+name->text().trimmed()+" [Folder]");
                for(const auto& member:members)new_order.push_back("  "+qs(member)+" · "+qs(document.objects.at(member).name));
            } else if(std::find(members.begin(),members.end(),sibling)==members.end())new_order.push_back(qs(sibling)+" · "+qs(document.objects.at(sibling).name));
        }
        preview->setPlainText("Old sibling order:\n"+old_order.join("\n")+"\n\nProposed structure:\n"+new_order.join("\n")+
            "\n\nFlattened paint order: unchanged");
    };
    update_preview();connect(name,&QLineEdit::textChanged,&dialog,[&](const QString&){update_preview();});
    auto* error=new QLabel(&dialog);error->setObjectName("create-folder-from-selection-error");error->setTextFormat(Qt::PlainText);layout->addWidget(error);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Ok)->setText("Create Folder");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&] {
        if(name->text().trimmed().isEmpty()){error->setText("Enter a Folder name.");return;}
        dialog.accept();
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(host.session_id!=frozen_session)throw Error("SESSION_CONFLICT","The Folder preview belongs to another document");
    if(host.session.revision()!=revision)throw Error("REVISION_CONFLICT","The document changed while previewing the Folder");
    canvas->cancel_interaction();host.session.apply({GroupContiguous{composition,parent,members,id,name->text().trimmed().toStdString()}},revision);
    canvas->set_selection(id);host.edited();
    statusBar()->showMessage("Created Folder from selected siblings in one edit; Undo restores their original structure",7000);
}
void Window::create_folder() {
    const auto id=new_id();
    const auto composition=canvas->active_composition();
    const auto parent=canvas->drill_scope();
    canvas->cancel_interaction();
    host.session.apply({CreateFolder{composition,parent,id,"Folder"}},host.session.revision());
    canvas->set_selection(id);host.edited();
    canvas->setFocus();
}
void Window::closeEvent(QCloseEvent* event) {
    cancel_angle_adapters(false);
    cancel_whip();
    canvas->cancel_interaction();
    try {host.flush();event->accept();}
    catch(const std::exception& e) {QMessageBox::warning(this,"Recovery failed",QString::fromUtf8(e.what())+"\nSave your document before closing.");event->ignore();}
}
}
