#ifndef GAME_EDITOR_EDITOR_SERVER_SETTINGS_COMPLETION_H
#define GAME_EDITOR_EDITOR_SERVER_SETTINGS_COMPLETION_H

#include <engine/keys.h>

#include <game/client/lineinput.h>
#include <game/client/ui.h>

// 按事件顺序记录修饰键与请求，避免同一帧松开 Ctrl 后丢失补全。
class CQmEditorSettingsCompletion
{
	unsigned m_Modifiers = 0;
	bool m_ModifiersObserved = false;
	CLineInput *m_pRequestedInput = nullptr;
	CLineInput *m_pSpaceInput = nullptr;
	uint32_t m_SpaceInputCount = 0;
	bool m_SpacePressObserved = false;
	bool m_SpaceHeld = false;

public:
	bool OnInput(CLineInput *pLineInput, const IInput &Input, const IInput::CEvent &Event)
	{
		if(Event.m_Key == KEY_SPACE && (Event.m_Flags & IInput::FLAG_PRESS))
			m_SpacePressObserved = true;
		unsigned Modifier = 0;
		switch(Event.m_Key)
		{
		case KEY_LCTRL:
			Modifier = 1;
			break;
		case KEY_RCTRL:
			Modifier = 2;
			break;
		case KEY_LGUI:
			Modifier = 4;
			break;
		case KEY_RGUI:
			Modifier = 8;
			break;
		}
		if(Modifier && (Event.m_Flags & (IInput::FLAG_PRESS | IInput::FLAG_RELEASE)))
		{
			m_ModifiersObserved = true;
			if(Event.m_Flags & IInput::FLAG_PRESS)
				m_Modifiers |= Modifier;
			if(Event.m_Flags & IInput::FLAG_RELEASE)
				m_Modifiers &= ~Modifier;
		}

		if(!pLineInput || CLineInput::GetActiveInput() != pLineInput || Input.HasComposition())
		{
			m_pRequestedInput = nullptr;
			m_pSpaceInput = nullptr;
			return false;
		}

		// 文本事件只与已收到的 Space 按下事件关联，不根据帧末键位猜测。
		if(m_pSpaceInput != pLineInput || m_SpaceInputCount != Event.m_InputCount)
			m_pSpaceInput = nullptr;
		if((Event.m_Flags & IInput::FLAG_TEXT) && m_pSpaceInput && str_comp(Event.m_aText, " ") == 0)
		{
			m_pSpaceInput = nullptr;
			return true;
		}

		const bool Modified = m_ModifiersObserved ? m_Modifiers != 0 : Input.ModifierIsPressed();
		if(!(Event.m_Flags & IInput::FLAG_PRESS) || !Modified || Input.AltIsPressed() ||
			Event.m_Key != KEY_SPACE)
			return false;
		if(!(Event.m_Flags & IInput::FLAG_REPEAT))
			m_pRequestedInput = pLineInput;
		m_pSpaceInput = pLineInput;
		m_SpaceInputCount = Event.m_InputCount;
		return true;
	}

	bool Request(CLineInput &LineInput, const IInput &Input, bool Clicked)
	{
		if(m_pRequestedInput != CLineInput::GetActiveInput() || Input.HasComposition())
			m_pRequestedInput = nullptr;
		const bool Requested = m_pRequestedInput == &LineInput;
		// 只补偿未路由到本组件的首次按下。帧状态也来自实际事件，不能恢复系统吞键。
		// 已路由的普通空格、重复按下，以及先按空格后按 Ctrl 都不能重新请求。
		const bool SpaceHeld = Input.KeyIsPressed(KEY_SPACE);
		const bool ShortcutPressed = !m_SpacePressObserved && !m_SpaceHeld &&
					     Input.KeyPress(KEY_SPACE) && SpaceHeld && Input.ModifierIsPressed() && !Input.AltIsPressed() && LineInput.IsActive();
		if(LineInput.IsActive())
		{
			m_SpaceHeld = SpaceHeld;
			m_SpacePressObserved = false;
		}
		if(Requested || Clicked || ShortcutPressed)
			m_pRequestedInput = nullptr;
		const bool CompletionRequested = Requested || ShortcutPressed;
		if(m_pSpaceInput == &LineInput)
			m_pSpaceInput = nullptr;
		// 窗口失焦可能没有松键事件，在绘制边界同步已释放的修饰键。
		if(!Input.ModifierIsPressed())
		{
			m_Modifiers = 0;
			m_ModifiersObserved = false;
		}
		if(Input.HasComposition() || (!Clicked && !(CompletionRequested && LineInput.IsActive())))
			return false;
		LineInput.Activate(EInputPriority::UI);
		return true;
	}
};

// 全部指令浏览与光标处的前缀补全分开；打开、关闭均不修改输入内容。
class CQmEditorSettingsCommandBrowser
{
	bool m_Open = false;

public:
	void Open() { m_Open = true; }
	void Close() { m_Open = false; }
	bool IsOpen() const { return m_Open; }
	static bool RestoreInputOnClick(CUi &Ui, CLineInput &LineInput, const CUIRect &DropdownRect, bool Selectable)
	{
		if(!Selectable || !Ui.MouseButtonClicked(0) || !Ui.MouseHovered(&DropdownRect))
			return false;
		Ui.SetActiveItem(&LineInput);
		LineInput.Activate(EInputPriority::UI);
		return true;
	}
	bool Matches(const char *pCommand, const char *pPrefix) const
	{
		return m_Open || str_startswith_nocase(pCommand, pPrefix);
	}
	bool Select(CLineInput &LineInput, const char *pCommand)
	{
		if(!m_Open || !pCommand || !pCommand[0])
			return false;
		LineInput.SetRange(pCommand, 0, LineInput.GetLength());
		Close();
		return true;
	}
};

#endif
