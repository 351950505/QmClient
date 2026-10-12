#ifndef GAME_CLIENT_QMUI_SETTINGSSEARCHSESSION_H
#define GAME_CLIENT_QMUI_SETTINGSSEARCHSESSION_H

#include <game/client/lineinput.h>
#include <game/client/ui.h>

// 离页和菜单关闭只释放编辑权；查询由输入控件显式清空，结果缓存沿用查询。
inline void ReleaseSettingsSearchInput(CUi *pUi, CLineInput *pInput)
{
	if(pUi->ActiveItem() == pInput)
		pUi->ReleaseActiveTextInput(pInput);
	else
		pInput->Deactivate();
}

#endif
