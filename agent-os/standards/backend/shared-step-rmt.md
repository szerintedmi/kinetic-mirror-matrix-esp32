# Shared-Step RMT Driver

## Architecture

- Single RMT channel generates shared STEP pulses for all motors
- All motors share one speed — permanent architectural constraint
- Individual motor control via DIR/SLEEP bit masking (shift register)
- ISR-driven requeue: TX-end interrupt reconstructs and queues next square wave item

## RMT Configuration

- Clock divider: 80 (1µs resolution)
- Loop mode: disabled (ISR-driven requeue instead)
- Min period: 2µs (500kHz), max half-duration: 32767 ticks (32.7ms ≈ 30 Hz)

## Direction Flip Guards

- 3µs pre + 3µs post guard window (empirically tuned)
- Guard must fit within STEP period gap; if period too short, flip rejected
- Scheduled at midpoint between STEP edges for maximum safety margin
- Phase anchor (`phase_anchor_us_`) tracks RMT ISR rising edges via hook callback
- Fallback: if flip can't be scheduled, immediate sequence while generator idle with 3µs delay

## Output Enable

- 74HC595 OE pin held HIGH (disabled) during boot
- Enabled on first successful SPI latch — prevents spurious motor wake during init
