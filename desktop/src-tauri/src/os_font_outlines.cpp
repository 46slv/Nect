// Public GDI+ wrappers only. Never read a font file or font/cache bytes.
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <memory>
#include <cmath>
using namespace Gdiplus;
struct Runtime {
    ULONG_PTR token = 0;
    Runtime() { GdiplusStartupInput input; if (GdiplusStartup(&token, &input, nullptr) != Ok) token = 0; }
    ~Runtime() { if (token) GdiplusShutdown(token); }
};
using FontCallback = void (*)(const wchar_t*, int, void*);
using PathCallback = void (*)(const PointF*, const BYTE*, int, float, void*);
extern "C" int nect_list_os_fonts(FontCallback callback, void* data) {
    Runtime runtime; if (!runtime.token) return -1;
    InstalledFontCollection installed;
    int count = installed.GetFamilyCount();
    if (count < 0 || count > 4096) return -2;
    std::unique_ptr<FontFamily[]> families(new FontFamily[count]);
    int found = 0;
    if (installed.GetFamilies(count, families.get(), &found) != Ok) return -3;
    for (int i = 0; i < found; ++i) {
        wchar_t name[LF_FACESIZE] = {};
        if (families[i].GetFamilyName(name, LANG_ENGLISH) != Ok || !name[0]) continue;
        int styles = 0;
        for (int style = 0; style <= 3; ++style)
            if (families[i].IsStyleAvailable(style)) styles |= 1 << style;
        if (styles) callback(name, styles, data);
    }
    return 0;
}
extern "C" int nect_outline_os_text(const wchar_t* name, int style,
    const wchar_t* text, int length, float size, PathCallback callback, void* data) {
    if (style < 0 || style > 3 || length < 1 || length > 1024 || !std::isfinite(size) || size < 0.01f || size > 10000) return -2;
    Runtime runtime; if (!runtime.token) return -1;
    FontFamily family(name);
    if (family.GetLastStatus() != Ok || !family.IsStyleAvailable(style)) return -4;
    // Reject missing characters instead of letting another font supply glyphs.
    HDC dc = CreateCompatibleDC(nullptr); if (!dc) return -5;
    bool covered = false;
    {
        Graphics graphics(dc); Font font(&family, size, style, UnitPixel);
        LOGFONTW lf = {};
        if (font.GetLogFontW(&graphics, &lf) == Ok) {
            HFONT selected = CreateFontIndirectW(&lf);
            if (selected) {
                HGDIOBJ prior = SelectObject(dc, selected);
                std::vector<WORD> glyphs(length);
                if (prior && prior != HGDI_ERROR) {
                    wchar_t actual[LF_FACESIZE] = {};
                    DWORD result = GetGlyphIndicesW(dc, text, length, glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS);
                    covered = result != GDI_ERROR && GetTextFaceW(dc, LF_FACESIZE, actual) > 0 && lstrcmpiW(actual, lf.lfFaceName) == 0;
                    for (WORD glyph : glyphs) if (glyph == 0xffff || glyph == 0) covered = false;
                    SelectObject(dc, prior);
                }
                DeleteObject(selected);
            }
        }
    }
    DeleteDC(dc); if (!covered) return -6;
    GraphicsPath path(FillModeWinding);
    if (path.AddString(text, length, &family, style, size, PointF(0, 0), StringFormat::GenericTypographic()) != Ok) return -7;
    int count = path.GetPointCount();
    if (count < 1 || count > 30000) return -8;
    std::vector<PointF> points(count); std::vector<BYTE> types(count);
    if (path.GetPathPoints(points.data(), count) != Ok || path.GetPathTypes(types.data(), count) != Ok) return -9;
    UINT16 em = family.GetEmHeight(style), ascent = family.GetCellAscent(style);
    if (!em) return -10;
    callback(points.data(), types.data(), count, size * ascent / em, data);
    return 0;
}
