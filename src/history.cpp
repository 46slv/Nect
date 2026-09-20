#include "nect/core.hpp"
#include <algorithm>
#include <iterator>
#include <limits>
#include <type_traits>

namespace nect {
namespace {
// An admission estimate, not a measurement of process heap or resident memory.
// Count reserved container capacity, map/list nodes, allocation bookkeeping and
// every owned string capacity (including SSO, deliberately conservatively).
constexpr std::size_t allocation_overhead=32;
std::size_t add(std::size_t a,std::size_t b) {
    const auto max=std::numeric_limits<std::size_t>::max();return a>max-b?max:a+b;
}
std::size_t multiply(std::size_t a,std::size_t b) {
    const auto max=std::numeric_limits<std::size_t>::max();return b&&a>max/b?max:a*b;
}
template<class... T>std::size_t total(T... values) {std::size_t result=0;((result=add(result,values)),...);return result;}
std::size_t extra(const std::string& value){return total(value.capacity(),std::size_t{1},allocation_overhead);}
std::size_t extra(const Ref&);
std::size_t extra(const Binding&);
std::size_t extra(const Expression&);
std::size_t extra(const Scalar&);
std::size_t extra(const Point&);
std::size_t extra(const Contour&);
std::size_t extra(const TextSource&);
std::size_t extra(const Primitive&);
std::size_t extra(const PointEdit&);
std::size_t extra(const GradientStop&);
std::size_t extra(const Gradient&);
std::size_t extra(const ShapeOperation&);
std::size_t extra(const Object&);
std::size_t extra(const ArtboardParent&);
std::size_t extra(const Artboard&);
std::size_t extra(const Composition&);
std::size_t extra(const Collection&);
std::size_t extra(const NamedColor&);
template<class T>std::size_t extra(const std::optional<T>&);
template<class A,class B>std::size_t extra(const std::pair<A,B>&);
template<class T,std::size_t N>std::size_t extra(const std::array<T,N>&);
template<class T>std::size_t extra(const std::vector<T>&);
template<class K,class V>std::size_t extra(const std::map<K,V>&);
template<class T>std::size_t extra(const std::optional<T>& value){return value?extra(*value):0;}
template<class A,class B>std::size_t extra(const std::pair<A,B>& value){return total(extra(value.first),extra(value.second));}
template<class T,std::size_t N>std::size_t extra(const std::array<T,N>& values) {
    std::size_t result=0;for(const auto& value:values)result=add(result,extra(value));return result;
}
template<class T>std::size_t extra(const std::vector<T>& values) {
    auto result=values.capacity()?total(multiply(values.capacity(),sizeof(T)),allocation_overhead):0;
    for(const auto& value:values)result=add(result,extra(value));return result;
}
template<class K,class V>std::size_t extra(const std::map<K,V>& values) {
    auto result=multiply(values.size()+1,sizeof(typename std::map<K,V>::value_type)+4*sizeof(void*)+allocation_overhead);
    for(const auto& [key,value]:values)result=total(result,extra(key),extra(value));return result;
}
std::size_t extra(const Ref& v){return total(extra(v.object),extra(v.point),extra(v.field));}
std::size_t extra(const Binding& v){return total(extra(v.source),extra(v.mode));}
std::size_t extra(const Expression& v){return extra(v.source);}
std::size_t extra(const Scalar& v){return total(extra(v.binding),extra(v.expression));}
std::size_t extra(const Point& v){return total(extra(v.id),extra(v.x),extra(v.y),extra(v.in_angle),extra(v.in_length),extra(v.out_angle),extra(v.out_length));}
std::size_t extra(const Contour& v){return total(extra(v.id),extra(v.points));}
std::size_t extra(const TextSource& v){return total(extra(v.id),extra(v.content),extra(v.family),extra(v.locale),extra(v.layout),extra(v.direction),extra(v.alignment),extra(v.parameters));}
std::size_t extra(const Primitive& v){return total(extra(v.id),extra(v.type),extra(v.parameters));}
std::size_t extra(const PointEdit& v){return total(extra(v.id),extra(v.overrides));}
std::size_t extra(const GradientStop& v){return total(extra(v.id),extra(v.offset),extra(v.rgba));}
std::size_t extra(const Gradient& v){return total(extra(v.id),extra(v.type),extra(v.start_x),extra(v.start_y),extra(v.end_x),extra(v.end_y),extra(v.stops));}
std::size_t extra(const ShapeOperation& v){return total(extra(v.id),extra(v.type),extra(v.parameters),extra(v.composite),extra(v.fill_rule),extra(v.gradient));}
std::size_t extra(const Object& v){return total(extra(v.id),extra(v.name),extra(v.children),extra(v.contours),extra(v.transform),extra(v.stack),extra(v.legacy_stroke),extra(v.source),extra(v.point_edit),extra(v.text),extra(v.anchor),extra(v.transform_parent));}
std::size_t extra(const ArtboardParent& v){return extra(v.artboard);}
std::size_t extra(const Artboard& v){return total(extra(v.id),extra(v.name),extra(v.parent_size));}
std::size_t extra(const Composition& v){return total(extra(v.id),extra(v.name),extra(v.roots),extra(v.artboards));}
std::size_t extra(const Collection& v){return total(extra(v.id),extra(v.name),extra(v.members));}
std::size_t extra(const NamedColor& v){return total(extra(v.id),extra(v.name),extra(v.rgba));}

std::string owner_name(const Document& before,const Document& after,const Id& id) {
    for(const auto* document:{&after,&before}) {
        if(const auto it=document->objects.find(id);it!=document->objects.end())return it->second.name;
        if(const auto it=document->named_colors.find(id);it!=document->named_colors.end())return it->second.name;
        for(const auto& comp:document->compositions) {
            if(comp.id==id)return comp.name;
            for(const auto& board:comp.artboards)if(board.id==id)return board.name;
        }
    }
    return id;
}
std::string operation_name(const std::string& type) {
    if(type=="nect.paint.fill")return "Fill";
    if(type=="nect.paint.stroke")return "Stroke";
    if(type=="nect.shape.repeater")return "Repeater";
    return "operation";
}
}

std::string Session::history_label(const std::vector<Command>& commands,const Document& candidate) const {
    const auto name=[&](const Id& id){return owner_name(document_,candidate,id);};
    const auto property_label=[&](const Ref& ref) {
        // A valid batch can edit then delete a target, including a newly created
        // target. A descriptive label must not reject that otherwise valid edit.
        std::string owner;
        try{owner=property_name(candidate,ref);}catch(const Error&){}
        if(owner.empty())try{owner=property_name(document_,ref);}catch(const Error&){}
        if(owner.empty())owner=name(ref.object);
        return owner+(ref.point.empty()?"":" / "+ref.point)+" / "+ref.field;
    };
    auto label=std::visit([&](const auto& c)->std::string {
        using T=std::decay_t<decltype(c)>;
        if constexpr(std::is_same_v<T,Set>)return "Set "+property_label(c.ref);
        else if constexpr(std::is_same_v<T,SetExpression>)return "Expression: "+std::to_string(c.targets.size())+" properties: "+property_label(c.targets.front());
        else if constexpr(std::is_same_v<T,Link>)return "Link "+property_label(c.target);
        else if constexpr(std::is_same_v<T,Unlink>)return "Unlink "+property_label(c.target);
        else if constexpr(std::is_same_v<T,SetColor>)return "Set color: "+property_label(c.ref);
        else if constexpr(std::is_same_v<T,LinkColor>)return "Link color: "+property_label(c.target);
        else if constexpr(std::is_same_v<T,UnlinkColor>)return "Unlink color: "+property_label(c.ref);
        else if constexpr(std::is_same_v<T,Rename>)return "Rename: "+c.name;
        else if constexpr(std::is_same_v<T,RenameNamedColor>)return "Rename color: "+c.name;
        else if constexpr(std::is_same_v<T,CreateNamedColor>)return "Add named color: "+c.color.name;
        else if constexpr(std::is_same_v<T,DeleteNamedColor>)return "Delete named color: "+name(c.color);
        else if constexpr(std::is_same_v<T,CreatePath>)return "Add Path: "+c.name;
        else if constexpr(std::is_same_v<T,CreatePrimitive>)return std::string(c.source.type=="nect.shape.circle"?"Add Circle: ":
            c.source.type=="nect.shape.rectangle"?"Add Rectangle: ":c.source.type=="nect.shape.polygon"?"Add Polygon: ":"Add Star: ")+c.name;
        else if constexpr(std::is_same_v<T,CreateText>)return "Add Text: "+c.name;
        else if constexpr(std::is_same_v<T,UpdateText>)return "Edit Text: "+name(c.object);
        else if constexpr(std::is_same_v<T,GroupContiguous>)return "Group: "+c.name;
        else if constexpr(std::is_same_v<T,CenterAnchor>)return "Center Anchor: "+name(c.object);
        else if constexpr(std::is_same_v<T,SetPosition>)return "Set Position: "+name(c.object);
        else if constexpr(std::is_same_v<T,TransformAroundAnchor>)return "Transform around Anchor: "+name(c.object);
        else if constexpr(std::is_same_v<T,SetTransformParent>)return std::string(c.parent?"Attach Transform Parent: ":"Detach Transform Parent: ")+name(c.object);
        else if constexpr(std::is_same_v<T,EditProperties>)return std::string(c.relative?"Adjust ":"Set ")+std::to_string(c.targets.size())+" properties: "+property_label(c.targets.front());
        else if constexpr(std::is_same_v<T,LinkProperties>)return std::string(c.relative?"Relative link ":"Link ")+std::to_string(c.targets.size())+" properties from "+property_label(c.source);
        else if constexpr(std::is_same_v<T,UnlinkProperties>)return "Unlink "+std::to_string(c.targets.size())+" properties: "+property_label(c.targets.front());
        else if constexpr(std::is_same_v<T,TranslateObjects>)return "Move "+std::to_string(c.objects.size())+" objects: "+name(c.objects.front());
        else if constexpr(std::is_same_v<T,DeleteObjects>)return "Delete "+std::to_string(c.objects.size())+" object(s): "+name(c.objects.front());
        else if constexpr(std::is_same_v<T,ReorderObjects>)return "Reorder objects: "+name(c.parent.empty()?c.composition:c.parent);
        else if constexpr(std::is_same_v<T,AddArtboard>)return "Add Artboard: "+c.artboard.name;
        else if constexpr(std::is_same_v<T,UpdateArtboard>)return "Edit Artboard: "+c.artboard.name;
        else if constexpr(std::is_same_v<T,DeleteArtboard>)return "Delete Artboard: "+name(c.artboard);
        else if constexpr(std::is_same_v<T,ReorderArtboards>)return "Reorder Artboards: "+name(c.composition);
        else if constexpr(std::is_same_v<T,DetachArtboardParent>)return "Detach Artboard size: "+name(c.artboard);
        else if constexpr(std::is_same_v<T,AddPoint>)return "Add point: "+name(c.object);
        else if constexpr(std::is_same_v<T,RemovePoint>)return "Delete point: "+name(c.object);
        else if constexpr(std::is_same_v<T,CloseContour>)return std::string(c.closed?"Close contour: ":"Open contour: ")+name(c.object);
        else if constexpr(std::is_same_v<T,ReorderPoints>)return "Reorder points: "+name(c.object);
        else if constexpr(std::is_same_v<T,EnablePointEdit>)return std::string(c.enabled?"Enable Point Edit: ":"Bypass Point Edit: ")+name(c.object);
        else if constexpr(std::is_same_v<T,ClearPointEdit>)return "Clear Point Edit: "+name(c.object);
        else if constexpr(std::is_same_v<T,ConvertToPath>)return "Convert source to Path: "+name(c.object);
        else if constexpr(std::is_same_v<T,AddOperation>)return "Add "+operation_name(c.operation.type)+": "+name(c.object);
        else if constexpr(std::is_same_v<T,RemoveOperation>)return "Remove operation: "+name(c.object);
        else if constexpr(std::is_same_v<T,ReorderOperations>)return "Reorder operations: "+name(c.object);
        else if constexpr(std::is_same_v<T,EnableOperation>)return std::string(c.enabled?"Enable operation: ":"Bypass operation: ")+name(c.object);
        else if constexpr(std::is_same_v<T,OperationOptions>)return "Edit operation options: "+name(c.object);
        else if constexpr(std::is_same_v<T,SetGradient>)return "Edit Gradient: "+name(c.object);
        else return "Edit document";
    },commands.front());
    if(commands.size()>1)label+=" ("+std::to_string(commands.size())+" commands)";
    return label;
}

std::size_t Session::estimate_history(const HistoryEntry& entry) {
    // Include a full extra list node per entry, conservatively covering its
    // sentinel allocation as well as bookkeeping on the supported STL.
    auto bytes=total(2*sizeof(HistoryEntry),8*sizeof(void*),2*allocation_overhead,extra(entry.label),extra(entry.compositions),extra(entry.collections));
    const auto changes=[&](const auto& items) {
        using Item=typename std::decay_t<decltype(items)>::value_type;
        auto result=items.capacity()?total(multiply(items.capacity(),sizeof(Item)),allocation_overhead):0;
        for(const auto& item:items)result=total(result,extra(item.key),extra(item.before),extra(item.after));
        return result;
    };
    return total(bytes,changes(entry.objects),changes(entry.colors));
}

void Session::commit(Document candidate,std::string label) {
    HistoryEntry entry;entry.id=next_history_id_;entry.label=std::move(label);
    const auto diff=[](const auto& before,const auto& after,auto& changes) {
        auto a=before.begin(),b=after.begin();
        while(a!=before.end()||b!=after.end()) {
            if(b==after.end()||(a!=before.end()&&a->first<b->first)) {changes.push_back({a->first,a->second,{}});++a;}
            else if(a==before.end()||b->first<a->first) {changes.push_back({b->first,{},b->second});++b;}
            else {if(a->second!=b->second)changes.push_back({a->first,a->second,b->second});++a;++b;}
        }
    };
    diff(document_.objects,candidate.objects,entry.objects);diff(document_.named_colors,candidate.named_colors,entry.colors);
    if(document_.compositions!=candidate.compositions)entry.compositions=std::pair{document_.compositions,candidate.compositions};
    if(document_.collections!=candidate.collections)entry.collections=std::pair{document_.collections,candidate.collections};
    entry.estimated_bytes=estimate_history(entry);
    if(entry.estimated_bytes>history_limits_.max_bytes)throw Error("HISTORY_LIMIT","This edit exceeds the estimated history memory budget; no edit was committed");
    if(next_history_id_==std::numeric_limits<std::uint64_t>::max())throw Error("HISTORY_LIMIT","History state ID space exhausted");

    // Allocate before touching the redo branch. Everything after insertion is
    // non-allocating, so admission/allocation failures preserve the old timeline.
    auto redo_start=std::next(history_.begin(),static_cast<std::ptrdiff_t>(history_cursor_));
    const bool has_redo=redo_start!=history_.end();
    history_.push_back(std::move(entry));const auto inserted=std::prev(history_.end());
    if(!has_redo)redo_start=inserted;
    for(auto it=redo_start;it!=inserted;) {history_bytes_-=it->estimated_bytes;it=history_.erase(it);}
    while(history_.size()>history_limits_.max_entries||history_bytes_>history_limits_.max_bytes-inserted->estimated_bytes) {
        boundary_id_=history_.front().id;history_bytes_-=history_.front().estimated_bytes;
        history_.pop_front();++pruned_entries_;
    }
    history_bytes_+=inserted->estimated_bytes;
    history_cursor_=history_.size();document_=std::move(candidate);++next_history_id_;++revision_;
}

void Session::apply_history(Document& candidate,const HistoryEntry& entry,bool forward) {
    const auto patch=[&](auto& objects,const auto& changes) {
        for(const auto& change:changes) {
            const auto& value=forward?change.after:change.before;
            if(value)objects.insert_or_assign(change.key,*value);else objects.erase(change.key);
        }
    };
    patch(candidate.objects,entry.objects);patch(candidate.named_colors,entry.colors);
    if(entry.compositions)candidate.compositions=forward?entry.compositions->second:entry.compositions->first;
    if(entry.collections)candidate.collections=forward?entry.collections->second:entry.collections->first;
}

HistoryInfo Session::history() const {
    HistoryInfo info;info.current_id=boundary_id_;info.retained_bytes=history_bytes_;
    info.max_entries=history_limits_.max_entries;info.max_bytes=history_limits_.max_bytes;info.pruned_entries=pruned_entries_;
    info.states.reserve(history_.size()+1);info.states.push_back({boundary_id_,pruned_entries_?"Earliest retained state":"Initial document",0});
    std::size_t index=0;for(const auto& entry:history_) {
        info.states.push_back({entry.id,entry.label,entry.estimated_bytes});if(++index==history_cursor_)info.current_id=entry.id;
    }
    return info;
}

void Session::restore_history(std::uint64_t state_id,std::uint64_t expected) {
    check_revision(expected);std::size_t target=0;
    if(state_id!=boundary_id_) {
        auto found=std::find_if(history_.begin(),history_.end(),[&](const auto& entry){return entry.id==state_id;});
        if(found==history_.end())throw Error("HISTORY_STATE_NOT_FOUND","The requested history state is no longer retained");
        target=static_cast<std::size_t>(std::distance(history_.begin(),found))+1;
    }
    if(target==history_cursor_)return;
    auto candidate=document_;auto cursor=history_cursor_;
    auto at=std::next(history_.begin(),static_cast<std::ptrdiff_t>(cursor));
    while(cursor>target){--at;apply_history(candidate,*at,false);--cursor;}
    while(cursor<target){apply_history(candidate,*at,true);++at;++cursor;}
    validate(candidate);document_=std::move(candidate);history_cursor_=target;++revision_;
}

void Session::undo(std::uint64_t expected) {
    check_revision(expected);if(!can_undo())throw Error("NO_UNDO","No undo entry");
    const auto id=history_cursor_==1?boundary_id_:std::next(history_.begin(),static_cast<std::ptrdiff_t>(history_cursor_-2))->id;
    restore_history(id,expected);
}
void Session::redo(std::uint64_t expected) {
    check_revision(expected);if(!can_redo())throw Error("NO_REDO","No redo entry");
    restore_history(std::next(history_.begin(),static_cast<std::ptrdiff_t>(history_cursor_))->id,expected);
}
} // namespace nect
