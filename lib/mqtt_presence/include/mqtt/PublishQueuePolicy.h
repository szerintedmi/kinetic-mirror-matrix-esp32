#pragma once

#include "mqtt/MqttPresenceClient.h"

#include <algorithm>
#include <deque>

namespace mqtt {

inline bool EnqueuePublishMessage(std::deque<PublishMessage>& queue,
                                  std::size_t capacity,
                                  const PublishMessage& msg) {
  if (msg.is_status) {
    for (auto& pending : queue) {
      if (pending.is_status) {
        pending = msg;
        return true;
      }
    }
    if (queue.size() >= capacity) {
      // Queue is full and contains only command responses; drop this status update.
      return true;
    }
    queue.push_back(msg);
    return true;
  }

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
