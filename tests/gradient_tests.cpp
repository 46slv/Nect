#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,("Expected "+std::string(code)+", got "+e.code).c_str());return;}throw std::runtime_error("Expected rejection");}
Primitive rectangle(const Id& id) {return default_primitive(id,"nect.shape.rectangle");}
Gradient linear_gradient(const Id& id,bool enabled) {
    Gradient gradient;gradient.id=id;gradient.enabled=enabled;gradient.end_x.literal=200;
    GradientStop first;first.id=id+"-first";first.rgba[0].literal=1;
    GradientStop second;second.id=id+"-second";second.offset.literal=1;second.rgba[2].literal=1;
    gradient.stops={first,second};return gradient;
}
void linked_gradient_semantics() {
    Session session(empty_document("linked-gradient-doc","plane-a","art-a"));
    auto apply=[&](std::vector<Command> commands){session.apply(std::move(commands),session.revision());};
    auto add_paint=[&](const Id& object,const Id& operation,const Id& gradient,bool enabled) {
        auto fill=default_operation(operation,"nect.paint.fill");
        apply({CreatePrimitive{"plane-a","",object,object,rectangle(object+"-shape")},
            AddOperation{object,fill,0},SetGradient{object,operation,linear_gradient(gradient,enabled)}});
    };
    add_paint("source","source-fill","source-gradient",true);
    add_paint("target","target-fill","target-gradient",false);
    add_paint("alternate","alternate-fill","alternate-gradient",false);
    const auto source=gradient_ref("source","source-fill","source-gradient","enabled");
    const auto target=gradient_ref("target","target-fill","target-gradient","enabled");
    const auto alternate=gradient_ref("alternate","alternate-fill","alternate-gradient","enabled");
    const auto source_operation=operation_ref("source","source-fill","enabled");
    const auto target_operation=operation_ref("target","target-fill","enabled");
    const auto original=encode(session.document());
    apply({LinkGradientEnabled{target,source}});
    auto state=gradient_enabled_state(session.document(),target);
    auto shape=evaluate_shape(session.document(),"target",evaluate(session.document()));
    const auto target_has_gradient=[&](const EvaluatedShape& value) {
        return std::any_of(value.paints.begin(),value.paints.end(),[](const auto& paint){return paint.operation=="target-fill"&&paint.gradient.has_value();});
    };
    check(state.literal==false&&state.driver==source&&state.evaluated&&target_has_gradient(shape),
        "A false authored target follows a true same-Composition gradient source in evaluated paint");
    const auto target_stop_ref=gradient_ref("target","target-fill","target-gradient",
        "stop.target-gradient-first.color");
    const auto enabled_gradients=evaluate_gradient_enableds(session.document());
    const auto enabled_operations=evaluate_operation_enableds(session.document());
    check(color_is_used(session.document(),target_stop_ref,&enabled_operations,&enabled_gradients),
        "Used-color discovery consumes the evaluated target Gradient bypass");
    check(operation_enabled_property(session.document(),source_operation)&&
        evaluate_operation_enabled(session.document(),source_operation)&&evaluate_operation_enabled(session.document(),target_operation),
        "Gradient dependency target keeps a separate owning operation enabled property");
    const auto linked=encode(session.document());
    check(session.history().states.back().label.find("Link Gradient enabled")!=std::string::npos,
        "Gradient link history receives a descriptive typed-command label");
    session.undo(session.revision());check(encode(session.document())==original,"Undo restores the authored literal and removes the gradient link");
    session.redo(session.revision());check(encode(session.document())==linked,"Redo restores the exact Gradient enabled source Ref");
    apply({EnableOperation{"source","source-fill",false}});
    shape=evaluate_shape(session.document(),"target",evaluate(session.document()));
    check(!evaluate_operation_enabled(session.document(),source_operation)&&
        evaluate_gradient_enabled(session.document(),target)&&target_has_gradient(shape),
        "Bypassing the source paint operation does not erase its independently evaluated Gradient bool");
    apply({EnableOperation{"source","source-fill",true}});

    auto target_replacement=*session.document().objects.at("target").stack.front().gradient;
    target_replacement.start_y.literal=35;target_replacement.stops.front().rgba[1].literal=.4;
    apply({SetGradient{"target","target-fill",target_replacement}});
    state=gradient_enabled_state(session.document(),target);
    check(state.literal==false&&state.driver==source&&state.evaluated&&
        session.document().objects.at("target").stack.front().gradient->start_y.literal==35,
        "Same-ID geometry and stop replacement preserves a driven target and its authored literal");
    const auto before_direct=encode(session.document());const auto before_revision=session.revision();
    auto changed_literal=*session.document().objects.at("target").stack.front().gradient;changed_literal.enabled=true;
    rejects("DRIVEN_PROPERTY",[&]{apply({SetGradient{"target","target-fill",changed_literal}});});
    check(session.revision()==before_revision&&encode(session.document())==before_direct,
        "SetGradient cannot change a driven literal and rejection preserves revision and authored bytes");
    auto changed_driver=*session.document().objects.at("target").stack.front().gradient;changed_driver.enabled_driver=alternate;
    rejects("USE_TYPED_COMMAND",[&]{apply({SetGradient{"target","target-fill",changed_driver}});});
    auto new_gradient=linear_gradient("injected-gradient",false);new_gradient.enabled_driver=source;
    rejects("USE_TYPED_COMMAND",[&]{apply({SetGradient{"target","target-fill",new_gradient}});});
    check(session.revision()==before_revision&&encode(session.document())==before_direct,
        "SetGradient cannot replace or smuggle a driver into either the same-ID or a new Gradient");

    rejects("DRIVEN_PROPERTY",[&]{apply({LinkGradientEnabled{target,alternate}});});
    const auto replacement_revision=session.revision();
    apply({LinkGradientEnabled{target,alternate,true}});
    check(gradient_enabled_state(session.document(),target).driver==alternate&&
        !gradient_enabled_state(session.document(),target).evaluated,
        "Driver replacement succeeds only with its explicit replacement flag");
    rejects("DEPENDENCY_CYCLE",[&]{apply({LinkGradientEnabled{alternate,target,true}});});
    check(session.revision()==replacement_revision+1&&gradient_enabled_state(session.document(),target).driver==alternate,
        "Cycle rejection leaves the previously accepted source unchanged");
    apply({UnlinkGradientEnabled{target}});
    state=gradient_enabled_state(session.document(),target);
    check(!state.driver&&!state.literal&&!state.evaluated&&
        session.history().states.back().label.find("Unlink Gradient enabled")!=std::string::npos,
        "Unlink freezes the evaluated bool into the target authored literal");
    apply({SetGradient{"alternate","alternate-fill",linear_gradient("alternate-gradient",true)}});
    check(!gradient_enabled_state(session.document(),target).evaluated,
        "An unlinked target no longer follows later source changes");

    apply({LinkGradientEnabled{target,source}});
    auto same_id_source=*session.document().objects.at("alternate").stack.front().gradient;
    same_id_source.end_y.literal=77;
    apply({SetGradient{"alternate","alternate-fill",same_id_source},LinkGradientEnabled{target,alternate,true}});
    // A failure after a valid preceding edit must leave both edits unapplied.
    const auto batch_before=encode(session.document());const auto batch_revision=session.revision();
    rejects("MISSING_GRADIENT",[&]{apply({SetGradient{"alternate","alternate-fill",linear_gradient("replacement-source-gradient",true)},
        UnlinkGradientEnabled{gradient_ref("alternate","alternate-fill","missing-gradient","enabled")}});});
    check(session.revision()==batch_revision&&encode(session.document())==batch_before,
        "Invalid later command rolls back a valid source replacement and preserves the existing dependency");
    rejects("MISSING_OPERATION",[&]{apply({RemoveOperation{"alternate","alternate-fill"}});});
    rejects("MISSING_GRADIENT",[&]{apply({SetGradient{"alternate","alternate-fill",linear_gradient("replacement-source-gradient",true)}});});
    check(session.revision()==batch_revision&&encode(session.document())==batch_before&&
        gradient_enabled_state(session.document(),target).driver==alternate,
        "Deleting or replacing a referenced source gradient fails without silently unlinking its target");

    const auto target_before_bad=encode(session.document());const auto bad_revision=session.revision();
    rejects("MISSING_REFERENCE",[&]{apply({Set{target,0}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{target,{operation_ref("target","target-fill","r"),1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{target},{"1",1},false}});});
    check(session.revision()==bad_revision&&encode(session.document())==target_before_bad,
        "Generic Scalar set, link and expression cannot mutate a typed gradient bool");

    Session duplicate(empty_document("duplicate-gradient-doc","duplicate-plane","duplicate-art"));
    auto duplicate_apply=[&](std::vector<Command> commands){duplicate.apply(std::move(commands),duplicate.revision());};
    auto duplicate_fill=default_operation("duplicate-fill","nect.paint.fill");
    auto duplicate_source_fill=default_operation("duplicate-source-fill","nect.paint.fill");
    duplicate_apply({CreatePrimitive{"duplicate-plane","","source","Source",rectangle("source-shape")},
        AddOperation{"source",duplicate_source_fill,0},SetGradient{"source","duplicate-source-fill",linear_gradient("source-gradient",true)},
        CreatePrimitive{"duplicate-plane","","target","Target",rectangle("target-shape")},
        AddOperation{"target",duplicate_fill,0},SetGradient{"target","duplicate-fill",linear_gradient("target-gradient",false)},
        LinkGradientEnabled{gradient_ref("target","duplicate-fill","target-gradient","enabled"),
            gradient_ref("source","duplicate-source-fill","source-gradient","enabled")}});
    duplicate_apply({DuplicateObjects{{"source","target"},"gradient-copy"}});
    const auto duplicate_source=duplicate.document().objects.at("gradient-copy-1").stack.front();
    const auto duplicate_target=duplicate.document().objects.at("gradient-copy-2").stack.front();
    const auto copied_target_ref=gradient_ref("gradient-copy-2",duplicate_target.id,duplicate_target.gradient->id,"enabled");
    const auto copied_source_ref=gradient_ref("gradient-copy-1",duplicate_source.id,duplicate_source.gradient->id,"enabled");
    check(duplicate_target.gradient->enabled_driver==copied_source_ref&&
        gradient_enabled_state(duplicate.document(),copied_target_ref).evaluated,
        "Duplicating both endpoints remaps the copied Gradient dependency to the copied source");
    duplicate_apply({DuplicateObjects{{"target"},"external-copy"}});
    const auto external_target=duplicate.document().objects.at("external-copy-1").stack.front();
    check(external_target.gradient->enabled_driver==gradient_ref("source","duplicate-source-fill","source-gradient","enabled"),
        "Duplicating only the target retains its original external source identity");

    auto two_planes=empty_document("cross-gradient-doc","plane-a","art-a");
    Composition second;second.id="plane-b";second.name="Other";second.artboards.push_back({"art-b","Artboard",0,0,640,480,{},{}});
    two_planes.compositions.push_back(second);Session cross(std::move(two_planes));
    auto first_fill=default_operation("first-fill","nect.paint.fill");
    auto cross_fill=default_operation("cross-fill","nect.paint.fill");
    cross.apply({CreatePrimitive{"plane-a","","cross-target","Target",rectangle("cross-target-shape")},
        AddOperation{"cross-target",first_fill,0},SetGradient{"cross-target","first-fill",linear_gradient("first-gradient",false)},
        CreatePrimitive{"plane-b","","cross-source","Source",rectangle("cross-shape")},
        AddOperation{"cross-source",cross_fill,0},SetGradient{"cross-source","cross-fill",linear_gradient("cross-gradient",true)}},0);
    const auto cross_target=gradient_ref("cross-target","first-fill","first-gradient","enabled");
    rejects("CROSS_COMPOSITION",[&]{cross.apply({LinkGradientEnabled{cross_target,
        gradient_ref("cross-source","cross-fill","cross-gradient","enabled")}},cross.revision());});

    constexpr int count=130;auto depth_session=Session(empty_document("depth-gradient-doc","depth-plane","depth-art"));
    std::vector<Command> nodes;nodes.reserve(count*2);
    for(int i=0;i<count;++i) {
        const auto id="depth-"+std::to_string(i),operation=id+"-fill",gradient=id+"-gradient";
        nodes.push_back(CreatePrimitive{"depth-plane","",id,id,rectangle(id+"-shape")});
        nodes.push_back(AddOperation{id,default_operation(operation,"nect.paint.fill"),0});
        nodes.push_back(SetGradient{id,operation,linear_gradient(gradient,i==count-2)});
    }
    for(int i=0;i<count-2;++i)nodes.push_back(LinkGradientEnabled{
        gradient_ref("depth-"+std::to_string(i),"depth-"+std::to_string(i)+"-fill","depth-"+std::to_string(i)+"-gradient","enabled"),
        gradient_ref("depth-"+std::to_string(i+1),"depth-"+std::to_string(i+1)+"-fill","depth-"+std::to_string(i+1)+"-gradient","enabled")});
    depth_session.apply(nodes,0);
    check(evaluate_gradient_enabled(depth_session.document(),gradient_ref("depth-0","depth-0-fill","depth-0-gradient","enabled")),
        "A dependency chain at the 128-edge limit evaluates successfully");
    const auto depth_before=encode(depth_session.document());
    rejects("DEPENDENCY_DEPTH",[&]{depth_session.apply({LinkGradientEnabled{
        gradient_ref("depth-128","depth-128-fill","depth-128-gradient","enabled"),
        gradient_ref("depth-129","depth-129-fill","depth-129-gradient","enabled")}},depth_session.revision());});
    check(depth_session.revision()==1&&encode(depth_session.document())==depth_before,
        "A 129-edge chain is rejected atomically at the evaluator depth limit");
}
void expression_gradient_semantics() {
    Session session(empty_document("expression-gradient-doc","expression-plane","expression-art"));
    auto apply=[&](std::vector<Command> commands){session.apply(std::move(commands),session.revision());};
    auto add_paint=[&](const Id& object,const Id& operation,const Id& gradient,bool enabled) {
        apply({CreatePrimitive{"expression-plane","",object,object,rectangle(object+"-shape")},
            AddOperation{object,default_operation(operation,"nect.paint.fill"),0},
            SetGradient{object,operation,linear_gradient(gradient,enabled)}});
    };
    add_paint("source","source-fill","source-gradient",false);
    add_paint("target","target-fill","target-gradient",false);
    const auto source=gradient_ref("source","source-fill","source-gradient","enabled");
    const auto target=gradient_ref("target","target-fill","target-gradient","enabled");
    const Expression expression{" ! ref ( \"source\" , \"\" , \"op.source-fill.gradient.source-gradient.enabled\" ) ",1};
    apply({SetGradientEnabledExpression{target,expression}});
    auto state=gradient_enabled_state(session.document(),target);
    const auto target_has_gradient=[&] {
        const auto shape=evaluate_shape(session.document(),"target",evaluate(session.document()));
        return std::any_of(shape.paints.begin(),shape.paints.end(),[](const auto& paint) {
            return paint.operation=="target-fill"&&paint.gradient.has_value();
        });
    };
    const auto target_operation=operation_ref("target","target-fill","enabled");
    check(state.literal==false&&!state.driver&&state.expression==expression&&state.evaluated&&target_has_gradient()&&
        evaluate_operation_enabled(session.document(),target_operation),
        "A false target literal evaluates a negated Gradient Ref and enables only its own retained paint");
    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.70\"")!=std::string::npos&&
        native.find("\"enabled_expression\":{\"source\":")!=std::string::npos&&encode(decode(native))==native,
        "Native 0.70 round-trips the optional Gradient expression source exactly");
    apply({DuplicateObjects{{"source","target"},"expression-copy"}});
    const auto copied_source_it=std::find_if(session.document().objects.begin(),session.document().objects.end(),
        [](const auto& item){return item.second.name=="source copy";});
    const auto copied_target_it=std::find_if(session.document().objects.begin(),session.document().objects.end(),
        [](const auto& item){return item.second.name=="target copy";});
    check(copied_source_it!=session.document().objects.end()&&copied_target_it!=session.document().objects.end(),
        "Duplicating both Gradient expression endpoints creates identified object copies");
    const auto& copied_source=copied_source_it->second;
    const auto& copied_target=copied_target_it->second;
    const auto& copied_source_operation=copied_source.stack.front();
    const auto& copied_target_operation=copied_target.stack.front();
    const auto copied_expression_source=" ! ref ( \""+copied_source.id+"\" , \"\" , \"op."+
        copied_source_operation.id+".gradient."+copied_source_operation.gradient->id+".enabled\" ) ";
    const auto copied_target_ref=gradient_ref(copied_target.id,copied_target_operation.id,
        copied_target_operation.gradient->id,"enabled");
    check(copied_target_operation.gradient->enabled_expression==Expression{copied_expression_source,1}&&
        gradient_enabled_state(session.document(),copied_target_ref).evaluated,
        "Duplicating both endpoints remaps the Gradient expression's object, operation and Gradient IDs");
    apply({DuplicateObjects{{"target"},"expression-external-copy"}});
    const auto& external_target=session.document().objects.at("expression-external-copy-1");
    const auto& external_target_operation=external_target.stack.front();
    const auto external_target_ref=gradient_ref(external_target.id,external_target_operation.id,
        external_target_operation.gradient->id,"enabled");
    check(external_target_operation.gradient->enabled_expression==expression&&
        gradient_enabled_state(session.document(),external_target_ref).evaluated,
        "Duplicating only the target preserves its exact reference to the external Gradient source");
    const auto current_version=native.find("\"version\":\"0.70\"");
    auto lied=native;lied.replace(current_version,std::string("\"version\":\"0.70\"").size(),"\"version\":\"0.68\"");
    rejects("UNSUPPORTED_GRADIENT_ENABLED_EXPRESSION",[&]{(void)decode(lied);});
    const auto same_revision=session.revision();apply({SetGradientEnabledExpression{target,expression}});
    check(session.revision()==same_revision,"Exact expression reapplication is idempotent");

    apply({SetVisibility{"source",false},EnableOperation{"source","source-fill",false}});
    check(evaluate_gradient_enabled(session.document(),target)&&target_has_gradient(),
        "Gradient expression evaluation ignores source Object visibility and operation enablement");
    auto target_geometry=*session.document().objects.at("target").stack.front().gradient;
    target_geometry.end_y.literal=47;apply({SetGradient{"target","target-fill",target_geometry}});
    state=gradient_enabled_state(session.document(),target);
    check(state.literal==false&&state.expression==expression&&state.evaluated&&
        session.document().objects.at("target").stack.front().gradient->end_y.literal==47,
        "Same-ID geometry edits preserve a driven Gradient expression and literal");
    const auto before_invalid=encode(session.document());const auto before_revision=session.revision();
    auto changed_literal=*session.document().objects.at("target").stack.front().gradient;changed_literal.enabled=true;
    rejects("DRIVEN_PROPERTY",[&]{apply({SetGradient{"target","target-fill",changed_literal}});});
    auto changed_source=*session.document().objects.at("target").stack.front().gradient;changed_source.enabled_expression=Expression{"false",1};
    rejects("USE_TYPED_COMMAND",[&]{apply({SetGradient{"target","target-fill",changed_source}});});
    auto injected=linear_gradient("injected-expression-gradient",false);injected.enabled_expression=expression;
    rejects("USE_TYPED_COMMAND",[&]{apply({SetGradient{"target","target-fill",injected}});});
    rejects("DRIVEN_PROPERTY",[&]{apply({LinkGradientEnabled{target,source}});});
    apply({LinkGradientEnabled{target,source,true}});
    state=gradient_enabled_state(session.document(),target);
    check(state.driver==source&&!state.expression&&!state.literal&&!state.evaluated,
        "Explicit link replacement clears the Gradient expression while retaining its authored literal");
    rejects("DRIVEN_PROPERTY",[&]{apply({SetGradientEnabledExpression{target,expression}});});
    apply({SetGradientEnabledExpression{target,expression,true}});

    rejects("DEPENDENCY_CYCLE",[&]{apply({SetGradientEnabledExpression{target,
        Expression{"ref(\"target\",\"\",\"op.target-fill.gradient.target-gradient.enabled\")",1},true}});});
    rejects("BOOLEAN_EXPRESSION_TYPE",[&]{apply({SetGradientEnabledExpression{target,
        Expression{"ref(\"source\",\"\",\"op.source-fill.enabled\")",1},true}});});
    rejects("BOOLEAN_EXPRESSION_TYPE",[&]{apply({SetGradientEnabledExpression{target,
        Expression{"ref(\"source\",\"red\",\"op.source-fill.gradient.source-gradient.enabled\")",1},true}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetGradientEnabledExpression{target,
        Expression{"ref(\"missing\",\"\",\"op.source-fill.gradient.source-gradient.enabled\")",1},true}});});
    rejects("MISSING_OPERATION",[&]{apply({SetGradientEnabledExpression{target,
        Expression{"ref(\"source\",\"\",\"op.missing.gradient.source-gradient.enabled\")",1},true}});});
    rejects("MISSING_GRADIENT",[&]{apply({SetGradientEnabledExpression{target,
        Expression{"ref(\"source\",\"\",\"op.source-fill.gradient.missing.enabled\")",1},true}});});
    rejects("MISSING_GRADIENT",[&]{apply({SetGradient{"source","source-fill",{}}});});
    rejects("MISSING_OPERATION",[&]{apply({RemoveOperation{"source","source-fill"}});});
    rejects("UNSUPPORTED_EXPRESSION_VERSION",[&]{apply({SetGradientEnabledExpression{target,{"true",2},true}});});
    check(session.revision()==before_revision+2&&encode(session.document())==before_invalid&&
        gradient_enabled_state(session.document(),target).expression==expression,
        "Explicit replace and exact restoration preserve the accepted expression content");

    auto source_enabled=*session.document().objects.at("source").stack.front().gradient;source_enabled.enabled=true;
    apply({SetGradient{"source","source-fill",source_enabled}});
    state=gradient_enabled_state(session.document(),target);
    check(!state.evaluated&&!state.literal&&state.expression==expression&&
        evaluate_operation_enabled(session.document(),target_operation)&&!target_has_gradient(),
        "Changing source false to true bypasses target paint while its authored literal, expression and operation stay fixed");
    const auto before_unlink=encode(session.document());apply({UnlinkGradientEnabled{target}});
    state=gradient_enabled_state(session.document(),target);
    check(!state.driver&&!state.expression&&!state.literal&&!state.evaluated&&
        session.history().states.back().label.find("Unlink Gradient enabled")!=std::string::npos,
        "Unlink freezes evaluated Gradient enabled into the authored literal in one Undo step");
    auto literal_document=session.document();
    for(auto& [object_id,object]:literal_document.objects) {
        (void)object_id;
        for(auto& operation:object.stack)if(operation.gradient)operation.gradient->enabled_expression.reset();
    }
    auto literal_native=encode(literal_document);const auto literal_version=literal_native.find("\"version\":\"0.70\"");
    literal_native.replace(literal_version,std::string("\"version\":\"0.70\"").size(),"\"version\":\"0.68\"");
    check(decode(literal_native)==literal_document,"Native 0.68 still reads Gradients without the new optional source");
    session.undo(session.revision());
    check(encode(session.document())==before_unlink&&gradient_enabled_state(session.document(),target).expression==expression,
        "Undo restores the exact Gradient expression and evaluated bypass");
    std::vector<Id> reordered;
    for(const auto& operation:session.document().objects.at("target").stack)reordered.push_back(operation.id);
    reordered.insert(reordered.begin()+std::min<std::size_t>(1,reordered.size()),"target-offset");
    std::reverse(reordered.begin(),reordered.end());
    apply({AddOperation{"target",default_operation("target-offset","nect.shape.offset"),1},
        Rename{"target","Renamed expression target"},ReorderOperations{"target",reordered}});
    check(!gradient_enabled_property(session.document(),target)&&
        gradient_enabled_state(session.document(),target).expression==expression&&
        gradient_enabled_state(session.document(),target).evaluated==evaluate_gradient_enabled(session.document(),target),
        "Object rename and operation reorder preserve the nested Gradient expression Ref");

    const auto cycle_before=encode(session.document());const auto cycle_revision=session.revision();
    rejects("DEPENDENCY_CYCLE",[&]{apply({Rename{"source","Should roll back"},LinkGradientEnabled{source,target,true}});});
    check(session.revision()==cycle_revision&&encode(session.document())==cycle_before,
        "A mixed link/expression cycle rejects a failed batch atomically");

    auto cross_document=empty_document("cross-expression-doc","cross-plane-a","cross-art-a");
    Composition second;second.id="cross-plane-b";second.name="Other";
    second.artboards.push_back({"cross-art-b","Artboard",0,0,640,480,{},{}});cross_document.compositions.push_back(second);
    Session cross(std::move(cross_document));auto cross_apply=[&](std::vector<Command> commands){cross.apply(std::move(commands),cross.revision());};
    cross_apply({CreatePrimitive{"cross-plane-a","","cross-target","Target",rectangle("cross-target-shape")},
        AddOperation{"cross-target",default_operation("cross-target-fill","nect.paint.fill"),0},
        SetGradient{"cross-target","cross-target-fill",linear_gradient("cross-target-gradient",false)},
        CreatePrimitive{"cross-plane-b","","cross-source","Source",rectangle("cross-source-shape")},
        AddOperation{"cross-source",default_operation("cross-source-fill","nect.paint.fill"),0},
        SetGradient{"cross-source","cross-source-fill",linear_gradient("cross-source-gradient",false)}});
    rejects("CROSS_COMPOSITION",[&]{cross_apply({SetGradientEnabledExpression{
        gradient_ref("cross-target","cross-target-fill","cross-target-gradient","enabled"),
        Expression{"ref(\"cross-source\",\"\",\"op.cross-source-fill.gradient.cross-source-gradient.enabled\")",1}}});});

    constexpr int count=130;auto depth=Session(empty_document("mixed-depth-doc","mixed-depth-plane","mixed-depth-art"));
    std::vector<Command> nodes;nodes.reserve(count*3);
    for(int i=0;i<count;++i) {
        const auto id="mixed-depth-"+std::to_string(i),operation=id+"-fill",gradient=id+"-gradient";
        nodes.push_back(CreatePrimitive{"mixed-depth-plane","",id,id,rectangle(id+"-shape")});
        nodes.push_back(AddOperation{id,default_operation(operation,"nect.paint.fill"),0});
        nodes.push_back(SetGradient{id,operation,linear_gradient(gradient,i==count-1)});
    }
    for(int i=0;i<count-1;++i) {
        const auto current="mixed-depth-"+std::to_string(i),next="mixed-depth-"+std::to_string(i+1);
        const auto target_ref=gradient_ref(current,current+"-fill",current+"-gradient","enabled");
        const auto source_ref=gradient_ref(next,next+"-fill",next+"-gradient","enabled");
        if(i%2==0)nodes.push_back(LinkGradientEnabled{target_ref,source_ref});
        else nodes.push_back(SetGradientEnabledExpression{target_ref,
            Expression{"ref(\""+next+"\",\"\",\"op."+next+"-fill.gradient."+next+"-gradient.enabled\")",1}});
    }
    rejects("DEPENDENCY_DEPTH",[&]{depth.apply(nodes,0);});
    check(depth.revision()==0,"A mixed 129-edge Gradient source chain rejects without committing state");
}
}
int main(){try{
    Session s(empty_document("doc","comp","art"));
    auto apply=[&](std::vector<Command> cmds){s.apply(cmds,s.revision());};
    auto has_property=[&](const Ref& ref) {
        const auto refs=properties(s.document());
        return std::find(refs.begin(),refs.end(),ref)!=refs.end();
    };
    apply({CreatePrimitive{"comp","","rect","Gradient",{"source","nect.shape.rectangle",1,
        {{"center_x",{100,{}}},{"center_y",{100,{}}},{"width",{200,{}}},{"height",{200,{}}}}}}});
    auto fill=default_operation("fill","nect.paint.fill");fill.parameters["a"].literal=.5;
    apply({RemoveOperation{"rect",s.document().objects.at("rect").legacy_stroke},AddOperation{"rect",fill,0}});
    const auto enabled_ref=gradient_ref("rect","fill","gradient","enabled");
    rejects("MISSING_GRADIENT",[&]{(void)gradient_enabled_property(s.document(),enabled_ref);});
    check(!has_property(enabled_ref),
        "A paint operation without an authored gradient has no gradient enabled property");
    Gradient g;g.id="gradient";g.end_x.literal=200;
    GradientStop red;red.id="red";red.rgba[0].literal=1;red.rgba[3].literal=.25;
    GradientStop blue;blue.id="blue";blue.offset.literal=1;blue.rgba[2].literal=1;
    g.stops={blue,red};apply({SetGradient{"rect","fill",g}});
    const auto red_ref=gradient_ref("rect","fill","gradient","stop.red.r");
    const auto x_ref=gradient_ref("rect","fill","gradient","end_x");
    const auto operation_enabled_ref=operation_ref("rect","fill","enabled");
    check(gradient_enabled_property(s.document(),enabled_ref)&&operation_enabled_property(s.document(),operation_enabled_ref)&&
        property_unit(enabled_ref)=="boolean"&&
        !evaluate(s.document()).contains(enabled_ref)&&
        has_property(enabled_ref)&&
        resolve_name(s.document(),"Gradient","","op.fill.gradient.gradient.enabled")==enabled_ref,
        "Gradient bypass discovery is typed and distinct from its paint operation enabled bit");
    rejects("MISSING_OPERATION",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","missing","gradient","enabled"));});
    rejects("MISSING_REFERENCE",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("missing-object","fill","gradient","enabled"));});
    rejects("MISSING_GRADIENT",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","fill","missing","enabled"));});
    rejects("INVALID_GRADIENT_REF",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","fill","","enabled"));});
    rejects("INVALID_GRADIENT_REF",[&]{(void)gradient_enabled_property(s.document(),Ref{"rect","not-empty",enabled_ref.field});});
    rejects("UNKNOWN_GRADIENT_PROPERTY",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","fill","gradient","enabled.extra"));});
    rejects("MISSING_REFERENCE",[&]{apply({Set{enabled_ref,0}});});
    rejects("MISSING_REFERENCE",[&]{apply({Link{enabled_ref,{operation_ref("rect","fill","b"),1,0,"copy_local_value"}}});});
    rejects("MISSING_REFERENCE",[&]{apply({SetExpression{{enabled_ref},{"1",1},false}});});
    const auto failed_batch_revision=s.revision();
    rejects("MISSING_REFERENCE",[&]{apply({Set{x_ref,25},Set{enabled_ref,0}});});
    check(s.revision()==failed_batch_revision&&property(s.document(),x_ref).literal==200,
        "An invalid Scalar edit after a valid gradient Scalar edit rejects the whole batch");
    auto current=[&]{return evaluate_shape(s.document(),"rect",evaluate(s.document()));};
    auto shape=current();
    check(shape.paints[0].gradient->stops[0].rgba[0]==1&&shape.paints[0].gradient->stops[1].rgba[2]==1,"Evaluation sorts offsets independently of authored stop order");
    check(shape.paints[0].rgba[3]==.5&&shape.paints[0].gradient->stops[0].rgba[3]==.25,"Overall alpha and stop alpha remain distinct");
    check(property_unit(x_ref)=="du"&&property_unit(red_ref)=="scalar","Gradient positions and colors have explicit units");
    apply({Link{operation_ref("rect","fill","b"),{red_ref,1,0,"copy_local_value"}}});
    std::reverse(g.stops.begin(),g.stops.end());apply({SetGradient{"rect","fill",g},Set{red_ref,.8}});
    check(evaluate(s.document()).at(operation_ref("rect","fill","b"))==.8,"Reordering authored stops preserves reference identity");
    const auto saved=encode(s.document());
    check(encode(decode(saved))==saved,"Native retains gradient order, Scalars, bindings and IDs exactly");
    rejects("MISSING_REFERENCE",[&]{apply({SetGradient{"rect","fill",{}}});});
    auto removed=*s.document().objects.at("rect").stack[0].gradient;
    removed.stops.erase(removed.stops.begin());GradientStop replacement=red;replacement.id="new-red";removed.stops.push_back(replacement);
    rejects("MISSING_REFERENCE",[&]{apply({SetGradient{"rect","fill",removed}});});
    check(encode(s.document())==saved,"Deleting a linked stop or component rejects atomically");
    rejects("GRADIENT_STOPS",[&]{apply({Set{gradient_ref("rect","fill","gradient","stop.blue.offset"),0}});});
    rejects("GRADIENT_GEOMETRY",[&]{apply({Set{x_ref,0}});});
    rejects("OUT_OF_RANGE",[&]{apply({Set{red_ref,1.01}});});
    rejects("UNIT_MISMATCH",[&]{apply({Link{x_ref,{red_ref,1,0,"copy_local_value"}}});});
    rejects("DEPENDENCY_CYCLE",[&]{apply({Link{red_ref,{operation_ref("rect","fill","b"),1,0,"copy_local_value"}}});});
    check(encode(s.document())==saved,"All invalid gradient edits preserve authored state");
    auto bypass=*s.document().objects.at("rect").stack[0].gradient;bypass.enabled=false;
    apply({SetGradient{"rect","fill",bypass}});
    check(!gradient_enabled_property(s.document(),enabled_ref)&&operation_enabled_property(s.document(),operation_enabled_ref)&&
        !current().paints[0].gradient&&evaluate(s.document()).at(operation_ref("rect","fill","b"))==.8&&
        has_property(enabled_ref),
        "Solid bypass reads false while retaining its stable Ref and separate paint operation state");
    s.undo(s.revision());check(gradient_enabled_property(s.document(),enabled_ref)&&current().paints[0].gradient.has_value(),
        "Undo restores the authored gradient bypass through the same Ref");
    s.begin_gesture(s.revision());s.update_gesture({Set{x_ref,260}});s.update_gesture({Set{x_ref,300}});s.commit_gesture();
    check(property(s.document(),x_ref).literal==300,"Endpoint gesture commits its final preview");
    s.undo(s.revision());check(property(s.document(),x_ref).literal==200,"Endpoint gesture is one Undo step");
    auto repeat=default_operation("repeat","nect.shape.repeater");repeat.parameters["copies"].literal=2;repeat.parameters["end_opacity"].literal=.2;
    apply({AddOperation{"rect",repeat,1}});shape=current();
    rejects("INVALID_DOMAIN",[&]{(void)gradient_enabled_property(s.document(),gradient_ref("rect","repeat","gradient","enabled"));});
    check(shape.paints.size()==2&&shape.paints[0].transform[4]==100&&std::abs(shape.paints[0].rgba[3]-.1)<1e-12&&shape.paints[0].gradient->end.x==200,"Repeat after paint carries gradient with transformed paint and opacity");
    apply({ReorderOperations{"rect",{"repeat","fill"}}});shape=current();
    check(shape.paints.size()==1&&shape.paints[0].paths.size()==2&&shape.paints[0].transform==identity_matrix,"Repeat before paint shares one gradient across the compound geometry");
    apply({Rename{"rect","Renamed Gradient"}});
    check(resolve_name(s.document(),"Renamed Gradient","",enabled_ref.field)==enabled_ref&&
        gradient_enabled_property(s.document(),enabled_ref),"Rename and operation reorder preserve the nested Ref identity");
    auto wrong_kind=s.document();
    auto wrong_kind_fill=std::find_if(wrong_kind.objects.at("rect").stack.begin(),wrong_kind.objects.at("rect").stack.end(),
        [](const auto& operation){return operation.id=="fill";});
    wrong_kind_fill->type="nect.shape.repeater";
    rejects("INVALID_DOMAIN",[&]{(void)gradient_enabled_property(wrong_kind,enabled_ref);});
    apply({DuplicateObjects{{"rect"},"gradient-copy"}});
    const auto copied_object_it=std::find_if(s.document().objects.begin(),s.document().objects.end(),
        [](const auto& item){return item.first!="rect"&&item.second.name!="Gradient"&&item.second.kind==Kind::path;});
    check(copied_object_it!=s.document().objects.end(),"Object duplication creates an independently identified paint owner");
    const auto& copied_object=copied_object_it->second;
    const auto copied_fill=std::find_if(copied_object.stack.begin(),copied_object.stack.end(),
        [](const auto& operation){return operation.type=="nect.paint.fill";});
    check(copied_fill!=copied_object.stack.end()&&copied_fill->gradient&&copied_fill->gradient->id!="gradient"&&
        gradient_enabled_property(s.document(),gradient_ref(copied_object.id,copied_fill->id,copied_fill->gradient->id,"enabled")),
        "Duplicating a paint remaps its gradient identity while preserving a readable bypass");
    auto radial=*s.document().objects.at("rect").stack[1].gradient;radial.type="radial";
    apply({SetGradient{"rect","fill",radial}});
    const auto svg=export_svg(s.document(),"comp","art");
    check(svg.find("radialGradient")!=std::string::npos&&svg.find("gradientUnits=\"userSpaceOnUse\"")!=std::string::npos&&svg.find("fill-opacity=\"0.5\"")!=std::string::npos,"SVG preserves gradient coordinate space and overall opacity");
    auto invalid=s.document();invalid.objects.at("rect").stack[1].gradient->stops[0].id="source";
    rejects("DUPLICATE_ID",[&]{validate(invalid);});
    invalid=s.document();invalid.objects.at("rect").stack[1].gradient->type="conic";
    rejects("UNSUPPORTED_GRADIENT",[&]{validate(invalid);});
    apply({Unlink{operation_ref("rect","fill","b")}});
    auto replacement_gradient=*s.document().objects.at("rect").stack[1].gradient;replacement_gradient.id="replacement-gradient";
    apply({SetGradient{"rect","fill",replacement_gradient}});
    const auto replacement_ref=gradient_ref("rect","fill","replacement-gradient","enabled");
    rejects("MISSING_GRADIENT",[&]{(void)gradient_enabled_property(s.document(),enabled_ref);});
    check(gradient_enabled_property(s.document(),replacement_ref)&&
        !has_property(enabled_ref)&&has_property(replacement_ref),
        "Replacing with a new gradient ID removes the old Ref and exposes the new one");
    apply({SetGradient{"rect","fill",{}}});
    rejects("MISSING_GRADIENT",[&]{(void)gradient_enabled_property(s.document(),replacement_ref);});
    check(!current().paints[0].gradient&&!has_property(replacement_ref),
        "Removing a gradient removes its typed enabled property");
    linked_gradient_semantics();
    expression_gradient_semantics();
    std::cout<<"PASS "<<checks<<" gradient checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
