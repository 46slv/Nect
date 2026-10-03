#pragma once
#include "svg_import.hpp"
#include "host.hpp"
#include <QFile>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>

namespace svg_rgb_tests {
using namespace nect;
using namespace nect::desktop;
inline void require(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
inline void near(double actual,double expected,double tolerance=1e-13) {require(std::abs(actual-expected)<tolerance,"RGB/alpha precision mismatch");}
inline QByteArray bytes(const QString& path) {
    QFile file(path);require(file.open(QIODevice::ReadOnly),"Read SVG test artifact");return file.readAll();
}
inline void write(const QString& path,const std::string& value) {
    QFile file(path);require(file.open(QIODevice::WriteOnly),"Write SVG test artifact");
    require(file.write(value.data(),static_cast<qint64>(value.size()))==static_cast<qint64>(value.size()),"Write complete SVG test artifact");
}
inline std::string artwork(const std::string& color) {
    return "<svg viewBox=\"0 0 20 20\"><path d=\"M0 0L10 10\" fill=\""+color+"\" stroke=\""+color+"\" fill-opacity=\".8\" stroke-opacity=\".4\"/></svg>";
}
inline void color(const ShapeOperation& operation,const std::array<double,4>& expected,double opacity=1,double tolerance=1e-13) {
    for(std::size_t i=0;i<4;++i)near(operation.parameters.at(std::array<const char*,4>{"r","g","b","a"}[i]).literal,expected[i]*(i==3?opacity:1),tolerance);
}
inline void roundtrip(bool alpha_hex=false) {
    Session source(empty_document("source-doc","comp","art"));
    source.apply(read_svg(artwork(alpha_hex?"#1A3c":"#204060"),"comp","source","Source",0,0).commands,0);
    const std::array<double,4> fill=alpha_hex?std::array<double,4>{1.0/15,10.0/15,3.0/15,.64}:std::array<double,4>{.123456789012345,1e-9,.75,.4};
    const std::array<double,4> stroke=alpha_hex?std::array<double,4>{1.0/15,10.0/15,3.0/15,.32}:std::array<double,4>{.2,.6,.9,.7};
    std::vector<Command> commands;
    for(const bool is_stroke:{false,true}) {
        const auto& expected=is_stroke?stroke:fill;const auto op=is_stroke?"source-n1-paint-stroke":"source-n1-paint-fill";
        for(std::size_t i=0;i<4;++i)commands.push_back(Set{operation_ref("source-n1",op,std::array<const char*,4>{"r","g","b","a"}[i]),expected[i]});
    }
    if(!alpha_hex)source.apply(commands,source.revision());
    color(source.document().objects.at("source-n1").stack[0],fill);color(source.document().objects.at("source-n1").stack[1],stroke);
    const auto native_source=encode(source.document());
    const auto exported=export_svg(source.document(),"comp","art");
    require(exported.find("rgb(")!=std::string::npos&&exported.find("%)")!=std::string::npos,"Real Nect exporter must emit percentage RGB");
    QTemporaryDir temp;require(temp.isValid(),"Allocate roundtrip scratch");const auto path=temp.path()+"/export.svg";write(path,exported);
    Host destination(temp.path()+"/recovery");const auto composition=destination.session.document().compositions.front().id;
    const auto before=destination.session.document();const auto history_before=destination.session.history();const auto revision=destination.session.revision();
    const auto result=destination.import_svg(path,composition,"roundtrip","Roundtrip",0,0,revision);
    require(result.value("paths").toInt()==2,"Exported fill and stroke must both import");
    require(destination.session.revision()==revision+1&&destination.session.history().states.size()==history_before.states.size()+1,"Import must be one transaction");
    std::size_t fills=0,strokes=0;
    for(const auto& [id,object]:destination.session.document().objects)for(const auto& operation:object.stack) {
        (void)id;
        if(operation.type=="nect.paint.fill"){color(operation,fill);++fills;}
        if(operation.type=="nect.paint.stroke"){color(operation,stroke);++strokes;}
    }
    require(fills==1&&strokes==1,"Imported paint count mismatch");
    require(bytes(path)==QByteArray::fromStdString(exported)&&encode(source.document())==native_source,"Roundtrip must preserve SVG and native source bytes");
    const auto accepted=destination.session.document();require(decode(encode(accepted))==accepted,"Imported paint native roundtrip");
    destination.session.undo(revision+1);require(destination.session.document()==before,"One Undo must remove entire SVG import");
    destination.session.redo(revision+2);require(destination.session.document()==accepted,"Redo must restore exact imported paint");
    std::cout<<(alpha_hex?"PASS CSS alpha HEX exact numeric oracle and exporter/importer roundtrip, source bytes, transaction, Undo/Redo\n":"PASS real exporter RGB roundtrip, source bytes, transaction, Undo/Redo\n");
}
inline void run() {
    roundtrip();
    roundtrip(true);
    {
        QTemporaryDir temp;require(temp.isValid(),"Allocate modern color scratch");
        const auto svg=std::string(R"svg(<svg viewBox="0 0 20 20"><path d="M0 0L10 10" fill="TRANSPARENT" stroke="transparent"/><path d="M0 0L10 10" fill="rgb(12.5% 64 75%/50%)" stroke="rgba(32 25% 192 / .25)" fill-opacity=".8" stroke-opacity=".4"/></svg>)svg");
        const auto path=temp.path()+"/modern.svg";write(path,svg);Host host(temp.path()+"/recovery");
        const auto before=host.session.document();const auto history_before=host.session.history();const auto revision=host.session.revision();
        host.import_svg(path,before.compositions.front().id,"modern","Modern",0,0,revision);
        const auto& clear=host.session.document().objects.at("modern-n1").stack;const auto& paint=host.session.document().objects.at("modern-n2").stack;
        require(clear.size()==2&&paint.size()==2,"Modern/transparent paint count");color(clear[0],{0,0,0,0});color(clear[1],{0,0,0,0});
        color(paint[0],{.125,64.0/255,.75,.4});color(paint[1],{32.0/255,.25,192.0/255,.1});
        require(bytes(path)==QByteArray::fromStdString(svg),"Modern SVG source bytes must remain unchanged");
        require(host.session.revision()==revision+1&&host.session.history().states.size()==history_before.states.size()+1,"Modern import must be one transaction");
        host.session.undo(revision+1);require(host.session.document()==before,"One Undo must remove modern/transparent import");
        std::cout<<"PASS modern RGB/transparent Host numeric oracle, alpha multiplication, source bytes and one Undo\n";
    }
    struct Case {const char* input;std::array<double,4> expected;};
    const Case cases[]{
        {"rgb(12.5%,25%,75%)",{.125,.25,.75,1}},
        {"rgb(32,64,192)",{32.0/255,64.0/255,192.0/255,1}},
        {"rgba(12.5%,25%,75%,.5)",{.125,.25,.75,.5}},
        {"rgba(32,64,192,25%)",{32.0/255,64.0/255,192.0/255,.25}},
        {"RGB( +1.25e1% , 2.5E1% , .75e2% )",{.125,.25,.75,1}},
        {"rgba(0,128.5,255)",{0,128.5/255,1,1}},
        {"rgb(0,0,0,50%)",{0,0,0,.5}},
        {"rgba(-20,300,127.5,2)",{0,1,.5,1}},
        {"rgba(-10%,120%,50%,-5%)",{0,1,.5,0}},
        {"rgba(0%,100%,0%,100%)",{0,1,0,1}},
        {"#abc",{170.0/255,187.0/255,204.0/255,1}},
        {"#1A3c",{1.0/15,10.0/15,3.0/15,12.0/15}},
        {"#12345678",{18.0/255,52.0/255,86.0/255,120.0/255}},
        {"#abc0",{10.0/15,11.0/15,12.0/15,0}},
        {"#12AB34ff",{18.0/255,171.0/255,52.0/255,1}},
        {"transparent",{0,0,0,0}},
        {"TRANSPARENT",{0,0,0,0}},
        {"rgb(12.5% 64 75%/.5)",{.125,64.0/255,.75,.5}},
        {"rgba(32 25% 192 / 25%)",{32.0/255,.25,192.0/255,.25}},
        {"rgb(0 128.5 255)",{0,128.5/255,1,1}},
        {"rgba(0% 100% 0%)",{0,1,0,1}},
        {"RGB( +1.25e1% 2.5E1% .75e2% / +5e-1)",{.125,.25,.75,.5}},
        {"red",{1,0,0,1}}
    };
    for(const auto& item:cases) {
        Session session(empty_document("case-doc","comp","art"));session.apply(read_svg(artwork(item.input),"comp","case","Case",0,0).commands,0);
        const auto& stack=session.document().objects.at("case-n1").stack;require(stack.size()==2,"RGB fill/stroke count");
        // Existing named/HEX intake uses QColor's float channel accessors;
        // numeric functional and CSS alpha HEX colors retain double precision.
        const auto length=std::string_view(item.input).size();
        const auto tolerance=item.input[0]=='#'&&(length==4||length==7)?1e-7:1e-13;
        color(stack[0],item.expected,.8,tolerance);color(stack[1],item.expected,.4,tolerance);
    }
    const auto alpha=read_svg(R"svg(<svg viewBox="0 0 20 20" opacity=".25"><g fill="rgba(10%,20%,30%,.5)" fill-opacity=".4" stroke="rgba(4,5,6,25%)" stroke-opacity=".8" opacity=".6"><path d="M0 0L10 10" opacity=".7"/><path d="M0 0L10 10" fill-opacity=".2" stroke-opacity=".4" style="fill:rgba(50%,25%,75%,.8)"/></g></svg>)svg","comp","alpha","Alpha",0,0);
    Session inherited(empty_document("alpha-doc","comp","art"));inherited.apply(alpha.commands,0);
    const auto& first=inherited.document().objects.at("alpha-n2").stack;const auto& second=inherited.document().objects.at("alpha-n3").stack;
    color(first[0],{.1,.2,.3,.2});color(first[1],{4.0/255,5.0/255,6.0/255,.2});
    color(second[0],{.5,.25,.75,.16});color(second[1],{4.0/255,5.0/255,6.0/255,.1});
    const auto values=evaluate(inherited.document());near(values.at({"alpha","","composite.opacity"}),.25);near(values.at({"alpha-n1","","composite.opacity"}),.6);near(values.at({"alpha-n2","","composite.opacity"}),.7);
    std::cout<<"PASS 23 numeric/named/HEX/transparent cases and inherited/inline color alpha with separate object/Group opacity\n";
    const char* invalid[]{
        "rgb()","rgb(1,2)","rgb(1,2,3,4,5)","rgb(,2,3)","rgb(1,,3)","rgb(1,2,3,)","rgb(1,2,3)tail",
        "rgb(1,2,3))","rgb(1,2,3","rgb(1. 2,3,4)","rgb(1.,2,3)","rgb(1% ,2,3%)","rgb(1,2%,3)",
        "rgb(1 2)","rgba(1,2,3 / .5)","rgb(1px,2,3)","rgb(1e,2,3)","rgb(NaN,2,3)","rgb(infinity,2,3)",
        "rgb(calc(1),2,3)","rgb(var(--red),2,3)","rgb(from red r g b)","rgb(none,2,3)","hsl(0,100%,50%)",
        "color(srgb 1 0 0)","url(#paint)","rgb(1,2,3)!important","#12345","#1234567","currentColor",
        "rgb(1 2,3)","rgb(1 2 3 /)","rgb(1 2 3 .5)","rgb(1 2 3 / .5 / .6)","rgb(1/**/ 2 3)"
    };
    for(const auto* input:invalid) {
        bool rejected=false;try{(void)read_svg(artwork(input),"comp","bad","Bad",0,0);}catch(const Error& error){require(error.code=="SVG_UNSUPPORTED","Malformed RGB reject code");rejected=true;}
        require(rejected,"Unsupported RGB must reject");
    }
    for(const auto* input:{"rgb(1e309,0,0)","rgba(1,2,3,1e309)","rgb(10000001,0,0)","rgb(-10000001%,0%,0%)",
        "rgb(256 0 0)","rgb(-.1 0 0)","rgb(100.1% 0% 0%)","rgba(0 0 0 / 1.01)","rgba(0 0 0 / -1%)","rgb(1e309 0 0)"}) {
        bool rejected=false;try{(void)read_svg(artwork(input),"comp","bad","Bad",0,0);}catch(const Error& error){require(error.code=="SVG_RANGE","RGB range reject code");rejected=true;}
        require(rejected,"Unbounded RGB must reject");
    }
    QTemporaryDir temp;require(temp.isValid(),"Allocate atomic refusal scratch");Host host(temp.path()+"/recovery");
    const auto composition=host.session.document().compositions.front().id;
    host.import_svg([&]{const auto path=temp.path()+"/seed.svg";write(path,artwork("#123"));return path;}(),composition,"seed","Seed",0,0,host.session.revision());
    const auto native=temp.path()+"/existing.nect";host.save(native);host.recover();
    const auto recovery=host.persistence().value("recovery_file").toString();
    const auto native_before=bytes(native),recovery_before=bytes(recovery);const auto document_before=host.session.document();const auto history_before=host.session.history();const auto revision=host.session.revision();
    for(const auto* input:{"rgb(1,2%,3)","rgb(1e309,0,0)","rgb(1,2,3)tail","url(#paint)","rgb(1 2,3)","#12345","#1234567","#12g4","#1234567z"}) {
        const auto svg="<svg viewBox=\"0 0 20 20\"><path d=\"M0 0L1 1\" fill=\"rgb(10%,20%,30%)\"/><path d=\"M0 0L2 2\" fill=\""+std::string(input)+"\"/></svg>";
        const auto path=temp.path()+"/reject.svg";write(path,svg);bool rejected=false;
        try{host.import_svg(path,composition,"reject","Reject",0,0,revision);}catch(const Error& error){require(error.code==(std::string(input).find("1e309")!=std::string::npos?"SVG_RANGE":"SVG_UNSUPPORTED"),"Host RGB reject code");rejected=true;}
        require(rejected,"Late malformed paint must reject entire import");
        require(host.session.document()==document_before&&host.session.history()==history_before&&host.session.revision()==revision,"Rejected import must preserve document/history/revision");
        require(bytes(native)==native_before&&bytes(recovery)==recovery_before&&bytes(path)==QByteArray::fromStdString(svg),"Rejected import must preserve native/recovery/SVG source bytes");
    }
    host.session.undo(revision);require(host.session.document().objects.empty(),"Refusal must preserve the preceding Undo entry");
    std::cout<<"PASS 35 unsupported, 10 bounded-range refusals and 9 late Host atomic refusals\n";
}
}
