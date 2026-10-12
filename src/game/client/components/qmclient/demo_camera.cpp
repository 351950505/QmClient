#include "demo_camera.h"

#include <base/log.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

void CQmDemoCamera::EnsureSession()
{
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const char *pFilename = DemoPlayer()->Filename();
	if(pFilename[0] == '\0' || m_Filename == pFilename)
		return;
	if(m_Dirty)
		Save();
	m_Filename = pFilename;
	m_Project = {};
	m_Sidecar.clear();
	m_Editing = false;
	m_Dirty = false;
	m_SaveFailed = false;
	m_EditFailed = false;
	m_PanelOpen = false;
	m_Selected = -1;
	// 浏览器可能明确选中了某个存储根，不能重新按 TYPE_ALL 找到同名的另一份 demo。
	m_Sidecar = m_PendingFilename == pFilename ? m_PendingSidecar : qm_demo_camera::ResolveSidecar(Storage(), pFilename, IStorage::TYPE_ALL_OR_ABSOLUTE);
	m_PendingFilename.clear();
	m_PendingSidecar.clear();
	m_LoadFailed = qm_demo_camera::Load(Storage(), m_Sidecar, m_Project) == qm_demo_camera::ELoadResult::FAILED;
	if(!Track().Empty())
		m_Selected = 0;
	GameClient()->m_TClient.SetForcedAspect();
}

void CQmDemoCamera::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_DEMOPLAYBACK)
		EnsureSession();
	else if(OldState == IClient::STATE_DEMOPLAYBACK)
	{
		if(m_Dirty)
			Save();
		m_Filename.clear();
		m_Sidecar.clear();
		m_Project = {};
		m_Editing = false;
		m_PanelOpen = false;
		m_Selected = -1;
		GameClient()->m_TClient.QueueAspectApply();
	}
}

void CQmDemoCamera::OnShutdown()
{
	if(m_Dirty)
		Save();
}

double CQmDemoCamera::CurrentTick() const
{
	return qm_demo_camera::PlaybackTick(Client()->PrevGameTick(0), Client()->GameTick(0), Client()->IntraGameTick(0));
}

float CQmDemoCamera::AspectOverride() const
{
	return Client()->State() == IClient::STATE_DEMOPLAYBACK && !Track().Empty() && (Enabled() || m_PanelOpen) ? Track().Aspect() : 0.0f;
}

std::optional<qm_demo_camera::SView> CQmDemoCamera::PlaybackView()
{
	EnsureSession();
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK || !Enabled() || m_Editing)
		return std::nullopt;
	return Track().Sample(CurrentTick());
}

void CQmDemoCamera::Select(int Index)
{
	m_Selected = Track().Empty() ? -1 : std::clamp(Index, 0, static_cast<int>(Track().Frames().size()) - 1);
}

void CQmDemoCamera::PrepareSource(const char *pFilename, int StorageType)
{
	m_PendingFilename = pFilename;
	m_PendingSidecar = qm_demo_camera::ResolveSidecar(Storage(), pFilename, StorageType);
}

void CQmDemoCamera::SetPanelOpen(bool Open)
{
	EnsureSession();
	m_PanelOpen = Open;
	GameClient()->m_TClient.SetForcedAspect();
}

void CQmDemoCamera::Changed()
{
	m_Dirty = true;
	m_LoadFailed = false;
	m_EditFailed = false;
	Save();
	GameClient()->m_TClient.SetForcedAspect();
}

bool CQmDemoCamera::Save()
{
	if(!m_Dirty)
		return !m_SaveFailed;
	m_SaveFailed = !qm_demo_camera::Save(Storage(), m_Sidecar, m_Project);
	if(!m_SaveFailed)
		m_Dirty = false;
	else
		log_error("demo/camera", "Failed to save camera path: %s", m_Sidecar.c_str());
	return !m_SaveFailed;
}

void CQmDemoCamera::SetEnabled(bool Enabled)
{
	EnsureSession();
	if(m_Project.m_Enabled == Enabled)
		return;
	m_Project.m_Enabled = Enabled;
	if(Enabled)
		m_Editing = false;
	Changed();
}

void CQmDemoCamera::BeginEditing()
{
	EnsureSession();
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const qm_demo_camera::SView View{GameClient()->m_Camera.m_Center, GameClient()->m_Camera.m_Zoom};
	m_Editing = true;
	DemoPlayer()->Pause();
	GameClient()->m_Spectator.Spectate(SPEC_FREEVIEW);
	GameClient()->m_Camera.SetDemoView(View.m_Center, View.m_Zoom);
	GameClient()->m_Camera.SetViewWorld(View.m_Center);
}

bool CQmDemoCamera::AddKeyframe()
{
	EnsureSession();
	const bool WasEmpty = Track().Empty();
	const qm_demo_camera::SKeyframe Frame{CurrentTick(), {GameClient()->m_Camera.m_Center, GameClient()->m_Camera.m_Zoom}};
	const int Index = m_Project.m_Track.Insert(Frame);
	if(Index < 0)
	{
		m_EditFailed = true;
		return false;
	}
	if(WasEmpty)
	{
		m_Project.m_Track.SetAspect(Graphics()->GameScreenAspect());
		m_Project.m_Enabled = true;
	}
	m_Selected = Index;
	Changed();
	return true;
}

bool CQmDemoCamera::UpdateKeyframe()
{
	if(m_Selected < 0 || m_Selected >= static_cast<int>(Track().Frames().size()))
		return false;
	auto Frame = Track().Frames()[m_Selected];
	Frame.m_View = {GameClient()->m_Camera.m_Center, GameClient()->m_Camera.m_Zoom};
	if(m_Project.m_Track.Insert(Frame) < 0)
	{
		m_EditFailed = true;
		return false;
	}
	Changed();
	return true;
}

void CQmDemoCamera::RemoveKeyframe()
{
	if(!m_Project.m_Track.Erase(m_Selected))
		return;
	Select(m_Selected);
	if(Track().Empty())
		m_Project.m_Enabled = false;
	Changed();
}

void CQmDemoCamera::CycleTransition()
{
	if(m_Selected < 0 || m_Selected >= static_cast<int>(Track().Frames().size()))
		return;
	auto Frame = Track().Frames()[m_Selected];
	Frame.m_Transition = static_cast<qm_demo_camera::ETransition>((static_cast<int>(Frame.m_Transition) + 1) % 3);
	Frame.m_EaseFrom = 0.0;
	Frame.m_EaseTo = 1.0;
	m_Project.m_Track.Insert(Frame);
	Changed();
}

bool CQmDemoCamera::GoToKeyframe()
{
	if(m_Selected < 0 || m_Selected >= static_cast<int>(Track().Frames().size()))
		return false;
	const auto Frame = Track().Frames()[m_Selected];
	BeginEditing();
	if(!GameClient()->m_Menus.DemoSeekToTick(std::min(static_cast<int>(Frame.m_Tick), std::numeric_limits<int>::max() - 1)))
		return false;
	DemoPlayer()->Pause();
	GameClient()->m_Camera.SetDemoView(Frame.m_View.m_Center, Frame.m_View.m_Zoom);
	GameClient()->m_Camera.SetViewWorld(Frame.m_View.m_Center);
	return true;
}

bool CQmDemoCamera::Preview()
{
	if(Track().Empty())
		return false;
	m_Editing = false;
	m_Project.m_Enabled = true;
	Changed();
	if(!GameClient()->m_Menus.DemoSeekToTick(std::min(static_cast<int>(Track().Frames().front().m_Tick), std::numeric_limits<int>::max() - 1)))
		return false;
	DemoPlayer()->Unpause();
	return true;
}

bool CQmDemoCamera::ExportCut(const char *pDemoPath, const std::vector<SDemoSliceSegment> &vSegments)
{
	if(Track().Empty())
	{
		// 覆盖已有剪辑时也清空旧镜头，避免给新文件套上上次导出的轨迹。
		qm_demo_camera::SProject Existing;
		const std::string Path = qm_demo_camera::SidecarPath(pDemoPath);
		return qm_demo_camera::Load(Storage(), Path, Existing) == qm_demo_camera::ELoadResult::MISSING || qm_demo_camera::Save(Storage(), Path, {});
	}
	std::vector<qm_demo_camera::SRange> vRanges;
	for(const auto &Segment : vSegments)
		vRanges.push_back({Segment.m_StartTick, Segment.m_EndTick});
	const auto Cut = Track().Cut(std::move(vRanges));
	return Cut.has_value() && qm_demo_camera::Save(Storage(), qm_demo_camera::SidecarPath(pDemoPath), {*Cut, Enabled()});
}

void CQmDemoCamera::OnRender()
{
	EnsureSession();
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK || !m_PanelOpen || !m_ShowGuides || Track().Empty())
		return;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() != nullptr)
		return;
#endif
	float X0, Y0, X1, Y1;
	Graphics()->GetScreen(&X0, &Y0, &X1, &Y1);
	Graphics()->MapScreenToGameInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, GameClient()->m_Camera.m_Zoom);
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	const auto &vFrames = Track().Frames();
	for(size_t i = 0; i < vFrames.size(); ++i)
	{
		const auto &Frame = vFrames[i];
		Graphics()->SetColor(i == static_cast<size_t>(m_Selected) ? ColorRGBA(1.0f, 0.8f, 0.25f, 0.9f) : ColorRGBA(0.4f, 0.7f, 1.0f, 0.45f));
		vec2 Size;
		Graphics()->CalcScreenParams(Track().Aspect(), Frame.m_View.m_Zoom, &Size.x, &Size.y);
		const vec2 Min = Frame.m_View.m_Center - Size * 0.5f;
		const vec2 Max = Frame.m_View.m_Center + Size * 0.5f;
		const IGraphics::CLineItem aFrame[] = {
			{Min.x, Min.y, Max.x, Min.y}, {Max.x, Min.y, Max.x, Max.y},
			{Max.x, Max.y, Min.x, Max.y}, {Min.x, Max.y, Min.x, Min.y}};
		Graphics()->LinesDraw(aFrame, std::size(aFrame));
		if(i + 1 < vFrames.size() && Frame.m_Transition != qm_demo_camera::ETransition::HOLD)
		{
			const IGraphics::CLineItem Path(Frame.m_View.m_Center.x, Frame.m_View.m_Center.y,
				vFrames[i + 1].m_View.m_Center.x, vFrames[i + 1].m_View.m_Center.y);
			Graphics()->LinesDraw(&Path, 1);
		}
	}
	Graphics()->LinesEnd();
	Graphics()->MapScreen(X0, Y0, X1, Y1);
}
