#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <exception>
#include <new>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite_3.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <atomic>
#endif
using namespace nect;
namespace {
int checks=0,missing=0;
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);++checks;}
std::map<std::string,double> values(const TextSource& source) {
    std::map<std::string,double> result;for(const auto& [key,value]:source.parameters)result[key]=value.literal;return result;
}
bool warning(const TextLayout& layout,const std::string& code) {
    return std::any_of(layout.warnings.begin(),layout.warnings.end(),[&](const auto& value){return value.starts_with(code);});
}
#ifdef _WIN32
using Microsoft::WRL::ComPtr;
std::string oracle_detail(const char* stage,const char* reason,std::optional<HRESULT> result={}) {
    std::ostringstream out;out<<"stage="<<stage<<" reason="<<reason;
    if(result)out<<" HRESULT=0x"<<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(8)<<static_cast<std::uint32_t>(*result);
    return out.str();
}
struct FixtureUnavailable : std::runtime_error {
    FixtureUnavailable(const char* stage,const char* reason,std::optional<HRESULT> result={}):std::runtime_error(oracle_detail(stage,reason,result)){}
};
void oracle_hr(HRESULT result,const char* call) {
    if(FAILED(result))throw std::runtime_error(oracle_detail(call,"HRESULT_CALL_FAILED",result));
}
// Preserve the exact API expression as context without changing default checks.
#define hr(call) oracle_hr((call),#call)
void interface_hr(HRESULT result,const char* stage) {
    // Only a required-interface query's documented absence is a capability skip.
    if(result==E_NOINTERFACE)throw FixtureUnavailable(stage,"REQUIRED_INTERFACE_UNAVAILABLE",result);
    oracle_hr(result,stage);
}
struct CallbackFailure {
    std::exception_ptr failure;
    template<class F> HRESULT invoke(F action) noexcept {
        try{return action();}catch(...){if(!failure)failure=std::current_exception();return E_FAIL;}
    }
    void rethrow() const {if(failure)std::rethrow_exception(failure);}
};
template<class F> int fixture_exit(bool& admitted,F action,std::ostream& output) {
    try{action();return 0;}
    catch(const FixtureUnavailable& error) {
        output<<(admitted?"FAIL: ":"ENV_MISSING_FIXTURE: ")<<"OTF_FEATURE_SOURCE_SANS_3 "<<error.what()<<'\n';return admitted?1:77;
    }
    catch(const std::exception& error){output<<"FAIL: OTF_FEATURE_SOURCE_SANS_3 "<<error.what()<<'\n';return 1;}
    catch(...){output<<"FAIL: OTF_FEATURE_SOURCE_SANS_3 unexpected nonstandard exception\n";return 1;}
}
std::wstring wide(const std::string& source) {
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.data(),static_cast<int>(source.size()),nullptr,0);
    std::wstring result(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.data(),static_cast<int>(source.size()),result.data(),count);return result;
}
std::string fixture_name(IDWriteLocalizedStrings* names) {
    if(!names||names->GetCount()==0)throw std::runtime_error("stage=fixture_name reason=INVALID_LOCALIZED_NAMES");
    UINT32 index=0,size=0;BOOL exists=FALSE;hr(names->FindLocaleName(L"en-us",&index,&exists));
    if(!exists)index=0;hr(names->GetStringLength(index,&size));std::wstring name(size+1,L'\0');
    hr(names->GetString(index,name.data(),size+1));name.resize(size);
    const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name.data(),static_cast<int>(size),nullptr,0,nullptr,nullptr);
    if(count<=0)throw std::runtime_error("Independent fixture name is unavailable");
    std::string result(count,'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name.data(),static_cast<int>(size),result.data(),count,nullptr,nullptr)!=count)
        throw std::runtime_error("Independent fixture name conversion failed");
    return result;
}
bool cff_face(IDWriteFontFace* face) {
    const void* data=nullptr;UINT32 size=0;void* context=nullptr;BOOL exists=FALSE;
    hr(face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('C','F','F',' '),&data,&size,&context,&exists));
    if(exists&&(!data||size==0)){face->ReleaseFontTable(context);throw std::runtime_error("stage=TryGetFontTable reason=INVALID_CFF_TABLE_RESULT");}
    const bool result=face->GetType()==DWRITE_FONT_FACE_TYPE_CFF&&exists&&data&&size>0;
    if(exists)face->ReleaseFontTable(context);return result;
}
std::vector<TextFontFileReference> fixture_files(IDWriteFontFace* face) {
    UINT32 count=0;hr(face->GetFiles(&count,nullptr));
    if(count==0||count>16)throw std::runtime_error("stage=GetFiles reason=INVALID_FILE_COUNT");
    std::vector<IDWriteFontFile*> raw(count,nullptr);std::vector<ComPtr<IDWriteFontFile>> files(count);
    const auto result=face->GetFiles(&count,raw.data());
    for(std::size_t i=0;i<raw.size();++i)files[i].Attach(raw[i]);oracle_hr(result,"IDWriteFontFace::GetFiles");
    std::vector<TextFontFileReference> references;
    for(const auto& file:files) {
        if(!file)throw std::runtime_error("stage=GetFiles reason=NULL_FILE_REFERENCE");
        const void* key=nullptr;UINT32 size=0;hr(file->GetReferenceKey(&key,&size));
        if(!key||size==0||size>65536)throw std::runtime_error("stage=GetReferenceKey reason=INVALID_KEY_RESULT");
        ComPtr<IDWriteFontFileLoader> loader;ComPtr<IDWriteLocalFontFileLoader> local;hr(file->GetLoader(&loader));
        unsigned char digest[32]{};
        if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(static_cast<const unsigned char*>(key)),size,digest,32)<0)
            throw std::runtime_error("Independent fixture key fingerprint failed");
        std::string hex;for(const auto byte:digest){hex+="0123456789abcdef"[byte>>4];hex+="0123456789abcdef"[byte&15];}
        references.push_back({"",hex,size,SUCCEEDED(loader.As(&local))});
    }
    return references; // Loader keys are hashed in place; no font bytes or paths escape.
}
// Capture actual drawn faces; never use requested format getters as evidence.
// The legacy route leaves axis_family empty and does not require Face5.
class LegacyCapture final : public IDWriteTextRenderer {
public:
    struct Run {
        std::vector<UINT16> glyphs;std::vector<double> advances;UINT32 index=0,simulations=0;
        DWRITE_FONT_WEIGHT weight{};DWRITE_FONT_STYLE style{};
        std::vector<DWRITE_FONT_AXIS_VALUE> axes;UINT32 start=0,length=0;
        std::string family,face;std::vector<TextFontFileReference> files;
    };
    explicit LegacyCapture(std::wstring axis_family={},bool source_sans=false):axis_family_(std::move(axis_family)),source_sans_(source_sans){}
    std::vector<Run> runs;
    CallbackFailure callback;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** object) override {
        if(!object)return E_POINTER;*object=nullptr;
        if(iid!=__uuidof(IUnknown)&&iid!=__uuidof(IDWriteTextRenderer)&&iid!=__uuidof(IDWritePixelSnapping))return E_NOINTERFACE;
        *object=static_cast<IDWriteTextRenderer*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override {const auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,BOOL* value) override {*value=TRUE;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,DWRITE_MATRIX* value) override {*value={1,0,0,1,0,0};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*,FLOAT* value) override {*value=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT,FLOAT,DWRITE_MEASURING_MODE,const DWRITE_GLYPH_RUN* value,const DWRITE_GLYPH_RUN_DESCRIPTION* description,IUnknown*) override {
        return callback.invoke([&]() -> HRESULT {
            if(!value||!value->fontFace||!value->glyphIndices||!value->glyphAdvances)throw std::invalid_argument("stage=DrawGlyphRun reason=INVALID_RUN_INPUT");
            ComPtr<IDWriteFontFace3> face;const auto face_hr=value->fontFace->QueryInterface(IID_PPV_ARGS(&face));
            if(source_sans_)interface_hr(face_hr,"DrawGlyphRun.IDWriteFontFace3");else oracle_hr(face_hr,"DrawGlyphRun.IDWriteFontFace3");
            Run run{{value->glyphIndices,value->glyphIndices+value->glyphCount},
                {value->glyphAdvances,value->glyphAdvances+value->glyphCount},value->fontFace->GetIndex(),
                static_cast<UINT32>(value->fontFace->GetSimulations()),face->GetWeight(),face->GetStyle()};
            if(source_sans_) {
                if(!description)throw std::invalid_argument("stage=DrawGlyphRun reason=MISSING_DESCRIPTION");
                if(!cff_face(value->fontFace))throw std::runtime_error("stage=DrawGlyphRun reason=UNEXPECTED_NON_CFF_RUN");
                ComPtr<IDWriteLocalizedStrings> families,names;hr(face->GetFamilyNames(&families));hr(face->GetFaceNames(&names));
                run.family=fixture_name(families.Get());run.face=fixture_name(names.Get());
                if(run.family!="Source Sans 3")throw std::runtime_error("stage=DrawGlyphRun reason=UNEXPECTED_FALLBACK_FAMILY");
                run.start=description->textPosition;run.length=description->stringLength;run.files=fixture_files(value->fontFace);
                if(run.glyphs.empty()||std::find(run.glyphs.begin(),run.glyphs.end(),0)!=run.glyphs.end()||
                    std::any_of(run.advances.begin(),run.advances.end(),[](double advance){return !std::isfinite(advance);}))
                    throw std::runtime_error("stage=DrawGlyphRun reason=INVALID_GLYPH_OR_ADVANCE");
            }
            if(!axis_family_.empty()) {
                if(!description)return E_FAIL;
                run.start=description->textPosition;run.length=description->stringLength;
                ComPtr<IDWriteFontFace5> face5;hr(face.As(&face5));
                if(!face5->HasVariations())return E_FAIL;
                const auto count=face5->GetFontAxisValueCount();if(count==0||count>256)return E_FAIL;
                run.axes.resize(count);hr(face5->GetFontAxisValues(run.axes.data(),count));
                if(std::any_of(run.axes.begin(),run.axes.end(),[](const auto& axis){return !std::isfinite(axis.value);}))return E_FAIL;
                ComPtr<IDWriteLocalizedStrings> names;hr(face->GetFamilyNames(&names));bool matches=false;
                for(UINT32 i=0;i<names->GetCount();++i) {
                    UINT32 size=0;hr(names->GetStringLength(i,&size));std::wstring name(size+1,L'\0');
                    hr(names->GetString(i,name.data(),size+1));name.resize(size);matches=matches||name==axis_family_;
                }
                if(!matches)return E_FAIL; // Never admit a fallback face as the fixture.
            }
            runs.push_back(std::move(run));return S_OK;
        });
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,const DWRITE_UNDERLINE*,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,const DWRITE_STRIKETHROUGH*,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) override {return E_NOTIMPL;}
private:std::atomic<ULONG> refs_{1};std::wstring axis_family_;bool source_sans_=false;
};
// Independent legacy CreateTextFormat route, deliberately without TextFormat3.
std::vector<LegacyCapture::Run> legacy(const TextSource& source) {
    ComPtr<IDWriteFactory> factory;hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
    ComPtr<IDWriteFontCollection> fonts;hr(factory->GetSystemFontCollection(&fonts));
    ComPtr<IDWriteTextFormat> format;const auto family=wide(source.family),locale=wide(source.locale),text=wide(source.content);
    hr(factory->CreateTextFormat(family.c_str(),fonts.Get(),static_cast<DWRITE_FONT_WEIGHT>(source.weight),
        source.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,48,locale.c_str(),&format));
    hr(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    ComPtr<IDWriteTextLayout> layout;hr(factory->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),1000000,1000000,&layout));
    ComPtr<LegacyCapture> renderer;renderer.Attach(new LegacyCapture);
    const auto draw_hr=layout->Draw(nullptr,renderer.Get(),0,0);renderer->callback.rethrow();oracle_hr(draw_hr,"legacy.layout.Draw");return renderer->runs;
}
// Same installed fixture, combined axes and layout settings as the variable
// test, constructed independently of evaluate_text and its submission/receipt.
// Face5 documents actual supported coordinates, not an exact submission echo:
// https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/nf-dwrite_3-idwritefontface5-getfontaxisvalues
std::vector<LegacyCapture::Run> variable_oracle(const TextSource& source) {
    check(source.layout=="auto"&&source.direction=="horizontal"&&source.alignment=="start"&&
        source.font_features.empty()&&source.parameters.at("line_spacing").literal==0&&
        source.additional_axis_values.size()==1&&source.additional_axis_values.contains("wdth"),
        "Variable oracle is scoped to this whole-text width fixture and layout");
    ComPtr<IDWriteFactory2> factory;hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory2),reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
    ComPtr<IDWriteFontCollection> fonts;hr(factory->GetSystemFontCollection(&fonts));
    const auto family=wide(source.family),locale=wide(source.locale),text=wide(source.content);
    ComPtr<IDWriteTextFormat> format;hr(factory->CreateTextFormat(family.c_str(),fonts.Get(),static_cast<DWRITE_FONT_WEIGHT>(source.weight),
        source.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,
        static_cast<float>(source.parameters.at("font_size").literal),locale.c_str(),&format));
    std::vector<DWRITE_FONT_AXIS_VALUE> axes{{DWRITE_FONT_AXIS_TAG_WEIGHT,static_cast<float>(source.weight)},
        {DWRITE_FONT_AXIS_TAG_ITALIC,source.italic?1.0f:0.0f}};
    const auto width=source.additional_axis_values.at("wdth");
    if(std::abs(width)<=std::numeric_limits<float>::max())axes.push_back({DWRITE_FONT_AXIS_TAG_WIDTH,static_cast<float>(width)});
    ComPtr<IDWriteTextFormat3> format3;hr(format.As(&format3));hr(format3->SetFontAxisValues(axes.data(),static_cast<UINT32>(axes.size())));
    hr(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));hr(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_NONE,0,0};hr(format->SetTrimming(&trimming,nullptr));
    ComPtr<IDWriteTextLayout> base;ComPtr<IDWriteTextLayout2> layout;
    hr(factory->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),1000000,1000000,&base));hr(base.As(&layout));
    hr(layout->SetVerticalGlyphOrientation(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT));hr(layout->SetLastLineWrapping(TRUE));
    hr(layout->SetCharacterSpacing(0,static_cast<float>(source.parameters.at("tracking").literal),0,{0,static_cast<UINT32>(text.size())}));
    DWRITE_TEXT_METRICS1 metrics{};hr(layout->GetMetrics(&metrics));
    hr(layout->SetMaxWidth(std::max(0.001f,metrics.widthIncludingTrailingWhitespace)));
    hr(layout->SetMaxHeight(std::max(0.001f,metrics.heightIncludingTrailingWhitespace)));hr(layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
    ComPtr<LegacyCapture> renderer;renderer.Attach(new LegacyCapture(family));
    const auto draw_hr=layout->Draw(nullptr,renderer.Get(),0,0);renderer->callback.rethrow();oracle_hr(draw_hr,"variable.layout.Draw");return renderer->runs;
}
struct VariableCapability {
    DWRITE_FONT_AXIS_RANGE weight{},width{};
};
// Fixture admission must not call evaluate_text: a broken product receipt is a
// product failure, never evidence that the host lacks a variable-font fixture.
std::optional<VariableCapability> variable_capability(const std::string& name) {
    ComPtr<IDWriteFactory> factory;
    if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))return {};
    ComPtr<IDWriteFontCollection> collection;
    if(FAILED(factory->GetSystemFontCollection(&collection)))return {};
    const auto family_name=wide(name);UINT32 index=0;BOOL exists=FALSE;
    if(FAILED(collection->FindFamilyName(family_name.c_str(),&index,&exists))||!exists)return {};
    ComPtr<IDWriteFontFamily> family;ComPtr<IDWriteFont> font;ComPtr<IDWriteFontFace> face;
    if(FAILED(collection->GetFontFamily(index,&family))||
        FAILED(family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STYLE_NORMAL,&font))||
        FAILED(font->CreateFontFace(&face)))return {};
    ComPtr<IDWriteFontFace5> face5;ComPtr<IDWriteFontResource> resource;
    if(FAILED(face.As(&face5))||!face5->HasVariations()||FAILED(face5->GetFontResource(&resource)))return {};
    const auto count=resource->GetFontAxisCount();
    if(count==0||count>256)return {};
    std::vector<DWRITE_FONT_AXIS_RANGE> ranges(count);
    if(FAILED(resource->GetFontAxisRanges(ranges.data(),count)))return {};
    std::optional<DWRITE_FONT_AXIS_RANGE> weight,width;
    for(UINT32 i=0;i<count;++i) {
        const auto& range=ranges[i];
        if(!std::isfinite(range.minValue)||!std::isfinite(range.maxValue)||range.minValue>=range.maxValue||
            (resource->GetFontAxisAttributes(i)&DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE)==0)continue;
        if(range.axisTag==DWRITE_FONT_AXIS_TAG_WEIGHT)weight=range;
        if(range.axisTag==DWRITE_FONT_AXIS_TAG_WIDTH)width=range;
    }
    if(!weight||!width||weight->minValue<1||weight->maxValue>999)return {};
    ComPtr<IDWriteTextFormat> format;ComPtr<IDWriteTextFormat3> format3;
    if(FAILED(factory->CreateTextFormat(family_name.c_str(),collection.Get(),DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,48,L"en-us",&format))||FAILED(format.As(&format3)))return {};
    const DWRITE_FONT_AXIS_VALUE values[]={{DWRITE_FONT_AXIS_TAG_WEIGHT,weight->maxValue},
        {DWRITE_FONT_AXIS_TAG_ITALIC,0},{DWRITE_FONT_AXIS_TAG_WIDTH,(width->minValue+width->maxValue)/2}};
    if(FAILED(format3->SetFontAxisValues(values,3)))return {};
    return VariableCapability{*weight,*width};
}
const TextFontAxisCheck& axis_check(const TextFontRun& run,const std::string& tag) {
    const auto found=std::find_if(run.axis_checks.begin(),run.axis_checks.end(),[&](const auto& value){return value.tag==tag;});
    check(found!=run.axis_checks.end(),"Every requested axis has an explicit actual-run assessment");return *found;
}
void require_axis(const TextLayout& layout,const std::vector<LegacyCapture::Run>& expected,const std::string& tag,
    const DWRITE_FONT_AXIS_RANGE& range,double requested) {
    check(!expected.empty()&&layout.font_runs.size()==expected.size(),"Variable product and independent oracle emit the same actual run count");
    const std::optional<double> submitted=std::abs(requested)<=std::numeric_limits<float>::max()?
        std::optional<double>{static_cast<float>(requested)}:std::nullopt;
    const auto request_axis=layout.font_request.submitted_axis_values.find(tag);
    check(submitted?(request_axis!=layout.font_request.submitted_axis_values.end()&&request_axis->second==*submitted):
        request_axis==layout.font_request.submitted_axis_values.end(),"Request receipt exposes the exact submitted float or its absence");
    for(std::size_t i=0;i<expected.size();++i) {
        const auto& run=layout.font_runs[i];const auto& oracle=expected[i];
        const auto expected_axis=std::find_if(oracle.axes.begin(),oracle.axes.end(),[&](const auto& axis){return axis.axisTag==range.axisTag;});
        check(expected_axis!=oracle.axes.end(),"Independent drawn Face5 exposes the requested axis tag");
        const auto& axis=axis_check(run,tag);
        std::cout<<"axis_oracle: "<<tag<<" requested="<<std::setprecision(17)<<requested<<" submitted=";
        if(submitted)std::cout<<*submitted;else std::cout<<"none";
        std::cout<<" independent="<<expected_axis->value<<" actual=";
        if(axis.resolved)std::cout<<*axis.resolved;else std::cout<<"unknown";
        std::cout<<" status="<<axis.status<<'\n';
        check(axis.requested==requested&&axis.submitted==submitted&&axis.owner==(tag=="wght"?"text.weight":"additional_axis_values"),
            "Run assessment separates exact authored intent, owner and submitted float");
        check(axis.resolved&&*axis.resolved==expected_axis->value,"Actual Face5 receipt exactly matches independent drawn-face readback");
        const std::string status=!submitted?"not_submitted_float_range":requested<range.minValue||requested>range.maxValue?"out_of_range":
            *submitted!=expected_axis->value?"resolved_different":requested!=*submitted?"resolved_float_rounded":"resolved";
        check(axis.status==status,"Axis assessment truthfully distinguishes submission rounding, backend difference and unsupported intent");
        const auto incompatible="FONT_AXIS_INCOMPATIBLE: Axis '"+tag+"' has status '"+status+"'";
        if(status!="resolved"&&status!="resolved_float_rounded")check(warning(layout,incompatible),"Actual mismatch or unsubmitted intent has an explicit incompatibility warning");
        const auto actual=std::find_if(run.axes.begin(),run.axes.end(),[&](const auto& entry){return entry.tag==tag;});
        check(actual!=run.axes.end()&&actual->value==axis.resolved&&actual->variable==true&&
            actual->minimum==range.minValue&&actual->maximum==range.maxValue,"Actual value and variable range agree with independent fixture evidence");
        check(run.has_variations==true&&run.axis_values_known&&run.axis_ranges_known,"Variable receipt has actual face/resource evidence");
        check(run.glyph_indices==oracle.glyphs&&run.glyph_advances==oracle.advances&&run.face_index==oracle.index&&run.simulations==oracle.simulations&&
            run.resolved_weight==static_cast<std::uint32_t>(oracle.weight)&&run.resolved_style==static_cast<std::uint32_t>(oracle.style)&&
            run.utf16_start==oracle.start&&run.utf16_length==oracle.length&&run.fallback==false,
            "Variable glyphs, advances, face selection and ranges exactly match the independent nonfallback oracle");
    }
}
void require_variable(const TextSource& source,const TextLayout& layout,const VariableCapability& capability) {
    check(layout.font_request.axis_application=="submitted"&&layout.font_request.additional_axis_values==source.additional_axis_values&&
        layout.font_request.weight==source.weight,"Every variable case preserves exact authored request independently of submitted and actual coordinates");
    const auto expected=variable_oracle(source);
    require_axis(layout,expected,"wght",capability.weight,source.weight);
    require_axis(layout,expected,"wdth",capability.width,source.additional_axis_values.at("wdth"));
}
bool same_glyphs(const TextLayout& a,const TextLayout& b) {
    if(a.font_runs.size()!=b.font_runs.size())return false;
    for(std::size_t i=0;i<a.font_runs.size();++i)if(a.font_runs[i].glyph_indices!=b.font_runs[i].glyph_indices||
        a.font_runs[i].glyph_advances!=b.font_runs[i].glyph_advances)return false;
    return true;
}
void ranges(const TextLayout& layout,UINT32 length) {
    std::vector<unsigned> coverage(length,0);std::size_t glyphs=0;
    for(const auto& run:layout.font_runs) {
        check(run.utf16_start&&run.utf16_length,"Actual runs report UTF-16 ranges");
        check(*run.utf16_start<=length&&*run.utf16_length<=length-*run.utf16_start,"UTF-16 ranges remain bounded");
        for(auto i=*run.utf16_start;i<*run.utf16_start+*run.utf16_length;++i)++coverage[i];
        check(run.locale.has_value(),"Actual run locale is present");
        check(run.glyph_advances&&run.glyph_advances->size()==run.glyph_indices.size(),"Actual run glyph/advance arrays agree");
        check(run.em_size_ratio&&*run.em_size_ratio>0,"Em-size ratio is measured from actual drawn run");
        glyphs+=run.glyph_indices.size();
    }
    check(glyphs==layout.glyph_count,"Actual run glyph totals match existing layout count");
    check(std::all_of(coverage.begin(),coverage.end(),[](unsigned n){return n==1;}),"Actual ranges cover every UTF-16 unit exactly once");
}
bool fixture(const std::vector<std::string>& fonts,const std::string& family) {
    if(std::binary_search(fonts.begin(),fonts.end(),family))return true;
    ++missing;std::cout<<"ENV_MISSING_FIXTURE: "<<family<<'\n';return false;
}
void validate_receipt(const TextLayout& layout) {
    for(const auto& run:layout.font_runs) {
        const auto warned=[&](const std::string& code){return std::any_of(run.warnings.begin(),run.warnings.end(),
            [&](const auto& value){return value.starts_with(code);});};
        check(run.family.has_value()||warned("FONT_FAMILY_UNKNOWN:"),"Unknown family is explicitly warned");
        check(run.face.has_value()||warned("FONT_FACE_UNKNOWN:"),"Unknown face is explicitly warned");
        check(!run.files.empty()||warned("FONT_FILE_REFERENCE_UNKNOWN:"),"Unknown file identity is explicitly warned");
        check(run.axis_values_known||warned("FONT_AXIS_METADATA_UNKNOWN:"),"Unknown axis metadata is explicitly warned");
        for(const auto& file:run.files)check(file.key_sha256.size()==64&&file.loader_scope.starts_with("layout_loader_")&&file.key_size>0,
            "File reference identity is a copied loader-scoped digest, never a path or claimed file hash");
    }
}
// Optional G11 fixture admission uses only the installed system collection and
// independent layouts. No product discovery/evaluation participates in admission.
std::vector<LegacyCapture::Run> source_sans_oracle(const TextSource& source) {
    if(source.family!="Source Sans 3"||source.locale!="en-us"||source.weight!=400||source.italic||source.font_features.size()>1||
        std::any_of(source.font_features.begin(),source.font_features.end(),[](const auto& feature){return feature.feature_tag.size()!=4||feature.scope!="whole_text"||feature.parameter>1;}))
        throw std::invalid_argument("stage=oracle.input reason=INVALID_FIXED_FIXTURE_INPUT");
    ComPtr<IDWriteFactory> factory;hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
    ComPtr<IDWriteFontCollection> fonts;hr(factory->GetSystemFontCollection(&fonts));
    const auto family=wide(source.family),locale=wide(source.locale),text=wide(source.content);
    ComPtr<IDWriteTextFormat> format;hr(factory->CreateTextFormat(family.c_str(),fonts.Get(),DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,static_cast<float>(source.parameters.at("font_size").literal),locale.c_str(),&format));
    hr(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));hr(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_NONE,0,0};hr(format->SetTrimming(&trimming,nullptr));
    ComPtr<IDWriteTextLayout> base;ComPtr<IDWriteTextLayout2> layout;
    hr(factory->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),1000000,1000000,&base));interface_hr(base.As(&layout),"oracle.IDWriteTextLayout2");
    if(!source.font_features.empty()) {
        ComPtr<IDWriteTypography> typography;hr(factory->CreateTypography(&typography));
        for(const auto& feature:source.font_features) {
            const auto& tag=feature.feature_tag;
            hr(typography->AddFontFeature({static_cast<DWRITE_FONT_FEATURE_TAG>(DWRITE_MAKE_OPENTYPE_TAG(tag[0],tag[1],tag[2],tag[3])),feature.parameter}));
        }
        hr(layout->SetTypography(typography.Get(),{0,static_cast<UINT32>(text.size())}));
    }
    hr(layout->SetVerticalGlyphOrientation(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT));hr(layout->SetLastLineWrapping(TRUE));
    hr(layout->SetCharacterSpacing(0,static_cast<float>(source.parameters.at("tracking").literal),0,{0,static_cast<UINT32>(text.size())}));
    DWRITE_TEXT_METRICS1 metrics{};hr(layout->GetMetrics(&metrics));
    hr(layout->SetMaxWidth(std::max(0.001f,metrics.widthIncludingTrailingWhitespace)));
    hr(layout->SetMaxHeight(std::max(0.001f,metrics.heightIncludingTrailingWhitespace)));hr(layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
    ComPtr<LegacyCapture> renderer;renderer.Attach(new LegacyCapture({},true));
    const auto draw_hr=layout->Draw(nullptr,renderer.Get(),0,0);renderer->callback.rethrow();oracle_hr(draw_hr,"source_sans.layout.Draw");
    if(renderer->runs.empty())throw std::runtime_error("Independent Source Sans 3 layout has no runs");return renderer->runs;
}
struct SourceSansFixture {
    TextSource source;std::string tag;
    std::vector<LegacyCapture::Run> baseline,off,on;
};
SourceSansFixture source_sans_admission() {
    ComPtr<IDWriteFactory> factory;hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
    ComPtr<IDWriteFontCollection> fonts;hr(factory->GetSystemFontCollection(&fonts));
    UINT32 index=0;BOOL exists=FALSE;hr(fonts->FindFamilyName(L"Source Sans 3",&index,&exists));
    if(!exists)throw FixtureUnavailable("admission.FindFamilyName","FAMILY_NOT_INSTALLED");
    ComPtr<IDWriteFontFamily> family;ComPtr<IDWriteFont> font;ComPtr<IDWriteFontFace> face;
    hr(fonts->GetFontFamily(index,&family));hr(family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STYLE_NORMAL,&font));
    hr(font->CreateFontFace(&face));
    if(!cff_face(face.Get()))throw FixtureUnavailable("admission.face","SYSTEM_REGULAR_FACE_NOT_OTF_CFF");
    // Fixed bounded candidates, chosen by observed glyph/advance effect, not
    // suffix, availability metadata, or Nect output. Never substitute a family.
    for(const auto& [tag,text]:{std::pair<const char*,const char*>{"liga","office affine ffi fi fl ff"},
        {"kern","AVATAR To Wa"},{"ss01","agIl0123456789"}}) {
        auto source=default_text("source-sans-source",text);source.family="Source Sans 3";source.locale="en-us";
        const auto baseline=source_sans_oracle(source);source.font_features={{tag,0,"whole_text"}};
        const auto off=source_sans_oracle(source);source.font_features.front().parameter=1;const auto on=source_sans_oracle(source);
        std::vector<UINT16> off_glyphs,on_glyphs;std::vector<double> off_advances,on_advances;
        for(const auto& run:off){off_glyphs.insert(off_glyphs.end(),run.glyphs.begin(),run.glyphs.end());off_advances.insert(off_advances.end(),run.advances.begin(),run.advances.end());}
        for(const auto& run:on){on_glyphs.insert(on_glyphs.end(),run.glyphs.begin(),run.glyphs.end());on_advances.insert(on_advances.end(),run.advances.begin(),run.advances.end());}
        if(off_glyphs!=on_glyphs||off_advances!=on_advances){source.font_features.clear();return SourceSansFixture{source,tag,baseline,off,on};}
    }
    throw FixtureUnavailable("admission.effect","NO_EFFECTIVE_FEATURE_LIGA_KERN_SS01");
}
// Read the shared adapter's JSON structurally without adding a test target
// dependency. validate_json handles syntax; this walker selects exact members.
std::size_t json_end(std::string_view json,std::size_t at) {
    if(at>=json.size())throw std::runtime_error("Missing JSON readback value");
    if(json[at]=='"') {
        for(auto i=at+1;i<json.size();++i) {
            if(json[i]=='\\'){++i;continue;}if(json[i]=='"')return i+1;
        }
    } else if(json[at]=='{'||json[at]=='[') {
        const auto close=json[at]=='{'?'}':']';auto i=at+1;
        while(i<json.size()) {
            if(json[i]==close)return i+1;
            if(json[i]=='"'||json[i]=='{'||json[i]=='[')i=json_end(json,i);else ++i;
        }
    } else {
        auto i=at;while(i<json.size()&&json[i]!=','&&json[i]!=']'&&json[i]!='}'&&json[i]!=' '&&json[i]!='\n'&&json[i]!='\r'&&json[i]!='\t')++i;
        if(i>at)return i;
    }
    throw std::runtime_error("Incomplete JSON readback value");
}
std::size_t json_space(std::string_view json,std::size_t at) {
    while(at<json.size()&&(json[at]==' '||json[at]=='\n'||json[at]=='\r'||json[at]=='\t'))++at;return at;
}
std::string_view json_member(std::string_view json,const std::string& key) {
    if(json.empty()||json.front()!='{')throw std::runtime_error("Expected JSON readback object");
    auto at=json_space(json,1);
    while(at<json.size()&&json[at]!='}') {
        const auto end=json_end(json,at);const auto name=json.substr(at,end-at);at=json_space(json,end);
        if(at>=json.size()||json[at]!=':')throw std::runtime_error("Expected JSON readback member");
        at=json_space(json,at+1);const auto value_end=json_end(json,at);
        if(name=="\""+key+"\"")return json.substr(at,value_end-at);
        at=json_space(json,value_end);if(at<json.size()&&json[at]==',')at=json_space(json,at+1);
    }
    throw std::runtime_error("Missing JSON readback member: "+key);
}
std::vector<std::string_view> json_array(std::string_view json) {
    if(json.empty()||json.front()!='[')throw std::runtime_error("Expected JSON readback array");
    std::vector<std::string_view> result;auto at=json_space(json,1);
    while(at<json.size()&&json[at]!=']') {
        const auto end=json_end(json,at);result.push_back(json.substr(at,end-at));at=json_space(json,end);
        if(at<json.size()&&json[at]==',')at=json_space(json,at+1);
    }
    return result;
}
double json_number(std::string_view json) {
    std::size_t consumed=0;const auto value=std::stod(std::string(json),&consumed);
    if(consumed!=json.size()||!std::isfinite(value))throw std::runtime_error("Invalid JSON readback number");return value;
}
TextLayout require_source_sans(Session& session,const TextSource& expected,const std::vector<LegacyCapture::Run>& oracle) {
    const auto native=encode(session.document());const auto revision=session.revision();
    const auto& object=session.document().objects.at("source-sans-text");
    check(session.document().id=="source-sans-doc"&&object.id=="source-sans-text"&&object.kind==Kind::text&&object.text==expected,
        "Source Sans feature commands preserve Document, Object and editable TextSource identity and exact intent");
    const auto actual=evaluate_text_projection(session.document(),object.id,evaluate(session.document()));
    check(actual.font_request.family==expected.family&&actual.font_request.locale==expected.locale&&actual.font_request.weight==expected.weight&&
        actual.font_request.italic==expected.italic&&actual.font_request.font_features==expected.font_features&&actual.font_request.additional_axis_values.empty()&&
        actual.font_request.feature_application==(expected.font_features.empty()?"default":"submitted"),"Source Sans request preserves exact whole-text feature intent");
    check(!warning(actual,"MISSING_FONT:")&&!warning(actual,"MISSING_GLYPH:")&&!warning(actual,"FONT_FEATURE_UNAVAILABLE:")&&
        !warning(actual,"FONT_FEATURE_APPLICATION_UNSUPPORTED:"),"Admitted Source Sans capability must succeed in product shaping");
    validate_receipt(actual);ranges(actual,static_cast<UINT32>(wide(expected.content).size()));
    const auto response=request(session,"{\"op\":\"text_layout\",\"object\":\"source-sans-text\"}");validate_json(response);
    check(json_member(response,"ok")=="true"&&json_member(response,"document_id")=="\"source-sans-doc\""&&
        json_number(json_member(response,"revision"))==revision,"Shared text_layout returns the same Session identity and revision");
    const auto result=json_member(response,"result"),intent=json_member(result,"font_request");
    check(json_member(result,"object")=="\"source-sans-text\""&&json_member(result,"font_embedded")=="false"&&
        json_member(intent,"family")=="\"Source Sans 3\""&&json_member(intent,"locale")=="\"en-us\""&&
        json_number(json_member(intent,"weight"))==expected.weight&&json_member(intent,"italic")=="false"&&
        json_member(intent,"additional_axis_values")=="{}"&&json_member(intent,"feature_application")=="\""+actual.font_request.feature_application+"\"",
        "Shared text_layout preserves authored family, locale, sole weight/italic owners and feature submission");
    const auto features=json_array(json_member(intent,"font_features"));
    check(features.size()==expected.font_features.size(),"Shared text_layout has exactly the authored feature records");
    for(std::size_t i=0;i<features.size();++i)check(json_member(features[i],"feature_tag")=="\""+expected.font_features[i].feature_tag+"\""&&
        json_number(json_member(features[i],"parameter"))==expected.font_features[i].parameter&&json_member(features[i],"scope")=="\"whole_text\"",
        "Shared text_layout reads exact tag, uint32 parameter and whole_text scope");
    const auto runs=json_array(json_member(result,"font_runs"));
    check(actual.font_runs.size()==oracle.size()&&runs.size()==oracle.size(),"Source Sans product and independent oracle agree on actual run count");
    for(std::size_t i=0;i<oracle.size();++i) {
        const auto& run=actual.font_runs[i];const auto& independent=oracle[i];
        check(run.family==independent.family&&run.face==independent.face&&run.face_index==independent.index&&run.simulations==independent.simulations&&
            run.resolved_weight==static_cast<std::uint32_t>(independent.weight)&&run.resolved_style==static_cast<std::uint32_t>(independent.style)&&
            run.utf16_start==independent.start&&run.utf16_length==independent.length&&run.fallback==false&&run.missing_glyph_count==0&&
            run.glyph_indices==independent.glyphs&&run.glyph_advances==independent.advances,"Source Sans actual face, ranges, glyphs and advances exactly match independent CFF layout");
        check(run.files.size()==independent.files.size(),"Source Sans actual run has independent file reference evidence");
        const auto files=json_array(json_member(runs[i],"file_references"));check(files.size()==independent.files.size(),"Shared text_layout returns every independent file reference");
        for(std::size_t f=0;f<files.size();++f)check(run.files[f].key_sha256==independent.files[f].key_sha256&&run.files[f].key_size==independent.files[f].key_size&&
            run.files[f].local_loader==independent.files[f].local_loader&&json_member(files[f],"key_sha256")=="\""+independent.files[f].key_sha256+"\""&&
            json_number(json_member(files[f],"key_size"))==independent.files[f].key_size&&
            json_member(files[f],"local_loader")==std::string_view(independent.files[f].local_loader?"true":"false")&&json_member(files[f],"portable_file_hash")=="false",
            "Shared text_layout file identity matches independent loader-key digest, never a font binary hash");
        check(json_member(runs[i],"resolved_family")=="\""+independent.family+"\""&&json_member(runs[i],"resolved_face")=="\""+independent.face+"\""&&
            json_number(json_member(runs[i],"face_index"))==independent.index&&json_number(json_member(runs[i],"simulations"))==independent.simulations&&
            json_number(json_member(runs[i],"resolved_weight"))==static_cast<std::uint32_t>(independent.weight)&&
            json_number(json_member(runs[i],"resolved_style"))==static_cast<std::uint32_t>(independent.style)&&
            json_number(json_member(runs[i],"utf16_start"))==independent.start&&json_number(json_member(runs[i],"utf16_length"))==independent.length&&
            json_member(runs[i],"fallback")=="false"&&json_number(json_member(runs[i],"missing_glyph_count"))==0,
            "Shared text_layout returns exact independent face identity and nonfallback ranges");
        const auto glyphs=json_array(json_member(runs[i],"glyph_indices")),advances=json_array(json_member(runs[i],"glyph_advances"));
        check(glyphs.size()==independent.glyphs.size()&&advances.size()==independent.advances.size(),"Shared glyph and advance counts match independent shaping");
        for(std::size_t g=0;g<glyphs.size();++g)check(json_number(glyphs[g])==independent.glyphs[g]&&json_number(advances[g])==independent.advances[g],
            "Shared text_layout glyphs and advances exactly match independently drawn DirectWrite runs");
    }
    check(native==encode(session.document())&&revision==session.revision(),"Shaping and shared readback never mutate native authored state or Session revision");
    return actual;
}
int source_sans_test() {
    bool admitted=false;
    return fixture_exit(admitted,[&] {
    const auto fixture=source_sans_admission();admitted=true;
    std::cout<<"OTF_FEATURE_SOURCE_SANS_3: independently admitted system CFF face; feature="<<fixture.tag<<" off=0 on=1\n";
    // After this boundary every error is a product/test failure, never a skip.
    Session session(empty_document("source-sans-doc","source-sans-comp","source-sans-art"));
    session.apply({CreateText{"source-sans-comp","","source-sans-text","Source Sans 3 fixture",fixture.source}},0);
    const auto baseline_document=session.document();const auto baseline_native=encode(baseline_document);
    const auto baseline=require_source_sans(session,fixture.source,fixture.baseline);
    auto expected=fixture.source;expected.font_features={{fixture.tag,0,"whole_text"}};
    session.apply({AddTextFontFeature{"source-sans-text",expected.font_features.front()}},session.revision());
    const auto off_native=encode(session.document());const auto off=require_source_sans(session,expected,fixture.off);
    expected.font_features.front().parameter=1;
    session.apply({UpdateTextFontFeature{"source-sans-text",fixture.tag,1}},session.revision());
    const auto on_native=encode(session.document());const auto on=require_source_sans(session,expected,fixture.on);
    check(!same_glyphs(off,on),"Independently effective Source Sans feature must change product glyphs or advances");
    session.undo(session.revision());check(encode(session.document())==off_native,"Source Sans feature Undo restores byte-exact off intent");
    expected.font_features.front().parameter=0;require_source_sans(session,expected,fixture.off);
    session.redo(session.revision());check(encode(session.document())==on_native,"Source Sans feature Redo restores byte-exact on intent");
    expected.font_features.front().parameter=1;require_source_sans(session,expected,fixture.on);
    Session reopened(decode(on_native));
    check(reopened.document()==session.document()&&encode(reopened.document())==on_native,"New Session native roundtrip preserves exact Source Sans authored intent and identity");
    require_source_sans(reopened,expected,fixture.on);
    session.apply({RemoveTextFontFeature{"source-sans-text",fixture.tag}},session.revision());expected.font_features.clear();
    check(session.document()==baseline_document&&encode(session.document())==baseline_native,"Source Sans removal restores complete baseline Document and native bytes");
    check(same_glyphs(baseline,require_source_sans(session,expected,fixture.baseline)),"Source Sans removal restores exact independent baseline glyphs and advances");
    session.undo(session.revision());check(encode(session.document())==on_native,"Source Sans removal Undo restores exact feature intent");
    expected.font_features={{fixture.tag,1,"whole_text"}};require_source_sans(session,expected,fixture.on);
    session.redo(session.revision());check(encode(session.document())==baseline_native,"Source Sans removal Redo restores exact baseline");
    require_source_sans(session,fixture.source,fixture.baseline);
    std::cout<<"source_sans_3_fixture: "<<checks<<" checks passed\n";
    },std::cout);
}
int source_sans_diagnostics() {
    const auto probe=[](bool admitted,auto action,int expected,std::string_view detail) {
        std::ostringstream output;const auto result=fixture_exit(admitted,action,output);
        check(result==expected,"Diagnostic classification has the exact expected exit code");
        check(output.str().find(detail)!=std::string::npos,"Diagnostic classification retains the specific reason/stage/HRESULT");
    };
    probe(false,[]{throw FixtureUnavailable("probe.family","FAMILY_NOT_INSTALLED");},77,"reason=FAMILY_NOT_INSTALLED");
    probe(false,[]{throw FixtureUnavailable("probe.face","SYSTEM_REGULAR_FACE_NOT_OTF_CFF");},77,"reason=SYSTEM_REGULAR_FACE_NOT_OTF_CFF");
    probe(false,[]{throw FixtureUnavailable("probe.effect","NO_EFFECTIVE_FEATURE_LIGA_KERN_SS01");},77,"reason=NO_EFFECTIVE_FEATURE_LIGA_KERN_SS01");
    probe(false,[]{interface_hr(E_NOINTERFACE,"probe.interface");},77,"stage=probe.interface reason=REQUIRED_INTERFACE_UNAVAILABLE HRESULT=0x80004002");
    probe(false,[]{interface_hr(E_FAIL,"probe.unknown");},1,"stage=probe.unknown reason=HRESULT_CALL_FAILED HRESULT=0x80004005");
    probe(false,[]{oracle_hr(E_INVALIDARG,"probe.input");},1,"stage=probe.input reason=HRESULT_CALL_FAILED HRESULT=0x80070057");
    probe(false,[]{oracle_hr(E_OUTOFMEMORY,"probe.allocation");},1,"stage=probe.allocation reason=HRESULT_CALL_FAILED HRESULT=0x8007000E");
    probe(false,[]{throw std::runtime_error("probe.unexpected_runtime");},1,"probe.unexpected_runtime");
    probe(false,[]{throw 42;},1,"unexpected nonstandard exception");
    probe(false,[]{throw std::bad_alloc();},1,"FAIL: OTF_FEATURE_SOURCE_SANS_3");
    probe(false,[]{auto source=default_text("invalid","fi");source.family="Source Sans 3";source.locale="en-us";source.font_features={{"bad",1,"whole_text"}};
        (void)source_sans_oracle(source);},1,"stage=oracle.input reason=INVALID_FIXED_FIXTURE_INPUT");
    CallbackFailure callback;
    check(callback.invoke([]() -> HRESULT {oracle_hr(E_INVALIDARG,"probe.callback.GetFaceNames");return S_OK;})==E_FAIL,"COM callback returns E_FAIL while retaining the exception");
    probe(false,[&]{callback.rethrow();},1,"stage=probe.callback.GetFaceNames reason=HRESULT_CALL_FAILED HRESULT=0x80070057");
    CallbackFailure capability;
    check(capability.invoke([]() -> HRESULT {interface_hr(E_NOINTERFACE,"probe.callback.Face3");return S_OK;})==E_FAIL,"COM capability callback retains classified absence");
    probe(false,[&]{capability.rethrow();},77,"stage=probe.callback.Face3 reason=REQUIRED_INTERFACE_UNAVAILABLE HRESULT=0x80004002");
    ComPtr<LegacyCapture> renderer;renderer.Attach(new LegacyCapture({},true));
    check(renderer->DrawGlyphRun(nullptr,0,0,DWRITE_MEASURING_MODE_NATURAL,nullptr,nullptr,nullptr)==E_FAIL,"Real renderer callback retains invalid run input failure");
    probe(false,[&]{renderer->callback.rethrow();},1,"stage=DrawGlyphRun reason=INVALID_RUN_INPUT");
    probe(true,[]{throw std::runtime_error("probe.product_mismatch");},1,"probe.product_mismatch");
    probe(true,[]{throw FixtureUnavailable("probe.product","FAMILY_NOT_INSTALLED");},1,"FAIL: OTF_FEATURE_SOURCE_SANS_3");
    std::cout<<"source_sans_3_diagnostics: "<<checks<<" checks passed\n";return 0;
}
#endif
}
int main(int argc,char** argv) {
    const bool source_sans=argc==2&&std::string_view(argv[1])=="--source-sans-3";
    const bool diagnostics=argc==2&&std::string_view(argv[1])=="--source-sans-3-diagnostics";
    if(argc!=1&&!source_sans&&!diagnostics){std::cerr<<"Usage: font_shaping_tests [--source-sans-3|--source-sans-3-diagnostics]\n";return 1;}
#ifndef _WIN32
    if(source_sans||diagnostics){std::cout<<"ENV_MISSING_FIXTURE: OTF_FEATURE_SOURCE_SANS_3 reason=WINDOWS_DIRECTWRITE_REQUIRED\n";return 77;}
    auto source=default_text("portable","AV");source.font_features={{"kern",0,"whole_text"}};source.additional_axis_values={{"wdth",87.5}};
    try{(void)evaluate_text(source,values(source));}catch(const Error& error) {
        if(error.code=="TEXT_PLATFORM_UNSUPPORTED") {std::cout<<"SKIP: Windows semantic shaping requires real DirectWrite; Linux remains explicitly unsupported\n";return 77;}
    }
    std::cerr<<"Non-Windows projection did not remain unsupported\n";return 1;
#else
    try {
        if(diagnostics)return source_sans_diagnostics();
        if(source_sans)return source_sans_test();
        const auto fonts=text_fonts();
        if(fixture(fonts,"Arial")) {
            auto source=default_text("font-source","AVATAR To Wa");source.family="Arial";source.locale="en-us";
            for(const auto [weight,italic]:{std::pair<unsigned,bool>{400,false},{700,false},{400,true},{700,true}}) {
                source.weight=weight;source.italic=italic;
                const auto expected=legacy(source);const auto actual=evaluate_text(source,values(source));
                check(actual.font_runs.size()==expected.size(),"Empty authoring intent preserves legacy static run segmentation");
                for(std::size_t i=0;i<expected.size();++i) {
                    check(actual.font_runs[i].glyph_indices==expected[i].glyphs&&actual.font_runs[i].glyph_advances==expected[i].advances,
                        "Empty authoring intent exactly preserves legacy normal/bold/italic glyphs and advances");
                    check(actual.font_runs[i].face_index==expected[i].index&&actual.font_runs[i].simulations==expected[i].simulations,
                        "Legacy static face index/simulations remain unchanged");
                    check(actual.font_runs[i].resolved_weight==static_cast<std::uint32_t>(expected[i].weight)&&
                        actual.font_runs[i].resolved_style==static_cast<std::uint32_t>(expected[i].style),
                        "Actual static face weight/style exactly match independent legacy selection");
                    check(expected[i].weight==weight&&(expected[i].style!=DWRITE_FONT_STYLE_NORMAL)==italic,
                        "Static legacy oracle actually selected requested weight and italic style");
                }
                validate_receipt(actual);ranges(actual,static_cast<UINT32>(source.content.size()));
            }
            source.weight=400;source.italic=false;const auto baseline=evaluate_text(source,values(source));
            source.font_features={{"kern",0,"whole_text"}};const auto off=evaluate_text(source,values(source));
            source.font_features.front().parameter=1;const auto on=evaluate_text(source,values(source));
            check(!same_glyphs(off,on),"Supported kern feature changes actual glyph/advance oracle");
            check(on.font_request.feature_application=="submitted"&&!on.font_runs.front().script_features.empty(),"Whole-text feature and script/locale availability are exposed");
            check(on.font_runs.front().script_features.front().features.front().availability=="available_including_partial",
                "API availability is explicitly weaker than visual feature effect");
            source.font_features.clear();check(same_glyphs(baseline,evaluate_text(source,values(source))),"Removing feature returns exact baseline oracle");
            source.font_features={{"ZZZZ",1,"whole_text"}};source.additional_axis_values={{"wdth",87.5},{"ZZZZ",12}};
            const auto intent=source;const auto unsupported=evaluate_text(source,values(source));
            check(source==intent&&unsupported.font_request.additional_axis_values==intent.additional_axis_values,"Unsupported static/unknown axes never rewrite intent");
            check(warning(unsupported,"FONT_FEATURE_UNAVAILABLE:")&&warning(unsupported,"FONT_AXIS_INCOMPATIBLE:"),"Unsupported features and axes have explicit incompatibility warnings");
            check(axis_check(unsupported.font_runs.front(),"wdth").status=="out_of_range"&&warning(unsupported,"FONT_AXIS_NONINTERPOLATING:"),
                "Static font width mismatch is explicit and is not reported as adjustable variable support");
            source.font_features.clear();source.additional_axis_values={{"wdth",100}};
            const auto fixed=evaluate_text(source,values(source));
            check(axis_check(fixed.font_runs.front(),"wdth").status=="resolved_noninterpolating"&&
                warning(fixed,"FONT_AXIS_NONINTERPOLATING:")&&!warning(fixed,"FONT_AXIS_INCOMPATIBLE:"),
                "Matching static axis reports successful noninterpolating resolution without false incompatibility");
            source.font_features.clear();source.additional_axis_values.clear();source.content="";
            const auto empty=evaluate_text(source,values(source));check(empty.font_runs.empty()&&empty.glyph_count==0,"Empty text has no fabricated runs");
            source.content="A\xf4\x8f\xbf\xbfZ";const auto supplementary=evaluate_text(source,values(source));ranges(supplementary,4);
            check(warning(supplementary,"MISSING_GLYPH:"),"Missing supplementary scalar exposes missing glyph warning");
            check(std::any_of(supplementary.font_runs.begin(),supplementary.font_runs.end(),[](const auto& run){return run.missing_glyph_count>0;}),
                "Missing glyphs are counted on their actual runs");
            source.content="Latin 日本語";const auto mixed=evaluate_text(source,values(source));ranges(mixed,9);validate_receipt(mixed);
            check(mixed.used_fonts.size()>=2&&warning(mixed,"FONT_FALLBACK:"),"Mixed Latin/Japanese records actual fallback families");
            check(mixed.font_request.family=="Arial","Fallback keeps requested family authority");
            source.family="Nect Missing Font Fixture 93D547";const auto missing_family=evaluate_text(source,values(source));
            check(warning(missing_family,"MISSING_FONT:")&&!missing_family.font_runs.empty(),"Missing requested family shapes with explicit substitution evidence");

            source=default_text("path-source","AVATAR To Wa");source.family="Arial";source.locale="en-us";source.font_features={{"kern",0,"whole_text"}};
            source.path_attachment=TextPathAttachment{"path","contour","distance",0,0,false};
            Point start,end;start.id="start";end.id="end";end.x.literal=2000;
            Session session(empty_document("font-doc","font-comp","font-art"));
            session.apply({CreatePath{"font-comp","","path","Path",{{"contour",false,{start,end}}}},CreateText{"font-comp","","text","Text",source}},0);
            const auto native=encode(session.document());const auto evaluated=evaluated_text_source(session.document(),"text");
            const auto shaped=evaluate_text(evaluated,values(evaluated));
            const auto projected=evaluate_text_projection(session.document(),"text",evaluate(session.document()));
            check(projected.font_request==shaped.font_request&&projected.font_runs==shaped.font_runs,"Text-on-Path propagates exact pregeometry font receipts");
            const auto response=request(session,"{\"op\":\"text_layout\",\"object\":\"text\"}");
            check(response.find("\"font_request\"")!=std::string::npos&&response.find("\"font_runs\"")!=std::string::npos&&
                response.find("loader_scoped_reference_key_digest")!=std::string::npos&&response.find("not_established_by_availability_query")!=std::string::npos,
                "Shared text_layout JSON exposes truthful evaluated request and actual run receipt");
            check(native==encode(session.document())&&decode(native)==session.document(),"Derived receipts never become persisted authored authority");
            session.apply({UpdateTextFontFeature{"text","kern",1}},session.revision());
            check(!same_glyphs(projected,evaluate_text_projection(session.document(),"text",evaluate(session.document()))),"Typed feature command changes attached Text shaping");
            session.undo(session.revision());check(encode(session.document())==native,"Feature Undo restores exact original attached Text intent");
            session.redo(session.revision());check(session.document().objects.at("text").text->font_features.front().parameter==1,"Feature Redo restores intent");
        }
        if(fixture(fonts,"Bahnschrift")) {
            auto source=default_text("variable-source","Variable AV");source.family="Bahnschrift";source.locale="en-us";
            const auto capability=variable_capability("Bahnschrift");
            if(!capability) {
                ++missing;std::cout<<"ENV_MISSING_FIXTURE: Independent DirectWrite preflight lacks Bahnschrift variable wght/wdth capability or runtime API\n";
            } else {
                const auto weight_axis=TextFontAxis{"wght",{},capability->weight.minValue,capability->weight.maxValue,{}};
                const auto width_axis=TextFontAxis{"wdth",{},capability->width.minValue,capability->width.maxValue,{}};
                source.weight=static_cast<unsigned>(std::floor(*weight_axis.maximum));
                const auto width=static_cast<double>(static_cast<float>((*width_axis.minimum+*width_axis.maximum)/2));
                source.additional_axis_values={{"wdth",width}};
                const auto requested=source;const auto actual=evaluate_text(source,values(source));
                check(actual.font_request.axis_application=="submitted","Independent available API requires product to submit axes");
                require_variable(source,actual,*capability);
                check(source==requested&&actual.font_request.additional_axis_values==source.additional_axis_values,"Actual axes remain separate from exact authored intent");
                source.additional_axis_values["wdth"]=*width_axis.minimum;const auto narrow=evaluate_text(source,values(source));require_variable(source,narrow,*capability);
                source.additional_axis_values["wdth"]=*width_axis.maximum;const auto wide=evaluate_text(source,values(source));require_variable(source,wide,*capability);
                check(!same_glyphs(narrow,wide),"Variable width range changes actual glyph/advance oracle");
                source.weight=static_cast<unsigned>(std::ceil(*weight_axis.minimum));
                require_variable(source,evaluate_text(source,values(source)),*capability);
                const auto outside_value=*width_axis.maximum+std::max(1.0,*width_axis.maximum-*width_axis.minimum);
                source.additional_axis_values["wdth"]=outside_value;const auto outside=evaluate_text(source,values(source));
                require_variable(source,outside,*capability);
                check(axis_check(outside.font_runs.front(),"wdth").status=="out_of_range"&&source.additional_axis_values.at("wdth")==outside_value,
                    "Out-of-range intent remains exact and backend resolution is an explicit incompatibility");
                const double precise=*width_axis.minimum+(*width_axis.maximum-*width_axis.minimum)*0.371234567890123;
                source.additional_axis_values["wdth"]=precise;
                const auto rounded=evaluate_text(source,values(source));require_variable(source,rounded,*capability);
                check(warning(rounded,"FONT_AXIS_FLOAT_ROUNDED:")&&rounded.font_request.additional_axis_values.at("wdth")==precise,
                    "Narrowing is explicit while exact authored double is preserved");
                source.additional_axis_values["wdth"]=std::numeric_limits<double>::max();const auto huge=evaluate_text(source,values(source));
                require_variable(source,huge,*capability);
                check(warning(huge,"FONT_AXIS_FLOAT_UNREPRESENTABLE:")&&axis_check(huge.font_runs.front(),"wdth").status=="not_submitted_float_range",
                    "Finite double outside float range stays authored with explicit unsubmitted status");
                check(axis_check(rounded.font_runs.front(),"wdth").resolved!=axis_check(huge.font_runs.front(),"wdth").resolved&&
                    !same_glyphs(rounded,huge),"Precise width takes effect independently of omitted-axis default selection");
                Session session(empty_document("axis-doc","axis-comp","axis-art"));
                session.apply({CreateText{"axis-comp","","axis-text","Variable text",source}},0);
                session.apply({SetTextAdditionalAxis{"axis-text","wdth",precise}},session.revision());
                const auto native=encode(session.document());
                const auto response=request(session,"{\"op\":\"text_layout\",\"object\":\"axis-text\"}");
                check(response.find("FONT_AXIS_FLOAT_ROUNDED:")!=std::string::npos&&native==encode(session.document()),
                    "Shared command/readback preserves precise Document axis after actual backend narrowing");
                check(decode(native).objects.at("axis-text").text->additional_axis_values.at("wdth")==precise,
                    "Native readback retains exact authored axis independently of actual float resolution");
            }
        }
        if(fixture(fonts,"Yu Gothic")) {
            auto source=default_text("cjk-source","日本語。、ABC");source.family="Yu Gothic";
            const auto horizontal=evaluate_text(source,values(source));source.direction="vertical";const auto vertical=evaluate_text(source,values(source));
            ranges(horizontal,8);ranges(vertical,8);validate_receipt(horizontal);validate_receipt(vertical);
            check(horizontal.glyph_count>0&&vertical.glyph_count>0,"CJK collection font still shapes horizontally and vertically");
        }
        if(fixture(fonts,"Times New Roman")) {
            auto source=default_text("arabic-source","السَّلَامُ عَلَيْكُمْ");source.family="Times New Roman";source.locale="ar-sa";
            source.font_features={{"kern",1,"whole_text"}};const auto actual=evaluate_text(source,values(source));
            ranges(actual,static_cast<UINT32>(wide(source.content).size()));validate_receipt(actual);
            check(std::all_of(actual.font_runs.begin(),actual.font_runs.end(),[](const auto& run){return run.missing_glyph_count==0;}),"Arabic diacritics retain nonmissing shaped glyphs");
        }
        std::cout<<"font_shaping_tests: "<<checks<<" checks passed; "<<missing<<" missing fixture families\n";
        return missing?77:0;
    }catch(const std::exception& error){std::cerr<<"font_shaping_tests: "<<error.what()<<'\n';return 1;}
#endif
}
