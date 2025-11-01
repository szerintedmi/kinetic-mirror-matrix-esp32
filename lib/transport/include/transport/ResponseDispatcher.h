#pragma once

#include <cstdint>
#include <functional>
#include <deque>
#include <unordered_map>

#include "transport/ResponseModel.h"
#include "transport/CommandSchema.h"

namespace transport {
namespace response {

class ResponseDispatcher {
public:
  using SinkToken = std::uint32_t;
  using SinkCallback = std::function<void(const Event &)>;

  static ResponseDispatcher &Instance();

  SinkToken RegisterSink(SinkCallback cb);
  void UnregisterSink(SinkToken token);
  void Emit(const Event &event);
  void Clear();
  bool Replay(const std::string &cmd_id, const std::function<void(const Event &)> &cb) const;
  std::size_t CachedCommandCount() const;

private:
  ResponseDispatcher() = default;

  struct CachedEvent {
    command::ResponseLine line;
    std::string action;
    EventType type = EventType::kInfo;
    bool valid = false;
  };
  struct CacheEntry {
    CachedEvent ack;
    CachedEvent done;
  };

  SinkToken next_token_ = 1;
  std::unordered_map<SinkToken, SinkCallback> sinks_;
  std::unordered_map<std::string, CacheEntry> cache_;
  std::deque<std::string> order_;
};

} // namespace response
} // namespace transport
