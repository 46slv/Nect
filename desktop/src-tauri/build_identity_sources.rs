use serde_json::Value;
use std::path::Path;

pub fn merge(base: &mut Value, overlay: Value) {
    if let Value::Object(patch) = overlay {
        if !base.is_object() {
            *base = Value::Object(Default::default());
        }
        let object = base.as_object_mut().unwrap();
        for (key, value) in patch {
            if value.is_null() {
                object.remove(&key);
            } else {
                merge(object.entry(key).or_insert(Value::Null), value);
            }
        }
    } else {
        *base = overlay;
    }
}

fn read_json(path: &Path) -> Value {
    serde_json::from_str(&std::fs::read_to_string(path).expect("Cannot read build identity source"))
        .expect("Invalid build identity JSON")
}

pub fn checked_config(root: &Path, cargo: &str) -> Value {
    let base_path = root.join("tauri.conf.json");
    let npm_path = root.join("../package.json");
    println!("cargo:rerun-if-changed={}", base_path.display());
    println!("cargo:rerun-if-changed={}", npm_path.display());
    let base = read_json(&base_path);
    let npm = read_json(&npm_path);
    let mut effective = base.clone();
    let os = std::env::var("CARGO_CFG_TARGET_OS").expect("Missing native target OS");
    let platform = root.join(format!("tauri.{os}.conf.json"));
    println!("cargo:rerun-if-changed={}", platform.display());
    if platform.exists() {
        let overlay = read_json(&platform);
        assert!(overlay.is_object(), "Invalid platform Tauri overlay");
        merge(&mut effective, overlay);
    }
    if let Some(raw) = std::env::var_os("TAURI_CONFIG") {
        let text = raw.into_string().expect("Invalid TAURI_CONFIG text");
        let overlay: Value = serde_json::from_str(&text).expect("Invalid TAURI_CONFIG JSON");
        assert!(overlay.is_object(), "Invalid TAURI_CONFIG overlay");
        merge(&mut effective, overlay);
    }
    super::build_identity_config::validate_version_set([
        cargo,
        npm.get("version").and_then(Value::as_str).unwrap_or(""),
        base.get("version").and_then(Value::as_str).unwrap_or(""),
        effective.get("version").and_then(Value::as_str).unwrap_or(""),
    ])
    .expect("Inconsistent build versions");
    effective
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    #[test]
    fn overlays_merge_recursively_and_null_removes_values() {
        let mut config = json!({"version":"0.1.0", "build":{"frontendDist":"../dist", "other":true}});
        merge(
            &mut config,
            json!({"build":{"frontendDist":"../dist-recovery-acceptance-test"}}),
        );
        assert_eq!(config["version"], "0.1.0");
        assert_eq!(config["build"]["other"], true);
        assert_eq!(
            config["build"]["frontendDist"],
            "../dist-recovery-acceptance-test"
        );
        merge(&mut config, json!({"version":null}));
        assert!(config.get("version").is_none());
    }
}
