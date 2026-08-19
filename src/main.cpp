#include "solar/Logger.hpp"
#include "solar/ModManager.hpp"
#include "solar/Paths.hpp"
#include "solar/TitleManager.hpp"

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/storage.h>

WUPS_PLUGIN_NAME("Solar Launcher");
WUPS_PLUGIN_DESCRIPTION("Universal Wii U modding framework for Aroma.");
WUPS_PLUGIN_VERSION("v0.1.0-dev");
WUPS_PLUGIN_AUTHOR("Eitan1414");
WUPS_PLUGIN_LICENSE("Unlicensed");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("solar_launcher");

namespace {

constexpr const char *EnabledConfigId = "enabled";
constexpr bool DefaultEnabled = true;
bool gEnabled = DefaultEnabled;

void EnabledChanged(ConfigItemBoolean *, bool newValue) {
    gEnabled = newValue;
    if (WUPSStorageAPI_StoreBool(nullptr, EnabledConfigId, gEnabled) != WUPS_STORAGE_ERROR_SUCCESS) {
        Solar::Logger::Error("Failed to store enabled setting");
    }
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle root) {
    if (WUPSConfigItemBoolean_AddToCategory(root, EnabledConfigId, "Enable Solar Launcher",
                                            DefaultEnabled, gEnabled, &EnabledChanged) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback() {
    WUPSStorageAPI_SaveStorage(false);
}

} // namespace

INITIALIZE_PLUGIN() {
    WUPSConfigAPIOptionsV1 options = {.name = "Solar Launcher"};
    if (WUPSConfigAPI_Init(options, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        Solar::Logger::Error("Failed to initialize WUPS config API");
    }

    WUPSStorageError storageResult = WUPSStorageAPI_GetBool(nullptr, EnabledConfigId, &gEnabled);
    if (storageResult == WUPS_STORAGE_ERROR_NOT_FOUND) {
        gEnabled = DefaultEnabled;
        WUPSStorageAPI_StoreBool(nullptr, EnabledConfigId, gEnabled);
        WUPSStorageAPI_SaveStorage(false);
    } else if (storageResult != WUPS_STORAGE_ERROR_SUCCESS) {
        gEnabled = DefaultEnabled;
        Solar::Logger::Warn("Could not read enabled setting; using default");
    }

    Solar::Logger::Info("Solar Launcher core initialized");
}

DEINITIALIZE_PLUGIN() {
    Solar::Logger::Info("Solar Launcher core deinitialized");
}

ON_APPLICATION_START() {
    const uint64_t titleId = Solar::TitleManager::CurrentTitleId();
    const std::string titleIdText = Solar::TitleManager::FormatTitleId(titleId);

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

    const auto mods = Solar::ModManager::ScanForTitle(titleId);
    Solar::Logger::Info("Detected title %s with %u compatible mod(s)",
                        titleIdText.c_str(), static_cast<unsigned int>(mods.size()));

    for (const auto &mod : mods) {
        Solar::Logger::Info("Found mod: %s | author=%s | version=%s | type=%s",
                            mod.name.c_str(), mod.author.c_str(), mod.version.c_str(), mod.type.c_str());
    }
}

ON_APPLICATION_ENDS() {
    // Solar V0.1 does not keep per-title runtime state yet.
}
