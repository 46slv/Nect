#include "nect/core.hpp"
#include "nect/raster.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

using namespace nect;
namespace {
using Bytes=std::vector<unsigned char>;
int checks=0;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
template<class F> void rejects(const char* code,F action) {
    try{action();}catch(const Error& e){check(e.code==code,(std::string("Expected ")+code+", got "+e.code+": "+e.what()).c_str());return;}
    throw std::runtime_error(std::string("Expected ")+code);
}
void append32(Bytes& b,std::uint32_t v){for(int shift=24;shift>=0;shift-=8)b.push_back(static_cast<unsigned char>(v>>shift));}
void put32(Bytes& b,std::size_t p,std::uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=static_cast<unsigned char>(v>>(24-i*8));}
void put16(Bytes& b,std::size_t p,unsigned v){b[p]=static_cast<unsigned char>(v>>8);b[p+1]=static_cast<unsigned char>(v);}
std::uint32_t checksum(const Bytes& bytes,std::size_t begin) {
    std::uint32_t result=0xffffffff;
    for(std::size_t i=begin;i<bytes.size();++i){result^=bytes[i];for(int bit=0;bit<8;++bit)result=(result&1)?(result>>1)^0xedb88320U:result>>1;}
    return result^0xffffffff;
}
void chunk(Bytes& png,const char* name,const Bytes& bytes) {
    append32(png,static_cast<std::uint32_t>(bytes.size()));const auto begin=png.size();
    png.insert(png.end(),name,name+4);png.insert(png.end(),bytes.begin(),bytes.end());append32(png,checksum(png,begin));
}
// Independent tiny fixture writer: PNG filters None + zlib stored blocks.
Bytes stored(const Bytes& source) {
    Bytes result{0x78,0x01};std::size_t offset=0;
    do {
        const auto n=std::min<std::size_t>(65535,source.size()-offset);result.push_back(offset+n==source.size()?1:0);
        result.push_back(static_cast<unsigned char>(n));result.push_back(static_cast<unsigned char>(n>>8));
        result.push_back(static_cast<unsigned char>(~n));result.push_back(static_cast<unsigned char>((~n)>>8));
        result.insert(result.end(),source.begin()+offset,source.begin()+offset+n);offset+=n;
    }while(offset<source.size());
    std::uint32_t a=1,b=0;for(const auto byte:source){a=(a+byte)%65521;b=(b+a)%65521;}append32(result,(b<<16)|a);return result;
}
Bytes png(const RasterPixels& pixels,const std::vector<std::pair<std::string,Bytes>>& metadata={}) {
    Bytes result{137,80,78,71,13,10,26,10},header;append32(header,pixels.width);append32(header,pixels.height);
    header.insert(header.end(),{8,6,0,0,0});chunk(result,"IHDR",header);
    for(const auto& [name,value]:metadata)chunk(result,name.c_str(),value);
    Bytes scan;for(std::uint32_t y=0;y<pixels.height;++y){scan.push_back(0);const auto p=std::size_t(y)*pixels.width*4;
        scan.insert(scan.end(),pixels.rgba.begin()+p,pixels.rgba.begin()+p+pixels.width*4);}
    chunk(result,"IDAT",stored(scan));chunk(result,"IEND",{});return result;
}
const RasterPixels sample{2,3,{255,0,0,255, 0,255,0,128, 0,0,255,0, 23,87,199,34, 128,128,128,255, 12,34,56,78}};

#ifdef _WIN32
using Microsoft::WRL::ComPtr;
void hr(HRESULT result){if(FAILED(result))throw std::runtime_error("Synthetic WIC fixture failed: "+std::to_string(static_cast<unsigned long>(result)));}
Bytes jpeg() {
    const auto status=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(status!=RPC_E_CHANGED_MODE)hr(status);
    Bytes result(65536);
    {
        ComPtr<IWICImagingFactory> factory;hr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        ComPtr<IWICStream> stream;hr(factory->CreateStream(&stream));hr(stream->InitializeFromMemory(result.data(),static_cast<DWORD>(result.size())));
        ComPtr<IWICBitmapEncoder> encoder;hr(CoCreateInstance(CLSID_WICJpegEncoder,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&encoder)));
        hr(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));ComPtr<IWICBitmapFrameEncode> frame;
        hr(encoder->CreateNewFrame(&frame,nullptr));hr(frame->Initialize(nullptr));hr(frame->SetSize(2,3));
        auto format=GUID_WICPixelFormat24bppBGR;hr(frame->SetPixelFormat(&format));check(IsEqualGUID(format,GUID_WICPixelFormat24bppBGR),"JPEG fixture is 8-bit RGB");
        Bytes bgr;for(unsigned i=0;i<6;++i){bgr.push_back(sample.rgba[i*4+2]);bgr.push_back(sample.rgba[i*4+1]);bgr.push_back(sample.rgba[i*4]);}
        hr(frame->WritePixels(3,6,static_cast<UINT>(bgr.size()),bgr.data()));hr(frame->Commit());hr(encoder->Commit());
        LARGE_INTEGER zero{};ULARGE_INTEGER position{};hr(stream->Seek(zero,STREAM_SEEK_CUR,&position));result.resize(static_cast<std::size_t>(position.QuadPart));
    }
    if(SUCCEEDED(status))CoUninitialize();return result;
}
Bytes segment(Bytes original,unsigned marker,const Bytes& data) {
    Bytes prefix{0xff,static_cast<unsigned char>(marker),static_cast<unsigned char>((data.size()+2)>>8),static_cast<unsigned char>(data.size()+2)};
    prefix.insert(prefix.end(),data.begin(),data.end());original.insert(original.begin()+2,prefix.begin(),prefix.end());return original;
}
Bytes orient(Bytes original,unsigned value) {
    Bytes data{'E','x','i','f',0,0,'I','I',42,0,8,0,0,0,1,0,0x12,1,3,0,1,0,0,0,
        static_cast<unsigned char>(value),0,0,0,0,0,0,0};return segment(std::move(original),0xe1,data);
}
Bytes linear_profile() {
    // Synthetic ICC v2 matrix/shaper profile: linear sRGB primaries, D50 PCS.
    // No external profile file or licensed fixture is required.
    std::vector<std::pair<std::string,Bytes>> tags;
    const auto xyz=[](double x,double y,double z){Bytes b{'X','Y','Z',' ',0,0,0,0};for(const auto v:{x,y,z})append32(b,static_cast<std::uint32_t>(std::lround(v*65536)));return b;};
    tags.push_back({"wtpt",xyz(.9642,1,.8249)});tags.push_back({"rXYZ",xyz(.4360747,.2225045,.0139322)});
    tags.push_back({"gXYZ",xyz(.3850649,.7168786,.0971045)});tags.push_back({"bXYZ",xyz(.1430804,.0606169,.7141733)});
    const Bytes curve{'c','u','r','v',0,0,0,0,0,0,0,1,1,0};
    for(const auto* name:{"rTRC","gTRC","bTRC"})tags.push_back({name,curve});
    Bytes description{'d','e','s','c',0,0,0,0};const std::string label="Synthetic linear RGB";append32(description,static_cast<std::uint32_t>(label.size()+1));
    description.insert(description.end(),label.begin(),label.end());description.push_back(0);description.resize(description.size()+78,0);tags.push_back({"desc",description});
    tags.push_back({"cprt",Bytes{'t','e','x','t',0,0,0,0,'T','e','s','t',0}});
    Bytes result(132+12*tags.size(),0);result[8]=2;result[9]=0x10;
    for(const auto& [offset,text]:std::vector<std::pair<std::size_t,std::string>>{{12,"mntr"},{16,"RGB "},{20,"XYZ "},{36,"acsp"},{40,"MSFT"}})
        std::copy_n(text.data(),4,result.data()+offset);
    put16(result,24,2026);put16(result,26,1);put16(result,28,1);
    put32(result,68,static_cast<std::uint32_t>(std::lround(.9642*65536)));put32(result,72,65536);put32(result,76,static_cast<std::uint32_t>(std::lround(.8249*65536)));
    put32(result,128,static_cast<std::uint32_t>(tags.size()));
    for(std::size_t i=0;i<tags.size();++i){while(result.size()%4)result.push_back(0);const auto offset=result.size();
        std::copy_n(tags[i].first.data(),4,result.data()+132+i*12);put32(result,136+i*12,static_cast<std::uint32_t>(offset));put32(result,140+i*12,static_cast<std::uint32_t>(tags[i].second.size()));
        result.insert(result.end(),tags[i].second.begin(),tags[i].second.end());}
    while(result.size()%4)result.push_back(0);put32(result,0,static_cast<std::uint32_t>(result.size()));return result;
}
Bytes iccp(const Bytes& profile){Bytes result{'t','e','s','t',0,0};const auto compressed=stored(profile);result.insert(result.end(),compressed.begin(),compressed.end());return result;}

void png_roundtrip() {
    const auto bytes=png(sample);const auto accepted=make_raster(bytes);
    check(accepted->bytes()==bytes&&accepted->mime()=="image/png"&&accepted->width()==2&&accepted->height()==3,"Original PNG bytes and oriented dimensions are retained");
    check(accepted->sha256().size()==64&&accepted->sha256().find_first_not_of("0123456789abcdef")==std::string::npos,"Source SHA256 is canonical hexadecimal");
    check(accepted->orientation()==1&&accepted->interpretation_version()==1&&accepted->color_interpretation()=="assumed_srgb","Unprofiled PNG reports versioned assumed-sRGB interpretation");
    check(decode_raster(*accepted)==sample,"RGBA decoder retains exact straight alpha and RGB beneath zero alpha");
    const auto same=make_raster(bytes);check(*same==*accepted&&same.get()!=accepted.get()&&same->sha256()==accepted->sha256(),"Payload equality is content-based across allocations");
    const auto normalized=encode_raster_png(sample);check(decode_raster(*make_raster(normalized))==sample,"Canonical PNG preserves every RGBA channel exactly");
    const auto declared=make_raster(png(sample,{{"sRGB",{0}}}));check(declared->color_interpretation()=="declared_srgb"&&decode_raster(*declared)==sample,"Explicit sRGB metadata is reported without changing pixels");
    check(declared->sha256()!=accepted->sha256()&&!(*declared==*accepted),"Distinct accepted source bytes are not collapsed merely because pixels match");
    check(accepted->bytes()==bytes,"Derivative decode/encode never rewrites accepted source bytes");
    Bytes indexed{137,80,78,71,13,10,26,10},header;append32(header,2);append32(header,1);header.insert(header.end(),{1,3,0,0,0});
    chunk(indexed,"IHDR",header);chunk(indexed,"PLTE",{255,0,0,0,0,255});chunk(indexed,"tRNS",{255,64});chunk(indexed,"IDAT",stored({0,0x40}));chunk(indexed,"IEND",{});
    check(decode_raster(*make_raster(indexed))==RasterPixels{2,1,{255,0,0,255,0,0,255,64}},"Packed palette PNG expands indices without changing 8-bit color/alpha");
    Bytes grayscale{137,80,78,71,13,10,26,10};header.clear();append32(header,2);append32(header,1);header.insert(header.end(),{8,0,0,0,0});
    chunk(grayscale,"IHDR",header);chunk(grayscale,"IDAT",stored({0,0,128}));chunk(grayscale,"IEND",{});
    check(decode_raster(*make_raster(grayscale))==RasterPixels{2,1,{0,0,0,255,128,128,128,255}},"8-bit gray PNG expands to sRGB without silent high-bit-depth conversion");
}
void orientation_cases() {
    const auto original=jpeg();const auto base=decode_raster(*make_raster(original));
    check(base.width==2&&base.height==3&&base.rgba.size()==24,"JPEG fixture decoded at original dimensions");
    constexpr std::array<std::array<unsigned,6>,8> indices{{{{0,1,2,3,4,5}},{{1,0,3,2,5,4}},{{5,4,3,2,1,0}},{{4,5,2,3,0,1}},
        {{0,2,4,1,3,5}},{{4,2,0,5,3,1}},{{5,3,1,4,2,0}},{{1,3,5,0,2,4}}}};
    for(unsigned orientation=1;orientation<=8;++orientation) {
        const auto bytes=orient(original,orientation);const auto accepted=make_raster(bytes);const auto pixels=decode_raster(*accepted);
        check(accepted->mime()=="image/jpeg"&&accepted->orientation()==orientation&&accepted->bytes()==bytes,"JPEG retains bytes and explicit EXIF orientation");
        check(pixels.width==(orientation>=5?3U:2U)&&pixels.height==(orientation>=5?2U:3U),"EXIF orientation swaps axes only for transposed cases");
        for(unsigned i=0;i<6;++i)check(std::equal(pixels.rgba.begin()+i*4,pixels.rgba.begin()+i*4+4,base.rgba.begin()+indices[orientation-1][i]*4),"EXIF 1..8 matches independent pixel permutation oracle");
        check(decode_raster(*make_raster(encode_raster_png(pixels)))==pixels,"Oriented JPEG canonical PNG has identical display pixels");
    }
    rejects("RASTER_METADATA",[&]{make_raster(orient(original,0));});rejects("RASTER_METADATA",[&]{make_raster(orient(original,9));});
    rejects("RASTER_METADATA",[&]{make_raster(orient(orient(original,1),1));});
    auto cmyk=original;bool changed=false;
    for(std::size_t i=2;i+9<cmyk.size();++i)if(cmyk[i]==0xff&&(cmyk[i+1]==0xc0||cmyk[i+1]==0xc1||cmyk[i+1]==0xc2)){cmyk[i+9]=4;changed=true;break;}
    check(changed,"Synthetic JPEG exposes its SOF component declaration");rejects("RASTER_PIXEL_FORMAT",[&]{make_raster(cmyk);});
}
void profile_cases() {
    const auto profile=linear_profile();RasterPixels gray{1,1,{128,128,128,73}};
    const auto accepted=make_raster(png(gray,{{"iCCP",iccp(profile)}}));const auto converted=decode_raster(*accepted);
    check(accepted->color_interpretation()=="embedded_icc_to_srgb","Embedded ICC conversion is explicit");
    for(unsigned c=0;c<3;++c)check(std::abs(int(converted.rgba[c])-188)<=4,"Linear-profile midpoint converts to sRGB near 188, not unchanged 128");
    check(converted.rgba[3]==73,"ICC conversion preserves straight alpha");
    check(decode_raster(*make_raster(encode_raster_png(converted)))==converted,"Canonical PNG preserves converted ICC display pixels");
    const auto original=jpeg();Bytes app{'I','C','C','_','P','R','O','F','I','L','E',0,1,1};app.insert(app.end(),profile.begin(),profile.end());
    check(make_raster(segment(original,0xe2,app))->color_interpretation()=="embedded_icc_to_srgb","JPEG embedded ICC uses the same memory transform");
    rejects("RASTER_COLOR",[&]{make_raster(png(gray,{{"iCCP",iccp(Bytes(150,0))}}));});
    auto corrupt=iccp(profile);corrupt.back()^=1;rejects("RASTER_COLOR",[&]{make_raster(png(gray,{{"iCCP",corrupt}}));});
    auto trailing=iccp(profile);trailing.insert(trailing.end()-4,0);rejects("RASTER_COLOR",[&]{make_raster(png(gray,{{"iCCP",trailing}}));});
    rejects("RASTER_COLOR",[&]{make_raster(png(gray,{{"iCCP",iccp(Bytes(1024*1024+1,0))}}));});
    rejects("RASTER_COLOR",[&]{make_raster(png(gray,{{"gAMA",{0,0,0xc3,0x50}}}));});
    rejects("RASTER_COLOR",[&]{make_raster(png(gray,{{"cHRM",Bytes(32,0)}}));});
}
void invalid_cases() {
    rejects("RASTER_FORMAT",[]{make_raster(Bytes{'G','I','F','8','9','a'});});
    rejects("RASTER_LIMIT",[]{make_raster({});});rejects("RASTER_LIMIT",[]{make_raster(Bytes(raster_source_limit+1,0));});
    auto bytes=png(sample);bytes[bytes.size()-1]^=1;rejects("RASTER_INVALID",[&]{make_raster(bytes);});
    bytes=png(sample);bytes.resize(bytes.size()-8);rejects("RASTER_INVALID",[&]{make_raster(bytes);});
    rejects("RASTER_ANIMATED",[]{make_raster(png(sample,{{"acTL",{0,0,0,2,0,0,0,0}}}));});
    rejects("RASTER_METADATA",[]{make_raster(png(sample,{{"eXIf",{1}}}));});
    auto header_case=[](unsigned width,unsigned height,unsigned depth){Bytes result{137,80,78,71,13,10,26,10},header;
        append32(header,width);append32(header,height);header.insert(header.end(),{static_cast<unsigned char>(depth),6,0,0,0});chunk(result,"IHDR",header);return result;};
    rejects("RASTER_LIMIT",[&]{make_raster(header_case(8193,1,8));});rejects("RASTER_LIMIT",[&]{make_raster(header_case(8192,8192,8));});
    rejects("RASTER_PIXEL_FORMAT",[&]{make_raster(header_case(1,1,16));});
    rejects("RASTER_INVALID",[]{encode_raster_png({1,1,{1,2,3}});});rejects("RASTER_LIMIT",[]{encode_raster_png({0,1,{}});});
}
#endif
}
int main(){try {
    static_assert(!std::is_default_constructible_v<RasterPayload>);static_assert(!std::is_copy_constructible_v<RasterPayload>);
#ifdef _WIN32
    png_roundtrip();orientation_cases();profile_cases();invalid_cases();
#else
    rejects("RASTER_PLATFORM_UNSUPPORTED",[]{make_raster(png(sample));});
    rejects("RASTER_PLATFORM_UNSUPPORTED",[]{encode_raster_png(sample);});
#endif
    std::cout<<"PASS "<<checks<<" memory raster checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
