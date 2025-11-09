#include "drivers/Esp32/Shift595Vspi.h"

#include "boards/Esp32Dev.hpp"

#include <Arduino.h>
#include <SPI.h>

// Use VSPI on ESP32
static SPIClass spi(VSPI);

Shift595Esp32::Shift595Esp32(int latch_pin, int oe_pin) : latch_pin_(latch_pin), oe_pin_(oe_pin) {}

void Shift595Esp32::begin() {
  pinMode(latch_pin_, OUTPUT);
  digitalWrite(latch_pin_, LOW);
  if (oe_pin_ >= 0) {
    pinMode(oe_pin_, OUTPUT);
    // Disable outputs until first known-good latch to avoid waking all motors while booting
    digitalWrite(oe_pin_, HIGH);  // HIGH = disabled (tri-state)
    outputs_enabled_ = false;
  }
  // VSPI default pins from board config; SS unused (we manage latch separately)
  spi.begin(VSPI_SCK /*SCK*/, VSPI_MISO /*MISO*/, VSPI_MOSI /*MOSI*/, VSPI_SS /*SS*/);
}

void Shift595Esp32::setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) {
  spi.beginTransaction(SPISettings(5000000, MSBFIRST, SPI_MODE0));
  // Send DIR then SLEEP
  spi.transfer(dir_bits);
  spi.transfer(sleep_bits);
  spi.endTransaction();
  // Latch rising edge
  digitalWrite(latch_pin_, HIGH);
  delayMicroseconds(1);
  digitalWrite(latch_pin_, LOW);
  // Enable outputs on first valid update if OE is controlled
  if (oe_pin_ >= 0 && !outputs_enabled_) {
    digitalWrite(oe_pin_, LOW);  // LOW = enable outputs
    outputs_enabled_ = true;
  }
}
