#include "solar/CupheadPlayer3.hpp"

#include "solar/CupheadAdapter.hpp"
#include "solar/Logger.hpp"
#include "solar/NativeHookRegistry.hpp"

#include <coreinit/dynload.h>
#include <coreinit/memorymap.h>
#include <function_patcher/function_patching.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace Solar::CupheadPlayer3 {
namespace {

using MonoCompileMethodFn = void *(*)(void *method);
using MonoMethodGetNameFn = const char *(*)(void *method);
using MonoMethodGetClassFn = void *(*)(void *method);
using MonoMethodGetFlagsFn = uint32_t (*)(void *method, uint32_t *iflags);
using MonoMethodSignatureFn = void *(*)(void *method);
using MonoClassGetNameFn = const char *(*)(void *klass);
using MonoClassGetNamespaceFn = const char *(*)(void *klass);
using MonoClassGetImageFn = void *(*)(void *klass);
using MonoClassFromNameFn = void *(*)(void *image, const char *nameSpace, const char *name);
using MonoClassGetMethodsFn = void *(*)(void *klass, void **iter);
using MonoSignatureGetParamCountFn = uint32_t (*)(void *signature);
using MonoSignatureGetParamsFn = void *(*)(void *signature, void **iter);
using MonoSignatureGetReturnTypeFn = void *(*)(void *signature);
using MonoTypeGetNameFn = char *(*)(void *type);

constexpr uint32_t LinkedTextBase = 0x02000000;
constexpr uint32_t MonoCompileMethodLinked = 0x02067430;
constexpr uint32_t MonoCompileTextOffset = MonoCompileMethodLinked - LinkedTextBase;
constexpr uint8_t MonoCompileStableBytes[] = {
    0x7C, 0x08, 0x02, 0xA6,
    0x90, 0x01, 0x00, 0x04,
    0x94, 0x21, 0xFF, 0xF8,
};

// Verified symbols from the supplied Unity-master.rpx.
constexpr uint32_t MonoMethodSignatureLinked = 0x0202524C;
constexpr uint32_t MonoMethodGetNameLinked = 0x0202713C;
constexpr uint32_t MonoMethodGetClassLinked = 0x02027144;
constexpr uint32_t MonoMethodGetFlagsLinked = 0x02027338;
constexpr uint32_t MonoSignatureGetParamsLinked = 0x02044598;
constexpr uint32_t MonoSignatureGetReturnTypeLinked = 0x020469F4;
constexpr uint32_t MonoSignatureGetParamCountLinked = 0x0204AEB8;
constexpr uint32_t MonoClassGetImageLinked = 0x02C475EC;
constexpr uint32_t MonoTypeGetNameLinked = 0x02C47940;
constexpr uint32_t MonoClassFromNameLinked = 0x02C4DB20;
constexpr uint32_t MonoClassGetMethodsLinked = 0x02C50E10;
constexpr uint32_t MonoClassGetNameLinked = 0x02C50CB8;
constexpr uint32_t MonoClassGetNamespaceLinked = 0x02C50CC0;

constexpr uint32_t MethodAttributeStatic = 0x0010;
constexpr uint32_t MaxMethodsPerClass = 160;
constexpr uint32_t MaxSignatureParams = 12;

MonoCompileMethodFn real_mono_compile_method_p3 __attribute__((section(".data"))) = nullptr;
int32_t gRuntimeDelta = 0;
bool gRuntimeReady = false;
bool gSurfaceDumped = false;
bool gHookActiveLogged = false;

constexpr const char *TargetClasses[] = {
    "PlayerManager",
    "PlayerInput",
    "AbstractPlayerController",
    "PlayerCameraController",
    "Level",
    "LevelHUD",
    "LevelHUDPlayer",
    "PlayerSuperGhost",
    "CreatePlayerTwoOnJoin",
};

bool EndsWith(const char *value, const char *suffix) {
    if (value == nullptr || suffix == nullptr) {
        return false;
    }
    const size_t valueLength = std::strlen(value);
    const size_t suffixLength = std::strlen(suffix);
    return suffixLength <= valueLength &&
           std::memcmp(value + valueLength - suffixLength, suffix, suffixLength) == 0;
}

bool BytesMatch(uint32_t address, const uint8_t *bytes, uint32_t size) {
    if (address == 0 || bytes == nullptr || size == 0 ||
        address > UINT32_MAX - (size - 1)) {
        return false;
    }
    if (!OSIsAddressValid(address) || !OSIsAddressValid(address + size - 1)) {
        return false;
    }
    return std::memcmp(reinterpret_cast<const void *>(static_cast<uintptr_t>(address)), bytes, size) == 0;
}

uint32_t RuntimeAddress(uint32_t linkedAddress) {
    const int64_t address = static_cast<int64_t>(linkedAddress) + gRuntimeDelta;
    if (address <= 0 || address > UINT32_MAX) {
        return 0;
    }
    return static_cast<uint32_t>(address);
}

template <typename T>
T RuntimeFunction(uint32_t linkedAddress) {
    const uint32_t address = RuntimeAddress(linkedAddress);
    return reinterpret_cast<T>(static_cast<uintptr_t>(address));
}

bool IsTargetClass(const char *name) {
    if (name == nullptr) {
        return false;
    }
    for (const char *target : TargetClasses) {
        if (std::strcmp(name, target) == 0) {
            return true;
        }
    }
    return false;
}

bool IsCandidateMethod(const char *name) {
    if (name == nullptr) {
        return false;
    }

    constexpr const char *Candidates[] = {
        "CreatePlayerTwoOnJoin",
        "SetupPlayerTwo",
        "RevivePlayer",
        "OnPlayerJoined",
        "OnPlayerDeath",
        "OnPlayerRevive",
        "OnPreRevive",
        "CreatePlayer",
        "SetupPlayer",
        "SpawnPlayer",
        "JoinPlayer",
        "AddPlayer",
        "GetPlayer",
    };

    for (const char *candidate : Candidates) {
        if (std::strcmp(name, candidate) == 0) {
            return true;
        }
    }
    return false;
}

std::string TypeName(MonoTypeGetNameFn typeGetName, void *type) {
    if (type == nullptr || typeGetName == nullptr) {
        return "?";
    }
    char *name = typeGetName(type);
    if (name == nullptr || name[0] == '\0') {
        return "?";
    }
    // Test 2 intentionally avoids depending on GLib's allocator surface. The
    // dump runs once and the small strings returned here are acceptable for a
    // one-shot diagnostic build.
    return std::string(name);
}

std::string BuildSignature(void *method, uint32_t &outFlags) {
    const auto methodSignature = RuntimeFunction<MonoMethodSignatureFn>(MonoMethodSignatureLinked);
    const auto signatureGetParamCount = RuntimeFunction<MonoSignatureGetParamCountFn>(MonoSignatureGetParamCountLinked);
    const auto signatureGetParams = RuntimeFunction<MonoSignatureGetParamsFn>(MonoSignatureGetParamsLinked);
    const auto signatureGetReturnType = RuntimeFunction<MonoSignatureGetReturnTypeFn>(MonoSignatureGetReturnTypeLinked);
    const auto typeGetName = RuntimeFunction<MonoTypeGetNameFn>(MonoTypeGetNameLinked);
    const auto methodGetFlags = RuntimeFunction<MonoMethodGetFlagsFn>(MonoMethodGetFlagsLinked);

    outFlags = 0;
    if (methodGetFlags != nullptr) {
        uint32_t implFlags = 0;
        outFlags = methodGetFlags(method, &implFlags);
    }

    if (methodSignature == nullptr || signatureGetParamCount == nullptr ||
        signatureGetParams == nullptr || signatureGetReturnType == nullptr ||
        typeGetName == nullptr) {
        return "(?)";
    }

    void *signature = methodSignature(method);
    if (signature == nullptr) {
        return "(?)";
    }

    std::string result = "(";
    const uint32_t paramCount = signatureGetParamCount(signature);
    void *iter = nullptr;
    const uint32_t count = paramCount < MaxSignatureParams ? paramCount : MaxSignatureParams;
    for (uint32_t index = 0; index < count; ++index) {
        void *paramType = signatureGetParams(signature, &iter);
        if (index != 0) {
            result += ", ";
        }
        result += TypeName(typeGetName, paramType);
    }
    if (paramCount > MaxSignatureParams) {
        result += ", ...";
    }
    result += ") -> ";
    result += TypeName(typeGetName, signatureGetReturnType(signature));
    return result;
}

void DumpClass(void *image,
               const char *fallbackNamespace,
               const char *className) {
    const auto classFromName = RuntimeFunction<MonoClassFromNameFn>(MonoClassFromNameLinked);
    const auto classGetMethods = RuntimeFunction<MonoClassGetMethodsFn>(MonoClassGetMethodsLinked);
    const auto methodGetName = RuntimeFunction<MonoMethodGetNameFn>(MonoMethodGetNameLinked);

    if (classFromName == nullptr || classGetMethods == nullptr || methodGetName == nullptr) {
        return;
    }

    void *klass = classFromName(image, "", className);
    if (klass == nullptr && fallbackNamespace != nullptr && fallbackNamespace[0] != '\0') {
        klass = classFromName(image, fallbackNamespace, className);
    }

    if (klass == nullptr) {
        Logger::Info("P3 Surface: class %s NOT FOUND", className);
        return;
    }

    Logger::Info("P3 Surface: class %s FOUND klass=%p", className, klass);

    void *iter = nullptr;
    uint32_t methodCount = 0;
    while (methodCount < MaxMethodsPerClass) {
        void *method = classGetMethods(klass, &iter);
        if (method == nullptr) {
            break;
        }

        ++methodCount;
        const char *methodName = methodGetName(method);
        uint32_t flags = 0;
        const std::string signature = BuildSignature(method, flags);
        const bool isStatic = (flags & MethodAttributeStatic) != 0;

        Logger::Info("P3 Method: %s::%s %s flags=%08X %s method=%p",
                     className,
                     methodName != nullptr ? methodName : "?",
                     signature.c_str(),
                     static_cast<unsigned int>(flags),
                     isStatic ? "STATIC" : "INSTANCE",
                     method);

        if (IsCandidateMethod(methodName)) {
            void *nativeCode = nullptr;
            if (real_mono_compile_method_p3 != nullptr) {
                nativeCode = real_mono_compile_method_p3(method);
            }
            Logger::Info("P3 CANDIDATE: %s::%s %s %s native=%p method=%p",
                         className,
                         methodName != nullptr ? methodName : "?",
                         signature.c_str(),
                         isStatic ? "STATIC" : "INSTANCE",
                         nativeCode,
                         method);
        }
    }

    Logger::Info("P3 Surface: class %s methods=%u",
                 className,
                 static_cast<unsigned int>(methodCount));
}

void DumpPlayer3Surface(void *seedClass, const char *seedNamespace) {
    if (gSurfaceDumped || seedClass == nullptr) {
        return;
    }

    const auto classGetImage = RuntimeFunction<MonoClassGetImageFn>(MonoClassGetImageLinked);
    if (classGetImage == nullptr) {
        return;
    }

    void *image = classGetImage(seedClass);
    if (image == nullptr) {
        return;
    }

    gSurfaceDumped = true;
    Logger::Info("P3 TEST2: managed gameplay image acquired=%p; dumping Player 3 surface", image);

    for (const char *className : TargetClasses) {
        DumpClass(image, seedNamespace, className);
    }

    Logger::Info("P3 TEST2: Player 3 managed surface dump COMPLETE");
}

void ObserveMethod(void *method, void *compiledCode) {
    const auto methodGetClass = RuntimeFunction<MonoMethodGetClassFn>(MonoMethodGetClassLinked);
    const auto methodGetName = RuntimeFunction<MonoMethodGetNameFn>(MonoMethodGetNameLinked);
    const auto classGetName = RuntimeFunction<MonoClassGetNameFn>(MonoClassGetNameLinked);
    const auto classGetNamespace = RuntimeFunction<MonoClassGetNamespaceFn>(MonoClassGetNamespaceLinked);

    if (methodGetClass == nullptr || methodGetName == nullptr ||
        classGetName == nullptr || classGetNamespace == nullptr) {
        return;
    }

    void *klass = methodGetClass(method);
    if (klass == nullptr) {
        return;
    }

    const char *className = classGetName(klass);
    const char *namespaceName = classGetNamespace(klass);
    const char *methodName = methodGetName(method);

    if (IsTargetClass(className)) {
        uint32_t flags = 0;
        const std::string signature = BuildSignature(method, flags);
        Logger::Info("P3 Live: %s%s%s::%s %s code=%p",
                     namespaceName != nullptr && namespaceName[0] != '\0' ? namespaceName : "",
                     namespaceName != nullptr && namespaceName[0] != '\0' ? "." : "",
                     className != nullptr ? className : "?",
                     methodName != nullptr ? methodName : "?",
                     signature.c_str(),
                     compiledCode);

        DumpPlayer3Surface(klass, namespaceName);
    }
}

void *my_mono_compile_method_p3(void *method) {
    if (real_mono_compile_method_p3 == nullptr) {
        Logger::Error("P3 TEST2: mono compile hook invoked without trampoline");
        return nullptr;
    }

    if (!gHookActiveLogged) {
        gHookActiveLogged = true;
        Logger::Info("P3 TEST2: Mono hook ACTIVE; Player 3 ID=%d", CupheadAdapter::PlayerThreeId);
    }

    void *compiledCode = real_mono_compile_method_p3(method);
    if (gRuntimeReady && method != nullptr && compiledCode != nullptr) {
        ObserveMethod(method, compiledCode);
    }
    return compiledCode;
}

bool ResolveRuntime() {
    const int moduleCount = OSDynLoad_GetNumberOfRPLs();
    if (moduleCount <= 0) {
        Logger::Warn("P3 TEST2: no loaded modules");
        return false;
    }

    constexpr int MaxModules = 32;
    OSDynLoad_NotifyData modules[MaxModules] {};
    const int count = moduleCount < MaxModules ? moduleCount : MaxModules;
    if (!OSDynLoad_GetRPLInfo(0, count, modules)) {
        Logger::Warn("P3 TEST2: OSDynLoad_GetRPLInfo failed");
        return false;
    }

    for (int index = 0; index < count; ++index) {
        if (!EndsWith(modules[index].name, CupheadAdapter::ExecutableName)) {
            continue;
        }

        const uint32_t runtimeCompile = modules[index].textAddr + MonoCompileTextOffset;
        if (!BytesMatch(runtimeCompile, MonoCompileStableBytes, sizeof(MonoCompileStableBytes))) {
            Logger::Warn("P3 TEST2: mono_compile_method signature mismatch at %08X",
                         static_cast<unsigned int>(runtimeCompile));
            return false;
        }

        const int64_t delta = static_cast<int64_t>(runtimeCompile) - MonoCompileMethodLinked;
        if (delta < INT32_MIN || delta > INT32_MAX) {
            return false;
        }

        gRuntimeDelta = static_cast<int32_t>(delta);
        gRuntimeReady = true;
        Logger::Info("P3 TEST2: runtime VERIFIED text=%08X mono=%08X delta=%d",
                     static_cast<unsigned int>(modules[index].textAddr),
                     static_cast<unsigned int>(runtimeCompile),
                     static_cast<int>(gRuntimeDelta));
        return true;
    }

    Logger::Warn("P3 TEST2: Unity-master.rpx not found");
    return false;
}

} // namespace

bool RegisterHook(const std::string &hookId) {
    Reset();
    if (hookId.empty() || !ResolveRuntime()) {
        return false;
    }

    const uint32_t runtimeCompile = RuntimeAddress(MonoCompileMethodLinked);
    const uint32_t physicalCompile = static_cast<uint32_t>(OSEffectiveToPhysical(runtimeCompile));

    function_replacement_data_t data {};
    data.version = FUNCTION_REPLACEMENT_DATA_STRUCT_VERSION;
    data.type = FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS;
    data.physicalAddr = physicalCompile;
    data.virtualAddr = runtimeCompile;
    data.replaceAddr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&my_mono_compile_method_p3));
    data.replaceCall = reinterpret_cast<uint32_t *>(&real_mono_compile_method_p3);
    data.targetProcess = FP_TARGET_PROCESS_ALL;
    static constexpr uint64_t TargetIds[] = {CupheadAdapter::TitleId};
    data.ReplaceInRPX.targetTitleIds = TargetIds;
    data.ReplaceInRPX.targetTitleIdsCount = 1;
    data.ReplaceInRPX.versionMin = CupheadAdapter::SupportedVersion;
    data.ReplaceInRPX.versionMax = CupheadAdapter::SupportedVersion;
    data.ReplaceInRPX.executableName = CupheadAdapter::ExecutableName;
    data.ReplaceInRPX.textOffset = MonoCompileTextOffset;
    data.ReplaceInRPX.functionName = nullptr;

    Logger::Info("P3 TEST2: registering managed-surface hook id=%s offset=%08X",
                 hookId.c_str(), static_cast<unsigned int>(MonoCompileTextOffset));
    return NativeHookRegistry::Register(hookId, data);
}

void Reset() {
    real_mono_compile_method_p3 = nullptr;
    gRuntimeDelta = 0;
    gRuntimeReady = false;
    gSurfaceDumped = false;
    gHookActiveLogged = false;
}

} // namespace Solar::CupheadPlayer3
