#include "nect/core.hpp"
#include "nect/expression.hpp"
#include <algorithm>
#include <cmath>
#include <charconv>
#include <functional>
#include <limits>
#include <set>
#include <numeric>
#include <numbers>
#include <string_view>
#include <type_traits>

namespace nect {
Error::Error(std::string c, const std::string& m) : std::runtime_error(m), code(std::move(c)) {}

namespace {
// Literal diagnostics need no owned string on the successful evaluation path.
// Construct the existing Error/message only when its condition actually fails.
void require(bool ok, const char* code, const char* message) {
    if(!ok) throw Error(code,message);
}
void require(bool ok, const char* code, const std::string& message) {
    if(!ok) throw Error(code,message);
}
void finite(double n) {
    require(std::isfinite(n),"NON_FINITE","Non-finite numeric value");
}
bool driven(const Scalar& scalar){return scalar.binding.has_value()||scalar.expression.has_value();}
using ExpressionCache=std::map<std::pair<unsigned,std::string>,CompiledExpression>;
const CompiledExpression& compiled_expression(ExpressionCache& cache,const Expression& expression) {
    const auto key=std::pair{expression.version,expression.source};
    if(const auto found=cache.find(key);found!=cache.end())return found->second;
    return cache.emplace(key,compile_expression(expression)).first->second;
}
void text_utf8(const std::string& value) {
    for(std::size_t i=0;i<value.size();) {
        const auto first=static_cast<unsigned char>(value[i++]);unsigned point=first,minimum=0;int extra=0;
        if(first>=0xf0&&first<=0xf4){point=first&7;extra=3;minimum=0x10000;}
        else if(first>=0xe0&&first<=0xef){point=first&15;extra=2;minimum=0x800;}
        else if(first>=0xc2&&first<=0xdf){point=first&31;extra=1;minimum=0x80;}
        else require(first<128,"INVALID_UTF8","Invalid text encoding");
        for(int k=0;k<extra;++k) {
            require(i<value.size(),"INVALID_UTF8","Truncated text encoding");const auto next=static_cast<unsigned char>(value[i++]);
            require((next&0xc0)==0x80,"INVALID_UTF8","Invalid text continuation");point=(point<<6)|(next&63);
        }
        require(point>=minimum&&point<=0x10ffff&&!(point>=0xd800&&point<=0xdfff),"INVALID_UTF8","Invalid Unicode scalar");
        require(point>=32||point==9||point==10||point==13,"INVALID_TEXT","Control character is not supported");
    }
}
void identity(const Id& id) {
    require(!id.empty() && id.size()<=96,"INVALID_ID","ID must have 1..96 ASCII identifier characters");
    for(const unsigned char c : id)
        require((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-',"INVALID_ID",id);
}
struct ParsedTextItalicExpression {
    bool is_literal=false;
    bool literal=false;
    bool negate=false;
    Ref source;
    std::size_t object_begin=0,object_end=0;
};
class TextItalicExpressionParser {
    std::string_view source_;
    std::size_t cursor_=0;
    void whitespace(){while(cursor_<source_.size()&&(source_[cursor_]==' '||source_[cursor_]=='\t'||source_[cursor_]=='\n'||source_[cursor_]=='\r'))++cursor_;}
    bool take(char c){whitespace();if(cursor_<source_.size()&&source_[cursor_]==c){++cursor_;return true;}return false;}
    void expect(char c){require(take(c),"BOOLEAN_EXPRESSION_SYNTAX","Invalid Text italic expression delimiter");}
    bool word(std::string_view expected) {
        whitespace();if(source_.substr(cursor_,expected.size())!=expected)return false;
        cursor_+=expected.size();return true;
    }
    std::string quoted(bool identifier,std::size_t* begin=nullptr,std::size_t* end=nullptr,bool allow_empty=false) {
        expect('"');const auto start=cursor_;
        while(cursor_<source_.size()&&source_[cursor_]!='"') {
            const auto c=source_[cursor_++];
            const bool valid=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||(!identifier&&c=='.');
            require(valid,"BOOLEAN_EXPRESSION_SYNTAX","Stable Ref arguments use unescaped ASCII identifiers");
        }
        require(cursor_<source_.size(),"BOOLEAN_EXPRESSION_SYNTAX","Unterminated Text italic Ref argument");
        const auto finish=cursor_;++cursor_;
        if(begin)*begin=start;if(end)*end=finish;
        const auto length=finish-start;
        require((identifier&&(length>0||allow_empty)&&length<=96)||(!identifier&&length<=512),"BOOLEAN_EXPRESSION_SYNTAX","Text italic Ref argument is out of range");
        return std::string(source_.substr(start,length));
    }
public:
    explicit TextItalicExpressionParser(std::string_view source):source_(source){}
    ParsedTextItalicExpression parse() {
        ParsedTextItalicExpression result;whitespace();
        if(word("true")){result.is_literal=true;result.literal=true;}
        else if(word("false")){result.is_literal=true;result.literal=false;}
        else {
            result.negate=take('!');
            require(word("ref"),"BOOLEAN_EXPRESSION_SYNTAX","Expected true, false or a Text italic ref");
            expect('(');
            result.source.object=quoted(true,&result.object_begin,&result.object_end);
            expect(',');result.source.point=quoted(true,nullptr,nullptr,true);
            expect(',');result.source.field=quoted(false);expect(')');
            require(result.source.point.empty()&&result.source.field=="text.italic","BOOLEAN_EXPRESSION_TYPE","Text italic expressions may reference only Text italic");
        }
        whitespace();require(cursor_==source_.size(),"BOOLEAN_EXPRESSION_SYNTAX","Unexpected trailing Text italic expression text");
        return result;
    }
};
ParsedTextItalicExpression parse_text_italic_expression(const Expression& expression) {
    require(expression.version==1,"UNSUPPORTED_EXPRESSION_VERSION","Only expression version 1 is supported for Text italic");
    require(!expression.source.empty()&&expression.source.size()<=4096,"EXPRESSION_LIMIT","Text italic expression source must contain 1..4096 bytes");
    return TextItalicExpressionParser(expression.source).parse();
}
const TextSource& text_italic_source(const Document& document,const Ref& ref) {
    require(ref.point.empty()&&ref.field=="text.italic","TYPE_MISMATCH","Only Text italic accepts a boolean property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text italic Ref must identify a Text object");
    return *object->second.text;
}
class TextItalicEvaluator {
    const Document& document_;
    std::map<Id,bool> values_;
    std::set<Id> active_;
    bool visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Text italic dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end())return found->second;
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Text italic dependency cycle");
        const auto& source=text_italic_source(document_,{id,"","text.italic"});
        bool value=source.italic;
        if(source.italic_driver) {
            if(const auto link=std::get_if<Ref>(&*source.italic_driver)) {
                (void)text_italic_source(document_,*link);value=visit(link->object,depth+1);
            }
            else {
                const auto parsed=parse_text_italic_expression(std::get<Expression>(*source.italic_driver));
                if(parsed.is_literal)value=parsed.literal;
                else {(void)text_italic_source(document_,parsed.source);value=visit(parsed.source.object,depth+1);if(parsed.negate)value=!value;}
            }
        }
        active_.erase(id);values_.emplace(id,value);return value;
    }
public:
    explicit TextItalicEvaluator(const Document& document):document_(document){}
    bool value(const Id& id){return visit(id,0);}
    std::map<Ref,bool> all() {
        std::map<Ref,bool> result;
        for(const auto& [id,object]:document_.objects)if(object.kind==Kind::text&&object.text)
            result.emplace(Ref{id,"","text.italic"},visit(id,0));
        return result;
    }
};
Expression remap_text_italic_expression(const Expression& expression,const std::function<Ref(const Ref&)>& remap) {
    const auto parsed=parse_text_italic_expression(expression);
    if(parsed.is_literal)return expression;
    const auto target=remap(parsed.source);if(target==parsed.source)return expression;
    require(target.point.empty()&&target.field=="text.italic","TYPE_MISMATCH","Duplicated Text italic Ref changed type");
    auto result=expression;result.source.replace(parsed.object_begin,parsed.object_end-parsed.object_begin,target.object);
    (void)parse_text_italic_expression(result);return result;
}
const std::array<std::string,6> point_fields{"x","y","in.angle","in.length","out.angle","out.length"};
bool polystar(const Primitive& source){return source.type=="nect.shape.polygon"||source.type=="nect.shape.star";}
unsigned primitive_point_count(const Primitive& source,double value) {
    require(std::isfinite(value)&&std::floor(value)==value&&value>=(source.type=="nect.shape.star"?2:3)&&value<=256,
        "OUT_OF_RANGE","Polygon points must be integral 3..256; Star points integral 2..256");
    return static_cast<unsigned>(value);
}
std::string phase_role(const char* lineage,unsigned numerator,unsigned denominator) {
    const auto divisor=std::gcd(numerator,denominator);
    return std::string(lineage)+"-"+std::to_string(numerator/divisor)+"-"+std::to_string(denominator/divisor);
}
std::vector<std::string> source_roles(const Primitive& source,std::optional<double> count={}) {
    require(source.version==1,"UNSUPPORTED_OPERATOR_VERSION","Unsupported primitive behavior version");
    if(source.type=="nect.shape.circle")return {"east","south","west","north"};
    if(source.type=="nect.shape.rectangle")return {"top-left","top-right","bottom-right","bottom-left"};
    if(polystar(source)) {
        require(count.has_value(),"EVALUATION_REQUIRED","Dynamic primitive topology needs an evaluated point count");
        const auto points=primitive_point_count(source,*count);std::vector<std::string> roles;
        for(unsigned i=0;i<points;++i) {
            roles.push_back(phase_role("outer",i,points));
            if(source.type=="nect.shape.star")roles.push_back(phase_role("inner",2*i+1,2*points));
        }
        return roles;
    }
    throw Error("UNSUPPORTED_OPERATOR",source.type);
}
bool generated_point(const Object& o,const Id& point) {
    if(!o.source||point.empty())return false;
    if(polystar(*o.source)) {
        // Structural address recognition only; active membership is resolved
        // through generator.points by the recursive evaluator below.
        const auto prefix=o.source->id+"-";if(!point.starts_with(prefix))return false;
        auto role=point.substr(prefix.size());
        if(role.starts_with("outer-"))role.erase(0,6);
        else if(o.source->type=="nect.shape.star"&&role.starts_with("inner-"))role.erase(0,6);
        else return false;
        const auto dash=role.find('-');if(dash==std::string::npos)return false;
        unsigned numerator=0,denominator=0;
        const auto a=std::from_chars(role.data(),role.data()+dash,numerator);
        const auto b=std::from_chars(role.data()+dash+1,role.data()+role.size(),denominator);
        return a.ec==std::errc{}&&a.ptr==role.data()+dash&&b.ec==std::errc{}&&b.ptr==role.data()+role.size()&&
            denominator>0&&denominator<=512&&numerator<denominator&&std::gcd(numerator,denominator)==1&&
            role==std::to_string(numerator)+"-"+std::to_string(denominator);
    }
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
template<class O> auto& operation(O& object,const Id& id) {
    auto it=std::find_if(object.stack.begin(),object.stack.end(),[&](const auto& op){return op.id==id;});
    require(it!=object.stack.end(),"MISSING_OPERATION",id);return *it;
}
std::pair<Id,std::string> operation_address(const std::string& field) {
    const auto end=field.find('.',3);
    require(end!=std::string::npos,"MISSING_REFERENCE",field);
    return {field.substr(3,end-3),field.substr(end+1)};
}
template<class O> auto& gradient_property(O& op,const std::string& parameter) {
    require(op.gradient.has_value(),"MISSING_REFERENCE",parameter);
    const auto prefix="gradient."+op.gradient->id+".";
    require(parameter.starts_with(prefix),"MISSING_REFERENCE",parameter);
    auto& g=*op.gradient;const auto field=parameter.substr(prefix.size());
    if(field=="start_x")return g.start_x;if(field=="start_y")return g.start_y;
    if(field=="end_x")return g.end_x;if(field=="end_y")return g.end_y;
    for(auto& stop:g.stops) {
        const auto root="stop."+stop.id+".";
        if(!field.starts_with(root))continue;
        const auto component=field.substr(root.size());
        if(component=="offset")return stop.offset;
        static const std::array<std::string,4> channels{"r","g","b","a"};
        for(std::size_t i=0;i<channels.size();++i)if(component==channels[i])return stop.rgba[i];
    }
    throw Error("MISSING_REFERENCE",parameter);
}
template<class D>
auto& lookup_property(D& d,const Ref& r) {
    if(auto color=d.named_colors.find(r.object);color!=d.named_colors.end()&&r.point.empty()) {
        const std::array<std::string,4> fields{"color.r","color.g","color.b","color.a"};
        const auto found=std::find(fields.begin(),fields.end(),r.field);
        require(found!=fields.end(),"MISSING_REFERENCE",r.field);
        return color->second.rgba[static_cast<std::size_t>(std::distance(fields.begin(),found))];
    }
    auto it=d.objects.find(r.object);
    require(it!=d.objects.end(),"MISSING_REFERENCE",r.object);
    auto& o=it->second;
    if(r.point.empty()) {
        if(r.field=="composite.opacity")return o.compositing.opacity;
        if(o.image&&r.field=="image.width")return o.image->width;
        if(o.image&&r.field=="image.height")return o.image->height;
        if(o.text&&r.field.starts_with("text.")) {
            const auto name=r.field.substr(5);require(o.text->parameters.contains(name),"MISSING_REFERENCE",r.field);
            return o.text->parameters.at(name);
        }
        if(o.source&&r.field.starts_with("generator.")) {
            const auto name=r.field.substr(10);
            require(o.source->parameters.contains(name),"MISSING_REFERENCE",r.field);
            return o.source->parameters.at(name);
        }
        static const std::array<std::string,6> tf{"a","b","c","d","tx","ty"};
        for(std::size_t i=0;i<tf.size();++i) if(r.field=="transform."+tf[i]) return o.transform[i];
        if(r.field=="transform.anchor_x")return o.anchor[0];
        if(r.field=="transform.anchor_y")return o.anchor[1];
        if(o.kind!=Kind::group) {
            if(r.field.starts_with("op.")||r.field.starts_with("stroke.")) {
                const auto address=r.field.starts_with("op.")?operation_address(r.field):
                    std::pair{o.legacy_stroke,r.field.substr(7)};
                require(!address.first.empty(),"MISSING_REFERENCE",r.field);
                auto& op=operation(o,address.first);
                if(address.second.starts_with("gradient."))return gradient_property(op,address.second);
                require(op.parameters.contains(address.second),"MISSING_REFERENCE",r.field);
                return op.parameters.at(address.second);
            }
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
    if(r.point.empty()&&r.field=="text.italic")return "boolean";
    if(r.field=="generator.points")return "scalar";
    if(r.field=="generator.rotation")return "degree";
    if(r.field.starts_with("color."))return "scalar";
    if(r.field.starts_with("text.")||r.field.starts_with("image."))return "du";
    if(r.field.starts_with("op.")) {
        const auto name=operation_address(r.field).second;
        if(name.starts_with("gradient.")&&(name.ends_with(".start_x")||name.ends_with(".start_y")||name.ends_with(".end_x")||name.ends_with(".end_y")))return "du";
        if(name=="width"||name=="amount"||name=="position_x"||name=="position_y"||name=="anchor_x"||name=="anchor_y")return "du";
        if(name=="rotation")return "degree";
        return "scalar";
    }
    if(r.field.starts_with("generator.")||r.field=="x"||r.field=="y"||r.field=="transform.tx"||r.field=="transform.ty"||
       r.field=="transform.anchor_x"||r.field=="transform.anchor_y"||
       r.field=="stroke.width"||r.field.ends_with(".length")) return "du";
    if(r.field.ends_with(".angle")) return "degree";
    return "scalar";
}
void value_range(const Ref& r,double v) {
    finite(v);
    require(std::abs(v)<=1e9,"OUT_OF_RANGE","Magnitude limit is 1e9 in v0.1");
    if(r.field=="composite.opacity")require(v>=0&&v<=1,"OUT_OF_RANGE","Compositing opacity must be in [0,1]");
    if(r.field.starts_with("color."))require(v>=0&&v<=1,"OUT_OF_RANGE","sRGB color channels must be in [0,1]");
    if(r.field=="image.width"||r.field=="image.height")require(v>0&&v<=1e7,"OUT_OF_RANGE","Image display dimensions must be in (0,10000000]");
    if(r.field=="text.font_size")require(v>0&&v<=10000,"OUT_OF_RANGE","Text size must be in (0,10000]");
    if(r.field=="text.frame_width"||r.field=="text.frame_height")require(v>0&&v<=1e6,"OUT_OF_RANGE","Text frame dimensions must be in (0,1000000]");
    if(r.field=="text.line_spacing")require(v>=0&&v<=10000,"OUT_OF_RANGE","Line spacing must be in [0,10000], with zero for automatic");
    if(r.field=="text.tracking")require(std::abs(v)<=10000,"OUT_OF_RANGE","Tracking magnitude limit 10000");
    if(r.field.ends_with(".length")||r.field=="stroke.width"||r.field=="generator.radius"||
       r.field=="generator.width"||r.field=="generator.height"||r.field=="generator.outer_radius"||r.field=="generator.inner_radius")
        require(v>=0,"OUT_OF_RANGE","Negative length");
    if(r.field=="generator.points")require(v>=2&&v<=256&&std::floor(v)==v,"OUT_OF_RANGE","Points must be an integer from 2 to 256");
    if(r.field=="stroke.miter_limit")require(v>=1&&v<=1000,"OUT_OF_RANGE","Miter limit must be in [1,1000]");
    if(r.field.starts_with("stroke.")&&r.field!="stroke.width"&&r.field!="stroke.miter_limit")
        require(v>=0&&v<=1,"OUT_OF_RANGE","sRGB/alpha channel outside [0,1]");
    if(r.field.starts_with("op.")) {
        const auto name=operation_address(r.field).second;
        if(name.starts_with("gradient.")&&name.find(".stop.")!=std::string::npos)
            require(v>=0&&v<=1,"OUT_OF_RANGE","Gradient offsets/color channels must lie in [0,1]");
        if(name=="r"||name=="g"||name=="b"||name=="a"||name=="start_opacity"||name=="end_opacity")
            require(v>=0&&v<=1,"OUT_OF_RANGE","sRGB/opacity outside [0,1]");
        if(name=="width")require(v>=0,"OUT_OF_RANGE","Negative stroke width");
        if(name=="amount")require(std::abs(v)<=1e6,"OUT_OF_RANGE","Offset amount magnitude limit 1000000");
        if(name=="miter_limit")require(v>=1&&v<=1000,"OUT_OF_RANGE","Miter limit must be in [1,1000]");
        if(name=="copies")require(v>=0&&v<=1000&&std::floor(v)==v,"OUT_OF_RANGE","Copies must be an integer from 0 to 1000");
        if(name=="scale_x"||name=="scale_y")require(v>0&&v<=100,"OUT_OF_RANGE","Repeater scale must be positive and <=100");
        if(name=="offset")require(std::abs(v)<=1000,"OUT_OF_RANGE","Repeater offset magnitude limit 1000");
    }
}

std::map<Ref, const Scalar*> property_index(const Document& document,bool include_disabled=false) {
    std::map<Ref, const Scalar*> index;
    static const std::array<std::string, 6> transform_fields{
        "transform.a", "transform.b", "transform.c", "transform.d", "transform.tx", "transform.ty"};

    for(const auto& [id,color]:document.named_colors) {
        const std::array<std::string,4> fields{"color.r","color.g","color.b","color.a"};
        for(std::size_t i=0;i<4;++i)index.emplace(Ref{id,"",fields[i]},&color.rgba[i]);
    }
    for (const auto& [id, object] : document.objects) {
        for (std::size_t i = 0; i < transform_fields.size(); ++i)
            index.emplace(Ref{id, "", transform_fields[i]}, &object.transform[i]);
        index.emplace(Ref{id,"","transform.anchor_x"},&object.anchor[0]);
        index.emplace(Ref{id,"","transform.anchor_y"},&object.anchor[1]);
        index.emplace(Ref{id,"","composite.opacity"},&object.compositing.opacity);

        if (object.kind == Kind::group) continue;
        if(object.image) {
            index.emplace(Ref{id,"","image.width"},&object.image->width);
            index.emplace(Ref{id,"","image.height"},&object.image->height);
        }
        if(object.text)for(const auto& [name,value]:object.text->parameters)index.emplace(Ref{id,"","text."+name},&value);

        for(const auto& op:object.stack) {
            for(const auto& [name,value]:op.parameters) {
                index.emplace(operation_ref(id,op.id,name),&value);
                if(op.id==object.legacy_stroke)index.emplace(Ref{id,"","stroke."+name},&value);
            }
            if(op.gradient) {
                const auto& g=*op.gradient;
                index.emplace(gradient_ref(id,op.id,g.id,"start_x"),&g.start_x);
                index.emplace(gradient_ref(id,op.id,g.id,"start_y"),&g.start_y);
                index.emplace(gradient_ref(id,op.id,g.id,"end_x"),&g.end_x);
                index.emplace(gradient_ref(id,op.id,g.id,"end_y"),&g.end_y);
                const std::array<std::string,4> channels{"r","g","b","a"};
                for(const auto& stop:g.stops) {
                    index.emplace(gradient_ref(id,op.id,g.id,"stop."+stop.id+".offset"),&stop.offset);
                    for(std::size_t i=0;i<channels.size();++i)index.emplace(gradient_ref(id,op.id,g.id,"stop."+stop.id+"."+channels[i]),&stop.rgba[i]);
                }
            }
        }

        if(object.source) {
            for(const auto& [name,value]:object.source->parameters)index.emplace(Ref{id,"","generator."+name},&value);
            if(object.point_edit&&(object.point_edit->enabled||include_disabled))
                for(const auto& [point,fields]:object.point_edit->overrides)
                    for(const auto& [field,value]:fields)index.emplace(Ref{id,point,field},&value);
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

Primitive default_primitive(Id id,const std::string& type) {
    Primitive source;source.id=std::move(id);source.type=type;
    source.parameters={{"center_x",{0,{}}},{"center_y",{0,{}}}};
    if(type=="nect.shape.circle")source.parameters.emplace("radius",Scalar{100,{}});
    else if(type=="nect.shape.rectangle") {source.parameters.emplace("width",Scalar{220,{}});source.parameters.emplace("height",Scalar{140,{}});}
    else if(type=="nect.shape.polygon"||type=="nect.shape.star") {
        source.parameters.emplace("points",Scalar{type=="nect.shape.star"?5.0:6.0,{}});
        source.parameters.emplace("rotation",Scalar{-90,{}});
        if(type=="nect.shape.polygon")source.parameters.emplace("radius",Scalar{100,{}});
        else {source.parameters.emplace("outer_radius",Scalar{100,{}});source.parameters.emplace("inner_radius",Scalar{50,{}});}
    } else throw Error("UNSUPPORTED_OPERATOR",type);
    return source;
}

void add_default_paint(Document& d,const Id& object,const std::string& type) {
    auto& o=d.objects.at(object);
    require((o.kind==Kind::path||o.kind==Kind::text)&&o.stack.empty(),"INVALID_OBJECT","Default paint needs a new Shape source");
    std::set<Id> ids{d.id};
    for(const auto& [id,color]:d.named_colors){(void)color;ids.insert(id);}
    for(const auto& [id,asset]:d.raster_assets){(void)asset;ids.insert(id);}
    for(const auto& c:d.compositions){
        ids.insert(c.id);for(const auto& a:c.artboards){ids.insert(a.id);if(a.layout&&a.layout->grid)ids.insert(a.layout->grid->id);}
        for(const auto& guide:c.guides)ids.insert(guide.id);
    }
    for(const auto& c:d.collections)ids.insert(c.id);
    for(const auto& [id,item]:d.objects) {
        ids.insert(id);for(const auto& op:item.stack) {
            ids.insert(op.id);if(op.gradient){ids.insert(op.gradient->id);for(const auto& stop:op.gradient->stops)ids.insert(stop.id);}
        }
        if(item.compositing.mask)ids.insert(item.compositing.mask->id);
        if(item.source){ids.insert(item.source->id);ids.insert(item.source->id+"-point-edit");ids.insert(item.source->id+"-contour");}
        if(item.text)ids.insert(item.text->id);
        for(const auto& contour:item.contours){ids.insert(contour.id);for(const auto& p:contour.points)ids.insert(p.id);}
    }
    const auto stem=object.substr(0,74)+(type=="nect.paint.fill"?"-fill":"-stroke");
    auto id=stem;unsigned suffix=0;
    // Reserve generated role addresses structurally. Do not evaluate an interim
    // transaction just to choose a paint ID: later commands may repair its links.
    auto occupied=[&](const Id& candidate) {
        if(ids.contains(candidate))return true;
        return std::any_of(d.objects.begin(),d.objects.end(),[&](const auto& entry){return generated_point(entry.second,candidate);});
    };
    while(occupied(id))id=stem+"-"+std::to_string(++suffix);
    if(type=="nect.paint.stroke")o.legacy_stroke=id;
    o.stack.push_back(default_operation(id,type));
}
void add_default_stroke(Document& d,const Id& object) {add_default_paint(d,object,"nect.paint.stroke");}

Scalar property(const Document& d,const Ref& r) {
    const auto object=d.objects.find(r.object);
    if(object!=d.objects.end()&&generated_point(object->second,r.point)) {
        if(property_origin(d,r)!="generated")return lookup_property(d,r);
        const auto values=evaluate(d);const auto found=values.find(r);
        require(found!=values.end(),"MISSING_REFERENCE",r.point+"/"+r.field);return {found->second,{}};
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

std::vector<Contour> path_contours(const Object& o,const std::map<Ref,double>* values) {
    if(!o.source)return o.contours;
    std::optional<double> count;
    if(polystar(*o.source)) {
        require(o.source->parameters.contains("points"),"INVALID_GENERATOR_PARAMETERS","Missing points parameter");
        const auto& points=o.source->parameters.at("points");
        if(values) {
            const auto found=values->find({o.id,"","generator.points"});
            require(found!=values->end(),"EVALUATION_REQUIRED","Evaluated point count is missing from the snapshot");count=found->second;
        } else {require(!driven(points),"EVALUATION_REQUIRED","Driven point count requires an evaluated snapshot");count=points.literal;}
    }
    Contour c{o.source->id+"-contour",true,{}};
    for(const auto& role:source_roles(*o.source,count)) {Point p;p.id=o.source->id+"-"+role;c.points.push_back(p);}
    return {c};
}

std::vector<Ref> conversion_blockers(const Document& d,const Id& object) {
    require(d.objects.contains(object),"MISSING_OBJECT",object);
    const auto& o=d.objects.at(object);
    require(o.source.has_value(),"NOT_PRIMITIVE","Select a parametric primitive");
    std::vector<Ref> blockers;
    ExpressionCache expressions;
    for(const auto& [ref,scalar]:property_index(d,true)) {
        if(!scalar||!driven(*scalar))continue;
        if(ref.point.empty()&&ref.field.starts_with("stroke."))continue;
        if(ref.object==object&&ref.field.starts_with("generator."))continue;
        if(ref.object==object&&!ref.point.empty()&&o.point_edit&&!o.point_edit->enabled)continue;
        const auto removed=[&](const Ref& source){return source.object==object&&source.point.empty()&&source.field.starts_with("generator.");};
        bool blocked=scalar->binding&&removed(scalar->binding->source);
        if(scalar->expression)for(const auto& source:expression_dependencies(compiled_expression(expressions,*scalar->expression)))blocked=blocked||removed(source);
        if(blocked)blockers.push_back(ref);
    }
    return blockers;
}

std::vector<Ref> properties(const Document& document) {
    std::vector<Ref> refs;
    for (const auto& [ref, value] : evaluate(document)) {
        (void)value;
        if(ref.point.empty()&&ref.field.starts_with("stroke."))continue;
        refs.push_back(ref);
    }
    for(const auto& [id,object]:document.objects)if(object.kind==Kind::text&&object.text)
        refs.push_back({id,"","text.italic"});
    return refs;
}

TextItalicProperty text_italic_property(const Document& document,const Ref& ref) {
    const auto& source=text_italic_source(document,ref);
    return {source.italic,source.italic_driver,evaluate_text_italic(document,ref.object)};
}
bool evaluate_text_italic(const Document& document,const Id& object) {
    return TextItalicEvaluator(document).value(object);
}
std::map<Ref,bool> evaluate_text_italics(const Document& document) {
    return TextItalicEvaluator(document).all();
}
std::string property_unit(const Ref& r) { return unit(r); }

Ref resolve_name(const Document& d,const std::string& name,const Id& p,const std::string& f) {
    std::vector<Id> matches;
    for(const auto& [id,o]:d.objects) if(o.name==name) matches.push_back(id);
    for(const auto& [id,color]:d.named_colors)if(color.name==name)matches.push_back(id);
    require(!matches.empty(),"MISSING_NAME","No matching object: "+name);
    require(matches.size()==1,"AMBIGUOUS_NAME","Name must resolve to exactly one object: "+name);
    Ref r{matches.front(),p,f};
    if(p.empty()&&f=="text.italic") {
        (void)text_italic_property(d,r);
        return r;
    }
    if(d.named_colors.contains(r.object)&&r.point.empty()&&r.field=="color") {
        (void)color_channels(d,r);
        return r;
    }
    (void)property(d,r);
    return r;
}

namespace {
std::map<Ref,double> evaluate_properties(const Document& d,const std::vector<Ref>* requested=nullptr) {
    // Snapshot-dependent commands may need only a few property domains. Reuse
    // the same dependency traversal without indexing or visiting unrelated data;
    // complete authored/evaluated validation still runs before any commit.
    const auto index = requested?std::map<Ref,const Scalar*>{}:property_index(d);
    std::map<Ref,double> values;
    std::set<Ref> active;
    ExpressionCache expressions;
    struct Topology {std::vector<std::string> roles;std::map<Id,std::size_t> positions;};
    std::map<Id,Topology> topologies;
    std::size_t points=0;
    for(const auto& [id,object]:d.objects){(void)id;for(const auto& contour:object.contours)points+=contour.points.size();}
    require(points<=50000,"LIMIT","Point limit 50000");
    std::function<const Topology&(const Object&,unsigned)> topology;
    std::function<double(const Ref&,unsigned)> visit=[&](const Ref& r,unsigned depth)->double {
        require(depth<=128,"DEPENDENCY_DEPTH","v0.1 dependency depth limit 128");
        if(auto i=values.find(r);i!=values.end()) return i->second;
        require(active.insert(r).second,"DEPENDENCY_CYCLE","Property dependency cycle");
        const auto found = index.find(r);const Scalar* p=found==index.end()?nullptr:found->second;
        const auto object=d.objects.find(r.object);
        const bool generated=object!=d.objects.end()&&object->second.source&&!r.point.empty();
        if(requested) {
            if(!generated)p=&lookup_property(d,r);
            else if(const auto& edit=object->second.point_edit;edit&&edit->enabled) {
                const auto point=edit->overrides.find(r.point);
                if(point!=edit->overrides.end()) {
                    const auto field=point->second.find(r.field);
                    if(field!=point->second.end())p=&field->second;
                }
            }
        }
        const Topology* roles=nullptr;std::size_t position=0;
        if(generated) {
            require(std::find(point_fields.begin(),point_fields.end(),r.field)!=point_fields.end(),"MISSING_REFERENCE",r.field);
            roles=&topology(object->second,depth+1);const auto role=roles->positions.find(r.point);
            if(role==roles->positions.end())throw Error("MISSING_REFERENCE",r.object+"/"+r.point+"/"+r.field);
            position=role->second;
        } else if(!p)throw Error("MISSING_REFERENCE",r.object+"/"+r.point+"/"+r.field);
        double v=p?p->literal:0;
        if(!p) {
            const auto& o=object->second;
            const auto& s=*o.source;
            const auto& role=roles->roles[position];
            const auto param=[&](const char* name){return visit({r.object,"",std::string("generator.")+name},depth+1);};
            if(s.type=="nect.shape.circle") {
                if(r.field=="x") {v=param("center_x");if(position==0)v+=param("radius");else if(position==2)v-=param("radius");}
                else if(r.field=="y") {v=param("center_y");if(position==1)v+=param("radius");else if(position==3)v-=param("radius");}
                else if(r.field.ends_with(".length"))v=param("radius")*0.5522847498307936;
                else if(r.field=="in.angle")v=static_cast<double>(position)*90-90;
                else if(r.field=="out.angle")v=static_cast<double>(position)*90+90;
            } else if(s.type=="nect.shape.rectangle") {
                if(r.field=="x")v=param("center_x")+param("width")*(role=="top-left"||role=="bottom-left"?-0.5:0.5);
                else if(r.field=="y")v=param("center_y")+param("height")*(role=="top-left"||role=="top-right"?-0.5:0.5);
            } else if(r.field=="x"||r.field=="y") {
                const auto angle=(param("rotation")+static_cast<double>(position)*360/static_cast<double>(roles->roles.size()))*std::numbers::pi/180;
                const auto radius=param(s.type=="nect.shape.polygon"?"radius":role.starts_with("inner-")?"inner_radius":"outer_radius");
                v=r.field=="x"?param("center_x")+radius*std::cos(angle):param("center_y")+radius*std::sin(angle);
            }
        } else if(p->expression) {
            require(!p->binding,"SCALAR_SOURCE_CONFLICT","A Scalar cannot have both Binding and Expression");
            v=evaluate_expression(compiled_expression(expressions,*p->expression),unit(r),[&](const Ref& source){return visit(source,depth+1);});
        } else if(p->binding) {
            const auto& b=*p->binding;
            require(b.mode=="copy_local_value","UNSUPPORTED_BINDING","Explicit copy_local_value binding required");
            finite(b.scale);
            finite(b.offset);
            const auto source=visit(b.source,depth+1);
            require(unit(r)==unit(b.source),"UNIT_MISMATCH","Implicit unit conversion is not supported");
            v=source*b.scale+b.offset;
        }
        value_range(r,v);
        active.erase(r);
        values.emplace(r,v);
        return v;
    };
    topology=[&](const Object& object,unsigned depth)->const Topology& {
        if(const auto found=topologies.find(object.id);found!=topologies.end())return found->second;
        const auto& source=*object.source;std::optional<double> count;
        if(polystar(source))count=visit({object.id,"","generator.points"},depth+1);
        Topology result;result.roles=source_roles(source,count);points+=result.roles.size();
        require(points<=50000,"LIMIT","Point limit 50000");
        for(std::size_t i=0;i<result.roles.size();++i)result.positions.emplace(source.id+"-"+result.roles[i],i);
        // Even bypassed corrections remain authored and may not silently lose roles.
        if(object.point_edit)for(const auto& [point,fields]:object.point_edit->overrides) {
            (void)fields;require(result.positions.contains(point),"UNRESOLVED_POINT_EDIT",point);
        }
        return topologies.emplace(object.id,std::move(result)).first->second;
    };
    if(requested) {
        for(const auto& ref:*requested)visit(ref,0);
        return values;
    }
    for (const auto& [ref, scalar] : index) {
        // Plain authored literals are dependency leaves. Avoid a second index
        // lookup and active-set allocation for each one during full evaluation.
        // Generated point overrides still need topology/role validation in visit.
        const auto object=d.objects.find(ref.object);
        const bool generated=object!=d.objects.end()&&object->second.source&&!ref.point.empty();
        if(scalar&&!driven(*scalar)&&!generated) {
            value_range(ref,scalar->literal);
            values.emplace_hint(values.end(),ref,scalar->literal);
        } else visit(ref, 0);
    }
    for(const auto& [id,object]:d.objects)if(object.source) {
        const auto& generated=topology(object,0);
        for(const auto& [point,position]:generated.positions) {
            (void)position;for(const auto& field:point_fields)visit({id,point,field},0);
        }
    }
    return values;
}
}
std::map<Ref,double> evaluate(const Document& d){return evaluate_properties(d);}

Artboard evaluate_artboard(const Composition& composition,const Id& artboard) {
    std::vector<const Artboard*> chain;std::set<Id> seen;auto next=artboard;
    for(;;) {
        require(seen.insert(next).second,"ARTBOARD_CYCLE","Parent artboard size dependency cycle");
        require(chain.size()<256,"LIMIT","Artboard parent depth limit 256");
        const auto found=std::find_if(composition.artboards.begin(),composition.artboards.end(),[&](const auto& a){return a.id==next;});
        require(found!=composition.artboards.end(),"MISSING_ARTBOARD",next);
        chain.push_back(&*found);
        if(!found->parent_size)break;
        next=found->parent_size->artboard;
    }
    Artboard result=*chain.back();
    for(auto it=chain.rbegin()+1;it!=chain.rend();++it) {
        auto child=**it;
        if(child.parent_size->width)child.width=result.width;
        if(child.parent_size->height)child.height=result.height;
        result=std::move(child);
    }
    return result;
}

static std::map<Ref,double> validate_evaluated(const Document& d) {
    require(d.objects.size()<=10000 && d.compositions.size()<=128,"LIMIT","Document size limit");
    std::set<Id> ids;
    auto add=[&](const Id& id) {
        identity(id);
        require(ids.insert(id).second,"DUPLICATE_ID",id);
    };

    add(d.id);
    require(d.raster_assets.size()<=128,"LIMIT","Raster asset count limit 128");
    std::size_t raster_bytes=0;std::uint64_t raster_pixels=0;
    for(const auto& [id,asset]:d.raster_assets) {
        add(id);require(id==asset.id,"ID_MISMATCH",id);
        require(!asset.name.empty()&&asset.name.size()<=4096,"INVALID_NAME","Asset name must be 1..4096 UTF-8 bytes");text_utf8(asset.name);
        require(asset.mode=="linked"||asset.mode=="embedded","INVALID_ASSET_MODE",asset.mode);
        if(asset.mode=="embedded")require(asset.locator.empty(),"INVALID_ASSET_LOCATOR","Embedded asset has no locator");
        else {
            const auto& path=asset.locator;
            const bool drive=path.size()>=3&&((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z'))&&path[1]==':'&&(path[2]=='/'||path[2]=='\\');
            const bool posix=path.size()>1&&path[0]=='/'&&path[1]!='/';
            require(path.size()<=32768&&(drive||posix)&&path.find('\0')==std::string::npos,"INVALID_ASSET_LOCATOR","Linked assets require an absolute local path (no URL or UNC path)");text_utf8(path);
        }
        require(bool(asset.payload),"INVALID_ASSET","Asset requires validated accepted bytes");
        raster_bytes+=asset.payload->bytes().size();raster_pixels+=std::uint64_t(asset.payload->width())*asset.payload->height();
    }
    require(raster_bytes<=document_raster_bytes_limit,"ASSET_LIMIT","Document accepted raster source limit 24 MiB");
    require(raster_pixels<=document_raster_pixels_limit,"ASSET_LIMIT","Document raster pixel limit 33554432");
    require(d.named_colors.size()<=1024,"LIMIT","Named color limit 1024");
    for(const auto& [id,color]:d.named_colors) {
        add(id);require(id==color.id,"ID_MISMATCH",id);
        require(!color.name.empty()&&color.name.size()<=4096,"INVALID_NAME","Named color requires a name of 1..4096 bytes");text_utf8(color.name);
    }
    std::size_t guide_count=0;
    for(const auto& comp:d.compositions) {
        add(comp.id);
        require(comp.guides.size()<=10000-guide_count,"LIMIT","Document Guide count limit 10000");
        guide_count+=comp.guides.size();
        for(const auto& guide:comp.guides) {
            add(guide.id);
            require(guide.name.size()<=4096,"INVALID_GUIDE","Guide name exceeds 4096 bytes");
            text_utf8(guide.name);
            require(guide.axis=="x"||guide.axis=="y","INVALID_GUIDE","Guide axis must be x or y");
            require(std::isfinite(guide.position)&&std::abs(guide.position)<=1e9,"INVALID_GUIDE","Guide position must be finite and within 1e9 du");
        }
        require(comp.artboards.size()<=1024,"LIMIT","Artboards per Composition limit 1024");
        for(const auto& a:comp.artboards) {
            add(a.id);
            finite(a.x); finite(a.y); finite(a.width); finite(a.height);
            require(a.width>0&&a.height>0&&a.width<=1e7&&a.height<=1e7,"INVALID_ARTBOARD",a.id);
            require(std::abs(a.x)<=1e9&&std::abs(a.y)<=1e9,"INVALID_ARTBOARD","Frame position exceeds 1e9");
            require(a.name.size()<=4096,"LIMIT","Artboard name too long");
            for(unsigned char ch:a.name)require(ch>=32||ch==9||ch==10||ch==13,"INVALID_NAME","XML-incompatible control character");
            if(a.parent_size)identity(a.parent_size->artboard);
            const auto evaluated=evaluate_artboard(comp,a.id);
            if(a.layout) {
                const auto& layout=*a.layout;
                require(layout.margin||layout.grid,"INVALID_LAYOUT","Artboard layout must contain Margin, Grid or both: "+a.id);
                if(layout.margin) {
                    const auto& margin=*layout.margin;
                    require(std::isfinite(margin.left)&&std::isfinite(margin.top)&&std::isfinite(margin.right)&&std::isfinite(margin.bottom),
                        "INVALID_LAYOUT","Margin values must be finite");
                    require(margin.left>=0&&margin.top>=0&&margin.right>=0&&margin.bottom>=0&&
                        margin.left+margin.right<evaluated.width&&margin.top+margin.bottom<evaluated.height,
                        "INVALID_LAYOUT","Margins must be nonnegative and leave positive content width and height");
                }
                if(layout.grid) {
                    const auto& grid=*layout.grid;add(grid.id);
                    const auto& bounds=grid.bounds;
                    require(std::isfinite(bounds.x)&&std::isfinite(bounds.y)&&std::isfinite(bounds.width)&&std::isfinite(bounds.height)&&
                        std::isfinite(grid.column_gutter)&&std::isfinite(grid.row_gutter),
                        "INVALID_LAYOUT","Grid values must be finite");
                    require(bounds.x>=0&&bounds.y>=0&&bounds.width>0&&bounds.height>0&&
                        bounds.x+bounds.width<=evaluated.width&&bounds.y+bounds.height<=evaluated.height,
                        "INVALID_LAYOUT","Grid bounds must be positive and contained in the evaluated Artboard");
                    require(grid.columns>=1&&grid.columns<=1000&&grid.rows>=1&&grid.rows<=1000&&
                        grid.column_gutter>=0&&grid.row_gutter>=0,
                        "INVALID_LAYOUT","Grid counts must be 1..1000 and gutters nonnegative");
                    const auto cell_width=(bounds.width-static_cast<double>(grid.columns-1)*grid.column_gutter)/static_cast<double>(grid.columns);
                    const auto cell_height=(bounds.height-static_cast<double>(grid.rows-1)*grid.row_gutter)/static_cast<double>(grid.rows);
                    require(cell_width>0&&cell_height>0,"INVALID_LAYOUT","Grid gutters must leave positive cell width and height");
                }
            }
        }
    }

    std::size_t point_count=0;
    for(const auto& [id,o]:d.objects) {
        add(id);
        require(id==o.id,"ID_MISMATCH",id);
        const auto& composite=o.compositing;
        require(composite.version==1,"UNSUPPORTED_COMPOSITING_VERSION","Only compositing version 1 is supported");
        static const std::array<std::string,12> blends{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn","hard-light","soft-light","difference","exclusion"};
        require(std::find(blends.begin(),blends.end(),composite.blend)!=blends.end(),"UNSUPPORTED_BLEND",composite.blend);
        if(composite.mask) {
            const auto& mask=*composite.mask;add(mask.id);identity(mask.source);
            require(mask.version==1,"UNSUPPORTED_MASK_VERSION","Only geometry mask version 1 is supported");
            require(mask.fill_rule=="nonzero"||mask.fill_rule=="evenodd","UNSUPPORTED_FILL_RULE",mask.fill_rule);
            require(mask.source!=id,"INVALID_MASK_SOURCE","A geometry mask cannot reference its owner");
            require(d.objects.contains(mask.source),"MISSING_MASK_SOURCE",mask.source);
            require((d.objects.at(mask.source).kind==Kind::path||d.objects.at(mask.source).kind==Kind::text),"INVALID_MASK_SOURCE","Geometry mask source must be a Path or Text");
        }
        if(o.transform_parent)identity(*o.transform_parent);
        require(o.name.size()<=4096,"LIMIT","Object name too long");
        for(unsigned char ch:o.name)
            require(ch>=32||ch==9||ch==10||ch==13,"INVALID_NAME","XML-incompatible control character");

        require(o.kind==Kind::group||o.kind==Kind::path||o.kind==Kind::text||o.kind==Kind::image,"INVALID_OBJECT","Unknown object kind");
        require(o.kind==Kind::image||!o.image,"INVALID_OBJECT","Only Image owns an image source");
        if(o.kind==Kind::image) {
            require(o.image.has_value()&&!o.text&&!o.source&&!o.point_edit&&o.contours.empty()&&o.children.empty(),"INVALID_IMAGE","Image requires exactly one image source");
            require(o.stack.empty()&&o.legacy_stroke.empty(),"INVALID_DOMAIN","Image shape stacks are unsupported");
            require(d.raster_assets.contains(o.image->asset),"MISSING_ASSET",o.image->asset);
        } else if(o.kind==Kind::group) {
            require(o.contours.empty(),"INVALID_OBJECT","Group cannot own path geometry");
            require(!o.source&&!o.point_edit&&!o.text,"INVALID_OBJECT","Group cannot own a geometry source");
            require(o.stack.empty()&&o.legacy_stroke.empty(),"INVALID_DOMAIN","Group shape stacks are not supported yet");
        } else {
            require(o.children.empty(),"INVALID_OBJECT","Path cannot own children");
            if(o.kind==Kind::text) {
                require(o.text.has_value()&&!o.source&&!o.point_edit&&o.contours.empty(),"INVALID_TEXT","Text owns one editable text source only");
                const auto& text=*o.text;add(text.id);require(text.version==1,"UNSUPPORTED_TEXT_VERSION","Only Text version 1 is supported");
                require(text.content.size()<=32768&&!text.family.empty()&&text.family.size()<=1024&&!text.locale.empty()&&text.locale.size()<=128,"LIMIT","Text content/family/locale limit");
                text_utf8(text.content);text_utf8(text.family);text_utf8(text.locale);
                require(text.layout=="auto"||text.layout=="frame","UNSUPPORTED_TEXT_LAYOUT",text.layout);
                require(text.direction=="horizontal"||text.direction=="vertical","UNSUPPORTED_TEXT_DIRECTION",text.direction);
                require(text.alignment=="start"||text.alignment=="center"||text.alignment=="end","UNSUPPORTED_TEXT_ALIGNMENT",text.alignment);
                require(text.weight>=1&&text.weight<=999,"OUT_OF_RANGE","Font weight must be 1..999");
                const auto expected=default_text("").parameters;
                require(text.parameters.size()==expected.size(),"INVALID_TEXT_PARAMETERS","Missing Text parameters");
                for(const auto& [name,value]:text.parameters){(void)value;require(expected.contains(name),"INVALID_TEXT_PARAMETERS",name);}
            } else require(!o.text,"INVALID_OBJECT","Path cannot own text source");
            require(o.stack.size()<=128,"LIMIT","Shape stack limit 128");
            for(const auto& op:o.stack) {
                add(op.id);const bool styled_stroke=op.type=="nect.paint.stroke"&&op.version==2;
                require(op.version==1||styled_stroke,"UNSUPPORTED_OPERATOR_VERSION",op.type);
                auto expected=default_operation(op.id,op.type);
                if(styled_stroke)expected.parameters.emplace("miter_limit",Scalar{4,{}});
                require(op.parameters.size()==expected.parameters.size(),"INVALID_OPERATOR_PARAMETERS",op.type);
                for(const auto& [name,value]:op.parameters){(void)value;require(expected.parameters.contains(name),"INVALID_OPERATOR_PARAMETERS",name);}
                require(op.composite=="above"||op.composite=="below","UNSUPPORTED_COMPOSITE",op.composite);
                require(op.fill_rule=="nonzero"||op.fill_rule=="evenodd","UNSUPPORTED_FILL_RULE",op.fill_rule);
                if(op.type!="nect.paint.fill"&&op.type!="nect.shape.offset")require(op.fill_rule=="nonzero","INVALID_OPERATOR_OPTIONS","Fill rule only applies to Fill or Offset");
                if(op.type=="nect.shape.offset") {
                    require(op.composite=="below","INVALID_OPERATOR_OPTIONS","Offset has no Above/Below compositing option");
                    require(op.line_join=="miter"||op.line_join=="round"||op.line_join=="bevel","INVALID_OPERATOR_OPTIONS","Offset joins are miter, round or bevel");
                } else if(styled_stroke)require(op.line_join=="miter"||op.line_join=="round"||op.line_join=="bevel","INVALID_OPERATOR_OPTIONS","Stroke joins are miter, round or bevel");
                else require(op.line_join=="miter","INVALID_OPERATOR_OPTIONS","Custom line join requires Offset or Stroke v2");
                if(styled_stroke)require(op.line_cap=="butt"||op.line_cap=="round"||op.line_cap=="square","INVALID_OPERATOR_OPTIONS","Stroke caps are butt, round or square");
                else require(op.line_cap=="butt","INVALID_OPERATOR_OPTIONS","Custom line cap requires Stroke v2");
                if(op.gradient) {
                    require(op.type=="nect.paint.fill"||op.type=="nect.paint.stroke","INVALID_DOMAIN","Gradient requires a paint operation");
                    const auto& g=*op.gradient;add(g.id);
                    require(g.version==1,"UNSUPPORTED_GRADIENT_VERSION","Only gradient version 1 is supported");
                    require(g.type=="linear"||g.type=="radial","UNSUPPORTED_GRADIENT",g.type);
                    require(g.stops.size()>=2&&g.stops.size()<=64,"INVALID_GRADIENT","Gradient requires 2..64 stable color stops");
                    for(const auto& stop:g.stops)add(stop.id);
                }
            }
            if(!o.legacy_stroke.empty())require(operation(o,o.legacy_stroke).type=="nect.paint.stroke","INVALID_LEGACY_ADDRESS","Legacy stroke must refer to a retained Stroke operation");
            if(o.source) {
                const auto& s=*o.source;
                require(o.contours.empty(),"INVALID_OBJECT","Primitive owns a generator, not a second authored contour list");
                require(s.id.size()<=64,"INVALID_ID","Generator ID is limited to 64 characters for stable role identities");
                add(s.id);
                add(s.id+"-point-edit"); // reserved correction instance namespace
                require(s.version==1,"UNSUPPORTED_OPERATOR_VERSION","Unsupported primitive behavior version");
                const auto expected=default_primitive(s.id,s.type).parameters;
                require(s.parameters.size()==expected.size(),"INVALID_GENERATOR_PARAMETERS","Missing or unsupported primitive parameters");
                for(const auto& [name,value]:s.parameters){(void)value;require(expected.contains(name),"INVALID_GENERATOR_PARAMETERS",name);}
                if(polystar(s))(void)primitive_point_count(s,s.parameters.at("points").literal);
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

    std::set<Id> owned;std::map<Id,Id> compositions;
    std::function<void(const Id&,const Id&,unsigned)> own=[&](const Id& id,const Id& composition,unsigned depth) {
        require(depth<=128,"HIERARCHY_DEPTH","Hierarchy depth limit 128");
        require(d.objects.contains(id),"MISSING_OBJECT",id);
        if(!owned.insert(id).second)throw Error("INVALID_HIERARCHY","Repeated owner or cycle: "+id);
        compositions.emplace(id,composition);
        for(const auto& child:d.objects.at(id).children) own(child,composition,depth+1);
    };
    for(const auto& comp:d.compositions) for(const auto& id:comp.roots) own(id,comp.id,0);
    require(owned.size()==d.objects.size(),"ORPHAN_OBJECT","Every object requires exactly one composition/tree owner");
    for(const auto& [id,object]:d.objects)if(object.compositing.mask)
        require(compositions.at(id)==compositions.at(object.compositing.mask->source),"CROSS_COMPOSITION","Geometry mask source must belong to the same Composition");

    for(const auto& c:d.collections) {
        add(c.id);
        std::set<Id> members;
        for(const auto& m:c.members) {
            require(d.objects.contains(m),"MISSING_OBJECT",m);
            require(members.insert(m).second,"DUPLICATE_MEMBER",m);
        }
    }

    const auto authored=property_index(d,true);
    ExpressionCache expressions;
    for (const auto& [ref, scalar] : authored) {
        if(!scalar)continue;
        value_range(ref, scalar->literal);
        require(!scalar->binding||!scalar->expression,"SCALAR_SOURCE_CONFLICT","A Scalar cannot have both Binding and Expression");
        if(scalar->expression)validate_expression_unit(compiled_expression(expressions,*scalar->expression),unit(ref));
        if(scalar->binding) {
            const auto& binding=*scalar->binding;
            require(binding.mode=="copy_local_value","UNSUPPORTED_BINDING","Explicit copy_local_value binding required");
            finite(binding.scale);finite(binding.offset);
        }
    }

    auto values=evaluate(d);
    // Boolean Text Italic links and expressions are a separate typed lane from
    // Scalar evaluation. Validate every authored driver before geometry uses it.
    (void)evaluate_text_italics(d);
    (void)evaluate_transforms(d,values);
    for(const auto& [ref,scalar]:authored)if(scalar&&scalar->binding) {
        const auto& source=scalar->binding->source;
        if(!values.contains(source))throw Error("MISSING_REFERENCE",source.object+"/"+source.point+"/"+source.field);
        require(unit(ref)==unit(source),"UNIT_MISMATCH","Implicit unit conversion is not supported");
    }
    for(const auto& [ref,scalar]:authored)if(scalar&&scalar->expression) {
        (void)ref;
        for(const auto& source:expression_dependencies(compiled_expression(expressions,*scalar->expression)))
            if(!values.contains(source))throw Error("MISSING_REFERENCE",source.object+"/"+source.point+"/"+source.field);
    }
    for(const auto& [id,object]:d.objects)if(object.source) {
        (void)id;
        for(const auto& contour:path_contours(object,&values)) {
            add(contour.id);for(const auto& point:contour.points){add(point.id);++point_count;}
        }
    }
    require(point_count<=50000,"LIMIT","Point limit 50000");
    for(const auto& [id,o]:d.objects) {
        for(const auto& op:o.stack)if(op.gradient) {
            const auto& g=*op.gradient;
            auto v=[&](const char* field){return values.at(gradient_ref(id,op.id,g.id,field));};
            if(op.enabled&&g.enabled)require(std::hypot(v("end_x")-v("start_x"),v("end_y")-v("start_y"))>1e-9,
                    "GRADIENT_GEOMETRY","Gradient start and end must differ");
            std::set<double> offsets;
            for(const auto& stop:g.stops)require(offsets.insert(values.at(gradient_ref(id,op.id,g.id,"stop."+stop.id+".offset"))).second,
                "GRADIENT_STOPS","Coincident gradient stop offsets are not yet supported");
        }
        bool check_shape=o.stack.size()>1||std::any_of(o.stack.begin(),o.stack.end(),[](const auto& op){return op.enabled&&(op.type=="nect.shape.repeater"||op.type=="nect.shape.offset");});
#ifdef _WIN32
        check_shape=check_shape||o.kind==Kind::text;
#else
        if(o.kind==Kind::text)check_shape=false; // Authored text remains readable without the Windows layout backend.
#endif
        if(check_shape)
            (void)evaluate_shape(d,id,values);
    }
    return values;
}

void validate(const Document& d) { (void)validate_evaluated(d); }

Session::Session(Document d,HistoryLimits limits):document_(std::move(d)),history_limits_(limits) {
    require(limits.max_entries>0&&limits.max_bytes>0,"INVALID_HISTORY_LIMITS","History limits must both be positive");
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
const std::array<std::string,6> affine_fields{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty"};
Affine local_affine(const Id& id,const std::map<Ref,double>& values) {
    Affine result;for(std::size_t i=0;i<result.size();++i)result[i]=values.at({id,"",affine_fields[i]});return result;
}
bool transform_equal(double a,double b) {
    if(!std::isfinite(a)||!std::isfinite(b))return false;
    return a==b||std::abs(a-b)<=32*std::numeric_limits<double>::epsilon()*std::max({1.0,std::abs(a),std::abs(b)});
}
void set_changed_scalar(Document& document,const Ref& ref,double value,const std::map<Ref,double>& values) {
    value_range(ref,value);
    auto& scalar=lookup_property(document,ref);const auto before=values.at(ref);
    if(before==value)return;
    // Matrix inversion/composition can introduce roundoff in an unchanged axis.
    // Keep its exact authored binding; a genuinely changed driven axis rejects.
    if(driven(scalar)&&transform_equal(before,value))return;
    require(!driven(scalar),"DRIVEN_PROPERTY","Unlink explicitly before changing a driven transform property");
    scalar.literal=value;
}
void set_affine(Document& document,const Id& id,const Affine& matrix,const std::map<Ref,double>& values) {
    for(std::size_t i=0;i<matrix.size();++i)set_changed_scalar(document,{id,"",affine_fields[i]},matrix[i],values);
}
void center_anchor(Document& document,const Id& id,bool require_geometry) {
    require(document.objects.contains(id),"MISSING_OBJECT",id);
    const auto values=evaluate(document);const auto transforms=evaluate_transforms(document,values);
    const auto bounds=object_bounds(document,id,values,transforms);
    if(!bounds){require(!require_geometry,"EMPTY_BOUNDS","Object has no geometry to center its Anchor");return;}
    set_changed_scalar(document,{id,"","transform.anchor_x"},bounds->left+(bounds->right-bounds->left)/2,values);
    set_changed_scalar(document,{id,"","transform.anchor_y"},bounds->top+(bounds->bottom-bounds->top)/2,values);
}
void scalar_targets(const Document& document,const std::vector<Ref>& targets,const std::map<Ref,double>& values) {
    require(!targets.empty()&&targets.size()<=1000,"INVALID_BATCH","Property targets must contain 1..1000 unique Scalars");
    std::set<Ref> unique;
    const auto expected_unit=unit(targets.front());
    for(const auto& target:targets) {
        require(values.contains(target),"MISSING_REFERENCE",target.object+"/"+target.point+"/"+target.field);
        require(unit(target)==expected_unit,"UNIT_MISMATCH","Property targets must share one scalar unit");
        auto canonical=target;
        if(target.point.empty()&&target.field.starts_with("stroke.")) {
            const auto& object=document.objects.at(target.object);
            canonical=operation_ref(target.object,object.legacy_stroke,target.field.substr(7));
        }
        require(unique.insert(std::move(canonical)).second,"DUPLICATE_TARGET","Each scalar target may occur only once, including aliases");
    }
}
Ref canonical_target(const Document& document,const Ref& target) {
    if(target.point.empty()&&target.field.starts_with("stroke."))
        return operation_ref(target.object,document.objects.at(target.object).legacy_stroke,target.field.substr(7));
    return target;
}
void translate_objects(Document& document,const std::vector<Id>& objects,const std::map<Id,Vec2>& displacements) {
    require(!objects.empty()&&objects.size()<=1000,"INVALID_BATCH","Translation requires 1..1000 unique objects");
    for(const auto& [id,delta]:displacements){(void)id;finite(delta.x);finite(delta.y);}
    std::set<Id> selected;
    for(const auto& id:objects) {
        require(document.objects.contains(id),"MISSING_OBJECT",id);
        require(selected.insert(id).second,"DUPLICATE_TARGET","Each translated object may occur only once");
    }
    std::vector<Ref> transform_refs;transform_refs.reserve(document.objects.size()*affine_fields.size());
    for(const auto& [id,object]:document.objects) {
        (void)object;for(const auto& field:affine_fields)transform_refs.push_back({id,"",field});
    }
    const auto values=evaluate_properties(document,&transform_refs);const auto transforms=evaluate_transforms(document,values);
    std::optional<Id> composition;
    for(const auto& plane:document.compositions) {
        std::function<void(const Id&)> own=[&](const Id& id) {
            if(selected.contains(id)) {
                require(!composition||*composition==plane.id,"CROSS_COMPOSITION","Translated objects must share one Composition");
                composition=plane.id;
            }
            for(const auto& child:document.objects.at(id).children)own(child);
        };
        for(const auto& root:plane.roots)own(root);
    }
    if(std::all_of(displacements.begin(),displacements.end(),[](const auto& item){return item.second.x==0&&item.second.y==0;}))return;
    std::map<Id,Affine> desired,prospective;
    for(const auto& id:selected) {
        auto world=transforms.at(id).world;world[4]+=displacements.at(id).x;world[5]+=displacements.at(id).y;
        for(const auto number:world)require(std::isfinite(number),"OUTPUT_RANGE","Translated world matrix must be finite");
        desired.emplace(id,world);
    }
    // Selected parents use their desired world matrix even before their local
    // fields are rewritten. Unselected intervening parents inherit that motion.
    // This is independent of target order and prevents ancestor/follower doubles.
    std::function<const Affine&(const Id&)> world=[&](const Id& id)->const Affine& {
        if(const auto found=prospective.find(id);found!=prospective.end())return found->second;
        if(const auto found=desired.find(id);found!=desired.end())return prospective.emplace(id,found->second).first->second;
        const auto& transform=transforms.at(id);
        const auto result=transform.effective_parent.empty()?transform.local:compose(world(transform.effective_parent),transform.local);
        return prospective.emplace(id,result).first->second;
    };
    for(const auto& id:selected) {
        const auto& transform=transforms.at(id);
        const auto basis=transform.effective_parent.empty()?identity_matrix:world(transform.effective_parent);
        Vec2 inherited{};auto ancestor=transform.effective_parent;
        while(!ancestor.empty()) {
            if(selected.contains(ancestor)){inherited=displacements.at(ancestor);break;}
            ancestor=transforms.at(ancestor).effective_parent;
        }
        // Subtract authored requested displacements rather than large world
        // positions, preserving representable sub-epsilon free translations.
        const auto dx=displacements.at(id).x-inherited.x,dy=displacements.at(id).y-inherited.y;
        if(dx==0&&dy==0)continue;
        const auto inverse=inverse_affine(basis);
        const auto tx=transform.local[4]+inverse[0]*dx+inverse[2]*dy;
        const auto ty=transform.local[5]+inverse[1]*dx+inverse[3]*dy;
        set_changed_scalar(document,{id,"","transform.tx"},tx,values);
        set_changed_scalar(document,{id,"","transform.ty"},ty,values);
    }
    const auto after=evaluate_transforms(document,evaluate_properties(document,&transform_refs));
    for(const auto& [id,target]:desired)for(std::size_t i=0;i<target.size();++i)
        require(transform_equal(after.at(id).world[i],target[i]),"TRANSFORM_PRESERVATION",
            "Selected world translation changed through dependent bindings or numeric conditioning");
}
void transform_objects(Document& document,const TransformObjects& command) {
    require(!command.objects.empty()&&command.objects.size()<=1000,"INVALID_BATCH","Transform requires 1..1000 unique objects");
    finite(command.rotation);finite(command.scale_x);finite(command.scale_y);
    require(std::abs(command.rotation)<=1e9&&std::abs(command.scale_x)<=1e9&&std::abs(command.scale_y)<=1e9,
        "OUT_OF_RANGE","Transform command magnitude limit 1e9");
    std::set<Id> selected;
    for(const auto& id:command.objects) {
        require(document.objects.contains(id),"MISSING_OBJECT",id);
        require(selected.insert(id).second,"DUPLICATE_TARGET","Each transformed object may occur only once");
    }
    const Composition* plane=nullptr;
    for(const auto& composition:document.compositions) {
        std::function<void(const Id&)> visit=[&](const Id& id) {
            if(selected.contains(id)) {
                require(!plane||plane==&composition,"CROSS_COMPOSITION","Transformed objects must share one Composition");
                plane=&composition;
            }
            for(const auto& child:document.objects.at(id).children)visit(child);
        };
        for(const auto& root:composition.roots)visit(root);
    }
    const auto values=evaluate(document);const auto transforms=evaluate_transforms(document,values);
    Vec2 pivot;
    if(command.pivot) {pivot={(*command.pivot)[0],(*command.pivot)[1]};finite(pivot.x);finite(pivot.y);}
    else {
        std::optional<Bounds> envelope;
        for(const auto& id:selected) {
            const auto bounds=object_bounds(document,id,values,transforms,true);
            require(bounds.has_value(),"EMPTY_BOUNDS","Choose an explicit pivot for geometry-free objects: "+id);
            if(!envelope)envelope=bounds;
            else {envelope->left=std::min(envelope->left,bounds->left);envelope->right=std::max(envelope->right,bounds->right);
                envelope->top=std::min(envelope->top,bounds->top);envelope->bottom=std::max(envelope->bottom,bounds->bottom);}
        }
        pivot={envelope->left+(envelope->right-envelope->left)/2,envelope->top+(envelope->bottom-envelope->top)/2};
    }
    const auto degrees=std::remainder(command.rotation,360.0);
    if(degrees==0&&command.scale_x==1&&command.scale_y==1)return;
    const auto angle=degrees*std::numbers::pi/180;auto cosine=std::cos(angle),sine=std::sin(angle);
    if(degrees==0){cosine=1;sine=0;}else if(degrees==90){cosine=0;sine=1;}
    else if(degrees==-90){cosine=0;sine=-1;}else if(std::abs(degrees)==180){cosine=-1;sine=0;}
    Affine edit{cosine*command.scale_x,sine*command.scale_x,-sine*command.scale_y,cosine*command.scale_y,0,0};
    edit[4]=pivot.x-edit[0]*pivot.x-edit[2]*pivot.y;
    edit[5]=pivot.y-edit[1]*pivot.x-edit[3]*pivot.y;
    std::map<Id,Affine> desired;
    for(const auto& id:selected) {
        const auto target=compose(edit,transforms.at(id).world);
        for(const auto value:target)require(std::isfinite(value),"OUTPUT_RANGE","Transformed world matrix must be finite");
        desired.emplace(id,target);
    }
    for(const auto& id:selected) {
        const auto& transform=transforms.at(id);auto ancestor=transform.effective_parent;
        while(!ancestor.empty()&&!selected.contains(ancestor))ancestor=transforms.at(ancestor).effective_parent;
        // The same left-multiplied world edit is inherited through any selected
        // effective ancestor. Retain exact local matrices, even for zero scale.
        if(!ancestor.empty())continue;
        const auto basis=transform.effective_parent.empty()?identity_matrix:transforms.at(transform.effective_parent).world;
        set_affine(document,id,compose(inverse_affine(basis),desired.at(id)),values);
    }
    const auto after=evaluate_transforms(document,evaluate(document));
    for(const auto& [id,target]:desired)for(std::size_t i=0;i<target.size();++i)
        require(transform_equal(after.at(id).world[i],target[i]),"TRANSFORM_PRESERVATION",
            "Selected world transform changed through dependent bindings or numeric conditioning");
}
void arrange_objects(Document& document,const std::vector<Id>& objects,const std::string& axis,
    const std::optional<std::string>& alignment,const std::string& requested_reference,
    const std::optional<double>& spacing,const std::optional<Id>& legacy_artboard={}) {
    require(axis=="x"||axis=="y","INVALID_ALIGNMENT","Axis must be x or y");
    require(!alignment||*alignment=="min"||*alignment=="center"||*alignment=="max"||*alignment=="baseline",
        "INVALID_ALIGNMENT","Alignment must be min, center, max or baseline");
    std::string reference=requested_reference;
    if(legacy_artboard) {
        require(reference=="selection","INVALID_REFERENCE","Specify either reference or the legacy artboard alias, not both");
        reference="artboard:"+*legacy_artboard;
    }
    enum class ReferenceKind { selection,key_object,artboard,grid,guide };
    struct Reference {ReferenceKind kind;Id id;};
    const auto parse_reference=[&]() {
        if(reference=="selection")return Reference{ReferenceKind::selection,{}};
        const auto separator=reference.find(':');
        require(separator!=std::string::npos&&separator+1<reference.size(),"INVALID_REFERENCE","Expected selection or a typed reference with an ID: "+reference);
        const auto kind=reference.substr(0,separator),id=reference.substr(separator+1);
        if(kind=="key_object")return Reference{ReferenceKind::key_object,id};
        if(kind=="artboard")return Reference{ReferenceKind::artboard,id};
        if(kind=="grid")return Reference{ReferenceKind::grid,id};
        if(kind=="guide")return Reference{ReferenceKind::guide,id};
        throw Error("INVALID_REFERENCE","Unsupported layout reference: "+reference);
    };
    const auto target=parse_reference();
    const bool baseline=alignment&&*alignment=="baseline";
    if(baseline)require(axis=="y","INVALID_ALIGNMENT","First-line baseline alignment only supports y");
    if(alignment)require(!spacing,"UNEXPECTED_SPACING","Spacing is only valid for key-object distribution");
    if(!alignment&&target.kind!=ReferenceKind::key_object)
        require(!spacing,"UNEXPECTED_SPACING","Explicit spacing is only valid for key-object distribution");
    if(!alignment&&target.kind==ReferenceKind::guide)
        throw Error("UNSUPPORTED_DISTRIBUTION_REFERENCE","Guide distribution is not supported");
    if(baseline&&target.kind!=ReferenceKind::selection&&target.kind!=ReferenceKind::key_object)
        throw Error("UNSUPPORTED_BASELINE_REFERENCE","Baseline alignment supports selection or key-object references only");
    if(target.kind==ReferenceKind::key_object)
        require(std::find(objects.begin(),objects.end(),target.id)!=objects.end(),"KEY_OBJECT_NOT_SELECTED","Key object must be included in the selection: "+target.id);
    if(!alignment&&target.kind==ReferenceKind::key_object) {
        require(spacing.has_value(),"MISSING_SPACING","Key-object distribution requires explicit spacing in Composition du");
        finite(*spacing);
        require(*spacing>=0,"NEGATIVE_SPACING","Distribution spacing must be nonnegative");
    }
    const std::size_t minimum_count=alignment?
        ((target.kind==ReferenceKind::selection||target.kind==ReferenceKind::key_object)?2u:1u):
        (target.kind==ReferenceKind::selection?3u:target.kind==ReferenceKind::key_object?2u:1u);
    require(objects.size()>=minimum_count&&objects.size()<=1000,"INVALID_BATCH","Selection size is invalid for the chosen Align / Distribute reference");
    std::set<Id> selected;
    for(const auto& id:objects){require(document.objects.contains(id),"MISSING_OBJECT",id);require(selected.insert(id).second,"DUPLICATE_TARGET",id);}
    const Composition* plane=nullptr;
    for(const auto& composition:document.compositions) {
        std::function<void(const Id&,bool)> visit=[&](const Id& id,bool selected_ancestor){
            const bool own=selected.contains(id);
            if(own){
                require(!selected_ancestor,"OVERLAPPING_SELECTION","Select a Group or its descendants, not both");
                require(!plane||plane==&composition,"CROSS_COMPOSITION","Alignment / distribution requires one Composition");
                plane=&composition;
            }
            for(const auto& child:document.objects.at(id).children)visit(child,selected_ancestor||own);
        };
        for(const auto& root:composition.roots)visit(root,false);
    }
    require(plane!=nullptr,"MISSING_COMPOSITION","Selection has no owning Composition");
    const auto cross_composition=[&](const Id& id,ReferenceKind kind) {
        for(const auto& composition:document.compositions)if(&composition!=plane) {
            if(kind==ReferenceKind::artboard&&std::any_of(composition.artboards.begin(),composition.artboards.end(),[&](const auto& item){return item.id==id;}))return true;
            if(kind==ReferenceKind::guide&&std::any_of(composition.guides.begin(),composition.guides.end(),[&](const auto& item){return item.id==id;}))return true;
            if(kind==ReferenceKind::grid&&std::any_of(composition.artboards.begin(),composition.artboards.end(),[&](const auto& item){return item.layout&&item.layout->grid&&item.layout->grid->id==id;}))return true;
        }
        return false;
    };
    const Artboard* target_artboard=nullptr;
    const Grid* target_grid=nullptr;
    const Guide* target_guide=nullptr;
    if(target.kind==ReferenceKind::artboard) {
        const auto found=std::find_if(plane->artboards.begin(),plane->artboards.end(),[&](const auto& item){return item.id==target.id;});
        if(found==plane->artboards.end())throw Error(cross_composition(target.id,target.kind)?"CROSS_COMPOSITION":"MISSING_ARTBOARD",target.id);
        target_artboard=&*found;
    } else if(target.kind==ReferenceKind::grid) {
        for(const auto& board:plane->artboards)if(board.layout&&board.layout->grid&&board.layout->grid->id==target.id) {
            target_artboard=&board;target_grid=&*board.layout->grid;break;
        }
        if(!target_grid)throw Error(cross_composition(target.id,target.kind)?"CROSS_COMPOSITION":"MISSING_GRID",target.id);
    } else if(target.kind==ReferenceKind::guide) {
        const auto found=std::find_if(plane->guides.begin(),plane->guides.end(),[&](const auto& item){return item.id==target.id;});
        if(found==plane->guides.end())throw Error(cross_composition(target.id,target.kind)?"CROSS_COMPOSITION":"MISSING_GUIDE",target.id);
        target_guide=&*found;
        require(target_guide->axis==axis,"GUIDE_AXIS_MISMATCH","Guide axis does not match the requested alignment axis: "+target.id);
    }
    const auto values=evaluate(document);const auto transforms=evaluate_transforms(document,values);
    std::map<Id,Bounds> initial;std::optional<Bounds> selection_bounds;
    for(const auto& id:objects) {
        const auto bounds=object_bounds(document,id,values,transforms,true);require(bounds.has_value(),"EMPTY_BOUNDS","Object has no geometric bounds: "+id);
        initial.emplace(id,*bounds);
        if(!selection_bounds)selection_bounds=bounds;
        else {
            selection_bounds->left=std::min(selection_bounds->left,bounds->left);selection_bounds->right=std::max(selection_bounds->right,bounds->right);
            selection_bounds->top=std::min(selection_bounds->top,bounds->top);selection_bounds->bottom=std::max(selection_bounds->bottom,bounds->bottom);
        }
    }
    const auto reference_bounds=[&]() -> std::optional<Bounds> {
        if(target.kind==ReferenceKind::selection)return selection_bounds;
        if(target.kind==ReferenceKind::key_object)return initial.at(target.id);
        if(target.kind==ReferenceKind::artboard) {
            const auto board=evaluate_artboard(*plane,target_artboard->id);
            return Bounds{board.x,board.y,board.x+board.width,board.y+board.height};
        }
        if(target.kind==ReferenceKind::grid) {
            const auto board=evaluate_artboard(*plane,target_artboard->id);const auto& grid=target_grid->bounds;
            return Bounds{board.x+grid.x,board.y+grid.y,board.x+grid.x+grid.width,board.y+grid.y+grid.height};
        }
        return std::nullopt;
    };
    const auto first_line_baselines=[&](const std::map<Ref,double>& scalar_values,
        const std::map<Id,EvaluatedTransform>& evaluated_transforms) {
        std::map<Id,double> result;
        for(const auto& id:objects) {
            const auto& object=document.objects.at(id);
            require(object.kind==Kind::text&&object.text.has_value(),"UNSUPPORTED_BASELINE","Baseline alignment requires Text objects with a first-line metric: "+id);
            std::map<std::string,double> parameters;
            for(const auto& [name,value]:object.text->parameters){(void)value;parameters.emplace(name,scalar_values.at({id,"","text."+name}));}
            auto text=*object.text;text.italic=evaluate_text_italic(document,id);
            const auto layout=evaluate_text(text,parameters);
            require(layout.first_line_baseline_y.has_value(),"UNSUPPORTED_BASELINE","Text has no horizontal first-line baseline metric: "+id);
            const auto& world=evaluated_transforms.at(id).world;
            require(world[1]==0&&world[2]==0,"UNSUPPORTED_BASELINE","Rotated or non-axis-aligned Text has no supported baseline: "+id);
            const auto y=world[3]*(*layout.first_line_baseline_y)+world[5];finite(y);result.emplace(id,y);
        }
        return result;
    };
    std::map<Id,Vec2> displacements;
    for(const auto& id:objects)displacements.emplace(id,Vec2{});
    double baseline_target=0;
    if(alignment) {
        if(baseline) {
            const auto baselines=first_line_baselines(values,transforms);
            Id source;
            if(target.kind==ReferenceKind::key_object)source=target.id;
            else source=*std::min_element(objects.begin(),objects.end(),[&](const Id& a,const Id& b){
                return baselines.at(a)==baselines.at(b)?a<b:baselines.at(a)<baselines.at(b);
            });
            baseline_target=baselines.at(source);
            for(const auto& id:objects)if(id!=source)displacements.at(id).y=baseline_target-baselines.at(id);
        } else {
            const auto coordinate=[&](const Bounds& bounds){
                const auto minimum=axis=="x"?bounds.left:bounds.top,maximum=axis=="x"?bounds.right:bounds.bottom;
                return *alignment=="min"?minimum:*alignment=="max"?maximum:minimum+(maximum-minimum)/2;
            };
            double desired=0;
            if(target.kind==ReferenceKind::guide)desired=target_guide->position;
            else desired=coordinate(*reference_bounds());
            for(const auto& id:objects) {
                if(target.kind==ReferenceKind::key_object&&id==target.id)continue;
                const auto delta=desired-coordinate(initial.at(id));
                displacements.at(id)=axis=="x"?Vec2{delta,0}:Vec2{0,delta};
            }
        }
    } else {
        auto minimum=[&](const Bounds& b){return axis=="x"?b.left:b.top;};
        auto maximum=[&](const Bounds& b){return axis=="x"?b.right:b.bottom;};
        auto extent=[&](const Bounds& b){return maximum(b)-minimum(b);};
        auto ordered=objects;
        std::sort(ordered.begin(),ordered.end(),[&](const Id& a,const Id& b){
            const auto x=minimum(initial.at(a)),y=minimum(initial.at(b));return x==y?a<b:x<y;
        });
        for(std::size_t i=1;i<ordered.size();++i)
            require(minimum(initial.at(ordered[i]))>=maximum(initial.at(ordered[i-1])),"OVERLAPPING_BOUNDS",
                "Distribution requires non-overlapping geometric bounds on the chosen axis");
        if(target.kind==ReferenceKind::key_object) {
            const auto key=std::find(ordered.begin(),ordered.end(),target.id);
            const auto key_index=static_cast<std::size_t>(std::distance(ordered.begin(),key));
            double prior_max=maximum(initial.at(target.id));
            for(std::size_t i=key_index+1;i<ordered.size();++i) {
                const auto& id=ordered[i];const auto destination=prior_max+*spacing;finite(destination);
                const auto delta=destination-minimum(initial.at(id));
                displacements.at(id)=axis=="x"?Vec2{delta,0}:Vec2{0,delta};
                prior_max=destination+extent(initial.at(id));finite(prior_max);
            }
            double prior_min=minimum(initial.at(target.id));
            for(std::size_t i=key_index;i>0;) {
                const auto& id=ordered[--i];const auto destination_max=prior_min-*spacing;finite(destination_max);
                const auto delta=destination_max-maximum(initial.at(id));
                displacements.at(id)=axis=="x"?Vec2{delta,0}:Vec2{0,delta};
                prior_min=destination_max-extent(initial.at(id));finite(prior_min);
            }
        } else if(target.kind==ReferenceKind::artboard||target.kind==ReferenceKind::grid) {
            const auto bounds=*reference_bounds();
            const auto reference_min=axis=="x"?bounds.left:bounds.top;
            const auto reference_max=axis=="x"?bounds.right:bounds.bottom;
            double total_extent=0;for(const auto& id:ordered)total_extent+=extent(initial.at(id));finite(total_extent);
            const auto gap=(reference_max-reference_min-total_extent)/static_cast<double>(ordered.size()+1);
            require(std::isfinite(gap)&&gap>=0,"REFERENCE_SPAN_TOO_SMALL","Reference span must fit every selected object and n+1 nonnegative gaps");
            double destination=reference_min+gap;
            for(const auto& id:ordered) {
                const auto delta=destination-minimum(initial.at(id));
                displacements.at(id)=axis=="x"?Vec2{delta,0}:Vec2{0,delta};
                destination+=extent(initial.at(id))+gap;finite(destination);
            }
        } else {
            double available=0;
            for(std::size_t i=1;i<ordered.size();++i)available+=minimum(initial.at(ordered[i]))-maximum(initial.at(ordered[i-1]));
            finite(available);
            const auto gap=available/static_cast<double>(ordered.size()-1);
            double destination=minimum(initial.at(ordered.front()));
            for(std::size_t i=0;i<ordered.size();++i) {
                const auto& id=ordered[i];const auto delta=(i==0||i+1==ordered.size())?0.0:destination-minimum(initial.at(id));
                displacements.at(id)=axis=="x"?Vec2{delta,0}:Vec2{0,delta};
                destination+=extent(initial.at(id))+gap;finite(destination);
            }
        }
    }
    translate_objects(document,objects,displacements);
    const auto after_values=evaluate(document);const auto after_transforms=evaluate_transforms(document,after_values);
    for(const auto& [id,before]:initial) {
        const auto after=object_bounds(document,id,after_values,after_transforms,true);const auto delta=displacements.at(id);
        require(after&&transform_equal(after->left,before.left+delta.x)&&transform_equal(after->right,before.right+delta.x)&&
            transform_equal(after->top,before.top+delta.y)&&transform_equal(after->bottom,before.bottom+delta.y),alignment?"ALIGNMENT_PRESERVATION":"DISTRIBUTION_PRESERVATION",
            "Dependent geometry changed during layout; resolve the dependency before arranging");
    }
    if(baseline) {
        const auto after_baselines=first_line_baselines(after_values,after_transforms);
        for(const auto& [id,value]:after_baselines) {
            (void)id;require(transform_equal(value,baseline_target),"ALIGNMENT_PRESERVATION","First-line baseline changed during alignment");
        }
    }
}
void group_contiguous(Document& document,const Id& composition,const Id& parent,const std::vector<Id>& members,const Id& id,const std::string& name) {
    require(!members.empty()&&!document.objects.contains(id),"INVALID_GROUP","New group ID and members required");
    auto& list=siblings(document,composition,parent);
    const auto start=std::search(list.begin(),list.end(),members.begin(),members.end());
    require(start!=list.end(),"NONCONTIGUOUS_GROUP","Only ordered contiguous siblings can be grouped without changing stacking");
    const auto index=std::distance(list.begin(),start);
    list.erase(start,start+static_cast<std::ptrdiff_t>(members.size()));list.insert(list.begin()+index,id);
    Object group;group.id=id;group.name=name;group.kind=Kind::group;group.children=members;
    document.objects.emplace(id,std::move(group));center_anchor(document,id,false);
}
void put_inside(Document& document,const PutInside& command) {
    require(!command.members.empty()&&command.members.size()<=1000,"INVALID_GROUP","Put Inside requires 1..1000 preceding siblings");
    require(document.objects.contains(command.group)&&document.objects.at(command.group).kind==Kind::group,"INVALID_PARENT","Put Inside destination must be a Group");
    auto& list=siblings(document,command.composition,command.parent);
    std::set<Id> unique;for(const auto& id:command.members)
        require(id!=command.group&&unique.insert(id).second,"DUPLICATE_TARGET","Moved roots must be unique and exclude the destination");
    auto block=command.members;block.push_back(command.group);
    const auto start=std::search(list.begin(),list.end(),block.begin(),block.end());
    require(start!=list.end(),"NONCONTIGUOUS_GROUP","Moved siblings must immediately precede the destination Group in order");
    const auto values=evaluate(document);const auto before=evaluate_transforms(document,values);
    const auto basis=before.at(command.group).world;
    list.erase(start,start+static_cast<std::ptrdiff_t>(command.members.size()));
    auto& children=document.objects.at(command.group).children;children.insert(children.begin(),command.members.begin(),command.members.end());
    std::optional<Affine> inverse;
    for(const auto& id:command.members) {
        if(document.objects.at(id).transform_parent)continue;
        const auto& old=before.at(id);const auto unchanged=compose(basis,old.local);
        bool matches=true;for(std::size_t i=0;i<6;++i)matches=matches&&transform_equal(unchanged[i],old.world[i]);
        if(matches)continue;
        if(!inverse)inverse=inverse_affine(basis);
        set_affine(document,id,compose(*inverse,old.world),values);
    }
    const auto after=evaluate_transforms(document,evaluate(document));
    for(const auto& id:command.members)for(std::size_t i=0;i<6;++i)
        require(transform_equal(before.at(id).world[i],after.at(id).world[i]),"TRANSFORM_PRESERVATION","Put Inside could not preserve moved world transforms through dependent bindings");
}
void ungroup(Document& document,const Ungroup& command) {
    require(document.objects.contains(command.group)&&document.objects.at(command.group).kind==Kind::group,"INVALID_GROUP","Ungroup requires a Group");
    const auto group=document.objects.at(command.group);auto& list=siblings(document,command.composition,command.parent);
    const auto at=std::find(list.begin(),list.end(),command.group);require(at!=list.end(),"INVALID_GROUP","Group must belong to the specified parent");
    require(group.visible&&group.compositing.blend=="normal"&&!group.compositing.isolated&&!group.compositing.mask&&
        group.compositing.opacity.literal==1&&!driven(group.compositing.opacity)&&group.stack.empty(),"UNGROUP_APPEARANCE","Ungroup requires a visible neutral Group without opacity, blend, isolation, mask or effects");
    require(!group.transform_parent&&std::none_of(group.transform.begin(),group.transform.end(),[](const Scalar& v){return driven(v);}),"UNGROUP_DYNAMIC","Ungroup requires static Group transforms following structure");
    const auto values=evaluate(document);const auto before=evaluate_transforms(document,values);std::set<Ref> changed_matrices;
    for(const auto& id:group.children) {
        if(document.objects.at(id).transform_parent)continue;
        set_affine(document,id,compose(before.at(command.group).local,before.at(id).local),values);
        for(const auto& field:affine_fields)changed_matrices.insert({id,"",field});
    }
    const auto index=std::distance(list.begin(),at);list.erase(at);list.insert(list.begin()+index,group.children.begin(),group.children.end());
    for(auto& collection:document.collections)std::erase(collection.members,command.group);
    document.objects.erase(command.group);
    // Existing reference validation refuses surviving links, expressions and
    // explicit Transform Parents that still address the removed container.
    validate(document);const auto after_values=evaluate(document);const auto after=evaluate_transforms(document,after_values);
    for(const auto& [id,old]:before)if(id!=command.group)for(std::size_t i=0;i<6;++i)
        require(transform_equal(old.world[i],after.at(id).world[i]),"TRANSFORM_PRESERVATION","Ungroup changed a surviving world transform");
    for(const auto& [ref,value]:values)if(ref.object!=command.group&&!changed_matrices.contains(ref))
        require(transform_equal(value,after_values.at(ref)),"UNGROUP_DEPENDENCY","Ungroup changed geometry or another property through a transform dependency");
}
struct DuplicationPlan {
    std::map<Id,Id> ids;
    std::vector<Id> roots;
    std::set<Id> objects;
};
DuplicationPlan plan_duplication(const Document& document,const DuplicateObjects& command) {
    require(!command.objects.empty()&&command.objects.size()<=1000,"INVALID_BATCH","Duplicate requires 1..1000 unique objects");
    identity(command.prefix);require(command.prefix.size()<=48,"INVALID_ID","Duplicate prefix is limited to 48 characters");
    std::set<Id> selected;
    for(const auto& id:command.objects) {
        require(document.objects.contains(id),"MISSING_OBJECT",id);
        require(selected.insert(id).second,"DUPLICATE_TARGET","Each selected object may occur only once");
    }
    DuplicationPlan plan;Id composition;
    std::function<void(const Id&,const Id&,bool)> visit=[&](const Id& id,const Id& owner,bool copied) {
        if(selected.contains(id)) {
            require(composition.empty()||composition==owner,"CROSS_COMPOSITION","Duplicate selection must belong to one Composition");composition=owner;
            if(!copied)plan.roots.push_back(id);
            copied=true;
        }
        if(copied)plan.objects.insert(id);
        for(const auto& child:document.objects.at(id).children)visit(child,owner,copied);
    };
    for(const auto& comp:document.compositions)for(const auto& root:comp.roots)visit(root,comp.id,false);
    require(document.objects.size()+plan.objects.size()<=10000,"LIMIT","Document object limit 10000");
    std::size_t serial=0;
    auto allocate=[&](const Id& id){plan.ids.emplace(id,command.prefix+"-"+std::to_string(++serial));};
    for(const auto& id:plan.objects)allocate(id);
    for(const auto& id:plan.objects) {
        const auto& object=document.objects.at(id);
        if(object.source) {
            allocate(object.source->id);
            for(const auto* suffix:{"-point-edit","-contour"})plan.ids.emplace(object.source->id+suffix,plan.ids.at(object.source->id)+suffix);
        }
        if(object.text)allocate(object.text->id);
        if(object.compositing.mask)allocate(object.compositing.mask->id);
        for(const auto& contour:object.contours){allocate(contour.id);for(const auto& point:contour.points)allocate(point.id);}
        for(const auto& op:object.stack) {
            allocate(op.id);if(op.gradient){allocate(op.gradient->id);for(const auto& stop:op.gradient->stops)allocate(stop.id);}
        }
    }
    return plan;
}
Ref duplicate_ref(const Document& original,const DuplicationPlan& plan,Ref ref) {
    if(!plan.objects.contains(ref.object))return ref;
    const auto& object=original.objects.at(ref.object);
    ref.object=plan.ids.at(ref.object);
    if(!ref.point.empty()) {
        if(generated_point(object,ref.point))ref.point=plan.ids.at(object.source->id)+ref.point.substr(object.source->id.size());
        else ref.point=plan.ids.at(ref.point);
    } else if(ref.field.starts_with("op.")) {
        const auto [op,parameter]=operation_address(ref.field);
        auto tail=parameter;
        if(tail.starts_with("gradient.")) {
            const auto end=tail.find('.',9);const auto gradient=tail.substr(9,end-9);tail=tail.substr(end+1);
            if(tail.starts_with("stop.")) {
                const auto stop_end=tail.find('.',5);const auto stop=tail.substr(5,stop_end-5);
                tail="stop."+plan.ids.at(stop)+tail.substr(stop_end);
            }
            tail="gradient."+plan.ids.at(gradient)+"."+tail;
        }
        ref.field="op."+plan.ids.at(op)+"."+tail;
    }
    return ref;
}
void duplicate_objects(Document& document,const DuplicateObjects& command) {
    const auto plan=plan_duplication(document,command);
    const auto original=document;
    auto remap=[&](const Ref& ref){return duplicate_ref(original,plan,ref);};
    for(const auto& id:plan.objects) {
        auto object=original.objects.at(id);object.id=plan.ids.at(id);
        // A long or invalid name never needs truncation to make a valid copy.
        if(object.name.size()<=4091)object.name+=" copy";
        for(auto& child:object.children)child=plan.ids.at(child);
        if(object.transform_parent&&plan.objects.contains(*object.transform_parent))object.transform_parent=plan.ids.at(*object.transform_parent);
        if(object.compositing.mask) {
            auto& mask=*object.compositing.mask;mask.id=plan.ids.at(mask.id);
            if(plan.objects.contains(mask.source))mask.source=plan.ids.at(mask.source);
        }
        if(object.source)object.source->id=plan.ids.at(object.source->id);
        if(object.text) {
            object.text->id=plan.ids.at(object.text->id);
            if(object.text->italic_driver) {
                if(auto link=std::get_if<Ref>(&*object.text->italic_driver))*link=remap(*link);
                else object.text->italic_driver=remap_text_italic_expression(std::get<Expression>(*object.text->italic_driver),remap);
            }
        }
        if(object.point_edit) {
            auto& edit=*object.point_edit;edit.id=plan.ids.at(edit.id);
            auto overrides=std::move(edit.overrides);edit.overrides.clear();
            for(auto& [point,fields]:overrides)edit.overrides.emplace(remap({id,point,"x"}).point,std::move(fields));
        }
        for(auto& contour:object.contours){contour.id=plan.ids.at(contour.id);for(auto& point:contour.points)point.id=plan.ids.at(point.id);}
        for(auto& op:object.stack) {
            op.id=plan.ids.at(op.id);if(op.gradient){op.gradient->id=plan.ids.at(op.gradient->id);for(auto& stop:op.gradient->stops)stop.id=plan.ids.at(stop.id);}
        }
        if(!object.legacy_stroke.empty())object.legacy_stroke=plan.ids.at(object.legacy_stroke);
        require(document.objects.emplace(object.id,std::move(object)).second,"DUPLICATE_ID","Duplicate prefix collides with an existing object");
    }
    // Includes disabled Point Edits/operations and every Scalar, including aliases.
    std::set<Scalar*> rewritten;
    for(const auto& [ref,value]:property_index(original,true))if(plan.objects.contains(ref.object)&&value) {
        auto& scalar=lookup_property(document,remap(ref));if(!rewritten.insert(&scalar).second)continue;
        if(scalar.binding)scalar.binding->source=remap(scalar.binding->source);
        if(scalar.expression)scalar.expression=remap_expression(*scalar.expression,remap);
    }
    const std::set<Id> roots(plan.roots.begin(),plan.roots.end());
    // Copies follow each selected sibling run, retaining its relative paint order.
    auto insert=[&](std::vector<Id>& list) {
        std::vector<Id> result,pending;
        for(const auto& id:list) {
            if(!roots.contains(id)){result.insert(result.end(),pending.begin(),pending.end());pending.clear();}
            result.push_back(id);if(roots.contains(id))pending.push_back(plan.ids.at(id));
        }
        result.insert(result.end(),pending.begin(),pending.end());list=std::move(result);
    };
    for(auto& comp:document.compositions)insert(comp.roots);
    for(const auto& [id,object]:original.objects){(void)object;if(!plan.objects.contains(id))insert(document.objects.at(id).children);}
}
Document edited(const Document& document,const std::vector<Command>& commands,std::map<Ref,double>* evaluated=nullptr) {
    require(!commands.empty()&&commands.size()<=1000,"INVALID_BATCH","Batch must have 1..1000 commands");

    auto candidate=document;
    for(const auto& command:commands) std::visit([&](const auto& c) {
        using T=std::decay_t<decltype(c)>;
        if constexpr(std::is_same_v<T,DuplicateObjects>) {
            duplicate_objects(candidate,c);
        } else if constexpr(std::is_same_v<T,SetVisibility>||std::is_same_v<T,SetCompositing>||std::is_same_v<T,SetMask>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);auto& object=candidate.objects.at(c.object);
            if constexpr(std::is_same_v<T,SetVisibility>)object.visible=c.visible;
            else if constexpr(std::is_same_v<T,SetCompositing>){object.compositing.blend=c.blend;object.compositing.isolated=c.isolated;}
            else object.compositing.mask=c.mask;
        } else if constexpr(std::is_same_v<T,MaskObjects>) {
            require(c.members.size()>=2&&c.members.size()<=1000,"INVALID_GROUP","Mask With requires 2..1000 ordered contiguous siblings");
            const auto source=c.top?c.members.back():c.members.front();
            require(candidate.objects.contains(source)&&(candidate.objects.at(source).kind==Kind::path||candidate.objects.at(source).kind==Kind::text),"INVALID_MASK_SOURCE","Mask With source must be a Path or Text");
            group_contiguous(candidate,c.composition,c.parent,c.members,c.id,c.name);
            candidate.objects.at(c.id).compositing.mask=GeometryMask{c.mask_id,source};candidate.objects.at(source).visible=false;
        } else if constexpr(std::is_same_v<T,Ungroup>) {
            ungroup(candidate,c);
        } else if constexpr(std::is_same_v<T,PutInside>) {
            put_inside(candidate,c);
        } else if constexpr(std::is_same_v<T,SetExpression>) {
            require(!c.targets.empty()&&c.targets.size()<=1000,"INVALID_BATCH","Expression targets must contain 1..1000 unique Scalars");
            const auto expression=compile_expression(c.expression);const auto expected_unit=unit(c.targets.front());
            validate_expression_unit(expression,expected_unit);std::set<Ref> unique;
            for(const auto& target:c.targets) {
                require(unit(target)==expected_unit,"UNIT_MISMATCH","Expression targets must share one scalar unit");
                prepare_point_edit(candidate,target);auto& scalar=lookup_property(candidate,target);
                require(unique.insert(canonical_target(candidate,target)).second,"DUPLICATE_TARGET","Each scalar target may occur only once, including aliases");
                require(!scalar.binding||c.replace_binding,"DRIVEN_PROPERTY","Replacing an existing Binding requires replace_binding=true");
                scalar.binding.reset();scalar.expression=c.expression;
            }
        } else if constexpr(std::is_same_v<T,LinkTextItalic>) {
            const auto& current=text_italic_source(candidate,c.target);
            (void)text_italic_source(candidate,c.source);
            require(!current.italic_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text italic driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->italic_driver=TextItalicDriver{c.source};
        } else if constexpr(std::is_same_v<T,SetTextItalicExpression>) {
            const auto& current=text_italic_source(candidate,c.target);
            (void)parse_text_italic_expression(c.expression);
            require(!current.italic_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text italic driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->italic_driver=TextItalicDriver{c.expression};
        } else if constexpr(std::is_same_v<T,UnlinkTextItalic>) {
            const auto value=evaluate_text_italic(candidate,c.target.object);
            auto& source=*candidate.objects.at(c.target.object).text;
            (void)text_italic_source(candidate,c.target);
            source.italic=value;source.italic_driver.reset();
        } else if constexpr(std::is_same_v<T,EditProperties>||std::is_same_v<T,LinkProperties>||std::is_same_v<T,UnlinkProperties>) {
            require(!c.targets.empty()&&c.targets.size()<=1000,"INVALID_BATCH","Property targets must contain 1..1000 unique Scalars");
            const auto values=evaluate(candidate);scalar_targets(candidate,c.targets,values);
            if constexpr(std::is_same_v<T,EditProperties>)finite(c.value);
            else if constexpr(std::is_same_v<T,LinkProperties>) {
                require(values.contains(c.source),"MISSING_REFERENCE",c.source.object+"/"+c.source.point+"/"+c.source.field);
                require(unit(c.source)==unit(c.targets.front()),"UNIT_MISMATCH","Link source and targets must share one scalar unit");
            }
            for(const auto& target:c.targets) {
                prepare_point_edit(candidate,target);auto& scalar=lookup_property(candidate,target);
                if constexpr(std::is_same_v<T,EditProperties>) {
                    require(!driven(scalar),"DRIVEN_PROPERTY","Unlink explicitly before editing a driven property");
                    scalar.literal=c.relative?values.at(target)+c.value:c.value;
                } else if constexpr(std::is_same_v<T,LinkProperties>) {
                    scalar.binding=Binding{c.source,1,c.relative?values.at(target)-values.at(c.source):0,"copy_local_value"};
                    scalar.expression.reset();
                } else scalar=Scalar{values.at(target),{}};
            }
        } else if constexpr(std::is_same_v<T,TranslateObjects>) {
            std::map<Id,Vec2> displacements;for(const auto& id:c.objects)displacements.emplace(id,Vec2{c.dx,c.dy});
            translate_objects(candidate,c.objects,displacements);
        } else if constexpr(std::is_same_v<T,TransformObjects>) {
            transform_objects(candidate,c);
        } else if constexpr(std::is_same_v<T,AlignObjects>) {
            arrange_objects(candidate,c.objects,c.axis,c.alignment,c.reference,{},c.artboard);
        } else if constexpr(std::is_same_v<T,DistributeObjects>) {
            arrange_objects(candidate,c.objects,c.axis,{},c.reference,c.spacing);
        } else if constexpr(std::is_same_v<T,CenterAnchor>) {
            center_anchor(candidate,c.object,true);
        } else if constexpr(std::is_same_v<T,SetPosition>||std::is_same_v<T,TransformAroundAnchor>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            const auto values=evaluate(candidate);const auto before=local_affine(c.object,values);auto matrix=before;
            const Vec2 anchor{values.at({c.object,"","transform.anchor_x"}),values.at({c.object,"","transform.anchor_y"})};
            auto position=map_point(before,anchor);
            if constexpr(std::is_same_v<T,SetPosition>) {
                finite(c.x);finite(c.y);position={c.x,c.y};
            } else {
                finite(c.rotation);finite(c.scale_x);finite(c.scale_y);
                require(std::abs(c.rotation)<=1e9&&std::abs(c.scale_x)<=1e9&&std::abs(c.scale_y)<=1e9,"OUT_OF_RANGE","Transform command magnitude limit 1e9");
                if(c.rotation==0&&c.scale_x==1&&c.scale_y==1)return;
                const auto degrees=std::remainder(c.rotation,360.0);const auto angle=degrees*std::numbers::pi/180;
                auto cosine=std::cos(angle),sine=std::sin(angle);
                if(degrees==0){cosine=1;sine=0;}else if(degrees==90){cosine=0;sine=1;}
                else if(degrees==-90){cosine=0;sine=-1;}else if(std::abs(degrees)==180){cosine=-1;sine=0;}
                matrix[0]=(cosine*before[0]-sine*before[1])*c.scale_x;
                matrix[1]=(sine*before[0]+cosine*before[1])*c.scale_x;
                matrix[2]=(cosine*before[2]-sine*before[3])*c.scale_y;
                matrix[3]=(sine*before[2]+cosine*before[3])*c.scale_y;
            }
            matrix[4]=position.x-matrix[0]*anchor.x-matrix[2]*anchor.y;
            matrix[5]=position.y-matrix[1]*anchor.x-matrix[3]*anchor.y;
            set_affine(candidate,c.object,matrix,values);
            const auto after_values=evaluate(candidate);const auto after=local_affine(c.object,after_values);
            const Vec2 after_anchor{after_values.at({c.object,"","transform.anchor_x"}),after_values.at({c.object,"","transform.anchor_y"})};
            const auto after_position=map_point(after,after_anchor);
            for(std::size_t i=0;i<matrix.size();++i)require(transform_equal(matrix[i],after[i]),"TRANSFORM_PRESERVATION",
                "Transform result changed through dependent bindings; the requested transform was not committed");
            require(transform_equal(position.x,after_position.x)&&transform_equal(position.y,after_position.y),"TRANSFORM_PRESERVATION",
                "Anchor placement changed through dependent bindings; the requested Position or pivot transform was not committed");
        } else if constexpr(std::is_same_v<T,SetTransformParent>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& object=candidate.objects.at(c.object);
            if(object.transform_parent==c.parent)return;
            if(!c.preserve_world){object.transform_parent=c.parent;return;}
            const auto values=evaluate(candidate);const auto before=evaluate_transforms(candidate,values).at(c.object).world;
            object.transform_parent=c.parent;
            const auto transforms=evaluate_transforms(candidate,values);const auto& parent=transforms.at(c.object).effective_parent;
            const auto basis=parent.empty()?identity_matrix:transforms.at(parent).world;
            const auto matrix=compose(inverse_affine(basis),before);
            set_affine(candidate,c.object,matrix,values);
            const auto after=evaluate_transforms(candidate,evaluate(candidate)).at(c.object).world;
            for(std::size_t i=0;i<before.size();++i)require(transform_equal(before[i],after[i]),"TRANSFORM_PRESERVATION",
                "Keep-world transform could not be preserved; check dependent matrix bindings or numeric conditioning");
        } else if constexpr(std::is_same_v<T,AddRasterAsset>) {
            require(candidate.raster_assets.emplace(c.asset.id,c.asset).second,"DUPLICATE_ID",c.asset.id);
        } else if constexpr(std::is_same_v<T,ReplaceRasterAsset>) {
            require(candidate.raster_assets.contains(c.asset.id),"MISSING_ASSET",c.asset.id);
            candidate.raster_assets.at(c.asset.id)=c.asset;
        } else if constexpr(std::is_same_v<T,DeleteRasterAsset>) {
            require(candidate.raster_assets.contains(c.asset),"MISSING_ASSET",c.asset);
            for(const auto& [id,object]:candidate.objects)require(!object.image||object.image->asset!=c.asset,"ASSET_IN_USE","Asset still has an Image placement");
            candidate.raster_assets.erase(c.asset);
        } else if constexpr(std::is_same_v<T,CreateImage>) {
            require(!candidate.objects.contains(c.id),"DUPLICATE_ID",c.id);
            Object object;object.id=c.id;object.name=c.name;object.kind=Kind::image;object.image=c.source;
            object.anchor[0].literal=c.source.width.literal/2;object.anchor[1].literal=c.source.height.literal/2;
            siblings(candidate,c.composition,c.parent).push_back(c.id);candidate.objects.emplace(c.id,std::move(object));
        } else if constexpr(std::is_same_v<T,CreateNamedColor>) {
            require(candidate.named_colors.emplace(c.color.id,c.color).second,"DUPLICATE_ID",c.color.id);
        } else if constexpr(std::is_same_v<T,RenameNamedColor>||std::is_same_v<T,DeleteNamedColor>) {
            const auto found=candidate.named_colors.find(c.color);require(found!=candidate.named_colors.end(),"MISSING_COLOR",c.color);
            if constexpr(std::is_same_v<T,RenameNamedColor>)found->second.name=c.name;
            else candidate.named_colors.erase(found);
        } else if constexpr(std::is_same_v<T,SetColor>) {
            require(c.value.space=="srgb"&&c.value.profile=="srgb"&&c.value.alpha=="straight","UNSUPPORTED_COLOR","Only sRGB, sRGB profile, straight alpha colors are supported");
            const auto channels=color_channels(candidate,c.ref);
            for(std::size_t i=0;i<4;++i) {
                auto& scalar=lookup_property(candidate,channels[i]);require(!driven(scalar),"DRIVEN_PROPERTY","Unlink the color explicitly before replacing its value");
                scalar.literal=c.value.rgba[i];
            }
        } else if constexpr(std::is_same_v<T,LinkColor>) {
            const auto targets=color_channels(candidate,c.target),sources=color_channels(candidate,c.source);
            for(std::size_t i=0;i<4;++i) {
                auto& scalar=lookup_property(candidate,targets[i]);scalar.binding=Binding{sources[i],1,0,"copy_local_value"};scalar.expression.reset();
            }
        } else if constexpr(std::is_same_v<T,UnlinkColor>) {
            const auto channels=color_channels(candidate,c.ref);const auto values=evaluate(candidate);
            for(const auto& ref:channels)lookup_property(candidate,ref)=Scalar{values.at(ref),{}};
        } else if constexpr(std::is_same_v<T,AddArtboard>||std::is_same_v<T,UpdateArtboard>||std::is_same_v<T,DeleteArtboard>||
            std::is_same_v<T,ReorderArtboards>||std::is_same_v<T,DetachArtboardParent>) {
            auto comp=std::find_if(candidate.compositions.begin(),candidate.compositions.end(),[&](const auto& item){return item.id==c.composition;});
            require(comp!=candidate.compositions.end(),"MISSING_COMPOSITION",c.composition);
            auto& boards=comp->artboards;
            if constexpr(std::is_same_v<T,AddArtboard>) {
                require(c.index<=boards.size(),"INVALID_ORDER","Artboard insertion index outside range");
                boards.insert(boards.begin()+static_cast<std::ptrdiff_t>(c.index),c.artboard);
            } else if constexpr(std::is_same_v<T,ReorderArtboards>) {
                std::map<Id,Artboard> old;for(const auto& board:boards)old.emplace(board.id,board);
                require(c.order.size()==old.size(),"INVALID_ORDER","Artboard order must be a permutation");
                boards.clear();for(const auto& id:c.order) {
                    require(old.contains(id),"INVALID_ORDER",id);boards.push_back(old.at(id));old.erase(id);
                }
            } else {
                const auto id=[&]{if constexpr(std::is_same_v<T,UpdateArtboard>)return c.artboard.id;else return c.artboard;}();
                auto board=std::find_if(boards.begin(),boards.end(),[&](const auto& a){return a.id==id;});
                require(board!=boards.end(),"MISSING_ARTBOARD",id);
                if constexpr(std::is_same_v<T,UpdateArtboard>) {
                    auto updated=c.artboard;
                    // Legacy Artboard updates carry only frame fields. A missing
                    // layout payload must not erase authored P02 definitions.
                    if(!updated.layout)updated.layout=board->layout;
                    *board=std::move(updated);
                }
                else if constexpr(std::is_same_v<T,DeleteArtboard>) {
                    require(boards.size()>1,"LAST_ARTBOARD","Keep at least one output frame per Composition");
                    boards.erase(board);
                } else {
                    auto resolved=evaluate_artboard(*comp,id);resolved.parent_size.reset();*board=std::move(resolved);
                }
            }
        } else if constexpr(std::is_same_v<T,AddGuide>||std::is_same_v<T,UpdateGuide>||std::is_same_v<T,DeleteGuide>) {
            auto comp=std::find_if(candidate.compositions.begin(),candidate.compositions.end(),[&](const auto& item){return item.id==c.composition;});
            require(comp!=candidate.compositions.end(),"MISSING_COMPOSITION",c.composition);
            if constexpr(std::is_same_v<T,AddGuide>)comp->guides.push_back(c.guide);
            else {
                const auto id=[&]{if constexpr(std::is_same_v<T,UpdateGuide>)return c.guide.id;else return c.guide_id;}();
                auto guide=std::find_if(comp->guides.begin(),comp->guides.end(),[&](const auto& item){return item.id==id;});
                if(guide==comp->guides.end()) {
                    const bool elsewhere=std::any_of(candidate.compositions.begin(),candidate.compositions.end(),[&](const auto& other) {
                        return other.id!=c.composition&&std::any_of(other.guides.begin(),other.guides.end(),[&](const auto& item){return item.id==id;});
                    });
                    if(elsewhere)throw Error("WRONG_COMPOSITION",id);
                    throw Error("MISSING_GUIDE",id);
                }
                if constexpr(std::is_same_v<T,UpdateGuide>)*guide=c.guide;
                else comp->guides.erase(guide);
            }
        } else if constexpr(std::is_same_v<T,SetArtboardLayout>) {
            auto comp=std::find_if(candidate.compositions.begin(),candidate.compositions.end(),[&](const auto& item){return item.id==c.composition;});
            require(comp!=candidate.compositions.end(),"MISSING_COMPOSITION",c.composition);
            auto board=std::find_if(comp->artboards.begin(),comp->artboards.end(),[&](const auto& item){return item.id==c.artboard_id;});
            if(board==comp->artboards.end()) {
                const bool elsewhere=std::any_of(candidate.compositions.begin(),candidate.compositions.end(),[&](const auto& other) {
                    return other.id!=c.composition&&std::any_of(other.artboards.begin(),other.artboards.end(),[&](const auto& item){return item.id==c.artboard_id;});
                });
                if(elsewhere)throw Error("WRONG_COMPOSITION",c.artboard_id);
                throw Error("MISSING_ARTBOARD",c.artboard_id);
            }
            board->layout=c.layout;
        } else if constexpr(std::is_same_v<T,Set>) {
            prepare_point_edit(candidate,c.ref);
            auto& p=lookup_property(candidate,c.ref);
            require(!driven(p),"DRIVEN_PROPERTY","Unlink explicitly before setting a driven property");
            p.literal=c.value;
        } else if constexpr(std::is_same_v<T,Link>) {
            prepare_point_edit(candidate,c.target);
            auto& scalar=lookup_property(candidate,c.target);scalar.binding=c.binding;scalar.expression.reset();
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
        } else if constexpr(std::is_same_v<T,CreateText>) {
            require(!candidate.objects.contains(c.id),"DUPLICATE_ID",c.id);
            require(!c.source.italic_driver,"USE_TYPED_COMMAND","Create Text Italic links with link_text_italic or set_text_italic_expression");
            Object object;object.id=c.id;object.name=c.name;object.kind=Kind::text;object.text=c.source;
            siblings(candidate,c.composition,c.parent).push_back(c.id);candidate.objects.emplace(c.id,std::move(object));
            add_default_paint(candidate,c.id,"nect.paint.fill");
        } else if constexpr(std::is_same_v<T,UpdateText>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);auto& o=candidate.objects.at(c.object);
            require(o.kind==Kind::text&&o.text.has_value(),"INVALID_TEXT","Select an editable Text object");
            require(c.source.id==o.text->id,"ID_MISMATCH","Text edits must retain the source identity");
            auto next=c.source;
            if(o.text->italic_driver) {
                require(!next.italic_driver||next.italic_driver==o.text->italic_driver,"DRIVEN_PROPERTY","UpdateText cannot replace or remove a Text italic driver");
                require(next.italic==o.text->italic,"DRIVEN_PROPERTY","Unlink or replace the Text italic driver before changing its authored literal");
                next.italic_driver=o.text->italic_driver;
            } else require(!next.italic_driver,"USE_TYPED_COMMAND","Create Text Italic links with link_text_italic or set_text_italic_expression");
            o.text=std::move(next);
        } else if constexpr(std::is_same_v<T,CreatePrimitive>) {
            require(!candidate.objects.contains(c.id),"DUPLICATE_ID",c.id);
            Object object;object.id=c.id;object.name=c.name;object.source=c.source;
            siblings(candidate,c.composition,c.parent).push_back(c.id);
            candidate.objects.emplace(c.id,std::move(object));
            add_default_stroke(candidate,c.id);
        } else if constexpr(std::is_same_v<T,AddOperation>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& o=candidate.objects.at(c.object);
            require(o.kind==Kind::path||o.kind==Kind::text,"INVALID_DOMAIN","Shape stack requires a Path or Text source");
            require(c.index<=o.stack.size(),"INVALID_ORDER","Operation insertion index out of range");
            o.stack.insert(o.stack.begin()+c.index,c.operation);
        } else if constexpr(std::is_same_v<T,RemoveOperation>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& o=candidate.objects.at(c.object);(void)operation(o,c.operation);
            std::erase_if(o.stack,[&](const auto& op){return op.id==c.operation;});
            if(o.legacy_stroke==c.operation)o.legacy_stroke.clear();
        } else if constexpr(std::is_same_v<T,ReorderOperations>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& stack=candidate.objects.at(c.object).stack;
            require(c.order.size()==stack.size(),"INVALID_ORDER","Operation order must be a permutation");
            std::map<Id,ShapeOperation> old;for(const auto& op:stack)old.emplace(op.id,op);
            stack.clear();for(const auto& id:c.order){require(old.contains(id),"INVALID_ORDER",id);stack.push_back(old.at(id));old.erase(id);}
        } else if constexpr(std::is_same_v<T,EnableOperation>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            operation(candidate.objects.at(c.object),c.operation).enabled=c.enabled;
        } else if constexpr(std::is_same_v<T,OperationOptions>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& op=operation(candidate.objects.at(c.object),c.operation);op.composite=c.composite;op.fill_rule=c.fill_rule;
            if(c.line_join)op.line_join=*c.line_join;
        } else if constexpr(std::is_same_v<T,StrokeStyle>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& op=operation(candidate.objects.at(c.object),c.operation);
            require(op.type=="nect.paint.stroke","INVALID_DOMAIN","Stroke style requires a Stroke operation");
            const auto ref=operation_ref(c.object,c.operation,"miter_limit");value_range(ref,c.miter_limit);
            if(op.version==1)op.parameters.emplace("miter_limit",Scalar{c.miter_limit,{}});
            else set_changed_scalar(candidate,ref,c.miter_limit,evaluate(candidate));
            op.version=2;op.line_cap=c.line_cap;op.line_join=c.line_join;
        } else if constexpr(std::is_same_v<T,SetGradient>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            operation(candidate.objects.at(c.object),c.operation).gradient=c.gradient;
        } else if constexpr(std::is_same_v<T,EnablePointEdit>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& o=candidate.objects.at(c.object);
            require(o.point_edit.has_value(),"NO_POINT_EDIT","Primitive has no authored point corrections");
            o.point_edit->enabled=c.enabled;
        } else if constexpr(std::is_same_v<T,ClearPointEdit>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& o=candidate.objects.at(c.object);
            require(o.source.has_value(),"NOT_PRIMITIVE","Point Edit reset requires a retained primitive");
            o.point_edit.reset();
        } else if constexpr(std::is_same_v<T,ConvertToPath>) {
            require(conversion_blockers(candidate,c.object).empty(),"CONVERSION_REFERENCE",
                "Generator properties are referenced; explicitly unlink/freeze dependent targets before conversion");
            auto& o=candidate.objects.at(c.object);
            const auto values=evaluate(candidate);
            const auto authored=property_index(candidate,true);
            auto contours=path_contours(o,&values);
            for(auto& ct:contours)for(auto& p:ct.points) {
                const std::array<Scalar*,6> fields{&p.x,&p.y,&p.in_angle,&p.in_length,&p.out_angle,&p.out_length};
                for(std::size_t i=0;i<fields.size();++i) {
                    const Ref ref{o.id,p.id,point_fields[i]};
                    const auto found=authored.find(ref);const auto* override_value=found==authored.end()?nullptr:found->second;
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
            add_default_stroke(candidate,c.id);
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
            group_contiguous(candidate,c.composition,c.parent,c.members,c.id,c.name);
        }
    },command);

    auto values=validate_evaluated(candidate);
    if(evaluated)*evaluated=std::move(values);
    return candidate;
}
}

std::vector<Id> duplicated_roots(const Document& document,const DuplicateObjects& command) {
    const auto plan=plan_duplication(document,command);std::vector<Id> result;
    for(const auto& id:plan.roots)result.push_back(plan.ids.at(id));return result;
}

void Session::apply(const std::vector<Command>& commands,std::uint64_t expected) {
    check_revision(expected);
    auto candidate=edited(document_,commands);
    const bool only_layout= !commands.empty()&&std::all_of(commands.begin(),commands.end(),[](const auto& command) {
        return std::holds_alternative<AlignObjects>(command)||std::holds_alternative<DistributeObjects>(command);
    });
    if(only_layout&&candidate==document_)return;
    auto label=history_label(commands,candidate);
    commit(std::move(candidate),std::move(label));
}

void Session::begin_gesture(std::uint64_t expected) {
    check_revision(expected);
    preview_=document_;
    preview_values_.reset();
    preview_changed_=false;
    preview_label_.clear();
}

void Session::update_gesture(const std::vector<Command>& commands) {
    require(gesture_active(),"NO_GESTURE","No active gesture");
    if(commands.empty()) { preview_=document_; preview_values_.reset(); preview_changed_=false; preview_label_.clear(); return; }
    std::map<Ref,double> values;
    auto next=edited(document_,commands,&values);
    auto label=history_label(commands,next);
    const bool only_layout=std::all_of(commands.begin(),commands.end(),[](const auto& command) {
        return std::holds_alternative<AlignObjects>(command)||std::holds_alternative<DistributeObjects>(command);
    });
    const bool changed=!(only_layout&&next==document_);
    preview_=std::move(next);
    preview_values_=std::move(values);
    preview_label_=std::move(label);
    preview_changed_=changed;
}

void Session::commit_gesture() {
    require(gesture_active(),"NO_GESTURE","No active gesture");
    // Keep the preview intact if history admission rejects the commit.
    if(preview_changed_) commit(*preview_,preview_label_);
    preview_.reset();
    preview_values_.reset();
    preview_changed_=false;
    preview_label_.clear();
}

void Session::cancel_gesture() {
    preview_.reset();
    preview_values_.reset();
    preview_changed_=false;
    preview_label_.clear();
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
        add_default_stroke(d,o.id);
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
