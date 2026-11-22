# Development-only Windows acceptance. Runs only the supplied unsigned Nect fixture in fresh scratch directories.
# The local pipe is only observed and never used to send payload; the probe must reject the actual process image.
from pathlib import Path
from queue import Queue
from threading import Thread
from tempfile import TemporaryDirectory
import ctypes, json, re, subprocess, sys, time

if not __debug__ or len(sys.argv) != 4 or sys.platform != 'win32':
    raise SystemExit('Usage: python -B verify-local-mcp-refusal.py UNSIGNED_HELPER SCRATCH_DIRECTORY PUBLISHER_PROBE (Windows, no -O)')
exe = Path(sys.argv[1]).resolve(strict=True)
root = Path(sys.argv[2]).resolve(strict=True)
probe_exe = Path(sys.argv[3]).resolve(strict=True)
assert exe.name == 'nect-mcp-local.exe' and root.is_dir() and probe_exe.name == 'verify-pe-publisher.exe'
api = ctypes.WinDLL('kernel32', use_last_error=True)
wait = api.WaitNamedPipeW
wait.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32]
wait.restype = ctypes.c_int

def read_offer(path):
    # The owning handle has write + delete-on-close; share both so it can
    # continue owning the exact object. This changes no filesystem ACL.
    from ctypes import wintypes as w
    create = api.CreateFileW
    create.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,w.LPVOID,w.DWORD,w.DWORD,w.HANDLE]
    create.restype = w.HANDLE
    read = api.ReadFile
    read.argtypes = [w.HANDLE,w.LPVOID,w.DWORD,ctypes.POINTER(w.DWORD),w.LPVOID]
    read.restype = w.BOOL
    close = api.CloseHandle
    close.argtypes = [w.HANDLE]
    close.restype = w.BOOL
    handle = create(str(path), 0x80000000, 7, None, 3, 0, None)
    if handle in (None, ctypes.c_void_p(-1).value):
        raise OSError(ctypes.get_last_error(), 'could not open owned offer')
    try:
        buf = ctypes.create_string_buffer(513)
        count = w.DWORD()
        assert read(handle, buf, len(buf), ctypes.byref(count), None) and 0 < count.value <= 512
        return buf.raw[:count.value]
    finally:
        close(handle)

def bounded(check, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = check()
        if value:
            return value
        time.sleep(.05)
    raise AssertionError('fixture condition not met')


def refuse_attributed_unsigned(pipe, proc):
    from ctypes import wintypes as w
    create = api.CreateFileW
    create.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,w.LPVOID,w.DWORD,w.DWORD,w.HANDLE]
    create.restype = w.HANDLE
    get_pid = api.GetNamedPipeServerProcessId
    get_pid.argtypes = [w.HANDLE, ctypes.POINTER(w.ULONG)]
    get_pid.restype = w.BOOL
    open_proc = api.OpenProcess
    open_proc.argtypes = [w.DWORD,w.BOOL,w.DWORD]
    open_proc.restype = w.HANDLE
    process_wait = api.WaitForSingleObject
    process_wait.argtypes = [w.HANDLE,w.DWORD]
    process_wait.restype = w.DWORD
    image_path = api.QueryFullProcessImageNameW
    image_path.argtypes = [w.HANDLE,w.DWORD,w.LPWSTR,ctypes.POINTER(w.DWORD)]
    image_path.restype = w.BOOL
    peek = api.PeekNamedPipe
    peek.argtypes = [w.HANDLE,w.LPVOID,w.DWORD,ctypes.POINTER(w.DWORD),ctypes.POINTER(w.DWORD),ctypes.POINTER(w.DWORD)]
    peek.restype = w.BOOL
    invalid = ctypes.c_void_p(-1).value
    handle = create(pipe, 0x00120083, 0, None, 3, 0x00100000, None)
    assert handle not in (None, invalid), ctypes.get_last_error()
    try:
        pid = w.ULONG()
        assert get_pid(handle,ctypes.byref(pid)) and pid.value == proc.pid
        server = open_proc(0x101000,False,pid.value)
        assert server, ctypes.get_last_error()
        try:
            assert process_wait(server,0) == 0x102
            buf = ctypes.create_unicode_buffer(32768)
            length = w.DWORD(len(buf))
            assert image_path(server,0,buf,ctypes.byref(length))
            assert Path(buf.value).resolve(strict=True) == exe
            check = subprocess.run([str(probe_exe),buf.value,'0'*64],cwd=root,text=True,
                                   capture_output=True,timeout=4,creationflags=subprocess.CREATE_NO_WINDOW)
            assert check.returncode == 2 and json.loads(check.stdout) == {
                'status':'signature-not-verified','provider':'800B0100'}, (check.returncode,check.stdout)
            available = w.DWORD()
            assert peek(handle,None,0,None,ctypes.byref(available),None) and not available.value
            assert process_wait(server,0) == 0x102 and get_pid(handle,ctypes.byref(pid)) and pid.value == proc.pid
        finally:
            api.CloseHandle(server)
    finally:
        api.CloseHandle(handle)

def scenario(flag, expire):
    with TemporaryDirectory(prefix='nect-local-window-check-', dir=root) as folder:
        cwd = Path(folder)
        proc = subprocess.Popen([str(exe), *flag], cwd=cwd, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, encoding='utf8', creationflags=subprocess.CREATE_NO_WINDOW)
        lines = Queue()
        def collect():
            try:
                for line in proc.stdout:
                    lines.put(json.loads(line))
            finally:
                lines.put(None)
        Thread(target=collect, daemon=True).start()
        def send(method, params, key=None):
            m = dict(jsonrpc='2.0', method=method, params=params)
            if key is not None:
                m['id'] = key
            proc.stdin.write(json.dumps(m)+'\n')
            proc.stdin.flush()
            if key is not None:
                reply = lines.get(timeout=4)
                assert reply is not None and reply['id'] == key, proc.stderr.read() if proc.poll() is not None else ''
                return reply
        try:
            assert send('initialize',dict(protocolVersion='2025-06-18',capabilities={},
                clientInfo=dict(name='window-check',version='0')),1)['result']['protocolVersion']=='2025-06-18'
            send('notifications/initialized',{})
            assert [tool['name'] for tool in send('tools/list',{},2)['result']['tools']]==['nect_inspect']
            assert send('tools/call',dict(name='nect_inspect',arguments={}),3)['result']['structuredContent']=={'status':'pairing-required'}
            if not flag:
                assert not tuple(cwd.iterdir())
                return
            files = bounded(lambda: list(cwd.glob('nect-mcp-pairing-fixture-*/pending.nect-pair')), 3)
            assert len(files)==1
            path = files[0]
            data = json.loads(read_offer(path))
            assert set(data)=={'kind','version','ticket'} and data['version']==1 and data['kind']=='nect-local-read-pairing'
            assert re.fullmatch('[0-9a-f]{32}',data['ticket'])
            pipe = r'\\.\pipe\NectPairLocal-'+data['ticket']
            assert wait(pipe,500), 'expected one local idle endpoint'
            if not expire:
                refuse_attributed_unsigned(pipe, proc)
            if expire:
                bounded(lambda:not path.exists(),12)
                bounded(lambda:not wait(pipe,200),1)
                assert send('ping',{},4)['result']=={}
                assert send('tools/call',dict(name='nect_inspect'),5)['result']['structuredContent']=={'status':'pairing-required'}
            else:
                proc.stdin.close()
                assert proc.wait(timeout=4)==0
                bounded(lambda:not path.exists(),1)
                assert not wait(pipe,200), 'closed helper left its local endpoint alive'
        finally:
            if proc.poll() is None:
                if not proc.stdin.closed:
                    proc.stdin.close()
                try:
                    proc.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=3)

scenario([],False)
scenario(['--verify-pairing-window'],True)
scenario(['--verify-pairing-window'],False)
print(json.dumps({'defaultNoArtifacts':True,'isolatedLocalOffer':True,'idleEndpointExists':True,
 'expiredDuringOpenMcpSession':True,'closeCleansEarly':True,'documentReturned':False,'kernelAttributedUnsignedRefused':True,'pipePayloadBytes':0}))
