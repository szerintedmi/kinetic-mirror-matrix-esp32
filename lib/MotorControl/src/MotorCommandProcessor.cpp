#include "MotorControl/MotorCommandProcessor.h"
#include "StubMotorController.h"
#if defined(USE_HARDWARE_BACKEND) && !defined(UNIT_TEST)
#include "MotorControl/HardwareMotorController.h"
#endif
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

static const long kMinPos = -1200;
static const long kMaxPos = 1200;
static const int kDefaultSpeed = 4000;
static const int kDefaultAccel = 16000;
static const long kDefaultOvershoot = 800;
static const long kDefaultBackoff = 50;

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
  os << "WAKE:<id|ALL>\n";
  os << "SLEEP:<id|ALL>";
  return os.str();
}

std::string MotorCommandProcessor::handleSTATUS()
{
  std::ostringstream os;
  for (size_t i = 0; i < controller_->motorCount(); ++i)
  {
    const MotorState &s = controller_->state(i);
    os << "id=" << (int)s.id
       << " pos=" << s.position
       << " speed=" << s.speed
       << " accel=" << s.accel
       << " moving=" << (s.moving ? 1 : 0)
       << " awake=" << (s.awake ? 1 : 0);
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
  if (!controller_->moveAbsMask(mask, target, speed, accel, now_ms))
    return "CTRL:ERR E04 BUSY";
  return "CTRL:OK";
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
  if (!controller_->homeMask(mask, overshoot, backoff, speed, accel, full_range, now_ms))
    return "CTRL:ERR E04 BUSY";
  return "CTRL:OK";
}

std::string MotorCommandProcessor::processLine(const std::string &line, uint32_t now_ms)
{
  std::string s = trim(line);
  if (s.empty())
    return "";
  auto p = s.find(':');
  std::string verb = s;
  std::string args;
  if (p != std::string::npos)
  {
    verb = s.substr(0, p);
    args = s.substr(p + 1);
  }
  std::transform(verb.begin(), verb.end(), verb.begin(), ::toupper);
  if (verb == "HELP")
    return handleHELP();
  if (verb == "STATUS")
    return handleSTATUS();
  if (verb == "WAKE")
    return handleWAKE(args);
  if (verb == "SLEEP")
    return handleSLEEP(args);
  if (verb == "MOVE")
    return handleMOVE(args, now_ms);
  if (verb == "HOME")
    return handleHOME(args, now_ms);
  return "CTRL:ERR E01 BAD_CMD";
}
