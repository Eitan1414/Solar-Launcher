#include "solar/RedirectEngine.hpp"
#include "solar/Logger.hpp"

#include <algorithm>
#include <content_redirection/redirection.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace Solar::RedirectEngine {
namespace {

bool gInitialized = false;
std::vector<CRLayerHandle> gLayers;

bool IsDirectory(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

const char *ChoosePackDirectory(const ModInfo &mod, const char *primary, const char *alias) {
    if (IsDirectory(mod.path + "/" + primary)) {
        return primary;
    }
    if (IsDirectory(mod.path + "/" + alias)) {
        return alias;
    }
    return nullptr;
}

bool AddLayer(const ModInfo &mod,
              const char *sourceSubdir,
              const char *targetLabel,
              FSLayerType layerType) {
    if (sourceSubdir == nullptr) {
        return false;
    }

    const std::string replacementPath = mod.path + "/" + sourceSubdir;
    if (!IsDirectory(replacementPath)) {
        return false;
    }

    CRLayerHandle handle = 0;
    const std::string layerName = "Solar: " + mod.name + " /vol/" + targetLabel + " <- " + sourceSubdir;
    const ContentRedirectionStatus result = ContentRedirection_AddFSLayer(
        &handle, layerName.c_str(), replacementPath.c_str(), layerType);

    if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        Logger::Error("Failed to add %s layer for %s: %s (%d)", sourceSubdir, mod.name.c_str(),
                      ContentRedirection_GetStatusStr(result), result);
        return false;
    }

    gLayers.push_back(handle);
    Logger::Info("Redirecting /vol/%s with %s (payload=%s priority=%d%s)",
                 targetLabel,
                 replacementPath.c_str(),
                 sourceSubdir,
                 mod.priority,
                 mod.legacySDCafiine ? ", SDCafiine" : "");
    return true;
}

} // namespace

bool Initialize() {
    if (gInitialized) {
        return true;
    }

    const ContentRedirectionStatus result = ContentRedirection_InitLibrary();
    if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        Logger::Warn("ContentRedirection unavailable: %s (%d). Games will launch without file mods.",
                     ContentRedirection_GetStatusStr(result), result);
        return false;
    }

    gInitialized = true;
    Logger::Info("ContentRedirection initialized");
    return true;
}

void Clear() {
    if (!gInitialized) {
        gLayers.clear();
        return;
    }

    for (auto it = gLayers.rbegin(); it != gLayers.rend(); ++it) {
        const ContentRedirectionStatus result = ContentRedirection_RemoveFSLayer(*it);
        if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
            Logger::Warn("Failed to remove redirection layer %u: %s (%d)",
                         static_cast<unsigned int>(*it), ContentRedirection_GetStatusStr(result), result);
        }
    }

    gLayers.clear();
}

void Shutdown() {
    if (!gInitialized) {
        return;
    }

    Clear();
    const ContentRedirectionStatus result = ContentRedirection_DeInitLibrary();
    if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        Logger::Warn("ContentRedirection deinit returned %s (%d)",
                     ContentRedirection_GetStatusStr(result), result);
    }

    gInitialized = false;
}

bool IsAvailable() {
    return gInitialized;
}

size_t Apply(const std::vector<ModInfo> &mods) {
    Clear();

    if (!gInitialized) {
        return 0;
    }

    std::vector<const ModInfo *> enabledMods;
    enabledMods.reserve(mods.size());

    for (const auto &mod : mods) {
        if (!mod.enabled ||
            (!mod.hasContent && !mod.hasAoc && !mod.hasTexturePack && !mod.hasBehaviorPack)) {
            continue;
        }
        enabledMods.push_back(&mod);
    }

    std::stable_sort(enabledMods.begin(), enabledMods.end(), [](const ModInfo *lhs, const ModInfo *rhs) {
        if (lhs->priority != rhs->priority) {
            return lhs->priority < rhs->priority;
        }
        return lhs->name < rhs->name;
    });

    size_t appliedMods = 0;
    for (const ModInfo *mod : enabledMods) {
        bool applied = false;
        if (mod->hasContent) {
            applied |= AddLayer(*mod, "content", "content", FS_LAYER_TYPE_CONTENT_MERGE);
        }
        if (mod->hasTexturePack) {
            const char *dir = ChoosePackDirectory(*mod, "textures", "texture_pack");
            applied |= AddLayer(*mod, dir, "content", FS_LAYER_TYPE_CONTENT_MERGE);
        }
        if (mod->hasBehaviorPack) {
            const char *dir = ChoosePackDirectory(*mod, "behavior", "behavior_pack");
            applied |= AddLayer(*mod, dir, "content", FS_LAYER_TYPE_CONTENT_MERGE);
        }
        if (mod->hasAoc) {
            applied |= AddLayer(*mod, "aoc", "aoc", FS_LAYER_TYPE_AOC_MERGE);
        }
        if (applied) {
            ++appliedMods;
        }
    }

    Logger::Info("Applied %u file/pack mod(s) using %u redirection layer(s)",
                 static_cast<unsigned int>(appliedMods),
                 static_cast<unsigned int>(gLayers.size()));
    return appliedMods;
}

} // namespace Solar::RedirectEngine
