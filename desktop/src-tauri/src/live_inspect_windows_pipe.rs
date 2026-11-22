//! A Windows-only prototype of the local single-client authorization boundary.
//! Not registered with Tauri or started by the app. No discovery or product data.
use std::{
    ffi::c_void,
    fs::File,
    io,
    os::windows::io::{AsRawHandle, FromRawHandle, OwnedHandle},
    ptr::null_mut,
    sync::atomic::{AtomicBool, Ordering},
    time::{Duration, Instant},
};

type Handle = *mut c_void;

const PIPE_ACCESS_DUPLEX: u32 = 3;
const FILE_FLAG_FIRST_PIPE_INSTANCE: u32 = 0x0008_0000;
const PIPE_REJECT_REMOTE_CLIENTS: u32 = 8;
const ERROR_PIPE_CONNECTED: i32 = 535;
const ERROR_IO_PENDING: i32 = 997;
const FILE_FLAG_OVERLAPPED: u32 = 0x4000_0000;
const WAIT_TIMEOUT: u32 = 258;
const PROCESS_QUERY_AND_WAIT: u32 = 0x0010_1000;
const MAX_IO: usize = 4096;

#[repr(C)]
struct Overlapped {
    internal: usize,
    internal_high: usize,
    offset: u32,
    offset_high: u32,
    event: Handle,
}

#[repr(C)]
struct SecurityAttributes {
    length: u32,
    descriptor: *mut c_void,
    inherit: i32,
}

#[link(name = "Advapi32")]
extern "system" {
    fn ConvertStringSecurityDescriptorToSecurityDescriptorW(
        sddl: *const u16,
        revision: u32,
        descriptor: *mut *mut c_void,
        size: *mut u32,
    ) -> i32;
}
#[link(name = "Kernel32")]
extern "system" {
    fn CreateNamedPipeW(
        name: *const u16,
        open_mode: u32,
        pipe_mode: u32,
        max_instances: u32,
        out_size: u32,
        in_size: u32,
        timeout: u32,
        security: *mut SecurityAttributes,
    ) -> Handle;
    fn ConnectNamedPipe(pipe: Handle, overlapped: *mut Overlapped) -> i32;
    fn OpenProcess(desired_access: u32, inherit: i32, id: u32) -> Handle;
    fn CreateEventW(
        attrs: *mut SecurityAttributes,
        manual_reset: i32,
        initial: i32,
        name: *const u16,
    ) -> Handle;
    fn WaitForSingleObject(handle: Handle, ms: u32) -> u32;
    fn CancelIoEx(handle: Handle, operation: *mut Overlapped) -> i32;
    fn GetOverlappedResult(
        handle: Handle,
        operation: *mut Overlapped,
        transferred: *mut u32,
        wait: i32,
    ) -> i32;
    fn ReadFile(
        handle: Handle,
        buffer: *mut u8,
        bytes: u32,
        transferred: *mut u32,
        operation: *mut Overlapped,
    ) -> i32;
    fn WriteFile(
        handle: Handle,
        buffer: *const u8,
        bytes: u32,
        transferred: *mut u32,
        operation: *mut Overlapped,
    ) -> i32;
    fn GetNamedPipeClientProcessId(pipe: Handle, id: *mut u32) -> i32;
    fn LocalFree(data: *mut c_void) -> *mut c_void;
}

fn wide(s: &str) -> Vec<u16> {
    s.encode_utf16().chain(std::iter::once(0)).collect()
}

fn endpoint_name(id: &str) -> Option<String> {
    (id.len() == 32
        && id
            .bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b)))
    .then(|| format!(r"\\.\pipe\NectReadProof-{id}"))
}

fn logon_sid(value: &str) -> bool {
    value.strip_prefix("S-1-5-5-").is_some_and(|tail| {
        let mut fields = tail.split('-');
        let decimal_u32 = |s: &str| {
            !s.is_empty() && s.bytes().all(|b| b.is_ascii_digit()) && s.parse::<u32>().is_ok()
        };
        fields.next().is_some_and(decimal_u32)
            && fields.next().is_some_and(decimal_u32)
            && fields.next().is_none()
    })
}

#[derive(Debug)]
struct SelectedClient {
    pid: u32,
    handle: OwnedHandle,
}
impl SelectedClient {
    // Bind selection before accepting a peer; the raw PID is not a grant.
    fn retain(pid: u32) -> io::Result<Self> {
        let raw = unsafe { OpenProcess(PROCESS_QUERY_AND_WAIT, 0, pid) };
        if raw.is_null() {
            return Err(io::Error::last_os_error());
        }
        Ok(Self {
            pid,
            handle: unsafe { OwnedHandle::from_raw_handle(raw) },
        })
    }

    fn live(&self) -> bool {
        (unsafe { WaitForSingleObject(self.handle.as_raw_handle(), 0) }) == WAIT_TIMEOUT
    }
}

struct IoWindow<'a> {
    deadline: Instant,
    revoked: &'a AtomicBool,
}

impl IoWindow<'_> {
    fn remaining(&self) -> io::Result<u32> {
        if self.revoked.load(Ordering::Acquire) {
            return Err(io::Error::new(
                io::ErrorKind::Interrupted,
                "Inspection revoked",
            ));
        }
        let left = self
            .deadline
            .checked_duration_since(Instant::now())
            .filter(|d| !d.is_zero())
            .ok_or_else(|| io::Error::from(io::ErrorKind::TimedOut))?;
        Ok(left.as_millis().clamp(1, 10) as u32)
    }
}

struct LocalReadPipe(File);
#[derive(Debug)]
struct ConnectedPipe<'a>(File, &'a SelectedClient);

fn overlapped_io(
    pipe: Handle,
    window: &IoWindow<'_>,
    allow_already_connected: bool,
    peer: &SelectedClient,
    start: impl FnOnce(*mut Overlapped) -> i32,
) -> io::Result<usize> {
    let remaining = || {
        if !peer.live() {
            return Err(io::Error::from(io::ErrorKind::BrokenPipe));
        }
        window.remaining()
    };
    remaining()?;
    let event = unsafe { CreateEventW(null_mut(), 1, 0, std::ptr::null()) };
    if event.is_null() {
        return Err(io::Error::last_os_error());
    }
    let event = unsafe { OwnedHandle::from_raw_handle(event) };
    let mut op = Overlapped {
        internal: 0,
        internal_high: 0,
        offset: 0,
        offset_high: 0,
        event: event.as_raw_handle(),
    };
    // Use one stable raw address across the pending operation and cancellation.
    // The stack event and caller-owned buffer remain valid until completion.
    let operation = std::ptr::addr_of_mut!(op);
    let mut transferred = 0;
    if start(operation) == 0 {
        let error = io::Error::last_os_error();
        if allow_already_connected && error.raw_os_error() == Some(ERROR_PIPE_CONNECTED) {
            remaining()?;
            return Ok(0);
        }
        if error.raw_os_error() != Some(ERROR_IO_PENDING) {
            return Err(error);
        }
        loop {
            let wait = remaining().and_then(|ms| {
                match unsafe { WaitForSingleObject(event.as_raw_handle(), ms) } {
                    0 => Ok(true),
                    WAIT_TIMEOUT => Ok(false),
                    _ => Err(io::Error::last_os_error()),
                }
            });
            match wait {
                Ok(true) => break,
                Ok(false) => {}
                Err(error) => {
                    // Kernel may still own the OVERLAPPED and buffer on cancellation.
                    // Drain completion before allowing either to go out of scope.
                    unsafe {
                        CancelIoEx(pipe, operation);
                        GetOverlappedResult(pipe, operation, &mut transferred, 1);
                    }
                    return Err(error);
                }
            }
        }
    }
    if unsafe { GetOverlappedResult(pipe, operation, &mut transferred, 0) } == 0 {
        return Err(io::Error::last_os_error());
    }
    remaining()?;
    Ok(transferred as usize)
}

impl LocalReadPipe {
    fn create(id: &str, logon: &str) -> io::Result<Self> {
        let name = endpoint_name(id)
            .filter(|_| logon_sid(logon))
            .ok_or_else(|| {
                io::Error::new(io::ErrorKind::InvalidInput, "Invalid endpoint identity")
            })?;
        let mut descriptor = null_mut();
        // FILE_GENERIC_READ with FILE_WRITE_DATA, not FILE_APPEND_DATA /
        // FILE_CREATE_PIPE_INSTANCE. The remote-client flag is independent.
        let sddl = wide(&format!("D:P(A;;0x00120083;;;{logon})"));
        if unsafe {
            ConvertStringSecurityDescriptorToSecurityDescriptorW(
                sddl.as_ptr(),
                1,
                &mut descriptor,
                null_mut(),
            )
        } == 0
        {
            return Err(io::Error::last_os_error());
        }
        let mut attrs = SecurityAttributes {
            length: std::mem::size_of::<SecurityAttributes>() as u32,
            descriptor,
            inherit: 0,
        };
        let raw = unsafe {
            CreateNamedPipeW(
                wide(&name).as_ptr(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                PIPE_REJECT_REMOTE_CLIENTS,
                1,
                4096,
                4096,
                100,
                &mut attrs,
            )
        };
        let error = io::Error::last_os_error();
        unsafe {
            LocalFree(descriptor);
        }
        if raw as isize == -1 {
            return Err(error);
        }
        Ok(Self(unsafe { File::from_raw_handle(raw) }))
    }

    fn attach<'a>(
        self,
        client: &'a SelectedClient,
        window: &IoWindow<'_>,
    ) -> io::Result<ConnectedPipe<'a>> {
        if !client.live() {
            return Err(io::Error::from(io::ErrorKind::PermissionDenied));
        }
        let handle = self.0.as_raw_handle();
        overlapped_io(handle, window, true, client, |op| unsafe {
            ConnectNamedPipe(handle, op)
        })?;
        let mut actual = 0;
        if unsafe { GetNamedPipeClientProcessId(handle, &mut actual) } == 0
            || actual != client.pid
            || !client.live()
        {
            return Err(io::Error::new(
                io::ErrorKind::PermissionDenied,
                "Unexpected client process",
            ));
        }
        Ok(ConnectedPipe(self.0, client))
    }
}

impl ConnectedPipe<'_> {
    fn read(&self, bytes: &mut [u8], window: &IoWindow<'_>) -> io::Result<usize> {
        if bytes.len() > MAX_IO {
            return Err(io::Error::from(io::ErrorKind::InvalidInput));
        }
        let handle = self.0.as_raw_handle();
        overlapped_io(handle, window, false, self.1, |op| unsafe {
            ReadFile(
                handle,
                bytes.as_mut_ptr(),
                bytes.len() as u32,
                null_mut(),
                op,
            )
        })
    }

    fn write(&self, bytes: &[u8], window: &IoWindow<'_>) -> io::Result<usize> {
        if bytes.len() > MAX_IO {
            return Err(io::Error::from(io::ErrorKind::InvalidInput));
        }
        let handle = self.0.as_raw_handle();
        overlapped_io(handle, window, false, self.1, |op| unsafe {
            WriteFile(handle, bytes.as_ptr(), bytes.len() as u32, null_mut(), op)
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        fs::OpenOptions,
        io::{Read, Write},
        os::windows::fs::OpenOptionsExt,
    };

    fn existing_logon_sid() -> String {
        let result = std::process::Command::new("whoami.exe")
            .arg("/logonid")
            .output()
            .unwrap();
        assert!(result.status.success());
        String::from_utf8_lossy(&result.stdout)
            .split_whitespace()
            .find(|part| logon_sid(part))
            .expect("Windows logon SID")
            .to_owned()
    }

    #[test]
    fn strict_endpoint_and_sid_shape() {
        assert!(endpoint_name("abcdef00112233445566778899aabbcc").is_some());
        assert!(endpoint_name(r"\\computer\pipe\another").is_none());
        assert!(logon_sid("S-1-5-5-12-34"));
        assert!(!logon_sid("S-1-5-5-12-34)"));
        assert!(!logon_sid("S-1-5-18"));
    }

    #[test]
    #[ignore = "requires a standard Windows logon session with local pipe access"]
    fn native_endpoint_matches_client_before_read_and_closes_on_mismatch() {
        let account = existing_logon_sid();
        let id = format!(
            "{:032x}",
            (std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos())
                ^ u128::from(std::process::id())
        );
        let name = endpoint_name(&id).unwrap();
        fn connect(name: &str) -> File {
            OpenOptions::new()
                .access_mode(0x00120083)
                .open(name)
                .unwrap()
        }
        let revoked = AtomicBool::new(false);
        let available = || IoWindow {
            deadline: Instant::now() + Duration::from_secs(2),
            revoked: &revoked,
        };
        let selected = SelectedClient::retain(std::process::id()).unwrap();
        let never_connected = LocalReadPipe::create(&id, &account).unwrap();
        let brief = IoWindow {
            deadline: Instant::now() + Duration::from_millis(30),
            revoked: &revoked,
        };
        assert_eq!(
            never_connected
                .attach(&selected, &brief)
                .unwrap_err()
                .kind(),
            io::ErrorKind::TimedOut
        );
        let pipe = LocalReadPipe::create(&id, &account).unwrap();
        let client = connect(&name);
        assert!(LocalReadPipe::create(&id, &account).is_err());
        // A deliberate mismatch is visible even when all traffic is in this
        // test process. The production owner must only construct via retain().
        let wrong = SelectedClient {
            pid: selected.pid ^ 1,
            handle: selected.handle.try_clone().unwrap(),
        };
        assert_eq!(
            pipe.attach(&wrong, &available()).unwrap_err().kind(),
            io::ErrorKind::PermissionDenied
        );
        drop(client);
        let pipe = LocalReadPipe::create(&id, &account).unwrap();
        let mut client = connect(&name);
        let server = pipe.attach(&selected, &available()).unwrap();
        let mut buf = [0; 7];
        let brief = IoWindow {
            deadline: Instant::now() + Duration::from_millis(30),
            revoked: &revoked,
        };
        assert_eq!(
            server.read(&mut buf, &brief).unwrap_err().kind(),
            io::ErrorKind::TimedOut
        );
        client.write_all(b"inspect").unwrap();
        assert_eq!(server.read(&mut buf, &available()).unwrap(), 7);
        assert_eq!(&buf, b"inspect");
        assert_eq!(server.write(b"fixture-only", &available()).unwrap(), 12);
        let mut buf = [0; 12];
        client.read_exact(&mut buf).unwrap();
        assert_eq!(&buf, b"fixture-only");
        std::thread::scope(|scope| {
            scope.spawn(|| {
                std::thread::sleep(Duration::from_millis(25));
                revoked.store(true, Ordering::Release);
            });
            let mut untouched = [0; 1];
            assert_eq!(
                server
                    .read(&mut untouched, &available())
                    .unwrap_err()
                    .kind(),
                io::ErrorKind::Interrupted
            );
        });
    }

    const CHILD_TEST: &str = "live_inspect_windows_pipe::tests::native_pipe_fixture_child";

    #[test]
    #[ignore = "internal child of the explicit cross-process fixture"]
    fn native_pipe_fixture_child() {
        let Ok(id) = std::env::var("NECT_PIPE_TEST_ID") else {
            return;
        };
        let expected = std::env::var("NECT_PIPE_TEST_OUTCOME").unwrap();
        assert!(matches!(expected.as_str(), "allow" | "deny"));
        let name = endpoint_name(&id).expect("fixture endpoint");
        let mut pipe = OpenOptions::new()
            .access_mode(0x00120083)
            .open(name)
            .unwrap();
        let mut bytes = [0; 12];
        if expected == "allow" {
            pipe.write_all(b"inspect").unwrap();
            pipe.read_exact(&mut bytes).unwrap();
            assert_eq!(&bytes, b"fixture-only");
            let mut release = [0; 1];
            std::io::stdin().read_exact(&mut release).unwrap();
            assert_eq!(release, [1]);
        } else {
            // A denied peer never sees application bytes.
            match pipe.read(&mut bytes) {
                Ok(0) => {}
                Err(e) if matches!(e.raw_os_error(), Some(109 | 232 | 233)) => {}
                other => panic!("Denied connector had an unexpected read: {other:?}"),
            }
        }
    }

    struct FixtureChild(std::process::Child);
    impl Drop for FixtureChild {
        fn drop(&mut self) {
            if self.0.try_wait().ok().flatten().is_none() {
                let _ = self.0.kill();
                let _ = self.0.wait();
            }
        }
    }
    impl FixtureChild {
        fn spawn(id: &str, expected: &str) -> Self {
            use std::os::windows::process::CommandExt;
            use std::process::{Command, Stdio};
            let exe = std::env::current_exe().unwrap();
            Self(
                Command::new(exe)
                    .args(["--exact", CHILD_TEST, "--include-ignored"])
                    .env("NECT_PIPE_TEST_ID", id)
                    .env("NECT_PIPE_TEST_OUTCOME", expected)
                    .stdin(Stdio::piped())
                    .stdout(Stdio::null())
                    .stderr(Stdio::null())
                    .creation_flags(0x0800_0000)
                    .spawn()
                    .unwrap(),
            )
        }
        fn finish(&mut self) {
            let until = Instant::now() + Duration::from_secs(4);
            loop {
                if let Some(status) = self.0.try_wait().unwrap() {
                    assert!(status.success(), "fixture client failed: {status}");
                    return;
                }
                assert!(Instant::now() < until, "fixture client did not exit");
                std::thread::sleep(Duration::from_millis(10));
            }
        }
    }

    #[test]
    #[ignore = "requires a standard Windows logon session with local pipe access"]
    fn distinct_processes_only_selected_client_reads_and_exit_revokes() {
        let account = existing_logon_sid();
        let revoked = AtomicBool::new(false);
        let window = || IoWindow {
            deadline: Instant::now() + Duration::from_secs(4),
            revoked: &revoked,
        };
        let stamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let id = format!("{:032x}", stamp ^ (u128::from(std::process::id()) << 32));
        let pipe = LocalReadPipe::create(&id, &account).unwrap();
        let mut not_selected = FixtureChild::spawn(&id, "deny");
        let selected_parent = SelectedClient::retain(std::process::id()).unwrap();
        assert_ne!(selected_parent.pid, not_selected.0.id());
        assert_eq!(
            pipe.attach(&selected_parent, &window()).unwrap_err().kind(),
            io::ErrorKind::PermissionDenied
        );
        not_selected.finish();

        let id = format!(
            "{:032x}",
            stamp.wrapping_add(1) ^ (u128::from(std::process::id()) << 32)
        );
        let pipe = LocalReadPipe::create(&id, &account).unwrap();
        let mut actual = FixtureChild::spawn(&id, "allow");
        let selected_child = SelectedClient::retain(actual.0.id()).unwrap();
        let connection = pipe.attach(&selected_child, &window()).unwrap();
        let mut bytes = [0; 7];
        assert_eq!(connection.read(&mut bytes, &window()).unwrap(), bytes.len());
        assert_eq!(&bytes, b"inspect");
        assert_eq!(connection.write(b"fixture-only", &window()).unwrap(), 12);
        actual.0.stdin.take().unwrap().write_all(&[1]).unwrap();
        actual.finish();
        assert!(!selected_child.live());
        assert_eq!(
            connection.read(&mut bytes, &window()).unwrap_err().kind(),
            io::ErrorKind::BrokenPipe
        );
    }
}
