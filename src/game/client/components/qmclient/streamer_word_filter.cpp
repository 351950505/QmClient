#include "streamer_word_filter.h"

#include <base/mem.h>
#include <base/str.h>

#include <cstddef>
#include <map>
#include <vector>

namespace
{
	// 基础词表按完整人名、事件与明确代称维护；不把普通地名、单字或数字当作规则。
	// 来源与维护边界见 docs/规格/2026-10-11-主播模式内置词过滤.md。
	constexpr const char *s_apWords[] = {
		"习近平", "毛泽东", "邓小平", "江泽民", "胡锦涛", "李克强", "李强", "温家宝", "朱镕基",
		"赵紫阳", "胡耀邦", "华国锋", "刘少奇", "周恩来", "彭德怀", "林彪", "薄熙来", "周永康",
		"王岐山", "王沪宁", "栗战书", "汪洋", "韩正", "蔡奇", "丁薛祥", "李希", "彭丽媛", "习明泽",
		"刘晓波", "刘晓霞", "魏京生", "王丹", "吾尔开希", "柴玲", "陈光诚", "高智晟", "郭文贵",
		"蔡英文", "赖清德", "陈水扁", "李登辉", "李洪志", "达赖喇嘛", "彭立发",
		"六四", "六四事件", "六四学运", "六四屠杀", "八九六四", "八九学运", "天安门事件", "天安门屠杀",
		"天安门母亲", "坦克人", "五月三十五日", "白纸运动", "白纸革命", "四通桥", "丰县铁链女",
		"乌鲁木齐大火", "新疆集中营", "再教育营", "雨伞运动", "反送中", "占领中环", "香港独立",
		"台湾独立", "西藏独立", "港独", "台独", "藏独", "疆独", "东突厥斯坦", "东伊运",
		"法轮功", "法轮大法", "大纪元", "明慧网", "九评共产党", "活摘器官", "三退", "退党",
		"中国共产党", "中共", "文化大革命", "大跃进", "零八宪章", "709大抓捕",
		"习禁评", "习近评", "习尽平", "刁近平", "习辶斤平", "习大大", "习包子", "包子帝",
		"庆丰帝", "总加速师", "小熊维尼", "维尼熊",
		"xijinping", "xjp", "maozedong", "dengxiaoping", "jiangzemin", "hujintao", "likeqiang",
		"liuxiaobo", "lihongzhi", "falungong", "falundafa", "tiananmenmassacre", "tankman", "freehongkong", "freetibet",
	};

	int Normalize(int Code)
	{
		if(Code >= 0xff01 && Code <= 0xff5e)
			Code -= 0xfee0;
		Code = str_utf8_tolower_codepoint(Code);
		switch(Code)
		{
		case U'習': return U'习';
		case U'澤': return U'泽';
		case U'東': return U'东';
		case U'鄧': return U'邓';
		case U'錦': return U'锦';
		case U'濤': return U'涛';
		case U'強': return U'强';
		case U'溫': return U'温';
		case U'寶': return U'宝';
		case U'鎔': return U'镕';
		case U'趙': return U'赵';
		case U'陽': return U'阳';
		case U'國': return U'国';
		case U'華': return U'华';
		case U'劉': return U'刘';
		case U'來': return U'来';
		case U'寧': return U'宁';
		case U'戰': return U'战';
		case U'書': return U'书';
		case U'麗': return U'丽';
		case U'曉': return U'晓';
		case U'爾': return U'尔';
		case U'開': return U'开';
		case U'陳': return U'陈';
		case U'誠': return U'诚';
		case U'貴': return U'贵';
		case U'賴': return U'赖';
		case U'輝': return U'辉';
		case U'達': return U'达';
		case U'學': return U'学';
		case U'運': return U'运';
		case U'動': return U'动';
		case U'門': return U'门';
		case U'殺': return U'杀';
		case U'親': return U'亲';
		case U'紙': return U'纸';
		case U'鐵': return U'铁';
		case U'鏈': return U'链';
		case U'豐': return U'丰';
		case U'縣': return U'县';
		case U'烏': return U'乌';
		case U'魯': return U'鲁';
		case U'齊': return U'齐';
		case U'營': return U'营';
		case U'傘': return U'伞';
		case U'佔': return U'占';
		case U'領': return U'领';
		case U'環': return U'环';
		case U'臺': return U'台';
		case U'灣': return U'湾';
		case U'獨': return U'独';
		case U'輪': return U'轮';
		case U'紀': return U'纪';
		case U'評': return U'评';
		case U'黨': return U'党';
		case U'產': return U'产';
		case U'躍': return U'跃';
		case U'進': return U'进';
		case U'憲': return U'宪';
		case U'慶': return U'庆';
		case U'總': return U'总';
		case U'師': return U'师';
		case U'維': return U'维';
		case U'à': case U'á': case U'â': case U'ä': case U'ā': case U'ǎ': return 'a';
		case U'è': case U'é': case U'ê': case U'ë': case U'ē': case U'ě': return 'e';
		case U'ì': case U'í': case U'î': case U'ï': case U'ī': case U'ǐ': return 'i';
		case U'ò': case U'ó': case U'ô': case U'ö': case U'ō': case U'ǒ': return 'o';
		case U'ù': case U'ú': case U'û': case U'ü': case U'ū': case U'ǔ': return 'u';
		case U'а': return 'a';
		case U'е': return 'e';
		case U'і': case U'ι': return 'i';
		case U'ј': return 'j';
		case U'о': case U'ο': return 'o';
		case U'р': case U'ρ': return 'p';
		case U'с': return 'c';
		case U'х': case U'χ': return 'x';
		case U'у': return 'y';
		case U'ѕ': return 's';
		default: return Code;
		}
	}

	bool IsFormatting(int Code)
	{
		return Code == 0x00ad || Code == 0x034f || Code == 0x061c || Code == 0xfeff ||
		       (Code >= 0x0300 && Code <= 0x036f) || (Code >= 0x180b && Code <= 0x180f) ||
		       (Code >= 0x200b && Code <= 0x200f) || (Code >= 0x202a && Code <= 0x202e) ||
		       (Code >= 0x2060 && Code <= 0x206f) || (Code >= 0xfe00 && Code <= 0xfe0f) ||
		       (Code >= 0xe0100 && Code <= 0xe01ef);
	}

	bool IsSeparator(int Code)
	{
		return IsFormatting(Code) || str_utf8_isspace(Code) ||
		       (Code > 0 && Code < 128 && !((Code >= 'a' && Code <= 'z') || (Code >= '0' && Code <= '9'))) ||
		       (Code >= 0x2000 && Code <= 0x206f) || (Code >= 0x2600 && Code <= 0x27bf) ||
		       (Code >= 0x3000 && Code <= 0x303f) || (Code >= 0x1f000 && Code <= 0x1faff);
	}

	bool IsWordCharacter(int Code)
	{
		Code = Normalize(Code);
		return (Code >= 'a' && Code <= 'z') || (Code >= '0' && Code <= '9');
	}

	bool IsWordBefore(const char *pText, const char *pStart)
	{
		while(pStart > pText)
		{
			--pStart;
			while(pStart > pText && (static_cast<unsigned char>(*pStart) & 0xc0) == 0x80)
				--pStart;
			const char *pDecode = pStart;
			const int Code = str_utf8_decode(&pDecode);
			if(!IsFormatting(Code))
				return IsWordCharacter(Code);
		}
		return false;
	}

	bool IsWordAfter(const char *pEnd)
	{
		while(*pEnd)
		{
			const int Code = str_utf8_decode(&pEnd);
			if(!IsFormatting(Code))
				return IsWordCharacter(Code);
		}
		return false;
	}

	struct SNode
	{
		std::map<int, size_t> m_Children;
		bool m_Terminal = false;
		bool m_RequiresWordBoundary = false;
	};

	class CWordTrie
	{
		std::vector<SNode> m_vNodes{1};

	public:
		CWordTrie()
		{
			for(const char *pWord : s_apWords)
			{
				size_t Node = 0;
				bool Latin = true;
				for(const char *pCursor = pWord; *pCursor;)
				{
					const int Code = Normalize(str_utf8_decode(&pCursor));
					Latin &= IsWordCharacter(Code);
					const auto It = m_vNodes[Node].m_Children.find(Code);
					if(It != m_vNodes[Node].m_Children.end())
						Node = It->second;
					else
					{
						const size_t Next = m_vNodes.size();
						m_vNodes[Node].m_Children.emplace(Code, Next);
						m_vNodes.emplace_back();
						Node = Next;
					}
				}
				m_vNodes[Node].m_Terminal = true;
				m_vNodes[Node].m_RequiresWordBoundary = Latin;
			}
		}

		const char *MatchEnd(const char *pText, const char *pStart) const
		{
			size_t Node = 0;
			const char *pEnd = nullptr;
			for(const char *pCursor = pStart; *pCursor;)
			{
				// 与现有 [[...]] 聊天颜色标记兼容，不能靠插入标记拆开词语。
				if(Node != 0 && pCursor[0] == '[' && pCursor[1] == '[')
				{
					const char *pMarker = pCursor + 2;
					while(*pMarker && *pMarker != '\n' && *pMarker != '\r' && !(pMarker[0] == ']' && pMarker[1] == ']'))
						++pMarker;
					if(*pMarker == ']')
					{
						pCursor = pMarker + 2;
						continue;
					}
				}
				const int Code = Normalize(str_utf8_decode(&pCursor));
				// 调用以单条消息为界；损坏的 UTF-8 不参与拼接。
				if(Code < 0)
					break;
				if(IsSeparator(Code))
				{
					if(Node == 0)
						break;
					continue;
				}
				const auto It = m_vNodes[Node].m_Children.find(Code);
				if(It == m_vNodes[Node].m_Children.end())
					break;
				Node = It->second;
				const SNode &Current = m_vNodes[Node];
				if(Current.m_Terminal &&
					(!Current.m_RequiresWordBoundary || (!IsWordBefore(pText, pStart) && !IsWordAfter(pCursor))))
					pEnd = pCursor;
			}
			return pEnd;
		}
	};
}

bool QmStreamerWordFilter::Filter(char *pText, bool Enabled)
{
	if(!Enabled || pText == nullptr || pText[0] == '\0')
		return false;

	// 初始化后词表只读；每次过滤直接在调用方缓冲区压缩，不分配临时字符数组。
	static const CWordTrie s_Trie;
	bool Changed = false;
	const char *pRead = pText;
	char *pWrite = pText;
	while(*pRead)
	{
		if(const char *pEnd = s_Trie.MatchEnd(pText, pRead))
		{
			// 最短规则为三个 ASCII 字节或两个汉字，固定三个星号不会增长输入。
			mem_copy(pWrite, "***", 3);
			pWrite += 3;
			pRead = pEnd;
			Changed = true;
		}
		else
		{
			const char *pNext = pRead;
			str_utf8_decode(&pNext);
			const size_t Length = pNext - pRead;
			mem_move(pWrite, pRead, Length);
			pWrite += Length;
			pRead = pNext;
		}
	}
	*pWrite = '\0';
	return Changed;
}

bool QmStreamerWordFilter::Filter(std::string &Text, bool Enabled)
{
	if(!Filter(Text.data(), Enabled))
		return false;
	Text.resize(str_length(Text.c_str()));
	return true;
}
