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
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0A2 2 0 0 0 4 4"/></svg>)svg",
        R"svg(<!DOCTYPE svg [<!ENTITY x "boom">]><svg viewBox="0 0 10 10"><path d="M0 0"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><script/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0L1,"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0" fill="url(file:///secret)"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0" stroke-linecap="round"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0"/><image href="https://invalid/"/></svg>)svg",
        R"svg(<svg viewBox="0 0 10 10"><path d="M0 0" class="unknown"/></svg>)svg"}) {
        bool rejected=false;try{(void)read_svg(invalid,"comp","bad","Bad",0,0);}catch(const Error&){rejected=true;}check(rejected,"Unsupported SVG must reject entirely");
    }
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
