#include "MotorControl/command/CommandHandlers.h"

#include "MotorControl/command/CommandResult.h"
#include "MotorControl/command/CommandUtils.h"
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/BuildConfig.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"
#include "transport/CompletionTracker.h"
#include "transport/CommandSchema.h"

#include <array>
#include <utility>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>

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

std::string FormatSignedTenths(int32_t tenths) {
  bool negative = tenths < 0;
  int32_t abs_value = negative ? -tenths : tenths;
  int32_t whole = abs_value / 10;
  int32_t fractional = abs_value % 10;
  std::ostringstream oss;
  if (negative) {
    oss << '-';
  }
  oss << whole << '.' << fractional;
  return oss.str();
}

std::string FormatTenths(int32_t tenths) {
  int32_t whole = tenths / 10;
  int32_t fractional = tenths % 10;
  std::ostringstream oss;
  oss << whole << '.' << fractional;
  return oss.str();
}

inline const char *BoolToFlag(bool value) {
  return value ? "1" : "0";
}

void EmitResponseEvent(const char *action, const transport::command::ResponseLine &line) {
  transport::response::ResponseDispatcher::Instance().Emit(
      transport::response::BuildEvent(line, action ? action : std::string()));
}

CommandResult MakeResultWithLine(const char *action, transport::command::ResponseLine line) {
  EmitResponseEvent(action, line);
  CommandResult res;
  res.append(line);
  return res;
}

CommandResult MakeDoneResult(const char *action,
                             const std::string &msg_id,
                             const std::vector<transport::command::Field> &fields = {}) {
  transport::response::Event event;
  event.type = transport::response::EventType::kDone;
  event.cmd_id = msg_id;
  if (action) {
    event.action = action;
  }
  event.attributes["status"] = "done";
  for (const auto &field : fields) {
    event.attributes[field.key] = field.value;
  }
  transport::response::ResponseDispatcher::Instance().Emit(event);
  transport::command::ResponseLine line = transport::response::EventToLine(event);
  CommandResult res;
  res.append(line);
  return res;
}

CommandResult AppendDoneResult(CommandResult res,
                               const char *action,
                               const std::string &msg_id,
                               const std::vector<transport::command::Field> &fields = {}) {
  CommandResult done = MakeDoneResult(action, msg_id, fields);
  res.mergeFrom(done);
  return res;
}

const char *NetStateToString(net_onboarding::State state) {
  using net_onboarding::State;
  switch (state) {
  case State::AP_ACTIVE:
    return "AP_ACTIVE";
  case State::CONNECTING:
    return "CONNECTING";
  case State::CONNECTED:
    return "CONNECTED";
  default:
    return "UNKNOWN";
  }
}

} // namespace

// ---------------- MotorCommandHandler ----------------

bool MotorCommandHandler::canHandle(const std::string &action) const {
  return action == "MOVE" || action == "M" ||
         action == "HOME" || action == "H" ||
         action == "WAKE" || action == "SLEEP";
}

CommandResult MotorCommandHandler::execute(const ParsedCommand &command,
                                           CommandExecutionContext &context,
                                           uint32_t now_ms) {
  if (command.action == "WAKE") {
    context.controller().tick(now_ms);
    return handleWake(command.args, context);
  }
  if (command.action == "SLEEP") {
    context.controller().tick(now_ms);
    return handleSleep(command.args, context);
  }
  if (command.action == "MOVE" || command.action == "M") {
    return handleMove(command.args, context, now_ms);
  }
  if (command.action == "HOME" || command.action == "H") {
    return handleHome(command.args, context, now_ms);
  }
  auto err_line = transport::command::MakeErrorLine(
      context.nextMsgId(),
      "E01",
      "BAD_CMD",
      {});
  return MakeResultWithLine(command.action.c_str(), err_line);
}

CommandResult MotorCommandHandler::handleWake(const std::string &args, CommandExecutionContext &context) {
  constexpr const char *kAction = "WAKE";
  std::string msg_id = context.nextMsgId();
  uint32_t mask;
  if (!ParseIdMask(Trim(args), mask, context.controller().motorCount())) {
    auto err_line = transport::command::MakeErrorLine(msg_id, "E02", "BAD_ID", {});
    return MakeResultWithLine(kAction, err_line);
  }
  for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
    if ((mask & (1u << id)) == 0) {
      continue;
    }
    const MotorState &s = context.controller().state(id);
    int avail_s = (s.budget_tenths >= 0) ? (s.budget_tenths / 10) : 0;
    if (avail_s <= 0) {
      if (context.thermalLimitsEnabled()) {
        auto err_line = transport::command::MakeErrorLine(msg_id, "E12", "THERMAL_NO_BUDGET_WAKE", {});
        return MakeResultWithLine(kAction, err_line);
      }
      context.controller().wakeMask(mask);
      CommandResult res;
      auto warn_line = transport::command::MakeWarnLine(msg_id, "THERMAL_NO_BUDGET_WAKE", "", {});
      EmitResponseEvent(kAction, warn_line);
      res.append(warn_line);
      return AppendDoneResult(std::move(res), kAction, msg_id);
    }
  }
  context.controller().wakeMask(mask);
  return MakeDoneResult(kAction, msg_id);
}

CommandResult MotorCommandHandler::handleSleep(const std::string &args, CommandExecutionContext &context) {
  constexpr const char *kAction = "SLEEP";
  std::string msg_id = context.nextMsgId();
  uint32_t mask;
  if (!ParseIdMask(Trim(args), mask, context.controller().motorCount())) {
    auto err_line = transport::command::MakeErrorLine(msg_id, "E02", "BAD_ID", {});
    return MakeResultWithLine(kAction, err_line);
  }
  if (!context.controller().sleepMask(mask)) {
    auto err_line = transport::command::MakeErrorLine(msg_id, "E04", "BUSY", {});
    return MakeResultWithLine(kAction, err_line);
  }
  return MakeDoneResult(kAction, msg_id);
}

CommandResult MotorCommandHandler::handleMove(const std::string &args,
                                              CommandExecutionContext &context,
                                              uint32_t now_ms) {
  using motor::command::Split;
  std::string msg_id = context.nextMsgId();
  using transport::command::Field;
  auto emitLine = [&](const transport::command::ResponseLine &line) {
    auto event = transport::response::BuildEvent(line, "MOVE");
    transport::response::ResponseDispatcher::Instance().Emit(event);
  };
  auto emitError = [&](const std::string &code,
                       const std::string &reason,
                       std::initializer_list<Field> fields = {}) -> CommandResult {
    auto line = transport::command::MakeErrorLine(msg_id, code, reason, fields);
    emitLine(line);
    return CommandResult::Error(line);
  };
  auto appendLine = [&](CommandResult &res, const transport::command::ResponseLine &line) {
    emitLine(line);
    res.append(line);
  };
  auto emitAck = [&](std::initializer_list<Field> fields) -> CommandResult {
    auto line = transport::command::MakeAckLine(msg_id, fields);
    emitLine(line);
    return CommandResult::SingleLine(line);
  };
  auto parts = Split(args, ',');
  if (parts.size() < 2) {
    return emitError("E03", "BAD_PARAM");
  }
  uint32_t mask;
  if (!ParseIdMask(Trim(parts[0]), mask, context.controller().motorCount())) {
    return emitError("E02", "BAD_ID");
  }
  long target;
  if (!ParseInt(Trim(parts[1]), target)) {
    return emitError("E03", "BAD_PARAM");
  }
  if (target < kMinPos || target > kMaxPos) {
    return emitError("E07", "POS_OUT_OF_RANGE");
  }
  int speed = context.defaultSpeed();
  int accel = context.defaultAccel();
#if (USE_SHARED_STEP)
  if ((parts.size() >= 3 && !Trim(parts[2]).empty()) ||
      (parts.size() >= 4 && !Trim(parts[3]).empty())) {
    return emitError("E03", "BAD_PARAM");
  }
#else
  if (parts.size() >= 3 && !Trim(parts[2]).empty()) {
    if (!ParseInt(Trim(parts[2]), speed) || speed <= 0) {
      return emitError("E03", "BAD_PARAM");
    }
  }
  if (parts.size() >= 4 && !Trim(parts[3]).empty()) {
    if (!ParseInt(Trim(parts[3]), accel) || accel <= 0) {
      return emitError("E03", "BAD_PARAM");
    }
  }
  if (parts.size() > 4) {
    for (size_t i = 4; i < parts.size(); ++i) {
      if (!Trim(parts[i]).empty()) {
        return emitError("E03", "BAD_PARAM");
      }
    }
  }
#endif
#if (USE_SHARED_STEP)
  if (!(context.inBatch() && context.batchInitiallyIdle())) {
    for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
      if (context.controller().state(id).moving) {
        return emitError("E04", "BUSY");
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
        return emitError(
            "E10",
            "THERMAL_REQ_GT_MAX",
            {
                {"id", std::to_string(static_cast<int>(id))},
                {"req_ms", std::to_string(req_ms)},
                {"max_budget_s", std::to_string(static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S))}
            });
      } else {
        if (!context.controller().moveAbsMask(mask, target, speed, accel, now_ms)) {
          return emitError("E04", "BUSY");
        }
        transport::response::CompletionTracker::Instance().RegisterOperation(msg_id, "MOVE", mask, context.controller());
        CommandResult res;
        appendLine(res, transport::command::MakeWarnLine(
            msg_id,
            "THERMAL_REQ_GT_MAX",
            "",
            {
                {"id", std::to_string(static_cast<int>(id))},
                {"req_ms", std::to_string(req_ms)},
                {"max_budget_s", std::to_string(static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S))}
            }));
        appendLine(res, transport::command::MakeAckLine(
            msg_id,
            {{"est_ms", std::to_string(req_ms)}}));
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
        return emitError(
            "E11",
            "THERMAL_NO_BUDGET",
            {
                {"id", std::to_string(static_cast<int>(id))},
                {"req_ms", std::to_string(max_req_ms)},
                {"budget_s", std::to_string(avail_s)},
                {"ttfc_s", std::to_string(ttfc_s)}
            });
      } else {
        if (!context.controller().moveAbsMask(mask, target, speed, accel, now_ms)) {
          return emitError("E04", "BUSY");
        }
        transport::response::CompletionTracker::Instance().RegisterOperation(msg_id, "MOVE", mask, context.controller());
        CommandResult res;
        appendLine(res, transport::command::MakeWarnLine(
            msg_id,
            "THERMAL_NO_BUDGET",
            "",
            {
                {"id", std::to_string(static_cast<int>(id))},
                {"req_ms", std::to_string(max_req_ms)},
                {"budget_s", std::to_string(avail_s)},
                {"ttfc_s", std::to_string(ttfc_s)}
            }));
        appendLine(res, transport::command::MakeAckLine(
            msg_id,
            {{"est_ms", std::to_string(max_req_ms)}}));
        return res;
      }
    }
  }
  if (!context.controller().moveAbsMask(mask, target, speed, accel, now_ms)) {
    return emitError("E04", "BUSY");
  }
  transport::response::CompletionTracker::Instance().RegisterOperation(msg_id, "MOVE", mask, context.controller());
  return emitAck({{"est_ms", std::to_string(max_req_ms)}});
}

CommandResult MotorCommandHandler::handleHome(const std::string &args,
                                              CommandExecutionContext &context,
                                              uint32_t now_ms) {
  auto parts = Split(args, ',');
  std::string msg_id = context.nextMsgId();
  using transport::command::Field;
  auto emitLine = [&](const transport::command::ResponseLine &line) {
    auto event = transport::response::BuildEvent(line, "HOME");
    transport::response::ResponseDispatcher::Instance().Emit(event);
  };
  auto emitError = [&](const std::string &code,
                       const std::string &reason,
                       std::initializer_list<Field> fields = {}) -> CommandResult {
    auto line = transport::command::MakeErrorLine(msg_id, code, reason, fields);
    emitLine(line);
    return CommandResult::Error(line);
  };
  auto appendLine = [&](CommandResult &res, const transport::command::ResponseLine &line) {
    emitLine(line);
    res.append(line);
  };
  auto emitAck = [&](std::initializer_list<Field> fields) -> CommandResult {
    auto line = transport::command::MakeAckLine(msg_id, fields);
    emitLine(line);
    return CommandResult::SingleLine(line);
  };

  if (parts.empty()) {
    return emitError("E03", "BAD_PARAM");
  }
  uint32_t mask;
  if (!ParseIdMask(Trim(parts[0]), mask, context.controller().motorCount())) {
    return emitError("E02", "BAD_ID");
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
      return emitError("E03", "BAD_PARAM");
    }
  }
  if (!token(2).empty()) {
    if (!ParseInt(token(2), backoff)) {
      return emitError("E03", "BAD_PARAM");
    }
  }

#if (USE_SHARED_STEP)
  if (!token(3).empty()) {
    if (!ParseInt(token(3), full_range)) {
      return emitError("E03", "BAD_PARAM");
    }
  }
  if (!token(4).empty()) {
    return emitError("E03", "BAD_PARAM");
  }
#else
  if (!token(3).empty()) {
    if (!ParseInt(token(3), speed) || speed <= 0) {
      return emitError("E03", "BAD_PARAM");
    }
  }
  if (!token(4).empty()) {
    if (!ParseInt(token(4), accel) || accel <= 0) {
      return emitError("E03", "BAD_PARAM");
    }
  }
  if (!token(5).empty()) {
    if (!ParseInt(token(5), full_range)) {
      return emitError("E03", "BAD_PARAM");
    }
  }
  if (parts.size() > 6) {
    for (size_t i = 6; i < parts.size(); ++i) {
      if (!Trim(parts[i]).empty()) {
        return emitError("E03", "BAD_PARAM");
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
      return emitError("E10",
                       "THERMAL_REQ_GT_MAX",
                       {
                           {"id", std::to_string(static_cast<int>(first_id))},
                           {"req_ms", std::to_string(req_ms_total)},
                           {"max_budget_s", std::to_string(static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S))}
                       });
    } else {
      if (!context.controller().homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms)) {
        return emitError("E04", "BUSY");
      }
      transport::response::CompletionTracker::Instance().RegisterOperation(msg_id, "HOME", mask, context.controller());
      CommandResult res;
      appendLine(res, transport::command::MakeWarnLine(
          msg_id,
          "THERMAL_REQ_GT_MAX",
          "",
          {
              {"id", std::to_string(static_cast<int>(first_id))},
              {"req_ms", std::to_string(req_ms_total)},
              {"max_budget_s", std::to_string(static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S))}
          }));
      appendLine(res, transport::command::MakeAckLine(
          msg_id,
          {{"est_ms", std::to_string(req_ms_total)}}));
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
        return emitError("E11",
                         "THERMAL_NO_BUDGET",
                         {
                             {"id", std::to_string(static_cast<int>(id))},
                             {"req_ms", std::to_string(req_ms_total)},
                             {"budget_s", std::to_string(avail_s)},
                             {"ttfc_s", std::to_string(ttfc_s)}
                         });
      } else {
        if (!context.controller().homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms)) {
          return emitError("E04", "BUSY");
        }
        transport::response::CompletionTracker::Instance().RegisterOperation(msg_id, "HOME", mask, context.controller());
        CommandResult res;
        appendLine(res, transport::command::MakeWarnLine(
            msg_id,
            "THERMAL_NO_BUDGET",
            "",
            {
                {"id", std::to_string(static_cast<int>(id))},
                {"req_ms", std::to_string(req_ms_total)},
                {"budget_s", std::to_string(avail_s)},
                {"ttfc_s", std::to_string(ttfc_s)}
            }));
        appendLine(res, transport::command::MakeAckLine(
            msg_id,
            {{"est_ms", std::to_string(req_ms_total)}}));
        return res;
      }
    }
  }

  if (!context.controller().homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms)) {
    return emitError("E04", "BUSY");
  }
  transport::response::CompletionTracker::Instance().RegisterOperation(msg_id, "HOME", mask, context.controller());
  return emitAck({{"est_ms", std::to_string(req_ms_total)}});
}

// ---------------- QueryCommandHandler ----------------

bool QueryCommandHandler::canHandle(const std::string &action) const {
  return action == "HELP" || action == "STATUS" || action == "ST" ||
         action == "GET" || action == "SET";
}

CommandResult QueryCommandHandler::execute(const ParsedCommand &command,
                                           CommandExecutionContext &context,
                                           uint32_t now_ms) {
  (void)now_ms;
  if (command.action == "HELP") {
    return handleHelp();
  }
  if (command.action == "STATUS" || command.action == "ST") {
    context.controller().tick(now_ms);
    return handleStatus(context);
  }
  if (command.action == "GET") {
    context.controller().tick(now_ms);
    return handleGet(command.args, context);
  }
  if (command.action == "SET") {
    context.controller().tick(now_ms);
    return handleSet(command.args, context);
  }
  auto err_line = transport::command::MakeErrorLine(
      context.nextMsgId(),
      "E01",
      "BAD_CMD",
      {});
  return MakeResultWithLine(command.action.c_str(), err_line);
}

CommandResult QueryCommandHandler::handleHelp() const {
  constexpr const char *kAction = "HELP";
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
  for (const auto &line : Split(os.str(), '\n')) {
    transport::command::ResponseLine info_line;
    info_line.type = transport::command::ResponseLineType::kInfo;
    info_line.raw = line;
    EmitResponseEvent(kAction, info_line);
    res.append(info_line);
  }
  return res;
}

CommandResult QueryCommandHandler::handleStatus(CommandExecutionContext &context) {
  constexpr const char *kAction = "STATUS";
  std::string msg_id = context.nextMsgId();
  CommandResult res;

  auto ack_line = transport::command::MakeAckLine(msg_id, {});
  EmitResponseEvent(kAction, ack_line);
  res.append(ack_line);

  for (size_t i = 0; i < context.controller().motorCount(); ++i) {
    const MotorState &s = context.controller().state(i);
    int32_t missing_t = MotorControlConstants::BUDGET_TENTHS_MAX - s.budget_tenths;
    if (missing_t < 0) {
      missing_t = 0;
    }
    int32_t ttfc_tenths = (missing_t <= 0)
                              ? 0
                              : static_cast<int32_t>(
                                    ((static_cast<int64_t>(missing_t) * 10 +
                                      MotorControlConstants::REFILL_TENTHS_PER_SEC - 1) /
                                     MotorControlConstants::REFILL_TENTHS_PER_SEC));
    const int32_t kTtfcMaxTenths = MotorControlConstants::MAX_COOL_DOWN_TIME_S * 10;
    if (ttfc_tenths > kTtfcMaxTenths) {
      ttfc_tenths = kTtfcMaxTenths;
    }

    transport::command::ResponseLine data_line;
    data_line.type = transport::command::ResponseLineType::kData;
    data_line.fields.push_back({"id", std::to_string(static_cast<int>(s.id))});
    data_line.fields.push_back({"pos", std::to_string(s.position)});
    data_line.fields.push_back({"moving", BoolToFlag(s.moving)});
    data_line.fields.push_back({"awake", BoolToFlag(s.awake)});
    data_line.fields.push_back({"homed", BoolToFlag(s.homed)});
    data_line.fields.push_back({"steps_since_home", std::to_string(s.steps_since_home)});
    data_line.fields.push_back({"budget_s", FormatSignedTenths(s.budget_tenths)});
    data_line.fields.push_back({"ttfc_s", FormatTenths(ttfc_tenths)});
    data_line.fields.push_back({"speed", std::to_string(s.speed)});
    data_line.fields.push_back({"accel", std::to_string(s.accel)});
    data_line.fields.push_back({"est_ms", std::to_string(s.last_op_est_ms)});
    data_line.fields.push_back({"started_ms", std::to_string(s.last_op_started_ms)});
    if (!s.last_op_ongoing) {
      data_line.fields.push_back({"actual_ms", std::to_string(s.last_op_last_ms)});
    }

    EmitResponseEvent(kAction, data_line);
    res.append(data_line);
  }
  return res;
}

CommandResult QueryCommandHandler::handleGet(const std::string &args, CommandExecutionContext &context) {
  constexpr const char *kAction = "GET";
  std::string msg_id = context.nextMsgId();
  std::string key = ToUpperCopy(Trim(args));
  if (key.empty() || key == "ALL") {
    return MakeDoneResult(kAction,
                          msg_id,
                          {{"SPEED", std::to_string(context.defaultSpeed())},
                           {"ACCEL", std::to_string(context.defaultAccel())},
                           {"DECEL", std::to_string(context.defaultDecel())},
                           {"THERMAL_LIMITING", context.thermalLimitsEnabled() ? "ON" : "OFF"},
                           {"max_budget_s",
                            std::to_string(static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S))}});
  }
  if (key == "SPEED") {
    return MakeDoneResult(kAction, msg_id, {{"SPEED", std::to_string(context.defaultSpeed())}});
  }
  if (key == "ACCEL") {
    return MakeDoneResult(kAction, msg_id, {{"ACCEL", std::to_string(context.defaultAccel())}});
  }
  if (key == "DECEL") {
    return MakeDoneResult(kAction, msg_id, {{"DECEL", std::to_string(context.defaultDecel())}});
  }
  if (key == "THERMAL_LIMITING") {
    return MakeDoneResult(kAction,
                          msg_id,
                          {{"THERMAL_LIMITING", context.thermalLimitsEnabled() ? "ON" : "OFF"},
                           {"max_budget_s", std::to_string(static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S))}});
  }
  if (key.rfind("LAST_OP_TIMING", 0) == 0) {
    std::string rest;
    size_t p = key.find(':');
    if (p != std::string::npos) rest = Trim(key.substr(p + 1));
    if (rest.empty() || rest == "ALL") {
      CommandResult res;
      std::string list_cid = context.nextMsgId();
      auto ack_line = transport::command::MakeAckLine(list_cid, {});
      EmitResponseEvent(kAction, ack_line);
      res.append(ack_line);

      transport::command::ResponseLine section_line;
      section_line.type = transport::command::ResponseLineType::kInfo;
      section_line.msg_id = list_cid;
      section_line.code = "LAST_OP_TIMING";
      section_line.raw = "LAST_OP_TIMING";
      EmitResponseEvent(kAction, section_line);
      res.append(section_line);

      for (uint8_t i = 0; i < context.controller().motorCount(); ++i) {
        const MotorState &s = context.controller().state(i);
        transport::command::ResponseLine data_line;
        data_line.type = transport::command::ResponseLineType::kData;
        data_line.fields.push_back({"id", std::to_string(static_cast<int>(i))});
        data_line.fields.push_back({"ongoing", BoolToFlag(s.last_op_ongoing)});
        data_line.fields.push_back({"est_ms", std::to_string(s.last_op_est_ms)});
        data_line.fields.push_back({"started_ms", std::to_string(s.last_op_started_ms)});
        if (!s.last_op_ongoing) {
          data_line.fields.push_back({"actual_ms", std::to_string(s.last_op_last_ms)});
        }
        EmitResponseEvent(kAction, data_line);
        res.append(data_line);
      }
      auto done = MakeDoneResult(kAction, list_cid);
      res.mergeFrom(done);
      return res;
    }
    uint32_t mask;
    if (!ParseIdMask(rest, mask, context.controller().motorCount())) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "E02", "BAD_ID", {});
      return MakeResultWithLine(kAction, err_line);
    }
    uint8_t id = 0;
    for (; id < context.controller().motorCount(); ++id) if (mask & (1u << id)) break;
    if (id >= context.controller().motorCount()) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "E02", "BAD_ID", {});
      return MakeResultWithLine(kAction, err_line);
    }
    const MotorState &s = context.controller().state(id);
    std::vector<transport::command::Field> fields = {
        {"LAST_OP_TIMING", "1"},
        {"ongoing", BoolToFlag(s.last_op_ongoing)},
        {"id", std::to_string(static_cast<int>(id))},
        {"est_ms", std::to_string(s.last_op_est_ms)},
        {"started_ms", std::to_string(s.last_op_started_ms)}};
    if (!s.last_op_ongoing) {
      fields.push_back({"actual_ms", std::to_string(s.last_op_last_ms)});
    }
    return MakeDoneResult(kAction, msg_id, fields);
  }
  auto err_line = transport::command::MakeErrorLine(msg_id, "E03", "BAD_PARAM", {});
  return MakeResultWithLine(kAction, err_line);
}

CommandResult QueryCommandHandler::handleSet(const std::string &args, CommandExecutionContext &context) {
  constexpr const char *kAction = "SET";
  std::string msg_id = context.nextMsgId();
  std::string payload = Trim(args);
  std::string up = ToUpperCopy(payload);
  size_t eq = up.find('=');
  if (eq == std::string::npos) {
    auto err_line = transport::command::MakeErrorLine(msg_id, "E03", "BAD_PARAM", {});
    return MakeResultWithLine(kAction, err_line);
  }
  std::string key = Trim(up.substr(0, eq));
  std::string val = Trim(up.substr(eq + 1));
  if (key == "THERMAL_LIMITING") {
    if (val == "ON") {
      context.setThermalLimitsEnabled(true);
      return MakeDoneResult(kAction, msg_id);
    } else if (val == "OFF") {
      context.setThermalLimitsEnabled(false);
      return MakeDoneResult(kAction, msg_id);
    }
    auto err_line = transport::command::MakeErrorLine(msg_id, "E03", "BAD_PARAM", {});
    return MakeResultWithLine(kAction, err_line);
  }
  if (key == "SPEED") {
    long v;
    if (!ParseInt(val, v) || v <= 0) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "E03", "BAD_PARAM", {});
      return MakeResultWithLine(kAction, err_line);
    }
    for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
      if (context.controller().state(id).moving) {
        auto err_line = transport::command::MakeErrorLine(msg_id, "E04", "BUSY", {});
        return MakeResultWithLine(kAction, err_line);
      }
    }
    context.defaultSpeed() = static_cast<int>(v);
    return MakeDoneResult(kAction, msg_id);
  }
  if (key == "ACCEL") {
    long v;
    if (!ParseInt(val, v) || v <= 0) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "E03", "BAD_PARAM", {});
      return MakeResultWithLine(kAction, err_line);
    }
    for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
      if (context.controller().state(id).moving) {
        auto err_line = transport::command::MakeErrorLine(msg_id, "E04", "BUSY", {});
        return MakeResultWithLine(kAction, err_line);
      }
    }
    context.defaultAccel() = static_cast<int>(v);
    return MakeDoneResult(kAction, msg_id);
  }
  if (key == "DECEL") {
    long v;
    if (!ParseInt(val, v) || v < 0) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "E03", "BAD_PARAM", {});
      return MakeResultWithLine(kAction, err_line);
    }
    for (uint8_t id = 0; id < context.controller().motorCount(); ++id) {
      if (context.controller().state(id).moving) {
        auto err_line = transport::command::MakeErrorLine(msg_id, "E04", "BUSY", {});
        return MakeResultWithLine(kAction, err_line);
      }
    }
    context.defaultDecel() = static_cast<int>(v);
    context.controller().setDeceleration(context.defaultDecel());
    return MakeDoneResult(kAction, msg_id);
  }
  auto err_line = transport::command::MakeErrorLine(msg_id, "E03", "BAD_PARAM", {});
  return MakeResultWithLine(kAction, err_line);
}

// ---------------- NetCommandHandler ----------------

bool NetCommandHandler::canHandle(const std::string &action) const {
  return action == "NET";
}

CommandResult NetCommandHandler::execute(const ParsedCommand &command,
                                         CommandExecutionContext &context,
                                         uint32_t /*now_ms*/) {
  using net_onboarding::State;
  constexpr const char *kAction = "NET";

  std::string a = Trim(command.args);
  std::string up = ToUpperCopy(a);
  std::string sub = up;
  size_t comma = up.find(',');
  if (comma != std::string::npos) sub = Trim(up.substr(0, comma));

  auto sub_field = [](const char *value) -> transport::command::Field {
    return {"sub_action", value};
  };

  if (sub == "RESET") {
    auto before = context.net().status().state;
    std::string msg_id = context.nextMsgId();
    if (before == State::CONNECTING) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "NET_BUSY_CONNECTING", "", {sub_field("RESET")});
      return MakeResultWithLine(kAction, err_line);
    }
    context.setActiveMsgId(msg_id);
    context.net().resetCredentials();
    auto after = context.net().status();
    if (before == State::AP_ACTIVE) {
      context.clearActiveMsgId();
      CommandResult res;
      std::string ssid = std::string(after.ssid.data());
      std::string ip = std::string(after.ip.data());
      transport::command::ResponseLine info_line;
      info_line.type = transport::command::ResponseLineType::kInfo;
      info_line.msg_id = msg_id;
      info_line.code = "NET:AP_ACTIVE";
      info_line.fields.push_back(sub_field("RESET"));
      info_line.fields.push_back({"state", "AP_ACTIVE"});
      info_line.fields.push_back({"ssid", QuoteString(ssid)});
      info_line.fields.push_back({"ip", ip});
      info_line.raw = "CTRL: NET:AP_ACTIVE msg_id=" + msg_id +
                      " ssid=" + QuoteString(ssid) +
                      " ip=" + ip;
      EmitResponseEvent(kAction, info_line);
      res.append(info_line);
      return AppendDoneResult(std::move(res),
                              kAction,
                              msg_id,
                              {sub_field("RESET"),
                               {"state", "AP_ACTIVE"},
                               {"ssid", QuoteString(ssid)},
                               {"ip", ip}});
    }
    auto ack_line = transport::command::MakeAckLine(msg_id,
                                                    {sub_field("RESET"),
                                                     {"state", NetStateToString(before)}});
    return MakeResultWithLine(kAction, ack_line);
  }

  if (sub == "STATUS") {
    auto s = context.net().status();
    std::string msg_id = context.nextMsgId();
    std::vector<transport::command::Field> fields = {
        sub_field("STATUS"),
        {"state", NetStateToString(s.state)},
        {"rssi", (s.state == State::CONNECTED) ? std::to_string(s.rssi_dbm) : "NA"},
        {"ip", s.ip.data()},
        {"ssid", QuoteString(std::string(s.ssid.data()))}};
    if (s.state == State::AP_ACTIVE) {
      std::array<char,65> pass;
      context.net().apPassword(pass);
      fields.push_back({"pass", QuoteString(std::string(pass.data()))});
    } else {
      fields.push_back({"pass", QuoteString("********")});
    }
    return MakeDoneResult(kAction, msg_id, fields);
  }

  if (sub == "SET") {
    std::string msg_id = context.nextMsgId();
    if (context.net().status().state == State::CONNECTING) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "NET_BUSY_CONNECTING", "", {sub_field("SET")});
      return MakeResultWithLine(kAction, err_line);
    }
    auto toks = ParseCsvQuoted(a);
    if (toks.size() != 3) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "NET_BAD_PARAM", "", {sub_field("SET")});
      return MakeResultWithLine(kAction, err_line);
    }
    const std::string &ssid = toks[1];
    const std::string &pass = toks[2];
    if (ssid.empty() || ssid.size() > 32) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "NET_BAD_PARAM", "", {sub_field("SET")});
      return MakeResultWithLine(kAction, err_line);
    }
    if (!(pass.size() == 0 || (pass.size() >= 8 && pass.size() <= 63))) {
      if (pass.size() > 0 && pass.size() < 8) {
        auto err_line = transport::command::MakeErrorLine(msg_id,
                                                          "NET_BAD_PARAM",
                                                          "PASS_TOO_SHORT",
                                                          {sub_field("SET")});
        return MakeResultWithLine(kAction, err_line);
      }
      auto err_line = transport::command::MakeErrorLine(msg_id, "NET_BAD_PARAM", "", {sub_field("SET")});
      return MakeResultWithLine(kAction, err_line);
    }
    context.setActiveMsgId(msg_id);
    bool ok = context.net().setCredentials(ssid.c_str(), pass.c_str());
    if (!ok) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "NET_SAVE_FAILED", "", {sub_field("SET")});
      return MakeResultWithLine(kAction, err_line);
    }
    auto ack_line = transport::command::MakeAckLine(msg_id, {sub_field("SET")});
    return MakeResultWithLine(kAction, ack_line);
  }

  if (sub == "LIST") {
    std::string msg_id = context.nextMsgId();
    if (context.net().status().state != State::AP_ACTIVE) {
      auto err_line = transport::command::MakeErrorLine(msg_id, "NET_SCAN_AP_ONLY", "", {sub_field("LIST")});
      return MakeResultWithLine(kAction, err_line);
    }
    CommandResult res;
    auto append_line = [&](transport::command::ResponseLine line) {
      EmitResponseEvent(kAction, line);
      res.append(line);
    };
    append_line(transport::command::MakeAckLine(msg_id,
                                                {sub_field("LIST"),
                                                 {"scanning", "1"}}));
    std::vector<net_onboarding::WifiScanResult> nets;
    int n = context.net().scanNetworks(nets, 12, true);
    if (n < 0) {
      n = 0;
    }
    transport::command::ResponseLine header;
    header.type = transport::command::ResponseLineType::kInfo;
    header.msg_id = msg_id;
    header.code = "NET:LIST";
    header.fields.push_back(sub_field("LIST"));
    header.fields.push_back({"count", std::to_string(n)});
    header.raw = "NET:LIST msg_id=" + msg_id;
    append_line(header);

    for (int i = 0; i < n; ++i) {
      const auto &r = nets[static_cast<size_t>(i)];
      transport::command::ResponseLine data_line;
      data_line.type = transport::command::ResponseLineType::kData;
      data_line.fields.push_back({"SSID", QuoteString(r.ssid)});
      data_line.fields.push_back({"rssi", std::to_string(r.rssi)});
      data_line.fields.push_back({"secure", r.secure ? "1" : "0"});
      data_line.fields.push_back({"channel", std::to_string(r.channel)});
      std::ostringstream raw;
      raw << "SSID=" << QuoteString(r.ssid)
          << " rssi=" << r.rssi
          << " secure=" << (r.secure ? 1 : 0)
          << " channel=" << r.channel;
      data_line.raw = raw.str();
      append_line(data_line);
    }
    return AppendDoneResult(std::move(res),
                            kAction,
                            msg_id,
                            {sub_field("LIST"),
                             {"count", std::to_string(n)}});
  }

  std::string msg_id = context.nextMsgId();
  auto err_line = transport::command::MakeErrorLine(msg_id, "E03", "BAD_PARAM",
                                                    {{"requested", sub}});
  return MakeResultWithLine(kAction, err_line);
}

} // namespace command
} // namespace motor
