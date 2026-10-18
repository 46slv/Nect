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
using GdiFamilyCallback=void(*)(const wchar_t*,int,void*);
using DesktopAxisCallback=void(*)(UINT32,const wchar_t*,float,float,float,float,float,int,void*);
extern "C" int nect_list_os_fonts(GdiFamilyCallback callback,void* data);
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
    // Shared factory creation dropped GDI-only families in this process on the
    // Windows acceptance host. An isolated factory preserves both catalogs.
    HRESULT hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
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
HRESULT gdiFace(IDWriteFactory3* factory,const wchar_t* family,int weight,int style,ComPtr<IDWriteFontFace3>& face) {
    LOGFONTW logical={};logical.lfHeight=-100;logical.lfWeight=weight;
    logical.lfItalic=style!=DWRITE_FONT_STYLE_NORMAL;logical.lfCharSet=DEFAULT_CHARSET;
    if(wcslen(family)>=LF_FACESIZE)return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    wcscpy_s(logical.lfFaceName,family);
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return E_FAIL;
    HFONT font=CreateFontIndirectW(&logical);if(!font){DeleteDC(dc);return E_FAIL;}
    HGDIOBJ prior=SelectObject(dc,font);HRESULT result=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    if(prior&&prior!=HGDI_ERROR){
        wchar_t actual[LF_FACESIZE]={};
        if(GetTextFaceW(dc,LF_FACESIZE,actual)>0&&lstrcmpiW(actual,family)==0){
            ComPtr<IDWriteGdiInterop> interop;ComPtr<IDWriteFontFace> base;
            result=factory->GetGdiInterop(&interop);
            if(SUCCEEDED(result))result=interop->CreateFontFaceFromHdc(dc,&base);
            if(SUCCEEDED(result))result=base.As(&face);
            if(SUCCEEDED(result)&&face->GetSimulations()!=DWRITE_FONT_SIMULATIONS_NONE)result=E_NOTIMPL;
        }
        SelectObject(dc,prior);
    }
    DeleteObject(font);DeleteDC(dc);return result;
}
HRESULT resolveFace(IDWriteFactory3* factory,IDWriteFontCollection* fonts,const wchar_t* familyInput,const wchar_t* faceInput,int weight,int style,int stretch,ComPtr<IDWriteFontFace3>& face){
    UINT32 index=0;BOOL exists=FALSE;
    if(SUCCEEDED(fonts->FindFamilyName(familyInput,&index,&exists))&&exists){
        ComPtr<IDWriteFontFamily> family;fonts->GetFontFamily(index,&family);
        for(UINT32 i=0;i<family->GetFontCount();++i){
            ComPtr<IDWriteFont> font;family->GetFont(i,&font);
            if(font->GetWeight()==weight&&font->GetStyle()==style&&font->GetStretch()==stretch&&font->GetSimulations()==DWRITE_FONT_SIMULATIONS_NONE&&faceName(font.Get())==faceInput){
                ComPtr<IDWriteFontFace> base;HRESULT hr=font->CreateFontFace(&base);return FAILED(hr)?hr:base.As(&face);
            }
        }
    }
    HRESULT hr=gdiFace(factory,familyInput,weight,style,face);if(FAILED(hr))return hr;
    ComPtr<IDWriteLocalizedStrings> names;face->GetFaceNames(&names);
    if(face->GetWeight()!=weight||face->GetStyle()!=style||face->GetStretch()!=stretch||localized(names.Get())!=faceInput)return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    return S_OK;
}
struct AxisInfo {UINT32 tag;std::wstring name;float minimum,maximum,defaultValue,baseValue,value;bool hidden;};
HRESULT readAxes(IDWriteFontFace3* face,std::vector<AxisInfo>& result){
    ComPtr<IDWriteFontFace5> variable;if(FAILED(face->QueryInterface(IID_PPV_ARGS(&variable)))||!variable->HasVariations())return S_OK;
    ComPtr<IDWriteFontResource> resource;HRESULT hr=variable->GetFontResource(&resource);if(FAILED(hr))return hr;
    UINT32 count=resource->GetFontAxisCount(),currentCount=variable->GetFontAxisValueCount();
    if(count>32||currentCount>32)return E_BOUNDS;
    std::vector<DWRITE_FONT_AXIS_RANGE> ranges(count);std::vector<DWRITE_FONT_AXIS_VALUE> defaults(count),current(currentCount);
    if(FAILED(resource->GetFontAxisRanges(ranges.data(),count))||FAILED(resource->GetDefaultFontAxisValues(defaults.data(),count))||FAILED(variable->GetFontAxisValues(current.data(),currentCount)))return E_FAIL;
    for(UINT32 i=0;i<count;++i){
        auto range=ranges[i];if(range.minValue==range.maxValue)continue;
        auto initial=std::find_if(current.begin(),current.end(),[&](const DWRITE_FONT_AXIS_VALUE& v){return v.axisTag==range.axisTag;});
        auto def=std::find_if(defaults.begin(),defaults.end(),[&](const DWRITE_FONT_AXIS_VALUE& v){return v.axisTag==range.axisTag;});
        if(initial==current.end()||def==defaults.end())return E_FAIL;
        ComPtr<IDWriteLocalizedStrings> names;resource->GetAxisNames(i,&names);auto name=localized(names.Get());
        if(name.empty())for(int byte=0;byte<4;++byte)name.push_back(static_cast<wchar_t>((range.axisTag>>(8*byte))&255));
        result.push_back({range.axisTag,name,range.minValue,range.maxValue,def->value,initial->value,initial->value,(resource->GetFontAxisAttributes(i)&DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN)!=0});
    }
    return S_OK;
}
void emitAxes(const std::vector<AxisInfo>& axes,DesktopAxisCallback callback,void* data){
    for(auto& a:axes)callback(a.tag,a.name.c_str(),a.minimum,a.maximum,a.defaultValue,a.baseValue,a.value,a.hidden?1:0,data);
}
struct NativePoint { float x,y; };
struct Geometry { std::vector<NativePoint> points;std::vector<unsigned char> types;bool failed=false;HRESULT failure=S_OK;UINT32 glyphs=0; };
class Sink final:public IDWriteGeometrySink {
    LONG refs=1;Geometry& out;float x,y;DWRITE_MATRIX transform;
    void point(float px,float py,unsigned char type) {
        float ax=px+x,ay=py+y,tx=ax*transform.m11+ay*transform.m21+transform.dx,ty=ax*transform.m12+ay*transform.m22+transform.dy;
        if(out.points.size()>=300000 || !std::isfinite(tx) || !std::isfinite(ty) || std::abs(tx)>10000000 || std::abs(ty)>10000000){out.failed=true;return;}
        out.points.push_back({tx,ty});out.types.push_back(type);
    }
public:
    Sink(Geometry& out,float x,float y,DWRITE_MATRIX transform):out(out),x(x),y(y),transform(transform){}
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
class Renderer final:public IDWriteTextRenderer1 {
    LONG refs=1;ComPtr<IDWriteFontFaceReference> expected;ComPtr<IDWriteFontFace5> expectedFace;ComPtr<IDWriteTextAnalyzer2> analyzer;Geometry& out;
    HRESULT refuse(HRESULT error){if(SUCCEEDED(out.failure))out.failure=error;return error;}
public:
    Renderer(IDWriteFontFace3* face,IDWriteFontFaceReference* expected,IDWriteTextAnalyzer2* analyzer,Geometry& out):expected(expected),analyzer(analyzer),out(out){face->QueryInterface(IID_PPV_ARGS(&expectedFace));}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** object)override{
        if(!object)return E_POINTER;*object=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IDWriteTextRenderer)||id==__uuidof(IDWriteTextRenderer1)||id==__uuidof(IDWritePixelSnapping)){*object=static_cast<IDWriteTextRenderer1*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release()override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,BOOL* value)override{*value=TRUE;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,DWRITE_MATRIX* matrix)override{*matrix={1,0,0,1,0,0};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*,FLOAT* value)override{*value=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE,const DWRITE_GLYPH_RUN* run,const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown*)override{
        return draw(x,y,run,{1,0,0,1,0,0});
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT x,FLOAT y,DWRITE_GLYPH_ORIENTATION_ANGLE angle,DWRITE_MEASURING_MODE,const DWRITE_GLYPH_RUN* run,const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown*)override{
        DWRITE_MATRIX transform={1,0,0,1,0,0};
        if(analyzer){HRESULT hr=analyzer->GetGlyphOrientationTransform(angle,run->isSideways,x,y,&transform);if(FAILED(hr))return refuse(hr);}
        else if(angle!=DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES)return refuse(E_NOTIMPL);
        return draw(x,y,run,transform);
    }
    HRESULT draw(FLOAT x,FLOAT y,const DWRITE_GLYPH_RUN* run,DWRITE_MATRIX transform){
        if(!run->glyphCount)return S_OK;
        ComPtr<IDWriteFontFace3> face;ComPtr<IDWriteFontFaceReference> actual;
        if(FAILED(run->fontFace->QueryInterface(IID_PPV_ARGS(&face)))||
           (expectedFace?!expectedFace->Equals(face.Get()):(FAILED(face->GetFontFaceReference(&actual))||!expected->Equals(actual.Get())))){
            if(_wgetenv(L"NECT_DESKTOP_FONT_TRACE")){
                ComPtr<IDWriteFontFace3> wanted;ComPtr<IDWriteLocalizedStrings> wantNames,gotNames;
                expected->CreateFontFace(&wanted);if(wanted)wanted->GetFamilyNames(&wantNames);if(face)face->GetFamilyNames(&gotNames);
                fwprintf(stderr,L"FACE_REFUSAL expected=%ls actual=%ls glyphs=%u\n",localized(wantNames.Get()).c_str(),localized(gotNames.Get()).c_str(),run->glyphCount);
                if(wanted&&face){
                    ComPtr<IDWriteFontFace5> wanted5;ComPtr<IDWriteLocalizedStrings> wantFace,gotFace;
                    wanted->GetFaceNames(&wantFace);face->GetFaceNames(&gotFace);
                    fwprintf(stderr,L"FACE_ID expected=%ls weight=%u style=%u stretch=%u sim=%u actual=%ls weight=%u style=%u stretch=%u sim=%u face5equals=%d\n",
                        localized(wantFace.Get()).c_str(),wanted->GetWeight(),wanted->GetStyle(),wanted->GetStretch(),wanted->GetSimulations(),
                        localized(gotFace.Get()).c_str(),face->GetWeight(),face->GetStyle(),face->GetStretch(),face->GetSimulations(),
                        SUCCEEDED(wanted.As(&wanted5))?wanted5->Equals(face.Get()):-1);
                }
            }
            return refuse(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        }
        if(face->IsColorFont())return refuse(E_NOTIMPL);
        if(out.glyphs+run->glyphCount>20000)return refuse(E_BOUNDS);
        for(UINT32 i=0;i<run->glyphCount;++i)if(!run->glyphIndices[i])return refuse(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        out.glyphs+=run->glyphCount;
        ComPtr<Sink> sink; sink.Attach(new Sink(out,x,y,transform));
        HRESULT hr=run->fontFace->GetGlyphRunOutline(run->fontEmSize,run->glyphIndices,run->glyphAdvances,run->glyphOffsets,run->glyphCount,run->isSideways,(run->bidiLevel&1)!=0,sink.Get());
        hr=FAILED(hr)?hr:sink->Close();return FAILED(hr)?refuse(hr):hr;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,const DWRITE_UNDERLINE*,IUnknown*)override{return refuse(E_NOTIMPL);}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,const DWRITE_STRIKETHROUGH*,IUnknown*)override{return refuse(E_NOTIMPL);}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*)override{return refuse(E_NOTIMPL);}
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,const DWRITE_UNDERLINE*,IUnknown*)override{return refuse(E_NOTIMPL);}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,const DWRITE_STRIKETHROUGH*,IUnknown*)override{return refuse(E_NOTIMPL);}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,IDWriteInlineObject*,BOOL,BOOL,IUnknown*)override{return refuse(E_NOTIMPL);}
};
}
using DesktopFaceCallback=void(*)(const wchar_t*,const wchar_t*,int,int,int,int,int,void*);
extern "C" int nect_list_desktop_font_axes(const wchar_t* family,const wchar_t* name,int weight,int style,int stretch,DesktopAxisCallback callback,void* data){
    ComPtr<IDWriteFactory3> factory;ComPtr<IDWriteFontCollection> fonts;HRESULT hr=system(factory,fonts);if(FAILED(hr))return hr;
    ComPtr<IDWriteFontFace3> face;hr=resolveFace(factory.Get(),fonts.Get(),family,name,weight,style,stretch,face);if(FAILED(hr))return hr;
    std::vector<AxisInfo> axes;hr=readAxes(face.Get(),axes);if(FAILED(hr))return hr;emitAxes(axes,callback,data);return S_OK;
}
extern "C" int nect_list_desktop_font_faces(DesktopFaceCallback callback,void* data){
    ComPtr<IDWriteFactory3> factory;ComPtr<IDWriteFontCollection> fonts;HRESULT hr=system(factory,fonts);if(FAILED(hr))return hr;
    UINT32 total=0;std::vector<std::wstring> normalFamilies;
    for(UINT32 i=0;i<fonts->GetFontFamilyCount()&&total<8192;++i){
        ComPtr<IDWriteFontFamily> family;if(FAILED(fonts->GetFontFamily(i,&family)))continue;
        auto name=familyName(family.Get());if(name.empty())continue;
        normalFamilies.push_back(name);
        for(UINT32 j=0;j<family->GetFontCount()&&total<8192;++j){
            ComPtr<IDWriteFont> font;ComPtr<IDWriteFontFace> face;ComPtr<IDWriteFontFace2> face2;
            if(FAILED(family->GetFont(j,&font))||font->GetSimulations()!=DWRITE_FONT_SIMULATIONS_NONE||FAILED(font->CreateFontFace(&face)))continue;
            auto label=faceName(font.Get());if(label.empty())continue;
            bool color=SUCCEEDED(face.As(&face2))&&face2->IsColorFont();
            callback(name.c_str(),label.c_str(),font->GetWeight(),font->GetStyle(),font->GetStretch(),face->GetType(),color?1:0,data);++total;
        }
    }
    struct ExtraFonts {IDWriteFactory3* factory;const std::vector<std::wstring>& normal;DesktopFaceCallback callback;void* data;UINT32& count;};
    ExtraFonts extra={factory.Get(),normalFamilies,callback,data,total};
    int gdiStatus=nect_list_os_fonts([](const wchar_t* name,int styles,void* pointer){
        auto& c=*static_cast<ExtraFonts*>(pointer);
        if(std::find(c.normal.begin(),c.normal.end(),name)!=c.normal.end())return;
        for(int style=0;style<=3&&c.count<8192;++style){
            if(!(styles&(1<<style)))continue;
            ComPtr<IDWriteFontFace3> face;HRESULT result=gdiFace(c.factory,name,(style&1)?700:400,(style&2)?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,face);
            if(FAILED(result))continue;
            ComPtr<IDWriteFontFaceReference> reference;if(FAILED(face->GetFontFaceReference(&reference)))continue;
            ComPtr<IDWriteLocalizedStrings> families,names;face->GetFamilyNames(&families);face->GetFaceNames(&names);
            auto family=localized(families.Get()),label=localized(names.Get());
            if(family!=name||label.empty())continue;
            c.callback(family.c_str(),label.c_str(),face->GetWeight(),face->GetStyle(),face->GetStretch(),face->GetType(),face->IsColorFont()?1:0,c.data);++c.count;
        }
    },&extra);
    if(gdiStatus)return E_FAIL;
    return 0;
}
using DesktopPathCallback=void(*)(const NativePoint*,const unsigned char*,int,float,float,float,int,int,void*);
extern "C" int nect_shape_desktop_text(const wchar_t* familyNameInput,const wchar_t* faceNameInput,int weight,int style,int stretch,
    const wchar_t* text,int length,float size,float tracking,float lineSpacing,int kerning,int ligatures,int alignment,
    const UINT32* axisTags,const float* axisValues,int axisCount,int vertical,DesktopAxisCallback axisCallback,DesktopPathCallback callback,void* data){
    if(length<0||length>4096||!std::isfinite(size)||size<0.01f||size>10000||!std::isfinite(tracking)||std::abs(tracking)>10000||!std::isfinite(lineSpacing)||lineSpacing<0.1f||lineSpacing>100||style<0||style>2||weight<1||weight>999||stretch<1||stretch>9||alignment<0||alignment>2)return E_INVALIDARG;
    ComPtr<IDWriteFactory3> factory;ComPtr<IDWriteFontCollection> fonts;HRESULT hr=system(factory,fonts);if(FAILED(hr))return hr;
    ComPtr<IDWriteFontFace3> face;ComPtr<IDWriteFontFaceReference> reference;
    hr=resolveFace(factory.Get(),fonts.Get(),familyNameInput,faceNameInput,weight,style,stretch,face);if(FAILED(hr))return hr;
    if(axisCount<-1||axisCount>32||vertical<0||vertical>1)return E_INVALIDARG;
    std::vector<AxisInfo> axes;std::vector<DWRITE_FONT_AXIS_VALUE> resolvedAxes;
    if(axisCount>=0){
        hr=readAxes(face.Get(),axes);if(FAILED(hr))return hr;if(axisCount!=static_cast<int>(axes.size()))return E_INVALIDARG;
        for(auto& a:axes){
            int match=-1;for(int i=0;i<axisCount;++i)if(axisTags[i]==a.tag){if(match>=0)return E_INVALIDARG;match=i;}
            if(match<0||!std::isfinite(axisValues[match])||axisValues[match]<a.minimum||axisValues[match]>a.maximum)return E_INVALIDARG;
            a.value=axisValues[match];
        }
        if(axisCount){
            ComPtr<IDWriteFontFace5> base;ComPtr<IDWriteFontResource> resource;
            if(FAILED(face.As(&base))||FAILED(base->GetFontResource(&resource)))return E_NOTIMPL;
            UINT32 count=base->GetFontAxisValueCount();if(count>32)return E_BOUNDS;resolvedAxes.resize(count);
            if(FAILED(base->GetFontAxisValues(resolvedAxes.data(),count)))return E_FAIL;
            for(auto& value:resolvedAxes)for(auto& a:axes)if(value.axisTag==a.tag)value.value=a.value;
            ComPtr<IDWriteFontFace5> instance;ComPtr<IDWriteFontFaceReference1> variedReference;
            if(FAILED(resource->CreateFontFace(DWRITE_FONT_SIMULATIONS_NONE,resolvedAxes.data(),count,&instance))||
               FAILED(resource->CreateFontFaceReference(DWRITE_FONT_SIMULATIONS_NONE,resolvedAxes.data(),count,&variedReference)))return E_FAIL;
            instance.As(&face);variedReference.As(&reference);
        }
    }
    if(!reference&&FAILED(face->GetFontFaceReference(&reference)))return E_FAIL;
    if(face->IsColorFont())return E_NOTIMPL;
    // A collection containing only this active face keeps matching exact. Any
    // fallback run is still rejected by the renderer's face-reference equality.
    ComPtr<IDWriteFontSetBuilder> builder;ComPtr<IDWriteFontSet> set;ComPtr<IDWriteFontCollection1> exactCollection;
    if(FAILED(factory->CreateFontSetBuilder(&builder))||FAILED(builder->AddFontFaceReference(reference.Get()))||FAILED(builder->CreateFontSet(&set))||FAILED(factory->CreateFontCollectionFromFontSet(set.Get(),&exactCollection)))return E_FAIL;
    ComPtr<IDWriteFontFamily> exactFamily;exactCollection->GetFontFamily(0,&exactFamily);auto exactName=familyName(exactFamily.Get());
    ComPtr<IDWriteTextFormat> format;
    hr=factory->CreateTextFormat(exactName.c_str(),exactCollection.Get(),static_cast<DWRITE_FONT_WEIGHT>(weight),static_cast<DWRITE_FONT_STYLE>(style),static_cast<DWRITE_FONT_STRETCH>(stretch),size,L"ja-jp",&format);if(FAILED(hr))return hr;
    if(axisCount>0){
        ComPtr<IDWriteTextFormat3> format3;if(FAILED(format.As(&format3)))return E_NOTIMPL;
        if(FAILED(format3->SetFontAxisValues(resolvedAxes.data(),static_cast<UINT32>(resolvedAxes.size())))||FAILED(format3->SetAutomaticFontAxes(DWRITE_AUTOMATIC_FONT_AXES_NONE)))return E_FAIL;
    }
    if(vertical&&(FAILED(format->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM))||FAILED(format->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT))))return E_FAIL;
    DWRITE_FONT_METRICS metrics;face->GetMetrics(&metrics);float ascent=size*metrics.ascent/metrics.designUnitsPerEm;
    if(FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP))||FAILED(format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,size*lineSpacing,ascent)))return E_INVALIDARG;
    ComPtr<IDWriteTextLayout> layout;
    hr=factory->CreateTextLayout(text,length,format.Get(),1000000,1000000,&layout);if(FAILED(hr))return hr;
    ComPtr<IDWriteTextLayout1> layout1;if(FAILED(layout.As(&layout1)))return E_NOTIMPL;
    ComPtr<IDWriteTextLayout2> layout2;layout.As(&layout2);
    if(vertical&&(!layout2||FAILED(layout2->SetVerticalGlyphOrientation(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT))))return E_NOTIMPL;
    DWRITE_TEXT_RANGE range={0,static_cast<UINT32>(length)};
    if(axisCount>0){
        ComPtr<IDWriteTextLayout4> layout4;if(FAILED(layout.As(&layout4)))return E_NOTIMPL;
        if(FAILED(layout4->SetFontAxisValues(resolvedAxes.data(),static_cast<UINT32>(resolvedAxes.size()),range))||FAILED(layout4->SetAutomaticFontAxes(DWRITE_AUTOMATIC_FONT_AXES_NONE)))return E_FAIL;
    }
    if(FAILED(layout1->SetCharacterSpacing(0,tracking,0,range))||FAILED(layout1->SetPairKerning(kerning!=0,range)))return E_INVALIDARG;
    ComPtr<IDWriteTypography> typography;factory->CreateTypography(&typography);
    typography->AddFontFeature({DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES,ligatures?1u:0u});
    if(FAILED(layout->SetTypography(typography.Get(),range)))return E_FAIL;
    DWRITE_TEXT_METRICS textMetrics;if(FAILED(layout->GetMetrics(&textMetrics))||textMetrics.lineCount>256)return E_BOUNDS;
    float advanceHeight=textMetrics.height;DWRITE_TEXT_METRICS1 extendedMetrics;
    if(layout2&&SUCCEEDED(layout2->GetMetrics(&extendedMetrics)))advanceHeight=extendedMetrics.heightIncludingTrailingWhitespace;
    layout->SetMaxWidth(std::max(1.0f,textMetrics.widthIncludingTrailingWhitespace));
    if(vertical)layout->SetMaxHeight(std::max(1.0f,advanceHeight));
    layout->SetTextAlignment(alignment==1?DWRITE_TEXT_ALIGNMENT_CENTER:alignment==2?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_LEADING);
    ComPtr<IDWriteTextAnalyzer2> analyzer;
    if(vertical){ComPtr<IDWriteTextAnalyzer> base;if(FAILED(factory->CreateTextAnalyzer(&base))||FAILED(base.As(&analyzer)))return E_NOTIMPL;}
    Geometry output;ComPtr<Renderer> renderer;renderer.Attach(new Renderer(face.Get(),reference.Get(),analyzer.Get(),output));
    hr=layout->Draw(nullptr,renderer.Get(),0,0);if(FAILED(output.failure))return output.failure;if(FAILED(hr)||output.failed)return FAILED(hr)?hr:E_BOUNDS;
    UINT32 lineCount=0;layout->GetLineMetrics(nullptr,0,&lineCount);std::vector<DWRITE_LINE_METRICS> lines(lineCount);
    if(lineCount&&SUCCEEDED(layout->GetLineMetrics(lines.data(),lineCount,&lineCount)))ascent=lines[0].baseline;
    if(axisCount>=0)emitAxes(axes,axisCallback,data);
    callback(output.points.data(),output.types.data(),static_cast<int>(output.points.size()),ascent,textMetrics.widthIncludingTrailingWhitespace,advanceHeight,static_cast<int>(textMetrics.lineCount),static_cast<int>(output.glyphs),data);
    return 0;
}
