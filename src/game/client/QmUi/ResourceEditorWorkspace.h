#ifndef GAME_CLIENT_QMUI_RESOURCEEDITORWORKSPACE_H
#define GAME_CLIENT_QMUI_RESOURCEEDITORWORKSPACE_H

namespace qm_resource_editor
{
	enum class EWorkspace
	{
		IMAGE = 0,
		AUDIO,
	};

	class CWorkspace
	{
	public:
		void Enter(int ReturnPage, int RequestedWorkspace = -1)
		{
			if(!m_Visible)
				m_ReturnPage = ReturnPage;
			m_Visible = true;
			Select(RequestedWorkspace);
		}
		bool Select(int Workspace)
		{
			if(Workspace < 0 || Workspace > 1 || Workspace == static_cast<int>(m_Selected))
				return false;
			m_Selected = static_cast<EWorkspace>(Workspace);
			return true;
		}
		void Suspend() { m_Visible = false; }
		bool Visible() const { return m_Visible; }
		EWorkspace Selected() const { return m_Selected; }
		int ReturnPage() const { return m_ReturnPage; }

	private:
		EWorkspace m_Selected = EWorkspace::IMAGE;
		bool m_Visible = false;
		int m_ReturnPage = -1;
	};
}

#endif
