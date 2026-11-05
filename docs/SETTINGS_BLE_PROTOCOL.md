# Settings BLE v1

Service `ee43a100-1b32-4d0f-9ab8-2248b0a01000`, characteristic
`ee43a103-1b32-4d0f-9ab8-2248b0a01000`: encrypted read/write, notify.
All integers little endian. App serializes commands and waits up to 6 seconds
for a matching notification. Write response alone does not acknowledge applying.

Read yields seven bytes: `[1, theme, face, timeoutLo, timeoutHi, allowNew, appCount]`.
Theme 0..7 (0..3 dark, 4..7 light); face 0..5; timeout 0,15,30,60,120,300 seconds (0 always on).
App count at most 24. App IDs are ASCII bundle identifiers, at most 95 bytes.

| Command | Payload after opcode and request ID | Success notification |
| --- | --- | --- |
| `10` | theme, face, timeoutLo, timeoutHi, allowNew | `90, requestID, 0, seven-byte settings` |
| `11` | app index | `91, requestID, 0, index, enabled, UTF-8 bundle ID` |
| `12` | enabled, UTF-8 bundle ID | `92, requestID, 0` |
| `13` | app index | `93, requestID, 0, index, enabled, bundle ID, 00, display name` |

Opcode values in this table are hexadecimal. Rejection has three bytes:
`[opcode | 80, requestID, status]`, status 1 invalid, 2 registry full,
3 request queue full, 4 NVS write failed (state rolled back).
Successful mutation is written to NVS before acknowledgement. Learned ANCS app
IDs are debounced for one second before persistence. Notification text is not stored.
Callbacks enqueue bounded requests; main thread applies settings, flash writes
and UI changes. A new default only affects unknown apps, existing rules remain.
Reading after reconnect is authoritative; app does not claim success without ACK.
Display names come from ANCS Get App Attributes / DisplayName on the iPhone,
cached in RAM (32 apps) and refetched after reconnect. Named catalog responses
are bounded to 160 bytes, display-name suffix at most 55 UTF-8-safe bytes.
Legacy command 11 retains its original format. Name changes announce a fresh
seven-byte settings snapshot after a debounce; the app defers catalog rereads
until any in-flight settings command completes.
