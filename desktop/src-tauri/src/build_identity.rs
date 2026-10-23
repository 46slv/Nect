use serde::Serialize;

#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct BuildIdentity {
    authority: &'static str,
    integrity: &'static str,
    app_id: &'static str,
    app_version: &'static str,
    commit: Option<&'static str>,
    build_id: Option<&'static str>,
    build_flavor: &'static str,
    asset_mode: &'static str,
    runtime_isolation: &'static str,
    target: Option<&'static str>,
    channel: Option<&'static str>,
    channel_authority: &'static str,
    package_kind: &'static str,
    package_authority: &'static str,
}

impl BuildIdentity {
    pub fn compiled(app_id: &str, version: Option<&str>, isolated: bool) -> Self {
        Self {
            authority: "native",
            integrity: if app_id == "com.nect.svg" && version == Some(env!("CARGO_PKG_VERSION")) {
                "verified"
            } else {
                "invalid"
            },
            app_id: "com.nect.svg",
            app_version: env!("CARGO_PKG_VERSION"),
            commit: None,
            build_id: None,
            build_flavor: if cfg!(feature = "recovery-acceptance-test") {
                "recovery-acceptance-test"
            } else {
                "normal"
            },
            asset_mode: if cfg!(feature = "custom-protocol") {
                "static"
            } else {
                "dev-server"
            },
            runtime_isolation: if isolated {
                "acceptance-isolated"
            } else {
                "ordinary"
            },
            target: option_env!("NECT_BUILD_TARGET"),
            channel: None,
            channel_authority: "unknown",
            package_kind: "unknown",
            package_authority: "unknown",
        }
    }
}

#[tauri::command]
pub fn query_build_identity(identity: tauri::State<'_, BuildIdentity>) -> BuildIdentity {
    identity.inner().clone()
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn native_context_mismatch_is_invalid_without_crashing() {
        assert_eq!(
            BuildIdentity::compiled("other", Some(env!("CARGO_PKG_VERSION")), false).integrity,
            "invalid"
        );
        assert_eq!(
            BuildIdentity::compiled("com.nect.svg", None, false).integrity,
            "invalid"
        );
        assert_eq!(
            BuildIdentity::compiled("com.nect.svg", Some("999.0.0"), false).integrity,
            "invalid"
        );
    }
    #[test]
    fn isolation_is_independent_of_build_flavor_and_provenance_stays_unknown() {
        let identity = BuildIdentity::compiled("com.nect.svg", Some(env!("CARGO_PKG_VERSION")), true);
        assert_eq!(identity.integrity, "verified");
        assert_eq!(identity.runtime_isolation, "acceptance-isolated");
        assert_eq!(identity.channel, None);
        assert_eq!(identity.channel_authority, "unknown");
        assert_eq!(identity.package_kind, "unknown");
        assert_eq!(identity.package_authority, "unknown");
        assert_eq!(identity.commit, None);
        assert_eq!(identity.build_id, None);
    }
}
