#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_H

#include "demo_camera_file.h"

#include <engine/demo.h>

#include <game/client/component.h>

class CQmDemoCamera : public CComponent
{
	qm_demo_camera::SProject m_Project;
	std::string m_Filename;
	std::string m_Sidecar;
	std::string m_PendingFilename;
	std::string m_PendingSidecar;
	bool m_Editing = false;
	bool m_Dirty = false;
	bool m_LoadFailed = false;
	bool m_SaveFailed = false;
	bool m_EditFailed = false;
	bool m_PanelOpen = false;
	bool m_ShowGuides = true;
	int m_Selected = -1;

	void EnsureSession();
	void Changed();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;
	void OnShutdown() override;

	const qm_demo_camera::CTrack &Track() const { return m_Project.m_Track; }
	bool Enabled() const { return m_Project.m_Enabled; }
	bool Editing() const { return m_Editing; }
	bool PanelOpen() const { return m_PanelOpen; }
	bool ShowGuides() const { return m_ShowGuides; }
	bool Dirty() const { return m_Dirty; }
	bool LoadFailed() const { return m_LoadFailed; }
	bool SaveFailed() const { return m_SaveFailed; }
	bool EditFailed() const { return m_EditFailed; }
	int Selected() const { return m_Selected; }
	void Select(int Index);
	void PrepareSource(const char *pFilename, int StorageType);
	void SetPanelOpen(bool Open);
	void ToggleGuides() { m_ShowGuides = !m_ShowGuides; }
	void SetEnabled(bool Enabled);
	void BeginEditing();
	bool AddKeyframe();
	bool UpdateKeyframe();
	void RemoveKeyframe();
	void CycleTransition();
	bool GoToKeyframe();
	bool Preview();
	bool Save();
	bool ExportCut(const char *pDemoPath, const std::vector<SDemoSliceSegment> &vSegments);
	double CurrentTick() const;
	float AspectOverride() const;
	std::optional<qm_demo_camera::SView> PlaybackView();
};

#endif // GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_H
