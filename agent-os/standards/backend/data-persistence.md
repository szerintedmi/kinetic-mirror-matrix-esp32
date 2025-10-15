## Data persistence

- **Choose the lightest fit**: Use ESP32 `Preferences`/`NVS` or RP2040 `littlefs` for configs; avoid full databases—prototypes rarely need them.
- **Version configs in code**: Embed a `kConfigVersion`; wipe and reapply defaults when mismatched, skipping migration work during early iterations.
- **Atomic writes**: Write to a staging key/file, then swap; prevents half-written settings when power drops mid-update.
- **Mirror critical settings in RAM**: Keep a cached copy guarded by a mutex; tasks read from RAM while a storage task persists updates.
- **Log persistence failures once**: Rate-limit error logs to avoid flooding serial output when flash wear or access conflicts appear.
