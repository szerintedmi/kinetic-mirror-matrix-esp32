#pragma once

// Initializes the serial console and command processor stack.
// Safe to call once from Arduino setup().
void serial_console_setup();

// Polls serial input, echoes characters, processes commands, and advances
// command-processor timing. Call frequently from Arduino loop().
void serial_console_tick();
