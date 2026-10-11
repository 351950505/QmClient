#ifndef ENGINE_CLIENT_INPUT_SDL3_H
#define ENGINE_CLIENT_INPUT_SDL3_H

#include <engine/keys.h>

#include <SDL3/SDL_scancode.h>

// 配置中的数字键位和键名沿用旧编号，不能直接把 SDL3 扫描码当作游戏键位。
inline int SdlScancodeToKey(SDL_Scancode Scancode)
{
	if(Scancode >= SDL_SCANCODE_UNKNOWN && Scancode <= SDL_SCANCODE_MODE)
		return static_cast<int>(Scancode);
	if(Scancode >= SDL_SCANCODE_SOFTLEFT && Scancode <= SDL_SCANCODE_ENDCALL)
		return KEY_SOFTLEFT + static_cast<int>(Scancode) - SDL_SCANCODE_SOFTLEFT;
	if(Scancode >= SDL_SCANCODE_AC_SEARCH && Scancode <= SDL_SCANCODE_AC_BOOKMARKS)
		return KEY_AC_SEARCH + static_cast<int>(Scancode) - SDL_SCANCODE_AC_SEARCH;
	switch(Scancode)
	{
	case SDL_SCANCODE_SLEEP: return KEY_SLEEP;
	case SDL_SCANCODE_MEDIA_PLAY:
	case SDL_SCANCODE_MEDIA_PLAY_PAUSE: return KEY_AUDIOPLAY;
	case SDL_SCANCODE_MEDIA_NEXT_TRACK: return KEY_AUDIONEXT;
	case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK: return KEY_AUDIOPREV;
	case SDL_SCANCODE_MEDIA_STOP: return KEY_AUDIOSTOP;
	case SDL_SCANCODE_MEDIA_SELECT: return KEY_MEDIASELECT;
	case SDL_SCANCODE_MEDIA_EJECT: return KEY_EJECT;
	case SDL_SCANCODE_MEDIA_REWIND: return KEY_AUDIOREWIND;
	case SDL_SCANCODE_MEDIA_FAST_FORWARD: return KEY_AUDIOFASTFORWARD;
	default: return KEY_UNKNOWN;
	}
}

#endif // ENGINE_CLIENT_INPUT_SDL3_H
