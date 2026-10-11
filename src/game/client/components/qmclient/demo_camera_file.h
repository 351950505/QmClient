#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_FILE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_FILE_H

#include "demo_camera_path.h"

#include <base/system.h>

#include <engine/shared/json.h>
#include <engine/storage.h>

#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#include <string>

namespace qm_demo_camera
{

	struct SProject
	{
		CTrack m_Track;
		bool m_Enabled = false;
	};

	inline std::string Encode(const SProject &Project)
	{
		std::ostringstream Stream;
		Stream.imbue(std::locale::classic());
		Stream << std::setprecision(std::numeric_limits<double>::max_digits10);
		Stream << "{\"version\":1,\"enabled\":" << (Project.m_Enabled ? "true" : "false")
		       << ",\"aspect\":" << Project.m_Track.Aspect() << ",\"keyframes\":[";
		bool First = true;
		for(const auto &Frame : Project.m_Track.Frames())
		{
			if(!First)
				Stream << ',';
			First = false;
			Stream << "{\"tick\":" << Frame.m_Tick << ",\"x\":" << Frame.m_View.m_Center.x
			       << ",\"y\":" << Frame.m_View.m_Center.y << ",\"zoom\":" << Frame.m_View.m_Zoom
			       << ",\"transition\":" << static_cast<int>(Frame.m_Transition)
			       << ",\"ease_from\":" << Frame.m_EaseFrom << ",\"ease_to\":" << Frame.m_EaseTo << '}';
		}
		Stream << "]}\n";
		return Stream.str();
	}

	inline bool ReadNumber(const json_value *pObject, const char *pName, double &Value)
	{
		const json_value *pField = json_object_get(pObject, pName);
		if(pField == nullptr || (pField->type != json_double && pField->type != json_integer))
			return false;
		Value = pField->type == json_double ? pField->u.dbl : static_cast<double>(pField->u.integer);
		return std::isfinite(Value);
	}

	inline bool Decode(const std::string &Json, SProject &Project)
	{
		if(Json.size() > 512 * 1024)
			return false;
		json_settings Settings{};
		Settings.max_memory = 4 * 1024 * 1024;
		std::unique_ptr<json_value, decltype(&json_value_free)> pRoot(JsonParseEx(&Settings, Json.data(), Json.size(), nullptr), json_value_free);
		if(!pRoot || pRoot->type != json_object)
			return false;
		const auto *pVersion = json_object_get(pRoot.get(), "version");
		const auto *pEnabled = json_object_get(pRoot.get(), "enabled");
		const auto *pFrames = json_object_get(pRoot.get(), "keyframes");
		double Aspect;
		if(pVersion == nullptr || pVersion->type != json_integer || pVersion->u.integer != 1 ||
			pEnabled == nullptr || pEnabled->type != json_boolean ||
			pFrames == nullptr || pFrames->type != json_array || pFrames->u.array.length > CTrack::MAX_KEYFRAMES ||
			!ReadNumber(pRoot.get(), "aspect", Aspect))
			return false;
		SProject Parsed;
		if(!Parsed.m_Track.SetAspect(static_cast<float>(Aspect)))
			return false;
		Parsed.m_Enabled = pEnabled->u.boolean != 0;
		double PreviousTick = -1.0;
		for(unsigned i = 0; i < pFrames->u.array.length; ++i)
		{
			const auto *pFrame = pFrames->u.array.values[i];
			double Tick, X, Y, Zoom, EaseFrom, EaseTo;
			if(pFrame == nullptr || pFrame->type != json_object || !ReadNumber(pFrame, "tick", Tick) ||
				!ReadNumber(pFrame, "x", X) || !ReadNumber(pFrame, "y", Y) || !ReadNumber(pFrame, "zoom", Zoom) ||
				!ReadNumber(pFrame, "ease_from", EaseFrom) || !ReadNumber(pFrame, "ease_to", EaseTo))
				return false;
			const auto *pTransition = json_object_get(pFrame, "transition");
			if(pTransition == nullptr || pTransition->type != json_integer || pTransition->u.integer < 0 || pTransition->u.integer > 2 || Tick <= PreviousTick)
				return false;
			if(Parsed.m_Track.Insert({Tick, {{static_cast<float>(X), static_cast<float>(Y)}, static_cast<float>(Zoom)},
				   static_cast<ETransition>(pTransition->u.integer), EaseFrom, EaseTo}) < 0)
				return false;
			PreviousTick = Tick;
		}
		Project = std::move(Parsed);
		return true;
	}

	inline std::string SidecarPath(const std::string &DemoPath)
	{
		const std::string Path = DemoPath + ".qm-camera.json";
		return DemoPath.empty() || Path.size() + 4 >= IO_MAX_PATH_LENGTH ? std::string() : Path;
	}

	inline std::string ResolveSidecar(IStorage *pStorage, const char *pFilename, int StorageType)
	{
		char aCompletePath[IO_MAX_PATH_LENGTH];
		IOHANDLE Demo = pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType, aCompletePath, sizeof(aCompletePath));
		if(Demo == nullptr)
			return {};
		io_close(Demo);
		std::string Path = aCompletePath;
		if(fs_is_relative_path(Path.c_str()))
		{
			char aDirectory[IO_MAX_PATH_LENGTH];
			if(!fs_getcwd(aDirectory, sizeof(aDirectory)))
				return {};
			Path = std::string(aDirectory) + "/" + Path;
		}
		return SidecarPath(Path);
	}

	enum class ELoadResult
	{
		MISSING,
		LOADED,
		FAILED,
	};

	inline ELoadResult Load(IStorage *pStorage, const std::string &Path, SProject &Project)
	{
		if(Path.empty())
			return ELoadResult::FAILED;
		IOHANDLE File = pStorage->OpenFile(Path.c_str(), IOFLAG_READ, IStorage::TYPE_ALL_OR_ABSOLUTE);
		if(File == nullptr)
			return pStorage->FileExists(Path.c_str(), IStorage::TYPE_ALL_OR_ABSOLUTE) ? ELoadResult::FAILED : ELoadResult::MISSING;
		const int64_t Length = io_length(File);
		if(Length < 0 || Length > 512 * 1024)
		{
			io_close(File);
			return ELoadResult::FAILED;
		}
		std::string Json(static_cast<size_t>(Length), '\0');
		const bool Read = io_read(File, Json.data(), static_cast<unsigned>(Json.size())) == Json.size();
		io_close(File);
		return Read && Decode(Json, Project) ? ELoadResult::LOADED : ELoadResult::FAILED;
	}

	inline bool Save(IStorage *pStorage, const std::string &Path, const SProject &Project)
	{
		if(Path.empty() || Path.size() + 4 >= IO_MAX_PATH_LENGTH)
			return false;
		const std::string Temporary = Path + ".tmp";
		const std::string Json = Encode(Project);
		IOHANDLE File = pStorage->OpenFile(Temporary.c_str(), IOFLAG_WRITE, IStorage::TYPE_SAVE_OR_ABSOLUTE);
		if(File == nullptr)
			return false;
		const bool Written = io_write(File, Json.data(), static_cast<unsigned>(Json.size())) == Json.size();
		const bool Closed = io_close(File) == 0;
		// 先完整写入再替换；替换失败保留已有镜头文件，允许用户重试。
		return Written && Closed && pStorage->RenameFile(Temporary.c_str(), Path.c_str(), fs_is_relative_path(Path.c_str()) ? IStorage::TYPE_SAVE : IStorage::TYPE_ABSOLUTE, false);
	}

} // namespace qm_demo_camera

#endif // GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_FILE_H
