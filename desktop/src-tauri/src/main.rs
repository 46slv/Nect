#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
mod acceptance_storage;
mod build_identity;
#[cfg(test)]
#[path = "../build_identity_config.rs"]
mod build_identity_config;
#[cfg(test)]
#[path = "../build_identity_sources.rs"]
mod build_identity_sources;
mod desktop_font_layout;
mod document_storage;
mod os_font_outlines;
#[cfg(feature = "recovery-acceptance-test")]
mod recovery_acceptance_gate;
use desktop_font_layout::{list_desktop_font_axes, list_desktop_font_faces, shape_desktop_text};
use document_storage::{
    choose_unsaved, enable_close_guard, finish_close, open_project, release_project, save_project,
    CloseGuard, Documents,
};
use os_font_outlines::{list_os_outline_fonts, outline_os_text};
use tauri::Manager;
#[rustfmt::skip]
mod system_fonts;
use acceptance_storage::RunStorage;
mod recovery_storage;
use recovery_storage::{
    acknowledge_recovery, begin_recovery, list_recovery, read_recovery, write_recovery, Recovery,
};
use system_fonts::{list_system_fonts, read_system_font, Catalog};

#[tauri::command]
fn open_svg_source(
    window: tauri::WebviewWindow,
    storage: tauri::State<'_, RunStorage>,
) -> Result<Option<(String, String)>, String> {
    let Some(path) = storage
        .dialog(&window)
        .add_filter("SVG vector", &["svg"])
        .pick_file()
    else {
        return Ok(None);
    };
    let contents = storage.read_svg_source(&path)?;
    let name = path
        .file_name()
        .ok_or("Missing SVG filename")?
        .to_string_lossy()
        .into_owned();
    Ok(Some((name, contents)))
}

#[tauri::command]
fn open_document(
    window: tauri::WebviewWindow,
    storage: tauri::State<'_, RunStorage>,
) -> Result<Option<String>, String> {
    let Some(path) = storage
        .dialog(&window)
        .add_filter("Nect project", &["json"])
        .pick_file()
    else {
        return Ok(None);
    };
    storage.read_project(&path).map(Some)
}

#[tauri::command]
fn save_document(
    window: tauri::WebviewWindow,
    contents: String,
    suggested_name: String,
    kind: String,
    storage: tauri::State<'_, RunStorage>,
) -> Result<bool, String> {
    if kind != "svg" && kind != "json" {
        return Err("Unsupported document kind".into());
    }
    if contents.len() > 50_000_000 {
        return Err("Document exceeds its size limit".into());
    }
    let extension = if kind == "svg" { "svg" } else { "json" };
    let Some(path) = storage
        .dialog(&window)
        .set_file_name(&suggested_name)
        .add_filter("Nect document", &[extension])
        .save_file()
    else {
        return Ok(false);
    };
    storage.write_bytes(&path, contents.as_bytes())?;
    Ok(true)
}

#[tauri::command]
fn save_png(
    window: tauri::WebviewWindow,
    bytes: Vec<u8>,
    suggested_name: String,
    storage: tauri::State<'_, RunStorage>,
) -> Result<bool, String> {
    if bytes.len() > 64_000_000 || !bytes.starts_with(&[137, 80, 78, 71, 13, 10, 26, 10]) {
        return Err("Invalid PNG or PNG exceeds 64 MB".into());
    }
    let Some(path) = storage
        .dialog(&window)
        .set_file_name(&suggested_name)
        .add_filter("PNG image", &["png"])
        .save_file()
    else {
        return Ok(false);
    };
    storage.write_bytes(&path, &bytes)?;
    Ok(true)
}

// Run before Builder::run: Tauri otherwise prepares its ordinary data directory
// for automatic windows before WebView2 sees environment overrides.
fn acceptance_window(
    config: &mut tauri::Config,
    enabled: bool,
) -> Result<Option<tauri::utils::config::WindowConfig>, String> {
    if !enabled {
        return Ok(None);
    }
    if config.app.windows.len() != 1 || !config.app.windows[0].create {
        return Err("Acceptance mode requires exactly one automatic window".into());
    }
    let window = config.app.windows[0].clone();
    config.app.windows[0].create = false;
    Ok(Some(window))
}

fn run() -> Result<(), String> {
    let requested = std::env::var_os(acceptance_storage::ROOT_ENV);
    #[cfg(feature = "recovery-acceptance-test")]
    recovery_acceptance_gate::require_isolation(requested.is_some())?;
    let mut context = tauri::generate_context!();
    let window = acceptance_window(context.config_mut(), requested.is_some())?;
    let storage = RunStorage::prepare(requested, || {
        std::env::current_exe()
            .map_err(|e| e.to_string())?
            .parent()
            .map(std::path::Path::to_path_buf)
            .ok_or_else(|| "Cannot locate acceptance executable directory".to_string())
    })?;
    if let Some(paths) = &storage.0 {
        // Process-only, before Tauri starts threads. This wins over an inherited
        // WebView2 UDF/registry override. The explicit builder directory also
        // prevents Tauri itself from creating its ordinary profile first.
        std::env::set_var("WEBVIEW2_USER_DATA_FOLDER", &paths.webview);
    }
    let window_storage = storage.clone();
    let identity = build_identity::BuildIdentity::compiled(
        &context.config().identifier,
        context.config().version.as_deref(),
        storage.0.is_some(),
    );
    tauri::Builder::default()
        .manage(identity)
        .manage(storage)
        .manage(Documents::default())
        .manage(CloseGuard::default())
        .manage(Catalog::default())
        .manage(Recovery::default())
        .invoke_handler(tauri::generate_handler![
            build_identity::query_build_identity,
            open_document,
            open_svg_source,
            save_document,
            save_png,
            list_system_fonts,
            read_system_font,
            list_os_outline_fonts,
            outline_os_text,
            list_desktop_font_faces,
            list_desktop_font_axes,
            shape_desktop_text,
            open_project,
            save_project,
            release_project,
            choose_unsaved,
            enable_close_guard,
            finish_close,
            begin_recovery,
            write_recovery,
            list_recovery,
            read_recovery,
            acknowledge_recovery
        ])
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                if let Ok(guard) = window.state::<CloseGuard>().0.lock() {
                    if let Some(channel) = guard.as_ref() {
                        api.prevent_close();
                        let _ = channel.send(());
                    }
                }
            }
        })
        .setup(move |app| {
            // Acceptance EXEs keep restartable recovery beside their disposable
            // profiles, wholly separate from the user's ordinary app data.
            let recovery_root = if let Some(paths) = &window_storage.0 {
                paths.check_directories().map(|_| paths.webview.parent().unwrap().parent().unwrap().join("nect-svg-acceptance-recovery"))
            } else {
                app.path().app_local_data_dir().map(|p| p.join("recovery-v1")).map_err(|e| e.to_string())
            };
            app.state::<Recovery>().initialize(recovery_root);
            if let Some(config) = window {
                let paths = window_storage
                    .0
                    .as_ref()
                    .ok_or("Missing acceptance storage")?;
                paths.check_directories()?;
                let builder = tauri::WebviewWindowBuilder::from_config(app, &config)?
                    .data_directory(paths.webview.clone());
                #[cfg(feature = "recovery-acceptance-test")]
                let builder = builder.initialization_script("Object.defineProperty(window, '__NECT_RECOVERY_TEST_INSTANCE__', { value: true });");
                builder.build()?;
            }
            Ok(())
        })
        .run(context)
        .expect("Could not start Nect SVG");
    Ok(())
}

fn main() {
    if let Err(error) = run() {
        eprintln!("Nect SVG startup failed: {error}");
        std::process::exit(2);
    }
}

#[cfg(test)]
mod startup_tests {
    use super::*;

    #[test]
    fn normal_windows_are_unchanged_and_acceptance_disables_auto_creation() {
        let mut config = tauri::Config::default();
        config.app.windows = vec![Default::default()];
        let before = config.app.windows[0].clone();
        assert!(acceptance_window(&mut config, false).unwrap().is_none());
        assert_eq!(config.app.windows[0].create, before.create);
        assert_eq!(config.app.windows[0].data_directory, before.data_directory);
        let manual = acceptance_window(&mut config, true).unwrap().unwrap();
        assert!(manual.create);
        assert!(!config.app.windows[0].create);
    }

    #[test]
    fn ambiguous_window_configuration_fails_without_mutation() {
        for count in [0, 2] {
            let mut config = tauri::Config::default();
            config.app.windows = vec![Default::default(); count];
            assert!(acceptance_window(&mut config, true).is_err());
            assert!(config.app.windows.iter().all(|window| window.create));
        }
        let mut config = tauri::Config::default();
        config.app.windows = vec![Default::default()];
        config.app.windows[0].create = false;
        assert!(acceptance_window(&mut config, true).is_err());
    }
}
