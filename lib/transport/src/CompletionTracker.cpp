#include "transport/CompletionTracker.h"

#include <algorithm>

#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"

namespace transport {
namespace response {

CompletionTracker &CompletionTracker::Instance() {
  static CompletionTracker tracker;
  return tracker;
}

void CompletionTracker::RegisterOperation(const std::string &cmd_id,
                                          const std::string &action,
                                          uint32_t mask,
                                          MotorController &controller) {
  if (cmd_id.empty() || mask == 0) {
    return;
  }
  // Remove any existing entry for this cmd_id
  pending_.erase(std::remove_if(pending_.begin(), pending_.end(), [&](const Pending &p) {
                    return p.cmd_id == cmd_id;
                  }),
                 pending_.end());
  Pending pending;
  pending.cmd_id = cmd_id;
  pending.action = action;
  pending.mask = mask;
  pending.controller = &controller;
  pending.active = true;
  pending_.push_back(std::move(pending));
}

void CompletionTracker::Tick(uint32_t /*now_ms*/) {
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (!it->active || it->controller == nullptr) {
      it = pending_.erase(it);
      continue;
    }
    if (it->controller->isAnyMovingForMask(it->mask)) {
      ++it;
      continue;
    }

    int32_t actual_ms = -1;
    size_t count = it->controller->motorCount();
    for (size_t idx = 0; idx < count; ++idx) {
      if (idx >= 32 || !(it->mask & (1u << idx))) {
        continue;
      }
      const MotorState &state = it->controller->state(idx);
      if (!state.last_op_ongoing) {
        actual_ms = std::max(actual_ms, static_cast<int32_t>(state.last_op_last_ms));
      }
    }
    if (actual_ms < 0) {
      ++it;
      continue;
    }

    Event evt;
    evt.type = EventType::kDone;
    evt.cmd_id = it->cmd_id;
    evt.action = it->action;
    evt.attributes["status"] = "done";
    if (actual_ms >= 0) {
      evt.attributes["actual_ms"] = std::to_string(actual_ms);
    }
    ResponseDispatcher::Instance().Emit(evt);

    it = pending_.erase(it);
  }
}

void CompletionTracker::Clear() {
  pending_.clear();
}

} // namespace response
} // namespace transport
