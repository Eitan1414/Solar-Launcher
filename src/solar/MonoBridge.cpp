#include "solar/MonoBridge.hpp"

#include "solar/Logger.hpp"
#include "solar/NativeHookRegistry.hpp"

#include <coreinit/memorymap.h>
#include <function_patcher/function_patching.h>

#include <cstdint>
#include <cstring>

namespace Solar::MonoBridge {
namespace {

using MonoCompileMethodFn = void *(*)(void *method);
using MonoMethodGetNameFn = const char *(*)(void *method);
using MonoMethodGetClassFn = void *(*)(void *method);
using MonoClassGetNameFn = const char *(*)(void *klass);
using MonoClassGetNamespaceFn = const char *(*)(void *klass);

// Verified from the symbol table of the legally extracted Cuphead Wii U
// Unity-master.rpx (Title ID 0005000021000000, title version 0).
// These are used only for read-only metadata helpers and are guarded by
// instruction-byte validation before the bridge calls through them.
constexpr uint32_t MonoMethodGetNameAddress = 0x0202713C;
constexpr uint32_t MonoMethodGetClassAddress = 0x02027144;
constexpr uint32_t MonoClassGetNameAddress = 0x02C50CB8;
constexpr uint32_t MonoClassGetNamespaceAddress = 0x02C50CC0;

constexpr uint8_t MonoMethodGetNameBytes[] = {0x80, 0x63, 0x00, 0x10, 0x4E, 0x80, 0x00, 0x20};
constexpr uint8_t MonoMethodGetClassBytes[] = {0x80, 0x63, 0x00, 0x08, 0x4E, 0x80, 0x00, 0x20};
constexpr uint8_t MonoClassGetNameBytes[] = {0x80, 0x63, 0x00, 0x30, 0x4E, 0x80, 0x00, 0x20};
constexpr uint8_t MonoClassGetNamespaceBytes[] = {0x80, 0x63, 0x00, 0x34, 0x4E, 0x80, 0x00, 0x20};

MonoCompileMethodFn real_mono_compile_method __attribute__((section(".data"))) = nullptr;

bool gRuntimeMetadataValidated = false;
bool gRuntimeMetadataRejected = false;
void *gObservedMethods[96] = {};
uint32_t gObservedMethodCount = 0;

bool BytesMatch(uint32_t address, const uint8_t *expected, uint32_t size) {
    if (!OSIsAddressValid(address) || !OSIsAddressValid(address + size - 1)) {
        return false;
    }
    return std::memcmp(reinterpret_cast<const void *>(static_cast<uintptr_t>(address)), expected, size) == 0;
}

bool ValidateRuntimeMetadataHelpers() {
    if (gRuntimeMetadataValidated) {
        return true;
    }
    if (gRuntimeMetadataRejected) {
        return false;
    }

    const bool valid =
        BytesMatch(MonoMethodGetNameAddress, MonoMethodGetNameBytes, sizeof(MonoMethodGetNameBytes)) &&
        BytesMatch(MonoMethodGetClassAddress, MonoMethodGetClassBytes, sizeof(MonoMethodGetClassBytes)) &&
        BytesMatch(MonoClassGetNameAddress, MonoClassGetNameBytes, sizeof(MonoClassGetNameBytes)) &&
        BytesMatch(MonoClassGetNamespaceAddress, MonoClassGetNamespaceBytes, sizeof(MonoClassGetNamespaceBytes));

    if (!valid) {
        gRuntimeMetadataRejected = true;
        Logger::Warn("Mono Bridge: Cuphead runtime metadata helper signatures did not match; trace metadata disabled");
        return false;
    }

    gRuntimeMetadataValidated = true;
    Logger::Info("Mono Bridge: Cuphead runtime metadata helpers validated");
    return true;
}

template <typename T>
T FunctionAt(uint32_t address) {
    return reinterpret_cast<T>(static_cast<uintptr_t>(address));
}

bool IsInterestingClass(const char *name) {
    if (name == nullptr) {
        return false;
    }

    static constexpr const char *Targets[] = {
        "PlayerManager",
        "PlayerInput",
        "AbstractPlayerController",
        "PlayerCameraController",
        "Level",
        "LevelHUD",
        "LevelHUDPlayer",
    };

    for (const char *target : Targets) {
        if (std::strcmp(name, target) == 0) {
            return true;
        }
    }
    return false;
}

bool AlreadyObserved(void *method) {
    for (uint32_t i = 0; i < gObservedMethodCount; ++i) {
        if (gObservedMethods[i] == method) {
            return true;
        }
    }
    return false;
}

void RememberObserved(void *method) {
    if (method == nullptr || AlreadyObserved(method)) {
        return;
    }
    if (gObservedMethodCount < (sizeof(gObservedMethods) / sizeof(gObservedMethods[0]))) {
        gObservedMethods[gObservedMethodCount++] = method;
    }
}

void TraceCompiledMethod(void *method, void *compiledCode) {
    if (method == nullptr || compiledCode == nullptr || AlreadyObserved(method)) {
        return;
    }
    if (!ValidateRuntimeMetadataHelpers()) {
        return;
    }

    const auto methodGetName = FunctionAt<MonoMethodGetNameFn>(MonoMethodGetNameAddress);
    const auto methodGetClass = FunctionAt<MonoMethodGetClassFn>(MonoMethodGetClassAddress);
    const auto classGetName = FunctionAt<MonoClassGetNameFn>(MonoClassGetNameAddress);
    const auto classGetNamespace = FunctionAt<MonoClassGetNamespaceFn>(MonoClassGetNamespaceAddress);

    void *klass = methodGetClass(method);
    if (klass == nullptr) {
        return;
    }

    const char *className = classGetName(klass);
    if (!IsInterestingClass(className)) {
        return;
    }

    const char *methodName = methodGetName(method);
    const char *namespaceName = classGetNamespace(klass);
    RememberObserved(method);

    Logger::Info("Mono Trace: %s%s%s::%s method=%p code=%p",
                 namespaceName != nullptr && namespaceName[0] != '\0' ? namespaceName : "",
                 namespaceName != nullptr && namespaceName[0] != '\0' ? "." : "",
                 className != nullptr ? className : "?",
                 methodName != nullptr ? methodName : "?",
                 method,
                 compiledCode);
}

void *my_mono_compile_method(void *method) {
    if (real_mono_compile_method == nullptr) {
        return nullptr;
    }

    void *compiledCode = real_mono_compile_method(method);
    TraceCompiledMethod(method, compiledCode);
    return compiledCode;
}

} // namespace

bool RegisterCompileTraceHook(const std::string &hookId, const CompileHookTarget &target) {
    if (hookId.empty() || target.titleIds == nullptr || target.titleIdCount == 0 ||
        target.executableName == nullptr || target.executableName[0] == '\0') {
        return false;
    }

    function_replacement_data_t data {};
    data.version = FUNCTION_REPLACEMENT_DATA_STRUCT_VERSION;
    data.type = FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_NAME;
    data.physicalAddr = 0;
    data.virtualAddr = 0;
    data.replaceAddr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&my_mono_compile_method));
    data.replaceCall = reinterpret_cast<uint32_t *>(&real_mono_compile_method);
    data.targetProcess = FP_TARGET_PROCESS_GAME;
    data.ReplaceInRPX.targetTitleIds = target.titleIds;
    data.ReplaceInRPX.targetTitleIdsCount = target.titleIdCount;
    data.ReplaceInRPX.versionMin = target.versionMin;
    data.ReplaceInRPX.versionMax = target.versionMax;
    data.ReplaceInRPX.executableName = target.executableName;
    data.ReplaceInRPX.textOffset = 0;
    data.ReplaceInRPX.functionName = "mono_compile_method";

    return NativeHookRegistry::Register(hookId, data);
}

void ResetObservations() {
    std::memset(gObservedMethods, 0, sizeof(gObservedMethods));
    gObservedMethodCount = 0;
    gRuntimeMetadataValidated = false;
    gRuntimeMetadataRejected = false;
}

} // namespace Solar::MonoBridge
