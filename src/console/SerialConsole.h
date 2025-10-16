#pragma once

// Initializes the serial console and protocol stack.
// Safe to call once from Arduino setup().
void serial_console_setup();

// Polls serial input, echoes characters, processes commands, and advances
// protocol timing. Call frequently from Arduino loop().
void serial_console_tick();

