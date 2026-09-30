#include "window.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QInputDialog>
#include <QTabWidget>
#include <QTimer>
#include <QPointer>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void events(){QApplication::processEvents();}
template<class T>T* named(Window& window,const QString& name,bool visible=true) {
    events();
    for(auto* item:window.findChildren<T*>())if(item->objectName()==name&&(!visible||item->isVisible()))return item;
    throw std::runtime_error("Missing "+name.toStdString());
}
void trigger(Window& window,const char* name) {
    auto* action=window.findChild<QAction*>(name);check(action,"Window action exists");action->trigger();events();
}
void click(Window& window,const QString& name) {named<QPushButton>(window,name)->click();events();}
void answer_text_dialog_later(Window& window,const QString& text,bool replace_session=false) {
    auto* poll=new QTimer(&window);poll->setInterval(5);
    QObject::connect(poll,&QTimer::timeout,&window,[&window,poll,text,replace_session]{
        auto* dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if(!dialog)return;
        poll->stop();poll->deleteLater();
        dialog->setTextValue(text);
        if(replace_session)window.host.create_document();
        dialog->accept();
    });
    poll->start();
}
void edit_property(Window& window,const Ref& ref,const char* value) {
    const auto key=QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
        {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);
    QLineEdit* input=nullptr;
    for(auto* candidate:window.findChildren<QLineEdit*>())
        if(candidate->isVisible()&&candidate->property("nect-reference").toByteArray()==key){input=candidate;break;}
    check(input,"The selected effect parameter is edited through the normal Properties control");
    input->setFocus();input->selectAll();QTest::keyClicks(input,value);QTest::keyClick(input,Qt::Key_Return);events();
}
QJsonObject api(const std::string& response) {
    const auto parsed=QJsonDocument::fromJson(QByteArray::fromStdString(response));
    check(parsed.isObject(),"Nect API response is JSON object");
    return parsed.object();
}
Bounds bounds(const EvaluatedShape& shape) {
    Bounds result{INFINITY,INFINITY,-INFINITY,-INFINITY};
    for(const auto& instance:shape.paths)for(const auto& contour:*instance.contours)for(const auto& point:contour.points) {
        const auto p=map_point(instance.transform,point.anchor);
        result.left=std::min(result.left,p.x);result.top=std::min(result.top,p.y);
        result.right=std::max(result.right,p.x);result.bottom=std::max(result.bottom,p.y);
    }
    return result;
}
MacroDefinition custom_macro() {
    auto offset=default_operation("effects-macro-offset","nect.shape.offset");
    offset.parameters.at("amount").literal=8;
    auto repeater=default_operation("effects-macro-repeater","nect.shape.repeater");
    repeater.parameters.at("copies").literal=2;
    repeater.parameters.at("position_x").literal=120;
    MacroDefinitionRevision revision;revision.revision=1;
    revision.input={"effects-macro-input","local_paths_and_paint"};
    revision.output={"effects-macro-output","local_paths_and_paint"};
    revision.nodes={{offset,"effects-offset-in","effects-offset-out"},
        {repeater,"effects-repeater-in","effects-repeater-out"}};
    revision.edges={{{"","effects-macro-input"},{"effects-macro-offset","effects-offset-in"}},
        {{"effects-macro-offset","effects-offset-out"},{"effects-macro-repeater","effects-repeater-in"}},
        {{"effects-macro-repeater","effects-repeater-out"},{"","effects-macro-output"}}};
    revision.output_mapping={"effects-macro-repeater","effects-repeater-out"};
    revision.public_parameters.push_back({"macro.offset.amount","Amount","effects-macro-offset","amount",
        "number","du","local_paths_and_paint"});
    MacroDefinition definition;definition.id="effects-custom-macro";definition.label="Custom Offset Repeat";
    definition.revisions.emplace(1,std::move(revision));
    return definition;
}
void same_bounds(const Bounds& a,const Bounds& b) {
    check(std::abs(a.left-b.left)<1e-8&&std::abs(a.top-b.top)<1e-8&&
        std::abs(a.right-b.right)<1e-8&&std::abs(a.bottom-b.bottom)<1e-8,
        "Effects-panel and API Offset produce equivalent derived geometry");
}
QAction* text_action(Window& window,const QString& text) {
    for(auto* action:window.findChildren<QAction*>())if(action->text()==text)return action;
    return nullptr;
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir temp;check(temp.isValid(),"Temporary test directory exists");
        Window window(temp.path());window.show();events();
        auto* effects=window.findChild<QDockWidget*>("effects");check(effects,"Dedicated Effects dock exists");
        effects->show();effects->raise();events();
        effects->close();events();check(!effects->isVisible(),"Effects dock can be closed");
        effects->toggleViewAction()->trigger();effects->raise();events();
        check(effects->isVisible(),"Effects dock can be reopened from its View action");
        auto& session=window.host.session;
        const auto empty=encode(session.document());const auto empty_revision=session.revision();const auto empty_history=session.history();
        click(window,"effects-apply");
        check(encode(session.document())==empty&&session.revision()==empty_revision&&session.history()==empty_history,
            "Apply without a selection is nonmutating");
        auto* status=named<QLabel>(window,"effects-status");
        check(status->text().contains("Target: none")&&status->text().contains("nect.shape.offset")&&status->text().contains("INVALID_DOMAIN"),
            "No-selection refusal names target, operator and reason in Effects");

        auto* search=named<QLineEdit>(window,"effects-search");
        search->setText("OFFsET");events();
        auto* catalog=named<QListWidget>(window,"effects-catalog");
        check(!catalog->item(0)->isHidden(),"Effects catalog search is case-insensitive");
        const auto builtin_definitions=api(request(session,R"({"op":"operator_types"})")).value("result").toArray();
        int catalog_index=0;
        for(const auto& descriptor:builtin_operation_types())if(descriptor.effects_catalog) {
            check(catalog_index<catalog->count(),"Executable built-in descriptor has an Effects catalog row");
            const auto* item=catalog->item(catalog_index++);
            QJsonObject definition;
            for(const auto value:builtin_definitions)if(value.toObject().value("type").toString()==QString::fromStdString(descriptor.type))
                definition=value.toObject();
            check(!definition.isEmpty()&&item->text()==QString::fromStdString(descriptor.label)&&
                item->data(Qt::UserRole).toString()==QString::fromStdString(descriptor.type)&&
                definition.value("version").toInt()==static_cast<int>(descriptor.version)&&
                definition.value("input").toString()==QString::fromStdString(descriptor.input)&&
                definition.value("output").toString()==QString::fromStdString(descriptor.output)&&
                definition.value("target_kind").toString()==QString::fromStdString(descriptor.target_kind),
                "Effects catalog and API discover the same executable built-in type contract");
        }
        check(catalog_index==catalog->count(),"Effects catalog contains only advertised built-in effects");
        const auto search_revision=session.revision();const auto search_document=encode(session.document());
        search->setText("unavailable effect");events();
        check(!named<QPushButton>(window,"effects-apply")->isEnabled()&&
            named<QLabel>(window,"effects-no-results")->isVisible()&&session.revision()==search_revision&&encode(session.document())==search_document,
            "Empty search result is clear and nonmutating");
        search->setText("offset");events();

        trigger(window,"add-rectangle");
        const auto ui_object=window.canvas->selected_object;
        auto* target=named<QLabel>(window,"effects-target");
        check(target->text().contains(QString::fromStdString(ui_object))&&target->text().contains("Rectangle"),
            "Effects panel reports the exact selected primitive ID and type");
        auto fill=default_operation("preexisting-fill","nect.paint.fill");
        const auto fill_id=fill.id;
        session.apply({AddOperation{ui_object,std::move(fill),session.document().objects.at(ui_object).stack.size()}},session.revision());
        window.host.edited();events();
        const auto apply_revision=session.revision();
        click(window,"effects-apply");
        check(session.revision()==apply_revision+1,"Effects Apply commits one Session revision");
        const auto& with_offset=session.document().objects.at(ui_object);
        check(!with_offset.stack.empty()&&with_offset.stack.back().type=="nect.shape.offset"&&with_offset.stack.back().version==1,
            "Effects Apply creates the real Offset operator at behavior v1");
        const auto offset_id=with_offset.stack.back().id;
        const auto amount=operation_ref(ui_object,offset_id,"amount");
        auto* edit=named<QPushButton>(window,"effects-edit-properties-"+QString::fromStdString(offset_id));
        edit->click();events();
        auto* properties=window.findChild<QDockWidget*>("properties");check(properties&&properties->isVisible(),
            "Effects action opens the normal Properties dock");
        auto* operation_group=named<QGroupBox>(window,"stack-operation-"+QString::fromStdString(offset_id));
        check(operation_group->isVisible(),"Effects navigation reveals the exact Offset instance in Properties");
        const auto edit_revision=session.revision();edit_property(window,amount,"12");
        check(session.revision()==edit_revision+1&&evaluate(session.document()).at(amount)==12,
            "Amount uses the ordinary property command and one revision");
        const auto ui_enabled_bounds=bounds(evaluate_shape(session.document(),ui_object,evaluate(session.document())));
        const auto& edited_stack=session.document().objects.at(ui_object).stack;
        const auto offset_position=std::find_if(edited_stack.begin(),edited_stack.end(),[&](const auto& item){return item.id==offset_id;});
        const auto fill_position=std::find_if(edited_stack.begin(),edited_stack.end(),[&](const auto& item){return item.id==fill_id;});
        check(offset_position!=edited_stack.end()&&fill_position!=edited_stack.end()&&fill_position<offset_position,
            "The target has a Fill before testing relative operation reorder");

        const auto reorder_revision=session.revision();
        click(window,"operation-up-"+QString::fromStdString(offset_id));
        const auto& reordered=session.document().objects.at(ui_object).stack;
        const auto moved_offset=std::find_if(reordered.begin(),reordered.end(),[&](const auto& item){return item.id==offset_id;});
        const auto moved_fill=std::find_if(reordered.begin(),reordered.end(),[&](const auto& item){return item.id==fill_id;});
        check(session.revision()==reorder_revision+1&&moved_offset<moved_fill,
            "Properties reorder moves the stable Offset instance atomically");
        auto* enabled=named<QCheckBox>(window,"operation-enabled-"+QString::fromStdString(offset_id));
        const auto bypass_revision=session.revision();enabled->setChecked(false);events();
        check(session.revision()==bypass_revision+1,"Bypass is one Session edit");
        const auto& bypassed=session.document().objects.at(ui_object).stack;
        const auto bypassed_offset=std::find_if(bypassed.begin(),bypassed.end(),[&](const auto& item){return item.id==offset_id;});
        check(bypassed_offset!=bypassed.end()&&!bypassed_offset->enabled,"Offset bypass state is authored");
        const auto panel_authored=encode(session.document());
        const auto file=temp.filePath("effects-panel.nect");window.host.save(file);window.host.open(file);events();
        check(encode(session.document())==panel_authored,"Saved panel-authored native document cold-reopens exactly");
        const auto& reopened=session.document().objects.at(ui_object);
        const auto reopened_offset=std::find_if(reopened.stack.begin(),reopened.stack.end(),[&](const auto& item){return item.id==offset_id;});
        check(reopened_offset!=reopened.stack.end()&&reopened_offset->type=="nect.shape.offset"&&reopened_offset->version==1&&
            reopened_offset->parameters.at("amount").literal==12&&!reopened_offset->enabled,
            "Native readback retains Offset identity, type, version, parameter and bypass");

        trigger(window,"add-rectangle");const auto api_object=window.canvas->selected_object;
        window.canvas->set_selection(ui_object);effects->raise();events();
        auto* current_edit=named<QPushButton>(window,"effects-edit-properties-"+QString::fromStdString(offset_id));
        QPointer<QPushButton> stale_navigation(current_edit);
        window.canvas->set_selection(api_object);
        const auto stale_revision=session.revision();
        check(stale_navigation.data()!=nullptr,"Stale panel action fixture exists");stale_navigation->click();events();
        check(session.revision()==stale_revision&&named<QLabel>(window,"effects-status")->text().contains("nect.shape.offset")&&
            named<QLabel>(window,"effects-status")->text().contains("REVISION_CONFLICT"),
            "Old Effects navigation refuses after the selected target changes");

        const auto definitions=api(request(session,R"({"op":"operator_types"})"));
        check(definitions.value("ok").toBool(),"Operator definitions are readable through the existing API");
        QJsonObject offset_definition;
        for(const auto value:definitions.value("result").toArray())if(value.toObject().value("type").toString()=="nect.shape.offset")offset_definition=value.toObject();
        check(!offset_definition.isEmpty()&&offset_definition.value("version").toInt()==1,
            "Existing API exposes Offset behavior v1");
        auto api_operation=offset_definition.value("template").toObject();api_operation.insert("id","api-offset");
        auto api_parameters=api_operation.value("parameters").toObject();
        auto api_amount=api_parameters.value("amount").toObject();api_amount.insert("literal",12);api_parameters.insert("amount",api_amount);
        api_operation.insert("parameters",api_parameters);
        const auto api_index=static_cast<int>(session.document().objects.at(api_object).stack.size());
        const auto api_revision=session.revision();
        const QJsonObject command{{"type","add_operation"},{"object",QString::fromStdString(api_object)},
            {"index",api_index},{"operation",api_operation}};
        const auto applied=api(request(session,QJsonDocument(QJsonObject{{"op","apply"},
            {"expected_revision",static_cast<qint64>(api_revision)},{"commands",QJsonArray{command}}}).toJson(QJsonDocument::Compact).toStdString()));
        check(applied.value("ok").toBool()&&session.revision()==api_revision+1,
            "API AddOperation uses the same atomic Session command");
        window.host.edited();events();
        const Ref api_amount_ref{api_object,"","op.api-offset.amount"};
        const auto get=api(request(session,QJsonDocument(QJsonObject{{"op","get"},{"ref",QJsonObject{
            {"object",QString::fromStdString(api_object)},{"point",""},{"field","op.api-offset.amount"}}}})
            .toJson(QJsonDocument::Compact).toStdString()));
        check(get.value("ok").toBool()&&get.value("result").toObject().value("evaluated").toDouble()==12&&
            evaluate(session.document()).at(amount)==12&&evaluate(session.document()).at(api_amount_ref)==12,
            "API readback and GUI-authored Offset expose the same Amount semantics");
        const auto values=evaluate(session.document());
        same_bounds(ui_enabled_bounds,bounds(evaluate_shape(session.document(),api_object,values)));

        window.canvas->set_selection(ui_object);effects->raise();events();
        auto definition=custom_macro();
        session.apply({MacroCommand{CreateMacroDefinition{definition}}},session.revision());
        window.host.edited();events();
        search->setText("Custom Offset Repeat");events();
        auto* macro_item=catalog->currentItem();
        check(macro_item&&!macro_item->isHidden()&&macro_item->data(Qt::UserRole).toString()=="macro:effects-custom-macro"&&
            macro_item->text()=="Custom Offset Repeat"&&named<QPushButton>(window,"effects-apply")->text().contains("Custom Offset Repeat"),
            "Effects search and selection include a document-local Macro definition");
        const auto macro_apply_revision=session.revision();click(window,"effects-apply");
        const auto& macro_applied=session.document().objects.at(ui_object).stack;
        const auto macro_instance=std::find_if(macro_applied.begin(),macro_applied.end(),[](const auto& entry){return entry.macro.has_value();});
        check(session.revision()==macro_apply_revision+1&&macro_instance!=macro_applied.end()&&
            macro_instance->macro->definition==definition.id&&macro_instance->macro->pinned_revision==1,
            "Applying a custom Effects catalog entry creates one pinned Macro stack instance");
        const auto macro_instance_id=macro_instance->id;
        click(window,"effects-edit-properties-"+QString::fromStdString(macro_instance_id));
        auto* macro_amount=named<QDoubleSpinBox>(window,"macro-amount-"+QString::fromStdString(macro_instance_id));
        const Ref macro_amount_ref=macro_parameter_ref(ui_object,macro_instance_id,"macro.offset.amount");
        const auto macro_amount_ref_json=QJsonDocument(QJsonObject{{"object",QString::fromStdString(macro_amount_ref.object)},
            {"point",QString::fromStdString(macro_amount_ref.point)},{"field",QString::fromStdString(macro_amount_ref.field)}})
            .toJson(QJsonDocument::Compact);
        check(macro_amount->suffix().contains("du")&&macro_amount->property("nect-reference").toByteArray()==
            macro_amount_ref_json,
            "Properties exposes the Macro amount as the same canonical stable Ref");
        const auto macro_edit_revision=session.revision();macro_amount->setValue(22);
        QTest::keyClick(macro_amount,Qt::Key_Return);events();
        check(session.revision()==macro_edit_revision+1&&
            macro_parameter_value(session.document(),ui_object,macro_instance_id,"macro.offset.amount")==22,
            "Macro Amount editor writes through one Macro Session command");
        auto* detach=named<QPushButton>(window,"macro-detach-"+QString::fromStdString(macro_instance_id));
        const auto& before_detach=session.document().objects.at(ui_object).stack;
        const auto macro_index=static_cast<std::size_t>(std::distance(before_detach.begin(),
            std::find_if(before_detach.begin(),before_detach.end(),[&](const auto& entry){return entry.id==macro_instance_id;})));
        const auto before_stack=before_detach;
        const auto detach_revision=session.revision();detach->click();events();
        const auto& detached=session.document().objects.at(ui_object).stack;
        check(session.revision()==detach_revision+1&&detached.size()==before_stack.size()+1&&
            detached[macro_index].type=="nect.shape.offset"&&!detached[macro_index].macro&&
            detached[macro_index+1].type=="nect.shape.repeater"&&!detached[macro_index+1].macro&&
            detached[macro_index].id!=macro_instance_id&&detached[macro_index+1].id!=macro_instance_id&&
            detached[macro_index].parameters.at("amount").literal==22,
            "Properties Detach replaces the Macro in place with fresh operations and copies its public value");

        search->setText("offset");events();
        QListWidgetItem* offset_item=nullptr;
        for(int i=0;i<catalog->count();++i)if(catalog->item(i)->data(Qt::UserRole).toString()=="nect.shape.offset")
            offset_item=catalog->item(i);
        check(offset_item,"Built-in Offset remains discoverable after Macro apply");
        catalog->setCurrentItem(offset_item);events();
        window.canvas->set_selection(ui_object);effects->raise();events();
        click(window,"effects-edit-properties-"+QString::fromStdString(offset_id));
        const auto remove_revision=session.revision();click(window,"operation-remove-"+QString::fromStdString(offset_id));
        check(session.revision()==remove_revision+1&&std::none_of(session.document().objects.at(ui_object).stack.begin(),
            session.document().objects.at(ui_object).stack.end(),[&](const auto& item){return item.id==offset_id;}),
            "Properties removes the selected instance atomically");
        auto* undo=text_action(window,"Undo");auto* redo=text_action(window,"Redo");check(undo&&redo,"Undo and Redo actions are available");
        undo->trigger();events();
        check(std::any_of(session.document().objects.at(ui_object).stack.begin(),session.document().objects.at(ui_object).stack.end(),
            [&](const auto& item){return item.id==offset_id&&!item.enabled;}),"Undo restores the exact bypassed Offset instance");
        redo->trigger();events();
        check(std::none_of(session.document().objects.at(ui_object).stack.begin(),session.document().objects.at(ui_object).stack.end(),
            [&](const auto& item){return item.id==offset_id;}),"Redo removes the same Offset instance");

        trigger(window,"add-rectangle");const auto group_a=window.canvas->selected_object;
        trigger(window,"add-rectangle");const auto group_b=window.canvas->selected_object;
        const auto composition=session.document().compositions.front().id;
        const Id group_id="effects-negative-group";
        session.apply({GroupContiguous{composition,"",{group_a,group_b},group_id,"Effects Group"}},session.revision());
        window.host.edited();window.canvas->set_selection(group_id);events();
        auto* group_target=named<QLabel>(window,"effects-target");
        check(group_target->text().contains("Group")&&group_target->text().contains(QString::fromStdString(group_id)),
            "Effects reports unsupported Group target identity and type");
        const auto group_document=encode(session.document());const auto group_revision=session.revision();const auto group_history=session.history();
        click(window,"effects-apply");
        check(encode(session.document())==group_document&&session.revision()==group_revision&&session.history()==group_history,
            "Group Offset refusal preserves Document, revision and history");
        status=named<QLabel>(window,"effects-status");
        check(status->text().contains("Group")&&status->text().contains(QString::fromStdString(group_id))&&status->text().contains("nect.shape.offset")&&
            status->text().contains("INVALID_DOMAIN"),"Group refusal displays target, operator and actual error");

        search->clear();events();
        const auto posterize_items=catalog->findItems("Group Posterize",Qt::MatchExactly);
        check(posterize_items.size()==1,"Group Posterize is a selectable built-in Effects entry");
        catalog->setCurrentItem(posterize_items.front());events();
        check(named<QPushButton>(window,"effects-apply")->text().contains("Group Posterize")&&
            named<QLabel>(window,"effects-status")->text().contains("Ready · nect.group.posterize v1"),
            "Group selection resolves the Posterize catalog entry against its exact target");

        window.canvas->set_selection(ui_object);events();
        const auto posterize_refusal_doc=encode(session.document());const auto posterize_refusal_rev=session.revision();
        const auto posterize_refusal_history=session.history();click(window,"effects-apply");
        check(encode(session.document())==posterize_refusal_doc&&session.revision()==posterize_refusal_rev&&
            session.history()==posterize_refusal_history&&named<QLabel>(window,"effects-status")->text().contains("nect.group.posterize")&&
            named<QLabel>(window,"effects-status")->text().contains("INVALID_DOMAIN"),
            "Group Posterize refuses a non-Group target visibly and without mutation");
        window.canvas->set_selection(group_id);events();
        const auto panel_before_revision=session.revision();
        click(window,"effects-apply");
        const auto& panel_stack=session.document().objects.at(group_id).stack;
        check(session.revision()==panel_before_revision+1&&panel_stack.size()==1&&panel_stack.front().type=="nect.group.posterize"&&
            panel_stack.front().version==1&&panel_stack.front().enabled&&panel_stack.front().parameters.at("levels").literal==2,
            "Effects Apply authors the default Group Posterize v1 in the selected Group through Session");
        const ShapeOperation panel_posterize_authored=panel_stack.front();
        const auto panel_posterize_id=panel_posterize_authored.id;
        auto* panel_card=named<QGroupBox>(window,"effects-operation-"+QString::fromStdString(panel_posterize_id));
        check(panel_card->title().contains("Group Posterize"),"Group Posterize instance has a stable Effects card");
        click(window,"effects-edit-properties-"+QString::fromStdString(panel_posterize_id));
        check(named<QGroupBox>(window,"stack-operation-"+QString::fromStdString(panel_posterize_id))->isVisible(),
            "Group Posterize card opens the normal Group Properties stack");
        const Ref panel_levels=operation_ref(group_id,panel_posterize_id,"levels");
        const auto posterize_definitions=api(request(session,R"({"op":"operator_types"})"));
        check(posterize_definitions.value("ok").toBool(),"Posterize definition uses the existing API operator registry");
        QJsonObject posterize_definition;
        for(const auto value:posterize_definitions.value("result").toArray())
            if(value.toObject().value("type").toString()=="nect.group.posterize")posterize_definition=value.toObject();
        check(!posterize_definition.isEmpty()&&posterize_definition.value("version").toInt()==1,
            "API exposes Group Posterize behavior v1");
        auto api_posterize=posterize_definition.value("template").toObject();api_posterize.insert("id","api-posterize");
        const auto api_group_index=static_cast<int>(session.document().objects.at(group_id).stack.size());
        const auto api_group_revision=session.revision();
        const QJsonObject posterize_command{{"type","add_operation"},{"object",QString::fromStdString(group_id)},
            {"index",api_group_index},{"operation",api_posterize}};
        const auto posterize_applied=api(request(session,QJsonDocument(QJsonObject{{"op","apply"},
            {"expected_revision",static_cast<qint64>(api_group_revision)},{"commands",QJsonArray{posterize_command}}})
            .toJson(QJsonDocument::Compact).toStdString()));
        check(posterize_applied.value("ok").toBool()&&session.revision()==api_group_revision+1,
            "API AddOperation authors Group Posterize on the same Session path");
        window.host.edited();events();
        const auto& api_posterize_operation=session.document().objects.at(group_id).stack.back();
        check(api_posterize_operation.id=="api-posterize"&&api_posterize_operation.type=="nect.group.posterize"&&
            api_posterize_operation.version==panel_posterize_authored.version&&api_posterize_operation.enabled==panel_posterize_authored.enabled&&
            api_posterize_operation.parameters==panel_posterize_authored.parameters,
            "Panel and API create equivalent Group Posterize authored semantics");
        const auto api_levels_ref=operation_ref(group_id,"api-posterize","levels");
        check(evaluate(session.document()).at(panel_levels)==evaluate(session.document()).at(api_levels_ref),
            "Panel-authored and API-authored levels evaluate identically");
        edit_property(window,panel_levels,"4");
        check(evaluate(session.document()).at(panel_levels)==4,
            "Group Posterize Levels uses the normal Properties command");
        const auto api_up_revision=session.revision();click(window,"operation-up-api-posterize");
        check(session.revision()==api_up_revision+1&&session.document().objects.at(group_id).stack.front().id=="api-posterize",
            "Group Posterize uses the ordinary stable-ID stack reorder control");
        auto* posterize_enabled=named<QCheckBox>(window,"operation-enabled-"+QString::fromStdString(panel_posterize_id));
        const auto posterize_bypass_revision=session.revision();posterize_enabled->setChecked(false);events();
        const auto after_bypass=std::find_if(session.document().objects.at(group_id).stack.begin(),
            session.document().objects.at(group_id).stack.end(),[&](const auto& op){return op.id==panel_posterize_id;});
        check(session.revision()==posterize_bypass_revision+1&&after_bypass!=session.document().objects.at(group_id).stack.end()&&
            !after_bypass->enabled,
            "Group Posterize bypass uses the ordinary authored stack control");
        const auto posterize_remove_revision=session.revision();click(window,"operation-remove-"+QString::fromStdString(panel_posterize_id));
        check(session.revision()==posterize_remove_revision+1&&std::none_of(session.document().objects.at(group_id).stack.begin(),
            session.document().objects.at(group_id).stack.end(),[&](const auto& op){return op.id==panel_posterize_id;}),
            "Group Posterize uses the ordinary stable-ID remove control");

        catalog->setCurrentRow(0);events();
        trigger(window,"add-curve");const auto open_path=window.canvas->selected_object;
        const auto open_document=encode(session.document());const auto open_revision=session.revision();const auto open_history=session.history();
        click(window,"effects-apply");
        check(encode(session.document())==open_document&&session.revision()==open_revision&&session.history()==open_history,
            "Open Path Offset refusal preserves Document, revision and history");
        status=named<QLabel>(window,"effects-status");
        check(status->text().contains(QString::fromStdString(open_path))&&status->text().contains("Path")&&
            status->text().contains("nect.shape.offset")&&status->text().contains("OFFSET_OPEN_PATH"),
            "Open Path refusal displays exact target, operator and actual geometry error");

        trigger(window,"add-rectangle");const auto preset_source=window.canvas->selected_object;
        auto source_offset=default_operation("ui-preset-offset","nect.shape.offset");source_offset.parameters.at("amount").literal=18;
        std::vector<Command> remove_source_entries;
        for(const auto& entry:session.document().objects.at(preset_source).stack) {
            check(!entry.macro,"Fresh Preset source does not contain an unrelated Macro instance");
            remove_source_entries.push_back(RemoveOperation{preset_source,entry.id});
        }
        if(!remove_source_entries.empty())session.apply(remove_source_entries,session.revision());
        session.apply({AddOperation{preset_source,source_offset,0}},session.revision());
        session.apply({MacroCommand{InstantiateMacro{preset_source,"effects-custom-macro","ui-preset-macro",1,1}}},session.revision());
        session.apply({MacroCommand{SetMacroOverride{preset_source,"ui-preset-macro","macro.offset.amount",27}}},session.revision());
        const auto source_stroke_id=Id("ui-preset-stroke");
        auto source_stroke=default_operation(source_stroke_id,"nect.paint.stroke");
        session.apply({AddOperation{preset_source,source_stroke,2}},session.revision());
        window.host.edited();events();
        auto* preset_tabs=window.findChild<QTabWidget*>("effects-tabs");check(preset_tabs,"Effects dock exposes a Presets tab");
        preset_tabs->setCurrentIndex(1);events();
        auto* preset_search=named<QLineEdit>(window,"presets-search");
        auto* preset_list=named<QListWidget>(window,"presets-catalog");
        const auto create_preset_revision=session.revision();
        answer_text_dialog_later(window,"UI Pair");click(window,"preset-save");
        check(session.revision()==create_preset_revision+1&&session.document().preset_definitions.size()==1,
            "Preset browser saves a named definition through one Session revision");
        const auto preset_id=session.document().preset_definitions.begin()->first;
        auto* preset_status=named<QLabel>(window,"presets-status");
        check(preset_status->text().contains(QString::fromStdString("ui-preset-offset"))&&
            preset_status->text().contains(QString::fromStdString("ui-preset-macro"))&&
            preset_status->text().contains(QString::fromStdString(source_stroke_id)),
            "Save status exposes the exact captured source operation IDs");
        const auto& saved_preset=session.document().preset_definitions.at(preset_id);
        check(saved_preset.schema_version==2&&saved_preset.entries.size()==3&&
            saved_preset.entries[0].type=="nect.shape.offset"&&saved_preset.entries[1].kind=="macro"&&
            saved_preset.entries[1].macro_definition=="effects-custom-macro"&&
            saved_preset.entries[1].overrides.at("macro.offset.amount")==27&&
            saved_preset.entries[2].type=="nect.paint.stroke",
            "Desktop Preset browser captures Offset, Macro, Stroke in processing order");
        preset_search->setText("ui pair");events();
        auto matching_presets=preset_list->findItems("UI Pair",Qt::MatchExactly);
        check(matching_presets.size()==1&&matching_presets.front()->data(Qt::UserRole).toString()==QString::fromStdString(preset_id),
            "Preset search resolves the stable definition ID by label");

          trigger(window,"add-rectangle");const auto preset_target=window.canvas->selected_object;
          const auto preset_target_base=session.document().objects.at(preset_target).stack.size();
        preset_list=named<QListWidget>(window,"presets-catalog");
        matching_presets=preset_list->findItems("UI Pair",Qt::MatchExactly);check(matching_presets.size()==1,"Preset remains searchable after target change");
        preset_list->setCurrentItem(matching_presets.front());events();
        const auto apply_preset_revision=session.revision();click(window,"preset-apply");
        const auto target_applied=session.document().objects.at(preset_target).stack;
          check(session.revision()==apply_preset_revision+1&&target_applied.size()==preset_target_base+3&&
              target_applied[preset_target_base].type=="nect.shape.offset"&&
              target_applied[preset_target_base+1].macro&&target_applied[preset_target_base+1].macro->definition=="effects-custom-macro"&&
              target_applied[preset_target_base+1].macro->pinned_revision==1&&
              target_applied[preset_target_base+1].macro->overrides.at("macro.offset.amount")==27&&
              target_applied[preset_target_base+2].type=="nect.paint.stroke"&&
              target_applied[preset_target_base].parameters.at("amount").literal==18,
            "Preset browser appends fresh Offset, pinned Macro and Stroke entries in order");
        const auto apply_undo_revision=session.revision();
        text_action(window,"Undo")->trigger();events();
          check(session.revision()==apply_undo_revision+1&&session.document().objects.at(preset_target).stack.size()==preset_target_base,
            "One UI Undo removes all applied preset entries together");
        text_action(window,"Redo")->trigger();events();
          check(session.document().objects.at(preset_target).stack.size()==preset_target_base+3,
            "One UI Redo restores the complete ordered Preset stack");

        window.canvas->set_selection(preset_source);events();
        const auto source_offset_ref=operation_ref(preset_source,"ui-preset-offset","amount");
        session.apply({Set{source_offset_ref,7}},session.revision());window.host.edited();events();
        const auto update_revision=session.revision();click(window,"preset-update");
        check(session.revision()==update_revision+1&&
            session.document().preset_definitions.at(preset_id).entries[0].parameters.at("amount")==7&&
              session.document().preset_definitions.at(preset_id).entries[1].overrides.at("macro.offset.amount")==27&&
              session.document().objects.at(preset_target).stack[preset_target_base].parameters.at("amount").literal==18&&
            named<QLabel>(window,"presets-status")->text().contains("ui-preset-offset"),
            "Update captures the full ordered source stack while existing applied snapshots stay unchanged");
        window.canvas->set_selection(preset_target);events();
        const auto reapply_revision=session.revision();click(window,"preset-apply");
        const auto& reedited_stack=session.document().objects.at(preset_target).stack;
          check(session.revision()==reapply_revision+1&&reedited_stack.size()==preset_target_base+6&&
              reedited_stack[preset_target_base].parameters.at("amount").literal==18&&
              reedited_stack[preset_target_base+3].parameters.at("amount").literal==7&&
              reedited_stack[preset_target_base+4].macro->overrides.at("macro.offset.amount")==27,
            "Later applications use edited built-in values and stable Macro overrides only for new entries");

        answer_text_dialog_later(window,"Renamed Pair");click(window,"preset-rename");
        check(session.document().preset_definitions.at(preset_id).label=="Renamed Pair",
            "Preset browser rename preserves its stable ID");
        preset_search=named<QLineEdit>(window,"presets-search");preset_search->setText("renamed");events();
        check(named<QListWidget>(window,"presets-catalog")->count()==1,"Preset browser searches the renamed definition");
        preset_search->setText("no matching preset");events();
        check(named<QListWidget>(window,"presets-catalog")->count()==0&&
            !named<QPushButton>(window,"preset-apply")->isEnabled(),
            "Empty Preset search disables Apply without changing the document");
        preset_search->clear();events();

        const auto before_delete=session.revision();click(window,"preset-delete");
        check(session.revision()==before_delete+1&&!session.document().preset_definitions.contains(preset_id),
            "Preset browser deletes one named definition through Session history");
        text_action(window,"Undo")->trigger();events();
        check(session.document().preset_definitions.contains(preset_id),"Undo restores the deleted PresetDefinition");

        const auto stale_before=session.revision();
        session.apply({Set{{preset_target,"","transform.anchor_x"},1}},stale_before);
        const auto stale_stack_size=session.document().objects.at(preset_target).stack.size();
        const auto stale_after=session.revision();click(window,"preset-apply");
        check(session.revision()==stale_after&&session.document().objects.at(preset_target).stack.size()==stale_stack_size&&
            named<QLabel>(window,"presets-status")->text().contains("REVISION_CONFLICT"),
            "Preset browser refuses an old captured revision without applying operations");
        window.host.edited();events();

        trigger(window,"add-rectangle");const auto driven_target=window.canvas->selected_object;
        auto driven_offset=default_operation("ui-driven-offset","nect.shape.offset");
        auto driven_repeater=default_operation("ui-driven-repeater","nect.shape.repeater");
        const auto driven_index=session.document().objects.at(driven_target).stack.size();
        session.apply({AddOperation{driven_target,driven_offset,driven_index},
            AddOperation{driven_target,driven_repeater,driven_index+1}},session.revision());
        session.apply({LinkOperationEnabled{operation_ref(driven_target,"ui-driven-offset","enabled"),
            operation_ref(driven_target,session.document().objects.at(driven_target).stack.front().id,"enabled"),false}},session.revision());
        window.host.edited();events();
        const auto driven_definitions=session.document().preset_definitions.size();
        answer_text_dialog_later(window,"Refused Driven Stack");click(window,"preset-save");
        const auto driven_status=named<QLabel>(window,"presets-status")->text();
        check(session.document().preset_definitions.size()==driven_definitions&&
            driven_status.contains("PRESET_NONPORTABLE_SOURCE")&&driven_status.contains(QString::fromStdString(driven_target))&&
            driven_status.contains("op.ui-driven-offset.enabled"),
            "Preset browser visibly refuses a driven captured field and displays its exact source Ref");

        const auto previous_session=window.host.session_id;
        answer_text_dialog_later(window,"Stale Session Preset",true);click(window,"preset-save");
        check(window.host.session_id!=previous_session&&session.document().preset_definitions.empty()&&
            named<QLabel>(window,"presets-status")->text().contains("SESSION_CONFLICT"),
            "Preset browser refuses a dialog opened for a replaced document Session");

        std::cout<<"PASS Effects panel search, selection guard, Apply, Inspector navigation, edits, native/API readback, history and refusals\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
