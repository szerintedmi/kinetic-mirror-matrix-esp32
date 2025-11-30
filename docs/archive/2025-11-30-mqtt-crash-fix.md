# MQTT Crash Fix - Rapid Command Bursts

## Problem

ESP32 crashed with `LoadProhibited` exception during rapid MQTT command bursts (8 MOVE commands in quick succession).

**Crash Signature:**
```
Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
EXCVADDR: 0xbaad5678  ← Heap poison pattern = use-after-free
```

## Root Causes

Two separate race conditions were identified and fixed:

### 1. Thread-Safe Command Queue (MqttCommandServer)

**Problem:** MQTT commands were processed directly in AsyncTCP callback (wrong thread).

**Solution:** Queue commands in callback, process in `loop()` on main thread.

```cpp
void MqttCommandServer::handleIncoming(...) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  command_queue_.push_back({topic, payload});
}

void MqttCommandServer::loop(uint32_t now_ms) {
  std::deque<PendingMessage> processing_queue;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    processing_queue = std::move(command_queue_);
  }
  // Process on main thread...
}
```

### 2. Eager Drain Reentrancy (MqttPresenceClient)

**Problem:** An optimization that called `drainPublishQueue()` from within `enqueue()` caused use-after-free when TCP buffer was full.

**Why it crashed:**
1. `enqueue()` called `drainPublishQueue()` eagerly
2. `drainPublishQueue()` used `std::move()` to extract messages
3. When TCP buffer full, message was requeued with another `std::move()`
4. Multiple move operations corrupted std::string internal state

**Solution:** Remove eager drain, increase tick frequency instead.

```cpp
// Before (dangerous):
bool enqueue(const PublishMessage& msg) {
  bool result = EnqueuePublishMessage(...);
  if (client_.connected()) {
    drainPublishQueue();  // ← Caused crash
  }
  return result;
}

// After (safe):
bool enqueue(const PublishMessage& msg) {
  return EnqueuePublishMessage(...);
}
```

## Configuration Changes

| Setting | Before | After | Rationale |
|---------|--------|-------|-----------|
| `kSlowTickIntervalMs` | 50ms | 20ms | Faster drain without reentrancy |
| `motion_interval_ms` | 200ms | 500ms | Reduce TCP buffer pressure |

## Files Modified

| File | Change |
|------|--------|
| `lib/mqtt_commands/src/MqttCommandServer.cpp` | Thread-safe command queue |
| `lib/mqtt_commands/include/mqtt/MqttCommandServer.h` | PendingMessage struct, mutex |
| `lib/mqtt_presence/src/MqttPresenceClient.cpp` | Remove eager drain |
| `src/console/SerialConsole.cpp` | 20ms tick, 500ms status interval |

## Key Learnings

1. **AsyncTCP callbacks run on a different task** - never do heavy processing there
2. **`std::move` on requeued objects can corrupt state** - avoid moving objects multiple times
3. **Heap poison pattern `0xbaad5678`** indicates use-after-free in ESP-IDF
4. **Single-threaded != no reentrancy** - callbacks can create reentrancy even without threads
