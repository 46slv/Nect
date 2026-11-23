use std::sync::Mutex;

// Single-flight mailbox owned by one app instance. Wire authentication and
// framing belong to the local transport, not to this mailbox.
#[derive(Default)]
pub(crate) struct LiveEditorMailbox {
    state: Mutex<State>,
}

#[derive(Default)]
struct State {
    sequence: u64,
    request: Option<(u64, Request)>,
}

enum Request {
    Queued(String),
    Claimed,
    Abandoned,
    UnreadableReceipt,
    Complete(String),
}

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum Poll {
    Pending,
    NotDispatched,
    OutcomeUnknown,
    Complete(String),
    Missing,
}

impl LiveEditorMailbox {
    pub(crate) fn enqueue(&self, packet: String) -> Option<u64> {
        let mut state = self.state.lock().ok()?;
        if state.request.is_some() || packet.is_empty() || packet.len() > 4_096 {
            return None;
        }
        let id = state.sequence.checked_add(1)?;
        state.sequence = id;
        state.request = Some((id, Request::Queued(packet)));
        Some(id)
    }

    pub(crate) fn claim(&self) -> Option<(u64, String)> {
        let mut state = self.state.lock().ok()?;
        let (id, request) = state.request.as_mut()?;
        if !matches!(request, Request::Queued(_)) {
            return None;
        }
        match std::mem::replace(request, Request::Claimed) {
            Request::Queued(packet) => Some((*id, packet)),
            _ => unreachable!(),
        }
    }

    pub(crate) fn finish(&self, id: u64, receipt: String) -> bool {
        let Ok(mut state) = self.state.lock() else {
            return false;
        };
        match state.request.as_mut() {
            Some((current, request)) if id == *current => match request {
                Request::Claimed => {
                    // A renderer reply is bounded independently from its input.
                    if receipt.is_empty() || receipt.len() > 4_096 {
                        *request = Request::UnreadableReceipt;
                        return false;
                    }
                    *request = Request::Complete(receipt);
                    true
                }
                Request::Abandoned => {
                    state.request = None;
                    false
                }
                _ => false,
            },
            _ => false,
        }
    }

    pub(crate) fn poll(&self, id: u64, timeout: bool) -> Poll {
        let Ok(mut state) = self.state.lock() else {
            return Poll::OutcomeUnknown;
        };
        match state.request.as_mut() {
            Some((current, request)) if id == *current => match request {
                Request::Queued(_) if timeout => {
                    state.request = None;
                    Poll::NotDispatched
                }
                Request::Claimed if timeout => {
                    *request = Request::Abandoned;
                    Poll::OutcomeUnknown
                }
                Request::Abandoned => Poll::OutcomeUnknown,
                Request::UnreadableReceipt => {
                    state.request = None;
                    Poll::OutcomeUnknown
                }
                Request::Complete(_) => match state.request.take() {
                    Some((_, Request::Complete(receipt))) => Poll::Complete(receipt),
                    _ => unreachable!(),
                },
                _ => Poll::Pending,
            },
            _ => Poll::Missing,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn expires_before_admission_and_never_claims_expired_packet() {
        let broker = LiveEditorMailbox::default();
        let old = broker.enqueue("old".into()).unwrap();
        assert_eq!(broker.poll(old, true), Poll::NotDispatched);
        let current = broker.enqueue("current".into()).unwrap();
        assert_ne!(old, current);
        assert_eq!(broker.claim(), Some((current, "current".into())));
        assert!(broker.claim().is_none());
        assert!(!broker.finish(old, "wrong".into()));
        assert!(broker.finish(current, "result".into()));
        assert_eq!(broker.poll(current, false), Poll::Complete("result".into()));
    }

    #[test]
    fn timeout_after_claim_never_reports_not_dispatched_or_admits_a_duplicate() {
        let broker = LiveEditorMailbox::default();
        let id = broker.enqueue("edit".into()).unwrap();
        assert!(broker.claim().is_some());
        assert_eq!(broker.poll(id, true), Poll::OutcomeUnknown);
        assert!(broker.enqueue("duplicate".into()).is_none());
        assert!(!broker.finish(id, "late receipt".into()));
        assert!(broker.enqueue("next".into()).is_some());
    }

    #[test]
    fn rejects_large_packet_and_unclaimed_or_unbounded_receipt() {
        let broker = LiveEditorMailbox::default();
        assert!(broker.enqueue("a".repeat(4_097)).is_none());
        let id = broker.enqueue("request".into()).unwrap();
        assert!(!broker.finish(id, "too early".into()));
        assert_eq!(broker.poll(id, false), Poll::Pending);
        assert!(broker.claim().is_some());
        assert!(!broker.finish(id, "a".repeat(4_097)));
        assert_eq!(broker.poll(id, false), Poll::OutcomeUnknown);
        assert!(broker.enqueue("next".into()).is_some());
    }
}
