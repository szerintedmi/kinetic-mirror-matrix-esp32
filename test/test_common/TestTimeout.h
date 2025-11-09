#pragma once

#if defined(ARDUINO)
// On embedded targets we skip host-only timeout handling.
struct TestTimeoutGuard {
  TestTimeoutGuard(const char*, unsigned int) {}
};
#define TEST_TIMEOUT_GUARD(ms) TestTimeoutGuard guard(__func__, ms)
#else

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

class TestTimeoutGuard {
public:
  TestTimeoutGuard(const char* test_name, unsigned int timeout_ms)
      : name_(test_name ? test_name : "(unknown)"), done_(false) {
    if (timeout_ms == 0) {
      return;
    }
    worker_ = std::thread([this, timeout_ms]() {
      const auto limit = std::chrono::milliseconds(timeout_ms);
      const auto deadline = std::chrono::steady_clock::now() + limit;
      while (!done_.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) {
          std::fprintf(
              stderr, "\n[FATAL] Test '%s' exceeded timeout of %u ms\n", name_.c_str(), timeout_ms);
          std::fflush(stderr);
          std::abort();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    });
  }

  ~TestTimeoutGuard() {
    done_.store(true, std::memory_order_release);
    if (worker_.joinable()) {
      worker_.join();
    }
  }

private:
  std::string name_;
  std::atomic<bool> done_;
  std::thread worker_;
};

#define TEST_TIMEOUT_GUARD(ms) ::TestTimeoutGuard guard(__func__, ms)

#endif
