#include "SettingsSearchIndex.h"

#include <base/str.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace qm_settings_search
{
	namespace
	{
		constexpr std::array<float, 6> s_aFieldWeights = {12.0f, 11.0f, 9.0f, 4.0f, 1.0f, 2.0f};

		bool IsCjk(char32_t Code)
		{
			return (Code >= 0x3400 && Code <= 0x9fff) || (Code >= 0x20000 && Code <= 0x3134f) ||
			       (Code >= 0x3040 && Code <= 0x30ff) || (Code >= 0xac00 && Code <= 0xd7af);
		}

		bool IsSeparator(char32_t Code)
		{
			return str_utf8_isspace(static_cast<int>(Code)) ||
			       (Code < 128 && !((Code >= U'a' && Code <= U'z') || (Code >= U'0' && Code <= U'9'))) ||
			       (Code >= 0x2000 && Code <= 0x206f) || (Code >= 0x3000 && Code <= 0x303f) ||
			       (Code >= 0xff00 && Code <= 0xff65);
		}

		std::u32string Normalize(const char *pText)
		{
			std::u32string Text;
			if(pText == nullptr)
				return Text;
			while(*pText != '\0')
			{
				int Code = str_utf8_decode(&pText);
				if(Code < 0)
					Code = ' ';
				if(Code >= 0xff01 && Code <= 0xff5e)
					Code -= 0xfee0;
				Code = str_utf8_tolower_codepoint(Code);
				if(IsSeparator(static_cast<char32_t>(Code)))
				{
					if(!Text.empty() && Text.back() != U' ')
						Text.push_back(U' ');
				}
				else
					Text.push_back(static_cast<char32_t>(Code));
			}
			if(!Text.empty() && Text.back() == U' ')
				Text.pop_back();
			return Text;
		}

		std::vector<std::u32string> Words(const std::u32string &Text)
		{
			std::vector<std::u32string> vWords;
			size_t Begin = 0;
			for(size_t Index = 0; Index <= Text.size(); ++Index)
			{
				if(Index == Text.size() || Text[Index] == U' ' ||
					(Index > Begin && IsCjk(Text[Index]) != IsCjk(Text[Begin])))
				{
					if(Index > Begin)
						vWords.push_back(Text.substr(Begin, Index - Begin));
					Begin = Index;
					if(Index < Text.size() && Text[Index] == U' ')
						++Begin;
				}
			}
			return vWords;
		}

		struct SPinyin
		{
			const char32_t *m_pText;
			const char32_t *m_pFull;
			const char32_t *m_pInitials;
		};

		// 仅维护常用设置词组，保留多音字的词组语义；未知汉字不猜读音。
		constexpr SPinyin s_aPinyin[] = {
			{U"聊天框", U"liaotiankuang", U"ltk"},
			{U"聊天", U"liaotian", U"lt"},
			{U"透明度", U"toumingdu", U"tmd"},
			{U"不透明度", U"butoumingdu", U"btmd"},
			{U"透明", U"touming", U"tm"},
			{U"字体大小", U"zitidaxiao", U"ztdx"},
			{U"文字大小", U"wenzidaxiao", U"wzdx"},
			{U"字体", U"ziti", U"zt"},
			{U"字号", U"zihao", U"zh"},
			{U"文字", U"wenzi", U"wz"},
			{U"灵动岛", U"lingdongdao", U"ldd"},
			{U"鼠标灵敏度", U"shubiaolingmindu", U"sblmd"},
			{U"灵敏度", U"lingmindu", U"lmd"},
			{U"鼠标", U"shubiao", U"sb"},
			{U"键盘", U"jianpan", U"jp"},
			{U"键鼠", U"jianshu", U"js"},
			{U"按键", U"anjian", U"aj"},
			{U"绑定", U"bangding", U"bd"},
			{U"显示", U"xianshi", U"xs"},
			{U"隐藏", U"yincang", U"yc"},
			{U"控制台", U"kongzhitai", U"kzt"},
			{U"计分板", U"jifenban", U"jfb"},
			{U"皮肤", U"pifu", U"pf"},
			{U"描边", U"miaobian", U"mb"},
			{U"颜色", U"yanse", U"ys"},
			{U"背景", U"beijing", U"bj"},
			{U"界面", U"jiemian", U"jm"},
			{U"菜单", U"caidan", U"cd"},
			{U"设置", U"shezhi", U"sz"},
			{U"动画", U"donghua", U"dh"},
			{U"特效", U"texiao", U"tx"},
			{U"模糊", U"mohu", U"mh"},
			{U"圆角", U"yuanjiao", U"yj"},
			{U"缩放", U"suofang", U"sf"},
			{U"大小", U"daxiao", U"dx"},
			{U"宽度", U"kuandu", U"kd"},
			{U"高度", U"gaodu", U"gd"},
			{U"位置", U"weizhi", U"wz"},
			{U"偏移", U"pianyi", U"py"},
			{U"名字", U"mingzi", U"mz"},
			{U"昵称", U"nicheng", U"nc"},
			{U"名牌", U"mingpai", U"mp"},
			{U"声音", U"shengyin", U"sy"},
			{U"音量", U"yinliang", U"yl"},
			{U"音效", U"yinxiao", U"yx"},
			{U"音乐", U"yinyue", U"yy"},
			{U"输入法", U"shurufa", U"srf"},
			{U"候选栏", U"houxuanlan", U"hxl"},
			{U"补全", U"buquan", U"bq"},
			{U"命令", U"mingling", U"ml"},
			{U"指令", U"zhiling", U"zl"},
			{U"表情", U"biaoqing", U"bq"},
			{U"回放", U"huifang", U"hf"},
			{U"录像", U"luxiang", U"lx"},
			{U"地图", U"ditu", U"dt"},
			{U"收藏", U"shoucang", U"sc"},
			{U"历史", U"lishi", U"ls"},
			{U"服务器", U"fuwuqi", U"fwq"},
			{U"好友", U"haoyou", U"hy"},
			{U"战队", U"zhandui", U"zd"},
			{U"镜头", U"jingtou", U"jt"},
			{U"视野", U"shiye", U"sy"},
			{U"武器", U"wuqi", U"wq"},
			{U"辅助线", U"fuzhuxian", U"fzx"},
			{U"弹道", U"dandao", U"dd"},
			{U"全屏", U"quanping", U"qp"},
			{U"窗口", U"chuangkou", U"ck"},
		};

		void AddUniqueWord(std::vector<std::u32string> &vWords, const std::u32string &Word)
		{
			if(!Word.empty() && std::find(vWords.begin(), vWords.end(), Word) == vWords.end())
				vWords.push_back(Word);
		}

		void AddPinyin(const std::u32string &Text, std::vector<std::u32string> &vWords)
		{
			std::u32string Full;
			std::u32string Initials;
			auto Flush = [&] {
				AddUniqueWord(vWords, Full);
				AddUniqueWord(vWords, Initials);
				Full.clear();
				Initials.clear();
			};
			for(size_t Position = 0; Position < Text.size();)
			{
				if(!IsCjk(Text[Position]))
				{
					Flush();
					++Position;
					continue;
				}
				const SPinyin *pMatch = nullptr;
				size_t MatchLength = 0;
				for(const SPinyin &Pinyin : s_aPinyin)
				{
					const size_t Length = std::char_traits<char32_t>::length(Pinyin.m_pText);
					if(Length > MatchLength && Text.compare(Position, Length, Pinyin.m_pText) == 0)
					{
						pMatch = &Pinyin;
						MatchLength = Length;
					}
				}
				if(pMatch != nullptr)
				{
					AddUniqueWord(vWords, pMatch->m_pFull);
					AddUniqueWord(vWords, pMatch->m_pInitials);
					Full += pMatch->m_pFull;
					Initials += pMatch->m_pInitials;
					Position += MatchLength;
				}
				else
				{
					Flush();
					++Position;
				}
			}
			Flush();
		}

		// 同义词只扩展检索，不改变标签和配置含义；可按玩家常用说法逐组维护。
		const std::vector<std::vector<std::u32string>> s_vSynonyms = {
			{U"透明度", U"不透明度", U"opacity", U"alpha"},
			{U"字号", U"字体大小", U"文字大小", U"font size", U"text size"},
			{U"聊天", U"聊天框", U"chat"},
			{U"背景", U"background"},
			{U"灵敏度", U"鼠标速度", U"sensitivity"},
			{U"计分板", U"积分榜", U"scoreboard"},
			{U"键鼠显示", U"按键显示", U"键鼠可视化", U"按键可视化", U"input overlay"},
			{U"昵称", U"名字", U"name"},
			{U"皮肤", U"skin"},
			{U"回放", U"录像", U"replay", U"demo"},
			{U"全屏", U"fullscreen"},
		};

		std::vector<std::u32string> Alternatives(const std::u32string &Term)
		{
			for(const auto &vGroup : s_vSynonyms)
			{
				if(std::find(vGroup.begin(), vGroup.end(), Term) != vGroup.end())
					return vGroup;
			}
			return {Term};
		}

		std::vector<std::u32string> QueryWords(const std::u32string &Query)
		{
			const std::vector<std::u32string> vWords = Words(Query);
			std::vector<std::u32string> vResult;
			for(size_t Index = 0; Index < vWords.size(); ++Index)
			{
				std::u32string Term = vWords[Index];
				size_t Consumed = 1;
				// 含空格的同义词保留为一个查询单元，例如 font size 与字号。
				for(const auto &vGroup : s_vSynonyms)
				{
					for(const std::u32string &Alternative : vGroup)
					{
						if(Alternative.find(U' ') == std::u32string::npos)
							continue;
						const auto vParts = Words(Alternative);
						if(vParts.size() > Consumed && Index + vParts.size() <= vWords.size() &&
							std::equal(vParts.begin(), vParts.end(), vWords.begin() + Index))
						{
							Term = Alternative;
							Consumed = vParts.size();
						}
					}
				}
				vResult.push_back(std::move(Term));
				Index += Consumed - 1;
			}
			return vResult;
		}

		bool IsAsciiWord(const std::u32string &Word)
		{
			return std::all_of(Word.begin(), Word.end(), [](char32_t Code) {
				return (Code >= U'a' && Code <= U'z') || (Code >= U'0' && Code <= U'9');
			});
		}

		// 有界 Damerau-Levenshtein：相邻字母换位算一次，短词不进入容错。
		int EditDistance(const std::u32string &Left, const std::u32string &Right, int Limit, std::array<std::vector<int>, 3> &aRows)
		{
			if(Left.size() > Right.size() + Limit || Right.size() > Left.size() + Limit)
				return Limit + 1;
			std::vector<int> &vPrevious = aRows[0];
			std::vector<int> &vPreviousPrevious = aRows[1];
			std::vector<int> &vCurrent = aRows[2];
			for(size_t Index = 0; Index <= Right.size(); ++Index)
				vPrevious[Index] = static_cast<int>(Index);
			for(size_t LeftIndex = 1; LeftIndex <= Left.size(); ++LeftIndex)
			{
				vCurrent[0] = static_cast<int>(LeftIndex);
				int RowMinimum = vCurrent[0];
				for(size_t RightIndex = 1; RightIndex <= Right.size(); ++RightIndex)
				{
					const int Cost = Left[LeftIndex - 1] == Right[RightIndex - 1] ? 0 : 1;
					int Distance = std::min({vPrevious[RightIndex] + 1, vCurrent[RightIndex - 1] + 1, vPrevious[RightIndex - 1] + Cost});
					if(LeftIndex > 1 && RightIndex > 1 && Left[LeftIndex - 1] == Right[RightIndex - 2] && Left[LeftIndex - 2] == Right[RightIndex - 1])
						Distance = std::min(Distance, vPreviousPrevious[RightIndex - 2] + 1);
					vCurrent[RightIndex] = Distance;
					RowMinimum = std::min(RowMinimum, Distance);
				}
				if(RowMinimum > Limit)
					return Limit + 1;
				vPreviousPrevious.swap(vPrevious);
				vPrevious.swap(vCurrent);
			}
			return vPrevious[Right.size()];
		}

		struct STermMatch
		{
			float m_Score = 0.0f;
			bool m_Fuzzy = false;
		};
	} // namespace

	void CIndex::SetDocuments(std::vector<SDocument> Documents)
	{
		m_vDocuments = std::move(Documents);
		m_vIndexedDocuments.clear();
		m_vIndexedDocuments.reserve(m_vDocuments.size());
		m_DocumentFrequency.clear();
		m_aAverageFieldLength.fill(0.0f);
		for(const SDocument &Document : m_vDocuments)
		{
			SIndexedDocument Indexed;
			const std::string *apText[] = {&Document.m_Label, &Document.m_ConfigName, &Document.m_Aliases, &Document.m_CardTitle, &Document.m_Description, &Document.m_Keywords};
			std::unordered_set<std::u32string> DocumentTerms;
			for(size_t FieldIndex = 0; FieldIndex < Indexed.m_aFields.size(); ++FieldIndex)
			{
				SField &Field = Indexed.m_aFields[FieldIndex];
				Field.m_Text = Normalize(apText[FieldIndex]->c_str());
				Field.m_vWords = Words(Field.m_Text);
				AddPinyin(Field.m_Text, Field.m_vWords);
				for(const std::u32string &Word : Field.m_vWords)
				{
					++Field.m_TermFrequency[Word];
					++Field.m_Length;
					if(IsCjk(Word.front()) && Word.size() > 2)
					{
						for(size_t Position = 0; Position + 1 < Word.size(); ++Position)
						{
							++Field.m_TermFrequency[Word.substr(Position, 2)];
							++Field.m_Length;
						}
					}
				}
				for(const auto &Term : Field.m_TermFrequency)
					DocumentTerms.insert(Term.first);
				m_aAverageFieldLength[FieldIndex] += static_cast<float>(Field.m_Length);
			}
			for(const auto &Term : DocumentTerms)
				++m_DocumentFrequency[Term];
			m_vIndexedDocuments.push_back(std::move(Indexed));
		}
		for(float &Average : m_aAverageFieldLength)
			Average = m_vDocuments.empty() ? 1.0f : std::max(1.0f, Average / static_cast<float>(m_vDocuments.size()));
	}

	std::vector<SMatch> CIndex::Search(const char *pQuery) const
	{
		const std::u32string Query = Normalize(pQuery);
		std::vector<SMatch> vMatches;
		// 限制外部输入的分段与编辑距离工作量，过长查询不截断为意外命中。
		if(Query.empty() || Query.size() > 256 || m_vIndexedDocuments.empty())
			return vMatches;
		const std::vector<std::u32string> vQueryWords = QueryWords(Query);
		std::unordered_map<std::u32string, std::vector<std::u32string>> QueryAlternatives;
		auto PrepareAlternatives = [&](const std::u32string &Term) {
			if(QueryAlternatives.find(Term) == QueryAlternatives.end())
				QueryAlternatives.emplace(Term, Alternatives(Term));
		};
		// 每次查询只生成一次分段文本，所有文档共享；匹配循环不再构造子串。
		std::vector<std::vector<std::vector<std::u32string>>> vvvSegments(vQueryWords.size());
		for(size_t WordIndex = 0; WordIndex < vQueryWords.size(); ++WordIndex)
		{
			const std::u32string &Word = vQueryWords[WordIndex];
			PrepareAlternatives(Word);
			if(!IsCjk(Word.front()) || Word.size() <= 2)
				continue;
			vvvSegments[WordIndex].resize(Word.size());
			for(size_t Begin = 0; Begin < Word.size(); ++Begin)
			{
				auto &vSegments = vvvSegments[WordIndex][Begin];
				vSegments.reserve(Word.size() - Begin - 1);
				for(size_t Length = 2; Length <= Word.size() - Begin; ++Length)
				{
					vSegments.push_back(Word.substr(Begin, Length));
					PrepareAlternatives(vSegments.back());
				}
			}
		}
		size_t LongestAlternative = 0;
		for(const auto &Entry : QueryAlternatives)
			for(const std::u32string &Alternative : Entry.second)
				LongestAlternative = std::max(LongestAlternative, Alternative.size());
		// 候选长度差最多为二，三行工作区覆盖所有同义词并在整次查询内复用。
		std::array<std::vector<int>, 3> aEditRows;
		for(auto &vRow : aEditRows)
			vRow.resize(LongestAlternative + 3);
		std::vector<float> vScores(Query.size() + 1);
		for(size_t DocumentIndex = 0; DocumentIndex < m_vIndexedDocuments.size(); ++DocumentIndex)
		{
			const SIndexedDocument &Document = m_vIndexedDocuments[DocumentIndex];
			auto MatchTerm = [&](const std::u32string &Term, bool AllowFuzzy) {
				STermMatch Best;
				const auto AlternativesIt = QueryAlternatives.find(Term);
				for(const std::u32string &Alternative : AlternativesIt->second)
				{
					for(size_t FieldIndex = 0; FieldIndex < Document.m_aFields.size(); ++FieldIndex)
					{
						const SField &Field = Document.m_aFields[FieldIndex];
						float Quality = 0.0f;
						bool Fuzzy = false;
						size_t Frequency = 1;
						const auto FrequencyIt = Field.m_TermFrequency.find(Alternative);
						if(FrequencyIt != Field.m_TermFrequency.end())
						{
							Quality = 1.0f;
							Frequency = FrequencyIt->second;
						}
						else if((IsCjk(Alternative.front()) || Alternative.find(U' ') != std::u32string::npos) && Field.m_Text.find(Alternative) != std::u32string::npos)
							Quality = 0.9f;
						else if(Alternative.size() >= 3)
						{
							for(const std::u32string &Word : Field.m_vWords)
							{
								if(Word.compare(0, Alternative.size(), Alternative) == 0)
									Quality = std::max(Quality, 0.75f);
								else if(Alternative.size() >= 4 && Word.find(Alternative) != std::u32string::npos)
									Quality = std::max(Quality, 0.55f);
							}
						}
						if(Quality == 0.0f && AllowFuzzy && Alternative.size() >= 5 && IsAsciiWord(Alternative) && FieldIndex != 4)
						{
							const int Limit = Alternative.size() >= 9 ? 2 : 1;
							for(const std::u32string &Word : Field.m_vWords)
							{
								if(Word.size() >= 5 && IsAsciiWord(Word) && EditDistance(Alternative, Word, Limit, aEditRows) <= Limit)
								{
									Quality = 0.35f;
									Fuzzy = true;
									break;
								}
							}
						}
						if(Quality == 0.0f)
							continue;
						const auto DocumentFrequencyIt = m_DocumentFrequency.find(Alternative);
						const float DocumentFrequency = DocumentFrequencyIt != m_DocumentFrequency.end() ? static_cast<float>(DocumentFrequencyIt->second) : 1.0f;
						const float InverseFrequency = std::log(1.0f + (static_cast<float>(m_vDocuments.size()) - DocumentFrequency + 0.5f) / (DocumentFrequency + 0.5f));
						const float TermFrequency = static_cast<float>(Frequency);
						const float LengthRatio = static_cast<float>(Field.m_Length) / m_aAverageFieldLength[FieldIndex];
						const float Bm25 = InverseFrequency * TermFrequency * 2.2f / (TermFrequency + 1.2f * (0.25f + 0.75f * LengthRatio));
						const float Score = s_aFieldWeights[FieldIndex] * Quality * (1.0f + Bm25);
						// 准确匹配优先于任何字段上的模糊匹配。
						if(Best.m_Score == 0.0f || (Best.m_Fuzzy && !Fuzzy) || (Best.m_Fuzzy == Fuzzy && Score > Best.m_Score))
							Best = {Score, Fuzzy};
					}
				}
				return Best;
			};

			float Score = 0.0f;
			bool AllMatched = true;
			bool AnyFuzzy = false;
			for(size_t WordIndex = 0; WordIndex < vQueryWords.size(); ++WordIndex)
			{
				const std::u32string &Word = vQueryWords[WordIndex];
				STermMatch Match = MatchTerm(Word, true);
				if(Match.m_Score == 0.0f && IsCjk(Word.front()) && Word.size() > 2)
				{
					// 中文连续输入按码点分段；要求整段被覆盖，不要求词与词之间的跨界 bigram 命中。
					std::fill_n(vScores.begin(), Word.size() + 1, -1.0f);
					vScores[0] = 0.0f;
					for(size_t Begin = 0; Begin < Word.size(); ++Begin)
					{
						if(vScores[Begin] < 0.0f)
							continue;
						for(size_t Length = 2; Length <= Word.size() - Begin; ++Length)
						{
							const STermMatch Segment = MatchTerm(vvvSegments[WordIndex][Begin][Length - 2], false);
							if(Segment.m_Score > 0.0f)
								vScores[Begin + Length] = std::max(vScores[Begin + Length], vScores[Begin] + Segment.m_Score * static_cast<float>(Length) / static_cast<float>(Word.size()));
						}
					}
					if(vScores[Word.size()] > 0.0f)
						Match = {vScores[Word.size()], false};
				}
				if(Match.m_Score == 0.0f)
				{
					AllMatched = false;
					break;
				}
				Score += Match.m_Score;
				AnyFuzzy = AnyFuzzy || Match.m_Fuzzy;
			}
			if(!AllMatched)
				continue;
			EMatchKind Kind = AnyFuzzy ? EMatchKind::FUZZY : EMatchKind::TERMS;
			if(Document.m_aFields[0].m_Text == Query || Document.m_aFields[1].m_Text == Query)
			{
				Kind = EMatchKind::EXACT;
				Score += 1000.0f;
				// 同一配置的预览条目没有标签，优先展示可直接调整的设置行。
				if(!Document.m_aFields[0].m_Text.empty())
					Score += 50.0f;
			}
			else if(!AnyFuzzy && (Document.m_aFields[0].m_Text.compare(0, Query.size(), Query) == 0 || Document.m_aFields[1].m_Text.compare(0, Query.size(), Query) == 0))
			{
				Kind = EMatchKind::PREFIX;
				Score += 100.0f;
			}
			vMatches.push_back({DocumentIndex, Score, Kind});
		}
		std::stable_sort(vMatches.begin(), vMatches.end(), [](const SMatch &Left, const SMatch &Right) {
			if(Left.m_Kind != Right.m_Kind)
				return Left.m_Kind < Right.m_Kind;
			return Left.m_Score > Right.m_Score;
		});
		return vMatches;
	}
} // namespace qm_settings_search
