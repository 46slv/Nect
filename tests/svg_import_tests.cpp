#include "svg_import.hpp"
#include "window.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QAction>
#include <QDialog>
#include <QTimer>
#include <QJsonDocument>
#include "nect/io.hpp"
#include <cmath>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void near(double a,double b){check(std::abs(a-b)<1e-7,"Unexpected imported coordinate");}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try {
    const std::string svg=R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100" viewBox="10 20 100 100"><title>Original fixture</title><g id="mark" fill="#e04020" transform="translate(10,5)"><path id="outline" d="M10 20h30v20h-30z"/><path d="M0 0q3 6 9 0t9 0" style="fill:none;stroke:#123;stroke-width:2"/></g></svg>)svg";
    auto plan=read_svg(svg,"comp","asset","Artwork",20,30);Session s(empty_document("doc","comp","art"));const auto before=s.document();s.apply(plan.commands,0);
    check(plan.paths==2&&plan.root=="asset"&&s.document().objects.at("asset").kind==Kind::group,"SVG becomes editable Group");
    const auto values=evaluate(s.document());const auto transforms=evaluate_transforms(s.document(),values);
    const auto& p=s.document().objects.at("asset-n2");check(p.contours[0].closed&&p.contours[0].points.size()==4,"H/V/relative/close path topology");
    const auto world=map_point(transforms.at(p.id).world,{10,20});near(world.x,80);near(world.y,35);
    near(p.stack[0].parameters.at("r").literal,224.0/255);check(p.stack[0].type=="nect.paint.fill","Inherited solid fill");
    const auto& curve=s.document().objects.at("asset-n3");check(curve.contours[0].points.size()==3&&curve.stack.size()==1,"Quadratic shorthand lowering and no-fill stroke");
    const auto accepted=s.document();check(decode(encode(accepted))==accepted,"Imported native roundtrip");s.undo(1);check(s.document()==before,"One Undo removes complete import");s.redo(2);check(s.document()==accepted,"Redo exact import");
    auto exported=export_svg(accepted,"comp","art");check(exported.find("<path")!=std::string::npos,"Imported paths export normally");
    auto compact=read_svg(R"svg(<svg viewBox="0 0 20 20"><path d="M.5.5 10-2 C1 2 3 4 5 6s7 8 9 10z"/></svg>)svg","comp","compact","Compact",0,0);check(compact.paths==1,"Compact SVG number grammar");
    for(const auto& invalid:std::vector<std::string>{
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0A2 2 0 2 0 4 4"/></svg>)svg",
        R"svg(<!DOCTYPE svg [<!ENTITY x "boom">]><svg viewBox="0 0 10 10"><path d="M0 0"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><script/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0L1,"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0" fill="url(file:///secret)"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0" stroke-linecap="round"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0"/><image href="https://invalid/"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0" class="unknown"/></svg>)svg"}) {
        bool rejected=false;try{(void)read_svg(invalid,"comp","bad","Bad",0,0);}catch(const Error&){rejected=true;}check(rejected,"Unsupported SVG must reject entirely");
    }
    const std::string shapes=R"svg(<svg viewBox="0 0 400 300"><g fill="#456">
      <rect x="10" y="20" width="60" height="40"/>
      <rect width="100" height="40" rx="80"/>
      <circle cx="40" cy="50" r="20"/>
      <ellipse cx="90" cy="100" rx="30" ry="10"/>
      <line x2="40" y2="30" stroke="black"/>
      <polyline points="0,0 10,20 30-5"/>
      <polygon points="0 0,30 0,15 20"/>
      <ellipse rx="12" ry="auto"/>
    </g></svg>)svg";
    const auto shape_plan=read_svg(shapes,"comp","shapes","Shapes",0,0);Session shaped(empty_document("shape-doc","comp","art"));shaped.apply(shape_plan.commands,0);
    check(shape_plan.paths==8,"Every basic shape becomes one editable path");
    const auto& square=shaped.document().objects.at("shapes-n2").contours[0];check(square.closed&&square.points.size()==4,"Square rectangle topology");near(square.points[2].x.literal,70);near(square.points[2].y.literal,60);
    const auto& rounded=shaped.document().objects.at("shapes-n3").contours[0];near(rounded.points[0].x.literal,50);check(rounded.closed,"Rounded rectangle clamps copied radii");
    const auto& line=shaped.document().objects.at("shapes-n6").contours[0];check(!line.closed&&line.points.size()==2,"Line defaults and open topology");near(line.points[0].x.literal,0);near(line.points[1].y.literal,30);
    check(!shaped.document().objects.at("shapes-n7").contours[0].closed&&shaped.document().objects.at("shapes-n8").contours[0].closed,"Polyline versus polygon close semantics");
    near(shaped.document().objects.at("shapes-n9").contours[0].points[2].y.literal,12);
    // Sample every cubic in normalized ellipse coordinates; catches orientation,
    // handle direction, closure and radius drift independently of the converter.
    const auto& ellipse=shaped.document().objects.at("shapes-n5").contours[0];check(ellipse.points.size()==8&&ellipse.closed,"Ellipse eight-span topology");
    auto control=[](const Point& p,bool out){const double rad=(out?p.out_angle:p.in_angle).literal*std::acos(-1)/180,len=(out?p.out_length:p.in_length).literal;return Vec2{p.x.literal+len*std::cos(rad),p.y.literal+len*std::sin(rad)};};
    for(std::size_t i=0;i<ellipse.points.size();++i){const auto& p=ellipse.points[i];const auto& q=ellipse.points[(i+1)%ellipse.points.size()];const auto a=control(p,true),b=control(q,false);
        for(int j=0;j<=100;++j){const double t=j/100.0,u=1-t,x=u*u*u*p.x.literal+3*u*u*t*a.x+3*u*t*t*b.x+t*t*t*q.x.literal,y=u*u*u*p.y.literal+3*u*u*t*a.y+3*u*t*t*b.y+t*t*t*q.y.literal;
            check(std::abs(std::hypot((x-90)/30,(y-100)/10)-1)<5e-6,"Ellipse radial approximation bound");}}
    check(decode(encode(shaped.document()))==shaped.document(),"Basic shapes native roundtrip");shaped.undo(1);check(shaped.document().objects.empty(),"Basic shapes one atomic Undo");
    for(const auto& element:std::vector<std::string>{"<rect width='-1' height='10'/>","<circle r='0'/>","<ellipse rx='10' ry='-2'/>","<polyline points='0 0 10'/>","<polygon points='0 0 10 20,'/>","<rect width='10%' height='20'/>","<circle r='5'><rect width='2' height='2'/></circle>"}) {
        bool rejected=false;try{(void)read_svg("<svg viewBox='0 0 40 40'>"+element+"</svg>","comp","invalid-shape","Bad",0,0);}catch(const Error&){rejected=true;}check(rejected,"Invalid/unsupported shape refuses entirely");
    }
    for(bool large:{false,true})for(bool sweep:{false,true}) {
        const auto input=std::string("<svg viewBox='-100 -100 400 400'><path d='M100 0 A100 100 0 ")+(large?"1":"0")+" "+(sweep?"1":"0")+" 0 100'/></svg>";
        const auto plan=read_svg(input,"comp","arc","Arc",0,0);Session arcs(empty_document("arc-doc","comp","art"));arcs.apply(plan.commands,0);const auto& points=arcs.document().objects.at("arc-n1").contours[0].points;
        check(points.size()==(large?7:3),"Arc sweep spans and flags");const auto& mid=points[points.size()/2];const double d=std::sqrt(0.5)*100,expected=large?(sweep?100+d:-d):(sweep?d:100-d);near(mid.x.literal,expected);near(mid.y.literal,expected);near(points.back().x.literal,0);near(points.back().y.literal,100);
    }
    const auto arc_plan=read_svg(R"svg(<svg viewBox="0 0 400 300"><path d="M0 0a10 10 0 0120 0 A0 10 0 0 1 30 0 A10 10 0 1 1 30 0 S40 5 50 0"/><path d="M10 0A1 1 0 0 1-10 0"/><path d="M0 20A20 10 90 0 1 0-20"/></svg>)svg","comp","arcs","Arcs",0,0);
    Session arcs(empty_document("arc-doc","comp","art"));arcs.apply(arc_plan.commands,0);
    const auto& degenerate=arcs.document().objects.at("arcs-n1").contours[0].points;check(degenerate.size()==7,"Relative compact flags, zero radius line, coincident endpoint omission");near(degenerate[5].x.literal,30);near(degenerate[5].out_length.literal,0);
    const auto& corrected=arcs.document().objects.at("arcs-n2").contours[0].points;near(corrected[2].y.literal,10);
    const auto& rotated=arcs.document().objects.at("arcs-n3").contours[0].points;near(rotated[2].x.literal,-10);near(rotated[2].y.literal,0);
    check(decode(encode(arcs.document()))==arcs.document(),"Arc authored/native roundtrip");
    QTemporaryDir temp;const auto input=temp.path()+"/art.svg";QFile file(input);check(file.open(QIODevice::WriteOnly),"Write owned SVG fixture");file.write(QByteArray::fromStdString(svg));file.close();
    Window window(temp.path()+"/recovery");window.show();QApplication::processEvents();auto& host=window.host;
    const auto composition=host.session.document().compositions.front().id;const auto original=host.session.document();
    const auto imported=host.import_svg(input,composition,"host-import","Imported",0,0,0);check(imported["paths"].toInt()==2&&host.session.revision()==1,"Host imports one complete transaction");
    const auto native=temp.path()+"/imported.nect";host.save(native);check(QFile(native).exists(),"Native import save available");
    const auto accepted_host=host.session.document();
    { Host reopened(temp.path()+"/reopened-recovery");reopened.open(native);check(reopened.session.document()==accepted_host,"Saved SVG conversion reopens as exact native state"); }
    bool stale=false;try{host.import_svg(input,composition,"stale","Stale",0,0,0);}catch(const Error& e){stale=e.code=="REVISION_CONFLICT";}check(stale&&host.session.document()==accepted_host,"Stale file import is atomic");
    host.session.undo(1);host.edited();check(host.session.document()==original,"Host one Undo removes import");
    auto before_bad=host.session.document();check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Write unsupported fixture");file.write("<svg viewBox='0 0 10 10'><script/></svg>");file.close();
    bool unsupported=false;try{host.import_svg(input,composition,"unsupported","Bad",0,0,2);}catch(const Error& e){unsupported=e.code=="SVG_UNSUPPORTED";}check(unsupported&&host.session.document()==before_bad&&host.session.revision()==2,"Unsupported file never commits parsed fragments");
    check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Restore owned SVG fixture");file.write(QByteArray::fromStdString(svg));file.close();
    bool dialog_seen=false;QTimer::singleShot(0,[&]{auto* dialog=window.findChild<QDialog*>("svg-import-dialog");if(dialog){dialog_seen=true;dialog->findChild<QLineEdit*>("svg-import-path")->setText(input);dialog->accept();}});
    window.findChild<QAction*>("import-svg")->trigger();check(dialog_seen&&host.session.revision()==3&&window.canvas->selected_objects().size()==1,"Real Window import action/dialog selects imported Group");
    host.session.undo(3);host.edited();check(host.session.document()==original,"GUI import one Undo");
    std::cout<<"SVG memory conversion, hierarchy, styles, curves, strict refusal, native/Undo contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
