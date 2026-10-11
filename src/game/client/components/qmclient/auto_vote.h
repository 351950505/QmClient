#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_AUTO_VOTE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_AUTO_VOTE_H

#include <engine/shared/protocol.h>

inline int QmAutoVoteChoice(int Mode, int CallerId, int LocalId, bool CallerIsFriend, int LocalTeam, int CallerTeam)
{
	if(Mode != 1 && Mode != 2)
		return 0;
	const bool KnownCaller = CallerId >= 0 && CallerId < MAX_CLIENTS;
	if(KnownCaller && (CallerId == LocalId || CallerIsFriend || (LocalTeam != 0 && LocalTeam == CallerTeam)))
		return 0;
	return Mode == 2 ? 1 : -1;
}

#endif
