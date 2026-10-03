#include "canvas.hpp"
#include "host.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace {int checks=0;void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void pixel(const QImage& im,int x,int y,QRgb expected,const char* why){if(im.pixel(x,y)!=expected)std::cerr<<std::hex<<im.pixel(x,y)<<" != "<<expected<<std::dec<<'\n';check(im.pixel(x,y)==expected,why);}}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
 auto d=empty_document("doc","comp","board");auto& board=d.compositions.front().artboards.front();board.width=4;board.height=4;ColorValue red;red.rgba={1,0,0,.5};board.background=red;
 pixel(Canvas::render_artboard(d,"comp","board",1,false),0,0,qRgba(128,0,0,128),"Empty output contains authored half-alpha underlay");
 Object o;o.id="blue";o.name="Blue";Contour c;c.id="shape";c.closed=true;for(auto xy:std::vector<Vec2>{{1,1},{3,1},{3,3},{1,3}}){Point p;p.id="p"+std::to_string(c.points.size());p.x.literal=xy.x;p.y.literal=xy.y;c.points.push_back(p);}o.contours={c};auto fill=default_operation("fill","nect.paint.fill");fill.parameters.at("r").literal=0;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=1;fill.parameters.at("a").literal=.5;o.stack.push_back(fill);o.compositing.blend="multiply";d.objects.emplace(o.id,o);d.compositions.front().roots={o.id};
 auto output=Canvas::render_artboard(d,"comp","board",1,false);pixel(output,1,1,qRgba(64,0,128,192),"Completed artwork is over underlay, not blended against it");
 pixel(Canvas::render_artboard(d,"comp","board",1,false,false),1,1,qRgba(0,0,128,128),"Artwork-only analysis excludes underlay");
 pixel(Canvas::render_artboard(d,"comp","board",1,true),1,1,qRgba(127,63,191,255),"Optional white matte is last");
 QTemporaryDir dir;Host host(dir.path()+"/recovery");host.session=Session(d);const auto png=dir.filePath("background.png");const auto native=encode(d);const auto receipt=host.export_png(png,"comp","board",1,false,host.session.revision());
 check(receipt.contains("authored_background")&&!receipt.value("authored_background").isNull(),"PNG receipt distinguishes authored background from matte");
 QImage decoded(png);decoded=decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);bool same=decoded.size()==output.size();for(int y=0;same&&y<output.height();++y)for(int x=0;x<output.width();++x)same=same&&decoded.pixel(x,y)==output.pixel(x,y);check(same,"PNG same pixels as shared Canvas");
 check(encode(host.session.document())==native,"Export leaves exact native source unchanged");
 std::cout<<"PASS "<<checks<<" background Canvas/PNG/analysis-boundary smoke checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<checks<<": "<<e.what()<<'\n';return 1;}}
