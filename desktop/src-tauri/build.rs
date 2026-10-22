mod recovery_build_profile;
fn main() {
    println!("cargo:rerun-if-env-changed=TAURI_CONFIG");
    println!("cargo:rerun-if-changed=recovery_build_profile.rs");
    let test_build = std::env::var_os("CARGO_FEATURE_RECOVERY_ACCEPTANCE_TEST").is_some();
    let static_assets = std::env::var_os("CARGO_FEATURE_CUSTOM_PROTOCOL").is_some();
    recovery_build_profile::require_static(test_build, static_assets).expect("Invalid Recovery test build mode");
    if static_assets {
        let overlay: serde_json::Value = std::env::var("TAURI_CONFIG").ok()
            .map(|s| serde_json::from_str(&s).expect("Invalid TAURI_CONFIG JSON"))
            .unwrap_or(serde_json::Value::Null);
        let frontend = overlay.get("build").and_then(|b| b.get("frontendDist"))
            .and_then(|p| p.as_str()).unwrap_or("../dist");
        let marker = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join(frontend).join("nect-build-profile.txt");
        println!("cargo:rerun-if-changed={}", marker.display());
        let profile = std::fs::read_to_string(&marker).expect("Build the matching frontend before native compilation");
        recovery_build_profile::validate(&profile, test_build).expect("Frontend assets do not match this normal/test native build");
    }
    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        cc::Build::new()
            .cpp(true)
            .file("src/os_font_outlines.cpp")
            .file("src/desktop_font_layout.cpp")
            .compile("nect_os_font_outlines");
        println!("cargo:rustc-link-lib=gdiplus");
        println!("cargo:rustc-link-lib=gdi32");
        println!("cargo:rustc-link-lib=dwrite");
        println!("cargo:rerun-if-changed=src/os_font_outlines.cpp");
        println!("cargo:rerun-if-changed=src/desktop_font_layout.cpp");
    }
    tauri_build::build()
}
