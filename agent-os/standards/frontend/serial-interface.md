## Serial interface

- **Structured commands**: Use a simple `<verb>[:payload]\n` format; keep verbs short (`PING`, `CFG.SET`) so manual testing over USB is painless.
- **Echo context**: Prefix responses with task/component name (`CTRL:OK`) to simplify multi-board lab setups.
- **Guard parsing**: Enforce max payload length before copying into buffers; prevents accidental overflow from noisy links.
- **Telemetry cadence**: Gate periodic logs behind a rate limiter or `LOG_EVERY_N` macro so the interface remains usable while streaming sensor data.
- **Document command help**: Provide a `HELP` command that dumps current verbs and payload format; prototype operators depend on it when scripts break.
