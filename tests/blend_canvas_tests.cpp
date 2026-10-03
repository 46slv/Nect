#include "canvas.hpp"
#include "window.hpp"
#include "host.hpp"
#include "nect/blend.hpp"
#include <QApplication>
#include <QColorSpace>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QTemporaryDir>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,std::string("Expected ")+code+", got "+e.code);return;}throw std::runtime_error(std::string("Missing rejection: ")+code);}
using Pixel=PremultipliedSrgb8;
constexpr std::array<const char*,4> modes{"hue","saturation","color","luminosity"};
constexpr std::array<Pixel,4> opaque_half{{{105,80,122,255},{41,104,168,255},{122,71,122,255},{56,107,158,255}}};
constexpr std::array<Pixel,4> opaque_full{{{160,58,92,255},{30,107,183,255},{194,41,92,255},{61,112,163,255}}};
constexpr std::array<Pixel,4> alpha_half{{{77,80,80,168},{48,90,104,168},{79,80,79,168},{45,85,98,168}}};
constexpr std::array<Pixel,4> alpha_full{{{135,81,59,208},{77,101,107,208},{137,80,57,208},{70,90,95,208}}};
constexpr std::array<Pixel,4> mask_half{{{78,91,138,255},{46,103,160,255},{87,87,137,255},{54,105,156,255}}};
constexpr Pixel b{51,102,153,255},s{204,51,102,255},ab{20,80,100,128},as{120,40,10,160};
Pixel pixel(const QImage& image,int x=20,int y=20){const QRgb p=image.pixel(x,y);return {std::uint8_t(qRed(p)),std::uint8_t(qGreen(p)),std::uint8_t(qBlue(p)),std::uint8_t(qAlpha(p))};}
void exact(const QImage& image,Pixel expected,const std::string& why,int x=20,int y=20){const auto got=pixel(image,x,y);check(got==expected,why+" actual="+std::to_string(got.r)+","+std::to_string(got.g)+","+std::to_string(got.b)+","+std::to_string(got.a));}
Object rectangle(Id id,double x,double y,double width,double height,QColor color){Object o;o.id=id;o.name=id;Contour c;c.id=id+"-contour";c.closed=true;for(const auto xy:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}){Point p;p.id=id+"-p"+std::to_string(c.points.size());p.x.literal=xy.x;p.y.literal=xy.y;c.points.push_back(p);}o.contours.push_back(c);auto f=default_operation(id+"-fill","nect.paint.fill");f.parameters.at("r").literal=color.redF();f.parameters.at("g").literal=color.greenF();f.parameters.at("b").literal=color.blueF();f.parameters.at("a").literal=color.alphaF();o.stack.push_back(f);return o;}
Document base(){auto d=empty_document("doc","comp","board");auto& board=d.compositions.front().artboards.front();board.width=64;board.height=64;return d;}
void add(Document& d,Object o){d.compositions.front().roots.push_back(o.id);d.objects.emplace(o.id,std::move(o));}
void group(Document& d,Id id,std::vector<Id> children){Object o;o.id=id;o.name=id;o.kind=Kind::group;o.children=std::move(children);auto& roots=d.compositions.front().roots;for(const auto& child:o.children)roots.erase(std::remove(roots.begin(),roots.end(),child),roots.end());add(d,std::move(o));}
QImage image(const Document& d,double scale=1){auto result=Canvas::render_artboard(d,"comp","board",scale,false);check(result.format()==QImage::Format_ARGB32_Premultiplied&&result.colorSpace()==QColorSpace::SRgb,"Shared Canvas output is actual premultiplied8 sRGB");return result;}
Document pair(bool alpha=false){auto d=base();add(d,rectangle("backdrop",0,0,64,64,alpha?QColor(40,160,200,128):QColor(51,102,153)));add(d,rectangle("source",8,8,32,32,alpha?QColor(192,64,16,160):QColor(204,51,102)));return d;}
void pixels(){
    auto staging=base();add(staging,rectangle("b",0,0,32,32,QColor(40,160,200,128)));add(staging,rectangle("s",32,0,32,32,QColor(192,64,16,160)));const auto staged=image(staging);exact(staged,ab,"Backdrop staging bytes");exact(staged,as,"Source staging bytes",40,20);
    for(std::size_t i=0;i<modes.size();++i)for(const auto opacity:{0.,.5,1.}){
        for(const bool alpha:{false,true}){auto d=pair(alpha);d.objects.at("source").compositing.blend=modes[i];d.objects.at("source").compositing.opacity.literal=opacity;const auto result=image(d);exact(result,opacity==0?(alpha?ab:b):opacity==.5?(alpha?alpha_half[i]:opaque_half[i]):alpha?alpha_full[i]:opaque_full[i],std::string("Object ")+modes[i]+" raw independent oracle");exact(result,alpha?ab:b,"Cropped source leaves exterior backdrop",50,50);}
        auto d=base();add(d,rectangle("source",8,8,32,32,QColor(192,64,16,160)));d.objects.at("source").compositing.blend=modes[i];d.objects.at("source").compositing.opacity.literal=opacity;exact(image(d),opacity==0?Pixel{}:opacity==.5?Pixel{60,20,5,80}:as,"Transparent backdrop fractional opacity");
        auto transparent=pair(true);transparent.objects.at("source").stack.front().parameters.at("a").literal=0;transparent.objects.at("source").compositing.blend=modes[i];transparent.objects.at("source").compositing.opacity.literal=opacity;exact(image(transparent),ab,"Transparent source preserves exact destination");
    }
}
void scopes(){
    for(std::size_t i=0;i<modes.size();++i){
        auto d=pair();add(d,rectangle("source2",16,8,32,32,QColor(204,51,102)));group(d,"group",{"source","source2"});auto& aggregate=d.objects.at("group").compositing;aggregate.blend=modes[i];aggregate.opacity.literal=.5;const auto result=image(d);exact(result,opaque_half[i],"Aggregate Group overlap applies source opacity once",20,20);exact(result,opaque_half[i],"Aggregate Group nonoverlap has same opacity",12,20);
        auto pass=pair();pass.objects.at("source").compositing.blend=modes[i];group(pass,"neutral",{"source"});exact(image(pass),opaque_full[i],"Neutral group passes through to current backdrop");pass.objects.at("neutral").compositing.isolated=true;exact(image(pass),s,"Explicit group isolates child from outer backdrop");
        // Alternate legacy/new scopes with nonzero origins. A local backdrop
        // inside nested isolation must be used; later normal paint must survive.
        auto nested=base();add(nested,rectangle("outer",0,0,64,64,Qt::yellow));add(nested,rectangle("local",8,8,40,40,QColor(51,102,153)));add(nested,rectangle("source",12,12,28,28,QColor(204,51,102)));nested.objects.at("source").compositing.blend=modes[i];group(nested,"inner",{"local","source"});nested.objects.at("inner").compositing.isolated=true;group(nested,"parent",{"inner"});nested.objects.at("parent").compositing.blend="multiply";add(nested,rectangle("later",48,48,8,8,Qt::green));const auto pixels=image(nested);const auto e=opaque_full[i];exact(pixels,{e.r,e.g,0,255},"Nested scope uses local backdrop before legacy parent blend");exact(pixels,{0,255,0,255},"Later normal sibling after byte/Qt boundaries",50,50);
    }
}
void masks(){
    for(std::size_t i=0;i<modes.size();++i)for(const auto mode:{"geometry","alpha","luma"}){
        auto d=pair();const bool geometry=std::string_view(mode)=="geometry";add(d,rectangle("mask-source",8,8,24,32,geometry?Qt::black:std::string_view(mode)=="alpha"?QColor(255,255,255,128):QColor(128,128,128)));d.objects.at("mask-source").visible=false;if(geometry)d.objects.at("mask-source").compositing.opacity.literal=0;
        auto& c=d.objects.at("source").compositing;c.blend=modes[i];c.opacity.literal=.5;GeometryMask m{"mask","mask-source"};m.mode=mode;c.mask=m;const auto result=image(d);exact(result,geometry?opaque_half[i]:mask_half[i],"Mask then opacity then blend raw oracle");exact(result,b,"Masked crop preserves exterior backdrop",36,20);
        if(!geometry){c.mask->invert=true;exact(image(d),opaque_half[i],"Inverted appearance mask covers target outside source",36,20);}
    }
    constexpr std::array<Pixel,4> effected{{{79,92,130,255},{38,105,172,255},{102,82,120,255},{47,98,149,255}}};
    for(std::size_t i=0;i<modes.size();++i){auto d=pair();group(d,"effect-group",{"source"});
        auto& g=d.objects.at("effect-group");g.stack.push_back(default_operation("posterize","nect.group.posterize"));
        g.compositing.blend=modes[i];g.compositing.opacity.literal=.5;
        add(d,rectangle("coverage",8,8,32,32,QColor(255,255,255,128)));d.objects.at("coverage").visible=false;
        GeometryMask m{"effect-mask","coverage"};m.mode="alpha";g.compositing.mask=m;
        exact(image(d),effected[i],"Aggregate Group effect then mask then opacity then new blend");}
    // Hidden Alpha source: neutral child color blend sees source-local white,
    // not the destination; root blend is ignored and root opacity is retained.
    auto d=pair();add(d,rectangle("mask-backdrop",8,8,32,32,Qt::white));add(d,rectangle("mask-child",8,8,32,32,QColor(204,51,102)));d.objects.at("mask-child").compositing.blend="color";group(d,"mask-group",{"mask-backdrop","mask-child"});auto& source=d.objects.at("mask-group");source.visible=false;source.compositing.blend="luminosity";source.compositing.opacity.literal=.5;GeometryMask mask{"mask","mask-group"};mask.mode="alpha";d.objects.at("source").compositing.mask=mask;d.objects.at("source").compositing.opacity.literal=.5;
    constexpr std::array<Pixel,4> nested_mask{{{78,91,138,255},{46,103,161,255},{87,87,138,255},{53,104,155,255}}};
    for(std::size_t i=0;i<modes.size();++i){d.objects.at("source").compositing.blend=modes[i];exact(image(d),nested_mask[i],"Hidden source nested new blend projects local scope and Qt root opacity127");}
    auto stage=d;stage.objects.erase("backdrop");stage.objects.erase("source");stage.compositions.front().roots={"mask-group"};
    stage.objects.at("mask-group").visible=true;stage.objects.at("mask-group").compositing.blend="normal";
    exact(image(stage),{127,127,127,127},"Explicit hidden-source staging oracle distinguishes Qt root opacity from kernel opacity");
}
void crop_and_dpr(){
    for(std::size_t i=0;i<modes.size();++i){auto d=pair();d.objects.at("source").compositing.blend=modes[i];auto& board=d.compositions.front().artboards.front();board.x=7;board.y=11;exact(image(d),opaque_full[i],"Nonzero crop origins preserve physical destination offsets",13,9);exact(image(d,2),opaque_full[i],"Export scale preserves same interior pixels",26,18);}
    auto d=pair();auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;d.objects.at("backdrop")=rectangle("backdrop",0,0,640,480,QColor(51,102,153));d.objects.at("source")=rectangle("source",100,100,200,200,QColor(204,51,102));d.objects.at("source").compositing.blend="hue";Session session(d);Canvas canvas(session);canvas.resize(740,580);canvas.show();QApplication::processEvents();canvas.fit_artboard();canvas.set_selection({});canvas.set_show_mask_outline(false);QApplication::processEvents();check(std::abs(canvas.zoom()-1)<1e-8,"Actual widget fixture has unit document zoom");const auto expected=qEnvironmentVariable("QT_SCALE_FACTOR").toDouble();if(expected>0)check(std::abs(canvas.devicePixelRatioF()-expected)<.02,"Actual widget DPR matches requested factor");const auto capture=canvas.grab().toImage();const double x=(canvas.width()/2.+(200-320)*canvas.zoom())*capture.devicePixelRatio(),y=(canvas.height()/2.+(200-240)*canvas.zoom())*capture.devicePixelRatio();exact(capture,opaque_full[0],"Actual widget physical-pixel cropped compositor",qRound(x),qRound(y));std::cout<<"actual_dpr="<<canvas.devicePixelRatioF()<<'\n';
}
template<class T>T* visible(Window& w,const char* name){QApplication::processEvents();for(auto* item:w.findChildren<T*>())if(item->isVisible()&&item->objectName()==name)return item;throw std::runtime_error(std::string("Missing ")+name);}
void ui(){QTemporaryDir dir;Window w(dir.path());w.host.session=Session(pair());w.host.edited();w.show();QApplication::processEvents();w.canvas->set_selection("source");QApplication::processEvents();auto& session=w.host.session;check(visible<QComboBox>(w,"object-blend")->count()==26,"Inspector exposes16 descriptor choices");for(const auto mode:modes){const auto before=session.document();const auto revision=session.revision();auto* combo=visible<QComboBox>(w,"object-blend");const int index=combo->findData(QString::fromLatin1(mode));check(index>=12,"New choice reachable by exact ID");combo->setCurrentIndex(index);check(session.revision()==revision+1&&session.document().objects.at("source").compositing.blend==mode,"Inspector edits exact stable ID through Session");auto* status=visible<QLabel>(w,"object-blend-status");check(status->text().contains("8-bit premultiplied")&&status->text().contains("AE parity unverified")&&status->text().contains("w3c-binary64")&&status->toolTip().contains("nearest-ties-up"),"Inspector status has current profile and arithmetic, no AE claim");combo=visible<QComboBox>(w,"object-blend");combo->setCurrentIndex(combo->currentIndex());check(session.revision()==revision+1,"Repeated selection does not create edit");const auto authored=session.document();session.undo(session.revision());w.host.edited();check(session.document()==before,"Inspector blend Undo exact");session.redo(session.revision());w.host.edited();check(session.document()==authored&&visible<QComboBox>(w,"object-blend")->currentData().toString()==mode,"Inspector blend Redo and choice readback");}}
void host(){QTemporaryDir dir;Host host(dir.path()+"/recovery");auto d=pair(true);const auto path=dir.path()+"/saved.nect",png=dir.path()+"/pixels.png";for(std::size_t i=0;i<modes.size();++i){d.objects.at("source").compositing.blend="normal";d.objects.at("source").compositing.opacity.literal=.5;host.session=Session(d);
        const auto envelope=[&](const QString& profile) {return QJsonObject{{"op","core"},{"session_id",host.session_id},
            {"document_id",QString::fromStdString(d.id)},{"request",QJsonObject{{"op","apply"},{"expected_revision",0},
            {"commands",QJsonArray{QJsonObject{{"type","set_compositing"},{"object","source"},{"blend",modes[i]},
                {"isolated",false},{"profile",profile}}}}}}};};
        const auto before_api=host.session.document();const auto before_api_history=host.session.history();
        const auto unsupported=QJsonDocument::fromJson(host.dispatch(QJsonDocument(envelope("linear-srgb16")).toJson())).object();
        check(!unsupported["ok"].toBool()&&unsupported["error"].toObject()["code"].toString()=="UNSUPPORTED_BLEND_PROFILE"&&
            host.session.document()==before_api&&host.session.history()==before_api_history&&host.session.revision()==0,
            "Production Host API preserves exact source on unavailable profile request");
        const auto edit=QJsonDocument::fromJson(host.dispatch(QJsonDocument(envelope("nect.srgb8-premultiplied/v1")).toJson())).object();
        check(edit["ok"].toBool()&&host.session.revision()==1&&host.session.document().objects.at("source").compositing.blend==modes[i],
            "Production Host API installs exact new ID through shared Session");
        host.save(path);const auto native=encode(host.session.document());host.open(path);check(encode(host.session.document())==native,"Host save/reopen exact authored state");const auto expected=image(host.session.document());host.export_png(png,"comp","board",1,false,host.session.revision());QImage decoded(png);decoded=decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);check(decoded.size()==expected.size(),"Host PNG decoded dimensions");for(int y=0;y<expected.height();++y)for(int x=0;x<expected.width();++x)check(decoded.pixel(x,y)==expected.pixel(x,y),"Host PNG uses same Canvas raw pixels");exact(decoded,alpha_half[i],"PNG independent interior oracle");const auto doc=host.session.document();const auto revision=host.session.revision();const auto history=host.session.history();const auto identity=host.session_id;const auto file=host.file_path;auto lie=native;const std::string marker="\"version\":\"0.80\"";lie.replace(lie.find(marker),marker.size(),"\"version\":\"0.78\"");QFile invalid(dir.path()+"/lie.nect");check(invalid.open(QIODevice::WriteOnly),"Write native lie fixture");invalid.write(QByteArray::fromStdString(lie));invalid.close();rejects("NATIVE_VERSION_MISMATCH",[&]{host.open(invalid.fileName());});check(host.session.document()==doc&&host.session.revision()==revision&&host.session.history()==history&&host.session_id==identity&&host.file_path==file,"Rejected0.78 native open preserves all live identity/state");QFile old(png);check(old.open(QIODevice::ReadOnly),"Read PNG destination");const auto bytes=old.readAll();old.close();rejects("REVISION_CONFLICT",[&]{host.export_png(png,"comp","board",1,false,revision+1);});check(old.open(QIODevice::ReadOnly)&&old.readAll()==bytes&&host.session.document()==doc,"Stale PNG request preserves destination and native state");const auto svg=export_svg(doc,"comp","board");check(svg.find(std::string("mix-blend-mode:")+modes[i])!=std::string::npos&&svg.find("isolation:isolate")!=std::string::npos,"SVG exact CSS ID and scope representation");}}
void allocation_limit(){
    auto d=base();auto& board=d.compositions.front().artboards.front();board.width=4096;board.height=4096;
    add(d,rectangle("source",0,0,4096,4096,QColor(204,51,102)));
    add(d,rectangle("coverage",0,0,4096,4096,Qt::white));d.objects.at("coverage").visible=false;
    auto& c=d.objects.at("source").compositing;c.blend="hue";GeometryMask mask{"limit-mask","coverage"};mask.mode="alpha";c.mask=mask;
    rejects("RENDER_LIMIT",[&]{(void)image(d);});
}
void bounded_performance(){auto d=base();d.compositions.front().artboards.front().width=640;d.compositions.front().artboards.front().height=480;for(int i=0;i<24;++i){auto o=rectangle("layer-"+std::to_string(i),i*12,i*8,300,220,QColor(180,80+i,140,180));o.compositing.blend=modes[std::size_t(i)%4];o.compositing.opacity.literal=.75;add(d,std::move(o));}QElapsedTimer timer;timer.start();const auto result=image(d);check(!result.isNull(),"Bounded24-layer representative scene renders");std::cout<<"representative_640x480_24_layers_ms="<<timer.elapsed()<<" (usability observation, no reference-hardware p95 claim)\n";}
}
int main(int argc,char** argv){if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{pixels();scopes();masks();crop_and_dpr();ui();host();allocation_limit();bounded_performance();std::cout<<"PASS "<<checks<<" new blend Canvas/raw-pixel/UI/Host/native/PNG consumer checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
