#pragma once
#include <stdint.h>
#include <memory>
#include <string>
#include "MotorControl/MotorController.h"

class MotorCommandProcessor {
public:
  MotorCommandProcessor();
  std::string processLine(const std::string& line, uint32_t now_ms);
  void tick(uint32_t now_ms) { controller_->tick(now_ms); }

private:
  std::unique_ptr<MotorController> controller_;
  bool thermal_limits_enabled_ = true; // Global flag, default ON
  std::string handleHELP();
  std::string handleSTATUS();
  std::string handleWAKE(const std::string& args);
  std::string handleSLEEP(const std::string& args);
  std::string handleMOVE(const std::string& args, uint32_t now_ms);
  std::string handleHOME(const std::string& args, uint32_t now_ms);
  std::string handleGET(const std::string& args);
  std::string handleSET(const std::string& args);

  bool parseIdMask(const std::string& token, uint32_t& mask);
  static bool parseInt(const std::string& s, long& out);
  static bool parseInt(const std::string& s, int& out);
};
