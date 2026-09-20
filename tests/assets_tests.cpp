#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F f){try{f();}catch(const Error& e){check(e.code==code,std::string("Expected ")+code+", got "+e.code+": "+e.what());return;}throw std::runtime_error(std::string("Missing ")+code);}
void apply(Session& s,std::vector<Command> c){apply_serializable(s,c,s.revision());}
void atomic(Session& s,const char* code,std::vector<Command> c){const auto d=s.document();const auto r=s.revision();const auto h=s.history();rejects(code,[&]{apply(s,c);});check(s.document()==d&&s.revision()==r&&s.history()==h,"Failed image command retains document, revision and history");}
Raster raster(unsigned char color=77,unsigned size=4) {
    RasterPixels p{size,size,{}};for(unsigned i=0;i<size*size;++i)p.rgba.insert(p.rgba.end(),{color,100,200,static_cast<unsigned char>(i%2?255:128)});
    return make_raster(encode_raster_png(p));
}
std::string replace(std::string text,const std::string& from,const std::string& to){const auto p=text.find(from);check(p!=text.npos,"Mutation fixture field exists");text.replace(p,from.size(),to);return text;}
void semantics() {
    const auto first=raster(),second=raster(180);Session s(empty_document("document","composition","artboard"));
    apply(s,{AddRasterAsset{{"asset","Image source","linked","C:/absent/image.png",first}},CreateImage{"composition","","image","Image",{"asset",{160},{120}}},CreateImage{"composition","","copy","Shared",{"asset",{80},{60}}}});
    check(s.revision()==1&&s.document().objects.size()==2,"Import+shared placement is one transaction");
    const auto before=s.document();const auto native=encode(before);check(decode(native)==before&&encode(decode(native))==native,"Original bytes, locators, IDs and Image Scalars round-trip exactly");
    check(s.document().raster_assets.at("asset").payload==first,"Session retains shared immutable payload");
    auto clone=s.document();check(clone.raster_assets.at("asset").payload==first,"Snapshot copies retain the same byte allocation");
    s.begin_gesture(s.revision());s.update_gesture({Set{{"image","","image.width"},170}});
    check(s.preview_document().raster_assets.at("asset").payload==first&&s.document()==before,"Gesture previews share bytes without mutating committed state");s.cancel_gesture();
    atomic(s,"ASSET_IN_USE",{DeleteRasterAsset{"asset"}});atomic(s,"MISSING_ASSET",{CreateImage{"composition","","bad","Bad",{"absent",{10},{10}}}});
    atomic(s,"INVALID_DOMAIN",{AddOperation{"image",default_operation("invalid-op","nect.paint.fill"),0}});
    atomic(s,"INVALID_MASK_SOURCE",{SetMask{"copy",GeometryMask{"invalid-mask","image"}}});
    atomic(s,"OUT_OF_RANGE",{Set{{"image","","image.width"},0}});
    atomic(s,"INVALID_ASSET_LOCATOR",{ReplaceRasterAsset{{"asset","Image","linked","https://invalid/image.png",first}}});
    apply(s,{Link{{"copy","","image.width"},Binding{{"image","","image.width"},.5,0,"copy_local_value"}},SetExpression{{{"copy","","image.height"}},Expression{"ref(\"copy\",\"\",\"image.width\") * .75",1},false}});
    check(property_unit({"image","","image.width"})=="du"&&evaluate(s.document()).at({"copy","","image.height"})==60,"Dimensions share unit-checked links and expressions");
    atomic(s,"DRIVEN_PROPERTY",{Set{{"copy","","image.height"},10}});
    apply(s,{Set{{"image","","image.width"},200}});check(evaluate(s.document()).at({"copy","","image.width"})==100&&evaluate(s.document()).at({"copy","","image.height"})==75,"Shared scalar dependency updates both dimensions");
    const auto linked=s.document();auto asset=linked.raster_assets.at("asset");asset.payload=second;apply(s,{ReplaceRasterAsset{asset}});
    check(s.document().objects==linked.objects&&s.document().raster_assets.at("asset").payload==second,"Shared replacement preserves authored placement properties");
    const auto replaced=s.document();s.undo(s.revision());check(s.document()==linked&&s.document().raster_assets.at("asset").payload==first,"One Undo restores accepted payload");s.redo(s.revision());check(s.document()==replaced,"Redo restores replacement");
    check(s.history().retained_bytes>first->bytes().size()+second->bytes().size(),"History accounts for accepted byte payloads");
    auto values=evaluate(s.document());auto transforms=evaluate_transforms(s.document(),values);const auto bounds=object_bounds(s.document(),"image",values,transforms);
    check(bounds&&bounds->left==0&&bounds->top==0&&bounds->right==200&&bounds->bottom==120,"Image bounds use display dimensions");
    const auto scene=evaluate_scene(s.document(),"composition",values,transforms);check(scene.images.size()==2&&scene.shapes.empty(),"Image scene projection is distinct from shape geometry");
    rejects("INVALID_DOMAIN",[&]{evaluate_shape(s.document(),"image",values);});
    auto mask=default_primitive("mask-source","nect.shape.circle");apply(s,{CreatePrimitive{"composition","","mask","Mask",mask},SetVisibility{"mask",false},SetMask{"image",GeometryMask{"clip","mask"}},SetCompositing{"image","multiply",false}});
    const auto svg=export_svg(s.document(),"composition","artboard");const auto data=svg.find("data:image/png;base64,");check(data!=svg.npos&&svg.find("data:image/png;base64,",data+1)==svg.npos,"SVG embeds each shared asset only once");
    check(svg.find("mix-blend-mode:multiply")!=svg.npos&&svg.find("clip-path=\"url(#clip)\"")!=svg.npos,"Image SVG interoperates with masks and blends");
    const auto start=data+22,end=svg.find('"',start);const auto png=make_raster(base64_decode(svg.substr(start,end-start)));
    check(decode_raster(*png)==decode_raster(*second),"SVG normalized PNG has exactly the accepted display pixels");
    atomic(s,"ASSET_IN_USE",{DeleteObjects{{"image"}},DeleteRasterAsset{"asset"}});
    apply(s,{DeleteObjects{{"image","copy"}},DeleteRasterAsset{"asset"}});check(s.document().raster_assets.empty(),"Unused asset can be removed in the same atomic delete batch");
    s.undo(s.revision());check(s.document().raster_assets.at("asset").payload==second,"Delete Undo restores accepted source allocation");
    auto malformed=replace(native,"\"sha256\":\""+first->sha256(),"\"sha256\":\""+std::string(64,'0'));
    rejects("ASSET_METADATA_MISMATCH",[&]{decode(malformed);});
    malformed=replace(native,"\"interpretation_version\":1","\"interpretation_version\":2");rejects("ASSET_METADATA_MISMATCH",[&]{decode(malformed);});
    malformed=replace(native,"\"version\":\"0.13\"","\"version\":\"0.12\"");rejects("UNKNOWN_FIELD",[&]{decode(malformed);});
    check(base64_decode(base64_encode(first->bytes()))==first->bytes(),"Canonical base64 preserves every byte");
    for(const auto* bad:{"","A","AA=A","AA==AAAA","AB==","AAB=","AA?A","AA\nA"})rejects("INVALID_ASSET_BYTES",[&]{base64_decode(bad);});
    Session limited(empty_document("limited","plane","frame"),{1024,128});rejects("HISTORY_LIMIT",[&]{limited.apply({AddRasterAsset{{"large","Large","embedded","",first}}},0);});check(limited.document().raster_assets.empty(),"History admission is atomic");
}
void budgets() {
    auto d=empty_document("d","c","a");const auto small=raster();
    for(int i=0;i<129;++i){const auto id="asset-"+std::to_string(i);d.raster_assets.emplace(id,RasterAsset{id,id,"embedded","",small});}
    rejects("LIMIT",[&]{validate(d);});d.raster_assets.clear();
    const auto million=raster(77,1024);
    for(int i=0;i<33;++i){const auto id="asset-"+std::to_string(i);d.raster_assets.emplace(id,RasterAsset{id,id,"embedded","",million});}
    rejects("ASSET_LIMIT",[&]{validate(d);});d.raster_assets.clear();
    RasterPixels noise{256,256,{}};unsigned state=72811;for(unsigned i=0;i<256*256*4;++i){state^=state<<13;state^=state>>17;state^=state<<5;noise.rgba.push_back(static_cast<unsigned char>(state));}
    const auto payload=make_raster(encode_raster_png(noise));check(payload->bytes().size()>250000,"Byte-budget fixture is incompressible source data");
    for(int i=0;i<101;++i){const auto id="asset-"+std::to_string(i);d.raster_assets.emplace(id,RasterAsset{id,id,"embedded","",payload});}
    rejects("ASSET_LIMIT",[&]{validate(d);});
}
}
int main() {
#ifndef _WIN32
    std::cout<<"Raster semantic fixtures require Windows WIC\n";return 0;
#else
    try{semantics();budgets();std::cout<<"PASS "<<checks<<" retained Image semantic/codec checks\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
#endif
}
