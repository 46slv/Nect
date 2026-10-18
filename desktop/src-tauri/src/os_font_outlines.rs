//! OS-font references produce geometry only; no font bytes cross IPC or reach disk.
#[derive(Clone, Debug, serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct OsFont { pub family: String, pub styles: Vec<u32> }
#[derive(Debug, serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct OsOutline {
    pub family: String, pub style: u32, pub text: String, pub size: f32,
    pub points: Vec<[f32; 2]>, pub types: Vec<u8>, pub ascent: f32,
}
#[cfg(windows)]
mod native {
    use super::*;
    use std::ffi::c_void;
    #[repr(C)]
    struct Point { x: f32, y: f32 }
    extern "C" {
        fn nect_list_os_fonts(callback: extern "C" fn(*const u16, i32, *mut c_void), data: *mut c_void) -> i32;
        fn nect_outline_os_text(name: *const u16, style: i32, text: *const u16, length: i32, size: f32,
            callback: extern "C" fn(*const Point, *const u8, i32, f32, *mut c_void), data: *mut c_void) -> i32;
    }
    extern "C" fn font(name: *const u16, styles: i32, data: *mut c_void) {
        // GDI+ supplies a NUL-terminated LF_FACESIZE buffer; never scan past it.
        let units = unsafe { std::slice::from_raw_parts(name, 32) };
        let end = units.iter().position(|v| *v == 0).unwrap_or(32);
        let Ok(family) = String::from_utf16(&units[..end]) else { return };
        if family.is_empty() || family.chars().any(char::is_control) { return; }
        let output = unsafe { &mut *(data as *mut Vec<OsFont>) };
        output.push(OsFont { family, styles: (0..=3).filter(|s| styles & (1 << s) != 0).collect() });
    }
    extern "C" fn outline(points: *const Point, types: *const u8, count: i32, ascent: f32, data: *mut c_void) {
        if !(1..=30000).contains(&count) { return; }
        let result = unsafe { &mut *(data as *mut OsOutline) };
        result.points = unsafe { std::slice::from_raw_parts(points, count as usize) }.iter().map(|p| [p.x,p.y]).collect();
        result.types = unsafe { std::slice::from_raw_parts(types, count as usize) }.to_vec();
        result.ascent = ascent;
    }
    pub fn list() -> Result<Vec<OsFont>, String> {
        let mut items = Vec::<OsFont>::new();
        let status = unsafe { nect_list_os_fonts(font, &mut items as *mut _ as *mut c_void) };
        if status != 0 { return Err(format!("OS font discovery failed ({status})")); }
        items.sort_by(|a,b| a.family.to_lowercase().cmp(&b.family.to_lowercase()));
        items.dedup_by(|a,b| a.family == b.family);
        Ok(items)
    }
    pub fn bake(family: String, style: u32, text: String, size: f32) -> Result<OsOutline, String> {
        if family.is_empty() || family.len() > 256 || text.is_empty() || text.len() > 1024 || !text.bytes().all(|c| (32..=126).contains(&c)) || text.trim().is_empty()
            || !size.is_finite() || !(0.01..=10000.0).contains(&size) || style > 3 {
            return Err("OS outlines require one printable ASCII line and size 0.01–10000 px".into());
        }
        if !list()?.iter().any(|f| f.family == family && f.styles.contains(&style)) {
            return Err("Requested OS font/style is no longer active".into());
        }
        let name: Vec<u16> = family.encode_utf16().chain(Some(0)).collect();
        let chars: Vec<u16> = text.encode_utf16().collect();
        let mut result = OsOutline { family, style, text, size, points: vec![], types: vec![], ascent: 0.0 };
        let status = unsafe { nect_outline_os_text(name.as_ptr(), style as i32, chars.as_ptr(), chars.len() as i32, size,
            outline, &mut result as *mut _ as *mut c_void) };
        if status != 0 || result.points.is_empty() || !result.ascent.is_finite() ||
            result.points.iter().flatten().any(|v| !v.is_finite() || v.abs() > 10000000.0) {
            return Err(format!("OS outline unavailable; no document change ({status}). Missing glyphs or unsupported outlines are refused."));
        }
        Ok(result)
    }
}
pub fn installed() -> Result<Vec<OsFont>, String> {
    #[cfg(windows)] { native::list() }
    #[cfg(not(windows))] { Ok(vec![]) }
}
pub fn prepare(family: String, style: u32, text: String, size: f32) -> Result<OsOutline, String> {
    #[cfg(windows)] { native::bake(family,style,text,size) }
    #[cfg(not(windows))] { let _ = (family,style,text,size); Err("OS outlines require Windows".into()) }
}
#[tauri::command]
pub async fn list_os_outline_fonts() -> Result<Vec<OsFont>, String> {
    tauri::async_runtime::spawn_blocking(installed).await.map_err(|e| e.to_string())?
}
#[tauri::command]
pub async fn outline_os_text(family: String, style: u32, text: String, size: f32) -> Result<OsOutline, String> {
    tauri::async_runtime::spawn_blocking(move || prepare(family,style,text,size)).await.map_err(|e| e.to_string())?
}

#[cfg(all(test, windows))]
mod tests {
    use super::*;
    #[test]
    fn active_os_reference_to_geometry_without_font_bytes() {
        let fonts = installed().unwrap();
        assert!(!fonts.is_empty());
        let font = fonts.iter().find(|f| f.family == "DIN Alternate")
            .or_else(|| fonts.iter().find(|f| f.family == "Arial")).unwrap();
        let style = font.styles[0];
        let outline = prepare(font.family.clone(), style, "DIN ABC 0123".into(), 100.0).unwrap();
        assert!(!outline.points.is_empty());
        assert_eq!(outline.points.len(),outline.types.len());
        assert!(outline.types.iter().any(|v| v & 128 != 0));
        assert!(outline.points.iter().flatten().all(|v| v.is_finite()));
        println!("OS_OUTLINE family={} style={} points={} ascent={}", font.family, style, outline.points.len(), outline.ascent);
        assert!(prepare("Missing-Nect-OS-Font-123456789".into(),0,"ABC".into(),100.0).is_err());
        assert!(prepare(font.family.clone(),style,"日本語".into(),100.0).is_err());
        assert!(prepare(font.family.clone(),4,"ABC".into(),100.0).is_err());
        assert!(prepare(font.family.clone(),style,"ABC".into(),f32::NAN).is_err());
        if let Some(output) = std::env::var_os("NECT_OS_OUTLINE_SMOKE_OUTPUT") {
            // Optional test receipt contains geometry, never font/cache data.
            std::fs::write(output,serde_json::to_vec(&outline).unwrap()).unwrap();
        }
    }
}
