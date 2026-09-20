#include "nect/io.hpp"
#include <boost/json.hpp>
#include <boost/json/basic_parser_impl.hpp>
#include <set>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <functional>
#include <limits>
#include <locale>
#include <numbers>
#include <sstream>

namespace nect {
namespace j=boost::json;

namespace {
void keys(const j::object& o,std::initializer_list<std::string_view> allowed) {
    for(const auto& p:o) {
        auto key=std::string_view(p.key().data(),p.key().size());
        if(std::find(allowed.begin(),allowed.end(),key)==allowed.end())
            throw Error("UNKNOWN_FIELD",std::string(key));
    }
}
std::string text(const j::value& v) {
    auto& s=v.as_string();
    return {s.data(),s.size()};
}
double number(const j::value& v) {
    return j::value_to<double>(v);
}
Ref read_ref(const j::value& v) {
    const auto& o=v.as_object();
    keys(o,{"object","point","field"});
    return {text(o.at("object")),text(o.at("point")),text(o.at("field"))};
}
j::object ref_json(const Ref& r) {
    return {{"object",r.object},{"point",r.point},{"field",r.field}};
}
Binding read_binding(const j::value& v) {
    const auto& o=v.as_object();
    keys(o,{"source","scale","offset","mode"});
    return {read_ref(o.at("source")),number(o.at("scale")),number(o.at("offset")),text(o.at("mode"))};
}
Scalar read_scalar(const j::value& v) {
    const auto& o=v.as_object();
    keys(o,{"literal","binding"});
    Scalar s{number(o.at("literal")),{}};
    if(auto* b=o.if_contains("binding");b&&!b->is_null()) s.binding=read_binding(*b);
    return s;
}
j::value scalar_json(const Scalar& s) {
    j::object o{{"literal",s.literal}};
    if(s.binding) {
        const auto& b=*s.binding;
        o["binding"]=j::object{
            {"source",ref_json(b.source)},{"scale",b.scale},{"offset",b.offset},{"mode",b.mode}};
    }
    return o;
}
std::vector<Id> ids(const j::value& v) {
    std::vector<Id> out;
    for(const auto& x:v.as_array()) out.push_back(text(x));
    return out;
}
j::array ids_json(const std::vector<Id>& ids_) {
    j::array a;
    for(const auto& x:ids_) a.push_back(j::value(x));
    return a;
}
std::string escape(const std::string& s) {
    std::string out;
    for(char c:s) switch(c) {
        case '&':out+="&amp;";break;
        case '<':out+="&lt;";break;
        case '>':out+="&gt;";break;
        case '"':out+="&quot;";break;
        default:out+=c;
    }
    return out;
}

struct UniqueKeys {
    static constexpr std::size_t max_array_size=1000000, max_object_size=1000000;
    static constexpr std::size_t max_key_size=4096, max_string_size=8*1024*1024;
    std::vector<std::set<std::string>> objects;
    std::string key;

    bool on_document_begin(j::error_code&) { return true; }
    bool on_document_end(j::error_code&) { return true; }
    bool on_object_begin(j::error_code&) { objects.emplace_back(); return true; }
    bool on_object_end(std::size_t,j::error_code&) { objects.pop_back(); return true; }
    bool on_array_begin(j::error_code&) { return true; }
    bool on_array_end(std::size_t,j::error_code&) { return true; }
    bool on_key_part(j::string_view s,std::size_t,j::error_code&) {
        key.append(s.data(),s.size());
        return true;
    }
    bool on_key(j::string_view s,std::size_t,j::error_code&) {
        key.append(s.data(),s.size());
        if(!objects.back().insert(key).second) throw Error("DUPLICATE_KEY",key);
        key.clear();
        return true;
    }
    bool on_string_part(j::string_view,std::size_t,j::error_code&) { return true; }
    bool on_string(j::string_view,std::size_t,j::error_code&) { return true; }
    bool on_number_part(j::string_view,j::error_code&) { return true; }
    bool on_int64(std::int64_t,j::string_view,j::error_code&) { return true; }
    bool on_uint64(std::uint64_t,j::string_view,j::error_code&) { return true; }
    bool on_double(double,j::string_view,j::error_code&) { return true; }
    bool on_bool(bool,j::error_code&) { return true; }
    bool on_null(j::error_code&) { return true; }
    bool on_comment_part(j::string_view,j::error_code&) { return true; }
    bool on_comment(j::string_view,j::error_code&) { return true; }
};

j::value parse(std::string_view s) {
    if(s.size()>8*1024*1024) throw Error("INPUT_LIMIT","Input exceeds 8 MiB");
    j::parse_options options;
    options.max_depth=64;

    j::basic_parser<UniqueKeys> unique(options);
    j::error_code ec;
    unique.write_some(false,s.data(),s.size(),ec);
    if(ec) throw Error("INVALID_JSON",ec.message());

    return j::parse(s,{},options);
}

Point read_point(const j::value& v) {
    const auto& p=v.as_object();
    keys(p,{"id","x","y","in_angle","in_length","out_angle","out_length"});
    return {text(p.at("id")),read_scalar(p.at("x")),read_scalar(p.at("y")),
        read_scalar(p.at("in_angle")),read_scalar(p.at("in_length")),
        read_scalar(p.at("out_angle")),read_scalar(p.at("out_length"))};
}
Contour read_contour(const j::value& v) {
    const auto& c=v.as_object();
    keys(c,{"id","closed","points"});
    Contour out{text(c.at("id")),c.at("closed").as_bool(),{}};
    for(const auto& p:c.at("points").as_array()) out.points.push_back(read_point(p));
    return out;
}

Command read_command(const j::value& v) {
    auto& o=v.as_object();
    auto type=text(o.at("type"));
    if(type=="create_path") {
        keys(o,{"type","composition","parent","id","name","contours"});
        std::vector<Contour> contours;
        for(const auto& c:o.at("contours").as_array()) contours.push_back(read_contour(c));
        return CreatePath{text(o.at("composition")),text(o.at("parent")),text(o.at("id")),
            text(o.at("name")),std::move(contours)};
    }
    if(type=="add_point") {
        keys(o,{"type","object","contour","point"});
        return AddPoint{text(o.at("object")),text(o.at("contour")),read_point(o.at("point"))};
    }
    if(type=="remove_point") {
        keys(o,{"type","object","contour","point"});
        return RemovePoint{text(o.at("object")),text(o.at("contour")),text(o.at("point"))};
    }
    if(type=="close_contour") {
        keys(o,{"type","object","contour","closed"});
        return CloseContour{text(o.at("object")),text(o.at("contour")),o.at("closed").as_bool()};
    }
    if(type=="delete_objects") {
        keys(o,{"type","objects"});
        return DeleteObjects{ids(o.at("objects"))};
    }
    if(type=="reorder_objects") {
        keys(o,{"type","composition","parent","order"});
        return ReorderObjects{text(o.at("composition")),text(o.at("parent")),ids(o.at("order"))};
    }
    if(type=="set") {
        keys(o,{"type","ref","value"});
        return Set{read_ref(o.at("ref")),number(o.at("value"))};
    }
    if(type=="link") {
        keys(o,{"type","target","binding"});
        return Link{read_ref(o.at("target")),read_binding(o.at("binding"))};
    }
    if(type=="unlink") {
        keys(o,{"type","target"});
        return Unlink{read_ref(o.at("target"))};
    }
    if(type=="rename") {
        keys(o,{"type","object","name"});
        return Rename{text(o.at("object")),text(o.at("name"))};
    }
    if(type=="reorder_points") {
        keys(o,{"type","object","contour","order"});
        return ReorderPoints{text(o.at("object")),text(o.at("contour")),ids(o.at("order"))};
    }
    if(type=="group_contiguous") {
        keys(o,{"type","composition","parent","members","id","name"});
        return GroupContiguous{
            text(o.at("composition")),text(o.at("parent")),ids(o.at("members")),
            text(o.at("id")),text(o.at("name"))};
    }
    throw Error("UNSUPPORTED_COMMAND",type);
}
}

void validate_json(std::string_view input) { (void)parse(input); }

Document decode(std::string_view input) {
    try {
        auto parsed=parse(input);
        const auto& root=parsed.as_object();
        keys(root,{"format","version","id","units","color_space","compositions","objects","collections"});

        if(text(root.at("format"))!="nect-native" || text(root.at("version"))!="0.1")
            throw Error("UNSUPPORTED_FORMAT","Only nect-native 0.1 is supported");
        if(text(root.at("units"))!="du96"||text(root.at("color_space"))!="srgb")
            throw Error("UNSUPPORTED_COLOR_OR_UNIT","v0.1 supports du96 and sRGB only");

        Document d;
        d.id=text(root.at("id"));

        for(const auto& cv:root.at("compositions").as_array()) {
            auto& co=cv.as_object();
            keys(co,{"id","name","roots","artboards"});
            Composition c;
            c.id=text(co.at("id"));
            c.name=text(co.at("name"));
            c.roots=ids(co.at("roots"));

            for(const auto& av:co.at("artboards").as_array()) {
                auto& a=av.as_object();
                keys(a,{"id","name","x","y","width","height"});
                c.artboards.push_back({
                    text(a.at("id")),text(a.at("name")),number(a.at("x")),number(a.at("y")),
                    number(a.at("width")),number(a.at("height"))});
            }
            d.compositions.push_back(std::move(c));
        }

        for(const auto& ov:root.at("objects").as_array()) {
            auto& o=ov.as_object();
            keys(o,{"id","name","kind","transform","children","contours","stroke","fill"});

            Object obj;
            obj.id=text(o.at("id"));
            obj.name=text(o.at("name"));

            auto kind=text(o.at("kind"));
            if(kind!="group"&&kind!="path") throw Error("UNSUPPORTED_OBJECT",kind);
            obj.kind=kind=="group"?Kind::group:Kind::path;

            auto& transform=o.at("transform").as_array();
            if(transform.size()!=6) throw Error("INVALID_TRANSFORM","Six matrix entries required");
            for(std::size_t k=0;k<6;++k) obj.transform[k]=read_scalar(transform[k]);

            if(obj.kind==Kind::group) {
                if(o.contains("contours")||o.contains("stroke")||o.contains("fill"))
                    throw Error("INVALID_OBJECT","Group has path-only fields");
                obj.children=ids(o.at("children"));
            } else {
                if(o.contains("children")) throw Error("INVALID_OBJECT","Path has children");
                if(text(o.at("fill"))!="none")
                    throw Error("UNSUPPORTED_APPEARANCE","Bootstrap supports stroked paths, no fill");

                auto& stroke=o.at("stroke").as_object();
                keys(stroke,{"rgba","width"});
                auto& rgba=stroke.at("rgba").as_array();
                if(rgba.size()!=4) throw Error("INVALID_COLOR","Four RGBA channels required");
                for(std::size_t k=0;k<4;++k) obj.color[k]=read_scalar(rgba[k]);
                obj.stroke_width=read_scalar(stroke.at("width"));

                for(const auto& pv:o.at("contours").as_array()) {
                    auto& path=pv.as_object();
                    keys(path,{"id","closed","points"});
                    Contour contour;
                    contour.id=text(path.at("id"));
                    contour.closed=path.at("closed").as_bool();

                    for(const auto& v:path.at("points").as_array()) {
                        auto& p=v.as_object();
                        keys(p,{"id","x","y","in_angle","in_length","out_angle","out_length"});
                        contour.points.push_back({
                            text(p.at("id")),
                            read_scalar(p.at("x")),read_scalar(p.at("y")),
                            read_scalar(p.at("in_angle")),read_scalar(p.at("in_length")),
                            read_scalar(p.at("out_angle")),read_scalar(p.at("out_length"))});
                    }
                    obj.contours.push_back(std::move(contour));
                }
            }

            if(!d.objects.emplace(obj.id,obj).second) throw Error("DUPLICATE_ID",obj.id);
        }

        for(const auto& cv:root.at("collections").as_array()) {
            auto& c=cv.as_object();
            keys(c,{"id","name","members"});
            d.collections.push_back({text(c.at("id")),text(c.at("name")),ids(c.at("members"))});
        }

        validate(d);
        return d;
    } catch(const Error&) {
        throw;
    } catch(const std::exception& e) {
        throw Error("INVALID_INPUT",e.what());
    }
}

std::string encode(const Document& d) {
    validate(d);

    j::array comps,objects,collections;

    for(const auto& c:d.compositions) {
        j::array boards;
        for(const auto& a:c.artboards)
            boards.push_back({
                {"id",a.id},{"name",a.name},{"x",a.x},{"y",a.y},
                {"width",a.width},{"height",a.height}});
        comps.push_back({
            {"id",c.id},{"name",c.name},{"roots",ids_json(c.roots)},{"artboards",boards}});
    }

    for(const auto& [id,o]:d.objects) {
        j::array tf;
        for(const auto& s:o.transform) tf.push_back(scalar_json(s));

        j::object out{
            {"id",id},{"name",o.name},{"kind",o.kind==Kind::group?"group":"path"},{"transform",tf}};

        if(o.kind==Kind::group) {
            out["children"]=ids_json(o.children);
        } else {
            j::array contours;
            for(const auto& c:o.contours) {
                j::array points;
                for(const auto& p:c.points)
                    points.push_back({
                        {"id",p.id},
                        {"x",scalar_json(p.x)},{"y",scalar_json(p.y)},
                        {"in_angle",scalar_json(p.in_angle)},{"in_length",scalar_json(p.in_length)},
                        {"out_angle",scalar_json(p.out_angle)},{"out_length",scalar_json(p.out_length)}});
                contours.push_back({{"id",c.id},{"closed",c.closed},{"points",points}});
            }

            out["contours"]=contours;
            out["fill"]="none";

            j::array rgba;
            for(const auto& s:o.color) rgba.push_back(scalar_json(s));
            out["stroke"]=j::object{{"rgba",rgba},{"width",scalar_json(o.stroke_width)}};
        }

        objects.push_back(out);
    }

    for(const auto& c:d.collections)
        collections.push_back({{"id",c.id},{"name",c.name},{"members",ids_json(c.members)}});

    return j::serialize(j::object{
        {"format","nect-native"},{"version","0.1"},{"id",d.id},
        {"units","du96"},{"color_space","srgb"},
        {"compositions",comps},{"objects",objects},{"collections",collections}});
}

std::string export_svg(const Document& d,const Id& comp_id,const Id& art_id) {
    validate(d);
    const auto values=evaluate(d);

    auto comp=std::find_if(d.compositions.begin(),d.compositions.end(),
        [&](const auto& c){return c.id==comp_id;});
    if(comp==d.compositions.end()) throw Error("MISSING_COMPOSITION",comp_id);

    auto art=std::find_if(comp->artboards.begin(),comp->artboards.end(),
        [&](const auto& a){return a.id==art_id;});
    if(art==comp->artboards.end()) throw Error("MISSING_ARTBOARD",art_id);

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out<<std::setprecision(std::numeric_limits<double>::max_digits10);
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<art->width
       <<"px\" height=\""<<art->height<<"px\" viewBox=\""<<art->x<<" "<<art->y
       <<" "<<art->width<<" "<<art->height<<"\">\n";

    std::function<void(const Id&)> render=[&](const Id& id) {
        const auto& o=d.objects.at(id);
        auto value=[&](const Id& p,const std::string& f){return values.at({id,p,f});};

        out<<"<g id=\""<<id<<"\" transform=\"matrix(";
        for(const auto* f:{"a","b","c","d","tx","ty"})
            out<<value("",std::string("transform.")+f)<<' ';
        out<<")\"><title>"<<escape(o.name)<<"</title>\n";

        if(o.kind==Kind::group) {
            for(const auto& child:o.children) render(child);
        } else {
            out<<"<path fill=\"none\" stroke=\"rgb("
               <<value("","stroke.r")*100<<"%,"
               <<value("","stroke.g")*100<<"%,"
               <<value("","stroke.b")*100<<"%)\" stroke-opacity=\""
               <<value("","stroke.a")<<"\" stroke-width=\""
               <<value("","stroke.width")<<"\" d=\"";

            for(const auto& c:o.contours) {
                auto xy=[&](const Point& p){return std::array{value(p.id,"x"),value(p.id,"y")};};
                const auto first=xy(c.points.front());
                out<<"M "<<first[0]<<' '<<first[1]<<' ';

                auto segments=c.closed?c.points.size():c.points.size()-1;
                for(std::size_t i=0;i<segments;++i) {
                    const auto& p=c.points[i];
                    const auto& q=c.points[(i+1)%c.points.size()];
                    const auto from=xy(p),to=xy(q);

                    double pa=value(p.id,"out.angle")*std::numbers::pi/180;
                    double qa=value(q.id,"in.angle")*std::numbers::pi/180;
                    double pl=value(p.id,"out.length");
                    double ql=value(q.id,"in.length");

                    out<<"C "
                       <<from[0]+pl*std::cos(pa)<<' '<<from[1]+pl*std::sin(pa)<<' '
                       <<to[0]+ql*std::cos(qa)<<' '<<to[1]+ql*std::sin(qa)<<' '
                       <<to[0]<<' '<<to[1]<<' ';
                }
                if(c.closed) out<<"Z ";
            }

            out<<"\"/>\n";
        }

        out<<"</g>\n";
    };

    for(const auto& id:comp->roots) render(id);
    out<<"</svg>\n";
    return out.str();
}

std::string request(Session& session,std::string_view input) {
    try {
        auto parsed=parse(input);
        auto& o=parsed.as_object();
        auto op=text(o.at("op"));
        j::value result;
        const bool mutation=op=="apply"||op=="undo"||op=="redo";
        j::value prior;
        std::map<Ref,double> prior_values;
        if(mutation) {prior=j::parse(encode(session.document()));prior_values=evaluate(session.document());}

        if(op=="get") {
            keys(o,{"op","ref"});
            auto r=read_ref(o.at("ref"));
            result=j::object{
                {"ref",ref_json(r)},
                {"authored",scalar_json(property(session.document(),r))},
                {"evaluated",evaluate(session.document()).at(r)}};
        } else if(op=="inspect") {
            keys(o,{"op"});
            result=j::parse(encode(session.document()));
        } else if(op=="properties") {
            keys(o,{"op"});
            j::array list;
            const auto values=evaluate(session.document());
            for(const auto& ref:properties(session.document()))
                list.push_back({{"ref",ref_json(ref)},
                    {"name",session.document().objects.at(ref.object).name},
                    {"type","number"},{"unit",property_unit(ref)},{"space","local"},
                    {"authored",scalar_json(property(session.document(),ref))},
                    {"evaluated",values.at(ref)}});
            result=std::move(list);
        } else if(op=="capabilities") {
            keys(o,{"op"});
            result=j::object{
                {"native_version","0.1"},
                {"transport","local-json-lines-not-mcp"},
                {"mcp",false},
                {"ai_codec",false},
                {"gui",false},
                {"expression_subset","copy_local_value * scale + offset"}};
        } else if(op=="resolve_name") {
            keys(o,{"op","name","point","field"});
            result=ref_json(resolve_name(
                session.document(),text(o.at("name")),text(o.at("point")),text(o.at("field"))));
        } else if(op=="apply") {
            keys(o,{"op","expected_revision","commands"});
            std::vector<Command> cmds;
            for(const auto& v:o.at("commands").as_array()) cmds.push_back(read_command(v));
            session.apply(cmds,j::value_to<std::uint64_t>(o.at("expected_revision")));
            result=j::object{{"changed",true}};
        } else if(op=="undo"||op=="redo") {
            keys(o,{"op","expected_revision"});
            auto rev=j::value_to<std::uint64_t>(o.at("expected_revision"));
            if(op=="undo") session.undo(rev);
            else session.redo(rev);
            result=j::object{{"changed",true}};
        } else if(op=="evaluate") {
            keys(o,{"op"});
            j::array a;
            for(const auto& [r,v]:evaluate(session.document()))
                a.push_back({{"ref",ref_json(r)},{"value",v}});
            result=a;
        } else if(op=="export_svg") {
            keys(o,{"op","composition","artboard"});
            result=export_svg(session.document(),text(o.at("composition")),text(o.at("artboard")));
        } else {
            throw Error("UNSUPPORTED_OPERATION",op);
        }

        if(mutation) {
            std::set<Id> changed;
            const auto after=j::parse(encode(session.document()));
            for(const auto* category:{"objects","compositions","collections"}) {
                std::map<Id,j::value> old;
                for(const auto& item:prior.as_object().at(category).as_array())old.emplace(text(item.as_object().at("id")),item);
                for(const auto& item:after.as_object().at(category).as_array()) {
                    const auto id=text(item.as_object().at("id"));
                    if(!old.contains(id)||old.at(id)!=item)changed.insert(id);
                    old.erase(id);
                }
                for(const auto& [id,value]:old) {(void)value;changed.insert(id);}
            }
            for(const auto& [ref,value]:evaluate(session.document()))
                if(!prior_values.contains(ref)||prior_values.at(ref)!=value)changed.insert(ref.object);
            result.as_object()["changed_ids"]=ids_json(std::vector<Id>(changed.begin(),changed.end()));
        }
        return j::serialize(j::object{
            {"ok",true},{"document_id",session.document().id},
            {"revision",session.revision()},{"result",result}});
    } catch(const Error& e) {
        return j::serialize(j::object{
            {"ok",false},{"revision",session.revision()},
            {"error",j::object{{"code",e.code},{"message",e.what()}}}});
    } catch(const std::exception& e) {
        return j::serialize(j::object{
            {"ok",false},{"revision",session.revision()},
            {"error",j::object{{"code","INVALID_REQUEST"},{"message",e.what()}}}});
    }
}
}
