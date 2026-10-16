//! Filesystem bindings only. No editable graph or Undo state lives here.
use crate::acceptance_storage::RunStorage;
use std::{collections::HashMap, path::PathBuf, sync::Mutex};

#[derive(Default)]
pub struct Documents(pub Mutex<Bindings>);
#[derive(Default)]
pub struct Bindings {
    next: u64,
    files: HashMap<String, (PathBuf, Vec<u8>)>,
}
#[derive(Default)]
pub struct CloseGuard(pub Mutex<Option<tauri::ipc::Channel<()>>>);

impl Bindings {
    fn bind(&mut self, path: PathBuf, contents: Vec<u8>) -> String {
        self.next += 1;
        let token = self.next.to_string();
        self.files.insert(token.clone(), (path, contents));
        token
    }

    fn release(&mut self, token: &str) {
        self.files.remove(token);
    }
}

#[tauri::command]
pub fn release_project(token: String, docs: tauri::State<'_, Documents>) -> Result<(), String> {
    docs.0
        .lock()
        .map_err(|_| "File binding unavailable")?
        .release(&token);
    Ok(())
}

#[tauri::command]
pub fn open_project(
    storage: tauri::State<'_, RunStorage>,
    docs: tauri::State<'_, Documents>,
) -> Result<Option<(String, String, String)>, String> {
    let Some(path) = storage
        .dialog()
        .add_filter("Nect project", &["json"])
        .pick_file()
    else {
        return Ok(None);
    };
    let contents = storage.read_project(&path)?;
    let token = docs
        .0
        .lock()
        .map_err(|_| "File binding unavailable")?
        .bind(path.clone(), contents.as_bytes().to_vec());
    Ok(Some((token, path.to_string_lossy().into_owned(), contents)))
}

#[tauri::command]
pub fn save_project(
    contents: String,
    token: Option<String>,
    operation: u32,
    storage: tauri::State<'_, RunStorage>,
    docs: tauri::State<'_, Documents>,
) -> Result<Option<(String, String, u32)>, String> {
    if contents.len() > 50_000_000 {
        return Err("Document exceeds 50 MB".into());
    }
    // Serializes host writes and binding updates, including Save As dialogs.
    let mut bindings = docs.0.lock().map_err(|_| "File binding unavailable")?;
    let (path, expected) = if let Some(token) = &token {
        let (path, contents) = bindings
            .files
            .get(token)
            .ok_or("Unknown file binding; use Save As")?;
        (path.clone(), Some(contents.clone()))
    } else {
        let Some(path) = storage
            .dialog()
            .set_file_name("nect-project.json")
            .add_filter("Nect project", &["json"])
            .save_file()
        else {
            return Ok(None);
        };
        let expected = storage.project_bytes_if_exists(&path)?;
        (path, expected)
    };
    storage.write_project_checked(&path, contents.as_bytes(), expected.as_deref())?;
    let display = path.to_string_lossy().into_owned();
    let token = match token {
        Some(token) => {
            bindings
                .files
                .insert(token.clone(), (path, contents.into_bytes()));
            token
        }
        None => bindings.bind(path, contents.into_bytes()),
    };
    Ok(Some((token, display, operation)))
}

#[tauri::command]
pub fn choose_unsaved(action: String) -> String {
    use rfd::{MessageButtons, MessageDialog, MessageDialogResult, MessageLevel};
    match MessageDialog::new()
        .set_title("Unsaved changes")
        .set_description(format!("Save the current document before {action}?"))
        .set_level(MessageLevel::Warning)
        .set_buttons(MessageButtons::YesNoCancel)
        .show()
    {
        MessageDialogResult::Yes => "save",
        MessageDialogResult::No => "discard",
        _ => "cancel",
    }
    .into()
}

#[tauri::command]
pub fn enable_close_guard(
    channel: tauri::ipc::Channel<()>,
    guard: tauri::State<'_, CloseGuard>,
) -> Result<(), String> {
    *guard.0.lock().map_err(|_| "Close guard unavailable")? = Some(channel);
    Ok(())
}

#[tauri::command]
pub fn finish_close(window: tauri::WebviewWindow) -> Result<(), String> {
    window.destroy().map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn repeated_replacement_releases_buffers_and_preserves_current_token() {
        let mut bindings = Bindings::default();
        let mut current = bindings.bind(PathBuf::from("first.json"), vec![1; 1_000_000]);
        for index in 0..32 {
            let next = bindings.bind(PathBuf::from(format!("{index}.json")), vec![2; 1_000_000]);
            bindings.release(&current);
            bindings.release(&current); // Idempotent retry must not affect the new token.
            assert_eq!(bindings.files.len(), 1);
            assert_eq!(bindings.files[&next].1.len(), 1_000_000);
            assert!(!bindings.files.contains_key(&current));
            current = next;
        }
        bindings.release("unknown");
        assert_eq!(bindings.files.len(), 1);
        bindings.release(&current);
        assert!(bindings.files.is_empty());
    }
}
