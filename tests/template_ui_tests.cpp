#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;

namespace {
void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}

template<class T>
T* visible_child(QWidget& parent,const char* name) {
    for(auto* candidate:parent.findChildren<T*>(QString::fromLatin1(name)))
        if(candidate->isVisible())return candidate;
    return nullptr;
}

QPushButton* button(Window& window,const char* name) {
    auto* found=visible_child<QPushButton>(window,name);
    check(found!=nullptr,"Expected named public Window button");
    check(found->isEnabled(),"Expected named public Window button to be enabled for the captured fixture");return found;
}

QDialog* dialog_named(const QString& name) {
    for(auto* widget:QApplication::topLevelWidgets())
        if(auto* dialog=qobject_cast<QDialog*>(widget);dialog&&dialog->objectName()==name)return dialog;
    return nullptr;
}

template<class Configure>
void apply_dialog(Window& window,const char* action,const char* dialog_name,Configure configure) {
    std::exception_ptr failure;
    QTimer::singleShot(0,&window,[&failure,&window,action,dialog_name,configure=std::move(configure)]() mutable {
        auto* dialog=dialog_named(QString::fromLatin1(dialog_name));
        if(!dialog) {
            QString observed;
            for(auto* widget:QApplication::topLevelWidgets())
                observed+=QString(" %1#%2").arg(widget->metaObject()->className(),widget->objectName());
            failure=std::make_exception_ptr(std::runtime_error(
                QString("Action %1 did not open %2; status=%3; active=%4; top-level=%5")
                    .arg(QString::fromLatin1(action),QString::fromLatin1(dialog_name),window.statusBar()->currentMessage(),
                        QApplication::activeModalWidget()?QApplication::activeModalWidget()->objectName():QString("none"),observed).toStdString()));
            if(auto* active=qobject_cast<QDialog*>(QApplication::activeModalWidget()))active->reject();
            return;
        }
        try {
            QTimer::singleShot(1500,dialog,[&failure,dialog] {
                failure=std::make_exception_ptr(std::runtime_error("Named Apply did not close its modal dialog"));
                dialog->reject();
            });
            configure(*dialog);
            auto* buttons=dialog->findChild<QDialogButtonBox*>();
            if(!buttons||!buttons->button(QDialogButtonBox::Apply))
                throw std::runtime_error("Expected a real Apply button in Template dialog");
            buttons->button(QDialogButtonBox::Apply)->click();
        } catch(...) {
            failure=std::current_exception();dialog->reject();
        }
    });
    if(auto* named_button=visible_child<QPushButton>(window,action))named_button->click();
    else if(auto* named_action=window.findChild<QAction*>(QString::fromLatin1(action)))named_action->trigger();
    else check(false,"Expected named public action");
    QApplication::processEvents();
    if(failure)std::rethrow_exception(failure);
}

QComboBox* combo(QDialog& dialog,const char* name) {
    auto* found=dialog.findChild<QComboBox*>(QString::fromLatin1(name));
    check(found!=nullptr,"Expected named source/field selector");return found;
}

void choose(QComboBox* selector,const QString& stable_id) {
    const auto index=selector->findData(stable_id);
    check(index>=0,"Stable ID is present in the user-facing selector");
    selector->setCurrentIndex(index);
}

void select_artboard(Window& window,const QString& stable_id) {
    auto* list=window.findChild<QListWidget*>("artboards");
    check(list!=nullptr,"Named Artboard list exists");
    for(int i=0;i<list->count();++i) {
        auto* item=list->item(i);
        if(item->data(Qt::UserRole+1).toString()==stable_id) {
            list->setCurrentItem(item);QApplication::processEvents();return;
        }
    }
    throw std::runtime_error("Stable Artboard ID is present in the public Artboard list");
}

QTreeWidgetItem* find_tree_item(QTreeWidgetItem* parent,const QString& stable_id) {
    if(parent->data(0,Qt::UserRole).toString()==stable_id)return parent;
    for(int i=0;i<parent->childCount();++i)
        if(auto* found=find_tree_item(parent->child(i),stable_id))return found;
    return nullptr;
}

void select_object(Window& window,const QString& stable_id) {
    auto* tree=window.findChild<QTreeWidget*>();check(tree!=nullptr,"Public Objects tree exists");
    QTreeWidgetItem* found=nullptr;
    for(int i=0;i<tree->topLevelItemCount()&&!found;++i)found=find_tree_item(tree->topLevelItem(i),stable_id);
    check(found!=nullptr,"Stable content Instance ID is present in the public Objects tree");
    tree->setCurrentItem(found);QApplication::processEvents();
    check(window.canvas->selected_objects()==std::vector<Id>{stable_id.toStdString()},"Public Objects tree selects the exact content Instance");
}

std::optional<Bounds> projected_bounds(const Document& document,const Id& instance,const Id& source) {
    const auto values=evaluate(document);
    const auto evaluated=evaluate_scene(document,"ui-comp",values,evaluate_transforms(document,values));
    if(!evaluated.expanded_document||!evaluated.expanded_values||!evaluated.expanded_transforms)return std::nullopt;
    for(const auto& [proxy,owner]:evaluated.instance_owners) {
        if(owner==instance&&evaluated.instance_sources.at(proxy)==source)
            return object_bounds(*evaluated.expanded_document,proxy,*evaluated.expanded_values,
                *evaluated.expanded_transforms,true);
    }
    return std::nullopt;
}

QAction* action_text(Window& window,const QString& text) {
    for(auto* action:window.findChildren<QAction*>())if(action->text()==text)return action;
    return nullptr;
}

void create_template(Window& window,const QString& name,const QString& source_artboard,
                     const QString& definition_id={}) {
    apply_dialog(window,"artboard-template-create","create-artboard-template-dialog",
        [name,source_artboard,definition_id](QDialog& dialog) {
            auto* title=dialog.findChild<QLineEdit*>("template-create-name");
            check(title!=nullptr,"Template name control exists");title->setText(name);
            auto* source=combo(dialog,"template-source-artboard");
            const auto source_index=source->findData(source_artboard);
            check(source_index>=0,"Stable source Artboard is present in the named selector");
            if(source_artboard=="source-art")
                check(source->itemText(source_index).contains("frame 2"),
                    "Same-named source Artboards have a user-facing frame disambiguator");
            choose(source,source_artboard);
            choose(combo(dialog,"template-definition-selector"),definition_id);
        });
}

void set_template_field(Window& window,const QString& field,double value=0) {
    apply_dialog(window,"artboard-template-set-override","set-artboard-template-override-dialog",
        [field,value](QDialog& dialog) {
            choose(combo(dialog,"template-override-field"),field);
            const auto numeric=[&](const char* name,double v) {
                auto* spin=dialog.findChild<QDoubleSpinBox*>(QString::fromLatin1(name));
                check(spin!=nullptr,"Normal numeric Template field control exists");spin->setValue(v);
            };
            if(field=="frame.width")numeric("template-frame-width",value);
            if(field=="frame.height")numeric("template-frame-height",value);
            if(field=="layout.margin") {
                numeric("template-margin-left",5);numeric("template-margin-top",5);
                numeric("template-margin-right",5);numeric("template-margin-bottom",5);
            }
            if(field=="layout.grid") {
                numeric("template-grid-x",30);numeric("template-grid-y",30);
                numeric("template-grid-width",1200);numeric("template-grid-height",840);
                auto* columns=dialog.findChild<QSpinBox*>("template-grid-columns");
                auto* rows=dialog.findChild<QSpinBox*>("template-grid-rows");
                check(columns&&rows,"Normal Grid count controls exist");columns->setValue(3);rows->setValue(2);
            }
        });
}

void reset_template_field(Window& window,const QString& field) {
    apply_dialog(window,"artboard-template-reset-override","reset-artboard-template-override-dialog",
        [field](QDialog& dialog){choose(combo(dialog,"template-reset-field"),field);});
}

QJsonObject set_width_via_public_host_api(Window& window,double value) {
    const auto revision=window.host.session.revision();
    QJsonObject command{{"type","set_artboard_template_override"},{"composition","ui-comp"},
        {"artboard","target"},{"field","frame.width"},{"value",value}};
    QJsonObject request{{"op","apply"},{"expected_revision",static_cast<qint64>(revision)},
        {"commands",QJsonArray{command}}};
    QJsonObject envelope{{"op","core"},{"session_id",window.host.session_id},
        {"document_id","ui-doc"},{"request",request}};
    const auto response=QJsonDocument::fromJson(window.host.dispatch(
        QJsonDocument(envelope).toJson(QJsonDocument::Compact))).object();
    check(response.value("ok").toBool(),"External canonical Host API edit succeeds while the modal remains open");
    return response;
}

void set_descendant_override(Window& window,const QString& instance,const QString& source,
                             const QString& field,double value) {
    select_object(window,instance);
    apply_dialog(window,"set-instance-override","set-instance-override-dialog",
        [source,field,value](QDialog& dialog) {
            auto* source_selector=combo(dialog,"set-instance-override-source");
            auto* property_selector=combo(dialog,"set-instance-override-property");
            choose(source_selector,source);choose(property_selector,field);
            auto* scalar=dialog.findChild<QDoubleSpinBox*>("set-instance-override-value");
            check(scalar!=nullptr,"R04 descendant Scalar control exists");scalar->setValue(value);
        });
}

void window_template_command_parity() {
    QTemporaryDir recovery;check(recovery.isValid(),"Owned recovery scratch exists");
    QSettings scratch_settings(recovery.filePath("folder-library.ini"),QSettings::IniFormat);
    auto scratch_library=std::make_unique<FolderLibrary>(scratch_settings);
    Window window(recovery.path(),std::move(scratch_library));
    auto document=empty_document("ui-doc","ui-comp","target");
    auto& composition=document.compositions.front();
    composition.artboards.front()={"target","Same frame",1400,100,1200,900};
    composition.artboards.front().local_guides.push_back(
        {"high-precision-guide","High precision","x",50.1234567890123,true});
    Artboard source{"source-art","Same frame",0,0,1200,900};
    Artboard spacer{"spacer","Spacer",0,0,40,40};
    ArtboardLayout source_layout;source_layout.margin=Margin{5,5,5,5};
    source_layout.grid=Grid{"source-grid",{20,20,1160,860},2,2,20,20};
    composition.roots={"source-root"};
    Object root;root.id="source-root";root.name="Logo source";root.kind=Kind::group;root.children={"logo-path"};
    Object path;path.id="logo-path";path.name="Logo mark";path.kind=Kind::path;
    path.source=default_primitive("logo-rectangle","nect.shape.rectangle");
    path.source->parameters.at("width").literal=80;path.source->parameters.at("height").literal=40;
    document.objects.emplace(root.id,root);document.objects.emplace(path.id,path);
    window.host.session=Session(std::move(document));
    window.host.session.apply({AddArtboard{"ui-comp",source,1},AddArtboard{"ui-comp",spacer,2},
        ArtboardGuideCommand{AddArtboardGuide{"ui-comp","source-art",
            {"source-precise","Precise source Guide","x",50.1234567890123,true}}},
        SetArtboardLayout{"ui-comp","source-art",source_layout},
        DefinitionCommand{CreateDefinition{{"logo-definition","Logo","source-root"}}},
        DefinitionCommand{CreateInstance{"ui-comp","","existing-instance","logo-definition","Existing Logo"}},
        GridBoundsXCommand{SetGridBoundsXExpression{{"source-grid","","grid.bounds.x"},
            {R"(ref("spacer","","artboard.width") * 0 + 20)",1},false}}},window.host.session.revision());
    window.refresh();window.show();QApplication::processEvents();

    select_artboard(window,"target");
    button(window,"artboard-edit")->click();QApplication::processEvents();
    check(visible_child<QWidget>(window,"artboard-template-panel")!=nullptr,
        "The Artboard editor exposes named Template controls");
    auto* guide_selector=visible_child<QComboBox>(window,"artboard-guide-selector");
    check(guide_selector!=nullptr,"The captured Artboard exposes its local Guide selector");
    choose(guide_selector,"high-precision-guide");
    apply_dialog(window,"artboard-guide-edit","edit-artboard-guide-dialog",[](QDialog& dialog) {
        auto* name=dialog.findChild<QLineEdit*>("artboard-guide-name");
        auto* enabled=dialog.findChild<QCheckBox*>("artboard-guide-enabled");
        auto* position=dialog.findChild<QDoubleSpinBox*>("artboard-guide-position");
        check(name&&enabled&&position,"Local Guide Edit exposes name, enabled and numeric position controls");
        check(position->value()==50.123,"The editor presents the configured three-decimal Guide control");
        name->setText("Renamed precise Guide");enabled->setChecked(false);
    });
    const auto& precise_guide=window.host.session.document().compositions.front().artboards.front().local_guides.front();
    check(precise_guide.name=="Renamed precise Guide"&&!precise_guide.enabled&&
        precise_guide.position==50.1234567890123,
        "Editing only Guide name/enabled preserves the exact untouched high-precision literal");
    create_template(window,"Shared","source-art","");
    create_template(window,"Shared","source-art","logo-definition");
    const auto& templates=window.host.session.document().compositions.front().templates;
    check(templates.size()==2&&templates[0].name==templates[1].name&&templates[0].id!=templates[1].id&&
        !templates[0].definition&&templates[1].definition==std::optional<Id>{"logo-definition"},
        "Two same-named Templates retain distinct stable IDs and the named Definition selector creates the second exact relation");
    const Id first_template=templates[0].id,content_template=templates[1].id;

    apply_dialog(window,"artboard-template-rename","rename-artboard-template-dialog",
        [first_template](QDialog& dialog) {
            choose(combo(dialog,"template-rename-selector"),QString::fromStdString(first_template));
            auto* name=dialog.findChild<QLineEdit*>("template-rename-name");
            check(name!=nullptr,"Template rename control exists");name->setText("First Template");
        });
    check(window.host.session.document().compositions.front().templates[0].id==first_template&&
        window.host.session.document().compositions.front().templates[0].name=="First Template"&&
        window.host.session.document().compositions.front().templates[1].id==content_template,
        "Named Apply renames exactly the stable Template selected from duplicate display names");

    auto* panel_selector=visible_child<QComboBox>(window,"artboard-template-selector");
    choose(panel_selector,QString::fromStdString(content_template));
    apply_dialog(window,"artboard-template-assign","assign-artboard-template-dialog",
        [&window,content_template](QDialog& dialog) {
            auto* selector=combo(dialog,"template-assign-selector");
            check(selector->currentData().toString()==QString::fromStdString(content_template),
                "The panel's selected stable Template ID preselects the modal even when it is not first");
            check(selector->currentText().contains("Definition Logo"),
                "Same-named Template choices expose their Definition in user-facing labels");
            auto* include=dialog.findChild<QCheckBox*>("template-create-content-instance");
            check(include&&include->isEnabled()&&include->isChecked(),
                "Definition-backed Template visibly creates a fresh content Instance");
            select_artboard(window,"source-art");
        });
    const auto& after_browse=window.host.session.document().compositions.front();
    const auto target_after_browse=std::find_if(after_browse.artboards.begin(),after_browse.artboards.end(),
        [](const Artboard& item){return item.id=="target";});
    const auto source_after_browse=std::find_if(after_browse.artboards.begin(),after_browse.artboards.end(),
        [](const Artboard& item){return item.id=="source-art";});
    check(target_after_browse->template_assignment&&target_after_browse->template_assignment->template_id==content_template&&
        source_after_browse->template_assignment==std::nullopt,
        "Browsing to a same-named source while Assign is open cannot retarget its captured target Artboard");
    check(target_after_browse->template_assignment->content_instance&&
        *target_after_browse->template_assignment->content_instance!="existing-instance",
        "The UI creates a fresh owned content Instance instead of reusing the ordinary existing Instance");
    const Id content_instance=*target_after_browse->template_assignment->content_instance;
    select_artboard(window,"target");
    const auto before_content_duplicate=encode(window.host.session.document());
    const auto before_content_duplicate_revision=window.host.session.revision();
    button(window,"artboard-duplicate")->click();QApplication::processEvents();
    check(encode(window.host.session.document())==before_content_duplicate&&
        window.host.session.revision()==before_content_duplicate_revision&&
        window.canvas->active_artboard()=="target"&&
        window.statusBar()->currentMessage().startsWith("ARTBOARD_DUPLICATE_CONTENT_UNSUPPORTED"),
        "Public Duplicate refuses a Template-owned content Instance without sharing or mutating identity");
    select_artboard(window,"target");button(window,"artboard-edit")->click();QApplication::processEvents();
    auto* precision_selector=visible_child<QComboBox>(window,"artboard-guide-selector");
    check(precision_selector!=nullptr,"The assigned Template exposes inherited Guide occurrences");
    choose(precision_selector,"source-precise");
    apply_dialog(window,"artboard-guide-override","set-artboard-guide-override-dialog",[](QDialog& dialog) {
        choose(combo(dialog,"artboard-guide-override-field"),"position");
        auto* position=dialog.findChild<QDoubleSpinBox*>("artboard-guide-override-position");
        check(position&&position->value()==50.123,"Override control presents the configured numeric Guide position");
    });
    check(window.host.session.document().compositions.front().artboards.front().template_assignment->
        guide_position_overrides.at("source-precise")==50.1234567890123,
        "Freezing an inherited Guide position without changing its numeric control preserves the exact source double");
    apply_dialog(window,"artboard-guide-reset","reset-artboard-guide-override-dialog",[](QDialog& dialog) {
        choose(combo(dialog,"artboard-guide-reset-field"),"position");
    });
    check(!window.host.session.document().compositions.front().artboards.front().template_assignment->
        guide_position_overrides.contains("source-precise"),
        "Reset clears only the precision regression's temporary position override");
    const auto& content_object=window.host.session.document().objects.at(content_instance);
    check(content_object.kind==Kind::instance&&content_object.instance&&
        content_object.instance->definition=="logo-definition"&&content_object.transform[4].literal==1400&&
        content_object.transform[5].literal==100&&
        window.host.session.document().objects.at("existing-instance").instance->definition=="logo-definition",
        "Named Assign creates ordinary R04 Definition content at the captured target origin and preserves existing content");

    select_artboard(window,"target");button(window,"artboard-edit")->click();QApplication::processEvents();
    auto* frame_x=visible_child<QLineEdit>(window,"artboard-x");
    check(frame_x!=nullptr,"Ordinary Artboard controls remain available after a model refresh");
    const auto before_frame_edit=window.host.session.revision();frame_x->setText("1410");frame_x->setModified(true);
    check(QMetaObject::invokeMethod(frame_x,"editingFinished",Qt::DirectConnection),
        "Ordinary Artboard frame action reaches the public editingFinished handler");
    check(window.host.session.revision()==before_frame_edit+1&&
        window.host.session.document().compositions.front().artboards.front().x==1410&&
        window.host.session.document().objects.at(content_instance).transform[4].literal==1400,
        "A normal Artboard crop control applies after refresh while preserving captured Template content placement");

    select_artboard(window,"target");
    apply_dialog(window,"artboard-template-set-override","set-artboard-template-override-dialog",
        [&window](QDialog& dialog) {
            choose(combo(dialog,"template-override-field"),"frame.width");
            auto* spin=dialog.findChild<QDoubleSpinBox*>("template-frame-width");
            check(spin!=nullptr,"Frame width is a normal numeric control");spin->setValue(1300);
        });
    check(window.host.session.document().compositions.front().artboards.front().template_assignment->width_override==1300&&
        evaluate_artboard(window.host.session.document().compositions.front(),"target").width==1300,
        "Named Apply sets only the captured target's authored/evaluated Template frame width");

    auto stale_api_response=QJsonObject{};
    apply_dialog(window,"artboard-template-set-override","set-artboard-template-override-dialog",
        [&window,&stale_api_response](QDialog& dialog) {
            choose(combo(dialog,"template-override-field"),"frame.width");
            auto* spin=dialog.findChild<QDoubleSpinBox*>("template-frame-width");
            check(spin!=nullptr,"Stale frame editor remains open for canonical API edit");spin->setValue(1500);
            stale_api_response=set_width_via_public_host_api(window,1250);
        });
    check(stale_api_response.value("ok").toBool()&&window.host.session.revision()==
        static_cast<std::uint64_t>(stale_api_response.value("revision").toInteger())&&
        evaluate_artboard(window.host.session.document().compositions.front(),"target").width==1250&&
        window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT"),
        "External canonical Host API edit makes the open modal refuse Apply without overwriting its newer value");

    set_template_field(window,"layout.margin");
    const auto margin_state=window.host.session.document().compositions.front().artboards.front();
    check(margin_state.template_assignment->margin_overridden&&margin_state.layout&&margin_state.layout->margin&&
        margin_state.layout->margin->left==5&&margin_state.layout->margin->right==5,
        "Named numeric Margin family Apply creates the captured target's local family");
    set_template_field(window,"layout.margin.absent");
    check(window.host.session.document().compositions.front().artboards.front().template_assignment->margin_overridden&&
        (!window.host.session.document().compositions.front().artboards.front().layout||
         !window.host.session.document().compositions.front().artboards.front().layout->margin),
        "The explicit local-absent Margin choice suppresses the inherited family");
    reset_template_field(window,"layout.margin");
    check(!window.host.session.document().compositions.front().artboards.front().template_assignment->margin_overridden&&
        evaluate_artboard(window.host.session.document().compositions.front(),"target").layout->margin->left==5,
        "Named Reset Apply restores Margin inheritance independently");

    set_template_field(window,"layout.grid");
    const auto grid_state=window.host.session.document().compositions.front().artboards.front();
    check(grid_state.template_assignment->grid_overridden&&grid_state.layout&&grid_state.layout->grid&&
        grid_state.layout->grid->id==grid_state.template_assignment->grid_id&&
        grid_state.layout->grid->bounds.x==30&&!grid_state.layout->grid->bounds_x_driver&&
        !grid_state.layout->grid->bounds_x_expression,
        "Grid editor constructs a target-ID literal from evaluated inherited values without smuggling source metadata");
    set_template_field(window,"layout.grid.absent");
    check(window.host.session.document().compositions.front().artboards.front().template_assignment->grid_overridden&&
        (!window.host.session.document().compositions.front().artboards.front().layout||
         !window.host.session.document().compositions.front().artboards.front().layout->grid),
        "The explicit local-absent Grid choice suppresses an inherited driven Grid");
    reset_template_field(window,"layout.grid");
    check(!window.host.session.document().compositions.front().artboards.front().template_assignment->grid_overridden&&
        evaluate_artboard(window.host.session.document().compositions.front(),"target").layout->grid->bounds_x_expression.has_value(),
        "Named Grid reset restores the source Grid expression as a live inherited family");
    reset_template_field(window,"frame.width");
    check(!window.host.session.document().compositions.front().artboards.front().template_assignment->width_override&&
        evaluate_artboard(window.host.session.document().compositions.front(),"target").width==1200,
        "Frame width reset restores inheritance without changing layout ownership");

    set_descendant_override(window,QString::fromStdString(content_instance),"logo-path","generator.width",140);
    auto overrides=window.host.session.document().objects.at(content_instance).instance->overrides;
    check(overrides.contains({"logo-path","","generator.width"})&&
        overrides.at({"logo-path","","generator.width"})==140,
        "Public R04 Instance action exposes and writes a descendant Rectangle generator override");
    set_descendant_override(window,QString::fromStdString(content_instance),"logo-path","transform.tx",24);
    overrides=window.host.session.document().objects.at(content_instance).instance->overrides;
    check(overrides.contains({"logo-path","","transform.tx"})&&
        overrides.at({"logo-path","","transform.tx"})==24,
        "Public R04 Instance action exposes and writes descendant transform offsets");

    select_artboard(window,"target");button(window,"artboard-edit")->click();QApplication::processEvents();
    check(visible_child<QWidget>(window,"artboard-template-panel")!=nullptr,
        "Returning to the captured Artboard exposes Template actions after browsing Objects");
    const auto attached_native=encode(window.host.session.document());
    const auto attached_bounds=projected_bounds(window.host.session.document(),content_instance,"logo-path");
    check(attached_bounds.has_value(),"Assigned Definition content has concrete projected world bounds before detach");
    const auto before_detach_revision=window.host.session.revision();
    button(window,"artboard-template-detach")->click();QApplication::processEvents();
    const auto detached=window.host.session.document().compositions.front().artboards.front();
    const auto& detached_group=window.host.session.document().objects.at(content_instance);
    if(detached.template_assignment)
        throw std::runtime_error("Named Detach did not clear the Template relation; revision="+
            std::to_string(before_detach_revision)+"->"+std::to_string(window.host.session.revision())+
            "; status="+window.statusBar()->currentMessage().toStdString());
    check(detached_group.kind==Kind::group&&!detached_group.instance,
        "Template detach materializes the owned Instance as an ordinary Group");
    check(detached_group.children.size()==1,"Materialized content Group owns its fresh Definition root");
    check(window.host.session.document().objects.at("existing-instance").kind==Kind::instance,
        "Template detach leaves unrelated existing Instances untouched");
    const auto detached_root=detached_group.children.front();
    const auto detached_logo=window.host.session.document().objects.at(detached_root).children.front();
    const auto detached_values=evaluate(window.host.session.document());
    const auto detached_transforms=evaluate_transforms(window.host.session.document(),detached_values);
    const auto detached_bounds=object_bounds(window.host.session.document(),detached_logo,detached_values,detached_transforms,true);
    check(detached_bounds.has_value()&&attached_bounds.has_value(),
        "Detached ordinary artwork retains concrete projected world geometry");
    check(std::abs(detached_bounds->left-attached_bounds->left)<1e-8&&
        std::abs(detached_bounds->top-attached_bounds->top)<1e-8&&
        std::abs(detached_bounds->right-attached_bounds->right)<1e-8&&
        std::abs(detached_bounds->bottom-attached_bounds->bottom)<1e-8,
        "Detach preserves exact visible placement and bounds including target origin and descendant overrides");
    const auto detached_native=encode(window.host.session.document());
    auto* undo=action_text(window,"Undo");auto* redo=action_text(window,"Redo");
    check(undo&&redo&&undo->isEnabled(),"Public Undo action is enabled after detaching the Template");
    undo->trigger();QApplication::processEvents();
    check(encode(window.host.session.document())==attached_native&&
        window.host.session.document().objects.at(content_instance).kind==Kind::instance,
        "Public Undo restores the exact assigned Template and owned R04 Instance state");
    check(redo->isEnabled(),"Public Redo action is enabled after Undo");
    redo->trigger();QApplication::processEvents();
    check(encode(window.host.session.document())==detached_native&&
        window.host.session.document().objects.at(content_instance).kind==Kind::group,
        "Public Redo restores the exact materialized Group and projected geometry state");
    apply_dialog(window,"artboard-template-delete","delete-artboard-template-dialog",
        [content_template](QDialog& dialog){choose(combo(dialog,"template-delete-selector"),QString::fromStdString(content_template));});
    check(std::none_of(window.host.session.document().compositions.front().templates.begin(),
        window.host.session.document().compositions.front().templates.end(),
        [&](const ArtboardTemplate& item){return item.id==content_template;}),
        "Named Delete Apply removes exactly the selected, now-unused Template ID");
    apply_dialog(window,"artboard-template-delete","delete-artboard-template-dialog",
        [first_template](QDialog& dialog){choose(combo(dialog,"template-delete-selector"),QString::fromStdString(first_template));});
    check(window.host.session.document().compositions.front().templates.empty()&&
        window.host.session.document().objects.contains(content_instance),
        "The remaining Template can be deleted without recreating or deleting detached ordinary content");

    const Id copy_template_id="copy-template";
    window.host.session.apply({
        StructuralCommand{ArtboardTemplateCommand{CreateArtboardTemplate{"ui-comp",
            {copy_template_id,"Copy Template","source-art",std::nullopt}}}},
        StructuralCommand{ArtboardTemplateCommand{AssignArtboardTemplate{"ui-comp","target",copy_template_id,std::nullopt}}}
    },window.host.session.revision());
    select_artboard(window,"target");
    const auto original_local_guides=window.host.session.document().compositions.front().artboards.front().local_guides;
    button(window,"artboard-duplicate")->click();QApplication::processEvents();
    const auto copied_artboard_id=window.canvas->active_artboard();
    const auto& copied_composition=window.host.session.document().compositions.front();
    const auto copied_artboard=std::find_if(copied_composition.artboards.begin(),copied_composition.artboards.end(),
        [&](const Artboard& item){return item.id==copied_artboard_id;});
    const auto copy_details=copied_artboard==copied_composition.artboards.end()?std::string("missing"):
        "template="+(copied_artboard->template_assignment?copied_artboard->template_assignment->template_id:"none")+
        " guides="+std::to_string(copied_artboard->local_guides.size())+
        (copied_artboard->local_guides.empty()?std::string{}:
            " first="+copied_artboard->local_guides.front().id+"/"+copied_artboard->local_guides.front().name+
            "/"+std::to_string(copied_artboard->local_guides.front().position)+"/"+
            std::to_string(copied_artboard->local_guides.front().enabled));
    check(copied_artboard!=copied_composition.artboards.end()&&copied_artboard->template_assignment&&
        copied_artboard->template_assignment->template_id==copy_template_id&&
        copied_artboard->local_guides.size()==original_local_guides.size()&&
        std::equal(copied_artboard->local_guides.begin(),copied_artboard->local_guides.end(),
            original_local_guides.begin(),[](const ArtboardGuide& copied,const ArtboardGuide& original) {
                return copied.id!=original.id&&copied.name==original.name&&copied.axis==original.axis&&
                    copied.position==original.position&&copied.enabled==original.enabled;
            }),
        ("Public Duplicate copies local Guide state with a fresh ID and retains the existing Template relation: id="+
        copied_artboard_id+" status="+window.statusBar()->currentMessage().toStdString()+" "+copy_details).c_str());
    const auto copied_native=encode(window.host.session.document());
    undo=action_text(window,"Undo");redo=action_text(window,"Redo");
    check(undo&&redo&&undo->isEnabled(),"Public Duplicate creates one undoable Guide-preserving frame action");
    undo->trigger();QApplication::processEvents();
    check(std::none_of(window.host.session.document().compositions.front().artboards.begin(),
        window.host.session.document().compositions.front().artboards.end(),
        [&](const Artboard& item){return item.id==copied_artboard_id;}),
        "One Undo removes the duplicated frame and its copied local Guide");
    redo->trigger();QApplication::processEvents();
    check(encode(window.host.session.document())==copied_native,
        "One Redo restores the same duplicated Guide ID and Template relationship");

    select_artboard(window,"target");
    button(window,"artboard-add")->click();QApplication::processEvents();
    const auto new_artboard_id=window.canvas->active_artboard();
    const auto& after_new=window.host.session.document().compositions.front().artboards;
    const auto new_artboard=std::find_if(after_new.begin(),after_new.end(),
        [&](const Artboard& item){return item.id==new_artboard_id;});
    check(new_artboard!=after_new.end()&&new_artboard_id!="target"&&new_artboard->local_guides.empty(),
        "Public New Artboard starts with no copied authored local Guides");
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        window_template_command_parity();
        std::cout<<"PASS offscreen Window Artboard Template, stale context, and descendant R04 controls\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
    }
}
