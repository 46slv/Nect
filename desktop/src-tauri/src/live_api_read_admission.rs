//! Testable admission state for the first normal desktop read-only API slice.
//! Does not authenticate processes or own a listener. The native transport
//! must authorize the client before calling issue, and own expiry/cleanup.

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum Admission {
    Disabled,
    Unavailable,
    NotGranted,
    Expired,
    Exhausted,
}

#[derive(Debug)]
pub(crate) struct ReadGrant {
    instance: u128,
    generation: u64,
    deadline: u64,
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
