## Resource management

- **Plan static budgets**: Document RAM/flash limits per target; reserve headroom (≈20%) so FreeRTOS stacks and heap never collide during spikes.
- **Prefer compile-time buffers**: Use `std::array` or `StaticQueue_t` for predictable allocation.
- **Throttle CPU usage**: Insert `vTaskDelay` or `ulTaskNotifyTake` instead of busy loops; keeps ESP32 cores cooler and frees cycles on RP2040.
- **Power-aware peripherals**: Gate high-draw components with GPIO or power domains and centralize the control API so sleep behavior stays consistent.
- **Watchdog discipline**: Feed watchdogs inside a supervised helper (`KickWatchdog()`), never scatter direct calls; easier to disable when debugging.
