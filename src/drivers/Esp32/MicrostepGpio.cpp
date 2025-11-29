#include "drivers/Esp32/MicrostepGpio.h"

#include <Arduino.h>

MicrostepGpio::MicrostepGpio(int m0_pin, int m1_pin, int m2_pin)
    : m0_pin_(m0_pin), m1_pin_(m1_pin), m2_pin_(m2_pin) {}

void MicrostepGpio::begin() {
  pinMode(m0_pin_, OUTPUT);
  pinMode(m1_pin_, OUTPUT);
  pinMode(m2_pin_, OUTPUT);
  // Initialize to full-step mode (all LOW)
  current_mode_ = MicrostepMode::FULL;
  applyPinStates();
}

void MicrostepGpio::setMode(MicrostepMode mode) {
  current_mode_ = mode;
  applyPinStates();
}

void MicrostepGpio::applyPinStates() {
  // DRV8825 microstepping truth table
  // MODE0    MODE1    MODE2    Resolution
  // Low      Low      Low      Full step
  // High     Low      Low      Half step
  // Low      High     Low      1/4 step
  // High     High     Low      1/8 step
  // Low      Low      High     1/16 step
  // High     Low      High     1/32 step
  switch (current_mode_) {
  case MicrostepMode::FULL:
    digitalWrite(m0_pin_, LOW);
    digitalWrite(m1_pin_, LOW);
    digitalWrite(m2_pin_, LOW);
    break;
  case MicrostepMode::HALF:
    digitalWrite(m0_pin_, HIGH);
    digitalWrite(m1_pin_, LOW);
    digitalWrite(m2_pin_, LOW);
    break;
  case MicrostepMode::QUARTER:
    digitalWrite(m0_pin_, LOW);
    digitalWrite(m1_pin_, HIGH);
    digitalWrite(m2_pin_, LOW);
    break;
  case MicrostepMode::EIGHTH:
    digitalWrite(m0_pin_, HIGH);
    digitalWrite(m1_pin_, HIGH);
    digitalWrite(m2_pin_, LOW);
    break;
  case MicrostepMode::SIXTEENTH:
    digitalWrite(m0_pin_, LOW);
    digitalWrite(m1_pin_, LOW);
    digitalWrite(m2_pin_, HIGH);
    break;
  case MicrostepMode::THIRTY_SECOND:
    digitalWrite(m0_pin_, HIGH);
    digitalWrite(m1_pin_, LOW);
    digitalWrite(m2_pin_, HIGH);
    break;
  }
}
