# Recovery native acceptance build

User-selected bounded continuation of PR74, base71be9026ae1365e1b07fe33672d68dc52bb01b66.
No generic runtime injection API or new native command. The single fixed
synthetic A/B scenario runs automatically only in an explicit test build.

Normal Vite mode defines the test branch false, emits only normal assets and
no test module/result/fault strings. Explicit recovery-acceptance-test mode
emits separate test assets. Cargo feature of the same name is not default.
Build.rs verifies a generated frontend profile marker against the native
feature, refusing mixed normal/test assets. Existing default frontendDist stays
unchanged. TAURI_CONFIG selects the test assets at build time only.

The feature requires the existing fresh acceptance root before Tauri startup.
After the existing path/identity checks the native test window receives a
read-only instance marker. No ordinary profile can enter the test scenario.
The JS module requires that marker, native bridge and fresh unbound revision0.
It wraps only this controller's writer, targets the synthetic A native token,
uses two bounded simulated failures, passes B and retry writes to the original
native writer, and restores that writer in finally. A read-only result getter
is the only test surface; no callable commands, paths or failure parameters.

The fixed scenario covers A r1 failure, newer A r2 failure across B switch,
stale failure revision/generation rejection, UI target selection, real native
retry success, and exact B project/file/history/SVG invariance. CLI backup and
one finish_close follow from the external verifier. Only newly created test
profiles/stores are inspected; no old Recovery, normal document or OS dialog.
Normal/test EXEs are separately named and hashed. The test build adds a guard,
marker, wrapper and automatic fixture, so it is not claimed byte/performance
identical to normal. The production writer/Session/renderer are unchanged.
