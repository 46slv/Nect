#include "flattened_svg_export.hpp"
#include "host.hpp"
#include "canvas.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QXmlStreamReader>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
QByteArray bytes(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"read output");return f.readAll();}
template<class F>void rejects(const char* code,F f){try{f();}catch(const Error& e){check(e.code==code,"typed refusal");return;}throw std::runtime_error("expected refusal");}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);try{
 QTemporaryDir temp;Host host(temp.path()+"/recovery");
 auto d=empty_document("flat-doc","comp","board");auto& b=d.compositions[0].artboards[0];
 b.x=3;b.y=5;b.width=8.25;b.height=6.25;b.background=ColorValue{"srgb","srgb","straight",{1,0,0,1}};
 Object shape;shape.id="shape";shape.name="Shape";shape.source=default_primitive("rectangle","nect.shape.rectangle");
 shape.source->parameters.at("width").literal=5;shape.source->parameters.at("height").literal=5;
 shape.transform[4].literal=5;shape.transform[5].literal=7;
 auto fill=default_operation("fill","nect.paint.fill");fill.parameters.at("r").literal=0;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=1;
 shape.stack={fill};shape.compositing.blend="hard-mix";d.objects.emplace(shape.id,shape);d.compositions[0].roots={shape.id};
 host.session=Session(d);const auto before=encode(d);const auto path=temp.path()+"/flat.svg";
 const auto result=export_flattened_svg(host,path,"comp","board",1.5,0);
 check(!result.value("editable").toBool(true)&&result.value("kind")=="flattened_svg","explicit raster loss");
 check(result.value("pixel_width").toInt()==13&&result.value("pixel_height").toInt()==10,"ceil raster size");
 QXmlStreamReader xml(bytes(path));QByteArray png;int images=0;
 while(!xml.atEnd()){xml.readNext();if(!xml.isStartElement())continue;
  if(xml.name()==u"svg")check(xml.attributes().value("viewBox")==u"0 0 8.25 6.25","document frame retained");
  if(xml.name()==u"image"){
   ++images;check(xml.attributes().value("width").toDouble()==13/1.5,"sample spacing retained");
   auto href=xml.attributes().value("http://www.w3.org/1999/xlink","href").toString();
   check(href.startsWith("data:image/png;base64,"),"self contained PNG");png=QByteArray::fromBase64(href.mid(22).toLatin1());
  }
 }
 check(!xml.hasError()&&images==1,"one valid SVG image");
 auto decoded=QImage::fromData(png,"png").convertToFormat(QImage::Format_ARGB32_Premultiplied);
 const auto expected=Canvas::render_artboard(d,"comp","board",1.5,false);
 check(decoded.size()==expected.size(),"decoded image size");
 for(int y=0;y<expected.height();++y)for(int x=0;x<expected.width();++x)check(decoded.pixel(x,y)==expected.pixel(x,y),"Canvas pixels preserved");
 check(encode(host.session.document())==before&&host.session.revision()==0&&!host.session.can_undo(),"source and history unchanged");
 const auto saved=bytes(path);
 rejects("REVISION_CONFLICT",[&]{export_flattened_svg(host,path,"comp","board",1,9);});
 host.file_path=path;rejects("EXPORT_TARGET",[&]{export_flattened_svg(host,path,"comp","board",1,0);});host.file_path.clear();
 rejects("EXPORT_TARGET",[&]{export_flattened_svg(host,temp.path()+"/bad.png","comp","board",1,0);});
 host.session.begin_gesture(0);rejects("GESTURE_ACTIVE",[&]{export_flattened_svg(host,path,"comp","board",1,0);});host.session.cancel_gesture();
 check(bytes(path)==saved,"refusals preserve existing output");
 QJsonObject request{{"op","export_flattened_svg"},{"session_id",host.session_id},{"document_id","flat-doc"},
 {"expected_revision",0},{"path",temp.path()+"/api.svg"},{"composition","comp"},{"artboard","board"},{"scale",1.5}};
 auto response=QJsonDocument::fromJson(host.dispatch(QJsonDocument(request).toJson())).object();
 check(response.value("ok").toBool(),"Host API export primary");
 request["session_id"]="stale";response=QJsonDocument::fromJson(host.dispatch(QJsonDocument(request).toJson())).object();
 check(!response.value("ok").toBool(),"Host identity guard");
 std::cout<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
