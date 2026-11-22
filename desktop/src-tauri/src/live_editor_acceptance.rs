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
    Option<Arc<inspect_channel::Lane>>,
);

#[cfg(all(windows, feature = "live-editor-acceptance-test"))]
mod inspect_channel {
    use crate::live_api_read_admission::{InspectSlot, ReadAdmission, ReadGrant};
    use std::{
        sync::{mpsc, Mutex},
        time::{Duration, Instant},
    };
    use tauri::ipc::Channel;

    pub(super) struct Lane {
        clock: Instant,
        state: Mutex<State>,
    }

    struct State {
        channel: Option<(u64, Channel<serde_json::Value>)>,
        binding_epoch: u64,
        gate: ReadAdmission,
        slot: InspectSlot,
        pending: Option<(u64, ReadGrant, mpsc::SyncSender<String>)>,
    }

    impl Default for Lane {
        fn default() -> Self {
            Self {
                clock: Instant::now(),
                state: Mutex::new(State {
                    channel: None,
                    binding_epoch: 0,
                    // Exactly one synthetic app instance; no production credentials.
                    gate: ReadAdmission::new(1),
                    slot: InspectSlot::default(),
                    pending: None,
                }),
            }
        }
    }

    impl Lane {
        fn now(&self) -> u64 {
            self.clock.elapsed().as_millis() as u64
        }

        pub(super) fn attach(&self, channel: Channel<serde_json::Value>) -> Option<u64> {
            let mut state = self.state.lock().ok()?;
            if state.channel.is_some() {
                return None;
            }
            let epoch = state
                .binding_epoch
                .checked_add(1)
                .filter(|epoch| *epoch <= 9_007_199_254_740_991)?;
            state.gate.enable().ok()?;
            state.gate.set_ready(true);
            state.binding_epoch = epoch;
            state.channel = Some((epoch, channel));
            Some(epoch)
        }

        pub(super) fn detach(&self, epoch: u64) -> bool {
            let Ok(mut state) = self.state.lock() else {
                return false;
            };
            if state.channel.as_ref().map(|(id, _)| *id) != Some(epoch) {
                return false;
            }
            state.gate.disable();
            state.slot.discard();
            state.pending = None;
            state.channel = None;
            true
        }

        pub(super) fn inspect(&self) -> Option<String> {
            // The stdio smoke client starts before the WebView loads its fixture.
            let startup_deadline = Instant::now() + Duration::from_secs(4);
            while self.state.lock().ok()?.channel.is_none() {
                if Instant::now() >= startup_deadline {
                    return None;
                }
                std::thread::sleep(Duration::from_millis(10));
            }
            let (tx, rx) = mpsc::sync_channel(1);
            let request = {
                let mut state = self.state.lock().ok()?;
                let State {
                    channel,
                    gate,
                    slot,
                    pending,
                    ..
                } = &mut *state;
                if pending.is_some() {
                    return None;
                }
                let (_, channel) = channel.as_ref()?;
                let now = self.now();
                let grant = gate.issue(now, now + 4000).ok()?;
                let prepare = slot.prepare(gate, &grant, now).ok()?;
                let publish = slot.publish(gate, &grant, now, prepare.request_id).ok()?;
                let request = prepare.request_id;
                if channel
                    .send(serde_json::json!(prepare))
                    .and_then(|_| channel.send(serde_json::json!(publish)))
                    .is_err()
                {
                    if let Some(discard) = slot.discard() {
                        let _ = channel.send(serde_json::json!(discard));
                    }
                    return None;
                }
                *pending = Some((request, grant, tx));
                request
            };
            let reply = rx.recv_timeout(Duration::from_secs(4)).ok();
            if reply.is_none() {
                let mut state = self.state.lock().ok()?;
                if state.pending.as_ref().map(|(id, _, _)| *id) == Some(request) {
                    state.pending = None;
                    if let Some(discard) = state.slot.discard() {
                        if let Some((_, channel)) = &state.channel {
                            let _ = channel.send(serde_json::json!(discard));
                        }
                    }
                }
            }
            reply
        }

        pub(super) fn respond(&self, epoch: u64, id: u64, receipt: &str) -> bool {
            let Ok(mut state) = self.state.lock() else {
                return false;
            };
            let State {
                channel,
                gate,
                slot,
                pending,
                ..
            } = &mut *state;
            if channel.as_ref().map(|(id, _)| *id) != Some(epoch) {
                return false;
            }
            let Some((_, grant, _)) = pending.as_ref() else {
                return false;
            };
            let Ok(reply) = slot.receive(gate, grant, self.now(), id, receipt.as_bytes()) else {
                return false;
            };
            let Ok(serialized) = serde_json::to_string(&reply) else {
                return false;
            };
            pending
                .take()
                .is_some_and(|(_, _, sender)| sender.try_send(serialized).is_ok())
        }
    }

    #[cfg(test)]
    mod tests {
        use super::*;
        use tauri::ipc::InvokeResponseBody;

        fn capture() -> (
            Channel<serde_json::Value>,
            mpsc::Receiver<serde_json::Value>,
        ) {
            let (tx, rx) = mpsc::channel();
            let channel = Channel::new(move |body| {
                if let InvokeResponseBody::Json(json) = body {
                    tx.send(serde_json::from_str(&json).unwrap()).unwrap();
                }
                Ok(())
            });
            (channel, rx)
        }

        fn published(rx: &mpsc::Receiver<serde_json::Value>) -> u64 {
            let prepare = rx.recv_timeout(Duration::from_secs(2)).unwrap();
            let publish = rx.recv_timeout(Duration::from_secs(2)).unwrap();
            assert_eq!(prepare["phase"], "prepare");
            assert_eq!(publish["phase"], "publish");
            assert_eq!(prepare["requestId"], publish["requestId"]);
            publish["requestId"].as_u64().unwrap()
        }

        #[test]
        fn closes_pending_and_rebind_ignores_old_epoch_and_receipts() {
            let lane = Lane::default();
            let (first, frames) = capture();
            let old_epoch = lane.attach(first).unwrap();
            let (second, new_frames) = capture();
            const EMPTY: &str = r#"{"status":"unavailable"}"#;
            std::thread::scope(|scope| {
                let old_reply = scope.spawn(|| lane.inspect());
                let old_id = published(&frames);
                assert!(lane.detach(old_epoch));
                let new_epoch = lane.attach(second).unwrap();
                assert_ne!(new_epoch, old_epoch);
                let new_reply = scope.spawn(|| lane.inspect());
                let id = published(&new_frames);
                assert_ne!(id, old_id);
                assert!(old_reply.join().unwrap().is_none());
                assert!(!lane.detach(old_epoch));
                assert!(!lane.respond(old_epoch, id, EMPTY));
                assert!(!lane.respond(new_epoch, old_id, EMPTY));
                assert!(lane.respond(new_epoch, id, EMPTY));
                assert!(!lane.respond(new_epoch, id, EMPTY));
                assert_eq!(new_reply.join().unwrap().as_deref(), Some(EMPTY));
                assert!(lane.detach(new_epoch));
            });
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
        Some(Arc::new(inspect_channel::Lane::default())),
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
