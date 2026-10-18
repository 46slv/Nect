//! App-owned recovery copies. Never reads or writes a project binding.
use serde::{Deserialize, Serialize};
use std::{
    collections::{HashMap, HashSet},
    fs::{self, File, OpenOptions},
    io::Write,
    path::{Path, PathBuf},
    sync::{Arc, Mutex},
    time::{SystemTime, UNIX_EPOCH},
};

const MAX_CONTENTS: usize = 50_000_000;
const MAX_RECORD: u64 = 110_000_000;
#[derive(Serialize, Deserialize, Clone)]
#[serde(rename_all = "camelCase")]
pub struct CopyRecord {
    version: u32,
    id: String,
    window: String,
    name: String,
    saved_at: u64,
    recoverable: bool,
    contents: String,
}
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CopyInfo {
    id: String,
    name: String,
    saved_at: u64,
    previous: bool,
}
pub struct Store {
    root: PathBuf,
    run: PathBuf,
    _lease: File,
    next: u64,
    owners: HashMap<String, String>,
    offered: HashSet<String>,
}
#[derive(Clone, Default)]
pub struct Recovery(pub Arc<Mutex<Option<Result<Store, String>>>>);
fn now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}
fn plain(path: &Path, directory: bool) -> Result<(), String> {
    let m = fs::symlink_metadata(path).map_err(|e| e.to_string())?;
    #[cfg(windows)]
    {
        use std::os::windows::fs::MetadataExt;
        if m.file_attributes() & 0x400 != 0 {
            return Err("Recovery path is a reparse point".into());
        }
    }
    if m.file_type().is_symlink() || m.is_dir() != directory {
        return Err("Invalid recovery path".into());
    }
    Ok(())
}
fn lease(path: &Path) -> std::io::Result<File> {
    let mut o = OpenOptions::new();
    o.read(true).write(true).create(true);
    #[cfg(windows)]
    {
        use std::os::windows::fs::OpenOptionsExt;
        o.share_mode(0);
    }
    o.open(path)
}
fn atomic(path: &Path, bytes: &[u8]) -> Result<(), String> {
    if path.exists() {
        plain(path, false)?;
    }
    let mut staged =
        tempfile::NamedTempFile::new_in(path.parent().ok_or("Invalid recovery folder")?)
            .map_err(|e| e.to_string())?;
    staged.write_all(bytes).map_err(|e| e.to_string())?;
    staged.as_file().sync_all().map_err(|e| e.to_string())?;
    staged.persist(path).map_err(|e| e.to_string())?;
    Ok(())
}
impl Store {
    fn new(root: PathBuf) -> Result<Self, String> {
        fs::create_dir_all(&root).map_err(|e| e.to_string())?;
        plain(&root, true)?;
        let root = fs::canonicalize(root).map_err(|e| e.to_string())?;
        let run = tempfile::Builder::new()
            .prefix("run-")
            .tempdir_in(&root)
            .map_err(|e| e.to_string())?
            .keep();
        let lock = lease(&run.join("alive.lock")).map_err(|e| e.to_string())?;
        Ok(Self {
            root,
            run,
            _lease: lock,
            next: 0,
            owners: HashMap::new(),
            offered: HashSet::new(),
        })
    }
    fn check(&self) -> Result<(), String> {
        plain(&self.root, true)?;
        plain(&self.run, true)
    }
    fn begin(&mut self, window: &str) -> Result<String, String> {
        self.check()?;
        self.next += 1;
        let id = format!(
            "{}/d-{}",
            self.run.file_name().unwrap().to_string_lossy(),
            self.next
        );
        self.owners.insert(id.clone(), window.into());
        Ok(id)
    }
    fn base(&self, id: &str) -> Result<PathBuf, String> {
        let parts: Vec<_> = id.split('/').collect();
        if parts.len() != 2
            || !parts[0].starts_with("run-")
            || !parts[1].starts_with("d-")
            || parts
                .iter()
                .any(|s| s.len() > 80 || !s.chars().all(|c| c.is_ascii_alphanumeric() || c == '-'))
        {
            return Err("Invalid recovery identity".into());
        }
        self.check()?;
        let dir = self.root.join(parts[0]);
        plain(&dir, true)?;
        Ok(dir.join(parts[1]))
    }
    fn record(&self, id: &str) -> Result<(CopyRecord, bool), String> {
        let base = self.base(id)?;
        for (suffix, previous) in [("json", false), ("previous.json", true)] {
            let file = base.with_extension(suffix);
            let loaded: Result<CopyRecord, String> = (|| {
                plain(&file, false)?;
                if fs::metadata(&file).map_err(|e| e.to_string())?.len() > MAX_RECORD {
                    return Err("Recovery copy exceeds its limit".into());
                }
                let record: CopyRecord =
                    serde_json::from_slice(&fs::read(file).map_err(|e| e.to_string())?)
                        .map_err(|e| e.to_string())?;
                if record.version != 1 || record.id != id || record.contents.len() > MAX_CONTENTS {
                    return Err("Invalid recovery copy".into());
                }
                Ok(record)
            })();
            if let Ok(record) = loaded {
                return Ok((record, previous));
            }
        }
        Err("Both recovery copies are unreadable; existing files preserved".into())
    }
    fn write(
        &mut self,
        window: &str,
        id: &str,
        contents: String,
        name: String,
        recoverable: bool,
    ) -> Result<u64, String> {
        if self.owners.get(id).map(String::as_str) != Some(window) {
            return Err("Recovery copy belongs to another document/window".into());
        }
        if contents.len() > MAX_CONTENTS || name.len() > 1024 {
            return Err("Recovery document exceeds 50 MB".into());
        }
        let parsed: serde_json::Value =
            serde_json::from_str(&contents).map_err(|e| e.to_string())?;
        if !parsed.is_object() {
            return Err("Invalid recovery document".into());
        }
        let base = self.base(id)?;
        // Preserve the previous validated generation before replacing the current one.
        if let Ok((last, _)) = self.record(id) {
            atomic(
                &base.with_extension("previous.json"),
                &serde_json::to_vec(&last).map_err(|e| e.to_string())?,
            )?;
        }
        let saved_at = now();
        let record = CopyRecord {
            version: 1,
            id: id.into(),
            window: window.into(),
            name,
            saved_at,
            recoverable,
            contents,
        };
        atomic(
            &base.with_extension("json"),
            &serde_json::to_vec(&record).map_err(|e| e.to_string())?,
        )?;
        Ok(saved_at)
    }
    fn list(&mut self) -> Result<Vec<CopyInfo>, String> {
        self.check()?;
        let mut result = Vec::new();
        self.offered.clear();
        for entry in fs::read_dir(&self.root)
            .map_err(|e| e.to_string())?
            .flatten()
        {
            let dir = entry.path();
            if dir == self.run
                || plain(&dir, true).is_err()
                || !entry.file_name().to_string_lossy().starts_with("run-")
            {
                continue;
            }
            let lock_path = dir.join("alive.lock");
            if plain(&lock_path, false).is_err() {
                continue;
            }
            let Ok(_inactive_lease) = lease(&lock_path) else {
                continue;
            };
            let mut ids = HashSet::new();
            for file in fs::read_dir(&dir).map_err(|e| e.to_string())?.flatten() {
                let name = file.file_name().to_string_lossy().into_owned();
                if let Some(stem) = name
                    .strip_suffix(".previous.json")
                    .or_else(|| name.strip_suffix(".json"))
                {
                    ids.insert(format!("{}/{}", entry.file_name().to_string_lossy(), stem));
                }
            }
            for id in ids {
                let Ok(base) = self.base(&id) else {
                    continue;
                };
                if base.with_extension("restored").exists() {
                    continue;
                }
                if let Ok((r, previous)) = self.record(&id) {
                    if r.recoverable {
                        self.offered.insert(id.clone());
                        result.push(CopyInfo {
                            id,
                            name: r.name,
                            saved_at: r.saved_at,
                            previous,
                        });
                    }
                }
            }
        }
        result.sort_by(|a, b| b.saved_at.cmp(&a.saved_at));
        Ok(result)
    }
}
impl Recovery {
    pub fn initialize(&self, root: Result<PathBuf, String>) {
        if let Ok(mut state) = self.0.lock() {
            *state = Some(root.and_then(Store::new));
        }
    }
    fn access<T>(&self, op: impl FnOnce(&mut Store) -> Result<T, String>) -> Result<T, String> {
        let mut state = self.0.lock().map_err(|_| "Recovery storage busy")?;
        match state.as_mut().ok_or("Recovery storage not initialized")? {
            Ok(store) => op(store),
            Err(e) => Err(e.clone()),
        }
    }
}
#[tauri::command]
pub fn begin_recovery(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, Recovery>,
) -> Result<String, String> {
    state.access(|s| s.begin(window.label()))
}
#[tauri::command]
pub async fn write_recovery(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, Recovery>,
    id: String,
    contents: String,
    name: String,
    recoverable: bool,
) -> Result<u64, String> {
    let state = state.inner().clone();
    let label = window.label().to_owned();
    tauri::async_runtime::spawn_blocking(move || {
        state.access(|s| s.write(&label, &id, contents, name, recoverable))
    })
    .await
    .map_err(|e| e.to_string())?
}
#[tauri::command]
pub async fn list_recovery(state: tauri::State<'_, Recovery>) -> Result<Vec<CopyInfo>, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || state.access(Store::list))
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
pub fn read_recovery(state: tauri::State<'_, Recovery>, id: String) -> Result<String, String> {
    state.access(|s| {
        if !s.offered.contains(&id) {
            return Err("Choose a listed recovery copy".into());
        }
        Ok(s.record(&id)?.0.contents)
    })
}
#[tauri::command]
pub fn acknowledge_recovery(state: tauri::State<'_, Recovery>, id: String) -> Result<(), String> {
    state.access(|s| {
        if !s.offered.contains(&id) {
            return Err("Unknown recovery choice".into());
        }
        atomic(
            &s.base(&id)?.with_extension("restored"),
            b"Recovered into another verified recovery copy",
        )
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn document_window_isolation_complete_bytes_and_two_generations() {
        let dir = tempfile::tempdir().unwrap();
        let mut s = Store::new(dir.path().join("recovery")).unwrap();
        let a = s.begin("main").unwrap();
        let b = s.begin("second").unwrap();
        let large = serde_json::json!({"embeddedImage": "abcdef".repeat(100_000), "embeddedFont": "日本語".repeat(10000)}).to_string();
        s.write("main", &a, large.clone(), "文書".into(), true)
            .unwrap();
        assert!(s
            .write("second", &a, "{}".into(), "wrong".into(), true)
            .is_err());
        s.write("second", &b, "{\"other\":1}".into(), "other".into(), true)
            .unwrap();
        s.write("main", &a, "{\"new\":2}".into(), "文書".into(), true)
            .unwrap();
        assert_eq!(s.record(&a).unwrap().0.contents, "{\"new\":2}");
        let base = s.base(&a).unwrap();
        fs::write(base.with_extension("json"), b"interrupted bytes").unwrap();
        let (fallback, previous) = s.record(&a).unwrap();
        assert!(previous);
        assert_eq!(fallback.contents, large);
        assert_eq!(s.record(&b).unwrap().0.contents, "{\"other\":1}");
    }
    #[test]
    fn failed_replacement_keeps_last_good_copy_and_does_not_write_source() {
        let dir = tempfile::tempdir().unwrap();
        let source = dir.path().join("manual.json");
        fs::write(&source, b"original").unwrap();
        let mut s = Store::new(dir.path().join("recovery")).unwrap();
        let id = s.begin("main").unwrap();
        s.write(
            "main",
            &id,
            "{\"first\":1}".into(),
            "manual.json".into(),
            true,
        )
        .unwrap();
        s.write(
            "main",
            &id,
            "{\"second\":2}".into(),
            "manual.json".into(),
            true,
        )
        .unwrap();
        let base = s.base(&id).unwrap();
        // A destination replaced by a directory models a deterministic write failure.
        fs::remove_file(base.with_extension("json")).unwrap();
        fs::create_dir(base.with_extension("json")).unwrap();
        assert!(s
            .write(
                "main",
                &id,
                "{\"failed\":3}".into(),
                "manual.json".into(),
                true
            )
            .is_err());
        assert_eq!(s.record(&id).unwrap().0.contents, "{\"first\":1}");
        assert_eq!(fs::read(source).unwrap(), b"original");
        assert!(s.base("../../manual").is_err());
    }
    #[test]
    #[cfg(windows)]
    fn restart_lists_inactive_only_and_keeps_clean_and_acknowledged_data() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path().join("recovery");
        let mut old = Store::new(root.clone()).unwrap();
        let id = old.begin("main").unwrap();
        old.write("main", &id, "{\"draft\":1}".into(), "Draft".into(), true)
            .unwrap();
        let mut next = Store::new(root.clone()).unwrap();
        assert!(next.list().unwrap().is_empty());
        drop(old);
        let copies = next.list().unwrap();
        assert_eq!(copies.len(), 1);
        assert_eq!(copies[0].id, id);
        let base = next.base(&id).unwrap();
        atomic(&base.with_extension("restored"), b"ack").unwrap();
        assert!(next.list().unwrap().is_empty());
        assert!(base.with_extension("json").is_file());
        let clean = next.begin("main").unwrap();
        next.write("main", &clean, "{}".into(), "Saved".into(), false)
            .unwrap();
        drop(next);
        assert!(Store::new(root).unwrap().list().unwrap().is_empty());
    }
}
