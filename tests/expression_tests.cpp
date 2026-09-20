#include "nect/core.hpp"
#include "nect/expression.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace nect;
namespace {
int checks=0;
void check(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);++checks;}
void near(double value,double expected,const std::string& message){check(std::abs(value-expected)<1e-8,message);}
template<class F>void rejects(const char* code,F action) {
    try{action();}catch(const Error& error){check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
void atomic_reject(Session& session,const char* code,std::vector<Command> commands) {
    const auto document=session.document();const auto revision=session.revision();const auto history=session.history();
    rejects(code,[&]{apply(session,std::move(commands));});
    check(session.document()==document&&session.revision()==revision&&session.history()==history,"Rejected formula preserves authored state, revision and History");
}
Ref x(const Id& id){return {id,id+"-point","x"};}
Ref tf(const Id& id,const std::string& field){return {id,"","transform."+field};}
std::string reference(const Ref& ref){return "ref(\""+ref.object+"\",\""+ref.point+"\",\""+ref.field+"\")";}
SetExpression formula(Ref target,std::string source,bool replace=false){return {{std::move(target)},{std::move(source)},replace};}
double value(const Session& session,const Ref& ref){return evaluate(session.document()).at(ref);}
Document fixture() {
    auto document=empty_document("document","composition","artboard");
    for(const auto& id:{"a","b","c"}) {
        Object object;object.id=id;object.name=id;Point point;point.id=object.id+"-point";
        point.x.literal=object.id=="a"?100:object.id=="b"?200:300;point.in_angle.literal=90;
        object.contours={{object.id+"-contour",false,{point}}};document.objects.emplace(id,object);document.compositions.front().roots.push_back(id);
    }
    return document;
}

void arithmetic_and_units() {
    Session session(fixture());
    apply(session,{formula(x("a")," -2 + 3 * 4 / 2 ")});near(value(session,x("a")),4,"Operator precedence and unary minus");
    apply(session,{formula(x("a"),"abs(-2)+min(3,4)+max(5,2)+clamp(99,0,10)+floor(2.9)+ceil(2.1)+round(-2.5)+sin(30)+cos(60)+sqrt(9)")});
    near(value(session,x("a")),26,"Bounded numeric functions follow their documented numeric semantics");
    apply(session,{formula(x("a"),"sin(30)*100")});near(value(session,x("a")),50,"Literal-only function arithmetic adopts target distance units");
    apply(session,{formula(x("a"),".5e2 + 2.5E+1")});near(value(session,x("a")),75,"Locale-independent decimal and exponent literals");
    apply(session,{formula(x("a"),"sin("+reference({"b","b-point","in.angle"})+")*"+reference(x("b")))});
    near(value(session,x("a")),200,"Degree function returns a dimensionless factor for a distance");
    apply(session,{formula(x("a"),"clamp("+reference(x("b"))+"+10,0,250)")});near(value(session,x("a")),210,"Additive literals adopt reference units");
    apply(session,{formula(tf("a","a"),reference(x("b"))+"/"+reference(x("c")))});near(value(session,tf("a","a")),2.0/3,"Equal-unit division gives a scalar ratio");
    for(const auto& expression:std::vector<std::string>{reference(x("b"))+"+"+reference({"b","b-point","in.angle"}),
        reference(x("b"))+"*"+reference(x("c")),"1/"+reference(x("b")),"sqrt("+reference(x("b"))+")", "sin("+reference(tf("b","a"))+")"})
        atomic_reject(session,"UNIT_MISMATCH",{formula(x("a"),expression)});
    atomic_reject(session,"UNIT_MISMATCH",{formula(tf("a","a"),reference(x("b")))});
    const auto dependencies=expression_dependencies(Expression{reference(x("b"))+"+"+reference(x("b"))+"-"+reference(x("c"))});
    check(dependencies==std::vector<Ref>{x("b"),x("c")},"Dependency discovery deduplicates stable refs in first-use order");
}

void mutation_history_and_driven_sources() {
    Session session(fixture());const auto original=session.document();
    apply(session,{SetExpression{{x("a"),x("c")},{reference(x("b"))+"*2+10"}}});
    near(value(session,x("a")),410,"Batch expression first target");near(value(session,x("c")),410,"Batch expression second target");
    check(session.history().states.size()==2&&session.history().states.back().label.find("Expression")!=std::string::npos,"One expression batch is one labelled History entry");
    apply(session,{Set{x("b"),250}});near(value(session,x("a")),510,"Formula follows source edits");
    atomic_reject(session,"DRIVEN_PROPERTY",{Set{x("a"),4}});atomic_reject(session,"DRIVEN_PROPERTY",{EditProperties{{x("a"),x("c")},1,true}});
    apply(session,{UnlinkProperties{{x("a"),x("c")}},Set{x("b"),80}});
    near(value(session,x("a")),510,"Batch Unlink freezes before source changes");near(value(session,x("c")),510,"Batch Unlink shares one initial snapshot");
    check(!property(session.document(),x("a")).expression,"Unlink clears the expression source");
    apply(session,{Link{x("a"),{x("b"),1,0,"copy_local_value"}}});
    atomic_reject(session,"DRIVEN_PROPERTY",{formula(x("a"),"1+1")});
    apply(session,{formula(x("a"),"1+1",true)});check(!property(session.document(),x("a")).binding,"Explicit expression replacement removes a Binding");
    const auto expression_revision=session.revision();apply(session,{formula(x("a"),"2")});
    check(session.revision()==expression_revision+1&&property(session.document(),x("a")).expression->source=="2","Same-result formula replacement remains an authored edit");
    const auto current=session.document();rejects("REVISION_CONFLICT",[&]{session.apply({formula(x("a"),"3")},expression_revision);});
    check(session.document()==current,"Stale formula replacement cannot overwrite newer authored text");
    session.undo(session.revision());check(property(session.document(),x("a")).expression->source=="1+1","Undo restores exact expression text");
    apply(session,{LinkProperties{{x("a")},x("b"),true}});
    check(!property(session.document(),x("a")).expression&&property(session.document(),x("a")).binding,"Explicit batch Link replaces a formula");
    near(value(session,x("a")),2,"Relative Link captures the expression's evaluated starting difference");
    apply(session,{formula(x("a"),"9",true),Link{x("a"),{x("b"),1,0,"copy_local_value"}}});
    check(!property(session.document(),x("a")).expression,"Single Link explicitly replaces a formula");

    Session gesture(original);gesture.begin_gesture(0);gesture.update_gesture({formula(x("a"),"42")});gesture.cancel_gesture();
    check(gesture.document()==original&&!gesture.can_undo(),"Cancelled expression preview preserves the original state");
    gesture.begin_gesture(0);gesture.update_gesture({formula(x("a"),"42")});gesture.commit_gesture();gesture.undo(gesture.revision());
    check(gesture.document()==original,"Committed formula gesture undoes in one step");
    Session small(original),large(original);apply(small,{formula(x("a"),"2")});apply(large,{formula(x("a"),std::string(2000,' ')+"2")});
    check(large.history().retained_bytes>=small.history().retained_bytes+1900,"History estimate includes owned expression source storage");
}

std::string balanced(std::vector<std::string> terms) {
    while(terms.size()>1) {std::vector<std::string> next;for(std::size_t i=0;i<terms.size();i+=2)next.push_back(i+1<terms.size()?"("+terms[i]+"+"+terms[i+1]+")":terms[i]);terms=std::move(next);}return terms.front();
}
void errors_and_limits() {
    Session session(fixture());
    for(const auto* expression:{"1e+","0x10","NaN","random(1)","1;2","min(1)","max(1,2,3)","ref(\"a\",\"a-point\")","1e999"})
        atomic_reject(session,"EXPRESSION_SYNTAX",{formula(x("a"),expression)});
    for(const auto* expression:{"1/0","sqrt(-1)","clamp(1,3,2)"})atomic_reject(session,"EXPRESSION_DOMAIN",{formula(x("a"),expression)});
    atomic_reject(session,"NON_FINITE",{formula(x("a"),"1e308*10")});
    atomic_reject(session,"OUT_OF_RANGE",{formula(x("a"),"1e10")});
    atomic_reject(session,"EXPRESSION_LIMIT",{formula(x("a"),std::string(4096,' ')+"1")});
    atomic_reject(session,"EXPRESSION_LIMIT",{formula(x("a"),std::string(33,'(')+"1"+std::string(33,')'))});
    atomic_reject(session,"EXPRESSION_LIMIT",{formula(x("a"),balanced(std::vector<std::string>(129,"1")))});
    atomic_reject(session,"EXPRESSION_LIMIT",{formula(x("a"),balanced(std::vector<std::string>(65,reference(x("b")))))});
    atomic_reject(session,"UNSUPPORTED_EXPRESSION_VERSION",{SetExpression{{x("a")},{"1",2}}});
    atomic_reject(session,"MISSING_REFERENCE",{formula(x("a"),reference(x("absent"))+"*0")});
    atomic_reject(session,"DEPENDENCY_CYCLE",{formula(x("a"),reference(x("b"))),formula(x("b"),reference(x("a")))});
    atomic_reject(session,"DUPLICATE_TARGET",{SetExpression{{x("a"),x("a")},{"1"}}});
    atomic_reject(session,"INVALID_BATCH",{SetExpression{{},{"1"}}});
    atomic_reject(session,"UNIT_MISMATCH",{SetExpression{{x("a"),tf("b","a")},{"1"}}});
    auto conflicting=fixture();conflicting.objects.at("a").contours[0].points[0].x={100,Binding{x("b"),1,0,"copy_local_value"},Expression{"1"}};
    rejects("SCALAR_SOURCE_CONFLICT",[&]{Session invalid(conflicting);});
    apply(session,{formula(x("a"),reference(x("b")))});atomic_reject(session,"MISSING_REFERENCE",{DeleteObjects{{"b"}}});
    apply(session,{Unlink{x("a")},DeleteObjects{{"b"}}});near(value(session,x("a")),200,"Explicit freeze allows dependency deletion in one transaction");
}

Session primitive_scene() {
    Session session(fixture());apply(session,{CreatePrimitive{"composition","","shape","Polygon",default_primitive("source","nect.shape.polygon")},Set{tf("b","a"),5}});return session;
}
void generated_points_and_conversion() {
    auto session=primitive_scene();const Ref count{"shape","","generator.points"},fifth{"shape","source-outer-1-5","x"};
    apply(session,{formula(count,reference(tf("b","a")))});
    rejects("EVALUATION_REQUIRED",[&]{(void)path_contours(session.document().objects.at("shape"));});
    apply(session,{formula(fifth,reference(x("b"))+"+10")});near(value(session,fifth),210,"Generated-point formula creates an absolute Point Edit");
    apply(session,{Set{tf("b","a"),10}});near(value(session,fifth),210,"Rational generated identity survives expression-dependent count changes");
    apply(session,{EnablePointEdit{"shape",false}});
    atomic_reject(session,"UNRESOLVED_POINT_EDIT",{Set{tf("b","a"),6}});
    const auto corrected=session.document();apply(session,{ClearPointEdit{"shape"},Set{tf("b","a"),6}});session.undo(session.revision());
    check(session.document()==corrected,"Undo restores formula, dormant correction and topology after explicit clear/count edit");
    apply(session,{EnablePointEdit{"shape",true}});
    apply(session,{formula(fifth,reference({"shape","","generator.radius"})+"+1")});
    check(conversion_blockers(session.document(),"shape")==std::vector<Ref>{fifth},"Conversion discovers generator dependencies inside formulas");
    atomic_reject(session,"CONVERSION_REFERENCE",{ConvertToPath{"shape"}});
    apply(session,{formula(fifth,reference(x("b"))+"+10"),ConvertToPath{"shape"}});
    check(!session.document().objects.at("shape").source&&property(session.document(),fifth).expression.has_value(),"Conversion preserves active formula and stable point identity");
    apply(session,{Set{x("b"),300}});near(value(session,fifth),310,"Converted point formula remains live");

    auto cycle=primitive_scene();
    atomic_reject(cycle,"DEPENDENCY_CYCLE",{formula(count,reference(tf("b","a"))),formula(tf("b","a"),reference(fifth)+"/"+reference(x("a")))});
}

void bypass_and_transform_projection() {
    auto initial=primitive_scene();const Ref east{"shape","source-outer-0-1","x"};
    apply(initial,{formula(east,"123"),EnablePointEdit{"shape",false}});const auto bypassed=initial.document();
    auto dormant=[&](std::string source) {auto document=bypassed;document.objects.at("shape").point_edit->overrides.at(east.point).at("x").expression=Expression{std::move(source)};return document;};
    Session runtime(dormant("1/0"));atomic_reject(runtime,"EXPRESSION_DOMAIN",{EnablePointEdit{"shape",true}});
    Session cycle(dormant(reference(east)));atomic_reject(cycle,"DEPENDENCY_CYCLE",{EnablePointEdit{"shape",true}});
    rejects("EXPRESSION_SYNTAX",[&]{Session invalid(dormant("1+"));});
    rejects("MISSING_REFERENCE",[&]{Session invalid(dormant(reference(x("missing"))));});
    rejects("UNIT_MISMATCH",[&]{Session invalid(dormant(reference(tf("b","a"))));});
    apply(runtime,{Unlink{east}});check(!property(runtime.document(),east).expression,"Unlink freezes generated fallback before enabling the dormant correction");
    near(value(runtime,east),0,"Bypassed polygon's default top point X is preserved on Unlink");

    auto document=fixture();document.objects.at("b").transform_parent="a";
    document.objects.at("a").transform[0].expression=Expression{"cos(60)"};
    document.objects.at("a").transform[4].expression=Expression{reference(x("c"))};
    Session transformed(document);const auto before=evaluate_transforms(document,evaluate(document));
    apply(transformed,{TranslateObjects{{"b"},20,30}});
    const auto after=evaluate_transforms(transformed.document(),evaluate(transformed.document()));
    near(after.at("b").world[4],before.at("b").world[4]+20,"Requested-root transform evaluation follows expressions");
    near(after.at("b").world[5],before.at("b").world[5]+30,"Expression-driven parent matrix participates in world translation");
    atomic_reject(transformed,"DRIVEN_PROPERTY",{TranslateObjects{{"a"},1,0}});
}

void color_sources() {
    auto document=fixture();NamedColor palette;palette.id="palette";palette.name="Palette";document.named_colors.emplace(palette.id,palette);
    NamedColor source;source.id="palette-source";source.name="Source";source.rgba[0].literal=.25;document.named_colors.emplace(source.id,source);
    Session session(document);const Ref red{"palette","","color.r"};apply(session,{formula(red,".5")});
    atomic_reject(session,"DRIVEN_PROPERTY",{SetColor{{"palette","","color"},ColorValue{}}});
    apply(session,{LinkColor{{"palette","","color"},{"palette-source","","color"}}});
    check(!property(session.document(),red).expression&&color_link(session.document(),{"palette","","color"}).has_value(),"LinkColor explicitly replaces formula channels");
    apply(session,{formula(red,".75",true),UnlinkColor{{"palette","","color"}}});
    check(!property(session.document(),red).expression&&!property(session.document(),red).binding,"UnlinkColor clears all source types");near(value(session,red),.75,"UnlinkColor freezes formula output");
}
}
int main() {
    try {
        arithmetic_and_units();mutation_history_and_driven_sources();errors_and_limits();generated_points_and_conversion();bypass_and_transform_projection();color_sources();
        std::cout<<"PASS "<<checks<<" expression grammar, units, dependencies, source replacement, conversion and History checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
