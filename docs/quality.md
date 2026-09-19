# Anti-slop working rules

Slop means implementation that increases ambiguity or change cost beyond the requirement it serves.

1. **Run one real user flow.** Stubs, TODOs, empty abstractions and UI-only buttons are not feature completion.
2. **Do not multiply authorities.** GUI/API/MCP share one Session and one command model.
3. **Preserve meaning.** Stable IDs, types, units, coordinate spaces and authored/evaluated values stay explicit.
4. **Do not fake success.** No partial commits, silent flattening, unknown-to-empty coercion, or catch-all success.
5. **Verify in proportion to risk.** Use independent expected values/readers where they matter; do not build a giant harness before the feature.
6. **Grow abstraction/optimization from evidence.** A helper or adapter is fine when it protects a real boundary. Managers, caches and workers need an actual caller, bottleneck or contract.

## Local AI rule

Candidate requirements are discovery backlog, not implementation authorization. The current goal selects a small subset.

A fresh agent should orient from README, ARCHITECTURE, AGENTS, CURRENT_GOAL and tests without reading old chat history.

Do not duplicate the same rule across Notion, prompt, AGENTS and code docs. Product intent belongs in Notion; implementation truth belongs in Git.
