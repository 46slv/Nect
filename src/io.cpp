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

std::string base64_encode(const std::vector<unsigned char>& bytes) {
    static constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;out.reserve((bytes.size()+2)/3*4);
    for(std::size_t i=0;i<bytes.size();i+=3) {
        const unsigned a=bytes[i],b=i+1<bytes.size()?bytes[i+1]:0,c=i+2<bytes.size()?bytes[i+2]:0;
        out+=alphabet[a>>2];out+=alphabet[((a&3)<<4)|(b>>4)];
        out+=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';out+=i+2<bytes.size()?alphabet[c&63]:'=';
    }
    return out;
}
std::vector<unsigned char> base64_decode(std::string_view source) {
    if(source.empty()||source.size()%4||source.size()>(raster_source_limit+2)/3*4)throw Error("INVALID_ASSET_BYTES","Expected bounded canonical base64 source bytes");
    const auto digit=[](char c)->unsigned {
        if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;
        if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;
        throw Error("INVALID_ASSET_BYTES","Invalid base64 character");
    };
    std::vector<unsigned char> bytes;bytes.reserve(source.size()/4*3);
    for(std::size_t i=0;i<source.size();i+=4) {
        const auto a=digit(source[i]),b=digit(source[i+1]);const bool p=source[i+2]=='=',q=source[i+3]=='=';
        if((p&&!q)||((p||q)&&i+4!=source.size()))throw Error("INVALID_ASSET_BYTES","Invalid base64 padding");
        const auto c=p?0:digit(source[i+2]),d=q?0:digit(source[i+3]);
        if((p&&(b&15))||(q&&!p&&(c&3)))throw Error("INVALID_ASSET_BYTES","Noncanonical base64 padding bits");
        bytes.push_back(static_cast<unsigned char>((a<<2)|(b>>4)));
        if(!p)bytes.push_back(static_cast<unsigned char>((b<<4)|(c>>2)));
        if(!q)bytes.push_back(static_cast<unsigned char>((c<<6)|d));
    }
    if(bytes.size()>raster_source_limit)throw Error("ASSET_LIMIT","Image source byte limit 8 MiB");
    return bytes;
}
void apply_serializable(Session& session,const std::vector<Command>& commands,std::uint64_t revision) {
    const bool asset_change=std::any_of(commands.begin(),commands.end(),[](const auto& c){return std::holds_alternative<AddRasterAsset>(c)||std::holds_alternative<ReplaceRasterAsset>(c);});
    if(asset_change) {
        if(session.revision()!=revision)throw Error("REVISION_CONFLICT","Expected revision differs from current Session");
        Session preview(session.document());preview.apply(commands,0);
        if(encode(preview.document()).size()>native_size_limit)throw Error("OUTPUT_LIMIT","Image edit would exceed the 64 MiB native serialization limit");
    }
    session.apply(commands,revision);
}
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
Expression read_expression(const j::value& v) {
    const auto& o=v.as_object();keys(o,{"source","version"});
    return {text(o.at("source")),j::value_to<unsigned>(o.at("version"))};
}
j::object expression_json(const Expression& expression) {
    return {{"source",expression.source},{"version",expression.version}};
}
Scalar read_scalar(const j::value& v,bool allow_expression=true) {
    const auto& o=v.as_object();
    if(allow_expression)keys(o,{"literal","binding","expression"});
    else keys(o,{"literal","binding"});
    Scalar s{number(o.at("literal")),{}};
    if(auto* b=o.if_contains("binding");b&&!b->is_null()) s.binding=read_binding(*b);
    if(auto* e=o.if_contains("expression");e&&!e->is_null())s.expression=read_expression(*e);
    return s;
}
j::value scalar_json(const Scalar& s) {
    j::object o{{"literal",s.literal}};
    if(s.binding) {
        const auto& b=*s.binding;
        o["binding"]=j::object{
            {"source",ref_json(b.source)},{"scale",b.scale},{"offset",b.offset},{"mode",b.mode}};
    }
    if(s.expression)o["expression"]=expression_json(*s.expression);
    return o;
}
RasterAsset read_asset(const j::value& value,bool persisted=false) {
    const auto& o=value.as_object();keys(o,{"id","name","mode","locator","version","bytes","sha256","mime","width","height","orientation","color_interpretation","interpretation_version"});
    if(j::value_to<unsigned>(o.at("version"))!=1)throw Error("UNSUPPORTED_ASSET_VERSION","Only raster asset version 1 is supported");
    RasterAsset a;a.id=text(o.at("id"));a.name=text(o.at("name"));a.mode=text(o.at("mode"));a.locator=text(o.at("locator"));
    a.payload=make_raster(base64_decode(text(o.at("bytes"))));
    const auto same=[&](const char* name,const j::value& expected) {
        const auto found=o.if_contains(name);
        if((persisted&&!found)||(found&&*found!=expected))throw Error("ASSET_METADATA_MISMATCH",std::string("Accepted bytes disagree with ")+name);
    };
    same("sha256",j::value(a.payload->sha256()));same("mime",j::value(a.payload->mime()));
    same("width",j::value(a.payload->width()));same("height",j::value(a.payload->height()));same("orientation",j::value(a.payload->orientation()));
    same("color_interpretation",j::value(a.payload->color_interpretation()));same("interpretation_version",j::value(a.payload->interpretation_version()));
    return a;
}
j::object asset_json(const RasterAsset& a,bool include_bytes=false) {
    const auto& p=*a.payload;
    j::object out{{"id",a.id},{"name",a.name},{"mode",a.mode},{"locator",a.locator},{"version",1},
        {"sha256",p.sha256()},{"mime",p.mime()},{"width",p.width()},{"height",p.height()},{"orientation",p.orientation()},
        {"color_interpretation",p.color_interpretation()},{"interpretation_version",p.interpretation_version()}};
    if(include_bytes)out["bytes"]=base64_encode(p.bytes());return out;
}
ImageSource read_image(const j::value& value) {
    const auto& o=value.as_object();keys(o,{"asset","width","height"});
    return {text(o.at("asset")),read_scalar(o.at("width")),read_scalar(o.at("height"))};
}
j::object image_json(const ImageSource& i){return {{"asset",i.asset},{"width",scalar_json(i.width)},{"height",scalar_json(i.height)}};}
GeometryMask read_mask(const j::value& value) {
    const auto& o=value.as_object();keys(o,{"id","source","version","enabled","fill_rule"});
    return {text(o.at("id")),text(o.at("source")),j::value_to<unsigned>(o.at("version")),o.at("enabled").as_bool(),text(o.at("fill_rule"))};
}
j::value mask_json(const std::optional<GeometryMask>& mask) {
    if(!mask)return nullptr;
    return j::object{{"id",mask->id},{"source",mask->source},{"version",mask->version},{"enabled",mask->enabled},{"fill_rule",mask->fill_rule}};
}
Compositing read_compositing(const j::value& value) {
    const auto& o=value.as_object();keys(o,{"version","opacity","blend","isolated","mask"});
    Compositing c;c.version=j::value_to<unsigned>(o.at("version"));c.opacity=read_scalar(o.at("opacity"));
    c.blend=text(o.at("blend"));c.isolated=o.at("isolated").as_bool();if(!o.at("mask").is_null())c.mask=read_mask(o.at("mask"));return c;
}
j::object compositing_json(const Compositing& c) {
    return {{"version",c.version},{"opacity",scalar_json(c.opacity)},{"blend",c.blend},{"isolated",c.isolated},{"mask",mask_json(c.mask)}};
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
    static constexpr std::size_t max_key_size=4096, max_string_size=native_size_limit;
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

j::parse_options precise_json_options() {
    j::parse_options options;
    options.max_depth=64;
    // Boost.JSON defaults to imprecise floating-point parsing. Native state and
    // API readback must preserve the exact doubles emitted by the serializer.
    options.numbers=j::number_precision::precise;
    return options;
}
j::value parse(std::string_view s) {
    if(s.size()>native_size_limit) throw Error("INPUT_LIMIT","Input exceeds 64 MiB");
    const auto options=precise_json_options();

    j::basic_parser<UniqueKeys> unique(options);
    j::error_code ec;
    unique.write_some(false,s.data(),s.size(),ec);
    if(ec) throw Error("INVALID_JSON",ec.message());

    return j::parse(s,{},options);
}

Point read_point(const j::value& v,bool allow_expression=true) {
    const auto& p=v.as_object();
    keys(p,{"id","x","y","in_angle","in_length","out_angle","out_length"});
    return {text(p.at("id")),read_scalar(p.at("x"),allow_expression),read_scalar(p.at("y"),allow_expression),
        read_scalar(p.at("in_angle"),allow_expression),read_scalar(p.at("in_length"),allow_expression),
        read_scalar(p.at("out_angle"),allow_expression),read_scalar(p.at("out_length"),allow_expression)};
}
Contour read_contour(const j::value& v,bool allow_expression=true) {
    const auto& c=v.as_object();
    keys(c,{"id","closed","points"});
    Contour out{text(c.at("id")),c.at("closed").as_bool(),{}};
    for(const auto& p:c.at("points").as_array()) out.points.push_back(read_point(p,allow_expression));
    return out;
}

TextSource read_text(const j::value& v,bool allow_expression=true) {
    const auto& o=v.as_object();keys(o,{"id","version","content","family","locale","layout","direction","alignment","weight","italic","parameters"});
    TextSource s;s.id=text(o.at("id"));s.version=j::value_to<unsigned>(o.at("version"));
    s.content=text(o.at("content"));s.family=text(o.at("family"));s.locale=text(o.at("locale"));
    s.layout=text(o.at("layout"));s.direction=text(o.at("direction"));s.alignment=text(o.at("alignment"));
    s.weight=j::value_to<unsigned>(o.at("weight"));s.italic=o.at("italic").as_bool();
    for(const auto& p:o.at("parameters").as_object())s.parameters.emplace(std::string(p.key()),read_scalar(p.value(),allow_expression));
    return s;
}
j::value color_json(const ColorValue& color) {
    j::array rgba;for(const auto value:color.rgba)rgba.push_back(value);
    return j::object{{"space",color.space},{"profile",color.profile},{"alpha",color.alpha},{"rgba",rgba}};
}
void color_format(const j::object& o) {
    if(text(o.at("space"))!="srgb"||text(o.at("profile"))!="srgb"||text(o.at("alpha"))!="straight")
        throw Error("UNSUPPORTED_COLOR","Only sRGB, sRGB profile, straight alpha colors are supported");
}
ColorValue read_color(const j::value& value) {
    const auto& o=value.as_object();keys(o,{"space","profile","alpha","rgba"});color_format(o);
    ColorValue color;const auto& rgba=o.at("rgba").as_array();if(rgba.size()!=4)throw Error("INVALID_COLOR","Four RGBA channels required");
    for(std::size_t i=0;i<4;++i)color.rgba[i]=number(rgba[i]);return color;
}
NamedColor read_named_color(const j::value& value,bool allow_expression=true) {
    const auto& o=value.as_object();keys(o,{"id","name","space","profile","alpha","rgba"});color_format(o);
    NamedColor color;color.id=text(o.at("id"));color.name=text(o.at("name"));const auto& rgba=o.at("rgba").as_array();
    if(rgba.size()!=4)throw Error("INVALID_COLOR","Four RGBA channels required");
    for(std::size_t i=0;i<4;++i)color.rgba[i]=read_scalar(rgba[i],allow_expression);return color;
}
j::object named_color_json(const NamedColor& color) {
    j::array rgba;for(const auto& scalar:color.rgba)rgba.push_back(scalar_json(scalar));
    return {{"id",color.id},{"name",color.name},{"space","srgb"},{"profile","srgb"},{"alpha","straight"},{"rgba",rgba}};
}
j::object color_property_json(const Document& d,const Ref& ref,const std::map<Ref,double>& values) {
    j::array channels,authored;for(const auto& channel:color_channels(d,ref)){channels.push_back(ref_json(channel));authored.push_back(scalar_json(property(d,channel)));}
    const auto link=color_link(d,ref);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","color"},{"authored",authored},
        {"evaluated",color_json(color_value(d,ref,values))},{"channels",channels},{"used",color_is_used(d,ref)},
        {"link",link?j::value(ref_json(*link)):j::value(nullptr)}};
}
j::value text_json(const TextSource& s) {
    j::object parameters;for(const auto& [name,value]:s.parameters)parameters[name]=scalar_json(value);
    return j::object{{"id",s.id},{"version",s.version},{"content",s.content},{"family",s.family},{"locale",s.locale},
        {"layout",s.layout},{"direction",s.direction},{"alignment",s.alignment},{"weight",s.weight},{"italic",s.italic},{"parameters",parameters}};
}
j::object text_layout_json(const Document& d,const Id& id) {
    const auto object=d.objects.find(id);
    if(object==d.objects.end())throw Error("MISSING_OBJECT",id);
    if(!object->second.text)throw Error("NOT_TEXT",id);
    const auto values=evaluate(d);std::map<std::string,double> parameters;
    for(const auto& [name,value]:object->second.text->parameters){(void)value;parameters[name]=values.at({id,"","text."+name});}
    const auto layout=evaluate_text(*object->second.text,parameters);
    return {{"object",id},{"x",layout.x},{"y",layout.y},{"width",layout.width},{"height",layout.height},
        {"overflow",layout.overflow},{"glyph_count",layout.glyph_count},{"warnings",ids_json(layout.warnings)},
        {"used_fonts",ids_json(layout.used_fonts)},{"svg_text","outlined"},{"font_embedded",false}};
}

Primitive read_primitive(const j::value& v,bool allow_polystar=true,bool allow_expression=true) {
    const auto& o=v.as_object();keys(o,{"id","type","version","parameters"});
    Primitive s{text(o.at("id")),text(o.at("type")),j::value_to<unsigned>(o.at("version")),{}};
    if(!allow_polystar&&(s.type=="nect.shape.polygon"||s.type=="nect.shape.star"))
        throw Error("UNSUPPORTED_OPERATOR","Polygon and Star require native 0.8");
    for(const auto& p:o.at("parameters").as_object())
        s.parameters.emplace(std::string(p.key()),read_scalar(p.value(),allow_expression));
    return s;
}
PointEdit read_point_edit(const j::value& v,bool allow_expression=true) {
    const auto& o=v.as_object();keys(o,{"id","type","version","enabled","overrides"});
    if(text(o.at("type"))!="nect.path.point-edit")throw Error("UNSUPPORTED_OPERATOR",text(o.at("type")));
    PointEdit edit{text(o.at("id")),j::value_to<unsigned>(o.at("version")),o.at("enabled").as_bool(),{}};
    for(const auto& p:o.at("overrides").as_object()) {
        auto& fields=edit.overrides[std::string(p.key())];
        for(const auto& f:p.value().as_object())fields.emplace(std::string(f.key()),read_scalar(f.value(),allow_expression));
    }
    return edit;
}
j::value primitive_json(const Primitive& source) {
    j::object parameters;for(const auto& [name,value]:source.parameters)parameters[name]=scalar_json(value);
    return j::object{{"id",source.id},{"type",source.type},{"version",source.version},{"parameters",parameters}};
}
j::value point_edit_json(const PointEdit& edit) {
    j::object overrides;
    for(const auto& [point,values]:edit.overrides) {
        j::object fields;for(const auto& [field,value]:values)fields[field]=scalar_json(value);
        overrides[point]=fields;
    }
    return j::object{{"id",edit.id},{"type","nect.path.point-edit"},{"version",edit.version},
        {"enabled",edit.enabled},{"overrides",overrides}};
}

Gradient read_gradient(const j::value& v,bool allow_expression=true) {
    const auto& o=v.as_object();keys(o,{"id","type","version","enabled","start_x","start_y","end_x","end_y","stops"});
    Gradient g;g.id=text(o.at("id"));g.type=text(o.at("type"));g.version=j::value_to<unsigned>(o.at("version"));
    g.enabled=o.at("enabled").as_bool();g.start_x=read_scalar(o.at("start_x"),allow_expression);g.start_y=read_scalar(o.at("start_y"),allow_expression);
    g.end_x=read_scalar(o.at("end_x"),allow_expression);g.end_y=read_scalar(o.at("end_y"),allow_expression);
    for(const auto& entry:o.at("stops").as_array()) {
        const auto& s=entry.as_object();keys(s,{"id","offset","rgba"});
        GradientStop stop;stop.id=text(s.at("id"));stop.offset=read_scalar(s.at("offset"),allow_expression);
        const auto& rgba=s.at("rgba").as_array();if(rgba.size()!=4)throw Error("INVALID_COLOR","Four RGBA channels required");
        for(std::size_t k=0;k<4;++k)stop.rgba[k]=read_scalar(rgba[k],allow_expression);
        g.stops.push_back(std::move(stop));
    }
    return g;
}
j::value gradient_json(const Gradient& g) {
    j::array stops;
    for(const auto& stop:g.stops) {
        j::array rgba;for(const auto& channel:stop.rgba)rgba.push_back(scalar_json(channel));
        stops.push_back({{"id",stop.id},{"offset",scalar_json(stop.offset)},{"rgba",rgba}});
    }
    return j::object{{"id",g.id},{"type",g.type},{"version",g.version},{"enabled",g.enabled},
        {"start_x",scalar_json(g.start_x)},{"start_y",scalar_json(g.start_y)},
        {"end_x",scalar_json(g.end_x)},{"end_y",scalar_json(g.end_y)},{"stops",stops}};
}
ShapeOperation read_operation(const j::value& v,bool allow_gradient=true,bool allow_expression=true,bool allow_offset=true) {
    const auto& o=v.as_object();
    if(allow_offset)keys(o,{"id","type","version","enabled","parameters","composite","fill_rule","gradient","line_join"});
    else if(allow_gradient)keys(o,{"id","type","version","enabled","parameters","composite","fill_rule","gradient"});
    else keys(o,{"id","type","version","enabled","parameters","composite","fill_rule"});
    ShapeOperation op;op.id=text(o.at("id"));op.type=text(o.at("type"));
    op.version=j::value_to<unsigned>(o.at("version"));op.enabled=o.at("enabled").as_bool();
    op.composite=text(o.at("composite"));op.fill_rule=text(o.at("fill_rule"));
    if(op.type=="nect.shape.offset") {
        if(!allow_offset)throw Error("UNSUPPORTED_OPERATOR","Offset Paths requires native 0.12");
        op.line_join=text(o.at("line_join"));
    } else if(o.contains("line_join"))throw Error("INVALID_OPERATOR_OPTIONS","Line join applies only to Offset Paths");
    for(const auto& p:o.at("parameters").as_object())op.parameters.emplace(std::string(p.key()),read_scalar(p.value(),allow_expression));
    if(const auto* g=o.if_contains("gradient"))op.gradient=read_gradient(*g,allow_expression);
    return op;
}
j::value operation_json(const ShapeOperation& op) {
    j::object parameters;for(const auto& [name,value]:op.parameters)parameters[name]=scalar_json(value);
    j::object result{{"id",op.id},{"type",op.type},{"version",op.version},{"enabled",op.enabled},
        {"parameters",parameters},{"composite",op.composite},{"fill_rule",op.fill_rule}};
    if(op.gradient)result["gradient"]=gradient_json(*op.gradient);
    if(op.type=="nect.shape.offset")result["line_join"]=op.line_join;
    return result;
}

Artboard read_artboard(const j::value& v,bool allow_parent=true) {
    const auto& a=v.as_object();
    if(allow_parent)keys(a,{"id","name","x","y","width","height","parent_size"});
    else keys(a,{"id","name","x","y","width","height"});
    Artboard result{text(a.at("id")),text(a.at("name")),number(a.at("x")),number(a.at("y")),number(a.at("width")),number(a.at("height"))};
    if(const auto* p=a.if_contains("parent_size")) {
        const auto& parent=p->as_object();keys(parent,{"artboard","width","height"});
        result.parent_size=ArtboardParent{text(parent.at("artboard")),parent.at("width").as_bool(),parent.at("height").as_bool()};
    }
    return result;
}
j::object artboard_json(const Artboard& a) {
    j::object result{{"id",a.id},{"name",a.name},{"x",a.x},{"y",a.y},{"width",a.width},{"height",a.height}};
    if(a.parent_size)result["parent_size"]=j::object{{"artboard",a.parent_size->artboard},{"width",a.parent_size->width},{"height",a.parent_size->height}};
    return result;
}

Command read_command(const j::value& v) {
    auto& o=v.as_object();
    auto type=text(o.at("type"));
    if(type=="add_raster_asset"||type=="replace_raster_asset") {
        keys(o,{"type","asset"});auto asset=read_asset(o.at("asset"));
        if(type=="add_raster_asset")return AddRasterAsset{std::move(asset)};return ReplaceRasterAsset{std::move(asset)};
    }
    if(type=="delete_raster_asset"){keys(o,{"type","asset"});return DeleteRasterAsset{text(o.at("asset"))};}
    if(type=="create_image") {
        keys(o,{"type","composition","parent","id","name","source"});
        return CreateImage{text(o.at("composition")),text(o.at("parent")),text(o.at("id")),text(o.at("name")),read_image(o.at("source"))};
    }
    if(type=="create_named_color") {
        keys(o,{"type","color"});return CreateNamedColor{read_named_color(o.at("color"))};
    }
    if(type=="rename_named_color") {
        keys(o,{"type","color","name"});return RenameNamedColor{text(o.at("color")),text(o.at("name"))};
    }
    if(type=="delete_named_color") {
        keys(o,{"type","color"});return DeleteNamedColor{text(o.at("color"))};
    }
    if(type=="set_color") {
        keys(o,{"type","ref","value"});return SetColor{read_ref(o.at("ref")),read_color(o.at("value"))};
    }
    if(type=="link_color") {
        keys(o,{"type","target","source"});return LinkColor{read_ref(o.at("target")),read_ref(o.at("source"))};
    }
    if(type=="unlink_color") {
        keys(o,{"type","ref"});return UnlinkColor{read_ref(o.at("ref"))};
    }
    if(type=="create_text") {
        keys(o,{"type","composition","parent","id","name","source"});
        return CreateText{text(o.at("composition")),text(o.at("parent")),text(o.at("id")),text(o.at("name")),read_text(o.at("source"))};
    }
    if(type=="update_text") {
        keys(o,{"type","object","source"});return UpdateText{text(o.at("object")),read_text(o.at("source"))};
    }
    if(type=="add_artboard") {
        keys(o,{"type","composition","artboard","index"});
        return AddArtboard{text(o.at("composition")),read_artboard(o.at("artboard")),j::value_to<std::size_t>(o.at("index"))};
    }
    if(type=="update_artboard") {
        keys(o,{"type","composition","artboard"});return UpdateArtboard{text(o.at("composition")),read_artboard(o.at("artboard"))};
    }
    if(type=="delete_artboard"||type=="detach_artboard_parent") {
        keys(o,{"type","composition","artboard"});
        if(type=="delete_artboard")return DeleteArtboard{text(o.at("composition")),text(o.at("artboard"))};
        return DetachArtboardParent{text(o.at("composition")),text(o.at("artboard"))};
    }
    if(type=="reorder_artboards") {
        keys(o,{"type","composition","order"});return ReorderArtboards{text(o.at("composition")),ids(o.at("order"))};
    }
    if(type=="set_gradient") {
        keys(o,{"type","object","operation","gradient"});
        std::optional<Gradient> g;if(!o.at("gradient").is_null())g=read_gradient(o.at("gradient"));
        return SetGradient{text(o.at("object")),text(o.at("operation")),std::move(g)};
    }
    if(type=="add_operation") {
        keys(o,{"type","object","operation","index"});
        return AddOperation{text(o.at("object")),read_operation(o.at("operation")),j::value_to<std::size_t>(o.at("index"))};
    }
    if(type=="remove_operation") {
        keys(o,{"type","object","operation"});return RemoveOperation{text(o.at("object")),text(o.at("operation"))};
    }
    if(type=="reorder_operations") {
        keys(o,{"type","object","order"});return ReorderOperations{text(o.at("object")),ids(o.at("order"))};
    }
    if(type=="enable_operation") {
        keys(o,{"type","object","operation","enabled"});
        return EnableOperation{text(o.at("object")),text(o.at("operation")),o.at("enabled").as_bool()};
    }
    if(type=="operation_options") {
        keys(o,{"type","object","operation","composite","fill_rule","line_join"});
        return OperationOptions{text(o.at("object")),text(o.at("operation")),text(o.at("composite")),text(o.at("fill_rule")),
            o.contains("line_join")?std::optional<std::string>{text(o.at("line_join"))}:std::nullopt};
    }
    if(type=="create_primitive") {
        keys(o,{"type","composition","parent","id","name","source"});
        return CreatePrimitive{text(o.at("composition")),text(o.at("parent")),text(o.at("id")),
            text(o.at("name")),read_primitive(o.at("source"))};
    }
    if(type=="enable_point_edit") {
        keys(o,{"type","object","enabled"});
        return EnablePointEdit{text(o.at("object")),o.at("enabled").as_bool()};
    }
    if(type=="clear_point_edit") {
        keys(o,{"type","object"});return ClearPointEdit{text(o.at("object"))};
    }
    if(type=="convert_to_path") {
        keys(o,{"type","object"});return ConvertToPath{text(o.at("object"))};
    }
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
    if(type=="duplicate_objects") {
        keys(o,{"type","objects","prefix"});
        return DuplicateObjects{ids(o.at("objects")),text(o.at("prefix"))};
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
    if(type=="center_anchor") {
        keys(o,{"type","object"});return CenterAnchor{text(o.at("object"))};
    }
    if(type=="set_visibility") {keys(o,{"type","object","visible"});return SetVisibility{text(o.at("object")),o.at("visible").as_bool()};}
    if(type=="set_compositing") {keys(o,{"type","object","blend","isolated"});return SetCompositing{text(o.at("object")),text(o.at("blend")),o.at("isolated").as_bool()};}
    if(type=="set_mask") {
        keys(o,{"type","object","mask"});std::optional<GeometryMask> mask;if(!o.at("mask").is_null())mask=read_mask(o.at("mask"));
        return SetMask{text(o.at("object")),std::move(mask)};
    }
    if(type=="mask_objects") {
        keys(o,{"type","composition","parent","members","id","mask_id","name","top"});
        return MaskObjects{text(o.at("composition")),text(o.at("parent")),ids(o.at("members")),text(o.at("id")),text(o.at("mask_id")),text(o.at("name")),o.at("top").as_bool()};
    }
    if(type=="put_inside") {
        keys(o,{"type","composition","parent","group","members"});
        return PutInside{text(o.at("composition")),text(o.at("parent")),text(o.at("group")),ids(o.at("members"))};
    }
    if(type=="set_expression") {
        keys(o,{"type","targets","expression","replace_binding"});
        std::vector<Ref> targets;for(const auto& r:o.at("targets").as_array())targets.push_back(read_ref(r));
        return SetExpression{std::move(targets),read_expression(o.at("expression")),o.at("replace_binding").as_bool()};
    }
    if(type=="edit_properties"||type=="link_properties"||type=="unlink_properties") {
        if(type=="edit_properties")keys(o,{"type","targets","value","relative"});
        else if(type=="link_properties")keys(o,{"type","targets","source","relative"});
        else keys(o,{"type","targets"});
        std::vector<Ref> targets;for(const auto& target:o.at("targets").as_array())targets.push_back(read_ref(target));
        if(type=="edit_properties")return EditProperties{std::move(targets),number(o.at("value")),o.at("relative").as_bool()};
        if(type=="link_properties")return LinkProperties{std::move(targets),read_ref(o.at("source")),o.at("relative").as_bool()};
        return UnlinkProperties{std::move(targets)};
    }
    if(type=="distribute_objects") {
        keys(o,{"type","objects","axis"});
        return DistributeObjects{ids(o.at("objects")),text(o.at("axis"))};
    }
    if(type=="align_objects") {
        keys(o,{"type","objects","axis","alignment","artboard"});
        return AlignObjects{ids(o.at("objects")),text(o.at("axis")),text(o.at("alignment")),o.at("artboard").is_null()?std::optional<Id>{}:std::optional<Id>{text(o.at("artboard"))}};
    }
    if(type=="translate_objects") {
        keys(o,{"type","objects","dx","dy"});return TranslateObjects{ids(o.at("objects")),number(o.at("dx")),number(o.at("dy"))};
    }
    if(type=="set_position") {
        keys(o,{"type","object","x","y"});return SetPosition{text(o.at("object")),number(o.at("x")),number(o.at("y"))};
    }
    if(type=="transform_around_anchor") {
        keys(o,{"type","object","rotation","scale_x","scale_y"});
        return TransformAroundAnchor{text(o.at("object")),number(o.at("rotation")),number(o.at("scale_x")),number(o.at("scale_y"))};
    }
    if(type=="set_transform_parent") {
        keys(o,{"type","object","parent","preserve_world"});
        return SetTransformParent{text(o.at("object")),o.at("parent").is_null()?std::optional<Id>{}:std::optional<Id>{text(o.at("parent"))},o.at("preserve_world").as_bool()};
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
        const auto version=text(root.at("version"));
        constexpr std::array<std::string_view,13> supported{"0.1","0.2","0.3","0.4","0.5","0.6","0.7","0.8","0.9","0.10","0.11","0.12","0.13"};
        const auto accepted=std::find(supported.begin(),supported.end(),version);
        if(text(root.at("format"))!="nect-native"||accepted==supported.end())
            throw Error("UNSUPPORTED_FORMAT","Only nect-native 0.1 through 0.13 are supported");
        const auto minor=std::distance(supported.begin(),accepted)+1;
        if(minor>=13)keys(root,{"format","version","id","units","color_space","compositions","objects","collections","named_colors","raster_assets"});
        else if(minor>=7)keys(root,{"format","version","id","units","color_space","compositions","objects","collections","named_colors"});
        else keys(root,{"format","version","id","units","color_space","compositions","objects","collections"});
        if(text(root.at("units"))!="du96"||text(root.at("color_space"))!="srgb")
            throw Error("UNSUPPORTED_COLOR_OR_UNIT","v0.1 supports du96 and sRGB only");

        Document d;
        d.id=text(root.at("id"));
        std::map<Id,ShapeOperation> legacy_paints;

        for(const auto& cv:root.at("compositions").as_array()) {
            auto& co=cv.as_object();
            keys(co,{"id","name","roots","artboards"});
            Composition c;
            c.id=text(co.at("id"));
            c.name=text(co.at("name"));
            c.roots=ids(co.at("roots"));

            for(const auto& av:co.at("artboards").as_array())c.artboards.push_back(read_artboard(av,minor>=5));
            d.compositions.push_back(std::move(c));
        }

        for(const auto& ov:root.at("objects").as_array()) {
            auto& o=ov.as_object();
            if(version=="0.1")keys(o,{"id","name","kind","transform","children","contours","stroke","fill"});
            else if(version=="0.2")keys(o,{"id","name","kind","transform","children","contours","stroke","fill","source","point_edit"});
            else if(minor>=13)keys(o,{"id","name","visible","compositing","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke","image"});
            else if(minor>=11)keys(o,{"id","name","visible","compositing","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke"});
            else if(minor>=9)keys(o,{"id","name","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke"});
            else if(minor>=6)keys(o,{"id","name","kind","transform","children","contours","source","point_edit","text","stack","legacy_stroke"});
            else keys(o,{"id","name","kind","transform","children","contours","source","point_edit","stack","legacy_stroke"});

            Object obj;
            obj.id=text(o.at("id"));
            obj.name=text(o.at("name"));
            if(minor>=11){obj.visible=o.at("visible").as_bool();obj.compositing=read_compositing(o.at("compositing"));}

            auto kind=text(o.at("kind"));
            if(kind!="group"&&kind!="path"&&!((minor>=6)&&kind=="text")&&!((minor>=13)&&kind=="image")) throw Error("UNSUPPORTED_OBJECT",kind);
            obj.kind=kind=="group"?Kind::group:kind=="text"?Kind::text:kind=="image"?Kind::image:Kind::path;
            if(o.contains("image")&&obj.kind!=Kind::image)throw Error("INVALID_OBJECT","Only Image may carry an image source");
            if(o.contains("text")&&obj.kind!=Kind::text)throw Error("INVALID_OBJECT","Only Text may carry a text source");

            auto& transform=o.at("transform").as_array();
            if(transform.size()!=6) throw Error("INVALID_TRANSFORM","Six matrix entries required");
            for(std::size_t k=0;k<6;++k) obj.transform[k]=read_scalar(transform[k],minor>=10);
            if(minor>=9) {
                const auto& anchor=o.at("anchor").as_array();
                if(anchor.size()!=2)throw Error("INVALID_TRANSFORM","Two anchor entries required");
                for(std::size_t k=0;k<2;++k)obj.anchor[k]=read_scalar(anchor[k],minor>=10);
                if(!o.at("transform_parent").is_null())obj.transform_parent=text(o.at("transform_parent"));
            }

            if(obj.kind==Kind::image) {
                for(const auto* field:{"children","contours","stroke","fill","source","point_edit","text","stack","legacy_stroke"})
                    if(o.contains(field))throw Error("INVALID_IMAGE","Image has incompatible vector fields");
                obj.image=read_image(o.at("image"));
            } else if(obj.kind==Kind::group) {
                if(o.contains("contours")||o.contains("stroke")||o.contains("fill")||o.contains("source")||o.contains("point_edit")||o.contains("stack")||o.contains("legacy_stroke"))
                    throw Error("INVALID_OBJECT","Group has path-only fields");
                obj.children=ids(o.at("children"));
            } else {
                if(o.contains("children")) throw Error("INVALID_OBJECT","Path has children");
                if(minor>=3) {
                    for(const auto& entry:o.at("stack").as_array())obj.stack.push_back(read_operation(entry,minor>=4,minor>=10,minor>=12));
                    obj.legacy_stroke=text(o.at("legacy_stroke"));
                } else {
                    if(text(o.at("fill"))!="none")throw Error("UNSUPPORTED_APPEARANCE","Legacy format only supports stroked paths");
                    const auto& stroke=o.at("stroke").as_object();keys(stroke,{"rgba","width"});
                    const auto& rgba=stroke.at("rgba").as_array();
                    if(rgba.size()!=4)throw Error("INVALID_COLOR","Four RGBA channels required");
                    auto paint=default_operation("","nect.paint.stroke");
                    const std::array<std::string,4> channels{"r","g","b","a"};
                    for(std::size_t k=0;k<4;++k)paint.parameters[channels[k]]=read_scalar(rgba[k],minor>=10);
                    paint.parameters["width"]=read_scalar(stroke.at("width"),minor>=10);legacy_paints.emplace(obj.id,std::move(paint));
                }

                if(obj.kind==Kind::text) {
                    if(o.contains("source")||o.contains("point_edit")||o.contains("contours"))throw Error("INVALID_OBJECT","Text has incompatible geometry fields");
                    obj.text=read_text(o.at("text"),minor>=10);
                } else if(o.contains("source")) {
                    if(o.contains("contours"))throw Error("INVALID_OBJECT","Generator and authored contours are mutually exclusive");
                    obj.source=read_primitive(o.at("source"),minor>=8,minor>=10);
                    if(o.contains("point_edit"))obj.point_edit=read_point_edit(o.at("point_edit"),minor>=10);
                } else {
                    if(o.contains("point_edit"))throw Error("INVALID_POINT_EDIT","Point Edit needs a retained generator");
                    for(const auto& c:o.at("contours").as_array())obj.contours.push_back(read_contour(c,minor>=10));
                }
            }

            if(!d.objects.emplace(obj.id,obj).second) throw Error("DUPLICATE_ID",obj.id);
        }

        for(const auto& cv:root.at("collections").as_array()) {
            auto& c=cv.as_object();
            keys(c,{"id","name","members"});
            d.collections.push_back({text(c.at("id")),text(c.at("name")),ids(c.at("members"))});
        }

        // All original IDs are present before allocating migration instances.
        if(minor>=7)for(const auto& entry:root.at("named_colors").as_array()) {
            auto color=read_named_color(entry,minor>=10);const auto id=color.id;
            if(!d.named_colors.emplace(id,std::move(color)).second)throw Error("DUPLICATE_ID",id);
        }
        if(minor>=13) {
            const auto& assets=root.at("raster_assets").as_array();if(assets.size()>128)throw Error("ASSET_LIMIT","Asset count limit 128");
            std::size_t bytes=0;std::uint64_t pixels=0;
            for(const auto& entry:assets) {
                auto a=read_asset(entry,true);bytes+=a.payload->bytes().size();pixels+=std::uint64_t(a.payload->width())*a.payload->height();
                if(bytes>document_raster_bytes_limit||pixels>document_raster_pixels_limit)throw Error("ASSET_LIMIT","Document raster byte/pixel budget exceeded");
                const auto id=a.id;if(!d.raster_assets.emplace(id,std::move(a)).second)throw Error("DUPLICATE_ID",id);
            }
        }
        for(const auto& [id,paint]:legacy_paints) {
            add_default_stroke(d,id);d.objects.at(id).stack.front().parameters=paint.parameters;
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

    j::array comps,objects,collections,named_colors,raster_assets;
    for(const auto& [id,asset]:d.raster_assets){(void)id;raster_assets.push_back(asset_json(asset,true));}
    for(const auto& [id,color]:d.named_colors){(void)id;named_colors.push_back(named_color_json(color));}

    for(const auto& c:d.compositions) {
        j::array boards;
        for(const auto& a:c.artboards)boards.push_back(artboard_json(a));
        comps.push_back({
            {"id",c.id},{"name",c.name},{"roots",ids_json(c.roots)},{"artboards",boards}});
    }

    for(const auto& [id,o]:d.objects) {
        j::array tf;
        for(const auto& s:o.transform) tf.push_back(scalar_json(s));
        j::array anchor;for(const auto& s:o.anchor)anchor.push_back(scalar_json(s));

        j::object out{
            {"id",id},{"name",o.name},{"visible",o.visible},{"compositing",compositing_json(o.compositing)},{"kind",o.kind==Kind::group?"group":o.kind==Kind::text?"text":o.kind==Kind::image?"image":"path"},{"transform",tf},{"anchor",anchor},{"transform_parent",o.transform_parent?j::value(*o.transform_parent):j::value(nullptr)}};

        if(o.image)out["image"]=image_json(*o.image);
        else if(o.kind==Kind::group) {
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

            if(o.text)out["text"]=text_json(*o.text);
            else if(o.source) {
                out["source"]=primitive_json(*o.source);
                if(o.point_edit)out["point_edit"]=point_edit_json(*o.point_edit);
            } else out["contours"]=contours;
            j::array stack;for(const auto& op:o.stack)stack.push_back(operation_json(op));
            out["stack"]=stack;out["legacy_stroke"]=o.legacy_stroke;
        }

        objects.push_back(out);
    }

    for(const auto& c:d.collections)
        collections.push_back({{"id",c.id},{"name",c.name},{"members",ids_json(c.members)}});

    return j::serialize(j::object{
        {"format","nect-native"},{"version",native_version},{"id",d.id},
        {"units","du96"},{"color_space","srgb"},
        {"compositions",comps},{"objects",objects},{"collections",collections},{"named_colors",named_colors},{"raster_assets",raster_assets}});
}

std::string export_svg(const Document& d,const Id& comp_id,const Id& art_id) {
    validate(d);
    const auto values=evaluate(d);
    const auto transforms=evaluate_transforms(d,values);
    const bool external_parenting=std::any_of(d.objects.begin(),d.objects.end(),[](const auto& entry){return entry.second.transform_parent.has_value();});

    auto comp=std::find_if(d.compositions.begin(),d.compositions.end(),
        [&](const auto& c){return c.id==comp_id;});
    if(comp==d.compositions.end()) throw Error("MISSING_COMPOSITION",comp_id);

    const auto resolved=evaluate_artboard(*comp,art_id);const auto* art=&resolved;

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out<<std::setprecision(std::numeric_limits<double>::max_digits10);
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<art->width
       <<"px\" height=\""<<art->height<<"px\" viewBox=\""<<art->x<<" "<<art->y
       <<" "<<art->width<<" "<<art->height<<"\">\n";

    std::set<Id> svg_ids;for(const auto& [id,object]:d.objects){svg_ids.insert(id);if(object.compositing.mask)svg_ids.insert(object.compositing.mask->id);}
    std::size_t gradient_serial=0,raster_bytes=0;
    std::map<Id,std::string> raster_ids;
    const auto paint_image=[&](const Id& id) {
        const auto& source=*d.objects.at(id).image;
        if(!raster_ids.contains(source.asset)) {
            const auto png=encode_raster_png(decode_raster(*d.raster_assets.at(source.asset).payload));
            raster_bytes+=png.size();if(raster_bytes>32*1024*1024)throw Error("SVG_RASTER_LIMIT","SVG normalized image data exceeds 32 MiB");
            std::string key;do{key="nect-raster-"+std::to_string(++gradient_serial);}while(svg_ids.contains(key));svg_ids.insert(key);raster_ids.emplace(source.asset,key);
            out<<"<defs><image id=\""<<key<<"\" width=\"1\" height=\"1\" preserveAspectRatio=\"none\" href=\"data:image/png;base64,"<<base64_encode(png)<<"\"/></defs>\n";
        }
        out<<"<use href=\"#"<<raster_ids.at(source.asset)<<"\" transform=\"scale("<<values.at({id,"","image.width"})<<' '<<values.at({id,"","image.height"})<<")\"/>\n";
    };

    const auto paint_shape=[&](const EvaluatedShape& shape) {
            for(const auto& paint:shape.paints) {
                const bool fill=paint.type=="nect.paint.fill";
                std::string gradient_id;
                if(paint.gradient) {
                    do{gradient_id="nect-gradient-"+std::to_string(++gradient_serial);}while(svg_ids.contains(gradient_id));svg_ids.insert(gradient_id);
                    const auto& g=*paint.gradient;const bool linear=g.type=="linear";
                    out<<"<defs><"<<(linear?"linearGradient":"radialGradient")<<" id=\""<<gradient_id
                       <<"\" gradientUnits=\"userSpaceOnUse\" spreadMethod=\"pad\" color-interpolation=\"sRGB\" ";
                    if(linear)out<<"x1=\""<<g.start.x<<"\" y1=\""<<g.start.y<<"\" x2=\""<<g.end.x<<"\" y2=\""<<g.end.y<<"\"";
                    else out<<"cx=\""<<g.start.x<<"\" cy=\""<<g.start.y<<"\" r=\""<<std::hypot(g.end.x-g.start.x,g.end.y-g.start.y)<<"\"";
                    out<<">\n";
                    for(const auto& stop:g.stops)out<<"<stop offset=\""<<stop.offset<<"\" stop-color=\"rgb("
                        <<stop.rgba[0]*100<<"%,"<<stop.rgba[1]*100<<"%,"<<stop.rgba[2]*100<<"%)\" stop-opacity=\""<<stop.rgba[3]<<"\"/>\n";
                    out<<"</"<<(linear?"linearGradient":"radialGradient")<<"></defs>\n";
                }
                out<<"<path transform=\"matrix(";for(const auto n:paint.transform)out<<n<<' ';
                out<<")\" ";
                if(fill)out<<"stroke=\"none\" fill-rule=\""<<paint.fill_rule<<"\" fill=\"";
                else out<<"fill=\"none\" stroke-linecap=\"butt\" stroke-linejoin=\"miter\" stroke-miterlimit=\"4\" stroke=\"";
                if(paint.gradient)out<<"url(#"<<gradient_id<<")\" ";
                else out<<"rgb("<<paint.rgba[0]*100<<"%,"<<paint.rgba[1]*100<<"%,"<<paint.rgba[2]*100<<"%)\" ";
                out<<(fill?"fill-opacity":"stroke-opacity")<<"=\""<<paint.rgba[3]<<"\" ";
                if(!fill)out<<"stroke-width=\""<<paint.width<<"\" ";
                out<<"d=\"";
                for(const auto& instance:paint.paths)for(const auto& c:*instance.contours) {
                    if(c.points.empty())continue;
                    const auto first=map_point(instance.transform,c.points.front().anchor);
                    out<<"M "<<first.x<<' '<<first.y<<' ';
                    const auto segments=c.closed?c.points.size():c.points.size()-1;
                    for(std::size_t i=0;i<segments;++i) {
                        const auto& p=c.points[i];const auto& q=c.points[(i+1)%c.points.size()];
                        const auto a=map_point(instance.transform,p.outgoing),b=map_point(instance.transform,q.incoming),end=map_point(instance.transform,q.anchor);
                        out<<"C "<<a.x<<' '<<a.y<<' '<<b.x<<' '<<b.y<<' '<<end.x<<' '<<end.y<<' ';
                    }
                    if(c.closed)out<<"Z ";
                }
                out<<"\"/>\n";
            }
    };
    // Neutral legacy scenes keep their original local-transform projection.
    // Appearance scopes use world-space clip wrappers, never inverse matrices.
    bool modern=false;
    std::function<void(const Id&)> detect=[&](const Id& id){const auto& object=d.objects.at(id);const auto& c=object.compositing;
        modern=modern||object.image.has_value()||!object.visible||values.at({id,"","composite.opacity"})!=1||c.blend!="normal"||c.isolated||(c.mask&&c.mask->enabled);
        for(const auto& child:object.children)detect(child);};
    for(const auto& id:comp->roots)detect(id);
    if(modern) {
        const auto scene=evaluate_scene(d,comp_id,values,transforms);
        std::function<void(const EvaluatedSceneNode&)> render_node=[&](const EvaluatedSceneNode& node) {
            if(!node.visible)return;
            const auto& object=d.objects.at(node.id);
            if(node.mask) {
                const auto& mask=*node.mask;const auto& mask_id=object.compositing.mask->id;
                out<<"<defs><clipPath id=\""<<mask_id<<"\" clipPathUnits=\"userSpaceOnUse\"><path clip-rule=\""<<mask.fill_rule<<"\" d=\"";
                for(const auto& instance:mask.paths)for(const auto& contour:*instance.contours) {
                    if(contour.points.empty())continue;
                    const auto first=map_point(instance.transform,contour.points.front().anchor);out<<"M "<<first.x<<' '<<first.y<<' ';
                    const auto segments=contour.closed?contour.points.size():contour.points.size()-1;
                    for(std::size_t i=0;i<segments;++i){const auto& p=contour.points[i];const auto& q=contour.points[(i+1)%contour.points.size()];
                        const auto a=map_point(instance.transform,p.outgoing),b=map_point(instance.transform,q.incoming),end=map_point(instance.transform,q.anchor);
                        out<<"C "<<a.x<<' '<<a.y<<' '<<b.x<<' '<<b.y<<' '<<end.x<<' '<<end.y<<' ';}
                    out<<"Z ";
                }
                out<<"\"/></clipPath></defs>\n";
            }
            out<<"<g id=\""<<node.id<<"\" opacity=\""<<node.opacity<<"\" color-interpolation=\"sRGB\" style=\"isolation:"<<(node.isolated?"isolate":"auto")<<";mix-blend-mode:"<<node.blend<<"\"";
            if(node.mask)out<<" clip-path=\"url(#"<<object.compositing.mask->id<<")\"";
            out<<"><title>"<<escape(object.name)<<"</title>\n";
            if(object.kind==Kind::group)for(const auto& child:node.children)render_node(child);
            else {
                if(object.text)out<<"<desc>Text outlined for SVG; editable source remains in native Nect.</desc>\n";
                out<<"<g transform=\"matrix(";for(const auto value:node.world)out<<value<<' ';out<<")\">\n";
                if(object.image)paint_image(node.id);else paint_shape(scene.shapes.at(node.id));out<<"</g>\n";
            }
            out<<"</g>\n";
        };
        for(const auto& node:scene.roots)render_node(node);out<<"</svg>\n";return out.str();
    }

    std::function<void(const Id&)> render=[&](const Id& id) {
        const auto& o=d.objects.at(id);
        out<<"<g id=\""<<id<<"\"";
        // Structure preserves order/names; only leaves project world transforms.
        // This also represents externally parented children under singular groups.
        if(!external_parenting||o.kind!=Kind::group) {
            out<<" transform=\"matrix(";
            for(const auto v:external_parenting?transforms.at(id).world:transforms.at(id).local)out<<v<<' ';
            out<<")\"";
        }
        out<<"><title>"<<escape(o.name)<<"</title>\n";
        if(o.text)out<<"<desc>Text outlined for SVG; editable text and font references remain in the native Nect document.</desc>\n";

        if(o.kind==Kind::group) {
            for(const auto& child:o.children) render(child);
        } else {
            paint_shape(evaluate_shape(d,id,values));
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
        const bool mutation=op=="apply"||op=="undo"||op=="redo"||op=="restore_history";
        Document prior;
        std::map<Ref,double> prior_values;
        std::map<Id,EvaluatedTransform> prior_transforms;
        std::map<Id,j::value> prior_frames;
        if(mutation) {
            prior=session.document();prior_values=evaluate(session.document());
            prior_transforms=evaluate_transforms(session.document(),prior_values);
            for(const auto& c:session.document().compositions)for(const auto& a:c.artboards)
                prior_frames.emplace(a.id,j::object{{"authored",artboard_json(a)},{"evaluated",artboard_json(evaluate_artboard(c,a.id))}});
        }

        if(op=="get") {
            keys(o,{"op","ref"});
            auto r=read_ref(o.at("ref"));
            if(r.point.empty()&&(r.field=="color"||r.field.ends_with(".color")))result=color_property_json(session.document(),r,evaluate(session.document()));
            else {
            const auto origin=property_origin(session.document(),r);
            result=j::object{
                {"ref",ref_json(r)},
                {"origin",origin},
                {"authored",origin=="generated"?j::value(nullptr):scalar_json(property(session.document(),r))},
                {"evaluated",evaluate(session.document()).at(r)}};
            }
        } else if(op=="inspect") {
            keys(o,{"op"});
            result=j::parse(encode(session.document()),{},precise_json_options());
        } else if(op=="properties") {
            keys(o,{"op"});
            j::array list;
            const auto values=evaluate(session.document());
            for(const auto& ref:properties(session.document())) {
                const auto origin=property_origin(session.document(),ref);
                list.push_back({{"ref",ref_json(ref)},
                    {"name",property_name(session.document(),ref)},
                    {"type","number"},{"unit",property_unit(ref)},{"space","local"},
                    {"origin",origin},{"authored",origin=="generated"?j::value(nullptr):scalar_json(property(session.document(),ref))},
                    {"evaluated",values.at(ref)}});
            }
            for(const auto& ref:color_properties(session.document()))list.push_back(color_property_json(session.document(),ref,values));
            result=std::move(list);
        } else if(op=="color_properties") {
            keys(o,{"op"});j::array list;const auto values=evaluate(session.document());
            for(const auto& ref:color_properties(session.document()))list.push_back(color_property_json(session.document(),ref,values));
            result=std::move(list);
        } else if(op=="used_colors") {
            keys(o,{"op"});const auto& d=session.document();const auto values=evaluate(d);
            std::map<std::array<double,4>,std::vector<Ref>> grouped;
            for(const auto& ref:color_properties(d))if(color_is_used(d,ref))grouped[color_value(d,ref,values).rgba].push_back(ref);
            j::array inventory;for(const auto& [rgba,refs]:grouped) {
                j::array uses;for(const auto& ref:refs)uses.push_back(ref_json(ref));ColorValue value;value.rgba=rgba;
                inventory.push_back({{"value",color_json(value)},{"uses",uses},{"count",refs.size()}});
            }
            result=j::object{{"scope","enabled_paint_inputs"},{"equal_values_imply_link",false},{"colors",inventory}};
        } else if(op=="conversion_plan") {
            keys(o,{"op","object"});const auto id=text(o.at("object"));
            j::array blockers;for(const auto& ref:conversion_blockers(session.document(),id))blockers.push_back(ref_json(ref));
            const auto& object=session.document().objects.at(id);
            result=j::object{{"object",id},{"allowed",blockers.empty()},{"blockers",blockers},
                {"source_instance",object.source->id},{"preserves_point_ids",true},
                {"freezes_generator",true},{"preserves_active_point_bindings",true},
                {"discards_bypassed_corrections",object.point_edit&&!object.point_edit->enabled}};
        } else if(op=="artboards") {
            keys(o,{"op","composition"});const auto id=text(o.at("composition"));
            const auto& comps=session.document().compositions;
            const auto comp=std::find_if(comps.begin(),comps.end(),[&](const auto& c){return c.id==id;});
            if(comp==comps.end())throw Error("MISSING_COMPOSITION",id);
            j::array frames;for(const auto& a:comp->artboards)frames.push_back(j::object{
                {"authored",artboard_json(a)},{"evaluated",artboard_json(evaluate_artboard(*comp,a.id))}});
            result=std::move(frames);
        } else if(op=="text_defaults") {
            keys(o,{"op"});result=text_json(default_text("new-text-source"));
        } else if(op=="text_fonts") {
            keys(o,{"op"});result=ids_json(text_fonts());
        } else if(op=="text_layout") {
            keys(o,{"op","object"});result=text_layout_json(session.document(),text(o.at("object")));
        } else if(op=="export_plan") {
            keys(o,{"op","composition","artboard"});const auto cid=text(o.at("composition"));
            const auto& d=session.document();const auto comp=std::find_if(d.compositions.begin(),d.compositions.end(),[&](const auto& c){return c.id==cid;});
            if(comp==d.compositions.end())throw Error("MISSING_COMPOSITION",cid);
            const auto board=evaluate_artboard(*comp,text(o.at("artboard")));j::array texts;
            std::function<void(const Id&)> walk=[&](const Id& id){const auto& object=d.objects.at(id);
                if(object.text)texts.push_back(text_layout_json(d,id));for(const auto& child:object.children)walk(child);};
            for(const auto& id:comp->roots)walk(id);
            result=j::object{{"format","svg"},{"artboard",artboard_json(board)},{"text",texts},
                {"property_policy","evaluated_values"},{"expressions_preserved",false},
                {"shape_policy","evaluated vector contours; live operators preserved only in native"},
                {"image_policy","accepted snapshot projected to oriented sRGB PNG in SVG; original bytes and links preserved in native"},
                {"image_limits",j::object{{"normalized_png_bytes",raster_png_limit},{"aggregate_png_bytes",32*1024*1024}}},
                {"compositing_policy","vector_geometry_clips_group_opacity_css_blend_and_isolation"},{"blend_reader_requirement","SVG CSS mix-blend-mode and isolation support"},
                {"text_policy","outlines"},{"native_source_preserved",true},{"fonts_embedded",false}};
        } else if(op=="compositing_types") {
            keys(o,{"op"});
            result=j::object{{"version",1},{"space","srgb"},{"alpha","source-over premultiplied compositing"},
                {"blends",j::array{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn","hard-light","soft-light","difference","exclusion"}},
                {"mask","final_path_geometry"},{"mask_sources",j::array{"path","text"}},{"mask_space","composition"},
                {"mask_paint_ignored",true},{"mask_open_contours","implicitly_closed"},{"mask_normal_visibility","independent"},
                {"neutral_groups","pass_through"},{"nonneutral_groups","isolated_then_clip_opacity_blend"},{"after_effects_full_parity",false}};
        } else if(op=="compositing_plan") {
            keys(o,{"op","composition"});const auto values=evaluate(session.document());
            const auto scene=evaluate_scene(session.document(),text(o.at("composition")),values,evaluate_transforms(session.document(),values));
            std::function<j::value(const EvaluatedSceneNode&)> node_json=[&](const EvaluatedSceneNode& node) {
                j::array children,world;for(const auto value:node.world)world.push_back(value);for(const auto& child:node.children)children.push_back(node_json(child));
                j::value mask=nullptr;if(node.mask)mask=j::object{{"source",node.mask->source},{"fill_rule",node.mask->fill_rule},{"space","composition"},{"path_instances",node.mask->paths.size()}};
                return j::object{{"object",node.id},{"world",world},{"visible",node.visible},{"opacity",node.opacity},{"blend",node.blend},{"isolated",node.isolated},{"mask",mask},{"children",children}};
            };
            j::array roots;for(const auto& node:scene.roots)roots.push_back(node_json(node));
            result=j::object{{"roots",roots},{"requires_compositing",scene.requires_compositing},{"backdrop","transparent"}};
        } else if(op=="expression_language") {
            keys(o,{"op"});
            result=j::object{{"version",1},{"reference","ref(\"object-id\",\"point-id-or-empty\",\"field\")"},
                {"operators",j::array{"+","-","*","/"}},{"functions",j::array{"abs","min","max","clamp","floor","ceil","round","sin","cos","sqrt"}},
                {"trigonometry","degrees"},{"source_bytes",4096},{"nodes",256},{"depth",32},{"references",64},
                {"units","Addition requires compatible units; multiplication needs a dimensionless factor; division needs a dimensionless divisor or equal units. Literal-only terms adopt context. sqrt is dimensionless."},
                {"effects","none"},{"drafts","UI-only until an atomic set_expression command succeeds"}};
        } else if(op=="primitive_types") {
            keys(o,{"op"});j::array definitions;
            for(const auto* type:{"nect.shape.circle","nect.shape.rectangle","nect.shape.polygon","nect.shape.star"}) {
                const auto source=default_primitive("new-source",type);
                definitions.push_back(j::object{{"type",type},{"version",1},{"template",primitive_json(source)},
                    {"point_edit","absolute_local_override"},{"topology_change","reject_unmapped_corrections_or_references"}});
            }
            result=std::move(definitions);
        } else if(op=="operator_types") {
            keys(o,{"op"});j::array definitions;
            for(const auto* type:{"nect.paint.fill","nect.paint.stroke","nect.shape.repeater","nect.shape.offset"}) {
                auto defaults=default_operation("new-operation",type);
                j::array parameters;for(const auto& [name,scalar]:defaults.parameters)
                    parameters.push_back({{"name",name},{"unit",property_unit(operation_ref("object",defaults.id,name))},{"default",scalar.literal}});
                j::object definition{{"type",type},{"version",1},{"input","local_paths_and_paint"},
                    {"output","local_paths_and_paint"},{"bypass","preserve_input"},
                    {"parameters",parameters},{"template",operation_json(defaults)}};
                if(defaults.type=="nect.shape.offset") {
                    definition["space"]="object-local after preceding path operations";
                    definition["scope"]="current paths and earlier paint geometry; paint coordinate bases retained";
                    definition["joins"]=j::array{"miter","round","bevel"};
                    definition["geometry"]="closed simple contours; nested/disjoint rings; nonzero/evenodd; separate instances are not unioned";
                    definition["unsupported"]=j::array{"open paths","self-intersections","touching/intersecting contour boundaries"};
                    definition["curve_flattening_tolerance_du"]=0.1;
                    definition["zero_amount"]="exact input; no geometry conversion";
                }
                definitions.push_back(std::move(definition));
            }
            result=std::move(definitions);
        } else if(op=="gradient_types") {
            keys(o,{"op"});j::array definitions;
            for(const auto* type:{"linear","radial"}) {
                Gradient g;g.id="new-gradient";g.type=type;
                GradientStop first;first.id="start-stop";GradientStop last;last.id="end-stop";last.offset.literal=1;
                last.rgba[0].literal=last.rgba[1].literal=last.rgba[2].literal=1;g.stops={first,last};
                definitions.push_back({{"type",type},{"version",1},{"space","local"},{"spread","pad"},
                    {"interpolation","srgb_components"},{"min_stops",2},{"max_stops",64},{"distinct_offsets",true},
                    {"template",gradient_json(g)}});
            }
            result=std::move(definitions);
        } else if(op=="render_plan") {
            keys(o,{"op","object"});const auto id=text(o.at("object"));
            const auto shape=evaluate_shape(session.document(),id,evaluate(session.document()));
            j::array paints;
            for(const auto& paint:shape.paints) {
                j::array matrix,rgba;for(const auto n:paint.transform)matrix.push_back(n);for(const auto n:paint.rgba)rgba.push_back(n);
                j::object entry{{"operation",paint.operation},{"type",paint.type},{"path_instances",paint.paths.size()},
                    {"transform",matrix},{"rgba",rgba},{"width",paint.width},{"fill_rule",paint.fill_rule}};
                if(paint.gradient) {
                    const auto& g=*paint.gradient;j::array stops;
                    for(const auto& stop:g.stops) {
                        j::array color;for(const auto channel:stop.rgba)color.push_back(channel);
                        stops.push_back({{"offset",stop.offset},{"rgba",color}});
                    }
                    entry["gradient"]=j::object{{"type",g.type},{"start",j::array{g.start.x,g.start.y}},
                        {"end",j::array{g.end.x,g.end.y}},{"stops",stops},{"interpolation","srgb_components"},{"spread","pad"}};
                }
                paints.push_back(std::move(entry));
            }
            result=j::object{{"path_instances",shape.paths.size()},{"paint_layers",paints}};
        } else if(op=="assets") {
            keys(o,{"op"});j::array list;
            for(const auto& [id,asset]:session.document().raster_assets) {
                auto entry=asset_json(asset);entry["source_bytes"]=asset.payload->bytes().size();j::array placements;
                for(const auto& [object_id,object]:session.document().objects)if(object.image&&object.image->asset==id)placements.push_back(j::value(object_id));
                entry["placements"]=placements;list.push_back(std::move(entry));
            }
            result=list;
        } else if(op=="capabilities") {
            keys(o,{"op"});
            result=j::object{
                {"native_version",native_version},
                {"transport","local-json-lines-not-mcp"},
                {"mcp",false},
                {"ai_codec",false},
                {"gui",false},
                {"expression_subset","bounded arithmetic and stable-ID references v1; see expression_language"}};
        } else if(op=="resolve_name") {
            keys(o,{"op","name","point","field"});
            result=ref_json(resolve_name(
                session.document(),text(o.at("name")),text(o.at("point")),text(o.at("field"))));
        } else if(op=="apply") {
            keys(o,{"op","expected_revision","commands"});
            std::vector<Command> cmds;
            for(const auto& v:o.at("commands").as_array()) cmds.push_back(read_command(v));
            apply_serializable(session,cmds,j::value_to<std::uint64_t>(o.at("expected_revision")));
            result=j::object{{"changed",true}};
        } else if(op=="history") {
            keys(o,{"op"});const auto history=session.history();j::array states;
            for(const auto& state:history.states)states.push_back(j::object{{"id",state.id},{"label",state.label},
                {"estimated_bytes",state.estimated_bytes},{"current",state.id==history.current_id}});
            result=j::object{{"states",states},{"current_id",history.current_id},{"retained_bytes",history.retained_bytes},
                {"max_entries",history.max_entries},{"max_bytes",history.max_bytes},{"pruned_entries",history.pruned_entries},
                {"memory_measure","conservative_retained_payload_estimate"},{"scope","current_session"},{"cross_restart",false}};
        } else if(op=="restore_history") {
            keys(o,{"op","expected_revision","state_id"});const auto before=session.revision();
            session.restore_history(j::value_to<std::uint64_t>(o.at("state_id")),j::value_to<std::uint64_t>(o.at("expected_revision")));
            result=j::object{{"changed",before!=session.revision()}};
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
        } else if(op=="transforms") {
            keys(o,{"op"});const auto values=evaluate(session.document());j::array list;
            for(const auto& [id,transform]:evaluate_transforms(session.document(),values)) {
                j::array local,world;for(const auto v:transform.local)local.push_back(v);for(const auto v:transform.world)world.push_back(v);
                const auto x=values.at({id,"","transform.anchor_x"}),y=values.at({id,"","transform.anchor_y"});
                const auto position=map_point(transform.local,{x,y});const auto world_anchor=map_point(transform.world,{x,y});
                list.push_back(j::object{{"object",id},{"effective_parent",transform.effective_parent},{"local",local},{"world",world},
                    {"anchor",j::array{x,y}},{"position",j::array{position.x,position.y}},{"world_anchor",j::array{world_anchor.x,world_anchor.y}}});
            }
            result=list;
        } else if(op=="export_svg") {
            keys(o,{"op","composition","artboard"});
            result=export_svg(session.document(),text(o.at("composition")),text(o.at("artboard")));
        } else {
            throw Error("UNSUPPORTED_OPERATION",op);
        }

        if(mutation) {
            std::set<Id> changed;
            const auto& after=session.document();
            const auto map_diff=[&](const auto& old,const auto& current) {
                for(const auto& [id,value]:current)if(!old.contains(id)||old.at(id)!=value)changed.insert(id);
                for(const auto& [id,value]:old){(void)value;if(!current.contains(id))changed.insert(id);}
            };
            map_diff(prior.objects,after.objects);map_diff(prior.named_colors,after.named_colors);map_diff(prior.raster_assets,after.raster_assets);
            const auto list_diff=[&](const auto& old,const auto& current) {
                for(const auto& item:current) {const auto found=std::find_if(old.begin(),old.end(),[&](const auto& x){return x.id==item.id;});if(found==old.end()||*found!=item)changed.insert(item.id);}
                for(const auto& item:old)if(std::none_of(current.begin(),current.end(),[&](const auto& x){return x.id==item.id;}))changed.insert(item.id);
            };
            list_diff(prior.compositions,after.compositions);list_diff(prior.collections,after.collections);
            for(const auto& [id,object]:after.objects)if(object.image&&changed.contains(object.image->asset))changed.insert(id);
            const auto current_values=evaluate(session.document());
            for(const auto& [ref,value]:current_values)
                if(!prior_values.contains(ref)||prior_values.at(ref)!=value)changed.insert(ref.object);
            for(const auto& [id,transform]:evaluate_transforms(session.document(),current_values))
                if(!prior_transforms.contains(id)||prior_transforms.at(id).world!=transform.world)changed.insert(id);
            for(const auto& c:session.document().compositions)for(const auto& a:c.artboards) {
                const j::value frame=j::object{{"authored",artboard_json(a)},{"evaluated",artboard_json(evaluate_artboard(c,a.id))}};
                if(!prior_frames.contains(a.id)||prior_frames.at(a.id)!=frame)changed.insert(a.id);
                prior_frames.erase(a.id);
            }
            for(const auto& [id,frame]:prior_frames){(void)frame;changed.insert(id);}
            for(const auto& [id,object]:session.document().objects)
                if(object.compositing.mask&&object.compositing.mask->enabled&&changed.contains(object.compositing.mask->source))changed.insert(id);
            result.as_object()["changed_ids"]=ids_json(std::vector<Id>(changed.begin(),changed.end()));
            j::array created;for(const auto& [id,object]:after.objects){(void)object;if(!prior.objects.contains(id))created.push_back(j::value(id));}
            result.as_object()["created_ids"]=std::move(created);
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
