// Local consistency only. This does not verify signatures or installation provenance.
pub fn strict_version(value: &str) -> bool {
    if value.is_empty() || value.len() > 128 || !value.is_ascii() {
        return false;
    }
    let mut build = value.split('+');
    let main = build.next().unwrap();
    if let Some(metadata) = build.next() {
        if !identifiers(metadata, false) || build.next().is_some() {
            return false;
        }
    }
    let (core, pre) = main
        .split_once('-')
        .map_or((main, None), |(a, b)| (a, Some(b)));
    let parts: Vec<_> = core.split('.').collect();
    parts.len() == 3
        && parts.iter().all(|part| numeric(part))
        && pre.is_none_or(|value| identifiers(value, true))
}

fn numeric(value: &str) -> bool {
    !value.is_empty()
        && value.bytes().all(|b| b.is_ascii_digit())
        && (value.len() == 1 || !value.starts_with('0'))
}

fn identifiers(value: &str, prerelease: bool) -> bool {
    value.split('.').all(|part| {
        !part.is_empty()
            && part.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'-')
            && (!prerelease || !part.bytes().all(|b| b.is_ascii_digit()) || numeric(part))
    })
}

pub fn validate_version_set(versions: [&str; 4]) -> Result<(), &'static str> {
    if !versions.iter().all(|version| strict_version(version)) {
        return Err("INVALID_VERSION");
    }
    if versions.iter().any(|version| *version != versions[0]) {
        return Err("VERSION_DRIFT");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn strict_complete_versions() {
        for value in [
            "0.1.0",
            "0.2.0+001",
            "1.2.3-beta.9007199254740993+000",
            "1.2.3--",
        ] {
            assert!(strict_version(value), "{value}");
        }
        for value in [
            "",
            "v1.2.3",
            "1.2",
            "1.2.3\n",
            " 1.2.3",
            "01.2.3",
            "1.2.3-01",
            "1.2.3+",
            "1.2.3-é",
            "1.2.3+a+b",
        ] {
            assert!(!strict_version(value), "{value}");
        }
        assert!(!strict_version(&format!("1.2.3+{}", "a".repeat(123))));
    }
    #[test]
    fn each_source_must_match_exactly() {
        assert_eq!(validate_version_set(["0.1.0"; 4]), Ok(()));
        for index in 0..4 {
            let mut versions = ["0.1.0"; 4];
            versions[index] = "0.1.0+001";
            assert_eq!(validate_version_set(versions), Err("VERSION_DRIFT"));
            versions[index] = "0.1";
            assert_eq!(validate_version_set(versions), Err("INVALID_VERSION"));
        }
    }
}
