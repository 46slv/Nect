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
#include <bcrypt.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <atomic>
#include <cwctype>
#include <sstream>
#include <tuple>
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

std::uint32_t opentype_tag(const std::string& tag) {
    if(tag.size()!=4)throw Error("TEXT_FONT_TAG","OpenType tags must contain exactly four bytes");
    std::uint32_t value=0;
    for(unsigned i=0;i<4;++i)value|=static_cast<std::uint32_t>(static_cast<unsigned char>(tag[i]))<<(i*8);
    return value;
}
std::string tag_string(std::uint32_t tag) {
    std::string result(4,' ');
    for(unsigned i=0;i<4;++i)result[i]=static_cast<char>((tag>>(i*8))&255);
    return result;
}
std::optional<std::string> metadata_name(IDWriteLocalizedStrings* names,const std::wstring& locale) {
    if(!names||names->GetCount()==0||names->GetCount()>4096)return {};
    UINT32 index=0;BOOL exists=FALSE;
    if(FAILED(names->FindLocaleName(L"en-us",&index,&exists)))return {};
    if(!exists&&FAILED(names->FindLocaleName(locale.c_str(),&index,&exists)))return {};
    try{return utf8(localized_string(names,exists?index:0));}catch(const Error&){return {};}
}
std::optional<std::string> reference_digest(const std::vector<unsigned char>& key) {
    struct Algorithm {BCRYPT_ALG_HANDLE value=nullptr;~Algorithm(){if(value)BCryptCloseAlgorithmProvider(value,0);}} algorithm;
    if(BCryptOpenAlgorithmProvider(&algorithm.value,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    std::array<unsigned char,32> digest{};
    if(BCryptHash(algorithm.value,nullptr,0,const_cast<PUCHAR>(key.data()),static_cast<ULONG>(key.size()),
        digest.data(),static_cast<ULONG>(digest.size()))<0)return {};
    constexpr char digits[]="0123456789abcdef";std::string result;
    for(const auto byte:digest){result+=digits[byte>>4];result+=digits[byte&15];}
    return result;
}

// DrawGlyphRun has no script analysis. Analyze the bounded original UTF-16 text
// once, and intersect those script spans with actual drawn run ranges below.
class ReceiptScriptAnalysis final : public IDWriteTextAnalysisSource,public IDWriteTextAnalysisSink {
public:
    ReceiptScriptAnalysis(const std::wstring& text,const std::wstring& locale,bool vertical)
        :scripts(text.size()),text_(text),locale_(locale),vertical_(vertical){}
    std::vector<std::optional<DWRITE_SCRIPT_ANALYSIS>> scripts;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** object) override {
        if(!object)return E_POINTER;*object=nullptr;
        if(iid==__uuidof(IUnknown)||iid==__uuidof(IDWriteTextAnalysisSource))*object=static_cast<IDWriteTextAnalysisSource*>(this);
        else if(iid==__uuidof(IDWriteTextAnalysisSink))*object=static_cast<IDWriteTextAnalysisSink*>(this);
        else return E_NOINTERFACE;
        AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++references_;}
    ULONG STDMETHODCALLTYPE Release() override {const auto n=--references_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 position,const WCHAR** text,UINT32* length) override {
        if(!text||!length)return E_POINTER;
        *text=position<text_.size()?text_.data()+position:nullptr;
        *length=position<text_.size()?static_cast<UINT32>(text_.size())-position:0;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 position,const WCHAR** text,UINT32* length) override {
        if(!text||!length)return E_POINTER;
        if(position>text_.size())return E_INVALIDARG;
        *text=position?text_.data():nullptr;*length=position;return S_OK;
    }
    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override {
        return vertical_?DWRITE_READING_DIRECTION_TOP_TO_BOTTOM:DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }
    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 position,UINT32* length,const WCHAR** locale) override {
        if(!length||!locale)return E_POINTER;
        if(position>text_.size())return E_INVALIDARG;
        *length=static_cast<UINT32>(text_.size())-position;*locale=locale_.c_str();return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 position,UINT32* length,IDWriteNumberSubstitution** substitution) override {
        if(!length||!substitution)return E_POINTER;
        if(position>text_.size())return E_INVALIDARG;
        *length=static_cast<UINT32>(text_.size())-position;*substitution=nullptr;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetScriptAnalysis(UINT32 position,UINT32 length,const DWRITE_SCRIPT_ANALYSIS* analysis) override {
        if(!analysis)return E_POINTER;
        if(position>scripts.size()||length>scripts.size()-position)return E_INVALIDARG;
        std::fill(scripts.begin()+position,scripts.begin()+position+length,*analysis);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetLineBreakpoints(UINT32,UINT32,const DWRITE_LINE_BREAKPOINT*) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE SetBidiLevel(UINT32,UINT32,UINT8,UINT8) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE SetNumberSubstitution(UINT32,UINT32,IDWriteNumberSubstitution*) override {return S_OK;}
private:
    std::atomic<ULONG> references_{1};
    const std::wstring& text_;const std::wstring& locale_;bool vertical_;
};

void submit_axes(IDWriteTextFormat* format,const TextSource& source,TextLayout& result) {
    std::vector<DWRITE_FONT_AXIS_VALUE> axes{{DWRITE_FONT_AXIS_TAG_WEIGHT,static_cast<float>(source.weight)},
        {DWRITE_FONT_AXIS_TAG_ITALIC,source.italic?1.0f:0.0f}};
    auto& request=result.font_request;
    std::map<std::string,double> submitted{{"wght",static_cast<double>(source.weight)},{"ital",source.italic?1.0:0.0}};
    for(const auto& [tag,value]:source.additional_axis_values) {
        if(!std::isfinite(value))throw Error("TEXT_FONT_AXIS","Font axis values must be finite");
        if(tag=="wght"||tag=="ital")throw Error("TEXT_FONT_AXIS","Weight and italic have dedicated authored fields");
        if(std::abs(value)>std::numeric_limits<float>::max()) {
            add_unique(result.warnings,"FONT_AXIS_FLOAT_UNREPRESENTABLE: Axis '"+tag+"' cannot be submitted as a DirectWrite float; authored intent is retained.");
            continue;
        }
        const auto narrowed=static_cast<float>(value);
        if(static_cast<double>(narrowed)!=value)
            add_unique(result.warnings,"FONT_AXIS_FLOAT_ROUNDED: Axis '"+tag+"' is narrowed to a DirectWrite float; exact authored intent is retained.");
        axes.push_back({static_cast<DWRITE_FONT_AXIS_TAG>(opentype_tag(tag)),narrowed});
        submitted.emplace(tag,narrowed);
    }
    ComPtr<IDWriteTextFormat3> format3;
    const auto query=format->QueryInterface(IID_PPV_ARGS(&format3));
    if(query==E_NOINTERFACE) {
        request.axis_application="unsupported_api";
        if(!source.additional_axis_values.empty())add_unique(result.warnings,
            "FONT_AXIS_API_UNSUPPORTED: This DirectWrite runtime cannot submit additional axes; legacy family/weight/style selection is retained.");
        return;
    }
    check_hr(query,"Query variable-axis text format");
    const auto hr=format3->SetFontAxisValues(axes.data(),static_cast<UINT32>(axes.size()));
    if(hr==E_NOTIMPL||hr==E_NOINTERFACE) {
        request.axis_application="unsupported_api";
        add_unique(result.warnings,"FONT_AXIS_API_UNSUPPORTED: DirectWrite cannot apply this axis request; authored intent is retained.");
        return;
    }
    // Unexpected failures remain failures of the atomic projection, not warnings.
    check_hr(hr,"Apply combined weight, italic and additional axes");
    request.axis_application="submitted";request.submitted_axis_values=std::move(submitted);
}
void submit_features(IDWriteFactory* factory,IDWriteTextLayout* layout,const TextSource& source,UINT32 length,TextLayout& result) {
    if(source.font_features.empty())return;
    if(length==0){result.font_request.feature_application="empty_text";return;}
    ComPtr<IDWriteTypography> typography;
    check_hr(factory->CreateTypography(&typography),"Create whole-text typography");
    for(const auto& feature:source.font_features)
        check_hr(typography->AddFontFeature({static_cast<DWRITE_FONT_FEATURE_TAG>(opentype_tag(feature.feature_tag)),feature.parameter}),
            "Add whole-text font feature");
    check_hr(layout->SetTypography(typography.Get(),{0,length}),"Apply whole-text typography");
    result.font_request.feature_application="submitted";
}

// DirectWrite supplies cubic outlines in baseline coordinates. The sink keeps
// those exact curves; it never flattens them or manufactures authored point IDs.
class OutlineSink : public IDWriteGeometrySink {
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

class OutlineRenderer : public IDWriteTextRenderer1 {
public:
    OutlineRenderer(IDWriteTextAnalyzer2* analyzer,IDWriteFontCollection* fonts,const std::wstring& requested,
        const std::wstring& locale,const std::wstring& text,std::vector<EvaluatedContour>& contours,TextLayout& result,bool vertical,bool capture_glyphs)
        :analyzer_(analyzer),fonts_(fonts),requested_(requested),locale_(locale),contours_(contours),result_(result),
         capture_glyphs_(capture_glyphs),vertical_(vertical),text_size_(static_cast<UINT32>(text.size())) {
        if(!result_.font_request.font_features.empty()&&!text.empty()) {
            scripts_.Attach(new ReceiptScriptAnalysis(text,locale,vertical));
            if(FAILED(analyzer_->AnalyzeScript(scripts_.Get(),0,text_size_,scripts_.Get()))) {
                scripts_.Reset();add_unique(result_.warnings,"FONT_SCRIPT_UNKNOWN: Script analysis is unavailable for feature availability receipts.");
            }
        }
    }
    std::exception_ptr failure;
    std::vector<std::pair<UINT32,double>> column_run_origins;
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
            if(capture_glyphs_&&(vertical_||run->isSideways||orientation!=DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES||
                (run->bidiLevel&1)!=0))
                throw Error("TEXT_PATH_RUN_UNSUPPORTED","Text-on-Path supports horizontal left-to-right DirectWrite glyph runs only");
            if(result_.glyph_count+run->glyphCount>max_glyphs)throw Error("TEXT_GLYPH_LIMIT","Text layout exceeds 65536 glyphs");
            result_.glyph_count+=run->glyphCount;
            if(!run->glyphIndices)throw Error("TEXT_OUTLINE_INVALID","DirectWrite returned a glyph run without glyph indices");
            collect_run(*run,description);
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
            if(capture_glyphs_) {
                std::vector<DWRITE_GLYPH_METRICS> nominal;
                DWRITE_FONT_METRICS font_metrics{};
                if(!run->glyphAdvances) {
                    nominal.resize(run->glyphCount);
                    check_hr(run->fontFace->GetDesignGlyphMetrics(run->glyphIndices,run->glyphCount,nominal.data(),FALSE),
                        "Measure shaped glyph advances");
                    run->fontFace->GetMetrics(&font_metrics);
                    if(font_metrics.designUnitsPerEm==0)throw Error("TEXT_PATH_ADVANCE_INVALID","Font reports zero design units per em");
                }
                double pen_x=x;
                for(UINT32 i=0;i<run->glyphCount;++i) {
                    const double advance=run->glyphAdvances?run->glyphAdvances[i]:
                        static_cast<double>(nominal[i].advanceWidth)*run->fontEmSize/font_metrics.designUnitsPerEm;
                    if(!std::isfinite(advance)||advance<0)
                        throw Error("TEXT_PATH_ADVANCE_INVALID","DirectWrite returned a non-finite or negative glyph advance");
                    const auto offset=run->glyphOffsets?run->glyphOffsets[i]:DWRITE_GLYPH_OFFSET{};
                    EvaluatedTextGlyph glyph;glyph.advance=advance;
                    glyph.baseline={pen_x+offset.advanceOffset,y+offset.ascenderOffset};
                    DWRITE_MATRIX glyph_transform{};
                    check_hr(analyzer_->GetGlyphOrientationTransform(orientation,run->isSideways,
                        static_cast<FLOAT>(pen_x),y,&glyph_transform),"Orient shaped glyph");
                    ComPtr<OutlineSink> glyph_sink;glyph_sink.Attach(new OutlineSink(glyph.contours,anchor_count_,glyph_transform,
                        static_cast<FLOAT>(pen_x),y));
                    const auto glyph_hr=run->fontFace->GetGlyphRunOutline(run->fontEmSize,run->glyphIndices+i,
                        run->glyphAdvances?run->glyphAdvances+i:nullptr,run->glyphOffsets?run->glyphOffsets+i:nullptr,
                        1,run->isSideways,(run->bidiLevel&1)!=0,glyph_sink.Get());
                    glyph_sink->Close();if(glyph_sink->failure)std::rethrow_exception(glyph_sink->failure);
                    check_hr(glyph_hr,"Project shaped glyph outline");
                    result_.glyphs.push_back(std::move(glyph));pen_x+=advance;
                    if(!std::isfinite(pen_x))throw Error("TEXT_PATH_ADVANCE_INVALID","Shaped glyph advances exceed finite layout bounds");
                }
                if(before==anchor_count_&&run->glyphCount>0&&visible_characters(description))
                    throw Error(color_font?"TEXT_COLOR_UNSUPPORTED":"TEXT_OUTLINE_UNSUPPORTED",
                        "Visible text has no supported glyph outlines; no blank replacement was committed");
                return S_OK;
            }
            ComPtr<OutlineSink> sink;sink.Attach(new OutlineSink(contours_,anchor_count_,transform,x,y));
            const auto hr=run->fontFace->GetGlyphRunOutline(run->fontEmSize,run->glyphIndices,run->glyphAdvances,
                run->glyphOffsets,run->glyphCount,run->isSideways,(run->bidiLevel&1)!=0,sink.Get());
            sink->Close();if(sink->failure)std::rethrow_exception(sink->failure);
            check_hr(hr,"Project glyph outlines");
            if(before==anchor_count_&&run->glyphCount>0&&visible_characters(description))
                throw Error(color_font?"TEXT_COLOR_UNSUPPORTED":"TEXT_OUTLINE_UNSUPPORTED","Visible text has no supported glyph outlines; no blank replacement was committed");
            if(vertical_&&description&&std::isfinite(x))
                column_run_origins.emplace_back(description->textPosition,x);
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
    bool capture_glyphs_;
    bool vertical_=false;
    UINT32 text_size_=0;
    ComPtr<ReceiptScriptAnalysis> scripts_;
    std::vector<ComPtr<IUnknown>> loaders_;
    std::vector<ComPtr<IDWriteFontFace>> feature_faces_;
    std::map<std::tuple<IDWriteFontFace*,UINT16,unsigned,std::wstring>,std::optional<std::vector<DWRITE_FONT_FEATURE_TAG>>> feature_cache_;
    void verify_color_base(const DWRITE_GLYPH_RUN& run,UINT32 index) {
        std::vector<EvaluatedContour> contours;std::size_t count=0;
        ComPtr<OutlineSink> sink;sink.Attach(new OutlineSink(contours,count,{1,0,0,1,0,0},0,0));
        const auto hr=run.fontFace->GetGlyphRunOutline(run.fontEmSize,run.glyphIndices+index,nullptr,nullptr,1,FALSE,FALSE,sink.Get());
        sink->Close();if(sink->failure)std::rethrow_exception(sink->failure);
        check_hr(hr,"Verify monochrome color-font base glyph");
        if(count==0)throw Error("TEXT_COLOR_UNSUPPORTED","A color glyph has no monochrome base outline; colored layer projection is not supported");
    }
    void warn(TextFontRun& receipt,const std::string& warning) {
        add_unique(receipt.warnings,warning);add_unique(result_.warnings,warning);
    }
    void collect_names(IDWriteFontFace* face,TextFontRun& receipt) {
        ComPtr<IDWriteLocalizedStrings> families,faces;ComPtr<IDWriteFontFace3> face3;
        if(SUCCEEDED(face->QueryInterface(IID_PPV_ARGS(&face3)))) {
            receipt.resolved_weight=static_cast<std::uint32_t>(face3->GetWeight());
            receipt.resolved_style=static_cast<std::uint32_t>(face3->GetStyle());
            if(FAILED(face3->GetFamilyNames(&families)))families.Reset();
            if(FAILED(face3->GetFaceNames(&faces)))faces.Reset();
        } else {
            ComPtr<IDWriteFont> font;ComPtr<IDWriteFontFamily> family;
            if(SUCCEEDED(fonts_->GetFontFromFontFace(face,&font))) {
                receipt.resolved_weight=static_cast<std::uint32_t>(font->GetWeight());
                receipt.resolved_style=static_cast<std::uint32_t>(font->GetStyle());
                if(SUCCEEDED(font->GetFontFamily(&family))&&FAILED(family->GetFamilyNames(&families)))families.Reset();
                if(FAILED(font->GetFaceNames(&faces)))faces.Reset();
            }
        }
        receipt.family=metadata_name(families.Get(),locale_);receipt.face=metadata_name(faces.Get(),locale_);
        if(!receipt.family)warn(receipt,"FONT_FAMILY_UNKNOWN: The actual run family name is unavailable.");
        else {
            add_unique(result_.used_fonts,*receipt.family);
            bool requested=false,complete=true;
            if(families&&families->GetCount()<=4096) {
                for(UINT32 i=0;i<families->GetCount();++i)try {
                    if(equal_name(localized_string(families.Get(),i),requested_)){requested=true;break;}
                } catch(const Error&){complete=false;}
            } else complete=false;
            if(requested||complete)receipt.fallback=!requested;
            if(receipt.fallback.value_or(false))warn(receipt,"FONT_FALLBACK: Installed family '"+*receipt.family+
                "' supplies part of this text; the requested family is retained.");
        }
        if(!receipt.face)warn(receipt,"FONT_FACE_UNKNOWN: The actual run face name is unavailable.");
        if(!receipt.resolved_weight||!receipt.resolved_style)warn(receipt,"FONT_STYLE_METADATA_UNKNOWN: Actual face weight/style metadata is unavailable.");
        if(!receipt.fallback)warn(receipt,"FONT_FALLBACK_UNKNOWN: Requested-family matching metadata is incomplete.");
    }
    void collect_files(IDWriteFontFace* face,TextFontRun& receipt) {
        UINT32 count=0;
        if(FAILED(face->GetFiles(&count,nullptr))||count==0||count>16) {
            warn(receipt,"FONT_FILE_REFERENCE_UNKNOWN: Font file references are missing or exceed the metadata bound.");return;
        }
        std::vector<IDWriteFontFile*> raw(count,nullptr);
        std::vector<ComPtr<IDWriteFontFile>> files(count);
        const auto hr=face->GetFiles(&count,raw.data());
        for(std::size_t i=0;i<raw.size();++i)files[i].Attach(raw[i]);
        if(FAILED(hr)) {warn(receipt,"FONT_FILE_REFERENCE_UNKNOWN: Cannot read actual font file references.");return;}
        for(const auto& file:files) {
            const void* key=nullptr;UINT32 size=0;ComPtr<IDWriteFontFileLoader> loader;ComPtr<IUnknown> identity;
            if(!file||FAILED(file->GetReferenceKey(&key,&size))||!key||size==0||size>65536||
                FAILED(file->GetLoader(&loader))||FAILED(loader.As(&identity))) {
                warn(receipt,"FONT_FILE_REFERENCE_UNKNOWN: Font file loader/key metadata is unavailable or exceeds the metadata bound.");continue;
            }
            // Copy while IDWriteFontFile owns the returned pointer. Never expose a
            // loader key (which may encode a user's path) or treat it as file bytes.
            const auto* bytes=static_cast<const unsigned char*>(key);
            const std::vector<unsigned char> copied_key(bytes,bytes+size);
            const auto digest=reference_digest(copied_key);
            if(!digest){warn(receipt,"FONT_FILE_REFERENCE_UNKNOWN: Cannot fingerprint the loader-scoped reference key.");continue;}
            auto found=std::find_if(loaders_.begin(),loaders_.end(),[&](const auto& entry){return entry.Get()==identity.Get();});
            if(found==loaders_.end()){loaders_.push_back(identity);found=loaders_.end()-1;}
            ComPtr<IDWriteLocalFontFileLoader> local;
            receipt.files.push_back({"layout_loader_"+std::to_string(found-loaders_.begin()+1),*digest,size,
                SUCCEEDED(loader.As(&local))});
        }
    }
    void collect_axes(IDWriteFontFace* face,TextFontRun& receipt) {
        ComPtr<IDWriteFontFace5> face5;
        if(FAILED(face->QueryInterface(IID_PPV_ARGS(&face5)))) {
            warn(receipt,"FONT_AXIS_METADATA_UNKNOWN: Actual face axis readback is unavailable.");return;
        }
        receipt.has_variations=face5->HasVariations()!=FALSE;
        auto axis=[&](const std::string& tag)->TextFontAxis& {
            const auto found=std::find_if(receipt.axes.begin(),receipt.axes.end(),[&](const auto& item){return item.tag==tag;});
            if(found!=receipt.axes.end())return *found;
            receipt.axes.push_back({tag,{},{},{},{},{}});return receipt.axes.back();
        };
        const auto count=face5->GetFontAxisValueCount();
        if(count<=256) {
            std::vector<DWRITE_FONT_AXIS_VALUE> values(count);
            if(count==0||SUCCEEDED(face5->GetFontAxisValues(values.data(),count))) {
                receipt.axis_values_known=true;
                for(const auto& value:values) {
                    if(!std::isfinite(value.value)){receipt.axis_values_known=false;continue;}
                    axis(tag_string(value.axisTag)).value=value.value;
                }
            }
        }
        if(!receipt.axis_values_known)warn(receipt,"FONT_AXIS_METADATA_UNKNOWN: Actual axis values are unavailable or exceed the metadata bound.");
        ComPtr<IDWriteFontResource> resource;
        if(SUCCEEDED(face5->GetFontResource(&resource))) {
            const auto range_count=resource->GetFontAxisCount();
            if(range_count<=256) {
                std::vector<DWRITE_FONT_AXIS_RANGE> ranges(range_count);
                if(range_count==0||SUCCEEDED(resource->GetFontAxisRanges(ranges.data(),range_count))) {
                    receipt.axis_ranges_known=true;
                    for(UINT32 i=0;i<range_count;++i) {
                        const auto& range=ranges[i];
                        if(!std::isfinite(range.minValue)||!std::isfinite(range.maxValue)||range.minValue>range.maxValue) {
                            receipt.axis_ranges_known=false;continue;
                        }
                        auto& entry=axis(tag_string(range.axisTag));entry.minimum=range.minValue;entry.maximum=range.maxValue;
                        entry.variable=(resource->GetFontAxisAttributes(i)&DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE)!=0;
                    }
                }
                std::vector<DWRITE_FONT_AXIS_VALUE> defaults(range_count);
                if(range_count&&SUCCEEDED(resource->GetDefaultFontAxisValues(defaults.data(),range_count)))
                    for(const auto& value:defaults)if(std::isfinite(value.value))axis(tag_string(value.axisTag)).default_value=value.value;
            }
        }
        if(!receipt.axis_ranges_known)warn(receipt,"FONT_AXIS_METADATA_UNKNOWN: Actual font resource axis ranges are unavailable or exceed the metadata bound.");
    }
    void check_axes(TextFontRun& receipt) {
        auto requested=result_.font_request.additional_axis_values;
        requested.emplace("wght",result_.font_request.weight);requested.emplace("ital",result_.font_request.italic?1.0:0.0);
        for(const auto& [tag,value]:requested) {
            const bool legacy=tag=="wght"||tag=="ital";
            TextFontAxisCheck check;check.tag=tag;check.requested=value;
            check.owner=legacy?(tag=="wght"?"text.weight":"text.italic"):"additional_axis_values";
            const auto submitted=result_.font_request.submitted_axis_values.find(tag);
            if(submitted!=result_.font_request.submitted_axis_values.end())check.submitted=submitted->second;
            const auto actual=std::find_if(receipt.axes.begin(),receipt.axes.end(),[&](const auto& axis){return axis.tag==tag;});
            if(actual!=receipt.axes.end())check.resolved=actual->value;
            if(legacy&&receipt.has_variations==false)check.status="legacy_static_selection";
            else if(result_.font_request.axis_application=="unsupported_api")check.status="unsupported_api";
            else if(!check.submitted)check.status="not_submitted_float_range";
            else if(!receipt.axis_values_known||!receipt.axis_ranges_known)check.status="unknown_metadata";
            else if(actual==receipt.axes.end()||!actual->minimum||!actual->maximum)check.status="unsupported_axis";
            else if(!actual->variable)check.status="unknown_metadata";
            else if(value<*actual->minimum||value>*actual->maximum)check.status="out_of_range";
            else if(!check.resolved)check.status="unknown_metadata";
            else if(check.resolved!=check.submitted)check.status="resolved_different";
            // VARIABLE=false can still describe a valid discrete/static match
            // over a nonempty range. Report resolution and interpolation separately.
            else if(!*actual->variable)check.status=legacy?"resolved_fixed_default":"resolved_noninterpolating";
            else check.status=value==*check.submitted?"resolved":"resolved_float_rounded";
            if(actual!=receipt.axes.end()&&actual->variable==false&&!legacy)
                warn(receipt,"FONT_AXIS_NONINTERPOLATING: Axis '"+tag+"' on this actual run does not interpolate; its resolved value is reported separately.");
            if(check.status!="resolved"&&check.status!="resolved_float_rounded"&&check.status!="resolved_fixed_default"&&
                check.status!="resolved_noninterpolating"&&check.status!="legacy_static_selection")
                warn(receipt,"FONT_AXIS_INCOMPATIBLE: Axis '"+tag+"' has status '"+check.status+"' on this actual run; authored intent is retained.");
            receipt.axis_checks.push_back(std::move(check));
        }
    }
    void collect_features(IDWriteFontFace* face,TextFontRun& receipt) {
        if(result_.font_request.font_features.empty())return;
        if(!scripts_||!receipt.utf16_start||!receipt.utf16_length||!receipt.locale) {
            warn(receipt,"FONT_FEATURE_AVAILABILITY_UNKNOWN: Script, locale or run range metadata is unavailable.");return;
        }
        const auto end=*receipt.utf16_start+*receipt.utf16_length;
        for(auto position=*receipt.utf16_start;position<end;) {
            if(position>=scripts_->scripts.size()||!scripts_->scripts[position]) {
                warn(receipt,"FONT_FEATURE_AVAILABILITY_UNKNOWN: Script analysis is incomplete for this run.");return;
            }
            const auto script=*scripts_->scripts[position];auto next=position+1;
            while(next<end&&scripts_->scripts[next]&&scripts_->scripts[next]->script==script.script&&scripts_->scripts[next]->shapes==script.shapes)++next;
            TextFontScriptFeatures context;context.utf16_start=position;context.utf16_length=next-position;
            context.script=script.script;context.shapes=static_cast<unsigned>(script.shapes);
            const auto actual_locale=utf16(*receipt.locale);
            const auto key=std::make_tuple(face,script.script,static_cast<unsigned>(script.shapes),actual_locale);
            auto found=feature_cache_.find(key);
            if(found==feature_cache_.end()) {
                std::optional<std::vector<DWRITE_FONT_FEATURE_TAG>> supported;
                UINT32 count=0;
                const auto hr=analyzer_->GetTypographicFeatures(face,script,actual_locale.c_str(),0,&count,nullptr);
                if((SUCCEEDED(hr)||hr==E_NOT_SUFFICIENT_BUFFER)&&count<=4096) {
                    std::vector<DWRITE_FONT_FEATURE_TAG> tags(count);
                    if(count==0||SUCCEEDED(analyzer_->GetTypographicFeatures(face,script,actual_locale.c_str(),count,&count,tags.data()))) {
                        if(count<=tags.size()){tags.resize(count);supported=std::move(tags);}
                    }
                }
                ComPtr<IDWriteFontFace> retained=face;feature_faces_.push_back(retained);
                found=feature_cache_.emplace(key,std::move(supported)).first;
            }
            for(const auto& feature:result_.font_request.font_features) {
                TextFontFeatureCheck check{feature.feature_tag,feature.parameter,"unknown"};
                if(found->second) {
                    const auto tag=static_cast<DWRITE_FONT_FEATURE_TAG>(opentype_tag(feature.feature_tag));
                    check.availability=std::find(found->second->begin(),found->second->end(),tag)!=found->second->end()?
                        "available_including_partial":"unavailable";
                }
                if(check.availability=="unknown")warn(receipt,"FONT_FEATURE_AVAILABILITY_UNKNOWN: Cannot query feature '"+check.tag+"' for this face/script/locale.");
                else if(check.availability=="unavailable")warn(receipt,"FONT_FEATURE_UNAVAILABLE: Feature '"+check.tag+"' is unavailable for this actual face/script/locale; authored intent is retained.");
                context.features.push_back(std::move(check));
            }
            receipt.script_features.push_back(std::move(context));position=next;
        }
    }
    void collect_run(const DWRITE_GLYPH_RUN& run,const DWRITE_GLYPH_RUN_DESCRIPTION* description) {
        if(!std::isfinite(run.fontEmSize)||run.fontEmSize<=0)throw Error("TEXT_OUTLINE_INVALID","DirectWrite returned an invalid run em size");
        TextFontRun receipt;receipt.face_index=run.fontFace->GetIndex();receipt.simulations=run.fontFace->GetSimulations();
        receipt.bidi_level=run.bidiLevel;receipt.sideways=run.isSideways!=FALSE;receipt.font_em_size=run.fontEmSize;
        if(std::isfinite(run.fontEmSize)&&result_.font_request.layout_em_size>0)
            receipt.em_size_ratio=run.fontEmSize/result_.font_request.layout_em_size;
        else warn(receipt,"FONT_EM_SIZE_UNKNOWN: Actual run em-size ratio is unavailable.");
        if(description&&description->textPosition<=text_size_&&description->stringLength<=text_size_-description->textPosition) {
            receipt.utf16_start=description->textPosition;receipt.utf16_length=description->stringLength;
        } else warn(receipt,"FONT_RUN_RANGE_UNKNOWN: Actual UTF-16 run range is unavailable or invalid.");
        if(description&&description->localeName)try{receipt.locale=utf8(description->localeName);}catch(const Error&){}
        if(!receipt.locale)warn(receipt,"FONT_RUN_LOCALE_UNKNOWN: Actual glyph-run locale is unavailable.");
        receipt.glyph_indices.assign(run.glyphIndices,run.glyphIndices+run.glyphCount);
        receipt.missing_glyph_count=static_cast<std::size_t>(std::count(receipt.glyph_indices.begin(),receipt.glyph_indices.end(),0));
        if(receipt.missing_glyph_count)warn(receipt,"MISSING_GLYPH: Some characters have no installed glyph; a missing-glyph outline is shown.");
        if(run.glyphAdvances) {
            std::vector<double> advances(run.glyphAdvances,run.glyphAdvances+run.glyphCount);
            if(std::all_of(advances.begin(),advances.end(),[](double value){return std::isfinite(value);}))receipt.glyph_advances=std::move(advances);
        }
        if(!receipt.glyph_advances)warn(receipt,"FONT_GLYPH_ADVANCES_UNKNOWN: Actual glyph advances are unavailable; no predicted advances are reported.");
        collect_names(run.fontFace,receipt);collect_files(run.fontFace,receipt);collect_axes(run.fontFace,receipt);
        check_axes(receipt);collect_features(run.fontFace,receipt);result_.font_runs.push_back(std::move(receipt));
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
    if(source.path_attachment) {
        if(source.path_attachment->start_mode!="distance"&&source.path_attachment->start_mode!="normalized")
            throw Error("TEXT_PATH_START_MODE","Text-on-Path start mode must be distance or normalized");
        if(source.layout!="auto")throw Error("TEXT_PATH_LAYOUT_UNSUPPORTED","Text-on-Path requires automatic one-line layout");
        if(source.direction!="horizontal")throw Error("TEXT_PATH_DIRECTION_UNSUPPORTED","Text-on-Path requires horizontal writing");
        if(std::any_of(text.begin(),text.end(),[](wchar_t c){return c==L'\n'||c==L'\r'||c==L'\v'||c==L'\f'||c==0x2028||c==0x2029;}))
            throw Error("TEXT_PATH_MULTILINE_UNSUPPORTED","Text-on-Path does not support hard line breaks");
    }
    const auto font_size=parameter(parameters,"font_size"),tracking=parameter(parameters,"tracking"),spacing=parameter(parameters,"line_spacing");
    const auto origin_x=parameter(parameters,"origin_x"),origin_y=parameter(parameters,"origin_y");
    const auto frame_width=parameter(parameters,"frame_width"),frame_height=parameter(parameters,"frame_height");
    if(font_size<=0||frame_width<=0||frame_height<=0||spacing<0)throw Error("TEXT_PARAMETER","Font and frame sizes must be positive; line spacing cannot be negative");
    if(source.layout!="auto"&&source.layout!="frame")throw Error("TEXT_LAYOUT_UNSUPPORTED","Text layout must be auto or frame");
    if(source.direction!="horizontal"&&source.direction!="vertical")throw Error("TEXT_DIRECTION_UNSUPPORTED","Text direction must be horizontal or vertical");
    if(source.alignment!="start"&&source.alignment!="center"&&source.alignment!="end")throw Error("TEXT_ALIGNMENT_UNSUPPORTED","Text alignment must be start, center or end");
    const bool automatic=source.layout=="auto",vertical=source.direction=="vertical";
    TextLayout result;result.x=origin_x;result.y=origin_y;
    result.font_request.family=source.family;result.font_request.locale=source.locale;
    result.font_request.weight=source.weight;result.font_request.italic=source.italic;
    result.font_request.requested_em_size=parameters.at("font_size");result.font_request.layout_em_size=font_size;
    result.font_request.font_features=source.font_features;result.font_request.additional_axis_values=source.additional_axis_values;
    ComPtr<IDWriteFactory2> factory;
    check_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory2),reinterpret_cast<IUnknown**>(factory.GetAddressOf())),"Create DirectWrite factory");
    ComPtr<IDWriteFontCollection> fonts;check_hr(factory->GetSystemFontCollection(&fonts),"Get installed fonts");
    UINT32 family_index=0;BOOL family_exists=FALSE;
    check_hr(fonts->FindFamilyName(family.c_str(),&family_index,&family_exists),"Find requested font family");
    if(!family_exists)add_unique(result.warnings,"MISSING_FONT: Requested family '"+source.family+"' is not installed; DirectWrite system fallback is used.");
    ComPtr<IDWriteTextFormat> format;
    check_hr(factory->CreateTextFormat(family.c_str(),fonts.Get(),static_cast<DWRITE_FONT_WEIGHT>(source.weight),
        source.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,font_size,locale.c_str(),&format),"Create text format");
    submit_axes(format.Get(),source,result);
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
    submit_features(factory.Get(),layout.Get(),source,static_cast<UINT32>(text.size()),result);
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
    ComPtr<OutlineRenderer> renderer;renderer.Attach(new OutlineRenderer(analyzer.Get(),fonts.Get(),family,locale,text,*contours,result,vertical,
        source.path_attachment.has_value()));
    const auto hr=layout->Draw(nullptr,renderer.Get(),origin_x-(automatic?metrics.left:0),draw_origin_y);
    if(renderer->failure)std::rethrow_exception(renderer->failure);
    check_hr(hr,"Draw shaped text outlines");
    if(vertical) {
        UINT32 line_count=0;
        const auto line_hr=layout->GetLineMetrics(nullptr,0,&line_count);
        if(FAILED(line_hr)&&line_hr!=E_NOT_SUFFICIENT_BUFFER)check_hr(line_hr,"Measure vertical columns");
        if(line_count>32769)throw Error("TEXT_LAYOUT_LIMIT","Text has too many columns");
        std::vector<DWRITE_LINE_METRICS> lines(line_count);
        if(line_count)check_hr(layout->GetLineMetrics(lines.data(),line_count,&line_count),"Measure vertical column positions");
        auto runs=renderer->column_run_origins;
        std::sort(runs.begin(),runs.end(),[](const auto& a,const auto& b){return a.first<b.first;});
        std::uint64_t start=0;
        std::size_t run_index=0;
        bool complete=true;
        for(UINT32 i=0;i<line_count;++i) {
            const auto end=start+lines[i].length;
            std::optional<double> baseline;
            while(run_index<runs.size()&&runs[run_index].first<start)++run_index;
            while(run_index<runs.size()&&runs[run_index].first<end) {
                const auto x=runs[run_index++].second;
                if(baseline&&std::abs(x-*baseline)>1e-5) {complete=false;break;}
                baseline=x;
            }
            if(!complete||!baseline) {complete=false;break;}
            result.column_baselines_x.push_back(*baseline);
            start=end;
        }
        if(!complete)result.column_baselines_x.clear();
    }
    result.contours=std::move(contours);
    if(source.path_attachment)result.x=result.y=result.width=result.height=0;
    return result;
#endif
}
} // namespace nect
