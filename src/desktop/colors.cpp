#include "colors.hpp"
#include "window.hpp"
#include "nect/io.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <set>

namespace nect::desktop {
namespace {
constexpr auto value_mime="application/x-nect-color-value";
constexpr auto reference_mime="application/x-nect-color-reference";
QString qs(const std::string& value){return QString::fromStdString(value);}
QString number(double value){return QString::number(value,'g',17);}
QJsonObject ref_json(const Ref& ref){return {{"object",qs(ref.object)},{"point",qs(ref.point)},{"field",qs(ref.field)}};}
QJsonObject color_json(const ColorValue& value) {
    QJsonArray rgba;for(const auto channel:value.rgba)rgba.append(channel);
    return {{"version",1},{"space",qs(value.space)},{"profile",qs(value.profile)},{"alpha",qs(value.alpha)},{"rgba",rgba}};
}
QByteArray serialized(const QJsonObject& value){return QJsonDocument(value).toJson(QJsonDocument::Compact);}
QString hex_color(const ColorValue& value) {
    QString result="#";for(const auto channel:value.rgba)result+=QString("%1").arg(qRound(channel*255),2,16,QChar('0'));
    return result.toUpper();
}
QString precise_color(const ColorValue& value) {
    return "sRGB · straight alpha\nRGBA: "+number(value.rgba[0])+", "+number(value.rgba[1])+", "+number(value.rgba[2])+", "+number(value.rgba[3]);
}
QIcon swatch(const ColorValue& value) {
    QPixmap pixmap(28,20);QPainter painter(&pixmap);
    for(int y=0;y<20;y+=5)for(int x=0;x<28;x+=5)painter.fillRect(x,y,5,5,((x+y)/5)%2?QColor(225,225,225):QColor(170,170,170));
    painter.fillRect(pixmap.rect(),QColor::fromRgbF(value.rgba[0],value.rgba[1],value.rgba[2],value.rgba[3]));
    painter.setPen(QColor(95,95,95));painter.drawRect(pixmap.rect().adjusted(0,0,-1,-1));painter.end();
    // These pixels describe a color value; selection/disabled styling must not tint them.
    QIcon icon;
    for(const auto mode:{QIcon::Normal,QIcon::Active,QIcon::Selected,QIcon::Disabled})
        for(const auto state:{QIcon::Off,QIcon::On})icon.addPixmap(pixmap,mode,state);
    return icon;
}
void exact_keys(const QJsonObject& value,const std::set<QString>& keys) {
    for(auto i=value.begin();i!=value.end();++i)if(!keys.contains(i.key()))
        throw Error("UNSUPPORTED_COLOR","The structured color contains unsupported metadata; it was not reduced to HEX");
    if(static_cast<std::size_t>(value.size())!=keys.size())throw Error("INVALID_COLOR","The structured color is incomplete");
}
QJsonObject json_object(const QByteArray& bytes) {
    validate_json(std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size())));
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!document.isObject())throw Error("INVALID_COLOR","Clipboard color data is not a JSON object");
    return document.object();
}
ColorValue parse_color(const QJsonObject& object) {
    exact_keys(object,{"version","space","profile","alpha","rgba"});
    if(!object["version"].isDouble()||object["version"].toDouble()!=1||object["space"].toString()!="srgb"||object["profile"].toString()!="srgb"||object["alpha"].toString()!="straight")
        throw Error("UNSUPPORTED_COLOR","Only version 1 sRGB, sRGB profile, straight-alpha color values are supported; structured data was not reduced to HEX");
    if(!object["rgba"].isArray()||object["rgba"].toArray().size()!=4)throw Error("INVALID_COLOR","Color requires four RGBA channels");
    ColorValue value;const auto rgba=object["rgba"].toArray();
    for(int i=0;i<4;++i) {
        if(!rgba[i].isDouble()||!std::isfinite(rgba[i].toDouble())||rgba[i].toDouble()<0||rgba[i].toDouble()>1)
            throw Error("INVALID_COLOR","sRGB color channels must be finite numbers from 0 to 1");
        value.rgba[static_cast<std::size_t>(i)]=rgba[i].toDouble();
    }
    return value;
}
ColorValue parse_hex(QString text) {
    text=text.trimmed();if(text.startsWith('#'))text.remove(0,1);
    if((text.size()!=6&&text.size()!=8)||!std::all_of(text.begin(),text.end(),[](QChar c){return QStringLiteral("0123456789abcdefABCDEF").contains(c);}))
        throw Error("INVALID_COLOR","Enter sRGB #RRGGBB or #RRGGBBAA");
    ColorValue value;
    for(int i=0;i<text.size()/2;++i){bool ok=false;value.rgba[static_cast<std::size_t>(i)]=text.mid(i*2,2).toUInt(&ok,16)/255.0;if(!ok)throw Error("INVALID_COLOR","Invalid HEX channel");}
    return value;
}
ColorValue clipboard_value() {
    const auto* data=QApplication::clipboard()->mimeData();
    if(data->hasFormat(value_mime))return parse_color(json_object(data->data(value_mime)));
    return parse_hex(data->text());
}
Ref clipboard_reference(const Document& document) {
    const auto* data=QApplication::clipboard()->mimeData();
    if(!data->hasFormat(reference_mime))throw Error("NO_COLOR_REFERENCE","Use Copy Reference on a color first");
    const auto object=json_object(data->data(reference_mime));exact_keys(object,{"version","document_id","ref"});
    if(object["version"].toDouble()!=1||!object["document_id"].isString()||!object["ref"].isObject())throw Error("INVALID_REFERENCE","Clipboard color reference is invalid");
    if(object["document_id"].toString().toStdString()!=document.id)throw Error("COLOR_DOCUMENT_CONFLICT","Color references can be pasted only into their original document; use Paste Value for independent color");
    const auto ref=object["ref"].toObject();exact_keys(ref,{"object","point","field"});
    if(!ref["object"].isString()||!ref["point"].isString()||!ref["field"].isString())throw Error("INVALID_REFERENCE","Clipboard color reference is invalid");
    return {ref["object"].toString().toStdString(),ref["point"].toString().toStdString(),ref["field"].toString().toStdString()};
}
bool same_scalar(const Scalar& a,const Scalar& b) {
    if(a.literal!=b.literal||a.binding.has_value()!=b.binding.has_value())return false;
    if(!a.binding)return true;
    return a.binding->source==b.binding->source&&a.binding->scale==b.binding->scale&&a.binding->offset==b.binding->offset&&a.binding->mode==b.binding->mode;
}
void replace_actions(QVBoxLayout* layout,QWidget* widget) {
    while(auto* item=layout->takeAt(0)){if(item->widget()){item->widget()->hide();item->widget()->deleteLater();}delete item;}
    if(widget)layout->addWidget(widget);
}
void clear_modified(QLineEdit* input,const QString& value){input->setText(value);input->setModified(false);}
} // namespace

ColorTools::ColorTools(Window& window):QObject(&window),window_(window){setObjectName("color-tools");}

void ColorTools::check_target(const Ref& ref,const QString& session) const {
    if(window_.host.session_id!=session)throw Error("SESSION_CONFLICT","This color action belongs to another document session");
    (void)color_channels(window_.host.session.document(),ref);
}

QPushButton* ColorTools::menu_button(const Ref& ref,QWidget* parent) {
    auto* button=new QPushButton("Color actions…",parent);button->setObjectName("color-actions");
    button->setProperty("nect-color-reference",serialized(ref_json(ref)));
    auto* menu=new QMenu(button);button->setMenu(menu);const auto session=window_.host.session_id;
    populate_menu(menu,ref,session);
    connect(menu,&QMenu::aboutToShow,this,[this,menu,ref,session]{populate_menu(menu,ref,session);});
    return button;
}

void ColorTools::populate_menu(QMenu* menu,const Ref& ref,const QString& session) {
    menu->clear();
    auto action=[&](const char* name,const QString& text,auto fn) {
        auto* item=menu->addAction(text);item->setObjectName(name);
        connect(item,&QAction::triggered,this,[this,fn]{window_.perform(fn);});return item;
    };
    action("color-copy-value","Copy Value",[this,ref,session]{copy(ref,session,false);});
    action("color-copy-reference","Copy Reference",[this,ref,session]{copy(ref,session,true);});menu->addSeparator();
    action("color-paste-value","Paste Value · independent",[this,ref,session]{
        check_target(ref,session);window_.host.session.apply({SetColor{ref,clipboard_value()}},window_.host.session.revision());window_.host.edited();
    });
    action("color-paste-link","Paste Link · all RGBA channels",[this,ref,session]{
        check_target(ref,session);const auto source=clipboard_reference(window_.host.session.document());
        window_.host.session.apply({LinkColor{ref,source}},window_.host.session.revision());window_.host.edited();
    });
    action("color-link-named","Link named color…",[this,ref,session]{check_target(ref,session);pick_named(ref,session);});
    action("color-unlink","Unlink · keep evaluated RGBA",[this,ref,session]{
        check_target(ref,session);window_.host.session.apply({UnlinkColor{ref}},window_.host.session.revision());window_.host.edited();
    });
    auto* history=menu->addMenu("Copied Color History · this window");history->setObjectName("color-history-menu");
    if(history_.empty()){auto* empty=history->addAction("No colors explicitly copied yet");empty->setEnabled(false);}
    for(const auto& value:history_) {
        auto* item=history->addAction(swatch(value),hex_color(value));item->setToolTip(precise_color(value));
        connect(item,&QAction::triggered,this,[this,ref,session,value]{window_.perform([&]{
            check_target(ref,session);window_.host.session.apply({SetColor{ref,value}},window_.host.session.revision());window_.host.edited();
        });});
    }
    menu->addSeparator();
    action("color-create-named","Save value as named color",[this,ref,session]{
        check_target(ref,session);const auto& document=window_.host.session.document();
        create_named(color_value(document,ref,evaluate(document)));
    });
    action("color-manager","Manage colors…",[this]{show_manager();});
}

void ColorTools::remember(ColorValue value) {
    std::erase(history_,value);history_.insert(history_.begin(),value);if(history_.size()>32)history_.resize(32);refresh_history();
}
void ColorTools::copy_value(const ColorValue& value) {
    auto* data=new QMimeData;data->setData(value_mime,serialized(color_json(value)));data->setText(hex_color(value));
    QApplication::clipboard()->setMimeData(data);remember(value);
}
void ColorTools::copy(const Ref& ref,const QString& session,bool reference) {
    check_target(ref,session);const auto& document=window_.host.session.document();const auto value=color_value(document,ref,evaluate(document));
    auto* data=new QMimeData;data->setData(value_mime,serialized(color_json(value)));data->setText(hex_color(value));
    if(reference)data->setData(reference_mime,serialized({{"version",1},{"document_id",qs(document.id)},{"ref",ref_json(ref)}}));
    QApplication::clipboard()->setMimeData(data);remember(value);
}

void ColorTools::pick_named(const Ref& target,const QString& session) {
    auto* dialog=new QDialog(&window_);dialog->setObjectName("named-color-picker");dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Link named color");dialog->resize(420,330);auto* layout=new QVBoxLayout(dialog);
    auto* label=new QLabel("Target: "+qs(property_name(window_.host.session.document(),target)));label->setWordWrap(true);layout->addWidget(label);
    auto* list=new QListWidget;list->setObjectName("named-color-sources");layout->addWidget(list);
    const auto& document=window_.host.session.document();const auto values=evaluate(document);
    for(const auto& [id,color]:document.named_colors)if(id!=target.object) {
        const auto value=color_value(document,{id,"","color"},values);auto* item=new QListWidgetItem(swatch(value),qs(color.name)+" · "+hex_color(value),list);
        item->setData(Qt::UserRole,qs(id));item->setToolTip(precise_color(value));
    }
    auto* note=new QLabel("This explicitly replaces all four channel links. Equal color values alone never create a link.");note->setWordWrap(true);layout->addWidget(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,this,[this,dialog,list,target,session]{window_.perform([&]{
        check_target(target,session);if(!list->currentItem())throw Error("NO_COLOR_SOURCE","Choose a named color");
        const Ref source{list->currentItem()->data(Qt::UserRole).toString().toStdString(),"","color"};
        window_.host.session.apply({LinkColor{target,source}},window_.host.session.revision());window_.host.edited();dialog->accept();
    });});dialog->show();
}

void ColorTools::show_manager() {
    if(manager_){manager_->show();manager_->raise();manager_->activateWindow();refresh();return;}
    editor_base_.reset();usage_selection_.reset();manager_session_=window_.host.session_id;
    auto* dialog=new QDialog(&window_);manager_=dialog;dialog->setObjectName("color-manager");dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Colors");dialog->resize(620,650);auto* layout=new QVBoxLayout(dialog);
    auto* scope=new QLabel("Named colors belong to this document. Used colors are an inventory, not automatic links. Copied history belongs to this window only.");
    scope->setWordWrap(true);layout->addWidget(scope);
    auto* tabs=new QTabWidget;tabs->setObjectName("color-tabs");layout->addWidget(tabs);
    auto* named_page=new QWidget;auto* named_layout=new QVBoxLayout(named_page);tabs->addTab(named_page,"Named Colors");
    named_=new QListWidget;named_->setObjectName("named-colors");named_->setMaximumHeight(160);named_layout->addWidget(named_);
    auto* controls=new QHBoxLayout;named_layout->addLayout(controls);
    auto* create=new QPushButton("New named color");create->setObjectName("named-color-create");controls->addWidget(create);
    auto* remove=new QPushButton("Delete");remove->setObjectName("named-color-delete");controls->addWidget(remove);
    connect(create,&QPushButton::clicked,this,[this]{window_.perform([&]{create_named(ColorValue{});});});
    connect(remove,&QPushButton::clicked,this,[this]{window_.perform([&]{
        if(!editor_base_)throw Error("NO_COLOR_SOURCE","Select a named color");
        if(editor_dirty())throw Error("COLOR_DRAFT","Apply or discard the current draft before deleting its color");
        check_target({editor_base_->id,"","color"},editor_session_);
        window_.host.session.apply({DeleteNamedColor{editor_base_->id}},window_.host.session.revision());window_.host.edited();
    });});
    editor_=new QGroupBox("Selected named color");auto* editor_layout=new QVBoxLayout(editor_);named_layout->addWidget(editor_);
    auto* form=new QFormLayout;form->setRowWrapPolicy(QFormLayout::WrapLongRows);editor_layout->addLayout(form);
    name_=new QLineEdit;name_->setObjectName("named-color-name");form->addRow("Name",name_);
    hex_=new QLineEdit;hex_->setObjectName("named-color-hex");form->addRow("HEX · sRGB",hex_);
    const std::array<const char*,4> names{"Red","Green","Blue","Alpha"};
    for(std::size_t i=0;i<channels_.size();++i) {
        channels_[i]=new QLineEdit;channels_[i]->setObjectName(QString("named-color-channel-%1").arg(i));form->addRow(names[i],channels_[i]);
        connect(channels_[i],&QLineEdit::textEdited,this,[this]{hex_->setModified(false);});
    }
    connect(hex_,&QLineEdit::textEdited,this,[this]{for(auto* channel:channels_)channel->setModified(false);});
    auto* actions=new QWidget;named_actions_=new QVBoxLayout(actions);named_actions_->setContentsMargins(0,0,0,0);editor_layout->addWidget(actions);
    auto* edit_controls=new QHBoxLayout;editor_layout->addLayout(edit_controls);
    apply_=new QPushButton("Apply name / color");apply_->setObjectName("named-color-apply");edit_controls->addWidget(apply_);
    discard_=new QPushButton("Discard draft");discard_->setObjectName("named-color-discard");edit_controls->addWidget(discard_);
    connect(apply_,&QPushButton::clicked,this,[this]{window_.perform([&]{apply_editor();});});
    connect(discard_,&QPushButton::clicked,this,[this]{
        if(editor_base_&&editor_session_==window_.host.session_id&&window_.host.session.document().named_colors.contains(editor_base_->id))
            load_editor(editor_base_->id,evaluate(window_.host.session.document()));
        else clear_editor();refresh();
    });
    editor_status_=new QLabel;editor_status_->setWordWrap(true);named_layout->addWidget(editor_status_);named_layout->addStretch();
    connect(named_,&QListWidget::currentItemChanged,this,[this](QListWidgetItem* item,QListWidgetItem*) {
        if(refreshing_)return;
        const auto id=item?item->data(Qt::UserRole).toString().toStdString():Id{};
        if(editor_dirty()&&(!editor_base_||id!=editor_base_->id||editor_session_!=window_.host.session_id)) {
            const QSignalBlocker blocker(named_);named_->setCurrentItem(nullptr);
            if(editor_session_==window_.host.session_id&&editor_base_)for(int i=0;i<named_->count();++i)
                if(named_->item(i)->data(Qt::UserRole).toString().toStdString()==editor_base_->id)named_->setCurrentItem(named_->item(i));
            editor_status_->setText("Apply or discard the current draft before selecting another color.");return;
        }
        if(id.empty())clear_editor();else load_editor(id,evaluate(window_.host.session.document()));
    });
    auto* used_page=new QWidget;auto* used_layout=new QVBoxLayout(used_page);tabs->addTab(used_page,"Document Used Colors");
    auto* used_note=new QLabel("Exact evaluated RGBA values are grouped. Counts include active solid paints or gradient stops across this document, not sampled pixels; hidden fallback values and palette entries are excluded.");
    used_note->setWordWrap(true);used_layout->addWidget(used_note);
    used_=new QListWidget;used_->setObjectName("used-colors");used_layout->addWidget(used_);
    usages_=new QListWidget;usages_->setObjectName("color-usages");usages_->setMaximumHeight(180);used_layout->addWidget(usages_);
    auto* usage_actions=new QWidget;usage_actions_=new QVBoxLayout(usage_actions);usage_actions_->setContentsMargins(0,0,0,0);used_layout->addWidget(usage_actions);
    connect(used_,&QListWidget::currentRowChanged,this,[this](int row){if(!refreshing_)select_used(row);});
    connect(usages_,&QListWidget::currentRowChanged,this,[this](int row){if(!refreshing_)select_usage(row);});
    auto* history_page=new QWidget;auto* history_layout=new QVBoxLayout(history_page);tabs->addTab(history_page,"Copied History");
    auto* history_note=new QLabel("This window session only · up to 32 exact colors. Only explicit Copy Value or Copy Reference adds history. Editing, pasting, hovering and unrelated clipboard changes do not.");
    history_note->setObjectName("color-history-scope");history_note->setWordWrap(true);history_layout->addWidget(history_note);
    copied_=new QListWidget;copied_->setObjectName("copied-colors");history_layout->addWidget(copied_);
    auto* copy_history=new QPushButton("Copy Value");copy_history->setObjectName("color-history-copy");history_layout->addWidget(copy_history);
    connect(copy_history,&QPushButton::clicked,this,[this]{window_.perform([&]{
        const auto row=copied_->currentRow();if(row<0||static_cast<std::size_t>(row)>=history_.size())throw Error("NO_COLOR_SOURCE","Select a copied history value");
        copy_value(history_[static_cast<std::size_t>(row)]);
    });});
    auto* close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,dialog,&QDialog::close);
    refresh();dialog->show();
}

bool ColorTools::editor_dirty() const {
    return manager_&&name_&&(name_->isModified()||hex_->isModified()||std::any_of(channels_.begin(),channels_.end(),[](const auto* input){return input->isModified();}));
}
void ColorTools::clear_editor() {
    editor_base_.reset();editor_session_.clear();editor_->setEnabled(false);
    clear_modified(name_,{});clear_modified(hex_,{});for(auto* input:channels_)clear_modified(input,{});
    replace_actions(named_actions_,nullptr);editor_status_->setText("Select or create a named color. Equal values remain independent until you explicitly link them.");
}
void ColorTools::load_editor(const Id& id,const std::map<Ref,double>& values) {
    const auto& document=window_.host.session.document();editor_base_=document.named_colors.at(id);editor_session_=window_.host.session_id;
    editor_value_=color_value(document,{id,"","color"},values);editor_->setEnabled(true);apply_->setEnabled(true);
    clear_modified(name_,qs(editor_base_->name));clear_modified(hex_,hex_color(editor_value_));
    for(std::size_t i=0;i<channels_.size();++i)clear_modified(channels_[i],number(editor_value_.rgba[i]));
    replace_actions(named_actions_,menu_button({id,"","color"}));
    const auto link=color_link(document,{id,"","color"});
    if(link)editor_status_->setText("Linked to "+qs(property_name(document,*link))+". Unlink explicitly before changing RGBA.");
    else if(std::any_of(editor_base_->rgba.begin(),editor_base_->rgba.end(),[](const auto& scalar){return scalar.binding.has_value();}))
        editor_status_->setText("Some channels have individual links. Unlink explicitly before replacing this color.");
    else editor_status_->setText("Independent sRGB color · straight alpha. RGBA inputs retain full numeric precision; HEX is an 8-bit view.");
}

void ColorTools::create_named(const ColorValue& value) {
    if(editor_dirty())throw Error("COLOR_DRAFT","Apply or discard the current named-color draft before creating another");
    NamedColor color;color.id=new_id();color.name="Color "+std::to_string(window_.host.session.document().named_colors.size()+1);
    for(std::size_t i=0;i<4;++i)color.rgba[i].literal=value.rgba[i];
    const auto id=color.id;window_.host.session.apply({CreateNamedColor{color}},window_.host.session.revision());window_.host.edited();
    show_manager();for(int i=0;i<named_->count();++i)if(named_->item(i)->data(Qt::UserRole).toString().toStdString()==id)named_->setCurrentItem(named_->item(i));
    name_->setFocus();name_->selectAll();
}
void ColorTools::apply_editor() {
    if(!editor_base_)throw Error("NO_COLOR_SOURCE","Select a named color");
    const auto id=editor_base_->id;const Ref ref{id,"","color"};check_target(ref,editor_session_);
    const auto& document=window_.host.session.document();const auto& current=document.named_colors.at(id);
    const bool rename=name_->isModified();const bool recolor=hex_->isModified()||std::any_of(channels_.begin(),channels_.end(),[](const auto* input){return input->isModified();});
    if(rename&&current.name!=editor_base_->name)throw Error("COLOR_DRAFT_CONFLICT","This color was renamed while the draft was open; discard the draft to reload it");
    if(recolor) {
        const bool same_authored=std::equal(current.rgba.begin(),current.rgba.end(),editor_base_->rgba.begin(),same_scalar);
        if(!same_authored||color_value(document,ref,evaluate(document))!=editor_value_)
            throw Error("COLOR_DRAFT_CONFLICT","This color changed while the draft was open; discard the draft to reload it");
    }
    std::vector<Command> commands;
    if(rename&&name_->text().toStdString()!=current.name)commands.push_back(RenameNamedColor{id,name_->text().toStdString()});
    if(recolor) {
        ColorValue value;
        if(hex_->isModified())value=parse_hex(hex_->text());
        else for(std::size_t i=0;i<4;++i) {
            bool ok=false;value.rgba[i]=channels_[i]->text().trimmed().toDouble(&ok);
            if(!ok||!std::isfinite(value.rgba[i])||value.rgba[i]<0||value.rgba[i]>1)throw Error("INVALID_COLOR","RGBA channels must be numbers from 0 to 1");
        }
        if(value!=editor_value_)commands.push_back(SetColor{ref,value});
    }
    if(!commands.empty())window_.host.session.apply(commands,window_.host.session.revision());
    name_->setModified(false);hex_->setModified(false);for(auto* input:channels_)input->setModified(false);
    if(!commands.empty())window_.host.edited();else load_editor(id,evaluate(window_.host.session.document()));
}

void ColorTools::refresh() {
    if(!manager_||refreshing_)return;
    refreshing_=true;
    try {
        const auto& document=window_.host.session.document();const auto values=evaluate(document);
        const bool same_session=manager_session_==window_.host.session_id;manager_session_=window_.host.session_id;
        const auto selected=same_session&&editor_base_?editor_base_->id:Id{};
        const QSignalBlocker named_blocker(named_),used_blocker(used_);
        named_->clear();for(const auto& [id,color]:document.named_colors) {
            const auto value=color_value(document,{id,"","color"},values);
            auto* item=new QListWidgetItem(swatch(value),qs(color.name)+" · "+hex_color(value),named_);item->setData(Qt::UserRole,qs(id));
            item->setToolTip(precise_color(value)+"\nStable ID: "+qs(id));if(id==selected)named_->setCurrentItem(item);
        }
        if(editor_dirty()) {
            const bool valid_session=editor_session_==window_.host.session_id&&editor_base_&&document.named_colors.contains(editor_base_->id);
            apply_->setEnabled(valid_session);editor_status_->setText(valid_session?
                "Draft kept during document refresh. Apply checks for conflicting edits; Discard reloads the current color.":
                "The target session or color changed. This draft is kept for review; discard it before selecting a current color.");
        } else if(!selected.empty()&&document.named_colors.contains(selected))load_editor(selected,values);
        else clear_editor();
        used_colors_.clear();std::map<std::array<double,4>,std::vector<Ref>> grouped;
        for(const auto& ref:color_properties(document))if(color_is_used(document,ref))grouped[color_value(document,ref,values).rgba].push_back(ref);
        used_->clear();int selected_group=-1;
        for(auto& [rgba,refs]:grouped) {
            ColorValue value;value.rgba=rgba;used_colors_.push_back({value,std::move(refs)});
            const auto& group=used_colors_.back();auto* item=new QListWidgetItem(swatch(value),hex_color(value)+" · "+QString::number(group.refs.size())+" uses",used_);
            item->setData(Qt::UserRole,serialized(color_json(value)));item->setToolTip(precise_color(value)+"\nEqual values do not imply a link.");
            if(usage_selection_&&std::find(group.refs.begin(),group.refs.end(),*usage_selection_)!=group.refs.end())selected_group=used_->count()-1;
        }
        if(selected_group<0&&used_->count()>0)selected_group=0;used_->setCurrentRow(selected_group);select_used(selected_group);
        refresh_history();
    }catch(...){refreshing_=false;throw;}
    refreshing_=false;
}

void ColorTools::select_used(int row) {
    const QSignalBlocker blocker(usages_);usages_->clear();replace_actions(usage_actions_,nullptr);
    if(row<0||static_cast<std::size_t>(row)>=used_colors_.size()){usage_selection_.reset();return;}
    const auto& group=used_colors_[static_cast<std::size_t>(row)];int selected=0;
    for(const auto& ref:group.refs) {
        auto* item=new QListWidgetItem(qs(property_name(window_.host.session.document(),ref)),usages_);
        item->setData(Qt::UserRole,serialized(ref_json(ref)));item->setToolTip(qs(ref.object+" / "+ref.field));
        if(usage_selection_&&ref==*usage_selection_)selected=usages_->count()-1;
    }
    usages_->setCurrentRow(selected);select_usage(selected);
}
void ColorTools::select_usage(int row) {
    const auto group=used_->currentRow();
    if(group<0||static_cast<std::size_t>(group)>=used_colors_.size()||row<0||static_cast<std::size_t>(row)>=used_colors_[static_cast<std::size_t>(group)].refs.size()) {
        usage_selection_.reset();replace_actions(usage_actions_,nullptr);return;
    }
    usage_selection_=used_colors_[static_cast<std::size_t>(group)].refs[static_cast<std::size_t>(row)];
    replace_actions(usage_actions_,menu_button(*usage_selection_));
}
void ColorTools::refresh_history() {
    if(!manager_||!copied_)return;const auto row=copied_->currentRow();copied_->clear();
    for(const auto& value:history_) {
        auto* item=new QListWidgetItem(swatch(value),hex_color(value),copied_);item->setToolTip(precise_color(value));item->setData(Qt::UserRole,serialized(color_json(value)));
    }
    if(!history_.empty())copied_->setCurrentRow(std::clamp(row,0,static_cast<int>(history_.size())-1));
}
} // namespace nect::desktop
