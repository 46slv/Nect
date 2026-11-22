//! Read-only diagnostics for an opt-in application build. No startup arm,
//! external listener, document content, target metadata, or persisted settings.
use serde_json::Value;
use tauri::{ipc::Channel, State, WebviewWindow};

#[derive(Default)]
pub(crate) struct Preview {
    #[cfg(all(windows, feature = "live-inspect-preview"))]
    lane: std::sync::Arc<crate::live_inspect_lane::LiveInspectLane>,
}

impl Preview {
    pub(crate) fn revoke(&self) {
        #[cfg(all(windows, feature = "live-inspect-preview"))]
        self.lane.revoke();
    }

    fn admit(window: &WebviewWindow) -> Result<(), &'static str> {
        if !cfg!(all(windows, feature = "live-inspect-preview")) || window.label() != "main" {
            return Err("Local inspection preview unavailable");
        }
        Ok(())
    }
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
        return Ok(state.lane.enable_and_attach(channel));
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
    return Ok(state.lane.detach(epoch));
    #[cfg(not(all(windows, feature = "live-inspect-preview")))]
    {
        let _ = (state, epoch);
        Err("Local inspection preview unavailable")
    }
}
