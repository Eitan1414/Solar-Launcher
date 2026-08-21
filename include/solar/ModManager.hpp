#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Solar {

struct ModInfo {
    std::string directoryName;
    std::string path;
    std::string name;
    std::string author;
    std::string version;
    std::string type;
    std::string declaredTitleId;

    bool enabled = true;
    int priority = 0;

    bool defaultEnabled = true;
    int defaultPriority = 0;

    bool hasContent = false;
    bool hasAoc = false;
    bool hasPatches = false;

    // Cuphead-specific convenience payloads. These are content-redirection
    // aliases with clearer semantics for pack creators:
    //   textures/ (or texture_pack/) -> /vol/content
    //   behavior/ (or behavior_pack/) -> /vol/content
    // Both directories mirror the game's /vol/content relative paths.
    bool hasTexturePack = false;
    bool hasBehaviorPack = false;

    bool legacySDCafiine = false;
};

class ModManager {
public:
    static std::vector<ModInfo> ScanForTitle(uint64_t titleId, bool includeLegacySDCafiine = true);
};

} // namespace Solar
