#pragma once
#include "svg_rgb_import_tests.hpp"
#include <set>

namespace svg_radial_gradient_tests {
using namespace svg_rgb_tests;
inline std::string stops() {return R"svg(<stop offset="0" stop-color="rgba(17,34,51,.5)" stop-opacity=".6"/><stop offset="37.5%" stop-color="#2468" stop-opacity="75%"/><stop offset="1" stop-color="transparent"/>)svg";}
inline std::string definition(const std::string& extra={},const std::string& radius="7.5px") {
    return "<radialGradient id=\"paint\" gradientUnits=\"userSpaceOnUse\" cx=\"-12.5px\" cy=\"23.25\" r=\""+radius+"\" "+extra+">"+stops()+"</radialGradient>";
}
inline std::string fixture(const std::string& gradient,const std::string& paint="url(#paint)") {
    return "<svg viewBox=\"0 0 200 200\"><path d=\"M0 0L20 20\" fill=\""+paint+"\"/><defs>"+gradient+"</defs></svg>";
}
inline Gradient expected_gradient(double cx=-12.5,double cy=23.25,double radius=7.5) {
    Gradient g;g.type="radial";g.start_x.literal=cx;g.start_y.literal=cy;g.end_x.literal=cx+radius;g.end_y.literal=cy;
    for(const auto& [offset,rgba]:std::vector<std::pair<double,std::array<double,4>>>{{0,{17.0/255,34.0/255,51.0/255,.3}},{.375,{2.0/15,4.0/15,6.0/15,.4}},{1,{0,0,0,0}}}) {
        GradientStop stop;stop.offset.literal=offset;for(std::size_t c=0;c<4;++c)stop.rgba[c].literal=rgba[c];g.stops.push_back(stop);
    }
    return g;
}
inline void gradient_equal(const Gradient& actual,const Gradient& expected) {
    require(actual.type=="radial"&&actual.enabled&&actual.version==1,"Imported editable centered radial gradient kind");
    near(actual.start_x.literal,expected.start_x.literal);near(actual.start_y.literal,expected.start_y.literal);
    near(actual.end_x.literal,expected.end_x.literal);near(actual.end_y.literal,expected.end_y.literal);
    near(std::hypot(actual.end_x.literal-actual.start_x.literal,actual.end_y.literal-actual.start_y.literal),std::hypot(expected.end_x.literal-expected.start_x.literal,expected.end_y.literal-expected.start_y.literal));
    require(actual.stops.size()==expected.stops.size(),"Radial stop count");
    for(std::size_t i=0;i<actual.stops.size();++i) {near(actual.stops[i].offset.literal,expected.stops[i].offset.literal);for(std::size_t c=0;c<4;++c)near(actual.stops[i].rgba[c].literal,expected.stops[i].rgba[c].literal);}
}
inline void run() {
    Session source(empty_document("radial-source","comp","art"));
    source.apply(read_svg(R"svg(<svg viewBox="0 0 200 200"><path d="M0 0L100 0L100 100Z" fill="red"/></svg>)svg","comp","source","Source",0,0).commands,0);
    auto gradient=expected_gradient(12.5,-7.25,50);gradient.id="source-gradient";
    // A 30/40 radius vector proves that importing SVG retains the circle, not
    // the native endpoint direction discarded by the real exporter.
    gradient.end_x.literal=42.5;gradient.end_y.literal=32.75;
    for(std::size_t i=0;i<gradient.stops.size();++i)gradient.stops[i].id="source-stop-"+std::to_string(i);
    source.apply({SetGradient{"source-n1","source-n1-paint-fill",gradient},Set{operation_ref("source-n1","source-n1-paint-fill","a"),.4},Set{{"source-n1","","transform.a"},1.5},Set{{"source-n1","","transform.c"},.25},Set{{"source-n1","","transform.tx"},17}},source.revision());
    const auto native_before=encode(source.document());const auto exported=export_svg(source.document(),"comp","art");
    require(exported.find("<radialGradient")!=std::string::npos&&exported.find("cx=\"12.5\" cy=\"-7.25\" r=\"50\"")!=std::string::npos,"Real native exporter center/radius oracle");
    QTemporaryDir temp;require(temp.isValid(),"Radial test scratch");const auto path=temp.path()+"/export.svg";write(path,exported);
    Host host(temp.path()+"/recovery");const auto before=host.session.document();const auto revision=host.session.revision();const auto history=host.session.history();
    host.import_svg(path,before.compositions.front().id,"roundtrip","Roundtrip",0,0,revision);
    Id object_id,operation_id;std::size_t count=0;
    for(const auto& [id,object]:host.session.document().objects)for(const auto& operation:object.stack)if(operation.gradient) {
        ++count;gradient_equal(*operation.gradient,expected_gradient(12.5,-7.25,50));near(operation.parameters.at("a").literal,.4);object_id=id;operation_id=operation.id;
    }
    require(count==1,"Radial export/import retains one editable paint");
    const auto original_world=evaluate_transforms(source.document(),evaluate(source.document())).at("source-n1").world;
    const auto imported_world=evaluate_transforms(host.session.document(),evaluate(host.session.document())).at(object_id).world;
    for(const auto& p:std::vector<Vec2>{{12.5,-7.25},{62.5,-7.25},{12.5,42.75},{-37.5,-7.25}}) {const auto original=map_point(original_world,p),imported=map_point(imported_world,p);near(imported.x,original.x);near(imported.y,original.y);}
    require(bytes(path)==QByteArray::fromStdString(exported)&&encode(source.document())==native_before,"Radial import preserves SVG/native source bytes");
    require(host.session.revision()==revision+1&&host.session.history().states.size()==history.states.size()+1,"Radial import is one transaction");
    const auto accepted=host.session.document();require(decode(encode(accepted))==accepted,"Radial native encoding roundtrip");host.session.undo(host.session.revision());require(host.session.document()==before,"One Undo removes entire radial import");host.session.redo(host.session.revision());require(host.session.document()==accepted,"Radial Redo exact state");
    const auto& imported=*accepted.objects.at(object_id).stack[0].gradient;
    host.session.apply({Set{gradient_ref(object_id,operation_id,imported.id,"end_x"),72.5},Set{gradient_ref(object_id,operation_id,imported.id,"stop."+imported.stops[0].id+".a"),.9}},host.session.revision());
    near(host.session.document().objects.at(object_id).stack[0].gradient->end_x.literal,72.5);near(host.session.document().objects.at(object_id).stack[0].gradient->stops[0].rgba[3].literal,.9);host.session.undo(host.session.revision());require(host.session.document()==accepted,"Radial coordinates/stops remain editable and undoable");
    std::cout<<"PASS native radial self-export/import, center/radius/world oracle, stop/paint alpha, editable fields, source bytes and one Undo/Redo\n";

    const auto shared="<svg viewBox=\"0 0 200 200\"><g opacity=\".5\"><path d=\"M0 0L20 20\" fill=\"url(#paint)\" stroke=\"url(#paint)\" fill-opacity=\".4\" stroke-opacity=\".8\" opacity=\".25\"/></g><defs>"+definition()+"</defs></svg>";
    Session clones(empty_document("clone-doc","comp","art"));clones.apply(read_svg(shared,"comp","clone","Clone",0,0).commands,0);std::set<Id> ids;
    for(const auto& [id,object]:clones.document().objects)for(const auto& operation:object.stack)if(operation.gradient) {
        gradient_equal(*operation.gradient,expected_gradient());require(ids.insert(operation.gradient->id).second,"Radial paint owns independent clone");near(operation.parameters.at("a").literal,operation.type=="nect.paint.fill"?.4:.8);
    }
    require(ids.size()==2,"Shared radial definition clones into fill/stroke");const auto values=evaluate(clones.document());near(values.at({"clone-n1","","composite.opacity"}),.5);near(values.at({"clone-n2","","composite.opacity"}),.25);
    const auto& fill=clones.document().objects.at("clone-n2").stack[0];const auto fill_id=fill.id,gradient_id=fill.gradient->id;
    clones.apply({Set{gradient_ref("clone-n2",fill_id,gradient_id,"end_x"),5}},clones.revision());near(clones.document().objects.at("clone-n2").stack[1].gradient->end_x.literal,-5);
    std::cout<<"PASS independent numeric radial/stop oracle, forward reference, separate alpha and editable per-paint clones\n";

    std::vector<std::string> invalid;
    for(const auto& extra:{"fx=\"-12.5\"","fy=\"23.25\"","fr=\"0\"","gradientTransform=\"matrix(1 0 0 1 0 0)\"","spreadMethod=\"reflect\"","color-interpolation=\"linearRGB\"","href=\"#paint\""})invalid.push_back(fixture(definition(extra)));
    invalid.push_back(fixture(definition(),"url(https://example.invalid/paint.svg#paint)"));
    for(const auto& radius:{"0","-1","1e-9","50%","1e8"})invalid.push_back(fixture(definition({},radius)));
    invalid.push_back(fixture("<radialGradient id=\"paint\" gradientUnits=\"objectBoundingBox\" cx=\"0\" cy=\"0\" r=\"1\">"+stops()+"</radialGradient>"));
    invalid.push_back(fixture("<radialGradient id=\"paint\" gradientUnits=\"userSpaceOnUse\" cx=\"10000000\" cy=\"0\" r=\"1\">"+stops()+"</radialGradient>"));
    invalid.push_back(fixture("<radialGradient id=\"paint\" gradientUnits=\"userSpaceOnUse\" cx=\"1000000\" cy=\"0\" r=\".00000001\">"+stops()+"</radialGradient>"));
    for(const auto& svg:invalid) {bool rejected=false;try{(void)read_svg(svg,"comp","reject","Reject",0,0);}catch(const Error& error){require(error.code=="SVG_UNSUPPORTED"||error.code=="SVG_RANGE","Explicit radial refusal code");rejected=true;}require(rejected,"Unsupported radial semantics must refuse");}
    host.save(temp.path()+"/existing.nect");host.recover();const auto native=temp.path()+"/existing.nect",recovery=host.persistence().value("recovery_file").toString();
    const auto saved=bytes(native),recovered=bytes(recovery);const auto document=host.session.document();const auto history_before=host.session.history();const auto current=host.session.revision();const auto reject_path=temp.path()+"/reject.svg";write(reject_path,invalid[0]);bool rejected=false;
    try{host.import_svg(reject_path,document.compositions.front().id,"reject","Reject",0,0,current);}catch(const Error& error){require(error.code=="SVG_UNSUPPORTED","Production radial refusal code");rejected=true;}
    require(rejected&&host.session.document()==document&&host.session.history()==history_before&&host.session.revision()==current,"Unsupported focal import refuses atomically");
    require(bytes(native)==saved&&bytes(recovery)==recovered&&bytes(reject_path)==QByteArray::fromStdString(invalid[0]),"Radial refusal preserves native/recovery/source bytes");
    std::cout<<"PASS 16 precise focal/transform/spread/interpolation/reference/range refusals and Host atomic refusal\n";
}
}
