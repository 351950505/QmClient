// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_THEME_SCAN_H
#define GAME_CLIENT_COMPONENTS_THEME_SCAN_H

#include <base/system.h>

#include <game/client/components/background.h>

#include <array>
#include <string>
#include <vector>

inline bool IsThemeFileCandidate(const char *pName)
{
	return pName != nullptr && str_length(pName) > 4 && str_endswith_nocase(pName, ".map");
}

struct SThemeFileEntry
{
	std::string m_Name;
	bool m_HasDay = false;
	bool m_HasNight = false;
	std::string m_IconPath;
	std::array<std::string, 3> m_aMapNames;
};

inline std::string ThemeBaseName(const char *pName)
{
	std::string Name = pName != nullptr ? pName : "";
	if(str_endswith_nocase(Name.c_str(), ".map"))
		Name.resize(Name.size() - 4);
	if(str_endswith_nocase(Name.c_str(), "_day"))
		Name.resize(Name.size() - 4);
	else if(str_endswith_nocase(Name.c_str(), "_night"))
		Name.resize(Name.size() - 6);
	return Name;
}

inline std::string ThemeDisplayName(const char *pName)
{
	if(pName == nullptr || pName[0] == '\0')
		return "(none)";
	if(str_comp(pName, "auto") == 0)
		return "(seasons)";
	if(str_comp(pName, "rand") == 0)
		return "(random)";
	// 扫描后的名称已归并一次；只有原始地图文件名需要去掉扩展和时段后缀。
	return IsThemeFileCandidate(pName) ? ThemeBaseName(pName) : std::string(pName);
}

// 预览图片不参与发现；昼夜地图共用一个配置值，跨存储重复只合并可用时段。
inline void AppendThemeFileEntry(std::vector<SThemeFileEntry> &vEntries, const char *pName, bool IsDirectory)
{
	if(IsDirectory || !IsThemeFileCandidate(pName))
		return;
	const std::string Name = ThemeBaseName(pName);
	if(Name.empty())
		return;
	const std::string FileName(pName, str_length(pName) - 4);
	const bool DayOnly = str_endswith_nocase(FileName.c_str(), "_day") != nullptr;
	const bool NightOnly = str_endswith_nocase(FileName.c_str(), "_night") != nullptr;
	const bool HasDay = !NightOnly;
	const bool HasNight = !DayOnly;
	const int Variant = DayOnly ? 1 : NightOnly ? 2 :
						      0;
	for(auto &Entry : vEntries)
	{
		if(Entry.m_Name == Name)
		{
			Entry.m_HasDay |= HasDay;
			Entry.m_HasNight |= HasNight;
			if(Entry.m_aMapNames[Variant].empty())
				Entry.m_aMapNames[Variant] = pName;
			return;
		}
	}
	SThemeFileEntry Entry{Name, HasDay, HasNight, std::string("themes/") + Name + ".png", {}};
	Entry.m_aMapNames[Variant] = pName;
	vEntries.push_back(std::move(Entry));
}

inline const std::string &ResolveThemeMapFileName(const std::array<std::string, 3> &aNames, bool IsDaytime)
{
	const int Preferred = IsDaytime ? 1 : 2;
	if(!aNames[Preferred].empty())
		return aNames[Preferred];
	if(!aNames[0].empty())
		return aNames[0];
	return aNames[IsDaytime ? 2 : 1];
}

inline std::string BuildThemeCandidatePath(const char *pBasePath, const char *pExtension)
{
	return std::string(pBasePath) + pExtension;
}

inline std::string ThemeIconPathFromName(const char *pName)
{
	const char *pThemeName = pName != nullptr && pName[0] != '\0' ? pName : "none";
	if(IsBackgroundImageExtension(pThemeName))
		return std::string("themes/") + pThemeName;

	char aBaseName[IO_MAX_PATH_LENGTH];
	str_copy(aBaseName, pThemeName, sizeof(aBaseName));
	if(const char *pExt = str_endswith_nocase(aBaseName, ".map"))
	{
		char aTmp[IO_MAX_PATH_LENGTH];
		str_truncate(aTmp, sizeof(aTmp), aBaseName, pExt - aBaseName);
		str_copy(aBaseName, aTmp, sizeof(aBaseName));
	}
	else if(const char *pBackgroundExt = FindBackgroundFileExtension(aBaseName))
	{
		char aTmp[IO_MAX_PATH_LENGTH];
		str_truncate(aTmp, sizeof(aTmp), aBaseName, str_length(aBaseName) - str_length(pBackgroundExt));
		str_copy(aBaseName, aTmp, sizeof(aBaseName));
	}

	if(const char *pDaySuffix = str_endswith(aBaseName, "_day"))
	{
		char aTmp[IO_MAX_PATH_LENGTH];
		str_truncate(aTmp, sizeof(aTmp), aBaseName, pDaySuffix - aBaseName);
		str_copy(aBaseName, aTmp, sizeof(aBaseName));
	}
	else if(const char *pNightSuffix = str_endswith(aBaseName, "_night"))
	{
		char aTmp[IO_MAX_PATH_LENGTH];
		str_truncate(aTmp, sizeof(aTmp), aBaseName, pNightSuffix - aBaseName);
		str_copy(aBaseName, aTmp, sizeof(aBaseName));
	}

	return std::string("themes/") + aBaseName + ".png";
}

#endif
