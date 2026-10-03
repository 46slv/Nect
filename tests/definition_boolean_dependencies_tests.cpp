#include "nect/core.hpp"
#include <iostream>
#include <stdexcept>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& message) {
    if(!ok)throw std::runtime_error(message);
    ++checks;
}
enum class Family { isolation, italic, operation };
std::string field(Family family,const Id& object="target") {
    if(family==Family::isolation)return "composite.isolated";
    if(family==Family::italic)return "text.italic";
    return "op."+object+"-fill.enabled";
}
Expression reference(Family family,const Id& source,bool inverted) {
    return {(inverted?" ! ":" ")+std::string("ref ( \"")+source+
        "\" , \"\" , \""+field(family,source)+"\" ) ",1};
}
Object item(const Id& id,Family family) {
    Object object;object.id=id;object.name=id;
    if(family==Family::isolation)object.kind=Kind::group;
    else if(family==Family::italic) {
        object.kind=Kind::text;object.text=default_text(id+"-text","");
    } else {
        object.source=default_primitive(id+"-shape","nect.shape.rectangle");
        auto fill=default_operation(id+"-fill","nect.paint.fill");
        fill.enabled=false;object.stack.push_back(fill);
    }
    return object;
}
Document fixture(Family family,const Expression& expression) {
    auto document=empty_document("bool-definition-doc","comp","art");
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;
    root.children={"control","target"};
    auto target=item("target",family);
    if(family==Family::isolation)target.compositing.isolated_expression=expression;
    else if(family==Family::italic)target.text->italic_driver=expression;
    else target.stack.front().enabled_expression=expression;
    for(const auto& object:{root,item("control",family),target,item("outside",family)})
        document.objects.emplace(object.id,object);
    document.compositions.front().roots={"source-root","outside"};
    return document;
}
Expression authored(const Document& document,Family family) {
    const auto& target=document.objects.at("target");
    if(family==Family::isolation)return *target.compositing.isolated_expression;
    if(family==Family::italic)return std::get<Expression>(*target.text->italic_driver);
    return *target.stack.front().enabled_expression;
}
bool evaluated(const Document& document,Family family) {
    if(family==Family::isolation)
        return composite_isolation_state(document,{"target","","composite.isolated"}).evaluated;
    if(family==Family::italic)return evaluate_text_italic(document,"target");
    return operation_enabled_state(document,operation_ref("target","target-fill","enabled")).evaluated;
}
void create_definition(Session& session) {
    session.apply({DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}}},session.revision());
}
void accepts(Family family,const Expression& expression,bool expected) {
    Session session(fixture(family,expression));const auto before=session.document().objects;
    create_definition(session);
    check(session.document().definitions.contains("definition"),field(family)+": closed boolean dependency accepted");
    check(session.document().objects==before&&authored(session.document(),family)==expression,
        field(family)+": Definition creation preserves exact authored expression and source objects");
    check(evaluated(session.document(),family)==expected,field(family)+": typed boolean expression retains its value");
}
void rejects_escape(Family family,bool inverted) {
    const auto expression=reference(family,"outside",inverted);
    Session session(fixture(family,expression));const auto before=session.document();
    const auto revision=session.revision();const auto history=session.history();
    try {create_definition(session);}
    catch(const Error& error) {
        check(error.code=="DEFINITION_DEPENDENCY_ESCAPE",field(family)+": expected dependency escape, got "+error.code);
        check(error.references==std::vector<Ref>{{"outside","",field(family,"outside")}},
            field(family)+": dependency refusal identifies the exact external Ref");
        check(session.document()==before&&session.revision()==revision&&session.history()==history&&
            authored(session.document(),family)==expression,
            field(family)+": failed Definition creation preserves Document, revision, history and expression");
        return;
    }
    throw std::runtime_error(field(family)+": external boolean dependency was accepted");
}
}
int main() {
    try {
        for(const auto family:{Family::isolation,Family::italic,Family::operation}) {
            accepts(family,reference(family,"control",false),false);
            accepts(family,reference(family,"control",true),true);
            accepts(family,{" true ",1},true);
            accepts(family,{" false ",1},false);
            rejects_escape(family,false);
            rejects_escape(family,true);
        }
        std::cout<<"PASS: "<<checks<<" Definition boolean dependency checks\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return 1;
    }
}
