//! Transport for an explicitly isolated native test build. Never listens or
//! discovers a running app. The parent owns both standard stream handles.
use crate::live_editor_frames::serve;
use crate::live_editor_mailbox::{LiveEditorMailbox, Poll};
use std::{
    io::BufReader,
    sync::Arc,
    thread,
    time::{Duration, Instant},
};

#[derive(Clone, Default)]
pub(crate) struct LiveAcceptance(pub(crate) Option<Arc<LiveEditorMailbox>>);

pub(crate) fn enabled(isolated: bool, explicit: bool) -> Result<LiveAcceptance, String> {
    if !cfg!(all(windows, feature = "live-editor-acceptance-test")) {
        return Ok(LiveAcceptance::default());
    }
    if !isolated || !explicit {
        return Err(
            "Live editor acceptance requires isolated storage and explicit stdio mode".into(),
        );
    }
    Ok(LiveAcceptance(Some(Arc::new(LiveEditorMailbox::default()))))
}

pub(crate) fn run(state: &LiveAcceptance) -> Result<(), String> {
    let Some(mailbox) = state.0.clone() else {
        return Ok(());
    };
    thread::Builder::new()
        .name("nect-live-acceptance-stdio".into())
        .spawn(move || {
            let stdin = std::io::stdin();
            let stdout = std::io::stdout();
            if let Err(error) = serve(BufReader::new(stdin.lock()), stdout.lock(), |packet| {
                let Some(id) = mailbox.enqueue(packet) else {
                    return r#"{"status":"busy"}"#.into();
                };
                let deadline = Instant::now() + Duration::from_secs(5);
                loop {
                    match mailbox.poll(id, Instant::now() >= deadline) {
                        Poll::Pending => thread::sleep(Duration::from_millis(10)),
                        Poll::NotDispatched => return r#"{"status":"not-dispatched"}"#.into(),
                        Poll::OutcomeUnknown | Poll::Missing => {
                            return r#"{"status":"outcome-unknown"}"#.into()
                        }
                        Poll::Complete(receipt) => return receipt,
                    }
                }
            }) {
                eprintln!("Live editor test stream closed: {}", error.kind());
            }
        })
        .map(|_| ())
        .map_err(|error| error.to_string())
}

fn admitted<'a>(
    state: &'a LiveAcceptance,
    label: &str,
) -> Result<&'a LiveEditorMailbox, &'static str> {
    if label != "main" {
        return Err("Live editor test window not admitted");
    }
    state.0.as_deref().ok_or("Live editor test disabled")
}

#[tauri::command]
pub(crate) fn live_acceptance_claim(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
) -> Result<Option<(u64, String)>, &'static str> {
    Ok(admitted(&state, window.label())?.claim())
}

#[tauri::command]
pub(crate) fn live_acceptance_finish(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    id: u64,
    receipt: String,
) -> Result<bool, &'static str> {
    Ok(admitted(&state, window.label())?.finish(id, receipt))
}

#[tauri::command]
pub(crate) fn live_acceptance_fixture(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
) -> Result<&'static str, &'static str> {
    admitted(&state, window.label())?;
    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    return Ok(include_str!("../../examples/blend-distance.json"));
    #[cfg(not(all(windows, feature = "live-editor-acceptance-test")))]
    Err("Live editor test disabled")
}

#[tauri::command]
pub(crate) fn live_acceptance_write_svg(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    storage: tauri::State<'_, crate::acceptance_storage::RunStorage>,
    contents: String,
) -> Result<bool, String> {
    admitted(&state, window.label())?;
    if contents.len() > 4_000_000 || !contents.starts_with("<svg") {
        return Err("Invalid acceptance SVG".into());
    }
    storage.write_acceptance_svg(contents.as_bytes())?;
    Ok(true)
}

#[tauri::command]
pub(crate) fn live_acceptance_save_copy(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    storage: tauri::State<'_, crate::acceptance_storage::RunStorage>,
    contents: String,
) -> Result<bool, String> {
    admitted(&state, window.label())?;
    if contents.len() > 4_000_000 || !contents.trim_start().starts_with('{') {
        return Err("Invalid acceptance project".into());
    }
    storage.write_acceptance_project_copy(contents.as_bytes())?;
    Ok(true)
}
