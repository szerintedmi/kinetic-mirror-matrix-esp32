#include "transport/ResponseDispatcher.h"

namespace transport {
namespace response {

namespace {

constexpr std::size_t kMaxCachedResponses = 0;

}

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
  if (!event.cmd_id.empty() && kMaxCachedResponses > 0) {
    auto emplace_result = cache_.emplace(event.cmd_id, CacheEntry{});
    if (emplace_result.second) {
      order_.push_back(event.cmd_id);
      while (order_.size() > kMaxCachedResponses) {
        const std::string &oldest = order_.front();
        cache_.erase(oldest);
        order_.pop_front();
      }
    }
    CacheEntry &entry = emplace_result.first->second;
    if (event.type == EventType::kAck) {
      entry.ack.line = EventToLine(event);
      entry.ack.action = event.action;
      entry.ack.type = event.type;
      entry.ack.valid = true;
    } else if (event.type == EventType::kDone) {
      entry.done.line = EventToLine(event);
      entry.done.action = event.action;
      entry.done.type = event.type;
      entry.done.valid = true;
    }
  }

  for (auto &pair : sinks_) {
    pair.second(event);
  }
}

void ResponseDispatcher::Clear() {
  cache_.clear();
  order_.clear();
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
    Event evt = BuildEvent(it->second.ack.line, it->second.ack.action);
    evt.type = it->second.ack.type;
    cb(evt);
    emitted = true;
  }
  if (it->second.done.valid) {
    Event evt = BuildEvent(it->second.done.line, it->second.done.action);
    evt.type = it->second.done.type;
    cb(evt);
    emitted = true;
  }
  return emitted;
}

std::size_t ResponseDispatcher::CachedCommandCount() const {
  return cache_.size();
}

} // namespace response
} // namespace transport
