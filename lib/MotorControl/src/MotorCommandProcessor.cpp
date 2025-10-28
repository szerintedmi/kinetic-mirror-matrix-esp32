#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/BuildConfig.h"
#include "StubMotorController.h"
#if !defined(USE_STUB_BACKEND) && !defined(UNIT_TEST)
#include "MotorControl/HardwareMotorController.h"
#endif
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <chrono>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <thread>
#endif
#include "net_onboarding/NetOnboarding.h"
#include "net_onboarding/NetSingleton.h"
#include "net_onboarding/Cid.h"
#include "net_onboarding/SerialImmediate.h"

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

// Parse CSV with optional quoted fields. Supports escaping of \" and \\
// Returns empty vector on malformed input (e.g., unclosed quote)
static std::vector<std::string> parseCsvQuoted(const std::string &s)
{
  std::vector<std::string> out;
  std::string cur;
  bool in_quotes = false;
  bool escape = false;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (in_quotes) {
      if (escape) {
        if (c == '"' || c == '\\') cur.push_back(c);
        else {
          // Unknown escape: keep char as-is
          cur.push_back(c);
        }
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        in_quotes = false;
      } else {
        cur.push_back(c);
      }
    } else {
      if (c == ',') {
        // trim whitespace for unquoted tokens
        out.push_back(trim(cur));
        cur.clear();
      } else if (c == '"') {
        // entering quotes: keep existing trimmed prefix if empty
        if (!trim(cur).empty()) {
          // Unexpected quote inside unquoted token -> treat as literal
          cur.push_back(c);
        } else {
          cur.clear();
          in_quotes = true;
        }
      } else {
        cur.push_back(c);
      }
    }
  }
  if (in_quotes) {
    // unclosed quote -> invalid
    return std::vector<std::string>();
  }
  out.push_back(trim(cur));
  return out;
}

// Quote a string for key=value outputs: wraps in double quotes and escapes
// embedded '"' and '\\'. Example: hello -> "hello", he"l\\o -> "he\"l\\o"
static std::string quote_string(const std::string &s)
{
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (char c : s) {
    if (c == '"' || c == '\\') out.push_back('\\');
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

MotorCommandProcessor::MotorCommandProcessor()
#if !defined(USE_STUB_BACKEND) && !defined(UNIT_TEST)
    : controller_(new HardwareMotorController())
#else
    : controller_(new StubMotorController(8))
#endif
{
  controller_->setThermalLimitsEnabled(thermal_limits_enabled_);
  default_speed_sps_ = MotorControlConstants::DEFAULT_SPEED_SPS;
  default_accel_sps2_ = MotorControlConstants::DEFAULT_ACCEL_SPS2;
  default_decel_sps2_ = 0; // shared-STEP default: no ramp-down unless set by user
  controller_->setDeceleration(default_decel_sps2_);
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
  // Always include the minimal canonical forms so help remains stable across builds
  os << "MOVE:<id|ALL>,<abs_steps>\n";
  os << "HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]\n";
#ifdef ARDUINO
  os << "NET:RESET\n";
  os << "NET:STATUS\n";
  os << "NET:SET,\"<ssid>\",\"<pass>\" (quote to allow commas/spaces; escape \\\" and \\\\)\n";
  os << "NET:LIST (scan nearby SSIDs)\n";
#else
  // NET verbs available in both native and Arduino builds; HELP remains consistent
  os << "NET:RESET\n";
  os << "NET:STATUS\n";
  os << "NET:SET,\"<ssid>\",\"<pass>\" (quote to allow commas/spaces; escape \\\" and \\\\)\n";
  os << "NET:LIST (scan nearby SSIDs)\n";
#endif
#if !(USE_SHARED_STEP)
  // Dedicated-step builds: also advertise the extended HOME/MOVE forms
  os << "MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]\n";
  os << "HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]\n";
#endif
  os << "STATUS\n";
  os << "GET\n";
  os << "GET ALL\n";
  os << "GET LAST_OP_TIMING[:<id|ALL>]\n";
  os << "GET SPEED\n";
  os << "GET ACCEL\n";
  os << "GET DECEL\n";
  // Thermal runtime limiting controls
  os << "GET THERMAL_LIMITING\n";
  os << "SET THERMAL_LIMITING=OFF|ON\n";
  os << "SET SPEED=<steps_per_second>\n";
  os << "SET ACCEL=<steps_per_second^2>\n";
  os << "SET DECEL=<steps_per_second^2>\n";
  os << "WAKE:<id|ALL>\n";
  os << "SLEEP:<id|ALL>\n";
  os << "Shortcuts: M=MOVE, H=HOME, ST=STATUS\n";
  os << "Multicommand: <cmd1>;<cmd2> note: no cmd queuing; only distinct motors allowed";
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
  uint32_t cid = net_onboarding::NextCid();
  // Support both colon and space-separated payloads; args already contains payload
  std::string key = to_upper_copy(trim_copy(args));
  // No-arg or ALL -> return all core settings in a single OK line
  if (key.empty() || key == "ALL") {
    std::ostringstream os;
    os << "CTRL:ACK CID=" << cid << ' '
       << "SPEED=" << default_speed_sps_ << ' '
       << "ACCEL=" << default_accel_sps2_ << ' '
       << "DECEL=" << default_decel_sps2_ << ' '
       << "THERMAL_LIMITING=" << (thermal_limits_enabled_ ? "ON" : "OFF")
       << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
    return os.str();
  }
  if (key == "SPEED") {
    std::ostringstream os; os << "CTRL:ACK CID=" << cid << " SPEED=" << default_speed_sps_;
    return os.str();
  }
  if (key == "ACCEL") {
    std::ostringstream os; os << "CTRL:ACK CID=" << cid << " ACCEL=" << default_accel_sps2_;
    return os.str();
  }
  if (key == "DECEL") {
    std::ostringstream os; os << "CTRL:ACK CID=" << cid << " DECEL=" << default_decel_sps2_;
    return os.str();
  }
  if (key == "THERMAL_LIMITING") {
    std::ostringstream os;
    os << "CTRL:ACK CID=" << cid
       << " THERMAL_LIMITING=" << (thermal_limits_enabled_ ? "ON" : "OFF")
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
      uint32_t list_cid = net_onboarding::NextCid();
      // ACK first for consistency
      os << "CTRL:ACK CID=" << list_cid << "\n";
      os << "LAST_OP_TIMING\n";
      for (uint8_t i = 0; i < controller_->motorCount(); ++i) {
        const MotorState &s = controller_->state(i);
        os << "id=" << (int)i
           << " ongoing=" << (s.last_op_ongoing ? 1 : 0)
           << " est_ms=" << s.last_op_est_ms
           << " started_ms=" << s.last_op_started_ms;
        if (!s.last_op_ongoing) os << " actual_ms=" << s.last_op_last_ms;
        if (i + 1 < controller_->motorCount()) os << "\n";
      }
      return os.str();
    }
    // Single-id query: return a single-line summary for that id
    uint32_t mask;
    if (!parseIdMask(rest, mask)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID"; return eo.str(); }
    // Find the first id in mask
    uint8_t id = 0; for (; id < controller_->motorCount(); ++id) if (mask & (1u << id)) break;
    if (id >= controller_->motorCount()) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID"; return eo.str(); }
    const MotorState &s = controller_->state(id);
    std::ostringstream os;
    os << "CTRL:ACK CID=" << cid << " LAST_OP_TIMING ongoing=" << (s.last_op_ongoing ? 1 : 0)
       << " id=" << (int)id
       << " est_ms=" << s.last_op_est_ms
       << " started_ms=" << s.last_op_started_ms;
    if (!s.last_op_ongoing) os << " actual_ms=" << s.last_op_last_ms;
    return os.str();
  }
  // ADAPTER_* debug commands removed during cleanup; fall through to BAD_PARAM
  { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
}

std::string MotorCommandProcessor::handleSET(const std::string &args)
{
  uint32_t cid = net_onboarding::NextCid();
  // Expect: THERMAL_LIMITING=OFF|ON (allow optional spaces around '=')
  std::string payload = trim_copy(args);
  // Normalize for comparison without destroying original spacing
  std::string up = to_upper_copy(payload);
  // Find '=' in original (position is same in upper)
  size_t eq = up.find('=');
  if (eq == std::string::npos) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  std::string key = trim_copy(up.substr(0, eq));
  std::string val = trim_copy(up.substr(eq + 1));
  if (key == "THERMAL_LIMITING") {
    if (val == "ON") {
      thermal_limits_enabled_ = true;
      controller_->setThermalLimitsEnabled(thermal_limits_enabled_);
      std::ostringstream os; os << "CTRL:ACK CID=" << cid; return os.str();
    } else if (val == "OFF") {
      thermal_limits_enabled_ = false;
      controller_->setThermalLimitsEnabled(thermal_limits_enabled_);
      std::ostringstream os; os << "CTRL:ACK CID=" << cid; return os.str();
    }
    { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  }
  if (key == "SPEED") {
    long v; if (!parseInt(val, v) || v <= 0) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
    // reject changes while any motor is moving
    for (uint8_t id = 0; id < controller_->motorCount(); ++id) {
      if (controller_->state(id).moving) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
    }
    default_speed_sps_ = (int)v;
    std::ostringstream os; os << "CTRL:ACK CID=" << cid; return os.str();
  }
  if (key == "ACCEL") {
    long v; if (!parseInt(val, v) || v <= 0) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
    for (uint8_t id = 0; id < controller_->motorCount(); ++id) {
      if (controller_->state(id).moving) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
    }
    default_accel_sps2_ = (int)v;
    std::ostringstream os; os << "CTRL:ACK CID=" << cid; return os.str();
  }
  if (key == "DECEL") {
    long v; if (!parseInt(val, v) || v < 0) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
    for (uint8_t id = 0; id < controller_->motorCount(); ++id) {
      if (controller_->state(id).moving) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
    }
    default_decel_sps2_ = (int)v;
    controller_->setDeceleration(default_decel_sps2_);
    std::ostringstream os; os << "CTRL:ACK CID=" << cid; return os.str();
  }
  { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
}

std::string MotorCommandProcessor::handleSTATUS()
{
  std::ostringstream os;
  // Emit ACK first for consistency (host may suppress it via CID)
  uint32_t cid = net_onboarding::NextCid();
  os << "CTRL:ACK CID=" << cid << "\n";
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
       << " accel=" << s.accel
       // Timing summary columns mirroring GET LAST_OP_TIMING
       << " est_ms=" << s.last_op_est_ms
       << " started_ms=" << s.last_op_started_ms;
    if (!s.last_op_ongoing) os << " actual_ms=" << s.last_op_last_ms;
    if (i + 1 < controller_->motorCount())
      os << "\n";
  }
  return os.str();
}

std::string MotorCommandProcessor::handleWAKE(const std::string &args)
{
  uint32_t cid = net_onboarding::NextCid();
  uint32_t mask;
  if (!parseIdMask(args, mask)) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID"; return eo.str();
  }
  // Thermal WAKE guard: reject when no budget if enabled; warn when disabled
  // Determine first violating id deterministically
  for (uint8_t id = 0; id < controller_->motorCount(); ++id) {
    if ((mask & (1u << id)) == 0) continue;
    const MotorState &s = controller_->state(id);
    int avail_s = (s.budget_tenths >= 0) ? (s.budget_tenths / 10) : 0;
    if (avail_s <= 0) {
      if (thermal_limits_enabled_) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E12 THERMAL_NO_BUDGET_WAKE"; return eo.str();
      } else {
        controller_->wakeMask(mask);
        std::ostringstream out; out << "CTRL:WARN CID=" << cid << " THERMAL_NO_BUDGET_WAKE\nCTRL:ACK CID=" << cid; return out.str();
      }
    }
  }
  controller_->wakeMask(mask);
  std::ostringstream os; os << "CTRL:ACK CID=" << cid; return os.str();
}

std::string MotorCommandProcessor::handleSLEEP(const std::string &args)
{
  uint32_t cid = net_onboarding::NextCid();
  uint32_t mask;
  if (!parseIdMask(args, mask)) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID"; return eo.str();
  }
  if (!controller_->sleepMask(mask)) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str();
  }
  std::ostringstream os; os << "CTRL:ACK CID=" << cid; return os.str();
}

// Batch context for multi-command processing
static bool s_in_batch = false;
static bool s_batch_initially_idle = false;

std::string MotorCommandProcessor::handleMOVE(const std::string &args, uint32_t now_ms)
{
  uint32_t cid = net_onboarding::NextCid();
  auto parts = split(args, ',');
  if (parts.size() < 2) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str();
  }
  uint32_t mask;
  if (!parseIdMask(parts[0], mask)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID"; return eo.str(); }
  long target;
  if (!parseInt(parts[1], target)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  if (target < kMinPos || target > kMaxPos) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E07 POS_OUT_OF_RANGE"; return eo.str(); }
  int speed = default_speed_sps_;
  int accel = default_accel_sps2_;
#if (USE_SHARED_STEP)
  // Per-move speed/accel not supported on shared-STEP builds
  if ((parts.size() >= 3 && !parts[2].empty()) || (parts.size() >= 4 && !parts[3].empty())) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
#else
  // Dedicated-step builds: accept optional per-move speed/accel
  if (parts.size() >= 3 && !parts[2].empty()) {
    if (!parseInt(parts[2], speed) || speed <= 0) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  }
  if (parts.size() >= 4 && !parts[3].empty()) {
    if (!parseInt(parts[3], accel) || accel <= 0) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  }
  // Reject extra non-empty tokens beyond the supported arity
  if (parts.size() > 4) {
    for (size_t i = 4; i < parts.size(); ++i) {
      if (!trim(parts[i]).empty()) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
    }
  }
#endif
  // Shared-STEP global busy rule: block new MOVE while any motor is moving,
  // but allow multi-command batches to start multiple MOVEs together when the
  // batch begins idle.
#if (USE_SHARED_STEP)
  if (!(s_in_batch && s_batch_initially_idle)) {
    for (uint8_t id = 0; id < controller_->motorCount(); ++id) {
      if (controller_->state(id).moving) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
    }
  }
#endif
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
    uint32_t req_ms = 0;
#if (USE_SHARED_STEP)
    req_ms = MotionKinematics::estimateMoveTimeMsSharedStep(dist, speed, accel, default_decel_sps2_);
#else
    req_ms = MotionKinematics::estimateMoveTimeMs(dist, speed, accel);
#endif
    if (req_ms > max_req_ms) max_req_ms = req_ms;
    int req_s = (int)((req_ms + 999) / 1000); // ceil to seconds
    // E10: required exceeds max budget
    if (req_s > (int)MotorControlConstants::MAX_RUNNING_TIME_S)
    {
      if (thermal_limits_enabled_)
      {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E10 THERMAL_REQ_GT_MAX id=" << (int)id
                                   << " req_ms=" << req_ms
                                   << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
        return eo.str();
      }
      else
      {
        std::ostringstream wo; wo << "CTRL:WARN CID=" << cid << " THERMAL_REQ_GT_MAX id=" << (int)id
                                   << " req_ms=" << req_ms
                                   << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
        // Proceed after warning; perform controller action and return combined lines with estimate
        if (!controller_->moveAbsMask(mask, target, speed, accel, now_ms)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
        std::ostringstream out; out << wo.str() << "\n"
                                    << "CTRL:ACK CID=" << cid << " est_ms=" << max_req_ms;
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
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E11 THERMAL_NO_BUDGET id=" << (int)id
                                   << " req_ms=" << req_ms
                                   << " budget_s=" << avail_s
                                   << " ttfc_s=" << ttfc_s;
        return eo.str();
      }
      else
      {
        std::ostringstream wo; wo << "CTRL:WARN CID=" << cid << " THERMAL_NO_BUDGET id=" << (int)id
                                   << " req_ms=" << req_ms
                                   << " budget_s=" << avail_s
                                   << " ttfc_s=" << ttfc_s;
        if (!controller_->moveAbsMask(mask, target, speed, accel, now_ms)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
        std::ostringstream out; out << wo.str() << "\n"
                                    << "CTRL:ACK CID=" << cid << " est_ms=" << max_req_ms;
        return out.str();
      }
    }
  }
  if (!controller_->moveAbsMask(mask, target, speed, accel, now_ms)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
  {
    std::ostringstream ok;
    ok << "CTRL:ACK CID=" << cid << " est_ms=" << max_req_ms;
    return ok.str();
  }
}

std::string MotorCommandProcessor::handleHOME(const std::string &args, uint32_t now_ms)
{
  uint32_t cid = net_onboarding::NextCid();
  auto parts = split(args, ',');
  if (parts.size() < 1) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  uint32_t mask;
  if (!parseIdMask(parts[0], mask)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID"; return eo.str(); }
  long overshoot = kDefaultOvershoot, backoff = kDefaultBackoff, full_range = 0;
  int speed = default_speed_sps_;
  int accel = default_accel_sps2_;
  if (parts.size() >= 2 && !parts[1].empty())
  {
    if (!parseInt(parts[1], overshoot)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  }
  if (parts.size() >= 3 && !parts[2].empty())
  {
    if (!parseInt(parts[2], backoff)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  }
  if (parts.size() >= 4 && !parts[3].empty())
  {
    // Backward-compatible handling:
    // - If only 4 tokens: interpret 4th as full_range (legacy form)
    // - If >=5 tokens (dedicated-step only): parse speed/accel at 4th/5th, and optional full_range at 6th
#if (USE_SHARED_STEP)
    if (!parseInt(parts[3], full_range)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
#else
    if (parts.size() == 4) {
      if (!parseInt(parts[3], full_range)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
    }
#endif
  }
#if !(USE_SHARED_STEP)
  // Dedicated-step builds: accept optional per-home speed/accel
  if (parts.size() >= 5 && !parts[4].empty()) {
    if (!parseInt(parts[4], speed) || speed <= 0) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  }
  if (parts.size() >= 6 && !parts[5].empty()) {
    if (!parseInt(parts[5], accel) || accel <= 0) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  }
  // Optional full_range at position 6 when speed/accel are present
  if (parts.size() >= 7 && !parts[6].empty()) {
    if (!parseInt(parts[6], full_range)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
  }
  // Reject extra non-empty tokens beyond supported arity
  if (parts.size() > 7) {
    for (size_t i = 7; i < parts.size(); ++i) {
      if (!trim(parts[i]).empty()) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM"; return eo.str(); }
    }
  }
#endif
  // Thermal pre-flight: tick and check required time vs limits
  controller_->tick(now_ms);
  if (full_range <= 0) {
    full_range = (MotorControlConstants::MAX_POS_STEPS - MotorControlConstants::MIN_POS_STEPS);
  }
  uint32_t req_ms_total = 0;
#if (USE_SHARED_STEP)
  req_ms_total = MotionKinematics::estimateHomeTimeMsWithFullRangeSharedStep(overshoot, backoff, full_range, speed, accel, default_decel_sps2_);
#else
  req_ms_total = MotionKinematics::estimateHomeTimeMsWithFullRange(overshoot, backoff, full_range, speed, accel);
#endif
  int req_s = (int)((req_ms_total + 999) / 1000);
  if (req_s > (int)MotorControlConstants::MAX_RUNNING_TIME_S)
  {
    // Use first addressed motor id for payload clarity
    uint8_t first_id = 0; for (; first_id < controller_->motorCount(); ++first_id) if (mask & (1u << first_id)) break;
    if (thermal_limits_enabled_)
    {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E10 THERMAL_REQ_GT_MAX id=" << (int)first_id
                                 << " req_ms=" << req_ms_total
                                 << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
      return eo.str();
    }
    else
    {
      std::ostringstream wo; wo << "CTRL:WARN CID=" << cid << " THERMAL_REQ_GT_MAX id=" << (int)first_id
                                 << " req_ms=" << req_ms_total
                                 << " max_budget_s=" << (int)MotorControlConstants::MAX_RUNNING_TIME_S;
      if (!controller_->homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms))
        { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
      std::ostringstream out; out << wo.str() << "\n"
                                  << "CTRL:ACK CID=" << cid << " est_ms=" << req_ms_total;
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
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E11 THERMAL_NO_BUDGET id=" << (int)id
                                   << " req_ms=" << req_ms_total
                                   << " budget_s=" << avail_s
                                   << " ttfc_s=" << ttfc_s;
        return eo.str();
      }
      else
      {
        std::ostringstream wo; wo << "CTRL:WARN CID=" << cid << " THERMAL_NO_BUDGET id=" << (int)id
                                   << " req_ms=" << req_ms_total
                                   << " budget_s=" << avail_s
                                   << " ttfc_s=" << ttfc_s;
        if (!controller_->homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms))
          { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
        std::ostringstream out; out << wo.str() << "\n"
                                    << "CTRL:ACK CID=" << cid << " est_ms=" << req_ms_total;
        return out.str();
      }
    }
  }
  if (!controller_->homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms)) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY"; return eo.str(); }
  {
    std::ostringstream ok;
    ok << "CTRL:ACK CID=" << cid << " est_ms=" << req_ms_total;
    return ok.str();
  }
}

std::string MotorCommandProcessor::processLine(const std::string &line, uint32_t now_ms)
{
  std::string s = trim(line);
  if (s.empty())
    return "";
  // Multi-command support: allow ';'-separated commands in one line for simple manual concurrency.
  // Reject the whole line if multiple commands target overlapping motor sets.
  if (s.find(';') != std::string::npos) {
    auto parts = split(s, ';');
    std::vector<std::string> cmds;
    cmds.reserve(parts.size());
    for (auto &p : parts) {
      auto t = trim(p);
      if (!t.empty()) cmds.push_back(t);
    }
    // Pre-scan for overlapping motor targets: consider MOVE/M, HOME/H, WAKE, SLEEP
    auto mask_for = [&](const std::string &cmd) -> uint32_t {
      // Parse verb/args like below
      size_t sp = cmd.find(' ');
      size_t cp = cmd.find(':');
      std::string verb = cmd;
      std::string args;
      if (sp != std::string::npos && (cp == std::string::npos || sp < cp)) {
        verb = cmd.substr(0, sp);
        args = cmd.substr(sp + 1);
      } else if (cp != std::string::npos) {
        verb = cmd.substr(0, cp);
        args = cmd.substr(cp + 1);
      }
      std::transform(verb.begin(), verb.end(), verb.begin(), ::toupper);
      uint32_t mask = 0;
      auto args_trim = trim(args);
      if (verb == "MOVE" || verb == "M") {
        auto ap = split(args_trim, ',');
        if (!ap.empty()) {
          (void)parseIdMask(trim(ap[0]), mask); // ignore parse failure; mask stays 0
        }
      } else if (verb == "HOME" || verb == "H") {
        auto ap = split(args_trim, ',');
        if (!ap.empty()) {
          (void)parseIdMask(trim(ap[0]), mask);
        }
      } else if (verb == "WAKE") {
        (void)parseIdMask(args_trim, mask);
      } else if (verb == "SLEEP") {
        (void)parseIdMask(args_trim, mask);
      }
      return mask;
    };
    uint32_t seen = 0;
    for (const auto &c : cmds) {
      uint32_t m = mask_for(c);
      if (m != 0 && (m & seen)) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << net_onboarding::NextCid() << " E03 BAD_PARAM MULTI_CMD_CONFLICT"; return eo.str();
      }
      seen |= m;
    }
    // Reject early if any unknown verb exists in the batch
    auto is_known_verb = [&](const std::string &cmd) -> bool {
      size_t sp = cmd.find(' ');
      size_t cp = cmd.find(':');
      std::string verb = cmd;
      if (sp != std::string::npos && (cp == std::string::npos || sp < cp)) verb = cmd.substr(0, sp);
      else if (cp != std::string::npos) verb = cmd.substr(0, cp);
      std::transform(verb.begin(), verb.end(), verb.begin(), ::toupper);
      return (verb == "HELP" || verb == "STATUS" || verb == "ST" || verb == "GET" || verb == "SET" ||
              verb == "NET" || verb == "WAKE" || verb == "SLEEP" || verb == "MOVE" || verb == "M" ||
              verb == "HOME" || verb == "H");
    };
    for (const auto &c : cmds) {
      if (!is_known_verb(c)) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << net_onboarding::NextCid() << " E01 BAD_CMD"; return eo.str();
      }
    }
    // Execute sequentially and join responses
    bool initially_idle = true;
    for (uint8_t i = 0; i < controller_->motorCount(); ++i) {
      if (controller_->state(i).moving) { initially_idle = false; break; }
    }
    bool prev_batch = s_in_batch;
    bool prev_idle = s_batch_initially_idle;
    s_in_batch = true;
    s_batch_initially_idle = initially_idle;
    std::string combined;
    // Aggregate MOVE/HOME OK est_ms from sub-commands into a single final line
    // Keep WARN/ERR lines from sub-commands. Drop individual OK est_ms lines.
    uint32_t agg_est_ms = 0;
    bool saw_est = false;
    bool saw_err = false;
    std::string first_err;
    for (size_t i = 0; i < cmds.size(); ++i) {
      auto r = processLine(cmds[i], now_ms);
      if (!r.empty()) {
        // Split into lines and filter/aggregate
        size_t start = 0;
        while (start <= r.size()) {
          size_t pos = r.find('\n', start);
          std::string ln = (pos == std::string::npos) ? r.substr(start) : r.substr(start, pos - start);
          if (!ln.empty()) {
            if (ln.rfind("CTRL:ERR", 0) == 0) {
              if (!saw_err) { saw_err = true; first_err = ln; }
              break;
            }
            // Look for CTRL:ACK est_ms=...
            if (ln.rfind("CTRL:ACK", 0) == 0 && ln.find("est_ms=") != std::string::npos) {
              // parse est_ms value
              size_t p = ln.find("est_ms=");
              if (p != std::string::npos) {
                p += 7; // past 'est_ms='
                // read number
                uint32_t v = 0;
                while (p < ln.size() && ln[p] == ' ') ++p;
                // Robust parse until non-digit
                uint32_t acc = 0; bool any = false;
                while (p < ln.size() && ln[p] >= '0' && ln[p] <= '9') { acc = acc * 10 + (uint32_t)(ln[p] - '0'); any = true; ++p; }
                if (any) { if (acc > agg_est_ms) agg_est_ms = acc; saw_est = true; }
              }
              // Do not append this OK line; it will be consolidated later
            } else {
              if (!combined.empty()) combined.push_back('\n');
              combined += ln;
            }
          }
          if (pos == std::string::npos || saw_err) break;
          start = pos + 1;
        }
        if (saw_err) break;
      }
    }
    s_in_batch = prev_batch;
    s_batch_initially_idle = prev_idle;
    if (saw_err) {
      return first_err;
    }
    if (saw_est) {
      uint32_t cid = net_onboarding::NextCid();
      if (!combined.empty()) combined.push_back('\n');
      std::ostringstream ok; ok << "CTRL:ACK CID=" << cid << " est_ms=" << agg_est_ms; combined += ok.str();
    }
    return combined;
  }
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
  if (verb == "NET")
  {
    // Delegate to NET sub-commands; do not advance motor controller time here
    // NET commands are independent of motor state
    using net_onboarding::Net;
    using net_onboarding::State;
    // args can be "RESET", "STATUS" or "SET,<ssid>,<pass>"
    std::string a = trim(args);
    // Normalize sub-verb for comparisons
    std::string up = to_upper_copy(a);
    // Extract first token before ',' if present for sub-verb
    std::string sub = up;
    size_t comma = up.find(',');
    if (comma != std::string::npos) sub = trim_copy(up.substr(0, comma));

    if (sub == "RESET")
    {
      auto before = Net().status().state;
      // Allocate CID and mark active for subsequent async NET events
      uint32_t cid = net_onboarding::NextCid();
      // Reject RESET while connecting
      if (before == State::CONNECTING) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BUSY_CONNECTING"; return eo.str(); }
      net_onboarding::SetActiveCid(cid);
      Net().resetCredentials();
      // If we were already in AP mode prior to RESET, emit an explicit event line
      // to confirm AP is active (the main loop will emit on transitions otherwise).
      auto after = Net().status();
      if (before == State::AP_ACTIVE)
      {
        std::ostringstream os;
        os << "CTRL:ACK CID=" << cid << "\n";
        os << "CTRL: NET:AP_ACTIVE CID=" << cid
           << " ssid=" << after.ssid.data()
           << " ip=" << after.ip.data();
        // Immediate AP_ACTIVE completes the RESET command; clear CID
        net_onboarding::ClearActiveCid();
        return os.str();
      }
      std::ostringstream ack; ack << "CTRL:ACK CID=" << cid; return ack.str();
    }
    if (sub == "STATUS")
    {
      auto s = Net().status();
      uint32_t cid = net_onboarding::NextCid();
      std::ostringstream os;
      os << "CTRL:ACK CID=" << cid << ' ' << "state=";
      if (s.state == State::AP_ACTIVE) os << "AP_ACTIVE";
      else if (s.state == State::CONNECTING) os << "CONNECTING";
      else if (s.state == State::CONNECTED) os << "CONNECTED";
      os << " rssi=";
      if (s.state == State::CONNECTED) os << s.rssi_dbm; else os << "NA";
      os << " ip=" << s.ip.data();
      os << " ssid=" << quote_string(std::string(s.ssid.data()));
      // Password exposure rules: show AP password; mask otherwise
      if (s.state == State::AP_ACTIVE) {
        std::array<char,65> pass; Net().apPassword(pass);
        os << " pass=" << quote_string(std::string(pass.data()));
      } else {
        os << " pass=" << quote_string("********");
      }
      return os.str();
    }
    if (sub == "SET")
    {
      // Allocate CID for async flow now
      uint32_t cid = net_onboarding::NextCid();
      // Reject SET while connecting
      if (Net().status().state == State::CONNECTING) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BUSY_CONNECTING"; return eo.str(); }
      // Parse CSV with quoted fields; expect 3 tokens: SET, ssid, pass
      auto toks = parseCsvQuoted(a);
      if (toks.size() != 3) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BAD_PARAM"; return eo.str(); }
      // Validate ssid/pass
      const std::string &ssid = toks[1];
      const std::string &pass = toks[2];
      if (ssid.empty() || ssid.size() > 32) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BAD_PARAM"; return eo.str(); }
      if (!(pass.size() == 0 || (pass.size() >= 8 && pass.size() <= 63))) {
        if (pass.size() > 0 && pass.size() < 8) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BAD_PARAM PASS_TOO_SHORT"; return eo.str(); }
        { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BAD_PARAM"; return eo.str(); }
      }
      // Save + attempt connect
      net_onboarding::SetActiveCid(cid);
      bool ok = Net().setCredentials(ssid.c_str(), pass.c_str());
      if (!ok) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_SAVE_FAILED"; return eo.str(); }
      std::ostringstream ack; ack << "CTRL:ACK CID=" << cid; return ack.str();
    }
    if (sub == "LIST")
    {
      // Suspend during CONNECTING to keep timing bounded
      uint32_t cid = net_onboarding::NextCid();
      if (Net().status().state == State::CONNECTING) { std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BUSY_CONNECTING"; return eo.str(); }
      std::ostringstream os;
      bool need_newline = false;

      // Emit ACK immediately for on-device UX
      std::ostringstream ack;
      ack << "CTRL:ACK CID=" << cid << " scanning=1";
      if (!net_onboarding::PrintCtrlLineImmediate(ack.str())) {
        os << ack.str();
        need_newline = true;
      }

      std::vector<net_onboarding::WifiScanResult> nets;
      int n = Net().scanNetworks(nets, /*max_results=*/12, /*include_hidden=*/true);

      std::ostringstream header;
      header << "NET:LIST CID=" << cid;
      if (!net_onboarding::PrintCtrlLineImmediate(header.str())) {
        if (need_newline) os << "\n";
        os << header.str();
        need_newline = true;
      }

      for (int i = 0; i < n; ++i) {
        const auto &r = nets[(size_t)i];
        std::ostringstream line;
        line << "SSID=" << quote_string(r.ssid)
             << " rssi=" << r.rssi
             << " secure=" << (r.secure ? 1 : 0)
             << " channel=" << r.channel;
        if (!net_onboarding::PrintCtrlLineImmediate(line.str())) {
          if (need_newline) os << "\n";
          os << line.str();
          need_newline = true;
        }
      }
      return os.str();
    }
    { std::ostringstream eo; eo << "CTRL:ERR CID=" << net_onboarding::NextCid() << " E03 BAD_PARAM"; return eo.str(); }
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
  { std::ostringstream eo; eo << "CTRL:ERR CID=" << net_onboarding::NextCid() << " E01 BAD_CMD"; return eo.str(); }
}
