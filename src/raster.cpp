#include "nect/raster.hpp"
#include "nect/core.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <span>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <boost/beast/zlib/inflate_stream.hpp>
#include <boost/beast/zlib/error.hpp>
#endif

namespace nect {
RasterPayload::RasterPayload(std::vector<unsigned char> bytes,std::string hash,std::string mime,
    std::uint32_t width,std::uint32_t height,unsigned orientation,std::string color)
    :bytes_(std::move(bytes)),sha256_(std::move(hash)),mime_(std::move(mime)),width_(width),height_(height),
    orientation_(orientation),color_(std::move(color)){}
bool RasterPayload::operator==(const RasterPayload& other) const {
    return this==&other||(sha256_==other.sha256_&&bytes_==other.bytes_&&mime_==other.mime_&&
        width_==other.width_&&height_==other.height_&&orientation_==other.orientation_&&color_==other.color_);
}

#ifdef _WIN32
namespace {
using Microsoft::WRL::ComPtr;
using Bytes=std::span<const unsigned char>;
[[noreturn]] void fail(const char* code,const char* message){throw Error(code,message);}
void require(bool condition,const char* code,const char* message){if(!condition)fail(code,message);}
void hr(HRESULT result,const char* operation,const char* code="RASTER_DECODE") {
    if(FAILED(result))throw Error(code,std::string(operation)+" failed (HRESULT "+std::to_string(static_cast<unsigned long>(result))+")");
}
void dimensions(std::uint32_t width,std::uint32_t height) {
    require(width&&height&&width<=raster_dimension_limit&&height<=raster_dimension_limit&&
        std::uint64_t(width)*height<=raster_pixel_limit,"RASTER_LIMIT","Raster dimensions exceed 8192 per axis or 16,777,216 pixels");
}
std::uint32_t be32(Bytes b,std::size_t p){require(p<=b.size()&&b.size()-p>=4,"RASTER_INVALID","Truncated raster metadata");return (std::uint32_t(b[p])<<24)|(std::uint32_t(b[p+1])<<16)|(std::uint32_t(b[p+2])<<8)|b[p+3];}
std::uint16_t be16(Bytes b,std::size_t p){require(p<=b.size()&&b.size()-p>=2,"RASTER_INVALID","Truncated raster metadata");return static_cast<std::uint16_t>((unsigned(b[p])<<8)|b[p+1]);}
bool signature(Bytes b,std::size_t p,const char* text,std::size_t length){return p<=b.size()&&length<=b.size()-p&&std::memcmp(b.data()+p,text,length)==0;}
constexpr auto crc_table=[] {
    std::array<std::uint32_t,256> result{};
    for(std::uint32_t n=0;n<256;++n){auto c=n;for(unsigned k=0;k<8;++k)c=(c&1)?0xedb88320U^(c>>1):c>>1;result[n]=c;}
    return result;
}();
std::uint32_t crc(Bytes b){std::uint32_t c=0xffffffff;for(const auto byte:b)c=crc_table[(c^byte)&255]^(c>>8);return c^0xffffffff;}
void validate_profile(Bytes);
std::vector<unsigned char> inflate_profile(Bytes input) {
    require(input.size()>=6&&(input[0]&15)==8&&(input[0]>>4)<=7&&
        ((unsigned(input[0])*256+input[1])%31)==0&&!(input[1]&32),"RASTER_COLOR","Invalid PNG ICC zlib header");
    constexpr std::size_t limit=1024*1024;std::vector<unsigned char> output(limit+1);
    boost::beast::zlib::inflate_stream inflater;inflater.reset((input[0]>>4)+8);
    boost::beast::zlib::z_params p{};p.next_in=input.data()+2;p.avail_in=input.size()-6;p.next_out=output.data();p.avail_out=output.size();
    boost::system::error_code error;inflater.write(p,boost::beast::zlib::Flush::finish,error);
    require(p.total_out<=limit,"RASTER_COLOR","Embedded ICC profile exceeds the 1 MiB limit");
    require(error==boost::beast::zlib::error::end_of_stream&&p.total_in==input.size()-6,
        "RASTER_COLOR","Incomplete PNG ICC deflate stream or trailing compressed bytes");
    output.resize(p.total_out);std::uint32_t a=1,b=0;for(const auto byte:output){a=(a+byte)%65521;b=(b+a)%65521;}
    require((b<<16|a)==be32(input,input.size()-4),"RASTER_COLOR","PNG ICC Adler32 mismatch");validate_profile(output);return output;
}
struct Metadata {
    bool png=false,icc=false,srgb=false;
    std::uint32_t width=0,height=0;
    unsigned orientation=1;
    unsigned exif_color=0;
    std::vector<unsigned char> profile;
};
void exif(Bytes b,Metadata& m) {
    require(b.size()>=8,"RASTER_METADATA","Truncated EXIF TIFF header");
    const bool little=signature(b,0,"II",2);
    require(little||signature(b,0,"MM",2),"RASTER_METADATA","Invalid EXIF byte order");
    const auto u16=[&](std::size_t p)->std::uint16_t {if(!little)return be16(b,p);return static_cast<std::uint16_t>((unsigned(be16(b,p))>>8)|((unsigned(be16(b,p))&255)<<8));};
    const auto u32=[&](std::size_t p)->std::uint32_t {if(!little)return be32(b,p);const auto v=be32(b,p);return (v>>24)|((v>>8)&0xff00)|((v<<8)&0xff0000)|(v<<24);};
    require(u16(2)==42,"RASTER_METADATA","Invalid EXIF TIFF marker");
    bool orientation_seen=false,color_seen=false;std::uint32_t subifd=0;
    const auto directory=[&](std::uint32_t offset,bool primary) {
        require(offset>=8&&offset<b.size(),"RASTER_METADATA","Invalid EXIF directory offset");
        const auto count=u16(offset);
        require(count<=4096&&std::size_t(offset)+2+std::size_t(count)*12+4<=b.size(),"RASTER_METADATA","Invalid EXIF directory length");
        for(unsigned i=0;i<count;++i) {
            const auto p=std::size_t(offset)+2+12*i;const auto tag=u16(p),type=u16(p+2);const auto n=u32(p+4);
            if(primary&&tag==0x112) {
                require(!orientation_seen&&type==3&&n==1,"RASTER_METADATA","Invalid or repeated EXIF orientation");
                orientation_seen=true;m.orientation=u16(p+8);
                require(m.orientation>=1&&m.orientation<=8,"RASTER_METADATA","EXIF orientation must be 1 through 8");
            } else if(primary&&tag==0x8769) {
                require(!subifd&&type==4&&n==1,"RASTER_METADATA","Invalid EXIF subdirectory");subifd=u32(p+8);
            } else if(!primary&&tag==0xa001) {
                require(!color_seen&&type==3&&n==1,"RASTER_COLOR","Invalid or repeated EXIF color space");color_seen=true;m.exif_color=u16(p+8);
            }
        }
    };
    directory(u32(4),true);if(subifd)directory(subifd,false);
}
Metadata inspect_png(Bytes b) {
    Metadata m;m.png=true;std::size_t p=8;bool header=false,data=false,end=false,profile=false,gamma=false,chroma=false,srgb=false;
    bool unusual_gamma=false,unusual_chroma=false;
    while(p<b.size()) {
        require(b.size()-p>=12,"RASTER_INVALID","Truncated PNG chunk");const auto length=be32(b,p);
        require(length<=b.size()-p-12,"RASTER_INVALID","Invalid PNG chunk length");const auto content=b.subspan(p+8,length);
        require(crc(b.subspan(p+4,std::size_t(length)+4))==be32(b,p+8+length),"RASTER_INVALID","PNG chunk CRC mismatch");
        const auto is=[&](const char* s){return signature(b,p+4,s,4);};
        require(header||is("IHDR"),"RASTER_INVALID","PNG must start with IHDR");
        if(is("IHDR")) {
            require(!header&&length==13,"RASTER_INVALID","Invalid or repeated PNG IHDR");header=true;
            m.width=be32(content,0);m.height=be32(content,4);dimensions(m.width,m.height);
            const auto depth=content[8],type=content[9];
            require((type==3&&(depth==1||depth==2||depth==4||depth==8))||
                ((type==0||type==2||type==4||type==6)&&depth==8),"RASTER_PIXEL_FORMAT","Only 8-bit RGB/grayscale and indexed PNG are supported");
            require(content[10]==0&&content[11]==0&&content[12]<=1,"RASTER_INVALID","Unsupported PNG compression/filter/interlace method");
        } else if(is("acTL")||is("fcTL")||is("fdAT"))fail("RASTER_ANIMATED","Animated PNG is unsupported");
        else if(is("eXIf"))fail("RASTER_METADATA","PNG EXIF is unsupported; normalize orientation before import");
        else if(is("iCCP")) {
            require(!profile&&!data,"RASTER_COLOR","Repeated or misplaced PNG color profile");profile=true;m.icc=true;
            const auto zero=std::find(content.begin(),content.end(),0);
            require(zero!=content.end()&&zero!=content.begin()&&zero-content.begin()<=79&&
                std::size_t(zero-content.begin())+2<content.size()&&*(zero+1)==0,"RASTER_COLOR","Malformed PNG embedded color profile");
            m.profile=inflate_profile(content.subspan(std::size_t(zero-content.begin())+2));
        } else if(is("sRGB")) {
            require(!srgb&&!data&&length==1&&content[0]<=3,"RASTER_COLOR","Invalid PNG sRGB declaration");srgb=true;m.srgb=true;
        } else if(is("gAMA")) {
            require(!gamma&&!data&&length==4&&be32(content,0)>0,"RASTER_COLOR","Invalid PNG gamma");gamma=true;unusual_gamma=be32(content,0)!=45455;
        } else if(is("cHRM")) {
            require(!chroma&&!data&&length==32,"RASTER_COLOR","Invalid PNG chromaticities");chroma=true;
            constexpr std::array<std::uint32_t,8> standard{31270,32900,64000,33000,30000,60000,15000,6000};
            for(unsigned i=0;i<8;++i)unusual_chroma=unusual_chroma||be32(content,i*4)!=standard[i];
        } else if(is("IDAT"))data=true;
        else if(is("IEND")) {require(length==0&&data,"RASTER_INVALID","Invalid PNG end");end=true;p+=12;break;}
        p+=std::size_t(length)+12;
    }
    require(header&&end&&p==b.size(),"RASTER_INVALID","PNG is incomplete or has trailing data");
    require(!(profile&&srgb),"RASTER_COLOR","PNG cannot declare both iCCP and sRGB");
    require(m.icc||(!unusual_gamma&&!unusual_chroma),"RASTER_COLOR","Non-sRGB PNG gamma/chromaticities require a usable embedded ICC profile");
    return m;
}
Metadata inspect_jpeg(Bytes b) {
    Metadata m;std::size_t p=2;bool frame=false,scan=false,end=false,seen_exif=false;
    unsigned icc_total=0;std::array<Bytes,256> icc_parts{};
    while(p<b.size()) {
        require(b[p++]==0xff,"RASTER_INVALID","Invalid JPEG marker");
        while(p<b.size()&&b[p]==0xff)++p;require(p<b.size(),"RASTER_INVALID","Truncated JPEG marker");const auto marker=b[p++];
        if(marker==0xd9){end=true;break;}
        require(marker!=0&&marker!=0xd8&&!(marker>=0xd0&&marker<=0xd7),"RASTER_INVALID","Unexpected JPEG marker");
        const auto length=be16(b,p);require(length>=2&&length<=b.size()-p,"RASTER_INVALID","Invalid JPEG segment length");
        const auto content=b.subspan(p+2,length-2);
        if(marker>=0xc0&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc) {
            require(!frame&&content.size()>=6,"RASTER_INVALID","Invalid or repeated JPEG frame");frame=true;
            require((marker==0xc0||marker==0xc1||marker==0xc2)&&content[0]==8&&(content[5]==1||content[5]==3),
                "RASTER_PIXEL_FORMAT","Only 8-bit grayscale/RGB JPEG is supported (CMYK/lossless JPEG is unsupported)");
            m.height=be16(content,1);m.width=be16(content,3);dimensions(m.width,m.height);
        } else if(marker==0xe1&&signature(content,0,"Exif\0\0",6)) {
            require(!seen_exif,"RASTER_METADATA","Repeated JPEG EXIF metadata");seen_exif=true;exif(content.subspan(6),m);
        } else if(marker==0xe2&&signature(content,0,"ICC_PROFILE\0",12)) {
            require(content.size()>14,"RASTER_COLOR","Truncated JPEG ICC segment");const auto index=content[12],total=content[13];
            require(index&&total&&index<=total&&icc_parts[index].empty()&&(!icc_total||icc_total==total),"RASTER_COLOR","Invalid JPEG ICC segment sequence");
            icc_total=total;icc_parts[index]=content.subspan(14);m.icc=true;
        }
        p+=length;
        if(marker==0xda) {
            require(frame,"RASTER_INVALID","JPEG scan precedes frame");scan=true;
            while(p<b.size()) {
                if(b[p++]!=0xff)continue;
                const auto start=p-1;while(p<b.size()&&b[p]==0xff)++p;
                require(p<b.size(),"RASTER_INVALID","Truncated JPEG entropy data");
                if(b[p]==0||(b[p]>=0xd0&&b[p]<=0xd7)){++p;continue;}
                p=start;break;
            }
        }
    }
    require(frame&&scan&&end&&p==b.size(),"RASTER_INVALID","JPEG is incomplete or has trailing data");
    for(unsigned i=1;i<=icc_total;++i) {
        require(!icc_parts[i].empty(),"RASTER_COLOR","Missing JPEG ICC profile segment");
        require(icc_parts[i].size()<=1024*1024-m.profile.size(),"RASTER_COLOR","Embedded ICC profile exceeds the 1 MiB limit");
        m.profile.insert(m.profile.end(),icc_parts[i].begin(),icc_parts[i].end());
    }
    if(m.icc)validate_profile(m.profile);
    require(m.icc||m.exif_color==0||m.exif_color==1||m.exif_color==65535,"RASTER_COLOR","Unsupported EXIF color space without embedded ICC profile");
    m.srgb=m.exif_color==1;return m;
}
Metadata inspect(Bytes b) {
    require(!b.empty()&&b.size()<=raster_source_limit,"RASTER_LIMIT","Original raster bytes must be nonempty and at most 8 MiB");
    if(signature(b,0,"\x89PNG\r\n\x1a\n",8))return inspect_png(b);
    if(b.size()>=2&&b[0]==0xff&&b[1]==0xd8)return inspect_jpeg(b);
    fail("RASTER_FORMAT","Only PNG and JPEG source bytes are supported");
}
struct Apartment {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    Apartment(){if(result!=RPC_E_CHANGED_MODE)hr(result,"Initialize COM","RASTER_PLATFORM");}
    ~Apartment(){if(SUCCEEDED(result))CoUninitialize();}
};
std::string hash(Bytes bytes) {
    struct Algorithm {BCRYPT_ALG_HANDLE handle=nullptr;~Algorithm(){if(handle)BCryptCloseAlgorithmProvider(handle,0);}} algorithm;
    const auto check=[](NTSTATUS result){if(result<0)fail("RASTER_HASH","Windows SHA256 failed");};
    check(BCryptOpenAlgorithmProvider(&algorithm.handle,BCRYPT_SHA256_ALGORITHM,nullptr,0));
    std::array<unsigned char,32> digest{};
    check(BCryptHash(algorithm.handle,nullptr,0,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),digest.data(),static_cast<ULONG>(digest.size())));
    constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);
    for(const auto v:digest){result+=hex[v>>4];result+=hex[v&15];}return result;
}
void validate_profile(Bytes profile) {
    require(profile.size()>=132&&profile.size()<=1024*1024&&be32(profile,0)==profile.size()&&signature(profile,36,"acsp",4),
        "RASTER_COLOR","Invalid or oversized ICC profile");
    require((profile[8]==2||profile[8]==4)&&(signature(profile,16,"RGB ",4)||signature(profile,16,"GRAY",4))&&
        (signature(profile,20,"XYZ ",4)||signature(profile,20,"Lab ",4)),"RASTER_COLOR","Only standard v2/v4 RGB or gray ICC profiles are supported");
    const auto count=be32(profile,128);require(count<=4096&&132ULL+12ULL*count<=profile.size(),"RASTER_COLOR","Invalid ICC tag directory");
    for(std::uint32_t i=0;i<count;++i){const auto p=132+12*std::size_t(i);const auto offset=be32(profile,p+4),size=be32(profile,p+8);
        require(offset<=profile.size()&&size<=profile.size()-offset,"RASTER_COLOR","ICC tag extends outside its profile");}
}
RasterPixels oriented(RasterPixels source,unsigned orientation) {
    if(orientation==1)return source;
    RasterPixels result;result.width=orientation>=5?source.height:source.width;result.height=orientation>=5?source.width:source.height;result.rgba.resize(source.rgba.size());
    for(std::uint32_t y=0;y<source.height;++y)for(std::uint32_t x=0;x<source.width;++x) {
        std::uint32_t dx=x,dy=y;
        switch(orientation) {
        case 2:dx=source.width-1-x;break;
        case 3:dx=source.width-1-x;dy=source.height-1-y;break;
        case 4:dy=source.height-1-y;break;
        case 5:dx=y;dy=x;break;
        case 6:dx=source.height-1-y;dy=x;break;
        case 7:dx=source.height-1-y;dy=source.width-1-x;break;
        case 8:dx=y;dy=source.width-1-x;break;
        }
        std::copy_n(source.rgba.data()+(std::size_t(y)*source.width+x)*4,4,result.rgba.data()+(std::size_t(dy)*result.width+dx)*4);
    }return result;
}
struct Decoded {RasterPixels pixels;std::string color;};
Decoded decode(Bytes bytes,const Metadata& m) {
    Apartment apartment;ComPtr<IWICImagingFactory> factory;
    hr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Create WIC factory");
    ComPtr<IWICStream> stream;hr(factory->CreateStream(&stream),"Create memory stream");
    // The Microsoft decoder only reads this stream; no stream writer is exposed.
    hr(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),static_cast<DWORD>(bytes.size())),"Initialize memory stream");
    ComPtr<IWICBitmapDecoder> decoder;
    hr(CoCreateInstance(m.png?CLSID_WICPngDecoder:CLSID_WICJpegDecoder,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&decoder)),"Create Microsoft PNG/JPEG decoder");
    hr(decoder->Initialize(stream.Get(),WICDecodeMetadataCacheOnDemand),"Decode image header");
    UINT frames=0;hr(decoder->GetFrameCount(&frames),"Count image frames");require(frames==1,"RASTER_ANIMATED","Only single-frame PNG/JPEG is supported");
    ComPtr<IWICBitmapFrameDecode> frame;hr(decoder->GetFrame(0,&frame),"Decode image frame");
    UINT width=0,height=0;hr(frame->GetSize(&width,&height),"Read raster dimensions");dimensions(width,height);
    require(width==m.width&&height==m.height,"RASTER_INVALID","Decoder dimensions disagree with encoded header");
    WICPixelFormatGUID native{};hr(frame->GetPixelFormat(&native),"Read raster pixel format");
    const std::array supported{GUID_WICPixelFormat1bppIndexed,GUID_WICPixelFormat2bppIndexed,GUID_WICPixelFormat4bppIndexed,
        GUID_WICPixelFormat8bppIndexed,GUID_WICPixelFormat8bppGray,GUID_WICPixelFormat24bppBGR,GUID_WICPixelFormat24bppRGB,
        GUID_WICPixelFormat32bppBGRA,GUID_WICPixelFormat32bppRGBA,GUID_WICPixelFormat32bppBGR};
    require(std::any_of(supported.begin(),supported.end(),[&](const auto& format){return IsEqualGUID(format,native);}),
        "RASTER_PIXEL_FORMAT","Decoder returned an unsupported pixel format");
    ComPtr<IWICFormatConverter> rgba;hr(factory->CreateFormatConverter(&rgba),"Create RGBA converter");
    hr(rgba->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Convert to straight RGBA8");
    RasterPixels pixels{width,height,std::vector<unsigned char>(std::size_t(width)*height*4)};
    hr(rgba->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.rgba.size()),pixels.rgba.data()),"Decode all pixels");
    std::string interpretation=m.srgb?"declared_srgb":"assumed_srgb";
    if(m.icc) {
        // Profile extraction is bounded before WIC sees the stream. Do not ask
        // WIC to allocate an unbounded decompressed PNG iCCP metadata value.
        ComPtr<IWICColorContext> source;hr(factory->CreateColorContext(&source),"Create source ICC context","RASTER_COLOR");
        hr(source->InitializeFromMemory(m.profile.data(),static_cast<UINT>(m.profile.size())),"Initialize embedded ICC context","RASTER_COLOR");
        ComPtr<IWICColorContext> destination;hr(factory->CreateColorContext(&destination),"Create sRGB context","RASTER_COLOR");
        hr(destination->InitializeFromExifColorSpace(1),"Initialize sRGB context","RASTER_COLOR");
        ComPtr<IWICColorTransform> transform;hr(factory->CreateColorTransformer(&transform),"Create ICC transform","RASTER_COLOR");
        hr(transform->Initialize(frame.Get(),source.Get(),destination.Get(),GUID_WICPixelFormat32bppRGBA),"Initialize ICC to sRGB transform","RASTER_COLOR");
        std::vector<unsigned char> converted(pixels.rgba.size());
        hr(transform->CopyPixels(nullptr,width*4,static_cast<UINT>(converted.size()),converted.data()),"Convert ICC pixels to sRGB","RASTER_COLOR");
        // Color conversion must not alter coverage, including fully transparent RGB.
        for(std::size_t i=3;i<converted.size();i+=4)converted[i]=pixels.rgba[i];pixels.rgba=std::move(converted);
        interpretation="embedded_icc_to_srgb";
    }
    return {oriented(std::move(pixels),m.orientation),std::move(interpretation)};
}
}

Raster make_raster(std::vector<unsigned char> bytes) {
    const auto metadata=inspect(bytes);const auto decoded=decode(bytes,metadata);const auto sha=hash(bytes);
    return Raster(new RasterPayload(std::move(bytes),sha,metadata.png?"image/png":"image/jpeg",decoded.pixels.width,
        decoded.pixels.height,metadata.orientation,decoded.color));
}
RasterPixels decode_raster(const RasterPayload& payload) {
    const auto metadata=inspect(payload.bytes());auto decoded=decode(payload.bytes(),metadata);
    require(decoded.pixels.width==payload.width()&&decoded.pixels.height==payload.height()&&decoded.color==payload.color_interpretation(),
        "RASTER_INTERPRETATION","Accepted raster interpretation changed");
    return std::move(decoded.pixels);
}
std::vector<unsigned char> encode_raster_png(const RasterPixels& pixels) {
    dimensions(pixels.width,pixels.height);
    require(pixels.rgba.size()==std::size_t(pixels.width)*pixels.height*4,"RASTER_INVALID","RGBA pixel buffer size does not match dimensions");
    Apartment apartment;ComPtr<IWICImagingFactory> factory;
    hr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Create WIC factory","RASTER_ENCODE");
    std::vector<unsigned char> output(raster_png_limit);ComPtr<IWICStream> stream;
    hr(factory->CreateStream(&stream),"Create PNG output stream","RASTER_ENCODE");
    hr(stream->InitializeFromMemory(output.data(),static_cast<DWORD>(output.size())),"Initialize bounded PNG output","RASTER_ENCODE");
    ComPtr<IWICBitmapEncoder> encoder;hr(CoCreateInstance(CLSID_WICPngEncoder,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&encoder)),"Create Microsoft PNG encoder","RASTER_ENCODE");
    hr(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Initialize PNG encoder","RASTER_ENCODE");
    ComPtr<IWICBitmapFrameEncode> frame;hr(encoder->CreateNewFrame(&frame,nullptr),"Create PNG frame","RASTER_ENCODE");
    hr(frame->Initialize(nullptr),"Initialize PNG frame","RASTER_ENCODE");hr(frame->SetSize(pixels.width,pixels.height),"Set PNG size","RASTER_ENCODE");
    auto format=GUID_WICPixelFormat32bppBGRA;hr(frame->SetPixelFormat(&format),"Set PNG pixel format","RASTER_ENCODE");
    require(IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA),"RASTER_ENCODE","PNG encoder did not preserve 8-bit RGBA");
    std::vector<unsigned char> row(std::size_t(pixels.width)*4);
    for(std::uint32_t y=0;y<pixels.height;++y) {
        const auto* source=pixels.rgba.data()+std::size_t(y)*row.size();
        for(std::uint32_t x=0;x<pixels.width;++x){row[x*4]=source[x*4+2];row[x*4+1]=source[x*4+1];row[x*4+2]=source[x*4];row[x*4+3]=source[x*4+3];}
        hr(frame->WritePixels(1,pixels.width*4,static_cast<UINT>(row.size()),row.data()),"Write bounded PNG pixels (maximum 16 MiB)","RASTER_ENCODE");
    }
    hr(frame->Commit(),"Commit PNG frame","RASTER_ENCODE");hr(encoder->Commit(),"Commit PNG stream","RASTER_ENCODE");
    ULARGE_INTEGER position{};LARGE_INTEGER zero{};hr(stream->Seek(zero,STREAM_SEEK_CUR,&position),"Read PNG output size","RASTER_ENCODE");
    require(position.QuadPart>0&&position.QuadPart<=raster_png_limit,"RASTER_LIMIT","Normalized PNG exceeds 16 MiB");
    output.resize(static_cast<std::size_t>(position.QuadPart));return output;
}
#else
Raster make_raster(std::vector<unsigned char>) {throw Error("RASTER_PLATFORM_UNSUPPORTED","PNG/JPEG raster interpretation requires the Windows WIC backend");}
RasterPixels decode_raster(const RasterPayload&) {throw Error("RASTER_PLATFORM_UNSUPPORTED","PNG/JPEG raster interpretation requires the Windows WIC backend");}
std::vector<unsigned char> encode_raster_png(const RasterPixels&) {throw Error("RASTER_PLATFORM_UNSUPPORTED","PNG/JPEG raster interpretation requires the Windows WIC backend");}
#endif
}
