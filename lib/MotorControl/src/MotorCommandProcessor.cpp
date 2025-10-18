#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/MotionKinematics.h"
#include "StubMotorController.h"
#if defined(USE_HARDWARE_BACKEND) && !defined(UNIT_TEST)
#include "MotorControl/HardwareMotorController.h"
#endif
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

constexpr long kMinPos = MotorControlConstants::MIN_POS_STEPS;
constexpr long kMaxPos = MotorControlConstants::MAX_POS_STEPS;
constexpr int kDefaultSpeed = MotorControlConstants::DEFAULT_SPEED_SPS;
constexpr int kDefaultAccel = MotorControlConstants::DEFAULT_ACCEL_SPS2;
constexpr long kDefaultOvershoot = MotorControlConstants::DEFAULT_OVERSHOOT;
constexpr long kDefaultBackoff = MotorControlConstants::DEFAULT_BACKOFF;

static std::string trim(const std::string &s)
{
  size_t a = 0;
  while (a < s.size() && isspace((unsigned char)s[a]))
    ++a;
  size_t b = s.size();
  while (b > a && isspace((unsigned char)s[b - 1]))
    --b;
  return s.substr(a, b - a);
}

static std::vector<std::string> split(const std::string &s, char delim)
{
  std::vector<std::string> out;
  std::string cur;
  std::stringstream ss(s);
  while (std::getline(ss, cur, delim))
    out.push_back(cur);
  return out;
}

MotorCommandProcessor::MotorCommandProcessor()
#if defined(USE_HARDWARE_BACKEND) && !defined(UNIT_TEST)
    : controller_(new HardwareMotorController())
#else
    : controller_(new StubMotorController(8))
#endif
{
  controller_->setThermalLimitsEnabled(thermal_limits_enabled_);
}

bool MotorCommandProcessor::parseInt(const std::string &s, long &out)
{
  if (s.empty())
    return false;
  char *end = nullptr;
  long v = strtol(s.c_str(), &end, 10);
  if (*end != '\0')
    return false;
  out = v;
  return true;
}

bool MotorCommandProcessor::parseInt(const std::string &s, int &out)
{
  long tmp;
  if (!parseInt(s, tmp))
    return false;
  out = (int)tmp;
  return true;
}

bool MotorCommandProcessor::parseIdMask(const std::string &token, uint32_t &mask)
{
  std::string t = token;
  std::transform(t.begin(), t.end(), t.begin(), ::toupper);
  if (t == "ALL")
  {
    mask = 0xFF;
    return true;
  }
  long id;
  if (!parseInt(token, id))
    return false;
  if (id < 0 || id > 7)
    return false;
  mask = (1u << (uint8_t)id);
  return true;
}

std::string MotorCommandProcessor::handleHELP()
{
  std::ostringstream os;
  os << "HELP\n";
  os << "MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]\n";
  os << "HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]\n";
  os << "STATUS\n";
  os << "GET LAST_OP_TIMING[:<id|ALL>]\n";
  // Thermal runtime limiting controls
  os << "GET THERMAL_RUNTIME_LIMITING\n";
  os << "SET THERMAL_RUNTIME_LIMITING=OFF|ON\n";
  os << "WAKE:<id|ALL>\n";
  os << "SLEEP:<id|ALL>\n";
  os << "Shortcuts: M=MOVE, H=HOME, ST=STATUS";
  return os.str();
}

static std::string to_upper_copy(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), ::toupper);
  return out;
}

static std::string trim_copy(const std::string &s) {
  return trim(s);
}

std::string MotorCommandProcessor::handleGET(const std::string &args)
{
  // Support both colon and space-separated payloads; args already contains payload
  std::string key = to_upper_copy(trim_copy(args));
  if (key == "THERMAL_RUNTIME_LIMITING") {
    std::ostringstream os;
    os << "CTRL:OK THERMAL_RUNTIME_LIMITING=" << (thermal_limits_enabled_ ? "ON" : "OFF")
       << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
    return os.str();
  }
  // LAST_OP_TIMING[:<id|ALL>] returns current/last timing summary
  if (key.rfind("LAST_OP_TIMING", 0) == 0) {
    std::string rest;
    size_t p = key.find(':');
    if (p != std::string::npos) rest = trim_copy(key.substr(p + 1));
    // If no param or ALL → list all motors (multi-line)
    if (rest.empty() || rest == "ALL") {
      std::ostringstream os;
      os << "LAST_OP_TIMING\n";
      for (uint8_t i = 0; i < controller_->motorCount(); ++i) {
        const MotorState &s = controller_->state(i);
        os << "id=" << (int)i
           << " ongoing=" << (s.last_op_ongoing ? 1 : 0)
           << " est_ms=" << s.last_op_est_ms
           << " started_ms=" << s.last_op_started_ms;
        if (!s.last_op_ongoing) os << " actual_ms=" << s.last_op_last_ms;
        os << "\n";
      }
      return os.str();
    }
    // Single-id query: return a single-line summary for that id
    uint32_t mask;
    if (!parseIdMask(rest, mask)) return "CTRL:ERR E02 BAD_ID";
    // Find the first id in mask
    uint8_t id = 0; for (; id < controller_->motorCount(); ++id) if (mask & (1u << id)) break;
    if (id >= controller_->motorCount()) return "CTRL:ERR E02 BAD_ID";
    const MotorState &s = controller_->state(id);
    std::ostringstream os;
    os << "CTRL:OK LAST_OP_TIMING ongoing=" << (s.last_op_ongoing ? 1 : 0)
       << " id=" << (int)id
       << " est_ms=" << s.last_op_est_ms
       << " started_ms=" << s.last_op_started_ms;
    if (!s.last_op_ongoing) os << " actual_ms=" << s.last_op_last_ms;
    return os.str();
  }
  return "CTRL:ERR E03 BAD_PARAM";
}

std::string MotorCommandProcessor::handleSET(const std::string &args)
{
  // Expect: THERMAL_RUNTIME_LIMITING=OFF|ON (allow optional spaces around '=')
  std::string payload = trim_copy(args);
  // Normalize for comparison without destroying original spacing
  std::string up = to_upper_copy(payload);
  // Find '=' in original (position is same in upper)
  size_t eq = up.find('=');
  if (eq == std::string::npos) return "CTRL:ERR E03 BAD_PARAM";
  std::string key = trim_copy(up.substr(0, eq));
  std::string val = trim_copy(up.substr(eq + 1));
  if (key != "THERMAL_RUNTIME_LIMITING") return "CTRL:ERR E03 BAD_PARAM";
  if (val == "ON") {
    thermal_limits_enabled_ = true;
    controller_->setThermalLimitsEnabled(thermal_limits_enabled_);
    return "CTRL:OK";
  } else if (val == "OFF") {
    thermal_limits_enabled_ = false;
    controller_->setThermalLimitsEnabled(thermal_limits_enabled_);
    return "CTRL:OK";
  }
  return "CTRL:ERR E03 BAD_PARAM";
}

std::string MotorCommandProcessor::handleSTATUS()
{
  std::ostringstream os;
  for (size_t i = 0; i < controller_->motorCount(); ++i)
  {
    const MotorState &s = controller_->state(i);
    // budget_s: show the true budget balance (can be negative), formatted to 1 decimal
    int32_t budget_t = s.budget_tenths;
    bool budget_neg = (budget_t < 0);
    int32_t budget_abs = budget_neg ? -budget_t : budget_t;
    // Compute actual cooldown time in tenths (ceil), accounting for negative budget
    // missing_t can exceed BUDGET_TENTHS_MAX if budget_tenths is negative
    int32_t missing_t = MotorControlConstants::BUDGET_TENTHS_MAX - s.budget_tenths;
    if (missing_t < 0) missing_t = 0;
    int32_t ttfc_tenths = (missing_t <= 0) ? 0
                         : (int32_t)(( (int64_t)missing_t * 10 + MotorControlConstants::REFILL_TENTHS_PER_SEC - 1 ) / MotorControlConstants::REFILL_TENTHS_PER_SEC);
    // Clamp displayed cooldown to MAX_COOL_DOWN_TIME_S
    const int32_t kTtfcMaxTenths = MotorControlConstants::MAX_COOL_DOWN_TIME_S * 10;
    if (ttfc_tenths > kTtfcMaxTenths) ttfc_tenths = kTtfcMaxTenths;

    os << "id=" << (int)s.id
       << " pos=" << s.position
       << " moving=" << (s.moving ? 1 : 0)
       << " awake=" << (s.awake ? 1 : 0)
       << " homed=" << (s.homed ? 1 : 0)
       << " steps_since_home=" << s.steps_since_home
       << " budget_s=" << (budget_neg ? "-" : "") << (budget_abs / 10) << "." << (budget_abs % 10)
       << " ttfc_s=" << (ttfc_tenths / 10) << "." << (ttfc_tenths % 10)
       << " speed=" << s.speed
       << " accel=" << s.accel;
    if (i + 1 < controller_->motorCount())
      os << "\n";
  }
  return os.str();
}

std::string MotorCommandProcessor::handleWAKE(const std::string &args)
{
  uint32_t mask;
  if (!parseIdMask(args, mask))
    return "CTRL:ERR E02 BAD_ID";
  // Thermal WAKE guard: reject when no budget if enabled; warn when disabled
  // Determine first violating id deterministically
  for (uint8_t id = 0; id < controller_->motorCount(); ++id) {
    if ((mask & (1u << id)) == 0) continue;
    const MotorState &s = controller_->state(id);
    int avail_s = (s.budget_tenths >= 0) ? (s.budget_tenths / 10) : 0;
    if (avail_s <= 0) {
      if (thermal_limits_enabled_) {
        return std::string("CTRL:ERR E12 THERMAL_NO_BUDGET_WAKE");
      } else {
        controller_->wakeMask(mask);
        return std::string("CTRL:WARN THERMAL_NO_BUDGET_WAKE\nCTRL:OK");
      }
    }
  }
  controller_->wakeMask(mask);
  return "CTRL:OK";
}

std::string MotorCommandProcessor::handleSLEEP(const std::string &args)
{
  uint32_t mask;
  if (!parseIdMask(args, mask))
    return "CTRL:ERR E02 BAD_ID";
  if (!controller_->sleepMask(mask))
    return "CTRL:ERR E04 BUSY";
  return "CTRL:OK";
}

std::string MotorCommandProcessor::handleMOVE(const std::string &args, uint32_t now_ms)
{
  auto parts = split(args, ',');
  if (parts.size() < 2)
    return "CTRL:ERR E03 BAD_PARAM";
  uint32_t mask;
  if (!parseIdMask(parts[0], mask))
    return "CTRL:ERR E02 BAD_ID";
  long target;
  if (!parseInt(parts[1], target))
    return "CTRL:ERR E03 BAD_PARAM";
  if (target < kMinPos || target > kMaxPos)
    return "CTRL:ERR E07 POS_OUT_OF_RANGE";
  int speed = kDefaultSpeed;
  int accel = kDefaultAccel;
  if (parts.size() >= 3 && !parts[2].empty())
  {
    if (!parseInt(parts[2], speed))
      return "CTRL:ERR E03 BAD_PARAM";
  }
  if (parts.size() >= 4 && !parts[3].empty())
  {
    if (!parseInt(parts[3], accel))
      return "CTRL:ERR E03 BAD_PARAM";
  }
  // Thermal pre-flight: tick to current time and check budgets
  controller_->tick(now_ms);
  // Compute per-motor required runtime and compare to limits
  // Reject on first violation (deterministic order: id ascending)
  uint32_t tmp = mask;
  uint32_t max_req_ms = 0;
  for (uint8_t id = 0; id < controller_->motorCount(); ++id)
  {
    if ((tmp & (1u << id)) == 0) continue;
    const MotorState &s = controller_->state(id);
    long dist = std::labs(target - s.position);
    uint32_t req_ms = MotionKinematics::estimateMoveTimeMs(dist, speed, accel);
    if (req_ms > max_req_ms) max_req_ms = req_ms;
    int req_s = (int)((req_ms + 999) / 1000); // ceil to seconds
    // E10: required exceeds max budget
    if (req_s > (int)MotorControlConstants::MAX_RUNNING_TIME_S)
    {
      if (thermal_limits_enabled_)
      {
        std::ostringstream eo; eo << "CTRL:ERR E10 THERMAL_REQ_GT_MAX id=" << (int)id
                                   << " req_ms=" << req_ms
                                   << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
        return eo.str();
      }
      else
      {
        std::ostringstream wo; wo << "CTRL:WARN THERMAL_REQ_GT_MAX id=" << (int)id
                                   << " req_ms=" << req_ms
                                   << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
        // Proceed after warning; perform controller action and return combined lines with estimate
        if (!controller_->moveAbsMask(mask, target, speed, accel, now_ms))
          return "CTRL:ERR E04 BUSY";
        std::ostringstream out; out << wo.str() << "\n"
                                    << "CTRL:OK est_ms=" << max_req_ms;
        return out.str();
      }
    }
    // E11: current budget insufficient
    int32_t bt = s.budget_tenths;
    int avail_s = (bt >= 0) ? (bt / 10) : 0;
    // Compute ttfc as in STATUS for payload context
    int32_t missing_t = MotorControlConstants::BUDGET_TENTHS_MAX - s.budget_tenths;
    if (missing_t < 0) missing_t = 0;
    int32_t ttfc_tenths = (missing_t <= 0) ? 0
                         : (int32_t)(((int64_t)missing_t * 10 + MotorControlConstants::REFILL_TENTHS_PER_SEC - 1) / MotorControlConstants::REFILL_TENTHS_PER_SEC);
    const int32_t kTtfcMaxTenths = MotorControlConstants::MAX_COOL_DOWN_TIME_S * 10;
    if (ttfc_tenths > kTtfcMaxTenths) ttfc_tenths = kTtfcMaxTenths;
    int ttfc_s = ttfc_tenths / 10;
    if (req_s > avail_s)
    {
      if (thermal_limits_enabled_)
      {
        std::ostringstream eo; eo << "CTRL:ERR E11 THERMAL_NO_BUDGET id=" << (int)id
                                   << " req_ms=" << req_ms
                                   << " budget_s=" << avail_s
                                   << " ttfc_s=" << ttfc_s;
        return eo.str();
      }
      else
      {
        std::ostringstream wo; wo << "CTRL:WARN THERMAL_NO_BUDGET id=" << (int)id
                                   << " req_ms=" << req_ms
                                   << " budget_s=" << avail_s
                                   << " ttfc_s=" << ttfc_s;
        if (!controller_->moveAbsMask(mask, target, speed, accel, now_ms))
          return "CTRL:ERR E04 BUSY";
        std::ostringstream out; out << wo.str() << "\n"
                                    << "CTRL:OK est_ms=" << max_req_ms;
        return out.str();
      }
    }
  }
  if (!controller_->moveAbsMask(mask, target, speed, accel, now_ms))
    return "CTRL:ERR E04 BUSY";
  {
    std::ostringstream ok;
    ok << "CTRL:OK est_ms=" << max_req_ms;
    return ok.str();
  }
}

std::string MotorCommandProcessor::handleHOME(const std::string &args, uint32_t now_ms)
{
  auto parts = split(args, ',');
  if (parts.size() < 1)
    return "CTRL:ERR E03 BAD_PARAM";
  uint32_t mask;
  if (!parseIdMask(parts[0], mask))
    return "CTRL:ERR E02 BAD_ID";
  long overshoot = kDefaultOvershoot, backoff = kDefaultBackoff, full_range = 0;
  int speed = kDefaultSpeed;
  int accel = kDefaultAccel;
  if (parts.size() >= 2 && !parts[1].empty())
  {
    if (!parseInt(parts[1], overshoot))
      return "CTRL:ERR E03 BAD_PARAM";
  }
  if (parts.size() >= 3 && !parts[2].empty())
  {
    if (!parseInt(parts[2], backoff))
      return "CTRL:ERR E03 BAD_PARAM";
  }
  if (parts.size() >= 4 && !parts[3].empty())
  {
    if (!parseInt(parts[3], speed))
      return "CTRL:ERR E03 BAD_PARAM";
  }
  if (parts.size() >= 5 && !parts[4].empty())
  {
    if (!parseInt(parts[4], accel))
      return "CTRL:ERR E03 BAD_PARAM";
  }
  if (parts.size() >= 6 && !parts[5].empty())
  {
    if (!parseInt(parts[5], full_range))
      return "CTRL:ERR E03 BAD_PARAM";
  }
  // Thermal pre-flight: tick and check required time vs limits
  controller_->tick(now_ms);
  if (full_range <= 0) {
    full_range = (MotorControlConstants::MAX_POS_STEPS - MotorControlConstants::MIN_POS_STEPS);
  }
  uint32_t req_ms_total = MotionKinematics::estimateHomeTimeMsWithFullRange(overshoot, backoff, full_range, speed, accel);
  int req_s = (int)((req_ms_total + 999) / 1000);
  if (req_s > (int)MotorControlConstants::MAX_RUNNING_TIME_S)
  {
    // Use first addressed motor id for payload clarity
    uint8_t first_id = 0; for (; first_id < controller_->motorCount(); ++first_id) if (mask & (1u << first_id)) break;
    if (thermal_limits_enabled_)
    {
      std::ostringstream eo; eo << "CTRL:ERR E10 THERMAL_REQ_GT_MAX id=" << (int)first_id
                                 << " req_ms=" << req_ms_total
                                 << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
      return eo.str();
    }
    else
    {
      std::ostringstream wo; wo << "CTRL:WARN THERMAL_REQ_GT_MAX id=" << (int)first_id
                                 << " req_ms=" << req_ms_total
                                 << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
      if (!controller_->homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms))
        return "CTRL:ERR E04 BUSY";
      std::ostringstream out; out << wo.str() << "\n"
                                  << "CTRL:OK est_ms=" << req_ms_total;
      return out.str();
    }
  }
  // E11: insufficient current budget per motor
  for (uint8_t id = 0; id < controller_->motorCount(); ++id)
  {
    if ((mask & (1u << id)) == 0) continue;
    const MotorState &s = controller_->state(id);
    int avail_s = (s.budget_tenths >= 0) ? (s.budget_tenths / 10) : 0;
    int32_t missing_t = MotorControlConstants::BUDGET_TENTHS_MAX - s.budget_tenths;
    if (missing_t < 0) missing_t = 0;
    int32_t ttfc_tenths = (missing_t <= 0) ? 0
                         : (int32_t)(((int64_t)missing_t * 10 + MotorControlConstants::REFILL_TENTHS_PER_SEC - 1) / MotorControlConstants::REFILL_TENTHS_PER_SEC);
    const int32_t kTtfcMaxTenths = MotorControlConstants::MAX_COOL_DOWN_TIME_S * 10;
    if (ttfc_tenths > kTtfcMaxTenths) ttfc_tenths = kTtfcMaxTenths;
    int ttfc_s = ttfc_tenths / 10;
    if (req_s > avail_s)
    {
      if (thermal_limits_enabled_)
      {
        std::ostringstream eo; eo << "CTRL:ERR E11 THERMAL_NO_BUDGET id=" << (int)id
                                   << " req_ms=" << req_ms_total
                                   << " budget_s=" << avail_s
                                   << " ttfc_s=" << ttfc_s;
        return eo.str();
      }
      else
      {
        std::ostringstream wo; wo << "CTRL:WARN THERMAL_NO_BUDGET id=" << (int)id
                                   << " req_ms=" << req_ms_total
                                   << " budget_s=" << avail_s
                                   << " ttfc_s=" << ttfc_s;
        if (!controller_->homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms))
          return "CTRL:ERR E04 BUSY";
        std::ostringstream out; out << wo.str() << "\n"
                                    << "CTRL:OK est_ms=" << req_ms_total;
        return out.str();
      }
    }
  }
  if (!controller_->homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms))
    return "CTRL:ERR E04 BUSY";
  {
    std::ostringstream ok;
    ok << "CTRL:OK est_ms=" << req_ms_total;
    return ok.str();
  }
}

std::string MotorCommandProcessor::processLine(const std::string &line, uint32_t now_ms)
{
  std::string s = trim(line);
  if (s.empty())
    return "";
  // Split verb/args: prefer space separator if it appears before ':'
  size_t sp = s.find(' ');
  size_t cp = s.find(':');
  std::string verb = s;
  std::string args;
  if (sp != std::string::npos && (cp == std::string::npos || sp < cp)) {
    verb = s.substr(0, sp);
    args = s.substr(sp + 1);
  } else if (cp != std::string::npos) {
    verb = s.substr(0, cp);
    args = s.substr(cp + 1);
  }
  std::transform(verb.begin(), verb.end(), verb.begin(), ::toupper);
  if (verb == "HELP")
    return handleHELP();
  if (verb == "STATUS" || verb == "ST")
  {
    // Advance controller state to 'now' to render up-to-date diagnostics
    controller_->tick(now_ms);
    return handleSTATUS();
  }
  if (verb == "GET")
  {
    controller_->tick(now_ms);
    return handleGET(args);
  }
  if (verb == "SET")
  {
    controller_->tick(now_ms);
    return handleSET(args);
  }
  if (verb == "WAKE")
  {
    controller_->tick(now_ms);
    return handleWAKE(args);
  }
  if (verb == "SLEEP")
  {
    controller_->tick(now_ms);
    return handleSLEEP(args);
  }
  if (verb == "MOVE" || verb == "M")
    return handleMOVE(args, now_ms);
  if (verb == "HOME" || verb == "H")
    return handleHOME(args, now_ms);
  return "CTRL:ERR E01 BAD_CMD";
}
