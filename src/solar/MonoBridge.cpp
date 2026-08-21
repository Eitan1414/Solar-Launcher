#include "solar/MonoBridge.hpp"

#include "solar/Logger.hpp"
#include "solar/NativeHookRegistry.hpp"

#include <coreinit/dynload.h>
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

constexpr int32_t CompileSearchRadius = 0x100;
constexpr int32_t MetadataSearchRadius = 0x100;
constexpr uint32_t RawTraceLimit = 16;

MonoCompileMethodFn real_mono_compile_method __attribute__((section(".data"))) = nullptr;

RuntimeMetadataProfile gResolvedRuntimeProfile {};
const RuntimeMetadataProfile *gRuntimeProfile = nullptr;
const char *const *gInterestingClasses = nullptr;
uint32_t gInterestingClassCount = 0;

bool gRuntimeMetadataValidated = false;
bool gRuntimeMetadataRejected = false;
bool gCompileHookActiveLogged = false;
uint32_t gRawTraceCount = 0;
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

bool EndsWith(const char *value, const char *suffix) {
    if (value == nullptr || suffix == nullptr) {
        return false;
    }
    const size_t valueLength = std::strlen(value);
    const size_t suffixLength = std::strlen(suffix);
    if (suffixLength > valueLength) {
        return false;
    }
    return std::memcmp(value + valueLength - suffixLength, suffix, suffixLength) == 0;
}

bool AddSignedDelta(uint32_t address, int32_t delta, uint32_t &outAddress) {
    const int64_t resolved = static_cast<int64_t>(address) + static_cast<int64_t>(delta);
    if (resolved <= 0 || resolved > UINT32_MAX) {
        return false;
    }
    outAddress = static_cast<uint32_t>(resolved);
    return true;
}

uint32_t AbsoluteDistance(int32_t value) {
    return value < 0 ? static_cast<uint32_t>(-static_cast<int64_t>(value)) : static_cast<uint32_t>(value);
}

bool IsInsideText(const OSDynLoad_NotifyData &module, uint32_t address, uint32_t size) {
    if (address < module.textAddr || size == 0) {
        return false;
    }
    const uint32_t offset = address - module.textAddr;
    if (module.textSize == 0) {
        return true;
    }
    return offset < module.textSize && size <= module.textSize - offset;
}

bool FindLoadedExecutable(const char *expectedExecutable, OSDynLoad_NotifyData &outModule) {
    const int moduleCount = OSDynLoad_GetNumberOfRPLs();
    Logger::Info("Mono Bridge diagnostic: loaded module count=%d expected=%s",
                 moduleCount, expectedExecutable != nullptr ? expectedExecutable : "?");

    if (moduleCount <= 0) {
        Logger::Warn("Mono Bridge diagnostic: no loaded RPX/RPL modules were reported");
        return false;
    }

    constexpr int MaxModulesToLog = 32;
    OSDynLoad_NotifyData modules[MaxModulesToLog] {};
    const int toRead = moduleCount < MaxModulesToLog ? moduleCount : MaxModulesToLog;

    if (!OSDynLoad_GetRPLInfo(0, toRead, modules)) {
        Logger::Warn("Mono Bridge diagnostic: OSDynLoad_GetRPLInfo failed");
        return false;
    }

    bool found = false;
    for (int index = 0; index < toRead; ++index) {
        Logger::Info("Mono Bridge diagnostic: module[%d]=%s text=%08X textOffset=%08X textSize=%08X",
                     index,
                     modules[index].name != nullptr ? modules[index].name : "?",
                     static_cast<unsigned int>(modules[index].textAddr),
                     static_cast<unsigned int>(modules[index].textOffset),
                     static_cast<unsigned int>(modules[index].textSize));

        if (!found && EndsWith(modules[index].name, expectedExecutable)) {
            outModule = modules[index];
            found = true;
        }
    }

    if (moduleCount > MaxModulesToLog) {
        Logger::Info("Mono Bridge diagnostic: %d additional module(s) omitted",
                     moduleCount - MaxModulesToLog);
    }

    if (!found) {
        Logger::Warn("Mono Bridge diagnostic: executable %s was not found in loaded modules",
                     expectedExecutable != nullptr ? expectedExecutable : "?");
        return false;
    }

    Logger::Info("Mono Bridge Test1B: executable FOUND text=%08X offset=%08X size=%08X",
                 static_cast<unsigned int>(outModule.textAddr),
                 static_cast<unsigned int>(outModule.textOffset),
                 static_cast<unsigned int>(outModule.textSize));
    return true;
}

bool ResolveCompileMethod(const CompileHookTarget &target,
                          const OSDynLoad_NotifyData &module,
                          uint32_t &outRuntimeAddress,
                          uint32_t &outTextOffset,
                          int32_t &outRuntimeDelta) {
    if (target.compileMethodAddress < target.linkedTextBase) {
        Logger::Warn("Mono Bridge: invalid linked compile method/base relationship");
        return false;
    }

    const uint32_t linkedOffset = target.compileMethodAddress - target.linkedTextBase;
    if (module.textAddr > UINT32_MAX - linkedOffset) {
        Logger::Warn("Mono Bridge: compile method prediction overflow");
        return false;
    }

    const uint32_t predicted = module.textAddr + linkedOffset;
    uint32_t foundAddress = 0;
    uint32_t bestDistance = UINT32_MAX;
    uint32_t matchCount = 0;

    for (int32_t adjustment = -CompileSearchRadius; adjustment <= CompileSearchRadius; adjustment += 4) {
        const int64_t candidate64 = static_cast<int64_t>(predicted) + adjustment;
        if (candidate64 <= 0 || candidate64 > UINT32_MAX) {
            continue;
        }

        const uint32_t candidate = static_cast<uint32_t>(candidate64);
        if (!IsInsideText(module, candidate, target.compileMethodBytesSize)) {
            continue;
        }

        if (BytesMatch(candidate, target.compileMethodBytes, target.compileMethodBytesSize)) {
            ++matchCount;
            const uint32_t distance = AbsoluteDistance(adjustment);
            if (foundAddress == 0 || distance < bestDistance) {
                foundAddress = candidate;
                bestDistance = distance;
            }
        }
    }

    if (foundAddress == 0) {
        Logger::Warn("Mono Bridge: mono_compile_method signature not found near predicted runtime address %08X",
                     static_cast<unsigned int>(predicted));
        return false;
    }

    const int64_t delta64 = static_cast<int64_t>(foundAddress) -
                            static_cast<int64_t>(target.compileMethodAddress);
    if (delta64 < INT32_MIN || delta64 > INT32_MAX) {
        Logger::Warn("Mono Bridge: resolved runtime delta is out of range");
        return false;
    }

    outRuntimeAddress = foundAddress;
    outTextOffset = foundAddress - module.textAddr;
    outRuntimeDelta = static_cast<int32_t>(delta64);

    Logger::Info("Mono Bridge: mono_compile_method VERIFIED linked=%08X runtime=%08X textBase=%08X textOffset=%08X delta=%d matches=%u",
                 static_cast<unsigned int>(target.compileMethodAddress),
                 static_cast<unsigned int>(foundAddress),
                 static_cast<unsigned int>(module.textAddr),
                 static_cast<unsigned int>(outTextOffset),
                 static_cast<int>(outRuntimeDelta),
                 static_cast<unsigned int>(matchCount));
    return true;
}

bool ResolveHelperAddress(const char *label,
                          uint32_t linkedAddress,
                          const uint8_t *signature,
                          uint32_t signatureSize,
                          int32_t compileDelta,
                          const OSDynLoad_NotifyData &module,
                          uint32_t &outAddress) {
    uint32_t predicted = 0;
    if (!AddSignedDelta(linkedAddress, compileDelta, predicted)) {
        Logger::Warn("Mono Bridge metadata: %s prediction overflow", label);
        return false;
    }

    if (IsInsideText(module, predicted, signatureSize) && BytesMatch(predicted, signature, signatureSize)) {
        outAddress = predicted;
        Logger::Info("Mono Bridge metadata: %s MATCH linked=%08X runtime=%08X",
                     label,
                     static_cast<unsigned int>(linkedAddress),
                     static_cast<unsigned int>(outAddress));
        return true;
    }

    uint32_t bestAddress = 0;
    uint32_t bestDistance = UINT32_MAX;
    uint32_t matchCount = 0;
    int32_t bestAdjustment = 0;

    for (int32_t adjustment = -MetadataSearchRadius; adjustment <= MetadataSearchRadius; adjustment += 4) {
        const int64_t candidate64 = static_cast<int64_t>(predicted) + adjustment;
        if (candidate64 <= 0 || candidate64 > UINT32_MAX) {
            continue;
        }

        const uint32_t candidate = static_cast<uint32_t>(candidate64);
        if (!IsInsideText(module, candidate, signatureSize) || !BytesMatch(candidate, signature, signatureSize)) {
            continue;
        }

        ++matchCount;
        const uint32_t distance = AbsoluteDistance(adjustment);
        if (bestAddress == 0 || distance < bestDistance) {
            bestAddress = candidate;
            bestDistance = distance;
            bestAdjustment = adjustment;
        }
    }

    if (bestAddress == 0) {
        Logger::Warn("Mono Bridge metadata: %s MISS predicted=%08X (scan +/-0x%X)",
                     label,
                     static_cast<unsigned int>(predicted),
                     static_cast<unsigned int>(MetadataSearchRadius));
        return false;
    }

    outAddress = bestAddress;
    Logger::Info("Mono Bridge metadata: %s AUTO-RESOLVED predicted=%08X runtime=%08X adjust=%d matches=%u",
                 label,
                 static_cast<unsigned int>(predicted),
                 static_cast<unsigned int>(outAddress),
                 static_cast<int>(bestAdjustment),
                 static_cast<unsigned int>(matchCount));
    return true;
}

bool ResolveRuntimeMetadataProfile(const RuntimeMetadataProfile &source,
                                   int32_t compileDelta,
                                   const OSDynLoad_NotifyData &module) {
    gResolvedRuntimeProfile = source;

    const bool methodName = ResolveHelperAddress(
        "mono_method_get_name",
        source.methodGetNameAddress,
        source.methodGetNameBytes,
        source.methodGetNameBytesSize,
        compileDelta,
        module,
        gResolvedRuntimeProfile.methodGetNameAddress);

    const bool methodClass = ResolveHelperAddress(
        "mono_method_get_class",
        source.methodGetClassAddress,
        source.methodGetClassBytes,
        source.methodGetClassBytesSize,
        compileDelta,
        module,
        gResolvedRuntimeProfile.methodGetClassAddress);

    const bool className = ResolveHelperAddress(
        "mono_class_get_name",
        source.classGetNameAddress,
        source.classGetNameBytes,
        source.classGetNameBytesSize,
        compileDelta,
        module,
        gResolvedRuntimeProfile.classGetNameAddress);

    const bool classNamespace = ResolveHelperAddress(
        "mono_class_get_namespace",
        source.classGetNamespaceAddress,
        source.classGetNamespaceBytes,
        source.classGetNamespaceBytesSize,
        compileDelta,
        module,
        gResolvedRuntimeProfile.classGetNamespaceAddress);

    const bool allResolved = methodName && methodClass && className && classNamespace;
    if (!allResolved) {
        gRuntimeProfile = nullptr;
        gRuntimeMetadataRejected = true;
        Logger::Warn("Mono Bridge Test1B: metadata helpers incomplete; compile hook will continue in RAW trace mode");
        return false;
    }

    gRuntimeProfile = &gResolvedRuntimeProfile;
    gRuntimeMetadataRejected = false;
    Logger::Info("Mono Bridge Test1B: all runtime metadata helpers RESOLVED");
    return true;
}

void LogRuntimeMetadataProbe(const RuntimeMetadataProfile &p) {
    const bool methodName =
        BytesMatch(p.methodGetNameAddress, p.methodGetNameBytes, p.methodGetNameBytesSize);
    const bool methodClass =
        BytesMatch(p.methodGetClassAddress, p.methodGetClassBytes, p.methodGetClassBytesSize);
    const bool className =
        BytesMatch(p.classGetNameAddress, p.classGetNameBytes, p.classGetNameBytesSize);
    const bool classNamespace =
        BytesMatch(p.classGetNamespaceAddress, p.classGetNamespaceBytes, p.classGetNamespaceBytesSize);

    Logger::Info("Mono Bridge diagnostic: metadata probes name=%s methodClass=%s className=%s namespace=%s",
                 methodName ? "MATCH" : "MISS",
                 methodClass ? "MATCH" : "MISS",
                 className ? "MATCH" : "MISS",
                 classNamespace ? "MATCH" : "MISS");
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
        Logger::Warn("Mono Bridge: runtime metadata helper signatures changed after registration; named trace disabled");
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
    if (method == nullptr || compiledCode == nullptr) {
        return;
    }

    if (gRawTraceCount < RawTraceLimit) {
        Logger::Info("Mono Raw Trace[%u]: method=%p code=%p metadata=%s",
                     static_cast<unsigned int>(gRawTraceCount + 1),
                     method,
                     compiledCode,
                     gRuntimeProfile != nullptr ? "ready" : "unavailable");
        ++gRawTraceCount;
    }

    if (AlreadyObserved(method) || !ValidateRuntimeMetadataHelpers()) {
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
        Logger::Error("Mono Bridge: compile hook invoked without a valid trampoline");
        return nullptr;
    }

    if (!gCompileHookActiveLogged) {
        gCompileHookActiveLogged = true;
        Logger::Info("Mono Bridge: compile hook ACTIVE (first invocation)");
    }

    void *compiledCode = real_mono_compile_method(method);
    TraceCompiledMethod(method, compiledCode);
    return compiledCode;
}

} // namespace

bool RegisterCompileTraceHook(const std::string &hookId, const CompileHookTarget &target) {
    if (hookId.empty() || target.titleIds == nullptr || target.titleIdCount == 0 ||
        target.executableName == nullptr || target.executableName[0] == '\0' ||
        target.linkedTextBase == 0 || target.compileMethodAddress == 0 ||
        target.compileMethodBytes == nullptr || target.compileMethodBytesSize == 0 ||
        target.runtimeProfile == nullptr || target.interestingClasses == nullptr ||
        target.interestingClassCount == 0) {
        return false;
    }

    gInterestingClasses = target.interestingClasses;
    gInterestingClassCount = target.interestingClassCount;

    OSDynLoad_NotifyData module {};
    if (!FindLoadedExecutable(target.executableName, module)) {
        return false;
    }

    uint32_t resolvedRuntimeAddress = 0;
    uint32_t resolvedTextOffset = 0;
    int32_t runtimeDelta = 0;
    if (!ResolveCompileMethod(target, module, resolvedRuntimeAddress, resolvedTextOffset, runtimeDelta)) {
        Logger::Warn("Mono Bridge: refusing hook because the RPX compile-method signature was not verified");
        return false;
    }

    const bool metadataReady = ResolveRuntimeMetadataProfile(*target.runtimeProfile, runtimeDelta, module);
    if (metadataReady && gRuntimeProfile != nullptr) {
        LogRuntimeMetadataProbe(*gRuntimeProfile);
    }

    const uint32_t physicalAddress = static_cast<uint32_t>(OSEffectiveToPhysical(resolvedRuntimeAddress));
    if (physicalAddress == 0) {
        Logger::Warn("Mono Bridge Test1B: direct-address fallback unavailable because physical translation failed for %08X",
                     static_cast<unsigned int>(resolvedRuntimeAddress));
    } else {
        Logger::Info("Mono Bridge Test1B: direct fallback ARMED effective=%08X physical=%08X",
                     static_cast<unsigned int>(resolvedRuntimeAddress),
                     static_cast<unsigned int>(physicalAddress));
    }

    function_replacement_data_t data {};
    data.version = FUNCTION_REPLACEMENT_DATA_STRUCT_VERSION;
    data.type = FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS;
    // Solar's NativeHookRegistry uses these verified addresses only if the normal
    // executable+offset patch is registered but not active immediately.
    data.physicalAddr = physicalAddress;
    data.virtualAddr = resolvedRuntimeAddress;
    data.replaceAddr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&my_mono_compile_method));
    data.replaceCall = reinterpret_cast<uint32_t *>(&real_mono_compile_method);
    data.targetProcess = FP_TARGET_PROCESS_ALL;
    data.ReplaceInRPX.targetTitleIds = target.titleIds;
    data.ReplaceInRPX.targetTitleIdsCount = target.titleIdCount;
    data.ReplaceInRPX.versionMin = target.versionMin;
    data.ReplaceInRPX.versionMax = target.versionMax;
    data.ReplaceInRPX.executableName = target.executableName;
    data.ReplaceInRPX.textOffset = resolvedTextOffset;
    data.ReplaceInRPX.functionName = nullptr;

    Logger::Info("Mono Bridge Test1B summary: module=OK compileSignature=OK delta=%d metadata=%s textOffset=%08X",
                 static_cast<int>(runtimeDelta),
                 metadataReady ? "READY" : "RAW-ONLY",
                 static_cast<unsigned int>(resolvedTextOffset));
    Logger::Info("Mono Bridge: registering VERIFIED executable-offset hook %s for %s",
                 hookId.c_str(), target.executableName);

    return NativeHookRegistry::Register(hookId, data);
}

void ResetObservations() {
    std::memset(gObservedMethods, 0, sizeof(gObservedMethods));
    gObservedMethodCount = 0;
    gRawTraceCount = 0;
    gRuntimeMetadataValidated = false;
    gRuntimeMetadataRejected = false;
    gCompileHookActiveLogged = false;
    gResolvedRuntimeProfile = {};
    gRuntimeProfile = nullptr;
    gInterestingClasses = nullptr;
    gInterestingClassCount = 0;
}

} // namespace Solar::MonoBridge
