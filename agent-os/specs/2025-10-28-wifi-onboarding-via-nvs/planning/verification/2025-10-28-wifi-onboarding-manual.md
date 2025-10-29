# Wi-Fi Onboarding Manual Verification Checklist

- **Date Prepared:** 2025-10-28
- **Spec:** 2025-10-28-wifi-onboarding-via-nvs

## Scenario A: SoftAP → Portal → STA Connect
- **Status:** ☐ Pending (hardware session required)
- **Prereqs:** Clear NVS, firmware + LittleFS image flashed
- **Steps:**
  1. Boot device with no stored credentials and observe fast LED blink.
  2. Join SoftAP SSID shown on serial (`SOFT_AP_*`).
  3. Browse to `http://192.168.4.1/` and confirm portal loads.
  4. Run “Refresh networks”, tap target SSID, enter WPA2 password, submit.
  5. Observe portal status switching to CONNECTING → CONNECTED and LED going solid.
  6. Verify `NET:STATUS` over serial reports CONNECTED with IP/RSSI.
- **Notes:** Capture screenshots plus serial log for reference.

## Scenario B: Serial Fallback Programming
- **Status:** ☐ Pending
- **Steps:**
  1. From host CLI, run `NET:RESET` to clear credentials; confirm portal resumes AP mode.
  2. Issue `NET:SET,"<ssid>","<pass>"` and watch serial for ACK + async CONNECTED event.
  3. Poll `NET:STATUS` to confirm same IP/RSSI reported as portal.
- **Notes:** Ensure CLI CID handling ignores background status ACKs.

## Scenario C: Long-Press Reset Recovery
- **Status:** ☐ Pending
- **Steps:**
  1. While device is CONNECTED, hold BOOT ≥5 s until LED returns to fast blink.
  2. Confirm serial logs `CTRL: NET:RESET_BUTTON LONG_PRESS` followed by `CTRL: NET:AP_ACTIVE`.
  3. Verify portal reachable again at `192.168.4.1`.

## Scenario D: API Smoke Tests (curl)
- **Status:** ☐ Pending
- **Steps:**
  1. `curl http://192.168.4.1/api/status` → verify JSON body and no caching headers.
  2. `curl http://192.168.4.1/api/scan` → ensure list sorted by RSSI.
  3. `curl -X POST http://192.168.4.1/api/wifi -H 'Content-Type: application/json' -d '{"ssid":"...","pass":"..."}'` → observe `state=CONNECTING` followed by portal+serial updates.
  4. Repeat POST while CONNECTING to confirm `409 busy` response.

> Record pass/fail outcomes and attach logs when the bench session is executed.
