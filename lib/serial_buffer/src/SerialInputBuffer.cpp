#include "serial_buffer/SerialInputBuffer.h"

SerialInputBuffer::AddResult SerialInputBuffer::addChar(char c) {
  // Ignore carriage return
  if (c == '\r') {
    return AddResult::kContinue;
  }

  // Newline completes the line
  if (c == '\n') {
    if (overflow_pending_) {
      // Overflow occurred - signal error and reset
      overflow_pending_ = false;
      length_ = 0;
      return AddResult::kOverflowError;
    }
    // Null-terminate the buffer
    buffer_[length_] = '\0';
    return AddResult::kLineReady;
  }

  // If overflow pending, discard characters until newline
  if (overflow_pending_) {
    return AddResult::kContinue;
  }

  // Handle backspace/delete
  if (c == kBackspaceChar || c == kDeleteChar) {
    if (length_ > 0) {
      length_--;
    }
    return AddResult::kContinue;
  }

  // Add character if there's room (need space for null terminator)
  if (length_ + 1 < buffer_.size()) {
    buffer_[length_++] = c;
    return AddResult::kContinue;
  }

  // Buffer full - mark overflow
  overflow_pending_ = true;
  return AddResult::kContinue;
}

const char* SerialInputBuffer::getLine() const {
  return buffer_.data();
}

void SerialInputBuffer::reset() {
  length_ = 0;
  overflow_pending_ = false;
}
