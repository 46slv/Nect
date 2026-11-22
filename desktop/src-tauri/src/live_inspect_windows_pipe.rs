//! A Windows-only prototype of the local single-client authorization boundary.
//! Not registered with Tauri or started by the app. No discovery or product data.
use std::{
    ffi::c_void,
    fs::File,
    io,
    os::windows::io::{AsRawHandle, FromRawHandle},
    ptr::null_mut,
};

type Handle = *mut c_void;

const PIPE_ACCESS_DUPLEX: u32 = 3;
const FILE_FLAG_FIRST_PIPE_INSTANCE: u32 = 0x0008_0000;
const PIPE_REJECT_REMOTE_CLIENTS: u32 = 8;
const ERROR_PIPE_CONNECTED: i32 = 535;

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
    fn ConnectNamedPipe(pipe: Handle, overlapped: *mut c_void) -> i32;
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

struct LocalReadPipe(File);

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
                PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
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

    fn attach(self, expected_pid: u32) -> io::Result<File> {
        if expected_pid == 0 {
            return Err(io::Error::new(
                io::ErrorKind::PermissionDenied,
                "Missing client process",
            ));
        }
        let handle = self.0.as_raw_handle();
        if unsafe { ConnectNamedPipe(handle, null_mut()) } == 0 {
            let error = io::Error::last_os_error();
            if error.raw_os_error() != Some(ERROR_PIPE_CONNECTED) {
                return Err(error);
            }
        }
        let mut actual = 0;
        if unsafe { GetNamedPipeClientProcessId(handle, &mut actual) } == 0
            || actual != expected_pid
        {
            return Err(io::Error::new(
                io::ErrorKind::PermissionDenied,
                "Unexpected client process",
            ));
        }
        Ok(self.0)
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
        let pipe = LocalReadPipe::create(&id, &account).unwrap();
        let client = connect(&name);
        assert!(LocalReadPipe::create(&id, &account).is_err());
        assert_eq!(
            pipe.attach(std::process::id() ^ 1).unwrap_err().kind(),
            io::ErrorKind::PermissionDenied
        );
        drop(client);
        let pipe = LocalReadPipe::create(&id, &account).unwrap();
        let mut client = connect(&name);
        let mut server = pipe.attach(std::process::id()).unwrap();
        client.write_all(b"inspect").unwrap();
        let mut buf = [0; 7];
        server.read_exact(&mut buf).unwrap();
        assert_eq!(&buf, b"inspect");
        server.write_all(b"fixture-only").unwrap();
        let mut buf = [0; 12];
        client.read_exact(&mut buf).unwrap();
        assert_eq!(&buf, b"fixture-only");
    }
}
