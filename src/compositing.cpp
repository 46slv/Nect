#include "nect/core.hpp"
#include <algorithm>
#include <functional>

namespace nect {
EvaluatedScene evaluate_scene(const Document& document,const Id& composition,const std::map<Ref,double>& values,
    const std::map<Id,EvaluatedTransform>& transforms) {
    auto projection=project_definition_instances(document,composition,values,transforms);
    const auto& scene_document=projection.document?*projection.document:document;
    const auto& scene_values=projection.values?*projection.values:values;
    const auto& scene_transforms=projection.transforms?*projection.transforms:transforms;
    const auto plane=std::find_if(scene_document.compositions.begin(),scene_document.compositions.end(),[&](const auto& c){return c.id==composition;});
    if(plane==scene_document.compositions.end())throw Error("MISSING_COMPOSITION",composition);
    const auto visibility=evaluate_object_visibilities(scene_document);
    const auto authored_isolation=evaluate_composite_isolations(scene_document);
    const auto fill_rules=evaluate_fill_rules(scene_document);
    const auto operation_enabled=evaluate_operation_enableds(scene_document);
    const auto gradient_enabled=evaluate_gradient_enableds(scene_document);
    const auto mask_enabled=evaluate_geometry_mask_enableds(scene_document);
    EvaluatedScene scene;
    scene.expanded_document=projection.document;
    scene.expanded_values=projection.values;
    scene.expanded_transforms=projection.transforms;
    scene.instance_owners=std::move(projection.instance_owners);
    scene.instance_sources=std::move(projection.instance_sources);
    const auto shape=[&](const Id& id)->const EvaluatedShape& {
        if(const auto found=scene.shapes.find(id);found!=scene.shapes.end())return found->second;
        return scene.shapes.emplace(id,evaluate_shape(scene_document,id,scene_values,&fill_rules,&operation_enabled,&gradient_enabled)).first->second;
    };
    std::function<EvaluatedSceneNode(const Id&,unsigned)> node=[&](const Id& id,unsigned depth) {
        if(depth>128)throw Error("HIERARCHY_DEPTH","Scene hierarchy depth limit 128");
        const auto& object=scene_document.objects.at(id);const auto& composite=object.compositing;
        EvaluatedSceneNode result;result.id=id;result.world=scene_transforms.at(id).world;
        result.visible=visibility.at(id);result.opacity=scene_values.at({id,"","composite.opacity"});result.blend=composite.blend;
        if(composite.mask&&mask_enabled.at(geometry_mask_enabled_ref(id,composite.mask->id))) {
            const auto& mask=*composite.mask;EvaluatedMask resolved;resolved.source=mask.source;
            resolved.mode=mask.mode;resolved.fill_rule=mask.fill_rule;
            resolved.mask_color_space=mask.mask_color_space;resolved.invert=mask.invert;
            if(mask.mode=="geometry")for(const auto& path:shape(mask.source).paths)
                resolved.paths.push_back({path.contours,compose(scene_transforms.at(mask.source).world,path.transform)});
            result.mask=std::move(resolved);
        }
        if(object.kind==Kind::group)for(const auto& operation:object.stack)
            if(operation.type=="nect.group.posterize"&&operation_enabled.at(operation_ref(id,operation.id,"enabled")))
                result.posterize_levels.push_back(static_cast<unsigned>(scene_values.at(operation_ref(id,operation.id,"levels"))));
        result.isolated=authored_isolation.at(id)||result.opacity!=1||result.blend!="normal"||result.mask.has_value()||!result.posterize_levels.empty();
        scene.requires_compositing=scene.requires_compositing||result.isolated;
        if(object.kind==Kind::group)for(const auto& child:object.children)result.children.push_back(node(child,depth+1));
        else if(object.image)scene.images.emplace(id,EvaluatedImage{scene_document.raster_assets.at(object.image->asset).payload,scene_values.at({id,"","image.width"}),scene_values.at({id,"","image.height"})});
        else (void)shape(id);
        return result;
    };
    for(const auto& id:plane->roots)scene.roots.push_back(node(id,0));
    return scene;
}
}
