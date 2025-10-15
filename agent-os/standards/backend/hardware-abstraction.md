## Hardware abstraction

- **Interface first**: Define clean C++ interfaces in `include/hal/`; concrete drivers live in `src/drivers/`. Keeps mocks trivial and lets you swap implementations per board.
- **Keep board config declarative**: Store pin maps and peripheral selects in `boards/<target>.hpp`; lets RP2040 builds reuse logic by swapping headers.
- **Driver init belongs in one module**: Expose `bool InitPeripherals()` that handles clocks, I2C/SPI buses, and error logging; avoids reinit bugs when tasks restart.
- **Guard shared buses**: Wrap I2C/SPI access with mutex-protected helpers so sensor and network stacks cannot starve each other.
- **Expose async errors**: Return status enums instead of `bool`; surface recoverable faults to tasks so watchdogs can decide whether to reboot.
