#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void apply(Session& s,std::vector<Command> commands){s.apply(commands,s.revision());}
const Artboard& board(const Document& d,const Id& id){
    const auto& bs=d.compositions.front().artboards;
    auto p=std::find_if(bs.begin(),bs.end(),[&](const auto& b){return b.id==id;});
    if(p==bs.end())throw std::runtime_error("missing board");return *p;
}
Document fixture(){
    auto d=empty_document("dup-doc","comp","source");
    d.compositions.front().artboards.push_back({"a","A",100,100,640,480});
    Object root;root.id="root";root.name="Source";root.kind=Kind::group;root.children={"logo"};root.visible=false;
    Object logo;logo.id="logo";logo.name="Logo";logo.kind=Kind::path;
    logo.source=default_primitive("rect","nect.shape.rectangle");
    logo.source->parameters.at("width").literal=60;logo.source->parameters.at("height").literal=40;
    logo.transform[4].literal=20;
    d.objects.emplace(root.id,root);d.objects.emplace(logo.id,logo);d.compositions.front().roots={"root"};
    Session s(d);
    apply(s,{DefinitionCommand{CreateDefinition{{"def","Shared","root"}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"tmpl","Template","source",Id{"def"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","a","tmpl",Id{"content"}}},
        DefinitionCommand{SetInstanceOverride{"content",{"logo","","transform.tx"},90}},
        ArtboardGuideCommand{AddArtboardGuide{"comp","source",{"source-guide","Source guide","x",20,true}}},
        ArtboardGuideCommand{AddArtboardGuide{"comp","a",{"local-guide","Local","y",15,true}}},
        ArtboardGuideCommand{SetArtboardGuideOverride{"comp","a","source-guide","position",45.0}}});
    return s.document();
}
Command duplicate(Id prefix="copy",double x=600,std::size_t index=2){
    return ArtboardTemplateCommand{DuplicateTemplateArtboard{"comp","a",prefix,x,100,index}};
}
void expect_atomic(Session& s,std::vector<Command> commands){
    auto bytes=encode(s.document());auto rev=s.revision();auto history=s.history();bool failed=false;
    try{apply(s,std::move(commands));}catch(const Error&){failed=true;}
    check(failed,"invalid command must refuse");
    check(encode(s.document())==bytes&&s.revision()==rev&&s.history()==history,"failure is atomic");
}
void lifecycle(){
    Session s(fixture());auto before=encode(s.document());auto source=s.document().objects.at("logo");
    apply(s,{duplicate()});auto after=encode(s.document());
    const auto& b=board(s.document(),"copy-artboard");
    check(b.x==600&&b.y==100&&b.template_assignment->template_id=="tmpl","new frame retains Template");
    check(b.template_assignment->grid_id=="copy-grid","fresh Grid identity");
    check(b.local_guides.size()==1&&b.local_guides[0].id=="copy-guide-1"&&b.local_guides[0].position==15,"fresh local Guide identity");
    check(b.template_assignment->guide_position_overrides.at("source-guide")==45,"Guide override retains source identity");
    check(b.template_assignment->content_instance=="copy-content-1","fresh owned Instance identity");
    const auto& item=s.document().objects.at("copy-content-1");
    check(item.instance->definition=="def"&&item.instance->overrides==s.document().objects.at("content").instance->overrides,"Definition and field overrides preserved");
    check(item.transform[4].literal==600&&item.transform[5].literal==100,"only placement gets frame delta");
    check(s.document().objects.at("logo")==source&&s.document().definitions.size()==1,"source artwork/Definition unchanged");
    check(s.document().compositions.front().roots==std::vector<Id>({"root","content","copy-content-1"}),"root insertion preserves original order");
    check(encode(decode(after))==after,"native cold read retains exact authored state");
    s.undo(s.revision());check(encode(s.document())==before,"one Undo restores pre-duplicate bytes");
    s.redo(s.revision());check(encode(s.document())==after,"one Redo restores all generated identities");
    apply(s,{Set{{"logo","","generator.width"},80}});
    const auto v=evaluate(s.document());auto scene=evaluate_scene(s.document(),"comp",v,evaluate_transforms(s.document(),v));
    int seen=0;
    for(const auto& [proxy,owner]:scene.instance_owners)if(scene.instance_sources.at(proxy)=="logo"){
        check(scene.expanded_document->objects.at(proxy).source->parameters.at("width").literal==80,"source width follows in both copies");++seen;
    }
    check(seen==2,"both instances evaluated");
    apply(s,{DefinitionCommand{ResetInstanceOverride{"copy-content-1",{"logo","","transform.tx"}}}});
    check(s.document().objects.at("copy-content-1").instance->overrides.empty()&&!s.document().objects.at("content").instance->overrides.empty(),"Reset only affects duplicate");
    const auto reset_values=evaluate(s.document());
    const auto reset_scene=evaluate_scene(s.document(),"comp",reset_values,evaluate_transforms(s.document(),reset_values));
    for(const auto& [proxy,owner]:reset_scene.instance_owners)if(reset_scene.instance_sources.at(proxy)=="logo")
        check(reset_scene.expanded_values->at({proxy,"","transform.tx"})==(owner=="content"?90:20),"Reset restores B x20 while A stays x90");
    expect_atomic(s,{duplicate()});
}
void negatives(){
    Session s(fixture());
    expect_atomic(s,{duplicate("bad",std::numeric_limits<double>::infinity())});
    expect_atomic(s,{duplicate("bad",600,1000)});
    expect_atomic(s,{ArtboardTemplateCommand{DuplicateTemplateArtboard{"comp","missing","bad",600,100,2}}});
    expect_atomic(s,{ArtboardTemplateCommand{DuplicateTemplateArtboard{"other","a","bad",600,100,2}}});
    expect_atomic(s,{ArtboardTemplateCommand{DuplicateTemplateArtboard{"comp","source","bad",600,100,2}}});
    expect_atomic(s,{duplicate(),Set{{"missing","","transform.tx"},1}});
    auto before=encode(s.document());bool rejected=false;
    try{s.apply({duplicate()},s.revision()+1);}catch(const Error&){rejected=true;}
    check(rejected&&encode(s.document())==before,"stale revision rejects");
    apply(s,{Link{{"content","","transform.tx"},{"logo","","transform.tx"}}});
    expect_atomic(s,{duplicate()});
}
void layout_and_placement(){
    auto d=fixture();
    auto& a=d.compositions.front().artboards[1];
    a.width_driver=ArtboardSizeDriver{Ref{"source","","artboard.width"}};
    a.layout=ArtboardLayout{Margin{10,20,30,40},Grid{a.template_assignment->grid_id,{0,0,900,150},2,3,4,5}};
    a.layout->margin->left_driver=Ref{"source","","artboard.height"};
    a.layout->grid->column_gutter_driver=Ref{"source","","artboard.height"};
    a.template_assignment->margin_overridden=true;a.template_assignment->grid_overridden=true;
    a.template_assignment->guide_enabled_overrides["source-guide"]=false;
    d.compositions.front().artboards[0].local_guides.push_back({"removed-source-guide","Suppressed","x",10,true});
    a.template_assignment->detached_guides={"removed-source-guide"};
    Session s(d);apply(s,{duplicate()});
    auto expected=a;const auto& b=board(s.document(),"copy-artboard");
    check(b.width_driver==expected.width_driver&&b.layout->margin==expected.layout->margin,"frame/Margin references retained");
    expected.layout->grid->id="copy-grid";
    check(b.layout->grid==expected.layout->grid,"Grid values/sources retained with fresh ID");
    check(b.template_assignment->guide_enabled_overrides==a.template_assignment->guide_enabled_overrides&&
        b.template_assignment->detached_guides==a.template_assignment->detached_guides,"Guide suppression/enabled overrides retained");
    const auto guides=effective_artboard_guides(s.document(),"comp","copy-artboard");
    check(std::none_of(guides.begin(),guides.end(),[](const auto& g){return g.guide_id=="removed-source-guide";}),"real inherited Guide remains suppressed");
    check(std::any_of(guides.begin(),guides.end(),[](const auto& g){return g.guide_id=="source-guide"&&!g.enabled;}),"inherited Guide remains disabled");
    auto bytes=encode(s.document());Session cold(decode(bytes));
    check(encode(cold.document())==bytes,"driven layout and Guide states survive cold read");
    for(int mode=0;mode<3;++mode){
        auto unsafe=fixture();auto& content=unsafe.objects.at("content");
        if(mode==0)content.transform[5].binding=Binding{Ref{"logo","","transform.ty"}};
        if(mode==1)content.transform[4].expression=Expression{"100",1};
        if(mode==2)content.transform_parent="logo";
        Session u(unsafe);expect_atomic(u,{duplicate()});
    }
    auto collision=fixture();collision.compositions.front().artboards[0].local_guides.push_back({"copy-grid","Collision","x",0,true});
    Session c(collision);expect_atomic(c,{duplicate()});
    Session overlong(fixture());expect_atomic(overlong,{duplicate(std::string(33,'a'))});
    Session maximum(fixture());apply(maximum,{duplicate(std::string(32,'a'))});
    check(maximum.document().objects.size()==4,"maximum prefix remains valid");
    for(const auto* id:{"copy-artboard","copy-guide-1"}){
        auto conflict=fixture();conflict.compositions.front().artboards[0].local_guides.push_back({id,"Collision","x",0,true});
        Session v(conflict);expect_atomic(v,{duplicate()});
    }
    auto wrong=fixture();wrong.compositions.push_back(Composition{"other","Other",{},{{"other-board","Other"}}});
    Session w(wrong);expect_atomic(w,{ArtboardTemplateCommand{DuplicateTemplateArtboard{"other","a","bad",600,100,1}}});
    auto affine=fixture();auto& inst=affine.objects.at("content");
    inst.transform[0].literal=2;inst.transform[3].literal=3;inst.transform[4].literal=130;
    Session shifted(affine);apply(shifted,{duplicate("offset",600,0)});
    check(shifted.document().compositions.front().artboards.front().id=="offset-artboard","explicit front insertion honored");
    const auto& moved=shifted.document().objects.at("offset-content-1");
    check(moved.transform[0].literal==2&&moved.transform[3].literal==3&&moved.transform[4].literal==630,
        "nonidentity affine and content offset preserved at front insertion");
}
void json_path(){
    Session s(fixture());
    const auto response=request(s,R"({"op":"apply","expected_revision":0,"commands":[{"type":"duplicate_template_artboard","composition":"comp","artboard":"a","id_prefix":"api","x":600,"y":100,"index":2}]})");
    check(response.find("\"ok\":true")!=std::string::npos,"canonical JSON command succeeds");
    check(board(s.document(),"api-artboard").template_assignment->content_instance=="api-content-1","JSON shares core identities");
    const auto bytes=encode(s.document());const auto revision=s.revision();const auto history=s.history();
    const auto bad=request(s,R"({"op":"apply","expected_revision":1,"commands":[{"type":"duplicate_template_artboard","composition":"comp","artboard":"a","id_prefix":"bad","x":600,"y":100,"index":-1}]})");
    check(bad.find("\"ok\":false")!=std::string::npos&&encode(s.document())==bytes&&s.revision()==revision&&s.history()==history,"invalid JSON index is atomic");
}
}
int main(){try{lifecycle();negatives();layout_and_placement();json_path();std::cout<<"PASS "<<checks<<" Template duplicate checks\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
