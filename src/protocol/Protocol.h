#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include "MotorBackend.h"
#include "StubBackend.h"

class Protocol {
public:
  Protocol();
  // Process a single command line (without trailing newline). Returns response string (may contain newlines).
  std::string processLine(const std::string& line, uint32_t now_ms);
  void tick(uint32_t now_ms) { backend_.tick(now_ms); }

private:
  StubBackend backend_;
  std::string handleHELP();
  std::string handleSTATUS();
  std::string handleWAKE(const std::string& args);
  std::string handleSLEEP(const std::string& args);
  std::string handleMOVE(const std::string& args, uint32_t now_ms);
  std::string handleHOME(const std::string& args, uint32_t now_ms);

  bool parseIdMask(const std::string& token, uint32_t& mask);
  static bool parseInt(const std::string& s, long& out);
  static bool parseInt(const std::string& s, int& out);
};

