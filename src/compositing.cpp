#include "nect/core.hpp"
#include <algorithm>
#include <functional>

namespace nect {
EvaluatedScene evaluate_scene(const Document& document,const Id& composition,const std::map<Ref,double>& values,
    const std::map<Id,EvaluatedTransform>& transforms) {
    const auto plane=std::find_if(document.compositions.begin(),document.compositions.end(),[&](const auto& c){return c.id==composition;});
    if(plane==document.compositions.end())throw Error("MISSING_COMPOSITION",composition);
    EvaluatedScene scene;
    const auto shape=[&](const Id& id)->const EvaluatedShape& {
        if(const auto found=scene.shapes.find(id);found!=scene.shapes.end())return found->second;
        return scene.shapes.emplace(id,evaluate_shape(document,id,values)).first->second;
    };
    std::function<EvaluatedSceneNode(const Id&,unsigned)> node=[&](const Id& id,unsigned depth) {
        if(depth>128)throw Error("HIERARCHY_DEPTH","Scene hierarchy depth limit 128");
        const auto& object=document.objects.at(id);const auto& composite=object.compositing;
        EvaluatedSceneNode result;result.id=id;result.world=transforms.at(id).world;
        result.visible=object.visible;result.opacity=values.at({id,"","composite.opacity"});result.blend=composite.blend;
        if(composite.mask&&composite.mask->enabled) {
            const auto& mask=*composite.mask;EvaluatedMask resolved;resolved.source=mask.source;resolved.fill_rule=mask.fill_rule;
            for(const auto& path:shape(mask.source).paths)
                resolved.paths.push_back({path.contours,compose(transforms.at(mask.source).world,path.transform)});
            result.mask=std::move(resolved);
        }
        result.isolated=composite.isolated||result.opacity!=1||result.blend!="normal"||result.mask.has_value();
        scene.requires_compositing=scene.requires_compositing||result.isolated;
        if(object.kind==Kind::group)for(const auto& child:object.children)result.children.push_back(node(child,depth+1));
        else (void)shape(id);
        return result;
    };
    for(const auto& id:plane->roots)scene.roots.push_back(node(id,0));
    return scene;
}
}
