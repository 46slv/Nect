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
const std::array<std::string,6> point_fields{"x","y","in.angle","in.length","out.angle","out.length"};
std::array<std::string,4> source_roles(const Primitive& source) {
    require(source.version==1,"UNSUPPORTED_OPERATOR_VERSION","Unsupported primitive behavior version");
    if(source.type=="nect.shape.circle")return {"east","south","west","north"};
    if(source.type=="nect.shape.rectangle")return {"top-left","top-right","bottom-right","bottom-left"};
    throw Error("UNSUPPORTED_OPERATOR",source.type);
}
bool generated_point(const Object& o,const Id& point) {
    if(!o.source||point.empty())return false;
    for(const auto& role:source_roles(*o.source))if(point==o.source->id+"-"+role)return true;
    return false;
}
void prepare_point_edit(Document& d,const Ref& ref) {
    auto it=d.objects.find(ref.object);
    if(it==d.objects.end()||!it->second.source||ref.point.empty())return;
    auto& o=it->second;
    require(generated_point(o,ref.point)&&std::find(point_fields.begin(),point_fields.end(),ref.field)!=point_fields.end(),
        "MISSING_REFERENCE",ref.point+"/"+ref.field);
    if(!o.point_edit)o.point_edit=PointEdit{o.source->id+"-point-edit",1,true,{}};
    o.point_edit->enabled=true;
    o.point_edit->overrides[ref.point].try_emplace(ref.field,Scalar{});
}
template<class D>
auto& lookup_property(D& d,const Ref& r) {
    auto it=d.objects.find(r.object);
    require(it!=d.objects.end(),"MISSING_REFERENCE",r.object);
    auto& o=it->second;
    if(r.point.empty()) {
        if(o.source&&r.field.starts_with("generator.")) {
            const auto name=r.field.substr(10);
            require(o.source->parameters.contains(name),"MISSING_REFERENCE",r.field);
            return o.source->parameters.at(name);
        }
        static const std::array<std::string,6> tf{"a","b","c","d","tx","ty"};
        for(std::size_t i=0;i<tf.size();++i) if(r.field=="transform."+tf[i]) return o.transform[i];
        if(o.kind==Kind::path) {
            static const std::array<std::string,4> ch{"r","g","b","a"};
            for(std::size_t i=0;i<ch.size();++i) if(r.field=="stroke."+ch[i]) return o.color[i];
            if(r.field=="stroke.width") return o.stroke_width;
        }
    } else if(o.kind==Kind::path) {
        if(o.source&&generated_point(o,r.point)&&o.point_edit) {
            auto p=o.point_edit->overrides.find(r.point);
            if(p!=o.point_edit->overrides.end()) {
                auto f=p->second.find(r.field);
                if(f!=p->second.end())return f->second;
            }
        }
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
    if(r.field.starts_with("generator.")||r.field=="x"||r.field=="y"||r.field=="transform.tx"||r.field=="transform.ty"||
       r.field=="stroke.width"||r.field.ends_with(".length")) return "du";
    if(r.field.ends_with(".angle")) return "degree";
    return "scalar";
}
void value_range(const Ref& r,double v) {
    finite(v);
    require(std::abs(v)<=1e9,"OUT_OF_RANGE","Magnitude limit is 1e9 in v0.1");
    if(r.field.ends_with(".length")||r.field=="stroke.width"||r.field=="generator.radius"||
       r.field=="generator.width"||r.field=="generator.height")
        require(v>=0,"OUT_OF_RANGE","Negative length");
    if(r.field.starts_with("stroke.")&&r.field!="stroke.width")
        require(v>=0&&v<=1,"OUT_OF_RANGE","sRGB/alpha channel outside [0,1]");
}

std::map<Ref, const Scalar*> property_index(const Document& document,bool include_disabled=false) {
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

        if(object.source) {
            for(const auto& [name,value]:object.source->parameters)index.emplace(Ref{id,"","generator."+name},&value);
            for(const auto& role:source_roles(*object.source)) {
                const auto point=object.source->id+"-"+role;
                for(const auto& field:point_fields) {
                    const Scalar* scalar=nullptr;
                    if(object.point_edit&&(object.point_edit->enabled||include_disabled)) {
                        const auto p=object.point_edit->overrides.find(point);
                        if(p!=object.point_edit->overrides.end()) {
                            const auto f=p->second.find(field);
                            if(f!=p->second.end())scalar=&f->second;
                        }
                    }
                    index.emplace(Ref{id,point,field},scalar);
                }
            }
        }

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

Scalar property(const Document& d,const Ref& r) {
    const auto object=d.objects.find(r.object);
    if(object!=d.objects.end()&&generated_point(object->second,r.point)) {
        if(property_origin(d,r)!="generated")return lookup_property(d,r);
        return {evaluate(d).at(r),{}};
    }
    return lookup_property(d,r);
}

std::string property_origin(const Document& d,const Ref& r) {
    const auto object=d.objects.find(r.object);
    if(object==d.objects.end()||!generated_point(object->second,r.point))return "authored";
    require(std::find(point_fields.begin(),point_fields.end(),r.field)!=point_fields.end(),"MISSING_REFERENCE",r.field);
    const auto& edit=object->second.point_edit;
    if(!edit)return "generated";
    const auto point=edit->overrides.find(r.point);
    if(point==edit->overrides.end()||!point->second.contains(r.field))return "generated";
    return edit->enabled?"point_edit":"bypassed_point_edit";
}

std::vector<Contour> path_contours(const Object& o) {
    if(!o.source)return o.contours;
    Contour c{o.source->id+"-contour",true,{}};
    for(const auto& role:source_roles(*o.source)) {Point p;p.id=o.source->id+"-"+role;c.points.push_back(p);}
    return {c};
}

std::vector<Ref> conversion_blockers(const Document& d,const Id& object) {
    require(d.objects.contains(object),"MISSING_OBJECT",object);
    const auto& o=d.objects.at(object);
    require(o.source.has_value(),"NOT_PRIMITIVE","Select a parametric primitive");
    std::vector<Ref> blockers;
    for(const auto& [ref,scalar]:property_index(d,true)) {
        if(!scalar||!scalar->binding)continue;
        if(ref.object==object&&ref.field.starts_with("generator."))continue;
        if(ref.object==object&&!ref.point.empty()&&o.point_edit&&!o.point_edit->enabled)continue;
        const auto& source=scalar->binding->source;
        if(source.object==object&&source.point.empty()&&source.field.starts_with("generator."))blockers.push_back(ref);
    }
    return blockers;
}

std::string property_unit(const Ref& r) { return unit(r); }

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
        const auto* p = found->second;
        double v=p?p->literal:0;
        if(!p) {
            const auto& o=d.objects.at(r.object);
            const auto& s=*o.source;
            const auto roles=source_roles(s);
            const auto role=r.point.substr(s.id.size()+1);
            const auto param=[&](const char* name){return visit({r.object,"",std::string("generator.")+name},depth+1);};
            if(s.type=="nect.shape.circle") {
                const auto position=std::find(roles.begin(),roles.end(),role)-roles.begin();
                if(r.field=="x") {v=param("center_x");if(position==0)v+=param("radius");else if(position==2)v-=param("radius");}
                else if(r.field=="y") {v=param("center_y");if(position==1)v+=param("radius");else if(position==3)v-=param("radius");}
                else if(r.field.ends_with(".length"))v=param("radius")*0.5522847498307936;
                else if(r.field=="in.angle")v=static_cast<double>(position)*90-90;
                else if(r.field=="out.angle")v=static_cast<double>(position)*90+90;
            } else {
                if(r.field=="x")v=param("center_x")+param("width")*(role=="top-left"||role=="bottom-left"?-0.5:0.5);
                else if(r.field=="y")v=param("center_y")+param("height")*(role=="top-left"||role=="top-right"?-0.5:0.5);
            }
        } else if(p->binding) {
            const auto& b=*p->binding;
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
            require(!o.source&&!o.point_edit,"INVALID_OBJECT","Group cannot own a primitive or Point Edit");
        } else {
            require(o.children.empty(),"INVALID_OBJECT","Path cannot own children");
            if(o.source) {
                const auto& s=*o.source;
                require(o.contours.empty(),"INVALID_OBJECT","Primitive owns a generator, not a second authored contour list");
                require(s.id.size()<=64,"INVALID_ID","Generator ID is limited to 64 characters for stable role identities");
                add(s.id);
                add(s.id+"-point-edit"); // reserved correction instance namespace
                (void)source_roles(s);
                const std::set<std::string> expected=s.type=="nect.shape.circle"?
                    std::set<std::string>{"center_x","center_y","radius"}:
                    std::set<std::string>{"center_x","center_y","width","height"};
                std::set<std::string> actual;for(const auto& [name,value]:s.parameters){(void)value;actual.insert(name);}
                require(actual==expected,"INVALID_GENERATOR_PARAMETERS","Missing or unsupported primitive parameters");
                if(o.point_edit) {
                    const auto& edit=*o.point_edit;
                    require(edit.id==s.id+"-point-edit","INVALID_POINT_EDIT","Correction instance must retain its source identity");
                    require(edit.version==1,"UNSUPPORTED_OPERATOR_VERSION","Unsupported Point Edit version");
                    for(const auto& [point,fields]:edit.overrides) {
                        require(generated_point(o,point)&&!fields.empty(),"UNRESOLVED_POINT_EDIT",point);
                        for(const auto& [field,value]:fields) {
                            (void)value;
                            require(std::find(point_fields.begin(),point_fields.end(),field)!=point_fields.end(),"UNKNOWN_FIELD",field);
                        }
                    }
                }
            } else require(!o.point_edit,"INVALID_POINT_EDIT","Point Edit needs its retained generator");
            for(const auto& c:path_contours(o)) {
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

    const auto authored=property_index(d,true);
    for (const auto& [ref, scalar] : authored) {
        if(!scalar)continue;
        value_range(ref, scalar->literal);
        if(scalar->binding) {
            const auto& binding=*scalar->binding;
            require(authored.contains(binding.source),"MISSING_REFERENCE",binding.source.object+"/"+binding.source.field);
            require(unit(ref)==unit(binding.source),"UNIT_MISMATCH","Implicit unit conversion is not supported");
            require(binding.mode=="copy_local_value","UNSUPPORTED_BINDING","Explicit copy_local_value binding required");
            finite(binding.scale);finite(binding.offset);
        }
    }

    (void)evaluate(d);
}

Session::Session(Document d):document_(std::move(d)) {
    validate(document_);
}

void Session::check_revision(std::uint64_t expected) const {
    require(expected==revision_,"REVISION_CONFLICT","Refresh revision before editing");
    require(!gesture_active(),"GESTURE_ACTIVE","Finish or cancel the current gesture before another edit");
}

namespace {
std::vector<Id>& siblings(Document& d, const Id& composition, const Id& parent) {
    auto comp=std::find_if(d.compositions.begin(),d.compositions.end(),
        [&](const auto& c){return c.id==composition;});
    require(comp!=d.compositions.end(),"MISSING_COMPOSITION",composition);
    if(parent.empty()) return comp->roots;
    bool found=false;
    std::function<void(const Id&)> visit=[&](const Id& id) {
        if(id==parent) found=true;
        for(const auto& child:d.objects.at(id).children) visit(child);
    };
    for(const auto& root:comp->roots) visit(root);
    require(found&&d.objects.at(parent).kind==Kind::group,"INVALID_PARENT",parent);
    return d.objects.at(parent).children;
}
Contour& contour(Document& d,const Id& object,const Id& id) {
    require(d.objects.contains(object),"MISSING_OBJECT",object);
    require(!d.objects.at(object).source,"GENERATED_TOPOLOGY","Convert to Path explicitly before changing generator topology");
    auto& list=d.objects.at(object).contours;
    auto it=std::find_if(list.begin(),list.end(),[&](const auto& c){return c.id==id;});
    require(it!=list.end(),"MISSING_CONTOUR",id);
    return *it;
}
Document edited(const Document& document,const std::vector<Command>& commands) {
    require(!commands.empty()&&commands.size()<=1000,"INVALID_BATCH","Batch must have 1..1000 commands");

    auto candidate=document;
    for(const auto& command:commands) std::visit([&](const auto& c) {
        using T=std::decay_t<decltype(c)>;
        if constexpr(std::is_same_v<T,Set>) {
            prepare_point_edit(candidate,c.ref);
            auto& p=lookup_property(candidate,c.ref);
            require(!p.binding,"DRIVEN_PROPERTY","Unlink explicitly before setting a driven property");
            p.literal=c.value;
        } else if constexpr(std::is_same_v<T,Link>) {
            prepare_point_edit(candidate,c.target);
            lookup_property(candidate,c.target).binding=c.binding;
        } else if constexpr(std::is_same_v<T,Unlink>) {
            const auto value=evaluate(candidate).at(c.target);
            prepare_point_edit(candidate,c.target);
            lookup_property(candidate,c.target)={value,{}};
        } else if constexpr(std::is_same_v<T,Rename>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            candidate.objects.at(c.object).name=c.name;
        } else if constexpr(std::is_same_v<T,ReorderPoints>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            require(!candidate.objects.at(c.object).source,"GENERATED_TOPOLOGY","Convert to Path explicitly before changing generator topology");
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
        } else if constexpr(std::is_same_v<T,CreatePrimitive>) {
            require(!candidate.objects.contains(c.id),"DUPLICATE_ID",c.id);
            Object object;object.id=c.id;object.name=c.name;object.source=c.source;
            siblings(candidate,c.composition,c.parent).push_back(c.id);
            candidate.objects.emplace(c.id,std::move(object));
        } else if constexpr(std::is_same_v<T,EnablePointEdit>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& o=candidate.objects.at(c.object);
            require(o.point_edit.has_value(),"NO_POINT_EDIT","Primitive has no authored point corrections");
            o.point_edit->enabled=c.enabled;
        } else if constexpr(std::is_same_v<T,ConvertToPath>) {
            require(conversion_blockers(candidate,c.object).empty(),"CONVERSION_REFERENCE",
                "Generator properties are referenced; explicitly unlink/freeze dependent targets before conversion");
            auto& o=candidate.objects.at(c.object);
            const auto values=evaluate(candidate);
            const auto authored=property_index(candidate,true);
            auto contours=path_contours(o);
            for(auto& ct:contours)for(auto& p:ct.points) {
                const std::array<Scalar*,6> fields{&p.x,&p.y,&p.in_angle,&p.in_length,&p.out_angle,&p.out_length};
                for(std::size_t i=0;i<fields.size();++i) {
                    const Ref ref{o.id,p.id,point_fields[i]};
                    const auto* override_value=authored.at(ref);
                    *fields[i]=o.point_edit&&o.point_edit->enabled&&override_value?*override_value:Scalar{values.at(ref),{}};
                }
            }
            o.contours=std::move(contours);o.source.reset();o.point_edit.reset();
        } else if constexpr(std::is_same_v<T,CreatePath>) {
            require(!candidate.objects.contains(c.id),"DUPLICATE_ID",c.id);
            require(!c.contours.empty(),"INVALID_PATH","Create Path needs a contour");
            Object object;
            object.id=c.id; object.name=c.name; object.contours=c.contours;
            siblings(candidate,c.composition,c.parent).push_back(c.id);
            candidate.objects.emplace(c.id,std::move(object));
        } else if constexpr(std::is_same_v<T,AddPoint>) {
            contour(candidate,c.object,c.contour).points.push_back(c.point);
        } else if constexpr(std::is_same_v<T,RemovePoint>) {
            auto& points=contour(candidate,c.object,c.contour).points;
            auto it=std::find_if(points.begin(),points.end(),[&](const auto& p){return p.id==c.point;});
            require(it!=points.end(),"MISSING_REFERENCE",c.point);
            require(points.size()>1,"INVALID_PATH","Delete the path to remove its last point");
            points.erase(it);
        } else if constexpr(std::is_same_v<T,CloseContour>) {
            contour(candidate,c.object,c.contour).closed=c.closed;
        } else if constexpr(std::is_same_v<T,ReorderObjects>) {
            auto& list=siblings(candidate,c.composition,c.parent);
            auto a=list,b=c.order;
            std::sort(a.begin(),a.end()); std::sort(b.begin(),b.end());
            require(a==b,"INVALID_ORDER","Object reorder must be a sibling permutation");
            list=c.order;
        } else if constexpr(std::is_same_v<T,DeleteObjects>) {
            require(!c.objects.empty(),"INVALID_BATCH","Select objects to delete");
            std::set<Id> removed;
            std::function<void(const Id&)> remove=[&](const Id& id) {
                require(candidate.objects.contains(id),"MISSING_OBJECT",id);
                if(!removed.insert(id).second) return;
                for(const auto& child:candidate.objects.at(id).children) remove(child);
            };
            for(const auto& id:c.objects) remove(id);
            auto prune=[&](std::vector<Id>& list) {
                std::erase_if(list,[&](const Id& id){return removed.contains(id);});
            };
            for(auto& comp:candidate.compositions) prune(comp.roots);
            for(auto& [id,object]:candidate.objects) { (void)id; prune(object.children); }
            for(auto& collection:candidate.collections) prune(collection.members);
            for(const auto& id:removed) candidate.objects.erase(id);
            // Validation rejects surviving references to deleted properties; callers
            // may explicitly unlink/freeze those targets in this same atomic batch.
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
    return candidate;
}
}

void Session::apply(const std::vector<Command>& commands,std::uint64_t expected) {
    check_revision(expected);
    commit(edited(document_,commands));
}

void Session::commit(Document candidate) {
    undo_.push_back(document_);
    if (undo_.size() > 64) undo_.erase(undo_.begin());
    document_ = std::move(candidate);
    redo_.clear();
    ++revision_;
}

void Session::begin_gesture(std::uint64_t expected) {
    check_revision(expected);
    preview_=document_;
    preview_changed_=false;
}

void Session::update_gesture(const std::vector<Command>& commands) {
    require(gesture_active(),"NO_GESTURE","No active gesture");
    if(commands.empty()) { preview_=document_; preview_changed_=false; return; }
    auto next=edited(document_,commands);
    preview_=std::move(next);
    preview_changed_=true;
}

void Session::commit_gesture() {
    require(gesture_active(),"NO_GESTURE","No active gesture");
    if(preview_changed_) commit(std::move(*preview_));
    preview_.reset();
    preview_changed_=false;
}

void Session::cancel_gesture() {
    preview_.reset();
    preview_changed_=false;
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

Document empty_document(Id document,Id composition,Id artboard) {
    Document d;
    d.id=std::move(document);
    d.compositions.push_back({std::move(composition),"Composition",{},
        {{std::move(artboard),"Artboard 1",0,0,960,640}}});
    validate(d);
    return d;
}
}
