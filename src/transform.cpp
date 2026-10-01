#include "nect/core.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>

namespace nect {
namespace {
void require(bool condition,const char* code,const std::string& message) {
    if(!condition)throw Error(code,message);
}
void finite_matrix(const Affine& matrix) {
    for(const auto value:matrix)require(std::isfinite(value),
        "OUTPUT_RANGE","Evaluated transform must have finite entries");
}
const std::array<std::string,6> matrix_fields{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty"};

class BoundsBuilder {
public:
    std::optional<Bounds> result;
    void point(Vec2 value) {
        require(std::isfinite(value.x)&&std::isfinite(value.y),"OUTPUT_RANGE","Non-finite geometric bounds");
        if(!result)result=Bounds{value.x,value.y,value.x,value.y};
        else {
            result->left=std::min(result->left,value.x);result->right=std::max(result->right,value.x);
            result->top=std::min(result->top,value.y);result->bottom=std::max(result->bottom,value.y);
        }
    }
    void rectangle(const Bounds& box,const Affine& matrix) {
        point(map_point(matrix,{box.left,box.top}));point(map_point(matrix,{box.right,box.top}));
        point(map_point(matrix,{box.left,box.bottom}));point(map_point(matrix,{box.right,box.bottom}));
    }
    void cubic(Vec2 p0,Vec2 p1,Vec2 p2,Vec2 p3) {
        for(const auto p:{p0,p1,p2,p3})require(std::isfinite(p.x)&&std::isfinite(p.y),"OUTPUT_RANGE","Non-finite cubic geometry");
        point(p0);point(p3);
        const auto at=[&](double t) {
            // De Casteljau avoids the cancellation in an expanded cubic near an endpoint.
            const auto mix=[&](Vec2 a,Vec2 b){return Vec2{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};};
            return mix(mix(mix(p0,p1),mix(p1,p2)),mix(mix(p1,p2),mix(p2,p3)));
        };
        const auto roots=[&](double v0,double v1,double v2,double v3) {
            double a=-v0+3*v1-3*v2+v3,b=2*(v0-2*v1+v2),c=v1-v0;
            const auto norm=std::max({std::abs(a),std::abs(b),std::abs(c)});
            if(norm==0)return;
            a/=norm;b/=norm;c/=norm;
            const auto add=[&](double t){if(t>0&&t<1)point(at(t));};
            if(a==0){if(b!=0)add(-c/b);return;}
            const auto discriminant=b*b-4*a*c;
            if(discriminant<0)return;
            const auto root=std::sqrt(discriminant);
            const auto q=-0.5*(b+std::copysign(root,b));
            if(q==0)add(-b/(2*a));
            else {add(q/a);add(c/q);}
        };
        roots(p0.x,p1.x,p2.x,p3.x);roots(p0.y,p1.y,p2.y,p3.y);
    }
    void contours(const std::vector<EvaluatedContour>& contours,const Affine& matrix) {
        for(const auto& contour:contours) {
            if(contour.points.empty())continue;
            point(map_point(matrix,contour.points.front().anchor));
            const auto segments=contour.closed?contour.points.size():contour.points.size()-1;
            for(std::size_t i=0;i<segments;++i) {
                const auto& p=contour.points[i];const auto& q=contour.points[(i+1)%contour.points.size()];
                cubic(map_point(matrix,p.anchor),map_point(matrix,p.outgoing),map_point(matrix,q.incoming),map_point(matrix,q.anchor));
            }
        }
    }
};
}

Affine inverse_affine(const Affine& matrix) {
    for(const auto value:matrix)require(std::isfinite(value),"SINGULAR_TRANSFORM","Cannot invert a non-finite transform");
    const auto norm=std::max({std::abs(matrix[0]),std::abs(matrix[1]),std::abs(matrix[2]),std::abs(matrix[3])});
    require(norm>0,"SINGULAR_TRANSFORM","Cannot invert a singular transform");
    const auto a=matrix[0]/norm,b=matrix[1]/norm,c=matrix[2]/norm,d=matrix[3]/norm;
    const auto determinant=a*d-b*c;
    require(determinant!=0&&std::isfinite(determinant),"SINGULAR_TRANSFORM","Cannot invert a singular transform");
    const auto factor=1/norm/determinant;
    Affine result{d*factor,-b*factor,-c*factor,a*factor,0,0};
    result[4]=-result[0]*matrix[4]-result[2]*matrix[5];
    result[5]=-result[1]*matrix[4]-result[3]*matrix[5];
    for(const auto value:result)require(std::isfinite(value),"SINGULAR_TRANSFORM","Transform inverse cannot be represented with finite values");
    return result;
}

std::map<Id,EvaluatedTransform> evaluate_transforms(const Document& document,const std::map<Ref,double>& values) {
    std::map<Id,Id> structural_parents,compositions;
    std::function<void(const Id&,const Id&,const Id&,unsigned)> own=[&](const Id& id,const Id& parent,const Id& composition,unsigned depth) {
        require(depth<=128,"HIERARCHY_DEPTH","Structural hierarchy depth limit 128");
        const auto object=document.objects.find(id);require(object!=document.objects.end(),"MISSING_OBJECT",id);
        require(compositions.emplace(id,composition).second,"INVALID_HIERARCHY","Repeated structural owner or cycle: "+id);
        structural_parents.emplace(id,parent);
        for(const auto& child:object->second.children)own(child,id,composition,depth+1);
    };
    for(const auto& composition:document.compositions)for(const auto& id:composition.roots)own(id,{},composition.id,0);
    require(compositions.size()==document.objects.size(),"ORPHAN_OBJECT","Every object requires one structural owner");

    struct FollowedItem {
        Id group;
        const GroupPathFollow* relation=nullptr;
        const GroupPathFollowItem* item=nullptr;
    };
    std::map<Id,FollowedItem> followed_items;
    for(const auto& [group_id,object]:document.objects)if(object.path_follow) {
        for(const auto& [child,item]:object.path_follow->items) {
            require(std::find(object.children.begin(),object.children.end(),child)!=object.children.end(),
                "GROUP_PATH_FOLLOW_NOT_CHILD","A followed Object must be a direct child of its Group");
            require(followed_items.emplace(child,FollowedItem{group_id,&*object.path_follow,&item}).second,
                "GROUP_PATH_FOLLOW_DUPLICATE_ITEM","An Object cannot be attached to more than one Group Path Follow relation");
        }
    }
    std::map<Id,EvaluatedTransform> result;
    std::map<Id,unsigned> chain_depth;
    std::map<std::pair<Id,Id>,PathSampler> sampler_cache;
    std::set<Id> active;
    std::function<const EvaluatedTransform&(const Id&,unsigned)> visit=[&](const Id& id,unsigned depth)->const EvaluatedTransform& {
        require(depth<=128,"TRANSFORM_DEPTH","Effective transform parent depth limit 128");
        if(const auto found=result.find(id);found!=result.end())return found->second;
        if(active.contains(id)&&followed_items.contains(id))
            throw Error("GROUP_PATH_FOLLOW_CYCLE","Group Path Follow and Transform Parent dependencies contain a cycle");
        require(active.insert(id).second,"TRANSFORM_CYCLE","Effective transform parent cycle at "+id);
        const auto& object=document.objects.at(id);EvaluatedTransform item;
        item.effective_parent=object.transform_parent?*object.transform_parent:structural_parents.at(id);
        if(object.transform_parent) {
            require(!item.effective_parent.empty()&&document.objects.contains(item.effective_parent),"MISSING_TRANSFORM_PARENT",item.effective_parent);
            require(compositions.at(id)==compositions.at(item.effective_parent),"CROSS_COMPOSITION","Transform Parent must be in the same Composition");
        }
        for(std::size_t i=0;i<matrix_fields.size();++i) {
            const auto found=values.find({id,"",matrix_fields[i]});
            require(found!=values.end(),"MISSING_REFERENCE",id+"/"+matrix_fields[i]);item.authored_local[i]=found->second;
        }
        finite_matrix(item.authored_local);
        if(const auto followed=followed_items.find(id);followed!=followed_items.end()) {
            const auto& attachment=followed->second;
            require(!object.transform_parent,"GROUP_PATH_FOLLOW_TRANSFORM_PARENT",
                "A followed child cannot have an explicit Transform Parent");
            require(item.effective_parent==attachment.group,"GROUP_PATH_FOLLOW_PARENT",
                "A followed child must inherit its Group transform structurally");
            const auto& group=visit(attachment.group,depth+1);
            const auto source=document.objects.find(attachment.relation->path);
            require(source!=document.objects.end(),"MISSING_PATH_ATTACHMENT","Group Path Follow source Path no longer exists");
            require(source->second.kind==Kind::path,"INVALID_PATH_ATTACHMENT","Group Path Follow source must be a Path object");
            const auto& path=visit(attachment.relation->path,depth+1);
            const auto sampler_key=std::pair{attachment.relation->path,attachment.relation->contour};
            auto sampler=sampler_cache.find(sampler_key);
            if(sampler==sampler_cache.end())sampler=sampler_cache.emplace(sampler_key,
                build_path_sampler(document,sampler_key.first,sampler_key.second,values,path.world)).first;
            const auto& relation=*attachment.relation;
            const auto& item_values=*attachment.item;
            double start=relation.start_mode=="normalized"?relation.start*sampler->second.length:relation.start;
            require(std::isfinite(start),"GROUP_PATH_FOLLOW_RANGE","Group Path Follow start resolves to a non-finite distance");
            if(!sampler->second.closed) {
                require(relation.start_mode=="distance"||relation.start_mode=="normalized","GROUP_PATH_FOLLOW_START_MODE",
                    "Group Path Follow start mode must be distance or normalized");
                require(relation.start>=0&&(relation.start_mode=="normalized"?relation.start<=1:relation.start<=sampler->second.length),
                    "GROUP_PATH_FOLLOW_RANGE","Group Path Follow start is outside its open contour");
            }
            const auto distance=start+item_values.distance;
            require(std::isfinite(distance),"GROUP_PATH_FOLLOW_RANGE","Group Path Follow item distance is non-finite");
            if(!sampler->second.closed)require(distance>=0&&distance<=sampler->second.length,"GROUP_PATH_FOLLOW_RANGE",
                "Group Path Follow item sample is outside its open contour");
            const auto sample=sample_path(sampler->second,distance,relation.reversed);
            const auto offset=relation.normal_offset+item_values.normal_offset;
            require(std::isfinite(offset),"GROUP_PATH_FOLLOW_OFFSET","Group Path Follow offset is non-finite");
            const Vec2 position{sample.position.x+sample.normal.x*offset,sample.position.y+sample.normal.y*offset};
            require(std::isfinite(position.x)&&std::isfinite(position.y),"OUTPUT_RANGE","Group Path Follow frame is non-finite");
            const auto group_inverse=inverse_affine(group.world);
            if(item_values.follow_tangent) {
                const Affine world_frame{sample.tangent.x,sample.tangent.y,-sample.tangent.y,sample.tangent.x,position.x,position.y};
                item.derived_local=compose(group_inverse,world_frame);
            } else {
                const auto local_position=map_point(group_inverse,position);
                item.derived_local={1,0,0,1,local_position.x,local_position.y};
            }
            finite_matrix(item.derived_local);
        }
        item.local=compose(item.derived_local,item.authored_local);
        finite_matrix(item.local);
        if(item.effective_parent.empty()){item.world=item.local;chain_depth.emplace(id,0);}
        else {
            item.world=compose(visit(item.effective_parent,depth+1).world,item.local);
            const auto levels=chain_depth.at(item.effective_parent)+1;
            require(levels<=128,"TRANSFORM_DEPTH","Effective transform parent depth limit 128");chain_depth.emplace(id,levels);
        }
        finite_matrix(item.world);active.erase(id);
        return result.emplace(id,std::move(item)).first->second;
    };
    for(const auto& [id,object]:document.objects){(void)object;visit(id,0);}
    return result;
}

std::optional<Bounds> object_bounds(const Document& document,const Id& id,const std::map<Ref,double>& values,
    const std::map<Id,EvaluatedTransform>& transforms,bool world_space) {
    require(document.objects.contains(id),"MISSING_OBJECT",id);
    require(transforms.contains(id),"MISSING_TRANSFORM",id);
    BoundsBuilder bounds;
    std::optional<Affine> target_inverse;
    const auto relative=[&](const Id& child) {
        if(world_space)return transforms.at(child).world;
        // Descendants whose effective chain reaches the target can be evaluated
        // locally even when an ancestor has collapsed an axis. An external chain
        // requires the actual target-world inverse and fails explicitly if singular.
        auto next=child;auto matrix=identity_matrix;
        for(unsigned depth=0;depth<=128;++depth) {
            if(next==id)return matrix;
            const auto found=transforms.find(next);require(found!=transforms.end(),"MISSING_TRANSFORM",next);
            matrix=compose(found->second.local,matrix);next=found->second.effective_parent;
            if(next.empty())break;
        }
        if(!target_inverse)target_inverse=inverse_affine(transforms.at(id).world);
        return compose(*target_inverse,transforms.at(child).world);
    };
    std::set<Id> visited;
    std::function<void(const Id&,unsigned)> visit=[&](const Id& child,unsigned depth) {
        require(depth<=128,"HIERARCHY_DEPTH","Structural hierarchy depth limit 128");
        require(visited.insert(child).second,"INVALID_HIERARCHY","Repeated structural child: "+child);
        const auto found=document.objects.find(child);require(found!=document.objects.end(),"MISSING_OBJECT",child);
        const auto& object=found->second;
        if(object.kind==Kind::group) {for(const auto& member:object.children)visit(member,depth+1);return;}
        const auto to_target=relative(child);
        if(object.image){bounds.rectangle({0,0,values.at({child,"","image.width"}),values.at({child,"","image.height"})},to_target);return;}
        const auto shape=evaluate_shape(document,child,values);
        std::optional<Bounds> text_bounds;
        if(object.text&&!object.text->path_attachment) {
            const auto layout=evaluate_text_projection(document,child,values);
            text_bounds=Bounds{layout.x,layout.y,layout.x+layout.width,layout.y+layout.height};
        }
        const auto path=[&](const PathInstance& instance,const Affine& parent) {
            const auto matrix=compose(parent,instance.transform);
            if(instance.contours)bounds.contours(*instance.contours,matrix);
            if(text_bounds)bounds.rectangle(*text_bounds,matrix);
        };
        // Include final path geometry (also editable when no paint is enabled)
        // plus any earlier geometry captured by ordered paints/Repeaters.
        for(const auto& instance:shape.paths)path(instance,to_target);
        for(const auto& paint:shape.paints)for(const auto& instance:paint.paths)path(instance,compose(to_target,paint.transform));
    };
    visit(id,0);return bounds.result;
}
}
