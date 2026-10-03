#pragma once
#include "svg_rgb_import_tests.hpp"
#include <set>

namespace svg_linear_gradient_tests {
using namespace svg_rgb_tests;
inline std::string stops() {return R"svg(<stop offset="0" stop-color="rgba(10%,20%,30%,.5)" stop-opacity=".4"/><stop offset="50%" stop-color="#2468" stop-opacity="75%"/><stop offset="1" stop-color="transparent"/>)svg";}
inline std::string definition(const std::string& body=stops(),const std::string& extra={}) {
    return "<linearGradient id=\"paint\" gradientUnits=\"userSpaceOnUse\" x1=\"10\" y1=\"20\" x2=\"90\" y2=\"40\" "+extra+">"+body+"</linearGradient>";
}
inline std::string fixture(const std::string& gradient,const std::string& paint="url(#paint)") {
    return "<svg viewBox=\"0 0 200 200\"><path d=\"M0 0L10 10\" fill=\"red\"/><path d=\"M0 0L20 20\" fill=\""+paint+"\"/><defs>"+gradient+"</defs></svg>";
}
inline void gradient_equal(const Gradient& actual,const Gradient& expected) {
    require(actual.type=="linear"&&actual.enabled&&actual.version==1,"Imported editable linear gradient kind");
    near(actual.start_x.literal,expected.start_x.literal);near(actual.start_y.literal,expected.start_y.literal);
    near(actual.end_x.literal,expected.end_x.literal);near(actual.end_y.literal,expected.end_y.literal);
    require(actual.stops.size()==expected.stops.size(),"Imported gradient stop count");
    for(std::size_t i=0;i<actual.stops.size();++i) {
        near(actual.stops[i].offset.literal,expected.stops[i].offset.literal);
        for(std::size_t c=0;c<4;++c)near(actual.stops[i].rgba[c].literal,expected.stops[i].rgba[c].literal);
    }
}
inline Gradient expected_gradient() {
    Gradient g;g.start_x.literal=10;g.start_y.literal=20;g.end_x.literal=90;g.end_y.literal=40;
    for(const auto& [offset,rgba]:std::vector<std::pair<double,std::array<double,4>>>{{0,{.1,.2,.3,.2}},{.5,{2.0/15,4.0/15,6.0/15,.4}},{1,{0,0,0,0}}}) {
        GradientStop stop;stop.offset.literal=offset;for(std::size_t c=0;c<4;++c)stop.rgba[c].literal=rgba[c];g.stops.push_back(stop);
    }
    return g;
}
inline void run() {
    Session source(empty_document("gradient-source","comp","art"));
    source.apply(read_svg(R"svg(<svg viewBox="0 0 200 200"><path d="M0 0L100 0L100 100Z" fill="red"/></svg>)svg","comp","source","Source",0,0).commands,0);
    auto gradient=expected_gradient();gradient.id="source-gradient";
    for(std::size_t i=0;i<gradient.stops.size();++i)gradient.stops[i].id="source-stop-"+std::to_string(i);
    source.apply({SetGradient{"source-n1","source-n1-paint-fill",gradient},Set{operation_ref("source-n1","source-n1-paint-fill","a"),.4},Set{{"source-n1","","transform.a"},1.5},Set{{"source-n1","","transform.tx"},17}},source.revision());
    const auto native_before=encode(source.document());const auto exported=export_svg(source.document(),"comp","art");
    require(exported.find("<linearGradient")!=std::string::npos&&exported.find("url(#nect-gradient-")!=std::string::npos,"Real Nect exporter gradient fixture");
    QTemporaryDir temp;require(temp.isValid(),"Gradient test scratch");const auto path=temp.path()+"/export.svg";write(path,exported);
    Host host(temp.path()+"/recovery");const auto before=host.session.document();const auto revision=host.session.revision();const auto history=host.session.history();
    host.import_svg(path,before.compositions.front().id,"roundtrip","Roundtrip",0,0,revision);
    Id imported_object,imported_operation;std::size_t count=0;
    for(const auto& [id,object]:host.session.document().objects)for(const auto& operation:object.stack)if(operation.gradient) {
        ++count;gradient_equal(*operation.gradient,gradient);near(operation.parameters.at("a").literal,.4);imported_object=id;imported_operation=operation.id;
    }
    require(count==1,"Export/import must preserve one editable gradient paint");
    const auto original_world=evaluate_transforms(source.document(),evaluate(source.document())).at("source-n1").world;
    const auto imported_world=evaluate_transforms(host.session.document(),evaluate(host.session.document())).at(imported_object).world;
    const auto original_end=map_point(original_world,{90,40}),imported_end=map_point(imported_world,{90,40});near(imported_end.x,original_end.x);near(imported_end.y,original_end.y);
    require(bytes(path)==QByteArray::fromStdString(exported)&&encode(source.document())==native_before,"Gradient roundtrip preserves source bytes");
    require(host.session.revision()==revision+1&&host.session.history().states.size()==history.states.size()+1,"Gradient import is one transaction");
    const auto accepted=host.session.document();require(decode(encode(accepted))==accepted,"Gradient native roundtrip");host.session.undo(host.session.revision());require(host.session.document()==before,"One Undo removes entire gradient import");host.session.redo(host.session.revision());require(host.session.document()==accepted,"Gradient Redo exact state");
    const auto& imported=*accepted.objects.at(imported_object).stack[0].gradient;
    host.session.apply({Set{gradient_ref(imported_object,imported_operation,imported.id,"end_x"),123},Set{gradient_ref(imported_object,imported_operation,imported.id,"stop."+imported.stops[0].id+".r"),.9}},host.session.revision());
    near(host.session.document().objects.at(imported_object).stack[0].gradient->end_x.literal,123);host.session.undo(host.session.revision());require(host.session.document()==accepted,"Imported gradient fields remain editable and undoable");
    std::cout<<"PASS real linear-gradient export/import, local/world coordinates, stop/paint alpha, editable fields, bytes and one Undo/Redo\n";

    const auto shared="<svg viewBox=\"0 0 200 200\"><g opacity=\".5\"><path d=\"M0 0L20 20\" fill=\"url(#paint)\" stroke=\"url(#paint)\" fill-opacity=\".4\" stroke-opacity=\".8\" opacity=\".25\"/><path d=\"M0 0L30 30\" fill=\"url(#paint)\" fill-opacity=\".2\"/></g><defs>"+definition()+"</defs></svg>";
    Session clones(empty_document("clones","comp","art"));clones.apply(read_svg(shared,"comp","clone","Clone",0,0).commands,0);std::set<Id> ids;
    for(const auto& [id,object]:clones.document().objects)for(const auto& operation:object.stack)if(operation.gradient) {gradient_equal(*operation.gradient,expected_gradient());require(ids.insert(operation.gradient->id).second,"Each paint owns an independent gradient clone");const auto alpha=id=="clone-n2"?(operation.type=="nect.paint.fill"?.4:.8):.2;near(operation.parameters.at("a").literal,alpha);}
    require(ids.size()==3,"Shared SVG definition clones into all three native paints");const auto values=evaluate(clones.document());near(values.at({"clone-n1","","composite.opacity"}),.5);near(values.at({"clone-n2","","composite.opacity"}),.25);
    const auto& fill=clones.document().objects.at("clone-n2").stack[0];const auto fill_id=fill.id,gradient_id=fill.gradient->id;
    clones.apply({Set{gradient_ref("clone-n2",fill_id,gradient_id,"end_y"),77}},clones.revision());near(clones.document().objects.at("clone-n2").stack[1].gradient->end_y.literal,40);near(clones.document().objects.at("clone-n3").stack[0].gradient->end_y.literal,40);
    std::cout<<"PASS forward internal reference, independent stop/paint/object/Group alpha and editable per-paint clones\n";

    std::vector<std::string> invalid{
        fixture(definition(stops(),"gradientTransform=\"matrix(1 0 0 1 0 0)\"")),fixture(definition(stops(),"spreadMethod=\"repeat\"")),fixture(definition(stops(),"color-interpolation=\"linearRGB\"")),fixture(definition(stops(),"href=\"#paint\"")),
        fixture("<radialGradient id=\"paint\"/>"),fixture(definition(),"url(https://example.invalid/paint.svg#paint)"),fixture(definition(),"url(#missing)"),fixture(definition()+definition()),
        fixture(definition(R"svg(<stop offset=".5"/><stop offset=".5"/>)svg")),fixture(definition(R"svg(<stop offset=".8"/><stop offset=".2"/>)svg")),fixture(definition(R"svg(<stop offset="0"/><stop offset="1.1"/>)svg")),fixture(definition("<stop offset=\"0\"/>")),
        fixture("<linearGradient id=\"paint\" gradientUnits=\"objectBoundingBox\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\">"+stops()+"</linearGradient>"),
        fixture("<linearGradient id=\"paint\" gradientUnits=\"userSpaceOnUse\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"0\">"+stops()+"</linearGradient>")
    };
    auto many=[](int n){std::string body;for(int i=0;i<n;++i)body+="<stop offset=\""+std::to_string(i)+"%\"/>";return body;};
    Session boundary(empty_document("boundary-doc","comp","art"));boundary.apply(read_svg(fixture(definition(many(64))),"comp","boundary","Boundary",0,0).commands,0);
    invalid.push_back(fixture(definition(many(65))));invalid.push_back(fixture(definition(many(256))));
    for(const auto& svg:invalid) {bool rejected=false;try{(void)read_svg(svg,"comp","reject","Reject",0,0);}catch(const Error& error){require(error.code=="SVG_UNSUPPORTED"||error.code=="SVG_RANGE","Explicit unsupported gradient refusal");rejected=true;}require(rejected,"Unsupported gradients must refuse");}
    host.save(temp.path()+"/existing.nect");host.recover();const auto native=temp.path()+"/existing.nect",recovery=host.persistence().value("recovery_file").toString();
    const auto saved=bytes(native),recovered=bytes(recovery);const auto document=host.session.document();const auto history_before=host.session.history();const auto current=host.session.revision();const auto reject_path=temp.path()+"/reject.svg";write(reject_path,invalid[0]);bool rejected=false;
    try{host.import_svg(reject_path,document.compositions.front().id,"reject","Reject",0,0,current);}catch(const Error& error){require(error.code=="SVG_UNSUPPORTED","Production gradient refusal code");rejected=true;}
    require(rejected&&host.session.document()==document&&host.session.history()==history_before&&host.session.revision()==current,"Malformed gradient import refuses atomically");
    require(bytes(native)==saved&&bytes(recovery)==recovered&&bytes(reject_path)==QByteArray::fromStdString(invalid[0]),"Gradient refusal preserves native/recovery/SVG source bytes");
    std::cout<<"PASS 64-stop native boundary, 16 unsupported/malformed gradient cases and Host atomic refusal\n";
}
}
