#include "nect/io.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace nect;
namespace {
int checks=0;
void check(bool value,const std::string& why) {
    if(!value)throw std::runtime_error(why);
    ++checks;
}
Ref content(const Id& object){return {object,"","text.content"};}
void add_text(Document& document,const Id& composition,const Id& id,const std::string& literal) {
    Object object;object.id=id;object.name="Text "+id;object.kind=Kind::text;
    object.text=default_text(id+"-source",literal);
    document.objects.emplace(id,std::move(object));
    auto found=std::find_if(document.compositions.begin(),document.compositions.end(),[&](const auto& item){return item.id==composition;});
    found->roots.push_back(id);
}
Document fixture() {
    // Seed authored objects directly: these semantic tests never create glyph
    // outlines or initialize creation Anchors through a platform font backend.
    auto document=empty_document("batch-doc","comp","frame");
    add_text(document,"comp","S","\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\nSource");
    add_text(document,"comp","B","Manual B");
    add_text(document,"comp","C","Manual C");
    add_text(document,"comp","D","Other source");
    Object path;path.id="path";path.name="Non-Text";document.objects.emplace(path.id,path);
    document.compositions.front().roots.push_back(path.id);
    document.compositions.push_back(empty_document("unused-doc","other-comp","other-frame").compositions.front());
    add_text(document,"other-comp","X","Cross-Composition source");
    validate(document);
    return document;
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
std::vector<Command> fanout(const Ref& source=content("S"),bool replace=false) {
    return {LinkTextContent{content("B"),source,replace},LinkTextContent{content("C"),source,replace}};
}
struct Snapshot {
    Document document;
    std::string bytes;
    std::uint64_t revision;
    HistoryInfo history;
    bool undo,redo;
    explicit Snapshot(const Session& session):document(session.document()),bytes(encode(document)),
        revision(session.revision()),history(session.history()),undo(session.can_undo()),redo(session.can_redo()){}
    void unchanged(const Session& session,const std::string& reason)const {
        check(session.document()==document,reason+": Document unchanged");
        check(encode(session.document())==bytes,reason+": exact authored native bytes unchanged");
        check(session.revision()==revision,reason+": revision unchanged");
        check(session.history()==history,reason+": history and redo branch unchanged");
        check(session.can_undo()==undo&&session.can_redo()==redo,reason+": Undo/Redo availability unchanged");
    }
};
template<class F>void reject(Session& session,const std::string& code,F action) {
    const Snapshot before(session);
    try {action();}
    catch(const Error& error) {
        check(error.code==code,"Expected "+code+", got "+error.code);
        before.unchanged(session,code);
        return;
    }
    throw std::runtime_error("Expected rejection: "+code);
}
void reject_batch(Session& session,const std::string& code,std::vector<Command> commands) {
    reject(session,code,[&]{apply(session,std::move(commands));});
}
void retain_redo(Session& session) {
    apply(session,{Rename{"D","Temporary rename"}});
    session.undo(session.revision());
    check(session.can_redo(),"Atomic rejection fixture has a retained redo branch");
}
void linked(const Session& session,const Id& target,const Ref& source,const std::string& literal) {
    const auto value=text_content_property(session.document(),content(target));
    check(value.literal==literal,"Fan-out preserves each distinct authored literal");
    check(value.driver==TextContentDriver{source},"Fan-out stores the exact immediate stable Ref");
    check(value.evaluated==evaluate_text_content(session.document(),source.object),"Target evaluates the shared source string");
}
void fanout_history_and_identity() {
    Session session(fixture());const auto original=session.document();const auto original_bytes=encode(original);
    const auto original_history=session.history();const auto source_text=original.objects.at("S").text->content;
    apply(session,fanout());
    auto expected=original;
    expected.objects.at("B").text->content_driver=TextContentDriver{content("S")};
    expected.objects.at("C").text->content_driver=TextContentDriver{content("S")};
    check(session.document()==expected,"Only the two content drivers change in the whole Document");
    check(session.revision()==1&&session.history().states.size()==original_history.states.size()+1,
        "One Session apply produces one revision and one History transition");
    check(session.history().states.back().label=="Link Text content batch (2)","One-source fan-out has its bounded content batch label");
    linked(session,"B",content("S"),"Manual B");linked(session,"C",content("S"),"Manual C");
    check(evaluate_text_content(session.document(),"B")==source_text,"UTF-8 and newline content evaluates byte-exactly");
    const auto linked_bytes=encode(session.document());
    check(encode(decode(linked_bytes))==linked_bytes,"Native codec roundtrip preserves the two existing content drivers byte-exactly");
    session.undo(session.revision());
    check(session.document()==original&&encode(session.document())==original_bytes,"One Undo restores both exact literals and the original Document");
    check(!session.can_undo()&&session.can_redo(),"Fan-out uses exactly one Undo transition");
    session.redo(session.revision());
    check(session.document()==expected&&encode(session.document())==linked_bytes,"One Redo restores both exact drivers and native bytes");
    auto updated=*session.document().objects.at("S").text;updated.content="Updated\nsource";
    apply(session,{UpdateText{"S",updated},Rename{"S","Renamed source"},ReorderObjects{"comp","",{"C","path","D","B","S"}}});
    linked(session,"B",content("S"),"Manual B");linked(session,"C",content("S"),"Manual C");
    check(evaluate_text_content(session.document(),"B")=="Updated\nsource"&&
        evaluate_text_content(session.document(),"C")=="Updated\nsource","Both targets follow source edits, rename and reorder by stable identity");
    const auto current=session.document();
    Session single(fixture());apply(single,{LinkTextContent{content("B"),content("S"),false}});
    check(single.revision()==1&&single.history().states.back().label=="Link Text content: Text B / text.content",
        "The existing single-target command and label remain unchanged");
    auto chained=fixture();chained.objects.at("S").text->content_driver=TextContentDriver{content("X")};
    Session chain(chained);apply(chain,fanout());
    linked(chain,"B",content("S"),"Manual B");linked(chain,"C",content("S"),"Manual C");
    check(evaluate_text_content(chain.document(),"B")=="Cross-Composition source","An already-driven source is linked directly without flattening");
    Session cross(fixture());apply(cross,fanout(content("X")));
    linked(cross,"B",content("X"),"Manual B");linked(cross,"C",content("X"),"Manual C");
    check(cross.document().compositions==original.compositions,"Same-Document cross-Composition linking keeps ownership unchanged");
    check(session.document()==current,"Independent Session checks never alter the original Session");
}
void replacement_and_editor_transaction() {
    auto original=fixture();original.objects.at("B").text->content_driver=TextContentDriver{content("D")};
    Session mixed(original);retain_redo(mixed);
    reject_batch(mixed,"DRIVEN_PROPERTY",{LinkTextContent{content("C"),content("S"),false},LinkTextContent{content("B"),content("S"),false}});
    apply(mixed,fanout(content("S"),true));
    linked(mixed,"B",content("S"),"Manual B");linked(mixed,"C",content("S"),"Manual C");
    mixed.undo(mixed.revision());
    check(mixed.document()==original,"One Undo restores a mixed pre-link driver and literal set");
    mixed.redo(mixed.revision());
    reject_batch(mixed,"DRIVEN_PROPERTY",fanout());
    reject_batch(mixed,"DRIVEN_PROPERTY",{LinkTextContent{content("B"),content("S"),false}});
    const auto rewrite_bytes=encode(mixed.document());const auto rewrite_revision=mixed.revision();const auto rewrite_history=mixed.history();
    apply(mixed,fanout(content("S"),true));
    check(encode(mixed.document())==rewrite_bytes&&mixed.revision()==rewrite_revision+1&&
        mixed.history().states.size()==rewrite_history.states.size()+1,"Explicit identical batch rewrite retains existing commit behavior");
    const auto single_revision=mixed.revision();apply(mixed,{LinkTextContent{content("B"),content("S"),true}});
    check(mixed.revision()==single_revision+1,"Explicit identical single rewrite also retains existing commit behavior");
    auto edit=*mixed.document().objects.at("B").text;edit.content="Typed draft";edit.content_driver.reset();
    const auto before_editor=mixed.document();const auto editor_revision=mixed.revision();const auto editor_states=mixed.history().states.size();
    apply(mixed,{UnlinkTextContent{content("B")},UpdateText{"B",edit}});
    check(mixed.document().objects.at("B").text->content=="Typed draft"&&!mixed.document().objects.at("B").text->content_driver,
        "Explicit UnlinkTextContent followed by UpdateText remains a valid literal editor transaction");
    check(mixed.revision()==editor_revision+1&&mixed.history().states.size()==editor_states+1,"Unlink and editor update remain one commit");
    mixed.undo(mixed.revision());check(mixed.document()==before_editor,"Editor transaction Undo restores the driver and prior authored literal exactly");
    mixed.redo(mixed.revision());
    auto source_edit=*mixed.document().objects.at("S").text;source_edit.content="Later source";apply(mixed,{UpdateText{"S",source_edit}});
    check(evaluate_text_content(mixed.document(),"B")=="Typed draft"&&evaluate_text_content(mixed.document(),"C")=="Later source",
        "Unlinked edited target remains literal while the other batch target continues following the source");
    auto two_drivers=fixture();two_drivers.objects.at("B").text->content_driver=TextContentDriver{content("D")};
    two_drivers.objects.at("C").text->content_driver=TextContentDriver{content("X")};Session different(two_drivers);
    apply(different,fanout(content("S"),true));different.undo(different.revision());
    check(different.document()==two_drivers,"One Undo restores two different pre-existing exact source refs");
}
void atomic_failures() {
    Session session(fixture());retain_redo(session);
    reject_batch(session,"INVALID_BATCH",{});
    reject_batch(session,"INVALID_BATCH",std::vector<Command>(1001,LinkTextContent{content("B"),content("S"),false}));
    reject_batch(session,"DUPLICATE_TARGET",{LinkTextContent{content("B"),content("S"),false},LinkTextContent{content("B"),content("S"),true}});
    reject_batch(session,"DUPLICATE_TARGET",{LinkTextContent{content("B"),content("S"),false},LinkTextContent{content("B"),content("D"),true}});
    reject_batch(session,"DUPLICATE_TARGET",{LinkTextContent{content("B"),content("S"),false},UnlinkTextContent{content("B")}});
    auto driven=fixture();driven.objects.at("B").text->content_driver=TextContentDriver{content("D")};Session unlink_first(driven);retain_redo(unlink_first);
    reject_batch(unlink_first,"DUPLICATE_TARGET",{UnlinkTextContent{content("B")},LinkTextContent{content("B"),content("S"),false}});
    reject_batch(unlink_first,"DUPLICATE_TARGET",{UnlinkTextContent{content("B")},UnlinkTextContent{content("B")}});
    const std::vector<std::pair<Ref,std::string>> invalid_refs{
        {content("missing"),"MISSING_REFERENCE"},{content("path"),"TYPE_MISMATCH"},
        {{"C","point","text.content"},"INVALID_TEXT_REF"},{{"C","","text.weight"},"TYPE_MISMATCH"}};
    for(const auto& [invalid,code]:invalid_refs) {
        reject_batch(session,code,{LinkTextContent{content("B"),content("S"),false},LinkTextContent{invalid,content("S"),false}});
        reject_batch(session,code,{LinkTextContent{content("B"),content("S"),false},LinkTextContent{content("C"),invalid,false}});
    }
    reject_batch(session,"DEPENDENCY_CYCLE",{LinkTextContent{content("B"),content("S"),false},LinkTextContent{content("C"),content("C"),false}});
    reject_batch(session,"DEPENDENCY_CYCLE",{LinkTextContent{content("B"),content("S"),false},LinkTextContent{content("S"),content("B"),false}});
    auto indirect=fixture();indirect.objects.at("S").text->content_driver=TextContentDriver{content("C")};Session cycle(indirect);retain_redo(cycle);
    reject_batch(cycle,"DEPENDENCY_CYCLE",fanout());
    reject(session,"REVISION_CONFLICT",[&]{session.apply(fanout(),session.revision()-1);});
    session.redo(session.revision());check(session.document().objects.at("D").name=="Temporary rename","A retained redo branch still executes after every failed batch");
    Session linked_session(fixture());apply(linked_session,fanout());retain_redo(linked_session);
    reject_batch(linked_session,"MISSING_REFERENCE",{UnlinkTextContent{content("B")},DeleteObjects{{"S"}}});
    auto driven_edit=*linked_session.document().objects.at("B").text;driven_edit.content="Forbidden literal edit";
    reject_batch(linked_session,"DRIVEN_PROPERTY",{UnlinkTextContent{content("C")},UpdateText{"B",driven_edit}});
    auto invalid_utf8=*linked_session.document().objects.at("S").text;invalid_utf8.content=std::string("\xc0\xaf",2);
    reject_batch(linked_session,"INVALID_UTF8",{UnlinkTextContent{content("B")},UpdateText{"S",invalid_utf8}});
    invalid_utf8.content=std::string(32769,'x');
    reject_batch(linked_session,"LIMIT",{UnlinkTextContent{content("B")},UpdateText{"S",invalid_utf8}});
    auto limited_document=fixture();limited_document.objects.at("B").text->content_driver=TextContentDriver{content("D")};
    Session measured(limited_document);apply(measured,fanout(content("S"),true));
    const auto batch_bytes=measured.history().retained_bytes;
    Session admission(limited_document,{100,batch_bytes-1});retain_redo(admission);
    reject_batch(admission,"HISTORY_LIMIT",fanout(content("S"),true));
}
void depth_size_and_generic_batches() {
    auto depth_document=fixture();
    for(int i=0;i<130;++i)add_text(depth_document,"comp","depth-"+std::to_string(i),"Depth "+std::to_string(i));
    for(int i=0;i<128;++i)depth_document.objects.at("depth-"+std::to_string(i)).text->content_driver=
        TextContentDriver{content("depth-"+std::to_string(i+1))};
    Session depth(depth_document);retain_redo(depth);
    check(evaluate_text_content(depth.document(),"depth-0")=="Depth 128","The existing 128-link depth is accepted");
    reject_batch(depth,"DEPENDENCY_DEPTH",{LinkTextContent{content("B"),content("S"),false},
        LinkTextContent{content("depth-128"),content("depth-129"),false}});
    auto maximum_document=fixture();std::vector<Command> maximum;
    for(int i=0;i<1000;++i) {
        const auto id="target-"+std::to_string(i);add_text(maximum_document,"comp",id,"Literal "+std::to_string(i));
        maximum.push_back(LinkTextContent{content(id),content("S"),false});
    }
    Session maximum_session(maximum_document);apply(maximum_session,maximum);
    check(maximum_session.revision()==1&&maximum_session.history().states.size()==2&&
        maximum_session.history().states.back().label=="Link Text content batch (1000)","The existing 1000-command upper boundary is accepted as one content batch");
    for(int i=0;i<1000;++i) {
        const auto id="target-"+std::to_string(i);linked(maximum_session,id,content("S"),"Literal "+std::to_string(i));
    }
    maximum_session.undo(maximum_session.revision());
    check(maximum_session.document()==maximum_document,"One Undo restores all 1000 targets without state drift");
    Session independent(fixture());apply(independent,{LinkTextContent{content("B"),content("S"),false},LinkTextContent{content("C"),content("D"),false}});
    check(independent.history().states.back().label=="Link Text content: Text B / text.content (2 commands)",
        "Unrelated independent content links with different sources remain legal and retain the generic label");
    Session unrelated(fixture());auto commands=fanout();commands.push_back(Rename{"D","Renamed unrelated object"});apply(unrelated,commands);
    check(unrelated.history().states.back().label=="Link Text content: Text B / text.content (3 commands)",
        "A mixed command batch stays legal and retains its existing generic history label");
    Session distinct(fixture());apply(distinct,{LinkTextContent{content("B"),content("S"),false},UnlinkTextContent{content("C")}});
    check(distinct.document().objects.at("B").text->content_driver.has_value()&&!distinct.document().objects.at("C").text->content_driver,
        "Link and unlink of distinct targets remain legal in the same generic batch");
}
}
int main() {
    try {
        fanout_history_and_identity();replacement_and_editor_transaction();atomic_failures();depth_size_and_generic_batches();
        std::cout<<"Text content batch contract: "<<checks<<" checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Text content batch contract: "<<error.what()<<'\n';
        return 1;
    }
}
