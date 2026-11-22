//! Host-owned, in-process lane for one scoped inspection. No listener, persistence,
//! or client authentication; transport authorization must precede use.
use crate::live_api_read_admission::{InspectSlot, ReadAdmission, ReadGrant};
use std::{
    sync::{mpsc, Mutex},
    time::{Duration, Instant},
};
use tauri::ipc::Channel;

pub(crate) struct LiveInspectLane {
    clock: Instant,
    state: Mutex<State>,
}

struct State {
    armed: bool,
    channel: Option<(u64, Channel<serde_json::Value>)>,
    binding_epoch: u64,
    gate: ReadAdmission,
    slot: InspectSlot,
    pending: Option<(u64, ReadGrant, mpsc::SyncSender<String>)>,
}

impl Default for LiveInspectLane {
    fn default() -> Self {
        Self {
            clock: Instant::now(),
            state: Mutex::new(State {
                armed: false,
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

impl LiveInspectLane {
    fn now(&self) -> u64 {
        self.clock.elapsed().as_millis() as u64
    }

    // Called by the host, never by the renderer attach command.
    pub(crate) fn arm(&self) -> bool {
        let Ok(mut state) = self.state.lock() else {
            return false;
        };
        if state.armed || state.gate.enable().is_err() {
            return false;
        }
        state.armed = true;
        true
    }

    pub(crate) fn revoke(&self) {
        if let Ok(mut state) = self.state.lock() {
            Self::close(&mut state);
        }
    }

    fn close(state: &mut State) {
        state.armed = false;
        state.gate.disable();
        state.slot.discard();
        state.pending = None;
        state.channel = None;
    }

    pub(crate) fn attach(&self, channel: Channel<serde_json::Value>) -> Option<u64> {
        let mut state = self.state.lock().ok()?;
        if !state.armed || state.channel.is_some() {
            return None;
        }
        let epoch = state
            .binding_epoch
            .checked_add(1)
            .filter(|epoch| *epoch <= 9_007_199_254_740_991)?;
        state.gate.set_ready(true);
        state.binding_epoch = epoch;
        state.channel = Some((epoch, channel));
        Some(epoch)
    }

    pub(crate) fn detach(&self, epoch: u64) -> bool {
        let Ok(mut state) = self.state.lock() else {
            return false;
        };
        if state.channel.as_ref().map(|(id, _)| *id) != Some(epoch) {
            return false;
        }
        Self::close(&mut state);
        true
    }

    pub(crate) fn inspect(&self) -> Option<String> {
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

    pub(crate) fn respond(&self, epoch: u64, id: u64, receipt: &str) -> bool {
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
        let lane = LiveInspectLane::default();
        let (denied, _) = capture();
        assert!(lane.attach(denied).is_none());
        assert!(lane.arm());
        let (first, frames) = capture();
        let old_epoch = lane.attach(first).unwrap();
        let (second, new_frames) = capture();
        const EMPTY: &str = r#"{"status":"unavailable"}"#;
        std::thread::scope(|scope| {
            let old_reply = scope.spawn(|| lane.inspect());
            let old_id = published(&frames);
            lane.revoke();
            let (denied, _) = capture();
            assert!(lane.attach(denied).is_none());
            assert!(lane.arm());
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
