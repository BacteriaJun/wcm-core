# Anchor persistence

Witnesses are transient physical evidence and do not survive a World Break. Durable application state belongs in an Anchor or another application-owned persistence domain.

WCM provides an A/B Anchor format and a hardware-neutral backend interface:

```c
wcm_anchor_backend_t backend = {
    .read = storage_read,
    .write = storage_write,
    .sync = storage_sync,
    .user = &storage,
};
```

`wcm_anchor_backend_commit()` writes the older or unused slot, asks the backend to synchronize when a sync callback is supplied, reads the slot back, and verifies the decoded sequence and payload. `wcm_anchor_backend_load()` selects the newest valid slot.

## Stable storage image

The persistent representation is not the in-memory C structure. Anchor format 2 uses a canonical little-endian image of `WCM_ANCHOR_IMAGE_BYTES` bytes (148 bytes with the default 128-byte payload capacity):

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic (`WCMA`) |
| 4 | 2 | format version |
| 6 | 2 | payload size |
| 8 | 8 | sequence |
| 16 | 4 | CRC-32 |
| 20 | payload capacity | payload and zero-filled remainder |

`wcm_anchor_encode()` and `wcm_anchor_decode()` are the storage boundary. A backend must read and write exactly `WCM_ANCHOR_IMAGE_BYTES`; it must not persist `sizeof(wcm_anchor_slot_t)` or depend on target padding/alignment.

The CRC covers the canonical header fields before the CRC and the used payload bytes. The sequence is 64-bit and monotonically assigned by the application or persistence owner.

## Backend responsibilities

The backend owns storage-specific guarantees that Core cannot provide generically, including erase behavior, write alignment, power-fail behavior below slot granularity, wear limits, ECC, media locking, and access serialization. A product that requires stronger atomicity should implement it below the two-slot WCM contract.

Anchor payloads may contain durable configuration, calibration, objectives, counters, or application identity. Do not persist Witnesses, Snapshots, pending Intents, temporary capability state, or actuator commands.

Target qualification should include interrupted writes, corrupted newest-slot recovery, media read failure, and any brownout behavior relevant to the selected storage technology.
