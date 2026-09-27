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
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <vector>

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
void check_morphology(const QJsonObject& value,const QJsonArray& expected,int area,const char* message) {
    const auto morphology=value.value("morphology").toObject();
    check(morphology.value("operation").toString()=="dilate"&&
        morphology.value("kernel").toString()=="cross-4-radius-1"&&
        morphology.value("border").toString()=="outside-background-clipped"&&
        morphology.value("coordinate_space").toString()=="artboard-output-pixels",message);
    const auto runs=morphology.value("runs").toArray();
    check(runs==expected&&morphology.value("area").toInt()==area,message);
    int run_area=0;
    for(const auto& run:runs)run_area+=run.toObject().value("width").toInt();
    check(run_area==morphology.value("area").toInt(),"Morphology run widths sum to the mask area");
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
QJsonObject expected_contour(int region,std::initializer_list<std::array<int,2>> points) {
    QJsonArray vertices;
    for(const auto& xy:points) {
        QJsonArray point;point.append(xy[0]);point.append(xy[1]);vertices.append(point);
    }
    return {{"region_index",region},{"closed",true},{"vertices",vertices}};
}
void check_contours(const QJsonObject& value,const QJsonArray& expected,const char* message) {
    check(value.value("outer_contours").toArray()==expected,message);
    check(value.value("contour_rule").toString()=="foreground-right-clockwise-outer"&&
        value.value("contour_coordinate_space").toString()=="artboard-output-pixel-corners"&&
        value.value("contour_closed").toString()=="implicit-last-to-first",
        "Contour rule, corner coordinate space and implicit closure are declared");
}
QJsonObject expected_line(const char* direction,std::array<int,2> start,std::array<int,2> end,int length) {
    QJsonArray start_point;start_point.append(start[0]);start_point.append(start[1]);
    QJsonArray end_point;end_point.append(end[0]);end_point.append(end[1]);
    return {{"direction",direction},{"start",start_point},{"end",end_point},{"length_pixels",length}};
}
void check_lines(const QJsonObject& value,const QJsonArray& expected,const char* message) {
    check(value.value("line_candidates").toArray()==expected,message);
    check(value.value("line_rule").toString()=="exact-one-pixel-wide-4-direction-min3"&&
        value.value("line_coordinate_space").toString()=="artboard-output-pixel-centers",
        "Line candidates declare the thinness rule and output pixel-center coordinates");
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
void independent_morphology_oracle() {
    QImage center(5,5,QImage::Format_ARGB32_Premultiplied);center.fill(Qt::transparent);
    center.setPixel(2,2,qRgba(0,0,0,255));
    const auto center_result=analyze_region_pixels(center,128,1.0,23);
    check_morphology(center_result,expected_runs({{1,2,1},{2,1,3},{3,2,1}}),5,
        "One interior source pixel dilates once with the cross kernel");

    QImage corner(5,5,QImage::Format_ARGB32_Premultiplied);corner.fill(Qt::transparent);
    corner.setPixel(0,0,qRgba(0,0,0,255));
    check_morphology(analyze_region_pixels(corner,128,1.0,24),expected_runs({{0,0,2},{1,0,1}}),3,
        "A top-left source pixel clips its cross dilation to the output rectangle");

    QImage empty(3,3,QImage::Format_ARGB32_Premultiplied);empty.fill(Qt::transparent);
    check_morphology(analyze_region_pixels(empty,128,1.0,25),{},0,
        "Zero-alpha input returns an empty morphology mask");

    QImage full(3,3,QImage::Format_ARGB32_Premultiplied);full.fill(qRgba(0,0,0,255));
    check_morphology(analyze_region_pixels(full,128,1.0,26),expected_runs({{0,0,3},{1,0,3},{2,0,3}}),9,
        "Full input remains full after clipped dilation");

    QImage diagonal(4,4,QImage::Format_ARGB32_Premultiplied);diagonal.fill(Qt::transparent);
    diagonal.setPixel(1,1,qRgba(0,0,0,255));diagonal.setPixel(2,2,qRgba(0,0,0,255));
    check_morphology(analyze_region_pixels(diagonal,128,1.0,27),
        expected_runs({{0,1,1},{1,0,3},{2,1,3},{3,2,1}}),8,
        "Diagonal source pixels join through their one-step cross dilation");

    QImage low_alpha(5,5,QImage::Format_ARGB32_Premultiplied);low_alpha.fill(Qt::transparent);
    low_alpha.setPixel(2,2,qRgba(0,0,0,127));
    check_morphology(analyze_region_pixels(low_alpha,128,1.0,28),{},0,
        "An alpha-127 source is absent at threshold 128");
    check_morphology(analyze_region_pixels(low_alpha,127,1.0,28),
        expected_runs({{1,2,1},{2,1,3},{3,2,1}}),5,
        "An alpha-127 source dilates when the threshold is 127");

    QImage hole(5,5,QImage::Format_ARGB32_Premultiplied);hole.fill(qRgba(0,0,0,255));
    hole.setPixel(2,2,qRgba(0,0,0,0));
    check_morphology(analyze_region_pixels(hole,128,1.0,29),
        expected_runs({{0,0,5},{1,0,5},{2,0,5},{3,0,5},{4,0,5}}),25,
        "Cross dilation fills a one-pixel hole without extending beyond the image");
}
QImage line_candidate_cap_image(bool extra_spur) {
    const auto height=extra_spur?401:399;
    QImage image(300,height,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    for(int spine=0;spine<50;++spine) {
        const auto x=1+spine*6;
        for(int y=0;y<399;++y)image.setPixel(x,y,qRgba(0,0,0,255));
        for(int stripe=0;stripe<200;++stripe) {
            const auto y=stripe*2;
            for(int arm=1;arm<=4;++arm)image.setPixel(x+arm,y,qRgba(0,0,0,255));
        }
    }
    if(extra_spur) {
        image.setPixel(1,399,qRgba(0,0,0,255));
        for(int x=1;x<=5;++x)image.setPixel(x,400,qRgba(0,0,0,255));
    }
    return image;
}
std::uint64_t independent_boundary_edges(const QImage& image) {
    std::uint64_t result=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x) {
        if(qAlpha(image.pixel(x,y))<1)continue;
        if(x==0||qAlpha(image.pixel(x-1,y))<1)++result;
        if(x+1==image.width()||qAlpha(image.pixel(x+1,y))<1)++result;
        if(y==0||qAlpha(image.pixel(x,y-1))<1)++result;
        if(y+1==image.height()||qAlpha(image.pixel(x,y+1))<1)++result;
    }
    return result;
}
bool independent_pixels_are_isolated(const QImage& image,int threshold) {
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x) {
        if(qAlpha(image.pixel(x,y))<threshold)continue;
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
            if(dx==0&&dy==0)continue;
            const auto nx=x+dx,ny=y+dy;
            if(nx>=0&&ny>=0&&nx<image.width()&&ny<image.height()&&qAlpha(image.pixel(nx,ny))>=threshold)
                return false;
        }
    }
    return true;
}
QImage morphology_cap_image(bool over_limit) {
    QImage image(409,277,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    for(int index=0;index<6'666;++index) {
        const auto x=4+(index%100)*4;
        const auto y=4+(index/100)*4;
        image.setPixel(x,y,qRgba(0,0,0,255));
    }
    image.setPixel(0,0,qRgba(0,0,0,255));
    if(over_limit)image.setPixel(404,272,qRgba(0,0,0,255));
    return image;
}
void morphology_run_cap_oracle(const QString& temp_directory) {
    const auto exact=morphology_cap_image(false);
    constexpr std::uint64_t source_count=6'667;
    constexpr std::uint64_t exact_runs=20'000;
    constexpr std::uint64_t exact_area=33'333;
    check(static_cast<std::uint64_t>(exact.width())*static_cast<std::uint64_t>(exact.height())<4'000'000&&
        independent_boundary_edges(exact)==source_count*4,
        "The morphology cap source fixture fits the pixel and boundary-edge limits");
    const auto exact_result=analyze_region_pixels(exact,128,1.0,31);
    const auto exact_morphology=exact_result.value("morphology").toObject();
    const auto exact_runs_array=exact_morphology.value("runs").toArray();
    check(exact_result.value("regions").toArray().size()==static_cast<int>(source_count)&&
        exact_result.value("edge_runs").toArray().size()==static_cast<int>(source_count)&&
        exact_result.value("line_candidates").toArray().isEmpty()&&
        exact_runs_array.size()==static_cast<int>(exact_runs)&&
        exact_morphology.value("area").toInt()==static_cast<int>(exact_area),
        "Exactly 20,000 morphology runs are accepted below every earlier analysis cap");

    const auto over=morphology_cap_image(true);
    constexpr std::uint64_t over_source_count=source_count+1;
    check(over_source_count<10'000&&over_source_count<100'000&&
        independent_boundary_edges(over)==over_source_count*4&&independent_boundary_edges(over)<200'000&&
        independent_pixels_are_isolated(over,128)&&
        static_cast<std::uint64_t>(over.width())*static_cast<std::uint64_t>(over.height())<4'000'000,
        "The over-cap fixture keeps D1-D4 component, edge-run, boundary and line counts under their limits");

    QTemporaryDir temp(temp_directory+"/morphology-cap-XXXXXX");
    check(temp.isValid(),"A temporary morphology-cap Canvas fixture directory is available");
    const auto populate_canvas_host=[&](Host& host,bool add_extra,const QString& id) {
        const auto document_id=id.toStdString();
        const auto composition_id=document_id+"-composition";
        const auto artboard_id=document_id+"-artboard";
        auto document=empty_document(document_id+"-document",composition_id,artboard_id);
        auto& artboard=document.compositions.front().artboards.front();
        artboard.width=409;artboard.height=277;
        host.session=Session(std::move(document));
        const auto input=temp.path()+"/"+id+".png";
        check(morphology_cap_image(add_extra).save(input,"PNG"),"Morphology-cap raster fixture is written");
        host.import_image(input,"embedded",composition_id,"",document_id+"-asset",document_id+"-image",document_id,0,0,0);
    };
    Host over_host(temp.path()+"/morphology-over-recovery");
    populate_canvas_host(over_host,true,"morphology-cap-over");
    const auto over_native=temp.path()+"/morphology-cap-over.nect";over_host.save(over_native);
    const auto over_native_before=bytes(over_native);
    const auto over_document_before=encode(over_host.session.document());
    const auto over_history_before=over_host.session.history();
    const auto over_revision=over_host.session.revision();
    const QJsonObject over_fields{{"expected_revision",static_cast<qint64>(over_revision)},
        {"composition","morphology-cap-over-composition"},{"artboard","morphology-cap-over-artboard"},
        {"scale",1.0},{"threshold",128}};
    const auto over_api=api(over_host,over_fields);
    const auto over_error=over_api.value("error").toObject();
    check(over_error.value("code").toString()=="ANALYSIS_LIMIT"&&!over_api.contains("result")&&
        over_error.value("message").toString().contains("20,000 morphology runs"),
        "The real Canvas API refuses the 20,001st run without returning a partial result");
    check(over_host.session.revision()==over_revision&&over_host.session.history()==over_history_before&&
        encode(over_host.session.document())==over_document_before&&bytes(over_native)==over_native_before,
        "Morphology cap refusal leaves Canvas Document, revision, History and native bytes unchanged");
}
void line_candidate_limit_oracle(const QString& temp_directory) {
    auto exact=line_candidate_cap_image(false);
    const auto foreground=50u*(399u+200u*4u);
    const auto expected_edge_runs=50u*399u;
    const auto expected_boundary_edges=50u*(800u+200u*8u);
    std::uint64_t observed_foreground=0,observed_edge_runs=0;
    for(int y=0;y<exact.height();++y) {
        bool in_run=false;
        for(int x=0;x<exact.width();++x) {
            const bool on=qAlpha(exact.pixel(x,y))>=1;
            if(on)++observed_foreground;
            if(on&&!in_run)++observed_edge_runs;
            in_run=on;
        }
    }
    check(static_cast<std::uint64_t>(exact.width())*exact.height()==119'700&&foreground==observed_foreground&&
        expected_edge_runs==19'950&&independent_boundary_edges(exact)==expected_boundary_edges&&
        observed_edge_runs==expected_edge_runs&&expected_boundary_edges==120'000&&
        50<10'000&&expected_edge_runs<100'000&&expected_boundary_edges<200'000,
        "The independently counted exact-cap construction stays under every earlier analysis limit");
    const auto exact_result=analyze_region_pixels(exact,128,1.0,21);
    check(exact_result.value("regions").toArray().size()==50&&
        exact_result.value("edge_runs").toArray().size()==static_cast<int>(expected_edge_runs)&&
        exact_result.value("line_candidates").toArray().size()==10'000,
        "Exactly 10,000 candidates are accepted below all pixel, component, edge-run and boundary-edge caps");

    QTemporaryDir temp(temp_directory+"/line-cap-XXXXXX");
    check(temp.isValid(),"A temporary line-cap Canvas fixture directory is available");
    auto populate_canvas_host=[&](Host& host,bool extra_spur,const QString& id) {
        const auto document_id=id.toStdString();
        const auto composition_id=document_id+"-composition";
        const auto artboard_id=document_id+"-artboard";
        auto document=empty_document(document_id+"-document",composition_id,artboard_id);
        auto& artboard=document.compositions.front().artboards.front();
        artboard.width=300;artboard.height=extra_spur?401:399;
        host.session=Session(std::move(document));
        const auto input=temp.path()+"/"+id+".png";
        check(line_candidate_cap_image(extra_spur).save(input,"PNG"),"Line-cap raster fixture is written");
        host.import_image(input,"embedded",composition_id,"",document_id+"-asset",document_id+"-image",document_id,0,0,0);
    };
    Host accepted_host(temp.path()+"/line-cap-exact-recovery");
    populate_canvas_host(accepted_host,false,"line-cap-exact");
    const auto accepted_native=temp.path()+"/line-cap-exact.nect";accepted_host.save(accepted_native);
    const auto accepted_native_before=bytes(accepted_native);
    const auto accepted_document_before=encode(accepted_host.session.document());
    const auto accepted_history_before=accepted_host.session.history();
    const auto accepted_revision=accepted_host.session.revision();
    const QJsonObject accepted_fields{{"expected_revision",static_cast<qint64>(accepted_revision)},
        {"composition","line-cap-exact-composition"},{"artboard","line-cap-exact-artboard"},
        {"scale",1.0},{"threshold",128}};
    const auto accepted_api=api(accepted_host,accepted_fields);
    const auto accepted_api_result=accepted_api.value("result").toObject();
    check(accepted_api.value("ok").toBool()&&accepted_api_result.value("line_candidates").toArray().size()==10'000&&
        accepted_api_result.value("regions").toArray().size()==50&&
        accepted_api_result.value("edge_runs").toArray().size()==static_cast<int>(expected_edge_runs),
        "The live Canvas API returns the exact 10,000-candidate boundary");
    check(accepted_host.session.revision()==accepted_revision&&accepted_host.session.history()==accepted_history_before&&
        encode(accepted_host.session.document())==accepted_document_before&&bytes(accepted_native)==accepted_native_before,
        "Successful line-candidate Canvas analysis leaves Document, revision, History and native bytes unchanged");

    auto over=line_candidate_cap_image(true);
    try {analyze_region_pixels(over,128,1.0,22);throw std::runtime_error("Expected 10,001-candidate pixel refusal");}
    catch(const Error& error) {check(error.code=="ANALYSIS_LIMIT","The 10,001st pixel candidate refuses with ANALYSIS_LIMIT");}
    Host over_host(temp.path()+"/line-cap-over-recovery");
    populate_canvas_host(over_host,true,"line-cap-over");
    const auto over_native=temp.path()+"/line-cap-over.nect";over_host.save(over_native);
    const auto over_native_before=bytes(over_native);
    const auto over_document_before=encode(over_host.session.document());
    const auto over_history_before=over_host.session.history();
    const auto over_revision=over_host.session.revision();
    const QJsonObject over_fields{{"expected_revision",static_cast<qint64>(over_revision)},
        {"composition","line-cap-over-composition"},{"artboard","line-cap-over-artboard"},
        {"scale",1.0},{"threshold",128}};
    const auto over_api=api(over_host,over_fields);
    const auto over_error=over_api.value("error").toObject();
    check(over_error.value("code").toString()=="ANALYSIS_LIMIT"&&!over_api.contains("result")&&
        over_error.value("message").toString().contains("10,000 line candidates"),
        "The live Canvas API refuses the 10,001st candidate without a partial result");
    check(over_host.session.revision()==over_revision&&over_host.session.history()==over_history_before&&
        encode(over_host.session.document())==over_document_before&&bytes(over_native)==over_native_before,
        "Line-candidate cap refusal leaves Document, revision, History and native bytes unchanged");
}
void independent_line_oracle() {
    QImage horizontal(8,3,QImage::Format_ARGB32_Premultiplied);horizontal.fill(Qt::transparent);
    for(int x=1;x<=5;++x)horizontal.setPixel(x,1,qRgba(0,0,0,255));
    check_lines(analyze_region_pixels(horizontal,128,1.0,30),
        QJsonArray{expected_line("horizontal",{1,1},{5,1},5)},
        "A five-pixel one-row stroke returns one inclusive center-coordinate candidate");

    QImage stripe(8,4,QImage::Format_ARGB32_Premultiplied);stripe.fill(Qt::transparent);
    for(int y=1;y<=2;++y)for(int x=1;x<=5;++x)stripe.setPixel(x,y,qRgba(0,0,0,255));
    check_lines(analyze_region_pixels(stripe,128,1.0,31),QJsonArray{},
        "A two-pixel-wide stripe has no exact one-pixel line candidate");
    QImage solid(5,5,QImage::Format_ARGB32_Premultiplied);solid.fill(Qt::transparent);
    for(int y=1;y<=3;++y)for(int x=1;x<=3;++x)solid.setPixel(x,y,qRgba(0,0,0,255));
    check_lines(analyze_region_pixels(solid,128,1.0,32),QJsonArray{},
        "A solid 3x3 rectangle has no exact one-pixel line candidate");

    QImage down(3,3,QImage::Format_ARGB32_Premultiplied);down.fill(Qt::transparent);
    down.setPixel(0,0,qRgba(0,0,0,255));down.setPixel(1,1,qRgba(0,0,0,255));down.setPixel(2,2,qRgba(0,0,0,255));
    const auto down_result=analyze_region_pixels(down,128,1.0,33);
    check(down_result.value("regions").toArray().size()==3,"A diagonal line still contains three separate 4-connected regions");
    check_lines(down_result,QJsonArray{expected_line("down_diagonal",{0,0},{2,2},3)},
        "Three diagonal pixels form one descending candidate with center indexes");
    down.setPixel(2,2,qRgba(0,0,0,127));
    check_lines(analyze_region_pixels(down,128,1.0,34),QJsonArray{},
        "Alpha 127 is excluded from a candidate at threshold 128");
    check_lines(analyze_region_pixels(down,127,1.0,34),
        QJsonArray{expected_line("down_diagonal",{0,0},{2,2},3)},
        "A threshold-equal alpha byte participates in a candidate at threshold 127");

    QImage branch(5,3,QImage::Format_ARGB32_Premultiplied);branch.fill(Qt::transparent);
    for(int x=0;x<5;++x)branch.setPixel(x,1,qRgba(0,0,0,255));
    branch.setPixel(2,0,qRgba(0,0,0,255));
    check_lines(analyze_region_pixels(branch,128,1.0,35),QJsonArray{},
        "A perpendicular branch disqualifies the junction and no candidate bridges it");
    QImage short_line(2,1,QImage::Format_ARGB32_Premultiplied);short_line.fill(qRgba(0,0,0,255));
    check_lines(analyze_region_pixels(short_line,128,1.0,36),QJsonArray{},
        "A two-pixel foreground run is omitted");

    QImage ordered(12,8,QImage::Format_ARGB32_Premultiplied);ordered.fill(Qt::transparent);
    for(int x=0;x<=2;++x)ordered.setPixel(x,0,qRgba(0,0,0,255));
    for(int x=8;x<=10;++x)ordered.setPixel(x,0,qRgba(0,0,0,255));
    for(int y=0;y<=2;++y)ordered.setPixel(5,y,qRgba(0,0,0,255));
    for(int offset=0;offset<=2;++offset)ordered.setPixel(offset,3+offset,qRgba(0,0,0,255));
    for(int offset=0;offset<=2;++offset)ordered.setPixel(9+offset,5-offset,qRgba(0,0,0,255));
    check_lines(analyze_region_pixels(ordered,128,1.0,37),QJsonArray{
        expected_line("horizontal",{0,0},{2,0},3),expected_line("horizontal",{8,0},{10,0},3),
        expected_line("vertical",{5,0},{5,2},3),expected_line("down_diagonal",{0,3},{2,5},3),
        expected_line("up_diagonal",{9,5},{11,3},3)},
        "All four orientations use direction order, y/x start order and inclusive image-border centers");
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
QImage serpentine(int width,int stripe_count) {
    QImage image(width,stripe_count*2-1,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    for(int stripe=0;stripe<stripe_count;++stripe) {
        const auto y=stripe*2;
        for(int x=0;x<width;++x)image.setPixel(x,y,qRgba(0,0,0,255));
        if(stripe+1<stripe_count) {
            const auto connector_x=(stripe%2==0)?width-1:0;
            image.setPixel(connector_x,y+1,qRgba(0,0,0,255));
        }
    }
    return image;
}
void independent_contour_oracle() {
    QImage rectangle_image(6,6,QImage::Format_ARGB32_Premultiplied);rectangle_image.fill(Qt::transparent);
    for(int y=1;y<=3;++y)for(int x=1;x<=3;++x)rectangle_image.setPixel(x,y,qRgba(0,0,0,255));
    const auto rectangle_result=analyze_region_pixels(rectangle_image,128,1.0,14);
    check_contours(rectangle_result,QJsonArray{expected_contour(0,{{1,1},{4,1},{4,4},{1,4}})},
        "A filled 3x3 pixel rectangle produces its four pixel-corner vertices");

    QImage boundary_image(3,3,QImage::Format_ARGB32_Premultiplied);boundary_image.fill(Qt::transparent);
    boundary_image.setPixel(2,2,qRgba(0,0,0,255));
    const auto boundary_result=analyze_region_pixels(boundary_image,128,1.0,15);
    check_contours(boundary_result,QJsonArray{expected_contour(0,{{2,2},{3,2},{3,3},{2,3}})},
        "A pixel at the bottom-right Artboard edge uses width and height corner coordinates");

    QImage diagonal_image(3,3,QImage::Format_ARGB32_Premultiplied);diagonal_image.fill(Qt::transparent);
    diagonal_image.setPixel(0,0,qRgba(0,0,0,255));
    diagonal_image.setPixel(1,1,qRgba(0,0,0,255));
    diagonal_image.setPixel(2,2,qRgba(0,0,0,127));
    const auto at_128=analyze_region_pixels(diagonal_image,128,1.0,16);
    check(at_128.value("regions").toArray().size()==2,"Diagonal corner contact remains two 4-connected regions");
    check_contours(at_128,QJsonArray{
        expected_contour(0,{{0,0},{1,0},{1,1},{0,1}}),
        expected_contour(1,{{1,1},{2,1},{2,2},{1,2}})},
        "Diagonal foreground pixels trace separate contours and alpha 127 is excluded at threshold 128");
    const auto at_127=analyze_region_pixels(diagonal_image,127,1.0,16);
    check(at_127.value("regions").toArray().size()==3,"Alpha equal to threshold 127 becomes a third region");
    check_contours(at_127,QJsonArray{
        expected_contour(0,{{0,0},{1,0},{1,1},{0,1}}),
        expected_contour(1,{{1,1},{2,1},{2,2},{1,2}}),
        expected_contour(2,{{2,2},{3,2},{3,3},{2,3}})},
        "The threshold-equal diagonal pixel receives its own canonical contour");

    QImage l_image(4,4,QImage::Format_ARGB32_Premultiplied);l_image.fill(Qt::transparent);
    l_image.setPixel(1,1,qRgba(0,0,0,255));l_image.setPixel(1,2,qRgba(0,0,0,255));
    l_image.setPixel(2,2,qRgba(0,0,0,255));
    const auto l_result=analyze_region_pixels(l_image,128,1.0,17);
    check_contours(l_result,QJsonArray{expected_contour(0,{{1,1},{2,1},{2,2},{3,2},{3,3},{1,3}})},
        "A three-pixel L preserves its concave corner while removing straight intermediate vertices");

    QImage self_touch_image(4,5,QImage::Format_ARGB32_Premultiplied);self_touch_image.fill(Qt::transparent);
    for(const auto& xy:std::vector<std::array<int,2>>{{1,1},{0,1},{0,2},{0,3},{1,3},{2,3},{2,2}})
        self_touch_image.setPixel(xy[0],xy[1],qRgba(0,0,0,255));
    const auto self_touch_result=analyze_region_pixels(self_touch_image,128,1.0,17);
    check(self_touch_result.value("regions").toArray().size()==1,"A connected foreground route reunites diagonal corner-touching pixels");
    check_contours(self_touch_result,QJsonArray{expected_contour(0,{{0,1},{2,1},{2,2},{1,2},{1,3},{2,3},
        {2,2},{3,2},{3,4},{0,4}})},
        "At a diagonal vertex crossing, right-turn pairing keeps the self-touch boundary walk deterministic");

    QImage ring_image(7,7,QImage::Format_ARGB32_Premultiplied);ring_image.fill(Qt::transparent);
    for(int y=1;y<=5;++y)for(int x=1;x<=5;++x)
        if(x==1||x==5||y==1||y==5)ring_image.setPixel(x,y,qRgba(0,0,0,255));
    const auto ring_result=analyze_region_pixels(ring_image,128,1.0,18);
    check_contours(ring_result,QJsonArray{expected_contour(0,{{1,1},{6,1},{6,6},{1,6}})},
        "A 5x5 opaque ring returns only its outer contour and omits the hole boundary");
    check_edges(ring_result,expected_runs({{1,1,5},{2,1,1},{2,5,1},{3,1,1},{3,5,1},
        {4,1,1},{4,5,1},{5,1,5}}),16,
        "R09-D2 edge runs still include foreground pixels adjacent to the transparent ring center");

    const auto exact_limit=analyze_region_pixels(serpentine(999,100),1,1.0,19);
    check(exact_limit.value("regions").toArray().size()==1&&exact_limit.value("outer_contours").toArray().size()==1,
        "Exactly 200,000 directed boundary edges are accepted for one connected serpentine region");
    try {analyze_region_pixels(serpentine(1000,100),1,1.0,20);throw std::runtime_error("Expected boundary edge limit refusal");}
    catch(const Error& error) {check(error.code=="ANALYSIS_LIMIT","More than 200,000 directed boundary edges refuses without a partial result");}
}
void live_canvas_api() {
    QTemporaryDir temp;check(temp.isValid(),"Temporary test directory is available");
    auto document=empty_document("region-document","region-composition","region-artboard");
    auto& composition=document.compositions.front();
    composition.artboards.front().width=8;composition.artboards.front().height=6;
    for(auto object:std::vector<Object>{rectangle("region-horizontal",3,0,5,1),rectangle("region-first",1,1,2,2),rectangle("region-second",5,2,3,3),
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
    check(regions.size()==4,"Canvas output excludes the alpha-127 region at threshold 128");
    check_region(regions[0].toObject(),5,3,0,5,1,"Canvas rendering and API match the thin border stroke");
    check_region(regions[1].toObject(),4,1,1,2,2,"Canvas rendering and API match the first independent block");
    check_region(regions[2].toObject(),9,5,2,3,3,"Canvas rendering and API match the second independent block");
    check_region(regions[3].toObject(),1,3,3,1,1,"Canvas API preserves diagonal-only separation");
    check_edges(result,expected_runs({{0,3,5},{1,1,2},{2,1,2},{2,5,3},{3,3,1},{3,5,1},{3,7,1},{4,5,3}}),18,
        "Live Canvas edge runs match the independent row-major pixel oracle");
    check_contours(result,QJsonArray{
        expected_contour(0,{{3,0},{8,0},{8,1},{3,1}}),
        expected_contour(1,{{1,1},{3,1},{3,3},{1,3}}),
        expected_contour(2,{{5,2},{8,2},{8,5},{5,5}}),
        expected_contour(3,{{3,3},{4,3},{4,4},{3,4}})},
        "Live Canvas contours preserve component indexes and use pixel-corner coordinates");
    check_lines(result,QJsonArray{expected_line("horizontal",{3,0},{7,0},5)},
        "Live Canvas API returns the exact one-pixel border stroke at pixel centers");
    check_morphology(result,expected_runs({{0,1,7},{1,0,8},{2,0,8},{3,1,7},{4,3,5},{5,5,3}}),38,
        "Live Canvas morphology matches the independently dilated thresholded artboard pixels");
    check(result.value("width").toInt()==8&&result.value("height").toInt()==6&&
        result.value("source_revision").toInt()==static_cast<int>(revision),"Live result reports the rendered dimensions and source revision");
    auto lower_fields=fields;lower_fields["threshold"]=127;
    const auto lower=api(host,lower_fields);
    const auto lower_regions=lower.value("result").toObject().value("regions").toArray();
    check(lower.value("ok").toBool()&&lower_regions.size()==5,"Canvas API includes alpha exactly at threshold 127");
    check_region(lower_regions[4].toObject(),1,0,5,1,1,"Canvas low-alpha component has exact area and bounds");
    check_edges(lower.value("result").toObject(),expected_runs({{0,3,5},{1,1,2},{2,1,2},{2,5,3},
        {3,3,1},{3,5,1},{3,7,1},{4,5,3},{5,0,1}}),19,
        "Canvas API includes the threshold-equal pixel in its edge map");
    check(lower.value("result").toObject().value("outer_contours").toArray().size()==5,
        "Canvas API returns the fourth threshold-equal component contour");
    check_lines(lower.value("result").toObject(),QJsonArray{expected_line("horizontal",{3,0},{7,0},5)},
        "The threshold-equal isolated pixel does not alter the live Canvas line candidate");
    check_morphology(lower.value("result").toObject(),expected_runs({{0,1,7},{1,0,8},{2,0,8},{3,1,7},
        {4,0,1},{4,3,5},{5,0,2},{5,5,3}}),41,
        "Live Canvas morphology includes the threshold-equal alpha-127 source and preserves row spans");
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

    auto serpent_document=empty_document("serpent-region-document","serpent-region-composition","serpent-region-artboard");
    auto& serpent_composition=serpent_document.compositions.front();
    serpent_composition.artboards.front().width=1000;serpent_composition.artboards.front().height=199;
    for(int stripe=0;stripe<100;++stripe) {
        const auto y=stripe*2;
        auto bar=rectangle("serpent-bar-"+std::to_string(stripe),0,y,1000,1);
        serpent_composition.roots.push_back(bar.id);serpent_document.objects.emplace(bar.id,std::move(bar));
        if(stripe<99) {
            const auto x=(stripe%2==0)?999:0;
            auto connector=rectangle("serpent-link-"+std::to_string(stripe),x,y+1,1,1);
            serpent_composition.roots.push_back(connector.id);serpent_document.objects.emplace(connector.id,std::move(connector));
        }
    }
    Host serpent_host(temp.path()+"/serpent-recovery");serpent_host.session=Session(std::move(serpent_document));
    const auto serpent_native=temp.path()+"/serpent.nect";serpent_host.save(serpent_native);
    const auto serpent_native_before=bytes(serpent_native);
    const auto serpent_document_before=encode(serpent_host.session.document());
    const auto serpent_history_before=serpent_host.session.history();
    const auto serpent_revision=serpent_host.session.revision();
    const auto serpent_fields=QJsonObject{{"expected_revision",static_cast<qint64>(serpent_revision)},
        {"composition","serpent-region-composition"},{"artboard","serpent-region-artboard"},
        {"scale",1.0},{"threshold",1}};
    const auto serpent_limit=api(serpent_host,serpent_fields);
    check(serpent_limit.value("error").toObject().value("code")=="ANALYSIS_LIMIT"&&!serpent_limit.contains("result"),
        "Live Canvas API rejects a connected 200,200-edge serpentine below the pixel, region and edge-run caps");
    check(serpent_host.session.revision()==serpent_revision&&serpent_host.session.history()==serpent_history_before&&
        encode(serpent_host.session.document())==serpent_document_before&&bytes(serpent_native)==serpent_native_before,
        "Boundary-edge limit refusal leaves Session revision, history, Document and native bytes unchanged");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        independent_pixel_oracle();
        independent_morphology_oracle();
        independent_line_oracle();
        independent_edge_oracle();
        independent_contour_oracle();
        live_canvas_api();
        QTemporaryDir line_cap_temp;check(line_cap_temp.isValid(),"Line-cap test directory is available");
        line_candidate_limit_oracle(line_cap_temp.path());
        QTemporaryDir morphology_cap_temp;check(morphology_cap_temp.isValid(),"Morphology-cap test directory is available");
        morphology_run_cap_oracle(morphology_cap_temp.path());
        std::cout<<"PASS region, edge-map, contour, thin-line and morphology pixel oracles, limits, live Canvas API and read-only behavior\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
