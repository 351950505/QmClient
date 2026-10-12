from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from qmclient_scripts.gate.check_settings_ui_migration import (
	_MODERN_NAVIGATION_SOURCE, _MODERN_PAGE_SOURCE,
	audit_modern_navigation_contracts,
)


class ModernSettingsNavigationContractTest(unittest.TestCase):
	def setUp(self):
		self.temp_dir = TemporaryDirectory()
		self.addCleanup(self.temp_dir.cleanup)
		self.root = Path(self.temp_dir.name)
		# 输入是待审计的注册结构，不模拟 C++ 的路由、迁移或 UI 行为。
		self.write(_MODERN_NAVIGATION_SOURCE, '''
const auto &PageOrder() {
    static const auto s_vPages = {CMenus::SETTINGS_TEE, CMenus::SETTINGS_SOUND, CMenus::SETTINGS_SEARCH};
    return s_vPages;
}
const auto &Views(int Page) {
    static const auto s_Views = {
        {CMenus::SETTINGS_TEE, {{"modern-tee-identity", Localizable("Player")}}},
        {CMenus::SETTINGS_SOUND, {{"modern-sound", Localizable("Sound")}}},
    };
    return s_Views;
}
''')
		self.write("src/game/client/components/menus_settings.cpp", '''
void CMenus::RenderSettings() { RenderModernSettingsPage(View); }
void CMenus::RenderSettingsPlayer() { RenderModernSettingsCards(View, "modern-tee-identity"); }
void CMenus::RenderSettingsSound() { RenderModernSettingsCards(View, "modern-sound"); }
''')
		self.write(_MODERN_PAGE_SOURCE, "void CMenus::RenderModernSettingsCards() { qm_card_catalog::BuildCards(Ctx, Ids, Cards); }")
		self.write("src/game/client/QmUi/SettingsSearchPage.cpp", "void CMenus::RenderSettingsGlobalSearchContent() { qm_card_catalog::BuildCard(Ctx, Id, Card); }")

	def write(self, relative, content):
		path = self.root / relative
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(content, encoding="utf-8")

	def replace(self, relative, before, after):
		path = self.root / relative
		source = path.read_text(encoding="utf-8")
		self.assertIn(before, source)
		path.write_text(source.replace(before, after), encoding="utf-8")

	def test_registered_views_and_shared_catalog_entries_are_accepted(self):
		self.assertEqual(audit_modern_navigation_contracts(self.root), [])

	def test_duplicate_view_identity_is_rejected(self):
		self.replace(_MODERN_NAVIGATION_SOURCE, '"modern-sound"', '"modern-tee-identity"')
		self.assertTrue(any("duplicate view registration" in error for error in audit_modern_navigation_contracts(self.root)))

	def test_view_owner_must_be_registered_in_sidebar(self):
		self.replace(_MODERN_NAVIGATION_SOURCE, "{CMenus::SETTINGS_SOUND, {{", "{CMenus::SETTINGS_HUD, {{")
		self.assertTrue(any("owner is not registered" in error for error in audit_modern_navigation_contracts(self.root)))

	def test_card_page_without_a_registered_view_is_rejected(self):
		self.replace(_MODERN_NAVIGATION_SOURCE, "CMenus::SETTINGS_SEARCH};", "CMenus::SETTINGS_HUD, CMenus::SETTINGS_SEARCH};")
		self.assertTrue(any("SETTINGS_HUD: modern view registration missing" in error for error in audit_modern_navigation_contracts(self.root)))

	def test_comment_cannot_supply_view_registration(self):
		self.replace(_MODERN_NAVIGATION_SOURCE, '{CMenus::SETTINGS_SOUND, {{"modern-sound", Localizable("Sound")}}},', '// {CMenus::SETTINGS_SOUND, {{"modern-sound", Localizable("Sound")}}},')
		self.assertTrue(any("SETTINGS_SOUND: modern view registration missing" in error for error in audit_modern_navigation_contracts(self.root)))

	def test_unrelated_literals_cannot_supply_the_view_registry(self):
		self.write(_MODERN_NAVIGATION_SOURCE, '''
const auto s_vPages = {CMenus::SETTINGS_SOUND};
const char *pText = "modern-sound";
// const auto s_Views = {{CMenus::SETTINGS_SOUND, {{"modern-sound", Localizable("Sound")}}}};
''')
		self.assertTrue(any("modern view registry missing" in error for error in audit_modern_navigation_contracts(self.root)))

	def test_wrapper_view_must_exist_in_structured_registry(self):
		self.replace("src/game/client/components/menus_settings.cpp", '"modern-sound"', '"unregistered-view"')
		self.assertTrue(any("unregistered-view: undeclared modern view" in error for error in audit_modern_navigation_contracts(self.root)))

	def test_settings_and_search_must_share_the_card_catalog(self):
		for path, before in (
			(_MODERN_PAGE_SOURCE, "qm_card_catalog::BuildCards"),
			("src/game/client/QmUi/SettingsSearchPage.cpp", "qm_card_catalog::BuildCard"),
		):
			with self.subTest(path=path):
				original = (self.root / path).read_text(encoding="utf-8")
				self.write(path, original.replace(before, "PageSpecificCards"))
				self.assertTrue(any("shared page/catalog delegation missing" in error for error in audit_modern_navigation_contracts(self.root)))
				self.write(path, original)

	def test_modern_page_cannot_reintroduce_legacy_settings_controls(self):
		self.replace(_MODERN_PAGE_SOURCE, "qm_card_catalog::BuildCards", "BeginSettingsCardDeck(); qm_card_catalog::BuildCards")
		self.assertTrue(any("modern page bypasses" in error for error in audit_modern_navigation_contracts(self.root)))


if __name__ == "__main__":
	unittest.main()
