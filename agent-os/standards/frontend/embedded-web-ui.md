## Embedded Web UI

- **Provide only when connectivity exists**: Ship the web server on Wi-Fi/Ethernet builds; guard with `#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))` so offline/native targets compile without it.
- **Current scope — onboarding only**: The web portal (`lib/net_onboarding/`) handles Wi-Fi provisioning (scan, connect, reset). It does not expose motor-control endpoints.
- **Server**: ESPAsyncWebServer on port 80, with ArduinoJson for request/response serialization.
- **Filesystem**: LittleFS (`board_build.filesystem = littlefs`). Mount with `LittleFS.begin(false)`.
- **Keep payloads tiny**: Minimal JSON responses (`{"status":"done"}`). Set cache headers: 5 min for HTML/CSS/JS, 24 hr for favicon.
- **Serve static assets from flash**: Source in `data_src/`, gzipped output in `data/`. The `tools/gzip_fs.py` pre-build script auto-compresses `.html`, `.css`, `.js`, `.ico`, etc. to `.gz`. Configured as `pre:tools/gzip_fs.py` in `platformio.ini`.
- **No auth on SoftAP**: The onboarding portal runs only in AP mode; network isolation is sufficient. Re-evaluate if the server ever runs in STA mode on shared networks.
- **Micro frameworks**: Pico CSS for classless styling, Alpine.js for reactivity. Only upgrade if UI requirements outgrow these.
