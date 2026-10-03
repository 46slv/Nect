#pragma once
#include "svg_bbox_gradient_import_tests.hpp"

namespace svg_gradient_transform_tests {
using namespace svg_rgb_tests;
using svg_bbox_gradient_tests::named;
using svg_bbox_gradient_tests::parameter;
using svg_bbox_gradient_tests::gradient_equal;
inline std::string definition(const std::string& transform,const std::string& coordinates="x1=\"0\" y1=\"0\" x2=\"10\" y2=\"10\"",const std::string& units="userSpaceOnUse") {
    return "<linearGradient id=\"paint\" gradientUnits=\""+units+"\" "+coordinates+(transform.empty()?"":" gradientTransform=\""+transform+"\"")+">"+svg_linear_gradient_tests::stops()+"</linearGradient>";
}
inline std::string fixture(const std::string& gradient,const std::string& shape=R"svg(<rect id="paint" width="80" height="40" fill="url(#paint)"/>)svg") {
    // Shape labels and paint-server IDs must not collide.
    auto artwork=shape;const auto at=artwork.find("id=\"paint\"");if(at!=std::string::npos)artwork.replace(at,10,"id=\"shape\"");
    return "<svg viewBox=\"0 0 300 200\">"+artwork+"<defs>"+gradient+"</defs></svg>";
}
inline void run() {
    const auto svg=fixture(definition("translate(5 7) scale(2 3)"),R"svg(<g transform="translate(100 50)"><rect id="shape" width="80" height="40" transform="matrix(2 0 .5 3 7 11)" fill="url(#paint)" fill-opacity=".4"/></g>)svg");
    QTemporaryDir temp;require(temp.isValid(),"Gradient transform scratch");const auto path=temp.path()+"/transform.svg";write(path,svg);
    Host host(temp.path()+"/recovery");const auto before=host.session.document();const auto revision=host.session.revision();const auto history=host.session.history();
    host.import_svg(path,before.compositions.front().id,"affine","Affine",0,0,revision);
    const auto accepted=host.session.document();const auto& object=named(accepted,"shape");const auto& gradient=*object.stack[0].gradient;
    gradient_equal(gradient,{5,7,5+360.0/13,7+240.0/13});near(object.stack[0].parameters.at("a").literal,.4);
    for(const auto& [point,t]:std::vector<std::pair<Vec2,double>>{{{5,7},0},{{25,7},.5},{{5,37},.5},{{25,37},1}})near(parameter(gradient,point),t);
    const auto world=evaluate_transforms(accepted,evaluate(accepted)).at(object.id).world;
    const auto start=map_point(world,{5,7}),end=map_point(world,{gradient.end_x.literal,gradient.end_y.literal});near(start.x,120.5);near(start.y,82);near(end.x,120.5+840.0/13);near(end.y,82+720.0/13);
    require(bytes(path)==QByteArray::fromStdString(svg),"Transformed SVG source unchanged");require(host.session.revision()==revision+1&&host.session.history().states.size()==history.states.size()+1,"Transformed gradient is one import transaction");
    require(decode(encode(accepted))==accepted,"Baked affine gradient native roundtrip");host.session.undo(host.session.revision());require(host.session.document()==before,"One Undo removes transformed gradient artwork");host.session.redo(host.session.revision());require(host.session.document()==accepted,"Transformed gradient Redo exact state");
    host.session.apply({Set{gradient_ref(object.id,object.stack[0].id,gradient.id,"end_x"),50}},host.session.revision());near(named(host.session.document(),"shape").stack[0].gradient->end_x.literal,50);host.session.undo(host.session.revision());require(host.session.document()==accepted,"Baked endpoints remain editable");
    std::cout<<"PASS independent nonuniform scale paint planes, ancestor/object transform once, alpha, source bytes, editable endpoints and one Undo/Redo\n";

    for(const auto& transform:{"translate(4 6) rotate(90) skewX(45)","matrix(0 1 -1 1 4 6)"}) {
        Session session(empty_document("shear-doc","comp","art"));session.apply(read_svg(fixture(definition(transform,"x1=\"0\" y1=\"0\" x2=\"10\" y2=\"0\"")),"comp","shear","Shear",0,0).commands,0);
        const auto& g=*named(session.document(),"shape").stack[0].gradient;gradient_equal(g,{4,6,9,11});near(parameter(g,{4,16}),1);near(parameter(g,{-6,16}),0);near(parameter(g,{2,13}),.5);
    }
    Session reflection(empty_document("reflection-doc","comp","art"));reflection.apply(read_svg(fixture(definition("scale(-2 3)")),"comp","reflection","Reflection",0,0).commands,0);
    const auto& reflected=*named(reflection.document(),"shape").stack[0].gradient;gradient_equal(reflected,{0,0,-360.0/13,240.0/13});near(parameter(reflected,{-20,0}),.5);near(parameter(reflected,{0,30}),.5);near(parameter(reflected,{-20,30}),1);
    Session skew(empty_document("skew-doc","comp","art"));skew.apply(read_svg(fixture(definition("skewY(45)","x1=\"0\" y1=\"0\" x2=\"10\" y2=\"0\"")),"comp","skew","Skew",0,0).commands,0);const auto& skewed=*named(skew.document(),"shape").stack[0].gradient;gradient_equal(skewed,{0,0,10,0});near(parameter(skewed,{5,25}),.5);
    Session bbox(empty_document("bbox-doc","comp","art"));bbox.apply(read_svg(fixture(definition("matrix(1 0 1 1 .25 .5)","x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\"","objectBoundingBox"),R"svg(<rect id="shape" x="10" y="20" width="80" height="40" fill="url(#paint)"/>)svg"),"comp","bbox","Bbox",0,0).commands,0);
    const auto& boxed=*named(bbox.document(),"shape").stack[0].gradient;gradient_equal(boxed,{30,40,46,8});near(parameter(boxed,{110,40}),1);near(parameter(boxed,{110,80}),0);near(parameter(boxed,{90,50}),.5);
    for(const auto& transform:{"","matrix(1 0 0 1 0 0)"}) {Session session(empty_document("identity-doc","comp","art"));session.apply(read_svg(fixture(definition(transform)),"comp","identity","Identity",0,0).commands,0);gradient_equal(*named(session.document(),"shape").stack[0].gradient,{0,0,10,10});}
    std::cout<<"PASS matrix/list shear and rotation, reflection, skewY, bbox-before-transform composition and unchanged no-transform/identity behavior\n";

    std::vector<std::string> invalid;
    for(const auto& transform:{"scale(0)","matrix(1 2 2 4 0 0)","matrix(1 0 1 .000000000001 0 0)","scale(.00000000001 1)","skewX(90)","rotate(1 2 3 4)","matrix(1 0 0 1 1e309 0)"})invalid.push_back(fixture(definition(transform)));
    invalid.push_back(fixture("<radialGradient id=\"paint\" gradientUnits=\"userSpaceOnUse\" cx=\"0\" cy=\"0\" r=\"10\" gradientTransform=\"matrix(1 0 0 1 0 0)\">"+svg_linear_gradient_tests::stops()+"</radialGradient>"));
    invalid.push_back(fixture(definition("scale(1)","x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\"","objectBoundingBox"),R"svg(<rect width="1" height=".00000000001" fill="url(#paint)"/>)svg"));
    host.save(temp.path()+"/existing.nect");host.recover();const auto native=temp.path()+"/existing.nect",recovery=host.persistence().value("recovery_file").toString();const auto saved=bytes(native),recovered=bytes(recovery);const auto document=host.session.document();const auto saved_history=host.session.history();const auto current=host.session.revision();const auto reject_path=temp.path()+"/reject.svg";
    for(const auto& rejected_svg:invalid) {
        write(reject_path,rejected_svg);bool rejected=false;try{host.import_svg(reject_path,document.compositions.front().id,"reject","Reject",0,0,current);}catch(const Error& error){require(error.code=="SVG_UNSUPPORTED"||error.code=="SVG_RANGE","Explicit affine gradient refusal");rejected=true;}
        require(rejected&&host.session.document()==document&&host.session.history()==saved_history&&host.session.revision()==current,"Singular/unsupported gradient import refuses atomically");require(bytes(native)==saved&&bytes(recovery)==recovered&&bytes(reject_path)==QByteArray::fromStdString(rejected_svg),"Affine gradient refusal preserves native/recovery/source bytes");
    }
    std::cout<<"PASS 9 singular/ill-conditioned/malformed/radial refusals through atomic Host import\n";
}
}
