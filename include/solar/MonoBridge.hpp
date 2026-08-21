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

    // Link-time .text information verified from the supplied RPX.
    uint32_t linkedTextBase = 0;
    uint32_t compileMethodAddress = 0;
    const uint8_t *compileMethodBytes = nullptr;
    uint32_t compileMethodBytesSize = 0;

    // Adapter-owned data with static lifetime.
    const RuntimeMetadataProfile *runtimeProfile = nullptr;
    const char *const *interestingClasses = nullptr;
    uint32_t interestingClassCount = 0;
};

// Registers a trusted FunctionPatcher hook for mono_compile_method. The bridge
// first validates the method signature in the loaded RPX and resolves the
// runtime text delta before registering an address-based patch.
bool RegisterCompileTraceHook(const std::string &hookId, const CompileHookTarget &target);

// Clears only runtime observations/profile state. Applied FunctionPatcher handles
// are owned by PatchEngine and hook registrations by NativeHookRegistry.
void ResetObservations();

} // namespace Solar::MonoBridge
