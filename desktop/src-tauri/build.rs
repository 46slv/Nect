mod build_identity_config;
mod build_identity_sources;
mod recovery_build_profile;
fn main() {
    println!("cargo:rerun-if-env-changed=TAURI_CONFIG");
    println!("cargo:rerun-if-changed=recovery_build_profile.rs");
    println!("cargo:rerun-if-changed=build_identity_config.rs");
    println!("cargo:rerun-if-changed=build_identity_sources.rs");
    let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let effective = build_identity_sources::checked_config(root, env!("CARGO_PKG_VERSION"));
    println!(
        "cargo:rustc-env=NECT_BUILD_TARGET={}",
        std::env::var("TARGET").expect("Missing native target")
    );
    let test_build = std::env::var_os("CARGO_FEATURE_RECOVERY_ACCEPTANCE_TEST").is_some();
    let static_assets = std::env::var_os("CARGO_FEATURE_CUSTOM_PROTOCOL").is_some();
    recovery_build_profile::require_static(test_build, static_assets)
        .expect("Invalid Recovery test build mode");
    if static_assets {
        let frontend = effective
            .get("build")
            .and_then(|b| b.get("frontendDist"))
            .and_then(|p| p.as_str())
            .unwrap_or("../dist");
        let marker = root.join(frontend).join("nect-build-profile.txt");
        println!("cargo:rerun-if-changed={}", marker.display());
        let profile = std::fs::read_to_string(&marker)
            .expect("Build the matching frontend before native compilation");
        recovery_build_profile::validate(&profile, test_build)
            .expect("Frontend assets do not match this normal/test native build");
        let version_marker = root.join(frontend).join("nect-build-version.txt");
        println!("cargo:rerun-if-changed={}", version_marker.display());
        let frontend_version = std::fs::read_to_string(version_marker)
            .expect("Build the version-matched frontend before native compilation");
        build_identity_config::validate_version_set([
            env!("CARGO_PKG_VERSION"),
            env!("CARGO_PKG_VERSION"),
            env!("CARGO_PKG_VERSION"),
            &frontend_version,
        ])
        .expect("Frontend assets do not match the native version");
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
