"""Standard-library smoke tests; run with python -m unittest discover -s Scripts/tests."""
import importlib.util
import io
import pathlib
import unittest

MODULE_PATH = pathlib.Path(__file__).resolve().parents[1] / 'AnalyzeGravityMotion.py'
spec = importlib.util.spec_from_file_location('orbit_motion_analysis', MODULE_PATH)
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


class CaptureTests(unittest.TestCase):
    def test_per_body_sleep_and_velocity(self):
        csv_source = io.StringIO(
            'body,t_sec,velocity_x,velocity_y,velocity_z,awake,experimental_chaos_multi_point_gravity\n'
            'Front,0,3,4,0,1,1\n'
            'Front,10,0,0,0,0,1\n'
            'Back,0,0,5,0,1,0\n'
        )
        result = analysis.parse_capture(csv_source, 'synthetic')
        self.assertEqual(result['rows'], 3)
        self.assertEqual(len(result['bodies']), 2)
        self.assertFalse(result['bodies']['Front']['last_awake'])
        self.assertAlmostEqual(result['bodies']['Back']['last_speed'], 5)
        self.assertEqual(result['flags']['experimental_chaos_multi_point_gravity'], 2)

    def test_rejects_missing_columns(self):
        with self.assertRaises(ValueError):
            analysis.parse_capture(io.StringIO('body,t_sec\nFront,1\n'), 'invalid')

    def test_flags_do_not_overlap(self):
        source = io.StringIO(
            'body,t_sec,velocity_x,velocity_y,velocity_z,awake,'
            'experimental_chaos_multi_point_gravity,experimental_chaos_mixed_point_directional_gravity\n'
            'Front,1,0,0,0,1,1,1\n'
        )
        result = analysis.parse_capture(source, 'invalid flags')
        self.assertEqual(result['invalid_flags'], 1)


if __name__ == '__main__':
    unittest.main()
