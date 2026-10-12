#ifndef GAME_CLIENT_QMUI_SETTINGSSEARCHINDEX_H
#define GAME_CLIENT_QMUI_SETTINGSSEARCHINDEX_H

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace qm_settings_search
{
	struct SDocument
	{
		std::string m_CardId;
		std::string m_SettingId;
		std::string m_Label;
		std::string m_ConfigName;
		std::string m_Aliases;
		std::string m_CardTitle;
		std::string m_Description;
		std::string m_Keywords;
	};

	enum class EMatchKind
	{
		EXACT,
		PREFIX,
		TERMS,
		FUZZY,
	};

	struct SMatch
	{
		size_t m_DocumentIndex = 0;
		float m_Score = 0.0f;
		EMatchKind m_Kind = EMatchKind::TERMS;
	};

	// 索引拥有全部文本；仅在注册表或语言变化时重建，查询不会修改索引。
	class CIndex
	{
		struct SField
		{
			std::u32string m_Text;
			std::vector<std::u32string> m_vWords;
			std::unordered_map<std::u32string, size_t> m_TermFrequency;
			size_t m_Length = 0;
		};
		struct SIndexedDocument
		{
			std::array<SField, 6> m_aFields;
		};

		std::vector<SDocument> m_vDocuments;
		std::vector<SIndexedDocument> m_vIndexedDocuments;
		std::unordered_map<std::u32string, size_t> m_DocumentFrequency;
		std::array<float, 6> m_aAverageFieldLength{};

	public:
		void SetDocuments(std::vector<SDocument> Documents);
		const std::vector<SDocument> &Documents() const { return m_vDocuments; }
		std::vector<SMatch> Search(const char *pQuery) const;
	};
} // namespace qm_settings_search

#endif // GAME_CLIENT_QMUI_SETTINGSSEARCHINDEX_H
