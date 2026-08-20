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

MonoCompileMethodFn real_mono_compile_method __attribute__((section(".data"))) = nullptr;

const RuntimeMetadataProfile *gRuntimeProfile = nullptr;
const char *const *gInterestingClasses = nullptr;
uint32_t gInterestingClassCount = 0;

bool gRuntimeMetadataValidated = false;
bool gRuntimeMetadataRejected = false;
void *gObservedMethods[96] = {};
uint32_t gObservedMethodCount = 0;

bool BytesMatch(uint32_t address, const uint8_t *expected, uint32_t size) {
    if (address == 0 || expected == nullptr || size == 0) {
        return false;
    }
    if (address > UINT32_MAX - (size - 1)) {
        return false;
    }
    if (!OSIsAddressValid(address) || !OSIsAddressValid(address + size - 1)) {
        return false;
    }
    return std::memcmp(reinterpret_cast<const void *>(static_cast<uintptr_t>(address)), expected, size) == 0;
}

bool ValidateRuntimeMetadataHelpers() {
    if (gRuntimeMetadataValidated) {
        return true;
    }
    if (gRuntimeMetadataRejected || gRuntimeProfile == nullptr) {
        return false;
    }

    const RuntimeMetadataProfile &p = *gRuntimeProfile;
    const bool valid =
        BytesMatch(p.methodGetNameAddress, p.methodGetNameBytes, p.methodGetNameBytesSize) &&
        BytesMatch(p.methodGetClassAddress, p.methodGetClassBytes, p.methodGetClassBytesSize) &&
        BytesMatch(p.classGetNameAddress, p.classGetNameBytes, p.classGetNameBytesSize) &&
        BytesMatch(p.classGetNamespaceAddress, p.classGetNamespaceBytes, p.classGetNamespaceBytesSize);

    if (!valid) {
        gRuntimeMetadataRejected = true;
        Logger::Warn("Mono Bridge: runtime metadata helper signatures did not match; trace metadata disabled");
        return false;
    }

    gRuntimeMetadataValidated = true;
    Logger::Info("Mono Bridge: runtime metadata helpers validated");
    return true;
}

template <typename T>
T FunctionAt(uint32_t address) {
    return reinterpret_cast<T>(static_cast<uintptr_t>(address));
}

bool IsInterestingClass(const char *name) {
    if (name == nullptr || gInterestingClasses == nullptr) {
        return false;
    }

    for (uint32_t i = 0; i < gInterestingClassCount; ++i) {
        const char *target = gInterestingClasses[i];
        if (target != nullptr && std::strcmp(name, target) == 0) {
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

    const RuntimeMetadataProfile &p = *gRuntimeProfile;
    const auto methodGetName = FunctionAt<MonoMethodGetNameFn>(p.methodGetNameAddress);
    const auto methodGetClass = FunctionAt<MonoMethodGetClassFn>(p.methodGetClassAddress);
    const auto classGetName = FunctionAt<MonoClassGetNameFn>(p.classGetNameAddress);
    const auto classGetNamespace = FunctionAt<MonoClassGetNamespaceFn>(p.classGetNamespaceAddress);

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
        target.executableName == nullptr || target.executableName[0] == '\0' ||
        target.runtimeProfile == nullptr || target.interestingClasses == nullptr ||
        target.interestingClassCount == 0) {
        return false;
    }

    gRuntimeProfile = target.runtimeProfile;
    gInterestingClasses = target.interestingClasses;
    gInterestingClassCount = target.interestingClassCount;

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
    gRuntimeProfile = nullptr;
    gInterestingClasses = nullptr;
    gInterestingClassCount = 0;
}

} // namespace Solar::MonoBridge
