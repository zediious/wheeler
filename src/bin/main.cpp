#include "UserInput/Input.h"
#include "UserInput/Controls.h"

#include "Rendering/RenderManager.h"
#include "Rendering/TextureManager.h"

#include "Hooks.h"

#include "Wheeler/WheelItems/WheelItemMutableManager.h"
#include "Wheeler/Wheeler.h"
#include "Utilities/UniqueIDHandler.h"
#include "Serialization/SerializationEntry.h"

#include "Config.h"
#include "Texts.h"
#include "ModCallbackEventHandler.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		WheelItemMutableManager::GetSingleton()->Register();
		Config::ReadStyleConfig();
		Config::ReadControlConfig();
		Config::OffsetSizingToViewport();
		Controls::BindAllInputsFromConfig();
		Texture::Init();
		Texts::LoadTranslations();
		ModCallbackEventHandler::Register();
		break;
	case SKSE::MessagingInterface::kPostLoad:
		break;

	case SKSE::MessagingInterface::kSaveGame:
		break;
	case SKSE::MessagingInterface::kNewGame:
		Wheeler::SetupDefaultWheels();
	case SKSE::MessagingInterface::kPostLoadGame:
		UniqueIDHandler::EnsureXListUniquenessInPcInventory();
		break;
	}
}

void onSKSEInit()
{
	RenderManager::Install();
	Wheeler::Init();
	Hooks::Install();
	auto serialization = SKSE::GetSerializationInterface();
	serialization->SetUniqueID(WHEELER_SERIALIZATION_ID);
	SerializationEntry::BindSerializationCallbacks(serialization);
}

namespace
{
	void InitializeLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif
		const auto level = spdlog::level::trace;

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%s(%#): [%^%l%$] %v"s);
	}
}

std::string wstring2string(const std::wstring& wstr, UINT CodePage)

{

	std::string ret;

	int len = WideCharToMultiByte(CodePage, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);

	ret.resize((size_t)len, 0);

	WideCharToMultiByte(CodePage, 0, wstr.c_str(), (int)wstr.size(), &ret[0], len, NULL, NULL);

	return ret;

}


extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_SSE_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;

	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);

	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });
	v.HasNoStructUse(true);

	return v;
}();


extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	// REL::Module::reset();  // Clib-NG bug workaround
	//std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	InitializeLog();
	logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());

	SKSE::Init(a_skse);

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}
	
	onSKSEInit();


	return true;
}

extern "C" __declspec(dllexport)
bool IsWheelerOpen()
{
	return Wheeler::GetSingleton()->IsWheelerOpen();
}
