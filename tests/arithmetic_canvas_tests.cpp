#include "canvas.hpp"
#include "window.hpp"
#include "host.hpp"
#include "nect/blend.hpp"
#include "fixtures/arithmetic-fixed-pixels.hpp"
#include <QApplication>
#include <QColorSpace>
#include <QComboBox>
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool v,const std::string& why){if(!v)throw std::runtime_error(why);++checks;}
using Pixel=std::array<unsigned,4>;
constexpr std::array<const char*,10> modes{"linear-burn","linear-dodge","linear-light","vivid-light","pin-light","hard-mix","subtract","divide","darker-color","lighter-color"};
constexpr Pixel backdrop{51,102,153,255},source{204,51,102,255},alpha_backdrop{20,80,100,128},alpha_source{120,40,10,160};
// Frozen independent oracle literals, not expectations computed by the renderer.
// DestinationIn stages opaque (204,51,102,255) through coverage 128 to
// (102,26,51,128); the arithmetic compositor then applies opacity 1/2.
constexpr std::array<Pixel,10> masked_half{{
 {38,76,115,255},{102,115,179,255},{89,76,140,255},{70,76,146,255},{76,102,153,255},
 {38,76,115,255},{38,89,128,255},{54,140,179,255},{51,102,153,255},{89,89,140,255}}};
// Red128 followed by Blue128 completes to (64,0,128,192). Default two-level
// postchildren Posterize makes (0,0,192,192), then Alpha128 makes (0,0,96,96).
// These literals are independent oracle results for that source at opacity 1/2.
constexpr std::array<Pixel,10> effected_masked_half{{
 {41,83,153,255},{51,102,172,255},{41,83,172,255},{41,83,172,255},{41,83,172,255},
 {41,83,172,255},{51,102,124,255},{89,131,153,255},{41,83,172,255},{51,102,153,255}}};
Object rect(Id id,double x,double y,double width,double height,QColor color){
 Object o;o.id=id;o.name=id;Contour c;c.id=id+"-contour";c.closed=true;
 for(const auto xy:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}){Point p;p.id=id+"-p"+std::to_string(c.points.size());p.x.literal=xy.x;p.y.literal=xy.y;c.points.push_back(p);}
 o.contours.push_back(c);auto f=default_operation(id+"-fill","nect.paint.fill");
 f.parameters.at("r").literal=color.redF();f.parameters.at("g").literal=color.greenF();f.parameters.at("b").literal=color.blueF();f.parameters.at("a").literal=color.alphaF();o.stack.push_back(f);return o;
}
Document empty_fixture(){auto d=empty_document("doc","comp","board");auto& b=d.compositions.front().artboards.front();b.width=64;b.height=64;return d;}
void add(Document& d,Object o){d.compositions.front().roots.push_back(o.id);d.objects.emplace(o.id,std::move(o));}
void group(Document& d,Id id,std::vector<Id> children){Object o;o.id=id;o.name=id;o.kind=Kind::group;o.children=std::move(children);auto& roots=d.compositions.front().roots;for(const auto& child:o.children)roots.erase(std::remove(roots.begin(),roots.end(),child),roots.end());add(d,std::move(o));}
Document fixture(bool alpha=false){auto d=empty_fixture();add(d,rect("backdrop",0,0,64,64,alpha?QColor(40,160,200,128):QColor(51,102,153)));add(d,rect("source",8,8,32,32,alpha?QColor(192,64,16,160):QColor(204,51,102)));return d;}
void pixel(const QImage& im,const Pixel& expected,const std::string& why,int x=20,int y=20){const auto p=im.pixel(x,y);const Pixel actual{unsigned(qRed(p)),unsigned(qGreen(p)),unsigned(qBlue(p)),unsigned(qAlpha(p))};check(actual==expected,why+" actual="+std::to_string(actual[0])+","+std::to_string(actual[1])+","+std::to_string(actual[2])+","+std::to_string(actual[3]));}
QImage image(const Document& d){auto im=Canvas::render_artboard(d,"comp","board",1,false);check(im.format()==QImage::Format_ARGB32_Premultiplied&&im.colorSpace()==QColorSpace::SRgb,"Production shared premultiplied sRGB image");return im;}
const Pixel& fixed(const char* mode,const char* name,double opacity){for(const auto& f:nect_r09_reference::pixel_fixtures)if(f.mode==mode&&f.name==name&&f.opacity==opacity){check(true,"Independent fixture exists");return f.binary64_expected;}throw std::runtime_error(std::string("Missing independent fixture: ")+mode+"/"+name);}
Document mask_fixture(const char* mode){auto d=fixture();const bool geometry=std::string_view(mode)=="geometry";add(d,rect("coverage",8,8,24,32,geometry?QColor(Qt::black):std::string_view(mode)=="alpha"?QColor(255,255,255,128):QColor(128,128,128)));d.objects.at("coverage").visible=false;if(geometry)d.objects.at("coverage").compositing.opacity.literal=0;GeometryMask mask{"mask","coverage"};mask.mode=mode;d.objects.at("source").compositing.mask=mask;return d;}
Document effect_fixture(){auto d=empty_fixture();add(d,rect("backdrop",0,0,64,64,QColor(51,102,153)));add(d,rect("red",8,8,24,32,QColor(255,0,0,128)));add(d,rect("blue",16,8,24,32,QColor(0,0,255,128)));group(d,"effect-group",{"red","blue"});return d;}
void staging(){
 auto d=empty_fixture();add(d,rect("backdrop",0,0,32,64,QColor(40,160,200,128)));add(d,rect("source",32,0,32,64,QColor(192,64,16,160)));const auto alpha=image(d);pixel(alpha,alpha_backdrop,"Independent unequal-alpha backdrop staging bytes");pixel(alpha,alpha_source,"Independent unequal-alpha source staging bytes",40,20);
 auto masked=mask_fixture("alpha");masked.objects.erase("backdrop");masked.compositions.front().roots={"source","coverage"};pixel(image(masked),{102,26,51,128},"Alpha coverage stages source bytes before blend and opacity");
 auto effect=effect_fixture();effect.objects.erase("backdrop");effect.compositions.front().roots={"effect-group"};pixel(image(effect),{64,0,128,192},"Children composite before Group pixel effects");effect.objects.at("effect-group").stack.push_back(default_operation("posterize","nect.group.posterize"));pixel(image(effect),{0,0,192,192},"Postchildren Posterize preserves aggregate alpha");add(effect,rect("coverage",8,8,24,32,QColor(255,255,255,128)));effect.objects.at("coverage").visible=false;GeometryMask mask{"effect-mask","coverage"};mask.mode="alpha";effect.objects.at("effect-group").compositing.mask=mask;pixel(image(effect),{0,0,96,96},"Group effect then mask fixed staging bytes");
}
void scopes(const char* mode){
 auto pass=fixture();auto& board=pass.compositions.front().artboards.front();board.x=7;board.y=11;pass.objects.at("source").compositing.blend=mode;group(pass,"neutral",{"source"});const auto normal=image(pass);pixel(normal,fixed(mode,"opaque-unequal",1),std::string(mode)+" cropped neutral Group reaches outer backdrop",13,9);pixel(normal,backdrop,"Cropped neutral Group preserves exterior",43,39);
 pass.objects.at("neutral").compositing.isolated=true;const auto isolated=image(pass);pixel(isolated,source,std::string(mode)+" explicit cropped Group excludes outer backdrop",13,9);pixel(isolated,backdrop,"Isolated cropped Group preserves exterior",43,39);
 auto nested=empty_fixture();auto& crop=nested.compositions.front().artboards.front();crop.x=7;crop.y=11;add(nested,rect("outer",0,0,64,64,Qt::yellow));add(nested,rect("local",8,8,40,40,QColor(51,102,153)));add(nested,rect("source",12,12,28,28,QColor(204,51,102)));nested.objects.at("source").compositing.blend=mode;group(nested,"inner",{"local","source"});nested.objects.at("inner").compositing.isolated=true;group(nested,"parent",{"inner"});nested.objects.at("parent").compositing.blend="multiply";add(nested,rect("later",48,48,8,8,Qt::green));const auto result=image(nested);const auto& expected=fixed(mode,"opaque-unequal",1);pixel(result,{expected[0],expected[1],0,255},std::string(mode)+" nested crop blends local backdrop before legacy parent",13,9);pixel(result,{0,255,0,255},"Later normal sibling survives isolated arithmetic and legacy scopes",43,39);pixel(result,{255,255,0,255},"Nested crop preserves outer exterior",53,49);
}
void masks_and_effects(std::size_t i){
 const auto mode=modes[i];for(const auto mask_mode:{"geometry","alpha","luma"}){auto d=mask_fixture(mask_mode);auto& c=d.objects.at("source").compositing;c.blend=mode;c.opacity.literal=.5;const auto result=image(d);pixel(result,std::string_view(mask_mode)=="geometry"?fixed(mode,"opaque-unequal",.5):masked_half[i],std::string(mode)+" "+mask_mode+" mask then opacity then arithmetic blend");pixel(result,backdrop,"Masked crop preserves uncovered source interior",36,20);}
 auto d=effect_fixture();auto& g=d.objects.at("effect-group");g.stack.push_back(default_operation("posterize","nect.group.posterize"));g.compositing.blend=mode;g.compositing.opacity.literal=.5;add(d,rect("coverage",8,8,24,32,QColor(255,255,255,128)));d.objects.at("coverage").visible=false;GeometryMask mask{"effect-mask","coverage"};mask.mode="alpha";g.compositing.mask=mask;const auto result=image(d);pixel(result,effected_masked_half[i],std::string(mode)+" completed Group effect then Alpha mask then opacity then blend");pixel(result,backdrop,"Effect Group mask crop preserves uncovered child",36,20);
}
}
int main(int argc,char** argv){if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
 QTemporaryDir dir;check(dir.isValid(),"Owned test namespace");staging();Window w(dir.path()+"/recovery");
 for(std::size_t i=0;i<modes.size();++i){const auto mode=modes[i];
  for(const double opacity:{0.,.5,1.})for(const bool alpha:{false,true}){
   auto d=fixture(alpha);d.objects.at("source").compositing.blend=mode;d.objects.at("source").compositing.opacity.literal=opacity;
   const auto im=image(d);pixel(im,fixed(mode,alpha?"unequal-alpha":"opaque-unequal",opacity),std::string(mode)+(alpha?" unequal-alpha":" opaque")+" actual Canvas independent interior");pixel(im,alpha?alpha_backdrop:backdrop,"Cropped source preserves exterior backdrop",50,50);
  }
  scopes(mode);masks_and_effects(i);
  w.host.session=Session(fixture());w.host.edited();w.show();QApplication::processEvents();w.canvas->set_selection("source");w.refresh();QApplication::processEvents();
  QComboBox* c=nullptr;for(auto* candidate:w.findChildren<QComboBox*>())if(candidate->isVisible()&&candidate->objectName()=="object-blend")c=candidate;check(c&&c->count()==26,"All26 choices reachable in Inspector");const auto index=c->findData(QString::fromLatin1(mode));check(index>=0,"Exact new ID in Inspector data");const auto old=w.host.session.document();const auto revision=w.host.session.revision();c->setCurrentIndex(index);QApplication::processEvents();
  check(w.host.session.document().objects.at("source").compositing.blend==mode&&w.host.session.revision()==revision+1,"Qt control authors exact ID through canonical Session");
  const auto changed=w.host.session.document();w.host.session.undo(w.host.session.revision());check(w.host.session.document()==old,"One Undo exact");w.host.session.redo(w.host.session.revision());check(w.host.session.document()==changed,"Redo exact");
  const auto native=encode(w.host.session.document());const auto path=dir.filePath(QString::fromLatin1(mode)+".nect");w.host.save(path);w.host.open(path);check(encode(w.host.session.document())==native,"Host native save/reopen exact");
  const auto expected=Canvas::render_artboard(w.host.session.document(),"comp","board",1,false);const auto png=dir.filePath(QString::fromLatin1(mode)+".png");w.host.export_png(png,"comp","board",1,false,w.host.session.revision());QImage decoded(png);decoded=decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);check(decoded.size()==expected.size(),"PNG dimensions preserved");bool all=true;for(int y=0;y<expected.height();++y)for(int x=0;x<expected.width();++x)all=all&&decoded.pixel(x,y)==expected.pixel(x,y);check(all,"PNG and Canvas identical pixels");
  bool refused=false;try{(void)export_svg(w.host.session.document(),"comp","board");}catch(const Error& e){check(e.code=="UNSUPPORTED_SVG_BLEND","Honest native SVG refusal");refused=true;}check(refused&&encode(w.host.session.document())==native,"Unsupported SVG does not mutate authored source");
 }
 std::cout<<"PASS "<<checks<<" arithmetic Canvas/Inspector/Host/native/PNG checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
