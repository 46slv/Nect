#include "nect/io.hpp"
#include "nect/expression.hpp"
#include <algorithm>
#include <iostream>

using namespace nect;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void apply(Session& s,std::vector<Command> commands){apply_serializable(s,commands,s.revision());}
void atomic(Session& s,const char* code,std::vector<Command> commands) {
    const auto document=s.document();const auto revision=s.revision();const auto history=s.history();
    try{apply(s,std::move(commands));throw std::runtime_error("Expected rejection");}
    catch(const Error& error){if(error.code!=code)throw std::runtime_error(std::string("Expected ")+code+", got "+error.code+": "+error.what());}
    check(s.document()==document&&s.revision()==revision&&s.history()==history,"Failure preserves state/revision/history");
}
Id copy_of(const Document& document,const Id& original) {
    for(const auto& [id,object]:document.objects)if(object.name==document.objects.at(original).name+" copy")return id;
    throw std::runtime_error("Copy not found: "+original);
}
Document fixture() {
    Session s(demo_document());
    RasterPixels pixels{1,1,{80,120,200,255}};
    apply(s,{CreatePrimitive{"comp-main","","star","Star",default_primitive("star-source","nect.shape.star")},
        CreateText{"comp-main","","text","Text",default_text("text-source","Independent")},
        AddRasterAsset{{"asset","Image","linked","C:/absent/original.png",make_raster(encode_raster_png(pixels))}},
        CreateImage{"comp-main","","image","Image",{"asset",{120},{80}}}});
    Gradient g;g.id="gradient";g.end_x.literal=90;GradientStop a,b;a.id="stop-a";b.id="stop-b";b.offset.literal=1;g.stops={a,b};
    NamedColor color;color.id="palette";color.name="Palette";color.rgba[0].literal=.3;
    apply(s,{SetGradient{"path-A",s.document().objects.at("path-A").legacy_stroke,g},CreateNamedColor{color},
        LinkColor{{"path-A","","op.path-A-stroke.gradient.gradient.stop.stop-a.color"},{"palette","","color"}},
        SetExpression{{{"path-A","point-A2","y"}},{"  ref ( \"path-A\", \"\", \"op.path-A-stroke.gradient.gradient.end_x\" )\n + ref(\"path-B\",\"point-B2\",\"y\")",1}},
        Link{{"path-A","point-A1","y"},{{"path-B","point-B1","y"}}},
        Link{{"path-B","point-B1","x"},{{"path-A","point-A1","x"}}},
        Link{{"star","","generator.outer_radius"},{{"path-A","point-A1","x"},.5,0}},
        Set{{"star","star-source-outer-0-1","x"},15},
        SetExpression{{{"star","star-source-outer-0-1","y"}},{"ref(\"path-A\",\"point-A1\",\"x\")",1}},EnablePointEdit{"star",false},
        AddOperation{"star",default_operation("offset","nect.shape.offset"),0},
        Set{{"star","","op.offset.amount"},0},
        ReorderObjects{"comp-main","",{"path-B","path-A","star","text","image"}}});
    apply(s,{GroupContiguous{"comp-main","",{"path-A","star","text","image"},"group","Group"},
        SetTransformParent{"group","path-B",false},SetTransformParent{"path-A","star",false},
        SetMask{"group",GeometryMask{"mask","star"}},SetVisibility{"star",false}});
    auto document=s.document();document.collections.push_back({"collection","Collection",{"group","path-A"}});validate(document);return document;
}
void retained_group() {
    const auto original=fixture();Session s(original);const DuplicateObjects duplicate{{"path-A","group","star"},"copy"};
    const auto roots=duplicated_roots(original,duplicate);check(roots.size()==1,"Selected descendants collapse under Group");
    apply(s,{duplicate});const auto copied=s.document();check(s.revision()==1,"One command/Undo boundary");
    check(copied.objects.size()==original.objects.size()+5,"Group closure copied once");
    const auto group=copy_of(copied,"group"),path=copy_of(copied,"path-A"),star=copy_of(copied,"star"),text=copy_of(copied,"text"),image=copy_of(copied,"image");
    check(roots.front()==group&&copied.compositions.front().roots==std::vector<Id>{"path-B","group",group},"Copy selected above source Group");
    for(const auto& [id,object]:original.objects)check(copied.objects.at(id)==object,"Original authored object and inbound links unchanged");
    check(copied.collections==original.collections&&copied.named_colors==original.named_colors&&copied.raster_assets==original.raster_assets,"Collections and shared definitions unchanged");
    check(copied.objects.at(group).children==std::vector<Id>{path,star,text,image},"Copy child paint order preserved");
    check(copied.objects.at(group).compositing.mask->source==star&&copied.objects.at(group).compositing.mask->id!="mask","Internal mask copied and remapped");
    check(copied.objects.at(group).transform_parent=="path-B"&&copied.objects.at(path).transform_parent==star,"External and internal Transform Parents preserved");
    const auto& p=copied.objects.at(path);const auto& point=p.contours.front().points.front();
    check(point.id!="point-A1"&&p.contours.front().id!="contour-A","Contour/point IDs fresh");
    check(point.y.binding->source==Ref{"path-B","point-B1","y"},"Outgoing binding retains original target");
    const auto& paint=p.stack.front();const auto& gradient=*paint.gradient;
    check(gradient.id!="gradient"&&gradient.stops.front().id!="stop-a"&&paint.id==p.legacy_stroke,"Gradient IDs and legacy alias remap");
    check(gradient.stops.front().rgba[0].binding->source.object=="palette","Shared Named Color stays shared");
    const auto& expression=*p.contours.front().points.back().y.expression;const auto deps=expression_dependencies(expression);
    check(deps==std::vector<Ref>{gradient_ref(path,paint.id,gradient.id,"end_x"),{"path-B","point-B2","y"}},"Expression mixes remapped internal and original external refs");
    check(expression.source.starts_with("  ref ( ")&&expression.source.find(")\n + ref(")!=std::string::npos,"Expression formatting retained");
    const auto& primitive=copied.objects.at(star);
    check(primitive.source->id!="star-source"&&primitive.point_edit->id==primitive.source->id+"-point-edit"&&!primitive.point_edit->enabled,"Retained generator and disabled correction copied");
    check(primitive.point_edit->overrides.contains(primitive.source->id+"-outer-0-1"),"Generated angular roles remapped");
    check(expression_dependencies(*primitive.point_edit->overrides.begin()->second.at("y").expression).front()==Ref{path,point.id,"x"},"Disabled correction expression remapped");
    check(copied.objects.at(text).text->id!="text-source"&&copied.objects.at(text).text->content=="Independent","Text retains content with fresh source");
    check(copied.objects.at(image).image->asset=="asset","Image placement retains shared accepted asset");
    const auto before=evaluate_transforms(original,evaluate(original)),after=evaluate_transforms(copied,evaluate(copied));
    for(const auto& id:{"group","path-A","star","text","image"})check(before.at(id).world==after.at(copy_of(copied,id)).world,"In-place world placement unchanged");
    check(decode(encode(copied))==copied,"Native roundtrip retains complete authored state");
    check(!export_svg(copied,"comp-main","art-main").empty(),"Copied scene exports");
    s.undo(s.revision());check(s.document()==original,"One Undo restores exact original");s.redo(s.revision());check(s.document()==copied,"Redo preserves new stable IDs");
    apply(s,{Set{{path,point.id,"x"},180}});
    check(evaluate(s.document()).at({star,"","generator.outer_radius"})==90&&evaluate(s.document()).at({"star","","generator.outer_radius"})==50,"Independent edits follow internal copy links only");
    check(evaluate(s.document()).at({"path-B","point-B1","x"})==100,"Inbound reference still follows original");
}
void selection_and_failures() {
    Session s(demo_document());apply(s,{DuplicateObjects{{"path-B","path-A"},"both"}});
    const auto& roots=s.document().compositions.front().roots;
    check(roots==std::vector<Id>{"path-A","path-B","both-1","both-2"},"Selected sibling run preserves paint order regardless of selection order");
    atomic(s,"DUPLICATE_ID",{DuplicateObjects{{"path-A","path-B"},"both"}});
    atomic(s,"INVALID_BATCH",{DuplicateObjects{{},"empty"}});atomic(s,"MISSING_OBJECT",{DuplicateObjects{{"absent"},"missing"}});
    atomic(s,"DUPLICATE_TARGET",{DuplicateObjects{{"path-A","path-A"},"repeat"}});
    atomic(s,"INVALID_ID",{DuplicateObjects{{"path-A"},std::string(49,'x')}});
    atomic(s,"OUT_OF_RANGE",{DuplicateObjects{{"path-A"},"rollback"},Set{{"path-A","","stroke.width"},-1}});
    auto document=s.document();auto other=empty_document("other-doc","other-comp","other-art").compositions.front();
    other.roots={"both-2"};document.compositions.front().roots.pop_back();document.compositions.push_back(other);Session cross(document);
    atomic(cross,"CROSS_COMPOSITION",{DuplicateObjects{{"path-A","both-2"},"cross"}});
    Session api(demo_document());const auto response=request(api,R"({"op":"apply","expected_revision":0,"commands":[{"type":"duplicate_objects","objects":["path-A"],"prefix":"api"}]})");
    check(response.find("\"ok\":true")!=std::string::npos&&response.find("\"created_ids\":[\"api-1\"]")!=std::string::npos,"Adapter exposes common command and created IDs");
    Session small(demo_document(),{1024,1});atomic(small,"HISTORY_LIMIT",{DuplicateObjects{{"path-A"},"limited"}});
}
void nested_selection_and_roles() {
    Session s(fixture());const auto before=s.document();
    const DuplicateObjects command{{"image","path-A"},"nested"};const auto roots=duplicated_roots(before,command);
    apply(s,{command});const auto path=copy_of(s.document(),"path-A"),image=copy_of(s.document(),"image");
    check(roots==std::vector<Id>{path,image},"Noncontiguous selection follows structural paint order");
    check(s.document().objects.at("group").children==std::vector<Id>{"path-A",path,"star","text","image",image},"Noncontiguous runs remain in their original structural owner");
    check(s.document().objects.at(path).transform_parent=="star","Unselected internal Group sibling remains an external Transform Parent");
    s.undo(s.revision());check(s.document()==before,"Undo restores unselected structural owner's exact order");
    apply(s,{SetExpression{{{"text","","text.font_size"}},{"ref(\"star\",\"star-source-outer-0-1\",\"x\") + 100",1}},
        SetExpression{{{"path-A","","composite.opacity"}},{"ref(\"path-A\",\"\",\"op.path-A-stroke.gradient.gradient.stop.stop-b.offset\")",1}}});
    apply(s,{DuplicateObjects{{"group"},"roles"}});const auto text=copy_of(s.document(),"text"),star=copy_of(s.document(),"star"),p=copy_of(s.document(),"path-A");
    const auto dependency=expression_dependencies(*property(s.document(),{text,"","text.font_size"}).expression).front();
    check(dependency==Ref{star,s.document().objects.at(star).source->id+"-outer-0-1","x"},"Generated point reference follows copied source role");
    const auto& paint=s.document().objects.at(p).stack.front();const auto& gradient=*paint.gradient;
    check(expression_dependencies(*property(s.document(),{p,"","composite.opacity"}).expression).front()==gradient_ref(p,paint.id,gradient.id,"stop."+gradient.stops.back().id+".offset"),"Gradient stop reference remaps each nested stable ID");
}
void text_italic_drivers() {
    Session s(empty_document("bool-doc","bool-comp","bool-frame"));
    auto source=default_text("source-text","Source");source.italic=true;
    auto linked=default_text("linked-text","Linked");
    auto expression=default_text("expression-text","Expression");
    apply(s,{CreateText{"bool-comp","","source","Source",source},
        CreateText{"bool-comp","","linked","Linked",linked},
        CreateText{"bool-comp","","expression","Expression",expression},
        LinkTextItalic{{"linked","","text.italic"},{"source","","text.italic"},false},
        SetTextItalicExpression{{"expression","","text.italic"},{"!ref(\"source\",\"\",\"text.italic\")",1},false}});
    const auto original=s.document();
    apply(s,{DuplicateObjects{{"source","linked","expression"},"boolcopy"}});
    const auto copied=s.document();
    const auto source_copy=copy_of(copied,"source");
    const auto linked_copy=copy_of(copied,"linked");
    const auto expression_copy=copy_of(copied,"expression");
    check(std::get<Ref>(*copied.objects.at(linked_copy).text->italic_driver)==Ref{source_copy,"","text.italic"}&&
        evaluate_text_italic(copied,linked_copy),"Duplicated Text italic link targets its copied source");
    const auto& copied_driver=std::get<Expression>(*copied.objects.at(expression_copy).text->italic_driver);
    check(copied_driver.source=="!ref(\""+source_copy+"\",\"\",\"text.italic\")"&&
        !evaluate_text_italic(copied,expression_copy),"Duplicated Text italic expression remaps stable Ref and preserves authored form");
    check(s.document().objects.at("linked").text->italic_driver==original.objects.at("linked").text->italic_driver&&
        s.document().objects.at("expression").text->italic_driver==original.objects.at("expression").text->italic_driver,
        "Text italic duplication leaves original drivers unchanged");
}
}
int main(){try{retained_group();selection_and_failures();nested_selection_and_roles();text_italic_drivers();std::cout<<"PASS "<<checks<<" duplication checks\n";return 0;}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
