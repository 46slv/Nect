//! Shaped OS typography through DirectWrite references; geometry only over IPC.
#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct DesktopFace {
    pub family: String,
    pub name: String,
    pub weight: u32,
    pub style: u32,
    pub stretch: u32,
    pub format: u32,
    pub color: bool,
}
#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct DesktopTextRequest {
    pub face: DesktopFace,
    pub text: String,
    pub size: f32,
    pub tracking: f32,
    pub line_spacing: f32,
    pub kerning: bool,
    pub ligatures: bool,
    pub alignment: u32,
}
#[derive(Debug, serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct DesktopOutline {
    pub engine: &'static str,
    pub face: DesktopFace,
    pub text: String,
    pub size: f32,
    pub tracking: f32,
    pub line_spacing: f32,
    pub kerning: bool,
    pub ligatures: bool,
    pub alignment: u32,
    pub points: Vec<[f32; 2]>,
    pub types: Vec<u8>,
    pub ascent: f32,
    pub advance_width: f32,
    pub line_count: u32,
    pub glyph_count: u32,
}
#[cfg(windows)]
mod native {
    use super::*;
    use std::ffi::c_void;
    #[repr(C)]
    struct Point {
        x: f32,
        y: f32,
    }
    extern "C" {
        fn nect_list_desktop_font_faces(
            callback: extern "C" fn(*const u16, *const u16, i32, i32, i32, i32, i32, *mut c_void),
            data: *mut c_void,
        ) -> i32;
        fn nect_shape_desktop_text(
            family: *const u16,
            name: *const u16,
            weight: i32,
            style: i32,
            stretch: i32,
            text: *const u16,
            length: i32,
            size: f32,
            tracking: f32,
            line_spacing: f32,
            kerning: i32,
            ligatures: i32,
            alignment: i32,
            callback: extern "C" fn(*const Point, *const u8, i32, f32, f32, i32, i32, *mut c_void),
            data: *mut c_void,
        ) -> i32;
    }
    unsafe fn string(value: *const u16) -> Option<String> {
        // The native producer bounds localized names to 255 units plus NUL.
        let mut units = Vec::new();
        for i in 0..256 {
            let unit = *value.add(i);
            if unit == 0 {
                return String::from_utf16(&units).ok();
            }
            units.push(unit);
        }
        None
    }
    extern "C" fn face(
        family: *const u16,
        name: *const u16,
        weight: i32,
        style: i32,
        stretch: i32,
        format: i32,
        color: i32,
        data: *mut c_void,
    ) {
        let (Some(family), Some(name)) = (unsafe { string(family) }, unsafe { string(name) })
        else {
            return;
        };
        if family.is_empty()
            || name.is_empty()
            || family.chars().chain(name.chars()).any(char::is_control)
        {
            return;
        }
        unsafe { &mut *(data as *mut Vec<DesktopFace>) }.push(DesktopFace {
            family,
            name,
            weight: weight as u32,
            style: style as u32,
            stretch: stretch as u32,
            format: format as u32,
            color: color != 0,
        });
    }
    extern "C" fn outline(
        points: *const Point,
        types: *const u8,
        count: i32,
        ascent: f32,
        advance: f32,
        lines: i32,
        glyphs: i32,
        data: *mut c_void,
    ) {
        if !(0..=300000).contains(&count) {
            return;
        }
        let result = unsafe { &mut *(data as *mut DesktopOutline) };
        result.ascent = ascent;
        result.advance_width = advance;
        result.line_count = lines as u32;
        result.glyph_count = glyphs as u32;
        if count > 0 {
            result.points = unsafe { std::slice::from_raw_parts(points, count as usize) }
                .iter()
                .map(|p| [p.x, p.y])
                .collect();
            result.types = unsafe { std::slice::from_raw_parts(types, count as usize) }.to_vec();
        }
    }
    pub fn list() -> Result<Vec<DesktopFace>, String> {
        let mut items = Vec::new();
        let status =
            unsafe { nect_list_desktop_font_faces(face, &mut items as *mut _ as *mut c_void) };
        if status != 0 {
            return Err(format!("Windows font discovery failed ({status})"));
        }
        items.sort_by(|a: &DesktopFace, b: &DesktopFace| {
            (&a.family, &a.name, a.weight, a.style, a.stretch)
                .cmp(&(&b.family, &b.name, b.weight, b.style, b.stretch))
        });
        items.dedup_by(|a, b| {
            a.family == b.family
                && a.name == b.name
                && a.weight == b.weight
                && a.style == b.style
                && a.stretch == b.stretch
        });
        Ok(items)
    }
    pub fn shape(request: DesktopTextRequest) -> Result<DesktopOutline, String> {
        let face = &request.face;
        if face.family.is_empty()
            || face.name.is_empty()
            || face.family.encode_utf16().count() > 255
            || face.name.encode_utf16().count() > 255
            || face
                .family
                .chars()
                .chain(face.name.chars())
                .any(char::is_control)
            || !(1..=999).contains(&face.weight)
            || face.style > 2
            || !(1..=9).contains(&face.stretch)
            || request.text.encode_utf16().count() > 4096
            || request.text.chars().any(|c| {
                (c.is_control() && c != '\n' && c != '\t') || matches!(c, '\u{fffe}' | '\u{ffff}')
            })
            || !request.size.is_finite()
            || !(0.01..=10000.0).contains(&request.size)
            || !request.tracking.is_finite()
            || request.tracking.abs() > 10000.0
            || !request.line_spacing.is_finite()
            || !(0.1..=100.0).contains(&request.line_spacing)
            || request.alignment > 2
        {
            return Err("Invalid Windows font, text or layout settings".into());
        }
        let family: Vec<u16> = face.family.encode_utf16().chain(Some(0)).collect();
        let name: Vec<u16> = face.name.encode_utf16().chain(Some(0)).collect();
        let text: Vec<u16> = request.text.encode_utf16().collect();
        let mut result = DesktopOutline {
            engine: "directwrite-v1",
            face: face.clone(),
            text: request.text,
            size: request.size,
            tracking: request.tracking,
            line_spacing: request.line_spacing,
            kerning: request.kerning,
            ligatures: request.ligatures,
            alignment: request.alignment,
            points: vec![],
            types: vec![],
            ascent: 0.0,
            advance_width: 0.0,
            line_count: 0,
            glyph_count: 0,
        };
        let status = unsafe {
            nect_shape_desktop_text(
                family.as_ptr(),
                name.as_ptr(),
                face.weight as i32,
                face.style as i32,
                face.stretch as i32,
                text.as_ptr(),
                text.len() as i32,
                result.size,
                result.tracking,
                result.line_spacing,
                result.kerning as i32,
                result.ligatures as i32,
                result.alignment as i32,
                outline,
                &mut result as *mut _ as *mut c_void,
            )
        };
        if status != 0 {
            return Err(match status{
            -2147023728=>"Selected Windows font/face is no longer available; saved outlines are preserved".into(),
            -2147023783=>"Missing glyphs or font substitution refused; choose a font containing the entire text".into(),
            -2147467263=>"Color/bitmap font or unsupported text-layout feature; no substitution was applied".into(),
            -2147483637=>"Text outline exceeds the supported geometry/line budget".into(),
            _=>format!("Windows text shaping failed ({status}); no document change"),
        });
        }
        if (!result.text.trim().is_empty() && result.points.is_empty())
            || !result.ascent.is_finite()
            || !result.advance_width.is_finite()
            || result.advance_width < 0.0
            || result.advance_width > 10000000.0
            || result.line_count > 256
            || result.glyph_count > 20000
            || result
                .points
                .iter()
                .flatten()
                .any(|v| !v.is_finite() || v.abs() > 10000000.0)
        {
            return Err(format!(
                "Invalid Windows outline result (points={}, ascent={}, lines={}, glyphs={})",
                result.points.len(),
                result.ascent,
                result.line_count,
                result.glyph_count
            ));
        }
        Ok(result)
    }
}
pub fn installed() -> Result<Vec<DesktopFace>, String> {
    #[cfg(windows)]
    {
        native::list()
    }
    #[cfg(not(windows))]
    {
        Ok(vec![])
    }
}
pub fn prepare(request: DesktopTextRequest) -> Result<DesktopOutline, String> {
    #[cfg(windows)]
    {
        native::shape(request)
    }
    #[cfg(not(windows))]
    {
        let _ = request;
        Err("Windows text shaping requires Windows".into())
    }
}
#[tauri::command]
pub async fn list_desktop_font_faces() -> Result<Vec<DesktopFace>, String> {
    tauri::async_runtime::spawn_blocking(installed)
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
pub async fn shape_desktop_text(request: DesktopTextRequest) -> Result<DesktopOutline, String> {
    tauri::async_runtime::spawn_blocking(move || prepare(request))
        .await
        .map_err(|e| e.to_string())?
}
#[cfg(all(test, windows))]
mod tests {
    use super::*;
    fn request(face: DesktopFace, text: &str) -> DesktopTextRequest {
        DesktopTextRequest {
            face,
            text: text.into(),
            size: 100.0,
            tracking: 0.0,
            line_spacing: 1.2,
            kerning: true,
            ligatures: true,
            alignment: 0,
        }
    }
    #[test]
    fn actual_faces_unicode_multiline_and_strict_no_fallback() {
        let faces = installed().unwrap();
        assert!(faces.len() > 10);
        println!("DIRECTWRITE_FACES={}", faces.len());
        for f in faces.iter().filter(|f| {
            f.family.to_lowercase().contains("din") || f.name.to_lowercase().contains("din")
        }) {
            println!(
                "DIRECTWRITE_DIN family={} face={} weight={} style={}",
                f.family, f.name, f.weight, f.style
            );
        }
        let mut outputs = vec![];
        for family in [
            "DIN Alternate",
            "Arial",
            "Times New Roman",
            "Meiryo",
            "Segoe UI",
        ] {
            let Some(face) = faces
                .iter()
                .find(|f| f.family == family && f.style == 0 && !f.color)
            else {
                continue;
            };
            let text = if family == "Meiryo" {
                "日本語かなカナ\n新しい文字 ABC"
            } else {
                "AV office fi\nFont ABC 123"
            };
            let result = prepare(request(face.clone(), text)).unwrap();
            assert!(result.points.len() > 10);
            assert_eq!(result.line_count, 2);
            println!(
                "DIRECTWRITE family={} face={} weight={} style={} format={} glyphs={} points={}",
                face.family,
                face.name,
                face.weight,
                face.style,
                face.format,
                result.glyph_count,
                result.points.len()
            );
            outputs.push(result);
        }
        let arial = faces
            .iter()
            .find(|f| f.family == "Arial" && f.weight == 400 && f.style == 0)
            .unwrap();
        assert!(prepare(request(arial.clone(), "日本語")).is_err());
        assert!(prepare(request(arial.clone(), "ABC日本語"))
            .unwrap_err()
            .contains("substitution"));
        if let Some(face) = faces.iter().find(|f| f.format == 0 && !f.color) {
            if let Ok(result) = prepare(request(face.clone(), "Font ABC 123")) {
                println!(
                    "DIRECTWRITE_CFF family={} face={} points={}",
                    face.family,
                    face.name,
                    result.points.len()
                );
                outputs.push(result);
            }
        }
        let meiryo = faces
            .iter()
            .find(|f| f.family == "Meiryo" && f.weight == 400 && f.style == 0)
            .unwrap();
        let mut req = request(meiryo.clone(), "日本語かな\nText ABC");
        outputs.push(prepare(req.clone()).unwrap());
        req.text = "日本語の編集\n新しい Text".into();
        outputs.push(prepare(req.clone()).unwrap());
        req.tracking = 4.0;
        outputs.push(prepare(req.clone()).unwrap());
        req.size = 80.0;
        outputs.push(prepare(req.clone()).unwrap());
        req.line_spacing = 2.0;
        outputs.push(prepare(req.clone()).unwrap());
        req.kerning = false;
        outputs.push(prepare(req.clone()).unwrap());
        req.ligatures = false;
        outputs.push(prepare(req.clone()).unwrap());
        req.alignment = 1;
        outputs.push(prepare(req.clone()).unwrap());
        for text in ["", " ", "\n", "\t"] {
            let blank = prepare(request(meiryo.clone(), text)).unwrap();
            assert!(blank.points.is_empty());
            assert!(blank.line_count >= 1);
        }
        if let Some(path) = std::env::var_os("NECT_DESKTOP_FONT_SMOKE_OUTPUT") {
            std::fs::write(path, serde_json::to_vec(&outputs).unwrap()).unwrap();
        }
    }
    #[test]
    fn complex_shaping_spacing_and_feature_controls() {
        let faces = installed().unwrap();
        let arial = faces
            .iter()
            .find(|f| f.family == "Arial" && f.weight == 400 && f.style == 0)
            .unwrap();
        for text in ["Ελληνικά Русский", "مرحبا بالعالم", "שלום עולם"]
        {
            let face = if text == "Ελληνικά Русский" {
                arial
            } else {
                faces
                    .iter()
                    .find(|f| f.family == "Segoe UI" && f.weight == 400 && f.style == 0)
                    .unwrap()
            };
            let shaped = prepare(request(face.clone(), text)).unwrap();
            println!(
                "DIRECTWRITE_SCRIPT text={text} glyphs={} points={}",
                shaped.glyph_count,
                shaped.points.len()
            );
        }
        let width = |r: &DesktopOutline| {
            r.points
                .iter()
                .map(|p| p[0])
                .fold(f32::NEG_INFINITY, f32::max)
                - r.points.iter().map(|p| p[0]).fold(f32::INFINITY, f32::min)
        };
        let base = prepare(request(arial.clone(), "AV office fi\nSpacing")).unwrap();
        let mut spaced_request = request(arial.clone(), "AV office fi\nSpacing");
        spaced_request.tracking = 4.0;
        spaced_request.line_spacing = 2.0;
        let spaced = prepare(spaced_request).unwrap();
        assert!(width(&spaced) > width(&base));
        assert!(
            spaced
                .points
                .iter()
                .map(|p| p[1])
                .fold(f32::NEG_INFINITY, f32::max)
                > base
                    .points
                    .iter()
                    .map(|p| p[1])
                    .fold(f32::NEG_INFINITY, f32::max)
        );
        let kern = prepare(request(arial.clone(), "AVAV")).unwrap();
        let mut no_kern = request(arial.clone(), "AVAV");
        no_kern.kerning = false;
        let no_kern = prepare(no_kern).unwrap();
        assert!((width(&kern) - width(&no_kern)).abs() > 0.01);
        let mut changed = false;
        for family in ["Times New Roman", "Calibri", "Segoe UI"] {
            if let Some(face) = faces
                .iter()
                .find(|f| f.family == family && f.weight == 400 && f.style == 0)
            {
                let with = prepare(request(face.clone(), "office fi ffi")).unwrap();
                let mut req = request(face.clone(), "office fi ffi");
                req.ligatures = false;
                let without = prepare(req).unwrap();
                if with.glyph_count != without.glyph_count {
                    changed = true;
                    println!(
                        "DIRECTWRITE_LIGATURE family={family} on={} off={}",
                        with.glyph_count, without.glyph_count
                    );
                }
            }
        }
        assert!(
            changed,
            "At least one real installed family must demonstrate ligature toggle"
        );
    }
}
