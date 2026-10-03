#pragma once
#include "svg_rgb_import_tests.hpp"

namespace svg_current_color_tests {
using namespace svg_rgb_tests;
inline const Object& named(const Document& document,const std::string& name) {
    const Object* found=nullptr;
    for(const auto& [id,object]:document.objects)if(object.name==name) {require(!found,"Unique fixture label");found=&object;}
    require(found,"Fixture object label exists");return *found;
}
inline void paints(const Object& object,const std::array<double,4>& rgba,double fill_alpha,double stroke_alpha) {
    require(object.stack.size()==2,"CurrentColor retains fill and stroke");color(object.stack[0],rgba,fill_alpha);color(object.stack[1],rgba,stroke_alpha);
    for(const auto& operation:object.stack) {
        require(!operation.gradient,"Resolved currentColor is solid paint");
        for(const auto& key:{"r","g","b","a"}) {const auto& scalar=operation.parameters.at(key);require(!scalar.binding&&!scalar.expression,"CurrentColor freezes into native literals");}
    }
}
inline std::string fixture(const std::string& attributes) {return "<svg viewBox=\"0 0 20 20\"><path id=\"paint\" d=\"M0 0L10 10\" "+attributes+"/></svg>";}
inline void run() {
    const auto svg=R"svg(<svg viewBox="0 0 200 200" color="rgba(20%,40%,60%,.5)" fill="currentColor" stroke="currentColor" fill-opacity=".8" stroke-opacity=".4" opacity=".75"><g id="parent" color="#2468" opacity=".5"><path id="inherited" d="M0 0L20 20" opacity=".25"/><g id="nested" color="inherit"><circle id="override" r="5" color="rgba(80% 10% 20% / .25)" fill-opacity=".6" opacity=".125"/><path id="self" d="M0 0L15 15" color="CuRrEnTcOlOr"/></g></g><path id="root-color" d="M0 0L30 30"/></svg>)svg";
    QTemporaryDir temp;require(temp.isValid(),"CurrentColor test scratch");const auto path=temp.path()+"/art.svg";write(path,svg);
    Host host(temp.path()+"/recovery");const auto before=host.session.document();const auto revision=host.session.revision();const auto history=host.session.history();
    host.import_svg(path,before.compositions.front().id,"current","Current",0,0,revision);
    const auto& document=host.session.document();
    paints(named(document,"inherited"),{2.0/15,4.0/15,6.0/15,8.0/15},.8,.4);
    paints(named(document,"self"),{2.0/15,4.0/15,6.0/15,8.0/15},.8,.4);
    paints(named(document,"override"),{.8,.1,.2,.25},.6,.4);
    paints(named(document,"root-color"),{.2,.4,.6,.5},.8,.4);
    const auto values=evaluate(document);near(values.at({"current","","composite.opacity"}),.75);near(values.at({named(document,"parent").id,"","composite.opacity"}),.5);near(values.at({named(document,"inherited").id,"","composite.opacity"}),.25);near(values.at({named(document,"override").id,"","composite.opacity"}),.125);
    require(host.session.revision()==revision+1&&host.session.history().states.size()==history.states.size()+1,"CurrentColor import is one atomic transaction");
    const auto accepted=document;require(decode(encode(accepted))==accepted,"Frozen currentColor native roundtrip");
    host.session.undo(host.session.revision());require(host.session.document()==before,"One Undo removes complete currentColor artwork");host.session.redo(host.session.revision());require(host.session.document()==accepted,"CurrentColor Redo exact state");
    const auto& editable=named(accepted,"override");host.session.apply({Set{operation_ref(editable.id,editable.stack[0].id,"r"),.9}},host.session.revision());near(named(host.session.document(),"override").stack[0].parameters.at("r").literal,.9);host.session.undo(host.session.revision());require(host.session.document()==accepted,"Frozen literal paint remains editable and undoable");
    require(bytes(path)==QByteArray::fromStdString(svg),"CurrentColor source bytes unchanged");
    std::cout<<"PASS inherited currentColor on svg/groups/shapes, descendant overrides, color:self/inherit, exact alpha/opacity, literal editability, source bytes and one Undo/Redo\n";

    for(const auto& attributes:{"fill=\"currentColor\" stroke=\"CURRENTCOLOR\"","color=\"currentColor\" fill=\"currentColor\" stroke=\"currentColor\""}) {
        Session session(empty_document("default-doc","comp","art"));session.apply(read_svg(fixture(attributes),"comp","default","Default",0,0).commands,0);paints(named(session.document(),"paint"),{0,0,0,1},1,1);
    }
    for(const auto& attributes:{
        "color=\"red\" fill=\"red\" stroke=\"red\" style=\"fill:currentColor;stroke:currentColor;color:rgba(10%,30%,70%,.5);fill-opacity:.8;stroke-opacity:.4\"",
        "fill=\"red\" stroke=\"red\" color=\"red\" style=\"color:rgba(10%,30%,70%,.5);stroke-opacity:.4;fill-opacity:.8;stroke:currentColor;fill:currentColor\"",
        "color=\"red\" fill=\"currentColor\" stroke=\"currentColor\" style=\"color:blue;color:rgba(10%,30%,70%,.5);fill-opacity:.8;stroke-opacity:.4\""}) {
        Session session(empty_document("style-doc","comp","art"));session.apply(read_svg(fixture(attributes),"comp","styled","Styled",0,0).commands,0);paints(named(session.document(),"paint"),{.1,.3,.7,.5},.8,.4);
    }
    Session transparent(empty_document("transparent-doc","comp","art"));transparent.apply(read_svg(fixture("color=\"transparent\" fill=\"currentColor\" stroke=\"currentColor\""),"comp","clear","Clear",0,0).commands,0);paints(named(transparent.document(),"paint"),{0,0,0,0},1,1);
    Session ordinary(empty_document("ordinary-doc","comp","art"));ordinary.apply(read_svg(fixture("color=\"blue\" stroke=\"red\""),"comp","ordinary","Ordinary",0,0).commands,0);color(named(ordinary.document(),"paint").stack[0],{0,0,0,1});color(named(ordinary.document(),"paint").stack[1],{1,0,0,1});
    std::cout<<"PASS default black, case-insensitive keyword, inline/presentation precedence, declaration order independence, transparent and unaffected ordinary paint\n";

    std::vector<std::string> invalid;
    for(const auto& value:{"var(--ink)","none","url(#paint)","hsl(0 100% 50%)","rgb(256 0 0)","currentColor()","initial",""})invalid.push_back(fixture("color=\""+std::string(value)+"\" fill=\"currentColor\""));
    for(const auto& attributes:{"style=\"color:red !important;color:blue\"","style=\"color:var(--ink);color:red\"","style=\"color:red;filter:none\"","color=\"red\" class=\"ink\"","fill=\"currentColour\""})invalid.push_back(fixture(attributes));
    auto gradient=[](const std::string& defs_extra,const std::string& gradient_extra,const std::string& stop_extra) {
        return "<svg viewBox=\"0 0 20 20\" color=\"red\"><path d=\"M0 0L10 10\" fill=\"url(#paint)\"/><defs "+defs_extra+"><linearGradient id=\"paint\" gradientUnits=\"userSpaceOnUse\" x1=\"0\" y1=\"0\" x2=\"10\" y2=\"0\" "+gradient_extra+"><stop offset=\"0\" "+stop_extra+"/><stop offset=\"1\"/></linearGradient></defs></svg>";
    };
    invalid.push_back(gradient({}, {},"stop-color=\"currentColor\""));invalid.push_back(gradient({}, {},"color=\"red\""));invalid.push_back(gradient({},"color=\"red\"",{}));invalid.push_back(gradient("color=\"red\"",{},{}));
    host.save(temp.path()+"/existing.nect");host.recover();const auto native=temp.path()+"/existing.nect",recovery=host.persistence().value("recovery_file").toString();
    const auto saved=bytes(native),recovered=bytes(recovery);const auto current_document=host.session.document();const auto current_history=host.session.history();const auto current_revision=host.session.revision();const auto reject_path=temp.path()+"/reject.svg";
    for(const auto& svg_invalid:invalid) {
        write(reject_path,svg_invalid);bool rejected=false;
        try{host.import_svg(reject_path,current_document.compositions.front().id,"reject","Reject",0,0,current_revision);}catch(const Error& error){require(error.code=="SVG_UNSUPPORTED"||error.code=="SVG_RANGE","Explicit currentColor refusal code");rejected=true;}
        require(rejected&&host.session.document()==current_document&&host.session.history()==current_history&&host.session.revision()==current_revision,"Malformed currentColor import refuses atomically");
        require(bytes(native)==saved&&bytes(recovery)==recovered&&bytes(reject_path)==QByteArray::fromStdString(svg_invalid),"CurrentColor refusal preserves native/recovery/source bytes");
    }
    host.session.undo(host.session.revision());require(host.session.document()==before,"Refusals preserve preceding single import Undo");
    std::cout<<"PASS 17 explicit malformed/unknown/style/gradient-color refusals, each atomic through production Host\n";
}
}
