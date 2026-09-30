#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection: "+std::string(code));}
void operation_enabled_read_contract() {
    Session session(empty_document("enabled-doc","enabled-comp","enabled-art"));
    session.apply({CreatePrimitive{"enabled-comp","","enabled-path","Enabled Path",
        default_primitive("enabled-shape","nect.shape.rectangle")}},0);
    session.apply({AddOperation{"enabled-path",default_operation("enabled-fill","nect.paint.fill"),1}},session.revision());
    const Ref ref=operation_ref("enabled-path","enabled-fill","enabled");
    const auto refs=properties(session.document());
    check(std::find(refs.begin(),refs.end(),ref)!=refs.end()&&
        resolve_name(session.document(),"Enabled Path","",ref.field)==ref&&
        operation_enabled_property(session.document(),ref),
        "Operation enabled is discoverable by its stable typed Ref");
    rejects("INVALID_OPERATION_REF",[&]{(void)operation_enabled_property(session.document(),
        {"enabled-path","point",ref.field});});
    rejects("MISSING_OPERATION",[&]{(void)operation_enabled_property(session.document(),
        operation_ref("enabled-path","missing","enabled"));});
    rejects("MISSING_REFERENCE",[&]{session.apply({Set{ref,0}},session.revision());});
    session.apply({EnableOperation{"enabled-path","enabled-fill",false}},session.revision());
    const auto shape=evaluate_shape(session.document(),"enabled-path",evaluate(session.document()));
    check(!operation_enabled_property(session.document(),ref)&&
        std::none_of(shape.paints.begin(),shape.paints.end(),[](const auto& paint){return paint.operation=="enabled-fill";}),
        "Existing enable command changes the typed value and bypasses the Fill consumer");
    session.undo(session.revision());
    check(operation_enabled_property(session.document(),ref),"Undo restores the authored operation enabled choice");
}
void operation_enabled_link_contract() {
    Session session(empty_document("enabled-link-doc","enabled-link-comp","enabled-link-art"));
    auto atomic_reject=[&](const char* code,std::vector<Command> commands) {
        const auto before=encode(session.document());const auto rev=session.revision();
        rejects(code,[&]{session.apply(std::move(commands),rev);});
        check(session.revision()==rev&&encode(session.document())==before,
            "Rejected operation-enabled command preserves authored bytes and revision");
    };
    session.apply({CreatePrimitive{"enabled-link-comp","","enabled-source","Source",
            default_primitive("enabled-source-shape","nect.shape.rectangle")},
        CreatePrimitive{"enabled-link-comp","","enabled-target","Target",
            default_primitive("enabled-target-shape","nect.shape.rectangle")},
        CreatePrimitive{"enabled-link-comp","","enabled-alternate","Alternate",
            default_primitive("enabled-alternate-shape","nect.shape.rectangle")}},0);
    auto source_op=default_operation("enabled-source-fill","nect.paint.fill");source_op.enabled=false;
    auto target_op=default_operation("enabled-target-fill","nect.paint.fill");target_op.enabled=false;
    auto alternate_op=default_operation("enabled-alternate-fill","nect.paint.fill");alternate_op.enabled=false;
    session.apply({AddOperation{"enabled-source",source_op,1},AddOperation{"enabled-target",target_op,1},
        AddOperation{"enabled-alternate",alternate_op,1}},session.revision());
    const Ref target=operation_ref("enabled-target","enabled-target-fill","enabled");
    const Ref source=operation_ref("enabled-source","enabled-source-fill","enabled");
    const Ref alternate=operation_ref("enabled-alternate","enabled-alternate-fill","enabled");
    const auto literal_state=operation_enabled_state(session.document(),target);
    check(property_unit(target)=="boolean"&&
        resolve_name(session.document(),"Target","",target.field)==target&&
        !literal_state.literal&&!literal_state.driver&&!literal_state.evaluated,
        "Operation enabled is a typed boolean property with the authored literal before linking");
    auto literal_native=encode(session.document());const auto literal_version=literal_native.find("\"version\":\"0.67\"");
    check(literal_version!=std::string::npos&&decode(literal_native)==session.document(),
        "Native 0.43 roundtrips a literal operation enabled property");
    auto legacy_literal=test_support::without_empty_presets_for_legacy_fixture(literal_native);
    legacy_literal.replace(literal_version,std::string("\"version\":\"0.67\"").size(),"\"version\":\"0.27\"");
    check(decode(legacy_literal)==session.document(),"Native 0.27 literal-only documents remain readable");

    session.apply({LinkOperationEnabled{target,source,false}},session.revision());
    auto state=operation_enabled_state(session.document(),target);
    const auto linked_shape=evaluate_shape(session.document(),"enabled-target",evaluate(session.document()));
    check(!state.literal&&state.driver==source&&!state.evaluated&&
        std::none_of(linked_shape.paints.begin(),linked_shape.paints.end(),
            [](const auto& paint){return paint.operation=="enabled-target-fill";}),
        "A same-composition operation link preserves the literal and bypasses the linked Fill");
    session.apply({EnableOperation{"enabled-source","enabled-source-fill",true}},session.revision());
    const auto active_shape=evaluate_shape(session.document(),"enabled-target",evaluate(session.document()));
    check(!operation_enabled_state(session.document(),target).literal&&
        operation_enabled_state(session.document(),target).evaluated&&
        std::any_of(active_shape.paints.begin(),active_shape.paints.end(),
            [](const auto& paint){return paint.operation=="enabled-target-fill";}),
        "An enabled source activates a linked Fill while its target authored literal remains false");
    session.apply({EnableOperation{"enabled-source","enabled-source-fill",false}},session.revision());
    Session duplicate(session.document());duplicate.apply({DuplicateObjects{{"enabled-source","enabled-target"},"enabled-copy"}},0);
    const auto copied_source=std::find_if(duplicate.document().objects.begin(),duplicate.document().objects.end(),
        [](const auto& entry){return entry.second.name=="Source copy";});
    const auto copied_target=std::find_if(duplicate.document().objects.begin(),duplicate.document().objects.end(),
        [](const auto& entry){return entry.second.name=="Target copy";});
    check(copied_source!=duplicate.document().objects.end()&&copied_target!=duplicate.document().objects.end(),
        "Duplicating both operation-enabled link endpoints creates stable object copies");
    const auto copied_operation=std::find_if(copied_target->second.stack.begin(),copied_target->second.stack.end(),
        [](const auto& operation){return operation.type=="nect.paint.fill";});
    const auto copied_source_operation=std::find_if(copied_source->second.stack.begin(),copied_source->second.stack.end(),
        [](const auto& operation){return operation.type=="nect.paint.fill";});
    check(copied_operation!=copied_target->second.stack.end()&&copied_operation->enabled_driver&&
        copied_source_operation!=copied_source->second.stack.end()&&
        copied_operation->enabled_driver==operation_ref(copied_source->first,copied_source_operation->id,"enabled")&&
        !evaluate_operation_enabled(duplicate.document(),operation_ref(copied_target->first,copied_operation->id,"enabled")),
        "Duplicating both endpoints remaps operation IDs and evaluates against the copied source");
    atomic_reject("DRIVEN_PROPERTY",{EnableOperation{"enabled-target","enabled-target-fill",false}});
    atomic_reject("DRIVEN_PROPERTY",{LinkOperationEnabled{target,alternate,false}});
    atomic_reject("DUPLICATE_TARGET",{LinkOperationEnabled{target,alternate,true},UnlinkOperationEnabled{target}});
    atomic_reject("DEPENDENCY_CYCLE",{LinkOperationEnabled{source,target,false}});
    atomic_reject("DEPENDENCY_CYCLE",{LinkOperationEnabled{target,target,true}});
    atomic_reject("MISSING_OPERATION",{LinkOperationEnabled{target,
        operation_ref("enabled-source","absent-operation","enabled"),true}});
    atomic_reject("TYPE_MISMATCH",{LinkOperationEnabled{target,
        {"enabled-source","","op.enabled-source-fill.gradient.absent-gradient.enabled"},true}});
    atomic_reject("INVALID_OPERATION_REF",{LinkOperationEnabled{target,{"enabled-source","point",source.field},false}});
    atomic_reject("TYPE_MISMATCH",{LinkOperationEnabled{target,operation_ref("enabled-source","enabled-source-fill","r"),false}});

    const auto revision=session.revision();

    session.apply({LinkOperationEnabled{target,alternate,true}},session.revision());
    check(operation_enabled_state(session.document(),target).driver==alternate&&
        !operation_enabled_state(session.document(),target).evaluated,
        "Explicit replacement switches the target to another stable boolean source");
    const auto linked=encode(session.document());
    check(linked.find("\"version\":\"0.67\"")!=std::string::npos&&
        linked.find("\"enabled_driver\":{\"link\":")!=std::string::npos&&encode(decode(linked))==linked,
        "Native 0.43 preserves the closed enabled driver and exact Ref");
    auto old_driver=test_support::without_empty_presets_for_legacy_fixture(linked);const auto old_version=old_driver.find("\"version\":\"0.67\"");
    old_driver.replace(old_version,std::string("\"version\":\"0.67\"").size(),"\"version\":\"0.27\"");
    rejects("UNSUPPORTED_OPERATION_ENABLED_DRIVER",[&]{(void)decode(old_driver);});

    session.apply({EnableOperation{"enabled-alternate","enabled-alternate-fill",true}},session.revision());
    check(operation_enabled_state(session.document(),target).evaluated,
        "An enabled source updates the target evaluated value without changing its literal");
    auto smuggled=default_operation("smuggled","nect.paint.fill");smuggled.enabled_driver=source;
    atomic_reject("USE_TYPED_COMMAND",{AddOperation{"enabled-target",smuggled,2}});
    const auto unlink_revision=session.revision();
    session.apply({UnlinkOperationEnabled{target}},unlink_revision);
    check(!operation_enabled_state(session.document(),target).driver&&
        operation_enabled_state(session.document(),target).literal&&
        operation_enabled_state(session.document(),target).evaluated,
        "Unlink freezes the currently evaluated enabled value into the authored literal");
    session.apply({EnableOperation{"enabled-alternate","enabled-alternate-fill",false}},session.revision());
    check(operation_enabled_state(session.document(),target).evaluated,
        "Frozen operation enabled literal no longer follows its former source");
    session.undo(session.revision());
    check(!operation_enabled_state(session.document(),target).driver&&
        operation_enabled_state(session.document(),target).evaluated,
        "The first Undo restores only the source toggle after unlink");
    session.undo(session.revision());
    check(operation_enabled_state(session.document(),target).driver==alternate&&
        operation_enabled_state(session.document(),target).evaluated,
        "Undo restores the stable enabled driver and computed value");

    atomic_reject("MISSING_OPERATION",{RemoveOperation{"enabled-alternate","enabled-alternate-fill"}});
    session.apply({UnlinkOperationEnabled{target},RemoveOperation{"enabled-alternate","enabled-alternate-fill"}},session.revision());
    check(session.revision()==revision+7,"Unlink permits removing the former source in the same atomic batch");

    auto other_composition=empty_document("other-enabled-doc","other-enabled-comp","other-enabled-art").compositions.front();
    auto cross_document=empty_document("cross-enabled-doc","cross-enabled-comp","cross-enabled-art");
    cross_document.compositions.push_back(other_composition);
    Session cross(cross_document);
    cross.apply({CreatePrimitive{"cross-enabled-comp","","cross-target","Target",
            default_primitive("cross-target-shape","nect.shape.rectangle")},
        CreatePrimitive{"other-enabled-comp","","cross-source","Source",
            default_primitive("cross-source-shape","nect.shape.rectangle")}},0);
    cross.apply({AddOperation{"cross-target",default_operation("cross-target-fill","nect.paint.fill"),1},
        AddOperation{"cross-source",default_operation("cross-source-fill","nect.paint.fill"),1}},cross.revision());
    rejects("CROSS_COMPOSITION",[&]{cross.apply({LinkOperationEnabled{
        operation_ref("cross-target","cross-target-fill","enabled"),
        operation_ref("cross-source","cross-source-fill","enabled"),false}},cross.revision());});

    auto deep=empty_document("enabled-depth-doc","enabled-depth-comp","enabled-depth-art");
    std::vector<Command> deep_links;
    for(int i=0;i<130;++i) {
        const auto id="enabled-depth-"+std::to_string(i);Object object;object.id=id;object.name=id;
        object.source=default_primitive(id+"-shape","nect.shape.rectangle");
        object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
        deep.objects.emplace(id,std::move(object));deep.compositions.front().roots.push_back(id);
        if(i<129)deep_links.push_back(LinkOperationEnabled{
            operation_ref(id,id+"-fill","enabled"),
            operation_ref("enabled-depth-"+std::to_string(i+1),"enabled-depth-"+std::to_string(i+1)+"-fill","enabled"),false});
    }
    Session depth(std::move(deep));
    rejects("DEPENDENCY_DEPTH",[&]{depth.apply(std::move(deep_links),depth.revision());});

}
void operation_enabled_expression_contract() {
    auto document=empty_document("enabled-expression-doc","enabled-expression-comp","enabled-expression-art");
    auto foreign=empty_document("enabled-expression-foreign-doc","enabled-expression-foreign-comp","enabled-expression-foreign-art");
    document.compositions.push_back(foreign.compositions.front());
    Session session(std::move(document));
    session.apply({CreatePrimitive{"enabled-expression-comp","","expr-source","Expression Source",
            default_primitive("expr-source-shape","nect.shape.rectangle")},
        CreatePrimitive{"enabled-expression-comp","","expr-target","Expression Target",
            default_primitive("expr-target-shape","nect.shape.rectangle")},
        CreatePrimitive{"enabled-expression-comp","","expr-alternate","Expression Alternate",
            default_primitive("expr-alternate-shape","nect.shape.rectangle")},
        CreatePrimitive{"enabled-expression-foreign-comp","","expr-foreign","Foreign Source",
            default_primitive("expr-foreign-shape","nect.shape.rectangle")}},0);
    auto source_operation=default_operation("expr-source-fill","nect.paint.fill");source_operation.enabled=false;
    auto target_operation=default_operation("expr-target-fill","nect.paint.fill");target_operation.enabled=false;
    auto alternate_operation=default_operation("expr-alternate-fill","nect.paint.fill");alternate_operation.enabled=false;
    auto foreign_operation=default_operation("expr-foreign-fill","nect.paint.fill");foreign_operation.enabled=false;
    session.apply({AddOperation{"expr-source",source_operation,1},AddOperation{"expr-target",target_operation,1},
        AddOperation{"expr-alternate",alternate_operation,1},AddOperation{"expr-foreign",foreign_operation,1}},session.revision());
    const Ref target=operation_ref("expr-target","expr-target-fill","enabled");
    const Ref source=operation_ref("expr-source","expr-source-fill","enabled");
    const Ref alternate=operation_ref("expr-alternate","expr-alternate-fill","enabled");
    const Expression expression{" ! ref ( \"expr-source\" , \"\" , \"op.expr-source-fill.enabled\" ) ",1};
    const auto before_expression=session.revision();
    session.apply({SetOperationEnabledExpression{target,expression}},before_expression);
    auto state=operation_enabled_state(session.document(),target);
    auto shape=evaluate_shape(session.document(),"expr-target",evaluate(session.document()));
    check(!state.literal&&!state.driver&&state.expression==expression&&state.evaluated&&
        std::any_of(shape.paints.begin(),shape.paints.end(),[](const auto& paint){return paint.operation=="expr-target-fill";}),
        "A negated same-Composition operation Ref activates a false-literal target through shape evaluation");
    const auto exact_revision=session.revision();
    session.apply({SetOperationEnabledExpression{target,expression}},exact_revision);
    check(session.revision()==exact_revision,"Reapplying the exact operation enabled expression is idempotent");
    session.apply({EnableOperation{"expr-source","expr-source-fill",true}},session.revision());
    state=operation_enabled_state(session.document(),target);
    shape=evaluate_shape(session.document(),"expr-target",evaluate(session.document()));
    check(!state.literal&&state.expression==expression&&!state.evaluated&&
        std::none_of(shape.paints.begin(),shape.paints.end(),[](const auto& paint){return paint.operation=="expr-target-fill";}),
        "Source edits update evaluated bypass while preserving the target literal and exact expression");
    session.undo(session.revision());
    check(operation_enabled_state(session.document(),target).evaluated,
        "Undo restores the source value and the expression result");
    session.redo(session.revision());
    check(!operation_enabled_state(session.document(),target).evaluated,
        "Redo restores the source edit through the same expression evaluator");

    auto atomic_reject=[&](const char* code,std::vector<Command> commands) {
        const auto before=encode(session.document());const auto revision=session.revision();
        rejects(code,[&]{session.apply(std::move(commands),revision);});
        check(session.revision()==revision&&encode(session.document())==before,
            "Rejected Operation enabled expression command preserves native bytes and revision");
    };
    auto smuggled=default_operation("expression-smuggle","nect.paint.fill");
    smuggled.enabled_expression=Expression{"true",1};
    atomic_reject("USE_TYPED_COMMAND",{AddOperation{"expr-target",smuggled,1}});
    atomic_reject("DRIVEN_PROPERTY",{EnableOperation{"expr-target","expr-target-fill",true}});
    atomic_reject("DRIVEN_PROPERTY",{SetOperationEnabledExpression{target,{"false",1},false}});
    atomic_reject("DRIVEN_PROPERTY",{LinkOperationEnabled{target,alternate,false}});
    atomic_reject("DUPLICATE_TARGET",{SetOperationEnabledExpression{target,expression,true},UnlinkOperationEnabled{target}});
    atomic_reject("DRIVEN_PROPERTY",{SetOperationEnabledExpression{alternate,{"true",1}},
        EnableOperation{"expr-target","expr-target-fill",true}});
    atomic_reject("DEPENDENCY_CYCLE",{LinkOperationEnabled{source,target,false}});
    atomic_reject("DEPENDENCY_CYCLE",{SetOperationEnabledExpression{target,{"ref(\"expr-target\",\"\",\"op.expr-target-fill.enabled\")",1},true}});
    atomic_reject("MISSING_OPERATION",{SetOperationEnabledExpression{target,{"ref(\"expr-source\",\"\",\"op.missing.enabled\")",1},true}});
    atomic_reject("BOOLEAN_EXPRESSION_TYPE",{SetOperationEnabledExpression{target,
        {"ref(\"expr-source\",\"\",\"op.expr-source-fill.gradient.some-gradient.enabled\")",1},true}});
    atomic_reject("BOOLEAN_EXPRESSION_TYPE",{SetOperationEnabledExpression{target,
        {"ref(\"expr-source\",\"point\",\"op.expr-source-fill.enabled\")",1},true}});
    atomic_reject("BOOLEAN_EXPRESSION_SYNTAX",{SetOperationEnabledExpression{target,{"true && false",1},true}});
    atomic_reject("BOOLEAN_EXPRESSION_SYNTAX",{SetOperationEnabledExpression{target,{"!!ref(\"expr-source\",\"\",\"op.expr-source-fill.enabled\")",1},true}});
    atomic_reject("CROSS_COMPOSITION",{SetOperationEnabledExpression{target,
        {"ref(\"expr-foreign\",\"\",\"op.expr-foreign-fill.enabled\")",1},true}});
    atomic_reject("MISSING_OPERATION",{RemoveOperation{"expr-source","expr-source-fill"}});
    rejects("REVISION_CONFLICT",[&]{session.apply({SetOperationEnabledExpression{target,expression,true}},session.revision()+1);});
    rejects("MISSING_REFERENCE",[&]{session.apply({Set{target,1}},session.revision());});

    session.apply({LinkOperationEnabled{target,alternate,true}},session.revision());
    state=operation_enabled_state(session.document(),target);
    check(state.driver==alternate&&!state.expression,"Replacing an expression with a link clears the old source");
    session.apply({SetOperationEnabledExpression{target,expression,true}},session.revision());
    const auto expression_native=encode(session.document());
    check(expression_native.find("\"version\":\"0.67\"")!=std::string::npos&&
        expression_native.find("\"enabled_expression\":{\"source\":\" ! ref ( \\\"expr-source\\\" , \\\"\\\" , \\\"op.expr-source-fill.enabled\\\" ) \",\"version\":1}")!=std::string::npos&&
        encode(decode(expression_native))==expression_native,
        "Native 0.67 roundtrips the closed operation expression beside its authored literal");
    auto old_version=expression_native;
    const auto writer=old_version.find("\"version\":\"0.67\"");
    old_version.replace(writer,std::string("\"version\":\"0.67\"").size(),"\"version\":\"0.66\"");
    rejects("UNSUPPORTED_OPERATION_ENABLED_EXPRESSION",[&]{(void)decode(old_version);});
    auto conflicting=expression_native;
    const auto expression_field=conflicting.find("\"enabled_expression\":");
    check(expression_field!=std::string::npos,"Native expression fixture exposes its source wrapper");
    conflicting.insert(expression_field,
        "\"enabled_driver\":{\"link\":{\"object\":\"expr-source\",\"point\":\"\",\"field\":\"op.expr-source-fill.enabled\"}},");
    rejects("INVALID_OPERATION_ENABLED_SOURCE",[&]{(void)decode(conflicting);});

    Session duplicate(session.document());duplicate.apply({DuplicateObjects{{"expr-source","expr-target"},"enabled-expression-copy"}},0);
    const auto copied_source=std::find_if(duplicate.document().objects.begin(),duplicate.document().objects.end(),
        [](const auto& entry){return entry.second.name=="Expression Source copy";});
    const auto copied_target=std::find_if(duplicate.document().objects.begin(),duplicate.document().objects.end(),
        [](const auto& entry){return entry.second.name=="Expression Target copy";});
    check(copied_source!=duplicate.document().objects.end()&&copied_target!=duplicate.document().objects.end(),
        "Duplicate selection contains both operation expression endpoints");
    const auto copied_source_op=std::find_if(copied_source->second.stack.begin(),copied_source->second.stack.end(),
        [](const auto& operation){return operation.type=="nect.paint.fill";});
    const auto copied_target_op=std::find_if(copied_target->second.stack.begin(),copied_target->second.stack.end(),
        [](const auto& operation){return operation.type=="nect.paint.fill";});
    auto expected=expression.source;
    const auto object_at=expected.find("\"expr-source\"");expected.replace(object_at+1,std::string("expr-source").size(),copied_source->first);
    const auto field_at=expected.find("op.expr-source-fill.enabled");
    expected.replace(field_at,std::string("op.expr-source-fill.enabled").size(),
        "op."+copied_source_op->id+".enabled");
    check(copied_target_op!=copied_target->second.stack.end()&&copied_target_op->enabled_expression&&
        copied_target_op->enabled_expression->source==expected&&
        !evaluate_operation_enabled(duplicate.document(),operation_ref(copied_target->first,copied_target_op->id,"enabled")),
        "Duplicate remaps only copied operation Ref IDs while retaining expression formatting and evaluation");

    const auto unlink_revision=session.revision();
    session.apply({UnlinkOperationEnabled{target}},unlink_revision);
    check(!operation_enabled_state(session.document(),target).driver&&!operation_enabled_state(session.document(),target).expression&&
        !operation_enabled_state(session.document(),target).literal&&!operation_enabled_state(session.document(),target).evaluated,
        "Unlink freezes the current expression value into the authored operation literal");
    session.apply({EnableOperation{"expr-source","expr-source-fill",false}},session.revision());
    check(!operation_enabled_state(session.document(),target).evaluated,
        "Frozen operation enabled value no longer follows the former expression source");
    session.undo(session.revision());session.undo(session.revision());
    check(operation_enabled_state(session.document(),target).expression==expression&&
        !operation_enabled_state(session.document(),target).evaluated,
        "Undo restores the exact expression and the preceding source state");
    session.redo(session.revision());session.redo(session.revision());
    check(!operation_enabled_state(session.document(),target).expression&&!operation_enabled_state(session.document(),target).evaluated,
        "Redo reapplies source-independent expression unlink");
    const Expression literal_expression{" true ",1};
    session.apply({SetOperationEnabledExpression{alternate,literal_expression}},session.revision());
    check(!operation_enabled_state(session.document(),alternate).literal&&
        operation_enabled_state(session.document(),alternate).expression==literal_expression&&
        operation_enabled_state(session.document(),alternate).evaluated,
        "Constant expression sources preserve the literal and accept only the bounded boolean grammar");
    session.apply({UnlinkOperationEnabled{alternate}},session.revision());
    check(operation_enabled_state(session.document(),alternate).literal&&
        !operation_enabled_state(session.document(),alternate).expression,
        "Unlinking a constant expression freezes its evaluated boolean into the authored literal");
}
void fill_rule_link_contract() {
    Session session(empty_document("fill-doc","fill-comp","fill-art"));
    session.apply({CreatePrimitive{"fill-comp","","fill-source","Source",default_primitive("source-shape","nect.shape.rectangle")},
        CreatePrimitive{"fill-comp","","fill-target","Target",default_primitive("target-shape","nect.shape.rectangle")}},0);
    auto source_fill=default_operation("source-fill","nect.paint.fill");
    auto target_fill=default_operation("target-fill","nect.paint.fill");target_fill.fill_rule="nonzero";
    session.apply({AddOperation{"fill-source",source_fill,1},AddOperation{"fill-target",target_fill,1},
        OperationOptions{"fill-source","source-fill","below","evenodd"}},session.revision());
    const Ref target{"fill-target","","op.target-fill.fill_rule"},source{"fill-source","","op.source-fill.fill_rule"};
    const auto target_shape_rule=[&] {
        const auto shape=evaluate_shape(session.document(),"fill-target",evaluate(session.document()));
        const auto paint=std::find_if(shape.paints.begin(),shape.paints.end(),[](const auto& layer){return layer.operation=="target-fill";});
        if(paint==shape.paints.end())throw std::runtime_error("Target Fill layer is missing from shape evaluation");
        return paint->fill_rule;
    };
    check(resolve_name(session.document(),"Target","",target.field)==target&&
        fill_rule_property(session.document(),target).literal=="nonzero"&&
        target_shape_rule()=="nonzero",
        "Fill rule has a stable typed Ref and literal paint evaluation");
    rejects("MISSING_REFERENCE",[&]{session.apply({Set{target,1}},session.revision());});
    rejects("INVALID_DOMAIN",[&]{session.apply({LinkFillRule{target,operation_ref("fill-source","fill-source-stroke","fill_rule")}},session.revision());});
    rejects("INVALID_OPERATION_REF",[&]{session.apply({LinkFillRule{target,{"fill-source","point","op.source-fill.fill_rule"}}},session.revision());});
    rejects("TYPE_MISMATCH",[&]{session.apply({LinkFillRule{target,{"fill-source","","op.source-fill.composite"}}},session.revision());});
    const auto literal_bytes=encode(session.document());
    check(literal_bytes.find("\"version\":\"0.67\"")!=std::string::npos&&
        literal_bytes.find("fill_rule_driver")==std::string::npos&&encode(decode(literal_bytes))==literal_bytes,
        "Native 0.43 omits an absent Fill rule driver and roundtrips literal state");
    auto legacy_literal=test_support::without_empty_presets_for_legacy_fixture(literal_bytes);
    const auto current_version=legacy_literal.find("\"version\":\"0.67\"");
    check(current_version!=std::string::npos,"Native writer exposes 0.36 for the literal migration fixture");
    legacy_literal.replace(current_version,std::string("\"version\":\"0.67\"").size(),"\"version\":\"0.27\"");
    check(decode(legacy_literal)==session.document(),"Native 0.27 retains literal-only Fill rules");

    session.apply({LinkFillRule{target,source}},session.revision());
    auto state=fill_rule_property(session.document(),target);
    check(state.literal=="nonzero","Linked Fill retains its authored literal");
    check(state.driver==FillRuleDriver{source},"Linked Fill stores the exact same-field source Ref");
    check(state.evaluated=="evenodd","Linked Fill evaluator follows the even-odd source");
    check(target_shape_rule()=="evenodd",
        "Linked Fill shape evaluation uses the resolved enum");
    auto linked=encode(session.document());check(linked.find("\"fill_rule_driver\":{\"link\":")!=std::string::npos&&
        encode(decode(linked))==linked,"Native 0.43 retains the closed stable Fill rule Ref exactly");
    auto false_version=test_support::without_empty_presets_for_legacy_fixture(linked);const auto version= false_version.find("\"version\":\"0.67\"");
    false_version.replace(version,std::string("\"version\":\"0.67\"").size(),"\"version\":\"0.25\"");
    rejects("UNSUPPORTED_FILL_RULE_DRIVER",[&]{decode(false_version);});
    auto malformed=linked;const auto field=malformed.find("op.source-fill.fill_rule");
    check(field!=std::string::npos,"Native linked source Ref is present");
    malformed.replace(field,std::string("op.source-fill.fill_rule").size(),"text.direction");
    rejects("TYPE_MISMATCH",[&]{decode(malformed);});

    const auto before_bad_batch=encode(session.document());const auto revision=session.revision();
    rejects("DEPENDENCY_CYCLE",[&]{session.apply({OperationOptions{"fill-target","target-fill","above","nonzero"},
        LinkFillRule{target,target}},revision);});
    check(session.revision()==revision&&encode(session.document())==before_bad_batch,
        "An invalid second command leaves the first composite edit and all authored bytes uncommitted");
    rejects("DUPLICATE_TARGET",[&]{session.apply({LinkFillRule{target,source,false},UnlinkFillRule{target}},revision);});
    rejects("DRIVEN_PROPERTY",[&]{session.apply({LinkFillRule{target,source}},revision);});
    rejects("DRIVEN_PROPERTY",[&]{session.apply({OperationOptions{"fill-target","target-fill","above","evenodd"}},revision);});
    session.apply({OperationOptions{"fill-target","target-fill","above","nonzero"}},revision);
    check(session.document().objects.at("fill-target").stack.back().fill_rule_driver==FillRuleDriver{source},
        "An unrelated composite edit preserves the Fill rule driver");
    session.apply({OperationOptions{"fill-source","source-fill","below","nonzero"}},session.revision());
    check(fill_rule_property(session.document(),target).evaluated=="nonzero"&&
        target_shape_rule()=="nonzero",
        "Changing the source choice updates target paint evaluation");
    rejects("MISSING_OPERATION",[&]{session.apply({RemoveOperation{"fill-source","source-fill"}},session.revision());});
    const auto unlink_revision=session.revision();session.apply({UnlinkFillRule{target}},unlink_revision);
    check(fill_rule_property(session.document(),target).literal=="nonzero"&&
        !fill_rule_property(session.document(),target).driver,"Unlink freezes the evaluated choice into the target literal");
    session.undo(session.revision());
    check(fill_rule_property(session.document(),target).driver==FillRuleDriver{source},"Undo restores the Fill rule link");
    session.apply({OperationOptions{"fill-source","source-fill","below","evenodd"}},session.revision());
    check(fill_rule_property(session.document(),target).evaluated=="evenodd","Undo-restored link follows later source edits");

    Session duplicate(session.document());
    duplicate.apply({DuplicateObjects{{"fill-source","fill-target"},"fill-copy"}},0);
    const auto copied_source=std::find_if(duplicate.document().objects.begin(),duplicate.document().objects.end(),
        [](const auto& entry){return entry.second.name=="Source copy";});
    const auto copied_target=std::find_if(duplicate.document().objects.begin(),duplicate.document().objects.end(),
        [](const auto& entry){return entry.second.name=="Target copy";});
    check(copied_source!=duplicate.document().objects.end()&&copied_target!=duplicate.document().objects.end(),
        "Duplicating both Fill link endpoints creates stable copies");
    const auto copied_fill=std::find_if(copied_target->second.stack.begin(),copied_target->second.stack.end(),
        [](const auto& operation){return operation.type=="nect.paint.fill";});
    check(copied_fill!=copied_target->second.stack.end()&&copied_fill->fill_rule_driver&&
        copied_fill->fill_rule_driver->link==operation_ref(copied_source->first,
            std::find_if(copied_source->second.stack.begin(),copied_source->second.stack.end(),
                [](const auto& operation){return operation.type=="nect.paint.fill";})->id,"fill_rule")&&
        evaluate_fill_rule(duplicate.document(),operation_ref(copied_target->first,copied_fill->id,"fill_rule"))=="evenodd",
        "Duplicating source and target remaps only the copied Fill link to the copied source");
}
}
int main(){try{
    operation_enabled_read_contract();
    operation_enabled_link_contract();
    Session s(empty_document("doc","comp","art"));
    s.apply({CreatePrimitive{"comp","","rect","Motif",{"source","nect.shape.rectangle",1,
        {{"center_x",{10,{}}},{"center_y",{10,{}}},{"width",{20,{}}},{"height",{10,{}}}}}}},0);
    const auto stroke=s.document().objects.at("rect").legacy_stroke;
    auto fill=default_operation("fill","nect.paint.fill");fill.parameters["r"].literal=1;fill.parameters["a"].literal=.5;
    auto repeat=default_operation("repeat","nect.shape.repeater");
    s.apply({AddOperation{"rect",fill,0},AddOperation{"rect",repeat,2}},1);
    auto evaluate_current=[&]{return evaluate_shape(s.document(),"rect",evaluate(s.document()));};
    auto shape=evaluate_current();
    check(shape.paths.size()==3&&shape.paints.size()==6,"Repeat after paint produces three independently painted copies");
    check(shape.paints[0].operation==stroke&&shape.paints[1].operation=="fill","Earlier paint entries composite above later by default");
    check(shape.paints.front().transform[4]==200&&shape.paints.back().transform[4]==0,"Repeater below places original on top");
    const auto top_left=map_point(shape.paths[2].transform,shape.paths[2].contours->front().points.front().anchor);
    check(top_left.x==200&&top_left.y==5,"Third virtual copy has hand-specified translated coordinates");
    s.apply({ReorderOperations{"rect",{"repeat","fill",stroke}}},2);
    shape=evaluate_current();
    check(shape.paints.size()==2&&shape.paints[0].paths.size()==3,"Repeat before paint produces a compound path per paint");
    check(shape.paints[0].transform==identity_matrix,"Compound paint is applied after path transforms");
    s.apply({OperationOptions{"rect","fill","above","evenodd"},EnableOperation{"rect","repeat",false}},3);
    shape=evaluate_current();check(shape.paths.size()==1&&shape.paints.size()==2&&shape.paints.back().fill_rule=="evenodd","Bypass preserves input; fill rule persists into evaluation");
    s.undo(4);check(evaluate_current().paths.size()==3,"Stack operation Undo restores evaluation");s.redo(5);
    const auto rr=operation_ref("rect","repeat","rotation");
    s.apply({EnableOperation{"rect","repeat",true},Set{operation_ref("rect","repeat","position_x"),0},
        Set{operation_ref("rect","repeat","anchor_x"),10},Set{operation_ref("rect","repeat","anchor_y"),10},
        Set{rr,90}},6);
    shape=evaluate_current();const auto rotated=map_point(shape.paths[1].transform,{20,10});
    check(std::abs(rotated.x-10)<1e-10&&std::abs(rotated.y-20)<1e-10,"Y-down clockwise rotation around authored repeat anchor");
    s.apply({Set{operation_ref("rect","repeat","scale_x"),2},Set{operation_ref("rect","repeat","scale_y"),2},
        Set{operation_ref("rect","repeat","offset"),1}},7);
    shape=evaluate_current();const auto scaled=map_point(shape.paths[0].transform,{20,10});
    check(std::abs(scaled.x-10)<1e-10&&std::abs(scaled.y-30)<1e-10,"Offset shifts transform exponent; scale is multiplicative");
    auto stored=encode(s.document());check(encode(decode(stored))==stored,"Ordered operators and parameters reopen deterministically");
    rejects("OUT_OF_RANGE",[&]{s.apply({Set{operation_ref("rect","repeat","copies"),2.5}},8);});
    rejects("OUT_OF_RANGE",[&]{s.apply({Set{operation_ref("rect","repeat","scale_x"),0}},8);});
    auto nested=default_operation("nested","nect.shape.repeater");nested.parameters["copies"].literal=1000;
    rejects("OUTPUT_LIMIT",[&]{s.apply({Set{operation_ref("rect","repeat","scale_x"),1},Set{operation_ref("rect","repeat","scale_y"),1},
        Set{operation_ref("rect","repeat","copies"),1000},AddOperation{"rect",nested,3}},8);});
    check(encode(s.document())==stored&&s.revision()==8,"Output-limit and parameter failures are atomic");
    const Ref point{"rect","source-top-left","x"};
    s.apply({Link{point,{{"rect","","stroke.width"},1,4,"copy_local_value"}}},8);
    s.apply({Set{operation_ref("rect",stroke,"width"),7}},9);
    check(evaluate(s.document()).at(point)==11&&property(s.document(),{"rect","","stroke.width"}).literal==7,"Legacy address and canonical operation share one Scalar authority");
    stored=encode(s.document());
    rejects("MISSING_REFERENCE",[&]{s.apply({RemoveOperation{"rect",stroke}},10);});
    check(encode(s.document())==stored,"Removing an operation cannot break stable references");
    s.apply({Unlink{point},RemoveOperation{"rect",stroke}},10);
    check(evaluate(s.document()).at(point)==11&&s.document().objects.at("rect").legacy_stroke.empty(),"Explicit freeze permits removing referenced legacy stroke");
    s.apply({Set{operation_ref("rect","repeat","copies"),0}},11);
    check(evaluate_current().paths.empty(),"Zero copies produces no shape geometry");
    s.apply({ConvertToPath{"rect"}},12);
    check(!s.document().objects.at("rect").source&&s.document().objects.at("rect").stack.size()==2&&
        s.document().objects.at("rect").stack.front().id=="repeat","Source conversion preserves the later editable shape stack");
    auto invalid=s.document();invalid.objects.at("rect").stack.front().version=2;
    rejects("UNSUPPORTED_OPERATOR_VERSION",[&]{validate(invalid);});
    fill_rule_link_contract();
    operation_enabled_expression_contract();
    std::cout<<"PASS "<<checks<<" ordered shape stack checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
