#include "nect/core.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <atomic>
#include <cwctype>
#include <sstream>
#endif

namespace nect {

TextSource default_text(Id id,std::string content) {
    TextSource result;
    result.id=std::move(id);result.content=std::move(content);
    result.parameters={{"origin_x",{0,{}}},{"origin_y",{0,{}}},{"font_size",{48,{}}},
        {"frame_width",{400,{}}},{"frame_height",{200,{}}},{"tracking",{0,{}}},{"line_spacing",{0,{}}}};
    return result;
}

#ifdef _WIN32
namespace {
using Microsoft::WRL::ComPtr;
constexpr std::size_t max_glyphs=65536,max_anchors=250000;

void check_hr(HRESULT hr,const char* operation) {
    if(SUCCEEDED(hr))return;
    std::ostringstream message;message<<operation<<" failed (DirectWrite HRESULT 0x"<<std::hex<<static_cast<unsigned long>(hr)<<")";
    throw Error("TEXT_LAYOUT_FAILED",message.str());
}
void add_unique(std::vector<std::string>& values,std::string value) {
    if(std::find(values.begin(),values.end(),value)==values.end())values.push_back(std::move(value));
}
std::wstring utf16(const std::string& value) {
    if(value.empty())return {};
    if(value.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()))throw Error("LIMIT","Text string is too large");
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(count==0)throw Error("INVALID_UTF8","Text must contain valid UTF-8");
    std::wstring result(static_cast<std::size_t>(count),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),count)!=count)
        throw Error("INVALID_UTF8","Cannot convert text to UTF-16");
    return result;
}
std::string utf8(const std::wstring& value) {
    if(value.empty())return {};
    const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(count==0)throw Error("TEXT_FONT_METADATA","Cannot read the installed font family name");
    std::string result(static_cast<std::size_t>(count),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),count,nullptr,nullptr)!=count)
        throw Error("TEXT_FONT_METADATA","Cannot convert the installed font family name");
    return result;
}
std::wstring localized_string(IDWriteLocalizedStrings* names,UINT32 index) {
    UINT32 length=0;check_hr(names->GetStringLength(index,&length),"Get font family length");
    if(length>4096)throw Error("TEXT_FONT_METADATA","Installed font family name exceeds the metadata limit");
    std::wstring result(static_cast<std::size_t>(length)+1,L'\0');
    check_hr(names->GetString(index,result.data(),length+1),"Get font family name");result.resize(length);return result;
}
bool equal_name(const std::wstring& a,const std::wstring& b) {
    return CompareStringOrdinal(a.data(),static_cast<int>(a.size()),b.data(),static_cast<int>(b.size()),TRUE)==CSTR_EQUAL;
}
bool same_point(const Vec2& a,const Vec2& b) {return a.x==b.x&&a.y==b.y;}

// DirectWrite supplies cubic outlines in baseline coordinates. The sink keeps
// those exact curves; it never flattens them or manufactures authored point IDs.
class OutlineSink final : public IDWriteGeometrySink {
public:
    OutlineSink(std::vector<EvaluatedContour>& contours,std::size_t& anchor_count,DWRITE_MATRIX transform,FLOAT x,FLOAT y)
        :contours_(contours),anchor_count_(anchor_count),transform_(transform),x_(x),y_(y){}
    std::exception_ptr failure;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** object) override {
        if(!object)return E_POINTER;*object=nullptr;
        if(iid==__uuidof(IUnknown)||iid==__uuidof(IDWriteGeometrySink)) {*object=static_cast<IDWriteGeometrySink*>(this);AddRef();return S_OK;}
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++references_;}
    ULONG STDMETHODCALLTYPE Release() override {const auto count=--references_;if(!count)delete this;return count;}
    void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE) override {}
    void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT) override {}
    void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F start,D2D1_FIGURE_BEGIN) override {
        capture([&]{
            if(open_)throw Error("TEXT_OUTLINE_INVALID","Font outline began a second unfinished contour");
            contours_.push_back({});open_=true;append(project(start));
        });
    }
    void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F* points,UINT32 count) override {
        capture([&]{for(UINT32 i=0;i<count;++i)append(project(points[i]));});
    }
    void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT* curves,UINT32 count) override {
        capture([&]{for(UINT32 i=0;i<count;++i) {
            require_open();contours_.back().points.back().outgoing=project(curves[i].point1);
            const auto incoming=project(curves[i].point2);append(project(curves[i].point3));
            contours_.back().points.back().incoming=incoming;
        }});
    }
    void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END end) override {
        capture([&]{
            require_open();auto& contour=contours_.back();contour.closed=end==D2D1_FIGURE_END_CLOSED;
            if(contour.closed&&contour.points.size()>1&&same_point(contour.points.front().anchor,contour.points.back().anchor)) {
                contour.points.front().incoming=contour.points.back().incoming;contour.points.pop_back();--anchor_count_;
            }
            open_=false;
        });
    }
    HRESULT STDMETHODCALLTYPE Close() override {
        capture([&]{if(open_)throw Error("TEXT_OUTLINE_INVALID","Font outline has an unfinished contour");});
        return failure?E_FAIL:S_OK;
    }
private:
    std::atomic<ULONG> references_{1};
    std::vector<EvaluatedContour>& contours_;std::size_t& anchor_count_;
    DWRITE_MATRIX transform_;FLOAT x_,y_;bool open_=false;
    template<class F>void capture(F fn) noexcept {
        if(failure)return;
        try{fn();}catch(...){failure=std::current_exception();}
    }
    void require_open() const {
        if(!open_||contours_.empty()||contours_.back().points.empty())throw Error("TEXT_OUTLINE_INVALID","Font outline segment has no start point");
    }
    Vec2 project(D2D1_POINT_2F point) const {
        const double x=static_cast<double>(point.x)+x_,y=static_cast<double>(point.y)+y_;
        return {x*transform_.m11+y*transform_.m21+transform_.dx,x*transform_.m12+y*transform_.m22+transform_.dy};
    }
    void append(Vec2 anchor) {
        if(!open_)throw Error("TEXT_OUTLINE_INVALID","Font outline segment has no contour");
        if(anchor_count_>=max_anchors)throw Error("TEXT_OUTLINE_LIMIT","Text outline exceeds 250000 anchors");
        if(!std::isfinite(anchor.x)||!std::isfinite(anchor.y))throw Error("TEXT_OUTLINE_INVALID","Font outline contains a non-finite coordinate");
        contours_.back().points.push_back({anchor,anchor,anchor});++anchor_count_;
    }
};

bool visible_characters(const DWRITE_GLYPH_RUN_DESCRIPTION* description) {
    if(!description)return true;
    for(UINT32 i=0;i<description->stringLength;++i) {
        const auto c=description->string[i];
        if(std::iswspace(c)||c<0x20||c==0x0085||c==0x00a0||c==0x1680||(c>=0x2000&&c<=0x200a)||
            c==0x2028||c==0x2029||c==0x202f||c==0x205f||c==0x3000||c==0x00ad||c==0x034f||c==0x061c||
            c==0x200b||c==0x200c||c==0x200d||c==0x2060||c==0xfeff||
            (c>=0x200e&&c<=0x200f)||(c>=0x202a&&c<=0x202e)||(c>=0x2066&&c<=0x2069)||(c>=0xfe00&&c<=0xfe0f))continue;
        return true;
    }
    return false;
}

class OutlineRenderer final : public IDWriteTextRenderer1 {
public:
    OutlineRenderer(IDWriteTextAnalyzer2* analyzer,IDWriteFontCollection* fonts,const std::wstring& requested,
        const std::wstring& locale,std::vector<EvaluatedContour>& contours,TextLayout& result)
        :analyzer_(analyzer),fonts_(fonts),requested_(requested),locale_(locale),contours_(contours),result_(result){}
    std::exception_ptr failure;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** object) override {
        if(!object)return E_POINTER;*object=nullptr;
        if(iid==__uuidof(IUnknown)||iid==__uuidof(IDWritePixelSnapping)||iid==__uuidof(IDWriteTextRenderer)||iid==__uuidof(IDWriteTextRenderer1)) {
            *object=static_cast<IDWriteTextRenderer1*>(this);AddRef();return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++references_;}
    ULONG STDMETHODCALLTYPE Release() override {const auto count=--references_;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,BOOL* disabled) override {if(!disabled)return E_POINTER;*disabled=TRUE;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,DWRITE_MATRIX* matrix) override {
        if(!matrix)return E_POINTER;*matrix={1,0,0,1,0,0};return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*,FLOAT* pixels) override {if(!pixels)return E_POINTER;*pixels=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void* context,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE mode,
        const DWRITE_GLYPH_RUN* run,const DWRITE_GLYPH_RUN_DESCRIPTION* description,IUnknown* effect) override {
        return DrawGlyphRun(context,x,y,DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES,mode,run,description,effect);
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT x,FLOAT y,DWRITE_GLYPH_ORIENTATION_ANGLE orientation,DWRITE_MEASURING_MODE,
        const DWRITE_GLYPH_RUN* run,const DWRITE_GLYPH_RUN_DESCRIPTION* description,IUnknown*) override {
        if(failure)return E_FAIL;
        try {
            if(!run||!run->fontFace)throw Error("TEXT_OUTLINE_INVALID","DirectWrite returned a glyph run without a font");
            if(run->glyphCount==0)return S_OK;
            if(result_.glyph_count+run->glyphCount>max_glyphs)throw Error("TEXT_GLYPH_LIMIT","Text layout exceeds 65536 glyphs");
            result_.glyph_count+=run->glyphCount;
            collect_font(run->fontFace);
            if(std::find(run->glyphIndices,run->glyphIndices+run->glyphCount,0)!=run->glyphIndices+run->glyphCount)
                add_unique(result_.warnings,"MISSING_GLYPH: Some characters have no installed glyph; a missing-glyph outline is shown.");
            bool color_font=false;
            ComPtr<IDWriteFontFace2> color_face;
            if(SUCCEEDED(run->fontFace->QueryInterface(IID_PPV_ARGS(&color_face))))color_font=color_face->IsColorFont()!=FALSE;
            const auto outline_formats=DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE|DWRITE_GLYPH_IMAGE_FORMATS_CFF;
            auto has_color=[&](DWRITE_GLYPH_IMAGE_FORMATS formats) {
                return (static_cast<unsigned>(formats)&~static_cast<unsigned>(outline_formats))!=0;
            };
            ComPtr<IDWriteFontFace4> image_face;
            if(SUCCEEDED(run->fontFace->QueryInterface(IID_PPV_ARGS(&image_face))))
                color_font=color_font||has_color(image_face->GetGlyphImageFormats());
            if(color_font) {
                if(!image_face)throw Error("TEXT_COLOR_UNSUPPORTED","This Windows text interface cannot verify monochrome outlines in a color font");
                for(UINT32 i=0;i<run->glyphCount;++i) {
                    DWRITE_GLYPH_IMAGE_FORMATS formats=DWRITE_GLYPH_IMAGE_FORMATS_NONE;
                    check_hr(image_face->GetGlyphImageFormats(run->glyphIndices[i],0,UINT32_MAX,&formats),"Inspect glyph image formats");
                    if(formats!=DWRITE_GLYPH_IMAGE_FORMATS_NONE&&(formats&outline_formats)==0)
                        throw Error("TEXT_COLOR_UNSUPPORTED","A color-only glyph cannot be represented as an editable monochrome text outline");
                    // A COLR font may advertise TrueType while its base glyph
                    // is empty and all visible contours live in color layers.
                    // Probe each colored base independently so a neighboring
                    // ordinary glyph cannot conceal that unsupported blank.
                    if(has_color(formats))verify_color_base(*run,i);
                }
                add_unique(result_.warnings,"COLOR_GLYPH_MONOCHROME: Color-font glyphs use their monochrome outlines and the object's paint.");
            }
            DWRITE_MATRIX transform{};
            check_hr(analyzer_->GetGlyphOrientationTransform(orientation,run->isSideways,x,y,&transform),"Orient glyph run");
            const auto before=anchor_count_;
            ComPtr<OutlineSink> sink;sink.Attach(new OutlineSink(contours_,anchor_count_,transform,x,y));
            const auto hr=run->fontFace->GetGlyphRunOutline(run->fontEmSize,run->glyphIndices,run->glyphAdvances,
                run->glyphOffsets,run->glyphCount,run->isSideways,(run->bidiLevel&1)!=0,sink.Get());
            sink->Close();if(sink->failure)std::rethrow_exception(sink->failure);
            check_hr(hr,"Project glyph outlines");
            if(before==anchor_count_&&run->glyphCount>0&&visible_characters(description))
                throw Error(color_font?"TEXT_COLOR_UNSUPPORTED":"TEXT_OUTLINE_UNSUPPORTED","Visible text has no supported glyph outlines; no blank replacement was committed");
            return S_OK;
        } catch(...) {failure=std::current_exception();return E_FAIL;}
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,const DWRITE_UNDERLINE*,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,const DWRITE_UNDERLINE*,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,const DWRITE_STRIKETHROUGH*,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,const DWRITE_STRIKETHROUGH*,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) override {return E_NOTIMPL;}
private:
    std::atomic<ULONG> references_{1};
    IDWriteTextAnalyzer2* analyzer_;IDWriteFontCollection* fonts_;
    const std::wstring& requested_;const std::wstring& locale_;
    std::vector<EvaluatedContour>& contours_;TextLayout& result_;std::size_t anchor_count_=0;
    void verify_color_base(const DWRITE_GLYPH_RUN& run,UINT32 index) {
        std::vector<EvaluatedContour> contours;std::size_t count=0;
        ComPtr<OutlineSink> sink;sink.Attach(new OutlineSink(contours,count,{1,0,0,1,0,0},0,0));
        const auto hr=run.fontFace->GetGlyphRunOutline(run.fontEmSize,run.glyphIndices+index,nullptr,nullptr,1,FALSE,FALSE,sink.Get());
        sink->Close();if(sink->failure)std::rethrow_exception(sink->failure);
        check_hr(hr,"Verify monochrome color-font base glyph");
        if(count==0)throw Error("TEXT_COLOR_UNSUPPORTED","A color glyph has no monochrome base outline; colored layer projection is not supported");
    }
    void collect_font(IDWriteFontFace* face) {
        ComPtr<IDWriteLocalizedStrings> names;ComPtr<IDWriteFontFace3> face3;
        if(SUCCEEDED(face->QueryInterface(IID_PPV_ARGS(&face3))))check_hr(face3->GetFamilyNames(&names),"Read actual font family");
        else {
            ComPtr<IDWriteFont> font;ComPtr<IDWriteFontFamily> family;
            check_hr(fonts_->GetFontFromFontFace(face,&font),"Resolve actual font family");
            check_hr(font->GetFontFamily(&family),"Read actual font family");
            check_hr(family->GetFamilyNames(&names),"Read actual font family names");
        }
        if(names->GetCount()==0)throw Error("TEXT_FONT_METADATA","Installed font did not provide a family name");
        UINT32 index=0;BOOL exists=FALSE;
        check_hr(names->FindLocaleName(L"en-us",&index,&exists),"Select font name locale");
        if(!exists)check_hr(names->FindLocaleName(locale_.c_str(),&index,&exists),"Select font name locale");
        if(!exists)index=0;
        const auto actual=utf8(localized_string(names.Get(),index));add_unique(result_.used_fonts,actual);
        bool requested=false;
        for(UINT32 i=0;i<names->GetCount();++i)if(equal_name(localized_string(names.Get(),i),requested_)){requested=true;break;}
        if(!requested)add_unique(result_.warnings,"FONT_FALLBACK: Installed family '"+actual+"' supplies part of this text; the requested family is retained.");
    }
};

float parameter(const std::map<std::string,double>& values,const char* name) {
    const auto found=values.find(name);
    if(found==values.end()||!std::isfinite(found->second)||std::abs(found->second)>1e9)
        throw Error("TEXT_PARAMETER","Missing, non-finite or out-of-range text parameter: "+std::string(name));
    return static_cast<float>(found->second);
}
} // namespace
#endif

std::vector<std::string> text_fonts() {
#ifndef _WIN32
    throw Error("TEXT_PLATFORM_UNSUPPORTED","Installed text font discovery currently requires Windows DirectWrite.");
#else
    ComPtr<IDWriteFactory> factory;
    check_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf())),"Create DirectWrite font discovery");
    ComPtr<IDWriteFontCollection> fonts;check_hr(factory->GetSystemFontCollection(&fonts),"Get installed font families");
    std::vector<std::string> result;
    for(UINT32 i=0;i<fonts->GetFontFamilyCount();++i) {
        ComPtr<IDWriteFontFamily> family;ComPtr<IDWriteLocalizedStrings> names;
        check_hr(fonts->GetFontFamily(i,&family),"Enumerate installed font family");
        check_hr(family->GetFamilyNames(&names),"Enumerate installed font family names");
        if(names->GetCount()==0)continue;
        UINT32 index=0;BOOL exists=FALSE;check_hr(names->FindLocaleName(L"en-us",&index,&exists),"Select font discovery locale");
        result.push_back(utf8(localized_string(names.Get(),exists?index:0)));
    }
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
#endif
}

TextLayout evaluate_text(const TextSource& source,const std::map<std::string,double>& parameters) {
#ifndef _WIN32
    (void)source;(void)parameters;
    throw Error("TEXT_PLATFORM_UNSUPPORTED","Text projection currently requires Windows DirectWrite; authored text remains portable.");
#else
    if(source.content.size()>32768)throw Error("LIMIT","Text content exceeds 32768 UTF-8 bytes");
    const auto text=utf16(source.content),family=utf16(source.family),locale=utf16(source.locale);
    const auto font_size=parameter(parameters,"font_size"),tracking=parameter(parameters,"tracking"),spacing=parameter(parameters,"line_spacing");
    const auto origin_x=parameter(parameters,"origin_x"),origin_y=parameter(parameters,"origin_y");
    const auto frame_width=parameter(parameters,"frame_width"),frame_height=parameter(parameters,"frame_height");
    if(font_size<=0||frame_width<=0||frame_height<=0||spacing<0)throw Error("TEXT_PARAMETER","Font and frame sizes must be positive; line spacing cannot be negative");
    if(source.layout!="auto"&&source.layout!="frame")throw Error("TEXT_LAYOUT_UNSUPPORTED","Text layout must be auto or frame");
    if(source.direction!="horizontal"&&source.direction!="vertical")throw Error("TEXT_DIRECTION_UNSUPPORTED","Text direction must be horizontal or vertical");
    if(source.alignment!="start"&&source.alignment!="center"&&source.alignment!="end")throw Error("TEXT_ALIGNMENT_UNSUPPORTED","Text alignment must be start, center or end");
    const bool automatic=source.layout=="auto",vertical=source.direction=="vertical";
    TextLayout result;result.x=origin_x;result.y=origin_y;
    ComPtr<IDWriteFactory2> factory;
    check_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory2),reinterpret_cast<IUnknown**>(factory.GetAddressOf())),"Create DirectWrite factory");
    ComPtr<IDWriteFontCollection> fonts;check_hr(factory->GetSystemFontCollection(&fonts),"Get installed fonts");
    UINT32 family_index=0;BOOL family_exists=FALSE;
    check_hr(fonts->FindFamilyName(family.c_str(),&family_index,&family_exists),"Find requested font family");
    if(!family_exists)add_unique(result.warnings,"MISSING_FONT: Requested family '"+source.family+"' is not installed; DirectWrite system fallback is used.");
    ComPtr<IDWriteTextFormat> format;
    check_hr(factory->CreateTextFormat(family.c_str(),fonts.Get(),static_cast<DWRITE_FONT_WEIGHT>(source.weight),
        source.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,font_size,locale.c_str(),&format),"Create text format");
    if(vertical) {
        check_hr(format->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM),"Set vertical reading direction");
        check_hr(format->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT),"Set vertical column flow");
    }
    check_hr(format->SetWordWrapping(automatic?DWRITE_WORD_WRAPPING_NO_WRAP:DWRITE_WORD_WRAPPING_WRAP),"Set text wrapping");
    check_hr(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR),"Set text paragraph alignment");
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_NONE,0,0};
    check_hr(format->SetTrimming(&trimming,nullptr),"Disable text truncation");
    ComPtr<IDWriteTextLayout> base_layout;ComPtr<IDWriteTextLayout2> layout;
    check_hr(factory->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),
        automatic?1000000.0f:frame_width,automatic?1000000.0f:frame_height,&base_layout),"Create text layout");
    check_hr(base_layout.As(&layout),"Get orientation-aware text layout");
    check_hr(layout->SetVerticalGlyphOrientation(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT),"Set script-aware glyph orientation");
    check_hr(layout->SetLastLineWrapping(TRUE),"Retain wrapping on overflow lines");
    check_hr(layout->SetCharacterSpacing(0,tracking,0,{0,static_cast<UINT32>(text.size())}),"Set tracking");
    if(spacing>0) {
        UINT32 count=0;const auto hr=layout->GetLineMetrics(nullptr,0,&count);
        if(FAILED(hr)&&hr!=E_NOT_SUFFICIENT_BUFFER)check_hr(hr,"Measure default line spacing");
        if(count>32769)throw Error("TEXT_LAYOUT_LIMIT","Text has too many lines");
        std::vector<DWRITE_LINE_METRICS> lines(count);
        if(count)check_hr(layout->GetLineMetrics(lines.data(),count,&count),"Measure default baselines");
        const auto ratio=!lines.empty()&&lines.front().height>0?lines.front().baseline/lines.front().height:0.8f;
        check_hr(layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,spacing,spacing*ratio),"Set uniform line spacing");
    }
    DWRITE_TEXT_METRICS1 metrics{};check_hr(layout->GetMetrics(&metrics),"Measure text");
    if(automatic) {
        // Vertical layout starts columns at the right edge. Measuring in a large
        // container is safe only if it is then resized and re-laid out before
        // extracting outlines; otherwise every glyph inherits that large X.
        check_hr(layout->SetMaxWidth(std::max(0.001f,metrics.widthIncludingTrailingWhitespace)),"Fit automatic text width");
        check_hr(layout->SetMaxHeight(std::max(0.001f,metrics.heightIncludingTrailingWhitespace)),"Fit automatic text height");
    }
    const auto alignment=source.alignment=="center"?DWRITE_TEXT_ALIGNMENT_CENTER:
        source.alignment=="end"?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_LEADING;
    check_hr(layout->SetTextAlignment(alignment),"Set text alignment");
    check_hr(layout->GetMetrics(&metrics),"Measure final text");
    if(!std::isfinite(metrics.widthIncludingTrailingWhitespace)||!std::isfinite(metrics.heightIncludingTrailingWhitespace)||
        metrics.widthIncludingTrailingWhitespace>1e9||metrics.heightIncludingTrailingWhitespace>1e9)
        throw Error("TEXT_LAYOUT_LIMIT","Text dimensions exceed the supported finite layout range");
    result.width=automatic?metrics.widthIncludingTrailingWhitespace:frame_width;
    result.height=automatic?metrics.heightIncludingTrailingWhitespace:frame_height;
    double draw_origin_y=origin_y;
    if(automatic)draw_origin_y-=metrics.top;
    if(!vertical) {
        UINT32 line_count=0;
        const auto line_hr=layout->GetLineMetrics(nullptr,0,&line_count);
        if(FAILED(line_hr)&&line_hr!=E_NOT_SUFFICIENT_BUFFER)
            check_hr(line_hr,"Measure first-line baseline");
        if(line_count>32769)throw Error("TEXT_LAYOUT_LIMIT","Text has too many lines");
        if(line_count) {
            std::vector<DWRITE_LINE_METRICS> lines(line_count);
            check_hr(layout->GetLineMetrics(lines.data(),line_count,&line_count),"Measure line baselines");
            lines.resize(line_count);
            double line_top=draw_origin_y;
            for(const auto& line:lines) {
                const auto baseline=line_top+line.baseline;
                if(std::isfinite(baseline))result.line_baselines_y.push_back(baseline);
                line_top+=line.height;
            }
            if(!result.line_baselines_y.empty())result.first_line_baseline_y=result.line_baselines_y.front();
        }
    }
    if(!automatic) {
        constexpr double tolerance=0.01;
        result.overflow=metrics.left<-tolerance||metrics.top<-tolerance||
            metrics.left+metrics.widthIncludingTrailingWhitespace>frame_width+tolerance||
            metrics.top+metrics.heightIncludingTrailingWhitespace>frame_height+tolerance;
        if(result.overflow)add_unique(result.warnings,"TEXT_OVERFLOW: Text extends beyond the frame; all glyphs are retained. Resize the frame or edit the content.");
    }
    ComPtr<IDWriteTextAnalyzer> base_analyzer;ComPtr<IDWriteTextAnalyzer2> analyzer;
    check_hr(factory->CreateTextAnalyzer(&base_analyzer),"Create glyph orientation analyzer");
    check_hr(base_analyzer.As(&analyzer),"Get glyph orientation analyzer");
    auto contours=std::make_shared<std::vector<EvaluatedContour>>();
    ComPtr<OutlineRenderer> renderer;renderer.Attach(new OutlineRenderer(analyzer.Get(),fonts.Get(),family,locale,*contours,result));
    const auto hr=layout->Draw(nullptr,renderer.Get(),origin_x-(automatic?metrics.left:0),draw_origin_y);
    if(renderer->failure)std::rethrow_exception(renderer->failure);
    check_hr(hr,"Draw shaped text outlines");result.contours=std::move(contours);return result;
#endif
}
} // namespace nect
