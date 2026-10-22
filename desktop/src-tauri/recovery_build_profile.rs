pub fn require_static(test_build: bool, static_assets: bool) -> Result<(), &'static str> {
    if test_build && !static_assets { Err("Recovery test build requires custom-protocol assets") } else { Ok(()) }
}
pub fn validate(profile: &str, test_build: bool) -> Result<(), &'static str> {
    let expected = if test_build { "recovery-acceptance-test" } else { "normal" };
    if profile.trim() == expected { Ok(()) } else { Err("Recovery frontend/native build profiles differ") }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn normal_accepts_only_normal_assets() { assert!(validate("normal\n", false).is_ok()); assert!(validate("recovery-acceptance-test", false).is_err()); }
    #[test] fn test_accepts_only_test_assets() { assert!(validate("recovery-acceptance-test\n", true).is_ok()); assert!(validate("normal", true).is_err()); }
    #[test] fn unknown_profile_rejects() { assert!(validate("", false).is_err()); assert!(validate("unknown", true).is_err()); }
    #[test] fn dev_is_unchanged_but_test_requires_static_assets() { assert!(require_static(false, false).is_ok()); assert!(require_static(true, false).is_err()); assert!(require_static(true, true).is_ok()); }
}
