#pragma once

#include <cstdint>
#include <string>

namespace Solar::MonoBridge {

struct RuntimeMetadataProfile {
    uint32_t methodGetNameAddress = 0;
    const uint8_t *methodGetNameBytes = nullptr;
    uint32_t methodGetNameBytesSize = 0;

    uint32_t methodGetClassAddress = 0;
    const uint8_t *methodGetClassBytes = nullptr;
    uint32_t methodGetClassBytesSize = 0;

    uint32_t classGetNameAddress = 0;
    const uint8_t *classGetNameBytes = nullptr;
    uint32_t classGetNameBytesSize = 0;

    uint32_t classGetNamespaceAddress = 0;
    const uint8_t *classGetNamespaceBytes = nullptr;
    uint32_t classGetNamespaceBytesSize = 0;
};

struct CompileHookTarget {
    const uint64_t *titleIds = nullptr;
    uint32_t titleIdCount = 0;
    uint16_t versionMin = 0;
    uint16_t versionMax = 0xFFFF;
    const char *executableName = nullptr;

    // Adapter-owned data with static lifetime.
    const RuntimeMetadataProfile *runtimeProfile = nullptr;
    const char *const *interestingClasses = nullptr;
    uint32_t interestingClassCount = 0;
};

// Registers a trusted FunctionPatcher hook for mono_compile_method.
// Target arrays, strings and the runtime profile must have static lifetime.
bool RegisterCompileTraceHook(const std::string &hookId, const CompileHookTarget &target);

// Clears only runtime observations/profile state. Applied FunctionPatcher handles
// are owned by PatchEngine and hook registrations by NativeHookRegistry.
void ResetObservations();

} // namespace Solar::MonoBridge
