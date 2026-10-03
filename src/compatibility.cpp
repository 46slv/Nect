#include "compatibility.hpp"
#include "nect/io.hpp"
#include "nect/blend.hpp"
#include <boost/uuid/detail/sha1.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
namespace nect {
namespace j=boost::json;
namespace {
j::array ids(const std::set<Id>& input){j::array out;for(const auto& id:input)out.push_back(j::value(id));return out;}
j::object ref(const Id& id,const Id& point,const std::string& field){return {{"object",id},{"point",point},{"field",field}};}
std::string digest(const std::string& value){boost::uuids::detail::sha1 hash;hash.process_bytes(value.data(),value.size());unsigned words[5];hash.get_digest(words);std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto word:words)out<<std::setw(8)<<word;return out.str();}
j::array affine(const Affine& value){j::array out;for(double x:value)out.push_back(x);return out;}
j::array geometry(const std::vector<PathInstance>& paths){
    j::array result;
    for(const auto& path:paths){j::array contours;
        for(const auto& contour:*path.contours){j::array points;
            for(const auto& point:contour.points)points.push_back(j::array{point.anchor.x,point.anchor.y,point.incoming.x,point.incoming.y,point.outgoing.x,point.outgoing.y});
            contours.push_back(j::object{{"closed",contour.closed},{"cubics",points}});
        }
        result.push_back(j::object{{"transform",affine(path.transform)},{"contours",contours}});
    }
    return result;
}
j::object bounds(const Bounds& b){return {{"x",b.left},{"y",b.top},{"width",b.right-b.left},{"height",b.bottom-b.top}};}
void unite(std::optional<Bounds>& a,const Bounds& b){if(!a)a=b;else{a->left=std::min(a->left,b.left);a->top=std::min(a->top,b.top);a->right=std::max(a->right,b.right);a->bottom=std::max(a->bottom,b.bottom);}}
}
j::object compatibility_plan(const Document& document,std::uint64_t revision,const Id& composition,
    const Id& artboard,const std::string& target,const j::object& options,const j::object& svg_evidence){
    if(target!="svg/1.1+css-compositing"&&target!="ai/30.8"&&target!="pdf/x-4:2008")throw Error("UNSUPPORTED_TARGET_PROFILE",target);
    double scale=1;
    for(const auto& entry:options){
        if(entry.key()=="raster_scale"){
            if(!entry.value().is_number())throw Error("INVALID_COMPATIBILITY_OPTIONS","raster_scale must be a finite number");
            scale=j::value_to<double>(entry.value());
            if(!std::isfinite(scale)||scale<=0||scale>16)throw Error("INVALID_COMPATIBILITY_OPTIONS","raster_scale must be in (0,16]");
        }else throw Error("INVALID_COMPATIBILITY_OPTIONS","Unknown option: "+std::string(entry.key()));
    }
    const j::object canonical{{"raster_scale",scale}};
    // This is an analysis identity, never a trust token/native ID. Include the
    // committed source as well as revision to distinguish independent Sessions.
    const auto identity=j::serialize(j::array{encode(document),revision,target,composition,artboard,canonical,1});
    j::object out{{"behavior_version",1},{"plan_id","nect.compatibility/1/"+digest(identity)},
        {"source_document",document.id},{"source_revision",revision},{"target_profile",target},
        {"composition",composition},{"artboard",artboard},{"options",canonical},
        {"native_source_preserved",true},{"derivative_generated",false},
        {"items",j::array{}},{"ir_nodes",j::array{}},{"bake_groups",j::array{}},
        {"preserved",j::array{}},{"expanded",j::array{}},{"rasterized",j::array{}},
        {"unsupported",j::array{}},{"editability_losses",j::array{}},{"warnings",j::array{}}};
    if(target!="svg/1.1+css-compositing"){
        out["status"]="policy_qualification_required";out["encoder_available"]=false;out["export_supported"]=false;
        out["expected_validator"]=target=="ai/30.8"?"Illustrator 30.8 native readback":"Acrobat Preflight PDF/X-4:2008";
        out["capability_requirements"]=j::array{"Per-feature target representability matrix","Editable Text/font strategy","Distinct process/Spot/ICC/output-intent/overprint semantics"};
        out["warnings"]=j::array{"Encoder availability is separate from target representability; no item classification is guessed", "PDF representation does not establish Illustrator private data or editable AI"};
        out["counts"]=j::object{{"editable_native",0},{"expandable",0},{"rasterize_required",0},{"unsupported",0}};
        return out;
    }
    out["status"]="planned";out["encoder_available"]=true;out["expected_validator"]="SVG reader with CSS compositing and isolation";
    out["export_supported"]=svg_evidence.at("svg_export_supported");out["legacy_export_plan"]=svg_evidence;
    out["warnings"]=j::array{"Bake groups describe planned derivatives only; the existing SVG encoder does not execute them", "Text is outlined; source text remains editable only in native", "Only encoded sRGB premultiplied RGBA8 bake context is qualified"};
    const auto values=evaluate(document);const auto transforms=evaluate_transforms(document,values);
    const auto scene=evaluate_scene(document,composition,values,transforms);
    const auto& d=scene.expanded_document?*scene.expanded_document:document;
    const auto& v=scene.expanded_values?*scene.expanded_values:values;
    const auto& t=scene.expanded_transforms?*scene.expanded_transforms:transforms;
    const auto enabled=evaluate_operation_enableds(d);
    const auto visible=evaluate_object_visibilities(d);
    const auto mask_enabled=evaluate_geometry_mask_enableds(d);
    const auto source_id=[&](const Id& id){auto found=scene.instance_sources.find(id);return found==scene.instance_sources.end()?id:found->second;};
    const auto source_ref=[&](const Id& occurrence,const Id& point,const std::string& field){
        const auto source=source_id(occurrence);Id authored_point=point;
        if(source!=occurrence&&document.objects.contains(source)){
            const auto& original=document.objects.at(source);const auto& projected=d.objects.at(occurrence);
            for(std::size_t i=0;i<projected.stack.size()&&i<original.stack.size();++i)
                if(projected.stack[i].id==point)authored_point=original.stack[i].id;
            if(projected.compositing.mask&&original.compositing.mask&&projected.compositing.mask->id==point)
                authored_point=original.compositing.mask->id;
        }
        return ref(source,authored_point,field);
    };
    auto& items=out.at("items").as_array();auto& ir=out.at("ir_nodes").as_array();auto& bakes=out.at("bake_groups").as_array();
    std::map<Id,std::vector<std::size_t>> item_indices;std::set<Id> rendered;
    const auto item=[&](const Id& occurrence,const Id& point,const std::string& field,const std::string& type,unsigned version,
        const std::string& classification,const std::string& reason,const std::set<Id>& dependencies){
        const auto source=source_id(occurrence);j::object record{{"source_id",source},{"source_ref",source_ref(occurrence,point,field)},
            {"occurrence_id",occurrence},{"semantic_type",type},{"semantic_version",version},
            {"classification",classification},{"reason_code",reason},{"dependencies",ids(dependencies)},
            {"native_source_preserved",true}};
        if(auto found=scene.instance_owners.find(occurrence);found!=scene.instance_owners.end())record["instance_id"]=found->second;
        item_indices[occurrence].push_back(items.size());items.push_back(record);
    };
    ir.push_back(j::object{{"kind","frame"},{"source_id",artboard},{"source_ref",ref(artboard,"","artboard")},{"frame",svg_evidence.at("artboard")}});
    const auto composition_it=std::find_if(document.compositions.begin(),document.compositions.end(),[&](const auto& c){return c.id==composition;});
    const auto background=artboard_background_state(*composition_it,artboard);
    if(background.value){j::array rgba;for(double channel:background.value->rgba)rgba.push_back(channel);
        ir.push_back(j::object{{"kind","output_background_underlay"},{"source_id",artboard},{"source_ref",ref(artboard,"","artboard.background")},
            {"source_artboard",background.source_artboard},{"inherited",background.inherited},{"space","srgb"},{"alpha","straight"},{"rgba",rgba},
            {"classification","editable_native"},{"compositing","destination_over_completed_artwork"},{"included_in_artwork_bakes",false}});
    }

    std::function<void(const EvaluatedSceneNode&)> describe=[&](const auto& node){
        if(!node.visible||node.opacity<=0)return;
        rendered.insert(node.id);const auto& object=d.objects.at(node.id);
        const std::string kind=object.kind==Kind::group?"group":object.text?"text":object.image?"image":"path";
        const bool expanded_instance=scene.instance_sources.contains(node.id)||(document.objects.contains(node.id)&&document.objects.at(node.id).kind==Kind::instance);
        std::string classification=object.text||object.source||object.instance||object.path_follow||expanded_instance?"expandable":"editable_native";
        const std::string reason=object.text?"TEXT_OUTLINES":classification=="expandable"?"EVALUATED_DERIVATIVE":"TARGET_PRIMITIVE";
        item(node.id,"","object",kind,1,classification,reason,{});
        j::object node_ir{{"kind",kind},{"source_id",source_id(node.id)},{"occurrence_id",node.id},
            {"source_ref",ref(source_id(node.id),"","object")},{"classification",classification},
            {"world_transform",affine(node.world)},{"opacity",node.opacity},{"blend",node.blend},{"isolation",node.isolated}};
        if(object.text){node_ir["text_strategy"]="outlines";node_ir["text_source_id"]=document.objects.contains(source_id(node.id))&&document.objects.at(source_id(node.id)).text?document.objects.at(source_id(node.id)).text->id:object.text->id;}
        if(auto found=scene.shapes.find(node.id);found!=scene.shapes.end()){
            node_ir["geometry"]=geometry(found->second.paths);j::array paints;
            for(const auto& paint:found->second.paints){j::array rgba;for(double channel:paint.rgba)rgba.push_back(channel);
                j::object record{{"source_ref",source_ref(node.id,paint.operation,"operation")},{"kind",paint.type},
                    {"geometry",geometry(paint.paths)},{"transform",affine(paint.transform)},
                    {"color",j::object{{"kind","process"},{"space","srgb"},{"components",rgba},{"alpha","straight"}}},
                    {"width",paint.width},{"fill_rule",paint.fill_rule},{"line_cap",paint.line_cap},{"line_join",paint.line_join},{"miter_limit",paint.miter_limit}};
                if(paint.gradient){j::array stops;for(const auto& stop:paint.gradient->stops){j::array color;for(double channel:stop.rgba)color.push_back(channel);stops.push_back(j::object{{"offset",stop.offset},{"rgba",color}});}
                    record["gradient"]=j::object{{"type",paint.gradient->type},{"start",j::array{paint.gradient->start.x,paint.gradient->start.y}},
                        {"end",j::array{paint.gradient->end.x,paint.gradient->end.y}},{"stops",stops}};
                }
                paints.push_back(std::move(record));
            }
            node_ir["paints"]=std::move(paints);
        }
        if(object.image){node_ir["image_asset"]=object.image->asset;node_ir["image_strategy"]="accepted_snapshot_to_oriented_srgb_png";}
        j::array children;for(const auto& child:node.children)children.push_back(j::value(child.id));node_ir["children"]=children;
        if(node.mask)node_ir["mask"]=j::object{{"source_id",source_id(node.mask->source)},{"mode",node.mask->mode},{"invert",node.mask->invert}};
        ir.push_back(std::move(node_ir));
        for(const auto& op:object.stack)if(enabled.at(operation_ref(node.id,op.id,"enabled"))){
            const bool paint=op.type.starts_with("nect.paint.");const bool pixel=op.type=="nect.group.posterize";
            item(node.id,op.id,"operation",op.type,op.version,pixel?"rasterize_required":paint?"editable_native":"expandable",
                pixel?"PIXEL_EFFECT_REQUIRES_DERIVATIVE":paint?"TARGET_PAINT":"EVALUATED_OPERATION",{});
        }
        if(node.mask)item(node.id,object.compositing.mask->id,"mask",node.mask->mode+"_mask",1,
            node.mask->mode=="geometry"?"editable_native":"rasterize_required",
            node.mask->mode=="geometry"?"VECTOR_CLIP":"APPEARANCE_MASK_REQUIRES_DERIVATIVE",{source_id(node.mask->source)});
        if(const auto* descriptor=find_blend_mode(node.blend);descriptor&&descriptor->svg_representation!="css-mix-blend-mode")
            item(node.id,"","composite.blend",node.blend,descriptor->behavior_version,"unsupported","EXTERNAL_BACKDROP_CLOSURE_UNRESOLVED",{});
        for(const auto& child:node.children)describe(child);
    };
    for(const auto& root:scene.roots)describe(root);
    std::set<Ref> named_inputs;
    for(const auto& color_ref:color_properties(d))if(rendered.contains(color_ref.object)){
        if(auto linked=color_link(d,color_ref);linked&&d.named_colors.contains(linked->object))named_inputs.insert(*linked);
    }
    for(const auto& color_ref:named_inputs){const auto color=color_value(d,color_ref,v);j::array components;for(double channel:color.rgba)components.push_back(channel);
        ir.push_back(j::object{{"kind","named_process_color"},{"source_id",color_ref.object},{"source_ref",ref(color_ref.object,"","color")},
            {"space",color.space},{"profile",color.profile},{"alpha",color.alpha},{"components",components},
            {"target_strategy","evaluated_process_color"},{"spot_semantics",false}});
    }
    
    std::function<void(const EvaluatedSceneNode&)> plan_bake=[&](const auto& root){
        if(!root.visible||root.opacity<=0)return;
        const bool needs=!root.posterize_levels.empty()||(root.mask&&root.mask->mode!="geometry");
        if(!needs){for(const auto& child:root.children)plan_bake(child);
            return;}
        std::set<Id> members,borrowed,mask_sources;std::optional<Bounds> region;std::string refusal;
        if(root.blend!="normal")refusal="EXTERNAL_BACKDROP_CLOSURE_UNRESOLVED";
        std::function<bool(const EvaluatedSceneNode&)> collect=[&](const auto& node){
            if(!node.visible||node.opacity<=0)return false;
            const auto& object=d.objects.at(node.id);bool contributes=false;
            if(object.kind==Kind::group){for(const auto& child:node.children)contributes=collect(child)||contributes;}
            else if(scene.images.contains(node.id))contributes=true;
            else if(auto shape=scene.shapes.find(node.id);shape!=scene.shapes.end()){
                for(const auto& paint:shape->second.paints){
                    bool alpha=paint.rgba[3]>0;
                    if(paint.gradient)alpha=alpha&&std::any_of(paint.gradient->stops.begin(),paint.gradient->stops.end(),[](const auto& stop){return stop.rgba[3]>0;});
                    const bool stroke=paint.type=="nect.paint.stroke";
                    if(alpha&&(!stroke||paint.width>0)&&(!paint.paths.empty()||(stroke&&!paint.degenerate_subpaths.empty()))){
                        contributes=true;if(stroke)refusal="STROKE_BAKE_BOUNDS_UNQUALIFIED";
                    }
                }
            }
            if(!contributes)return false;
            members.insert(node.id);
            if(object.kind!=Kind::group)if(const auto b=object_bounds(d,node.id,v,t,true))unite(region,*b);
            if(node.mask){borrowed.insert(node.mask->source);mask_sources.insert(node.mask->source);}
            if(object.path_follow||scene.deformation_owners.contains(node.id)||(object.text&&object.text->path_attachment))refusal="UNQUALIFIED_GEOMETRY_DEPENDENCY";
            return true;
        };collect(root);
        // Keep borrowed mask/transform inputs out of the editability-loss set.
        // They are sampled, not removed from their original editable location.
        std::set<Id> visited;
        std::function<void(const Id&)> dependencies=[&](const Id& id){
            if(!visited.insert(id).second)return;
            if(auto tr=t.find(id);tr!=t.end()&&!tr->second.effective_parent.empty()){
                const auto parent=tr->second.effective_parent;if(!members.contains(parent))borrowed.insert(parent);dependencies(parent);
            }
        };
        for(const auto& id:members)dependencies(id);
        const auto initial_borrowed=mask_sources;
        std::function<void(const Id&)> mask_inputs=[&](const Id& id){
            if(!d.objects.contains(id)){refusal="MISSING_BAKE_DEPENDENCY";return;}
            dependencies(id);const auto& object=d.objects.at(id);
            for(const auto& child:object.children)if(visible.at(child)&&v.at({child,"","composite.opacity"})>0){borrowed.insert(child);mask_inputs(child);}
            if(object.compositing.mask&&mask_enabled.at(geometry_mask_enabled_ref(id,object.compositing.mask->id))){
                // Nested appearance-mask evaluation has independent visibility
                // and enabled rules; defer until that closure is qualified.
                refusal="NESTED_MASK_CLOSURE_UNQUALIFIED";
            }
        };
        for(const auto& id:initial_borrowed)if(!members.contains(id))mask_inputs(id);
        for(const auto& [p,evaluated]:v)if(members.contains(p.object)||borrowed.contains(p.object)){
            (void)evaluated;
            try{const auto scalar=property(d,p);if(scalar.binding||scalar.expression)refusal="DRIVEN_BAKE_CLOSURE_UNQUALIFIED";}
            catch(const Error&){refusal="TYPED_BAKE_DEPENDENCY_UNQUALIFIED";}
        }
        auto dependency_objects=members;dependency_objects.insert(borrowed.begin(),borrowed.end());
        for(const auto& id:dependency_objects){const auto& object=d.objects.at(id);
            if(object.path_follow||(object.text&&object.text->path_attachment))refusal="UNQUALIFIED_GEOMETRY_DEPENDENCY";
            if(object.visibility_driver||object.visibility_expression||object.compositing.isolated_driver||object.compositing.isolated_expression)
                refusal="DRIVEN_BAKE_CLOSURE_UNQUALIFIED";
            if(object.point_edit&&(object.point_edit->enabled_driver||object.point_edit->enabled_expression))refusal="DRIVEN_BAKE_CLOSURE_UNQUALIFIED";
            if(object.text){const auto& text=*object.text;
                if(text.content_driver||text.family_driver||text.locale_driver||text.direction_driver||text.layout_driver||text.alignment_driver||text.weight_driver||text.weight_expression||text.italic_driver)refusal="DRIVEN_BAKE_CLOSURE_UNQUALIFIED";
            }
            for(const auto& operation:object.stack)if(operation.enabled_driver||operation.enabled_expression||operation.fill_rule_driver||
                (operation.gradient&&(operation.gradient->enabled_driver||operation.gradient->enabled_expression)))refusal="DRIVEN_BAKE_CLOSURE_UNQUALIFIED";
            if(object.compositing.mask&&(object.compositing.mask->enabled_driver||object.compositing.mask->enabled_expression))refusal="DRIVEN_BAKE_CLOSURE_UNQUALIFIED";
        }
        for(const auto& id:members)borrowed.erase(id);
        if(!region)refusal="EMPTY_BAKE_REGION";
        if(region){
            region->left=std::floor(region->left*scale)/scale;region->top=std::floor(region->top*scale)/scale;
            region->right=std::ceil(region->right*scale)/scale;region->bottom=std::ceil(region->bottom*scale)/scale;
            const double width=(region->right-region->left)*scale,height=(region->bottom-region->top)*scale;
            if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||width>8192||height>8192||width*height>16777216)
                refusal="BAKE_RESOURCE_LIMIT";
        }
        if(!refusal.empty()){
            for(const auto index:item_indices[root.id]){auto& record=items[index].as_object();
                if(record.at("classification")=="rasterize_required"){record["classification"]="unsupported";record["reason_code"]=refusal;}
            }
            for(const auto& child:root.children)plan_bake(child);
            return;
        }
        std::set<Id> source_members,source_dependencies;
        for(const auto& id:members)source_members.insert(source_id(id));
        for(const auto& id:borrowed)source_dependencies.insert(source_id(id));
        const auto bake_id="bake/"+digest(j::serialize(j::array{out.at("plan_id"),root.id,ids(members),ids(borrowed)}));
        bakes.push_back(j::object{{"id",bake_id},{"root",source_id(root.id)},{"occurrence_root",root.id},
            {"members",ids(source_members)},{"occurrence_members",ids(members)},{"borrowed_dependencies",ids(source_dependencies)},
            {"bounds",bounds(*region)},{"padding",0},{"scale",scale},{"resolution_unit","pixels_per_document_unit"},
            {"color_space","encoded-srgb"},{"alpha","premultiplied"},{"channel_bits",8},{"alpha_bits",8},
            {"render_context","time-independent"},{"filter_radius",0},{"losses",ids(source_members)},
            {"execution_available",false},{"policy","isolated_local_fill_image_subtree; no external backdrop flatten"}});
        ir.push_back(j::object{{"kind","planned_raster_derivative"},{"source_id",source_id(root.id)},
            {"source_ref",ref(source_id(root.id),"","object")},{"bake_id",bake_id},{"bounds",bounds(*region)},
            {"classification","rasterize_required"}});
        for(auto& entry:ir){auto& record=entry.as_object();
            if(auto occurrence=record.if_contains("occurrence_id");occurrence&&members.contains(std::string(occurrence->as_string()))){
                record["classification"]="rasterize_required";record["bake_id"]=bake_id;record["emitted_independently"]=false;
            }
        }
        for(const auto& id:members)for(const auto index:item_indices[id]){auto& record=items[index].as_object();
            record["classification"]="rasterize_required";record["reason_code"]="LOCAL_DERIVATIVE_CLOSURE";record["bake_id"]=bake_id;
            record["dependencies"]=ids(source_dependencies);
        }
    };
    for(const auto& root:scene.roots)plan_bake(root);
    // Legacy SVG rejects some authored hidden effects/masks before rendering.
    // Keep those refusals visible without inventing a rendered bake region.
    for(const auto* field:{"unsupported_effects","unsupported_masks"})for(const auto& entry:svg_evidence.at(field).as_array()){
        const auto& evidence=entry.as_object();const std::string id(evidence.at("object").as_string());
        if(!rendered.contains(id)){
            const auto point=evidence.if_contains("operation");
            item(id,point?std::string(point->as_string()):Id{},field,"legacy_svg_admission",1,"unsupported","LEGACY_AUTHORED_SOURCE_ADMISSION",{});
        }
    }
    for(auto& entry:items){auto& record=entry.as_object();
        if(record.at("classification")=="rasterize_required"&&!record.contains("bake_id")){
            record["classification"]="unsupported";record["reason_code"]="NO_QUALIFIED_PIXEL_CLOSURE";
        }
    }
    j::object counts{{"editable_native",0},{"expandable",0},{"rasterize_required",0},{"unsupported",0}};
    for(const auto& entry:items){const auto& record=entry.as_object();const std::string classification(record.at("classification").as_string());
        counts[classification]=counts.at(classification).as_int64()+1;
        const auto list=classification=="editable_native"?"preserved":classification=="expandable"?"expanded":classification=="rasterize_required"?"rasterized":"unsupported";
        out.at(list).as_array().push_back(record.at("source_ref"));
        if(classification=="expandable"||classification=="rasterize_required")out.at("editability_losses").as_array().push_back(j::object{
            {"source_ref",record.at("source_ref")},{"occurrence_id",record.at("occurrence_id")},{"reason_code",record.at("reason_code")},
            {"loss",classification=="rasterize_required"?"target_editability_replaced_by_planned_raster":"procedural_or_text_editability_expanded_to_target_primitives"}});
    }
    out["counts"]=std::move(counts);return out;
}
}
