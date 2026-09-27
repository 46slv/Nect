#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);++checks;}
void near(double a,double b,const char* message){check(std::abs(a-b)<1e-8,message);}
template<class F>void rejects(const char* code,F action) {
    try{action();}catch(const Error& error){check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
void atomic(Session& session,const char* code,std::vector<Command> commands) {
    const auto before=session.document();const auto rev=session.revision();const auto history=session.history();
    rejects(code,[&]{apply(session,std::move(commands));});
    check(session.document()==before&&session.revision()==rev&&session.history()==history,"Rejected composite command is atomic");
}
void atomic_reason(Session& session,const char* code,const char* reason,std::vector<Command> commands) {
    const auto before=session.document();const auto rev=session.revision();const auto history=session.history();
    try{apply(session,std::move(commands));}
    catch(const Error& error) {
        check(error.code==code,"Expected "+std::string(code)+", got "+error.code+": "+error.what());
        check(std::string(error.what()).find(reason)!=std::string::npos,"Appearance refusal names boundary: "+std::string(error.what()));
        check(session.document()==before&&session.revision()==rev&&session.history()==history,"Appearance refusal retains document, revision and history");
        return;
    }
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
Object rectangle(Id id,double x=0,double y=0) {
    Object object;object.id=id;object.name=id;Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(const auto& xy:std::vector<Vec2>{{x,y},{x+40,y},{x+40,y+30},{x,y+30}}) {
        Point p;p.id=id+"-p"+std::to_string(contour.points.size());p.x.literal=xy.x;p.y.literal=xy.y;contour.points.push_back(p);
    }
    object.contours.push_back(contour);object.stack.push_back(default_operation(id+"-fill","nect.paint.fill"));return object;
}
void matrix(Object& object,Affine a){for(std::size_t i=0;i<6;++i)object.transform[i].literal=a[i];}
Document fixture() {
    auto document=empty_document("doc","comp","art");
    for(const auto* id:{"a","b","source"}){document.objects.emplace(id,rectangle(id));document.compositions[0].roots.push_back(id);}return document;
}
EvaluatedScene scene(const Document& document){const auto values=evaluate(document);return evaluate_scene(document,"comp",values,evaluate_transforms(document,values));}
void object_visibility_read_contract() {
    Session session(fixture());
    const Ref ref{"a","","object.visible"};
    const auto refs=properties(session.document());
    check(std::find(refs.begin(),refs.end(),ref)!=refs.end()&&
        resolve_name(session.document(),"a","","object.visible")==ref&&
        object_visibility_property(session.document(),ref),
        "Object visibility is discoverable by stable ID and unique name");
    const auto initial=request(session,R"({"op":"get","ref":{"object":"a","point":"","field":"object.visible"}})");
    check(initial.find("\"type\":\"bool\"")!=std::string::npos&&
        initial.find("\"literal\":true")!=std::string::npos&&
        initial.find("\"evaluated\":true")!=std::string::npos&&
        initial.find("\"driver\":null")!=std::string::npos&&
        initial.find("\"link\":true")!=std::string::npos,
        "API get exposes authored visibility and the stable-link capability");
    rejects("INVALID_OBJECT_REF",[&]{(void)object_visibility_property(session.document(),{"a","point","object.visible"});});
    rejects("TYPE_MISMATCH",[&]{(void)object_visibility_property(session.document(),{"a","","object.other"});});
    rejects("MISSING_REFERENCE",[&]{(void)object_visibility_property(session.document(),{"missing","","object.visible"});});
    atomic(session,"MISSING_REFERENCE",{SetVisibility{"a",false},Set{ref,0}});
    rejects("REVISION_CONFLICT",[&]{session.apply({SetVisibility{"a",false}},session.revision()+1);});
    check(session.revision()==0&&object_visibility_property(session.document(),ref),
        "Stale visibility draft leaves the committed literal and revision unchanged");
    apply(session,{SetVisibility{"a",false}});
    check(!object_visibility_property(session.document(),ref)&&
        !scene(session.document()).roots.front().visible,
        "Shared SetVisibility changes the typed value and scene consumer");
    const auto hidden=request(session,R"({"op":"get","ref":{"object":"a","point":"","field":"object.visible"}})");
    check(hidden.find("\"literal\":false")!=std::string::npos&&
        hidden.find("\"evaluated\":false")!=std::string::npos,
        "API get reads the same hidden authored value");
    const auto saved=encode(session.document());
    check(object_visibility_property(decode(saved),ref)==false&&encode(decode(saved))==saved,
        "Native roundtrip retains the authored visibility without migration");
    session.undo(session.revision());
    check(object_visibility_property(session.document(),ref),"Undo restores authored visibility");
    apply(session,{Rename{"a","Renamed"}});
    check(resolve_name(session.document(),"Renamed","","object.visible")==ref,
        "Rename preserves the stable visibility Ref");
}
void object_visibility_link_contract() {
    auto document=fixture();document.objects.at("source").visible=false;
    Session session(document);const Ref target{"a","","object.visible"},source{"source","","object.visible"};
    const auto link=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"link_object_visibility","target":{"object":"a","point":"","field":"object.visible"},"source":{"object":"source","point":"","field":"object.visible"},"replace_driver":false}]})");
    check(link.find("\"changed\":true")!=std::string::npos&&session.revision()==1,
        "JSON-lines links Object visibility through the shared Session command");
    auto state=object_visibility_state(session.document(),target);
    check(state.literal&&state.driver==source&&!state.evaluated,
        "A stable same-field link preserves the target literal while evaluating the source value");
    const auto get=request(session,R"({"op":"get","ref":{"object":"a","point":"","field":"object.visible"}})");
    check(get.find("\"authored\":{\"literal\":true,\"driver\":{\"link\":{\"object\":\"source\",\"point\":\"\",\"field\":\"object.visible\"}}}")!=std::string::npos&&
        get.find("\"evaluated\":false")!=std::string::npos&&get.find("\"link\":true")!=std::string::npos,
        "API get reports literal, stable driver Ref and evaluated own value separately");
    const auto linked_native=encode(session.document());
    check(linked_native.find("\"version\":\"0.29\"")!=std::string::npos&&
        linked_native.find("\"visibility_driver\":{\"link\":{\"object\":\"source\",\"point\":\"\",\"field\":\"object.visible\"}}")!=std::string::npos&&
        decode(linked_native)==session.document(),
        "Native 0.29 roundtrip preserves the optional stable visibility driver");
    atomic(session,"DRIVEN_PROPERTY",{SetVisibility{"a",true}});
    atomic(session,"DRIVEN_PROPERTY",{LinkObjectVisibility{target,{"b","","object.visible"},false}});
    atomic(session,"DUPLICATE_TARGET",{LinkObjectVisibility{target,source,false},UnlinkObjectVisibility{target}});
    atomic(session,"DEPENDENCY_CYCLE",{LinkObjectVisibility{source,target,false}});

    apply(session,{SetVisibility{"b",false},LinkObjectVisibility{target,{"b","","object.visible"},true}});
    state=object_visibility_state(session.document(),target);
    check(state.literal&&!state.evaluated&&state.driver==Ref{"b","","object.visible"},
        "Explicit replacement changes only the driver and uses its evaluated boolean");
    apply(session,{UnlinkObjectVisibility{target}});
    state=object_visibility_state(session.document(),target);
    check(!state.literal&&!state.evaluated&&!state.driver,
        "Unlink freezes the current evaluated own value into the authored literal");
    session.undo(session.revision());
    state=object_visibility_state(session.document(),target);
    check(state.driver==Ref{"b","","object.visible"}&&state.literal,
        "Undo restores the exact driven state before unlink");
    session.redo(session.revision());
    apply(session,{SetVisibility{"b",true}});
    check(!object_visibility_state(session.document(),target).evaluated,
        "A frozen literal no longer follows the former source");

    auto deletion_document=fixture();deletion_document.objects.at("source").visible=false;
    deletion_document.objects.at("a").visibility_driver=source;
    Session deletion(std::move(deletion_document));
    atomic(deletion,"MISSING_REFERENCE",{DeleteObjects{{"source"}}});
    apply(deletion,{UnlinkObjectVisibility{target},DeleteObjects{{"source"}}});
    check(deletion.revision()==1&&!deletion.document().objects.contains("source")&&
        !deletion.document().objects.at("a").visibility_driver&&
        !object_visibility_state(deletion.document(),target).literal,
        "Deleting a visibility source is atomic unless the target is first unlinked and frozen in the same batch");

    auto old_literal=encode(document);const auto current_version=old_literal.find("\"version\":\"0.29\"");
    check(current_version!=std::string::npos,"Native writer emits 0.29 for the 0.27 compatibility fixture");
    old_literal.replace(current_version,std::string("\"version\":\"0.29\"").size(),"\"version\":\"0.27\"");
    check(decode(old_literal)==document,"Native 0.27 literal-only documents remain readable unchanged");
    auto false_version=linked_native;const auto linked_version=false_version.find("\"version\":\"0.29\"");
    false_version.replace(linked_version,std::string("\"version\":\"0.29\"").size(),"\"version\":\"0.26\"");
    rejects("UNKNOWN_FIELD",[&]{decode(false_version);});

    auto cross_document=fixture();auto other=empty_document("other-doc","other-comp","other-art").compositions.front();
    auto external=rectangle("external");cross_document.objects.emplace(external.id,external);other.roots.push_back(external.id);
    cross_document.compositions.push_back(other);Session cross(cross_document);
    atomic(cross,"CROSS_COMPOSITION",{LinkObjectVisibility{target,{"external","","object.visible"},false}});

    auto deep=empty_document("depth-doc","depth-comp","depth-art");std::vector<Command> links;
    for(int i=0;i<130;++i) {
        Object object;object.id="visibility-"+std::to_string(i);object.name=object.id;
        deep.objects.emplace(object.id,object);deep.compositions.front().roots.push_back(object.id);
        if(i<129)links.push_back(LinkObjectVisibility{{object.id,"","object.visible"},{"visibility-"+std::to_string(i+1),"","object.visible"},false});
    }
    Session depth(std::move(deep));atomic(depth,"DEPENDENCY_DEPTH",std::move(links));
}
void mask_enabled_read_contract() {
    auto document=fixture();
    document.objects.at("a").compositing.mask=GeometryMask{"mask-a","source",1,true,"nonzero"};
    Session session(document);
    const Ref ref{"a","","mask.enabled"};
    const auto refs=properties(session.document());
    check(std::find(refs.begin(),refs.end(),ref)!=refs.end()&&
        resolve_name(session.document(),"a","","mask.enabled")==ref&&
        geometry_mask_enabled_property(session.document(),ref)&&property_unit(ref)=="boolean",
        "Present optional mask exposes a stable owner Ref by ID and unique name as a boolean");
    const auto initial=request(session,R"({"op":"get","ref":{"object":"a","point":"","field":"mask.enabled"}})");
    check(initial.find("\"type\":\"bool\"")!=std::string::npos&&
        initial.find("\"unit\":\"boolean\"")!=std::string::npos&&
        initial.find("\"origin\":\"authored\"")!=std::string::npos&&
        initial.find("\"literal\":true")!=std::string::npos&&
        initial.find("\"evaluated\":true")!=std::string::npos&&
        initial.find("\"driver\":null")!=std::string::npos&&
        initial.find("\"link\":false")!=std::string::npos&&
        initial.find("\"expression\":false")!=std::string::npos,
        "API get reports the authored mask bypass bit without dependency capability");
    rejects("INVALID_OBJECT_REF",[&]{(void)geometry_mask_enabled_property(session.document(),{"a","point","mask.enabled"});});
    rejects("TYPE_MISMATCH",[&]{(void)geometry_mask_enabled_property(session.document(),{"a","","object.visible"});});
    rejects("MISSING_REFERENCE",[&]{(void)geometry_mask_enabled_property(session.document(),{"missing","","mask.enabled"});});
    atomic(session,"MISSING_REFERENCE",{SetMask{"a",GeometryMask{"mask-a","source",1,false,"nonzero"}},Set{ref,1}});
    rejects("REVISION_CONFLICT",[&]{session.apply({SetMask{"a",GeometryMask{"mask-a","source",1,false,"nonzero"}},SetMask{"a",GeometryMask{"mask-a","source",1,true,"nonzero"}}},session.revision()+1);});

    auto disabled=*session.document().objects.at("a").compositing.mask;
    disabled.enabled=false;
    apply(session,{SetMask{"a",disabled}});
    check(!geometry_mask_enabled_property(session.document(),ref)&&!scene(session.document()).roots.front().mask,
        "SetMask changes the same typed bit and scene bypasses clipping while retaining the mask source object");
    const auto disabled_get=request(session,R"({"op":"get","ref":{"object":"a","point":"","field":"mask.enabled"}})");
    check(disabled_get.find("\"literal\":false")!=std::string::npos&&disabled_get.find("\"evaluated\":false")!=std::string::npos,
        "API read follows the disabled authored literal");
    session.undo(session.revision());
    check(geometry_mask_enabled_property(session.document(),ref)&&scene(session.document()).roots.front().mask,
        "Undo restores mask clipping and the exact enabled literal");
    const auto saved=encode(session.document());
    check(geometry_mask_enabled_property(decode(saved),ref)&&decode(saved).objects.at("a").compositing.mask->id=="mask-a"&&encode(decode(saved))==saved,
        "Native 0.29 roundtrip preserves mask enable and identity without byte drift");
    const auto all=request(session,R"({"op":"properties"})");
    check(all.find("\"field\":\"mask.enabled\"")!=std::string::npos,
        "Properties enumeration includes only the present optional mask field");
    atomic(session,"MISSING_REFERENCE",{Set{ref,0}});
    apply(session,{SetMask{"a",std::nullopt}});
    const auto removed_refs=properties(session.document());
    check(std::find(removed_refs.begin(),removed_refs.end(),ref)==removed_refs.end(),
        "Removing the mask removes the optional property from discovery");
    rejects("MISSING_MASK",[&]{(void)geometry_mask_enabled_property(session.document(),ref);});
    rejects("MISSING_MASK",[&]{(void)resolve_name(session.document(),"a","","mask.enabled");});
    const auto missing_get=request(session,R"({"op":"get","ref":{"object":"a","point":"","field":"mask.enabled"}})");
    check(missing_get.find("\"code\":\"MISSING_MASK\"")!=std::string::npos,
        "Exact get after removal rejects with MISSING_MASK instead of inventing false");
    apply(session,{SetMask{"a",GeometryMask{"replacement-mask","source",1,true,"nonzero"}}});
    check(geometry_mask_enabled_property(session.document(),ref)&&session.document().objects.at("a").compositing.mask->id=="replacement-mask",
        "Owner-slot Ref becomes valid for a replacement mask without claiming persistent mask identity");
}
void object_visibility_consumer_parity() {
    auto document=fixture();document.objects.at("source").visible=true;document.objects.at("b").visible=false;
    Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"a"};
    group.visibility_driver=Ref{"b","","object.visible"};document.objects.emplace(group.id,group);
    document.objects.at("a").visibility_driver=Ref{"source","","object.visible"};
    document.compositions.front().roots={"group","b","source"};
    const auto evaluated=scene(document);
    check(!evaluated.roots.front().visible&&evaluated.roots.front().children.front().visible&&
        evaluate_object_visibility(document,"a"),
        "Scene exposes each node's evaluated own visibility while preserving a true child beneath a hidden Group");
    const auto nested_svg=export_svg(document,"comp","art");
    check(nested_svg.find("<g id=\"group\"")==std::string::npos&&nested_svg.find("<g id=\"a\"")==std::string::npos,
        "SVG suppresses a linked-hidden Group and its otherwise-visible descendants");

    auto linked_hidden=fixture();linked_hidden.objects.at("source").visible=false;
    linked_hidden.objects.at("a").visibility_driver=Ref{"source","","object.visible"};
    check(linked_hidden.objects.at("a").visible&&!evaluate_object_visibility(linked_hidden,"a"),
        "A linked false value is independent from the authored true literal");
    const auto svg=export_svg(linked_hidden,"comp","art");
    check(svg.find("<g id=\"a\"")==std::string::npos&&svg.find("<g id=\"b\"")!=std::string::npos,
        "SVG switches to evaluated modern visibility when a true literal links to false");
}
std::map<Id,EvaluatedTransform> transforms(const Document& document){return evaluate_transforms(document,evaluate(document));}
void same_matrix(const Affine& a,const Affine& b){for(std::size_t i=0;i<6;++i)near(a[i],b[i],"World placement preserved");}
std::vector<Id> drawable_order(const Document& document,const EvaluatedScene& evaluated) {
    std::vector<Id> result;
    std::function<void(const EvaluatedSceneNode&)> append=[&](const EvaluatedSceneNode& node) {
        if(document.objects.at(node.id).kind==Kind::group)for(const auto& child:node.children)append(child);
        else result.push_back(node.id);
    };
    for(const auto& root:evaluated.roots)append(root);
    return result;
}

void create_empty_folder() {
    auto document=fixture();document.compositions.front().roots={"source","a","b"};
    document.collections={{"collection","Stable members",{"a","b"}}};
    document.objects.at("b").transform[4].binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};
    Session session(document);const auto before=session.document();const auto before_values=evaluate(before);
    const auto before_transforms=transforms(before);const auto before_scene=scene(before);
    const auto response=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"create_folder","composition":"comp","parent":"","id":"folder","name":"Folder"}]})");
    check(response.find("\"changed\":true")!=std::string::npos,"JSON-lines API accepts Create Folder through the shared Session");
    const auto& composition=session.document().compositions.front();
    check(composition.roots==std::vector<Id>{"source","a","b","folder"},"Create Folder appends after existing roots");
    const auto& folder=session.document().objects.at("folder");
    check(folder.kind==Kind::group&&folder.name=="Folder"&&folder.children.empty()&&folder.stack.empty()&&
        folder.compositing==Compositing{}&&!folder.source&&!folder.text&&!folder.image&&!folder.transform_parent&&folder.visible,
        "New Folder is an empty neutral Group in the existing native model");
    check(session.document().collections==before.collections,"Folder creation does not change Collection membership");
    for(const auto& [id,object]:before.objects)check(session.document().objects.at(id)==object,"Existing authored object bytes remain unchanged");
    const auto after_values=evaluate(session.document());const auto after_transforms=transforms(session.document());
    check(after_transforms.at("folder").world==identity_matrix,"New Folder starts with an identity transform");
    for(const auto& [ref,value]:before_values)check(after_values.at(ref)==value,"Existing property values and stable references remain unchanged");
    for(const auto& [id,transform]:before_transforms)check(after_transforms.at(id).world==transform.world,"Existing world transforms remain unchanged");
    const auto after_scene=scene(session.document());
    check(drawable_order(before,before_scene)==drawable_order(session.document(),after_scene),
        "An empty Folder adds no drawable content and preserves paint order");
    check(decode(encode(session.document()))==session.document(),"Folder survives native 0.20 encode/decode exactly");
    const auto created=session.document();const auto created_revision=session.revision();const auto created_history=session.history();
    rejects("REVISION_CONFLICT",[&]{session.apply({CreateFolder{"comp","","stale-folder","Stale"}},0);});
    check(session.document()==created&&session.revision()==created_revision&&session.history()==created_history,
        "Stale Create Folder revision rejects without an authored delta");
    session.undo(session.revision());check(session.document()==before,"Create Folder has one exact Undo boundary");
    session.redo(session.revision());check(session.document()==created,"Create Folder has one exact Redo boundary");
    check(!created_history.states.empty()&&created_history.states.back().label=="Create Folder: Folder","History names the new Folder action");

    const auto before_move_values=evaluate(created);const auto before_move_transforms=transforms(created);
    const auto before_move_scene=scene(created);
    apply(session,{PutInside{"comp","","folder",{"a","b"}}});
    const auto moved=session.document();
    check(moved.compositions.front().roots==std::vector<Id>{"source","folder"}&&
        moved.objects.at("folder").children==std::vector<Id>{"a","b"},
        "Put Inside moves the immediately preceding A/B block into the new Folder in paint order");
    for(const auto* id:{"a","b"}) {
        check(moved.objects.at(id)==created.objects.at(id),"Put Inside retains A/B stable authored object state");
        check(transforms(moved).at(id).world==before_move_transforms.at(id).world,"Put Inside preserves A/B world transforms");
    }
    const auto moved_values=evaluate(moved);
    for(const auto& [ref,value]:before_move_values)check(moved_values.at(ref)==value,"Put Inside preserves stable property references and values");
    check(drawable_order(created,before_move_scene)==drawable_order(moved,scene(moved)),
        "Put Inside preserves the existing drawable paint order");
    check(decode(encode(moved))==moved,"Folder organization survives native 0.20 encode/decode exactly");
    session.undo(session.revision());check(session.document()==created,"Put Inside into Folder is one exact Undo");
    session.redo(session.revision());check(session.document()==moved,"Put Inside into Folder is one exact Redo");

    atomic(session,"DUPLICATE_ID",{CreateFolder{"comp","","a","Duplicate"}});
    atomic(session,"DUPLICATE_ID",{CreateFolder{"comp","","a-p0","Point collision"}});
    atomic(session,"MISSING_COMPOSITION",{CreateFolder{"missing","","missing-comp","Missing composition"}});
    atomic(session,"INVALID_PARENT",{CreateFolder{"comp","missing-parent","missing-parent-child","Missing parent"}});
    auto separate=fixture();Object foreign;foreign.id="foreign-parent";foreign.name="Foreign";foreign.kind=Kind::group;
    separate.objects.emplace(foreign.id,foreign);separate.compositions.push_back(Composition{"other","Other",{"foreign-parent"},{}});
    Session planes(separate);atomic(planes,"INVALID_PARENT",{CreateFolder{"comp","foreign-parent","foreign-child","Foreign parent"}});
}

void batch_rename_api() {
    auto document=fixture();
    document.collections={{"collection","Stable members",{"a","b","source"}}};
    document.objects.at("source").contours[0].points[0].x.binding=
        Binding{{"a","","transform.tx"},2,3,"copy_local_value"};
    Session session(document);
    const auto before=session.document();const auto before_values=evaluate(before);const auto before_scene=scene(before);
    const auto response=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"rename","object":"a","name":"Renamed A"},{"type":"rename","object":"b","name":"Renamed B"},{"type":"rename","object":"source","name":"Renamed source"}]})");
    check(response.find("\"changed\":true")!=std::string::npos,"JSON-lines accepts an ordered three-command Rename batch through Session.apply");
    auto expected=before;expected.objects.at("a").name="Renamed A";expected.objects.at("b").name="Renamed B";expected.objects.at("source").name="Renamed source";
    check(session.revision()==1&&session.document()==expected,"API Rename batch changes only the three stable object names in one revision");
    check(session.document().collections==before.collections&&evaluate(session.document())==before_values&&
        drawable_order(before,before_scene)==drawable_order(session.document(),scene(session.document())),
        "API Rename batch preserves Collection members, driven values and paint order");
    check(decode(encode(session.document()))==session.document(),"API Rename batch preserves native source and stable references exactly");
    const auto applied=session.document();session.undo(session.revision());check(session.document()==before,"API Rename batch has one exact Undo boundary");
    session.redo(session.revision());check(session.document()==applied,"API Rename batch Redo restores every proposed name");

    const auto stable=session.document();const auto stable_revision=session.revision();const auto stable_history=session.history();
    const auto missing=request(session,R"({"op":"apply","expected_revision":3,"commands":[{"type":"rename","object":"a","name":"Changed"},{"type":"rename","object":"missing","name":"Missing"},{"type":"rename","object":"source","name":"Also changed"}]})");
    check(missing.find("\"code\":\"MISSING_OBJECT\"")!=std::string::npos&&session.document()==stable&&
        session.revision()==stable_revision&&session.history()==stable_history,
        "A missing middle API Rename target rejects the whole batch without partial state or history");
    const auto stale=request(session,R"({"op":"apply","expected_revision":1,"commands":[{"type":"rename","object":"a","name":"Stale"},{"type":"rename","object":"b","name":"Stale"}]})");
    check(stale.find("\"code\":\"REVISION_CONFLICT\"")!=std::string::npos&&session.document()==stable&&
        session.revision()==stable_revision&&session.history()==stable_history,
        "A stale API Rename batch refuses atomically");
}

void sort_paint_order_api() {
    auto document=fixture();
    document.collections={{"collection","Stable members",{"a","b","source"}}};
    document.objects.at("source").contours[0].points[0].x.binding=
        Binding{{"a","","transform.tx"},2,3,"copy_local_value"};
    Session session(document);
    const auto before=session.document();const auto before_values=evaluate(before);
    const auto response=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"reorder_objects","composition":"comp","parent":"","order":["b","a","source"]}]})");
    check(response.find("\"changed\":true")!=std::string::npos,
        "JSON-lines reaches shared ReorderObjects for a full paint-order permutation");
    auto expected=before;expected.compositions.front().roots={"b","a","source"};
    check(session.revision()==1&&session.document()==expected&&evaluate(session.document())==before_values&&
        session.document().collections==before.collections,
        "API paint-order sort changes only sibling order while stable references and Collection membership remain");
    const auto stable=session.document();const auto revision=session.revision();const auto history=session.history();
    const auto invalid=request(session,R"({"op":"apply","expected_revision":1,"commands":[{"type":"reorder_objects","composition":"comp","parent":"","order":["a","a","source"]}]})");
    check(invalid.find("\"code\":\"INVALID_ORDER\"")!=std::string::npos&&
        session.document()==stable&&session.revision()==revision&&session.history()==history,
        "Invalid API paint order rejects atomically without Document, revision or history delta");
}

void scene_contract() {
    Session session(fixture());auto evaluated=scene(session.document());check(!evaluated.requires_compositing&&evaluated.roots.size()==3,"Neutral scene retains direct rendering");
    apply(session,{GroupContiguous{"comp","",{"a","b"},"group","Group"}});
    evaluated=scene(session.document());check(!evaluated.roots[0].isolated&&evaluated.roots[0].children[0].id=="a"&&evaluated.roots[0].children[1].id=="b","Neutral group preserves paint order and pass-through boundary");
    const Ref isolated{"group","","composite.isolated"};
    const auto discovered=properties(session.document());
    check(std::find(discovered.begin(),discovered.end(),isolated)!=discovered.end(),
        "Composite isolation has a typed property address");
    check(resolve_name(session.document(),"Group","","composite.isolated")==isolated&&
        !composite_isolated_property(session.document(),isolated),
        "Neutral Group authored isolation is false by stable ID and name");
    rejects("INVALID_OBJECT_REF",[&]{(void)composite_isolated_property(session.document(),{"group","point","composite.isolated"});});
    rejects("TYPE_MISMATCH",[&]{(void)composite_isolated_property(session.document(),{"group","","composite.other"});});
    rejects("MISSING_REFERENCE",[&]{(void)composite_isolated_property(session.document(),{"missing","","composite.isolated"});});
    atomic(session,"MISSING_REFERENCE",{SetCompositing{"group","normal",true},Set{isolated,1}});
    rejects("REVISION_CONFLICT",[&]{session.apply({SetCompositing{"group","normal",true}},session.revision()+1);});
    check(!composite_isolated_property(session.document(),isolated),"Stale isolation edit leaves authored state unchanged");
    apply(session,{SetExpression{{{"group","","composite.opacity"}},{".5"}}});
    evaluated=scene(session.document());check(evaluated.roots[0].isolated&&evaluated.roots[0].opacity==.5,"Opacity is an ordinary expression-capable Scalar and aggregate boundary");
    atomic(session,"DRIVEN_PROPERTY",{Set{{"group","","composite.opacity"},.8}});
    apply(session,{Unlink{{"group","","composite.opacity"}},Set{{"group","","composite.opacity"},1}});
    for(const auto* blend:{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn","hard-light","soft-light","difference","exclusion"}) {
        apply(session,{SetCompositing{"group",blend,false}});evaluated=scene(session.document());
        check(evaluated.roots[0].blend==blend&&evaluated.roots[0].isolated==(std::string(blend)!="normal"),"Supported blend resolves exact isolation semantics");
        check(!composite_isolated_property(session.document(),isolated),
            "Non-normal blend may isolate the scene while the authored isolation remains false");
    }
    atomic(session,"UNSUPPORTED_BLEND",{SetCompositing{"group","plus",false}});
    atomic(session,"OUT_OF_RANGE",{Set{{"group","","composite.opacity"},1.1}});
    apply(session,{SetCompositing{"group","normal",true}});
    check(scene(session.document()).roots[0].isolated&&composite_isolated_property(session.document(),isolated),
        "Explicit normal isolation changes both authored typed state and scene");
    const auto isolated_get=request(session,R"({"op":"get","ref":{"object":"group","point":"","field":"composite.isolated"}})");
    check(isolated_get.find("\"type\":\"bool\"")!=std::string::npos&&
        isolated_get.find("\"literal\":true")!=std::string::npos&&
        isolated_get.find("\"evaluated\":true")!=std::string::npos,
        "API get returns authored isolation without a derived scene projection");
    const auto native=encode(session.document());
    check(composite_isolated_property(decode(native),isolated)&&encode(decode(native))==native,
        "Native retains the existing authored isolation bit exactly");
    session.undo(session.revision());
    check(!composite_isolated_property(session.document(),isolated),"Undo restores authored isolation");
    session.redo(session.revision());
    apply(session,{Rename{"group","Renamed Group"}});
    check(resolve_name(session.document(),"Renamed Group","","composite.isolated")==isolated,
        "Renaming a Group preserves its stable isolation Ref");
    apply(session,{SetVisibility{"group",false}});evaluated=scene(session.document());
    check(!evaluated.roots[0].visible&&evaluated.shapes.contains("a"),"Hidden tree retains editable evaluated leaf geometry");
    check(object_visibility_property(session.document(),{"a","","object.visible"})&&
        !object_visibility_property(session.document(),{"group","","object.visible"}),
        "Typed visibility reports each authored toggle, not effective ancestor visibility");
}

void group_posterize_native_api_and_refusals() {
    auto d=empty_document("posterize-doc","comp","art");
    Object child;child.id="child";child.name="Child";
    Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"child"};
    d.objects.emplace("child",std::move(child));d.objects.emplace("group",std::move(group));d.compositions[0].roots={"group"};
    Session session(d);
    const auto add_response=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"add_operation","object":"group","index":0,"operation":{"id":"posterize","type":"nect.group.posterize","version":1,"enabled":true,"parameters":{"levels":{"literal":2}},"composite":"below","fill_rule":"nonzero"}}]})");
    check(add_response.find("\"changed\":true")!=std::string::npos&&session.revision()==1,
        "JSON-lines API adds Group Posterize through the shared Session revision: "+add_response);
    const Ref levels{"group","","op.posterize.levels"};
    check(property(session.document(),levels).literal==2&&property_unit(levels)=="scalar","Group levels is an ordinary stable Scalar Ref");
    const auto set_response=request(session,R"({"op":"apply","expected_revision":1,"commands":[{"type":"set","ref":{"object":"group","point":"","field":"op.posterize.levels"},"value":4}]})");
    check(set_response.find("\"changed\":true")!=std::string::npos&&evaluate(session.document()).at(levels)==4&&session.revision()==2,
        "API Set changes evaluated Group levels in one revision");
    auto evaluated=scene(session.document());
    check(evaluated.requires_compositing&&evaluated.roots[0].isolated&&evaluated.roots[0].posterize_levels==std::vector<unsigned>{4},
        "Scene marks enabled Group Posterize as ordered postchildren isolation");
    auto second=default_operation("posterize-second","nect.group.posterize");second.parameters.at("levels").literal=3;
    session.apply({AddOperation{"group",second,1}},session.revision());
    check(scene(session.document()).roots[0].posterize_levels==std::vector<unsigned>({4,3}),
        "Multiple Group Posterize instances evaluate in authored stack order");
    session.apply({ReorderOperations{"group",{"posterize-second","posterize"}}},session.revision());
    check(scene(session.document()).roots[0].posterize_levels==std::vector<unsigned>({3,4}),
        "ReorderOperations changes the ordered postchildren evaluation");
    const auto native=encode(session.document());
    check(native.find("\"version\":\"0.29\"")!=std::string::npos&&decode(native)==session.document()&&
        decode(native).objects.at("group").stack[0].id=="posterize-second",
        "Native 0.29 preserves Group operation IDs, levels and reordered stack");
    check(request(session,R"({"op":"operator_types"})").find("nect.group.posterize")!=std::string::npos,
        "API operator discovery advertises the Group pixel effect");
    check(request(session,R"({"op":"operator_types"})").find("postchildren_premultiplied_srgb_rgba")!=std::string::npos,
        "API operator discovery names the Group pixel input domain");
    const auto composite_plan=request(session,R"({"op":"compositing_plan","composition":"comp"})");
    check(composite_plan.find("postchildren_effects")!=std::string::npos&&composite_plan.find("posterize-second")!=std::string::npos,
        "API compositing plan exposes ordered Group effects and stable IDs");
    const auto plan=request(session,R"({"op":"export_plan","composition":"comp","artboard":"art"})");
    check(plan.find("\"svg_export_supported\":false")!=std::string::npos&&plan.find("enabled Group Posterize")!=std::string::npos,
        "SVG export plan discloses the unsupported enabled Group derivative");
    const Ref second_enabled=operation_ref("group","posterize-second","enabled");
    const Ref source_enabled=operation_ref("group","posterize","enabled");
    apply(session,{LinkOperationEnabled{second_enabled,source_enabled,false},
        EnableOperation{"group","posterize",false}});
    const auto bypassed_scene=scene(session.document());
    const auto bypassed_plan=request(session,R"({"op":"compositing_plan","composition":"comp"})");
    check(!bypassed_scene.requires_compositing&&bypassed_scene.roots[0].posterize_levels.empty()&&
        bypassed_plan.find("\"enabled\":false")!=std::string::npos&&
        export_svg(session.document(),"comp","art").find("<svg")!=std::string::npos,
        "A linked disabled Group operation is bypassed in scene, compositing plan, and SVG support checks");
    apply(session,{EnableOperation{"group","posterize",true}});
    check(scene(session.document()).roots[0].posterize_levels==std::vector<unsigned>({3,4})&&
        request(session,R"({"op":"compositing_plan","composition":"comp"})").find("\"enabled\":true")!=std::string::npos,
        "Re-enabling the source restores the linked Group effect in evaluation and plan rows");
    rejects("UNSUPPORTED_SVG_EFFECT",[&]{(void)export_svg(session.document(),"comp","art");});
    apply(session,{UnlinkOperationEnabled{second_enabled},EnableOperation{"group","posterize",false}});
    check(scene(session.document()).roots[0].posterize_levels==std::vector<unsigned>({3}),
        "Unlink freezes evaluated Group enabled state independently from the former source");
    apply(session,{EnableOperation{"group","posterize",true}});
    rejects("UNSUPPORTED_SVG_EFFECT",[&]{(void)export_svg(session.document(),"comp","art");});

    auto legacy_bytes=encode(d);
    const auto old_version=legacy_bytes.find("\"version\":\"0.29\"");
    check(old_version!=std::string::npos,"Native fixture writer uses 0.29 before migration downgrade");
    legacy_bytes.replace(old_version,std::string("\"version\":\"0.29\"").size(),"\"version\":\"0.24\"");
    const auto group_id=legacy_bytes.find("\"id\":\"group\"");
    check(group_id!=std::string::npos,"Native fixture contains the target Group object");
    const auto object_start=legacy_bytes.rfind('{',group_id);
    check(object_start!=std::string::npos,"Native fixture Group object has an opening brace");
    std::size_t object_end=std::string::npos;
    bool in_string=false,escaped=false;
    int depth=0;
    for(std::size_t i=object_start;i<legacy_bytes.size();++i) {
        const char c=legacy_bytes[i];
        if(in_string) {
            if(escaped)escaped=false;
            else if(c=='\\')escaped=true;
            else if(c=='\"')in_string=false;
        } else if(c=='\"')in_string=true;
        else if(c=='{')++depth;
        else if(c=='}'&&--depth==0) {object_end=i;break;}
    }
    check(object_end!=std::string::npos,"Native fixture Group object has a closing brace");
    const auto group_stack=legacy_bytes.find("\"stack\":[]",group_id);
    check(group_stack!=std::string::npos&&group_stack<object_end,"Native fixture Group stack is empty");
    const auto stack_end=group_stack+std::string("\"stack\":[]").size();
    std::size_t erase_start=group_stack,erase_end=stack_end;
    if(erase_end<object_end&&legacy_bytes[erase_end]==',')++erase_end;
    else if(erase_start>object_start&&legacy_bytes[erase_start-1]==',')--erase_start;
    else throw std::runtime_error("Could not remove Group stack member from legacy fixture");
    legacy_bytes.erase(erase_start,erase_end-erase_start);
    check(decode(legacy_bytes).objects.at("group").stack.empty(),"Native 0.24 Group migrates to an empty effect stack");
    auto smuggled=native;const auto old_writer=smuggled.find("\"version\":\"0.29\"");
    smuggled.replace(old_writer,std::string("\"version\":\"0.29\"").size(),"\"version\":\"0.24\"");
    rejects("INVALID_OBJECT",[&]{(void)decode(smuggled);});

    session.apply({EnableOperation{"group","posterize",false},EnableOperation{"group","posterize-second",false}},session.revision());
    check(!scene(session.document()).roots[0].isolated&&export_svg(session.document(),"comp","art").find("<svg")!=std::string::npos,
        "Bypassed Group Posterize passes through and permits ordinary SVG projection");
    const auto before=session.document();const auto revision=session.revision();const auto history=session.history();
    rejects("REVISION_CONFLICT",[&]{session.apply({Set{levels,3}},revision-1);});
    check(session.document()==before&&session.revision()==revision&&session.history()==history,"Stale Group level edit is atomic");
    atomic(session,"OUT_OF_RANGE",{Set{levels,1}});atomic(session,"OUT_OF_RANGE",{Set{levels,2.5}});atomic(session,"OUT_OF_RANGE",{Set{levels,17}});
    atomic(session,"INVALID_DOMAIN",{AddOperation{"group",default_operation("group-fill","nect.paint.fill"),1}});
    atomic(session,"INVALID_DOMAIN",{AddOperation{"child",default_operation("child-posterize","nect.group.posterize"),0}});
    atomic(session,"DUPLICATE_ID",{AddOperation{"group",default_operation("group","nect.group.posterize"),1}});
    auto unknown=d;auto unknown_operation=default_operation("unknown","nect.group.posterize");unknown_operation.type="nect.group.unknown";unknown.objects.at("group").stack.push_back(unknown_operation);
    rejects("INVALID_DOMAIN",[&]{validate(unknown);});
    auto unsupported_version=d;auto bad_version=default_operation("unsupported","nect.group.posterize");bad_version.version=2;unsupported_version.objects.at("group").stack.push_back(bad_version);
    rejects("UNSUPPORTED_OPERATOR_VERSION",[&]{validate(unsupported_version);});
}

void mask_geometry_and_validation() {
    auto document=fixture();auto& source=document.objects.at("source");source.visible=false;source.compositing.opacity.literal=0;
    source.stack[0].parameters.at("a").literal=0;source.contours.front().closed=false;
    auto repeat=default_operation("source-repeat","nect.shape.repeater");repeat.parameters.at("copies").literal=2;repeat.parameters.at("position_x").literal=50;source.stack.push_back(repeat);
    Object parent;parent.id="parent";parent.name="Parent";parent.kind=Kind::group;matrix(parent,{0,2,-3,0,200,30});
    document.objects.emplace(parent.id,parent);document.compositions[0].roots.push_back(parent.id);source.transform_parent=parent.id;matrix(source,{1,0,0,1,10,5});
    document.objects.at("a").compositing.mask=GeometryMask{"mask","source",1,true,"evenodd"};
    Session session(document);const auto evaluated=scene(document);const auto& mask=*evaluated.roots[0].mask;
    check(mask.source=="source"&&mask.fill_rule=="evenodd"&&mask.paths.size()==2,"Mask uses repeated geometry despite hidden/transparent source appearance");
    check(mask.paths.front().contours==evaluated.shapes.at("source").paths.front().contours,"Source artwork and mask share one evaluated contour allocation");
    auto point=map_point(mask.paths.front().transform,mask.paths.front().contours->front().points.front().anchor);
    near(point.x,185,"Mask source world X respects external parenting");near(point.y,50,"Mask source world Y respects external parenting");
    point=map_point(mask.paths.back().transform,{0,0});near(point.x,185,"Repeat local X maps through rotated world basis");near(point.y,150,"Repeated mask uses final source geometry");
    check(!mask.paths.front().contours->front().closed,"Implicit mask closure never rewrites authored open contours");
    atomic(session,"MISSING_MASK_SOURCE",{DeleteObjects{{"source"}}});
    atomic(session,"INVALID_MASK_SOURCE",{SetMask{"a",GeometryMask{"other-mask","a"}}});
    atomic(session,"INVALID_MASK_SOURCE",{SetMask{"a",GeometryMask{"other-mask","parent"}}});
    atomic(session,"DUPLICATE_ID",{SetMask{"b",GeometryMask{"mask","source"}}});
    atomic(session,"UNSUPPORTED_MASK_VERSION",{SetMask{"a",GeometryMask{"mask","source",2}}});
    atomic(session,"UNSUPPORTED_FILL_RULE",{SetMask{"a",GeometryMask{"mask","source",1,true,"inverse"}}});
    auto disabled=*session.document().objects.at("a").compositing.mask;disabled.enabled=false;apply(session,{SetMask{"a",disabled}});
    check(!scene(session.document()).roots[0].mask,"Bypass removes only the evaluated mask");
    atomic(session,"MISSING_MASK_SOURCE",{DeleteObjects{{"source"}}});
    apply(session,{DeleteObjects{{"a","source"}}});check(!session.document().objects.contains("source"),"Deleting source together with its owner is valid");
    auto foreign=fixture();foreign.compositions.push_back({"other","Other",{"source"},{{"other-art","Page"}}});foreign.compositions[0].roots.pop_back();Session planes(foreign);
    atomic(planes,"CROSS_COMPOSITION",{SetMask{"a",GeometryMask{"mask","source"}}});
}

void mask_with_and_put_inside() {
    for(const bool top:{false,true}) {
        auto document=fixture();document.collections={{"collection","Selection",{"a","b"}}};Session session(document);
        apply(session,{MaskObjects{"comp","",{"a","b"},"group","mask","Masked",top}});
        const auto& group=session.document().objects.at("group");const auto source=top?"b":"a";
        check(group.children==std::vector<Id>{"a","b"}&&group.compositing.mask->source==source&&!session.document().objects.at(source).visible,"Mask With preserves order and hides the selected source");
        check(session.document().collections==document.collections,"Mask grouping preserves Collection membership");
        session.undo(session.revision());check(session.document()==document,"Mask With is one complete Undo including visibility");
    }
    auto driven_source=fixture();driven_source.objects.at("b").visibility_driver=Ref{"a","","object.visible"};
    Session refused_mask(driven_source);
    atomic(refused_mask,"DRIVEN_PROPERTY",{MaskObjects{"comp","",{"a","b"},"group","mask","Masked",true}});
    auto document=fixture();Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"source"};matrix(group,{0,2,-3,0,200,30});
    document.objects.emplace(group.id,group);document.compositions[0].roots={"a","b","group"};matrix(document.objects.at("a"),{1,0,0,1,100,120});
    document.objects.at("a").compositing.opacity.literal=.5;document.objects.at("a").compositing.blend="multiply";
    document.objects.at("b").transform_parent="a";matrix(document.objects.at("b"),{1,0,0,1,15,20});
    Session session(document);const auto before=transforms(document);const auto source_appearance=document.objects.at("a").compositing;
    apply(session,{PutInside{"comp","","group",{"a","b"}}});const auto after=transforms(session.document());
    check(session.document().objects.at("group").children==std::vector<Id>{"a","b","source"},"Put Inside inserts moved roots before existing children");
    for(const auto* id:{"a","b","source"})same_matrix(before.at(id).world,after.at(id).world);
    check(session.document().objects.at("a").compositing==source_appearance,"Neutral destination allows and retains source sibling compositing");
    check(session.document().objects.at("b").transform==document.objects.at("b").transform&&session.document().objects.at("b").transform_parent==Id{"a"},"Explicit Transform Parent and local Scalars stay unchanged");
    const auto moved=session.document();session.undo(session.revision());check(session.document()==document,"Put Inside Undo restores Structure and canonical affine values");session.redo(session.revision());check(session.document()==moved,"Put Inside Redo restores exact state");
    Session invalid(document);atomic(invalid,"NONCONTIGUOUS_GROUP",{PutInside{"comp","","group",{"a"}}});
    auto hidden_invalid=document;hidden_invalid.objects.at("group").visible=false;Session hidden_selection(hidden_invalid);
    atomic(hidden_selection,"NONCONTIGUOUS_GROUP",{PutInside{"comp","","group",{"a"}}});
    auto driven=document;driven.objects.at("a").transform[0].expression=Expression{"1"};Session linked(driven);atomic(linked,"DRIVEN_PROPERTY",{PutInside{"comp","","group",{"a","b"}}});
    auto singular=document;matrix(singular.objects.at("group"),{0,0,0,1,0,0});Session collapsed(singular);atomic(collapsed,"SINGULAR_TRANSFORM",{PutInside{"comp","","group",{"a","b"}}});
    auto coupled=document;matrix(coupled.objects.at("group"),identity_matrix);coupled.objects.at("group").transform[4].binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};Session dependent(coupled);
    atomic(dependent,"TRANSFORM_PRESERVATION",{PutInside{"comp","","group",{"a","b"}}});
    auto property_coupled=document;
    property_coupled.objects.at("source").contours[0].points[0].x.binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};
    Session property_dependent(property_coupled);
    atomic(property_dependent,"PUT_INSIDE_DEPENDENCY",{PutInside{"comp","","group",{"a","b"}}});

    auto appearance_refusal=[&](auto change,const char* reason) {
        auto unsafe=document;change(unsafe.objects.at("group"));Session blocked(unsafe);
        atomic_reason(blocked,"PUT_INSIDE_APPEARANCE",reason,{PutInside{"comp","","group",{"a","b"}}});
    };
    appearance_refusal([](Object& g){g.visible=false;},"destination Group is hidden");
    appearance_refusal([](Object& g){g.compositing.opacity.literal=.5;},"non-neutral opacity");
    appearance_refusal([](Object& g){g.compositing.opacity.expression=Expression{"1"};},"opacity is driven");
    appearance_refusal([](Object& g){g.compositing.opacity.binding=Binding{{"source","","composite.opacity"},1,0,"copy_local_value"};},"opacity is driven");
    appearance_refusal([](Object& g){g.compositing.blend="multiply";},"non-normal blend mode");
    appearance_refusal([](Object& g){g.compositing.isolated=true;},"destination Group is isolated");
    appearance_refusal([](Object& g){g.compositing.mask=GeometryMask{"group-mask","source"};},"destination Group has a mask");
    auto linked_hidden=document;linked_hidden.objects.at("source").visible=false;
    linked_hidden.objects.at("group").visibility_driver=Ref{"source","","object.visible"};
    Session linked_hidden_destination(linked_hidden);
    atomic_reason(linked_hidden_destination,"PUT_INSIDE_APPEARANCE","destination Group is hidden",
        {PutInside{"comp","","group",{"a","b"}}});

    auto api_document=document;api_document.objects.at("group").visible=false;Session api(api_document);
    const auto api_before=api.document();const auto api_history=api.history();
    const auto response=request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"put_inside","composition":"comp","parent":"","group":"group","members":["a","b"]}]})");
    check(response.find("\"code\":\"PUT_INSIDE_APPEARANCE\"")!=std::string::npos&&
        response.find("Put Inside destination Group is hidden")!=std::string::npos,
        "JSON-lines Put Inside reports the same scoped appearance refusal and reason");
    check(api.document()==api_before&&api.revision()==0&&api.history()==api_history,
        "JSON-lines appearance refusal retains document, revision and history");
}
void neutral_ungroup() {
    auto document=fixture();Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"a","b"};matrix(group,{0,2,-3,0,200,30});
    document.objects.emplace(group.id,group);document.compositions[0].roots={"group","source"};document.collections={{"collection","Members",{"group","a"}}};
    document.objects.at("b").transform_parent="source";matrix(document.objects.at("b"),{1,0,0,1,15,20});
    Session session(document);const auto before=transforms(document);apply(session,{Ungroup{"comp","","group"}});const auto after=transforms(session.document());
    check(session.document().compositions[0].roots==std::vector<Id>{"a","b","source"}&&!session.document().objects.contains("group"),"Ungroup replaces exact stacking slot with children");
    for(const auto* id:{"a","b","source"}){same_matrix(before.at(id).world,after.at(id).world);check(session.document().objects.at(id).contours==document.objects.at(id).contours,"Ungroup preserves stable authored geometry");}
    check(session.document().objects.at("b")==document.objects.at("b"),"Explicit external child parent and local state remain intact");
    check(session.document().collections[0].members==std::vector<Id>{"a"},"Only removed Group collection membership pruned");
    const auto ungrouped=session.document();session.undo(session.revision());check(session.document()==document,"Ungroup exact Undo");session.redo(session.revision());check(session.document()==ungrouped,"Ungroup exact Redo");
    for(int case_id=0;case_id<5;++case_id){auto d=document;auto& g=d.objects.at("group");if(case_id==0)g.visible=false;if(case_id==1)g.compositing.opacity.literal=.5;if(case_id==2)g.compositing.blend="multiply";if(case_id==3)g.compositing.isolated=true;if(case_id==4)g.compositing.mask=GeometryMask{"mask","source"};Session blocked(d);atomic(blocked,"UNGROUP_APPEARANCE",{Ungroup{"comp","","group"}});}
    auto linked_hidden=document;linked_hidden.objects.at("source").visible=false;
    linked_hidden.objects.at("group").visibility_driver=Ref{"source","","object.visible"};
    Session linked_hidden_group(linked_hidden);atomic(linked_hidden_group,"UNGROUP_APPEARANCE",{Ungroup{"comp","","group"}});
    auto dynamic=document;dynamic.objects.at("group").transform[4].expression=Expression{"200"};Session driven_group(dynamic);atomic(driven_group,"UNGROUP_DYNAMIC",{Ungroup{"comp","","group"}});
    dynamic=document;dynamic.objects.at("group").transform_parent="source";Session followed(dynamic);atomic(followed,"UNGROUP_DYNAMIC",{Ungroup{"comp","","group"}});
    auto linked=document;linked.objects.at("source").contours[0].points[0].x.binding=Binding{{"group","","transform.tx"},1,0};Session referenced(linked);atomic(referenced,"MISSING_REFERENCE",{Ungroup{"comp","","group"}});
    linked=document;linked.objects.at("b").transform_parent="group";Session parented(linked);atomic(parented,"MISSING_TRANSFORM_PARENT",{Ungroup{"comp","","group"}});
    linked=document;linked.objects.at("a").transform[0].expression=Expression{"1"};Session driven_child(linked);atomic(driven_child,"DRIVEN_PROPERTY",{Ungroup{"comp","","group"}});
    linked=document;linked.objects.at("a").contours[0].points[0].x.binding=Binding{{"a","","transform.tx"},1,0};Session geometry_dependency(linked);atomic(geometry_dependency,"UNGROUP_DEPENDENCY",{Ungroup{"comp","","group"}});
    auto singular=document;matrix(singular.objects.at("group"),{0,0,0,1,20,30});Session collapsed(singular);const auto old=transforms(singular);apply(collapsed,{Ungroup{"comp","","group"}});same_matrix(old.at("a").world,transforms(collapsed.document()).at("a").world);
    auto nested=document;Object outer;outer.id="outer";outer.name="Outer";outer.kind=Kind::group;outer.children={"group"};matrix(outer,{2,0,0,2,50,60});nested.objects.emplace("outer",outer);nested.compositions[0].roots={"outer","source"};Session nesting(nested);const auto prior=transforms(nested);apply(nesting,{Ungroup{"comp","outer","group"}});same_matrix(prior.at("a").world,transforms(nesting.document()).at("a").world);check(nesting.document().objects.at("outer").children==std::vector<Id>{"a","b"},"Nested Group unwrap preserves outer container");
}
Document move_out_fixture() {
    auto document=fixture();document.objects.emplace("c",rectangle("c",90,15));document.objects.emplace("y",rectangle("y",220,30));
    Object group;group.id="folder";group.name="Folder";group.kind=Kind::group;group.children={"a","b","c"};matrix(group,{0,2,-3,0,200,30});
    document.objects.emplace(group.id,group);document.compositions[0].roots={"source","folder","y"};
    matrix(document.objects.at("a"),{1,.5,1.2,2,10,30});document.objects.at("b").transform_parent="source";
    document.objects.at("c").transform_parent="folder";
    document.collections={{"collection","Stable members",{"folder","a","c"}}};
    document.objects.at("source").contours[0].points[0].x.binding=Binding{{"folder","","transform.tx"},1,0,"copy_local_value"};
    return document;
}
void move_out_preservation(const Document& before,const Session& session,const std::vector<Id>& roots,const std::vector<Id>& children,const std::vector<Id>& moved) {
    const auto& after=session.document();
    check(after.compositions.front().roots==roots,"Move Out inserts moved roots beside the retained Folder in paint order");
    check(after.objects.at("folder").children==children,"Move Out retains Folder with the remaining children");
    check(after.collections==before.collections,"Move Out preserves Collection membership");
    const std::array<std::string,6> affine_fields{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty"};
    const auto before_values=evaluate(before),after_values=evaluate(after);const auto before_transforms=transforms(before),after_transforms=transforms(after);
    for(const auto& [id,object]:before.objects) {
        same_matrix(before_transforms.at(id).world,after_transforms.at(id).world);
        if(std::find(moved.begin(),moved.end(),id)==moved.end()) {
            auto retained=after.objects.at(id);if(id=="folder")retained.children=object.children;
            check(retained==object,"Unmoved authored object remains exact apart from Folder child ownership");
        }
        else {
            auto moved_object=after.objects.at(id);const auto& original=object;
            for(std::size_t i=0;i<6;++i)moved_object.transform[i]=original.transform[i];
            check(moved_object==original,"Move Out changes only the moved object's local affine values");
        }
    }
    for(const auto& [ref,value]:before_values) {
        const bool moved_affine=std::find(moved.begin(),moved.end(),ref.object)!=moved.end()&&ref.point.empty()&&ref.field.starts_with("transform.")&&
            std::find(std::begin(affine_fields),std::end(affine_fields),ref.field)!=std::end(affine_fields);
        if(!moved_affine)check(after_values.at(ref)==value,"Move Out preserves every non-adjusted evaluated property value");
    }
    check(drawable_order(before,scene(before))==drawable_order(after,scene(after)),"Move Out preserves drawable paint order");
    check(after.objects.at("source").contours[0].points[0].x.binding==before.objects.at("source").contours[0].points[0].x.binding,
        "Move Out preserves a stable property reference to the retained Folder");
    check(decode(encode(after))==after,"Move Out result survives native 0.20 encode/decode");
}
void move_out_folder() {
    const auto document=move_out_fixture();
    Session prefix(document);const auto response=request(prefix,R"({"op":"apply","expected_revision":0,"commands":[{"type":"move_out","composition":"comp","parent":"","group":"folder","members":["a"],"placement":"before"}]})");
    check(response.find("\"changed\":true")!=std::string::npos,"JSON-lines Move Out reaches the shared Session command");
    move_out_preservation(document,prefix,{"source","a","folder","y"},{"b","c"},{"a"});
    const auto prefix_result=prefix.document();prefix.undo(prefix.revision());check(prefix.document()==document,"Move Out prefix uses one exact Undo boundary");
    prefix.redo(prefix.revision());check(prefix.document()==prefix_result,"Move Out prefix Redo restores the exact extraction");

    Session suffix(document);apply(suffix,{MoveOut{"comp","","folder",{"c"},"after"}});
    move_out_preservation(document,suffix,{"source","folder","c","y"},{"a","b"},{"c"});
    const auto suffix_result=suffix.document();suffix.undo(suffix.revision());check(suffix.document()==document,"Move Out suffix uses one exact Undo boundary");
    suffix.redo(suffix.revision());check(suffix.document()==suffix_result,"Move Out suffix Redo restores the exact extraction");

    Session whole(document);apply(whole,{MoveOut{"comp","","folder",{"a","b","c"},"after"}});
    move_out_preservation(document,whole,{"source","folder","a","b","c","y"},{},{"a","b","c"});

    auto reject_move=[&](const Document& candidate,const char* code,const MoveOut& command) {Session invalid(candidate);atomic(invalid,code,{command});};
    reject_move(document,"MOVE_OUT_SELECTION",MoveOut{"comp","","folder",{"b"},"before"});
    reject_move(document,"MOVE_OUT_SELECTION",MoveOut{"comp","","folder",{"a","c"},"before"});
    reject_move(document,"MOVE_OUT_PLACEMENT",MoveOut{"comp","","folder",{"a"},"inside"});
    reject_move(document,"INVALID_PARENT",MoveOut{"comp","source","folder",{"a"},"before"});
    reject_move(document,"MISSING_COMPOSITION",MoveOut{"missing","","folder",{"a"},"before"});
    auto hidden=document;hidden.objects.at("folder").visible=false;
    reject_move(hidden,"UNGROUP_APPEARANCE",MoveOut{"comp","","folder",{"a"},"before"});
    auto linked_hidden=document;linked_hidden.objects.at("source").visible=false;
    linked_hidden.objects.at("folder").visibility_driver=Ref{"source","","object.visible"};
    reject_move(linked_hidden,"UNGROUP_APPEARANCE",MoveOut{"comp","","folder",{"a"},"before"});
    for(int case_id=0;case_id<5;++case_id) {
        auto unsafe=document;auto& folder=unsafe.objects.at("folder");
        if(case_id==0)folder.compositing.opacity.literal=.5;
        if(case_id==1)folder.compositing.opacity.expression=Expression{"1"};
        if(case_id==2)folder.compositing.blend="multiply";
        if(case_id==3)folder.compositing.isolated=true;
        if(case_id==4)folder.compositing.mask=GeometryMask{"folder-mask","source"};
        reject_move(unsafe,"UNGROUP_APPEARANCE",MoveOut{"comp","","folder",{"a"},"before"});
    }
    auto dynamic=document;dynamic.objects.at("folder").transform[4].expression=Expression{"200"};
    reject_move(dynamic,"UNGROUP_DYNAMIC",MoveOut{"comp","","folder",{"a"},"before"});
    dynamic=document;dynamic.objects.at("folder").transform_parent="source";
    reject_move(dynamic,"UNGROUP_DYNAMIC",MoveOut{"comp","","folder",{"a"},"before"});
    auto driven=document;driven.objects.at("a").transform[0].expression=Expression{"1"};
    reject_move(driven,"DRIVEN_PROPERTY",MoveOut{"comp","","folder",{"a"},"before"});
    auto dependent=document;dependent.objects.at("source").contours[0].points[0].x.binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};
    reject_move(dependent,"UNGROUP_DEPENDENCY",MoveOut{"comp","","folder",{"a"},"before"});

    Session stale(document);const auto stale_doc=stale.document();const auto stale_history=stale.history();const auto stale_revision=stale.revision();
    rejects("REVISION_CONFLICT",[&]{stale.apply({MoveOut{"comp","","folder",{"a"},"before"}},stale_revision+1);});
    check(stale.document()==stale_doc&&stale.revision()==stale_revision&&stale.history()==stale_history,"Stale Move Out retains document, revision and history");
}
void adjacent_folder_transfer() {
    auto document=move_out_fixture();
    document.objects.emplace("d",rectangle("d",280,25));
    Object next;next.id="next-folder";next.name="Next";next.kind=Kind::group;next.children={"d"};
    document.objects.emplace(next.id,next);
    auto& roots=document.compositions[0].roots;
    roots.insert(std::find(roots.begin(),roots.end(),"y"),"next-folder");
    Session session(document);
    const auto before_world=transforms(document);
    const auto before_order=drawable_order(document,scene(document));
    const auto response=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"move_out","composition":"comp","parent":"","group":"folder","members":["b","c"],"placement":"after"},{"type":"put_inside","composition":"comp","parent":"","group":"next-folder","members":["b","c"]}]})");
    check(response.find("\"changed\":true")!=std::string::npos,"JSON-lines composes both existing structural commands in one Session edit");
    const auto& after=session.document();
    check(after.compositions[0].roots==document.compositions[0].roots&&
        after.objects.at("folder").children==std::vector<Id>{"a"}&&
        after.objects.at("next-folder").children==std::vector<Id>{"b","c","d"},"Adjacent Folder transfer preserves parent slots and child order");
    const auto after_world=transforms(after);
    for(const auto& [id,old]:before_world)same_matrix(old.world,after_world.at(id).world);
    check(drawable_order(after,scene(after))==before_order,"Adjacent Folder transfer preserves drawable order");
    check(after.collections==document.collections&&after.objects.at("source").contours==document.objects.at("source").contours,
        "Adjacent Folder transfer retains Collection membership and external source refs");
    check(decode(encode(after))==after,"Adjacent Folder transfer survives native roundtrip");
    const auto moved=after;session.undo(session.revision());check(session.document()==document,"Adjacent Folder transfer has one exact Undo");
    session.redo(session.revision());check(session.document()==moved,"Adjacent Folder transfer has one exact Redo");

    auto unsafe=document;unsafe.objects.at("next-folder").visible=false;Session rejected(unsafe);
    atomic(rejected,"PUT_INSIDE_APPEARANCE",{MoveOut{"comp","","folder",{"c"},"after"},PutInside{"comp","","next-folder",{"c"}}});
    auto driven=document;driven.objects.at("a").transform[4].expression=Expression{"1"};Session blocked(driven);
    atomic(blocked,"DRIVEN_PROPERTY",{MoveOut{"comp","","folder",{"a","b","c"},"after"},PutInside{"comp","","next-folder",{"a","b","c"}}});
}
void reverse_adjacent_folder_transfer() {
    auto document=move_out_fixture();
    document.objects.emplace("d",rectangle("d",280,25));
    Object next;next.id="next-folder";next.name="Next";next.kind=Kind::group;next.children={"d"};
    document.objects.emplace(next.id,next);
    auto& roots=document.compositions[0].roots;
    roots.insert(std::find(roots.begin(),roots.end(),"y"),"next-folder");
    Session session(document);const auto before_world=transforms(document);
    const auto before_order=drawable_order(document,scene(document));
    const std::vector<Command> commands{
        MoveOut{"comp","","next-folder",{"d"},"before"},
        ReorderObjects{"comp","",{"source","d","folder","next-folder","y"}},
        PutInside{"comp","","folder",{"d"}},
        ReorderObjects{"comp","folder",{"a","b","c","d"}}};
    const auto response=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"move_out","composition":"comp","parent":"","group":"next-folder","members":["d"],"placement":"before"},{"type":"reorder_objects","composition":"comp","parent":"","order":["source","d","folder","next-folder","y"]},{"type":"put_inside","composition":"comp","parent":"","group":"folder","members":["d"]},{"type":"reorder_objects","composition":"comp","parent":"folder","order":["a","b","c","d"]}]})");
    check(response.find("\"changed\":true")!=std::string::npos,"JSON-lines composes the reverse transfer into one Session edit");
    const auto& after=session.document();
    check(after.compositions[0].roots==roots&&after.objects.at("folder").children==std::vector<Id>{"a","b","c","d"}&&
        after.objects.at("next-folder").children.empty(),"Reverse adjacent transfer appends a prefix to the previous Folder");
    const auto after_world=transforms(after);
    for(const auto& [id,old]:before_world)same_matrix(old.world,after_world.at(id).world);
    check(drawable_order(after,scene(after))==before_order,"Reverse adjacent transfer preserves drawable paint order");
    check(after.collections==document.collections&&decode(encode(after))==after,"Reverse transfer keeps Collections and native identity");
    const auto moved=after;session.undo(session.revision());check(session.document()==document,"Reverse transfer has one exact Undo");
    session.redo(session.revision());check(session.document()==moved,"Reverse transfer has one exact Redo");
    auto unsafe=document;unsafe.objects.at("folder").visible=false;Session rejected(unsafe);
    atomic(rejected,"PUT_INSIDE_APPEARANCE",commands);
}
void explicit_nonadjacent_folder_transfer() {
    auto document=move_out_fixture();
    Object gap;gap.id="empty-gap";gap.name="Empty gap";gap.kind=Kind::group;
    document.objects.emplace(gap.id,gap);
    document.objects.emplace("d",rectangle("d",280,25));
    Object next;next.id="next-folder";next.name="Next";next.kind=Kind::group;next.children={"d"};
    document.objects.emplace(next.id,next);
    document.compositions[0].roots={"source","folder","empty-gap","next-folder","y"};
    const auto original_order=drawable_order(document,scene(document));
    const auto original_world=transforms(document);
    const auto original_values=evaluate(document);
    Session forward(document);
    const auto response=request(forward,R"({"op":"apply","expected_revision":0,"commands":[{"type":"move_out","composition":"comp","parent":"","group":"folder","members":["b","c"],"placement":"after"},{"type":"reorder_objects","composition":"comp","parent":"","order":["source","folder","empty-gap","b","c","next-folder","y"]},{"type":"put_inside","composition":"comp","parent":"","group":"next-folder","members":["b","c"]}]})");
    check(response.find("\"changed\":true")!=std::string::npos,"JSON-lines composes an explicit nonadjacent Folder move in one edit");
    const auto forward_document=forward.document();
    check(forward_document.compositions[0].roots==document.compositions[0].roots&&
        forward_document.objects.at("folder").children==std::vector<Id>{"a"}&&
        forward_document.objects.at("next-folder").children==std::vector<Id>{"b","c","d"},
        "Nonadjacent transfer crosses only an empty Folder and preserves hierarchy order");
    check(drawable_order(forward_document,scene(forward_document))==original_order,
        "Nonadjacent transfer preserves flattened paint order");
    for(const auto& [id,old]:original_world)same_matrix(old.world,transforms(forward_document).at(id).world);
    for(const auto& [ref,value]:original_values)if(ref.object!="b"&&ref.object!="c")
        check(evaluate(forward_document).at(ref)==value,"Nonadjacent transfer preserves untouched evaluated values");
    check(forward_document.collections==document.collections&&
        forward_document.objects.at("source").contours[0].points[0].x.binding==document.objects.at("source").contours[0].points[0].x.binding&&
        decode(encode(forward_document))==forward_document,"Nonadjacent transfer preserves references, Collections and native state");
    forward.undo(forward.revision());check(forward.document()==document,"Nonadjacent forward transfer has one exact Undo");
    forward.redo(forward.revision());check(forward.document()==forward_document,"Nonadjacent forward transfer has one exact Redo");

    Session reverse(document);
    const auto reverse_response=request(reverse,R"({"op":"apply","expected_revision":0,"commands":[{"type":"move_out","composition":"comp","parent":"","group":"next-folder","members":["d"],"placement":"before"},{"type":"reorder_objects","composition":"comp","parent":"","order":["source","d","folder","empty-gap","next-folder","y"]},{"type":"put_inside","composition":"comp","parent":"","group":"folder","members":["d"]},{"type":"reorder_objects","composition":"comp","parent":"folder","order":["a","b","c","d"]}]})");
    check(reverse_response.find("\"changed\":true")!=std::string::npos,"JSON-lines composes explicit reverse nonadjacent movement atomically");
    const auto reverse_document=reverse.document();
    check(reverse_document.compositions[0].roots==document.compositions[0].roots&&
        reverse_document.objects.at("folder").children==std::vector<Id>{"a","b","c","d"}&&
        reverse_document.objects.at("next-folder").children.empty()&&
        drawable_order(reverse_document,scene(reverse_document))==original_order,
        "Reverse transfer across an empty Folder preserves hierarchy and paint order");
    for(const auto& [id,old]:original_world)same_matrix(old.world,transforms(reverse_document).at(id).world);
    check(reverse_document.collections==document.collections&&decode(encode(reverse_document))==reverse_document,
        "Reverse nonadjacent transfer preserves Collections and native state");
    reverse.undo(reverse.revision());check(reverse.document()==document,"Reverse nonadjacent transfer has one exact Undo");
    reverse.redo(reverse.revision());check(reverse.document()==reverse_document,"Reverse nonadjacent transfer has one exact Redo");

    auto unsafe=document;unsafe.objects.at("next-folder").visible=false;Session rejected(unsafe);
    atomic(rejected,"PUT_INSIDE_APPEARANCE",{MoveOut{"comp","","folder",{"c"},"after"},
        ReorderObjects{"comp","",{"source","folder","empty-gap","c","next-folder","y"}},
        PutInside{"comp","","next-folder",{"c"}}});
    Session stale(document);const auto stale_document=stale.document();const auto stale_history=stale.history();
    rejects("REVISION_CONFLICT",[&]{stale.apply({MoveOut{"comp","","folder",{"c"},"after"},
        ReorderObjects{"comp","",{"source","folder","empty-gap","c","next-folder","y"}},
        PutInside{"comp","","next-folder",{"c"}}},stale.revision()+1);});
    check(stale.document()==stale_document&&stale.history()==stale_history&&stale.revision()==0,
        "Stale explicit transfer leaves Document, History and revision unchanged");
}
void previewed_folder_api() {
    auto document=fixture();
    document.collections={{"selected-set","Selected",{"a","b"}}};
    document.objects.at("source").contours[0].points[0].x.binding=Binding{{"a","","transform.tx"},1,0,"copy_local_value"};
    const auto before_order=drawable_order(document,scene(document));const auto before_world=transforms(document);
    Session session(document);
    const auto response=request(session,R"({"op":"apply","expected_revision":0,"commands":[{"type":"group_contiguous","composition":"comp","parent":"","members":["a","b"],"id":"preview-folder","name":"Artwork"}]})");
    check(response.find("\"changed\":true")!=std::string::npos,"JSON-lines reaches the shared contiguous Folder command");
    const auto grouped=session.document();
    check(grouped.compositions[0].roots==std::vector<Id>{"preview-folder","source"}&&
        grouped.objects.at("preview-folder").children==std::vector<Id>{"a","b"}&&
        grouped.objects.at("preview-folder").name=="Artwork","Shared command creates the previewed structure");
    check(drawable_order(grouped,scene(grouped))==before_order&&grouped.collections==document.collections&&
        grouped.objects.at("source").contours[0].points[0].x.binding==document.objects.at("source").contours[0].points[0].x.binding,
        "Batch Folder grouping retains paint order, Collection and stable property reference");
    for(const auto& [id,old]:before_world)same_matrix(old.world,transforms(grouped).at(id).world);
    check(decode(encode(grouped))==grouped,"Batch Folder survives native roundtrip");
    session.undo(session.revision());check(session.document()==document,"Batch Folder has one exact Undo");
    session.redo(session.revision());check(session.document()==grouped,"Batch Folder has one exact Redo");
    Session rejected(document);atomic(rejected,"NONCONTIGUOUS_GROUP",{GroupContiguous{"comp","",{"a","source"},"bad-folder","Bad"}});
    Session stale(document);const auto original=stale.document();const auto history=stale.history();
    rejects("REVISION_CONFLICT",[&]{stale.apply({GroupContiguous{"comp","",{"a","b"},"stale-folder","Stale"}},1);});
    check(stale.document()==original&&stale.history()==history&&stale.revision()==0,"Stale grouping leaves Document, History and revision unchanged");
}

}
int main() {
    try{object_visibility_read_contract();object_visibility_link_contract();mask_enabled_read_contract();object_visibility_consumer_parity();create_empty_folder();batch_rename_api();sort_paint_order_api();scene_contract();group_posterize_native_api_and_refusals();mask_geometry_and_validation();mask_with_and_put_inside();neutral_ungroup();move_out_folder();adjacent_folder_transfer();reverse_adjacent_folder_transfer();explicit_nonadjacent_folder_transfer();previewed_folder_api();std::cout<<"PASS "<<checks<<" compositing scene, mask, visibility and structure checks\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
