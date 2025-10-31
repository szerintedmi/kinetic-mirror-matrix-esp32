## Embedded web UI

- **Provide only when connectivity exists**: Ship the web server on Wi-Fi/Ethernet builds; disable it entirely on offline targets to save flash and RAM.
- **Keep payloads tiny**: Use minimal JSON responses (`{"status":"done"}`) and cache header directives to stay under memory limits.
- **Reuse serial actions**: Keep HTTP endpoints aligned with serial commands (`/api/config` <-> `CFG.SET`) so scripts can switch transports effortlessly.
- **Serve static assets from flash**: Keep source files in `data_src/`, store gzipped assets in `data/`.  Assume compression script (eg. )`tools/gzip_fs.py`)  exists and configured in  `platformio.ini`
- **Auth for demos**: For prototypes, expose at least a shared secret or token in `platformio.ini`; prevents accidental exposure on open networks.
- **Adopt micro frameworks deliberately**: Only bring in lightweight CSS or JS helpers (Pico.css/water.css and Alpine.js/htmx/petite-vue/preact/lit etc.) when the UI reuirements outgrow static markup
