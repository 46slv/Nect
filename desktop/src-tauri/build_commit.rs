// The receipt uses the same GitHub Actions checkout SHA. No runtime Git lookup.
pub fn github_commit<'a>(
    actions: Option<&str>,
    sha: Option<&'a str>,
) -> Result<Option<&'a str>, &'static str> {
    if actions != Some("true") {
        return Ok(None);
    }
    let sha = sha.ok_or("Missing GitHub Actions checkout SHA")?;
    if sha.len() != 40
        || !sha
            .bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
    {
        return Err("Invalid GitHub Actions checkout SHA");
    }
    Ok(Some(sha))
}

#[cfg(test)]
mod tests {
    use super::*;

    const RECEIPT_COMMIT: &str = "d0d9c9265a1628983f727715bc205a44aaa5c0a6";

    #[test]
    fn ci_commit_is_the_exact_receipt_checkout_not_a_version() {
        assert_eq!(
            github_commit(Some("true"), Some(RECEIPT_COMMIT)),
            Ok(Some(RECEIPT_COMMIT))
        );
    }

    #[test]
    fn outside_github_actions_provenance_remains_unknown() {
        for actions in [None, Some("false"), Some("TRUE")] {
            assert_eq!(github_commit(actions, Some(RECEIPT_COMMIT)), Ok(None));
        }
        assert_eq!(github_commit(None, None), Ok(None));
    }

    #[test]
    fn ci_does_not_silently_publish_missing_or_malformed_provenance() {
        assert!(github_commit(Some("true"), None).is_err());
        for sha in [
            "",
            "main",
            "d0d9c926",
            "D0D9C9265A1628983F727715BC205A44AAA5C0A6",
            " d0d9c9265a1628983f727715bc205a44aaa5c0a6",
            "d0d9c9265a1628983f727715bc205a44aaa5c0a6\n",
        ] {
            assert!(github_commit(Some("true"), Some(sha)).is_err(), "{sha:?}");
        }
    }
}
