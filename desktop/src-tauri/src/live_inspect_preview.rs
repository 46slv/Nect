//! Read-only diagnostics for an opt-in application build. No startup arm,
//! external listener, or persisted settings. The normal preview UI only gets a
//! boolean. A disposable build may serve a single read on inherited handles.
use serde_json::Value;
use tauri::{ipc::Channel, State, WebviewWindow};

#[derive(Default)]
pub(crate) struct Preview {
    #[cfg(all(windows, feature = "live-inspect-preview"))]
    lane: std::sync::Arc<crate::live_inspect_lane::LiveInspectLane>,
    #[cfg(all(windows, feature = "live-inspect-preview"))]
    pipe: pipe_proof::State,
}

#[cfg(all(windows, feature = "live-inspect-preview"))]
mod pipe_proof;

impl Preview {
    pub(crate) fn configured(
        request: Option<std::ffi::OsString>,
        isolated: bool,
        other_proof: bool,
    ) -> Result<Self, &'static str> {
        let Some(request) = request else {
            return Ok(Self::default());
        };
        if !cfg!(all(windows, feature = "live-inspect-preview")) || !isolated || other_proof {
            return Err("Inspection pipe proof requires an isolated Windows preview");
        }
        #[cfg(all(windows, feature = "live-inspect-preview"))]
        return Ok(Self {
            pipe: pipe_proof::State::from_request(request)?,
            ..Self::default()
        });
        #[cfg(not(all(windows, feature = "live-inspect-preview")))]
        {
            let _ = request;
            Err("Inspection pipe proof unavailable")
        }
    }
    pub(crate) fn require_disposable_stdio(
        requested: bool,
        isolated: bool,
        other_stdio: bool,
    ) -> Result<(), &'static str> {
        if !requested {
            return Ok(());
        }
        if !cfg!(all(windows, feature = "live-inspect-preview")) || !isolated || other_stdio {
            return Err("Inspection stdio requires a disposable preview and exclusive handles");
        }
        Ok(())
    }

    pub(crate) fn run_disposable_stdio(&self) -> Result<(), String> {
        #[cfg(all(windows, feature = "live-inspect-preview"))]
        {
            use crate::live_editor_frames::serve_one;
            let lane = self.lane.clone();
            return std::thread::Builder::new()
                .name("nect-preview-read-proof".into())
                .spawn(move || {
                    let stdin = std::io::stdin();
                    let stdout = std::io::stdout();
                    let _ = serve_one(
                        std::io::BufReader::new(stdin.lock()),
                        stdout.lock(),
                        |packet, writer| read_once(&lane, &packet, writer),
                    );
                })
                .map(|_| ())
                .map_err(|e| e.to_string());
        }
        #[cfg(not(all(windows, feature = "live-inspect-preview")))]
        Err("Inspection stdio unavailable".into())
    }

    pub(crate) fn revoke(&self) {
        #[cfg(all(windows, feature = "live-inspect-preview"))]
        {
            self.pipe.close(None, || self.lane.revoke());
        }
    }

    fn admit(window: &WebviewWindow) -> Result<(), &'static str> {
        if !cfg!(all(windows, feature = "live-inspect-preview")) || window.label() != "main" {
            return Err("Local inspection preview unavailable");
        }
        Ok(())
    }
}

#[cfg(all(windows, feature = "live-inspect-preview"))]
fn read_once(
    lane: &crate::live_inspect_lane::LiveInspectLane,
    packet: &str,
    writer: &mut impl std::io::Write,
) -> std::io::Result<()> {
    read_once_at(lane, packet, lane.current_epoch(), writer)
}

#[cfg(all(windows, feature = "live-inspect-preview"))]
fn read_once_at(
    lane: &crate::live_inspect_lane::LiveInspectLane,
    packet: &str,
    epoch: Option<u64>,
    writer: &mut impl std::io::Write,
) -> std::io::Result<()> {
    use crate::live_editor_frames::write_frame;
    let contract = serde_json::json!({"version":1,"operation":"editor.inspect","arguments":{}});
    if serde_json::from_str::<Value>(packet).ok().as_ref() != Some(&contract) {
        return write_frame(writer, r#"{"status":"unsupported-operation"}"#);
    }
    if let Some(epoch) = epoch {
        if let Some(reply) = lane.inspect_now(Some(epoch)) {
            if let Some(outcome) = lane.publish_for_epoch(epoch, || write_frame(writer, &reply)) {
                return outcome;
            }
        }
    }
    write_frame(writer, r#"{"status":"unavailable"}"#)
}

#[tauri::command]
pub(crate) fn live_preview_available(window: WebviewWindow) -> bool {
    Preview::admit(&window).is_ok()
}

#[tauri::command]
pub(crate) fn live_preview_attach(
    window: WebviewWindow,
    state: State<'_, Preview>,
    channel: Channel<Value>,
) -> Result<Option<u64>, &'static str> {
    Preview::admit(&window)?;
    #[cfg(all(windows, feature = "live-inspect-preview"))]
    {
        let Some(epoch) = state.lane.enable_and_attach(channel) else {
            return Ok(None);
        };
        if let Err(error) = state.pipe.begin(state.lane.clone(), epoch) {
            state.lane.detach(epoch);
            return Err(error);
        }
        return Ok(Some(epoch));
    }
    #[cfg(not(all(windows, feature = "live-inspect-preview")))]
    {
        let _ = (state, channel);
        Err("Local inspection preview unavailable")
    }
}

#[tauri::command]
pub(crate) fn live_preview_reply(
    window: WebviewWindow,
    state: State<'_, Preview>,
    epoch: u64,
    id: u64,
    receipt: String,
) -> Result<bool, &'static str> {
    Preview::admit(&window)?;
    #[cfg(all(windows, feature = "live-inspect-preview"))]
    return Ok(state.lane.respond(epoch, id, &receipt));
    #[cfg(not(all(windows, feature = "live-inspect-preview")))]
    {
        let _ = (state, epoch, id, receipt);
        Err("Local inspection preview unavailable")
    }
}

#[tauri::command]
pub(crate) async fn live_preview_read(
    window: WebviewWindow,
    state: State<'_, Preview>,
    epoch: u64,
) -> Result<bool, &'static str> {
    Preview::admit(&window)?;
    #[cfg(all(windows, feature = "live-inspect-preview"))]
    {
        let lane = state.lane.clone();
        return tauri::async_runtime::spawn_blocking(move || {
            if !lane.is_current(epoch) {
                return false;
            }
            let available = lane
                .inspect_now(Some(epoch))
                .and_then(|r| serde_json::from_str::<Value>(&r).ok())
                .is_some_and(|r| r["status"] == "ok");
            available && lane.is_current(epoch)
        })
        .await
        .map_err(|_| "Local inspection preview unavailable");
    }
    #[cfg(not(all(windows, feature = "live-inspect-preview")))]
    {
        let _ = (state, epoch);
        Err("Local inspection preview unavailable")
    }
}

#[tauri::command]
pub(crate) fn live_preview_detach(
    window: WebviewWindow,
    state: State<'_, Preview>,
    epoch: u64,
) -> Result<bool, &'static str> {
    Preview::admit(&window)?;
    #[cfg(all(windows, feature = "live-inspect-preview"))]
    {
        return Ok(state.pipe.close(Some(epoch), || state.lane.detach(epoch)));
    }
    #[cfg(not(all(windows, feature = "live-inspect-preview")))]
    {
        let _ = (state, epoch);
        Err("Local inspection preview unavailable")
    }
}

#[cfg(all(test, windows, feature = "live-inspect-preview"))]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[test]
    fn disposable_stdio_is_opt_in_and_exclusive() {
        assert!(Preview::require_disposable_stdio(false, false, true).is_ok());
        assert!(Preview::require_disposable_stdio(true, true, false).is_ok());
        assert!(Preview::require_disposable_stdio(true, false, false).is_err());
        assert!(Preview::require_disposable_stdio(true, true, true).is_err());
    }

    #[test]
    fn exactly_one_frame_and_unarmed_read_unavailable() {
        let lane = crate::live_inspect_lane::LiveInspectLane::default();
        let input = b"{\"version\":1,\"operation\":\"editor.inspect\",\"arguments\":{}}\nignored\n";
        let mut output = vec![];
        crate::live_editor_frames::serve_one(Cursor::new(input), &mut output, |packet, writer| {
            read_once(&lane, &packet, writer)
        })
        .unwrap();
        assert_eq!(output, b"{\"status\":\"unavailable\"}\n");
        output.clear();
        read_once(
            &lane,
            r#"{"version":1,"operation":"artwork.list","arguments":{}}"#,
            &mut output,
        )
        .unwrap();
        assert_eq!(output, b"{\"status\":\"unsupported-operation\"}\n");
    }
}
