from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import trace_correlate as tc


class CorrelationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.session = self.root / "test.sr"
        self.log = self.root / "test.csv"
        self.records = "100,2,IN\n110,2,OUT\n114,1,IN\n120,1,OUT\n"
        self.log.write_text(self.records)
        self.data = bytearray(40)
        self.data[5:15] = bytes([4]) * 10
        self.data[19:25] = bytes([2]) * 6
        self.write_session()

    def write_session(self, unitsize=1, broken_chunks=False):
        metadata = ("[device 1]\ncapturefile=logic-1\nsamplerate=1 MHz\n"
                    f"unitsize={unitsize}\ntotal probes=8\n")
        with zipfile.ZipFile(self.session, "w") as archive:
            archive.writestr("version", "2")
            archive.writestr("metadata", metadata)
            archive.writestr("logic-1-3" if broken_chunks else "logic-1-2", self.data[20:])
            archive.writestr("logic-1-1", self.data[:20])

    def test_chunks_and_known_edges(self):
        rate, samples, edges = tc.read_session(self.session)
        self.assertEqual((rate, samples), (1e6, 40))
        self.assertEqual(edges, [(5, 2, "IN"), (15, 2, "OUT"),
                                 (19, 1, "IN"), (25, 1, "OUT")])
        result = tc.analyze(self.session, self.log, freq_hz=1e6, tolerance_us=0)
        self.assertTrue(result["pass"])
        self.assertEqual(result["matches"][0]["first_edge"]["max_abs_us"], 0)
        self.assertEqual(result["switches"]["min"], 4)

    def test_capture_boundaries_and_identical_data(self):
        block = "# W1E2 SCHEDULER TRACE\n" + self.records + "# CAPTURE DONE\n"
        self.log.write_text(block * 2)
        captures = tc.read_log(self.log)
        self.assertEqual(len(captures), 2)
        self.assertEqual(captures[0], captures[1])
        with self.assertRaisesRegex(ValueError, "no complete GPIO"):
            tc.analyze(self.session, self.log, freq_hz=1e6)

    def test_64bit_cycles_and_raw_wrap_rejection(self):
        self.log.write_text("4294967290,2,IN\n4294967301,2,OUT\n")
        self.assertEqual(tc.read_log(self.log)[0][1][0], 4294967301)
        self.log.write_text("4294967290,2,IN\n5,2,OUT\n")
        with self.assertRaisesRegex(ValueError, "increase"):
            tc.read_log(self.log)

    def test_wrong_task_or_pair_is_rejected(self):
        self.log.write_text(self.records.replace("110,2,OUT", "110,1,OUT"))
        with self.assertRaisesRegex(ValueError, "tags differ"):
            tc.read_log(self.log)
        self.log.write_text(self.records.replace("120,1,OUT\n", ""))
        with self.assertRaisesRegex(ValueError, "complete"):
            tc.read_log(self.log)

    def test_overflow_and_bad_csv_are_rejected(self):
        self.log.write_text("# overflow=1\n" + self.records)
        with self.assertRaisesRegex(ValueError, "overflow"):
            tc.read_log(self.log)
        self.log.write_text("garbage\n")
        with self.assertRaises(ValueError):
            tc.read_log(self.log)

    def test_gpio_mismatch_is_not_silently_paired(self):
        self.data[19:25] = bytes([1]) * 6
        self.write_session()
        with self.assertRaisesRegex(ValueError, "no complete GPIO"):
            tc.analyze(self.session, self.log, freq_hz=1e6)

    def test_offset_fit_does_not_hide_failed_first_edge_bound(self):
        self.log.write_text(self.records.replace("120,1,OUT", "122,1,OUT"))
        result = tc.analyze(self.session, self.log, freq_hz=1e6, tolerance_us=1)
        self.assertFalse(result["pass"])
        match = result["matches"][0]
        self.assertAlmostEqual(match["first_edge"]["max_abs_us"], 2)
        self.assertLess(match["mean_all"]["max_abs_us"], 2)

    def test_one_interval_has_no_context_switch(self):
        self.data[19:25] = bytes([0]) * 6
        self.write_session()
        self.log.write_text("100,2,IN\n110,2,OUT\n")
        result = tc.analyze(self.session, self.log, freq_hz=1e6)
        self.assertTrue(result["pass"])
        self.assertEqual(result["switches"]["n"], 0)
        self.assertIsNone(result["switches"]["min"])

    def test_bad_session_and_parameters(self):
        self.write_session(unitsize=2)
        with self.assertRaisesRegex(ValueError, "one-byte"):
            tc.read_session(self.session)
        self.write_session(broken_chunks=True)
        with self.assertRaisesRegex(ValueError, "missing"):
            tc.read_session(self.session)
        for frequency in (0, -1, float("nan")):
            with self.assertRaises(ValueError):
                tc.analyze(self.session, self.log, freq_hz=frequency)
        with self.assertRaises(ValueError):
            tc.read_session(self.session, channels=(0, 0))


if __name__ == "__main__":
    unittest.main()
