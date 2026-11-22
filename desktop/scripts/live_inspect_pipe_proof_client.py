"""One-shot, local-only client for the isolated Nect inspection proof.

This test transport never starts an app, discovers endpoints, or loads a user profile.
"""
import _winapi as win
import json
import re
from time import monotonic


def inspect_endpoint(identifier):
    if not re.fullmatch(r"[a-f0-9]{32}", identifier):
        raise ValueError("Invalid local proof identifier")
    return r"\\.\pipe\NectReadProof-" + identifier


def parse_reply(frame):
    reply = json.loads(frame)
    if reply == {"status": "unavailable"}:
        return reply
    if not isinstance(reply, dict) or reply.keys() != {"status", "target"} or reply["status"] != "ok":
        return None
    target = reply["target"]
    ids = ("compositionId", "sessionId")
    nums = ("documentGeneration", "sessionRevision", "projectRevision")
    if not isinstance(target, dict) or target.keys() != set(ids + nums):
        return None
    if any(type(target[k]) is not int or not 0 <= target[k] <= 9007199254740991 for k in nums):
        return None
    if any(not isinstance(target[k], str) or not 1 <= len(target[k].encode("utf-16-le")) // 2 <= 256
           for k in ids):
        return None
    return reply


def inspect_once(endpoint):
    try:
        # No FILE_APPEND_DATA (which also grants pipe instance creation).
        # SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION denies impersonation.
        handle = win.CreateFile(endpoint, 0x00120083, 0, 0, win.OPEN_EXISTING,
                                win.FILE_FLAG_OVERLAPPED | 0x00110000, 0)
    except OSError as exc:
        return {"status": "app-unavailable" if exc.winerror in (2, 3, 231) else "pipe-denied"}

    deadline = monotonic() + 7

    def complete(overlapped, error):
        pending = error == 997  # ERROR_IO_PENDING
        try:
            if pending:
                wait = win.WaitForSingleObject(
                    overlapped.event, max(0, int((deadline - monotonic()) * 1000))
                )
                if wait != win.WAIT_OBJECT_0:
                    raise TimeoutError
                pending = False  # The event signalled, so I/O has completed.
            elif error:
                raise OSError(error, "Pipe I/O failed")
            count, error = overlapped.GetOverlappedResult(True)
            if error:
                raise OSError(error, "Pipe I/O failed")
            return count
        finally:
            if pending:
                try:
                    overlapped.cancel()
                finally:
                    # Drain before releasing the buffer or OS handle, even when
                    # wait or cancel raises. This may outlast the deadline.
                    overlapped.GetOverlappedResult(True)

    try:
        request = b'{"version":1,"operation":"editor.inspect","arguments":{}}\n'
        if complete(*win.WriteFile(handle, request, overlapped=True)) != len(request):
            return {"status": "outcome-unknown"}
        frame = bytearray()
        while len(frame) < 4097 and monotonic() < deadline:
            op, error = win.ReadFile(handle, 4097 - len(frame), overlapped=True)
            if not complete(op, error):
                break
            frame.extend(op.getbuffer())
            if b"\n" in frame:
                if frame.count(b"\n") != 1 or not frame.endswith(b"\n"):
                    break
                payload = parse_reply(frame)
                if payload is not None:
                    return payload
                break
        return {"status": "outcome-unknown"}
    except (OSError, ValueError):
        return {"status": "outcome-unknown"}
    finally:
        win.CloseHandle(handle)

