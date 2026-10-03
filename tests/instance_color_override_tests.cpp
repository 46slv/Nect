#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <process.h>
#endif

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& message) {
    if(!ok)throw std::runtime_error(message);
    ++checks;
}
void apply(Session& session,std::vector<Command> commands) {
    session.apply(commands,session.revision());
}
template<class F> void rejects(const char* code,F action) {
    try{action();}catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
struct State {
    std::string native;
    std::uint64_t revision;
    HistoryInfo history;
};
State state(const Session& session) {
    return {encode(session.document()),session.revision(),session.history()};
}
void unchanged(const Session& session,const State& before,const char* message) {
    check(encode(session.document())==before.native&&session.revision()==before.revision&&
        session.history()==before.history,message);
}
void atomic(Session& session,const char* code,std::vector<Command> commands,
    std::optional<std::uint64_t> expected={}) {
    const auto before=state(session);
    rejects(code,[&]{session.apply(commands,expected.value_or(session.revision()));});
    unchanged(session,before,"Refusal preserves exact native bytes, revision and history");
}
ColorValue color(double r,double g,double b,double a=1) {
    ColorValue value;value.rgba={r,g,b,a};return value;
}
const ColorValue source_color=color(.125,.375,.625,.875);
const ColorValue local_color=color(.75,.25,.125,.5);
const ColorValue sibling_color=color(.25,.75,.5,.25);
Ref fill(const Id& object) {return operation_ref(object,object+"-fill","color");}
Object rectangle(const Id& id,double y=0) {
    Object object;object.id=id;object.name=id;
    object.source=default_primitive(id+"-rectangle","nect.shape.rectangle");
    object.source->parameters.at("width").literal=30;
    object.source->parameters.at("height").literal=20;
    object.transform[5].literal=y;
    auto operation=default_operation(id+"-fill","nect.paint.fill");
    const std::array<const char*,4> channels{"r","g","b","a"};
    for(std::size_t i=0;i<channels.size();++i)
        operation.parameters.at(channels[i]).literal=source_color.rgba[i];
    object.stack.push_back(operation);return object;
}
Document source_document(bool driven=false) {
    auto document=empty_document("color-override-doc","comp","source-board");
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;
    root.children={"driver","logo","peer","folder","gradient"};root.transform[4].literal=10;
    auto driver=rectangle("driver"),logo=rectangle("logo",25),peer=rectangle("peer",50);
    logo.stack.push_back(default_operation("logo-stroke","nect.paint.stroke"));
    if(driven) {
        const std::array<const char*,4> channels{"r","g","b","a"};
        for(const auto* channel:channels) {
            logo.stack[0].parameters.at(channel).binding=Binding{operation_ref("driver","driver-fill",channel)};
            peer.stack[0].parameters.at(channel).expression=Expression{
                std::string(" ref ( \"driver\" , \"\" , \"op.driver-fill.")+channel+"\" ) ",1};
        }
    }
    Object folder;folder.id="folder";folder.name="Folder";folder.kind=Kind::group;
    folder.children={"nested"};
    auto nested=rectangle("nested",75),gradient=rectangle("gradient",100),outside=rectangle("outside",130);
    Gradient ramp;ramp.id="gradient-ramp";
    GradientStop first;first.id="gradient-first";
    GradientStop last;last.id="gradient-last";last.offset.literal=1;
    ramp.stops={first,last};gradient.stack[0].gradient=ramp;
    for(const auto& object:{root,driver,logo,peer,folder,nested,gradient,outside})
        document.objects.emplace(object.id,object);
    NamedColor palette;palette.id="palette";palette.name="Palette";
    document.named_colors.emplace(palette.id,palette);
    document.compositions.front().roots={"source-root","outside"};return document;
}
Document fixture(bool driven=false) {
    Session session(source_document(driven));
    apply(session,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        DefinitionCommand{CreateInstance{"comp","","instance-a","definition","A"}},
        DefinitionCommand{CreateInstance{"comp","","instance-b","definition","B"}},
        Set{{"instance-a","","transform.tx"},120},Set{{"instance-b","","transform.tx"},240}});
    return session.document();
}
EvaluatedScene scene(const Document& document) {
    const auto values=evaluate(document);
    return evaluate_scene(document,"comp",values,evaluate_transforms(document,values));
}
Id proxy(const EvaluatedScene& evaluated,const Id& owner,const Id& source) {
    for(const auto& [id,instance]:evaluated.instance_owners)
        if(instance==owner&&evaluated.instance_sources.at(id)==source)return id;
    throw std::runtime_error("Missing occurrence: "+owner+" / "+source);
}
const EvaluatedSceneNode& node(const std::vector<EvaluatedSceneNode>& nodes,const Id& id) {
    for(const auto& item:nodes) {
        if(item.id==id)return item;
        if(!item.children.empty()) {
            try{return node(item.children,id);}catch(const std::out_of_range&){}
        }
    }
    throw std::out_of_range("Missing scene node: "+id);
}
const DefinitionInstance& instance(const Document& document,const Id& id="instance-a") {
    return *document.objects.at(id).instance;
}
void same_sources(const Document& actual,const Document& expected) {
    for(const auto* id:{"source-root","driver","logo","peer","folder","nested","gradient","outside"})
        check(actual.objects.at(id)==expected.objects.at(id),"Authored source remains unchanged: "+Id{id});
    check(actual.named_colors==expected.named_colors,"Local Fill Color never changes the source palette");
}
void near(double actual,double expected,const std::string& message) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<1e-12,message);
}
void rendered_color(const EvaluatedScene& evaluated,const Id& object,const ColorValue& expected,
    const std::string& message) {
    const auto& authored=evaluated.expanded_document->objects.at(object);
    const auto& operation=authored.stack.front();
    check(operation.type=="nect.paint.fill"&&!operation.gradient,"Numeric oracle selects a solid Fill");
    const auto target=operation_ref(object,operation.id,"color");
    const auto projected=color_value(*evaluated.expanded_document,target,*evaluated.expanded_values);
    const auto& paints=evaluated.shapes.at(object).paints;
    const auto paint=std::find_if(paints.begin(),paints.end(),[&](const PaintLayer& item) {
        return item.operation==operation.id;
    });
    check(paint!=paints.end()&&paint->type=="nect.paint.fill"&&!paint->gradient,
        "Rendered scene has the selected solid Fill layer");
    for(std::size_t i=0;i<4;++i) {
        near(projected.rgba[i],expected.rgba[i],message+" projected channel "+std::to_string(i));
        near(paint->rgba[i],expected.rgba[i],message+" rendered channel "+std::to_string(i));
    }
}
void occurrence_color(const EvaluatedScene& evaluated,const Id& owner,const Id& source,
    const ColorValue& expected,const std::string& message) {
    rendered_color(evaluated,proxy(evaluated,owner,source),expected,message);
}
void literal_channels(const Document& document,const Id& object,const ColorValue& expected) {
    const auto& operation=document.objects.at(object).stack.front();
    const auto channels=color_channels(document,operation_ref(object,operation.id,"color"));
    for(std::size_t i=0;i<channels.size();++i) {
        const auto scalar=property(document,channels[i]);
        check(!scalar.binding&&!scalar.expression&&scalar.literal==expected.rgba[i],
            "Only selected materialized/projected channels are source-free local literals");
    }
}

void independent_projection_source_follow_and_reset() {
    Session session(fixture());const auto source=session.document();const auto before=state(session);
    apply(session,{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("logo"),local_color}},
        DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("peer"),sibling_color}},
        DefinitionCommand{SetInstanceOverride{"instance-a",{"logo","","transform.tx"},90}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","peer",false}}});
    const auto overridden=state(session);same_sources(session.document(),source);
    auto evaluated=scene(session.document());
    rendered_color(evaluated,"logo",source_color,"Plain source keeps its Fill");
    occurrence_color(evaluated,"instance-a","logo",local_color,"Selected occurrence uses local RGBA");
    occurrence_color(evaluated,"instance-b","logo",source_color,"Sibling occurrence inherits source RGBA");
    occurrence_color(evaluated,"instance-a","peer",sibling_color,"A second local Fill is independent");
    check(!node(evaluated.roots,proxy(evaluated,"instance-a","peer")).visible&&
        node(evaluated.roots,proxy(evaluated,"instance-b","peer")).visible,
        "Fill Color and visibility overrides keep independent state");
    near(evaluated.expanded_values->at({proxy(evaluated,"instance-a","logo"),"","transform.tx"}),90,
        "Fill Color and Scalar overrides apply together");
    const auto svg=export_svg(session.document(),"comp","source-board");
    check(svg.find("id=\""+proxy(evaluated,"instance-a","logo")+"\"")!=std::string::npos,
        "SVG exports the occurrence that uses evaluated local Fill");
    unchanged(session,overridden,"Render projection and SVG preserve exact authored bytes and history");
    session.undo(session.revision());check(encode(session.document())==before.native,"One Undo restores the entire mixed override batch");
    session.redo(session.revision());check(encode(session.document())==overridden.native,"One Redo restores exact mixed maps");

    const auto changed=color(.65,.4,.25,.8);
    apply(session,{SetColor{fill("logo"),changed},Rename{"logo","Renamed Logo"},
        ReorderObjects{"comp","source-root",{"folder","peer","logo","driver","gradient"}}});
    evaluated=scene(session.document());
    rendered_color(evaluated,"logo",changed,"Source color edit updates plain source");
    occurrence_color(evaluated,"instance-a","logo",local_color,"Source edit cannot replace local Fill");
    occurrence_color(evaluated,"instance-b","logo",changed,"Unmodified occurrence follows source color edit");
    check(instance(session.document()).color_overrides.at(fill("logo"))==local_color,
        "Source rename/reorder retains stable object and operation target identity");
    const auto before_reset=encode(session.document());
    apply(session,{DefinitionCommand{ResetInstanceColorOverride{"instance-a",fill("logo")}}});
    const auto after_reset=encode(session.document());evaluated=scene(session.document());
    occurrence_color(evaluated,"instance-a","logo",changed,"Reset resumes current source color");
    occurrence_color(evaluated,"instance-a","peer",sibling_color,"Selective Reset retains sibling local Fill");
    check(instance(session.document()).color_overrides==std::map<Ref,ColorValue>{{fill("peer"),sibling_color}}&&
        instance(session.document()).visibility_overrides.at("peer")==false&&
        instance(session.document()).overrides.at({"logo","","transform.tx"})==90,
        "Color Reset leaves sibling Color, visibility and Scalar maps intact");
    session.undo(session.revision());check(encode(session.document())==before_reset,"Undo restores selected removed Color override");
    session.redo(session.revision());check(encode(session.document())==after_reset,"Redo restores exact selective Reset result");
    const auto later=color(.1,.25,.75,.4);apply(session,{SetColor{fill("logo"),later}});
    occurrence_color(scene(session.document()),"instance-a","logo",later,"Later source edits remain live after Reset");
    apply(session,{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("nested"),source_color}}});
    check(instance(session.document()).color_overrides.contains(fill("nested")),
        "An explicit Color equal to its source still retains occurrence-local identity");
    apply(session,{SetColor{fill("nested"),later}});evaluated=scene(session.document());
    occurrence_color(evaluated,"instance-a","nested",source_color,"Equal-valued local Color stays independent after source changes");
    occurrence_color(evaluated,"instance-b","nested",later,"Equal-valued sibling without an override remains live");
    const auto endpoints=color(0,1,0,1);
    apply(session,{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("nested"),endpoints}}});
    occurrence_color(scene(session.document()),"instance-a","nested",endpoints,"Closed channel bounds zero and one are valid");
}

void driven_source_channels_remain_authored_and_reset_live() {
    Session session(fixture(true));const auto before=session.document();
    apply(session,{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("logo"),local_color}},
        DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("peer"),sibling_color}}});
    same_sources(session.document(),before);auto evaluated=scene(session.document());
    occurrence_color(evaluated,"instance-a","logo",local_color,"Local literal supersedes source channel Links");
    occurrence_color(evaluated,"instance-a","peer",sibling_color,"Local literal supersedes source channel Expressions");
    occurrence_color(evaluated,"instance-b","logo",source_color,"Sibling retains source Links");
    occurrence_color(evaluated,"instance-b","peer",source_color,"Sibling retains source Expressions");
    literal_channels(*evaluated.expanded_document,proxy(evaluated,"instance-a","logo"),local_color);
    literal_channels(*evaluated.expanded_document,proxy(evaluated,"instance-a","peer"),sibling_color);
    const auto linked=proxy(evaluated,"instance-b","logo"),expression=proxy(evaluated,"instance-b","peer");
    for(const auto* channel:{"r","g","b","a"}) {
        check(evaluated.expanded_document->objects.at(linked).stack[0].parameters.at(channel).binding.has_value()&&
            evaluated.expanded_document->objects.at(expression).stack[0].parameters.at(channel).expression.has_value(),
            "Unoverridden clone retains each remapped channel driver");
    }
    const auto changed=color(.7,.6,.5,.3);apply(session,{SetColor{fill("driver"),changed}});evaluated=scene(session.document());
    occurrence_color(evaluated,"instance-a","logo",local_color,"Driver edit leaves local linked-target override intact");
    occurrence_color(evaluated,"instance-a","peer",sibling_color,"Driver edit leaves local expression-target override intact");
    occurrence_color(evaluated,"instance-b","logo",changed,"Unoverridden channel Links follow source driver");
    occurrence_color(evaluated,"instance-b","peer",changed,"Unoverridden channel Expressions follow source driver");
    apply(session,{DefinitionCommand{ResetInstanceColorOverride{"instance-a",fill("logo")}}});evaluated=scene(session.document());
    occurrence_color(evaluated,"instance-a","logo",changed,"Reset restores current linked value");
    const auto reset_logo=proxy(evaluated,"instance-a","logo"),reset_driver=proxy(evaluated,"instance-a","driver");
    check(color_link(*evaluated.expanded_document,operation_ref(reset_logo,
        evaluated.expanded_document->objects.at(reset_logo).stack[0].id,"color"))==
        operation_ref(reset_driver,evaluated.expanded_document->objects.at(reset_driver).stack[0].id,"color"),
        "Reset restores stable correctly remapped aggregate channel Link");
    apply(session,{DefinitionCommand{ResetInstanceColorOverride{"instance-a",fill("peer")}}});
    const auto later=color(.05,.2,.95,.65);apply(session,{SetColor{fill("driver"),later}});evaluated=scene(session.document());
    occurrence_color(evaluated,"instance-a","logo",later,"Reset linked target follows later driver update");
    occurrence_color(evaluated,"instance-a","peer",later,"Reset expression target follows later driver update");
    check(session.document().objects.at("logo")==before.objects.at("logo")&&
        session.document().objects.at("peer")==before.objects.at("peer")&&
        session.document().objects.at("peer").stack[0].parameters.at("r").expression->source==
            " ref ( \"driver\" , \"\" , \"op.driver-fill.r\" ) ",
        "Set, evaluation and Reset preserve source channels, links and exact expression bytes");
}

void duplicate_template_and_detach() {
    Session duplicate(fixture());apply(duplicate,{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("logo"),local_color}},
        DefinitionCommand{SetInstanceOverride{"instance-a",{"logo","","generator.width"},55}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","peer",false}}});
    const auto original=duplicate.document();const auto original_bytes=encode(original);
    const DuplicateObjects command{{"instance-a"},"copy"};
    const auto copied=duplicated_roots(original,command).front();apply(duplicate,{command});
    check(instance(duplicate.document(),copied)==instance(original),
        "Ordinary Instance duplication retains source-keyed Color, Scalar and visibility maps");
    occurrence_color(scene(duplicate.document()),copied,"logo",local_color,"Duplicated occurrence renders copied local color");
    const auto duplicated=encode(duplicate.document());const auto cold=decode(duplicated);
    check(encode(cold)==duplicated,"Cold reopen preserves duplicate maps and native identities");
    occurrence_color(scene(cold),copied,"logo",local_color,"Cold duplicate renders local RGBA");
    duplicate.undo(duplicate.revision());check(encode(duplicate.document())==original_bytes,"Undo removes only ordinary duplicate");
    duplicate.redo(duplicate.revision());check(encode(duplicate.document())==duplicated,"Redo restores exact ordinary duplicate");
    apply(duplicate,{DefinitionCommand{ResetInstanceColorOverride{copied,fill("logo")}}});
    auto evaluated=scene(duplicate.document());
    occurrence_color(evaluated,copied,"logo",source_color,"Reset on duplicate resumes its source");
    occurrence_color(evaluated,"instance-a","logo",local_color,"Reset on duplicate leaves original local color");

    auto document=source_document();document.compositions.front().artboards.push_back({"target","Target",100,100,640,480});
    Session target(document);apply(target,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"template","T","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target","template",Id{"content"}}},
        DefinitionCommand{SetInstanceColorOverride{"content",fill("logo"),local_color}},
        ArtboardTemplateCommand{DuplicateTemplateArtboard{"comp","target","target-copy",700,100,2}}});
    check(instance(target.document(),"target-copy-content-1").color_overrides==instance(target.document(),"content").color_overrides,
        "Template artboard duplication retains occurrence-local source Ref keys");
    occurrence_color(scene(decode(encode(target.document()))),"target-copy-content-1","logo",local_color,
        "Cold duplicated Template content renders local RGBA");

    Session session(fixture(true));apply(session,{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("logo"),local_color}},
        DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("peer"),sibling_color}},
        DefinitionCommand{SetInstanceOverride{"instance-a",{"logo","","transform.tx"},90}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","peer",false}}});
    const auto live=session.document();const auto live_bytes=encode(live);apply(session,{DefinitionCommand{DetachInstance{"instance-a","detached"}}});
    const auto detached=session.document();const auto detached_bytes=encode(detached);
    const auto& placement=detached.objects.at("instance-a");const auto& root=detached.objects.at(placement.children.front());
    const auto logo=root.children[1],peer=root.children[2];
    check(placement.kind==Kind::group&&!placement.instance&&placement.transform[4].literal==120&&
        root.transform[4].literal==0,"Detach preserves occurrence placement and materializes the source tree");
    literal_channels(detached,logo,local_color);literal_channels(detached,peer,sibling_color);
    check(detached.objects.at(logo).transform[4].literal==90&&!detached.objects.at(peer).visible,
        "Whole Detach retains independent Scalar and visibility local state");
    same_sources(detached,live);evaluated=scene(detached);
    rendered_color(evaluated,logo,local_color,"Detached Fill renders frozen local RGBA");
    session.undo(session.revision());check(encode(session.document())==live_bytes,"One Undo of Detach restores exact live Color maps and drivers");
    session.redo(session.revision());check(encode(session.document())==detached_bytes,"One Redo of Detach restores exact materialized identities");
    check(encode(decode(detached_bytes))==detached_bytes,"Cold reopen preserves detached local channel literals");
    apply(session,{SetColor{fill("driver"),color(.9,.8,.7,.6)}});
    check(session.document().objects.at(logo)==detached.objects.at(logo)&&
        session.document().objects.at(peer)==detached.objects.at(peer),"Later source driver edits cannot alter detached local children");
    rendered_color(scene(session.document()),logo,local_color,"Detached local Fill stays frozen after source edits");
}

std::string quoted(const std::string& value) {
#ifdef _WIN32
    return "\""+value+"\"";
#else
    std::string result="'";
    for(const auto ch:value)result+=ch=='\''?"'\\''":std::string(1,ch);
    return result+"'";
#endif
}
void cold_read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);check(file.good(),"Cold process can read the saved native file");
    const std::string native{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    Session reopened(decode(native));
    check(encode(reopened.document())==native,"Fresh process preserves exact native 0.85 authored bytes");
    check(reopened.revision()==0&&!reopened.can_undo()&&!reopened.can_redo(),
        "Cold process begins with an independent Session and no warm History");
    auto evaluated=scene(reopened.document());
    const auto changed=color(.375,.125,.875,.75);
    if(reopened.document().objects.at("instance-a").instance) {
        check(instance(reopened.document()).color_overrides==
            std::map<Ref,ColorValue>{{fill("logo"),local_color},{fill("peer"),sibling_color}},
            "Cold process retains exact authored local Color keys and values");
        occurrence_color(evaluated,"instance-a","logo",local_color,"Cold live occurrence recomputes local Color");
        occurrence_color(evaluated,"instance-a","peer",sibling_color,"Cold live occurrence recomputes sibling local Color");
        occurrence_color(evaluated,"instance-b","logo",source_color,"Cold sibling recomputes source inheritance");
        check(instance(reopened.document()).overrides.at({"logo","","transform.tx"})==90&&
            !instance(reopened.document()).visibility_overrides.at("peer"),
            "Cold process retains independent Scalar and visibility maps");
        apply(reopened,{SetColor{fill("driver"),changed}});evaluated=scene(reopened.document());
        occurrence_color(evaluated,"instance-a","logo",local_color,"Cold local Color survives a fresh driver edit");
        occurrence_color(evaluated,"instance-b","logo",changed,"Cold sibling has a live remapped driver");
        apply(reopened,{DefinitionCommand{ResetInstanceColorOverride{"instance-a",fill("logo")}}});
        evaluated=scene(reopened.document());
        occurrence_color(evaluated,"instance-a","logo",changed,"Cold Reset restores current source driver");
        occurrence_color(evaluated,"instance-a","peer",sibling_color,"Cold Reset remains selective");
    } else {
        const auto& placement=reopened.document().objects.at("instance-a");
        check(placement.kind==Kind::group,"Cold detached occurrence is an ordinary materialized Group");
        const auto& root=reopened.document().objects.at(placement.children.front());
        const auto logo=root.children[1],peer=root.children[2];
        literal_channels(reopened.document(),logo,local_color);literal_channels(reopened.document(),peer,sibling_color);
        rendered_color(evaluated,logo,local_color,"Cold detached Color recomputes source-free local RGBA");
        check(reopened.document().objects.at(logo).transform[4].literal==90&&
            !reopened.document().objects.at(peer).visible,"Cold detached tree retains mixed local state");
        apply(reopened,{SetColor{fill("driver"),changed}});evaluated=scene(reopened.document());
        rendered_color(evaluated,logo,local_color,"Cold detached Color stays frozen after fresh source edit");
        occurrence_color(evaluated,"instance-b","logo",changed,"Cold non-detached sibling still follows source");
    }
}
void cold_replay(const std::string& executable,const Document& document) {
    const auto path=std::filesystem::temp_directory_path()/
        ("nect instance-color 0.85 "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".nect");
    {std::ofstream file(path,std::ios::binary);file<<encode(document);check(file.good(),"Native file is saved before cold replay");}
#ifdef _WIN32
    // Use the CRT spawn API instead of cmd.exe outer-quote parsing. Quote argv
    // values so both executable and native paths may contain spaces.
    const auto result=_spawnl(_P_WAIT,executable.c_str(),quoted(executable).c_str(),
        "--cold-read",quoted(path.string()).c_str(),static_cast<const char*>(nullptr));
#else
    const auto result=std::system((quoted(executable)+" --cold-read "+quoted(path.string())).c_str());
#endif
    std::filesystem::remove(path);
    check(result==0,"A fresh process cold-reopens 0.85 and verifies projected Color, source follow and Reset/Detach");
}
void fresh_process_live_and_detached(const std::string& executable) {
    Session session(fixture(true));
    apply(session,{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("logo"),local_color}},
        DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("peer"),sibling_color}},
        DefinitionCommand{SetInstanceOverride{"instance-a",{"logo","","transform.tx"},90}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","peer",false}}});
    const auto before=state(session);cold_replay(executable,session.document());
    unchanged(session,before,"Fresh-process replay cannot mutate the live parent Session");
    apply(session,{DefinitionCommand{DetachInstance{"instance-a","cold-detached"}}});
    const auto detached=state(session);cold_replay(executable,session.document());
    unchanged(session,detached,"Fresh-process detached replay leaves parent exact bytes and History intact");
}

std::string versioned(std::string native,const char* version) {
    const auto marker=test_support::current_native_version_marker();const auto at=native.find(marker);
    check(at!=std::string::npos,"Fixture uses current native writer");
    native.replace(at,marker.size(),std::string("\"version\":\"")+version+"\"");return native;
}
std::string color_json(const ColorValue& value) {
    std::ostringstream out;out<<std::setprecision(17)<<R"({"space":"srgb","profile":"srgb","alpha":"straight","rgba":[)";
    for(std::size_t i=0;i<4;++i){if(i)out<<',';out<<value.rgba[i];}out<<"]}";return out.str();
}
std::string ref_json(const Ref& ref) {
    return "{\"object\":\""+ref.object+"\",\"point\":\""+ref.point+"\",\"field\":\""+ref.field+"\"}";
}
std::string native_entry(const Ref& ref,const ColorValue& value) {
    return "{\"target\":"+ref_json(ref)+",\"value\":"+color_json(value)+"}";
}
std::string color_array(std::string native,const std::string& entries) {
    const std::string field="\"color_overrides\":[";const auto at=native.find(field);
    check(at!=std::string::npos,"Native fixture has the optional Color override array");
    const auto begin=at+field.size();std::size_t end=begin,depth=1;bool quoted=false,escaped=false;
    for(;end<native.size();++end) {
        const auto ch=native[end];
        if(quoted){if(escaped)escaped=false;else if(ch=='\\')escaped=true;else if(ch=='"')quoted=false;continue;}
        if(ch=='"')quoted=true;else if(ch=='[')++depth;else if(ch==']'&&--depth==0)break;
    }
    check(end<native.size(),"Native fixture has a closed nested Color array");
    native.replace(begin,end-begin,entries);return native;
}
void native_contract_and_atomic_refusals() {
    Session session(fixture());const auto legacy_document=session.document();
    check(std::string(native_version)=="0.85","Fill Color overrides remain supported by the native 0.85 writer");
    check(encode(legacy_document).find("\"color_overrides\"")==std::string::npos,
        "Empty Color maps omit the optional native field");
    check(decode(versioned(encode(legacy_document),"0.83"))==legacy_document,
        "Native 0.83 without the new optional field remains readable");
    apply(session,{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("logo"),local_color}}});
    const auto saved=encode(session.document());
    check(saved.find("\"color_overrides\":[{\"target\":"+ref_json(fill("logo"))+
        ",\"value\":{\"space\":\"srgb\",\"profile\":\"srgb\",\"alpha\":\"straight\",\"rgba\":[")!=std::string::npos,
        "Native writer serializes exact source Ref and structured ColorValue");
    const auto reopened=decode(saved);check(reopened==session.document()&&encode(reopened)==saved,
        "Native 0.85 cold decode restores authored Color maps without drift");
    occurrence_color(scene(reopened),"instance-a","logo",local_color,"Cold reopen renders local Color");
    occurrence_color(scene(reopened),"instance-b","logo",source_color,"Cold reopen retains independent sibling inheritance");
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(saved,"0.83"));});
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(color_array(saved,""),"0.83"));});
    const auto entry=native_entry(fill("logo"),local_color);
    rejects("DUPLICATE_OVERRIDE_KEY",[&]{(void)decode(color_array(saved,entry+","+native_entry(fill("logo"),sibling_color)));});
    rejects("DUPLICATE_KEY",[&]{(void)decode(color_array(saved,"{\"target\":"+ref_json(fill("logo"))+
        ",\"value\":"+color_json(local_color)+",\"value\":"+color_json(sibling_color)+"}"));});
    rejects("UNKNOWN_FIELD",[&]{(void)decode(color_array(saved,"{\"target\":"+ref_json(fill("logo"))+
        ",\"value\":"+color_json(local_color)+",\"source\":\"logo\"}"));});
    rejects("UNSUPPORTED_OVERRIDE",[&]{(void)decode(color_array(saved,native_entry(operation_ref("logo","logo-stroke","color"),local_color)));});
    rejects("DANGLING_OVERRIDE",[&]{(void)decode(color_array(saved,native_entry(fill("outside"),local_color)));});
    rejects("DANGLING_OVERRIDE",[&]{(void)decode(color_array(saved,native_entry(fill("missing"),local_color)));});
    auto malformed=local_color;malformed.rgba[3]=1.01;
    rejects("OUT_OF_RANGE",[&]{(void)decode(color_array(saved,native_entry(fill("logo"),malformed)));});
    const std::string bad_format="{\"target\":"+ref_json(fill("logo"))+
        R"(,"value":{"space":"srgb","profile":"display-p3","alpha":"straight","rgba":[0,1,0,1]}})";
    rejects("UNSUPPORTED_COLOR",[&]{(void)decode(color_array(saved,bad_format));});
    const auto live_state=state(session);
    rejects("DUPLICATE_KEY",[&]{(void)decode(color_array(saved,std::string("{\"target\":")+
        R"({"object":"logo","point":"","field":"op.logo-fill.color","field":"op.logo-stroke.color"})"+
        ",\"value\":"+color_json(local_color)+"}"));});
    unchanged(session,live_state,"Failed independent native readers leave the live Session unchanged");

    const std::vector<Ref> unsupported{{"source-root","","op.source-root-fill.color"},
        {"logo","","op.logo-stroke.color"},fill("gradient"),
        gradient_ref("gradient","gradient-fill","gradient-ramp","stop.gradient-first.color"),
        {"logo","","op.logo-fill.r"},{"logo","","op.logo-fill.a"},
        {"logo","","generator.width"},{"logo","","composite.opacity"},
        {"logo","","color"},{"logo","","op.missing.color"}};
    for(const auto& ref:unsupported)
        atomic(session,"UNSUPPORTED_OVERRIDE",{DefinitionCommand{SetInstanceColorOverride{"instance-a",ref,local_color}}});
    for(const auto& ref:std::vector<Ref>{fill("missing"),fill("outside"),{"palette","","color"},
        {"source-board","","artboard.background"},fill(proxy(scene(session.document()),"instance-a","logo"))})
        atomic(session,"DANGLING_OVERRIDE",{DefinitionCommand{SetInstanceColorOverride{"instance-a",ref,local_color}}});
    atomic(session,"INVALID_OVERRIDE",{DefinitionCommand{SetInstanceColorOverride{"instance-a",{"logo","point","op.logo-fill.color"},local_color}}});
    atomic(session,"TYPE_MISMATCH",{DefinitionCommand{SetInstanceColorOverride{"logo",fill("peer"),local_color}}});
    atomic(session,"MISSING_OBJECT",{DefinitionCommand{SetInstanceColorOverride{"missing",fill("logo"),local_color}}});
    atomic(session,"NO_OVERRIDE",{DefinitionCommand{ResetInstanceColorOverride{"instance-a",fill("peer")}}});
    atomic(session,"REVISION_CONFLICT",{DefinitionCommand{ResetInstanceColorOverride{"instance-a",fill("logo")}}},session.revision()+1);
    for(std::size_t i=0;i<4;++i)for(const auto value:{-.01,1.01}) {
        auto invalid=local_color;invalid.rgba[i]=value;
        atomic(session,"OUT_OF_RANGE",{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("peer"),invalid}}});
    }
    for(const auto value:{std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()}) {
        auto invalid=local_color;invalid.rgba[2]=value;
        atomic(session,"NON_FINITE",{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("peer"),invalid}}});
    }
    for(const auto* field:{"space","profile","alpha"}) {
        auto invalid=local_color;
        if(std::string(field)=="space")invalid.space="display-p3";
        else if(std::string(field)=="profile")invalid.profile="display-p3";else invalid.alpha="premultiplied";
        atomic(session,"UNSUPPORTED_COLOR",{DefinitionCommand{SetInstanceColorOverride{"instance-a",fill("peer"),invalid}}});
    }
    atomic(session,"MISSING_OBJECT",{DefinitionCommand{SetInstanceColorOverride{"instance-b",fill("peer"),local_color}},SetVisibility{"missing",false}});
    atomic(session,"MISSING_OBJECT",{DefinitionCommand{ResetInstanceColorOverride{"instance-a",fill("logo")}},SetVisibility{"missing",false}});
    atomic(session,"DANGLING_OVERRIDE",{DeleteObjects{{"logo"}}});
    atomic(session,"UNSUPPORTED_OVERRIDE",{RemoveOperation{"logo","logo-fill"}});

    auto root_document=empty_document("root-color-doc","comp","source-board");auto root=rectangle("root-shape");
    root_document.objects.emplace(root.id,root);root_document.compositions.front().roots={root.id};Session root_session(root_document);
    apply(root_session,{DefinitionCommand{CreateDefinition{{"root-definition","D","root-shape"}}},
        DefinitionCommand{CreateInstance{"comp","","root-instance","root-definition","I"}}});
    atomic(root_session,"UNSUPPORTED_OVERRIDE",{DefinitionCommand{SetInstanceColorOverride{"root-instance",fill("root-shape"),local_color}}});
}

std::string api_apply(Session& session,const std::string& commands) {
    return request(session,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(session.revision())+
        ",\"commands\":["+commands+"]}");
}
std::string api_set(const Id& instance_id,const Ref& ref,const ColorValue& value) {
    return "{\"type\":\"set_instance_color_override\",\"instance\":\""+instance_id+
        "\",\"target\":"+ref_json(ref)+",\"value\":"+color_json(value)+"}";
}
void api_atomic(Session& session,const char* code,const std::string& commands) {
    const auto before=state(session);const auto response=api_apply(session,commands);
    check(response.find("\"ok\":false")!=std::string::npos&&
        response.find(std::string("\"code\":\"")+code+"\"")!=std::string::npos,"API reports "+std::string(code)+": "+response);
    unchanged(session,before,"Invalid API request preserves exact native bytes, revision and history");
}
void canonical_api() {
    Session session(fixture());const auto initial=session.document();
    const auto set=api_apply(session,api_set("instance-a",fill("logo"),local_color)+","+api_set("instance-a",fill("peer"),sibling_color));
    check(set.find("\"ok\":true")!=std::string::npos&&instance(session.document()).color_overrides==
        std::map<Ref,ColorValue>{{fill("logo"),local_color},{fill("peer"),sibling_color}},
        "Canonical API authors two structured local ColorValues in one Session batch");
    same_sources(session.document(),initial);
    occurrence_color(scene(session.document()),"instance-a","logo",local_color,"API-authored local Color reaches numeric renderer");
    check(request(session,R"({"op":"inspect"})").find("\"color_overrides\":[{\"target\":"+ref_json(fill("logo")))!=std::string::npos,
        "Inspect exposes canonical authored source Ref and structured ColorValue");
    api_atomic(session,"UNSUPPORTED_OVERRIDE",api_set("instance-a",operation_ref("logo","logo-stroke","color"),local_color));
    api_atomic(session,"DANGLING_OVERRIDE",api_set("instance-a",fill("outside"),local_color));
    auto invalid=local_color;invalid.rgba[0]=1.1;api_atomic(session,"OUT_OF_RANGE",api_set("instance-a",fill("logo"),invalid));
    api_atomic(session,"INVALID_REQUEST",R"({"type":"set_instance_color_override","instance":"instance-a","target":{"object":"logo","point":"","field":"op.logo-fill.color"},"value":false})");
    api_atomic(session,"INVALID_COLOR",R"({"type":"set_instance_color_override","instance":"instance-a","target":{"object":"logo","point":"","field":"op.logo-fill.color"},"value":{"space":"srgb","profile":"srgb","alpha":"straight","rgba":[0,1,0]}})");
    api_atomic(session,"UNSUPPORTED_COLOR",R"({"type":"set_instance_color_override","instance":"instance-a","target":{"object":"logo","point":"","field":"op.logo-fill.color"},"value":{"space":"srgb","profile":"srgb","alpha":"premultiplied","rgba":[0,1,0,1]}})");
    api_atomic(session,"DUPLICATE_KEY",R"({"type":"set_instance_color_override","instance":"instance-a","target":{"object":"logo","point":"","field":"op.logo-fill.color"},"value":{"space":"srgb","profile":"srgb","alpha":"straight","rgba":[0,1,0,1],"rgba":[1,0,0,1]}})");
    api_atomic(session,"UNKNOWN_FIELD",R"({"type":"reset_instance_color_override","instance":"instance-a","target":{"object":"logo","point":"","field":"op.logo-fill.color"},"value":{"space":"srgb","profile":"srgb","alpha":"straight","rgba":[0,1,0,1]}})");
    const std::string reset=R"({"type":"reset_instance_color_override","instance":"instance-a","target":{"object":"logo","point":"","field":"op.logo-fill.color"}})";
    api_atomic(session,"MISSING_OBJECT",reset+R"(,{"type":"set_visibility","object":"missing","visible":false})");
    const auto before=state(session);const auto stale=request(session,
        "{\"op\":\"apply\",\"expected_revision\":"+std::to_string(session.revision()+1)+",\"commands\":["+reset+"]}");
    check(stale.find("REVISION_CONFLICT")!=std::string::npos,"API refuses stale Color Reset");
    unchanged(session,before,"Stale API Reset retains exact native bytes, revision and history");
    const auto before_reset=encode(session.document());const auto response=api_apply(session,reset);
    check(response.find("\"ok\":true")!=std::string::npos&&instance(session.document()).color_overrides==
        std::map<Ref,ColorValue>{{fill("peer"),sibling_color}},"Canonical API Reset selectively restores inheritance");
    occurrence_color(scene(session.document()),"instance-a","logo",source_color,"API Reset reaches source RGBA in renderer");
    session.undo(session.revision());check(encode(session.document())==before_reset,"API Reset has one exact Undo boundary");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3&&std::string(argv[1])=="--cold-read") {
            cold_read(argv[2]);std::cout<<"PASS "<<checks<<" cold Instance Fill Color override checks\n";return 0;
        }
        independent_projection_source_follow_and_reset();driven_source_channels_remain_authored_and_reset_live();
        duplicate_template_and_detach();native_contract_and_atomic_refusals();canonical_api();
        fresh_process_live_and_detached(std::filesystem::absolute(argv[0]).string());
        std::cout<<"PASS "<<checks<<" Instance Fill Color override checks\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;
    }
}
