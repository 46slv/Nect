#pragma once
#include "svg_linear_gradient_import_tests.hpp"

namespace svg_bbox_gradient_tests {
using namespace svg_rgb_tests;
inline const Object& named(const Document& document,const std::string& name) {
    for(const auto& [id,object]:document.objects)if(object.name==name)return object;
    throw std::runtime_error("Missing bbox fixture label");
}
inline std::string definition(const std::string& attributes="x1=\"25%\" y1=\"0\" x2=\".75\" y2=\"100%\"") {
    return "<linearGradient id=\"paint\" "+attributes+">"+svg_linear_gradient_tests::stops()+"</linearGradient>";
}
inline std::string fixture(const std::string& shape,const std::string& gradient=definition()) {
    return "<svg viewBox=\"0 0 300 200\">"+shape+"<defs>"+gradient+"</defs></svg>";
}
inline void gradient_equal(const Gradient& actual,const std::array<double,4>& coordinates) {
    auto expected=svg_linear_gradient_tests::expected_gradient();expected.start_x.literal=coordinates[0];expected.start_y.literal=coordinates[1];expected.end_x.literal=coordinates[2];expected.end_y.literal=coordinates[3];
    svg_linear_gradient_tests::gradient_equal(actual,expected);
}
inline double parameter(const Gradient& gradient,Vec2 point) {
    const auto dx=gradient.end_x.literal-gradient.start_x.literal,dy=gradient.end_y.literal-gradient.start_y.literal;
    return ((point.x-gradient.start_x.literal)*dx+(point.y-gradient.start_y.literal)*dy)/(dx*dx+dy*dy);
}
inline void run() {
    const auto svg=fixture(R"svg(<g id="parent" transform="translate(100 50)"><rect id="wide" x="10" y="20" width="80" height="40" transform="matrix(2 0 .5 3 7 11)" fill="url(#paint)" stroke="url(#paint)" stroke-width="80" fill-opacity=".4" stroke-opacity=".8" opacity=".25"/><rect id="tall" x="-20" y="50" width="40" height="80" fill="url(#paint)"/></g>)svg");
    QTemporaryDir temp;require(temp.isValid(),"Bbox gradient scratch");const auto path=temp.path()+"/bbox.svg";write(path,svg);
    Host host(temp.path()+"/recovery");const auto before=host.session.document();const auto revision=host.session.revision();const auto history=host.session.history();
    host.import_svg(path,before.compositions.front().id,"bbox","Bbox",0,0,revision);
    const auto accepted=host.session.document();const auto& wide=named(accepted,"wide");const auto& tall=named(accepted,"tall");
    require(wide.stack.size()==2&&tall.stack.size()==1,"Per-paint bbox gradient count");
    for(const auto& operation:wide.stack) {gradient_equal(*operation.gradient,{30,20,710.0/17,1140.0/17});near(operation.parameters.at("a").literal,operation.type=="nect.paint.fill"?.4:.8);}
    gradient_equal(*tall.stack[0].gradient,{-10,50,40,100});near(tall.stack[0].parameters.at("a").literal,1);
    require(wide.stack[0].gradient->id!=wide.stack[1].gradient->id&&wide.stack[0].gradient->id!=tall.stack[0].gradient->id,"Shared bbox definition clones per paint/target");
    // Independent SVG normalized-space oracle. On the 80x40 box, naive
    // endpoint scaling would produce t=.5, rather than .2, at (70,20).
    for(const auto& [point,t]:std::vector<std::pair<Vec2,double>>{{{30,20},0},{{70,20},.2},{{70,60},1},{{50,40},.5},{{10,20},-.1}})near(parameter(*wide.stack[0].gradient,point),t);
    near(parameter(*tall.stack[0].gradient,{10,50}),.2);near(parameter(*tall.stack[0].gradient,{10,130}),1);
    const auto values=evaluate(accepted);near(values.at({wide.id,"","composite.opacity"}),.25);const auto world=evaluate_transforms(accepted,values).at(wide.id).world;
    const auto start=map_point(world,{30,20}),end=map_point(world,{710.0/17,1140.0/17});near(start.x,177);near(start.y,121);near(end.x,107+1990.0/17);near(end.y,61+3420.0/17);
    require(bytes(path)==QByteArray::fromStdString(svg),"Bbox source bytes unchanged");require(host.session.revision()==revision+1&&host.session.history().states.size()==history.states.size()+1,"Bbox import is one transaction");
    require(decode(encode(accepted))==accepted,"Bbox gradient native roundtrip");host.session.undo(host.session.revision());require(host.session.document()==before,"One Undo removes complete bbox import");host.session.redo(host.session.revision());require(host.session.document()==accepted,"Bbox Redo exact state");
    host.session.apply({Set{gradient_ref(wide.id,wide.stack[0].id,wide.stack[0].gradient->id,"end_x"),50}},host.session.revision());near(named(host.session.document(),"wide").stack[1].gradient->end_x.literal,710.0/17);host.session.undo(host.session.revision());require(host.session.document()==accepted,"Per-paint bbox literals remain independently editable");
    std::cout<<"PASS per-target nonuniform bbox/diagonal paint oracle, stroke exclusion, transformed local/world endpoints, clones, source bytes and one Undo/Redo\n";

    Session curve(empty_document("curve-doc","comp","art"));curve.apply(read_svg(fixture(R"svg(<path id="curve" d="M10 20C10 100 90 100 90 20" fill="url(#paint)"/>)svg",definition("gradientUnits=\"objectBoundingBox\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"")),"comp","curve","Curve",0,0).commands,0);
    // Anchors alone have height zero; the control hull ends at 100. The true
    // cubic maximum is y=80 at t=.5, and must determine the gradient endpoint.
    gradient_equal(*named(curve.document(),"curve").stack[0].gradient,{10,20,10,80});
    const auto rectangle=R"svg(<rect id="rect" x="2" y="3" width="10" height="20" fill="url(#paint)"/>)svg";
    for(const auto& attributes:{std::string{},std::string("gradientUnits=\"objectBoundingBox\"")}) {
        Session defaults(empty_document("default-doc","comp","art"));defaults.apply(read_svg(fixture(rectangle,definition(attributes)),"comp","defaults","Default",0,0).commands,0);gradient_equal(*named(defaults.document(),"rect").stack[0].gradient,{2,3,12,3});
    }
    Session outside(empty_document("outside-doc","comp","art"));outside.apply(read_svg(fixture(rectangle,definition("x1=\"-.25\" x2=\"125%\"")),"comp","outside","Outside",0,0).commands,0);gradient_equal(*named(outside.document(),"rect").stack[0].gradient,{-.5,3,14.5,3});
    Session local(empty_document("local-doc","comp","art"));local.apply(read_svg(fixture(rectangle,svg_linear_gradient_tests::definition()),"comp","local","Local",0,0).commands,0);gradient_equal(*named(local.document(),"rect").stack[0].gradient,{10,20,90,40});
    std::cout<<"PASS exact cubic extrema rather than anchors/control hull, omitted/explicit bbox defaults, fractions/percent outside [0,1], retained userSpaceOnUse\n";

    std::vector<std::string> invalid{
        fixture(rectangle,definition("gradientUnits=\"objectBoundingBox\" x2=\"1px\"")),
        fixture(rectangle,definition("gradientUnits=\"userSpaceOnUse\" x1=\"0\" y1=\"0\" x2=\"100%\" y2=\"0\"")),
        fixture(rectangle,definition("gradientTransform=\"scale(0)\"")),
        fixture(rectangle,"<radialGradient id=\"paint\" gradientUnits=\"objectBoundingBox\" cx=\".5\" cy=\".5\" r=\".5\">"+svg_linear_gradient_tests::stops()+"</radialGradient>"),
        fixture(R"svg(<path d="M0 0L20 0" fill="none" stroke="url(#paint)" stroke-width="20"/>)svg"),
        fixture(R"svg(<path d="M0 0" fill="url(#paint)"/>)svg"),
        fixture(R"svg(<path d="M0 0A10 5 30 0 1 20 20" fill="url(#paint)"/>)svg")
    };
    host.save(temp.path()+"/existing.nect");host.recover();const auto native=temp.path()+"/existing.nect",recovery=host.persistence().value("recovery_file").toString();
    const auto saved=bytes(native),recovered=bytes(recovery);const auto document=host.session.document();const auto saved_history=host.session.history();const auto current=host.session.revision();const auto reject_path=temp.path()+"/reject.svg";
    for(const auto& rejected_svg:invalid) {
        write(reject_path,rejected_svg);bool rejected=false;try{host.import_svg(reject_path,document.compositions.front().id,"reject","Reject",0,0,current);}catch(const Error& error){require(error.code=="SVG_UNSUPPORTED","Explicit bbox refusal code");rejected=true;}
        require(rejected&&host.session.document()==document&&host.session.history()==saved_history&&host.session.revision()==current,"Wrong-domain/unsupported bbox import refuses atomically");
        require(bytes(native)==saved&&bytes(recovery)==recovered&&bytes(reject_path)==QByteArray::fromStdString(rejected_svg),"Bbox refusal preserves native/recovery/SVG bytes");
    }
    std::cout<<"PASS 7 wrong-domain/transform/radial/zero-box/arc refusals through atomic Host import\n";
}
}
