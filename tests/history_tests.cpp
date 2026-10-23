#include "nect/io.hpp"
#include <algorithm>
#include <iostream>
#include <numeric>

using namespace nect;
namespace {
int checks=0;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action) {
    try{action();}catch(const Error& error){check(error.code==code,("Expected "+std::string(code)+", got "+error.code).c_str());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
void apply(Session& session,std::vector<Command> commands){session.apply(commands,session.revision());}
std::uint64_t state(const Session& session){return session.history().current_id;}
Document representative() {
    Session seed(empty_document("history-doc","comp","art"));std::vector<Command> commands;
    for(int i=0;i<80;++i) {
        const auto suffix=std::to_string(i);std::vector<Point> points;
        for(int j=0;j<4;++j) {
            Point point;point.id="point-"+suffix+"-"+std::to_string(j);point.x.literal=50+i*5+j*20;point.y.literal=60+j*30;
            point.in_length.literal=12;point.out_length.literal=18;points.push_back(point);
        }
        commands.push_back(CreatePath{"comp","","path-"+suffix,"Curve "+suffix,{{"contour-"+suffix,false,points}}});
    }
    apply(seed,commands);return seed.document();
}
std::size_t snapshot_lower_bound(const Document& document) {
    // Deliberately omits string buffers, allocator overhead and map links. This
    // is a lower bound for one old-style full authored Document snapshot, not a
    // process-memory measurement or a competing serialization implementation.
    std::size_t bytes=sizeof(Document)+document.compositions.size()*sizeof(Composition)+document.collections.size()*sizeof(Collection);
    for(const auto& comp:document.compositions)bytes+=comp.roots.size()*sizeof(Id)+comp.artboards.size()*sizeof(Artboard);
    for(const auto& [id,object]:document.objects) {
        (void)id;bytes+=sizeof(std::pair<const Id,Object>)+object.children.size()*sizeof(Id)+object.contours.size()*sizeof(Contour)+object.stack.size()*sizeof(ShapeOperation);
        for(const auto& contour:object.contours)bytes+=contour.points.size()*sizeof(Point);
        for(const auto& operation:object.stack)bytes+=operation.parameters.size()*sizeof(std::pair<const std::string,Scalar>);
    }
    return bytes;
}
void long_history() {
    const auto original=representative();Session session(original);const Ref x{"path-0","point-0-0","x"};
    check(session.history().states.size()==1&&state(session)==0&&!session.can_undo()&&!session.can_redo(),"Initial document is one navigable state without a snapshot entry");
    std::map<std::uint64_t,std::string> native{{0,encode(original)}};
    for(int i=1;i<=320;++i) {
        apply(session,{Set{x,1000.0+i}});
        check(state(session)==static_cast<std::uint64_t>(i),"History IDs increase on each edit beyond the old 64-entry limit");
        if(i==100||i==200||i==320)native.emplace(state(session),encode(session.document()));
    }
    const auto info=session.history();
    check(info.states.size()==321&&info.max_entries==1024&&info.max_bytes==64*1024*1024&&info.pruned_entries==0,"Default history retains 320 edits inside explicit entry and estimated-byte limits");
    check(info.states.back().label.find("Curve 0")!=std::string::npos&&info.states.back().label.find("Set")!=std::string::npos,"History label includes the operation and readable owner");
    const auto sum=std::accumulate(info.states.begin(),info.states.end(),std::size_t{0},[](auto total,const auto& item){return total+item.estimated_bytes;});
    check(sum==info.retained_bytes,"Reported retained estimate equals admitted transition estimates");
    const auto baseline=snapshot_lower_bound(original)*320;
    check(info.retained_bytes<baseline/8,"Single-object deltas use less than one eighth of a conservative 80-object snapshot baseline");
    std::cout<<"History storage: objects=80 edits=320 retained_estimated_bytes="<<info.retained_bytes
        <<" full_snapshot_lower_bound_bytes="<<baseline<<" estimated_ratio="<<static_cast<double>(info.retained_bytes)/baseline<<'\n';
    for(const auto id:{100ULL,320ULL,0ULL,200ULL}) {
        const auto revision=session.revision();session.restore_history(id,revision);
        check(session.revision()==revision+1&&state(session)==id&&encode(session.document())==native.at(id),"Arbitrary backward/forward jump restores exact native authored state with one revision increment");
    }
    const auto same=session.revision();session.restore_history(200,same);check(session.revision()==same,"Restoring the current state is a true no-op");
    session.undo(session.revision());check(state(session)==199&&property(session.document(),x).literal==1199,"Undo follows the same timeline after a jump");
    session.redo(session.revision());check(state(session)==200&&encode(session.document())==native.at(200),"Redo follows the same retained timeline");
    const auto before=session.history();const auto saved=encode(session.document());
    rejects("HISTORY_STATE_NOT_FOUND",[&]{session.restore_history(99999,session.revision());});
    rejects("REVISION_CONFLICT",[&]{session.restore_history(100,0);});
    check(session.history()==before&&encode(session.document())==saved,"Unknown and stale history requests preserve the complete timeline and state");
    apply(session,{Set{x,42}});check(state(session)==321&&session.history().states.size()==202&&!session.can_redo(),"Editing from an earlier state replaces only redo and never reuses an ID");
    check(session.history().pruned_entries==0,"Discarded redo is distinct from budget pruning");
    rejects("HISTORY_STATE_NOT_FOUND",[&]{session.restore_history(320,session.revision());});
    session.undo(session.revision());check(state(session)==200&&encode(session.document())==native.at(200),"New branch retains its exact predecessor");
    session.redo(session.revision());check(state(session)==321&&property(session.document(),x).literal==42,"New branch can be redone");
}
void authored_roundtrip() {
    auto original=demo_document();original.collections={{"collection","Selected artwork",{"path-A","path-B"}}};
    Session session(original);std::vector<std::pair<std::uint64_t,std::string>> snapshots{{0,encode(original)}};
    const auto edit=[&](std::vector<Command> commands){apply(session,std::move(commands));snapshots.emplace_back(state(session),encode(session.document()));};
    NamedColor color;color.id="brand";color.name="Brand";color.rgba[0].literal=.123456789;
    edit({CreateNamedColor{color},CreatePrimitive{"comp-main","","circle","Circle",{"source","nect.shape.circle",1,
        {{"center_x",{200,{}}},{"center_y",{220,{}}},{"radius",{80,{}}}}}}});
    check(session.history().states.back().label.find("2 commands")!=std::string::npos,"Batch history displays its command count");
    edit({Set{{"circle","source-east","x"},310},Link{{"circle","source-north","y"},{{"path-A","point-A1","y"},1,0,"copy_local_value"}},
        LinkColor{operation_ref("circle","circle-stroke","color"),{"brand","","color"}}});
    check(session.document().objects.at("circle").point_edit.has_value(),"Direct generated point edits create retained PointEdit state");
    edit({EnablePointEdit{"circle",false},RenameNamedColor{"brand","Brand revised"}});
    edit({EnablePointEdit{"circle",true},ConvertToPath{"circle"}});
    Gradient gradient;gradient.id="gradient";GradientStop left;left.id="left";GradientStop right;right.id="right";right.offset.literal=1;gradient.stops={left,right};
    edit({SetGradient{"circle","circle-stroke",gradient}});
    edit({AddArtboard{"comp-main",{"second","Alternate crop",800,0,320,240},1},ReorderArtboards{"comp-main",{"second","art-main"}}});
    edit({GroupContiguous{"comp-main","",{"path-A","path-B"},"group","Group"}});
    edit({Unlink{{"circle","source-north","y"}},DeleteObjects{{"path-A"}}});
    check(session.document().collections.front().members==std::vector<Id>{"path-B"},"Deletion changes collection membership and group children");
    edit({UnlinkColor{operation_ref("circle","circle-stroke","color")},DeleteNamedColor{"brand"},DeleteObjects{{"circle"}}});
    // Label derivation must tolerate a target removed later in a valid batch.
    edit({Set{{"path-B","point-B1","x"},321},DeleteObjects{{"path-B"}}});
    for(auto it=snapshots.rbegin();it!=snapshots.rend();++it) {
        session.restore_history(it->first,session.revision());
        check(encode(session.document())==it->second&&decode(it->second)==session.document(),"Backward history restores full authored native equality including sources, bindings, gradients, collections and identity");
    }
    for(const auto& [id,native]:snapshots) {
        session.restore_history(id,session.revision());
        check(encode(session.document())==native,"Forward history reapplies creates/deletes and vector changes exactly");
    }
    Session reopened(decode(encode(session.document())));check(reopened.history().states.size()==1&&state(reopened)==0,"Native reopen starts a new Session timeline without persisting history");
}
void typed_text_layout_history() {
    Session session(empty_document("layout-history-doc","layout-history-comp","layout-history-frame"));
    auto source=default_text("layout-history-source-text","Source");source.locale="ar-SA";
    auto target=default_text("layout-history-target-text","Target");target.layout="frame";
    apply(session,{CreateText{"layout-history-comp","","layout-history-source","Source",source},
        CreateText{"layout-history-comp","","layout-history-target","Target",target}});
    const auto before=session.history().retained_bytes;
    const Ref source_ref{"layout-history-source","","text.layout"},target_ref{"layout-history-target","","text.layout"};
    apply(session,{LinkTextLayout{target_ref,source_ref,false}});
    const auto linked=session.history();
    check(linked.retained_bytes>before&&linked.states.back().estimated_bytes>0&&
        linked.states.back().label.find("Link Text layout")!=std::string::npos,
        "Typed Text layout link stores its driver delta in estimated History with a readable label");
    apply(session,{UnlinkTextLayout{target_ref}});
    check(session.history().states.back().label.find("Unlink Text layout")!=std::string::npos&&
        !session.document().objects.at("layout-history-target").text->layout_driver,
        "Typed Text layout unlink records a labeled History transition");
    session.undo(session.revision());
    check(session.document().objects.at("layout-history-target").text->layout_driver->link==source_ref&&
        evaluate_text_layout(session.document(),"layout-history-target")=="auto",
        "History Undo restores the typed Text layout driver and projection");
    session.redo(session.revision());
    check(!session.document().objects.at("layout-history-target").text->layout_driver&&
        session.document().objects.at("layout-history-target").text->layout=="auto",
        "History Redo restores the frozen Text layout literal");
    const Ref source_locale{"layout-history-source","","text.locale"},target_locale{"layout-history-target","","text.locale"};
    const auto locale_before=session.history().retained_bytes;
    apply(session,{LinkTextLocale{target_locale,source_locale,false}});
    const auto locale_linked=session.history();
    check(locale_linked.retained_bytes>locale_before&&locale_linked.states.back().estimated_bytes>0&&
        locale_linked.states.back().label.find("Link Text locale")!=std::string::npos&&
        evaluate_text_locale(session.document(),"layout-history-target")=="ar-SA",
        "Typed Text locale link stores its driver delta and evaluated string in retained History");
    apply(session,{UnlinkTextLocale{target_locale}});
    check(session.history().states.back().label.find("Unlink Text locale")!=std::string::npos&&
        !session.document().objects.at("layout-history-target").text->locale_driver&&
        session.document().objects.at("layout-history-target").text->locale=="ar-SA",
        "Typed Text locale unlink records its frozen literal in History");
    session.undo(session.revision());
    check(session.document().objects.at("layout-history-target").text->locale_driver->link==source_locale&&
        evaluate_text_locale(session.document(),"layout-history-target")=="ar-SA",
        "History Undo restores the typed Text locale driver and projection");
    session.redo(session.revision());
    check(!session.document().objects.at("layout-history-target").text->locale_driver&&
        session.document().objects.at("layout-history-target").text->locale=="ar-SA",
        "History Redo restores the frozen Text locale literal");
}
void point_edit_enabled_history() {
    Session seed(empty_document("point-edit-history-doc","point-edit-history-comp","point-edit-history-art"));
    const auto target_source=default_primitive("history-target-generator","nect.shape.circle");
    const auto short_source=default_primitive("short-history-generator","nect.shape.circle");
    const auto long_source=default_primitive("point-edit-history-source-with-a-much-longer-stable-id-generator","nect.shape.circle");
    apply(seed,{CreatePrimitive{"point-edit-history-comp","","history-target","Target",target_source},
        CreatePrimitive{"point-edit-history-comp","","short-source","Short source",short_source},
        CreatePrimitive{"point-edit-history-comp","","long-source","Long source",long_source},
        Set{{"history-target","history-target-generator-east","x"},210},
        Set{{"short-source","short-history-generator-east","x"},320},
        Set{{"long-source","point-edit-history-source-with-a-much-longer-stable-id-generator-east","x"},430}});
    const auto base=seed.document();
    const auto target=point_edit_enabled_ref("history-target","history-target-generator-point-edit");
    const auto short_ref=point_edit_enabled_ref("short-source","short-history-generator-point-edit");
    const auto long_ref=point_edit_enabled_ref("long-source","point-edit-history-source-with-a-much-longer-stable-id-generator-point-edit");
    Session short_measure(base),long_measure(base);
    apply(short_measure,{LinkPointEditEnabled{target,short_ref}});
    apply(long_measure,{LinkPointEditEnabled{target,long_ref}});
    const auto short_bytes=short_measure.history().states.back().estimated_bytes;
    const auto long_bytes=long_measure.history().states.back().estimated_bytes;
    check(long_bytes>short_bytes,
        "Point Edit driver Ref strings contribute to the retained transition estimate");
    Session fits(base,{10,short_bytes}),too_large(base,{10,short_bytes});
    apply(fits,{LinkPointEditEnabled{target,short_ref}});
    const auto before=too_large.document();const auto before_history=too_large.history();const auto before_revision=too_large.revision();
    rejects("HISTORY_LIMIT",[&]{apply(too_large,{LinkPointEditEnabled{target,long_ref}});});
    check(too_large.document()==before&&too_large.history()==before_history&&too_large.revision()==before_revision,
        "A driver Ref that exceeds the byte budget is rejected without changing Document, revision or History");

    Session session(base);
    apply(session,{LinkPointEditEnabled{target,short_ref}});
    auto linked=point_edit_enabled_state(session.document(),target);
    check(session.history().states.back().label.find("Link Point Edit enabled")!=std::string::npos&&
        session.history().states.back().estimated_bytes>0&&linked.driver==short_ref&&linked.literal&&linked.evaluated,
        "Link history retains its exact Point Edit driver and readable transition label");
    apply(session,{EnablePointEdit{"short-source",false}});
    apply(session,{UnlinkPointEditEnabled{target}});
    check(session.history().states.back().label.find("Unlink Point Edit enabled")!=std::string::npos&&
        !point_edit_enabled_state(session.document(),target).driver&&
        !point_edit_enabled_state(session.document(),target).literal,
        "Unlink history stores the evaluated false bypass bit as the frozen literal");
    session.undo(session.revision());
    check(point_edit_enabled_state(session.document(),target).driver==short_ref&&
        !point_edit_enabled_state(session.document(),target).evaluated,
        "Undo restores the Point Edit driver and follows its current source value");
    session.redo(session.revision());
    check(!point_edit_enabled_state(session.document(),target).driver&&
        !point_edit_enabled_state(session.document(),target).literal,
        "Redo restores the frozen authored literal without its former driver");
}
void group_path_follow_history_accounts_for_relation_items() {
    auto document=empty_document("follow-history-doc","comp","art");
    Object group;group.id="follow-history-group";group.name="Followers";group.kind=Kind::group;
    for(int i=0;i<64;++i) {
        Object child;child.id="follow-history-child-"+std::to_string(i);child.name=child.id;
        Point a;a.id=child.id+"-a";Point b;b.id=child.id+"-b";b.x.literal=1;
        child.contours={{child.id+"-contour",false,{a,b}}};group.children.push_back(child.id);document.objects.emplace(child.id,child);
    }
    Object guide;guide.id="follow-history-guide";guide.name="Guide";
    Point a;a.id="follow-history-guide-a";Point b;b.id="follow-history-guide-b";b.x.literal=500;
    guide.contours={{"follow-history-guide-contour",false,{a,b}}};document.objects.emplace(guide.id,guide);document.objects.emplace(group.id,group);
    document.compositions.front().roots={group.id,guide.id};
    GroupPathFollow relation;relation.id="follow-history-relation";relation.path=guide.id;relation.contour="follow-history-guide-contour";
    Session empty_items(document);apply(empty_items,{GroupPathFollowCommand{AttachGroupPathFollow{group.id,relation}}});
    const auto empty_bytes=empty_items.history().retained_bytes;
    for(const auto& child:group.children)relation.items[child]={0,0,true};
    relation.mode="deform";Session many_items(document);apply(many_items,{GroupPathFollowCommand{AttachGroupPathFollow{group.id,relation}}});
    check(many_items.history().retained_bytes>empty_bytes+64*sizeof(GroupPathFollowItem),
        "History accounts for retained relation strings and stable child-ID map allocation");
    const auto attached=encode(many_items.document());many_items.undo(many_items.revision());
    check(many_items.document()==document,"Undo releases relation state without changing retained child geometry");
    many_items.redo(many_items.revision());check(encode(many_items.document())==attached,"Redo restores exact mode/axis and stable item map");
    Session limited(document,{100,empty_bytes+1});const auto before=limited.document();const auto history=limited.history();
    rejects("HISTORY_LIMIT",[&]{apply(limited,{GroupPathFollowCommand{AttachGroupPathFollow{group.id,relation}}});});
    check(limited.document()==before&&limited.history()==history&&limited.revision()==0,
        "Relation item footprint above admitted budget refuses atomically with Undo preserved");
}
void limits_and_gestures() {
    const auto document=demo_document();const Ref x{"path-A","point-A1","x"};
    Session limited(document,{3,64*1024*1024});
    for(int i=1;i<=6;++i)apply(limited,{Set{x,100.0+i}});
    auto info=limited.history();check(info.states.size()==4&&info.states.front().id==3&&info.pruned_entries==3,"Entry pruning retains a usable earliest boundary and its successors");
    limited.restore_history(3,limited.revision());check(property(limited.document(),x).literal==103&&!limited.can_undo()&&limited.can_redo(),"Earliest retained boundary remains restorable without its own full snapshot");
    rejects("HISTORY_STATE_NOT_FOUND",[&]{limited.restore_history(2,limited.revision());});
    Session measured(document);apply(measured,{Set{x,101}});const auto one=measured.history().retained_bytes;
    Session bytes(document,{100,one*2});
    for(int i=1;i<=5;++i)apply(bytes,{Set{x,100.0+i}});
    info=bytes.history();check(info.states.size()==3&&info.pruned_entries==3&&info.retained_bytes<=info.max_bytes,"Estimated-byte budget prunes oldest transitions independently of entry count");
    Session tiny(document,{100,one-1});const auto tiny_native=encode(tiny.document());
    rejects("HISTORY_LIMIT",[&]{apply(tiny,{Set{x,101}});});
    check(tiny.revision()==0&&tiny.history().states.size()==1&&encode(tiny.document())==tiny_native,"An oversized single edit rejects atomically without silently losing Undo");
    bytes.undo(bytes.revision());const auto before=bytes.history();const auto before_native=encode(bytes.document());const auto revision=bytes.revision();
    std::vector<Point> many_points;
    for(int i=0;i<100;++i){Point point;point.id="large-point-"+std::to_string(i);point.x.literal=i;many_points.push_back(point);}
    const CreatePath large{"comp-main","","large-path","Large Path",{{"large-contour",false,many_points}}};
    rejects("HISTORY_LIMIT",[&]{apply(bytes,{large});});
    check(bytes.history()==before&&bytes.revision()==revision&&encode(bytes.document())==before_native&&bytes.can_redo(),"Oversize branch attempt preserves redo, current ID, revision and authored data");
    bytes.begin_gesture(bytes.revision());bytes.update_gesture({large});const auto preview=encode(bytes.preview_document());
    rejects("HISTORY_LIMIT",[&]{bytes.commit_gesture();});
    check(bytes.gesture_active()&&encode(bytes.preview_document())==preview&&bytes.history()==before&&encode(bytes.document())==before_native,"Failed gesture commit retains its preview and the entire committed timeline");
    check(bytes.preview_values()&&*bytes.preview_values()==evaluate(bytes.preview_document()),"History rejection retains matching preview evaluation");
    bytes.cancel_gesture();check(bytes.history()==before,"Cancel after failed admission does not add history");
    Session gestures(document);gestures.begin_gesture(0);
    for(int i=1;i<=40;++i)gestures.update_gesture({Set{x,200.0+i}});
    const auto preview_before=encode(gestures.preview_document());
    rejects("MISSING_REFERENCE",[&]{gestures.update_gesture({Set{{"missing","","x"},1}});});
    check(gestures.revision()==0&&gestures.history().states.size()==1&&encode(gestures.preview_document())==preview_before,"Failed gesture update preserves the last valid preview without creating history");
    rejects("GESTURE_ACTIVE",[&]{gestures.restore_history(0,0);});
    gestures.commit_gesture();check(gestures.revision()==1&&gestures.history().states.size()==2&&property(gestures.document(),x).literal==240,"Forty previews commit as exactly one history entry");
    gestures.begin_gesture(1);gestures.update_gesture({Set{x,99}});gestures.cancel_gesture();check(gestures.revision()==1&&gestures.history().states.size()==2,"Canceled gesture creates no state");
    gestures.begin_gesture(1);gestures.update_gesture({Set{x,99}});gestures.update_gesture({});gestures.commit_gesture();check(gestures.revision()==1&&gestures.history().states.size()==2,"Gesture reset to its start creates no history");
    const auto failed_before=gestures.history();rejects("MISSING_REFERENCE",[&]{apply(gestures,{Set{x,1},Set{{"missing","","x"},2}});});
    check(gestures.history()==failed_before&&property(gestures.document(),x).literal==240,"Failed command batch does not change history");
    apply(gestures,{Set{x,240}});check(gestures.revision()==2&&gestures.history().states.size()==3,"Existing accepted same-value command revision semantics remain intact");
    rejects("INVALID_HISTORY_LIMITS",[&]{Session invalid(document,{0,1000});});
}
}
int main(){try{long_history();authored_roundtrip();typed_text_layout_history();point_edit_enabled_history();group_path_follow_history_accounts_for_relation_items();limits_and_gestures();std::cout<<"PASS "<<checks<<" history checks\n";return 0;}
catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
