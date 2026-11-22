//! Format for an explicitly selected, untrusted local MCP pairing offer.
//! This parser does not perform I/O, contact a peer, or authorize anything.
use serde::Deserialize;

const MAX_OFFER_BYTES: usize = 512;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct WireOffer {
    version: u32,
    kind: String,
    ticket: String,
}

pub(crate) struct LocalPairingOffer {
    // At most a rendezvous identifier. Do not display or log it; do not use
    // possession as authority to access the editor.
    ticket: String,
}

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum InvalidOffer {
    Frame,
    Schema,
}

impl LocalPairingOffer {
    pub(crate) fn decode(bytes: &[u8]) -> Result<Self, InvalidOffer> {
        if bytes.is_empty() || bytes.len() > MAX_OFFER_BYTES {
            return Err(InvalidOffer::Frame);
        }
        let wire: WireOffer = serde_json::from_slice(bytes).map_err(|_| InvalidOffer::Schema)?;
        if wire.version != 1
            || wire.kind != "nect-local-read-pairing"
            || wire.ticket.len() != 32
            || !wire
                .ticket
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        {
            return Err(InvalidOffer::Schema);
        }
        Ok(Self {
            ticket: wire.ticket,
        })
    }

    pub(crate) fn ticket(&self) -> &str {
        &self.ticket
    }
}

#[cfg(all(windows, feature = "live-mcp-pairing"))]
pub(crate) fn windows_seed() -> Result<(String, LocalPairingOffer), InvalidOffer> {
    extern "C" {
        fn nect_pairing_seed(sid: *mut u16, capacity: u32, nonce: *mut u8) -> i32;
    }
    let mut wide_sid = [0u16; 128];
    let mut nonce = [0u8; 16];
    // The native routine accepts the provided buffers only and never retains them.
    let ready = unsafe {
        nect_pairing_seed(
            wide_sid.as_mut_ptr(),
            wide_sid.len() as u32,
            nonce.as_mut_ptr(),
        )
    };
    if ready != 1 {
        return Err(InvalidOffer::Frame);
    }
    let end = wide_sid
        .iter()
        .position(|&u| u == 0)
        .ok_or(InvalidOffer::Schema)?;
    let sid = String::from_utf16(&wide_sid[..end]).map_err(|_| InvalidOffer::Schema)?;
    let logon = sid.strip_prefix("S-1-5-5-").ok_or(InvalidOffer::Schema)?;
    let mut fields = logon.split('-');
    let decimal_u32 = |s: &str| {
        !s.is_empty() && s.bytes().all(|b| b.is_ascii_digit()) && s.parse::<u32>().is_ok()
    };
    if !fields.next().is_some_and(decimal_u32)
        || !fields.next().is_some_and(decimal_u32)
        || fields.next().is_some()
    {
        return Err(InvalidOffer::Schema);
    }
    const HEX: &[u8] = b"0123456789abcdef";
    let mut ticket = String::with_capacity(32);
    for byte in nonce {
        ticket.push(char::from(HEX[(byte >> 4) as usize]));
        ticket.push(char::from(HEX[(byte & 15) as usize]));
    }
    Ok((sid, LocalPairingOffer { ticket }))
}

#[cfg(test)]
mod tests {
    use super::*;

    const TICKET: &str = "1234567890abcdef1234567890abcdef";

    fn valid() -> String {
        format!(r#"{{"version":1,"kind":"nect-local-read-pairing","ticket":"{TICKET}"}}"#)
    }

    #[test]
    fn decodes_only_the_rendezvous_identity() {
        let text = valid();
        assert_eq!(
            LocalPairingOffer::decode(text.as_bytes()).unwrap().ticket(),
            TICKET
        );
    }

    #[test]
    fn rejects_malformed_stacked_or_oversized_frames() {
        let text = valid();
        for bytes in [
            Vec::new(),
            vec![b' '; MAX_OFFER_BYTES + 1],
            b"{}".to_vec(),
            vec![0xff],
            format!("{text}{text}").into_bytes(),
        ] {
            assert!(LocalPairingOffer::decode(&bytes).is_err());
        }
    }

    #[test]
    fn rejects_unknown_claims_and_ambiguous_versions() {
        let text = valid();
        for changed in [
            text.replacen(r#""version":1"#, r#""version":0"#, 1),
            text.replacen(r#""version":1"#, r#""version":1,"version":1"#, 1),
            text.replacen(r#""version":1"#, r#""version":1,"pid":123"#, 1),
            text.replacen(r#""version":1"#, r#""version":1,"scope":"write""#, 1),
            text.replace("nect-local-read-pairing", "unregistered"),
            text.replace(TICKET, "1234567890ABCDEF1234567890ABCDEF"),
            text.replace(TICKET, "other"),
        ] {
            assert_eq!(
                LocalPairingOffer::decode(changed.as_bytes()).err(),
                Some(InvalidOffer::Schema)
            );
        }
    }
}

#[cfg(all(windows, feature = "live-mcp-pairing"))]
pub(crate) fn write_selected_offer(
    owned_root: &std::path::Path,
    logon_sid: &str,
    offer: &LocalPairingOffer,
) -> std::io::Result<std::fs::File> {
    use std::{
        ffi::c_void,
        io::Write,
        os::windows::{
            ffi::OsStrExt,
            io::{FromRawHandle, RawHandle},
        },
    };
    extern "C" {
        fn nect_open_private_offer(path: *const u16, sid: *const u16) -> *mut c_void;
    }
    // Only the OS-derived SID is supplied by the current caller. The offer
    // itself cannot choose a location or principal.
    let path = owned_root.join("pending.nect-pair");
    let path: Vec<u16> = path.as_os_str().encode_wide().chain(Some(0)).collect();
    let sid: Vec<u16> = logon_sid.encode_utf16().chain(Some(0)).collect();
    let raw = unsafe { nect_open_private_offer(path.as_ptr(), sid.as_ptr()) };
    if raw.is_null() || raw as isize == -1 {
        return Err(std::io::Error::last_os_error());
    }
    let mut output = unsafe { std::fs::File::from_raw_handle(raw as RawHandle) };
    let body = serde_json::json!({
        "version": 1,
        "kind": "nect-local-read-pairing",
        "ticket": offer.ticket()
    });
    output.write_all(body.to_string().as_bytes())?;
    output.sync_all()?;
    Ok(output)
}

#[cfg(all(test, windows, feature = "live-mcp-pairing"))]
#[test]
fn windows_offer_is_readable_only_after_creation_and_owned_by_its_handle() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("pending.nect-pair");
    assert!(!path.exists());
    let (sid, offer) = windows_seed().unwrap();
    let lease = write_selected_offer(dir.path(), &sid, &offer).unwrap();
    let loaded = LocalPairingOffer::decode(&std::fs::read(&path).unwrap()).unwrap();
    assert!(loaded.ticket() == offer.ticket());
    assert!(write_selected_offer(dir.path(), &sid, &offer).is_err());
    drop(lease);
    assert!(
        !path.exists(),
        "Offer remains after its owner's handle is closed"
    );
}

#[cfg(all(test, windows, feature = "live-mcp-pairing"))]
#[test]
fn windows_token_supplies_a_logon_sid_and_fresh_system_entropy() {
    let (first_sid, first) = windows_seed().unwrap();
    let (second_sid, second) = windows_seed().unwrap();
    assert!(
        first_sid == second_sid,
        "The host logon ID changed within a process"
    );
    assert!(
        first.ticket() != second.ticket(),
        "The rendezvous draw was repeated"
    );
}
