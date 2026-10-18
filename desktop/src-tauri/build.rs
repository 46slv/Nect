fn main() {
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
