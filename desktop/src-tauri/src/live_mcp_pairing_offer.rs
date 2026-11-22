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
