#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "MotorControl/MotorController.h"

namespace transport {
namespace response {

class CompletionTracker {
public:
  static CompletionTracker &Instance();

  void RegisterOperation(const std::string &cmd_id,
                         const std::string &action,
                         uint32_t mask,
                         MotorController &controller);
  void Tick(uint32_t now_ms);
  void Clear();
  void RemoveController(MotorController *controller);

private:
  CompletionTracker() = default;

  struct Pending {
    std::string cmd_id;
    std::string action;
    uint32_t mask = 0;
    MotorController *controller = nullptr;
    bool active = false;
  };

  std::vector<Pending> pending_;
};

} // namespace response
} // namespace transport
