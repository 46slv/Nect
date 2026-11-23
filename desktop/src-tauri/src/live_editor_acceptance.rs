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
pub(crate) struct LiveAcceptance(
    pub(crate) Option<Arc<LiveEditorMailbox>>,
    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    Option<Arc<crate::live_inspect_lane::LiveInspectLane>>,
);

impl LiveAcceptance {
    pub(crate) fn arm_inspection_for_fixture(&self) {
        #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
        if let Some(lane) = &self.1 {
            lane.arm();
        }
    }

    pub(crate) fn revoke_inspection(&self) {
        #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
        if let Some(lane) = &self.1 {
            lane.revoke();
        }
    }
}

pub(crate) fn enabled(isolated: bool, explicit: bool) -> Result<LiveAcceptance, String> {
    if !cfg!(all(windows, feature = "live-editor-acceptance-test")) {
        return Ok(LiveAcceptance::default());
    }
    if !isolated || !explicit {
        return Err(
            "Live editor acceptance requires isolated storage and explicit stdio mode".into(),
        );
    }
    Ok(LiveAcceptance(
        Some(Arc::new(LiveEditorMailbox::default())),
        #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
        Some(Arc::new(
            crate::live_inspect_lane::LiveInspectLane::default(),
        )),
    ))
}

pub(crate) fn run(state: &LiveAcceptance) -> Result<(), String> {
    let Some(mailbox) = state.0.clone() else {
        return Ok(());
    };
    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    let inspect_lane = state.1.clone();
    thread::Builder::new()
        .name("nect-live-acceptance-stdio".into())
        .spawn(move || {
            let stdin = std::io::stdin();
            let stdout = std::io::stdout();
            if let Err(error) = serve(BufReader::new(stdin.lock()), stdout.lock(), |packet| {
                #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
                if is_inspect_packet(&packet) {
                    return inspect_lane
                        .as_ref()
                        .and_then(|lane| lane.inspect())
                        .unwrap_or_else(|| r#"{"status":"unavailable"}"#.into());
                }
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
                        Poll::Complete(receipt) => {
                            return receipt;
                        }
                    }
                }
            }) {
                eprintln!("Live editor test stream closed: {}", error.kind());
            }
        })
        .map(|_| ())
        .map_err(|error| error.to_string())
}

#[cfg(all(windows, feature = "live-editor-acceptance-test"))]
fn is_inspect_packet(packet: &str) -> bool {
    use serde_json::json;
    serde_json::from_str::<serde_json::Value>(packet).ok()
        == Some(json!({"version":1, "operation":"editor.inspect", "arguments":{}}))
}

#[tauri::command]
pub(crate) fn live_acceptance_inspect_attach(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    channel: tauri::ipc::Channel<serde_json::Value>,
) -> Result<Option<u64>, &'static str> {
    admitted(&state, window.label())?;
    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    return Ok(state.1.as_ref().and_then(|lane| lane.attach(channel)));
    #[cfg(not(all(windows, feature = "live-editor-acceptance-test")))]
    {
        let _ = channel;
        Err("Live editor test disabled")
    }
}

#[tauri::command]
pub(crate) fn live_acceptance_inspect_detach(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    epoch: u64,
) -> Result<bool, &'static str> {
    admitted(&state, window.label())?;
    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    return Ok(state.1.as_ref().is_some_and(|lane| lane.detach(epoch)));
    #[cfg(not(all(windows, feature = "live-editor-acceptance-test")))]
    {
        let _ = epoch;
        Err("Live editor test disabled")
    }
}

#[tauri::command]
pub(crate) fn live_acceptance_inspect_reply(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    epoch: u64,
    id: u64,
    receipt: String,
) -> Result<bool, &'static str> {
    admitted(&state, window.label())?;
    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    return Ok(state
        .1
        .as_ref()
        .is_some_and(|lane| lane.respond(epoch, id, &receipt)));
    #[cfg(not(all(windows, feature = "live-editor-acceptance-test")))]
    {
        let _ = (epoch, id, receipt);
        Err("Live editor test disabled")
    }
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
) -> Result<String, &'static str> {
    admitted(&state, window.label())?;
    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    {
        let blend = include_str!("../../examples/blend-distance.json");
        let vector_wire = std::env::var("NECT_ACCEPTANCE_VECTOR_WIRE").as_deref() == Ok("1");
        let scalar_wire = std::env::var("NECT_ACCEPTANCE_SCALAR_WIRE").as_deref() == Ok("1");
        if vector_wire && scalar_wire {
            return Err("Select one graph wire fixture");
        }
        if vector_wire || scalar_wire {
            let mut project: serde_json::Value =
                serde_json::from_str(blend).map_err(|_| "Invalid embedded Blend fixture")?;
            let mut layer: serde_json::Value = serde_json::from_str(include_str!(
                "../../tests/fixtures/live-vector-wire-layer.json"
            ))
            .map_err(|_| "Invalid embedded graph wire fixture")?;
            if scalar_wire {
                let nodes = layer["nodes"].as_array_mut().ok_or("Missing graph nodes")?;
                let target = nodes
                    .iter_mut()
                    .find(|node| node["id"] == "fixture-vector-target")
                    .ok_or("Missing graph wire target")?;
                target["params"]["size"]["source"] = serde_json::json!("fixture-scalar-source");
                target["params"]["size"]["sourcePort"] = serde_json::json!("x");
                nodes.push(serde_json::json!({
                    "id":"fixture-scalar-source", "type":"Separate Vector", "name":"Separate Vector",
                    "position":[200,200], "width":264, "collapsed":false,
                    "params":{"vector":{"type":"Vector2","value":[72,30],"expression":"","locked":false}},
                    "points":[], "operation":"subtract", "edits":[], "text":"Nect", "font":"default"
                }));
            }
            project["layers"]
                .as_array_mut()
                .ok_or("Missing embedded Blend layers")?
                .push(layer);
            return serde_json::to_string(&project)
                .map_err(|_| "Invalid graph wire fixture project");
        }
        if std::env::var("NECT_ACCEPTANCE_LAYER_FOLDERS").as_deref() == Ok("1") {
            let mut project: serde_json::Value =
                serde_json::from_str(blend).map_err(|_| "Invalid embedded Blend fixture")?;
            let layer_id = project["layers"][0]["id"]
                .as_str()
                .ok_or("Missing embedded Blend layer ID")?;
            let editor = serde_json::json!({
                "visible":true,"locked":false,"gridVisible":false,"gridSize":8,
                "snap":false,"snapGuides":true,"snapArtboard":true,"snapGrid":true,"guides":[],
                "layerFolders":[
                {"id":"fixture-folder-parent","name":"Synthetic Parent","members":[]},
                {"id":"fixture-folder-child","name":"Synthetic Child","parentId":"fixture-folder-parent",
                 "members":[{"layerId":layer_id}]}
            ]});
            project["editor"] = editor;
            if std::env::var("NECT_ACCEPTANCE_LAYER_FOLDER_MEMBERSHIP").as_deref() == Ok("1") {
                project["editor"]["layerFolders"] = serde_json::json!([
                    {"id":"fixture-folder-flat","name":"Synthetic Flat","members":[]}
                ]);
            }
            return serde_json::to_string(&project).map_err(|_| "Invalid embedded Folder fixture");
        }
        if std::env::var("NECT_SVG_ACCEPTANCE_JP_FIXTURE").as_deref() != Ok("1") {
            return Ok(blend.into());
        }
        let mut combined: serde_json::Value =
            serde_json::from_str(blend).map_err(|_| "Invalid embedded Blend fixture")?;
        let source: serde_json::Value = serde_json::from_str(include_str!(
            "../../tests/fixtures/project-integrity-native.json"
        ))
        .map_err(|_| "Invalid embedded font fixture")?;
        let font = source["fontAssets"]["font-integrity-fixture"]
            .as_object()
            .ok_or("Missing embedded test font")?;
        let layer = source["compositions"][0]["layers"]
            .as_array()
            .and_then(|layers| layers.iter().find(|l| l["id"] == "integrity-text-layer"))
            .ok_or("Missing embedded text layer")?;
        let project = combined.as_object_mut().ok_or("Invalid test project")?;
        if project.contains_key("fontAssets") {
            return Err("Unexpected fonts in Blend fixture");
        }
        project.insert(
            "fontAssets".into(),
            serde_json::json!({"font-integrity-fixture":font}),
        );
        project
            .get_mut("layers")
            .and_then(serde_json::Value::as_array_mut)
            .ok_or("Missing embedded Blend layers")?
            .push(layer.clone());
        return serde_json::to_string(&combined).map_err(|_| "Invalid combined test fixture");
    }
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

#[tauri::command]
pub(crate) fn live_acceptance_write_png(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    storage: tauri::State<'_, crate::acceptance_storage::RunStorage>,
    bytes: Vec<u8>,
) -> Result<bool, String> {
    admitted(&state, window.label())?;
    if bytes.len() > 4_000_000 || !bytes.starts_with(b"\x89PNG\r\n\x1a\n") {
        return Err("Invalid acceptance PNG".into());
    }
    storage.write_acceptance_png(&bytes)?;
    Ok(true)
}

#[tauri::command]
pub(crate) fn live_acceptance_read_copy(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    storage: tauri::State<'_, crate::acceptance_storage::RunStorage>,
) -> Result<Option<String>, String> {
    admitted(&state, window.label())?;
    storage.read_acceptance_project_copy()
}

#[tauri::command]
pub(crate) fn live_acceptance_save_as(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, LiveAcceptance>,
    storage: tauri::State<'_, crate::acceptance_storage::RunStorage>,
    docs: tauri::State<'_, crate::document_storage::Documents>,
    contents: String,
    operation: u32,
) -> Result<(String, String, u32), String> {
    admitted(&state, window.label())?;
    if contents.len() > 4_000_000 || !contents.trim_start().starts_with('{') {
        return Err("Invalid acceptance project".into());
    }
    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    return crate::document_storage::save_acceptance_bound_project(
        &storage, &docs, contents, operation,
    );
    #[cfg(not(all(windows, feature = "live-editor-acceptance-test")))]
    {
        let _ = (storage, docs, operation);
        Err("Live editor test disabled".into())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normal_build_never_creates_or_admits_a_stdio_mailbox() {
        if cfg!(all(windows, feature = "live-editor-acceptance-test")) {
            return;
        }
        for (isolated, explicit) in [(false, false), (true, false), (false, true), (true, true)] {
            let state = enabled(isolated, explicit).unwrap();
            assert!(state.0.is_none());
            assert!(admitted(&state, "main").is_err());
        }
    }

    #[cfg(all(windows, feature = "live-editor-acceptance-test"))]
    #[test]
    fn acceptance_build_requires_both_opt_ins_and_the_main_window() {
        for (isolated, explicit) in [(false, false), (true, false), (false, true)] {
            assert!(enabled(isolated, explicit).is_err());
        }
        let state = enabled(true, true).unwrap();
        assert!(admitted(&state, "main").is_ok());
        assert!(admitted(&state, "other-window").is_err());
    }
}
