#include "MotorControl/command/CommandHandlers.h"

#include "MotorControl/command/CommandResult.h"
#include "MotorControl/command/CommandUtils.h"
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/BuildConfig.h"

#include <array>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>

namespace motor {
namespace command {

namespace {

constexpr long kMinPos = MotorControlConstants::MIN_POS_STEPS;
constexpr long kMaxPos = MotorControlConstants::MAX_POS_STEPS;

uint32_t parseEstMs(const std::string &line) {
  size_t pos = line.find("est_ms=");
  if (pos == std::string::npos) return 0;
  pos += 7;
  while (pos < line.size() && line[pos] == ' ') ++pos;
  uint32_t value = 0;
  bool any = false;
  while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
    value = value * 10u + static_cast<uint32_t>(line[pos] - '0');
    ++pos;
    any = true;
  }
  return any ? value : 0;
}

std::string formatAckWithEst(uint32_t cid, uint32_t est_ms) {
  std::ostringstream os;
  os << "CTRL:ACK CID=" << cid << " est_ms=" << est_ms;
  return os.str();
}

} // namespace

// ---------------- MotorCommandHandler ----------------

bool MotorCommandHandler::canHandle(const std::string &verb) const {
  return verb == "MOVE" || verb == "M" ||
         verb == "HOME" || verb == "H" ||
         verb == "WAKE" || verb == "SLEEP";
}

CommandResult MotorCommandHandler::execute(const ParsedCommand &command,
                                           CommandExecutionContext &context,
                                           uint32_t now_ms) {
  if (command.verb == "WAKE") {
    context.controller().tick(now_ms);
    return handleWake(command.args, context);
  }
  if (command.verb == "SLEEP") {
    context.controller().tick(now_ms);
    return handleSleep(command.args, context);
  }
  if (command.verb == "MOVE" || command.verb == "M") {
    return handleMove(command.args, context, now_ms);
  }
  if (command.verb == "HOME" || command.verb == "H") {
    return handleHome(command.args, context, now_ms);
  }
  return CommandResult::Error("CTRL:ERR CID=" + std::to_string(context.nextCid()) + " E01 BAD_CMD");
}

CommandResult MotorCommandHandler::handleWake(const std::string &args, CommandExecutionContext &context) {
  uint32_t cid = context.nextCid();
  uint32_t mask;
  if (!ParseIdMask(Trim(args), mask, context.controller().motorCount())) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID";
    return CommandResult::Error(eo.str());
  }
  for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
    if ((mask & (1u << id)) == 0) continue;
    const MotorState &s = context.controller().state(id);
    int avail_s = (s.budget_tenths >= 0) ? (s.budget_tenths / 10) : 0;
    if (avail_s <= 0) {
      if (context.thermalLimitsEnabled()) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E12 THERMAL_NO_BUDGET_WAKE";
        return CommandResult::Error(eo.str());
      } else {
        context.controller().wakeMask(mask);
        std::ostringstream out;
        out << "CTRL:WARN CID=" << cid << " THERMAL_NO_BUDGET_WAKE\nCTRL:ACK CID=" << cid;
        CommandResult res;
        res.lines = Split(out.str(), '\n');
        return res;
      }
    }
  }
  context.controller().wakeMask(mask);
  return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
}

CommandResult MotorCommandHandler::handleSleep(const std::string &args, CommandExecutionContext &context) {
  uint32_t cid = context.nextCid();
  uint32_t mask;
  if (!ParseIdMask(Trim(args), mask, context.controller().motorCount())) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID";
    return CommandResult::Error(eo.str());
  }
  if (!context.controller().sleepMask(mask)) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
    return CommandResult::Error(eo.str());
  }
  return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
}

CommandResult MotorCommandHandler::handleMove(const std::string &args,
                                              CommandExecutionContext &context,
                                              uint32_t now_ms) {
  using motor::command::Split;
  uint32_t cid = context.nextCid();
  auto parts = Split(args, ',');
  if (parts.size() < 2) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
    return CommandResult::Error(eo.str());
  }
  uint32_t mask;
  if (!ParseIdMask(Trim(parts[0]), mask, context.controller().motorCount())) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID";
    return CommandResult::Error(eo.str());
  }
  long target;
  if (!ParseInt(Trim(parts[1]), target)) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
    return CommandResult::Error(eo.str());
  }
  if (target < kMinPos || target > kMaxPos) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E07 POS_OUT_OF_RANGE";
    return CommandResult::Error(eo.str());
  }
  int speed = context.defaultSpeed();
  int accel = context.defaultAccel();
#if (USE_SHARED_STEP)
  if ((parts.size() >= 3 && !Trim(parts[2]).empty()) ||
      (parts.size() >= 4 && !Trim(parts[3]).empty())) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
    return CommandResult::Error(eo.str());
  }
#else
  if (parts.size() >= 3 && !Trim(parts[2]).empty()) {
    if (!ParseInt(Trim(parts[2]), speed) || speed <= 0) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
  }
  if (parts.size() >= 4 && !Trim(parts[3]).empty()) {
    if (!ParseInt(Trim(parts[3]), accel) || accel <= 0) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
  }
  if (parts.size() > 4) {
    for (size_t i = 4; i < parts.size(); ++i) {
      if (!Trim(parts[i]).empty()) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
        return CommandResult::Error(eo.str());
      }
    }
  }
#endif
#if (USE_SHARED_STEP)
  if (!(context.inBatch() && context.batchInitiallyIdle())) {
    for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
      if (context.controller().state(id).moving) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
        return CommandResult::Error(eo.str());
      }
    }
  }
#endif
  context.controller().tick(now_ms);
  uint32_t tmp = mask;
  uint32_t max_req_ms = 0;
  for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
    if ((tmp & (1u << id)) == 0) continue;
    const MotorState &s = context.controller().state(id);
    long dist = std::labs(target - s.position);
    uint32_t req_ms = 0;
#if (USE_SHARED_STEP)
    req_ms = MotionKinematics::estimateMoveTimeMsSharedStep(dist, speed, accel, context.defaultDecel());
#else
    req_ms = MotionKinematics::estimateMoveTimeMs(dist, speed, accel);
#endif
    if (req_ms > max_req_ms) max_req_ms = req_ms;
    int req_s = static_cast<int>((req_ms + 999) / 1000);
    if (req_s > static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S)) {
      if (context.thermalLimitsEnabled()) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E10 THERMAL_REQ_GT_MAX id=" << static_cast<int>(id)
                                  << " req_ms=" << req_ms
                                  << " max_budget_s=" << static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S);
        return CommandResult::Error(eo.str());
      } else {
        std::ostringstream wo; wo << "CTRL:WARN CID=" << cid << " THERMAL_REQ_GT_MAX id=" << static_cast<int>(id)
                                  << " req_ms=" << req_ms
                                  << " max_budget_s=" << static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S);
        if (!context.controller().moveAbsMask(mask, target, speed, accel, now_ms)) {
          std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
          return CommandResult::Error(eo.str());
        }
        std::ostringstream ack; ack << "CTRL:ACK CID=" << cid << " est_ms=" << req_ms;
        CommandResult res;
        res.lines.push_back(wo.str());
        res.lines.push_back(ack.str());
        return res;
      }
    }
  }
  for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
    if ((mask & (1u << id)) == 0) continue;
    const MotorState &s = context.controller().state(id);
    int avail_s = (s.budget_tenths >= 0) ? (s.budget_tenths / 10) : 0;
    int req_s = static_cast<int>((max_req_ms + 999) / 1000);
    int32_t missing_t = MotorControlConstants::BUDGET_TENTHS_MAX - s.budget_tenths;
    if (missing_t < 0) missing_t = 0;
    int32_t ttfc_tenths = (missing_t <= 0) ? 0 :
      static_cast<int32_t>(((static_cast<int64_t>(missing_t) * 10 + MotorControlConstants::REFILL_TENTHS_PER_SEC - 1) /
                            MotorControlConstants::REFILL_TENTHS_PER_SEC));
    const int32_t kTtfcMaxTenths = MotorControlConstants::MAX_COOL_DOWN_TIME_S * 10;
    if (ttfc_tenths > kTtfcMaxTenths) ttfc_tenths = kTtfcMaxTenths;
    int ttfc_s = ttfc_tenths / 10;
    if (req_s > avail_s) {
      if (context.thermalLimitsEnabled()) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E11 THERMAL_NO_BUDGET id=" << static_cast<int>(id)
                                  << " req_ms=" << max_req_ms
                                  << " budget_s=" << avail_s
                                  << " ttfc_s=" << ttfc_s;
        return CommandResult::Error(eo.str());
      } else {
        std::ostringstream wo; wo << "CTRL:WARN CID=" << cid << " THERMAL_NO_BUDGET id=" << static_cast<int>(id)
                                  << " req_ms=" << max_req_ms
                                  << " budget_s=" << avail_s
                                  << " ttfc_s=" << ttfc_s;
        if (!context.controller().moveAbsMask(mask, target, speed, accel, now_ms)) {
          std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
          return CommandResult::Error(eo.str());
        }
        std::ostringstream ack; ack << "CTRL:ACK CID=" << cid << " est_ms=" << max_req_ms;
        CommandResult res;
        res.lines.push_back(wo.str());
        res.lines.push_back(ack.str());
        return res;
      }
    }
  }
  if (!context.controller().moveAbsMask(mask, target, speed, accel, now_ms)) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
    return CommandResult::Error(eo.str());
  }
  std::ostringstream ack; ack << "CTRL:ACK CID=" << cid << " est_ms=" << max_req_ms;
  return CommandResult::SingleLine(ack.str());
}

CommandResult MotorCommandHandler::handleHome(const std::string &args,
                                              CommandExecutionContext &context,
                                              uint32_t now_ms) {
  auto parts = Split(args, ',');
  uint32_t cid = context.nextCid();
  if (parts.empty()) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
    return CommandResult::Error(eo.str());
  }
  uint32_t mask;
  if (!ParseIdMask(Trim(parts[0]), mask, context.controller().motorCount())) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID";
    return CommandResult::Error(eo.str());
  }

  auto token = [&](size_t idx) -> std::string {
    if (idx >= parts.size()) return std::string();
    return Trim(parts[idx]);
  };

  long overshoot = MotorControlConstants::DEFAULT_OVERSHOOT;
  long backoff = MotorControlConstants::DEFAULT_BACKOFF;
  long full_range = 0;
  int speed = context.defaultSpeed();
  int accel = context.defaultAccel();

  if (!token(1).empty()) {
    if (!ParseInt(token(1), overshoot)) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
  }
  if (!token(2).empty()) {
    if (!ParseInt(token(2), backoff)) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
  }

#if (USE_SHARED_STEP)
  if (!token(3).empty()) {
    if (!ParseInt(token(3), full_range)) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
  }
  if (!token(4).empty()) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
    return CommandResult::Error(eo.str());
  }
#else
  if (parts.size() == 4 && !token(3).empty()) {
    if (!ParseInt(token(3), full_range)) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
  } else {
    if (!token(3).empty()) {
      if (!ParseInt(token(3), speed) || speed <= 0) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
        return CommandResult::Error(eo.str());
      }
    }
    if (!token(4).empty()) {
      if (!ParseInt(token(4), accel) || accel <= 0) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
        return CommandResult::Error(eo.str());
      }
    }
    if (!token(5).empty()) {
      if (!ParseInt(token(5), full_range)) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
        return CommandResult::Error(eo.str());
      }
    }
    if (parts.size() > 6) {
      for (size_t i = 6; i < parts.size(); ++i) {
        if (!Trim(parts[i]).empty()) {
          std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
          return CommandResult::Error(eo.str());
        }
      }
    }
  }
#endif

  context.controller().tick(now_ms);
  if (full_range <= 0) {
    full_range = MotorControlConstants::MAX_POS_STEPS - MotorControlConstants::MIN_POS_STEPS;
  }

  uint32_t req_ms_total = 0;
#if (USE_SHARED_STEP)
  req_ms_total = MotionKinematics::estimateHomeTimeMsWithFullRangeSharedStep(
      overshoot, backoff, full_range, speed, accel, context.defaultDecel());
#else
  req_ms_total = MotionKinematics::estimateHomeTimeMsWithFullRange(
      overshoot, backoff, full_range, speed, accel);
#endif

  int req_s = static_cast<int>((req_ms_total + 999) / 1000);
  uint8_t first_id = 0;
  for (; first_id < context.controller().motorCount(); ++first_id) {
    if (mask & (1u << first_id)) break;
  }

  if (req_s > static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S)) {
    if (context.thermalLimitsEnabled()) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E10 THERMAL_REQ_GT_MAX id=" << static_cast<int>(first_id)
                                << " req_ms=" << req_ms_total
                                << " max_budget_s=" << static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S);
      return CommandResult::Error(eo.str());
    } else {
      std::ostringstream wo; wo << "CTRL:WARN CID=" << cid << " THERMAL_REQ_GT_MAX id=" << static_cast<int>(first_id)
                                << " req_ms=" << req_ms_total
                                << " max_budget_s=" << static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S);
      if (!context.controller().homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms)) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
        return CommandResult::Error(eo.str());
      }
      std::ostringstream out;
      out << wo.str() << "\n"
          << "CTRL:ACK CID=" << cid << " est_ms=" << req_ms_total;
      CommandResult res;
      res.lines = Split(out.str(), '\n');
      return res;
    }
  }

  for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
    if ((mask & (1u << id)) == 0) continue;
    const MotorState &s = context.controller().state(id);
    int avail_s = (s.budget_tenths >= 0) ? (s.budget_tenths / 10) : 0;
    int32_t missing_t = MotorControlConstants::BUDGET_TENTHS_MAX - s.budget_tenths;
    if (missing_t < 0) missing_t = 0;
    int32_t ttfc_tenths = (missing_t <= 0) ? 0 :
      static_cast<int32_t>(((static_cast<int64_t>(missing_t) * 10 + MotorControlConstants::REFILL_TENTHS_PER_SEC - 1) /
                            MotorControlConstants::REFILL_TENTHS_PER_SEC));
    const int32_t kTtfcMaxTenths = MotorControlConstants::MAX_COOL_DOWN_TIME_S * 10;
    if (ttfc_tenths > kTtfcMaxTenths) ttfc_tenths = kTtfcMaxTenths;
    int ttfc_s = ttfc_tenths / 10;
    if (req_s > avail_s) {
      if (context.thermalLimitsEnabled()) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E11 THERMAL_NO_BUDGET id=" << static_cast<int>(id)
                                  << " req_ms=" << req_ms_total
                                  << " budget_s=" << avail_s
                                  << " ttfc_s=" << ttfc_s;
        return CommandResult::Error(eo.str());
      } else {
        std::ostringstream wo; wo << "CTRL:WARN CID=" << cid << " THERMAL_NO_BUDGET id=" << static_cast<int>(id)
                                  << " req_ms=" << req_ms_total
                                  << " budget_s=" << avail_s
                                  << " ttfc_s=" << ttfc_s;
        if (!context.controller().homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms)) {
          std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
          return CommandResult::Error(eo.str());
        }
        std::ostringstream out;
        out << wo.str() << "\n"
            << "CTRL:ACK CID=" << cid << " est_ms=" << req_ms_total;
        CommandResult res;
        res.lines = Split(out.str(), '\n');
        return res;
      }
    }
  }

  if (!context.controller().homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms)) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
    return CommandResult::Error(eo.str());
  }
  std::ostringstream ok;
  ok << "CTRL:ACK CID=" << cid << " est_ms=" << req_ms_total;
  return CommandResult::SingleLine(ok.str());
}

// ---------------- QueryCommandHandler ----------------

bool QueryCommandHandler::canHandle(const std::string &verb) const {
  return verb == "HELP" || verb == "STATUS" || verb == "ST" ||
         verb == "GET" || verb == "SET";
}

CommandResult QueryCommandHandler::execute(const ParsedCommand &command,
                                           CommandExecutionContext &context,
                                           uint32_t now_ms) {
  (void)now_ms;
  if (command.verb == "HELP") {
    return handleHelp();
  }
  if (command.verb == "STATUS" || command.verb == "ST") {
    context.controller().tick(now_ms);
    return handleStatus(context);
  }
  if (command.verb == "GET") {
    context.controller().tick(now_ms);
    return handleGet(command.args, context);
  }
  if (command.verb == "SET") {
    context.controller().tick(now_ms);
    return handleSet(command.args, context);
  }
  return CommandResult::Error("CTRL:ERR CID=" + std::to_string(context.nextCid()) + " E01 BAD_CMD");
}

CommandResult QueryCommandHandler::handleHelp() const {
  std::ostringstream os;
  os << "HELP\n";
  os << "MOVE:<id|ALL>,<abs_steps>\n";
  os << "HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]\n";
  os << "NET:RESET\n";
  os << "NET:STATUS\n";
  os << "NET:SET,\"<ssid>\",\"<pass>\" (quote to allow commas/spaces; escape \\\" and \\\\)\n";
  os << "NET:LIST (scan nearby SSIDs; AP mode only)\n";
#if !(USE_SHARED_STEP)
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
  os << "GET THERMAL_LIMITING\n";
  os << "SET THERMAL_LIMITING=OFF|ON\n";
  os << "SET SPEED=<steps_per_second>\n";
  os << "SET ACCEL=<steps_per_second^2>\n";
  os << "SET DECEL=<steps_per_second^2>\n";
  os << "WAKE:<id|ALL>\n";
  os << "SLEEP:<id|ALL>\n";
  os << "Shortcuts: M=MOVE, H=HOME, ST=STATUS\n";
  os << "Multicommand: <cmd1>;<cmd2> note: no cmd queuing; only distinct motors allowed";
  CommandResult res;
  res.lines = Split(os.str(), '\n');
  return res;
}

CommandResult QueryCommandHandler::handleStatus(CommandExecutionContext &context) {
  uint32_t cid = context.nextCid();
  CommandResult res;
  std::ostringstream ack;
  ack << "CTRL:ACK CID=" << cid;
  res.lines.push_back(ack.str());
  for (size_t i = 0; i < context.controller().motorCount(); ++i) {
    const MotorState &s = context.controller().state(i);
    int32_t budget_t = s.budget_tenths;
    bool budget_neg = (budget_t < 0);
    int32_t budget_abs = budget_neg ? -budget_t : budget_t;
    int32_t missing_t = MotorControlConstants::BUDGET_TENTHS_MAX - s.budget_tenths;
    if (missing_t < 0) missing_t = 0;
    int32_t ttfc_tenths = (missing_t <= 0) ? 0 :
      static_cast<int32_t>(((static_cast<int64_t>(missing_t) * 10 + MotorControlConstants::REFILL_TENTHS_PER_SEC - 1) /
                            MotorControlConstants::REFILL_TENTHS_PER_SEC));
    const int32_t kTtfcMaxTenths = MotorControlConstants::MAX_COOL_DOWN_TIME_S * 10;
    if (ttfc_tenths > kTtfcMaxTenths) ttfc_tenths = kTtfcMaxTenths;
    std::ostringstream os;
    os << "id=" << static_cast<int>(s.id)
       << " pos=" << s.position
       << " moving=" << (s.moving ? 1 : 0)
       << " awake=" << (s.awake ? 1 : 0)
       << " homed=" << (s.homed ? 1 : 0)
       << " steps_since_home=" << s.steps_since_home
       << " budget_s=" << (budget_neg ? "-" : "") << (budget_abs / 10) << "." << (budget_abs % 10)
       << " ttfc_s=" << (ttfc_tenths / 10) << "." << (ttfc_tenths % 10)
       << " speed=" << s.speed
       << " accel=" << s.accel
       << " est_ms=" << s.last_op_est_ms
       << " started_ms=" << s.last_op_started_ms;
    if (!s.last_op_ongoing) os << " actual_ms=" << s.last_op_last_ms;
    res.lines.push_back(os.str());
  }
  return res;
}

CommandResult QueryCommandHandler::handleGet(const std::string &args, CommandExecutionContext &context) {
  uint32_t cid = context.nextCid();
  std::string key = ToUpperCopy(Trim(args));
  if (key.empty() || key == "ALL") {
    std::ostringstream os;
    os << "CTRL:ACK CID=" << cid << ' '
       << "SPEED=" << context.defaultSpeed() << ' '
       << "ACCEL=" << context.defaultAccel() << ' '
       << "DECEL=" << context.defaultDecel() << ' '
       << "THERMAL_LIMITING=" << (context.thermalLimitsEnabled() ? "ON" : "OFF")
       << " max_budget_s=" << static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S);
    return CommandResult::SingleLine(os.str());
  }
  if (key == "SPEED") {
    std::ostringstream os; os << "CTRL:ACK CID=" << cid << " SPEED=" << context.defaultSpeed();
    return CommandResult::SingleLine(os.str());
  }
  if (key == "ACCEL") {
    std::ostringstream os; os << "CTRL:ACK CID=" << cid << " ACCEL=" << context.defaultAccel();
    return CommandResult::SingleLine(os.str());
  }
  if (key == "DECEL") {
    std::ostringstream os; os << "CTRL:ACK CID=" << cid << " DECEL=" << context.defaultDecel();
    return CommandResult::SingleLine(os.str());
  }
  if (key == "THERMAL_LIMITING") {
    std::ostringstream os;
    os << "CTRL:ACK CID=" << cid
       << " THERMAL_LIMITING=" << (context.thermalLimitsEnabled() ? "ON" : "OFF")
       << " max_budget_s=" << static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S);
    return CommandResult::SingleLine(os.str());
  }
  if (key.rfind("LAST_OP_TIMING", 0) == 0) {
    std::string rest;
    size_t p = key.find(':');
    if (p != std::string::npos) rest = Trim(key.substr(p + 1));
    if (rest.empty() || rest == "ALL") {
      CommandResult res;
      uint32_t list_cid = context.nextCid();
      std::ostringstream header;
      header << "CTRL:ACK CID=" << list_cid;
      res.lines.push_back(header.str());
      res.lines.push_back("LAST_OP_TIMING");
      for (uint8_t i = 0; i < context.controller().motorCount(); ++i) {
        const MotorState &s = context.controller().state(i);
        std::ostringstream os;
        os << "id=" << static_cast<int>(i)
           << " ongoing=" << (s.last_op_ongoing ? 1 : 0)
           << " est_ms=" << s.last_op_est_ms
           << " started_ms=" << s.last_op_started_ms;
        if (!s.last_op_ongoing) os << " actual_ms=" << s.last_op_last_ms;
        res.lines.push_back(os.str());
      }
      return res;
    }
    uint32_t mask;
    if (!ParseIdMask(rest, mask, context.controller().motorCount())) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID";
      return CommandResult::Error(eo.str());
    }
    uint8_t id = 0;
    for (; id < context.controller().motorCount(); ++id) if (mask & (1u << id)) break;
    if (id >= context.controller().motorCount()) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E02 BAD_ID";
      return CommandResult::Error(eo.str());
    }
    const MotorState &s = context.controller().state(id);
    std::ostringstream os;
    os << "CTRL:ACK CID=" << cid << " LAST_OP_TIMING ongoing=" << (s.last_op_ongoing ? 1 : 0)
       << " id=" << static_cast<int>(id)
       << " est_ms=" << s.last_op_est_ms
       << " started_ms=" << s.last_op_started_ms;
    if (!s.last_op_ongoing) os << " actual_ms=" << s.last_op_last_ms;
    return CommandResult::SingleLine(os.str());
  }
  std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
  return CommandResult::Error(eo.str());
}

CommandResult QueryCommandHandler::handleSet(const std::string &args, CommandExecutionContext &context) {
  uint32_t cid = context.nextCid();
  std::string payload = Trim(args);
  std::string up = ToUpperCopy(payload);
  size_t eq = up.find('=');
  if (eq == std::string::npos) {
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
    return CommandResult::Error(eo.str());
  }
  std::string key = Trim(up.substr(0, eq));
  std::string val = Trim(up.substr(eq + 1));
  if (key == "THERMAL_LIMITING") {
    if (val == "ON") {
      context.setThermalLimitsEnabled(true);
      return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
    } else if (val == "OFF") {
      context.setThermalLimitsEnabled(false);
      return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
    }
    std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
    return CommandResult::Error(eo.str());
  }
  if (key == "SPEED") {
    long v;
    if (!ParseInt(val, v) || v <= 0) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
    for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
      if (context.controller().state(id).moving) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
        return CommandResult::Error(eo.str());
      }
    }
    context.defaultSpeed() = static_cast<int>(v);
    return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
  }
  if (key == "ACCEL") {
    long v;
    if (!ParseInt(val, v) || v <= 0) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
    for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
      if (context.controller().state(id).moving) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
        return CommandResult::Error(eo.str());
      }
    }
    context.defaultAccel() = static_cast<int>(v);
    return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
  }
  if (key == "DECEL") {
    long v;
    if (!ParseInt(val, v) || v < 0) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
    for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
      if (context.controller().state(id).moving) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E04 BUSY";
        return CommandResult::Error(eo.str());
      }
    }
    context.defaultDecel() = static_cast<int>(v);
    context.controller().setDeceleration(context.defaultDecel());
    return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
  }
  std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " E03 BAD_PARAM";
  return CommandResult::Error(eo.str());
}

// ---------------- NetCommandHandler ----------------

bool NetCommandHandler::canHandle(const std::string &verb) const {
  return verb == "NET";
}

CommandResult NetCommandHandler::execute(const ParsedCommand &command,
                                         CommandExecutionContext &context,
                                         uint32_t /*now_ms*/) {
  using net_onboarding::State;

  std::string a = Trim(command.args);
  std::string up = ToUpperCopy(a);
  std::string sub = up;
  size_t comma = up.find(',');
  if (comma != std::string::npos) sub = Trim(up.substr(0, comma));

  if (sub == "RESET") {
    auto before = context.net().status().state;
    uint32_t cid = context.nextCid();
    if (before == State::CONNECTING) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BUSY_CONNECTING";
      return CommandResult::Error(eo.str());
    }
    context.setActiveCid(cid);
    context.net().resetCredentials();
    auto after = context.net().status();
    if (before == State::AP_ACTIVE) {
      std::ostringstream os;
      os << "CTRL:ACK CID=" << cid << "\n";
      os << "CTRL: NET:AP_ACTIVE CID=" << cid
         << " ssid=" << after.ssid.data()
         << " ip=" << after.ip.data();
      context.clearActiveCid();
      CommandResult res;
      res.lines = Split(os.str(), '\n');
      return res;
    }
    return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
  }

  if (sub == "STATUS") {
    auto s = context.net().status();
    uint32_t cid = context.nextCid();
    std::ostringstream os;
    os << "CTRL:ACK CID=" << cid << ' ' << "state=";
    if (s.state == State::AP_ACTIVE) os << "AP_ACTIVE";
    else if (s.state == State::CONNECTING) os << "CONNECTING";
    else if (s.state == State::CONNECTED) os << "CONNECTED";
    os << " rssi=";
    if (s.state == State::CONNECTED) os << s.rssi_dbm; else os << "NA";
    os << " ip=" << s.ip.data();
    os << " ssid=" << QuoteString(std::string(s.ssid.data()));
    if (s.state == State::AP_ACTIVE) {
      std::array<char,65> pass; context.net().apPassword(pass);
      os << " pass=" << QuoteString(std::string(pass.data()));
    } else {
      os << " pass=" << QuoteString("********");
    }
    return CommandResult::SingleLine(os.str());
  }

  if (sub == "SET") {
    uint32_t cid = context.nextCid();
    if (context.net().status().state == State::CONNECTING) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BUSY_CONNECTING";
      return CommandResult::Error(eo.str());
    }
    auto toks = ParseCsvQuoted(a);
    if (toks.size() != 3) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
    const std::string &ssid = toks[1];
    const std::string &pass = toks[2];
    if (ssid.empty() || ssid.size() > 32) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
    if (!(pass.size() == 0 || (pass.size() >= 8 && pass.size() <= 63))) {
      if (pass.size() > 0 && pass.size() < 8) {
        std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BAD_PARAM PASS_TOO_SHORT";
        return CommandResult::Error(eo.str());
      }
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_BAD_PARAM";
      return CommandResult::Error(eo.str());
    }
    context.setActiveCid(cid);
    bool ok = context.net().setCredentials(ssid.c_str(), pass.c_str());
    if (!ok) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_SAVE_FAILED";
      return CommandResult::Error(eo.str());
    }
    return CommandResult::SingleLine("CTRL:ACK CID=" + std::to_string(cid));
  }

  if (sub == "LIST") {
    uint32_t cid = context.nextCid();
    if (context.net().status().state != State::AP_ACTIVE) {
      std::ostringstream eo; eo << "CTRL:ERR CID=" << cid << " NET_SCAN_AP_ONLY";
      return CommandResult::Error(eo.str());
    }
    CommandResult res;
    bool need_newline = false;

    std::ostringstream ack;
    ack << "CTRL:ACK CID=" << cid << " scanning=1";
    if (!context.printCtrlLineImmediate(ack.str())) {
      res.lines.push_back(ack.str());
      need_newline = true;
    }

    std::vector<net_onboarding::WifiScanResult> nets;
    int n = context.net().scanNetworks(nets, 12, true);

    std::ostringstream header;
    header << "NET:LIST CID=" << cid;
    if (!context.printCtrlLineImmediate(header.str())) {
      res.lines.push_back(header.str());
      need_newline = true;
    }

    for (int i = 0; i < n; ++i) {
      const auto &r = nets[static_cast<size_t>(i)];
      std::ostringstream line;
      line << "SSID=" << QuoteString(r.ssid)
           << " rssi=" << r.rssi
           << " secure=" << (r.secure ? 1 : 0)
           << " channel=" << r.channel;
      if (!context.printCtrlLineImmediate(line.str())) {
        res.lines.push_back(line.str());
        need_newline = true;
      }
    }
    if (!need_newline && res.lines.empty()) {
      // Ensure at least an empty payload to preserve original semantics.
      res.lines.emplace_back();
      res.lines.clear();
    }
    return res;
  }

  std::ostringstream eo; eo << "CTRL:ERR CID=" << context.nextCid() << " E03 BAD_PARAM";
  return CommandResult::Error(eo.str());
}

} // namespace command
} // namespace motor
