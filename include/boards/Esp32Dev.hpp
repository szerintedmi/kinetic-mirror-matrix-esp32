#pragma once

// ESP32 DevKit default pin assignments for this project

// VSPI hardware pins
constexpr int VSPI_SCK  = 18;
constexpr int VSPI_MISO = 19; // unused in 74HC595 path
constexpr int VSPI_MOSI = 23;
constexpr int VSPI_SS   = -1; // not used; latch (RCLK) handled separately

// 74HC595 latch (RCLK) pin
constexpr int SHIFT595_RCLK = 5;

// STEP pins for motors 0..7
constexpr int STEP_PINS[8] = {4, 16, 17, 25, 26, 27, 32, 33};

