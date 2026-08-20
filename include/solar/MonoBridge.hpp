#pragma once

#include <cstdint>
#include <string>

namespace Solar::MonoBridge {

struct CompileHookTarget {
    const uint64_t *titleIds = nullptr;
    uint32_t titleIdCount = 0;
    uint16_t versionMin = 0;
    uint16_t versionMax = 0xFFFF;
    const char *executableName = nullptr;
};

// Registers a trusted FunctionPatcher hook for mono_compile_method.
// The target arrays/strings must have static lifetime because FunctionPatcher
// stores pointers to them until the hook is removed.
bool RegisterCompileTraceHook(const std::string &hookId, const CompileHookTarget &target);

// Clears only runtime observations. Applied FunctionPatcher handles are owned
// by PatchEngine and registrations are owned by NativeHookRegistry.
void ResetObservations();

} // namespace Solar::MonoBridge
