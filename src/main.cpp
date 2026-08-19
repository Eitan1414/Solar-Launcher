#include "solar/Logger.hpp"
#include "solar/ModManager.hpp"
#include "solar/Paths.hpp"
#include "solar/RedirectEngine.hpp"
#include "solar/TitleManager.hpp"

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/storage.h>

WUPS_PLUGIN_NAME("Solar Launcher");
WUPS_PLUGIN_DESCRIPTION("Universal Wii U modding framework for Aroma.");
WUPS_PLUGIN_VERSION("v0.2.0-dev");
WUPS_PLUGIN_AUTHOR("Eitan1414");
WUPS_PLUGIN_LICENSE("Unlicensed");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("solar_launcher");

namespace {

constexpr const char *EnabledConfigId = "enabled";
constexpr const char *FileModsConfigId = "file_mods_enabled";
constexpr const char *LegacySDCafiineConfigId = "legacy_sdcafiine_enabled";

constexpr bool DefaultEnabled = true;
constexpr bool DefaultFileModsEnabled = true;
constexpr bool DefaultLegacySDCafiineEnabled = true;

bool gEnabled = DefaultEnabled;
bool gFileModsEnabled = DefaultFileModsEnabled;
bool gLegacySDCafiineEnabled = DefaultLegacySDCafiineEnabled;

void EnabledChanged(ConfigItemBoolean *, bool newValue) {
    gEnabled = newValue;
    if (WUPSStorageAPI_StoreBool(nullptr, EnabledConfigId, gEnabled) != WUPS_STORAGE_ERROR_SUCCESS) {
        Solar::Logger::Error("Failed to store enabled setting");
    }
}

void FileModsChanged(ConfigItemBoolean *, bool newValue) {
    gFileModsEnabled = newValue;
    if (WUPSStorageAPI_StoreBool(nullptr, FileModsConfigId, gFileModsEnabled) != WUPS_STORAGE_ERROR_SUCCESS) {
        Solar::Logger::Error("Failed to store file mods setting");
    }
}

void LegacySDCafiineChanged(ConfigItemBoolean *, bool newValue) {
    gLegacySDCafiineEnabled = newValue;
    if (WUPSStorageAPI_StoreBool(nullptr, LegacySDCafiineConfigId, gLegacySDCafiineEnabled) != WUPS_STORAGE_ERROR_SUCCESS) {
        Solar::Logger::Error("Failed to store SDCafiine compatibility setting");
    }
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle root) {
    if (WUPSConfigItemBoolean_AddToCategory(root, EnabledConfigId, "Enable Solar Launcher",
                                            DefaultEnabled, gEnabled, &EnabledChanged) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    if (WUPSConfigItemBoolean_AddToCategory(root, FileModsConfigId, "Enable file replacement mods",
                                            DefaultFileModsEnabled, gFileModsEnabled, &FileModsChanged) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    if (WUPSConfigItemBoolean_AddToCategory(root, LegacySDCafiineConfigId, "Detect SDCafiine packs",
                                            DefaultLegacySDCafiineEnabled, gLegacySDCafiineEnabled,
                                            &LegacySDCafiineChanged) != WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback() {
    WUPSStorageAPI_SaveStorage(false);
}

void LoadBoolSetting(const char *key, bool defaultValue, bool &target) {
    const WUPSStorageError result = WUPSStorageAPI_GetBool(nullptr, key, &target);
    if (result == WUPS_STORAGE_ERROR_NOT_FOUND) {
        target = defaultValue;
        WUPSStorageAPI_StoreBool(nullptr, key, target);
    } else if (result != WUPS_STORAGE_ERROR_SUCCESS) {
        target = defaultValue;
        Solar::Logger::Warn("Could not read setting %s; using default", key);
    }
}

} // namespace

INITIALIZE_PLUGIN() {
    WUPSConfigAPIOptionsV1 options = {.name = "Solar Launcher"};
    if (WUPSConfigAPI_Init(options, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        Solar::Logger::Error("Failed to initialize WUPS config API");
    }

    LoadBoolSetting(EnabledConfigId, DefaultEnabled, gEnabled);
    LoadBoolSetting(FileModsConfigId, DefaultFileModsEnabled, gFileModsEnabled);
    LoadBoolSetting(LegacySDCafiineConfigId, DefaultLegacySDCafiineEnabled, gLegacySDCafiineEnabled);
    WUPSStorageAPI_SaveStorage(false);

    Solar::RedirectEngine::Initialize();
    Solar::Logger::Info("Solar Launcher v0.2 core initialized");
}

DEINITIALIZE_PLUGIN() {
    Solar::RedirectEngine::Shutdown();
    Solar::Logger::Info("Solar Launcher v0.2 core deinitialized");
}

ON_APPLICATION_START() {
    const uint64_t titleId = Solar::TitleManager::CurrentTitleId();
    const std::string titleIdText = Solar::TitleManager::FormatTitleId(titleId);

    Solar::RedirectEngine::Clear();

    if (!Solar::TitleManager::IsGameTitle(titleId)) {
        Solar::Logger::Info("Ignoring non-game title %s", titleIdText.c_str());
        return;
    }

    if (!gEnabled) {
        Solar::Logger::Info("Solar disabled; title %s will start normally", titleIdText.c_str());
        return;
    }

    if (!Solar::Paths::EnsureBaseDirectories()) {
        Solar::Logger::Error("Could not initialize Solar SD directories");
        return;
    }

    if (!Solar::Paths::EnsureTitleDirectory(titleId)) {
        Solar::Logger::Error("Could not initialize mod directory for title %s", titleIdText.c_str());
        return;
    }

    const auto mods = Solar::ModManager::ScanForTitle(titleId, gLegacySDCafiineEnabled);
    Solar::Logger::Info("Detected title %s with %u compatible mod(s)",
                        titleIdText.c_str(), static_cast<unsigned int>(mods.size()));

    for (const auto &mod : mods) {
        Solar::Logger::Info("Found mod: %s | author=%s | version=%s | type=%s | enabled=%s | priority=%d | content=%s | aoc=%s%s",
                            mod.name.c_str(), mod.author.c_str(), mod.version.c_str(), mod.type.c_str(),
                            mod.enabled ? "yes" : "no", mod.priority,
                            mod.hasContent ? "yes" : "no", mod.hasAoc ? "yes" : "no",
                            mod.legacySDCafiine ? " | source=SDCafiine" : "");
    }

    if (!gFileModsEnabled) {
        Solar::Logger::Info("File replacement mods are disabled; launching without redirection");
        return;
    }

    if (!Solar::RedirectEngine::IsAvailable()) {
        Solar::Logger::Warn("File replacement engine unavailable; launching without redirection");
        return;
    }

    Solar::RedirectEngine::Apply(mods);
}

ON_APPLICATION_ENDS() {
    Solar::RedirectEngine::Clear();
}
