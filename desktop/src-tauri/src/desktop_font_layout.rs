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
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub axes: Option<std::collections::BTreeMap<String, f32>>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub writing_mode: Option<String>,
}
#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct DesktopAxis {
    pub tag: String,
    pub name: String,
    pub min: f32,
    pub max: f32,
    pub default_value: f32,
    pub base_value: f32,
    pub value: f32,
    pub hidden: bool,
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
    #[serde(skip_serializing_if = "Option::is_none")]
    pub axes: Option<Vec<DesktopAxis>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub writing_mode: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub advance_height: Option<f32>,
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
    type AxisCallback = extern "C" fn(u32, *const u16, f32, f32, f32, f32, f32, i32, *mut c_void);
    extern "C" {
        fn nect_list_desktop_font_axes(
            family: *const u16,
            name: *const u16,
            weight: i32,
            style: i32,
            stretch: i32,
            callback: AxisCallback,
            data: *mut c_void,
        ) -> i32;
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
            axis_tags: *const u32,
            axis_values: *const f32,
            axis_count: i32,
            vertical: i32,
            axis_callback: AxisCallback,
            callback: extern "C" fn(
                *const Point,
                *const u8,
                i32,
                f32,
                f32,
                f32,
                i32,
                i32,
                *mut c_void,
            ),
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
        advance_height: f32,
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
        if result.advance_height.is_some() {
            result.advance_height = Some(advance_height);
        }
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
    fn axis_value(
        tag: u32,
        label: *const u16,
        min: f32,
        max: f32,
        default: f32,
        base: f32,
        value: f32,
        hidden: i32,
    ) -> Option<DesktopAxis> {
        let bytes = tag.to_le_bytes();
        if !bytes.iter().all(|b| (32..=126).contains(b)) {
            return None;
        }
        let name = unsafe { string(label) }?;
        if name.is_empty()
            || name.chars().any(char::is_control)
            || ![min, max, default, base, value]
                .iter()
                .all(|v| v.is_finite() && v.abs() <= 1000000.0)
            || min >= max
            || [default, base, value].iter().any(|v| *v < min || *v > max)
        {
            return None;
        }
        Some(DesktopAxis {
            tag: String::from_utf8(bytes.to_vec()).ok()?,
            name,
            min,
            max,
            default_value: default,
            base_value: base,
            value,
            hidden: hidden != 0,
        })
    }
    extern "C" fn axis(
        tag: u32,
        label: *const u16,
        min: f32,
        max: f32,
        default: f32,
        base: f32,
        value: f32,
        hidden: i32,
        data: *mut c_void,
    ) {
        if let Some(item) = axis_value(tag, label, min, max, default, base, value, hidden) {
            unsafe { &mut *(data as *mut Vec<DesktopAxis>) }.push(item);
        }
    }
    extern "C" fn outline_axis(
        tag: u32,
        label: *const u16,
        min: f32,
        max: f32,
        default: f32,
        base: f32,
        value: f32,
        hidden: i32,
        data: *mut c_void,
    ) {
        if let Some(item) = axis_value(tag, label, min, max, default, base, value, hidden) {
            if let Some(items) = &mut unsafe { &mut *(data as *mut DesktopOutline) }.axes {
                items.push(item);
            }
        }
    }
    pub fn axes(face: DesktopFace) -> Result<Vec<DesktopAxis>, String> {
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
        {
            return Err("Invalid Windows font face".into());
        }
        let family: Vec<u16> = face.family.encode_utf16().chain(Some(0)).collect();
        let name: Vec<u16> = face.name.encode_utf16().chain(Some(0)).collect();
        let mut result = Vec::new();
        let status = unsafe {
            nect_list_desktop_font_axes(
                family.as_ptr(),
                name.as_ptr(),
                face.weight as i32,
                face.style as i32,
                face.stretch as i32,
                axis,
                &mut result as *mut _ as *mut c_void,
            )
        };
        if status != 0 {
            return Err(format!("Windows font-axis discovery failed ({status})"));
        }
        result.sort_by(|a: &DesktopAxis, b: &DesktopAxis| a.tag.cmp(&b.tag));
        Ok(result)
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
            || request.axes.as_ref().is_some_and(|axes| {
                axes.len() > 32
                    || axes.iter().any(|(k, v)| {
                        k.len() != 4
                            || !k.bytes().all(|b| (32..=126).contains(&b))
                            || !v.is_finite()
                            || v.abs() > 1000000.0
                    })
            })
            || request
                .writing_mode
                .as_ref()
                .is_some_and(|v| !matches!(v.as_str(), "horizontal-tb" | "vertical-rl"))
        {
            return Err("Invalid Windows font, text or layout settings".into());
        }
        let family: Vec<u16> = face.family.encode_utf16().chain(Some(0)).collect();
        let name: Vec<u16> = face.name.encode_utf16().chain(Some(0)).collect();
        let text: Vec<u16> = request.text.encode_utf16().collect();
        let extended = request.axes.is_some() || request.writing_mode.is_some();
        let requested_axes = request.axes.clone().unwrap_or_default();
        let axis_tags: Vec<u32> = requested_axes
            .keys()
            .map(|k| u32::from_le_bytes(k.as_bytes().try_into().unwrap()))
            .collect();
        let axis_values: Vec<f32> = requested_axes.values().copied().collect();
        let mut result = DesktopOutline {
            engine: if extended {
                "directwrite-v2"
            } else {
                "directwrite-v1"
            },
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
            axes: if extended { Some(vec![]) } else { None },
            writing_mode: if extended {
                Some(
                    request
                        .writing_mode
                        .unwrap_or_else(|| "horizontal-tb".into()),
                )
            } else {
                None
            },
            advance_height: if extended { Some(0.0) } else { None },
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
                axis_tags.as_ptr(),
                axis_values.as_ptr(),
                if extended { axis_tags.len() as i32 } else { -1 },
                if result.writing_mode.as_deref() == Some("vertical-rl") {
                    1
                } else {
                    0
                },
                outline_axis,
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
            || result.axes.as_ref().is_some_and(|axes| {
                axes.len() != requested_axes.len()
                    || axes
                        .iter()
                        .any(|a| requested_axes.get(&a.tag) != Some(&a.value))
            })
            || result
                .advance_height
                .is_some_and(|v| !v.is_finite() || v < 0.0 || v > 10000000.0)
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
pub fn axes(face: DesktopFace) -> Result<Vec<DesktopAxis>, String> {
    #[cfg(windows)]
    {
        native::axes(face)
    }
    #[cfg(not(windows))]
    {
        let _ = face;
        Err("Windows font axes require Windows".into())
    }
}
#[tauri::command]
pub async fn list_desktop_font_axes(face: DesktopFace) -> Result<Vec<DesktopAxis>, String> {
    tauri::async_runtime::spawn_blocking(move || axes(face))
        .await
        .map_err(|e| e.to_string())?
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
            axes: None,
            writing_mode: None,
        }
    }
    #[test]
    fn actual_os_variable_axes_shape_and_refuse_unknown_clamped_requests() {
        let faces = installed().unwrap();
        let mut chosen = None;
        for family in [
            "Bahnschrift",
            "Segoe UI Variable Text",
            "Segoe UI Variable Display",
            "Segoe UI Variable Small",
        ] {
            if let Some(face) = faces
                .iter()
                .find(|f| f.family == family && f.weight == 400 && f.style == 0 && f.stretch == 5)
            {
                let ranges = axes(face.clone()).unwrap();
                println!("OS_AXES family={family} ranges={ranges:?}");
                if family.starts_with("Segoe UI") {
                    for a in &ranges {
                        let mut check = request(face.clone(), "Variable AV office\nText ABC 123");
                        check.axes = Some(
                            ranges
                                .iter()
                                .map(|v| (v.tag.clone(), v.base_value))
                                .collect(),
                        );
                        check.writing_mode = Some("horizontal-tb".into());
                        check.axes.as_mut().unwrap().insert(a.tag.clone(), a.min);
                        let low = prepare(check.clone()).unwrap();
                        check.axes.as_mut().unwrap().insert(a.tag.clone(), a.max);
                        let high = prepare(check).unwrap();
                        assert_ne!(
                            low.points, high.points,
                            "{family} axis {} must alter real outlines",
                            a.tag
                        );
                    }
                }
                if !ranges.is_empty() && chosen.is_none() {
                    chosen = Some((face.clone(), ranges));
                }
            }
        }
        let (face, ranges) =
            chosen.expect("A real OS variable family is required on this acceptance host");
        let mut req = request(face, "Variable AV office\nText ABC 123");
        req.axes = Some(
            ranges
                .iter()
                .map(|a| (a.tag.clone(), a.base_value))
                .collect(),
        );
        req.writing_mode = Some("horizontal-tb".into());
        let base = prepare(req.clone()).unwrap();
        assert_eq!(base.engine, "directwrite-v2");
        assert_eq!(base.axes.as_ref().unwrap().len(), ranges.len());
        assert_eq!(base.line_count, 2);
        let axis = ranges
            .iter()
            .find(|a| a.tag == "wght")
            .unwrap_or(&ranges[0]);
        req.axes
            .as_mut()
            .unwrap()
            .insert(axis.tag.clone(), axis.min);
        let low = prepare(req.clone()).unwrap();
        req.axes
            .as_mut()
            .unwrap()
            .insert(axis.tag.clone(), axis.max);
        let high = prepare(req.clone()).unwrap();
        assert_ne!(
            low.points, high.points,
            "Changing a real variable axis must alter the outline"
        );
        println!(
            "OS_AXIS_SHAPED family={} axis={} min={} max={} points={}..{}",
            base.face.family,
            axis.tag,
            axis.min,
            axis.max,
            low.points.len(),
            high.points.len()
        );
        req.axes
            .as_mut()
            .unwrap()
            .insert(axis.tag.clone(), axis.max + 1.0);
        assert!(prepare(req.clone()).is_err());
        req.axes
            .as_mut()
            .unwrap()
            .insert(axis.tag.clone(), axis.base_value);
        let mut outputs = vec![base, low, high];
        for text in ["", " ", "\n"] {
            let mut blank = req.clone();
            blank.text = text.into();
            assert!(prepare(blank).unwrap().points.is_empty());
        }
        for other in ranges.iter().filter(|a| a.tag != axis.tag) {
            req.axes
                .as_mut()
                .unwrap()
                .insert(other.tag.clone(), other.min);
            let low = prepare(req.clone()).unwrap();
            req.axes
                .as_mut()
                .unwrap()
                .insert(other.tag.clone(), other.max);
            let high = prepare(req.clone()).unwrap();
            assert_ne!(low.points, high.points);
            outputs.push(low);
            outputs.push(high);
            req.axes
                .as_mut()
                .unwrap()
                .insert(other.tag.clone(), other.base_value);
        }
        req.text = "日本語".into();
        assert!(prepare(req.clone()).unwrap_err().contains("substitution"));
        req.text = "Variable AV office\nText ABC 123".into();
        req.axes.as_mut().unwrap().insert("JUNK".into(), 0.0);
        assert!(prepare(req).is_err());
        if let Some(path) = std::env::var_os("NECT_OS_AXES_SMOKE_OUTPUT") {
            std::fs::write(path, serde_json::to_vec(&outputs).unwrap()).unwrap();
        }
    }
    #[test]
    fn japanese_vertical_keeps_upright_shapes_rotates_latin_and_flows_right_to_left() {
        let faces = installed().unwrap();
        let face = faces
            .iter()
            .find(|f| f.family == "Meiryo" && f.weight == 400 && f.style == 0)
            .unwrap();
        let vertical = |text: &str| {
            let mut r = request(face.clone(), text);
            r.axes = Some(Default::default());
            r.writing_mode = Some("vertical-rl".into());
            r
        };
        for text in ["", " ", "\n", "\t"] {
            assert!(prepare(vertical(text)).unwrap().points.is_empty());
        }
        let mut outputs = vec![];
        for (text, sideways) in [("あ", false), ("A", true)] {
            let h = prepare(request(face.clone(), text)).unwrap();
            let v = prepare(vertical(text)).unwrap();
            assert_eq!(h.points.len(), v.points.len());
            for (hp, vp) in h.points.iter().zip(&v.points) {
                let dx = hp[0] - h.points[0][0];
                let dy = hp[1] - h.points[0][1];
                let expected = if sideways { [-dy, dx] } else { [dx, dy] };
                assert!(
                    (vp[0] - v.points[0][0] - expected[0]).abs() < 0.002
                        && (vp[1] - v.points[0][1] - expected[1]).abs() < 0.002,
                    "Glyph orientation for {text}: {hp:?} {vp:?}"
                );
            }
            println!(
                "VERTICAL_GLYPH text={text} points={} width={} height={} ascent={}",
                v.points.len(),
                v.advance_width,
                v.advance_height.unwrap(),
                v.ascent
            );
            outputs.push(v);
        }
        let single = prepare(vertical("あ")).unwrap();
        let pair = prepare(vertical("あ\nあ")).unwrap();
        assert_eq!(pair.points.len(), single.points.len() * 2);
        assert_eq!(pair.line_count, 2);
        let column_dx = pair.points[single.points.len()][0] - pair.points[0][0];
        let column_dy = pair.points[single.points.len()][1] - pair.points[0][1];
        assert!(column_dx < -100.0);
        assert!(column_dy.abs() < 0.002);
        println!("VERTICAL_COLUMNS dx={column_dx} dy={column_dy}");
        outputs.push(pair);
        let mut req = vertical("日本語。かな、カナ（ABC）\n縦書きテスト 123");
        let mixed = prepare(req.clone()).unwrap();
        assert_eq!(mixed.line_count, 2);
        assert!(!mixed.points.is_empty());
        println!(
            "VERTICAL_MIXED points={} glyphs={} width={} height={} ascent={}",
            mixed.points.len(),
            mixed.glyph_count,
            mixed.advance_width,
            mixed.advance_height.unwrap(),
            mixed.ascent
        );
        outputs.push(mixed);
        req.tracking = 4.0;
        req.line_spacing = 2.0;
        outputs.push(prepare(req.clone()).unwrap());
        req.size = 32.0;
        req.tracking = 0.0;
        req.line_spacing = 1.2;
        outputs.push(prepare(req.clone()).unwrap());
        let mut horizontal = request(face.clone(), &req.text);
        horizontal.size = 32.0;
        outputs.push(prepare(horizontal.clone()).unwrap());
        horizontal.axes = Some(Default::default());
        horizontal.writing_mode = Some("horizontal-tb".into());
        outputs.push(prepare(horizontal).unwrap());
        req.tracking = 4.0;
        outputs.push(prepare(req.clone()).unwrap());
        req.line_spacing = 2.0;
        outputs.push(prepare(req).unwrap());
        if let Some(path) = std::env::var_os("NECT_OS_VERTICAL_SMOKE_OUTPUT") {
            std::fs::write(path, serde_json::to_vec(&outputs).unwrap()).unwrap();
        }
    }
    #[test]
    fn exact_gdi_faces_preserve_catalog_and_shape_without_reference_alias_rejection() {
        let before = crate::os_font_outlines::installed().unwrap();
        let first = installed().unwrap();
        let second = installed().unwrap();
        let after = crate::os_font_outlines::installed().unwrap();
        let mut old: Vec<_> = before.iter().map(|f| f.family.clone()).collect();
        old.sort();
        let mut new: Vec<_> = after.iter().map(|f| f.family.clone()).collect();
        new.sort();
        assert_eq!(old, new, "DirectWrite must not remove active GDI families");
        let ids = |faces: &Vec<DesktopFace>| {
            faces
                .iter()
                .map(|f| (&f.family, &f.name, f.weight, f.style, f.stretch))
                .map(|(a, b, c, d, e)| (a.clone(), b.clone(), c, d, e))
                .collect::<Vec<_>>()
        };
        assert_eq!(ids(&first), ids(&second));
        if before.iter().any(|f| f.family == "DIN Alternate") {
            let face = first
                .iter()
                .find(|f| f.family == "DIN Alternate")
                .expect("Active GDI DIN must expose an exact DirectWrite face");
            println!(
                "DIN_EXACT_FACE {face:?}; GDI families before={} after={}",
                before.len(),
                after.len()
            );
            for text in ["DIN ABC 0123", "office fi", "DIN ABC 0123\nTEXT ABC 123"] {
                let outline = prepare(request(face.clone(), text)).unwrap();
                assert!(!outline.points.is_empty());
                assert_eq!(outline.line_count, if text.contains('\n') { 2 } else { 1 });
            }
            assert!(prepare(request(face.clone(), "日本語"))
                .unwrap_err()
                .contains("substitution"));
        }
    }
    #[test]
    fn actual_faces_unicode_multiline_and_strict_no_fallback() {
        let faces = installed().unwrap();
        assert!(faces.len() > 10);
        println!("DIRECTWRITE_FACES={}", faces.len());
        for f in faces
            .iter()
            .filter(|f| f.family == "DIN Alternate" || f.family.starts_with("DIN "))
        {
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
