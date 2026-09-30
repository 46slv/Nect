#include "nect/io.hpp"
#include <algorithm>
#include <iostream>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F>void rejects(const char* code,F action) {
    try{action();}catch(const Error& error){check(error.code==code,
        "Expected "+std::string(code)+", got "+error.code+": "+error.what());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
Object path(Id id,double x) {
    Object object;object.id=id;object.name=id;
    Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(const auto& xy:std::vector<Vec2>{{x,0},{x+20,0},{x+20,20},{x,20}}) {
        Point point;point.id=id+"-point-"+std::to_string(contour.points.size());
        point.x.literal=xy.x;point.y.literal=xy.y;contour.points.push_back(point);
    }
    object.contours.push_back(contour);object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));
    return object;
}
Document fixture() {
    auto document=empty_document("collection-doc","comp","art");
    for(const auto& id:std::vector<Id>{"A","B","C"})document.objects.emplace(id,path(id,id=="A"?0:id=="B"?30:60));
    Object group;group.id="G";group.name="G";group.kind=Kind::group;group.children={"D"};
    document.objects.emplace("G",group);document.objects.emplace("D",path("D",90));
    document.compositions.front().roots={"A","B","C","G"};return document;
}
void same_artwork(const Document& before,const Document& after) {
    check(before.objects==after.objects&&before.compositions==after.compositions,
        "Collection mutation preserves Object hierarchy, order, transforms and authored art");
    check(export_svg(before,"comp","art")==export_svg(after,"comp","art"),
        "Collection-only mutation preserves rendered SVG output");
}
void atomic_rejection(Session& session,const char* code,std::vector<Command> commands,std::uint64_t expected) {
    const auto before=session.document();const auto bytes=encode(before);
    const auto revision=session.revision();const auto history=session.history();
    rejects(code,[&]{session.apply(commands,expected);});
    check(session.document()==before&&encode(session.document())==bytes&&session.revision()==revision&&
        session.history()==history,"Failed Collection command preserves Document, native bytes, revision and history");
}
void membership_native_and_history() {
    Session session(fixture());const auto before=session.document();
    apply(session,{CollectionCommand{CreateCollection{{"K","Primary",{"A","B"}}}}});
    same_artwork(before,session.document());
    check(session.document().collections.size()==1&&session.document().collections.front().members==std::vector<Id>({"A","B"}),
        "Create Collection stores ordered stable Object IDs without expanding Group descendants");
    const auto first=encode(session.document());
    check(decode(first)==session.document(),"Native 0.67 cold reopen preserves Collection identity and member order");
    apply(session,{CollectionCommand{SetCollectionMembers{"K",{"A","B","C","D"}}}});
    check(session.document().collections.front().members==std::vector<Id>({"A","B","C","D"}),
        "Explicit D membership does not imply G membership");
    same_artwork(before,session.document());
    session.undo(session.revision());
    check(encode(session.document())==first,"One Undo restores the exact prior Collection membership");
    session.redo(session.revision());
    check(session.document().collections.front().members==std::vector<Id>({"A","B","C","D"}),
        "One Redo restores ordered members");
    apply(session,{Rename{"A","Renamed A"},CollectionCommand{RenameCollection{"K","Renamed K"}}});
    check(session.document().collections.front().members.front()=="A"&&
        session.document().collections.front().name=="Renamed K","Rename preserves member and Collection IDs");
    apply(session,{GroupContiguous{"comp","",{"B"},"B-folder","B Folder"}});
    check(session.document().collections.front().members[1]=="B"&&
        session.document().objects.at("B-folder").children==std::vector<Id>({"B"}),
        "Reparenting B through the existing Group command does not retarget Collection membership");
    apply(session,{CollectionCommand{CreateCollection{{"K2","Secondary",{"A","G"}}}}});
    check(session.document().collections.size()==2&&session.document().collections[1].members==std::vector<Id>({"A","G"}),
        "One Object may belong to two Collections and G does not imply child D");
    apply(session,{DeleteObjects{{"A"}}});
    check(session.document().collections.front().members==std::vector<Id>({"B","C","D"})&&
        session.document().collections[1].members==std::vector<Id>({"G"}),
        "Source deletion prunes membership in every Collection");
    apply(session,{CollectionCommand{DeleteCollection{"K2"}}});
    check(session.document().collections.size()==1&&session.document().objects.contains("G"),
        "Deleting Collection keeps member artwork");
}
void validation_and_api() {
    Session session(fixture());
    atomic_rejection(session,"DUPLICATE_ID",{CollectionCommand{CreateCollection{{"A","Collision",{}}}}},0);
    atomic_rejection(session,"DUPLICATE_MEMBER",{CollectionCommand{CreateCollection{{"K","Duplicate",{"A","A"}}}}},0);
    atomic_rejection(session,"MISSING_OBJECT",{CollectionCommand{CreateCollection{{"K","Missing",{"ghost"}}}}},0);
    atomic_rejection(session,"REVISION_CONFLICT",{CollectionCommand{CreateCollection{{"K","Valid",{"A"}}}}},1);
    std::vector<Id> excessive(10001,"A");
    atomic_rejection(session,"LIMIT",{CollectionCommand{CreateCollection{{"K","Too many",excessive}}}},0);
    const auto result=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"create_collection","id":"K","name":"API K","members":["A","B"]}]})");
    check(result.find("\"changed\":true")!=std::string::npos&&
        request(session,R"({"op":"collections"})").find("\"members\":[\"A\",\"B\"]")!=std::string::npos&&
        request(session,R"({"op":"collection","id":"K"})").find("API K")!=std::string::npos,
        "JSON-lines API creates and reads Collection stable IDs and ordered members");
    check(request(session,R"({"op":"apply","expected_revision":1,"commands":[{"type":"set_collection_members","collection":"K","members":["B","D"]}]})")
        .find("\"changed\":true")!=std::string::npos&&
        session.document().collections.front().members==std::vector<Id>({"B","D"}),
        "API SetCollectionMembers updates one canonical ordered list");
    check(request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"rename_collection","collection":"K","name":"Stale"}]})")
        .find("REVISION_CONFLICT")!=std::string::npos,"API rejects stale Collection revision");
    check(request(session,R"({"op":"apply","expected_revision":2,"commands":[{"type":"rename_collection","collection":"K","name":"Renamed"},{"type":"delete_collection","collection":"K"}]})")
        .find("\"changed\":true")!=std::string::npos&&session.document().collections.empty(),
        "API rename and delete share one atomic Session command batch");
}
}
int main() {
    try{membership_native_and_history();validation_and_api();
        std::cout<<"PASS "<<checks<<" Collection core, API, native and history checks\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
