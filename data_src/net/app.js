function wifiPortal() {
  return {
    status: {
      state: "AP_ACTIVE",
      stateLabel: "Access Point",
      ssid: "",
      ip: "",
      rssiLabel: "—",
      apPassword: "",
      apSsid: "",
      mac: "",
      firmwareVersion: "",
      firmwareDate: "",
    },
    config: {
      primary: { ssid: "", configured: false },
      secondary: { ssid: "", configured: false },
      connected_to: null,
    },
    networks: [],
    hasScanned: false,
    lastSubmittedSsid: "",
    form: {
      ssid: "",
      pass: "",
      slot: "primary",
    },
    loading: {
      status: false,
      scan: false,
      config: false,
    },
    busy: false,
    message: {
      kind: "",
      heading: "",
      text: "",
    },
    pollId: null,

    init() {
      Promise.all([this.refreshStatus(), this.refreshConfig()]).finally(() => {
        this.startPolling();
      });
    },

    async refreshStatus() {
      if (this.loading.status) return;
      this.loading.status = true;
      try {
        const res = await fetch("/api/status", { cache: "no-store" });
        if (!res.ok) throw new Error("Status request failed");
        const payload = await res.json();
        const state = payload.state || "AP_ACTIVE";
        this.status.state = state;
        this.status.stateLabel = this.describeState(state);
        this.status.ssid = payload.ssid || "";
        this.status.ip = payload.ip || "";
        this.status.apSsid = payload.apSsid || "";
        this.status.mac = payload.mac || "";
        this.status.firmwareVersion = payload.firmwareVersion || "";
        this.status.firmwareDate = payload.firmwareDate || "";
        const apPassValue =
          typeof payload.apPassword === "string"
            ? payload.apPassword
            : this.status.apPassword || "";
        this.status.apPassword = apPassValue;
        if (state === "CONNECTED" && typeof payload.rssi === "number") {
          this.status.rssiLabel = `${payload.rssi} dBm`;
        } else {
          this.status.rssiLabel = "—";
        }
        if (this.busy && state === "CONNECTED") {
          this.busy = false;
          const target =
            this.lastSubmittedSsid || this.status.ssid || "the network";
          this.setMessage(
            "success",
            "Connected",
            `Device joined "${target}". Connect to that network to access the node.`
          );
          this.refreshConfig();
        } else if (this.busy && state === "AP_ACTIVE") {
          this.busy = false;
          const apName = this.status.apSsid || "the device AP";
          const apPass = this.status.apPassword || "unknown";
          this.setMessage(
            "info",
            "SoftAP Available",
            `Credentials cleared or connection failed. Connect to "${apName}" (password: ${apPass}) and open http://192.168.4.1 to continue.`
          );
          this.refreshConfig();
        }
        if (!this.isApMode()) {
          this.networks = [];
          this.loading.scan = false;
          this.hasScanned = false;
        }
      } catch (err) {
        console.error(err);
        this.busy = false;
        this.setMessage(
          "error",
          "Status Error",
          "Unable to refresh device status. Ensure you remain connected to the device."
        );
      } finally {
        this.loading.status = false;
      }
    },

    async refreshConfig() {
      if (this.loading.config) return;
      this.loading.config = true;
      try {
        const res = await fetch("/api/config", { cache: "no-store" });
        if (!res.ok) throw new Error("Config request failed");
        const payload = await res.json();
        this.config.primary = payload.primary || { ssid: "", configured: false };
        this.config.secondary = payload.secondary || { ssid: "", configured: false };
        this.config.connected_to = payload.connected_to || null;
      } catch (err) {
        console.error(err);
      } finally {
        this.loading.config = false;
      }
    },

    async refreshNetworks() {
      if (this.loading.scan) return;
      if (!this.isApMode()) {
        this.hasScanned = false;
        this.networks = [];
        return;
      }
      this.loading.scan = true;
      this.hasScanned = true;
      try {
        const res = await fetch("/api/scan", { cache: "no-store" });
        if (res.status === 409) {
          this.networks = [];
          return;
        }
        if (!res.ok) throw new Error("Scan request failed");
        const list = await res.json();
        if (Array.isArray(list)) {
          const mapped = list
            .map((item, index) => {
              const ssid =
                item && typeof item.ssid === "string" && item.ssid.trim().length
                  ? item.ssid
                  : "(unknown)";
              const rssi =
                item && typeof item.rssi === "number" ? item.rssi : 0;
              const secure = !!(item && item.secure);
              const channel =
                item && typeof item.channel === "number" ? item.channel : 0;
              return {
                ssid,
                rssi,
                secure,
                channel,
                id: `${ssid}-${channel}-${index}`,
              };
            })
            .sort((a, b) => b.rssi - a.rssi);
          this.networks = mapped;
        }
      } catch (err) {
        console.error(err);
        this.setMessage(
          "error",
          "Scan Error",
          "Unable to list nearby networks right now."
        );
      } finally {
        this.loading.scan = false;
      }
    },

    chooseNetwork(network) {
      this.form.ssid = network.ssid;
      if (!network.secure) this.form.pass = "";
      this.clearMessage();
      const triggerScroll = () => this.scrollToForm();
      if (typeof this.$nextTick === "function") {
        this.$nextTick(triggerScroll);
      } else {
        requestAnimationFrame(triggerScroll);
      }
    },

    canSubmit() {
      const ssidOk =
        this.form.ssid.trim().length > 0 && this.form.ssid.trim().length <= 32;
      const passLen = this.form.pass.length;
      const passOk = passLen === 0 || (passLen >= 8 && passLen <= 63);
      return ssidOk && passOk;
    },

    async submit() {
      if (!this.canSubmit()) {
        this.setMessage(
          "error",
          "Validation Error",
          "Check SSID and passphrase lengths."
        );
        return;
      }
      const ssid = this.form.ssid.trim();
      const slot = this.form.slot;
      const slotLabel = slot === "primary" ? "primary" : "secondary";
      const endpoint = slot === "primary" ? "/api/wifi" : "/api/wifi/secondary";

      const confirmation = this.isApMode()
        ? `Set "${ssid}" as ${slotLabel} network and connect now?`
        : `Save "${ssid}" as ${slotLabel} network? ${slot === "primary" ? "This will reconnect the device." : ""}`;
      if (!window.confirm(confirmation)) {
        return;
      }
      this.busy = true;
      this.lastSubmittedSsid = ssid;

      const actionMsg = slot === "primary"
        ? (this.isApMode() ? `Connecting to "${ssid}"…` : `Updating primary and reconnecting to "${ssid}"…`)
        : `Saving "${ssid}" as secondary network…`;
      this.setMessage("info", slot === "primary" ? "Connecting" : "Saving", actionMsg);

      try {
        const res = await fetch(endpoint, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ ssid, pass: this.form.pass }),
        });
        if (res.status === 409) {
          this.busy = false;
          this.setMessage(
            "info",
            "Already Connecting",
            "Please wait until the current connection attempt finishes."
          );
          return;
        }
        if (res.status === 400) {
          const payload = await res.json().catch(() => ({}));
          this.busy = false;
          const code = payload.error || "invalid-request";
          let msg = "Input was not accepted. Verify SSID and passphrase.";
          if (code === "pass-too-short")
            msg = "Passphrase must be at least 8 characters for secure networks.";
          if (code === "invalid-ssid")
            msg = "SSID must be between 1 and 32 characters.";
          this.setMessage("error", "Unable to Save", msg);
          return;
        }
        if (!res.ok) {
          this.busy = false;
          this.setMessage(
            "error",
            "Save Failed",
            "Device could not save credentials. Check credentials and try again."
          );
          this.scrollToMessage();
          return;
        }
        // Success
        if (slot === "secondary") {
          this.busy = false;
          this.setMessage("success", "Saved", `Secondary network "${ssid}" saved successfully.`);
          this.refreshConfig();
        } else {
          this.pollImmediately();
        }
      } catch (err) {
        console.error(err);
        this.busy = false;
        this.setMessage(
          "error",
          "Request Failed",
          "Network request failed. Confirm you are still connected to the device."
        );
        this.scrollToMessage();
      }
    },

    async clearSecondary() {
      if (!window.confirm("Clear the secondary network configuration?")) {
        return;
      }
      this.busy = true;
      try {
        const res = await fetch("/api/wifi/secondary/clear", { method: "POST" });
        if (!res.ok) throw new Error("Clear failed");
        this.setMessage("success", "Cleared", "Secondary network configuration removed.");
        await this.refreshConfig();
      } catch (err) {
        console.error(err);
        this.setMessage("error", "Clear Failed", "Could not clear secondary network.");
      } finally {
        this.busy = false;
      }
    },

    async resetToAp() {
      const apName = this.status.apSsid || "the device AP";
      const apPass = this.status.apPassword || "unknown";
      const confirmed = window.confirm(
        `Reset ALL Wi-Fi settings and re-enable "${apName}" SSID (password: ${apPass}) at http://192.168.4.1? This clears both primary and secondary networks.`
      );
      if (!confirmed) return;
      this.busy = true;
      try {
        const res = await fetch("/api/reset", { method: "POST" });
        if (!res.ok) throw new Error("Reset failed");
        const payload = await res.json();
        this.hasScanned = false;
        this.networks = [];
        this.setMessage(
          "info",
          "SoftAP Enabled",
          `All credentials cleared. Connect to "${payload.apSsid}" (password: ${payload.apPassword}) and open http://192.168.4.1 to continue.`
        );
        await Promise.all([this.refreshStatus(), this.refreshConfig()]);
      } catch (err) {
        console.error(err);
        this.setMessage(
          "error",
          "Reset Failed",
          "Device could not reset credentials. Try again."
        );
        this.scrollToMessage();
      } finally {
        this.busy = false;
      }
    },

    pollImmediately() {
      this.refreshStatus().then(() => {
        if (this.isApMode()) {
          this.refreshNetworks();
        } else {
          this.networks = [];
        }
      });
    },

    startPolling() {
      this.stopPolling();
      this.pollId = setInterval(() => {
        this.refreshStatus();
      }, 3000);
    },

    stopPolling() {
      if (this.pollId) {
        clearInterval(this.pollId);
        this.pollId = null;
      }
    },

    pageTitle() {
      const name = this.status.apSsid || "Device";
      if (this.isApMode()) {
        return `Setup Wi-Fi for ${name}`;
      }
      return `${name} Connected to Wi-Fi`;
    },

    pageSubtitle() {
      return this.isApMode()
        ? "Configure primary and secondary networks for automatic failover."
        : "Update or reset credentials to move the device to a different network.";
    },

    networkHint() {
      return this.isApMode()
        ? "Choose a nearby network or enter credentials manually."
        : "Scanning is only available in AP (Wi-Fi setup) mode. Reset Wi-Fi settings to switch to AP mode.";
    },

    manualHeading() {
      return this.isApMode()
        ? "Save Wi-Fi Credentials"
        : "Override Wi-Fi Credentials";
    },

    manualHint() {
      return this.isApMode()
        ? "Enter the SSID and passphrase. Choose primary for your main network, secondary for backup."
        : "Provide new credentials to replace the current connection.";
    },

    submitLabel() {
      if (this.form.slot === "secondary") {
        return "Save Secondary";
      }
      return this.isApMode() ? "Save & Connect" : "Replace & Connect";
    },

    showResetButton() {
      return !this.isApMode();
    },

    statusClass() {
      switch (this.status.state) {
        case "CONNECTED":
          return "status-pill connected";
        case "CONNECTING":
          return "status-pill connecting";
        default:
          return "status-pill ap";
      }
    },

    describeState(state) {
      if (state === "CONNECTED") return "Connected";
      if (state === "CONNECTING") return "Connecting…";
      return "Access Point Active";
    },

    isApMode() {
      return this.status.state === "AP_ACTIVE";
    },

    isStaMode() {
      return this.status.state === "CONNECTED";
    },

    setMessage(kind, heading, text) {
      this.message.kind = kind;
      this.message.heading = heading;
      this.message.text = text;
    },

    clearMessage() {
      this.message.kind = "";
      this.message.heading = "";
      this.message.text = "";
    },

    scrollToMessage() {
      const notice = document.querySelector(".notice");
      if (notice) {
        notice.scrollIntoView({ behavior: "smooth", block: "start" });
      }
    },

    scrollToForm() {
      const target = document.getElementById("manual-entry");
      if (target) {
        target.scrollIntoView({ behavior: "smooth", block: "start" });
        if (typeof target.focus === "function") {
          target.focus({ preventScroll: true });
        }
      }
    },

    formatFirmwareDate(isoDate) {
      if (!isoDate) return "";
      try {
        const d = new Date(isoDate);
        const date = d.toLocaleDateString(undefined, {
          year: "numeric",
          month: "short",
          day: "numeric",
        });
        const time = d.toLocaleTimeString(undefined, {
          hour: "2-digit",
          minute: "2-digit",
        });
        return `${date} ${time}`;
      } catch {
        return isoDate.split("T")[0];
      }
    },
  };
}

window.wifiPortal = wifiPortal;
