// Compiled into the native app only with the explicit test feature.
pub fn require_isolation(requested: bool) -> Result<(), &'static str> {
    if requested { Ok(()) } else { Err("Recovery test build requires a fresh isolated acceptance root") }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn test_build_refuses_normal_profile() { assert!(require_isolation(false).is_err()); }
    #[test] fn test_build_still_requires_the_existing_path_guard() { assert!(require_isolation(true).is_ok()); }
}
