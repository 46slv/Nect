// DirectWrite shapes active OS font references. No file/table/font-byte access.
#define NOMINMAX
#include <windows.h>
#include <dwrite_3.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
using Microsoft::WRL::ComPtr;
namespace {
std::wstring localized(IDWriteLocalizedStrings* names) {
    if (!names || !names->GetCount()) return {};
    UINT32 index=0,length=0; BOOL exists=FALSE;
    if (FAILED(names->FindLocaleName(L"en-us",&index,&exists)) || !exists) index=0;
    if (FAILED(names->GetStringLength(index,&length)) || length>255) return {};
    std::vector<wchar_t> value(length+1);
    if (FAILED(names->GetString(index,value.data(),length+1))) return {};
    return value.data();
}
HRESULT system(ComPtr<IDWriteFactory3>& factory,ComPtr<IDWriteFontCollection>& fonts) {
    HRESULT hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
    if (FAILED(hr)) return hr;
    return static_cast<IDWriteFactory*>(factory.Get())->GetSystemFontCollection(&fonts,TRUE);
}
std::wstring familyName(IDWriteFontFamily* family) {
    ComPtr<IDWriteLocalizedStrings> names;
    return SUCCEEDED(family->GetFamilyNames(&names))?localized(names.Get()):L"";
}
std::wstring faceName(IDWriteFont* font) {
    ComPtr<IDWriteLocalizedStrings> names;
    return SUCCEEDED(font->GetFaceNames(&names))?localized(names.Get()):L"";
}
struct NativePoint { float x,y; };
struct Geometry { std::vector<NativePoint> points;std::vector<unsigned char> types;bool failed=false;HRESULT failure=S_OK;UINT32 glyphs=0; };
class Sink final:public IDWriteGeometrySink {
    LONG refs=1;Geometry& out;float x,y;
    void point(float px,float py,unsigned char type) {
        if(out.points.size()>=300000 || !std::isfinite(px+x) || !std::isfinite(py+y) || std::abs(px+x)>10000000 || std::abs(py+y)>10000000){out.failed=true;return;}
        out.points.push_back({px+x,py+y});out.types.push_back(type);
    }
public:
    Sink(Geometry& out,float x,float y):out(out),x(x),y(y){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** object)override{
        if(!object)return E_POINTER;*object=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IDWriteGeometrySink)){*object=static_cast<IDWriteGeometrySink*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release()override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE mode)override{if(mode!=D2D1_FILL_MODE_WINDING)out.failed=true;}
    void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT)override{}
    void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F p,D2D1_FIGURE_BEGIN)override{point(p.x,p.y,0);}
    void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F* p,UINT32 n)override{for(UINT32 i=0;i<n;++i)point(p[i].x,p[i].y,1);}
    void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT* p,UINT32 n)override{for(UINT32 i=0;i<n;++i){point(p[i].point1.x,p[i].point1.y,3);point(p[i].point2.x,p[i].point2.y,3);point(p[i].point3.x,p[i].point3.y,3);}}
    void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END end)override{if(end!=D2D1_FIGURE_END_CLOSED||out.types.empty())out.failed=true;else out.types.back()|=128;}
    HRESULT STDMETHODCALLTYPE Close()override{return out.failed?E_FAIL:S_OK;}
};
class Renderer final:public IDWriteTextRenderer {
    LONG refs=1;ComPtr<IDWriteFontFaceReference> expected;Geometry& out;
    HRESULT refuse(HRESULT error){if(SUCCEEDED(out.failure))out.failure=error;return error;}
public:
    Renderer(IDWriteFontFaceReference* expected,Geometry& out):expected(expected),out(out){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** object)override{
        if(!object)return E_POINTER;*object=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IDWriteTextRenderer)||id==__uuidof(IDWritePixelSnapping)){*object=static_cast<IDWriteTextRenderer*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release()override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,BOOL* value)override{*value=TRUE;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,DWRITE_MATRIX* matrix)override{*matrix={1,0,0,1,0,0};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*,FLOAT* value)override{*value=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE,const DWRITE_GLYPH_RUN* run,const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown*)override{
        ComPtr<IDWriteFontFace3> face;ComPtr<IDWriteFontFaceReference> actual;
        if(FAILED(run->fontFace->QueryInterface(IID_PPV_ARGS(&face)))||FAILED(face->GetFontFaceReference(&actual))||!expected->Equals(actual.Get())){
            if(_wgetenv(L"NECT_DESKTOP_FONT_TRACE")){
                ComPtr<IDWriteFontFace3> wanted;ComPtr<IDWriteLocalizedStrings> wantNames,gotNames;
                expected->CreateFontFace(&wanted);if(wanted)wanted->GetFamilyNames(&wantNames);if(face)face->GetFamilyNames(&gotNames);
                fwprintf(stderr,L"FACE_REFUSAL expected=%ls actual=%ls glyphs=%u\n",localized(wantNames.Get()).c_str(),localized(gotNames.Get()).c_str(),run->glyphCount);
            }
            return refuse(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        }
        if(face->IsColorFont())return refuse(E_NOTIMPL);
        if(out.glyphs+run->glyphCount>20000)return refuse(E_BOUNDS);
        for(UINT32 i=0;i<run->glyphCount;++i)if(!run->glyphIndices[i])return refuse(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        out.glyphs+=run->glyphCount;
        ComPtr<Sink> sink; sink.Attach(new Sink(out,x,y));
        HRESULT hr=run->fontFace->GetGlyphRunOutline(run->fontEmSize,run->glyphIndices,run->glyphAdvances,run->glyphOffsets,run->glyphCount,run->isSideways,(run->bidiLevel&1)!=0,sink.Get());
        hr=FAILED(hr)?hr:sink->Close();return FAILED(hr)?refuse(hr):hr;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,const DWRITE_UNDERLINE*,IUnknown*)override{return refuse(E_NOTIMPL);}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,const DWRITE_STRIKETHROUGH*,IUnknown*)override{return refuse(E_NOTIMPL);}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*)override{return refuse(E_NOTIMPL);}
};
}
using DesktopFaceCallback=void(*)(const wchar_t*,const wchar_t*,int,int,int,int,int,void*);
extern "C" int nect_list_desktop_font_faces(DesktopFaceCallback callback,void* data){
    ComPtr<IDWriteFactory3> factory;ComPtr<IDWriteFontCollection> fonts;HRESULT hr=system(factory,fonts);if(FAILED(hr))return hr;
    UINT32 total=0;
    for(UINT32 i=0;i<fonts->GetFontFamilyCount()&&total<8192;++i){
        ComPtr<IDWriteFontFamily> family;if(FAILED(fonts->GetFontFamily(i,&family)))continue;
        auto name=familyName(family.Get());if(name.empty())continue;
        for(UINT32 j=0;j<family->GetFontCount()&&total<8192;++j){
            ComPtr<IDWriteFont> font;ComPtr<IDWriteFontFace> face;ComPtr<IDWriteFontFace2> face2;
            if(FAILED(family->GetFont(j,&font))||font->GetSimulations()!=DWRITE_FONT_SIMULATIONS_NONE||FAILED(font->CreateFontFace(&face)))continue;
            auto label=faceName(font.Get());if(label.empty())continue;
            bool color=SUCCEEDED(face.As(&face2))&&face2->IsColorFont();
            callback(name.c_str(),label.c_str(),font->GetWeight(),font->GetStyle(),font->GetStretch(),face->GetType(),color?1:0,data);++total;
        }
    }
    return 0;
}
using DesktopPathCallback=void(*)(const NativePoint*,const unsigned char*,int,float,float,int,int,void*);
extern "C" int nect_shape_desktop_text(const wchar_t* familyNameInput,const wchar_t* faceNameInput,int weight,int style,int stretch,
    const wchar_t* text,int length,float size,float tracking,float lineSpacing,int kerning,int ligatures,int alignment,DesktopPathCallback callback,void* data){
    if(length<0||length>4096||!std::isfinite(size)||size<0.01f||size>10000||!std::isfinite(tracking)||std::abs(tracking)>10000||!std::isfinite(lineSpacing)||lineSpacing<0.1f||lineSpacing>100||style<0||style>2||weight<1||weight>999||stretch<1||stretch>9||alignment<0||alignment>2)return E_INVALIDARG;
    ComPtr<IDWriteFactory3> factory;ComPtr<IDWriteFontCollection> fonts;HRESULT hr=system(factory,fonts);if(FAILED(hr))return hr;
    UINT32 familyIndex=0;BOOL exists=FALSE;if(FAILED(fonts->FindFamilyName(familyNameInput,&familyIndex,&exists))||!exists)return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    ComPtr<IDWriteFontFamily> family;fonts->GetFontFamily(familyIndex,&family);ComPtr<IDWriteFont> selected;
    for(UINT32 i=0;i<family->GetFontCount();++i){ComPtr<IDWriteFont> font;family->GetFont(i,&font);
        if(font->GetWeight()==weight&&font->GetStyle()==style&&font->GetStretch()==stretch&&font->GetSimulations()==DWRITE_FONT_SIMULATIONS_NONE&&faceName(font.Get())==faceNameInput){selected=font;break;}}
    if(!selected)return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    ComPtr<IDWriteFontFace> baseFace;ComPtr<IDWriteFontFace3> face;ComPtr<IDWriteFontFaceReference> reference;
    if(FAILED(selected->CreateFontFace(&baseFace))||FAILED(baseFace.As(&face))||FAILED(face->GetFontFaceReference(&reference)))return E_FAIL;
    if(face->IsColorFont())return E_NOTIMPL;
    // A collection containing only this active face keeps matching exact. Any
    // fallback run is still rejected by the renderer's face-reference equality.
    ComPtr<IDWriteFontSetBuilder> builder;ComPtr<IDWriteFontSet> set;ComPtr<IDWriteFontCollection1> exactCollection;
    if(FAILED(factory->CreateFontSetBuilder(&builder))||FAILED(builder->AddFontFaceReference(reference.Get()))||FAILED(builder->CreateFontSet(&set))||FAILED(factory->CreateFontCollectionFromFontSet(set.Get(),&exactCollection)))return E_FAIL;
    ComPtr<IDWriteFontFamily> exactFamily;exactCollection->GetFontFamily(0,&exactFamily);auto exactName=familyName(exactFamily.Get());
    ComPtr<IDWriteTextFormat> format;
    hr=factory->CreateTextFormat(exactName.c_str(),exactCollection.Get(),static_cast<DWRITE_FONT_WEIGHT>(weight),static_cast<DWRITE_FONT_STYLE>(style),static_cast<DWRITE_FONT_STRETCH>(stretch),size,L"ja-jp",&format);if(FAILED(hr))return hr;
    DWRITE_FONT_METRICS metrics;face->GetMetrics(&metrics);float ascent=size*metrics.ascent/metrics.designUnitsPerEm;
    if(FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP))||FAILED(format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,size*lineSpacing,ascent)))return E_INVALIDARG;
    ComPtr<IDWriteTextLayout> layout;
    hr=factory->CreateTextLayout(text,length,format.Get(),1000000,1000000,&layout);if(FAILED(hr))return hr;
    ComPtr<IDWriteTextLayout1> layout1;if(FAILED(layout.As(&layout1)))return E_NOTIMPL;
    DWRITE_TEXT_RANGE range={0,static_cast<UINT32>(length)};
    if(FAILED(layout1->SetCharacterSpacing(0,tracking,0,range))||FAILED(layout1->SetPairKerning(kerning!=0,range)))return E_INVALIDARG;
    ComPtr<IDWriteTypography> typography;factory->CreateTypography(&typography);
    typography->AddFontFeature({DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES,ligatures?1u:0u});
    if(FAILED(layout->SetTypography(typography.Get(),range)))return E_FAIL;
    DWRITE_TEXT_METRICS textMetrics;if(FAILED(layout->GetMetrics(&textMetrics))||textMetrics.lineCount>256)return E_BOUNDS;
    layout->SetMaxWidth(std::max(1.0f,textMetrics.widthIncludingTrailingWhitespace));
    layout->SetTextAlignment(alignment==1?DWRITE_TEXT_ALIGNMENT_CENTER:alignment==2?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_LEADING);
    Geometry output;ComPtr<Renderer> renderer;renderer.Attach(new Renderer(reference.Get(),output));
    hr=layout->Draw(nullptr,renderer.Get(),0,0);if(FAILED(output.failure))return output.failure;if(FAILED(hr)||output.failed)return FAILED(hr)?hr:E_BOUNDS;
    UINT32 lineCount=0;layout->GetLineMetrics(nullptr,0,&lineCount);std::vector<DWRITE_LINE_METRICS> lines(lineCount);
    if(lineCount&&SUCCEEDED(layout->GetLineMetrics(lines.data(),lineCount,&lineCount)))ascent=lines[0].baseline;
    callback(output.points.data(),output.types.data(),static_cast<int>(output.points.size()),ascent,textMetrics.widthIncludingTrailingWhitespace,static_cast<int>(textMetrics.lineCount),static_cast<int>(output.glyphs),data);
    return 0;
}
