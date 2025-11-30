#pragma once

#include "mqtt/MqttPresenceClient.h"

#include <algorithm>
#include <deque>

namespace mqtt {

// Enqueues a message with priority: command responses over status updates.
// When queue is full, status updates are dropped or replaced first.
inline bool EnqueuePublishMessage(std::deque<PublishMessage>& queue,
                                  std::size_t capacity,
                                  const PublishMessage& msg) {
  if (msg.is_status) {
    // Replace existing status message if present
    for (auto& pending : queue) {
      if (pending.is_status) {
        pending = msg;
        return true;
      }
    }
    // Drop status if queue full of command responses
    if (queue.size() >= capacity) {
      return true;
    }
    queue.push_back(msg);
    return true;
  }

  // Command response: make room if needed
  if (queue.size() >= capacity) {
    auto drop_status = std::find_if(queue.begin(),
                                    queue.end(),
                                    [](const PublishMessage& pending) {
                                      return pending.is_status;
                                    });
    if (drop_status != queue.end()) {
      queue.erase(drop_status);
    } else if (!queue.empty()) {
      queue.pop_front();
    }
  }

  queue.push_back(msg);
  return true;
}

}  // namespace mqtt
