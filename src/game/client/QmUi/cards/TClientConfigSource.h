#ifndef GAME_CLIENT_QMUI_CARDS_TCLIENTCONFIGSOURCE_H
#define GAME_CLIENT_QMUI_CARDS_TCLIENTCONFIGSOURCE_H

#include <base/str.h>

inline bool IsTClientConfigVariable(const char *pName)
{
	// 变量已统一使用 qm_ 前缀，来源从原始配置清单取得。
	static constexpr const char *s_apNames[] = {
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) #ScriptName,
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) #ScriptName,
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) #ScriptName,
#include <engine/shared/config_variables_tclient.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
	};
	for(const char *pTClientName : s_apNames)
		if(str_comp(pName, pTClientName) == 0)
			return true;
	return false;
}

#endif
