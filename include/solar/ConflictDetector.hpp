#pragma once

#include "solar/ModManager.hpp"

#include <cstddef>
#include <vector>

namespace Solar {

struct ConflictReport {
    size_t conflictingPaths = 0;
    std::vector<size_t> perMod;
    bool truncated = false;
};

class ConflictDetector {
public:
    static ConflictReport Analyze(const std::vector<ModInfo> &mods);
};

} // namespace Solar
