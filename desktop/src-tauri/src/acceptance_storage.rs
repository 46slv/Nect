//! Opt-in disposable storage. Normal mode changes no directory or environment.
use std::{
    ffi::OsString,
    fs,
    io::{Read, Write},
    path::{Component, Path, PathBuf},
};

pub const ROOT_ENV: &str = "NECT_SVG_ACCEPTANCE_ROOT";
const ROOT_PREFIX: &str = "nect-svg-acceptance-";

#[derive(Clone, Default)]
pub struct RunStorage(pub Option<AcceptancePaths>);

#[derive(Clone)]
pub struct AcceptancePaths {
    root: PathBuf,
    pub webview: PathBuf,
    documents: PathBuf,
}

fn component_is_unambiguous(value: &str) -> bool {
    let stem = value
        .split('.')
        .next()
        .unwrap_or("")
        .trim_end_matches(' ')
        .to_ascii_uppercase();
    let reserved = ["CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"];
    let port = stem
        .strip_prefix("COM")
        .or_else(|| stem.strip_prefix("LPT"));
    !value.is_empty()
        && value != "."
        && value != ".."
        && !value.ends_with(['.', ' '])
        && !value
            .chars()
            .any(|c| c.is_control() || "<>:\"/\\|?*".contains(c))
        && !reserved.contains(&stem.as_str())
        && !port.is_some_and(|s| {
            ["1", "2", "3", "4", "5", "6", "7", "8", "9", "¹", "²", "³"].contains(&s)
        })
}

fn absolute_windows_path(value: &str) -> Result<PathBuf, String> {
    let bytes = value.as_bytes();
    if value.trim() != value
        || bytes.len() < 4
        || !bytes[0].is_ascii_alphabetic()
        || bytes[1] != b':'
        || bytes[2] != b'\\'
        || !value[3..].split('\\').all(component_is_unambiguous)
    {
        return Err("Acceptance paths require an unambiguous absolute local drive path".into());
    }
    Ok(PathBuf::from(value))
}

fn is_reparse(metadata: &fs::Metadata) -> bool {
    #[cfg(windows)]
    {
        use std::os::windows::fs::MetadataExt;
        metadata.file_attributes() & 0x400 != 0
    }
    #[cfg(not(windows))]
    {
        metadata.file_type().is_symlink()
    }
}

fn check_directory_chain(path: &Path) -> Result<(), String> {
    let mut current = PathBuf::new();
    for component in path.components() {
        current.push(component.as_os_str());
        if matches!(component, Component::Prefix(_)) {
            continue;
        }
        let metadata = fs::symlink_metadata(&current).map_err(|e| e.to_string())?;
        if !metadata.is_dir() || is_reparse(&metadata) {
            return Err("Acceptance directories must not contain links or reparse points".into());
        }
    }
    Ok(())
}

fn canonical_directory(path: &Path) -> Result<PathBuf, String> {
    check_directory_chain(path)?;
    let canonical = fs::canonicalize(path).map_err(|e| e.to_string())?;
    // Use ordinary absolute drive spelling with WebView2 and native dialogs.
    #[cfg(windows)]
    {
        let text = canonical
            .to_str()
            .ok_or("Non-Unicode acceptance directory")?;
        absolute_windows_path(text.strip_prefix(r"\\?\").unwrap_or(text))
    }
    #[cfg(not(windows))]
    {
        Ok(canonical)
    }
}

fn same_path(a: &Path, b: &Path) -> bool {
    // Canonical paths may belong to case-sensitive NTFS directories. Never
    // equate distinct canonical spellings; conservative refusal is intentional.
    a == b
}

impl RunStorage {
    pub fn prepare(
        requested: Option<OsString>,
        executable_directory: impl FnOnce() -> Result<PathBuf, String>,
    ) -> Result<Self, String> {
        let Some(requested) = requested else {
            return Ok(Self::default());
        };
        if !cfg!(windows) {
            return Err("Acceptance storage mode is supported only on Windows".into());
        }
        let text = requested
            .into_string()
            .map_err(|_| "Non-Unicode acceptance root")?;
        let requested = absolute_windows_path(&text)?;
        let name = requested
            .file_name()
            .and_then(|n| n.to_str())
            .ok_or("Missing run name")?;
        if !name.starts_with(ROOT_PREFIX) || name.len() == ROOT_PREFIX.len() {
            return Err(
                "Acceptance root needs the prefix nect-svg-acceptance- and a run ID".into(),
            );
        }
        let parent = canonical_directory(requested.parent().ok_or("Missing run parent")?)?;
        let executable = canonical_directory(&executable_directory()?)?;
        if !same_path(&parent, &executable) {
            return Err(
                "Acceptance root must be a new direct child of the isolated EXE directory".into(),
            );
        }
        let root = parent.join(name);
        // Atomic refusal of every existing root, even an empty directory.
        // Never create ancestors or fall back to the ordinary profile.
        fs::create_dir(&root).map_err(|e| format!("Cannot create fresh acceptance root: {e}"))?;
        let paths = AcceptancePaths {
            webview: root.join("webview2"),
            documents: root.join("documents"),
            root,
        };
        fs::create_dir(&paths.webview).map_err(|e| e.to_string())?;
        fs::create_dir(&paths.documents).map_err(|e| e.to_string())?;
        paths.check_directories()?;
        Ok(Self(Some(paths)))
    }

    pub fn dialog(&self, window: &tauri::WebviewWindow) -> rfd::FileDialog {
        let dialog = rfd::FileDialog::new().set_parent(window);
        match &self.0 {
            Some(paths) => dialog.set_directory(&paths.documents),
            None => dialog,
        }
    }

    fn document_path(&self, path: &Path, must_exist: bool) -> Result<PathBuf, String> {
        let Some(paths) = &self.0 else {
            return Ok(path.to_path_buf());
        };
        paths.check_directories()?;
        let path = absolute_windows_path(path.to_str().ok_or("Non-Unicode document path")?)?;
        let parent = canonical_directory(path.parent().ok_or("Missing document directory")?)?;
        if !same_path(&parent, &paths.documents) {
            return Err(
                "Acceptance Open/Save is restricted to this run's documents directory".into(),
            );
        }
        let target = parent.join(path.file_name().ok_or("Missing document name")?);
        match fs::symlink_metadata(&target) {
            Ok(metadata) if !metadata.is_file() || is_reparse(&metadata) => {
                return Err("Acceptance documents must be regular files, not links".into());
            }
            Ok(_) => {}
            Err(e) if !must_exist && e.kind() == std::io::ErrorKind::NotFound => {}
            Err(e) => return Err(e.to_string()),
        }
        Ok(target)
    }

    pub fn read_project(&self, path: &Path) -> Result<String, String> {
        let path = self.document_path(path, true)?;
        let file = fs::File::open(path).map_err(|e| e.to_string())?;
        let mut text = String::new();
        file.take(50_000_001)
            .read_to_string(&mut text)
            .map_err(|e| e.to_string())?;
        if text.len() > 50_000_000 {
            return Err("Project exceeds 50 MB".into());
        }
        Ok(text)
    }

    pub fn read_svg_source(&self, path: &Path) -> Result<String, String> {
        let path = self.document_path(path, true)?;
        let file = fs::File::open(path).map_err(|e| e.to_string())?;
        let mut text = String::new();
        file.take(10_000_001)
            .read_to_string(&mut text)
            .map_err(|e| format!("SVG must be readable UTF-8: {e}"))?;
        if text.len() > 10_000_000 {
            return Err("SVG exceeds 10 MB".into());
        }
        Ok(text)
    }

    /// Read-only, bounded binary input with exactly the acceptance document
    /// confinement used by Open/Save. No font path or OS error crosses IPC.
    pub fn read_private_font(&self, path: &Path) -> Result<Vec<u8>, String> {
        const LIMIT: u64 = 128 * 1024 * 1024;
        let path = self.document_path(path, true)
            .map_err(|_| "Selected private font is outside the permitted storage boundary or is not a regular file")?;
        let mut options = fs::OpenOptions::new();
        options.read(true);
        #[cfg(windows)] {
            use std::os::windows::fs::OpenOptionsExt;
            // Open a final reparse point itself, never follow it between check
            // and open. The handle metadata check below rejects that point.
            options.custom_flags(0x00200000); // FILE_FLAG_OPEN_REPARSE_POINT
        }
        let file = options.open(&path).map_err(|_| "Cannot open the selected private font file")?;
        let metadata = file.metadata().map_err(|_| "Cannot inspect the selected private font file")?;
        if !metadata.is_file() || is_reparse(&metadata) || !(12..=LIMIT).contains(&metadata.len()) {
            return Err("Private font must be a regular file between 12 bytes and 128 MiB".into());
        }
        self.document_path(&path, true)
            .map_err(|_| "Private font storage boundary changed before reading")?;
        self.verify_private_font_handle(&file, &path)?;
        let mut bytes = Vec::new();
        (&file).take(LIMIT + 1).read_to_end(&mut bytes)
            .map_err(|_| "Cannot read the selected private font file")?;
        if !(12..=LIMIT as usize).contains(&bytes.len()) {
            return Err("Private font must be between 12 bytes and 128 MiB".into());
        }
        self.document_path(&path, true)
            .map_err(|_| "Private font storage boundary changed while reading")?;
        self.verify_private_font_handle(&file, &path)?;
        Ok(bytes)
    }

    fn verify_private_font_handle(&self, file: &fs::File, expected: &Path) -> Result<(), String> {
        if self.0.is_none() {
            return Ok(());
        }
        #[cfg(windows)]
        {
            use std::os::windows::{ffi::OsStringExt, io::AsRawHandle};
            #[link(name = "kernel32")]
            extern "system" {
                fn GetFinalPathNameByHandleW(
                    file: *mut std::ffi::c_void,
                    path: *mut u16,
                    size: u32,
                    flags: u32,
                ) -> u32;
            }
            let mut buffer = vec![0u16; 32768];
            let length = unsafe {
                GetFinalPathNameByHandleW(
                    file.as_raw_handle(),
                    buffer.as_mut_ptr(),
                    buffer.len() as u32,
                    0,
                )
            } as usize;
            if length == 0 || length >= buffer.len() {
                return Err("Cannot verify private font storage boundary".into());
            }
            let actual = PathBuf::from(OsString::from_wide(&buffer[..length]));
            if !same_path(&actual, expected) {
                return Err("Opened private font is outside its permitted storage boundary".into());
            }
        }
        #[cfg(not(windows))]
        let _ = (file, expected);
        Ok(())
    }

    pub fn write_bytes(&self, path: &Path, contents: &[u8]) -> Result<(), String> {
        let path = self.document_path(path, false)?;
        let parent = path.parent().ok_or("Invalid destination")?;
        let mut staged = tempfile::NamedTempFile::new_in(parent).map_err(|e| e.to_string())?;
        staged.write_all(contents).map_err(|e| e.to_string())?;
        staged.as_file().sync_all().map_err(|e| e.to_string())?;
        self.document_path(&path, false)?;
        staged.persist(&path).map_err(|e| e.to_string())?;
        Ok(())
    }

    pub fn project_bytes_if_exists(&self, path: &Path) -> Result<Option<Vec<u8>>, String> {
        let path = self.document_path(path, false)?;
        match fs::File::open(path) {
            Ok(file) => {
                let mut bytes = Vec::new();
                file.take(50_000_001)
                    .read_to_end(&mut bytes)
                    .map_err(|e| e.to_string())?;
                if bytes.len() > 50_000_000 {
                    return Err("Project exceeds 50 MB".into());
                }
                Ok(Some(bytes))
            }
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(None),
            Err(e) => Err(e.to_string()),
        }
    }

    pub fn write_project_checked(
        &self,
        path: &Path,
        contents: &[u8],
        expected: Option<&[u8]>,
    ) -> Result<(), String> {
        let path = self.document_path(path, false)?;
        let check = || -> Result<(), String> {
            if self.project_bytes_if_exists(&path)?.as_deref() != expected {
                return Err(
                    "File changed outside Nect; original preserved. Use Save As or reopen.".into(),
                );
            }
            Ok(())
        };
        check()?;
        let mut staged =
            tempfile::NamedTempFile::new_in(path.parent().ok_or("Invalid destination")?)
                .map_err(|e| e.to_string())?;
        staged.write_all(contents).map_err(|e| e.to_string())?;
        staged.as_file().sync_all().map_err(|e| e.to_string())?;
        check()?;
        staged.persist(&path).map_err(|e| e.to_string())?;
        if self.project_bytes_if_exists(&path)?.as_deref() != Some(contents) {
            return Err(
                "File changed immediately after saving; verify destination before closing".into(),
            );
        }
        Ok(())
    }
}

impl AcceptancePaths {
    pub fn check_directories(&self) -> Result<(), String> {
        for path in [&self.root, &self.webview, &self.documents] {
            if !same_path(&canonical_directory(path)?, path) {
                return Err("Acceptance directory identity changed".into());
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn svg_source_read_is_bounded_utf8_and_does_not_change_the_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("part.svg");
        let storage = RunStorage::default();
        let svg = "<svg><title>日本語 SVG</title><rect width=\"20\" height=\"20\"/></svg>";
        fs::write(&path, svg).unwrap();
        assert_eq!(storage.read_svg_source(&path).unwrap(), svg);
        assert_eq!(fs::read_to_string(&path).unwrap(), svg);
        fs::write(&path, vec![b' '; 10_000_001]).unwrap();
        assert!(storage
            .read_svg_source(&path)
            .unwrap_err()
            .contains("10 MB"));
        fs::write(&path, [0xff, 0xfe]).unwrap();
        assert!(storage
            .read_svg_source(&path)
            .unwrap_err()
            .contains("UTF-8"));
    }

    #[test]
    fn private_font_read_is_bounded_read_only_and_creates_no_copy() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("synthetic.ttf");
        let storage = RunStorage::default();
        let bytes = b"OTTO\0\0\0\0\0\0\0\0";
        fs::write(&path, bytes).unwrap();
        assert_eq!(storage.read_private_font(&path).unwrap(), bytes);
        assert_eq!(fs::read(&path).unwrap(), bytes);
        assert_eq!(fs::read_dir(dir.path()).unwrap().count(), 1);
        let file = fs::OpenOptions::new().write(true).open(&path).unwrap();
        file.set_len(128 * 1024 * 1024 + 1).unwrap();
        assert!(storage.read_private_font(&path).unwrap_err().contains("128 MiB"));
        file.set_len(0).unwrap();
        assert!(storage.read_private_font(&path).is_err());
        assert!(storage.read_private_font(dir.path()).is_err());
    }

    #[test]
    #[cfg(windows)]
    fn private_font_acceptance_read_is_confined_and_keeps_only_source_file() {
        let (_fixture, storage) = fixture();
        let paths = storage.0.as_ref().unwrap();
        let path = paths.documents.join("synthetic.ttc");
        let bytes = b"ttcf\0\x01\0\0\0\0\0\0";
        fs::write(&path, bytes).unwrap();
        assert_eq!(storage.read_private_font(&path).unwrap(), bytes);
        assert_eq!(fs::read_dir(&paths.documents).unwrap().count(), 1);
        let outside = tempfile::tempdir().unwrap();
        let outside_path = outside.path().join("private.ttf");
        fs::write(&outside_path, bytes).unwrap();
        assert!(storage.read_private_font(&outside_path).is_err());
    }

    #[test]
    fn checked_save_preserves_external_bytes_and_supports_cold_reopen() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("project.json");
        let storage = RunStorage::default();
        storage
            .write_project_checked(&path, b"first", None)
            .unwrap();
        assert_eq!(storage.read_project(&path).unwrap(), "first");
        storage
            .write_project_checked(&path, b"second", Some(b"first"))
            .unwrap();
        fs::write(&path, b"extern").unwrap();
        assert!(storage
            .write_project_checked(&path, b"ours", Some(b"second"))
            .is_err());
        assert_eq!(fs::read(&path).unwrap(), b"extern");
        assert!(storage.write_project_checked(&path, b"ours", None).is_err());
        fs::remove_file(&path).unwrap();
        assert!(storage
            .write_project_checked(&path, b"ours", Some(b"extern"))
            .is_err());
        assert!(!path.exists());
    }

    #[test]
    fn rejects_ambiguous_windows_spellings() {
        for value in [
            "",
            " ",
            "C:run",
            "run",
            r"\run",
            r"C:\",
            r"C:\run\",
            r"C:\a\..\run",
            r"C:\a\.\run",
            r"C:\a\\run",
            r"\\server\share\run",
            r"\\?\C:\run",
            r"\\.\C:\run",
            r"C:/run",
            r"C:\run.",
            "C:\\run ",
            r"C:\run:stream",
            r"C:\NUL.txt\run",
            r"C:\COM1\run",
            r"C:\LPT¹\run",
            " C:\\run",
        ] {
            assert!(absolute_windows_path(value).is_err(), "accepted {value:?}");
        }
        assert!(absolute_windows_path(r"C:\test space\日本語\nect-svg-acceptance-001").is_ok());
    }

    #[test]
    fn absent_option_does_not_resolve_or_create_any_directory() {
        let storage =
            RunStorage::prepare(None, || panic!("normal mode resolved an EXE path")).unwrap();
        assert!(storage.0.is_none());
    }

    #[test]
    fn canonical_identity_does_not_fold_case_sensitive_directory_names() {
        let first = Path::new(r"C:\task\Bin\documents");
        let second = Path::new(r"C:\task\bin\documents");
        assert!(!same_path(first, second));
        assert!(same_path(first, first));
    }

    #[test]
    fn invalid_opt_in_never_resolves_an_executable_or_falls_back() {
        for value in ["", " ", "relative", "C:relative", r"\\server\share\run"] {
            let result = RunStorage::prepare(Some(OsString::from(value)), || {
                panic!("invalid input reached executable resolution")
            });
            assert!(
                result.is_err(),
                "invalid input became normal mode: {value:?}"
            );
        }
        #[cfg(windows)]
        {
            use std::os::windows::ffi::OsStringExt;
            let invalid = OsString::from_wide(&[0xd800]);
            assert!(RunStorage::prepare(Some(invalid), || panic!("non-Unicode root")).is_err());
        }
    }

    #[cfg(windows)]
    fn fixture() -> (tempfile::TempDir, RunStorage) {
        let directory = tempfile::tempdir().unwrap();
        let base = canonical_directory(directory.path()).unwrap();
        let root = base.join("nect-svg-acceptance-001");
        let storage = RunStorage::prepare(Some(root.into_os_string()), || Ok(base)).unwrap();
        (directory, storage)
    }

    #[test]
    #[cfg(windows)]
    fn private_font_opened_handle_cannot_impersonate_an_allowed_path() {
        let (directory, storage) = fixture();
        let allowed = storage.0.as_ref().unwrap().documents.join("allowed.otf");
        let outside = directory.path().join("outside.otf");
        fs::write(&allowed, b"OTTO12345678").unwrap();
        fs::write(&outside, b"OTTOabcdefgh").unwrap();
        let allowed = storage.document_path(&allowed, true).unwrap();
        let safe_handle = fs::File::open(&allowed).unwrap();
        assert!(storage.verify_private_font_handle(&safe_handle, &allowed).is_ok());
        let outside_handle = fs::File::open(&outside).unwrap();
        assert!(storage.verify_private_font_handle(&outside_handle, &allowed).is_err());
        assert_eq!(fs::read(&outside).unwrap(), b"OTTOabcdefgh");
    }

    #[test]
    #[cfg(windows)]
    fn fresh_root_and_native_roundtrip_stay_in_disposable_documents() {
        let (_fixture, storage) = fixture();
        let paths = storage.0.as_ref().unwrap();
        assert!(paths.webview.is_dir());
        let file = paths.documents.join("日本語 project.json");
        storage.write_bytes(&file, b"{\"roundtrip\":1}").unwrap();
        assert_eq!(storage.read_project(&file).unwrap(), "{\"roundtrip\":1}");
        storage.write_bytes(&file, b"{\"roundtrip\":2}").unwrap();
        assert_eq!(storage.read_project(&file).unwrap(), "{\"roundtrip\":2}");
        assert_eq!(fs::read_dir(&paths.documents).unwrap().count(), 1);
        assert_eq!(fs::read_dir(&paths.root).unwrap().count(), 2);
    }

    #[test]
    #[cfg(windows)]
    fn existing_root_wrong_parent_and_bad_name_preserve_existing_data() {
        let (fixture, storage) = fixture();
        let paths = storage.0.as_ref().unwrap();
        let base = canonical_directory(fixture.path()).unwrap();
        let sentinel = paths.documents.join("existing.json");
        fs::write(&sentinel, b"preserve").unwrap();
        let again = RunStorage::prepare(Some(paths.root.clone().into_os_string()), || {
            Ok(base.clone())
        });
        assert!(again.is_err());
        let other = tempfile::tempdir().unwrap();
        let outside = canonical_directory(other.path())
            .unwrap()
            .join("nect-svg-acceptance-other");
        let wrong =
            RunStorage::prepare(Some(outside.clone().into_os_string()), || Ok(base.clone()));
        assert!(wrong.is_err());
        assert!(!outside.exists());
        let bad = base.join("normal-profile");
        let invalid = RunStorage::prepare(Some(bad.clone().into_os_string()), || Ok(base.clone()));
        assert!(invalid.is_err());
        assert!(!bad.exists());
        assert_eq!(fs::read(sentinel).unwrap(), b"preserve");
    }

    #[test]
    #[cfg(windows)]
    fn outside_traversal_subdirectory_and_stream_targets_cannot_write() {
        let (fixture, storage) = fixture();
        let paths = storage.0.as_ref().unwrap();
        let outside = canonical_directory(fixture.path())
            .unwrap()
            .join("outside.json");
        fs::write(&outside, b"preserve").unwrap();
        let nested = paths.documents.join("nested");
        fs::create_dir(&nested).unwrap();
        for target in [
            outside.clone(),
            paths.documents.join(r"..\outside.json"),
            nested.join("file.json"),
            paths.documents.join("file.json:stream"),
            paths.webview.join("file.json"),
        ] {
            assert!(storage.write_bytes(&target, b"bad").is_err());
            assert!(storage.read_project(&target).is_err());
            assert!(storage.read_private_font(&target).is_err());
        }
        assert_eq!(fs::read(outside).unwrap(), b"preserve");
        assert_eq!(fs::read_dir(&paths.documents).unwrap().count(), 1);
    }

    #[test]
    #[cfg(windows)]
    fn junction_replacement_is_refused_before_native_writes() {
        use std::process::Command;
        let (_fixture, storage) = fixture();
        let paths = storage.0.as_ref().unwrap();
        let outside = tempfile::tempdir().unwrap();
        let sentinel = outside.path().join("preserve.json");
        fs::write(&sentinel, b"unchanged").unwrap();
        fs::remove_dir(&paths.documents).unwrap();
        let result = Command::new("cmd.exe")
            .args(["/D", "/C", "mklink", "/J"])
            .arg(&paths.documents)
            .arg(outside.path())
            .output()
            .unwrap();
        assert!(
            result.status.success(),
            "junction fixture failed: {result:?}"
        );
        assert!(storage
            .write_bytes(&paths.documents.join("preserve.json"), b"bad")
            .is_err());
        assert!(storage
            .read_project(&paths.documents.join("preserve.json"))
            .is_err());
        assert!(storage.read_private_font(&paths.documents.join("preserve.json")).is_err());
        assert_eq!(fs::read(sentinel).unwrap(), b"unchanged");
        fs::remove_dir(&paths.documents).unwrap();
    }

    #[test]
    fn normal_native_file_adapter_retains_existing_transaction_behavior() {
        let fixture = tempfile::tempdir().unwrap();
        let file = fixture.path().join("normal.json");
        let storage = RunStorage::default();
        storage.write_bytes(&file, b"normal").unwrap();
        assert_eq!(storage.read_project(&file).unwrap(), "normal");
        assert_eq!(fs::read_dir(fixture.path()).unwrap().count(), 1);
    }
}

