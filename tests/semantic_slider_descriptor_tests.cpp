#include "nect/semantic_controls.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace nect;
namespace {
int checks=0;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
void refuses(const SemanticParameterDescriptor& descriptor){
    try{(void)validate_semantic_descriptor(descriptor);throw std::runtime_error("Invalid slider accepted");}
    catch(const Error& error){check(error.code=="INVALID_CONTROL_DESCRIPTOR","Invalid slider has a typed descriptor refusal");}
}
}
int main(){try{
    const auto dropdown=*builtin_semantic_descriptor("nect.paint.fill","fill_rule");
    check(dropdown.enum_default=="nonzero"&&dropdown.choices.size()==2,"Fill rule has typed canonical choices");
    check(validate_semantic_descriptor(dropdown).widget==SemanticWidget::dropdown,"Fill rule resolves to dropdown");
    auto bad_enum=dropdown;bad_enum.enum_default="unknown";refuses(bad_enum);
    bad_enum=dropdown;bad_enum.choices.push_back(bad_enum.choices.front());refuses(bad_enum);
    bad_enum=dropdown;bad_enum.widget_hint="toggle";refuses(bad_enum);
    bad_enum=dropdown;bad_enum.boolean_default=false;refuses(bad_enum);
    bad_enum=dropdown;bad_enum.widget_hint="future-enum";
    check(validate_semantic_descriptor(bad_enum).fallback,"Unknown enum hint has same-type fallback");
    const auto toggle=*builtin_semantic_descriptor("nect.shape.offset","enabled");
    check(toggle.value_type=="boolean"&&toggle.boolean_default==true,"Enabled retains typed canonical default");
    check(validate_semantic_descriptor(toggle).widget==SemanticWidget::toggle,"Enabled resolves to toggle");
    auto bad_toggle=toggle;bad_toggle.boolean_default.reset();refuses(bad_toggle);
    bad_toggle=toggle;bad_toggle.minimum=0;refuses(bad_toggle);
    bad_toggle=toggle;bad_toggle.widget_hint="numeric";refuses(bad_toggle);
    bad_toggle=toggle;bad_toggle.unit="scalar";refuses(bad_toggle);
    bad_toggle=toggle;bad_toggle.widget_hint="future-bool";
    check(validate_semantic_descriptor(bad_toggle).widget==SemanticWidget::toggle&&validate_semantic_descriptor(bad_toggle).fallback,"Unknown boolean hint falls back only within boolean family");
    const auto copies=*builtin_semantic_descriptor("nect.shape.repeater","copies");
    check(copies.widget_hint=="slider"&&copies.minimum==0&&copies.maximum==1000&&copies.step==1,
        "Copies describes one bounded shared numeric slider");
    check(validate_semantic_descriptor(copies).widget==SemanticWidget::slider,"Slider resolves to the standard widget");
    auto invalid=copies;invalid.minimum.reset();refuses(invalid);
    invalid=copies;invalid.maximum.reset();refuses(invalid);
    invalid=copies;invalid.step.reset();refuses(invalid);
    invalid=copies;invalid.step=0;refuses(invalid);
    invalid=copies;invalid.maximum=2000000;refuses(invalid);
    invalid=copies;invalid.maximum=invalid.minimum;refuses(invalid);
    invalid=copies;invalid.maximum=std::numeric_limits<double>::infinity();refuses(invalid);
    invalid=copies;invalid.value_type="text";refuses(invalid);
    invalid=copies;invalid.angle_semantics="signed_turns_indicator_modulo_360";refuses(invalid);
    invalid=copies;invalid.default_value=1001;refuses(invalid);
    auto future=copies;future.widget_hint="future-number-control";
    check(validate_semantic_descriptor(future).fallback&&validate_semantic_descriptor(future).widget==SemanticWidget::numeric,
        "Unknown hint retains safe same-type numeric fallback");
    check(validate_semantic_descriptor(*builtin_semantic_descriptor("nect.shape.offset","amount")).widget==SemanticWidget::numeric,
        "Offset numeric control is unchanged");
    check(validate_semantic_descriptor(*builtin_semantic_descriptor("nect.shape.repeater","rotation")).widget==SemanticWidget::angle,
        "Signed-turn angle control is unchanged");
    Session session(empty_document("slider-doc","composition","artboard"));
    session.apply({CreatePrimitive{"composition","","target","Target",default_primitive("source","nect.shape.rectangle")}},0);
    MacroDefinition definition;definition.id="definition";definition.label="Slider Macro";
    MacroDefinitionRevision first;first.input={"input","local_paths_and_paint"};first.output={"output","local_paths_and_paint"};
    first.nodes={{default_operation("offset","nect.shape.offset"),"offset-in","offset-out"},
        {default_operation("repeat","nect.shape.repeater"),"repeat-in","repeat-out"}};
    first.edges={{{"","input"},{"offset","offset-in"}},{{"offset","offset-out"},{"repeat","repeat-in"}},
        {{"repeat","repeat-out"},{"","output"}}};first.output_mapping={"repeat","repeat-out"};
    first.public_parameters={{"macro.offset.amount","Amount","offset","amount","number","du","local_paths_and_paint"}};
    definition.revisions.emplace(1,first);auto second=first;second.revision=2;second.interface_version=2;
    second.nodes.back().operation.parameters.at("copies").literal=37;
    second.public_parameters.push_back({"macro.copies","Custom copies","repeat","copies","number","scalar","local_paths_and_paint"});
    definition.revisions.emplace(2,second);definition.latest_revision=2;
    session.apply({MacroCommand{CreateMacroDefinition{definition}},MacroCommand{InstantiateMacro{"target","definition","instance",2,0}}},session.revision());
    const auto published=macro_semantic_descriptor(session.document(),{"target","instance","macro.copies"});
    check(published.widget_hint==copies.widget_hint&&published.unit==copies.unit&&published.minimum==copies.minimum&&
        published.maximum==copies.maximum&&published.step==copies.step&&published.default_value==37&&
        published.key=="macro.copies"&&published.label=="Custom copies",
        "Published control derives the same slider metadata and retains its public identity, label and mapped default");
    std::cout<<"Semantic slider descriptor checks: "<<checks<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
