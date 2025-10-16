#ifndef UNIT_TEST
#include <Arduino.h>
#include <SPI.h>
#include "drivers/shift595.h"

// Use VSPI on ESP32
static SPIClass spi(VSPI);

Shift595::Shift595(int latch_pin) : latch_pin_(latch_pin) {}

void Shift595::begin() {
  pinMode(latch_pin_, OUTPUT);
  digitalWrite(latch_pin_, LOW);
  // VSPI default pins: SCK=18, MISO=19, MOSI=23, SS=5 (unused); we'll control latch separately
  spi.begin(18 /*SCK*/, 19 /*MISO*/, 23 /*MOSI*/, -1 /*SS*/);
}

void Shift595::setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) {
  spi.beginTransaction(SPISettings(5000000, MSBFIRST, SPI_MODE0));
  spi.transfer(dir_bits);
  spi.transfer(sleep_bits);
  spi.endTransaction();
  // Latch rising edge
  digitalWrite(latch_pin_, HIGH);
  delayMicroseconds(1);
  digitalWrite(latch_pin_, LOW);
}

#else
// UNIT_TEST (native): stub implementation that records bytes and latch count
#include "drivers/shift595.h"

Shift595::Shift595(int latch_pin) : latch_pin_(latch_pin) {}

void Shift595::begin() {
  last_dir_ = 0;
  last_sleep_ = 0;
  latch_count_ = 0;
}

void Shift595::setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) {
  last_dir_ = dir_bits;
  last_sleep_ = sleep_bits;
  latch_count_++;
}

#endif

