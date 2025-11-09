#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))

#include "drivers/Esp32/SharedStepRmt.h"
#include "MotorControl/SharedStepTiming.h"
#include <Arduino.h>
#include <driver/rmt.h>

// RMT-based shared STEP generator
#ifndef SHARED_STEP_GPIO
#define SHARED_STEP_GPIO 4 // NOLINT(cppcoreguidelines-macro-usage)
#endif
static constexpr int kStepPin = SHARED_STEP_GPIO; // overridable via -DSHARED_STEP_GPIO
static constexpr rmt_channel_t kRmtChannel = RMT_CHANNEL_7; // avoid common channel collisions

// Forward decl of ISR callback
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static void IRAM_ATTR on_rmt_tx_end(rmt_channel_t channel, void* arg);

SharedStepRmtGenerator::SharedStepRmtGenerator() = default;

struct SquareWaveDurations {
  uint16_t high_us = 0;
  uint16_t low_us = 0;
  explicit constexpr SquareWaveDurations(uint16_t high = 0, uint16_t low = 0) : high_us(high), low_us(low) {}
};

static inline rmt_item32_t make_square_item(SquareWaveDurations durations)
{
  rmt_item32_t square_item{};
  square_item.level0 = 1;
  square_item.duration0 = durations.high_us;
  square_item.level1 = 0;
  square_item.duration1 = durations.low_us;
  return square_item;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
void IRAM_ATTR SharedStepRmtGenerator::onTxEndIsr()
{
  if (!running_) return;
  uint32_t period = SharedStepTiming::step_period_us(speed_sps_);
  if (period < 2) period = 2;
  uint32_t half = period / 2;
  if (half > 32767) half = 32767;
  rmt_item32_t item = make_square_item(SquareWaveDurations(static_cast<uint16_t>(half), static_cast<uint16_t>(half)));
  // Queue next item without waiting; generator continues seamlessly
  rmt_write_items(kRmtChannel, &item, 1, false);
  if (hook_) {
    hook_();
  }
}

bool SharedStepRmtGenerator::begin()
{
  // Configure STEP pin low as idle
  pinMode(kStepPin, OUTPUT);
  digitalWrite(kStepPin, LOW);
  // Configure RMT for 1us resolution, TX on kStepPin, idle low, NO loop mode
  rmt_config_t cfg{};
  cfg.rmt_mode = RMT_MODE_TX;
  cfg.channel = kRmtChannel;
  cfg.gpio_num = (gpio_num_t)kStepPin;
  cfg.clk_div = 80; // 80MHz / 80 = 1MHz → 1 tick = 1us
  cfg.mem_block_num = 1;
  cfg.tx_config.loop_en = false; // use ISR-driven requeue instead of loop mode
  cfg.tx_config.carrier_en = false;
  cfg.tx_config.idle_output_en = true;
  cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  if (rmt_config(&cfg) != ESP_OK) return false;
  if (rmt_driver_install(kRmtChannel, 0, 0) != ESP_OK) return false;
  // Register TX-end callback used to requeue the square item continuously
  rmt_register_tx_end_callback(on_rmt_tx_end, this);
  return true;
}

void SharedStepRmtGenerator::setSpeed(uint32_t speed_sps)
{
  speed_sps_ = speed_sps;
  if (speed_sps_ == 0)
  {
    // Stop output immediately
    rmt_tx_stop(kRmtChannel);
    running_ = false;
    return;
  }
  // Compute 50% duty square with 1us ticks
  uint32_t period = SharedStepTiming::step_period_us(speed_sps_);
  if (period < 2) period = 2;
  uint32_t half = period / 2;
  if (half > 32767) half = 32767; // duration field limit
  // If already running, the next ISR requeue will pick up the new half period
  // by reconstructing the item on the fly.
  (void)half; // no immediate write here
}

void SharedStepRmtGenerator::start()
{
  if (speed_sps_ == 0)
    return;
  if (running_) return; // idempotent start
  // Start TX by writing a single square item; ISR will requeue
  uint32_t period = SharedStepTiming::step_period_us(speed_sps_);
  if (period < 2) period = 2;
  uint32_t half = period / 2;
  if (half > 32767) half = 32767;
  rmt_item32_t item = make_square_item(SquareWaveDurations(static_cast<uint16_t>(half), static_cast<uint16_t>(half)));
  rmt_write_items(kRmtChannel, &item, 1, false);
  running_ = true;
}

void SharedStepRmtGenerator::stop()
{
  rmt_tx_stop(kRmtChannel);
  running_ = false;
}

void SharedStepRmtGenerator::setEdgeHook(SharedStepEdgeHook hook)
{
  hook_ = hook;
}

// ISR: RMT TX end. Requeue the single square item for continuous output.
static void IRAM_ATTR on_rmt_tx_end(rmt_channel_t channel, void* arg)
{
  if (channel != kRmtChannel) return;
  auto* self = reinterpret_cast<SharedStepRmtGenerator*>(arg);
  if (!self) return;
  self->onTxEndIsr();
}

#endif // ARDUINO ESP32
