from __future__ import annotations

import io
import os
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lib import runner


class GateRunnerResourceTest(unittest.TestCase):
	def test_local_default_respects_six_job_budget(self):
		with mock.patch.dict(os.environ, {}, clear=True), mock.patch.object(os, "cpu_count", return_value=32):
			self.assertEqual(runner.resolve_parallel_jobs(), "6")

	def test_small_machine_does_not_exceed_available_cpus(self):
		with mock.patch.dict(os.environ, {}, clear=True), mock.patch.object(os, "cpu_count", return_value=2):
			self.assertEqual(runner.resolve_parallel_jobs(), "2")

	def test_ci_uses_available_cpus(self):
		with mock.patch.dict(os.environ, {"CI": "true"}, clear=True), mock.patch.object(os, "cpu_count", return_value=16):
			self.assertEqual(runner.resolve_parallel_jobs(), "16")

	def test_explicit_share_overrides_local_or_ci_default(self):
		with mock.patch.dict(os.environ, {"CI": "true", "QM_GATE_BUILD_JOBS": "3"}, clear=True), mock.patch.object(os, "cpu_count", return_value=16):
			self.assertEqual(runner.resolve_parallel_jobs(), "3")

	def test_invalid_share_is_rejected(self):
		for value in ("", "0", "-1", "abc"):
			with self.subTest(value=value), mock.patch.dict(os.environ, {"QM_GATE_BUILD_JOBS": value}, clear=True):
				with self.assertRaisesRegex(ValueError, "QM_GATE_BUILD_JOBS"):
					runner.resolve_parallel_jobs()


class GateRunnerConsoleTest(unittest.TestCase):
	def test_gbk_console_keeps_command_result_with_unencodable_output(self):
		for returncode in (0, 7):
			with self.subTest(returncode=returncode):
				raw = io.BytesIO()
				console = io.TextIOWrapper(raw, encoding="gbk", errors="strict")
				with mock.patch.object(sys, "stdout", console):
					code, output = runner.run([
						sys.executable,
						"-c",
						f"import sys; sys.stdout.buffer.write(b'diagnostic: ' + bytes([255,10])); sys.exit({returncode})",
					])
					console.flush()
				self.assertEqual(code, returncode)
				self.assertIn("diagnostic: \ufffd", output)
				self.assertIn(b"diagnostic: ?", raw.getvalue())

	def test_unicode_string_console_preserves_full_output(self):
		console = io.StringIO()
		with mock.patch.object(sys, "stdout", console):
			code, output = runner.run([
				sys.executable,
				"-c",
				"import sys; sys.stdout.buffer.write(b'diagnostic: ' + bytes([255,10]))",
			])
		self.assertEqual(code, 0)
		self.assertEqual(output, "diagnostic: \ufffd")
		self.assertIn("diagnostic: \ufffd", console.getvalue())


if __name__ == "__main__":
	unittest.main()
