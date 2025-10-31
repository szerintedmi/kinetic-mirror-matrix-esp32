#include "transport/ResponseDispatcher.h"

namespace transport {
namespace response {

ResponseDispatcher &ResponseDispatcher::Instance() {
  static ResponseDispatcher instance;
  return instance;
}

ResponseDispatcher::SinkToken ResponseDispatcher::RegisterSink(SinkCallback cb) {
  if (!cb) {
    return 0;
  }
  SinkToken token = next_token_++;
  sinks_[token] = std::move(cb);
  return token;
}

void ResponseDispatcher::UnregisterSink(SinkToken token) {
  sinks_.erase(token);
}

void ResponseDispatcher::Emit(const Event &event) {
  if (!event.cmd_id.empty()) {
    CacheEntry &entry = cache_[event.cmd_id];
    if (event.type == EventType::kAck) {
      entry.ack.event = event;
      entry.ack.valid = true;
    } else if (event.type == EventType::kDone) {
      entry.done.event = event;
      entry.done.valid = true;
    }
  }

  for (auto &pair : sinks_) {
    pair.second(event);
  }
}

void ResponseDispatcher::Clear() {
  cache_.clear();
}

bool ResponseDispatcher::Replay(const std::string &cmd_id,
                                const std::function<void(const Event &)> &cb) const {
  if (!cb || cmd_id.empty()) {
    return false;
  }
  auto it = cache_.find(cmd_id);
  if (it == cache_.end()) {
    return false;
  }
  bool emitted = false;
  if (it->second.ack.valid) {
    cb(it->second.ack.event);
    emitted = true;
  }
  if (it->second.done.valid) {
    cb(it->second.done.event);
    emitted = true;
  }
  return emitted;
}

} // namespace response
} // namespace transport
