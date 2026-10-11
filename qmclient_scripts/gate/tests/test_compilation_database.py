"""当前平台编译数据库过滤的行为回归。"""

import json
import os
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lib.compilation_database import compilation_units
from checks import strict_build
from lib.report import ResultCollector


class CompilationDatabaseTests(unittest.TestCase):
	def test_only_registered_platform_units_are_selected(self):
		with TemporaryDirectory() as temporary:
			root = Path(temporary)
			database = root / "compile_commands.json"
			windows = root / "src/windows.cpp"
			android = root / "src/android.cpp"
			database.write_text(json.dumps([{"directory": str(root), "file": str(windows)}]), encoding="utf-8")
			units = compilation_units(database)
			self.assertIn(windows.resolve(), units)
			self.assertNotIn(android.resolve(), units)

	def test_relative_files_resolve_against_each_entry_directory(self):
		with TemporaryDirectory() as temporary:
			root = Path(temporary)
			database = root / "compile_commands.json"
			database.write_text(
				json.dumps([
					{"directory": "build", "file": "../src/first.cpp"},
					{"directory": str(root / "other"), "file": "second.cpp"},
					{"directory": str(root), "file": str(root / "src/first.cpp")},
				]),
				encoding="utf-8",
			)
			self.assertEqual(compilation_units(database), {(root / "src/first.cpp").resolve(), (root / "other/second.cpp").resolve()})

	def test_invalid_database_is_not_treated_as_an_empty_scan(self):
		with TemporaryDirectory() as temporary:
			database = Path(temporary) / "compile_commands.json"
			for content in ("{}", "[{}]", "[null]", '[{"file":1,"directory":"."}]', "broken json"):
				with self.subTest(content=content):
					database.write_text(content, encoding="utf-8")
					with self.assertRaises(ValueError):
						compilation_units(database)

	def test_missing_database_is_reported(self):
		with TemporaryDirectory() as temporary:
			with self.assertRaises(FileNotFoundError):
				compilation_units(Path(temporary) / "missing.json")

	def test_strict_scan_reports_foreign_platform_as_a_skip(self):
		with TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "qmclient_scripts").mkdir()
			(root / "qmclient_scripts/cmake-windows.cmd").touch()
			(root / "src").mkdir()
			for name in ("windows.cpp", "android.cpp"):
				(root / "src" / name).touch()
			build = root / "cmake-build-debug"
			build.mkdir()
			(build / "compile_commands.json").write_text(json.dumps([{"directory": str(root), "file": "src/windows.cpp"}]), encoding="utf-8")
			results = ResultCollector()
			previous = Path.cwd()
			try:
				with (
					mock.patch.object(strict_build, "REPO_ROOT", root),
					mock.patch.object(strict_build.runner, "resolve_cmake_command", return_value="cmake"),
					mock.patch.object(strict_build.shutil, "which", return_value="clang-tidy"),
					mock.patch.object(strict_build, "_configure_and_build"),
					mock.patch.object(strict_build, "_repo_command") as command,
				):
					strict_build.run(results, ["src/windows.cpp", "src/android.cpp"], base_ref="origin/master")
				self.assertEqual(command.call_count, 1)
				self.assertEqual(command.call_args.args[2][1], str(root / "src/windows.cpp"))
				skipped = [item for item in results.items if item.status == "SKIP"]
				self.assertEqual(len(skipped), 1)
				self.assertIn("android.cpp", skipped[0].detail)
			finally:
				os.chdir(previous)


if __name__ == "__main__":
	unittest.main()
