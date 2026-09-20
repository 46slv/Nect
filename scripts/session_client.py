"""Client of the desktop-owned Session. No document state or mutation logic here."""
import json
import os
import socket
import time

LIMIT = 8 * 1024 * 1024


def call(endpoint, request, timeout=10):
    payload = json.dumps(request, ensure_ascii=False, allow_nan=False).encode('utf-8') + b'\n'
    if len(payload) > LIMIT:
        raise ValueError('Request exceeds 8 MiB')
    if os.name != 'nt':
        with socket.socket(socket.AF_UNIX) as channel:
            channel.settimeout(timeout)
            channel.connect(endpoint)
            channel.sendall(payload)
            response = bytearray()
            while b'\n' not in response:
                block = channel.recv(65536)
                if not block:
                    raise ConnectionError('Desktop disconnected; mutation outcome may be unknown')
                response.extend(block)
                if len(response) > LIMIT:
                    raise ValueError('Response exceeds 8 MiB')
    else:
        import ctypes
        import msvcrt
        from ctypes import wintypes
        peek = ctypes.WinDLL('kernel32', use_last_error=True).PeekNamedPipe
        peek.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                         wintypes.LPVOID, ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
        peek.restype = wintypes.BOOL
        path = endpoint if endpoint.startswith('\\\\.\\pipe\\') else '\\\\.\\pipe\\' + endpoint
        with open(path, 'r+b', buffering=0) as channel:
            channel.write(payload)
            deadline = time.monotonic() + timeout
            response = bytearray()
            while b'\n' not in response:
                available = wintypes.DWORD()
                if not peek(msvcrt.get_osfhandle(channel.fileno()), None, 0, None,
                            ctypes.byref(available), None):
                    raise ConnectionError('Desktop disconnected; mutation outcome may be unknown')
                if available.value:
                    response.extend(channel.read(min(available.value, 65536)))
                elif time.monotonic() >= deadline:
                    raise TimeoutError('Session response timed out; do not retry a mutation without readback')
                else:
                    time.sleep(0.005)
                if len(response) > LIMIT:
                    raise ValueError('Response exceeds 8 MiB')
    return json.loads(response.split(b'\n', 1)[0])
