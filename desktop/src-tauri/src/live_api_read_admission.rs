//! Testable admission state for the first normal desktop read-only API slice.
//! Does not authenticate processes or own a listener. The native transport
//! must authorize the client before calling issue, and own expiry/cleanup.

use serde::{Deserialize, Serialize};

const MAX_INSPECT_BYTES: usize = 4096;
const MAX_JS_REVISION: u64 = 9_007_199_254_740_991;

#[derive(Debug, Deserialize, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub(crate) struct InspectTarget {
    document_generation: u64,
    composition_id: String,
    session_id: String,
    session_revision: u64,
    project_revision: u64,
}

#[derive(Debug, Deserialize, PartialEq, Eq, Serialize)]
#[serde(tag = "status", rename_all = "kebab-case", deny_unknown_fields)]
pub(crate) enum InspectReply {
    Ok { target: InspectTarget },
    Unavailable {},
}

impl InspectReply {
    fn parse(bytes: &[u8]) -> Result<Self, Admission> {
        if bytes.len() > MAX_INSPECT_BYTES {
            return Err(Admission::InvalidRendererReply);
        }
        let reply: Self =
            serde_json::from_slice(bytes).map_err(|_| Admission::InvalidRendererReply)?;
        if let Self::Ok { target } = &reply {
            if [
                target.document_generation,
                target.session_revision,
                target.project_revision,
            ]
            .iter()
            .any(|&n| n > MAX_JS_REVISION)
                || [&target.composition_id, &target.session_id]
                    .iter()
                    .any(|id| id.is_empty() || id.encode_utf16().count() > 256)
            {
                return Err(Admission::InvalidRendererReply);
            }
        }
        Ok(reply)
    }
}

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum Admission {
    Disabled,
    Unavailable,
    NotGranted,
    Expired,
    Exhausted,
    UnsupportedOperation,
    InvalidRendererReply,
}

#[derive(Debug)]
pub(crate) struct ReadGrant {
    instance: u128,
    generation: u64,
    deadline: u64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum ReadOperation {
    Inspect,
    ListArtwork,
}

#[derive(Debug)]
pub(crate) struct PendingRead<'a> {
    grant: &'a ReadGrant,
    pub(crate) operation: ReadOperation,
}

pub(crate) struct ReadAdmission {
    instance: u128,
    generation: u64,
    enabled: bool,
    ready: bool,
}

impl ReadAdmission {
    pub(crate) fn new(instance: u128) -> Self {
        Self {
            instance,
            generation: 0,
            enabled: false,
            ready: false,
        }
    }

    pub(crate) fn enable(&mut self) -> Result<(), Admission> {
        if self.enabled {
            return Ok(());
        }
        self.advance()?;
        self.enabled = true;
        Ok(())
    }

    pub(crate) fn disable(&mut self) {
        self.enabled = false;
        self.ready = false;
    }

    pub(crate) fn set_ready(&mut self, ready: bool) {
        if self.ready && !ready && self.advance().is_err() {
            return;
        }
        self.ready = self.enabled && ready;
    }

    // Host supplies timestamps from the same monotonic clock.
    pub(crate) fn issue(&mut self, now: u64, deadline: u64) -> Result<ReadGrant, Admission> {
        self.check_ready()?;
        if deadline <= now {
            return Err(Admission::Expired);
        }
        self.advance()?;
        Ok(ReadGrant {
            instance: self.instance,
            generation: self.generation,
            deadline,
        })
    }

    pub(crate) fn admit(&self, grant: Option<&ReadGrant>, now: u64) -> Result<(), Admission> {
        self.check_ready()?;
        let Some(grant) = grant else {
            return Err(Admission::NotGranted);
        };
        if grant.instance != self.instance || grant.generation != self.generation {
            return Err(Admission::NotGranted);
        }
        if now >= grant.deadline {
            return Err(Admission::Expired);
        }
        Ok(())
    }

    pub(crate) fn begin<'a>(
        &self,
        grant: Option<&'a ReadGrant>,
        now: u64,
        operation: &str,
    ) -> Result<PendingRead<'a>, Admission> {
        self.admit(grant, now)?;
        let grant = grant.ok_or(Admission::NotGranted)?;
        let operation = match operation {
            "editor.inspect" => ReadOperation::Inspect,
            "artwork.list" => ReadOperation::ListArtwork,
            _ => return Err(Admission::UnsupportedOperation),
        };
        Ok(PendingRead { grant, operation })
    }

    // Call immediately before the host publishes a read result. The real host
    // must serialize this check with the handoff to its connection writer.
    pub(crate) fn finish<T>(
        &self,
        pending: PendingRead<'_>,
        now: u64,
        result: T,
    ) -> Result<T, Admission> {
        self.admit(Some(pending.grant), now)?;
        Ok(result)
    }

    pub(crate) fn finish_inspect(
        &self,
        pending: PendingRead<'_>,
        now: u64,
        bytes: &[u8],
    ) -> Result<InspectReply, Admission> {
        self.admit(Some(pending.grant), now)?;
        if pending.operation != ReadOperation::Inspect {
            return Err(Admission::UnsupportedOperation);
        }
        InspectReply::parse(bytes)
    }

    fn check_ready(&self) -> Result<(), Admission> {
        if !self.enabled {
            return Err(Admission::Disabled);
        }
        if !self.ready {
            return Err(Admission::Unavailable);
        }
        Ok(())
    }

    fn advance(&mut self) -> Result<(), Admission> {
        if let Some(next) = self.generation.checked_add(1) {
            self.generation = next;
            Ok(())
        } else {
            self.disable();
            Err(Admission::Exhausted)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ready(instance: u128) -> ReadAdmission {
        let mut state = ReadAdmission::new(instance);
        state.enable().unwrap();
        state.set_ready(true);
        state
    }

    #[test]
    fn denies_until_enabled_ready_and_owner_issued() {
        let mut state = ReadAdmission::new(1);
        state.set_ready(true);
        assert_eq!(state.issue(3, 5).unwrap_err(), Admission::Disabled);
        state.enable().unwrap();
        assert_eq!(state.admit(None, 3), Err(Admission::Unavailable));
        state.set_ready(true);
        assert_eq!(state.admit(None, 3), Err(Admission::NotGranted));
        let grant = state.issue(3, 5).unwrap();
        assert_eq!(state.admit(Some(&grant), 3), Ok(()));
        assert_eq!(state.admit(Some(&grant), 5), Err(Admission::Expired));
    }

    #[test]
    fn instance_mismatch_and_disable_reenable_do_not_reuse_a_grant() {
        let mut state = ready(1);
        let grant = state.issue(3, 30).unwrap();
        let mut other = ready(2);
        let _other_grant = other.issue(3, 30).unwrap();
        assert_eq!(other.admit(Some(&grant), 4), Err(Admission::NotGranted));
        state.disable();
        assert_eq!(state.admit(Some(&grant), 4), Err(Admission::Disabled));
        state.enable().unwrap();
        state.set_ready(true);
        assert_eq!(state.admit(Some(&grant), 4), Err(Admission::NotGranted));
    }

    #[test]
    fn loss_of_readiness_and_reconnect_require_a_fresh_grant() {
        let mut state = ready(1);
        let old = state.issue(1, 20).unwrap();
        state.set_ready(false);
        assert_eq!(state.admit(Some(&old), 2), Err(Admission::Unavailable));
        state.set_ready(true);
        assert_eq!(state.admit(Some(&old), 2), Err(Admission::NotGranted));
        let new = state.issue(2, 20).unwrap();
        assert_eq!(state.admit(Some(&new), 2), Ok(()));
    }

    #[test]
    fn only_the_two_read_verbs_are_dispatched_and_never_without_a_grant() {
        let mut state = ready(1);
        let grant = state.issue(1, 20).unwrap();
        assert_eq!(
            state.begin(None, 3, "editor.inspect").unwrap_err(),
            Admission::NotGranted
        );
        for denied in ["document.saveAs", "blend.setDistance", "command.dispatch"] {
            assert_eq!(
                state.begin(Some(&grant), 3, denied).unwrap_err(),
                Admission::UnsupportedOperation
            );
        }
        let read = state.begin(Some(&grant), 3, "artwork.list").unwrap();
        assert_eq!(read.operation, ReadOperation::ListArtwork);
        assert_eq!(
            state.finish(read, 4, "fake address list"),
            Ok("fake address list")
        );
    }

    #[test]
    fn queued_results_do_not_survive_connection_loss_or_grant_expiry() {
        let mut state = ready(1);
        let old = state.issue(1, 20).unwrap();
        let from_old = state.begin(Some(&old), 2, "editor.inspect").unwrap();
        state.set_ready(false);
        state.set_ready(true);
        let new = state.issue(3, 10).unwrap();
        assert_eq!(
            state.finish(from_old, 4, "must not return"),
            Err(Admission::NotGranted)
        );
        let pending = state.begin(Some(&new), 9, "editor.inspect").unwrap();
        assert_eq!(
            state.finish(pending, 10, "must not return"),
            Err(Admission::Expired)
        );
    }

    #[test]
    fn inspect_reply_requires_the_closed_bounded_target_and_a_current_grant() {
        let mut state = ready(1);
        let grant = state.issue(1, 20).unwrap();
        let finish = |bytes: &[u8]| {
            state.finish_inspect(
                state.begin(Some(&grant), 3, "editor.inspect").unwrap(),
                4,
                bytes,
            )
        };
        let valid = br#"{"status":"ok","target":{"documentGeneration":2,"compositionId":"c","sessionId":"s","sessionRevision":4,"projectRevision":5}}"#;
        assert_eq!(
            serde_json::to_value(finish(valid).unwrap()).unwrap(),
            serde_json::from_slice::<serde_json::Value>(valid).unwrap()
        );
        assert_eq!(
            finish(br#"{"status":"unavailable"}"#),
            Ok(InspectReply::Unavailable {})
        );
        for invalid in [
            br#"{"status":"unavailable","path":"private"}"#.as_slice(),
            br#"{"status":"ok","target":{"documentGeneration":2,"compositionId":"c","sessionId":"s","sessionRevision":4,"projectRevision":5,"path":"private"}}"#,
            br#"{"status":"ok","target":{"documentGeneration":2,"compositionId":"c","sessionId":"s","sessionRevision":4,"projectRevision":9007199254740992}}"#,
            br#"{"status":"ok","target":{"documentGeneration":2,"compositionId":"","sessionId":"s","sessionRevision":4,"projectRevision":5}}"#,
            br#"{"status":"ok","target":null}"#,
        ] {
            assert_eq!(finish(invalid), Err(Admission::InvalidRendererReply));
        }
        assert_eq!(
            finish(&[b' '; MAX_INSPECT_BYTES + 1]),
            Err(Admission::InvalidRendererReply)
        );
        let wrong_verb = state.begin(Some(&grant), 3, "artwork.list").unwrap();
        assert_eq!(
            state.finish_inspect(wrong_verb, 4, valid),
            Err(Admission::UnsupportedOperation)
        );
        let expired = state.begin(Some(&grant), 3, "editor.inspect").unwrap();
        assert_eq!(
            state.finish_inspect(expired, 20, valid),
            Err(Admission::Expired)
        );
    }

    #[test]
    fn rotation_replaces_the_grant_and_issue_fails_if_already_expired() {
        let mut state = ready(1);
        let old = state.issue(2, 10).unwrap();
        assert_eq!(state.issue(5, 5).unwrap_err(), Admission::Expired);
        assert_eq!(state.admit(Some(&old), 5), Ok(()));
        let new = state.issue(5, 15).unwrap();
        assert_eq!(state.admit(Some(&old), 5), Err(Admission::NotGranted));
        assert_eq!(state.admit(Some(&new), 5), Ok(()));
    }
}
