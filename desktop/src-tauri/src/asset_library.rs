//! Registered SVG folders. No IPC accepts an absolute path. Authored state stays in JS Session.
use serde::{Deserialize, Serialize};
use std::{
    collections::HashMap,
    fs::{self, File},
    io::{Read, Write},
    path::{Component, Path, PathBuf},
    sync::Mutex,
};

const MAX_ROOTS: usize = 16;
const MAX_ENTRIES: usize = 2048;
const MAX_BYTES: u64 = 12_000_000;
fn asset_extension(path: &Path) -> bool {
    path.extension().is_some_and(|s| {
        ["svg", "png", "jpg", "jpeg"]
            .iter()
            .any(|ext| s.eq_ignore_ascii_case(ext))
    })
}
const MAX_TOTAL: usize = 32_000_000;

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Root {
    pub id: String,
    pub name: String,
    path: PathBuf,
    identity: String,
}
#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Item {
    pub relative_path: String,
    pub bytes: usize,
}
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Listing {
    pub root: Root,
    pub generation: String,
    pub items: Vec<Item>,
    pub warnings: Vec<String>,
}
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Opened {
    pub bytes: Vec<u8>,
    pub source_location: String,
}
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Saved {
    version: u8,
    roots: Vec<Root>,
}
struct Snapshot {
    generation: String,
    files: HashMap<String, (String, Vec<u8>)>,
}
#[derive(Default)]
pub struct Registry {
    file: Option<PathBuf>,
    roots: Vec<Root>,
    snapshots: HashMap<String, Snapshot>,
    sequence: u64,
    error: Option<String>,
}
#[derive(Default)]
pub struct AssetLibrary(pub Mutex<Registry>);

fn plain(value: &str, max: usize) -> bool {
    !value.is_empty() && value.len() <= max && !value.chars().any(char::is_control)
}
fn no_link(path: &Path) -> Result<(), String> {
    let m = fs::symlink_metadata(path).map_err(|e| e.to_string())?;
    if m.file_type().is_symlink() {
        return Err("Symbolic links are not library sources".into());
    }
    #[cfg(windows)]
    {
        use std::os::windows::fs::MetadataExt;
        if m.file_attributes() & 0x400 != 0 {
            return Err("Reparse points are not library sources".into());
        }
    }
    Ok(())
}
fn open_handle(path: &Path, directory: bool) -> Result<File, String> {
    let mut options = File::options();
    options.read(true);
    #[cfg(windows)]
    {
        use std::os::windows::fs::OpenOptionsExt;
        // Deny delete/write sharing during the identity+content read.
        options.share_mode(1);
        if directory {
            options.custom_flags(0x02000000);
        }
    }
    #[cfg(not(windows))]
    let _ = directory;
    options.open(path).map_err(|e| e.to_string())
}
fn handle_identity(file: &File, expected: &Path) -> Result<String, String> {
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
            fn GetFileInformationByHandle(file: *mut std::ffi::c_void, info: *mut u32) -> i32;
        }
        let mut path = vec![0u16; 32768];
        let length = unsafe {
            GetFinalPathNameByHandleW(
                file.as_raw_handle(),
                path.as_mut_ptr(),
                path.len() as u32,
                0,
            )
        } as usize;
        if length == 0
            || length >= path.len()
            || PathBuf::from(std::ffi::OsString::from_wide(&path[..length])) != expected
        {
            return Err("Opened library path changed or escaped".into());
        }
        let mut info = [0u32; 13];
        if unsafe { GetFileInformationByHandle(file.as_raw_handle(), info.as_mut_ptr()) } == 0 {
            return Err("Cannot verify library file identity".into());
        }
        Ok(format!(
            "{}-{}-{}-{}-{}",
            info[7], info[11], info[12], info[1], info[2]
        ))
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        let m = file.metadata().map_err(|e| e.to_string())?;
        let p = fs::metadata(expected).map_err(|e| e.to_string())?;
        if m.dev() != p.dev() || m.ino() != p.ino() {
            return Err("Opened library path changed".into());
        }
        Ok(format!("{}-{}", m.dev(), m.ino()))
    }
    #[cfg(not(any(windows, unix)))]
    {
        let _ = (file, expected);
        Err("Library handle verification unavailable".into())
    }
}
fn relative_path(root: &Root, relative: &str) -> Result<PathBuf, String> {
    if !plain(relative, 2048)
        || relative.contains(['\\', ':'])
        || relative.starts_with('/')
        || relative
            .split('/')
            .any(|p| p.is_empty() || p == "." || p == "..")
    {
        return Err("Invalid library relative path".into());
    }
    let mut path = root.path.clone();
    for part in Path::new(relative).components() {
        let Component::Normal(p) = part else {
            return Err("Invalid library relative path".into());
        };
        path.push(p);
        no_link(&path)?;
    }
    let canonical = path.canonicalize().map_err(|e| e.to_string())?;
    if !canonical.starts_with(&root.path) || canonical == root.path {
        return Err("Library source escaped registered folder".into());
    }
    Ok(canonical)
}
fn check_root(root: &Root) -> Result<File, String> {
    no_link(&root.path)?;
    if root.path.canonicalize().map_err(|e| e.to_string())? != root.path {
        return Err("Registered folder moved".into());
    }
    let file = open_handle(&root.path, true)?;
    if !file.metadata().map_err(|e| e.to_string())?.is_dir()
        || handle_identity(&file, &root.path)? != root.identity
    {
        return Err("Registered folder was replaced; register it again".into());
    }
    Ok(file)
}
fn read_source(root: &Root, relative: &str) -> Result<(String, Vec<u8>, PathBuf), String> {
    let _root_handle = check_root(root)?;
    let path = relative_path(root, relative)?;
    if !asset_extension(&path) {
        return Err("Choose an SVG, PNG or JPEG source".into());
    }
    let mut file = open_handle(&path, false)?;
    let identity = handle_identity(&file, &path)?;
    let m = file.metadata().map_err(|e| e.to_string())?;
    if !m.is_file() || m.len() > MAX_BYTES {
        return Err("Asset exceeds 12 MB or is not a file".into());
    }
    let mut bytes = Vec::new();
    Read::by_ref(&mut file)
        .take(MAX_BYTES + 1)
        .read_to_end(&mut bytes)
        .map_err(|e| e.to_string())?;
    if bytes.len() as u64 != m.len() || bytes.len() as u64 > MAX_BYTES {
        return Err("SVG changed while reading".into());
    }
    // Recheck the complete relative chain and root after reading the open handle.
    if relative_path(root, relative)? != path || handle_identity(&file, &path)? != identity {
        return Err("SVG source changed while reading".into());
    }
    check_root(root)?;
    Ok((
        format!("{}-{:?}-{}", identity, m.modified().ok(), m.len()),
        bytes,
        path,
    ))
}
impl Registry {
    pub fn initialize(&mut self, file: Result<PathBuf, String>) {
        let result = (|| {
            self.file = Some(file?);
            let file = self.file.as_ref().unwrap();
            if !file.exists() {
                return Ok(());
            }
            let mut data = Vec::new();
            File::open(file)
                .map_err(|e| e.to_string())?
                .take(100_001)
                .read_to_end(&mut data)
                .map_err(|e| e.to_string())?;
            if data.len() > 100_000 {
                return Err("Library registry exceeds limit".into());
            }
            let saved: Saved = serde_json::from_slice(&data).map_err(|e| e.to_string())?;
            if saved.version != 1 || saved.roots.len() > MAX_ROOTS {
                return Err("Unsupported library registry".into());
            }
            let mut ids = std::collections::HashSet::new();
            for r in &saved.roots {
                if !plain(&r.id, 100)
                    || !plain(&r.name, 255)
                    || !r.path.is_absolute()
                    || !ids.insert(&r.id)
                {
                    return Err("Invalid registered library folder".into());
                }
            }
            self.roots = saved.roots;
            Ok(())
        })();
        self.error = result.err();
    }
    fn ready(&self) -> Result<(), String> {
        self.error.as_ref().map_or(Ok(()), |e| Err(e.clone()))
    }
    fn persist(&self, roots: &[Root]) -> Result<(), String> {
        self.ready()?;
        let path = self.file.as_ref().ok_or("Library storage unavailable")?;
        let parent = path.parent().ok_or("Missing library storage directory")?;
        fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        let mut file = tempfile::NamedTempFile::new_in(parent).map_err(|e| e.to_string())?;
        let data = serde_json::to_vec(&Saved {
            version: 1,
            roots: roots.to_vec(),
        })
        .map_err(|e| e.to_string())?;
        file.write_all(&data).map_err(|e| e.to_string())?;
        file.as_file().sync_all().map_err(|e| e.to_string())?;
        file.persist(path).map_err(|e| e.to_string())?;
        Ok(())
    }
    fn register(&mut self, path: PathBuf) -> Result<Vec<Root>, String> {
        self.ready()?;
        no_link(&path)?;
        let path = path.canonicalize().map_err(|e| e.to_string())?;
        let file = open_handle(&path, true)?;
        if !file.metadata().map_err(|e| e.to_string())?.is_dir() {
            return Err("Choose a folder".into());
        }
        let identity = handle_identity(&file, &path)?;
        if self.roots.iter().any(|r| r.identity == identity) {
            return Ok(self.roots.clone());
        }
        if self.roots.len() >= MAX_ROOTS {
            return Err("At most 16 library folders".into());
        }
        self.sequence += 1;
        let id = format!(
            "folder-{}-{}",
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map_err(|e| e.to_string())?
                .as_nanos(),
            self.sequence
        );
        let name = path
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .into_owned();
        if !plain(&name, 255) {
            return Err("Invalid folder name".into());
        }
        let mut roots = self.roots.clone();
        roots.push(Root {
            id,
            name,
            path,
            identity,
        });
        self.persist(&roots)?;
        self.roots = roots;
        Ok(self.roots.clone())
    }
    fn refresh(&mut self, id: &str) -> Result<Listing, String> {
        self.ready()?;
        // An unsuccessful refresh invalidates old byte tickets as well.
        self.snapshots.remove(id);
        let root = self
            .roots
            .iter()
            .find(|r| r.id == id)
            .ok_or("Unknown library folder")?
            .clone();
        let _handle = check_root(&root)?;
        let mut pending = vec![(root.path.clone(), 0)];
        let mut count = 0;
        let mut files = HashMap::new();
        let mut items = vec![];
        let mut warnings = vec![];
        let mut total = 0;
        while let Some((dir, depth)) = pending.pop() {
            no_link(&dir)?;
            let directory_handle = open_handle(&dir, true)?;
            handle_identity(&directory_handle, &dir)?;
            for entry in fs::read_dir(dir).map_err(|e| e.to_string())? {
                count += 1;
                if count > MAX_ENTRIES {
                    return Err("Folder exceeds 2048 entries; register a smaller folder".into());
                }
                let path = entry.map_err(|e| e.to_string())?.path();
                if no_link(&path).is_err() {
                    if warnings.len() < 32 {
                        warnings.push("Skipped symbolic link or reparse point".into());
                    }
                    continue;
                }
                let relative = path
                    .strip_prefix(&root.path)
                    .map_err(|e| e.to_string())?
                    .to_str()
                    .ok_or("Library filename is not Unicode")?
                    .replace('\\', "/");
                if path.is_dir() {
                    if depth >= 8 {
                        return Err("Folder exceeds 8 nested directories".into());
                    }
                    relative_path(&root, &relative)?;
                    pending.push((path, depth + 1));
                } else if asset_extension(&path) {
                    match read_source(&root, &relative) {
                        Ok((identity, bytes, _)) => {
                            total += bytes.len();
                            if total > MAX_TOTAL {
                                return Err("Folder asset content exceeds 32 MB".into());
                            }
                            items.push(Item {
                                relative_path: relative.clone(),
                                bytes: bytes.len(),
                            });
                            files.insert(relative, (identity, bytes));
                        }
                        Err(e) => {
                            if warnings.len() < 32 {
                                warnings.push(format!("{relative}: {e}"));
                            }
                        }
                    }
                }
            }
        }
        check_root(&root)?;
        items.sort_by(|a, b| a.relative_path.cmp(&b.relative_path));
        self.sequence += 1;
        let generation = self.sequence.to_string();
        self.snapshots.insert(
            id.into(),
            Snapshot {
                generation: generation.clone(),
                files,
            },
        );
        Ok(Listing {
            root,
            generation,
            items,
            warnings,
        })
    }
    fn open(&self, id: &str, relative: &str, generation: &str) -> Result<Opened, String> {
        self.ready()?;
        let root = self
            .roots
            .iter()
            .find(|r| r.id == id)
            .ok_or("Unknown library folder")?;
        let snapshot = self
            .snapshots
            .get(id)
            .filter(|s| s.generation == generation)
            .ok_or("Refresh the library folder before opening assets")?;
        let (identity, accepted) = snapshot
            .files
            .get(relative)
            .ok_or("Asset is missing from this folder refresh")?;
        let (current, bytes, path) = read_source(root, relative)?;
        if &current != identity || &bytes != accepted {
            return Err("Asset changed; refresh the library folder".into());
        }
        Ok(Opened {
            bytes,
            source_location: path.to_string_lossy().into_owned(),
        })
    }
}

#[tauri::command]
pub fn list_asset_folders(state: tauri::State<'_, AssetLibrary>) -> Result<Vec<Root>, String> {
    let r = state.0.lock().map_err(|_| "Library unavailable")?;
    r.ready()?;
    Ok(r.roots.clone())
}
#[tauri::command]
pub fn register_asset_folder(
    window: tauri::WebviewWindow,
    storage: tauri::State<'_, crate::acceptance_storage::RunStorage>,
    state: tauri::State<'_, AssetLibrary>,
) -> Result<Option<Vec<Root>>, String> {
    let Some(path) = storage.dialog(&window).pick_folder() else {
        return Ok(None);
    };
    if let Some(paths) = &storage.0 {
        paths.check_directories()?;
        if !path.starts_with(paths.webview.parent().unwrap().parent().unwrap()) {
            return Err("Acceptance folders must be inside the isolated run root".into());
        }
    }
    state
        .0
        .lock()
        .map_err(|_| "Library unavailable")?
        .register(path)
        .map(Some)
}
#[tauri::command]
pub fn remove_asset_folder(
    root_id: String,
    state: tauri::State<'_, AssetLibrary>,
) -> Result<Vec<Root>, String> {
    let mut r = state.0.lock().map_err(|_| "Library unavailable")?;
    let roots: Vec<_> = r
        .roots
        .iter()
        .filter(|s| s.id != root_id)
        .cloned()
        .collect();
    r.persist(&roots)?;
    r.roots = roots;
    r.snapshots.remove(&root_id);
    Ok(r.roots.clone())
}
#[tauri::command]
pub async fn refresh_asset_folder(
    root_id: String,
    state: tauri::State<'_, AssetLibrary>,
) -> Result<Listing, String> {
    state
        .0
        .lock()
        .map_err(|_| "Library unavailable")?
        .refresh(&root_id)
}
#[tauri::command]
pub async fn open_asset_source(
    root_id: String,
    relative_path: String,
    generation: String,
    state: tauri::State<'_, AssetLibrary>,
) -> Result<Opened, String> {
    state
        .0
        .lock()
        .map_err(|_| "Library unavailable")?
        .open(&root_id, &relative_path, &generation)
}

#[cfg(test)]
mod tests {
    use super::*;
    fn fixture() -> (tempfile::TempDir, Registry, PathBuf) {
        let t = tempfile::tempdir().unwrap();
        let dir = t.path().join("assets");
        fs::create_dir(&dir).unwrap();
        fs::write(dir.join("mark.svg"), b"<svg/>").unwrap();
        let mut r = Registry::default();
        r.initialize(Ok(t.path().join("settings/library.json")));
        r.register(dir.clone()).unwrap();
        (t, r, dir)
    }
    #[test]
    fn persistent_roots_exact_refresh_open_and_changed_missing_refusal() {
        let (_t, mut r, dir) = fixture();
        let id = r.roots[0].id.clone();
        let listing = r.refresh(&id).unwrap();
        assert_eq!(listing.items.len(), 1);
        assert_eq!(
            r.open(&id, "mark.svg", &listing.generation).unwrap().bytes,
            b"<svg/>"
        );
        let mut restart = Registry::default();
        restart.initialize(Ok(r.file.clone().unwrap()));
        assert_eq!(restart.roots[0].id, id);
        assert!(restart.open(&id, "mark.svg", &listing.generation).is_err());
        for bad in [
            "../mark.svg",
            "/mark.svg",
            "C:/mark.svg",
            "a/../mark.svg",
            "a\\mark.svg",
        ] {
            assert!(relative_path(&r.roots[0], bad).is_err());
        }
        fs::write(dir.join("mark.svg"), b"<svg>changed</svg>").unwrap();
        assert!(r.open(&id, "mark.svg", &listing.generation).is_err());
        let new = r.refresh(&id).unwrap();
        assert!(r.open(&id, "mark.svg", &listing.generation).is_err());
        assert!(r.open(&id, "mark.svg", &new.generation).is_ok());
        fs::remove_file(dir.join("mark.svg")).unwrap();
        assert!(r.open(&id, "mark.svg", &new.generation).is_err());
    }
    #[test]
    fn replaced_root_and_settings_failure_do_not_admit_sources() {
        let (_t, mut r, dir) = fixture();
        let id = r.roots[0].id.clone();
        let old = r.refresh(&id).unwrap();
        fs::rename(&dir, dir.with_extension("old")).unwrap();
        fs::create_dir(&dir).unwrap();
        fs::write(dir.join("mark.svg"), b"<svg/>").unwrap();
        assert!(r.refresh(&id).is_err());
        assert!(r.open(&id, "mark.svg", &old.generation).is_err());
        let mut broken = Registry::default();
        broken.initialize(Ok(dir.join("mark.svg/no-settings")));
        assert!(broken.register(dir).is_err());
        assert!(broken.roots.is_empty());
    }
    #[test]
    fn same_names_are_root_relative_and_same_byte_replacements_are_rejected() {
        let (t, mut r, dir) = fixture();
        fs::create_dir(dir.join("marks")).unwrap();
        fs::write(dir.join("marks/mark.svg"), b"<svg><rect/></svg>").unwrap();
        let other = t.path().join("other");
        fs::create_dir(&other).unwrap();
        fs::write(other.join("mark.svg"), b"<svg><circle/></svg>").unwrap();
        let id = r.roots[0].id.clone();
        r.register(other).unwrap();
        let other_id = r.roots[1].id.clone();
        let first = r.refresh(&id).unwrap();
        let second = r.refresh(&other_id).unwrap();
        assert_eq!(first.items.len(), 2);
        assert_ne!(
            r.open(&id, "mark.svg", &first.generation).unwrap().bytes,
            r.open(&other_id, "mark.svg", &second.generation)
                .unwrap()
                .bytes
        );
        assert!(r.open("unknown", "mark.svg", &first.generation).is_err());
        fs::rename(dir.join("mark.svg"), dir.join("old.svg")).unwrap();
        fs::write(dir.join("mark.svg"), b"<svg/>").unwrap();
        assert!(r.open(&id, "mark.svg", &first.generation).is_err());
    }
    #[test]
    fn nesting_and_total_content_limits_invalidate_the_old_catalog() {
        let (_t, mut r, dir) = fixture();
        let id = r.roots[0].id.clone();
        let old = r.refresh(&id).unwrap();
        let mut deep = dir.clone();
        for _ in 0..9 {
            deep.push("sub");
            fs::create_dir(&deep).unwrap();
        }
        assert!(r.refresh(&id).is_err());
        assert!(r.open(&id, "mark.svg", &old.generation).is_err());
        fs::remove_dir(&deep).unwrap();
        for i in 0..3 {
            fs::write(
                dir.join(format!("large{i}.svg")),
                vec![b' '; MAX_BYTES as usize],
            )
            .unwrap();
        }
        assert!(r.refresh(&id).is_err());
    }
    #[cfg(windows)]
    #[test]
    fn case_variant_registration_does_not_duplicate_a_root() {
        let (_t, mut r, dir) = fixture();
        r.register(PathBuf::from(dir.to_string_lossy().to_uppercase()))
            .unwrap();
        assert_eq!(r.roots.len(), 1);
    }
    #[test]
    fn size_and_entry_limits_are_explicit() {
        let (_t, mut r, dir) = fixture();
        let id = r.roots[0].id.clone();
        fs::write(dir.join("large.svg"), vec![0; MAX_BYTES as usize + 1]).unwrap();
        let listing = r.refresh(&id).unwrap();
        assert_eq!(listing.items.len(), 1);
        assert!(!listing.warnings.is_empty());
        for i in 0..MAX_ENTRIES {
            fs::write(dir.join(format!("{i}.txt")), b"").unwrap();
        }
        assert!(r.refresh(&id).is_err());
    }
    #[cfg(windows)]
    #[test]
    fn junction_escape_is_skipped_and_cannot_be_opened() {
        let (t, mut r, dir) = fixture();
        let outside = t.path().join("outside");
        fs::create_dir(&outside).unwrap();
        fs::write(outside.join("private.svg"), b"<svg/>").unwrap();
        let link = dir.join("escape");
        let output = std::process::Command::new("cmd")
            .args(["/C", "mklink", "/J"])
            .arg(&link)
            .arg(&outside)
            .output()
            .unwrap();
        assert!(output.status.success());
        let id = r.roots[0].id.clone();
        let listing = r.refresh(&id).unwrap();
        assert_eq!(listing.items.len(), 1);
        assert!(!listing.warnings.is_empty());
        assert!(relative_path(&r.roots[0], "escape/private.svg").is_err());
        // Remove the junction itself, never traverse the target on cleanup.
        fs::remove_dir(link).unwrap();
    }
}
