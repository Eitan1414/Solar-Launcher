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
    bool legacySDCafiine = false;
};

class ModManager {
public:
    static std::vector<ModInfo> ScanForTitle(uint64_t titleId, bool includeLegacySDCafiine = true);
};

} // namespace Solar
