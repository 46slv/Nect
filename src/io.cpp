#include "nect/io.hpp"
#include "nect/blend.hpp"
#include "compatibility.hpp"
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
void keys(const j::object& o,const std::vector<std::string_view>& allowed) {
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
std::int64_t signed_integer(const j::value& v) {
    return j::value_to<std::int64_t>(v);
}
std::uint32_t uint32_number(const j::value& value,std::string_view field) {
    std::uint64_t result=0;
    if(value.is_uint64())result=value.as_uint64();
    else if(value.is_int64()) {
        const auto signed_value=value.as_int64();
        if(signed_value<0)throw Error("INVALID_TEXT_FONT_FEATURE",std::string(field)+" must be an integer from 0 through 4294967295");
        result=static_cast<std::uint64_t>(signed_value);
    } else throw Error("INVALID_TEXT_FONT_FEATURE",std::string(field)+" must be an integer from 0 through 4294967295");
    if(result>std::numeric_limits<std::uint32_t>::max())
        throw Error("INVALID_TEXT_FONT_FEATURE",std::string(field)+" must be an integer from 0 through 4294967295");
    return static_cast<std::uint32_t>(result);
}
std::uint64_t preset_unsigned(const j::value& value,std::uint64_t maximum,std::string_view field) {
    std::uint64_t result=0;
    if(value.is_uint64())result=value.as_uint64();
    else if(value.is_int64()) {
        const auto signed_value=value.as_int64();
        if(signed_value<0)throw Error("INVALID_PRESET_NUMBER",std::string(field)+" must be a positive integer");
        result=static_cast<std::uint64_t>(signed_value);
    } else throw Error("INVALID_PRESET_NUMBER",std::string(field)+" must be an integer number");
    if(result==0||result>maximum)throw Error("INVALID_PRESET_NUMBER",std::string(field)+" is outside the supported positive integer range");
    return result;
}
std::uint64_t macro_unsigned(const j::value& value,std::uint64_t maximum,std::string_view field,bool allow_zero=false) {
    std::uint64_t result=0;
    if(value.is_uint64())result=value.as_uint64();
    else if(value.is_int64()) {
        const auto signed_value=value.as_int64();
        if(signed_value<0)throw Error("INVALID_MACRO_COMMAND",std::string(field)+" must be a nonnegative integer");
        result=static_cast<std::uint64_t>(signed_value);
    } else throw Error("INVALID_MACRO_COMMAND",std::string(field)+" must be an integer JSON number");
    if((!allow_zero&&result==0)||result>maximum)
        throw Error("INVALID_MACRO_COMMAND",std::string(field)+" is outside its supported integer range");
    return result;
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
GeometryMask read_mask(const j::value& value,bool allow_enabled_driver=false,bool allow_enabled_expression=false,
    bool allow_mode_fields=false,bool require_mode_fields=false,
    bool allow_mask_color_space=false,bool require_mask_color_space=false) {
    const auto& o=value.as_object();
    if(!allow_enabled_driver&&o.contains("enabled_driver"))
        throw Error("UNSUPPORTED_MASK_ENABLED_DRIVER","Geometry mask enabled drivers require native 0.31 and the dedicated link command");
    if(!allow_enabled_expression&&o.contains("enabled_expression"))
        throw Error("UNSUPPORTED_MASK_ENABLED_EXPRESSION","Geometry mask enabled expressions require native 0.68 and the dedicated expression command");
    if(require_mode_fields&&(!o.contains("mode")||!o.contains("invert")))
        throw Error("NATIVE_VERSION_MISMATCH","Native 0.71 masks require explicit mode and invert fields");
    if(require_mask_color_space&&!o.contains("mask_color_space"))
        throw Error("NATIVE_VERSION_MISMATCH","Native 0.72 masks require an explicit mask_color_space");
    if(allow_mask_color_space) {
        if(allow_enabled_expression)keys(o,{"id","source","version","enabled","fill_rule","enabled_driver","enabled_expression","mode","invert","mask_color_space"});
        else if(allow_enabled_driver)keys(o,{"id","source","version","enabled","fill_rule","enabled_driver","mode","invert","mask_color_space"});
        else keys(o,{"id","source","version","enabled","fill_rule","mode","invert","mask_color_space"});
    } else if(allow_mode_fields) {
        if(allow_enabled_expression)keys(o,{"id","source","version","enabled","fill_rule","enabled_driver","enabled_expression","mode","invert"});
        else if(allow_enabled_driver)keys(o,{"id","source","version","enabled","fill_rule","enabled_driver","mode","invert"});
        else keys(o,{"id","source","version","enabled","fill_rule","mode","invert"});
    } else if(allow_enabled_expression)keys(o,{"id","source","version","enabled","fill_rule","enabled_driver","enabled_expression"});
    else if(allow_enabled_driver)keys(o,{"id","source","version","enabled","fill_rule","enabled_driver"});
    else keys(o,{"id","source","version","enabled","fill_rule"});
    GeometryMask result{text(o.at("id")),text(o.at("source")),j::value_to<unsigned>(o.at("version")),o.at("enabled").as_bool(),text(o.at("fill_rule"))};
    if(allow_mode_fields) {
        if(const auto* mode=o.if_contains("mode"))result.mode=text(*mode);
        if(const auto* invert=o.if_contains("invert"))result.invert=invert->as_bool();
    }
    if(allow_mask_color_space)if(const auto* color_space=o.if_contains("mask_color_space"))
        result.mask_color_space=text(*color_space);
    if(const auto* driver=o.if_contains("enabled_driver")) {
        const auto& wrapper=driver->as_object();keys(wrapper,{"link"});result.enabled_driver=read_ref(wrapper.at("link"));
    }
    if(const auto* expression=o.if_contains("enabled_expression"))result.enabled_expression=read_expression(*expression);
    if(result.enabled_driver&&result.enabled_expression)
        throw Error("INVALID_MASK_ENABLED_SOURCE","Geometry mask enabled link and expression are mutually exclusive");
    return result;
}
j::value mask_json(const std::optional<GeometryMask>& mask) {
    if(!mask)return nullptr;
    j::object result{{"id",mask->id},{"source",mask->source},{"version",mask->version},{"enabled",mask->enabled},
        {"fill_rule",mask->fill_rule},{"mode",mask->mode},{"invert",mask->invert},
        {"mask_color_space",mask->mask_color_space}};
    if(mask->enabled_driver)result["enabled_driver"]=j::object{{"link",ref_json(*mask->enabled_driver)}};
    if(mask->enabled_expression)result["enabled_expression"]=expression_json(*mask->enabled_expression);
    return result;
}
j::object blend_descriptor_json(const BlendModeDescriptor& descriptor) {
    j::array profiles;
    for(const auto& profile:descriptor.profiles)profiles.push_back(j::object{{"id",profile.id},
        {"channel_bits",profile.channel_bits},{"alpha_bits",profile.alpha_bits},{"color_space",profile.color_space},
        {"alpha_representation",profile.alpha_representation},{"intermediate_precision",profile.intermediate_precision},
        {"quantization",profile.quantization}});
    return j::object{{"id",descriptor.id},{"label",descriptor.label},{"family",descriptor.family},
        {"behavior_version",descriptor.behavior_version},{"color_operation",descriptor.color_operation},
        {"alpha_behavior",descriptor.alpha_behavior},{"backdrop_scope",descriptor.backdrop_scope},
        {"time_dependency",descriptor.time_dependency},{"profiles",profiles},{"ae_oracle_status",descriptor.ae_oracle_status},
        {"introduced_native_version","0."+std::to_string(descriptor.introduced_native_minor)},
        {"renderer",descriptor.renderer},{"svg",j::object{{"representation",descriptor.svg_representation},
            {"reader_requirement",descriptor.svg_reader_requirement},{"cross_reader_pixel_identity",false},
            {"native_source_preserved",true},{"nect_svg_intake_supported",false}}}};
}
j::array unsupported_svg_blends(const EvaluatedScene& scene) {
    j::array result;
    std::function<void(const EvaluatedSceneNode&)> walk=[&](const auto& node) {
        if(!node.visible||node.opacity<=0)return;
        const auto* descriptor=find_blend_mode(node.blend);
        if(!descriptor||descriptor->svg_representation!="css-mix-blend-mode")
            result.push_back(j::object{{"object",node.id},{"blend",node.blend},
                {"reason","No standard native CSS blend or implemented lossless backdrop-aware projection"}});
        for(const auto& child:node.children)walk(child);
    };
    for(const auto& node:scene.roots)walk(node);
    return result;
}
j::array blend_descriptors_json() {
    j::array result;for(const auto& descriptor:blend_modes())result.push_back(blend_descriptor_json(descriptor));return result;
}
Compositing read_compositing(const j::value& value,bool allow_isolated_driver,bool allow_mask_enabled_driver=false,
    bool allow_isolated_expression=false,bool allow_mask_enabled_expression=false,
    bool allow_mask_mode=false,bool require_mask_mode=false,
    bool allow_mask_color_space=false,bool require_mask_color_space=false) {
    const auto& o=value.as_object();
    if(allow_isolated_expression)keys(o,{"version","opacity","blend","isolated","mask","isolated_driver","isolated_expression"});
    else if(allow_isolated_driver)keys(o,{"version","opacity","blend","isolated","mask","isolated_driver"});
    else keys(o,{"version","opacity","blend","isolated","mask"});
    Compositing c;c.version=j::value_to<unsigned>(o.at("version"));c.opacity=read_scalar(o.at("opacity"));
    c.blend=text(o.at("blend"));c.isolated=o.at("isolated").as_bool();
    if(!o.at("mask").is_null())c.mask=read_mask(o.at("mask"),allow_mask_enabled_driver,allow_mask_enabled_expression,
        allow_mask_mode,require_mask_mode,allow_mask_color_space,require_mask_color_space);
    if(const auto* driver=o.if_contains("isolated_driver")) {
        const auto& wrapper=driver->as_object();keys(wrapper,{"link"});c.isolated_driver=read_ref(wrapper.at("link"));
    }
    if(allow_isolated_expression)if(const auto* expression=o.if_contains("isolated_expression"))
        c.isolated_expression=read_expression(*expression);
    if(c.isolated_driver&&c.isolated_expression)
        throw Error("INVALID_COMPOSITE_ISOLATION_SOURCE","Composite isolation link and expression are mutually exclusive");
    return c;
}
j::object compositing_json(const Compositing& c) {
    j::object result{{"version",c.version},{"opacity",scalar_json(c.opacity)},{"blend",c.blend},{"isolated",c.isolated},{"mask",mask_json(c.mask)}};
    if(c.isolated_driver)result["isolated_driver"]=j::object{{"link",ref_json(*c.isolated_driver)}};
    if(c.isolated_expression)result["isolated_expression"]=expression_json(*c.isolated_expression);
    return result;
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

TextItalicDriver read_text_italic_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")) {keys(driver,{"link"});return TextItalicDriver{read_ref(driver.at("link"))};}
    if(driver.contains("expression")) {keys(driver,{"expression"});return TextItalicDriver{read_expression(driver.at("expression"))};}
    throw Error("INVALID_TEXT_ITALIC_DRIVER","Text italic driver requires exactly one link or expression");
}
j::object text_italic_driver_json(const TextItalicDriver& driver) {
    if(const auto* link=std::get_if<Ref>(&driver))return {{"link",ref_json(*link)}};
    return {{"expression",expression_json(std::get<Expression>(driver))}};
}
TextWeightDriver read_text_weight_driver(const j::value& value,bool allow_offset=true) {
    const auto& driver=value.as_object();
    if(driver.contains("link")) {
        if(allow_offset)keys(driver,{"link","offset"});else keys(driver,{"link"});
        return TextWeightDriver{read_ref(driver.at("link")),driver.contains("offset")?signed_integer(driver.at("offset")):0};
    }
    throw Error("INVALID_TEXT_WEIGHT_DRIVER","Text weight driver requires one link");
}
j::object text_weight_driver_json(const TextWeightDriver& driver) {
    j::object result{{"link",ref_json(driver.link)}};
    if(driver.offset!=0)result["offset"]=driver.offset;
    return result;
}
TextContentDriver read_text_content_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")){keys(driver,{"link"});return TextContentDriver{read_ref(driver.at("link"))};}
    throw Error("INVALID_TEXT_CONTENT_DRIVER","Text content driver requires one link");
}
j::object text_content_driver_json(const TextContentDriver& driver) {
    return {{"link",ref_json(driver.link)}};
}
TextFamilyDriver read_text_family_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")){keys(driver,{"link"});return TextFamilyDriver{read_ref(driver.at("link"))};}
    throw Error("INVALID_TEXT_FAMILY_DRIVER","Text family driver requires one link");
}
j::object text_family_driver_json(const TextFamilyDriver& driver) {
    return {{"link",ref_json(driver.link)}};
}
TextLocaleDriver read_text_locale_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")){keys(driver,{"link"});return TextLocaleDriver{read_ref(driver.at("link"))};}
    throw Error("INVALID_TEXT_LOCALE_DRIVER","Text locale driver requires one link");
}
j::object text_locale_driver_json(const TextLocaleDriver& driver) {
    return {{"link",ref_json(driver.link)}};
}
TextDirectionDriver read_text_direction_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")){keys(driver,{"link"});return TextDirectionDriver{read_ref(driver.at("link"))};}
    throw Error("INVALID_TEXT_DIRECTION_DRIVER","Text direction driver requires one link");
}
j::object text_direction_driver_json(const TextDirectionDriver& driver) {
    return {{"link",ref_json(driver.link)}};
}
TextLayoutDriver read_text_layout_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")){keys(driver,{"link"});return TextLayoutDriver{read_ref(driver.at("link"))};}
    throw Error("INVALID_TEXT_LAYOUT_DRIVER","Text layout driver requires one link");
}
j::object text_layout_driver_json(const TextLayoutDriver& driver) {
    return {{"link",ref_json(driver.link)}};
}
TextAlignmentDriver read_text_alignment_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")){keys(driver,{"link"});return TextAlignmentDriver{read_ref(driver.at("link"))};}
    throw Error("INVALID_TEXT_ALIGNMENT_DRIVER","Text alignment driver requires one link");
}
j::object text_alignment_driver_json(const TextAlignmentDriver& driver) {
    return {{"link",ref_json(driver.link)}};
}
FillRuleDriver read_fill_rule_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")){keys(driver,{"link"});return FillRuleDriver{read_ref(driver.at("link"))};}
    throw Error("INVALID_FILL_RULE_DRIVER","Fill rule driver requires one link");
}
j::object fill_rule_driver_json(const FillRuleDriver& driver) {
    return {{"link",ref_json(driver.link)}};
}
TextPathAttachment read_text_path_attachment(const j::value& value) {
    const auto& attachment=value.as_object();keys(attachment,{"path","contour","start_mode","start","spacing","reversed"});
    return {text(attachment.at("path")),text(attachment.at("contour")),text(attachment.at("start_mode")),
        number(attachment.at("start")),number(attachment.at("spacing")),attachment.at("reversed").as_bool()};
}
j::object text_path_attachment_json(const TextPathAttachment& attachment) {
    return {{"path",attachment.path},{"contour",attachment.contour},{"start_mode",attachment.start_mode},
        {"start",attachment.start},{"spacing",attachment.spacing},{"reversed",attachment.reversed}};
}
GroupPathFollowItem read_group_path_follow_item(const j::value& value) {
    const auto& item=value.as_object();keys(item,{"distance","normal_offset","follow_tangent"});
    return {number(item.at("distance")),number(item.at("normal_offset")),item.at("follow_tangent").as_bool()};
}
j::object group_path_follow_item_json(const GroupPathFollowItem& item) {
    return {{"distance",item.distance},{"normal_offset",item.normal_offset},{"follow_tangent",item.follow_tangent}};
}
GroupPathFollow read_group_path_follow(const j::value& value,bool allow_deform=true) {
    const auto& relation=value.as_object();
    if(allow_deform)keys(relation,{"id","path","contour","start_mode","start","normal_offset","reversed","items","mode","deform_axis"});
    else keys(relation,{"id","path","contour","start_mode","start","normal_offset","reversed","items"});
    GroupPathFollow result;result.id=text(relation.at("id"));result.path=text(relation.at("path"));
    result.contour=text(relation.at("contour"));result.start_mode=text(relation.at("start_mode"));
    result.start=number(relation.at("start"));result.normal_offset=number(relation.at("normal_offset"));
    result.reversed=relation.at("reversed").as_bool();
    if(const auto* mode=relation.if_contains("mode"))result.mode=text(*mode);
    if(const auto* axis=relation.if_contains("deform_axis"))result.deform_axis=text(*axis);
    for(const auto& item:relation.at("items").as_object())
        if(!result.items.emplace(std::string(item.key()),read_group_path_follow_item(item.value())).second)
            throw Error("DUPLICATE_TARGET","Duplicate Group Path Follow child Object ID");
    return result;
}
j::object group_path_follow_json(const GroupPathFollow& relation) {
    j::object items;
    for(const auto& [object,item]:relation.items)items[object]=group_path_follow_item_json(item);
    j::object result{{"id",relation.id},{"path",relation.path},{"contour",relation.contour},{"start_mode",relation.start_mode},
        {"start",relation.start},{"normal_offset",relation.normal_offset},{"reversed",relation.reversed},{"items",items}};
    if(relation.mode!="rigid")result["mode"]=relation.mode;
    if(relation.mode=="deform"||relation.deform_axis!="x")result["deform_axis"]=relation.deform_axis;
    return result;
}
TextFontFeature read_text_font_feature(const j::value& value) {
    const auto& object=value.as_object();keys(object,{"feature_tag","parameter","scope"});
    return {text(object.at("feature_tag")),uint32_number(object.at("parameter"),"feature parameter"),text(object.at("scope"))};
}
TextSource read_text(const j::value& v,bool allow_expression=true,bool allow_italic_driver=true,bool allow_weight_driver=true,bool allow_content_driver=true,bool allow_family_driver=true,bool allow_locale_driver=true,bool allow_direction_driver=true,bool allow_layout_driver=true,bool allow_alignment_driver=true,bool allow_path_attachment=true,bool allow_weight_expression=true,bool allow_weight_offset=true,bool allow_font_authoring=true) {
    const auto& o=v.as_object();
    if(!allow_italic_driver&&o.contains("italic_driver"))throw Error("UNSUPPORTED_TEXT_ITALIC_DRIVER","Text italic drivers require native 0.15");
    if(!allow_weight_driver&&o.contains("weight_driver"))throw Error("UNSUPPORTED_TEXT_WEIGHT_DRIVER","Text weight drivers require native 0.16");
    if(!allow_weight_expression&&o.contains("weight_expression"))throw Error("UNSUPPORTED_TEXT_WEIGHT_EXPRESSION","Text weight expressions require native 0.55");
    if(!allow_weight_offset&&o.contains("weight_driver")&&o.at("weight_driver").as_object().contains("offset"))
        throw Error("UNSUPPORTED_TEXT_WEIGHT_OFFSET","Text weight offsets require native 0.61");
    if(!allow_content_driver&&o.contains("content_driver"))throw Error("UNSUPPORTED_TEXT_CONTENT_DRIVER","Text content drivers require native 0.17");
    if(!allow_family_driver&&o.contains("family_driver"))throw Error("UNSUPPORTED_TEXT_FAMILY_DRIVER","Text family drivers require native 0.18");
    if(!allow_locale_driver&&o.contains("locale_driver"))throw Error("UNSUPPORTED_TEXT_LOCALE_DRIVER","Text locale drivers require native 0.22");
    if(!allow_direction_driver&&o.contains("direction_driver"))throw Error("UNSUPPORTED_TEXT_DIRECTION_DRIVER","Text direction drivers require native 0.19");
    if(!allow_layout_driver&&o.contains("layout_driver"))throw Error("UNSUPPORTED_TEXT_LAYOUT_DRIVER","Text layout drivers require native 0.20");
    if(!allow_alignment_driver&&o.contains("alignment_driver"))throw Error("UNSUPPORTED_TEXT_ALIGNMENT_DRIVER","Text alignment drivers require native 0.21");
    if(!allow_path_attachment&&o.contains("path_attachment"))throw Error("UNSUPPORTED_TEXT_PATH_ATTACHMENT","Text path attachments require native 0.24");
    if(!allow_font_authoring&&(o.contains("font_features")||o.contains("additional_axis_values")))
        throw Error("NATIVE_VERSION_MISMATCH","Text font features and additional axes require native 0.78 or later");
    keys(o,{"id","version","content","content_driver","family","family_driver","locale","locale_driver","layout","layout_driver","direction","direction_driver","alignment","alignment_driver","weight","italic","italic_driver","weight_driver","weight_expression","parameters","path_attachment","font_features","additional_axis_values"});
    TextSource s;s.id=text(o.at("id"));s.version=j::value_to<unsigned>(o.at("version"));
    s.content=text(o.at("content"));s.family=text(o.at("family"));s.locale=text(o.at("locale"));
    s.layout=text(o.at("layout"));s.direction=text(o.at("direction"));s.alignment=text(o.at("alignment"));
    s.weight=j::value_to<unsigned>(o.at("weight"));s.italic=o.at("italic").as_bool();
    if(const auto* driver=o.if_contains("italic_driver"))s.italic_driver=read_text_italic_driver(*driver);
    if(const auto* driver=o.if_contains("weight_driver"))s.weight_driver=read_text_weight_driver(*driver,allow_weight_offset);
    if(const auto* expression=o.if_contains("weight_expression"))s.weight_expression=read_expression(*expression);
    if(s.weight_driver&&s.weight_expression)throw Error("TEXT_WEIGHT_SOURCE_CONFLICT","Text weight link and expression are mutually exclusive");
    if(const auto* driver=o.if_contains("content_driver"))s.content_driver=read_text_content_driver(*driver);
    if(const auto* driver=o.if_contains("family_driver"))s.family_driver=read_text_family_driver(*driver);
    if(const auto* driver=o.if_contains("locale_driver"))s.locale_driver=read_text_locale_driver(*driver);
    if(const auto* driver=o.if_contains("direction_driver"))s.direction_driver=read_text_direction_driver(*driver);
    if(const auto* driver=o.if_contains("layout_driver"))s.layout_driver=read_text_layout_driver(*driver);
    if(const auto* driver=o.if_contains("alignment_driver"))s.alignment_driver=read_text_alignment_driver(*driver);
    if(const auto* attachment=o.if_contains("path_attachment"))s.path_attachment=read_text_path_attachment(*attachment);
    for(const auto& p:o.at("parameters").as_object())s.parameters.emplace(std::string(p.key()),read_scalar(p.value(),allow_expression));
    if(const auto* features=o.if_contains("font_features"))
        for(const auto& feature:features->as_array())s.font_features.push_back(read_text_font_feature(feature));
    if(const auto* axes=o.if_contains("additional_axis_values"))
        for(const auto& [tag,value]:axes->as_object())s.additional_axis_values.emplace(std::string(tag),number(value));
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
j::object background_property_json(const Document& d,const Ref& r) {
    if(!r.point.empty()||r.field!="artboard.background")throw Error("INVALID_ARTBOARD_REF","Background requires exact Artboard Ref");
    for(const auto& comp:d.compositions)for(const auto& board:comp.artboards)if(board.id==r.object){
        const auto state=artboard_background_state(comp,board.id);
        return {{"ref",ref_json(r)},{"type","optional_color"},{"link",false},{"expression",false},
            {"authored",j::object{{"value",board.background?color_json(*board.background):j::value(nullptr)},{"overridden",state.overridden}}},
            {"evaluated",state.value?color_json(*state.value):j::value(nullptr)},
            {"inherited",state.inherited},{"source_artboard",state.source_artboard},{"immediate_source_artboard",state.immediate_source_artboard},
            {"mutation","set_artboard_background"},{"render_policy","output_underlay_after_transparent_artwork"}};
    }
    throw Error("MISSING_ARTBOARD",r.object);
}
j::object named_color_json(const NamedColor& color) {
    j::array rgba;for(const auto& scalar:color.rgba)rgba.push_back(scalar_json(scalar));
    return {{"id",color.id},{"name",color.name},{"space","srgb"},{"profile","srgb"},{"alpha","straight"},{"rgba",rgba}};
}
j::object color_property_json(const Document& d,const Ref& ref,const std::map<Ref,double>& values,
    const std::map<Ref,bool>* operation_enabled=nullptr,const std::map<Ref,bool>* gradient_enabled=nullptr) {
    j::array channels,authored;for(const auto& channel:color_channels(d,ref)){channels.push_back(ref_json(channel));authored.push_back(scalar_json(property(d,channel)));}
    const auto link=color_link(d,ref);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","color"},{"authored",authored},
        {"evaluated",color_json(color_value(d,ref,values))},{"channels",channels},
        {"used",color_is_used(d,ref,operation_enabled,gradient_enabled)},
        {"link",link?j::value(ref_json(*link)):j::value(nullptr)}};
}
j::value text_json(const TextSource& s) {
    j::object parameters;for(const auto& [name,value]:s.parameters)parameters[name]=scalar_json(value);
    j::object result{{"id",s.id},{"version",s.version},{"content",s.content},{"family",s.family},{"locale",s.locale},
        {"layout",s.layout},{"direction",s.direction},{"alignment",s.alignment},{"weight",s.weight},{"italic",s.italic},{"parameters",parameters}};
    if(s.italic_driver)result["italic_driver"]=text_italic_driver_json(*s.italic_driver);
    if(s.weight_driver)result["weight_driver"]=text_weight_driver_json(*s.weight_driver);
    if(s.weight_expression)result["weight_expression"]=expression_json(*s.weight_expression);
    if(s.content_driver)result["content_driver"]=text_content_driver_json(*s.content_driver);
    if(s.family_driver)result["family_driver"]=text_family_driver_json(*s.family_driver);
    if(s.locale_driver)result["locale_driver"]=text_locale_driver_json(*s.locale_driver);
    if(s.direction_driver)result["direction_driver"]=text_direction_driver_json(*s.direction_driver);
    if(s.layout_driver)result["layout_driver"]=text_layout_driver_json(*s.layout_driver);
    if(s.alignment_driver)result["alignment_driver"]=text_alignment_driver_json(*s.alignment_driver);
    if(s.path_attachment)result["path_attachment"]=text_path_attachment_json(*s.path_attachment);
    if(!s.font_features.empty()) {
        j::array features;
        for(const auto& feature:s.font_features)features.push_back(j::object{
            {"feature_tag",feature.feature_tag},{"parameter",feature.parameter},{"scope",feature.scope}});
        result["font_features"]=std::move(features);
    }
    if(!s.additional_axis_values.empty()) {
        j::object axes;for(const auto& [tag,value]:s.additional_axis_values)axes[tag]=value;
        result["additional_axis_values"]=std::move(axes);
    }
    return result;
}
j::object text_italic_property_json(const Document& d,const Ref& ref,const TextItalicProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=text_italic_driver_json(*value.driver);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},{"unit","boolean"},{"space","local"},
        {"origin","authored"},{"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)}}},{"evaluated",value.evaluated}};
}
j::object text_weight_property_json(const Document& d,const Ref& ref,const TextWeightProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=j::object{{"link",ref_json(value.driver->link)},
        {"offset",value.driver->offset}};
    j::value expression=nullptr;if(value.expression)expression=expression_json(*value.expression);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","integer"},{"unit","unitless"},{"space","local"},
        {"origin","authored"},{"range",j::object{{"min",1},{"max",999}}},
        {"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)},
            {"expression",std::move(expression)},{"source_kind",value.driver?"link":value.expression?"expression":"literal"}}},
        {"evaluated",value.evaluated},{"link",true},{"expression",true}};
}
j::object artboard_size_property_json(const Document& d,const Ref& ref,const ArtboardSizeProperty& value) {
    j::value expression=nullptr;if(value.expression)expression=expression_json(*value.expression);
    j::object authored{{"literal",value.literal},{"driver",value.driver?j::value(ref_json(*value.driver)):j::value(nullptr)},
        {"source_kind",value.source_kind},{"expression",std::move(expression)}};
    if(value.template_source)authored["template_source"]=ref_json(*value.template_source);
    if(value.template_override)authored["template_override"]=*value.template_override;
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","number"},{"unit","du"},
        {"space","composition"},{"origin","authored"},
        {"range",j::object{{"min_exclusive",0},{"max",1e7}}},
        {"authored",std::move(authored)},
        {"evaluated",value.evaluated},{"link",true},{"expression",true},
        {"unlink",value.source_kind=="link"||value.source_kind=="expression"},
        {"use_template",value.template_source.has_value()}};
}
bool artboard_layout_has_template_reset(const Document& d,const Ref& ref) {
    const bool margin=ref.field.starts_with("margin.");
    for(const auto& composition:d.compositions)for(const auto& board:composition.artboards) {
        if(!board.template_assignment)continue;
        if(margin&&board.id==ref.object)return true;
        if(!margin&&board.template_assignment->grid_id==ref.object)return true;
    }
    return false;
}
j::object artboard_layout_property_json(const Document& d,const Ref& ref,const ArtboardLayoutProperty& value) {
    const bool integer=std::holds_alternative<std::size_t>(value.literal);
    const auto literal=std::visit([](const auto& item){return j::value(item);},value.literal);
    const auto evaluated=std::visit([](const auto& item){return j::value(item);},value.evaluated);
    j::object result{{"ref",ref_json(ref)},{"name",property_name(d,ref)},
        {"type",integer?"integer":"number"},{"unit",integer?"unitless":"du"},
        {"space","artboard_local"},{"origin","authored"},
        {"authored",j::object{{"literal",literal}}},{"evaluated",literal},
        {"link",false},{"expression",false},{"unlink",false},{"use_template",false}};
    if(ref.field=="grid.columns") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        j::value expression=nullptr;if(value.expression)expression=expression_json(*value.expression);
        result["range"]=j::object{{"min",1},{"max",1000}};
        result["authored"]=j::object{{"literal",literal},{"driver",std::move(driver)},
            {"expression",std::move(expression)},{"source_kind",value.source_kind}};
        result["evaluated"]=evaluated;result["link"]=true;result["expression"]=true;
    } else if(ref.field=="grid.rows") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        j::object authored{{"literal",literal},{"driver",std::move(driver)},
            {"source_kind",value.source_kind}};
        if(value.expression)authored["expression"]=expression_json(*value.expression);
        result["authored"]=std::move(authored);
        result["range"]=j::object{{"min",1},{"max",1000}};
        result["evaluated"]=evaluated;result["link"]=true;result["expression"]=true;
    } else if(ref.field=="margin.top"||ref.field=="margin.right") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        j::object authored{{"literal",literal},{"driver",std::move(driver)},
            {"source_kind",value.source_kind}};
        if(value.expression)authored["expression"]=expression_json(*value.expression);
        result["authored"]=std::move(authored);
        result["evaluated"]=evaluated;result["link"]=true;result["expression"]=true;
    } else if(ref.field=="margin.bottom") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        j::object authored{{"literal",literal},{"driver",std::move(driver)},
            {"source_kind",value.source_kind}};
        if(value.expression)authored["expression"]=expression_json(*value.expression);
        result["authored"]=std::move(authored);
        result["evaluated"]=evaluated;result["link"]=true;result["expression"]=true;
    } else if(ref.field=="grid.bounds.width") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        j::object authored{{"literal",literal},{"driver",std::move(driver)},
            {"source_kind",value.source_kind}};
        if(value.expression)authored["expression"]=expression_json(*value.expression);
        result["authored"]=std::move(authored);
        result["evaluated"]=evaluated;result["link"]=true;result["expression"]=true;
    } else if(ref.field=="grid.bounds.height") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        j::object authored{{"literal",literal},{"driver",std::move(driver)},
            {"source_kind",value.source_kind}};
        if(value.expression)authored["expression"]=expression_json(*value.expression);
        result["authored"]=std::move(authored);
        result["evaluated"]=evaluated;result["link"]=true;result["expression"]=true;
    } else if(ref.field=="grid.column_gutter") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        j::object authored{{"literal",literal},{"driver",std::move(driver)},
            {"source_kind",value.source_kind}};
        if(value.expression)authored["expression"]=expression_json(*value.expression);
        result["authored"]=std::move(authored);
        result["evaluated"]=evaluated;result["link"]=true;result["expression"]=true;
    } else if(ref.field=="grid.row_gutter") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        j::object authored{{"literal",literal},{"driver",std::move(driver)},
            {"source_kind",value.source_kind}};
        if(value.expression)authored["expression"]=expression_json(*value.expression);
        result["authored"]=std::move(authored);
        result["evaluated"]=evaluated;result["link"]=true;result["expression"]=true;
    } else if(ref.field=="margin.left"||ref.field=="grid.bounds.x"||ref.field=="grid.bounds.y") {
        j::value driver=nullptr;if(value.driver)driver=ref_json(*value.driver);
        const auto source_kind=value.source_kind;
        j::object authored{{"literal",literal},{"driver",std::move(driver)},
            {"source_kind",source_kind}};
        if(value.expression)authored["expression"]=expression_json(*value.expression);
        result["authored"]=std::move(authored);
        result["evaluated"]=evaluated;result["link"]=true;
        result["expression"]=true;
    }
    if(value.template_source)result["authored"].as_object()["template_source"]=ref_json(*value.template_source);
    const bool has_driver=value.driver.has_value()||value.expression.has_value();
    result["unlink"]=value.source_kind!="template"&&has_driver;
    result["use_template"]=value.template_source.has_value()||artboard_layout_has_template_reset(d,ref);
    return result;
}
std::string guide_property_name(const Document& d,const Ref& ref) {
    for(const auto& composition:d.compositions)for(const auto& guide:composition.guides)
        if(guide.id==ref.object)return guide.name;
    throw Error("MISSING_GUIDE",ref.object);
}
j::object guide_position_property_json(const std::string& name,const Ref& ref,const GuidePositionProperty& value) {
    j::value expression=nullptr;if(value.expression)expression=expression_json(*value.expression);
    return {{"ref",ref_json(ref)},{"name",name},{"type","number"},{"unit","du"},
        {"space","composition"},{"origin","authored"},
        {"range",j::object{{"min",-1e9},{"max",1e9}}},
        {"authored",j::object{{"literal",value.literal},{"driver",value.driver?j::value(ref_json(*value.driver)):j::value(nullptr)},
            {"source_kind",value.source_kind},{"expression",std::move(expression)}}},
        {"evaluated",value.evaluated},{"link",true},{"expression",true}};
}
j::object artboard_guide_property_json(const Ref& ref,const ArtboardGuideProperty& value,const std::string& display_name) {
    const auto& item=value.occurrence;
    const bool position=ref.field=="artboard.guide.position";
    j::value target_override=nullptr;
    if(item.inherited&&position&&item.position_overridden)target_override=item.position;
    if(item.inherited&&!position&&item.enabled_overridden)target_override=item.enabled;
    j::object authored{{"literal",target_override}};
    if(!item.inherited)authored["literal"]=position?j::value(item.position):j::value(item.enabled);
    j::object source{{"artboard",item.source_artboard},{"guide_id",item.guide_id},
        {"name",item.name},{"axis",item.axis},
        {"literal_position",item.source_position},{"literal_enabled",item.source_enabled}};
    j::value immediate_source=nullptr;
    if(item.inherited)immediate_source=j::object{{"artboard",item.template_source_artboard},
        {"position",item.template_position},{"enabled",item.template_enabled}};
    const auto evaluated=position?j::value(item.position):j::value(item.enabled);
    j::object result{{"ref",ref_json(ref)},{"name",display_name},
        {"type",position?"number":"bool"},{"unit",position?"du":"boolean"},
        {"space","artboard_local"},{"origin",item.inherited?"template":"authored"},
        {"authored",std::move(authored)},{"source",std::move(source)},
        {"template_source",std::move(immediate_source)},{"evaluated",evaluated},
        {"target_artboard",item.target_artboard},{"source_artboard",item.source_artboard},
        {"guide_id",item.guide_id},{"name_inherited",item.inherited},{"axis",item.axis},
        {"inherited",item.inherited},{"position_overridden",item.position_overridden},
        {"enabled_overridden",item.enabled_overridden},{"detached",false},
        {"link",false},{"expression",false}};
    return result;
}
j::object text_content_property_json(const Document& d,const Ref& ref,const TextContentProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=text_content_driver_json(*value.driver);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","string"},{"origin","authored"},
        {"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)}}},
        {"evaluated",value.evaluated},{"link",true},{"expression",false}};
}
j::object text_family_property_json(const Document& d,const Ref& ref,const TextFamilyProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=text_family_driver_json(*value.driver);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","string"},{"origin","authored"},
        {"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)}}},
        {"evaluated",value.evaluated},{"link",true},{"expression",false}};
}
j::object text_locale_property_json(const Document& d,const Ref& ref,const TextLocaleProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=text_locale_driver_json(*value.driver);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","string"},{"origin","authored"},
        {"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)} }},
        {"evaluated",value.evaluated},{"link",true},{"expression",false}};
}
j::object text_direction_property_json(const Document& d,const Ref& ref,const TextDirectionProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=text_direction_driver_json(*value.driver);
    j::array choices;choices.push_back("horizontal");choices.push_back("vertical");
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","enum"},{"origin","authored"},
        {"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)}}},
        {"evaluated",value.evaluated},{"link",true},{"expression",false},{"choices",std::move(choices)}};
}
j::object text_layout_property_json(const Document& d,const Ref& ref,const TextLayoutProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=text_layout_driver_json(*value.driver);
    j::array choices;choices.push_back("auto");choices.push_back("frame");
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","enum"},{"origin","authored"},
        {"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)} }},
        {"evaluated",value.evaluated},{"link",true},{"expression",false},{"choices",std::move(choices)}};
}
j::object text_alignment_property_json(const Document& d,const Ref& ref,const TextAlignmentProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=text_alignment_driver_json(*value.driver);
    j::array choices;choices.push_back("start");choices.push_back("center");choices.push_back("end");
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","enum"},{"origin","authored"},
        {"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)}}},
        {"evaluated",value.evaluated},{"link",true},{"expression",false},{"choices",std::move(choices)}};
}
j::object fill_rule_property_json(const Document& d,const Ref& ref,const FillRuleProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=fill_rule_driver_json(*value.driver);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","enum"},{"origin","authored"},
        {"choices",j::array{"nonzero","evenodd"}},
        {"authored",j::object{{"literal",value.literal},{"driver",std::move(driver)}}},
        {"evaluated",value.evaluated},{"link",true},{"expression",false}};
}
j::object operation_enabled_property_json(const Document& d,const Ref& ref,const OperationEnabledProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=j::object{{"link",ref_json(*value.driver)}};
    const auto& operations=d.objects.at(ref.object).stack;
    const auto entry=std::find_if(operations.begin(),operations.end(),[&](const auto& candidate) {
        return operation_ref(ref.object,candidate.id,"enabled")==ref;
    });
    const bool expression_capable=entry!=operations.end()&&!entry->macro;
    j::object authored{{"literal",value.literal},{"driver",std::move(driver)}};
    if(value.expression) {
        authored["source_kind"]="expression";
        authored["expression"]=expression_json(*value.expression);
    }
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},
        {"unit","boolean"},{"space","local"},{"origin","authored"},
        {"authored",std::move(authored)},
        {"evaluated",value.evaluated},{"link",true},{"expression",expression_capable}};
}
j::object gradient_enabled_property_json(const Document& d,const Ref& ref,const GradientEnabledProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=j::object{{"link",ref_json(*value.driver)}};
    j::object authored{{"literal",value.literal},{"driver",std::move(driver)},
        {"source_kind",value.driver?"link":value.expression?"expression":"literal"}};
    if(value.expression)authored["expression"]=expression_json(*value.expression);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},
        {"unit","boolean"},{"space","local"},{"origin","authored"},
        {"authored",std::move(authored)},
        {"evaluated",value.evaluated},{"link",true},{"expression",true}};
}
j::object object_visibility_property_json(const Document& d,const Ref& ref,const ObjectVisibilityProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=j::object{{"link",ref_json(*value.driver)}};
    j::object authored{{"literal",value.literal},{"driver",std::move(driver)},
        {"source_kind",value.driver?"link":value.expression?"expression":"literal"}};
    j::object result{{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},
        {"unit","boolean"},{"space","local"},{"origin","authored"},
        {"authored",std::move(authored)},
        {"evaluated",value.evaluated},{"link",true},{"expression",true}};
    if(value.expression) {
        result["authored"].as_object()["expression"]=expression_json(*value.expression);
    }
    return result;
}
j::object composite_isolated_property_json(const Document& d,const Ref& ref,const CompositeIsolationProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=j::object{{"link",ref_json(*value.driver)}};
    j::object authored{{"literal",value.literal},{"driver",std::move(driver)},
        {"source_kind",value.driver?"link":value.expression?"expression":"literal"}};
    if(value.expression)authored["expression"]=expression_json(*value.expression);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},
        {"unit","boolean"},{"space","local"},{"origin","authored"},
        {"authored",std::move(authored)},
        {"evaluated",value.evaluated},{"link",true},{"expression",true}};
}
j::object geometry_mask_enabled_property_json(const Document& d,const Ref& ref,bool enabled) {
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},
        {"unit","boolean"},{"space","local"},{"origin","authored"},
        {"authored",j::object{{"literal",enabled},{"driver",nullptr}}},
        {"evaluated",enabled},{"link",false},{"expression",false}};
}
j::object geometry_mask_enabled_state_json(const Document& d,const Ref& ref,const GeometryMaskEnabledProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=j::object{{"link",ref_json(*value.driver)}};
    j::object authored{{"literal",value.literal},{"driver",std::move(driver)},
        {"source_kind",value.driver?"link":value.expression?"expression":"literal"}};
    if(value.expression)authored["expression"]=expression_json(*value.expression);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},
        {"unit","boolean"},{"space","local"},{"origin","authored"},
        {"authored",std::move(authored)},
        {"evaluated",value.evaluated},{"link",true},{"expression",true}};
}
j::object point_edit_enabled_property_json(const Document& d,const Ref& ref,bool enabled) {
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},
        {"unit","boolean"},{"space","local"},{"origin","authored"},
        {"authored",j::object{{"literal",enabled},{"driver",nullptr}}},
        {"evaluated",enabled},{"link",false},{"expression",false}};
}
j::object point_edit_enabled_state_json(const Document& d,const Ref& ref,const PointEditEnabledProperty& value) {
    j::value driver=nullptr;if(value.driver)driver=j::object{{"link",ref_json(*value.driver)}};
    j::object authored{{"literal",value.literal},{"driver",std::move(driver)},
        {"source_kind",value.driver?"link":value.expression?"expression":"literal"}};
    if(value.expression)authored["expression"]=expression_json(*value.expression);
    return {{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type","bool"},
        {"unit","boolean"},{"space","local"},{"origin","authored"},
        {"authored",std::move(authored)},
        {"evaluated",value.evaluated},{"link",true},{"expression",true}};
}
j::object text_readonly_property_json(const Document& d,const Ref& ref,const TextPropertyValue& value) {
    const auto type=value.kind==TextPropertyKind::string?"string":"enum";
    j::object result{{"ref",ref_json(ref)},{"name",property_name(d,ref)},{"type",type},{"origin","authored"},
        {"authored",j::object{{"literal",value.literal}}},{"evaluated",value.literal},{"link",false},{"expression",false}};
    if(value.kind==TextPropertyKind::enumeration) {
        j::array choices;for(const auto& choice:value.choices)choices.push_back(j::value(choice));
        result["choices"]=std::move(choices);
    }
    return result;
}
j::value font_optional(const std::optional<double>& value) {return value?j::value(*value):j::value(nullptr);}
j::value font_optional(const std::optional<std::string>& value) {return value?j::value(*value):j::value(nullptr);}
j::value font_optional(const std::optional<bool>& value) {return value?j::value(*value):j::value(nullptr);}
j::value font_optional(const std::optional<std::uint32_t>& value) {return value?j::value(*value):j::value(nullptr);}
j::object font_request_json(const TextFontRequest& request) {
    j::array features;for(const auto& feature:request.font_features)
        features.push_back(j::object{{"feature_tag",feature.feature_tag},{"parameter",feature.parameter},{"scope",feature.scope}});
    j::object axes,submitted;
    for(const auto& [tag,value]:request.additional_axis_values)axes[tag]=value;
    for(const auto& [tag,value]:request.submitted_axis_values)submitted[tag]=value;
    return {{"family",request.family},{"locale",request.locale},{"weight",request.weight},{"italic",request.italic},
        {"requested_em_size",request.requested_em_size},{"layout_em_size",request.layout_em_size},
        {"font_features",std::move(features)},{"additional_axis_values",std::move(axes)},
        {"submitted_axis_values",std::move(submitted)},{"axis_application",request.axis_application},
        {"feature_application",request.feature_application},{"authority","evaluated_request_not_resolved_face"}};
}
j::array font_runs_json(const std::vector<TextFontRun>& runs) {
    j::array result;
    for(const auto& run:runs) {
        j::array files,axes,axis_checks,glyphs,scripts;
        for(const auto& file:run.files)files.push_back(j::object{{"loader_scope",file.loader_scope},{"key_sha256",file.key_sha256},
            {"key_size",file.key_size},{"local_loader",file.local_loader},{"identity_kind","loader_scoped_reference_key_digest"},
            {"portable_file_hash",false}});
        for(const auto& axis:run.axes)axes.push_back(j::object{{"tag",axis.tag},{"value",font_optional(axis.value)},
            {"minimum",font_optional(axis.minimum)},{"maximum",font_optional(axis.maximum)},{"default",font_optional(axis.default_value)},
            {"variable",font_optional(axis.variable)}});
        for(const auto& check:run.axis_checks)axis_checks.push_back(j::object{{"tag",check.tag},{"owner",check.owner},
            {"requested",check.requested},{"submitted",font_optional(check.submitted)},{"resolved",font_optional(check.resolved)},
            {"status",check.status}});
        for(const auto glyph:run.glyph_indices)glyphs.push_back(glyph);
        j::value advances=nullptr;
        if(run.glyph_advances){j::array values;for(const auto value:*run.glyph_advances)values.push_back(value);advances=std::move(values);}
        for(const auto& context:run.script_features) {
            j::array features;for(const auto& feature:context.features)features.push_back(j::object{{"feature_tag",feature.tag},
                {"parameter",feature.parameter},{"availability",feature.availability}});
            scripts.push_back(j::object{{"utf16_start",context.utf16_start},{"utf16_length",context.utf16_length},
                {"script_id",context.script},{"script_shapes",context.shapes},{"features",std::move(features)}});
        }
        result.push_back(j::object{{"utf16_start",font_optional(run.utf16_start)},{"utf16_length",font_optional(run.utf16_length)},
            {"locale",font_optional(run.locale)},{"resolved_family",font_optional(run.family)},{"resolved_face",font_optional(run.face)},
            {"face_index",run.face_index},{"simulations",run.simulations},{"bidi_level",run.bidi_level},{"sideways",run.sideways},
            {"resolved_weight",font_optional(run.resolved_weight)},{"resolved_style",font_optional(run.resolved_style)},
            {"fallback",font_optional(run.fallback)},{"font_em_size",run.font_em_size},{"em_size_ratio",font_optional(run.em_size_ratio)},
            {"em_size_ratio_kind","actual_run_em_size_over_layout_em_size_not_mapcharacters_scale"},
            {"file_references",std::move(files)},{"has_variations",font_optional(run.has_variations)},
            {"axis_values_known",run.axis_values_known},{"axis_ranges_known",run.axis_ranges_known},{"axes",std::move(axes)},
            {"axis_checks",std::move(axis_checks)},{"glyph_count",run.glyph_indices.size()},{"glyph_indices",std::move(glyphs)},
            {"glyph_advances",std::move(advances)},{"missing_glyph_count",run.missing_glyph_count},
            {"script_features",std::move(scripts)},{"feature_effect","not_established_by_availability_query"},
            {"warnings",ids_json(run.warnings)}});
    }
    return result;
}

j::object text_layout_json(const Document& d,const Id& id) {
    const auto object=d.objects.find(id);
    if(object==d.objects.end())throw Error("MISSING_OBJECT",id);
    if(!object->second.text)throw Error("NOT_TEXT",id);
    const auto values=evaluate(d);
    auto text_source=evaluated_text_source(d,id);
    const auto layout=evaluate_text_projection(d,id,values);
    j::value attachment=nullptr;
    if(text_source.path_attachment)attachment=text_path_attachment_json(*text_source.path_attachment);
    return {{"object",id},{"weight",text_source.weight},{"x",layout.x},{"y",layout.y},{"width",layout.width},{"height",layout.height},
        {"overflow",layout.overflow},{"glyph_count",layout.glyph_count},{"warnings",ids_json(layout.warnings)},
        {"used_fonts",ids_json(layout.used_fonts)},{"font_request",font_request_json(layout.font_request)},
        {"font_runs",font_runs_json(layout.font_runs)},{"path_attachment",std::move(attachment)},
        {"attachment_status",text_source.path_attachment?"attached":"detached"},{"svg_text","outlined"},{"font_embedded",false}};
}

Primitive read_primitive(const j::value& v,bool allow_polystar=true,bool allow_expression=true,bool allow_ellipse=true) {
    const auto& o=v.as_object();keys(o,{"id","type","version","parameters"});
    Primitive s{text(o.at("id")),text(o.at("type")),j::value_to<unsigned>(o.at("version")),{}};
    if(!allow_polystar&&(s.type=="nect.shape.polygon"||s.type=="nect.shape.star"))
        throw Error("UNSUPPORTED_OPERATOR","Polygon and Star require native 0.8");
    if(!allow_ellipse&&s.type=="nect.shape.ellipse")
        throw Error("UNSUPPORTED_OPERATOR","Ellipse requires native 0.73");
    for(const auto& p:o.at("parameters").as_object())
        s.parameters.emplace(std::string(p.key()),read_scalar(p.value(),allow_expression));
    return s;
}
PointEdit read_point_edit(const j::value& v,bool allow_expression=true,bool allow_enabled_driver=false,
    bool allow_enabled_expression=false) {
    const auto& o=v.as_object();
    if(!allow_enabled_driver&&o.contains("enabled_driver"))
        throw Error("UNSUPPORTED_POINT_EDIT_ENABLED_DRIVER","Point Edit enabled drivers require native 0.32 and the dedicated link command");
    if(!allow_enabled_expression&&o.contains("enabled_expression"))
        throw Error("UNSUPPORTED_POINT_EDIT_ENABLED_EXPRESSION","Point Edit enabled expressions require native 0.70 and the dedicated expression command");
    if(allow_enabled_expression)keys(o,{"id","type","version","enabled","overrides","enabled_driver","enabled_expression"});
    else if(allow_enabled_driver)keys(o,{"id","type","version","enabled","overrides","enabled_driver"});
    else keys(o,{"id","type","version","enabled","overrides"});
    if(text(o.at("type"))!="nect.path.point-edit")throw Error("UNSUPPORTED_OPERATOR",text(o.at("type")));
    PointEdit edit{text(o.at("id")),j::value_to<unsigned>(o.at("version")),o.at("enabled").as_bool(),{}};
    if(const auto* driver=o.if_contains("enabled_driver")) {
        const auto& wrapper=driver->as_object();keys(wrapper,{"link"});edit.enabled_driver=read_ref(wrapper.at("link"));
    }
    if(const auto* expression=o.if_contains("enabled_expression"))edit.enabled_expression=read_expression(*expression);
    if(edit.enabled_driver&&edit.enabled_expression)
        throw Error("INVALID_POINT_EDIT_ENABLED_SOURCE","Point Edit enabled link and expression are mutually exclusive");
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
    j::object result{{"id",edit.id},{"type","nect.path.point-edit"},{"version",edit.version},
        {"enabled",edit.enabled},{"overrides",overrides}};
    if(edit.enabled_driver)result["enabled_driver"]=j::object{{"link",ref_json(*edit.enabled_driver)}};
    if(edit.enabled_expression)result["enabled_expression"]=expression_json(*edit.enabled_expression);
    return result;
}

Gradient read_gradient(const j::value& v,bool allow_expression=true,bool allow_enabled_driver=true,
    bool allow_enabled_expression=true) {
    const auto& o=v.as_object();
    if(!allow_enabled_driver&&o.contains("enabled_driver"))
        throw Error("UNSUPPORTED_GRADIENT_ENABLED_DRIVER","Gradient enabled drivers require native 0.29 and the dedicated link command");
    if(!allow_enabled_expression&&o.contains("enabled_expression"))
        throw Error("UNSUPPORTED_GRADIENT_ENABLED_EXPRESSION","Gradient enabled expressions require native 0.69 and the dedicated expression command");
    std::vector<std::string_view> allowed{"id","type","version","enabled","start_x","start_y","end_x","end_y","stops"};
    if(allow_enabled_driver)allowed.push_back("enabled_driver");
    if(allow_enabled_expression)allowed.push_back("enabled_expression");
    keys(o,allowed);
    Gradient g;g.id=text(o.at("id"));g.type=text(o.at("type"));g.version=j::value_to<unsigned>(o.at("version"));
    g.enabled=o.at("enabled").as_bool();g.start_x=read_scalar(o.at("start_x"),allow_expression);g.start_y=read_scalar(o.at("start_y"),allow_expression);
    g.end_x=read_scalar(o.at("end_x"),allow_expression);g.end_y=read_scalar(o.at("end_y"),allow_expression);
    if(const auto* driver=o.if_contains("enabled_driver")) {
        const auto& wrapper=driver->as_object();keys(wrapper,{"link"});g.enabled_driver=read_ref(wrapper.at("link"));
    }
    if(const auto* expression=o.if_contains("enabled_expression"))g.enabled_expression=read_expression(*expression);
    if(g.enabled_driver&&g.enabled_expression)
        throw Error("INVALID_GRADIENT_ENABLED_SOURCE","Gradient enabled link and expression are mutually exclusive");
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
    j::object result{{"id",g.id},{"type",g.type},{"version",g.version},{"enabled",g.enabled},
        {"start_x",scalar_json(g.start_x)},{"start_y",scalar_json(g.start_y)},
        {"end_x",scalar_json(g.end_x)},{"end_y",scalar_json(g.end_y)},{"stops",stops}};
    if(g.enabled_driver)result["enabled_driver"]=j::object{{"link",ref_json(*g.enabled_driver)}};
    if(g.enabled_expression)result["enabled_expression"]=expression_json(*g.enabled_expression);
    return result;
}
ShapeOperation read_operation(const j::value& v,bool allow_gradient=true,bool allow_expression=true,bool allow_offset=true,
    bool allow_stroke_style=true,bool allow_fill_rule_driver=false,bool allow_enabled_driver=false,
    bool allow_gradient_enabled_driver=false,bool allow_enabled_expression=false,bool allow_gradient_enabled_expression=true) {
    const auto& o=v.as_object();
    if(!allow_fill_rule_driver&&o.contains("fill_rule_driver"))throw Error("UNSUPPORTED_FILL_RULE_DRIVER","Fill rule drivers require native 0.26 and the dedicated link command");
    if(!allow_enabled_driver&&o.contains("enabled_driver"))
        throw Error("UNSUPPORTED_OPERATION_ENABLED_DRIVER","Operation enabled drivers require native 0.28 and the dedicated link command");
    if(!allow_enabled_expression&&o.contains("enabled_expression"))
        throw Error("UNSUPPORTED_OPERATION_ENABLED_EXPRESSION","Operation enabled expressions require native 0.67 and the dedicated expression command");
    std::vector<std::string_view> allowed{"id","type","version","enabled","parameters","composite","fill_rule"};
    if(allow_fill_rule_driver)allowed.push_back("fill_rule_driver");
    if(allow_gradient)allowed.push_back("gradient");
    if(allow_offset||allow_stroke_style)allowed.push_back("line_join");
    if(allow_stroke_style)allowed.push_back("line_cap");
    if(allow_enabled_driver)allowed.push_back("enabled_driver");
    if(allow_enabled_expression)allowed.push_back("enabled_expression");
    keys(o,allowed);
    ShapeOperation op;op.id=text(o.at("id"));op.type=text(o.at("type"));
    op.version=j::value_to<unsigned>(o.at("version"));op.enabled=o.at("enabled").as_bool();
    op.composite=text(o.at("composite"));op.fill_rule=text(o.at("fill_rule"));
    if(const auto* driver=o.if_contains("fill_rule_driver"))op.fill_rule_driver=read_fill_rule_driver(*driver);
    if(const auto* driver=o.if_contains("enabled_driver")) {
        const auto& wrapper=driver->as_object();keys(wrapper,{"link"});
        op.enabled_driver=read_ref(wrapper.at("link"));
    }
    if(const auto* expression=o.if_contains("enabled_expression"))op.enabled_expression=read_expression(*expression);
    if(op.enabled_driver&&op.enabled_expression)
        throw Error("INVALID_OPERATION_ENABLED_SOURCE","Operation enabled link and expression are mutually exclusive");
    if(op.type=="nect.shape.offset") {
        if(!allow_offset)throw Error("UNSUPPORTED_OPERATOR","Offset Paths requires native 0.12");
        op.line_join=text(o.at("line_join"));
    } else if(op.type=="nect.paint.stroke"&&op.version==2) {
        if(!allow_stroke_style)throw Error("UNSUPPORTED_OPERATOR_VERSION","Stroke v2 requires native 0.13");
        op.line_join=text(o.at("line_join"));op.line_cap=text(o.at("line_cap"));
    } else if(o.contains("line_join"))throw Error("INVALID_OPERATOR_OPTIONS","Line join applies only to Offset or Stroke v2");
    if(o.contains("line_cap")&&!(op.type=="nect.paint.stroke"&&op.version==2))throw Error("INVALID_OPERATOR_OPTIONS","Line cap applies only to Stroke v2");
    for(const auto& p:o.at("parameters").as_object())op.parameters.emplace(std::string(p.key()),read_scalar(p.value(),allow_expression));
    if(const auto* g=o.if_contains("gradient"))op.gradient=read_gradient(*g,allow_expression,
        allow_gradient_enabled_driver,allow_gradient_enabled_expression);
    return op;
}
j::value operation_json(const ShapeOperation& op) {
    j::object parameters;for(const auto& [name,value]:op.parameters)parameters[name]=scalar_json(value);
    j::object result{{"id",op.id},{"type",op.type},{"version",op.version},{"enabled",op.enabled},
        {"parameters",parameters},{"composite",op.composite},{"fill_rule",op.fill_rule}};
    if(op.fill_rule_driver)result["fill_rule_driver"]=fill_rule_driver_json(*op.fill_rule_driver);
    if(op.enabled_driver)result["enabled_driver"]=j::object{{"link",ref_json(*op.enabled_driver)}};
    if(op.enabled_expression)result["enabled_expression"]=expression_json(*op.enabled_expression);
    if(op.gradient)result["gradient"]=gradient_json(*op.gradient);
    if(op.type=="nect.shape.offset")result["line_join"]=op.line_join;
    if(op.type=="nect.paint.stroke"&&op.version==2){result["line_join"]=op.line_join;result["line_cap"]=op.line_cap;}
    return result;
}

MacroPort read_macro_port(const j::value& value) {
    const auto& object=value.as_object();keys(object,{"id","domain"});
    return {text(object.at("id")),text(object.at("domain"))};
}
j::value macro_port_json(const MacroPort& port) {return j::object{{"id",port.id},{"domain",port.domain}};}
MacroEndpoint read_macro_endpoint(const j::value& value) {
    const auto& object=value.as_object();keys(object,{"node","port"});
    return {text(object.at("node")),text(object.at("port"))};
}
j::value macro_endpoint_json(const MacroEndpoint& endpoint) {return j::object{{"node",endpoint.node},{"port",endpoint.port}};}
MacroDefinitionRevision read_macro_revision(const j::value& value) {
    const auto& object=value.as_object();keys(object,{"revision","input","output","nodes","edges","output_mapping","public_parameters","graph_version","interface_version"});
    MacroDefinitionRevision revision;revision.revision=j::value_to<std::uint64_t>(object.at("revision"));
    if(const auto* version=object.if_contains("graph_version")) {
        const auto value=number(*version);
        if(value!=1&&value!=2)throw Error("UNSUPPORTED_MACRO_GRAPH_VERSION","Supported Macro graph versions are 1 and 2");
        revision.graph_version=static_cast<unsigned>(value);
    }
    if(const auto* version=object.if_contains("interface_version")) {
        const auto value=number(*version);
        if(value!=1&&value!=2)throw Error("UNSUPPORTED_MACRO_INTERFACE_VERSION","Supported Macro interface versions are 1 and 2");
        revision.interface_version=static_cast<unsigned>(value);
    }
    revision.input=read_macro_port(object.at("input"));revision.output=read_macro_port(object.at("output"));
    for(const auto& value:object.at("nodes").as_array()) {
        const auto& node=value.as_object();keys(node,{"operation","input_port","output_port"});
        revision.nodes.push_back({read_operation(node.at("operation"),false,false,true,true,false,false,false),
            text(node.at("input_port")),text(node.at("output_port"))});
    }
    for(const auto& value:object.at("edges").as_array()) {
        const auto& edge=value.as_object();keys(edge,{"from","to"});
        revision.edges.push_back({read_macro_endpoint(edge.at("from")),read_macro_endpoint(edge.at("to"))});
    }
    revision.output_mapping=read_macro_endpoint(object.at("output_mapping"));
    for(const auto& value:object.at("public_parameters").as_array()) {
        const auto& parameter=value.as_object();keys(parameter,{"id","label","node","parameter","value_type","unit","domain"});
        revision.public_parameters.push_back({text(parameter.at("id")),text(parameter.at("label")),text(parameter.at("node")),
            text(parameter.at("parameter")),text(parameter.at("value_type")),text(parameter.at("unit")),text(parameter.at("domain"))});
    }
    return revision;
}
j::value macro_revision_json(const MacroDefinitionRevision& revision) {
    j::array nodes,edges,parameters;
    for(const auto& node:revision.nodes)nodes.push_back(j::object{{"operation",operation_json(node.operation)},
        {"input_port",node.input_port},{"output_port",node.output_port}});
    for(const auto& edge:revision.edges)edges.push_back(j::object{{"from",macro_endpoint_json(edge.from)},
        {"to",macro_endpoint_json(edge.to)}});
    for(const auto& parameter:revision.public_parameters)parameters.push_back(j::object{{"id",parameter.id},
        {"label",parameter.label},{"node",parameter.node},{"parameter",parameter.parameter},
        {"value_type",parameter.value_type},{"unit",parameter.unit},{"domain",parameter.domain}});
    j::object result{{"revision",revision.revision},{"input",macro_port_json(revision.input)},
        {"output",macro_port_json(revision.output)},{"nodes",nodes},{"edges",edges},
        {"output_mapping",macro_endpoint_json(revision.output_mapping)},{"public_parameters",parameters}};
    if(revision.graph_version!=1)result["graph_version"]=revision.graph_version;
    if(revision.interface_version!=1)result["interface_version"]=revision.interface_version;
    return result;
}
MacroDefinition read_macro_definition(const j::value& value) {
    const auto& object=value.as_object();keys(object,{"id","label","latest_revision","revisions"});
    MacroDefinition definition;definition.id=text(object.at("id"));definition.label=text(object.at("label"));
    definition.latest_revision=j::value_to<std::uint64_t>(object.at("latest_revision"));
    for(const auto& value:object.at("revisions").as_array()) {
        auto revision=read_macro_revision(value);const auto number=revision.revision;
        if(!definition.revisions.emplace(number,std::move(revision)).second)throw Error("DUPLICATE_MACRO_REVISION",std::to_string(number));
    }
    return definition;
}
j::value macro_definition_json(const MacroDefinition& definition) {
    j::array revisions;for(const auto& [number,revision]:definition.revisions){(void)number;revisions.push_back(macro_revision_json(revision));}
    return j::object{{"id",definition.id},{"label",definition.label},{"latest_revision",definition.latest_revision},{"revisions",revisions}};
}
j::value processing_entry_json(const ProcessingEntry& entry) {
    if(!entry.macro)return j::object{{"kind","operation"},{"operation",operation_json(entry)}};
    j::object overrides;for(const auto& [parameter,value]:entry.macro->overrides)overrides[parameter]=value;
    return j::object{{"kind","macro"},{"id",entry.id},{"enabled",entry.enabled},
        {"definition",entry.macro->definition},{"revision",entry.macro->pinned_revision},{"overrides",overrides}};
}
ProcessingEntry read_processing_entry(const j::value& value,bool allow_enabled_expression=false,
    bool allow_gradient_enabled_expression=false) {
    const auto& object=value.as_object();const auto kind=text(object.at("kind"));
    if(kind=="operation") {
        keys(object,{"kind","operation"});return ProcessingEntry{read_operation(object.at("operation"),true,true,true,true,true,true,true,
            allow_enabled_expression,allow_gradient_enabled_expression)};
    }
    if(kind=="macro") {
        keys(object,{"kind","id","enabled","definition","revision","overrides"});
        ProcessingEntry entry;entry.id=text(object.at("id"));entry.type=macro_entry_type;entry.enabled=object.at("enabled").as_bool();
        MacroInstance instance;instance.definition=text(object.at("definition"));instance.pinned_revision=j::value_to<std::uint64_t>(object.at("revision"));
        for(const auto& [parameter,value]:object.at("overrides").as_object())instance.overrides.emplace(std::string(parameter),number(value));
        entry.macro=std::move(instance);return entry;
    }
    throw Error("UNSUPPORTED_STACK_ENTRY",kind);
}

PresetEntry read_preset_builtin_entry(const j::object& o) {
    keys(o,{"type","version","enabled","parameters","composite","fill_rule","line_join","line_cap"});
    PresetEntry entry;entry.kind="builtin";entry.type=text(o.at("type"));
    entry.version=static_cast<unsigned>(preset_unsigned(o.at("version"),std::numeric_limits<unsigned>::max(),"Preset operation version"));
    entry.enabled=o.at("enabled").as_bool();entry.composite=text(o.at("composite"));entry.fill_rule=text(o.at("fill_rule"));
    entry.line_join=text(o.at("line_join"));entry.line_cap=text(o.at("line_cap"));
    for(const auto& parameter:o.at("parameters").as_object())
        entry.parameters.emplace(std::string(parameter.key()),number(parameter.value()));
    return entry;
}
PresetEntry read_preset_entry(const j::value& value,unsigned schema_version) {
    const auto& o=value.as_object();
    if(schema_version==1)return read_preset_builtin_entry(o);
    keys(o,{"kind","operation","definition","revision","enabled","overrides"});
    const auto kind=text(o.at("kind"));
    if(kind=="builtin") {
        keys(o,{"kind","operation"});return read_preset_builtin_entry(o.at("operation").as_object());
    }
    if(kind=="macro") {
        keys(o,{"kind","definition","revision","enabled","overrides"});
        PresetEntry entry;entry.kind="macro";entry.type=macro_entry_type;entry.macro_definition=text(o.at("definition"));
        entry.pinned_revision=preset_unsigned(o.at("revision"),std::numeric_limits<std::uint64_t>::max(),"Preset Macro revision");entry.enabled=o.at("enabled").as_bool();
        for(const auto& [parameter,value]:o.at("overrides").as_object())
            entry.overrides.emplace(std::string(parameter),number(value));
        return entry;
    }
    throw Error("INVALID_PRESET_ENTRY",kind);
}
PresetDefinition read_preset_definition(const j::value& value) {
    const auto& o=value.as_object();keys(o,{"id","schema_version","label","category","tags","target_domain","entries"});
    PresetDefinition definition;definition.id=text(o.at("id"));
    definition.schema_version=static_cast<unsigned>(preset_unsigned(o.at("schema_version"),std::numeric_limits<unsigned>::max(),"Preset schema version"));
    if(definition.schema_version!=1&&definition.schema_version!=2)
        throw Error("UNSUPPORTED_PRESET_SCHEMA","Preset schema version must be 1 or 2");
    definition.label=text(o.at("label"));if(const auto* category=o.if_contains("category"))definition.category=text(*category);
    if(const auto* tags=o.if_contains("tags"))for(const auto& tag:tags->as_array())definition.tags.push_back(text(tag));
    definition.target_domain=text(o.at("target_domain"));
    for(const auto& entry:o.at("entries").as_array())definition.entries.push_back(read_preset_entry(entry,definition.schema_version));
    return definition;
}
j::value preset_json(const PresetDefinition& definition) {
    j::array tags,entries;for(const auto& tag:definition.tags)tags.push_back(j::value(tag));
    for(const auto& entry:definition.entries) {
        if(definition.schema_version==1||entry.kind=="builtin") {
            j::object parameters;for(const auto& [name,value]:entry.parameters)parameters[name]=value;
            j::object builtin{{"type",entry.type},{"version",entry.version},{"enabled",entry.enabled},
                {"parameters",parameters},{"composite",entry.composite},{"fill_rule",entry.fill_rule},
                {"line_join",entry.line_join},{"line_cap",entry.line_cap}};
            entries.push_back(definition.schema_version==1?j::value(std::move(builtin)):
                j::value(j::object{{"kind","builtin"},{"operation",std::move(builtin)}}));
        } else {
            j::object overrides;for(const auto& [name,value]:entry.overrides)overrides[name]=value;
            entries.push_back(j::object{{"kind","macro"},{"definition",entry.macro_definition},
                {"revision",entry.pinned_revision},{"enabled",entry.enabled},{"overrides",overrides}});
        }
    }
    return j::object{{"id",definition.id},{"schema_version",definition.schema_version},{"label",definition.label},
        {"category",definition.category},{"tags",tags},{"target_domain",definition.target_domain},{"entries",entries}};
}

std::string canonical_json(const j::value& value) {
    if(value.is_object()) {
        std::vector<const j::key_value_pair*> members;
        members.reserve(value.as_object().size());
        for(const auto& member:value.as_object())members.push_back(&member);
        std::sort(members.begin(),members.end(),[](const auto* left,const auto* right) {
            return std::string_view(left->key().data(),left->key().size())<
                std::string_view(right->key().data(),right->key().size());
        });
        std::string result="{";bool first=true;
        for(const auto* member:members) {
            if(!first)result+=',';first=false;
            const auto key=member->key();
            result+=j::serialize(j::value(j::string(key.data(),key.size())));
            result+=':';result+=canonical_json(member->value());
        }
        result+='}';return result;
    }
    if(value.is_array()) {
        std::string result="[";bool first=true;
        for(const auto& item:value.as_array()) {
            if(!first)result+=',';first=false;result+=canonical_json(item);
        }
        result+=']';return result;
    }
    return j::serialize(value);
}

Definition read_definition(const j::value& value) {
    const auto& o=value.as_object();keys(o,{"id","name","root"});
    return {text(o.at("id")),text(o.at("name")),text(o.at("root"))};
}
j::value definition_json(const Definition& definition) {
    return j::object{{"id",definition.id},{"name",definition.name},{"root",definition.root}};
}
DefinitionInstance read_instance(const j::value& value) {
    const auto& o=value.as_object();keys(o,{"definition","overrides","visibility_overrides","color_overrides","text_content_overrides"});
    DefinitionInstance instance;instance.definition=text(o.at("definition"));
    for(const auto& item:o.at("overrides").as_array()) {
        const auto& entry=item.as_object();keys(entry,{"target","value"});
        auto target=read_ref(entry.at("target"));
        if(!instance.overrides.emplace(std::move(target),number(entry.at("value"))).second)
            throw Error("DUPLICATE_OVERRIDE_KEY","Instance contains the same OverrideKey more than once");
    }
    if(const auto* visibility=o.if_contains("visibility_overrides")) {
        for(const auto& item:visibility->as_array()) {
            const auto& entry=item.as_object();keys(entry,{"source","visible"});
            if(!instance.visibility_overrides.emplace(text(entry.at("source")),entry.at("visible").as_bool()).second)
                throw Error("DUPLICATE_OVERRIDE_KEY","Instance contains the same visibility source more than once");
        }
    }
    if(const auto* colors=o.if_contains("color_overrides")) {
        for(const auto& item:colors->as_array()) {
            const auto& entry=item.as_object();keys(entry,{"target","value"});
            if(!instance.color_overrides.emplace(read_ref(entry.at("target")),read_color(entry.at("value"))).second)
                throw Error("DUPLICATE_OVERRIDE_KEY","Instance contains the same color target more than once");
        }
    }
    if(const auto* contents=o.if_contains("text_content_overrides")) {
        for(const auto& item:contents->as_array()) {
            const auto& entry=item.as_object();keys(entry,{"source","content"});
            if(!instance.text_content_overrides.emplace(text(entry.at("source")),text(entry.at("content"))).second)
                throw Error("DUPLICATE_OVERRIDE_KEY","Instance contains the same Text content source more than once");
        }
    }
    return instance;
}
j::value instance_json(const DefinitionInstance& instance) {
    j::array overrides;
    for(const auto& [target,value]:instance.overrides)
        overrides.push_back(j::object{{"target",ref_json(target)},{"value",value}});
    j::object result{{"definition",instance.definition},{"overrides",overrides}};
    if(!instance.visibility_overrides.empty()) {
        j::array visibility;
        for(const auto& [source,visible]:instance.visibility_overrides)
            visibility.push_back(j::object{{"source",source},{"visible",visible}});
        result["visibility_overrides"]=std::move(visibility);
    }
    if(!instance.color_overrides.empty()) {
        j::array colors;
        for(const auto& [target,value]:instance.color_overrides)
            colors.push_back(j::object{{"target",ref_json(target)},{"value",color_json(value)}});
        result["color_overrides"]=std::move(colors);
    }
    if(!instance.text_content_overrides.empty()) {
        j::array contents;
        for(const auto& [source,content]:instance.text_content_overrides)
            contents.push_back(j::object{{"source",source},{"content",content}});
        result["text_content_overrides"]=std::move(contents);
    }
    return result;
}

double layout_number(const j::value& value) {
    if(!value.is_number())throw Error("INVALID_LAYOUT","Layout values must be JSON numbers");
    return number(value);
}
std::size_t layout_count(const j::value& value) {
    if(!value.is_number())throw Error("INVALID_LAYOUT","Grid counts must be integers from 1 through 1000");
    const auto count=number(value);
    if(!std::isfinite(count)||std::floor(count)!=count||count<1||count>1000)
        throw Error("INVALID_LAYOUT","Grid counts must be integers from 1 through 1000");
    return static_cast<std::size_t>(count);
}
Guide read_guide(const j::value& value,bool allow_position_driver=false,bool allow_position_expression=false) {
    try {
        if(!value.is_object())throw Error("INVALID_GUIDE","Guide must be an object");
        const auto& object=value.as_object();
        if(allow_position_driver&&allow_position_expression)
            keys(object,{"id","name","axis","position","position_driver","position_expression"});
        else if(allow_position_driver)keys(object,{"id","name","axis","position","position_driver"});
        else keys(object,{"id","name","axis","position"});
        if(!object.contains("id")||!object.contains("name")||!object.contains("axis")||!object.contains("position"))
            throw Error("INVALID_GUIDE","Guide requires id, name, axis and position");
        if(!object.at("id").is_string()||!object.at("name").is_string()||!object.at("axis").is_string()||
           !object.at("position").is_number())
            throw Error("INVALID_GUIDE","Guide fields must be strings except for numeric position");
        Guide guide;guide.id=text(object.at("id"));guide.name=text(object.at("name"));
        guide.axis=text(object.at("axis"));guide.position=number(object.at("position"));
        if(object.contains("position_driver")) {
            const auto& driver=object.at("position_driver");
            if(!driver.is_object())throw Error("INVALID_GUIDE","Guide position_driver must contain a link Ref");
            const auto& fields=driver.as_object();keys(fields,{"link"});
            if(!fields.contains("link"))throw Error("INVALID_GUIDE","Guide position_driver requires link");
            guide.position_driver=read_ref(fields.at("link"));
        }
        if(object.contains("position_expression")) {
            if(!allow_position_expression)
                throw Error("INVALID_GUIDE","Guide position expressions require native 0.34");
            guide.position_expression=read_expression(object.at("position_expression"));
        }
        return guide;
    } catch(const Error& error) {
        if(error.code=="INVALID_GUIDE")throw;
        throw Error("INVALID_GUIDE",error.what());
    } catch(const std::exception& error) {
        throw Error("INVALID_GUIDE",error.what());
    }
}
j::value guide_json(const Guide& guide) {
    j::object result{{"id",guide.id},{"name",guide.name},{"axis",guide.axis},{"position",guide.position}};
    if(guide.position_driver)result["position_driver"]=j::object{{"link",ref_json(*guide.position_driver)}};
    if(guide.position_expression)result["position_expression"]=expression_json(*guide.position_expression);
    return result;
}
ArtboardLayout read_layout(const j::value& value,bool allow_margin_driver=false,bool allow_grid_x_driver=false,
    bool allow_grid_x_expression=false,bool allow_margin_expression=false,bool allow_grid_y_driver=false,
    bool allow_grid_y_expression=false,bool allow_margin_top_driver=false,bool allow_margin_top_expression=false,
    bool allow_margin_right_driver=false,bool allow_margin_right_expression=false,
    bool allow_margin_bottom_driver=false,bool allow_margin_bottom_expression=false,
    bool allow_grid_width_driver=false,bool allow_grid_width_expression=false,
    bool allow_grid_height_driver=false,bool allow_grid_height_expression=false,
    bool allow_grid_column_gutter_driver=false,bool allow_grid_row_gutter_driver=false,
    bool allow_grid_row_gutter_expression=false,bool allow_grid_column_gutter_expression=false,
    bool allow_grid_columns_driver=false,bool allow_grid_rows_driver=false,bool allow_grid_columns_expression=false,
    bool allow_grid_rows_expression=false) {
    try {
        if(!value.is_object())throw Error("INVALID_LAYOUT","Layout must be an object");
        const auto& object=value.as_object();keys(object,{"margin","grid"});ArtboardLayout layout;
        if(object.contains("margin")) {
            if(!object.at("margin").is_object())throw Error("INVALID_LAYOUT","Margin must be an object");
            const auto& margin=object.at("margin").as_object();
            std::vector<std::string_view> allowed_margin{"left","top","right","bottom"};
            if(allow_margin_driver)allowed_margin.push_back("left_driver");
            if(allow_margin_expression)allowed_margin.push_back("left_expression");
            if(allow_margin_top_driver)allowed_margin.push_back("top_driver");
            if(allow_margin_top_expression)allowed_margin.push_back("top_expression");
            if(allow_margin_right_driver)allowed_margin.push_back("right_driver");
            if(allow_margin_right_expression)allowed_margin.push_back("right_expression");
            if(allow_margin_bottom_driver)allowed_margin.push_back("bottom_driver");
            if(allow_margin_bottom_expression)allowed_margin.push_back("bottom_expression");
            keys(margin,allowed_margin);
            if(!margin.contains("left")||!margin.contains("top")||!margin.contains("right")||!margin.contains("bottom"))
                throw Error("INVALID_LAYOUT","Margin requires left, top, right and bottom");
            layout.margin=Margin{layout_number(margin.at("left")),layout_number(margin.at("top")),
                layout_number(margin.at("right")),layout_number(margin.at("bottom"))};
            if(allow_margin_driver)if(const auto* driver=margin.if_contains("left_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.margin->left_driver=read_ref(fields.at("link"));
            }
            if(allow_margin_expression)if(const auto* expression=margin.if_contains("left_expression"))
                layout.margin->left_expression=read_expression(*expression);
            if(allow_margin_top_driver)if(const auto* driver=margin.if_contains("top_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.margin->top_driver=read_ref(fields.at("link"));
            }
            if(allow_margin_top_expression)if(const auto* expression=margin.if_contains("top_expression"))
                layout.margin->top_expression=read_expression(*expression);
            if(allow_margin_right_driver)if(const auto* driver=margin.if_contains("right_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.margin->right_driver=read_ref(fields.at("link"));
            }
            if(allow_margin_right_expression)if(const auto* expression=margin.if_contains("right_expression"))
                layout.margin->right_expression=read_expression(*expression);
            if(allow_margin_bottom_driver)if(const auto* driver=margin.if_contains("bottom_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.margin->bottom_driver=read_ref(fields.at("link"));
            }
            if(allow_margin_bottom_expression)if(const auto* expression=margin.if_contains("bottom_expression"))
                layout.margin->bottom_expression=read_expression(*expression);
        }
        if(object.contains("grid")) {
            if(!object.at("grid").is_object())throw Error("INVALID_LAYOUT","Grid must be an object");
            const auto& grid=object.at("grid").as_object();
            if(allow_grid_rows_expression) {
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","columns_driver","columns_expression","rows_driver","rows_expression",
                    "column_gutter_driver","column_gutter_expression","row_gutter_driver","row_gutter_expression",
                    "bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression",
                    "bounds_width_driver","bounds_width_expression","bounds_height_driver","bounds_height_expression"});
            }
            else if(allow_grid_columns_expression) {
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","columns_driver","columns_expression","rows_driver",
                    "column_gutter_driver","column_gutter_expression","row_gutter_driver","row_gutter_expression",
                    "bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression",
                    "bounds_width_driver","bounds_width_expression","bounds_height_driver","bounds_height_expression"});
            }
            else if(allow_grid_rows_driver) {
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","columns_driver","rows_driver",
                    "column_gutter_driver","column_gutter_expression","row_gutter_driver","row_gutter_expression",
                    "bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression",
                    "bounds_width_driver","bounds_width_expression","bounds_height_driver","bounds_height_expression"});
            }
            else if(allow_grid_columns_driver) {
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","columns_driver",
                    "column_gutter_driver","column_gutter_expression","row_gutter_driver","row_gutter_expression",
                    "bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression",
                    "bounds_width_driver","bounds_width_expression","bounds_height_driver","bounds_height_expression"});
            }
            else if(allow_grid_row_gutter_expression||allow_grid_column_gutter_expression) {
                std::vector<std::string_view> allowed_grid{"id","bounds","columns","rows","column_gutter","row_gutter",
                    "column_gutter_driver","row_gutter_driver","bounds_x_driver","bounds_x_expression","bounds_y_driver",
                    "bounds_y_expression","bounds_width_driver","bounds_width_expression","bounds_height_driver","bounds_height_expression"};
                if(allow_grid_column_gutter_expression)allowed_grid.push_back("column_gutter_expression");
                if(allow_grid_row_gutter_expression)allowed_grid.push_back("row_gutter_expression");
                keys(grid,allowed_grid);
            }
            else if(allow_grid_row_gutter_driver)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","column_gutter_driver","row_gutter_driver","bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression","bounds_width_driver","bounds_width_expression","bounds_height_driver","bounds_height_expression"});
            else if(allow_grid_column_gutter_driver)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","column_gutter_driver","bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression","bounds_width_driver","bounds_width_expression","bounds_height_driver","bounds_height_expression"});
            else if(allow_grid_height_expression)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression","bounds_width_driver","bounds_width_expression","bounds_height_driver","bounds_height_expression"});
            else if(allow_grid_height_driver)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression","bounds_width_driver","bounds_width_expression","bounds_height_driver"});
            else if(allow_grid_width_expression)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression","bounds_width_driver","bounds_width_expression"});
            else if(allow_grid_width_driver)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression","bounds_width_driver"});
            else if(allow_grid_y_expression)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver","bounds_x_expression","bounds_y_driver","bounds_y_expression"});
            else if(allow_grid_y_driver&&allow_grid_x_expression)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver","bounds_x_expression","bounds_y_driver"});
            else if(allow_grid_y_driver&&allow_grid_x_driver)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver","bounds_y_driver"});
            else if(allow_grid_y_driver)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_y_driver"});
            else if(allow_grid_x_expression)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver","bounds_x_expression"});
            else if(allow_grid_x_driver)
                keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter","bounds_x_driver"});
            else keys(grid,{"id","bounds","columns","rows","column_gutter","row_gutter"});
            if(!grid.contains("id")||!grid.contains("bounds")||!grid.contains("columns")||!grid.contains("rows")||
               !grid.contains("column_gutter")||!grid.contains("row_gutter"))
                throw Error("INVALID_LAYOUT","Grid requires id, bounds, counts and gutters");
            if(!grid.at("id").is_string()||!grid.at("bounds").is_object())
                throw Error("INVALID_LAYOUT","Grid id must be a string and bounds must be an object");
            const auto& bounds=grid.at("bounds").as_object();keys(bounds,{"x","y","width","height"});
            if(!bounds.contains("x")||!bounds.contains("y")||!bounds.contains("width")||!bounds.contains("height"))
                throw Error("INVALID_LAYOUT","Grid bounds require x, y, width and height");
            layout.grid=Grid{text(grid.at("id")),
                {layout_number(bounds.at("x")),layout_number(bounds.at("y")),layout_number(bounds.at("width")),layout_number(bounds.at("height"))},
                layout_count(grid.at("columns")),layout_count(grid.at("rows")),
                layout_number(grid.at("column_gutter")),layout_number(grid.at("row_gutter"))};
            if(allow_grid_columns_driver)if(const auto* driver=grid.if_contains("columns_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.grid->columns_driver=read_ref(fields.at("link"));
            }
            if(allow_grid_columns_expression)if(const auto* expression=grid.if_contains("columns_expression"))
                layout.grid->columns_expression=read_expression(*expression);
            if(allow_grid_rows_driver)if(const auto* driver=grid.if_contains("rows_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.grid->rows_driver=read_ref(fields.at("link"));
            }
            if(allow_grid_rows_expression)if(const auto* expression=grid.if_contains("rows_expression"))
                layout.grid->rows_expression=read_expression(*expression);
            if(allow_grid_x_driver)if(const auto* driver=grid.if_contains("bounds_x_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.grid->bounds_x_driver=read_ref(fields.at("link"));
            }
            if(allow_grid_x_expression)if(const auto* expression=grid.if_contains("bounds_x_expression"))
                layout.grid->bounds_x_expression=read_expression(*expression);
            if(allow_grid_y_driver)if(const auto* driver=grid.if_contains("bounds_y_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.grid->bounds_y_driver=read_ref(fields.at("link"));
            }
            if(allow_grid_y_expression)if(const auto* expression=grid.if_contains("bounds_y_expression"))
                layout.grid->bounds_y_expression=read_expression(*expression);
            if(allow_grid_width_driver)if(const auto* driver=grid.if_contains("bounds_width_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.grid->bounds_width_driver=read_ref(fields.at("link"));
            }
            if(allow_grid_width_expression)if(const auto* expression=grid.if_contains("bounds_width_expression"))
                layout.grid->bounds_width_expression=read_expression(*expression);
            if(allow_grid_height_driver)if(const auto* driver=grid.if_contains("bounds_height_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.grid->bounds_height_driver=read_ref(fields.at("link"));
            }
            if(allow_grid_height_expression)if(const auto* expression=grid.if_contains("bounds_height_expression"))
                layout.grid->bounds_height_expression=read_expression(*expression);
            if(allow_grid_column_gutter_driver)if(const auto* driver=grid.if_contains("column_gutter_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.grid->column_gutter_driver=read_ref(fields.at("link"));
            }
            if(allow_grid_column_gutter_expression)if(const auto* expression=grid.if_contains("column_gutter_expression"))
                layout.grid->column_gutter_expression=read_expression(*expression);
            if(allow_grid_row_gutter_driver)if(const auto* driver=grid.if_contains("row_gutter_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});
                layout.grid->row_gutter_driver=read_ref(fields.at("link"));
            }
            if(allow_grid_row_gutter_expression)if(const auto* expression=grid.if_contains("row_gutter_expression"))
                layout.grid->row_gutter_expression=read_expression(*expression);
        }
        return layout;
    } catch(const Error& error) {
        if(error.code=="INVALID_LAYOUT")throw;
        throw Error("INVALID_LAYOUT",error.what());
    } catch(const std::exception& error) {
        throw Error("INVALID_LAYOUT",error.what());
    }
}
j::value grid_json(const Grid& grid) {
    j::object result{{"id",grid.id},
        {"bounds",j::object{{"x",grid.bounds.x},{"y",grid.bounds.y},
            {"width",grid.bounds.width},{"height",grid.bounds.height}}},
        {"columns",grid.columns},{"rows",grid.rows},
        {"column_gutter",grid.column_gutter},{"row_gutter",grid.row_gutter}};
    if(grid.columns_driver)result["columns_driver"]=j::object{{"link",ref_json(*grid.columns_driver)}};
    if(grid.columns_expression)result["columns_expression"]=expression_json(*grid.columns_expression);
    if(grid.rows_driver)result["rows_driver"]=j::object{{"link",ref_json(*grid.rows_driver)}};
    if(grid.rows_expression)result["rows_expression"]=expression_json(*grid.rows_expression);
    if(grid.bounds_x_driver)result["bounds_x_driver"]=j::object{{"link",ref_json(*grid.bounds_x_driver)}};
    if(grid.bounds_x_expression)result["bounds_x_expression"]=expression_json(*grid.bounds_x_expression);
    if(grid.bounds_y_driver)result["bounds_y_driver"]=j::object{{"link",ref_json(*grid.bounds_y_driver)}};
    if(grid.bounds_y_expression)result["bounds_y_expression"]=expression_json(*grid.bounds_y_expression);
    if(grid.bounds_width_driver)result["bounds_width_driver"]=j::object{{"link",ref_json(*grid.bounds_width_driver)}};
    if(grid.bounds_width_expression)result["bounds_width_expression"]=expression_json(*grid.bounds_width_expression);
    if(grid.bounds_height_driver)result["bounds_height_driver"]=j::object{{"link",ref_json(*grid.bounds_height_driver)}};
    if(grid.bounds_height_expression)result["bounds_height_expression"]=expression_json(*grid.bounds_height_expression);
    if(grid.column_gutter_driver)result["column_gutter_driver"]=j::object{{"link",ref_json(*grid.column_gutter_driver)}};
    if(grid.column_gutter_expression)result["column_gutter_expression"]=expression_json(*grid.column_gutter_expression);
    if(grid.row_gutter_driver)result["row_gutter_driver"]=j::object{{"link",ref_json(*grid.row_gutter_driver)}};
    if(grid.row_gutter_expression)result["row_gutter_expression"]=expression_json(*grid.row_gutter_expression);
    return result;
}
j::value layout_json(const ArtboardLayout& layout) {
    j::object result;
    if(layout.margin) {
        j::object margin{{"left",layout.margin->left},{"top",layout.margin->top},
            {"right",layout.margin->right},{"bottom",layout.margin->bottom}};
        if(layout.margin->left_driver)margin["left_driver"]=j::object{{"link",ref_json(*layout.margin->left_driver)}};
        if(layout.margin->left_expression)margin["left_expression"]=expression_json(*layout.margin->left_expression);
        if(layout.margin->top_driver)margin["top_driver"]=j::object{{"link",ref_json(*layout.margin->top_driver)}};
        if(layout.margin->top_expression)margin["top_expression"]=expression_json(*layout.margin->top_expression);
        if(layout.margin->right_driver)margin["right_driver"]=j::object{{"link",ref_json(*layout.margin->right_driver)}};
        if(layout.margin->right_expression)margin["right_expression"]=expression_json(*layout.margin->right_expression);
        if(layout.margin->bottom_driver)margin["bottom_driver"]=j::object{{"link",ref_json(*layout.margin->bottom_driver)}};
        if(layout.margin->bottom_expression)margin["bottom_expression"]=expression_json(*layout.margin->bottom_expression);
        result["margin"]=std::move(margin);
    }
    if(layout.grid)result["grid"]=grid_json(*layout.grid);
    return result;
}

Artboard::SizeDriver read_artboard_size_driver(const j::value& value) {
    const auto& driver=value.as_object();
    if(driver.contains("link")){keys(driver,{"link"});return Artboard::SizeDriver{read_ref(driver.at("link"))};}
    if(driver.contains("expression")){keys(driver,{"expression"});return Artboard::SizeDriver{read_expression(driver.at("expression"))};}
    throw Error("INVALID_ARTBOARD_DRIVER","Artboard size driver requires exactly one link or expression");
}
j::object artboard_size_driver_json(const Artboard::SizeDriver& driver) {
    if(const auto* link=std::get_if<Ref>(&driver.value))return {{"link",ref_json(*link)}};
    return {{"expression",expression_json(std::get<Expression>(driver.value))}};
}
ArtboardTemplate read_artboard_template(const j::value& value) {
    const auto& object=value.as_object();keys(object,{"id","name","source_artboard","definition"});
    ArtboardTemplate result{text(object.at("id")),text(object.at("name")),text(object.at("source_artboard")),{}};
    if(const auto* definition=object.if_contains("definition");definition&&!definition->is_null())result.definition=text(*definition);
    return result;
}
j::object artboard_template_json(const ArtboardTemplate& value) {
    return {{"id",value.id},{"name",value.name},{"source_artboard",value.source_artboard},
        {"definition",value.definition?j::value(*value.definition):j::value(nullptr)}};
}
ArtboardGuide read_artboard_guide(const j::value& value) {
    const auto& object=value.as_object();keys(object,{"id","name","axis","position","enabled"});
    return {text(object.at("id")),text(object.at("name")),text(object.at("axis")),
        number(object.at("position")),object.at("enabled").as_bool()};
}
j::object artboard_guide_json(const ArtboardGuide& value) {
    return {{"id",value.id},{"name",value.name},{"axis",value.axis},
        {"position",value.position},{"enabled",value.enabled}};
}
ArtboardTemplateAssignment read_template_assignment(const j::value& value,bool allow_guide_state=false,bool allow_background=false) {
    const auto& object=value.as_object();
    std::vector<std::string_view> allowed{"template_id","grid_id","content_instance","width_override","height_override",
        "margin_overridden","grid_overridden"};
    if(allow_guide_state) {
        allowed.push_back("guide_position_overrides");allowed.push_back("guide_enabled_overrides");
        allowed.push_back("detached_guides");
    }
    if(allow_background)allowed.push_back("background_overridden");
    keys(object,allowed);
    ArtboardTemplateAssignment result;
    result.template_id=text(object.at("template_id"));result.grid_id=text(object.at("grid_id"));
    if(const auto* content=object.if_contains("content_instance");content&&!content->is_null())result.content_instance=text(*content);
    if(const auto* width=object.if_contains("width_override");width&&!width->is_null())result.width_override=number(*width);
    if(const auto* height=object.if_contains("height_override");height&&!height->is_null())result.height_override=number(*height);
    result.margin_overridden=object.at("margin_overridden").as_bool();
    result.grid_overridden=object.at("grid_overridden").as_bool();
    if(allow_background)if(const auto* flag=object.if_contains("background_overridden"))result.background_overridden=flag->as_bool();
    if(allow_guide_state) {
        if(const auto* entries=object.if_contains("guide_position_overrides"))for(const auto& entry:entries->as_array()) {
            const auto& fields=entry.as_object();keys(fields,{"guide_id","position"});
            const auto guide_id=text(fields.at("guide_id"));
            if(!result.guide_position_overrides.emplace(guide_id,number(fields.at("position"))).second)
                throw Error("DUPLICATE_ARTBOARD_GUIDE_OVERRIDE",guide_id);
        }
        if(const auto* entries=object.if_contains("guide_enabled_overrides"))for(const auto& entry:entries->as_array()) {
            const auto& fields=entry.as_object();keys(fields,{"guide_id","enabled"});
            const auto guide_id=text(fields.at("guide_id"));
            if(!result.guide_enabled_overrides.emplace(guide_id,fields.at("enabled").as_bool()).second)
                throw Error("DUPLICATE_ARTBOARD_GUIDE_OVERRIDE",guide_id);
        }
        if(const auto* detached=object.if_contains("detached_guides"))result.detached_guides=ids(*detached);
    }
    return result;
}
j::object template_assignment_json(const ArtboardTemplateAssignment& value) {
    j::object result{{"template_id",value.template_id},{"grid_id",value.grid_id},
        {"content_instance",value.content_instance?j::value(*value.content_instance):j::value(nullptr)},
        {"width_override",value.width_override?j::value(*value.width_override):j::value(nullptr)},
        {"height_override",value.height_override?j::value(*value.height_override):j::value(nullptr)},
        {"margin_overridden",value.margin_overridden},{"grid_overridden",value.grid_overridden}};
    if(value.background_overridden)result["background_overridden"]=true;
    if(!value.guide_position_overrides.empty()) {
        j::array entries;for(const auto& [guide_id,position]:value.guide_position_overrides)
            entries.push_back(j::object{{"guide_id",guide_id},{"position",position}});
        result["guide_position_overrides"]=std::move(entries);
    }
    if(!value.guide_enabled_overrides.empty()) {
        j::array entries;for(const auto& [guide_id,enabled]:value.guide_enabled_overrides)
            entries.push_back(j::object{{"guide_id",guide_id},{"enabled",enabled}});
        result["guide_enabled_overrides"]=std::move(entries);
    }
    if(!value.detached_guides.empty())result["detached_guides"]=ids_json(value.detached_guides);
    return result;
}
Artboard read_artboard(const j::value& v,bool allow_parent=true,bool allow_layout=false,bool allow_size_driver=false,
    bool allow_margin_driver=false,bool allow_grid_x_driver=false,bool allow_grid_x_expression=false,
    bool allow_margin_expression=false,bool allow_grid_y_driver=false,bool allow_grid_y_expression=false,
    bool allow_margin_top_driver=false,bool allow_margin_top_expression=false,bool allow_margin_right_driver=false,
    bool allow_margin_right_expression=false,bool allow_margin_bottom_driver=false,bool allow_margin_bottom_expression=false,
    bool allow_grid_width_driver=false,bool allow_grid_width_expression=false,bool allow_grid_height_driver=false,
    bool allow_grid_height_expression=false,bool allow_grid_column_gutter_driver=false,
    bool allow_grid_row_gutter_driver=false,bool allow_grid_row_gutter_expression=false,
    bool allow_grid_column_gutter_expression=false,bool allow_grid_columns_driver=false,bool allow_grid_rows_driver=false,
    bool allow_grid_columns_expression=false,bool allow_grid_rows_expression=false,bool allow_template=false,
    bool allow_local_guides=false,bool allow_background=false) {
    const auto& a=v.as_object();
    std::vector<std::string_view> allowed{"id","name","x","y","width","height"};
    if(allow_parent)allowed.push_back("parent_size");
    if(allow_layout)allowed.push_back("layout");
    if(allow_size_driver){allowed.push_back("width_driver");allowed.push_back("height_driver");}
    if(allow_template)allowed.push_back("template_assignment");
    if(allow_local_guides)allowed.push_back("local_guides");
    if(allow_background)allowed.push_back("background");
    keys(a,allowed);
    Artboard result{text(a.at("id")),text(a.at("name")),number(a.at("x")),number(a.at("y")),number(a.at("width")),number(a.at("height"))};
    if(const auto* p=a.if_contains("parent_size")) {
        const auto& parent=p->as_object();keys(parent,{"artboard","width","height"});
        result.parent_size=ArtboardParent{text(parent.at("artboard")),parent.at("width").as_bool(),parent.at("height").as_bool()};
    }
    if(allow_layout)if(const auto* layout=a.if_contains("layout")) {
        if(layout->is_null())throw Error("INVALID_LAYOUT","Artboard layout must be omitted or an object; clear it with set_artboard_layout");
        result.layout=read_layout(*layout,allow_margin_driver,allow_grid_x_driver,allow_grid_x_expression,
            allow_margin_expression,allow_grid_y_driver,allow_grid_y_expression,allow_margin_top_driver,
            allow_margin_top_expression,allow_margin_right_driver,allow_margin_right_expression,allow_margin_bottom_driver,
            allow_margin_bottom_expression,allow_grid_width_driver,allow_grid_width_expression,allow_grid_height_driver,
            allow_grid_height_expression,allow_grid_column_gutter_driver,allow_grid_row_gutter_driver,
            allow_grid_row_gutter_expression,allow_grid_column_gutter_expression,allow_grid_columns_driver,allow_grid_rows_driver,
            allow_grid_columns_expression,allow_grid_rows_expression);
    }
    if(allow_size_driver) {
        if(const auto* driver=a.if_contains("width_driver"))result.width_driver=read_artboard_size_driver(*driver);
        if(const auto* driver=a.if_contains("height_driver"))result.height_driver=read_artboard_size_driver(*driver);
    }
    if(allow_template)if(const auto* assignment=a.if_contains("template_assignment"))
        result.template_assignment=read_template_assignment(*assignment,allow_local_guides,allow_background);
    if(allow_background)if(const auto* background=a.if_contains("background"))result.background=read_color(*background);
    if(allow_local_guides)if(const auto* guides=a.if_contains("local_guides"))
        for(const auto& value:guides->as_array())result.local_guides.push_back(read_artboard_guide(value));
    return result;
}
j::object artboard_json(const Artboard& a) {
    j::object result{{"id",a.id},{"name",a.name},{"x",a.x},{"y",a.y},{"width",a.width},{"height",a.height}};
    if(a.background)result["background"]=color_json(*a.background);
    if(a.parent_size)result["parent_size"]=j::object{{"artboard",a.parent_size->artboard},{"width",a.parent_size->width},{"height",a.parent_size->height}};
    if(a.layout)result["layout"]=layout_json(*a.layout);
    if(a.width_driver)result["width_driver"]=artboard_size_driver_json(*a.width_driver);
    if(a.height_driver)result["height_driver"]=artboard_size_driver_json(*a.height_driver);
    if(a.template_assignment)result["template_assignment"]=template_assignment_json(*a.template_assignment);
    if(!a.local_guides.empty()) {
        j::array guides;for(const auto& guide:a.local_guides)guides.push_back(artboard_guide_json(guide));
        result["local_guides"]=std::move(guides);
    }
    return result;
}

PresetCommand read_preset_command(const j::value& v) {
    auto& o=v.as_object();
    auto type=text(o.at("type"));
    if(type=="create_preset") {
        keys(o,{"type","definition"});return PresetCommand{CreatePreset{read_preset_definition(o.at("definition"))}};
    }
    if(type=="create_preset_from_stack") {
        keys(o,{"type","id","object","label","category","tags","schema_version"});PresetDefinition metadata;
        if(const auto* version=o.if_contains("schema_version"))metadata.schema_version=j::value_to<unsigned>(*version);
        metadata.id=text(o.at("id"));metadata.label=text(o.at("label"));
        if(const auto* category=o.if_contains("category"))metadata.category=text(*category);
        if(const auto* tags=o.if_contains("tags"))for(const auto& tag:tags->as_array())metadata.tags.push_back(text(tag));
        return PresetCommand{CreatePresetFromStack{std::move(metadata),text(o.at("object"))}};
    }
    if(type=="rename_preset") {
        keys(o,{"type","preset","label"});return PresetCommand{RenamePreset{text(o.at("preset")),text(o.at("label"))}};
    }
    if(type=="update_preset") {
        keys(o,{"type","definition"});return PresetCommand{UpdatePreset{read_preset_definition(o.at("definition"))}};
    }
    if(type=="delete_preset") {
        keys(o,{"type","preset"});return PresetCommand{DeletePreset{text(o.at("preset"))}};
    }
    if(type=="apply_preset") {
        keys(o,{"type","preset","object","operation_id_prefix"});
        return PresetCommand{ApplyPreset{text(o.at("preset")),text(o.at("object")),text(o.at("operation_id_prefix"))}};
    }
    if(type=="import_apply_preset") {
        keys(o,{"type","definition","definition_id","object","operation_id_prefix","asset_id","accepted_revision"});
        return PresetCommand{ImportAndApplyPreset{read_preset_definition(o.at("definition")),
            text(o.at("definition_id")),text(o.at("object")),text(o.at("operation_id_prefix")),
            text(o.at("asset_id")),preset_unsigned(o.at("accepted_revision"),std::numeric_limits<std::uint64_t>::max(),
                "Preset asset accepted revision")}};
    }
    throw Error("UNSUPPORTED_PRESET_OPERATION",type);
}

DefinitionCommand read_definition_command(const j::value& v) {
    const auto& o=v.as_object();const auto type=text(o.at("type"));
    if(type=="create_definition") {
        keys(o,{"type","id","name","root"});
        return DefinitionCommand{CreateDefinition{{text(o.at("id")),text(o.at("name")),text(o.at("root"))}}};
    }
    if(type=="rename_definition") {
        keys(o,{"type","definition","name"});return DefinitionCommand{RenameDefinition{text(o.at("definition")),text(o.at("name"))}};
    }
    if(type=="delete_definition") {
        keys(o,{"type","definition"});return DefinitionCommand{DeleteDefinition{text(o.at("definition"))}};
    }
    if(type=="create_instance") {
        keys(o,{"type","composition","parent","id","definition","name"});
        return DefinitionCommand{CreateInstance{text(o.at("composition")),text(o.at("parent")),text(o.at("id")),
            text(o.at("definition")),text(o.at("name"))}};
    }
    if(type=="set_instance_override") {
        keys(o,{"type","instance","target","value"});
        return DefinitionCommand{SetInstanceOverride{text(o.at("instance")),read_ref(o.at("target")),number(o.at("value"))}};
    }
    if(type=="reset_instance_override") {
        keys(o,{"type","instance","target"});
        return DefinitionCommand{ResetInstanceOverride{text(o.at("instance")),read_ref(o.at("target"))}};
    }
    if(type=="set_instance_visibility_override") {
        keys(o,{"type","instance","source","visible"});
        return DefinitionCommand{SetInstanceVisibilityOverride{text(o.at("instance")),text(o.at("source")),o.at("visible").as_bool()}};
    }
    if(type=="reset_instance_visibility_override") {
        keys(o,{"type","instance","source"});
        return DefinitionCommand{ResetInstanceVisibilityOverride{text(o.at("instance")),text(o.at("source"))}};
    }
    if(type=="set_instance_color_override") {
        keys(o,{"type","instance","target","value"});
        return DefinitionCommand{SetInstanceColorOverride{text(o.at("instance")),read_ref(o.at("target")),read_color(o.at("value"))}};
    }
    if(type=="reset_instance_color_override") {
        keys(o,{"type","instance","target"});
        return DefinitionCommand{ResetInstanceColorOverride{text(o.at("instance")),read_ref(o.at("target"))}};
    }
    if(type=="set_instance_text_content_override") {
        keys(o,{"type","instance","source","content"});
        return DefinitionCommand{SetInstanceTextContentOverride{text(o.at("instance")),text(o.at("source")),text(o.at("content"))}};
    }
    if(type=="reset_instance_text_content_override") {
        keys(o,{"type","instance","source"});
        return DefinitionCommand{ResetInstanceTextContentOverride{text(o.at("instance")),text(o.at("source"))}};
    }
    if(type=="detach_instance") {
        keys(o,{"type","instance","id_prefix"});
        return DefinitionCommand{DetachInstance{text(o.at("instance")),text(o.at("id_prefix"))}};
    }
    throw Error("UNSUPPORTED_DEFINITION_OPERATION",type);
}

ArtboardTemplateCommand read_artboard_template_command(const j::value& value) {
    const auto& object=value.as_object();const auto type=text(object.at("type"));
    if(type=="set_artboard_background") {
        keys(object,{"type","composition","artboard","value"});
        return ArtboardTemplateCommand{SetArtboardBackground{text(object.at("composition")),text(object.at("artboard")),
            object.at("value").is_null()?std::optional<ColorValue>{}:std::optional<ColorValue>{read_color(object.at("value"))}}};
    }
    if(type=="create_artboard_template") {
        keys(object,{"type","composition","id","name","source_artboard","definition"});
        std::optional<Id> definition;
        if(const auto* item=object.if_contains("definition");item&&!item->is_null())definition=text(*item);
        return ArtboardTemplateCommand{CreateArtboardTemplate{text(object.at("composition")),
            ArtboardTemplate{text(object.at("id")),text(object.at("name")),text(object.at("source_artboard")),definition}}};
    }
    if(type=="rename_artboard_template") {
        keys(object,{"type","composition","template","name"});
        return ArtboardTemplateCommand{RenameArtboardTemplate{text(object.at("composition")),text(object.at("template")),text(object.at("name"))}};
    }
    if(type=="delete_artboard_template") {
        keys(object,{"type","composition","template"});
        return ArtboardTemplateCommand{DeleteArtboardTemplate{text(object.at("composition")),text(object.at("template"))}};
    }
    if(type=="assign_artboard_template") {
        keys(object,{"type","composition","artboard","template","content_instance"});
        std::optional<Id> content;
        if(const auto* item=object.if_contains("content_instance");item&&!item->is_null())content=text(*item);
        return ArtboardTemplateCommand{AssignArtboardTemplate{text(object.at("composition")),text(object.at("artboard")),
            text(object.at("template")),content}};
    }
    if(type=="set_artboard_template_override") {
        keys(object,{"type","composition","artboard","field","value"});
        const auto field=text(object.at("field"));
        std::variant<double,std::optional<Margin>,std::optional<Grid>,std::optional<ColorValue>> value_payload;
        if(field=="frame.width"||field=="frame.height")value_payload=number(object.at("value"));
        else if(field=="layout.margin") {
            if(object.at("value").is_null())value_payload=std::optional<Margin>{};
            else {const auto layout=read_layout(j::object{{"margin",object.at("value")}});value_payload=layout.margin;}
        } else if(field=="layout.grid") {
            if(object.at("value").is_null())value_payload=std::optional<Grid>{};
            else {const auto layout=read_layout(j::object{{"grid",object.at("value")}});value_payload=layout.grid;}
        } else if(field=="background")value_payload=object.at("value").is_null()?std::optional<ColorValue>{}:std::optional<ColorValue>{read_color(object.at("value"))};
        else throw Error("UNSUPPORTED_TEMPLATE_OVERRIDE",field);
        return ArtboardTemplateCommand{SetArtboardTemplateOverride{text(object.at("composition")),
            text(object.at("artboard")),field,std::move(value_payload)}};
    }
    if(type=="reset_artboard_template_override") {
        keys(object,{"type","composition","artboard","field"});
        return ArtboardTemplateCommand{ResetArtboardTemplateOverride{text(object.at("composition")),
            text(object.at("artboard")),text(object.at("field"))}};
    }
    if(type=="duplicate_template_artboard") {
        keys(object,{"type","composition","artboard","id_prefix","x","y","index"});
        return ArtboardTemplateCommand{DuplicateTemplateArtboard{text(object.at("composition")),
            text(object.at("artboard")),text(object.at("id_prefix")),number(object.at("x")),
            number(object.at("y")),j::value_to<std::size_t>(object.at("index"))}};
    }
    if(type=="detach_artboard_template") {
        keys(object,{"type","composition","artboard","id_prefix"});
        return ArtboardTemplateCommand{DetachArtboardTemplate{text(object.at("composition")),
            text(object.at("artboard")),text(object.at("id_prefix"))}};
    }
    throw Error("UNSUPPORTED_ARTBOARD_TEMPLATE_OPERATION",type);
}

ArtboardGuideCommand read_artboard_guide_command(const j::value& value) {
    const auto& object=value.as_object();const auto type=text(object.at("type"));
    const auto composition=text(object.at("composition"));
    const auto artboard=text(object.at("artboard"));
    if(type=="add_artboard_guide") {
        keys(object,{"type","composition","artboard","guide"});
        return ArtboardGuideCommand{AddArtboardGuide{composition,artboard,read_artboard_guide(object.at("guide"))}};
    }
    if(type=="update_artboard_guide") {
        keys(object,{"type","composition","artboard","guide"});
        return ArtboardGuideCommand{UpdateArtboardGuide{composition,artboard,read_artboard_guide(object.at("guide"))}};
    }
    if(type=="delete_artboard_guide") {
        keys(object,{"type","composition","artboard","guide_id"});
        return ArtboardGuideCommand{DeleteArtboardGuide{composition,artboard,text(object.at("guide_id"))}};
    }
    if(type=="set_artboard_guide_override") {
        keys(object,{"type","composition","artboard","guide_id","field","value"});
        const auto field=text(object.at("field"));
        std::variant<double,bool> payload;
        if(field=="position")payload=number(object.at("value"));
        else if(field=="enabled")payload=object.at("value").as_bool();
        else throw Error("UNSUPPORTED_ARTBOARD_GUIDE_OVERRIDE",field);
        return ArtboardGuideCommand{SetArtboardGuideOverride{composition,artboard,
            text(object.at("guide_id")),field,std::move(payload)}};
    }
    if(type=="reset_artboard_guide_override") {
        keys(object,{"type","composition","artboard","guide_id","field"});
        return ArtboardGuideCommand{ResetArtboardGuideOverride{composition,artboard,
            text(object.at("guide_id")),text(object.at("field"))}};
    }
    if(type=="detach_artboard_guide") {
        keys(object,{"type","composition","artboard","guide_id","new_guide_id"});
        return ArtboardGuideCommand{DetachArtboardGuide{composition,artboard,
            text(object.at("guide_id")),text(object.at("new_guide_id"))}};
    }
    throw Error("UNSUPPORTED_ARTBOARD_GUIDE_OPERATION",type);
}

CollectionCommand read_collection_command(const j::value& v) {
    const auto& o=v.as_object();const auto type=text(o.at("type"));
    if(type=="create_collection") {
        keys(o,{"type","id","name","members"});
        return CollectionCommand{CreateCollection{{text(o.at("id")),text(o.at("name")),ids(o.at("members"))}}};
    }
    if(type=="rename_collection") {
        keys(o,{"type","collection","name"});
        return CollectionCommand{RenameCollection{text(o.at("collection")),text(o.at("name"))}};
    }
    if(type=="set_collection_members") {
        keys(o,{"type","collection","members"});
        return CollectionCommand{SetCollectionMembers{text(o.at("collection")),ids(o.at("members"))}};
    }
    if(type=="delete_collection") {
        keys(o,{"type","collection"});
        return CollectionCommand{DeleteCollection{text(o.at("collection"))}};
    }
    throw Error("UNSUPPORTED_COLLECTION_OPERATION",type);
}

MacroCommand read_macro_command(const j::value& value) {
    const auto& object=value.as_object();const auto type=text(object.at("type"));
    if(type=="create_macro_definition") {
        keys(object,{"type","definition"});return MacroCommand{CreateMacroDefinition{read_macro_definition(object.at("definition"))}};
    }
    if(type=="rename_macro_definition") {
        keys(object,{"type","definition","label"});return MacroCommand{RenameMacroDefinition{text(object.at("definition")),text(object.at("label"))}};
    }
    if(type=="update_macro_definition") {
        keys(object,{"type","definition","revision"});return MacroCommand{UpdateMacroDefinition{text(object.at("definition")),read_macro_revision(object.at("revision"))}};
    }
    if(type=="delete_macro_definition") {
        keys(object,{"type","definition"});return MacroCommand{DeleteMacroDefinition{text(object.at("definition"))}};
    }
    if(type=="import_apply_macro") {
        keys(object,{"type","definition","definition_id","object","instance","pinned_revision","index",
            "overrides","asset_id","accepted_revision"});
        std::map<std::string,double> overrides;
        if(const auto* supplied=object.if_contains("overrides")) {
            if(!supplied->is_object())throw Error("INVALID_MACRO_OVERRIDES","Macro import overrides must be an object of stable PublicParamID numeric values");
            for(const auto& [public_id,value]:supplied->as_object())
                overrides.emplace(std::string(public_id),number(value));
        }
        const auto accepted_revision=macro_unsigned(object.at("accepted_revision"),
            9007199254740991ULL,"Macro asset accepted revision");
        const auto pinned_revision=macro_unsigned(object.at("pinned_revision"),
            std::numeric_limits<std::uint64_t>::max(),"Pinned Macro revision");
        const auto insertion_index=macro_unsigned(object.at("index"),
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()),"Macro insertion index",true);
        return MacroCommand{InstantiateMacro{text(object.at("object")),text(object.at("definition_id")),
            text(object.at("instance")),pinned_revision,static_cast<std::size_t>(insertion_index),read_macro_definition(object.at("definition")),
            text(object.at("asset_id")),accepted_revision,std::move(overrides)}};
    }
    if(type=="instantiate_macro"||type=="apply_macro") {
        keys(object,{"type","object","definition","instance","revision","index"});
        return MacroCommand{InstantiateMacro{text(object.at("object")),text(object.at("definition")),text(object.at("instance")),
            j::value_to<std::uint64_t>(object.at("revision")),j::value_to<std::size_t>(object.at("index"))}};
    }
    if(type=="set_macro_override") {
        keys(object,{"type","object","instance","public_parameter","value"});
        return MacroCommand{SetMacroOverride{text(object.at("object")),text(object.at("instance")),text(object.at("public_parameter")),number(object.at("value"))}};
    }
    if(type=="reset_macro_override") {
        keys(object,{"type","object","instance","public_parameter"});
        return MacroCommand{ResetMacroOverride{text(object.at("object")),text(object.at("instance")),text(object.at("public_parameter"))}};
    }
    if(type=="update_macro_instance") {
        keys(object,{"type","object","instance","revision"});
        return MacroCommand{UpdateMacroInstance{text(object.at("object")),text(object.at("instance")),j::value_to<std::uint64_t>(object.at("revision"))}};
    }
    if(type=="detach_macro_instance") {
        keys(object,{"type","object","instance","operation_id_prefix"});
        return MacroCommand{DetachMacroInstance{text(object.at("object")),text(object.at("instance")),text(object.at("operation_id_prefix"))}};
    }
    throw Error("UNSUPPORTED_MACRO_OPERATION",type);
}

bool is_preset_command(const j::value& v) {
    const auto type=text(v.as_object().at("type"));
    return type=="create_preset"||type=="create_preset_from_stack"||type=="rename_preset"||
        type=="update_preset"||type=="delete_preset"||type=="apply_preset"||type=="import_apply_preset";
}

Command read_command(const j::value& v) {
    auto& o=v.as_object();
    auto type=text(o.at("type"));
    if(type=="create_artboard_template"||type=="rename_artboard_template"||type=="delete_artboard_template"||
        type=="assign_artboard_template"||type=="set_artboard_background"||type=="set_artboard_template_override"||
        type=="reset_artboard_template_override"||type=="detach_artboard_template"||type=="duplicate_template_artboard")
        return StructuralCommand{read_artboard_template_command(v)};
    if(type=="add_artboard_guide"||type=="update_artboard_guide"||type=="delete_artboard_guide"||
        type=="set_artboard_guide_override"||type=="reset_artboard_guide_override"||type=="detach_artboard_guide")
        return StructuralCommand{read_artboard_guide_command(v)};
    if(type.find("_macro_")!=std::string::npos||type=="instantiate_macro"||type=="apply_macro"||
        type=="import_apply_macro")return StructuralCommand{read_macro_command(v)};
    if(type.ends_with("_definition")||type=="create_instance"||type=="set_instance_override"||
        type=="reset_instance_override"||type=="set_instance_visibility_override"||
        type=="reset_instance_visibility_override"||type=="set_instance_color_override"||
        type=="reset_instance_color_override"||type=="set_instance_text_content_override"||
        type=="reset_instance_text_content_override"||type=="detach_instance")return read_definition_command(v);
    if(type=="create_collection"||type=="rename_collection"||type=="set_collection_members"||
        type=="delete_collection")return read_collection_command(v);
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
    if(type=="add_text_font_feature") {
        keys(o,{"type","object","feature"});return AddTextFontFeature{text(o.at("object")),read_text_font_feature(o.at("feature"))};
    }
    if(type=="update_text_font_feature") {
        keys(o,{"type","object","feature_tag","parameter"});
        return UpdateTextFontFeature{text(o.at("object")),text(o.at("feature_tag")),uint32_number(o.at("parameter"),"feature parameter")};
    }
    if(type=="remove_text_font_feature") {
        keys(o,{"type","object","feature_tag"});return RemoveTextFontFeature{text(o.at("object")),text(o.at("feature_tag"))};
    }
    if(type=="set_text_additional_axis") {
        keys(o,{"type","object","axis_tag","value"});
        return SetTextAdditionalAxis{text(o.at("object")),text(o.at("axis_tag")),number(o.at("value"))};
    }
    if(type=="remove_text_additional_axis") {
        keys(o,{"type","object","axis_tag"});return RemoveTextAdditionalAxis{text(o.at("object")),text(o.at("axis_tag"))};
    }
    if(type=="link_text_italic") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkTextItalic{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_text_italic_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetTextItalicExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_italic") {
        keys(o,{"type","target"});return UnlinkTextItalic{read_ref(o.at("target"))};
    }
    if(type=="link_text_weight") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkTextWeight{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_text_weight_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetTextWeightExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_weight") {
        keys(o,{"type","target"});return UnlinkTextWeight{read_ref(o.at("target"))};
    }
    if(type=="edit_text_weights") {
        keys(o,{"type","targets","value","relative"});
        std::vector<Ref> targets;for(const auto& target:o.at("targets").as_array())targets.push_back(read_ref(target));
        return TextWeightBatch{TextWeightBatchMode::edit,std::move(targets),signed_integer(o.at("value")),{},o.at("relative").as_bool(),false};
    }
    if(type=="link_text_weights") {
        keys(o,{"type","targets","source","relative","replace_driver"});
        std::vector<Ref> targets;for(const auto& target:o.at("targets").as_array())targets.push_back(read_ref(target));
        return TextWeightBatch{TextWeightBatchMode::link,std::move(targets),0,read_ref(o.at("source")),
            o.at("relative").as_bool(),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_weights") {
        keys(o,{"type","targets"});
        std::vector<Ref> targets;for(const auto& target:o.at("targets").as_array())targets.push_back(read_ref(target));
        return TextWeightBatch{TextWeightBatchMode::unlink,std::move(targets),0,{},false,false};
    }
    if(type=="link_text_content") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkTextContent{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_content") {
        keys(o,{"type","target"});return UnlinkTextContent{read_ref(o.at("target"))};
    }
    if(type=="link_text_family") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkTextFamily{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_family") {
        keys(o,{"type","target"});return UnlinkTextFamily{read_ref(o.at("target"))};
    }
    if(type=="link_text_locale") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkTextLocale{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_locale") {
        keys(o,{"type","target"});return UnlinkTextLocale{read_ref(o.at("target"))};
    }
    if(type=="link_text_direction") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkTextDirection{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_direction") {
        keys(o,{"type","target"});return UnlinkTextDirection{read_ref(o.at("target"))};
    }
    if(type=="link_text_layout") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkTextLayout{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_layout") {
        keys(o,{"type","target"});return UnlinkTextLayout{read_ref(o.at("target"))};
    }
    if(type=="link_text_alignment") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkTextAlignment{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_text_alignment") {
        keys(o,{"type","target"});return UnlinkTextAlignment{read_ref(o.at("target"))};
    }
    if(type=="link_object_visibility") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkObjectVisibility{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_object_visibility_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetObjectVisibilityExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_object_visibility") {
        keys(o,{"type","target"});return UnlinkObjectVisibility{read_ref(o.at("target"))};
    }
    if(type=="link_composite_isolated") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkCompositeIsolated{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_composite_isolated") {
        keys(o,{"type","target"});return UnlinkCompositeIsolated{read_ref(o.at("target"))};
    }
    if(type=="set_composite_isolated_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetCompositeIsolatedExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="link_operation_enabled") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkOperationEnabled{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_operation_enabled_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetOperationEnabledExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_operation_enabled") {
        keys(o,{"type","target"});return UnlinkOperationEnabled{read_ref(o.at("target"))};
    }
    if(type=="link_gradient_enabled") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkGradientEnabled{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_gradient_enabled_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetGradientEnabledExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_gradient_enabled") {
        keys(o,{"type","target"});return UnlinkGradientEnabled{read_ref(o.at("target"))};
    }
    if(type=="link_mask_enabled") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkMaskEnabled{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_mask_enabled_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetMaskEnabledExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_mask_enabled") {
        keys(o,{"type","target"});return UnlinkMaskEnabled{read_ref(o.at("target"))};
    }
    if(type=="link_fill_rule") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkFillRule{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_fill_rule") {
        keys(o,{"type","target"});return UnlinkFillRule{read_ref(o.at("target"))};
    }
    if(type=="add_artboard") {
        keys(o,{"type","composition","artboard","index"});
        return AddArtboard{text(o.at("composition")),read_artboard(o.at("artboard"),true,true,false,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true),j::value_to<std::size_t>(o.at("index"))};
    }
    if(type=="update_artboard") {
        keys(o,{"type","composition","artboard"});return UpdateArtboard{text(o.at("composition")),read_artboard(o.at("artboard"),true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true)};
    }
    if(type=="add_guide"||type=="update_guide") {
        keys(o,{"type","composition","guide"});
        if(type=="add_guide")return AddGuide{text(o.at("composition")),read_guide(o.at("guide"))};
        return UpdateGuide{text(o.at("composition")),read_guide(o.at("guide"),true,true)};
    }
    if(type=="delete_guide") {
        keys(o,{"type","composition","guide_id"});return DeleteGuide{text(o.at("composition")),text(o.at("guide_id"))};
    }
    if(type=="link_guide_position") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkGuidePosition{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_guide_position_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetGuidePositionExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_guide_position") {
        keys(o,{"type","target"});return UnlinkGuidePosition{read_ref(o.at("target"))};
    }
    if(type=="set_artboard_layout") {
        keys(o,{"type","composition","artboard_id","layout"});
        std::optional<ArtboardLayout> layout;if(!o.at("layout").is_null())layout=read_layout(o.at("layout"),
            true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true);
        return SetArtboardLayout{text(o.at("composition")),text(o.at("artboard_id")),std::move(layout)};
    }
    if(type=="link_margin_left") {
        keys(o,{"type","target","source","replace_driver"});
        return MarginLeftCommand{LinkMarginLeft{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_margin_left_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return MarginLeftCommand{SetMarginLeftExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_margin_left") {
        keys(o,{"type","target"});return MarginLeftCommand{UnlinkMarginLeft{read_ref(o.at("target"))}};
    }
    if(type=="link_margin_top") {
        keys(o,{"type","target","source","replace_driver"});
        return MarginTopCommand{LinkMarginTop{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_margin_top_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return MarginTopCommand{SetMarginTopExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_margin_top") {
        keys(o,{"type","target"});return MarginTopCommand{UnlinkMarginTop{read_ref(o.at("target"))}};
    }
    if(type=="link_margin_right") {
        keys(o,{"type","target","source","replace_driver"});
        return MarginRightCommand{LinkMarginRight{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_margin_right_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return MarginRightCommand{SetMarginRightExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_margin_right") {
        keys(o,{"type","target"});return MarginRightCommand{UnlinkMarginRight{read_ref(o.at("target"))}};
    }
    if(type=="link_margin_bottom") {
        keys(o,{"type","target","source","replace_driver"});
        return MarginBottomCommand{LinkMarginBottom{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_margin_bottom") {
        keys(o,{"type","target"});return MarginBottomCommand{UnlinkMarginBottom{read_ref(o.at("target"))}};
    }
    if(type=="set_margin_bottom_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return MarginBottomCommand{SetMarginBottomExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="link_grid_bounds_x") {
        keys(o,{"type","target","source","replace_driver"});
        return GridBoundsXCommand{LinkGridBoundsX{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="link_grid_columns") {
        keys(o,{"type","target","source","replace_driver"});
        return GridColumnsCommand{LinkGridColumns{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_grid_columns_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return GridColumnsCommand{SetGridColumnsExpression{read_ref(o.at("target")),
            read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_grid_columns") {
        keys(o,{"type","target"});return GridColumnsCommand{UnlinkGridColumns{read_ref(o.at("target"))}};
    }
    if(type=="link_grid_rows") {
        keys(o,{"type","target","source","replace_driver"});
        return GridRowsCommand{LinkGridRows{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_grid_rows_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return GridRowsCommand{SetGridRowsExpression{read_ref(o.at("target")),
            read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_grid_rows") {
        keys(o,{"type","target"});return GridRowsCommand{UnlinkGridRows{read_ref(o.at("target"))}};
    }
    if(type=="unlink_grid_bounds_x") {
        keys(o,{"type","target"});return GridBoundsXCommand{UnlinkGridBoundsX{read_ref(o.at("target"))}};
    }
    if(type=="link_grid_bounds_y") {
        keys(o,{"type","target","source","replace_driver"});
        return GridBoundsYCommand{LinkGridBoundsY{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_grid_bounds_y") {
        keys(o,{"type","target"});return GridBoundsYCommand{UnlinkGridBoundsY{read_ref(o.at("target"))}};
    }
    if(type=="link_grid_bounds_width") {
        keys(o,{"type","target","source","replace_driver"});
        return GridBoundsWidthCommand{LinkGridBoundsWidth{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_grid_bounds_width") {
        keys(o,{"type","target"});return GridBoundsWidthCommand{UnlinkGridBoundsWidth{read_ref(o.at("target"))}};
    }
    if(type=="link_grid_bounds_height") {
        keys(o,{"type","target","source","replace_driver"});
        return GridBoundsHeightCommand{LinkGridBoundsHeight{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_grid_bounds_height") {
        keys(o,{"type","target"});return GridBoundsHeightCommand{UnlinkGridBoundsHeight{read_ref(o.at("target"))}};
    }
    if(type=="link_grid_column_gutter") {
        keys(o,{"type","target","source","replace_driver"});
        return GridColumnGutterCommand{LinkGridColumnGutter{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_grid_column_gutter") {
        keys(o,{"type","target"});return GridColumnGutterCommand{UnlinkGridColumnGutter{read_ref(o.at("target"))}};
    }
    if(type=="set_grid_column_gutter_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return GridColumnGutterCommand{SetGridColumnGutterExpression{read_ref(o.at("target")),
            read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="link_grid_row_gutter") {
        keys(o,{"type","target","source","replace_driver"});
        return GridRowGutterCommand{LinkGridRowGutter{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()}};
    }
    if(type=="unlink_grid_row_gutter") {
        keys(o,{"type","target"});return GridRowGutterCommand{UnlinkGridRowGutter{read_ref(o.at("target"))}};
    }
    if(type=="set_grid_row_gutter_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return GridRowGutterCommand{SetGridRowGutterExpression{read_ref(o.at("target")),
            read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_grid_bounds_y_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return GridBoundsYCommand{SetGridBoundsYExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_grid_bounds_width_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return GridBoundsWidthCommand{SetGridBoundsWidthExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_grid_bounds_height_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return GridBoundsHeightCommand{SetGridBoundsHeightExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="set_grid_bounds_x_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return GridBoundsXCommand{SetGridBoundsXExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()}};
    }
    if(type=="delete_artboard"||type=="detach_artboard_parent") {
        keys(o,{"type","composition","artboard"});
        if(type=="delete_artboard")return DeleteArtboard{text(o.at("composition")),text(o.at("artboard"))};
        return DetachArtboardParent{text(o.at("composition")),text(o.at("artboard"))};
    }
    if(type=="link_artboard_size") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkArtboardSize{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_artboard_size_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetArtboardSizeExpression{read_ref(o.at("target")),read_expression(o.at("expression")),o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_artboard_size") {
        keys(o,{"type","target"});return UnlinkArtboardSize{read_ref(o.at("target"))};
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
    if(type=="stroke_style") {
        keys(o,{"type","object","operation","line_cap","line_join","miter_limit"});
        return StrokeStyle{text(o.at("object")),text(o.at("operation")),text(o.at("line_cap")),text(o.at("line_join")),number(o.at("miter_limit"))};
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
    if(type=="link_point_edit_enabled") {
        keys(o,{"type","target","source","replace_driver"});
        return LinkPointEditEnabled{read_ref(o.at("target")),read_ref(o.at("source")),o.at("replace_driver").as_bool()};
    }
    if(type=="set_point_edit_enabled_expression") {
        keys(o,{"type","target","expression","replace_driver"});
        return SetPointEditEnabledExpression{read_ref(o.at("target")),read_expression(o.at("expression")),
            o.at("replace_driver").as_bool()};
    }
    if(type=="unlink_point_edit_enabled") {
        keys(o,{"type","target"});return UnlinkPointEditEnabled{read_ref(o.at("target"))};
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
    if(type=="create_folder") {
        keys(o,{"type","composition","parent","id","name"});
        return CreateFolder{text(o.at("composition")),text(o.at("parent")),text(o.at("id")),text(o.at("name"))};
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
    if(type=="set_compositing") {
        keys(o,{"type","object","blend","isolated","profile"});
        const auto mode=text(o.at("blend"));const auto* descriptor=find_blend_mode(mode);
        if(!descriptor)throw Error("UNSUPPORTED_BLEND",mode);
        if(const auto* profile=o.if_contains("profile")) {
            const auto selected=text(*profile);
            if(std::none_of(descriptor->profiles.begin(),descriptor->profiles.end(),[&](const auto& supported){return supported.id==selected;}))
                throw Error("UNSUPPORTED_BLEND_PROFILE",selected);
        }
        return SetCompositing{text(o.at("object")),mode,o.at("isolated").as_bool()};
    }
    if(type=="set_mask") {
        keys(o,{"type","object","mask"});std::optional<GeometryMask> mask;
        if(!o.at("mask").is_null())mask=read_mask(o.at("mask"),false,false,true,false,true,false);
        return SetMask{text(o.at("object")),std::move(mask)};
    }
    if(type=="mask_objects") {
        keys(o,{"type","composition","parent","members","id","mask_id","name","top"});
        return MaskObjects{text(o.at("composition")),text(o.at("parent")),ids(o.at("members")),text(o.at("id")),text(o.at("mask_id")),text(o.at("name")),o.at("top").as_bool()};
    }
    if(type=="ungroup") {
        keys(o,{"type","composition","parent","group"});
        return Ungroup{text(o.at("composition")),text(o.at("parent")),text(o.at("group"))};
    }
    if(type=="move_out") {
        keys(o,{"type","composition","parent","group","members","placement"});
        return MoveOut{text(o.at("composition")),text(o.at("parent")),text(o.at("group")),ids(o.at("members")),text(o.at("placement"))};
    }
    if(type=="put_inside") {
        keys(o,{"type","composition","parent","group","members"});
        return PutInside{text(o.at("composition")),text(o.at("parent")),text(o.at("group")),ids(o.at("members"))};
    }
    if(type=="attach_group_path_follow") {
        keys(o,{"type","group","relation"});return GroupPathFollowCommand{AttachGroupPathFollow{text(o.at("group")),read_group_path_follow(o.at("relation"))}};
    }
    if(type=="update_group_path_follow") {
        keys(o,{"type","group","relation"});return GroupPathFollowCommand{UpdateGroupPathFollow{text(o.at("group")),read_group_path_follow(o.at("relation"))}};
    }
    if(type=="clear_group_path_follow") {
        keys(o,{"type","group"});return GroupPathFollowCommand{ClearGroupPathFollow{text(o.at("group"))}};
    }
    if(type=="set_group_path_follow_item") {
        keys(o,{"type","group","object","item"});return GroupPathFollowCommand{SetGroupPathFollowItem{text(o.at("group")),text(o.at("object")),read_group_path_follow_item(o.at("item"))}};
    }
    if(type=="remove_group_path_follow_item") {
        keys(o,{"type","group","object"});return GroupPathFollowCommand{RemoveGroupPathFollowItem{text(o.at("group")),text(o.at("object"))}};
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
        keys(o,{"type","objects","axis","reference","spacing"});
        std::optional<double> spacing;
        if(o.contains("spacing")&&!o.at("spacing").is_null())spacing=number(o.at("spacing"));
        return DistributeObjects{ids(o.at("objects")),text(o.at("axis")),
            o.contains("reference")?text(o.at("reference")):"selection",spacing};
    }
    if(type=="align_objects") {
        keys(o,{"type","objects","axis","alignment","artboard","reference","guide_artboard"});
        if(o.contains("artboard")&&o.contains("reference"))
            throw Error("INVALID_REFERENCE","Specify either reference or the legacy artboard alias, not both");
        std::optional<Id> artboard;
        if(o.contains("artboard")&&!o.at("artboard").is_null())artboard=text(o.at("artboard"));
        std::optional<Id> guide_artboard;
        if(o.contains("guide_artboard")&&!o.at("guide_artboard").is_null())guide_artboard=text(o.at("guide_artboard"));
        return AlignObjects{ids(o.at("objects")),text(o.at("axis")),text(o.at("alignment")),artboard,
            o.contains("reference")?text(o.at("reference")):"selection",guide_artboard};
    }
    if(type=="transform_objects") {
        keys(o,{"type","objects","rotation","scale_x","scale_y","pivot"});
        std::optional<std::array<double,2>> pivot;
        if(!o.at("pivot").is_null()) {
            const auto& pair=o.at("pivot").as_array();if(pair.size()!=2)throw Error("INVALID_COMMAND","Pivot must contain x and y");
            pivot=std::array<double,2>{number(pair[0]),number(pair[1])};
        }
        return TransformObjects{ids(o.at("objects")),number(o.at("rotation")),number(o.at("scale_x")),number(o.at("scale_y")),pivot};
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

PortablePresetAssetEnvelope read_portable_preset_asset_envelope(std::string_view input) {
    j::value parsed;
    try {parsed=parse(input);}
    catch(const Error& error) {throw Error("INVALID_PRESET_ASSET",std::string("Preset asset envelope JSON is malformed: ")+error.what());}
    catch(const std::exception& error) {throw Error("INVALID_PRESET_ASSET",std::string("Preset asset envelope JSON is malformed: ")+error.what());}
    if(!parsed.is_object())throw Error("INVALID_PRESET_ASSET","Preset asset envelope must be a JSON object");
    const auto& object=parsed.as_object();
    static const std::set<std::string> allowed{"version","kind","asset_id","accepted_revision","sha256","payload_schema","payload","label","provenance"};
    for(const auto& member:object) {
        const std::string key(member.key().data(),member.key().size());
        if(!allowed.contains(key))throw Error("INVALID_PRESET_ASSET","Unknown Preset asset envelope field: "+key);
    }
    for(const auto* required:{"version","kind","asset_id","accepted_revision","sha256","payload_schema","payload","label"})
        if(!object.contains(required))throw Error("INVALID_PRESET_ASSET","Preset asset envelope is missing field: "+std::string(required));
    if(const auto provenance=object.find("provenance");provenance!=object.end()&&!provenance->value().is_object())
        throw Error("INVALID_PRESET_ASSET","Preset asset provenance must be an object when present");
    const auto positive_integer=[](const j::value& value,std::string_view field) {
        std::uint64_t result=0;
        if(value.is_uint64())result=value.as_uint64();
        else if(value.is_int64()) {
            const auto signed_value=value.as_int64();
            if(signed_value<0)throw Error("INVALID_PRESET_ASSET",std::string(field)+" must be positive");
            result=static_cast<std::uint64_t>(signed_value);
        } else throw Error("INVALID_PRESET_ASSET",std::string(field)+" must be an integer JSON number");
        if(result==0||result>9007199254740991ULL)
            throw Error("INVALID_PRESET_ASSET",std::string(field)+" is outside its supported positive integer range");
        return result;
    };
    const auto required_string=[&](std::string_view key) {
        const auto found=object.find(key);
        if(found==object.end()||!found->value().is_string())
            throw Error("INVALID_PRESET_ASSET","Preset asset envelope has a missing or mistyped string field: "+std::string(key));
        return text(found->value());
    };
    return {positive_integer(object.at("version"),"Envelope version"),
        positive_integer(object.at("accepted_revision"),"Accepted revision"),
        positive_integer(object.at("payload_schema"),"Payload schema"),
        required_string("kind"),required_string("asset_id"),required_string("sha256"),
        required_string("payload"),required_string("label")};
}

Document decode(std::string_view input) {
    try {
        auto parsed=parse(input);
        const auto& root=parsed.as_object();
        const auto version=text(root.at("version"));
        constexpr std::array<std::string_view,86> supported{"0.1","0.2","0.3","0.4","0.5","0.6","0.7","0.8","0.9","0.10","0.11","0.12","0.13","0.14","0.15","0.16","0.17","0.18","0.19","0.20","0.21","0.22","0.23","0.24","0.25","0.26","0.27","0.28","0.29","0.30","0.31","0.32","0.33","0.34","0.35","0.36","0.37","0.38","0.39","0.40","0.41","0.42","0.43","0.44","0.45","0.46","0.47","0.48","0.49","0.50","0.51","0.52","0.53","0.54","0.55","0.56","0.57","0.58","0.59","0.60","0.61","0.62","0.63","0.64","0.65","0.66","0.67","0.68","0.69","0.70","0.71","0.72","0.73","0.74","0.75","0.76","0.77","0.78","0.79","0.80","0.81","0.82","0.83","0.84","0.85","0.86"};
        const auto accepted=std::find(supported.begin(),supported.end(),version);
        if(text(root.at("format"))!="nect-native"||accepted==supported.end())
            throw Error("UNSUPPORTED_FORMAT","Only nect-native 0.1 through 0.86 are supported");
        const auto minor=std::distance(supported.begin(),accepted)+1;
        if(minor<86)if(const auto* definitions=root.if_contains("macros"))
            for(const auto& definition:definitions->as_array())for(const auto& revision:definition.at("revisions").as_array())
                if(revision.as_object().contains("interface_version"))throw Error("NATIVE_VERSION_MISMATCH","Macro interface versions require native 0.86");
        if(minor<85)for(const auto& object:root.at("objects").as_array()) {
            if(const auto* instance=object.as_object().if_contains("instance");instance&&instance->as_object().contains("text_content_overrides"))
                throw Error("NATIVE_VERSION_MISMATCH","Instance Text content overrides require native 0.85");
        }
        if(minor<84)for(const auto& object:root.at("objects").as_array()) {
            if(const auto* instance=object.as_object().if_contains("instance");instance&&instance->as_object().contains("color_overrides"))
                throw Error("NATIVE_VERSION_MISMATCH","Instance color overrides require native 0.84");
        }
        if(minor<83)if(const auto* macros=root.if_contains("macros"))
            for(const auto& definition:macros->as_array())for(const auto& revision:definition.as_object().at("revisions").as_array())
                if(revision.as_object().contains("graph_version"))throw Error("NATIVE_VERSION_MISMATCH","Macro graph versions require native 0.83");
        if(minor<82)for(const auto& object:root.at("objects").as_array()) {
            if(const auto* instance=object.as_object().if_contains("instance");instance&&instance->as_object().contains("visibility_overrides"))
                throw Error("NATIVE_VERSION_MISMATCH","Instance visibility overrides require native 0.82");
        }
        if(minor<81)for(const auto& composition:root.at("compositions").as_array())for(const auto& board:composition.as_object().at("artboards").as_array()){
            const auto& fields=board.as_object();const auto* assignment=fields.if_contains("template_assignment");
            if(fields.contains("background")||(assignment&&assignment->as_object().contains("background_overridden")))
                throw Error("NATIVE_VERSION_MISMATCH","Artboard background requires native 0.81");
        }
        // Introduction gate precedes historical-field validation so an Object
        // carrying a new mode cannot claim any older native version, even one
        // that predates the compositing field. Definition sources live here too.
        for(const auto& value:root.at("objects").as_array()) {
            const auto* composite=value.as_object().if_contains("compositing");
            if(!composite)continue;
            const auto* blend=composite->as_object().if_contains("blend");
            if(!blend)continue;
            const auto id=text(*blend);const auto* descriptor=find_blend_mode(id);
            if(!descriptor)throw Error("UNSUPPORTED_BLEND",id);
            if(static_cast<unsigned>(minor)<descriptor->introduced_native_minor)
                throw Error("NATIVE_VERSION_MISMATCH",id+" requires native 0."+std::to_string(descriptor->introduced_native_minor)+" or later");
        }
        if(minor>=65)keys(root,{"format","version","id","units","color_space","compositions","objects","collections","named_colors","raster_assets","presets","definitions","macros"});
        else if(minor>=64)keys(root,{"format","version","id","units","color_space","compositions","objects","collections","named_colors","raster_assets","presets","definitions"});
        else if(minor>=63)keys(root,{"format","version","id","units","color_space","compositions","objects","collections","named_colors","raster_assets","presets"});
        else if(minor>=13)keys(root,{"format","version","id","units","color_space","compositions","objects","collections","named_colors","raster_assets"});
        else if(minor>=7)keys(root,{"format","version","id","units","color_space","compositions","objects","collections","named_colors"});
        else keys(root,{"format","version","id","units","color_space","compositions","objects","collections"});
        if(text(root.at("units"))!="du96"||text(root.at("color_space"))!="srgb")
            throw Error("UNSUPPORTED_COLOR_OR_UNIT","v0.1 supports du96 and sRGB only");

        Document d;
        d.id=text(root.at("id"));
        if(minor>=63)for(const auto& value:root.at("presets").as_array()) {
            auto preset=read_preset_definition(value);
            if(minor<66&&preset.schema_version>=2)
                throw Error("NATIVE_VERSION_MISMATCH","Preset schema v2 requires native 0.66 or later");
            if(!d.preset_definitions.emplace(preset.id,preset).second)
                throw Error("DUPLICATE_PRESET_ID",preset.id);
        }
        if(minor>=64)for(const auto& value:root.at("definitions").as_array()) {
            auto definition=read_definition(value);
            if(!d.definitions.emplace(definition.id,definition).second)
                throw Error("DUPLICATE_DEFINITION_ID",definition.id);
        }
        if(minor>=65)for(const auto& value:root.at("macros").as_array()) {
            auto definition=read_macro_definition(value);const auto id=definition.id;
            if(!d.macro_definitions.emplace(id,std::move(definition)).second)
                throw Error("DUPLICATE_MACRO_ID",id);
        }
        std::map<Id,ShapeOperation> legacy_paints;

        for(const auto& cv:root.at("compositions").as_array()) {
            auto& co=cv.as_object();
            if(minor>=76)keys(co,{"id","name","roots","artboards","guides","templates"});
            else if(minor>=14)keys(co,{"id","name","roots","artboards","guides"});
            else keys(co,{"id","name","roots","artboards"});
            Composition c;
            c.id=text(co.at("id"));
            c.name=text(co.at("name"));
            c.roots=ids(co.at("roots"));

            for(const auto& av:co.at("artboards").as_array())c.artboards.push_back(read_artboard(av,minor>=5,minor>=14,minor>=33,minor>=35,minor>=36,minor>=37,minor>=38,minor>=39,minor>=40,minor>=41,minor>=42,minor>=43,minor>=44,minor>=45,minor>=46,minor>=47,minor>=48,minor>=49,minor>=50,minor>=51,minor>=52,minor>=53,minor>=54,minor>=56,minor>=57,minor>=58,minor>=59,minor>=76,minor>=77,minor>=81));
            if(minor>=14)for(const auto& gv:co.at("guides").as_array())c.guides.push_back(read_guide(gv,minor>=23,minor>=34));
            if(minor>=76)for(const auto& tv:co.at("templates").as_array())c.templates.push_back(read_artboard_template(tv));
            d.compositions.push_back(std::move(c));
        }

        for(const auto& ov:root.at("objects").as_array()) {
            auto& o=ov.as_object();
            if(version=="0.1")keys(o,{"id","name","kind","transform","children","contours","stroke","fill"});
            else if(version=="0.2")keys(o,{"id","name","kind","transform","children","contours","stroke","fill","source","point_edit"});
            else if(minor>=74)keys(o,{"id","name","visible","compositing","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke","image","visibility_driver","visibility_expression","instance","path_follow"});
            else if(minor>=64)keys(o,{"id","name","visible","compositing","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke","image","visibility_driver","visibility_expression","instance"});
            else if(minor>=60)keys(o,{"id","name","visible","compositing","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke","image","visibility_driver","visibility_expression"});
            else if(minor>=27)keys(o,{"id","name","visible","compositing","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke","image","visibility_driver"});
            else if(minor>=13)keys(o,{"id","name","visible","compositing","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke","image"});
            else if(minor>=11)keys(o,{"id","name","visible","compositing","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke"});
            else if(minor>=9)keys(o,{"id","name","kind","transform","anchor","transform_parent","children","contours","source","point_edit","text","stack","legacy_stroke"});
            else if(minor>=6)keys(o,{"id","name","kind","transform","children","contours","source","point_edit","text","stack","legacy_stroke"});
            else keys(o,{"id","name","kind","transform","children","contours","source","point_edit","stack","legacy_stroke"});

            Object obj;
            obj.id=text(o.at("id"));
            obj.name=text(o.at("name"));
            if(minor>=11){obj.visible=o.at("visible").as_bool();obj.compositing=read_compositing(o.at("compositing"),minor>=30,minor>=31,minor>=62,minor>=68,minor>=71,minor>=71,minor>=72,minor>=72);
                const auto* blend=find_blend_mode(obj.compositing.blend);
                if(!blend)throw Error("UNSUPPORTED_BLEND",obj.compositing.blend);
                if(static_cast<unsigned>(minor)<blend->introduced_native_minor)
                    throw Error("NATIVE_VERSION_MISMATCH",obj.compositing.blend+" requires native 0."+std::to_string(blend->introduced_native_minor)+" or later");}
            if(minor>=27)if(const auto* driver=o.if_contains("visibility_driver")) {
                const auto& fields=driver->as_object();keys(fields,{"link"});obj.visibility_driver=read_ref(fields.at("link"));
            }
            if(minor>=60)if(const auto* expression=o.if_contains("visibility_expression"))
                obj.visibility_expression=read_expression(*expression);
            if(obj.visibility_driver&&obj.visibility_expression)
                throw Error("INVALID_VISIBILITY_SOURCE","Object visibility link and expression are mutually exclusive");

            auto kind=text(o.at("kind"));
            if(kind!="group"&&kind!="path"&&!((minor>=6)&&kind=="text")&&!((minor>=13)&&kind=="image")&&!((minor>=64)&&kind=="instance")) throw Error("UNSUPPORTED_OBJECT",kind);
            if(kind!="group"&&o.contains("path_follow"))throw Error("INVALID_PATH_FOLLOW","Only a Group can own a Path Follow relation");
            obj.kind=kind=="group"?Kind::group:kind=="text"?Kind::text:kind=="image"?Kind::image:kind=="instance"?Kind::instance:Kind::path;
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

            if(obj.kind==Kind::instance) {
                for(const auto* field:{"children","contours","stroke","fill","source","point_edit","text","stack","legacy_stroke","image"})
                    if(o.contains(field))throw Error("INVALID_INSTANCE","Instance cannot carry source content");
                obj.instance=read_instance(o.at("instance"));
            } else if(obj.kind==Kind::image) {
                for(const auto* field:{"children","contours","stroke","fill","source","point_edit","text","stack","legacy_stroke"})
                    if(o.contains(field))throw Error("INVALID_IMAGE","Image has incompatible vector fields");
                obj.image=read_image(o.at("image"));
            } else if(obj.kind==Kind::group) {
                if(o.contains("contours")||o.contains("stroke")||o.contains("fill")||o.contains("source")||o.contains("point_edit")||o.contains("legacy_stroke"))
                    throw Error("INVALID_OBJECT","Group has path-only fields");
                if(minor>=25) {
                    if(!o.contains("stack"))throw Error("INVALID_OBJECT","Native 0.25 Group requires an operation stack");
                    for(const auto& entry:o.at("stack").as_array())obj.stack.push_back(read_operation(entry,true,true,true,true,
                        minor>=26,minor>=28,minor>=29,minor>=67,minor>=69));
                } else if(o.contains("stack"))throw Error("INVALID_OBJECT","Group operation stacks require native 0.25");
                obj.children=ids(o.at("children"));
                if(minor>=74)if(const auto* follow=o.if_contains("path_follow"))obj.path_follow=read_group_path_follow(*follow,minor>=75);
            } else {
                if(o.contains("children")) throw Error("INVALID_OBJECT","Path has children");
                if(minor>=3) {
                    for(const auto& entry:o.at("stack").as_array()) {
                        if(minor>=65)obj.stack.push_back(read_processing_entry(entry,minor>=67,minor>=69));
                        else obj.stack.push_back(read_operation(entry,minor>=4,minor>=10,minor>=12,minor>=13,
                            minor>=26,minor>=28,minor>=29,minor>=67,minor>=69));
                    }
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
                    obj.text=read_text(o.at("text"),minor>=10,minor>=15,minor>=16,minor>=17,minor>=18,minor>=22,minor>=19,minor>=20,minor>=21,minor>=24,minor>=55,minor>=61,minor>=78);
                } else if(o.contains("source")) {
                    if(o.contains("contours"))throw Error("INVALID_OBJECT","Generator and authored contours are mutually exclusive");
                    obj.source=read_primitive(o.at("source"),minor>=8,minor>=10,minor>=73);
                    if(o.contains("point_edit"))obj.point_edit=read_point_edit(o.at("point_edit"),minor>=10,minor>=32,minor>=70);
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

        if(minor<76)for(const auto& [id,object]:d.objects)if(object.instance)for(const auto& [ref,value]:object.instance->overrides) {
            (void)value;
            if(ref.field=="transform.tx"||ref.field=="transform.ty"||
                ((ref.field=="generator.width"||ref.field=="generator.height")&&
                    d.objects.contains(ref.object)&&d.objects.at(ref.object).source&&
                    d.objects.at(ref.object).source->type=="nect.shape.rectangle"))
                throw Error("NATIVE_VERSION_MISMATCH","Descendant transform or Rectangle size overrides require native 0.76");
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

    j::array comps,objects,collections,named_colors,raster_assets,presets,definitions,macros;
    for(const auto& [id,asset]:d.raster_assets){(void)id;raster_assets.push_back(asset_json(asset,true));}
    for(const auto& [id,color]:d.named_colors){(void)id;named_colors.push_back(named_color_json(color));}
    for(const auto& [id,preset]:d.preset_definitions){(void)id;presets.push_back(preset_json(preset));}
    for(const auto& [id,definition]:d.definitions){(void)id;definitions.push_back(definition_json(definition));}
    for(const auto& [id,definition]:d.macro_definitions){(void)id;macros.push_back(macro_definition_json(definition));}

    for(const auto& c:d.compositions) {
        j::array boards,guides,templates;
        for(const auto& a:c.artboards)boards.push_back(artboard_json(a));
        for(const auto& guide:c.guides)guides.push_back(guide_json(guide));
        for(const auto& item:c.templates)templates.push_back(artboard_template_json(item));
        comps.push_back({
            {"id",c.id},{"name",c.name},{"roots",ids_json(c.roots)},{"artboards",boards},{"guides",guides},{"templates",templates}});
    }

    for(const auto& [id,o]:d.objects) {
        j::array tf;
        for(const auto& s:o.transform) tf.push_back(scalar_json(s));
        j::array anchor;for(const auto& s:o.anchor)anchor.push_back(scalar_json(s));

        j::object out{
            {"id",id},{"name",o.name},{"visible",o.visible},{"compositing",compositing_json(o.compositing)},{"kind",o.kind==Kind::group?"group":o.kind==Kind::text?"text":o.kind==Kind::image?"image":o.kind==Kind::instance?"instance":"path"},{"transform",tf},{"anchor",anchor},{"transform_parent",o.transform_parent?j::value(*o.transform_parent):j::value(nullptr)}};
        if(o.visibility_driver)out["visibility_driver"]=j::object{{"link",ref_json(*o.visibility_driver)}};
        if(o.visibility_expression)out["visibility_expression"]=expression_json(*o.visibility_expression);

        if(o.instance)out["instance"]=instance_json(*o.instance);
        else if(o.image)out["image"]=image_json(*o.image);
        else if(o.kind==Kind::group) {
            out["children"]=ids_json(o.children);
            j::array stack;for(const auto& op:o.stack)stack.push_back(operation_json(op));
            out["stack"]=stack;
            if(o.path_follow)out["path_follow"]=group_path_follow_json(*o.path_follow);
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
            j::array stack;for(const auto& op:o.stack)stack.push_back(processing_entry_json(op));
            out["stack"]=stack;out["legacy_stroke"]=o.legacy_stroke;
        }

        objects.push_back(out);
    }

    for(const auto& c:d.collections)
        collections.push_back({{"id",c.id},{"name",c.name},{"members",ids_json(c.members)}});

    return j::serialize(j::object{
        {"format","nect-native"},{"version",native_version},{"id",d.id},
        {"units","du96"},{"color_space","srgb"},
        {"compositions",comps},{"objects",objects},{"collections",collections},{"named_colors",named_colors},
        {"raster_assets",raster_assets},{"presets",presets},{"definitions",definitions},{"macros",macros}});
}

std::string canonical_preset_payload(const PresetDefinition& definition) {
    validate_portable_literal_preset(definition);
    const auto payload=canonical_json(preset_json(definition));
    if(payload.size()>portable_preset_payload_limit)
        throw Error("PRESET_PAYLOAD_LIMIT","Portable Preset payload exceeds 256 KiB");
    return payload;
}

std::string canonical_macro_payload(const MacroDefinition& definition) {
    validate_portable_macro_definition(definition);
    const auto payload=canonical_json(macro_definition_json(definition));
    if(payload.size()>portable_macro_payload_limit)
        throw Error("MACRO_PAYLOAD_LIMIT","Portable Macro payload exceeds 256 KiB");
    return payload;
}

MacroDefinition read_canonical_macro_payload(std::string_view input) {
    if(input.size()>portable_macro_payload_limit)
        throw Error("MACRO_PAYLOAD_LIMIT","Portable Macro payload exceeds 256 KiB");
    MacroDefinition definition;
    try {definition=read_macro_definition(parse(input));}
    catch(const Error& error) {
        if(error.code=="UNSUPPORTED_MACRO_NODE"||error.code=="UNSUPPORTED_MACRO_NODE_VERSION"||
            error.code=="INVALID_MACRO_GRAPH"||error.code=="INVALID_MACRO_ORDER"||
            error.code=="INVALID_MACRO_DOMAIN")throw;
        throw Error("UNAVAILABLE_MACRO_ASSET",std::string("Portable Macro payload is unavailable: ")+error.code+": "+error.what());
    } catch(const std::exception& error) {
        throw Error("UNAVAILABLE_MACRO_ASSET",std::string("Portable Macro payload schema is malformed: ")+error.what());
    }
    validate_portable_macro_definition(definition);
    if(canonical_macro_payload(definition)!=input)
        throw Error("NONCANONICAL_MACRO_PAYLOAD","Portable Macro payload is not in canonical serialized form");
    return definition;
}

PresetDefinition read_canonical_preset_payload(std::string_view input) {
    if(input.size()>portable_preset_payload_limit)
        throw Error("PRESET_PAYLOAD_LIMIT","Portable Preset payload exceeds 256 KiB");
    const auto definition=read_preset_definition(parse(input));
    validate_portable_literal_preset(definition);
    if(canonical_preset_payload(definition)!=input)
        throw Error("NONCANONICAL_PRESET_PAYLOAD","Portable Preset payload is not in canonical serialized form");
    return definition;
}

std::string export_svg(const Document& d,const Id& comp_id,const Id& art_id) {
    validate(d);
    const auto operation_enabled=evaluate_operation_enableds(d);
    const auto gradient_enabled=evaluate_gradient_enableds(d);
    const auto mask_enabled=evaluate_geometry_mask_enableds(d);
    auto comp=std::find_if(d.compositions.begin(),d.compositions.end(),
        [&](const auto& c){return c.id==comp_id;});
    if(comp==d.compositions.end()) throw Error("MISSING_COMPOSITION",comp_id);

    std::function<void(const Id&)> reject_unsupported=[&](const Id& id) {
        const auto& object=d.objects.at(id);
        if(object.compositing.mask&&object.compositing.mask->mode!="geometry"&&
            mask_enabled.at(geometry_mask_enabled_ref(id,object.compositing.mask->id))) {
            const auto mode=object.compositing.mask->mode;
            if(mode=="alpha")
                throw Error("UNSUPPORTED_SVG_ALPHA_MASK","SVG export cannot represent Alpha mask on Object "+id+" without a lossless projection");
            throw Error("UNSUPPORTED_SVG_LUMA_MASK","SVG export cannot represent Luma mask on Object "+id+" without a lossless projection");
        }
        for(const auto& operation:object.stack)if(
            operation_enabled.at(operation_ref(id,operation.id,"enabled"))&&operation.type=="nect.group.posterize")
            throw Error("UNSUPPORTED_SVG_EFFECT","SVG export cannot represent enabled Group Posterize instance "+operation.id+" on Group "+id);
        for(const auto& child:object.children)reject_unsupported(child);
    };
    for(const auto& id:comp->roots)reject_unsupported(id);

    const auto values=evaluate(d);
    const auto fill_rule_values=evaluate_fill_rules(d);
    const auto transforms=evaluate_transforms(d,values);
    const bool external_parenting=std::any_of(d.objects.begin(),d.objects.end(),[](const auto& entry){return entry.second.transform_parent.has_value();});

    const auto resolved=evaluate_artboard(*comp,art_id);const auto* art=&resolved;

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out<<std::setprecision(std::numeric_limits<double>::max_digits10);
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<art->width
       <<"px\" height=\""<<art->height<<"px\" viewBox=\""<<art->x<<" "<<art->y
       <<" "<<art->width<<" "<<art->height<<"\">\n";

    if(art->background){const auto& rgba=art->background->rgba;
        out<<"<rect x=\""<<art->x<<"\" y=\""<<art->y<<"\" width=\""<<art->width<<"\" height=\""<<art->height
            <<"\" fill=\"rgb("<<rgba[0]*100<<"%,"<<rgba[1]*100<<"%,"<<rgba[2]*100<<"%)\" fill-opacity=\""<<rgba[3]<<"\"/>\n";
        out<<"<g style=\"isolation:isolate\" color-interpolation=\"sRGB\">\n";
    }
    std::set<Id> svg_ids;for(const auto& [id,object]:d.objects){svg_ids.insert(id);if(object.compositing.mask)svg_ids.insert(object.compositing.mask->id);}
    const Document* render_document=&d;
    const std::map<Ref,double>* render_values=&values;
    std::size_t gradient_serial=0,raster_bytes=0;
    std::map<Id,std::string> raster_ids;
    const auto paint_image=[&](const Id& id) {
        const auto& source=*render_document->objects.at(id).image;
        if(!raster_ids.contains(source.asset)) {
            const auto png=encode_raster_png(decode_raster(*render_document->raster_assets.at(source.asset).payload));
            raster_bytes+=png.size();if(raster_bytes>32*1024*1024)throw Error("SVG_RASTER_LIMIT","SVG normalized image data exceeds 32 MiB");
            std::string key;do{key="nect-raster-"+std::to_string(++gradient_serial);}while(svg_ids.contains(key));svg_ids.insert(key);raster_ids.emplace(source.asset,key);
            out<<"<defs><image id=\""<<key<<"\" width=\"1\" height=\"1\" preserveAspectRatio=\"none\" href=\"data:image/png;base64,"<<base64_encode(png)<<"\"/></defs>\n";
        }
        out<<"<use href=\"#"<<raster_ids.at(source.asset)<<"\" transform=\"scale("<<render_values->at({id,"","image.width"})<<' '<<render_values->at({id,"","image.height"})<<")\"/>\n";
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
                else out<<"fill=\"none\" stroke-linecap=\""<<paint.line_cap<<"\" stroke-linejoin=\""<<paint.line_join<<"\" stroke-miterlimit=\""<<paint.miter_limit<<"\" stroke=\"";
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
    const auto visibility=evaluate_object_visibilities(d);
    const auto authored_isolation=evaluate_composite_isolations(d);
    bool modern=false;
    std::function<void(const Id&)> detect=[&](const Id& id){const auto& object=d.objects.at(id);const auto& c=object.compositing;
        modern=modern||(object.path_follow&&object.path_follow->mode=="deform")||object.kind==Kind::instance||object.image.has_value()||!visibility.at(id)||values.at({id,"","composite.opacity"})!=1||c.blend!="normal"||authored_isolation.at(id)||
            (c.mask&&mask_enabled.at(geometry_mask_enabled_ref(id,c.mask->id)));
        for(const auto& child:object.children)detect(child);};
    for(const auto& id:comp->roots)detect(id);
    if(modern) {
        const auto scene=evaluate_scene(d,comp_id,values,transforms);
        const auto unsupported_blends=unsupported_svg_blends(scene);
        if(!unsupported_blends.empty()) {
            const auto& issue=unsupported_blends.front().as_object();
            throw Error("UNSUPPORTED_SVG_BLEND","SVG cannot represent blend "+text(issue.at("blend"))+" on Object "+text(issue.at("object")));
        }
        if(scene.expanded_document) {
            render_document=scene.expanded_document.get();
            render_values=scene.expanded_values.get();
            for(const auto& [id,object]:render_document->objects){svg_ids.insert(id);if(object.compositing.mask)svg_ids.insert(object.compositing.mask->id);}
        }
        std::function<void(const EvaluatedSceneNode&)> render_node=[&](const EvaluatedSceneNode& node) {
            if(!node.visible||node.opacity<=0)return;
            const auto& object=render_document->objects.at(node.id);
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
            if(scene.deformation_owners.contains(node.id))out<<"<desc>Evaluated Group Path Deform derivative; retained source geometry and relation remain in native Nect.</desc>\n";
            if(object.text)out<<"<desc>Text outlined for SVG; editable source remains in native Nect.</desc>\n";
            out<<"<g transform=\"matrix(";for(const auto value:node.world)out<<value<<' ';out<<")\">\n";
                if(object.image)paint_image(node.id);else paint_shape(scene.shapes.at(node.id));out<<"</g>\n";
            }
            out<<"</g>\n";
        };
        for(const auto& node:scene.roots)render_node(node);if(art->background)out<<"</g>\n";out<<"</svg>\n";return out.str();
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
            paint_shape(evaluate_shape(d,id,values,&fill_rule_values,&operation_enabled,&gradient_enabled));
        }

        out<<"</g>\n";
    };

    for(const auto& id:comp->roots) render(id);
    if(art->background)out<<"</g>\n";
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
        std::map<Id,j::value> prior_guides,prior_grids;
        if(mutation) {
            prior=session.document();prior_values=evaluate(session.document());
            prior_transforms=evaluate_transforms(session.document(),prior_values);
            for(const auto& c:session.document().compositions) {
                for(const auto& a:c.artboards) {
                    prior_frames.emplace(a.id,j::object{{"authored",artboard_json(a)},{"evaluated",artboard_json(evaluate_artboard(c,a.id))}});
                    if(a.layout&&a.layout->grid)prior_grids.emplace(a.layout->grid->id,grid_json(*a.layout->grid));
                }
                for(const auto& guide:c.guides)prior_guides.emplace(guide.id,guide_json(guide));
            }
        }

        if(op=="get") {
            keys(o,{"op","ref"});
            auto r=read_ref(o.at("ref"));
            if(r.field.starts_with("text.")&&!is_text_readonly_field(r.field)&&r.field!="text.italic"&&r.field!="text.weight") {
                if(!r.point.empty())throw Error("INVALID_TEXT_REF","Text properties require an empty point ID");
                const auto object=session.document().objects.find(r.object);
                if(object==session.document().objects.end())throw Error("MISSING_REFERENCE",r.object);
                if(object->second.kind!=Kind::text||!object->second.text)throw Error("TYPE_MISMATCH","Text property Ref must identify a Text object");
                if(!object->second.text->parameters.contains(r.field.substr(5)))
                    throw Error("UNKNOWN_TEXT_PROPERTY","Unsupported Text source property: "+r.field);
            }
            if(r.field=="artboard.background")result=background_property_json(session.document(),r);
            else if(r.field.starts_with("margin.")||r.field.starts_with("grid."))
                result=artboard_layout_property_json(session.document(),r,artboard_layout_property(session.document(),r));
            else if(r.field=="guide.position")result=guide_position_property_json(
                guide_property_name(session.document(),r),r,guide_position_property(session.document(),r));
            else if(r.field.starts_with("artboard.guide."))
                result=artboard_guide_property_json(r,artboard_guide_property(session.document(),r),
                    property_name(session.document(),r));
            else if(r.field.starts_with("artboard."))result=artboard_size_property_json(session.document(),r,artboard_size_property(session.document(),r));
            else if(r.field=="object.visible")result=object_visibility_property_json(
                session.document(),r,object_visibility_state(session.document(),r));
            else if(r.field=="composite.isolated")result=composite_isolated_property_json(
                session.document(),r,composite_isolation_state(session.document(),r));
            else if(r.field=="mask.enabled")result=geometry_mask_enabled_property_json(
                session.document(),r,geometry_mask_enabled_property(session.document(),r));
            else if(r.field.starts_with("mask.")&&r.field.ends_with(".enabled"))result=geometry_mask_enabled_state_json(
                session.document(),r,geometry_mask_enabled_state(session.document(),r));
            else if(r.field=="point_edit.enabled")result=point_edit_enabled_property_json(
                session.document(),r,point_edit_enabled_property(session.document(),r));
            else if(r.field.starts_with("point_edit.")&&r.field.ends_with(".enabled"))result=point_edit_enabled_state_json(
                session.document(),r,point_edit_enabled_state(session.document(),r));
            else if(r.field.starts_with("op.")&&r.field.find(".gradient.")!=std::string::npos&&r.field.ends_with(".enabled"))
                result=gradient_enabled_property_json(session.document(),r,gradient_enabled_state(session.document(),r));
            else if(r.field.starts_with("op.")&&r.field.ends_with(".enabled"))result=operation_enabled_property_json(
                session.document(),r,operation_enabled_state(session.document(),r));
            else if(r.field.starts_with("op.")&&r.field.ends_with(".fill_rule"))result=fill_rule_property_json(session.document(),r,fill_rule_property(session.document(),r));
            else if(r.field.starts_with("macro.")&&!r.point.empty()) {
                const auto value=macro_parameter_value(session.document(),r.object,r.point,r.field);
                result=j::object{{"ref",ref_json(r)},{"origin","macro_public_parameter"},
                    {"authored",value},{"evaluated",value}};
            }
            else if(r.field=="text.content")result=text_content_property_json(session.document(),r,text_content_property(session.document(),r));
            else if(r.field=="text.family")result=text_family_property_json(session.document(),r,text_family_property(session.document(),r));
            else if(r.field=="text.locale")result=text_locale_property_json(session.document(),r,text_locale_property(session.document(),r));
            else if(r.field=="text.direction")result=text_direction_property_json(session.document(),r,text_direction_property(session.document(),r));
            else if(r.field=="text.layout")result=text_layout_property_json(session.document(),r,text_layout_property(session.document(),r));
            else if(r.field=="text.alignment")result=text_alignment_property_json(session.document(),r,text_alignment_property(session.document(),r));
            else if(is_text_readonly_field(r.field))result=text_readonly_property_json(session.document(),r,text_readonly_property(session.document(),r));
            else if(r.field=="text.italic")result=text_italic_property_json(session.document(),r,text_italic_property(session.document(),r));
            else if(r.field=="text.weight")result=text_weight_property_json(session.document(),r,text_weight_property(session.document(),r));
            else if(r.point.empty()&&(r.field=="color"||r.field.ends_with(".color")))result=color_property_json(session.document(),r,evaluate(session.document()));
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
        } else if(op=="presets") {
            keys(o,{"op"});j::array definitions;
            for(const auto& [id,definition]:session.document().preset_definitions){(void)id;definitions.push_back(preset_json(definition));}
            result=std::move(definitions);
        } else if(op=="definitions") {
            keys(o,{"op"});j::array definitions;
            for(const auto& [id,definition]:session.document().definitions){(void)id;definitions.push_back(definition_json(definition));}
            result=std::move(definitions);
        } else if(op=="instance_text_content") {
            keys(o,{"op","instance","source"});
            const auto instance=text(o.at("instance")),source=text(o.at("source"));
            const auto& document=session.document();
            const auto content=evaluate_instance_text_content(document,instance,source);
            result=j::object{{"instance",instance},{"source",source},{"content",content},
                {"source_content",evaluate_text_content(document,source)},
                {"overridden",document.objects.at(instance).instance->text_content_overrides.contains(source)}};
        } else if(op=="macros") {
            keys(o,{"op"});j::array definitions;
            for(const auto& [id,definition]:session.document().macro_definitions){(void)id;definitions.push_back(macro_definition_json(definition));}
            result=std::move(definitions);
        } else if(op=="macro") {
            keys(o,{"op","id"});const auto id=text(o.at("id"));
            const auto found=session.document().macro_definitions.find(id);
            if(found==session.document().macro_definitions.end())throw Error("MISSING_MACRO_DEFINITION",id);
            result=macro_definition_json(found->second);
        } else if(op=="collections") {
            keys(o,{"op"});j::array collections;
            for(const auto& collection:session.document().collections)
                collections.push_back({{"id",collection.id},{"name",collection.name},{"members",ids_json(collection.members)}});
            result=std::move(collections);
        } else if(op=="collection") {
            keys(o,{"op","id"});const auto id=text(o.at("id"));
            const auto found=std::find_if(session.document().collections.begin(),session.document().collections.end(),
                [&](const Collection& item){return item.id==id;});
            if(found==session.document().collections.end())throw Error("MISSING_COLLECTION",id);
            result=j::object{{"id",found->id},{"name",found->name},{"members",ids_json(found->members)}};
        } else if(op=="definition") {
            keys(o,{"op","id"});const auto id=text(o.at("id"));
            const auto found=session.document().definitions.find(id);
            if(found==session.document().definitions.end())throw Error("MISSING_DEFINITION",id);
            result=definition_json(found->second);
        } else if(op=="preset") {
            keys(o,{"op","id"});const auto id=text(o.at("id"));
            const auto found=session.document().preset_definitions.find(id);
            if(found==session.document().preset_definitions.end())throw Error("MISSING_PRESET",id);
            result=preset_json(found->second);
        } else if(op=="properties") {
            keys(o,{"op"});
            j::array list;
            const auto values=evaluate(session.document());
            const auto italic_values=evaluate_text_italics(session.document());
            const auto weight_values=evaluate_text_weights(session.document());
            const auto content_values=evaluate_text_contents(session.document());
            const auto family_values=evaluate_text_families(session.document());
            const auto locale_values=evaluate_text_locales(session.document());
            const auto direction_values=evaluate_text_directions(session.document());
            const auto layout_values=evaluate_text_layouts(session.document());
            const auto alignment_values=evaluate_text_alignments(session.document());
            const auto fill_rule_values=evaluate_fill_rules(session.document());
            const auto operation_enabled_values=evaluate_operation_enableds(session.document());
            const auto gradient_enabled_values=gradient_enabled_states(session.document());
            const auto mask_enabled_values=evaluate_geometry_mask_enableds(session.document());
            const auto point_edit_enabled_values=evaluate_point_edit_enableds(session.document());
            std::map<Id,std::pair<std::string,GuidePositionProperty>> guide_values;
            std::map<Id,std::string> artboard_names;
            std::map<Id,std::map<Id,EffectiveArtboardGuide>> artboard_guide_values;
            for(const auto& composition:session.document().compositions) {
                const auto positions=evaluate_guide_positions(session.document(),composition.id);
                for(const auto& guide:composition.guides)guide_values.emplace(guide.id,
                    std::make_pair(guide.name,GuidePositionProperty{
                        guide.position,guide.position_driver,guide.position_expression,
                        guide.position_driver?"link":guide.position_expression?"expression":"literal",
                        positions.at(guide.id)}));
                for(const auto& board:composition.artboards) {
                    artboard_names.emplace(board.id,board.name);
                    auto& cached=artboard_guide_values[board.id];
                    for(auto& occurrence:effective_artboard_guides(session.document(),composition.id,board.id))
                        cached.emplace(occurrence.guide_id,std::move(occurrence));
                }
            }
            for(const auto& ref:properties(session.document())) {
                if(ref.field.starts_with("margin.")||ref.field.starts_with("grid.")) {
                    list.push_back(artboard_layout_property_json(session.document(),ref,
                        artboard_layout_property(session.document(),ref)));
                    continue;
                }
                if(ref.field=="guide.position") {
                    const auto& [name,value]=guide_values.at(ref.object);
                    list.push_back(guide_position_property_json(name,ref,value));
                    continue;
                }
                if(ref.field.starts_with("artboard.guide.")) {
                    const auto& occurrence=artboard_guide_values.at(ref.object).at(ref.point);
                    const auto evaluated=ref.field=="artboard.guide.position"?
                        std::variant<double,bool>{occurrence.position}:std::variant<double,bool>{occurrence.enabled};
                    list.push_back(artboard_guide_property_json(ref,{occurrence,evaluated},
                        artboard_names.at(ref.object)+" / "+occurrence.name));
                    continue;
                }
                if(ref.field=="artboard.background"){list.push_back(background_property_json(session.document(),ref));continue;}
                if(ref.field.starts_with("artboard.")) {
                    list.push_back(artboard_size_property_json(session.document(),ref,artboard_size_property(session.document(),ref)));
                    continue;
                }
                if(ref.field=="object.visible") {
                    list.push_back(object_visibility_property_json(session.document(),ref,
                        object_visibility_state(session.document(),ref)));
                    continue;
                }
                if(ref.field=="composite.isolated") {
                    list.push_back(composite_isolated_property_json(session.document(),ref,
                        composite_isolation_state(session.document(),ref)));
                    continue;
                }
                if(ref.field=="mask.enabled") {
                    list.push_back(geometry_mask_enabled_property_json(session.document(),ref,
                        geometry_mask_enabled_property(session.document(),ref)));
                    continue;
                }
                if(ref.field.starts_with("mask.")&&ref.field.ends_with(".enabled")) {
                    const auto& mask=*session.document().objects.at(ref.object).compositing.mask;
                    list.push_back(geometry_mask_enabled_state_json(session.document(),ref,
                        {mask.enabled,mask.enabled_driver,mask.enabled_expression,
                            mask_enabled_values.at(ref)}));
                    continue;
                }
                if(ref.field=="point_edit.enabled") {
                    list.push_back(point_edit_enabled_property_json(session.document(),ref,
                        point_edit_enabled_property(session.document(),ref)));
                    continue;
                }
                if(ref.field.starts_with("point_edit.")&&ref.field.ends_with(".enabled")) {
                    const auto& point_edit=*session.document().objects.at(ref.object).point_edit;
                    list.push_back(point_edit_enabled_state_json(session.document(),ref,
                        {point_edit.enabled,point_edit.enabled_driver,point_edit.enabled_expression,
                            point_edit_enabled_values.at(ref)}));
                    continue;
                }
                if(ref.field.starts_with("op.")&&ref.field.find(".gradient.")!=std::string::npos&&ref.field.ends_with(".enabled")) {
                    list.push_back(gradient_enabled_property_json(session.document(),ref,gradient_enabled_values.at(ref)));
                    continue;
                }
                if(ref.field.starts_with("op.")&&ref.field.ends_with(".enabled")) {
                    const auto& object=session.document().objects.at(ref.object);
                    const auto operation=std::find_if(object.stack.begin(),object.stack.end(),[&](const auto& candidate) {
                        return operation_ref(ref.object,candidate.id,"enabled")==ref;
                    });
                    list.push_back(operation_enabled_property_json(session.document(),ref,
                        {operation->enabled,operation->enabled_driver,operation->enabled_expression,
                            operation_enabled_values.at(ref)}));
                    continue;
                }
                if(ref.field.starts_with("op.")&&ref.field.ends_with(".fill_rule")) {
                    const auto& object=session.document().objects.at(ref.object);
                    const auto operation=std::find_if(object.stack.begin(),object.stack.end(),[&](const auto& candidate) {
                        return operation_ref(ref.object,candidate.id,"fill_rule")==ref;
                    });
                    list.push_back(fill_rule_property_json(session.document(),ref,
                        {operation->fill_rule,operation->fill_rule_driver,fill_rule_values.at(ref)}));
                    continue;
                }
                if(ref.field.starts_with("macro.")&&!ref.point.empty()) {
                    const auto& object=session.document().objects.at(ref.object);
                    const auto entry=std::find_if(object.stack.begin(),object.stack.end(),[&](const auto& candidate) {
                        return candidate.id==ref.point&&candidate.macro.has_value();
                    });
                    if(entry==object.stack.end())throw Error("MISSING_MACRO_INSTANCE",ref.point);
                    const auto& definition=session.document().macro_definitions.at(entry->macro->definition);
                    const auto& revision=definition.revisions.at(entry->macro->pinned_revision);
                    const auto parameter=std::find_if(revision.public_parameters.begin(),revision.public_parameters.end(),[&](const auto& item) {
                        return item.id==ref.field;
                    });
                    if(parameter==revision.public_parameters.end())throw Error("MISSING_MACRO_PARAMETER",ref.field);
                    const auto effective=macro_parameter_value(session.document(),ref.object,entry->id,ref.field);
                    list.push_back({{"ref",ref_json(ref)},{"name",definition.label+" / "+parameter->label},
                        {"type",parameter->value_type},{"unit",parameter->unit},{"space","local"},
                        {"origin",entry->macro->overrides.contains(ref.field)?"macro_override":"macro_default"},
                        {"authored",effective},{"evaluated",effective}});
                    continue;
                }
                if(is_text_readonly_field(ref.field)) {
                    if(ref.field=="text.content") {
                        const auto& source=*session.document().objects.at(ref.object).text;
                        list.push_back(text_content_property_json(session.document(),ref,
                            {source.content,source.content_driver,content_values.at(ref)}));
                        continue;
                    }
                    if(ref.field=="text.family") {
                        const auto& source=*session.document().objects.at(ref.object).text;
                        list.push_back(text_family_property_json(session.document(),ref,
                            {source.family,source.family_driver,family_values.at(ref)}));
                        continue;
                    }
                    if(ref.field=="text.locale") {
                        const auto& source=*session.document().objects.at(ref.object).text;
                        list.push_back(text_locale_property_json(session.document(),ref,
                            {source.locale,source.locale_driver,locale_values.at(ref)}));
                        continue;
                    }
                    if(ref.field=="text.direction") {
                        const auto& source=*session.document().objects.at(ref.object).text;
                        list.push_back(text_direction_property_json(session.document(),ref,
                            {source.direction,source.direction_driver,direction_values.at(ref)}));
                        continue;
                    }
                    if(ref.field=="text.layout") {
                        const auto& source=*session.document().objects.at(ref.object).text;
                        list.push_back(text_layout_property_json(session.document(),ref,
                            {source.layout,source.layout_driver,layout_values.at(ref)}));
                        continue;
                    }
                    if(ref.field=="text.alignment") {
                        const auto& source=*session.document().objects.at(ref.object).text;
                        list.push_back(text_alignment_property_json(session.document(),ref,
                            {source.alignment,source.alignment_driver,alignment_values.at(ref)}));
                        continue;
                    }
                    list.push_back(text_readonly_property_json(session.document(),ref,text_readonly_property(session.document(),ref)));
                    continue;
                }
                if(ref.point.empty()&&ref.field=="text.italic") {
                    const auto& source=*session.document().objects.at(ref.object).text;
                    list.push_back(text_italic_property_json(session.document(),ref,{source.italic,source.italic_driver,italic_values.at(ref)}));
                    continue;
                }
                if(ref.point.empty()&&ref.field=="text.weight") {
                    const auto& source=*session.document().objects.at(ref.object).text;
                    list.push_back(text_weight_property_json(session.document(),ref,{source.weight,source.weight_driver,source.weight_expression,weight_values.at(ref)}));
                    continue;
                }
                const auto origin=property_origin(session.document(),ref);
                list.push_back({{"ref",ref_json(ref)},
                    {"name",property_name(session.document(),ref)},
                    {"type","number"},{"unit",property_unit(ref)},{"space","local"},
                    {"origin",origin},{"authored",origin=="generated"?j::value(nullptr):scalar_json(property(session.document(),ref))},
                    {"evaluated",values.at(ref)}});
            }
            const auto operation_enabled=evaluate_operation_enableds(session.document());
            const auto gradient_enabled=evaluate_gradient_enableds(session.document());
            for(const auto& ref:color_properties(session.document()))list.push_back(color_property_json(session.document(),ref,values,&operation_enabled,&gradient_enabled));
            result=std::move(list);
        } else if(op=="color_properties") {
            keys(o,{"op"});j::array list;const auto values=evaluate(session.document());
            const auto operation_enabled=evaluate_operation_enableds(session.document());
            const auto gradient_enabled=evaluate_gradient_enableds(session.document());
            for(const auto& ref:color_properties(session.document()))list.push_back(color_property_json(session.document(),ref,values,&operation_enabled,&gradient_enabled));
            result=std::move(list);
        } else if(op=="used_colors") {
            keys(o,{"op"});const auto& d=session.document();const auto values=evaluate(d);
            std::map<std::array<double,4>,std::vector<Ref>> grouped;
            const auto operation_enabled=evaluate_operation_enableds(d);
            const auto gradient_enabled=evaluate_gradient_enableds(d);
            for(const auto& ref:color_properties(d))if(color_is_used(d,ref,&operation_enabled,&gradient_enabled))
                grouped[color_value(d,ref,values).rgba].push_back(ref);
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
                {"discards_bypassed_corrections",object.point_edit&&!evaluate_point_edit_enabled(
                    session.document(),point_edit_enabled_ref(id,object.point_edit->id))}};
        } else if(op=="artboards") {
            keys(o,{"op","composition"});const auto id=text(o.at("composition"));
            const auto& comps=session.document().compositions;
            const auto comp=std::find_if(comps.begin(),comps.end(),[&](const auto& c){return c.id==id;});
            if(comp==comps.end())throw Error("MISSING_COMPOSITION",id);
            j::array frames;for(const auto& a:comp->artboards) {
                auto evaluated=evaluate_artboard(*comp,a.id);
                j::array guides;
                for(const auto& guide:effective_artboard_guides(session.document(),comp->id,a.id))guides.push_back(j::object{
                    {"target_artboard",guide.target_artboard},{"source_artboard",guide.source_artboard},
                    {"template_source_artboard",guide.template_source_artboard},{"guide_id",guide.guide_id},
                    {"name",guide.name},{"axis",guide.axis},{"position",guide.position},
                    {"world_position",(guide.axis=="x"?evaluated.x:evaluated.y)+guide.position},
                    {"enabled",guide.enabled},{"inherited",guide.inherited},
                    {"position_overridden",guide.position_overridden},{"enabled_overridden",guide.enabled_overridden}});
                frames.push_back(j::object{{"authored",artboard_json(a)},{"evaluated",artboard_json(evaluated)},
                    {"background_state",background_property_json(session.document(),{a.id,"","artboard.background"})},
                    {"effective_guides",std::move(guides)}});
            }
            result=std::move(frames);
        } else if(op=="text_defaults") {
            keys(o,{"op"});result=text_json(default_text("new-text-source"));
        } else if(op=="text_fonts") {
            keys(o,{"op"});result=ids_json(text_fonts());
        } else if(op=="text_layout") {
            keys(o,{"op","object"});result=text_layout_json(session.document(),text(o.at("object")));
        } else if(op=="export_plan"||op=="compatibility_plan") {
            if(op=="export_plan")keys(o,{"op","composition","artboard"});
            else keys(o,{"op","composition","artboard","target_profile","options"});
            const auto cid=text(o.at("composition"));
            const auto& d=session.document();const auto comp=std::find_if(d.compositions.begin(),d.compositions.end(),[&](const auto& c){return c.id==cid;});
            if(comp==d.compositions.end())throw Error("MISSING_COMPOSITION",cid);
            Id selected_board;
            if(o.contains("artboard"))selected_board=text(o.at("artboard"));
            else if(op=="compatibility_plan"&&comp->artboards.size()==1)selected_board=comp->artboards.front().id;
            else throw Error("ARTBOARD_REQUIRED","Select an Artboard explicitly for a multi-Artboard compatibility plan");
            const auto board=evaluate_artboard(*comp,selected_board);
            if(op=="compatibility_plan"&&text(o.at("target_profile"))!="svg/1.1+css-compositing"){
                if(o.contains("options")&&!o.at("options").is_object())throw Error("INVALID_COMPATIBILITY_OPTIONS","options must be an object");
                result=compatibility_plan(d,session.revision(),cid,selected_board,text(o.at("target_profile")),
                    o.contains("options")?o.at("options").as_object():j::object{},{});
            }else{
            j::array texts,unsupported_effects,unsupported_masks;
            const auto operation_enabled=evaluate_operation_enableds(d);
            const auto mask_enabled=evaluate_geometry_mask_enableds(d);
            std::function<void(const Id&)> walk=[&](const Id& id){const auto& object=d.objects.at(id);
                if(object.text)texts.push_back(text_layout_json(d,id));
                if(object.compositing.mask&&object.compositing.mask->mode!="geometry"&&
                    mask_enabled.at(geometry_mask_enabled_ref(id,object.compositing.mask->id)))
                    unsupported_masks.push_back(j::object{{"object",id},{"source",object.compositing.mask->source},
                        {"mode",object.compositing.mask->mode},{"mask_color_space",object.compositing.mask->mask_color_space},
                        {"derivative","svg"},{"projection","isolated_rgba"},
                        {"reason",object.compositing.mask->mode=="luma"?
                            "SVG cannot represent sRGB Luma mask coverage without a lossless projection":
                            "SVG cannot represent Alpha mask RGBA coverage without a lossless projection"}});
                for(const auto& operation:object.stack)if(object.kind==Kind::group&&
                    operation_enabled.at(operation_ref(id,operation.id,"enabled"))&&operation.type=="nect.group.posterize")
                    unsupported_effects.push_back(j::object{{"object",id},{"operation",operation.id},{"type",operation.type},
                        {"derivative","svg"},{"reason","SVG export cannot represent enabled Group Posterize without baking"}});
                for(const auto& child:object.children)walk(child);};
            for(const auto& id:comp->roots)walk(id);
            const auto blend_values=evaluate(d);
            const auto blend_scene=evaluate_scene(d,cid,blend_values,evaluate_transforms(d,blend_values));
            const auto unsupported_blends=unsupported_svg_blends(blend_scene);
            result=j::object{{"format","svg"},{"artboard",artboard_json(board)},{"text",texts},
                {"svg_export_supported",unsupported_effects.empty()&&unsupported_masks.empty()&&unsupported_blends.empty()},
                {"unsupported_effects",unsupported_effects},{"unsupported_masks",unsupported_masks},{"unsupported_blends",unsupported_blends},
                {"property_policy","evaluated_values"},{"expressions_preserved",false},
                {"shape_policy","evaluated vector contours; live operators preserved only in native"},
                {"path_deform_policy","evaluated derivative geometry; source IDs, contours and relation preserved only in native; gradient fields retain source affine coordinates"},
                {"image_policy","accepted snapshot projected to oriented sRGB PNG in SVG; original bytes and links preserved in native"},
                {"image_limits",j::object{{"normalized_png_bytes",raster_png_limit},{"aggregate_png_bytes",32*1024*1024}}},
                {"compositing_policy","vector_geometry_clips_group_opacity_css_blend_and_isolation"},{"blend_reader_requirement","SVG CSS mix-blend-mode and isolation support"},
                {"blend_capabilities",blend_descriptors_json()},{"cross_reader_pixel_identity",false},{"nect_svg_blend_intake_supported",false},
                {"text_policy","outlines"},{"native_source_preserved",true},{"fonts_embedded",false}};
            if(op=="compatibility_plan"){
                const auto profile=text(o.at("target_profile"));
                if(o.contains("options")&&!o.at("options").is_object())throw Error("INVALID_COMPATIBILITY_OPTIONS","options must be an object");
                const j::object options=o.contains("options")?o.at("options").as_object():j::object{};
                result=compatibility_plan(d,session.revision(),cid,selected_board,profile,options,result.as_object());
            }
            }
        } else if(op=="compositing_types") {
            keys(o,{"op"});j::array blends;for(const auto& descriptor:blend_modes())blends.push_back(j::value(descriptor.id));
            result=j::object{{"version",1},{"space","srgb"},{"alpha","source-over premultiplied compositing"},
                {"blends",blends},{"blend_descriptors",blend_descriptors_json()},
                {"production_profile",nonseparable_blend_profile_id},
                {"profile_selection","fixed-production-profile"},
                {"unavailable_profiles",j::array{
                    j::object{{"channel_bits",16},{"supported",false},{"reason","No 16-bpc production renderer"}},
                    j::object{{"channel_bits",32},{"supported",false},{"reason","No 32-bpc production renderer"}},
                    j::object{{"color_space","linear-srgb"},{"supported",false},{"reason","Only encoded-sRGB production blending is implemented"}}}},
                {"mask","final_path_geometry_or_alpha_or_srgb_luma_rgba"},{"mask_sources",j::array{"path","text"}},
                {"mask_modes",j::array{"geometry","alpha","luma"}},{"geometry_mask_sources",j::array{"path","text"}},
                {"alpha_mask_sources",j::array{"path","text","image","group"}},{"mask_space","composition"},
                {"mask_paint_ignored",true},{"geometry_mask_paint_ignored",true},
                {"alpha_mask_source_projection","isolated_rgba"},{"luma_mask_sources",j::array{"path","text","image","group"}},
                {"luma_mask_source_projection","isolated_rgba"},{"luma_mask_color_space","srgb"},
                {"luma_mask_coefficients",j::array{0.2125,0.7154,0.0721}},
                {"alpha_mask_root_visibility_ignored",true},{"alpha_mask_root_blend_ignored",true},
                {"alpha_mask_invert","alpha_only"},{"luma_mask_invert","after_luminance_alpha"},{"luma_mask_supported",true},
                {"mask_open_contours","implicitly_closed_geometry_only"},{"mask_normal_visibility","independent"},
                {"neutral_groups","pass_through"},{"nonneutral_groups","isolated_then_clip_opacity_blend"},{"after_effects_full_parity",false}};
        } else if(op=="compositing_plan") {
            keys(o,{"op","composition"});const auto values=evaluate(session.document());
            const auto scene=evaluate_scene(session.document(),text(o.at("composition")),values,evaluate_transforms(session.document(),values));
            const auto& render_document=scene.expanded_document?*scene.expanded_document:session.document();
            const auto& render_values=scene.expanded_values?*scene.expanded_values:values;
            const auto operation_enabled=evaluate_operation_enableds(render_document);
            std::function<j::value(const EvaluatedSceneNode&)> node_json=[&](const EvaluatedSceneNode& node) {
                j::array children,world,effects;for(const auto value:node.world)world.push_back(value);for(const auto& child:node.children)children.push_back(node_json(child));
                const auto& object=render_document.objects.at(node.id);
                for(const auto& operation:object.stack)if(operation.type=="nect.group.posterize") {
                    j::value driver=nullptr;if(operation.enabled_driver)driver=j::object{{"link",ref_json(*operation.enabled_driver)}};
                    j::object effect{
                        {"id",operation.id},{"type",operation.type},{"version",operation.version},
                        {"authored_enabled",operation.enabled},{"enabled_driver",std::move(driver)},
                        {"enabled",operation_enabled.at(operation_ref(node.id,operation.id,"enabled"))},
                        {"levels",render_values.at(operation_ref(node.id,operation.id,"levels"))}};
                    if(operation.enabled_expression)effect["enabled_expression"]=expression_json(*operation.enabled_expression);
                    effects.push_back(std::move(effect));
                }
                j::value mask=nullptr;
                if(node.mask) {
                    j::object details{{"source",node.mask->source},{"mode",node.mask->mode},
                        {"invert",node.mask->invert},{"mask_color_space",node.mask->mask_color_space},
                        {"space","composition"}};
                    if(node.mask->mode=="geometry") {
                        details["fill_rule"]=node.mask->fill_rule;
                        details["path_instances"]=node.mask->paths.size();
                    } else details["projection"]="isolated_rgba";
                    mask=std::move(details);
                }
                j::object result{{"object",node.id},{"world",world},{"visible",node.visible},{"opacity",node.opacity},{"blend",node.blend},
                    {"isolated",node.isolated},{"mask",mask},{"postchildren_effects",effects},{"children",children}};
                if(scene.deformation_owners.contains(node.id)) {
                    const auto& group=scene.deformation_owners.at(node.id);const auto& relation=*render_document.objects.at(group).path_follow;
                    j::array points;
                    const auto vector=[](Vec2 value){return j::array{value.x,value.y};};
                    for(const auto& point:scene.deformation_points.at(node.id))points.push_back(j::object{
                        {"point",point.id},{"contour",point.contour},{"anchor",vector(point.anchor)},
                        {"incoming",vector(point.incoming)},{"outgoing",vector(point.outgoing)}});
                    result["deformation"]=j::object{{"group",group},{"relation",relation.id},{"source_object",node.id},
                        {"path",relation.path},{"contour",relation.contour},{"mode",relation.mode},{"axis",relation.deform_axis},
                        {"space","group_local"},{"authored_source_preserved",true},{"points",points}};
                }
                return result;
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
            for(const auto* type:{"nect.shape.circle","nect.shape.ellipse","nect.shape.rectangle","nect.shape.polygon","nect.shape.star"}) {
                const auto source=default_primitive("new-source",type);
                definitions.push_back(j::object{{"type",type},{"version",1},{"template",primitive_json(source)},
                    {"point_edit","absolute_local_override"},{"topology_change","reject_unmapped_corrections_or_references"}});
            }
            result=std::move(definitions);
        } else if(op=="operator_types") {
            keys(o,{"op"});j::array definitions;
            for(const auto& descriptor:builtin_operation_types()) {
                auto defaults=default_operation("new-operation",descriptor.type);
                j::array parameters;for(const auto& [name,scalar]:defaults.parameters)
                    parameters.push_back({{"name",name},{"unit",property_unit(operation_ref("object",defaults.id,name))},{"default",scalar.literal}});
                j::object definition{{"type",descriptor.type},{"version",descriptor.version},{"input",descriptor.input},
                    {"output",descriptor.output},{"target_kind",descriptor.target_kind},{"bypass","preserve_input"},
                    {"parameters",parameters},{"template",operation_json(defaults)}};
                if(defaults.type=="nect.paint.stroke") {
                    definition["supported_versions"]=j::array{1,2};
                    definition["style_command"]="stroke_style";
                    definition["caps"]=j::array{"butt","round","square"};definition["joins"]=j::array{"miter","round","bevel"};
                    definition["miter_limit_range"]=j::array{1,1000};
                    definition["v1_style"]="butt/miter/4; explicitly promote to v2 using stroke_style";
                }
                if(defaults.type=="nect.shape.offset") {
                    definition["space"]="object-local after preceding path operations";
                    definition["scope"]="current paths and earlier paint geometry; paint coordinate bases retained";
                    definition["joins"]=j::array{"miter","round","bevel"};
                    definition["geometry"]="closed simple contours; nested/disjoint rings; nonzero/evenodd; separate instances are not unioned";
                    definition["unsupported"]=j::array{"open paths","self-intersections","touching/intersecting contour boundaries"};
                    definition["curve_flattening_tolerance_du"]=0.1;
                    definition["zero_amount"]="exact input; no geometry conversion";
                }
                if(defaults.type=="nect.group.posterize") {
                    definition["scope"]="group";
                    definition["domain"]="Group postchildren premultiplied sRGB RGBA pixels";
                    definition["placement"]="after children composite in Group order; before Group mask, opacity and blend";
                    definition["quantization"]="unpremultiply RGB; floor(channel*(levels-1)+0.5)/(levels-1); premultiply unchanged alpha";
                    definition["levels_range"]=j::array{2,16};definition["integer_parameter"]="levels";
                    definition["transparent_zero"]="transparent black";
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
            const auto values=evaluate(session.document());auto shape=evaluate_shape(session.document(),id,values);
            std::optional<EvaluatedScene> scene;
            if(std::any_of(session.document().objects.begin(),session.document().objects.end(),[](const auto& entry) {
                return entry.second.path_follow&&entry.second.path_follow->mode=="deform";
            })) {
                const auto transforms=evaluate_transforms(session.document(),values);
                std::function<bool(const Id&)> contains=[&](const Id& current) {
                    if(current==id)return true;
                    for(const auto& child:session.document().objects.at(current).children)if(contains(child))return true;
                    return false;
                };
                for(const auto& composition:session.document().compositions)
                    if(std::any_of(composition.roots.begin(),composition.roots.end(),contains)) {
                        scene=evaluate_scene(session.document(),composition.id,values,transforms);
                        shape=scene->shapes.at(id);break;
                    }
            }
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
            j::object plan{{"path_instances",shape.paths.size()},{"paint_layers",paints}};
            if(scene&&scene->deformation_owners.contains(id)) {
                j::array world;for(const auto value:scene->geometry_worlds.at(id))world.push_back(value);
                plan["geometry_space"]="group_local";plan["geometry_world"]=world;
                plan["deformation_group"]=scene->deformation_owners.at(id);plan["authored_source_preserved"]=true;
            }
            result=std::move(plan);
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
            const auto& wire_commands=o.at("commands").as_array();
            const auto expected=j::value_to<std::uint64_t>(o.at("expected_revision"));
            const bool has_preset=std::any_of(wire_commands.begin(),wire_commands.end(),is_preset_command);
            if(has_preset) {
                if(wire_commands.size()!=1)throw Error("PRESET_SINGLE_OPERATION",
                    "Preset commands are single Session operations and cannot be mixed into a generic command batch");
                const auto preset=read_preset_command(wire_commands.front());
                const auto* apply=std::get_if<ApplyPreset>(&preset.mutation);
                const auto* import=std::get_if<ImportAndApplyPreset>(&preset.mutation);
                j::array captured_source_operations,captured_source_entries;
                if(const auto* capture=std::get_if<CreatePresetFromStack>(&preset.mutation)) {
                    if(const auto object=session.document().objects.find(capture->object);object!=session.document().objects.end())
                        for(const auto& entry:object->second.stack) {
                            captured_source_operations.push_back(j::object{{"id",entry.id},{"type",entry.type}});
                            if(entry.macro)captured_source_entries.push_back(j::object{{"id",entry.id},{"kind","macro"},
                                {"definition",entry.macro->definition},{"revision",entry.macro->pinned_revision}});
                            else captured_source_entries.push_back(j::object{{"id",entry.id},{"kind","builtin"},{"type",entry.type}});
                        }
                }
                const auto before=session.revision();
                session.apply_preset_command(preset,expected);
                j::array applied,applied_library;
                if(apply&&session.revision()!=before) {
                    const auto& definition=session.document().preset_definitions.at(apply->preset);
                    j::array operation_ids;
                    for(const auto& id:preset_operation_ids(definition,apply->operation_id_prefix))operation_ids.push_back(j::value(id));
                    applied.push_back(j::object{{"preset",preset_json(definition)},
                        {"target",apply->object},{"operation_ids",operation_ids},{"processing_entry_ids",operation_ids}});
                }
                if(import&&session.revision()!=before) {
                    const auto& definition=session.document().preset_definitions.at(import->document_definition_id);
                    j::array operation_ids;
                    for(const auto& id:preset_operation_ids(definition,import->operation_id_prefix))operation_ids.push_back(j::value(id));
                    applied_library.push_back(j::object{{"asset_id",import->asset_id},
                        {"accepted_revision",import->accepted_revision},{"definition_id",definition.id},
                        {"label",definition.label},{"schema_version",definition.schema_version},
                        {"target",import->object},{"processing_entry_ids",operation_ids},
                        {"asset_identity_source","caller_supplied"}});
                }
                result=j::object{{"changed",session.revision()!=before},{"applied_presets",applied},
                    {"applied_library_presets",applied_library},
                    {"captured_source_operations",captured_source_operations},{"captured_source_entries",captured_source_entries}};
            } else {
                std::vector<Command> commands;
                std::vector<InstantiateMacro> imported_macros;
                std::vector<DetachArtboardGuide> detached_artboard_guides;
                for(const auto& v:wire_commands) {
                    auto command=read_command(v);
                    if(const auto* structural=std::get_if<StructuralCommand>(&command))
                        if(const auto* macro=std::get_if<MacroCommand>(structural))
                            if(macro->mutation)
                                if(const auto* instance=std::get_if<InstantiateMacro>(macro->mutation.get());
                                    instance&&instance->imported_definition)
                                    imported_macros.push_back(*instance);
                    if(const auto* structural=std::get_if<StructuralCommand>(&command))
                        if(const auto* guides=std::get_if<ArtboardGuideCommand>(structural))
                            if(const auto* detach=std::get_if<DetachArtboardGuide>(&guides->mutation))
                                detached_artboard_guides.push_back(*detach);
                    commands.push_back(std::move(command));
                }
                const auto before=session.revision();
                apply_serializable(session,commands,expected);
                j::array applied_library_macros;
                if(session.revision()!=before)for(const auto& imported:imported_macros) {
                    const auto& source=*imported.imported_definition;
                    const bool definition_present=session.document().macro_definitions.contains(imported.definition);
                    bool instance_present=false;
                    if(const auto object=session.document().objects.find(imported.object);
                        object!=session.document().objects.end())
                        instance_present=std::any_of(object->second.stack.begin(),object->second.stack.end(),[&](const auto& value) {
                            return value.id==imported.instance&&value.macro.has_value();
                        });
                    j::object overrides;for(const auto& [id,value]:imported.overrides)overrides[id]=value;
                    j::array retained_revisions;
                    for(const auto& [number,revision]:source.revisions){(void)revision;retained_revisions.push_back(number);}
                    applied_library_macros.push_back(j::object{{"asset_id",imported.asset_id},
                        {"accepted_revision",imported.accepted_asset_revision},
                        {"source_definition_id",source.id},{"definition_id",imported.definition},{"label",source.label},
                        {"latest_revision",source.latest_revision},{"retained_revisions",retained_revisions},
                        {"pinned_revision",imported.revision},{"target",imported.object},
                        {"instance",imported.instance},{"overrides",overrides},
                        {"definition_present_after_batch",definition_present},{"instance_present_after_batch",instance_present},
                        {"asset_identity_source","caller_supplied"}});
                }
                j::array detached_guide_results;
                if(session.revision()!=before)for(const auto& detach:detached_artboard_guides) {
                    const auto composition=std::find_if(session.document().compositions.begin(),
                        session.document().compositions.end(),[&](const Composition& item){return item.id==detach.composition;});
                    if(composition==session.document().compositions.end())continue;
                    const auto board=std::find_if(composition->artboards.begin(),composition->artboards.end(),
                        [&](const Artboard& item){return item.id==detach.artboard_id;});
                    if(board==composition->artboards.end())continue;
                    const auto guide=std::find_if(board->local_guides.begin(),board->local_guides.end(),
                        [&](const ArtboardGuide& item){return item.id==detach.new_guide_id;});
                    if(guide==board->local_guides.end())continue;
                    const bool source_suppressed=board->template_assignment&&
                        std::find(board->template_assignment->detached_guides.begin(),
                            board->template_assignment->detached_guides.end(),detach.guide_id)!=
                                board->template_assignment->detached_guides.end();
                    detached_guide_results.push_back(j::object{
                        {"target",j::object{{"composition",detach.composition},{"artboard",detach.artboard_id}}},
                        {"old_occurrence",ref_json(Ref{detach.artboard_id,detach.guide_id,"artboard.guide.position"})},
                        {"source_guide_id",detach.guide_id},
                        {"new_authored_guide",artboard_guide_json(*guide)},
                        {"source_suppressed",source_suppressed}});
                }
                result=j::object{{"changed",session.revision()!=before},{"applied_presets",j::array{}},
                    {"applied_library_macros",applied_library_macros},
                    {"detached_artboard_guides",detached_guide_results}};
            }
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
                j::array local,authored_local,derived_local,world;
                for(const auto v:transform.local)local.push_back(v);
                for(const auto v:transform.authored_local)authored_local.push_back(v);
                for(const auto v:transform.derived_local)derived_local.push_back(v);
                for(const auto v:transform.world)world.push_back(v);
                const auto x=values.at({id,"","transform.anchor_x"}),y=values.at({id,"","transform.anchor_y"});
                const auto position=map_point(transform.local,{x,y});const auto world_anchor=map_point(transform.world,{x,y});
                list.push_back(j::object{{"object",id},{"effective_parent",transform.effective_parent},{"local",local},{"world",world},
                    {"authored_local",authored_local},{"derived_local",derived_local},
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
            map_diff(prior.preset_definitions,after.preset_definitions);map_diff(prior.definitions,after.definitions);
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
            for(const auto& c:session.document().compositions) {
                for(const auto& guide:c.guides) {
                    const auto value=guide_json(guide);const auto found=prior_guides.find(guide.id);
                    if(found==prior_guides.end()||found->second!=value)changed.insert(guide.id);
                    prior_guides.erase(guide.id);
                }
                for(const auto& a:c.artboards)if(a.layout&&a.layout->grid) {
                    const auto value=grid_json(*a.layout->grid);const auto id=a.layout->grid->id;const auto found=prior_grids.find(id);
                    if(found==prior_grids.end()||found->second!=value)changed.insert(id);
                    prior_grids.erase(id);
                }
            }
            for(const auto& [id,guide]:prior_guides){(void)guide;changed.insert(id);}
            for(const auto& [id,grid]:prior_grids){(void)grid;changed.insert(id);}
            const auto prior_mask_enabled=evaluate_geometry_mask_enableds(prior);
            const auto current_mask_enabled=evaluate_geometry_mask_enableds(after);
            for(const auto& [ref,value]:current_mask_enabled)
                if(!prior_mask_enabled.contains(ref)||prior_mask_enabled.at(ref)!=value)changed.insert(ref.object);
            bool mask_consumers_changed=true;
            while(mask_consumers_changed) {
                mask_consumers_changed=false;
                for(const auto& [id,object]:session.document().objects)
                    if(object.compositing.mask&&current_mask_enabled.at(geometry_mask_enabled_ref(id,object.compositing.mask->id))&&
                        changed.contains(object.compositing.mask->source)&&changed.insert(id).second)mask_consumers_changed=true;
            }
            result.as_object()["changed_ids"]=ids_json(std::vector<Id>(changed.begin(),changed.end()));
            j::array created;for(const auto& [id,object]:after.objects){(void)object;if(!prior.objects.contains(id))created.push_back(j::value(id));}
            result.as_object()["created_ids"]=std::move(created);
        }
        return j::serialize(j::object{
            {"ok",true},{"document_id",session.document().id},
            {"revision",session.revision()},{"result",result}});
    } catch(const Error& e) {
        j::object error{{"code",e.code},{"message",e.what()}};
        if(!e.references.empty()) {j::array references;for(const auto& ref:e.references)references.push_back(ref_json(ref));error["references"]=std::move(references);}
        return j::serialize(j::object{{"ok",false},{"revision",session.revision()},{"error",std::move(error)}});
    } catch(const std::exception& e) {
        return j::serialize(j::object{
            {"ok",false},{"revision",session.revision()},
            {"error",j::object{{"code","INVALID_REQUEST"},{"message",e.what()}}}});
    }
}
}
