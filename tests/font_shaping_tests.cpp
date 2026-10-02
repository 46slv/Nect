#include "nect/io.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite_3.h>
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
void hr(HRESULT result) {if(FAILED(result))throw std::runtime_error("Legacy DirectWrite oracle failed");}
std::wstring wide(const std::string& source) {
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.data(),static_cast<int>(source.size()),nullptr,0);
    std::wstring result(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.data(),static_cast<int>(source.size()),result.data(),count);return result;
}
// Independent legacy CreateTextFormat route, deliberately without TextFormat3.
// Compare the real drawn face/glyph/advance oracle, not a requested format getter.
class LegacyCapture final : public IDWriteTextRenderer {
public:
    struct Run {std::vector<UINT16> glyphs;std::vector<double> advances;UINT32 index=0,simulations=0;DWRITE_FONT_WEIGHT weight{};DWRITE_FONT_STYLE style{};};
    std::vector<Run> runs;
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
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT,FLOAT,DWRITE_MEASURING_MODE,const DWRITE_GLYPH_RUN* value,const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown*) override {
        try {
            if(!value||!value->fontFace||!value->glyphIndices||!value->glyphAdvances)return E_FAIL;
            ComPtr<IDWriteFontFace3> face;hr(value->fontFace->QueryInterface(IID_PPV_ARGS(&face)));
            runs.push_back({{value->glyphIndices,value->glyphIndices+value->glyphCount},
                {value->glyphAdvances,value->glyphAdvances+value->glyphCount},value->fontFace->GetIndex(),
                value->fontFace->GetSimulations(),face->GetWeight(),face->GetStyle()});return S_OK;
        } catch(...){return E_FAIL;}
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,const DWRITE_UNDERLINE*,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,const DWRITE_STRIKETHROUGH*,IUnknown*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) override {return E_NOTIMPL;}
private:std::atomic<ULONG> refs_{1};
};
std::vector<LegacyCapture::Run> legacy(const TextSource& source) {
    ComPtr<IDWriteFactory> factory;hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
    ComPtr<IDWriteFontCollection> fonts;hr(factory->GetSystemFontCollection(&fonts));
    ComPtr<IDWriteTextFormat> format;const auto family=wide(source.family),locale=wide(source.locale),text=wide(source.content);
    hr(factory->CreateTextFormat(family.c_str(),fonts.Get(),static_cast<DWRITE_FONT_WEIGHT>(source.weight),
        source.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,48,locale.c_str(),&format));
    hr(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    ComPtr<IDWriteTextLayout> layout;hr(factory->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),1000000,1000000,&layout));
    ComPtr<LegacyCapture> renderer;renderer.Attach(new LegacyCapture);
    hr(layout->Draw(nullptr,renderer.Get(),0,0));return renderer->runs;
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
void require_axis(const TextLayout& layout,const std::string& tag,double value) {
    check(!layout.font_runs.empty(),"Variable fixture emits actual drawn runs");
    for(const auto& run:layout.font_runs) {
        const auto& axis=axis_check(run,tag);
        check(axis.resolved&&*axis.resolved==value,"Actual Face5 readback agrees with requested variable axis");
        check(axis.status=="resolved"||axis.status=="resolved_float_rounded","Available variable axis receives an affirmative resolution assessment");
        const auto actual=std::find_if(run.axes.begin(),run.axes.end(),[&](const auto& entry){return entry.tag==tag;});
        check(actual!=run.axes.end()&&actual->variable==true,"Actual resource VARIABLE metadata establishes interpolation support");
        check(run.has_variations==true&&run.axis_values_known&&run.axis_ranges_known,"Variable receipt has actual face/resource evidence");
    }
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
#endif
}
int main() {
#ifndef _WIN32
    auto source=default_text("portable","AV");source.font_features={{"kern",0,"whole_text"}};source.additional_axis_values={{"wdth",87.5}};
    try{(void)evaluate_text(source,values(source));}catch(const Error& error) {
        if(error.code=="TEXT_PLATFORM_UNSUPPORTED") {std::cout<<"SKIP: Windows semantic shaping requires real DirectWrite; Linux remains explicitly unsupported\n";return 77;}
    }
    std::cerr<<"Non-Windows projection did not remain unsupported\n";return 1;
#else
    try {
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
                require_axis(actual,"wght",source.weight);require_axis(actual,"wdth",width);
                check(source==requested&&actual.font_request.additional_axis_values==source.additional_axis_values,"Actual axes remain separate from exact authored intent");
                source.additional_axis_values["wdth"]=*width_axis.minimum;const auto narrow=evaluate_text(source,values(source));
                source.additional_axis_values["wdth"]=*width_axis.maximum;const auto wide=evaluate_text(source,values(source));
                check(!same_glyphs(narrow,wide),"Variable width range changes actual glyph/advance oracle");
                source.weight=static_cast<unsigned>(std::ceil(*weight_axis.minimum));
                require_axis(evaluate_text(source,values(source)),"wght",source.weight);
                const auto outside_value=*width_axis.maximum+std::max(1.0,*width_axis.maximum-*width_axis.minimum);
                source.additional_axis_values["wdth"]=outside_value;const auto outside=evaluate_text(source,values(source));
                check(axis_check(outside.font_runs.front(),"wdth").status=="out_of_range"&&source.additional_axis_values.at("wdth")==outside_value,
                    "Out-of-range intent remains exact and backend resolution is an explicit incompatibility");
                const double precise=*width_axis.minimum+(*width_axis.maximum-*width_axis.minimum)*0.371234567890123;
                source.additional_axis_values["wdth"]=precise;
                const auto rounded=evaluate_text(source,values(source));require_axis(rounded,"wdth",static_cast<float>(precise));
                check(warning(rounded,"FONT_AXIS_FLOAT_ROUNDED:")&&rounded.font_request.additional_axis_values.at("wdth")==precise,
                    "Narrowing is explicit while exact authored double is preserved");
                source.additional_axis_values["wdth"]=std::numeric_limits<double>::max();const auto huge=evaluate_text(source,values(source));
                check(warning(huge,"FONT_AXIS_FLOAT_UNREPRESENTABLE:")&&axis_check(huge.font_runs.front(),"wdth").status=="not_submitted_float_range",
                    "Finite double outside float range stays authored with explicit unsubmitted status");
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
