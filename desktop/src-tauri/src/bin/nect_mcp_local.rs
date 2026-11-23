//! Fixed stdio entry for the opt-in local MCP pairing candidate. No automatic
//! discovery, listener, editor launch, or document access is implemented here.

use serde_json::{json, Value};
use std::io::{self, BufRead, Write};

#[path = "../live_editor_frames.rs"]
mod frames;

#[cfg(windows)]
#[allow(dead_code)]
#[path = "../live_inspect_windows_pipe.rs"]
mod local_pipe;
#[cfg(windows)]
#[allow(dead_code)]
#[path = "../live_mcp_pairing_offer.rs"]
mod pairing_offer;

const REVISION: &str = "2025-06-18";
const INSPECT: &str = "nect_inspect";

#[derive(Default, Eq, PartialEq)]
enum Handshake {
    #[default]
    Fresh,
    Initialized,
    Ready,
}

fn handle(state: &mut Handshake, frame: &str) -> Option<Value> {
    let result = |id: &Value, data: Value| json!({"jsonrpc":"2.0","id":id,"result":data});
    let error = |id: &Value, code: i32, message: &str| json!({"jsonrpc":"2.0","id":id,"error":{"code":code,"message":message}});
    let Ok(request) = serde_json::from_str::<Value>(frame) else {
        return Some(error(&Value::Null, -32700, "Parse error"));
    };
    let Some(method) = request.get("method").and_then(Value::as_str) else {
        return Some(error(&Value::Null, -32600, "Invalid request"));
    };
    if request.get("jsonrpc").and_then(Value::as_str) != Some("2.0") {
        return Some(error(&Value::Null, -32600, "Invalid request"));
    }
    let Some(id) = request.get("id") else {
        if method == "notifications/initialized"
            && *state == Handshake::Initialized
            && request.get("params").is_none_or(Value::is_object)
        {
            *state = Handshake::Ready;
        }
        return None;
    };
    if !(id.is_string() || id.is_i64() || id.is_u64()) {
        return Some(error(&Value::Null, -32600, "Invalid request ID"));
    }
    if request.get("params").is_some_and(|p| !p.is_object()) {
        return Some(error(id, -32602, "Invalid parameters"));
    }
    let params = request.get("params");
    let empty_params = params.is_none_or(|p| p.as_object().is_some_and(|m| m.is_empty()));
    Some(match method {
        "initialize" if *state == Handshake::Fresh => {
            let p = params.unwrap_or(&Value::Null);
            if !p.get("protocolVersion").is_some_and(Value::is_string)
                || !p.get("capabilities").is_some_and(Value::is_object)
                || !p.get("clientInfo").is_some_and(Value::is_object)
            {
                error(id, -32602, "Invalid initialize parameters")
            } else {
                *state = Handshake::Initialized;
                result(
                    id,
                    json!({
                        "protocolVersion": REVISION,
                        "capabilities":{"tools":{"listChanged":false}},
                        "serverInfo":{"name":"nect-local","version":env!("CARGO_PKG_VERSION")}
                    }),
                )
            }
        }
        "ping" if empty_params => result(id, json!({})),
        _ if *state != Handshake::Ready => error(id, -32002, "Initialization required"),
        "tools/list" if empty_params => result(
            id,
            json!({"tools":[{
                "name":INSPECT,
                "description":"Inspect the active document only after local Nect approval",
                "inputSchema":{"type":"object","additionalProperties":false},
                "annotations":{"readOnlyHint":true,"openWorldHint":false}
            }]}),
        ),
        "tools/call" => {
            let p = params.unwrap_or(&Value::Null);
            let args = p.get("arguments");
            if p.get("name").and_then(Value::as_str) != Some(INSPECT)
                || !args.is_none_or(|a| a.as_object().is_some_and(|m| m.is_empty()))
            {
                error(id, -32602, "Unknown tool or invalid arguments")
            } else {
                result(
                    id,
                    json!({"isError":true,
                        "content":[{"type":"text","text":"No local editor has granted access."}],
                        "structuredContent":{"status":"pairing-required"}
                    }),
                )
            }
        }
        "tools/list" | "ping" => error(id, -32602, "Invalid parameters"),
        _ => error(id, -32601, "Method not found"),
    })
}

fn run(mut requests: impl BufRead, mut replies: impl Write) -> io::Result<()> {
    let mut state = Handshake::default();
    while let Some(frame) = frames::read_frame(&mut requests)? {
        if let Some(reply) = handle(&mut state, &frame) {
            frames::write_frame(&mut replies, &reply.to_string())?;
        }
    }
    Ok(())
}

#[cfg(windows)]
struct FixtureWindow {
    stop: std::sync::mpsc::Sender<()>,
    owner: Option<std::thread::JoinHandle<()>>,
}

#[cfg(windows)]
impl FixtureWindow {
    fn start() -> io::Result<Self> {
        // A developer must start a separate process with the exact fixture
        // flag from a controlled working directory. MCP requests cannot open
        // this window. It does not attach or read from any peer.
        let parent = std::env::current_dir()?.canonicalize()?;
        let dir = tempfile::Builder::new()
            .prefix("nect-mcp-pairing-fixture-")
            .tempdir_in(parent)?;
        let (sid, offer) = pairing_offer::windows_seed()
            .map_err(|_| io::Error::other("Windows pairing identity unavailable"))?;
        let idle_pipe = local_pipe::LocalReadPipe::create_pairing(offer.ticket(), &sid)?;
        let offer_file = pairing_offer::write_selected_offer(dir.path(), &sid, &offer)?;
        let (stop, stop_signal) = std::sync::mpsc::channel();
        let owner = std::thread::Builder::new()
            .name("pairing-fixture-owner".into())
            .spawn(move || {
                // A live MCP client may keep stdin open. Revoke independently.
                let _ = stop_signal.recv_timeout(std::time::Duration::from_secs(10));
                drop(offer_file);
                drop(idle_pipe);
                drop(dir);
            })?;
        Ok(Self {
            stop,
            owner: Some(owner),
        })
    }
}

#[cfg(windows)]
impl Drop for FixtureWindow {
    fn drop(&mut self) {
        let _ = self.stop.send(());
        if let Some(owner) = self.owner.take() {
            let _ = owner.join();
        }
    }
}

fn main() -> io::Result<()> {
    let mut args = std::env::args_os().skip(1);
    match (args.next(), args.next()) {
        (None, None) => run(io::stdin().lock(), io::stdout().lock()),
        #[cfg(windows)]
        (Some(flag), None) if flag == "--verify-pairing-window" => {
            let _window = FixtureWindow::start()?;
            run(io::stdin().lock(), io::stdout().lock())
        }
        _ => Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Unsupported mode",
        )),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn send(state: &mut Handshake, method: &str, params: Value) -> Value {
        handle(
            state,
            &json!({"jsonrpc":"2.0","id":1,"method":method,"params":params}).to_string(),
        )
        .unwrap()
    }

    fn initialize(state: &mut Handshake) {
        let init = send(
            state,
            "initialize",
            json!({
                "protocolVersion":REVISION, "capabilities":{},
                "clientInfo":{"name":"fixture","version":"0"}
            }),
        );
        assert_eq!(init["result"]["protocolVersion"], REVISION);
    }

    #[test]
    fn exposes_only_inspect_after_standard_initialize_notification() {
        let mut state = Handshake::Fresh;
        let list = |s: &mut Handshake| send(s, "tools/list", json!({}));
        assert_eq!(list(&mut state)["error"]["code"], -32002);
        initialize(&mut state);
        assert_eq!(list(&mut state)["error"]["code"], -32002);
        assert_eq!(
            handle(
                &mut state,
                r#"{"jsonrpc":"2.0","method":"notifications/initialized"}"#
            ),
            None
        );
        let reply = list(&mut state);
        let tools = reply["result"]["tools"].as_array().unwrap();
        assert_eq!(tools.len(), 1);
        assert_eq!(tools[0]["name"], INSPECT);
    }

    #[test]
    fn inspect_is_closed_and_cannot_return_a_document_without_pairing() {
        let mut state = Handshake::Ready;
        for args in [
            json!({"name":"nect_edit"}),
            json!({"name":INSPECT,"arguments":{"file":"x"}}),
        ] {
            assert_eq!(
                send(&mut state, "tools/call", args)["error"]["code"],
                -32602
            );
        }
        let reply = send(
            &mut state,
            "tools/call",
            json!({"name":INSPECT,"arguments":{}}),
        );
        let output = &reply["result"]["structuredContent"];
        assert_eq!(*output, json!({"status":"pairing-required"}));
        assert_eq!(reply["result"]["isError"], true);
    }
}
