#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_STREAMER_WORD_FILTER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_STREAMER_WORD_FILTER_H

#include <string>

namespace QmStreamerWordFilter
{
	// 只替换命中片段，不增长缓冲区；调用方决定是否启用，不修改网络输入。
	bool Filter(char *pText, bool Enabled);
	bool Filter(std::string &Text, bool Enabled);
}

#endif
