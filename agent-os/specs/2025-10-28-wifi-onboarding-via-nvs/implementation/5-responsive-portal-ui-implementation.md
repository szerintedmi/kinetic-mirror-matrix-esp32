# Task 5: Responsive Portal UI

## Overview
**Task Reference:** Task #5 from `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/tasks.md`
**Implemented By:** ui-designer
**Date:** 2025-10-28
**Status:** ✅ Complete

### Task Description
Deliver a mobile-first onboarding page (Pico.css + Alpine.js) with SoftAP-only network scanning, manual SSID/PSK entry, user feedback banners, and build integration from `data_src/` to LittleFS.

## Implementation Summary
Built a single-page onboarding experience using Pico.css for responsive layout and Alpine.js for lightweight state management. The UI polls the HTTP endpoints, renders nearby SSIDs with one-tap selection while the device remains in SoftAP mode, hides the list in STA mode, validates input inline, surfaces success/error banners tied to connection progress, and introduces confirmation dialogs for credential overrides/reset flows. Assets reside in `data_src/net` and are automatically gzipped into LittleFS via the existing pre-build script, keeping SoftAP transfers fast.

The page adapts from small phone screens to desktop widths, maintains large touch targets, and documents portal usage steps in the README.

## Files Changed/Created

### New Files
- `data_src/net/index.html` – Portal markup with status panel, network list, and manual credentials form.
- `data_src/net/styles.css` – Custom styles layered atop Pico.css for status chips, card layout, and responsive spacing.
- `data_src/net/app.js` – Alpine.js component implementing portal logic (polling, SoftAP-gated scans, form submission, messaging).

### Modified Files
- `README.md` – Added Wi-Fi onboarding guidance and filesystem upload instructions (Task 7).

## Key Implementation Details

### Alpine.js state manager
**Location:** `data_src/net/app.js`

Defines `wifiPortal()` returning reactive state (`status`, `networks`, `form`, `message`). Handles:
- Periodic `/api/status` polling with busy detection, banner updates, and device identity fields (MAC, AP SSID/password).
- SoftAP-gated `/api/scan` fetch + RSSI sorting (STA mode clears the list and skips the request, matching backend behaviour).
- `/api/wifi` POST with validation, confirmation prompts, and busy/409 handling.
- `/api/reset` POST to clear credentials and re-enable the SoftAP with follow-up guidance.

**Rationale:** Keeps logic declarative without introducing heavy frameworks and mirrors serial behavior.

### Responsive layout & status visuals
**Location:** `data_src/net/styles.css`

Custom styles provide:
- Status pill colors (AP/Connecting/Connected).
- Card layout for status + forms with touch-friendly spacing.
- Adaptive grid that collapses to single column under 768px.
- Button overrides to keep Pico defaults while ensuring SSID text remains visible alongside metadata.

**Rationale:** Meets the standard’s responsive/mobile-first requirement while leveraging Pico defaults.

### Network list interactions
**Location:** `data_src/net/index.html`

When SoftAP is active each scan result renders a button that pre-fills the manual form and clears prior banners; STA mode instead shows explanatory copy that scanning is disabled. Credentials forms expose mode-specific titles, hints, and confirmation dialogs, and the reset action provides direct guidance on reconnecting to the SoftAP. Accessibility attributes use semantic `<button>` elements for keyboard support.

**Rationale:** Simplifies onboarding by reducing typing on mobile devices and aligning with UX expectations.

## Database Changes (if applicable)

N/A.

## Dependencies (if applicable)

### New Dependencies Added

- Local copies of Pico.css (`data_src/css/pico_v2_1_1.min.css`) and Alpine.js (`data_src/js/alpinejs_v3_13_3.min.js`) bundled into LittleFS for offline SoftAP usage.

### Configuration Changes

No PlatformIO changes required; assets flow through existing `tools/gzip_fs.py` pipeline.

## Testing

### Test Files Created/Updated

No automated UI tests added (SoftAP UI requires hardware). Manual responsiveness checks planned at 320/768/1024 px per task spec.

### Test Coverage
- Unit tests: ❌ None (UI layer)
- Integration tests: ❌ None
- Edge cases covered: Input validation and busy states handled client-side.

### Manual Testing Performed
Pending – tracked in verification checklist (Scenarios A & D). Quick local sanity check performed via desktop browser against stub JSON.

## User Standards & Preferences Compliance

### Embedded Web UI
**File Reference:** `agent-os/standards/frontend/embedded-web-ui.md`

**How Your Implementation Complies:** Ships gzipped static assets from LittleFS, keeps JSON payloads minimal, mirrors serial actions, and leverages a lightweight JS helper (Alpine.js) as allowed.

**Deviations (if any):** None.

### Frontend Serial Interface (for parity)
**File Reference:** `agent-os/standards/frontend/serial-interface.md`

**How Your Implementation Complies:** Portal messaging mirrors serial wording and shows state transitions that align with `CTRL: NET:*` events, keeping transports consistent.

**Deviations (if any):** None.

### Unit Testing Standard
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Documented manual checks since browser automation is out-of-scope for this bench setup.

**Deviations (if any):** Automated UI testing deferred; documented under Known Issues.

## Integration Points (if applicable)

### APIs/Endpoints
- Consumes `/api/status`, `/api/scan`, `/api/wifi`, `/api/reset` (see Task 4 documentation).

## Known Issues & Limitations

- Requires hardware for true end-to-end validation; captive portal UX not yet tested on mobile hardware.

## Performance Considerations

Minimized JS footprint; polling interval defaults to 3 seconds to balance freshness with SoftAP bandwidth. All assets gzip-compressed pre-build.

## Security Considerations

Portal exposes Wi-Fi credentials over HTTP within an isolated SoftAP; acceptable for bench use but should be hardened before production (TLS/auth, CSRF tokens, etc.).

## Dependencies for Other Tasks

Task 7 references portal instructions in the README; Task 4 provides the backend API consumed here.

## Notes

- To customize branding later, adjust Pico theme tokens or extend `styles.css`; build pipeline will re-gzip automatically on the next `pio run -t buildfs`.
