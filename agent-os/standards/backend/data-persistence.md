## Data persistence

- **Choose the lightest fit**: Use ESP32 `Preferences`/`NVS` or RP2040 `littlefs` for configs; avoid full databases—prototypes rarely need them.
- **Isolate namespaces**: Each subsystem gets its own NVS namespace (e.g. `"net"`, `"mqtt"`); prevents key collisions and allows independent wipe/reset.
- **Version configs in code**: Embed a `kConfigVersion`; wipe and reapply defaults when mismatched, skipping migration work during early iterations.
- **Atomic writes**: Write to a staging key/file, then swap; prevents half-written settings when power drops mid-update.
- **Mirror critical settings in RAM**: Keep a cached copy guarded by a mutex; tasks read from RAM while a storage task persists updates.
- **Lazy-load from flash**: Defer NVS reads until first access (`ensureLoaded` guard); avoids startup latency for configs that may never be needed.
- **Skip redundant writes**: Compare new value to cached copy before writing; reduces flash wear on frequent save paths.
- **Abstract storage for tests**: Wrap `Preferences` in an adapter that swaps to an in-memory map under `USE_STUB_BACKEND`; enables host-side unit tests without hardware.
- **Log persistence failures once**: Rate-limit error logs to avoid flooding serial output when flash wear or access conflicts appear.
