#include <Arduino.h>
#include <SPI.h>
#include "boards/Esp32Dev.hpp"
#include "drivers/Esp32/Shift595Vspi.h"

// Use VSPI on ESP32
static SPIClass spi(VSPI);

Shift595Esp32::Shift595Esp32(int latch_pin) : latch_pin_(latch_pin) {}

void Shift595Esp32::begin() {
  pinMode(latch_pin_, OUTPUT);
  digitalWrite(latch_pin_, LOW);
  // VSPI default pins from board config; SS unused (we manage latch separately)
  spi.begin(VSPI_SCK /*SCK*/, VSPI_MISO /*MISO*/, VSPI_MOSI /*MOSI*/, VSPI_SS /*SS*/);
}

void Shift595Esp32::setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) {
  spi.beginTransaction(SPISettings(5000000, MSBFIRST, SPI_MODE0));
  spi.transfer(dir_bits);
  spi.transfer(sleep_bits);
  spi.endTransaction();
  // Latch rising edge
  digitalWrite(latch_pin_, HIGH);
  delayMicroseconds(1);
  digitalWrite(latch_pin_, LOW);
}
