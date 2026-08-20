#include "solar/CupheadAdapter.hpp"

#include "solar/Logger.hpp"
#include "solar/MonoBridge.hpp"

namespace Solar::CupheadAdapter {
namespace {

constexpr uint64_t SupportedTitleIds[] = {TitleId};

// Verified from the symbol table and .text bytes of the supplied Cuphead
// Unity-master.rpx (Title ID 0005000021000000, title version 0).
constexpr uint8_t MonoMethodGetNameBytes[] = {0x80, 0x63, 0x00, 0x10, 0x4E, 0x80, 0x00, 0x20};
constexpr uint8_t MonoMethodGetClassBytes[] = {0x80, 0x63, 0x00, 0x08, 0x4E, 0x80, 0x00, 0x20};
constexpr uint8_t MonoClassGetNameBytes[] = {0x80, 0x63, 0x00, 0x30, 0x4E, 0x80, 0x00, 0x20};
constexpr uint8_t MonoClassGetNamespaceBytes[] = {0x80, 0x63, 0x00, 0x34, 0x4E, 0x80, 0x00, 0x20};

constexpr MonoBridge::RuntimeMetadataProfile RuntimeProfile = {
    0x0202713C, MonoMethodGetNameBytes, sizeof(MonoMethodGetNameBytes),
    0x02027144, MonoMethodGetClassBytes, sizeof(MonoMethodGetClassBytes),
    0x02C50CB8, MonoClassGetNameBytes, sizeof(MonoClassGetNameBytes),
    0x02C50CC0, MonoClassGetNamespaceBytes, sizeof(MonoClassGetNamespaceBytes),
};

constexpr const char *InterestingClasses[] = {
    "PlayerManager",
    "PlayerInput",
    "AbstractPlayerController",
    "PlayerCameraController",
    "Level",
    "LevelHUD",
    "LevelHUDPlayer",
};

} // namespace

bool Supports(uint64_t titleId) {
    return titleId == TitleId;
}

bool RegisterHooks() {
    MonoBridge::CompileHookTarget target;
    target.titleIds = SupportedTitleIds;
    target.titleIdCount = 1;
    target.versionMin = SupportedVersion;
    target.versionMax = SupportedVersion;
    target.executableName = ExecutableName;
    target.runtimeProfile = &RuntimeProfile;
    target.interestingClasses = InterestingClasses;
    target.interestingClassCount = sizeof(InterestingClasses) / sizeof(InterestingClasses[0]);

    const bool registered = MonoBridge::RegisterCompileTraceHook(MonoTraceHookId, target);
    if (registered) {
        Logger::Info("Cuphead Adapter v0.1: Mono trace hook ready; reserved Player 3 id=%d",
                     PlayerThreeId);
    }
    return registered;
}

} // namespace Solar::CupheadAdapter
