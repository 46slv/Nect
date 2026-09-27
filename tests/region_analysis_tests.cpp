#include "canvas.hpp"
#include "host.hpp"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
QByteArray bytes(const QString& path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Could not read native fixture");
    return file.readAll();
}
Object rectangle(Id id,double x,double y,double width,double height,int alpha=255) {
    Object object;object.id=id;object.name=id;
    Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(const auto& xy:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}) {
        Point point;point.id=id+"-p"+std::to_string(contour.points.size());point.x.literal=xy.x;point.y.literal=xy.y;
        contour.points.push_back(point);
    }
    object.contours.push_back(contour);
    auto fill=default_operation(id+"-fill","nect.paint.fill");
    fill.parameters.at("r").literal=0;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=0;
    fill.parameters.at("a").literal=static_cast<double>(alpha)/255.0;
    object.stack.push_back(std::move(fill));return object;
}
QJsonObject api(Host& host,const QJsonObject& fields) {
    QJsonObject request{{"op","analyze_regions"},{"session_id",host.session_id},
        {"document_id",QString::fromStdString(host.session.document().id)}};
    for(auto it=fields.begin();it!=fields.end();++it)request.insert(it.key(),it.value());
    return QJsonDocument::fromJson(host.dispatch(QJsonDocument(request).toJson(QJsonDocument::Compact))).object();
}
void check_region(const QJsonObject& value,int area,int x,int y,int width,int height,const char* message) {
    check(value.value("area").toInt()==area,message);
    check(value.value("x").toInt()==x&&value.value("y").toInt()==y&&
        value.value("width").toInt()==width&&value.value("height").toInt()==height,message);
}
QJsonArray expected_runs(std::initializer_list<std::array<int,3>> spans) {
    QJsonArray result;
    for(const auto& span:spans)result.append(QJsonObject{{"y",span[0]},{"x",span[1]},{"width",span[2]}});
    return result;
}
void check_edges(const QJsonObject& value,const QJsonArray& expected,int pixel_count,const char* message) {
    const auto runs=value.value("edge_runs").toArray();
    check(runs==expected,message);
    check(value.value("edge_pixel_count").toInt()==pixel_count,message);
    check(value.value("edge_rule").toString()=="foreground-4-neighbor",message);
    int run_pixel_count=0;
    for(const auto& run:runs)run_pixel_count+=run.toObject().value("width").toInt();
    check(run_pixel_count==value.value("edge_pixel_count").toInt(),"Edge run widths sum to the edge pixel count");
}
QJsonObject pixel_request(std::uint64_t revision,int threshold) {
    QImage image(8,6,QImage::Format_ARGB32_Premultiplied);image.fill(qRgba(0,0,0,0));
    for(int y=1;y<3;++y)for(int x=1;x<3;++x)image.setPixel(x,y,qRgba(40,90,130,255));
    for(int y=2;y<5;++y)for(int x=5;x<8;++x)image.setPixel(x,y,qRgba(200,80,20,255));
    auto result=analyze_region_pixels(image,threshold,1.0,revision);
    return result;
}
void independent_pixel_oracle() {
    const auto expected=pixel_request(7,128);
    const auto regions=expected.value("regions").toArray();
    check(regions.size()==2,"Two disjoint opaque rectangles produce two regions");
    check_region(regions[0].toObject(),4,1,1,2,2,"First rectangle has independent area and bounds");
    check_region(regions[1].toObject(),9,5,2,3,3,"Second rectangle has independent area and bounds");
    check(expected.value("width").toInt()==8&&expected.value("height").toInt()==6&&
        expected.value("threshold").toInt()==128&&expected.value("connectivity").toInt()==4&&
        expected.value("scale").toDouble()==1.0&&expected.value("source_revision").toInt()==7&&
        expected.value("color_space").toString()=="sRGB"&&
        expected.value("coordinate_space").toString()=="artboard-output-pixels"&&
        expected.value("origin").toString()=="top-left"&&
        expected.value("pixel_format").toString()=="ARGB32_Premultiplied"&&
        expected.value("alpha_domain").toString()=="8-bit premultiplied Canvas output alpha byte",
        "Analysis reports the declared pixel and revision domain");

    QImage edge_cases(8,6,QImage::Format_ARGB32_Premultiplied);edge_cases.fill(qRgba(0,0,0,0));
    for(int y=1;y<3;++y)for(int x=1;x<3;++x)edge_cases.setPixel(x,y,qRgba(0,0,0,255));
    for(int y=2;y<5;++y)for(int x=5;x<8;++x)edge_cases.setPixel(x,y,qRgba(0,0,0,255));
    edge_cases.setPixel(3,3,qRgba(0,0,0,255)); // Diagonal to the first block only.
    edge_cases.setPixel(0,5,qRgba(0,0,0,127));
    const auto at_128=analyze_region_pixels(edge_cases,128,1.0,8).value("regions").toArray();
    check(at_128.size()==3,"Diagonal contact remains a separate 4-connected region and alpha 127 is excluded");
    check_region(at_128[0].toObject(),4,1,1,2,2,"Row-major first seed is stable");
    check_region(at_128[1].toObject(),9,5,2,3,3,"Row-major second seed is stable");
    check_region(at_128[2].toObject(),1,3,3,1,1,"Diagonal-only pixel remains separate");
    const auto at_127=analyze_region_pixels(edge_cases,127,1.0,8).value("regions").toArray();
    check(at_127.size()==4,"An alpha byte equal to the threshold is included");
    check_region(at_127[3].toObject(),1,0,5,1,1,"The low-alpha pixel appears as a separate row-major region");

    QImage exact_limit(2000,2000,QImage::Format_ARGB32_Premultiplied);exact_limit.fill(Qt::transparent);
    const auto accepted=analyze_region_pixels(exact_limit,1,1.0,0);
    check(accepted.value("regions").toArray().isEmpty(),"Exactly 4,000,000 pixels is accepted");
    QImage over_limit(2001,2000,QImage::Format_ARGB32_Premultiplied);over_limit.fill(Qt::transparent);
    try {analyze_region_pixels(over_limit,1,1.0,0);throw std::runtime_error("Expected pixel limit refusal");}
    catch(const Error& error) {check(error.code=="ANALYSIS_LIMIT","More than 4,000,000 pixels refuses before traversal");}

    QImage exact_components(201,200,QImage::Format_ARGB32_Premultiplied);exact_components.fill(Qt::transparent);
    for(int y=0;y<200;y+=2)for(int x=0;x<200;x+=2)exact_components.setPixel(x,y,qRgba(0,0,0,255));
    check(analyze_region_pixels(exact_components,1,1.0,0).value("regions").toArray().size()==10'000,
        "Exactly 10,000 disconnected components is accepted");
    exact_components.setPixel(200,0,qRgba(0,0,0,255));
    try {analyze_region_pixels(exact_components,1,1.0,0);throw std::runtime_error("Expected component limit refusal");}
    catch(const Error& error) {check(error.code=="ANALYSIS_LIMIT","The 10,001st component refuses without a partial result");}
}
void independent_edge_oracle() {
    QImage rectangle_image(6,5,QImage::Format_ARGB32_Premultiplied);rectangle_image.fill(Qt::transparent);
    for(int y=1;y<=3;++y)for(int x=1;x<=3;++x)rectangle_image.setPixel(x,y,qRgba(0,0,0,255));
    const auto rectangle_result=analyze_region_pixels(rectangle_image,128,1.0,9);
    check_region(rectangle_result.value("regions").toArray()[0].toObject(),9,1,1,3,3,
        "Independent edge rectangle remains a nine-pixel component");
    check_edges(rectangle_result,expected_runs({{1,1,3},{2,1,1},{2,3,1},{3,1,3}}),8,
        "A 3x3 solid rectangle excludes its interior center from the edge runs");

    QImage boundary_image(3,3,QImage::Format_ARGB32_Premultiplied);boundary_image.fill(qRgba(0,0,0,255));
    const auto boundary_result=analyze_region_pixels(boundary_image,128,1.0,10);
    check_edges(boundary_result,expected_runs({{0,0,3},{1,0,1},{1,2,1},{2,0,3}}),8,
        "Pixels at every Artboard edge see outside neighbors as transparent");

    QImage diagonal_image(3,3,QImage::Format_ARGB32_Premultiplied);diagonal_image.fill(Qt::transparent);
    diagonal_image.setPixel(0,0,qRgba(0,0,0,255));
    diagonal_image.setPixel(1,1,qRgba(0,0,0,255));
    diagonal_image.setPixel(2,2,qRgba(0,0,0,127));
    const auto at_128=analyze_region_pixels(diagonal_image,128,1.0,11);
    check(at_128.value("regions").toArray().size()==2,"Diagonal-only pixels remain separate 4-connected regions");
    check_edges(at_128,expected_runs({{0,0,1},{1,1,1}}),2,
        "Two diagonally touching pixels each remain an edge pixel and alpha 127 is excluded");
    const auto at_127=analyze_region_pixels(diagonal_image,127,1.0,11);
    check(at_127.value("regions").toArray().size()==3,"Alpha equal to the lowered threshold is included");
    check_edges(at_127,expected_runs({{0,0,1},{1,1,1},{2,2,1}}),3,
        "The formerly excluded alpha-127 pixel appears as one edge run at threshold 127");

    QImage hole_image(7,7,QImage::Format_ARGB32_Premultiplied);hole_image.fill(Qt::transparent);
    for(int y=1;y<=5;++y)for(int x=1;x<=5;++x)
        if(x!=3||y!=3)hole_image.setPixel(x,y,qRgba(0,0,0,255));
    const auto hole_result=analyze_region_pixels(hole_image,128,1.0,12);
    const auto hole_regions=hole_result.value("regions").toArray();
    check(hole_regions.size()==1&&hole_regions[0].toObject().value("area").toInt()==24,
        "A transparent hole stays outside the foreground component area");
    check_edges(hole_result,expected_runs({{1,1,5},{2,1,1},{2,3,1},{2,5,1},
        {3,1,2},{3,4,2},{4,1,1},{4,3,1},{4,5,1},{5,1,5}}),20,
        "Transparent hole pixels contribute inner boundary edges around the hole");

    QImage comb(2004,102,QImage::Format_ARGB32_Premultiplied);comb.fill(Qt::transparent);
    for(int x=0;x<comb.width();++x)comb.setPixel(x,0,qRgba(0,0,0,255));
    for(int y=1;y<comb.height();++y)for(int x=0;x<comb.width();x+=2)comb.setPixel(x,y,qRgba(0,0,0,255));
    try {analyze_region_pixels(comb,1,1.0,13);throw std::runtime_error("Expected edge run limit refusal");}
    catch(const Error& error) {check(error.code=="ANALYSIS_LIMIT","The 100,001st edge run refuses without a partial result");}
}
void live_canvas_api() {
    QTemporaryDir temp;check(temp.isValid(),"Temporary test directory is available");
    auto document=empty_document("region-document","region-composition","region-artboard");
    auto& composition=document.compositions.front();
    composition.artboards.front().width=8;composition.artboards.front().height=6;
    for(auto object:std::vector<Object>{rectangle("region-first",1,1,2,2),rectangle("region-second",5,2,3,3),
            rectangle("region-diagonal",3,3,1,1),rectangle("region-low-alpha",0,5,1,1,127)}) {
        composition.roots.push_back(object.id);document.objects.emplace(object.id,std::move(object));
    }
    Host host(temp.path()+"/recovery");
    host.session=Session(std::move(document));
    const auto native=temp.path()+"/regions.nect";host.save(native);
    const auto native_before=bytes(native);
    const auto original_document=encode(host.session.document());
    const auto original_history=host.session.history();
    const auto revision=host.session.revision();
    const QJsonObject fields{{"expected_revision",static_cast<qint64>(revision)},
        {"composition","region-composition"},{"artboard","region-artboard"},{"scale",1.0},{"threshold",128}};
    const auto response=api(host,fields);
    check(response.value("ok").toBool()&&response.value("revision").toInt()==static_cast<int>(revision),
        "Live desktop API returns a successful read at the source revision");
    const auto result=response.value("result").toObject();
    const auto regions=result.value("regions").toArray();
    check(regions.size()==3,"Canvas output excludes the alpha-127 region at threshold 128");
    check_region(regions[0].toObject(),4,1,1,2,2,"Canvas rendering and API match the first independent block");
    check_region(regions[1].toObject(),9,5,2,3,3,"Canvas rendering and API match the second independent block");
    check_region(regions[2].toObject(),1,3,3,1,1,"Canvas API preserves diagonal-only separation");
    check_edges(result,expected_runs({{1,1,2},{2,1,2},{2,5,3},{3,3,1},{3,5,1},{3,7,1},{4,5,3}}),13,
        "Live Canvas edge runs match the independent row-major pixel oracle");
    check(result.value("width").toInt()==8&&result.value("height").toInt()==6&&
        result.value("source_revision").toInt()==static_cast<int>(revision),"Live result reports the rendered dimensions and source revision");
    auto lower_fields=fields;lower_fields["threshold"]=127;
    const auto lower=api(host,lower_fields);
    const auto lower_regions=lower.value("result").toObject().value("regions").toArray();
    check(lower.value("ok").toBool()&&lower_regions.size()==4,"Canvas API includes alpha exactly at threshold 127");
    check_region(lower_regions[3].toObject(),1,0,5,1,1,"Canvas low-alpha component has exact area and bounds");
    check_edges(lower.value("result").toObject(),expected_runs({{1,1,2},{2,1,2},{2,5,3},
        {3,3,1},{3,5,1},{3,7,1},{4,5,3},{5,0,1}}),14,
        "Canvas API includes the threshold-equal pixel in its edge map");
    check(host.session.revision()==revision&&host.session.history()==original_history&&
        encode(host.session.document())==original_document&&bytes(native)==native_before,
        "Analysis leaves Session revision, history, authored document and native bytes unchanged");

    auto stale=fields;stale["expected_revision"]=static_cast<qint64>(revision+1);
    check(api(host,stale).value("error").toObject().value("code")=="REVISION_CONFLICT","Stale expected revision is rejected");
    auto wrong_identity=QJsonObject{{"op","analyze_regions"},{"session_id","wrong-session"},
        {"document_id",QString::fromStdString(host.session.document().id)}};
    for(auto it=fields.begin();it!=fields.end();++it)wrong_identity.insert(it.key(),it.value());
    check(QJsonDocument::fromJson(host.dispatch(QJsonDocument(wrong_identity).toJson())).object()
        .value("error").toObject().value("code")=="SESSION_CONFLICT","Mismatched live Session identity is rejected");
    auto unknown=fields;unknown["unexpected"]=true;
    check(api(host,unknown).value("error").toObject().value("code")=="UNKNOWN_FIELD","Unknown API fields are rejected");
    auto fractional=fields;fractional["threshold"]=127.5;
    check(api(host,fractional).value("error").toObject().value("code")=="INVALID_THRESHOLD","Fractional thresholds are rejected");
    auto missing_board=fields;missing_board["artboard"]="missing-artboard";
    check(api(host,missing_board).value("error").toObject().value("code")=="MISSING_ARTBOARD","Unknown Artboard is rejected");

    host.session.begin_gesture(revision);
    host.session.update_gesture({Set{{"region-first","region-first-p0","x"},1.5}});
    check(api(host,fields).value("error").toObject().value("code")=="GESTURE_ACTIVE","Active gesture preview is rejected");
    host.session.cancel_gesture();

    auto oversized_document=empty_document("large-region-document","large-region-composition","large-region-artboard");
    oversized_document.compositions.front().artboards.front().width=2001;
    oversized_document.compositions.front().artboards.front().height=2000;
    Host oversized(temp.path()+"/large-recovery");oversized.session=Session(std::move(oversized_document));
    const auto large_fields=QJsonObject{{"expected_revision",0},{"composition","large-region-composition"},
        {"artboard","large-region-artboard"},{"scale",1.0},{"threshold",128}};
    const auto limit=api(oversized,large_fields);
    check(limit.value("error").toObject().value("code")=="ANALYSIS_LIMIT","Live API applies the 4,000,000 pixel limit before render allocation");

    auto comb_document=empty_document("comb-region-document","comb-region-composition","comb-region-artboard");
    auto& comb_composition=comb_document.compositions.front();
    comb_composition.artboards.front().width=2004;comb_composition.artboards.front().height=102;
    auto comb_bar=rectangle("comb-bar",0,0,2004,1);
    comb_composition.roots.push_back(comb_bar.id);comb_document.objects.emplace(comb_bar.id,std::move(comb_bar));
    for(int x=0;x<2004;x+=2) {
        auto tooth=rectangle("comb-tooth-"+std::to_string(x),x,1,1,101);
        comb_composition.roots.push_back(tooth.id);comb_document.objects.emplace(tooth.id,std::move(tooth));
    }
    Host comb_host(temp.path()+"/comb-recovery");comb_host.session=Session(std::move(comb_document));
    const auto comb_native=temp.path()+"/comb.nect";comb_host.save(comb_native);
    const auto comb_native_before=bytes(comb_native);
    const auto comb_document_before=encode(comb_host.session.document());
    const auto comb_history_before=comb_host.session.history();
    const auto comb_revision=comb_host.session.revision();
    const auto comb_fields=QJsonObject{{"expected_revision",static_cast<qint64>(comb_revision)},
        {"composition","comb-region-composition"},{"artboard","comb-region-artboard"},{"scale",1.0},{"threshold",1}};
    const auto comb_limit=api(comb_host,comb_fields);
    check(comb_limit.value("error").toObject().value("code")=="ANALYSIS_LIMIT"&&!comb_limit.contains("result"),
        "Live API refuses an over-limit edge map without a partial result");
    check(comb_host.session.revision()==comb_revision&&comb_host.session.history()==comb_history_before&&
        encode(comb_host.session.document())==comb_document_before&&bytes(comb_native)==comb_native_before,
        "Edge-map limit refusal leaves Session revision, history, Document and native bytes unchanged");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        independent_pixel_oracle();
        independent_edge_oracle();
        live_canvas_api();
        std::cout<<"PASS region and edge-map pixel oracles, limits, live Canvas API and read-only behavior\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
