from __future__ import annotations

from pathlib import Path
import re
import tempfile
import unittest

from qmclient_scripts import check_config_variables


class ConfigVariableUsageTest(unittest.TestCase):
    def setUp(self):
        self.pattern = re.compile(
            check_config_variables.generate_regex(
                "QmDefaultsProfileVersion", "qm_defaults_profile_version"
            )
        )

    def test_passed_config_objects_count_as_uses(self):
        for expression in (
            "Config.m_QmDefaultsProfileVersion = 1;",
            "pConfig->m_QmDefaultsProfileVersion >= 1",
            "g_Config.m_QmDefaultsProfileVersion",
            "Config()->m_QmDefaultsProfileVersion",
        ):
            with self.subTest(expression=expression):
                self.assertIsNotNone(self.pattern.search(expression))

    def test_config_member_pointers_count_as_uses(self):
        self.assertIsNotNone(
            self.pattern.search("&CConfig::m_QmDefaultsProfileVersion")
        )

    def test_similarly_named_members_do_not_count_as_uses(self):
        for expression in (
            "Config.m_QmDefaultsProfileVersionBackup",
            "Config.m_QmDefaultsProfileVersion2",
            "Config.m_OtherQmDefaultsProfileVersion",
        ):
            with self.subTest(expression=expression):
                self.assertIsNone(self.pattern.search(expression))

    def test_scan_keeps_reporting_genuinely_unused_variables(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "default_profile.h"
            source.write_text(
                "void Initialize(CConfig &Config) { Config.m_QmDefaultsProfileVersion = 1; }\n",
                encoding="utf-8",
            )
            unused = check_config_variables.find_config_variables(
                {
                    "QmDefaultsProfileVersion": "qm_defaults_profile_version",
                    "QmUnusedSetting": "qm_unused_setting",
                },
                source_root=directory,
            )
        self.assertEqual(unused, {"QmUnusedSetting"})

    def test_scan_preserves_member_and_console_usage_contexts(self):
        expressions = (
            "Config.m_QmDefaultsProfileVersion = 1;",
            "pConfig->m_QmDefaultsProfileVersion >= 1",
            "Config()->m_QmDefaultsProfileVersion",
            "&CConfig::m_QmDefaultsProfileVersion",
            'Console()->Register("qm_defaults_profile_version", "", 0, nullptr, nullptr, "");',
            'Console()->Chain("qm_defaults_profile_version", nullptr, nullptr);',
            'Console()->ExecuteLine("qm_defaults_profile_version");',
        )
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "usage.cpp"
            for expression in expressions:
                with self.subTest(expression=expression):
                    source.write_text(expression, encoding="utf-8")
                    self.assertEqual(check_config_variables.find_config_variables(
                        {"QmDefaultsProfileVersion": "qm_defaults_profile_version"},
                        source_root=directory,
                    ), set())

    def test_scan_does_not_accept_bare_members_or_unrelated_strings(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "unused.cpp"
            source.write_text(
                'm_QmDefaultsProfileVersion; "qm_defaults_profile_version"; Config.m_QmDefaultsProfileVersionBackup;',
                encoding="utf-8",
            )
            self.assertEqual(check_config_variables.find_config_variables(
                {"QmDefaultsProfileVersion": "qm_defaults_profile_version"},
                source_root=directory,
            ), {"QmDefaultsProfileVersion"})


if __name__ == "__main__":
    unittest.main()
