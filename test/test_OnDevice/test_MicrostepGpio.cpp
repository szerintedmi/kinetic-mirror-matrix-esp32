#ifdef ARDUINO
#include <Arduino.h>
#include <unity.h>

#include "boards/Esp32Dev.hpp"
#include "drivers/Esp32/MicrostepGpio.h"

// DRV8825 microstepping truth table verification
// MODE0    MODE1    MODE2    Resolution
// Low      Low      Low      Full step
// High     Low      Low      Half step
// Low      High     Low      1/4 step
// High     High     Low      1/8 step
// Low      Low      High     1/16 step
// High     Low      High     1/32 step

void test_microstep_full_step_pins() {
  MicrostepGpio gpio(MICROSTEP_M0_PIN, MICROSTEP_M1_PIN, MICROSTEP_M2_PIN);
  gpio.begin();
  gpio.setMode(MicrostepMode::FULL);
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M0_PIN));
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M1_PIN));
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M2_PIN));
  TEST_ASSERT_EQUAL(1, gpio.multiplier());
}

void test_microstep_half_step_pins() {
  MicrostepGpio gpio(MICROSTEP_M0_PIN, MICROSTEP_M1_PIN, MICROSTEP_M2_PIN);
  gpio.begin();
  gpio.setMode(MicrostepMode::HALF);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(MICROSTEP_M0_PIN));
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M1_PIN));
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M2_PIN));
  TEST_ASSERT_EQUAL(2, gpio.multiplier());
}

void test_microstep_quarter_step_pins() {
  MicrostepGpio gpio(MICROSTEP_M0_PIN, MICROSTEP_M1_PIN, MICROSTEP_M2_PIN);
  gpio.begin();
  gpio.setMode(MicrostepMode::QUARTER);
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M0_PIN));
  TEST_ASSERT_EQUAL(HIGH, digitalRead(MICROSTEP_M1_PIN));
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M2_PIN));
  TEST_ASSERT_EQUAL(4, gpio.multiplier());
}

void test_microstep_eighth_step_pins() {
  MicrostepGpio gpio(MICROSTEP_M0_PIN, MICROSTEP_M1_PIN, MICROSTEP_M2_PIN);
  gpio.begin();
  gpio.setMode(MicrostepMode::EIGHTH);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(MICROSTEP_M0_PIN));
  TEST_ASSERT_EQUAL(HIGH, digitalRead(MICROSTEP_M1_PIN));
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M2_PIN));
  TEST_ASSERT_EQUAL(8, gpio.multiplier());
}

void test_microstep_sixteenth_step_pins() {
  MicrostepGpio gpio(MICROSTEP_M0_PIN, MICROSTEP_M1_PIN, MICROSTEP_M2_PIN);
  gpio.begin();
  gpio.setMode(MicrostepMode::SIXTEENTH);
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M0_PIN));
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M1_PIN));
  TEST_ASSERT_EQUAL(HIGH, digitalRead(MICROSTEP_M2_PIN));
  TEST_ASSERT_EQUAL(16, gpio.multiplier());
}

void test_microstep_thirtysecond_step_pins() {
  MicrostepGpio gpio(MICROSTEP_M0_PIN, MICROSTEP_M1_PIN, MICROSTEP_M2_PIN);
  gpio.begin();
  gpio.setMode(MicrostepMode::THIRTY_SECOND);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(MICROSTEP_M0_PIN));
  TEST_ASSERT_EQUAL(LOW, digitalRead(MICROSTEP_M1_PIN));
  TEST_ASSERT_EQUAL(HIGH, digitalRead(MICROSTEP_M2_PIN));
  TEST_ASSERT_EQUAL(32, gpio.multiplier());
}

#endif  // ARDUINO
