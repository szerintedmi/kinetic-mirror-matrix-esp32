#include "MotorControl/command/HelpText.h"

#include "MotorControl/BuildConfig.h"

#include <sstream>

namespace motor {
namespace command {

const std::string& HelpText() {
  static const std::string text = [] {
    std::ostringstream os;
    os << "HELP\n";
#if !(USE_SHARED_STEP)
    os << "MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>][,<overshoot>][,<dither_amp>][,<dither_cycles>]\n";
    os << "HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]\n";
#else
    os << "MOVE:<id|ALL>,<abs_steps>\n";
    os << "HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]\n";
#endif
    os << "NET:RESET (clear all networks, enter AP mode)\n";
    os << "NET:STATUS\n";
    os << "NET:GET_CONFIG (show configured primary/secondary SSIDs)\n";
    os << "NET:SET,\"<ssid>\",\"<pass>\" (set primary network, same as SET_PRIMARY)\n";
    os << "NET:SET_PRIMARY,\"<ssid>\",\"<pass>\" (set primary/home network)\n";
    os << "NET:SET_SECONDARY,\"<ssid>\",\"<pass>\" (set secondary/backup network)\n";
    os << "NET:CLEAR_SECONDARY (remove secondary network only)\n";
    os << "NET:LIST (scan nearby SSIDs; AP mode only)\n";
    os << "MQTT:GET_CONFIG\n";
    os << "MQTT:SET_CONFIG host=<host> port=<port> user=<user> pass=\\\"<pass>\\\"\n";
    os << "MQTT:SET_CONFIG RESET\n";
    os << "STATUS\n";
    os << "GET\n";
    os << "GET ALL\n";
    os << "GET LAST_OP_TIMING[:<id|ALL>]\n";
    os << "GET SPEED\n";
    os << "GET ACCEL\n";
    os << "GET DECEL\n";
    os << "GET THERMAL_LIMITING\n";
    os << "SET THERMAL_LIMITING=OFF|ON\n";
    os << "SET THERMAL_BUDGET:<id>=<tenths> (debug: set budget in 0.1s units, e.g. -60=-6s)\n";
    os << "SET SPEED=<steps_per_second>\n";
    os << "SET ACCEL=<steps_per_second^2>\n";
    os << "SET DECEL=<steps_per_second^2>\n";
    os << "GET MOVE_OVERSHOOT\n";
    os << "GET DITHER_AMPLITUDE\n";
    os << "GET DITHER_CYCLES\n";
    os << "GET DITHER_MIN_AMPLITUDE\n";
    os << "SET MOVE_OVERSHOOT=<steps> (sign=approach dir, 0=off, default=80)\n";
    os << "SET DITHER_AMPLITUDE=<steps> (0=disabled)\n";
    os << "SET DITHER_CYCLES=<count>\n";
    os << "SET DITHER_MIN_AMPLITUDE=<steps>\n";
    os << "GET MICROSTEP\n";
    os << "SET MICROSTEP=FULL|HALF|1/4|1/8|1/16|1/32 (all motors must be asleep)\n";
    os << "WAKE:<id|ALL>\n";
    os << "SLEEP:<id|ALL>\n";
    os << "Shortcuts: M=MOVE, H=HOME, ST=STATUS\n";
    os << "Multicommand: <cmd1>;<cmd2> note: no cmd queuing; only distinct motors allowed";
    return os.str();
  }();
  return text;
}

}  // namespace command
}  // namespace motor
