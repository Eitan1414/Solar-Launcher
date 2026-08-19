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
};

class ModManager {
public:
    static std::vector<ModInfo> ScanForTitle(uint64_t titleId);
};

} // namespace Solar
