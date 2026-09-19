#include "nect/core.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <type_traits>

namespace nect {
Error::Error(std::string c, const std::string& m) : std::runtime_error(m), code(std::move(c)) {}

namespace {
void require(bool ok, const char* code, const std::string& message) {
    if(!ok) throw Error(code,message);
}
void finite(double n) {
    require(std::isfinite(n),"NON_FINITE","Non-finite numeric value");
}
void identity(const Id& id) {
    require(!id.empty() && id.size()<=96,"INVALID_ID","ID must have 1..96 ASCII identifier characters");
    for(const unsigned char c : id)
        require((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-',"INVALID_ID",id);
}
template<class D>
auto& lookup_property(D& d,const Ref& r) {
    auto it=d.objects.find(r.object);
    require(it!=d.objects.end(),"MISSING_REFERENCE",r.object);
    auto& o=it->second;
    if(r.point.empty()) {
        static const std::array<std::string,6> tf{"a","b","c","d","tx","ty"};
        for(std::size_t i=0;i<tf.size();++i) if(r.field=="transform."+tf[i]) return o.transform[i];
        if(o.kind==Kind::path) {
            static const std::array<std::string,4> ch{"r","g","b","a"};
            for(std::size_t i=0;i<ch.size();++i) if(r.field=="stroke."+ch[i]) return o.color[i];
            if(r.field=="stroke.width") return o.stroke_width;
        }
    } else if(o.kind==Kind::path) {
        for(auto& c:o.contours) for(auto& p:c.points) if(p.id==r.point) {
            if(r.field=="x") return p.x;
            if(r.field=="y") return p.y;
            if(r.field=="in.angle") return p.in_angle;
            if(r.field=="in.length") return p.in_length;
            if(r.field=="out.angle") return p.out_angle;
            if(r.field=="out.length") return p.out_length;
        }
    }
    throw Error("MISSING_REFERENCE",r.object+"/"+r.point+"/"+r.field);
}
std::string unit(const Ref& r) {
    if(r.field=="x"||r.field=="y"||r.field=="transform.tx"||r.field=="transform.ty"||
       r.field=="stroke.width"||r.field.ends_with(".length")) return "du";
    if(r.field.ends_with(".angle")) return "degree";
    return "scalar";
}
void value_range(const Ref& r,double v) {
    finite(v);
    require(std::abs(v)<=1e9,"OUT_OF_RANGE","Magnitude limit is 1e9 in v0.1");
    if(r.field.ends_with(".length")||r.field=="stroke.width")
        require(v>=0,"OUT_OF_RANGE","Negative length");
    if(r.field.starts_with("stroke.")&&r.field!="stroke.width")
        require(v>=0&&v<=1,"OUT_OF_RANGE","sRGB/alpha channel outside [0,1]");
}

std::map<Ref, const Scalar*> property_index(const Document& document) {
    std::map<Ref, const Scalar*> index;
    static const std::array<std::string, 6> transform_fields{
        "transform.a", "transform.b", "transform.c", "transform.d", "transform.tx", "transform.ty"};
    static const std::array<std::string, 4> color_fields{"stroke.r", "stroke.g", "stroke.b", "stroke.a"};

    for (const auto& [id, object] : document.objects) {
        for (std::size_t i = 0; i < transform_fields.size(); ++i)
            index.emplace(Ref{id, "", transform_fields[i]}, &object.transform[i]);

        if (object.kind != Kind::path) continue;

        for (std::size_t i = 0; i < color_fields.size(); ++i)
            index.emplace(Ref{id, "", color_fields[i]}, &object.color[i]);
        index.emplace(Ref{id, "", "stroke.width"}, &object.stroke_width);

        for (const auto& contour : object.contours) {
            for (const auto& point : contour.points) {
                index.emplace(Ref{id, point.id, "x"}, &point.x);
                index.emplace(Ref{id, point.id, "y"}, &point.y);
                index.emplace(Ref{id, point.id, "in.angle"}, &point.in_angle);
                index.emplace(Ref{id, point.id, "in.length"}, &point.in_length);
                index.emplace(Ref{id, point.id, "out.angle"}, &point.out_angle);
                index.emplace(Ref{id, point.id, "out.length"}, &point.out_length);
            }
        }
    }
    return index;
}
}

const Scalar& property(const Document& d,const Ref& r) {
    return lookup_property(d,r);
}

std::vector<Ref> properties(const Document& document) {
    std::vector<Ref> refs;
    for (const auto& [ref, scalar] : property_index(document)) {
        (void)scalar;
        refs.push_back(ref);
    }
    return refs;
}

Ref resolve_name(const Document& d,const std::string& name,const Id& p,const std::string& f) {
    std::vector<Id> matches;
    for(const auto& [id,o]:d.objects) if(o.name==name) matches.push_back(id);
    require(!matches.empty(),"MISSING_NAME","No matching object: "+name);
    require(matches.size()==1,"AMBIGUOUS_NAME","Name must resolve to exactly one object: "+name);
    Ref r{matches.front(),p,f};
    (void)property(d,r);
    return r;
}

std::map<Ref,double> evaluate(const Document& d) {
    const auto index = property_index(d);
    std::map<Ref,double> values;
    std::set<Ref> active;
    std::function<double(const Ref&,unsigned)> visit=[&](const Ref& r,unsigned depth)->double {
        require(depth<=128,"DEPENDENCY_DEPTH","v0.1 dependency depth limit 128");
        if(auto i=values.find(r);i!=values.end()) return i->second;
        require(active.insert(r).second,"DEPENDENCY_CYCLE","Property dependency cycle");
        const auto found = index.find(r);
        require(found != index.end(), "MISSING_REFERENCE", r.object + "/" + r.point + "/" + r.field);
        const auto& p = *found->second;
        double v=p.literal;
        if(p.binding) {
            const auto& b=*p.binding;
            require(b.mode=="copy_local_value","UNSUPPORTED_BINDING","Explicit copy_local_value binding required");
            finite(b.scale);
            finite(b.offset);
            require(unit(r)==unit(b.source),"UNIT_MISMATCH","Implicit unit conversion is not supported");
            v=visit(b.source,depth+1)*b.scale+b.offset;
        }
        value_range(r,v);
        active.erase(r);
        values.emplace(r,v);
        return v;
    };
    for (const auto& [ref, scalar] : index) {
        (void)scalar;
        visit(ref, 0);
    }
    return values;
}

void validate(const Document& d) {
    require(d.objects.size()<=10000 && d.compositions.size()<=128,"LIMIT","Document size limit");
    std::set<Id> ids;
    auto add=[&](const Id& id) {
        identity(id);
        require(ids.insert(id).second,"DUPLICATE_ID",id);
    };

    add(d.id);
    for(const auto& comp:d.compositions) {
        add(comp.id);
        for(const auto& a:comp.artboards) {
            add(a.id);
            finite(a.x); finite(a.y); finite(a.width); finite(a.height);
            require(a.width>0&&a.height>0&&a.width<=1e7&&a.height<=1e7,"INVALID_ARTBOARD",a.id);
        }
    }

    std::size_t point_count=0;
    for(const auto& [id,o]:d.objects) {
        add(id);
        require(id==o.id,"ID_MISMATCH",id);
        require(o.name.size()<=4096,"LIMIT","Object name too long");
        for(unsigned char ch:o.name)
            require(ch>=32||ch==9||ch==10||ch==13,"INVALID_NAME","XML-incompatible control character");

        if(o.kind==Kind::group) {
            require(o.contours.empty(),"INVALID_OBJECT","Group cannot own path geometry");
        } else {
            require(o.children.empty(),"INVALID_OBJECT","Path cannot own children");
            for(const auto& c:o.contours) {
                add(c.id);
                require(!c.points.empty(),"INVALID_PATH","Contour needs at least one point");
                for(const auto& p:c.points) {
                    add(p.id);
                    ++point_count;
                }
            }
        }
    }
    require(point_count<=50000,"LIMIT","Point limit 50000");

    std::set<Id> owned;
    std::function<void(const Id&,unsigned)> own=[&](const Id& id,unsigned depth) {
        require(depth<=128,"HIERARCHY_DEPTH","Hierarchy depth limit 128");
        require(d.objects.contains(id),"MISSING_OBJECT",id);
        require(owned.insert(id).second,"INVALID_HIERARCHY","Repeated owner or cycle: "+id);
        for(const auto& child:d.objects.at(id).children) own(child,depth+1);
    };
    for(const auto& comp:d.compositions) for(const auto& id:comp.roots) own(id,0);
    require(owned.size()==d.objects.size(),"ORPHAN_OBJECT","Every object requires exactly one composition/tree owner");

    for(const auto& c:d.collections) {
        add(c.id);
        std::set<Id> members;
        for(const auto& m:c.members) {
            require(d.objects.contains(m),"MISSING_OBJECT",m);
            require(members.insert(m).second,"DUPLICATE_MEMBER",m);
        }
    }

    for (const auto& [ref, scalar] : property_index(d))
        value_range(ref, scalar->literal);

    (void)evaluate(d);
}

Session::Session(Document d):document_(std::move(d)) {
    validate(document_);
}

void Session::check_revision(std::uint64_t expected) const {
    require(expected==revision_,"REVISION_CONFLICT","Refresh revision before editing");
}

void Session::apply(const std::vector<Command>& commands,std::uint64_t expected) {
    check_revision(expected);
    require(!commands.empty()&&commands.size()<=1000,"INVALID_BATCH","Batch must have 1..1000 commands");

    auto candidate=document_;
    for(const auto& command:commands) std::visit([&](const auto& c) {
        using T=std::decay_t<decltype(c)>;
        if constexpr(std::is_same_v<T,Set>) {
            auto& p=lookup_property(candidate,c.ref);
            require(!p.binding,"DRIVEN_PROPERTY","Unlink explicitly before setting a driven property");
            p.literal=c.value;
        } else if constexpr(std::is_same_v<T,Link>) {
            lookup_property(candidate,c.target).binding=c.binding;
        } else if constexpr(std::is_same_v<T,Unlink>) {
            const auto value=evaluate(candidate).at(c.target);
            lookup_property(candidate,c.target)={value,{}};
        } else if constexpr(std::is_same_v<T,Rename>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            candidate.objects.at(c.object).name=c.name;
        } else if constexpr(std::is_same_v<T,ReorderPoints>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& contours=candidate.objects.at(c.object).contours;
            auto it=std::find_if(contours.begin(),contours.end(),[&](const auto& x){return x.id==c.contour;});
            require(it!=contours.end(),"MISSING_CONTOUR",c.contour);
            std::map<Id,Point> old;
            for(const auto& p:it->points) old.emplace(p.id,p);
            require(c.order.size()==old.size(),"INVALID_ORDER","Point reorder must be a permutation");
            std::vector<Point> reordered;
            for(const auto& id:c.order) {
                require(old.contains(id),"INVALID_ORDER",id);
                reordered.push_back(old.at(id));
                old.erase(id);
            }
            it->points=std::move(reordered);
        } else if constexpr(std::is_same_v<T,GroupContiguous>) {
            require(!c.members.empty()&&!candidate.objects.contains(c.id),"INVALID_GROUP","New group ID and members required");
            auto comp=std::find_if(candidate.compositions.begin(),candidate.compositions.end(),
                [&](const auto& x){return x.id==c.composition;});
            require(comp!=candidate.compositions.end(),"MISSING_COMPOSITION",c.composition);

            auto* siblings=&comp->roots;
            if(!c.parent.empty()) {
                std::set<Id> descendants;
                std::function<void(const Id&)> collect=[&](const Id& id) {
                    descendants.insert(id);
                    for(const auto& x:candidate.objects.at(id).children) collect(x);
                };
                for(const auto& id:comp->roots) collect(id);
                require(descendants.contains(c.parent)&&candidate.objects.at(c.parent).kind==Kind::group,
                        "INVALID_PARENT",c.parent);
                siblings=&candidate.objects.at(c.parent).children;
            }

            auto start=std::search(siblings->begin(),siblings->end(),c.members.begin(),c.members.end());
            require(start!=siblings->end(),"NONCONTIGUOUS_GROUP",
                    "Only ordered contiguous siblings can be grouped without changing stacking");
            auto index=std::distance(siblings->begin(),start);
            siblings->erase(start,start+static_cast<std::ptrdiff_t>(c.members.size()));
            siblings->insert(siblings->begin()+index,c.id);

            Object group;
            group.id=c.id;
            group.name=c.name;
            group.kind=Kind::group;
            group.children=c.members;
            candidate.objects.emplace(group.id,std::move(group));
        }
    },command);

    validate(candidate);

    undo_.push_back(document_);
    if (undo_.size() > 64) undo_.erase(undo_.begin());
    document_ = std::move(candidate);
    redo_.clear();
    ++revision_;
}

void Session::undo(std::uint64_t expected) {
    check_revision(expected);
    require(!undo_.empty(),"NO_UNDO","No undo entry");
    redo_.push_back(document_);
    document_=std::move(undo_.back());
    undo_.pop_back();
    ++revision_;
}

void Session::redo(std::uint64_t expected) {
    check_revision(expected);
    require(!redo_.empty(),"NO_REDO","No redo entry");
    undo_.push_back(document_);
    document_=std::move(redo_.back());
    redo_.pop_back();
    ++revision_;
}

Document demo_document() {
    Document d;
    d.id="document-demo";

    Composition comp;
    comp.id="comp-main";
    comp.name="Main";
    comp.roots={"path-A","path-B"};
    comp.artboards={{"art-main","Canvas",0,0,640,480}};
    d.compositions.push_back(comp);

    for(const auto* suffix:{"A","B"}) {
        std::string s=suffix;
        Object o;
        o.id="path-"+s;
        o.name="Curve "+s;

        Point a;
        a.id="point-"+s+"1";
        a.x.literal=100;
        a.y.literal=s=="A"?150:300;
        a.out_length.literal=90;
        a.out_angle.literal=-45;

        Point b;
        b.id="point-"+s+"2";
        b.x.literal=500;
        b.y.literal=a.y.literal;
        b.in_length.literal=90;
        b.in_angle.literal=135;

        o.contours={{"contour-"+s,false,{a,b}}};
        d.objects.emplace(o.id,o);
    }

    validate(d);
    return d;
}
}
