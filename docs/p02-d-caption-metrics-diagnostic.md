# P02-D actual-widget font metrics diagnostic

Base c6106c84f337de45f2a305e6cc75b26b62ca287f. Production source is unchanged.

Exact Windows c610 built/deployed but focused19 was10pass/9fail and full91 was81pass/10fail with no skipped/NotRun tests. Caption minimum became100186px, row hints100230x80. First required98x80 sample failed with116x64 ink and46 outside pixels; later Mixed/extreme/wider assertions were not reached. Report SHA2564290e77d4f77c9384e1ba181be081fa644d5a91b3d1e2c2232240fd6e3c1ff23.

Test-only instrumentation logs actual settled-widget fonts, resolved copy, application font, QPA/style/DPI/DPR, codepoints, all27 production reserve candidates, advances/bounds/tight bounds/bearings and replayed minimum. NECT_TEST_QPA_PLATFORM explicitly selects a diagnostic platform; default remains offscreen. Set NECT_GEOMETRY_DIAGNOSTICS=1 and NECT_GEOMETRY_ONLY=1 for bounded comparison. No assertion or production allocation was changed.

Linux offscreen OpenAI Sans resolves actual/replayed100px and all9 focused suites pass. Explicit125% override works. Linux minimal QPA independently reproduces actual/replayed100240px with empty resolved family: worst token -4.940656e-324° has advance240, boundingRect(100000,100000,240,16), tightBoundingRect(0,-16,240,16). Installed Qt6.5.3 private glyph_metrics_t uses100000 as an invalid origin sentinel. This establishes a local mechanism, not exact Windows attribution. Compare Windows offscreen/native on the same product-derived test widgets before choosing a production fix or qualifying normal Windows use.

Local receipt build/d0-evidence/p02-d-caption-metrics-receipt.json SHA25651c412bb35d317c888214485dbfca2fa775b2ac0e969c9ad6708616d9248a6fb. Independent root source inspection confirms diagnostic-only additions and unchanged default/fit assertions. Three focused targets compile; default9/9 pass. Production window.cpp remains SHA256d770148c965e4357e2f355d493cf4a80f2319a5f8de81dcb911c12717dc18039.

Exact c610 styled Linux normal/125% GUI separately passed37 native snapshots,15 Undo pairs,4 view pairs and15 saved-value checks; root verified hashes/JSON/pairs. Receipt d76da5e34986e2bd15f6bbe7eaffa981f026111db453e82407283ffb6900fdde, frozen binary a62390a4fc61c06cbba3c9d37d4d0be4d2d77c46d35d751a4fc0744cd6ec2e10. Four owned sessions closed. This does not resolve Windows offscreen failures or qualify held-live styled capture.
