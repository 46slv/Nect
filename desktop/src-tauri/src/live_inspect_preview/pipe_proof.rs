use crate::live_inspect_lane::LiveInspectLane;
use crate::live_inspect_windows_pipe::{
    endpoint_name, logon_sid, IoWindow, LocalReadPipe, SelectedClient,
};
use std::{
    io::{self, BufReader, Read, Write},
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
    time::{Duration, Instant},
};

struct Proof {
    id: String,
    logon_sid: String,
    client: SelectedClient,
}

#[derive(Default)]
pub(super) struct State {
    prepared: Mutex<Option<Proof>>,
    active: Mutex<Option<(u64, Arc<AtomicBool>)>>,
}

impl State {
    pub(super) fn from_request(request: std::ffi::OsString) -> Result<Self, &'static str> {
        let request = request.to_str().ok_or("Invalid inspection proof")?;
        let mut fields = request.split(',');
        let id = fields
            .next()
            .filter(|s| endpoint_name(s).is_some())
            .ok_or("Invalid proof identity")?;
        let sid = fields
            .next()
            .filter(|s| logon_sid(s))
            .ok_or("Invalid proof account")?;
        let pid = fields
            .next()
            .and_then(|s| s.parse::<u32>().ok())
            .filter(|&p| p != 0 && p != std::process::id())
            .ok_or("Invalid proof client")?;
        if fields.next().is_some() {
            return Err("Invalid inspection proof");
        }
        let client =
            SelectedClient::retain(pid).map_err(|_| "Inspection proof client unavailable")?;
        if !client.live() {
            return Err("Inspection proof client unavailable");
        }
        Ok(Self {
            prepared: Mutex::new(Some(Proof {
                id: id.into(),
                logon_sid: sid.into(),
                client,
            })),
            ..Self::default()
        })
    }

    pub(super) fn close<T>(&self, epoch: Option<u64>, close_binding: impl FnOnce() -> T) -> T {
        // Same order as begin (active then lane). Signal cancellation before
        // waiting on the lane, while excluding a new old-epoch worker.
        let mut active = self
            .active
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        if active
            .as_ref()
            .is_some_and(|(current, _)| epoch.is_none_or(|id| id == *current))
        {
            if let Some((_, cancelled)) = active.take() {
                cancelled.store(true, Ordering::Release);
            }
        }
        let result = close_binding();
        drop(active);
        result
    }

    pub(super) fn begin(&self, lane: Arc<LiveInspectLane>, epoch: u64) -> Result<(), &'static str> {
        let mut active = self
            .active
            .lock()
            .map_err(|_| "Inspection proof unavailable")?;
        if !lane.is_current(epoch) {
            return Err("Inspection binding closed");
        }
        let Some(proof) = self
            .prepared
            .lock()
            .map_err(|_| "Inspection proof unavailable")?
            .take()
        else {
            return Ok(());
        };
        let pipe = LocalReadPipe::create(&proof.id, &proof.logon_sid)
            .map_err(|_| "Inspection pipe unavailable")?;
        let cancelled = Arc::new(AtomicBool::new(false));
        *active = Some((epoch, cancelled.clone()));
        let result = std::thread::Builder::new()
            .name("nect-inspect-pipe-proof".into())
            .spawn(move || {
                let window = IoWindow {
                    deadline: Instant::now() + Duration::from_secs(12),
                    revoked: &cancelled,
                };
                let Ok(pipe) = pipe.attach(&proof.client, &window) else {
                    return;
                };
                let read = |buf: &mut [u8]| pipe.read(buf, &window);
                let write = |buf: &[u8]| pipe.write(buf, &window);
                let _ = crate::live_editor_frames::serve_one(
                    BufReader::with_capacity(4096, Reader(read)),
                    Writer(write),
                    |packet, writer| super::read_once_at(&lane, &packet, Some(epoch), writer),
                );
            });
        if result.is_err() {
            *active = None;
            return Err("Inspection proof worker unavailable");
        }
        Ok(())
    }
}

struct Reader<F>(F);
impl<F: FnMut(&mut [u8]) -> io::Result<usize>> Read for Reader<F> {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        (self.0)(buffer)
    }
}
struct Writer<F>(F);
impl<F: FnMut(&[u8]) -> io::Result<usize>> Write for Writer<F> {
    fn write(&mut self, buffer: &[u8]) -> io::Result<usize> {
        (self.0)(buffer).map_err(|error| {
            if error.kind() == io::ErrorKind::Interrupted {
                io::Error::from(io::ErrorKind::ConnectionAborted)
            } else {
                error
            }
        })
    }
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn close_signals_before_invalidating_binding_and_ignores_previous_epoch() {
        let state = State::default();
        let stop = Arc::new(AtomicBool::new(false));
        *state.active.lock().unwrap() = Some((7, stop.clone()));
        state.close(Some(6), || assert!(!stop.load(Ordering::Acquire)));
        state.close(Some(7), || {
            assert!(stop.load(Ordering::Acquire));
            assert!(matches!(
                state.active.try_lock(),
                Err(std::sync::TryLockError::WouldBlock)
            ));
        });
        assert!(state.active.lock().unwrap().is_none());
    }
}
