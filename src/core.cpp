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
std::pair<Id,std::string> operation_address(const std::string& field);
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
const TextSource& text_weight_source(const Document& document,const Ref& ref) {
    require(ref.point.empty()&&ref.field=="text.weight","TYPE_MISMATCH","Only Text weight accepts an integer property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text weight Ref must identify a Text object");
    return *object->second.text;
}
const TextSource& text_content_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_TEXT_REF","Text content properties require an empty point ID");
    require(ref.field=="text.content","TYPE_MISMATCH","Only Text content accepts a string property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text content Ref must identify a Text object");
    return *object->second.text;
}
const TextSource& text_family_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_TEXT_REF","Text family properties require an empty point ID");
    require(ref.field=="text.family","TYPE_MISMATCH","Only Text family accepts a string property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text family Ref must identify a Text object");
    return *object->second.text;
}
const TextSource& text_locale_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_TEXT_REF","Text locale properties require an empty point ID");
    require(ref.field=="text.locale","TYPE_MISMATCH","Only Text locale accepts a string property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text locale Ref must identify a Text object");
    return *object->second.text;
}
const TextSource& text_direction_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_TEXT_REF","Text direction properties require an empty point ID");
    require(ref.field=="text.direction","TYPE_MISMATCH","Only Text direction accepts an enum property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text direction Ref must identify a Text object");
    return *object->second.text;
}
const TextSource& text_layout_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_TEXT_REF","Text layout properties require an empty point ID");
    require(ref.field=="text.layout","TYPE_MISMATCH","Only Text layout accepts an enum property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text layout Ref must identify a Text object");
    return *object->second.text;
}
const TextSource& text_alignment_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_TEXT_REF","Text alignment properties require an empty point ID");
    require(ref.field=="text.alignment","TYPE_MISMATCH","Only Text alignment accepts an enum property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text alignment Ref must identify a Text object");
    return *object->second.text;
}
const ShapeOperation& fill_rule_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_OPERATION_REF","Fill rule properties require an empty point ID");
    require(ref.field.starts_with("op."),"TYPE_MISMATCH","Fill rule Ref must identify an operation property");
    const auto [operation_id,parameter]=operation_address(ref.field);
    require(parameter=="fill_rule","TYPE_MISMATCH","Only Fill rule accepts this operation property Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::path||object->second.kind==Kind::text,
        "TYPE_MISMATCH","Fill rule Ref must identify a Path or Text object");
    const auto found=std::find_if(object->second.stack.begin(),object->second.stack.end(),
        [&](const auto& operation){return operation.id==operation_id;});
    require(found!=object->second.stack.end(),"MISSING_OPERATION",operation_id);
    require(found->type=="nect.paint.fill","INVALID_DOMAIN","Fill rule links apply only to Fill operations");
    return *found;
}
const ShapeOperation& operation_enabled_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_OPERATION_REF","Operation enabled properties require an empty point ID");
    require(ref.field.starts_with("op."),"TYPE_MISMATCH","Operation enabled Ref must identify an operation property");
    const auto [operation_id,parameter]=operation_address(ref.field);
    identity(operation_id);
    require(parameter=="enabled","TYPE_MISMATCH","Only operation enabled accepts this Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::path||object->second.kind==Kind::text||object->second.kind==Kind::group,
        "INVALID_DOMAIN","Operation enabled requires a Path, Text or Group");
    const auto found=std::find_if(object->second.stack.begin(),object->second.stack.end(),
        [&](const auto& operation){return operation.id==operation_id;});
    require(found!=object->second.stack.end(),"MISSING_OPERATION",operation_id);
    return *found;
}
std::pair<Id,std::string> geometry_mask_enabled_address(const std::string& field) {
    constexpr std::string_view prefix="mask.";
    constexpr std::string_view suffix=".enabled";
    require(field.starts_with(prefix)&&field.ends_with(suffix)&&field.size()>prefix.size()+suffix.size(),
        "INVALID_MASK_REF","Mask enabled Ref must name one mask instance");
    const auto mask_length=field.size()-prefix.size()-suffix.size();
    const auto mask_id=field.substr(prefix.size(),mask_length);
    identity(mask_id);
    return {mask_id,"enabled"};
}
const GeometryMask& geometry_mask_enabled_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_MASK_REF","Geometry mask enabled requires an empty point ID");
    const auto [mask_id,field]=geometry_mask_enabled_address(ref.field);
    require(field=="enabled","TYPE_MISMATCH","Only mask enabled accepts this Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.compositing.mask.has_value(),"MISSING_MASK","Object has no geometry mask: "+ref.object);
    require(object->second.compositing.mask->id==mask_id,"MISSING_MASK","Geometry mask ID is no longer installed: "+mask_id);
    return *object->second.compositing.mask;
}
Id point_edit_enabled_address(const std::string& field) {
    constexpr std::string_view prefix="point_edit.";
    constexpr std::string_view suffix=".enabled";
    require(field.starts_with(prefix)&&field.ends_with(suffix)&&field.size()>prefix.size()+suffix.size(),
        "INVALID_POINT_EDIT_REF","Point Edit enabled Ref must name one correction instance");
    const auto id=field.substr(prefix.size(),field.size()-prefix.size()-suffix.size());
    identity(id);
    return id;
}
const PointEdit& point_edit_enabled_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_POINT_EDIT_REF","Point Edit enabled requires an empty point ID");
    const auto point_edit_id=point_edit_enabled_address(ref.field);
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::path&&object->second.source.has_value(),
        "TYPE_MISMATCH","Point Edit enabled Ref must identify a procedural Path");
    require(object->second.point_edit.has_value(),"NO_POINT_EDIT","Primitive has no authored point corrections");
    require(object->second.point_edit->id==point_edit_id,"MISSING_POINT_EDIT",
        "Point Edit ID is no longer retained: "+point_edit_id);
    return *object->second.point_edit;
}
template<class DocumentType>
auto& gradient_enabled_source(DocumentType& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_GRADIENT_REF","Gradient enabled requires an empty point ID");
    require(ref.field.starts_with("op."),"INVALID_GRADIENT_REF","Gradient enabled Ref must identify a paint operation");
    const auto separator=ref.field.find('.',3);
    require(separator!=std::string::npos&&separator>3,"INVALID_GRADIENT_REF","Gradient enabled Ref requires an operation ID");
    const auto [operation_id,field]=operation_address(ref.field);
    identity(operation_id);
    constexpr std::string_view prefix="gradient.";
    constexpr std::string_view suffix=".enabled";
    require(field.starts_with(prefix)&&field.ends_with(suffix),"UNKNOWN_GRADIENT_PROPERTY",field);
    const auto gradient_length=field.size()-prefix.size()-suffix.size();
    require(gradient_length>0,"INVALID_GRADIENT_REF","Gradient enabled Ref requires a gradient ID");
    const auto gradient_id=field.substr(prefix.size(),gradient_length);
    require(gradient_id.find('.')==std::string::npos,"INVALID_GRADIENT_REF","Gradient enabled Ref has a malformed gradient ID");
    identity(gradient_id);
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    const auto found=std::find_if(object->second.stack.begin(),object->second.stack.end(),
        [&](const auto& operation){return operation.id==operation_id;});
    require(found!=object->second.stack.end(),"MISSING_OPERATION",operation_id);
    require(found->type=="nect.paint.fill"||found->type=="nect.paint.stroke",
        "INVALID_DOMAIN","Gradient enabled requires a Fill or Stroke operation");
    require(found->gradient.has_value(),"MISSING_GRADIENT",gradient_id);
    require(found->gradient->id==gradient_id,"MISSING_GRADIENT",gradient_id);
    return *found->gradient;
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
class TextWeightEvaluator {
    const Document& document_;
    std::map<Id,unsigned> values_;
    std::set<Id> active_;
    unsigned visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Text weight dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end())return found->second;
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Text weight dependency cycle");
        const auto& source=text_weight_source(document_,{id,"","text.weight"});
        require(source.weight>=1&&source.weight<=999,"OUT_OF_RANGE","Font weight must be 1..999");
        auto value=source.weight;
        if(source.weight_driver) {
            (void)text_weight_source(document_,source.weight_driver->link);
            value=visit(source.weight_driver->link.object,depth+1);
        }
        active_.erase(id);values_.emplace(id,value);return value;
    }
public:
    explicit TextWeightEvaluator(const Document& document):document_(document){}
    unsigned value(const Id& id){return visit(id,0);}
    std::map<Ref,unsigned> all() {
        std::map<Ref,unsigned> result;
        for(const auto& [id,object]:document_.objects)if(object.kind==Kind::text&&object.text)
            result.emplace(Ref{id,"","text.weight"},visit(id,0));
        return result;
    }
};
class TextContentEvaluator {
    const Document& document_;
    std::map<Id,std::string> values_;
    std::set<Id> active_;
    std::string visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Text content dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end())return found->second;
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Text content dependency cycle");
        const auto& source=text_content_source(document_,{id,"","text.content"});
        auto value=source.content;
        if(source.content_driver) {
            (void)text_content_source(document_,source.content_driver->link);
            value=visit(source.content_driver->link.object,depth+1);
        }
        require(value.size()<=32768,"LIMIT","Evaluated Text content exceeds 32768 UTF-8 bytes");
        text_utf8(value);
        active_.erase(id);values_.emplace(id,value);return value;
    }
public:
    explicit TextContentEvaluator(const Document& document):document_(document){}
    std::string value(const Id& id){return visit(id,0);}
    std::map<Ref,std::string> all() {
        std::map<Ref,std::string> result;
        for(const auto& [id,object]:document_.objects)if(object.kind==Kind::text&&object.text)
            result.emplace(Ref{id,"","text.content"},visit(id,0));
        return result;
    }
};
class TextFamilyEvaluator {
    const Document& document_;
    std::map<Id,std::string> values_;
    std::set<Id> active_;
    std::string visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Text family dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end())return found->second;
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Text family dependency cycle");
        const auto& source=text_family_source(document_,{id,"","text.family"});
        auto value=source.family;
        if(source.family_driver) {
            (void)text_family_source(document_,source.family_driver->link);
            value=visit(source.family_driver->link.object,depth+1);
        }
        require(!value.empty()&&value.size()<=1024,"LIMIT","Evaluated Text family must contain 1..1024 UTF-8 bytes");
        text_utf8(value);
        active_.erase(id);values_.emplace(id,value);return value;
    }
public:
    explicit TextFamilyEvaluator(const Document& document):document_(document){}
    std::string value(const Id& id){return visit(id,0);}
    std::map<Ref,std::string> all() {
        std::map<Ref,std::string> result;
        for(const auto& [id,object]:document_.objects)if(object.kind==Kind::text&&object.text)
            result.emplace(Ref{id,"","text.family"},visit(id,0));
        return result;
    }
};
class TextLocaleEvaluator {
    const Document& document_;
    std::map<Id,std::string> values_;
    std::set<Id> active_;
    std::string visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Text locale dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end())return found->second;
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Text locale dependency cycle");
        const auto& source=text_locale_source(document_,{id,"","text.locale"});
        auto value=source.locale;
        if(source.locale_driver) {
            (void)text_locale_source(document_,source.locale_driver->link);
            value=visit(source.locale_driver->link.object,depth+1);
        }
        require(!value.empty()&&value.size()<=128,"LIMIT","Evaluated Text locale must contain 1..128 UTF-8 bytes");
        text_utf8(value);
        active_.erase(id);values_.emplace(id,value);return value;
    }
public:
    explicit TextLocaleEvaluator(const Document& document):document_(document){}
    std::string value(const Id& id){return visit(id,0);}
    std::map<Ref,std::string> all() {
        std::map<Ref,std::string> result;
        for(const auto& [id,object]:document_.objects)if(object.kind==Kind::text&&object.text)
            result.emplace(Ref{id,"","text.locale"},visit(id,0));
        return result;
    }
};
class TextDirectionEvaluator {
    const Document& document_;
    std::map<Id,std::string> values_;
    std::set<Id> active_;
    std::string visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Text direction dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end())return found->second;
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Text direction dependency cycle");
        const auto& source=text_direction_source(document_,{id,"","text.direction"});
        auto value=source.direction;
        if(source.direction_driver) {
            (void)text_direction_source(document_,source.direction_driver->link);
            value=visit(source.direction_driver->link.object,depth+1);
        }
        require(value=="horizontal"||value=="vertical","UNSUPPORTED_TEXT_DIRECTION",value);
        active_.erase(id);values_.emplace(id,value);return value;
    }
public:
    explicit TextDirectionEvaluator(const Document& document):document_(document){}
    std::string value(const Id& id){return visit(id,0);}
    std::map<Ref,std::string> all() {
        std::map<Ref,std::string> result;
        for(const auto& [id,object]:document_.objects)if(object.kind==Kind::text&&object.text)
            result.emplace(Ref{id,"","text.direction"},visit(id,0));
        return result;
    }
};
class TextLayoutEvaluator {
    const Document& document_;
    std::map<Id,std::string> values_;
    std::set<Id> active_;
    std::string visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Text layout dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end())return found->second;
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Text layout dependency cycle");
        const auto& source=text_layout_source(document_,{id,"","text.layout"});
        auto value=source.layout;
        if(source.layout_driver) {
            (void)text_layout_source(document_,source.layout_driver->link);
            value=visit(source.layout_driver->link.object,depth+1);
        }
        require(value=="auto"||value=="frame","TEXT_LAYOUT_UNSUPPORTED","Text layout must be auto or frame");
        active_.erase(id);values_.emplace(id,value);return value;
    }
public:
    explicit TextLayoutEvaluator(const Document& document):document_(document){}
    std::string value(const Id& id){return visit(id,0);}
    std::map<Ref,std::string> all() {
        std::map<Ref,std::string> result;
        for(const auto& [id,object]:document_.objects)if(object.kind==Kind::text&&object.text)
            result.emplace(Ref{id,"","text.layout"},visit(id,0));
        return result;
    }
};
class TextAlignmentEvaluator {
    const Document& document_;
    std::map<Id,std::string> values_;
    std::set<Id> active_;
    std::string visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Text alignment dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end())return found->second;
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Text alignment dependency cycle");
        const auto& source=text_alignment_source(document_,{id,"","text.alignment"});
        auto value=source.alignment;
        if(source.alignment_driver) {
            (void)text_alignment_source(document_,source.alignment_driver->link);
            value=visit(source.alignment_driver->link.object,depth+1);
        }
        require(value=="start"||value=="center"||value=="end","UNSUPPORTED_TEXT_ALIGNMENT",value);
        active_.erase(id);values_.emplace(id,value);return value;
    }
public:
    explicit TextAlignmentEvaluator(const Document& document):document_(document){}
    std::string value(const Id& id){return visit(id,0);}
    std::map<Ref,std::string> all() {
        std::map<Ref,std::string> result;
        for(const auto& [id,object]:document_.objects)if(object.kind==Kind::text&&object.text)
            result.emplace(Ref{id,"","text.alignment"},visit(id,0));
        return result;
    }
};
class FillRuleEvaluator {
    const Document& document_;
    std::map<Ref,std::string> values_;
    std::set<Ref> active_;
    std::string visit(const Ref& ref,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Fill rule dependency depth limit 128");
        if(const auto found=values_.find(ref);found!=values_.end())return found->second;
        require(active_.insert(ref).second,"DEPENDENCY_CYCLE","Fill rule dependency cycle");
        const auto& operation=fill_rule_source(document_,ref);
        auto value=operation.fill_rule;
        if(operation.fill_rule_driver) {
            const auto& source=operation.fill_rule_driver->link;
            (void)fill_rule_source(document_,source);
            value=visit(source,depth+1);
        }
        require(value=="nonzero"||value=="evenodd","UNSUPPORTED_FILL_RULE",value);
        active_.erase(ref);values_.emplace(ref,value);return value;
    }
public:
    explicit FillRuleEvaluator(const Document& document):document_(document){}
    std::string value(const Ref& ref){return visit(ref,0);}
    std::map<Ref,std::string> all() {
        for(const auto& [id,object]:document_.objects)
            if(object.kind==Kind::path||object.kind==Kind::text)
                for(const auto& operation:object.stack)if(operation.type=="nect.paint.fill")
                    (void)visit(operation_ref(id,operation.id,"fill_rule"),0);
        return values_;
    }
};
struct OperationEnabledEvaluation { bool value=true; unsigned remaining_edges=0; };
class OperationEnabledEvaluator {
    const Document& document_;
    std::map<Id,Id> compositions_;
    std::map<Ref,OperationEnabledEvaluation> values_;
    std::set<Ref> active_;

    OperationEnabledEvaluation visit(const Ref& ref,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Operation enabled dependency depth limit 128");
        if(const auto found=values_.find(ref);found!=values_.end()) {
            require(depth+found->second.remaining_edges<=128,"DEPENDENCY_DEPTH","Operation enabled dependency depth limit 128");
            return found->second;
        }
        require(active_.insert(ref).second,"DEPENDENCY_CYCLE","Operation enabled dependency cycle");
        const auto& operation=operation_enabled_source(document_,ref);
        OperationEnabledEvaluation result{operation.enabled,0};
        if(operation.enabled_driver) {
            (void)operation_enabled_source(document_,*operation.enabled_driver);
            require(*operation.enabled_driver!=ref,"DEPENDENCY_CYCLE","Operation enabled cannot link to itself");
            const auto target_owner=compositions_.find(ref.object),source_owner=compositions_.find(operation.enabled_driver->object);
            require(target_owner!=compositions_.end()&&source_owner!=compositions_.end(),
                "ORPHAN_OBJECT","Operation enabled links require objects owned by a Composition");
            require(target_owner->second==source_owner->second,"CROSS_COMPOSITION",
                "Operation enabled links must stay in one Composition");
            const auto upstream=visit(*operation.enabled_driver,depth+1);
            result={upstream.value,upstream.remaining_edges+1};
        }
        require(depth+result.remaining_edges<=128,"DEPENDENCY_DEPTH","Operation enabled dependency depth limit 128");
        active_.erase(ref);
        values_.emplace(ref,result);
        return result;
    }
public:
    explicit OperationEnabledEvaluator(const Document& document):document_(document) {
        std::set<Id> active;
        std::function<void(const Id&,const Id&,unsigned)> own=[&](const Id& id,const Id& composition,unsigned depth) {
            require(depth<=128,"HIERARCHY_DEPTH","Hierarchy depth limit 128");
            require(document_.objects.contains(id),"MISSING_OBJECT",id);
            require(active.insert(id).second,"INVALID_HIERARCHY","Repeated owner or cycle: "+id);
            require(compositions_.emplace(id,composition).second,"INVALID_HIERARCHY","Repeated object owner: "+id);
            for(const auto& child:document_.objects.at(id).children)own(child,composition,depth+1);
            active.erase(id);
        };
        for(const auto& composition:document_.compositions)
            for(const auto& root:composition.roots)own(root,composition.id,0);
    }
    bool value(const Ref& ref) { (void)operation_enabled_source(document_,ref);return visit(ref,0).value; }
    std::map<Ref,bool> all() {
        std::map<Ref,bool> result;
        for(const auto& [id,object]:document_.objects)for(const auto& operation:object.stack) {
            const auto ref=operation_ref(id,operation.id,"enabled");
            result.emplace(ref,visit(ref,0).value);
        }
        return result;
    }
};
struct GradientEnabledEvaluation { bool value=true; unsigned remaining_edges=0; };
class GradientEnabledEvaluator {
    const Document& document_;
    std::map<Id,Id> compositions_;
    std::map<Ref,GradientEnabledEvaluation> values_;
    std::set<Ref> active_;

    GradientEnabledEvaluation visit(const Ref& ref,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Gradient enabled dependency depth limit 128");
        if(const auto found=values_.find(ref);found!=values_.end()) {
            require(depth+found->second.remaining_edges<=128,"DEPENDENCY_DEPTH","Gradient enabled dependency depth limit 128");
            return found->second;
        }
        require(active_.insert(ref).second,"DEPENDENCY_CYCLE","Gradient enabled dependency cycle");
        const auto& gradient=gradient_enabled_source(document_,ref);
        GradientEnabledEvaluation result{gradient.enabled,0};
        if(gradient.enabled_driver) {
            (void)gradient_enabled_source(document_,*gradient.enabled_driver);
            require(*gradient.enabled_driver!=ref,"DEPENDENCY_CYCLE","Gradient enabled cannot link to itself");
            const auto target_owner=compositions_.find(ref.object),source_owner=compositions_.find(gradient.enabled_driver->object);
            require(target_owner!=compositions_.end()&&source_owner!=compositions_.end(),
                "ORPHAN_OBJECT","Gradient enabled links require objects owned by a Composition");
            require(target_owner->second==source_owner->second,"CROSS_COMPOSITION",
                "Gradient enabled links must stay in one Composition");
            const auto upstream=visit(*gradient.enabled_driver,depth+1);
            result={upstream.value,upstream.remaining_edges+1};
        }
        require(depth+result.remaining_edges<=128,"DEPENDENCY_DEPTH","Gradient enabled dependency depth limit 128");
        active_.erase(ref);values_.emplace(ref,result);return result;
    }
public:
    explicit GradientEnabledEvaluator(const Document& document):document_(document) {
        std::set<Id> active;
        std::function<void(const Id&,const Id&,unsigned)> own=[&](const Id& id,const Id& composition,unsigned depth) {
            require(depth<=128,"HIERARCHY_DEPTH","Hierarchy depth limit 128");
            require(document_.objects.contains(id),"MISSING_OBJECT",id);
            require(active.insert(id).second,"INVALID_HIERARCHY","Repeated owner or cycle: "+id);
            require(compositions_.emplace(id,composition).second,"INVALID_HIERARCHY","Repeated object owner: "+id);
            for(const auto& child:document_.objects.at(id).children)own(child,composition,depth+1);
            active.erase(id);
        };
        for(const auto& composition:document_.compositions)
            for(const auto& root:composition.roots)own(root,composition.id,0);
    }
    bool value(const Ref& ref) { (void)gradient_enabled_source(document_,ref);return visit(ref,0).value; }
    std::map<Ref,bool> all() {
        std::map<Ref,bool> result;
        for(const auto& [id,object]:document_.objects)for(const auto& operation:object.stack)if(operation.gradient) {
            const auto ref=gradient_ref(id,operation.id,operation.gradient->id,"enabled");
            result.emplace(ref,visit(ref,0).value);
        }
        return result;
    }
    std::map<Ref,GradientEnabledProperty> states() {
        std::map<Ref,GradientEnabledProperty> result;
        for(const auto& [id,object]:document_.objects)for(const auto& operation:object.stack)if(operation.gradient) {
            const auto ref=gradient_ref(id,operation.id,operation.gradient->id,"enabled");
            const auto evaluated=visit(ref,0).value;
            result.emplace(ref,GradientEnabledProperty{operation.gradient->enabled,operation.gradient->enabled_driver,evaluated});
        }
        return result;
    }
};
struct GeometryMaskEnabledEvaluation { bool value=true; unsigned remaining_edges=0; };
class GeometryMaskEnabledEvaluator {
    const Document& document_;
    std::map<Id,Id> compositions_;
    std::map<Ref,GeometryMaskEnabledEvaluation> values_;
    std::set<Ref> active_;

    GeometryMaskEnabledEvaluation visit(const Ref& ref,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Geometry mask enabled dependency depth limit 128");
        if(const auto found=values_.find(ref);found!=values_.end()) {
            require(depth+found->second.remaining_edges<=128,"DEPENDENCY_DEPTH","Geometry mask enabled dependency depth limit 128");
            return found->second;
        }
        require(active_.insert(ref).second,"DEPENDENCY_CYCLE","Geometry mask enabled dependency cycle");
        const auto& mask=geometry_mask_enabled_source(document_,ref);
        GeometryMaskEnabledEvaluation result{mask.enabled,0};
        if(mask.enabled_driver) {
            (void)geometry_mask_enabled_source(document_,*mask.enabled_driver);
            require(*mask.enabled_driver!=ref,"DEPENDENCY_CYCLE","Geometry mask enabled cannot link to itself");
            const auto target_owner=compositions_.find(ref.object),source_owner=compositions_.find(mask.enabled_driver->object);
            require(target_owner!=compositions_.end()&&source_owner!=compositions_.end(),
                "ORPHAN_OBJECT","Geometry mask enabled links require objects owned by a Composition");
            require(target_owner->second==source_owner->second,"CROSS_COMPOSITION",
                "Geometry mask enabled links must stay in one Composition");
            const auto upstream=visit(*mask.enabled_driver,depth+1);
            result={upstream.value,upstream.remaining_edges+1};
        }
        require(depth+result.remaining_edges<=128,"DEPENDENCY_DEPTH","Geometry mask enabled dependency depth limit 128");
        active_.erase(ref);values_.emplace(ref,result);return result;
    }
public:
    explicit GeometryMaskEnabledEvaluator(const Document& document):document_(document) {
        std::set<Id> active;
        std::function<void(const Id&,const Id&,unsigned)> own=[&](const Id& id,const Id& composition,unsigned depth) {
            require(depth<=128,"HIERARCHY_DEPTH","Hierarchy depth limit 128");
            require(document_.objects.contains(id),"MISSING_OBJECT",id);
            require(active.insert(id).second,"INVALID_HIERARCHY","Repeated owner or cycle: "+id);
            require(compositions_.emplace(id,composition).second,"INVALID_HIERARCHY","Repeated object owner: "+id);
            for(const auto& child:document_.objects.at(id).children)own(child,composition,depth+1);
            active.erase(id);
        };
        for(const auto& composition:document_.compositions)
            for(const auto& root:composition.roots)own(root,composition.id,0);
    }
    bool value(const Ref& ref) { (void)geometry_mask_enabled_source(document_,ref);return visit(ref,0).value; }
    std::map<Ref,bool> all() {
        std::map<Ref,bool> result;
        for(const auto& [id,object]:document_.objects)if(object.compositing.mask) {
            const auto ref=geometry_mask_enabled_ref(id,object.compositing.mask->id);
            result.emplace(ref,visit(ref,0).value);
        }
        return result;
    }
};
struct PointEditEnabledEvaluation { bool value=true; unsigned remaining_edges=0; };
class PointEditEnabledEvaluator {
    const Document& document_;
    std::map<Id,Id> compositions_;
    std::map<Ref,PointEditEnabledEvaluation> values_;
    std::set<Ref> active_;

    PointEditEnabledEvaluation visit(const Ref& ref,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Point Edit enabled dependency depth limit 128");
        if(const auto found=values_.find(ref);found!=values_.end()) {
            require(depth+found->second.remaining_edges<=128,"DEPENDENCY_DEPTH","Point Edit enabled dependency depth limit 128");
            return found->second;
        }
        require(active_.insert(ref).second,"DEPENDENCY_CYCLE","Point Edit enabled dependency cycle");
        const auto& point_edit=point_edit_enabled_source(document_,ref);
        PointEditEnabledEvaluation result{point_edit.enabled,0};
        if(point_edit.enabled_driver) {
            (void)point_edit_enabled_source(document_,*point_edit.enabled_driver);
            require(*point_edit.enabled_driver!=ref,"DEPENDENCY_CYCLE","Point Edit enabled cannot link to itself");
            const auto target_owner=compositions_.find(ref.object),source_owner=compositions_.find(point_edit.enabled_driver->object);
            require(target_owner!=compositions_.end()&&source_owner!=compositions_.end(),
                "ORPHAN_OBJECT","Point Edit enabled links require objects owned by a Composition");
            require(target_owner->second==source_owner->second,"CROSS_COMPOSITION",
                "Point Edit enabled links must stay in one Composition");
            const auto upstream=visit(*point_edit.enabled_driver,depth+1);
            result={upstream.value,upstream.remaining_edges+1};
        }
        require(depth+result.remaining_edges<=128,"DEPENDENCY_DEPTH","Point Edit enabled dependency depth limit 128");
        active_.erase(ref);values_.emplace(ref,result);return result;
    }
public:
    explicit PointEditEnabledEvaluator(const Document& document):document_(document) {
        std::set<Id> active;
        std::function<void(const Id&,const Id&,unsigned)> own=[&](const Id& id,const Id& composition,unsigned depth) {
            require(depth<=128,"HIERARCHY_DEPTH","Hierarchy depth limit 128");
            require(document_.objects.contains(id),"MISSING_OBJECT",id);
            require(active.insert(id).second,"INVALID_HIERARCHY","Repeated owner or cycle: "+id);
            require(compositions_.emplace(id,composition).second,"INVALID_HIERARCHY","Repeated object owner: "+id);
            for(const auto& child:document_.objects.at(id).children)own(child,composition,depth+1);
            active.erase(id);
        };
        for(const auto& composition:document_.compositions)
            for(const auto& root:composition.roots)own(root,composition.id,0);
    }
    bool value(const Ref& ref) { (void)point_edit_enabled_source(document_,ref);return visit(ref,0).value; }
    std::map<Ref,bool> all() {
        std::map<Ref,bool> result;
        for(const auto& [id,object]:document_.objects)if(object.point_edit) {
            const auto ref=point_edit_enabled_ref(id,object.point_edit->id);
            result.emplace(ref,visit(ref,0).value);
        }
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
    require(!o.point_edit->enabled_driver||o.point_edit->enabled,"DRIVEN_PROPERTY",
        "Unlink Point Edit enabled before editing generated points when its authored literal is false");
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
    if(r.point.empty()&&r.field=="guide.position")
        throw Error("TYPE_MISMATCH","Guide positions use the dedicated Guide link commands");
    if(r.point.empty()&&r.field!="point_edit.enabled"&&r.field.starts_with("point_edit.")&&r.field.ends_with(".enabled")) {
        (void)point_edit_enabled_address(r.field);
        throw Error("USE_TYPED_COMMAND","Point Edit enabled uses the dedicated link_point_edit_enabled and unlink_point_edit_enabled commands");
    }
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
        if(r.field.starts_with("op.")||r.field.starts_with("stroke.")) {
            const auto address=r.field.starts_with("op.")?operation_address(r.field):
                std::pair{o.legacy_stroke,r.field.substr(7)};
            require(!address.first.empty(),"MISSING_REFERENCE",r.field);
            auto& op=operation(o,address.first);
            if(address.second.starts_with("gradient."))return gradient_property(op,address.second);
            require(op.parameters.contains(address.second),"MISSING_REFERENCE",r.field);
            return op.parameters.at(address.second);
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
    if(r.point.empty()&&r.field=="guide.position")return "du";
    if(r.point.empty()&&(r.field=="grid.columns"||r.field=="grid.rows"))return "unitless";
    if(r.point.empty()&&(r.field.starts_with("grid.")||r.field.starts_with("margin.")))return "du";
    if(r.point.empty()&&r.field=="text.italic")return "boolean";
    if(r.point.empty()&&r.field=="mask.enabled")return "boolean";
    if(r.point.empty()&&r.field.starts_with("mask.")&&r.field.ends_with(".enabled")) {
        (void)geometry_mask_enabled_address(r.field);return "boolean";
    }
    if(r.point.empty()&&r.field=="point_edit.enabled")return "boolean";
    if(r.point.empty()&&r.field.starts_with("point_edit.")&&r.field.ends_with(".enabled")) {
        (void)point_edit_enabled_address(r.field);return "boolean";
    }
    if(r.point.empty()&&r.field=="text.weight")return "unitless";
    if(r.field=="generator.points")return "scalar";
    if(r.field=="generator.rotation")return "degree";
    if(r.field.starts_with("color."))return "scalar";
    if(r.field.starts_with("text.")||r.field.starts_with("image.")||r.field=="artboard.width"||r.field=="artboard.height")return "du";
    if(r.field.starts_with("op.")) {
        const auto name=operation_address(r.field).second;
        if(name=="enabled")return "boolean";
        if(name.starts_with("gradient.")&&name.ends_with(".enabled"))return "boolean";
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
        if(name=="levels")require(v>=2&&v<=16&&std::floor(v)==v,"OUT_OF_RANGE","Posterize levels must be an integer from 2 to 16");
        if(name=="scale_x"||name=="scale_y")require(v>0&&v<=100,"OUT_OF_RANGE","Repeater scale must be positive and <=100");
        if(name=="offset")require(std::abs(v)<=1000,"OUT_OF_RANGE","Repeater offset magnitude limit 1000");
    }
}

std::map<Ref, const Scalar*> property_index(const Document& document,bool include_disabled=false,
    const std::map<Ref,bool>* evaluated_point_edit_enabled=nullptr) {
    std::map<Ref, const Scalar*> index;
    const auto computed_point_edit_enabled=include_disabled||evaluated_point_edit_enabled
        ?std::map<Ref,bool>{}:evaluate_point_edit_enableds(document);
    const auto& point_edit_enabled=evaluated_point_edit_enabled?*evaluated_point_edit_enabled:computed_point_edit_enabled;
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
            if(object.point_edit&&(include_disabled||point_edit_enabled.at(point_edit_enabled_ref(id,object.point_edit->id))))
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
    return evaluate_point_edit_enabled(d,point_edit_enabled_ref(r.object,edit->id))?"point_edit":"bypassed_point_edit";
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
    const bool corrections_enabled=o.point_edit&&evaluate_point_edit_enabled(
        d,point_edit_enabled_ref(object,o.point_edit->id));
    for(const auto& [ref,scalar]:property_index(d,true)) {
        if(!scalar||!driven(*scalar))continue;
        if(ref.point.empty()&&ref.field.starts_with("stroke."))continue;
        if(ref.object==object&&ref.field.starts_with("generator."))continue;
        if(ref.object==object&&!ref.point.empty()&&o.point_edit&&!corrections_enabled)continue;
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
    for(const auto& [id,object]:document.objects) {
        refs.push_back({id,"","object.visible"});
        refs.push_back({id,"","composite.isolated"});
        if(object.compositing.mask) {
            refs.push_back({id,"","mask.enabled"});
            refs.push_back(geometry_mask_enabled_ref(id,object.compositing.mask->id));
        }
        if(object.kind==Kind::path&&object.source&&object.point_edit) {
            refs.push_back({id,"","point_edit.enabled"});
            refs.push_back(point_edit_enabled_ref(id,object.point_edit->id));
        }
        if(object.kind==Kind::text&&object.text)
        {refs.push_back({id,"","text.italic"});refs.push_back({id,"","text.weight"});
            refs.push_back({id,"","text.content"});refs.push_back({id,"","text.family"});refs.push_back({id,"","text.locale"});
            refs.push_back({id,"","text.layout"});refs.push_back({id,"","text.direction"});refs.push_back({id,"","text.alignment"});}
        for(const auto& operation:object.stack) {
            refs.push_back(operation_ref(id,operation.id,"enabled"));
            if(operation.gradient)
                refs.push_back(gradient_ref(id,operation.id,operation.gradient->id,"enabled"));
            if((object.kind==Kind::path||object.kind==Kind::text)&&operation.type=="nect.paint.fill")
                refs.push_back(operation_ref(id,operation.id,"fill_rule"));
        }
    }
    for(const auto& composition:document.compositions)for(const auto& board:composition.artboards) {
        refs.push_back({board.id,"","artboard.width"});
        refs.push_back({board.id,"","artboard.height"});
        if(board.layout&&board.layout->margin)
            for(const auto* side:{"left","top","right","bottom"})refs.push_back({board.id,"",std::string("margin.")+side});
        if(board.layout&&board.layout->grid) {
            const auto& grid=*board.layout->grid;
            for(const auto* field:{"grid.bounds.x","grid.bounds.y","grid.bounds.width","grid.bounds.height",
                "grid.columns","grid.rows","grid.column_gutter","grid.row_gutter"})
                refs.push_back({grid.id,"",field});
        }
    }
    for(const auto& composition:document.compositions)for(const auto& guide:composition.guides)
        refs.push_back({guide.id,"","guide.position"});
    return refs;
}

ArtboardSizeProperty artboard_size_property(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_ARTBOARD_REF","Artboard dimensions require an empty point ID");
    require(ref.field=="artboard.width"||ref.field=="artboard.height","UNKNOWN_ARTBOARD_PROPERTY",ref.field);
    for(const auto& composition:document.compositions)for(const auto& board:composition.artboards)if(board.id==ref.object) {
        const bool width=ref.field=="artboard.width";
        std::optional<Ref> driver;
        std::optional<Expression> expression;
        std::string source_kind="literal";
        if(board.parent_size&&(width?board.parent_size->width:board.parent_size->height)) {
            driver=Ref{board.parent_size->artboard,"",ref.field};
            source_kind="parent_size";
        } else if(const auto& typed=width?board.width_driver:board.height_driver;typed) {
            if(const auto* link=std::get_if<Ref>(&typed->value)) {driver=*link;source_kind="link";}
            else {expression=std::get<Expression>(typed->value);source_kind="expression";}
        }
        const auto evaluated=evaluate_artboard(composition,board.id);
        return {width?board.width:board.height,std::move(driver),std::move(expression),std::move(source_kind),
            width?evaluated.width:evaluated.height};
    }
    throw Error("MISSING_ARTBOARD",ref.object);
}

ArtboardLayoutProperty artboard_layout_property(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_LAYOUT_REF","Artboard layout properties require an empty point ID");
    const bool margin=ref.field=="margin.left"||ref.field=="margin.top"||
        ref.field=="margin.right"||ref.field=="margin.bottom";
    const bool grid_bounds=ref.field=="grid.bounds.x"||ref.field=="grid.bounds.y"||
        ref.field=="grid.bounds.width"||ref.field=="grid.bounds.height";
    const bool grid_count=ref.field=="grid.columns"||ref.field=="grid.rows";
    const bool grid_gutter=ref.field=="grid.column_gutter"||ref.field=="grid.row_gutter";
    const bool grid=grid_bounds||grid_count||grid_gutter;
    require(margin||grid,"UNKNOWN_LAYOUT_PROPERTY",ref.field);

    for(const auto& composition:document.compositions)for(const auto& board:composition.artboards) {
        if(board.id==ref.object) {
            if(!margin)throw Error("TYPE_MISMATCH","Grid properties must use the stable Grid ID");
            if(!board.layout||!board.layout->margin)throw Error("MISSING_MARGIN",ref.object);
            const auto& value=*board.layout->margin;
            if(ref.field=="margin.left") {
                const bool driven=value.left_driver||value.left_expression;
                const auto evaluated=driven?evaluate_artboard(composition,board.id).layout->margin->left:value.left;
                return {value.left,value.left_driver,evaluated,value.left_expression,
                    value.left_driver?"link":value.left_expression?"expression":"literal"};
            }
            if(ref.field=="margin.top") {
                const bool driven=value.top_driver||value.top_expression;
                const auto evaluated=driven?evaluate_artboard(composition,board.id).layout->margin->top:value.top;
                return {value.top,value.top_driver,evaluated,value.top_expression,
                    value.top_driver?"link":value.top_expression?"expression":"literal"};
            }
            if(ref.field=="margin.right")return {value.right,{},value.right};
            return {value.bottom,{},value.bottom};
        }
        if(board.layout&&board.layout->grid&&board.layout->grid->id==ref.object) {
            if(!grid)throw Error("TYPE_MISMATCH","Margin properties must use the owning Artboard ID");
            const auto& value=*board.layout->grid;
            if(ref.field=="grid.bounds.x") {
                const bool driven=value.bounds_x_driver||value.bounds_x_expression;
                const auto evaluated=driven?evaluate_artboard(composition,board.id).layout->grid->bounds.x:value.bounds.x;
                return {value.bounds.x,value.bounds_x_driver,evaluated,value.bounds_x_expression,
                    value.bounds_x_driver?"link":value.bounds_x_expression?"expression":"literal"};
            }
            if(ref.field=="grid.bounds.y") {
                const bool driven=value.bounds_y_driver||value.bounds_y_expression;
                const auto evaluated=driven?evaluate_artboard(composition,board.id).layout->grid->bounds.y:value.bounds.y;
                return {value.bounds.y,value.bounds_y_driver,evaluated,value.bounds_y_expression,
                    value.bounds_y_driver?"link":value.bounds_y_expression?"expression":"literal"};
            }
            if(ref.field=="grid.bounds.width")return {value.bounds.width,{},value.bounds.width};
            if(ref.field=="grid.bounds.height")return {value.bounds.height,{},value.bounds.height};
            if(ref.field=="grid.columns")return {value.columns,{},value.columns};
            if(ref.field=="grid.rows")return {value.rows,{},value.rows};
            if(ref.field=="grid.column_gutter")return {value.column_gutter,{},value.column_gutter};
            return {value.row_gutter,{},value.row_gutter};
        }
    }
    for(const auto& composition:document.compositions) {
        if(composition.id==ref.object)throw Error("TYPE_MISMATCH","Layout property Ref must identify an Artboard or Grid");
        for(const auto& guide:composition.guides)if(guide.id==ref.object)
            throw Error("TYPE_MISMATCH","Layout property Ref must identify an Artboard or Grid");
    }
    if(document.objects.contains(ref.object)||document.named_colors.contains(ref.object)||
        document.raster_assets.contains(ref.object)||
        std::any_of(document.collections.begin(),document.collections.end(),[&](const auto& item){return item.id==ref.object;}))
        throw Error("TYPE_MISMATCH","Layout property Ref must identify an Artboard or Grid");
    throw Error(margin?"MISSING_ARTBOARD":"MISSING_GRID",ref.object);
}

GuidePositionProperty guide_position_property(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_GUIDE_REF","Guide position requires an empty point ID");
    require(ref.field=="guide.position","UNKNOWN_GUIDE_PROPERTY",ref.field);
    for(const auto& composition:document.compositions)for(const auto& guide:composition.guides)if(guide.id==ref.object)
        return {guide.position,guide.position_driver,guide.position_expression,
            guide.position_driver?"link":guide.position_expression?"expression":"literal",
            evaluate_guide_position(document,composition.id,guide.id)};
    if(document.objects.contains(ref.object)||document.named_colors.contains(ref.object))
        throw Error("TYPE_MISMATCH","Guide position Ref must identify a Guide");
    for(const auto& composition:document.compositions)for(const auto& board:composition.artboards)if(board.id==ref.object)
        throw Error("TYPE_MISMATCH","Guide position Ref must identify a Guide");
    throw Error("MISSING_GUIDE",ref.object);
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
TextWeightProperty text_weight_property(const Document& document,const Ref& ref) {
    const auto& source=text_weight_source(document,ref);
    return {source.weight,source.weight_driver,evaluate_text_weight(document,ref.object)};
}
unsigned evaluate_text_weight(const Document& document,const Id& object) {
    return TextWeightEvaluator(document).value(object);
}
TextSource evaluated_text_source(const Document& document,const Id& object) {
    const auto found=document.objects.find(object);
    require(found!=document.objects.end(),"MISSING_OBJECT",object);
    require(found->second.kind==Kind::text&&found->second.text.has_value(),"TYPE_MISMATCH",
        "Evaluated Text source requires a Text object: "+object);
    auto source=*found->second.text;
    source.content=evaluate_text_content(document,object);
    source.family=evaluate_text_family(document,object);
    source.locale=evaluate_text_locale(document,object);
    source.layout=evaluate_text_layout(document,object);
    source.direction=evaluate_text_direction(document,object);
    source.alignment=evaluate_text_alignment(document,object);
    source.italic=evaluate_text_italic(document,object);
    source.weight=evaluate_text_weight(document,object);
    return source;
}
std::map<Ref,unsigned> evaluate_text_weights(const Document& document) {
    return TextWeightEvaluator(document).all();
}
TextContentProperty text_content_property(const Document& document,const Ref& ref) {
    const auto& source=text_content_source(document,ref);
    return {source.content,source.content_driver,evaluate_text_content(document,ref.object)};
}
std::string evaluate_text_content(const Document& document,const Id& object) {
    return TextContentEvaluator(document).value(object);
}
std::map<Ref,std::string> evaluate_text_contents(const Document& document) {
    return TextContentEvaluator(document).all();
}
TextFamilyProperty text_family_property(const Document& document,const Ref& ref) {
    const auto& source=text_family_source(document,ref);
    return {source.family,source.family_driver,evaluate_text_family(document,ref.object)};
}
std::string evaluate_text_family(const Document& document,const Id& object) {
    return TextFamilyEvaluator(document).value(object);
}
std::map<Ref,std::string> evaluate_text_families(const Document& document) {
    return TextFamilyEvaluator(document).all();
}
TextLocaleProperty text_locale_property(const Document& document,const Ref& ref) {
    const auto& source=text_locale_source(document,ref);
    return {source.locale,source.locale_driver,evaluate_text_locale(document,ref.object)};
}
std::string evaluate_text_locale(const Document& document,const Id& object) {
    return TextLocaleEvaluator(document).value(object);
}
std::map<Ref,std::string> evaluate_text_locales(const Document& document) {
    return TextLocaleEvaluator(document).all();
}
TextDirectionProperty text_direction_property(const Document& document,const Ref& ref) {
    const auto& source=text_direction_source(document,ref);
    return {source.direction,source.direction_driver,evaluate_text_direction(document,ref.object)};
}
std::string evaluate_text_direction(const Document& document,const Id& object) {
    return TextDirectionEvaluator(document).value(object);
}
std::map<Ref,std::string> evaluate_text_directions(const Document& document) {
    return TextDirectionEvaluator(document).all();
}
TextLayoutProperty text_layout_property(const Document& document,const Ref& ref) {
    const auto& source=text_layout_source(document,ref);
    return {source.layout,source.layout_driver,evaluate_text_layout(document,ref.object)};
}
std::string evaluate_text_layout(const Document& document,const Id& object) {
    return TextLayoutEvaluator(document).value(object);
}
std::map<Ref,std::string> evaluate_text_layouts(const Document& document) {
    return TextLayoutEvaluator(document).all();
}
TextAlignmentProperty text_alignment_property(const Document& document,const Ref& ref) {
    const auto& source=text_alignment_source(document,ref);
    return {source.alignment,source.alignment_driver,evaluate_text_alignment(document,ref.object)};
}
std::string evaluate_text_alignment(const Document& document,const Id& object) {
    return TextAlignmentEvaluator(document).value(object);
}
std::map<Ref,std::string> evaluate_text_alignments(const Document& document) {
    return TextAlignmentEvaluator(document).all();
}
FillRuleProperty fill_rule_property(const Document& document,const Ref& ref) {
    const auto& operation=fill_rule_source(document,ref);
    return {operation.fill_rule,operation.fill_rule_driver,evaluate_fill_rule(document,ref)};
}
std::string evaluate_fill_rule(const Document& document,const Ref& ref) {
    return FillRuleEvaluator(document).value(ref);
}
std::map<Ref,std::string> evaluate_fill_rules(const Document& document) {
    return FillRuleEvaluator(document).all();
}
OperationEnabledProperty operation_enabled_state(const Document& document,const Ref& ref) {
    const auto& operation=operation_enabled_source(document,ref);
    return {operation.enabled,operation.enabled_driver,OperationEnabledEvaluator(document).value(ref)};
}
bool evaluate_operation_enabled(const Document& document,const Ref& ref) {
    return OperationEnabledEvaluator(document).value(ref);
}
std::map<Ref,bool> evaluate_operation_enableds(const Document& document) {
    return OperationEnabledEvaluator(document).all();
}
bool operation_enabled_property(const Document& document,const Ref& ref) {
    return operation_enabled_source(document,ref).enabled;
}
bool gradient_enabled_property(const Document& document,const Ref& ref) {
    return gradient_enabled_source(document,ref).enabled;
}
GradientEnabledProperty gradient_enabled_state(const Document& document,const Ref& ref) {
    const auto& gradient=gradient_enabled_source(document,ref);
    return {gradient.enabled,gradient.enabled_driver,evaluate_gradient_enabled(document,ref)};
}
std::map<Ref,GradientEnabledProperty> gradient_enabled_states(const Document& document) {
    return GradientEnabledEvaluator(document).states();
}
bool evaluate_gradient_enabled(const Document& document,const Ref& ref) {
    return GradientEnabledEvaluator(document).value(ref);
}
std::map<Ref,bool> evaluate_gradient_enableds(const Document& document) {
    return GradientEnabledEvaluator(document).all();
}
bool object_visibility_property(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_OBJECT_REF","Object visibility requires an empty point ID");
    require(ref.field=="object.visible","TYPE_MISMATCH","Only object.visible accepts this Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    return object->second.visible;
}
namespace {
const Object& visibility_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_OBJECT_REF","Object visibility requires an empty point ID");
    require(ref.field=="object.visible","TYPE_MISMATCH","Only object.visible accepts this Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    return object->second;
}
struct VisibilityEvaluation { bool value=true; unsigned remaining_edges=0; };
class ObjectVisibilityEvaluator {
    const Document& document_;
    std::map<Id,Id> compositions_;
    std::map<Id,VisibilityEvaluation> values_;
    std::set<Id> active_;

    VisibilityEvaluation visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Object visibility dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end()) {
            require(depth+found->second.remaining_edges<=128,"DEPENDENCY_DEPTH","Object visibility dependency depth limit 128");
            return found->second;
        }
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Object visibility dependency cycle");
        const auto& object=visibility_source(document_,{id,"","object.visible"});
        VisibilityEvaluation result{object.visible,0};
        if(object.visibility_driver) {
            const auto& source=visibility_source(document_,*object.visibility_driver);
            require(source.id!=id,"DEPENDENCY_CYCLE","Object visibility cannot link to itself");
            const auto target_owner=compositions_.find(id),source_owner=compositions_.find(source.id);
            require(target_owner!=compositions_.end()&&source_owner!=compositions_.end(),
                "ORPHAN_OBJECT","Visibility links require objects owned by a Composition");
            require(target_owner->second==source_owner->second,"CROSS_COMPOSITION",
                "Object visibility links must stay in one Composition");
            const auto upstream=visit(source.id,depth+1);
            result={upstream.value,upstream.remaining_edges+1};
        }
        require(depth+result.remaining_edges<=128,"DEPENDENCY_DEPTH","Object visibility dependency depth limit 128");
        active_.erase(id);
        values_.emplace(id,result);
        return result;
    }
public:
    explicit ObjectVisibilityEvaluator(const Document& document):document_(document) {
        std::set<Id> active;
        std::function<void(const Id&,const Id&,unsigned)> own=[&](const Id& id,const Id& composition,unsigned depth) {
            require(depth<=128,"HIERARCHY_DEPTH","Hierarchy depth limit 128");
            require(document_.objects.contains(id),"MISSING_OBJECT",id);
            require(active.insert(id).second,"INVALID_HIERARCHY","Repeated owner or cycle: "+id);
            require(compositions_.emplace(id,composition).second,"INVALID_HIERARCHY","Repeated object owner: "+id);
            for(const auto& child:document_.objects.at(id).children)own(child,composition,depth+1);
            active.erase(id);
        };
        for(const auto& composition:document_.compositions)
            for(const auto& root:composition.roots)own(root,composition.id,0);
    }
    bool value(const Id& id) { (void)visibility_source(document_,{id,"","object.visible"});return visit(id,0).value; }
    std::map<Id,bool> all() {
        std::map<Id,bool> result;
        for(const auto& [id,object]:document_.objects) { (void)object;result.emplace(id,visit(id,0).value); }
        return result;
    }
};
const Compositing& composite_isolation_source(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_OBJECT_REF","Composite isolation requires an empty point ID");
    require(ref.field=="composite.isolated","TYPE_MISMATCH","Only composite.isolated accepts this Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    return object->second.compositing;
}
struct CompositeIsolationEvaluation { bool value=false; unsigned remaining_edges=0; };
class CompositeIsolationEvaluator {
    const Document& document_;
    std::map<Id,Id> compositions_;
    std::map<Id,CompositeIsolationEvaluation> values_;
    std::set<Id> active_;

    CompositeIsolationEvaluation visit(const Id& id,unsigned depth) {
        require(depth<=128,"DEPENDENCY_DEPTH","Composite isolation dependency depth limit 128");
        if(const auto found=values_.find(id);found!=values_.end()) {
            require(depth+found->second.remaining_edges<=128,"DEPENDENCY_DEPTH","Composite isolation dependency depth limit 128");
            return found->second;
        }
        require(active_.insert(id).second,"DEPENDENCY_CYCLE","Composite isolation dependency cycle");
        const auto& compositing=composite_isolation_source(document_,{id,"","composite.isolated"});
        CompositeIsolationEvaluation result{compositing.isolated,0};
        if(compositing.isolated_driver) {
            const auto& source=composite_isolation_source(document_,*compositing.isolated_driver);
            (void)source;
            require(*compositing.isolated_driver!=Ref{id,"","composite.isolated"},
                "DEPENDENCY_CYCLE","Composite isolation cannot link to itself");
            const auto target_owner=compositions_.find(id),source_owner=compositions_.find(compositing.isolated_driver->object);
            require(target_owner!=compositions_.end()&&source_owner!=compositions_.end(),
                "ORPHAN_OBJECT","Composite isolation links require objects owned by a Composition");
            require(target_owner->second==source_owner->second,"CROSS_COMPOSITION",
                "Composite isolation links must stay in one Composition");
            const auto upstream=visit(compositing.isolated_driver->object,depth+1);
            result={upstream.value,upstream.remaining_edges+1};
        }
        require(depth+result.remaining_edges<=128,"DEPENDENCY_DEPTH","Composite isolation dependency depth limit 128");
        active_.erase(id);values_.emplace(id,result);return result;
    }
public:
    explicit CompositeIsolationEvaluator(const Document& document):document_(document) {
        std::set<Id> active;
        std::function<void(const Id&,const Id&,unsigned)> own=[&](const Id& id,const Id& composition,unsigned depth) {
            require(depth<=128,"HIERARCHY_DEPTH","Hierarchy depth limit 128");
            require(document_.objects.contains(id),"MISSING_OBJECT",id);
            require(active.insert(id).second,"INVALID_HIERARCHY","Repeated owner or cycle: "+id);
            require(compositions_.emplace(id,composition).second,"INVALID_HIERARCHY","Repeated object owner: "+id);
            for(const auto& child:document_.objects.at(id).children)own(child,composition,depth+1);
            active.erase(id);
        };
        for(const auto& composition:document_.compositions)
            for(const auto& root:composition.roots)own(root,composition.id,0);
    }
    bool value(const Ref& ref) { (void)composite_isolation_source(document_,ref);return visit(ref.object,0).value; }
    std::map<Id,bool> all() {
        std::map<Id,bool> result;
        for(const auto& [id,object]:document_.objects) { (void)object;result.emplace(id,visit(id,0).value); }
        return result;
    }
};
}
ObjectVisibilityProperty object_visibility_state(const Document& document,const Ref& ref) {
    const auto& object=visibility_source(document,ref);
    ObjectVisibilityEvaluator evaluator(document);
    return {object.visible,object.visibility_driver,evaluator.value(ref.object)};
}
bool evaluate_object_visibility(const Document& document,const Id& object) {
    return ObjectVisibilityEvaluator(document).value(object);
}
std::map<Id,bool> evaluate_object_visibilities(const Document& document) {
    return ObjectVisibilityEvaluator(document).all();
}
bool composite_isolated_property(const Document& document,const Ref& ref) {
    return composite_isolation_source(document,ref).isolated;
}
CompositeIsolationProperty composite_isolation_state(const Document& document,const Ref& ref) {
    const auto& compositing=composite_isolation_source(document,ref);
    return {compositing.isolated,compositing.isolated_driver,CompositeIsolationEvaluator(document).value(ref)};
}
bool evaluate_composite_isolation(const Document& document,const Ref& ref) {
    return CompositeIsolationEvaluator(document).value(ref);
}
std::map<Id,bool> evaluate_composite_isolations(const Document& document) {
    return CompositeIsolationEvaluator(document).all();
}
bool geometry_mask_enabled_property(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_OBJECT_REF","Geometry mask enabled requires an empty point ID");
    require(ref.field=="mask.enabled","TYPE_MISMATCH","Only mask.enabled accepts this Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.compositing.mask.has_value(),"MISSING_MASK","Object has no geometry mask: "+ref.object);
    return object->second.compositing.mask->enabled;
}
Ref geometry_mask_enabled_ref(const Id& object,const Id& mask) {
    identity(mask);
    return {object,"","mask."+mask+".enabled"};
}
GeometryMaskEnabledProperty geometry_mask_enabled_state(const Document& document,const Ref& ref) {
    const auto& mask=geometry_mask_enabled_source(document,ref);
    return {mask.enabled,mask.enabled_driver,GeometryMaskEnabledEvaluator(document).value(ref)};
}
bool evaluate_geometry_mask_enabled(const Document& document,const Ref& ref) {
    return GeometryMaskEnabledEvaluator(document).value(ref);
}
std::map<Ref,bool> evaluate_geometry_mask_enableds(const Document& document) {
    return GeometryMaskEnabledEvaluator(document).all();
}
bool point_edit_enabled_property(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_POINT_EDIT_REF","Point Edit enabled requires an empty point ID");
    require(ref.field=="point_edit.enabled","TYPE_MISMATCH","Only point_edit.enabled accepts this Ref");
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::path&&object->second.source.has_value(),
        "TYPE_MISMATCH","Point Edit enabled Ref must identify a procedural Path");
    require(object->second.point_edit.has_value(),"NO_POINT_EDIT","Primitive has no authored point corrections");
    return object->second.point_edit->enabled;
}
Ref point_edit_enabled_ref(const Id& object,const Id& point_edit) {
    identity(point_edit);
    return {object,"","point_edit."+point_edit+".enabled"};
}
PointEditEnabledProperty point_edit_enabled_state(const Document& document,const Ref& ref) {
    const auto& point_edit=point_edit_enabled_source(document,ref);
    return {point_edit.enabled,point_edit.enabled_driver,PointEditEnabledEvaluator(document).value(ref)};
}
bool evaluate_point_edit_enabled(const Document& document,const Ref& ref) {
    return PointEditEnabledEvaluator(document).value(ref);
}
std::map<Ref,bool> evaluate_point_edit_enableds(const Document& document) {
    return PointEditEnabledEvaluator(document).all();
}
bool is_text_readonly_field(const std::string& field) {
    return field=="text.content"||field=="text.family"||field=="text.locale"||
        field=="text.layout"||field=="text.direction"||field=="text.alignment";
}
TextPropertyValue text_readonly_property(const Document& document,const Ref& ref) {
    require(ref.point.empty(),"INVALID_TEXT_REF","Text source properties require an empty point ID");
    require(is_text_readonly_field(ref.field),"UNKNOWN_TEXT_PROPERTY","Unsupported Text source property: "+ref.field);
    const auto object=document.objects.find(ref.object);
    require(object!=document.objects.end(),"MISSING_REFERENCE",ref.object);
    require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text source property Ref must identify a Text object");
    const auto& source=*object->second.text;
    if(ref.field=="text.content")return {TextPropertyKind::string,source.content,{}};
    if(ref.field=="text.family")return {TextPropertyKind::string,source.family,{}};
    if(ref.field=="text.locale")return {TextPropertyKind::string,source.locale,{}};
    if(ref.field=="text.layout")return {TextPropertyKind::enumeration,source.layout,{"auto","frame"}};
    if(ref.field=="text.direction")return {TextPropertyKind::enumeration,source.direction,{"horizontal","vertical"}};
    return {TextPropertyKind::enumeration,source.alignment,{"start","center","end"}};
}
std::string property_unit(const Ref& r) { return unit(r); }

Ref resolve_name(const Document& d,const std::string& name,const Id& p,const std::string& f) {
    std::vector<Id> matches;
    if(f.starts_with("margin.")) {
        require(p.empty(),"INVALID_LAYOUT_REF","Margin properties require an empty point ID");
        require(f=="margin.left"||f=="margin.top"||f=="margin.right"||f=="margin.bottom",
            "UNKNOWN_LAYOUT_PROPERTY",f);
        for(const auto& composition:d.compositions)for(const auto& board:composition.artboards)
            if(board.name==name)matches.push_back(board.id);
        require(!matches.empty(),"MISSING_NAME","No matching Artboard: "+name);
        require(matches.size()==1,"AMBIGUOUS_NAME","Artboard name must resolve uniquely: "+name);
        Ref ref{matches.front(),p,f};(void)artboard_layout_property(d,ref);return ref;
    }
    if(f.starts_with("grid.")) {
        require(p.empty(),"INVALID_LAYOUT_REF","Grid properties require an empty point ID");
        require(f=="grid.bounds.x"||f=="grid.bounds.y"||f=="grid.bounds.width"||
            f=="grid.bounds.height"||f=="grid.columns"||f=="grid.rows"||
            f=="grid.column_gutter"||f=="grid.row_gutter","UNKNOWN_LAYOUT_PROPERTY",f);
        throw Error("GRID_ID_REQUIRED","Grid properties are discovered by their stable Grid ID");
    }
    if(f=="guide.position") {
        require(p.empty(),"INVALID_GUIDE_REF","Guide position requires an empty point ID");
        for(const auto& composition:d.compositions)for(const auto& guide:composition.guides)
            if(guide.name==name)matches.push_back(guide.id);
        require(!matches.empty(),"MISSING_NAME","No matching Guide: "+name);
        require(matches.size()==1,"AMBIGUOUS_NAME","Guide name must resolve uniquely: "+name);
        Ref ref{matches.front(),p,f};(void)guide_position_property(d,ref);return ref;
    }
    if(f.starts_with("artboard.")) {
        for(const auto& composition:d.compositions)for(const auto& board:composition.artboards)
            if(board.name==name)matches.push_back(board.id);
        require(!matches.empty(),"MISSING_NAME","No matching Artboard: "+name);
        require(matches.size()==1,"AMBIGUOUS_NAME","Artboard name must resolve uniquely: "+name);
        Ref ref{matches.front(),p,f};(void)artboard_size_property(d,ref);return ref;
    }
    for(const auto& [id,o]:d.objects) if(o.name==name) matches.push_back(id);
    for(const auto& [id,color]:d.named_colors)if(color.name==name)matches.push_back(id);
    require(!matches.empty(),"MISSING_NAME","No matching object: "+name);
    require(matches.size()==1,"AMBIGUOUS_NAME","Name must resolve to exactly one object: "+name);
    Ref r{matches.front(),p,f};
    if(f=="object.visible") {
        (void)object_visibility_property(d,r);
        return r;
    }
    if(f=="composite.isolated") {
        (void)composite_isolated_property(d,r);
        return r;
    }
    if(f=="mask.enabled") {
        (void)geometry_mask_enabled_property(d,r);
        return r;
    }
    if(f.starts_with("mask.")&&f.ends_with(".enabled")) {
        (void)geometry_mask_enabled_state(d,r);
        return r;
    }
    if(f=="point_edit.enabled") {
        (void)point_edit_enabled_property(d,r);
        return r;
    }
    if(f.starts_with("point_edit.")&&f.ends_with(".enabled")) {
        (void)point_edit_enabled_state(d,r);
        return r;
    }
    if(f.starts_with("op.")&&f.find(".gradient.")!=std::string::npos&&f.ends_with(".enabled")) {
        (void)gradient_enabled_property(d,r);
        return r;
    }
    if(f.starts_with("op.")&&f.ends_with(".enabled")) {
        (void)operation_enabled_property(d,r);
        return r;
    }
    if(f.starts_with("op.")&&f.ends_with(".fill_rule")) {
        (void)fill_rule_property(d,r);
        return r;
    }
    if(f.starts_with("text.")) {
        require(p.empty(),"INVALID_TEXT_REF","Text properties require an empty point ID");
        if(is_text_readonly_field(f)) {
            (void)text_readonly_property(d,r);
            return r;
        }
        if(f=="text.italic") {
            (void)text_italic_property(d,r);
            return r;
        }
        if(f=="text.weight") {
            (void)text_weight_property(d,r);
            return r;
        }
        const auto object=d.objects.find(r.object);
        require(object!=d.objects.end(),"TYPE_MISMATCH","Text property Ref must identify a Text object");
        require(object->second.kind==Kind::text&&object->second.text.has_value(),"TYPE_MISMATCH","Text property Ref must identify a Text object");
        require(object->second.text->parameters.contains(f.substr(5)),"UNKNOWN_TEXT_PROPERTY","Unsupported Text source property: "+f);
        (void)property(d,r);
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
    const auto point_edit_enabled_values=evaluate_point_edit_enableds(d);
    const auto index = requested?std::map<Ref,const Scalar*>{}:property_index(d,false,&point_edit_enabled_values);
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
            else if(const auto& edit=object->second.point_edit;edit&&
                point_edit_enabled_values.at(point_edit_enabled_ref(object->first,edit->id))) {
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

namespace {
struct ArtboardSizeEvaluation {double value=0;std::size_t remaining_edges=0;};

Ref artboard_size_ref(const Id& artboard,bool width) {
    return {artboard,"",width?"artboard.width":"artboard.height"};
}
bool artboard_size_ref(const Ref& ref) {
    return ref.point.empty()&&(ref.field=="artboard.width"||ref.field=="artboard.height");
}
class ArtboardSizeEvaluator {
public:
    explicit ArtboardSizeEvaluator(const Composition& composition):composition_(composition) {
        for(const auto& board:composition_.artboards)boards_.emplace(board.id,&board);
    }

    double value(const Ref& ref) {
        require(artboard_size_ref(ref),"INVALID_ARTBOARD_REF","Artboard dimensions require an empty-point width or height Ref");
        validate_parent_chain(ref.object);
        std::set<Ref> active;
        return visit(ref,0,active).value;
    }

private:
    const Composition& composition_;
    std::map<Id,const Artboard*> boards_;
    std::map<Ref,ArtboardSizeEvaluation> cache_;
    ExpressionCache expressions_;

    void validate_parent_chain(const Id& id) const {
        std::set<Id> seen;auto next=id;std::size_t depth=0;
        for(;;) {
            require(seen.insert(next).second,"ARTBOARD_CYCLE","Parent Artboard relationship cycle at "+next);
            require(depth++<256,"ARTBOARD_DEPTH","Artboard parent depth limit 256");
            const auto found=boards_.find(next);
            require(found!=boards_.end(),"MISSING_ARTBOARD",next);
            if(!found->second->parent_size)break;
            next=found->second->parent_size->artboard;
        }
    }

    ArtboardSizeEvaluation visit(const Ref& ref,std::size_t depth,std::set<Ref>& active) {
        require(artboard_size_ref(ref),"INVALID_ARTBOARD_REF","Artboard expressions may reference only Artboard width and height");
        require(depth<256,"ARTBOARD_DEPTH","Artboard size dependency depth limit 256");
        if(const auto found=cache_.find(ref);found!=cache_.end()) {
            require(depth+found->second.remaining_edges<256,"ARTBOARD_DEPTH","Artboard size dependency depth limit 256");
            return found->second;
        }
        require(active.insert(ref).second,"ARTBOARD_CYCLE","Artboard size dependency cycle at "+ref.object+"/"+ref.field);
        const auto found=boards_.find(ref.object);
        require(found!=boards_.end(),"MISSING_ARTBOARD",ref.object);
        const auto& board=*found->second;
        const bool width=ref.field=="artboard.width";
        const double literal=width?board.width:board.height;
        const auto& driver=width?board.width_driver:board.height_driver;
        const bool parent_driven=board.parent_size&&(width?board.parent_size->width:board.parent_size->height);
        require(!(parent_driven&&driver),"ARTBOARD_SOURCE_CONFLICT","An Artboard dimension has more than one source");

        ArtboardSizeEvaluation result{literal,0};
        if(parent_driven) {
            const auto source=artboard_size_ref(board.parent_size->artboard,width);
            const auto upstream=visit(source,depth+1,active);
            result={upstream.value,upstream.remaining_edges+1};
        } else if(driver) {
            if(const auto* link=std::get_if<Ref>(&driver->value)) {
                require(artboard_size_ref(*link),"INVALID_ARTBOARD_REF","Artboard links require an empty-point Artboard width or height Ref");
                require(*link!=ref,"ARTBOARD_SELF_LINK","An Artboard dimension cannot link to itself");
                const auto upstream=visit(*link,depth+1,active);
                result={upstream.value,upstream.remaining_edges+1};
            } else {
                const auto& expression=std::get<Expression>(driver->value);
                const auto& compiled=compiled_expression(expressions_,expression);
                validate_expression_unit(compiled,"du");
                std::size_t max_edges=0;
                for(const auto& source:expression_dependencies(compiled)) {
                    require(artboard_size_ref(source),"ARTBOARD_EXPRESSION_TYPE",
                        "Artboard size expressions may reference only Artboard width and height in the same Composition");
                    const auto upstream=visit(source,depth+1,active);
                    max_edges=std::max(max_edges,upstream.remaining_edges+1);
                }
                result.value=evaluate_expression(compiled,"du",[&](const Ref& source) {
                    return visit(source,depth+1,active).value;
                });
                result.remaining_edges=max_edges;
            }
        }
        require(std::isfinite(result.value)&&result.value>0&&result.value<=1e7,
            "ARTBOARD_SIZE_RANGE","Evaluated Artboard dimensions must be in (0,10000000]");
        active.erase(ref);
        return cache_.emplace(ref,result).first->second;
    }
};

struct ArtboardDimensionLocation {Composition* composition=nullptr;Artboard* board=nullptr;bool width=false;};
ArtboardDimensionLocation artboard_dimension_location(Document& document,const Ref& ref,const char* role) {
    require(ref.point.empty(),"INVALID_ARTBOARD_REF",std::string("Artboard ")+role+" requires an empty point ID");
    require(ref.field=="artboard.width"||ref.field=="artboard.height","UNKNOWN_ARTBOARD_PROPERTY",ref.field);
    identity(ref.object);
    for(auto& composition:document.compositions)for(auto& board:composition.artboards)
        if(board.id==ref.object)return {&composition,&board,ref.field=="artboard.width"};
    bool another_kind=document.objects.contains(ref.object)||document.named_colors.contains(ref.object);
    for(const auto& composition:document.compositions) {
        another_kind=another_kind||composition.id==ref.object;
        another_kind=another_kind||std::any_of(composition.guides.begin(),composition.guides.end(),[&](const auto& guide){return guide.id==ref.object;});
    }
    if(another_kind)throw Error("TYPE_MISMATCH",std::string("Artboard ")+role+" must identify an Artboard: "+ref.object);
    throw Error("MISSING_ARTBOARD",ref.object);
}
struct MarginLeftLocation {Composition* composition=nullptr;Artboard* board=nullptr;};
MarginLeftLocation margin_left_location(Document& document,const Ref& ref,const char* role) {
    require(ref.point.empty(),"INVALID_LAYOUT_REF",std::string("Margin left ")+role+" requires an empty point ID");
    require(ref.field=="margin.left","UNKNOWN_LAYOUT_PROPERTY",ref.field);
    identity(ref.object);
    for(auto& composition:document.compositions)for(auto& board:composition.artboards)
        if(board.id==ref.object)return {&composition,&board};
    bool another_kind=document.objects.contains(ref.object)||document.named_colors.contains(ref.object);
    for(const auto& composition:document.compositions) {
        another_kind=another_kind||composition.id==ref.object;
        another_kind=another_kind||std::any_of(composition.guides.begin(),composition.guides.end(),
            [&](const Guide& guide){return guide.id==ref.object;});
        another_kind=another_kind||std::any_of(composition.artboards.begin(),composition.artboards.end(),
            [&](const Artboard& board){return board.layout&&board.layout->grid&&board.layout->grid->id==ref.object;});
    }
    if(another_kind)throw Error("TYPE_MISMATCH",std::string("Margin left ")+role+" must identify an Artboard: "+ref.object);
    throw Error("MISSING_ARTBOARD",ref.object);
}
MarginLeftLocation margin_top_location(Document& document,const Ref& ref,const char* role) {
    require(ref.point.empty(),"INVALID_LAYOUT_REF",std::string("Margin top ")+role+" requires an empty point ID");
    require(ref.field=="margin.top","UNKNOWN_LAYOUT_PROPERTY",ref.field);
    identity(ref.object);
    for(auto& composition:document.compositions)for(auto& board:composition.artboards)
        if(board.id==ref.object)return {&composition,&board};
    bool another_kind=document.objects.contains(ref.object)||document.named_colors.contains(ref.object);
    for(const auto& composition:document.compositions) {
        another_kind=another_kind||composition.id==ref.object;
        another_kind=another_kind||std::any_of(composition.guides.begin(),composition.guides.end(),
            [&](const Guide& guide){return guide.id==ref.object;});
        another_kind=another_kind||std::any_of(composition.artboards.begin(),composition.artboards.end(),
            [&](const Artboard& board){return board.layout&&board.layout->grid&&board.layout->grid->id==ref.object;});
    }
    if(another_kind)throw Error("TYPE_MISMATCH",std::string("Margin top ")+role+" must identify an Artboard: "+ref.object);
    throw Error("MISSING_ARTBOARD",ref.object);
}
struct GridBoundsXLocation {Composition* composition=nullptr;Artboard* board=nullptr;Grid* grid=nullptr;};
using GridBoundsYLocation=GridBoundsXLocation;
CompiledExpression compile_grid_bounds_x_expression(const Expression& expression);
CompiledExpression compile_grid_bounds_y_expression(const Expression& expression);
CompiledExpression compile_margin_left_expression(const Expression& expression);
CompiledExpression compile_margin_top_expression(const Expression& expression);
GridBoundsXLocation grid_bounds_x_location(Document& document,const Ref& ref,const char* role) {
    require(ref.point.empty(),"INVALID_LAYOUT_REF",std::string("Grid bounds x ")+role+" requires an empty point ID");
    require(ref.field=="grid.bounds.x","UNKNOWN_LAYOUT_PROPERTY",ref.field);
    identity(ref.object);
    for(auto& composition:document.compositions)for(auto& board:composition.artboards)
        if(board.layout&&board.layout->grid&&board.layout->grid->id==ref.object)
            return {&composition,&board,&*board.layout->grid};
    for(const auto& composition:document.compositions) {
        if(composition.id==ref.object||std::any_of(composition.artboards.begin(),composition.artboards.end(),
            [&](const Artboard& board){return board.id==ref.object;})||
            std::any_of(composition.guides.begin(),composition.guides.end(),
                [&](const Guide& guide){return guide.id==ref.object;}))
            throw Error("TYPE_MISMATCH",std::string("Grid bounds x ")+role+" must identify a stable Grid ID: "+ref.object);
    }
    if(document.objects.contains(ref.object)||document.named_colors.contains(ref.object)||document.raster_assets.contains(ref.object)||
        std::any_of(document.collections.begin(),document.collections.end(),[&](const auto& item){return item.id==ref.object;}))
        throw Error("TYPE_MISMATCH",std::string("Grid bounds x ")+role+" must identify a stable Grid ID: "+ref.object);
    throw Error("MISSING_GRID",ref.object);
}
GridBoundsYLocation grid_bounds_y_location(Document& document,const Ref& ref,const char* role) {
    require(ref.point.empty(),"INVALID_LAYOUT_REF",std::string("Grid bounds y ")+role+" requires an empty point ID");
    require(ref.field=="grid.bounds.y","UNKNOWN_LAYOUT_PROPERTY",ref.field);
    identity(ref.object);
    for(auto& composition:document.compositions)for(auto& board:composition.artboards)
        if(board.layout&&board.layout->grid&&board.layout->grid->id==ref.object)
            return {&composition,&board,&*board.layout->grid};
    for(const auto& composition:document.compositions) {
        if(composition.id==ref.object||std::any_of(composition.artboards.begin(),composition.artboards.end(),
            [&](const Artboard& board){return board.id==ref.object;})||
            std::any_of(composition.guides.begin(),composition.guides.end(),
                [&](const Guide& guide){return guide.id==ref.object;}))
            throw Error("TYPE_MISMATCH",std::string("Grid bounds y ")+role+" must identify a stable Grid ID: "+ref.object);
    }
    if(document.objects.contains(ref.object)||document.named_colors.contains(ref.object)||document.raster_assets.contains(ref.object)||
        std::any_of(document.collections.begin(),document.collections.end(),[&](const auto& item){return item.id==ref.object;}))
        throw Error("TYPE_MISMATCH",std::string("Grid bounds y ")+role+" must identify a stable Grid ID: "+ref.object);
    throw Error("MISSING_GRID",ref.object);
}
void edit_grid_bounds_x(Document& document,const LinkGridBoundsX& command) {
    require(command.target.point.empty()&&command.target.field=="grid.bounds.x","INVALID_LAYOUT_REF",
        "Grid bounds x link target must be an empty-point grid.bounds.x Ref");
    require(artboard_size_ref(command.source),"INVALID_ARTBOARD_REF",
        "Grid bounds x link source must be an empty-point Artboard width or height Ref");
    const auto target=grid_bounds_x_location(document,command.target,"link target");
    const auto source=artboard_dimension_location(document,command.source,"link source");
    require(target.composition==source.composition,"WRONG_COMPOSITION","Grid bounds x links must stay within one Composition");
    require(target.board->id!=source.board->id,"GRID_SELF_LINK","Grid bounds x cannot depend on its owning Artboard size");
    auto& slot=target.grid->bounds_x_driver;
    const bool same_link=slot&&*slot==command.source&&!target.grid->bounds_x_expression;
    require((!slot&&!target.grid->bounds_x_expression)||same_link||command.replace_driver,"DRIVEN_GRID_BOUNDS_X",
        "Replacing a Grid bounds x source requires replace_driver=true");
    slot=command.source;
    target.grid->bounds_x_expression.reset();
}
void edit_grid_bounds_x(Document& document,const SetGridBoundsXExpression& command) {
    require(command.target.point.empty()&&command.target.field=="grid.bounds.x","INVALID_LAYOUT_REF",
        "Grid bounds x expression target must be an empty-point grid.bounds.x Ref");
    const auto target=grid_bounds_x_location(document,command.target,"expression target");
    const auto compiled=compile_grid_bounds_x_expression(command.expression);
    for(const auto& source_ref:expression_dependencies(compiled)) {
        const auto source=artboard_dimension_location(document,source_ref,"expression source");
        require(target.composition==source.composition,"WRONG_COMPOSITION",
            "Grid bounds x expressions must stay within one Composition");
        require(target.board->id!=source.board->id,"GRID_SELF_LINK",
            "Grid bounds x cannot depend on its owning Artboard size");
    }
    const bool same_expression=!target.grid->bounds_x_driver&&target.grid->bounds_x_expression==command.expression;
    require((!target.grid->bounds_x_driver&&!target.grid->bounds_x_expression)||same_expression||command.replace_driver,
        "DRIVEN_GRID_BOUNDS_X","Replacing a Grid bounds x source requires replace_driver=true");
    target.grid->bounds_x_driver.reset();
    target.grid->bounds_x_expression=command.expression;
}
void edit_grid_bounds_x(Document& document,const UnlinkGridBoundsX& command) {
    require(command.target.point.empty()&&command.target.field=="grid.bounds.x","INVALID_LAYOUT_REF",
        "Grid bounds x unlink target must be an empty-point grid.bounds.x Ref");
    const auto target=grid_bounds_x_location(document,command.target,"unlink target");
    require(target.grid->bounds_x_driver.has_value()||target.grid->bounds_x_expression.has_value(),
        "GRID_BOUNDS_X_NOT_LINKED","Grid bounds x has no link or expression to unlink");
    const auto resolved=evaluate_artboard(*target.composition,target.board->id);
    target.grid->bounds.x=resolved.layout->grid->bounds.x;
    target.grid->bounds_x_driver.reset();
    target.grid->bounds_x_expression.reset();
}
void edit_grid_bounds_y(Document& document,const LinkGridBoundsY& command) {
    require(command.target.point.empty()&&command.target.field=="grid.bounds.y","INVALID_LAYOUT_REF",
        "Grid bounds y link target must be an empty-point grid.bounds.y Ref");
    require(artboard_size_ref(command.source),"INVALID_ARTBOARD_REF",
        "Grid bounds y link source must be an empty-point Artboard width or height Ref");
    const auto target=grid_bounds_y_location(document,command.target,"link target");
    const auto source=artboard_dimension_location(document,command.source,"link source");
    require(target.composition==source.composition,"WRONG_COMPOSITION","Grid bounds y links must stay within one Composition");
    require(target.board->id!=source.board->id,"GRID_SELF_LINK","Grid bounds y cannot depend on its owning Artboard size");
    auto& slot=target.grid->bounds_y_driver;
    const bool same_link=slot&&*slot==command.source&&!target.grid->bounds_y_expression;
    require((!slot&&!target.grid->bounds_y_expression)||same_link||command.replace_driver,"DRIVEN_GRID_BOUNDS_Y",
        "Replacing a Grid bounds y source requires replace_driver=true");
    slot=command.source;
    target.grid->bounds_y_expression.reset();
}
void edit_grid_bounds_y(Document& document,const SetGridBoundsYExpression& command) {
    require(command.target.point.empty()&&command.target.field=="grid.bounds.y","INVALID_LAYOUT_REF",
        "Grid bounds y expression target must be an empty-point grid.bounds.y Ref");
    const auto target=grid_bounds_y_location(document,command.target,"expression target");
    const auto compiled=compile_grid_bounds_y_expression(command.expression);
    for(const auto& source_ref:expression_dependencies(compiled)) {
        const auto source=artboard_dimension_location(document,source_ref,"expression source");
        require(target.composition==source.composition,"WRONG_COMPOSITION",
            "Grid bounds y expressions must stay within one Composition");
        require(target.board->id!=source.board->id,"GRID_SELF_LINK",
            "Grid bounds y cannot depend on its owning Artboard size");
    }
    const bool same_expression=!target.grid->bounds_y_driver&&target.grid->bounds_y_expression==command.expression;
    require((!target.grid->bounds_y_driver&&!target.grid->bounds_y_expression)||same_expression||command.replace_driver,
        "DRIVEN_GRID_BOUNDS_Y","Replacing a Grid bounds y source requires replace_driver=true");
    target.grid->bounds_y_driver.reset();
    target.grid->bounds_y_expression=command.expression;
}
void edit_grid_bounds_y(Document& document,const UnlinkGridBoundsY& command) {
    require(command.target.point.empty()&&command.target.field=="grid.bounds.y","INVALID_LAYOUT_REF",
        "Grid bounds y unlink target must be an empty-point grid.bounds.y Ref");
    const auto target=grid_bounds_y_location(document,command.target,"unlink target");
    require(target.grid->bounds_y_driver.has_value()||target.grid->bounds_y_expression.has_value(),"GRID_BOUNDS_Y_NOT_LINKED",
        "Grid bounds y has no Artboard size link or expression to unlink");
    const auto resolved=evaluate_artboard(*target.composition,target.board->id);
    target.grid->bounds.y=resolved.layout->grid->bounds.y;
    target.grid->bounds_y_driver.reset();
    target.grid->bounds_y_expression.reset();
}
void edit_margin_left(Document& document,const LinkMarginLeft& command) {
    require(command.target.point.empty()&&command.target.field=="margin.left","INVALID_LAYOUT_REF",
        "Margin left link target must be an empty-point margin.left Ref");
    require(artboard_size_ref(command.source),"INVALID_ARTBOARD_REF",
        "Margin left link source must be an empty-point Artboard width or height Ref");
    const auto target=margin_left_location(document,command.target,"link target");
    const auto source=artboard_dimension_location(document,command.source,"link source");
    require(target.composition==source.composition,"WRONG_COMPOSITION","Margin left links must stay within one Composition");
    require(target.board->id!=source.board->id,"ARTBOARD_SELF_LINK","Margin left cannot depend on its own Artboard size");
    require(target.board->layout&&target.board->layout->margin,"MISSING_MARGIN",target.board->id);
    auto& margin=*target.board->layout->margin;
    auto& slot=margin.left_driver;
    const bool same_link=slot&&*slot==command.source&&!margin.left_expression;
    require((!slot&&!margin.left_expression)||same_link||command.replace_driver,"DRIVEN_MARGIN_LEFT",
        "Replacing a Margin left source requires replace_driver=true");
    slot=command.source;
    margin.left_expression.reset();
}
void edit_margin_left(Document& document,const SetMarginLeftExpression& command) {
    require(command.target.point.empty()&&command.target.field=="margin.left","INVALID_LAYOUT_REF",
        "Margin left expression target must be an empty-point margin.left Ref");
    const auto target=margin_left_location(document,command.target,"expression target");
    require(target.board->layout&&target.board->layout->margin,"MISSING_MARGIN",target.board->id);
    const auto compiled=compile_margin_left_expression(command.expression);
    for(const auto& source_ref:expression_dependencies(compiled)) {
        const auto source=artboard_dimension_location(document,source_ref,"expression source");
        require(target.composition==source.composition,"WRONG_COMPOSITION",
            "Margin left expressions must stay within one Composition");
        require(target.board->id!=source.board->id,"ARTBOARD_SELF_LINK",
            "Margin left cannot depend on its own Artboard size");
    }
    auto& margin=*target.board->layout->margin;
    const bool same_expression=!margin.left_driver&&margin.left_expression==command.expression;
    require((!margin.left_driver&&!margin.left_expression)||same_expression||command.replace_driver,
        "DRIVEN_MARGIN_LEFT","Replacing a Margin left source requires replace_driver=true");
    margin.left_driver.reset();
    margin.left_expression=command.expression;
}
void edit_margin_left(Document& document,const UnlinkMarginLeft& command) {
    require(command.target.point.empty()&&command.target.field=="margin.left","INVALID_LAYOUT_REF",
        "Margin left unlink target must be an empty-point margin.left Ref");
    const auto target=margin_left_location(document,command.target,"unlink target");
    require(target.board->layout&&target.board->layout->margin,"MISSING_MARGIN",target.board->id);
    auto& margin=*target.board->layout->margin;
    require(margin.left_driver.has_value()||margin.left_expression.has_value(),"MARGIN_LEFT_NOT_LINKED",
        "Margin left has no Artboard size link or expression to unlink");
    const auto resolved=evaluate_artboard(*target.composition,target.board->id);
    margin.left=resolved.layout->margin->left;
    margin.left_driver.reset();
    margin.left_expression.reset();
}
void edit_margin_top(Document& document,const LinkMarginTop& command) {
    require(command.target.point.empty()&&command.target.field=="margin.top","INVALID_LAYOUT_REF",
        "Margin top link target must be an empty-point margin.top Ref");
    require(artboard_size_ref(command.source),"INVALID_ARTBOARD_REF",
        "Margin top link source must be an empty-point Artboard width or height Ref");
    const auto target=margin_top_location(document,command.target,"link target");
    const auto source=artboard_dimension_location(document,command.source,"link source");
    require(target.composition==source.composition,"WRONG_COMPOSITION","Margin top links must stay within one Composition");
    require(target.board->id!=source.board->id,"ARTBOARD_SELF_LINK","Margin top cannot depend on its own Artboard size");
    require(target.board->layout&&target.board->layout->margin,"MISSING_MARGIN",target.board->id);
    auto& margin=*target.board->layout->margin;
    const bool same_link=margin.top_driver&&*margin.top_driver==command.source&&!margin.top_expression;
    require((!margin.top_driver&&!margin.top_expression)||same_link||command.replace_driver,"DRIVEN_MARGIN_TOP",
        "Replacing a Margin top source requires replace_driver=true");
    margin.top_driver=command.source;
    margin.top_expression.reset();
}
void edit_margin_top(Document& document,const SetMarginTopExpression& command) {
    require(command.target.point.empty()&&command.target.field=="margin.top","INVALID_LAYOUT_REF",
        "Margin top expression target must be an empty-point margin.top Ref");
    const auto target=margin_top_location(document,command.target,"expression target");
    require(target.board->layout&&target.board->layout->margin,"MISSING_MARGIN",target.board->id);
    const auto compiled=compile_margin_top_expression(command.expression);
    for(const auto& source_ref:expression_dependencies(compiled)) {
        const auto source=artboard_dimension_location(document,source_ref,"expression source");
        require(target.composition==source.composition,"WRONG_COMPOSITION",
            "Margin top expressions must stay within one Composition");
        require(target.board->id!=source.board->id,"ARTBOARD_SELF_LINK",
            "Margin top cannot depend on its own Artboard size");
    }
    auto& margin=*target.board->layout->margin;
    const bool same_expression=!margin.top_driver&&margin.top_expression==command.expression;
    require((!margin.top_driver&&!margin.top_expression)||same_expression||command.replace_driver,
        "DRIVEN_MARGIN_TOP","Replacing a Margin top source requires replace_driver=true");
    margin.top_driver.reset();
    margin.top_expression=command.expression;
}
void edit_margin_top(Document& document,const UnlinkMarginTop& command) {
    require(command.target.point.empty()&&command.target.field=="margin.top","INVALID_LAYOUT_REF",
        "Margin top unlink target must be an empty-point margin.top Ref");
    const auto target=margin_top_location(document,command.target,"unlink target");
    require(target.board->layout&&target.board->layout->margin,"MISSING_MARGIN",target.board->id);
    auto& margin=*target.board->layout->margin;
    require(margin.top_driver.has_value()||margin.top_expression.has_value(),"MARGIN_TOP_NOT_LINKED",
        "Margin top has no Artboard size link or expression to unlink");
    margin.top=evaluate_artboard(*target.composition,target.board->id).layout->margin->top;
    margin.top_driver.reset();
    margin.top_expression.reset();
}
CompiledExpression compile_artboard_size_expression(const Expression& expression) {
    const auto compiled=compile_expression(expression);validate_expression_unit(compiled,"du");
    for(const auto& source:expression_dependencies(compiled)) {
        require(artboard_size_ref(source),"ARTBOARD_EXPRESSION_TYPE",
            "Artboard size expressions may reference only Artboard width and height");
    }
    return compiled;
}
CompiledExpression compile_grid_bounds_x_expression(const Expression& expression) {
    const auto compiled=compile_expression(expression);validate_expression_unit(compiled,"du");
    for(const auto& source:expression_dependencies(compiled))
        require(artboard_size_ref(source),"GRID_BOUNDS_X_EXPRESSION_TYPE",
            "Grid bounds x expressions may reference only empty-point Artboard width and height properties");
    return compiled;
}
CompiledExpression compile_grid_bounds_y_expression(const Expression& expression) {
    const auto compiled=compile_expression(expression);validate_expression_unit(compiled,"du");
    for(const auto& source:expression_dependencies(compiled))
        require(artboard_size_ref(source),"GRID_BOUNDS_Y_EXPRESSION_TYPE",
            "Grid bounds y expressions may reference only empty-point Artboard width and height properties");
    return compiled;
}
CompiledExpression compile_margin_left_expression(const Expression& expression) {
    const auto compiled=compile_expression(expression);validate_expression_unit(compiled,"du");
    for(const auto& source:expression_dependencies(compiled))
        require(artboard_size_ref(source),"MARGIN_LEFT_EXPRESSION_TYPE",
            "Margin left expressions may reference only empty-point Artboard width and height properties");
    return compiled;
}
CompiledExpression compile_margin_top_expression(const Expression& expression) {
    const auto compiled=compile_expression(expression);validate_expression_unit(compiled,"du");
    for(const auto& source:expression_dependencies(compiled))
        require(artboard_size_ref(source),"MARGIN_TOP_EXPRESSION_TYPE",
            "Margin top expressions may reference only empty-point Artboard width and height properties");
    return compiled;
}
bool artboard_references_id(const Artboard& board,const Id& id) {
    if(board.parent_size&&board.parent_size->artboard==id)return true;
    if(board.layout&&board.layout->margin&&board.layout->margin->left_driver&&
        board.layout->margin->left_driver->object==id)return true;
    if(board.layout&&board.layout->margin&&board.layout->margin->top_driver&&
        board.layout->margin->top_driver->object==id)return true;
    if(board.layout&&board.layout->margin&&board.layout->margin->top_expression)
        for(const auto& source:expression_dependencies(*board.layout->margin->top_expression))
            if(source.object==id)return true;
    if(board.layout&&board.layout->margin&&board.layout->margin->left_expression)
        for(const auto& source:expression_dependencies(*board.layout->margin->left_expression))
            if(source.object==id)return true;
    if(board.layout&&board.layout->grid&&board.layout->grid->bounds_x_driver&&
        board.layout->grid->bounds_x_driver->object==id)return true;
    if(board.layout&&board.layout->grid&&board.layout->grid->bounds_y_driver&&
        board.layout->grid->bounds_y_driver->object==id)return true;
    if(board.layout&&board.layout->grid&&board.layout->grid->bounds_y_expression)
        for(const auto& source:expression_dependencies(*board.layout->grid->bounds_y_expression))
            if(source.object==id)return true;
    if(board.layout&&board.layout->grid&&board.layout->grid->bounds_x_expression)
        for(const auto& source:expression_dependencies(*board.layout->grid->bounds_x_expression))
            if(source.object==id)return true;
    for(const auto* driver:{&board.width_driver,&board.height_driver})if(*driver) {
        if(const auto* link=std::get_if<Ref>(&(**driver).value)) {
            if(link->object==id)return true;
        } else {
            for(const auto& source:expression_dependencies(std::get<Expression>((**driver).value)))
                if(source.object==id)return true;
        }
    }
    return false;
}
void preserve_margin_left_source(const Margin* existing,Margin* incoming) {
    const bool existing_source=existing&&(existing->left_driver||existing->left_expression);
    const bool incoming_source=incoming&&(incoming->left_driver||incoming->left_expression);
    if(existing_source) {
        require(incoming,"DRIVEN_MARGIN_LEFT","Unlink Margin left before clearing its Margin");
        require(incoming->left==existing->left,"DRIVEN_MARGIN_LEFT",
            "Unlink Margin left before changing its authored literal");
        require(!incoming->left_driver||incoming->left_driver==existing->left_driver,
            "MARGIN_DRIVER_SMUGGLING","Use a dedicated Margin left command to change its source");
        require(!incoming->left_expression||incoming->left_expression==existing->left_expression,
            "MARGIN_DRIVER_SMUGGLING","Use set_margin_left_expression to change its source");
        incoming->left_driver=existing->left_driver;
        incoming->left_expression=existing->left_expression;
    } else require(!incoming_source,"MARGIN_DRIVER_SMUGGLING",
        "Create Margin left sources with link_margin_left or set_margin_left_expression");
}
void preserve_margin_top_source(const Margin* existing,Margin* incoming) {
    const bool existing_source=existing&&(existing->top_driver||existing->top_expression);
    const bool incoming_source=incoming&&(incoming->top_driver||incoming->top_expression);
    if(existing_source) {
        require(incoming,"DRIVEN_MARGIN_TOP","Unlink Margin top before clearing its Margin");
        require(incoming->top==existing->top,"DRIVEN_MARGIN_TOP",
            "Unlink Margin top before changing its authored literal");
        require(!incoming->top_driver||incoming->top_driver==existing->top_driver,
            "MARGIN_DRIVER_SMUGGLING","Use link_margin_top to change the Margin top source");
        require(!incoming->top_expression||incoming->top_expression==existing->top_expression,
            "MARGIN_DRIVER_SMUGGLING","Use set_margin_top_expression to change the Margin top source");
        incoming->top_driver=existing->top_driver;
        incoming->top_expression=existing->top_expression;
    } else require(!incoming_source,"MARGIN_DRIVER_SMUGGLING",
        "Create Margin top sources with link_margin_top or set_margin_top_expression");
}
void preserve_grid_bounds_x_source(const Grid* existing,Grid* incoming) {
    const bool existing_source=existing&&(existing->bounds_x_driver||existing->bounds_x_expression);
    const bool incoming_source=incoming&&(incoming->bounds_x_driver||incoming->bounds_x_expression);
    if(existing_source) {
        require(incoming,"DRIVEN_GRID_BOUNDS_X","Unlink Grid bounds x before clearing its Grid");
        require(incoming->id==existing->id,"DRIVEN_GRID_BOUNDS_X",
            "Unlink Grid bounds x before replacing the stable Grid ID");
        require(incoming->bounds.x==existing->bounds.x,"DRIVEN_GRID_BOUNDS_X",
            "Unlink Grid bounds x before changing its authored literal");
        require(!incoming->bounds_x_driver||incoming->bounds_x_driver==existing->bounds_x_driver,
            "GRID_DRIVER_SMUGGLING","Use the dedicated Grid bounds x command to change its source");
        require(!incoming->bounds_x_expression||incoming->bounds_x_expression==existing->bounds_x_expression,
            "GRID_DRIVER_SMUGGLING","Use set_grid_bounds_x_expression to change the Grid bounds x source");
        incoming->bounds_x_driver=existing->bounds_x_driver;
        incoming->bounds_x_expression=existing->bounds_x_expression;
    } else {
        require(!incoming_source,"GRID_DRIVER_SMUGGLING",
            "Create Grid bounds x sources with link_grid_bounds_x or set_grid_bounds_x_expression");
    }
}
void preserve_grid_bounds_y_source(const Grid* existing,Grid* incoming) {
    const bool existing_source=existing&&(existing->bounds_y_driver||existing->bounds_y_expression);
    const bool incoming_source=incoming&&(incoming->bounds_y_driver||incoming->bounds_y_expression);
    if(existing_source) {
        require(incoming,"DRIVEN_GRID_BOUNDS_Y","Unlink Grid bounds y before clearing its Grid");
        require(incoming->id==existing->id,"DRIVEN_GRID_BOUNDS_Y",
            "Unlink Grid bounds y before replacing the stable Grid ID");
        require(incoming->bounds.y==existing->bounds.y,"DRIVEN_GRID_BOUNDS_Y",
            "Unlink Grid bounds y before changing its authored literal");
        require(!incoming->bounds_y_driver||incoming->bounds_y_driver==existing->bounds_y_driver,
            "GRID_DRIVER_SMUGGLING","Use the dedicated Grid bounds y command to change its source");
        require(!incoming->bounds_y_expression||incoming->bounds_y_expression==existing->bounds_y_expression,
            "GRID_DRIVER_SMUGGLING","Use set_grid_bounds_y_expression to change the Grid bounds y source");
        incoming->bounds_y_driver=existing->bounds_y_driver;
        incoming->bounds_y_expression=existing->bounds_y_expression;
    } else require(!incoming_source,"GRID_DRIVER_SMUGGLING",
        "Create Grid bounds y sources with link_grid_bounds_y or set_grid_bounds_y_expression");
}
}

Artboard evaluate_artboard(const Composition& composition,const Id& artboard) {
    const auto found=std::find_if(composition.artboards.begin(),composition.artboards.end(),[&](const auto& item){return item.id==artboard;});
    require(found!=composition.artboards.end(),"MISSING_ARTBOARD",artboard);
    ArtboardSizeEvaluator evaluator(composition);
    ExpressionCache expressions;
    auto result=*found;
    result.width=evaluator.value(artboard_size_ref(artboard,true));
    result.height=evaluator.value(artboard_size_ref(artboard,false));
    if(result.layout&&result.layout->margin) {
        auto& margin=*result.layout->margin;
        require(!(margin.left_driver&&margin.left_expression),"MARGIN_SOURCE_CONFLICT",
            "Margin left may have only one active source");
        require(!(margin.top_driver&&margin.top_expression),"MARGIN_SOURCE_CONFLICT",
            "Margin top may have only one active source");
        if(margin.left_driver) {
            require(margin.left_driver->object!=artboard,"ARTBOARD_SELF_LINK","Margin left cannot depend on its own Artboard size");
            margin.left=evaluator.value(*margin.left_driver);
        } else if(margin.left_expression) {
            const auto& compiled=compiled_expression(expressions,*margin.left_expression);
            validate_expression_unit(compiled,"du");
            for(const auto& source:expression_dependencies(compiled)) {
                require(artboard_size_ref(source),"MARGIN_LEFT_EXPRESSION_TYPE",
                    "Margin left expressions may reference only empty-point Artboard width and height properties");
                require(source.object!=artboard,"ARTBOARD_SELF_LINK","Margin left cannot depend on its own Artboard size");
                (void)evaluator.value(source);
            }
            margin.left=evaluate_expression(compiled,"du",[&](const Ref& source){return evaluator.value(source);});
        }
        if(margin.top_driver) {
            require(artboard_size_ref(*margin.top_driver),"INVALID_ARTBOARD_REF",
                "Margin top source must be an empty-point Artboard width or height Ref");
            require(margin.top_driver->object!=artboard,"ARTBOARD_SELF_LINK","Margin top cannot depend on its own Artboard size");
            margin.top=evaluator.value(*margin.top_driver);
        } else if(margin.top_expression) {
            const auto& compiled=compiled_expression(expressions,*margin.top_expression);
            validate_expression_unit(compiled,"du");
            for(const auto& source:expression_dependencies(compiled)) {
                require(artboard_size_ref(source),"MARGIN_TOP_EXPRESSION_TYPE",
                    "Margin top expressions may reference only empty-point Artboard width and height properties");
                require(source.object!=artboard,"ARTBOARD_SELF_LINK","Margin top cannot depend on its own Artboard size");
                (void)evaluator.value(source);
            }
            margin.top=evaluate_expression(compiled,"du",[&](const Ref& source){return evaluator.value(source);});
        }
    }
    if(result.layout&&result.layout->grid) {
        auto& grid=*result.layout->grid;
        require(!(grid.bounds_x_driver&&grid.bounds_x_expression),"GRID_SOURCE_CONFLICT",
            "Grid bounds x may have only one active source");
        if(grid.bounds_x_driver) {
            const auto& driver=*grid.bounds_x_driver;
            require(driver.object!=artboard,"GRID_SELF_LINK","Grid bounds x cannot depend on its owning Artboard size");
            grid.bounds.x=evaluator.value(driver);
        } else if(grid.bounds_x_expression) {
            const auto& compiled=compiled_expression(expressions,*grid.bounds_x_expression);
            validate_expression_unit(compiled,"du");
            for(const auto& source:expression_dependencies(compiled)) {
                require(artboard_size_ref(source),"GRID_BOUNDS_X_EXPRESSION_TYPE",
                    "Grid bounds x expressions may reference only empty-point Artboard width and height properties");
                require(source.object!=artboard,"GRID_SELF_LINK","Grid bounds x cannot depend on its owning Artboard size");
                (void)evaluator.value(source);
            }
            grid.bounds.x=evaluate_expression(compiled,"du",[&](const Ref& source){return evaluator.value(source);});
        }
        if(grid.bounds_y_driver) {
            require(grid.bounds_y_driver->object!=artboard,"GRID_SELF_LINK",
                "Grid bounds y cannot depend on its owning Artboard size");
            grid.bounds.y=evaluator.value(*grid.bounds_y_driver);
        } else if(grid.bounds_y_expression) {
            const auto& compiled=compiled_expression(expressions,*grid.bounds_y_expression);
            validate_expression_unit(compiled,"du");
            for(const auto& source:expression_dependencies(compiled)) {
                require(artboard_size_ref(source),"GRID_BOUNDS_Y_EXPRESSION_TYPE",
                    "Grid bounds y expressions may reference only empty-point Artboard width and height properties");
                require(source.object!=artboard,"GRID_SELF_LINK","Grid bounds y cannot depend on its owning Artboard size");
                (void)evaluator.value(source);
            }
            grid.bounds.y=evaluate_expression(compiled,"du",[&](const Ref& source){return evaluator.value(source);});
        }
    }
    return result;
}

namespace {
struct GuideLocation { const Composition* composition; const Guide* guide; };
struct GuideEvaluation { double value; std::size_t remaining_edges; };

class GuidePositionEvaluator {
public:
    explicit GuidePositionEvaluator(const Document& document):document_(document) {
        for(const auto& composition:document_.compositions) {
            compositions_.emplace(composition.id,&composition);
            for(const auto& guide:composition.guides)guides_.emplace(guide.id,GuideLocation{&composition,&guide});
            for(const auto& board:composition.artboards)artboards_.insert(board.id);
        }
    }

    double evaluate(const Id& composition_id,const Id& guide_id) {
        require(compositions_.contains(composition_id),"MISSING_COMPOSITION",composition_id);
        const auto found=guides_.find(guide_id);
        if(found==guides_.end())missing(guide_id,"Guide position target");
        require(found->second.composition->id==composition_id,"WRONG_COMPOSITION",guide_id);
        std::set<Id> active;
        return visit(guide_id,composition_id,0,active).value;
    }

private:
    const Document& document_;
    std::map<Id,const Composition*> compositions_;
    std::map<Id,GuideLocation> guides_;
    std::set<Id> artboards_;
    std::map<Id,GuideEvaluation> cache_;
    ExpressionCache expressions_;

    [[noreturn]] void missing(const Id& id,const char* role) const {
        if(document_.objects.contains(id)||document_.named_colors.contains(id)||artboards_.contains(id))
            throw Error("TYPE_MISMATCH",std::string(role)+" must identify a Guide: "+id);
        throw Error("MISSING_GUIDE",id);
    }

    GuideEvaluation visit(const Id& current,const Id& composition_id,std::size_t depth,std::set<Id>& active) {
        const auto found=guides_.find(current);
        if(found==guides_.end())missing(current,"Guide position link source");
        const auto& location=found->second;
        require(location.composition->id==composition_id,"WRONG_COMPOSITION",current);
        if(const auto cached=cache_.find(current);cached!=cache_.end()) {
            require(depth+cached->second.remaining_edges<256,"GUIDE_DEPTH","Guide position dependency depth exceeds 256");
            return cached->second;
        }
        require(active.insert(current).second,"GUIDE_CYCLE","Guide position dependency cycle at "+current);
        GuideEvaluation result{location.guide->position,0};
        require(!(location.guide->position_driver&&location.guide->position_expression),
            "GUIDE_SOURCE_CONFLICT","A Guide position can have one active source");
        auto resolve_source=[&](const Ref& source) {
            require(source.point.empty()&&source.field=="guide.position","GUIDE_EXPRESSION_TYPE",
                "Guide position dependencies may reference only guide.position");
            identity(source.object);
            require(source.object!=current,"GUIDE_SELF_LINK","A Guide position cannot reference itself");
            const auto source_location=guides_.find(source.object);
            if(source_location==guides_.end())missing(source.object,"Guide position source");
            require(source_location->second.composition->id==composition_id,"WRONG_COMPOSITION",
                "Guide position dependencies must stay in one Composition");
            require(source_location->second.guide->axis==location.guide->axis,"GUIDE_AXIS_MISMATCH",
                "Guide position dependencies must use the same axis");
            return visit(source.object,composition_id,depth+1,active);
        };
        if(location.guide->position_driver) {
            const auto& source=*location.guide->position_driver;
            require(source.point.empty()&&source.field=="guide.position","INVALID_GUIDE_REF",
                "Guide position links must target another guide.position Ref");
            const auto source_value=resolve_source(source);
            result.value=source_value.value;
            result.remaining_edges=source_value.remaining_edges+1;
        } else if(location.guide->position_expression) {
            const auto& compiled=compiled_expression(expressions_,*location.guide->position_expression);
            std::size_t max_edges=0;
            for(const auto& source:expression_dependencies(compiled)) {
                const auto upstream=resolve_source(source);
                max_edges=std::max(max_edges,upstream.remaining_edges+1);
            }
            validate_expression_unit(compiled,"du");
            result.value=evaluate_expression(compiled,"du",[&](const Ref& source) {
                return resolve_source(source).value;
            });
            result.remaining_edges=max_edges;
        }
        require(depth+result.remaining_edges<256,"GUIDE_DEPTH","Guide position dependency depth exceeds 256");
        require(std::isfinite(result.value)&&std::abs(result.value)<=1e9,"GUIDE_POSITION_RANGE",
            "Evaluated Guide position must be finite and within [-1e9,1e9] du");
        active.erase(current);
        cache_.emplace(current,result);
        return result;
    }
};
}

double evaluate_guide_position(const Document& document,const Id& composition,const Id& guide) {
    return GuidePositionEvaluator(document).evaluate(composition,guide);
}

std::map<Id,double> evaluate_guide_positions(const Document& document,const Id& composition_id) {
    const auto composition=std::find_if(document.compositions.begin(),document.compositions.end(),
        [&](const auto& candidate){return candidate.id==composition_id;});
    require(composition!=document.compositions.end(),"MISSING_COMPOSITION",composition_id);
    GuidePositionEvaluator evaluator(document);
    std::map<Id,double> result;
    for(const auto& guide:composition->guides)result.emplace(guide.id,evaluator.evaluate(composition_id,guide.id));
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
            if(a.layout&&a.layout->margin) {
                const auto& margin=*a.layout->margin;
                require(!(margin.left_driver&&margin.left_expression),"MARGIN_SOURCE_CONFLICT",
                    "Margin left may have only one active source");
                const auto validate_margin_source=[&](const Ref& source,bool top=false) {
                    const auto property=top?"Margin top":"Margin left";
                    require(artboard_size_ref(source),"INVALID_ARTBOARD_REF",
                        std::string(property)+" sources require an empty-point Artboard width or height Ref");
                    require(source.object!=a.id,"ARTBOARD_SELF_LINK",std::string(property)+" cannot depend on its own Artboard size");
                    const bool in_composition=std::any_of(comp.artboards.begin(),comp.artboards.end(),
                        [&](const Artboard& candidate){return candidate.id==source.object;});
                    if(!in_composition) {
                        const bool elsewhere=std::any_of(d.compositions.begin(),d.compositions.end(),[&](const Composition& other) {
                            return other.id!=comp.id&&std::any_of(other.artboards.begin(),other.artboards.end(),
                                [&](const Artboard& candidate){return candidate.id==source.object;});
                        });
                        if(elsewhere)throw Error("WRONG_COMPOSITION",std::string(property)+" sources must stay within one Composition");
                        const bool another_kind=d.objects.contains(source.object)||d.named_colors.contains(source.object)||
                            std::any_of(d.compositions.begin(),d.compositions.end(),[&](const Composition& other) {
                                return other.id==source.object||std::any_of(other.guides.begin(),other.guides.end(),
                                    [&](const Guide& guide){return guide.id==source.object;});
                            });
                        if(another_kind)throw Error("TYPE_MISMATCH",std::string(property)+" source must identify an Artboard: "+source.object);
                        throw Error("MISSING_ARTBOARD",source.object);
                    }
                };
                if(margin.left_driver)validate_margin_source(*margin.left_driver);
                if(margin.left_expression) {
                    const auto compiled=compile_margin_left_expression(*margin.left_expression);
                    for(const auto& source:expression_dependencies(compiled))validate_margin_source(source);
                }
                if(margin.top_driver)validate_margin_source(*margin.top_driver,true);
                if(margin.top_expression) {
                    const auto compiled=compile_margin_top_expression(*margin.top_expression);
                    for(const auto& source:expression_dependencies(compiled))validate_margin_source(source,true);
                }
            }
            if(a.layout&&a.layout->grid) {
                const auto& grid=*a.layout->grid;
                require(!(grid.bounds_x_driver&&grid.bounds_x_expression),"GRID_SOURCE_CONFLICT",
                    "Grid bounds x may have only one active source");
                require(!(grid.bounds_y_driver&&grid.bounds_y_expression),"GRID_SOURCE_CONFLICT",
                    "Grid bounds y may have only one active source");
                const auto validate_grid_source=[&](const Ref& source) {
                    require(artboard_size_ref(source),"INVALID_ARTBOARD_REF",
                        "Grid sources require an empty-point Artboard width or height Ref");
                    require(source.object!=a.id,"GRID_SELF_LINK","Grid cannot depend on its owning Artboard size");
                    const bool in_composition=std::any_of(comp.artboards.begin(),comp.artboards.end(),
                        [&](const Artboard& candidate){return candidate.id==source.object;});
                    if(!in_composition) {
                        const bool elsewhere=std::any_of(d.compositions.begin(),d.compositions.end(),[&](const Composition& other) {
                            return other.id!=comp.id&&std::any_of(other.artboards.begin(),other.artboards.end(),
                                [&](const Artboard& candidate){return candidate.id==source.object;});
                        });
                        if(elsewhere)throw Error("WRONG_COMPOSITION","Grid sources must stay within one Composition");
                        const bool another_kind=d.objects.contains(source.object)||d.named_colors.contains(source.object)||
                            std::any_of(d.compositions.begin(),d.compositions.end(),[&](const Composition& other) {
                                return other.id==source.object||std::any_of(other.guides.begin(),other.guides.end(),
                                    [&](const Guide& guide){return guide.id==source.object;});
                            });
                        if(another_kind)throw Error("TYPE_MISMATCH","Grid source must identify an Artboard: "+source.object);
                        throw Error("MISSING_ARTBOARD",source.object);
                    }
                };
                if(grid.bounds_x_driver)validate_grid_source(*grid.bounds_x_driver);
                if(grid.bounds_x_expression) {
                    const auto compiled=compile_grid_bounds_x_expression(*grid.bounds_x_expression);
                    for(const auto& source:expression_dependencies(compiled))validate_grid_source(source);
                }
                if(grid.bounds_y_driver)validate_grid_source(*grid.bounds_y_driver);
                if(grid.bounds_y_expression) {
                    const auto compiled=compile_grid_bounds_y_expression(*grid.bounds_y_expression);
                    for(const auto& source:expression_dependencies(compiled))validate_grid_source(source);
                }
            }
            const auto evaluated=evaluate_artboard(comp,a.id);
            if(a.layout) {
                const auto& layout=*a.layout;
                require(layout.margin||layout.grid,"INVALID_LAYOUT","Artboard layout must contain Margin, Grid or both: "+a.id);
                if(layout.margin) {
                    const auto& margin=*layout.margin;
                    require(std::isfinite(margin.left)&&std::isfinite(margin.top)&&std::isfinite(margin.right)&&std::isfinite(margin.bottom),
                        "INVALID_LAYOUT","Margin values must be finite");
                    require(margin.left>=0&&margin.top>=0&&margin.right>=0&&margin.bottom>=0&&
                        evaluated.layout->margin->left>=0&&std::isfinite(evaluated.layout->margin->top)&&
                        evaluated.layout->margin->top>=0&&
                        evaluated.layout->margin->left+margin.right<evaluated.width&&
                        evaluated.layout->margin->top+margin.bottom<evaluated.height,
                        "INVALID_LAYOUT","Margins must be nonnegative and leave positive content width and height");
                }
                if(layout.grid) {
                    const auto& grid=*layout.grid;add(grid.id);
                    const auto& bounds=grid.bounds;
                    const auto& evaluated_bounds=evaluated.layout->grid->bounds;
                    require(std::isfinite(bounds.x)&&std::isfinite(bounds.y)&&std::isfinite(bounds.width)&&std::isfinite(bounds.height)&&
                        std::isfinite(grid.column_gutter)&&std::isfinite(grid.row_gutter),
                        "INVALID_LAYOUT","Grid values must be finite");
                    require(bounds.x>=0&&evaluated_bounds.x>=0&&bounds.y>=0&&evaluated_bounds.y>=0&&bounds.width>0&&bounds.height>0&&
                        evaluated_bounds.x+bounds.width<=evaluated.width&&evaluated_bounds.y+bounds.height<=evaluated.height,
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

    GuidePositionEvaluator guide_positions(d);
    for(const auto& comp:d.compositions)for(const auto& guide:comp.guides)
        (void)guide_positions.evaluate(comp.id,guide.id);

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
            require(o.legacy_stroke.empty(),"INVALID_DOMAIN","Groups do not own legacy Stroke addresses");
            require(o.stack.size()<=128,"LIMIT","Group operation stack limit 128");
            for(const auto& op:o.stack) {
                add(op.id);
                require(op.type=="nect.group.posterize","INVALID_DOMAIN","Groups support only nect.group.posterize postchildren operations");
                require(op.version==1,"UNSUPPORTED_OPERATOR_VERSION",op.type);
                const auto expected=default_operation(op.id,op.type);
                require(op.parameters.size()==expected.parameters.size(),"INVALID_OPERATOR_PARAMETERS",op.type);
                for(const auto& [name,value]:op.parameters){(void)value;require(expected.parameters.contains(name),"INVALID_OPERATOR_PARAMETERS",op.type);}
                require(op.composite=="below"&&op.fill_rule=="nonzero"&&op.line_join=="miter"&&op.line_cap=="butt"&&!op.gradient&&!op.fill_rule_driver,
                    "INVALID_OPERATOR_OPTIONS","Group Posterize has no shape compositing, fill, stroke or gradient options");
            }
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
                require(op.type!="nect.group.posterize","INVALID_DOMAIN","nect.group.posterize requires a Group target");
                add(op.id);const bool styled_stroke=op.type=="nect.paint.stroke"&&op.version==2;
                require(op.version==1||styled_stroke,"UNSUPPORTED_OPERATOR_VERSION",op.type);
                auto expected=default_operation(op.id,op.type);
                if(styled_stroke)expected.parameters.emplace("miter_limit",Scalar{4,{}});
                require(op.parameters.size()==expected.parameters.size(),"INVALID_OPERATOR_PARAMETERS",op.type);
                for(const auto& [name,value]:op.parameters){(void)value;require(expected.parameters.contains(name),"INVALID_OPERATOR_PARAMETERS",name);}
                require(op.composite=="above"||op.composite=="below","UNSUPPORTED_COMPOSITE",op.composite);
                require(op.fill_rule=="nonzero"||op.fill_rule=="evenodd","UNSUPPORTED_FILL_RULE",op.fill_rule);
                if(op.type!="nect.paint.fill"&&op.type!="nect.shape.offset")require(op.fill_rule=="nonzero","INVALID_OPERATOR_OPTIONS","Fill rule only applies to Fill or Offset");
                require(!op.fill_rule_driver||op.type=="nect.paint.fill","INVALID_DOMAIN","Fill rule links apply only to Fill operations");
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
    (void)evaluate_object_visibilities(d);
    (void)evaluate_composite_isolations(d);
    (void)evaluate_geometry_mask_enableds(d);
    (void)evaluate_point_edit_enableds(d);
    for(const auto& [id,object]:d.objects)if(object.text&&object.text->path_attachment) {
        const auto& attachment=*object.text->path_attachment;
        identity(attachment.path);identity(attachment.contour);
        require(attachment.start_mode=="distance"||attachment.start_mode=="normalized","TEXT_PATH_START_MODE",
            "Text-on-Path start mode must be distance or normalized");
        finite(attachment.start);finite(attachment.spacing);
        require(attachment.spacing>=0,"TEXT_PATH_SPACING","Text-on-Path spacing cannot be negative");
        const auto path=d.objects.find(attachment.path);
        require(path!=d.objects.end(),"MISSING_PATH_ATTACHMENT","Text-on-Path source Path no longer exists");
        require(path->second.kind==Kind::path,"INVALID_PATH_ATTACHMENT","Text-on-Path source must be a Path object");
        require(!path->second.source,"GENERATED_PATH_ATTACHMENT","Text-on-Path requires an authored Path contour; convert generated geometry first");
        require(std::any_of(path->second.contours.begin(),path->second.contours.end(),[&](const Contour& contour){return contour.id==attachment.contour;}),
            "MISSING_PATH_CONTOUR","Text-on-Path source contour ID no longer exists");
        require(compositions.at(id)==compositions.at(attachment.path),"CROSS_COMPOSITION","Text and its Path source must share a Composition");
    }

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
        if(scalar->expression) {
            const auto& expression=compiled_expression(expressions,*scalar->expression);
            validate_expression_unit(expression,unit(ref));
            for(const auto& source:expression_dependencies(expression))
                require(!artboard_size_ref(source),"CROSS_TYPE_DEPENDENCY",
                    "Scalar expressions cannot reference Artboard dimensions; use an Artboard size expression");
        }
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
    (void)evaluate_text_weights(d);
    (void)evaluate_text_contents(d);
    (void)evaluate_text_families(d);
    (void)evaluate_text_locales(d);
    (void)evaluate_text_directions(d);
    (void)evaluate_text_layouts(d);
    (void)evaluate_text_alignments(d);
    const auto fill_rule_values=evaluate_fill_rules(d);
    const auto operation_enabled_values=evaluate_operation_enableds(d);
    const auto gradient_enabled_values=evaluate_gradient_enableds(d);
    const auto transforms=evaluate_transforms(d,values);
    for(const auto& [id,object]:d.objects)if(object.text&&object.text->path_attachment) {
        const auto source=evaluated_text_source(d,id);
        require(source.layout=="auto","TEXT_PATH_LAYOUT_UNSUPPORTED","Text-on-Path requires automatic one-line layout");
        require(source.direction=="horizontal","TEXT_PATH_DIRECTION_UNSUPPORTED","Text-on-Path requires horizontal writing");
        require(source.content.find_first_of("\r\n\v\f")==std::string::npos&&
            source.content.find("\xe2\x80\xa8")==std::string::npos&&source.content.find("\xe2\x80\xa9")==std::string::npos,
            "TEXT_PATH_MULTILINE_UNSUPPORTED","Text-on-Path does not support hard line breaks");
        const auto& attachment=*object.text->path_attachment;
        const auto sampler=build_path_sampler(d,attachment.path,attachment.contour,values);
        (void)inverse_affine(transforms.at(id).world);
        if(!sampler.closed) {
            if(attachment.start_mode=="normalized")require(attachment.start>=0&&attachment.start<=1,
                "TEXT_PATH_OVERFLOW","Normalized Text-on-Path start must be in [0,1] for an open contour");
            else require(attachment.start>=0&&attachment.start<=sampler.length,
                "TEXT_PATH_OVERFLOW","Text-on-Path start must be within the open contour");
        }
        auto parent=transforms.at(object.text->path_attachment->path).effective_parent;
        for(unsigned depth=0;!parent.empty()&&depth<=128;++depth) {
            require(parent!=id,"TEXT_PATH_TRANSFORM_CYCLE","A Text-on-Path source cannot inherit its consumer's transform");
            const auto found=transforms.find(parent);if(found==transforms.end())break;parent=found->second.effective_parent;
        }
    }
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
            if(operation_enabled_values.at(operation_ref(id,op.id,"enabled"))&&
                gradient_enabled_values.at(gradient_ref(id,op.id,g.id,"enabled")))
                require(std::hypot(v("end_x")-v("start_x"),v("end_y")-v("start_y"))>1e-9,
                    "GRADIENT_GEOMETRY","Gradient start and end must differ");
            std::set<double> offsets;
            for(const auto& stop:g.stops)require(offsets.insert(values.at(gradient_ref(id,op.id,g.id,"stop."+stop.id+".offset"))).second,
                "GRADIENT_STOPS","Coincident gradient stop offsets are not yet supported");
        }
        bool check_shape=(o.kind==Kind::path||o.kind==Kind::text)&&(o.stack.size()>1||
            std::any_of(o.stack.begin(),o.stack.end(),[&](const auto& op){return
                operation_enabled_values.at(operation_ref(id,op.id,"enabled"))&&
                (op.type=="nect.shape.repeater"||op.type=="nect.shape.offset");}));
#ifdef _WIN32
        check_shape=check_shape||o.kind==Kind::text;
#else
        if(o.kind==Kind::text)check_shape=false; // Authored text remains readable without the Windows layout backend.
#endif
        if(check_shape)
            (void)evaluate_shape(d,id,values,&fill_rule_values,&operation_enabled_values,&gradient_enabled_values);
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
            const auto board=evaluate_artboard(*plane,target_artboard->id);const auto& grid=board.layout->grid->bounds;
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
            const auto layout=evaluate_text_projection(document,id,scalar_values);
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
            if(target.kind==ReferenceKind::guide)desired=evaluate_guide_position(document,plane->id,target_guide->id);
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
    const auto& destination=document.objects.at(command.group);
    require(evaluate_object_visibility(document,command.group),"PUT_INSIDE_APPEARANCE","Put Inside destination Group is hidden");
    require(destination.compositing.opacity.literal==1,"PUT_INSIDE_APPEARANCE","Put Inside destination Group has non-neutral opacity");
    require(!driven(destination.compositing.opacity),"PUT_INSIDE_APPEARANCE","Put Inside destination Group opacity is driven");
    require(destination.compositing.blend=="normal","PUT_INSIDE_APPEARANCE","Put Inside destination Group uses a non-normal blend mode");
    require(!evaluate_composite_isolation(document,{command.group,"","composite.isolated"}),
        "PUT_INSIDE_APPEARANCE","Put Inside destination Group is isolated");
    require(!destination.compositing.mask,"PUT_INSIDE_APPEARANCE","Put Inside destination Group has a mask");
    require(destination.stack.empty(),"PUT_INSIDE_APPEARANCE","Put Inside destination Group has effects");
    const auto values=evaluate(document);const auto before=evaluate_transforms(document,values);
    const auto basis=before.at(command.group).world;
    list.erase(start,start+static_cast<std::ptrdiff_t>(command.members.size()));
    auto& children=document.objects.at(command.group).children;children.insert(children.begin(),command.members.begin(),command.members.end());
    std::optional<Affine> inverse;std::set<Ref> changed_affines;
    for(const auto& id:command.members) {
        if(document.objects.at(id).transform_parent)continue;
        const auto& old=before.at(id);const auto unchanged=compose(basis,old.local);
        bool matches=true;for(std::size_t i=0;i<6;++i)matches=matches&&transform_equal(unchanged[i],old.world[i]);
        if(matches)continue;
        if(!inverse)inverse=inverse_affine(basis);
        set_affine(document,id,compose(*inverse,old.world),values);
        for(const auto& field:affine_fields)changed_affines.insert({id,"",field});
    }
    const auto after_values=evaluate(document);const auto after=evaluate_transforms(document,after_values);
    for(const auto& [id,old]:before)for(std::size_t i=0;i<6;++i) {
        const auto current=after.at(id).world[i];
        const auto roundoff=unique.contains(id)&&
            std::abs(old.world[i]-current)<=32*std::numeric_limits<double>::epsilon()*
                std::max({1.0,std::abs(old.world[i]),std::abs(current),std::abs(basis[4]),std::abs(basis[5])});
        require(transform_equal(old.world[i],current)||roundoff,"TRANSFORM_PRESERVATION","Put Inside changed world transform of "+id);
    }
    for(const auto& [ref,value]:values)if(!changed_affines.contains(ref))
        require(transform_equal(value,after_values.at(ref)),"PUT_INSIDE_DEPENDENCY","Put Inside changed a dependent property value");
}
void ungroup(Document& document,const Ungroup& command) {
    require(document.objects.contains(command.group)&&document.objects.at(command.group).kind==Kind::group,"INVALID_GROUP","Ungroup requires a Group");
    const auto group=document.objects.at(command.group);auto& list=siblings(document,command.composition,command.parent);
    const auto at=std::find(list.begin(),list.end(),command.group);require(at!=list.end(),"INVALID_GROUP","Group must belong to the specified parent");
    require(evaluate_object_visibility(document,command.group)&&group.compositing.blend=="normal"&&
        !evaluate_composite_isolation(document,{command.group,"","composite.isolated"})&&!group.compositing.mask&&
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
void move_out(Document& document,const MoveOut& command) {
    require(command.placement=="before"||command.placement=="after","MOVE_OUT_PLACEMENT","Placement must be before or after the Folder");
    require(!command.members.empty()&&command.members.size()<=1000,"MOVE_OUT_SELECTION","Move Out requires 1..1000 ordered Folder children");
    require(document.objects.contains(command.group)&&document.objects.at(command.group).kind==Kind::group,"INVALID_GROUP","Move Out requires a Group Folder");
    const auto group=document.objects.at(command.group);
    auto& siblings_list=siblings(document,command.composition,command.parent);
    const auto group_at=std::find(siblings_list.begin(),siblings_list.end(),command.group);
    require(group_at!=siblings_list.end(),"INVALID_GROUP","Folder must belong to the specified parent and Composition");
    require(evaluate_object_visibility(document,command.group)&&group.compositing.blend=="normal"&&
        !evaluate_composite_isolation(document,{command.group,"","composite.isolated"})&&!group.compositing.mask&&
        group.compositing.opacity.literal==1&&!driven(group.compositing.opacity)&&group.stack.empty(),
        "UNGROUP_APPEARANCE","Move Out requires a visible neutral Folder without opacity, blend, isolation, mask or effects");
    require(!group.transform_parent&&std::none_of(group.transform.begin(),group.transform.end(),[](const Scalar& v){return driven(v);}),
        "UNGROUP_DYNAMIC","Move Out requires static Folder transforms following structure");

    auto& children=document.objects.at(command.group).children;
    std::set<Id> unique;
    for(const auto& id:command.members) {
        require(id!=command.group&&unique.insert(id).second,"DUPLICATE_TARGET","Moved Folder children must be unique");
        require(document.objects.contains(id),"MISSING_OBJECT",id);
    }
    const auto is_prefix=command.members.size()<=children.size()&&
        std::equal(command.members.begin(),command.members.end(),children.begin());
    const auto suffix_start=children.size()-std::min(children.size(),command.members.size());
    const auto is_suffix=command.members.size()<=children.size()&&
        std::equal(command.members.begin(),command.members.end(),children.begin()+static_cast<std::ptrdiff_t>(suffix_start));
    require((command.placement=="before"&&is_prefix)||(command.placement=="after"&&is_suffix),
        "MOVE_OUT_SELECTION","Move Out requires an ordered contiguous prefix before or suffix after the Folder");

    const auto values=evaluate(document);const auto before=evaluate_transforms(document,values);
    std::set<Ref> changed_affines;
    for(const auto& id:command.members) {
        if(document.objects.at(id).transform_parent)continue;
        const auto desired=compose(before.at(command.group).local,before.at(id).local);
        for(std::size_t i=0;i<desired.size();++i) {
            const Ref ref{id,"",affine_fields[i]};
            if(transform_equal(values.at(ref),desired[i]))continue;
            set_changed_scalar(document,ref,desired[i],values);
            changed_affines.insert(ref);
        }
    }

    const auto group_index=static_cast<std::size_t>(std::distance(siblings_list.begin(),group_at));
    const auto child_index=command.placement=="before"?std::size_t{0}:children.size()-command.members.size();
    children.erase(children.begin()+static_cast<std::ptrdiff_t>(child_index),
        children.begin()+static_cast<std::ptrdiff_t>(child_index+command.members.size()));
    auto insert_at=siblings_list.begin()+static_cast<std::ptrdiff_t>(group_index+(command.placement=="after"?1:0));
    siblings_list.insert(insert_at,command.members.begin(),command.members.end());

    validate(document);
    const auto after_values=evaluate(document);const auto after=evaluate_transforms(document,after_values);
    for(const auto& [id,old]:before)for(std::size_t i=0;i<old.world.size();++i)
        require(transform_equal(old.world[i],after.at(id).world[i]),"TRANSFORM_PRESERVATION","Move Out changed a surviving world transform");
    for(const auto& [ref,value]:values)if(!changed_affines.contains(ref))
        require(transform_equal(value,after_values.at(ref)),"UNGROUP_DEPENDENCY","Move Out changed geometry or another property through a transform dependency");
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
    } else if(ref.field.starts_with("mask.")&&ref.field.ends_with(".enabled")) {
        const auto [mask_id,parameter]=geometry_mask_enabled_address(ref.field);
        if(plan.ids.contains(mask_id))ref.field="mask."+plan.ids.at(mask_id)+"."+parameter;
    } else if(ref.field.starts_with("point_edit.")&&ref.field.ends_with(".enabled")) {
        const auto point_edit_id=point_edit_enabled_address(ref.field);
        if(plan.ids.contains(point_edit_id))ref.field="point_edit."+plan.ids.at(point_edit_id)+".enabled";
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
        if(object.visibility_driver)object.visibility_driver=remap(*object.visibility_driver);
        if(object.compositing.isolated_driver)object.compositing.isolated_driver=remap(*object.compositing.isolated_driver);
        if(object.compositing.mask) {
            auto& mask=*object.compositing.mask;
            if(mask.enabled_driver)mask.enabled_driver=remap(*mask.enabled_driver);
            mask.id=plan.ids.at(mask.id);
            if(plan.objects.contains(mask.source))mask.source=plan.ids.at(mask.source);
        }
        if(object.source)object.source->id=plan.ids.at(object.source->id);
        if(object.text) {
            object.text->id=plan.ids.at(object.text->id);
            if(object.text->path_attachment&&plan.objects.contains(object.text->path_attachment->path)) {
                object.text->path_attachment->path=plan.ids.at(object.text->path_attachment->path);
                object.text->path_attachment->contour=plan.ids.at(object.text->path_attachment->contour);
            }
            if(object.text->content_driver)object.text->content_driver->link=remap(object.text->content_driver->link);
            if(object.text->family_driver)object.text->family_driver->link=remap(object.text->family_driver->link);
            if(object.text->locale_driver)object.text->locale_driver->link=remap(object.text->locale_driver->link);
            if(object.text->direction_driver)object.text->direction_driver->link=remap(object.text->direction_driver->link);
            if(object.text->layout_driver)object.text->layout_driver->link=remap(object.text->layout_driver->link);
            if(object.text->alignment_driver)object.text->alignment_driver->link=remap(object.text->alignment_driver->link);
            if(object.text->weight_driver)object.text->weight_driver->link=remap(object.text->weight_driver->link);
            if(object.text->italic_driver) {
                if(auto link=std::get_if<Ref>(&*object.text->italic_driver))*link=remap(*link);
                else object.text->italic_driver=remap_text_italic_expression(std::get<Expression>(*object.text->italic_driver),remap);
            }
        }
        if(object.point_edit) {
            auto& edit=*object.point_edit;
            if(edit.enabled_driver)edit.enabled_driver=remap(*edit.enabled_driver);
            edit.id=plan.ids.at(edit.id);
            auto overrides=std::move(edit.overrides);edit.overrides.clear();
            for(auto& [point,fields]:overrides)edit.overrides.emplace(remap({id,point,"x"}).point,std::move(fields));
        }
        for(auto& contour:object.contours){contour.id=plan.ids.at(contour.id);for(auto& point:contour.points)point.id=plan.ids.at(point.id);}
        for(auto& op:object.stack) {
            if(op.enabled_driver)op.enabled_driver=remap(*op.enabled_driver);
            if(op.fill_rule_driver)op.fill_rule_driver->link=remap(op.fill_rule_driver->link);
            op.id=plan.ids.at(op.id);if(op.gradient){
                if(op.gradient->enabled_driver)op.gradient->enabled_driver=remap(*op.gradient->enabled_driver);
                op.gradient->id=plan.ids.at(op.gradient->id);for(auto& stop:op.gradient->stops)stop.id=plan.ids.at(stop.id);
            }
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
void edit_structural_command(Document& candidate,const CreatePath& command) {
    require(!candidate.objects.contains(command.id),"DUPLICATE_ID",command.id);
    require(!command.contours.empty(),"INVALID_PATH","Create Path needs a contour");
    Object object;object.id=command.id;object.name=command.name;object.contours=command.contours;
    siblings(candidate,command.composition,command.parent).push_back(command.id);
    candidate.objects.emplace(command.id,std::move(object));
    add_default_stroke(candidate,command.id);
}
void edit_structural_command(Document& candidate,const AddPoint& command) {
    contour(candidate,command.object,command.contour).points.push_back(command.point);
}
void edit_structural_command(Document& candidate,const RemovePoint& command) {
    auto& points=contour(candidate,command.object,command.contour).points;
    auto it=std::find_if(points.begin(),points.end(),[&](const auto& point){return point.id==command.point;});
    require(it!=points.end(),"MISSING_REFERENCE",command.point);
    require(points.size()>1,"INVALID_PATH","Delete the path to remove its last point");
    points.erase(it);
}
void edit_structural_command(Document& candidate,const CloseContour& command) {
    contour(candidate,command.object,command.contour).closed=command.closed;
}
void require_no_point_edit_dependents(const Document& document,const Ref& source,const std::set<Id>* departing=nullptr) {
    for(const auto& [id,object]:document.objects)if(object.point_edit&&object.point_edit->enabled_driver==source&&
        (!departing||!departing->contains(id)))
        throw Error("POINT_EDIT_IN_USE","Unlink or remove dependent Point Edit before removing "+source.field);
}
void edit_structural_command(Document& candidate,const ReorderObjects& command) {
    auto& list=siblings(candidate,command.composition,command.parent);
    auto a=list,b=command.order;
    std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
    require(a==b,"INVALID_ORDER","Object reorder must be a sibling permutation");
    list=command.order;
}
void edit_structural_command(Document& candidate,const DeleteObjects& command) {
    require(!command.objects.empty(),"INVALID_BATCH","Select objects to delete");
    std::set<Id> removed;
    std::function<void(const Id&)> remove=[&](const Id& id) {
        require(candidate.objects.contains(id),"MISSING_OBJECT",id);
        if(!removed.insert(id).second)return;
        for(const auto& child:candidate.objects.at(id).children)remove(child);
    };
    for(const auto& id:command.objects)remove(id);
    for(const auto& id:removed)if(const auto& object=candidate.objects.at(id);object.point_edit)
        require_no_point_edit_dependents(candidate,point_edit_enabled_ref(id,object.point_edit->id),&removed);
    auto prune=[&](std::vector<Id>& list){std::erase_if(list,[&](const Id& id){return removed.contains(id);});};
    for(auto& comp:candidate.compositions)prune(comp.roots);
    for(auto& [id,object]:candidate.objects){(void)id;prune(object.children);}
    for(auto& collection:candidate.collections)prune(collection.members);
    for(const auto& id:removed)candidate.objects.erase(id);
    // Validation rejects surviving references to deleted properties; callers
    // may explicitly unlink/freeze those targets in this same atomic batch.
}
void edit_structural_command(Document& candidate,const GroupContiguous& command) {
    group_contiguous(candidate,command.composition,command.parent,command.members,command.id,command.name);
}
void edit_structural_command(Document& candidate,const CreateFolder& command) {
    require(!candidate.objects.contains(command.id),"DUPLICATE_ID",command.id);
    Object object;object.id=command.id;object.name=command.name;object.kind=Kind::group;
    siblings(candidate,command.composition,command.parent).push_back(command.id);
    candidate.objects.emplace(command.id,std::move(object));
}
void edit_mask_enabled(Document& candidate,const LinkMaskEnabled& command) {
    const auto& target=geometry_mask_enabled_source(candidate,command.target);
    (void)geometry_mask_enabled_source(candidate,command.source);
    require(command.target!=command.source,"DEPENDENCY_CYCLE","Geometry mask enabled cannot link to itself");
    require(!target.enabled_driver||command.replace_driver,"DRIVEN_PROPERTY",
        "Replacing a Geometry mask enabled driver requires replace_driver=true");
    auto& mask=*candidate.objects.at(command.target.object).compositing.mask;mask.enabled_driver=command.source;
}
void edit_mask_enabled(Document& candidate,const UnlinkMaskEnabled& command) {
    const auto& target=geometry_mask_enabled_source(candidate,command.target);
    require(target.enabled_driver.has_value(),"PROPERTY_NOT_LINKED","Geometry mask enabled has no driver to unlink");
    const auto frozen=evaluate_geometry_mask_enabled(candidate,command.target);
    auto& mask=*candidate.objects.at(command.target.object).compositing.mask;
    mask.enabled=frozen;mask.enabled_driver.reset();
}
void edit_point_edit_enabled(Document& candidate,const LinkPointEditEnabled& command) {
    const auto& target=point_edit_enabled_source(candidate,command.target);
    (void)point_edit_enabled_source(candidate,command.source);
    require(command.target!=command.source,"DEPENDENCY_CYCLE","Point Edit enabled cannot link to itself");
    require(!target.enabled_driver||command.replace_driver,"DRIVEN_PROPERTY",
        "Replacing a Point Edit enabled driver requires replace_driver=true");
    candidate.objects.at(command.target.object).point_edit->enabled_driver=command.source;
}
void edit_point_edit_enabled(Document& candidate,const UnlinkPointEditEnabled& command) {
    const auto& target=point_edit_enabled_source(candidate,command.target);
    require(target.enabled_driver.has_value(),"PROPERTY_NOT_LINKED","Point Edit enabled has no driver to unlink");
    const auto frozen=evaluate_point_edit_enabled(candidate,command.target);
    auto& point_edit=*candidate.objects.at(command.target.object).point_edit;
    point_edit.enabled=frozen;point_edit.enabled_driver.reset();
}
void edit_guide_position_expression(Document& candidate,const SetGuidePositionExpression& command) {
    require(command.target.point.empty()&&command.target.field=="guide.position","INVALID_GUIDE_REF",
        "Guide expression target must be an empty-point guide.position Ref");
    identity(command.target.object);
    Composition* owner=nullptr;Guide* target=nullptr;
    for(auto& item:candidate.compositions)for(auto& guide:item.guides)if(guide.id==command.target.object) {
        owner=&item;target=&guide;
    }
    if(!target) {
        if(candidate.objects.contains(command.target.object)||candidate.named_colors.contains(command.target.object))
            throw Error("TYPE_MISMATCH","Guide expression target must identify a Guide: "+command.target.object);
        for(const auto& item:candidate.compositions)for(const auto& board:item.artboards)if(board.id==command.target.object)
            throw Error("TYPE_MISMATCH","Guide expression target must identify a Guide: "+command.target.object);
        throw Error("MISSING_GUIDE",command.target.object);
    }
    const bool same_expression=target->position_expression&&*target->position_expression==command.expression;
    require((!target->position_driver&&!target->position_expression)||same_expression||command.replace_driver,
        "DRIVEN_GUIDE_POSITION","Replacing a Guide position source requires replace_driver=true");
    target->position_driver.reset();
    target->position_expression=command.expression;
    (void)evaluate_guide_position(candidate,owner->id,target->id);
}
void edit_guide_position(Document& candidate,const LinkGuidePosition& command) {
    const auto valid_ref=[](const Ref& ref,const char* role) {
        require(ref.point.empty()&&ref.field=="guide.position","INVALID_GUIDE_REF",
            std::string("Guide ")+role+" must be an empty-point guide.position Ref");
        identity(ref.object);
    };
    valid_ref(command.target,"target");valid_ref(command.source,"source");
    require(command.target.object!=command.source.object,"GUIDE_SELF_LINK","A Guide cannot link its position to itself");
    Composition* target_composition=nullptr;Guide* target=nullptr;
    Composition* source_composition=nullptr;Guide* source=nullptr;
    for(auto& item:candidate.compositions)for(auto& guide:item.guides) {
        if(guide.id==command.target.object){target_composition=&item;target=&guide;}
        if(guide.id==command.source.object){source_composition=&item;source=&guide;}
    }
    const auto wrong_kind=[&](const Id& id) {
        if(candidate.objects.contains(id)||candidate.named_colors.contains(id))return true;
        for(const auto& item:candidate.compositions)for(const auto& board:item.artboards)if(board.id==id)return true;
        return false;
    };
    if(!target) {
        if(wrong_kind(command.target.object))throw Error("TYPE_MISMATCH","Guide link target must identify a Guide: "+command.target.object);
        throw Error("MISSING_GUIDE",command.target.object);
    }
    if(!source) {
        if(wrong_kind(command.source.object))throw Error("TYPE_MISMATCH","Guide link source must identify a Guide: "+command.source.object);
        throw Error("MISSING_GUIDE",command.source.object);
    }
    require(target_composition==source_composition,"WRONG_COMPOSITION","Guide position links must stay in one Composition");
    require(target->axis==source->axis,"GUIDE_AXIS_MISMATCH","Linked Guides must use the same axis");
    const bool same_link=target->position_driver&&*target->position_driver==command.source;
    require((!target->position_driver&&!target->position_expression)||same_link||command.replace_driver,
        "DRIVEN_GUIDE_POSITION","Replacing a Guide position source requires replace_driver=true");
    target->position_driver=command.source;
    target->position_expression.reset();
    (void)evaluate_guide_position(candidate,target_composition->id,target->id);
}
void edit_guide_position(Document& candidate,const SetGuidePositionExpression& command) {
    edit_guide_position_expression(candidate,command);
}
void edit_guide_position(Document& candidate,const UnlinkGuidePosition& command) {
    require(command.target.point.empty()&&command.target.field=="guide.position","INVALID_GUIDE_REF",
        "Guide unlink target must be an empty-point guide.position Ref");
    identity(command.target.object);
    Composition* owner=nullptr;Guide* target=nullptr;
    for(auto& item:candidate.compositions)for(auto& guide:item.guides)if(guide.id==command.target.object) {
        owner=&item;target=&guide;
    }
    if(!target) {
        if(candidate.objects.contains(command.target.object)||candidate.named_colors.contains(command.target.object))
            throw Error("TYPE_MISMATCH","Guide unlink target must identify a Guide: "+command.target.object);
        throw Error("MISSING_GUIDE",command.target.object);
    }
    require(target->position_driver.has_value()||target->position_expression.has_value(),
        "GUIDE_NOT_LINKED","Guide position has no source to unlink");
    const auto frozen=evaluate_guide_position(candidate,owner->id,target->id);
    target->position=frozen;target->position_driver.reset();target->position_expression.reset();
}
Document edited(const Document& document,const std::vector<Command>& commands,std::map<Ref,double>* evaluated=nullptr) {
    require(!commands.empty()&&commands.size()<=1000,"INVALID_BATCH","Batch must have 1..1000 commands");
    std::set<Ref> fill_rule_targets;
    std::set<Ref> visibility_targets;
    std::set<Ref> operation_enabled_targets;
    std::set<Ref> gradient_enabled_targets;
    std::set<Ref> geometry_mask_enabled_targets;
    std::set<Ref> point_edit_enabled_targets;
    std::set<Ref> composite_isolation_targets;
    std::set<Ref> artboard_size_targets;
    std::set<Ref> margin_left_targets;
    std::set<Ref> margin_top_targets;
    std::set<Ref> grid_bounds_x_targets;
    std::set<Ref> grid_bounds_y_targets;
    std::set<Ref> guide_position_targets;
    for(const auto& command:commands)std::visit([&](const auto& value) {
        using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,LinkFillRule>||std::is_same_v<T,UnlinkFillRule>)
            require(fill_rule_targets.insert(value.target).second,"DUPLICATE_TARGET","A Fill rule target may be linked or unlinked only once per batch");
        else if constexpr(std::is_same_v<T,LinkObjectVisibility>||std::is_same_v<T,UnlinkObjectVisibility>)
            require(visibility_targets.insert(value.target).second,"DUPLICATE_TARGET","An Object visibility target may be linked or unlinked only once per batch");
        else if constexpr(std::is_same_v<T,LinkOperationEnabled>||std::is_same_v<T,UnlinkOperationEnabled>)
            require(operation_enabled_targets.insert(value.target).second,"DUPLICATE_TARGET","An operation enabled target may be linked or unlinked only once per batch");
        else if constexpr(std::is_same_v<T,LinkGradientEnabled>||std::is_same_v<T,UnlinkGradientEnabled>)
            require(gradient_enabled_targets.insert(value.target).second,"DUPLICATE_TARGET","A Gradient enabled target may be linked or unlinked only once per batch");
        else if constexpr(std::is_same_v<T,LinkMaskEnabled>||std::is_same_v<T,UnlinkMaskEnabled>)
            require(geometry_mask_enabled_targets.insert(value.target).second,"DUPLICATE_TARGET","A geometry mask enabled target may be linked or unlinked only once per batch");
        else if constexpr(std::is_same_v<T,LinkPointEditEnabled>||std::is_same_v<T,UnlinkPointEditEnabled>)
            require(point_edit_enabled_targets.insert(value.target).second,"DUPLICATE_TARGET","A Point Edit enabled target may be linked or unlinked only once per batch");
        else if constexpr(std::is_same_v<T,LinkCompositeIsolated>||std::is_same_v<T,UnlinkCompositeIsolated>)
            require(composite_isolation_targets.insert(value.target).second,"DUPLICATE_TARGET","A Composite isolation target may be linked or unlinked only once per batch");
        else if constexpr(std::is_same_v<T,LinkArtboardSize>||std::is_same_v<T,SetArtboardSizeExpression>||std::is_same_v<T,UnlinkArtboardSize>)
            require(artboard_size_targets.insert(value.target).second,"DUPLICATE_TARGET","An Artboard size target may be changed only once per batch");
        else if constexpr(std::is_same_v<T,LayoutDependencyCommand>)
            std::visit([&](const auto& operation) {
                using Operation=std::decay_t<decltype(operation)>;
                constexpr bool margin_operation=std::is_same_v<Operation,LinkMarginLeft>||
                    std::is_same_v<Operation,SetMarginLeftExpression>||std::is_same_v<Operation,UnlinkMarginLeft>;
                constexpr bool margin_top_operation=std::is_same_v<Operation,LinkMarginTop>||
                    std::is_same_v<Operation,SetMarginTopExpression>||std::is_same_v<Operation,UnlinkMarginTop>;
                constexpr bool grid_y_operation=std::is_same_v<Operation,LinkGridBoundsY>||std::is_same_v<Operation,UnlinkGridBoundsY>;
                std::set<Ref>* targets;
                if constexpr(margin_operation)targets=&margin_left_targets;
                else if constexpr(margin_top_operation)targets=&margin_top_targets;
                else if constexpr(grid_y_operation)targets=&grid_bounds_y_targets;
                else targets=&grid_bounds_x_targets;
                require(targets->insert(operation.target).second,"DUPLICATE_TARGET",
                    margin_operation?"A Margin left target may be changed only once per batch":
                        margin_top_operation?"A Margin top target may be changed only once per batch":
                        grid_y_operation?"A Grid bounds y target may be changed only once per batch":
                            "A Grid bounds x target may be changed only once per batch");
            },value.operation);
        else if constexpr(std::is_same_v<T,LinkGuidePosition>||std::is_same_v<T,SetGuidePositionExpression>||std::is_same_v<T,UnlinkGuidePosition>)
            require(guide_position_targets.insert(value.target).second,"DUPLICATE_TARGET","A Guide position target may be changed only once per batch");
    },command);

    auto candidate=document;
    for(const auto& command:commands) std::visit([&](const auto& c) {
        using T=std::decay_t<decltype(c)>;
        if constexpr(std::is_same_v<T,DuplicateObjects>) {
            duplicate_objects(candidate,c);
        } else if constexpr(std::is_same_v<T,SetVisibility>||std::is_same_v<T,SetCompositing>||std::is_same_v<T,SetMask>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);auto& object=candidate.objects.at(c.object);
            if constexpr(std::is_same_v<T,SetVisibility>) {
                require(!object.visibility_driver,"DRIVEN_PROPERTY","Unlink Object visibility before changing its authored literal");
                object.visible=c.visible;
            }
            else if constexpr(std::is_same_v<T,SetCompositing>){
                require(!object.compositing.isolated_driver||c.isolated==object.compositing.isolated,
                    "DRIVEN_PROPERTY","Unlink Composite isolation before changing its authored literal");
                object.compositing.blend=c.blend;object.compositing.isolated=c.isolated;
            }
            else {
                const auto& current=object.compositing.mask;
                if(current&&c.mask&&current->id==c.mask->id) {
                    require(!c.mask->enabled_driver||c.mask->enabled_driver==current->enabled_driver,
                        "USE_TYPED_COMMAND","Create or replace Geometry mask enabled links with link_mask_enabled");
                    require(c.mask->enabled==current->enabled||!current->enabled_driver,
                        "DRIVEN_PROPERTY","Unlink the Geometry mask enabled driver before changing its authored literal");
                    auto next=*c.mask;next.enabled_driver=current->enabled_driver;object.compositing.mask=std::move(next);
                } else {
                    require(!c.mask||!c.mask->enabled_driver,"USE_TYPED_COMMAND",
                        "Create Geometry mask enabled links with link_mask_enabled");
                    object.compositing.mask=c.mask;
                }
            }
        } else if constexpr(std::is_same_v<T,LinkObjectVisibility>) {
            const auto& target=visibility_source(candidate,c.target);
            (void)visibility_source(candidate,c.source);
            require(c.target.object!=c.source.object,"DEPENDENCY_CYCLE","Object visibility cannot link to itself");
            require(!target.visibility_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing an Object visibility driver requires replace_driver=true");
            candidate.objects.at(c.target.object).visibility_driver=c.source;
        } else if constexpr(std::is_same_v<T,LinkOperationEnabled>) {
            const auto& target=operation_enabled_source(candidate,c.target);
            (void)operation_enabled_source(candidate,c.source);
            require(c.target!=c.source,"DEPENDENCY_CYCLE","Operation enabled cannot link to itself");
            require(!target.enabled_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing an operation enabled driver requires replace_driver=true");
            const auto [operation_id,parameter]=operation_address(c.target.field);(void)parameter;
            operation(candidate.objects.at(c.target.object),operation_id).enabled_driver=c.source;
        } else if constexpr(std::is_same_v<T,UnlinkOperationEnabled>) {
            const auto& target=operation_enabled_source(candidate,c.target);
            require(target.enabled_driver.has_value(),"PROPERTY_NOT_LINKED","Operation enabled has no driver to unlink");
            const auto frozen=evaluate_operation_enabled(candidate,c.target);
            const auto [operation_id,parameter]=operation_address(c.target.field);(void)parameter;
            auto& source=operation(candidate.objects.at(c.target.object),operation_id);
            source.enabled=frozen;source.enabled_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkGradientEnabled>) {
            const auto& target=gradient_enabled_source(candidate,c.target);
            (void)gradient_enabled_source(candidate,c.source);
            require(c.target!=c.source,"DEPENDENCY_CYCLE","Gradient enabled cannot link to itself");
            require(!target.enabled_driver||c.replace_driver,
                "DRIVEN_PROPERTY","Replacing a Gradient enabled driver requires replace_driver=true");
            auto& gradient=gradient_enabled_source(candidate,c.target);gradient.enabled_driver=c.source;
        } else if constexpr(std::is_same_v<T,UnlinkGradientEnabled>) {
            const auto& target=gradient_enabled_source(candidate,c.target);
            require(target.enabled_driver.has_value(),"PROPERTY_NOT_LINKED","Gradient enabled has no driver to unlink");
            const auto frozen=evaluate_gradient_enabled(candidate,c.target);
            auto& gradient=gradient_enabled_source(candidate,c.target);
            gradient.enabled=frozen;gradient.enabled_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkMaskEnabled>||std::is_same_v<T,UnlinkMaskEnabled>) {
            edit_mask_enabled(candidate,c);
        } else if constexpr(std::is_same_v<T,LinkPointEditEnabled>||std::is_same_v<T,UnlinkPointEditEnabled>) {
            edit_point_edit_enabled(candidate,c);
        } else if constexpr(std::is_same_v<T,UnlinkObjectVisibility>) {
            const auto& target=visibility_source(candidate,c.target);
            require(target.visibility_driver.has_value(),"PROPERTY_NOT_LINKED","Object visibility has no driver to unlink");
            const auto frozen=evaluate_object_visibility(candidate,c.target.object);
            auto& object=candidate.objects.at(c.target.object);
            object.visible=frozen;object.visibility_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkCompositeIsolated>) {
            const auto& target=composite_isolation_source(candidate,c.target);
            (void)composite_isolation_source(candidate,c.source);
            require(c.target!=c.source,"DEPENDENCY_CYCLE","Composite isolation cannot link to itself");
            require(!target.isolated_driver||c.replace_driver,
                "DRIVEN_PROPERTY","Replacing a Composite isolation driver requires replace_driver=true");
            candidate.objects.at(c.target.object).compositing.isolated_driver=c.source;
        } else if constexpr(std::is_same_v<T,UnlinkCompositeIsolated>) {
            const auto& target=composite_isolation_source(candidate,c.target);
            require(target.isolated_driver.has_value(),"PROPERTY_NOT_LINKED","Composite isolation has no driver to unlink");
            const auto frozen=evaluate_composite_isolation(candidate,c.target);
            auto& compositing=candidate.objects.at(c.target.object).compositing;
            compositing.isolated=frozen;compositing.isolated_driver.reset();
        } else if constexpr(std::is_same_v<T,MaskObjects>) {
            require(c.members.size()>=2&&c.members.size()<=1000,"INVALID_GROUP","Mask With requires 2..1000 ordered contiguous siblings");
            const auto source=c.top?c.members.back():c.members.front();
            require(candidate.objects.contains(source)&&(candidate.objects.at(source).kind==Kind::path||candidate.objects.at(source).kind==Kind::text),"INVALID_MASK_SOURCE","Mask With source must be a Path or Text");
            require(!candidate.objects.at(source).visibility_driver,"DRIVEN_PROPERTY","Unlink the mask source visibility before Mask With changes its authored literal");
            group_contiguous(candidate,c.composition,c.parent,c.members,c.id,c.name);
            candidate.objects.at(c.id).compositing.mask=GeometryMask{c.mask_id,source};candidate.objects.at(source).visible=false;
        } else if constexpr(std::is_same_v<T,Ungroup>) {
            ungroup(candidate,c);
        } else if constexpr(std::is_same_v<T,MoveOut>) {
            move_out(candidate,c);
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
        } else if constexpr(std::is_same_v<T,LinkTextWeight>) {
            const auto& current=text_weight_source(candidate,c.target);
            (void)text_weight_source(candidate,c.source);
            require(!current.weight_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text weight driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->weight_driver=TextWeightDriver{c.source};
        } else if constexpr(std::is_same_v<T,UnlinkTextWeight>) {
            (void)text_weight_source(candidate,c.target);
            const auto value=evaluate_text_weight(candidate,c.target.object);
            auto& source=*candidate.objects.at(c.target.object).text;
            source.weight=value;source.weight_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkTextContent>) {
            const auto& current=text_content_source(candidate,c.target);
            (void)text_content_source(candidate,c.source);
            require(!current.content_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text content driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->content_driver=TextContentDriver{c.source};
        } else if constexpr(std::is_same_v<T,UnlinkTextContent>) {
            (void)text_content_source(candidate,c.target);
            const auto value=evaluate_text_content(candidate,c.target.object);
            auto& source=*candidate.objects.at(c.target.object).text;
            source.content=value;source.content_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkTextFamily>) {
            const auto& current=text_family_source(candidate,c.target);
            (void)text_family_source(candidate,c.source);
            require(!current.family_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text family driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->family_driver=TextFamilyDriver{c.source};
        } else if constexpr(std::is_same_v<T,UnlinkTextFamily>) {
            (void)text_family_source(candidate,c.target);
            const auto value=evaluate_text_family(candidate,c.target.object);
            auto& source=*candidate.objects.at(c.target.object).text;
            source.family=value;source.family_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkTextLocale>) {
            const auto& current=text_locale_source(candidate,c.target);
            (void)text_locale_source(candidate,c.source);
            require(!current.locale_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text locale driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->locale_driver=TextLocaleDriver{c.source};
        } else if constexpr(std::is_same_v<T,UnlinkTextLocale>) {
            (void)text_locale_source(candidate,c.target);
            const auto value=evaluate_text_locale(candidate,c.target.object);
            auto& source=*candidate.objects.at(c.target.object).text;
            source.locale=value;source.locale_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkTextDirection>) {
            const auto& current=text_direction_source(candidate,c.target);
            (void)text_direction_source(candidate,c.source);
            require(!current.direction_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text direction driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->direction_driver=TextDirectionDriver{c.source};
        } else if constexpr(std::is_same_v<T,UnlinkTextDirection>) {
            (void)text_direction_source(candidate,c.target);
            const auto value=evaluate_text_direction(candidate,c.target.object);
            auto& source=*candidate.objects.at(c.target.object).text;
            source.direction=value;source.direction_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkTextLayout>) {
            const auto& current=text_layout_source(candidate,c.target);
            (void)text_layout_source(candidate,c.source);
            require(!current.layout_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text layout driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->layout_driver=TextLayoutDriver{c.source};
        } else if constexpr(std::is_same_v<T,UnlinkTextLayout>) {
            (void)text_layout_source(candidate,c.target);
            const auto value=evaluate_text_layout(candidate,c.target.object);
            auto& source=*candidate.objects.at(c.target.object).text;
            source.layout=value;source.layout_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkTextAlignment>) {
            const auto& current=text_alignment_source(candidate,c.target);
            (void)text_alignment_source(candidate,c.source);
            require(!current.alignment_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Text alignment driver requires replace_driver=true");
            candidate.objects.at(c.target.object).text->alignment_driver=TextAlignmentDriver{c.source};
        } else if constexpr(std::is_same_v<T,UnlinkTextAlignment>) {
            (void)text_alignment_source(candidate,c.target);
            const auto value=evaluate_text_alignment(candidate,c.target.object);
            auto& source=*candidate.objects.at(c.target.object).text;
            source.alignment=value;source.alignment_driver.reset();
        } else if constexpr(std::is_same_v<T,LinkFillRule>) {
            const auto& current=fill_rule_source(candidate,c.target);
            (void)fill_rule_source(candidate,c.source);
            require(c.target!=c.source,"DEPENDENCY_CYCLE","A Fill rule cannot link to itself");
            require(!current.fill_rule_driver||c.replace_driver,"DRIVEN_PROPERTY","Replacing a Fill rule driver requires replace_driver=true");
            const auto operation_id=operation_address(c.target.field).first;
            operation(candidate.objects.at(c.target.object),operation_id).fill_rule_driver=FillRuleDriver{c.source};
        } else if constexpr(std::is_same_v<T,UnlinkFillRule>) {
            (void)fill_rule_source(candidate,c.target);
            const auto value=evaluate_fill_rule(candidate,c.target);
            const auto operation_id=operation_address(c.target.field).first;
            auto& source=operation(candidate.objects.at(c.target.object),operation_id);
            source.fill_rule=value;source.fill_rule_driver.reset();
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
                require(!c.artboard.width_driver&&!c.artboard.height_driver,"ARTBOARD_DRIVER_SMUGGLING",
                    "Create Artboard size drivers with link_artboard_size or set_artboard_size_expression");
                require(!c.artboard.layout||!c.artboard.layout->margin||!c.artboard.layout->margin->left_driver,
                    "MARGIN_DRIVER_SMUGGLING","Create Margin left drivers with link_margin_left");
                require(!c.artboard.layout||!c.artboard.layout->margin||!c.artboard.layout->margin->left_expression,
                    "MARGIN_DRIVER_SMUGGLING","Create Margin left expressions with set_margin_left_expression");
                require(!c.artboard.layout||!c.artboard.layout->margin||!c.artboard.layout->margin->top_driver,
                    "MARGIN_DRIVER_SMUGGLING","Create Margin top drivers with link_margin_top");
                require(!c.artboard.layout||!c.artboard.layout->margin||!c.artboard.layout->margin->top_expression,
                    "MARGIN_DRIVER_SMUGGLING","Create Margin top expressions with set_margin_top_expression");
                require(!c.artboard.layout||!c.artboard.layout->grid||!c.artboard.layout->grid->bounds_x_driver,
                    "GRID_DRIVER_SMUGGLING","Create Grid bounds x drivers with link_grid_bounds_x");
                require(!c.artboard.layout||!c.artboard.layout->grid||!c.artboard.layout->grid->bounds_x_expression,
                    "GRID_DRIVER_SMUGGLING","Create Grid bounds x expressions with set_grid_bounds_x_expression");
                require(!c.artboard.layout||!c.artboard.layout->grid||!c.artboard.layout->grid->bounds_y_driver,
                    "GRID_DRIVER_SMUGGLING","Create Grid bounds y drivers with link_grid_bounds_y");
                require(!c.artboard.layout||!c.artboard.layout->grid||!c.artboard.layout->grid->bounds_y_expression,
                    "GRID_DRIVER_SMUGGLING","Create Grid bounds y expressions with set_grid_bounds_y_expression");
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
                    const auto* existing_margin=board->layout&&board->layout->margin?&*board->layout->margin:nullptr;
                    auto* incoming_margin=updated.layout&&updated.layout->margin?&*updated.layout->margin:nullptr;
                    preserve_margin_left_source(existing_margin,incoming_margin);
                    preserve_margin_top_source(existing_margin,incoming_margin);
                    const auto* existing_grid=board->layout&&board->layout->grid?&*board->layout->grid:nullptr;
                    auto* incoming_grid=updated.layout&&updated.layout->grid?&*updated.layout->grid:nullptr;
                    preserve_grid_bounds_x_source(existing_grid,incoming_grid);
                    preserve_grid_bounds_y_source(existing_grid,incoming_grid);
                    auto preserve_driver=[&](bool width) {
                        const auto& existing=width?board->width_driver:board->height_driver;
                        auto& incoming=width?updated.width_driver:updated.height_driver;
                        if(existing) {
                            require(!incoming||incoming==existing,"ARTBOARD_DRIVER_SMUGGLING",
                                "Use the dedicated Artboard size commands to replace or remove a driver");
                            require((width?updated.width:updated.height)==(width?board->width:board->height),
                                "DRIVEN_ARTBOARD_SIZE","Unlink or replace the Artboard size driver before editing its literal");
                            require(!updated.parent_size||!(width?updated.parent_size->width:updated.parent_size->height),
                                "ARTBOARD_SOURCE_CONFLICT","A typed Artboard size driver cannot be combined with parent_size inheritance");
                            incoming=existing;
                        } else require(!incoming,"ARTBOARD_DRIVER_SMUGGLING",
                            "Create Artboard size drivers with link_artboard_size or set_artboard_size_expression");
                    };
                    preserve_driver(true);preserve_driver(false);
                    *board=std::move(updated);
                }
                else if constexpr(std::is_same_v<T,DeleteArtboard>) {
                    require(boards.size()>1,"LAST_ARTBOARD","Keep at least one output frame per Composition");
                    for(const auto& dependent:boards)if(dependent.id!=id&&artboard_references_id(dependent,id))
                        throw Error("ARTBOARD_IN_USE","Artboard "+id+" is still referenced by "+dependent.id);
                    boards.erase(board);
                } else {
                    auto resolved=evaluate_artboard(*comp,id);
                    if(board->parent_size) {
                        if(board->parent_size->width&&!board->width_driver)board->width=resolved.width;
                        if(board->parent_size->height&&!board->height_driver)board->height=resolved.height;
                        board->parent_size.reset();
                    }
                }
            }
        } else if constexpr(std::is_same_v<T,LinkArtboardSize>) {
            require(artboard_size_ref(c.target),"INVALID_ARTBOARD_REF","Artboard link target must be an empty-point width or height Ref");
            require(artboard_size_ref(c.source),"INVALID_ARTBOARD_REF","Artboard link source must be an empty-point width or height Ref");
            const auto target=artboard_dimension_location(candidate,c.target,"link target");
            const auto source=artboard_dimension_location(candidate,c.source,"link source");
            require(target.composition==source.composition,"WRONG_COMPOSITION","Artboard size links must stay within one Composition");
            require(c.target!=c.source,"ARTBOARD_SELF_LINK","An Artboard dimension cannot link to itself");
            auto& slot=target.width?target.board->width_driver:target.board->height_driver;
            const bool parent_driven=target.board->parent_size&&
                (target.width?target.board->parent_size->width:target.board->parent_size->height);
            require(!(parent_driven||slot.has_value())||c.replace_driver,"DRIVEN_ARTBOARD_SIZE",
                "Replacing an Artboard size driver requires replace_driver=true");
            if(parent_driven) {
                if(target.width)target.board->parent_size->width=false;
                else target.board->parent_size->height=false;
            }
            slot=Artboard::SizeDriver{c.source};
        } else if constexpr(std::is_same_v<T,SetArtboardSizeExpression>) {
            require(artboard_size_ref(c.target),"INVALID_ARTBOARD_REF","Artboard expression target must be an empty-point width or height Ref");
            const auto target=artboard_dimension_location(candidate,c.target,"expression target");
            const auto compiled=compile_artboard_size_expression(c.expression);
            for(const auto& ref:expression_dependencies(compiled)) {
                const auto source=artboard_dimension_location(candidate,ref,"expression source");
                require(target.composition==source.composition,"WRONG_COMPOSITION","Artboard size expressions must stay within one Composition");
            }
            auto& slot=target.width?target.board->width_driver:target.board->height_driver;
            const bool parent_driven=target.board->parent_size&&
                (target.width?target.board->parent_size->width:target.board->parent_size->height);
            require(!(parent_driven||slot.has_value())||c.replace_driver,"DRIVEN_ARTBOARD_SIZE",
                "Replacing an Artboard size driver requires replace_driver=true");
            if(parent_driven) {
                if(target.width)target.board->parent_size->width=false;
                else target.board->parent_size->height=false;
            }
            slot=Artboard::SizeDriver{c.expression};
        } else if constexpr(std::is_same_v<T,UnlinkArtboardSize>) {
            require(artboard_size_ref(c.target),"INVALID_ARTBOARD_REF","Artboard unlink target must be an empty-point width or height Ref");
            const auto target=artboard_dimension_location(candidate,c.target,"unlink target");
            auto& slot=target.width?target.board->width_driver:target.board->height_driver;
            require(slot.has_value(),"ARTBOARD_SIZE_NOT_LINKED","Artboard size has no typed link or expression to unlink");
            const auto resolved=evaluate_artboard(*target.composition,target.board->id);
            if(target.width)target.board->width=resolved.width;
            else target.board->height=resolved.height;
            slot.reset();
        } else if constexpr(std::is_same_v<T,LayoutDependencyCommand>) {
            std::visit([&](const auto& operation) {
                using Operation=std::decay_t<decltype(operation)>;
                if constexpr(std::is_same_v<Operation,LinkMarginLeft>||std::is_same_v<Operation,SetMarginLeftExpression>||
                    std::is_same_v<Operation,UnlinkMarginLeft>)
                    edit_margin_left(candidate,operation);
                else if constexpr(std::is_same_v<Operation,LinkMarginTop>||std::is_same_v<Operation,SetMarginTopExpression>||
                    std::is_same_v<Operation,UnlinkMarginTop>)
                    edit_margin_top(candidate,operation);
                else if constexpr(std::is_same_v<Operation,LinkGridBoundsX>||std::is_same_v<Operation,SetGridBoundsXExpression>||
                    std::is_same_v<Operation,UnlinkGridBoundsX>)
                    edit_grid_bounds_x(candidate,operation);
                else edit_grid_bounds_y(candidate,operation);
            },c.operation);
        } else if constexpr(std::is_same_v<T,AddGuide>||std::is_same_v<T,UpdateGuide>||std::is_same_v<T,DeleteGuide>) {
            auto comp=std::find_if(candidate.compositions.begin(),candidate.compositions.end(),[&](const auto& item){return item.id==c.composition;});
            require(comp!=candidate.compositions.end(),"MISSING_COMPOSITION",c.composition);
            if constexpr(std::is_same_v<T,AddGuide>) {
                require(!c.guide.position_driver&&!c.guide.position_expression,"GUIDE_DRIVER_SMUGGLING",
                    "Use a dedicated Guide position command after adding an undriven Guide");
                comp->guides.push_back(c.guide);
            }
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
                if constexpr(std::is_same_v<T,UpdateGuide>) {
                    auto updated=c.guide;
                    if(guide->position_driver||guide->position_expression) {
                        require(updated.position==guide->position,"DRIVEN_GUIDE_POSITION",
                            "Unlink the Guide position explicitly before changing its literal");
                        require(!updated.position_driver||updated.position_driver==guide->position_driver,
                            "DRIVEN_GUIDE_DRIVER","Use link_guide_position to replace a Guide position driver");
                        require(!updated.position_expression||updated.position_expression==guide->position_expression,
                            "DRIVEN_GUIDE_DRIVER","Use set_guide_position_expression to replace a Guide position driver");
                        updated.position_driver=guide->position_driver;
                        updated.position_expression=guide->position_expression;
                    } else require(!updated.position_driver&&!updated.position_expression,"GUIDE_DRIVER_SMUGGLING",
                        "Use a dedicated Guide position command to add a source");
                    *guide=std::move(updated);
                }
                else comp->guides.erase(guide);
            }
        } else if constexpr(std::is_same_v<T,LinkGuidePosition>||std::is_same_v<T,SetGuidePositionExpression>||
            std::is_same_v<T,UnlinkGuidePosition>) {
            edit_guide_position(candidate,c);
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
            auto updated_layout=c.layout;
            const auto* existing_margin=board->layout&&board->layout->margin?&*board->layout->margin:nullptr;
            auto* incoming_margin=updated_layout&&updated_layout->margin?&*updated_layout->margin:nullptr;
            preserve_margin_left_source(existing_margin,incoming_margin);
            preserve_margin_top_source(existing_margin,incoming_margin);
            const auto* existing_grid=board->layout&&board->layout->grid?&*board->layout->grid:nullptr;
            auto* incoming_grid=updated_layout&&updated_layout->grid?&*updated_layout->grid:nullptr;
            preserve_grid_bounds_x_source(existing_grid,incoming_grid);
            preserve_grid_bounds_y_source(existing_grid,incoming_grid);
            board->layout=std::move(updated_layout);
        } else if constexpr(std::is_same_v<T,Set>) {
            prepare_point_edit(candidate,c.ref);
            auto& p=lookup_property(candidate,c.ref);
            require(!driven(p),"DRIVEN_PROPERTY","Unlink explicitly before setting a driven property");
            p.literal=c.value;
        } else if constexpr(std::is_same_v<T,Link>) {
            prepare_point_edit(candidate,c.target);
            auto& scalar=lookup_property(candidate,c.target);scalar.binding=c.binding;scalar.expression.reset();
        } else if constexpr(std::is_same_v<T,Unlink>) {
            if(c.target.point.empty()&&c.target.field=="guide.position")
                throw Error("TYPE_MISMATCH","Guide positions use the dedicated Guide link commands");
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
            require(!c.source.weight_driver,"USE_TYPED_COMMAND","Create Text weight links with link_text_weight");
            require(!c.source.content_driver,"USE_TYPED_COMMAND","Create Text content links with link_text_content");
            require(!c.source.family_driver,"USE_TYPED_COMMAND","Create Text family links with link_text_family");
            require(!c.source.locale_driver,"USE_TYPED_COMMAND","Create Text locale links with link_text_locale");
            require(!c.source.direction_driver,"USE_TYPED_COMMAND","Create Text direction links with link_text_direction");
            require(!c.source.layout_driver,"USE_TYPED_COMMAND","Create Text layout links with link_text_layout");
            require(!c.source.alignment_driver,"USE_TYPED_COMMAND","Create Text alignment links with link_text_alignment");
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
            if(o.text->weight_driver) {
                require(!next.weight_driver||next.weight_driver==o.text->weight_driver,"DRIVEN_PROPERTY","UpdateText cannot replace or remove a Text weight driver");
                require(next.weight==o.text->weight,"DRIVEN_PROPERTY","Unlink the Text weight driver before changing its authored literal");
                next.weight_driver=o.text->weight_driver;
            } else require(!next.weight_driver,"USE_TYPED_COMMAND","Create Text weight links with link_text_weight");
            if(o.text->content_driver) {
                require(!next.content_driver||next.content_driver==o.text->content_driver,"DRIVEN_PROPERTY","UpdateText cannot replace or remove a Text content driver");
                require(next.content==o.text->content,"DRIVEN_PROPERTY","Unlink the Text content driver before changing its authored literal");
                next.content_driver=o.text->content_driver;
            } else require(!next.content_driver,"USE_TYPED_COMMAND","Create Text content links with link_text_content");
            if(o.text->family_driver) {
                require(!next.family_driver||next.family_driver==o.text->family_driver,"DRIVEN_PROPERTY","UpdateText cannot replace or remove a Text family driver");
                require(next.family==o.text->family,"DRIVEN_PROPERTY","Unlink the Text family driver before changing its authored literal");
                next.family_driver=o.text->family_driver;
            } else require(!next.family_driver,"USE_TYPED_COMMAND","Create Text family links with link_text_family");
            if(o.text->locale_driver) {
                require(!next.locale_driver||next.locale_driver==o.text->locale_driver,"DRIVEN_PROPERTY","UpdateText cannot replace or remove a Text locale driver");
                require(next.locale==o.text->locale,"DRIVEN_PROPERTY","Unlink the Text locale driver before changing its authored literal");
                next.locale_driver=o.text->locale_driver;
            } else require(!next.locale_driver,"USE_TYPED_COMMAND","Create Text locale links with link_text_locale");
            if(o.text->direction_driver) {
                require(!next.direction_driver||next.direction_driver==o.text->direction_driver,"DRIVEN_PROPERTY","UpdateText cannot replace or remove a Text direction driver");
                require(next.direction==o.text->direction,"DRIVEN_PROPERTY","Unlink the Text direction driver before changing its authored literal");
                next.direction_driver=o.text->direction_driver;
            } else require(!next.direction_driver,"USE_TYPED_COMMAND","Create Text direction links with link_text_direction");
            if(o.text->layout_driver) {
                require(!next.layout_driver||next.layout_driver==o.text->layout_driver,"DRIVEN_PROPERTY","UpdateText cannot replace or remove a Text layout driver");
                require(next.layout==o.text->layout,"DRIVEN_PROPERTY","Unlink the Text layout driver before changing its authored literal");
                next.layout_driver=o.text->layout_driver;
            } else require(!next.layout_driver,"USE_TYPED_COMMAND","Create Text layout links with link_text_layout");
            if(o.text->alignment_driver) {
                require(!next.alignment_driver||next.alignment_driver==o.text->alignment_driver,"DRIVEN_PROPERTY","UpdateText cannot replace or remove a Text alignment driver");
                require(next.alignment==o.text->alignment,"DRIVEN_PROPERTY","Unlink the Text alignment driver before changing its authored literal");
                next.alignment_driver=o.text->alignment_driver;
            } else require(!next.alignment_driver,"USE_TYPED_COMMAND","Create Text alignment links with link_text_alignment");
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
            require(!c.operation.fill_rule_driver,"USE_TYPED_COMMAND","Create Fill rule links with link_fill_rule");
            require(!c.operation.enabled_driver,"USE_TYPED_COMMAND","Create operation enabled links with link_operation_enabled");
            require(!c.operation.gradient||!c.operation.gradient->enabled_driver,"USE_TYPED_COMMAND","Create Gradient enabled links with link_gradient_enabled");
            require(o.kind==Kind::path||o.kind==Kind::text||(o.kind==Kind::group&&c.operation.type=="nect.group.posterize"),
                "INVALID_DOMAIN","Shape operations require a Path or Text; Group postchildren operations require nect.group.posterize");
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
            auto& target=operation(candidate.objects.at(c.object),c.operation);
            require(!target.enabled_driver,"DRIVEN_PROPERTY","Unlink the operation enabled driver before changing its authored literal");
            target.enabled=c.enabled;
        } else if constexpr(std::is_same_v<T,OperationOptions>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& op=operation(candidate.objects.at(c.object),c.operation);
            require(!op.fill_rule_driver||op.fill_rule==c.fill_rule,"DRIVEN_PROPERTY","Unlink the Fill rule driver before changing its authored choice");
            op.composite=c.composite;op.fill_rule=c.fill_rule;
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
            auto& target=operation(candidate.objects.at(c.object),c.operation);
            if(!c.gradient)target.gradient.reset();
            else {
                auto next=*c.gradient;
                if(target.gradient&&target.gradient->id==next.id) {
                    require(!next.enabled_driver||next.enabled_driver==target.gradient->enabled_driver,
                        "USE_TYPED_COMMAND","SetGradient cannot replace a Gradient enabled driver");
                    require(next.enabled==target.gradient->enabled||!target.gradient->enabled_driver,
                        "DRIVEN_PROPERTY","Unlink the Gradient enabled driver before changing its authored literal");
                    next.enabled_driver=target.gradient->enabled_driver;
                } else require(!next.enabled_driver,"USE_TYPED_COMMAND","Create Gradient enabled links with link_gradient_enabled");
                target.gradient=std::move(next);
            }
        } else if constexpr(std::is_same_v<T,EnablePointEdit>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& o=candidate.objects.at(c.object);
            require(o.point_edit.has_value(),"NO_POINT_EDIT","Primitive has no authored point corrections");
            require(!o.point_edit->enabled_driver,"DRIVEN_PROPERTY","Unlink Point Edit enabled before changing its authored literal");
            o.point_edit->enabled=c.enabled;
        } else if constexpr(std::is_same_v<T,ClearPointEdit>) {
            require(candidate.objects.contains(c.object),"MISSING_OBJECT",c.object);
            auto& o=candidate.objects.at(c.object);
            require(o.source.has_value(),"NOT_PRIMITIVE","Point Edit reset requires a retained primitive");
            if(o.point_edit)require_no_point_edit_dependents(candidate,point_edit_enabled_ref(c.object,o.point_edit->id));
            o.point_edit.reset();
        } else if constexpr(std::is_same_v<T,ConvertToPath>) {
            require(conversion_blockers(candidate,c.object).empty(),"CONVERSION_REFERENCE",
                "Generator properties are referenced; explicitly unlink/freeze dependent targets before conversion");
            auto& o=candidate.objects.at(c.object);
            if(o.point_edit)require_no_point_edit_dependents(candidate,point_edit_enabled_ref(c.object,o.point_edit->id));
            const auto values=evaluate(candidate);
            const auto authored=property_index(candidate,true);
            const bool point_edit_enabled=o.point_edit&&evaluate_point_edit_enabled(
                candidate,point_edit_enabled_ref(o.id,o.point_edit->id));
            auto contours=path_contours(o,&values);
            for(auto& ct:contours)for(auto& p:ct.points) {
                const std::array<Scalar*,6> fields{&p.x,&p.y,&p.in_angle,&p.in_length,&p.out_angle,&p.out_length};
                for(std::size_t i=0;i<fields.size();++i) {
                    const Ref ref{o.id,p.id,point_fields[i]};
                    const auto found=authored.find(ref);const auto* override_value=found==authored.end()?nullptr:found->second;
                    *fields[i]=point_edit_enabled&&override_value
                        ?*override_value:Scalar{values.at(ref),{}};
                }
            }
            o.contours=std::move(contours);o.source.reset();o.point_edit.reset();
        } else if constexpr(std::is_same_v<T,CreatePath>||std::is_same_v<T,AddPoint>||
            std::is_same_v<T,RemovePoint>||std::is_same_v<T,CloseContour>||
            std::is_same_v<T,ReorderObjects>||std::is_same_v<T,DeleteObjects>||
            std::is_same_v<T,GroupContiguous>||std::is_same_v<T,CreateFolder>) {
            edit_structural_command(candidate,c);
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
    if(candidate==document_&&commands.size()==1) {
        if(const auto* layout_source=std::get_if<LayoutDependencyCommand>(&commands.front())) {
            const bool source_transition=std::visit([](const auto& operation) {
                using T=std::decay_t<decltype(operation)>;
                return std::is_same_v<T,LinkMarginLeft>||std::is_same_v<T,SetMarginLeftExpression>||
                    std::is_same_v<T,LinkMarginTop>||std::is_same_v<T,SetMarginTopExpression>||
                    std::is_same_v<T,LinkGridBoundsX>||std::is_same_v<T,SetGridBoundsXExpression>||
                    std::is_same_v<T,LinkGridBoundsY>||std::is_same_v<T,SetGridBoundsYExpression>;
            },layout_source->operation);
            if(source_transition)return;
        }
    }
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
