#pragma once

#include "solar/ModManager.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Solar {

struct PatchApplyReport {
    size_t patchFiles = 0;
    size_t memoryPatchesFound = 0;
    size_t memoryPatchesApplied = 0;
    size_t memoryPatchesSkipped = 0;
    size_t memoryPatchesFailed = 0;
    size_t hookRequestsFound = 0;
    size_t hooksApplied = 0;
    size_t hooksSkipped = 0;
};

namespace PatchEngine {

bool Initialize();
void Shutdown();
void Clear();

bool IsInitialized();
bool IsFunctionPatcherAvailable();

PatchApplyReport Apply(uint64_t titleId, const std::vector<ModInfo> &mods);

} // namespace PatchEngine
} // namespace Solar
