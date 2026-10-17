# Sprint 5 publication record — 17 October 2026

Scope: independent persistent background transactions; durable terminal-result
notification integration; live menu activity; approved branding; empty-state and
ordinary Discover-launch corrections. The implementation details are in
[SPRINT5.md](SPRINT5.md) and [ARCHITECTURE.md](ARCHITECTURE.md).

The final fixes were built and accepted by the device owner before publication.
The owner reported the menu branding/empty-state correction working and the
Discover launch issue solved. Authoring source and installer checks are separate
from device compilation and acceptance.

The supplied Dukkan CTest suite has five cases: catalog/backend behavior, navigation
nonce persistence, shared catalog, artwork metadata, and full gallery/QML behavior.
The paired menu has 14 JavaScript suites, Python notification/recovery checks and
Qt presentation cases at 1× and 2× display scaling. These checks do not claim every
possible real-world reboot/network/controller timing permutation has been exercised.

No sessiond or Pegasus source change was required. The accepted Guide/controller
ownership behavior, Steam ownership and PS2 conditional-launcher/profile-selection
repair remain outside this publication's changes.

## Publish and build policy

Publish directly from the existing tested Sprint 5 source checkouts. Committing,
pushing and updating documentation do not require recompilation. Build outputs,
SQLite state, notification history, device configs and logs are not source assets.
Already-tested executables can be distributed separately as versioned release
artifacts when packaging is requested; do not substitute binaries for source.

Keep stable source/build cache locations for subsequent work and rebuild changed
targets. Before replacing an active executable, use a separate installed copy and
the existing guarded activation/rollback procedure. Moving the publication to a
new clone/build tree is unnecessary. In particular, this sprint does not require a
Rust/sessiond rebuild.

The supplied systemd worker unit is a development template pointing to the accepted
Sprint 5 checkout. Downstream installation must set ExecStart to its real installed
worker path; publication does not rewrite the current device's service paths.
