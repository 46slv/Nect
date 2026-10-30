//! Explicit private files: process-only immutable snapshots and opaque capabilities.
//! Neither paths nor bytes cross IPC or enter authored documents. No OS registration.
use crate::desktop_font_layout::{
    self, DesktopAxis, DesktopFace, DesktopOutline, DesktopTextRequest,
};
use std::sync::{Arc, Mutex};

const MAX_FILE_BYTES: usize = 128 * 1024 * 1024;
const MAX_TOTAL_BYTES: usize = 256 * 1024 * 1024;
const MAX_FILES: usize = 8;

#[derive(Clone, serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PrivateFace {
    pub face_index: u32,
    pub face: DesktopFace,
}
#[derive(Clone, serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PrivateFile {
    pub token: String,
    pub sha256: String,
    pub faces: Vec<PrivateFace>,
}
#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PrivateSource {
    kind: &'static str,
    sha256: String,
    face_index: u32,
}
#[derive(serde::Serialize)]
pub struct PrivateOutline {
    source: PrivateSource,
    outline: DesktopOutline,
}
struct StoredFile {
    metadata: PrivateFile,
    bytes: Vec<u8>,
}
#[derive(Default)]
struct Catalog {
    files: Vec<StoredFile>,
    total_bytes: usize,
}
// One lock also serializes native snapshot copies/operations: bounded memory even
// when IPC requests overlap. The catalog is intentionally not persisted or Debug.
#[derive(Clone, Default)]
pub struct PrivateFonts(Arc<Mutex<Catalog>>);

fn validate_container(bytes: &[u8]) -> Result<(), String> {
    if bytes.len() < 12 || bytes.len() > MAX_FILE_BYTES {
        return Err("Private font must be between 12 bytes and 128 MiB".into());
    }
    let signature = &bytes[..4];
    if signature == b"ttcf" {
        let version = u32::from_be_bytes(bytes[4..8].try_into().unwrap());
        let count = u32::from_be_bytes(bytes[8..12].try_into().unwrap()) as usize;
        if !matches!(version, 0x00010000 | 0x00020000)
            || !(1..=256).contains(&count)
            || bytes.len() < 12 + count * 4
        {
            return Err("Invalid or oversized private font collection".into());
        }
        for i in 0..count {
            let offset =
                u32::from_be_bytes(bytes[12 + i * 4..16 + i * 4].try_into().unwrap()) as usize;
            let Some(face) = bytes.get(offset..).filter(|face| face.len() >= 12) else {
                return Err("Invalid private font collection face offset".into());
            };
            if !matches!(&face[..4], b"\0\x01\0\0" | b"OTTO" | b"true") {
                return Err("Unsupported private font collection face".into());
            }
        }
    } else if !matches!(signature, b"\0\x01\0\0" | b"OTTO" | b"true") {
        return Err(
            "Choose an uncompressed TTF, OTF, TTC or OTC font; no unpacking is performed".into(),
        );
    }
    Ok(())
}

#[cfg(windows)]
fn read_selection(
    storage: &crate::acceptance_storage::RunStorage,
    path: &std::path::Path,
) -> Result<Vec<u8>, String> {
    let extension = path
        .extension()
        .and_then(|s| s.to_str())
        .unwrap_or("")
        .to_ascii_lowercase();
    if !matches!(extension.as_str(), "ttf" | "otf" | "ttc" | "otc") {
        return Err("Choose a TTF, OTF, TTC or OTC font file".into());
    }
    // RunStorage retains disposable acceptance-directory/reparse guards. The
    // original handle/path is discarded; all later work uses immutable bytes.
    let bytes = storage.read_private_font(path)?;
    validate_container(&bytes)?;
    Ok(bytes)
}

#[cfg(windows)]
fn random_token() -> Result<String, String> {
    #[link(name = "bcrypt")]
    extern "system" {
        fn BCryptGenRandom(
            algorithm: *mut std::ffi::c_void,
            buffer: *mut u8,
            length: u32,
            flags: u32,
        ) -> i32;
    }
    let mut bytes = [0u8; 32];
    // BCRYPT_USE_SYSTEM_PREFERRED_RNG; never substitute time/counters for entropy.
    let status = unsafe {
        BCryptGenRandom(
            std::ptr::null_mut(),
            bytes.as_mut_ptr(),
            bytes.len() as u32,
            2,
        )
    };
    if status < 0 {
        return Err("Cannot create a private font process capability".into());
    }
    Ok(hex(&bytes))
}
fn hex(bytes: &[u8]) -> String {
    use std::fmt::Write;
    let mut result = String::with_capacity(bytes.len() * 2);
    for byte in bytes {
        write!(&mut result, "{byte:02x}").unwrap();
    }
    result
}
// FIPS 180-4 SHA-256. Content identity only (not an authorization credential).
// Kept dependency-free; standard empty/abc/multiblock/million-a vectors below.
fn sha256(bytes: &[u8]) -> String {
    const K: [u32; 64] = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2,
    ];
    let mut state: [u32; 8] = [
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab,
        0x5be0cd19,
    ];
    let blocks = (bytes.len() + 9 + 63) / 64;
    for block in 0..blocks {
        let mut data = [0u8; 64];
        for (i, byte) in data.iter_mut().enumerate() {
            let offset = block * 64 + i;
            if offset < bytes.len() {
                *byte = bytes[offset];
            } else if offset == bytes.len() {
                *byte = 0x80;
            }
        }
        if block + 1 == blocks {
            data[56..].copy_from_slice(&((bytes.len() as u64) * 8).to_be_bytes());
        }
        let mut w = [0u32; 64];
        for i in 0..16 {
            w[i] = u32::from_be_bytes(data[i * 4..i * 4 + 4].try_into().unwrap());
        }
        for i in 16..64 {
            let x = w[i - 15];
            let y = w[i - 2];
            w[i] = w[i - 16]
                .wrapping_add(x.rotate_right(7) ^ x.rotate_right(18) ^ (x >> 3))
                .wrapping_add(w[i - 7])
                .wrapping_add(y.rotate_right(17) ^ y.rotate_right(19) ^ (y >> 10));
        }
        let [mut a, mut b, mut c, mut d, mut e, mut f, mut g, mut h] = state;
        for i in 0..64 {
            let t1 = h
                .wrapping_add(e.rotate_right(6) ^ e.rotate_right(11) ^ e.rotate_right(25))
                .wrapping_add((e & f) ^ (!e & g))
                .wrapping_add(K[i])
                .wrapping_add(w[i]);
            let t2 = (a.rotate_right(2) ^ a.rotate_right(13) ^ a.rotate_right(22))
                .wrapping_add((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d.wrapping_add(t1);
            d = c;
            c = b;
            b = a;
            a = t1.wrapping_add(t2);
        }
        for (old, new) in state.iter_mut().zip([a, b, c, d, e, f, g, h]) {
            *old = old.wrapping_add(new);
        }
    }
    hex(&state
        .iter()
        .flat_map(|value| value.to_be_bytes())
        .collect::<Vec<_>>())
}
impl Catalog {
    fn lookup(&self, token: &str, face_index: u32) -> Result<(&StoredFile, &PrivateFace), String> {
        if token.len() != 64
            || !token
                .bytes()
                .all(|c| c.is_ascii_digit() || (b'a'..=b'f').contains(&c))
        {
            return Err("Invalid private font process capability".into());
        }
        let file = self
            .files
            .iter()
            .find(|file| file.metadata.token == token)
            .ok_or("Private font is not loaded in this process; choose the original file again")?;
        let face = file
            .metadata
            .faces
            .iter()
            .find(|face| face.face_index == face_index)
            .ok_or("Private font collection face index is not available")?;
        Ok((file, face))
    }
    fn shape(
        &self,
        token: &str,
        face_index: u32,
        request: DesktopTextRequest,
    ) -> Result<PrivateOutline, String> {
        let (file, selected) = self.lookup(token, face_index)?;
        if selected.face != request.face {
            return Err("Private font face descriptor does not match the selected snapshot".into());
        }
        let outline = desktop_font_layout::prepare_private(request, &file.bytes, face_index)?;
        Ok(PrivateOutline {
            source: PrivateSource {
                kind: "file-native-v1",
                sha256: file.metadata.sha256.clone(),
                face_index,
            },
            outline,
        })
    }
    #[cfg(windows)]
    fn insert(&mut self, bytes: Vec<u8>) -> Result<PrivateFile, String> {
        validate_container(&bytes)?;
        let fingerprint = sha256(&bytes);
        if let Some(file) = self.files.iter().find(|f| f.metadata.sha256 == fingerprint) {
            if file.bytes != bytes {
                return Err("Private font fingerprint collision refused".into());
            }
            return Ok(file.metadata.clone());
        }
        if self.files.len() >= MAX_FILES
            || bytes.len() > MAX_TOTAL_BYTES.saturating_sub(self.total_bytes)
        {
            return Err(
                "Private font session limit reached (8 files / 256 MiB); restart Nect to clear it"
                    .into(),
            );
        }
        let faces = desktop_font_layout::private_faces(&bytes)?;
        let metadata = PrivateFile {
            token: random_token()?,
            sha256: fingerprint,
            faces,
        };
        if self
            .files
            .iter()
            .any(|f| f.metadata.token == metadata.token)
        {
            return Err("Private font process capability collision refused".into());
        }
        self.total_bytes += bytes.len();
        self.files.push(StoredFile {
            metadata: metadata.clone(),
            bytes,
        });
        Ok(metadata)
    }
}

#[tauri::command]
pub async fn open_private_font_file(
    window: tauri::WebviewWindow,
    storage: tauri::State<'_, crate::acceptance_storage::RunStorage>,
    catalog: tauri::State<'_, PrivateFonts>,
) -> Result<Option<PrivateFile>, String> {
    #[cfg(windows)]
    {
        let storage = storage.inner().clone();
        let catalog = catalog.inner().clone();
        tauri::async_runtime::spawn_blocking(move || {
            // Never wait on a shaping operation or parse/hash 128 MiB on the UI thread.
            let mut catalog = catalog
                .0
                .lock()
                .map_err(|_| "Private font catalog is unavailable")?;
            let Some(path) = storage
                .dialog(&window)
                .add_filter("Private font file", &["ttf", "otf", "ttc", "otc"])
                .pick_file()
            else {
                return Ok(None);
            };
            catalog.insert(read_selection(&storage, &path)?).map(Some)
        })
        .await
        .map_err(|e| e.to_string())?
    }
    #[cfg(not(windows))]
    {
        let _ = (window, storage, catalog);
        Err("Private font loading requires Windows DirectWrite".into())
    }
}
#[tauri::command]
pub async fn list_private_font_files(
    catalog: tauri::State<'_, PrivateFonts>,
) -> Result<Vec<PrivateFile>, String> {
    if !cfg!(windows) {
        return Err("Private font loading requires Windows DirectWrite".into());
    }
    let catalog = catalog.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        let catalog = catalog
            .0
            .lock()
            .map_err(|_| "Private font catalog is unavailable")?;
        Ok(catalog
            .files
            .iter()
            .map(|file| file.metadata.clone())
            .collect())
    })
    .await
    .map_err(|e| e.to_string())?
}
#[tauri::command]
pub async fn list_private_font_axes(
    token: String,
    face_index: u32,
    catalog: tauri::State<'_, PrivateFonts>,
) -> Result<Vec<DesktopAxis>, String> {
    if !cfg!(windows) {
        return Err("Private font loading requires Windows DirectWrite".into());
    }
    let catalog = catalog.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        let catalog = catalog
            .0
            .lock()
            .map_err(|_| "Private font catalog is unavailable")?;
        let (file, _) = catalog.lookup(&token, face_index)?;
        desktop_font_layout::private_axes(&file.bytes, face_index)
    })
    .await
    .map_err(|e| e.to_string())?
}
#[tauri::command]
pub async fn shape_private_font_text(
    token: String,
    face_index: u32,
    request: DesktopTextRequest,
    catalog: tauri::State<'_, PrivateFonts>,
) -> Result<PrivateOutline, String> {
    if !cfg!(windows) {
        return Err("Private font loading requires Windows DirectWrite".into());
    }
    let catalog = catalog.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        let catalog = catalog
            .0
            .lock()
            .map_err(|_| "Private font catalog is unavailable")?;
        catalog.shape(&token, face_index, request)
    })
    .await
    .map_err(|e| e.to_string())?
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn standard_sha256_vectors() {
        assert_eq!(
            sha256(b""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        );
        assert_eq!(
            sha256(b"abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
        assert_eq!(
            sha256(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
        );
        assert_eq!(
            sha256(&vec![b'a'; 1_000_000]),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"
        );
    }
    #[test]
    fn invalid_container_and_collection_bounds() {
        for bytes in [
            &b""[..],
            &b"wOFF12345678"[..],
            &b"wOF212345678"[..],
            &b"ttcf\0\x01\0\0\0\0\x01\x01"[..],
        ] {
            assert!(validate_container(bytes).is_err());
        }
        let mut collection = b"ttcf\0\x01\0\0\0\0\0\x01\xff\xff\xff\xff".to_vec();
        assert!(validate_container(&collection).is_err());
        collection[12..16].copy_from_slice(&16u32.to_be_bytes());
        collection.extend_from_slice(b"OTTO\0\0\0\0\0\0\0\0");
        assert!(validate_container(&collection).is_ok()); // Native Analyze still required.
        collection[16..20].copy_from_slice(b"wOFF");
        assert!(validate_container(&collection).is_err());
    }
    #[test]
    fn capabilities_fail_closed_and_serialization_has_no_bytes_or_paths() {
        let mut catalog = Catalog::default();
        assert!(catalog.lookup("../a.ttf", 0).is_err());
        assert!(catalog.lookup(&"0".repeat(64), 0).is_err());
        let metadata = PrivateFile {
            token: "a".repeat(64),
            sha256: sha256(b"abc"),
            faces: vec![PrivateFace {
                face_index: 7,
                face: DesktopFace {
                    family: "Synthetic".into(),
                    name: "Regular".into(),
                    weight: 400,
                    style: 0,
                    stretch: 5,
                    format: 1,
                    color: false,
                },
            }],
        };
        catalog.files.push(StoredFile {
            metadata: metadata.clone(),
            bytes: vec![1, 2, 3],
        });
        assert!(catalog.lookup(&metadata.token, 0).is_err());
        assert_eq!(catalog.lookup(&metadata.token, 7).unwrap().1.face_index, 7);
        let value = serde_json::to_value(metadata).unwrap();
        assert_eq!(value.as_object().unwrap().len(), 3);
        assert!(value.get("path").is_none() && value.get("bytes").is_none());
        assert_eq!(value["faces"][0]["faceIndex"], 7);
    }
    #[cfg(windows)]
    #[test]
    fn cng_capabilities_are_random_and_invalid_fonts_are_atomic() {
        let first = random_token().unwrap();
        let second = random_token().unwrap();
        assert_ne!(first, second);
        assert_eq!(first.len(), 64);
        let mut catalog = Catalog::default();
        assert!(catalog.insert(b"OTTO\0\0\0\0\0\0\0\0".to_vec()).is_err());
        assert!(catalog.files.is_empty());
        assert_eq!(catalog.total_bytes, 0);
    }
}

#[cfg(all(test, windows))]
mod windows_tests {
    use super::*;
    fn fixture(text: &str) -> Vec<u8> {
        let compact: String = text.split_whitespace().collect();
        (0..compact.len())
            .step_by(2)
            .map(|i| u8::from_str_radix(&compact[i..i + 2], 16).unwrap())
            .collect()
    }
    fn request(face: DesktopFace, text: &str) -> DesktopTextRequest {
        DesktopTextRequest {
            face,
            text: text.into(),
            size: 100.0,
            tracking: 0.0,
            line_spacing: 1.2,
            kerning: true,
            ligatures: true,
            alignment: 0,
            axes: None,
            writing_mode: None,
            wrap_width: None,
            wrap_height: None,
            wrap_policy: None,
        }
    }
    #[test]
    fn private_collection_indices_snapshot_identity_and_no_fallback() {
        // Wholly synthetic data; both faces deliberately have identical descriptors.
        let bytes = fixture(include_str!("../tests/fixtures/private-collection.hex"));
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("synthetic.ttc");
        std::fs::write(&path, &bytes).unwrap();
        let snapshot =
            read_selection(&crate::acceptance_storage::RunStorage::default(), &path).unwrap();
        let mut catalog = Catalog::default();
        let selected = catalog.insert(snapshot).unwrap();
        assert_eq!(selected.faces.len(), 2);
        assert_eq!(selected.faces[0].face_index, 0);
        assert_eq!(selected.faces[1].face_index, 1);
        assert_eq!(selected.faces[0].face, selected.faces[1].face);
        assert_eq!(selected.sha256, sha256(&bytes));
        let again = catalog.insert(bytes.clone()).unwrap();
        assert_eq!(again.token, selected.token);
        assert_eq!(catalog.files.len(), 1);
        assert_eq!(catalog.total_bytes, bytes.len());
        assert_eq!(std::fs::read(&path).unwrap(), bytes); // Import never modifies the original.
        std::fs::write(&path, b"replaced original").unwrap();
        std::fs::remove_file(&path).unwrap();
        let first = catalog
            .shape(
                &selected.token,
                0,
                request(selected.faces[0].face.clone(), "AB"),
            )
            .unwrap();
        let second = catalog
            .shape(
                &selected.token,
                1,
                request(selected.faces[1].face.clone(), "AB"),
            )
            .unwrap();
        assert!(!first.outline.points.is_empty());
        assert!(!second.outline.points.is_empty());
        assert!(second.outline.advance_width > first.outline.advance_width + 1.0);
        assert_eq!(first.source.sha256, selected.sha256);
        assert_eq!(second.source.face_index, 1);
        assert!(catalog
            .shape(
                &selected.token,
                2,
                request(selected.faces[0].face.clone(), "A")
            )
            .is_err());
        let mut wrong = request(selected.faces[0].face.clone(), "A");
        wrong.face.name.push_str(" spoofed");
        assert!(catalog.shape(&selected.token, 0, wrong).is_err());
        assert!(catalog
            .shape(
                &selected.token,
                0,
                request(selected.faces[0].face.clone(), "Z")
            )
            .is_err());
        let outline_json = serde_json::to_value(second).unwrap();
        assert_eq!(outline_json["source"]["kind"], "file-native-v1");
        assert!(outline_json["source"].get("token").is_none());
        assert!(outline_json.get("bytes").is_none() && outline_json.get("path").is_none());
    }
    #[test]
    fn private_variable_axes_and_session_budget() {
        let bytes = fixture(include_str!("../tests/fixtures/private-variable.hex"));
        let mut catalog = Catalog::default();
        let selected = catalog.insert(bytes.clone()).unwrap();
        let axes = desktop_font_layout::private_axes(&catalog.files[0].bytes, 0).unwrap();
        assert_eq!(axes.len(), 1);
        assert_eq!(axes[0].tag, "wght");
        assert_eq!(axes[0].min, 100.0);
        assert_eq!(axes[0].max, 900.0);
        let mut low = request(selected.faces[0].face.clone(), "AB");
        low.axes = Some(std::collections::BTreeMap::from([("wght".into(), 100.0)]));
        let mut high = low.clone();
        high.axes.as_mut().unwrap().insert("wght".into(), 900.0);
        let low = catalog.shape(&selected.token, 0, low).unwrap();
        let high = catalog.shape(&selected.token, 0, high).unwrap();
        assert!(high.outline.advance_width > low.outline.advance_width + 1.0);
        let mut wrong = request(selected.faces[0].face.clone(), "A");
        wrong.axes = Some(std::collections::BTreeMap::from([("wght".into(), 901.0)]));
        assert!(catalog.shape(&selected.token, 0, wrong).is_err());
        let other = fixture(include_str!("../tests/fixtures/private-collection.hex"));
        catalog.total_bytes = MAX_TOTAL_BYTES;
        assert!(catalog.insert(other).is_err());
        assert_eq!(catalog.files.len(), 1);
        catalog.total_bytes = bytes.len();
        while catalog.files.len() < MAX_FILES {
            catalog.files.push(StoredFile {
                metadata: selected.clone(),
                bytes: vec![],
            });
        }
        assert!(catalog
            .insert(fixture(include_str!(
                "../tests/fixtures/private-collection.hex"
            )))
            .is_err());
    }
}
