#include "nect/io.hpp"
#include "native_legacy_fixture.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
struct State { std::string native;std::uint64_t revision;HistoryInfo history; };
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
    unchanged(session,before,"Refusal preserves exact native bytes, revision and History");
}
const std::string source_content="Source";
// Quotes, backslash, Japanese, an astral scalar, CRLF and tab must stay exact.
const std::string local_content="Local \"title\" \\ " "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e" " \xf0\x9f\x99\x82\r\n\tSecond line";
ColorValue local_color() { ColorValue value;value.rgba={.75,.25,.125,.5};return value; }
Ref content_ref(const Id& id) {return {id,"","text.content"};}
Ref fill_ref(const Id& id) {return operation_ref(id,id+"-fill","color");}
Object text_object(const Id& id,const std::string& content) {
    Object object;object.id=id;object.name=id;object.kind=Kind::text;
    object.text=default_text(id+"-text",content);
    object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));return object;
}
Document fixture(bool path_consumer=false) {
    auto document=empty_document("text-override-doc","comp","source-board");
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;
    root.children={"driver","title","peer","folder","shape"};root.transform[4].literal=10;
    auto driver=text_object("driver",source_content),title=text_object("title","Authored title");
    title.text->content_driver=TextContentDriver{content_ref("driver")};
    auto peer=text_object("peer","Peer"),nested=text_object("nested","Authored nested");
    nested.text->content_driver=TextContentDriver{content_ref("title")};
    Object folder;folder.id="folder";folder.name="Folder";folder.kind=Kind::group;folder.children={"nested"};
    Object shape;shape.id="shape";shape.name="Shape";
    Point first,last;first.id="first";last.id="last";last.x.literal=2000;
    shape.contours={{"shape-contour",false,{first,last}}};
    auto outside=text_object("outside","Outside");
    for(const auto& object:{root,driver,title,peer,folder,nested,shape,outside})
        document.objects.emplace(object.id,object);
    if(path_consumer) {
        auto path_text=text_object("path-text","Path");
        path_text.text->content_driver=TextContentDriver{content_ref("nested")};
        path_text.text->path_attachment=TextPathAttachment{"shape","shape-contour","distance",0,0,false};
        document.objects.emplace(path_text.id,path_text);document.objects.at("source-root").children.push_back(path_text.id);
    }
    document.compositions.front().roots={"source-root","outside"};Session session(document);
    apply(session,{DefinitionCommand{CreateDefinition{{"definition","D","source-root"}}},
        DefinitionCommand{CreateDefinition{{"outside-definition","Outside D","outside"}}},
        DefinitionCommand{CreateInstance{"comp","","instance-a","definition","A"}},
        DefinitionCommand{CreateInstance{"comp","","instance-b","definition","B"}},
        Set{{"instance-a","","transform.tx"},120},Set{{"instance-b","","transform.tx"},240}});
    return session.document();
}
const DefinitionInstance& instance(const Document& document,const Id& id="instance-a") {
    return *document.objects.at(id).instance;
}
SceneProjection scene(const Document& document) {
    const auto values=evaluate(document);
    const auto transforms=evaluate_transforms(document,values);
    auto projected=project_definition_instances(document,"comp",values,transforms);
#ifdef _WIN32
    const auto rendered=evaluate_scene(document,"comp",values,transforms);
    check(rendered.instance_sources==projected.instance_sources&&
        encode(*rendered.expanded_document)==encode(*projected.document),
        "DirectWrite scene rendering consumes the same exact occurrence projection");
#endif
    return projected;
}
Id proxy(const SceneProjection& evaluated,const Id& owner,const Id& source) {
    for(const auto& [id,instance_id]:evaluated.instance_owners)
        if(instance_id==owner&&evaluated.instance_sources.at(id)==source)return id;
    throw std::runtime_error("Missing occurrence: "+owner+" / "+source);
}
void occurrence_content(const SceneProjection& evaluated,const Id& owner,const Id& source,
    const std::string& expected,const char* message) {
    const auto id=proxy(evaluated,owner,source);
    check(text_content_property(*evaluated.document,content_ref(id)).evaluated==expected&&
        evaluated_text_source(*evaluated.document,id).content==expected,message);
}
void local_literal(const Document& document,const Id& id,const std::string& expected) {
    const auto& source=*document.objects.at(id).text;
    check(source.content==expected&&!source.content_driver,
        "The selected clone is an exact local literal with its content driver cleared");
}
void same_sources(const Document& actual,const Document& expected) {
    for(const auto* id:{"source-root","driver","title","peer","folder","nested","shape","outside"})
        check(actual.objects.at(id)==expected.objects.at(id),"Source object and exact content driver stay authored: "+Id{id});
}
UpdateText update_content(const Document& document,const Id& id,const std::string& content) {
    auto source=*document.objects.at(id).text;source.content=content;return {id,source};
}
void mixed_overrides(Session& session) {
    apply(session,{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title",local_content}},
        DefinitionCommand{SetInstanceTextContentOverride{"instance-a","peer",""}},
        DefinitionCommand{SetInstanceOverride{"instance-a",{"title","","transform.tx"},90}},
        DefinitionCommand{SetInstanceVisibilityOverride{"instance-a","peer",false}},
        DefinitionCommand{SetInstanceColorOverride{"instance-a",fill_ref("title"),local_color()}}});
}

void projection_source_follow_and_reset() {
    Session session(fixture());const auto source=session.document();const auto before=state(session);
    mixed_overrides(session);const auto local=state(session);same_sources(session.document(),source);
    check(evaluate_instance_text_content(session.document(),"instance-a","title")==local_content,"Typed occurrence readback returns own local literal");
    check(evaluate_instance_text_content(session.document(),"instance-a","nested")==local_content,"Typed occurrence baseline follows upstream local override");
    check(evaluate_instance_text_content(session.document(),"instance-b","nested")==source_content,"Typed occurrence readback retains sibling inheritance");
    rejects("UNSUPPORTED_OVERRIDE",[&]{(void)evaluate_instance_text_content(session.document(),"instance-a","source-root");});
    rejects("DANGLING_OVERRIDE",[&]{(void)evaluate_instance_text_content(session.document(),"instance-a","outside");});
    auto evaluated=scene(session.document());
    check(evaluate_text_content(session.document(),"title")==source_content,
        "Authored linked Text evaluates from its original source");
    occurrence_content(evaluated,"instance-a","title",local_content,"The selected occurrence projects exact multiline UTF-8 content");
    occurrence_content(evaluated,"instance-a","nested",local_content,"A remapped descendant link consumes the overridden local title");
    occurrence_content(evaluated,"instance-a","peer","","Empty local content is meaningful");
    occurrence_content(evaluated,"instance-b","title",source_content,"The sibling occurrence inherits the source content");
    local_literal(*evaluated.document,proxy(evaluated,"instance-a","title"),local_content);
    const auto other_title=proxy(evaluated,"instance-b","title"),other_driver=proxy(evaluated,"instance-b","driver");
    check(evaluated.document->objects.at(other_title).text->content_driver==
        TextContentDriver{content_ref(other_driver)},"Unmodified occurrence retains the correctly remapped content driver");
    check(evaluated.values->at({proxy(evaluated,"instance-a","title"),"","transform.tx"})==90,
        "Text and Scalar overrides coexist in the same projection");
#ifdef _WIN32
    (void)export_svg(session.document(),"comp","source-board");
#endif
    unchanged(session,local,"Pure occurrence projection preserves exact authored bytes and History");
    session.undo(session.revision());check(encode(session.document())==before.native,"One Undo restores the complete mixed batch");
    session.redo(session.revision());check(encode(session.document())==local.native,"One Redo restores all exact occurrence maps");

    apply(session,{update_content(session.document(),"driver","Changed source"),Rename{"title","Renamed Title"},
        ReorderObjects{"comp","source-root",{"folder","peer","shape","title","driver"}}});
    evaluated=scene(session.document());
    occurrence_content(evaluated,"instance-a","title",local_content,"Source edits and rename/reorder retain local content by stable ID");
    occurrence_content(evaluated,"instance-b","nested","Changed source","Unmodified linked descendants follow current source content");
    const auto before_reset=encode(session.document());
    apply(session,{DefinitionCommand{ResetInstanceTextContentOverride{"instance-a","title"}}});
    const auto after_reset=encode(session.document());evaluated=scene(session.document());
    occurrence_content(evaluated,"instance-a","title","Changed source","Reset restores current source content and driver");
    occurrence_content(evaluated,"instance-a","nested","Changed source","Reset propagates through the occurrence's remapped content links");
    check(instance(session.document()).text_content_overrides==std::map<Id,std::string>{{"peer",""}}&&
        instance(session.document()).overrides.at({"title","","transform.tx"})==90&&
        !instance(session.document()).visibility_overrides.at("peer")&&
        instance(session.document()).color_overrides.at(fill_ref("title"))==local_color(),
        "Selective Text Reset retains sibling content, Scalar, visibility and Fill Color maps");
    session.undo(session.revision());check(encode(session.document())==before_reset,"Undo restores the selected Text override");
    session.redo(session.revision());check(encode(session.document())==after_reset,"Redo restores exact selective Text Reset");
    apply(session,{update_content(session.document(),"driver","Later source")});
    occurrence_content(scene(session.document()),"instance-a","title","Later source","Later source edits remain live after Reset");
    apply(session,{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title","Later source"}}});
    apply(session,{update_content(session.document(),"driver","Newest source")});
    evaluated=scene(session.document());
    occurrence_content(evaluated,"instance-a","title","Later source","Equal-valued local content keeps occurrence-local identity");
    occurrence_content(evaluated,"instance-b","title","Newest source","Its unoverridden sibling still follows the source");
    check(session.document().objects.at("title").text==source.objects.at("title").text,
        "Projection, source edits and Reset never replace the authored title literal or source driver");
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
    std::ifstream file(path,std::ios::binary);check(file.good(),"Fresh process can read the native file");
    const std::string native{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    Session session(decode(native));
    check(encode(session.document())==native,"Fresh process preserves exact native 0.85 bytes");
    check(session.revision()==0&&!session.can_undo()&&!session.can_redo(),"Cold Session has independent empty History");
    auto evaluated=scene(session.document());
    if(session.document().objects.at("instance-a").instance) {
        check(instance(session.document()).text_content_overrides==
            std::map<Id,std::string>{{"peer",""},{"title",local_content}},"Cold live occurrence retains exact source-keyed UTF-8 content");
        occurrence_content(evaluated,"instance-a","title",local_content,"Cold live occurrence recomputes local content");
        occurrence_content(evaluated,"instance-a","peer","","Cold live occurrence retains empty local content");
        apply(session,{update_content(session.document(),"driver","Cold changed")});evaluated=scene(session.document());
        occurrence_content(evaluated,"instance-a","title",local_content,"Cold local content survives a fresh source edit");
        occurrence_content(evaluated,"instance-b","title","Cold changed","Cold sibling has a live remapped source driver");
        apply(session,{DefinitionCommand{ResetInstanceTextContentOverride{"instance-a","title"}}});
        occurrence_content(scene(session.document()),"instance-a","title","Cold changed","Cold Reset resumes current source inheritance");
        check(instance(session.document()).text_content_overrides==std::map<Id,std::string>{{"peer",""}},"Cold Reset stays selective");
    } else {
        const auto& placement=session.document().objects.at("instance-a");
        const auto& root=session.document().objects.at(placement.children.front());
        const auto title=root.children[1],peer=root.children[2];
        check(placement.kind==Kind::group,"Cold detached occurrence is an ordinary Group");
        local_literal(session.document(),title,local_content);local_literal(session.document(),peer,"");
        check(evaluate_text_content(session.document(),session.document().objects.at(root.children[3]).children.front())==local_content,
            "Cold materialized downstream link resolves to the detached local title");
        apply(session,{update_content(session.document(),"driver","Cold changed")});
        check(evaluate_text_content(session.document(),title)==local_content,"Cold detached content stays frozen after a source edit");
        occurrence_content(scene(session.document()),"instance-b","title","Cold changed","Cold sibling remains live after whole Detach");
    }
}
void cold_replay(const std::string& executable,const Document& document) {
    const auto path=std::filesystem::temp_directory_path()/
        ("nect instance-text 0.85 "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".nect");
    {std::ofstream file(path,std::ios::binary);file<<encode(document);check(file.good(),"Save the native file before cold replay");}
#ifdef _WIN32
    // Avoid cmd.exe outer-quote parsing; both executable and native path may contain spaces.
    const auto result=_spawnl(_P_WAIT,executable.c_str(),quoted(executable).c_str(),
        "--cold-read",quoted(path.string()).c_str(),static_cast<const char*>(nullptr));
#else
    const auto result=std::system((quoted(executable)+" --cold-read "+quoted(path.string())).c_str());
#endif
    std::filesystem::remove(path);check(result==0,"A fresh process verifies local content, source follow and Reset/Detach");
}
void duplicate_detach_and_cold_reopen(const std::string& executable) {
    Session session(fixture());mixed_overrides(session);const auto live=session.document();const auto live_state=state(session);
    cold_replay(executable,live);unchanged(session,live_state,"Cold replay cannot mutate the parent Session");
    const DuplicateObjects command{{"instance-a"},"copy"};const auto copied=duplicated_roots(live,command).front();
    apply(session,{command});const auto duplicated=encode(session.document());
    check(instance(session.document(),copied)==instance(live),"Ordinary duplication retains every source-keyed local map");
    occurrence_content(scene(decode(duplicated)),copied,"title",local_content,"Cold duplicated occurrence renders copied content");
    session.undo(session.revision());check(encode(session.document())==live_state.native,"Undo removes only the ordinary duplicate");
    session.redo(session.revision());check(encode(session.document())==duplicated,"Redo restores exact duplicate identities and maps");
    apply(session,{DefinitionCommand{ResetInstanceTextContentOverride{copied,"title"}}});
    auto evaluated=scene(session.document());
    occurrence_content(evaluated,copied,"title",source_content,"Duplicate Reset resumes its own source inheritance");
    occurrence_content(evaluated,"instance-a","title",local_content,"Duplicate Reset leaves the original override intact");

    const auto before_detach=encode(session.document());
    apply(session,{DefinitionCommand{DetachInstance{"instance-a","detached"}}});
    const auto detached=session.document();const auto detached_state=state(session);
    const auto& placement=detached.objects.at("instance-a");const auto& root=detached.objects.at(placement.children.front());
    const auto title=root.children[1],peer=root.children[2],nested=detached.objects.at(root.children[3]).children.front();
    check(placement.kind==Kind::group&&!placement.instance&&placement.transform[4].literal==120&&
        root.transform[4].literal==0,"Whole Detach retains occurrence placement and materializes the source tree");
    local_literal(detached,title,local_content);local_literal(detached,peer,"");same_sources(detached,live);
    check(detached.objects.at(nested).text->content_driver==TextContentDriver{content_ref(title)}&&
        evaluate_text_content(detached,nested)==local_content,"Detach remaps downstream links to the materialized local title");
    check(detached.objects.at(title).transform[4].literal==90&&!detached.objects.at(peer).visible,
        "Whole Detach retains Scalar and visibility overrides beside local Text content");
    session.undo(session.revision());check(encode(session.document())==before_detach,"One Undo restores exact live Text maps and drivers");
    session.redo(session.revision());check(encode(session.document())==detached_state.native,"One Redo restores exact detached identities and literals");
    const auto replay_before=state(session);cold_replay(executable,session.document());
    unchanged(session,replay_before,"Detached cold replay preserves the parent History");
    apply(session,{update_content(session.document(),"driver","Later source")});
    check(evaluate_text_content(session.document(),title)==local_content&&evaluate_text_content(session.document(),nested)==local_content,
        "Detached content and remapped downstream links stay frozen after source edits");
}

std::string json_string(const std::string& value) {
    std::string result="\"";const char* hex="0123456789abcdef";
    for(const unsigned char byte:value) {
        if(byte=='"'||byte=='\\'){result+='\\';result+=static_cast<char>(byte);}
        else if(byte<32){result+="\\u00";result+=hex[byte>>4];result+=hex[byte&15];}
        else result+=static_cast<char>(byte);
    }
    return result+'"';
}
std::string entry(const Id& source,const std::string& content) {
    return "{\"source\":"+json_string(source)+",\"content\":"+json_string(content)+"}";
}
std::string content_array(std::string native,const std::string& entries) {
    const std::string field="\"text_content_overrides\":[";const auto at=native.find(field);
    check(at!=std::string::npos,"Native fixture has the optional Text content array");
    const auto begin=at+field.size();auto end=begin;bool in_string=false,escaped=false;
    for(;end<native.size();++end) {
        const auto ch=native[end];
        if(in_string){if(escaped)escaped=false;else if(ch=='\\')escaped=true;else if(ch=='"')in_string=false;continue;}
        if(ch=='"')in_string=true;else if(ch==']')break;
    }
    check(end<native.size(),"Native fixture has a closed Text content array");native.replace(begin,end-begin,entries);return native;
}
std::string versioned(std::string native,const char* version) {
    const auto marker=test_support::current_native_version_marker();const auto at=native.find(marker);
    check(at!=std::string::npos,"Fixture uses the current native writer");
    native.replace(at,marker.size(),std::string("\"version\":\"")+version+"\"");return native;
}
std::string api_set(const Id& instance_id,const Id& source,const std::string& content) {
    return "{\"type\":\"set_instance_text_content_override\",\"instance\":"+json_string(instance_id)+
        ",\"source\":"+json_string(source)+",\"content\":"+json_string(content)+"}";
}
std::string api_reset(const Id& instance_id,const Id& source) {
    return "{\"type\":\"reset_instance_text_content_override\",\"instance\":"+json_string(instance_id)+
        ",\"source\":"+json_string(source)+"}";
}
std::string api_apply(Session& session,const std::string& commands) {
    return request(session,"{\"op\":\"apply\",\"expected_revision\":"+std::to_string(session.revision())+
        ",\"commands\":["+commands+"]}");
}
void api_atomic(Session& session,const char* code,const std::string& commands) {
    const auto before=state(session);const auto response=api_apply(session,commands);
    check(response.find("\"ok\":false")!=std::string::npos&&
        response.find(std::string("\"code\":\"")+code+"\"")!=std::string::npos,"API reports "+std::string(code)+": "+response);
    unchanged(session,before,"Rejected API batch preserves exact authored bytes, revision and History");
}
void native_api_and_atomic_contracts() {
    Session session(fixture());const auto legacy=session.document();
    check(std::string(native_version)=="0.85","Text content override writer is native 0.85");
    check(encode(legacy).find("\"text_content_overrides\"")==std::string::npos,"Empty Text maps omit the optional field");
    check(decode(versioned(encode(legacy),"0.84"))==legacy,"Native 0.84 without the optional field remains readable");
    const auto before=state(session);
    const auto response=api_apply(session,api_set("instance-a","title",local_content)+","+api_set("instance-a","peer",""));
    Session read_session(fixture());
    apply(read_session,{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title","API local"}}});
    const auto before_read=state(read_session);
    const auto inherited=request(read_session,R"({"op":"instance_text_content","instance":"instance-a","source":"nested"})");
    check(inherited.find("\"ok\":true")!=std::string::npos&&inherited.find("\"content\":\"API local\"")!=std::string::npos&&
        inherited.find("\"source_content\":\"Source\"")!=std::string::npos&&inherited.find("\"overridden\":false")!=std::string::npos,
        "Read-only API distinguishes upstream occurrence inheritance from original source content: "+inherited);
    const auto own=request(read_session,R"({"op":"instance_text_content","instance":"instance-a","source":"title"})");
    check(own.find("\"overridden\":true")!=std::string::npos,"Read-only API identifies an own local override");
    const auto bad=request(read_session,R"({"op":"instance_text_content","instance":"instance-a","source":"outside"})");
    check(bad.find("\"code\":\"DANGLING_OVERRIDE\"")!=std::string::npos,"Read-only API rejects non-member source items");
    unchanged(read_session,before_read,"Occurrence content queries preserve exact authored state and History");

    check(response.find("\"ok\":true")!=std::string::npos&&instance(session.document()).text_content_overrides==
        std::map<Id,std::string>{{"peer",""},{"title",local_content}},"Canonical API authors exact multiline and empty content in one batch");
    const auto saved=encode(session.document());const auto after=state(session);same_sources(session.document(),legacy);
    check(decode(saved)==session.document()&&encode(decode(saved))==saved,"Native 0.85 retains exact Text map identities and UTF-8 bytes");
    occurrence_content(scene(decode(saved)),"instance-a","title",local_content,"Cold decoded API override reaches the shared Text projection");
    check(request(session,R"({"op":"inspect"})").find("\"text_content_overrides\":[{\"source\":\"peer\",\"content\":\"\"}")!=std::string::npos,
        "Inspect exposes the canonical optional source/content records");
    unchanged(session,after,"Inspect and cold projection do not mutate History");
    session.undo(session.revision());check(encode(session.document())==before.native,"One Undo restores the complete canonical API batch");
    session.redo(session.revision());check(encode(session.document())==saved,"One Redo restores exact API-authored content");
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(saved,"0.84"));});
    rejects("NATIVE_VERSION_MISMATCH",[&]{(void)decode(versioned(content_array(saved,""),"0.84"));});
    rejects("DUPLICATE_OVERRIDE_KEY",[&]{(void)decode(content_array(saved,entry("title","A")+","+entry("title","B")));});
    rejects("DUPLICATE_KEY",[&]{(void)decode(content_array(saved,R"({"source":"title","content":"A","content":"B"})"));});
    rejects("DUPLICATE_KEY",[&]{(void)decode(content_array(saved,R"({"source":"title","source":"peer","content":"A"})"));});
    rejects("UNKNOWN_FIELD",[&]{(void)decode(content_array(saved,R"({"source":"title","content":"A","value":"B"})"));});
    rejects("UNSUPPORTED_OVERRIDE",[&]{(void)decode(content_array(saved,entry("shape","A")));});
    rejects("DANGLING_OVERRIDE",[&]{(void)decode(content_array(saved,entry("outside","A")));});
    rejects("LIMIT",[&]{(void)decode(content_array(saved,entry("title",std::string(32769,'x'))));});
    rejects("INVALID_TEXT",[&]{(void)decode(content_array(saved,entry("title",std::string(1,'\0'))));});

    for(const auto* source:{"source-root","shape","folder"})
        atomic(session,"UNSUPPORTED_OVERRIDE",{DefinitionCommand{SetInstanceTextContentOverride{"instance-a",source,"A"}}});
    for(const auto& source:std::vector<Id>{"missing","outside",proxy(scene(session.document()),"instance-a","title")})
        atomic(session,"DANGLING_OVERRIDE",{DefinitionCommand{SetInstanceTextContentOverride{"instance-a",source,"A"}}});
    atomic(session,"TYPE_MISMATCH",{DefinitionCommand{SetInstanceTextContentOverride{"title","peer","A"}}});
    atomic(session,"MISSING_OBJECT",{DefinitionCommand{SetInstanceTextContentOverride{"missing","title","A"}}});
    atomic(session,"NO_OVERRIDE",{DefinitionCommand{ResetInstanceTextContentOverride{"instance-a","nested"}}});
    atomic(session,"REVISION_CONFLICT",{DefinitionCommand{ResetInstanceTextContentOverride{"instance-a","title"}}},session.revision()+1);
    for(const auto& invalid:std::vector<std::string>{std::string("\xc0\xaf",2),std::string("\xed\xa0\x80",3),std::string("\xf0\x9f",2)})
        atomic(session,"INVALID_UTF8",{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title",invalid}}});
    atomic(session,"INVALID_TEXT",{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title",std::string(1,'\0')}}});
    atomic(session,"LIMIT",{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title",std::string(32769,'x')}}});
    atomic(session,"MISSING_OBJECT",{DefinitionCommand{SetInstanceTextContentOverride{"instance-b","title","A"}},SetVisibility{"missing",false}});
    atomic(session,"MISSING_OBJECT",{DefinitionCommand{ResetInstanceTextContentOverride{"instance-a","title"}},SetVisibility{"missing",false}});
    atomic(session,"DANGLING_OVERRIDE",{DeleteObjects{{"peer"}}});
    std::string boundary;for(unsigned i=0;i<8192;++i)boundary+="\xf0\x9f\x99\x82";
    check(boundary.size()==32768,"Boundary fixture is exactly 32768 UTF-8 bytes");
    Session boundary_session(session.document());
    apply(boundary_session,{DefinitionCommand{SetInstanceTextContentOverride{"instance-b","peer",boundary}}});
    check(instance(decode(encode(boundary_session.document())),"instance-b").text_content_overrides.at("peer")==boundary,
        "The inclusive byte limit accepts valid multibyte content without truncation");
    atomic(boundary_session,"LIMIT",{DefinitionCommand{SetInstanceTextContentOverride{"instance-b","peer",boundary+"x"}}});

    auto root_document=empty_document("text-root-doc","comp","source-board");auto root=text_object("text-root","Root");
    root_document.objects.emplace(root.id,root);root_document.compositions.front().roots={root.id};Session root_session(root_document);
    apply(root_session,{DefinitionCommand{CreateDefinition{{"root-definition","D","text-root"}}},
        DefinitionCommand{CreateInstance{"comp","","root-instance","root-definition","I"}}});
    atomic(root_session,"UNSUPPORTED_OVERRIDE",{DefinitionCommand{SetInstanceTextContentOverride{"root-instance","text-root","A"}}});

    api_atomic(session,"UNSUPPORTED_OVERRIDE",api_set("instance-a","shape","A"));
    api_atomic(session,"DANGLING_OVERRIDE",api_set("instance-a","outside","A"));
    api_atomic(session,"INVALID_REQUEST",R"({"type":"set_instance_text_content_override","instance":"instance-a","source":"title","content":false})");
    api_atomic(session,"DUPLICATE_KEY",R"({"type":"set_instance_text_content_override","instance":"instance-a","source":"title","content":"A","content":"B"})");
    api_atomic(session,"UNKNOWN_FIELD",R"({"type":"reset_instance_text_content_override","instance":"instance-a","source":"title","content":"A"})");
    api_atomic(session,"LIMIT",api_set("instance-a","title",std::string(32769,'x')));
    api_atomic(session,"INVALID_TEXT",api_set("instance-a","title",std::string(1,'\0')));
    api_atomic(session,"MISSING_OBJECT",api_reset("instance-a","title")+R"(,{"type":"set_visibility","object":"missing","visible":false})");
    const auto stale_before=state(session);const auto stale=request(session,"{\"op\":\"apply\",\"expected_revision\":"+
        std::to_string(session.revision()+1)+",\"commands\":["+api_reset("instance-a","title")+"]}");
    check(stale.find("REVISION_CONFLICT")!=std::string::npos,"Canonical API refuses stale Text Reset");
    unchanged(session,stale_before,"Stale API Reset preserves exact bytes and History");
    check(api_apply(session,api_reset("instance-a","title")).find("\"ok\":true")!=std::string::npos&&
        instance(session.document()).text_content_overrides==std::map<Id,std::string>{{"peer",""}},"Canonical API Reset selectively restores inheritance");
    occurrence_content(scene(session.document()),"instance-a","title",source_content,"API Reset reaches the shared Text projection");

    Session attached(fixture(true));const auto path_before=state(attached);
    for(const auto& line_break:std::vector<std::string>{"\n","\r","\xe2\x80\xa8","\xe2\x80\xa9"}) {
        atomic(attached,"TEXT_PATH_MULTILINE_UNSUPPORTED",{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","path-text","A"+line_break+"B"}}});
        atomic(attached,"TEXT_PATH_MULTILINE_UNSUPPORTED",{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title","A"+line_break+"B"}}});
    }
    atomic(attached,"TEXT_PATH_MULTILINE_UNSUPPORTED",{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","peer","Allowed"}},
        DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title","A\nB"}}});
    api_atomic(attached,"TEXT_PATH_MULTILINE_UNSUPPORTED",api_set("instance-a","title","A\nB"));
    unchanged(attached,path_before,"Direct and multi-hop path refusal leave the entire occurrence and History untouched");
    apply(attached,{DefinitionCommand{SetInstanceTextContentOverride{"instance-a","path-text","Local one line"}},
        DefinitionCommand{SetInstanceTextContentOverride{"instance-a","title","A\nB"}}});
    const auto valid_path=encode(attached.document());auto evaluated=scene(attached.document());
    occurrence_content(evaluated,"instance-a","path-text","Local one line","Local path literal cuts its upstream content dependency");
    occurrence_content(evaluated,"instance-a","title","A\nB","An ordinary Text permits multiline content when its path consumer is locally independent");
    local_literal(*evaluated.document,proxy(evaluated,"instance-a","path-text"),"Local one line");
    atomic(attached,"TEXT_PATH_MULTILINE_UNSUPPORTED",{DefinitionCommand{ResetInstanceTextContentOverride{"instance-a","path-text"}}});
    rejects("TEXT_PATH_MULTILINE_UNSUPPORTED",[&]{(void)decode(content_array(valid_path,entry("path-text","A\nB")));});
    rejects("TEXT_PATH_MULTILINE_UNSUPPORTED",[&]{(void)decode(content_array(valid_path,entry("title","A\nB")));});
    apply(attached,{DefinitionCommand{ResetInstanceTextContentOverride{"instance-a","title"}},
        DefinitionCommand{ResetInstanceTextContentOverride{"instance-a","path-text"}}});
    occurrence_content(scene(attached.document()),"instance-a","path-text",source_content,"Reset restores the valid multi-hop path source");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3&&std::string(argv[1])=="--cold-read") {
            cold_read(argv[2]);std::cout<<"PASS "<<checks<<" cold Instance Text content override checks\n";return 0;
        }
        projection_source_follow_and_reset();
        duplicate_detach_and_cold_reopen(std::filesystem::absolute(argv[0]).string());
        native_api_and_atomic_contracts();
#ifdef _WIN32
        std::cout<<"PASS "<<checks<<" Instance Text content override checks, including DirectWrite scene/SVG\n";
#else
        std::cout<<"PASS "<<checks<<" portable Instance Text content override checks; DirectWrite scene/SVG require Windows\n";
#endif
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;
    }
}
