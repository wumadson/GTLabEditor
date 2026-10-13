# Phase 3A — compare-only foundation

Base: modern-ui 99f4d5a. Reference experiments: a8ff6c5 (macOS), f1af982 (Windows).

AutoSyncCoordinator owns admission, one active generation, validated Temporary Buffer
decode and immutable-by-convention result publication. AutoSyncRuntime samples existing
application gates, owns one isolated midiIO transaction, and reserves the existing
exclusive-memory gate. The normal build never requests a candidate automatically.
No transport class or protocol mapping is modified.

## Identity and dirty protection

Source validity means the device patch has all comparable 00–0C data. Dirty baseline
validity means PatchChangeGuard has a baseline associated with the loaded source.
Known identity is separate metadata. The coordinator does not require positive bank/
patch numbers for compare-only; it requires a trustworthy dirty baseline. The current
application's PatchChangeGuard cannot establish a baseline for 0/0, so that runtime case
is refused as BASELINE, not treated as a known clean patch. Supporting a trustworthy
temporary baseline without stored identity needs a separately reviewed change later.
Future apply must additionally require known identity and explicit policy; no apply API
exists here. During flight source, identity and session are compared again. Production
activity/refresh/reconnect latches invalidation even if final state appears restored.

## Safety and ownership

Admission checks connection/readiness, source/baseline validity, dirty state, manual READ,
WRITE, patch management, System activity/readiness, signal-chain activity, backup,
Quick Settings/prefetch, modal and exclusive-memory conflicts. RtMidi uses existing port
selection; WinUSB uses selected/open/non-lost session and epoch, without RtMidi ports.
Callbacks require the active generation and adapter-owned midiIO pointer. Completion
must precede exactly one reply; a result is published only on thread completion. Stale
tokens cannot complete another flight. Invalid, duplicate or out-of-order replies yield
ERROR with no candidate. Shutdown waits for its transaction before deleting the runtime.

## Test surface

CONFIG+=autosync_foundation_test enables Ctrl+Alt+R and one qInfo result line only.
Status: 0 UNCHANGED, 1 CHANGED, 2 REFUSED, 3 ERROR. There is no pipe, append log, timer,
scheduler, checkbox or preference. tests/autoSyncCoordinatorTests.pro is an offline QtTest
target using real full-patch fixture decode and injected state/start dependencies.
No candidate is applied, no baseline reset occurs, and no DT1/WRITE is constructed.

## Known exclusive-memory queue risk

KNOWN_EXCLUSIVE_QUEUE_SIDE_EFFECT_RISK

The coordinator currently uses the existing exclusive-memory reservation.
`bankTreeList::setExclusiveMemoryOperation()` clears `queuedPhysicalBank` and
`queuedPhysicalPatch`, which can discard pending physical patch-change events.
This queue behavior has not been validated under concurrent patch changes.
Phase 3A does not guarantee preservation of the physical-event queue.
Polling or automatic scheduling must not be enabled until this behavior is
reviewed/resolved in Phase 3B, before automatic activation.

TEMP 0/0 without a trustworthy dirty-protection baseline remains REFUSED (BASELINE).
