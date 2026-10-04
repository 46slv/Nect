#include "nect/core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
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

namespace {
int checks=0;
void check(bool value,const std::string& message) {if(!value)throw std::runtime_error(message);++checks;}
void near(double a,double b,const std::string& message,double tolerance=0.003) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tolerance,
        message+": "+std::to_string(a)+" != "+std::to_string(b));
}
#ifdef _WIN32
using Microsoft::WRL::ComPtr;
struct MissingFixture : std::runtime_error {using std::runtime_error::runtime_error;};
void hr(HRESULT value,const char* stage) {
    if(FAILED(value))throw std::runtime_error(std::string(stage)+" HRESULT="+std::to_string(static_cast<unsigned long>(value)));
}
struct P {double x=0,y=0;};
struct Edge {P a,c1,c2,b;};
struct Box {
    double left=std::numeric_limits<double>::infinity(),top=left,right=-left,bottom=-left;
    void add(P p) {check(std::isfinite(p.x)&&std::isfinite(p.y),"Finite oracle geometry");
        left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
    double width() const{return right-left;}double height() const{return bottom-top;}
};
bool same(P a,P b,double tolerance=0.003) {return std::abs(a.x-b.x)<=tolerance&&std::abs(a.y-b.y)<=tolerance;}
P cubic(const Edge& e,double t) {
    const double s=1-t;return {s*s*s*e.a.x+3*s*s*t*e.c1.x+3*s*t*t*e.c2.x+t*t*t*e.b.x,
        s*s*s*e.a.y+3*s*s*t*e.c1.y+3*s*t*t*e.c2.y+t*t*t*e.b.y};
}
Box bounds(const std::vector<Edge>& edges) {
    Box result;for(const auto& e:edges)for(int i=0;i<=32;++i)result.add(cubic(e,i/32.0));return result;
}
// Deliberately stores DirectWrite's segments, rather than Nect's point/handle
// representation or its outline helpers. All COM callbacks contain exceptions.
class Segments final : public IDWriteGeometrySink {
public:
    explicit Segments(DWRITE_MATRIX matrix,FLOAT x,FLOAT y):matrix_(matrix),x_(x),y_(y){}
    std::vector<Edge> edges;std::exception_ptr failure;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=__uuidof(IUnknown)&&iid!=__uuidof(IDWriteGeometrySink))return E_NOINTERFACE;
        *out=static_cast<IDWriteGeometrySink*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override{const auto n=--refs_;if(!n)delete this;return n;}
    void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE mode) override {invoke([&]{check(mode==D2D1_FILL_MODE_WINDING,"Oracle requires nonzero winding fill");});}
    void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT) override {}
    void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F p,D2D1_FIGURE_BEGIN) override {
        invoke([&]{check(!open_,"Oracle figure is not nested");start_=last_=project(p);open_=true;});
    }
    void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F* p,UINT32 n) override {
        invoke([&]{check(open_&&(!n||p),"Oracle line has an open figure");for(UINT32 i=0;i<n;++i){const auto end=project(p[i]);append({last_,last_,end,end});last_=end;}});
    }
    void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT* p,UINT32 n) override {
        invoke([&]{check(open_&&(!n||p),"Oracle cubic has an open figure");for(UINT32 i=0;i<n;++i){const auto end=project(p[i].point3);append({last_,project(p[i].point1),project(p[i].point2),end});last_=end;}});
    }
    void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END end) override {
        invoke([&]{check(open_&&end==D2D1_FIGURE_END_CLOSED,"Fixture outlines are closed");if(!same(last_,start_,0))append({last_,last_,start_,start_});open_=false;});
    }
    HRESULT STDMETHODCALLTYPE Close() override {invoke([&]{check(!open_,"Oracle outline completed");});return failure?E_FAIL:S_OK;}
private:
    std::atomic<ULONG> refs_{1};DWRITE_MATRIX matrix_;FLOAT x_,y_;P start_,last_;bool open_=false;
    template<class F> void invoke(F action) noexcept {if(!failure)try{action();}catch(...){failure=std::current_exception();}}
    void append(Edge e) {if(!(same(e.a,e.b,0)&&same(e.a,e.c1,0)&&same(e.a,e.c2,0)))edges.push_back(e);}
    P project(D2D1_POINT_2F p) const {
        const double x=static_cast<double>(p.x)+x_,y=static_cast<double>(p.y)+y_;
        return {matrix_.m11*x+matrix_.m21*y+matrix_.dx,matrix_.m12*x+matrix_.m22*y+matrix_.dy};
    }
};
std::string name(IDWriteLocalizedStrings* names) {
    check(names&&names->GetCount()>0,"Actual face has localized names");UINT32 index=0,length=0;BOOL exists=FALSE;
    hr(names->FindLocaleName(L"en-us",&index,&exists),"FindLocaleName");if(!exists)index=0;
    hr(names->GetStringLength(index,&length),"GetStringLength");std::wstring result(length+1,L'\0');hr(names->GetString(index,result.data(),length+1),"GetString");
    const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,result.data(),static_cast<int>(length),nullptr,0,nullptr,nullptr);
    check(n>0,"Face name is UTF-8 convertible");std::string utf8(n,'\0');
    check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,result.data(),static_cast<int>(length),utf8.data(),n,nullptr,nullptr)==n,"Complete face name conversion");return utf8;
}
std::vector<std::string> file_keys(IDWriteFontFace* face) {
    UINT32 count=0;hr(face->GetFiles(&count,nullptr),"GetFiles count");check(count>0&&count<16,"Bounded actual font files");
    std::vector<IDWriteFontFile*> raw(count,nullptr);std::vector<ComPtr<IDWriteFontFile>> files(count);
    const auto result=face->GetFiles(&count,raw.data());for(std::size_t i=0;i<raw.size();++i)files[i].Attach(raw[i]);hr(result,"GetFiles");
    std::vector<std::string> keys;for(const auto& file:files) {
        const void* key=nullptr;UINT32 size=0;hr(file->GetReferenceKey(&key,&size),"GetReferenceKey");check(key&&size>0&&size<65536,"Bounded font identity key");
        unsigned char digest[32]{};check(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(static_cast<const unsigned char*>(key)),size,digest,32)>=0,"Hash font identity key");
        std::string hex;for(const auto b:digest){hex+="0123456789abcdef"[b>>4];hex+="0123456789abcdef"[b&15];}keys.push_back(hex);
    }return keys; // No font bytes, paths, or unhashed loader keys are printed.
}
struct Run {
    UINT32 start=0,length=0,index=0,simulations=0,bidi=0;FLOAT em=0,x=0,y=0;
    DWRITE_GLYPH_ORIENTATION_ANGLE angle{};BOOL sideways=FALSE;DWRITE_FONT_WEIGHT weight{};DWRITE_FONT_STYLE style{};
    std::string family,face;std::vector<std::string> keys;ComPtr<IDWriteFontFace> font;
    std::vector<UINT16> indices,clusters;std::vector<FLOAT> advances;std::vector<DWRITE_GLYPH_OFFSET> offsets;std::vector<Edge> edges;
};
// Renderer1's orientation overload is essential: the legacy overload loses the
// rotation. SDK contract: https://learn.microsoft.com/en-us/windows/win32/api/dwrite_2/nn-dwrite_2-idwritetextrenderer1
class Capture final : public IDWriteTextRenderer1 {
public:
    explicit Capture(IDWriteTextAnalyzer2* analyzer):analyzer_(analyzer){}
    std::vector<Run> runs;std::exception_ptr failure;unsigned oriented_calls=0,legacy_calls=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=__uuidof(IUnknown)&&iid!=__uuidof(IDWritePixelSnapping)&&iid!=__uuidof(IDWriteTextRenderer)&&iid!=__uuidof(IDWriteTextRenderer1))return E_NOINTERFACE;
        *out=static_cast<IDWriteTextRenderer1*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override{const auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,BOOL* out) override {if(!out)return E_POINTER;*out=TRUE;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,DWRITE_MATRIX* out) override {if(!out)return E_POINTER;*out={1,0,0,1,0,0};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*,FLOAT* out) override {if(!out)return E_POINTER;*out=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void* c,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE m,const DWRITE_GLYPH_RUN* r,const DWRITE_GLYPH_RUN_DESCRIPTION* d,IUnknown* e) override {
        ++legacy_calls;return DrawGlyphRun(c,x,y,DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES,m,r,d,e);
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT x,FLOAT y,DWRITE_GLYPH_ORIENTATION_ANGLE angle,DWRITE_MEASURING_MODE,
        const DWRITE_GLYPH_RUN* r,const DWRITE_GLYPH_RUN_DESCRIPTION* d,IUnknown*) override {
        if(failure)return E_FAIL;
        try {
            ++oriented_calls;check(r&&d&&r->fontFace&&r->glyphIndices&&r->glyphAdvances&&d->clusterMap,"Complete actual oriented run");
            if(!r->glyphCount)return S_OK;
            Run run;run.start=d->textPosition;run.length=d->stringLength;run.index=r->fontFace->GetIndex();run.simulations=r->fontFace->GetSimulations();
            run.bidi=r->bidiLevel;run.em=r->fontEmSize;run.x=x;run.y=y;run.angle=angle;run.sideways=r->isSideways;run.font=r->fontFace;
            run.indices.assign(r->glyphIndices,r->glyphIndices+r->glyphCount);run.advances.assign(r->glyphAdvances,r->glyphAdvances+r->glyphCount);
            run.clusters.assign(d->clusterMap,d->clusterMap+d->stringLength);run.offsets.resize(r->glyphCount);
            if(r->glyphOffsets)std::copy(r->glyphOffsets,r->glyphOffsets+r->glyphCount,run.offsets.begin());
            ComPtr<IDWriteFontFace3> face;hr(run.font.As(&face),"Actual IDWriteFontFace3");
            ComPtr<IDWriteLocalizedStrings> families,names;hr(face->GetFamilyNames(&families),"Actual family");hr(face->GetFaceNames(&names),"Actual face");
            run.family=name(families.Get());run.face=name(names.Get());run.weight=face->GetWeight();run.style=face->GetStyle();run.keys=file_keys(run.font.Get());
            DWRITE_MATRIX transform{};hr(analyzer_->GetGlyphOrientationTransform(angle,r->isSideways,x,y,&transform),"Independent orientation matrix");
            ComPtr<Segments> sink;sink.Attach(new Segments(transform,x,y));
            const auto result=r->fontFace->GetGlyphRunOutline(r->fontEmSize,r->glyphIndices,r->glyphAdvances,r->glyphOffsets,r->glyphCount,r->isSideways,(r->bidiLevel&1)!=0,sink.Get());
            sink->Close();if(sink->failure)std::rethrow_exception(sink->failure);hr(result,"Independent run outline");run.edges=std::move(sink->edges);
            runs.push_back(std::move(run));return S_OK;
        } catch(...) {failure=std::current_exception();return E_FAIL;}
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,const DWRITE_UNDERLINE*,IUnknown*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,const DWRITE_UNDERLINE*,IUnknown*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,const DWRITE_STRIKETHROUGH*,IUnknown*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,const DWRITE_STRIKETHROUGH*,IUnknown*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,DWRITE_GLYPH_ORIENTATION_ANGLE,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) override{return E_NOTIMPL;}
private:std::atomic<ULONG> refs_{1};ComPtr<IDWriteTextAnalyzer2> analyzer_;
};
struct Oracle {std::vector<Run> runs;std::vector<Edge> edges;std::vector<DWRITE_LINE_METRICS> lines;};
constexpr FLOAT em=48,width=600,height=900,origin_x=17.25f,origin_y=-11.5f;
const std::wstring text=L"「日本。、」ABC12\n（文章）123";
Oracle oracle(bool vertical) {
    ComPtr<IDWriteFactory2> factory;hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory2),reinterpret_cast<IUnknown**>(factory.GetAddressOf())),"Oracle factory");
    ComPtr<IDWriteFontCollection> fonts;hr(factory->GetSystemFontCollection(&fonts),"Oracle font collection");UINT32 index=0;BOOL exists=FALSE;
    hr(fonts->FindFamilyName(L"Yu Gothic",&index,&exists),"Find Yu Gothic");if(!exists)throw MissingFixture("Yu Gothic is not installed");
    ComPtr<IDWriteTextFormat> format;hr(factory->CreateTextFormat(L"Yu Gothic",fonts.Get(),DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,em,L"ja-JP",&format),"Oracle format");
    if(vertical){hr(format->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM),"Oracle TTB");hr(format->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT),"Oracle RTL columns");}
    hr(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP),"Oracle wrap");hr(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING),"Oracle leading");
    hr(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR),"Oracle paragraph near");
    ComPtr<IDWriteTextLayout> base;hr(factory->CreateTextLayout(text.data(),static_cast<UINT32>(text.size()),format.Get(),width,height,&base),"Oracle fixed frame");
    ComPtr<IDWriteTextLayout2> layout;const auto layout_result=base.As(&layout);if(layout_result==E_NOINTERFACE)throw MissingFixture("IDWriteTextLayout2 unavailable");hr(layout_result,"Oracle layout2");
    hr(layout->SetVerticalGlyphOrientation(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT),"Oracle script orientation");
    hr(layout->SetLastLineWrapping(TRUE),"Oracle last line wrap");hr(layout->SetCharacterSpacing(0,0,0,{0,static_cast<UINT32>(text.size())}),"Oracle zero tracking");
    ComPtr<IDWriteTextAnalyzer> analyzer_base;hr(factory->CreateTextAnalyzer(&analyzer_base),"Oracle analyzer");ComPtr<IDWriteTextAnalyzer2> analyzer;
    const auto analyzer_result=analyzer_base.As(&analyzer);if(analyzer_result==E_NOINTERFACE)throw MissingFixture("IDWriteTextAnalyzer2 unavailable");hr(analyzer_result,"Oracle analyzer2");
    ComPtr<Capture> capture;capture.Attach(new Capture(analyzer.Get()));const auto result=layout->Draw(nullptr,capture.Get(),origin_x,origin_y);
    if(capture->failure)std::rethrow_exception(capture->failure);hr(result,"Oracle draw");
    check(capture->oriented_calls>0&&capture->legacy_calls==0,"Renderer1 orientation callbacks were actually used");
    Oracle out;out.runs=std::move(capture->runs);for(const auto& run:out.runs)out.edges.insert(out.edges.end(),run.edges.begin(),run.edges.end());
    UINT32 count=0;const auto metrics_result=layout->GetLineMetrics(nullptr,0,&count);if(metrics_result!=E_NOT_SUFFICIENT_BUFFER)hr(metrics_result,"Oracle line count");
    out.lines.resize(count);hr(layout->GetLineMetrics(out.lines.data(),count,&count),"Oracle lines");out.lines.resize(count);return out;
}
std::vector<Edge> actual_edges(const nect::TextLayout& layout) {
    check(layout.contours!=nullptr,"Nect immutable contour payload");std::vector<Edge> out;
    auto p=[](nect::Vec2 v){return P{v.x,v.y};};
    for(const auto& contour:*layout.contours) {
        check(contour.closed&&!contour.points.empty(),"Nect glyph contours are closed");
        for(std::size_t i=0;i<contour.points.size();++i){const auto& a=contour.points[i];const auto& b=contour.points[(i+1)%contour.points.size()];
            Edge e{p(a.anchor),p(a.outgoing),p(b.incoming),p(b.anchor)};if(!(same(e.a,e.b,0)&&same(e.a,e.c1,0)&&same(e.a,e.c2,0)))out.push_back(e);}
    }return out;
}
void geometry(const std::vector<Edge>& expected,const std::vector<Edge>& actual) {
    check(!expected.empty()&&expected.size()==actual.size(),"Independent and Nect segment cardinality matches");
    std::vector<bool> used(actual.size());
    for(const auto& e:expected){bool found=false;for(std::size_t i=0;i<actual.size();++i)if(!used[i]){const auto& a=actual[i];
        if(same(e.a,a.a)&&same(e.c1,a.c1)&&same(e.c2,a.c2)&&same(e.b,a.b)){used[i]=true;found=true;break;}}
        check(found,"Every absolute DirectWrite cubic/line is present in Nect within 0.003 DIP");}
    const auto a=bounds(actual),e=bounds(expected);near(a.left,e.left,"Ink left");near(a.top,e.top,"Ink top");near(a.right,e.right,"Ink right");near(a.bottom,e.bottom,"Ink bottom");
}
// A small monochrome coverage oracle, not an antialiasing/font-rasterizer claim.
// Both independent DW cubics and Nect cubics are flattened at 32 subdivisions;
// nonzero winding is sampled on a common 0.5-DIP grid at pixel centers.
std::vector<unsigned char> coverage(const std::vector<Edge>& edges,const Box& box,int w,int h) {
    struct Line{P a,b;};std::vector<Line> lines;
    for(const auto& edge:edges){P previous=edge.a;for(int i=1;i<=32;++i){const auto next=cubic(edge,i/32.0);if(!same(previous,next,0))lines.push_back({previous,next});previous=next;}}
    std::vector<unsigned char> bitmap(static_cast<std::size_t>(w)*h);
    for(int y=0;y<h;++y){const double scan_y=box.top+(y+0.5)*0.5;std::vector<std::pair<double,int>> crossings;
        for(const auto& line:lines)if((line.a.y<=scan_y&&line.b.y>scan_y)||(line.b.y<=scan_y&&line.a.y>scan_y))
            crossings.emplace_back(line.a.x+(scan_y-line.a.y)*(line.b.x-line.a.x)/(line.b.y-line.a.y),line.b.y>line.a.y?1:-1);
        std::sort(crossings.begin(),crossings.end());std::size_t i=0;int winding=0;
        for(int x=0;x<w;++x){const double scan_x=box.left+(x+0.5)*0.5;while(i<crossings.size()&&crossings[i].first<=scan_x)winding+=crossings[i++].second;
            bitmap[static_cast<std::size_t>(y)*w+x]=winding!=0;}
    }return bitmap;
}
struct Character {const Run* run;UINT16 glyph;Box ink;};
Character character(const Oracle& oracle,UINT32 position) {
    for(const auto& run:oracle.runs)if(position>=run.start&&position<run.start+run.length){
        const auto index=run.clusters.at(position-run.start);check(index<run.indices.size()&&run.bidi==0,"Fixture character maps to an LTR shaped glyph");
        double advance=0;for(UINT16 i=0;i<index;++i)advance+=run.advances[i];
        ComPtr<IDWriteFactory> factory;hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf())),"Character factory");
        ComPtr<IDWriteTextAnalyzer> base;hr(factory->CreateTextAnalyzer(&base),"Character analyzer");ComPtr<IDWriteTextAnalyzer2> analyzer;hr(base.As(&analyzer),"Character analyzer2");
        DWRITE_MATRIX transform{};hr(analyzer->GetGlyphOrientationTransform(run.angle,run.sideways,run.x,run.y,&transform),"Character run pivot");
        ComPtr<Segments> sink;sink.Attach(new Segments(transform,static_cast<FLOAT>(run.x+advance),run.y));
        const auto result=run.font->GetGlyphRunOutline(run.em,&run.indices[index],&run.advances[index],&run.offsets[index],1,run.sideways,FALSE,sink.Get());
        sink->Close();if(sink->failure)std::rethrow_exception(sink->failure);hr(result,"Character independent outline");
        check(!sink->edges.empty(),"Fixture character has ink");return {&run,run.indices[index],bounds(sink->edges)};
    }throw std::runtime_error("Fixture UTF-16 character is absent from drawn runs");
}
void run_identity(const Oracle& oracle,const nect::TextLayout& layout) {
    check(oracle.runs.size()==layout.font_runs.size(),"Actual drawn run count matches oracle");
    for(std::size_t i=0;i<oracle.runs.size();++i){const auto& e=oracle.runs[i];const auto& a=layout.font_runs[i];
        check(e.family=="Yu Gothic"&&e.simulations==0,"Exact installed Yu Gothic without fallback/synthesis");
        check(a.utf16_start==e.start&&a.utf16_length==e.length&&a.family==e.family&&a.face==e.face&&a.face_index==e.index&&a.simulations==e.simulations&&a.bidi_level==e.bidi&&a.sideways==(e.sideways!=FALSE),"Run ranges and actual face identity agree");
        check(a.resolved_weight==static_cast<UINT32>(e.weight)&&a.resolved_style==static_cast<UINT32>(e.style)&&a.glyph_indices==e.indices,"Actual weight/style and shaped glyph indices agree");
        near(a.font_em_size,e.em,"Actual em size");check(a.files.size()==e.keys.size(),"Font file identity count agrees");
        for(std::size_t f=0;f<e.keys.size();++f)check(a.files[f].key_sha256==e.keys[f],"Actual font file identity hash agrees");
        check(a.glyph_advances&&a.glyph_advances->size()==e.advances.size(),"Actual advances are captured");
        for(std::size_t g=0;g<e.advances.size();++g){check(e.indices[g]!=0,"Fixture has no missing glyph");near(a.glyph_advances->at(g),e.advances[g],"Shaped advance");check(std::isfinite(e.offsets[g].advanceOffset)&&std::isfinite(e.offsets[g].ascenderOffset),"Finite actual glyph offsets");}
        check(std::isfinite(e.x)&&std::isfinite(e.y),"Finite callback baseline origins");
        std::cout<<"run utf16="<<e.start<<"+"<<e.length<<" angle="<<static_cast<unsigned>(e.angle)*90<<" sideways="<<(e.sideways!=FALSE)<<" origin="<<e.x<<","<<e.y<<" glyphs="<<e.indices.size()<<'\n';
    }
}
void fixture() {
    const auto vertical=oracle(true),horizontal=oracle(false);
    check(vertical.lines.size()==2,"Generous vertical frame retains exactly two hard-break columns");
    auto source=nect::default_text("japanese-vertical-oracle","「日本。、」ABC12\n（文章）123");
    source.family="Yu Gothic";source.locale="ja-JP";source.direction="vertical";source.layout="frame";source.alignment="start";
    source.parameters.at("font_size").literal=em;source.parameters.at("frame_width").literal=width;source.parameters.at("frame_height").literal=height;
    source.parameters.at("origin_x").literal=origin_x;source.parameters.at("origin_y").literal=origin_y;
    std::map<std::string,double> parameters;for(const auto& [key,value]:source.parameters)parameters[key]=value.literal;
    const auto actual=nect::evaluate_text(source,parameters);check(!actual.overflow,"Generous fixed frame does not clip or overflow");run_identity(vertical,actual);
    const auto projected=actual_edges(actual);geometry(vertical.edges,projected);
    auto box=bounds(vertical.edges);box.left=std::floor(box.left)-1;box.top=std::floor(box.top)-1;box.right=std::ceil(box.right)+1;box.bottom=std::ceil(box.bottom)+1;
    const auto w=static_cast<int>(std::ceil(box.width()*2)),h=static_cast<int>(std::ceil(box.height()*2));check(w>0&&h>0&&w<2400&&h<2400,"Bounded ink coverage region");
    const auto expected_bitmap=coverage(vertical.edges,box,w,h),actual_bitmap=coverage(projected,box,w,h);std::size_t changed=0,ink=0;
    for(std::size_t i=0;i<expected_bitmap.size();++i){changed+=expected_bitmap[i]!=actual_bitmap[i];ink+=expected_bitmap[i]!=0;}
    check(ink>500&&changed<=1+expected_bitmap.size()/10000,"0.5-DIP nonzero coverage differs by at most 0.01 percent plus one sample");
    // Assert vertical substitutions separately from global ink: a nonempty run
    // cannot hide horizontal punctuation/brackets or a missing character.
    for(const UINT32 position:{0u,3u,4u,5u,12u,15u}) {
        const auto v=character(vertical,position),hchar=character(horizontal,position);
        check(v.run->keys==hchar.run->keys&&v.run->index==hchar.run->index,"Punctuation oracle uses the same exact installed face");
        std::cout<<"punctuation utf16="<<position<<" glyph="<<hchar.glyph<<"->"<<v.glyph
            <<" ink="<<v.ink.left<<","<<v.ink.top<<","<<v.ink.right<<","<<v.ink.bottom<<'\n';
        check(v.glyph!=hchar.glyph,"Yu Gothic punctuation/bracket uses a distinct vertical-form glyph at UTF-16 "+std::to_string(position));
        check(v.ink.width()>0&&v.ink.height()>0&&v.ink.left>origin_x&&v.ink.right<origin_x+width+em&&v.ink.top>origin_y-em,"Vertical punctuation ink is positioned in the bounded frame");
    }
    // The second hard-break column is left of the first. CJK central origins
    // agree within a column; punctuation positions proceed down that column.
    const auto first=character(vertical,1),second=character(vertical,13);check(second.run->x<first.run->x-em/2,"Hard-break columns flow right to left");
    const auto stop=character(vertical,3),comma=character(vertical,4);check(comma.ink.top>stop.ink.top+em/2,"Japanese punctuation advances downward in source order");
    // Default script orientation rotates Latin/digits clockwise by 90 degrees,
    // while CJK remains upright. Compare actual per-glyph outline proportions
    // to independent horizontal layout, in addition to callback angles.
    // https://learn.microsoft.com/en-us/windows/win32/api/dwrite_1/ne-dwrite_1-dwrite_vertical_glyph_orientation
    for(const UINT32 position:{6u,7u,8u,9u,10u,16u,17u,18u}) {
        const auto v=character(vertical,position),hchar=character(horizontal,position);
        check(v.run->angle==DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES&&!v.run->sideways,"Latin/digit callback reports clockwise sideways presentation");
        check(v.glyph==hchar.glyph,"Latin/digit keeps its horizontal glyph identity");near(v.ink.width(),hchar.ink.height(),"Latin/digit rotated ink width");near(v.ink.height(),hchar.ink.width(),"Latin/digit rotated ink height");
    }
    for(const UINT32 position:{1u,2u,13u,14u}){const auto v=character(vertical,position),hchar=character(horizontal,position);
        near(v.ink.width(),hchar.ink.width(),"CJK upright ink width");near(v.ink.height(),hchar.ink.height(),"CJK upright ink height");}
    check(actual.column_baselines_x.size()==2,"Nect exposes both measured columns");
    double right=origin_x+width;for(std::size_t i=0;i<vertical.lines.size();++i){near(actual.column_baselines_x[i],right-vertical.lines[i].baseline,"Independent column baseline");right-=vertical.lines[i].height;}
    std::cout<<"coverage ink="<<ink<<" mismatch="<<changed<<" samples="<<expected_bitmap.size()<<'\n';
}
#endif
}
int main() {
#ifndef _WIN32
    std::cout<<"ENV_MISSING_FIXTURE: JAPANESE_VERTICAL_ORACLE requires Windows DirectWrite\n";return 77;
#else
    try {fixture();std::cout<<"PASS: Japanese vertical independent DirectWrite oracle ("<<checks<<" checks)\n";return 0;}
    catch(const MissingFixture& e){std::cout<<"ENV_MISSING_FIXTURE: JAPANESE_VERTICAL_ORACLE "<<e.what()<<'\n';return 77;}
    catch(const std::exception& e){std::cerr<<"FAIL: JAPANESE_VERTICAL_ORACLE "<<e.what()<<'\n';return 1;}
#endif
}
