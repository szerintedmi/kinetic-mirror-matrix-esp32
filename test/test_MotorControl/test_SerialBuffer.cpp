#include "serial_buffer/SerialInputBuffer.h"

#include <cstring>
#include <string>
#include <unity.h>

// Test normal input accumulation and line completion
void test_buffer_normal_input() {
  SerialInputBuffer buf;

  // Add characters
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kContinue, buf.addChar('H'));
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kContinue, buf.addChar('E'));
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kContinue, buf.addChar('L'));
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kContinue, buf.addChar('P'));
  TEST_ASSERT_EQUAL(4u, buf.length());

  // Complete with newline
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kLineReady, buf.addChar('\n'));
  TEST_ASSERT_EQUAL_STRING("HELP", buf.getLine());
}

// Test that carriage return is ignored
void test_buffer_ignores_cr() {
  SerialInputBuffer buf;

  buf.addChar('A');
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kContinue, buf.addChar('\r'));
  buf.addChar('B');
  buf.addChar('\n');

  TEST_ASSERT_EQUAL_STRING("AB", buf.getLine());
}

// Test overflow detection when buffer is full
void test_buffer_overflow_sets_flag() {
  SerialInputBuffer buf;

  // Fill buffer to capacity (511 chars + null terminator)
  for (size_t i = 0; i < SerialInputBuffer::kBufferSize - 1; ++i) {
    auto result = buf.addChar('X');
    TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kContinue, result);
    TEST_ASSERT_FALSE(buf.isOverflowPending());
  }
  TEST_ASSERT_EQUAL(SerialInputBuffer::kBufferSize - 1, buf.length());

  // One more char triggers overflow
  buf.addChar('Y');
  TEST_ASSERT_TRUE(buf.isOverflowPending());
}

// Test that overflow discards characters until newline
void test_buffer_overflow_discards_until_newline() {
  SerialInputBuffer buf;

  // Fill buffer to trigger overflow
  for (size_t i = 0; i < SerialInputBuffer::kBufferSize; ++i) {
    buf.addChar('X');
  }
  TEST_ASSERT_TRUE(buf.isOverflowPending());

  // Additional characters should be discarded
  buf.addChar('A');
  buf.addChar('B');
  buf.addChar('C');
  TEST_ASSERT_TRUE(buf.isOverflowPending());

  // Newline should return overflow error
  auto result = buf.addChar('\n');
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kOverflowError, result);
  TEST_ASSERT_FALSE(buf.isOverflowPending());
}

// Test recovery after overflow
void test_buffer_recovery_after_overflow() {
  SerialInputBuffer buf;

  // Trigger overflow
  for (size_t i = 0; i < SerialInputBuffer::kBufferSize + 10; ++i) {
    buf.addChar('X');
  }
  buf.addChar('\n');  // Clear overflow state

  // Buffer should be reset and ready for new input
  TEST_ASSERT_FALSE(buf.isOverflowPending());
  TEST_ASSERT_EQUAL(0u, buf.length());

  // New input should work normally
  buf.addChar('O');
  buf.addChar('K');
  auto result = buf.addChar('\n');
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kLineReady, result);
  TEST_ASSERT_EQUAL_STRING("OK", buf.getLine());
}

// Test backspace handling
void test_buffer_backspace_handling() {
  SerialInputBuffer buf;

  buf.addChar('A');
  buf.addChar('B');
  buf.addChar('C');
  TEST_ASSERT_EQUAL(3u, buf.length());

  // Backspace removes last char
  buf.addChar(SerialInputBuffer::kBackspaceChar);
  TEST_ASSERT_EQUAL(2u, buf.length());

  // Delete also removes last char
  buf.addChar(SerialInputBuffer::kDeleteChar);
  TEST_ASSERT_EQUAL(1u, buf.length());

  // Backspace on empty buffer is no-op
  buf.addChar(SerialInputBuffer::kBackspaceChar);
  buf.addChar(SerialInputBuffer::kBackspaceChar);
  TEST_ASSERT_EQUAL(0u, buf.length());

  buf.addChar('D');
  buf.addChar('\n');
  TEST_ASSERT_EQUAL_STRING("D", buf.getLine());
}

// Test exactly max size input (should not overflow)
void test_buffer_exactly_max_size() {
  SerialInputBuffer buf;

  // Fill to exactly kBufferSize - 1 (max valid input length)
  for (size_t i = 0; i < SerialInputBuffer::kBufferSize - 1; ++i) {
    buf.addChar('M');
  }
  TEST_ASSERT_EQUAL(SerialInputBuffer::kBufferSize - 1, buf.length());
  TEST_ASSERT_FALSE(buf.isOverflowPending());

  // Should complete successfully
  auto result = buf.addChar('\n');
  TEST_ASSERT_EQUAL(SerialInputBuffer::AddResult::kLineReady, result);
  TEST_ASSERT_EQUAL(SerialInputBuffer::kBufferSize - 1, strlen(buf.getLine()));
}

// Test reset functionality
void test_buffer_reset() {
  SerialInputBuffer buf;

  buf.addChar('T');
  buf.addChar('E');
  buf.addChar('S');
  buf.addChar('T');
  TEST_ASSERT_EQUAL(4u, buf.length());

  buf.reset();
  TEST_ASSERT_EQUAL(0u, buf.length());
  TEST_ASSERT_FALSE(buf.isOverflowPending());

  // Can accept new input after reset
  buf.addChar('N');
  buf.addChar('E');
  buf.addChar('W');
  buf.addChar('\n');
  TEST_ASSERT_EQUAL_STRING("NEW", buf.getLine());
}

// No main(); tests are registered in test_main.cpp
