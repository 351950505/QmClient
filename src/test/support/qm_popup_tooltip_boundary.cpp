#include <game/client/components/menus.h>
#include <game/client/components/tooltips.h>

#include <gtest/gtest.h>

// 胶囊表面使用真实公共绘制；完整菜单按钮不属于独立目标，误入即失败。
int CMenus::DoButton_MenuTab(CButtonContainer *, const char *, int, const CUIRect *, int, SUIAnimator *, const ColorRGBA *, const ColorRGBA *, const ColorRGBA *, float, const CCommunityIcon *, CUIElement *, float, bool)
{
	ADD_FAILURE() << "Full menu tab buttons are outside the isolated UI target";
	return 0;
}

// 独立输入／下拉集成目标没有完整菜单与配置管理器；提示作为外部边界，
// 不参与这些测试的断言。按钮和输入仍链接真实生产实现。
void CTooltips::DoConfigToolTip(const void *, const CUIRect *, const void *, const void *)
{
}

void CTooltips::DoSettingsToolTipForConfig(const void *, const CUIRect *, const void *, const CUIRect *, const void *)
{
}

// 独立 UI 目标不创建提示组件；左右按钮仍通过真实公共入口独立登记。
void CTooltips::DoToolTipForRect(const void *, const CUIRect *, const char *, float)
{
}

// 本目标不启用菜单卡片说明作用域；括号标签解析由生产策略单元测试覆盖。
const char *CTooltips::PrepareCardLabel(const CUIRect *, const char *pText, bool)
{
	return pText;
}

// 为 Itanium ABI 的 key function 提供定义，sanitizer 的 vptr 检查需要完整 RTTI。
// 本目标不构造或测试完整 Tooltip 组件；真实文字布局由独立协作测试覆盖。
void CTooltips::OnReset()
{
}

void CTooltips::OnRender()
{
}

// vtable 的窗口/退出入口会引用清理函数；本目标不构造完整提示组件。
void CTooltips::ClearActiveTooltip()
{
}

// 独立目标没有客户端 owner；仅提供未使用的基类 key function 以生成 RTTI。
void CComponentInterfaces::OnInterfacesInit(CGameClient *)
{
}
