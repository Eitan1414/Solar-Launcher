#include "solar/CupheadAdapter.hpp"

#include "solar/Logger.hpp"
#include "solar/MonoBridge.hpp"

namespace Solar::CupheadAdapter {
namespace {

constexpr uint64_t SupportedTitleIds[] = {TitleId};

// Verified directly from the supplied Cuphead Unity-master.rpx symbol table and
// decompressed .text section (Title ID 0005000021000000, title version 0).
constexpr uint32_t LinkedTextBase = 0x02000000;
constexpr uint32_t MonoCompileMethodAddress = 0x02067430;

// Only the first 12 bytes are used for runtime verification. The following
// instruction contains a relocation in the RPX (.rela.text at +0x0E), so its
// immediate changes when the Wii U loader maps the executable in memory.
constexpr uint8_t MonoCompileMethodBytes[] = {
    0x7C, 0x08, 0x02, 0xA6,
    0x90, 0x01, 0x00, 0x04,
    0x94, 0x21, 0xFF, 0xF8,
};

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
    target.linkedTextBase = LinkedTextBase;
    target.compileMethodAddress = MonoCompileMethodAddress;
    target.compileMethodBytes = MonoCompileMethodBytes;
    target.compileMethodBytesSize = sizeof(MonoCompileMethodBytes);
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
