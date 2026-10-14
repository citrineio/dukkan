# Zaman managed storefront integration

Sprint 1 adds `com.kawnelectro.Dukkan` as a stable desktop/Wayland app identity.
The existing fullscreen QML window and input gating are unchanged.

Use `zamanctl storefront launch dukkan` or Zaman Guide > Storefronts > Dukkan
with the matching `sprint1/storefronts` branches of zaman-sessiond/zaman-menu.
Configure the built executable's absolute path in
`~/.config/zaman/storefronts.d/dukkan.toml`. Close a manually launched copy first.

Sessiond supervises the client in its own systemd cgroup/Sway workspace; Home
preserves it, Resume restores it, and Close storefront terminates it explicitly.
This change does not move installations into a background service or supervise
Flatpak-launched games as independent Zaman sessions. Those remain follow-on work.

Kadhem built and activated the candidate on Q6A and verified Dukkan opens and
closes normally through the Zaman menu (2026-10-14). Emulator and Steam recovery
regressions also passed. Installing/removing games is not claimed accepted by
that result; installed-game launch remains unimplemented.

Local Python checks alone do not establish Qt compilation/runtime behavior.
