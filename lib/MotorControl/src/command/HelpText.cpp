#include "MotorControl/command/HelpText.h"

#include <sstream>

#include "MotorControl/BuildConfig.h"

namespace motor {
namespace command {

const std::string &HelpText() {
  static const std::string text = [] {
    std::ostringstream os;
    os << "HELP\n";
    os << "MOVE:<id|ALL>,<abs_steps>\n";
    os << "HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]\n";
    os << "NET:RESET\n";
    os << "NET:STATUS\n";
    os << "NET:SET,\"<ssid>\",\"<pass>\" (quote to allow commas/spaces; escape \\\" and \\\\)\n";
    os << "NET:LIST (scan nearby SSIDs; AP mode only)\n";
    os << "MQTT:GET_CONFIG\n";
    os << "MQTT:SET_CONFIG host=<host> port=<port> user=<user> pass=\\\"<pass>\\\"\n";
    os << "MQTT:SET_CONFIG RESET\n";
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
    return os.str();
  }();
  return text;
}

} // namespace command
} // namespace motor
