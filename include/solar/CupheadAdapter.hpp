#pragma once

#include <cstdint>

namespace Solar::CupheadAdapter {

constexpr uint64_t TitleId = 0x0005000021000000ULL;
constexpr uint16_t SupportedVersion = 0;
constexpr int PlayerThreeId = 2;
// Address-based FunctionPatcher matching uses an ends_with comparison, so the
// stable RPX basename is safer than the build-machine path stored in the module.
constexpr const char *ExecutableName = "Unity-master.rpx";
constexpr const char *MonoTraceHookId = "cuphead.mono.compileTrace";

bool Supports(uint64_t titleId);
bool RegisterHooks();

} // namespace Solar::CupheadAdapter
