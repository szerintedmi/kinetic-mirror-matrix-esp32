import unittest

from tools.serial_cli.command_builder import (
    CommandParseError,
    UnsupportedCommandError,
    build_requests,
    split_batches,
)


class CommandBuilderTests(unittest.TestCase):
    def test_move_command(self):
        reqs = build_requests("MOVE:1,900")
        self.assertEqual(len(reqs), 1)
        req = reqs[0]
        self.assertEqual(req.action, "MOVE")
        self.assertEqual(req.params["target_ids"], 1)
        self.assertEqual(req.params["position_steps"], 900)

    def test_home_with_optionals(self):
        req = build_requests("HOME:ALL,800,150,3000,12000,2400")[0]
        self.assertEqual(req.action, "HOME")
        self.assertEqual(req.params["target_ids"], "ALL")
        self.assertEqual(req.params["overshoot_steps"], 800)
        self.assertEqual(req.params["backoff_steps"], 150)
        self.assertEqual(req.params["speed_sps"], 3000)
        self.assertEqual(req.params["accel_sps2"], 12000)
        self.assertEqual(req.params["full_range_steps"], 2400)

    def test_net_set_quotes(self):
        req = build_requests('NET:SET,"MyNet","pass"')[0]
        self.assertEqual(req.action, "NET:SET")
        self.assertEqual(req.params["ssid"], "MyNet")
        self.assertEqual(req.params["pass"], "pass")

    def test_split_batches(self):
        parts = split_batches('MOVE:0,100;MOVE:1,200;SET SPEED=4000')
        self.assertEqual(parts, ['MOVE:0,100', 'MOVE:1,200', 'SET SPEED=4000'])

    def test_unsupported_command(self):
        with self.assertRaises(UnsupportedCommandError):
            build_requests("STATUS")

    def test_bad_syntax(self):
        with self.assertRaises(CommandParseError):
            build_requests("MOVE:abc")


if __name__ == "__main__":
    unittest.main()
