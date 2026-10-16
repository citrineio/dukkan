# Sprint 4 publication

This checkpoint adds best-effort terminal install/remove notifications to the
existing curated storefront, gallery, live progress and managed launch integration.
A successful transaction still requires the relay's verified terminal result;
notification-service downtime does not alter that result or block installation.

The client is selected by `ZAMAN_NOTIFICATION_CLIENT`, falling back to
`~/.local/libexec/zaman-notifications`. It is called asynchronously with a unique
`transaction:` occurrence key and a bounded child lifetime. No general Steam
activity or background-download service is introduced.

The notification implementation and public API live in
[zaman-menu](https://github.com/citrineio/zaman-menu/blob/main/docs/notifications-api.md).
System/session producers live in zaman-sessiond. See that companion's Sprint 4
closure notes for cross-project acceptance boundaries.

The current README describes existing features rather than the historical
implementation sequence. The previous README is retained in `docs/history`.
A public game-submission template and Kawn Electro contact address are included.
Dukkan remains a prototype; publisher enquiries do not imply package compatibility,
a commercial agreement or acceptance into the curated catalog.
