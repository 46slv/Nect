use std::io::{self, BufRead, Write};

const LIMIT: usize = 4_096;

// The owner supplies the streams; this module never opens a listener or picks
// the caller or an app instance. A partial frame is never dispatched.
pub(crate) fn serve<R: BufRead, W: Write>(
    mut requests: R,
    mut replies: W,
    mut handle: impl FnMut(String) -> String,
) -> io::Result<()> {
    let mut frame = Vec::with_capacity(LIMIT);
    loop {
        let bytes = requests.fill_buf()?;
        if bytes.is_empty() {
            return if frame.is_empty() {
                Ok(())
            } else {
                Err(io::Error::new(
                    io::ErrorKind::UnexpectedEof,
                    "partial request",
                ))
            };
        }
        let newline = bytes.iter().position(|&byte| byte == b'\n');
        let size = newline.unwrap_or(bytes.len());
        if frame.len() + size > LIMIT {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "request too large",
            ));
        }
        frame.extend_from_slice(&bytes[..size]);
        let consumed = size + usize::from(newline.is_some());
        requests.consume(consumed);
        if newline.is_none() {
            continue;
        }
        if frame.last() == Some(&b'\r') {
            frame.pop();
        }
        let packet = String::from_utf8(std::mem::take(&mut frame))
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "invalid request encoding"))?;
        let reply = handle(packet);
        if reply.is_empty() || reply.len() > LIMIT || reply.contains(['\r', '\n']) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "invalid reply frame",
            ));
        }
        replies.write_all(reply.as_bytes())?;
        replies.write_all(b"\n")?;
        replies.flush()?;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{BufReader, Cursor};
    #[test]
    fn handles_fragmented_and_back_to_back_frames_in_order() {
        let mut seen = Vec::new();
        let mut output = Vec::new();
        serve(
            BufReader::with_capacity(2, Cursor::new(b"a\r\nhello\n")),
            &mut output,
            |packet| {
                seen.push(packet);
                "{\"status\":\"ok\"}".to_string()
            },
        )
        .unwrap();
        assert_eq!(seen, ["a", "hello"]);
        assert_eq!(output, b"{\"status\":\"ok\"}\n{\"status\":\"ok\"}\n");
    }
    #[test]
    fn rejects_partial_oversized_and_invalid_utf8_before_dispatch() {
        for packet in [
            b"incomplete".to_vec(),
            vec![b'x'; LIMIT + 1],
            vec![0xff, b'\n'],
        ] {
            let mut output = Vec::new();
            assert!(serve(Cursor::new(packet), &mut output, |_| panic!(
                "must not dispatch"
            ))
            .is_err());
            assert!(output.is_empty());
        }
    }
    #[test]
    fn rejects_reply_that_crosses_frame_boundary() {
        let mut output = Vec::new();
        assert!(serve(Cursor::new(b"{}\n"), &mut output, |_| "one\ntwo".into()).is_err());
        assert!(output.is_empty());
    }
}
