#include "transport/MessageId.h"

#include <array>
#include <cstdio>
#include <mutex>
#include <random>
#include <string>
#include <utility>

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
#include <esp_system.h>
#endif

namespace {

std::mutex &IdMutex() {
  static std::mutex m;
  return m;
}

std::function<std::string()> &Generator() {
  static std::function<std::string()> gen;
  return gen;
}

std::string &ActiveId() {
  static std::string active;
  return active;
}

std::string &LastIssuedId() {
  static std::string last;
  return last;
}

std::string GenerateUuidV4() {
  std::array<uint8_t, 16> bytes{};

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
  for (size_t i = 0; i < bytes.size(); i += 4) {
    uint32_t r = esp_random();
    bytes[i + 0] = static_cast<uint8_t>(r & 0xFF);
    bytes[i + 1] = static_cast<uint8_t>((r >> 8) & 0xFF);
    bytes[i + 2] = static_cast<uint8_t>((r >> 16) & 0xFF);
    bytes[i + 3] = static_cast<uint8_t>((r >> 24) & 0xFF);
  }
#else
  static std::random_device rd;
  static std::mt19937_64 rng(rd());
  for (size_t i = 0; i < bytes.size();) {
    uint64_t value = rng();
    for (size_t j = 0; j < 8 && i < bytes.size(); ++j, ++i) {
      bytes[i] = static_cast<uint8_t>((value >> (j * 8)) & 0xFF);
    }
  }
#endif

  // RFC 4122 version 4 UUID formatting.
  bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
  bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

  char out[37];
  std::snprintf(out, sizeof(out),
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                bytes[0], bytes[1], bytes[2], bytes[3],
                bytes[4], bytes[5],
                bytes[6], bytes[7],
                bytes[8], bytes[9],
                bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
  return std::string(out);
}

std::string DefaultUuidFactory() {
  return GenerateUuidV4();
}

} // namespace

namespace transport {
namespace message_id {

std::string Next() {
  std::lock_guard<std::mutex> lock(IdMutex());
  if (!Generator()) {
    Generator() = DefaultUuidFactory;
  }
  std::string next;
  do {
    next = Generator()();
  } while (!ActiveId().empty() && next == ActiveId());
  if (!LastIssuedId().empty() && next == LastIssuedId()) {
    next = Generator()();
  }
  LastIssuedId() = next;
  return next;
}

void SetActive(const std::string &msg_id) {
  std::lock_guard<std::mutex> lock(IdMutex());
  ActiveId() = msg_id;
}

bool HasActive() {
  std::lock_guard<std::mutex> lock(IdMutex());
  return !ActiveId().empty();
}

std::string Active() {
  std::lock_guard<std::mutex> lock(IdMutex());
  return ActiveId();
}

void ClearActive() {
  std::lock_guard<std::mutex> lock(IdMutex());
  ActiveId().clear();
}

void SetGenerator(std::function<std::string()> generator) {
  std::lock_guard<std::mutex> lock(IdMutex());
  Generator() = std::move(generator);
}

void ResetGenerator() {
  std::lock_guard<std::mutex> lock(IdMutex());
  Generator() = DefaultUuidFactory;
}

} // namespace message_id
} // namespace transport

