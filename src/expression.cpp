#include "nect/expression.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <numbers>
#include <string_view>

namespace nect {
namespace {
enum class Unit { literal,scalar,distance,degree };
enum class Op { number,reference,positive,negative,add,subtract,multiply,divide,
    absolute,minimum,maximum,clamp,floor,ceil,round,sine,cosine,square_root };
struct Node {
    Op op;Unit unit=Unit::literal;unsigned depth=1;
    std::size_t a=0,b=0,c=0;double number=0;Ref ref;
    std::array<std::pair<std::size_t,std::size_t>,3> ref_spans{};
};
void require(bool value,const char* code,const char* message) {if(!value)throw Error(code,message);}
Unit unit_of(const std::string& name) {
    if(name=="du")return Unit::distance;if(name=="degree")return Unit::degree;
    require(name=="scalar","UNIT_MISMATCH","Unsupported expression unit");return Unit::scalar;
}
Unit additive(Unit a,Unit b) {
    if(a==Unit::literal)return b;if(b==Unit::literal)return a;
    require(a==b,"UNIT_MISMATCH","Expression operands require the same unit");return a;
}
bool dimensionless(Unit unit){return unit==Unit::literal||unit==Unit::scalar;}
bool letter(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';}
bool digit(char c){return c>='0'&&c<='9';}
bool id_character(char c){return letter(c)||digit(c)||c=='-';}
}

struct ExpressionProgram {std::vector<Node> nodes;std::vector<Ref> dependencies;std::size_t root=0;};

namespace {
class Parser {
    std::string_view source_;std::size_t cursor_=0,reference_count_=0;
    ExpressionProgram result_;
    void whitespace(){while(cursor_<source_.size()&&(source_[cursor_]==' '||source_[cursor_]=='\t'||source_[cursor_]=='\n'||source_[cursor_]=='\r'))++cursor_;}
    bool take(char c){whitespace();if(cursor_<source_.size()&&source_[cursor_]==c){++cursor_;return true;}return false;}
    void expect(char c){require(take(c),"EXPRESSION_SYNTAX","Missing expression delimiter");}
    std::size_t append(Node node) {
        require(result_.nodes.size()<256,"EXPRESSION_LIMIT","Expression node limit 256");
        require(node.depth<=32,"EXPRESSION_LIMIT","Expression AST depth limit 32");
        result_.nodes.push_back(std::move(node));return result_.nodes.size()-1;
    }
    std::size_t unary(Op op,std::size_t child) {
        const auto& a=result_.nodes[child];Node node{op};node.a=child;node.depth=a.depth+1;node.unit=a.unit;
        if(op==Op::sine||op==Op::cosine) {
            require(a.unit==Unit::literal||a.unit==Unit::degree,"UNIT_MISMATCH","sin/cos require degrees or a literal angle");
            node.unit=a.unit==Unit::literal?Unit::literal:Unit::scalar;
        } else if(op==Op::square_root)require(dimensionless(a.unit),"UNIT_MISMATCH","sqrt requires a dimensionless value");
        return append(std::move(node));
    }
    std::size_t binary(Op op,std::size_t left,std::size_t right) {
        const auto& a=result_.nodes[left];const auto& b=result_.nodes[right];Node node{op};
        node.a=left;node.b=right;node.depth=std::max(a.depth,b.depth)+1;
        if(op==Op::multiply) {
            require(dimensionless(a.unit)||dimensionless(b.unit),"UNIT_MISMATCH","Multiplication cannot create compound units");
            node.unit=dimensionless(a.unit)?(b.unit==Unit::literal?a.unit:b.unit):a.unit;
        } else if(op==Op::divide) {
            if(dimensionless(b.unit))node.unit=a.unit==Unit::literal?b.unit:a.unit;
            else {require(a.unit==b.unit,"UNIT_MISMATCH","Division requires a dimensionless divisor or equal-unit ratio");node.unit=Unit::scalar;}
        } else node.unit=additive(a.unit,b.unit);
        return append(std::move(node));
    }
    std::string quoted(bool field,bool empty=false) {
        expect('"');const auto start=cursor_;
        while(cursor_<source_.size()&&source_[cursor_]!='"') {
            const char c=source_[cursor_++];
            require(id_character(c)||(field&&c=='.'),"EXPRESSION_SYNTAX","ref uses stable ASCII IDs and property fields, without escapes");
        }
        require(cursor_<source_.size(),"EXPRESSION_SYNTAX","Unterminated ref argument");
        const auto count=cursor_-start;++cursor_;
        require((empty||count>0)&&count<=(field?512:96),"EXPRESSION_SYNTAX","Invalid ref argument length");
        return std::string(source_.substr(start,count));
    }
    std::size_t atom(unsigned depth) {
        require(depth<=32,"EXPRESSION_LIMIT","Expression nesting limit 32");whitespace();
        if(take('+'))return unary(Op::positive,atom(depth+1));
        if(take('-'))return unary(Op::negative,atom(depth+1));
        if(take('(')){const auto node=sum(depth+1);expect(')');return node;}
        require(cursor_<source_.size(),"EXPRESSION_SYNTAX","Expected an expression value");
        if(digit(source_[cursor_])||source_[cursor_]=='.') {
            const auto start=cursor_;bool digits=false;
            while(cursor_<source_.size()&&digit(source_[cursor_])){digits=true;++cursor_;}
            if(cursor_<source_.size()&&source_[cursor_]=='.') {
                ++cursor_;while(cursor_<source_.size()&&digit(source_[cursor_])){digits=true;++cursor_;}
            }
            require(digits,"EXPRESSION_SYNTAX","Invalid decimal literal");
            if(cursor_<source_.size()&&(source_[cursor_]=='e'||source_[cursor_]=='E')) {
                ++cursor_;if(cursor_<source_.size()&&(source_[cursor_]=='+'||source_[cursor_]=='-'))++cursor_;
                const auto exponent=cursor_;while(cursor_<source_.size()&&digit(source_[cursor_]))++cursor_;
                require(cursor_>exponent,"EXPRESSION_SYNTAX","Missing decimal exponent");
            }
            Node node{Op::number};const auto parsed=std::from_chars(source_.data()+start,source_.data()+cursor_,node.number,std::chars_format::general);
            require(parsed.ec==std::errc{}&&parsed.ptr==source_.data()+cursor_&&std::isfinite(node.number),"EXPRESSION_SYNTAX","Decimal literal must be finite and representable");
            return append(std::move(node));
        }
        const auto start=cursor_;while(cursor_<source_.size()&&letter(source_[cursor_]))++cursor_;
        require(cursor_>start,"EXPRESSION_SYNTAX","Unknown expression token");
        const auto name=source_.substr(start,cursor_-start);expect('(');
        if(name=="ref") {
            Node node{Op::reference};
            auto argument=[&](std::size_t slot,bool field,bool empty=false) {
                whitespace();const auto begin=cursor_+1;auto value=quoted(field,empty);
                node.ref_spans[slot]={begin,cursor_-1};return value;
            };
            node.ref.object=argument(0,false);expect(',');node.ref.point=argument(1,false,true);expect(',');node.ref.field=argument(2,true);expect(')');
            require(++reference_count_<=64,"EXPRESSION_LIMIT","Expression reference limit 64");
            node.unit=unit_of(property_unit(node.ref));
            if(std::find(result_.dependencies.begin(),result_.dependencies.end(),node.ref)==result_.dependencies.end())result_.dependencies.push_back(node.ref);
            return append(std::move(node));
        }
        const auto a=sum(depth+1);
        if(name=="min"||name=="max") {expect(',');const auto b=sum(depth+1);expect(')');return binary(name=="min"?Op::minimum:Op::maximum,a,b);}
        if(name=="clamp") {
            expect(',');const auto b=sum(depth+1);expect(',');const auto c=sum(depth+1);expect(')');
            Node node{Op::clamp};node.a=a;node.b=b;node.c=c;
            node.depth=std::max({result_.nodes[a].depth,result_.nodes[b].depth,result_.nodes[c].depth})+1;
            node.unit=additive(additive(result_.nodes[a].unit,result_.nodes[b].unit),result_.nodes[c].unit);return append(std::move(node));
        }
        expect(')');Op op;
        if(name=="abs")op=Op::absolute;else if(name=="floor")op=Op::floor;else if(name=="ceil")op=Op::ceil;
        else if(name=="round")op=Op::round;else if(name=="sin")op=Op::sine;else if(name=="cos")op=Op::cosine;
        else if(name=="sqrt")op=Op::square_root;else throw Error("EXPRESSION_SYNTAX","Unknown expression function");
        return unary(op,a);
    }
    std::size_t product(unsigned depth) {
        auto result=atom(depth);
        for(;;) {if(take('*'))result=binary(Op::multiply,result,atom(depth));else if(take('/'))result=binary(Op::divide,result,atom(depth));else return result;}
    }
    std::size_t sum(unsigned depth) {
        auto result=product(depth);
        for(;;) {if(take('+'))result=binary(Op::add,result,product(depth));else if(take('-'))result=binary(Op::subtract,result,product(depth));else return result;}
    }
public:
    explicit Parser(std::string_view source):source_(source){}
    ExpressionProgram parse() {
        result_.root=sum(1);whitespace();require(cursor_==source_.size(),"EXPRESSION_SYNTAX","Unexpected trailing expression text");return std::move(result_);
    }
};
const ExpressionProgram& program(const CompiledExpression& expression) {
    require(bool(expression.program),"EXPRESSION_SYNTAX","Missing compiled expression");return *expression.program;
}
}

CompiledExpression compile_expression(const Expression& expression) {
    require(expression.version==1,"UNSUPPORTED_EXPRESSION_VERSION","Only expression version 1 is supported");
    require(!expression.source.empty()&&expression.source.size()<=4096,"EXPRESSION_LIMIT","Expression source must contain 1..4096 bytes");
    return {std::make_shared<const ExpressionProgram>(Parser(expression.source).parse())};
}
void validate_expression_unit(const CompiledExpression& expression,const std::string& expected) {
    const auto& p=program(expression);const auto unit=p.nodes[p.root].unit;
    require(unit==Unit::literal||unit==unit_of(expected),"UNIT_MISMATCH","Expression result does not match the target unit");
}
Expression remap_expression(const Expression& expression,const std::function<Ref(const Ref&)>& remap) {
    const auto compiled=compile_expression(expression);
    struct Replacement {std::size_t begin,end;std::string value;};
    std::vector<Replacement> replacements;
    for(const auto& node:program(compiled).nodes)if(node.op==Op::reference) {
        const auto ref=remap(node.ref);
        const std::array<std::string,3> before{node.ref.object,node.ref.point,node.ref.field},after{ref.object,ref.point,ref.field};
        for(std::size_t i=0;i<3;++i)if(before[i]!=after[i])replacements.push_back({node.ref_spans[i].first,node.ref_spans[i].second,after[i]});
    }
    std::sort(replacements.begin(),replacements.end(),[](const auto& a,const auto& b){return a.begin>b.begin;});
    auto result=expression;
    for(const auto& replacement:replacements)result.source.replace(replacement.begin,replacement.end-replacement.begin,replacement.value);
    (void)compile_expression(result);return result;
}
const std::vector<Ref>& expression_dependencies(const CompiledExpression& expression){return program(expression).dependencies;}
std::vector<Ref> expression_dependencies(const Expression& expression){return expression_dependencies(compile_expression(expression));}
double evaluate_expression(const CompiledExpression& expression,const std::string& expected,const std::function<double(const Ref&)>& resolve) {
    validate_expression_unit(expression,expected);const auto& p=program(expression);std::vector<double> values;values.reserve(p.nodes.size());
    for(const auto& node:p.nodes) {
        double value=0;const auto a=[&]{return values.at(node.a);};const auto b=[&]{return values.at(node.b);};
        switch(node.op) {
        case Op::number:value=node.number;break;case Op::reference:value=resolve(node.ref);break;
        case Op::positive:value=a();break;case Op::negative:value=-a();break;
        case Op::add:value=a()+b();break;case Op::subtract:value=a()-b();break;case Op::multiply:value=a()*b();break;
        case Op::divide:require(b()!=0,"EXPRESSION_DOMAIN","Division by zero");value=a()/b();break;
        case Op::absolute:value=std::abs(a());break;case Op::minimum:value=std::min(a(),b());break;case Op::maximum:value=std::max(a(),b());break;
        case Op::clamp:require(b()<=values.at(node.c),"EXPRESSION_DOMAIN","Clamp minimum exceeds maximum");value=std::clamp(a(),b(),values.at(node.c));break;
        case Op::floor:value=std::floor(a());break;case Op::ceil:value=std::ceil(a());break;case Op::round:value=std::round(a());break;
        case Op::sine:value=std::sin(std::remainder(a(),360.0)*std::numbers::pi/180);break;
        case Op::cosine:value=std::cos(std::remainder(a(),360.0)*std::numbers::pi/180);break;
        case Op::square_root:require(a()>=0,"EXPRESSION_DOMAIN","sqrt requires a nonnegative value");value=std::sqrt(a());break;
        }
        require(std::isfinite(value),"NON_FINITE","Expression result is not finite");values.push_back(value);
    }
    return values.at(p.root);
}
}
