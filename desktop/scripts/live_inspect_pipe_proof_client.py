"""One-shot, local-only client for the isolated Nect inspection proof.

This test transport never starts an app, discovers endpoints, or loads a user profile.
"""
import _winapi as win
import ctypes
from ctypes import wintypes
import json
import os
import re
from time import monotonic

kernel = ctypes.WinDLL("kernel32", use_last_error=True)
pipe_server_pid = kernel.GetNamedPipeServerProcessId
pipe_server_pid.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.ULONG)]
pipe_server_pid.restype = wintypes.BOOL
process_image = kernel.QueryFullProcessImageNameW
process_image.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
                          ctypes.POINTER(wintypes.DWORD)]
process_image.restype = wintypes.BOOL


def retain_expected_server(pipe, expected_exe):
    pid = wintypes.ULONG()
    if not pipe_server_pid(pipe, ctypes.byref(pid)) or pid.value == 0:
        return None
    # Retain the OS process for this operation. Synchronize + query-limited only.
    handle = win.OpenProcess(0x00101000, False, pid.value)
    matched = False
    try:
        path = ctypes.create_unicode_buffer(32768)
        chars = wintypes.DWORD(len(path))
        if not process_image(handle, 0, path, ctypes.byref(chars)):
            return None
        if os.path.normcase(path.value) != os.path.normcase(os.fspath(expected_exe)):
            return None
        matched = win.WaitForSingleObject(handle, 0) == win.WAIT_TIMEOUT
        return handle if matched else None
    finally:
        if not matched:
            win.CloseHandle(handle)


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


def inspect_once(endpoint, expected_exe):
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

    server = None
    try:
        try:
            server = retain_expected_server(handle, expected_exe)
        except OSError:
            return {"status": "server-unverified"}
        if server is None:
            return {"status": "server-unverified"}
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
        if server is not None:
            win.CloseHandle(server)

