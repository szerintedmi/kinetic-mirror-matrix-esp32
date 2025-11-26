#pragma once

#include <array>
#include <cstddef>

// Platform-independent serial input buffer with overflow handling.
// Can be unit tested in native environment without Arduino dependencies.
class SerialInputBuffer {
public:
  static constexpr size_t kBufferSize = 512;
  static constexpr char kBackspaceChar = 0x08;
  static constexpr char kDeleteChar = 0x7F;

  // Result of adding a character
  enum class AddResult {
    kContinue,      // Character added, keep reading
    kLineReady,     // Newline received, line is ready (call getLine())
    kOverflowError  // Newline received after overflow (line is invalid)
  };

  // Add a character to the buffer.
  // Returns the result indicating what action to take.
  AddResult addChar(char c);

  // Get the completed line (valid after addChar returns kLineReady).
  // Returns null-terminated string.
  const char* getLine() const;

  // Get current buffer length (for testing/debugging).
  size_t length() const { return length_; }

  // Check if buffer is in overflow state (for testing).
  bool isOverflowPending() const { return overflow_pending_; }

  // Reset buffer for next line.
  void reset();

private:
  std::array<char, kBufferSize> buffer_{};
  size_t length_ = 0;
  bool overflow_pending_ = false;
};
