#include "nect/core.hpp"
#include <algorithm>
#include <functional>
#include <cmath>
#include <limits>
#include <numbers>

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
    struct DeformLeaf { Id group;const GroupPathFollow* relation;const GroupPathFollowItem* item; };
    std::map<Id,DeformLeaf> deform_leaves;
    std::map<Id,PathSampler> deform_samplers;
    std::map<Id,std::pair<double,double>> deform_spans;
    std::map<Id,std::size_t> deform_counts;
    for(const auto& [group_id,group]:scene_document.objects)if(group.path_follow&&group.path_follow->mode=="deform") {
        const auto& relation=*group.path_follow;
        // Only groups belonging to this Composition are projected here.
        bool belongs=false;
        std::function<void(const Id&)> find=[&](const Id& current) {
            if(current==group_id)belongs=true;
            for(const auto& child:scene_document.objects.at(current).children)find(child);
        };
        for(const auto& root:plane->roots)find(root);
        if(!belongs)continue;
        (void)inverse_affine(scene_transforms.at(group_id).world);
        deform_samplers.emplace(group_id,build_path_sampler(scene_document,relation.path,relation.contour,
            scene_values,scene_transforms.at(relation.path).world));
        deform_spans.emplace(group_id,std::pair{std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()});
        std::function<void(const Id&,const GroupPathFollowItem&)> attach=[&](const Id& current,const GroupPathFollowItem& item) {
            const auto& member=scene_document.objects.at(current);
            if(member.kind==Kind::group) {for(const auto& child:member.children)attach(child,item);return;}
            if(member.kind!=Kind::path)throw Error("GROUP_PATH_DEFORM_UNSUPPORTED_DOMAIN","Only Path/primitive geometry can deform");
            if(!deform_leaves.emplace(current,DeformLeaf{group_id,&relation,&item}).second)
                throw Error("GROUP_PATH_DEFORM_NESTED_RELATION","A leaf cannot be deformed by multiple Group relations");
        };
        for(const auto& [child,item]:relation.items)attach(child,item);
    }
    const auto shape=[&](const Id& id)->const EvaluatedShape& {
        if(const auto found=scene.shapes.find(id);found!=scene.shapes.end())return found->second;
        auto projected=evaluate_shape(scene_document,id,scene_values,&fill_rules,&operation_enabled,&gradient_enabled);
        if(const auto follower=deform_leaves.find(id);follower!=deform_leaves.end()) {
            const auto& leaf=follower->second;const auto& relation=*leaf.relation;const auto& item=*leaf.item;
            const auto& sampler=deform_samplers.at(leaf.group);
            const auto group_world=scene_transforms.at(leaf.group).world;
            const auto group_inverse=inverse_affine(group_world);
            const auto local_to_group=compose(group_inverse,scene_transforms.at(id).world);
            const auto start=(relation.start_mode=="normalized"?relation.start*sampler.length:relation.start)+item.distance;
            const bool y_axis=relation.deform_axis=="y";
            const auto check_coordinate=[&](Vec2 p) {
                const auto u=(y_axis?p.y:p.x)+start;
                if(!std::isfinite(u)||!std::isfinite(p.x)||!std::isfinite(p.y))
                    throw Error("OUTPUT_RANGE","Group Path Deform source coordinates must be finite");
                auto& span=deform_spans.at(leaf.group);span.first=std::min(span.first,u);span.second=std::max(span.second,u);
                if(!sampler.closed&&(u<0||u>sampler.length))
                    throw Error("GROUP_PATH_DEFORM_RANGE","Group Path Deform source geometry exceeds the open contour");
                if(sampler.closed&&span.second-span.first>sampler.length)
                    throw Error("GROUP_PATH_DEFORM_SPAN","Group Path Deform source span exceeds one closed-contour lap");
            };
            const auto project_point=[&](const CubicPoint& source,const Affine& matrix) {
                const auto anchor=map_point(matrix,source.anchor);
                const auto incoming=map_point(matrix,source.incoming),outgoing=map_point(matrix,source.outgoing);
                for(const auto p:{anchor,incoming,outgoing})check_coordinate(p);
                const auto sample=sample_path(sampler,start+(y_axis?anchor.y:anchor.x),relation.reversed);
                const Affine frame{sample.tangent.x,sample.tangent.y,sample.normal.x,sample.normal.y,
                    sample.position.x,sample.position.y};
                const auto local_frame=compose(group_inverse,frame);
                const auto uv=[&](Vec2 p) {return y_axis?Vec2{p.y,p.x}:p;};
                const auto coordinate=uv(anchor);
                const auto position=map_point(local_frame,{0,relation.normal_offset+item.normal_offset+coordinate.y});
                const auto handle=[&](Vec2 control) {
                    const auto delta=uv(Vec2{control.x-anchor.x,control.y-anchor.y});
                    return Vec2{position.x+local_frame[0]*delta.x+local_frame[2]*delta.y,
                        position.y+local_frame[1]*delta.x+local_frame[3]*delta.y};
                };
                CubicPoint result{position,handle(incoming),handle(outgoing)};
                for(const auto p:{result.anchor,result.incoming,result.outgoing})
                    if(!std::isfinite(p.x)||!std::isfinite(p.y))throw Error("OUTPUT_RANGE","Group Path Deform output is non-finite");
                return result;
            };
            // Cache snapshots shared by final geometry and ordered paint layers.
            using Key=std::pair<const std::vector<EvaluatedContour>*,Affine>;
            std::map<Key,std::shared_ptr<const std::vector<EvaluatedContour>>> snapshots;
            const auto project_path=[&](PathInstance& instance,const Affine& parent) {
                const auto matrix=compose(local_to_group,compose(parent,instance.transform));
                const Key key{instance.contours.get(),matrix};
                auto cached=snapshots.find(key);
                if(cached==snapshots.end()) {
                    auto contours=std::make_shared<std::vector<EvaluatedContour>>(*instance.contours);
                    for(auto& contour:*contours)for(auto& point:contour.points) {
                        if(++deform_counts[leaf.group]>1000000)
                            throw Error("OUTPUT_LIMIT","Group Path Deform limit is 1000000 projected anchors per relation");
                        point=project_point(point,matrix);
                    }
                    cached=snapshots.emplace(key,std::move(contours)).first;
                }
                instance.contours=cached->second;instance.transform=identity_matrix;
            };
            for(auto& instance:projected.paths)project_path(instance,identity_matrix);
            for(auto& paint:projected.paints) {
                const auto paint_matrix=compose(local_to_group,paint.transform);
                for(auto& instance:paint.paths)project_path(instance,paint.transform);
                for(auto& point:paint.degenerate_subpaths)point=project_point({point,point,point},paint_matrix).anchor;
                if(paint.gradient) {
                    // Paint coordinates retain their authored affine field;
                    // deformation bends cubic geometry, not the gradient field.
                    paint.gradient->start=map_point(paint_matrix,paint.gradient->start);
                    paint.gradient->end=map_point(paint_matrix,paint.gradient->end);
                }
                paint.transform=identity_matrix;
            }
            auto& provenance=scene.deformation_points[id];
            for(const auto& contour:path_contours(scene_document.objects.at(id),&scene_values))for(const auto& point:contour.points) {
                const auto v=[&](const char* field){return scene_values.at({id,point.id,field});};
                const Vec2 anchor{v("x"),v("y")};
                const auto handle=[&](const char* angle,const char* length) {
                    const auto radians=v(angle)*std::numbers::pi/180;
                    return Vec2{anchor.x+std::cos(radians)*v(length),anchor.y+std::sin(radians)*v(length)};
                };
                const auto mapped=project_point({anchor,handle("in.angle","in.length"),handle("out.angle","out.length")},local_to_group);
                provenance.push_back({point.id,contour.id,mapped.anchor,mapped.incoming,mapped.outgoing,true});
            }
            scene.geometry_worlds[id]=group_world;scene.deformation_owners[id]=leaf.group;
        }
        return scene.shapes.emplace(id,std::move(projected)).first->second;
    };
    std::function<EvaluatedSceneNode(const Id&,unsigned)> node=[&](const Id& id,unsigned depth) {
        if(depth>128)throw Error("HIERARCHY_DEPTH","Scene hierarchy depth limit 128");
        const auto& object=scene_document.objects.at(id);const auto& composite=object.compositing;
        if(object.kind!=Kind::group&&!object.image)(void)shape(id);
        EvaluatedSceneNode result;result.id=id;
        result.world=scene.geometry_worlds.contains(id)?scene.geometry_worlds.at(id):scene_transforms.at(id).world;
        result.visible=visibility.at(id);result.opacity=scene_values.at({id,"","composite.opacity"});result.blend=composite.blend;
        if(composite.mask&&mask_enabled.at(geometry_mask_enabled_ref(id,composite.mask->id))) {
            const auto& mask=*composite.mask;EvaluatedMask resolved;resolved.source=mask.source;
            resolved.mode=mask.mode;resolved.fill_rule=mask.fill_rule;
            resolved.mask_color_space=mask.mask_color_space;resolved.invert=mask.invert;
            if(mask.mode=="geometry")for(const auto& path:shape(mask.source).paths)
                resolved.paths.push_back({path.contours,compose(scene.geometry_worlds.contains(mask.source)?
                    scene.geometry_worlds.at(mask.source):scene_transforms.at(mask.source).world,path.transform)});
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
